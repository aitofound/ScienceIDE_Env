!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module CON_planet_field

  ! This class provides the magnetic field of the planet
  ! for an arbitrary spatial position at an arbitrary time.
  ! It also provides the mapping from an arbitrary point to a given
  ! radial distance.
  ! The position as well as the magnetic field can be represented as 3
  ! scalars or 1 three-element array.
  ! The coordinate system and the normalization of the coordinates
  ! and the magnetic field can be given with string input arguments.

  use CON_planet
  use CON_axes
  use ModUtilities, ONLY: CON_stop

  implicit none

  private ! except

  ! PUBLIC MEMBER FUNCTIONS
  public :: get_planet_field  ! Get planet field at some time and place
  public :: map_planet_field  ! Map planet field from a point to a radius
  public :: map_planet_field_fast ! map dipole field (GPU version)
  public :: test_planet_field ! Test the methods in this module

  ! revision history:
  ! 11Aug03 - Gabor Toth <gtoth@umich.edu> - initial prototype/prolog/code
  ! 28Nov04 - Gabor Toth - added optional arguments DoNotConvertBack and
  !                        Jacobian matrix to the map_planet_field

  interface get_planet_field
     module procedure &
          get_planet_field11, &
          get_planet_field13, &
          get_planet_field31, &
          get_planet_field33
  end interface

  interface map_planet_field
     module procedure &
          map_planet_field11, &
          map_planet_field33
  end interface

  integer, parameter :: x_=1, y_=2, z_=3

  character(len=*), parameter :: NameMod = 'CON_planet_field'

contains
  !============================================================================
  subroutine get_planet_field11(TimeSim, XyzIn_D, TypeCoord, b_D)

    real,              intent(in) :: TimeSim      ! simulation time
    real,              intent(in) :: XyzIn_D(3)   ! spatial position
    character(len=*),  intent(in) :: TypeCoord    ! type of coordinates

    real,              intent(out):: b_D(3)       ! magnetic field

    ! This is the fundamental subroutine that provides the magnetic
    ! field at a given position at a given simulation time.
    ! If called repeatedly, the subroutine remembers the last simulation time
    ! argument, so it does not recalculate the position of the magnetic axis.
    ! The position may be normalized with the radius of the planet.
    ! The coordinate system and normalization information
    ! for the position is given by the string TypeCoord.
    ! The first 3 characters should contain the coordinate system.
    ! This may be followed (after some spaces) by the characters "NORM"
    ! in all capitals. For example "MAG", "GSM NORM", "GSE NORMALIZED" etc.

    real :: Xyz_D(3)     ! Normalized (and rotated) position
    real :: Dipole_D(3)  ! Dipole moment
    real :: r, r2, rInv, r2Inv, r3Inv, Term1
    character (len=3) :: NameCoordSystem

!!! logical :: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'get_planet_field11'
    !--------------------------------------------------------------------------

!!! call CON_set_do_test(NameSub,DoTest,DoTestMe)

    if(TypeBField == 'NONE')then
       b_D = 0
       RETURN
    end if

    if(len(TypeCoord)<3)call CON_stop(NameSub//&
         ' SWMF_ERROR: coordinate type should be at least 3 characters,'// &
         ' TypeCoord='//TypeCoord)

    ! Normalize position if necessary
    if(index(TypeCoord,"NORM")>0)then
       Xyz_D = XyzIn_D
    else
       Xyz_D = XyzIn_D / RadiusPlanet
    end if

    ! The coord system name is stored in the first 3 characters
    NameCoordSystem = TypeCoord(1:3)

    ! Calculate magnetic field
    select case(TypeBField)
    case('DIPOLE')

       Xyz_D = Xyz_D - MagCenter_D

       ! radial distance squared
       r2 = sum(Xyz_D**2)
       if(r2 < 1E-12) then
           ! return zero field if very small
           b_D = 0
           RETURN
       end if

       ! Update axes
       call set_axes(TimeSim)

       ! Various powers of radial distance
       r2Inv = 1/r2
       r     = sqrt(r2)
       rInv  = 1/r
       r3Inv = rInv*r2Inv

       select case(NameCoordSystem)
       case('GSM')
          ! Dipole_D has X and Z components only
          Dipole_D = DipoleStrength*MagAxisGsm_D
          Term1 = sum(Dipole_D(x_:z_:2)*Xyz_D(x_:z_:2))*3*r2Inv
          b_D = (Term1*Xyz_D - Dipole_D)*r3Inv
       case('GSE')
          ! Dipole_D has X,Y and Z components in general
          Dipole_D = DipoleStrength*MagAxis_D
          Term1 = sum(Dipole_D*Xyz_D)*3*r2Inv
          b_D = (Term1*Xyz_D - Dipole_D)*r3Inv
       case('GEO')
          ! Dipole_D has X,Y and Z components in general
          Dipole_D = DipoleStrength*MagAxisGeo_D
          Term1 = sum(Dipole_D*Xyz_D)*3*r2Inv
          b_D = (Term1*Xyz_D - Dipole_D)*r3Inv
       case('MAG','SMG')
          ! Dipole is aligned with the Z axis
          Term1      = DipoleStrength*Xyz_D(3)*3*r2Inv
          b_D(x_:y_) = Term1*Xyz_D(x_:y_)*r3Inv
          b_D(z_)    = (Term1*Xyz_D(z_)-DipoleStrength)*r3Inv
       case default
          call CON_stop(NameSub// &
               ' SWMF_ERROR: unimplemented NameCoordSystem='//NameCoordSystem)
       end select

    case("MULTIPOLE")
       ! As of now the multipole option is only implemented for a GSE/GEO type
       ! coordinate system.

       ! radial distance squared
       r2 = sum(Xyz_D**2)
       if(r2 < 1E-12) then
          ! return zero field if very small
          b_D = 0
          RETURN
       end if

       ! Update axes
       call set_axes(TimeSim)
       call calculate_multipole_field(Xyz_D, b_D)

       ! -------- Old piece of code -----------------------------------------
       ! case('QUADRUPOLE','OCTUPOLE')
       !   ! Transform to MAG system
       !   Xyz_D = coord_transform(TimeSim,Xyz_D,NameCoordSystem,'MAG')
       !
       !   ! Dipole is aligned with Z
       !   Term1      = DipoleStrength*Xyz_D(3)*3*r2Inv
       !   b_D(x_:y_) = Term1*Xyz_D(1:2)*r3Inv
       !   b_D(z_)    = (Term1*Xyz_D(3)-DipoleStrength)*r3Inv
       !
       !   ! Add quadrupole terms
       !
       !   if(TypeBField == 'OCTUPOLE')then
       !      ! Add octupole terms
       !   end if
       !
       !   ! Transform the magnetic field back to the input coordinate system
       !   b_D = coord_transform(TimeSim,b_D,'MAG',NameCoordSystem)
       ! --------------------------------------------------------------------

    case default
       call CON_stop(NameSub//' SWMF_ERROR: unimplemented TypeBField='//&
            TypeBField)
    end select

  end subroutine get_planet_field11
  !============================================================================
  subroutine get_planet_field13(TimeSim, XyzIn_D, TypeCoord, Bx, By, Bz)

    real,              intent(in) :: TimeSim      ! simulation time
    real,              intent(in) :: XyzIn_D(3)   ! spatial position
    character(len=*),  intent(in) :: TypeCoord    ! type of coordinates
    real,              intent(out):: Bx, By, Bz   ! magnetic field

    real :: b_D(3)
    !--------------------------------------------------------------------------
    call get_planet_field(TimeSim, XyzIn_D, TypeCoord, b_D)

    Bx = b_D(x_)
    By = b_D(y_)
    Bz = b_D(z_)

  end subroutine get_planet_field13
  !============================================================================
  subroutine get_planet_field31(TimeSim, x, y, z, TypeCoord, b_D)

    real,              intent(in) :: TimeSim      ! simulation time
    real,              intent(in) :: x, y, z      ! spatial position
    character(len=*),  intent(in) :: TypeCoord    ! type of coordinates
    real,              intent(out):: b_D(3)       ! magnetic field

    !--------------------------------------------------------------------------
    call get_planet_field(TimeSim, [x, y, z], TypeCoord, b_D)

  end subroutine get_planet_field31
  !============================================================================
  subroutine get_planet_field33(TimeSim, x, y, z, TypeCoord, Bx, By, Bz)

    real,              intent(in) :: TimeSim      ! simulation time
    real,              intent(in) :: x, y, z      ! spatial position
    character(len=*),  intent(in) :: TypeCoord    ! type of coordinates
    real,              intent(out):: Bx, By, Bz   ! magnetic field

    real :: b_D(3)
    !--------------------------------------------------------------------------
    call get_planet_field(TimeSim, [x, y, z], TypeCoord, b_D)

    Bx = b_D(x_)
    By = b_D(y_)
    Bz = b_D(z_)

  end subroutine get_planet_field33
  !============================================================================
  subroutine calculate_multipole_field(XyzIn_D, b_D)

    ! Calculate the (Bx, By, Bz) components of the magnetic field based
    ! on Schimdt coefficients declared in CON_planet.f90

    use ModNumConst, ONLY: cTwoPi

    real, intent(in) :: XyzIn_D(3)
    real, intent(out) :: b_D(3)

    integer :: n, m
    real :: theta, phi, Br, Btheta, Bphi, XyzGeo_D(3)
    real, allocatable :: P_II(:,:), diffP_II(:,:)

    real :: sinmphi, cosmphi, sintheta, costheta, sinphi, cosphi
    real :: a_r, inv_sintheta, sinphi_prev, cosphi_prev

    ! Convert input location to GEO coordinate system
    ! We assume input coordinate system is GSE since
    ! mag-rot axes are aligned anyway.
    ! If you want to be proper and use multipole in another
    ! coordinate system you could use transform_matrix
    ! to find the appropriate transformation.
    ! That may be unnecessarily slow.
    ! Instead just replace GseGeo_DD to GsmGeo_DD e.g.
    !
    !    XyzGeo_D = matmul(GsmGeo_DD, XyzIn_D)
    !
    ! (also do this while re-converting to input coordsystem)
    !
    !   b_D = matmul(GeoGsm_DD, b_D)
    !
    ! (GSM here is just an example. If mag axis is aligned with
    !  rotation axis then GSE == GSM)
    !
    ! The purpose of this is to always calculate the multipole field
    ! in the coordinate system it was originally designed in i.e. GEO
    !
    ! Reference for spherical harmonics -
    ! Wertz, J. R. (Ed.). (2012). Spacecraft attitude determination and
    !    control (Vol. 73). Springer Science & Business Media.
    !    (Appendices G & H)
    !--------------------------------------------------------------------------
    XyzGeo_D = matmul(GeoGse_DD, XyzIn_D)

    ! Assuming a=1.0 (planetary normalization done in get_planet_field)
    a_r = 1.0/sqrt(XyzGeo_D(1)**2 + XyzGeo_D(2)**2 + XyzGeo_D(3)**2)
    theta = atan2(sqrt(XyzGeo_D(1)**2 + XyzGeo_D(2)**2), XyzGeo_D(3))
    phi = atan2(XyzGeo_D(2), XyzGeo_D(1))
    if(phi < 0.0) phi = phi + cTwoPi

    ! Additional variables to improve speed of calculation
    sintheta = sin(theta)
    costheta = cos(theta)
    sinphi = sin(phi)
    cosphi = cos(phi)

    inv_sintheta = 0.0
    if(abs(sintheta) > 1.0e-3) inv_sintheta = 1./sintheta

    if(.not.allocated(P_II)) then
      allocate(P_II(0:MaxHarmonicDegree, 0:MaxHarmonicDegree))
      allocate(diffP_II(0:MaxHarmonicDegree, 0:MaxHarmonicDegree))
    end if

    call calculate_legendre_polynomials(theta, P_II, diffP_II)

    Br = 0.0
    Btheta = 0.0
    Bphi = 0.0

    do n=0,MaxHarmonicDegree

      m=0
      sinphi_prev = 0.0
      cosphi_prev = 1.0
      Br = Br + (a_r)**(n+2) * (n+1) * P_II(n,m) * gPlanet_II(n,m)
      Btheta = Btheta - (a_r)**(n+2) * diffP_II(n,m) * gPlanet_II(n,m)
      ! Bphi contribution is 0 for m=0.

      do m=1,n

        sinmphi = sinphi_prev * cosphi + cosphi_prev * sinphi
        cosmphi = cosphi_prev * cosphi - sinphi_prev * sinphi
        Br = Br + (a_r)**(n+2) * (n+1) * P_II(n,m) * &
          (gPlanet_II(n,m)*cosmphi + hPlanet_II(n,m)*sinmphi)

        Btheta = Btheta - (a_r)**(n+2) * diffP_II(n,m) * &
          (gPlanet_II(n,m)*cosmphi + hPlanet_II(n,m)*sinmphi)

        Bphi = Bphi + inv_sintheta * (a_r)**(n+2) * P_II(n,m) * m * &
            (gPlanet_II(n,m)*sinmphi - hPlanet_II(n,m)*cosmphi)

        sinphi_prev = sinmphi
        cosphi_prev = cosmphi

      end do
    end do

    ! write(*,*) 'Bphi = ', Bphi
    ! write(*,*) 'Conversion Factor = ', Io2No_V(UnitB_)

    b_D(1) = (Br*sintheta*cosphi + Btheta*costheta*cosphi  &
               - Bphi*sinphi) * 1e-9
    b_D(2) = (Br*sintheta*sinphi + Btheta*costheta*sinphi &
               - Bphi*cosphi) * 1e-9
    b_D(3) = (Br*costheta - Btheta*sintheta) * 1e-9
    b_D = matmul(GseGeo_DD, b_D)
        ! Reconvert to input coordinate system.

  end subroutine calculate_multipole_field
  !============================================================================
  subroutine calculate_legendre_polynomials(theta, P_II, diffP_II)

    ! Subroutine to calculate the Schmidt normalized associated Legendre
    ! polynomials P(n,m) and their derivatives wrt theta diffP(n,m) to be
    ! used in the calculation of the magnetic field.
    ! Note that P, diffP are calculated for each value of theta

    ! This routine is optimized for performance. For straightforward alogrithm
    ! please refer to ModUserJupiter<latest>.f90 or the reference
    ! https://doi.org/10.1007/978-94-009-9907-7 (Appendix G,H)

    ! The Legendre polynomials calculated here are GAUSS NORMALIZED. Instead of
    ! normalizing them to Schmidt-semi-normalized we normalize the Schmidt
    ! coefficients instead at the beginning of the simulation.
    ! See subroutine normalize_schmidt_coefficients in CON_planet.f90
    real, intent(in) :: theta
    real, intent(out) :: P_II(0:MaxHarmonicDegree, 0:MaxHarmonicDegree)
    real, intent(out) :: diffP_II(0:MaxHarmonicDegree, 0:MaxHarmonicDegree)

    integer :: m, n
    real, allocatable :: K_II(:,:)

    real :: sintheta, costheta
    !--------------------------------------------------------------------------
    if(.not.allocated(K_II)) then
      allocate(K_II(0:MaxHarmonicDegree, 0:MaxHarmonicDegree))
    end if

    sintheta = sin(theta)
    costheta = cos(theta)

    P_II = 0.0
    diffP_II = 0.0
    K_II = 0.0

    P_II(0,0) = 1.0
    diffP_II(0,0) = 0.0

    do n=1,MaxHarmonicDegree
      P_II(n,n) = sintheta * P_II(n-1,n-1)
      diffP_II(n,n) = sintheta * diffP_II(n-1,n-1) + costheta * P_II(n-1,n-1)

      do m=0, n-1
         if (n == 1) then
            ! Note: For n=1, ==> m=0 and K_II(n,m) = 0
            P_II(n,m) = costheta*P_II(n-1,m)
            diffP_II(n,m) = costheta*diffP_II(n-1,m) - sintheta*P_II(n-1,m)
         else
            K_II(n,m) = ((n-1.)**2 - m**2) / ((2.*n - 1.) * (2.*n - 3.))
            P_II(n,m) = costheta*P_II(n-1,m) - K_II(n,m)*P_II(n-2,m)
            diffP_II(n,m) = costheta*diffP_II(n-1,m) - sintheta*P_II(n-1,m) &
                - K_II(n,m)*diffP_II(n-2,m)
         end if
      end do
    end do

    ! deallocate(K)
    ! deallocate(S)

  end subroutine calculate_legendre_polynomials
  !============================================================================
  subroutine map_planet_field11(TimeSim, XyzIn_D, TypeCoord, &
       rMapIn, XyzMap_D, iHemisphere, DoNotConvertBack, DdirDxyz_DD)

    real,              intent(in) :: TimeSim      ! simulation time
    real,              intent(in) :: XyzIn_D(3)   ! spatial position
    character(len=*),  intent(in) :: TypeCoord    ! type of coordinates
    real,              intent(in) :: rMapIn       ! radial distance to map to
    logical, optional, intent(in) :: DoNotConvertBack ! Leave XyzMap in SMG/MAG

    real,              intent(out):: XyzMap_D(3)      ! mapped position
    integer,           intent(out):: iHemisphere      ! which hemisphere
    real, optional,    intent(out):: DdirDxyz_DD(2,3) ! Jacobian matrix

    ! Map the planet field from the input position to the mapping radius.
    ! The coordinate system of the input coordinates is given by the first 3
    ! characters of the TypeCoord string. If the input coordinates are
    ! already normalized (given in units of planet radius), TypeCoord should
    ! contain the NORM string. Otherwise the coordinates are assumed to be
    ! in SI units (meters).
    !
    ! If the DoNotConvertBack argument is present, the mapped point will remain
    ! in the SMG (when the input coordinate system is not corotating)
    ! or MAG coordinates (when the input coordinate system rotates),
    ! otherwise it is converted back to the coordinate system of the
    ! input coordinates. The units for the output
    ! position are always the same as for the input coordinates.
    !
    ! The routine also returns which hemisphere the point maps to:
    ! +1 for north and -1 for south.
    ! If the point does not map to the defined radius at all, 0 is returned,
    ! and the output position is set to a radial projection of the input
    ! position to the magnetic equator.
    !
    ! If the DdirDxyz\_DD argument is present, the 2 x 3 Jacobian matrix
    ! dTheta/dx, dTheta/dy, dTheta/dz, dPhi/dx, dPhi/dy, dPhi/dz
    ! is returned.

    !PARAMETERS:

    ! If the  normalized input or mapping radius is less than this value
    ! an error occurs
    real, parameter :: rNormLimit = 0.9

    ! If difference between the normalized input and mapping radii
    ! is less than DrNormLimit a trivial mapping is done
    real, parameter :: DrNormLimit = 0.0001

    real             :: Xyz_D(3)        ! Normalized and rotated position
    character(len=3) :: NameCoordSystem ! Input/Output coordinate system

    ! Temporary variables for the analytic mapping
    real :: rMap, rMap2, rMap3, r, r3, XyRatio, XyMap2, XyMap, Xy2
    logical :: DoConvert
    real    :: Convert_DD(3,3)

!!! logical :: DoTest, DoTestMe

!!! call CON_set_do_test(NameSub,DoTest,DoTestMe)

    character(len=*), parameter:: NameSub = 'map_planet_field11'
    !--------------------------------------------------------------------------
    if(TypeBField == 'NONE') call CON_stop(NameSub// &
         ' SWMF_ERROR: the planet has no magnetic field')

    if(len(TypeCoord)<3)call CON_stop(NameSub//&
         ' SWMF_ERROR: coordinate type should be at least 3 characters,'// &
         ' TypeCoord='//TypeCoord)

    ! Normalize position and mapping radius if necessary
    if(index(TypeCoord,"NORM")>0)then
       Xyz_D = XyzIn_D
       rMap  = rMapIn
    else
       Xyz_D = XyzIn_D / RadiusPlanet
       rMap  = rMapIn  / RadiusPlanet
    end if

    ! Check if the mapping radius is outside of the planet
    if ( rMap < rNormLimit ) then
       write(*,*)NameSub,' mapping radius and coord type =',rMapIn,TypeCoord
       write(*,*)NameSub,' normalized mapping radius rMap=',rMap
       call CON_stop(NameSub// &
            ' SWMF_ERROR mapping radius is less than planet radius')
    end if

    ! Convert input position into MAG or SMG system
    DoConvert=.true.
    NameCoordSystem = TypeCoord(1:3)
    select case(NameCoordSystem)
    case('MAG','SMG')
       ! There is nothing to do
       DoConvert=.false.
    case ('GEO')
       ! Convert into MAG
       Convert_DD = MagGeo_DD
    case('GSM')
       ! Convert into SMG
       call set_axes(TimeSim)
       Convert_DD = SmgGsm_DD
    case('GSE')
       ! Convert into SMG
       call set_axes(TimeSim)
       Convert_DD = SmgGse_DD
    case default
       call CON_stop(NameSub//' SWMF_ERROR unimplemented NameCoordSystem='//&
            NameCoordSystem)
    end select

    if(DoConvert) Xyz_D = matmul(Convert_DD, Xyz_D)

    ! In MAG/SMG coordinates the hemisphere depends on the sign of Z
    iHemisphere = sign(1.0,Xyz_D(3))

    ! Normalized radial distance
    r = sqrt(sum(Xyz_D**2))

    ! Check if the point is outside the planet
    if ( r < rNormLimit ) then
       write(*,*)NameSub,' input position and coord type=',XyzIn_D,TypeCoord
       write(*,*)NameSub,' normalized radius r=',r
       call CON_stop(NameSub// &
            ' SWMF_ERROR radial distance is less than planet radius')
    end if

    ! Check if the mapping radius differs from the radius of input position
    if( abs(r-rMap) < DrNormLimit .and. .not.present(DdirDxyz_DD) ) then

       if(present(DoNotConvertBack) .and. DoConvert)then
          ! Output is the converted coordinates
          XyzMap_D = Xyz_D
          if( index(TypeCoord,"NORM")<=0 ) Xyzmap_D = Xyzmap_D * RadiusPlanet
       else
          ! Trivial mapping
          XyzMap_D = XyzIn_D
       end if
       ! The hemisphere has been established already
       RETURN

    end if

    ! Find the mapped position
    select case(TypeBField)
    case('DIPOLE')
       ! Solution of the vector potential equation
       ! The vector potential is proportional to (x^2+y^2)/r^3
       ! so sqrt(xMap^2+yMap^2)/sqrt(x^2+y^2) = sqrt(rMap^3/r^3)

       ! Calculate powers of the radii
       rMap2 = rMap**2
       rMap3 = rMap2*rMap
       r3    = r**3

       ! This is the ratio of input and mapped X and Y components
       XyRatio = sqrt(rMap3/r3)

       ! Calculate the X and Y components of the mapped position
       XyzMap_D(1:2) = XyRatio*Xyz_D(1:2)

       ! The squared distance of the mapped position from the magnetic axis
       XyMap2 = XyzMap_D(1)**2 + XyzMap_D(2)**2

       ! Check if there is a mapping at all
       if(rMap2 < XyMap2)then
          ! The point does not map to the given radius
          iHemisphere = 0

          ! Put mapped point to the magnetic equator
          XyzMap_D(1:2) = (rMap/sqrt(Xyz_D(1)**2 + Xyz_D(2)**2))*Xyz_D(1:2)
          XyzMap_D(3) = 0
       else
          ! Calculate the Z component of the mapped position
          ! Select the same hemisphere as for the input position
          XyzMap_D(3) = iHemisphere*sqrt(rMap2 - XyMap2)
       end if

       if(present(DdirDxyz_DD))then
          ! dTheta/dx = -xMap*(0.5-1.5*z^2/r^2)/(zMap*sqrt(x^2+y^2))
          ! dTheta/dy = -yMap*(0.5-1.5*z^2/r^2)/(zMap*sqrt(x^2+y^2))

          XyMap = sqrt(XyMap2)

          DdirDxyz_DD(1,1:2) = - XyzMap_D(1:2) * &
               ( 0.5 - 1.5 * (Xyz_D(3) / r)**2 ) / &
               ( XyzMap_D(3) * XyMap / XyRatio )

          ! dTheta/dz = - sqrt(xMap^2+yMap^2)/zMap*1.5*z/r^2
          DdirDxyz_DD(1,3) = - XyMap / XyzMap_D(3) * 1.5 * Xyz_D(3) / r**2

          ! dPhi/dx = -y/(x^2+y^2)
          ! dPhi/dy =  x/(x^2+y^2)
          Xy2              =   Xyz_D(1)**2 + Xyz_D(2)**2
          DdirDxyz_DD(2,1) = - Xyz_D(2) / Xy2
          DdirDxyz_DD(2,2) =   Xyz_D(1) / Xy2

          ! dPhi/dz = 0.0
          DdirDxyz_DD(2,3) = 0.0

          ! Transform into the system of the input coordinates
          ! dDir/dXyzIn = dDir/dXyzSMGMAG . dXyzSMGMAG/dXyzIn
          if(DoConvert) DdirDxyz_DD = matmul(DdirDxyz_DD, Convert_DD)

       endif

    case default
       call CON_stop(NameSub//' unimplemented TypeBField='//TypeBField)
    end select

    ! Convert position back to the input coordinate system if required
    if(.not.present(DoNotConvertBack) .and. DoConvert) &
         XyzMap_D = matmul(XyzMap_D, Convert_DD)

    ! Undo the normalization
    if( index(TypeCoord,"NORM")<=0 ) then
       Xyzmap_D = Xyzmap_D * RadiusPlanet
       if(present(DdirDxyz_DD)) DdirDxyz_DD = DdirDxyz_DD / RadiusPlanet
    end if

  end subroutine map_planet_field11
  !============================================================================
  subroutine map_planet_field33(TimeSim, xIn, yIn, zIn, TypeCoord, &
       rMap, xMap, yMap, zMap, iHemisphere, DoNotConvertBack, DdirDxyz_DD)

    real,              intent(in) :: TimeSim       ! simulation time
    real,              intent(in) :: xIn, yIn, zIn ! spatial position
    character(len=*),  intent(in) :: TypeCoord     ! type of coordinates
    real,              intent(in) :: rMap          ! radial distance to map to
    logical, optional, intent(in) :: DoNotConvertBack

    real,              intent(out):: xMap, yMap, zMap ! mapped position
    integer,           intent(out):: iHemisphere      ! mapped hemisphere
    real, optional,    intent(out):: DdirDxyz_DD(2,3) ! Jacobian matrix

    ! Interface to the map\_planet\_field11 routine with 3 scalars for both
    ! input and output positions

    ! local variables

    real :: XyzIn_D(3), XyzMap_D(3)
    !--------------------------------------------------------------------------
    XyzIn_D(1)=xIn; XyzIn_D(2)=yIn; XyzIn_D(3)=zIn

    call map_planet_field(TimeSim, XyzIn_D, TypeCoord, rMap, &
         XyzMap_D, iHemisphere, DoNotConvertBack, DdirDxyz_DD)

    xMap=XyzMap_D(1); yMap=XyzMap_D(2); zMap=XyzMap_D(3)

  end subroutine map_planet_field33
  !============================================================================
  subroutine map_planet_field_fast(XyzIn_D, rMap, XyzMap_D, iHemisphere, &
       DdirDxyz_DD, UseGsmIn, DoNotConvertBack)
    !$acc routine seq

    ! This subroutine is a simplified version of
    ! CON_planet_field.f90:map_planet_field11

    real,              intent(in) :: XyzIn_D(3)   ! spatial position
    real,              intent(in) :: rMap       ! radial distance to map to
    real,              intent(out):: XyzMap_D(3)      ! mapped position
    integer,           intent(out):: iHemisphere      ! which hemisphere
    real, optional,    intent(out):: DdirDxyz_DD(2,3) ! Jacobian matrix
    logical, optional, intent(in) :: UseGsmIn         ! use GSM coords
    logical, optional, intent(in) :: DoNotConvertBack ! Leave XyzMap in SMG/MAG

    real             :: Xyz_D(3)        ! Normalized and rotated position

    ! Temporary variables for the analytic mapping
    real :: rMap2, rMap3, r, r3, XyRatio, XyMap2, XyMap, Xy2

    logical :: UseGsm

    character(len=*), parameter:: NameSub = 'map_planet_field_fast'
    !--------------------------------------------------------------------------
    UseGsm = .false.
    if(present(UseGsmIn)) UseGsm = UseGsmIn

    if(UseGsm) then
       Xyz_D = matmul( SmgGsm_DD, XyzIn_D)
    else
       Xyz_D = XyzIn_D
    end if

    ! In MAG/SMG coordinates the hemisphere depends on the sign of Z
    iHemisphere = sign(1.0,Xyz_D(3))

    ! Normalized radial distance
    r = sqrt(sum(Xyz_D**2))

    ! Calculate powers of the radii
    rMap2 = rMap**2
    rMap3 = rMap2*rMap
    r3    = r**3

    ! This is the ratio of input and mapped X and Y components
    XyRatio = sqrt(rMap3/r3)

    ! Calculate the X and Y components of the mapped position
    XyzMap_D(1:2) = XyRatio*Xyz_D(1:2)

    ! The squared distance of the mapped position from the magnetic axis
    XyMap2 = XyzMap_D(1)**2 + XyzMap_D(2)**2

    ! Check if there is a mapping at all
    if(rMap2 < XyMap2)then
       ! The point does not map to the given radius
       iHemisphere = 0

       ! Put mapped point to the magnetic equator
       XyzMap_D(1:2) = (rMap/sqrt(Xyz_D(1)**2 + Xyz_D(2)**2))*Xyz_D(1:2)
       XyzMap_D(3) = 0
    else
       ! Calculate the Z component of the mapped position
       ! Select the same hemisphere as for the input position
       XyzMap_D(3) = iHemisphere*sqrt(rMap2 - XyMap2)
    end if

    if(.not.present(DoNotConvertBack) .and. UseGsm) &
         XyzMap_D = matmul(XyzMap_D, SmgGsm_DD )

    if(.not.present(DdirDxyz_DD)) RETURN

    XyMap = sqrt(XyMap2)

    DdirDxyz_DD(1,1:2) = - XyzMap_D(1:2) * &
         ( 0.5 - 1.5 * (Xyz_D(3) / r)**2 ) / &
         ( XyzMap_D(3) * XyMap / XyRatio )

    ! dTheta/dz = - sqrt(xMap^2+yMap^2)/zMap*1.5*z/r^2
    DdirDxyz_DD(1,3) = - XyMap / XyzMap_D(3) * 1.5 * Xyz_D(3) / r**2

    ! dPhi/dx = -y/(x^2+y^2)
    ! dPhi/dy =  x/(x^2+y^2)
    Xy2              =   Xyz_D(1)**2 + Xyz_D(2)**2
    DdirDxyz_DD(2,1) = - Xyz_D(2) / Xy2
    DdirDxyz_DD(2,2) =   Xyz_D(1) / Xy2

    ! dPhi/dz = 0.0
    DdirDxyz_DD(2,3) = 0.0

    ! Transform into the system of the input coordinates
    ! dDir/dXyzIn = dDir/dXyzSMGMAG . dXyzSMGMAG/dXyzIn
    if(UseGsm) DdirDxyz_DD = matmul(DdirDxyz_DD, SmgGsm_DD)

  end subroutine map_planet_field_fast
  !============================================================================
  subroutine test_planet_field

    ! Test the methods in this class.

    real :: TimeSim
    real :: xSmg_D(3), xGsm_D(3), xGse_D(3), bSmg_D(3), bGsm_D(3), bGse_D(3)
    real :: x_D(3), rMap, xMap_D(3)
    integer :: iHemisphere
    type(TimeType):: TimeEquinox
    !--------------------------------------------------------------------------
    write(*,*)
    write(*,'(a)')'TEST GET_PLANET_FIELD'
    write(*,*)

    TimeEquinox = TimeType(2000, 3, 20, 7, 35, 0, 0.0, 0.0_Real8_, '')
    call time_int_to_real(TimeEquinox)
    write(*,*) 'TimeEquinox=', TimeEquinox
    call init_axes(TimeEquinox % Time)

    xSmg_D = [1.0, 1.0, 0.1]
    write(*,'(a,3f5.0)')'Location xSmg_D = ',xSmg_D
    call get_planet_field(0.0,xSmg_D,'SMG NORM',bSmg_D)
    write(*,'(a,3es14.6)')'Field    bSmg_D = ',bSmg_D
    write(*,*)
    call get_planet_field(0.0,xSmg_D*RadiusPlanet,'SMG',bSmg_D)
    write(*,'(a,3es14.6)')'Field    bSmg_D = ',bSmg_D
    write(*,*)
    xGsm_D = matmul(xSmg_D,SmgGsm_DD)
    write(*,'(a,3es14.6)')'Location xGsm_D =',xGsm_D
    call get_planet_field(0.0,xGsm_D,'GSM NORM',bGsm_D)
    write(*,'(a,3es14.6)')'Field    bGsm_D = ',bGsm_D
    write(*,'(a,3es14.6)')'Rotated  bGsm_D = ',matmul(SmgGsm_DD,bGsm_D)
    write(*,*)
    xGse_D = matmul(xGsm_D,GsmGse_DD)
    write(*,'(a,3es14.6)')'Location xGse_D =',xGse_D
    call get_planet_field(0.0,xGse_D,'GSE NORM',bGse_D)
    write(*,'(a,3es14.6)')'Field    bGse_D = ',bGse_D
    write(*,'(a,3es14.6)')'Rotated  bGse_D = ', &
         matmul(SmgGsm_DD,matmul(GsmGse_DD,bGse_D))

    TimeSim = 6*3600
    write(*,'(a,3es14.6)')'Test at time=', TimeSim
    call get_planet_field(TimeSim,xSmg_D,'SMG NORM',bSmg_D)
    write(*,'(a,3es14.6)')'Field    bSmg_D = ',bSmg_D
    call get_planet_field(TimeSim,xGsm_D,'GSM NORM',bGsm_D)
    write(*,'(a,3es14.6)')'Field    bGsm_D = ',bGsm_D

    write(*,*)
    write(*,'(a)')'TEST MAP_PLANET_FIELD'
    write(*,*)
    X_D=[6.0, -8.0, -0.0001]
    rMap  = 1.0
    write(*,'(a,4es14.6)')'x_D,rMap=',x_D,rMap
    call map_planet_field(0.0,x_D,'SMG NORM',rMap,xMap_D,iHemisphere)
    write(*,'(a,3es14.6,i3)')'xMap_D,iHemisphere=',xMap_D,iHemisphere

  end subroutine test_planet_field
  !============================================================================
end module CON_planet_field
!==============================================================================
