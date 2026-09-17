; Make images & movie for observations
; User needs to set the directory/year and the
; instruments to be done.
; Set commondir
; Last Update:
; NishthaS - Dec 30,2023

CommonDir = '~/Work/CME/OBSERVATIONS/2023/'

DoSaveMovieOnly = 0             ;Make a movie from previously saved images
DoSaveImages = 1   ; Make images & movie while saving
DoC2 = 1
DoC3 = 1
DoCOR1A = 1
DoCOR1B = 1
DoCOR2A = 1
DoCOR2B = 1

Inst_array=[DoC2,DoC3,DoCOR1A,DoCOR2A,DoCOR1B,DoCOR2B]
loc = where(Inst_array ne 0)    ; where it is 1
nobs = n_elements(loc)   

for i=0,5 do begin
   if (Inst_array[i] eq 1) then begin
      if (i eq 0) then begin
         ObsDir = CommonDir+'c2/'
         filenames= file_search(ObsDir+'25*.fts',count=nfiles)
         print,'Plotting LASCO C2 Observations'
         Outfile = 'LASCO_C2_OBS_'
         Videofile = 'movie_C2.mp4'
         loadct,3
      endif
      if (i eq 1) then begin
         ObsDir = CommonDir+'c3/'
         filenames= file_search(ObsDir+'35*.fts',count=nfiles)
         print,'Plotting LASCO C3 Observations'
         Outfile = 'LASCO_C3_OBS_'
         Videofile = 'movie_C3.mp4'
         loadct,1
      endif
      if (i eq 2) then begin
         ObsDir = CommonDir+'cor1a/'
         filenames = file_search(ObsDir+'*14c1A*.fts',count=nfiles)
         print,'Plotting STEREO COR1A Observations'
         Outfile = 'STEREO_COR1A_OBS_'
         Videofile = 'movie_COR1A.mp4'
         secchi_colors,'cor1',/load
      endif
      if (i eq 3) then begin
         ObsDir = CommonDir+'cor2a/'
         filenames= file_search(ObsDir+'*14c2A*.fts',count=nfiles)
         print,'Plotting STEREO COR2A Observations'
         Outfile = 'STEREO_COR2A_OBS_'
         Videofile = 'movie_COR2A.mp4'
         secchi_colors,'cor2',/load
      endif
      if (i eq 4) then begin
         ObsDir = CommonDir+'cor1b/'
         filenames = file_search(ObsDir+'*14c1B*.fts',count=nfiles)
         print,'Plotting STEREO COR1B Observations'
         Outfile = 'STEREO_COR1B_OBS_'
         Videofile = 'movie_COR1B.mp4'
         secchi_colors,'cor1',/load
      endif
      if (i eq 5) then begin
         ObsDir = CommonDir+'cor2b/'
         filenames = file_search(ObsDir+'*14c2B*.fts',count=nfiles)
         print,'Plotting STEREO COR2B Observations'
         Outfile = 'STEREO_COR2B_OBS_'
         Videofile = 'movie_COR2B.mp4'
         secchi_colors,'cor2',/load
      endif

      if (nfiles eq 0) then print,'No Files Found'
      
      if (nfiles ne 0 and DoSaveImages) then begin
         back_obs0 = filenames[0]
         print,'Background FileName = ',filenames[0]
         fits2map,back_obs0,back_map0
         print,'Max,Min of data =',max(back_map0.data),min(back_map0.data)
         print,' '
         ;; c2.map0.xc/yc are in arcsec and converted to Rs.
         back_map0 = make_map(back_map0.data,xc=back_map0.xc/960.0,$
                              yc=back_map0.yc/960.0,$
                              dx=back_map0.dx/960.0,dy=back_map0.dy/960.0,$
                              time=back_map0.time,id=back_map0.id)
; Read Remaining files for ratio images
         for k=1,nfiles-1 do begin
            img_obs = filenames(k)
            fits2map,img_obs,img_map
            img_map = make_map(img_map.data,xc=img_map.xc/960.0,$
                               yc=img_map.yc/960.0,$
                               dx=img_map.dx/960.0,dy=img_map.dy/960.0,$
                               time=img_map.time,id=img_map.id)
; extract TIME
            tmp = anytim2utc(img_map.time,/ccsds)
            tmp_date = strmid(tmp,0,10)
            tmp_time = strmid(tmp,11,5)
            print,'Observation File = ',filenames(k),' at Time = ',tmp
            print,'Max,Min of Observation =',max(img_map.data),min(img_map.data)
;     Ratio of CME/BACKGROUND
            wl_ratmap = img_map
            wl_ratmap.data = img_map.data/back_map0.data
;     PLOT OBS
            !p.font=1
            wdef,2,800,800
            prange = [min(wl_ratmap.data),max(wl_ratmap.data)]
            print,'Max, Min of Ratio Map =', prange[1],prange[0]
            print,' '
            prange = [0.96,1.25]
            plot_map,wl_ratmap,CHARSIZE=2.5,XTITLE='SOLAR RADII',$
                     YTITLE='SOLAR RADII',$
                     dmin=prange[0],dmax=prange[1], $
                     xthick=2,charthick=2,ythick=2,xrange=[-6,6],yrange=[-6,6]
            plot_map_colorbar,prange,charsize=2.5,charthick=2
            tvcircle,2.2,0,0,color=0,/data,/fill
            tvcircle,6.0,0,0,color=255,/data,thick=2
            IMAGER = TVRD(TRUE=1)
            WRITE_PNG,ObsDir+Outfile+tmp_date+'T'+tmp_time+'.png',IMAGER,R,G,B
         endfor
      endif                     ; End of saving all images for the instrument
      
;Make a movie with images that have been saved previously OR saved now
      if (nfiles ne 0 and (DoSaveMovieOnly or DoSaveImages))  then begin
         framerate=5
         xsize=800
         ysize=800
         video_file=ObsDir+Outfile+'movie.mp4'
         print,video_file
         video_OBS = IDLffVideoWrite(video_file);,FORMAT='mp4')
         print,video_OBS
         stream_OBS = video_OBS.AddVideoStream(xsize,ysize,framerate)
         print,stream_OBS
         files_OBS = File_Search(ObsDir+Outfile+'*.png',Count=filecnt)
         print,files_OBS
         for k=0,filecnt-1 do begin
            image_OBS = read_png(files_OBS[k])
            if (n_elements(image_OBS) eq 0) then break
            voidOBS = video_OBS.Put(stream_OBS,image_OBS)
         endfor
         video_OBS.cleanup
         video_OBS=0
         image_OBS=0
      endif   
   endif
endfor
end
