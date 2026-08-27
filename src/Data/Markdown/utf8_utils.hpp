#ifndef UTF8_UTILS_H
#define UTF8_UTILS_H

#include "string.hpp"

namespace moebius {

/* UTF-8 utilities for safe string handling */

/* Returns the length (in characters) of a UTF-8 sequence starting at position pos */
static inline int utf8_char_length (char c) {
    if ((c & 0x80) == 0) return 1;           /* 0xxxxxxx */
    if ((c & 0xE0) == 0xC0) return 2;       /* 110xxxxx */
    if ((c & 0xF0) == 0xE0) return 3;       /* 1110xxxx */
    if ((c & 0xF8) == 0xF0) return 4;       /* 11110xxx */
    return 1;                                /* Fallback to 1 byte */
}

/* Extract a single UTF-8 character from position pos (byte index) */
static inline string extract_utf8_char (string s, int pos) {
    int char_len = utf8_char_length (s[pos]);
    return s (pos, pos + char_len);
}

/* Find the position (byte index) of the next UTF-8 character after pos */
static inline int next_utf8_char (string s, int pos) {
    int char_len = utf8_char_length (s[pos]);
    return pos + char_len;
}

/* Check if a string (UTF-8) contains a specific pattern starting at position pos (character index) */
static inline bool starts_with_utf8 (string s, int char_pos, const char* pattern) {
    int pattern_len = 0;
    while (pattern[pattern_len] != '\0') {
        pattern_len++;
    }

    int byte_pos = 0;
    for (int i = 0; i < char_pos; i++) {
        if (byte_pos >= N (s)) return false;
        byte_pos = next_utf8_char (s, byte_pos);
    }

    if (byte_pos + pattern_len > N (s)) return false;
    for (int i = 0; i < pattern_len; i++) {
        if (s[byte_pos + i] != pattern[i]) return false;
    }
    return true;
}

/* Count the number of UTF-8 characters in a string */
static inline int utf8_char_count (string s) {
    int count = 0;
    int pos = 0;
    while (pos < N (s)) {
        count++;
        pos = next_utf8_char (s, pos);
    }
    return count;
}

/* Extract substring based on UTF-8 character positions */
static inline string utf8_substring (string s, int start_char, int end_char) {
    int start_byte = 0;
    int end_byte = 0;
    for (int i = 0; i < start_char; i++) {
        start_byte = next_utf8_char (s, start_byte);
    }
    for (int i = 0; i < end_char; i++) {
        end_byte = next_utf8_char (s, end_byte);
    }
    return s (start_byte, end_byte);
}

/* Append UTF-8 character to string */
static inline void append_utf8_char (string& s, const char* c) {
    int len = 0;
    while (c[len] != '\0') len++;
    for (int i = 0; i < len; i++) {
        s << c[i];
    }
}

} // namespace moebius

#endif /* defined(UTF8_UTILS_H) */