!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModReadMhData

  ! This module contains methods for reading input MH data

  use SP_ModGrid, ONLY: iblock_to_lon_lat, get_other_state_var, nMhData, &
       nLine, Z_, Used_B, FootPoint_VB, nVertex_B, MhData_VIB, LagrID_, T_, &
       UseAppendedRead
  use SP_ModTime, ONLY: SPTime, DataInputTime
  use SP_ModDistribution, ONLY: offset
  use ModPlotFile, ONLY: read_plot_file
  use ModUtilities, ONLY: fix_dir_name, open_file, close_file, CON_stop
  use ModIoUnit, ONLY: io_unit_new

  implicit none

  SAVE

  private ! except

  ! Public members
  public:: init         ! Initialize module variables
  public:: read_param   ! Read module variables
  public:: read_mh_data ! Read MH_data from files
  public:: finalize     ! Finalize module variables DoReadMhData
  ! If the following logical is true, read MH_data from files
  logical, public :: DoReadMhData = .false.
  ! the input directory
  character(len=100) :: NameInputDir=""
  ! the name with list of file tags (used only when .not.UseAppendedRead)
  character(len=100) :: NameTagFile=""
  ! the input file name base
  character(len=4)   :: NameFileExtension
  character(len=20)  :: TypeMhDataFile

  ! IO unit for file with list of tags (non-appended read mode)
  integer:: iIOTag

  ! UseAppendedRead is in ModGrid (set via #APPENDMHDATA in ModPlot)

  ! Per-line file units for appended read mode (one open file per field line)
  integer, allocatable :: iUnit_B(:)

contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    ! set parameters of input files with background data
    integer :: nFileRead
    character (len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#READMHDATA')
       ! determine whether to read the MHD data
       call read_var('DoReadMhData', DoReadMhData)
       if(.not. DoReadMhData) RETURN
       ! the input directory
       call read_var('NameInputDir', NameInputDir)
       call fix_dir_name(NameInputDir) ! adds "/" if not present
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
       ! number of input files (used only in non-appended read mode)
       call read_var('nFileRead', nFileRead)
       ! name of the file with the list of tags (non-appended read mode)
       call read_var('NameTagFile', NameTagFile)
    end select

  end subroutine read_param
  !============================================================================
  subroutine init

    use SP_ModPlot, ONLY: nTag, NameMHData
    ! initialize by setting the time and iteration index of input files
    integer:: iTag, iLine, iLat, iLon
    character(len=50):: StringAux
    character(len=100):: NameFile
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    if(.not.DoReadMhData) RETURN

    if(UseAppendedRead) then
       ! Open one persistent file unit per field line
       allocate(iUnit_B(nLine))
       iUnit_B = -1
       do iLine = 1, nLine
          if(.not.Used_B(iLine)) CYCLE
          call iblock_to_lon_lat(iLine, iLon, iLat)
          write(NameFile,'(a,i3.3,a,i3.3,a)') &
               trim(NameInputDir)//NameMHData//'_', iLon, &
               '_', iLat, NameFileExtension
          inquire(file=NameFile, exist=Used_B(iLine))
          if(.not.Used_B(iLine)) then
             write(*,'(a)') NameSub//': file '//trim(NameFile)//' not found'
             write(*,'(a)') NameSub//': line marked as unused'
             nVertex_B(iLine) = 0
             CYCLE
          end if
          iUnit_B(iLine) = io_unit_new()
          call open_file(iUnitIn=iUnit_B(iLine), &
               file=NameFile, status='old', NameCaller=NameSub)
       end do

       ! If no files were found, stop
       if(.not.any(Used_B(1:nLine))) &
            call CON_stop(NameSub// &
            ': no appended MH_data files found in '// &
            trim(NameInputDir))
       ! Skip nTag-1 blocks already consumed (restart case);
       ! the nTag-th block is the last written state and will be read below
       ! as the "first input file" to set initial conditions.
       if(nTag > 1) then
          do iTag = 1, nTag-1
             call read_mh_data(DoOffsetIn=.false.)
          end do
       end if
    else
       ! Open the tag list file and skip already-consumed tags
       iIOTag = io_unit_new()
       call open_file(iUnitIn=iIOTag, &
            file=trim(NameInputDir)//trim(NameTagFile), status='old')
       if(nTag > 0) then
          do iTag = 1, nTag-1
             read(iIOTag,'(a)') StringAux
          end do
       end if
    end if

    ! Read the first time step
    call read_mh_data(DoOffsetIn = .false.)
    call get_other_state_var
    SPTime = DataInputTime

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

    use ModConst, ONLY: cKeV
    use SP_ModPlot, ONLY: NameMHData
    use SP_ModUnit, ONLY: Si2Io_V, UnitEnergy_

    logical, optional, intent(in):: DoOffsetIn
    ! read 1D MH data, which are produced by write_mh_1d in SP_ModPlot
    ! In non-appended mode: separate file per field line per time step,
    !   name format: MH_data_<iLon>_<iLat>_t<ddhhmmss>_n<iIter>.{out/dat}
    ! In appended mode: one file per field line, all steps appended,
    !   name format: MH_data_<iLon>_<iLat>.{out/dat}
    ! name of the input file (non-appended mode only)
    character(len=100):: NameFile
    ! loop variables
    integer:: iLine
    ! indexes of corresponding node, latitude and longitude
    integer:: iLat, iLon
    ! size of the offset to apply compared to the previous state
    integer:: iOffset
    ! local value of DoOffset
    logical:: DoOffset
    ! auxilary parameter index
    integer, parameter:: RShock_ = Z_ + 2
    integer, parameter:: StartTime_  = RShock_ + 1
    integer, parameter:: StartJulian_= StartTime_ + 1
    ! additional parameters of lines
    real:: Param_I(LagrID_:StartJulian_)
    ! data input time before reading new data file
    real:: DataInputTimeOld
    ! timetag (non-appended mode)
    character(len=50):: StringTag
    ! IO error status
    integer:: iError
    character(len=*), parameter:: NameSub = 'read_mh_data'
    !--------------------------------------------------------------------------
    if(present(DoOffsetIn))then
       DoOffset = DoOffsetIn
    else
       DoOffset = .true.
    end if

    if(.not.UseAppendedRead) then
       ! get the tag for files
       read(iIOTag,'(a)', iostat=iError) StringTag
       if(iError /= 0) call CON_stop( &
            NameSub//': end of tag file reached; no more MH data')
    end if

    ! save the current data input time
    DataInputTimeOld = DataInputTime

    ! read the data
    line:do iLine = 1, nLine
       if(.not.Used_B(iLine))then
          nVertex_B(iLine) = 0
          CYCLE line
       end if

       call iblock_to_lon_lat(iLine, iLon, iLat)
       if(UseAppendedRead) then
          ! Set the file name (needed by read_plot_file even with iUnitIn)
          write(NameFile,'(a,i3.3,a,i3.3,a)') &
               trim(NameInputDir)//NameMHData//'_',iLon,&
               '_',iLat, NameFileExtension
          ! Read next block from the already-open per-line file
          call read_plot_file(NameFile          ,&
               iUnitIn    = iUnit_B(iLine)      ,&
               TypeFileIn = TypeMhDataFile      ,&
               TimeOut    = DataInputTime       ,&
               n1out      = nVertex_B(iLine)    ,&
               ParamOut_I = Param_I(LagrID_:StartJulian_),&
               iErrorOut  = iError)
          if(iError /= 0) call CON_stop(NameSub// &
               ': end of appended file reached; no more MH data')
       else
          ! set the file name
          write(NameFile,'(a,i3.3,a,i3.3,a)') &
               trim(NameInputDir)//NameMHData//'_',iLon,&
               '_',iLat, '_'//trim(StringTag)//NameFileExtension
          inquire(file=NameFile,exist=Used_B(iLine))
          if(.not.Used_B(iLine))then
             write(*,'(a)')NameSub//': the file '//NameFile//' is not found!'
             write(*,'(a)')NameSub//': the line marked as unused'
             nVertex_B(iLine) = 0
             CYCLE line
          end if
          ! read the header
          call read_plot_file(NameFile          ,&
               TypeFileIn = TypeMhDataFile      ,&
               TimeOut    = DataInputTime       ,& 
               n1out      = nVertex_B(iLine)    ,&
               ParamOut_I = Param_I(LagrID_:StartJulian_))
       end if

       ! find offset in data between new and old states
       if(DoOffset)then
          ! check consistency: time counter MUST advance
          if(DataInputTimeOld >= DataInputTime) then
             if(UseAppendedRead) then
                ! In appended mode, time not advancing means end of data
                write(*,*) NameSub// &
                     ': end of data in appended file (time did not advance)'
                call CON_stop(NameSub// &
                     ': no more MH data in appended files')
             else
                call CON_stop(NameSub//': time counter did not advance')
             end if
          end if
          ! amount of the offset is determined from difference in LagrID_
          iOffset = nint(FootPoint_VB(LagrID_,iLine) - Param_I(LagrID_))
       else
          iOffset = 0
       end if

       ! Parameters
       FootPoint_VB(LagrID_:Z_,iLine) = Param_I(LagrID_:Z_)

       ! read MH data
       if(UseAppendedRead) then
          call read_plot_file(NameFile    ,  &
               iUnitIn    = iUnit_B(iLine),  &
               TypeFileIn = TypeMhDataFile,  &
               Coord1Out_I= MhData_VIB(LagrID_, 1:nVertex_B(iLine), iLine),&
               VarOut_VI  = MhData_VIB(1:nMhData, 1:nVertex_B(iLine), iLine))
       else
          call read_plot_file(NameFile    ,  &
               TypeFileIn = TypeMhDataFile,  &
               Coord1Out_I= MhData_VIB(LagrID_, 1:nVertex_B(iLine), iLine),&
               VarOut_VI  = MhData_VIB(1:nMhData, 1:nVertex_B(iLine), iLine))
       end if

       MhData_VIB(T_, 1:nVertex_B(iLine), iLine) =    &
            MhData_VIB(T_, 1:nVertex_B(iLine), iLine) &  ! default: in KeV
            * cKeV                                    &  ! KeV to Si unit
            * Si2Io_V(UnitEnergy_)                       ! Si to Io unit
       ! apply offset
       call offset(iLine, iOffset)
    end do line

  end subroutine read_mh_data
  !============================================================================
end module SP_ModReadMhData
!==============================================================================
