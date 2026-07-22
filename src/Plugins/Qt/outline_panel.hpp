/******************************************************************************
 * MODULE     : outline_panel.hpp
 * DESCRIPTION: Outline (table of contents) sidebar for document navigation
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#ifndef OUTLINE_PANEL_HPP
#define OUTLINE_PANEL_HPP

#include "path.hpp"
#include "string.hpp"
#include "tree.hpp"

#include <QDockWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVector>

#include <moebius/tree_label.hpp>

using moebius::tree_label;

class qt_tm_widget_rep;

/**
 * @brief A dock widget that displays the document's section outline.
 *
 * Periodically refreshes from the edit tree (via a 500 ms timer) and
 * allows the user to click a section heading to navigate there.
 *
 * Reuses the same section-tag logic as focus toolbar's
 * tree-search-sections (tree_traverse.cpp): a section node is a
 * compound tree with exactly one child whose tree_label is one of
 * part/chapter/section/subsection/... (with or without * suffix).
 * Hierarchical nesting mirrors the Scheme filter-sections logic.
 */
class OutlinePanel : public QDockWidget {
  Q_OBJECT

public:
  OutlinePanel (qt_tm_widget_rep* parentWidget, QWidget* parent = nullptr);
  ~OutlinePanel ();

  /** Toggle visibility (show/hide). */
  void toggleVisibility ();

  /** Toggle narrow strip mode for compact usage */
  void toggleCompactMode ();

  /** Check if in narrow strip mode */
  bool isCompactMode () const;

public slots:
  /** Rebuild the outline tree from the current document. */
  void refresh ();

  /** Navigate to the section when an item is clicked. */
  void onItemClicked (QTreeWidgetItem* item, int column);

private:
  /** One collected section entry. */
  struct SectionEntry {
    path  p;      // tree path for navigation
    int   level;  // nesting level (1=part … 7=subparagraph)
    string title; // display text
  };

  /** Recursive traversal: collect all sections into a flat list. */
  void collectSections (tree t, path base,
                        QVector<SectionEntry>& entries);

  /** Extract a safe section title, filtering out magic-paste artifacts */
  static string extract_section_title (tree t);

  /** Serialize a path to a comma-separated string for QVariant storage. */
  static QString pathToString (path p);

  /** Deserialize back. */
  static path stringToPath (const QString& s);

  /** Build the QTreeWidget hierarchy from a flat list of entries. */
  void buildOutlineTree (const QVector<SectionEntry>& entries);

  qt_tm_widget_rep* m_parentWidget;
  QTreeWidget* m_tree;
  QTimer* m_refreshTimer;

  /** Constants for compact mode dimensions */
  static constexpr int COMPACT_WIDTH = 50;   // narrow strip width
  static constexpr int NORMAL_WIDTH = 200;   // normal width

  /** Current compact mode state */
  bool m_compactMode{false};
};

#endif // OUTLINE_PANEL_HPP