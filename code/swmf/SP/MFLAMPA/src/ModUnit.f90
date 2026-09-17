!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModUnit

  ! Unit for particle energy,  energy to momentum conversion
  ! for proton, names for all units
  ! Dec.24 2017 Sokolov & Borovikov.

  use SP_ModGrid, ONLY: nVar, LagrID_
  use ModConst, ONLY: Rsun, energy_in,                        &
       gen_kin_energy_to_momentum => kinetic_energy_to_momentum,&
       gen_momentum_to_kin_energy => momentum_to_kinetic_energy,&
       gen_momentum_to_energy     => momentum_to_energy
  use ModUtilities, ONLY: CON_stop

  implicit none

  SAVE

  private ! Except

  ! Public routines
  public :: init

  ! Convert particle momentum to energy or kinetic energy and
  ! kinetic energy to momnetum, for proton
  public :: kinetic_energy_to_momentum, momentum_to_energy, &
       momentum_to_kinetic_energy, read_param

  ! Only protons are simulated at this moment
  character(len=*), public, parameter :: NameParticle = 'proton'

  ! Unit of SEP energy, also applicable for ion kinetic temperature
  character(len=3), public            :: NameEnergyUnit = 'kev'

  ! Integral flux unit, in usual case, called Io since it is per cm**2
  character(len=6), public, parameter :: NameFluxUnit = 'pfu'

  ! Differential flux unit
  character(len=7), public            :: NameDiffFluxUnit = 'pfu/kev'

  ! Unit particle flux, 1 particle per cm2 per s per steradian,
  ! corresponds to 10,000 particles per m2 per s per steradian.
  real,                     parameter :: UnitParticleFluxSi = 10000.0
  character(len=12),public            :: NameEnergyFluxUnit = 'kev/cm2*s*sr'

  ! Unit conversions
  integer, public, parameter :: &
       UnitX_      = 1, & ! Coordinate conversion between Si (m) and Io (Rsun)
       UnitEnergy_ = 2, & ! Energy conversion between Si (J) and Io (keV, ...)
       UnitFlux_   = 3    ! Flux conversion between Si (per m**2) and Io (pfu)
  real, public, dimension(UnitX_:UnitFlux_) :: Io2Si_V, Si2Io_V

  ! Unit for all the state variables:
  ! Length is in the unit of Rs, Rho is in the unit of amu/m^3,
  ! temperature is in the unit of kinetic energy, all others are in SI units.
  character(len=6), public :: NameVarUnit_V(LagrID_:nVar) = [&
       'none  ', &
       'RSun  ', &
       'RSun  ', &
       'RSun  ', &
       'amu/m3', &
       'kev   ', &
       'm/s   ', &
       'm/s   ', &
       'm/s   ', &
       'T     ', &
       'T     ', &
       'T     ', &
       'J/m3  ', &
       'J/m3  ', &
       'RSun  ', &
       'RSun  ', &
       'RSun  ', &
       'm/s   ', &
       'T     ', &
       'amu/m3', &
       'm/s   ', &
       'T     ']

  logical :: DoInit = .true.
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    use ModUtilities, ONLY: lower_case
    use SP_ModGrid, ONLY: T_
    character(len=*), intent(in):: NameCommand ! From PARAM.in

    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#PARTICLEENERGYUNIT')
       ! Read unit to be used for particle energy: eV, keV, MeV, GeV, TeV
       call read_var('ParticleEnergyUnit', NameEnergyUnit)

       call lower_case(NameEnergyUnit)
       NameEnergyFluxUnit = trim(NameEnergyUnit)//'/cm2*s*sr'
       NameDiffFluxUnit   = trim(NameFluxUnit)//'/'//trim(NameEnergyUnit)
       NameVarUnit_V(T_)  = trim(NameEnergyUnit)//'   '
    case default
       call CON_stop(NameSub//' Unknown command '//NameCommand)
    end select

  end subroutine read_param
  !============================================================================
  subroutine init
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    if(.not.DoInit)RETURN

    ! unit conversion: IO to Si
    Io2Si_V(UnitX_)      = Rsun
    Io2Si_V(UnitEnergy_) = energy_in(trim(NameEnergyUnit))
    Io2Si_V(UnitFlux_)   = UnitParticleFluxSi

    ! unit conversion: Si to IO
    Si2Io_V = 1.0 / Io2Si_V

    DoInit = .false.
  end subroutine init
  !============================================================================
  real function kinetic_energy_to_momentum(Energy)
    real, intent(in) :: Energy
    !--------------------------------------------------------------------------
    kinetic_energy_to_momentum = &
         gen_kin_energy_to_momentum(Energy, NameParticle)
  end function kinetic_energy_to_momentum
  !============================================================================
  real function momentum_to_energy(Momentum)
    real, intent(in) :: Momentum
    !--------------------------------------------------------------------------
    momentum_to_energy = &
         gen_momentum_to_energy(Momentum, NameParticle)
  end function momentum_to_energy
  !============================================================================
  real function momentum_to_kinetic_energy(Momentum)
    real, intent(in) :: Momentum
    !--------------------------------------------------------------------------
    momentum_to_kinetic_energy = &
         gen_momentum_to_kin_energy(Momentum, NameParticle)
  end function momentum_to_kinetic_energy
  !============================================================================
end module SP_ModUnit
!==============================================================================
