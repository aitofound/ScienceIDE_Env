;reads unformatted data
;=======================
GET_LUN,fiUNIT

fi=[findfile('e1_????.dat'),findfile('e2_????.dat'),findfile('e3_????.dat')]

ifi=1

HeadInfo={iStep:0L, nDim:0L, iGCN:0L, nCell_D:INTARR(3)*0L, tSimulation:0.d0, $
             dt:0.d0, Dx_D:DBLARR(3), minValue:0.d0, maxValue:0.d0}
iHead=0L
iBack=0L

nsize=size(fi,/N_ELEMENTS)

nsize3=nsize/3
rmin=fltarr(nsize)
rmax=fltarr(nsize)

for ifi=0,(nsize-1)*1+0 do begin 
NameFile=fi(ifi)

openr,fiUNIT,NameFile
readu,fiUNIT,iHead,HeadInfo,iBack

if(ifi eq 0L) then print,HeadInfo,iHead,iBack

nX=HeadInfo.nCell_D(0)+HeadInfo.iGCN*2
nY=HeadInfo.nCell_D(1)+HeadInfo.iGCN*2
nZ=HeadInfo.nCell_D(2)+HeadInfo.iGCN*2

;E=dblarr(nX,nY,nZ)
E=dblarr(nX,nY)
;help,E

readu,fiUNIT,iHead,e,iBack
;if(ifi eq 0L) then print,'fi=',fi(ifi),' Max,Min=',max(e),min(e),iHead-iBack


EE=dblarr(nX,nY)
;help,EE

readu,fiUNIT,iHead,EE,iBack
;if(ifi eq 0L) then print,'fi=',fi(ifi),' Max,Min=',max(ee),min(ee),iHead-iBack

print,ifi,' ',fi(ifi),' Max,Min=',HeadInfo.minValue,HeadInfo.maxValue

rmin(ifi)=HeadInfo.minValue
rmax(ifi)=HeadInfo.maxValue

;wait,5

close,fiUNIT

;tvscl,e,(nX+4)*ifi, (nX+4) *(1+(2-ifi/nsize3))
;tvscl,ee,(nX+4)*ifi, (nX+4)*(0+(2-ifi/nsize3))
tvscl,-e, (nX+4)*(ifi-ifi/nsize3*nsize3),(nX+4)*(1+(4-ifi/nsize3*2))
tvscl,-ee,(nX+4)*(ifi-ifi/nsize3*nsize3),(nX+4)*(0+(4-ifi/nsize3*2))

;print,ifi,'==',(1+(4-ifi/nsize3*2)),(0+(4-ifi/nsize3*2)),ifi-ifi/nsize3*nsize3
end

FREE_LUN,fiUNIT

;print,headinfo

end
