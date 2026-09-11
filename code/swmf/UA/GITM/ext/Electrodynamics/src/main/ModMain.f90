
Module ModMain

  use ModKind

  character(len=100) :: outputDir = 'IE/output'
  character(len=100) :: filename = 'test.bin'
  integer :: LunMainOutput = 60
  logical :: isInitialized = .false.

  integer :: nTimes = 12, iT
  real(kind=Real8_) :: currentTime = -1.0
  real(kind=Real8_) :: startTime = -1.0
  real(kind=Real8_) :: endTime = -1.0

  real :: imfBx, imfBy, imfBz
  real :: ae, au, al, hp
  real :: swV, swN

  integer :: nLons = 25
  integer :: nLats = 30
  real :: minLat = 60.0
  logical :: isGeographic = .false.
  real, allocatable :: magLats(:, :)
  real, allocatable :: magLons(:, :)
  real, allocatable :: magMlts(:, :)

  real, allocatable :: geoLats(:, :)
  real, allocatable :: geoLons(:, :)

  real, allocatable :: gseX(:, :)
  real, allocatable :: gseY(:, :)
  real, allocatable :: gseZ(:, :)

  real, allocatable :: potential(:, :)
  real, allocatable :: eDiffuseAvee(:, :)
  real, allocatable :: eDiffuseEflux(:, :)
  real, allocatable :: iDiffuseAvee(:, :)
  real, allocatable :: iDiffuseEflux(:, :)
  real, allocatable :: eMonoAvee(:, :)
  real, allocatable :: eMonoEflux(:, :)
  real, allocatable :: eWaveAvee(:, :)
  real, allocatable :: eWaveEflux(:, :)
  real, allocatable :: polarCap(:, :)

contains

  ! ----------------------------------------------------------------
  ! Allocate all variables
  ! ----------------------------------------------------------------

  subroutine allocate_all_variables

    if (allocated(magLats)) return

    allocate(magLats(nLons, nLats))
    allocate(magMlts(nLons, nLats))

    allocate(potential(nLons, nLats))
    allocate(eDiffuseAvee(nLons, nLats))
    allocate(eDiffuseEflux(nLons, nLats))
    allocate(iDiffuseAvee(nLons, nLats))
    allocate(iDiffuseEflux(nLons, nLats))
    allocate(eMonoAvee(nLons, nLats))
    allocate(eMonoEflux(nLons, nLats))
    allocate(eWaveAvee(nLons, nLats))
    allocate(eWaveEflux(nLons, nLats))
    allocate(polarcap(nLons, nLats))

    if (isGeographic) then
      allocate(geoLats(nLons, nLats))
      allocate(geoLons(nLons, nLats))
      allocate(magLons(nLons, nLats))
      allocate(gseX(nLons, nLats))
      allocate(gseY(nLons, nLats))
      allocate(gseZ(nLons, nLats))
    endif

  end subroutine allocate_all_variables

  ! ----------------------------------------------------------------
  ! Deallocate all variables
  ! ----------------------------------------------------------------

  subroutine deallocate_all_variables

    if (.not. allocated(magLats)) return

    deallocate(magLats)
    deallocate(magMlts)

    deallocate(potential)
    deallocate(eDiffuseAvee)
    deallocate(eDiffuseEflux)
    deallocate(iDiffuseAvee)
    deallocate(iDiffuseEflux)
    deallocate(eMonoAvee)
    deallocate(eMonoEflux)
    deallocate(eWaveAvee)
    deallocate(eWaveEflux)
    deallocate(polarcap)

    if (isGeographic) then
      deallocate(geoLats)
      deallocate(geoLons)
      deallocate(magLons)
      deallocate(gseX)
      deallocate(gseY)
      deallocate(gseZ)
    endif

  end subroutine deallocate_all_variables

end Module ModMain
