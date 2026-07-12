/******************************************************************************
 * MODULE     : memory_renderer.hpp
 * DESCRIPTION: A memory-buffer-based renderer (NO external GUI dependency).
 *
 * This renderer draws into a flat ARGB pixel buffer in host memory.
 * It requires zero Qt, zero display, zero system GUI — pure CPU rendering.
 *
 * Purpose:
 *   1. Validate that renderer_rep's abstract interface is complete.
 *   2. Enable unit tests for Box-tree rendering without launching a GUI.
 *   3. Serve as a reference implementation for future backends (Canvas, Skia).
 *
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#ifndef MEMORY_RENDERER_H
#define MEMORY_RENDERER_H

#include "basic_renderer.hpp"

class memory_renderer_rep : public basic_renderer_rep {
public:
  /* ── Surface ──────────────────────────────────────────────────────────── */
  array<unsigned int>     pixels;   // ARGB (host byte order), w × h
                                    // (w, h inherited from basic_renderer_rep)

public:
  memory_renderer_rep (int w2, int h2);
  ~memory_renderer_rep ();

  /* pixel accessors (useful for tests / image export) */
  inline unsigned int  get_pixel (int x, int y) const;
  inline void          set_pixel (int x, int y, unsigned int argb);
  void                 clear_all (unsigned int argb = 0xFF000000);  // opaque black

  /* ── renderer_rep interface — required primitives ───────────────────── */

  void draw (int char_code, font_glyphs fn, SI x, SI y,
             int codepoint = -1) override;
  void line  (SI x1, SI y1, SI x2, SI y2)     override;
  void lines (array<SI> xs, array<SI> ys)      override;
  void clear (SI x1, SI y1, SI x2, SI y2)     override;
  void fill  (SI x1, SI y1, SI x2, SI y2)     override;
  void arc   (SI x1, SI y1, SI x2, SI y2,
              int alpha, int delta)            override;
  void fill_arc (SI x1, SI y1, SI x2, SI y2,
                 int alpha, int delta)         override;
  void polygon (array<SI> xs, array<SI> ys,
                bool convex = true)            override;

  /* ── helpers (clipping, coordinate conversion) ──────────────────────── */

private:
  /* Convert SI -> pixel coordinate, accounting for origin (ox,oy) and
     pixel scale.  Returns false if the point is outside the surface. */
  inline bool si_to_pixel (SI sx, SI sy, int& px, int& py) const;

  /* Clip a rectangle against the surface extents + renderer clipping
     region.  Output is in pixel coordinates. */
  bool clip_rect (SI sx1, SI sy1, SI sx2, SI sy2,
                  int& px1, int& py1, int& px2, int& py2) const;

  /* Plot a single ARGB pixel after origin/shift + clipping. */
  inline void plot (SI sx, SI sy, unsigned int argb);
};

typedef memory_renderer_rep* memory_renderer;

#endif // MEMORY_RENDERER_H
