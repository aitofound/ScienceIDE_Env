;; This code can convert SDO fits files into the HDF5 files used
;; by the open source HipFT code. Note that several things are
;; hard coded here, such as number of days in February and the selected
;; year and months. Note that read_sdo comes for SolarSoft.

pro hipft_hmi2h5

default,year,2017
default,hipft_dir,'/Users/qplazmfree/Documents/research/hipft/HipFT-main'
default,hmi_dir,'/Volumes/PortableSSD/hipft/HMI/'+strtrim(string(year),2)
default,mg_dir,hipft_dir+'/run_2017/mg'

days_in_month=[31,28,31,30,31,30,31,31,30,31,30,31]

dim1=(findgen(1024))/1023*2.*!dpi
dim2=(findgen(512))/511*!dpi
dim3=findgen(3)

window,xs=800,ys=800
openw,lun,mg_dir+'/map.csv',/get_lun
printf,lun,'target_datetime_utc,obs_datetime_utc,obs_jd,map_path'

for month=7,8 do begin
  
  directory_month=mg_dir+'/'+strtrim(string(year),2)+'/'+string(month,format='(i02)')
  if not file_test(directory_month) then file_mkdir,directory_month
  
  for day=1,days_in_month[month-1] do begin
    
    directory_day=directory_month+'/'+string(day,format='(i02)')
    if not file_test(directory_day) then file_mkdir,directory_day
    
    files=file_search(hmi_dir+'/'+strtrim(string(month),2)+'/'+strtrim(string(day),2),'*.fits',count=nfiles)
    
    if nfiles gt 0 then begin
      for ifile=0,nfiles-1 do begin
        
        file=files[ifile]
        h5_filename=directory_day+'/'+file_basename(file,'.fits')+'.h5'
        oclock=strmid(file_basename(file),20,2)
        print,oclock
        
        read_sdo,file,index,data
        printf,lun,strmid(index.DATE_D$OBS,0,19)+','+strmid(index.DATE_D$OBS,0,19)+','+string(julday(month,day,year,oclock),format='(f13.5)')+$
          ','+strtrim(string(year),2)+'/'+string(month,format='(i02)')+'/'+string(day,format='(i02)')+$
          '/'+file_basename(file,'.fits')+'.h5'
        
        if not file_test(h5_filename) then begin
          
          hipft_disk2hgr,index,data,data_hgr,weight,mu
          plot_image,data_hgr,max=100,min=-100,posi=[0,0,1,0.5],/noerase
          plot_image,mu,max=1,min=-1,posi=[0,0.5,1,1],/noerase

          fid = H5F_CREATE(h5_filename)
          data = fltarr(1024,512,3)
          data[*,*,0]=data_hgr
          data[*,*,1]=weight
          data[*,*,2]=mu

          ;; get data type and space, needed to create the dataset

          datatype_id = H5T_IDL_CREATE(data)
          dataspace_id = H5S_CREATE_SIMPLE(size(data,/DIMENSIONS))
          dataset_id = H5D_CREATE(fid,'Data',datatype_id,dataspace_id)
          H5D_WRITE,dataset_id,data
          
          datatype_id = H5T_IDL_CREATE(dim1)
          dataspace_id = H5S_CREATE_SIMPLE(size(dim1,/DIMENSIONS))
          dataset_id = H5D_CREATE(fid,'dim1',datatype_id,dataspace_id)
          H5D_WRITE,dataset_id,dim1

          datatype_id = H5T_IDL_CREATE(dim2)
          dataspace_id = H5S_CREATE_SIMPLE(size(dim2,/DIMENSIONS))
          dataset_id = H5D_CREATE(fid,'dim2',datatype_id,dataspace_id)
          H5D_WRITE,dataset_id,dim2
          
          datatype_id = H5T_IDL_CREATE(dim3)
          dataspace_id = H5S_CREATE_SIMPLE(size(dim3,/DIMENSIONS))
          dataset_id = H5D_CREATE(fid,'dim3',datatype_id,dataspace_id)
          H5D_WRITE,dataset_id,dim3
          
          H5D_CLOSE,dataset_id
          H5S_CLOSE,dataspace_id
          H5T_CLOSE,datatype_id
          H5F_CLOSE,fid
        endif
      endfor
    endif
  endfor
endfor

close,lun

end



pro hipft_disk2hgr,head_hmi,map_hmi,map_hmi_hgr,weight,mu
  wcs_hmi = fitshead2wcs(head_hmi)
  coord_hmi = wcs_get_coord(wcs_hmi)
  
  coord_hmi=rebin(coord_hmi,2,2048,2048)
  map_hmi=rebin(map_hmi,2048,2048)
  
  x1_hmi=wcs_hmi.POSITION.DSUN_OBS*cos(coord_hmi[1,*,*]*!dtor/3600.)*sin(coord_hmi[0,*,*]*!dtor/3600.)
  z1_hmi=wcs_hmi.POSITION.DSUN_OBS*sin(coord_hmi[1,*,*]*!dtor/3600.)
  y1_hmi=-sqrt(6.96e8^2-x1_hmi^2-z1_hmi^2)

  z_hmi=z1_hmi*cos(wcs_hmi.POSITION.CRLT_OBS*!dtor)-y1_hmi*sin(wcs_hmi.POSITION.CRLT_OBS*!dtor)
  y_hmi=y1_hmi*cos(wcs_hmi.POSITION.CRLT_OBS*!dtor)+z1_hmi*sin(wcs_hmi.POSITION.CRLT_OBS*!dtor)

  CRLN_hmi=acos(-y_hmi/sqrt(y_hmi^2+x1_hmi^2))*sgn(x1_hmi)
  CRLN_hmi=reform(wcs_hmi.POSITION.CRLN_OBS+CRLN_hmi/!dtor)
  CRLA_hmi=reform(asin(z_hmi/6.96e8)/!dtor)
  CRLN_hmi[where(CRLN_hmi gt 360.)]-=360
  CRLN_hmi[where(CRLN_hmi lt 0.)]+=360
  
  nth=512
  nph=1024
  th_list=(findgen(nth)+0.5)/nth*180
  ph_list=(findgen(nph)+0.5)/nph*360
  th_all=transpose(rebin(th_list,nth,nph))
  ph_all=rebin(ph_list,nph,nth)
  
  dth_all=th_all-90-wcs_hmi.POSITION.CRLT_OBS
  dph_all=ph_all-wcs_hmi.POSITION.CRLN_OBS

  posi_ph=interpol(findgen(nph),ph_list,CRLN_hmi)
  posi_th=interpol(findgen(nth),th_list,-CRLA_hmi+90)
  
  max_inc=80.*!dtor

  map_hmi_hgr=fltarr(nph,nth)
  mu=cos(dph_all*!dtor)*cos(dth_all*!dtor)
  weight=mu^4.
  weight[where(abs(dph_all) gt 90. xor abs(dth_all) gt 90.)]=0.
  count=fltarr(nph,nth)

  inclination=asin(sqrt(coord_hmi[0,*,*]^2+coord_hmi[1,*,*]^2)/(6.96e8/(wcs_hmi.POSITION.DSUN_OBS*!dtor/3600.)))
  flag=where(inclination lt max_inc and finite(posi_ph) and finite(posi_th))
  
  
  for i=0,n_elements(flag)-1 do begin
    ii=flag[i]
    posi_ph1=posi_ph[ii]
    posi_th1=posi_th[ii]
    inclination1=inclination[ii]
    map_hmi_hgr[posi_ph1,posi_th1]+=map_hmi[ii]/cos(inclination1)
    count[posi_ph1,posi_th1]+=1
  endfor
  map_hmi_hgr=map_hmi_hgr/count
  map_hmi_hgr[where(~finite(map_hmi_hgr))]=0.
end



