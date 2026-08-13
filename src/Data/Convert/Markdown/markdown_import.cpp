/******************************************************************************
 * MODULE     : markdown_import.cpp
 * DESCRIPTION: Markdown → TeXmacs tree converter using md4c
 * COPYRIGHT  : (C) 2026  Mogan contributors
 *******************************************************************************
 * This software falls under the GNU general public license version 3 or later.
 * It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
 * in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
 ******************************************************************************/

#include "convert.hpp"
#include "converter.hpp"
#include "tree.hpp"
#include "tree_helper.hpp"
extern "C" {
#include "md4c.h"
}
#include <moebius/tree_label.hpp>
#include <moebius/vars.hpp>

#include <stack>

using namespace moebius;

/******************************************************************************
 * Context for markdown parsing
 ******************************************************************************/

struct md_context {
    tree              doc;         /* final DOCUMENT tree */
    std::stack<tree>  node_stack;  /* stack of open blocks/spans */

    md_context () : doc (DOCUMENT) {}
};

/******************************************************************************
 * Helpers
 ******************************************************************************/

static inline string
md_str (const MD_CHAR* text, MD_SIZE size) {
    /* md4c hands us raw UTF-8 bytes; Mogan tree labels are cork-encoded
     * (like verbatim's encode(..., "utf-8")), so convert here once for
     * all text, URLs, alt texts, language tags, etc. */
    string s (text, (int) size);
    return utf8_to_cork (s);
}

/* Append a child to the top of the stack */
static inline void
md_append (md_context& ctx, tree child) {
    if (ctx.node_stack.empty ()) return;
    ctx.node_stack.top () << child;
}

/* Append raw text to the top of the stack, merging with previous atomic */
static void
md_append_text (md_context& ctx, const string& s) {
    if (s == "" || ctx.node_stack.empty ()) return;
    tree& top = ctx.node_stack.top ();
    int   n   = N (top);
    if (n > 0 && is_atomic (top[n - 1]))
        top[n - 1]->label = top[n - 1]->label * s;
    else
        top << tree (s);
}

/******************************************************************************
 * Block callbacks
 ******************************************************************************/

static int
md_enter_block (MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto& ctx = *static_cast<md_context*> (userdata);

    switch (type) {
    case MD_BLOCK_DOC:
        ctx.node_stack.push (tree (DOCUMENT));
        break;

    case MD_BLOCK_P:
        ctx.node_stack.push (tree (CONCAT));
        break;

    case MD_BLOCK_H: {
        unsigned level = static_cast<MD_BLOCK_H_DETAIL*> (detail)->level;
        /* Level mapping follows tmu3-v2.lua section_tags:
         *   #   → chapter*, ## → section*, ### → subsection*,
         *   #### → subsubsection*, ##### → paragraph*, ###### → subparagraph*
         * The trailing '*' means "unnumbered" (plain Markdown titles). */
        string tag;
        switch (level) {
        case 1: tag= "chapter*";       break;
        case 2: tag= "section*";       break;
        case 3: tag= "subsection*";    break;
        case 4: tag= "subsubsection*"; break;
        case 5: tag= "paragraph*";     break;
        default: tag= "subparagraph*"; break;
        }
        /* Push heading container, then a separate CONCAT on top of the
         * stack to collect the heading's inline text.  md_text appends to
         * node_stack.top(), so text would otherwise land directly on the
         * (tag concat) node instead of inside its CONCAT, losing the
         * heading text.  Same pattern as inline spans below. */
        ctx.node_stack.push (compound (tag, tree (CONCAT)));
        ctx.node_stack.push (tree (CONCAT));
        break;
    }

    case MD_BLOCK_UL:
        /* Standard Mogan list tree is (itemize (document (item ...)...)):
         * the items must be wrapped in a DOCUMENT so that to_tmu's
         * tmu_writer::apply() detects the document argument and emits the
         * block form <\itemize>...</itemize> instead of the inline
         * <itemize|<item|...>> form.  Push the container, then a DOCUMENT
         * on top of the stack to collect the items. */
        ctx.node_stack.push (compound ("itemize"));
        ctx.node_stack.push (tree (DOCUMENT));
        break;

    case MD_BLOCK_OL:
        ctx.node_stack.push (compound ("enumerate"));
        ctx.node_stack.push (tree (DOCUMENT));
        break;

    case MD_BLOCK_LI: {
        /* Standard Mogan list item shape (see TeXmacs/tests/221_10.scm):
         *     (itemize (document (concat (item) "content") ...))
         * (item) is a ZERO-ARGUMENT marker: it sits as the FIRST element
         * of a CONCAT and the item content follows as siblings.  The
         * renderer only draws the bullet for (item), so wrapping content
         * INSIDE an (item ...) container loses the text.  Push a CONCAT
         * and seed it with the (item) marker. */
        tree li (CONCAT);
        li << compound ("item");
        ctx.node_stack.push (li);
        break;
    }

    case MD_BLOCK_CODE: {
        MD_BLOCK_CODE_DETAIL* det = static_cast<MD_BLOCK_CODE_DETAIL*> (detail);
        string lang = md_str (det->lang.text, det->lang.size);
        tree   code (CONCAT);
        ctx.node_stack.push (code);
        (void) lang; /* TODO: use language for syntax highlighting */
        break;
    }

    case MD_BLOCK_QUOTE:
        ctx.node_stack.push (compound ("quote", tree (DOCUMENT)));
        break;

    case MD_BLOCK_HR:
        /* handled in leave_block */
        ctx.node_stack.push (tree (DOCUMENT));
        break;

    case MD_BLOCK_HTML:
        /* raw HTML: treat as plain text paragraph */
        ctx.node_stack.push (tree (CONCAT));
        break;

    default:
        ctx.node_stack.push (tree (DOCUMENT));
        break;
    }
    return 0;
}

static int
md_leave_block (MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto& ctx = *static_cast<md_context*> (userdata);
    (void) detail;

    if (type == MD_BLOCK_DOC) {
        tree t = ctx.node_stack.top ();
        ctx.node_stack.pop ();
        ctx.doc = t;
        return 0;
    }

    if (ctx.node_stack.size () <= 1) return 0;

    tree child = ctx.node_stack.top ();
    ctx.node_stack.pop ();

    switch (type) {
    case MD_BLOCK_H: {
        /* Heading: child is the CONCAT that collected the heading text
         * (pushed separately in md_enter_block); the heading container
         * (tag concat) sits right below it on the stack.  Merge text into
         * a fresh (tag ...) node and append it to the *real* parent —
         * which is the stack top AFTER popping the container.  (Grabbing a
         * reference to the container itself and appending there would drop
         * the heading into a node that is no longer reachable.) */
        tree content = child;                    /* collected heading text */
        tree container = ctx.node_stack.top ();  /* (tag concat) */
        ctx.node_stack.pop ();
        string tag = as_string (L (container));
        tree h = compound (tag);
        for (int i = 0; i < N (content); i++)
            h << content[i];
        if (ctx.node_stack.size () >= 1)
            ctx.node_stack.top () << h;
        break;
    }

    case MD_BLOCK_P:
    case MD_BLOCK_HTML:
        /* paragraph: child is a CONCAT, append to parent.
         * If the parent is itself a CONCAT (list item / heading), flatten
         * the paragraph's children into the parent instead of nesting a
         * CONCAT — TeXmacs list items are flat (concat (item) text...). */
        if (is_concat (ctx.node_stack.top ()) && is_concat (child)) {
            for (int i = 0; i < N (child); i++)
                ctx.node_stack.top () << child[i];
        }
        else {
            ctx.node_stack.top () << child;
        }
        break;

    case MD_BLOCK_CODE: {
        /* code block: child is CONCAT of text fragments
         * join into a single string */
        string code_text;
        for (int i = 0; i < N (child); i++) {
            if (i > 0) code_text << '\n';
            if (is_atomic (child[i]))
                code_text << child[i]->label;
        }
        ctx.node_stack.top () << compound ("verbatim", code_text);
        break;
    }

    case MD_BLOCK_HR:
        /* horizontal rule: replace dummy with hrule */
        ctx.node_stack.top () << compound ("hrule");
        break;

    case MD_BLOCK_UL:
    case MD_BLOCK_OL: {
        /* List finished: child is the DOCUMENT that collected the items
         * (pushed right after the container in md_enter_block).  Attach it
         * to the container, then append the container to the real parent.
         * Result: (itemize (document (item ...) (item ...))) — the
         * standard Mogan list shape which to_tmu serializes as a block. */
        tree doc       = child;
        tree container = ctx.node_stack.top ();
        ctx.node_stack.pop ();
        container << doc;
        if (ctx.node_stack.size () >= 1)
            ctx.node_stack.top () << container;
        break;
    }

    default:
        /* quote, list items, etc.: append as-is */
        ctx.node_stack.top () << child;
        break;
    }
    return 0;
}

/******************************************************************************
 * Span (inline) callbacks
 ******************************************************************************/

static int
md_enter_span (MD_SPANTYPE type, void* detail, void* userdata) {
    auto& ctx = *static_cast<md_context*> (userdata);

    /* For inline spans, push a CONCAT to collect content.
     * When leaving, we wrap it in the appropriate compound. */
    tree f;
    switch (type) {
    case MD_SPAN_EM:     f = tree (CONCAT); break;
    case MD_SPAN_STRONG: f = tree (CONCAT); break;
    case MD_SPAN_CODE:   f = tree (CONCAT); break;
    case MD_SPAN_DEL:    f = tree (CONCAT); break;
    case MD_SPAN_U:      f = tree (CONCAT); break;
    case MD_SPAN_MARK:   f = tree (CONCAT); break;
    case MD_SPAN_SUPERSCRIPT: f = tree (CONCAT); break;
    case MD_SPAN_SUBSCRIPT:   f = tree (CONCAT); break;
    case MD_SPAN_A: {
        auto* a = static_cast<MD_SPAN_A_DETAIL*> (detail);
        string href = md_str (a->href.text, a->href.size);
        /* store href in a temporary for later wrapping */
        f = tree (CONCAT);
        /* We need to remember the href. Use a simple trick:
         * push the href string as the first child, then unwrap later. */
        f << tree (href);
        break;
    }
    case MD_SPAN_IMG: {
        auto* img = static_cast<MD_SPAN_IMG_DETAIL*> (detail);
        string src   = md_str (img->src.text, img->src.size);
        string title = md_str (img->title.text, img->title.size);
        /* image: push a CONCAT with src and title as first children */
        f = tree (CONCAT);
        f << tree (src) << tree (title);
        break;
    }
    default:
        f = tree (CONCAT);
        break;
    }

    ctx.node_stack.push (f);
    return 0;
}

static int
md_leave_span (MD_SPANTYPE type, void* detail, void* userdata) {
    auto& ctx = *static_cast<md_context*> (userdata);
    (void) detail;

    if (ctx.node_stack.size () <= 1) return 0;

    tree content = ctx.node_stack.top ();
    ctx.node_stack.pop ();
    tree& parent = ctx.node_stack.top ();

    tree result;

    switch (type) {
    case MD_SPAN_EM:
        /* italic — standard TeXmacs form (tmu3-v2.lua):
         * <with|font-shape|italic|...> */
        result = compound ("with", "font-shape", "italic");
        break;
    case MD_SPAN_STRONG:
        /* bold — standard TeXmacs form (tmu3-v2.lua):
         * <with|font-series|bold|...> */
        result = compound ("with", "font-series", "bold");
        break;
    case MD_SPAN_CODE:
        /* code: join all text into single string */
        {
            string code_text;
            for (int i = 0; i < N (content); i++) {
                if (is_atomic (content[i]))
                    code_text << content[i]->label;
            }
            result = compound ("verbatim", code_text);
        }
        break;
    case MD_SPAN_DEL:
        result = compound ("strikeout");
        break;
    case MD_SPAN_U:
        result = compound ("underline");
        break;
    case MD_SPAN_MARK:
        result = compound ("mark");
        break;
    case MD_SPAN_SUPERSCRIPT:
        result = compound ("rsup");
        break;
    case MD_SPAN_SUBSCRIPT:
        result = compound ("rsub");
        break;
    case MD_SPAN_A: {
        /* content[0] is the href, rest is display text */
        string href = (N (content) >= 1 && is_atomic (content[0]))
                      ? content[0]->label : "";
        tree   display = tree (CONCAT);
        for (int i = 1; i < N (content); i++)
            display << content[i];
        result = compound ("hlink", display, href);
        break;
    }
    case MD_SPAN_IMG: {
        /* content[0] = src, content[1] = title, rest = alt text */
        string src   = (N (content) >= 1 && is_atomic (content[0]))
                       ? content[0]->label : "";
        string title = (N (content) >= 2 && is_atomic (content[1]))
                       ? content[1]->label : "";
        tree   alt = tree (CONCAT);
        for (int i = 2; i < N (content); i++)
            alt << content[i];
        (void) title;
        result = compound ("image", src, alt);
        break;
    }
    default:
        /* unknown span: pass content through */
        for (int i = 0; i < N (content); i++)
            parent << content[i];
        return 0;
    }

    /* For wrapping spans, move children from content to result */
    if (type != MD_SPAN_CODE && type != MD_SPAN_A && type != MD_SPAN_IMG) {
        for (int i = 0; i < N (content); i++)
            result << content[i];
    }

    parent << result;
    return 0;
}

/******************************************************************************
 * Text callback
 ******************************************************************************/

static int
md_text (MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
    auto& ctx = *static_cast<md_context*> (userdata);

    if (size == 0) return 0;
    if (ctx.node_stack.empty ()) return 0;

    string s = md_str (text, size);

    switch (type) {
    case MD_TEXT_NORMAL:
    case MD_TEXT_ENTITY:
        md_append_text (ctx, s);
        break;

    case MD_TEXT_CODE:
        /* inside code span/block */
        md_append_text (ctx, s);
        break;

    case MD_TEXT_HTML:
        md_append_text (ctx, s);
        break;

    case MD_TEXT_BR:
        md_append (ctx, compound ("next-line"));
        break;

    case MD_TEXT_SOFTBR:
        md_append_text (ctx, " ");
        break;

    case MD_TEXT_NULLCHAR:
        /* ignore */
        break;

    case MD_TEXT_LATEXMATH:
        /* TODO: handle LaTeX math */
        md_append_text (ctx, s);
        break;
    }
    return 0;
}

/******************************************************************************
 * Public API
 ******************************************************************************/

tree
markdown_to_tree (string s) {
    md_context ctx;

    MD_PARSER parser = {};
    parser.abi_version  = 0;
    parser.flags        = MD_FLAG_COLLAPSEWHITESPACE;
    parser.enter_block   = md_enter_block;
    parser.leave_block   = md_leave_block;
    parser.enter_span    = md_enter_span;
    parser.leave_span    = md_leave_span;
    parser.text          = md_text;

    c_string cs (s);
    md_parse ((const MD_CHAR*) (const char*) cs, N (s), &parser, &ctx);

    return ctx.doc;
}

tree
markdown_document_to_tree (string s) {
    /* Build a standard Mogan document tree rooted at DOCUMENT, mirroring
     * verbatim_document_to_tree():
     *   (DOCUMENT (body ...) (style (tuple "generic")) (initial ...))
     * markdown_to_tree() returns a bare (DOCUMENT <content...>) whose
     * children must be UNWRAPPED into the body node — the previous code
     * only wrapped when arity==0 (empty doc), so non-empty markdown came
     * back as a bare DOCUMENT with no body/initial framework and the
     * editor opened it as an empty buffer.
     *
     * The style child is mandatory: Mogan documents without it display
     * incorrectly (no sectioning was applied).  The standard default
     * style is <style|<tuple|generic>>, matching the "generic" fallback
     * in upgradetm (and the fromtm extraction of style attributes).
     */
    tree body= markdown_to_tree (string (s));
    tree b   = compound ("body");
    /* is_document (body): check the compound label directly — the old
     * `is_compound (body, "DOCUMENT")` never matched, because is_compound
     * compares against the DRD name which is lowercase ("document"). */
    if (is_document (body))
        for (int i= 0; i < N (body); i++)
            b << body[i];
    else
        b << body;
    tree init= tree (COLLECTION,
                     tree (ASSOCIATE, LANGUAGE, "english"),
                     tree (ASSOCIATE, PAR_FIRST, "0cm"));
    tree style= compound ("style", tree (TUPLE, "generic"));
    return tree (DOCUMENT, b, style, compound ("initial", init));
}
