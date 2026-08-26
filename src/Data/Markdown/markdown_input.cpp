/******************************************************************************
 * MODULE     : markdown_input.cpp
 * DESCRIPTION: Markdown transparent input integration for Mogan editor
 * COPYRIGHT  : (C) 2026  Mogan contributors
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#include "markdown_inline_patterns.hpp"
#include "path.hpp"
#include "tree.hpp"
#include "tree_cursor.hpp"
#include "tree_helper.hpp"
#include "tree_observer.hpp"  // assign(): protocol-correct tree modification

#include <cstdio>

using namespace moebius;

/* TEMPORARY DEBUG (B.4 diagnosis) — prints every step of the markdown
   transparent-input pipeline to stderr.  Remove once the "typed markdown
   does not convert" bug is fixed.  Enable with -DMD_DEBUG=1 or by flipping
   the constant below to 1. */
#ifndef MD_DEBUG
#define MD_DEBUG 1
#endif
#define MD_LOG(...) do { if (MD_DEBUG) { fprintf (stderr, "[MD] " __VA_ARGS__); fflush (stderr); } } while (0)

/* Moebius string has no c_str(): use as_charp (from lolly string.hpp). */
#define MD_S(s) as_charp (s)

/******************************************************************************
 * Check if a tree has any structural formatting
 *
 * Returns true only if the tree (or any of its non-atomic descendants)
 * has at least one compound node (e.g., strong, em, code, hlink).
 ******************************************************************************/
static bool
has_formatting (tree t) {
    if (is_atomic (t)) return false;
    if (N (t) == 0) return false;
    /* Compound-label test must use is_func (t, CONCAT), NOT `t == "CONCAT"`:
       the lolly operator== (tree, const char*) only matches ATOMIC nodes
       (t->op == STRING), so it is always false for a compound CONCAT, and
       the DRD label string is lowercase "concat" anyway. */
    if (!is_func (t, CONCAT)) return true;
    for (int i = 0; i < N (t); i++) {
        if (has_formatting (t[i])) return true;
    }
    return false;
}

/******************************************************************************
 * Recursively collect all text from a tree
 ******************************************************************************/
static void
collect_text (tree t, string& out) {
    if (is_atomic (t)) {
        out << t->label;
    } else {
        for (int i = 0; i < N (t); i++) {
            collect_text (t[i], out);
        }
    }
}

/******************************************************************************
 * Locate the paragraph container of the cursor path.
 *
 * Mogan stores paragraphs in TWO shapes (confirmed by [MD] debug logs):
 *   1. DOCUMENT -> CONCAT -> atomic(...)     multi-atom paragraph (TeXmacs
 *      style), or
 *   2. DOCUMENT -> atomic(...)               SINGLE-ATOM paragraph WITHOUT a
 *      CONCAT wrapper!  This is the common case while typing plain text:
 *      the debug log shows "walk: p=1.0 label=<atomic>" followed directly
 *      by "walk: p=1 label=document" — there is NO concat level at all.
 *
 * WARNING — parent_subtree (et, tp) is NOT usable here:
 *   tp is a FULL cursor path (e.g. (0).(0).(k): DOCUMENT[0] = CONCAT (or
 *   atomic), CONCAT[0] = text atom, k = char).  parent_subtree() strips only
 *   the LAST item (k), therefore it returns the text ATOM (CONCAT[0]), which
 *   is atomic -> apply_markdown_inline_conversion() silently bailed out on
 *   "is_atomic (parent)" and never converted anything.
 *
 *   We walk upwards from tp, one level at a time, until we hit a compound
 *   CONCAT container (the paragraph).  If we hit DOCUMENT before any CONCAT,
 *   the paragraph is DOCUMENT's direct ATOMIC child (shape 2) — we return
 *   that atomic node as the container.
 ******************************************************************************/
static tree*
search_concat_parent (tree& et, path tp, path& out_p) {
    MD_LOG ("  walk: tp=%s\n", MD_S (as_string (tp)));
    path p = tp;
    path last_atom;   /* last atomic path seen while climbing (for shape 2) */
    while (!is_nil (p)) {
        p = path_up (p);
        if (is_nil (p) || !has_subtree (et, p)) {
            MD_LOG ("  walk: stop at p=%s (nil or no subtree)\n",
                    MD_S (as_string (p)));
            break;
        }
        tree* node = &subtree (et, p);
        if (is_atomic (*node)) {
            MD_LOG ("  walk: p=%s label=<atomic>\n", MD_S (as_string (p)));
            last_atom = p;
            continue;
        }
        MD_LOG ("  walk: p=%s label=%s\n", MD_S (as_string (p)),
                MD_S (as_string (L (*node))));
        if (is_func (*node, CONCAT)) {
            out_p = p;
            return node;
        }
        if (is_func (*node, DOCUMENT)) {
            /* Shape 2: single-atom paragraph, no CONCAT wrapper.
               The paragraph is the atomic child we passed through whose
               parent is this DOCUMENT. */
            if (!is_nil (last_atom) && path_up (last_atom) == p) {
                MD_LOG ("  walk: single-atom paragraph at p=%s\n",
                        MD_S (as_string (last_atom)));
                out_p = last_atom;
                return &subtree (et, last_atom);
            }
            break;
        }
        /* Other compound (with/strong/em/...): keep climbing */
    }
    out_p = path ();
    return NULL;
}

/******************************************************************************
 * Core entry point: apply the Markdown inline patterns at the cursor.
 *
 * Called from edit_interface_rep::apply_changes() on pure tree changes
 * (typing).  We examine the paragraph container of the cursor position
 * (a CONCAT for multi-atom paragraphs, or a bare ATOMIC for single-atom
 * paragraphs — both shapes occur in Mogan); if it is plain text forming a
 * complete inline Markdown pattern, we replace the container's content
 * with the parsed TeXmacs formatting tree.
 *
 * Returns true if a conversion was performed (caller re-typesets).
 ******************************************************************************/
bool
apply_markdown_inline_conversion (tree& et, path tp, path& out_p,
                                  path& out_tp) {
    MD_LOG ("inline: enter tp=%s\n", MD_S (as_string (tp)));
    if (is_nil (tp)) return false;

    path parent_p;
    tree* pp = search_concat_parent (et, tp, parent_p);
    MD_LOG ("inline: search_concat_parent -> %s (pp=%p)\n",
            parent_p == path () ? "NOT FOUND" : MD_S (as_string (parent_p)),
            (void*) pp);
    if (pp == NULL) return false;
    tree& parent = *pp;

    /* Only operate on a plain-text paragraph container (CONCAT or atomic).
       Avoid flattening nodes that already carry structure. */
    if (has_formatting (parent)) return false;

    /* Collect all text from the container. */
    string text;
    collect_text (parent, text);
    MD_LOG ("inline: text=\"%s\" len=%d\n", MD_S (text), N (text));
    if (is_empty (text)) return false;

    /* Incomplete input (e.g. "**bold" without closing) stays plain text. */
    bool complete = is_complete_markdown_input (text);
    MD_LOG ("inline: is_complete_markdown_input=%d\n", (int) complete);
    if (!complete) return false;

    md_parse_result result = try_parse_inline_markdown (text);
    bool fmt = has_formatting (result.result);
    MD_LOG ("inline: parse produced formatting=%d\n", (int) fmt);
    if (!fmt) return false;

    /* Replace the container content with the parsed structure (idempotent).
       ROOT-CAUSE FIX (2026-08-26): NEVER assign through a raw C++ reference.
       lolly's operator= only swaps the rep pointer — no observer announce/
       notify/detach — so the typesetter bridge, undo stack and ip_observers
       stayed out of sync and the NEXT keystroke crashed (its regular insert
       notification was mapped onto an externally mutated bridge tree ->
       SIGSEGV).  assign() walks apply() -> raw_assign(), which announces to
       every observer (the bridge auto-syncs and marks itself CORRUPTED, so
       the caller must NOT call typeset_invalidate anymore), detaches old
       subtree observers and records undo. */
    tree fresh = result.result;
    assign (subtree (et, parent_p), fresh);
    /* If apply() postponed our modification (is_busy queue), the document
       did not change yet — bail out cleanly; the next apply_changes round
       will retry on the same plain-text paragraph. */
    if (!strong_equal (subtree (et, parent_p), fresh)) {
      MD_LOG ("inline: assign postponed by busy queue, skipping round\n");
      return false;
    }
    out_p = parent_p;
    /* Move the cursor to the END of the converted subtree's LAST LEAF.
       CRASH FIX (2026-08-17): end(et, parent_p) on CONCAT(strong("bold"))
       returns the STRONG NODE ITSELF (parent_p.0), not the end of the text
       inside it.  strong is an enforcing node: the cursor cannot rest on a
       node, so the next keystroke used an out-of-bounds path and SIGSEGV'd
       (same family as the empty nullary section() crash in the heading
       pass).  Instead we descend to the last atomic leaf and position past
       its text — the same convention tree_traverse.cpp uses for text atoms
       (path_up(p) * N(label)).  The old tp pointed into the plain-text atom
       (e.g. (0).(0).(8) for "**bold**"); after the morph the paragraph is
       CONCAT(strong("bold")) and the old path is out of bounds.
       NOTE: descend via N(fresh) (the tree we just assigned), not through
       the stale `parent` reference, which raw_assign() may have refreshed. */
    path leaf = parent_p * (N (fresh) - 1);
    while (!is_atomic (subtree (et, leaf)))
        leaf = leaf * (N (subtree (et, leaf)) - 1);
    /* N(tree) is only valid for COMPOUND nodes (CHECK_COMPOUND asserts on
       atoms) — use N(as_string(...)) on the atomic label, the same way
       tree_traverse.cpp positions at the end of a text atom. */
    out_tp = leaf * N (as_string (subtree (et, leaf)));
    MD_LOG ("inline: CONVERTED -> out_p=%s out_tp=%s\n",
            MD_S (as_string (out_p)), MD_S (as_string (out_tp)));
    return true;
}

/******************************************************************************
 * B.4.1  Block-level heading: `# ` / `## ` / … / `###### ` at the start of
 *        a paragraph is transformed into a TeXmacs section heading.
 *
 * DESIGN NOTE — why "in-place morph" instead of a structural replace:
 *   Mogan stores paragraphs as DOCUMENT children WITHOUT a <paragraph> wrapper:
 *       DOCUMENT -> CONCAT("…text…")   (one CONCAT per paragraph)
 *   We locate the paragraph that CONTAINS the cursor with
 *   search_concat_parent (same routine as the inline pass), then REPLACE that
 *   paragraph node in place with the heading compound.  Because the paragraph
 *   keeps its path (parent_p), the DOCUMENT child index does not shift and the
 *   outer part of tp stays valid — this is what transparent input needs.
 *   The exported .md still round-trips correctly via markdown_export (it
 *   iterates DOCUMENT children and looks at each child's tag).
 *
 * FIX (2026-08-17): the previous implementation hard-coded `doc[0]`, so a
 *   heading typed in any paragraph OTHER than the first never converted.
 *   Now we operate on the cursor's own paragraph.
 *
 * CRASH FIX (2026-08-17): an EMPTY heading ("# " with nothing after the
 *   space) is NOT converted.  An empty section() is a nullary compound node:
 *   end()/correct_cursor() cannot produce a valid cursor inside it (the
 *   "arity (parent_subtree (t, p)) == 0" shortcut returns p itself, which is
 *   not a legal cursor on a nullary node), so the next keystroke used a
 *   stale/out-of-bounds path and SIGSEGV'd.  We wait until the user typed
 *   the actual title text, then convert — the heading always has arity 1.
 *
 * TRIGGER: only when the cursor's paragraph is plain text that already starts
 *   with N hashes followed by a space AND has at least one character after
 *   the space.  We strip the leading "#… " and set the label.
 *
 * IDEMPOTENT: no-op when the paragraph is already a section/subsection/etc.
 ******************************************************************************/
bool
apply_markdown_heading_conversion (tree& et, path tp, path& out_p,
                                   path& out_tp) {
    MD_LOG ("heading: enter tp=%s\n", MD_S (as_string (tp)));
    if (is_nil (tp)) return false;

    /* Locate the paragraph the cursor is inside (CONCAT or single atom) —
       exactly like the inline pass. */
    path parent_p;
    tree* pp = search_concat_parent (et, tp, parent_p);
    MD_LOG ("heading: search_concat_parent -> %s (pp=%p)\n",
            parent_p == path () ? "NOT FOUND" : MD_S (as_string (parent_p)),
            (void*) pp);
    if (pp == NULL) return false;
    tree& para = *pp;

    /* Only plain-text paragraphs; don't clobber existing structure. */
    if (has_formatting (para)) return false;

    /* Collect text and locate a leading "#… " marker. */
    string text;
    collect_text (para, text);
    MD_LOG ("heading: text=\"%s\"\n", MD_S (text));
    int n = N (text);
    int hashes = 0;
    while (hashes < n && text[hashes] == '#') hashes++;
    if (hashes < 1 || hashes > 6) return false;          // 1..6 levels only
    if (hashes >= n || text[hashes] != ' ') return false; // must be "# "
    int after = hashes + 1;                               // skip "# "

    /* CRASH FIX: empty heading ("# " only) stays plain text.  A nullary
       section() has no legal cursor position; converting now would crash
       on the next keystroke.  Wait for the title text. */
    string rest = text (after, n);
    if (is_empty (rest)) return false;

    /* DEFER (2026-08-26): if the text right after "# " starts with an inline
       Markdown marker, the user is typing something like "# **bold**", not a
       plain title.  Converting now would eat their first '*' into a plain
       heading and create confusing intermediate states.  Let the inline pass
       handle it first — once formatting appears, has_formatting() keeps this
       pass away from the paragraph for good. */
    if (rest[0] == '*' || rest[0] == '_' || rest[0] == '~' || rest[0] == '`')
      return false;

    /* Map level -> TeXmacs tag (keep in sync with markdown_import.cpp). */
    string tag;
    switch (hashes) {
    case 1: tag = "section";        break;
    case 2: tag = "subsection";     break;
    case 3: tag = "subsubsection";  break;
    case 4: tag = "paragraph";      break;
    case 5: tag = "subparagraph";   break;
    default: tag = "subsubparagraph"; break;
    }

    /* Replace the paragraph in place with the heading (arity 1: text atom).
       parent_p is preserved, so the outer path indices stay valid.
       ROOT-CAUSE FIX (2026-08-26): use the official assign() primitive —
       raw C++ reference assignment left the observer network (bridge, undo,
       ip_observers) out of sync and the NEXT keystroke SIGSEGV'd.  The
       bridge auto-syncs through the observer chain; do NOT typeset_invalidate
       manually anymore. */
    tree fresh = compound (tag, tree (rest));
    assign (subtree (et, parent_p), fresh);
    /* Bail out if apply() postponed us (is_busy queue) — retry next round. */
    if (!strong_equal (subtree (et, parent_p), fresh)) {
      MD_LOG ("heading: assign postponed by busy queue, skipping round\n");
      return false;
    }
    out_p = parent_p;
    /* Move the cursor to the END of the heading text atom.
       CRASH FIX (2026-08-18): end(et, parent_p) on a section/subsection
       (an ENFORCING node) returns the enforcing node ITSELF (parent_p.0),
       not the end of the text inside it — the same defect as the inline
       pass.  The cursor cannot rest on an enforcing node, so the next
       keystroke used an out-of-bounds path and SIGSEGV'd (confirmed by the
       [MD] log: tp jumped from 1.0.1 back to 1.0.0.14 on a section node).
       Instead descend to the last atomic leaf and position past its text
       — the same convention tree_traverse.cpp uses for text atoms.

       NOTE (2026-08-21): must use the HEADING ARITY, NOT N(parent_p) —
       parent_p is a PATH (array<int>): N() on a path returns the PATH LENGTH
       (e.g. 2 for "1.0"), not the arity of the heading node.
       parent_p * (2-1) = 1.0.1 is OUT OF BOUNDS for section(rest) (arity 1,
       only child index 0), and subtree(et, 1.0.1) crashed with SIGSEGV
       (confirmed in the 8/21 md_debug.log).  Arity is 1, so leaf = parent_p * 0
       = the text atom.  (2026-08-26: reads N(fresh) — the tree we assigned.) */
    path leaf = parent_p * (N (fresh) - 1);
    while (!is_atomic (subtree (et, leaf)))
        leaf = leaf * (N (subtree (et, leaf)) - 1);
    out_tp = leaf * N (as_string (subtree (et, leaf)));
    MD_LOG ("heading: CONVERTED -> out_p=%s out_tp=%s tag=%s rest=\"%s\"\n",
            MD_S (as_string (out_p)), MD_S (as_string (out_tp)), MD_S (tag),
            MD_S (rest));
    return true;
}
