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
#include "tree_helper.hpp"

using namespace moebius;

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
    string label = as_string (L (t));
    if (label != "CONCAT") return true;
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
 * Locate the nearest CONCAT ancestor of the cursor path.
 *
 * WARNING — parent_subtree (et, tp) is NOT usable here:
 *   tp is a FULL cursor path (e.g. (0).(0).(k): DOCUMENT[0] = CONCAT,
 *   CONCAT[0] = text atom, k = char).  parent_subtree() strips only the LAST
 *   item (k), therefore it returns the text ATOM (CONCAT[0]), which is atomic
 *   -> apply_markdown_inline_conversion() silently bailed out on
 *   "is_atomic (parent)" and never converted anything.
 *
 *   We walk upwards from tp, one level at a time, until we hit a compound
 *   CONCAT container (the paragraph).  Single-atom paragraphs are their own
 *   CONCAT's first child; multi-atom paragraphs may wrap the cursor deeper.
 ******************************************************************************/
static tree*
search_concat_parent (tree& et, path tp, path& out_p) {
    path p = tp;
    while (!is_nil (p)) {
        p = path_up (p);
        if (is_nil (p) || !has_subtree (et, p)) break;
        tree* node = &subtree (et, p);
        if (is_atomic (*node)) continue;
        if (*node == "CONCAT") {
            out_p = p;
            return node;
        }
    }
    out_p = path ();
    return NULL;
}

/******************************************************************************
 * Core entry point: apply the Markdown inline patterns at the cursor.
 *
 * Called from edit_interface_rep::apply_changes() on pure tree changes
 * (typing).  We examine the CONCAT ancestor of the cursor position; if it is
 * plain text forming a complete inline Markdown pattern, we replace the
 * CONCAT's content with the parsed TeXmacs formatting tree.
 *
 * Returns true if a conversion was performed (caller re-typesets).
 ******************************************************************************/
bool
apply_markdown_inline_conversion (tree& et, path tp, path& out_p) {
    if (is_nil (tp)) return false;

    path parent_p;
    tree* pp = search_concat_parent (et, tp, parent_p);
    if (pp == NULL) return false;
    tree& parent = *pp;

    /* Only operate on a plain-text CONCAT container.
       Avoid flattening nodes that already carry structure. */
    if (has_formatting (parent)) return false;

    /* Collect all text from the CONCAT. */
    string text;
    collect_text (parent, text);
    if (is_empty (text)) return false;

    /* Incomplete input (e.g. "**bold" without closing) stays plain text. */
    if (!is_complete_markdown_input (text)) return false;

    md_parse_result result = try_parse_inline_markdown (text);
    if (!has_formatting (result.result)) return false;

    /* Replace the CONCAT content with the parsed structure (idempotent). */
    parent = result.result;
    out_p = parent_p;
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
apply_markdown_heading_conversion (tree& et, path rp, path& out_p) {
    if (is_nil (rp) || !has_subtree (et, rp)) return false;
    tree& doc = subtree (et, rp);
    if (is_atomic (doc)) return false;
    if (!(doc == "DOCUMENT")) return false;
    if (N (doc) == 0) return false;

    tree& para = doc[0];         // the current paragraph (a CONCAT in a new doc)
    /* Already a heading? nothing to do (idempotent).  Use the tree==const char*
       operator (auto is_atomic check, no inactive-union access) rather than
       as_string(L(para)) / is_func(para,"CONCAT"), which would touch the
       inactive union member or pass a string where an int arity is expected. */
    if (!(para == "CONCAT")) return false;
    if (has_formatting (para)) return false;   // don't clobber existing structure

    /* Collect text and locate a leading "#… " marker. */
    string text;
    collect_text (para, text);
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

    /* Drop the leading "# " from the raw text atoms.  We walk the CONCAT's
       text atoms; each atom is a string label.  Skip 'after' chars total,
       trimming the atom where the marker ends. */
    int k = 0;
    for (; k < N (para); k++) {
        if (!is_atomic (para[k])) { after = 0; break; }   // safety: non-text before marker
        string s = as_string (para[k]);
        int sl = N (s);
        if (sl < after) {
            after -= sl;
            para[k] = tree ("");
            if (after <= 0) break;       // marker fully consumed across atoms
        }
        else { para[k] = tree (s (after, sl)); after = 0; break; }
    }
    if (after != 0) return false;     // marker split across atoms in an unexpected way

    /* Build the heading node with compound(tag, …) — same shape as the B.1
       import converter.  Reconstruct into a temporary, then assign into the
       SAME DOCUMENT slot (et[0]) so the cursor path tp=(0).(0).k keeps its
       outer index 0 valid.  We never reassign para (an lvalue ref) to a new
       tree, and never write L(para)=… (union-label assignment is not portable
       on MSVC), hence the explicit rebuild. */
    tree heading = compound (tag);
    for (int j = 0; j < N (para); j++)
        if (!(is_atomic (para[j]) && is_empty (as_string (para[j]))))
            heading << para[j];

    doc[0] = heading;    // slot 0 preserved; inner CONCAT layer removed
    out_p = rp * 0;
    return true;
}
