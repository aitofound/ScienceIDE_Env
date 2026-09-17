;  Copyright (C) 2002 Regents of the University of Michigan, 
;  portions used with permission 
;  For more information, see http://csem.engin.umich.edu/tools/swmf
pro fits_to_ascii, FileIn, FileOut, silent=silent

; Purpose:
;  Read fits magnetogram file and write out an ASCII file.
;
; Usage:
;   fits_to_ascii [,FileIn] [, FileOut] [,/silent]
;
; FileIn  - name of the fits file. Default is fitsfile.fits
; FileOut - first part of the names of the output files. Default is fitsfile
;           so the files will be fitstfile.H, fitsfile_tec.dat, fitsfile.out

; /silent - suppress verbose information.

if n_elements(FileIn)  eq 0 then FileIn  = 'fitsfile.fits'
if n_elements(FileOut) eq 0 then FileOut = 'fitsfile'

print,FileIn
nMax=180

FileHeader= FileOut + '.H'
FileTec   = FileOut + '_tec.dat'
FileIdl   = FileOut + '.out' 
DataName  = 'Br [G]'  

Data = readfits(FileIn, ImHeader, StringHeader, LongShift, CRnumber, IsUniformLat, silent=silent)
;StringHeader is the Header with details to be output

if not keyword_set(silent) then begin
    print,''
    print,'Writing file with fitsfile header info: ',FileHeader
    print,''
 endif

openw,lun,FileHeader,/get_lun
printf,lun,ImHeader
free_lun, lun

; Get image dimensions
s=size(Data)
nLon=s(1)
nLat=s(2)

if not keyword_set(silent) then begin
    print,''
    print,'Writing TecPlot file for plotting Br: ',FileTec
    print,''
endif

openw, lun, FileTec, /get_lun
printf,lun,' TITLE="',FileIn,'"'
printf,lun,'VARIABLES = "',DataName,'"'
printf,lun,'ZONE T="',FileTec,'", I= ',nLon,' J= ',nLat,' , K=1, F=POINT'

for i=0L,nLat-1 do for j=0L,nLon-1 do $
  printf,lun, format = '(1e14.6)',Data(j,i)

free_lun, lun

if not keyword_set(silent) then begin
    print,''
    print,'Writing output file to be read by HARMONICS/FDIPS.exe: ', FileIdl
    print,''
endif

openw, lun, FileIdl, /get_lun
printf,lun,StringHeader
printf,lun, 0, 0.0, 2, 2, 1
printf,lun, nLon,' ',nLat
printf,lun, LongShift, CRnumber
printf,lun,'Longitude Latitude Br LongitudeShift CarringtonRotation'

; Cell Centered Coordinates
dLon = 360.0/nLon
for i=0L,nLat-1 do begin
   if(IsUniformLat eq 1)then begin
      Latitude =  (i + 0.5 - 90.)* (180./nLat)
   endif else begin
      Latitude =  asin((2*i-nLat+1.0)/nLat)/!dtor
   endelse
   for j=0L,nLon-1 do begin
      printf,lun,format ='(3e14.6)', (j+0.5)*dLon, Latitude, Data(j,i)
   endfor
endfor

free_lun,lun

if not keyword_set(silent) then print,'Conversion done'

end

