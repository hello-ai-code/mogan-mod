/******************************************************************************
 * MODULE     : render_visitor.hpp
 * DESCRIPTION: Visitor that renders boxes onto a renderer.
 *              Replaces the display() method in each box class.
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#ifndef RENDER_VISITOR_H
#define RENDER_VISITOR_H

#include "Boxes/box_visitor.hpp"

class renderer;

class RenderVisitor : public BoxVisitor {
public:
  renderer ren;

  RenderVisitor (renderer r): ren (r) {}

  virtual void visit_default (box_rep& b) override;
  // visit() overrides are defined in the individual box .cpp files
};

#endif // defined RENDER_VISITOR_H
