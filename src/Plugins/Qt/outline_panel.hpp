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

#include "hashset.hpp"
#include "path.hpp"
#include "tree.hpp"

#include <QDockWidget>
#include <QTimer>
#include <QTreeWidget>

#include <moebius/tree_label.hpp>

using moebius::tree_label;

class qt_tm_widget_rep;

/**
 * @brief A dock widget that displays the document's section outline.
 *
 * Periodically refreshes from the edit tree (via a 500 ms timer) and
 * allows the user to click a section heading to navigate there.
 */
class OutlinePanel : public QDockWidget {
  Q_OBJECT

public:
  OutlinePanel (qt_tm_widget_rep* parentWidget, QWidget* parent = nullptr);
  ~OutlinePanel ();

public slots:
  /** Rebuild the outline tree from the current document. */
  void refresh ();

  /** Toggle visibility (show/hide). */
  void toggleVisibility ();

private slots:
  void onItemClicked (QTreeWidgetItem* item, int column);

private:
  /** Ensure the cached section-tag set is populated (one-shot). */
  static void ensureSectionTags ();

  /** Recursive traversal: collect sections into the tree view. */
  void collectOutline (const tree& t, path base, QTreeWidgetItem* parent);

  /** Serialize a path to a comma-separated string for QVariant storage. */
  static QString pathToString (const path& p);

  /** Deserialize back. */
  static path stringToPath (const QString& s);

  qt_tm_widget_rep* m_parentWidget;
  QTreeWidget* m_tree;
  QTimer* m_refreshTimer;

  /** Lazily-initialised set of all section tree_labels. */
  static hashset<tree_label> the_section_tags;
  static bool section_tags_ready;
};

#endif // OUTLINE_PANEL_HPP
