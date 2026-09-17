!  Copyright (C) 2024 Regents of the University of Michigan, portions
!  used with permission For more information, see
!  https://github.com/GITMCode/Electrodynamics

Module ModAMIE_Interface

  use ModCharSize
  use ModErrors
  use ModKind
  use ModTimeAmie

  implicit none

  integer :: iUnitAmie_ = 78
  integer, parameter :: nVarsMax = 100
  integer :: AMIE_iDebugLevel = 0
  real, parameter :: bufferTime = 300.0

  ! For now we assume that there are only 2 files, but this is
  ! hopefully written in such a generic way that there can be more
  ! files in the future:
  integer :: nFiles = 2

  ! These are the variable types this interface understands:
  integer, parameter :: iPotential_ = 1
  integer, parameter :: iPotentialy_ = 2
  integer, parameter :: iEle_diff_eflux_ = 3
  integer, parameter :: iEle_diff_avee_ = 4
  integer, parameter :: iIon_diff_eflux_ = 5
  integer, parameter :: iIon_diff_avee_ = 6
  integer, parameter :: iEle_mono_eflux_ = 7
  integer, parameter :: iEle_mono_avee_ = 8
  integer, parameter :: iEle_wave_eflux_ = 9
  integer, parameter :: iEle_wave_avee_ = 10
  integer, parameter :: iPolarCap_ = 11
  integer, parameter :: nValues = 11
  integer, parameter :: nNamesMax = 200
  integer :: nNames = 0

  ! This is the maximum size of the memory we want to take, so we
  ! don't run out of memory (this is equivalent to a course grid at
  ! 1 minute resolution with only 3 variables):
  integer :: nMaxSize = 25*20*3*1440

  ! The names of the variables are defined in this array, and initialized
  ! in the function below
  character(len=iCharLenIE_) :: AMIE_Names(nValues)
  character(len=iCharLenIE_) :: AMIE_Names_List(nNamesMax)
  integer :: AMIE_Names_Mapper(nNamesMax)

  ! --------------------------------------------------------------------
  ! AMIE file storage as a structure?
  ! --------------------------------------------------------------------

  type, public :: amieFile

    logical :: isOk = .true.
    character(len=iCharLenIE_) :: fileName = "none"

    ! ----------------------------------------------------------------
    ! ----------------------------------------------------------------
    integer :: nLats = 0
    integer :: nMLTs = 0
    integer :: nVars = 0
    integer :: nTimes = 0
    integer :: nTimesGoal = 0
    integer :: nVarsInMemory = 0

    real(kind=Real8_), allocatable, dimension(:) :: times
    real*4, allocatable, dimension(:) :: lats, mlts
    character(len=30), dimension(nVarsMax) :: varNames = ''

    logical :: ReverseLats = .false.
    logical :: IsMirror = .false.
    logical :: IsNorth = .true.

    integer :: headerLength = 0
    integer :: oneTimeLength = 0

    ! Figure out which variables are in the file
    logical :: hasPotential = .false.
    logical :: hasDiffuse = .false.
    logical :: hasIons = .false.
    logical :: hasMono = .false.
    logical :: hasWave = .false.
    integer, dimension(nValues) :: iMap_ = -1
    real, dimension(nValues) :: unitConvert = 1.0

    integer :: nTimesInMemory = 0
    real(kind=Real8_), allocatable, dimension(:) :: timesInMemory

    ! mlts, lats, (subsample of) times, and variables
    real*4, allocatable, dimension(:, :, :, :) :: dataInMemory

    ! mlts, lats, and variables at one single time
    real, allocatable, dimension(:, :, :) :: dataOneTime

  contains

    ! Initialize the library:
    procedure :: init => initialize

  end type amieFile

  type(amieFile), allocatable, dimension(:) :: allFiles

  integer :: nCellsPad

  integer :: nMltsNeeded, nLatsNeeded
  integer, allocatable, dimension(:, :, :) :: interpolationIndices
  real, allocatable, dimension(:, :, :) :: interpolationRatios

  real :: rDummyeFlux = 1.0e-6 ! in W/m2
  real :: rDummyAveE = 2.0 ! in keV
  real :: rDummyIonAveE = 20.0 ! in keV

  integer, parameter :: AMIE_Closest_ = 1
  integer, parameter :: AMIE_After_ = 2
  integer, parameter :: AMIE_Interpolate_ = 3

  ! mlts, lats, blocks, times, and variable
  real*4, allocatable, dimension(:, :, :, :, :) :: AMIE_Storage

contains

  ! --------------------------------------------------------------------
  ! These are the names of the variables we will be looking for
  ! in the AMIE input files.  If you want to use these types of
  ! aurora, you need to use these names for the variables!
  ! --------------------------------------------------------------------

  subroutine add_to_list_and_mapper(iVar_, cName)

    implicit none
    character(len=*), intent(in) :: cName
    integer, intent(in) :: iVar_

    nNames = nNames + 1
    AMIE_Names_List(nNames) = cName
    AMIE_Names_Mapper(nNames) = iVar_

  end subroutine add_to_list_and_mapper

  subroutine AMIE_link_variable_names()

    ! The maximum length for variable names is 30

    implicit none

    ! All of the names for Potential:
    call add_to_list_and_mapper(iPotential_, "Potential")
    call add_to_list_and_mapper(iPotential_, "Potential (kV)")
    call add_to_list_and_mapper(iPotential_, "Potential (V)")
    call add_to_list_and_mapper(iPotential_, "Electric Potential")
    call add_to_list_and_mapper(iPotentialy_, "PotentialY")

    ! All of the names for electron energy flux (diffuse):
    call add_to_list_and_mapper(iEle_diff_eflux_, "Electron Energy Flux")
    call add_to_list_and_mapper(iEle_diff_eflux_, "Electron Energy Flux (ergs/cm2")
    call add_to_list_and_mapper(iEle_diff_eflux_, "Electron Energy Flux (mW/m2)")
    call add_to_list_and_mapper(iEle_diff_eflux_, "Auroral Energy Flux (er/cm2/s)")

    ! All of the names for electron average energy (diffuse):
    call add_to_list_and_mapper(iEle_diff_avee_, "Electron Mean Energy")
    call add_to_list_and_mapper(iEle_diff_avee_, "Electron Mean Energy (keV)")
    call add_to_list_and_mapper(iEle_diff_avee_, "Auroral Mean Energy (keV)")

    ! All of the names for ion energy flux:
    call add_to_list_and_mapper(iIon_diff_eflux_, "Ion Energy Flux")
    call add_to_list_and_mapper(iIon_diff_eflux_, "Ion Energy Flux (ergs/cm2/s)")
    call add_to_list_and_mapper(iIon_diff_eflux_, "Ion Energy Flux (mW/m2)")

    ! All of the names for ion average energy:
    call add_to_list_and_mapper(iIon_diff_avee_, "Ion Mean Energy")
    call add_to_list_and_mapper(iIon_diff_avee_, "Ion Mean Energy (keV)")

    ! Names for mono, waves, and polar cap:
    call add_to_list_and_mapper(iEle_mono_eflux_, "ME Energy Flux")
    call add_to_list_and_mapper(iEle_mono_eflux_, "ME Energy Flux (ergs/cm2/s)")
    call add_to_list_and_mapper(iEle_mono_avee_, "ME Mean Energy")
    call add_to_list_and_mapper(iEle_mono_avee_, "ME Mean Energy (keV)")
    call add_to_list_and_mapper(iEle_wave_eflux_, "BB Energy Flux")
    call add_to_list_and_mapper(iEle_wave_eflux_, "BB Energy Flux (ergs/cm2/s)")
    call add_to_list_and_mapper(iEle_wave_avee_, "BB Mean Energy")
    call add_to_list_and_mapper(iEle_wave_avee_, "BB Mean Energy (keV)")
    call add_to_list_and_mapper(iPolarCap_, "Polar Cap Indicator")

    ! These are the "official" names for each data type (not searched for...)
    AMIE_Names(iPotential_) = "Potential"
    AMIE_Names(iPotentialy_) = "PotentialY"
    AMIE_Names(iEle_diff_eflux_) = "Electron Energy Flux"
    AMIE_Names(iEle_diff_avee_) = "Electron Mean Energy"
    AMIE_Names(iIon_diff_eflux_) = "Ion Energy Flux"
    AMIE_Names(iIon_diff_avee_) = "Ion Mean Energy"
    AMIE_Names(iEle_mono_eflux_) = "ME Energy Flux"
    AMIE_Names(iEle_mono_avee_) = "ME Mean Energy"
    AMIE_Names(iEle_wave_eflux_) = "BB Energy Flux"
    AMIE_Names(iEle_wave_avee_) = "BB Mean Energy"
    AMIE_Names(iPolarCap_) = "Polar Cap Indicator"

  end subroutine AMIE_link_variable_names

  ! --------------------------------------------------------------------
  ! Initialize file
  ! --------------------------------------------------------------------

  subroutine initialize(this, fileIn, isNorthIn, isMirrorIn)

    implicit none

    class(amieFile) :: this
    character(len=iCharLenIE_), intent(in) :: fileIn
    logical, intent(in) :: isNorthIn
    logical, intent(in) :: isMirrorIn
    integer :: iError = 0, iVar, i

    this%fileName = fileIn
    this%isNorth = isNorthIn
    this%isMirror = isMirrorIn

    if (AMIE_iDebugLevel > -1) then
      if (isNorthIn) then
        write(*, *) " => Reading AMIE file (north) : ", trim(fileIn)
      else
        write(*, *) " => Reading AMIE file (south) : ", trim(fileIn)
      endif
      if (isMirrorIn) write(*, *) "  ==> This file is a mirror"
    endif

    call read_amie_header(this)
    call read_amie_times(this)

    allocate(this%timesInMemory(this%nTimesGoal), stat=iError)
    if (iError /= 0) then
      call set_error("Error trying to allocate timesInMemory in initialize")
      return
    endif

    allocate(this%dataInMemory( &
             nValues, &
             this%nMlts, &
             this%nLats + nCellsPad, &
             this%nTimesGoal), &
             stat=iError)
    this%dataInMemory = 0.0
    if (iError /= 0) then
      call set_error("Error trying to allocate dataInMemory in initialize")
      return
    endif

    allocate(this%dataOneTime( &
             nValues, &
             this%nMlts, &
             this%nLats + nCellsPad), &
             stat=iError)
    if (iError /= 0) then
      call set_error("Error trying to allocate dataOneTime in initialize")
      return
    endif

  end subroutine initialize

  ! --------------------------------------------------------------------
  ! Update the AMIE files so that it contains the data for the present
  ! time
  ! --------------------------------------------------------------------

  subroutine update_amie_files(timeIn)

    implicit none

    real(kind=Real8_), intent(in) :: timeIn
    integer iFile
    real(kind=Real8_) :: sTime, eTime
    logical didFindFile

    didFindFile = .false.
    ! Look for north first
    do iFile = 1, nFiles
      if (allFiles(iFile)%isNorth) then
        sTime = allFiles(iFile)%times(1)
        eTime = allFiles(iFile)%times(AllFiles(iFile)%nTimes)
        if (timeIn >= sTime .and. timeIn <= eTime) then
          call read_amie_data(allFiles(iFile), timeIn)
          didFindFile = .true.
        else
          allFiles(iFile)%nTimesInMemory = 0
        endif
      endif
    enddo

    if (.not. didFindFile) then
      ! open up search
      do iFile = 1, nFiles
        if (allFiles(iFile)%isNorth) then
          sTime = allFiles(iFile)%times(1) - bufferTime
          eTime = allFiles(iFile)%times(AllFiles(iFile)%nTimes)
          if (iFile == nFiles) eTime = eTime + bufferTime
          if (timeIn >= sTime .and. timeIn <= eTime) then
            call read_amie_data(allFiles(iFile), timeIn)
            didFindFile = .true.
          else
            allFiles(iFile)%nTimesInMemory = 0
          endif
        endif
      enddo
    endif

    if (.not. didFindFile) &
      call set_error("Error trying to find time in AMIE files (north)!")

    didFindFile = .false.
    ! Look for south
    do iFile = 1, nFiles
      if (.not. allFiles(iFile)%isNorth) then
        sTime = allFiles(iFile)%times(1)
        eTime = allFiles(iFile)%times(AllFiles(iFile)%nTimes)
        if (timeIn >= sTime .and. timeIn <= eTime) then
          call read_amie_data(allFiles(iFile), timeIn)
          didFindFile = .true.
        else
          allFiles(iFile)%nTimesInMemory = 0
        endif
      endif
    enddo

    if (.not. didFindFile) then
      ! open up search
      do iFile = 1, nFiles
        if (.not. allFiles(iFile)%isNorth) then
          sTime = allFiles(iFile)%times(1) - bufferTime
          eTime = allFiles(iFile)%times(AllFiles(iFile)%nTimes)
          if (iFile == nFiles) eTime = eTime + bufferTime
          if (timeIn >= sTime .and. timeIn <= eTime) then
            call read_amie_data(allFiles(iFile), timeIn)
            didFindFile = .true.
          else
            allFiles(iFile)%nTimesInMemory = 0
          endif
        endif
      enddo
    endif

    if (.not. didFindFile) &
      call set_error("Error trying to find time in AMIE files (south)!")

  end subroutine update_amie_files

  ! --------------------------------------------------------------------
  ! This finds the index of time in the list of times that is before
  ! or equal to the input time
  ! --------------------------------------------------------------------

  subroutine find_time(timeList, nTimes, timeIn, indexOut)

    implicit none
    integer, intent(in) :: nTimes
    real(kind=Real8_), intent(in) :: timeList(nTimes)
    real(kind=Real8_), intent(in) :: timeIn
    integer, intent(out) :: indexOut
    integer :: iMin, iMax, iMid
    logical :: isFound

    iMin = 1
    iMax = nTimes
    isFound = .false.

    do while (.not. isFound)

      iMid = (iMax + iMin)/2
      if (iMid == iMin .or. iMid == iMax) then
        isFound = .true.
      else
        if (timeList(iMid) == timeIn) then
          isFound = .true.
        else
          if (timeList(iMid) > timeIn) then
            iMax = iMid
          else
            iMin = iMid
          endif
        endif
      endif
    enddo

    if (timeList(iMid) == timeIn) then
      indexOut = iMid
    else
      indexOut = iMin
    endif

  end subroutine find_time

  ! --------------------------------------------------------------------
  ! This function loads the array dataInMemory, which is most likely
  ! a subset of the whole file
  ! --------------------------------------------------------------------

  subroutine read_amie_data(this, startTime)

    implicit none

    class(amieFile) :: this
    real(kind=Real8_), intent(in) :: startTime

    integer :: iError = 0
    integer :: ntemp, iyr, imo, ida, ihr, imi, iTime
    integer :: iFilePos
    integer :: startIndex, endIndex
    integer, dimension(7) :: itime_i
    logical :: doReadMore

    real*4, allocatable, dimension(:, :, :) :: AllDataOneTime
    real(kind=Real8_) :: rtime
    real*4  :: swv, bx, by, bz, aei, ae, au, al, dsti, dst, hpi, sjh, pot
    real :: dPotential

    integer :: i, j, iField, n, iVal, iT
    integer(4) :: recl_start, recl_end ! record start/end length, pads all data

    doReadMore = .false.

    if (this%nTimesInMemory == 0) then
      doReadMore = .true.
    else
      if (startTime < this%timesInMemory(1) .or. &
          startTime >= this%timesInMemory(this%nTimesInMemory)) then
        doReadMore = .true.
      endif
    endif

    if (doReadMore) then

      if (AMIE_iDebugLevel > 1) &
        write(*, *) "==> Need to read more AMIE data!"

      call find_time(this%times, this%nTimes, startTime, startIndex)
      endIndex = startIndex + this%nTimesGoal - 1

      if (AMIE_iDebugLevel > 0) &
        write(*, *) 'looking for times : ', this%times(this%nTimes), startTime

      if (startTime > this%times(this%nTimes)) write(*, *) "time exceeded!"

      if (endIndex > this%nTimes) endIndex = this%nTimes

      if (AMIE_iDebugLevel > 1) &
        write(*, *) "==> Opening AMIE file to read data : ", &
        trim(this%fileName)

      if (allocated(AllDataOneTime)) deallocate(AllDataOneTime)
      allocate(AllDataOneTime(this%nMlts, this%nLats, this%nVars), &
               stat=iError)
      if (iError /= 0) then
        call set_error("Error trying to allocate AllDataOneTime in AMIE")
      endif

      open(iUnitAmie_, &
           file=this%fileName, &
           status='old', &
           access='stream', &
           form='unformatted', &
           iostat=iError)

      if (iError /= 0) then
        call set_error("Error trying to open file in read_amie_data: ")
        call set_error(this%fileName)
        return
      endif

      ! Move to the correct time:
      this%nTimesInMemory = 0
      do iTime = startIndex, endIndex
        iT = iTime - startIndex + 1
        iFilePos = this%headerLength + this%oneTimeLength*(iTime - 1)
        ! Stream seek to the start of this time block, then read the first
        ! (time-stamp) record, bracketed by its 4-byte length markers.
        read(iUnitAmie_, pos=iFilePos + 1) &
          recl_start, ntemp, iyr, imo, ida, ihr, imi, recl_end
        if (AMIE_iDebugLevel > 2) &
          write(*, *) '====> Reading AMIE time : ', &
          ntemp, iyr, imo, ida, ihr, imi

        itime_i(1) = iyr
        itime_i(2) = imo
        itime_i(3) = ida
        itime_i(4) = ihr
        itime_i(5) = imi
        itime_i(6) = 0
        itime_i(7) = 0
        call time_int_to_real(itime_i, rtime)
        this%nTimesInMemory = this%nTimesInMemory + 1
        this%timesInMemory(this%nTimesInMemory) = rtime

        ! We don't care about these,
        ! but read them to move forward in the file:
        read(iUnitAmie_) recl_start, swv, bx, by, bz, aei, ae, au, al, &
          dsti, dst, hpi, sjh, pot, recl_end

        do iField = 1, this%nVars
          if (this%ReverseLats) then
            read(iUnitAmie_) recl_start, &
              ((AllDataOneTime(j, i, iField), &
                j=1, this%nMlts), &
               i=this%nLats, 1, -1), &
              recl_end
          else
            read(iUnitAmie_) recl_start, &
              ((AllDataOneTime(j, i, iField), &
                j=1, this%nMlts), &
               i=1, this%nLats), &
              recl_end
          endif
        enddo

        ! We need Potential to be in Volts
        !         AveE to be in keV
        !         EFlux to be in W/m2

        if (AMIE_iDebugLevel > 1) write(*, *) '  --> Pushing in to storage '
        do iVal = 1, nValues
          if (this%iMap_(iVal) > 0) then
            ! if we are mirroring, we need to do something special
            !  but only if we are talking about the potential:
            if ((this%IsMirror) .and. &
                ((iVal == iPotential_) .or. (iVal == iPotentialY_))) then
              do i = 1, this%nMlts
                this%dataInMemory(iVal, i, 1:this%nLats, iT) = &
                  -AllDataOneTime(this%nMlts + 1 - i, 1:this%nLats, this%iMap_(iVal))
              enddo
            else
              ! This is the default :
              !    (need loop for optimization issues in gfortran...)
              do i = 1, this%nMlts
                this%dataInMemory(iVal, i, 1:this%nLats, iT) = &
                  AllDataOneTime(i, 1:this%nLats, this%iMap_(iVal))
              enddo
            endif

            ! If the variable is the potential, linearly
            ! interpolate it to lower latitudes, and then smooth
            ! it in mlt:
            if ((iVal == iPotential_) .or. (iVal == iPotentialY_)) then
              ! First extend the potential:
              do i = 1, this%nMlts
                dPotential = &
                  this%dataInMemory(iVal, i, this%nLats, iT)/ &
                  nCellsPad
                do j = this%nLats + 1, this%nLats + nCellsPad
                  this%dataInMemory(iVal, i, j, iT) = &
                    this%dataInMemory(iVal, i, j - 1, iT) - &
                    dPotential
                enddo
              enddo
              ! Then smooth the extension in MLT:
              do j = this%nLats + 1, this%nLats + nCellsPad
                ! We are going to smooth more and more as we go
                ! down in latitude
                do n = 1, j - this%nLats
                  i = 1
                  this%dataInMemory(iVal, i, j, iT) = &
                    (this%dataInMemory(iVal, this%nMlts - 1, j, iT) + &
                     2*this%dataInMemory(iVal, i, j, iT) + &
                     this%dataInMemory(iVal, i + 1, j, iT))/4.0

                  do i = 2, this%nMlts - 1
                    this%dataInMemory(iVal, i, j, iT) = &
                      (this%dataInMemory(iVal, i - 1, j, iT) + &
                       2*this%dataInMemory(iVal, i, j, iT) + &
                       this%dataInMemory(iVal, i + 1, j, iT))/4.0
                  enddo

                  i = this%nMlts
                  this%dataInMemory(iVal, i, j, iT) = &
                    (this%dataInMemory(iVal, i - 1, j, iT) + &
                     2*this%dataInMemory(iVal, i, j, iT) + &
                     this%dataInMemory(iVal, 2, j, iT))/4.0

                enddo
              enddo
            endif
          endif
        enddo

      enddo

      close(iUnitAmie_)
      deallocate(AllDataOneTime)
    endif

    ! Now let's pull out one time and put it into the single time array:

    call find_time( &
      this%timesInMemory, &
      this%nTimesInMemory, &
      startTime, &
      startIndex)
    call time_real_to_int(this%timesInMemory(startIndex), itime_i)
    this%dataOneTime(:, :, :) = this%dataInMemory(:, :, :, startIndex)

    return

  end subroutine read_amie_data

  ! --------------------------------------------------------------------
  ! Read all of the times in the AMIE file
  ! --------------------------------------------------------------------

  subroutine read_amie_times(this)

    implicit none

    class(amieFile) :: this
    integer :: iError = 0
    integer :: ntemp, iyr, imo, ida, ihr, imi, iTime
    integer :: iFilePos
    integer, dimension(7) :: itime_i
    real(kind=Real8_) :: rtime
    integer(4) :: recl_start, recl_end

    if (AMIE_iDebugLevel > 1) &
      write(*, *) "==> Opening AMIE file to read times : ", trim(this%fileName)

    open(iUnitAmie_, &
         file=this%fileName, &
         status='old', &
         access='stream', &
         form='unformatted', &
         iostat=iError)

    if (iError /= 0) then
      call set_error("Error trying to open file in read_amie_times: ")
      call set_error(this%fileName)
      return
    endif

    do iTime = 1, this%ntimes

      iFilePos = this%headerLength + this%oneTimeLength*(iTime - 1)
      ! Stream seek to the start of this time block, then read the first record
      ! (time-stamp), bracketed by its 4-byte length markers.
      read(iUnitAmie_, pos=iFilePos + 1) &
        recl_start, ntemp, iyr, imo, ida, ihr, imi, recl_end
      if (AMIE_iDebugLevel > 2) then
        write(*, *) '====> Reading AMIE time : ', ntemp, iyr, imo, ida, ihr, imi
        write(*, *) '====> At file position: ', iFilePos, '  <===='
      endif

      itime_i(1) = iyr
      itime_i(2) = imo
      itime_i(3) = ida
      itime_i(4) = ihr
      itime_i(5) = imi
      itime_i(6) = 0
      itime_i(7) = 0
      call time_int_to_real(itime_i, rtime)
      this%times(iTime) = rtime

    enddo

    close(iUnitAmie_)

    return

  end subroutine read_amie_times

  ! --------------------------------------------------------------------
  ! Read AMIE header and allocate some variables:
  ! --------------------------------------------------------------------

  subroutine read_amie_header(this)

    implicit none

    class(amieFile) :: this
    integer :: iError = 0, iVar, i, tmp_amiepos
    real*4, allocatable, dimension(:) :: TempLats

    integer(4) :: recl_start, recl_end

    if (AMIE_iDebugLevel > 1) &
      write(*, *) "==> Opening AMIE file to read header : ", trim(this%fileName)

    open(iUnitAmie_, &
         file=this%fileName, &
         status='old', &
         access='stream', &
         form='unformatted', &
         iostat=iError)

    if (iError /= 0) then
      call set_error("Error trying to open file in read_amie_header: ")
      call set_error(this%fileName)
      return
    endif

    ! Each record is bracketed by an (int4) length marker;
    ! These are read into recl_start / recl_end around every data we want

    ! Read nLats, nMlts, and nTimes
    read(iUnitAmie_) recl_start, this%nLats, this%nMlts, this%nTimes, recl_end

    if (AMIE_iDebugLevel > 2) &
      write(*, *) "==> nLats, nMlts, nTimes : ", &
      this%nLats, this%nMlts, this%nTimes

    allocate(this%times(this%nTimes), stat=iError)
    if (iError /= 0) then
      call set_error("Error trying to allocate times in read_amie_header")
      return
    endif
    allocate(this%lats(this%nLats), stat=iError)
    if (iError /= 0) then
      call set_error("Error trying to allocate lats in read_amie_header")
      return
    endif
    allocate(this%mlts(this%nMlts), stat=iError)
    if (iError /= 0) then
      call set_error("Error trying to allocate mlts in read_amie_header")
      return
    endif

    read(iUnitAmie_) &
      recl_start, (this%lats(i), i=1, this%nLats), recl_end, & ! lats
      recl_start, (this%mlts(i), i=1, this%nMlts), recl_end, & ! mlt
      recl_start, this%nVars, recl_end ! number of vars
    this%lats = 90.0 - this%lats

    do iVar = 1, this%nVars
      read(iUnitAmie_) recl_start, this%varNames(iVar), recl_end
      if (AMIE_iDebugLevel > 0) &
        write(*, *) "==> AMIE Variable : ", iVar, this%varNames(iVar)
    enddo

    ! We expect the data to be arranged from high latitudes to low
    ! latitudes.  If it is not, we need to reverse things.
    if (this%lats(this%nLats) > this%lats(1)) then
      this%ReverseLats = .true.
      if (allocated(TempLats)) deallocate(TempLats)
      allocate(TempLats(this%nLats), stat=iError)
      if (iError /= 0) then
        call set_error("Error: allocating templats in AMIE")
      endif
      TempLats = this%lats
      do i = 1, this%nLats
        this%lats(i) = TempLats(this%nLats + 1 - i)
      enddo
      this%ReverseLats = .true.
      deallocate(TempLats)
    endif

    !\
    ! We have run into a problem with AMIE during storms.
    ! The potential is not zero at the boundary.  It is sometimes quite
    ! high.  This means that the gradient in the potential will be
    ! large - meaning that very strong flows can exist in this last
    ! cell.  This is not good.
    ! To rectify this, we will pad the AMIE results by 15 grid cells, and
    ! force the potential to go to zero linearly from the last cell to the
    ! new last cell.
    ! Since it is assumed that all of the AMIE quantities are on the same
    ! grid, we have to fill in the eflux and avee also.  We will use the
    ! last cell to fill in those value.
    ! We also have to extend the grid.
    ! - We don't really need this for SWMF or other products that extend to
    !   lower latitudes.
    !/

    if (abs(this%lats(this%nLats)) < 40.0) then
      nCellsPad = 1
    else
      nCellsPad = 15
    endif

    ! Now that we know how many cells to pad, we need to remake the latitude
    ! array. Seems hacky, but it works:

    if (allocated(TempLats)) deallocate(TempLats)
    allocate(TempLats(this%nLats + nCellsPad), stat=iError)
    if (iError /= 0) then
      call set_error("Error: allocating templats (2nd time!) in AMIE")
    endif
    TempLats(1:this%nLats) = this%lats

    if (allocated(this%lats)) deallocate(this%lats)
    allocate(this%Lats(this%nLats + nCellsPad), stat=iError)
    this%lats = TempLats
    do i = this%nLats + 1, this%nLats + nCellsPad
      this%Lats(i) = this%Lats(i - 1) + &
                     (this%Lats(this%nLats) - this%Lats(this%nLats - 1))
    enddo
    if (this%lats(1) > 89.9) this%lats(1) = 90.0

    ! The header length should be this:
    this%headerLength = &
      4*3 + 8 + &
      4*this%nLats + 8 + &
      4*this%nMlts + 8 + &
      4 + 8 + &
      (30 + 8)*this%nVars

    if (AMIE_iDebugLevel > 2) &
      write(*, *) 'calculated header length : ', this%headerLength

    ! Measure the actual header length:
    ! inquire(pos=X) on a access='stream' file returns the position of the next
    ! read indexed from 1, so our location is pos - 1.
    ! Replaces an earlier `ftell` call since ifort/ifx/nagfor returned garbage.
    inquire(unit=iUnitAmie_, pos=tmp_amiepos)
    tmp_amiepos = tmp_amiepos - 1

    if (AMIE_iDebugLevel >= 1) &
      write(*, *) ' --> current AMIE file position (inquire) : ', tmp_amiepos

    close(iUnitAmie_)

    if (tmp_amiepos .ne. this%headerLength) then
      if (AMIE_iDebugLevel > -1) then
        write(*, *) 'WARNING ---->>>> AMIE file header length is strange!'
        write(*, *) '   calculated header length : ', this%headerLength
        write(*, *) '   measured length : ', tmp_amiepos
        write(*, *) '   -> forcing length to measured instead of calculated!'
        write(*, *) '   -> if code dies or behaves unexpectedly, then complain to Aaron!'
      endif
      this%headerLength = tmp_amiepos
    endif

    this%oneTimeLength = &
      6*4 + 8 + &
      13*4 + 8 + &
      (this%nMlts*this%nLats*4 + 8)*this%nVars

    this%nTimesGoal = nMaxSize/(this%nMlts*this%nLats*nValues)
    if (AMIE_iDebugLevel > 2) &
      write(*, *) "==> AMIE nTimesGoals : ", this%nTimesGoal

  end subroutine read_amie_header

  ! --------------------------------------------------------------------
  ! Initialize AMIE file system
  ! --------------------------------------------------------------------

  subroutine initialize_amie_files(nFilesNorth, filesNorth, nFilesSouth, filesSouth, iDebugLevel)

    implicit none

    integer, intent(in) :: nFilesNorth, nFilesSouth
    character(len=iCharLenIE_), intent(in) :: filesNorth(nFilesNorth), filesSouth(nFilesSouth)
    integer, intent(in) :: iDebugLevel
    logical :: FileExists
    integer :: iError, iFile

    ! 1. set the debug level:
    AMIE_iDebugLevel = iDebugLevel

    ! 2. Define the variable names to look for in the files:
    !    This is hardcoded stuff, so check this function to
    !    understand what variables the code is looking for.
    call AMIE_link_variable_names()

    ! 2.5 Before the next step, let's make sure that AMIE files are set & exist
    nFiles = 0
    do iFile = 1, nFilesNorth
      inquire(file=trim(filesNorth(iFile)), exist=FileExists)
      if (.not. FileExists) then
        call set_error("Error: AMIE North file does not exist! Ensure it is set correctly. ")
        call set_error(trim(filesNorth(iFile)))
      else
        nFiles = nFiles + 1
      endif
    enddo

    do iFile = 1, nFilesSouth
      if (trim(filesSouth(iFile)) /= 'mirror') then
        inquire(file=trim(filesSouth(iFile)), exist=FileExists)
        if (.not. FileExists) then
          call set_error("Error: AMIE South file does not exist! Ensure it is set correctly.")
          call set_error(filesSouth(iFile))
        else
          nFiles = nFiles + 1
        endif
      else
        nFiles = nFiles + 1
      endif
    enddo

    ! 3. Initialize the Northern Hemisphere File:

    allocate(allFiles(nFiles), stat=iError)
    if (iError /= 0) then
      call set_error("Error: allocating allFiles in initiailize_amie_files")
    endif

    ! isNorth = .true., isMirror = .false.
    do iFile = 1, nFilesNorth
      call allFiles(iFile)%init(filesNorth(iFile), .true., .false.)
      call AMIE_link_vars_to_keys(allFiles(iFile))
    enddo

    do iFile = 1, nFilesSouth
      if (trim(filesSouth(iFile)) == 'mirror') then
        ! isNorth = .false., isMirror = .true.
        call allFiles(nFilesNorth + iFile)%init(filesNorth(iFile), .false., .true.)
      else
        ! isNorth = .false., isMirror = .false.
        call allFiles(nFilesNorth + iFile)%init(filesSouth(iFile), .false., .false.)
      endif
      call AMIE_link_vars_to_keys(allFiles(nFilesNorth + iFile))
    enddo

    return

  end subroutine initialize_amie_files

  ! --------------------------------------------------------------------
  ! Link all of the variable names in the files to the proper pointers
  ! --------------------------------------------------------------------

  subroutine AMIE_link_vars_to_keys(this)

    implicit none

    class(amieFile) :: this
    integer :: iField, iVal, iName
    logical :: isFound

    do iField = 1, this%nVars
      if (AMIE_iDebugLevel > 0) &
        write(*, *) '==> Trying to match ->', trim(this%varNames(iField)), '<-'
      isFound = .false.
      do iName = 1, nNames
        if ((AMIE_iDebugLevel > 0) .and. (.not. isFound)) &
          write(*, *) '    --> comparing to ->', trim(AMIE_Names_List(iName)), '<-'

        if ((index(trim(this%varNames(iField)), trim(AMIE_Names_List(iName))) > 0) .and. &
            (index(trim(AMIE_Names_List(iName)), trim(this%varNames(iField))) > 0) .and. &
            (.not. isFound)) then
          this%iMap_(AMIE_Names_Mapper(iName)) = iField
          isFound = .true.
          ! Since we found the variable, let's kill it in the AMIE list:
          if (AMIE_iDebugLevel > 0) then
            write(*, *) "   <--- ", trim(AMIE_Names_List(iName)), " Found"
            write(*, *) "     Amie field, list number : ", iField, AMIE_Names_Mapper(iName)
          endif
        endif
      enddo
    enddo
    if (AMIE_iDebugLevel > 0) then
      write(*, *) '==> Summary of variables found in AMIE file : '
      do iVal = 1, nValues
        if (this%iMap_(iVal) > 0) then
          write(*, *) '==> Expected : ', trim(AMIE_Names(iVal)), &
            '; found : ', trim(this%varNames(this%iMap_(iVal)))
        endif
      enddo
    endif

    if (.not. isOk) return

    ! Check to see if some of these exist:
    if (this%iMap_(iPotential_) > 0) then
      this%hasPotential = .true.
    endif
    if ((this%iMap_(iEle_diff_eflux_) > 0) .and. (this%iMap_(iEle_diff_avee_) > 0)) then
      !call set_error("Could not find Electron Diffuse in file!")
      this%hasDiffuse = .true.
    endif

    if ((this%iMap_(iIon_diff_eflux_) > 0) .and. (this%iMap_(iIon_diff_avee_) > 0)) then
      this%hasIons = .true.
      if (AMIE_iDebugLevel > 1) &
        write(*, *) "==> Input Electrodynamics is using Ions!"
    else
      this%hasIons = .false.
    endif
    if ((this%iMap_(iEle_mono_eflux_) > 0) .and. (this%iMap_(iEle_mono_avee_) > 0)) then
      this%hasMono = .true.
      if (AMIE_iDebugLevel > 1) &
        write(*, *) "==> Input Electrodynamics is using Mono!"
    else
      this%hasMono = .false.
    endif
    if ((this%iMap_(iEle_wave_eflux_) > 0) .and. (this%iMap_(iEle_wave_avee_) > 0)) then
      this%hasWave = .true.
      if (AMIE_iDebugLevel > 1) &
        write(*, *) "==> Input Electrodynamics is using Waves!"
    else
      this%hasWave = .false.
    endif

  end subroutine AMIE_link_vars_to_keys

  ! --------------------------------------------------------------------
  !\
  ! This routine finds a point on in the spherical file system, given
  ! a Theta, Phi:
  ! LocIn(1) = Phi
  ! LocIn(2) = Theta
  ! It returns a 4 element array:
  ! LocOut(1) = Index of Block
  ! LocOut(2) = Index of Longitude
  ! LocOut(3) = Index of Latitude
  ! LocOut(4) = Multiplication factor for Longitude
  ! LocOut(5) = Multiplication factor for Latitude
  !/
  ! --------------------------------------------------------------------

  subroutine FindPoint(LocIn, LocOut)

    use ModErrors

    implicit none

    real, dimension(2), intent(in)  :: LocIn
    real, dimension(5), intent(out) :: LocOut
    real :: MLTIn, LatIn, MLTUp, MLTDown
    integer :: j, i, iBLK
    real :: sig = 1.0

    logical :: IsFound

    LocOut = -1.0

    !\
    ! Check to see if the point is even on the grid.
    !/

    MLTIn = mod(LocIn(1) + 24.0, 24.0)

    LatIn = LocIn(2)
    if (LatIn > 90.0) then
      LatIn = 180.0 - LatIn
      MLTIn = mod(MLTIn + 12.0, 24.0)
    endif
    if (LatIn < -90.0) then
      LatIn = -180.0 - LatIn
      MLTIn = mod(MLTIn + 12.0, 24.0)
    endif

    if (MLTIn > 24.0 .or. MLTIn < 0 .or. LatIn > 90.0 .or. LatIn < -90.0) then
      call set_error("Input lat / mlt is outside of -90-90 and 0-24 range! (AMIE FindPoint)")
      return
    endif

    iBLK = 1
    do while (iBLK <= nFiles)
      j = 1
      if (allFiles(iBLK)%isNorth) then
        sig = 1.0
      else
        sig = -1.0
      endif
      do while (j < allFiles(iBLK)%nMLTs)
        i = 1
        do while (i < allFiles(iBLK)%nLats)

          !\
          ! Check to see if the point is within the current cell
          !/

          MLTUp = allFiles(iBLK)%mlts(j + 1)
          MLTDown = allFiles(iBLK)%mlts(j)
          if (MLTUp == 0.0 .and. MLTDown >= 23.0) MLTUp = 24.0

          ! This assume that we start at the pole and go to lower latitudes:
          if (sig*LatIn <= allFiles(iBLK)%Lats(i) .and. &
              sig*LatIn > allFiles(iBLK)%Lats(i + 1) .and. &
              MLTIn < MLTUp .and. &
              MLTIn >= MLTDown) then

            !\
            ! If it is, then store the cell number and calculate
            ! the interpolation coefficients.
            !/

            LocOut(1) = iBLK
            LocOut(2) = j
            LocOut(3) = i

            LocOut(4) = (MLTIn - MLTDown)/ &
                        (MLTUp - MLTDown)
            ! To keep the MLT and LAT ratios uses consistent, we need to calculate
            ! the ratios backward, since lat shrinks with increasing index:
            LocOut(5) = (allFiles(iBLK)%Lats(i) - sig*LatIn)/ &
                        (allFiles(iBLK)%Lats(i) - allFiles(iBLK)%Lats(i + 1))

            ! Once we find the point, just push all of the
            ! counters to the max value, which exists all of the
            ! loops:
            iBLK = nFiles
            j = allFiles(iBLK)%nMLTs
            i = allFiles(iBLK)%nLats

          endif

          i = i + 1

        enddo

        j = j + 1

      enddo

      iBLK = iBLK + 1

    enddo

    if (AMIE_iDebugLevel > 4) &
      write(*, *) 'file point!', LatIn, MltIn, LocOut

  end subroutine FindPoint

  ! --------------------------------------------------------------------
  ! Set interpolation indices for the general system
  ! --------------------------------------------------------------------

  subroutine set_interpolation_indices(nMltsIn, nLatsIn, mltsIn, latsIn)

    integer, intent(in) :: nMltsIn, nLatsIn
    real, intent(in) :: mltsIn(nMltsIn, nLatsIn), latsIn(nMltsIn, nLatsIn)

    real, dimension(2) :: mlt_and_lat
    real, dimension(5) :: interpolation_info
    integer :: iError, iLat, iMlt

    nMltsNeeded = nMltsIn
    nLatsNeeded = nLatsIn

    if (AMIE_iDebugLevel > 2) &
      write(*, *) "=> Getting interpolation indices from AMIE files", nMltsIn, nLatsIn
    if (allocated(interpolationIndices)) then
      deallocate(interpolationIndices)
      deallocate(interpolationRatios)
    endif
    allocate(interpolationIndices(nMltsIn, nLatsIn, 3), stat=iError)
    if (iError /= 0) then
      call set_error("Error allocating interpolationIndices!")
      return
    endif
    allocate(interpolationRatios(nMltsIn, nLatsIn, 2), stat=iError)
    if (iError /= 0) then
      call set_error("Error allocating interpolationRatios!")
      return
    endif

    interpolationIndices = -1

    do iMlt = 1, nMltsIn
      do iLat = 1, nLatsIn
        mlt_and_lat(1) = mltsIn(iMlt, iLat)
        mlt_and_lat(2) = latsIn(iMlt, iLat)
        call FindPoint(mlt_and_lat, interpolation_info)
        if (iError == 0) then
          interpolationIndices(iMlt, iLat, 1:3) = interpolation_info(1:3)
          interpolationRatios(iMlt, iLat, 1:2) = interpolation_info(4:5)
        else
          interpolationIndices(iMlt, iLat, 1:3) = -1
        endif
      enddo
    enddo

  end subroutine set_interpolation_indices

  ! --------------------------------------------------------------------
  ! Get Value from File System
  ! --------------------------------------------------------------------

  subroutine get_amie_values(iVarToGetIn, valueOut)

    integer, intent(in) :: iVarToGetIn
    real, intent(inout) ::  valueOut(nMltsNeeded, nLatsNeeded)
    integer :: iMlt, iLat, iB, iM, iL, iFile
    real :: dM, dL
    logical :: isNorth

    valueOut = 0.0

    do iMlt = 1, nMltsNeeded
      do iLat = 1, nLatsNeeded

        iB = interpolationIndices(iMLT, iLat, 1)
        iM = interpolationIndices(iMLT, iLat, 2)
        iL = interpolationIndices(iMLT, iLat, 3)
        dM = interpolationRatios(iMLT, iLat, 1)
        dL = interpolationRatios(iMLT, iLat, 2)

        if (iB > 0) then

          ! We need to figure out which block we should use, since
          ! the find indices could only be called once.  Ugh.
          ! At this point, assume all files / blocks have the same grid
          if (allFiles(iB)%nTimesInMemory == 0) then
            isNorth = allFiles(iB)%isNorth
            ! We have nothing in memory for this file!
            iB = 0
            do iFile = 1, nFiles
              if ((allFiles(iFile)%nTimesInMemory > 0) .and. &
                  (allFiles(iFile)%isNorth .eqv. isNorth)) &
                iB = iFile
            enddo
          endif

          ValueOut(iMLT, iLat) = &
            (1.0 - dM)*(1.0 - dL)*allFiles(iB)%dataOneTime(iVarToGetIn, iM, iL) + &
            (1.0 - dM)*(dL)*allFiles(iB)%dataOneTime(iVarToGetIn, iM, IL + 1) + &
            (dM)*(dL)*allFiles(iB)%dataOneTime(iVarToGetIn, iM + 1, IL + 1) + &
            (dM)*(1.0 - dL)*allFiles(iB)%dataOneTime(iVarToGetIn, iM + 1, IL)
        endif

      enddo
    enddo
    return
  end subroutine get_amie_values

  ! --------------------------------------------------------------------
  ! Get Potential
  ! --------------------------------------------------------------------

  subroutine get_amie_potential(potentialOut)
    real, intent(inout) ::  PotentialOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Electric Potential from AMIE"
    call get_amie_values(iPotential_, PotentialOut)
  end subroutine get_amie_potential

  ! --------------------------------------------------------------------
  ! Get electron diffuse e-flux
  ! --------------------------------------------------------------------

  subroutine get_amie_electron_diffuse_eflux(elecDiffEfluxOut)
    real, intent(inout) ::  elecDiffEfluxOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Electron Diffuse Energy Flux from AMIE"
    call get_amie_values(iEle_diff_eflux_, elecDiffEfluxOut)
  end subroutine get_amie_electron_diffuse_eflux

  ! --------------------------------------------------------------------
  ! Get electron diffuse ave-e
  ! --------------------------------------------------------------------

  subroutine get_amie_electron_diffuse_avee(elecDiffAveEOut)
    real, intent(inout) ::  elecDiffAveEOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Electron Diffuse Average Energy from AMIE"
    call get_amie_values(iEle_diff_avee_, elecDiffAveEOut)
  end subroutine get_amie_electron_diffuse_avee

  ! --------------------------------------------------------------------
  ! Get Ion diffuse e-flux
  ! --------------------------------------------------------------------

  subroutine get_amie_ion_diffuse_eflux(ionDiffEfluxOut)
    real, intent(inout) ::  ionDiffEfluxOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Ion Diffuse Energy Flux from AMIE"
    call get_amie_values(iIon_diff_eflux_, ionDiffEfluxOut)
  end subroutine get_amie_ion_diffuse_eflux

  ! --------------------------------------------------------------------
  ! Get Ion diffuse ave-e
  ! --------------------------------------------------------------------

  subroutine get_amie_ion_diffuse_avee(ionDiffAveEOut)
    real, intent(inout) ::  ionDiffAveEOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Ion Diffuse Average Energy from AMIE"
    call get_amie_values(iIon_diff_avee_, ionDiffAveEOut)
  end subroutine get_amie_ion_diffuse_avee

  ! --------------------------------------------------------------------
  ! Get electron mono e-flux
  ! --------------------------------------------------------------------

  subroutine get_amie_electron_mono_eflux(elecMonoEfluxOut)
    real, intent(inout) ::  elecMonoEfluxOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Electron Mono Energy Flux from AMIE"
    call get_amie_values(iEle_mono_eflux_, elecMonoEfluxOut)
  end subroutine get_amie_electron_mono_eflux

  ! --------------------------------------------------------------------
  ! Get electron mono ave-e
  ! --------------------------------------------------------------------

  subroutine get_amie_electron_mono_avee(elecMonoAveEOut)
    real, intent(inout) ::  elecMonoAveEOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Electron Mono Average Energy from AMIE"
    call get_amie_values(iEle_mono_avee_, elecMonoAveEOut)
  end subroutine get_amie_electron_mono_avee

  ! --------------------------------------------------------------------
  ! Get electron wave e-flux
  ! --------------------------------------------------------------------

  subroutine get_amie_electron_wave_eflux(elecWaveEfluxOut)
    real, intent(inout) ::  elecWaveEfluxOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Electron Wave Energy Flux from AMIE"
    call get_amie_values(iEle_wave_eflux_, elecWaveEfluxOut)
  end subroutine get_amie_electron_wave_eflux

  ! --------------------------------------------------------------------
  ! Get electron wave ave-e
  ! --------------------------------------------------------------------

  subroutine get_amie_electron_wave_avee(elecWaveAveEOut)
    real, intent(inout) ::  elecWaveAveEOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Electron Wave Average Energy from AMIE"
    call get_amie_values(iEle_wave_avee_, elecWaveAveEOut)
  end subroutine get_amie_electron_wave_avee

  ! --------------------------------------------------------------------
  ! Get polar cap
  ! --------------------------------------------------------------------

  subroutine get_amie_polar_cap(polarCapOut)
    real, intent(inout) ::  polarCapOut(nMltsNeeded, nLatsNeeded)
    if (AMIE_iDebugLevel > 3) &
      write(*, *) "=> Getting Polar Cap from AMIE"
    call get_amie_values(iPolarCap_, polarCapOut)
  end subroutine get_amie_polar_cap

end Module ModAMIE_Interface

