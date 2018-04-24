(define-module (burro process wait-on-click)
  #:use-module (burro process base)
  #:export (wait-on-click-process))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; The wait-on-click process

;; This waits for the user to click anywhere
;; the screen.

(define (on-update self delta-seconds)
  (base-process-on-update self delta-seconds)
  (when (get-active-flag self)
	(let ((location (var-ref self 'mouse-click)))
	  (when location
		(process-kill! self)))))

(define (wait-on-click-process)
  (let ((self (make-base-process)))
    (set-name! self "wait-on-click")
    (set-type! self PROC_CONTROL)
    (set-process-flags! self PROCESS_FLAG_MOUSE_CLICK)
    (set-on-update-func! self on-update)
    self))
