//markdown_input.cpp新版本：局部转换实现

#include "markdown_inline_patterns.hpp"
#include "path.hpp"
#include "tree.hpp"
#include "tree_cursor.hpp"
#include "tree_helper.hpp"
#include "tree_observer.hpp"  // assign(): protocol-correct tree modification

#include <cstdio>

using namespace moebius;

/* TEMPORARY DEBUG (B.4 diagnosis) — prints every step of the markdown
   transparent-input pipeline to stderr.  Remove once the "typed markdown
   does not convert" bug is fixed.  Enable with -DMD_DEBUG=1 or by flipping
   the constant below to 1. */
#ifndef MD_DEBUG
#define MD_DEBUG 1
#endif
#define MD_LOG(...) do { if (MD_DEBUG) { fprintf (stderr, "[MD] " __VA_ARGS__); fflush (stderr); } } while (0)

/* Moebius string has no c_str(): use as_charp (from lolly string.hpp). */
#define MD_S(s) as_charp (s)

/******************************************************************************
 * 检查是否包含任何格式化标记的辅助函数
 ******************************************************************************/
static bool
has_formatting (tree t) {
    if (is_atomic (t)) return false;
    if (N (t) == 0) return false;
    if (!is_func (t, CONCAT)) return true;
    for (int i = 0; i < N (t); i++) {
        if (has_formatting (t[i])) return true;
    }
    return false;
}

/******************************************************************************
 * 递归收集树中所有文本的辅助函数
 ******************************************************************************/
static void
collect_text (tree t, string& out) {
    if (is_atomic (t)) {
        out << t->label;
    } else {
        for (int i = 0; i < N (t); i++) {
            collect_text (t[i], out);
        }
    }
}

/******************************************************************************
 * 找到光标所在的原子节点
 *
 * Mogan存储文本的方式：
 *   - 单个文字/原子直接存储在CONCAT中
 *   - 持续输入会形成嵌套CONCAT(原子1, 原子2, ...)
 * 光标路径(tp)指向光标所在的具体位置（可能是字符偏移，如 1.0.0.5），
 * 我们需要向上遍历找到包含光标的原子节点
 ******************************************************************************/
static bool
find_cursor_atomic (tree& et, path tp, path& atom_p) {
    MD_LOG ("  find_cursor_atomic: tp=%s\n", MD_S (as_string (tp)));
    path p = tp;

    // 向上遍历，直到找到原子节点或到达根
    while (!is_nil (p)) {
        if (has_subtree (et, p) && is_atomic (subtree (et, p))) {
            atom_p = p;
            MD_LOG ("  find_cursor_atomic: found atomic node at path: %s\n", MD_S (as_string (p)));
            return true;
        }
        p = path_up (p);
    }

    MD_LOG ("  find_cursor_atomic: no atomic node found for tp=%s\n", MD_S (as_string (tp)));
    return false;
}

/******************************************************************************
 * 应用Markdown内联转换（局部转换）
 * 
 * 与之前的版本不同，这个版本不会替换整个段落，而是：
 * 1. 找到光标所在的原子节点
 * 2. 只在该原子节点的文本内查找Markdown模式
 * 3. 用格式化树替换被匹配的模式，保留前后文本
 * 4. 设置光标位置到格式化内容之后
 ******************************************************************************/
bool
apply_markdown_inline_conversion (tree& et, path tp, path& out_p,
                                  path& out_tp) {
    MD_LOG ("inline: enter tp=%s\n", MD_S (as_string (tp)));
    if (is_nil (tp)) return false;

    // 找到光标所在的原子节点
    path atom_p;
    if (!find_cursor_atomic (et, tp, atom_p)) return false;
    
    MD_LOG ("inline: found atomic node at path: %s\n", MD_S (as_string (atom_p)));
    
    tree& atomic_node = subtree (et, atom_p);
    
    // 原子节点必须是纯文本（否则无法转换Markdown）
    if (has_formatting (atomic_node)) return false;
    
    // 获取原子节点的内容
    string atomic_text = as_string (atomic_node);
    MD_LOG ("inline: atomic_text=\"%s\" len=%d\n", MD_S (atomic_text), N (atomic_text));
    
    if (N (atomic_text) == 0) return false;

    // 检查是否包含完整的Markdown标记
    bool complete = is_complete_markdown_input_utf8 (atomic_text);
    MD_LOG ("inline: is_complete_markdown_input_utf8=%d\n", (int) complete);
    if (!complete) return false;

    // 使用UTF-8安全的解析器找到第一个完整的Markdown模式
    md_local_match result = try_parse_inline_markdown_utf8 (atomic_text);
    MD_LOG ("inline: parse result valid=%d start_char=%d end_char=%d type=%s\n", 
            (int) result.valid, result.start_char, result.end_char, MD_S (result.pattern_type));

    if (!result.valid) return false;

    // 将UTF-8字符位置转换为字节位置
    int start_byte = 0;
    int end_byte = 0;
    
    for (int i = 0; i < result.start_char; i++) {
        start_byte = mdutf8::next (atomic_text, start_byte);
    }
    for (int i = 0; i < result.end_char; i++) {
        end_byte = mdutf8::next (atomic_text, end_byte);
    }
    
    MD_LOG ("inline: byte positions: start=%d, end=%d, matched text=\"%s\"\n", 
            start_byte, end_byte, MD_S (atomic_text (start_byte, end_byte)));

    // 获取前后文本
    string prefix = atomic_text (0, start_byte);
    string match_text = atomic_text (start_byte, end_byte);
    string suffix = atomic_text (end_byte, N (atomic_text));
    
    MD_LOG ("inline: prefix=\"%s\" suffix=\"%s\"\n", MD_S (prefix), MD_S (suffix));

    // 构建新的原子节点：前文 + 格式化树 + 后文
    tree new_atomic_node = tree (CONCAT);
    
    if (N (prefix) > 0) {
        new_atomic_node << tree (prefix);
    }
    
    if (!is_atomic (result.converted) || N (as_string (L (result.converted))) > 0) {
        new_atomic_node << result.converted;
    }
    
    if (N (suffix) > 0) {
        new_atomic_node << tree (suffix);
    }

    // 使用assign替换原子节点（保持observer网络完整）
    assign (atomic_node, new_atomic_node);
    
    MD_LOG ("inline: assigned new atomic node, original atomic: %s\n", MD_S (as_string (atomic_node)));

    // 设置输出路径：光标位置在格式化内容之后
    out_p = atom_p;

    /* CRASH/NAVIGATION FIX (2026-08-29): previously the cursor was computed
       as atom_p * total_chars_to_move, i.e. the NUMBER OF CHARS was used as
       the CONCAT CHILD INDEX.  After conversion atom_p holds a CONCAT like
       CONCAT(strong("bold")) whose arity is 1, so a path like 1.0.0.8 is
       OUT OF BOUNDS — the next Enter/arrow used an illegal path and the
       cursor could not move (Enter produced no newline).  Instead descend to
       the LAST LEAF atom of the converted CONCAT and place the cursor past
       its text, exactly like the heading pass does. */
    path leaf = atom_p * (N (new_atomic_node) - 1);
    while (!is_atomic (subtree (et, leaf)))
        leaf = leaf * (N (subtree (et, leaf)) - 1);
    out_tp = leaf * N (as_string (subtree (et, leaf)));
    MD_LOG ("inline: CONVERTED -> out_p=%s out_tp=%s new_arity=%d\n",
            MD_S (as_string (out_p)), MD_S (as_string (out_tp)),
            N (new_atomic_node));
    return true;
}

/******************************************************************************
 * B.4.1  块级标题：# / ## / ... / ###### at the start of a paragraph
 *        转换成TeXmacs section标题
 *        与inline pass保持相同的实现方式
 ******************************************************************************/
bool
apply_markdown_heading_conversion (tree& et, path tp, path& out_p,
                                   path& out_tp) {
    MD_LOG ("heading: enter tp=%s\n", MD_S (as_string (tp)));
    if (is_nil (tp)) return false;

    /* 找到光标所在的原子节点 */
    path atom_p;
    if (!find_cursor_atomic (et, tp, atom_p)) return false;
    
    tree& atomic_node = subtree (et, atom_p);

    /* 原子节点必须是纯文本 */
    if (has_formatting (atomic_node)) return false;

    /* 收集文本 */
    string text = as_string (atomic_node);
    MD_LOG ("heading: text=\"%s\"\n", MD_S (text));
    int n = N (text);
    int hashes = 0;
    while (hashes < n && text[hashes] == '#') hashes++;
    if (hashes < 1 || hashes > 6) return false;          // 1..6 levels only
    if (hashes >= n || text[hashes] != ' ') return false; // must be "# "
    int after = hashes + 1;                               // skip "# "

    /* CRASH FIX: empty heading ("# " only) stays plain text.
       A nullary section() has no legal cursor position; converting now
       would crash on the next keystroke.  Wait for the title text. */
    string rest = text (after, n);
    if (N (rest) == 0) return false;

    /* DEFER (2026-08-26): if the text right after "# " starts with an inline
       Markdown marker, the user is typing something like "# **bold**", not a
       plain title.  Converting now would eat their first '*' into a plain
       heading and create confusing intermediate states.  Let the inline pass
       handle it first — once formatting appears, has_formatting() keeps this
       pass away from the paragraph for good. */
    if (!N (rest) == 0) {
        int first_byte = rest[0];
        if (first_byte == '*' || first_byte == '_' || first_byte == '~' || first_byte == '`') return false;
    }

    /* Map level -> TeXmacs tag (keep in sync with markdown_import.cpp). */
    string tag;
    switch (hashes) {
    case 1: tag = "section";        break;
    case 2: tag = "subsection";     break;
    case 3: tag = "subsubsection";  break;
    case 4: tag = "paragraph";      break;
    case 5: tag = "subparagraph";   break;
    default: tag = "subsubparagraph"; break;
    }

    /* Replace the atomic node in place with the heading (arity 1: text atom).
       atom_p is preserved, so the outer path indices stay valid.
       ROOT-CAUSE FIX (2026-08-26): use the official assign() primitive —
       raw C++ reference assignment left the observer network (bridge, undo,
       ip_observers) out of sync and the NEXT keystroke SIGSEGV'd.  The
       bridge auto-syncs through the observer chain; do NOT typeset_invalidate
       manually anymore. */
    tree fresh = compound (tag, tree (rest));
    assign (atomic_node, fresh);
    /* Bail out if apply() postponed us (is_busy queue) — retry next round. */
    if (!strong_equal (atomic_node, fresh)) {
      MD_LOG ("heading: assign postponed by busy queue, skipping round\n");
      return false;
    }
    out_p = atom_p;
    /* Move the cursor to the END of the heading text atom.
       CRASH FIX (2026-08-18): end(et, parent_p) on a section/subsection
       (an ENFORCING node) returns the enforcing node ITSELF (parent_p.0),
       not the end of the text inside it — the same defect as the inline
       pass.  The cursor cannot rest on an enforcing node, so the next
       keystroke used an out-of-bounds path and SIGSEGV'd (confirmed by the
       [MD] log: tp jumped from 1.0.1 back to 1.0.0.14 on a section node).
       Instead descend to the last atomic leaf and position past its text
       — the same convention tree_traverse.cpp uses for text atoms.
       NOTE (2026-08-21): must use N(fresh) (the tree we assigned), NOT N(parent_p)
       — parent_p is a PATH (array<int>): N() on a path returns the PATH LENGTH
       (e.g. 2 for "1.0"), not the arity of the heading node.
       parent_p * (2-1) = 1.0.1 is OUT OF BOUNDS for section(rest) (arity 1,
       only child index 0), and subtree(et, 1.0.1) crashed with SIGSEGV
       (confirmed in the 8/21 md_debug.log).  Arity is 1, so leaf = parent_p * 0
       = the text atom. (2026-08-26: reads N(fresh) — the tree we assigned.) */
    path leaf = atom_p * (N (fresh) - 1);
    while (!is_atomic (subtree (et, leaf)))
        leaf = leaf * (N (subtree (et, leaf)) - 1);
    out_tp = leaf * N (as_string (subtree (et, leaf)));
    MD_LOG ("heading: CONVERTED -> out_p=%s out_tp=%s tag=%s rest=\"%s\"\n",
            MD_S (as_string (out_p)), MD_S (as_string (out_tp)), MD_S (tag),
            MD_S (rest));
    return true;
}