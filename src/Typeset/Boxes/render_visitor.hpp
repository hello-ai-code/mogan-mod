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

class renderer_rep;

class RenderVisitor : public BoxVisitor {
public:
  renderer_rep* ren;

  RenderVisitor (renderer_rep* r): ren (r) {}

  virtual void visit_default (box_rep& b) override;

  // Explicit visit() declarations required by MSVC
  // (must be declared in the derived class before out-of-class definition)
  virtual void visit (test_box_rep& b) override;
  virtual void visit (line_box_rep& b) override;
  virtual void visit (polygon_box_rep& b) override;
  virtual void visit (arc_box_rep& b) override;
  virtual void visit (image_box_rep& b) override;
  virtual void visit (text_box_rep& b) override;
  virtual void visit (bracket_box_rep& b) override;
  virtual void visit (phrase_box_rep& b) override;
  virtual void visit (specific_box_rep& b) override;
  virtual void visit (toc_box_rep& b) override;
  virtual void visit (page_box_rep& b) override;
  virtual void visit (stack_box_rep& b) override;
  virtual void visit (remember_box_rep& b) override;
  virtual void visit (point_box_rep& b) override;
  virtual void visit (curve_box_rep& b) override;
  virtual void visit (spacial_box_rep& b) override;
  virtual void visit (grid_box_rep& b) override;
};

#endif // defined RENDER_VISITOR_H
