;set up the start and end time for obtaining the in-situ observations
pro compare_insitu, dir_sim=dir_sim, dir_plot=dir_plot,     $
                    extra_plt_info=extra_plt_info,          $
                    UseTimePlotName=UseTimePlotName,        $
                    CharSizeLocal=CharSizeLocal,            $
                    DoPlotTe=DoPlotTe, ModelIn=ModelIn,     $
                    dir_obs=dir_obs,                        $
                    EventTimeDist=EventTimeDist,            $
                    TimeWindowDist=TimeWindowDist,          $
                    dir_CME_list=dir_CME_list

  if (not keyword_set(dir_sim)) then begin
     if (file_test('./simdata', /directory)) then begin
        dir_sim  = './simdata/'
        print, ' Uses the default dir_sim = ./simdata'
     endif else begin
        print, ' Please specify the directory containing simulation results'
        return
     endelse
  endif

  if (not keyword_set(dir_plot)) then begin
     dir_plot = './output'
     print, ' Saves into the default dir_plot = ./output'
  endif

  if (not keyword_set(dir_obs)) then begin
     dir_obs = './obsdata'
     print, ' Saves into the default dir_obs = ./obsdata'
  endif

  if (file_test(dir_plot, /directory) eq 0) then file_mkdir, dir_plot
  if (file_test(dir_obs,  /directory) eq 0) then file_mkdir, dir_obs

  if (not keyword_set(extra_plt_info)) then begin
     extra_plt_info = ''
  endif else begin
     if (strmid(extra_plt_info,0,1) ne '_') then $
        extra_plt_info = '_' + extra_plt_info
  endelse
  if (not keyword_set(UseTimePlotName)) then UseTimePlotName = 0
  if (not keyword_set(CharSizeLocal))   then CharSizeLocal = 2.5
  if (not keyword_set(DoPlotTe))        then DoPlotTe = 0
  
  if (strpos(dir_sim, 'AWSoM2T') ge 0) then begin
     Model = 'AWSoM-2T'
  endif else if (strpos(dir_sim, 'AWSoMR') ge 0) then begin
     Model = 'AWSoM-R'
  endif else if (strpos(dir_sim, 'AWSoM') ge 0) then begin
     Model = 'AWSoM'
  endif
  
  if (keyword_set(ModelIn)) then begin
     if (ModelIn ne 'DEFAULT') then Model = ModelIn
  endif else begin
     Model = 'AWSoM'
  endelse

  files_sim = file_search(dir_sim+'/*sat', count = nSimFile)

  dirs_adapt = file_search(dir_sim+'/run[01][0-9]', count = nDir)

  if (nSimFile eq 0 and nDir eq 0) then begin
     print, ' no simulation data'
     return
  endif

  if (not isa(EventTimeDist))  then EventTimeDist  = 'none'
  if (not isa(TimeWindowDist)) then TimeWindowDist = -7

  if (not keyword_set(dir_CME_list)) then dir_CME_list = './'

  start_time_CME_I = ''
  end_time_CME_I   = ''

  ;; default is to save the observation
  DoSaveObs = 1

  print, "+++++++++++++++++++++++++++++++++++++++++++++++++++++"
  print, "compare_remote: dir_obs    =", dir_obs
  print, "compare_remote: files_sim  =", files_sim
  print, "compare_remote: dirs_adapt =", dirs_adapt

  if nDir gt 0 then begin
     TypeADAPT_I = ['earth', 'sta', 'stb', 'solo', 'psp']

     for iType = 0, n_elements(TypeADAPT_I)-1 do begin
        ;; reset start_time_CME_I and end_time_CME_I
        start_time_CME_I = ''
        end_time_CME_I   = ''

        ;; for each directory and each type, reset the default value
        DoSaveObs = 1

        TypeADAPT = TypeADAPT_I[iType]
        files_adapt_one = file_search(dirs_adapt+'/IH/*'+TypeADAPT+'*sat', $
                                      count = nFileAdaptOne)

        set_plot,'PS'
        device,/encapsulated
        device,filename=dir_sim+'/'+TypeADAPT+'_all.eps',/color,bits_per_pixel=8
        device,xsize=20,ysize=20
        !p.multi=[0,1,4]

        IsOverPlot = 0
        DoLegend   = 1

        if (TypeADAPT) eq 'earth' then begin
           if file_test(dir_CME_list+'/ICME_list_EARTH.csv') then $
              get_CME_interval,dir_CME_list+'/ICME_list_EARTH.csv',start_time_CME_I, end_time_CME_I
        endif

        u_max = 0
        n_max = 0
        T_max = 0
        B_max = 0

        for iFile=0, nFileAdaptOne-1 do begin
           file_sim_adapt=files_adapt_one[iFile]

           read_swmf_sat, file_sim_adapt, time_swmf, n_swmf, ux_swmf, uy_swmf,        $
                          uz_swmf, bx_swmf, by_swmf, bz_swmf, ti_swmf, te_swmf,       $
                          ut_swmf, ur_swmf, B_swmf, Btotal_swmf, br_swmf,             $
                          deltaB_swmf,DoContainData=DoContainData,                    $
                          TypeData=TypeData, TypePlot=TypePlot,                       $
                          start_time=start_time, end_time=end_time,                   $
                          DoPlotDeltaB=DoPlotDeltaB

           if DoContainData ne 1 then begin
              print, " Error: filename=", file_sim_adapt, " does not contain any data"
              continue
           endif
           
           u_max = max([u_max, ut_swmf])
           n_max = max([n_max, n_swmf])
           T_max = max([T_max, ti_swmf])
           B_max = max([B_max, B_swmf*1e5])
           if DoPlotDeltaB then begin
              Btotal_max = max([B_max, Btotal_swmf*1e5])
              deltaB_max = max([Btotal_max,deltaB_swmf])
           endif   
        endfor
        
           
        get_insitu_data, start_time, end_time, TypeData, u_obs, n_obs, tem_obs,  $
                         mag_obs, time_obs, br_obs, DoContainData=DoContainData

        if DoContainData ne 1 then begin
           print, " Error: no observational data are found."
           continue
        endif
        
        u_max = max([u_max, u_obs])*1.3
        n_max = max([n_max, n_obs])*1.3
        T_max = max([T_max, tem_obs])*1.3
        B_max = max([B_max, mag_obs])*1.3

        for iFile=0, nFileAdaptOne-1 do begin
           file_sim_adapt=files_adapt_one[iFile]

           read_swmf_sat, file_sim_adapt, time_swmf, n_swmf, ux_swmf, uy_swmf,        $
                          uz_swmf, bx_swmf, by_swmf, bz_swmf, ti_swmf, te_swmf,       $
                          ut_swmf, ur_swmf, B_swmf, Btotal_swmf, br_swmf,             $
                          deltaB_swmf, DoContainData=DoContainData,                   $
                          TypeData=TypeData, TypePlot=TypePlot,                       $
                          start_time=start_time, end_time=end_time,                   $
                          DoPlotDeltaB=DoPlotDeltaB

           plot_insitu, time_obs, u_obs,  n_obs,  tem_obs, mag_obs,                 $
                        time_swmf, ut_swmf, n_swmf,  ti_swmf,  te_swmf, B_swmf,     $
                        Btotal_swmf, deltaB_swmf, start_time, end_time,             $
                        typeData=typeData,charsize=CharSizeLocal,                   $
                        DoPlotTe = DoPlotTe,                                        $
                        legendNames=Model, DoShowDist=0, IsOverPlot=IsOverPlot,     $
                        DoLegend=DoLegend,ymax_I=[u_max,n_max,T_max,B_max],         $
                        DoLogT=1, linethick=5, DoPlotDeltaB=DoPlotDeltaB,           $
                        start_time_CME_I=start_time_CME_I, end_time_CME_I=end_time_CME_I
           
           IsOverPlot = 1
           DoLegend   = 0
        endfor
        device,/close_file

        for iFile=0, nFileAdaptOne-1 do begin
           file_sim_adapt=files_adapt_one[iFile]

           compare_insitu_one, file_sim=file_sim_adapt, extra_plt_info=extra_plt_info,    $
                               UseTimePlotName=UseTimePlotName,                           $
                               CharSizeLocal=CharSizeLocal, DoPlotTe=DoPlotTe,            $
                               Model=Model, dir_obs=dir_obs, dir_plot=dirs_adapt[iFile],  $
                               DoSaveObs=DoSaveObs, DoLogT=1, EventTimeDist=EventTimeDist,$
                               TimeWindowDist=TimeWindowDist, DoPlotDeltaB=DoPlotDeltaB,  $
                               start_time_CME_I=start_time_CME_I, end_time_CME_I=end_time_CME_I
           DoSaveObs = 0
        endfor
     endfor
  endif

  ;; reset the default value
  DoSaveObs = 1

  for i = 0, nSimFile-1 do begin
     file_sim     = files_sim(i)

     start_time_CME_I = ''
     end_time_CME_I   = ''

     if (strpos(file_sim,'earth') ge 0 and file_test(dir_CME_list+'/ICME_list_EARTH.csv')) then $
        get_CME_interval,dir_CME_list+'/ICME_list_EARTH.csv',start_time_CME_I, end_time_CME_I
     
     compare_insitu_one, file_sim=file_sim, extra_plt_info=extra_plt_info, $
                         UseTimePlotName=UseTimePlotName,                  $
                         CharSizeLocal=CharSizeLocal, DoPlotTe=DoPlotTe,   $
                         Model=Model, dir_obs=dir_obs, dir_plot=dir_plot,  $
                         DoSaveObs=DoSaveObs, EventTimeDist=EventTimeDist, $
                         TimeWindowDist=TimeWindowDist,                    $
                         DoPlotDeltaB=DoPlotDeltaB,                        $
                         start_time_CME_I=start_time_CME_I, end_time_CME_I=end_time_CME_I
  endfor
end
