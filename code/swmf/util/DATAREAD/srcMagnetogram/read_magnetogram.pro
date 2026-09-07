;For reading magnetograms in FITS or BATS-R-US format.

FUNCTION read_magnetogram, file, PlotRadius, UseBATS

  if UseBATS then begin
                                ; Setup common block for BATSRUS/Idl
     common getpict_param, filename
     common file_head
     common plot_data, grid, x, w

     filename = file
     read_data

     if gencoord then begin
        print, 'file '+file+' should contain a regular grid'
        retall
     endif

     case ndim of
        2:begin
           if PlotRadius ne 1.0 then begin
              print,'PlotRadius cannot be specified with 2D data!'
              retall
           endif

           if variables(0) ne "Longitude" or variables(1) ne "Latitude" or $
              variables(2) ne 'Br' then begin
              print, 'variables should be Longitude Latitude Br!'
              retall
           endif

           nlon = nx[0]
           nlat = nx[1]
           
           mag_info = {nlon:nlon,$
                       nlat:nlat,$
                       time:time,$
                       longitude:fltarr(nlon,nlat),$
                       latitude:fltarr(nlon,nlat),$
                       br_field:fltarr(nlon,nlat),$
                       blon_field:fltarr(nlon,nlat),$
                       blat_field:fltarr(nlon,nlat),$
                       occPos:fltarr(nlon,nlat),$
                       occNeg:fltarr(nlon,nlat),$
                       neqpar:neqpar,$
                       eqpar:fltarr(neqpar)}
           mag_info.longitude = x(*,*,0)*!dtor
           mag_info.latitude  = x(*,*,1)*!dtor
           mag_info.br_field = w(*,*,0)
           mag_info.eqpar    = eqpar
           if nw ge 2 then mag_info.blon_field = w(*,*,1)
           if nw ge 3 then mag_info.blat_field = w(*,*,2)
           if nw ge 4 then mag_info.occPos = w(*,*,3)
           if nw ge 5 then mag_info.occNeg = w(*,*,4)
           
        end

        3:begin
           if variables(0) ne "Radius" or variables(1) ne "Longitude" or $
              variables(2) ne "Latitude" or variables(3) ne 'Br' then begin
              print, 'variables should be Radius Longitude Latitude Br!'
              retall
           endif
           
           nlon = nx[1] - 1
           nlat = nx[2]
           
           mag_info = {nlon:nlon,$
                       nlat:nlat,$
                       time:time,$
                       longitude:fltarr(nlon,nlat),$
                       latitude:fltarr(nlon,nlat),$
                       br_field:fltarr(nlon,nlat),$
                       blon_field:fltarr(nlon,nlat),$
                       blat_field:fltarr(nlon,nlat),$
                       neqpar:neqpar,$
                       eqpar:fltarr(neqpar)}

           radius = x(*,0,0,0)
           longitude = x(0,*,*,1)
           latitude  = x(0,*,*,2)
           
                                ; find index for the cut                                             
           d = abs(radius - PlotRadius)
           icut = where( d eq min(d) )
           br_field     = w(icut,*,*,0)
           bphi_field   = w(icut,*,*,1)
           btheta_field = w(icut,*,*,2)
           
           mag_info.br_field = reform(br_field[0,0:nlon-1,*])
           mag_info.blon_field = reform(blon_field[0,0:nlon-1,*])
           mag_info.blat_field = reform(blat_field[0,0:nlon-1,*])
           mag_info.longitude = reform(longitude[0,0:nlon-1,*])
           mag_info.latitude = reform(latitude[0,0:nlon-1,*])
        end
        else: begin
           print, 'ndim=', ndim, ' should be 2 or 3'
           retall
        end
     endcase
  endif else begin
                                ;read magnetogram in FITS format. For
                                ;transfering to Python, read_fits
                                ;function can be replaced by astropy
                                ;function.

     ; read_fits and readfits give the same result.
     ;except readfits now has options to recognize 
     ;sintheta/theta grids in latitude


;     br_field=readfits(file,index,/noscale)
     br_field=readfits(file,index,StringHeader,LongShift,CRnumber,IsUniformLat,/noscale)
     
     s=size(br_field)
     nlon=s[1]
     nlat=s[2]
     
     mag_info = {nlon:nlon,$
                 nlat:nlat,$
                 longitude:fltarr(nlon,nlat),$
                 latitude:fltarr(nlon,nlat),$
                 br_field:fltarr(nlon,nlat),$
                 blon_field:fltarr(nlon,nlat),$
                 blat_field:fltarr(nlon,nlat),$
                 neqpar:0, eqpar:fltarr(1)}
     
    ;;;; already assumes that it is sin theta grid


     if(IsUniformLat EQ 0)then begin
        lat=findgen(nlat)*2./nlat
        lat=asin(lat-lat[nlat-1]/2.)
     endif else begin
        lat=findgen(nlat)*!dtor
        lat = lat - lat[nlat-1]/2.
     endelse
     print,lat/!dtor
     lon=findgen(nlon)*!DPI*2./nlon
     latitude=fltarr(nlon,nlat)
     longitude=fltarr(nlon,nlat)
     for i=0,nlon-1 do begin
        for j=0,nlat-1 do begin
           latitude[i,j]=lat[j]
           longitude[i,j]=lon[i]
        endfor
     endfor
     
     mag_info.longitude=longitude
     mag_info.latitude=latitude
     mag_info.br_field=br_field
  endelse

  return, mag_info

end
