/******************************************************************************
 * MODULE     : render_visitor_extra.hpp
 * DESCRIPTION: Extra render visitors for pre_display, post_display,
 *              and display_background phases.
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#ifndef RENDER_VISITOR_EXTRA_H
#define RENDER_VISITOR_EXTRA_H

#include "Boxes/box_visitor.hpp"

class renderer_rep;

/******************************************************************************
 * PreRenderVisitor — replaces pre_display() in each box class
 ******************************************************************************/

class PreRenderVisitor : public BoxVisitor {
public:
  renderer_rep* ren;
  PreRenderVisitor (renderer_rep* r): ren (r) {}
  virtual void visit_default (box_rep& b) override;

  virtual void visit (graphics_box_rep& b) override;
  virtual void visit (transformed_box_rep& b) override;
  virtual void visit (clip_box_rep& b) override;
  virtual void visit (cell_box_rep& b) override;
  virtual void visit (highlight_box_rep& b) override;
  virtual void visit (art_box_rep& b) override;
  virtual void visit (anim_box_rep& b) override;
  virtual void visit (composite_anim_box_rep& b) override;
  virtual void visit (anim_compose_box_rep& b) override;
  virtual void visit (anim_repeat_box_rep& b) override;
  virtual void visit (anim_effect_box_rep& b) override;
  virtual void visit (sound_box_rep& b) override;
  virtual void visit (concat_box_rep& b) override;
  virtual void visit (flag_box_rep& b) override;
  virtual void visit (scatter_box_rep& b) override;
  virtual void visit (page_box_rep& b) override;
  virtual void visit (page_border_box_rep& b) override;
  virtual void visit (crop_marks_box_rep& b) override;
};

/******************************************************************************
 * PostRenderVisitor — replaces post_display() in each box class
 ******************************************************************************/

class PostRenderVisitor : public BoxVisitor {
public:
  renderer_rep* ren;
  PostRenderVisitor (renderer_rep* r): ren (r) {}
  virtual void visit_default (box_rep& b) override;

  virtual void visit (graphics_box_rep& b) override;
  virtual void visit (transformed_box_rep& b) override;
  virtual void visit (clip_box_rep& b) override;
  virtual void visit (cell_box_rep& b) override;
  virtual void visit (locus_box_rep& b) override;
  virtual void visit (highlight_box_rep& b) override;
  virtual void visit (art_box_rep& b) override;
  virtual void visit (anim_effect_box_rep& b) override;
  virtual void visit (concat_box_rep& b) override;
  virtual void visit (flag_box_rep& b) override;
  virtual void visit (page_box_rep& b) override;
};

/******************************************************************************
 * BackgroundRenderVisitor — replaces display_background() in each box class
 ******************************************************************************/

class BackgroundRenderVisitor : public BoxVisitor {
public:
  renderer_rep* ren;
  BackgroundRenderVisitor (renderer_rep* r): ren (r) {}
  virtual void visit_default (box_rep& b) override;

  virtual void visit (scatter_box_rep& b) override;
  virtual void visit (page_box_rep& b) override;
  virtual void visit (page_border_box_rep& b) override;
  virtual void visit (crop_marks_box_rep& b) override;
};

#endif // defined RENDER_VISITOR_EXTRA_H
