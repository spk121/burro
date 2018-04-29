;; burro.scm

;; Copyright (C) 2018   Michael L. Gran
;; This file is part of Burro Engine

;; Burro Engine is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; Burro Engine is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with Burro Engine.  If not, see <http://www.gnu.org/licenses/>.

(define-module (burro)
  #:use-module (burro drivers)
  #:use-module (burro engine)
  #:use-module (burro colors)
  #:use-module (burro error)
  #:use-module (burro debug)
  #:use-module (ice-9 sandbox)
  #:re-export (clickable-text
	       timed-text
	       wait-on-click-text)
  #:export (make-sandbox
	    load-file-into-sandbox
	    eval-string-in-sandbox
	    call-with-limits))

(set! *random-state* (random-state-from-platform))

(define sandbox-bindings
  '(((guile)
     random
     random:normal)

    ((burro debug)
     log-info
     Format)

    ((burro colors)
     color)

    ((burro drivers)
     clickable-text
     timed-text
     wait-on-click-text)

    ((burro pm)
     pm-info)
    ((burro engine)
     ;; From Burro
     set-title
     debug-peek-append
     stop
     ;; receive-clock-tick
     ;; register-game-loop-handler
     ;; From Canvas
     canvas-width
     canvas-height
     set-brightness
     get-brightness
     set-colorswap
     get-colorswap
     set-blank
     get-blank
     set-backdrop
     get-backdrop
     set-markup
     set-markup-bgcolor
     set-font
     set-font-color
     VRAM_A
     VRAM_B
     VRAM_C
     VRAM_D
     VRAM_E
     VRAM_F
     VRAM_G
     VRAM_H
     VRAM_I
     VRAM_J
     get-vram-type
     get-vram-filename
     get-vram-image-size
     vram-get-u32-size
     load-image-file
     load-audio-file
     audio-channel-play
     audio-channel-pause
     audio-channel-unpause
     audio-channel-stop
     audio-channel-playing?
     audio-channel-set-volume
     audio-channel-get-volume
     audio-pause
     audio-unpause
     audio-set-volume
     audio-get-volume
     BG_1 BG_2 BG_3 BG_4
     bg-setup
     bg-get-setup
     bg-set-position
     bg-get-position
     bg-reset
     bg-hide
     bg-show
     bg-shown?
     bg-get-colorswap
     bg-set-colorswap
     bg-set-brightness
     bg-set-brightness
     )))

(define (make-sandbox)
  (make-sandbox-module
   (append all-pure-and-impure-bindings
	   sandbox-bindings)))

(define *last-line-loaded* 0)
(define *last-expr-loaded* '())

(define (load-from-port-into-sandbox port sandbox)
  (set! *last-line-loaded* 0)
  (set! *last-expr-loaded* '())
  (let loop ((expr (read port)))
    (cond
     ((not (eof-object? expr))
      (when (integer? (port-line port))
	(set! *last-line-loaded* (port-line port)))
      (when expr
	(set! *last-expr-loaded* expr))
      (eval-in-sandbox expr #:module sandbox #:time-limit 5.0 #:sever-module? #f)
      (loop (read port)))
     (else
      sandbox))))

(define (sandbox-load-error-handler key . args)
  "This handler tries to return a nicely formatted error string for
errors that happen when reading a file into the sandbox."
  (let ((exception-msg (call-with-output-string
			 (lambda (port)
			   (print-exception+ port #f key args)))))
    (string-append
     exception-msg
     "\nin expression\n"
     (object->string *last-expr-loaded*)
     "\nnear line "
     (number->string *last-line-loaded*))))

#|
(define (default-error-handler key . args)
  "This handler tries to return a nicely formatted error string for
general errors."
  (call-with-output-string
    (lambda (port)
      (cond
       ((eqv? key 'limit-exceeded)
	(let ((subr (car args))
	      (msg (cadr args))
	      (msgargs (caddr args)))
	  (if subr
	      (format port "In procedure ~a: "subr))
	  (apply format port msg (or args '()))))
       (else
	(print-exception port #f key args))))))
|#

(define (load-file-into-sandbox filename)
  "Read expressions from a file and evaluate them in SANDBOX.  If an
error occurs, a string is returned with a description of the problem."
  (let ((port (string-if-exception (open-input-file filename))))
    (set-port-encoding! port "UTF-8")
    (if (string? port)
	port
	;; else
	(catch #t
	  (lambda ()
	    (let ((sandbox (make-sandbox)))
	      (load-from-port-into-sandbox port sandbox)
	      sandbox))
	  sandbox-load-error-handler))))

(define (eval-string-in-sandbox str sandbox)
  "Evaluates a string in a sandbox. If an error occurs, a string is
returned with a description of the problem."
  (string-if-exception
   (call-with-input-string str
     (lambda (port)
       (set-port-filename! port "console")
       (let loop ((expr (read port))
		  (result #f))
	 (cond
	  ((not (eof-object? expr))
	   (let ((val (eval-in-sandbox expr #:module sandbox #:sever-module? #f)))
	     (loop (read port)
		   val)))
	  (else
	   (object->string result))))))))

(define (call-with-limits proc . args)
  "Evaluates a procedure using the time and allocation limits
from the sandbox.  On exception, returns a string describing
the error."
  (string-if-exception
   (call-with-time-and-allocation-limits 0.1 #e10e6
					(lambda ()
					  (apply proc args)))))
