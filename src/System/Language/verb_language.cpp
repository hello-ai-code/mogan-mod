
/******************************************************************************
 * MODULE     : verb_language.cpp
 * DESCRIPTION: the "verbatim" language
 * COPYRIGHT  : (C) 1999  Joris van der Hoeven
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#include "analyze.hpp"
#include "code_wrap.hpp"
#include "impl_language.hpp"
#include "observers.hpp"
#include "packrat.hpp"
#include "scheme.hpp"

verb_language_rep::verb_language_rep (string name) : language_rep (name) {
  hl_lan= packrat_abbreviation (res_name, "Main");
}

inline static bool
is_sep_char (char c) {
  return c == '-' || c == '/' || c == '\\' || c == ',' || c == '?';
}

text_property
verb_language_rep::advance (tree t, int& pos) {
  string s= t->label;
  if (pos == N (s)) return &tp_normal_rep;
  if (s[pos] == ' ') {
    pos++;
    return &tp_space_rep;
  }
  if (is_sep_char (s[pos])) {
    pos++;
    while (pos < N (s) && is_sep_char (s[pos]) && s[pos] != '-')
      pos++;
    return &tp_hyph_rep;
  }

  array<int> cols= obtain_highlight (t, hl_lan);
  if (N (cols) == 0)
    while ((pos < N (s)) && (s[pos] != ' ') && !is_sep_char (s[pos]))
      pos++;
  else if ((pos < N (s)) && (s[pos] != ' ') && !is_sep_char (s[pos])) {
    pos++;
    while ((pos < N (s)) && (s[pos] != ' ') && !is_sep_char (s[pos]) &&
           cols[pos] == cols[pos - 1])
      pos++;
  }
  return &tp_normal_rep;
}

array<int>
verb_language_rep::get_hyphens (string s) {
  int        i;
  array<int> penalty (N (s) + 1);
  for (i= 0; i < N (penalty); i++)
    penalty[i]= HYPH_PANIC;
  return penalty;
}

/**
 * @brief 按代码原子边界切分 verbatim 文本，避免将 "<#...>" 内部拆开。
 * @param s 待切分的原始字符串。
 * @param after 布局器建议的断行位置（可能落在原子内部）。
 * @param left 返回断点左侧内容。
 * @param right 返回断点右侧内容。
 *
 * @note 示例：当 s 为 "ab<#4E2D>cd" 且 after 落在 "<#4E2D>" 内部时，
 * 会将断点回退到原子起止边界，避免产生非法拆分。
 */
void
verb_language_rep::hyphenate (string s, int after, string& left,
                              string& right) {
  int a= tm_snap_after_boundary_for_code_wrap (s, after);
  left = s (0, a);
  right= s (a, N (s));
}

string
verb_language_rep::get_color (tree t, int start, int end) {
  if (start >= end) return "";
  array<int> cols= obtain_highlight (t, hl_lan);
  if (start < N (cols) && cols[start] != 0)
    return decode_color (res_name, cols[start]);
  return "";
}
