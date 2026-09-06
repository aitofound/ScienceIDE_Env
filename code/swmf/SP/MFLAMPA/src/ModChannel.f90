!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModChannel

  ! Setting energy channels and get the integral/differential intensity

  use ModUtilities, ONLY: CON_stop
  use SP_ModGrid, ONLY: nLine, nVertex_B, Used_B, nP, nMu
  use SP_ModUnit, ONLY: Io2Si_V, Si2Io_V, UnitEnergy_, UnitFlux_, &
       NameFluxUnit, NameDiffFluxUnit, NameEnergyFluxUnit

  implicit none

  save

  private ! Except

  public:: init               ! initialize the flux channels
  public:: read_param         ! read satellite-related parameters
  public:: distr_to_flux      ! convert distribution function to fluxes
  public:: get_integral_flux  ! calculate Flux_VIB

  ! ------------ For saving the intensity in energy channels ------------
  integer, public :: nFluxChannelSat = 1  ! GOES satellite as default
  integer, public :: nFluxChannel    = 6  ! GOES as default, 6 channels
  integer, parameter :: LenNameSat   = 12 ! satellite name length
  type FluxChannelSat
     ! Full set of information, for the energy channels of each satellite
     !
     ! ------ General information ------
     ! Satellite name
     character(len=LenNameSat):: NameSat
     ! Flux channel counts
     integer:: nFluxChannel
     ! Satellite index
     integer:: iKindSat
     !
     ! ------ Saved energy channels and flux type ------
     ! Satellite flux type
     integer:: iKindFlux
     ! Energy channels: Lo & Hi bounds, first in the unit of MeV
     real, allocatable, dimension(:):: &
          EChannelLoIo_I, &   ! Low bound
          EChannelHiIo_I, &   ! High bound
          EChannelMidIo_I     ! Middle value
  end type FluxChannelSat

  ! All plot files
  type(FluxChannelSat), public, allocatable :: FluxChannelSat_I(:)
  integer, parameter :: &
       FluxChannelSELF_      =  0, &  ! Self-defined
       FluxChannelGOESInt_   =  1, &  ! GOES integral flux
       FluxChannelGOES08_    =  2, &  ! GOES-08 differential flux
       FluxChannelGOES08S14_ =  3, &  ! GOES-08 differential flux, calibrated
       FluxChannelGOES10_    =  4, &  ! GOES-10 differential flux
       FluxChannelGOES11_    =  5, &  ! GOES-11 differential flux
       FluxChannelGOES11S14_ =  6, &  ! GOES-11 differential flux, calibrated
       FluxChannelGOES12_    =  7, &  ! GOES-12 differential flux
       FluxChannelGOES13_    =  8, &  ! GOES-13 differential flux
       FluxChannelGOES13S14_ =  9, &  ! GOES-13 differential flux, calibrated
       FluxChannelGOES13B17_ = 10, &  ! GOES-13 differential flux, calibrated
       FluxChannelGOES14_    = 11, &  ! GOES-14 differential flux
       FluxChannelGOES14S14_ = 12, &  ! GOES-14 differential flux, calibrated
       FluxChannelGOES15_    = 13, &  ! GOES-15 differential flux
       FluxChannelGOES15S14_ = 14, &  ! GOES-15 differential flux, calibrated
       FluxChannelGOES15B17_ = 15, &  ! GOES-15 differential flux, calibrated
       FluxChannelGOES16_    = 16, &  ! GOES-16 differential flux
       FluxChannelGOES17_    = 17, &  ! GOES-17 differential flux
       FluxChannelGOES18_    = 18, &  ! GOES-18 differential flux
       FluxChannelERNE_      = 19, &  ! SOHO/ERNE
       FluxChannelEPHIN_     = 20, &  ! SOHO/EPHIN
       FluxChannelEPAM_      = 21, &  ! ACE/EPAM
       FluxChannelGME_       = 22, &  ! IMP-8/GME
       FluxChannelLET_       = 23, &  ! STEREO-A/B LET
       FluxChannelHET_       = 24, &  ! STEREO-A/B HET
       FluxChannelSEPT_      = 25     ! STEREO-A/B SEPT
  integer, parameter :: &
       FluxInt_  = 1, &    ! Integral intensity
       FluxDiff_ = 2       ! Differential intensity

  ! Total integral (simulated) particle flux
  integer, parameter, public :: Flux0_     = 0 ! Total particle flux
  integer, parameter, public :: FluxFirst_ = 1 ! The first channel
  integer,            public :: FluxLast_  = 6 ! The last channel
  integer,            public :: EFlux_     = 7 ! Total integral energy flux
  integer,            public :: FluxMax_   = 7 ! Maximum flux channel index

  ! Flux saved for this satellite
  ! 1st index: Energy channel
  ! 2nd index: iVertex along the field line
  ! 3rd index: Field line number
  real, public, allocatable :: Flux_VIB(:,:,:)
  ! Initial values of fluxes in energy channels
  real, public, allocatable :: FluxChannelInit_V(:)

  ! Energy channels of the instrument, or defined by users
  integer, public, allocatable, dimension(:) :: EChannelType_I
  real,    public, allocatable, dimension(:) :: &
       EChannelLoIo_I, EChannelHiIo_I, EChannelMidIo_I
  ! Energy channel bin, for get_integral_flux
  real,            allocatable, dimension(:) :: InvEChannelBinIo_I
  ! Energy channel names and units
  character(len=:),  public, allocatable, dimension(:) :: NameFluxChannel_I
  character(len=12), public, allocatable, dimension(:) :: NameChannelSource_I
  character(len=20), public, allocatable, dimension(:) :: NameFluxUnit_I

  ! Logical variable: whether this is the initial call
  logical :: DoInit = .true.
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    use ModUtilities, ONLY: split_string, lower_case, upper_case
    character(len=*), intent(in) :: NameCommand
    ! general loop variables
    integer           :: iFlux
    ! input characters from EChannelIo_I in #ECHANNEL
    integer, parameter:: nStringEChannel = 3
    character(len=30) :: StringEChannel
    character(len=10) :: StringEChannel_I(nStringEChannel)
    ! input characters from EChannelIo_I in #ECHANNELSAT
    ! loop variable for satellites
    integer           :: iSat
    ! local variable of satellite names
    character(len=LenNameSat) :: NameSat
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------

    select case(NameCommand)
    case('#ECHANNEL')
       call read_var('nFluxChannel', nFluxChannel)
       if(nFluxChannel < 0) then
          call CON_stop(NameSub // ': nFluxChannel must be non-negative')
       else if(nFluxChannel == 0) then
          RETURN
       else
          ! Do nothing
       end if
       FluxLast_ = nFluxChannel

       ! Allocate arrays for flux type, lower and upper bounds
       if(allocated(EChannelType_I)) deallocate(EChannelType_I)
       if(allocated(EChannelLoIo_I)) deallocate(EChannelLoIo_I)
       if(allocated(EChannelHiIo_I)) deallocate(EChannelHiIo_I)
       if(allocated(EChannelMidIo_I)) deallocate(EChannelMidIo_I)
       if(allocated(NameChannelSource_I)) deallocate(NameChannelSource_I)
       allocate(EChannelType_I(FluxFirst_:FluxLast_), &
            EChannelLoIo_I(FluxFirst_:FluxLast_), &
            EChannelHiIo_I(FluxFirst_:FluxLast_), &
            EChannelMidIo_I(FluxFirst_:FluxLast_), &
            NameChannelSource_I(FluxFirst_:FluxLast_))
       ! Get the input EChannelIo_I in the unit of MeV
       do iFlux = FluxFirst_, FluxLast_
          call read_var('EChannelIo_I', StringEChannel)
          ! in the order of {int/diff} {low bound} {high bound}, in MeV!!
          call split_string(StringEChannel, StringEChannel_I)

          ! 1) Energy channel type: integral or differential intensity
          call lower_case(StringEChannel_I(1))
          select case(StringEChannel_I(1))
          case('int', 'inte', 'integral', 'integrate', 'integrated')
             EChannelType_I(iFlux) = FluxInt_
          case('diff', 'differential', 'spec', 'spectrum', 'spectra')
             EChannelType_I(iFlux) = FluxDiff_
          case default
             call CON_stop(NameSub // ": Unknown EChannelType, " // &
                  StringEChannel_I(1) // " at EChannel ", iFlux)
          end select

          ! 2) Low energy bound: do not know KinEnergyIo_G(1) (min value) here
          read(StringEChannel_I(2), *) EChannelLoIo_I(iFlux)
          if(EChannelLoIo_I(iFlux) < 0.0) call CON_stop(NameSub // &
               ": Energy channel lower bound should not be negative.")

          ! 3) High energy bound: do not know EnergyMaxIo (max value) here
          read(StringEChannel_I(3), *) EChannelHiIo_I(iFlux)
          if(EChannelHiIo_I(iFlux) < EChannelLoIo_I(iFlux)) &
               call CON_stop(NameSub // ": Energy channel upper bound" // &
               " should not be less than lower bound.")

          ! 4) Get the temporary middle energy
          EChannelMidIo_I(iFlux) = sqrt( &
               EChannelLoIo_I(iFlux)*EChannelHiIo_I(iFlux))

          ! last but not least, name the flux channel source
          NameChannelSource_I(iFlux) = "Self-Defined"
       end do
    case('#ECHANNELSAT')
       ! Read the number of satellites, specified with the energy channels
       call read_var('nFluxChannelSat', nFluxChannelSat)
       ! This means we do not use any energy channel from any satellite
       if(nFluxChannelSat < 0) then
          call CON_stop(NameSub // ': nFluxChannelSat must be non-negative')
       else if(nFluxChannelSat == 0) then
          RETURN
       else
          ! Do nothing
       end if

       if(allocated(FluxChannelSat_I)) deallocate(FluxChannelSat_I)
       allocate(FluxChannelSat_I(1:nFluxChannelSat))

       ! Set KindSat and nFluxChannel for each specified satellite
       do iSat = 1, nFluxChannelSat
          call read_var('NameSatellite', NameSat)
          call upper_case(NameSat)
          FluxChannelSat_I(iSat) % NameSat = trim(NameSat)

          select case(trim(FluxChannelSat_I(iSat) % NameSat))
          case('GOES', 'GOES_Int', 'GOES_Integral')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOESInt_
             FluxChannelSat_I(iSat) % nFluxChannel = 6
          case('GOES-08', 'GOES-08_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES08_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-08S14', 'GOES-08S14_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES08S14_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-10', 'GOES-10_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES10_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-11', 'GOES-11_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES11_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-11S14', 'GOES-11S14_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES11S14_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-12', 'GOES-12_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES12_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-13', 'GOES-13_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES13_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-13S14', 'GOES-13S14_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES13S14_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-13B17', 'GOES-13B17_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES13B17_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-14', 'GOES-14_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES14_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-14S14', 'GOES-14_S14Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES14S14_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-15', 'GOES-15_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES15_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-15S14', 'GOES-15S14_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES15S14_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-15B17', 'GOES-15B17_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES15B17_
             FluxChannelSat_I(iSat) % nFluxChannel = 10
          case('GOES-16', 'GOES-16_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES16_
             FluxChannelSat_I(iSat) % nFluxChannel = 14
          case('GOES-17', 'GOES-17_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES17_
             FluxChannelSat_I(iSat) % nFluxChannel = 14
          case('GOES-18', 'GOES-18_Differential')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGOES18_
             FluxChannelSat_I(iSat) % nFluxChannel = 14
          case('ERNE', 'SOHO/ERNE')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelERNE_
             FluxChannelSat_I(iSat) % nFluxChannel = 20
          case('EPHIN', 'SOHO/EPHIN')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelEPHIN_
             FluxChannelSat_I(iSat) % nFluxChannel = 4
          case('EPAM', 'ACE/EPAM')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelEPAM_
             FluxChannelSat_I(iSat) % nFluxChannel = 8
          case('GME', 'IMP8/GME', 'IMP-8/GME')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelGME_
             FluxChannelSat_I(iSat) % nFluxChannel = 30
          case('LET', 'STAB/LET')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelLET_
             FluxChannelSat_I(iSat) % nFluxChannel = 12
          case('HET', 'STAB/HET')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelHET_
             FluxChannelSat_I(iSat) % nFluxChannel = 11
          case('SEPT', 'STAB/SEPT')
             FluxChannelSat_I(iSat) % iKindSat = FluxChannelSEPT_
             FluxChannelSat_I(iSat) % nFluxChannel = 32
          case default
             FluxChannelSat_I(iSat) % iKindSat = -1
             FluxChannelSat_I(iSat) % nFluxChannel = 0
             call CON_stop(NameSub // ": satellite name (for " // &
                  "energy channels) isn't properly set in PARAM.in")
          end select
       end do
    end select

  end subroutine read_param
  !============================================================================
  subroutine init
    ! To initialize the energy channels of the satellite(s)

    use ModConst, ONLY: cMeV
    use ModUtilities, ONLY: check_allocate
    use SP_ModDistribution, ONLY: KinEnergyIo_G, &
         EnergyInjIo, EnergyMaxIo, FluxInitIo
    use SP_ModProc, ONLY: iError
    use SP_ModSize, ONLY: nVertexMax
    ! loop variables
    integer :: iSat, iFlux
    ! index VAR for convenience
    integer :: FluxLastSat_
    ! Tmp VARs
    integer :: nFluxChannelTmp = 0
    integer, allocatable, dimension(:) :: EChannelTypeTmp_I
    real,    allocatable, dimension(:) :: &
         EChannelLoIoTmp_I, EChannelHiIoTmp_I, EChannelMidIoTmp_I
    character(len=20), allocatable, dimension(:) :: NameChannelSourceTmp_I
    ! local NameFluxChannel, saved the channel index
    character(len=2) :: NameFluxChannel

    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    if(.not.DoInit) RETURN
    DoInit = .false.

    ! Not specify satellite energy channels: GOES by default
    ! Not specify self-defined energy channel: 0
    ! 1) To ONLY use FluxChannelSat_I: not set extra nFluxChannel
    ! 2) To include self-defined channels: set nFluxChannel
    ! 3) To ONLY use self-defined channels: set nFluxChannel <= 0
    ! 4) To include both FluxChannelSat_I and self-defined channels: set both
    ! Note that here we allow for nFluxChannel==0 and nFluxChannelSat==0, and
    ! in this case, we only save Flux0_ and EFlux_ in the MH_data files.
    if(allocated(EChannelType_I)) then
       ! Self-defined channels: will drop satellite channels if not specified
       if(.not.allocated(FluxChannelSat_I)) nFluxChannelSat = 0
       ! else: we specify nFluxChannelSat > 0, summed later

       ! Convert units for self-defined energy channels, and the lower
       ! and upper bounds should fall between KinEnergyIo_G(1) and EnergyMaxIo
       EChannelLoIo_I = MIN(EnergyMaxIo, MAX(KinEnergyIo_G(1), &
            EChannelLoIo_I*cMeV*Si2Io_V(UnitEnergy_)))
       EChannelHiIo_I = MIN(EnergyMaxIo, MAX(KinEnergyIo_G(1), &
            EChannelHiIo_I*cMeV*Si2Io_V(UnitEnergy_)))
       EChannelMidIo_I = sqrt(EChannelLoIo_I*EChannelHiIo_I)
    else
       ! No self-defined channels
       nFluxChannel = 0
       ! In this case, nFluxChannelSat should be >= 0, either specified by
       ! users, or set to be default. For the former, FluxChannelSat_I is
       ! allocated as long as nFluxChannelSat>0, but when nFluxChannelSat==0,
       ! FluxChannelSat_I is not allocated, same as the latter situation.
       ! However, nFluxChannelSat is > 0, set as default for the latter case,
       ! so we can differentiate them by nFluxChannelSat.

       ! if no specify satellite energy channels: GOES by default
       ! To turn this off: set nFluxChannelSat = 0
       if(.not.allocated(FluxChannelSat_I) .and. nFluxChannelSat>0) then
          allocate(FluxChannelSat_I(1:nFluxChannelSat))
          FluxChannelSat_I(1) % NameSat = 'GOES'
          FluxChannelSat_I(1) % iKindSat = FluxChannelGOESInt_
          FluxChannelSat_I(1) % nFluxChannel = 6
       end if
    end if

    ! Save self-defined channels if there are plus nFluxChannelSat>0
    if(allocated(EChannelType_I) .and. nFluxChannelSat>0) then
       ! Save the self-defined flux channels
       nFluxChannelTmp = nFluxChannel
       allocate(NameChannelSourceTmp_I(FluxFirst_:nFluxChannelTmp), &
            EChannelTypeTmp_I(FluxFirst_:nFluxChannelTmp), &
            EChannelLoIoTmp_I(FluxFirst_:nFluxChannelTmp), &
            EChannelHiIoTmp_I(FluxFirst_:nFluxChannelTmp), &
            EChannelMidIoTmp_I(FluxFirst_:nFluxChannelTmp))
       NameChannelSourceTmp_I = NameChannelSource_I
       EChannelTypeTmp_I = EChannelType_I
       EChannelLoIoTmp_I = EChannelLoIo_I
       EChannelHiIoTmp_I = EChannelHiIo_I
       EChannelMidIoTmp_I = EChannelMidIo_I
       deallocate(NameChannelSource_I, EChannelType_I, &
            EChannelLoIo_I, EChannelHiIo_I, EChannelMidIo_I)
    end if

    ! Next we initialize the details for nFluxChannelSat
    do iSat = 1, nFluxChannelSat
       ! Exclude unreasonable satellites
       if(FluxChannelSat_I(iSat)%iKindSat <= 0) call CON_stop(NameSub // &
            ": Incorrect satellite, " // FluxChannelSat_I(iSat)%NameSat // &
            ", set for energy channels.")

       ! Set the last channel index of satellite, according to its nFluxChannel
       FluxLastSat_ = FluxChannelSat_I(iSat)%nFluxChannel

       ! Allocate the energy channels, names, and units
       ! Lower bound: from 0 => physical KinEnergyIo_G(1) in code
       ! Upper bound: to +\infty => physical EnergyMaxIo in code
       ! Middle: geometric mean of lower and upper bound
       if(allocated(FluxChannelSat_I(iSat)%EChannelLoIo_I)) &
            deallocate(FluxChannelSat_I(iSat)%EChannelLoIo_I)
       allocate(FluxChannelSat_I(iSat)%EChannelLoIo_I(FluxFirst_:FluxLastSat_))
       if(allocated(FluxChannelSat_I(iSat)%EChannelHiIo_I)) &
            deallocate(FluxChannelSat_I(iSat)%EChannelHiIo_I)
       allocate(FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxFirst_:FluxLastSat_))
       if(allocated(FluxChannelSat_I(iSat)%EChannelMidIo_I)) &
            deallocate(FluxChannelSat_I(iSat)%EChannelMidIo_I)
       allocate(FluxChannelSat_I(iSat)%EChannelMidIo_I(FluxFirst_:FluxLastSat_))

       ! Get the energy channels, names, and units (still in MeV now)
       select case(FluxChannelSat_I(iSat) % iKindSat)
       case(FluxChannelGOESInt_)
          ! GOES nominal integral flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxInt_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [5, 10, 30, 50, 60, 100]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = &
               EnergyMaxIo*Io2Si_V(UnitEnergy_)/cMeV
       case(FluxChannelGOES08_, FluxChannelGOES10_, &
            FluxChannelGOES11_, FluxChannelGOES12_)
          ! GOES-08, 10, 11, and 12 differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [4.0, 9.0, 15.0, 40.0, &
               80.0, 165.0, 350.0, 420.0, 510.0, 700.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [9.0, 15.0, 44.0, 80.0, &
               165.0, 500.0, 420.0, 510.0, 700.0, &
               EnergyMaxIo*Io2Si_V(UnitEnergy_)/cMeV]
       case(FluxChannelGOES08S14_)
          ! GOES-08 calibrated differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [4.0, 7.4, 13.3, 37.0, &
               91.5, 119.0, 350.0, 420.0, 510.0, 700.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [7.9, 15.0, 21.3, 53.6, &
               113.0, 179.0, 420.0, 510.0, 700.0, &
               EnergyMaxIo*Io2Si_V(UnitEnergy_)/cMeV]
       case(FluxChannelGOES11S14_)
          ! GOES-11 calibrated differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [5.0, 9.4, 16.7, 32.5, &
               89.8, 120.0, 350.0, 420.0, 510.0, 700.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [7.9, 15.9, 23.2, 56.4, &
               114.0, 186.0, 420.0, 510.0, 700.0, &
               EnergyMaxIo*Io2Si_V(UnitEnergy_)/cMeV]
       case(FluxChannelGOES13_, FluxChannelGOES14_, FluxChannelGOES15_)
          ! GOES-13, 14, and 15 differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [4.2, 8.7, 15.0, 38.0, &
               84.0, 110.0, 330.0, 420.0, 510.0, 700.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [8.7, 14.5, 40.0, 82.0, &
               200.0, 900.0, 420.0, 510.0, 700.0, &
               EnergyMaxIo*Io2Si_V(UnitEnergy_)/cMeV]
       case(FluxChannelGOES13S14_, FluxChannelGOES14S14_, &
            FluxChannelGOES15S14_)
          ! GOES-13, 14, and 15 calibrated differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [5.0, 9.4, 16.7, 32.5, &
               89.8, 120.0, 330.0, 420.0, 510.0, 700.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [7.9, 15.9, 23.2, 56.4, &
               114.0, 186.0, 420.0, 510.0, 700.0, &
               EnergyMaxIo*Io2Si_V(UnitEnergy_)/cMeV]
       case(FluxChannelGOES13B17_)
          ! GOES-13 calibrated (Bruno 17) differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [5.0, 9.4, 16.7, 32.5, &
               93.3, 146.7, 273.9, 330.0, 418.7, 852.6]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [7.9, 15.9, 23.2, 56.4, &
               129.1, 205.6, 387.5, 458.0, 566.0, 1081.2]
       case(FluxChannelGOES15B17_)
          ! GOES-15 calibrated (Bruno 17) differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [5.0, 9.4, 16.7, 32.5, &
               97.9, 145.0, 240.4, 325.3, 420.4, 878.6]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [7.9, 15.9, 23.2, 56.4, &
               135.3, 202.3, 335.6, 464.6, 573.1, 1230.0]
       case(FluxChannelGOES16_, FluxChannelGOES17_, FluxChannelGOES18_)
          ! GOES-16, 17, and 18 differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [1.02, 1.9, 2.31, 3.4, &
               5.84, 11.64, 24.9, 40.3, 83.7, 99.9, 115.0, 160.0, 276.0, 500.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [1.86, 2.3, 3.34, 6.48, &
               11.0, 23.27, 38.1, 73.4, 98.5, 118.0, 143.0, 242.0, 404.0, &
               EnergyMaxIo*Io2Si_V(UnitEnergy_)/cMeV]
       case(FluxChannelERNE_)
          ! SOHO/ERNE (LED + HED) differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [1.3, 1.6, 2.0, 2.5, &
               3.2, 4.0, 5.0, 6.4, 8.0, 10.0, 13.0, 16.0, 20.0, 25.0, &
               32.0, 40.0, 50.0, 64.0, 80.0, 100.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxFirst_:FluxLastSat_-1) = &
               FluxChannelSat_I(iSat)%EChannelLoIo_I(FluxFirst_+1:FluxLastSat_)
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxLastSat_) = 130.0
       case(FluxChannelEPHIN_)
          ! SOHO/EPHIN differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [4.3, 7.8, 25.0, 40.9]
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxFirst_:FluxLastSat_-1) = &
               FluxChannelSat_I(iSat)%EChannelLoIo_I(FluxFirst_+1:FluxLastSat_)
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxLastSat_) = 53.0
       case(FluxChannelEPAM_)
          ! ACE/EPRM differential flux for protons
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [0.046, 0.067, 0.115, &
               0.193, 0.315, 0.580, 1.06, 1.88]
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxFirst_:FluxLastSat_-1) = &
               FluxChannelSat_I(iSat)%EChannelLoIo_I(FluxFirst_+1:FluxLastSat_)
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxLastSat_) = 4.70
       case(FluxChannelGME_)
          ! IMP-8/GME (LED + MED) differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [0.88, 1.15, 1.43, 1.79, &
               2.27, 3.03, 4.20, 4.94, 5.96, 7.25, 8.65, 11.1, 13.6, 16.1, &
               18.7, 19.8, 24.2, 28.7, 35.2, 42.9, 51.0, 63.2, 87.0, 92.5, &
               107.0, 121.0, 154.0, 178.0, 230.0, 327.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [1.15, 1.43, 1.79, 2.27, &
               3.03, 4.20, 4.94, 5.96, 7.25, 8.64, 11.1, 13.6, 16.1, 18.7, &
               22.5, 24.2, 28.7, 35.2, 42.9, 51.0, 63.2, 81.0, 92.5, 107.0, &
               121.0, 154.0, 178.0, 230.0, 327.0, 485.0]
       case(FluxChannelLET_)
          ! STEREO-AB/LET differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [1.8, 2.2, 2.7, 3.2, &
               3.6, 4.0, 4.5, 5.0, 6.0, 8.0, 10.0, 12.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxFirst_:FluxLastSat_-1) = &
               FluxChannelSat_I(iSat)%EChannelLoIo_I(FluxFirst_+1:FluxLastSat_)
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxLastSat_) = 15.0
       case(FluxChannelHET_)
          ! STEREO-AB/HET differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [13.6, 14.9, 17.0, 20.8, &
               23.8, 26.3, 29.5, 33.4, 35.5, 40.0, 60.0]
          FluxChannelSat_I(iSat)%EChannelHiIo_I = [15.1, 17.1, 19.3, 23.8, &
               26.4, 29.7, 33.4, 35.8, 40.5, 60.0, 100.0]
       case(FluxChannelSEPT_)
          ! STEREO-AB/SEPT differential flux
          FluxChannelSat_I(iSat)%iKindFlux = FluxDiff_
          FluxChannelSat_I(iSat)%EChannelLoIo_I = [0.0258, 0.0546, 0.0838, &
               0.0926, 0.101, 0.109, 0.121, 0.137, 0.155, 0.174, 0.195, &
               0.219, 0.246, 0.275, 0.311, 0.350, 0.391, 0.439, 0.495, &
               0.556, 0.623, 0.701, 0.786, 0.853, 0.902, 1.046, 1.248, &
               1.402, 1.577, 1.767, 1.982, 2.510]
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxFirst_:FluxLastSat_-1) = &
               FluxChannelSat_I(iSat)%EChannelLoIo_I(FluxFirst_+1:FluxLastSat_)
          FluxChannelSat_I(iSat)%EChannelHiIo_I(FluxLastSat_) = 3.586
       end select

       ! Convert energy channel units: MeV to SI to Io, and the lower
       ! and upper bounds should fall between KinEnergyIo_G(1) and EnergyMaxIo
       FluxChannelSat_I(iSat)%EChannelLoIo_I = &
            MIN(EnergyMaxIo, MAX(KinEnergyIo_G(1), &
            FluxChannelSat_I(iSat)%EChannelLoIo_I*cMeV*Si2Io_V(UnitEnergy_)))
       FluxChannelSat_I(iSat)%EChannelHiIo_I = &
            MIN(EnergyMaxIo, MAX(KinEnergyIo_G(1), &
            FluxChannelSat_I(iSat)%EChannelHiIo_I*cMeV*Si2Io_V(UnitEnergy_)))

       ! Get the middle energy
       FluxChannelSat_I(iSat)%EChannelMidIo_I = sqrt( &
            FluxChannelSat_I(iSat)%EChannelLoIo_I * &
            FluxChannelSat_I(iSat)%EChannelHiIo_I)

       ! Finally sum for nFluxChannel
       nFluxChannel = nFluxChannel + FluxChannelSat_I(iSat)%nFluxChannel
    end do

    ! With summed nFluxChannel, we get indices for flux channels
    FluxLast_ = nFluxChannel
    EFlux_    = FluxLast_ + 1
    FluxMax_  = EFlux_

    ! Re-assign energy channel info to the whole array when nFluxChannelSat>0
    if(nFluxChannelSat > 0) then
       allocate(NameChannelSource_I(FluxFirst_:FluxLast_), &
            EChannelType_I(FluxFirst_:FluxLast_), &
            EChannelLoIo_I(FluxFirst_:FluxLast_), &
            EChannelHiIo_I(FluxFirst_:FluxLast_), &
            EChannelMidIo_I(FluxFirst_:FluxLast_))

       ! For the first a few channels: self-defined energy channels
       if(nFluxChannelTmp > 0) then
          iFlux = nFluxChannelTmp
          NameChannelSource_I(FluxFirst_:iFlux) = NameChannelSourceTmp_I
          EChannelType_I(FluxFirst_:iFlux) = EChannelTypeTmp_I
          EChannelLoIo_I(FluxFirst_:iFlux) = EChannelLoIoTmp_I
          EChannelHiIo_I(FluxFirst_:iFlux) = EChannelHiIoTmp_I
          EChannelMidIo_I(FluxFirst_:iFlux) = EChannelMidIoTmp_I
          ! Ready for the satellite energy channel assignments
          iFlux = iFlux + 1

          deallocate(NameChannelSourceTmp_I, EChannelTypeTmp_I, &
               EChannelLoIoTmp_I, EChannelHiIoTmp_I, EChannelMidIoTmp_I)
       else
          iFlux = FluxFirst_
       end if

       ! For the rest channels: satellite energy channels
       do iSat = 1, nFluxChannelSat
          NameChannelSource_I(iFlux:iFlux-1+ &
               FluxChannelSat_I(iSat)%nFluxChannel) = &
               trim(FluxChannelSat_I(iSat)%NameSat)
          EChannelType_I(iFlux:iFlux-1+ &
               FluxChannelSat_I(iSat)%nFluxChannel) = &
               FluxChannelSat_I(iSat)%iKindFlux
          EChannelLoIo_I(iFlux:iFlux-1+ &
               FluxChannelSat_I(iSat)%nFluxChannel) = &
               FluxChannelSat_I(iSat)%EChannelLoIo_I
          EChannelHiIo_I(iFlux:iFlux-1+ &
               FluxChannelSat_I(iSat)%nFluxChannel) = &
               FluxChannelSat_I(iSat)%EChannelHiIo_I
          EChannelMidIo_I(iFlux:iFlux-1+ &
               FluxChannelSat_I(iSat)%nFluxChannel) = &
               FluxChannelSat_I(iSat)%EChannelMidIo_I
          iFlux = iFlux + FluxChannelSat_I(iSat)%nFluxChannel
       end do
    end if

    ! Calculate the energy channel bin width (inverse). Note that we allow
    ! nFluxChannel == 0 to only save Flux0_ and EFlux_, so we should be careful
    if(nFluxChannel > 0) then
       allocate(InvEChannelBinIo_I(FluxFirst_:FluxLast_))
       InvEChannelBinIo_I = 1.0/(EChannelHiIo_I - EChannelLoIo_I)
    end if

    ! Write the header of the energy channels for particle flux
    allocate(character(len=14) :: NameFluxChannel_I(Flux0_:FluxMax_))
    do iFlux = FluxFirst_, FluxLast_
       write(NameFluxChannel,'(I2.2)') iFlux
       NameFluxChannel_I(iFlux) = 'flux_Channel' // NameFluxChannel
    end do
    ! Write the header of total particle and energy fluxes
    NameFluxChannel_I(Flux0_) = 'flux_total    '
    NameFluxChannel_I(EFlux_) = 'eflux         '

    ! Initialize NameFluxUnit_I for different energy channels
    if(allocated(NameFluxUnit_I)) deallocate(NameFluxUnit_I)
    allocate(NameFluxUnit_I(Flux0_:FluxMax_))
    NameFluxUnit_I(Flux0_:FluxLast_) = NameFluxUnit
    ! In case that nFluxChannel == 0
    if(allocated(EChannelType_I)) then
       where(EChannelType_I(FluxFirst_:FluxLast_) == FluxDiff_)
          NameFluxUnit_I(FluxFirst_:FluxLast_) = NameDiffFluxUnit
       end where
    end if
    NameFluxUnit_I(EFlux_) = NameEnergyFluxUnit

    ! Finally allocate Flux_VIB, saved for outputs
    if(.not.allocated(Flux_VIB)) then
       allocate(Flux_VIB(Flux0_:FluxMax_, 1:nVertexMax, nLine), stat=iError)
       call check_allocate(iError, 'Flux_VIB')
       Flux_VIB = -1.0
    else
       call CON_stop(NameSub // ' Flux_VIB already allocated')
    end if

    ! Fill initial values of flux in energy channels
    allocate(FluxChannelInit_V(Flux0_:FluxMax_))
    ! FluxChannelInit_V, for the assumed initial distribution (~1/p^2)
    FluxChannelInit_V(Flux0_) = FluxInitIo
    FluxChannelInit_V(FluxFirst_:FluxLast_) = FluxInitIo * &
         (EChannelHiIo_I - EChannelLoIo_I) / (EnergyMaxIo - EnergyInjIo)
    FluxChannelInit_V(EFlux_) = FluxInitIo*0.5*(EnergyMaxIo + EnergyInjIo)

  end subroutine init
  !============================================================================
  subroutine get_integral_flux

    use SP_ModDistribution, ONLY: Distribution_CB
    ! loop variables
    integer:: iLine, iVertex
    !--------------------------------------------------------------------------
    do iLine = 1, nLine
       if(.not.Used_B(iLine)) CYCLE
       do iVertex = 1, nVertex_B(iLine)
          call distr_to_flux(Distribution_CB(1:nP,1:nMu,iVertex,iLine), &
               Flux_VIB(:,iVertex,iLine))
       end do
    end do
  end subroutine get_integral_flux
  !============================================================================
  subroutine distr_to_flux(Distribution_II, Flux_V)
    ! compute the total (simulated) integral flux of particles and particle
    ! fluxes in specified energy channels; also compute total energy flux

    use ModNumConst, ONLY: cTiny
    use SP_ModDistribution, ONLY: DeltaMu, Momentum_G, &
         KinEnergyIo_G, search_kinetic_energy

    real, intent(in) :: Distribution_II(1:nP, 1:nMu)
    real, intent(out):: Flux_V(Flux0_:FluxMax_)
    ! loop variables
    integer:: iP, iFlux, iPLo, iPHi
    ! VDF*Momentum_G**2, VDF*Momentum_G**2*KinEnergy
    real   :: DistTimesP2_I(1:nP), DistTimesP2E_I(1:nP)
    ! increments of each bin for particle and energy fluxes
    real   :: dFlux_I(1:nP-1), dEFlux_I(1:nP-1)
    ! increments of the bin where the channel falls in
    real   :: dFluxChannelLo, dFluxChannelHi
    ! logical variables, whether the iKinEnergyOut index is found
    logical:: IsFoundChannelIo, IsFoundChannelHi
    ! particle flux of energy channels
    real   :: FluxLo_I(FluxFirst_:FluxLast_), FluxHi_I(FluxFirst_:FluxLast_)

    !--------------------------------------------------------------------------
    ! Calculate intermediate variables
    DistTimesP2_I = sum(Distribution_II(1:nP, 1:nMu), &
         DIM=2)*DeltaMu*0.5 * Momentum_G(1:nP)**2
    DistTimesP2E_I = DistTimesP2_I*KinEnergyIo_G(1:nP)
    ! Increment of particle and energy fluxes
    ! Integration loop with midpoint rule
    dFlux_I  = 0.5*(KinEnergyIo_G(2:nP) - KinEnergyIo_G(1:nP-1))* &
         (DistTimesP2_I(1:nP-1) + DistTimesP2_I(2:nP))
    dEFlux_I = 0.5*(KinEnergyIo_G(2:nP) - KinEnergyIo_G(1:nP-1))* &
         (DistTimesP2E_I(1:nP-1) + DistTimesP2E_I(2:nP))

    ! Calculate and store the total particle and energy fluxes
    Flux_V(Flux0_) = sum(dFlux_I )*Si2Io_V(UnitFlux_)
    Flux_V(EFlux_) = sum(dEFlux_I)*Si2Io_V(UnitFlux_)

    ! Reset values
    FluxLo_I = 0.0; FluxHi_I = 0.0
    ! Calculate the particle fluxes for all energy channels
    do iFlux = FluxFirst_, FluxLast_
       ! 1) For the flux from EnergyLo to EnergyMax
       ! Get the partial flux increments
       call search_kinetic_energy(EChannelLoIo_I(iFlux), iPLo, &
            IsFoundChannelIo, DistTimesP2_I, dFluxChannelLo)
       ! iPLo: from 1 to nP-1, due to the range of EChannelLoIo_I
       if(iPLo == nP-1) then
          ! Only contributed from nP-1 to nP, when iPLo == nP-1
          FluxLo_I(iFlux) = dFluxChannelLo
       else
          ! For the rest bins: make a summation, when iPLo < nP-1
          FluxLo_I(iFlux) = dFluxChannelLo + sum(dFlux_I(iPLo+1:nP-1))
       end if

       ! 2) For the flux from EnergyHi to EnergyMax
       if(abs(EChannelHiIo_I(iFlux)-KinEnergyIo_G(nP)) < cTiny) then
          FluxHi_I(iFlux) = 0.0
       else
          ! Get the partial flux increments
          call search_kinetic_energy(EChannelHiIo_I(iFlux), iPHi, &
               IsFoundChannelHi, DistTimesP2_I, dFluxChannelHi)
          ! iPHi: from 1 to nP-1, due to the range of EChannelHiIo_I
          if(iPHi == nP-1) then
             ! Only contributed from nP-1 to nP, when iPHi == nP-1
             FluxHi_I(iFlux) = dFluxChannelHi
          else
             ! For the rest bins: make a summation, when iPHi < nP-1
             FluxHi_I(iFlux) = dFluxChannelHi + sum(dFlux_I(iPHi+1:nP-1))
          end if
       end if
    end do

    ! Store the results for specified energy channels
    Flux_V(FluxFirst_:FluxLast_) = &
         (FluxLo_I - FluxHi_I)*Si2Io_V(UnitFlux_)
    ! In case that nFluxChannel == 0
    if(allocated(EChannelType_I)) then
       where(EChannelType_I(FluxFirst_:FluxLast_) == FluxDiff_)
          Flux_V(FluxFirst_:FluxLast_) = Flux_V(FluxFirst_:FluxLast_) &
               *InvEChannelBinIo_I
       end where
    end if
  end subroutine distr_to_flux
  !============================================================================
end module SP_ModChannel
!==============================================================================
