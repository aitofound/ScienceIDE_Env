;+
; NAME:       PARTICLES_BOX
;
; AUTHOR:     DIPANJAN MUKHEJRE
;
; PURPOSE:    Returns a structure with particles located within a region of space.
;             The extent of the box is defined in pos[xyz] arrays. Each pos# array
;              is a 2D array of beginning and end position of box.
;
; Parameters:
; parts        [IN]   Input particle structure
; posx         [IN]   A 2D array of beginning and end of coordinates of the box for
;                     the x1 coordinate.
; posy         [IN]   Same as posx but for x2. Optional. If not present the entire range of x2 is taken.
; posz         [IN]   Same as posx but for x3. Optional. If not present the entire range of x2 is taken.
;
; Returns sub-structure with particles bounded within the box.
;
; Usage: 
;       > PARTICLES_BOX, particles, posy=[0.1,1] 
;                     ---- will return particles in the range: x1=[min(x1),max(x1)], x2=[0.1,1], x3=[min(x3),max(x3)]
;
;       > PARTICLES_BOX, particles, pos=[1,2], posy=[2,4]
;                     ---- will return particles in the range: x1=[1,2], x2=[2,4], x3=[min(x3),max(x3)]
;
;-

 FUNCTION PARTICLES_BOX, parts, posx=posx, posy=posy, posz=posz

  IF (KEYWORD_SET(posx) AND (n_elements(posx) NE 2)) THEN BEGIN
     PRINT,'! posy should have 2 elements. Abort!'
     STOP
  ENDIF 

  IF (NOT KEYWORD_SET(posx)) THEN BEGIN
     posx = fltarr(2)
     posx[0] = min(parts.x1,max=maxx1)
     posx[1] = maxx1
  ENDIF

  IF (KEYWORD_SET(posy) AND (n_elements(posy) NE 2)) THEN BEGIN
     PRINT,'! posy should have 2 elements. Abort!'
     STOP
  ENDIF 

  IF (NOT KEYWORD_SET(posy)) THEN BEGIN
     posy = fltarr(2)
     posy[0] = min(parts.x2,max=maxx2)
     posy[1] = maxx2
  ENDIF

  IF (KEYWORD_SET(posz) AND (n_elements(posz) NE 2)) THEN BEGIN
     PRINT,'! posz should have 2 elements. Abort!'
     STOP
  ENDIF
 
  IF (NOT KEYWORD_SET(posz)) THEN BEGIN
     posz = fltarr(2)
     posz[0] = min(parts.x3,max=maxx3)
     posz[1] = maxx3
  ENDIF

  idx = WHERE(     (parts.x1 ge posx[0]) AND (parts.x1 le posx[1]) $
               AND (parts.x2 ge posy[0]) AND (parts.x2 le posy[1]) $
               AND (parts.x3 ge posz[0]) AND (parts.x3 le posz[1]) )

  RETURN, parts[idx]

 END

