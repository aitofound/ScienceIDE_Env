; Compare OBS And model Line of sight images.
; Updates:

; April 27,2022 by Nishtha Sachdeva
; Automate to loop through all observation & simulation files
; to produce Rato images
; May 17,2022 by Nishtha Sachdeva
; Improve readability, Work in progress

DoPlotObs  = 0
DoPlotSim  = 1

if DoPlotObs then begin
   ObsDir = '~/Work/SWQU/CR2161/OBS/C2/'
;  Enter OBS BACKGROUND file
   filenames = file_search(ObsDir+'*.fts',count=nfiles)
   c2_obs0 = filenames[0]
   print,' Background FileName = ',filenames[0]
   fits2map,c2_obs0,c2_map0
   c2_map0 = make_map(c2_map0.data,xc=c2_map0.xc/960.0,yc=c2_map0.yc/960.0,$
                      dx=c2_map0.dx/960.0,dy=c2_map0.dy/960.0,$
                      time=c2_map0.time,id=c2_map0.id)
   for i=1,nfiles-1 do begin
      c2_obs = filenames(i)
      fits2map,c2_obs,c2_map
      c2_map = make_map(c2_map.data,xc=c2_map.xc/960.0,yc=c2_map.yc/960.0,$
                        dx=c2_map.dx/960.0,dy=c2_map.dy/960.0,$
                        time=c2_map.time,id=c2_map.id)
      time = c2_map.time
      tmp_time = strmid(time,12,5)
      print,'LASCO C2 Observation File = ',filenames(i),' at Time = ',tmp_time
      c2_dmap = c2_map
;     Ratio of CME/BACKGROUND
      c2_dmap.data = c2_map.data/c2_map0.data
;     PLOT OBS
      loadct,3
      !p.font=1
      wdef,2,800,800
      prange = [min(c2_dmap.data),max(c2_dmap.data)]
      prange = [0.96,1.25]
      plot_map,c2_dmap,CHARSIZE=2.5,XTITLE='SOLAR RADII',$
               YTITLE='SOLAR RADII',$
               dmin=prange[0],dmax=prange[1], $
               xthick=2,charthick=2,ythick=2,xrange=[-6,6],yrange=[-6,6] 
      plot_map_colorbar,prange,charsize=2.5,charthick=2
      tvcircle,2.2,0,0,color=0,/data,/fill
      tvcircle,6.0,0,0,color=255,/data,thick=2
      IMAGER = TVRD(TRUE=1)
      WRITE_PNG,ObsDir+'LASCO_C2_OBS_'+tmp_time+'.png',IMAGER,R,G,B
   endfor
endif
  
if DoPlotSim then begin
;  template=ascii_template(file)
;  save,template,filename='los_template.sav'
   restore,'los_template.sav'
   simdir =$
      '~/Work/SWQU/CR2161/CR2161/run108_AWSoM_restart_run017_AWSoM/run09/SC/'
   outdir = '~/Work/SWQU/CR2161/output_C2/'
   filename = file_search(simdir+'los_soho_c2*.dat',count=nfiles)
   data0 = read_ascii(filename[0],template=template)
   print,'Simulation Background FileName = ',filename[0]
;  F and K corona  contribution 
   xx0=data0.field1
   yy0=data0.field2
   wl0=data0.field3
   pb0=data0.field4
   rr0=sqrt(xx0^2+yy0^2)
   cos2t0=2.0*(xx0/rr0)^2-1.0
;  All these manipulations are done to bckground
   bk0=rr0^(-3.07)+82.0*rr0^(-6.95)
   bf0_temp=rr0^(0.22*cos2t0-2.47)
   index0=where(abs(rr0-2.3) lt 0.01)
   bk0_avg=average(bk0[index0])
   bf0_avg=average(bf0_temp[index0])
   cc=bk0_avg/bf0_avg
   bf0=bf0_temp*cc
   ratio_fk0=bf0/bk0
   wl_image0=fltarr(300,300)
   fk_image0=fltarr(300,300)
   nn=long(0)
   for i=0, 299 do begin
      for j=0, 299 do begin
         wl_image0[i,j]=wl0[nn]
         fk_image0[i,j]=ratio_fk0[nn]
         nn=nn+1
      endfor
   endfor
; wl_image0[0,*]=0.
; Now, AWSoM CME output
   for k=1,nfiles-1 do begin
      data=read_ascii(filename[k],template=template)
      tmp = strsplit(filename[k],'_',/extract)
      time = strsplit(tmp[8],'t',/extract)
      print,'Simulation file = ',filename[k],fix(time)
      timing=FIX(time)/10000
      tmp1=FIX(time) MOD 10000
      mins=tmp1/100
      plot_time = String(timing)+'Hr'+Trim(STRING(mins))+'Mins'
      xx=data.field1
      yy=data.field2
      wl=data.field3
      pb=data.field4
      wl_image=fltarr(300,300)
      nn=long(0)
      for i=0, 299 do begin
         for j=0, 299 do begin
            wl_image[i,j]=wl[nn]
            nn=nn+1
         endfor
      endfor
; wl_image[0,*]=0.
; Ratio of CME MODEL / BACKGROUND MODEL
      dimage=wl_image/wl_image0
      dimage_cor=(dimage+fk_image0)/(1.0+fk_image0)
; REMOVE NAN'S
      bad=WHERE(FINITE(dimage_cor, /NAN))
      dimage_cor[bad]=-999
      loadct,3
; Determine RANGE from OBSERVATIONS
      prange=[min(dimage_cor>0),max(dimage_cor)]
      prange=[0.96,1.25]
      wdef,3,800,800
      MAP=MAKE_MAP(DIMAGE_COR,XC=0,YC=0,DX=12./300.,DY=12./300.)
      PLOT_MAP,MAP,TITLE='AWSoM CME SIM (LASCO C2)'+' T= '+ $
               trim(plot_time), $
               CHARSIZE=2.5,XTITLE='SOLAR RADII',YTITLE='SOLAR RADII',$
               dmin=prange[0],dmax=prange[1], xthick=2,charthick=2,ythick=2
      plot_map_colorbar,prange,charsize=2.5,charthick=2
      tvcircle,2.2,0,0,color=0,/data,/fill
      IMAGER=TVRD(TRUE=1)
      WRITE_PNG,outdir+'LASCO_C2_SIM_T='+TRIM(plot_time)+'.png',$
                IMAGER,R,G,B
     endfor     
  endif
end
