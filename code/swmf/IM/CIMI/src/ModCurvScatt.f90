!*******************************************************************************
!                           ModCurvScatt.f90
! Module field line curvature (FLC) scattering.
!*******************************************************************************
module ModCurvScatt
  !!use ModCimiGrid  ,ONLY: ir=>np,ip=>nt, nspec
  use ModCimiGrid  ,ONLY: ir=>np,ip=>nt,iw=>nm,ik=>nk, nspec

  private
  
  logical,public :: DoFlcLoss
  
  real,dimension(ir,ip) :: pc_flc,zeta1,zeta2
  real, public, allocatable :: tau_flc(:,:,:), &
                    Daa_flc(:,:,:,:,:)

  ! index of K, 2nd adiabatic invariant at the loss cone pitch-angle.
  integer, allocatable :: m_lc(:,:,:) 

  ! number of points used in curvature calc. The "half" is really how many on
  ! each side of ibmin with the ibmin point in the middle
  integer,parameter :: nPoint=5

  public :: calc_FLC_para
  public :: FLC_loss

contains
  !***********************************************************************
  !                        calc_FLC_para      
  !  Routine calculates parameters required to calculate pitch-angle
  !   diffusion coefficient (Daa) due to field line curvature 
  !   (curvature scattering).
  !***********************************************************************
  ! for coupled model, this subroutine should take lat and lon index
  ! and the min curvature (Rc), associated d2Rc/ds2, d2B/ds2, and B
  ! in region around min B location (9 points)
  ! calculates zeta1 and zeta2 which are needed for scattering calcuation
  subroutine calc_FLC_para(i,j)
    use ModGmCimi, only: StateBmin_IIV, UseGm    
    use ModConst, only: EM_speed=>cLightSpeed
    use ModCimiPlanet, ONLY: re_m
    use ModCimiTrace, only: bo,ro,CurvaturePointsXyz_IIID,BCurvaturePoints_III
    !!use CurvScatt, only: pc_flc,zeta1,zeta2
    implicit none
    
    integer,intent(in) :: i,j
    real,dimension(5) :: xa,ya,za,ds,Rc
    real Rc0,Rc1,Rc2,ds0,ds1,ds2,drC1,drC2,B0,dB1,dB2
    real :: eVtoKeV=1.e-3
    integer ::  is,is1,is2, iPoint

    ! allocate needed variables
    if(.not. allocated(tau_flc)) allocate(tau_flc(nspec,ir,ip),&
                    Daa_flc(nspec,ir,ip,iw,ik),  m_lc(ir,ip,ik))

    !When r less than 4Re (or whatever value) just use the dipole
    !curvature scattering values
    !for zeta. Otherwise calculate 
    if (ro(i,j).gt.4.) then   ! assume a dipole < 4 RE
       if (UseGm) then
          ! extract points from buffer around min b from couple buffer
          xa(1) = StateBmin_IIV(i,j,6)
          ya(1) = StateBmin_IIV(i,j,7)
          za(1) = StateBmin_IIV(i,j,8)
          
          xa(2) = StateBmin_IIV(i,j,9)
          ya(2) = StateBmin_IIV(i,j,10)
          za(2) = StateBmin_IIV(i,j,11)
          
          xa(3) = StateBmin_IIV(i,j,12)
          ya(3) = StateBmin_IIV(i,j,13)
          za(3) = StateBmin_IIV(i,j,14)
          
          xa(4) = StateBmin_IIV(i,j,15)
          ya(4) = StateBmin_IIV(i,j,16)
          za(4) = StateBmin_IIV(i,j,17)
          
          xa(5) = StateBmin_IIV(i,j,18)
          ya(5) = StateBmin_IIV(i,j,19)
          za(5) = StateBmin_IIV(i,j,20)
          
          
       else
          xa(1) = CurvaturePointsXyz_IIID(i,j,1,1)
          ya(1) = CurvaturePointsXyz_IIID(i,j,1,2)
          za(1) = CurvaturePointsXyz_IIID(i,j,1,3)
          
          xa(2) = CurvaturePointsXyz_IIID(i,j,2,1)
          ya(2) = CurvaturePointsXyz_IIID(i,j,2,2)
          za(2) = CurvaturePointsXyz_IIID(i,j,2,3)
          
          xa(3) = CurvaturePointsXyz_IIID(i,j,3,1)
          ya(3) = CurvaturePointsXyz_IIID(i,j,3,2)
          za(3) = CurvaturePointsXyz_IIID(i,j,3,3)
          
          xa(4) = CurvaturePointsXyz_IIID(i,j,4,1)
          ya(4) = CurvaturePointsXyz_IIID(i,j,4,2)
          za(4) = CurvaturePointsXyz_IIID(i,j,4,3)
          
          xa(5) = CurvaturePointsXyz_IIID(i,j,5,1)
          ya(5) = CurvaturePointsXyz_IIID(i,j,5,2)
          za(5) = CurvaturePointsXyz_IIID(i,j,5,3)
          
          !ds(1:nPointHalf*2+1)=dss(i01:i10)
       endif
       
       !calculate ds
       do iPoint=1,nPoint-1
          ds(iPoint) = sqrt((xa(iPoint+1)-xa(iPoint))**2&
               + (ya(iPoint+1)-ya(iPoint))**2&
               + (za(iPoint+1)-za(iPoint))**2)
       enddo
       ds(nPoint)=ds(nPoint-1)
       
       call calc_FLC(nPoint,xa,ya,za,ds,Rc)
       
       !minimum of field line curvature is always 3rd point (middle) passed 
       is=3
       Rc0=Rc(is)
       
       ! (2) calculate pc_flc,zeta1,zeta2 at bo
       ! pc_flc is the momentum which has a gyroradius that matches
       ! the minimum field line curvature
       ! Zeta1 and Zeta2 from Young et al 2002; 2008 are parameters
       ! which govern the calculation of the scattering
       ! note that the two curvature values for points around the
       ! min curvature are also saved for
       ! future calculation of second derivative of curvature
       
       ! pc corresponding to FLC in keV
       pc_flc(i,j)=BCurvaturePoints_III(i,j,is)*EM_speed*Rc0*re_m*eVtoKeV
       is1=is-1
       is2=is+1
       Rc1=Rc(is1)
       Rc2=Rc(is2)
       ds1=(is1)
       ds2=(is2)
       ds0=0.5*(ds1+ds2)
       
       
       drC1=Rc0-Rc1
       drC2=Rc2-Rc0
       !Curvature is usually minimum at Bmin. However, if this is not the case
       !we set the minimum at one of the next points and force the second
       !derivative to be positive.
       if (drC1>0. .and. drC2<0.) then
          if(abs(drC1)>abs(drc2)) then
             !put min curvature at is1
             drC2=drC1
             drC1=-abs(drC2)
          else
             !put min curvature at is2
             drC1=drC2
             drC2=abs(drC1)
          end if
       elseif (drC1>0.) then
          drC2=drC1
          drC1=-abs(drC2)
       elseif (drC2<0.) then
          drC1=drC2
          drC2=abs(drC1)
       endif
       zeta1(i,j)=Rc0*(drC2/ds2-drC1/ds1)/ds0       ! Zeta1 as in Young et al. 2002; 2008
       if (zeta1(i,j) <0) then
          write(*,*) 'Rc0,drC2/ds2,drC1,ds1,ds0',Rc0,drC2/ds2,drC1/ds1,ds0
       endif
       B0=BCurvaturePoints_III(i,j,is)
       dB1=B0-BCurvaturePoints_III(i,j,is1)
       dB2=BCurvaturePoints_III(i,j,is2)-B0           
       zeta2(i,j)=Rc0*Rc0/B0*(dB2/ds2-dB1/ds1)/ds0  ! Zeta2 as in Young et al. 2002; 2008
    else
       zeta1(i,j)=0.6666667
       zeta2(i,j)=1.

       Rc0=0.5*ro(i,j)
       pc_flc(i,j)=bo(i,j)*EM_speed*Rc0*re_m*eVtoKeV
       
    endif

    !set the Daa coeficients for FLC
    call calc_Daa_flc(i,j)
  end subroutine calc_FLC_para


  !***********************************************************************
  !                           calc_FLC      
  !  Routine calculates field line curvature 
  !***********************************************************************
  subroutine calc_FLC(n,xa,ya,za,ds,Rc)

    implicit none

    integer,intent(in) :: n
    real,dimension(n),intent(in) :: xa,ya,za,ds
    real,intent(out) :: Rc(n)
    integer i,i1, i_1
    real :: ds0,ds1,ds2,dx1,dx2,dy1,dy2,dz1,dz2,ddx,ddy,ddz
    real :: dxds2,dyds2,dzds2,dx,dy,dz, dxds1, dyds1, dzds1

    ds1=ds(1)
    dx1=xa(2)-xa(1)
    dy1=ya(2)-ya(1)
    dz1=za(2)-za(1)
    
    dxds1=dx1/ds1
    dyds1=dy1/ds1
    dzds1=dz1/ds1
    do i=2,n-1
       ds0=ds(i)
       i1=i+1
       i_1=i-1
       dx2=xa(i1)-xa(i)
       dy2=ya(i1)-ya(i)
       dz2=za(i1)-za(i)
       !ds2=0.5*(ds(i1)+ds(i))  ! ds from i to i1
       ds2=ds(i1)  
       !ds2=sqrt((xa(i1)-xa(i))**2 &
       !        +(ya(i1)-ya(i))**2 &
       !        +(za(i1)-za(i))**2 )
       dxds2=dx2/ds2
       dyds2=dy2/ds2
       dzds2=dz2/ds2
       dx=0.5*(xa(i1)-xa(i_1))
       dy=0.5*(ya(i1)-ya(i_1))
       dz=0.5*(za(i1)-za(i_1))
       ddx=(dxds2-dxds1)
       ddy=(dyds2-dyds1)
       ddz=(dzds2-dzds1)
       Rc(i)=(dx**2+dy**2+dz**2)**1.5&
            /sqrt((ddx*dy-dx*ddy)**2 &
                 +(ddy*dz-dy*ddz)**2 &
                 +(ddz*dx-dz*ddx)**2)/ds0
       dxds1=dxds2
       dyds1=dyds2
       dzds1=dzds2
    enddo
    Rc(1)=Rc(2)
    Rc(n)=Rc(n-1)

  end subroutine calc_FLC



!***********************************************************************
!                        calc_Daa_flc      
!  Routine calculates parameters required to calculate pitch-angle
!   diffusion coefficient (Daa) due to field line curvature 
!   (curvature scattering).
!***********************************************************************
  subroutine calc_Daa_flc(i,j)

  use ModCimiPlanet,ONLY: nspec,amu_I
  use ModCimiGrid  ,ONLY: ir=>np,ip=>nt,iw=>nm,ik=>nk
  use ModCimiTrace, ONLY: ekev, iba, sinA, Tbounce, alscone 
  ! use , Only: bo ?
  implicit none
  
  integer,intent(in) :: i,j

  real E02(nspec)    ! rest mass * 2 in keV
  real sinaL,aL,Bi,a0sq(iK),aLsq

  ! variables as in Young et al. 2002; 2008
  real e, e2, e3,&  ! epsilon, epsilon^2, epsilon^3
       c,a1,a2,De,b0,w0,tb,D0,N2,Amx0,&
       a0(ik),&     ! pitch-angle in rad
       cosa(ik),&   ! cos(a0)
       sinacosa(ik),&  ! sina * cosa
       cosa_bar,&   ! cos(a0) at maximum of Amax
       Dmax         ! diffusion coefficient upper limit
  real,dimension(iw,ik) :: Amax,Aa0
  integer n,k,m,m_bar
  real :: ekev0,pc

  do n = 1,nspec
     E02(n)= amu_I(n)*938272.0813*2      ! amu* p+ rest mass*2
  enddo

  ! calculate loss cone
  !Bi=?  ! B field at the loss cone
  !sinaL=sqrt(bo/Bi)
  !find largest PA inside loss cone
  !set default as ik since ik is the smallest PA.
  m_lc(i,j,:)=ik
  LOSS_CONE:do m=1,ik
     if (alscone(1,i,j,1,m) < 1.) then
        sinaL=sinA(i,j,m)
        m_lc(i,j,m)=m                ! index at loss cone
        exit LOSS_CONE
     endif
  end do LOSS_CONE
  
  aL=asin(sinaL)
  aLsq=aL**2

  ! calculate a0 and cosa
  do m=1,ik
     a0(m)=asin(sinA(i,j,m))
     cosa(m)=cos(a0(m))
     a0sq(m)=a0(m)**2
     sinacosa(m)=sinA(i,j,m)*cosa(m)
  enddo


  do n=1,nspec

  ! calculate coefficients for Daa 
     do k=1,iw
        do m=1,ik
           ekev0=ekev(n,i,j,k,m)
           pc=sqrt(ekev0*(ekev0+E02(n)))  ! momentum * c in keV
           ! coefficients as in Young et al. 2002; 2008
           e=pc/pc_flc(i,j)       ! epsilon
           !if (e.gt.0.584) e=0.584  ! slow diffusion limit
           if (e.gt.10.) e=10.       ! no slow diff limit
           if (e.gt.0.05) then
              e2=e*e
              e3=e2*e
              w0=1.051354+0.13513581*e-0.50787555*e2        ! omega
              if (w0>1.999999) w0=1.999999
              c=1.0663037-1.0944973/e+0.016679378/e2-0.00049938987/e3
              a1=-0.35533865+0.12800347/e+0.0017113113/e2    
              a2= 0.23156321+0.15561211/e-0.001860433/e2    
              b0=-0.51057275+0.93651781/e-0.0031690066/e2    
              De=-0.49667826-0.00819418/e+0.0013621659/e2  ! D(epsilon)
              Amax(k,m)=exp(c)*(zeta1(i,j)**a1*zeta2(i,j)**a2+De)
              Aa0(k,m)=sin(w0*a0(m))*cosa(m)**b0
           else 
              Amax(k,m)=0.
              Aa0(k,m)=0.
           endif
        enddo
     enddo
   
     do k=1,iw
        m_bar=maxloc(abs(Aa0(k,:)),dim=1)
        cosa_bar=cosa(m_bar)
        do m=1,ik
           Amx0=Amax(k,m)
           if (Amx0.ne.0.) then
              tb=Tbounce(n,i,j,k,m)
              N2=1./Aa0(k,m_bar)/Aa0(k,m_bar)
              if (tb>1e-5) then
                 D0=0.5*Amx0*Amx0/tb
              else
                 D0=0.
              endif
              !Dmax=4.9348/tb    ! da0=0.5pi during 0.5*tb
              if (m<m_lc(i,j,m)) then
                 Dmax=2.*a0sq(m)/tb ! da0=a0 during 0.5*tb
              else
                 Dmax=2.*aLsq/tb    ! da0=aL during 0.5*tb
              endif
              Daa_flc(n,i,j,k,m)=&
                D0*N2*(Aa0(k,m)/sinacosa(m))**2
              if (Daa_flc(n,i,j,k,m).gt.Dmax) Daa_flc(n,i,j,k,m)=Dmax
           else
              Daa_flc(n,i,j,k,m)=0.
           endif
        enddo
     enddo
  
  enddo  ! loop of nspec

  end subroutine calc_Daa_flc



  !***********************************************************************
  !                        FLC_loss     
  !  This subroutine calculates characteristic lifetime due to 
  !   field line curvature scattering and applies loss to f2
  !   based on Albert and Shprits, 2009
  !
  !  The followings contribute to lifetime calculation
  !  (1) loss timescale from 90 deg from loss cone pitch-angle,
  !       assuming Daa=Daa at loss cone
  !  (2) loss timesacle assuming Daa=0 except for Daa_max
  !  (3) half bounce period 
  !***********************************************************************
  subroutine FLC_loss(Dt)
    use ModCimiPlanet,ONLY: nspec
    use ModCimiGrid  ,ONLY: ir=>np,ip=>nt,iw=>nm,ik=>nk, MinLonPar, MaxLonPar
    use ModCimi,      ONLY: f2
    ! ro and ekev are for test only, to be removed after tests
    use ModCimiTrace, ONLY: iba, sinA, ro, ekev,Tbounce
                   
    real, intent(in):: Dt
    real :: tau_flc1,tau_flc2
    integer ::  n,i,j,k,m,m_L,m_loc
        
    do j=MinLonPar, MaxLonPar
       do i=1,iba(j)
          do n=1,nspec
             do k=1,iw
                do m=1,ik
                   m_L=m_lc(i,j,m)
                   if (Daa_flc(n,i,j,k,m_L)/=0.) then
                      m_loc=m_L
                      if (m<m_L) m_loc=minloc(Daa_flc(n,i,j,k,m:m_L),1)
                      if (Daa_flc(n,i,j,k,m_loc)/=0.) then
                         tau_flc2=0.5*abs(1.-sinA(i,j,m_loc-1)/sinA(i,j,m_loc)) &
                                  /Daa_flc(n,i,j,k,m_loc) ! loss timescale at Daa max
                         !write(*,*) tau_flc2, Daa_flc(n,i,j,k,m_loc)
                         !write(*,*) sinA(i,j,m_L),Daa_flc(n,i,j,k,m_L),tau_flc2,Tbounce(n,i,j,k,m) 
                         tau_flc1=0.5*abs(1.-log(sinA(i,j,m_L)))/Daa_flc(n,i,j,k,m_L) & 
                              ! timescale from 90deg to loss cone + half bounce time
                              +tau_flc2 + 0.5*Tbounce(n,i,j,k,m)
                         f2(n,i,j,k,m)=f2(n,i,j,k,m)*exp(-Dt/tau_flc1)
                      endif  
!\
! \
! print loss timescale
 if (i==1 .and. j==24 .and. n==1 .and. k==nm/2 .and. m==1) then
    write(*,*) 'write(*,*) n,j,k,m ',n,j,k,m
    write(*,*) 'H+: ro, ekev, tau_flc, Tbounce*0.5, Daa'
 endif
 if (i==1 .and. j==24 .and. n==nspec .and. k==nm/2 .and. m==1) then
    write(*,*) 'write(*,*) n,j,k,m ',n,j,k,m
    write(*,*) 'e-: ro, ekev, tau_flc1, tau_flc2, Tbounce*0.5, Daa'
 endif
 if (j==24 .and. n==1 .and. k==nm/2 .and. m==1) &
    write(*,'(f7.2,1p5E11.3)') &
     ro(i,j),ekev(n,i,j,k,m),tau_flc1,tau_flc2,Tbounce(n,i,j,k,m)*0.5,Daa_flc(n,i,j,k,m_L)
 if (j==24 .and. n==nspec .and. k==nm/2 .and. m==1) &
    write(*,'(f7.2,1p5E11.3)') &
     ro(i,j),ekev(n,i,j,k,m),tau_flc1,tau_flc2,Tbounce(n,i,j,k,m)*0.5,Daa_flc(n,i,j,k,m_L)
! /
!/
                   endif
                enddo
             enddo!  k
          enddo   !  n
       enddo      !  i
    enddo         !  j

  end subroutine FLC_loss

end module ModCurvScatt
!    use ModCimiPlanet,ONLY: nspec,amu_I
!    use ModCimiGrid  ,ONLY: ir=>np,ip=>nt,iw=>nm,ik=>nk, MinLonPar, MaxLonPar
!    use ModCimiPlanet,ONLY: rc ! loss cone altitude
!    use ModCimi,      ONLY: f2
!    use ModCimiTrace, ONLY: ekev, iba, y=>sinA, rmir, Tbounce 
!   
!    implicit none
!
!    real, intent(in):: Dt
!    real :: ekev0,Tbounce0(iw,ik),sina,cosa,a0
!    real :: E02(4),pc,&
!         e,e2,e3,c,a1,a2,De,w,b,Amax,tb,D0,Ni0,Dmax,&
!         Daa_flc1,tau_flc1
!    integer ::  n,i,j,k,m,m_lc,k1
!
!    !this should be set in ModPlanet.f90 and used here
!    do n = 1,nspec
!       E02(n)= amu_I(n)*938272.0813*2      ! amu* p+ rest mass*2
!    enddo
!
!
!    do j=MinLonPar, MaxLonPar
!       do i=1,iba(j)
!
!          ! find the index m adjacent to loss cone 
!          do m=ik,1,-1
!             if (rmir(i,j,m).ge.rc) exit
!          enddo
!          m_lc=m+1
!          sina=y(i,j,m_lc)
!          a0=asin(sina)
!          cosa=cos(a0)
!
!          tau_flc(:,i,j)=8640000.  !! for print only
!
!          !for each species calculate the coefficients as in Young et al
!          ! to determine the scattering liftime and apply the loss
!          do n=1,nspec
!             if (n == nspec) then
!                !electrons
!                do k=1,iw
!                   if (ekev(n,i,j,k,m_lc).ge.1000.) exit
!                enddo
!             else
!                !ions
!                do k=1,iw
!                   if (ekev(n,i,j,k,m_lc).ge.10.) exit
!                enddo
!             endif
!             k1=k
!             do k=1,iw
!                ekev0=ekev(n,i,j,k,m_lc)
!                pc=sqrt(ekev0*(ekev0+E02(n)))
!                tb=Tbounce(n,i,j,k,m)
!                ! coefficients as in Young et al. 2002; 2008
!                ! e=RatioGyroToCurvature or RatioGyroCurv
!                e=pc/pc_flc(i,j)                         ! epsilon
!                ! Ratio of Gyroradius to curvature is assumed to be limited
!                ! as in Young et al as they assume slow scattering limit
!                ! for applicability of diffusion approach. We assume
!                ! same limit as in that paper.
!                if (e.gt.0.584) e=0.584
!                if (e.gt.0.05) then
!                   e2=e*e
!                   e3=e2*e
!                   !these variable names are identical to Young et al paper
!                   ! but Amax would be A0
!                   w=1.051354+0.13513581*e-0.50787555*e2        ! omega
!                   if (w>1.999999) w=1.999999
!                   c=1.0663037-1.0944973/e+0.016679378/e2-0.00049938987/e3
!                   a1=-0.35533865+0.12800347/e+0.0017113113/e2    
!                   a2= 0.23156321+0.15561211/e-0.001860433/e2    
!                   b=-0.51057275+0.93651781/e-0.0031690066/e2    
!                   De=-0.49667826-0.00819418/e+0.0013621659/e2  ! D(epsilon)
!                   !write(*,*) 'i,j,k,c,zeta1(i,j),a1,zeta2(i,j),a2,De',&
!                   !     i,j,k,c,zeta1(i,j),a1,zeta2(i,j),a2,De
!                   Amax=exp(c)*(zeta1(i,j)**a1*zeta2(i,j)**a2+De)
!
!                   ! now use coef. to calculate lifetime and apply loss
!                   Ni0=sin(w*a0)*cosa**b
!                   D0=0.5*Amax*Amax/tb
!                   Dmax=4.9348/tb    ! da0=0.5pi during 0.5*tb
!                   Daa_flc1=D0/Ni0/Ni0*(sin(w*a0)/sina)**2*cosa**(2.*b-2.)
!                   if (Daa_flc1.gt.Dmax) Daa_flc1=Dmax
!                   tau_flc1=0.5*abs(log(sina))/Daa_flc1    ! life time
!                   f2(n,i,j,k,:)=f2(n,i,j,k,:)*exp(-Dt/tau_flc1)
!                   if (i==iba(j)-1 .and. j==24 .and. n==1) &
!                        write(*,*) 'write(*,*) n,i,j,k, tau_flc1',n,i,j,k, tau_flc1
!                else 
!                   w=0.
!                   b=0.
!                   Amax=0. 
!                   tau_flc1=864000.
!                endif
!
!                if (e>0.05) then
!
!                else
!
!                endif
!                if (k.eq.k1) tau_flc(n,i,j)=tau_flc1 
!             enddo    ! end of k
!
!          enddo       ! end of n
!       enddo       ! end of i
!    enddo       ! end of j
!
!  end subroutine FLC_loss

  !***********************************************************************
  !                           write_FLC     
  ! 
  !  Routine prints ratio of 10 keV proton gyroradius to
  !   field line curvature radius
  !***********************************************************************
  !subroutine write_FLC(t,tstep)
  !  use cread1, only:outname
  !  use cread2, only: tstart,ijs,js
  !  use cfield, only: ro,xmlto
  !  !!use ModCurvScatt, only: pc_flc
  !  implicit none
  !
  !  real,intent(in) :: t,tstep
  !  real E02(4),E1,pc
  !  integer n
  !  character outname1*40
  !
  !  E1=10.     ! in keV
  !
  !  E02(1)=938272.0813*2      ! p+ rest mass*2
  !  E02(2)=16.*E02(1)         ! O+ rest mass*2
  !  E02(3)=4.*E02(1)          ! He+ rest mass*2
  !  E02(4)=1021.997692        ! e- rest mass*2 in keV
  !
  !  pc=sqrt(E1*(E1+E02(1)))   ! pc for E1 proton
  !
  !  if (t.eq.tstart) then 
  !     call system('mkdir -p FLC')
  !     open(unit=61,file='FLC/'//trim(outname)//'.parm',&
  !          form='unformatted',status='replace')
  !     write(61) tstart,tstep,ir,ip,ijs
  !     write(61) js(1:ijs)
  !     close(61)
  !  endif
  !  write(outname1,'(a,i8.8,a)') 'FLC/'//trim(outname)//&
  !       '_',int(t),'.flc'
  !  open(unit=60,file=trim(outname1),&
  !       form='unformatted',status='replace')
  !  write(60) t
  !  write(60) ro(:,:)
  !  write(60) xmlto(:,:)
  !  write(60) pc/pc_flc(:,:)
  !  do n=1,ijs
  !     write(60) tau_flc(n,:,:)
  !  enddo
  !  close(60)
  !  open(unit=62,file='FLC/'//trim(outname)//'.end',&
  !       form='unformatted',status='replace')
  !  write(62) t
  !  close(62)
  !
  !
  !end subroutine write_FLC

!  !***********************************************************************
!  !                        calc_Daa_FLC      
!  !  Routine calculates parameters required to calculate pitch-angle
!  !   diffusion coefficient (Daa) due to field line curvature 
!  !   (curvature scattering).
!  !***********************************************************************
!  ! This subroutine not used in coupled model, but may save for the future
!  ! Daa calculation
!  subroutine calc_Daa_FLC(i,j,ekev0,Tbounce0,sina)
!
!  use cimigrid_dim, only: iw,ik
!  !!use ModCurvScatt, only: ie,pc_flc,zeta1,zeta2,Daa_flc,ekev_flc
!  implicit none
!  
!  integer,intent(in) :: i,j
!  real,intent(in) :: ekev0(iw,ik),Tbounce0(iw,ik),sina(0:ik+1)
!  real E02,emin,emax,emin_log,emax_log,dlogE,ein_log,ein(ie),pc,&
!       e,e2,e3,e_1,e_2,c,a1,a2,De,b0,w0,tb,D0,Ni0,Amx0,&
!       cosa(0:ik+1),a0(0:ik),cosa_bar,Dmax
!  real,dimension(ie,ik) :: w,b,Amax
!  integer k,m,m_bar
!
!  E02=1021.997692      ! e- rest mass*2 in keV
!
!  emin=minval(ekev0(1 ,1:ik))
!  emax=maxval(ekev0(iw,1:ik))
!  emin_log=log10(emin)
!  emax_log=log10(emax)
!  dlogE=(emax_log-emin_log)/(ie-1)
!  do k=1,ie
!     ein_log=emin_log+float(k-1)*dlogE
!     ein(k)=10.**ein_log
!     ekev_flc(i,j,k)=ein(k)
!  enddo
!
!  do m=0,ik+1
!     a0(m)=asin(sina(m))
!     cosa=cos(a0(m))
!  enddo
!
!  do m=1,ik
!     do k=1,ie
!        pc=sqrt(ein(k)*(ein(k)+E02))
!        ! coefficients as in Young et al. 2002; 2008
!        e=pc/pc_flc(i,j)                         ! epsilon
!        if (e.gt.0.584) e=0.584
!        if (e.gt.0.05) then
!           e2=e*e
!           e3=e2*e
!           w(k,m)=1.051354+0.13513581*e-0.50787555*e2        ! omega
!           c=1.0663037-1.0944973/e+0.016679378/e2-0.00049938987/e3
!           a1=-0.35533865+0.12800347/e+0.0017113113/e2    
!           a2= 0.23156321+0.15561211/e-0.001860433/e2    
!           b(k,m)=-0.51057275+0.93651781/e-0.0031690066/e2    
!           De=-0.49667826-0.00819418/e+0.0013621659/e2  ! D(epsilon)
!           Amax(k,m)=exp(c)*(zeta1(i,j)**a1*zeta2(i,j)**a2+De)
!        else 
!           w(k,m)=0.
!           b(k,m)=0.
!           Amax(k,m)=0. 
!        endif
!     enddo
!  enddo 
!
!  do k=1,ie
!     m_bar=maxloc(Amax(k,:),dim=1)
!     cosa_bar=cosa(m_bar)
!     do m=1,ik
!        Amx0=Amax(k,m)
!        if (Amx0.ne.0.) then
!           w0=w(k,m)
!           b0=b(k,m)
!           Ni0=sin(w0*a0(m_bar))*cosa_bar**b0
!           call lintp(ekev0(:,m),Tbounce0(:,m),iw,ein(k),tb)    ! tau_b
!           D0=0.5*Amx0*Amx0/tb
!           Dmax=4.9348/tb    ! da0=0.5pi during 0.5*tb
!           Daa_flc(i,j,k,m)=D0/Ni0/Ni0*(sin(w0*a0(m))/sina(m))**2*cosa(m)**(2.*b0-2.)
!           if (Daa_flc(i,j,k,m).gt.Dmax) Daa_flc(i,j,k,m)=Dmax
!        else
!           Daa_flc(i,j,k,m)=0.
!        endif
!     enddo
!     Daa_flc(i,j,k,0)=Daa_flc(i,j,k,1)
!     Daa_flc(i,j,k,ik+1)=Daa_flc(i,j,k,ik)
!  enddo
!
!  end subroutine calc_Daa_FLC
