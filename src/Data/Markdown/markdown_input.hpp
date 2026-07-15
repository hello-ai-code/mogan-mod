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
 * @param et  The editor's main tree
 * @param tp  The cursor path (must be non-nil)
 * @return    true if conversion was performed
 */
bool
apply_markdown_inline_conversion (tree& et, path tp);

#endif /* defined MARKDOWN_INPUT_H */
