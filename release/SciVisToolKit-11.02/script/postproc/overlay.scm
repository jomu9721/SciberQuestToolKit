
(define
  (overlay inFile overlayFile outFile x0 y0)
    (let*
      (
      ; load background the im
      (im (car (gimp-file-load RUN-NONINTERACTIVE inFile inFile)))
      (dw (car (gimp-image-get-active-layer im)))
      ; load the overlay im
      (la (car (gimp-file-load-layer RUN-NONINTERACTIVE im overlayFile)))
      )

      ; debug
      ; (gimp-message-set-handler ERROR-CONSOLE)
      ; (gimp-message inFile)
      ; (gimp-message overlayFile)
      ; (gimp-message outFile)
      ; (gimp-message x0)
      ; (gimp-message y0)

      ; position the overlay
      (gimp-image-add-layer im la -1)
      (gimp-layer-set-offsets la (string->number x0) (string->number y0))

      ; save the composite
      (gimp-image-merge-visible-layers im  CLIP-TO-IMAGE)
      (set! dw (car (gimp-image-get-active-layer im)))

      (gimp-file-save RUN-NONINTERACTIVE im dw outFile outFile)
      (gimp-image-delete im)
   )
)
