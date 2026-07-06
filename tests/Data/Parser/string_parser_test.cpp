/******************************************************************************
 * MODULE     : string_parser_test.cpp
 * DESCRIPTION: Properties of String Parser
 * COPYRIGHT  : (C) 2026  Mogan Developers
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#include "string_parser.hpp"

#include <QtTest/QtTest>

class TestStringParser : public QObject {
  Q_OBJECT

private slots:
  void test_double_escape_disabled ();
  void test_double_escape_enabled ();
};

static void
configure_single_quote (string_parser_rep& parser) {
  hashmap<string, string> pairs;
  pairs ("\'")= "\'";
  parser.set_pairs (pairs);
}

void
TestStringParser::test_double_escape_disabled () {
  string_parser_rep parser;
  configure_single_quote (parser);

  string s  = "\'it\'\'s ok\'";
  int    pos= 0;

  QVERIFY (parser.parse (s, pos));
  QCOMPARE (pos, 4);
  QVERIFY (!parser.unfinished ());
}

void
TestStringParser::test_double_escape_enabled () {
  string_parser_rep parser;
  configure_single_quote (parser);

  array<string> double_escapes;
  double_escapes << string ("\'");
  parser.set_double_escapes (double_escapes);

  string s  = "\'it\'\'s ok\'";
  int    pos= 0;

  QVERIFY (parser.parse (s, pos));
  QCOMPARE (pos, N (s));
  QVERIFY (!parser.unfinished ());
}

QTEST_MAIN (TestStringParser)
#include "string_parser_test.moc"
