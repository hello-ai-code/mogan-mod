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
 * Core entry point: convert Markdown inline patterns at the cursor.
 *
 * Called from edit_interface_rep::apply_changes() on pure tree changes
 * (typing).  We examine the CONCAT parent of the cursor position; if it is
 * plain text forming a complete inline Markdown pattern, we replace the
 * CONCAT's content with the parsed TeXmacs formatting tree.
 *
 * Returns true if a conversion was performed (caller re-typesets).
 ******************************************************************************/
bool
apply_markdown_inline_conversion (tree& et, path tp) {
    if (is_nil (tp)) return false;

    tree& parent = parent_subtree (et, tp);
    if (is_atomic (parent)) return false;

    /* Only operate on a plain-text CONCAT container.
       Avoid flattening nodes that already carry structure. */
    if (as_string (L (parent)) != "CONCAT") return false;
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
    return true;
}
