pro compare_EUV, TimeEvent=TimeEvent, varnames=varnames, nvars=nvars,        $
                 dataSim=dataSim, nx=nx, ny=ny,                              $
                 dir_obs=dir_obs, dir_plot=dir_plot,                         $
                 extra_plt_info=extra_plt_info, TypePlotFile=TypePlotFile,   $
                 UseTimePlotName=UseTimePlotName,                            $
                 CharSizeLocal = CharSizeLocal, unitlog=unitlog,             $
                 NameSat=NameSat, xs_map=xs_map,ys_map=ys_map

  NameSub = 'compare_EUV'

  if (not keyword_set(UseTimePlotName)) then UseTimePlotName = 0
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
  imagefov = [-1200,1200]
  plotmin  = 1.e-2

  get_euv_info, NameSat = NameSat, SourceName = SourceName,   $
                InstName = InstName, DetName = DetName,       $
                titlePlt = titlePlt, titleModel = titleModel, $
                InstPlt = InstPlt

  set_FilePlotName, TimeEvent, InstPlt = InstPlt,                   $
                    UseTimePlotName = UseTimePlotName,              $
                    TimeStrFile = TimeStrFile, fileplot = fileplot, $
                    dir_plot = dir_plot

  ;;---------------------------------------------------------------------------
  ;; Download the data

  download_images, TimeEvent = TimeEvent, CaseInst = 'euv',      $
                   SourceName = SourceName, InstName = InstName, $
                   DetName = DetName, InstPlt = InstPlt,         $
                   filename_I = filename_I, unitlog = unitlog,   $
                   dir_obs = dir_obs, TimeStrFile = TimeStrFile

  ;;---------------------------------------------------------------------------
  ;; prepare the image for plotting

  process_euv, filename_I(0), DetName = DetName, euv_map = euv171_map, $
               InstName = InstName, xy_map = xs_map, ys_map = ys_map,  $
               index=index

  process_euv, filename_I(1), DetName = DetName, euv_map = euv195_map, $
               InstName = InstName, xy_map = xs_map, ys_map = ys_map

  process_euv, filename_I(2), DetName = DetName, euv_map = euv284_map, $
               InstName = InstName, xy_map = xs_map, ys_map = ys_map


  ;; Save for IDL comparisons
  XPixelToRadius = (index.naxis1/xs_map) / index.rsun
  YPixelToRadius = (index.naxis2/ys_map) / index.rsun

  EventTime = strmid(TimeEvent,0,19)
  EventTime=repstr(EventTime,'/','_')
  EventTime=repstr(EventTime,':','_')

  ObsFileName=dir_obs+'/'+InstPlt+'_Observations_'+EventTime+'.out'

  ;; only save the Observations in BATSRUS format if the file
  ;; does not exist
  if (file_test(ObsFileName) ne 1) then begin
     print,'Saving Observations in BATSRUS format'
     nw = 3
     w  = fltarr(nx,ny,nw)
     x  = fltarr(nx,ny,2)
     w(*) =[[euv171_map.data],[euv195_map.data], [euv284_map.data]]
     varname  = ['x','y','EUV:171','EUV:195','EUV:284']
     x(*,*,0) = ((findgen(nx) - nx/2)*XPixelToRadius) # (fltarr(ny) + 1)
     x(*,*,1) = (fltarr(nx) + 1) # ((findgen(ny) - ny/2)*YPixelToRadius)
     it = 0.
     time = 0.0

     ;;Save Observations in BATSRUS format
     save_pict,ObsFileName,InstPlt+'_Observations_data',varname,w,x
  endif

  ;;---------------------------------------------------------------------------
  ;; processing the swmf data
  rSizeImage=1.98
  dxy = rSizeImage*2000./nx

  make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='171',    $
                     nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,         $
                     map_out =euv171_image
  make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='195',     $
                     nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,          $
                     map_out =euv195_image
  make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='284',     $
                     nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,          $
                     map_out =euv284_image

  ;;---------------------------------------------------------------------------
  ;; plotting
  nx_plot=3 & ny_plot=2
  !p.multi=[0,3,2]

  case TypePlotFile of
     'png': begin
        set_plot,'X'

        cal_pos,nx_plot,ny_plot,y_x_ratio=y_x,x1=x1,y1=y1,x2=x2,y2=y2,$
                tm=0.1,dy=0,dx=0.5,lm=0.5

        wdef,4,1200,900
     end
     'eps': begin
        set_plot,'PS'
        device,/encapsulated
        device,filename=fileplot+extra_plt_info+'.eps',/color,bits_per_pixel=8
        device,xsize=20,ysize=14

        cal_pos,nx_plot,ny_plot,y_x_ratio=y_x,x1=x1,y1=y1,x2=x2,y2=y2, $
                tm=-0.5,dy=-0.5,dx=0.5,lm=0.8,bm=0.3
     end
  endcase        

  ;; ----------------------- 171 ---------------------------------------
  eit_colors,171
  pos=[x1[0],y1[1],x2[0],y2[1]]
  plotmax=max([max(euv171_map.data),max(euv171_image.data)])
  plot_map_local, euv171_image, position=pos, charsize=CharSizeLocal,  $
                  xrange=imagefov, yrange=imagefov,                    $
                  dmin=plotmin, dmax=plotmax,                          $
                  title=titleModel+' 171', xtitle=''

  pos=[x1[0],y1[0],x2[0],y2[0]]
  plot_map_local, euv171_map, position=pos, charsize=CharSizeLocal,    $
                  xrange=imagefov, yrange=imagefov,                    $
                  dmin=plotmin,dmax=plotmax,                           $
                  title=titlePlt+' 171'


  ;; ----------------------- 195 ---------------------------------------
  eit_colors,195
  pos=[x1[1],y1[1],x2[1],y2[1]]
  plotmax=max([max(euv195_map.data),max(euv195_image.data)])
  plot_map_local, euv195_image, position=pos, charsize=CharSizeLocal,  $
                  xrange=imagefov, yrange=imagefov,                    $
                  dmin=plotmin,dmax=plotmax,                           $
                  title=titleModel+' 195', xtitle='', ytitle=''

  pos=[x1[1],y1[0],x2[1],y2[0]]
  plot_map_local, euv195_map, position=pos, charsize=CharSizeLocal,    $
                  xrange=imagefov, yrange=imagefov,                    $
                  dmin=plotmin,dmax=plotmax,                           $
                  title=titlePlt+' 195', ytitle=''

  ;; ----------------------- 284 ---------------------------------------
  eit_colors,284
  pos=[x1[2],y1[1],x2[2],y2[1]]
  plotmax=max([max(euv284_map.data),max(euv284_image.data)])
  plot_map_local, euv284_image, position=pos, charsize=CharSizeLocal,  $
                  xrange=imagefov, yrange=imagefov,                    $
                  dmin=plotmin, dmax=plotmax,                          $
                  title=titleModel+' 284', xtitle='', ytitle=''

  pos=[x1[2],y1[0],x2[2],y2[0]]
  plot_map_local, euv284_map, position=pos, charsize=CharSizeLocal,    $
                  xrange=imagefov, yrange=imagefov,                    $
                  dmin=plotmin, dmax=plotmax,                          $
                  title=titlePlt+' 284',ytitle=''

  case TypePlotFile of
     'png': begin
        imager=tvrd(true=1)
        write_png,fileplot+'.png',imager,r,g,b

        ;; reset
        wdelete,4
     end
     'eps': begin
        device,/close
        
        ;; reset window
        set_plot,'X'
     end
  endcase

  !p.multi=0
  loadct,0
  
  printf,unitlog,''
  printf, unitlog,'euv171: max(obs), max(swmf) = ',$
          max(euv171_map.data), max(euv171_image.data)
  printf, unitlog,'euv171: min(obs), min(swmf) = ', $
          min(euv171_map.data), min(euv171_image.data)
  printf, unitlog,'euv195: max(obs), max(swmf) = ',$
          max(euv195_map.data),max(euv195_image.data)
  printf, unitlog,'euv195: min(obs), min(swmf) = ',$
          min(euv195_map.data),min(euv195_image.data)
  printf, unitlog,'euv284: max(obs), max(swmf) = ',$
          max(euv284_map.data),max(euv284_image.data)
  printf, unitlog,'euv284: min(obs), min(swmf) = ',$
          min(euv284_map.data),min(euv284_image.data)

  printf,unitlog,''
  printf,unitlog,'Quantify LOS:'
  printf,unitlog,'EUV:171'
  quantify_los,euv171_map,euv171_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
            unitlog=unitlog
  printf,unitlog,''
  printf,unitlog,'EUV:195'
  quantify_los,euv195_map,euv195_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
            unitlog=unitlog
  printf,unitlog,''
  printf,unitlog,'EUV:284'
  quantify_los,euv284_map,euv284_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
            unitlog=unitlog
  printf,unitlog,''
  out: printf, unitlog, NameSub, ' is finished.'
  printf, unitlog, ''
end
