;--------------------------------------------
;                constq
;
;  Input: constQ.dat
;         constQ2.dat
;
;  Creaded by S.-B. Kang, Code 673, GSFC
;--------------------------------------------
pro constq

iwc=0
ipa=0

FileName1='chorus_constQ.dat'
FileName2='chorus_constQ2.dat'
FileName3='hiss_constQ.dat'
FileName4='hiss_constQ2.dat'

if file_test(FileName1) eq 1 then begin
   openr,1,FileName1
   readf,1,ipc,iwc,ipa
   ckeV=fltarr(iwc)
   a=fltarr(ipc,iwc,ipa)
   a1=fltarr(iwc)
   readf,1,ckeV
   for j=0,ipc-1 do begin
      for m=0,ipa-1 do begin
          readf,1,a1
          a(j,*,m)=a1
      endfor
   endfor
   close,1
endif  ; FileName1

if file_test(FileName2) eq 1 then begin
   openr,1,FileName2
   readf,1,ipc,iq,ipa
   Ec=fltarr(iq)
   a0=findgen(ipa)+1.
   E1=fltarr(ipa)
   E=fltarr(ipc,iq,ipa)
   readf,1,Ec
   for j=0,ipc-1 do begin
       for k=0,iq-1 do begin
           readf,1,E1
           E(j,k,*)=E1
       endfor
   endfor
   close,1
endif ; FileName2
   
if file_test(FileName1) eq 1 then begin
   set_plot,'ps'
   device,filename='diag_'+FileName1+'.ps',/inches,/color,yoffset=0., $
          xoffset=0.1,xsize=6.,ysize=6.
   for j=0,ipc-1 do begin
       ymin=min(a(j,*,*))
       ymax=max(a(j,*,*))
       print,ymin,ymax
       plot,a(j,*,0),Ec,/xs,/ys,$
   	xrange=[0.1,90.],yrange=[ymin,ymax],$
   	/ylog,/noerase,$
   	xtitle='Pitch-angle (deg)',$
   	ytitle='Energy (keV)',$
   	title='Constant Q!D1!N (deg) curve for chorus'
       for m=1,ipa-1 do oplot,a(j,*,m),Ec
       erase
   endfor
   ;;plot,a(*,0),ckeVm,/xs,/ys,xrange=[1.e1,90.],yrange=[1.e0,1.e3],/ylog
   ;;for m=1,ipa-1 do oplot,a(*,m),ckeVm
   device,/close_file
endif ; FileName1

if file_test(FileName2) eq 1 then begin
   pos1=[0.1,0.445,0.688,0.9]
   device,filename='diag_'+FileName2+'.ps',/inches,/color,yoffset=0., $
          xoffset=0.1,xsize=8.5,ysize=11.
   for j=0,ipc-1 do begin
       ymin=min(E)
       ymax=max(E)
       print,ymin,ymax
       ymin=0.1
       plot,a0,E(j,0,*),/xs,/ys,$
   	xrange=[0.1,90.],yrange=[ymin,ymax],$
   	/ylog,/noerase,pos=pos1,$
   	xtitle='Pitch-angle (deg)',$
   	ytitle='Energy (keV)',$
   	title='Constant Q!D2!N (keV) curve for chorus'
       for k=1,iq-1 do oplot,a0,E(j,k,*)
       erase
   endfor
   device,/close_file
endif ; FileName2

if file_test(FileName3) eq 1 then begin
   openr,1,FileName3
   readf,1,ipc,iwc,ipa
   ckeV=fltarr(iwc)
   a=fltarr(ipc,iwc,ipa)
   a1=fltarr(iwc)
   readf,1,ckeV
   for j=0,ipc-1 do begin
      for m=0,ipa-1 do begin
          readf,1,a1
          a(j,*,m)=a1
      endfor
   endfor
   close,1
endif  ; FileName3

if file_test(FileName4) eq 1 then begin
   openr,1,FileName4
   readf,1,ipc,iq,ipa
   Ec=fltarr(iq)
   a0=findgen(ipa)+1.
   E1=fltarr(ipa)
   E=fltarr(ipc,iq,ipa)
   readf,1,Ec
   for j=0,ipc-1 do begin
       for k=0,iq-1 do begin
           readf,1,E1
           E(j,k,*)=E1
       endfor
   endfor
   close,1
endif ; FileName4


if file_test(FileName3) eq 1 then begin
   set_plot,'ps'
   device,filename='diag_'+FileName3+'.ps',/inches,/color,yoffset=0., $
          xoffset=0.1,xsize=6.,ysize=6.
   for j=0,ipc-1 do begin
       ymin=min(a(j,*,*))
       ymax=max(a(j,*,*))
       print,ymin,ymax
       plot,a(j,*,0),Ec,/xs,/ys,$
   	xrange=[0.1,90.],yrange=[ymin,ymax],$
   	/ylog,/noerase,$
   	xtitle='Pitch-angle (deg)',$
   	ytitle='Energy (keV)',$
   	title='Constant Q!D1!N (deg) curve for hiss'
       for m=1,ipa-1 do oplot,a(j,*,m),Ec
       erase
   endfor
   ;;plot,a(*,0),ckeVm,/xs,/ys,xrange=[1.e1,90.],yrange=[1.e0,1.e3],/ylog
   ;;for m=1,ipa-1 do oplot,a(*,m),ckeVm
   device,/close_file
endif ; FileName3

if file_test(FileName4) eq 1 then begin
   pos1=[0.1,0.445,0.688,0.9]
   device,filename='diag_'+FileName4+'.ps',/inches,/color,yoffset=0., $
          xoffset=0.1,xsize=8.5,ysize=11.
   for j=0,ipc-1 do begin
       ymin=min(E)
       ymax=max(E)
       print,ymin,ymax
       ymin=0.1
       plot,a0,E(j,0,*),/xs,/ys,$
   	xrange=[0.1,90.],yrange=[ymin,ymax],$
   	/ylog,/noerase,pos=pos1,$
   	xtitle='Pitch-angle (deg)',$
   	ytitle='Energy (keV)',$
   	title='Constant Q!D2!N (keV) curve for hiss'
       for k=1,iq-1 do oplot,a0,E(j,k,*)
       erase
   endfor
   device,/close_file
endif ; FileName4

end
