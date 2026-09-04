CoordSys = "HGI"

StartTimeRel = -1166400.0D
EndTimeRel =  1166400.0D
TimeStep = 3600.0D

StartTime7 = {year:2015,$
              month:3,$
              day:6,$
              hour:4,$
              minute:0,$
              second:0,$
              millisecond: 0}

Satellite = ['earth', 'sta', 'stb', 'psp', 'solo']

nSat = n_elements(Satellite)

StartTime = utc2sec(anytim2utc(StartTime7))
nTime = floor((EndTimeRel - StartTimeRel)/TimeStep) + 1

CurrentTime=dblarr(nTime)
for iTime = 0, nTime-1 do begin
   CurrentTime[iTime] = StartTime + StartTimeRel + TimeStep*iTime
endfor

rSun = 6.96D5

for iSat = 0, nSat-1 do begin

   Sat = Satellite[iSat]

   case CoordSys of
      'HGR': pos = get_sunspice_coord(sec2utc(CurrentTime),Sat,$
                                      system='Carrington')
      'HGI': pos = get_sunspice_coord(sec2utc(CurrentTime),Sat,$
                                      system='HCI')
      else:
   endcase

   openw,lun,'./output/'+Sat+'_traj.dat',/get_lun

   printf,lun,"#COOR"
   printf,lun,CoordSys
   printf,lun
   printf,lun,"#DATE"
   printf,lun,StartTime7,format='(i12)'
   printf,lun
   printf,lun,"#TIMELOOP"
   printf,lun,StartTimeRel,EndTimeRel,TimeStep,format='(3f12.3)'
   printf,lun
   printf,lun,"#START"
   for i = 0, nTime-1 do begin
      printf,lun,anytim(sec2utc(CurrentTime[i]),/UTC_EXT),pos[0:2,i]/rSun,$
             format='(7i4,3f12.2)'
   endfor

   close,lun
   free_lun,lun

endfor

end
