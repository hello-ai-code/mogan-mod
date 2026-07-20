;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; MODULE      : markdown.scm
;; DESCRIPTION : Markdown data format
;; COPYRIGHT   : (C) 2026  Mogan contributors
;;
;; This software falls under the GNU general public license version 3 or later.
;; It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
;; in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(texmacs-module (data markdown)
  (:use (kernel texmacs tm-convert)
        (kernel texmacs tm-preferences)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; B.4 Markdown transparent input: runtime toggle preference
;; Defaults to "on" so behavior matches the previous always-on build.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-preferences
  ("markdown input" "on" noop))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Markdown format definition
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-format markdown-snippet
  (:name "Markdown snippet")
  (:suffix "md" "markdown" "mdown"))

(define-format markdown-document
  (:name "Markdown document")
  (:suffix "md" "markdown" "mdown"))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Format detection
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(tm-define (markdown-recognizes? s)
  (:synopsis "Quick heuristic to detect if string s is Markdown")
  (and (string? s)
       (or (string-starts? s "#")
           (string-starts? s "---")
           (string-starts? s ">")
           (string-contains? s "\n#")
           (string-contains? s "\n---")
           (string-contains? s "\n>")
           (string-contains? s "\n- ")
           (string-contains? s "\n* ")
           (string-contains? s "\n1. ")
           (string-contains? s "```"))))

(define-format markdown-snippet
  (:must-recognize markdown-recognizes?))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Converters
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

/*
 * The actual conversion is done in C++ via:
 *   - markdown_to_tree(s) -> tree
 *   - tree_to_markdown(doc) -> string
 *
 * Previously these wrappers called generic->texmacs / texmacs->generic,
 * which route through the convert path table and re-enter the very
 * converter being registered here, causing infinite recursion and the
 * "Error: bad format or data." fallback. We now call the C++ primitives
 * directly (exposed through glue_basic.lua) to bypass the path table.
 * The Scheme layer still guards against empty/erroneous C++ output.
 */

; Wrapper for markdown -> texmacs-tree
(tm-define (cpp-markdown->texmacs s opt)
  (:synopsis "Convert Markdown string to TeXmacs tree")
  (with r (markdown-to-tree s)
    (if (and r (not (tree-is? r 'error)))
        r
        (stree->tree '(error "bad format or data"))))
) ;tm-define

(tm-define (cpp-markdown-document->texmacs s opt)
  (:synopsis "Convert Markdown document string to full TeXmacs tree")
  (with r (markdown-to-tree s)
    (if (and r (not (tree-is? r 'error)))
        r
        (stree->tree '(error "bad format or data"))))
) ;tm-define

; Wrapper for texmacs-tree -> markdown
(tm-define (cpp-texmacs->markdown t opt)
  (:synopsis "Convert TeXmacs tree to Markdown string")
  (with r (tree-to-markdown t)
    (if (string? r) r "Error: bad format or data."))
) ;tm-define

(tm-define (cpp-texmacs->markdown-document t opt)
  (:synopsis "Convert TeXmacs tree to Markdown document string")
  (with r (tree-to-markdown t)
    (if (string? r) r "Error: bad format or data."))
) ;tm-define

; Register converters using correct :function syntax (inline body is silently ignored)
(converter markdown-snippet texmacs-tree
  (:penalty 1.0)
  (:function cpp-markdown->texmacs)
) ;converter

(converter markdown-document texmacs-tree
  (:penalty 1.0)
  (:function cpp-markdown-document->texmacs)
) ;converter

(converter texmacs-tree markdown-snippet
  (:penalty 1.0)
  (:function cpp-texmacs->markdown)
) ;converter

(converter texmacs-tree markdown-document
  (:penalty 1.0)
  (:function cpp-texmacs->markdown-document)
) ;converter