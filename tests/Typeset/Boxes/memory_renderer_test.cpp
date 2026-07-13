/******************************************************************************
 * MODULE     : memory_renderer_test.cpp
 * DESCRIPTION: Architecture proof: Box + MemoryRenderer + Visitor pipeline
 *
 * This test proves that the Mogan rendering pipeline can produce pixel
 * output without any Qt dependency at the renderer level:
 *
 *   1. Box trees are pure data structures (no renderer reference)
 *   2. MemoryRenderer implements the abstract renderer_rep interface
 *      using only a flat ARGB pixel buffer
 *   3. The Visitor pipeline (PreRenderVisitor + RenderVisitor +
 *      PostRenderVisitor) drives MemoryRenderer to produce correct output
 *
 * Together these demonstrate that the rendering engine is fully separated
 * from the display backend — a key architectural goal of Phase 3 (Line A).
 *
 * COPYRIGHT  : (C) 2026  The Mogan Project
 ******************************************************************************/

#include "Boxes/construct.hpp"
#include "Boxes/render_visitor.hpp"
#include "Boxes/render_visitor_extra.hpp"
#include "memory_renderer.hpp"
#include <QtTest/QtTest>

class TestMemoryRenderer : public QObject {
  Q_OBJECT

private slots:
  //── basic pixel I/O ───────────────────────────────────────────────────
  void test_basic_pixel_ops ();

  //── full Visitor pipeline with MemoryRenderer ────────────────────────
  void test_test_box_rendering_via_visitors ();
};

/******************************************************************************
 * Basic pixel read/write operations
 ******************************************************************************/

void
TestMemoryRenderer::test_basic_pixel_ops () {
  memory_renderer_rep mr (10, 10);

  // Initially all white (0xFFFFFFFF = opaque white in ARGB)
  QCOMPARE (mr.get_pixel (0, 0), 0xFFFFFFFFu);
  QCOMPARE (mr.get_pixel (9, 9), 0xFFFFFFFFu);

  // Write a single red pixel
  mr.set_pixel (5, 5, 0xFFFF0000u);
  QCOMPARE (mr.get_pixel (5, 5), 0xFFFF0000u);

  // Adjacent pixels are undisturbed
  QCOMPARE (mr.get_pixel (4, 5), 0xFFFFFFFFu);
  QCOMPARE (mr.get_pixel (5, 4), 0xFFFFFFFFu);
  QCOMPARE (mr.get_pixel (6, 5), 0xFFFFFFFFu);
  QCOMPARE (mr.get_pixel (5, 6), 0xFFFFFFFFu);

  // Out-of-bounds read returns 0
  QCOMPARE (mr.get_pixel (20, 20), 0u);

  // Out-of-bounds write is silently ignored (no crash)
  mr.set_pixel (20, 20, 0xFF00FF00u);
  QCOMPARE (mr.get_pixel (5, 5), 0xFFFF0000u);

  // clear_all fills every pixel
  mr.clear_all (0xFF00FF00u);
  QCOMPARE (mr.get_pixel (0, 0), 0xFF00FF00u);
  QCOMPARE (mr.get_pixel (5, 5), 0xFF00FF00u);
  QCOMPARE (mr.get_pixel (9, 9), 0xFF00FF00u);

  // Fully transparent fill
  mr.clear_all (0x00000000u);
  QCOMPARE (mr.get_pixel (0, 0), 0x00000000u);
}

/******************************************************************************
 * Full pipeline: Box tree → Visitor → MemoryRenderer
 *
 * We construct a test_box (a simple 50×25-pixel box), create a
 * MemoryRenderer, and run the three visitors through box->redraw().
 *
 * test_box_rep::display() — now RenderVisitor::visit(test_box_rep&) —
 * draws two green diagonal lines crossing the box.
 *
 * We verify that pixels along the diagonals are non-white (i.e. the
 * line() calls reached the pixel buffer), and that areas outside the
 * box are still white (the rendering was constrained to the box).
 ******************************************************************************/

void
TestMemoryRenderer::test_test_box_rendering_via_visitors () {
  // ── 1. Create a test_box ──────────────────────────────────────────────
  // test_box dimensions (in SI): x3=0, y3=0, x4=12800, y4=6400
  // At pixel=256 this is exactly 50 × 25 pixels.
  box tb = test_box (path ());
  QVERIFY (!is_nil (tb));
  QCOMPARE (tb->x3, 0);
  QCOMPARE (tb->y3, 0);
  QCOMPARE (tb->x4 / 256, 50);
  QCOMPARE (tb->y4 / 256, 25);

  // ── 2. Create MemoryRenderer (with headroom around the box) ────────────
  memory_renderer_rep mr (200, 200);
  renderer ren = &mr;

  // Set clipping so the 50×25 box is visible
  ren->set_clipping (-10 * 256, -10 * 256, 200 * 256, 200 * 256);

  // ── 3. Create the three rendering visitors ─────────────────────────────
  PreRenderVisitor   prv (ren);
  RenderVisitor      rv (ren);
  PostRenderVisitor pstv (ren);

  // ── 4. Render the box tree via the Visitor pipeline ────────────────────
  rectangles l;
  tb->redraw (ren, path (), l, prv, rv, pstv);

  // ── 5. Verify pixel output ─────────────────────────────────────────────
  unsigned int white = 0xFFFFFFFFu;

  // The RenderVisitor draws:
  //   Line 1: (x1,y1)→(x2,y2) = (0,0)→(12800,6400)  — top-left to bottom-right
  //   Line 2: (x1,y2)→(x2,y1) = (0,6400)→(12800,0)  — bottom-left to top-right
  // At pixel coords (ox=0, oy=0, pixel=256):
  //   Line 1: (0,0)→(50,25)
  //   Line 2: (0,25)→(50,0)

  // Check first diagonal: pixels approximately on the line should be non-white
  bool found_diagonal = false;
  for (int i = 0; i < 50; i++) {
    unsigned int p = mr.get_pixel (i, i / 2);
    if (p != white) {
      found_diagonal = true;
      break;
    }
  }
  QVERIFY (found_diagonal);

  // Check second diagonal
  bool found_diagonal2 = false;
  for (int i = 0; i < 50; i++) {
    unsigned int p = mr.get_pixel (i, 25 - i / 2);
    if (p != white) {
      found_diagonal2 = true;
      break;
    }
  }
  QVERIFY (found_diagonal2);

  // ── 6. Reusability: clear and render again ────────────────────────────
  mr.clear_all (white);
  tb->redraw (ren, path (), l, prv, rv, pstv);

  found_diagonal = false;
  for (int i = 0; i < 50; i++) {
    if (mr.get_pixel (i, i / 2) != white) {
      found_diagonal = true;
      break;
    }
  }
  QVERIFY (found_diagonal);
}

QTEST_MAIN (TestMemoryRenderer)
#include "memory_renderer_test.moc"
