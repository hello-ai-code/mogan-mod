/******************************************************************************
 * MODULE     : render_visitor_extra.cpp
 * DESCRIPTION: Default implementations for extra render visitors.
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#include "Boxes/render_visitor_extra.hpp"

void
PreRenderVisitor::visit_default (box_rep& b) {
  (void) b;
}

void
PostRenderVisitor::visit_default (box_rep& b) {
  (void) b;
}

void
BackgroundRenderVisitor::visit_default (box_rep& b) {
  (void) b;
}
