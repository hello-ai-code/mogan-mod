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
#include "tree.hpp"
#include "editor.hpp"

#include "qt_tm_widget.hpp"

#include <QHeaderView>
#include <QStringList>
#include <QString>
#include <QSettings>

using namespace moebius;

/* ================================================================== */
/*  Section-tag helpers (reuses same logic as tree_traverse.cpp's     */
/*  search_sections / init_sections, but without the Scheme eval).    */
/*  These mirror the section-tag-list + section*-tag-list from        */
/*  TeXmacs/progs/text/text-drd.scm.                                 */
/* ================================================================== */

/**
 * Whether @p tl is a known section label (part, chapter, section,
 * subsection, … with or without the star suffix).
 */
static bool
is_section_label (tree_label tl) {
  return tl == make_tree_label ("part")         || tl == make_tree_label ("part*")
      || tl == make_tree_label ("chapter")      || tl == make_tree_label ("chapter*")
      || tl == make_tree_label ("section")      || tl == make_tree_label ("section*")
      || tl == make_tree_label ("subsection")   || tl == make_tree_label ("subsection*")
      || tl == make_tree_label ("subsubsection")|| tl == make_tree_label ("subsubsection*")
      || tl == make_tree_label ("paragraph")    || tl == make_tree_label ("paragraph*")
      || tl == make_tree_label ("subparagraph") || tl == make_tree_label ("subparagraph*");
}

/**
 * Nesting level for a section label.
 * Lower = higher in the hierarchy (1 ≅ part … 7 ≅ subparagraph).
 */
static int
section_level (tree_label tl) {
  if (tl == make_tree_label ("part")         || tl == make_tree_label ("part*"))         return 1;
  if (tl == make_tree_label ("chapter")      || tl == make_tree_label ("chapter*"))      return 2;
  if (tl == make_tree_label ("section")      || tl == make_tree_label ("section*"))      return 3;
  if (tl == make_tree_label ("subsection")   || tl == make_tree_label ("subsection*"))   return 4;
  if (tl == make_tree_label ("subsubsection")|| tl == make_tree_label ("subsubsection*"))return 5;
  if (tl == make_tree_label ("paragraph")    || tl == make_tree_label ("paragraph*"))    return 6;
  if (tl == make_tree_label ("subparagraph") || tl == make_tree_label ("subparagraph*")) return 7;
  return 99;
}

/* ================================================================== */
/*  Helper: check if a string consists only of digits                 */
/* ================================================================== */

static bool
is_all_digits (string s) {
  if (N (s) == 0) return false;
  for (int i = 0; i < N (s); ++i) {
    char c = s[i];
    if (c < '0' || c > '9') return false;
  }
  return true;
}

/* ================================================================== */
/*  Helper: check if a string is of the form "<digits>"               */
/* ================================================================== */

static bool
is_angle_bracket_digits (string s) {
  if (N (s) < 3) return false;
  if (s[0] != '<' || s[N (s) - 1] != '>') return false;
  for (int i = 1; i < N (s) - 1; ++i) {
    char c = s[i];
    if (c < '0' || c > '9') return false;
  }
  return true;
}

/* ================================================================== */
/*  Helper: extract a safe section title, filtering out artifacts     */
/* ================================================================== */

string
OutlinePanel::extract_section_title (tree t) {
  if (N (t) == 0) return string ();

  // Try to get the first child as the title
  tree title_child = t[0];

  // Case 1: atomic (simple text) - use as_string
  if (is_atomic (title_child)) {
    string text = as_string (tree (title_child));
    // Filter out pure digits or "<digits>" (magic paste artifacts)
    if (is_all_digits (text) || is_angle_bracket_digits (text)) {
      return string (); // invalid title, skip this section
    }
    return text;
  }

  // Case 2: compound tree - use as_string which flattens markup
  string text = as_string (title_child);
  if (is_all_digits (text) || is_angle_bracket_digits (text)) {
    return string ();
  }
  return text;
}

/* ================================================================== */
/*  Construction / Destruction                                        */
/* ================================================================== */

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

  // Load compact mode state from settings
  QSettings settings;
  m_compactMode = settings.value ("OutlinePanel/compactMode", false).toBool ();
  if (m_compactMode) {
    setMinimumWidth (COMPACT_WIDTH);
    setMaximumWidth (COMPACT_WIDTH);
    // Hide the tree widget to save resources
    if (m_tree) m_tree->setVisible (false);
  }
}

OutlinePanel::~OutlinePanel () {
  // owned by Qt parent hierarchy
}

/* ================================================================== */
/*  Slot: toggle visibility                                           */
/* ================================================================== */

void
OutlinePanel::toggleVisibility () {
  setVisible (!isVisible ());
}

/* ================================================================== */
/*  Toggle compact mode (narrow strip <-> normal width)               */
/* ================================================================== */

void
OutlinePanel::toggleCompactMode () {
  m_compactMode = !m_compactMode;

  if (m_compactMode) {
    // Collapse to narrow strip
    setMinimumWidth (COMPACT_WIDTH);
    setMaximumWidth (COMPACT_WIDTH);
    // Hide the tree widget to save resources
    if (m_tree) m_tree->setVisible (false);
  } else {
    // Expand to normal width
    setMinimumWidth (NORMAL_WIDTH);
    setMaximumWidth (QWIDGETSIZE_MAX);
    if (m_tree) m_tree->setVisible (true);
  }

  // Persist state
  QSettings settings;
  settings.setValue ("OutlinePanel/compactMode", m_compactMode);
}

bool
OutlinePanel::isCompactMode () const {
  return m_compactMode;
}

/* ================================================================== */
/*  Slot: rebuild the outline from the current document               */
/* ================================================================== */

void
OutlinePanel::refresh () {
  m_refreshTimer->stop ();

  editor ed = get_current_editor ();
  if (is_nil (ed)) return;
  tree doc = ed->the_buffer (); // current document tree, not global the_et
  if (is_atomic (doc)) return;

  // --- Pass 1: flat collection ---
  QVector<SectionEntry> entries;
  collectSections (doc, path (), entries);

  // --- Pass 2: rebuild tree widget with hierarchy ---
  m_tree->clear ();
  buildOutlineTree (entries);
}

/* ================================================================== */
/*  Slot: click handler — navigate to section                         */
/* ================================================================== */

void
OutlinePanel::onItemClicked (QTreeWidgetItem* item, int /*column*/) {
  QVariant data = item->data (0, Qt::UserRole);
  if (data.isNull ()) return;

  path bufferRel = stringToPath (data.toString ());

  editor ed = get_current_editor ();
  if (!is_nil (ed)) {
    // The path was collected relative to ed->the_buffer(); prepend rp
    // so it becomes an absolute path in et for go_to().
    ed->go_to (ed->rp / bufferRel);
  }
}

/* ================================================================== */
/*  Recursive collection (depth-first) of all section nodes.          */
/*                                                                     */
/*  Reuses the same traversal pattern as search_sections() — walks    */
/*  the tree, recognises section nodes by their tree_label, records   */
/*  the tree path for navigation, and does NOT recurse into section   */
/*  children (subsections are siblings, not children, in TeXmacs).    */
/* ================================================================== */

void
OutlinePanel::collectSections (tree t, path base,
                               QVector<SectionEntry>& entries) {
  if (is_atomic (t)) return;

  int n = N (t);

  // Section-like node: has >= 1 child and is a known section label.
  tree_label label = L (t);
  if (n >= 1 && is_section_label (label)) {

    // Extract the section title using safe extraction
    string title_text = extract_section_title (t);

    if (N (title_text) != 0) {
      entries.append ({ base, section_level (label), title_text });
    }
    // Do NOT recurse into section children — subsections are siblings
    // at the document level, not children of this node.
    return;
  }

  // Non-section container — recurse into every child.
  for (int i = 0; i < n; ++i) {
    collectSections (t[i], base * i, entries);
  }
}

/* ================================================================== */
/*  Build QTreeWidget hierarchy from a flat list of SectionEntry.     */
/*                                                                     */
/*  Algorithm (mirrors the Scheme section-list->nested logic):        */
/*    - Maintain a stack of the most recent QTreeWidgetItem at each   */
/*      nesting level.                                                */
/*    - For a section at level L, its parent is stack[L-1] (the       */
/*      nearest ancestor section at a higher level).                  */
/*    - stack[L] is then set to the new item, and deeper entries in   */
/*      the stack are cleared (they are siblings of the new item's    */
/*      children, not ancestors).                                     */
/* ================================================================== */

void
OutlinePanel::buildOutlineTree (const QVector<SectionEntry>& entries) {
  // stack[lvl] = parent QTreeWidgetItem for level lvl
  // Index 0 is unused (levels start at 1).
  const int MAX_LEVEL = 8;
  QTreeWidgetItem* stack[MAX_LEVEL] = { nullptr };

  for (const auto& entry : entries) {
    QTreeWidgetItem* item = new QTreeWidgetItem;
    item->setText (0, QString::fromUtf8 (as_charp (entry.title)));
    item->setData (0, Qt::UserRole, QVariant (pathToString (entry.p)));

    int lvl = entry.level;
    QTreeWidgetItem* parent = (lvl > 1 && lvl < MAX_LEVEL)
                                  ? stack[lvl - 1] : nullptr;

    if (parent != nullptr) {
      parent->addChild (item);
    } else {
      m_tree->addTopLevelItem (item);
    }

    // Record this item as the most recent at this level, then clear
    // any deeper levels (they are no longer valid descendants).
    if (lvl < MAX_LEVEL) {
      stack[lvl] = item;
      for (int i = lvl + 1; i < MAX_LEVEL; ++i) {
        stack[i] = nullptr;
      }
    }
  }

  // Expand all items so the user sees the full TOC immediately.
  m_tree->expandAll ();
}

/* ================================================================== */
/*  Path serialisation                                                */
/* ================================================================== */

QString
OutlinePanel::pathToString (path p) {
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