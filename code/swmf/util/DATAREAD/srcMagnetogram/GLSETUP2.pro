pro GLSETUP2, FILE=FILE, UseBATS=UseBATS

;-----------------------------------------------------------------------
; NAME:
;   GLSETUP2
;
; OUTPUTS:
;   user choise for stdout file
;
; SYSTEM REQUIREMENTS:
; Mouse with left and right button
;
;
; KEYWORDS: 
;
;   FILE = input magnetogram file (can be FITS or SWMF format).
;   UseBATS = if set, will read BATS-R-US format (2D or 3D). Default
;             will read FITS format.

;Setup the color mode and a better IDL font.
  device,decomposed=0
  !p.font=1
  PlotRadius =1.

;Read the magnetogram
  mag_info=read_magnetogram(file,PlotRadius,UseBATS)
  nlat=mag_info.nlat
  nlon=mag_info.nlon
  longitude=mag_info.longitude
  latitude=mag_info.latitude
  br_field=mag_info.br_field
  sizemap_p= mag_info.blon_field
  sizemap_n= mag_info.blat_field
  occPos = mag_info.occPos
  occNeg = mag_info.occNeg

  neqpar = mag_info.neqpar
  eqpar  = mag_info.eqpar

  xPositive = eqpar[2]
  yPositive = eqpar[3]
  xNegative = eqpar[4]
  yNegative = eqpar[5]
  XyARCenter_D = [eqpar[6],eqpar[7]]
  nPIL = (neqpar - 8)/2
  xPIL_I = eqpar[8:7+nPIL]
  yPIL_I = eqpar[8+nPIL:7+2*nPIL]

  ;Read the magnetogram
  mag1_info=read_magnetogram('FRMagnetogram.out',PlotRadius,UseBATS)
  gl_bradfield = mag1_info.br_field
  gl_blonfield = mag1_info.blon_field
  gl_blatfield = mag1_info.blat_field

  br_field_show=br_field ; from AfterGlSETUP.out
  index=where(br_field lt -20)
  br_field_show[index]=-20
  index=where(br_field gt 20)
  br_field_show[index]=20
  
  inPos = where(occPos gt 0)
  inNeg = where(occNeg gt 0)
  occPos_show = occPos
  occPos_show[inPos] = 20
  occNeg_show = occNeg
  occNeg_show[inNeg] = 20
  ;Read the HMI vector field components (Blat & Blon) to overplot
  ;on the original magnetogram field Br
  cp =fltarr(nPIL)
  hmi_file = file_search('hmi_map.out',count=count)
  if (count eq 1) then begin
     IsPresentHMI = 1
     hmi_info = read_magnetogram('hmi_map.out',PlotRadius,UseBATS)
     hmi_Brad = hmi_info.br_field
     hmi_Blon = hmi_info.blon_field
     hmi_Blat = hmi_info.blat_field
     for i = 0, nPIL -1 do begin
        cp[i] = (longitude[xPIL_I(i)] * hmi_Blat[xPIL_I(i),yPIL_I(i)] - $
                 latitude[yPIL_I(i)] * hmi_Blon[xPIL_I(i),yPIL_I(i)])
     endfor
  endif else begin
     print, 'No HMI output found'
     IsPresentHMI = 0
  endelse

  window,2,xs=1200,ys=1200.*float(nlat)/float(nlon)*4./3.
  loadct,0
  contour,br_field_show,min=-20,max=20,charsize=3,$
          title='SWMF Input Magnetogram (R ='$
          +strtrim(PlotRadius,2)+' Rs)',xtitle='Solar Longitude (Pixel)',$
          ytitle='Solar Latitude (Pixel)',/fill,nlevels=60,/iso,xstyle=1,$
          ystyle=1
  
  loadct,39
  if(neqpar ge 2) then begin
     ;plot the Earth Carrington coordinate:
     if eqpar[1] gt 0 then begin
        ;eqpar[1] is the Carrington coordinate of the Earth
        ;eqpar[0] is the Carrington coordinate of the left margin
        MapLongEarth = 360*(1 - mag_info.time) - eqpar[0]
        ;If the left margin of the map is in the next Carrington 
        ;rotation, add 360 deg:
        if MapLongEarth lt 0 then MapLongEarth = MapLongEarth +360
        ;Our plot coordinates are in pixels, not in degrees:
        PixelEarth = (MapLongEarth/360.)*nlon
        xEarthLine = findgen(nlat)*0.+PixelEarth
        yEarthLine = findgen(nlat)
        oplot,xEarthLine,yEarthLine,color=250,linestyle=5
     endif
  endif
  plots,xPositive,yPositive,/data,psym=-2,color=250
  plots,xNegative,yNegative,/data,psym=-2,color=100
;plot center of the flux rope 
  plots,XyARCenter_D[0],XyARCenter_D[1],/data,psym=-2,color=150
;show PIL
  for i=0, nPIL-1 do begin
     plots,xPIL_I(i),yPIL_I(i),psym=-1,color=210
  endfor
; Showing positive and negative spots
  contour,sizemap_p,/overplot,c_color=100
  contour,abs(sizemap_n),/overplot,c_color=100
  contour,gl_bradfield,/overplot,min=-10,max=10,nlevels=20,$
          c_colors=indgen(20)*long(10)+long(25)

  wait, 10

;The region size is used to cover the whole area of active region in
;order to show a zoom-in image. Shorter RegionSize for near-Limb
;regions when needed.

  RegionSize=round(long(50)*nlon/360)
  
;Display the zoom-in image of the active region with weighted centers and PIL.
  window,3,xs=1200,ys=1200,xpos=400,ypos=400
  device,decomposed=0
  loadct,0
  
  sub_x1=max([round(XyARCenter_D[0])-RegionSize/2,0])
  sub_x2=min([round(XyARCenter_D[0])+RegionSize/2,nlon-1])
  sub_y1=max([round(XyARCenter_D[1])-RegionSize/2,0])
  sub_y2=min([round(XyARCenter_D[1])+RegionSize/2,nlat-1])

  contour,br_field_show[sub_x1:sub_x2,sub_y1:sub_y2],$
          min=-20,max=20,charsize=3,title='CME Source Region (R ='$
          +strtrim(PlotRadius,2)+' Rs)',xtitle='Solar Longitude (Degree)',$
          ytitle='Solar Latitude (Pixel)',/fill,nlevels=60,/iso,$
          xstyle=1,ystyle=1

  loadct,39

;Showing positive and negative spots
  contour,sizemap_p[sub_x1:sub_x2,sub_y1:sub_y2],/overplot,c_color=100
  contour,abs(sizemap_n[sub_x1:sub_x2,sub_y1:sub_y2]),/overplot,c_color=100
  contour,gl_bradfield[sub_x1:sub_x2,sub_y1:sub_y2],/overplot,min=-10,max=10,$
          nlevels=20,c_colors=indgen(20)*long(10)+long(25)
  plots,xPositive-sub_x1,yPositive-sub_y1,$
        /data,psym=-2,color=250,symsize=2,thick=2
  plots,xNegative-sub_x1,yNegative-sub_y1,$
        /data,psym=-2,color=100,symsize=2,thick=2
  plots,XyARCenter_D[0]-sub_x1,XyARCenter_D[1]-sub_y1,/data,psym=-2,$
        color=150,symsize=2,thick=2
  for i=0, nPIL-1 do begin
     if IsPresentHMI eq 1 then begin
        if cp(i) > 0 then begin
           plots,xPIL_I(i)-sub_x1,yPIL_I(i)-sub_y1,psym=-1,color=210,$
                 symsize=2,thick=3
        endif else plots,xPIL_I(i)-sub_x1,yPIL_I(i)-sub_y1,psym=-1,color=190,$
                         symsize=2,thick=3
     endif else  plots,xPIL_I(i)-sub_x1,yPIL_I(i)-sub_y1,psym=-1,color=210,$
                       symsize=2,thick=3 
  endfor
  if (IsPresentHMI eq 1) then begin 
     velovect,smooth(hmi_Blon[sub_x1:sub_x2,sub_y1:sub_y2],5),$
              smooth(hmi_Blat[sub_x1:sub_x2,sub_y1:sub_y2],5),COLOR=50, $
              LENGTH=3.5,max=20,min=-20,thick=1.95,/NOERASE,/overplot
  endif
  ; save the zoomed in AR with the flux rope, AR spots, PIL, HMI (if present)
  write_png,'AR_output1.png',TVRD(/TRUE)

  velovect,gl_blonfield[sub_x1:sub_x2,sub_y1:sub_y2],$
           gl_blatfield[sub_x1:sub_x2,sub_y1:sub_y2],color=200,/overplot,$
           thick=1.5,/NOERASE,LENGTH=1.5

  ; same as above along with the transverse component of GL field
  write_png,'AR_output2.png',TVRD(/TRUE)

  window,4,xs=1200,ys=1200,xpos=400,ypos=400
  device,decomposed=0
  loadct,0

  contour,gl_bradfield[sub_x1:sub_x2,sub_y1:sub_y2],$
          min=-20,max=20,charsize=3,title='CME Source Region (R ='$
          +strtrim(PlotRadius,2)+' Rs)',xtitle='Solar Longitude (Degree)',$
          ytitle='Solar Latitude (Pixel)',/fill,nlevels=60,/iso,$
          xstyle=1,ystyle=1

  loadct,39

;Showing positive and negative spots
  contour,sizemap_p[sub_x1:sub_x2,sub_y1:sub_y2],/overplot,c_color=100
  contour,abs(sizemap_n[sub_x1:sub_x2,sub_y1:sub_y2]),/overplot,c_color=100
  contour,gl_bradfield[sub_x1:sub_x2,sub_y1:sub_y2],/overplot,min=-10,max=10,$
          nlevels=20,c_colors=indgen(20)*long(10)+long(25)
  plots,xPositive-sub_x1,yPositive-sub_y1,$
        /data,psym=-2,color=250,symsize=2,thick=2
  plots,xNegative-sub_x1,yNegative-sub_y1,$
        /data,psym=-2,color=100,symsize=2,thick=2
  plots,XyARCenter_D[0]-sub_x1,XyARCenter_D[1]-sub_y1,/data,psym=-2,$
        color=150,symsize=2,thick=2
  for i=0, nPIL-1 do begin
     if IsPresentHMI eq 1 then begin
        if cp(i) > 0 then begin
           plots,xPIL_I(i)-sub_x1,yPIL_I(i)-sub_y1,psym=-1,color=210,$
                 symsize=2,thick=3
        endif else plots,xPIL_I(i)-sub_x1,yPIL_I(i)-sub_y1,psym=-1,color=190,$
                         symsize=2,thick=3
     endif else  plots,xPIL_I(i)-sub_x1,yPIL_I(i)-sub_y1,psym=-1,color=210,$
                       symsize=2,thick=3 
  endfor
  velovect,gl_blonfield[sub_x1:sub_x2,sub_y1:sub_y2],$
           gl_blatfield[sub_x1:sub_x2,sub_y1:sub_y2],color=200,/overplot,$
           thick=1.5,/NOERASE,LENGTH=1.5

  ; same as above without the HMI field
  write_png,'AR_output3.png',TVRD(/TRUE)
  wait,2
  wdelete,2
  wait,2
  wdelete,3
  wait,2
  wdelete,4

;Display the zoom-in image of the active region with weighted centers and PIL.
  window,5,xs=1200,ys=1200,xpos=400,ypos=400
  device,decomposed=0
  loadct,0
  
  sub_x1=max([round(XyARCenter_D[0])-RegionSize/2,0])
  sub_x2=min([round(XyARCenter_D[0])+RegionSize/2,nlon-1])
  sub_y1=max([round(XyARCenter_D[1])-RegionSize/2,0])
  sub_y2=min([round(XyARCenter_D[1])+RegionSize/2,nlat-1])

  contour,br_field_show[sub_x1:sub_x2,sub_y1:sub_y2],$
          min=-20,max=20,charsize=3,title='CME Source Region (R ='$
          +strtrim(PlotRadius,2)+' Rs)',xtitle='Solar Longitude (Degree)',$
          ytitle='Solar Latitude (Pixel)',/fill,nlevels=60,/iso,$
          xstyle=1,ystyle=1

  loadct,39
  contour,occPos_show[sub_x1:sub_x2,sub_y1:sub_y2],/overplot,c_color=200
  contour,occNeg_show[sub_x1:sub_x2,sub_y1:sub_y2],/overplot,c_color=200

  for j=0,nlat-1 do begin
     for i=0,nlon-1 do begin
        if (occPos[i,j] eq 1) then begin
           oplot,[i-sub_x1,i-sub_x1],[j-sub_y1,j-sub_y1],psym=2,symsize=2,$
                 color=250
        endif else begin
           if (occPos[i,j] gt 1) then begin
              color1 = occPos[i,j]*100
              oplot,[i-sub_x1,i-sub_x1],[j-sub_y1,j-sub_y1],psym=1,$
                    symsize=1.5,color=color1
           endif
        endelse
        if (occNeg[i,j] eq 1) then begin
           oplot,[i-sub_x1,i-sub_x1],[j-sub_y1,j-sub_y1],psym=2,$
                 symsize=2,color=100
        endif else begin
           if (occNeg[i,j] gt 1) then begin
              color1 = occNeg[i,j]*100
              oplot,[i-sub_x1,i-sub_x1],[j-sub_y1,j-sub_y1],psym=1,$
                    symsize=1.5,color=color1
           endif
        endelse
     endfor
  endfor
  plots,xPositive-sub_x1,yPositive-sub_y1,$
        /data,psym=-2,color=250,symsize=2,thick=2
  plots,xNegative-sub_x1,yNegative-sub_y1,$
        /data,psym=-2,color=100,symsize=2,thick=2
  wait,10
  ; save the zoomed in AR with the positive and negative regions marked
  ; based on occupancy matrix
  write_png,'AR_output4.png',TVRD(/TRUE)
  wdelete,5
  exit
end
