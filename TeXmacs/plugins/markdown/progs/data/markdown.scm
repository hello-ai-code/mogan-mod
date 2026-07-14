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
  (:use (kernel texmacs tm-convert)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Markdown format definition
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-format markdown
  (:name "Markdown")
  (:suffix "md" "markdown" "mdown"))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Markdown format detection
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

(define-format markdown
  (:must-recognize markdown-recognizes?))
