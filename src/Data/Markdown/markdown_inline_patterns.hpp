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
#include "utf8_utils.hpp"

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

/* UTF-8 utilities */
namespace utf8 {
    inline int char_length (char c) {
        if ((c & 0x80) == 0) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 1;
    }
    inline int next_char (string s, int pos) {
        return pos + char_length (s[pos]);
    }
    inline string extract_char (string s, int pos) {
        int len = char_length (s[pos]);
        return s (pos, pos + len);
    }
}

/* Check if string starts with a specific marker (UTF-8 safe) */
static inline bool
starts_with_utf8 (string s, int char_pos, const char* prefix) {
    int byte_pos = 0;
    for (int i = 0; i < char_pos; i++) {
        if (byte_pos >= N (s)) return false;
        byte_pos = utf8::next_char (s, byte_pos);
    }
    
    int prefix_len = 0;
    while (prefix[prefix_len] != '\0') prefix_len++;
    
    if (byte_pos + prefix_len > N (s)) return false;
    for (int i = 0; i < prefix_len; i++) {
        if (s[byte_pos + i] != prefix[i]) return false;
    }
    return true;
}

/* Find closing marker, handling escapes (UTF-8 safe) */
static inline int
find_closing_marker_utf8 (string s, int start_char, const char* opener, const char* closer) {
    int opener_len = 0, closer_len = 0;
    while (opener[opener_len]) opener_len++;
    while (closer[closer_len]) closer_len++;
    
    int char_pos = start_char + opener_len;
    int escape_count = 0;
    
    while (true) {
        if (char_pos >= utf8::utf8_char_count (s)) return -1;
        
        int byte_pos = 0;
        for (int i = 0; i < char_pos; i++) {
            byte_pos = utf8::next_char (s, byte_pos);
        }
        
        /* Handle escape sequences */
        if (s[byte_pos] == '\\') {
            char_pos += closer_len;
            continue;
        }
        
        if (starts_with_utf8 (s, char_pos, closer) && escape_count == 0) {
            return char_pos;
        }
        
        char_pos++;
    }
}

/* Extract content between markers, stripping escapes (UTF-8 safe) */
static string
unescape_markers_utf8 (string s, int start_char, int end_char) {
    string result;
    int char_pos = start_char;
    while (char_pos < end_char) {
        int byte_pos = 0;
        for (int i = 0; i < char_pos; i++) {
            byte_pos = utf8::next_char (s, byte_pos);
        }
        
        if (s[byte_pos] == '\\' && char_pos + 1 < end_char) {
            /* Skip escape and take next character */
            int next_byte = utf8::next_char (s, byte_pos);
            result << s (byte_pos + 1, next_byte);
            char_pos += 2;
        } else {
            int next_byte = utf8::next_char (s, byte_pos);
            result << s (byte_pos, next_byte);
            char_pos++;
        }
    }
    return result;
}

/* Pattern handlers (unchanged from original, but now receive byte indices) */
static tree
parse_strong (string s, int start_byte, int end_byte) {
    string content = unescape_markers_utf8 (s, 
        utf8::utf8_char_count (s (0, start_byte)) + 2, 
        utf8::utf8_char_count (s (0, end_byte)) - 2);
    if (is_empty (content)) return tree ("");
    return compound ("strong", tree (content));
}

static tree
parse_emphasis (string s, int start_byte, int end_byte) {
    string content = unescape_markers_utf8 (s, 
        utf8::utf8_char_count (s (0, start_byte)) + 1, 
        utf8::utf8_char_count (s (0, end_byte)) - 1);
    if (is_empty (content)) return tree ("");
    return compound ("em", tree (content));
}

static tree
parse_inline_code (string s, int start_byte, int end_byte) {
    string content = s (start_byte + 1, end_byte - 1);
    return compound ("code", tree (content));
}

static tree
parse_link (string s, int start_byte, int end_byte) {
    /* Find the ]( separator */
    int bracket_pos_byte = -1;
    int search_pos = start_byte;
    while (search_pos < end_byte) {
        if (s[search_pos] == ']' && search_pos + 1 < end_byte && s[search_pos + 1] == '(') {
            bracket_pos_byte = search_pos;
            break;
        }
        search_pos++;
    }
    
    if (bracket_pos_byte == -1) return tree (s (start_byte, end_byte));
    
    /* Extract text content */
    int text_start = start_byte + 1;
    int text_end = bracket_pos_byte;
    
    /* Extract URL */
    int url_start = bracket_pos_byte + 2;
    int url_end = end_byte - 1;
    
    if (text_start >= text_end || url_start >= url_end) {
        return tree (s (start_byte, end_byte));
    }
    
    tree link_node = compound ("hlink", s (text_start, text_end), s (url_start, url_end));
    return link_node;
}

static tree
parse_strikeout (string s, int start_byte, int end_byte) {
    string content = unescape_markers_utf8 (s, 
        utf8::utf8_char_count (s (0, start_byte)) + 2, 
        utf8::utf8_char_count (s (0, end_byte)) - 2);
    if (is_empty (content)) return tree ("");
    return compound ("strikeout", tree (content));
}

static tree
parse_image (string s, int start_byte, int end_byte) {
    /* Format: ![alt](url) */
    if (start_byte + 2 >= end_byte) return tree (s (start_byte, end_byte));
    
    /* Find the ]( separator */
    int bracket_pos_byte = -1;
    int search_pos = start_byte + 2;
    while (search_pos < end_byte) {
        if (s[search_pos] == ']' && search_pos + 1 < end_byte && s[search_pos + 1] == '(') {
            bracket_pos_byte = search_pos;
            break;
        }
        search_pos++;
    }
    
    if (bracket_pos_byte == -1) return tree (s (start_byte, end_byte));
    
    /* Extract alt text */
    int alt_start = start_byte + 2;
    int alt_end = bracket_pos_byte;
    
    /* Extract URL */
    int url_start = bracket_pos_byte + 2;
    int url_end = end_byte - 1;
    
    if (url_start >= url_end) {
        return tree (s (start_byte, end_byte));
    }
    
    tree image_node = compound ("image", tree (""), s (url_start, url_end), s (alt_start, alt_end));
    return image_node;
}

/* Check if all Markdown markers are properly paired (UTF-8 safe) */
static inline bool
is_complete_markdown_input_utf8 (string s) {
    int n_char = utf8::utf8_char_count (s);
    
    /* --- Strong: **...** --- */
    {
        int depth = 0;
        int char_pos = 0;
        while (char_pos < n_char) {
            int byte_pos = 0;
            for (int i = 0; i < char_pos; i++) {
                byte_pos = utf8::next_char (s, byte_pos);
            }
            if (s[byte_pos] == '\\') { 
                char_pos += 2; 
                continue; 
            }
            if (char_pos + 1 < n_char && 
                starts_with_utf8 (s, char_pos, "**")) {
                depth = 1 - depth;
                char_pos += 2;
            } else {
                char_pos++;
            }
        }
        if (depth != 0) return false;
    }

    /* --- Emphasis: *...* (single) --- */
    {
        int depth = 0;
        int char_pos = 0;
        while (char_pos < n_char) {
            int byte_pos = 0;
            for (int i = 0; i < char_pos; i++) {
                byte_pos = utf8::next_char (s, byte_pos);
            }
            if (s[byte_pos] == '\\') { 
                char_pos += 2; 
                continue; 
            }
            if (char_pos + 1 < n_char && 
                starts_with_utf8 (s, char_pos, "**")) { 
                char_pos += 2; 
                continue; 
            }
            if (starts_with_utf8 (s, char_pos, "*")) {
                depth = 1 - depth;
                char_pos++;
            } else {
                char_pos++;
            }
        }
        if (depth != 0) return false;
    }

    /* --- Strikeout: ~~...~~ --- */
    {
        int depth = 0;
        int char_pos = 0;
        while (char_pos < n_char) {
            int byte_pos = 0;
            for (int i = 0; i < char_pos; i++) {
                byte_pos = utf8::next_char (s, byte_pos);
            }
            if (s[byte_pos] == '\\') { 
                char_pos += 2; 
                continue; 
            }
            if (char_pos + 1 < n_char && 
                starts_with_utf8 (s, char_pos, "~~")) {
                depth = 1 - depth;
                char_pos += 2;
            } else {
                char_pos++;
            }
        }
        if (depth != 0) return false;
    }

    /* --- Inline code: backtick-based --- */
    {
        int depth = 0;
        int char_pos = 0;
        while (char_pos < n_char) {
            int byte_pos = 0;
            for (int i = 0; i < char_pos; i++) {
                byte_pos = utf8::next_char (s, byte_pos);
            }
            if (s[byte_pos] == '\\') { 
                char_pos += 2; 
                continue; 
            }
            if (char_pos + 1 < n_char && 
                s[byte_pos] == '`' && 
                !(char_pos + 1 < n_char && s[byte_pos + 1] == '`')) {
                depth = 1 - depth;
                char_pos++;
            } else if (char_pos + 1 < n_char && 
                     s[byte_pos] == '`' && s[byte_pos + 1] == '`') {
                /* double backtick → skip both, no toggle */
                char_pos += 2;
            } else {
                char_pos++;
            }
        }
        if (depth != 0) return false;
    }

    /* --- Link: [...] and image: ![alt]... must have matching [ and ] --- */
    {
        int depth = 0;
        int char_pos = 0;
        while (char_pos < n_char) {
            int byte_pos = 0;
            for (int i = 0; i < char_pos; i++) {
                byte_pos = utf8::next_char (s, byte_pos);
            }
            if (s[byte_pos] == '\\') { 
                char_pos += 2; 
                continue; 
            }
            if (char_pos + 1 < n_char && 
                s[byte_pos] == '!' && s[byte_pos + 1] == '[') {
                depth++;
                char_pos += 2;
            } else if (starts_with_utf8 (s, char_pos, "[")) {
                depth++;
                char_pos++;
            } else if (starts_with_utf8 (s, char_pos, "]")) {
                depth--;
                if (depth < 0) return false;
                char_pos++;
                
                /* Check for following (…) */
                if (char_pos < n_char) {
                    int byte_pos2 = 0;
                    for (int i = 0; i < char_pos; i++) {
                        byte_pos2 = utf8::next_char (s, byte_pos2);
                    }
                    if (s[byte_pos2] == '(') {
                        /* find closing ) */
                        int j = char_pos + 1;
                        int paren_depth = 1;
                        while (j < n_char && paren_depth > 0) {
                            int byte_pos3 = 0;
                            for (int k = 0; k < j; k++) {
                                byte_pos3 = utf8::next_char (s, byte_pos3);
                            }
                            if (s[byte_pos3] == '\\') { 
                                j += 2; 
                                continue; 
                            }
                            if (s[byte_pos3] == '(') paren_depth++;
                            if (s[byte_pos3] == ')') paren_depth--;
                            if (paren_depth > 0) j++;
                        }
                        if (paren_depth != 0) return false;
                        char_pos = j;
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            } else {
                char_pos++;
            }
        }
        if (depth != 0) return false;
    }

    return true;
}

/* Main parser: find first complete inline markdown pattern in UTF-8 string */
static md_local_match
try_parse_inline_markdown_utf8 (string s) {
    md_local_match result;
    result.valid = false;
    
    if (is_empty (s)) {
        return result;
    }
    
    int n_char = utf8::utf8_char_count (s);
    int char_pos = 0;
    
    while (char_pos < n_char) {
        int match_start_char = -1;
        int match_end_char = -1;
        string pattern_type;
        
        /* Get byte position for current char */
        int byte_pos = 0;
        for (int i = 0; i < char_pos; i++) {
            byte_pos = utf8::next_char (s, byte_pos);
        }
        
        /* Try each pattern at current position */
        
        /* 1. Strong: **...** */
        if (char_pos + 1 < n_char && 
            starts_with_utf8 (s, char_pos, "**")) {
            int end_char = find_closing_marker_utf8 (s, char_pos, "**", "**");
            if (end_char != -1 && end_char > char_pos + 2) {
                match_start_char = char_pos;
                match_end_char = end_char + 2;
                pattern_type = "strong";
            }
        }
        
        /* 2. Emphasis: *...* (but not inside words, and not **) */
        if (pattern_type.empty() && s[byte_pos] == '*' && 
            !(char_pos + 1 < n_char && starts_with_utf8 (s, char_pos, "**"))) {
            /* Check word boundary */
            bool valid_start = (char_pos == 0 || 
                (!is_alpha (s[byte_pos - 1]) && s[byte_pos - 1] != '_'));
            if (valid_start) {
                int end_char = find_closing_marker_utf8 (s, char_pos, "*", "*");
                if (end_char != -1 && end_char > char_pos + 1) {
                    match_start_char = char_pos;
                    match_end_char = end_char + 1;
                    pattern_type = "emphasis";
                }
            }
        }
        
        /* 3. Inline code: backtick-delimited */
        if (pattern_type.empty() && s[byte_pos] == '`') {
            int end_char = find_closing_marker_utf8 (s, char_pos, "`", "`");
            if (end_char != -1 && end_char > char_pos + 1) {
                match_start_char = char_pos;
                match_end_char = end_char + 1;
                pattern_type = "code";
            }
        }
        
        /* 4. Image: ![alt](url) */
        if (pattern_type.empty() && 
            char_pos + 1 < n_char && 
            starts_with_utf8 (s, char_pos, "![[")) {
            /* Actually: ![, need to find ] then ( */
            int bracket_char = -1;
            int search_pos = char_pos + 2;
            while (search_pos < n_char) {
                int byte_pos2 = 0;
                for (int i = 0; i < search_pos; i++) {
                    byte_pos2 = utf8::next_char (s, byte_pos2);
                }
                if (s[byte_pos2] == ']' && search_pos + 1 < n_char && 
                    s[byte_pos2 + 1] == '(') {
                    bracket_char = search_pos;
                    break;
                }
                search_pos++;
            }
            if (bracket_char != -1) {
                int close_paren = -1;
                int search_pos2 = bracket_char + 2;
                while (search_pos2 < n_char) {
                    int byte_pos3 = 0;
                    for (int i = 0; i < search_pos2; i++) {
                        byte_pos3 = utf8::next_char (s, byte_pos3);
                    }
                    if (s[byte_pos3] == ')') {
                        close_paren = search_pos2;
                        break;
                    }
                    search_pos2++;
                }
                if (close_paren != -1) {
                    match_start_char = char_pos;
                    match_end_char = close_paren + 1;
                    pattern_type = "image";
                }
            }
        }
        
        /* 5. Link: [text](url) - only if not preceded by ! */
        if (pattern_type.empty() && s[byte_pos] == '[' && 
            !(char_pos > 0 && 
              /* Check if preceded by ! */ 
              [&]{
                  int byte_pos2 = 0;
                  for (int i = 0; i < char_pos - 1; i++) {
                      byte_pos2 = utf8::next_char (s, byte_pos2);
                  }
                  return char_pos > 0 && s[byte_pos2] == '!';
              }())) {
            int bracket_char = -1;
            int search_pos = char_pos + 1;
            while (search_pos < n_char) {
                int byte_pos2 = 0;
                for (int i = 0; i < search_pos; i++) {
                    byte_pos2 = utf8::next_char (s, byte_pos2);
                }
                if (s[byte_pos2] == ']' && search_pos + 1 < n_char && 
                    s[byte_pos2 + 1] == '(') {
                    bracket_char = search_pos;
                    break;
                }
                search_pos++;
            }
            if (bracket_char != -1) {
                int close_paren = -1;
                int search_pos2 = bracket_char + 2;
                while (search_pos2 < n_char) {
                    int byte_pos3 = 0;
                    for (int i = 0; i < search_pos2; i++) {
                        byte_pos3 = utf8::next_char (s, byte_pos3);
                    }
                    if (s[byte_pos3] == ')') {
                        close_paren = search_pos2;
                        break;
                    }
                    search_pos2++;
                }
                if (close_paren != -1) {
                    match_start_char = char_pos;
                    match_end_char = close_paren + 1;
                    pattern_type = "link";
                }
            }
        }
        
        /* 6. Strikeout: ~~...~~ */
        if (pattern_type.empty() && 
            char_pos + 1 < n_char && 
            starts_with_utf8 (s, char_pos, "~~")) {
            int end_char = find_closing_marker_utf8 (s, char_pos, "~~", "~~");
            if (end_char != -1 && end_char > char_pos + 2) {
                match_start_char = char_pos;
                match_end_char = end_char + 2;
                pattern_type = "strikeout";
            }
        }
        
        if (!pattern_type.empty() && match_start_char != -1 && match_end_char != -1) {
            /* Convert byte positions */
            int start_byte = 0;
            int end_byte = 0;
            for (int i = 0; i < match_start_char; i++) {
                start_byte = utf8::next_char (s, start_byte);
            }
            for (int i = 0; i < match_end_char; i++) {
                end_byte = utf8::next_char (s, end_byte);
            }
            
            tree converted;
            if (pattern_type == "strong")
                converted = parse_strong (s, start_byte, end_byte);
            else if (pattern_type == "emphasis")
                converted = parse_emphasis (s, start_byte, end_byte);
            else if (pattern_type == "code")
                converted = parse_inline_code (s, start_byte, end_byte);
            else if (pattern_type == "link")
                converted = parse_link (s, start_byte, end_byte);
            else if (pattern_type == "image")
                converted = parse_image (s, start_byte, end_byte);
            else if (pattern_type == "strikeout")
                converted = parse_strikeout (s, start_byte, end_byte);
            
            if (!is_atomic (converted) || !is_empty (copy (as_string (L (converted))))) {
                result.start_char = match_start_char;
                result.end_char = match_end_char;
                result.pattern_type = pattern_type;
                result.converted = converted;
                result.valid = true;
                return result;
            }
            
            /* Skip past this match */
            char_pos = match_end_char;
        } else {
            char_pos++;
        }
    }
    
    return result;
}

#endif /* defined MARKDOWN_INLINE_PATTERNS_H */