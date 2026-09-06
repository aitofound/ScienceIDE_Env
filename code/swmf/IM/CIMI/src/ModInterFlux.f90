   module ModInterFlux

   integer :: iOrderLat=2,iOrderLon=2
   logical :: UseHigherOrder=.false.  ! use higher order inter-flux

   !the number of ghost cells required for a given scheme order
   !Fill at set time
   integer :: nGhostLonLeft, nGhostLonRight
   
 contains

   ! Set the number of longitude ghostcells based on the order of the
   ! scheme
   !
   ! March 2022 - CMK: Update as needed as other order schemes are
   ! implemented
   subroutine set_nghostcell_scheme

     DEFINE_GHOST_CELLS: select case( iOrderLon )

     case( 2 )
        nGhostLonLeft  = 1
        nGhostLonRight = 2
        
     case( 7 )
        nGhostLonLeft  = 3
        nGhostLonRight = 4
        
     case default 
        nGhostLonLeft  = 1
        nGhostLonRight = 2
        
     end select DEFINE_GHOST_CELLS

   end subroutine set_nghostcell_scheme
   
     
     
!*******************************************************************************
!                            FLS_2D_ho
!
!  Routine calculates the inter-flux, f(i+cl,j) and f(i,j+cp), 
!  using 7th order Lagrangian polynomial interpolation 
!  and ULTIMATE universal limiter as in Leonard [1991].
!  where cl=cl(i,j), cp=cp(i,j)
!
!  The idea behind this scheme is to assume distribution f at (x,t) is 
!   f(x,t)=f(x-v*dt,t-dt) 
!   and find f(x-v*dt,t-dt) using Lagrange interpolation
!   and find left and right side fluxes to apply flux limiter,
!   where v is flow velocity in x-direction.
!  Left and right side fluxes are not at the half grids or grid faces
!   but their location vary with v.
!
!  Note: Use an odd-order scheme!
!        odd-order schemes are more accurate then even order schemes.
!
!  fhol,fhop,flw: higher order inter flux
!  fupl,fupp,fup: 1th order upwind inter flux
!  a00,b00 : modified Pascal's progression
!
!  By S.-B. Kang, Code 673, at NASA/GSFC in 2017
!*******************************************************************************
  subroutine FLS_2D_ho(ir,ip,iba,fb0,fb1,cl,cp,f,fhol,fhop,fupl,fupp)
  use ModCimi,       ONLY: MinLonPar,MaxLonPar
  implicit none

  integer,intent(in) :: ir,ip,iba(ip)
  real,intent(in) ::  fb0(ip),fb1(ip),cl(ir,ip),cp(ir,ip),f(ir,ip)
  real,intent(out) :: fhol(0:ir,ip),fhop(ir,ip),fupl(0:ir,ip),fupp(ir,ip)
  real fwbc(-2:ir+4,ip),df,cl1,cp1,cl2,cp2,c1,&
       xsignl(ir,ip),xsignp(ir,ip),xsign,fup,flw2,flw3,flw4,flw5,flw6,flw7,&
       x,r,xlimiter,corr,d1,d2,fc,fu,fd,ref,del,adel,acurv,flim
  real a21,a22,a23,a24,&
       a41,a42,a43,a44,a45,a46,&
       a61,a62,a63,a64,a65,a66,a67,a68,&        ! a_(order)_(ordinal)
       b31,b32,b33,b34,&
       b51,b52,b53,b54,b55,b56,&
       b71,b72,b73,b74,b75,b76,b77,b78          ! b_(order)_(ordinal)
  integer i0,i1,ib,ib2,ibm
  integer i01,i02,i03,i10,i20,i30,i40,&! j_(positive)_(negative)
          j01,j02,j03,j10,j20,j30,j40  ! j_(positive)_(negative)
  integer i,j,k         ! dummy index

  
   i0=1   

!!erLat=3
!!erLon=3

! aOO,bOO
  a21=1./12.
  a22=-1./12.
  a23=a22
  a24=a21
  a41=1./240.
  a42=-3./240.
  a43=2./240.
  a44=a43
  a45=a42
  a46=a41
  a61=1./10080.
  a62=-5./10080.
  a63=9./10080.
  a64=-5./10080.
  a65=a64
  a66=a63
  a67=a62
  a68=a61
  
  b31=1./24.
  b32=-3./24.
  b33=3./24.
  b34=-1./24.
  b51=1./720.
  b52=-5./720.
  b53=10./720.
  b54=-10./720.
  b55=5./720.
  b56=-1./720.
  b71=1./10080.
  b72=-7./10080.
  b73=21./10080.
  b74=-35./10080.
  b75=35./10080.
  b76=-21./10080.
  b77=7./10080.
  b78=-1./10080.

  fwbc(1:ir,1:ip)=f(1:ir,1:ip)        ! flwbc is f with boundary condition

! up boundary condition
  do j=MinLonPar,MaxLonPar
     fwbc(-2:0,j)=fb0(j)
     ib=iba(j)
     fwbc(ib+1:ir+4,j)=fb1(j)
  enddo

! determine xsign
  do i=i0,ir
     do j=MinLonPar,MaxLonPar
        xsignl(i,j)=sign(1.,cl(i,j))
        xsignp(i,j)=sign(1.,cp(i,j))
     enddo
  enddo

!  1st order fup and 7th order fho
  do j=MinLonPar,MaxLonPar
     j01=j-1
     j02=j-2
     j03=j-3
     j10=j+1
     j20=j+2
     j30=j+3
     j40=j+4
     if (j01.lt.1) j01=j01+ip
     if (j02.lt.1) j02=j02+ip
     if (j03.lt.1) j03=j03+ip
     if (j10.gt.ip) j10=j10-ip
     if (j20.gt.ip) j20=j20-ip
     if (j30.gt.ip) j30=j30-ip
     if (j40.gt.ip) j40=j40-ip
     ib=max(iba(j),iba(j10))
     do i=i0,ib
        cl1=cl(i,j)
        cp1=cp(i,j)   
        cl2=cl1**2
        cp2=cp1**2
        i01=i-1
        i02=i-2
        i03=i-3
        i10=i+1
        i20=i+2
        i30=i+3
        i40=i+4
   ! find f*l
   ! 1st order
        xsign=xsignl(i,j)
        if (xsign.eq.1.) then
           fup=fwbc(i,j)
        else
           fup=fwbc(i10,j)
        endif
        fupl(i,j)=fup
   ! 2nd order (Lax-Wendroff)
        flw2=0.5*((fwbc(i10,j)+fwbc(i,j))+cl1*(fwbc(i,j)-fwbc(i10,j)))   ! 2nd order inter-flux
        if (iOrderLat.eq.2) then
        ! Superbee limter
           x=fwbc(i10,j)-fwbc(i,j)
           if (abs(x).le.1.e-27) fhol(i,j)=fup
           if (abs(x).gt.1.e-27) then
              if (xsign.eq.1.) r=(fwbc(i,j)-fwbc(i01,j))/x
              if (xsign.eq.-1.) r=(fwbc(i20,j)-fwbc(i10,j))/x
              if (r.le.0.) fhol(i,j)=fup
              if (r.gt.0.) then
                 xlimiter=max(min(2.*r,1.),min(r,2.))
                 corr=flw2-fup
                 fhol(i,j)=fup+xlimiter*corr
                 if (fhol(i,j).lt.0.) fhol(i,j)=fup     ! fsbp can't be < 0
              endif
           endif
        endif
   ! 3rd order (QUICKEST)
        if (iOrderLat.ge.3) then
           d1=a21*fwbc(i20,j)+a22*fwbc(i10,j)+a23*fwbc(i,j)+a24*fwbc(i01,j) 
           d2=b31*fwbc(i20,j)+b32*fwbc(i10,j)+b33*fwbc(i,j)+b34*fwbc(i01,j)
           c1=cl2-1.
           if (iOrderLat.eq.3) then
              d2=d2*2.
              flw3=flw2+c1*(d1-xsign*d2)
              fhol(i,j)=flw3
           endif
        endif
   ! 4th order (ULTIMATE)
        if (iOrderLat.ge.4) then
           flw4=flw2+c1*(d1-cl1*d2)   ! 4th order inter-flux
        endif
   ! 5th order (ULTIMATE)
        if (iOrderLat.ge.5) then
           d1=a41*fwbc(i30,j)+a42*fwbc(i20,j)+a43*fwbc(i10,j)+a44*fwbc(i,j)&
             +a45*fwbc(i01,j)+a46*fwbc(i02,j) 
           d2=b51*fwbc(i30,j)+b52*fwbc(i20,j)+b53*fwbc(i10,j)+b54*fwbc(i,j)&
             +b55*fwbc(i01,j)+b56*fwbc(i02,j)
           c1=c1*(cl2-4.) 
           if (iOrderLat.eq.5) then
              d2=d2*3.
              flw5=flw4+c1*(d1-xsign*d2)   ! 6th order interflux
              fhol(i,j)=flw5
           endif
        endif
   ! 6th order (ULTIMATE)
        if (iOrderLat.ge.6) then
           flw6=flw4+c1*(d1-cl1*d2)   ! 6th order interflux
        endif
   ! 7th order (ULTIMATE)
        if (iOrderLat.eq.7) then
           d1=a61*fwbc(i40,j)+a62*fwbc(i30,j)+a63*fwbc(i20,j)+a64*fwbc(i10,j)&
             +a65*fwbc(i,j)+a66*fwbc(i01,j)+a67*fwbc(i02,j)+a68*fwbc(i03,j)
           d2=b71*fwbc(i40,j)+b72*fwbc(i30,j)+b73*fwbc(i20,j)+b74*fwbc(i10,j)&
             +b75*fwbc(i,j)+b76*fwbc(i01,j)+b77*fwbc(i02,j)+b78*fwbc(i03,j)
           c1=c1*(cl2-9.) 
           flw7=flw6+c1*(d1-xsign*d2)   ! 7th order interflux
           fhol(i,j)=flw7
        endif
   ! find f*p
   ! 1st order
        xsign=xsignp(i,j)
        if (xsign.eq.1.) then
           fup=fwbc(i,j)
        else
           fup=fwbc(i,j10)
        endif
        fupp(i,j)=fup
   ! 2nd order (Lax-Wendroff)
        flw2=0.5*((fwbc(i,j10)+fwbc(i,j))+cp1*(fwbc(i,j)-fwbc(i,j10)))   ! 2nd order flux
        if (iOrderLon.eq.2) then
        ! Superbee limter
           x=fwbc(i,j10)-fwbc(i,j)
           if (abs(x).le.1.e-27) fhop(i,j)=fup
           if (abs(x).gt.1.e-27) then
              if (xsign.eq.1.) r=(fwbc(i,j)-fwbc(i,j01))/x
              if (xsign.eq.-1.) r=(fwbc(i,j20)-fwbc(i,j10))/x
              if (r.le.0.) fhop(i,j)=fup
              if (r.gt.0.) then
                 xlimiter=max(min(2.*r,1.),min(r,2.))
                 corr=flw2-fup
                 fhop(i,j)=fup+xlimiter*corr
                 if (fhop(i,j).lt.0.) fhop(i,j)=fup     ! fsbp can't be < 0
              endif
           endif
        endif
   ! 3rd order (QUICKEST)
        if (iOrderLon.ge.3) then
           d1=a21*fwbc(i,j20)+a22*fwbc(i,j10)+a23*fwbc(i,j)+a24*fwbc(i,j01) 
           d2=b31*fwbc(i,j20)+b32*fwbc(i,j10)+b33*fwbc(i,j)+b34*fwbc(i,j01)
           c1=cp2-1. 
           if (iOrderLon.eq.3) then
              d2=2.*d2
              flw3=flw2+c1*(d1-xsign*d2)   ! 3th order inter-flux
              fhop(i,j)=flw3
           endif
        endif
   ! 4th order (ULTIMATE)
        if (iOrderLon.ge.4) then
           flw4=flw2+c1*(d1-cp1*d2)   ! 4th order flux
        endif
   ! 5th order (ULTIMATE)
        if (iOrderLon.ge.5) then
           d1=a41*fwbc(i,j30)+a42*fwbc(i,j20)+a43*fwbc(i,j10)+a44*fwbc(i,j)&
             +a45*fwbc(i,j01)+a46*fwbc(i,j02) 
           d2=b51*fwbc(i,j30)+b52*fwbc(i,j20)+b53*fwbc(i,j10)+b54*fwbc(i,j)&
             +b55*fwbc(i,j01)+b56*fwbc(i,j02) 
           c1=c1*(cp2-4.) 
           if (iOrderLon.eq.5) then
              d2=3.*d2
              flw5=flw4+c1*(d1-xsign*d2)   ! 6th order interflux
              fhop(i,j)=flw5
           endif
        endif
   ! 6th order (ULTIMATE)
        if (iOrderLon.ge.6) then
           flw6=flw4+c1*(d1-cp1*d2)   ! 6th order interflux
        endif
   ! 7th order (ULTIMATE)
        if (iOrderLon.eq.7) then
           d1=a61*fwbc(i,j40)+a62*fwbc(i,j30)+a63*fwbc(i,j20)+a64*fwbc(i,j10)&
             +a65*fwbc(i,j)+a66*fwbc(i,j01)+a67*fwbc(i,j02)+a68*fwbc(i,j03)
           d2=b71*fwbc(i,j40)+b72*fwbc(i,j30)+b73*fwbc(i,j20)+b74*fwbc(i,j10)&
             +b75*fwbc(i,j)+b76*fwbc(i,j01)+b77*fwbc(i,j02)+b78*fwbc(i,j03)
           c1=c1*(cp2-9.) 
           flw7=flw6+c1*(d1-xsign*d2)   ! 7th order interflux
           fhop(i,j)=flw7
        endif
     enddo              ! end of do i=i0,ir  
  enddo                 ! end of do j=1,ip

! ULTIMATE universial limiter
  do j=MinLonPar,MaxLonPar
     j01=j-1
     j10=j+1
     j20=j+2
     if (j01.lt.1) j01=j01+ip
     if (j10.gt.ip) j10=j10-ip
     if (j20.gt.ip) j20=j20-ip
     ib=max(iba(j),iba(j10))
     do i=i0,ib
     ! f*l
        cl1=abs(cl(i,j))
        cp1=abs(cp(i,j))
        if (iOrderLat.ge.3) then
           i01=i-1
           i10=i+1
           i20=i+2
           if (cl(i,j).gt.0.) then
              fu=fwbc(i01,j)
              fc=fwbc(i,j)
              fd=fwbc(i10,j)               
           else
              fu=fwbc(i20,j)               
              fc=fwbc(i10,j)
              fd=fwbc(i,j)               
           endif
           del=fd-fu
           adel=abs(del)
           acurv=abs(fd+fu-fc-fc)
           if (acurv.ge.adel) then
              fhol(i,j)=fc
           else
              if (cl1.ne.0.) then
                 ref=fu+(fc-fu)/cl1
              else
                 ref=fu
              endif
              if (del.gt.0.) then
                 flim=max(fhol(i,j),fc)
                 fhol(i,j)=min(flim,min(ref,fd))            
              else
                 flim=max(fhol(i,j),max(ref,fd))            
                 fhol(i,j)=min(flim,fc)
              endif
           endif
        endif    ! end of iOrderLat
     ! f*p
        if (iOrderLon.ge.3) then
           if (cp(i,j).gt.0.) then
              fu=fwbc(i,j01)               
              fc=fwbc(i,j)
              fd=fwbc(i,j10)               
           else
              fu=fwbc(i,j20)               
              fc=fwbc(i,j10)
              fd=fwbc(i,j)               
           endif
           del=fd-fu
           adel=abs(del)
           acurv=abs(fd+fu-fc-fc)
           if (acurv.ge.adel) then
              fhop(i,j)=fc
           else
              if (cl1.ne.0.) then
                 ref=fu+(fc-fu)/cp1
              else
                 ref=fu
              endif
              if (del.gt.0.) then
                 flim=max(fhop(i,j),fc)
                 fhop(i,j)=min(flim,min(ref,fd))            
              else
                 flim=max(fhop(i,j),max(ref,fd))            
                 fhop(i,j)=min(flim,fc)
              endif
           endif
        endif    ! end of iOrderLong
     enddo              ! end of do i=i0,ir  
  enddo                 ! end of do j=1,ip


  end subroutine FLS_2D_ho

   end module ModInterFlux
