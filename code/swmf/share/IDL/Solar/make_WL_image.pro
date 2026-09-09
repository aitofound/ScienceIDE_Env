; This code plots Synthetic WL images
; from the model.
; User needs to set the directory name
; either in SWQU format, or the
; full location until the SC directory
; calling - make_WL_image,CommonDir='${DIRPATH}/'
; For eg, .r make_WL_image
; make_WL_image,CommonDir='/Users/name/Desktop/Results/'
; Keywords available to set - Instruments, Ratio, RD or both, Movie
; Last Update:
; NishthaS:
; Dec 29,2023
; Mar 15. 2025 - creates Running Diff images
;              - makes movies
; Dec 21, 2025 - include reading .out files
; Still left to do fk corona for .out files

function set_inst,file
  tmp=strsplit(file,'/',/extract,count=count)
  tmp1 = strsplit(tmp[count-1],'_',/extract) ;last one
  SatName = tmp1[1]
  InstName = tmp1[2] ; INSTRUMENT NAME
  ; outfile,colortable,occulter,size,xyrange
  if SatName eq 'soho' then begin
     if InstName eq 'c2' then begin
        inst_file = ['LASCO_C2_SIM_','C2','2.2','12.','6']
     endif
     if InstName eq 'c3' then begin
        inst_file = ['LASCO_C3_SIM_','C3','4.0','64.','20.']
     endif
  endif
  if SatName eq 'sta' then begin
     if InstName eq 'cor1' then begin
        inst_file = ['STEREOA_COR1_SIM_','COR1A',$
                     '1.6','8','4']
     endif
     if InstName eq 'cor2' then begin
        inst_file = ['STEREOA_COR2_SIM_','COR2A',$,
                     '4.0','30','15']
     endif
  endif
  if SatName eq 'stb' then begin
     if InstName eq 'cor1' then begin
        inst_file = ['STEREOB_COR1_SIM_','COR1B',$
                     '1.6','8','4']
     endif
     if InstName eq 'cor2' then begin
        inst_file = ['STEREOB_COR2_SIM_','COR2B',$
                     '4.0','30','15']
     endif
  endif
  return,inst_file
end
;; Returns all details pertaining to the instrument
;; name, instrument, rocc, dxy, xyrange

;; Func to read the wl los image, only .dat file format
function read_swmf_dat,file,los_size,FileType ; los_file, size, type
  if (FileType eq 1) then begin
     print,'Reading File = ',file
     restore,'los_template.sav'
     data = read_ascii(file,template=template)
     xx=data.field1
     yy=data.field2
     wl=data.field3
; check size : 512 (newer data) or 300  
;  tmp = size(xx)
;  if (tmp[1]/512. eq 512) then nsize = 512 else nsize = 300
     nsize=los_size
     wl_image=fltarr(nsize,nsize)
     nn=long(0)
     for i=0, nsize-1 do begin
        for j=0, nsize-1 do begin
           wl_image[i,j]=wl[nn]
           nn=nn+1
        endfor
     endfor
     return,wl_image
  endif
end
; returns the white-light image

function read_swmf_out,file,los_size,FileType
  if (FileType eq 2) then begin
     common getpict_param, filename
     common file_head
     common plot_data, grid, x, w
     filename=file[0]
     read_data
     nvars = 0
     varnames = ''
     ObsXyz = eqpar
     nsize = nx[0]
     ny0 = nx[1]

     xx=x(*,*,0)
     yy=x(*,*,1)
     wl=w(*,*,0)
 ;    pb=w(*,*,1)
     wl_image = wl
     wl_image[0,*]=0
;     w = wl_image
;     plot_data
;     wl_image=fltarr(nsize,nsize)
;     nn=long(0)
;     for i=0, nsize-1 do begin
;        for j=0, nsize-1 do begin
;           wl_image[i,j]=wl[nn]
 ;          nn=nn+1
 ;       endfor
;    endfor
     print,'Hellow I am here reading .otu file'
     return,wl_image
  endif
end

function fk_corona,file,los_size,FileType ; For background only, file, inst_info
  nsize=los_size
  print,'FK corona for background = ',file
  if (FileType eq 1) then begin ;; dat file
     restore,'los_template.sav'
; Read background & calculate f-k corona contribution
     data = read_ascii(file,template=template)
     xx0=data.field1
     yy0=data.field2
     wl0=data.field3
  endif
  rr0=sqrt(xx0^2+yy0^2)
  cos2t0=2.0*(xx0/rr0)^2-1.0
  bk0=rr0^(-3.07)+82.0*rr0^(-6.95)
  bf0_temp=rr0^(0.22*cos2t0-2.47)
  index0=where(abs(rr0-2.3) lt 0.01)
  bk0_avg=average(bk0[index0])
  bf0_avg=average(bf0_temp[index0])
  cc=bk0_avg/bf0_avg
  bf0=bf0_temp*cc
  ratio_fk0=bf0/bk0
  fk_image0=fltarr(nsize,nsize)
  nn=long(0)
  for i=0, nsize-1 do begin
     for j=0, nsize-1 do begin
        fk_image0[i,j]=ratio_fk0[nn]
        nn=nn+1
     endfor
  endfor
  return,fk_image0
end
; return F & K corona contribution for the background

function extract_time,file      ; Extract time of image
  tmp1=strsplit(file,'/',/extract,count=count)
  tmp = strsplit(tmp1[count-1],'_',/extract)
  time = strsplit(tmp[4],'t',/extract)
;  print,'time=',FIX(time/10000.)
  timing=FIX(time/10000.)        ; Hrs
  tmp1=FIX(time MOD 10000.)
;  print,'tmp1 =', FIX(time MOD 10000.)
  mins=tmp1/100.                 ;Mins
  secs=tmp1 MOD 100.
  plot_time = $
     String(timing,Format='%02d')+'Hr'+STRING(mins,FORMAT='%02d')+'Mins'
  return,plot_time
end
; return the time of the image

pro make_plot,dimage,inst,time,outfolder,los_size,DoRatio,DoRD,FileType
  if (DoRatio eq 1) then  print,'Plotting Ratio' else print,'Plotting RD'
  ndxy=los_size
  case inst[1] of
     'C2': BEGIN
        range=FLOAT(inst[4])
        prange=[min(dimage>0),max(dimage)]
        prange_Rat = [0.96,1.25]
        prange_RD = [0.0,0.001]
     END
     'C3': BEGIN
        range=FLOAT(inst[4])
        prange=[min(dimage>0),max(dimage)]
        prange = [0.96,max(dimage)]
        prange_Rat = [0.96,1.25]
        prange_RD = [0.0,1e-5]
     END
     'COR1A': BEGIN
        range=FLOAT(inst[4])
        prange=[min(dimage>0),max(dimage)]
        prange = [0.96,max(dimage)]
        prange_Rat = [0.96,1.25]
        prange_RD = [0.96,1.25]
     END
     'COR2A': BEGIN
        range=FLOAT(inst[4])
        prange=[min(dimage>0),max(dimage)]
        prange = [0.96,max(dimage)]
        prange_Rat = [0.96,1.25]
        prange_RD = [0.96,1.25]
     END
     'COR1B': BEGIN
        range=FLOAT(inst[4])
        prange=[min(dimage>0),max(dimage)]
        prange = [0.96,max(dimage)]
        prange_Rat = [0.96,1.25]
        prange_Rat = [0.96,1.25]
     END
     'COR2B': BEGIN
        range=FLOAT(inst[4])
        prange=[min(dimage>0),max(dimage)]
        prange = [0.96,max(dimage)]
        prange_Rat = [0.96,1.25]
        prange_RD = [0.96,1.25]
     END
  endcase
  dxy = inst[3]
;  MAP=MAKE_MAP(DIMAGE,XC=0,YC=0,DX=dxy/ndxy,DY=dxy/ndxy)
;  thisdevice = !d.name
  if DoRatio then begin
     prange = prange_Rat
     plot_type = 'Ratio_'
  endif
  if DoRD then begin
     prange = prange_RD
     plot_type = 'RD_'
  endif
  MAP=MAKE_MAP(DIMAGE,XC=0,YC=0,DX=dxy/ndxy,DY=dxy/ndxy)
  thisdevice = !d.name
  set_plot,'Z',/copy
  device,set_resolution=[800,800],z_buffer=0
  erase
  PLOT_MAP,MAP,TITLE='AWSoM CME SIM '+inst[1]+' T= '+trim(time), $
           CHARSIZE=2.0,XTITLE='SOLAR RADII',YTITLE='SOLAR RADII',$
           dmin=prange[0],dmax=prange[1], xthick=2,charthick=2,$
           ythick=2,xrange=[-range,range],yrange=[-range,range],/log
  plot_map_colorbar,prange,charsize=2.,charthick=2
  tvcircle,inst[2],0,0,color=0,/data,/fill
  snapshot=TVRD()
  TVLCT,r,g,b,/get
  device,z_buffer=1
  set_plot,thisdevice
  image=BYTARR(3,800,800)
  image[0,*,*] =r[snapshot]
  image[1,*,*] =g[snapshot]
  image[2,*,*] =b[snapshot]
;  IMAGER=TVRD(TRUE=1)
  WRITE_PNG,outfolder+inst[0]+STRING(plot_type)+TRIM(time)+'.png',image ;,R,G,B
end

pro sim_movie,instrument,outfolder,DoRatio,DoRD
  framerate=5
  xsize=800
  ysize=800
  if DoRatio then begin
     video_file=outfolder+instrument+'Ratio_movie.mp4'
     files_img = File_Search(outfolder+instrument+'Ratio*.png',Count=filecnt)
     video_img = IDLffVideoWrite(video_file)
     stream_img = video_img.AddVideoStream(xsize,ysize,framerate)
;  files_img = File_Search(outfolder+instrument+'*.png',Count=filecnt)
     for k=0,filecnt-1 do begin
        image_img = read_png(files_img[k])
        if (n_elements(image_img) eq 0) then break
        voidimg = video_img.Put(stream_img,image_img)
     endfor
     video_img.cleanup
     video_img=0
     image_img=0
  endif  
  if DoRD then begin
     video_file=outfolder+instrument+'RD_movie.mp4'
     files_img = File_Search(outfolder+instrument+'RD*.png',Count=filecnt)
     video_img = IDLffVideoWrite(video_file)
     stream_img = video_img.AddVideoStream(xsize,ysize,framerate)
;  files_img = File_Search(outfolder+instrument+'*.png',Count=filecnt)
     for k=0,filecnt-1 do begin
        image_img = read_png(files_img[k])
        if (n_elements(image_img) eq 0) then break
        voidimg = video_img.Put(stream_img,image_img)
     endfor
     video_img.cleanup
     video_img=0
     image_img=0
  endif
end

;; read los*.out file, FielType = 2
;; Have to include fk corona
pro save_image_out,file,outfolder,nlos,los_size,DoRatio,DoRD,FileType
  DoSaveMovie = 1
  if (FileType eq 2) then begin
     if (nlos ne 0) then begin
        file_info = set_inst(file[0])
        print,'No. of LOS files found for '+file_info[1]+'= ',nlos
        nsize = los_size
;; Background
        wl_image0 = read_swmf_out(file[0],nsize,FileType)
        tmpsize = size(wl_image0)
        los_size = float(tmpsize[1])
        print,'Read the background',size(wl_image0)
        wl_image_prev = wl_image0
;        fk_image0 = fk_corona(file[0],nsize,FileType)
; Next set of files
        for p=1,nlos-1 do begin
           wl_image=read_swmf_out(file[p],nsize,FileType)
           if DoRatio then begin
              dimage_Rat=wl_image/wl_image0
              dimage_Ratio = dimage_Rat
;              dimage_Ratio = (dimage_Rat+fk_image0)/(1.0+fk_image0)
              print,'Ratio min,max =',min(dimage_Ratio),max(dimage_Ratio)
              bad=WHERE(FINITE(dimage_Ratio, /NAN))
              dimage_Ratio[bad]=-999
              time_tmp = extract_time(file[p])
              c=0
              make_plot,dimage_Ratio,file_info,time_tmp,outfolder,los_size,DoRatio,c,FileType
           endif
           if DoRD then begin
              dimage_RD=wl_image-wl_image_prev
              wl_image_prev = wl_image
              print,'RD min,max =',min(dimage_RD),max(dimage_RD)
              bad=WHERE(FINITE(dimage_RD, /NAN))
              dimage_RD[bad]=-999
              time_tmp = extract_time(file[p])
              c=0
              make_plot,dimage_RD,file_info,time_tmp,outfolder,los_size,c,DoRD
           endif
        endfor
     endif
  endif
end

function get_los_size,file
  common getpict_param, filename
  common file_head
  common plot_data, grid, x, w
  filename=file
  read_data
  nvars = 0
  varnames = ''
  ObsXyz = eqpar
  nsize = nx[0]
  return,nsize
end

;; reads los*.dat file, and makes map, ratio & Rd
pro save_image_dat,file,outfolder,nlos,los_size,DoRatio,DoRD,FileType
  DoSaveMovie = 1
  if (nlos ne 0) then begin
     file_info = set_inst(file[0])
     print,'No. of LOS files found for '+file_info[1]+'= ',nlos
     nsize = los_size
;; Background
     wl_image0 = read_swmf_dat(file[0],nsize,FileType)
     wl_image_prev = wl_image0
     fk_image0 = fk_corona(file[0],nsize,FileType)
;     time_tmp = '00'
;     make_plot,wl_image0,file_info,time_tmp,outfolder,los_size,DoRatio,0
;     make_plot,wl_image0,file_info,time_tmp,outfolder,los_size,0,DoRD
; Next set of files
     for p=1, nlos-1 do begin
        wl_image=read_swmf_dat(file[p],nsize,FileType)
        if DoRatio then begin
           dimage_Rat=wl_image/wl_image0
           dimage_Ratio = (dimage_Rat+fk_image0)/(1.0+fk_image0)
           print,'Ratio min,max =',min(dimage_Ratio),max(dimage_Ratio)
           bad=WHERE(FINITE(dimage_Ratio, /NAN))
           dimage_Ratio[bad]=-999
           ;print,bad
           time_tmp = extract_time(file[p])
           c=0
           make_plot,dimage_Ratio,file_info,time_tmp,outfolder,los_size,DoRatio,c
        endif
        if DoRD then begin
           dimage_RD=wl_image-wl_image_prev
           wl_image_prev = wl_image
           print,'RD min,max =',min(dimage_RD),max(dimage_RD)
           bad=WHERE(FINITE(dimage_RD, /NAN))
           dimage_RD[bad]=-999
           time_tmp = extract_time(file[p])
           c=0
           make_plot,dimage_RD,file_info,time_tmp,outfolder,los_size,c,DoRD
        endif
     endfor
     if DoSaveMovie then $
        sim_movie,file_info[0],outfolder,1,1
  endif else begin
     print,'No files found'
  endelse
end

pro make_WL_image,CommonDir=CommonDir
  DoC2 = 1
  DoC3 = 1
  DoCOR1A = 1
  DoCOR2A = 1
  DoCOR1B = 1
  DoCOR2B = 1
  DoRatio = 1
  DoRD = 0

  if (not keyword_set(CommonDir)) then CommonDir = 'Results/'
;; CommonDir = '/Users/nishthas/Desktop/Results/run_test/'
  nfolders = file_search(CommonDir+'run*_restart*/run*/SC/',count=nfiles)
  if (nfiles eq 0) then begin
     nfolders = file_search(CommonDir+'run*/SC/',count=nfilestmp)
     nfiles = nfilestmp
  endif
  if (nfiles eq 0) then begin
     nfolders = file_search(CommonDir+'SC/',count=nfilestmp)
     nfiles = nfilestmp
  endif
  print,'No. of folders found =',nfiles
  print,'Run Folders Found = ',nfolders
  if (nfiles eq 0) then print,'No Run folders found'

;; Loop through all folders & read the background file
  for m=0,nfiles-1 do begin
     print,'----------'
     print,'Looping through =',nfolders[m]
     print,'----------'
;; Creates an output folder if not present already
     simdir_tmp = nfolders[m]
     if (file_search(simdir_tmp+'/output/') ne simdir_tmp+'/output/') then $
        file_mkdir,simdir_tmp+'/output/'
     simdir = simdir_tmp+'/'
     outdir = simdir+'output/'
     print,'Simulation Folder',simdir
;; Check if .dat or .out files will need to be read
;; done for each instrument type

;; C2
     if DoC2 then begin
        loadct,3
        filename = file_search(simdir+'los_soho_c2_*.dat',count=nlos)
        if nlos ne 0 then begin
           FileType = 1 ;; for dat files
           restore,'los_template.sav'
           data_tmp = read_ascii(filename[0],template=template)  ;; background
           xx=data_tmp.field1
           ;; check size : 512 (newer data) or 300  
           tmp = size(xx)
           los_size = sqrt(tmp[1])
           ;; send all los.dat filenames to make ratio & RD images for this inst
           save_image_dat,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endif else begin
           filename = file_search(simdir+'los_soho_c2_*.out',count=nlos)
           FileType = 2 ;; for out fils
           los_size=get_los_size(filename[0])
           save_image_out,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endelse
     endif
; C3
     if DoC3 then begin
        loadct,1
        filename= file_search(simdir+'los_soho_c3_*.dat',count=nlos)
        if nlos ne 0 then begin
           restore,'los_template.sav'
           data_tmp = read_ascii(filename[0],template=template)
           xx=data_tmp.field1
; check size : 512 (newer data) or 300  
           tmp = size(xx)
           los_size = sqrt(tmp[1])
           save_image_dat,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endif else begin
           filename = file_search(simdir+'los_soho_c3_*.out',count=nlos)
           FileType = 2 ;; for out files
           los_size=get_los_size(filename[0])
           save_image_out,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endelse
     endif
     
; NEXT COR1A
     if DoCOR1A then begin
        secchi_colors,'cor1',/load
        filename = file_search(simdir+'los_sta_cor1_*.dat',count=nlos)
        if nlos ne 0 then begin
           restore,'los_template.sav'
           data_tmp = read_ascii(filename[0],template=template)
           xx=data_tmp.field1
; check size : 512 (newer data) or 300  
           tmp = size(xx)
           los_size = sqrt(tmp[1])
           print,'Size is =',los_size
           save_image_dat,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endif else begin
           filename = file_search(simdir+'los_sta_cor1_*.out',count=nlos)
           FileType = 2 ;; for out fils
           los_size=get_los_size(filename[0])
           save_image_out,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endelse
     endif

; NEXT COR2A
     if DoCOR2A then begin
        secchi_colors,'cor2',/load
        filename = file_search(simdir+'los_sta_cor2_*.dat',count=nlos)
        if nlos ne 0 then begin
           restore,'los_template.sav'
           data_tmp = read_ascii(filename[0],template=template)
           xx=data_tmp.field1
; check size : 512 (newer data) or 300  
           tmp = size(xx)
           los_size = sqrt(tmp[1])
           save_image_dat,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endif else begin
           filename = file_search(simdir+'los_sta_cor2_*.out',count=nlos)
           FileType = 2 ;; for out fils
           los_size=get_los_size(filename[0])
           save_image_out,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endelse
     endif
     
     if DoCOR1B then begin
; NEXT COR1B
        secchi_colors,'cor1',/load
        filename = file_search(simdir+'los_stb_cor1_*.dat',count=nlos)
        if nlos ne 0 then begin
           restore,'los_template.sav'
           data_tmp = read_ascii(filename[0],template=template)
           xx=data_tmp.field1
; check size : 512 (newer data) or 300  
           tmp = size(xx)
           los_size = sqrt(tmp[1])
           save_image_dat,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endif else begin
           filename = file_search(simdir+'los_stb_cor1_*.out',count=nlos)
           FileType = 2 ;; for out fils
           los_size=get_los_size(filename[0])
           save_image_out,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endelse
     endif
     
     if DoCOR2B then begin
; NEXT COR2B
        secchi_colors,'cor2',/load
        filename = file_search(simdir+'los_stb_cor2_*.dat',count=nlos)
        if nlos ne 0 then begin
           restore,'los_template.sav'
           data_tmp = read_ascii(filename[0],template=template)
           xx=data_tmp.field1
; check size : 512 (newer data) or 300  
           tmp = size(xx)
           los_size = sqrt(tmp[1])
           save_image_dat,filename,outdir,nlos,los_size,DoRatio,DoRD
        endif else begin
           filename = file_search(simdir+'los_stb_cor2_*.out',count=nlos)
           FileType = 2 ;; for out fils
           los_size=get_los_size(filename[0])
           save_image_out,filename,outdir,nlos,los_size,DoRatio,DoRD,FileType
        endelse
     endif
  endfor
end

;; Var info
;; nfolders - folders that have los files
;; nfiles   - number of folders to loop through
;; nlos     - number of los* files for a particular instrument
