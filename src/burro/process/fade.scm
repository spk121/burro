(define-module (burro process fade)
  #:use-module (burro engine)
  #:use-module (burro process base)
  #:export (fade-out-process
	    fade-in-process))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; The fadeout process

;; This fades the screen to black, or the inverse.

(define (fade-process-on-update p delta-milliseconds)
  (base-process-on-update p delta-milliseconds)
  (when (get-active-flag p)
    (var-add! p 'start (exact->inexact delta-milliseconds))
    (let ((start (var-ref p 'start))
	  (stop  (var-ref p 'stop)))
      ;; (pk "fade-process-on-update start/stop" (list start stop))
      (cond
       ((< start stop)
	(let ((ratio (/ (- stop start) stop)))
	  (if (not (var-ref p 'fadeout))
	      (set! ratio (+ (- ratio) 1.0)))
	  ;; (pk "ratio" ratio)
	  (set-brightness ratio)))
       (else
	(if (var-ref p 'fadeout)
	    (set-brightness 0.0)
	    (set-brightness 1.0))
	(process-kill! p))))))

(define (fade-process milliseconds fadeout?)
  "Fades the screen over MILLISCONDS of time.  If FADEOUT? is true, we
fade to black, if it is false, we do an inverse fade from black to
normal intensity."
  (let ((self (make-base-process)))
    ;; (pk "fadeprocess" self)
    (set-name! self
	       (if fadeout? "fade-out" "fade-in"))
    (set-type! self PROC_SCREEN)
    (var-set! self 'start 0)
    (var-set! self 'stop milliseconds)
    (var-set! self 'fadeout fadeout?)
    (set-on-update-func! self fade-process-on-update)
    self))

(define (fade-out-process milliseconds)
  (fade-process milliseconds #t))

(define (fade-in-process milliseconds)
  (fade-process milliseconds #f))
