filelist_new = file_search('3D*.bin')

nfiles_new = n_elements(filelist_new)

if nfiles_new gt 1 then begin
   ift = 0
   type = ' '
   filetype = strarr(nfiles_new)
   for ifile = 0, nfiles_new - 1 do begin
      l1 = strpos(filelist_new(ifile),'/',/reverse_search)+1
      type = strmid(filelist_new(ifile),l1,5)
      if ifile eq 0 then filetype(0) = type

      if filetype(ift) ne type then begin
         ift = ift + 1
         filetype(ift) = type
         endif
   endfor
   filetype = filetype(0:ift)
   display, filetype
      if n_elements(ft) eq 0 then ft = 0
      ft = fix(ask('which filetype: ',tostr(ft)))
      whichtype = filetype(ft)
      filelist_new = file_search('*'+whichtype+'*.bin')
      nfiles_new = n_elements(filelist_new)
;print, filelist_new
   endif
display, filelist_new
if n_elements(whichfile) eq 0 then whichfile = -1
whichfile = fix(ask('which file to plot (-1 for all): ',tostr(whichfile)))

if whichfile ge 0 then begin
    filelist = file_search(filelist_new(whichfile)) 
    nfiles = 1
endif else begin
    filelist = filelist_new
    nfiles = n_elements(filelist)
endelse 

for iFile = 0, nFiles-1 do begin

    filename = filelist(iFile)

    print, 'Reading file ',filename

    read_thermosphere_file, filename, nvars, nalts, nlats, nlons, $
      vars, data, rb, cb, bl_cnt,time,version

    pl = strmid(tostrf(version),2,1)
    if pl eq '4' then mars = 1 else mars = 0
    mars = 1
    alt = reform(data(2,*,*,*)) / 1000.0
    lat = reform(data(1,*,*,*)) / !dtor
    lon = reform(data(0,*,*,*)) / !dtor
    

    if mars then Rotation_period = 88775.0 else rotation_period = 86400.0
    hoursperday = rotation_period/3600.

;   ivernaltime=[1998,7,14,16,0,0]  ;Mars, Midnight at Vernal Equinox
    ivernaltime=[1998,7,14,13,40,0] ;Mars, Midnight at Vernal Equinox (Fixed, Feb. 2013)
    c_a_to_r,ivernaltime,vernaltime
    c_a_to_r,time,rt
    slt = (rt/3600.0 + lon(*,*,0)*!dtor * HoursPerDay/(2*!pi)) mod HoursPerDay
;    if Mars then begin
;       vars(13) = 'Solar Local Time'
;       for ialt = 0, nalts - 1 do begin
;          data(13,*,*,ialt) = slt
;       endfor
;    endif
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
         year = fix(strmid(filename,07, 2))
         mont = fix(strmid(filename,09, 2))
         day  = fix(strmid(filename,11, 2))
         hour = float(strmid(filename,14, 2))
         minu = float(strmid(filename,16, 2))
         seco = float(strmid(filename,18, 2))
     endelse
     
     if year lt 50 then iyear = year + 2000 else iyear = year + 1900
     stryear = strtrim(string(iyear),2)
     strmth = strtrim(string(mont),2)
     strday = strtrim(string(day),2)
     uttime = hour+minu/60.+seco/60./60.
    
     strdate = stryear+'-'+strmth+'-'+strday
     strdate = strdate(0)
     
     if (iFile eq 0) then begin
         
        if n_elements(sel) eq 0 then sel = 3
        for i=0,nvars-1 do print, tostr(i)+'. '+vars(i)
        sel = fix(ask('which var to plot',tostr(sel)))

        plotlog = ask('whether you want log or not (y/n)','n')
        if (strpos(plotlog,'y') eq 0) then plotlog = 1 else plotlog = 0

        if n_elements(psfile) eq 0 then psfile = 'plot_0000.ps'
        psfile = ask('ps file name',psfile)

        print, '1. Constant Altitude Plot'
        print, '2. Constant Longitude Plot'
        print, '3. Constant Latitude Plot'
        slice = fix(ask('type of plot to make','1'))

        cnt1 = 0
        cnt2 = 0
        cnt3 = 0
        
       
        
;cnt1 is a lat/lon plot
        if (slice eq 1) then cnt1 = 1

;cnt1 is a lat/alt plot
        if (slice eq 2) then cnt3 = 1

;cnt1 is a lon/alt plot
        if (slice eq 3) then cnt2 = 1

        if (slice eq 1) then begin
            for i=0,nalts-1 do print, tostr(i)+'. '+string(alt(2,2,i))
            if n_elements(selset) eq 0 then selset = 0
            selset = fix(ask('which altitude to plot',tostr(selset)))
        endif

        if (slice eq 2) then begin
            for i=0,nlons-1 do print, tostr(i)+'. '+string(lon(i,2,2))
            if n_elements(selset) eq 0 then selset = 0
            selset = fix(ask('which longitude to plot',tostr(selset)))
        endif

        if (slice eq 3) then begin
            for i=0,nlats-1 do print, tostr(i)+'. '+string(lat(2,i,2))
             if n_elements(selset) eq 0 then selset = 0
            selset = fix(ask('which latitude to plot',tostr(selset)))
        endif

        if n_elements(smini) eq 0 then smini = 0.0
        if n_elements(smaxi) eq 0 then smaxi = 0.0
        smini = ask('minimum (0.0 for automatic)',tostrf(smini))
        smaxi = ask('maximum (0.0 for automatic)',tostrf(smaxi))

        if n_elements(pv) eq 0 then pv = 'n'
        plotvector = ask('whether you want vectors or not (y/n)',pv)
        pv = plotvector
        if strpos(plotvector,'y') eq 0 then plotvector=1 else plotvector = 0

        if (plotvector) then begin
            print,'-1  : automatic selection'
            factors = [1.0, 5.0, 10.0, 20.0, 25.0, $
                       50.0, 75.0, 100.0, 150.0, 200.0]
            nfacs = n_elements(factors)
            for i=0,nfacs-1 do print, tostr(i)+'. '+string(factors(i)*10.0)
            if n_elements(vector_factor) eq 0 then vector_factor = -1
            vector_factor = fix(ask('velocity factor',tostr(vector_factor)))
        endif else vector_factor = 0

; cursor position variables, which don't matter at this point
        cursor_x = 0.0
        cursor_y = 0.0
        strx = '0.0'
        stry = '0.0'

; yes is whether ghostcells are plotted or not:
        yes = 0
        no  = 1

; yeslog is whether variable should be logged or not:
        if (plotlog) then begin 
            yeslog = 1
            nolog  = 0
        endif else begin
            yeslog = 0
            nolog = 1
        endelse

; yeswrite_cnt is whether we have to output to a ps file or not.
        yeswrite_cnt = 1

; polar is variable to say whether we have polar plots or not
polar = 0
ortho = 0
       if slice eq 1 then begin
           polar = 0
           ortho = 0
           polar = fix(ask("if rectangular, polar, or ortho (0,1,2): ",tostr(polar)))
           
           if polar eq 2 then begin
               polar = 0
               ortho = 1
            
               if n_elements(tlat) eq 0 then tlat = 0
               if n_elements(tlon) eq 0 then tlon = 0
               tlat = float(ask('ortho center latitude (0.0 for subsolar): ',$
                                tostrf(tlat)))
               tlon = float(ask('ortho center longitude (0.0 for subsolar): ',$
                                tostrf(tlon)))
            
           endif 


       if ortho eq 1 then begin
           if tlat eq 0.0 and tlon eq 0.0 then begin
           
           zsun,strdate,uttime ,0,0,zenith,azimuth,solfac,$
             lonsun=lonsun,latsun=latsun
        
           if lonsun lt 0.0 then lonsun = 360.0 - abs(lonsun)
           
           plat = latsun
           plon = lonsun

           print, 'Coordinates: ',lonsun ,' Long. ',latsun,' Lat.'
       endif else begin
           
           plat = tlat
           plon = tlon

       endelse
   endif
       endif
; npolar is whether we are doing the northern or southern hemisphere
       npolar = 1

; MinLat is for polar plots:
        MinLat = 40.0

; showgridyes says whether to plot the grid or not.
        showgridyes = 0

;plotvectoryes says whether to plot vectors or not
        plotvectoryes = plotvector

; number of points to skip when plotting vectors:
        step = 2

; vi_cnt is whether to plot vectors of Vi
        vi_cnt = 0

; vn_cnt is whether to plot vectors of Vn

        vn_cnt = 1

        cursor_cnt = 0

        xrange = [0.0,0.0]

        yrange = [0.0,0.0]

    endif

    if (nFiles gt 1) then begin
        p = strpos(psfile,'.ps')
        if (p gt -1) then psfile = strmid(psfile,0,p-5)
        psfile_final = psfile+'_'+chopr('000'+tostr(iFile),4)+'.ps'
    endif else begin
        psfile_final = psfile
    endelse
    psfile = psfile_final
    smini_final = smini
    smaxi_final = smaxi

    thermo_plotbatch_v3,cursor_x,cursor_y,strx,stry,step,nvars,sel,nfiles,	$
      cnt1,cnt2,cnt3,yes,no,yeslog,  	  $
      1-yeslog,nalts,nlats,nlons,yeswrite_cnt,$
      polar,npolar,MinLat,showgridyes,	  $
      plotvectoryes,vi_cnt,vn_cnt,vector_factor,	  $
      cursor_cnt,data,alt,lat,lon,	  $
      xrange,yrange,selset,smini_final,smaxi_final,	  $
      filename,vars, psfile_final, mars, 'all',itime,ortho,plat,plon

endfor


end
