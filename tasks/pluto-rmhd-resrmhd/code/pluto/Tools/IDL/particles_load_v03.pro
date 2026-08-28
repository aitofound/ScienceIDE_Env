;+
;
; NAME:      PARTICLES_BINARY_LOAD
;
; AUTHOR:    Andrea Mignone
;
; PURPOSE:
;
;   Read PLUTO particle file in .flt or .dbl binary data format. 
;   Called from PARTICLES_LOAD with the /FLOAT (or no) keywords.
;   
; SYNTAX:
;
;   PARTICLES_BINARY_LOAD, fname[, /FLOAT][, /SILENT], endianity, err
;   
; ARGUMENTS:
;
;   fname      The name of a valid .flt or .dbl particle file.
;
; KEYWORDS:
;
;
; EXAMPLES:
;
;  * Example #1: Load the 10-th particles output file:
;
;    IDL> PARTICLES_LOAD, 10
;
;
; LAST MODIFIED:
;
;  May 15, 2018 by D. Mukherjee & A. Mignone (dipanjan.mukherjee@unito.it)
;
;-
PRO PARTICLES_BINARY_LOAD, fname, FLOAT=FLOAT, SLICE=SLICE, SILENT=SILENT, err, TOTFILE=totfile

  COMMON PLUTO_GRID
  COMMON PLUTO_VAR
  COMMON PLUTO_RUN


; --------------------------------------------
; 0. Read header file section
; --------------------------------------------

  OPENR, unit, fname, /GET_LUN, ERROR = err
  IF (err NE 0) THEN BEGIN
    PRINT,"! Cannot open file "+fname
    err = 1
    RETURN
  ENDIF

  header = ''
  READF,unit,header
  nfields = 0
  s = STRMID(header,0,1); Get first character of the string
  totfile = 1 ; Initialise to 1 (minimum)
  
  WHILE (s EQ '#') DO BEGIN ;  Loop on header strings
    POINT_LUN, -unit, pos ;  Get current file position

    substr = STRSPLIT(header,/EXTRACT,count=count)
    
  ; -- Extract number of particles --
    IF (substr[1] EQ "nparticles") THEN nparticles = ULONG64(substr[2])

  ; -- Extract number of particles --
    IF (substr[1] EQ "Totparticles") THEN Totparticles = ULONG64(substr[2])

  ; -- Extract number of files --
    IF (substr[1] EQ "nfiles") THEN totfile = FIX(substr[2])

  ; -- Extract number of fields --
    IF (substr[1] EQ "nfields") THEN nfields = FIX(substr[2]) 

  ; -- Extract field names --
    IF (nfields GT 0 AND substr[1] EQ "field_names") THEN BEGIN
      field_names = substr[2:*]
    ENDIF
 
 ; -- Extract field dimension --
    IF (nfields GT 0 AND substr[1] EQ "field_dim") THEN BEGIN
      field_dim = FIX(substr[2:*])
    ENDIF
    
  ; -- Extract endianity --
    IF (substr[1] EQ "endianity") THEN endianity = substr[2]

    IF (~ EOF(unit)) THEN BEGIN ;--if no particle in domain, EOF after header
    READF,unit,header     ;  Read next string
    s = STRMID(header,0,1);  Get first character
    ENDIF ELSE s='0'
  ENDWHILE

  POINT_LUN,unit,pos;  Set file pointer to beginning of binary section
    
; --------------------------------------------
; 1. Print some info
; --------------------------------------------
  IF (NOT KEYWORD_SET(SILENT)) THEN BEGIN
    PRINT,"> Binary data file: ",fname
    IF ( KEYWORD_SET (FLOAT) OR KEYWORD_SET (SLICE)) THEN BEGIN
      PRINT,"> Precision:    float"
    ENDIF ELSE BEGIN
      PRINT,"> Precision:    double"
    ENDELSE
    PRINT,"> nfields:      ", ARG2STR(nfields)
    PRINT,"> Field_names:  ", field_names  
    PRINT,"> Endianity:    ", endianity
    PRINT,"> nparticles:   ", ARG2STR(nparticles)
    IF (NOT KEYWORD_SET(SLICE)) THEN BEGIN
;       PRINT,"> Totparticles: ", ARG2STR(Totparticles)
       PRINT,"> nfiles:       ", ARG2STR(totfile)
    ENDIF
  ENDIF

; --------------------------------------------------------
; 2. Create Paricle structure and allocate memory
; --------------------------------------------------------
  IF (n_elements(field_names) eq 0) THEN BEGIN
    PRINT,"Empty field names. Returning"
    RETURN
  ENDIF

  IF (nparticles eq 0) THEN BEGIN
    PRINT,"No particles in domain."
    RETURN
  ENDIF
   
; ---------------------------------------------------------
; 2a. Create the first template structure
;     Then loop over fields to append and build
; ---------------------------------------------------------

  struct1 = CREATE_STRUCT(field_names[0],0.0)     
  IF (KEYWORD_SET(FLOAT) OR KEYWORD_SET(SLICE)) THEN BEGIN
    FOR nf=1,nfields-1 DO BEGIN
      IF (field_dim[nf] eq 1) THEN BEGIN
        struct1 = CREATE_STRUCT(TEMPORARY(struct1),field_names[nf],0.0)
      ENDIF ELSE BEGIN
        struct1 = CREATE_STRUCT(TEMPORARY(struct1),field_names[nf],$
                                FLTARR(field_dim[nf]))
      ENDELSE
    ENDFOR
  ENDIF ELSE BEGIN 
    FOR nf=1,nfields-1 DO BEGIN
      IF (field_dim[nf] eq 1) THEN BEGIN
        struct1 = CREATE_STRUCT(TEMPORARY(struct1),field_names[nf],0.0D0)
      ENDIF ELSE BEGIN
        struct1 = CREATE_STRUCT(TEMPORARY(struct1),field_names[nf],$
                                DBLARR(field_dim[nf]))
      ENDELSE
    ENDFOR
  ENDELSE ;----IF FLOAT/DOUBLE

 
  IF (KEYWORD_SET(FLOAT)) THEN BEGIN
     struct1  = CREATE_STRUCT(name='particle_struct_flt',TEMPORARY(struct1)) 
  ENDIF ELSE IF (KEYWORD_SET(SLICE)) THEN BEGIN 
     struct1  = CREATE_STRUCT(name='particle_struct_slice',TEMPORARY(struct1)) 
  ENDIF ELSE BEGIN
     struct1  = CREATE_STRUCT(name='particle_struct_dbl',TEMPORARY(struct1)) 
  ENDELSE    
 
  particles = 0.0;  Free memory (?)
  IF (KEYWORD_SET(FLOAT)) THEN BEGIN
      particles = REPLICATE({particle_struct_flt},nparticles)
  ENDIF ELSE IF (KEYWORD_SET(SLICE)) THEN BEGIN
      particles = REPLICATE({particle_struct_slice},nparticles)
  ENDIF ELSE BEGIN
      particles = REPLICATE({particle_struct_dbl},nparticles)
  ENDELSE

; ---------------------------------------------------------------
; 3. Read data (using assoc):
; ---------------------------------------------------------------
  nelem = TOTAL(field_dim)

  POINT_LUN,-unit,pos
  IF (KEYWORD_SET(FLOAT) OR KEYWORD_SET(SLICE)) THEN BEGIN
    Q = ASSOC(unit,FLTARR(nelem,nparticles),pos)
  ENDIF ELSE BEGIN
    Q = ASSOC(unit,DBLARR(nelem,nparticles),pos)
  ENDELSE

  ; -- Swap endianity and close file --

  Q = Q[0]
  IF (endianity EQ "little") THEN BEGIN
    Q = SWAP_ENDIAN (Q, /SWAP_IF_BIG_ENDIAN)
  ENDIF ELSE BEGIN v
    Q = SWAP_ENDIAN (Q, /SWAP_IF_LITTLE_ENDIAN)
  ENDELSE

; ---------------------------------------------------------------
; 4. Assign array value to structure members.
;
; ---------------------------------------------------------------
  iend = -1
  FOR nf = 0,nfields-1 DO BEGIN
    ibeg = iend + 1
    iend = ibeg + field_dim[nf] - 1 

    IF (field_names[nf] EQ "id")        THEN particles.id  = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "x1")        THEN particles.x1  = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "x2")        THEN particles.x2  = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "x3")        THEN particles.x3  = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "vx1")       THEN particles.vx1 = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "vx2")       THEN particles.vx2 = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "vx3")       THEN particles.vx3 = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "tinj")      THEN particles.tinj = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "density")   THEN particles.density = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "pressure")  THEN particles.pressure = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "bx1")       THEN particles.bx1 = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "bx2")       THEN particles.bx2 = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "bx3")       THEN particles.bx3 = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "nmicro")    THEN particles.nmicro = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "cmp_ratio") THEN particles.cmp_ratio = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "shkflag")   THEN particles.shkflag = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "shk_gradp") THEN particles.shk_gradp = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "ca")        THEN particles.ca = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "cr")        THEN particles.cr = REFORM(Q[ibeg:iend,*]); 
    IF (field_names[nf] EQ "vL")        THEN particles.vL[0:field_dim[nf]-1] = REFORM(Q[ibeg:iend,*]);
    IF (field_names[nf] EQ "vR")        THEN particles.vR[0:field_dim[nf]-1] = REFORM(Q[ibeg:iend,*]);
    IF (field_names[nf] EQ "eng")       THEN particles.eng[0:field_dim[nf]-1] = REFORM(Q[ibeg:iend,*]);
    IF (field_names[nf] EQ "chi")       THEN particles.chi[0:field_dim[nf]-1] = REFORM(Q[ibeg:iend,*]);

    ; Color can be 1 element or multiple, hence accounted for differently
    IF (field_names[nf] EQ "color") THEN BEGIN  
                   IF (field_dim[nf] GT 1) THEN BEGIN
                      particles.color[0:field_dim[nf]-1] = REFORM(Q[ibeg:iend,*]); 
                   ENDIF ELSE BEGIN
                      particles.color = REFORM(Q[ibeg:iend,*]);
                   ENDELSE
    ENDIF 

  ENDFOR

  CLOSE,unit
  FREE_LUN,unit
  err = 0
END

;+
;
; NAME:      PARTICLES_VTK_LOAD
;
; AUTHOR:    Andrea Mignone
;
; PURPOSE:
;
;   Read PLUTO particles file in .vtk data format. 
;   Called from PARTICLES_LOAD with the /VTK keyword.
;   
; SYNTAX:
;
;   PARTICLES_VTKLOAD, fname
;   
; ARGUMENTS:
;
;   fname      The name of a valid .vtk paticle file.
;
; KEYWORDS:
;
;
; EXAMPLES:
;
;  * Example #1: Load the 10-th particles output file:
;
;    IDL> PARTICLES_LOAD, /VTK,10
;
;
; LAST MODIFIED:
;
;  April 6, 2018 by D. Mukherjee (dipanjan.mukherjee@unito.it)
;
;-
PRO PARTICLES_VTK_LOAD, fname, SILENT=SILENT, err

  COMMON PLUTO_GRID
  COMMON PLUTO_VAR
  COMMON PLUTO_RUN

  T0 = SYSTIME(1)
    
; ---------------------------------------------------------
; 0. Define Particle structure here.
;    The main particle structure contains 11 fields placed
;    in the following order:
;
;    id ,  x1,  x2,  x3, vx1, vx2, vx3,  tinj, color
;    (0), (1), (2), (3), (4), (5), (6),   (7),  (8)
; ---------------------------------------------------------

  struct1 = {particle_struct_vtk, id: 0.d0, x1:  0.d0,  x2: 0.d0, x3: 0.d0,    $
                                       vx1: 0.d0, vx2: 0.d0, vx3: 0.d0,  $
                                       tinj:0.d0, color:0.d0}

  PRINT,"> Reading vtk data format"

; ---------------------------------------------------------
; 1. Open file for reading
; ---------------------------------------------------------

  OPENR, unit, fname, /GET_LUN, ERROR = err, /SWAP_IF_LITTLE_ENDIAN
  IF (err NE 0) THEN BEGIN
    PRINT,"! Cannot open file "+fname
    err = 1
    RETURN
  ENDIF
  endianity = "big"
  
; ---------------------------------------------------------
; 2. Start reading data until end of file
; ---------------------------------------------------------

  WHILE ~ EOF(unit) DO BEGIN
    sline = ''
    READF, unit, sline
    words = STRSPLIT(sline," ", /EXTRACT)
    IF (SIZE(words,/N_ELEMENT) LE 1) THEN CONTINUE
    
    ; -- Read particles positions -- 
    
    IF (words[0] EQ "POINTS") THEN BEGIN
      nparticles = ULONG64(words[1])
      particles = 0.0;
      IF (nparticles EQ 0) THEN RETURN
      particles = REPLICATE({particle_struct_vtk},nparticles)

      POINT_LUN, -unit, offset; obtain the current position in the file
      Q     = ASSOC(unit,FLTARR(3,nparticles),offset)
      Q     = Q[0]
      particles.(1) = REFORM(Q[0,*]);   particles x1-coordinate
      particles.(2) = REFORM(Q[1,*]);   particles x2-coordinate
      particles.(3) = REFORM(Q[2,*]);   particles x3-coordinate
    ENDIF

    ; -- Read particles identity --

    IF (words[0] EQ "SCALARS" AND words[1] EQ "Identity") THEN BEGIN
      READF,unit,sline;         skip next line ("LOOKUP_TABLE")
      POINT_LUN, -unit, offset; obtain the current position in the file
      Q     = ASSOC(unit,LONARR(nparticles),offset)
      Q     = Q[0]
      particles.(0) = REFORM(Q[*]);   particles identity
    ENDIF

    ; -- Read particle velocity --

    IF (words[0] EQ "VECTORS" AND words[1] EQ "Velocity") THEN BEGIN
      POINT_LUN, -unit, offset; obtain the current position in the file
      Q     = ASSOC(unit,FLTARR(3, nparticles),offset)
      Q     = Q[0]
      particles.(4) = REFORM(Q[0,*]);   particles x1 velocity
      particles.(5) = REFORM(Q[1,*]);   particles x2 velocity
      particles.(6) = REFORM(Q[2,*]);   particles x3 velocity
    ENDIF

    ; -- Read particle Four-velocity --

    IF (words[0] EQ "VECTORS" AND words[1] EQ "Four-Velocity") THEN BEGIN
      POINT_LUN, -unit, offset; obtain the current position in the file
      Q     = ASSOC(unit,FLTARR(3, nparticles),offset)
      Q     = Q[0]
      particles.(4) = REFORM(Q[0,*]);   particles x1 four-velocity
      particles.(5) = REFORM(Q[1,*]);   particles x2 four-velocity
      particles.(6) = REFORM(Q[2,*]);   particles x3 four-velocity
    ENDIF

    ; -- Read particles injection time --

    IF (words[0] EQ "SCALARS" AND words[1] EQ "tinj") THEN BEGIN
      READF,unit,sline;         skip next line ("LOOKUP_TABLE")
      POINT_LUN, -unit, offset; obtain the current position in the file
      Q     = ASSOC(unit,FLTARR(nparticles),offset)
      Q     = Q[0]
      particles.(7) = REFORM(Q[*]);   particles injection time
    ENDIF

    ; -- Read particles color --

    IF (words[0] EQ "SCALARS" AND words[1] EQ "Color") THEN BEGIN
      READF,unit,sline;         skip next line ("LOOKUP_TABLE")
      POINT_LUN, -unit, offset; obtain the current position in the file
      Q     = ASSOC(unit,FLTARR(nparticles),offset)
      Q     = Q[0]
      particles.(8) = REFORM(Q[*]);   particles color
    ENDIF

  ENDWHILE

; ---------------------------------------------------------
; 3. Print some info
; ---------------------------------------------------------

  IF (NOT KEYWORD_SET(SILENT)) THEN BEGIN
    PRINT,"> Binary data file: ",fname
    IF (KEYWORD_SET (FLOAT)) THEN BEGIN
      PRINT,"> Precision: float"
    ENDIF ELSE BEGIN
      PRINT,"> Precision: double"
    ENDELSE
    PRINT,"> Endianity:  ",endianity
    PRINT,"> nparticles: ", ARG2STR(nparticles)
  ENDIF

  CLOSE,unit
  FREE_LUN,unit
  err = 0
END

;+
;
; NAME:      PARTICLES_LOAD
;
; AUTHOR:    Andrea Mignone
;
; PURPOSE:  
;
;   Read PLUTO particles file in .dbl, .flt and .vtk format.
;   By default, if neither /VTK or /FLOAT are given, double precision 
;   raw-binary files are read (particles.nnnn.dbl). 
;   
;   Data is read and stored in the 'particle' array of structures which has
;   variables size depending on the actual number of particles (nparticles).
;   Both particle and nparticles can be shared among different routines using
;   the PLUTO_VAR common block.    
;
; SYNTAX:
;
;   PARTICLES_LOAD, nout[, DIR=DIR][, SILENT=SILENT][, FLOAT=FLOAT][, VTK = VTK]
;
; ARGUMENTS:
;
;   nout  = the output number, e.g., 0,1,2, ...
;   nfile = the particle sub-file for a given nout. e.g. nfile=1 if particles.0001.dbl.01 
;           is to be loaded. nfile=0 by default and may be omitted for particles.0001.dbl.00
;
; KEYWORDS:
;
;   DIR        the directory name where output files are located (e.g. 
;              DIR = 'Data1/'). Default is current directory.
;
;   /SILENT    set this keyword to suppress printing information
;
;   /FLOAT     set this keyword to read single precision binary datafile
;              (particles.nnnn.flt). 
;              
;   /VTK       set this keyword to read .vtk particle files.
;              In this case, the PARTICLES_VTKLOAD procedure is invoked.
;
;   /REDUCED   Assume particle data files have been written with
;              the PARTCILES_REDUCE_FLT switch turned to YES.
;              In this case, the particle structure will contain
;              fewer fields.
;
;   /SL    OR 
;   /SLICE      Loads a particle structure sliced in a plane for a 
;               customised output.
;               Assume input in float.
;
;
;   /ALL        For a multi-file particle output, loads all particles
;               from all files sequentially and stores in a single 
;               particle structure. 
;               NOTE: For large files can consume significant memory.
;
;
; EXAMPLES:
;
;  * Example #1: Load the 10-th particles output file in single precision
;                and print the x2-component of velocity of the 4-th particle:
;
;    IDL> PARTICLES_LOAd,10,/FLOAT      ;--Loads particles from particles.0010.flt.00
;    IDL> PRINT,particles[4].vx2
;
;    IDL> PARTICLES_LOAd,10,1,/FLOAT    ;--Loads particles from particles.0010.flt.01
;    IDL> PARTICLES_LOAd,10,/FLOAT,/ALL ;--Loads particles from all particles.0010.flt.*
;
; LAST MODIFIED:
;
;  October 19, 2018 by D. Mukherjee (dipanjan.mukherjee@unito.it)
;
;-
PRO PARTICLES_LOAD, nout, nfile, dir=dir, SILENT=SILENT, FLOAT=FLOAT, FLT=FLT, VTK=VTK,$
                    REDUCED=REDUCED, ALL=ALL, SLICE=SLICE, SL=SL
  COMMON PLUTO_GRID
  COMMON PLUTO_VAR
  COMMON PLUTO_RUN

  T0 = SYSTIME(1)
  IF (NOT KEYWORD_SET(DIR)) THEN dir = "."


; ---------------------------------------------------------
; 1. Build file name & call corresponding
;    loader
; ---------------------------------------------------------

  IF (NOT KEYWORD_SET(nfile) OR KEYWORD_SET(ALL)) THEN nfile = 0

  str_nout = STRCOMPRESS(STRING(nout,FORMAT='(I4.4)'))
  str_nf   = STRCOMPRESS(STRING(nfile,FORMAT='(I02)'))
  fname  = dir+"/particles."+str_nout

  IF (KEYWORD_SET(FLOAT) OR KEYWORD_SET(FLT)) THEN BEGIN
    fname += "_"+str_nf+".flt"
    PARTICLES_BINARY_LOAD,fname,/FLOAT,SILENT=SILENT, err, totfile=totfile
  ENDIF ELSE IF (KEYWORD_SET(SLICE) OR KEYWORD_SET(SL)) THEN BEGIN
    fname += ".slice"
    PARTICLES_BINARY_LOAD,fname,/SLICE,SILENT=SILENT, err
  ENDIF ELSE IF (KEYWORD_SET(VTK)) THEN BEGIN
    PARTICLES_VTK_LOAD,fname+".vtk",SILENT=SILENT, err
  ENDIF ELSE BEGIN
;    fname += "_"+str_nf+".dbl"
    fname += ".dbl"
    PARTICLES_BINARY_LOAD,fname,SILENT=SILENT, err, totfile=totfile
  ENDELSE  


   IF (~KEYWORD_SET(SILENT)) THEN BEGIN
      PRINT,"> Data in structure: particles "
   ENDIF
 
  IF (err NE 0) THEN RETURN

;  PRINT,"Function took ",SYSTIME(1)-T0, "seconds"


  IF (KEYWORD_SET(ALL)) THEN BEGIN
  dum = particles ; Temporary dummy struct

     FOR nf = 1, totfile-1 DO BEGIN ; Loop over all other files
        str_nout = STRCOMPRESS(STRING(nout,FORMAT='(I4.4)'))
        str_nf   = STRCOMPRESS(STRING(nf,FORMAT='(I02)'))
        fname  = dir+"/particles."+str_nout

        IF (KEYWORD_SET(FLOAT) OR KEYWORD_SET(FLT)) THEN BEGIN
           fname += ".flt."+str_nf
           PARTICLES_BINARY_LOAD,fname,/FLOAT,SILENT=SILENT, err
        ENDIF ELSE BEGIN
           fname += ".dbl."+str_nf
           PARTICLES_BINARY_LOAD,fname,SILENT=SILENT, err
        ENDELSE  
        dum = [dum,particles] ;---concatenate
    ENDFOR

  particles = dum
  dum = 0 ;---free dummy struct
  ENDIF

  PRINT, "> Used memory:", $
    STRCOMPRESS(STRING(MEMORY(/CURRENT,/L64)/1024.d0/1024.d0))," Mb"

END
