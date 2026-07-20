# Mogan Markdown 转换与透明输入修复方案

> 本文档由架构分析得出，供在任意目录（A 或 B）重新启动 Codex 会话时加载上下文。
> 当前两处仓库（A: codex-mogan-mod/.../mogan-2026.2.6，B: qwenpaw/.../moganlab-src/mogan-source/mogan-2026.2.6）HEAD 均为 7fa55d4，内容逐字节一致（B 仅缺 .agents 目录）。

## 架构概览

Mogan 是 TeXmacs 深度定制版，Markdown 转换分两层：

- C++ 转换核心：src/Data/Convert/（markdown_import.cpp / markdown_export.cpp / Generic/generic.cpp）提供 markdown_to_tree / tree_to_markdown。
- Scheme 转换器注册：TeXmacs/plugins/markdown/progs/data/markdown.scm 通过 converter 宏把 C++ 包装函数注册到 convert 路径表。
- 透明输入：src/Data/Markdown/markdown_input.cpp 的 apply_markdown_inline_conversion / apply_markdown_heading_conversion，由 src/Edit/Interface/edit_interface.cpp:1018 的 apply_changes() 调用，受偏好 "markdown input" 开关控制（默认 "on"）。
- Scheme<->C++ 绑定：src/Scheme/Glue/glue_basic.lua（被 src/Scheme/L5/init_glue_l5.cpp 加载）以 scm_name/cpp_name 暴露 C++ 函数给 Scheme。

## 根因（已坐实）

### 根因 1：导出写入 "Error: bad format or data."（致命，递归）
- 递归点：`(converter texmacs-tree markdown-snippet (:function cpp-texmacs->markdown))`（markdown.scm）使 cpp-texmacs->markdown 调用 texmacs->generic -> convert doc "texmacs-tree" "markdown-snippet" -> 查表再次命中 cpp-texmacs->markdown -> 无限递归。
- 最终 convert 返回 #f，texmacs->generic（tm-convert.scm:361）兜底输出 "Error: bad format or data."。
- 注意：generic.cpp 的 tree_to_generic 对 markdown-snippet 直调 tree_to_markdown 不经过 Scheme，问题仅在 Scheme 层 cpp-texmacs->markdown 走 convert 路径表回环。
- 正确修法：让 Scheme 包装器直调 C++ 的 tree_to_markdown / markdown_to_tree（经 glue 暴露的 primitive），绕过 convert 路径表。

### 根因 2：导入显示空白（致命，fall-through）
- src/Data/Convert/Generic/input.cpp:233 case MODE_MARKDOWN 后缺 break，执行 markdown_flush 后 fall-through 进 MODE_HTML 的 html_flush，把刚写入的树当 HTML 处理导致空白。
- markdown_flush（input.cpp:310）仅 force 时写；导入路径 DATA_END（input.cpp:140）与 eof()（input.cpp:178）均调 flush(true)，force 必触发，补 break 安全不丢内容。

### 根因 3：透明输入打字+粘贴都不生效（逻辑缺陷，THE_TREE 门槛）
- edit_interface.cpp:1018-1024 用 `if (env_change & THE_TREE)` 包住两个转换函数。纯文本打字/粘贴只置文本或 cursor 标志，不置 THE_TREE，整段被跳过。
- 偏好 "markdown input" 默认 "on" 可被 get_preference 读到（非问题）。
- markdown_input.cpp 的 apply_markdown_* 内部已对 et 做合法性检查（非 CONCAT/已格式化则 no-op），放宽触发条件安全。

## 修复方案（A 变体，已批准方向）

### 第 1 步：修导出递归 + 暴露 C++ 原语
- 在 src/Scheme/Glue/glue_basic.lua 新增两个 primitive：
  - scm_name="tree-to-markdown", cpp_name="tree_to_markdown", ret_type="string", arg_list={"tree"}
  - scm_name="markdown-to-tree", cpp_name="markdown_to_tree", ret_type="scheme_tree", arg_list={"string"}
  （convert.hpp:68/67 已声明这两个 C++ 函数，无需改 C++ 函数体）
- 修改 TeXmacs/plugins/markdown/progs/data/markdown.scm 四个包装函数体：
  - cpp-texmacs->markdown: (texmacs->generic t "markdown-snippet") -> (call "tree-to-markdown" t)
  - cpp-texmacs->markdown-document: (texmacs->generic t "markdown-document") -> (call "tree-to-markdown" t)
  - cpp-markdown->texmacs: (generic->texmacs s "markdown-snippet") -> (call "markdown-to-tree" s)
  - cpp-markdown-document->texmacs: (generic->texmacs s "markdown-document") -> (call "markdown-to-tree" s)
- 保留 Scheme 中间层做错误兜底（C++ 返回空/异常时不输出 "Error: bad format..."）。同步更新文件头注释。

### 第 2 步：修导入空白
- src/Data/Convert/Generic/input.cpp:234 之后补 break，阻止 fall-through 进 html_flush。
- 已确认 markdown_flush 在导入场景被 force 触发，补 break 安全。

### 第 3 步：修透明输入触发条件
- edit_interface.cpp:1018 附近，将 `if (env_change & THE_TREE) { apply_markdown_inline_conversion(...); apply_markdown_heading_conversion(...); }` 改为偏好开启时对更宽标志触发（如 THE_TREE | THE_CURSOR | THE_SELECTION，或内容非空即运行），让纯文本打字和粘贴都触发。

### 第 4 步：round-trip 标签对齐（只读核对，大概率不改）
- 核对 markdown_import.cpp 与 markdown_export.cpp 的 section/subsection/verbatim/hlink 映射一致。export 侧 heading 标签（section..subsubparagraph）与 markdown_input.cpp 映射一致；import 侧需再确认，一致则不改。

### 第 5 步：构建验证
- xmake b stem 构建。
- 最小验证：导入不空白、导出非错误串、透明输入（打字 **x** 与粘贴 .md）生效。

## 目录说明
- 用户给定的 ~/.qwenpaw/.../moganlab-src/ 本身不是 Mogan 仓库（git 根为上一级 default/，历史为中文技术文档分析），真正 Mogan 源码在其 moganlab-src/mogan-source/mogan-2026.2.6/ 子目录。
- 本文件应放在 Mogan 仓库根（mogan-2026.2.6/），而非 moganlab-src/。
