FUNCTION FFT_FREQUENCIES, dx, nx

  nK = FINDGEN((nx - 1)/2) + 1
  is_N_even = (nx MOD 2) EQ 0
  IF (is_N_even) THEN k = [0.0, nK, nx/2, -nx/2 + nK]/(nx*dx) $
  ELSE                k = [0.0, nK, -(nx/2 + 1) + nK]/(nx*dx)
  
  RETURN, k
END

;+
; NAME:        FOURIER_1D
;
; AUTHOR:      A. Mignone (mignone@to.infn.it)
;
; PURPOSE:     Compute the fast Fourier transform of a 1D array or a
;              mutldimensional array in a specific direction.
;              Return the complex FFT along with the frequency array
;              sorted in ascending order.
;
; SYNTAX:      FOURIER_1D, Q, xi, Qk, kxi[,/POWER][,DIMENSION=DIMENSION]
;
; ARGUMENTS:
;
;   Q  [in]     A 1, 2 or 3D array to which the Fast Fourier Transform should
;               be applied.
;   xi  [in]    The coordinate array (e.g. time or space) in the
;               direction across the FFT is desired.  
;   Qk [out]    The output FFT array with dimensions identical to
;               those of the input array.
;   kxi [out]   The frequency array (output) corresponding to xi.
;
; KEYWORDS:
;
;   DIMENSION   A scalar indicating the dimension across which to calculate
;               the FFT.
;
;   POWER      When enabled, the output array Qk will contain the normalized
;              power.
;-
PRO FOURIER_1D, Q, xi, Qk, kxi, POWER = POWER, DIMENSION = DIMENSION

  Q    = REFORM(Q)
  sq   = SIZE(Q)
  dimQ = sq[0];         Spatial dimenions of Q
  nxi  = N_ELEMENTS(xi); Number of elements in xi

  IF (NOT KEYWORD_SET(DIMENSION)) THEN dimension = 0

; -----------------------------------------------
; 1a. For 2D or 3D arrays, DIMENSION cannot
;     be zero.
; -----------------------------------------------
  IF (dimQ GT 1 AND DIMENSION EQ 0) THEN BEGIN
    PRINT, "! FOURIER_1D: Error: unknown DIMENSION for a multi-D array"
    RETURN
  ENDIF

; -----------------------------------------------
; 1b. Check that the dimension of the coordinate
;     array match the array dimension.
; -----------------------------------------------
  IF (KEYWORD_SET(DIMENSIONS) AND nxi NE sQ[DIMENSION]) THEN BEGIN
    PRINT,"! FOURIER_1D: Error: coordinate array not correct."
    RETURN
  ENDIF

; -----------------------------------------------
; 2. Take Fourier transform of Q across "dimension".
;    Q can be a 1, 2 or 3D array.
;    x is the space or time coordinate in the
;    direction across which the FFT is taken.
; -----------------------------------------------
  Qk = FFT(Q, -1, DIMENSION=dimension, /DOUBLE)

  dxi = xi[1] - xi[0];  Assume uniform grid spacing
  kxi = FFT_FREQUENCIES(dxi, nxi)

; -----------------------------------------------
; 3. Sort wavenumber (and corresponding FFT
;    array) in ascending order.
; -----------------------------------------------

  sortxi= SORT(kxi)

; -----------------------------------------------
; 3a. If Q is a 1D array, sort.
; -----------------------------------------------
  IF (dimQ EQ 1) THEN BEGIN
    Qk  = Qk[sortxi]
    kxi = kxi[sortxi]
  ENDIF

; -----------------------------------------------
; 3b. If Q is a 2D array, sort the corresponding
;     dimension only.
; -----------------------------------------------
  IF (dimQ EQ 2) THEN BEGIN
    IF (dimension EQ 1) THEN BEGIN
      FOR j = 0, nxi-1 DO Qk[*,j] = Qk[sortxi,j]
      kxi = kxi[sortxi]
    ENDIF

    IF (dimension EQ 2) THEN BEGIN
      FOR i = 0, nxi-1 DO Qk[i,*] = Qk[i, sortxi]
      kxi   = kxi[sortxi]
    ENDIF

  ENDIF

; -----------------------------------------------
; 4. Compute power if /POWER keyword is detected
; -----------------------------------------------
  IF (KEYWORD_SET(power)) THEN BEGIN
    dkxi = kxi[1] - kxi[0]
    IF (KEYWORD_SET(dim)) THEN BEGIN
      Qk   = ABS(Qk)^2/TOTAL(ABS(Qk)^2*dkxi, dim)
    ENDIF ELSE BEGIN
      Qk   = ABS(Qk)^2/TOTAL(ABS(Qk)^2*dkxi)
    ENDELSE
  ENDIF

  RETURN
END


;+
; NAME:        FOURIER_2D
;
; AUTHOR:      A. Mignone (mignone@to.infn.it)
;
; PURPOSE:     Compute the fast Fourier transform of a 2D array.
;              Return the 2D complex FFT along with the frequency arrays
;              sorted in ascending order.
;
; SYNTAX:      FOURIER_2D, Q, x, y, Qk, kx, ky[, /POWER]
;
; ARGUMENTS:
;
;   Q  [in]     The 2D array to which the Fast Fourier Transform should
;               be applied.
;   x  [in]     The coordinate array (e.g. time or space) relative to 1st dim.
;   y  [in]     The coordinate array (e.g. time or space) relative to 2nd dim.
;   Qk [out]   ÊThe output FFT array with dimensions identical to
;               those of the input array.
;   kx [out]    The frequency array (output) for the 1st dim.
;   ky [out]    The frequency array (output) for the 2nd dim.
;
; KEYWORDS:
;
;   POWER      When enabled, the output array Qk will contain the normalized
;              power.
;-
PRO FOURIER_2D, Q, x, y, Qk, kx, ky, power=power

  Q  = REFORM(Q)
  sq = SIZE(Q)

  ; -----------------------------------------------
  ; 1. Compute FFT and freq. arrays
  ; -----------------------------------------------
  Qk = FFT(Q, -1, /DOUBLE);    Fourier transform Q
  nx = N_ELEMENTS(x)
  ny = N_ELEMENTS(y)
  
  dx = x[1] - x[0];  Assume uniform grid spacing
  dy = y[1] - y[0];  Assume uniform grid spacing
  kx = FFT_FREQUENCIES(dx, nx)
  ky = FFT_FREQUENCIES(dy, ny)
  
  ; -----------------------------------------------
  ; 2. Sort Arrays
  ; -----------------------------------------------
  sort_i = SORT(kx)
  sort_j = SORT(ky)
  
  FOR i = 0, nx-1 DO Qk[i,*] = Qk[i, sort_j]
  FOR j = 0, ny-1 DO Qk[*,j] = Qk[sort_i,j]
    
  kx = kx[sort_i]
  ky = ky[sort_j]
  
  ; -----------------------------------------------
  ; 3. Compute power if /POWER keyword is detected
  ; -----------------------------------------------
  IF (KEYWORD_SET(power)) THEN BEGIN
    dkx = kx[1] - kx[0]
    dky = ky[1] - ky[0]
    Qk  = ABS(Qk)^2/TOTAL(ABS(Qk)^2*dkx*dky)
  ENDIF

  RETURN
END

;+
; NAME:        FOURIER_3D
;
; AUTHOR:      A. Mignone (mignone@to.infn.it)
;
; PURPOSE:     Compute the fast Fourier transform of a 2D array.
;              Return the 2D complex FFT along with the frequency arrays
;              sorted in ascending order.
;
; SYNTAX:      FOURIER_3D, Q, x, y, z, Qk, kx, ky, kz[, /POWER]
;
; ARGUMENTS:
;
;   Q  [in]     The 2D array to which the Fast Fourier Transform should
;               be applied.
;   x  [in]     The coordinate array (e.g. time or space) relative to 1st dim.
;   y  [in]     The coordinate array (e.g. time or space) relative to 2nd dim.
;   z  [in]     The coordinate array (e.g. time or space) relative to 3rd dim.
;   Qk [out]   ÊThe output FFT array with dimensions identical to
;               those of the input array.
;   kx [out]    The frequency array (output) for the 1st dim.
;   ky [out]    The frequency array (output) for the 2nd dim.
;   kz [out]    The frequency array (output) for the 3rd dim.
;
; KEYWORDS:
;
;   POWER      When enabled, the output array Qk will contain the normalized
;              power.
;-
PRO FOURIER_3D, Q, x, y, z, Qk, kx, ky, kz, power=power

  Q  = REFORM(Q)
  sq = SIZE(Q)

  ; -----------------------------------------------
  ; 1. Compute FFT and freq. arrays
  ; -----------------------------------------------
  Qk = FFT(Q, -1, /DOUBLE);    Fourier transform Q
  nx = N_ELEMENTS(x)
  ny = N_ELEMENTS(y)
  nz = N_ELEMENTS(z)
  
  dx = x[1] - x[0];  Assume uniform grid spacing
  dy = y[1] - y[0];  Assume uniform grid spacing
  dz = z[1] - z[0];  Assume uniform grid spacing
  kx = FFT_FREQUENCIES(dx, nx)
  ky = FFT_FREQUENCIES(dy, ny)
  kz = FFT_FREQUENCIES(dz, nz)
  
  ; -----------------------------------------------
  ; 2. Sort Arrays
  ; -----------------------------------------------
  sort_i = SORT(kx)
  sort_j = SORT(ky)
  sort_k = SORT(kz)
  
  FOR i = 0, nx-1 DO FOR j = 0, ny-1 DO Qk[i,j,*] = Qk[i, j, sort_k]
  FOR i = 0, nx-1 DO FOR k = 0, nz-1 DO Qk[i,*,k] = Qk[i, sort_j, k]
  FOR j = 0, ny-1 DO FOR k = 0, nz-1 DO Qk[*,j,k] = Qk[sort_i, j, k]
    
  kx = kx[sort_i]
  ky = ky[sort_j]
  kz = kz[sort_k]
  
  ; -----------------------------------------------
  ; 3. Compute power if /POWER keyword is detected
  ; -----------------------------------------------
  IF (KEYWORD_SET(power)) THEN BEGIN
    dkx = kx[1] - kx[0]
    dky = ky[1] - ky[0]
    dkz = kz[1] - kz[0]
    Qk  = ABS(Qk)^2
    Qk  = Qk/TOTAL(Qk*dkx*dky*dkz)
  ENDIF

  RETURN
END

;+
; NAME:        FOURIER
;
; AUTHOR:      A. Mignone (mignone@to.infn.it)
;
; PURPOSE:     Compute the fast Fourier transform of a 1- or 2D multidimensional
;              array.
;              Return the complex FFT along with the frequency array(s).
;              This function is actually a wrapper for FOURIER_1D or FOURIER_2D.
;
; SYNTAX:      FOURIER, Q, x, Qk, kx[, /POWER][DIMENSION=DIMENSION]   (in 1D)
;
;                or
;
;              FOURIER, Q, x, y, Qk, kx,ky[, /POWER]                  (in 2D)
;              
;
; ARGUMENTS:
;
;   Q  [in]     The 1D or 2D array to which the Fast Fourier Transform should
;               be applied.
;   x  [in]     The coordinate array (e.g. time or space) relative to 1st dim.
;   y  [in]     The coordinate array (e.g. time or space) relative to 2nd dim.
;   Qk [out]    The output FFT array with dimensions identical to
;               those of the input array.
;   kx [out]    The frequency array (output) for the 1st dim.
;   ky [out]    The frequency array (output) for the 2nd dim.
;
;
; KEYWORDS:
;
;   DIM        A scalar indicating the dimension across which to calculate
;              the FFT.
;   POWER      When enabled, the output array Qk will contain the normalized
;              power.
;
; EXAMPLES:
;
;   IDL> Nx = 2048
;   IDL> x  = 0.0 + DINDGEN(Nx)/(Nx-1.0); Generate a uniform grid in [0,1] 
;   IDL> twopi = 2.0*!PI*x
;   IDL> ; Build simple array with three frequencies
;   IDL> Q  = 5.0*sin(twopi) + 2.0*cos(10.0*twopi) + cos(30.0*twopi)
;   IDL> FOURIER, Q, x, Qk, k, /POWER
;   IDL> PLOT, k, pwr, psym=-6,xrange=[-64,64], xstyle=1
;
;
; LAST MODIFIED:   March 29, 2019 by A. Mignone
;
;-
PRO FOURIER, a1, a2, a3, a4, a5, a6, a7, a8, power=power, dimension=dimension

  IF (N_PARAMS() EQ 4) THEN BEGIN
  ; ---------------------------------------------
  ; For FOURIER_1D,
  ; {a1, a2, a3, a4} = {Q, x, Qk, kx} 
  ; ---------------------------------------------
    FOURIER_1D, a1, a2, a3, a4, power=power, dimension=dimension
    RETURN
  ENDIF 

  IF (N_PARAMS() EQ 6) THEN BEGIN
  ; ---------------------------------------------
  ; For FOURIER_2D,
  ; {a1, a2, a3, a4, a5, a6} = {Q, x, y, Qk, kx, ky} 
  ; ---------------------------------------------
    FOURIER_2D, a1, a2, a3, a4, a5, a6, power=power
    RETURN
  ENDIF 

  IF (N_PARAMS() EQ 8) THEN BEGIN
  ; ---------------------------------------------
  ; For FOURIER_3D,
  ; {a1, a2, a3, a4, a5, a6, a7, a7} = {Q, x, y, z, Qk, kx, ky, kz} 
  ; ---------------------------------------------
    FOURIER_3D, a1, a2, a3, a4, a5, a6, a7, a8, power=power
    RETURN
  ENDIF 

  PRINT,"> FOURIER: invalid number of parameters"  
END










PRO TEST_FOURIER_1D
  ; -----------------------------------------------
  ; Generate a uniform grid in [0,1] and
  ; build a simple array with 3 frequencies
  ; -----------------------------------------------
  Nx = 2048
  x  = 0.0 + DINDGEN(Nx)/(Nx-1.0)
  dx = x[1] - x[0];
  twopi = 2.0*!PI*x
  Q     = 1 +5.0*sin(twopi) + 2.0*cos(10.0*twopi) + cos(30.0*twopi)

  ; -----------------------------------------------
  ; Take FFT and compute frequencies
  ; -----------------------------------------------
  Qk = FFT(Q,-1, /DOUBLE)

  nk = FINDGEN((Nx - 1)/2) + 1
  is_N_even = (Nx MOD 2) EQ 0
  IF (is_N_even) THEN k = [0.0, nk, Nx/2, -Nx/2 + nk]/(Nx*dx) $
  else                k = [0.0, nk, -(Nx/2 + 1) + nk]/(Nx*dx)
  dk = k[1] - k[0]

  ; -----------------------------------------------
  ; Verify Parseval Theorem
  ; -----------------------------------------------
  PRINT,"Totals = ",TOTAL(ABS(Qk)^2*dk), TOTAL(ABS(Q)^2*dx)

  ; -----------------------------------------------
  ; Plot
  ; -----------------------------------------------
  pwn = ABS(Qk)^2/TOTAL(ABS(Qk)^2*dk)  
  PLOT, k, pwn , psym=2, xrange=[-40,40], xstyle=1

  ; -----------------------------------------------
  ; Redo same thing....
  ; -----------------------------------------------
  FOURIER, Q, x, pwr, kx, /POWER
  OPLOT, kx, pwr, psym=-6

END


PRO TEST_FOURIER_2D
; -----------------------------------------------
; Generate two uniform grid2 in [0,1] and
; build a simple 2D array
; -----------------------------------------------
Nx = 2048
x  = 0.0 + DINDGEN(Nx)/(Nx-1.0)
y  = 0.0 + DINDGEN(Nx)/(Nx-1.0)
dx = x[1] - x[0];
dy = y[1] - y[0];
twopi = 2.0*!PI*x
Qx     = 5.0*sin(twopi) + 2.0*cos(10.0*twopi) + cos(30.0*twopi)
Qy     = sin(twopi + 60)

Q = Qx#Qy

; -----------------------------------------------
; Take Fourier Transform
; -----------------------------------------------
FOURIER, Q, y, Qk, ky, DIM=2
dk = ky[1] - ky[0]
pwr = ABS(Qk)^2/TOTAL(ABS(Qk)^2*dk)
PLOT, ky,pwr[Nx/4,*],psym=-6, xrange=[-20,20]

STOP

;FOURIER, Q, x, y, Qk, kx, ky
;dkx  = kx[1] - kx[0]
;dky  = ky[1] - ky[0]
;pwr = ABS(Qk)^2/TOTAL(ABS(Qk)^2*dkx*dky)

FOURIER, Q, x, y, pwr, kx, ky, /POWER
DISPLAY, ALOG10(pwr), x1=kx,x2=ky, xrange=[-100,100],$
                                   yrange=[-100,100]

END



