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

    /* Replace the container content with the parsed structure (idempotent). */
    parent = result.result;
    out_p = parent_p;
    /* Move the cursor to the END of the converted subtree.  The old tp
       pointed into the plain-text atom (e.g. (0).(0).(8) for "**bold**");
       after the morph the paragraph is CONCAT(strong("bold")) and the old
       path is out of bounds (the crash the user reported).  end(et, p)
       computes the last accessible cursor position inside the converted
       paragraph, which is where the user keeps typing. */
    out_tp = end (et, parent_p);
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
 *   et is the FULL buffer tree; rp points at the DOCUMENT root inside et.
 *   When the user types at the start of the first paragraph, the cursor path is
 *       tp = rp * 0 * 0 * k   // DOCUMENT[0] = CONCAT, CONCAT[0] = 1st text atom, k = char
 *   apply_changes() calls this with that tp, then keeps using the SAME tp later
 *   (find_check_cursor(tp), subtree(et, path_up(tp)), …).  If we replaced
 *   DOCUMENT[0] with a brand-new `section` node, the indices would shift (the
 *   CONCAT layer disappears) and tp would address an out-of-bounds position ->
 *   crash.  So we MORPH doc[0] in place: the DOCUMENT child index (0) is
 *   preserved, only the (CONCAT, content) pair is rewritten as (section, content
 *   minus the leading "# ").  The cursor tp stays valid, which is exactly what
 *   transparent input needs.  The exported .md still round-trips correctly via
 *   markdown_export (it iterates DOCUMENT children and looks at each child's tag).
 *
 * TRIGGER: only when the paragraph is a plain-text CONCAT that already starts
 *   with N hashes followed by a space (e.g. the user just typed the space, or
 *   the text begins that way).  We strip the leading "#… " and set the label.
 *
 * IDEMPOTENT: no-op when doc[0] is already a section/subsection/etc.
 ******************************************************************************/
bool
apply_markdown_heading_conversion (tree& et, path rp, path& out_p,
                                   path& out_tp) {
    MD_LOG ("heading: enter rp=%s\n", MD_S (as_string (rp)));
    if (is_nil (rp) || !has_subtree (et, rp)) return false;
    tree& doc = subtree (et, rp);
    if (is_atomic (doc)) return false;
    MD_LOG ("heading: doc=%s\n", MD_S (as_string (L (doc))));
    if (!is_func (doc, DOCUMENT)) return false;
    if (N (doc) == 0) return false;

    tree& para = doc[0];         // the current paragraph (CONCAT or single atom)
    bool para_is_atomic = is_atomic (para);
    /* Already a heading? nothing to do (idempotent).  Use is_func (para,
       CONCAT): the compound-label test must NOT use `para == "CONCAT"` (the
       lolly operator== (tree, const char*) only matches atomic nodes).
       NOTE: Mogan stores a single-atom paragraph WITHOUT a CONCAT wrapper
       (DOCUMENT -> atomic), so accept plain atomic paragraphs as well. */
    if (!para_is_atomic && !is_func (para, CONCAT)) return false;
    if (has_formatting (para)) return false;   // don't clobber existing structure

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

    /* Drop the leading "# " from the raw text. */
    tree heading = compound (tag);
    if (para_is_atomic) {
        /* Single-atom paragraph: strip the label directly. */
        string s = as_string (para);
        string rest = s (after, N (s));
        if (!is_empty (rest)) heading << tree (rest);
    }
    else {
        /* CONCAT paragraph: walk its text atoms, trimming the atom where
           the marker ends.  If an atom is shorter than the remaining
           marker, it is entirely part of the marker -> blank it out. */
        for (int k = 0; k < N (para); k++) {
            if (!is_atomic (para[k])) { after = 0; break; } // safety: non-text
            string s = as_string (para[k]);
            int sl = N (s);
            if (sl < after) {
                after -= sl;
                para[k] = tree ("");      // whole atom is marker text
            }
            else {
                para[k] = tree (s (after, sl));  // keep the tail after marker
                after = 0;
                break;
            }
        }
        if (after != 0) return false; // marker split across atoms unexpectedly
        for (int j = 0; j < N (para); j++)
            if (!(is_atomic (para[j]) && is_empty (as_string (para[j]))))
                heading << para[j];
    }

    /* Assign into the SAME DOCUMENT slot (doc[0]) so the cursor path
       tp=(rp*0).k keeps its outer index 0 valid. */
    doc[0] = heading;    // slot 0 preserved; inner CONCAT/atomic layer replaced
    out_p = rp * 0;
    /* Move the cursor to the END of the converted heading.  The old tp
       pointed into the plain-text atom (e.g. (0).(0).(2) after typing "# ");
       the morphed doc[0] is now a compound section node whose arity may be
       0 (empty heading) or 1 (text), so the old path is out of bounds.
       end(et, rp*0) gives the last accessible cursor position inside the
       heading — the user keeps typing there. */
    out_tp = end (et, rp * 0);
    MD_LOG ("heading: CONVERTED -> out_p=%s out_tp=%s tag=%s\n",
            MD_S (as_string (out_p)), MD_S (as_string (out_tp)), MD_S (tag));
    return true;
}
