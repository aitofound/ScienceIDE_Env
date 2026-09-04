!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module CON_planet

  ! Physical information about the planet. The planet is described
  ! with its name. Default values can be set with {\bf planet\_init}.
  ! Simplifying assumptions, such as no rotation, aligned magnetic
  ! and rotational axes etc. can be made.
  !
  ! This is a public class. The variables should be modified by CON only.
  ! Components can only access the data through the inquiry methods
  ! via the {\bf CON\_physics} class.

  use ModNumConst, ONLY: cPi, cHalfPi, cTwoPi, cDegToRad, cTiny, cUnit_DD
  use ModPlanetConst
  use ModTimeConvert, ONLY: TimeType, time_int_to_real
  use ModUtilities, ONLY: upper_case, CON_stop

  ! revision history:
  ! 01Aug03 - Aaron Ridly <ridley@umich.edu> and
  !           Gabor Toth <gtoth@umich.edu>   - initial prototype/prolog/code
  ! 23Mar03 - added get_planet subroutine for OO type access
  ! 06May04 - K.C. Hansen and G. Toth added Saturn
  !           G.Toth fixed bugs in degree to radian conversions

  implicit none

  save

  character(len=*), parameter, private :: NameMod='CON_planet'

  character (len=lNamePlanet) :: NamePlanet = ''

  logical :: IsInitializedPlanet = .false.

  ! Initial time in 8 byte real
  real(Real8_) :: tStart = -1.0
  !$acc declare create(tStart)

  ! Desired GEO subsolar longitude at the start of the simulation.
  real :: MeridianGeo = -1.0
  ! Angle between GEI and GEO X axes at tStart.
  real :: AngleGeiGeoStart = -1.0

  ! Define variables
  real:: RadiusPlanet
  real:: MassPlanet
  real:: IonosphereHeight
  real:: OmegaRotation   ! Average angular speed relative to Sun/Star
  real:: OmegaPlanet     ! OmegaRotation + OmegaOrbit (inertial omega)
  real:: RotPeriodPlanet ! 2*pi/OmegaPlanet (inertial period)
  real:: OmegaOrbit      ! Average angular speed of orbital motion

  ! Orbit description
  logical:: IsOrbitSet = .false.
  type(OrbitType) :: Orbit

  !$acc declare create(OmegaPlanet, OmegaRotation)

  ! Magnetic field type and strength in teslas
  character (len=lTypeBField) :: TypeBField = 'DIPOLE'
  real                        :: DipoleStrength
  !$acc declare create(DipoleStrength)
  real    :: MagAxisThetaGeo  ! Permanent theta  in GEO
  real    :: MagAxisPhiGeo    ! Permanent phi    in GEO

  ! Orientation of the axes
  real    :: RotAxisTheta      ! Permanent theta angle in GSE
  real    :: RotAxisPhi        ! Permanent phi   angle in GSE
  real    :: MagAxisTheta      ! Current   theta angla in GSE
  real    :: MagAxisPhi        ! Current   phi   angla in GSE
  !$acc declare create(MagAxisTheta, MagAxisPhi)

  ! Offset of the magnetic field center
  real    :: MagCenter_D(3) = [ 0.0, 0.0, 0.0]
  !$acc declare create(MagCenter_D)

  ! Optional changes relative to the "real" planet
  logical :: UseRotation     = .true.
  logical :: UseAlignedAxes  = .false.
  logical :: UseRealRotAxis  = .true.
  logical :: UseSetRotAxis   = .false.
  logical :: UseRealMagAxis  = .true.
  logical :: UseSetMagAxis   = .false.
  !$acc declare create(UseRotation)

  ! Frequency of updating the magnetic field information
  logical :: DoUpdateB0      = .true.
  real    :: DtUpdateB0      = 0.0001
  !$acc declare create(DoUpdateB0, DtUpdateB0)

  ! A primary axis is set to the true value
  ! a secondary axis is aligned with the primary axis
  logical :: IsRotAxisPrimary = .true., IsMagAxisPrimary = .true.

  ! A logical to indicate it the default parameters have been
  ! modified for this planet
  logical :: IsPlanetModified = .false.

  ! Variables added for the multipole option for the magnetic field
  logical :: UseMultipoleB0 = .false.
  integer :: MaxHarmonicDegree = -1
  character(len=200) :: NamePlanetHarmonicsFile
  real, allocatable :: gPlanet_II(:, :), hPlanet_II(:, :)

  ! Legacy Orbit parameters
  real :: rOrbitPlanet    ! [m]
  !$acc declare create(rOrbitPlanet)

contains
  !============================================================================
  subroutine set_planet_defaults

    ! Initialize parameters for Earth as the default planet in case
    ! there is no \#PLANET command.

    character(len=*), parameter:: NameSub = 'set_planet_defaults'
    !--------------------------------------------------------------------------
    if(.not.is_planet_init("EARTH")) call CON_stop(NameSub // &
         ': failed to initialize with Earth')

    ! Allow switching to a different planet
    IsInitializedPlanet = .false.

  end subroutine set_planet_defaults
  !============================================================================
  function is_planet_init(NamePlanetIn) result(IsKnown)

    character(len=*), intent(in) :: NamePlanetIn

    ! return value
    logical :: IsKnown

    ! Initialize parameters for the planet identified by its name and
    ! return true if the planet is known. If it is not known return false.
    ! Store the name in either case. The planet data can be initialized at most
    ! once.
    integer :: i

    character(len=*), parameter:: NameSub = 'is_planet_init'
    !--------------------------------------------------------------------------
    IsKnown = .true.
    if(IsInitializedPlanet)then
       if(NamePlanet == NamePlanetIn) RETURN
       call CON_stop(NameSub//&
            ' ERROR: attempt to change planet name from '// &
            trim(NamePlanet)//' to '//NamePlanetIn)
    end if

    NamePlanet = NamePlanetIn; call upper_case(NamePlanet)
    IsInitializedPlanet = .true.

    ! Reset optional start-time subsolar longitude override.
    MeridianGeo = -1.0
    ! Reset GEI/GEO angle so it is recalculated for the new planet.
    AngleGeiGeoStart = -1.0

    IsKnown = .false.
    do i = NoPlanet_, MaxPlanet
       if (NamePlanet == NamePlanet_I(i)) then
          IsKnown = .true.
          iPlanet = i
          Planet_ = i
          EXIT
       end if
    end do

    if (.not. IsKnown)  then
       iPlanet    = NewPlanet_
       Planet_    = NewPlanet_
       NamePlanet = NamePlanetIn
    end if

    ! Set all values for the selected planet
    RadiusPlanet     = rPlanet_I(iPlanet)
    MassPlanet       = MassPlanet_I(iPlanet)
    IonosphereHeight = IonoHeightPlanet_I(iPlanet)

    rOrbitPlanet = OrbitJ2k_I(iPlanet) % aAu * cAU
    OmegaOrbit   = dOrbitJ2k_I(iPlanet) % MeanLonDeg*cDegToRad/cCentury

    OmegaPlanet   = dRotationIcrf_I(iPlanet) % WDeg*cDegToRad/cDay
    OmegaRotation = OmegaPlanet - OmegaOrbit
    if (OmegaPlanet /= 0.0) then
       RotPeriodPlanet = cTwoPi/OmegaPlanet
    else
       RotPeriodPlanet = 0.0
    end if

    ! Magnetic field type and strength in teslas
    TypeBField        = TypeBFieldPlanet_I(iPlanet)
    DipoleStrength    = DipoleStrengthPlanet_I(iPlanet)
    MagAxisThetaGeo   = bAxisThetaPlanet_I(iPlanet)  ! Permanent theta  in GEO
    MagAxisPhiGeo     = bAxisPhiPlanet_I(iPlanet)    ! Permanent phi    in GEO

    ! For Enceladus the dipole is at Saturn's center
    if(iPlanet == Enceladus_) MagCenter_D(2) = 944.23

    !$acc update device(OmegaPlanet, OmegaRotation,&
    !$acc MagAxisPhi, MagAxisTheta, rOrbitPlanet)

  end function is_planet_init
  !============================================================================
  subroutine read_planet_var(NameCommand)

    use ModUtilities, ONLY: upper_case
    use ModReadParam, ONLY: read_var, lStringLine
    use ModIoUnit, ONLY: UnitTmp_

    character (len=*), intent(in) :: NameCommand

    ! Planet related temporary variables
    character (len=lNamePlanet) :: NamePlanetIn
    character (len=lStringLine) :: NamePlanetCommands=''
    logical :: UseNonDipole

    integer :: m, n, m1, n1
    character(len=100) :: StringHeader

    character(len=*), parameter:: NameSub = 'read_planet_var'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case("#PLANET", "#MOON", "#COMET")
       call read_var('NamePlanet',NamePlanetIn)
       call upper_case(NamePlanetIn)

       ! Check if planet has been already initialized
       if(NamePlanet == NamePlanetIn) RETURN

       if (NamePlanetCommands /= '') &
            call CON_stop(NameSub// &
            ' ERROR: #PLANET should precede '// &
            NamePlanetCommands)

       ! If planet has not been initialized ...
       if ( .not. is_planet_init(NamePlanetIn) ) then
          call read_var('RadiusPlanet', RadiusPlanet)
          call read_var('MassPlanet',   MassPlanet)
          call read_var('OmegaPlanet',  OmegaPlanet)
          if (OmegaPlanet /= 0.0) then
             RotPeriodPlanet = cTwoPi/OmegaPlanet
             call read_var('RotAxisTheta', RotAxisTheta)
             RotAxisTheta = RotAxisTheta*cDegToRad
             if(RotAxisTheta /= 0.0)then
                call read_var('RotAxisPhi', RotAxisPhi)
                RotAxisPhi = RotAxisPhi*cDegToRad
             end if
          else
             RotPeriodPlanet = 0.0
             RotAxisTheta    = 0.0
             RotAxisPhi      = -cHalfPi ! equinox-like value
          end if
          UseSetRotAxis  = .true.
          UseRealRotAxis = .false.
          UseSetMagAxis  = .true.
          UseRealMagAxis = .false.

          call read_var('TypeBField',   TypeBField)
          call upper_case(TypeBField)

          select case(TypeBField)
          case('NONE')
             MagAxisTheta   = 0.0
             MagAxisPhi     = 0.0
             DipoleStrength = 0.0

          case('DIPOLE')
             call read_var('MagAxisTheta', MagAxisTheta)
             MagAxisTheta = MagAxisTheta * cDegToRad
             if(MagAxisTheta /= 0.0) then
                call read_var('MagAxisPhi', MagAxisPhi)
                MagAxisPhi = MagAxisPhi * cDegToRad
             else
                MagAxisPhi = 0.0
             end if
             call read_var('DipoleStrength', DipoleStrength)

          case default
             call CON_stop(NameSub// &
                  ' ERROR: TypeBfield not specified for planet.'//TypeBField)

          end select
       end if
       !$acc update device(OmegaPlanet)
    case('#IDEALAXES')
       ! This is a short version of setting one axis parallel with Z
       ! and the other one aligned with it
       NamePlanetCommands = '#IDEALAXES ' // NamePlanetCommands
       IsPlanetModified = .true.

       UseRealRotAxis   = .false.
       IsRotAxisPrimary = .true.
       UseSetRotAxis    = .true.
       RotAxisTheta     = 0.0
       RotAxisPhi       = 0.0
       UseRealMagAxis   = .false.
       IsMagAxisPrimary = .false.
       UseSetMagAxis    = .false.

    case('#ROTATIONAXIS')
       NamePlanetCommands = '#ROTATIONAXIS ' // NamePlanetCommands
       IsPlanetModified = .true.
       UseRealRotAxis = .false.
       UseRealMagAxis = .false. ! Cannot use real mag axis

       call read_var('IsRotAxisPrimary', IsRotAxisPrimary)
       if (IsRotAxisPrimary) then

          UseSetRotAxis = .true.

          call read_var('RotAxisTheta', RotAxisTheta)
          if(RotAxisTheta < 0.0)call CON_stop(NameSub// &
               ' ERROR: negative tilt should be entered as Phi=180.0')
          RotAxisTheta = cDegToRad * RotAxisTheta

          call read_var('RotAxisPhi', RotAxisPhi)
          RotAxisPhi = cDegToRad * RotAxisPhi
       else
          if(.not.IsMagAxisPrimary)call CON_stop(NameSub// &
               ' ERROR: either rotation or magnetic axis must be primary')
       end if

    case('#MAGNETICAXIS')
       NamePlanetCommands = '#MAGNETICAXIS ' // NamePlanetCommands
       IsPlanetModified = .true.
       UseRealMagAxis = .false.

       call read_var('IsMagAxisPrimary', IsMagAxisPrimary)
       if (IsMagAxisPrimary) then

          UseSetMagAxis = .true.

          call read_var('MagAxisTheta', MagAxisTheta)
          if(MagAxisTheta < 0.0)call CON_stop(NameSub// &
               ' ERROR: negative tilt should be entered as Phi=180.0')
          MagAxisTheta = cDegToRad * MagAxisTheta

          call read_var('MagAxisPhi', MagAxisPhi)
          MagAxisPhi = cDegToRad * MagAxisPhi
       else
          if(.not.IsRotAxisPrimary)call CON_stop(NameSub//&
               ' ERROR: either rotation or magnetic axis must be primary')
       end if

    case('#MAGNETICCENTER')
       NamePlanetCommands = '#MAGNETICCENTER ' // NamePlanetCommands
       IsPlanetModified = .true.
       call read_var('MagneticCenterX',MagCenter_D(1))
       call read_var('MagneticCenterY',MagCenter_D(2))
       call read_var('MagneticCenterZ',MagCenter_D(3))

    case('#MERIDIAN')
       NamePlanetCommands = '#MERIDIAN ' // NamePlanetCommands
       IsPlanetModified = .true.

       ! Set the GEO subsolar longitude at the start of the simulation.
       call read_var('MeridianGeo', MeridianGeo)
       MeridianGeo = modulo(MeridianGeo*cDegToRad, cTwoPi)

    case('#ROTATION')
       NamePlanetCommands = '#ROTATION ' // NamePlanetCommands
       IsPlanetModified = .true.

       call read_var('UseRotation', UseRotation)
       if (.not.UseRotation) then
          OmegaPlanet     = 0.0
          RotPeriodPlanet = 0.0
       else
          call read_var('Rotation period [hours]',  RotPeriodPlanet)
          RotPeriodPlanet = RotPeriodPlanet * 3600
          OmegaPlanet = cTwoPi / RotPeriodPlanet
       endif
       !$acc update device(UseRotation, OmegaPlanet)
    case('#NONDIPOLE')
       NamePlanetCommands = '#NONDIPOLE ' // NamePlanetCommands
       IsPlanetModified = .true.

       call read_var('UseNonDipole',UseNonDipole)
       if (.not.UseNonDipole) then
          TypeBField = 'DIPOLE'
       else
          call CON_stop(NameSub// &
               ' ERROR: nondipole magnetic field unimplemented')
       endif

    case('#DIPOLE')
       NamePlanetCommands = '#DIPOLE ' // NamePlanetCommands
       IsPlanetModified = .true.

       call read_var('DipoleStrength',DipoleStrength)
       if(DipoleStrength ==0.0)then
          TypeBField="NONE"
       else
          TypeBField="DIPOLE"
       end if

    case('#UPDATEB0')
       call read_var('DtUpdateB0',DtUpdateB0)

    case('#MULTIPOLEB0')
       call read_var('UseMultipoleB0', UseMultipoleB0)
       if(UseMultipoleB0) then
          NamePlanetCommands = '#MULTIPOLEB0 ' // NamePlanetCommands
          IsPlanetModified = .true.

          ! If multipole is used, the magnetic axis is aligned
          ! with the rotation axis. Rotation axis may be real or
          ! user-specified using #ROTATIONAXIS command.
          UseRealRotAxis   = .true.
          IsRotAxisPrimary = .true.
          UseRealMagAxis   = .false.
          IsMagAxisPrimary = .false.
          UseSetMagAxis    = .false.

          ! Set multipole properties
          TypeBField="MULTIPOLE"
          call read_var('MaxHarmonicDegree', MaxHarmonicDegree)
          call read_var('NamePlanetHarmonicsFile', NamePlanetHarmonicsFile)

          if(.not.allocated(gPlanet_II) .and. .not.allocated(hPlanet_II)) then
             allocate(gPlanet_II(0:MaxHarmonicDegree, 0:MaxHarmonicDegree))
             allocate(hPlanet_II(0:MaxHarmonicDegree, 0:MaxHarmonicDegree))
          end if

          ! Read in the coefficients from the file
          open(UnitTmp_, file=NamePlanetHarmonicsFile, status='old')
          read(UnitTmp_, *) StringHeader
          do n = 0, MaxHarmonicDegree
             do m = 0, n
                ! Read data from the file, default fortran format
                read(UnitTmp_, *) &
                     n1, m1, gPlanet_II(n, m), hPlanet_II(n, m)
             end do
          end do
          close(UnitTmp_)

          call normalize_schmidt_coefficients
       end if
    case('#ORBIT')
       NamePlanetCommands = '#ORBIT ' // NamePlanetCommands
       IsPlanetModified = .true.
       IsOrbitSet = .true.
       ! Use HGI (instead of J2000 and ICRF) for orbit and rotation
       HgiJ2k_DD  = cUnit_DD
       HgiIcrf_DD = cUnit_DD
       J2kIcrf_DD = cUnit_DD
       call read_var('OrbitalPeriodPlanet', OmegaOrbit)
       if(OmegaOrbit /= 0.0) OmegaOrbit = cTwoPi/OmegaOrbit
       ! Correct omega planet?
       call read_var('rOrbitPlanet',  Orbit % aAu) ! [m]
       Orbit % aAu = Orbit % aAu/cAU
       call read_var('Eccentricity',  Orbit % Eccentricity)
       call read_var('Inclination',   Orbit % InclinationDeg) ! [deg]
       call read_var('MeanLongitude', Orbit % MeanLonDeg)     ! [deg]
       call read_var('LongitudePeri', Orbit % LonPeriDeg)     ! [deg]
       call read_var('LongitudeNode', Orbit % LonNodeDeg)     ! [deg]

    case default
       call CON_stop(NameSub//': unknown command='//NameCommand)
    end select

  end subroutine read_planet_var
  !============================================================================
  subroutine check_planet_var(IsProc0, DoTimeAccurate)

    logical, intent(in) :: IsProc0, DoTimeAccurate

    ! The rotation and magnetic axes are aligned if any of them is not a
    ! primary axis or if multipoleB0 is used (implicitly).
    character(len=*), parameter:: NameSub = 'check_planet_var'
    !--------------------------------------------------------------------------
    UseAlignedAxes = (.not. IsRotAxisPrimary) .or. (.not. IsMagAxisPrimary)

    ! Warn if setting is unphysical
    if(UseSetMagAxis .and. UseRealRotAxis .and. IsProc0) &
         write(*,*)NameSub,' WARNING: magnetic axis is explicitly set ',&
         'while rotation axis is calculated from real time ?!'

    ! Check if there is a need to update the magnetic field
    DoUpdateB0 = DtUpdateB0 > 0.0 .and. DoTimeAccurate .and. UseRotation &
         .and. .not.(UseAlignedAxes .and. .not.UseMultipoleB0)

    ! Multipole B0 uses IdealAxes to set the proxy mag-rot axes,
    ! so it needs DoUpdateB0 to be True even when IdealAxes is used.
    ! If dipole is used with IdealAxes, no update is needed.

    !$acc update device(DoUpdateB0, DtUpdateB0)

  end subroutine check_planet_var
  !============================================================================
  subroutine get_planet( &
       NamePlanetOut, RadiusPlanetOut, MassPlanetOut, OmegaPlanetOut, &
       RotationPeriodOut, IonosphereHeightOut, &
       UseRotationOut, DipoleStrengthOut, DoUpdateB0Out, DtUpdateB0Out)

    character(len=*), optional, intent(out) :: NamePlanetOut
    real,             optional, intent(out) :: RadiusPlanetOut
    real,             optional, intent(out) :: MassPlanetOut
    real,             optional, intent(out) :: OmegaPlanetOut
    real,             optional, intent(out) :: RotationPeriodOut
    real,             optional, intent(out) :: IonosphereHeightOut
    logical,          optional, intent(out) :: UseRotationOut
    real,             optional, intent(out) :: DipoleStrengthOut
    logical,          optional, intent(out) :: DoUpdateB0Out
    real,             optional, intent(out) :: DtUpdateB0Out
    !--------------------------------------------------------------------------
    if(present(NamePlanetOut))      NamePlanetOut       = NamePlanet
    if(present(RadiusPlanetOut))    RadiusPlanetOut     = RadiusPlanet
    if(present(MassPlanetOut))      MassPlanetOut       = MassPlanet
    if(present(OmegaPlanetOut))     OmegaPlanetOut      = OmegaPlanet
    if(present(RotationPeriodOut))  RotationPeriodOut   = RotPeriodPlanet
    if(present(IonosphereHeightOut))IonosphereHeightOut = IonosphereHeight
    if(present(UseRotationOut))     UseRotationOut      = UseRotation
    if(present(DipoleStrengthOut))  DipoleStrengthOut   = DipoleStrength
    if(present(DoUpdateB0Out))      DoUpdateB0Out       = DoUpdateB0
    if(present(DtUpdateB0Out))      DtUpdateB0Out       = DtUpdateB0

  end subroutine get_planet
  !============================================================================
  subroutine normalize_schmidt_coefficients

    ! Instead of normalizing the Legendre polynomials, from Gauss to Schmidt
    ! -semi-normalized, we normalize the Schmidt coefficients instead for
    ! a more efficient calculation. We only need to do this once at the
    ! beginning of the run.

    integer :: m, n
    real, allocatable :: s_II(:,:)
    !--------------------------------------------------------------------------
    if(.not.allocated(s_II)) &
         allocate(s_II(0:MaxHarmonicDegree,0:MaxHarmonicDegree))

    s_II = 1.0

    do n = 1, MaxHarmonicDegree
       s_II(n,0) = s_II(n-1,0)*(2*n - 1.0)/n
       s_II(n,1) = s_II(n,0)*sqrt(2*n/(n + 1.0))
       gPlanet_II(n,0) = gPlanet_II(n,0)*s_II(n,0)
       hPlanet_II(n,0) = hPlanet_II(n,0)*s_II(n,0)
       gPlanet_II(n,1) = gPlanet_II(n,1)*s_II(n,1)
       hPlanet_II(n,1) = hPlanet_II(n,1)*s_II(n,1)
       do m = 2, n
          s_II(n,m) = s_II(n,m-1) * sqrt((n - m + 1.0)/(n + m))
          gPlanet_II(n,m) = gPlanet_II(n,m)*s_II(n,m)
          hPlanet_II(n,m) = hPlanet_II(n,m)*s_II(n,m)
       end do
    end do

    deallocate(s_II)

  end subroutine normalize_schmidt_coefficients
  !============================================================================
  subroutine orbit_in_hgi(TimeSim, XyzHgi_D, vHgi_D)

    ! Calculate location and velocity at current Time in HGI coordinate system

    real, intent(in):: TimeSim
    real,intent(out):: XyzHgi_D(3)
    real, optional, intent(out) :: vHgi_D(3)

    real :: a, Ecc, Inc, OmegaNode, LongPeri, Lon
    real :: EAnom, dEAnom, CosEAnom, SinEAnom, dEdt
    real :: b
    real :: xOrb, yOrb, VxOrb, VyOrb
    real :: CosO, SinO, CosI, SinI, CosW, SinW
    real :: P_D(3), Q_D(3)
    integer :: iIter

    character(len=*), parameter:: NameSub = 'orbit_in_hgi'
    !--------------------------------------------------------------------------
    a         = Orbit%aAu*cAU
    Ecc       = Orbit%Eccentricity
    Inc       = Orbit%InclinationDeg*cDegToRad
    OmegaNode = Orbit%LonNodeDeg*cDegToRad
    LongPeri  = Orbit%LonPeriDeg*cDegToRad
    ! Get the mean longitude based on mean angular speed
    Lon = modulo((Orbit%MeanLonDeg - Orbit%LonPeriDeg)*cDegToRad &
         + OmegaOrbit*TimeSim, cTwoPi)
    if(Lon > cPi) Lon = Lon - cTwoPi

    ! Initialize the true anomaly (=true longitude)
    if(Ecc < 0.8)then
       EAnom = Lon
    else
       EAnom = sign(cPi, Lon)
    end if
    ! Iterative Kepler solver for true anomaly (true longitude)
    do iIter = 1, 12
       dEAnom = (EAnom - Ecc*sin(EAnom) - Lon)/max(1 - Ecc*cos(EAnom), cTiny)
       EAnom = EAnom - dEAnom
       if(abs(dEAnom) < cTiny) EXIT
    end do

    CosEAnom = cos(EAnom); SinEAnom = sin(EAnom)
    b = a*sqrt(max(1 - Ecc**2, 0.0)) ! minor axis
    xOrb = a*(CosEAnom - Ecc)        ! position along major axis direction
    yOrb = b*SinEAnom                ! position along minor axis direction

    CosO = cos(OmegaNode); SinO = sin(OmegaNode)
    CosI  = cos(Inc);       SinI  = sin(Inc)
    CosW  = cos(LongPeri - OmegaNode)
    SinW  = sin(LongPeri - OmegaNode)

    ! Unit vector towards periapsis
    P_D = [CosO*CosW - SinO*SinW*CosI, SinO*CosW + CosO*SinW*CosI, &
         SinW*SinI]
    ! Unit vector perpendicular to periapsis in the orbital plane
    Q_D = [-CosO*SinW - SinO*CosW*CosI, -SinO*SinW + CosO*CosW*CosI, &
         CosW*SinI]

    ! Location in HGI
    XyzHgi_D = xOrb*P_D + yOrb*Q_D
    XyzHgi_D = matmul(HgiJ2k_DD, XyzHgi_D)

    if(present(vHgi_D))then
       dEdt   = OmegaOrbit/max(1.0 - Ecc*CosEAnom, cTiny)
       VxOrb  = -a*SinEAnom*dEdt
       VyOrb  =  b*CosEAnom*dEdt
       vHgi_D = VxOrb*P_D + VyOrb*Q_D
       vHgi_D = matmul(HgiJ2k_DD, vHgi_D)
    end if

  end subroutine orbit_in_hgi
  !============================================================================
  subroutine get_rotation_axis_hgi(AxisHgi_D)

    real, intent(out):: AxisHgi_D(3)

    type(RotationType) :: Rot
    real :: Alpha, Delta
    !--------------------------------------------------------------------------
    call get_planet_rotation_elements(tStart, Rot)
    Alpha = Rot % AlphaDeg*cDegToRad
    Delta = Rot % DeltaDeg*cDegToRad

    AxisHgi_D = matmul(HgiIcrf_DD, &
         [cos(Delta)*cos(Alpha), cos(Delta)*sin(Alpha), sin(Delta)])

  end subroutine get_rotation_axis_hgi
  !============================================================================
  subroutine get_gei_geo_angle(Angle)

    ! Set angle between GEI and GEO systems at start time

    real, intent(out) :: Angle

    type(RotationType) :: Rot
    real :: Alpha, Delta, Incl, Node, EquinoxNorm, GeiOffset
    real :: PoleIcrf_D(3), OrbitJ2k_D(3), OrbitIcrf_D(3)
    real :: IcrfNode_D(3), Equinox_D(3)
    !--------------------------------------------------------------------------
    call get_planet_rotation_elements(tStart, Rot)

    ! Calculate offset angle between ICRF 0 longitude and GEI 0 longitude
    Alpha = Rot%AlphaDeg*cDegToRad
    Delta = Rot%DeltaDeg*cDegToRad
    Incl  = Orbit%InclinationDeg*cDegToRad
    Node  = Orbit%LonNodeDeg*cDegToRad

    ! Pole direction in ICRF/J2000 equatorial coordinates
    PoleIcrf_D = [cos(Delta)*cos(Alpha), cos(Delta)*sin(Alpha), sin(Delta)]

    ! Orbit normal from J2000 ecliptic elements, converted to ICRF
    OrbitJ2k_D = [sin(Node)*sin(Incl), -cos(Node)*sin(Incl), cos(Incl)]
    OrbitIcrf_D = matmul(OrbitJ2k_D, J2kIcrf_DD)

    ! GEI x-axis is the planet's vernal equinox direction on the equator
    Equinox_D = cross_product(PoleIcrf_D, OrbitIcrf_D)
    EquinoxNorm = norm2(Equinox_D)
    if(EquinoxNorm < cTiny)then
       ! If the equinox direction is singular, set the offset to pi.
       GeiOffset = cPi
    else
       Equinox_D = Equinox_D/EquinoxNorm

       ! IAU node direction on the ICRF equator
       IcrfNode_D = [-sin(Alpha), cos(Alpha), 0.0]

       ! Exact signed angle from IAU node (0 longitude) to the GEI x-axis.
       ! The cPi is due to GEO having 0 at midnight while the rotation
       ! parameters define the angle for the noon meridian.
       ! Make sure GeiOffset is non-negative
       GeiOffset = modulo( cPi + atan2( &
            dot_product(cross_product(IcrfNode_D, Equinox_D), PoleIcrf_D), &
            dot_product(IcrfNode_D, Equinox_D)), cTwoPi)
    end if

    ! Make sure Angle is non-negative
    Angle = modulo(Rot%WDeg*cDegToRad - GeiOffset, cTwoPi)

  end subroutine get_gei_geo_angle
  !============================================================================
end module CON_planet
!==============================================================================
