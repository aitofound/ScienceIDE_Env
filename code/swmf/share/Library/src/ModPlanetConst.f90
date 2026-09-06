!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModPlanetConst

  use ModNumConst, ONLY: cDegToRad, cRadToDeg, cPi, cTwoPi, cHalfPi, cTiny
  use ModConst, ONLY: cAU, cHour => cSecondPerHour, cDay => cSecondPerDay, &
       cCentury => cSecondPerCentury
  use ModCoordTransform, ONLY: rot_matrix_x, rot_matrix_z, show_rot_matrix, &
       cross_product
  use ModTimeConvert, ONLY: TimeType, time_int_to_real
  use ModKind

  implicit none

  save

  ! All astronomical bodies other than the Sun are defined below.
  ! Solar constants are defined in ModConst.
  !
  ! The maximum number of astronomical bodies.  This is set at 200, it can be
  ! increased if necessary

  type(TimeType):: TimeJ2k = &
       TimeType(2000, 1, 1, 12, 0, 0, 0.0, 0.0_REAL8_, '')

  integer, parameter:: MaxPlanet = 200

  integer, parameter:: lNamePlanet = 40
  integer, parameter:: lTypeBField = 40

  real, parameter:: DayPerCentury  = 36525.0

  ! Rotation angle between J2K and Icrf is Earth inclination at J2000 epoch
  real, parameter:: InclJ2k = 23.4392911*cDegToRad
  ! Conversion matrix from equatorial ICRF to ecliptic J2000 = rot_x(-InclJ2k)
  real:: J2kIcrf_DD(3,3)
  ! Conversion matrix from ecliptic J2000 to HGI coordinates
  real:: HgiJ2k_DD(3,3)
  ! Conversion matrix from equatorial ICRF to HGI coordinates
  real:: HgiIcrf_DD(3,3)

  type OrbitType
     real :: aAu            ! Semi major axis measured in [au]
     real :: Eccentricity   ! Eccentricity of orbit (0 for circular)
     real :: InclinationDeg ! Inclination of orbit in the  reference frame
     real :: MeanLonDeg     ! Longitude of planet for constant angular speed
     real :: LonPeriDeg     ! Longitude of periapsis (if Eccentricity /= 0)
     real :: LonNodeDeg     ! Longiude of the node (if Inclination /= 0)
  end type OrbitType

  type RotationType
     real :: AlphaDeg  ! longirude of rotation axis in reference frame
     real :: DeltaDeg  ! latiude of rotation axis in reference frame
     real :: WDeg      ! direction of meridian in ICRF frame
  end type RotationType

  ! Declarations for the variables that we are storing to define each body.
  !
  ! NOTE THAT ALL VARIABLES IN THIS FILE SHOULD BE IN SI UNITS.
  !  (m,s,kg,m/s, ... )
  !
  ! NOTE THE THE PRECISE DEFINITIONS OF WHAT THE VARIABLES MEAN CAN BE
  ! FOUND AT THE END OF THE FILE AND IN THE CODE DOCUMENTATION (we hope).

  ! Radius, mass, and simplified orbital parameters for moons
  real, dimension(0:MaxPlanet+1):: &
       rPlanet_I, MassPlanet_I, rOrbitMoon_I, OrbitalPeriodMoon_I

  ! Magnetic field of planet
  character (len=lTypeBField):: TypeBFieldPlanet_I(0:MaxPlanet+1)
  real, dimension(0:MaxPlanet+1):: &
       DipoleStrengthPlanet_I, bAxisThetaPlanet_I, bAxisPhiPlanet_I

  real, dimension(0:MaxPlanet+1):: IonoHeightPlanet_I

  character(len=lNamePlanet):: NamePlanet_I(0:MaxPlanet+1)

  ! Index of the selected planet
  integer:: iPlanet = -1
  integer:: Planet_ = -1 ! For compatibility with some other code

  type(OrbitType), dimension(0:MaxPlanet+1):: &
       OrbitJ2k_I, dOrbitJ2k_I
  type(RotationType), dimension(0:MaxPlanet+1):: &
       RotationIcrf_I, dRotationIcrf_I

  ! Below are defining constants for all astronomical bodies.  They are
  ! grouped using a system similar to JPL's naif/spice toolkit although
  ! the definitions are not quite the same.

  ! First define the storage location for all bodies.  This is so that you
  ! can easily find the index and can also see the naming system

  ! No Planet (in other words, no body)
  integer, parameter:: NoPlanet_  =  0

  ! New Planet (a body that is not in the database below)
  integer, parameter:: NewPlanet_  =  MaxPlanet+1

  ! Sun, planets + Pluto
  integer, parameter:: Sun_       =  1
  integer, parameter:: Mercury_   = 10
  integer, parameter:: Venus_     = 20
  integer, parameter:: Earth_     = 30
  integer, parameter:: Mars_      = 40
  integer, parameter:: Jupiter_   = 50
  integer, parameter:: Saturn_    = 60
  integer, parameter:: Uranus_    = 70
  integer, parameter:: Neptune_   = 80
  integer, parameter:: Pluto_     = 90

  ! Moons of planets (the order of the moons is not in radial distance)
  integer, parameter:: Moon_      = 31
  integer, parameter:: Io_        = 51
  integer, parameter:: Europa_    = 52
  integer, parameter:: Titan_     = 61
  integer, parameter:: Enceladus_ = 62

  ! For other solar system bodies (comets, asteroids, extra solar planets)
  ! the index is 100 or above
  integer, parameter:: Halley_               = 100
  integer, parameter:: Comet1P_              = 100
  integer, parameter:: Borrelly_             = 101
  integer, parameter:: Comet19P_             = 101
  integer, parameter:: CometCG_              = 102
  integer, parameter:: Comet67P_             = 102
  integer, parameter:: HaleBopp_             = 103

contains
  !============================================================================
  subroutine init_planet_const

    use ModUtilities, ONLY: upper_case

    integer:: i
    !--------------------------------------------------------------------------
    ! Initialize all values - below set only the non-default values
    NamePlanet_I           = ''

    rPlanet_I              = 0.0                        ! [m]
    MassPlanet_I           = 0.0                        ! [kg]
    rOrbitMoon_I           = 0.0                        ! [m]
    OrbitalPeriodMoon_I    = 0.0                        ! [s]

    TypeBFieldPlanet_I     = 'NONE'
    DipoleStrengthPlanet_I = 0.0                        ! [T]
    bAxisThetaPlanet_I     = 0.0                        ! [rad]
    bAxisPhiPlanet_I       = 0.0                        ! [rad]

    OrbitJ2k_I             = OrbitType(1.0,0.0,0.0,0.0,0.0,0.0)
    dOrbitJ2k_I            = OrbitType(0.0,0.0,0.0,360000.0,0.0,0.0)
    RotationIcrf_I         = RotationType(90.0,90.0,0.0)
    dRotationIcrf_I        = RotationType(0.0,0.0,0.0)

    IonoHeightPlanet_I     = 0.0                        ! [m]

    ! Mercury (10)
    NamePlanet_I(Mercury_)              = 'MERCURY'

    rPlanet_I(Mercury_)                 = 2439.0e+3               ! [m]
    MassPlanet_I(Mercury_)              = 3.3022e+23              ! [kg]
    TypeBFieldPlanet_I(Mercury_)        = 'DIPOLE'
    DipoleStrengthPlanet_I(Mercury_)    = -200.0e-9               ! [T]
    ! Dipole offset_Z = 484 km

    ! Venus (20)
    NamePlanet_I(Venus_)                = 'VENUS'

    rPlanet_I(Venus_)                   = 6052.0e+3               ! [m]
    MassPlanet_I(Venus_)                = 4.865e+24               ! [kg]

    ! Earth (30)
    NamePlanet_I(Earth_)                = 'EARTH'

    rPlanet_I(Earth_)                   = 6378.0e+3               ! [m]
    MassPlanet_I(Earth_)                = 5.976e+24               ! [kg]
    TypeBFieldPlanet_I(Earth_)          = 'DIPOLE'
    DipoleStrengthPlanet_I(Earth_)      = -31100.0e-9             ! [T]
    bAxisThetaPlanet_I(Earth_)          =  11.0 * cDegToRad       ! [rad]
    bAxisPhiPlanet_I(Earth_)            = 289.1 * cDegToRad       ! [rad]

    IonoHeightPlanet_I(Earth_)          = 110000.0                ! [m]

    ! Moon (31)
    NamePlanet_I(Moon_)                 = 'MOON'

    rPlanet_I(Moon_)                    = 1737.0e+3               ! [m]
    MassPlanet_I(Moon_)                 = 7.3477e+22              ! [kg]

    ! Mars (40)
    NamePlanet_I(Mars_)                 = 'MARS'

    rPlanet_I(Mars_)                    = 3396.0e+3              ! [m]
    MassPlanet_I(Mars_)                 = 0.6436e+24             ! [kg]

    ! Jupiter (50)
    NamePlanet_I(Jupiter_)              = 'JUPITER'

    rPlanet_I(Jupiter_)                 = 71492.0e+3             ! [m]
    MassPlanet_I(Jupiter_)              = 1.8980e+27             ! [kg]
    TypeBFieldPlanet_I(Jupiter_)        = 'DIPOLE'
    DipoleStrengthPlanet_I(Jupiter_)    = 428000.0e-9            ! [T]
    bAxisThetaPlanet_I(Jupiter_)        = 10.31 * cDegToRad      ! [rad]
    bAxisPhiPlanet_I(Jupiter_)          = 159.0 * cDegToRad      ! [rad]
    ! Dipole offset_Z = 7,800 km
    IonoHeightPlanet_I(Jupiter_)        = 1000.0e+3              ! [m]

    ! Saturn (60)
    NamePlanet_I(Saturn_)               = 'SATURN'

    rPlanet_I(Saturn_)                  = 60268.0e+3             ! [m]
    MassPlanet_I(Saturn_)               = 0.5685e+27             ! [kg]
    TypeBFieldPlanet_I(Saturn_)         = 'DIPOLE'
    DipoleStrengthPlanet_I(Saturn_)     = 20800.0e-9             ! [T]
    bAxisThetaPlanet_I(Saturn_)         = 0.0                    ! [rad]
    bAxisPhiPlanet_I(Saturn_)           = 0.0                    ! [rad]
    ! Dipole offset_Z = 2400 km

    IonoHeightPlanet_I(Saturn_)         = 1000.0e+3              ! [m]

    ! Uranus (70)
    NamePlanet_I(Uranus_)               = 'URANUS'
    rPlanet_I(Uranus_)                  = 25559.0e+3             ! [m]
    MassPlanet_I(Uranus_)               = 8.681e+25              ! [kg]
    TypeBFieldPlanet_I(Uranus_)         = 'DIPOLE'
    DipoleStrengthPlanet_I(Uranus_)     = 22800.0e-9             ! [T]
    bAxisThetaPlanet_I(Uranus_)         = 58.6 * cDegToRad       ! [rad]
    bAxisPhiPlanet_I(Uranus_)           = 49.9 * cDegToRad       ! [rad]
    ! Dipole offset = (0.03, -0.19, 0.24)rPlanet
    IonoHeightPlanet_I(Uranus_)         = 110000.0               ! [m]

    ! Neptune (80)
    NamePlanet_I(Neptune_)              = 'NEPTUNE'
    rPlanet_I(Neptune_)                 = 24764.0e+3             ! [m]
    MassPlanet_I(Neptune_)              = 1.02413e+26            ! [kg]
    TypeBFieldPlanet_I(Neptune_)        = 'DIPOLE'
    DipoleStrengthPlanet_I(Neptune_)    = 14200.0e-9             ! [T]
    bAxisThetaPlanet_I(Neptune_)        = 46.8 * cDegToRad       ! [rad]
    bAxisPhiPlanet_I(Neptune_)          = 289.1 * cDegToRad      ! [rad]
    IonoHeightPlanet_I(Neptune_)        = 110000.0               ! [m]
    ! Dipole offset = (-0.43, -0.31, -0.02)rPlanet

    ! Pluto (90)
    NamePlanet_I(Pluto_)                = 'PLUTO'
    rPlanet_I(Pluto_)                   = 1188.3e+3              ! [m]
    MassPlanet_I(Pluto_)                = 1.303e22               ! [kg]

    ! Io (51)
    NamePlanet_I(Io_)                   = 'IO'
    rPlanet_I(Io_)                      = 1821.0e+3              ! [m]
    MassPlanet_I(Io_)                   = 8.93e22                ! [kg]

    ! Europa (52)
    NamePlanet_I(Europa_)               = 'EUROPA'

    rPlanet_I(Europa_)                  = 1569.0e+3              ! [m]
    MassPlanet_I(Europa_)               = 4.80e22                ! [kg]
    OrbitalPeriodMoon_I(Europa_)        = 3.551 * cDay           ! [s]
    TypeBFieldPlanet_I(Europa_)         = 'DIPOLE'
    DipoleStrengthPlanet_I(Europa_)     =    100.0e-9            ! [T]
    bAxisThetaPlanet_I(Europa_)         =  90.0 * cDegToRad      ! [rad]
    bAxisPhiPlanet_I(Europa_)           =   0.0 * cDegToRad      ! [rad]

    ! Titan (61)
    NamePlanet_I(Titan_)                = 'TITAN'

    rPlanet_I(Titan_)                   = 2575.0e+3              ! [m]
    MassPlanet_I(Titan_)                = 0.1346e+24             ! [kg]
    rOrbitMoon_I(Titan_)                = 1.222e+9               ! [m]
    OrbitalPeriodMoon_I(Titan_)         = 15.945*cDay            ! [s]

    ! Enceladus (62)
    NamePlanet_I(Enceladus_)            = 'ENCELADUS'

    rPlanet_I(Enceladus_)               = 252.0e+3               ! [m]
    MassPlanet_I(Enceladus_)            = 8.4e+19                ! [kg]
    rOrbitMoon_I(Enceladus_)            = 2.3802e+8              ! [m]
    OrbitalPeriodMoon_I(Enceladus_)     = 1.370218 * cDay        ! [s]

    ! This is the field of Saturn but using a different distance unit !!!
    TypeBFieldPlanet_I(Enceladus_)         = 'DIPOLE'
    DipoleStrengthPlanet_I(Enceladus_) = &
         DipoleStrengthPlanet_I(Saturn_)* &
         (rPlanet_I(Saturn_)/rPlanet_I(Enceladus_))**3

    ! Comets
    NamePlanet_I(Halley_) = 'HALLEY'
    rPlanet_I(Halley_)                  = 1.0E9                 ! [m]

    NamePlanet_I(CometCG_)='CometCG'
    rPlanet_I(CometCG_)                 = 1.0E3                 ! [m]

    ! Get the base time for the J2000 system
    call time_int_to_real(TimeJ2k)

    ! Table-driven orbital elements and rates (JPL Table 1, 1800-2050)
    ! a [au], e [-], I/L/long.peri/long.node [deg], rates per century.

    OrbitJ2k_I(Mercury_) = OrbitType(&
         0.38709927,0.20563593,7.00497902,252.25032350,77.45779628, &
         48.33076593)
    dOrbitJ2k_I(Mercury_) = OrbitType(&
         0.00000037,0.00001906,-0.00594749,149472.67411175,0.16047689,&
         -0.12534081)

    OrbitJ2k_I(Venus_) = OrbitType(&
         0.72333566,0.00677672,3.39467605,181.97909950,131.60246718, &
         76.67984255)
    dOrbitJ2k_I(Venus_) = OrbitType(&
         0.00000390,-0.00004107,-0.00078890,58517.81538729,0.00268329, &
         -0.27769418)

    ! Use EMB elements for Earth as commonly done in JPL approximation.
    OrbitJ2k_I(Earth_) = OrbitType(&
         1.00000261,0.01671123,-0.00001531,100.46457166,102.93768193,0.0)
    dOrbitJ2k_I(Earth_) = OrbitType(&
         0.00000562,-0.00004392,-0.01294668,35999.37244981,0.32327364,0.0)

    OrbitJ2k_I(Mars_) = OrbitType(&
         1.52371034,0.09339410,1.84969142,-4.55343205,-23.94362959, &
         49.55953891)
    dOrbitJ2k_I(Mars_) = OrbitType(&
         0.00001847,0.00007882,-0.00813131,19140.30268499,0.44441088, &
         -0.29257343)

    OrbitJ2k_I(Jupiter_) = OrbitType(&
         5.20288700,0.04838624,1.30439695,34.39644051,14.72847983, &
         100.47390909)
    dOrbitJ2k_I(Jupiter_) = OrbitType(&
         -0.00011607,-0.00013253,-0.00183714,3034.74612775,0.21252668, &
         0.20469106)

    OrbitJ2k_I(Saturn_) = OrbitType(&
         9.53667594,0.05386179,2.48599187,49.95424423,92.59887831, &
         113.66242448)
    dOrbitJ2k_I(Saturn_) = OrbitType(&
         -0.00125060,-0.00050991,0.00193609,1222.49362201,-0.41897216, &
         -0.28867794)

    OrbitJ2k_I(Uranus_) = OrbitType(&
         19.18916464,0.04725744,0.77263783,313.23810451,170.95427630, &
         74.01692503)
    dOrbitJ2k_I(Uranus_) = OrbitType(&
         -0.00196176,-0.00004397,-0.00242939,428.48202785,0.40805281, &
         0.04240589)

    OrbitJ2k_I(Neptune_) = OrbitType(&
         30.06992276,0.00859048,1.77004347,-55.12002969,44.96476227, &
         131.78422574)
    dOrbitJ2k_I(Neptune_) = OrbitType(&
         0.00026291,0.00005105,0.00035372,218.45945325,-0.32241464, &
         -0.00508664)

    ! Pluto values in J2000 ecliptic coordinates (osculating-like set).
    ! Rates other than mean longitude are set to zero in this approximation.
    OrbitJ2k_I(Pluto_) = OrbitType(&
         39.48211675,0.24882730,17.14001206,238.92903833,224.06891629, &
         110.30393684)
    dOrbitJ2k_I(Pluto_) = OrbitType(&
         0.0,0.0,0.0,145.1964,0.0,0.0)

    ! Requested simplification: moons use parent-planet heliocentric orbit.
    OrbitJ2k_I(Moon_)       = OrbitJ2k_I(Earth_)
    dOrbitJ2k_I(Moon_)      = dOrbitJ2k_I(Earth_)
    OrbitJ2k_I(Io_)         = OrbitJ2k_I(Jupiter_)
    dOrbitJ2k_I(Io_)        = dOrbitJ2k_I(Jupiter_)
    OrbitJ2k_I(Europa_)     = OrbitJ2k_I(Jupiter_)
    dOrbitJ2k_I(Europa_)    = dOrbitJ2k_I(Jupiter_)
    OrbitJ2k_I(Titan_)      = OrbitJ2k_I(Saturn_)
    dOrbitJ2k_I(Titan_)     = dOrbitJ2k_I(Saturn_)
    OrbitJ2k_I(Enceladus_)  = OrbitJ2k_I(Saturn_)
    dOrbitJ2k_I(Enceladus_) = dOrbitJ2k_I(Saturn_)

    ! Table-driven rotation parameters (IAU WGCCRE 2009, Table 1)
    ! alpha, delta, W [deg] rates are per century for alpha/delta
    ! and per day for W.

    RotationIcrf_I(Sun_)  = RotationType(286.13,63.87,84.176)
    dRotationIcrf_I(Sun_) = RotationType(0.0,0.0,14.1844000)

    RotationIcrf_I(Mercury_)  = RotationType(281.0097,61.4143,329.5469)
    dRotationIcrf_I(Mercury_) = RotationType(-0.0328,-0.0049,6.1385025)

    RotationIcrf_I(Venus_)  = RotationType(272.76,67.16,160.20)
    dRotationIcrf_I(Venus_) = RotationType(0.0,0.0,-1.4813688)

    RotationIcrf_I(Earth_)  = RotationType(0.00,90.00,190.147)
    dRotationIcrf_I(Earth_) = RotationType(-0.641,-0.557,360.9856235)

    RotationIcrf_I(Mars_)  = RotationType(317.68143,52.88650,176.630)
    dRotationIcrf_I(Mars_) = RotationType(-0.1061,-0.0609,350.89198226)

    RotationIcrf_I(Jupiter_)  = RotationType(268.056595,64.495303,284.95)
    dRotationIcrf_I(Jupiter_) = RotationType(-0.006499,0.002413,870.5360000)

    RotationIcrf_I(Saturn_)  = RotationType(40.589,83.537,38.90)
    dRotationIcrf_I(Saturn_) = RotationType(-0.036,-0.004,810.7939024)

    RotationIcrf_I(Uranus_)  = RotationType(257.311,-15.175,203.81)
    dRotationIcrf_I(Uranus_) = RotationType(0.0,0.0,-501.1600928)

    RotationIcrf_I(Neptune_)  = RotationType(299.36,43.46,253.18)
    dRotationIcrf_I(Neptune_) = RotationType(0.0,0.0,536.3128492)

    ! Moon and selected moons from NAIF PCK pck00011.tpc.
    RotationIcrf_I(Moon_)  = RotationType(269.9949,66.5392,38.3213)
    dRotationIcrf_I(Moon_) = RotationType(0.0031,0.0130,13.17635815)

    RotationIcrf_I(Io_)  = RotationType(268.05,64.50,200.39)
    dRotationIcrf_I(Io_) = RotationType(-0.009,0.003,203.4889538)

    RotationIcrf_I(Europa_)  = RotationType(268.08,64.51,36.022)
    dRotationIcrf_I(Europa_) = RotationType(-0.009,0.003,101.3747235)

    RotationIcrf_I(Titan_)  = RotationType(39.4827,83.4279,186.5855)
    dRotationIcrf_I(Titan_) = RotationType(0.0,0.0,22.5769768)

    RotationIcrf_I(Enceladus_)  = RotationType(40.66,83.52,6.32)
    dRotationIcrf_I(Enceladus_) = RotationType(-0.036,-0.004,262.7318996)

    RotationIcrf_I(Pluto_)  = RotationType(132.993,-6.163,302.695)
    dRotationIcrf_I(Pluto_) = RotationType(0.0,0.0,56.3625225)

    ! Calculate the rotation matrix for J2k to Icrf:
    J2kIcrf_DD = rot_matrix_x(-InclJ2k)

    ! Calculate the rotation matrix from ecliptic J2000 to HGI system
    HgiJ2k_DD = matmul(rot_matrix_x(-7.25*cDegToRad), & ! inclination
         rot_matrix_z(-75.77*cDegToRad)) ! ascending node

    ! Equatorial ICRF/J2000: rotation by obliquity (Earth tilt at J2000 epoch)
    HgiIcrf_DD = matmul(HgiJ2k_DD, J2kIcrf_DD)

    ! No Planet (0)
    !     - No Planet and no body - defaults for everything, just set name.
    NamePlanet_I(NoPlanet_) = 'NONE'

    ! New Planet (MaxPlanet+1)
    !     - A planet whose parameters are not defined in the database above.
    !       A few values are set to clearly meaningless values to prevent
    !       a users from using the values incorrectly.
    NamePlanet_I(NewPlanet_) = 'NEW/UNKNOWN'
    rPlanet_I(NewPlanet_)    = -1.0
    MassPlanet_I(NewPlanet_) = -1.0

    ! make all the planet names upper case
    do i = 0, MaxPlanet+1
       call upper_case(NamePlanet_I(i))  ! make all the names upper case
    end do

  end subroutine init_planet_const
  !============================================================================
  subroutine get_planet_orbit(Time, Orbit)

    ! Set orbit parameters for time Time

    real(Real8_),    intent(in) :: Time
    type(OrbitType), intent(out):: Orbit

    real :: T
    !--------------------------------------------------------------------------
    T = (Time - TimeJ2k % Time)/cCentury

    Orbit % aAu = OrbitJ2k_I(iPlanet)%aAu + &
         T*dOrbitJ2k_I(iPlanet)%aAu
    Orbit % Eccentricity = OrbitJ2k_I(iPlanet)%Eccentricity + &
         T*dOrbitJ2k_I(iPlanet)%Eccentricity
    Orbit % InclinationDeg = OrbitJ2k_I(iPlanet)%InclinationDeg + &
         T*dOrbitJ2k_I(iPlanet)%InclinationDeg
    Orbit % MeanLonDeg = OrbitJ2k_I(iPlanet)%MeanLonDeg &
         + T*dOrbitJ2k_I(iPlanet)%MeanLonDeg
    Orbit % LonPeriDeg = OrbitJ2k_I(iPlanet)%LonPeriDeg + &
         T*dOrbitJ2k_I(iPlanet)%LonPeriDeg
    Orbit % LonNodeDeg = OrbitJ2k_I(iPlanet)%LonNodeDeg + &
         T*dOrbitJ2k_I(iPlanet)%LonNodeDeg

    ! write(*,*)'!!! MeanLon0, Rate, Period [yr]=', &
    !     OrbitJ2k_I(iPlanet)%MeanLonDeg, &
    !     dOrbitJ2k_I(iPlanet)%MeanLonDeg, &
    !     36000/dOrbitJ2k_I(iPlanet)%MeanLonDeg

  end subroutine get_planet_orbit
  !============================================================================
  subroutine transform_orbit_j2k_hgi(OrbitJ2k, OrbitHgi)

    use ModCoordTransform, ONLY: cross_product

    ! Convert J2000 orbit elements into HGI orbit elements

    type(OrbitType), intent(in)::  OrbitJ2k
    type(OrbitType), intent(out):: OrbitHgi

    real :: pJ2k_D(3), hJ2k_D(3), pHgi_D(3), hHgi_D(3)
    real :: Incl, Node, Peri
    real :: Node_D(3), ArgPeriX, ArgPeriY
    !--------------------------------------------------------------------------
    ! Copy major axis and Eccentricity
    OrbitHgi % aAU          = OrbitJ2k % aAU
    OrbitHgi % Eccentricity = OrbitJ2k % Eccentricity

    ! Get angles from J2000 orbit
    Incl = OrbitJ2k % InclinationDeg * cDegToRad
    Node = OrbitJ2k % LonNodeDeg * cDegToRad
    Peri = (OrbitJ2k % LonPeriDeg - OrbitJ2k % LonNodeDeg) * cDegToRad

    ! pJ2k is a unit vector pointing directly toward periapsis
    pJ2k_D(1) = cos(Node)*cos(Peri) - sin(Node)*sin(Peri)*cos(Incl)
    pJ2k_D(2) = sin(Node)*cos(Peri) + cos(Node)*sin(Peri)*cos(Incl)
    pJ2k_D(3) = sin(Peri)*sin(Incl)

    ! hJ2k is the normal vector to the orbital plane in J2000
    hJ2k_D(1) = sin(Node)*sin(Incl)
    hJ2k_D(2) = -cos(Node)*sin(Incl)
    hJ2k_D(3) = cos(Incl)

    ! Transform the vectors to the HGI frame
    pHgi_D = matmul(HgiJ2k_DD, pJ2k_D)
    hHgi_D = matmul(HgiJ2k_DD, hJ2k_D)

    ! Extract HGI Inclination safely capped to avoid acos numerical overflow
    Incl = acos(max(-1.0, min(1.0, hHgi_D(3))))

    ! Extract Periapsis safely using vector projections
    if (hHgi_D(3) < 0.999999) then
       Node = atan2(hHgi_D(1), -hHgi_D(2))
       if (Node < 0) Node = Node + cTwoPi

       ! Node_D points to the ascending node in the HGI XY plane
       Node_D(1) = cos(Node)
       Node_D(2) = sin(Node)
       Node_D(3) = 0.0

       ! PeriX and PeriY are the components of arg periapsis to node line
       ArgPeriX = dot_product(Node_D, pHgi_D)
       ArgPeriY = dot_product(cross_product(hHgi_D, Node_D), pHgi_D)
       Peri = modulo(Node + atan2(ArgPeriY, ArgPeriX), cTwoPi)
    else
       ! Node is conventionally set to 0
       Node = 0.0
       ! For flat equatorial orbit, calculate angle directly
       Peri = modulo(atan2(pHgi_D(2), pHgi_D(1)), cTwoPi)
    end if

    ! Put angles into the output
    OrbitHgi % InclinationDeg = Incl * cRadToDeg
    OrbitHgi % LonPeriDeg     = Peri * cRadToDeg
    OrbitHgi % LonNodeDeg     = Node * cRadToDeg

  end subroutine transform_orbit_j2k_hgi
  !============================================================================
  subroutine get_planet_rotation_elements(Time, Rot)

    real(Real8_),       intent(in) :: Time
    type(RotationType), intent(out):: Rot

    real :: T, d, Angle
    !--------------------------------------------------------------------------
    T = (Time - TimeJ2k % Time)/cCentury
    d = (Time - TimeJ2k % Time)/cDay

    Rot%AlphaDeg = RotationIcrf_I(iPlanet)%AlphaDeg &
         + T*dRotationIcrf_I(iPlanet)%AlphaDeg
    Rot%DeltaDeg = RotationIcrf_I(iPlanet)%DeltaDeg + &
         T*dRotationIcrf_I(iPlanet)%DeltaDeg
    Rot%WDeg     = RotationIcrf_I(iPlanet)%WDeg &
         + d*dRotationIcrf_I(iPlanet)%WDeg

    select case(iPlanet)
    case(Mercury_)
       call add_mercury_rotation_terms(d, Rot%WDeg)
    case(Jupiter_)
       call add_jupiter_rotation_terms(T, Rot%AlphaDeg, Rot%DeltaDeg)
    case(Neptune_)
       Angle = (357.85 + 52.316*T)*cDegToRad
       Rot%AlphaDeg = RotationIcrf_I(Neptune_)%AlphaDeg + 0.70*sin(Angle)
       Rot%DeltaDeg = RotationIcrf_I(Neptune_)%DeltaDeg - 0.51*cos(Angle)
       Rot%WDeg      = RotationIcrf_I(Neptune_)%WDeg &
            + d*dRotationIcrf_I(Neptune_)%WDeg - 0.48*sin(Angle)
    end select

  end subroutine get_planet_rotation_elements
  !============================================================================
  subroutine add_mercury_rotation_terms(d, WDeg)

    real, intent(in)    :: d
    real, intent(inout) :: WDeg

    real :: Angle1, Angle2, Angle3, Angle4, Angle5
    !--------------------------------------------------------------------------
    Angle1 = (174.791086 +  4.092335*d)*cDegToRad
    Angle2 = (349.582171 +  8.184670*d)*cDegToRad
    Angle3 = (164.373257 + 12.277005*d)*cDegToRad
    Angle4 = (339.164343 + 16.369340*d)*cDegToRad
    Angle5 = (153.955429 + 20.461675*d)*cDegToRad

    WDeg = WDeg &
         + 0.00993822*sin(Angle1) &
         - 0.00104581*sin(Angle2) &
         - 0.00010280*sin(Angle3) &
         - 0.00002364*sin(Angle4) &
         - 0.00000532*sin(Angle5)

  end subroutine add_mercury_rotation_terms
  !============================================================================
  subroutine add_jupiter_rotation_terms(T, AlphaDeg, DeltaDeg)

    real, intent(in)    :: T
    real, intent(inout) :: AlphaDeg, DeltaDeg

    real :: Ja, Jb, Jc, Jd, Je
    !--------------------------------------------------------------------------
    Ja = ( 99.360714 + 4850.4046*T)*cDegToRad
    Jb = (175.895369 + 1191.9605*T)*cDegToRad
    Jc = (300.323162 +  262.5475*T)*cDegToRad
    Jd = (114.012305 + 6070.2476*T)*cDegToRad
    Je = ( 49.511251 +   64.3000*T)*cDegToRad

    AlphaDeg = AlphaDeg &
         + 0.000117*sin(Ja) &
         + 0.000938*sin(Jb) &
         + 0.001432*sin(Jc) &
         + 0.000030*sin(Jd) &
         + 0.002150*sin(Je)
    DeltaDeg = DeltaDeg &
         + 0.000050*cos(Ja) &
         + 0.000404*cos(Jb) &
         + 0.000617*cos(Jc) &
         - 0.000013*cos(Jd) &
         + 0.000926*cos(Je)

  end subroutine add_jupiter_rotation_terms
  !============================================================================
  subroutine igrf_mag_axis(Time, Dipole_D)

    ! This was part of the RECALC subroutine from geopack.f by Tsyganenko
    !
    ! 1/26/2010: Darren De Zeeuw extend to 2015 with updated
    !            IGRF-11 coefficients.
    !
    ! 01/22/2016: G.Toth updated to IGRF-12 coefficients to 2020.
    !             Fixed JDAY/365 to (JDAY-1)/365.25 (as in GEOPACK-2008).
    !             Rewrote the whole thing.
    ! 02/04/2020: G.Toth updated to IGRF-13 coefficients to 2025 from
    !             https://www.ngdc.noaa.gov/IAGA/vmod/coeffs/igrf13coeffs.txt
    ! 02/01/2026  I. Sokolov updated IGRF-14 coefficients beyond year 2025
    !             https://www.ngdc.noaa.gov/IAGA/vmod/coeffs/IGRF14coeffs.xlsx
    ! Moved into ModPlanetConst to eliminate Geopack dependence.

    use ModTimeConvert, ONLY: time_real_to_int, n_day_of_year

    real(Real8_), intent(in):: Time        ! Time
    real,        intent(out):: Dipole_D(3) ! Dipole vector

    integer:: iYearLast=0, iDayLast=0  ! Store year and day of last call
    logical:: DoWarnSmallYear = .true. ! Only warn once iYearIn < MinYear
    logical:: DoWarnLargeYear = .true. ! Only warn once iYearIn > MaxYear

    integer:: iTime_I(7), iYear, iDay

    ! Year range
    integer, parameter:: MinYear = 1965
    integer, parameter:: MaxYear = 2025

    ! IGRF coefficients are given every 5 year
    integer, parameter:: DnYear = 5
    integer, parameter:: nEpoch  = (MaxYear - MinYear)/DnYear + 1

    ! Array of IGRF coefficients from MinYear to MaxYear
    ! The 1st column is -1* 3rd element of the IGRF G coefficients (G11)
    ! The 2nd column is -1* 2nd element of the IGRF H coefficients (H11)
    ! The 3rd column is +1* 2nd element of the IGRF G coefficients (G10)
    ! These provide the X, Y and Z components of the magnetic dipole in nT.

    real, parameter:: Dipole_DI(3,nEpoch) = reshape( [ &
         -2119.,   +5776.,   -30334.,   & ! 1965
         -2068.,   +5737.,   -30220.,   & ! 1970
         -2013.,   +5675.,   -30100.,   & ! 1975
         -1956.,   +5604.,   -29992.,   & ! 1980
         -1905.,   +5500.,   -29873.,   & ! 1985
         -1848.,   +5406.,   -29775.,   & ! 1990
         -1784.,   +5306.,   -29692.,   & ! 1995
         -1728.2,  +5186.1,  -29619.4,  & ! 2000
         -1669.05, +5077.99, -29554.63, & ! 2005
         -1586.42, +4944.26, -29496.57, & ! 2010
         -1501.77, +4795.99, -29441.46, & ! 2015
         -1450.9,  +4652.5,  -29404.8,  & ! 2020
         -1410.3,  +4545.5,  -29350.8   & ! 2025
         ], [3, nEpoch] )

    integer:: iEpoch        ! Index of the 5 year "epoch"
    real:: Weight1, Weight2 ! interpolation weights

    character(len=*), parameter:: NameSub = 'igrf_mag_axis'
    !--------------------------------------------------------------------------
    call time_real_to_int(Time, iTime_I)
    iYear = iTime_I(1)
    iDay  = n_day_of_year(iYear, iTime_I(2), iTime_I(3)) ! day of year

    if(iYear < MinYear) then
       if(DoWarnSmallYear)then
          write(*,*) NameSub, ' WARNING: no IGRF coefficients before year ', &
               MinYear
          write(*,*)NameSub,': setting iYear=', MinYear,' iDay=1'
          DoWarnSmallYear = .false.
       end if
       iYear = MinYear
       iDay  = 1
    endif
    if(iYear > MaxYear + DnYear) then
       if(DoWarnLargeYear)then
          write(*,*) 'WARNING!!! Update IGRF coefficients in ',NameSub, &
               ' in share/Library/src/ModPlanetConst.f90 !!!'
          write(*,*) NameSub, ': no IGRF coefficients beyond year ', MaxYear
          write(*,*)NameSub,': setting iYear=', MaxYear+DnYear,' iDay=365'
          DoWarnLargeYear = .false.
       end if
       iYear = MaxYear + DnYear
       iDay  = 365
    endif

    ! No need to recalculate if the call uses the same values as last time
    if (iYear == iYearLast .and. iDay == iDayLast) RETURN
    iYearLast = iYear
    iDayLast  = iDay

    ! Find the epoch index for iYear
    iEpoch  = min(nEpoch-1, (iYear - MinYear)/DnYear + 1)

    ! Calculate time relative to epoch start normalized to epoch length
    Weight2 = (iYear + (iDay-1)/365.25 - MinYear - (iEpoch-1)*DnYear)/DnYear
    Weight1 = 1.0 - Weight2

    Dipole_D = Weight1*Dipole_DI(:,iEpoch) + Weight2*Dipole_DI(:,iEpoch+1)

    ! Convert from nT to Tesla (SI units)
    Dipole_D = 1e-9*Dipole_D

  end subroutine igrf_mag_axis
  !============================================================================
end module ModPlanetConst
!==============================================================================
