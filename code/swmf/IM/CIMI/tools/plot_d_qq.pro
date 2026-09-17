;Processor to graphicalze Diffusion coefficient of Albert's form
; Modified at 2014.01.07
 function stickformat, axis, index, val
    pow=alog10(val)
    pow=strtrim(string(pow,format='(I)'),2)
    return, '10!U'+pow+'!N'
 end
 pro plot_d_qq

 lambda0=35. ; in degree
 meanAmp=10^0.75/0.04/alog(10.)*(exp(0.04*lambda0*alog(10.))-1)/35.
 meanP=meanAmp*meanAmp
 print,'mean Amplitude : ',meanP
 
 c_speed=2.998e8  ; in m/s
 e_c=1.602e-19    ; charge of e- in C
 E0=511.          ; e- rest mass in keV 

 header='' 

;-------------------------------------------
; Read chorus
 fname='D_LBchorus_QQ'
 openr,2,fname+'.dat'
 readf,2,Cpower,Lc0
 readf,2,ipc,iwc
 ipa=89
 Daa=dblarr(ipc,ipa,iwc) & Dqq=Daa
 fpefce=fltarr(ipc) & Q=fltarr(iwc)
 a0=findgen(ipa)+1. 
 readf,2,fpefce
 readf,2,Q
 daa1=0.d
 dqq1=0.d
 for j=0,ipc-1 do begin
     for k=0,iwc-1 do begin
         readf,2,header
         readf,2,header
         for m=0,ipa-1 do begin
             readf,2,pa1,daa1,dqq1
             if daa1 gt 1.e-30 then Daa(j,m,k)=daa1 else Daa(j,m,k)=1.e-30
             if dqq1 gt 1.e-30 then Dqq(j,m,k)=dqq1 else Dqq(j,m,k)=1.e-30
         endfor
     endfor
 endfor
 close,2

 logDaa=alog10(Daa) & logDqq=alog10(Dqq) 
    
 dmax=max(logDaa)
 print,'max of Daa=',dmax
 print,'min of Daa=',min(logDaa)
 read,'Maximum of Daa => ',dmax
 ;dmin=dmax-6.
 dmin=dmax-7.
 ;if max(logDaa) gt Dmax then logDaa(where(logDaa gt Dmax))=Dmax*0.9999
 ;if max(logDEE) gt Dmax then logDEE(where(logDEE gt Dmax))=Dmax*0.9999
 ;if max(logDaE) gt Dmax then logDaE(where(logDaE gt Dmax))=Dmax*0.9999
 
; set level
 nlevel=60
 colrmax=254 & colrmin=1
 clvl=intarr(nlevel) & lvl=fltarr(nlevel)
 dlvl=(dmax-dmin)/(nlevel-1)
 dcolr=float(colrmax-colrmin)/(nlevel-1)
 for n=0,nlevel-1 do begin
     clvl(n)=colrmin+round(dcolr*n)
     lvl(n)=dmin+dlvl*n
 endfor
 
 loadct,33  ; red-blue
 tvlct, red, green, blue,/get
 tvlct,0,0,0,0 ; dark red color to white in red-blue table
 tvlct,255,255,255,255 ; dark red color to black in red-blue table
 !p.background=255
 !p.color=0

 
 yrg=[0.0001,10.]      ; in MeV
 psn=[0.90,0.15,0.95,0.9]
 psn1=[0.1,0.15,0.82,0.9]
 psnD=fltarr(3,3,4)
 psnD(0,0,0:3)=[0.07,0.76,0.29,0.92]
 psnD(0,1,0:3)=[0.35,0.76,0.57,0.92]
 psnD(0,2,0:3)=[0.63,0.76,0.85,0.92]
 psnD(1,0,0:3)=[0.07,0.52,0.29,0.68]
 psnD(1,1,0:3)=[0.35,0.52,0.57,0.68]
 psnD(1,2,0:3)=[0.63,0.52,0.85,0.68]
 psnD(2,0,0:3)=[0.07,0.28,0.29,0.44]
 psnD(2,1,0:3)=[0.35,0.28,0.57,0.44]
 psnD(2,2,0:3)=[0.63,0.28,0.85,0.44]

 set_plot,'ps'
 device,filename=fname+'.ps',/color,/inches,xsize=8.5,ysize=11.,xoffset=0.,yoffset=0.
 pe=-1
 new_plot:
 pe=pe+1
 if pe ge ipc then goto, end_plot
    x1=0.85+0.03
    x2=x1+0.03
    y0=0.76
    yf=0.92
    ym=0.5*(y0+yf)
    dy=(yf-y0)/(nlevel-1)
    xyouts,0.07,0.95,'fpe/fce='+string(fpefce(pe),'(f6.2)'),/normal
    contour,logDaa(pe,*,*),a0,Q*1.e-3, yrange=yrg,xrange=[0,90],$
        /ylog,xticks=3,/fill,levels=lvl,c_color=clvl,title='D!d!7aa!x!n',$
        xstyle=1,ystyle=1,charsize=1,position=psnD(0,0,*),ytitle='Q (MeV)',/noerase
    contour,logDqq(pe,*,*),a0,Q*1.e-3, yrange=yrg,xrange=[0,90],$
        /ylog,xticks=3,/fill,levels=lvl,c_color=clvl,title='D!dQ2Q2!n/Q!u2!n',$
        xstyle=1,ystyle=1,charsize=1,position=psnD(0,1,*),/noerase
    for i=0,nlevel-1 do begin
        y1=y0+i*dy
        y2=y1+1.02*dy
        polyfill,[x1,x2,x2,x1,x1],[y1,y1,y2,y2,y1],color=clvl(i),/normal
    endfor
    axis,x2,ym,yaxis=1,yrange=[10.^dmin,10.^dmax],yticks=6,/ylog,ystyle=1,$
        color=0,charsize=1,ytitle='(1/sec)',ytickformat='stickformat',/normal
 erase
 goto,new_plot
 end_plot:
 device,/close


;-------------------------------------------
; Read hiss  
 fname='D_hiss_QQ'
 openr,2,fname+'.dat'
 readf,2,Cpower,Lc0
 readf,2,ipc,iwc
 ipa=89
 Daa=dblarr(ipc,ipa,iwc) & Dqq=Daa
 fpefce=fltarr(ipc) & Q=fltarr(iwc)
 a0=findgen(ipa)+1. 
 readf,2,fpefce
 readf,2,Q
 daa1=0.d
 dqq1=0.d
 for j=0,ipc-1 do begin
     for k=0,iwc-1 do begin
         readf,2,header
         readf,2,header
         for m=0,ipa-1 do begin
             readf,2,pa1,daa1,dqq1
             if daa1 gt 1.e-30 then Daa(j,m,k)=daa1 else Daa(j,m,k)=1.e-30
             if dqq1 gt 1.e-30 then Dqq(j,m,k)=dqq1 else Dqq(j,m,k)=1.e-30
         endfor
     endfor
 endfor
 close,2

 logDaa=alog10(Daa) & logDqq=alog10(Dqq) 
    
 dmax=max(logDaa)
 print,'max of Daa=',dmax
 print,'min of Daa=',min(logDaa)
 read,'Maximum of Daa => ',dmax
 ;dmin=dmax-6.
 dmin=dmax-7.
 ;if max(logDaa) gt Dmax then logDaa(where(logDaa gt Dmax))=Dmax*0.9999
 ;if max(logDEE) gt Dmax then logDEE(where(logDEE gt Dmax))=Dmax*0.9999
 ;if max(logDaE) gt Dmax then logDaE(where(logDaE gt Dmax))=Dmax*0.9999
 
; set level
 nlevel=60
 colrmax=254 & colrmin=1
 clvl=intarr(nlevel) & lvl=fltarr(nlevel)
 dlvl=(dmax-dmin)/(nlevel-1)
 dcolr=float(colrmax-colrmin)/(nlevel-1)
 for n=0,nlevel-1 do begin
     clvl(n)=colrmin+round(dcolr*n)
     lvl(n)=dmin+dlvl*n
 endfor
 
 loadct,33  ; red-blue
 tvlct, red, green, blue,/get
 tvlct,0,0,0,0 ; dark red color to white in red-blue table
 tvlct,255,255,255,255 ; dark red color to black in red-blue table
 !p.background=255
 !p.color=0

 
 yrg=[0.0001,10.]      ; in MeV
 psn=[0.90,0.15,0.95,0.9]
 psn1=[0.1,0.15,0.82,0.9]
 psnD=fltarr(3,3,4)
 psnD(0,0,0:3)=[0.07,0.76,0.29,0.92]
 psnD(0,1,0:3)=[0.35,0.76,0.57,0.92]
 psnD(0,2,0:3)=[0.63,0.76,0.85,0.92]
 psnD(1,0,0:3)=[0.07,0.52,0.29,0.68]
 psnD(1,1,0:3)=[0.35,0.52,0.57,0.68]
 psnD(1,2,0:3)=[0.63,0.52,0.85,0.68]
 psnD(2,0,0:3)=[0.07,0.28,0.29,0.44]
 psnD(2,1,0:3)=[0.35,0.28,0.57,0.44]
 psnD(2,2,0:3)=[0.63,0.28,0.85,0.44]

 set_plot,'ps'
 device,filename=fname+'.ps',/color,/inches,xsize=8.5,ysize=11.,xoffset=0.,yoffset=0.
 pe=-1
 new_plot2:
 pe=pe+1
 if pe ge ipc then goto, end_plot2
    x1=0.85+0.03
    x2=x1+0.03
    y0=0.76
    yf=0.92
    ym=0.5*(y0+yf)
    dy=(yf-y0)/(nlevel-1)
    xyouts,0.07,0.95,'fpe/fce='+string(fpefce(pe),'(f6.2)'),/normal
    contour,logDaa(pe,*,*),a0,Q*1.e-3, yrange=yrg,xrange=[0,90],$
        /ylog,xticks=3,/fill,levels=lvl,c_color=clvl,title='D!d!7aa!x!n',$
        xstyle=1,ystyle=1,charsize=1,position=psnD(0,0,*),ytitle='Q (MeV)',/noerase
    contour,logDqq(pe,*,*),a0,Q*1.e-3, yrange=yrg,xrange=[0,90],$
        /ylog,xticks=3,/fill,levels=lvl,c_color=clvl,title='D!dQ2Q2!n/Q!u2!n',$
        xstyle=1,ystyle=1,charsize=1,position=psnD(0,1,*),/noerase
    for i=0,nlevel-1 do begin
        y1=y0+i*dy
        y2=y1+1.02*dy
        polyfill,[x1,x2,x2,x1,x1],[y1,y1,y2,y2,y1],color=clvl(i),/normal
    endfor
    axis,x2,ym,yaxis=1,yrange=[10.^dmin,10.^dmax],yticks=6,/ylog,ystyle=1,$
        color=0,charsize=1,ytitle='(1/sec)',ytickformat='stickformat',/normal
 erase
 goto,new_plot2
 end_plot2:
 device,/close


 end
