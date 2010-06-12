;
;   ____    _ __           ____               __    ____
;  / __/___(_) /  ___ ____/ __ \__ _____ ___ / /_  /  _/__  ____
; _\ \/ __/ / _ \/ -_) __/ /_/ / // / -_|_-</ __/ _/ // _ \/ __/
;/___/\__/_/_.__/\__/_/  \___\_\_,_/\__/___/\__/ /___/_//_/\__(_)
;
;Copyright 2008 SciberQuest Inc.
;
; Adds Axes to a plot
;
; Todo: 
;       1. make the ylabel 90degree orientation....DONE
;       2. Create the brush........................DONE
;       3. Make the fonts of the axes offset correcly
;          so that they don't intersect the axes...
;
;       4. 


;
; Temporary function that gets around a limitation of (my)
; gimp that was preventing using the script-fu variety 
; of this code.
(define (sciber-make-brush-rectangular name width height spacing )
  (let* ((img (car (gimp-image-new width height GRAY)))
         (drawable (car (gimp-layer-new img
                                        width height GRAY-IMAGE
                                        "MakeBrush" 100 NORMAL-MODE)))

         (filename (string-append gimp-directory
                                  "/brushes/r"
                                  (number->string width)
                                  "x"
                                  (number->string height)
                                  ".gbr")))
    (gimp-context-push)
    (gimp-image-undo-disable img)
    (gimp-image-add-layer img drawable 0)
    (gimp-context-set-background '(255 255 255))
    (gimp-drawable-fill drawable BACKGROUND-FILL)
    (gimp-rect-select img 0 0 width height CHANNEL-OP-REPLACE FALSE 0)
    (gimp-context-set-background '(0 0 0))
    (gimp-edit-fill drawable BACKGROUND-FILL)
    (file-gbr-save 1 img drawable filename "" spacing name)
    (gimp-image-delete img)

    (gimp-context-pop)

    (gimp-brushes-refresh)
    (gimp-context-set-brush name)
   );let
);define

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
; plugin::name= sciber-axes
; plugin::desc= 
;
; plugin::notes= 
;                1. The fontsize for the title is 2x the regular fontsize
;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
( define (sciber-axes inFile outFile x0 x1 y0 y1 nx ny xLabel yLabel title fontSize fontType )
  (let* (
         (frameIm (car (gimp-file-load RUN-NONINTERACTIVE inFile inFile)))
         (frameDw (car (gimp-image-get-active-layer frameIm)))
         (imwc (car (gimp-image-width frameIm)))
         (imhc (car (gimp-image-height frameIm)))
         (fpx (* imhc fontSize ))
         (lm 50 )
         (bm 50 ) 
         (rm 50 ) 
         (tm 50 )
         (th 50 )
         (tw 50 )
         (fpTitle  (* 2 fpx ))
         
         xTitle  yTitle
         yInitial xInitial
         xAxisOffset yAxisOffset
         delta
         increment counter
         xPosition  yPosition
         displayNumber
         delta increment 
         )

    (set! originalWidth (car (gimp-image-height  frameIm )))
    (set! originalHeight (car (gimp-image-width  frameIm )))
;    (gimp-message (string-append "Original width: " (number->string originalWidth )))
;    (gimp-message (string-append "Original Height is : " (number->string imhc )))
    (set! frameDw (car (gimp-image-get-active-layer frameIm )))

    (plug-in-autocrop RUN-NONINTERACTIVE frameIm frameDw)

    (set! fs  0.25) 
    (set! lm (ceiling (* fpx     (+ fs 2.0))))
    (set! tm (ceiling (* fpTitle (+ fs 1.2))))
    (set! bm (ceiling (* fpx     (+ fs 4 ))))


    (set! imhc (car (gimp-drawable-height frameDw)))
    (set! imwc (car (gimp-drawable-width frameDw)))
                                        ; resize
    (gimp-context-set-background '(255 255 255))
    (gimp-context-set-foreground '(  0   0   0))

    ; resize the image
;    (gimp-message (string-append "Resizing to "  (number->string (+ imwc lm rm))))
    (gimp-image-resize frameIm (+ imwc lm rm) (+ imhc tm bm) lm tm)
    (gimp-layer-resize frameDw (+ imwc lm rm) (+ imhc tm bm) lm tm)


    (set! imcx (+ lm (/ imwc 2)))
    (set! imcy (+ tm (/ imhc 2)))


    (set! newWidth  (gimp-image-width frameIm ))
    (set! newHeight (gimp-image-width frameIm ))
    
    
    (set! imwc (car (gimp-image-width  frameIm )))
    (set! imhc (car (gimp-image-height frameIm )))

;    (gimp-message (string-append "Imhc: " 
;                                 (number->string imhc)
;                                 " " 
;                                 " Tm " 
;                                 (number->string tm ))
;    )
                                 

;    (set! yInitial  (- imhc (+ tm bm) ))
    (set! yInitial ( - imhc bm ))
    (set! xInitial  lm )

    ; A tweaking option
    (set! xAxisDelta 0 )
    (set! xAxisOffset (- (floor (/ fpx 2 )) xAxisDelta ))
    (set! yAxisOffset (floor (/ lm 2 )))

;    (set! delta   (floor (/ (- imwc (+ lm rm )) (- nx  1))))
    (set! delta   (floor (/ (- imwc (+ lm rm )) (- nx  1))))
    (set! increment (/ (- (string->number x1) (string->number x0 )) (- nx 1 )))
    (set! counter 0 )
    (set! xPosition xInitial )
;    (gimp-message (string-append "New width: " (number->string imwc )
;                                 " "
;                                 "Delta: " (number->string delta )
;                                 " "
;                                 "Width: " (number->string (- imwc lm rm ))
;                                 " "
;                                 "Increment :" (number->string increment )
;                                 " "
;                                 "xInitial: " (number->string xInitial )
;                  );string-append
;     );
;    (gimp-message (string-append "yinitial: "
;                                 (number->string yInitial )
;                  )
;    )

    ; 
    ; Setup the Paint brush to draw the small lines
    ; 1.Change paint brush to small 
    ; This is temporary until its possible to use the 
    ; Built in brushes
    (sciber-make-brush-rectangular  "AxisBrush" 2.0 2.0 0.0 )

    ;
    ; XAxis
    ;
    (set! xAxisYValue (+ yInitial xAxisOffset))
;    (gimp-message (string-append "Values " (number->string xAxisYValue)))
;    (gimp-message (string-append "Axis should be " (number->string lm )))
    (while (< counter nx )
           (set! xPosition (+ xInitial (* counter delta )))
           (draw-line frameDw xPosition (- imhc bm ) xPosition (+ (- imhc bm ) 5 ) )
           (set! displayNumber  (+ (* counter increment ) (string->number x0 )))
;           (gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw xPosition xAxisYValue (number->string displayNumber) 0 TRUE fpx PIXELS "Nimbus Mono L Bold Oblique")))

           (gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw (- xPosition (/ fpx 2)) xAxisYValue (number->string displayNumber) 0 TRUE fpx PIXELS "Nimbus Mono L Bold Oblique")))

           ;
           ; Draw the ticks
           (set! counter (+ 1 counter ))
     );while

    ;
    ; Xlabel
    ;
;    (set! xLabelYPosition (- imhc (/ bm 2 )))
    (set! xLabelYPosition (- imhc (+ (/ bm 2 ) (/ fpx 2 ))))
    (set! xLabelXPosition (/ imwc 2 ))
;    (gimp-message (string-append "Putting in label at x= " 
;                                 (number->string xLabelXPosition )
;                                 "  y="
;                                 (number->string xLabelYPosition ) )
;     )

    (gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw xLabelXPosition xLabelYPosition xLabel 0 TRUE fpx PIXELS "Nimbus Mono L Bold Oblique")))

    ;
    ; Yaxis
    ;
    (set! delta   (floor (/ (- imhc (+ tm bm )) (- ny 1 ))))
    (set! increment (/ (- (string->number y1) (string->number y0 )) (- ny 1 )))
    (set! counter ny )
;    (set! yInitial (+ yInitial tm ))
    (set! yInitial (- imhc bm ))
    (set! yPosition yInitial )
    (set! yAxisXValue (floor (- xInitial yAxisOffset )))
;    (gimp-message (string-append "Yinitial value is : " (number->string yPosition )))

    (while (> counter 0 )
           (set! yPosition (- yInitial ( * ( - ny counter)  delta )))
           (draw-line frameDw (- lm 5) yPosition lm yPosition )
           (set! yPosition (- yPosition (/ fpx 2 )))

           (set! displayNumber  (+ (* (- ny counter ) increment ) (string->number y0 )))
           (gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw yAxisXValue (- yPosition (/ fpx 2 )) (number->string displayNumber) 0 TRUE fpx PIXELS "Nimbus Mono L Bold Oblique")))

           (set! counter (- counter 1 ))

     );while

;    (gimp-message (string-append "Final Y is : " (number->string yPosition )))
;           (gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw (floor (- xInitial yAxisOffset ))   yPosition (number->string displayNumber) 0 TRUE fpx PIXELS "Nimbus Mono L Bold Oblique")))


    ;
    ; Ylabel
    ;
    (set! yLabelYPosition (/ imhc 2 ))
    (set! yLabelXPosition (- (floor (/ lm 3 )) (* 2 fpx) ))
;(gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw yLabelXPosition yLabelYPosition yLabel 0 TRUE fpx PIXELS "Nimbus Mono L Bold Oblique")))
    (set! PI_2 (/ 3.14159 2 ))

;    (gimp-message (string-append "Putting in label at x= " 
;                                 (number->string yLabelXPosition )
;                                 "  y="
;                                 (number->string yLabelYPosition ) )
;     )


    (set! yLabelObj (car (gimp-text-fontname frameIm frameDw yLabelXPosition yLabelYPosition yLabel 0 TRUE fpx PIXELS "Nimbus Mono L Bold Oblique")))
    (set! textWidth  (car (gimp-drawable-width yLabelObj )))
    (set! textHeight (car (gimp-drawable-height yLabelObj )))

    ;
    ; Need to position the text to a better location
    ; Positioning seems to be difficult in terms of figuring out the 
    ; size of widths and lengths
    (gimp-drawable-transform-rotate-default yLabelObj (* -1 PI_2 )  1 ( / textWidth 2 ) (/ textHeight 2 ) 0 0)
    (gimp-floating-sel-anchor yLabelObj )

;    (sciber-make-brush-rectangular  "AxisBrush" 2.0 2.0 0.0 )

    (set! curLayer  )
    (draw-line frameDw xInitial (- imhc bm ) (- imwc rm ) (- imhc bm ))
    (set! points (cons-array 4 'double))
    (gimp-palette-set-foreground '(0 0 0 ))
    (draw-line frameDw xInitial yInitial xInitial tm )

    ;
    ; Title
    ;
    (set! xTitle (- imcx (* (/ (string-length title) 2.0) fpx)))
;    (set! yTitle (- (/ tm 2.0) (/ fpx 2.0))  )
    (set! yTitle 0 )
    (set! fpTitle  (* 2 fpx ) )
; (gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw  xTitle  yTitle title 0 TRUE fpTitle PIXELS "Nimbus Mono L Bold Oblique")))
; (gimp-message (string-append "Adding title at: x=" 
;                                 (number->string xTitle ) 
;                                 " y=" 
;                                 (number->string yTitle )
;                                 " and font= " 
;                                 (number->string fpx ))
;    );message
    
    (gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw xTitle yTitle title 0 TRUE fpTitle PIXELS "Nimbus Mono L Bold Oblique")))
           

;    (gimp-floating-sel-to-layer (car (gimp-text-fontname frameIm frameDw xTitle yTitle title 0 TRUE 100 PIXELS "Nimbus Mono L Bold Oblique")))

    (gimp-displays-flush )
    (gimp-file-save RUN-NONINTERACTIVE frameIm frameDw outFile outFile)
    );let*
);define


(script-fu-register "sciber-axes" 
                    "Axes"
                    "Automatically adds axes to an image "
                    "Jimi Damon <jdamon@gmail.com>"
                    "Jimi Damon"
                    "2010-06-08"
                    ""
                    SF-STRING "InputFile" "0"
                    SF-STRING "Output File" "0"
                    SF-STRING "xInitial (x0) as a string" "0"
                    SF-STRING "xFinal   (xF) as a string" "10"
                    SF-STRING "yInitial (y0) as a string" "0"
                    SF-STRING "yFinal   (yF) as a string" "0"
                    SF-STRING "Tile" "TITLE"
                    SF-VALUE "Xoffset for the yaxis tick marks" "10"
                    SF-VALUE "Yoffset for the xaxis tick marks" "10"
                    SF-VALUE "Font Size for Label as a %" "15"
                    SF-STRING "Font Type"  "San"
                    )
(script-fu-menu-register "sciber-crop" "<Toolbox>/Xtns/Script-Fu/SciberQuest" )


;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
; Helper functions
;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

(define (add-message message x y ) 
  (let*(()
        (gimp-floating-sel-anchor (car (gimp-text-fontname frameIm frameDw x y message 0 TRUE fpx PIXELS "Nimbus Mono L Bold Oblique")))
        )
    )
);define


(define (draw-line layer x0 y0 x1 y1 )
  (let* ((points)
         )
    (set! points (cons-array 4 'double))
    (aset points 0 x0 )
    (aset points 1 y0 )
    (aset points 2 x1 )
    (aset points 3 y1 )
    (gimp-palette-set-foreground '(0 0 0 ))
    (gimp-pencil layer 4 points )
    );let
);define


(define (ceiling  x ) 
  (+ x (- 1 (fmod x 1)))
);

(define (floor  x ) 
  (- x (fmod x 1))
);


(define (font-pixelsize-from-percent-size image )
  (2 )
);define


;(sciber-axes  "/home/jdamon/TestCase1.png" "/home/jdamon/foo2.png" "2" "10" "0" "10" 5 15  "xlabel" "ylabel" "NEW TITLE" 12 13  0.23 "San" )