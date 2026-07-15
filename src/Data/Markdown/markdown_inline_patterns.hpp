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

using namespace moebius;

/******************************************************************************
 * Pattern matching helpers (no external regex dependency)
 ******************************************************************************/

/* Simple state machine for inline markdown parsing */
struct md_parse_result {
    tree result;
    bool complete;  /* false if input is incomplete (e.g., "**bold" without closing **) */
};

/* Check if string starts with a specific marker */
static inline bool
starts_with (string s, int pos, const char* prefix) {
    int i = 0;
    while (prefix[i] != '\0') {
        if (pos + i >= N (s)) return false;
        if (s[pos + i] != prefix[i]) return false;
        i++;
    }
    return true;
}

/* Find closing marker, handling escapes */
static inline int
find_closing_marker (string s, int start, const char* opener, const char* closer) {
    int open_len = 0, close_len = 0;
    while (opener[open_len]) open_len++;
    while (closer[close_len]) close_len++;
    
    int pos = start + open_len;
    int escape_count = 0;
    
    while (pos <= N (s) - close_len) {
        /* Handle escape sequences */
        if (s[pos] == '\\') {
            pos += close_len;
            continue;
        }
        
        if (starts_with (s, pos, closer) && escape_count == 0) {
            return pos;
        }
        
        pos++;
    }
    
    return -1;  /* Not found */
}

/* Extract content between markers, stripping escapes */
static string
unescape_markers (string s) {
    string result;
    int n = N (s);
    int i = 0;
    while (i < n) {
        if (s[i] == '\\' && i + 1 < n) {
            result << s[i + 1];
            i += 2;
        } else {
            result << s[i];
            i++;
        }
    }
    return result;
}

/******************************************************************************
 * Pattern handlers
 ******************************************************************************/

/* Parse **text** → strong node */
static tree
parse_strong (string s, int start, int end) {
    string content = unescape_markers (substring (s, start + 2, end - 2));
    if (is_empty (content)) return tree ("");
    return tree ("strong", 1) << content;
}

/* Parse *text* → em node */
static tree
parse_emphasis (string s, int start, int end) {
    string content = unescape_markers (substring (s, start + 1, end - 1));
    if (is_empty (content)) return tree ("");
    return tree ("em", 1) << content;
}

/* Parse `code` → code/verbatim node */
static tree
parse_inline_code (string s, int start, int end) {
    string content = substring (s, start + 1, end - 1);
    /* No unescaping for code blocks - literal content */
    return tree ("code", 1) << content;
}

/* Parse [text](url) → hlink node */
static tree
parse_link (string s, int start, int end) {
    /* Find the ]( separator */
    int bracket_pos = -1;
    for (int i = start; i < end; i++) {
        if (s[i] == ']' && i + 1 < end && s[i + 1] == '(') {
            bracket_pos = i;
            break;
        }
    }
    
    if (bracket_pos == -1) return tree (substring (s, start, end));
    
    /* Extract text content */
    string text = substring (s, start + 1, bracket_pos);
    
    /* Extract URL (everything between ( and )) */
    int url_start = bracket_pos + 2;
    int url_end = end - 1;
    string url = substring (s, url_start, url_end);
    
    if (is_empty (text) || is_empty (url)) {
        return tree (substring (s, start, end));
    }
    
    tree link_node ("hlink", 2);
    link_node[0] = tree (text);
    link_node[1] = tree (url);
    return link_node;
}

/* Parse ~~text~~ → strikeout node */
static tree
parse_strikeout (string s, int start, int end) {
    string content = unescape_markers (substring (s, start + 2, end - 2));
    if (is_empty (content)) return tree ("");
    return tree ("strikeout", 1) << content;
}

/* Parse ![](url) → image node */
static tree
parse_image (string s, int start, int end) {
    /* Format: ![alt](url) */
    if (start + 2 >= end) return tree (substring (s, start, end));
    
    /* Find the ]( separator */
    int bracket_pos = -1;
    for (int i = start + 2; i < end; i++) {
        if (s[i] == ']' && i + 1 < end && s[i + 1] == '(') {
            bracket_pos = i;
            break;
        }
    }
    
    if (bracket_pos == -1) return tree (substring (s, start, end));
    
    /* Extract alt text */
    string alt = substring (s, start + 2, bracket_pos);
    
    /* Extract URL */
    int url_start = bracket_pos + 2;
    int url_end = end - 1;
    string src = substring (s, url_start, url_end);
    
    if (is_empty (src)) {
        return tree (substring (s, start, end));
    }
    
    tree image_node ("image", 3);
    image_node[0] = tree ("");  /* Empty label */
    image_node[1] = tree (src);
    image_node[2] = tree (alt);
    return image_node;
}

/******************************************************************************
 * Main parser: incrementally parse markdown from partial input
 ******************************************************************************/

/*
 * Parse inline markdown patterns from potentially incomplete input.
 * Returns parsed tree structure, or original text if no patterns detected.
 * 
 * Key design decision: If input is incomplete (e.g., "**bold" without closing),
 * treat it as plain text to avoid jarring UX changes mid-typing.
 */
static md_parse_result
try_parse_inline_markdown (string s) {
    md_parse_result result;
    result.result = tree (CONCAT);
    result.complete = true;
    
    if (is_empty (s)) {
        return result;
    }
    
    int pos = 0;
    int n = N (s);
    
    while (pos < n) {
        int match_start = -1;
        int match_end = -1;
        string pattern_type;
        
        /* Try each pattern at current position */
        
        /* 1. Strong: **...** */
        if (starts_with (s, pos, "**")) {
            int end = find_closing_marker (s, pos, "**", "**");
            if (end != -1 && end > pos + 2) {
                match_start = pos;
                match_end = end + 2;
                pattern_type = "strong";
            }
        }
        
        /* 2. Emphasis: *...* (but not inside words, and not **) */
        if (pattern_type.empty () && s[pos] == '*' && !starts_with (s, pos, "**")) {
            /* Check it's not part of a word boundary issue */
            bool valid_start = (pos == 0 || (!is_alnum (s[pos - 1])));
            if (valid_start) {
                int end = find_closing_marker (s, pos, "*", "*");
                if (end != -1 && end > pos + 1) {
                    match_start = pos;
                    match_end = end + 1;
                    pattern_type = "emphasis";
                }
            }
        }
        
        /* 3. Inline code: `...` */
        if (pattern_type.empty () && s[pos] == '`') {
            int end = find_closing_marker (s, pos, "`", "`");
            if (end != -1 && end > pos + 1) {
                match_start = pos;
                match_end = end + 1;
                pattern_type = "code";
            }
        }
        
        /* 4. Image: ![alt](url) */
        if (pattern_type.empty () && starts_with (s, pos, "![")) {
            int paren_pos = -1;
            for (int i = pos + 2; i < n; i++) {
                if (s[i] == ']' && i + 1 < n && s[i + 1] == '(') {
                    paren_pos = i;
                    break;
                }
            }
            if (paren_pos != -1) {
                int close_paren = -1;
                for (int i = paren_pos + 2; i < n; i++) {
                    if (s[i] == ')') {
                        close_paren = i;
                        break;
                    }
                }
                if (close_paren != -1) {
                    match_start = pos;
                    match_end = close_paren + 1;
                    pattern_type = "image";
                }
            }
        }
        
        /* 5. Link: [text](url) - only if not preceded by ! */
        if (pattern_type.empty () && s[pos] == '[' && !(pos > 0 && s[pos - 1] == '!')) {
            int paren_pos = -1;
            for (int i = pos + 1; i < n; i++) {
                if (s[i] == ']' && i + 1 < n && s[i + 1] == '(') {
                    paren_pos = i;
                    break;
                }
            }
            if (paren_pos != -1) {
                int close_paren = -1;
                for (int i = paren_pos + 2; i < n; i++) {
                    if (s[i] == ')') {
                        close_paren = i;
                        break;
                    }
                }
                if (close_paren != -1) {
                    match_start = pos;
                    match_end = close_paren + 1;
                    pattern_type = "link";
                }
            }
        }
        
        /* 6. Strikeout: ~~...~~ */
        if (pattern_type.empty () && starts_with (s, pos, "~~")) {
            int end = find_closing_marker (s, pos, "~~", "~~");
            if (end != -1 && end > pos + 2) {
                match_start = pos;
                match_end = end + 2;
                pattern_type = "strikeout";
            }
        }
        
        if (!pattern_type.empty () && match_start != -1 && match_end != -1) {
            /* Complete match found - convert to tree */
            tree converted;
            
            if (pattern_type == "strong")
                converted = parse_strong (s, match_start, match_end);
            else if (pattern_type == "emphasis")
                converted = parse_emphasis (s, match_start, match_end);
            else if (pattern_type == "code")
                converted = parse_inline_code (s, match_start, match_end);
            else if (pattern_type == "link")
                converted = parse_link (s, match_start, match_end);
            else if (pattern_type == "image")
                converted = parse_image (s, match_start, match_end);
            else if (pattern_type == "strikeout")
                converted = parse_strikeout (s, match_start, match_end);
            
            if (!is_atomic (converted) || !is_empty (copy (as_string (L (converted))))) {
                result.result << converted;
            }
            
            pos = match_end;
        } else {
            /* No pattern match at this position - copy character as-is */
            result.result << tree (s[pos]);
            pos++;
        }
    }
    
    return result;
}

/* Simplified version that only checks if input is complete markdown */
static inline bool
is_complete_markdown_input (string s) {
    /* Quick heuristic: check for unclosed markers */
    int asterisk_count = 0;
    int backtick_count = 0;
    int tilde_count = 0;
    
    for (int i = 0; i < N (s); i++) {
        if (s[i] == '*') asterisk_count++;
        if (s[i] == '`') backtick_count++;
        if (s[i] == '~') tilde_count++;
    }
    
    /* Single * or ~ usually indicates incomplete input */
    if (asterisk_count == 1 || tilde_count == 1) return false;
    
    /* Backticks must be even */
    if (backtick_count % 2 != 0) return false;
    
    return true;
}

#endif /* defined MARKDOWN_INLINE_PATTERNS_H */
