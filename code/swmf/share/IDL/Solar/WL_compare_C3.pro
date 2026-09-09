; Compare white-light observations with AWSoM LOS outputs for LASCO-C3
; Updates
; Automated on May 17,2022 by Nishtha Sachdeva

DoPlotObs = 0
DoPlotSim = 1
DoPlotDiff = 0

if DoPlotObs then begin
   ObsDir = '~/Work/SWQU/CR2161/OBS/C3/'
;  Enter OBS BACKGROUND file
   filenames = file_search(ObsDir+'*.fts',count=nfiles)
   c3_obs0=filenames[0]
   print,' Background Filename = ', filenames[0]
   ;c3_obs0=ObsDir+'20150315_0206.fts'
   fits2map,c3_obs0,c3_map0
   c3_map0=make_map(c3_map0.data,xc=c3_map0.xc/960.0,yc=c3_map0.yc/960.0,$
                    dx=c3_map0.dx/960.0,dy=c3_map0.dy/960.0,$
                    time=c3_map0.time,id=c3_map0.id)
   for i=1,nfiles-1 do begin
      c3_obs = filenames(i)
      fits2map,c3_obs,c3_map
      c3_map = make_map(c3_map.data,xc=c3_map.xc/960.0,yc=c3_map.yc/960.0,$
                        dx=c3_map.dx/960.0,dy=c3_map.dy/960.0,$
                        time=c3_map.time,id=c3_map.id)
      time = c3_map.time
      tmp_time = strmid(time,12,5)
      print,'LASCO C3 Observation File = ',filenames(i),' at Time = ',tmp_time
      c3_dmap = c3_map
;     Ration of CME/Background
      c3_dmap.data = c3_map.data/c3_map0.data
;     PLOT OBS
      loadct,1                  ; blue CT
      !p.font=1
      wdef,2,800,800
      prange=[0.96,1.03]
      plot_map,c3_dmap,CHARSIZE=2.5,XTITLE='SOLAR RADII',YTITLE='SOLAR RADII',$
               dmin=prange[0],dmax=prange[1], $
               xthick=2,charthick=2,ythick=2,xrange=[-20,20],yrange=[-20,20] 
      plot_map_colorbar,prange,charsize=2.5,charthick=2
      tvcircle,4.0,0,0,color=0,/data,/fill
      tvcircle,20.0,0,0,color=255,/data,thick=2
      IMAGER=TVRD(TRUE=1)
      WRITE_PNG,ObsDir+'LASCO_C3_OBS_'+tmp_time+'.png',IMAGER,R,G,B
   endfor
endif

if DoPlotSim then begin
;  template=ascii_template(file)
;  save,template,filename='los_templte.sav'
   restore,'los_template.sav'
   simdir=$
      '~/Work/SWQU/CR2161/CR2161/run021_AWSoM_restart_run017_AWSoM/run09/SC/'
   outdir='~/Work/SWQU/CR2161/output_C3/'
   filename = file_search(simdir+'los_soho_c3*.dat',count=nfiles)
   data0=read_ascii(filename[0],template=template)
   print,'Simulation Background FileName = ',filename[0]
;  F & K corona contribution     
   xx0=data0.field1
   yy0=data0.field2
   wl0=data0.field3
   pb0=data0.field4
   rr0=sqrt(xx0^2+yy0^2)
   cos2t0=2.0*(xx0/rr0)^2-1.0
   print,min(rr0),max(rr0)
   bk0=rr0^(-3.07)+82.0*rr0^(-6.95)
   bf0_temp=rr0^(0.22*cos2t0-2.47)
   index0=where(abs(rr0-1.5) lt 0.01)
   print,index0,rr0[index0]
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
   ; Now, actual CME images
   for k=26,nfiles-1 do begin
      data =read_ascii(filename[k],template=template)
      tmp = strsplit(filename[k],'_',/extract)
      time = strsplit(tmp[8],'t',/extract)
      print,'Simulation file = ',filename[k],fix(time)
      timing=FIX(time)/10000
      tmp1=FIX(time) MOD 10000
      mins=tmp1/100
      plot_time = String(timing)+'Hr'+TRIM(STRING(mins))+'Mins'
      print,plot_time
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
      ; Ratio of CME model / background model
      dimage=wl_image/wl_image0
      dimage_cor=(dimage+fk_image0)/(1.0+fk_image0)
      bad=WHERE(FINITE(dimage_cor, /NAN))
      loadct,1
      dimage_cor[bad]=-999
      prange=[min(dimage_cor>0),max(dimage_cor)]
      prange=[0.96,1.03]
      wdef,3,800,800
      MAP=MAKE_MAP(DIMAGE_COR,XC=0,YC=0,DX=64./300.,DY=64./300.)
      PLOT_MAP,MAP,TITLE='AWSoM CME SIM (LASCO C3)'+'  T= '+ $
               trim(plot_time), $
               CHARSIZE=2.5,XTITLE='SOLAR RADII',YTITLE='SOLAR RADII',$
               dmin=prange[0],dmax=prange[1], xthick=2,charthick=2,ythick=2,$
               xrange=[-20,20],yrange=[-20,20]
      plot_map_colorbar,prange,charsize=2.5,charthick=2
      tvcircle,4.0,0,0,color=0,/data,/fill
      IMAGER=TVRD(TRUE=1)
      WRITE_PNG,outdir+'LASCO_C3_SIM_T='+TRIM(plot_time)+'.png',$
                IMAGER,R,G,B
   endfor
endif
end
