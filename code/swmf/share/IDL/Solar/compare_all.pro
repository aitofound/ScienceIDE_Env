pro compare_all,dir_sim=dir_sim, dir_plot=dir_plot,   $
                extra_plt_info=extra_plt_info,        $
                UseTimePlotName=UseTimePlotName

  if (not keyword_set(dir_sim))  then dir_sim  = './simdata/'
  if (not keyword_set(dir_plot)) then dir_plot = './output/'
  if (not keyword_set(extra_plt_info)) then begin
     extra_plt_info = ''
  endif else begin
     extra_plt_info = extra_plt_info
  endelse

  if (not keyword_set(UseTimePlotName)) then UseTimePlotName = 0

  compare_insitu,dir_sim=dir_sim, dir_plot=dir_plot,   $
                 extra_plt_info=extra_plt_info,        $
                 UseTimePlotName=UseTimePlotName

  compare_remote,dir_sim=dir_sim, dir_plot=dir_plot,   $
                 extra_plt_info=extra_plt_info,        $
                 UseTimePlotName=UseTimePlotName
end
