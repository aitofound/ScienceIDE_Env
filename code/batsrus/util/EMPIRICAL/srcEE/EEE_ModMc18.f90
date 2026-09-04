!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module EEE_ModMc18

  ! Rosenbluth & Bussac 1979 force-free spheromak model
  ! with image dipoles (Lin 2006) and 
  ! uniform field subtraction (Borovikov et al 2018).

#ifdef _OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use EEE_ModCommonVariables, HalfOpeningAngle=>OrientationCme

  implicit none

  SAVE

  private ! except

  public :: set_parameters_mc18
  public :: get_mc18_fluxrope
  public :: get_mc18_size
  public :: mc18_init

  ! Local variables

  ! Geometric characteristics of the superimposed configuration:

  ! distance from the magnetic configuration center to heliocenter
  real :: rDistance1 = 0.0

  ! Radius of the magnetic configuration (spheromak)
  real :: Radius = 0.0
  !$acc declare create(Radius)

  ! Height of the configuration base above the solar surface [rSun]:
  !   1 + BaseHeight = rDistance1 - Radius
  ! Specifying BaseHeight is more straightforward than specifying rDistance1.
  real :: BaseHeight = 0.0

  ! The derivative of current over flux function
  !(\mu_0)dI/d\psi) has the dimentions of inverse length
  real :: Alpha0
  !$acc declare create(Alpha0)

  ! Characteristic value of magnetic field of the spheromak
  ! configuration: the field in the center of configuration
  ! equals 2(1/3 -\beta_0)B_0 \approx 0.7 B_0:
  real :: B0      ! dimensionless
  real :: B0Dim   ! in Gauss
  !$acc declare create(B0)

  ! Sign of alpha: chosen so the toroidal field at the spheromak bottom
  ! opposes the ambient field component perpendicular to the
  ! plane spanned by DirCme_D and bConf_D.
  real :: iHelicity = 1.0
  !$acc declare create(iHelicity)

  ! Dimensionless product of R0 by Alpha0.
  ! Boundary condition: j1(Alpha0R0)/Alpha0R0 = Beta0
  ! For Beta0=0: Alpha0R0 is the first zero of j1 (~4.4934).
  ! For Beta0>0: solved iteratively in find_alpha0r0.
  ! (GL98 used 5.763854, the first zero of j_2)
  real :: Alpha0R0 = 4.493409457909064
  !$acc declare create(Alpha0R0)

  ! Vector characteristic of the configuration: radius vector of the
  ! configuration center and B0 multiplied by unit vector along
  ! the axis of symmetry
  real :: XyzCenterConf_D(3), bConf_D(3)
  !$acc declare create(XyzCenterConf_D, bConf_D)

  ! Normalized ambient field at the spheromak center: bAmbientCenterSi_D in
  ! code units.  Used for the Borovikov et al. (2018) uniform field subtraction:
  ! interior perturbation = B_Bessel - bAmbientConf_D,
  ! exterior dipole moment = -bAmbientConf_D * Radius^3/2.
  real :: bAmbientConf_D(3) = 0.0
  !$acc declare create(bAmbientConf_D)

  ! Parameter to control self-similar solution
  real :: uCmeSi = 0.0
  !$acc declare create(uCmeSi)
  real, parameter :: Delta = 0.1

  ! Lin (2006) image dipole parameters
  logical :: UseImageDipoles = .false.
  !$acc declare create(UseImageDipoles)
  integer :: nDiscDipoles = 200

  ! Pre-computed image dipoles: 
  ! 1 point image + up to MaxImgDipoles-1 line images
  integer, parameter :: MaxImgDipoles = 1001
  integer :: nImgDipoles = 0
  !$acc declare create(nImgDipoles)
  real :: rImgDipole_DI(3, MaxImgDipoles) = 0.0
  real :: mImgDipole_DI(3, MaxImgDipoles) = 0.0
  !$acc declare create(rImgDipole_DI, mImgDipole_DI)

  ! Spheromak Beta0 and ejecta temperature (same convention as TD99)
  ! Boundary parameter beta0: j1(alpha0*r0)/(alpha0*r0) = beta0.
  ! Pressure from PDF Eq.(5): p = [j1/(a0 r) - b0]*b0*alpha0^2*(r x B0)^2
  logical :: UseBeta0 = .false.
  !$acc declare create(UseBeta0)
  real :: Beta0 = 0.0
  real :: EjectaTemperature = 0.0           ! normalized code units
  real :: EjectaTemperatureDim = 5.0e4      ! [K]
  !$acc declare create(Beta0, EjectaTemperature, EjectaTemperatureDim)

contains
  !============================================================================
  subroutine mc18_init

    use ModCoordTransform, ONLY: cross_product
    !--------------------------------------------------------------------------
    ! Solve boundary condition j1(Alpha0R0)/Alpha0R0 = Beta0.
    ! For force-free (UseBeta0=F), Beta0=0 and Alpha0R0 is
    ! the first zero of j1 (~4.4934).
    Alpha0R0 = find_alpha0r0(merge(Beta0, 0.0, UseBeta0))

    ! Wave number k = Alpha0R0 / r0
    Alpha0 = Alpha0R0/Radius

    ! Field amplitude and axis from the ambient field at the CME center.
    ! bAmbientCenterSi_D is filled by the MHD solver before mc18_init is called
    ! (see SC_user_initial_perturbation in ModUserAwsom.f90).
    bConf_D        = (-3.0/(2.0*spher_bessel2(Alpha0R0))) * bAmbientCenterSi_D*Si2No_V(UnitB_)
    B0             = norm2(bConf_D)
    B0Dim          = B0*No2Io_V(UnitB_)
    bAmbientConf_D = bAmbientCenterSi_D*Si2No_V(UnitB_)

    ! Helicity: choose sign of alpha so that the toroidal field at the
    ! spheromak bottom opposes the component of the ambient field there
    ! perpendicular to the plane(DirCme_D, bConf_D).
    ! Derivation: b_tor_bot ~ -iHelicity*(DirCme_D x bConf_D), so
    ! iHelicity = sign(bAmbientBottomSi_D · (DirCme_D x bConf_D)).
    iHelicity = sign(1.0, sum(bAmbientBottomSi_D &
         *cross_product(DirCme_D, bConf_D)))

    ! Convert self-similar CME speed from km/s to SI
    uCmeSi = uCmeSi*Io2Si_V(UnitU_)

    if(iProc==0)then
       write(*,*) prefix
       write(*,*) prefix, &
            '>>>>>>>>>>>>>>>>>>>                            '//&
            '<<<<<<<<<<<<<<<<<<<<<'
       write(*,*) prefix
       write(*,*) prefix, &
            '     EEGMC Magnetic Cone Model'//&
            ' (Rosenbluth-Bussac 1979) is initiated'
       write(*,*) prefix
       write(*,*) prefix, &
            '>>>>>>>>>>>>>>>>>>>                            '//&
            '<<<<<<<<<<<<<<<<<<<<<'
       write(*,*) prefix
       write(*,*) prefix, 'B0Dim          = ', B0Dim,               '[Gauss]'
       write(*,*) prefix, 'iHelicity      = ', iHelicity
       write(*,*) prefix, 'Radius         = ', Radius,              '[rSun]'
       write(*,*) prefix, 'HalfOpeningAngle= ', HalfOpeningAngle,   '[degrees]'
       write(*,*) prefix, 'rDistance1     = ', rDistance1,          '[rSun]'
       write(*,*) prefix, 'BaseHeight     = ', BaseHeight,          '[rSun]'
       write(*,*) prefix, 'LongitudeCme   = ', LongitudeCme,        '[degrees]'
       write(*,*) prefix, 'LatitudeCme    = ', LatitudeCme,         '[degrees]'
       write(*,*) prefix, 'Alpha0         = ', Alpha0,              '[1/rSun]'
       write(*,*) prefix, 'UseImageDipoles= ', UseImageDipoles
       if(UseImageDipoles) &
            write(*,*) prefix, 'nDiscDipoles   = ', nDiscDipoles
       write(*,*) prefix, 'UseBeta0  = ', UseBeta0
       if(UseBeta0)then
          write(*,*) prefix, 'Beta0          = ', Beta0
          write(*,*) prefix, 'EjectaTemp     = ', EjectaTemperatureDim, '[K]'
       end if
       write(*,*) prefix, 'Start time     = ', tStartCme,            '[s]'
       write(*,*) prefix, 'CME speed      = ', uCmeSi*Si2Io_V(UnitU_),'[km/s]'
       write(*,*) prefix
    end if

    ! Center position of the configuration in the heliocentric frame
    XyzCenterConf_D = rDistance1*DirCme_D

    EjectaTemperature = EjectaTemperatureDim*Io2No_V(UnitTemperature_)

    !$acc update device(Alpha0R0, Alpha0, XyzCenterConf_D, bConf_D, bAmbientConf_D)
    !$acc update device(uCmeSi, B0, Radius, UseImageDipoles)
    !$acc update device(iHelicity)
    !$acc update device(UseBeta0, Beta0, EjectaTemperature)

    if(UseImageDipoles) call mc18_compute_image_dipoles

  end subroutine mc18_init
  !============================================================================
  subroutine mc18_compute_image_dipoles

    real :: mDip_D(3)
    real :: mR_D(3), mT_D(3)
    real :: dImg, scale3, du, uCtr
    integer :: i
    !--------------------------------------------------------------------------
    ! Equivalent external dipole: m = -bAmbientConf_D * Radius^3/2
    ! (Rosenbluth-Bussac B_r-continuity condition; see Borovikov et al. 2018)
    mDip_D = -bAmbientConf_D*(Radius**3/2.0)

    mR_D = sum(mDip_D*DirCme_D)*DirCme_D  ! radial component of moment
    mT_D = mDip_D - mR_D                  ! transverse component of moment

    ! Image point at (a^2/d)*rHat = (1/d)*rHat  (solar surface a = 1 R_sun)
    dImg   = 1.0/rDistance1
    scale3 = dImg**3              ! = (a/d)^3

    nImgDipoles = 1 + nDiscDipoles

    ! Combined point image: moment = scale3*(m_t - m_r)  (Lin 2006 Eq. II.B-C)
    rImgDipole_DI(:,1) = dImg*DirCme_D
    mImgDipole_DI(:,1) = scale3*(mT_D - mR_D)

    ! Discretised line images: cell-centred over [0, dImg] along rHat_D
    du = dImg/nDiscDipoles
    do i = 1, nDiscDipoles
       uCtr = (i - 0.5)*du
       rImgDipole_DI(:, 1+i) = uCtr*DirCme_D
       mImgDipole_DI(:, 1+i) = -(mT_D/rDistance1)*uCtr*du
    end do

    !$acc update device(nImgDipoles, rImgDipole_DI, mImgDipole_DI)

  end subroutine mc18_compute_image_dipoles
  !============================================================================
  subroutine set_parameters_mc18(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in):: NameCommand

    real :: SinAlpha
    character(len=*), parameter:: NameSub = 'set_parameters_mc18'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case("#CME","#MAGCONE")
       call read_var('BaseHeight',      BaseHeight)     ![rSun]
       call read_var('uCmeSi',          uCmeSi)         ![km/s]
       call read_var('UseImageDipoles', UseImageDipoles)
       if(UseImageDipoles)then
          call read_var('nDiscDipoles', nDiscDipoles)
          if(nDiscDipoles > MaxImgDipoles - 1) call CON_stop( &
               NameSub//': nDiscDipoles exceeds MaxImgDipoles-1 = 1000')
       end if
       call read_var('UseBeta0', UseBeta0)
       if(UseBeta0)then
          call read_var('Beta0', Beta0)
          call read_var('EjectaTemperature', EjectaTemperatureDim)
       end if

       ! Derive center distance and radius from opening angle and base height.
       ! Geometry: sin(alpha) = Radius/rDistance1
       !           1 + BaseHeight = rDistance1 - Radius 
       !                          = rDistance1*(1 - sin(alpha))
       if(HalfOpeningAngle <= 0.0 .or. HalfOpeningAngle >= 90.0) call &
            CON_stop(NameSub//': HalfOpeningAngle must be in (0, 90) degrees')
       SinAlpha   = sin(HalfOpeningAngle*cDegToRad)
       rDistance1 = (1.0 + BaseHeight)/(1.0 - SinAlpha)
       Radius     = rDistance1*SinAlpha

       ! position of the CME apex
       XyzCmeApexSi_D = DirCme_D*(rDistance1 + Radius)

       ! position of CME center and bottom
       XyzCmeCenterSi_D = XyzCmeApexSi_D - DirCme_D*Radius
       XyzCmeBottomSi_D = XyzCmeApexSi_D - DirCme_D*2.0*Radius
       DoNormalizeXyz = .true.

    case default
       call CON_stop(NameSub//' unknown NameCommand='//NameCommand)
    end select

  end subroutine set_parameters_mc18
  !============================================================================
  subroutine get_mc18_fluxrope(XyzIn_D, Rho, p, b_D, u_D, TimeNow)
    !$acc routine seq

    ! Magnetic field perturbation of the Rosenbluth-Bussac force-free spheromak.
    ! Interior: spherical Bessel (j1) field minus the uniform ambient field
    !   (Borovikov et al. 2018 uniform-field subtraction for div-B continuity).
    ! Exterior: pure dipole of equivalent moment m = -bAmbientConf_D * Radius^3/2.
    ! Image dipole correction (Lin 2006) applied everywhere if UseImageDipoles.

    use ModCoordTransform, ONLY: cross_product

    ! Coordinates of the input point, in rSun
    real, intent(in) :: XyzIn_D(3)

    ! OUTPUTS
    real, intent(out) :: b_D(3)
    real, intent(out), optional :: u_D(3)
    real, optional, intent(in)  :: TimeNow

    ! Density, pressure (non-zero inside if UseBeta0)
    real, intent(out) :: Rho, p

    real :: XyzConf_D(3), Distance2ConfCenter
    real :: R2CrossB0_D(3), Alpha0R2
    real :: mDip_D(3), MdotR
    real :: PhiInv
    real :: dr_D(3), R2img
    integer :: i
    !--------------------------------------------------------------------------
    if(UseMagCone)then
       PhiInv = 1.0/(1.0 + (TimeNow - tStartCme)*uCmeSi*rCmeApexInvSi)
    else
       PhiInv = 1.0
    end if

    Rho = 0.0
    p   = 0.0
    if(present(u_D)) u_D = 0.0

    ! Position relative to spheromak center (self-similar scaling applied)
    XyzConf_D = XyzIn_D*PhiInv - XyzCenterConf_D

    Distance2ConfCenter = norm2(XyzConf_D)
    if(Distance2ConfCenter <= Delta)then
       XyzConf_D           = XyzConf_D*(Delta/Distance2ConfCenter)
       Distance2ConfCenter = Delta
    end if

    if(Distance2ConfCenter <= Radius)then

       ! INSIDE: force-free spherical Bessel field
       Alpha0R2    = Alpha0*Distance2ConfCenter
       R2CrossB0_D = cross_product(XyzConf_D, bConf_D)
       b_D = (2*bConf_D + iHelicity*Alpha0*R2CrossB0_D) &
            *(spher_bessel1_over_x(Alpha0R2) - Beta0) &
            + spher_bessel2(Alpha0R2)/Distance2ConfCenter**2 &
            *cross_product(XyzConf_D, R2CrossB0_D)          &
            - bAmbientConf_D

       if(UseBeta0)then
          p   = (spher_bessel1_over_x(Alpha0R2) - Beta0) &
               * Beta0 * Alpha0**2 * sum(R2CrossB0_D**2)
          Rho = p/EjectaTemperature
       end if

       if(present(u_D) .and. UseMagCone)then
          u_D = XyzIn_D*PhiInv  &
               *No2Si_V(UnitX_) &
               *uCmeSi*rCmeApexInvSi
       end if

    else

       ! OUTSIDE: pure dipole  m = -bAmbientConf_D * Radius^3/2
       ! (ensures B_r continuity with the subtracted-ambient interior)
       mDip_D = -bAmbientConf_D*(Radius**3/2.0)
       MdotR  = sum(mDip_D*XyzConf_D)
       b_D = (3.0*MdotR*XyzConf_D/Distance2ConfCenter**2 - mDip_D) &
            /Distance2ConfCenter**3

    end if

    ! Lin (2006) image dipole corrections (heliocentric coordinates, unscaled)
    if(UseImageDipoles)then
       do i = 1, nImgDipoles
          dr_D  = XyzIn_D - rImgDipole_DI(:,i)
          R2img = sum(dr_D**2)
          MdotR = sum(mImgDipole_DI(:,i)*dr_D)
          b_D   = b_D + (3.0*MdotR*dr_D/R2img - mImgDipole_DI(:,i)) &
               /(sqrt(R2img)*R2img)
       end do
    end if

    b_D = b_D*No2Si_V(UnitB_)
    Rho = Rho*No2Si_V(UnitRho_)
    p   = p  *No2Si_V(UnitP_)
    ! u_D is already in SI [m/s] from the interior formula above
  end subroutine get_mc18_fluxrope
  !============================================================================
  real function spher_bessel0(x)
    real, intent(in) :: x
    !--------------------------------------------------------------------------
    spher_bessel0 = sin(x)/x
  end function spher_bessel0
  !============================================================================
  real function spher_bessel1(x)
    real, intent(in) :: x
    !--------------------------------------------------------------------------
    spher_bessel1 = (sin(x) - x*cos(x))/x**2
  end function spher_bessel1
  !============================================================================
  real function spher_bessel1_over_x(x)
    real, intent(in) :: x
    !--------------------------------------------------------------------------
    if(x == 0)then
       spher_bessel1_over_x = 1.0/3.0
    else
       spher_bessel1_over_x = (sin(x) - x*cos(x))/x**3
    end if
  end function spher_bessel1_over_x
  !============================================================================
  real function spher_bessel2(x)
    real, intent(in) :: x
    !--------------------------------------------------------------------------
    spher_bessel2 = 3*spher_bessel1_over_x(x) - spher_bessel0(x)
  end function spher_bessel2
  !============================================================================
  real function find_alpha0r0(Beta0In)

    ! Solve j1(x)/x = Beta0In for x = Alpha0R0 by Newton-Raphson.
    ! j1(x)/x is monotone decreasing from 1/3 at x=0 (Taylor limit)
    ! to 0 at x=4.4934 (first zero of j1), so a unique solution
    ! exists for any 0 < Beta0In < 1/3.
    ! Derivative: d/dx[j1(x)/x] = -j2(x)/x (from Bessel recurrence).

    real, intent(in) :: Beta0In

    real    :: x, fx, dfdx
    integer :: i
    integer, parameter :: nIter = 20
    real,    parameter :: Tol   = 1.0e-7
    real,    parameter :: xZero = 4.493409457909064
    character(len=*), parameter :: NameSub = 'find_alpha0r0'
    !--------------------------------------------------------------------------
    if(Beta0In <= 0.0)then
       find_alpha0r0 = xZero
       return
    end if
    if(Beta0In >= 1.0/3.0) call CON_stop( &
         NameSub//': Beta0 >= 1/3, no solution to j1(x)/x = Beta0 exists')

    ! Linear interpolation on (0, xZero) as initial guess
    x = xZero*(1.0 - 3.0*Beta0In)

    do i = 1, nIter
       fx   =  spher_bessel1_over_x(x) - Beta0In
       dfdx = -spher_bessel2(x)/x
       x    =  x - fx/dfdx
       if(abs(fx) < Tol) exit
    end do
    find_alpha0r0 = x

  end function find_alpha0r0
  !============================================================================
  subroutine get_mc18_size(SizeXY,  SizeZ)
    real,  intent(out) :: SizeXY,  SizeZ
    !--------------------------------------------------------------------------
    SizeXY = Radius                    ! Horizontal size
    SizeZ  = rDistance1 + Radius - 1.0 ! Apex height above solar surface
  end subroutine get_mc18_size
  !============================================================================
end module EEE_ModMc18
!==============================================================================
