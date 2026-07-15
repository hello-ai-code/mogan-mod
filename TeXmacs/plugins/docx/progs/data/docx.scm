;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; MODULE      : docx.scm
;; DESCRIPTION : DOCX data format
;; COPYRIGHT   : (C) 2024  ATQlove
;;
;; This software falls under the GNU general public license version 3 or later.
;; It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
;; in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(import (liii uuid))
(import (liii os))

(texmacs-module (data docx)
  (:use (binary pandoc)
        (texmacs texmacs tm-files)
        (network url)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; DOCX format defination
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-format docx
  (:name "docx")
  (:suffix "docx"))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Function to export TeXmacs document to DOCX using Pandoc
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(tm-define (texmacs-tree->docx-string t opt)
  (:synopsis "Export TeXmacs document to DOCX format using Pandoc")
  (let* (
         (temp-name (string-append "/" (uuid4)))  
         (temp-dir (string-append (os-temp-dir) temp-name))
         (tex-temp-url (system->url (string-append temp-dir ".tex")))
         (docx-temp-url (system->url (string-append temp-dir ".docx")))
         (tex-dir (url-head (url->string tex-temp-url)))
         (tex-dir-str (url->string tex-dir)))
    ;; First, export the document to LaTeX (preserves more structure than HTML)
    (export-buffer-main (current-buffer) tex-temp-url "latex" ())
    ;; Then, use Pandoc to convert the LaTeX to DOCX
    (if (has-binary-pandoc?)
        (begin
        (chdir tex-dir-str)
        (let ((cmd (string-append "\"" (url->string (find-binary-pandoc)) "\""
                                  " \""
                                  (url->string tex-temp-url)
                                  "\" -o \""
                                  (url->string docx-temp-url)
                                  "\"")))
          (debug-message "debug-io" (string-append "debug: cmd for Pandoc: " cmd "\n")) ;; For debugging
          (system cmd)
          (with result (string-load docx-temp-url)
            (system-remove tex-temp-url)
            (system-remove docx-temp-url)
            result))
          ) ;; Expected: $TEXMACS_PATH/tests/tm.tex")
        (error "Pandoc binary not found"))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Converter for exporting TeXmacs tree to DOCX string
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(converter texmacs-tree docx-document
  (:require (has-binary-pandoc?))
  (:function-with-options texmacs-tree->docx-string)
  (:option "texmacs->latex:source-tracking" "off")
  (:option "texmacs->latex:conservative" "on")
  (:option "texmacs->latex:expand-macros" "on")
  (:option "texmacs->latex:expand-user-macros" "off")
  (:option "texmacs->latex:use-macros" "off")
  (:option "texmacs->latex:encoding" "UTF-8"))
