!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModReadMhData

  ! This module contains methods for reading input MH data

  use PT_ModGrid, ONLY: iblock_to_lon_lat, get_other_state_var,   &
       nMHData, nLine, Z_, Used_B, FootPoint_VB, nVertex_B, MHData_VIB, &
       LagrID_, MinLagr, MaxLagr, MinLagrOld, MaxLagrOld, calc_density_gradient
  use PT_ModTime, ONLY: PTTime, DataInputTime
  use ModPlotFile, ONLY: read_plot_file
  use ModUtilities, ONLY: fix_dir_name, open_file, close_file, CON_stop, sleep
  use ModIoUnit, ONLY: io_unit_new

  implicit none

  SAVE

  private ! except

  ! Public members
  public:: init         ! Initialize module variables
  public:: read_param   ! Read module variables
  public:: read_mh_data ! Read MH_data from files
  public:: finalize     ! Finalize module variables DoReadMhData
  ! If the folliwing logical is true, read MH_data from files
  logical, public :: DoReadMhData
  ! If true, read all time levels from a single appended file per line
  ! (set via #APPENDMHDATA); otherwise one file per time level, tag driven
  logical, public :: UseAppendedRead = .false.
  ! the input directory
  character(len=100)         :: NameInputDir=""
  ! the name with list of file tags (used only when .not.UseAppendedRead)
  character(len=100)         :: NameTagFile=""
  ! the input file name base
  character(len=*), parameter:: NameMHData = "MH_data"
  character(len=4)           :: NameFileExtension
  character(len=20)          :: TypeMhDataFile

  ! IO unit for file with list of tags (non-appended read mode)
  integer :: iIOTag
  integer :: iLonFile, iLatFile

  ! Per-line file units for appended read mode; all lines read the same
  ! appended file independently, each unit keeping its own position
  integer, allocatable :: iUnit_B(:)

contains
  !============================================================================
   subroutine read_param(NameCommand)

      use ModReadParam, ONLY: read_var
      ! set parameters of input files with background data
      ! integer :: nFileRead
      character (len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
      select case(NameCommand)
      case('#READMHDATA')
         ! determine whether to read the MHD data
         call read_var('DoReadMhData', DoReadMhData)

         if(.not. DoReadMhData)&
            RETURN
         ! the input directory
         call read_var('NameInputDir', NameInputDir)
         call fix_dir_name(NameInputDir) ! adds "/" if not present
         call read_var('iLonFile', iLonFile)
         call read_var('iLatFile', iLatFile)
      case('#MHDATA')
         ! type of data files
         call read_var('TypeFile', TypeMhDataFile)
         ! the format of output file must be set
         select case(trim(TypeMhDataFile))
         case('tec')
            NameFileExtension='.dat'
         case('idl','ascii')
            TypeMhDataFile = 'ascii'
            NameFileExtension='.out'
         case('real4','real8')
            NameFileExtension='.out'
         case default
            call CON_stop(NameSub//': input format was not set in PARAM.in')
         end select
         ! number of input files
         ! call read_var('nFileRead', nFileRead)
         ! name of the file with the list of tags
         call read_var('NameTagFile', NameTagFile)
      case('#APPENDMHDATA')
         ! read all time levels from a single appended file per line
         call read_var('UseAppendedRead', UseAppendedRead)
      end select

   end subroutine read_param
  !============================================================================
   subroutine init
      ! initialize by setting the time and interation index of input files
      integer:: iLine
      logical:: DoesFileExist
      character(len=100):: NameFile
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
      if(.not.DoReadMhData) RETURN

      if(UseAppendedRead) then
         ! open one persistent file unit per field line; all lines read the
         ! same appended file, each unit keeping its own position
         write(NameFile,'(a,i3.3,a,i3.3,a)') &
            trim(NameInputDir)//NameMHData//'_',iLonFile,&
            '_',iLatFile, NameFileExtension
         inquire(file=NameFile, exist=DoesFileExist)
         if(.not.DoesFileExist) call CON_stop(NameSub// &
            ': appended MH_data file '//trim(NameFile)//' not found')
         allocate(iUnit_B(nLine))
         iUnit_B = -1
         do iLine = 1, nLine
            if(.not.Used_B(iLine)) CYCLE
            iUnit_B(iLine) = io_unit_new()
            call open_file(iUnitIn=iUnit_B(iLine), &
               file=NameFile, status='old', NameCaller=NameSub)
         end do
      else
         ! open the file with the list of tags
         iIOTag = io_unit_new()
         call open_file(iUnitIn=iIOTag, &
            file=trim(NameInputDir)//trim(NameTagFile), status='old')
      end if

      ! read the first input file
      call read_mh_data(DoOffsetIn = .false.)
      call get_other_state_var
      call calc_density_gradient

      PTTime = DataInputTime

   end subroutine init
  !============================================================================
  subroutine finalize

    integer :: iLine
    ! close currently opened files
    !--------------------------------------------------------------------------
    if(.not.DoReadMhData) RETURN
    if(UseAppendedRead) then
       if(allocated(iUnit_B)) then
          do iLine = 1, nLine
             if(iUnit_B(iLine) >= 0) call close_file(iUnitIn=iUnit_B(iLine))
          end do
          deallocate(iUnit_B)
       end if
    else
       call close_file(iUnitIn=iIOTag)
    end if
  end subroutine finalize
  !============================================================================
  subroutine read_mh_data(DoOffsetIn)

   use PT_ModProc, ONLY: iProc

   logical, optional, intent(in ):: DoOffsetIn

   ! read 1D MH data, which are produced by MFLAMPA in
   ! write_mh_1d n ModWrite

   ! separate file is read for each field line, name format is
   ! (usually)MH_data_<iLon>_<iLat>_t<ddhhmmss>_n<iIter>.{out/dat}

   ! name of the input file
   character(len=100):: NameFile
   ! loop variables
   integer:: iLine
   ! indexes of corresponding node, latitude and longitude
   integer:: iLat, iLon
   ! size of the offset to apply compared to the previous state
   integer:: iOffset
   integer :: startIndex = 1
   ! local value of DoOffset
   logical:: DoOffset
   ! whether the input file exists; scalar INQUIRE target, works around a
   ! nagfor 7.1 -O2 -C -gline miscompilation of exist=Used_B(iLine)
   logical:: DoesFileExist
   ! auxilary variables to apply positive offset for appended particles
   ! auxilary parameter index
   integer, parameter:: RShock_ = Z_ + 2
   integer, parameter:: StartTime_  = RShock_ + 1
   integer, parameter:: StartJulian_= StartTime_ + 1
   ! additional parameters of lines
   real:: Param_I(LagrID_:StartJulian_)
   ! data input time before reading new data file
   real:: DataInputTimeOld
   ! timetag
   character(len=50):: StringTag
   ! status of reading .lst file
   integer :: ioStat, SleepCounter = 0
   integer :: TimeToWait, TimeToQuit
   integer :: ioError

   ! check whether need to apply offset, default is .true.

    character(len=*), parameter:: NameSub = 'read_mh_data'
    !--------------------------------------------------------------------------
   if(present(DoOffsetIn))then
      DoOffset = DoOffsetIn
   else
      DoOffset = .true.
   end if

   if(.not.UseAppendedRead) then
      ! get the tag for files

      ! For Feb 2026 Artemis real-time demonstration
      ! Continually check that .lst file is updated and wait until it is
      ! Terminate if .lst is not updated after 2 minutes
      TimeToWait = 1   ! seconds
      TimeToQuit = 600 ! seconds
      do
         read(iIOtag, '(a)', iostat = ioStat) StringTag
         ! sometimes the file returns empty string and the next read is
         ! successful; if the string is empty, the fieldline will be marked
         ! as unused
         if(StringTag == '') read(iIOtag, '(a)', iostat = ioStat) StringTag
         if(ioStat < 0) then
            backspace(iIOTag)
            SleepCounter = SleepCounter + TimeToWait
            call sleep(real(TimeToWait))
            if(SleepCounter > TimeToQuit.and.iProc == 0) &
               call CON_Stop(NameSub// &
               ': .lst file not updated for 120 seconds.')
            CYCLE
         else if(ioStat > 0.and.iProc == 0) then
            call CON_Stop(NameSub//': error reading .lst file.')
         else
            ! Reset sleep counter for next file read
            SleepCounter = 0
            EXIT
         end if

      end do
   end if
   ! ---------------------------------------------

   ! save the current data input time
   DataInputTimeOld = DataInputTime
   ! read the data
   line:do iLine = 1, nLine

      if(.not.Used_B(iLine))then
         nVertex_B(iLine) = 0
         MinLagr(iLine) = 1
         MaxLagr(iLine) = 1
         CYCLE line
      end if

      call iblock_to_lon_lat(iLine, iLon, iLat)
      if(UseAppendedRead) then
         ! set the file name (needed by read_plot_file even with iUnitIn)
         write(NameFile,'(a,i3.3,a,i3.3,a)') &
            trim(NameInputDir)//NameMHData//'_',iLonFile,&
            '_',iLatFile, NameFileExtension
         ! read the header of the next appended block first
         call read_plot_file(NameFile        ,&
            iUnitIn    = iUnit_B(iLine)      ,&
            TypeFileIn = TypeMhDataFile      ,&
            TimeOut    = DataInputTime       ,&
            n1out      = nVertex_B(iLine)    ,&
            ParamOut_I = Param_I(LagrID_:StartJulian_),&
            iErrorOut  = ioError)
         if(ioError /= 0) call CON_stop(NameSub// &
            ': end of appended file reached; no more MH data')
      else
         ! set the file name
         write(NameFile,'(a,i3.3,a,i3.3,a)') &
            trim(NameInputDir)//NameMHData//'_',iLonFile,&
            '_',iLatFile, '_'//trim(StringTag)//NameFileExtension

         inquire(file=NameFile,exist=DoesFileExist)
         Used_B(iLine) = DoesFileExist

         if(.not.Used_B(iLine))then
            write(*,'(a)')NameSub//': the file '//NameFile//' is not found!'
            write(*,'(a)')NameSub//': the line marked as unused'
            nVertex_B(iLine) = 0
            MinLagr(iLine) = 1
            MaxLagr(iLine) = 1
            CYCLE line
         end if
         ! read the header first

         call read_plot_file(NameFile        ,&
            TypeFileIn = TypeMhDataFile      ,&
            TimeOut    = DataInputTime       ,&
            n1out      = nVertex_B(iLine)    ,&
            ParamOut_I = Param_I(LagrID_:StartJulian_))
      end if

      ! For Feb 2026 Artemis real-time demonstration
      ! Sometimes M-FLAMPA files take a long time to write (hardware issue?)
      ! Try for 10 minutes before aborting MITTENS

      ! TimeToWait = 5   ! seconds
      ! TimeToQuit = 600 ! seconds
      ! do
      !    call read_plot_file(NameFile          ,&
      !       TypeFileIn = TypeMhDataFile      ,&
      !       TimeOut    = DataInputTime       ,&
      !       n1out      = nVertex_B(iLine)    ,&
      !       ParamOut_I = Param_I(LagrID_:StartJulian_), &
      !       iErrorOut = ioError)

      !    if(ioError /= 0) then
      !       SleepCounter = SleepCounter + TimeToWait
      !       call sleep(TimeToWait)
      !       if(SleepCounter > TimeToQuit.and.iProc == 0) &
      !          call CON_Stop(NameSub//': read_plot_file could not read file for 600 seconds.')
      !       cycle
      !    else
      !       SleepCounter = 0
      !       exit
      !    end if

      ! end do

      ! find offset in data between new and old states
      if(DoOffset)then
         ! check consistency: time counter MUST advance
         if(DataInputTimeOld >= DataInputTime)then
            if(UseAppendedRead) then
               ! in appended mode time not advancing means end of data
               call CON_stop(NameSub//&
                  ': time counter didnt advance in appended file; '//&
                  'no more MH data')
            else
               call CON_stop(NameSub//&
                  ': time counter didnt advance when reading mh data file '//&
                  'with tag '//trim(StringTag)//&
                  '; the tag may be repeated in '//trim(NameTagFile))
            end if
         end if
         ! amount of the offset is determined from difference
         ! in LagrID_
         iOffset = nint(FootPoint_VB(LagrID_,iLine) - Param_I(LagrID_))
      else
         iOffset = 0
      end if
      ! Parameters
      FootPoint_VB(LagrID_:Z_,iLine) = Param_I(LagrID_:Z_)
      ! read MH data
      if(UseAppendedRead) then
         call read_plot_file(NameFile        ,&
            iUnitIn    = iUnit_B(iLine)      ,&
            TypeFileIn = TypeMhDataFile      ,&
            Coord1Out_I= MHData_VIB(LagrID_  ,&
            1:nVertex_B(iLine),iLine)        ,&
            VarOut_VI  = MHData_VIB(1:nMHData,&
            1:nVertex_B(iLine),iLine))
      else
         call read_plot_file(NameFile        ,&
            TypeFileIn = TypeMhDataFile      ,&
            Coord1Out_I= MHData_VIB(LagrID_  ,&
            1:nVertex_B(iLine),iLine)        ,&
            VarOut_VI  = MHData_VIB(1:nMHData,&
            1:nVertex_B(iLine),iLine))
      end if

      ! Shift data such that index == lagr coordinate

      ! sometimes negative lagr coords appear
      ! ignoring those for now - consult igor

      startIndex = 1
      do while(MhData_VIB(LagrID_, startIndex, iLine) < 1)
         startIndex = startIndex + 1
      end do

      MinLagr(iLine) = MhData_VIB(LagrID_, startIndex, iLine)
      MaxLagr(iLine) = MhData_VIB(LagrID_, nVertex_B(iLine), iLine)
      MHData_VIB(:, MinLagr(iLine):MaxLagr(iLine), iLine) = &
         MHData_VIB(:, startIndex:nVertex_B(iLine), iLine)

      ! zero out lagr coord with no data
      if(MinLagr(iLine) > 1) &
         MHData_VIB(:, 1:MinLagr(iLine)-1, iLine) = 0.0

    end do line

  end subroutine read_mh_data
  !============================================================================
end module PT_ModReadMhData
!==============================================================================
