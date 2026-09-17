pro compare_AIA, TimeEvent=TimeEvent, varnames=varnames, nvars=nvars,        $
                 dataSim=dataSim, nx=nx, ny=ny,                              $
                 dir_obs=dir_obs, dir_plot=dir_plot,                         $
                 extra_plt_info=extra_plt_info, TypePlotFile=TypePlotFile,   $
                 UseTimePlotName=UseTimePlotName,                            $
                 CharSizeLocal = CharSizeLocal, unitlog=unitlog,             $
                 NameSat = NameSat, xs_map=xs_map,ys_map=ys_map,             $
                 DoIDLCompare=DoIDLCompare, file_sim=file_sim

  NameSub = 'compare_AIA'

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
  count = 0
  InstPlt = 'aia'

  set_FilePlotName, TimeEvent, UseTimePlotName = UseTimePlotName,    $
                    TimeStrFile = TimeStrFile, fileplot = fileplot,  $
                    dir_plot = dir_plot, InstPlt = InstPlt
  
  ;;-------------------------------------------------------------------------
  ; checking to see if images need to be downloaded or already exist
  download_images, TimeEvent = TimeEvent, CaseInst = 'aia',       $
                   InstPlt = InstPlt, filename_I = filename_I,    $
                   dir_obs = dir_obs, TimeStrFile = TimeStrFile,  $
                   unitlog = unitlog
     ;;------------------------------------------------------------------------
  ;;if DoIDLCompare!=0 then check if the observation output file
  ;;already exists
  if DoIDLCompare eq 1 then begin
     TimeEventTmp = strmid(TimeEvent,0,19)
     TimeEventTmp=repstr(TimeEventTmp,'/','_')
     TimeEventTmp=repstr(TimeEventTmp,':','_')
     file_obs=file_search(dir_obs+'/AIA_observations_'+TimeEventTmp+'.out',count=count)
     if count ne 0 then begin
        print,'AIA Observations file exists at: ',file_obs
        compare_IDL_plot,file_obs,file_sim,fileplot,unitlog=unitlog
     endif
  endif
  
  if (count eq 0) or (DoIDLCompare eq 0) then begin
     ;;if Observations are not saved yet, or los*.dat is to be compared
     ;;download data & save the data for future comparisons
     ;; prepare the image for plotting
     
     process_aia, filename_I(0), aia_map  = aia94_map,  $
                  xy_map = xs_map, ys_map = ys_map
     
     process_aia, filename_I(1), aia_map  = aia131_map, $
                  xy_map = xs_map, ys_map = ys_map
     
     process_aia, filename_I(2), aia_map  = aia171_map, $
                  xy_map = xs_map, ys_map = ys_map, index = index
     
     XPixelToRadius = (index.naxis1/xs_map) / index.r_sun
     YPixelToRadius = (index.naxis2/ys_map) / index.r_sun
     
     process_aia, filename_I(3), aia_map  = aia193_map, $
                  xy_map = xs_map, ys_map = ys_map
     
     process_aia, filename_I(4), aia_map  = aia211_map, $
                  xy_map = xs_map, ys_map = ys_map
     
     process_aia, filename_I(5), aia_map  = aia304_map, $
                  xy_map = xs_map, ys_map = ys_map
     
     process_aia, filename_I(6), aia_map  = aia335_map, $
                  xy_map = xs_map, ys_map = ys_map

     ;; Save for IDL comparisons
     EventTime = strmid(TimeEvent,0,19)
     EventTime=repstr(EventTime,'/','_')
     EventTime=repstr(EventTime,':','_')

     ObsFileName=dir_obs+'/AIA_Observations_'+EventTime+'.out'
     
     ;; only save the Observations in BATSRUS format if the file does
     ;; not exist
     if (file_test(ObsFileName) ne 1) then begin
        print,'Saving Observations in BATSRUS format'
        nw = 7
        w  = fltarr(nx,ny,nw)
        x  = fltarr(nx,ny,2)
        w(*) =[[aia94_map.data],[aia131_map.data], [aia171_map.data],$
               [aia193_map.data],[aia211_map.data], [aia304_map.data],$
               [aia335_map.data]]
        varname  = ['x','y','AIA:94','AIA:131','AIA:171','AIA:193','AIA:211','AIA:304','AIA:335']
        x(*,*,0) = ((findgen(nx) - nx/2)*XPixelToRadius) # (fltarr(ny) + 1)
        x(*,*,1) = (fltarr(nx) + 1) # ((findgen(ny) - ny/2)*YPixelToRadius)
        it = 0.
        time = 0.0
        ;;Save Observations in BATSRUS format
        save_pict,ObsFileName,'AIA_Observations_data',varname,w,x
     endif

     if (DoIDLCompare eq 1) then begin
        compare_IDL_plot,ObsFileName,file_sim,fileplot,unitlog=unitlog
     endif else begin
        ;;---------------------------------------------------------------------
        ;; processing the swmf data
        rSizeImage=1.98
        dxy = rSizeImage*2000./nx
        
        make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='aia:94', $
                           nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,         $
                           map_out =aia94_image
        make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='aia:131',$
                           nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,         $
                           map_out =aia131_image
        make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='aia:171',$
                           nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,         $
                           map_out =aia171_image
        make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='aia:193',$
                           nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,         $
                           map_out =aia193_image
        make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='aia:211',$
                           nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,         $
                           map_out =aia211_image
        make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='aia:304',$
                           nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,         $
                           map_out =aia304_image
        make_map_swmf_data,dataSim=dataSim, varnames=varnames, namevar='aia:335',$
                           nx=nx, ny = ny, dxy=dxy, TimeEvent=TimeEvent,         $
                           map_out =aia335_image
        ;;------------------------------------------------------------------

        ;; plotting
        
        case TypePlotFile of
           'png': begin
              set_plot,'X'
              
              nx_plot=3
              ny_plot=2
              !p.multi=[0,3,2]
              cal_pos,nx_plot,ny_plot,y_x_ratio=y_x,x1=x1,y1=y1,x2=x2,y2=y2,$
                      tm=0.1,dy=0,dx=0.5,lm=0.5
              
              wdef,2,800,600
              
              ny_offset = 0
           end
           'eps': begin
              set_plot,'PS'
              device,/encapsulated
              device,filename=fileplot + extra_plt_info+'.eps', $
                     /color,bits_per_pixel=8
              device,xsize=20,ysize=26
              
              nx_plot=3
              ny_plot=4
              !p.multi=[0,3,4]
              cal_pos,nx_plot,ny_plot,y_x_ratio=y_x,x1=x1,y1=y1,x2=x2,y2=y2,$
                      tm=-0.5,dy=-0.5,dx=0.5,lm=0.8,bm=0.3
              
              ny_offset = 2
           end
        endcase
        
        ;;-------------------------  94 --------------------------------------
        aia_lct,wavelnth=94,/load
        pos=[x1[0],y1[1+ny_offset],x2[0],y2[1+ny_offset]]
        plotmax=max([max(aia94_map.data),max(aia94_image.data)])
        plot_map_local, aia94_image, position=pos, charsize=CharSizeLocal,   $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        xtitle='', title='Model AIA 94'

        pos=[x1[0],y1[0+ny_offset],x2[0],y2[0+ny_offset]]
        plot_map_local, aia94_map, position=pos, charsize=CharSizeLocal,     $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        xtitle='', title='SDO AIA 94'

        ;;-------------------------- 171 --------------------------------------
        aia_lct,wavelnth=171,/load
        pos=[x1[1],y1[1+ny_offset],x2[1],y2[1+ny_offset]]
        plotmax=max([max(aia171_map.data),max(aia171_image.data)])
        plot_map_local, aia171_image, position=pos, charsize=CharSizeLocal,  $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        xtitle='', ytitle='', title='Model AIA 171'

        pos=[x1[1],y1[0+ny_offset],x2[1],y2[0+ny_offset]]
        plot_map_local, aia171_map,position=pos, charsize=CharSizeLocal,     $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        xtitle='', ytitle='',title='SDO AIA 171'

        ;;-------------------------- 193 --------------------------------------
        aia_lct,wavelnth=193,/load
        pos=[x1[2],y1[1+ny_offset],x2[2],y2[1+ny_offset]]
        plotmax=max([max(aia193_map.data),max(aia193_image.data)])
        plot_map_local, aia193_image, position=pos, charsize=CharSizeLocal,  $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov, yrange=imagefov,                    $
                        xtitle='',ytitle='',title='Model AIA 193'

        pos=[x1[2],y1[0+ny_offset],x2[2],y2[0+ny_offset]]
        plot_map_local, aia193_map, position=pos, charsize=CharSizeLocal,    $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        xtitle='',ytitle='',title='SDO AIA 193'

        ;;--------------------------- save png part 1 -----------------------
        case TypePlotFile of
           'png': begin
              imager=tvrd(true=1)
              write_png,fileplot+'_part1'+extra_plt_info+'.png',imager,r,g,b
                                ; wdef,3,1200,900
              wdef,3,800,600
           end
           'eps':
        endcase

        ;;-------------------------- 131 --------------------------------------
        aia_lct,wavelnth=131,/load
        pos=[x1[0],y1[1],x2[0],y2[1]]
        plotmax=max([max(aia131_map.data),max(aia131_image.data)])
        plot_map_local, aia131_image, position=pos, charsize=CharSizeLocal,  $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        xtitle='',title='Model AIA 131'
        pos=[x1[0],y1[0],x2[0],y2[0]]
        plot_map_local, aia131_map, position=pos, charsize=CharSizeLocal,    $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        title='SDO AIA 131'

        ;;-------------------------- 211 ------------------------------------
        aia_lct,wavelnth=211,/load
        pos=[x1[1],y1[1],x2[1],y2[1]]
        plotmax=max([max(aia211_map.data),max(aia211_image.data)])
        plot_map_local, aia211_image, position=pos, charsize=CharSizeLocal,  $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        xtitle='',ytitle='',title='Model AIA 211'
        pos=[x1[1],y1[0],x2[1],y2[0]]
        plot_map_local, aia211_map, position=pos, charsize=CharSizeLocal,    $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        ytitle='',title='SDO AIA 211'

        ;;-------------------------- 335 --------------------------------------
        aia_lct,wavelnth=335,/load
        pos=[x1[2],y1[1],x2[2],y2[1]]
        plotmax=max([max(aia335_map.data),max(aia335_image.data)])
        plot_map_local, aia335_image, position=pos, charsize=CharSizeLocal,  $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        xtitle='',ytitle='',title='Model AIA 335'
        pos=[x1[2],y1[0],x2[2],y2[0]]
        plot_map_local, aia335_map, position=pos, charsize=CharSizeLocal,    $
                        dmin=plotmin, dmax=plotmax,                          $
                        xrange=imagefov,yrange=imagefov,                     $
                        ytitle='',title='SDO AIA 335'

        ;;-------------------------- save ------------------------------------
        case TypePlotFile of
           'png': begin
              imager=tvrd(true=1)
              write_png,fileplot+'_part2'+extra_plt_info+'.png',imager,r,g,b

              ;; reset
              wdelete,2,3
           end
           'eps': begin
              device,/close
              
              ;; reset window
              set_plot,'X'
           end
        endcase

        !p.multi=0
        loadct,0

        ;; Quantitative comparisons:

        printf, unitlog,'aia94: max(obs), max(swmf) = ',$
                max(aia94_map.data), max(aia94_image.data)
        printf, unitlog,'aia94: min(obs), min(swmf) = ', $
                min(aia94_map.data), min(aia94_image.data)
        printf, unitlog,'aia171: max(obs), max(swmf) = ',$ 
                max(aia171_map.data),max(aia171_image.data)
        printf, unitlog,'aia171: min(obs), min(swmf) = ',$
                min(aia171_map.data),min(aia171_image.data)
        printf, unitlog,'aia193: max(obs), max(swmf) = ',$
                max(aia193_map.data),max(aia193_image.data)
        printf, unitlog,'aia193: min(obs), min(swmf) = ',$
                min(aia193_map.data),min(aia193_image.data)
        printf, unitlog,'aia131: max(obs), max(swmf) = ',$
                max(aia131_map.data),max(aia131_image.data)
        printf, unitlog,'aia131: min(obs), min(swmf) =',$
                min(aia131_map.data),min(aia131_image.data)
        printf, unitlog,'aia211: max(obs), max(swmf) = ',$
                max(aia211_map.data),max(aia211_image.data)
        printf, unitlog,'aia211: min(obs), min(swmf) = ',$
                min(aia211_map.data),min(aia211_image.data)
        printf, unitlog,'aia304: max(obs), max(swmf) = ',$
                max(aia304_map.data),max(aia304_image.data)
        printf, unitlog,'aia304: min(obs), min(swmf) = ',$
                min(aia304_map.data),min(aia304_image.data)
        printf, unitlog,'aia335: max(obs), max(swmf) = ',$
                max(aia335_map.data),max(aia335_image.data)
        printf, unitlog,'aia335: min(obs), min(swmf) = ',$
                min(aia335_map.data),min(aia335_image.data)

        printf,unitlog,''
        printf,unitlog,'Quantify LOS:'
        printf,unitlog,'AIA:94'
        quantify_los,aia94_map,aia94_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
                     unitlog=unitlog
        printf,unitlog,''
        printf,unitlog,'AIA:171'
        quantify_los,aia171_map,aia171_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
                     unitlog=unitlog
        printf,unitlog,''
        printf,unitlog,'AIA:193'
        quantify_los,aia193_map,aia193_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
                     unitlog=unitlog

        printf,unitlog,''
        printf,unitlog,'AIA:131'
        quantify_los,aia131_map,aia131_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
                     unitlog=unitlog
        printf,unitlog,''
        printf,unitlog,'AIA:211'
        quantify_los,aia211_map,aia211_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
                     unitlog=unitlog
        printf,unitlog,''
        printf,unitlog,'AIA:304'
        quantify_los,aia304_map,aia304_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
                     unitlog=unitlog
        printf,unitlog,''
        printf,unitlog,'AIA:335'
        quantify_los,aia335_map,aia335_image,DoDiffMap=0,DoRatioMap=0,DoRmse=1,$
                     unitlog=unitlog
     endelse
  endif
   out: printf, unitlog, NameSub, ' is finished.'
   printf, unitlog, ''
end

