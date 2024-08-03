;;; bl-mode.el --- Major mode for the BL language.

;; Maintainer: Martin Dorazil
;; Version: 1.0

;;; Commentary:

;; Major mode for editing BL files.

;;; Code:

(require 'js)

(defvar bl-mode-syntax-table
  (let ((st (make-syntax-table)))
    (modify-syntax-entry ?_ "w" st)
    (modify-syntax-entry ?# "w" st)
    (modify-syntax-entry ?/ ". 124b" st)
    (modify-syntax-entry ?* ". 23" st)
    (modify-syntax-entry ?\n "> b" st)
    st)
  "Syntax table used while in BL mode.")

(defvar bl-font-lock-keywords
  (list
   ;; Keywords
   `(,(regexp-opt
       '("loop" "if" "switch" "continue" "else" "defer" "struct" "enum" "union" "fn" "return" "cast" "auto" "default" "using" "break" "unreachable" "then") 'symbols) . font-lock-keyword-face)
   ;; Preprocessor
   `(,(regexp-opt
       '("#load" "#link" "#call_location" "#extern" "#compiler" "#private" "#inline" "#noinline" "#file" "#line" "#base" "#entry" "#build_entry" "#if" "#tag" "#noinit" "#intrinsic" "#test" "#import" "#export" "#scope" "#thread_local" "#flags" "#maybe_unused" "#comptime" "#obsolete" "#enable_if") 'symbols) . font-lock-preprocessor-face)
   ;; Builtin functions
   `(,(regexp-opt
       '("sizeof" "typeof" "alignof" "typeinfo" "typekind" "typeid" "panic" "assert" "static_assert" "debugbreak") 'symbols) . font-lock-builtin-face)
   ;; Builtin constants
   `(,(regexp-opt
       '("true" "false" "null") 'symbols) . font-lock-constant-face)
   ;; Functions
   `(,"\\([A-Za-z][A-Za-z0-9_]*\\)\s*:\.*:\s*fn" 1 font-lock-function-name-face)
   ;; Record types and enums
   `(,"\\([A-Za-z][A-Za-z0-9_]*\\)\s*:\.*:\s*\\(struct\\|union\\|enum\\)" 1 font-lock-type-face)
   ;; Types
   `(,(regexp-opt '("u8" "u16" "u32" "u64" "s8" "s16" "s32" "s64" "f32" "f64" "string" "string_view" "type" "Error" "Any" "bool") 'symbols) . font-lock-type-face))
  "Syntax highlighting for BL.")

;; Emacs 23 compatibility.
(defalias 'bl-mode-prog-mode
  (if (fboundp 'prog-mode)
      'prog-mode
    'fundamental-mode))

;;;###autoload
(define-derived-mode bl-mode bl-mode-prog-mode "BL"
  "Major mode for editing BL source files.
\\{bl-mode-map}
  Runs `bl-mode-hook' on startup."
  (setq font-lock-defaults `(bl-font-lock-keywords))
  (setq-local indent-line-function 'js-indent-line)
  (setq-local comment-start "/"))

;;;###autoload
(add-to-list 'auto-mode-alist (cons "\\.bl\\'" 'bl-mode))

(provide 'bl-mode)

;;; bl-mode.el ends here
