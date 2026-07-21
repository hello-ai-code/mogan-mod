/******************************************************************************
 * MODULE     : outline_panel.cpp
 * DESCRIPTION: Outline (table of contents) sidebar for document navigation
 * COPYRIGHT  : (C) 2026  The Mogan Project
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#include "outline_panel.hpp"

#include "new_document.hpp"
#include "new_view.hpp"
#include "path.hpp"
#include "scheme.hpp"
#include "tree.hpp"

#include "qt_tm_widget.hpp"

#include <QHeaderView>
#include <QStringList>
#include <QString>

using namespace moebius;

/* ------------------------------------------------------------------ */
/*  Static members                                                    */
/* ------------------------------------------------------------------ */
hashset<tree_label> OutlinePanel::the_section_tags;
bool OutlinePanel::section_tags_ready = false;

/* ------------------------------------------------------------------ */
/*  Construction / Destruction                                        */
/* ------------------------------------------------------------------ */
OutlinePanel::OutlinePanel (qt_tm_widget_rep* parentWidget, QWidget* parent)
    : QDockWidget ("目录", parent), m_parentWidget (parentWidget) {
  setObjectName ("outlineDock");

  // ---- central tree widget ----
  m_tree = new QTreeWidget (this);
  m_tree->setHeaderHidden (true);
  m_tree->setIndentation (12);
  m_tree->setAnimated (true);
  m_tree->setRootIsDecorated (true);
  m_tree->setSelectionMode (QAbstractItemView::SingleSelection);
  setWidget (m_tree);

  // ---- timer (500 ms cooldown) ----
  m_refreshTimer = new QTimer (this);
  m_refreshTimer->setSingleShot (true);
  m_refreshTimer->setInterval (500);
  connect (m_refreshTimer, SIGNAL (timeout ()), this, SLOT (refresh ()));

  // ---- clicks ----
  connect (m_tree,
           SIGNAL (itemClicked (QTreeWidgetItem*, int)),
           this,
           SLOT (onItemClicked (QTreeWidgetItem*, int)));

  // kick off first refresh when the Scheme VM is warm
  QTimer::singleShot (2000, this, SLOT (refresh ()));
}

OutlinePanel::~OutlinePanel () {
  // owned by Qt parent hierarchy
}

/* ------------------------------------------------------------------ */
/*  Slot: toggle visibility                                           */
/* ------------------------------------------------------------------ */
void
OutlinePanel::toggleVisibility () {
  setVisible (!isVisible ());
}

/* ------------------------------------------------------------------ */
/*  Slot: trigger a deferred refresh                                  */
/* ------------------------------------------------------------------ */
void
OutlinePanel::refresh () {
  m_refreshTimer->stop ();

  ensureSectionTags ();

  tree doc = the_et; // defined in new_document.hpp
  if (is_atomic (doc)) return;

  m_tree->clear ();
  collectOutline (doc, path (), m_tree->invisibleRootItem ());
}

/* ------------------------------------------------------------------ */
/*  Slot: click handler — navigate to section                         */
/* ------------------------------------------------------------------ */
void
OutlinePanel::onItemClicked (QTreeWidgetItem* item, int /*column*/) {
  QVariant data = item->data (0, Qt::UserRole);
  if (data.isNull ()) return;

  path p = stringToPath (data.toString ());

  editor ed = get_current_editor ();
  if (!is_nil (ed)) {
    ed->go_to (p, true);
  }
}

/* ------------------------------------------------------------------ */
/*  Ensure section-tag set is populated from Scheme (one-shot)        */
/* ------------------------------------------------------------------ */
void
OutlinePanel::ensureSectionTags () {
  if (section_tags_ready) return;

  // Mirrors init_sections() in tree_traverse.cpp.
  // Note: we deliberately use a flat list (append of plain + star lists)
  // so that ensureSectionTags() stays in sync with the existing Scheme-side
  // tag configuration.
  eval ("(use-modules (text text-drd))");
  object l = eval ("(append (section-tag-list) (section*-tag-list))");

  while (!is_null (l)) {
    tree_label tl = as_tree_label (as_symbol (car (l)));
    if (tl != UNKNOWN) {
      the_section_tags->insert (tl);
    }
    l = cdr (l);
  }
  section_tags_ready = true;
}

/* ------------------------------------------------------------------ */
/*  Recursive collection of sections (flat tree, depth-first order)   */
/*  TeXmacs sections are tree siblings, so the tree view is flat.     */
/*  (Hierarchical nesting via section-tag level is left for a future  */
/*   enhancement when a label→string API is available.)               */
/* ------------------------------------------------------------------ */
void
OutlinePanel::collectOutline (const tree& t, path base,
                              QTreeWidgetItem* parent) {
  if (is_atomic (t)) return;

  int n = N (t);

  // Section-like node: has >=1 child and its label is a known section tag.
  if (n >= 1 && the_section_tags->contains (L (t))) {

    // Extract the section title from child 0 (the title subtree).
    string title_text;
    if (is_atomic (t[0])) {
      title_text = as_string (tree (t[0]));
    } else {
      title_text = as_string (t[0]); // flattens markup
    }

    if (N (title_text) > 0) {
      QTreeWidgetItem* item = new QTreeWidgetItem (parent);
      item->setText (0, QString::fromUtf8 (title_text.c_str ()));
      item->setData (0, Qt::UserRole, QVariant (pathToString (base)));
    }
    // Do NOT recurse into section children — body content may contain
    // other trees but subsections are siblings in the document tree,
    // not children.
    return;
  }

  // Non-section container — recurse into every child.
  for (int i = 0; i < n; ++i) {
    collectOutline (t[i], base * i, parent);
  }
}

/* ------------------------------------------------------------------ */
/*  Path serialisation                                                */
/* ------------------------------------------------------------------ */
QString
OutlinePanel::pathToString (const path& p) {
  QStringList parts;
  int n = N (p);
  for (int i = 0; i < n; ++i) {
    parts << QString::number (p[i]);
  }
  return parts.join (",");
}

path
OutlinePanel::stringToPath (const QString& s) {
  path result (0);
  QStringList parts = s.split (',', Qt::SkipEmptyParts);
  for (const QString& part : parts) {
    result = result * part.toInt ();
  }
  return result;
}
