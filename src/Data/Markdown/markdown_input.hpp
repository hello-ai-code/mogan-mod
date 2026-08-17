/******************************************************************************
 * MODULE     : markdown_input.hpp
 * DESCRIPTION: Markdown transparent input integration for Mogan editor
 * COPYRIGHT  : (C) 2026  Mogan contributors
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#ifndef MARKDOWN_INPUT_H
#define MARKDOWN_INPUT_H

#include "tree.hpp"
#include "path.hpp"

/*
 * Apply Markdown inline pattern conversion to the text at the cursor.
 *
 * Examines the CONCAT parent of the cursor position. If all content
 * is plain text and forms a complete Markdown inline pattern, converts
 * it to the corresponding TeXmacs formatting tree.
 *
 * @param et    The editor's main tree
 * @param tp    The cursor path (must be non-nil)
 * @param out_p On success, the path of the modified CONCAT node
 *              (relative to et, e.g. (0).(1)); unchanged otherwise.
 * @param out_tp On success, the new cursor position (valid path inside
 *              the converted subtree); unchanged otherwise.
 * @return      true if conversion was performed
 */
bool
apply_markdown_inline_conversion (tree& et, path tp, path& out_p,
                                  path& out_tp);

/*
 * B.4.1 Block-level heading conversion.
 *
 * Detects a leading "# " / "## " / … / "###### " marker at the start of the
 * paragraph containing the cursor and morphs that paragraph in place into a
 * TeXmacs section/subsection/… node (stripping the leading "# ").
 *
 * The paragraph is located with the same search_concat_parent walk as the
 * inline pass, so headings convert in ANY paragraph, not just the first one
 * of the DOCUMENT.  The paragraph node is replaced in place (its path is
 * preserved), so the outer cursor path stays valid.
 *
 * An empty heading ("# " with nothing after the space) is deliberately NOT
 * converted: a nullary section() has no legal cursor position, and converting
 * it crashes on the next keystroke.  We wait for the actual title text.
 *
 * @param et    The editor's full buffer tree
 * @param tp    The cursor path (must be non-nil)
 * @param out_p On success, the absolute path (relative to et) of the modified
 *              node; unchanged otherwise.
 * @param out_tp On success, the new cursor position (valid path inside
 *              the converted heading); unchanged otherwise.
 * @return      true if a heading conversion was performed
 */
bool
apply_markdown_heading_conversion (tree& et, path tp, path& out_p,
                                   path& out_tp);

#endif /* defined MARKDOWN_INPUT_H */
