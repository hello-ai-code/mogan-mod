/******************************************************************************
 * MODULE     : render_target.hpp
 * DESCRIPTION: RenderTarget enum + factory function for creating renderer backends
 *
 * This module provides a unified way to create different renderer backends
 * (screen, memory, printer, PDF) through a single factory function.
 *
 * Previously, renderers were created through ad-hoc mechanisms:
 *   - Qt:   the_qt_renderer()  (global singleton)
 *   - Memory: tm_new<memory_renderer_rep>(w, h)
 *   - Printer/PDF: direct tm_new<...>
 *
 * The factory formalizes these creation paths into a single entry point.
 * Extensions (e.g. Skia, Canvas) can be added by extending the enum and
 * registering their creation logic in the factory.
 *
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#ifndef RENDER_TARGET_H
#define RENDER_TARGET_H

#include "renderer.hpp"

/**
 * @brief The type of rendering target.
 *
 * Each target corresponds to a concrete renderer_rep subclass.
 * The factory function create_renderer() maps each target to its
 * corresponding implementation.
 */
enum class render_target {
  screen,  ///< Platform-native screen renderer (Qt on desktop, X11 on Linux)
  memory,  ///< In-memory pixel buffer renderer (MemoryRenderer, headless)
  printer, ///< Physical printer output (printer_rep)
  pdf      ///< PDF export (pdf_hummus_renderer_rep)
};

/**
 * @brief Create a renderer for the specified target.
 *
 * @param target  The type of renderer to create.
 * @param w       Width in pixels (used by memory renderer; ignored for screen).
 * @param h       Height in pixels (used by memory renderer; ignored for screen).
 *
 * @return A renderer pointer, or NULL if the target is not supported.
 *
 * @note The screen target delegates to the_qt_renderer() when compiled with
 *       QTTEXMACS.  On non-Qt builds the screen target returns NULL.
 * @note The printer and pdf targets are not yet implemented in this factory
 *       (they are created directly in their respective plugin code).
 */
renderer create_renderer (render_target target, int w = 0, int h = 0);

#endif // RENDER_TARGET_H