;+
; NAME:   MIRROR
; 
; AUTHOR:   A. Mignone
;
; PURPOSE:  Build a symmetric image out of a 2D or 2D array A.
;
; SYNTAX:  MIRROR, a[, X1BEG=x1beg][, X1END=x1end]
;                   [, X2BEG=x2beg][, X2END=x2end]
;                   [, X3BEG=x3beg][, X3END=x3end]
;
; ARGUMENTS:  
;
;   a   = 2D or 3D array
;
; KEYWORDS
;
;  One keyword between X1BEG,... X3END should be given with values equal to
;
;   1  = reflect the image w/ respect to the axis
;  -1  = reflect the image w/ respect to the axis and change sign
;
; LAST MODIFIED:   Jan 17, 2025 by A. Mignone (andrea.mignone@unito.it)
;
;-
FUNCTION MIRROR, A, x1beg=x1beg, x2beg=x2beg, x3beg=x3beg,$
                    x1end=x1end, x2end=x2end, x3end=x3end

  ; -----------------------------------
  ; 0. Determine array size & type
  ; -----------------------------------

  nx   = 1
  ny   = 1
  nz   = 1

  ss   = size(A)
  ndim = ss[0]
  IF (ndim GE 1) THEN nx   = ss[1]
  IF (ndim GE 2) THEN ny   = ss[2]
  IF (ndim EQ 3) THEN nz   = ss[3]

  type = SIZE(A, /type); array type (4 = float, 5 = double, etc...)

  ; -----------------------------------
  ; X1.beg 
  ; -----------------------------------

  IF (KEYWORD_SET(X1BEG)) THEN BEGIN
    ng    = x1beg/ABS(x1beg)
    x1beg = ABS(x1beg)

    IF (ndim EQ 1) THEN BEGIN
      scrh = MAKE_ARRAY(2*nx, type=type)
      scrh[nx:*]   = A[*]
      scrh[0:nx-1] = ng*REVERSE(A[*],1)    
    ENDIF

    IF (ndim EQ 2) THEN BEGIN
      scrh = MAKE_ARRAY(2*nx, ny, type=type)
      scrh[nx:*,*]   = A[*,*]
      scrh[0:nx-1,*] = ng*REVERSE(A[*,*],1)    
    ENDIF

    IF (ndim EQ 3) THEN BEGIN
      scrh = MAKE_ARRAY(2*nx, ny, nz, type=type)
      scrh[nx:*,*,*]  = A[*,*,*]
      scrh[0:nx-1,*,*] = ng*REVERSE(A[*,*,*],1)
    ENDIF
  ENDIF

  ; -----------------------------------
  ; X1END
  ; -----------------------------------

  IF (KEYWORD_SET(X1END)) THEN BEGIN
    ng    = x1end/ABS(x1end)
    x1end = ABS(x1end)

    IF (ndim EQ 1) THEN BEGIN
      scrh           = MAKE_ARRAY(2*nx, type=type)
      scrh[nx:*]   = ng*REVERSE(A[*],1)
      scrh[0:nx-1] = A[*]
    ENDIF

    IF (ndim EQ 2) THEN BEGIN
      scrh           = MAKE_ARRAY(2*nx, ny, type=type)
      scrh[nx:*,*]   = ng*REVERSE(A[*,*],1)
      scrh[0:nx-1,*] = A[*,*]
    ENDIF

    IF (ndim EQ 3) THEN BEGIN
      scrh             = MAKE_ARRAY(2*nx, ny, nz, type=type)
      scrh[nx:*,*,*]   = ng*REVERSE(A[*,*,*],1)
      scrh[0:nx-1,*,*] = A[*,*,*]
    ENDIF
  ENDIF

  ; -----------------------------------
  ; X2BEG
  ; -----------------------------------

  IF (KEYWORD_SET(X2BEG)) THEN BEGIN
    ng    = x2beg/ABS(x2beg)
    x2beg = ABS(x2beg)

    IF (ndim EQ 1) THEN BEGIN
      scrh = MAKE_ARRAY(2*ny, type=type)
      scrh[ny:*]   = A[*]
      scrh[0:ny-1] = ng*REVERSE(A[*],2)    
    ENDIF

    IF (ndim EQ 2) THEN BEGIN
      scrh = MAKE_ARRAY(nx, 2*ny, type=type)
      scrh[*,ny:*]   = A[*,*]
      scrh[*,0:ny-1] = ng*REVERSE(A[*,*],2)    
    ENDIF

    IF (ndim EQ 3) THEN BEGIN
      scrh = MAKE_ARRAY(nx, 2*ny, nz, type=type)
      scrh[*,ny:*,*]   = A[*,*,*]
      scrh[*,0:ny-1,*] = ng*REVERSE(A[*,*,*],2)    
    ENDIF
  ENDIF

  ; -----------------------------------
  ; X2END
  ; -----------------------------------

  IF (KEYWORD_SET(X2END)) THEN BEGIN
    ng    = x2end/ABS(x2end)
    x2end = ABS(x2end)

    IF (ndim EQ 1) THEN BEGIN
      scrh           = MAKE_ARRAY(2*ny, type=type)
      scrh[ny:*]   = ng*REVERSE(A[*],2)
      scrh[0:ny-1] = A[*]
    ENDIF

    IF (ndim EQ 2) THEN BEGIN
      scrh           = MAKE_ARRAY(nx, 2*ny, type=type)
      scrh[*,ny:*]   = ng*REVERSE(A[*,*],2)
      scrh[*,0:ny-1] = A[*,*]
    ENDIF

    IF (ndim EQ 3) THEN BEGIN
      scrh           = MAKE_ARRAY(nx, 2*ny, nz, type=type)
      scrh[*,ny:*,*]   = ng*REVERSE(A[*,*,*],2)
      scrh[*,0:ny-1,*] = A[*,*,*]
    ENDIF
  ENDIF

  ; -----------------------------------
  ; X3BEG
  ; -----------------------------------

  IF (KEYWORD_SET(X3BEG)) THEN BEGIN
    ng    = x3beg/ABS(x3beg)
    x3beg = ABS(x3beg)

    IF (ndim EQ 3) THEN BEGIN
      scrh = MAKE_ARRAY(nx, ny, 2*nz, type=type)
      scrh[*,*,nz:*]   = A[*,*,*]
      scrh[*,*,0:nz-1] = ng*REVERSE(A[*,*,*],3)    
    ENDIF
  ENDIF

  ; -----------------------------------
  ; X3END 
  ; -----------------------------------

  IF (KEYWORD_SET(X3END)) THEN BEGIN
    ng    = x3end/ABS(x3end)
    x3end = ABS(x3end)

    IF (ndim EQ 3) THEN BEGIN
      scrh             = MAKE_ARRAY(nx, ny, 2*nz, type=type)
      scrh[*,*,nz:*]   = ng*REVERSE(A[*,*,*],3)
      scrh[*,*,0:nz-1] = A[*,*,*]
    ENDIF
  ENDIF

  RETURN, scrh
END
