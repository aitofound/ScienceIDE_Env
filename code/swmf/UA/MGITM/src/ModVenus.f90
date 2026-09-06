!  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module GITM_planet

  use ModConstants
  use ModSizeGITM, only: nAlts, nLons, nLats

  implicit none

  integer, parameter :: iCO2_= 1
  integer, parameter :: iCO_ = 2
  integer, parameter :: iO_  = 3
  integer, parameter :: iN2_ = 4
  integer, parameter :: iAr_ = 5
  integer, parameter :: iHe_ = 6
  integer, parameter :: nSpecies = 6

  integer, parameter :: nSpeciesTotal = 6

  integer, parameter  :: iOP_    = 1
  integer, parameter  :: iO2P_   = 2
  integer, parameter  :: ie_     = 3
  integer, parameter  :: nIons   = ie_
  integer, parameter  :: nIonsAdvect = 1
  integer, parameter  :: nSpeciesAll = nSpeciesTotal + nIons - 1
  
  character (len=20) :: cSpecies(nSpeciesTotal)
  character (len=20) :: cIons(nIons)

  real :: Mass(nSpeciesTotal), MassI(nIons)

  real :: Vibration(nSpeciesTotal)

  integer, parameter :: iE2470_ = 1
  integer, parameter :: iE7320_ = 2
  integer, parameter :: iE3726_ = 3
  integer, parameter :: iE5200_ = 4
  integer, parameter :: iE10400_ = 5
  integer, parameter :: iE6300_ = 6
  integer, parameter :: iE6364_ = 7
  
  integer, parameter :: nEmissions = 10
  
  integer, parameter :: i3371_ = 1
  integer, parameter :: i4278_ = 2
  integer, parameter :: i5200_ = 3
  integer, parameter :: i5577_ = 4
  integer, parameter :: i6300_ = 5
  integer, parameter :: i7320_ = 6
  integer, parameter :: i10400_ = 7
  integer, parameter :: i3466_ = 8
  integer, parameter :: i7774_ = 9
  integer, parameter :: i8446_ = 10
  integer, parameter :: i3726_ = 11

  real, parameter :: GC_Venus               = 8.87                  ! m/s^2
  real, parameter :: RP_Venus               = 2.0997e+07            ! seconds
  real, parameter :: R_Venus                = 6052.0*1000.0         ! meters
  real, parameter :: DP_Venus               = 0.0                   ! nT

  real, parameter :: Gravitational_Constant = GC_Venus
  real, parameter :: Rotation_Period        = RP_Venus
  real, parameter :: RBody                  = R_Venus
  real, parameter :: DipoleStrength         = DP_Venus

  real, parameter :: OMEGABody              = 2.00*pi/Rotation_Period  ! rad/s

  real, parameter :: HoursPerDay = Rotation_Period / 3600.0
  real, parameter :: EarthHoursPerDay = 24.0
  real, parameter :: EarthSecondsPerDay = 24.0*3600.0
  real, parameter :: EarthDaysPerPlanetDay = Rotation_Period/EarthSecondsPerDay
  real, parameter :: Tilt = 177.0

  real, parameter :: DaysPerYear = 0.9246
  real, parameter :: SecondsPerYear = DaysPerYear * Rotation_Period

  integer, parameter :: iVernalYear   = 1999
  integer, parameter :: iVernalMonth  =    3
  integer, parameter :: iVernalDay    =   21
  integer, parameter :: iVernalHour   =    0
  integer, parameter :: iVernalMinute =    0
  integer, parameter :: iVernalSecond =    0

  ! Old orbital parameters
 !real, parameter :: SunOrbit_A = 1.000110
 !real, parameter :: SunOrbit_B = 0.034221
 !real, parameter :: SunOrbit_C = 0.001280
 !real, parameter :: SunOrbit_D = 0.000719
 !real, parameter :: SunOrbit_E = 0.000077

  !New Orbital Parameters
  !A: semi-major axis in AU
  !B: eccentricity
  !C: Longitude of perihelion
  !D: Mean Longitude
  !E: For calulating actual Longitude
 real, parameter :: SunOrbit_A = 0.7200000000
 real, parameter :: SunOrbit_B = 0.0000
 real, parameter :: SunOrbit_C = 0.0
 real, parameter :: SunOrbit_D = 0.0
 real, parameter :: SunOrbit_E = 0.0

  !Used as a damping term in Vertical solver.
  real :: VertTau(nAlts)

  logical :: IsVenus = .true.
  logical :: IsEarth = .false.
  logical :: IsMars = .false.
  logical :: IsTitan = .false.
  logical :: NonMagnetic = .false.
  real, parameter :: PlanetNum = 0.02

  character (len=10) :: cPlanet = "Venus"
  
  integer, parameter :: nEmissionWavelengths = 20
  integer, parameter :: nPhotoBins = 190

  real, parameter, dimension(nSpecies, nSpecies) :: Diff0 = 1.0e17 * reshape( (/ &
   ! These are Aij coefficients from B&K (1973) formulation: Aij*1.0E+17
   ! Use Jared Bell's Titan GITM formulation for Venus GITM
      !------------------------------------------------+
      ! i=C02      CO      O        N2      Ar      He  
      !------------------------------------------------+
       0.0000, 0.7762, 0.2219,  0.6580,  1.1970, 2.4292,         &  ! CO2
       0.7762, 0.0000, 0.9466,  0.9280,  0.6625,1159.55,         &  ! CO
       0.2219, 0.9466, 0.0000,  0.9690,  0.5510, 3.4346,         &  ! O
       0.6580, 0.9280, 0.9690,  0.0000,  0.6640,1159.55,         &  ! N2
       1.1970, 0.6625, 0.5510,  0.6640,  0.0000,1000.00,         &  ! O2
       2.4292,1159.55, 3.4346,1159.550,1000.000, 0.000 /),       &  ! He
       (/nSpecies,nSpecies/) )
!
  ! These are s-exponents from B&K (1973) formulation: T**s

    real, parameter, dimension(nSpecies, nSpecies) :: DiffExp = reshape( (/ &
     !------------------------------------------------+
     ! i=C02      CO      O     N2     Ar     He  
     !------------------------------------------------+
       0.000,  0.750,  0.750, 0.752, 0.750, 0.720,  &           ! CO2
       0.750,  0.000,  0.750, 0.710, 0.750, 0.524,  &           ! CO
       0.750,  0.750,  0.000, 0.774, 0.841, 0.749,  &           ! O
       0.752,  0.710,  0.774, 0.000, 0.750, 0.524,  &           ! N2
       0.750,  0.750,  0.841, 0.750, 0.000, 0.524,  &           ! Ar
       0.720,  0.524,  0.749, 0.524, 0.524, 0.000 /), &         ! He
       (/nSpecies,nSpecies/) ) 

  
  ! These are for the neutral friction routine...

   real, Dimension(1:nLons,1:nLats,-1:nAlts+2,1:nSpecies) :: ChemicalSourcesTotal
   real, Dimension(1:nLons,1:nLats,-1:nAlts+2,1:nSpecies) :: TotalRhoFluxes
   real, Dimension(1:nLons,1:nLats,-1:nAlts+2,1:nSpecies) :: EddyVelocity
   real, Dimension(1:nLons,1:nLats,-1:nAlts+2) :: EddyHeatFlux
   real, Dimension(1:nLons,1:nLats,-1:nAlts+2) :: LogEddyHeatFlux
   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: GradLogEddyHeatFlux
   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: EddyHeatFluxUpdate

!!! Energy Balance Checks
   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: CO2CoolingRate
   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: OCoolingRate
   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: NOCoolingRate
   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: CO2NonLTEMask

!!! Testing NO Chemical Rates
   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: NOTotalProduction
   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: NOTotalLoss
   real, Dimension(1:nLons,1:nLats, 1:nAlts,1:5  ) :: NOProdRxns
   real, Dimension(1:nLons,1:nLats, 1:nAlts,1:7  ) :: NOLossRxns

   real, Dimension(1:nLons,1:nLats, 1:nAlts  ) :: SZA3D
!----------------------
  integer, parameter :: nVTGCMLons = 72
  integer, parameter :: nVTGCMSlts = 72
  integer, parameter :: nVTGCMLats = 36
  integer, parameter :: nVTGCMAlts = 76

  real, dimension(nVTGCMSlts) :: VTGCMSlts
  real, dimension(nVTGCMLons) :: VTGCMLons
  real, dimension(nVTGCMLats) :: VTGCMLats
  real, dimension(nVTGCMAlts) :: VTGCMAlts

  real, dimension(nVTGCMLons,nVTGCMLats,nVTGCMAlts) :: &
        VTGCMCoolingRates, LogVTGCMCoolingRates

contains

  subroutine init_planet

    use ModTime

    integer :: itime(7)

    Mass(iHe_)  =  4.003 * AMU
    Mass(iO_)   = 15.999 * AMU
    Mass(iN2_)  = 28.013 * AMU
    Mass(iCO_)  = 28.11*AMU 
    Mass(iCO2_) = 44.01*AMU 
    Mass(iAr_)  = 39.948*AMU 

    cSpecies(iHe_)   = "He"
    cSpecies(iO_) = "O"
    cSpecies(iN2_)   = "N!D2!N"
    cSpecies(iCO_)   = "CO"
    cSpecies(iCO2_)   = "CO!D2!N"
    cSpecies(iAr_)   = "Ar"

    cIons(iOP_)    = "O!U+!N"
    cIons(iO2P_)   = "O!D2!U+!N"
    cIons(ie_)     = "e-"

    Vibration(iCO2_)  = 8.955 ! Corrected by Bell (for 300 K)  
    Vibration(iCO_)   = 7.01  !
    Vibration(iO_)    = 5.0
    Vibration(iN2_)   = 7.0
    Vibration(iHe_)   = 5.0

    MassI(iOP_)  = Mass(iO_)
    MassI(iO2P_) = 2*Mass(iO_)
    MassI(ie_)  = Mass_Electron

    VertTau = 1.0e9

    itime = 0
    itime(1) = iVernalYear
    itime(2) = iVernalMonth
    itime(3) = iVernalDay
    itime(4) = iVernalHour
    itime(5) = iVernalMinute
    itime(6) = iVernalSecond
    call time_int_to_real(itime, VernalTime)

  end subroutine init_planet

!! Placeholder subroutines (for Titan specific Phyisics)

  subroutine init_radcooling
  return
  end subroutine init_radcooling

  subroutine init_magheat
  return
  end subroutine init_magheat

  subroutine init_isochem
  return
  end subroutine init_isochem

  subroutine init_aerosol
  return
  end subroutine init_aerosol

  subroutine init_topography
    return
  end subroutine init_topography

end module GITM_planet
