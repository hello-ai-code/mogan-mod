/******************************************************************************
 * MODULE     : markdown_export.cpp
 * DESCRIPTION: TeXmacs tree → Markdown converter
 * COPYRIGHT  : (C) 2026  Mogan contributors
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#include "convert.hpp"
#include "tree.hpp"
#include "tree_helper.hpp"
#include "string.hpp"

using namespace moebius;

/******************************************************************************
 * Context for markdown export
 ******************************************************************************/

struct md_export_context {
    string result;
};

/******************************************************************************
 * Helpers
 ******************************************************************************/

/* Check if a compound label matches any of the given strings */
static inline bool
is_label (tree t, const char* label1) {
    return is_compound (t, label1);
}

static inline bool
is_label (tree t, const char* label1, const char* label2) {
    return (is_compound (t, label1) || is_compound (t, label2));
}

static inline bool
is_heading_tag (string s) {
    return (s == "section" || s == "subsection" ||
            s == "subsubsection" || s == "paragraph" ||
            s == "subparagraph" || s == "subsubparagraph");
}

/* Escape special Markdown characters in plain text */
static string
md_escape_text (string s) {
    string result;
    int n = N (s);
    for (int i = 0; i < n; i++) {
        char c = s[i];
        switch (c) {
            case '\\': result << "\\\\"; break;
            case '#':  result << "\\#"; break;
            case '*':  result << "\\*"; break;
            case '_':  result << "\\_"; break;
            case '[':  result << "\\["; break;
            case ']':  result << "\\]"; break;
            case '(':  result << "\\("; break;
            case ')':  result << "\\)"; break;
            case '<':  result << "\\<"; break;
            case '>':  result << "\\>"; break;
            case '`':  result << "\\`"; break;
            case '|':  result << "\\|"; break;
            default:   result << c; break;
        }
    }
    return result;
}

/******************************************************************************
 * Main export function
 ******************************************************************************/

static void
export_tree_to_markdown (tree t, md_export_context& ctx, int indent_level);

/* Export a single tree node to Markdown */
static void
export_single_node (tree t, md_export_context& ctx) {
    if (is_atomic (t)) {
        /* Escape special Markdown characters */
        string s = t->label;
        int n = N (s);
        for (int i = 0; i < n; i++) {
            char c = s[i];
            switch (c) {
                case '\\': ctx.result << "\\\\"; break;
                case '#':  ctx.result << "\\#"; break;
                case '*':  ctx.result << "\\*"; break;
                case '_':  ctx.result << "\\_"; break;
                case '[':  ctx.result << "\\["; break;
                case ']':  ctx.result << "\\]"; break;
                case '(':  ctx.result << "\\("; break;
                case ')':  ctx.result << "\\)"; break;
                case '<':  ctx.result << "\\<"; break;
                case '>':  ctx.result << "\\>"; break;
                case '`':  ctx.result << "\\`"; break;
                case '|':  ctx.result << "\\|"; break;
                default:   ctx.result << c; break;
            }
        }
        return;
    }

    string label = as_string (L (t));

    /* Handle CONCAT: just export children sequentially */
    if (label == "CONCAT") {
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
        return;
    }

    /* Inline formatting */
    if (is_label (t, "em")) {
        if (N (t) > 0) {
            ctx.result << "*";
            export_single_node (t[0], ctx);
            ctx.result << "*";
        }
    }
    else if (is_label (t, "strong")) {
        ctx.result << "**";
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
        ctx.result << "**";
    }
    else if (is_label (t, "code", "verbatim")) {
        ctx.result << "`";
        for (int i = 1; i < N (t); i++)
            export_single_node (t[i], ctx);
        ctx.result << "`";
    }
    else if (is_label (t, "strikeout")) {
        ctx.result << "~~";
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
        ctx.result << "~~";
    }
    else if (is_label (t, "underline")) {
        ctx.result << "<u>";
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
        ctx.result << "</u>";
    }
    else if (is_label (t, "mark")) {
        ctx.result << "==";
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
        ctx.result << "==";
    }
    else if (is_label (t, "rsup")) {
        ctx.result << "<sup>";
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
        ctx.result << "</sup>";
    }
    else if (is_label (t, "rsub")) {
        ctx.result << "<sub>";
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
        ctx.result << "</sub>";
    }
    else if (is_label (t, "hlink")) {
        if (N (t) >= 2) {
            tree content = t[0];
            string href = as_string (t[1]);
            ctx.result << "[";
            export_single_node (content, ctx);
            ctx.result << "](" << href << ")";
        }
    }
    else if (is_label (t, "image")) {
        if (N (t) >= 3) {
            string src = as_string (t[1]);
            tree alt = t[2];
            ctx.result << "![";
            export_single_node (alt, ctx);
            ctx.result << "](" << src << ")";
        }
    }
    else if (is_label (t, "next-line")) {
        ctx.result << "\n\n";
    }
    else if (is_label (t, "line-break")) {
        ctx.result << "  \n";
    }
    else {
        /* Unknown compound: export children raw */
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
    }
}

/* Export tree with proper block handling */
static void
export_tree_to_markdown (tree t, md_export_context& ctx, int indent_level) {
    if (is_atomic (t)) {
        ctx.result << md_escape_text (t->label);
        return;
    }

    string label = as_string (L (t));

    /* Document: flatten and export all paragraphs */
    if (label == "DOCUMENT") {
        for (int i = 0; i < N (t); i++)
            export_tree_to_markdown (t[i], ctx, indent_level);
        return;
    }

    /* Body/TeXmacs wrapper: unwrap */
    if (label == "body" || label == "TeXmacs" || 
        label == "initial" || label == "style") {
        for (int i = 0; i < N (t); i++)
            export_tree_to_markdown (t[i], ctx, indent_level);
        return;
    }

    /* Concat: export children on same logical line */
    if (label == "CONCAT") {
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);
        return;
    }

    /* Headings */
    if (is_heading_tag (label)) {
        int level = 0;
        if (label == "section") level = 1;
        else if (label == "subsection") level = 2;
        else if (label == "subsubsection") level = 3;
        else if (label == "paragraph") level = 4;
        else if (label == "subparagraph") level = 5;
        else level = 6;

        /* Add blank line before heading if not at start */
        int res_len = N (ctx.result);
        if (res_len > 0 && ctx.result[res_len - 1] != '\n')
            ctx.result << "\n\n";

        for (int i = 0; i < level; i++)
            ctx.result << "#";
        ctx.result << " ";

        /* Export heading content */
        for (int i = 0; i < N (t); i++)
            export_single_node (t[i], ctx);

        ctx.result << "\n\n";
        return;
    }

    /* Itemize list */
    if (is_label (t, "itemize")) {
        for (int i = 0; i < N (t); i++) {
            tree item = t[i];
            if (is_label (item, "item")) {
                for (int j = 1; j < N (item); j++)
                    export_tree_to_markdown (item[j], ctx, indent_level);
            }
            else {
                export_tree_to_markdown (item, ctx, indent_level);
            }
            ctx.result << "\n";
        }
        return;
    }

    /* Enumerate list */
    if (is_label (t, "enumerate")) {
        int num = 1;
        for (int i = 0; i < N (t); i++) {
            tree item = t[i];
            if (is_label (item, "item")) {
                ctx.result << as_string (num) << ". ";
                for (int j = 1; j < N (item); j++)
                    export_tree_to_markdown (item[j], ctx, indent_level);
            }
            else {
                ctx.result << as_string (num) << ". ";
                export_tree_to_markdown (item, ctx, indent_level);
            }
            num++;
            ctx.result << "\n";
        }
        return;
    }

    /* Quote block */
    if (is_label (t, "quote")) {
        for (int i = 0; i < N (t); i++)
            export_tree_to_markdown (t[i], ctx, indent_level + 1);
        return;
    }

    /* Paragraph-like structures (flatten content) */
    if (label == "PARA" || label == "quote*") {
        int start = 0;
        while (start < N (t) && t[start] == "")
            start++;

        if (indent_level > 0) {
            for (int k = 0; k < indent_level; k++)
                ctx.result << "    ";
        }

        /* Export paragraph content */
        int first = 1;
        for (int i = start; i < N (t); i++) {
            if (!first && is_atomic (t[i]))
                ctx.result << " ";
            first = 0;
            export_single_node (t[i], ctx);
        }
        
        ctx.result << "\n\n";
        return;
    }

    /* Verbatim/code block */
    if (is_label (t, "verbatim")) {
        string code_text;
        for (int i = 1; i < N (t); i++) {
            if (is_atomic (t[i]))
                code_text << t[i]->label;
        }

        ctx.result << "```\n";
        ctx.result << code_text;
        if (N (code_text) > 0 && code_text[N(code_text)-1] != '\n')
            ctx.result << "\n";
        ctx.result << "```\n\n";
        return;
    }

    /* Horizontal rule */
    if (is_label (t, "hrule")) {
        int res_len = N (ctx.result);
        if (res_len > 0 && ctx.result[res_len - 1] != '\n')
            ctx.result << "\n";
        ctx.result << "---\n\n";
        return;
    }

    /* Default: try to extract text content */
    for (int i = 0; i < N (t); i++)
        export_tree_to_markdown (t[i], ctx, indent_level);
}

/******************************************************************************
 * Public API
 ******************************************************************************/

string
tree_to_markdown (tree t) {
    md_export_context ctx;
    export_tree_to_markdown (t, ctx, 0);
    
    /* Clean up trailing whitespace */
    int len = N (ctx.result);
    while (len > 0 && (ctx.result[len-1] == '\n' || ctx.result[len-1] == ' '))
        len--;
    
    string out;
    for (int i = 0; i < len; i++)
        out << ctx.result[i];
    
    return out;
}
