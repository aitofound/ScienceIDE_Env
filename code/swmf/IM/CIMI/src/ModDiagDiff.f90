!*******************************************************************************
!
!                               ModDiagDiff.f90  
!
! This module contains subroutines that calculates momentum space diffusion
!  in (Q1, Q2) coordinates.
!  where (Q1, Q2) is found such that cross-diffusion coef., DQ1Q2 and DQ2Q1 are
!  zero [Albert and Young, 2005].
!
!  Q1 = K
!  const Q2 curves: dE/da0 = DaE/Daa
!
!  D_Q1Q1 = DKK = Daa(dK/da)^2
!  D_Q2Q2 = (DEE - DaE^2/Daa)(dQ2/dE)^2
! 
! Created by Suk-Bin Kang, NASA/GSFC Geospace Physics Lab, in Jan - Mar 2018.
!
!*******************************************************************************

module ModDiagDiff 
  use ModCimiGrid, only: ir=>np,ip=>nt,iw=>nm,ik=>nk
  use ModUtilities, ONLY: CON_stop

  private !! except

  public :: iq            ,&
       !!VarQ          ,& ! variable Q2 [keV]
       !!E_Q2c         ,& ! E [keV] for const. Q2 curve, chorus
       !!E_Q2h         ,& ! E [keV] for const. Q2 curve, hiss
       !!cDqq1,cDqq2   ,& ! diffusion coef. in (Q1,Q2) for chorus
       !!hDqq1,hDqq2   ,& ! diffusion coef. in (Q1,Q2) for chorus
       !!PSD_Q         ,& ! phase space density [#s3/kg3m6] in fixed Q2
       !!PSD_CHTest  ,& !    "   "      " for chorus diffusion test
       !!PSD_HITest  ,& !    "   "      " for chorus diffusion test
       UseDiagDiffusion              ,&  
       UsePitchAngleDiffusionTest    ,&
       UseEnergyDiffusionTest        ,&
       ! subroutines
       diffuse_Q2    ,& ! calcualte diffusion in Q2 and fixed Q1
       diffuse_Q1    ,& !     "         "     in Q1 and fixed Q2
       calc_DQQ      ,& ! calculate Dq1q1, Dq2q2 from Daa,DaE,DEE
       mapPSDtoQ     ,& ! map PSD from M [J/T] to Q2 [keV]
       mapPSDtoE     ,& ! map PSD from Q2 [keV] to M [J/T]
       interpol_D_coefK      ,&
       init_PSD_for_diff_test,&
       calc_Dcoef_for_diff_test,&
       write_PSD_for_diff_test,&
       init_diag_diff
  
  !private :: cSign,hSign  ! sign of DaE for chorus and hiss
  !           rpp          ! plasma pause location in RE
  !           iLpp         ! MLT index at plasma pause location
  !           ipc0,iph     ! closest fpe/fce to table values 
  !                             at given latitude and MLT   
  ! subroutines
  !           find_loc_plasmapause        ,&
  !           tridiagonal                 ,&
  !           find_ompe_index             ,&
  !           calc_Q_curve                ,&
  !           DaEtoDQQ                    ,&
  !           interpol_D_coef             ,&
  !           rk4                         ,&
  !           write_Q2info                ,&
  !           calc_num_ptl                ,&
  !           lintp                       ,&
  !           lintp2                      ,&
  !           locate1                     ,&
  !           spline 
  
  
  ! Variables  
  integer,parameter :: iq=80
  real VarQ(0:iq+1)
  real,allocatable,dimension(:,:,:) :: E_Q2c,E_Q2h,&
       cDqq1,cDqq2,hDqq1,hDqq2,cSign,hSign
  real, allocatable, dimension(:,:,:,:) :: Dqq1K, Dqq2K, dQ2dEK, E_Q2K, PSD_Q, PSDcopy, PSD_Qcopy
  real cLambda2D           ! 1 / analytic solution decay time scale
  real,allocatable :: PSD_CHTest(:),PSD_HITest(:)
  real rpp(ip)
  integer, allocatable :: ipc0(:,:),iph0(:,:),iLpp(:)
  
  logical :: UseDiagDiffusion=.true.,&
       UsePitchAngleDiffusionTest=.false.,&
       UseEnergyDiffusionTest=.false.  
  ! Use test_diff.f90 
  !  instead of cimi.f90

contains

  subroutine init_diag_diff
    
    if( allocated( PSD_Q ) ) RETURN
    allocate( PSD_Q(ir,ip,iq,ik), Dqq1K(ir,ip,iq,0:ik+1), &
         Dqq2K(ir,ip,iq,0:ik+1), dQ2dEK(ir,ip,iq,0:ik+1), &
         E_Q2K(ir,ip,iq,0:ik+1), ipc0(ir,ip), iph0(ir,ip), iLpp(ip), &
         PSDcopy(ir,ip,iw,ik), PSD_Qcopy(ir,ip,iq,ik) )
    
  end subroutine init_diag_diff

! ************************************************************************
!                        find_loc_plasmapause
!  Routine calculates the plasmapause locations
! ************************************************************************
  subroutine find_loc_plasmapause
  use ModCimiGrid, only: np,nt
  use ModCimiTrace, only: ro,iba
  use ModPlasmasphere, ONLY:PlasDensity_C,PlasmaPauseDensity
  implicit none

  integer i,j

  do j=1,nt
     find_Lpp: do i=iba(j),1,-1
        if (PlasDensity_C(i,j).ge.PlasmaPauseDensity) then
           rpp(j)=ro(i,j)
           iLpp(j)=i
           exit find_Lpp 
        endif
     enddo find_Lpp
  enddo

  end subroutine find_loc_plasmapause
  
  
! ************************************************************************
!                            diffuse_Q2
!  Routine calculates the change of electron distributions due to
!  diffusion in Q2.
! ************************************************************************
  subroutine diffuse_Q2
  use ModConst, only: e_mass=>cElectronMass,&
                      EM_speed=>cLightSpeed,&
                      echarge=>cElectronCharge
  use ModCimiPlanet, only: ijs=>nspec,&
                           NameSpeciesExtension_I
  use ModCimiGrid, only: ir=>np,ip=>nt,ik=>nk
  use ModCimiInitialize, only: xjac
  use ModCimi, only: f2,dt
  use CIMI_waves, only: UseChorus, UseHiss,&
                     CHpower,HIpower,&
                     Cpower0,Hpower0,BLc0,BLh0
  use ModCimiTrace, only: iba,bo,ro
  use ModCimi,       ONLY: MinLonPar,MaxLonPar
  !!use diagDiffCoef, only: iq,VarQ,Dqq2K,Dqq2K,dQ2dEK,E_Q2K,PSD_Q,&
  !!                        ipc0,iph0,iLpp
  implicit none

  real,dimension(iq,ik) :: Dqq2,dQ2dE,E1,PSD
  real,dimension(iq) :: G_Q2,Dqq2m,Dqq2g
  real,dimension(iq) :: b1,b2,b3,b4,fnew
  real dU_Q2,jac0,E0,E02,ompe1,Po
  integer n,i,j,k,m,nel
  logical DoDiff

! (0) Do not calculate diffusion in Q2 when Q1 diffusion is tested
  if (.not.UsePitchAngleDiffusionTest) then 

!(1) Find the "n" corresponds to electrons, nel 
  nel=0
  do n=1,ijs
     if (NameSpeciesExtension_I(n).eq.'_e') nel=n
  enddo

!(2) Determine dU_Q2
  dU_Q2=log(VarQ(2)/VarQ(1))
  jac0=dt/dU_Q2/dU_Q2
  E0=e_mass*EM_speed**2/echarge*1.e-3 ! in keV
  E02=2.*E0

  do j=MinLonPar,MaxLonPar    ! start of j
     do i=1,iba(j)      ! start of i
        DoDiff=.False.
        if (i.gt.iLpp(j).and.UseChorus) then
!(3) outside plasmasphere
           Po=(BLc0/bo(i,j))*CHpower(i,j)/Cpower0
           DoDiff=.True.
        else if (i.le.iLpp(j).and.UseHiss) then
!(4) inside plasmasphere
           Po=(BLh0/bo(i,j))*HIpower(i,j)/Hpower0
           DoDiff=.True.
        else
           !!! Either inside the plasmasphere, but not using hiss, ore
           !!! or not using chorus and not using hiss
           Po=-1
           DoDiff=.false.
        endif
        ! no mapping if Po =< 0.
        !write(*,*) '!!! Po', Po
        if (Po.le.0.) DoDiff=.False.
        if (DoDiff) then
           Dqq2(1:iq,1:ik)=Dqq2K(i,j,1:iq,1:ik)*Po
           dQ2dE(1:iq,1:ik)=dQ2dEK(i,j,1:iq,1:ik)
           E1(1:iq,1:ik)=E_Q2K(i,j,1:iq,1:ik)
           PSD(1:iq,1:ik)=PSD_Q(i,j,1:iq,1:ik)
           do m=1,ik      ! start of m
!(5) calulate Jacobian G_Q2
              G_Q2(:)=(E1(:,m)+E0)*sqrt(E1(:,m)*(E1(:,m)+E02))/dQ2dE(:,m)*VarQ(1:iq)
!(6) calculate Dq2q2 at each mid-grid point
              Dqq2g(:)=Dqq2(:,m)*G_Q2(:)
              do k=1,iq-1   ! start of k
                 Dqq2m(k)=0.5*(Dqq2g(k)+Dqq2g(k+1))    ! arithmetic mean
              enddo       ! end of k
!(7) Set BTCS (Backward Time and Cetral Space) variables
              do k=2,iq-1   ! start of k
                 b1(k)=-Dqq2m(k-1)/G_Q2(k)*jac0
                 b3(k)=-Dqq2m(k  )/G_Q2(k)*jac0
                 b2(k)=1.-b1(k)-b3(k)
                 b4(k)=PSD(k,m)
              enddo       ! end of k
!(8) Apply boundary condition
              if (.not.UseEnergyDiffusionTest) then
                 b4(2)=b4(2)-b1(2)*PSD(1,m)         ! const. at lowest V
              else
                 b2(2)=b1(2)+b2(2)                  ! zero gradient
              endif
                 
              b4(iq-1)=b4(iq-1)-b3(iq-1)*PSD(iq,m)  ! const. at highest V
!(9) Compute diffusion using Thomas Algorithm
              call tridiagonal(b1(2:iq-1),b2(2:iq-1),b3(2:iq-1),b4(2:iq-1),iq-2,&
                               fnew(2:iq-1))
              if (UseEnergyDiffusionTest) fnew(1)=fnew(2)
!(10) Make sure f2 > 0
              do k=2,iq-1   ! start of k
                 if (fnew(k).gt.1.e90) then
                    write(*,*) ' PSD > 1.e90 !'
                    write(*,'(a,4i3)') 'i,j,k,m =',i,j,k,m
                    write(*,'(1p3E15.7)') PSD(k-1:k+1,m)
                    write(*,'(1pE15.7)') fnew(k)
                    write(*,'(1p2E15.7)') Dqq2m(k-1:k)/G_Q2(k)
                    call CON_stop('IM: CIMI dies in diffuse_Q2')
                 endif
                 if (fnew(k).ne.fnew(k).or.fnew(k).lt.-1.e-50) then
                    write(*,'(a,i3,a,i3,a,i3,a,i3,a)') &
                    'At i =',i,' j=',j,', k=',k,', m =',m,' in diffusion_Q2'
                    write(*,*) fnew(k)
                    write(*,'(1p10E11.3)') fnew(2:iq-1)
                    write(*,*) ' f2(t-1)'
                    write(*,'(1p10E11.3)') PSD(:,m)
                    write(*,*) ' D_Q2Q2(t)'
                    write(*,'(1p10E11.3)') Dqq2m(1:iq-1)
                    write(*,*) ' G_Q2(t)'
                    write(*,'(1p10E11.3)') G_Q2(:)
                    write(*,*) ' dQ2/dE(t)'
                    write(*,'(1p10E11.3)') dQ2dE(:,m)
                    write(*,*) ' E(t) for const. Q2'
                    write(*,'(1p10E11.3)') E1(:,m)
                    call CON_stop('IM: CIMI dies in diffuse_Q2')
                 endif
              enddo       ! end of k
!(11) Update PSD_Q
              PSD_Q(i,j,2:iq-1,m)=fnew(2:iq-1)
           enddo          ! end of m
        endif
     enddo             ! end of i
  enddo                ! end of j
  
  endif ! end of (.not.UsePitchAngleDiffusion)
  
  end subroutine diffuse_Q2


! ************************************************************************
!                            diffuse_Q1
!  Routine calculates the change of electron distributions due to
!  diffusion in Q1(=K).
! ************************************************************************
  subroutine diffuse_Q1
  use ModCimiPlanet, only: re_m,&
                           ijs=>nspec,&
                           NameSpeciesExtension_I
  use ModConst, only: e_mass=>cElectronMass,&
                      EM_speed=>cLightSpeed,&
                      echarge=>cElectronCharge
  use ModCimiGrid, only: ir=>np,ip=>nt,ik=>nk
  use ModCimiInitialize, only: xjac,xk
  use ModCimi, only: f2,dt
  use CIMI_waves, only: UseChorus, UseHiss,&
                     CHpower,HIpower,&
                     Cpower0,Hpower0,BLc0,BLh0
  use ModCimiTrace, only: iba,bo,bm,ro,tya,y=>sinA
  use ModCimi,       ONLY: MinLonPar,MaxLonPar
  !!use diagDiffCoef, only: iq,Dqq1K,PSD_Q,&
  !!                        ipc0,iph0,iLpp
  implicit none

  real,dimension(iq,ik) :: PSD
  real,dimension(iq,0:ik+1) :: Daa1
  real,dimension(0:ik+1) :: VarK,G_a,bm1,G_Q1,dKda_K,Dqq1m,Dqq1g
  real,dimension(ik) :: b1,b2,b3,b4,fnew
  real dU_Q1,jac0,Po,cosa,sina
  integer n,i,j,k,m,nel
  logical DoDiff

! (0) Do not calculate diffusion in Q2 when Q1 diffusion is tested
  if (.not.UseEnergyDiffusionTest) then 

!(1) Find the "n" corresponds to electrons, nel 
  nel=0
  do n=1,ijs
     if (NameSpeciesExtension_I(n).eq.'_e') nel=n
  enddo

  do j=MinLonPar,MaxLonPar ! start of j
     do i=1,iba(j)         ! start of i
!(2) Determine dU_Q1
        !!VarK(0:ik+1)=xk(0:ik+1)/re_m*ro(i,j)  ! K in T^0.5 RE
        VarK(0:ik+1)=xk(0:ik+1)/re_m  ! K in T^0.5 RE
        dU_Q1=log(VarK(2)/VarK(1))
        jac0=dt/dU_Q1/dU_Q1
        DoDiff=.False.
        if (i.gt.iLpp(j).and.UseChorus) then
!(3) outside plasmasphere
           Po=(BLc0/bo(i,j))*CHpower(i,j)/Cpower0
           DoDiff=.True.
        else if (UseHiss) then
!(4) inside plasmasphere
           Po=(BLh0/bo(i,j))*HIpower(i,j)/Hpower0
           DoDiff=.True.
        else 
           !!! Either inside the plasmasphere, but not using hiss, ore
           !!! or not using chorus and not using hiss
           Po=-1.
           DoDiff=.false.
        endif
        ! no mapping if Po =< 0.
        if (Po.le.0.) DoDiff=.False.
        if (DoDiff) then
           Daa1(1:iq,0:ik+1)=Dqq1K(i,j,1:iq,0:ik+1)*Po
           PSD(1:iq,1:ik)=PSD_Q(i,j,1:iq,1:ik)
!(5) calulate Jacobian G_Q1
           bm1(1:ik)=bm(i,j,1:ik)
           bm1(0)=bo(i,j)/y(i,j,0)**2
           bm1(ik+1)=bo(i,j)/y(i,j,ik+1)**2
           do m=0,ik+1
              sina=y(i,j,m)
              cosa=sqrt(1.-sina*sina)
              G_a(m)=tya(i,j,m)*sina*cosa                            ! T(a0)sin2a0/2
              ! note: Tya is normalized in 1/4 bounce versus
              !       VarK or si is in 1/2 bounce.
              !write(*,*) 'ro(i,j),bm1(m),cosa,sina,tya(i,j,m),VarK(m)',ro(i,j),bm1(m),cosa,sina,tya(i,j,m),VarK(m)
              dKda_K(m)=2.*ro(i,j)*sqrt(bm1(m)) &
                       *cosa/sina*tya(i,j,m)/VarK(m) ! -dK/da/K
              G_Q1(m)=G_a(m)/dKda_K(m)
           enddo
           do k=1,iq        ! start of k
!(6) calculate Dq1q1 at each mid-grid point
              Dqq1g(0:ik+1)=Daa1(k,0:ik+1)*G_a(0:ik+1)*dKda_K(0:ik+1) ! DKK*G_Q1*K2
              do m=0,ik     ! start of m
                 Dqq1m(m)=0.5*(Dqq1g(m)+Dqq1g(m+1))              ! arithmetic mean
              enddo         ! end of m
!(7) Set BTCS (Backward Time and Cetral Space) variables
              do m=1,ik     ! start of k
                 b1(m)=-Dqq1m(m-1)/G_Q1(m)*jac0
                 b3(m)=-Dqq1m(m)/G_Q1(m)*jac0
                 b2(m)=1.-b1(m)-b3(m)
                 b4(m)=PSD(k,m)
              enddo         ! end of m
!(8) Apply boundary condition
              b2(1)=b1(1)+b2(1)                     ! zero gradient at lowest K
              if (.not.UsePitchAngleDiffusionTest) &
                 b2(ik)=b2(ik)+b3(ik)               ! zero gradient at highest K
!(9) Compute diffusion using Thomas Algorithm
              call tridiagonal(b1,b2,b3,b4,ik,fnew)
              !!call tridiagonal(b1(1:ik-1),b2(1:ik-1),b3(1:ik-1),&
              !!                 b4(1:ik-1),ik-1,fnew(1:ik-1))
              !!fnew(ik)=PSD(k,ik)
!(10) Make sure f2 > 0
              do m=1,ik     ! start of m
                 if (fnew(m).ne.fnew(m).or.fnew(m).lt.-1.e-50) then
                    write(*,'(a,i3,a,i3,a,i3,a,i3,a)') &
                    'At i =',i,' j=',j,', k=',k,', m =',m,' in diffusion_Q1'
                    write(*,*) fnew(m)
                    write(*,'(1p10E11.3)') fnew(1:ik)
                    write(*,*) ' f2(t-1) in #s3/m6/kg3'
                    write(*,'(1p10E11.3)') PSD(k,:)
                    write(*,*) ' DKK(t) in 1/s'
                    write(*,'(1p10E11.3)') Dqq1m(0:ik)/G_Q1(0:ik)
                    write(*,*) ' a0(t) in deg'
                    write(*,'(10f11.4)') asin(y(i,j,0:ik+1))*57.2957795
                    write(*,*) ' Daa(t) in 1/s'
                    write(*,'(1p10E11.3)') Daa1(k,0:ik+1)
                    write(*,*) ' T(y) in RE'
                    write(*,'(1p10E11.3)') tya(i,j,0:ik+1)
                    call CON_stop('IM: CIMI dies in diffuse_Q1')
                 endif
              enddo       ! end of k
!(11) Update PSD_Q
              PSD_Q(i,j,k,1:ik)=fnew(1:ik)
           enddo          ! end of k
        endif
     enddo             ! end of i
  enddo                ! end of j
  
  endif ! end of (.not.UseEnergyDiffusionTest)
  
  end subroutine diffuse_Q1


! ************************************************************************
!                          tridiagonal      
!
!  Subroutine solves inversion of tridiagonal matrix 
!   through Thomas's tridiagonal matrix algorithm.
!
!  b1*fi-1 + b2*fi + b3*fi+1 = b4,  i= 1,2,3,..,n 
!
!  Inputs : b1,b2,b3,b4,n
!  Outputs : f
! ************************************************************************
      subroutine tridiagonal(b1,b2,b3,b4,n,f)
      implicit none

      integer,intent(in) :: n
      real,intent(in) :: b1(n),b2(n),b3(n),b4(n)
      real,intent(out) :: f(n)
      real c0,c1(n),c2(n)
      integer i
 
      c1(1)=b3(1)/b2(1)
      c2(1)=b4(1)/b2(1)
      do i=2,n
         c0=b2(i)-b1(i)*c1(i-1)
         c1(i)=b3(i)/c0
         c2(i)=(b4(i)-b1(i)*c2(i-1))/c0
      enddo      

      f(n)=c2(n)
      do i=n-1,1,-1
         f(i)=c2(i)-c1(i)*f(i+1)
      enddo

      end subroutine tridiagonal


! ************************************************************************
!                        find_ompe_index       
!
! Subroutine finds indices of closest ompe 
! ************************************************************************
  subroutine find_ompe_index
  use ModCimiGrid, only: ir=>np,ip=>nt,ik=>nk
  use ModCimiTrace, only: iba
  use CIMI_waves, only: UseChorus, UseHiss,&
                         ompe,&
                         cOmpe,hOmpe,&
                         ipc,iph
  use ModCimi,       ONLY: MinLonPar,MaxLonPar
  !!use diagDiffCoeff, only:ipc0,iph0
  implicit none

  real ompe1
  integer i,j,ipc1,iph1

!!!
  ipc1=0
  iph1=0
  
  do j=MinLonPar,MaxLonPar ! start of j
     do i=1,iba(j)         ! start of i
        ompe1=ompe(i,j)                         ! fpe/fce
        if (UseChorus) then
           !write(*,*)' '
           !write(*,*)'ompe1 = ',ompe1
           !write(*,*)'cOmpe',cOmpe
           call locate1(cOmpe,ipc,ompe1,ipc1,'find_ompe_index')
           if (ipc1.eq.0) ipc1=1
           if (ipc1.lt.ipc) then
              if((cOmpe(ipc1+1)-ompe1).lt.(ompe1-cOmpe(ipc1))) ipc1=ipc1+1
              ipc0(i,j)=ipc1
           else
              !!!ipc1=ipc
              ipc0(i,j)=ipc
           endif
        endif
        if (UseHiss) then
           call locate1(hOmpe,iph,ompe1,iph1,'find_ompe_index')
           if (iph1.eq.0) iph1=1
           if (iph1.gt.iph) iph1=iph
           if (iph1.lt.iph) then
              if ((hOmpe(iph1+1)-ompe1).lt.(ompe1-hOmpe(iph1))) iph1=iph1+1
              iph0(i,j)=iph1
           else
              !!!iph1=iph
              iph0(i,j)=iph1
           endif
        endif
     enddo              ! end of i
  enddo                 ! end of j

  do j=MinLonPar,MaxLonPar ! start of j
     do i=1,iba(j)         ! start of i
        if (ipc0(i,j)==0) write(*,*) i,j,ompe(i,j),cOmpe(:)
     enddo
  enddo

  end subroutine find_ompe_index
  
  
!*****************************************************************************
!                           calc_Dqq    
!  Routine find grids (Q1,Q2) and calculates diffusion coefficients in (Q1,Q2).
!  DQQ = (Daa - DaE^2/Daa)(dQ/dE)**2 = (DKK - 2DKE^2/DKK)*(dQ/dE)**2
!
!  Q1=a0 (or K)
!  Q2 is chosen such that Dq1q2=0 [Albert and Young 2005; Camperol et al. 2013]
!
!  Note
!  All Daa,DaE,DEE due to all plasma waves at the same location should be 
!   taken into account, to determine (Q1,Q2)
!*****************************************************************************
  subroutine calc_DQQ
  use ModNumConst, only: pi=>cPi
  use ModCimiGrid, only: ir=>np,ip=>nt,ik=>nk,&
                         iProc
  use CIMI_waves, only: ipc,iwc,iph,iwh,ipa,&
                         UseChorus, UseHiss,&
                         ckeV,hkeV,&
                         cDEE,cDaa,cDaE,&
                         hDEE,hDaa,hDaE,&
                         cPA,cOmpe,hOmpe
  !!use diagDiffCoef, only: iq,VarQ,E_Q2c,E_Q2h,cDqq1,cDqq2,hDqq1,hDqq2,&
  !!                        cSign,hSign,Dqq1K,Dqq2K,dQ2dEK,E_Q2K
  implicit none

  real Qmin,Qmax,rQ,Eq(0:iq+1),logEq(0:iq+1)
  real,dimension(ipc,0:iq+1,ipa) :: cDaa0,cDaE0,cDEE0
  real,dimension(iph,0:iq+1,ipa) :: hDaa0,hDaE0,hDEE0
  integer j,k,m,k1,k2

!(1) Set variable Q2
   Qmin=1.      ! at k = 1  
   Qmax=1.e4    ! at k = iq+1
   rQ=10.**(log10(Qmax/Qmin)/iq)
   VarQ(0)=Qmin/rQ
   do k=1,iq+1
      VarQ(k)=VarQ(k-1)*rQ
   enddo
   Eq(0:iq+1)=VarQ(0:iq+1)
   logEq(:)=log(Eq(:))

  if (UseChorus) then
     allocate (cSign(ipc,iwc,ipa))
     cSign(:,:,:)=sign(1.,cDaE(:,:,:))
     where (cDaE(:,:,:).eq.0.) cSign(:,:,:)=1.

!(2) interpolate diffusion coef. to same grids of Q
     call interpol_D_coef(cDaa,cDaE,cDEE,cSign,ckeV,logEq,ipc,iwc,iq,cDaa0,cDaE0,cDEE0)
     allocate (E_Q2c(ipc,0:iq+1,ipa))
     allocate (cDqq1(ipc,iq,ipa),cDqq2(ipc,iq,ipa))

!(3) calculate energy crossponding to constant Q
     call calc_Q_curve(cDaa0,cDaE0,Eq,cPA*pi/180,ipc,0,iq+1,ipa,E_Q2c,'chorus')

!(4) calculate Daa and Dqq2 at the fixed grids of (a0,Q2)
     call DaEtoDQQ(cDaa0,cDEE0,cDaE0,Eq,E_Q2c,VarQ,ipc,iq,iq,&
                   cDqq1,cDqq2,'chorus')
  endif

!(5) Hiss
  if (UseHiss) then
     allocate (hSign(iph,iwh,ipa))
     hSign(:,:,:)=sign(1.,hDaE(:,:,:))
     where (hDaE(:,:,:).eq.0.) hSign(:,:,:)=1.
!(6) interpolate diffusion coef. to same grids of Q for hiss
     call interpol_D_coef(hDaa,hDaE,hDEE,hSign,hkeV,logEq,iph,iwh,iq,hDaa0,hDaE0,hDEE0)
     allocate (E_Q2h(iph,0:iq+1,ipa))
     allocate (hDqq1(iph,iq,ipa),hDqq2(iph,iq,ipa))

!(7) calculate energy crossponding to constant Q for hiss
     call calc_Q_curve(hDaa0,hDaE0,Eq,cPA*pi/180,iph,0,iq+1,ipa,E_Q2h,'hiss')

!(8) calculate Daa and Dqq2 at the fixed grids of (a0,Q2) for hiss
     call DaEtoDQQ(hDaa0,hDEE0,hDaE0,Eq,E_Q2h,VarQ,iph,iq,iq,&
                   hDqq1,hDqq2,'hiss')
  endif

  if (iProc.eq.0) then
     if (UseChorus) call write_Q2info(cDEE,cDaE,cDaa0,cDaE0,cDEE0,&
                       cDqq1,cDqq2,&
                       ckeV,cOmpe,Eq,E_Q2c,ipc,iwc,iq,&
                       'IM/plots/chorus_constQ.dat',&
                       'IM/plots/chorus_constQ2.dat',&
                       'IM/plots/D_LBchorus_QQ.dat')
     if (UseHiss) call write_Q2info(hDEE,hDaE,hDaa0,hDaE0,hDEE0,&
                       hDqq1,hDqq2,&
                       hkeV,hOmpe,Eq,E_Q2h,iph,iwh,iq,&
                       'IM/plots/hiss_constQ.dat',&
                       'IM/plots/hiss_constQ2.dat',&
                       'IM/plots/D_hiss_QQ.dat')
  endif
  call interpol_D_coefK
  
  end subroutine calc_DQQ


!*****************************************************************************
!                           calc_Q_curve    
!  Routine calculates energy E(a,Q) corresponding to contant Q curve.
!*****************************************************************************
  subroutine calc_Q_curve(Daa,DaE,keV,a0,ip,iw0,iw,ipa,E,NameWave)
  use ModNumConst, only: pi=>cPi
  implicit none

  integer,intent(in) :: ip,iw0,iw,ipa
  real,dimension(ip,iw0:iw,ipa),intent(in) :: Daa,DaE
  real,intent(in) :: keV(iw0:iw),a0(ipa)
  real,intent(out) :: E(ip,iw0:iw,ipa)
  character(len=*),intent(in) :: NameWave
  real,allocatable :: da(:),keV1(:),dEda(:,:)
  real r,dEdam,dEda1
  integer j,k,m,m0,k1,k2,kk,iww

!!! this is a limit on the DaE to Daa ratio which in the future should
  !!! be a parameter input in PARAM.in. We should also include an offset
  real, parameter :: DaEtoDaaMax = 0.0
  real, parameter :: LowEnerLimit = 10.0
  
  
  iww=iw-iw0+1
!!!allocate (da(ipa-1))
  allocate (da(ipa))
  allocate (keV1(iww),dEda(iww,ipa))
 
  r=0.99  ! minimum ratio difference between adjacent grids
  do m=1,ipa-1
     da(m)=a0(m+1)-a0(m)
  enddo
!!!
  da(ipa)=a0(ipa)-a0(ipa-1)
  
  keV1(1:iww)=keV(iw0:iw)

  do j=1,ip
     dEda(:,:)=0.
!    where(Daa(j,:,:).ne.0.) 
!       dEda(1:iww,1:ipa)=&
!            min(DaE(j,iw0:iw,1:ipa)/Daa(j,iw0:iw,1:ipa),DaEtoDaaMax)
!       dEda(1:iww,1:ipa)=&
!            max(dEda(1:iww,1:ipa),-1.0*DaEtoDaaMax)
!    end where

     do m=1,ipa
        do k=1,iww
           if (Daa(j,k+iw0-1,m).ne.0. .and. keV1(k)<LowEnerLimit) then
              dEda(k,m)=&
                   min(DaE(j,k+iw0-1,m)/Daa(j,k+iw0-1,m),DaEtoDaaMax)
              dEda(k,m)=&
                   max(dEda(k,m),-1.0*DaEtoDaaMax)
           end if
        end do
     end do
           
     do m=1,ipa
        dEda(:,m)=dEda(:,m)*keV1(:)
     enddo
     do k=iw0,iw
        call rk4(keV(k),da,keV1,a0,dEda(:,:),iww,ipa,E(j,k,:))
     enddo
! make sure E is monotonically increasing
     do m=2,ipa
        m0=m-1
        do k=iw-1,iw0,-1
           k2=k+1
           if (E(j,k,m).ge.E(j,k2,m)) then
              write(*,*) ' E(a0,Q) calculation'
              write(*,'(a,3i3,a)') ' At j,k,m =',j,k,m,',  E(k-1)>E(k)'
              write(*,'(a,1p2E11.3)') ' E(j,k,1),E(j,k+1,1) =',E(j,k:k2,1)
              write(*,'(a,1p2E11.3)') ' E(j,k,m),E(j,k+1,m) =',E(j,k:k2,m)
              write(*,'(a,1p2E11.3)') ' DaE/Daa(k,m-1:m) (in keV) =',dEda(k,m0:m)*keV(k)
              write(*,'(a,1p2E11.3)') ' DaE/Daa(k+1,m-1:m) (in keV) =',dEda(k+1,m0:m)*keV(k+1)
              if (E(j,k2,m)/E(j,k,m).ge.0.97) then
                 !!E(j,k,m)=0.97*E(j,k2,m)
              else
                 call CON_stop('IM: CIMI dies in calc_Q_curve for '//NameWave)
                 write(*,'(f6.2,a)') E(j,k2,m)/E(j,k,m)*100.,' %'
              endif
              call CON_stop('IM: CIMI dies in calc_Q_curve for '//NameWave)
              !!E1=E(j,k2,m)*r
              !!E(j,k,m)=E1
           endif
        enddo
     enddo
  enddo
  
  deallocate (da,keV1,dEda)

  end subroutine calc_Q_curve


!*****************************************************************************
!                           DaEtoDQQ  
!  Routine calculates DQQ(a0,Q) at fixed grids of (a0,Q)
!   from Daa,DEE,DaE at fixed grids of (a0,E)
!*****************************************************************************
     subroutine DaEtoDQQ(Daa,DEE,DaE,keV,E,VarQ,ip,iw,iq,Dqq1,Dqq2,NameWave)
     use CIMI_waves, only:ipa
     use ModCimi,       ONLY: MinLonPar,MaxLonPar
     implicit none

     integer,intent(in) :: ip,iw,iq
     real,dimension(ip,0:iw+1,ipa),intent(in) :: Daa,DEE,DaE
     real,dimension(ip,0:iq+1,ipa),intent(in) :: E
     real,intent(in) :: keV(0:iw+1),VarQ(0:iq+1)
     character(len=*),intent(in) :: NameWave
     real,dimension(ip,iq,ipa),intent(out) :: Dqq1,Dqq2
     real,dimension(iw+2) ::  keVlog,logDaa,logDEE,logDaE
     real,dimension(0:iq+1) :: logE,VarQ2,E2
     real dQ(iq),Daa1,DaE1,DEE1,dQdE,DaE2,DaE2max,Dqq0,&
          logDaa1,logDaE1,logDEE1
     integer k1,iw1,j,k,m

     Dqq1(:,:,:)=0.
     Dqq2(:,:,:)=0.
     keVlog(1:iw+2)=log(keV(0:iw+1))
     do k=1,iq
        dQ(k)=VarQ(k+1)-VarQ(k-1)
     enddo
     VarQ2(0:iq+1)=VarQ(0:iq+1)*VarQ(0:iq+1) 

     do j=1,ip
        do m=1,ipa
           E2(0:iq+1)=E(j,0:iq+1,m)*E(j,0:iq+1,m)
           !write(*,*) ' '
           !write(*,*) E(j,0:iq+1,m)
           logE(0:iq+1)=log(E(j,0:iq+1,m))
           logDaa(:)=-75.
           logDaE(:)=-75.
           logDEE(:)=-75.
           where(Daa(j,0:iw+1,m).ne.0.) &
                logDaa(1:iw+2)=log(Daa(j,0:iw+1,m))
           where(DaE(j,0:iw+1,m).ne.0.) &
                logDaE(1:iw+2)=log(abs(DaE(j,0:iw+1,m))) !  in s^-1
           where(DEE(j,0:iw+1,m).ne.0.) &
                logDEE(1:iw+2)=log(DEE(j,0:iw+1,m))      !  in s^-1
           do k=1,iq
              if (logE(k).ge.keVlog(1).and.logE(k).le.keVlog(iw+2)) then
                 call lintp(keVlog(:),logDaa(:),iw+2,logE(k),logDaa1,'DaEtoDQQ')
                 call lintp(keVlog(:),logDaE(:),iw+2,logE(k),logDaE1,'DaEtoDQQ')
                 call lintp(keVlog(:),logDEE(:),iw+2,logE(k),logDEE1,'DaEtoDQQ')
                 if (logDaa1.gt.-70.) then
                    Daa1=exp(logDaa1)
                 else
                    Daa1=0.
                 endif
                 if (logDEE1.gt.-70.) then
                    DEE1=exp(logDEE1)
                 else
                    DEE1=0.
                 endif
                 if (Daa1.gt.0..and.DEE1.gt.0.) then
                    if (logDaE1.gt.-70.) then
                       DaE1=exp(logDaE1)
                    else
                       DaE1=0.
                    endif
                 else
                    DaE1=0.
                 endif
                 Dqq1(j,k,m)=Daa1
                 if (Daa1.eq.0.) then
                    Dqq2(j,k,m)=DEE1*E2(k)/VarQ2(k)
                 else
                    DaE2=DaE1*DaE1
                    DaE2max=Daa1*DEE1
                    if (DaE2.gt.DaE2max) DaE2=DaE2max
                    DQQ0=(DaE2max-DaE2)/Daa1
                    dQdE=dQ(k)/(E(j,k+1,m)-E(j,k-1,m))
                    Dqq2(j,k,m)=DQQ0*dQdE*dQdE*E2(k)/VarQ2(k) ! Dqq2/Q^2 in 1/s
                 endif
                 if (Dqq2(j,k,m).lt.0.) then
                    write(*,*) ' Dqq2 has to be positive definite!'
                    write(*,'(a,3i3,a,1p5E11.3)')&
                     ' At j,k,m =',j,k,m,', DQQ,Daa,DEE,DaE =',&
                     Dqq2(j,k,m),Daa1,DEE1,DaE1,DEE1-DaE1*DaE1/Daa1
                    call CON_stop('IM: CIMI dies in mapping DaE to Dqq for '//NameWave)
                 endif
              endif
           enddo
        enddo
     enddo

     end subroutine DaEtoDQQ


!*****************************************************************************
!                         interpol_D_coef
!  Routine interpolates Daa,DaE,DEE to Q2(m=1) energy grids
!   where Q2 = E at a0 = a0(m=1)
!*****************************************************************************
     subroutine interpol_D_coef(Daa,DaE,DEE,xSign,keV,logE,ip,iw,iq,Daa0,DaE0,DEE0)
     use CIMI_waves, only: cPA,ipa
     use ModCimi,       ONLY: MinLonPar,MaxLonPar
     implicit none

     integer,intent(in) :: ip,iw,iq
     real,dimension(ip,iw,ipa),intent(in) :: Daa,DaE,DEE,xSign
     real,intent(in) :: keV(iw),logE(0:iq+1)
     real,dimension(ip,0:iq+1,ipa),intent(out) :: Daa0,DaE0,DEE0
     real,dimension(iw) :: keVlog,logDoo
     !!real,dimension(iw) :: b,c,d
     real minDoo,sign1,DaEmax,dE,dE2
     integer j,k,m,kk

     keVlog(:)=log(keV(:))
     Daa0(:,:,:)=0.   
     DaE0(:,:,:)=0.   
     DEE0(:,:,:)=0.   
     do j=1,ip
        do m=1,ipa
!(1) interpolate Daa
           minDoo=minval(Daa(j,:,m),Daa(j,:,m).gt.0.)
           minDoo=log(minDoo)
           logDoo(:)=-115.
           where(Daa(j,:,m).gt.0.) logDoo(:)=log(Daa(j,:,m))
           do k=0,iq+1
           ! linear interpolation
              call lintp(keVlog,logDoo,iw,logE(k),Daa0(j,k,m),&
                         'interpol_D_coef')
              if (Daa0(j,k,m).ge.minDoo) then
                 Daa0(j,k,m)=exp(Daa0(j,k,m))
              else
                 Daa0(j,k,m)=0.
              endif
           enddo
!(2) interpolate DEE
           minDoo=minval(DEE(j,:,m),DEE(j,:,m).gt.0.)
           minDoo=log(minDoo)
           logDoo(:)=-115.
           where(DEE(j,:,m).gt.0.) logDoo(:)=log(DEE(j,:,m))
           do k=0,iq+1
           ! linear interpolation
              call lintp(keVlog,logDoo,iw,logE(k),DEE0(j,k,m),&
                         'interpol_D_coef')
              if (DEE0(j,k,m).ge.minDoo) then
                 DEE0(j,k,m)=exp(DEE0(j,k,m))
              else
                 DEE0(j,k,m)=0.
              endif
           enddo
!(3) interpolate DaE
           minDoo=minval(abs(DaE(j,:,m)),abs(DaE(j,:,m)).gt.0.)
           minDoo=log(minDoo)
           logDoo(:)=-115.
           where(abs(DaE(j,:,m)).gt.0.) logDoo(:)=log(abs(DaE(j,:,m)))
           do k=0,iq+1
           ! linear interpolation
              call lintp(keVlog,logDoo,iw,logE(k),DaE0(j,k,m),&
                         'interpol_D_coef')
              call lintp(keVlog,xSign(j,:,m),iw,logE(k),sign1,&
                         'interpol_D_coef')
              if (DaE0(j,k,m).ge.minDoo) then
                 DaE0(j,k,m)=sign(exp(DaE0(j,k,m)),sign1)
              else
                 DaE0(j,k,m)=0.
              endif
           ! DaE < sqrt(Daa * DEE)
              if (Daa0(j,k,m).eq.0..or.DEE0(j,k,m).eq.0.) then
                 DaE0(j,k,m)=0.
              else
                 DaEmax=sqrt(Daa0(j,k,m)*DEE0(j,k,m))
                 if (abs(DaE0(j,k,m)).gt.DaEmax) &
                    DaE0(j,k,m)=sign(DaEmax,sign1)
              endif
           enddo
        enddo
     enddo
     end subroutine interpol_D_coef


!*****************************************************************************
!                         interpol_D_coefK
!  Routine interpolates Dqq1,Dqq2 from a0=1,...,89 to K grids
!      and calculates jacobian dQ2/dE
!*****************************************************************************
  subroutine interpol_D_coefK
  use ModNumConst, only: pi=>cPi
  use ModCimiGrid, only: ir=>np,ip=>nt,ik=>nk,&
                         MinLonPar,MaxLonPar
  use CIMI_waves, only: ipc,iwc,iph,iwh,ipa,&
                         UseChorus, UseHiss,&
                         cOmpe,cPA
  use ModCimiTrace, only: y=>sinA,iba,ro
  !use ModCimiGrid,       ONLY: MinLonPar,MaxLonPar
  !!use diagDiffCoef, only: iq,cDqq1,cDqq2,hDqq1,hDqq2,Dqq1K,&
  !!                        Dqq2K,E_Q2c,E_Q2h,dQ2dEK,E_Q2K,VarQ,&
  !!                        ipc0,iph0,iLpp
  implicit none

  real a0(0:ik+1),dQ(iq),&
       PAwBound(ipa+2),&     ! pitch angle from 0 - 90 deg
       DkkIn(ipa+2),&        ! D_Q1Q1 going in to interpolation
       DqqIn(ipa+2),&        ! D_Q2Q2 going in to interpolation
       Eq2In(ipa+2)          ! E_Q2 going in to interpolation
  real RadToDeg
  integer i,j,k,m,l,m1,m2,ipc1,iph1
 
  call find_ompe_index

  RadToDeg=180./pi

  do k=1,iq
     dQ(k)=VarQ(k+1)-VarQ(k-1)
  enddo

  PAwBound(2:ipa+1)=cPA(1:ipa)
  PAwBound(1)=PAwBound(2)
  PAwBound(ipa+2)=PAwBound(ipa+1)

  ! find Lpp(1:nt)
  call find_loc_plasmapause

  do j=MinLonPar,MaxLonPar
!(1) interpolate begins when L>Lpp for chorus
     do i=iLpp(j)+1,iba(j)
        if (UseChorus) then
           ipc1=ipc0(i,j)
           a0(0:ik+1)=asin(y(i,j,0:ik+1))*RadToDeg
           m1=ik+1
           m2=0
           do m=0,ik+1
              !!if (a0(m).ge.PAwBound(1).and.a0(m).le.PAwBound(ipa+2)) then
              if (a0(m).ge.cPA(1).and.a0(m).le.cPA(ipa)) then
                 if (m.le.m1) m1=m
                 if (m.ge.m2) m2=m
!!!!!!!!!!if (i.eq.iLpp(1)+4.and.j.eq.1) write(*,*) m,a0(m),m1,m2
!(2) interpolate Dqq1, Dqq2 to y grids
                 do k=1,iq
                    call lintp(cPA,cDqq1(ipc1,k,:),ipa,a0(m),Dqq1K(i,j,k,m),&
                               'interpol_D_coefK')
                    if (Dqq1K(i,j,k,m).ne.Dqq1K(i,j,k,m).or.&
                        Dqq1K(i,j,k,m).lt.0.) then
                       write(*,*) 'i,j,k,m =',i,j,k,m
                       write(*,*) 'Dqq1K =',Dqq1K(i,j,k,m)
                       call CON_stop('IM: CIMI dies in '//'interpol_D_coefK')
                    endif
                    call lintp(cPA,cDqq2(ipc1,k,:),ipa,a0(m),Dqq2K(i,j,k,m),&
                               'interpol_D_coefK')
                    if (Dqq2K(i,j,k,m).ne.Dqq2K(i,j,k,m).or.&
                        Dqq2K(i,j,k,m).lt.0.) then
                       write(*,*) 'i,j,k,m =',i,j,k,m
                       write(*,*) 'Dqq2K =',Dqq2K(i,j,k,m)
                       call CON_stop('IM: CIMI dies in '//'interpol_D_coefK')
                    endif
                    call lintp(cPA,E_Q2c(ipc1,k,:),ipa,a0(m),E_Q2K(i,j,k,m),&
                               'interpol_D_coefK')
                    if (E_Q2K(i,j,k,m).ne.E_Q2K(i,j,k,m).or.&
                        E_Q2K(i,j,k,m).le.0.) then
                       write(*,*) 'i,j,k,m =',i,j,k,m
                       write(*,*) 'E_Q2K =',E_Q2K(i,j,k,m)
                       write(*,*) 'a0 (=asin(sinA(i,j,:))'
                       write(*,'(10f8.3)') a0
                       write(*,*) 'cPA'
                       write(*,'(10f8.3)') cPA
                       write(*,*) 'E_Q2c (ipc1,k,:)'
                       write(*,'(1p9E13.5)') E_Q2c(ipc1,k,:)
                       call CON_stop('IM: CIMI dies in '//'interpol_D_coefK')
                    endif
                 enddo   ! end of k
              endif
           enddo         ! end of m
!(3) assign Dqq1, Dqq2, where <1 deg and >89 deg
           do m=0,m1
              Dqq1K(i,j,:,m)=Dqq1K(i,j,:,m1)
              Dqq2K(i,j,:,m)=Dqq2K(i,j,:,m1)
              E_Q2K(i,j,:,m)=VarQ(1:iq)
           enddo         ! end of m
           do m=m2,ik+1
              Dqq1K(i,j,:,m)=Dqq1K(i,j,:,m2)
              Dqq2K(i,j,:,m)=Dqq2K(i,j,:,m2)
              E_Q2K(i,j,:,m)=E_Q2K(i,j,:,m2)
           enddo         ! end of m
!(4) Calculate jacobian dQ2/dE
           do m=0,ik+1
              do k=2,iq-1
                 dQ2dEK(i,j,k,m)=dQ(k)/(E_Q2K(i,j,k+1,m)-E_Q2K(i,j,k-1,m))
              enddo      ! end of k
              dQ2dEK(i,j,1,0:ik+1)=dQ2dEK(i,j,2,0:ik+1)
              dQ2dEK(i,j,iq,0:ik+1)=dQ2dEK(i,j,iq-1,0:ik+1)
           enddo         ! end of m
        endif
     enddo               ! end of i
  enddo                  ! end of j
  do j=MinLonPar,MaxLonPar
!(5) interpolate begins when L<Lpp for hiss
     do i=1,iLpp(j)
        if (UseHiss) then
           iph1=iph0(i,j)
           a0(:)=asin(y(i,j,:))*RadToDeg
           m1=ik+1
           m2=0
           do m=0,ik+1
              if (a0(m).ge.cPA(1).and.a0(m).le.cPA(ipa)) then
                 if (m.le.m1) m1=m
                 if (m.ge.m2) m2=m
!(6) interpolate Dqq1, Dqq2 to y grids
                 do k=1,iq
                    call lintp(cPA,hDqq1(iph1,k,:),ipa,a0(m),Dqq1K(i,j,k,m),&
                               'interpol_D_coefK')
                    if (Dqq1K(i,j,k,m).ne.Dqq1K(i,j,k,m).or.& 
                        Dqq1K(i,j,k,m).lt.0.) then
                       write(*,*) 'i,j,k,m =',i,j,k,m
                       write(*,*) 'Dqq1K =',Dqq1K(i,j,k,m)
                       call CON_stop('IM: CIMI dies in '//'interpol_D_coefK')
                    endif
                    call lintp(cPA,hDqq2(iph1,k,:),ipa,a0(m),Dqq2K(i,j,k,m),&
                               'interpol_D_coefK')
                    if (Dqq2K(i,j,k,m).ne.Dqq2K(i,j,k,m).or.& 
                        Dqq2K(i,j,k,m).lt.0.) then
                       write(*,*) 'i,j,k,m =',i,j,k,m
                       write(*,*) 'Dqq2K =',Dqq2K(i,j,k,m)
                       call CON_stop('IM: CIMI dies in '//'interpol_D_coefK')
                    endif
                    call lintp(cPA,E_Q2h(iph1,k,:),ipa,a0(m),E_Q2K(i,j,k,m),&
                               'interpol_D_coefK')
                    if (E_Q2K(i,j,k,m).ne.E_Q2K(i,j,k,m).or.& 
                        E_Q2K(i,j,k,m).le.0.) then
                       write(*,*) 'i,j,k,m =',i,j,k,m
                       write(*,*) 'E_Q2K =',E_Q2K(i,j,k,m)
                       write(*,*) 'a0 (=asin(sinA(i,j,:))'
                       write(*,'(10f8.3)') a0
                       write(*,*) 'cPA'
                       write(*,'(10f8.3)') cPA
                       write(*,*) 'E_Q2h (iph1,k,:)'
                       write(*,'(1p9E13.5)') E_Q2h(iph1,k,:)
                       write(*,*) 'E_Q2K (i,j,k,:)'
                       write(*,'(1p9E13.5)') E_Q2K(i,j,k,0:m)
                       call CON_stop('IM: CIMI dies in '//'interpol_D_coefK')
                    endif
                 enddo       ! end of k
             endif ! if (a0(m).ge.cPA(1).and.a0(m).le.cPA(ipa))
           enddo    ! end of m
!(7) assign Dqq1, Dqq2, where <1 deg and >89 deg
           do m=0,m1
              Dqq1K(i,j,:,m)=Dqq1K(i,j,:,m1)
              Dqq2K(i,j,:,m)=Dqq2K(i,j,:,m1)
              E_Q2K(i,j,:,m)=VarQ(1:iq)
           enddo         ! end of m
           do m=m2,ik+1
              Dqq1K(i,j,:,m)=Dqq1K(i,j,:,m2)
              Dqq2K(i,j,:,m)=Dqq2K(i,j,:,m2)
              E_Q2K(i,j,:,m)=E_Q2K(i,j,:,m2)
           enddo         ! end of m
!(8) Calculate jacobian dQ2/dE
           do m=0,ik+1
              do k=2,iq-1
                 dQ2dEK(i,j,k,m)=dQ(k)/(E_Q2K(i,j,k+1,m)-E_Q2K(i,j,k-1,m))
              enddo   ! end of k
              dQ2dEK(i,j,1,0:ik+1)=dQ2dEK(i,j,2,0:ik+1)
              dQ2dEK(i,j,iq,0:ik+1)=dQ2dEK(i,j,iq-1,0:ik+1)
           enddo         ! end of m
        endif
     enddo               ! end of i
  enddo                  ! end of j
  
  end subroutine interpol_D_coefK


!*****************************************************************************
!                           mapPSDtoQ 
!  Routine maps PSD to fixed grids of (K,Q) from (K,V)
!*****************************************************************************
  subroutine mapPSDtoQ
  use ModCimiPlanet, only: ijs=>nspec,&  ! ijs = ns
                           NameSpeciesExtension_I
  use ModCimiGrid, only: ir=>np,ip=>nt,iw=>nm,ik=>nk,&
                         MinLonPar,MaxLonPar
  use ModCimiInitialize, only: xjac
  use CIMI_waves, only: UseChorus, UseHiss,&
                     CHpower,HIpower  
  use ModCimiTrace, only: iba,y=>sinA,ekev
  use ModCimi, only: f2
  !use ModCimi,       ONLY: MinLonPar,MaxLonPar
  !!use diagDiffCoef, only: iq,E_Q2K,PSD_Q,iLpp
  implicit none

  real logEQ,logPSD1,EQ,dE,dE2
  real,dimension(iw) :: ekev0,psd0,logPSD,logEkeV,b,c,d
  integer n,i,j,k,m,nel,k1,k2,kk
  logical DoMapping


!(-1) Find the "n" corresponds to electrons, nel 
  nel=0
  do n=1,ijs
     if (NameSpeciesExtension_I(n).eq.'_e') nel=n
  enddo

!(0) snap a copy of the untransformed PSD to add the difference to later
  !do j=1,ip
  do j=MinLonPar,MaxLonPar
     do i=1,iba(j)
        do m=1,ik
           PSDcopy(i,j,1:iw,m) = f2(nel,i,j,1:iw,m)/xjac(nel,i,1:iw)
        enddo
     enddo
  enddo

  do j=MinLonPar,MaxLonPar
     do i=1,iba(j)
        DoMapping=.False.
        if (UseChorus .and.i.gt.iLpp(j)) then
           DoMapping=.True.
           ! no mapping if CHpower =< 0.
           if (CHpower(i,j).le.0.) DoMapping=.False.
        else if (UseHiss.and.i.le.iLpp(j)) then
           DoMapping=.True.
           ! no mapping if HIpower =< 0.
           if (HIpower(i,j).le.0.) DoMapping=.False.
        endif
        if (DoMapping) then
           do m=1,ik
              ekev0(:)=ekev(nel,i,j,1:iw,m)
              logEkeV(:)=log(ekev0)
              !!psd0(:)=f2(nel,i,j,1:iw,m)/xjac(nel,i,1:iw,m)
              psd0(:)=f2(nel,i,j,1:iw,m)/xjac(nel,i,1:iw)
              where(psd0(:).ge.1.e-50) logPSD(:)=log(psd0)
              where(psd0(:).lt.1.e-50) logPSD(:)=-50.
! !(1) find cubic spline interpolation coefficients.
!               call spline (logEkeV, logPSD, b, c, d, iw, 'mapPSDtoQ')
!               call locate1(E_Q2K(i,j,:,m),iq,ekev0(1 ),k1,'mapPSDtoQ')
!               call locate1(E_Q2K(i,j,:,m),iq,ekev0(iw),k2,'mapPSDtoQ')
!               k1=k1+1
              call locate1(E_Q2K(i,j,:,m),iq,ekev0(1 ),k1,'mapPSDtoQ')
              call locate1(E_Q2K(i,j,:,m),iq,ekev0(iw),k2,'mapPSDtoQ')
              k1=k1+1
              do k=k1,k2
! !(2) map PSD to fixed Q2 grids
                 EQ=E_Q2K(i,j,k,m)
                 logEQ=log(EQ)
                 !call locate1(logEkeV,iw,logEQ,kk,'mapPSDtoQ')
                 !dE=logEQ-logEkeV(kk)
                 !dE2=dE*dE
                 !logPSD1=logPSD(kk)+b(kk)*dE+c(kk)*dE2+d(kk)*dE2*dE
                 !if (logPSD1.lt.min(logPSD(kk),logPSD(kk+1)).or.&
                 !    logPSD1.gt.max(logPSD(kk),logPSD(kk+1))) &
                 !   call lintp(logEkeV,logPSD,iw,logEQ,logPSD1,&
                 !              'mapPSDtoQ')
                 call lintp(logEkeV,logPSD,iw,logEQ,logPSD1,&
                               'mapPSDtoQ')
                 PSD_Q(i,j,k,m)=exp(logPSD1)
              enddo      ! end of k
!(3) fill PSD beyond interpolation ranges
              PSD_Q(i,j,1:k1,m)=PSD_Q(i,j,k1,m)
              PSD_Q(i,j,k2:iq,m)=PSD_Q(i,j,k2,m)
           enddo         ! end of m
        endif            ! end of DoMapping
     enddo               ! end of i
  enddo                  ! end of j
!(4) snap a copy of the transformed PSD to take a difference later
  PSD_Qcopy(:,:,:,:) = PSD_Q(:,:,:,:)

  end subroutine mapPSDtoQ


!*****************************************************************************
!                           mapPSDtoE 
!  Routine maps PSD to fixed grids of (K,V) from (K,Q)
!*****************************************************************************
  subroutine mapPSDtoE
  use ModCimiPlanet, only: ijs=>nspec,&   ! ns=ijs
                           NameSpeciesExtension_I
  use ModCimiGrid, only: ir=>np,ip=>nt,iw=>nm,ik=>nk,&
                         MinLonPar,MaxLonPar
  use ModCimiInitialize, only: xjac
  use CIMI_waves, only: UseChorus, UseHiss,&
                     CHpower,HIpower  
  use ModCimiTrace, only: iba,y=>sinA,ekev
  use ModCimi, only: f2
  !use ModCimi,       ONLY: MinLonPar,MaxLonPar
  !!use diagDiffCoef, only: iq,E_Q2K,PSD_Q,iLpp
  implicit none

  real ekev0,dPSD
  !real,dimension(iq) :: logPSD,logPSD0,EQ,logEQ,b,c,d,dPSD_Q
  !real,dimension(iq) :: logPSD,EQ,logEQ,b,c,d
  real,dimension(iq) :: EQ,dPSD_Q
  integer n,i,j,k,m,nel,kk
  logical DoMapping

!(1) Find the "n" corresponds to electrons, nel 
  nel=0
  do n=1,ijs
     if (NameSpeciesExtension_I(n).eq.'_e') nel=n
  enddo

  do j=MinLonPar,MaxLonPar
     do i=1,iba(j)
        DoMapping=.False.
        if (UseChorus.and.i.gt.iLpp(j)) then
           DoMapping=.True.
           ! no mapping if CHpower =< 0.
           if (CHpower(i,j).le.0.) DoMapping=.False.
        else if (UseHiss.and.i.le.iLpp(j)) then
           DoMapping=.True.
           ! no mapping if HIpower =< 0.
           if (HIpower(i,j).le.0.) DoMapping=.False.
        endif
        if (DoMapping) then
           do m=1,ik
!(1) map PSD to fixed E grids for chorus L>Lpp
              EQ(:)=E_Q2K(i,j,1:iq,m) ! Energy gridpoints in Q1Q2 space
              ! where(EQ(:).ge.1.e-50) logEQ(:)=log(EQ)
              ! where(EQ(:).lt.1.e-50) logEQ(:)=-50.   
              !logPSD(:)=log(PSD_Q(i,j,1:iq,m))
              !logPSD(:)=-50.
              ! where(PSD_Q(i,j,:,m).ge.1.e-50)logPSD(:)=log(PSD_Q(i,j,1:iq,m))
              ! where(PSD_Q(i,j,:,m).lt.1.e-50)logPSD(:)=-50.
              !call spline (logEQ, logPSD, b, c, d, iq, 'mapPSDtoE')
              !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            !Ratio
              ! where(PSD_Qcopy(i,j,:,m).ge.1.e-50)logPSD0(:)=log(PSD_Qcopy(i,j,1:iq,m))
              ! where(PSD_Qcopy(i,j,:,m).lt.1.e-50)logPSD0(:)=-50.
              ! logPSD_Q_ratio(:) = logPSD(:)-logPSD0(:)
              ! call spline (logEQ, logPSD_Q_ratio, b, c, d, iq, 'mapPSDtoE')
              !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

              !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            !Difference
              ! Compute the difference between the PSD since we did diffusion
              dPSD_Q(:) = PSD_Q(i,j,1:iq,m)-PSD_Qcopy(i,j,1:iq,m)
              !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
              do k=2,iw    ! keep PSD(k=1) unchanged during mapping
                 ekev0=ekev(nel,i,j,k,m) ! Energy gridpoints in M K space
                 ! If the M/K energy is within the Q1Q2 energy range
                 if (ekev0.ge.EQ(1).and.ekeV0.le.EQ(iq)) then
                    ! logEkeV=log(ekev0)
                    ! call locate1(EQ,iq,ekev0,kk,'mapPSDtoE')
                    ! dE=logEkeV-logEQ(kk)
                    ! dE2=dE*dE
                    !logPSD1=logPSD(kk)+b(kk)*dE+c(kk)*dE2+d(kk)*dE2*dE
                    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                  !Ratio
                    ! logPSD_ratio1=logPSD_Q_ratio(kk)+b(kk)*dE+c(kk)*dE2+d(kk)*dE2*dE
                    ! If the result would create a new extrema in the PSD curve
                    ! if (logPSD_ratio1.lt.min(logPSD_Q_ratio(kk),logPSD_Q_ratio(kk+1)).or.&
                    !     logPSD_ratio1.gt.max(logPSD_Q_ratio(kk),logPSD_Q_ratio(kk+1))) &
                       ! Instead, linearly interpolate the point
                    ! call lintp(EQ,PSD_Q(i,j,:,m)/PSD_Qcopy(i,j,:,m),iq,ekev0,PSD_ratio1,&
                    !                'mapPSDtoE')
                    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                  !Ddifference
                    ! Interpolate the dPSD height in the M/K coordinates
                    call lintp(EQ,dPSD_Q,iq,ekev0,dPSD,'mapPSDtoE')
                    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                    ! If the result would create a new extrema in the PSD curve
                    ! if (logPSD1.lt.min(logPSD(kk),logPSD(kk+1)).or.&
                    !     logPSD1.gt.max(logPSD(kk),logPSD(kk+1))) &
                    !   ! Instead, linearly interpolate the point
                    !    call lintp(logEQ,logPSD,iq,logEkeV,logPSD1,&
                    !               'mapPSDtoE')

                    !!f2(nel,i,j,k,m)=exp(logPSD1)*xjac(nel,i,k,m)
                    !f2(nel,i,j,k,m)=exp(logPSD1)*xjac(nel,i,k) 
!(1) use the PSD change difference to update f2
                    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                  !Ratio
                    ! f2(nel,i,j,k,m) = PSDcopy(i,j,k,m)*PSD_ratio1*xjac(nel,i,k)
                    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                  !Difference
                    if ((PSDcopy(i,j,k,m)+dPSD).lt.1.e-60) then
                       f2(nel,i,j,k,m) = 1.e-60*xjac(nel,i,k)
                    else
                       f2(nel,i,j,k,m) = (PSDcopy(i,j,k,m)+dPSD)*xjac(nel,i,k)
                    endif
                    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                 endif
              enddo      ! end of k
           enddo         ! end of m
        endif            ! end of DoMapping
     enddo            ! end of i
  enddo               ! end of j
                    
  end subroutine mapPSDtoE


!*****************************************************************************
!                                 rk4    
!  Routine calculates y(t) integrating dy/dt and dt using Runge-Kutta 4th 
!   method
!  
!  y(t+dt) = y(t) + (k1 + 2*k2 + 2*k3 + k4) * dt/6
!  k1=dy/dt(t, y(t))
!  k2=dy/dt(t + 0.5*dt, y(t) + 0.5*dt*k1)
!  k3=dy/dt(t + 0.5*dt, y(t) + 0.5*dt*k2)
!  k4=dy/dt(t + dt, y(t) + dt*k3)
!*****************************************************************************
   subroutine rk4(y0,dt,y,t,dydt,ny,nt,y_out)
   implicit none
   integer,intent(in) :: ny,nt
   real,intent(in) :: y0,dydt(ny,nt),dt(nt),y(ny),t(nt)
   real,intent(out) :: y_out(:)
   integer i,i_1
   real y1,dt1,dt2,k1,k2,k3,k4

   y_out(1)=y0
   do i=2,nt
      i_1=i-1
      y1=y_out(i_1)
      dt1=dt(i_1)
      dt2=0.5*dt1
      call lintp2(y,t,dydt,ny,nt,y1,t(i_1),k1,'rk4')
      call lintp2(y,t,dydt,ny,nt,y1+dt2*k1,t(i_1)+dt2,k2,'rk4')
      call lintp2(y,t,dydt,ny,nt,y1+dt2*k2,t(i_1)+dt2,k3,'rk4')
      call lintp2(y,t,dydt,ny,nt,y1+dt1*k3,t(i_1)+dt1,k4,'rk4')
      y_out(i)=y1+dt1*(k1 + 2.*k2 + 2*k3 + k4)/6.
   enddo

   end subroutine rk4


!*****************************************************************************
!                        write_Q2info
!  Routine prints Q2 grid information.      
!   (1) constant Q1 in (a0,E), setting Q2=E
!   (2) constant Q2 in (a0,E), setting Q1=a0
!   (3) Daa, Dq2,q2
!*****************************************************************************
  subroutine write_Q2info(DEE,DaE,Daa0,DaE0,DEE0,Dqq1,Dqq2,keV,Ompe,&
                          Eq,E,ip,iw,iq,&
                          FileName1,FileName2,FileName3)
  use CIMI_waves, only: ipa,cPA
  !!use diagDiffCoef, only: VarQ
  implicit none

  integer,intent(in) :: ip,iw,iq
  real,intent(in),dimension(ip,iw,ipa) :: DEE,DaE
  real,intent(in),dimension(ip,0:iq+1,ipa) :: Daa0,DaE0,DEE0,E
  real,intent(in),dimension(ip,iq,ipa) :: Dqq1,Dqq2
  real,intent(in) :: keV(iw),Eq(0:iq+1),Ompe(ip)
  character(len=*),intent(in) :: FileName1,FileName2,FileName3
  real a(iw),DaEEE(iw,ipa),dE
  integer j,k,m

!!!! write Dqq
  open(unit=48,file=trim(FileName3))
  write(48,'(f7.0,f7.2)') 10000.,6.5 
  write(48,*) ip,iq,ipa
  write(48,'(10f7.2)') Ompe
  write(48,'(8f12.5)') VarQ(1:iq)  
  do j=1,ip
     do k=1,iq
        write(48,'(f7.2,f12.5,a)') Ompe(j),VarQ(k),'     ! fpe/fce   E(keV)'
        write(48,*) 'alpha0    Daa/p^2     DQQ/Q^2  (sec^-1)'
        do m=1,ipa
           write(48,'(f5.1,1p2E18.5)') cPA(m),Dqq1(j,k,m),Dqq2(j,k,m)
        enddo
     enddo
  enddo
  close(48)
  write(*,*) 'write Dq1q1, Dq2q2'
     
  open(unit=60,file=trim(FileName1))
  write(60,*) ip,iw,ipa
  write(60,'(1p10E11.3)') keV(:)
  do j=1,ip
     where (DEE(j,:,:).ne.0.) DaEEE(:,:)=DaE(j,:,:)/DEE(j,:,:)
     where (DEE(j,:,:).eq.0.) DaEEE(:,:)=0.
     do m=1,ipa
        a(1)=cPA(m)
        do k=2,iw
           dE=(keV(k)-keV(k-1))
           a(k)=a(k-1)+0.5*(DaEEE(k,m)/keV(k)+DaEEE(k-1,m)/keV(k-1))*dE
        enddo
        write(60,'(1p10E12.4)') a(:)
     enddo
  enddo
  close(60)
  open(unit=60,file=trim(FileName2))
  write(60,*) ip,iq,ipa
  write(60,'(1p10E11.3)') Eq(1:iq)
  do j=1,ip
     do k=1,iq
        write(60,'(1p10E12.4)') E(j,k,:)
     enddo
  enddo
  close(60)

  end subroutine write_Q2info


! ************************************************************************
!                        calc_num_ptl          
!
! Subroutine calculates a value proportional to the number of particles
! ************************************************************************
  subroutine calc_num_ptl(f2,x,n)
  implicit none

  integer,intent(in) :: n
  real,intent(in) :: f2(n),x(n)
  real ptl
  integer i

  ptl=0.
  do i=2,n
     ptl=ptl+0.5*(f2(n)+f2(n-1))*(x(n)-x(n-1))
  enddo
  ptl=ptl+f2(1)*x(1)
  write(*,'(a,1pE11.3)') '# particle =',ptl

  end subroutine calc_num_ptl
!
!\
! Below subroutines are copied from CIMI standalone
!/  
!-----------------------------------------------------------------------
  subroutine lintp(xx,yy,n,x,y,NameCaller)
!-----------------------------------------------------------------------
!  Routine does 1-D interpolation.  xx must be increasing or decreasing
!  monotonically. 
   implicit none

   integer,intent(in) :: n
   real,intent(in) :: xx(:),yy(:),x
   character(len=*),intent(in) :: NameCaller
   real,intent(out) :: y
   integer i,jl,ju,jm,j
   real d,x1,minxx,maxxx  

!  Make sure xx is increasing or decreasing monotonically
  do i=2,n
     if (xx(n).gt.xx(1).and.xx(i).lt.xx(i-1)) then
        write(*,*) ' lintp: xx is not increasing monotonically '
        write(*,*) n,(xx(j),j=1,n)
        call CON_stop('IM: CIMI dies in DiagDiff lintp called from'//&
                      NameCaller)
      endif
     if (xx(n).lt.xx(1).and.xx(i).gt.xx(i-1)) then
        write(*,*) ' lintp: xx is not decreasing monotonically '
        write(*,*) n,(xx(j),j=1,n)
        call CON_stop('IM: CIMI dies in DiagDiff lintp called from'//&
                      NameCaller)
      endif
  enddo

!   Make sure x is inside xx range
    minxx=minval(xx)
    maxxx=maxval(xx)
    x1=x
    if (x1.lt.minxx)then
        x1=minxx
    endif
    if (x1.gt.maxxx)then
        x1=maxxx
    endif


! initialize lower and upper values
!
  jl=1
  ju=n
!
! if not dne compute a midpoint
!
10 if(ju-jl.gt.1)then
     jm=(ju+jl)/2
!
!    now replace lower or upper limit
!
     if((xx(n).gt.xx(1)).eqv.(x1.gt.xx(jm)))then
       jl=jm
     else
       ju=jm
     endif
!
!    try again
!
      go to 10
   endif
!
!    this is j
!
   j=jl      ! if x.le.xx(1) then j=1
!              if x.gt.xx(j).and.x.le.xx(j+1) then j=j
!              if x.gt.xx(n) then j=n-1
   d=xx(j+1)-xx(j)
   y=0.5*(yy(j)+yy(j+1))
   if(d.ne.0) y=(yy(j)*(xx(j+1)-x1)+yy(j+1)*(x1-xx(j)))/d

   end subroutine lintp


!-------------------------------------------------------------------------------
   subroutine lintp2(x,y,v,nx,ny,x1,y1,v1,NameCaller)
!-------------------------------------------------------------------------------
!  Routine does 2-D interpolation.  x and y must be increasing or decreasing
!  monotonically
   implicit none
 
   integer,intent(in) :: nx,ny
   real,intent(in) :: x(nx),y(ny),v(nx,ny),x1,y1
   character(len=*),intent(in) :: NameCaller
   real,intent(out) :: v1
 
   real a,b,q00,q01,q10,q11
   integer i,i1,j1,j

   call locate1(x,nx,x1,i,NameCaller//'and lintp2')
   if (i.gt.(nx-1)) i=nx-1      ! extrapolation if out of range
   if (i.lt.1) i=1              ! extrapolation if out of range
   i1=i+1
   a=(x1-x(i))/(x(i1)-x(i))

   call locate1(y,ny,y1,j,NameCaller//'and lintp2')
   if (j.gt.(ny-1)) j=ny-1      ! extrapolation if out of range
   if (j.lt.1) j=1              ! extrapolation if out of range
   j1=j+1
   b=(y1-y(j))/(y(j1)-y(j))

   q00=(1.-a)*(1.-b)
   q01=(1.-a)*b
   q10=a*(1.-b)
   q11=a*b
   v1=q00*v(i,j)+q01*v(i,j1)+q10*v(i1,j)+q11*v(i1,j1)

   end subroutine lintp2

!--------------------------------------------------------------------------
   subroutine locate1(xx,n,x,j,NameCaller)
!--------------------------------------------------------------------------
!  Routine return a value of j such that x is between xx(j) and xx(j+1).
!  xx must be increasing or decreasing monotonically. If not, the locate will
!  stop at the turning point.
!  If xx is increasing:
!     If x=xx(m), j=m-1 so if x=xx(1), j=0  and if x=xx(n), j=n-1
!     If x < xx(1), j=0  and if x > xx(n), j=n
!  If xx is decreasing:
!     If x=xx(m), j=m so if x=xx(1), j=1  and if x=xx(n), j=n
!     If x > xx(1), j=0  and if x < xx(n), j=n
   implicit none

   integer,intent(in) :: n
   real,intent(in) :: xx(:),x
   character(len=*),intent(in) :: NameCaller
   integer,intent(out) :: j

   integer nn,i,jl,ju,jm

!  Make sure xx is increasing or decreasing monotonically
   nn=n
   monoCheck: do i=2,n
      if (xx(n).gt.xx(1).and.xx(i).lt.xx(i-1)) then
         nn=i-1
         exit monoCheck
      endif
      if (xx(n).lt.xx(1).and.xx(i).gt.xx(i-1)) then
         nn=i-1
         exit monoCheck
      endif
   enddo monoCheck
   if (nn.ne.n) then
      write(*,*)'locate1: xx is not increasing or decreasing monotonically'
      call CON_stop('IM: CIMI dies in DiagDiff locate1 called from'//&
                    NameCaller)
   endif

   jl=0
   ju=nn+1
10 if(ju-jl.gt.1)then
     jm=(ju+jl)/2
     if((xx(nn).gt.xx(1)).eqv.(x.gt.xx(jm)))then
       jl=jm
     else
       ju=jm
     endif
   go to 10
   endif
   j=jl

   end subroutine locate1

!\
! Spline interpolation subroutine from open source
!/
   subroutine spline (x, y, b, c, d, n, NameCaller)
!======================================================================
!  Calculate the coefficients b(i), c(i), and d(i), i=1,2,...,n
!  for cubic spline interpolation
!  s(x) = y(i) + b(i)*(x-x(i)) + c(i)*(x-x(i))**2 + d(i)*(x-x(i))**3
!  for  x(i) <= x <= x(i+1)
!  Alex G: January 2010
!----------------------------------------------------------------------
!  input..
!  x = the arrays of data abscissas (in strictly increasing order)
!  y = the arrays of data ordinates
!  n = size of the arrays xi() and yi() (n>=2)
!  output..
!  b, c, d  = arrays of spline coefficients
!  comments ...
!  spline.f90 program is based on fortran version of program spline.f
!  the accompanying function fspline can be used for interpolation
!======================================================================
implicit none
integer,intent(in) :: n
real,intent(in) :: x(n), y(n)
character(len=*),intent(in) :: NameCaller
real,intent(out) :: b(n), c(n), d(n)
integer i, j, gap
real h

gap = n-1
! check input
if (n.le.2) then
   write(*,*) ' In subroutine spline n < 2'
   call CON_stop('IM: CIMI dies in DiagDiff spline called from'//&
                 NameCaller)
endif
if (n.le.3) then
  b(1) = (y(2)-y(1))/(x(2)-x(1))   ! linear interpolation
  c(1) = 0.
  d(1) = 0.
  b(2) = b(1)
  c(2) = 0.
  d(2) = 0.
  return
end if
!
! step 1: preparation
!
d(1) = x(2) - x(1)
c(2) = (y(2) - y(1))/d(1)
do i = 2, gap
  d(i) = x(i+1) - x(i)
  b(i) = 2.*(d(i-1) + d(i))
  c(i+1) = (y(i+1) - y(i))/d(i)
  c(i) = c(i+1) - c(i)
end do
!
! step 2: end conditions 
!
b(1) = -d(1)
b(n) = -d(n-1)
c(1) = 0.
c(n) = 0.
if(n.ne.3) then
  c(1) = c(3)/(x(4)-x(2)) - c(2)/(x(3)-x(1))
  c(n) = c(n-1)/(x(n)-x(n-2)) - c(n-2)/(x(n-1)-x(n-3))
  c(1) = c(1)*d(1)**2/(x(4)-x(1))
  c(n) = -c(n)*d(n-1)**2/(x(n)-x(n-3))
end if
!
! step 3: forward elimination 
!
do i = 2, n
  h = d(i-1)/b(i-1)
  b(i) = b(i) - h*d(i-1)
  c(i) = c(i) - h*c(i-1)
end do
!
! step 4: back substitution
!
c(n) = c(n)/b(n)
do j = 1, gap
  i = n-j
  c(i) = (c(i) - d(i)*c(i+1))/b(i)
end do
!
! step 5: compute spline coefficients
!
b(n) = (y(n) - y(gap))/d(gap) + d(gap)*(c(gap) + 2.0*c(n))
do i = 1, gap
  b(i) = (y(i+1) - y(i))/d(i) - d(i)*(c(i+1) + 2.0*c(i))
  d(i) = (c(i+1) - c(i))/d(i)
  c(i) = 3.*c(i)
end do
c(n) = 3.0*c(n)
d(n) = d(n-1)

end subroutine spline


!*****************************************************************************
!                        init_PSD_for_diff_test
! 
!  Routine initializes PSD for diffusion routine test based on
!   analytical solution.
!*****************************************************************************
  subroutine init_PSD_for_diff_test(iChorMltLoc,iHissMltLoc,D0,xjac)
  use ModNumConst, ONLY: cPi
  use ModCimiPlanet, ONLY: nspec
  use ModCimiGrid, ONLY: nm
  use ModCimi, ONLY: f2
  use ModCimiTrace, ONLY: sinA
  
  implicit none 

  integer,intent(in) :: iChorMltLoc,&  ! MLT index for chorus wave test
                        iHissMltLoc    !   "       "   hiss wave test
  real,intent(in) :: D0,xjac(nspec,ir,nm)
  integer m,&  ! varK index
          n,i,j,k,&
          iChorLatLoc,&
          iHissLatLoc
  real :: c1 = 1.38, &
          c2 = 0.32, &
          c3,c4,&
          sinAc,sinAh,&
          Emin,Emax,E1,Xmin,Xmax,X1,DEE0,&
          E0=510.99895000        ! electron rest mass in keV

  call test_plasmasphere 
  call find_loc_plasmapause

  iChorLatLoc=iLpp(iChorMltLoc)+4
  iHissLatLoc=iLpp(iHissMltLoc)-4
  write(*,*) 'Chorus Lat, MLT =',iChorLatLoc,iChorMltLoc
  write(*,*) 'Hiss Lat, MLT ='  ,iHissLatLoc,iHissMltLoc

  if (UsePitchAngleDiffusionTest) then
     if (.not.allocated(PSD_CHTest)) &
        allocate(PSD_CHTest(ik))
     if (.not.allocated(PSD_HITest)) &
        allocate(PSD_HITest(ik))
     ! initial analytic solution
     !!c3 = 0.5*cPi/(c1-22./15.*c2)
     c3 = 0.549048316*cPi
     c4 = c3*c3
     do m=1,ik 
        sinAc=sinA(iChorLatLoc,iChorMltLoc,m)
        sinAh=sinA(iHissLatLoc,iHissMltLoc,m)

        PSD_CHTest(m)=sin(c3*(c1*sinAc**2 &
                      - 0.666666667*c2*sinAc**3 &
                      - 0.8        *c2*sinAc**2.5))
        PSD_HITest(m)=sin(c3*(c1*sinAh**2 &
                      - 0.666666667*c2*sinAh**3 &
                      - 0.8        *c2*sinAh**2.5))
     enddo
     cLambda2D=c4*D0
     ! set PSD_Q
     do j=1,ip
        do m=1,ik
           do i=1,iLpp(j)
              PSD_Q(i,j,:,m)=PSD_HITest(m)
           enddo
           do i=iLpp(j)+1,ir
              PSD_Q(i,j,:,m)=PSD_CHTest(m)
           enddo
        enddo
     enddo
     ! set f2
     do m=1,ik
        do k=1,nm
           do j=1,ip
              do n=1,nspec
                 do i=1,iLpp(j)
                    f2(n,i,j,k,m)=xjac(n,i,k)*PSD_HITest(m)
                 enddo
                 do i=iLpp(j)+1,ir
                    f2(n,i,j,k,m)=xjac(n,i,k)*PSD_CHTest(m)
                 enddo
              enddo
           enddo
        enddo
     enddo
  endif

  if (UseEnergyDiffusionTest) then
     if (.not.allocated(PSD_CHTest)) &
        allocate(PSD_CHTest(iq))
     if (.not.allocated(PSD_HITest)) &
        allocate(PSD_HITest(iq))
     DEE0=D0*1.e7
     Emin=VarQ(1)/E0
     Emax=VarQ(iq)/E0
     Xmin=(Emin*(Emin+2.))**1.5/3.
     Xmax=(Emax*(Emax+2.))**1.5/3.
     c3 = 0.5*cPi/(Xmax-Xmin)
     c4 = c3*c3
     cLambda2D=c4*DEE0
     do k=1,iq
        E1=VarQ(k)/E0
        X1=(E1*(E1+2.))**1.5/3.
        PSD_CHTest(k)=cos(c3*(X1-Xmin))
        PSD_HITest(k)=PSD_CHTest(k)
        PSD_Q(:,:,k,:)=PSD_CHTest(k)
     enddo 
  endif

  end subroutine init_PSD_for_diff_test


!*****************************************************************************
!                        calc_Dcoef_for_diff_test
! 
!  This subroutine calculates Daa and overwrites chorus and hiss wave power
!   analytical solution.
!*****************************************************************************
  subroutine calc_Dcoef_for_diff_test(iChorMltLoc,iHissMltLoc,D0)
  use ModNumConst,    ONLY: cDegToRad
  use CIMI_waves, ONLY: cDaa,cDaE,cDEE,&
                         hDaa,hDaE,hDEE,&
                         CHpower,HIpower,&
                         Cpower0,Hpower0,BLc0,BLh0,&
                         cPA,ipa
  
  use ModCimiTrace, ONLY: bo,sinA,tya
  implicit none

  integer,intent(in) :: iChorMltLoc,iHissMltLoc
  real,intent(in) :: D0

  integer m,&  ! varK index
          k    ! varQ index
  integer iChorLatLoc  ,&
          iHissLatLoc
  real :: c1 = 1.38, &
          c2 = 0.32, &
          E0=510.99895000       ! electron rest mass in keV
  real y,sin2A,Daa0,Daa,cPo,hPo,&
       E1,DEE0,DEE,dXdE

  ! set all diff coef. and wave power zero
  Dqq1K(:,:,:,:)=0.
  Dqq2K(:,:,:,:)=0.
 
  ! set plasmasphere density and location 
  call test_plasmasphere  
  call find_loc_plasmapause

  ! set the area of interest for chorus and hiss    
  iChorLatLoc=iLpp(iChorMltLoc)+4
  iHissLatLoc=iLpp(iHissMltLoc)-4

  ! set Po=(BLc0/bo(i,j))*CHpower(i,j)/Cpower0=1 
  CHpower(:,:)=0.   ! except
  CHpower(iChorLatLoc,iChorMltLoc)=Cpower0 
  
  ! set Po=(BLh0/bo(i,j))*HIpower(i,j)/Hpower0=1 
  HIpower(:,:)=0.   ! except 
  HIpower(iHissLatLoc,iHissMltLoc)=Hpower0 
   

  cPo=(BLc0/bo(iChorLatLoc,iChorMltLoc))&
      *CHpower(iChorLatLoc,iChorMltLoc)/Cpower0
  hPo=(BLh0/bo(iHissLatLoc,iHissMltLoc))&
      *HIpower(iHissLatLoc,iHissMltLoc)/Hpower0
  if (UsePitchAngleDiffusionTest) then
     do m=0,ik+1
        ! chorus Daa
        y=sinA(iChorLatLoc,iChorMltLoc,m)
        sin2A=2.*y*sqrt(1.-y**2)
        Daa0=D0/((c1 - c2*(y+sqrt(y)))*sin2A)**2
        Daa=Daa0/cPo
        Dqq1K(iChorLatLoc,iChorMltLoc,:,m)=Daa
        ! hiss Daa
        y=sinA(iHissLatLoc,iHissMltLoc,m)
        sin2A=2.*y*sqrt(1.-y**2)
        Daa0=D0/((c1 - c2*(y+sqrt(y)))*sin2A)**2
        Daa=Daa0/hPo
        Dqq1K(iHissLatLoc,iHissMltLoc,:,m)=Daa
     enddo
  endif

  if (UseEnergyDiffusionTest) then
     DEE0=D0*1.e7
     do k=1,iq
        E1=VarQ(k)/E0
        dXdE=(E1+1.)*sqrt(E1*(E1+2.))
        DEE=DEE0/dXdE/dXdE/E1/E1       ! DEE/E^2
        Dqq2K(iChorLatLoc,iChorMltLoc,k,:)=DEE/cPo
        Dqq2K(iHissLatLoc,iHissMltLoc,k,:)=DEE/hPo
     enddo
  endif 

! recalculate tya to approximated solution in a dipole
  tya(:,:,:)=1.38 - 0.32 *(sinA(:,:,:) + sqrt(sinA(:,:,:)))

! const. Q2 curve
  do k=0,iq+1
     E_Q2c(:,k,:)=VarQ(k)
     E_Q2h(:,k,:)=VarQ(k)
  enddo

! dQ2/dE 
  dQ2dEK(:,:,:,:)=1. 

  end subroutine calc_Dcoef_for_diff_test


!*****************************************************************************
!                        write_PSD_for_diff_test
! 
!  Routine initializes PSD for diffusion routine test based on
!   analytical solution.
!*****************************************************************************
  subroutine write_PSD_for_diff_test(Time,DtPSDTest,&
                                     iChorMltLoc,iHissMltLoc)
  use ModImTime, ONLY: TimeMax
  implicit none
  
  integer,intent(in) :: iChorMltLoc,iHissMltLoc
  real,intent(in) :: Time,&
                     DtPSDTest
  integer nstep         ,&
          iChorLatLoc   ,&
          iHissLatLoc

  nstep=int(TimeMax/DtPSDTest)+1

  ! set the area of interest for chorus and hiss    
  iChorLatLoc=iLpp(iChorMltLoc)+4
  iHissLatLoc=iLpp(iHissMltLoc)-4

  if (Time.eq.0.) then
     open(unit=60,file='IM/plots/PSD_chorus_test.dat')
     open(unit=62,file='IM/plots/PSD_hiss_test.dat')
     if (UsePitchAngleDiffusionTest) then
        write(60,*) nstep,ik
        write(62,*) nstep,ik
     endif
     if (UseEnergyDiffusionTest) then
        write(60,*) nstep,iq
        write(62,*) nstep,iq
     endif
  else   
     open(unit=60,file='IM/plots/PSD_chorus_test.dat',&
          status='old',position='append')
     open(unit=62,file='IM/plots/PSD_hiss_test.dat',&
          status='old',position='append')
  endif 
  if (UsePitchAngleDiffusionTest) then
     write(60,'(1p8E13.5)') PSD_CHTest(1:ik)/exp(cLambda2D*Time)
     write(60,'(1p8E13.5)') PSD_Q(iChorLatLoc,iChorMltLoc,1,1:ik) 
     write(62,'(1p8E13.5)') PSD_HITest(1:ik)/exp(cLambda2D*Time) 
     write(62,'(1p8E13.5)') PSD_Q(iHissLatLoc,iHissMltLoc,1,1:ik) 
  endif
  if (UseEnergyDiffusionTest) then
     write(60,'(1p9E13.5)') PSD_CHTest(1:iq)/exp(cLambda2D*Time)
     write(60,'(1p9E13.5)') PSD_Q(iChorLatLoc,iChorMltLoc,1:iq,1) 
     write(62,'(1p9E13.5)') PSD_HITest(1:iq)/exp(cLambda2D*Time) 
     write(62,'(1p9E13.5)') PSD_Q(iHissLatLoc,iHissMltLoc,1:iq,1) 
  endif
  close(60)
  close(62)

  end subroutine write_PSD_for_diff_test

  !=============================================================================
  !subroutine to fill plasmasphere for testing
  subroutine test_plasmasphere
    use ModCimiGrid,    ONLY: MinLonPar, MaxLonPar,ir=>np, ip=>nt
    use ModCimiTrace,   ONLY: ro, iba
    use ModPlasmasphere, ONLY:PlasDensity_C
    integer :: i,j
    !--------------------------------------------------------------------------
    allocate(PlasDensity_C(ir,ip))
    do j = MinLonPar, MaxLonPar
       do i = 1, iba(j)
          if (ro(i,j) <5.0) then
             PlasDensity_C(i,j) = 1000.0
          else
             PlasDensity_C(i,j) = 1.0
          endif
       end do
    end do
          
  end subroutine test_plasmasphere
  
  end module
