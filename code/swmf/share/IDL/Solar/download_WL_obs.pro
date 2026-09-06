; This script is used to download SOHO/LASCO C2,C2,
; STEREO A/B COR1, COR2 white light coronagraph images
; and make movies
; USER has to set the ObsTime & ObsDir & downloading inst ONLY.
; Last Update:
; NishthaS - Dec 30,2023

Obstime='2023-01-05T11:00:00'
ObsDir='../OBSERVATIONS/2023/'

;Flags to select the download data
download_lascoc2=0
download_lascoc3=0
download_cor1a=0
download_cor2a=1
download_cor1b=0
download_cor2b=0

;Time window to download the data in hours. Since the high time
;resolution of SDO/AIA observation, twin1 is used for AIA. twin is
;used for all the other observation.
twin=5
twincor = 3
twin1=0.1

;Display Flag, if set to non-zero, the raw observational images will be
;displayed after downloading.
display_check=0


;Set the Time Range
InTime = utc2tai(anytim2utc(Obstime))

; Set the interval to include 1 hour before CME lasco time and then upto 5 hrs)
TimeStringMinus = strjoin(strsplit(utc2str(tai2utc(InTime - 3600.)),'-', $
                                   /extract) , '/')
TimeStringPlus = strjoin(strsplit(utc2str(tai2utc(InTime + twin*3600.)),'-', $
                                  /extract) , '/')
if download_cor1a eq 1 then begin ; 5 mins before to 3 hours 
   TimeStringMinus = strjoin(strsplit(utc2str(tai2utc(InTime - 300.)),'-', $
                                      /extract) , '/')
   TimeStringPlus = strjoin(strsplit(utc2str(tai2utc(InTime + twin*3600.)), $
                                     '-', /extract) , '/')
endif
TimeRange = TimeStringMinus + '-' + TimeStringPlus
print,'TimeRange for download = ', TimeRange

;----------Download Part -----------------------------------------------
if download_lascoc2 then begin
   List_lascoc2 = vso_search(date=TimeRange,source='SOHO',inst='LASCO',$
                             det='C2',count=count)
; gets the metadata of the records found
   ListTime_lascoc2 = utc2tai(List_lascoc2.Time.Start)
endif

if download_lascoc3 then begin
   List_lascoc3 = vso_search(date=TimeRange,source='SOHO',inst='LASCO',$
                             det='C3')
   ListTime_lascoc3 = utc2tai(List_lascoc3.Time.Start)
endif

if download_cor1a then begin
   List_cor1a = vso_search(date=TimeRange,source='STEREO_A',inst='SECCHI',$
                           det='COR1')
   ListTime_cor1a = utc2tai(List_cor1a.Time.Start)
endif

if download_cor2a then begin
   List_cor2a = vso_search(date=TimeRange,source='STEREO_A',inst='SECCHI',$
                           det='COR2')
   ListTime_cor2a = utc2tai(List_cor2a.Time.Start)
endif

if download_cor1b then begin
   List_cor1b = vso_search(date=TimeRange,source='STEREO_B',inst='SECCHI',$
                           det='COR1')
   ListTime_cor1b = utc2tai(List_cor1b.Time.Start)
endif

if download_cor2b then begin
   List_cor2b = vso_search(date=TimeRange,source='STEREO_B',inst='SECCHI',$
                           det='COR2')
   ListTime_cor2b = utc2tai(List_cor2b.Time.Start)
endif

if download_lascoc2 then begin
;download LASCO C2 data
   IsFolder = file_search(Obsdir+'c2/',count=nfolder)
   if (nfolder eq 0) then file_mkdir,Obsdir+'c2/'
   FitsDir= ObsDir+'c2/'
   Match = where(List_lascoc2.Size gt 600., Count)
   if Count lt 1 then Match = where(List_lascoc2.Size gt 0., Count)
   
   if Count lt 1 then begin
      print,'ERROR!'
      print,'get_save_obs: could not find an image for LASCO C2'
      print,'For time range: ', TimeRange
      print,'EXITING'
   endif
   
   ObsTimeBand_I = ListTime_lascoc2[Match] ;  these are the non-zero items
   Index_I = where(abs(ListTime_lascoc2 - InTime) ge $
                   min(abs(ObsTimeBand_I - InTime)))
   i=0
   FileNames_I = strarr(Count)
   while i lt Count do begin
      StringLine = strsplit(List_lascoc2[Index_I[i]].fileid,'/', /extract)
      nStrings = n_elements(StringLine)
      ;; extracting the filename from the FileId
      FileNames_I[i]= StringLine[nStrings-1] 
      i =i+1
   endwhile
   GoodList = List_lascoc2[Index_I]
   print,'Downloading LASCO C2 images for times: ', GoodList.Time.Start
   
   Status = vso_get(GoodList,out_dir=FitsDir)
   FileNames_I = FitsDir + FileNames_I
   ;;   print,'Status'
   ;;   print_struct,Status
   print,''
   print,'----- DONE DOWNLOADING IMAGES -----'
   print,'LASCO C2: ', FileNames_I
   print,''
   
   if display_check then begin
      mreadfits,filenames_I,index,image
      image=MK_IMG(filenames_I,0,255,/mask_occ,/do_bytscl)
      index2map,index,image,map
      window,/free,xs=500,ys=500
      loadct,3
      plot_map,map
   endif
   filename_c2=filenames_I
endif

if download_lascoc3 then begin
   IsFolder = file_search(Obsdir+'c3/',count=nfolder)
   if (nfolder eq 0) then file_mkdir,Obsdir+'c3/'
   FitsDir=ObsDir+'c3/'
   
;download LASCO C3 data
   Match = where(List_lascoc3.Size gt 600., Count)
   if Count lt 1 then Match = where(List_lascoc3.Size gt 0., Count)

   if Count lt 1 then begin
      print,'ERROR!'
      print,'get_save_obs: could not find an image for LASCO C3'
      print,'For time range: ', TimeRange
      print,'EXITING'
   endif

   ObsTimeBand_I = ListTime_lascoc3[Match]
   Index_I = where(abs(ListTime_lascoc3 - InTime) ge $
                   min(abs(ObsTimeBand_I - InTime)))
   i=0
   FileNames_I = strarr(Count)
   while i lt Count do begin
      StringLine = strsplit(List_lascoc3[Index_I[i]].fileid,'/', /extract)
      nStrings = n_elements(StringLine)
      FileNames_I[i]= StringLine[nStrings-1]
      i=i+1
   endwhile
   GoodList = List_lascoc3[Index_I]
   print,'Downloading LASCO C3 images for times: ', GoodList.Time.Start
   
   Status = vso_get(GoodList,out_dir=FitsDir)
   FileNames_I = FitsDir + FileNames_I

   print,''
   print,'----- DONE DOWNLOADING IMAGES -----'
   print,'LASCO C3: ', FileNames_I
   print,''
   
   if display_check then begin
      mreadfits,filenames_I,index,image
      image=MK_IMG(filenames_I,0,255,/mask_occ,/do_bytscl)
      index2map,index,image,map
      window,/free,xs=500,ys=500
      loadct,3
      plot_map,map
   endif
   
   filename_c3=filenames_I
endif

if download_cor1a then begin
   IsFolder = file_search(Obsdir+'cor1a/',count=nfolder)
   if (nfolder eq 0) then file_mkdir,Obsdir+'cor1a/'
   FitsDir=ObsDir+'cor1a/'
   ;; download COR1A data
   Match = where(List_cor1a.Size gt 600., Count)
   if Count lt 1 then Match = where(List_cor1a.Size gt 0., Count)
   
   if Count lt 1 then begin
      print,'ERROR!'
      print,'get_save_obs: could not find an image for COR1A'
      print,'For time range: ', TimeRange
      print,'EXITING'
   endif

   ObsTimeBand_I = ListTime_cor1a[Match]
   Index_I = where(abs(ListTime_cor1a - InTime) ge $
                   min(abs(ObsTimeBand_I - InTime)))
   i=0
   FileNames_I = strarr(Count)
   while i lt Count do begin
      StringLine = strsplit(List_cor1a[Index_I[i]].fileid,'/', /extract)
      nStrings = n_elements(StringLine)
      FileNames_I[i] = StringLine[nStrings-1]
      i=i+1
   endwhile
   GoodList = List_cor1a[Index_I]
   
   print,'Downloading COR1A images for times: ', GoodList.Time.Start

   Status = vso_get(GoodList,out_dir=FitsDir)
   FileNames_I = FitsDir + FileNames_I
   
   print,''
   print,'----- DONE DOWNLOADING IMAGES -----'
   print,'STEREO COR1A: ', FileNames_I
   print,''
   
   filenames_cor1a=filenames_I
   if display_check then begin
      mreadfits,filenames_I,index,image
      image=MK_IMG(filenames_I,0,255,/mask_occ,/do_bytscl)
      index2map,index,image,map
      window,/free,xs=500,ys=500
      loadct,3
      plot_map,map
   endif
endif

if download_cor2a then begin
   IsFolder = file_search(Obsdir+'cor2a/',count=nfolder)
   if (nfolder eq 0) then file_mkdir,Obsdir+'cor2a/'
   FitsDir=ObsDir+'cor2a/'
   ;; download COR2A data
   Match = where(List_cor2a.Size gt 600., Count)
   if Count lt 1 then Match = where(List_cor2a.Size gt 0., Count)

   if Count lt 1 then begin
      print,'ERROR!'
      print,'get_save_obs: could not find an image for COR2A'
      print,'For time range: ', TimeRange
      print,'EXITING'
   endif

   ObsTimeBand_I = ListTime_cor2a[Match]
   Index_I = where(abs(ListTime_cor2a - InTime) ge $
                   min(abs(ObsTimeBand_I - InTime)))

   i=0
   FileNames_I = strarr(Count)
   while i lt Count do begin
      StringLine = strsplit(List_cor2a[Index_I[i]].fileid,'/', /extract)
      nStrings = n_elements(StringLine)
      FileNames_I[i] = StringLine[nStrings-1]
      i=i+1
   endwhile
   GoodList = List_cor2a[Index_I]
   
   print,'Downloading COR2A images for times: ', GoodList.Time.Start

   Status = vso_get(GoodList,out_dir=FitsDir)
   FileNames_I = FitsDir + FileNames_I

   print,''
   print,'----- DONE DOWNLOADING IMAGES -----'
   print,'STEREO COR2A: ', FileNames_I
   print,''

   filenames_cor2a=filenames_I

   if display_check then begin
      mreadfits,filenames_I,index,image
      image=MK_IMG(filenames_I,0,255,/mask_occ,/do_bytscl)
      index2map,index,image,map
      window,/free,xs=500,ys=500
      loadct,3
      plot_map,map
   endif
endif

if download_cor1b then begin
   IsFolder = file_search(Obsdir+'cor1b/',count=nfolder)
   if (nfolder eq 0) then file_mkdir,Obsdir+'cor1b/'
   FitsDir=ObsDir+'cor1b/'
   ;; download COR1B data
   Match = where(List_cor1b.Size gt 600., Count)
   if Count lt 1 then Match = where(List_cor1b.Size gt 0., Count)
   
   if Count lt 1 then begin
      print,'ERROR!'
      print,'get_save_obs: could not find an image for COR1B'
      print,'For time range: ', TimeRange
      print,'EXITING'
   endif

   ObsTimeBand_I = ListTime_cor1b[Match]
   Index_I = where(abs(ListTime_cor1b - InTime) ge $
                   min(abs(ObsTimeBand_I - InTime)))
   i=0
   FileNames_I = strarr(Count)
   while i lt Count do begin
      StringLine = strsplit(List_cor1b[Index_I[i]].fileid,'/', /extract)
      nStrings = n_elements(StringLine)
      FileNames_I[i] = StringLine[nStrings-1]
   endwhile
   GoodList = List_cor1b[Index_I]
   
   print,'Downloading COR1B images for times: ', GoodList.Time.Start
   
   Status = vso_get(GoodList,out_dir=FitsDir)
   FileNames_I = FitsDir + FileNames_I
   
   print,''
   print,'----- DONE DOWNLOADING IMAGES -----'
   print,'STEREO COR1B: ', FileNames_I
   print,''
   
   filenames_cor1b=filenames_I
   
   if display_check then begin
      mreadfits,filenames_I,index,image
      image=MK_IMG(filenames_I,0,255,/mask_occ,/do_bytscl)
      index2map,index,image,map
      window,/free,xs=500,ys=500
      loadct,3
      plot_map,map
   endif
endif

if download_cor2b then begin
   IsFolder = file_search(Obsdir+'cor2b/',count=nfolder)
   if (nfolder eq 0) then file_mkdir,Obsdir+'cor2b/'
   FitsDir = ObsDir+'cor2b/'
;download COR2B data
   Match = where(List_cor2b.Size gt 600., Count)
   if Count lt 1 then Match = where(List_cor2b.Size gt 0., Count)
   
   if Count lt 1 then begin
      print,'ERROR!'
      print,'get_save_obs: could not find an image for COR2B'
      print,'For time range: ', TimeRange
      print,'EXITING'
   endif

   ObsTimeBand_I = ListTime_cor2b[Match]
   Index_I = where(abs(ListTime_cor2b - InTime) ge $
                   min(abs(ObsTimeBand_I - InTime)))
   i = 0
   FileNames_I =strarr(Count)
   while i lt Count do begin
      StringLine = strsplit(List_cor2b[Index_I[i]].fileid,'/', /extract)
      nStrings = n_elements(StringLine)
      FileNames_I[i] = StringLine[nStrings-1]
   endwhile
   GoodList = List_cor2b[Index_I]
   
   print,'Downloading COR2B images for times: ', GoodList.Time.Start

   Status = vso_get(GoodList,out_dir=FitsDir)
   FileNames_I = FitsDir + FileNames_I
   
   print,''
   print,'----- DONE DOWNLOADING IMAGES -----'
   print,'STEREO COR2B: ', FileNames_I
   print,''
   
   filenames_cor2b=filenames_I
   if display_check then begin
      mreadfits,filenames_I,index,image
      image=MK_IMG(filenames_I,0,255,/mask_occ,/do_bytscl)
      index2map,index,image,map
      window,/free,xs=500,ys=500
      loadct,3
      plot_map,map
   endif
endif

;------------ Prepping the data ------------------------

print,'Prepping the data now ! '

if download_lascoc2 then begin
   for i =0,n_elements(filename_c2)-1 do begin
      reduce_level_1,filename_c2(i),SAVEDIR=ObsDir+'c2/',OUTFILE=outfile
      print,'LASCO-C2 Level 1 fits filename = ',outfile
   endfor
endif

if download_lascoc3 then begin
   for i =0,n_elements(filename_c3)-1 do begin
      reduce_level_1,filename_c3(i),SAVEDIR=ObsDir+'c3/',OUTFILE=outfile
      print,'LASCO-C3 Level 1 fits filename = ',outfile
   endfor
endif

if download_cor1a then begin
   for i=0,n_elements(filenames_cor1a)-1 do begin
      secchi_prep,filenames_cor1a(i),index,data,SAVEPATH=ObsDir+'cor1a/',$
                  /rotate_on,/WRITE_FITS
      ;;   secchi_prep,filenames_cor1a,SAVEPATH='../obsdata/2017/',/rotate_on,/WRITE_FITS
   endfor
endif

if download_cor2a then begin
   for i=0,n_elements(filenames_cor2a)-1 do begin
      secchi_prep,filenames_cor2a(i),index,data,SAVEPATH=ObsDir+'cor2a/',$
                  /rotate_on,/WRITE_FITS
   endfor
endif

if download_cor1b then begin
   for i=0,n_elements(filenames_cor1b)-1 do begin
      secchi_prep,filenames_cor1b(i),index,data,SAVEPATH=ObsDir+'cor1b/',$
                  /rotate_on,/write_fts
   endfor
endif

if download_cor2b then begin
   for i=0,n_elements(filenames_cor2b)-1 do begin
      secchi_prep,filenames_cor2b(i),index,data,SAVEPATH=ObsDir+'cor2b/',$
                  /rotate_on,/write_fts
   endfor
endif
end
