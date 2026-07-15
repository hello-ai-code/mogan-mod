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
 *   - generic_to_tree(s, "markdown-*") -> tree
 *   - tree_to_generic(doc, "markdown-*") -> string
 *
 * The C++ functions are exported to Scheme as generic->texmacs and texmacs->generic.
 */

; Wrapper for markdown -> texmacs-tree
(tm-define (cpp-markdown->texmacs s opt)
  (:synopsis "Convert Markdown string to TeXmacs tree")
  (generic->texmacs s "markdown-snippet")
) ;tm-define

(tm-define (cpp-markdown-document->texmacs s opt)
  (:synopsis "Convert Markdown document string to full TeXmacs tree")
  (generic->texmacs s "markdown-document")
) ;tm-define

; Wrapper for texmacs-tree -> markdown
(tm-define (cpp-texmacs->markdown t opt)
  (:synopsis "Convert TeXmacs tree to Markdown string")
  (texmacs->generic t "markdown-snippet")
) ;tm-define

(tm-define (cpp-texmacs->markdown-document t opt)
  (:synopsis "Convert TeXmacs tree to Markdown document string")
  (texmacs->generic t "markdown-document")
) ;tm-define

; Register converters using Scheme macro
(converter markdown-snippet texmacs-tree
  (:penalty 1.0)
  (cpp-markdown->texmacs from)
) ;converter

(converter markdown-document texmacs-tree
  (:penalty 1.0)
  (cpp-markdown-document->texmacs from)
) ;converter

(converter texmacs-tree markdown-snippet
  (:penalty 1.0)
  (cpp-texmacs->markdown to)
) ;converter

(converter texmacs-tree markdown-document
  (:penalty 1.0)
  (cpp-texmacs->markdown-document to)
) ;converter
