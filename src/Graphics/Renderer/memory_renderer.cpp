/******************************************************************************
 * MODULE     : memory_renderer.cpp
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

#include "memory_renderer.hpp"
#include "bitmap_font.hpp"
#include "colors.hpp"

/* ── Constructor / Destructor ──────────────────────────────────────────── */

memory_renderer_rep::memory_renderer_rep (int w2, int h2)
  : basic_renderer_rep (false, w2, h2)
{
  pixels = array<unsigned int> (w * h);
  // fill with white
  unsigned int* buf = A (pixels);
  for (int i = 0; i < w * h; i++)
    buf[i] = 0xFFFFFFFFu;
}

memory_renderer_rep::~memory_renderer_rep () {}

/* ── Pixel helpers ─────────────────────────────────────────────────────── */

inline unsigned int
memory_renderer_rep::get_pixel (int x, int y) const {
  if (x < 0 || x >= w || y < 0 || y >= h) return 0;
  const unsigned int* buf = A (pixels);
  return buf[y * w + x];
}

inline void
memory_renderer_rep::set_pixel (int x, int y, unsigned int argb) {
  if (x < 0 || x >= w || y < 0 || y >= h) return;
  unsigned int* buf = A (pixels);
  buf[y * w + x]= argb;
}

void
memory_renderer_rep::clear_all (unsigned int argb) {
  unsigned int* buf = A (pixels);
  for (int i = 0; i < w * h; i++)
    buf[i]= argb;
}

/* ── Coordinate conversion ───────────────────────────────────────────────
 *
 * Mogan uses a Y-DOWN coordinate system (higher Y = further down on screen)
 * for rendering commands.  The base class encode/decode methods convert
 * between this space and an internal Y-UP pixel space.
 *
 * For our pixel buffer (Y-DOWN, origin at top-left), we compute:
 *   buf_x = (x + ox) / pixel      — left-to-right
 *   buf_y = (y + oy) / pixel      — Y-down, after adding back origin
 *
 * These are derived from the encode function:
 *   encode:  SI_x = pixel_x * pixel - ox
 *   encode:  SI_y = (-pixel_y) * pixel - oy
 * Inverting gives:
 *   pixel_x = (SI_x + ox) / pixel
 *   pixel_y = -(SI_y + oy) / pixel    (Y-UP)
 * We want Y-DOWN, so:
 *   buf_y = -pixel_y = (SI_y + oy) / pixel
 * ──────────────────────────────────────────────────────────────────────── */

inline bool
memory_renderer_rep::si_to_pixel (SI sx, SI sy, int& px, int& py) const {
  px = ((int) ((sx + ox) / pixel));
  py = ((int) ((sy + oy) / pixel));
  // Y-DOWN: (sy + oy) / pixel gives 0 at origin (top), increases downward.
  // Clipping to visible area is handled by set_pixel / clip_rect.
  return (px >= 0 && px < w && py >= 0 && py < h);
}

bool
memory_renderer_rep::clip_rect (SI sx1, SI sy1, SI sx2, SI sy2,
                                int& px1, int& py1, int& px2, int& py2) const {
  // Apply renderer-level clipping against the clip region (cx1,cy1)-(cx2,cy2)
  // in origin-adjusted SI space.  Mogan convention: cy1 <= cy2 (Y-down).
  if (sx1 < cx1 - ox) sx1 = cx1 - ox;
  if (sy1 < cy1 - oy) sy1 = cy1 - oy;
  if (sx2 > cx2 - ox) sx2 = cx2 - ox;
  if (sy2 > cy2 - oy) sy2 = cy2 - oy;

  // Cull empty rectangles
  if (sx1 >= sx2 || sy1 >= sy2) return false;

  // Convert to pixel coords (Y-DOWN buffer)
  px1 = (int) ((sx1 + ox) / pixel);
  py1 = (int) ((sy1 + oy) / pixel);
  px2 = (int) ((sx2 + ox) / pixel);
  py2 = (int) ((sy2 + oy) / pixel);

  // Clamp to buffer surface
  if (px1 < 0) px1 = 0;
  if (py1 < 0) py1 = 0;
  if (px2 > w) px2 = w;
  if (py2 > h) py2 = h;

  return (px1 < px2 && py1 < py2);
}

inline void
memory_renderer_rep::plot (SI sx, SI sy, unsigned int argb) {
  int px, py;
  if (si_to_pixel (sx, sy, px, py))
    set_pixel (px, py, argb);
}

/* ── Color helpers ─────────────────────────────────────────────────────── */

static unsigned int
color_to_argb (color col) {
  int r, g, b, a;
  get_rgb_color (col, r, g, b, a);
  return ((unsigned int)(a) << 24) |
         ((unsigned int)(r) << 16) |
         ((unsigned int)(g) <<  8) |
         ((unsigned int)(b)      );
}

/* ── renderer_rep interface — required primitives ─────────────────────── */

void
memory_renderer_rep::draw (int char_code, font_glyphs fng, SI x, SI y,
                           int codepoint) {
  (void) codepoint;

  if (is_nil (fng)) return;
  glyph gl = fng->get (char_code);
  if (is_nil (gl)) return;

  // Current foreground color
  unsigned int fg_argb = color_to_argb (pen->get_color ());

  // Glyph bitmap position (converted to pixel coords, Y-DOWN)
  SI xo, yo;
  glyph shrunk = shrink (gl, shrinkf, shrinkf, xo, yo);
  int gw = shrunk->width;
  int gh = shrunk->height;

  // Top-left corner of glyph bitmap in SI, then convert to pixel
  SI gx_si = x + xo * shrinkf;
  SI gy_si = y - yo * shrinkf;  // Mogan: yoff is subtracted (Y-DOWN)
  int gpx, gpy;
  if (!si_to_pixel (gx_si, gy_si, gpx, gpy)) return;

  int nr_cols = shrinkf * shrinkf;
  if (nr_cols >= 64) nr_cols = 64;

  unsigned int* buf = A (pixels);

  // Copy glyph bitmap to pixel buffer
  for (int j = 0; j < gh; j++) {
    for (int i = 0; i < gw; i++) {
      int coverage = shrunk->get_x (i, j); // 0 .. nr_cols
      if (coverage == 0) continue;

      int px = gpx + i;
      int py = gpy + j;
      if (px < 0 || px >= w || py < 0 || py >= h) continue;

      // Alpha blend glyph over existing pixel
      unsigned int src = fg_argb;
      unsigned int dst = buf[py * w + px];
      int alpha = (coverage * ((src >> 24) & 0xFF)) / nr_cols;
      if (alpha == 0) continue;

      int inv_a = 255 - alpha;
      int dr = ((dst >> 16) & 0xFF) * inv_a / 255;
      int dg = ((dst >>  8) & 0xFF) * inv_a / 255;
      int db = ((dst      ) & 0xFF) * inv_a / 255;
      int sr = ((src >> 16) & 0xFF) * alpha / 255;
      int sg = ((src >>  8) & 0xFF) * alpha / 255;
      int sb = ((src      ) & 0xFF) * alpha / 255;
      int a_out = alpha + ((dst >> 24) & 0xFF) * inv_a / 255;

      buf[py * w + px] =
        ((unsigned int)(a_out) << 24) |
        ((unsigned int)(dr + sr) << 16) |
        ((unsigned int)(dg + sg) <<  8) |
        ((unsigned int)(db + sb));
    }
  }
}

void
memory_renderer_rep::line (SI x1, SI y1, SI x2, SI y2) {
  // Convert endpoints to pixel coords (Y-DOWN buffer)
  int px1, py1, px2, py2;
  if (!si_to_pixel (x1, y1, px1, py1)) return;
  if (!si_to_pixel (x2, y2, px2, py2)) return;

  unsigned int color = color_to_argb (pen->get_color ());

  // Bresenham line
  int dx = abs (px2 - px1);
  int dy = abs (py2 - py1);
  int sx = (px1 < px2) ? 1 : -1;
  int sy = (py1 < py2) ? 1 : -1;
  int err = dx - dy;

  while (true) {
    set_pixel (px1, py1, color);
    if (px1 == px2 && py1 == py2) break;
    int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; px1 += sx; }
    if (e2 <  dx) { err += dx; py1 += sy; }
  }
}

void
memory_renderer_rep::lines (array<SI> xs, array<SI> ys) {
  int n = N (xs);
  if (N (ys) != n || n < 2) return;
  for (int i = 0; i < n - 1; i++)
    line (xs[i], ys[i], xs[i + 1], ys[i + 1]);
}

void
memory_renderer_rep::clear (SI x1, SI y1, SI x2, SI y2) {
  int px1, py1, px2, py2;
  if (!clip_rect (x1, y1, x2, y2, px1, py1, px2, py2)) return;

  unsigned int bg = color_to_argb (bg_brush->get_color ());
  unsigned int* buf = A (pixels);
  for (int py = py1; py < py2; py++)
    for (int px = px1; px < px2; px++)
      buf[py * w + px] = bg;
}

void
memory_renderer_rep::fill (SI x1, SI y1, SI x2, SI y2) {
  int px1, py1, px2, py2;
  if (!clip_rect (x1, y1, x2, y2, px1, py1, px2, py2)) return;

  unsigned int fg = color_to_argb (pen->get_color ());
  unsigned int* buf = A (pixels);
  for (int py = py1; py < py2; py++)
    for (int px = px1; px < px2; px++)
      buf[py * w + px] = fg;
}

/* ── Arc / fill_arc (polygon approximation) ────────────────────────────── */

void
memory_renderer_rep::arc (SI x1, SI y1, SI x2, SI y2, int alpha, int delta) {
  (void) x1; (void) y1; (void) x2; (void) y2;
  (void) alpha; (void) delta;
  // Not implemented — arcs are uncommon in TeXmacs rendering.
  // A polygon approximation can be added when needed.
}

void
memory_renderer_rep::fill_arc (SI x1, SI y1, SI x2, SI y2,
                               int alpha, int delta) {
  (void) x1; (void) y1; (void) x2; (void) y2;
  (void) alpha; (void) delta;
  // Not implemented — filled arcs are uncommon in TeXmacs rendering.
}

/* ── Polygon fill (simple scanline algorithm) ─────────────────────────── */

void
memory_renderer_rep::polygon (array<SI> xs, array<SI> ys, bool convex) {
  (void) convex;
  int n = N (xs);
  if (N (ys) != n || n < 3) return;

  // Convert all vertices to pixel coords
  int i;
  array<int> px (n), py (n);
  for (i = 0; i < n; i++) {
    if (!si_to_pixel (xs[i], ys[i], px[i], py[i])) return;
  }

  unsigned int fg = color_to_argb (pen->get_color ());
  unsigned int* buf = A (pixels);

  // Find vertical bounds
  int ymin = py[0], ymax = py[0];
  for (i = 1; i < n; i++) {
    if (py[i] < ymin) ymin = py[i];
    if (py[i] > ymax) ymax = py[i];
  }
  if (ymin < 0) ymin = 0;
  if (ymax >= h) ymax = h - 1;
  if (ymin >= ymax) return;

  // Simple scanline fill (even-odd rule)
  for (int y = ymin; y <= ymax; y++) {
    // Collect x-intersections
    array<int> xsect;
    for (i = 0; i < n; i++) {
      int j = (i + 1) % n;
      int y0 = py[i], y1 = py[j];
      if ((y0 <= y && y < y1) || (y1 <= y && y < y0)) {
        int x0 = px[i], x1 = px[j];
        int x = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        xsect << x;
      }
    }

    // Sort intersections
    int m = N (xsect);
    if (m < 2) continue;
    for (int a = 0; a < m - 1; a++)
      for (int b = 0; b < m - 1 - a; b++)
        if (xsect[b] > xsect[b + 1]) {
          int t = xsect[b];
          xsect[b]= xsect[b + 1];
          xsect[b + 1]= t;
        }

    // Fill between pairs
    for (int k = 0; k + 1 < m; k += 2) {
      int xa = xsect[k], xb = xsect[k + 1];
      if (xa < 0) xa = 0;
      if (xb > w) xb = w;
      for (int x = xa; x < xb; x++)
        buf[y * w + x] = fg;
    }
  }
}
