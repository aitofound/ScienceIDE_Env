!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module EEE_ModGL98

  ! Gibson-Low CME flux rope model

#ifdef _OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use EEE_ModCommonVariables

  implicit none

  SAVE

  private ! except

  public :: set_parameters_gl98
  public :: get_gl98_fluxrope
  public :: get_gl98_size
  public :: gl98_init

  ! Local variables

  ! Geometric characteristics of the superimposed configuration:

  ! contraction distance as in   r --> r -a
  real :: Stretch = 0.0
  !$acc declare create(Stretch)

  ! distance from the magnetic configuration center to heliocenter
  real :: rDistance1 = 0.0

  ! Radius of the magnetic configuration (spheromac)
  real :: Radius = 0.0
  !$acc declare create(Radius)

  ! This is the combination of the above parameters:
  !
  ! 1[rSun] + ApexHeight + Stretch = rDistance1 + Radius
  !
  ! Not used in calculations, however, is more easy input parameter
  ! than rDistance1.
  real :: ApexHeight = 0.0

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

  ! Characteristic ratio of plasma pressure to B_0^2/\mu_0
  ! Exact definition in terms of the pressure derivative over
  ! the flux function: \beta_0=\mu_0/(B_0 Alpha_0^2) dp/d\psi
  real :: Beta0
  !$acc declare create(Beta0)

  ! Dimensionless product of R0 by Alpha0. For any given \beta_0,
  ! this product is found from the equation,
  !
  ! j_1(\alpha_0r_0)/(\alpha_0r_0)=\beta_0, (*)
  !
  ! where j_1(x)=sin(x)/x^2 -cos(x)/x is the spherical Bessel function.
  ! With this equation satisfied, the radial and phi- components of the
  ! spheromak field as well as the flux function, current function and
  ! pressure all turn to zero at the external boundary of configuration,
  ! at r=r_0, while theta-component of the field is non-zero and
  ! proportional to j_2(\alpha_0r_0). The GL paper chooses a special case of
  ! (negative) \beta_0. Specifically, the product \alpha_0r_0 is chosen
  ! to be a first zero of j_2 function, and then \beta_0 is chosen to satisfy
  ! Eq (*) with this value of .
  real, parameter :: Alpha0R0 = 5.763854

  ! Vector characteristic of the configuration: radius vector of the
  ! configuration center and B0 multiplied by unit vector along
  ! the axis of symmetry
  real :: XyzCenterConf_D(3), bConf_D(3)
  !$acc declare create(XyzCenterConf_D, bConf_D)

  ! Parameter to control self-similar solution
  real :: uCmeSi = 0.0
  !$acc declare create(uCmeSi)
  real, parameter :: Delta = 0.1

contains
  !============================================================================
  subroutine gl98_init

    use ModCoordTransform, ONLY: rot_xyz_mercator, rot_matrix_z

    real :: Rotate_DD(3,3)

    !--------------------------------------------------------------------------
    alpha0 = Alpha0R0/Radius
    Beta0 = (sin(Alpha0R0) - Alpha0R0*cos(Alpha0R0))/Alpha0R0**3

    ! The constant coefficient, Beta0 = -2.8723629148938019E-02
    ! This is Beta0 parameter for the GL particular configuration
    B0 = B0Dim*Io2No_V(UnitB_)

    ! Relation between the cme_a1 parameter by Gibson-Low and B0
    ! B0 = - cme_a1*Io2No_V(UnitB_)/(Beta0*Alpha0**2)*4*cPi

    ! The costant coefficient,
    ! -4*cPi/(Beta0*Alpha0R0**2) = 13.1687517342067082
    ! Normalize the self-similar CME speed
    if(UseSpheromak) uCmeSi = uCmeSi*Io2Si_V(UnitU_)
    if(iProc==0)then
       write(*,*) prefix
       write(*,*) prefix, &
            '>>>>>>>>>>>>>>>>>>>                  <<<<<<<<<<<<<<<<<<<<<'
       write(*,*) prefix
       write(*,*) prefix, &
            '            Gibson and Low CME is initiated!!!'
       write(*,*) prefix
       write(*,*) prefix, &
            '>>>>>>>>>>>>>>>>>>>                  <<<<<<<<<<<<<<<<<<<<<'
       write(*,*) prefix
       write(*,*) prefix, 'B0Dim          = ', B0*No2Io_V(UnitB_),'[Gauss]'
       write(*,*) prefix, 'Radius         = ', Radius,'[rSun]'
       write(*,*) prefix, 'Stretch        = ', Stretch, '[rSun]'
       write(*,*) prefix, 'rDistance1     = ', rDistance1,'[rSun]'
       write(*,*) prefix, 'ApexHeight     = ', ApexHeight,'[rSun]'
       write(*,*) prefix, 'LongitudeCme   = ', LongitudeCme,'[degrees]'
       write(*,*) prefix, 'LatitudeCme    = ', LatitudeCme,'[degrees]'
       write(*,*) prefix, 'OrientationCme = ', OrientationCme,'[degrees]'
       if(UseSpheromak)then
           write(*,*) prefix, '>> Self-similar solution for spheromak <<'
           write(*,*) prefix, 'Start time     = ',tStartCme,'[s]'
           write(*,*) prefix, 'CME speed      = ',uCmeSi*Si2Io_V(UnitU_),&
                '[km/s]'
           write(*,*) prefix, 'is reached at R = ',&
                Si2Io_V(UnitX_)/rCmeApexInvSi,'[rSun]'
           if(uCmeSi > 0.0 .and. 1 + ApexHeight > 2*Radius)then
              write(*,'(a,f4.1,a)') prefix//&
                   'The lowest CME point reaches the heliodistance of '// &
                   '1 Rs at time=',&
                   ((2*Radius - ApexHeight)/(1 + ApexHeight -2*Radius)&
                   /(uCmeSi*rCmeApexInvSi) + tStartCme)/3600,'[h]'
              write(*,'(a,f4.1,a)') prefix//&
                   'The lowest CME point reaches the heliodistance of '// &
                   '1.15 Rs at time=',&
                   ((0.15 + 2*Radius - ApexHeight)/(1 + ApexHeight -2*Radius)&
                   /(uCmeSi*rCmeApexInvSi) + tStartCme)/3600,'[h]'
           end if
        end if
       Write(*,*) prefix
    end if

    ! Construct the rotational matrix Rotate_DD,
    ! Rotate to the local Mercator map around the CME center and then rotate
    ! about the vertical direction to align x-axis with the direction from
    ! positive to negative spot.

    Rotate_DD  = matmul(rot_xyz_mercator(DirCme_D), &
         rot_matrix_z(OrientationCme*cDegToRad))

    ! In the rotated frane magnetic field at the center of configuration
    ! is directed along y-axis in the case of positive helicity or in the
    ! opposite direction in the case of negative helicity. With the properly
    ! signed B0, we have

    bConf_D = matmul(Rotate_DD, [0.0, B0, 0.0])

    ! In the rotated coordinates the coordinate vector from
    ! the heiocenter to the center of configuration is
    ! (/0, 0, rDistance1/). Find this vector in the original
    ! coodinate frame.

    XyzCenterConf_D = rDistance1*DirCme_D

    !$acc update device(Stretch, Alpha0, Beta0, XyzCenterConf_D, bConf_D)
    !$acc update device(uCmeSi, B0, Radius)

  end subroutine gl98_init
  !============================================================================
  subroutine set_parameters_gl98(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in):: NameCommand

    integer:: iHelicity
    character(len=*), parameter:: NameSub = 'set_parameters_gl98'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case("#CME","#SPHEROMAK")
       call read_var('BStrength',   B0Dim)         ![Gauss]
       call read_var('iHelicity',   iHelicity)
       B0Dim = abs(B0Dim)*iHelicity ! Set the sign of B0Dim based on iHelicity
       call read_var('Radius',      Radius)        ![rSun]
       call read_var('Stretch',     Stretch)       ![rSun]
       call read_var('ApexHeight',  ApexHeight)    ![rSun]
       if(UseSpheromak)call read_var('uCmeSi', uCmeSi)  ![km/s]

       rDistance1 = 1 + ApexHeight + Stretch - Radius

       ! position of the CME apex
       XyzCmeApexSi_D = DirCme_D*(1 + ApexHeight)

       ! position of CME center
       XyzCmeCenterSi_D = XyzCmeApexSi_D - DirCme_D*Radius
       DoNormalizeXyz = .true.

    case default
       call CON_stop(NameSub//' unknown NameCommand='//NameCommand)
    end select

  end subroutine set_parameters_gl98
  !============================================================================
  subroutine get_gl98_fluxrope(XyzIn_D, Rho, p, b_D, u_D, TimeNow)
    !$acc routine seq

    ! Definition of Parameters used for the initial state
    !   Stretch    = contraction distance as in   r --> r -a
    !   rDistance1  = distance from the flux rope center to heliocenter
    !   Radius0     = radius of flux rope
    !
    ! OrientationCME : The counter-clockwise angle between the fluxrope
    !                  axis and the solar equator.
    !
    ! Calculates magnetic field, pressure and density for a coronal flux
    ! rope capable of self-similar expansion and erupting in a CME.
    ! The analytical solution is taken from Gibson and Low
    ! Astrophysical Journal, Vol 493, p. 460.
    !
    ! Written by Chip Manchester Jan 18 2001
    ! Rewritten by Chip Nov 29 2001 for flux rope injection
    !
    !   Bug fixes
    !   March 18       dpres_1dr1 = cme_a1*(dA2dr*dr2dr1 + dA2dth*dth2dr1)
    !   change to..... dpres_1dr1 = a1scl*(dA2dr*dr2dr1 + dA2dth*dth2dr1)
    !   without above fix, same as used for runs 12, 13, 14
    !   Feb  07, 2002 Br1 is changed to Br1**2 in density calc thanks to Ilia
    !   Feb  12, 2002 expression for ga0r0 is fixed
    !   Feb  22, 2002 derivative of B2*2/dr1 is fixed
    !   Feb  22, 2002 angles in 2nd coordinate system are redefined
    !
    ! Igor 2017-03-03 Bug is found. Formulae from the Gibson-Low paper
    ! were derived in CGS system with the use of 1/4\pi factor in the
    ! magnetic pressure and magnetic force density. In the existing
    ! version of the eruptive event generator, the dimensionless formulation
    ! of the MHD equations are employed, with no 1/4\pi coefficient. To keep
    ! some backward compatibility, the coefficient 4\pi is retained in the
    ! definition of a1scl, hence old input parameters will provide same
    ! distribution for the magnetic field, however, the perturbations of
    ! pressure and density will be a factor of 4\pi times larger.
    !
    ! Igor 2017-03-04 Fix a bug  Before the 'helicity switch' was
    ! implemented as follows:
    ! if(cme_a1 > 0) then;Br2 = -Br2; Btheta2 = -Btheta2; end if
    ! However, below, the magnetic pressure gradient
    ! is calculated including Br2*dBr2dr1 + Btheta2*dBtheta2dr1.
    ! Therefore, the sign in the magnetic field derivatives should
    ! be changed too. Add two more operators prior to end if:
    ! dBr2dr1     = -dBr2dr1; dBtheta2dr1 = -dBtheta2dr1
    !
    ! Igor 2020-06-04 finalized implementation to get it match to the paper
    !   Borovikov, D., I. V. Sokolov, W. B. Manchester, M. Jin, and
    !   T. I. Gombosi (2017), Eruptive event generator based on the Gibson-Low
    !   magnetic configuration, JGR, 122, 7979–7984, doi:10.1002/2017JA024304

    use ModCoordTransform, ONLY: cross_product

    ! Coordinates of the input point, in rSun
    real, intent(in) :: XyzIn_D(3)

    ! OUTPUTS
    ! Magnetic field
    real, intent(out) :: b_D(3)

    ! Optional: velocity of self-similar expansion
    real, intent(out),optional :: u_D(3)
    ! Optional: input time
    real, optional, intent(in) :: TimeNow

    ! Density, pressure
    real, intent(out) :: Rho, p

    ! User declared local variables go here::
    ! Radial coordinate of the input point
    real :: r

    ! Coordinates of the input point after
    ! stretching transformation
    real :: rTransformed, XyzTransformed_D(3)

    ! Ccordinate vector originating at the center of
    ! the magnetic configuration and its module
    real :: Distance2ConfCenter, XyzConf_D(3)

    ! Local pressure in the magnetic configuration
    real :: pConf

    ! Variables needed to calculate parameters of the
    ! stretched magnetic configuration:
    ! 1. Unit vector of radial direction and the magnetic
    ! field projection onto it:
    real :: eBoldR_D(3), Br1

    ! 2. Magnetic field squared, total pressure gradient
    ! and its projection on the radial direction
    real ::  bSquared, DpTotalDr1, GradPTotal_D(3)

    ! 3. Radial tension of stretched field
    real :: RadialTension

    ! 4. Local acceleration of the gravitational force
    real :: gGravity

    real :: R2CrossB0_D(3)
    real :: Alpha0R2

    ! Parameters of the self-similar expansion
    real :: Phi     ! 1 + Time*(U/r)
    real :: PhiInv  ! 1/Phi
    !--------------------------------------------------------------------------

    r = norm2(XyzIn_D)
    ! Unit vector of radial direction
    eBoldR_D = XyzIn_D/r
    if(UseSpheromak)then
       ! Expansion factor for self-similar solution
       Phi = 1 + (TimeNow - tStartCme)*uCmeSi*rCmeApexInvSi
       PhiInv = 1/Phi
       ! Input argument for the self-similar solution
       r = r*PhiInv
    end if

    ! Stretched CARTESIAN COORDINATES
    rTransformed = r + Stretch
    XyzTransformed_D = eBoldR_D*rTransformed

    ! Coordinates relative to CME flux rope center
    XyzConf_D = XyzTransformed_D - XyzCenterConf_D
    Distance2ConfCenter = norm2(XyzConf_D)

    if (Distance2ConfCenter <= Delta) then
       XyzConf_D = XyzConf_D*(Delta/Distance2ConfCenter)
       Distance2ConfCenter = Delta
    end if
    if (Distance2ConfCenter <= Radius) then

       ! INSIDE THE MAGNETIC FLUX ROPE REGION
       ! An argument of spherical Bessel functions
       Alpha0R2 = Alpha0*Distance2ConfCenter

       ! Magnetic field components
       R2CrossB0_D = cross_product(XyzConf_D,bConf_D)
       b_D = (2*bConf_D &
            - sign(Alpha0, B0)* &  ! Helicity
            R2CrossB0_D)*(spher_bessel1_over_x(Alpha0R2) - Beta0) &
            + spher_bessel2(Alpha0R2)/Distance2ConfCenter**2*&
            cross_product(XyzConf_D, R2CrossB0_D)

       ! Compute kinetic gas pressure
       pConf =  Beta0*(Alpha0**2)*&
            sum(R2CrossB0_D**2)*(spher_bessel1_over_x(Alpha0R2) - Beta0)

       ! If we use stretch:
       Br1 = sum(eBoldR_D*b_D)
       bSquared = sum(b_D**2)
       GradPTotal_D =&
            Alpha0**2*(B0**2*XyzConf_D - bConf_D*sum(bConf_D*XyzConf_D))*&
            (spher_bessel1_over_x(Alpha0R2)**2 - Beta0**2)&
            - Alpha0*(XyzConf_D/Distance2ConfCenter**3)*sum(R2CrossB0_D**2)*&
            spher_bessel2(Alpha0R2)*spher_bessel1(Alpha0R2) &
            - 4*B0**2*(XyzConf_D/Distance2ConfCenter**2)*&
            spher_bessel2(Alpha0R2)*(spher_bessel1_over_x(Alpha0R2) - Beta0)&
            + (XyzConf_D*sum(bConf_D*XyzConf_D)**2/Distance2ConfCenter**4 - &
            bConf_D*sum(bConf_D*XyzConf_D)/Distance2ConfCenter**2&
            )*(spher_bessel2(Alpha0R2)**2 - 4*spher_bessel2(Alpha0R2)*&
            (spher_bessel1_over_x(Alpha0R2) - Beta0)) +&
            Alpha0*(XyzConf_D/Distance2ConfCenter**3)*sum(R2CrossB0_D**2)*(&
            (spher_bessel1(Alpha0R2) - 3*spher_bessel2(Alpha0R2)/Alpha0R2)*&
            (spher_bessel2(Alpha0R2) - 2*&
            (spher_bessel1_over_x(Alpha0R2) - Beta0)) + &
            2*spher_bessel2(Alpha0R2)**2/Alpha0R2 )
       DpTotalDr1 = sum(GradPTotal_D*eBoldR_D)

       ! MAGNETIC FIELD transformed with stretching transformation
       b_D = (rTransformed/r)*b_D + Stretch*XyzTransformed_D*Br1/r**2

       ! PLASMA DENSITY with stretching transformation
       gGravity   = abs(Gbody)/(r**2)
       RadialTension = (Stretch/r)*(rTransformed/r)**2 &
            *( (2 + Stretch/r)*(bSquared/rTransformed + DpTotalDr1) &
            + 2*pConf/rTransformed - (3 + 2*Stretch/r)*Br1**2/r )
       Rho = RadialTension/gGravity

       ! PLASMA PRESSURE with contraction transformation
       p = pConf*(rTransformed/r)**2 &
            - 0.5*((rTransformed/r)**2)*((rTransformed/r)**2 - 1)*Br1**2
       Rho = Rho*No2Si_V(UnitRho_)
       p = p*No2Si_V(UnitP_)
       b_D = b_D*No2Si_V(UnitB_)
       ! Temporalily disable computation of self-similar expansion velocity
       if(present(u_D)) u_D = 0 !.and. UseSpheromak) &
       ! u_D = XyzIn_D*PhiInv   & ! Inputs for self-similar solution
       !*No2Si_V(UnitX_)       & ! Conversion to SI units
       !*uCmeSi*rCmeApexInvSi    ! uCmeSi velocity is reached at the apex

    else
       b_D = 0; Rho = 0; p = 0
       if(present(u_D)) u_D = 0
    endif

  contains
    !==========================================================================
    real function spher_bessel0(x)
      real, intent(in) :: x
      !------------------------------------------------------------------------
      spher_bessel0 = sin(x)/x
    end function spher_bessel0
    !==========================================================================
    real function spher_bessel1(x)
      real, intent(in) :: x
      !------------------------------------------------------------------------
      spher_bessel1 = (sin(x) - x*cos(x))/x**2
    end function spher_bessel1
    !==========================================================================
    real function spher_bessel1_over_x(x)
      real, intent(in) :: x
      !------------------------------------------------------------------------
      if(x == 0)then
         spher_bessel1_over_x = 1.0/3.0
      else
         spher_bessel1_over_x = (sin(x) - x*cos(x))/x**3
      end if
    end function spher_bessel1_over_x
    !==========================================================================
    real function spher_bessel2(x)
      real, intent(in) :: x
      !------------------------------------------------------------------------
      spher_bessel2 = 3*spher_bessel1_over_x(x) - spher_bessel0(x)
    end function spher_bessel2
    !==========================================================================
  end subroutine get_gl98_fluxrope
  !============================================================================
  subroutine get_GL98_size(SizeXY,  SizeZ)
    real,  intent(out) :: SizeXY,  SizeZ
    !--------------------------------------------------------------------------
    SizeXY = Radius                    ! Horizontal size
    SizeZ  = ApexHeight                ! Apex height
  end subroutine get_GL98_size
  !============================================================================
end module EEE_ModGL98
!==============================================================================
