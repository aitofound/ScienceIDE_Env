pro compare_remote_one, TimeEvent=TimeEvent, varnames=varnames,              $
                        nvars=nvars, dataSim=dataSim, nx=nx, ny=ny,          $
                        CharSizeLocal=CharSizeLocal, dir_plot=dir_plot,      $
                        extra_plt_info=extra_plt_info,                       $
                        TypePlotFile=TypePlotFile,                           $
                        UseTimePlotName=UseTimePlotName, unitlog=unitlog,    $
                        DoWl =DoWl, DoAIA=DoAIA, DoXRT=DoXRT, DoEUV=DoEUV,   $
                        NameSat=NameSat,rMaxSim=rMaxSim, xs_map=xs_map,      $
                        ys_map=ys_map,DoIDLCompare=DoIDLCompare,             $
                        file_sim=file_sim, dir_obs = dir_obs

  if (DoAIA) then $
     compare_AIA, TimeEvent=TimeEvent, varnames=varnames,                   $
                  nvars=nvars, dataSim=dataSim, nx=nx, ny=ny,               $
                  CharSizeLocal=CharSizeLocal, dir_plot=dir_plot,           $
                  extra_plt_info=extra_plt_info, TypePlotFile=TypePlotFile, $
                  UseTimePlotName=UseTimePlotName, unitlog=unitlog,         $
                  xs_map=xs_map, ys_map=ys_map, NameSat=NameSat,            $
                  DoIDLCompare=DoIDLCompare, file_sim=file_sim,             $
                  dir_obs=dir_obs

  if(DoEUV) then $
     compare_EUV, TimeEvent=TimeEvent, varnames=varnames,                   $
                  nvars=nvars, dataSim=dataSim, nx=nx, ny=ny,               $
                  CharSizeLocal=CharSizeLocal, dir_plot=dir_plot,           $
                  extra_plt_info=extra_plt_info, TypePlotFile=TypePlotFile, $
                  UseTimePlotName=UseTimePlotName, unitlog=unitlog,         $
                  xs_map=xs_map, ys_map=ys_map, NameSat=NameSat,            $
                  dir_obs=dir_obs


  if(DoXRT) then begin
     printf, unitlog, '************************************************'
     printf, unitlog, ' XRT is not tested !!!!!!!'
     printf, unitlog, '************************************************'
  endif

  if(DoWl) then begin
     printf, unitlog, '************************************************'
     printf, unitlog, ' white light is not tested !!!!!!!'
     printf, unitlog, '************************************************'
  endif
end
