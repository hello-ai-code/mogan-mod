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
 * first paragraph of the DOCUMENT node at et[rp] and morphs that CONCAT
 * paragraph in place into a TeXmacs section/subsection/… node (stripping the
 * leading "# ").
 *
 * NOTE — et is the FULL buffer tree (editor_rep::et, "all TeXmacs trees");
 * rp is the path of the DOCUMENT root inside et (editor_rep::rp).  The
 * DOCUMENT is NOT et itself, hence rp must be supplied by the caller.
 *
 * In-place morph (rather than a structural replace) is deliberate: the cursor
 * path tp used elsewhere in apply_changes() must stay valid, and the DOCUMENT
 * child index 0 is preserved. See markdown_input.cpp for the full rationale.
 *
 * @param et    The editor's full buffer tree
 * @param rp    Path of the DOCUMENT root inside et
 * @param out_p On success, the absolute path (relative to et) of the modified
 *              node (rp * 0); unchanged otherwise.
 * @param out_tp On success, the new cursor position (valid path inside
 *              the converted heading); unchanged otherwise.
 * @return      true if a heading conversion was performed
 */
bool
apply_markdown_heading_conversion (tree& et, path rp, path& out_p,
                                   path& out_tp);

#endif /* defined MARKDOWN_INPUT_H */
