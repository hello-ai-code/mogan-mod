/******************************************************************************
 * MODULE     : render_target.cpp
 * DESCRIPTION: RenderTarget factory implementation
 *
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#include "render_target.hpp"
#include "memory_renderer.hpp"

#ifdef QTTEXMACS
#include "qt_renderer.hpp"
#endif

renderer
create_renderer (render_target target, int w, int h) {
  switch (target) {
    case render_target::memory:
      return tm_new<memory_renderer_rep> (w, h);

    case render_target::screen:
#ifdef QTTEXMACS
      // Delegate to the Qt global singleton; w,h are ignored
      // (dimensions are determined by the widget's QPainter device).
      (void) w;
      (void) h;
      return the_qt_renderer ();
#else
      // Non-Qt builds have no screen renderer.
      (void) w;
      (void) h;
      return NULL;
#endif

    case render_target::printer:
    case render_target::pdf:
    default:
      // Not yet wired through the factory.
      // These are still created directly in their respective plugin code.
      (void) w;
      (void) h;
      return NULL;
  }
}