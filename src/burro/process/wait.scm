(define-module (burro process wait)
  #:use-module (burro process base)
  #:export (wait-process))

;; A wait process.

;; It just waits.  It is useful only if you want to delay before you
;; start another process.

(define (wait-process-on-update self delta-seconds)
  (base-process-on-update self delta-seconds)
  (when (get-active-flag self)
    (var-add! self 'start delta-seconds)
    (when (>= (var-ref self 'start) (var-ref self 'stop))
      (process-kill! self))))


(define (wait-process seconds)
  (let ((self (make-base-process)))
    (set-name! self "wait")
    (set-type! self PROC_WAIT)
    (var-set! self 'start 0)
    (var-set! self 'stop seconds)
    (set-on-update-func! self wait-process-on-update)
    self))
  
