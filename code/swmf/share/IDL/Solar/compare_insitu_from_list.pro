pro compare_insitu_from_list, filename_list=filename_list, dir_plot=dir_plot,   $
                              DoPlotTe=DoPlotTe, CharSizeLocal=CharSizeLocal,   $
                              nyMaxLegend=nyMaxLegend,DoHighlight=DoHighlight,  $
                              DoStrictRange=DoStrictRange, dir_CME_list=dir_CME_list, $
                              DoPlotDeltaB=DoPlotDeltaB, yMax_I=yMax_I

  if (not keyword_set(filename_list)) then begin
     if (file_test('./filename_list.txt')) then begin
        filename_list  = './filename_list.txt'
     endif else begin
        print,'Error, ./filename_list.txt does not exist, need to specify filename_list.'
        return
     endelse
  endif
  
  if (not keyword_set(dir_plot)) then begin
     if (file_test('./output', /directory) eq 0) then file_mkdir, './output'
     dir_plot = './output'
     print, ' Saves into the default dir_plot = ./output'
  endif

  if (not keyword_set(DoPlotTe))       then DoPlotTe = 0
  if (not keyword_set(CharSizeLocal))  then CharSizeLocal = 2.5
  if (not keyword_set(nyMaxLegend))    then nyMaxLegend=3
  if (not keyword_set(DoHighlight))    then DoHighlight=0
  if (keyword_set(DoHighlight))        then $
     openw, lun_opt, './output/avg_dist.txt',/get_lun
  if (not keyword_set(DoStrictRange))  then DoStrictRange=0

  if (not keyword_set(DoPlotDeltaB))    then DoPlotDeltaB = 0

  if (not keyword_set(dir_CME_list)) then dir_CME_list = './'
  if (not keyword_set(yMax_I)) then yMax_I=[1000,40,1e6,30]

  EventTimeDist  = 'none'
  TimeWindowDist = -7
  
  umax_plot = yMax_I(0)
  nmax_plot = yMax_I(1)
  tmax_plot = yMax_I(2)
  bmax_plot = yMax_I(3)

  umax = 0
  nmax = 0
  tmax = 0
  bmax = 0

  dist_un_min = 999.9

  ;; first iteration to get the CR number and the range of the plot...
  openr, lun, filename_list, /get_lun
  line = ''
  ;; everything before #START is header...
  while not eof(lun) do begin
     readf,lun,line
     if line ne '#START' then continue
     break
  end
  while not eof(lun) do begin
     readf,lun,line
     strings_local = strsplit(line,',', /EXTRACT)

     filenames_sat  = strings_local[0]
     filename_sat_I = FILE_SEARCH(filenames_sat, count=nFileSim)

     if nFileSim eq 0 then begin
        print, 'no file is found for '+filenames_sat
        continue
     endif

     for ifile=1,nFileSim do begin
        filename_sat = filename_sat_I(ifile-1)
        read_swmf_sat, filename_sat, time_swmf, n_swmf, ux_swmf, uy_swmf,       $
                       uz_swmf, bx_swmf, by_swmf, bz_swmf, ti_swmf, te_swmf,    $
                       ut_swmf, ur_swmf, B_swmf, Btotal_swmf, br_swmf,          $
                       DoContainData=DoContainData, TypeData=TypeData,          $
                       TypePlot=TypePlot, start_time=start_time, end_time=end_time

        if DoContainData ne 1 then begin
           print, " Error: filename=", filename_sat, " does not contain any data"
           continue
        endif

        CR_number = fix(tim2carr(time_swmf(n_elements(time_swmf)/2),/dc))

        if (not keyword_set(CR_number_plot)) then $
           CR_number_plot = CR_number

        if CR_number_plot ne CR_number then begin
           print, 'CR_number_plot /= CR_number, the comparison must be done with the same CR'
           print, 'correct the list to proceed. '
           print, 'CR_number_plot =', CR_number_plot
           print, 'CR_number      =', CR_number
           return
        endif

        umax = max([umax,max(ur_swmf)*1.3])
        nmax = max([nmax,max(n_swmf )*1.3])
        tmax = max([tmax,max(ti_swmf)*1.3])
        bmax = max([bmax,max(B_swmf )*1.3*1e5])
        if DoPlotTe then tmax = max([tmax,max(te_swmf)*1.3])

        ;; download the observatonal data if time_obs is not defined yet
        ;; no need to download if time_obs is defined (already downloaded)
        if (not isa(time_obs)) then begin
           get_insitu_data, start_time, end_time, TypeData, u_obs, n_obs, tem_obs,  $
                            mag_obs, time_obs, br_obs, DoContainData=DoContainData

           if DoContainData ne 1 then begin
              print, " Error: no observational data are found."
              return
           endif
        endif

        ;; calculate the distance values to determine the minimum distance
        if DoHighlight then begin
           dist_int = calc_dist_insitu(time_obs, u_obs,  n_obs, tem_obs, mag_obs, $
                                       time_swmf,ut_swmf,n_swmf,ti_swmf, B_swmf,  $
                                       dist_int_u, dist_int_t,                    $
                                       dist_int_n, dist_int_b,                    $
                                       EventTimeDist, TimeWindowDist)

           print,filename_sat+' avg dist of n and u is: ', $
                 (dist_int_u+dist_int_n)/2.0
           printf,lun_opt,filename_sat+' avg dist of n and u is: ', $
                  (dist_int_u+dist_int_n)/2.0
           
           dist_un_min = min([dist_un_min,(dist_int_u+dist_int_n)/2.0])
        endif
     end
  end ;; while not eof(lun)
  close,lun
  free_lun,lun

  ;; get the CME intervals based on the list file.
  start_time_CME_I = ''
  end_time_CME_I   = ''
  print, 'TypeData =', TypeData
  if (TypeData eq 'OMNI') then begin
     if file_test(dir_CME_list+'/ICME_list_EARTH.csv') then $
        get_CME_interval,dir_CME_list+'/ICME_list_EARTH.csv',start_time_CME_I, end_time_CME_I
  endif

  ;; adjust again with the observation
  if DoStrictRange then begin
     umax_plot = min([umax_plot,max([umax,max(u_obs)*1.3])])
     nmax_plot = min([nmax_plot,max([nmax,max(n_obs)*1.3])])
     tmax_plot = min([tmax_plot,max([tmax,max(tem_obs)*1.3])])
     bmax_plot = min([bmax_plot,max([bmax,max(mag_obs)*1.3])])
  endif else begin
     umax_plot = max([umax,max(u_obs)*1.3])
     nmax_plot = max([nmax,max(n_obs)*1.3])
     tmax_plot = max([tmax,max(tem_obs)*1.3])
     bmax_plot = max([bmax,max(mag_obs)*1.3])
  endelse

  ;; set the filename for the output figure...
  fileplot = dir_plot + '/CR'+string(CR_number_plot,format='(i4)') +TypePlot $
             + '_compare_list' + '.eps'

  set_plot,'PS'
  device,/encapsulated
  device,filename=fileplot,/color,bits_per_pixel=8
  device,xsize=20,ysize=20

  IsOverPlot       = 0
  nLegendPlotTotal = 0
  
  openr, lun, filename_list, /get_lun
  while not eof(lun) do begin
     readf,lun,line
     if line ne '#START' then continue
     break
  end
  while not eof(lun) do begin
     readf,lun,line
     strings_local = strsplit(line,',', /EXTRACT)

     filenames_sat  = strings_local[0]
     filename_sat_I = FILE_SEARCH(filenames_sat, count=nFileSim)

     if nFileSim eq 0 then begin
        print, 'no file is found for '+filenames_sat
        continue
     endif

     for ifile=1,nFileSim do begin
        filename_sat=filename_sat_I(ifile-1)
     
        read_swmf_sat, filename_sat, time_swmf, n_swmf, ux_swmf, uy_swmf,       $
                       uz_swmf, bx_swmf, by_swmf, bz_swmf, ti_swmf, te_swmf,    $
                       ut_swmf, ur_swmf, B_swmf, Btotal_swmf, br_swmf,          $
                       DoContainData=DoContainData, TypeData=TypeData,          $
                       TypePlot=TypePlot, start_time=start_time, end_time=end_time

        if DoContainData ne 1 then begin
           print, " Error: filename=", filename_sat, " does not contain any data"
           continue
        endif

        DoLegendIn = 1
        if n_elements(strings_local) ge 2 then begin
           ;; set the legend and DoLegendIn
           if (strings_local[1] eq 'none') then begin
              ;; none means don't write the legend for this line
              DoLegendIn = 0
           endif else if (strings_local[1] eq 'default') then begin
              ;; not so sure whether a user should use default, but just in case...
              if (strpos(filename_sat, 'AWSoM2T') ge 0) then begin
                 legendNameIn = 'AWSoM-2T'
              endif else if (strpos(filename_sat, 'AWSoMR') ge 0) then begin
                 legendNameIn = 'AWSoM-R'
              endif else begin
                 legendNameIn = 'AWSoM'
              endelse
           endif else begin
              legendNameIn = strings_local[1]
           endelse
        endif else begin
           DoLegendIn = 0
        endelse

        ;; set the line thickness and the color
        if n_elements(strings_local) ge 3 then begin
           linethickIn = float(strings_local[2])
        endif else begin
           linethickIn = 3
        endelse

        if n_elements(strings_local) ge 4 then begin
           colorIn = fix(strings_local[3])
        endif else begin
           colorIn = 6
        endelse

        if DoHighlight then begin
           dist_int = calc_dist_insitu(time_obs, u_obs,  n_obs, tem_obs, mag_obs, $
                                       time_swmf,ut_swmf,n_swmf,ti_swmf, B_swmf,  $
                                       dist_int_u, dist_int_t,                    $
                                       dist_int_n, dist_int_b,                    $
                                       EventTimeDist, TimeWindowDist)

           if abs(dist_un_min - (dist_int_n+dist_int_u)/2.0) le 1e-5 then begin
              time_swmf_h = time_swmf
              ur_swmf_h   = ur_swmf
              n_swmf_h    = n_swmf
              ti_swmf_h   = ti_swmf
              te_swmf_h   = te_swmf
              B_swmf_h    = B_swmf
              filename_sat_h = filename_sat
           endif
        endif

        ;; calculate the position for the legend, 0.02112 is from the
        ;; print out value when calling legend in procedures_local...
        nxLegend = nLegendPlotTotal/nyMaxLegend
        nyLegend = nLegendPlotTotal mod nyMaxLegend
        legendPosLIn=[0.15+0.2*nxLegend,0.95-0.02112*nyLegend]

        plot_insitu, time_obs, u_obs,  n_obs,  tem_obs, mag_obs,                 $
                     time_swmf, ur_swmf, n_swmf,  ti_swmf,  te_swmf, B_swmf,     $
                     Btotal_swmf, start_time, end_time, typeData=typeData,       $
                     charsize=CharSizeLocal, DoPlotTe = DoPlotTe,                $
                     legendNames=legendNameIn, DoShowDist=0,                     $
                     ymax_I=[umax_plot,nmax_plot,tmax_plot,bmax_plot],           $
                     IsOverPlot=IsOverPlot, DoLogT=1, linethick=linethickIn,     $
                     colorLocal=colorIn, DoLegend=DoLegendIn,                    $
                     nLegendPlot=nLegendPlot,legendPosL=legendPosLIn,            $
                     EventTimeDist=EventTimeDist, TimeWindowDist=TimeWindowDist, $
                     DoPlotDeltaB=DoPlotDeltaB, start_time_CME_I=start_time_CME_I,$
                     end_time_CME_I=end_time_CME_I

        ;; nLegendPlot is the number of legend printed in plot_insitu
        nLegendPlotTotal = nLegendPlotTotal + nLegendPlot
        IsOverPlot = 1

        ;; print,' nLegendPlotTotal =', nLegendPlotTotal
     end
  end
  close,lun
  free_lun,lun

  if DoHighlight then begin
     nxLegend = nLegendPlotTotal/nyMaxLegend
     nyLegend = nLegendPlotTotal mod nyMaxLegend
     legendPosLIn=[0.15+0.2*nxLegend,0.95-0.02112*nyLegend]

     plot_insitu, time_obs, u_obs,  n_obs,  tem_obs, mag_obs,                 $
                  time_swmf_h, ur_swmf_h, n_swmf_h,  ti_swmf_h,  te_swmf_h, B_swmf_h,     $
                  Btotal_swmf, start_time, end_time, typeData=typeData,       $
                  charsize=CharSizeLocal, DoPlotTe = DoPlotTe,                $
                  legendNames='Optimal Run', DoShowDist=0,                    $
                  ymax_I=[umax_plot,nmax_plot,tmax_plot,bmax_plot],           $
                  IsOverPlot=IsOverPlot, DoLogT=1, linethick=15,              $
                  colorLocal=5, DoLegend=1,                                   $
                  nLegendPlot=nLegendPlot,legendPosL=legendPosLIn,            $
                  DoPlotDeltaB=DoPlotDeltaB

     print, 'The optimal run is found at '+filename_sat_h
     printf, lun_opt, 'The optimal run is found at '+filename_sat_h
     
     close,lun_opt
     free_lun,lun_opt
  endif

  device,/close_file
end
