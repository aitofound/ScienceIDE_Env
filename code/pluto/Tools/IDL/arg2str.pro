; ***************************************************************
;   String format conversion routine
; ***************************************************************
FUNCTION ARG2STR, x, format=format

  szx = SIZE(x)

  IF (szx[1] EQ 4 OR szx[1] EQ 5) THEN BEGIN  ; arguments is either double 
    sgnx = x GT 0.0 ? 1.0:-1.0                ; or float
    n    = 0
    IF (ABS(x) GT 1.e-7) THEN n = FLOOR(ALOG10(ABS(x)))

    IF (n GT 3) THEN BEGIN
      y = x/10.0^n
      IF (NOT KEYWORD_SET(format)) THEN FORMAT = '(i8)'
      sn = STRCOMPRESS(STRING(n,format=format),/remove_all)
      sx = STRING(y,format='(f18.3)')
      sx += " x 10^"+sn
    ENDIF ELSE BEGIN
      IF (NOT KEYWORD_SET(format)) THEN FORMAT = '(f18.3)'
      sx = STRING(x,format=format)
    ENDELSE
    RETURN,strcompress(sx,/remove_all)
  ENDIF ELSE BEGIN
    IF (NOT KEYWORD_SET(format)) THEN FORMAT='(I4)'
    RETURN, STRCOMPRESS(STRING(x, FORMAT=format),/remove_all)
  ENDELSE

END


