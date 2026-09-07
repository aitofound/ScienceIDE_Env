pro compare_wl,  TimeEvent=TimeEvent, varnames=varnames, nvars=nvars,        $
                 dataSim=dataSim, nx=nx, ny=ny,                              $
                 dir_obs=dir_obs, dir_plot=dir_plot,                         $
                 extra_plt_info=extra_plt_info, TypePlotFile=TypePlotFile,   $
                 UseTimePlotName=UseTimePlotName,                            $
                 CharSizeLocal=CharSizeLocal, unitlog=unitlog,               $
                 NameSat=NameSat,  xs_map=xs_map,ys_map=ys_map,              $
                 DoIDLCompare=DoIDLCompare, file_sim=file_sim, rMaxSim=rMaxSim

  NameSub = 'compare_wl'

  if (not keyword_set(UseTimePlotName)) then UseTimePlotName = 1
  if (not keyword_set(TypePlotFile))    then TypePlotFile    = 'png'

  if (not keyword_set(dir_obs))  then begin
     dir_obs = './obsdata'
     printf, unitlog, ' saves into the default dir_obs = ./obsdata'
  endif

  if (not keyword_set(dir_plot)) then begin
     dir_plot = './output'
     printf, unitlog, ' saves into the default dir_plot = ./output'
  endif

  if (file_test(dir_obs,  /directory) eq 0) then file_mkdir, dir_obs
  if (file_test(dir_plot, /directory) eq 0) then file_mkdir, dir_plot

  if (not keyword_set(xs_map)) then xs_map = 512
  if (not keyword_set(ys_map)) then ys_map = 512

  ;; params for plotting
  imagefov = [-6000,6000]
  plotmin  = 1.e-2

  get_wl_info, NameSat, SourceName=SourceName, InstName = InstName, $
               DetName = DetName, titlePlt = titlePlt,              $
               titleModel = titleModel, InstPlt = InstPlt

  set_FilePlotName, TimeEvent, UseTimePlotName = UseTimePlotName,    $
                    TimeStrFile = TimeStrFile, fileplot = fileplot,  $
                    dir_plot = dir_plot, InstPlt = InstPlt

  printf, unitlog, NameSub, ': started for ', InstPlt

  ;;------------------------------------------------------------------------
  ;; Download the data
  TimeWin    = 8.0*3600
  TimeEventT = utc2tai(anytim2utc(TimeEvent))

  TimeStart  =strjoin( strsplit( utc2str( tai2utc(TimeEventT-TimeWin) ), '-', $
                                 /extract) , '/')
  TimeEnd    =strjoin( strsplit( utc2str( tai2utc(TimeEventT+TimeWin) ), '-', $
                                 /extract) , '/')

  TimeRange = TimeStart + '-' + TimeEnd

  ListLocal_I = vso_search(date=TimeRange,source=SourceName,inst=InstName, $
                           det=DetName, COUNT=nFiles)

  if (nFiles eq 0) then begin
     printf, unitlog, '*******************************************************'
     printf, unitlog, ' cannot find any data in the server !!!!!!'
     printf, unitlog, '*******************************************************'
     goto, out
  endif

  ListTimeLocal_I = utc2tai(ListLocal_I.Time.Start)

  indexTmp = where(ListLocal_I.Size gt 600., Count)

  if Count lt 1 then indexTmp = where(ListLocal_I.Size gt 0., Count)

  if Count lt 1 then begin
     printf, unitlog, NameSub, ': ERROR!!!!'
     printf, unitlog, NameSub, ': could not find an image for ', InstPlt
     printf, unitlog,'           for time range: ', TimeRange
     goto, out
  endif

  ObsTime_I = ListTimeLocal_I[indexTmp]
  index_I   = where(abs(ListTimeLocal_I - TimeEventT) eq     $
                    min(abs(ObsTime_I - TimeEventT)))

  StringLine = strsplit(ListLocal_I[index_I].fileid,'/', /extract)
  nStrings   = n_elements(StringLine)
  filename_I = StringLine[nStrings-1]

  GoodList_I = ListLocal_I[index_I]

  DoDownload = 1

  files_filename_save = dir_obs+'filenames_'+TimeEventFile+'_'+InstPlt+'.sav'
  maps_filename_save  = dir_obs+'maps_'+TimeEventFile+'_'+InstPlt+'.sav'

  if (file_test(files_filename_save)) then begin
     restore, files_filename_save
     
     ;; if one file is different, re-download all...
     DoDownload = total(filename_I eq filenameOrig_I) ne 1

  endif

  if (not file_test(maps_filename_save)) then DoDownload = 1

  if (not DoDownload) then goto, skip_download

  filenameOrig_I = filename_I

  printf, unitlog, NameSub, ': downloading ', InstPlt, ' images for times: ', $
          GoodList_I.Time.Start

  Status     = vso_get(GoodList_I,out_dir=dir_obs)
  filename_I = dir_obs + filename_I

  printf, unitlog, NameSub, ': downloaded at ', filename_I
  printf, unitlog, ''

  ;;------------------------------------------------------------------------
  ;; prepare the image for plotting
  if (DetName eq 'C2' or DetName eq 'C3') then begin
     lasco_prep,filename_I,indexlocal,datalocal
     index2map,indexlocal,datalocal,obs_map,outsize=[1024,1024]
  endif else begin
     ;; ????????
     secchi_prep,filename_I,indexlocal,datalocal,/rotation_on
     index2map,indexlocal,datalocal,obs_map,outsize=[1024,1024]
  endelse

  save, filenameOrig_I, filename=files_filename_save
  save, obs_map, filename=maps_filename_save

  skip_download: if (not DoDownload) then begin
     printf, unitlog, NameSub, ' restored ', InstPlt, ', orig: ', $
             filenameOrig_I[0]
     printf, unitlog, '                        new:  ', filename_I[0]

     restore, maps_filename_save
  endif

  ;;------------------------------------------------------------------------
  ;; processing the swmf data
  rSizeImage = 6.0
  dxy        = rSizeImage*2000./nx

  make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='wl', $
                     nx=nx, ny=ny, dxy=dxy, TimeEvent=TimeEvent,       $
                     map_out =sim_map

  ;;------------------------------------------------------------------------
  ;; plotting
  
  nx_plot=2 & ny_plot=1
  !p.multi=[0,2,1]
  loadct,1

  plotmax=max([max(obs_map.data),max(sim_map.data)])

  if (TypePlotFile eq 'png') then begin
     set_plot,'X'

     cal_pos,nx_plot,ny_plot,y_x_ratio=y_x,x1=x1,y1=y1,x2=x2,y2=y2,   $
             tm=0.1,dy=0,dx=0.8,lm=0.8,bm=0.5

     wdef,2,1000,500

     pos=[x1[0],y1[0],x2[0],y2[0]]
     plot_map,sim_map,/iso,charsize=CharSizeLocal,title=titleModel,/log,    $
              dmin=plotmin,dmax=plotmax,xrange=imagefov,yrange=imagefov,    $
              position=pos

     pos=[x1[1],y1[0],x2[1],y2[0]]
     plot_map,obs_map,/iso,charsize=CharSizeLocal,/log,                     $
              dmin=plotmin,dmax=plotmax,xrange=imagefov,yrange=imagefov,    $
              position=pos

     imager = tvrd(true=1)
     write_png,fileplot+extra_plt_info+'.png',imager,r,g,b

     ;; delete windows
     wdelete,2
  endif else if (TypePlotFile eq 'eps') then begin
     set_plot,'PS'
     device,/encapsulated
     device,filename=fileplot+extra_plt_info+'.eps',/color,bits_per_pixel=8
     device,xsize=34,ysize=17

     cal_pos,nx_plot,ny_plot,y_x_ratio=y_x,x1=x1,y1=y1,x2=x2,y2=y2,  $
             tm=0.1,dy=0,dx=0.6,lm=0.8,bm=0.5

     pos=[x1[0],y1[0],x2[0],y2[0]]
     plot_map,sim_map,/iso,charsize=CharSizeLocal,title=titleModel,/log,     $
              dmin=plotmin,dmax=plotmax,xrange=imagefov,yrange=imagefov,     $
              position=pos

     pos=[x1[1],y1[0],x2[1],y2[0]]
     plot_map,obs_map,/iso,charsize=CharSizeLocal,title=titlePlt,/log,       $
              dmin=plotmin,dmax=plotmax,xrange=imagefov,yrange=imagefov,     $
              position=pos,ytitle=''

     device,/close
     
     ;; reset window
     set_plot,'X'
  endif

  ;; reset
  !p.multi=0
  loadct,0

  ;;------------------------------------------------------------------------
  out: printf, unitlog, NameSub, ' is finished.'
  printf, unitlog, ''
end
