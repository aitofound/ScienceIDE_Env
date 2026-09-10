
MODULE ModIE

  use ModCharSize
  use ModTimeAmie
  use ModW05_read_data
  use w05sc, only: setmodel, epotval
  use EIE_ModWeimer, only: get_tilt
  use ModFtaModel
  use ModIHP
  use ModNewell
  use ModAMIE_Interface, only: initialize_amie_files, update_amie_files
  use ModKind

  implicit none

  private

  integer, parameter, public :: iZero_ = 0

  ! Electric Potential Types:
  integer, parameter, public :: iWeimer05_ = 1
  integer, parameter, public :: iMillstone_ = 2
  integer, parameter, public :: iHepMay_ = 3
  integer, parameter, public :: iAmiePot_ = 4

  ! Auroral Types:
  integer, parameter, public :: iFTA_ = 1
  integer, parameter, public :: iFRE_ = 2
  integer, parameter, public :: iPEM_ = 3
  integer, parameter, public :: iOvationPrime_ = 4
  integer, parameter, public :: iOvationSme_ = 5
  integer, parameter, public :: iAmieAur_ = 6

  real, parameter, public :: rBadValue = -1.0e32

  integer, external :: efield_interpret_name
  integer, external :: aurora_interpret_name

  type, public :: ieModel

    logical :: isOk = .true.
    logical :: isFtaLimit = .false.
    real :: FtaAe = 25
    real :: FtaAU = 5
    real :: FtaAL = -20

    integer :: iDebugLevel = 0
    integer :: iEfield_ = -1
    integer :: iAurora_ = -1

    character(len=iCharLenIE_) :: modelDir = "data/ext/"
    character(len=iCharLenIE_) :: modelDirFta = "FTA/"
    character(len=iCharLenIE_) :: northFile = "none"
    character(len=iCharLenIE_) :: southFile = "none"
    character(len=iCharLenIE_), allocatable, dimension(:) :: northFiles
    character(len=iCharLenIE_), allocatable, dimension(:) :: southFiles
    integer :: nFilesNorth = 0
    integer :: nFilesSouth = 0

    ! ----------------------------------------------------------------
    ! These are the states that the models has, if we either read in
    ! a file or we are coupling to some other model.
    ! They are 3D, because there is latitude, local time, and block,
    ! where block can be north/south or whatever.
    ! ----------------------------------------------------------------
    integer :: havenLats = 0
    integer :: havenMLTs = 0
    integer :: havenBLKs = 0
    real, allocatable, dimension(:, :, :) :: haveLats
    real, allocatable, dimension(:, :, :) :: haveMLTs
    ! Field-aligned Currents:
    real, allocatable, dimension(:, :, :) :: haveFac
    ! Potentials:
    real, allocatable, dimension(:, :, :) :: havePotential
    ! Electron diffuse:
    real, allocatable, dimension(:, :) :: haveDiffuseEeFlux
    real, allocatable, dimension(:, :) :: haveDiffuseEAveE
    ! Ion diffuse:
    real, allocatable, dimension(:, :) :: haveDiffuseIeFlux
    real, allocatable, dimension(:, :) :: haveDiffuseIAveE
    ! Discrete or Monoenergetic:
    real, allocatable, dimension(:, :) :: haveMonoEeFlux
    real, allocatable, dimension(:, :) :: haveMonoEAveE
    ! Broadband or Wave-drive:
    real, allocatable, dimension(:, :) :: haveWaveEeFlux
    real, allocatable, dimension(:, :) :: haveWaveEAveE
    ! Is Polar Cap (1 if is polar cap, 0 otherwise):
    real, allocatable, dimension(:, :) :: havePolarCap

    ! ----------------------------------------------------------------
    ! These are what the code that is calling this library needs
    ! ----------------------------------------------------------------

    integer :: neednLats = 0
    integer :: neednMLTs = 0
    real, allocatable, dimension(:, :) :: needLats
    real, allocatable, dimension(:, :) :: needMLTs

    integer :: iProc = 0

    integer :: havenTimes = 0
    real(kind=Real8_) :: currentTime = rBadValue

    real :: needImfBz = rBadValue
    real :: needImfBy = rBadValue
    real :: needSwV = rBadValue
    real :: needSwN = rBadValue
    real :: needHp = rBadValue
    real :: needHpN = rBadValue
    real :: needHpS = rBadValue
    real :: needAu = rBadValue
    real :: needAl = rBadValue
    real :: needAe = rBadValue
    real :: needKp = rBadValue
    logical :: useAeForHp = .false.

    real :: weimerTilt = 0.0

    ! ----------------------------------------------------------------
    ! To make the indices reading/retrieving a bit more more modular
    ! ----------------------------------------------------------------

    logical :: doReadMHD = .false.
    logical :: doReadSME = .false.
    logical :: doReadKp = .false.
    logical :: doReadHPI = .false. ! not actually used yet. remove noaa hpi altogether??

    ! --------------------------------------------------------------------------
    ! Keep track of whether model has been run after time & indices are updated
    ! --------------------------------------------------------------------------

    logical :: isAuroraUpdated = .false.
    logical :: isPotentialUpdated = .false.

  contains

    ! Set verbose level:
    procedure :: verbose => set_verbose

    ! Set model types:
    procedure :: efield_model => set_efield_model
    procedure :: aurora_model => set_aurora_model
    procedure :: filename_north => set_filename_north
    procedure :: filename_south => set_filename_south

    procedure :: nfiles_north => set_nfiles_north
    procedure :: nfiles_south => set_nfiles_south
    procedure :: filename_list_north => set_filename_list_north
    procedure :: filename_list_south => set_filename_list_south

    ! Where to find the data files for empirical models:
    procedure :: model_dir => set_model_dir

    ! Initialize the library:
    procedure :: init => initialize

    ! set indices to run empirical models:
    procedure :: imfBz => set_bz
    procedure :: imfBy => set_by
    procedure :: swV => set_swv
    procedure :: swN => set_swn
    procedure :: hp => set_hp
    procedure :: hpN => set_hpn
    procedure :: hpS => set_hps
    procedure :: au => set_au
    procedure :: al => set_al
    procedure :: ae => set_ae
    procedure :: kp => set_kp
    procedure :: useAeHp => set_useAeForHp
    procedure :: dontAeHp => unset_useAeForHp
    procedure :: aehp => set_hp_from_ae
    procedure :: check_indices => run_check_indices

    ! Grid information for the calling code:
    procedure :: nMlts => set_nMlts
    procedure :: nLats => set_nLats
    procedure :: grid => set_grid
    procedure :: mlts => set_mlts
    procedure :: lats => set_lats

    procedure :: time_real => set_time_real
    procedure :: time_ymdhms => set_time_ymdhms
    procedure :: check_time => run_check_time

    ! Get model results:
    procedure :: get_potential => run_potential_model
    procedure :: get_aurora => run_aurora_model_electron_diffuse
    procedure :: get_electron_diffuse_aurora => run_aurora_model_electron_diffuse
    procedure :: get_electron_mono_aurora => run_aurora_model_electron_mono
    procedure :: get_electron_wave_aurora => run_aurora_model_electron_wave
    procedure :: get_ion_diffuse_aurora => run_aurora_model_ion_diffuse
    procedure :: weimer05 => run_weimer05_model
    procedure :: hepmay => run_heppner_maynard_model
    procedure :: fta => run_fta_model
    procedure :: hpi_pem => run_hpi_pem_model
    procedure :: get_polarcap => get_polarcap_results
    procedure :: ovation_e_diffuse => run_ovation_model_electron_diffuse
    procedure :: ovation_e_mono => run_ovation_model_electron_mono
    procedure :: ovation_e_wave => run_ovation_model_electron_wave
    procedure :: ovation_ion_diffuse => run_ovation_model_ion_diffuse

  end type ieModel

contains

  ! ------------------------------------------------------------
  ! Initialize all of the different models
  subroutine initialize(this)
    use ModErrors

    class(ieModel) :: this
    character(len=iCharLenIE_) :: modelDirTotal
    character(len=iCharLenIE_) :: inFileNameTotal
    character(len=iCharLenIE_) :: name
    integer :: UnitTmp_ = 76
    integer :: iError = 0

    if (this%iDebugLevel >= 0) &
      write(*, *) "> Initializing IE Library"

    if (this%iDebugLevel > 1) &
      write(*, *) "==> Model data directory : ", trim(this%modelDir)

    !\
    ! --------------------------------------------------------------------
    ! Electric Field Models
    ! --------------------------------------------------------------------
    !/

    ! --------------
    ! --- Weimer ---
    if (this%iEfield_ == iWeimer05_) then
      call read_all_files(this%modelDir)
      this%doReadMHD = .true.
      if (this%iDebugLevel >= 0) write(*, *) " => Weimer Initialized"
    endif

    ! -------------------------
    ! --- Heppner & Maynard ---
    if (this%iEfield_ == iHepMay_) then
      this%doReadMHD = .true.
      this%doReadKP = .true.

      inFileNameTotal = 'hmr89.cofcnts'
      call merge_str(this%modelDir, inFileNameTotal)
      open(UnitTmp_, file=inFileNameTotal, status='old', iostat=iError)
      if (iError /= 0) then
        call set_error('Error opening heppner maynard file :')
        call set_error(inFileNameTotal)
      else
        call gethmr(UnitTmp_)
        close(UnitTmp_)
      endif
      if (this%iDebugLevel >= 0) write(*, *) " => HepMay Initialized"
    endif

    ! ------------------
    ! --- AMIE Files ---
    if ((this%iEfield_ == iAmiePot_) .or. (this%iAurora_ == iAmieAur_)) then
      call initialize_amie_files(this%nFilesNorth, this%northFiles, &
                                 this%nFilesSouth, this%southFiles, this%iDebugLevel)
      if (this%iDebugLevel >= 0) write(*, *) " => AMIE Files Initialized"
    endif

    !\
    ! --------------------------------------------------------------------
    ! Aurora Models
    ! --------------------------------------------------------------------
    !/
    if (this%iAurora_ == iFta_) then
      this%doReadSME = .true.

      modelDirTotal = this%modelDirFta
      call merge_str(this%modelDir, modelDirTotal)
      call initialize_fta(modelDirTotal)
      if (this%iDebugLevel >= 0) write(*, *) " => FTA Initialized"
    endif

    if (this%iAurora_ == iFRE_) then
      this%doReadHPI = .true.

      name = 'ihp'
      call read_conductance_model(name, this%modelDir, this%iDebugLevel)
      if (this%iDebugLevel >= 0) write(*, *) " => IHP Initialized"
    endif

    if (this%iAurora_ == iPEM_) then
      this%doReadHPI = .true.

      name = 'pem'
      call read_conductance_model(name, this%modelDir, this%iDebugLevel)
      if (this%iDebugLevel >= 0) write(*, *) " => PEM Initialized"
    endif

    if (this%iAurora_ == iOvationPrime_) then
      call init_newell(this%modelDir, this%iDebugLevel)
      if (this%iDebugLevel >= 0) write(*, *) " => OVATION Initialized"
    endif

  end subroutine initialize

  ! ------------------------------------------------------------
  ! Set the verbose level for the library:
  subroutine set_verbose(this, level)
    class(ieModel) :: this
    integer, intent(in) :: level
    this%iDebugLevel = level
  end subroutine set_verbose

  ! ------------------------------------------------------------
  ! Check indices to see if the model has what it needs:
  subroutine run_check_time(ie)
    class(ieModel) :: ie
    if (ie%currentTime == rBadValue) then
      call set_error("current time not set!")
      call set_error("   --> need to call one of the set_time routines!")
    endif
    return
  end subroutine run_check_time

  ! ------------------------------------------------------------
  ! Set the current time to run the empirical model
  subroutine set_time_real(this, ut)
    class(ieModel) :: this
    real(kind=Real8_), intent(in) :: ut
    integer :: iYear
    integer :: iMonth
    integer :: iDay
    integer, dimension(7) :: itime
    real :: rHour

    this%currentTime = ut

    this%isAuroraUpdated = .false.
    this%isPotentialUpdated = .false.

    ! Now, do some updating of different model parameters if needed:

    if (this%iEfield_ == iWeimer05_) then
      call time_real_to_int(ut, itime)
      iYear = itime(1)
      iMonth = itime(2)
      iDay = itime(3)
      rHour = float(iTime(4)) + float(iTime(5))/60.0
      this%weimerTilt = get_tilt(iYear, iMonth, iDay, rHour)
    endif

    if (this%iEfield_ == iAmiePot_ .or. &
        this%iAurora_ == iAmieAur_) then
      call update_amie_files(ut)
    endif

  end subroutine set_time_real

  ! ------------------------------------------------------------
  ! Set the current time to run the empirical model
  subroutine set_time_ymdhms(this, iYear, iMonth, iDay, iHour, iMin, iSec)
    class(ieModel) :: this
    integer, intent(in) :: iYear
    integer, intent(in) :: iMonth
    integer, intent(in) :: iDay
    integer, intent(in) :: iHour
    integer, intent(in) :: iMin
    integer, intent(in) :: iSec
    integer :: iTime(1:7)
    real(kind=Real8_) :: ut

    iTime(1) = iYear
    iTime(2) = iMonth
    iTime(3) = iDay
    iTime(4) = iHour
    iTime(5) = iMin
    iTime(6) = iSec
    iTime(7) = 0
    call time_int_to_real(iTime, ut)
    call this%time_real(ut)

    this%isAuroraUpdated = .false.
    this%isPotentialUpdated = .false.

  end subroutine set_time_ymdhms

  ! ------------------------------------------------------------
  ! Set indices routines
  ! ------------------------------------------------------------
  INCLUDE "model_subroutines.f90"

  ! ------------------------------------------------------------
  ! Set indices routines
  ! ------------------------------------------------------------
  INCLUDE "indices_subroutines.f90"

  ! ------------------------------------------------------------
  ! Set grid routines
  ! ------------------------------------------------------------
  INCLUDE "grid_subroutines.f90"

end MODULE ModIE
