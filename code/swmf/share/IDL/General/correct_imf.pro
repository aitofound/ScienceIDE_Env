;  Copyright (C) 2002 Regents of the University of Michigan,
;  portions used with permission 
;  For more information, see http://csem.engin.umich.edu/tools/swmf
pro correct_imf,wIn,xIn,inputfile,outputfile,gsm=gsm,decay=decay

  common log_data, timeunit
  
; wIn contains the upstream data with 15 columns:
;    yr mo dy hr mn sc ms bx by bz ux uy uz rho T
; xIn should be set to the X position of the satellite 
;    relative to the inflow boundary in units of km !
; inputfile is the name of the original IMF file 
; outputfile is the name of the corrected IMF file
; if gsm=1 then convert from GSM to GSE before propagation and back afterward.
; if decay is set, find shocks, calculate shock normal from ux,uy,uz
;   change and use it for the propagation direction and time delay
;   Decay the normal back to -X direction exponentially with "decay" (minutes)

w = wIn
x = xIn

; Check arrays
siz = size(w)
if siz(0) ne 2 then begin
   print,'ERROR: w array should be 2D! Size(w)=',siz
   retall
endif
nPoint = siz(1)
nVar   = siz(2)

if nVar ne 15 then begin
   print,'ERROR: nVar (2nd dimension of w) is not 15! nVar=',nVar
   retall
endif

if n_elements(x) ne nPoint then begin
    print,'ERROR: w and x arrays have different sizes:',$
      nPoint, n_elements(x)
    retall
endif

; Calculate the time in seconds measured from the beginning of the day
iyr=0 & imo=1 & idy = 2 

timeunit='s'
Time = log_time(w,['year','mo','dy','hr','mn','sc','msc'])

; calculate epoch0 from initial year, month, day 
; number of msec from 01-Jan-0000 00:00:00.000
cdf_epoch, epoch0, w(0,iyr), w(0,imo), w(0,idy), /compute

if keyword_set(gsm) then begin

   ;; Add logtime converted to millisec
   epoch = epoch0 + Time*1d3

   b = transpose(w(*,7:9))
   gsm_gse, b, epoch
   w(*,7:9) = transpose(b)
   
   u = transpose(w(*,10:12))
   gsm_gse, u, epoch
   w(*,10:12) = transpose(u)
endif

iux = 10

; Calculate the simple time delay
TimeDelay = -x/w(*,iux)

; If decay is set, look for shocks and recalculate it
if keyword_set(decay) then begin
   ;; indexes used
   iuy  = iux + 1
   iuz  = iuy + 1
   irho = iuz + 1
   ;; initial value of (shock) normal vector
   normal = [-1., 0., 0.]
   for i = 1, nPoint-2 do begin
      ;; detect shocks: dux/dt < -10(km/s)/min and d(log rho)/dt > 0.1/min
      dt      = (Time(i+1) - Time(i-1))/60.0 ; in minutes
      dux     = w(i+1,iux) - w(i-1,iux)
      dlogrho = alog(w(i+1,irho)/w(i-1,irho))
      if dux/dt lt -10.0 and dlogrho/dt gt 0.1 then begin
         duy = w(i+1,iuy) - w(i-1,iuy)
         duz = w(i+1,iuz) - w(i-1,iuz)
         ;; transverse components must increase through an inclined shock
         if abs(w(i+1,iuy)) lt abs(w(i-1,iuy)) then duy = 0.0
         if abs(w(i+1,iuz)) lt abs(w(i-1,iuz)) then duz = 0.0
         normal = [ dux, duy, duz ]
         normal /= norm(normal)
         print,'Shock normal at time ',Time(i)/3600.,'h: ',normal
      endif
      ;; normalize the normal vector to unit length
      normal /= norm(normal)
      unormal = total(normal*w(i,iux:iuz))
      TimeDelayOrig = TimeDelay(i)
      TimeDelay(i) = -x(i)*normal(0)/unormal

      if abs(normal(1)) gt 0.2 or abs(normal(2)) gt 0.2  then $
         print,'TimeDelayOrig, TimeDelay=', TimeDelayOrig, TimeDelay(i), $
               ', normal=', normal
      
      ;; Decay normal vector towards 1,0,0
      normal(1:2) = exp(-dt/decay)*normal(1:2)
   endfor
endif

NewTime = Time + TimeDelay

; Smooth out rarefaction waves
ibx = 7
for iVar=ibx,nVar-1 do begin
    i0   = 0
    Var0 = w(0,iVar)
    for i1=1,nPoint-1 do begin
        if w(i1,iVar) ne Var0 then begin
                                ; For rarefaction waves the negative
                                ; velocity is increasing
                                ; Maybe a more sophisticated shock
                                ; recognition is needed

            if w(i1,iux) gt w(i1-1,iux) then begin
                for i=i0+1,i1-1 do begin
                    w(i,iVar)= ((Time(i)-Time(i0))*w(i1,iVar) + $
                                (Time(i1)-Time(i))*w(i0,iVar)   $
                               )/(Time(i1)-Time(i0))
                endfor
            endif
            i0   = i1
            Var0 = w(i1,iVar)
        endif
    endfor
endfor

; Take shock waves into account
for i=1,nPoint-1 do begin
                                ; Set time=-1 for previous 
                                ; entries with longer arrival time    
    index = where(NewTime(0:i-1) gt NewTime(i))
    if index(0) ge 0 then NewTime(index) = -1
endfor

print,'nPoint=',nPoint

; Print out corrected IMF file
close,1

print,'writing into output file=', outputfile
help,NewTime
print,NewTime(0:10)
print,'min/max NewTime=', min(NewTime),max(NewTime)

if keyword_set(gsm) then begin
   ;; Add logtime converted to millisec
   epoch = epoch0 + NewTime*1d3

   b = transpose(w(*,7:9))
   gse_gsm, b, epoch
   w(*,7:9) = transpose(b)
   
   u = transpose(w(*,10:12))
   gse_gsm, u, epoch
   w(*,10:12) = transpose(u)
endif


openw,1,outputfile
printf,1,'Corrected IMF based on ',inputfile
printf,1,'yr mo dy hr mn sc ms bx by bz ux uy uz rho T'
printf,1,'#START'
for i=0,nPoint-1 do begin
    ; Skip entries marked with negative times
    if(NewTime(i) ge 0)then begin
        ; Calculate integer times
        epoch = epoch0 + NewTime(i)*1d3
        cdf_epoch, epoch, year, month, day, hour, min, sec, msc, /break
        printf,1,year,month,day,hour,min,sec,msc,w(i,ibx:nVar-1),$
          format='(i4,5i3.2,i4,7f11.2,f13.2)'
    endif
endfor
close,1

end
