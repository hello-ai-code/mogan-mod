/******************************************************************************
 * MODULE     : render_visitor_unity.cpp
 * DESCRIPTION: Unity build file for all RenderVisitor-related sources.
 *              Compiling all box files + RenderVisitor in a single .obj
 *              ensures that MSVC LTCG can resolve vtable-to-function
 *              references across all visit() implementations without
 *              LNK2001 errors.
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

/* boxes.cpp — box_rep::accept() and redraw() */
#include "Basic/boxes.cpp"

/* render_visitor.cpp — RenderVisitor::visit_default() and vtable */
#include "render_visitor.cpp"

/* box .cpp files with RenderVisitor::visit() implementations: */
#include "Basic/basic_boxes.cpp"   /* test, line, polygon, arc, image */
#include "Basic/rubber_boxes.cpp"  /* bracket */
#include "Basic/text_boxes.cpp"    /* text */
#include "Composite/concat_boxes.cpp"     /* phrase */
#include "Composite/decoration_boxes.cpp" /* specific, toc */
#include "Composite/misc_boxes.cpp"       /* page */
#include "Composite/stack_boxes.cpp"      /* stack */
#include "Modifier/change_boxes.cpp"      /* remember */
#include "Graphics/graphics_boxes.cpp"    /* point, curve, spacial */
#include "Graphics/grid_boxes.cpp"        /* grid */
