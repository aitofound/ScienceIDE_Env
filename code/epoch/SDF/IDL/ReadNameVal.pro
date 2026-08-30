;
; SDF (Self-Describing Format) IDL reader
; Copyright (c) 2010-2016, SDF Development Team
;
; Distributed under the terms of the BSD 3-clause License.
; See the LICENSE file for details.
;

FUNCTION parse_name_val, str, delim=delim, include_strings=include_strings
  ; Parse a line like abc = xyz into a name-value pair
  ; Attempt to infer type of value (string, long or double)
  ; delim is the assignment character
  ; include_strings is flag to include values that are strings

  IF (N_ELEMENTS(delim) EQ 0) THEN delim = '='
  IF (N_ELEMENTS(include_strings) EQ 0) THEN include_strings = 0
  IF (STRPOS(str, delim) EQ -1) THEN RETURN, !NULL
  name_val = STRTRIM(strsplit(str, delim, /EXTRACT), 2)

  IF ((SIZE(name_val))[1] GE 2) THEN BEGIN
    ; We have two fields, try making the second into number
    ; Default is double, but if no decimal point, make long
    ; Valid number contains digits 0-9, decimal point, exponential character
    ; and plus/minus signs
    ; IDL escaping on \- seems wonky, interpreting as a range unless this
    ; comes last
    IF ((STREGEX(name_val[1], '^([0-9\.deDE\+\-]+)$', /EXTRACT))[0] EQ '') $
          THEN BEGIN
      IF (include_strings) THEN BEGIN
        RETURN, LIST(name_val[0]+'_s', name_val[1])
      ENDIF ELSE BEGIN
        RETURN, !NULL
      ENDELSE
    ENDIF

    IF (STRPOS(name_val[1], '.') EQ -1 && STREGEX(str, '([deDE]+)') EQ -1) $
          THEN BEGIN
      val = LONG(name_val[1])
    ENDIF ELSE BEGIN
      val = DOUBLE(name_val[1])
    ENDELSE

    RETURN, LIST(name_val[0], val)
    ; Return first value as string and second as correct type
  ENDIF ELSE BEGIN
    RETURN, !NULL
  ENDELSE
END


FUNCTION ReadNameVal, dir=dir, pref=pref, file=file
  ; Read from deck.status into a struct of the available constants
  ; Extracts all strings which are 'name = value'
  COMPILE_OPT IDL2

  ; Force long ints and proper brackets
  IF (N_ELEMENTS(pref) EQ 0) THEN pref = ''

  IF (N_ELEMENTS(file) GT 0) THEN BEGIN
    name = file
  ENDIF ELSE BEGIN
    name = pref + 'const.status'
  ENDELSE

  IF (N_ELEMENTS(dir) EQ 0) THEN BEGIN
    filename = get_wkdir() + '/' + name
  ENDIF ELSE BEGIN
    filename = dir[0] + '/' + name
  ENDELSE

  IF (~FILE_TEST(filename)) THEN BEGIN
    PRINT, 'File ' + filename + ' not found'
    RETURN, !NULL
  END

  OPENR, filenum, filename, /GET_LUN

  str = ''
  const = 0
  WHILE (~EOF(filenum)) DO BEGIN
    READF, filenum, str
    nv = parse_name_val(str)
    IF (~ISA(nv, 'LIST')) THEN CONTINUE
    IF ((SIZE(nv))[1] LT 2) THEN CONTINUE

    IF (ISA(const, 'struct')) THEN BEGIN
      ind = WHERE(TAG_NAMES(const) EQ STRUPCASE(nv[0]))
      IF (ind EQ -1) THEN BEGIN
        const = CREATE_STRUCT(const, nv[0], nv[1])
      ENDIF ELSE BEGIN
        ; Following overwrites, so that last occurence is kept
        ; For constants, the last thing in the file is the summary block
        const.(ind) = nv[1]
      ENDELSE
    ENDIF ELSE BEGIN
      const = CREATE_STRUCT(nv[0], nv[1])
    ENDELSE
  ENDWHILE

  const = CREATE_STRUCT(const, 'file', filename)
  FREE_LUN, filenum

  RETURN, const
END


FUNCTION ReadDeckAll, dir=dir, pref=pref, file=file, $
                      include_strings=include_strings
  ; Read all deck specs into struct
  ; This includes attempting to find all the 'Element ... handled OK' lines
  ; include_strings is 0 by default; if set to 1 then string RHS's are included
  COMPILE_OPT IDL2

  ; Force long ints and proper brackets
  IF (N_ELEMENTS(pref) EQ 0) THEN pref = ''

  IF (N_ELEMENTS(file) GT 0) THEN BEGIN
    name = file
  ENDIF ELSE BEGIN
    name = pref + 'const.status'
  ENDELSE

  IF(N_ELEMENTS(dir) EQ 0) THEN BEGIN
    filename = get_wkdir() + '/' + name
  ENDIF ELSE BEGIN
    filename = dir[0] + '/' + name
  ENDELSE

  IF(N_ELEMENTS(include_strings) EQ 0) THEN include_strings = 0

  IF(~FILE_TEST(filename)) THEN BEGIN
    PRINT, 'File ' + filename + ' not found'
    RETURN, !NULL
  END

  OPENR, filenum, filename, /GET_LUN

  str = ''
  deck_specs = 0
  WHILE (~EOF(filenum)) DO BEGIN
    READF, filenum, str
    IF (~STRMATCH(str, '*=*')) THEN CONTINUE
    ;IF(STRMATCH(str, '*Element*handled OK')) THEN CONTINUE
    match = STREGEX(str, 'Element (.*) handled OK', /EXTRACT, /SUBEXPR)
    IF (match[0] NE '') THEN str = match[1]

    nv = parse_name_val(str, include_strings=include_strings)
    IF (~ ISA(nv, 'LIST')) THEN CONTINUE
    IF ((SIZE(nv))[1] LT 2) THEN CONTINUE
    IF (ISA(deck_specs, 'struct')) THEN BEGIN
      IF (WHERE(TAG_NAMES(deck_specs) EQ STRUPCASE(nv[0])) EQ -1) THEN BEGIN
        deck_specs = CREATE_STRUCT(deck_specs, nv[0], nv[1])
      ENDIF
    ENDIF ELSE BEGIN
      deck_specs = CREATE_STRUCT(nv[0], nv[1])
    ENDELSE
  ENDWHILE
  deck_specs = CREATE_STRUCT(deck_specs, 'file', filename)

  FREE_LUN, filenum
  RETURN, deck_specs
END
