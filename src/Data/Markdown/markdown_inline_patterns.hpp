/******************************************************************************
 * MODULE     : markdown_inline_patterns.hpp
 * DESCRIPTION: Markdown inline pattern recognition and conversion
 * COPYRIGHT  : (C) 2026  Mogan contributors
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#ifndef MARKDOWN_INLINE_PATTERNS_H
#define MARKDOWN_INLINE_PATTERNS_H

#include "tree.hpp"
#include "string.hpp"
#include "tree_helper.hpp"
#include <moebius/tree_label.hpp>

using namespace moebius;

/* Result of local pattern matching */
struct md_local_match {
    int start_char;      /* UTF-8 character index where match starts */
    int end_char;        /* UTF-8 character index where match ends (exclusive) */
    string pattern_type; /* matched pattern type */
    tree converted;      /* converted tree node */
    bool valid;          /* whether a valid match was found */
};

/* Result of full string parsing (for completion checking) */
struct md_parse_result {
    tree result;
    bool complete;  /* false if input is incomplete (e.g., "**bold" without closing **) */
};

/*
 * Internal UTF-8 helpers.
 * Kept in a dedicated namespace so they NEVER collide with the
 * 'using namespace moebius' above or with any global helpers; callers
 * always say mdutf8::xxx(...).
 */
namespace mdutf8 {
    inline int char_len (char c) {
        if ((c & 0x80) == 0) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 1;
    }
    inline int next (string s, int pos) {          /* byte index -> next char start */
        return pos + char_len (s[pos]);
    }
    /* byte index reached at character index `ci` */
    inline int to_byte (string s, int ci) {
        int pos = 0;
        for (int i = 0; i < ci && pos < N(s); i++)
            pos = next (s, pos);
        return pos;
    }
    inline int count (string s) {
        int n = 0, pos = 0;
        while (pos < N (s)) { n++; pos = next (s, pos); }
        return n;
    }
    /* does the substring starting at CHARACTER pos ci begin with prefix? */
    inline bool starts_with (string s, int ci, const char* pfx) {
        int bp = to_byte (s, ci);
        int plen = 0; while (pfx[plen]) plen++;
        if (bp + plen > N (s)) return false;
        for (int i = 0; i < plen; i++)
            if (s[bp+i] != pfx[i]) return false;
        return true;
    }
    inline string substr (string s, int ci0, int ci1) {
        return s (to_byte (s, ci0), to_byte (s, ci1));
    }
}

/* Find closing marker (UTF-8 safe on character positions).
   Returns the character index of the closer's start, or -1 if not found. */
static int
find_closing_marker (string s, int start_ci, const char* opener,
                     const char* closer) {
    int olen = 0; while (opener[olen]) olen++;
    int clen = 0; while (closer[clen]) clen++;
    int ci = start_ci + olen;
    int nchar = mdutf8::count (s);

    while (ci <= nchar - clen) {
        int bp = mdutf8::to_byte (s, ci);
        if (s[bp] == '\\') { ci += 2; continue; }
        if (mdutf8::starts_with (s, ci, closer))
            return ci;
        ci++;
    }
    return -1;
}

/* ASCII alpha check (for Markdown emphasis boundary) */
static inline bool is_alpha (char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* Extract content between markers, stripping backslash escapes (UTF-8 safe). */
static string
unescape_slice (string s, int start_ci, int end_ci) {
    string out;
    int ci = start_ci;
    while (ci < end_ci) {
        int bp = mdutf8::to_byte (s, ci);
        int len = mdutf8::char_len (s[bp]);
        if (s[bp] == '\\' && ci + 1 < end_ci) {
            out << mdutf8::substr (s, ci + 1, ci + 2);
            ci++;               /* skip the escaped char (1 or N bytes) */
        } else {
            out << mdutf8::substr (s, ci, ci + 1);
        }
        ci++;
    }
    return out;
}

/******************************************************************************
 * Pattern handlers — receive the raw string and the CHARACTER range of the
 * full match (including the delimiters) so they can compute byte offsets.
 ******************************************************************************/
static tree
parse_strong (string s, int m_ci0, int m_ci1) {
    string content = unescape_slice (s, m_ci0 + 2, m_ci1 - 2);
    if (N (content) == 0) return tree ("");
    return compound ("strong", tree (content));
}

static tree
parse_emphasis (string s, int m_ci0, int m_ci1) {
    string content = unescape_slice (s, m_ci0 + 1, m_ci1 - 1);
    if (N (content) == 0) return tree ("");
    return compound ("em", tree (content));
}

static tree
parse_inline_code (string s, int m_ci0, int m_ci1) {
    return compound ("code", tree (unescape_slice (s, m_ci0 + 1, m_ci1 - 1)));
}

static tree
parse_strikeout (string s, int m_ci0, int m_ci1) {
    string content = unescape_slice (s, m_ci0 + 2, m_ci1 - 2);
    if (N (content) == 0) return tree ("");
    return compound ("strikeout", tree (content));
}

static tree
parse_link (string s, int m_ci0, int m_ci1) {
    /* [text](url) — locate ] and ( by character scan from m_ci0 */
    int nchar = mdutf8::count (s);
    int bracket_ci = -1;
    for (int i = m_ci0 + 1; i < m_ci1 - 1; i++) {
        if (mdutf8::starts_with (s, i, "](")) { bracket_ci = i; break; }
    }
    if (bracket_ci < 0) return tree (mdutf8::substr (s, m_ci0, m_ci1));

    string txt = mdutf8::substr (s, m_ci0 + 1, bracket_ci);
    string url = mdutf8::substr (s, bracket_ci + 2, m_ci1 - 1);
    if (N (txt) == 0 || N (url) == 0)
        return tree (mdutf8::substr (s, m_ci0, m_ci1));
    return compound ("hlink", tree (txt), tree (url));
}

static tree
parse_image (string s, int m_ci0, int m_ci1) {
    /* ![alt](url) — locate ] and ( by character scan from m_ci0 + 2 */
    int bracket_ci = -1;
    for (int i = m_ci0 + 2; i < m_ci1 - 1; i++) {
        if (mdutf8::starts_with (s, i, "](")) { bracket_ci = i; break; }
    }
    if (bracket_ci < 0) return tree (mdutf8::substr (s, m_ci0, m_ci1));
    string alt = mdutf8::substr (s, m_ci0 + 2, bracket_ci);
    string url = mdutf8::substr (s, bracket_ci + 2, m_ci1 - 1);
    if (N (url) == 0)
        return tree (mdutf8::substr (s, m_ci0, m_ci1));
    return compound ("image", tree (""), tree (url), tree (alt));
}

/******************************************************************************
 * is_complete_markdown_input — UTF-8 safe character scan
 ******************************************************************************/
static bool
is_complete_markdown_input_utf8 (string s) {
    int nchar = mdutf8::count (s);

    /* Strong: ** ... ** must be paired */
    { int depth = 0;
      int ci = 0;
      while (ci < nchar) {
          int bp = mdutf8::to_byte (s, ci);
          if (s[bp] == '\\') { ci += 2; continue; }
          if (mdutf8::starts_with (s, ci, "**")) { depth = 1 - depth; ci += 2; }
          else ci++;
      }
      if (depth != 0) return false; }

    /* Emphasis: single * paired (double ** ignored for this pass) */
    { int depth = 0;
      int ci = 0;
      while (ci < nchar) {
          int bp = mdutf8::to_byte (s, ci);
          if (s[bp] == '\\') { ci += 2; continue; }
          if (mdutf8::starts_with (s, ci, "**")) { ci += 2; continue; }
          if (mdutf8::starts_with (s, ci, "*"))  { depth = 1 - depth; ci++; }
          else ci++;
      }
      if (depth != 0) return false; }

    /* Strikeout: ~~ paired */
    { int depth = 0;
      int ci = 0;
      while (ci < nchar) {
          int bp = mdutf8::to_byte (s, ci);
          if (s[bp] == '\\') { ci += 2; continue; }
          if (mdutf8::starts_with (s, ci, "~~")) { depth = 1 - depth; ci += 2; }
          else ci++;
      }
      if (depth != 0) return false; }

    /* Inline code: unbalanced backticks */
    { int depth = 0;
      int ci = 0;
      while (ci < nchar) {
          int bp = mdutf8::to_byte (s, ci);
          if (s[bp] == '\\') { ci += 2; continue; }
          if (mdutf8::starts_with (s, ci, "`")) {
              if (ci + 1 < nchar && mdutf8::starts_with (s, ci + 1, "`")) {
                  ci += 2;  /* '' → skip, balanced by itself */
              } else {
                  depth = 1 - depth; ci++;
              }
          } else ci++;
      }
      if (depth != 0) return false; }

    /* Brackets: [...] ] and ![alt] must pair, each ] must be followed by (…) */
    { int depth = 0;
      int ci = 0;
      while (ci < nchar) {
          int bp = mdutf8::to_byte (s, ci);
          if (s[bp] == '\\') { ci += 2; continue; }
          if (mdutf8::starts_with (s, ci, "![["))      { depth++; ci += 3; continue; }
          if (mdutf8::starts_with (s, ci, "!["))       { depth++; ci += 2; continue; }
          if (mdutf8::starts_with (s, ci, "["))        { depth++; ci += 1; continue; }
          if (mdutf8::starts_with (s, ci, "]")) {
              depth--;
              if (depth < 0) return false;
              ci++;
              /* must be followed by (url) */
              if (ci < nchar && mdutf8::starts_with (s, ci, "(")) {
                  int pd = 1; int j = ci + 1;
                  while (j < nchar && pd > 0) {
                      int bj = mdutf8::to_byte (s, j);
                      if (s[bj] == '\\') { j += 2; continue; }
                      if (mdutf8::starts_with (s, j, "(")) pd++;
                      if (mdutf8::starts_with (s, j, ")")) pd--;
                      if (pd > 0) j++;
                  }
                  if (pd != 0) return false;
                  ci = j + 1;
              } else return false;
          } else ci++;
      }
      if (depth != 0) return false; }

    return true;
}

/******************************************************************************
 * Main parser: find the FIRST complete inline markdown pattern (UTF-8 safe).
 * Operates on a single atomic node's text — returns character indices and the
 * converted tree so the caller can rebuild [prefix][fmt][suffix].
 ******************************************************************************/
static md_local_match
try_parse_inline_markdown_utf8 (string s) {
    md_local_match res;
    res.valid = false;
    if (N (s) == 0) return res;

    int nchar = mdutf8::count (s);
    int ci = 0;

    while (ci < nchar) {
        int m_start_ci = -1, m_end_ci = -1;
        string ptype;
        tree converted;
        int bp = mdutf8::to_byte (s, ci);

        /* 1. Strong: **...** */
        if (mdutf8::starts_with (s, ci, "**")) {
            int close_ci = find_closing_marker (s, ci, "**", "**");
            if (close_ci > ci + 2) { /* needs content between */
                m_start_ci = ci; m_end_ci = close_ci + 2; ptype = "strong";
                converted = parse_strong (s, m_start_ci, m_end_ci);
            }
        }

        /* 2. Emphasis: *...* (single star, not followed by *) */
        if (ptype.empty () && mdutf8::starts_with (s, ci, "*")
            && !(ci + 1 < nchar && mdutf8::starts_with (s, ci, "**"))) {
            bool vb = (ci == 0 ||
                       (!is_alpha (s[mdutf8::to_byte (s, ci) - 1]) &&
                        s[mdutf8::to_byte (s, ci) - 1] != '_'));
            if (vb) {
                int close_ci = find_closing_marker (s, ci, "*", "*");
                if (close_ci > ci + 1) {
                    m_start_ci = ci; m_end_ci = close_ci + 1; ptype = "emphasis";
                    converted = parse_emphasis (s, m_start_ci, m_end_ci);
                }
            }
        }

        /* 3. Inline code: `…` */
        if (ptype.empty () && mdutf8::starts_with (s, ci, "`")) {
            int close_ci = find_closing_marker (s, ci, "`", "`");
            if (close_ci > ci + 1) {
                m_start_ci = ci; m_end_ci = close_ci + 1; ptype = "code";
                converted = parse_inline_code (s, m_start_ci, m_end_ci);
            }
        }

        /* 4. Image: ![alt](url) */
        if (ptype.empty () && mdutf8::starts_with (s, ci, "![")) {
            int bracket_ci = -1;
            for (int i = ci + 2; i < nchar; i++) {
                if (mdutf8::starts_with (s, i, "](")) { bracket_ci = i; break; }
            }
            if (bracket_ci >= 0) {
                int rp = -1;
                for (int i = bracket_ci + 2; i < nchar; i++) {
                    if (mdutf8::starts_with (s, i, ")")) { rp = i; break; }
                }
                if (rp >= 0) {
                    m_start_ci = ci; m_end_ci = rp + 1; ptype = "image";
                    converted = parse_image (s, m_start_ci, m_end_ci);
                }
            }
        }

        /* 5. Link: [text](url) — not preceded by ! */
        if (ptype.empty () && mdutf8::starts_with (s, ci, "[") &&
            !(ci > 0 && s[mdutf8::to_byte (s, ci) - 1] == '!')) {
            int bracket_ci = -1;
            for (int i = ci + 1; i < nchar; i++) {
                if (mdutf8::starts_with (s, i, "](")) { bracket_ci = i; break; }
            }
            if (bracket_ci >= 0) {
                int rp = -1;
                for (int i = bracket_ci + 2; i < nchar; i++) {
                    if (mdutf8::starts_with (s, i, ")")) { rp = i; break; }
                }
                if (rp >= 0) {
                    m_start_ci = ci; m_end_ci = rp + 1; ptype = "link";
                    converted = parse_link (s, m_start_ci, m_end_ci);
                }
            }
        }

        /* 6. Strikeout: ~~…~~ */
        if (ptype.empty () && mdutf8::starts_with (s, ci, "~~")) {
            int close_ci = find_closing_marker (s, ci, "~~", "~~");
            if (close_ci > ci + 2) {
                m_start_ci = ci; m_end_ci = close_ci + 2; ptype = "strikeout";
                converted = parse_strikeout (s, m_start_ci, m_end_ci);
            }
        }

        if (!ptype.empty ()) {
            res.start_char = m_start_ci;
            res.end_char  = m_end_ci;
            res.pattern_type = ptype;
            res.converted = converted;
            res.valid = true;
            return res;
            /* first complete match wins */;
        }

        ci++;   /* advance one UTF-8 character */
    }

    return res;
}

#endif /* defined MARKDOWN_INLINE_PATTERNS_H */