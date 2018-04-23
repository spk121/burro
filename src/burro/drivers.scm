(define-module (burro drivers)
  #:use-module (burro engine)
  #:use-module (burro pm)
  #:use-module (burro process base)
  #:use-module (burro process call-procedure)
  #:use-module (burro process fade)
  #:use-module (burro process text-click)
  #:use-module (burro process wait)
  #:use-module (burro xml)
  #:use-module (srfi srfi-1)
  #:use-module (sxml simple)
  #:export (clickable-text
	    timed-text))

(define (find-action actions index)
  "Searches in the action list for an entry that is active for index."
  (if (null? actions)
      #f
      ;; The actions list should have the form
      ;; ((start-index end-index thunk) ...)
      (let ((action-entry
	     (find (lambda (entry)
		     (and (<= (first entry) index)
			  (< index (second entry))))
		   actions)))
	(if action-entry
	    (third action-entry)
	    #f))))

(define (make-button-press-handler actions)
  "Creates a procedure that can be used as the button press callback
handler.  When called, it tries to convert x,y locations into
character locations.  If those character locations have associated
actions, they are activated."
  (lambda (time x y)
    ;; (format #t "BUTTON PRESS HANDLER ~s ~s ~s~%" time x y)
    (let ((index (position-to-string-index x y)))
      (when index
	(let ((action-thunk (find-action actions index)))
	  (when action-thunk
	    (action-thunk)))))))



;; FIXME: This should set process so that
;; - blanks the screen
;; - sets the text
;; - fades in
;; - listens for mouse
;; - fades out
;; - calls next handler
(define* (clickable-text burro-sxml-tree-inner #:key
			x y width height
			(fade-time 0.5))

  ;; We let the caller drop the uninteresting *TOP* node
  (let ((burro-sxml-tree
	 `(*TOP* ,burro-sxml-tree-inner)))
    (let ((actions (sxml-locate-actions burro-sxml-tree))
	  (pango-sxml-tree (sxml-style-actions burro-sxml-tree)))
      (let ((pango-xml-string
	     (with-output-to-string
	       (lambda () (sxml->xml pango-sxml-tree)))))
	;; Write the string to the screen
	(set-markup pango-xml-string x y width height)
	
	;; And now set up a script for the process manager.
	(let ((one (fade-in-process fade-time))
	      (two (text-click-process actions
				       "alice blue"
				       "cornflower blue"
				       0.5))
	      (three (fade-out-process fade-time)))
	  (process-set-next! one two)
	  (process-set-next! two three)
	  (pm-attach one))))))

(define* (timed-text burro-sxml-tree-inner next #:key
		     (time-limit 4.0) x y width height
		     (fade-time 0.5))
  ;; We let the caller drop the uninteresting *TOP* node
  (let ((pango-sxml-tree
	 `(*TOP* ,burro-sxml-tree-inner)))
    (let ((pango-xml-string
	   (with-output-to-string
	     (lambda () (sxml->xml pango-sxml-tree)))))
      (set-markup pango-xml-string x y width height)
	
      ;; And now set up a script for the process manager.
      (let ((one (fade-in-process fade-time))
	    (two (wait-process time-limit))
	    (three (fade-out-process fade-time))
	    (four (call-procedure-process next)))
	(process-set-next! one two)
	(process-set-next! two three)
	(process-set-next! three four)
	(pm-attach one)))))

					   
	  
	
