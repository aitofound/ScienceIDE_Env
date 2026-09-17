pro GLSETUP1, FILE=FILE

;-----------------------------------------------------------------------
; NAME:
;   GLSETUP1
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
;Read the magnetogram (fitsfile.out in BATSRUS format)
  mag_info=read_magnetogram(file,PlotRadius,1)
  nlat=mag_info.nlat
  nlon=mag_info.nlon
  longitude=mag_info.longitude
  latitude=mag_info.latitude
  br_field=mag_info.br_field
  neqpar =mag_info.neqpar
  eqpar  =mag_info.eqpar

;Display the magnetogram and let user interactively select the CME source region. The
;procedure to select is:
; 1. Click the CME source region of positive polarity with 'left' button of mouse
; 2. Click the CME source region of negative polarity with 'right' button of mouse
;
;Note that the user can click anywhere inside the active region. However, click closer
;to the center of the positive/negative patterns is recommended.
;
;Note the solar latitude is expressed in pixel due to the non-uniform spacing. The latitude
;is uniform in sin(latitude). This will be changed in the future to degree. 

  br_field_show=br_field
  index=where(br_field lt -20)
  br_field_show[index]=-20
  index=where(br_field gt 20)
  br_field_show[index]=20

  window,2,xs=1200,ys=1200.*float(nlat)/float(nlon)*4./3.
  loadct,0
  contour,br_field_show,min=-20,max=20,charsize=3,$
          title='SWMF Input Magnetogram (R ='$
          +strtrim(PlotRadius,2)+' Rs)',xtitle='Solar Longitude (Pixel)',$
          ytitle='Solar Latitude (Pixel)',/fill,nlevels=60,/iso,xstyle=1,ystyle=1
  
  loadct,39
  if(neqpar ge 2) then begin
     ;plot the Earth Carrington coordinate:
     if eqpar[1] gt 0 then begin
        ;eqpar[1] is the Carrington coordinate of the Earth
        ;eqpar[0] is the Carrington coordinate of the left margin
        MapLongEarth = 360*(1 - mag_info.time)  - eqpar[0]
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
  !MOUSE.button = 0
  while(!MOUSE.button ne 1) do begin
     cursor,xPositive,yPositive,/data,/down
     if br_field[xPositive,yPositive] lt 0 then begin
        print,'Negative Polarity! Please Select POSITIVE Polarity!'   
        !MOUSE.button=0
     endif else begin
        plots,xPositive,yPositive,/data,psym=-2,color=250 
     endelse
  endwhile
  while(!MOUSE.button ne 4) do begin
     cursor,xNegative,yNegative,/data,/down
     if br_field[xNegative,yNegative] gt 0 then begin
        print,'Positive Polarity! Please Select NEGATIVE Polarity!'   
        !MOUSE.button=0
     endif else begin
        plots,xNegative,yNegative,/data,psym=-2,color=100
     endelse
  endwhile
  print, '==='
  print, xPositive,yPositive,xNegative,yNegative
  !mouse.button=0
  wait,2
  wdelete,2
  exit
end

pro TDSETUP1, FILE=FILE

;-----------------------------------------------------------------------
; NAME:
;   GLSETUP1
;
; OUTPUTS:
;   user choise for stdout file
;
; SYSTEM REQUIREMENTS:
; Mouse with left and right button
;
;
; KEYWORD: 
;
;   FILE = input zoomed magnetogram file
; color 250 - positive Br (center of the spot, footpoint)
; color 100 - negative Br (center of the spot, footpoint)
; color 150 - central point (midpoint, or apex)

;Setup the color mode and a better IDL font.

  device,decomposed=0
  !p.font=1
  PlotRadius =1.
;Read the magnetogram (fitsfile.out in BATSRUS format)
  mag_info=read_magnetogram(file,PlotRadius,1)
  nlat=mag_info.nlat
  nlon=mag_info.nlon
  longitude=mag_info.longitude
  latitude=mag_info.latitude
  br_field=mag_info.br_field
  sizemap_p= mag_info.blon_field
  sizemap_n= mag_info.blat_field
  neqpar =mag_info.neqpar
  eqpar  =mag_info.eqpar
  xPositive = eqpar[2]
  yPositive = eqpar[3]
  xNegative = eqpar[4]
  yNegative = eqpar[5]
  xCenter   = eqpar[6]
  yCenter   = eqpar[7]
;Display the magnetogram and let user interactively select the CME source region. The
;procedure to select is:
; 1. Click the CME source region of positive polarity with 'left' button of mouse
; 2. Click the CME source region of negative polarity with 'right' button of mouse
;
;Note that the user can click anywhere inside the active region. However, click closer
;to the center of the positive/negative patterns is recommended.
;
;Note the solar latitude is expressed in pixel due to the non-uniform spacing. The latitude
;is uniform in sin(latitude). This will be changed in the future to degree. 

  br_field_show=br_field
  index=where(br_field lt -20)
  br_field_show[index]=-20
  index=where(br_field gt 20)
  br_field_show[index]=20
  window,1,xs=800,ys=800,xpos=400,ypos=400
  loadct,0
  contour,br_field_show,min=-20,max=20,charsize=3,$
          title='SWMF Input Magnetogram (R ='$
          +strtrim(PlotRadius,2)+' Rs)',xtitle='Solar Longitude (Pixel)',$
          ytitle='Solar Latitude (Pixel)',/fill,nlevels=60,/iso,xstyle=1,$
          ystyle=1
  
  loadct,39
  plots,xPositive,yPositive,/data,psym=-2,color=250,symsize=5,thick=3
  plots,xNegative,yNegative,/data,psym=-2,color=100,symsize=5,thick=3
;plot center of the flux rope 
  plots,xCenter,yCenter,/data,psym=-2,color=150,symsize=5,thick=3
  ; Showing positive and negative spots
  contour,sizemap_p,/overplot,c_color=100
  contour,abs(sizemap_n),/overplot,c_color=200
  !MOUSE.button = 0
  while(!MOUSE.button ne 1) do begin
     cursor,xPositive,yPositive,/data,/down
     if br_field[xPositive,yPositive] lt 0 then begin
        print,'Negative Polarity! Please Select POSITIVE Polarity!'   
        !MOUSE.button=0
     endif else begin
        plots,xPositive,yPositive,/data,psym=-2,color=250 
     endelse
  endwhile
  while(!MOUSE.button ne 4) do begin
     cursor,xNegative,yNegative,/data,/down
     if br_field[xNegative,yNegative] gt 0 then begin
        print,'Positive Polarity! Please Select NEGATIVE Polarity!'   
        !MOUSE.button=0
     endif else begin
        plots,xNegative,yNegative,/data,psym=-2,color=100
     endelse
  endwhile
  print, '==='
  print, xPositive,yPositive,xNegative,yNegative
  !mouse.button=0
  wait,2
  wdelete,1
  exit
end

pro TDSETUP2, FILE=FILE,  RADIUS=RADIUS, APEX=APEX, BMAX=BMAX

;-----------------------------------------------------------------------
; NAME:
;   TDSETUP2
;
; OUTPUTS:
;   user choise for stdout file
;
; SYSTEM REQUIREMENTS:
; Mouse with left and right button
;
;
; KEYWORD: 
;
;   FILE = input file with strapping field array (SWMF format).
; color 250 - positive Br (center of the spot, footpoint)
; color 100 - negative Br (center of the spot, footpoint)
; color 150 - central point (midpoint, or apex)
  common getpict_param, filename
  common file_head
  common plot_data, grid, x, w
; named indexes:
  Bx_   =0               ; Strapping field component (horizontal) 
  Bz_   =2               ; Vertical field Br
  filename = FILE
  read_data
  DXyz  = eqpar[3]       ; Grid size
  iXMid = (nx[0] - 1)/2  ; 0:iXMid -negative x, iXMid:2*iXMid - positive x
  strap_field=w(*,*,0)
  strap_field_show=strap_field
  index=where(strap_field lt 2)
  strap_field_show[index]=2
  index=where(strap_field gt BMAX)
  strap_field_show[index]=BMAX
  window,0,xs=1200,ys=1200*nx[1]/nx[0],xpos=400,ypos=400
  device,decomposed=0
  !p.font=1
  loadct,39
  contour,strap_field_show,x(*,*,0),x(*,*,1),min=2,max=BMAX,charsize=3,$
          title='Strapping field [Gs] Rs)',xtitle='y',$
          ytitle='Altitude',/fill,nlevels=60,/iso,xstyle=1,ystyle=1
; left footpoint, x=-Radius, calculate x-index:
  iX = iXMid - round(Radius/DXyz)
; color with the sign  of radial field
  if w(iX,0,Bz_) gt 0.0 then begin
     plots,-Radius,0.0,/data,psym=1,color=250,symsize=5,thick=3   
  endif else begin
     plots,-Radius,0.0,/data,psym=1,color=100,symsize=5,thick=3
  endelse
; right footpoint, x=+Radius, calculate x-index:
  iX = iXMid + round(Radius/DXyz)
; color with the sign  of radial field
  if w(iX,0,Bz_) gt 0.0 then begin
     plots,Radius,0.0,/data,psym=1,color=250,symsize=5,thick=3   
  endif else begin
     plots,Radius,0.0,/data,psym=1,color=100,symsize=5,thick=3
  endelse
  print, 'Now, you see isolines of strapping field.'
  print, 'If there is an isoline of strapping field, which:'
  print, '    -Looks like a part of circumference;'
  print, '    -Connects footpoints marked with two crosses,'
  print, 'then click on the apex of this isoline.'
  print, 'This will become the toroidal magnetic axis of the TD configuration'
  print, 'Otherwise, click somewhere very far or very close to the point 0,0'
  print, 'You will be able to modify your choice of the flux rope footpoints'
  if Apex gt 0.0 then begin
     print, "User input: Apex=",Apex
     plots,0.0,Apex,/data,psym=1,color=150,symsize=5,thick=3
     ; calculate y-index of the apex
     iY=round(Apex/DXyz)
     BStrap=w(iXMid,iY,Bx_)
     print, '+++'
     print, Apex,BStrap
     wait,2
     wdelete,0
     exit
  endif
  !MOUSE.button = 0
  while(!MOUSE.button ne 1) do begin
     cursor,xApex,yApex,/data,/down
     if abs(xApex) gt Radius or yApex lt 0.05 or yApex gt Radius then begin
        print,'Failure, choose different foootpoints'   
     endif else begin
        plots,0.0,yApex,/data,psym=1,color=150,symsize=5,thick=3
        ; calculate y-index of the apex
        iY=round(yApex/DXyz)
        BStrap=w(iXMid,iY,Bx_)
        print, '+++'
        print, yApex,BStrap
     endelse
  endwhile
  !mouse.button=0
  wait,2
  wdelete,0
  exit
end



