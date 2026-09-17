!modules from standalone cimi as reference
!module constants
!  integer,parameter :: i_zero=0,i_one=1,m_one=-1
!  real,parameter :: pi=3.14159265358979
!  real,parameter :: re_m=6.3712e6	            ! earth's average radius (m)
!  real,parameter :: xmp=1.673e-27               ! mass of H+ in kg
!  real,parameter :: e_mass=9.11e-31             ! electron mass in kg
!  real,parameter :: echarge=1.6e-19	            ! electron charge
!  real,parameter :: EM_speed=2.998e8            ! speed of light (m/s)
!  real,parameter :: epsilon0=8.8542e-12         ! permittivity of free space
!end module constants
!
!
!module cWpower
!  use cimigrid_dim
!  real,parameter::densityP=4.e7   ! density to define plasmapause
!  real rppa(ip)                   ! plasmapause radial distance
!  real CHpower(ir,ip),HIpower(ir,ip),ompe(ir,ip),r_wave
!  real Cpower0,Hpower0,BLc0,BLh0
!end module cWpower

Module CIMI_waves
  !these data used to be taken from the constants module of the standalone cimi
  use ModConst, only: cEps,cLightSpeed, cElectronCharge, cElectronMass, cProtonMAss
  use ModNumConst,      ONLY: cPi
  use ModPlanetConst,	ONLY: Earth_, rPlanet_I
  use ModUtilities,     ONLY: CON_stop
  implicit none
  private !except

  !arrays for the wave power copy and paste directly from cWpower in
  !standalone code
  real,public, allocatable :: CHpower(:,:),HIpower(:,:),ompe(:,:),r_wave
  real,public  ::  Cpower0,Hpower0,BLc0,BLh0
  

  !copy and paste directly from waveDiffCoef in standalone code
  real,allocatable,public,dimension(:) :: cOmpe,hOmpe,ckeV,hkeV
  !chorus diffusion coef
  real,public,allocatable,dimension(:,:,:) :: cDaa,cDEE,cDaE!cDpp,cDap,hDaa,hDpp,hDap
  !hiss diffusion coef
  real,public,allocatable,dimension(:,:,:) :: hDaa,hDEE,hDaE
  
  !The type of wave model. In the standalone code this was ichor, ihiss
  character(len=100) :: TypeHiss, TypeChorus

  !Name of input wave files
  character(len=100),public :: NameHissFile, NameChorusFile

  !flags that indicate if we are using waves and what kind
  logical, public :: UseWaves=.false., UseChorus, UseHiss

  ! dimension of LB chorus & hiss diff coef copied from cimigrid_dim module of
  ! standalone code
  ! dimension of LB chorus & hiss diff coef
  integer,public :: ipc,iwc,iph,iwh
  integer, public,parameter :: ipa=89

  real,public :: cPA(ipa)
  
  ! option to use KpIndex instead of AE. This was origional wave approach
  logical, public :: UseKpIndex

  public :: readDiffCoef
  public :: WavePower
contains

  !*****************************************************************************
  !                           readDiffCoef
  !  Routine reads chorus and hiss diffusion coeff.
  !  directly adapted from standalone code
  !*****************************************************************************
  subroutine readDiffCoef(ir,ip)
    
    implicit none
    integer, intent(in)::ir,ip
    integer :: m
    
    !allocate arrays 
    allocate(CHpower(ir,ip),HIpower(ir,ip),ompe(ir,ip))

    !set the filetype for hiss and chorus based on the file name
    if (UseHiss) then
       select case (NameHissFile)
       case('D_hiss_UCLA.dat')
          TypeHiss='UCLA'
       !case('D_hiss_Albert.dat')
       !   TypeHiss='Albert'
       case default
          call CON_stop('IM ERROR: Hiss wave file type not recognized.')
       end select
    endif

    if (UseChorus) then
       select case (NameChorusFile)
       case('D_LBchorus_QZ.dat','D_LBchorus_merge2.dat',&
            'D_LBchorus_merge3.dat')
          TypeChorus='ChorusFileType1'
       !case('D_LBchorus_merge.dat')
       !   TypeChorus='ChorusFileType2'
       case default
          call CON_stop('IM ERROR: Chorus wave file type not recognized.')
       end select
    endif

    
    ! pitch-angle grid
    do m=1,ipa
       cPA(m)=real(m) !m*1.
    enddo

    !read chorus data
    if (UseChorus) then
       call read_chorus
    endif

    if (UseHiss) then
       call read_hiss
    endif
  end subroutine readDiffCoef

  !***************************************************************************
  subroutine read_chorus
    use ModIoUnit, ONLY: UnitTmp_
    use ModCimiPlanet, only: xme => dipmom
    !index vars
    integer ::  j,k,m,n

    ! to hold record diffusion coef from file
    real:: pa,daa,dap,dpp,daE,dEE
    
    !reference L location for waves
    real :: Lc0 !
  
    character header*80
    !---------------------------------------------------------------------------
    
    open(unit=UnitTmp_,file='IM/'//NameChorusFile,status='old')
    !write(*,*) 'IM: Using Chorus wave file: ', NameChorusFile
    read(UnitTmp_,*) Cpower0,Lc0
    read(UnitTmp_,*) ipc,iwc
    !note some additional variables will be needed when we go to p-k system
    !those are commented out for now
    allocate (cOmpe(ipc),ckeV(iwc))!,ckeVlog(iwc))
    !allocate (wDaa(iwc,ipa),wDap(iwc,ipa),wDpp(iwc,ipa))
    !allocate (Daa1(iwc),Dap1(iwc),Dpp1(iwc))
    !allocate (cDaa(ipc,0:iw+1,ipa),cDap(ipc,0:iw+1,ipa),cDpp(ipc,0:iw+1,ipa))
    allocate (cDEE(ipc,iwc,ipa),cDaa(ipc,iwc,ipa),cDaE(ipc,iwc,ipa))
    read(UnitTmp_,*) cOmpe
    read(UnitTmp_,*) ckeV
    !ckeVlog(:)=log10(ckeV(:))
    BLc0=xme/(Lc0*rPlanet_I(Earth_))**3       ! dipole B at Lc0
    cDaa(:,:,:)=0.
    cDaE(:,:,:)=0.
    cDEE(:,:,:)=0.
    do j=1,ipc
       ! Read coefficients
       do k=1,iwc
          read(UnitTmp_,'(a80)') header
          read(UnitTmp_,'(a80)') header
          do m=1,ipa
             read(UnitTmp_,*) pa,daa,dap,dpp,daE,dEE
             cDaa(j,k,m) = daa    ! coeff in 1/sec
             cDaE(j,k,m) = daE
             cDEE(j,k,m) = dEE
          enddo
       enddo
    enddo
    close(UnitTmp_)
    
  end subroutine read_chorus
  
  !***************************************************************************
  subroutine read_hiss
    use ModIoUnit, ONLY: UnitTmp_
    use ModCimiPlanet, only: xme => dipmom
    !index vars
    integer ::  j,k,m,n

    ! to hold record diffusion coef from file
    real:: pa,daa,dap,dpp,daE,dEE
    
    !reference L location for waves
    real :: Lh0 , EEo
  
    character header*80
    !---------------------------------------------------------------------------
    
    open(unit=UnitTmp_,file='IM/'//NameHissFile,status='old')
    !write(*,*) 'IM: Using Hiss wave file: ', NameHissFile
    read(UnitTmp_,*) Hpower0,Lh0
    read(UnitTmp_,*) iph,iwh
    !note some additional variables will be needed when we go to p-k system
    !those are commented out for now
    allocate (hOmpe(iph),hkeV(iwh))!,ckeVlog(iwc))
    !allocate (wDaa(iwc,ipa),wDap(iwc,ipa),wDpp(iwc,ipa))
    !allocate (Daa1(iwc),Dap1(iwc),Dpp1(iwc))
    !allocate (cDaa(ipc,0:iw+1,ipa),cDap(ipc,0:iw+1,ipa),cDpp(ipc,0:iw+1,ipa))
    allocate (hDEE(iph,iwh,ipa),hDaa(iph,iwh,ipa),hDaE(iph,iwh,ipa))
    read(UnitTmp_,*) hOmpe
    read(UnitTmp_,*) hkeV
    !ckeVlog(:)=log10(ckeV(:))
    BLh0=xme/(Lh0*rPlanet_I(Earth_))**3       ! dipole B at Lc0
    hDaa(:,:,:)=0.
    hDaE(:,:,:)=0.
    hDEE(:,:,:)=0.
    do j=1,iph
       ! Read coefficients
       read(UnitTmp_,'(a80)') header
       read(UnitTmp_,'(a80)') header
       do k=1,iwh
          do m=1,ipa
             read(UnitTmp_,*) Lh0,EEo,pa,daa,dap,dpp,daE,dEE
             hDaa(j,k,m) = daa    ! coeff in 1/sec
             hDaE(j,k,m) = daE
             hDEE(j,k,m) = dEE
          enddo
       enddo
    enddo
    close(UnitTmp_)
    
  end subroutine read_hiss
      
  
  !****************************************************************************
  subroutine WavePower(ir,ip,t,density,AE,Kp,Bo,ro,xmlto,iba,MinLonPar,MaxLonPar)
    ! Routine determines Chorus and hiss wave power (pT)^2 and fpe/fce as a
    ! function of location
    
    implicit none
    integer, intent(in)::ir,ip,MinLonPar, MaxLonPar
    integer,intent(in) :: iba(ip)
    real,   intent(in) :: t, AE, Kp,ro(ir,ip), density(ir,ip), Bo(ir,ip), &
         xmlto(ir,ip)
    real :: ro1,xmlt1,r_hiss, AE_chorus, AE_hiss
    integer :: iae,jae,i,j
    
    r_wave=2.0               ! lower boundary in RE of wave diffusion
    r_hiss=8.                ! upper boundary in RE of hiss
    
    
    if (UseKpIndex) then
       if (Kp.lt.2.) then
          iae=1
          jae=1
          AE_chorus = 0
          AE_hiss = 0
       endif
       if (Kp.ge.2..and.Kp.lt.4.) then
          iae=2
          jae=2
          AE_chorus = 100
          AE_hiss = 100
       endif
       if (Kp.ge.4.) then
          iae=3
          jae=3
          AE_chorus = 300
          AE_hiss = 500
       endif
    else
       ! Determine AE level in chorus wave power data provided by Meredith
       if (AE.lt.100.) iae=1
       if (AE.ge.100..and.AE.lt.300.) iae=2
       if (AE.ge.300.) iae=3
       
       ! Determine AE level in hiss wave power data provided by Meredith
       if (AE.lt.100.) jae=1
       if (AE.ge.100..and.AE.lt.500.) jae=2
       if (AE.ge.500.) jae=3
       AE_chorus = AE
       AE_hiss = AE
    endif
    
    ! Determine wave power (in pT^2), and ompe (fpe/fce)
    CHpower=0.
    HIpower=0.
    ompe=0.            ! initial values
    do j=MinLonPar,MaxLonPar
       do i=1,iba(j)
          if (density(i,j).eq.0.) then
             write(*,*) 'Error: density(i,j).eq.0, t,iba(j),i,j ',t,iba(j),i,j
             stop
          endif
          !write(*,*)'i,j,density(i,j),bo(i,j)',i,j,density(i,j),bo(i,j)
          ompe(i,j)=sqrt(density(i,j)*cElectronMass/cEps)/bo(i,j)    ! fpe/fce
          ro1=ro(i,j)
          xmlt1=xmlto(i,j)
          if (xmlt1.lt.0.) xmlt1=xmlt1+24.
          if (UseChorus .and.ro1.ge.r_wave) call ChorusBpower(ro1,xmlt1,iae, &
               CHpower(i,j), AE_chorus)
          if (UseHiss.and.ro1.ge.r_wave.and.ro1.le.r_hiss) &
               call HissBpower(ro1,xmlt1,jae,HIpower(i,j), AE_hiss)
       enddo
    enddo
    
  end subroutine WavePower
  
  
  !*****************************************************************************
  subroutine ChorusBpower(ro,xmlt,iae,Bpower, AE)
    ! Routine calculates the lower band Chorus power in (pT)^2 by Gaussian fits
    ! provided by Qiuhua.
    
    implicit none

    real,    intent(in) :: ro,xmlt, AE
    integer, intent(in) :: iae
    real   , intent(out):: Bpower
    
    real    :: xxbar,yybar,pi2,r12,sigx1,sigy1,qq,amp_fit, r_xy, x_bar, y_bar,&
               sig_x, sig_y
    real    :: Amp(3),xbar(3),sigx(3),ybar(3),sigy(3),rxy(3)
    !--------------------------------------------------------------------------
    data Amp/18951.,54237.,111104./  
    data xbar/7.46,7.82,5.20/        ! mlt of peak power
    data ybar/6.44,6.37,5.70/        ! ro of peak power
    data sigx/5.50,4.87,4.64/
    data sigy/1.30,1.68,1.77/
    data rxy/0.,-0.1,0./

    if (AE < 300) then
       amp_fit = 23245.615 * sqrt(0.249696 * AE + 55.73685) - 154594.057
       r_xy = 0.000005 * (AE ** 2) - (0.0015 * AE)
       x_bar = -0.0000556667 * (AE ** 2) + (0.00916667 * AE) + 7.46
       y_bar = 2.9966 + (6.44-2.9966) / (1 + (AE/520.915) ** 2.3479)
    else
       amp_fit = 111104.
       r_xy = 0.0
       x_bar = 5.20
       y_bar = 5.70
    end if
    if (AE < 1) then
       sig_x = 5.50734
       sig_y = 1.30006
    else
       sig_x = -0.14667 * log(AE) + 5.50734
       sig_y = 0.0824351 * log(AE) + 1.30006
    end if
    
    pi2=2.*cPi        
    r12=1./sqrt(1.-r_xy*r_xy)
    sigx1=sig_x
    sigy1=sig_y
    
    xxbar=abs(xmlt-x_bar)
    if (xxbar.gt.12.) xxbar=24.-xxbar
    yybar=abs(ro-y_bar)
    qq=xxbar**2/sigx1**2+yybar**2/sigy1**2-2.*r_xy*xxbar*yybar/sigx1/sigy1
    Bpower=amp_fit*r12/pi2/sigx1/sigy1/exp(0.5*r12*qq)

    !kludge
    !Bpower=0.0
    !Bpower=1.0e-40
    
  end subroutine ChorusBpower

  
  !*****************************************************************************
  subroutine HissBpower(ro,xmlt,iae,Bpower, AE)
    ! Routine calculates the CRRES hiss power in (pT)^2 by Gaussian fits
    ! provided by Qiuhua.
    
    implicit none
    
    real,    intent(in) :: ro,xmlt, AE
    integer, intent(in) :: iae
    real   , intent(out):: Bpower
    
    real xxbar,yybar,pi2,r12,sigx1,sigy1,qq, amp_fit,r_xy, x_bar, y_bar,&
               sig_x, sig_y
    real Amp(3),xbar(3),sigx(3),ybar(3),sigy(3),rxy(3)
    
    data Amp/37964.,71459.,130742./          
    data xbar/15.20,11.82,14.46/
    data ybar/3.0,3.25,3.55/
    data sigx/6.08,4.87,5.64/
    data sigy/0.81,1.31,1.32/
    data rxy/0.,0.,0./

    if (AE < 500) then
       amp_fit = 899.89849 * sqrt(30.45545 * AE + 497.35063) + 17895.0403
       r_xy = 0.0
       x_bar = 0.0000808 * (AE ** 2) - (0.04188 * AE) + 15.20
       y_bar = 0.00793319 * sqrt(9.60314 * AE) + 3.00148
       sig_x = 0.00002805 * (AE ** 2) - (0.014905 * AE) + 6.08
       sig_y = -0.00000995 * (AE ** 2) + (0.005995 * AE) + 0.81
    else
       amp_fit = 130742.
       r_xy = 0.0
       x_bar = 14.46
       sig_x = 5.64
       sig_y = 1.32
    end if
    

    pi2=2.*cPi          
    r12=1./sqrt(1.-r_xy*r_xy)
    sigx1=sig_x
    sigy1=sig_y
    
    xxbar=abs(xmlt-x_bar)
    if (xxbar.gt.12.) xxbar=24.-xxbar 
    yybar=abs(ro-y_bar)
    qq=xxbar**2/sigx1**2+yybar**2/sigy1**2-2.*r_xy*xxbar*yybar/sigx1/sigy1
    Bpower=amp_fit*r12/pi2/sigx1/sigy1/exp(0.5*r12*qq)


    !kludge
    !Bpower=0.0
    !Bpower=1.0e-40

  end subroutine HissBpower
  
  !note that other routines like diffuse_aa and diffuse_ee can be added later
  !from the standalone code
end Module CIMI_waves







  
