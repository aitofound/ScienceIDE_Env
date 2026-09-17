

pro thermo_plotbatch_v3,cursor_x,cursor_y,strx,stry,step,nvars,sel,nfiles, $
                cnt1,cnt2,cnt3,ghostcells,no,yeslog,  	  $
                nolog,nalts,nlats,nlons,yeswrite_cnt,$
                polar,npolar,MinLat,showgridyes,	  $
                plotvectoryes,vi_cnt,vn_cnt,cf,	  $
                cursor_cnt,data,alt,lat,lon,	  $
                xrange,yrange,selset,smini,smaxi,	  $
                filename,vars, psfile, mars, colortable, itime,ortho,plat,plon

if mars then begin
  file = 'marsflat.jpg'
   read_jpeg, file, image
   nx = n_elements(image(0,*,0))
   ny = n_elements(image(0,0,*))
   new_image = fltarr(nx,ny)
                                ;We usually plot with 0 lon in the
                                ;middle, but the jpeg as 0 lon on the right...
for i=nx/2, nx-1 do begin
   new_image(i-nx/2,0:ny-1)  = image(2,i,*)
   new_image(i,0:ny-1)  = image(2,i-nx/2,*)
endfor
endif

if (n_elements(colortable) eq 0) then colortable = 'mid'
if (strlen(colortable) eq 0) then colortable = 'mid'

if (n_elements(logplot) eq 0) then logplot = yeslog

if (min(data(sel,*,*,*)) lt 0.0) then begin
  logplot = 0
  yeslog = 0
  nolog = 1
endif

;if (n_elements(iTime) eq 0) then begin

    if (strpos(filename,"save") gt 0) then begin

        fn = findfile(filename)
        if (strlen(fn(0)) eq 0) then begin
            print, "Bad filename : ", filename
            stop
        endif else filename = fn(0)
        
        l1 = strpos(filename,'.save')
        fn2 = strmid(filename,0,l1)
        len = strlen(fn2)
        l2 = l1-1
        while (strpos(strmid(fn2,l2,len),'.') eq -1) do l2 = l2 - 1
        l = l2 - 13
        year = fix(strmid(filename,l, 2))
        mont = fix(strmid(filename,l+2, 2))
        day  = fix(strmid(filename,l+4, 2))
        hour = float(strmid(filename, l+7, 2))
        minu = float(strmid(filename,l+9, 2))
        seco = float(strmid(filename,l+11, 2))
    endif else begin
        if (strpos(filename,"bin") gt 0) then begin
            l1 = strpos(filename,'.bin')
            fn2 = strmid(filename,0,l1)
            len = strlen(fn2)
            l2 = l1-1
            l = l1 - 13
            year = fix(strmid(filename,l, 2))
            mont = fix(strmid(filename,l+2, 2))
            day  = fix(strmid(filename,l+4, 2))
            hour = float(strmid(filename, l+7, 2))
            minu = float(strmid(filename,l+9, 2))
            seco = float(strmid(filename,l+11, 2))
        endif else begin
            year = fix(strmid(filename,07, 2))
            mont = fix(strmid(filename,09, 2))
            day  = fix(strmid(filename,11, 2))
            hour = float(strmid(filename,14, 2))
            minu = float(strmid(filename,16, 2))
            seco = float(strmid(filename,18, 2))
        endelse
    endelse

    itime = [year,mont,day,fix(hour),fix(minu),fix(seco)]

;endif

c_a_to_s, itime, stime
c_a_to_r,itime,rtime

ut = itime(3) + itime(4)/60.0 + itime(5)/3600.0
if (polar) then utrot = ut * 15.0 else utrot = 0.0

if (cnt1 eq 0 and polar) then polar = 0

;Get Subsolar point
zdate = tostr(year(0)+2000)+'-'+chopr('0'+tostr(mont(0)),2)+'-'+chopr('0'+tostr(day(0)),2)
ztime = fix(hour)+fix(minu)/60.+fix(seco)/3600.
zlat = 0
zlon = 0

zsun,zdate,ztime,zlat,zlon,zenith,azimuth,solfac,latsun=latsun,lonsun=lonsun
;
nLons = n_elements(lon(*,0,0))
localtime = fltarr(nlons)

for ilon = 0, nlons - 1 do begin
   localtime(ilon) = convert_time(rtime,lon(ilon,0,0))
endfor

locs = where(localtime lt 0,il)
if il gt 0 then localtime(locs) = 0.0

;According to the button user selected, get the values for plotting
if cnt1 eq 1 then begin
    if (polar) then MinLat = MinLat else MinLat = -1000.0
    if not (polar) then mr = 1090
    if (polar) then begin
        mr = 90.0 - abs(MinLat)
    endif
;    if (polar) and not (npolar) then begin
;        mr = 90.0 + MinLat
;    endif

  
    if (polar) then begin
        if (npolar) then begin
            loc = where(lat(0,*,0) ge abs(MinLat) and abs(lat(0,*,0)) lt 90.0)
        endif else begin
            loc = where(lat(0,*,0) le -abs(MinLat) and abs(lat(0,*,0)) lt 90.0)
        endelse
    endif else begin
        loc = where(lat(0,*,0) ge -200 and abs(lat(0,*,0)) lt 200.0)
    endelse

    if (polar) then begin
        datatoplot=reform(data(sel,2:nLons-2,loc,selset))
        datatoplot(nLons-4,*) = datatoplot(0,*)
    endif else datatoplot=reform(data(sel,*,loc,selset))
    maxi=max(datatoplot)
    mini=min(datatoplot)
;   maxi= 3.1623e+14
;   mini= 3.1623e+10
    
    if (polar) then begin

        if (npolar) then begin
            x = reform( (90.0 - lat(2:nLons-2,loc,selset)) * $
                        cos((lon(2:nLons-2,loc,selset)+utrot)*!pi/180.0 - !pi/2.0))
            y = reform( (90.0 - lat(2:nLons-2,loc,selset)) * $
                        sin((lon(2:nLons-2,loc,selset)+utrot)*!pi/180.0 - !pi/2.0))
        endif else begin
            x = reform( (90.0 + lat(2:nLons-2,loc,selset)) * $
                        cos((lon(2:nLons-2,loc,selset)+utrot)*!pi/180.0 - !pi/2.0))
            y = reform( (90.0 + lat(2:nLons-2,loc,selset)) * $
                        sin((lon(2:nLons-2,loc,selset)+utrot)*!pi/180.0 - !pi/2.0))
        endelse
        xrange = [-mr,mr]
        yrange = [-mr,mr]
        xtitle=' '
        ytitle=' '

    endif else begin
        if ortho then begin
            x=reform(lon(*,loc,selset))
            y=reform(lat(*,loc,selset))
           
           
            datatoplot = datatoplot(1:nLons-2,1:nLats-2)
            
            y = y(1:nLons-2,1:nLats-2)
            x = x(1:nLons-2,1:nLats-2)
            nLons  = nLons-2
            nLats  = nLats-2
            
            datatoplot(0,*)       = (datatoplot(1,*)+datatoplot(nLons-2,*))/2.0
            datatoplot(nLons-1,*) = (datatoplot(1,*)+datatoplot(nLons-2,*))/2.0
            datatoplot(*,0)       = mean(datatoplot(*,1))
            datatoplot(*,nLats-1) = mean(datatoplot(*,nLats-2))
            
            x(0)       = 0.0
            x(nLons-1,*) = 360.0
            y(*,0) = -90.0
            y(*,nLats-1) =  90.0

;        save, newrat, newlon, newlat
            if ghostcells eq 0 then begin
                xrange=[0,360]
                yrange=[-90,90]
            endif
            xtitle='Longitude (deg)'
            ytitle='Latitude (deg)'
         endif else begin
            x=reform(lon(*,loc,selset))
            y=reform(lat(*,loc,selset))
            if ghostcells eq 1 then begin
                xrange=mm(lon)
                yrange=mm(lat)
            endif
            if ghostcells eq 0 then begin
                xrange=[0,360]
                yrange=[-90,90]
            endif
            xtitle='Longitude (deg)'
            ytitle='Latitude (deg)'
        endelse
    endelse
    ygrids = n_elements(loc)
    xgrids = nlons
    location = string(alt(0,0,selset),format='(f5.1)')+' km Altitude'
endif

if cnt2 eq 1 then begin
    datatoplot=reform(data(sel,*,selset,*))
    maxi=max(datatoplot)
    mini=min(datatoplot)
    x=reform(lon(*,selset,*))
    y=reform(alt(*,selset,*))
    location = string(lat(0,selset,0),format='(f5.1)')+' deg Latitude'
    xtitle='Longitude (deg)'
    ytitle='Altitude (km)'
    nLons = n_elements(lon(*,0,0))
   

    if ghostcells eq 1 then begin
        xrange=mm(lon)
        yrange=mm(alt)
    endif
    if ghostcells eq 0 then begin
        backup_xrange=mm(lon)
        backup_yrange=mm(alt)
        default_xrange=[0,360]
        default_yrange=mm(alt)
;If out of range then use 'mm' to set xrange and yrange values.
;Else use default values.
        if (backup_xrange[0] lt default_xrange[0]) $
          and (backup_xrange[1] gt default_xrange[1]) then begin
            xrange=mm(lon)
            yrange=mm(alt)
        endif else begin
            xrange=[0,360]
            yrange=mm(alt)
        endelse
    endif

xrange = [0,360]

    ygrids=nalts
    xgrids=nlons
endif

; --------------------------------------------------------------------
if cnt3 eq 1 then begin
    datatoplot=reform(data(sel,selset,*,*))
    maxi=max(datatoplot)
    mini=min(datatoplot)
    x=reform(lat(selset,*,*))
    y=reform(alt(selset,*,*))
    location = string(lon(selset,0,0),format='(f5.1)')+' deg Longitude'
    xtitle='Latitude (deg)'
    ytitle='Altitude (km)'
    if ghostcells eq 1 then begin
        xrange=mm(lat)
;       yrange = [0,90]
        yrange=mm(alt)
     endif
    if ghostcells eq 0 then begin
        backup_xrange=mm(lat)
;       backup_yrange= [0,90]
        backup_yrange=mm(alt)
        default_xrange=[-90,90]
;       default_yrange= [0,90]
        default_yrange=mm(alt)
;If out of range then use 'mm' to set xrange and yrange values.
;Else use default values.
        if (backup_xrange[0] lt default_xrange[0]) $
          and (backup_xrange[1] gt default_xrange[1]) then begin
            xrange=mm(lat)
;           yrange = [0,90]
            yrange=mm(alt)
        endif else begin
            xrange=[-90,90]
;           yrange = [0,90]
            yrange=mm(alt)
        endelse
    endif
;   ygrids=36
    ygrids=nalts
    xgrids=nlats

xrange = [-90,90]
;yrange = [0,90]
; --------------------------------------------------------------------

endif

  ;Calculate the xld, yld according to the cursor position user set.
  ;Calculate and get the array will be plotted.  

xld = x(*,0)
yld = y(0,*)
dist_x=abs(xld-cursor_x)
dist_y=abs(yld-cursor_y)
locx=where(dist_x eq min(dist_x))
locy=where(dist_y eq min(dist_y))
datald=reform(data(sel,*,locx,locy))

if n_elements(smini) eq 0 then smini = '0.0'
if n_elements(smaxi) eq 0 then smaxi = '0.0'

if (float(smini) ne 0 or float(smaxi) ne 0) then begin
    mini = float(smini)
    maxi = float(smaxi)
    mini = mini(0)
    maxi = maxi(0)
endif else begin
    mini = mini(0)
    maxi = maxi(0)
    r = (maxi-mini)*0.05
    mini = mini - r
    maxi = maxi + r
    if (logplot) then begin
        if (maxi gt 0.0) then maxi = alog10(maxi)
        if (mini gt 0.0) then mini = alog10(mini)
;    Most densities for Mars
;       if (maxi-mini gt 8) then begin
;           mini = maxi-8
;           print, "Limiting minimum..."
;       endif
;    CO2 densities for Mars
        if (maxi-mini gt 4) then begin
            mini = maxi-4
            print, "Limiting minimum..."
        endif
    endif 
endelse

if mini eq maxi then maxi=mini*1.01+1.0
levels = findgen(31)/30.0*(maxi-mini) + mini
loc = where(datatoplot lt levels(1), count)
if (count gt 0) then datatoplot(loc) = levels(1)

 ; Check if user wants to write the result to a file
 ;If user wanted then setdevice. 

if yeswrite_cnt eq 1 then begin

    if (strlen(psfile) eq 0) then psfile = filename+'.ps'
    setdevice,psfile,'l',4,0.95

endif

plotdumb

;if not ortho then variable = strcompress(vars(sel),/remove) $
;  else variable = vars(sel)
variable = vars(sel)
;  makect,'wyr'

 makect,'mid'
;makect, colortable
;loadct, 0
;makect,'grey'
clevels = findgen(31)/30 * 253.0 + 1.0

if (polar) then begin
    xstyle = 5 
    ystyle = 5 
endif else begin
    xstyle = 1
    ystyle = 1
endelse

if (not polar and cnt1) then ppp = 2 else ppp = 1

space = 0.075
pos_space, ppp, space, sizes, ny = 1
get_position, ppp, space, sizes, 0, pos

if (not polar) then begin
    if (cnt1) then begin
        r = pos(2) - pos(0)
        pos(2) = pos(0) + r*2.0

    endif else begin
        get_position, ppp, space, sizes, 0, pos, /rect
        pos(1) = pos(1) + space
        pos(3) = pos(3) - space*2.0
        pos(0) = pos(0) + space*1.0
        pos(2) = pos(2) - space*1.0
    endelse
endif
charsize = 1.2
;If user DOES NOT do the Ghost Cells Contour. 
if ghostcells eq 0 then begin

    ;If user DO NOT do plot log. 

    if (logplot) then begin
        loc = where(datatoplot lt max(datatoplot)*1e-8,count)
        if (count gt 0) then datatoplot(loc) = max(datatoplot)*1e-8
        datatoplot = alog10(datatoplot)
    endif

    if (cnt1) then begin 

;ppp = 1            
;    pos(0) = .45
;print, pos

        if (not polar and not ortho) then begin
            locx = where(x(*,0) ge   0.0 and x(*,0) le 360.0,nx)
            locy = where(y(0,*) ge -90.0 and y(0,*) le  90.0,ny)
            d2 = fltarr(nx,ny)
            x2 = fltarr(nx,ny)
            y2 = fltarr(nx,ny)
            for i=nx/2, nx-1 do begin
                d2(i-nx/2,0:ny-1)  = datatoplot(locx(i),locy)
                x2(i-nx/2,0:ny-1)  = x(locx(i),locy)
                y2(i-nx/2,0:ny-1)  = y(locx(i),locy)
                d2(i,0:ny-1)  = datatoplot(locx(i-nx/2),locy)
                x2(i,0:ny-1)  = x(locx(i-nx/2),locy)
                y2(i,0:ny-1)  = y(locx(i-nx/2),locy)
            endfor
        endif else begin
            d2 = datatoplot
            x2 = x
            y2 = y
            ny = n_elements(y2(0,*))
            nx = n_elements(x2(0,*))
        endelse


        if (not polar) then begin

            if ortho ne 1 then begin
                !p.position = pos
                if not mars then map_set, title=variable+' at '+location+' at '+$
                  strmid(stime,0,15)+' UT',/cont else $
                     map_set,charsize = charsize

            endif else begin
                pos = [.05,.05,.72,.95]
                !p.position = pos
                map_set, title=' ',plat,plon,/ortho,/noborder

            endelse
        endif else begin

            ;-------------------------------------
            ; polar plot
            ;-------------------------------------

            plot, [-mr, mr], [-mr, mr], pos=pos, $
              xstyle=5, ystyle=5,/nodata,/noerase, $
              title=variable+' at '+location+' '+stime

        endelse


    endif else begin

        d2 = datatoplot
        x2 = x
        y2 = y
        if not mars then begin
           plot, mm(x2), mm(y2), /nodata, xstyle = 1, ystyle=1,$
                 /noerase, pos = pos, $
                 title=variable+' at '+location+' '+stime, $
                 xrange=xrange,yrange=yrange
        endif else begin
           plot, mm(x2), mm(y2), /nodata, xstyle = 1, ystyle=1,$
                 /noerase, pos = pos, $
                 title=variable+stime, $
                 xrange=xrange,yrange=yrange,xtickname = strarr(10)+' ',$
                 ytickname = strarr(10) + ' '
        endelse
    endelse

;    endif else begin 

;    endelse

    if (cnt1) then begin
plotsubsolar = 0
plotlines = 0
maxt = (max(datatoplot))
linelevels = findgen(9) * .3*2. / 8 - .3

;findgen(31)/30 * 253.0 + 1.0
linestyle = intarr(9)
linestyle(0:4) = 1
linestyle(5:8) = 0

plotsyms = 0
plotbox = 0
nsyms = 1
symlats = 22.5
symlons = 292.5

;nsyms = 6
;symlats = [22.5,27.5,37.5,32.5,27.5,32.5]
;symlons = [282.5,302.5,327.5,287.5,322.5,337.5]

        loc = where(d2 gt max(levels(n_elements(levels)-2)),count)
        if (count gt 0) then d2(loc) = max(levels(n_elements(levels)-2))

      
        contour,d2, x2, y2,POS=pos,$
          levels=levels,xstyle=xstyle,ystyle=ystyle,$
          xrange=xrange,yrange=yrange,$
          c_colors=clevels,$
         /cell_fill,charsize=charsize,/over
       ; xyouts, (pos(0)+pos(2))/2.-.03,(pos(1)-.05),'Longitude',/norm,charsize=1.3
       ; xyouts, pos(0)-.01,(pos(1)+pos(3))/2.-.02,'Latitude',/norm,charsize=1.3,orientation=90

        if mars then begin
           newlon = reform(lon(2:nlons-3,0,0))
           xticks = 11
           ticknames = findgen(xticks+1)*2
           xtickv = fltarr(xticks+1)
           mindiff = min(abs(localtime(2:nlons-3) - 0),imin)
           ltlons = newlon(imin)
;           if ltlons gt 180 then ltlons = ltlons - 360

           xtickv(0) = (ltlons/360.) + 180/360. ;zero is in the middle not the right
           if xtickv(0) gt 1 then xtickv(0) = xtickv(0) - 1
           
           if xtickv(0) lt 0 then begin
             print,  "something is funny!"
              stop
           endif
           for itick = 1, xticks  do begin
              xtickv(itick) = xtickv(0) + itick/12.

              if xtickv(itick) gt 1 then xtickv(itick) = xtickv(itick) - 1

           endfor

;for itick = 0, xticks  - 0 do begin
          ;    mindiff = min(abs(localtime(2:nlons-3) - ticknames(itick)),imin)
         ;     ltlons(itick) = lon(imin+2)

        ;endfor

;           locs = where(ltlons gt 180)
;           ltlons(locs) = ltlons(locs) - 360
           
           minlon = min(xtickv,imini)
            xtickvnew = shift(xtickv,-imini)
            xtickname = tostr(shift(ticknames,-imini))
;stop
           axis,xaxis=1,xtickv=xtickvnew,xticks = xticks,xtickn = xtickname,xtitle='Solar Local Time ('+$
                strmid(stime,0,15)+' UT)',$
                charsize = charsize
                
          lons2=[-180, -90, 0, 90, 180]
          axis,xaxis=0,xticks = 4,xtickn = lons2,xtitle='Longitude',$
                charsize = charsize

           lats2=[-90, 0, 90] 
           axis,yaxis=0,yticks=2,ytickn=lats2,ytitle='Latitude',charsize=charsize

;           xyouts,(xtickv(xticks)-xtickv(0))/2.,min(y2)-1.3*(max(y2)-min(y2))/8,'Local Time',$
;                  alignment = 0.5
        endif
        if plotsubsolar then begin
            plots,lonsun,latsun,psym=sym(1),symsize = 3,/data,thick=4,color=254
            plots,lonsun+180,latsun,psym=sym(2),symsize = 3,/data,thick=4,color=50
        endif
        
        if plotbox then begin
          ;  plots,225,0,/data
          ;  plots,225,45,/data,/continue,linestyle=2
          ;  plots,45,45,/data,/continue,linestyle=2
          ;  plots,45,0,/data,/continue,linestyle=2
          ;  plots,225,0,/data
          ;  plots,45,0,/data,/continue,linestyle=2
            plots,270,0,/data
            plots,270,45,/data,/continue,linestyle=2
            plots,315,45,/data,/continue,linestyle=2
            plots,315,0,/data,/continue,linestyle=2
            plots,270,0,/data,/continue,linestyle=2
        endif
        
        if plotsyms then begin
;            loadct, 39
            for isym = 0, nsyms - 1 do begin
                plots,symlons(isym),symlats(isym),psym=sym(2),symsize=2,/data,color = 0
            endfor
;            makect,'mid'
        endif

        if plotlines then begin
            contour,d2,x2,y2, pos = pos, $
              xstyle=xstyle,ystyle=ystyle,$
              xrange=xrange,yrange=yrange,$
              /noerase, levels = linelevels, /follow, $
              c_linestyle = linestyle,/over
        endif
        
        if ortho then begin
            year = '0'+tostr(itime(0))
            year = strmid(year,strlen(year)-2,2)
            mont = '0'+tostr(itime(1))
            mont = strmid(mont,strlen(mont)-2,2)
            day = '0'+tostr(itime(2))
            day = strmid(day,strlen(day)-2,2)
            hour = '0'+tostr(itime(3))
            hour = strmid(hour,strlen(hour)-2,2)
            min = '0'+tostr(itime(4))
            min = strmid(min,strlen(min)-2,2)

            string = mont+'/'+ $
              day + '/' + $
              year+ ' ' + $
               hour+ ':' + min + ' (Perturbed)'
            xyouts,.62,.9,string,/norm
            endif
     endif else begin
;yrange = [0,100]
;yt = y2(0,*)
;ny2 = fltarr(nlats,nalts)
;for i = 0, nlats -1 do begin
;   ny2(i,*) = yt
;endfor 
        contour,d2, x2, y2,POS=pos,$
          levels=levels,xstyle=xstyle,ystyle=ystyle,$
          xrange=xrange,yrange=yrange,$
          c_colors=clevels,$
          xtitle=xtitle,ytitle=ytitle,/cell_fill,/noerase,charsize=charsize

        if cnt2 then begin
           
          if mars then begin
           xticks = 12
           xtickv = findgen(xticks+1)*(360/float(xticks))

           xtickname = strarr(xticks+1)
           for itick = 0, xticks  do begin
              if xtickv(itick) lt 0 then xt = xtickv(itick)+360 else xt = xtickv(itick)
              ival = min(abs((xt) - lon(2:nlons-3,0,0)),imin) 
              imin = imin + 2
              xtickname(itick) = tostr(localtime(imin) - ((lon(imin) - xt) / 14.62))
              if float(xtickname(itick)) lt 0 then xtickname(itick) = tostr(float(xtickname(itick)) + 24.)
;tostr(localtime(imin) + (localtime(imin) - localtime(imin-1)) * $
;                                 (lon(imin) - (xt))/(lon(imin)-lon(imin+1)))
              xyouts, xtickv(itick),min(y2)-(max(y2)-min(y2))/8.,xtickname(itick),alignment=0.5

           endfor
           
           xyouts,(xtickv(xticks)-xtickv(0))/2.,min(y2)-1.3*(max(y2)-min(y2))/8,'Local Time',$
                  alignment = 0.5
;           axis,xaxis=1,xticks = xticks,xtickn = xtickname,xtitle='Solar Local Time',$
;                charsize = 1.1,/data
        endif
       endif
        
        if cnt3 then begin
           if mars then begin
              xt = lon(selset,0,0)
              ival = min(abs((xt) - lon(2:nlons-3,0,0)),imin) 
              imin = imin + 2
              ltplot = chopl(tostrf(localtime(imin) - ((lon(imin) - xt) / 14.62))+'00',5)
              xyouts,90,min(y2)-.65*(max(y2)-min(y2))/8.,"SLT= "+ltplot,alignment=1.0

           endif
        endif
     endelse


    if (not polar and cnt1 and not mars) then map_continents, color = 0
      if (cnt1 and not polar and mars) then begin
         contour, new_image(*,*), levels = [150], pos = pos, /noerase, $
                  xstyle =5, ystyle=5, color = 0, thick=1.5,/t3d,zvalue=zvalue,xtitle=xtitle,ytitle=ytitle

endif
    if (polar) then plotmlt, mr, /no06, /no12
    if not (polar) then mr = 1090

;    if (cnt1 eq 1) then begin

;    endif
    
    ;Draw grid.
    if (showgridyes eq 1) then begin
        for i=0,ygrids-1 do begin
            oplot,x(*,i),y(*,i)
        endfor
        for j=0,xgrids-1 do begin
            oplot,x(j,*),y(j,*)
        endfor
    endif
    ;If user set cursor position to plot, then do plot of datald.
    ;Else clean up the cursor text fields on the interface. 

    if cursor_cnt eq 1 then begin
        if cnt1 eq 1 then begin
            plot,datald,alt(0,0,*),xtitle='datald',ytitle='Alt. (deg)'
        endif
        if cnt2 eq 1 then begin
            plot,datald,lat(0,*,0),ystyle=1,xtitle='datald',ytitle='Lat. (deg)'
        endif
        if cnt3 eq 1 then begin
            plot,datald,lon(*,0,0),xtitle='datald',ytitle='Long. (deg)'
        endif
    endif else begin	
        txt=''
        ;widget_control,(*ptr).curx_txt,set_value=txt
        ;widget_control,(*ptr).cury_txt,set_value=txt
    endelse
endif


                                ;If user DOES the Ghost Cells Contour.
if ghostcells eq 1 then begin
    x1=min(lon)
    x2=max(lon)
    y1=min(lat)
    y2=min(lat)
                                ;If user does not want to do the Plot Log.
    if nolog eq 1 then begin
        maxi=max(datatoplot)
        mini=min(datatoplot)
        if mini eq maxi then maxi=mini+1
        levels = findgen(31)/30.0*(maxi-mini) + mini

        contour,datatoplot(*,*),x(*,*),y(*,*),POS=pos,$
          levels=levels,xstyle=1,ystyle=1,$
          xrange=xrange,yrange=yrange,$
          title='Contour Plot Thermosphere',c_colors=clevels,$
          xtitle=xtitle,ytitle=ytitle,/cell_fill,/NOERASE
        
;        if cnt1 eq 1 then begin	
;            if (plotvectoryes eq 1) then begin
;                thermo_plotvectors,vars,selset,data,lat,lon,nlats,nlons,cf,vi_cnt,vn_cnt,step,$
;                  polar, mr								
;            endif
;        endif
        
                                ;Draw grid.
        if (showgridyes eq 1) then begin
            for i=0,ygrids-1 do begin
                oplot,mm(x),[y(0,i),y(0,i)]
            endfor
            for j=0,xgrids-1 do begin
                oplot,[x(j,0),x(j,0)],mm(y)
            endfor
        endif

                                ;If user set cursor position to plot, then do plot of datald.
                                ;Else clean up the cursor text fields on the inteRrface. 
        if cursor_cnt eq 1 then begin
            if cnt1 eq 1 then begin
                plot,datald,alt,xtitle='datald',ytitle='Alt. (deg)'
            endif
            if cnt2 eq 1 then begin
                plot,datald,lat,xtitle='datald',ytitle='Lat. (deg)'
            endif
            if cnt3 eq 1 then begin
                plot,datald,lon,xtitle='datald',ytitle='Long. (deg)'
            endif
        endif else begin	
            txt=''
                                ;widget_control,(*ptr).curx_txt,set_value=txt
                                ;widget_control,(*ptr).cury_txt,set_value=txt
        endelse
    endif                       ;End of if nolog eq 1

                                ;If user does want to do the Plot Log. 
    if yeslog eq 1 then begin
        nzsubs=where(datatoplot gt 0, cnt)
        datatoplot(nzsubs)=datatoplot(nzsubs)
        datatoplot=ALOG10(datatoplot)
        maxi=max(datatoplot)
        mini=min(datatoplot)
        if mini eq maxi then maxi=mini+1
        levels = findgen(31)/30.0*(maxi-mini) + mini	
        
        contour,datatoplot(*,*),x(*,*),y(*,*),POS=pos,$
          levels=levels,xstyle=1,ystyle=1,$
          xrange=xrange,yrange=yrange,$
          title='Contour Plot Thermosphere',c_colors=clevels,$
          xtitle=xtitle,ytitle=ytitle,/cell_fill,/NOERASE

;        if cnt1 eq 1 then begin 
;            if (plotvectoryes eq 1) then begin
;                utrot = 180.0
;                thermo_plotvectors,vars,selset,data,lat,lon,nlats,nlons,cf,vi_cnt,vn_cnt,step,$
;                  polar, mr
;            endif
;        endif
        
                                ;Draw grid.
        if (showgridyes eq 1) then begin
            for i=0,ygrids-1 do begin
                oplot,mm(x),[y(0,i),y(0,i)]
            endfor
            for j=0,xgrids-1 do begin
                oplot,[x(j,0),x(j,0)],mm(y)
            endfor
        endif
        
                                ;If user set cursor position to plot, then do plot of datald.
                                ;Else clean up the cursor text fields on the interface. 
        if cursor_cnt eq 1 then begin
            if cnt1 eq 1 then begin
                plot,datald,alt,xtitle='datald',ytitle='Alt. (deg)'
            endif
            if cnt2 eq 1 then begin
                plot,datald,lat,xtitle='datald',ytitle='Lat. (deg)'
            endif
            if cnt3 eq 1 then begin
                plot,datald,lon,xtitle='datald',ytitle='Long. (deg)'
            endif
        endif else begin	
            txt=''
                                ;widget_control,(*ptr).curx_txt,set_value=txt
                                ;widget_control,(*ptr).cury_txt,set_value=txt
        endelse
    endif                       ;End of if yeslog eq 1
endif                           ;End of if yes eq 1


if (cnt1) then plane = 1
if (cnt2) then plane = 2
if (cnt3) then plane = 3

if (plotvectoryes eq 1) then begin
    if (not polar) then $
      plot,xrange,yrange, /nodata,xstyle=5,ystyle=5,/noerase, pos = pos
    utrot = 0.0

    if (plane eq 1 and not ghostcells and not polar) then begin
        lon = lon + 180.0
        loc = where(lon gt 360.0,count)
        if (count gt 0) then lon(loc) = lon(loc) - 360.0
    endif
    thermo_plotvectors,vars,selset,data,lat, $
      lon, utrot, alt, $
      nlats,nlons,nalts, cf,vi_cnt,vn_cnt,step, polar, mr, plane, npolar
endif 

;stfr = 90.0-67.0
;stft = 309.0 + utrot
;
;cirr = 2.0
;cirt = findgen(17)*2*!pi/16
;cirx = cirr*cos(cirt)
;ciry = cirr*sin(cirt)
;stfx = stfr*cos(stft*!pi/180.0-!pi/2) + cirx
;stfy = stfr*sin(stft*!pi/180.0-!pi/2) + ciry
;polyfill, stfx, stfy, color = 0


                                ;Draw color bar.

;	   pos = [0.82,0.05,0.87,0.96]
pos(0) = pos(2)+0.025
pos(2) = pos(0)+0.03
maxmin = mm(levels)

if ortho then begin
    pos(1) = pos(1) + .1
    pos(3) = pos(3) - .1
endif 

title = variable
plotct,254,pos,maxmin,title,/right,color=color

maxi=max(datatoplot)
mini=min(datatoplot)

r = (maxmin(1) - maxmin(0)) * 0.03

if (mini gt maxmin(0)-r) then begin
    plots, [0,1], [mini, mini], thick = 3
    plots, [0,0.5], [mini, mini-r], thick = 3
    plots, [0,0.5], [mini, mini+r], thick = 3
    if (abs(mini) lt 10000.0 and abs(mini) gt 0.01) then begin
        smin = strcompress(string(mini, format = '(f10.2)'), /remove)
    endif else begin
        smin = strcompress(string(mini, format = '(e12.3)'), /remove)
    endelse
    xyouts, -0.1, mini, smin, align = 0.5, charsize = 0.75, orient = 90
endif

if (maxi lt maxmin(1)+r) then begin

    plots, [0,1], [maxi, maxi], thick = 3
    plots, [0,0.5], [maxi, maxi-r], thick = 3
    plots, [0,0.5], [maxi, maxi+r], thick = 3
    if (abs(maxi) lt 10000.0 and abs(maxi) gt 0.01) then begin
        smax = strcompress(string(maxi, format = '(f10.2)'), /remove)
    endif else begin
        smax = strcompress(string(maxi, format = '(e12.3)'), /remove)
    endelse
    xyouts, -0.1, maxi, smax, align = 0.5, charsize = 0.75, orient = 90
endif

;If user write the result to a file, then closedevice right now. 
;;if yeswrite_cnt eq 1 then begin
;;closedevice
;;mes=widget_message('Done with writing into file!')
;;endif
smini = mini
smaxi = maxi

closedevice

getvalues = 0
if getvalues then begin
   slt = reform(data(13,*,0,0))
;   if n_elements(getslt) eq 0 then getslt = 0.0
;   if n_elements(getlat) eq 0 then getlat = 0.;0
;   if n_elements(getalt) eq 0 then getalt = 0.0
   getslt = 12;float(ask('which SLT: ',tostrf(getslt)))
   getlat = 40;float(ask('which Latitude: ',tostrf(getlat)))
   getalt = 130;float(ask('which Altitude: ',tostrf(getalt)))
   
   
   diff = getlat - lat(0,*,0)
   minlat = min(abs(diff),iminlat)

   diff = 85 - alt(0,0,*)
   minlat = min(abs(diff),iealt)

   diff = getslt - slt
   minslt = min(abs(diff),iminslt)

   diff = getalt - alt(0,0,*)
   minalt = min(abs(diff),iminalt)


   value = data(3,iminslt,iminlat,iminalt)
   tvalue = data(15,iminslt,iminlat,101)
   print, "Density at 130 km: ",value
   print, "Temperature at 250 km: ",tvalue
   evalue = reform(data(30,iminslt,iminlat,iealt:*))
   ealt = reform(alt(0,0,iealt:*))
   plot,evalue,alt(0,0,iealt:*),yrange = [80,250]
   emax = max(evalue,imax)
   print, "Ion peak of "+tostrf(emax)+" at " +tostr(ealt(imax)) + " km"


endif



end
