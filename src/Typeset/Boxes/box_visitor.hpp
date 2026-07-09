/******************************************************************************
 * MODULE     : box_visitor.hpp
 * DESCRIPTION: Visitor interface for box_rep hierarchy.
 *              Enables separation of rendering logic from layout data.
 * COPYRIGHT  : (C) 2025  Mogan STEM authors
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#ifndef BOX_VISITOR_H
#define BOX_VISITOR_H

/* Note: This header is intentionally lightweight.
 * Full box class declarations are not needed here — only forward references.
 * Each box subclass defines its own accept() to call the right visit() overload.
 */

class box_rep;
class anim_box_rep;
class anim_compose_box_rep;
class anim_constant_box_rep;
class anim_effect_box_rep;
class anim_progressive_box_rep;
class anim_repeat_box_rep;
class anim_translate_box_rep;
class arc_box_rep;
class art_box_rep;
class bracket_box_rep;
class case_box_rep;
class cell_box_rep;
class clip_box_rep;
class composite_anim_box_rep;
class composite_box_rep;
class concat_box_rep;
class control_box_box_rep;
class control_lazy_box_rep;
class control_tree_box_rep;
class crop_marks_box_rep;
class curve_box_rep;
class dummy_box_rep;
class dummy_script_box_rep;
class effect_box_rep;
class empty_box_rep;
class flag_box_rep;
class frac_box_rep;
class frozen_box_rep;
class graphics_box_rep;
class graphics_group_box_rep;
class grid_box_rep;
class highlight_box_rep;
class image_box_rep;
class info_box_rep;
class lim_box_rep;
class line_box_rep;
class locus_box_rep;
class macro_box_rep;
class macro_delimiter_box_rep;
class marker_box_rep;
class move_box_rep;
class move_delimiter_box_rep;
class neg_box_rep;
class note_box_rep;
class page_border_box_rep;
class page_box_rep;
class phrase_box_rep;
class point_box_rep;
class polygon_box_rep;
class relay_box_rep;
class remember_box_rep;
class repeat_box_rep;
class resize_box_rep;
class scatter_box_rep;
class scrollbar_box_rep;
class shift_box_rep;
class shorter_box_rep;
class side_box_rep;
class sound_box_rep;
class spacial_box_rep;
class specific_box_rep;
class sqrt_box_rep;
class stack_box_rep;
class superpose_box_rep;
class symbol_box_rep;
class table_box_rep;
class tag_box_rep;
class test_box_rep;
class text_at_box_rep;
class text_box_rep;
class toc_box_rep;
class transformed_box_rep;
class tree_box_rep;
class vcorrect_box_rep;
class vresize_box_rep;
class wide_box_rep;

class BoxVisitor {
public:
  virtual ~BoxVisitor () = default;

  // Every box type gets a visit() overload.
  // Default base implementations are empty (no-op for unhandled types).
  // Concrete visitors override only the types they care about.

  virtual void visit (box_rep& b)                    {}
  virtual void visit (anim_box_rep& b)               {}
  virtual void visit (anim_compose_box_rep& b)       {}
  virtual void visit (anim_constant_box_rep& b)      {}
  virtual void visit (anim_effect_box_rep& b)        {}
  virtual void visit (anim_progressive_box_rep& b)   {}
  virtual void visit (anim_repeat_box_rep& b)        {}
  virtual void visit (anim_translate_box_rep& b)     {}
  virtual void visit (arc_box_rep& b)                {}
  virtual void visit (art_box_rep& b)                {}
  virtual void visit (bracket_box_rep& b)            {}
  virtual void visit (case_box_rep& b)               {}
  virtual void visit (cell_box_rep& b)               {}
  virtual void visit (clip_box_rep& b)               {}
  virtual void visit (composite_anim_box_rep& b)     {}
  virtual void visit (composite_box_rep& b)          {}
  virtual void visit (concat_box_rep& b)             {}
  virtual void visit (control_box_box_rep& b)        {}
  virtual void visit (control_lazy_box_rep& b)       {}
  virtual void visit (control_tree_box_rep& b)       {}
  virtual void visit (crop_marks_box_rep& b)         {}
  virtual void visit (curve_box_rep& b)              {}
  virtual void visit (dummy_box_rep& b)              {}
  virtual void visit (dummy_script_box_rep& b)       {}
  virtual void visit (effect_box_rep& b)             {}
  virtual void visit (empty_box_rep& b)              {}
  virtual void visit (flag_box_rep& b)               {}
  virtual void visit (frac_box_rep& b)               {}
  virtual void visit (frozen_box_rep& b)             {}
  virtual void visit (graphics_box_rep& b)           {}
  virtual void visit (graphics_group_box_rep& b)     {}
  virtual void visit (grid_box_rep& b)               {}
  virtual void visit (highlight_box_rep& b)          {}
  virtual void visit (image_box_rep& b)              {}
  virtual void visit (info_box_rep& b)               {}
  virtual void visit (lim_box_rep& b)                {}
  virtual void visit (line_box_rep& b)               {}
  virtual void visit (locus_box_rep& b)              {}
  virtual void visit (macro_box_rep& b)              {}
  virtual void visit (macro_delimiter_box_rep& b)    {}
  virtual void visit (marker_box_rep& b)             {}
  virtual void visit (move_box_rep& b)               {}
  virtual void visit (move_delimiter_box_rep& b)     {}
  virtual void visit (neg_box_rep& b)                {}
  virtual void visit (note_box_rep& b)               {}
  virtual void visit (page_border_box_rep& b)        {}
  virtual void visit (page_box_rep& b)               {}
  virtual void visit (phrase_box_rep& b)             {}
  virtual void visit (point_box_rep& b)              {}
  virtual void visit (polygon_box_rep& b)            {}
  virtual void visit (relay_box_rep& b)              {}
  virtual void visit (remember_box_rep& b)           {}
  virtual void visit (repeat_box_rep& b)             {}
  virtual void visit (resize_box_rep& b)             {}
  virtual void visit (scatter_box_rep& b)            {}
  virtual void visit (scrollbar_box_rep& b)          {}
  virtual void visit (shift_box_rep& b)              {}
  virtual void visit (shorter_box_rep& b)            {}
  virtual void visit (side_box_rep& b)               {}
  virtual void visit (sound_box_rep& b)              {}
  virtual void visit (spacial_box_rep& b)            {}
  virtual void visit (specific_box_rep& b)           {}
  virtual void visit (sqrt_box_rep& b)               {}
  virtual void visit (stack_box_rep& b)              {}
  virtual void visit (superpose_box_rep& b)          {}
  virtual void visit (symbol_box_rep& b)             {}
  virtual void visit (table_box_rep& b)              {}
  virtual void visit (tag_box_rep& b)                {}
  virtual void visit (test_box_rep& b)               {}
  virtual void visit (text_at_box_rep& b)            {}
  virtual void visit (text_box_rep& b)               {}
  virtual void visit (toc_box_rep& b)                {}
  virtual void visit (transformed_box_rep& b)        {}
  virtual void visit (tree_box_rep& b)               {}
  virtual void visit (vcorrect_box_rep& b)           {}
  virtual void visit (vresize_box_rep& b)            {}
  virtual void visit (wide_box_rep& b)               {}
protected:
  // Default fallback for unhandled types.
  // Concrete visitors override this to define base behavior.
  virtual void visit_default (box_rep& b) = 0;
};

#endif // BOX_VISITOR_H
