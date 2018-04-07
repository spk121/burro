(define-module (burro debug)
  #:use-module (burro engine)
  #:use-module (rnrs io ports)
  #:use-module (ice-9 pretty-print)
  #:use-module (system vm frame)
  #:use-module (system repl repl)
  #:use-module (system repl debug)
  #:export (log-info
	    Format)
  #:re-export (frame-procedure-name
	       stack->vector
	       procedure-name))

;; Internal flags
(define LOG_FLAG_FATAL     (ash 1 1))
;; Log level for errors and assertion failures
(define LOG_LEVEL_ERROR    (ash 1 2))
;; Log level for serious warnings
(define LOG_LEVEL_CRITICAL (ash 1 3))
(define LOG_LEVEL_WARNING  (ash 1 4))
(define LOG_LEVEL_MESSAGE  (ash 1 5))
;; For informational messages
(define LOG_LEVEL_INFO     (ash 1 6))
(define LOG_LEVEL_DEBUG    (ash 1 7))

(define-syntax __FILE__
  (syntax-rules ()
    ((_)
     (if (assv-ref (current-source-location) 'filename)
	 (basename (assv-ref (current-source-location) 'filename))
	 ;; else
	 "(unknown file)"))))

(define-syntax __LINE__
  (syntax-rules ()
    ((_)
     (or (assv-ref (current-source-location) 'line)
	 -1))))

(define-syntax __FUNC__
  (syntax-rules ()
    ((_)
     (let ((stk (stack->vector (make-stack #t))))
       (let loop ((i 1))
	 (cond
	  ((frame-procedure-name (vector-ref stk i))
	   (let ((pname (frame-procedure-name (vector-ref stk i))))
	     (cond
	      ((eqv? pname '%start-stack)
	       "(top level)")
	      ((eqv? pname 'save-module-excursion)
	       "(top level)")
	      (else
	       (symbol->string pname)))))
	  ((< i (vector-length stk))
	   (loop (1+ i)))
	  (else
	   "(unknown func)")))))))

(define-syntax STRLOC
  (syntax-rules ()
    ((_)
     (let ((loc (assv-ref (current-source-location) 'line)))
       (if loc
	   (number->string loc)
	   "(unknown line)")))))

(define-syntax __LOCALS__
  (syntax-rules ()
    ((_)
     (let ((stk (stack->vector (make-stack #t)))
	   (out "...\n"))
       (let loop ((i 1))
	 (let* ((frame (vector-ref stk i))
		(name (frame-procedure-name frame)))
	   (set! out
	     (string-append out
			    (with-output-to-string
			      (lambda ()
				(print-locals frame
					      #:width 160
					      #:per-line-prefix "**   ")))))
	   (if (and (not name) (< i (vector-length stk)))
	       (loop (1+ i))
	       ;; else
	       out)))))))

(define-syntax log-debug-locals
  (syntax-rules ()
    ((_ ...)
     (log-structured LOG_LEVEL_DEBUG
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" (string->format-escaped-string (__LOCALS__))))))

(define-syntax log-debug-time
  (syntax-rules ()
    ((_ ...)
     (log-structured LOG_LEVEL_DEBUG
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" "Time is ~6,5f"
		     (* 1e-6 (- (monotonic-time) *log-start-time*))))))

(define-syntax log-error
  (syntax-rules ()
    ((_ msg ...)
     (log-structured LOG_LEVEL_ERROR
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" msg ...))))

(define-syntax log-message
  (syntax-rules ()
    ((_ msg ...)
     (log-structured LOG_LEVEL_MESSAGE
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" msg ...))))

(define-syntax log-critical
  (syntax-rules ()
    ((_ msg ...)
     (log-structured LOG_LEVEL_CRITICAL
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" msg ...))))

(define-syntax log-warning
  (syntax-rules ()
    ((_ msg ...)
     (log-structured LOG_LEVEL_WARNING
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" msg ...))))

(define-syntax log-info
  (syntax-rules ()
    ((_ msg ...)
     (log-structured LOG_LEVEL_INFO
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" msg ...))))

(define-syntax log-debug
  (syntax-rules ()
    ((_ msg ...)
     (log-structured LOG_LEVEL_DEBUG
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" msg ...))))

(define-syntax warn-if-reached
  (syntax-rules ()
    ((_)
     (log-structured LOG_LEVEL_CRITICAL
		     "CODE_FILE" (__FILE__)
		     "CODE_LINE" (STRLOC)
		     "CODE_FUNC" (__FUNC__)
		     "MESSAGE" "code should not be reached"))))

(define-syntax warn-if-used
  (syntax-rules ()
    ((_ val)
     (begin
       (log-structured LOG_LEVEL_CRITICAL
		       "CODE_FILE" (__FILE__)
		       "CODE_LINE" (STRLOC)
		       "CODE_FUNC" (__FUNC__)
		       "MESSAGE" "value '~a' should not be used"
		       val)
       val))))

(define-syntax warn-if-false
  (syntax-rules ()
    ((_ expr)
     (let ((ret expr))
       (or ret
	   (begin
	     (log-structured LOG_LEVEL_CRITICAL
			     "CODE_FILE" (__FILE__)
			     "CODE_LINE" (STRLOC)
			     "CODE_FUNC" (__FUNC__)
			     "MESSAGE" "'~a' is false"
			     (quote expr))
	     #f))))))

(define-syntax warn-val-if-false
  (syntax-rules ()
    ((_ expr val)
     (let ((ret expr))
       (or ret
	   (begin
	     (log-structured LOG_LEVEL_CRITICAL
			     "CODE_FILE" (__FILE__)
			     "CODE_LINE" (STRLOC)
			     "CODE_FUNC" (__FUNC__)
			     "MESSAGE" "'~a' is false. '~a'='~a'"
			     (quote expr) (quote val) val)
	     #f))))))

(define-syntax log-debug-pk
  (syntax-rules ()
    ((_ expr)
     (let ((ret expr))
       (log-structured LOG_LEVEL_DEBUG
		       "CODE_FILE" (__FILE__)
		       "CODE_LINE" (STRLOC)
		       "CODE_FUNC" (__FUNC__)
		       "MESSAGE" "'~s' is '~s'"
		       (quote expr) ret)
       ret))))

(define (log-level->priority log-level)
  "A one-byte name for a log level."
  (cond
   ((logtest log-level LOG_LEVEL_ERROR)    "3")
   ((logtest log-level LOG_LEVEL_CRITICAL) "4")
   ((logtest log-level LOG_LEVEL_WARNING)  "4")
   ((logtest log-level LOG_LEVEL_MESSAGE)  "5")
   ((logtest log-level LOG_LEVEL_INFO)     "6")
   ((logtest log-level LOG_LEVEL_DEBUG)    "7")))

(define (%add-fields-from-list-to-alist _output rest)
  (let loop ((key #f)
	     (cur (car rest))
	     (rest (cdr rest))
	     (output _output))
    (cond
     ((not key)
      (if (null? rest)
	  (begin
	    ;; We have a leftover key with no value.
	    output)
	  ;; Else, this element must be a key.
	  (loop cur (car rest) (cdr rest) output)))
     ((not (string=? key "MESSAGE"))
      (if (null? rest)
	  (begin
	    ;; We have a key with no value.
	    output)
	  ;; Else, this element must be the value of a key/value pair.
	  (begin
	    (set! output (assoc-set! output key cur))
	    (loop #f (car rest) (cdr rest) output))))
     (else
      ;; We have a "MESSAGE" key, so the remaining params are a format
      ;; specifier and its parameters.
      ;;(pk 'key key 'cur cur 'rest rest)
      (set! output (assoc-set! output key
			       (apply format (append (list #f)
						     (list cur)
						     rest))))
      output))))

(define (log-structured log-level . rest)
  "Log a message to structured data.  The message will be passed
through the log writer set by the application using
log-set-writer-func.  If the message is fatal, the program will be
aborted.

For each key to log, the procedure takes two parameters: a key
string and a value string.

There must always be a MESSAGE key, which must be the last entry in
the list.  For that key, the variables that follow the MESSAGE key
will be merged into a single string using (format #f param1 param2 ...)"
  ;;(pk 'log-structured 'log-domain log-domain 'log-level log-level 'rest rest)
  (let ((fields-alist (acons "PRIORITY" (log-level->priority log-level) '())))
    (set! fields-alist (%add-fields-from-list-to-alist fields-alist rest))
    (send-alist-to-logger fields-alist)))

(define-public *console-port*
  (make-custom-binary-output-port "console"
				  console-write-bytevector
				  #f #f #f))

(setvbuf *console-port* 'none)

(define-public (Format message . args)
  "Writes an information icon and the MESSAGE to a new line on
developer console.  MESSAGE can contains '~A' and '~S' escapes.  When
printed, the escapes are replaced with the corresponding number of
ARGS.  '~A' is a human-friendly representation of the argument.  '~S'
is a more machine-friendly representation."
  (let ((str (apply simple-format (append (list #f message) args))))
    (display str *console-port*)))

(define-public (watch . stuff)
  "Writes the arguments passed to it to the developer console,
returning the last argument.  This is useful for tracing function
calls.  For example, you could replace a function call like

  (function arg)
  with
  (pk \"function returns\" (function arg))"
  (let ((stack (make-stack #t 2)))
    (debug-peek-append
     (object->string (car stuff))
     (with-output-to-string
       (lambda ()
	 (pretty-print
	  (car (last-pair stuff)))))
     (call-with-output-string
       (lambda (port)
	 (display-backtrace stack port 0 4))))))
