!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModConnectivity

  ! Trace magnetic connectivity through the active M-FLAMPA field-line mesh.
  ! For each requested target position, the module performs the following steps:
  !   1. Construct a sequence of spherical shells from the target radius inward.
  !   2. Intersect every active M-FLAMPA field line with each shell.
  !   3. Gather the shell intersections to MPI rank 0.
  !   4. Build a spherical Delaunay triangulation of the intersections.
  !   5. Locate the target-connected point in one triangle and propagate its
  !      barycentric stencil and weights to the next inner shell.
  !   6. Write the resulting radial trace, field-line indices, and interpolation
  !      weights to a connectivity file.

  use ModIoUnit, ONLY: UnitTmp_
  use ModNumConst, ONLY: cRadToDeg
  use ModUtilities, ONLY: open_file, close_file, CON_stop
  use ModMpi
  use ModCoordTransform, ONLY: xyz_to_rlonlat
  use ModTimeConvert, ONLY: time_real_to_int

  use SP_ModGrid, ONLY: nLine, nLineAll, iLineAll0, Used_B, &
          MHData_VIB, X_, Y_, Z_, search_line, TypeCoordSystem
  use SP_ModProc, ONLY: iProc, nProc, iComm, iError
  use SP_ModTime, ONLY: SPTime, iIter, StartTime, IsSteadyState

  use ModTriangulateSpherical, ONLY: trmesh, find_triangle_orig, find_triangle_sph

  implicit none

  private

  public :: read_param
  public :: write_connectivity_satellite_files
  public :: save_connectivity_targets
  public :: trace_connectivity_target
  public :: trace_connectivity_target_rlonlat
  public :: DoSaveConnectivity
  public :: DoDebugConnectivity

  logical :: DoSaveConnectivity     = .false.           ! master switch for this module
  logical :: DoDebugConnectivity    = .false.           ! detailed progress and wall-time diagnostics
  logical :: DoSaveConnectivityOnce = .true.            ! limit a steady-state trajectory sweep to one call
  logical :: DoSaveConnectivityDone = .false.           ! records whether that one sweep has finished
  real    :: DtOutput              = 3600.0             ! sample/output cadence in simulation seconds
  real    :: StartTimeTraj        = -14.0*86400.0       ! steady-state trajectory window, in seconds
  real    :: EndTimeTraj          =  14.0*86400.0
  integer :: iTimeOutputConnectivity = -huge(1)         ! last completed time-accurate DtOutput interval
  integer :: nTraceConnectivity   = 300                 ! number of requested radial shells
  real    :: PowerConnectivity      = 3.0               ! power-law exponent for radial shell spacing
  real    :: RadiusMinConnectivity  = 1.15              ! innermost requested radius, normally in Rs

  real, parameter :: cTiny = 1.0e-10
  real, parameter :: cRsToAu = 0.0046524726             ! conversion from solar radii to astronomical units
  real(kind=8), parameter :: SlowConnectivityTime = 30.0_8      ! elapsed time that triggers a slow-stage message
  integer, parameter :: nConnectivityProgress = 10      ! approximate number of progress reports per trace

contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var

    character(len=*), intent(in) :: NameCommand

    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#SAVECONNECTIVITY')
       call read_var('DoSaveConnectivity', DoSaveConnectivity)
       if(.not.DoSaveConnectivity) RETURN

       ! Output cadence.
       ! Reset optional values to defaults every time this command is read.
       ! DtOutput is used both as the steady-state trajectory spacing and as the
       ! interval between time-accurate connectivity outputs.
       DtOutput = 3600.0
       if(is_next_param_line_named('DtOutput')) then
          call read_time_parameter('DtOutput', DtOutput)
       end if

       ! Read radial resolution, radial refinement, and lower boundary.
       call read_var('nTraceConnectivity', nTraceConnectivity)
       call read_var('PowerConnectivity', PowerConnectivity)
       call read_var('RadiusMinConnectivity', RadiusMinConnectivity)

       ! Steady-state trajectory sampling window. The values are
       ! relative to StartTime and are interpreted as durations.
       ! If omitted, default to -14 days and +14 days.
       StartTimeTraj = -14.0*86400.0
       EndTimeTraj   =  14.0*86400.0
       if(is_next_param_line_named('StartTimeTraj')) &
            call read_time_parameter('StartTimeTraj', StartTimeTraj)
       if(is_next_param_line_named('EndTimeTraj')) &
            call read_time_parameter('EndTimeTraj', EndTimeTraj)

       ! This option controls only the steady-state trajectory sweep.
       DoSaveConnectivityOnce = .true.
       if(is_next_param_line_named('DoSaveConnectivityOnce')) &
            call read_var('DoSaveConnectivityOnce', DoSaveConnectivityOnce)

       ! Optional debug line. Read the exact label found by the look-ahead.
       ! Accepted PARAM.in lines:
       !   T  DebugConnectivity
       !   T  DoDebugConnectivity
       DoDebugConnectivity = .false.
       if(is_next_param_line_named('DebugConnectivity')) then
          call read_var('DebugConnectivity', DoDebugConnectivity)
       else if(is_next_param_line_named('DoDebugConnectivity')) then
          call read_var('DoDebugConnectivity', DoDebugConnectivity)
       end if

       if(DtOutput <= 0.0) &
            call CON_stop(NameSub//': DtOutput must be positive')
       if(EndTimeTraj < StartTimeTraj) &
            call CON_stop(NameSub//': EndTimeTraj must be >= StartTimeTraj')
       if(nTraceConnectivity < 2) &
            call CON_stop(NameSub//': nTraceConnectivity must be at least 2')
       if(PowerConnectivity <= 0.0) &
            call CON_stop(NameSub//': PowerConnectivity must be positive')
       if(RadiusMinConnectivity <= 0.0) &
            call CON_stop(NameSub//': RadiusMinConnectivity must be positive')

       ! Re-arm both scheduling mechanisms after new parameters are read.
       DoSaveConnectivityDone = .false.
       iTimeOutputConnectivity = -huge(1)
    case default
       call CON_stop(NameSub//': unknown command '//trim(NameCommand))
    end select

  end subroutine read_param
  !============================================================================
  subroutine read_time_parameter(NameVar, TimeValue)

    ! Read a PARAM.in line whose value is a duration with an optional unit.
    ! Accepted examples:
    !   1800     DtOutput      ! seconds by default
    !   30 m     DtOutput
    !   1 h      DtOutput
    !   -14 d    StartTimeTraj
    !   14 d     EndTimeTraj

    use ModReadParam, ONLY: read_var

    character(len=*), intent(in)  :: NameVar
    real,             intent(out) :: TimeValue

    ! StringValue retains the complete value field so a unit may follow the
    ! number, for example "30 m", "1 h", or "-14 d".
    character(len=80) :: StringValue

    !--------------------------------------------------------------------------
    call read_var(NameVar, StringValue)
    call parse_time_duration(StringValue, NameVar, TimeValue)

  end subroutine read_time_parameter
  !============================================================================
  subroutine parse_time_duration(StringIn, NameVar, TimeValue)

    use ModUtilities, ONLY: lower_case

    character(len=*), intent(in)  :: StringIn, NameVar
    real,             intent(out) :: TimeValue

    ! String is a working copy with optional quotes removed.
    ! Unit defaults to seconds when no unit token is supplied.
    ! Value is the numeric part and Factor converts the selected unit to seconds.
    ! iStat is the internal-read status and nLen is the trimmed string length.
    character(len=80) :: String, Unit
    real :: Value, Factor
    integer :: iStat, nLen

    !--------------------------------------------------------------------------
    String = adjustl(StringIn)
    nLen = len_trim(String)
    if(nLen >= 2) then
       if((String(1:1) == '"' .and. String(nLen:nLen) == '"') .or. &
            (String(1:1) == "'" .and. String(nLen:nLen) == "'")) then
          String = adjustl(String(2:nLen-1))
       end if
    end if

    Unit = 's'
    read(String,*,iostat=iStat) Value, Unit
    if(iStat /= 0) then
       Unit = 's'
       read(String,*,iostat=iStat) Value
    end if
    if(iStat /= 0) call CON_stop('SP_ModConnectivity::parse_time_duration: '// &
         'could not parse '//trim(NameVar)//' = '//trim(StringIn))

    call lower_case(Unit)

    select case(trim(Unit))
    case('s','sec','secs','second','seconds')
       Factor = 1.0
    case('m','min','mins','minute','minutes')
       Factor = 60.0
    case('h','hr','hrs','hour','hours')
       Factor = 3600.0
    case('d','day','days')
       Factor = 86400.0
    case default
       call CON_stop('SP_ModConnectivity::parse_time_duration: unknown unit '// &
            trim(Unit)//' for '//trim(NameVar))
    end select

    TimeValue = Value*Factor

  end subroutine parse_time_duration
  !============================================================================
  logical function is_next_param_line_named(NameVar)

    ! Non-consuming look-ahead used for optional parameters at the end of a
    ! fixed-size PARAM.in block.  A line is considered a match only if the next
    ! unread line is not a command and contains the requested variable label.

    use ModReadParam, ONLY: read_text, i_line_read, n_line_read, lStringLine
    use ModUtilities, ONLY: lower_case

    character(len=*), intent(in) :: NameVar

    ! read_text exposes unread PARAM.in text without advancing the parser.
    ! StringNext is the next unread line; lower-case copies are used for a
    ! case-insensitive label search. iLineNow/nLineLast delimit the unread block.
    character(len=lStringLine), allocatable :: StringLine_I(:)
    character(len=lStringLine) :: StringNext, StringLower, NameLower
    integer :: iLine, iLineNow, nLineLast, nLineText

    !--------------------------------------------------------------------------
    is_next_param_line_named = .false.

    iLineNow  = i_line_read()
    nLineLast = n_line_read()
    if(iLineNow >= nLineLast) RETURN

    allocate(StringLine_I(iLineNow+1:nLineLast))
    call read_text(StringLine_I, nLineText)

    ! Match the next actual parameter line, rather than only the next physical
    ! line. This permits blank lines between optional settings.
    NameLower = NameVar
    call lower_case(NameLower)

    do iLine = iLineNow + 1, nLineLast
       StringNext = adjustl(StringLine_I(iLine))
       if(len_trim(StringNext) == 0) CYCLE
       if(StringNext(1:1) == '#') EXIT

       StringLower = StringNext
       call lower_case(StringLower)
       is_next_param_line_named = index(StringLower, trim(NameLower)) > 0
       EXIT
    end do

    deallocate(StringLine_I)

  end function is_next_param_line_named
  !============================================================================
  subroutine write_connectivity_satellite_files(IsInitialOutput)

    use SP_ModSatellite, ONLY: nSat, NameSat_I, get_satellite_time_range, &
         get_satellite_position_at_time

    ! The caller supplies whether this is the initial output call, but this
    ! subroutine currently does not use the flag. Passing .true. or .false.
    ! therefore produces the same behavior.
    logical, intent(in) :: IsInitialOutput

    integer :: iSat			! current satellite/trajectory
    integer :: iSample			! current steady-state trajectory sample
    integer :: nSampleMax		! maximum number of samples in the requested window
    integer :: iTimeOutputNew		! integer DtOutput interval containing SPTime
    real :: TimeSatStart, TimeSatEnd	! ephemeris coverage reported by SP_ModSatellite
    real :: TimeStart, TimeEnd		! requested steady-state sampling window
    real :: TimeSample			! time used to evaluate the satellite position
    real :: XyzSat_D(3)			! satellite Cartesian position at TimeSample
    logical :: DoTrack			! true when the ephemeris query returned a valid position
    logical :: IsTimeAccurate		! should be straightforward
    character(len=100) :: NameTarget	! target identifier passed to the file writer

    character(len=*), parameter:: NameSub = 'write_connectivity_satellite_files'
    !--------------------------------------------------------------------------
    if(.not.DoSaveConnectivity) RETURN
    if(nSat <= 0) then
       if(iProc == 0) write (*,*) &
          'SP:CONNECTIVITY skipped: no #SATELLITE trajectory defined'
       RETURN
    end if

    if(DtOutput <= 0.0) &
         call CON_stop(NameSub//': DtOutput must be positive')

    IsTimeAccurate = .not.IsSteadyState

    ! In time-accurate runs the field state advances. Therefore we do not sweep
    ! StartTimeTraj:EndTimeTraj. Instead, we use the current simulation time and
    ! let DtOutput thin the output relative to the coupling/output calls.
    !
    ! In steady-state runs the field state is fixed. Therefore we sweep the
    ! trajectory window once, unless DoSaveConnectivityOnce is disabled.
    ! However, do not disable DoSaveConnectivityOnce in steady-state, otherwise
    ! chaos will ensue.

    if(IsTimeAccurate) then
       ! Divide simulation time into integer DtOutput intervals. A trace is
       ! produced only after entering an interval newer than the last saved one,
       ! allowing this routine to be called more frequently than DtOutput.
       iTimeOutputNew = int(SPTime/DtOutput)
       if(iTimeOutputNew <= iTimeOutputConnectivity) then
          if(iProc == 0 .and. DoDebugConnectivity) write(*,*) &
               'SP:CONNECTIVITY time-accurate skipped by DtOutput: ', &
               ' SPTime=', SPTime, ' DtOutput=', DtOutput, &
               ' iTimeOutput=', iTimeOutputNew, &
               ' last=', iTimeOutputConnectivity
          RETURN
       end if
       iTimeOutputConnectivity = iTimeOutputNew

       if(iProc == 0 .and. DoDebugConnectivity) then
          write(*,*) 'SP:CONNECTIVITY start time-accurate: nSat=', nSat, &
               ' DtOutput=', DtOutput, &
               ' SPTime=', SPTime, &
               ' nTraceConnectivity=', nTraceConnectivity, &
               ' RadiusMinConnectivity=', RadiusMinConnectivity, &
               ' DoDebugConnectivity=', DoDebugConnectivity
       end if

       do iSat = 1, nSat
          call get_satellite_time_range(iSat, TimeSatStart, TimeSatEnd, DoTrack)
          if(.not.DoTrack) then
             if(iProc == 0) write(*,*) &
                  'SP:CONNECTIVITY satellite skipped: ', trim(NameSat_I(iSat)), &
                  ' no valid ephemeris time range'
             CYCLE
          end if

          TimeSample = SPTime
          call get_satellite_position_at_time(iSat, TimeSample, XyzSat_D, DoTrack)
          if(.not.DoTrack) then
             if(iProc == 0) write(*,*) &
                  'SP:CONNECTIVITY satellite skipped: ', trim(NameSat_I(iSat)), &
                  ' no ephemeris position at SPTime=', TimeSample, &
                  ' EphemTimeStart=', TimeSatStart, &
                  ' EphemTimeEnd=', TimeSatEnd
             CYCLE
          end if

          NameTarget = trim(NameSat_I(iSat))

          if(iProc == 0) then
             if(DoDebugConnectivity) then
                write(*,*) 'SP:CONNECTIVITY time-accurate sample start: target=', &
                     trim(NameTarget), ' target_time=', TimeSample, &
                     ' field_time=', SPTime, &
                     ' iTimeOutput=', iTimeOutputConnectivity
             else
                write(*,*) 'SP:CONNECTIVITY time-accurate sample: target=', &
                     trim(NameTarget), ' target_time=', TimeSample
             end if
          end if

          call trace_connectivity_target(trim(NameTarget), XyzSat_D, &
               RadiusMinIn = RadiusMinConnectivity, &
               nTraceIn    = nTraceConnectivity, &
               PowerIn     = PowerConnectivity, &
               NameDirIn   = 'SP/IO2/', &
               TargetTimeIn= TimeSample)
       end do

       if(iProc == 0 .and. DoDebugConnectivity) &
            write(*,*) 'SP:CONNECTIVITY done time-accurate'

       RETURN
    end if

    if(DoSaveConnectivityOnce .and. DoSaveConnectivityDone) RETURN

    if(iProc == 0 .and. DoDebugConnectivity) then
       write(*,*) 'SP:CONNECTIVITY start steady-state: nSat=', nSat, &
            ' DtOutput=', DtOutput, &
            ' StartTimeTraj=', StartTimeTraj, &
            ' EndTimeTraj=', EndTimeTraj, &
            ' nTraceConnectivity=', nTraceConnectivity, &
            ' RadiusMinConnectivity=', RadiusMinConnectivity, &
            ' DoDebugConnectivity=', DoDebugConnectivity
    end if

    do iSat = 1, nSat
       call get_satellite_time_range(iSat, TimeSatStart, TimeSatEnd, DoTrack)
       if(.not.DoTrack) then
          if(iProc == 0) write(*,*) &
               'SP:CONNECTIVITY satellite skipped: ', trim(NameSat_I(iSat)), &
               ' no valid ephemeris time range'
          CYCLE
       end if

       ! The ephemeris query decides whether each requested sample actually exists.
       TimeStart = StartTimeTraj
       TimeEnd   = EndTimeTraj
       if(TimeEnd < TimeStart) CYCLE

       nSampleMax = max(1, int((TimeEnd - TimeStart)/DtOutput) + 1)

       if(iProc == 0 .and. DoDebugConnectivity) then
          write(*,*) 'SP:CONNECTIVITY satellite: ', trim(NameSat_I(iSat)), &
               ' EphemTimeStart=', TimeSatStart, ' EphemTimeEnd=', TimeSatEnd, &
               ' StartTimeTraj=', TimeStart, ' EndTimeTraj=', TimeEnd, &
               ' nSample=', nSampleMax
       end if

       do iSample = 1, nSampleMax
          TimeSample = TimeStart + real(iSample - 1)*DtOutput
          if(TimeSample > TimeEnd + 0.5*DtOutput) EXIT

          call get_satellite_position_at_time(iSat, TimeSample, XyzSat_D, DoTrack)
          if(.not.DoTrack) then
             if(iProc == 0 .and. DoDebugConnectivity) write(*,*) &
                  'SP:CONNECTIVITY sample skipped: satellite=', &
                  trim(NameSat_I(iSat)), ' sample=', iSample, '/', nSampleMax, &
                  ' target_time=', TimeSample
             CYCLE
          end if

          write(NameTarget,'(a,"_s",i6.6)') trim(NameSat_I(iSat)), iSample

          if(iProc == 0) then
             if(DoDebugConnectivity) then
                write(*,*) 'SP:CONNECTIVITY sample start: target=', &
                     trim(NameTarget), ' sample=', iSample, '/', nSampleMax, &
                     ' target_time=', TimeSample
             else
                write(*,*) 'SP:CONNECTIVITY sample: target=', trim(NameTarget), &
                     ' sample=', iSample, '/', nSampleMax
             end if
          end if

          call trace_connectivity_target(trim(NameTarget), XyzSat_D, &
               RadiusMinIn = RadiusMinConnectivity, &
               nTraceIn    = nTraceConnectivity, &
               PowerIn     = PowerConnectivity, &
               NameDirIn   = 'SP/IO2/', &
               TargetTimeIn= TimeSample)
       end do
    end do

    DoSaveConnectivityDone = .true.

    if(iProc == 0 .and. DoDebugConnectivity) write(*,*) &
         'SP:CONNECTIVITY done steady-state'

  end subroutine write_connectivity_satellite_files
  !============================================================================
  subroutine save_connectivity_targets(nTarget, NameTarget_I, XyzTarget_DI, &
       RadiusMinIn, nTraceIn, PowerIn, NameDirIn)

    ! Save one connectivity trace for each target.  XyzTarget_DI must be in the
    ! same Cartesian coordinate system and distance unit as MHData_VIB(X_:Z_,:,:).
    ! In the current MFLAMPA output this distance unit is normally Rs.

    integer,          intent(in) :: nTarget			! number of independent positions
    character(len=*), intent(in) :: NameTarget_I(nTarget)	! output identifiers
    real,             intent(in) :: XyzTarget_DI(3,nTarget)	! one Cartesian vector per target
    real,    optional,intent(in) :: RadiusMinIn
    integer, optional,intent(in) :: nTraceIn
    real,    optional,intent(in) :: PowerIn
    character(len=*), optional,intent(in) :: NameDirIn

    integer :: iTarget

    character(len=*), parameter:: NameSub = 'save_connectivity_targets'
    !--------------------------------------------------------------------------
    if(nTarget < 1) RETURN

    do iTarget = 1, nTarget
       call trace_connectivity_target(trim(NameTarget_I(iTarget)), &
            XyzTarget_DI(:,iTarget), RadiusMinIn=RadiusMinIn, &
            nTraceIn=nTraceIn, PowerIn=PowerIn, NameDirIn=NameDirIn)
    end do

  end subroutine save_connectivity_targets
  !============================================================================

  subroutine trace_connectivity_target_rlonlat(NameTarget, RadiusTarget, &
       LonTarget, LatTarget, RadiusMinIn, nTraceIn, PowerIn, NameDirIn)

    ! Convenience wrapper if the target location is already known as
    ! radius/longitude/latitude.  Angles are radians.  Radius is in the same
    ! distance unit as MHData_VIB(X_:Z_,:,:), normally Rs.

    character(len=*), intent(in) :: NameTarget
    real,             intent(in) :: RadiusTarget, LonTarget, LatTarget
    real,    optional,intent(in) :: RadiusMinIn
    integer, optional,intent(in) :: nTraceIn
    real,    optional,intent(in) :: PowerIn
    character(len=*), optional,intent(in) :: NameDirIn

    ! XyzTarget_D is the Cartesian form passed to the main tracer. CosLat is
    ! calculated once and reused for both horizontal components.
    real :: XyzTarget_D(3), CosLat

    !--------------------------------------------------------------------------
    CosLat = cos(LatTarget)
    XyzTarget_D(1) = RadiusTarget*CosLat*cos(LonTarget)
    XyzTarget_D(2) = RadiusTarget*CosLat*sin(LonTarget)
    XyzTarget_D(3) = RadiusTarget*sin(LatTarget)

    call trace_connectivity_target(NameTarget, XyzTarget_D, &
         RadiusMinIn=RadiusMinIn, nTraceIn=nTraceIn, &
         PowerIn=PowerIn, NameDirIn=NameDirIn)

  end subroutine trace_connectivity_target_rlonlat
  !============================================================================
  subroutine trace_connectivity_target(NameTarget, XyzTarget_D, RadiusMinIn, &
       nTraceIn, PowerIn, NameDirIn, TargetTimeIn)

    ! Trace and save one target.  All MPI ranks must enter this routine together.
    ! Rank 0 performs the triangulation and writing after each rank has contributed
    ! its local field-line intersections.

    character(len=*), intent(in) :: NameTarget
    real,             intent(in) :: XyzTarget_D(3)
    real,    optional,intent(in) :: RadiusMinIn
    integer, optional,intent(in) :: nTraceIn
    real,    optional,intent(in) :: PowerIn
    character(len=*), optional,intent(in) :: NameDirIn
    real,    optional,intent(in) :: TargetTimeIn

    integer :: nTrace			! number of requested radial shells
    integer :: iTrace			! current shell index, progressing from the target inward
    integer :: nOut			! number of shells successfully added to the output trace
    integer :: nValid			! number of field lines intersecting the current shell
    integer :: iStatus			! status returned by the spherical triangulation

    ! A containing triangle is represented by three field-line indices.
    ! iCompactStencil_I indexes the shell-local trmesh node array.
    ! iGlobalStencil_I maps those nodes back to global field-line indices.
    ! iPrevStencil_I stores the previous shell's global triangle for propagation.
    integer :: iCompactStencil_I(3), iGlobalStencil_I(3), iPrevStencil_I(3)

    ! iList_I/iPointer_I/iEnd_I are the TRMESH adjacency structure.
    ! iLineNode_I maps compact shell nodes to persistent global line indices.
    integer, allocatable :: iList_I(:), iPointer_I(:), iEnd_I(:)
    integer, allocatable :: iLineNode_I(:)

    ! MPI gather counts and zero-based displacements, indexed by process.
    ! The Data variants are measured in real values rather than complete nodes.
    integer, allocatable :: nLocalValid_P(:), iDisp_P(:)
    integer, allocatable :: nRecvData_P(:), iDispData_P(:)

    real :: RadiusMin			! innermost requested shell
    real :: Power			! exponent controlling inward shell concentration
    real :: RadiusTarget		! norm of the Cartesian target position
    real :: LonTarget, LatTarget	! target angles in radians
    real :: AuxRadius, Norm2		! unused radius outputs from xyz_to_rlonlat conversions

    ! XyzPoint_D is the currently propagated unit-sphere position.
    ! Weight_I belongs to iPrevStencil_I and maps that triangle to the next shell.
    ! WeightNew_I is returned by the containing-triangle search on this shell.
    real :: XyzPoint_D(3), Weight_I(3), WeightNew_I(3)

    ! TimeField identifies the MHD state. TimeTarget identifies the satellite
    ! ephemeris position; the two differ during a steady-state trajectory sweep.
    real :: TimeField, TimeTarget

    ! Wall-clock diagnostics. The collection total uses the maximum elapsed
    ! time across MPI ranks because collective progress is limited by the slowest
    ! participant. TimeCollectPart_I separates reset, search, count gather, data
    ! gather, and root lookup fill.
    real(kind=8) :: Time0, Time1, DeltaTime, DeltaTimeMax
    real(kind=8) :: TimeTrace0, TimeTrace1
    real(kind=8) :: TimeCollect, TimeReorder, TimeTri
    real(kind=8) :: TimeFind, TimeWrite
    real(kind=8) :: TimeCollectReset, TimeCollectSearch
    real(kind=8) :: TimeCollectGatherCount, TimeCollectGatherData
    real(kind=8) :: TimeCollectLookup
    real(kind=8) :: TimeCollectPart_I(5)

    real, allocatable :: RadiusTrace_I(:)		! requested shell radii from the target inward
    real, allocatable :: XyzAll_DI(:,:)			! shell positions indexed by global field-line number
    real, allocatable :: XyzNode_DI(:,:)		! compact shell positions passed to trmesh
    real, allocatable :: DataLocal_DI(:,:)		! compact crossings generated on the local MPI rank
    real, allocatable :: DataNode_DI(:,:)		! compact crossings gathered on rank 0

    ! Rank-0 output arrays. Each successful shell stores one position, one
    ! global three-line stencil, and its three barycentric interpolation weights.
    real, allocatable :: LonOut_I(:), LatOut_I(:), ROut_I(:)
    real, allocatable :: XOut_I(:), YOut_I(:), ZOut_I(:)
    real, allocatable :: WeightOut_DI(:,:)

    ! TimeReduce_I is the MPI_MAX timing reduction buffer. DoLine_I marks
    ! which global field lines have a valid crossing on the current shell.
    real :: TimeReduce_I(6)
    integer, allocatable :: StencilOut_II(:,:)
    logical, allocatable :: DoLine_I(:)

    logical :: IsActive				! rank-0 trace can still proceed inward
    logical :: IsFound				! containing spherical triangle was found
    logical :: IsOk				! generic geometry/reordering success flag
    logical :: DoHaveTargetTime			! caller supplied an explicit ephemeris time
    integer :: ProgressInterval			! shell spacing between diagnostic progress messages
    character(len=300) :: StopReason		! final success or termination explanation

    character(len=*), parameter:: NameSub = 'trace_connectivity_target'
    !--------------------------------------------------------------------------
    call get_wall_time_seconds(TimeTrace0)

    TimeField = SPTime
    DoHaveTargetTime = present(TargetTimeIn)
    if(DoHaveTargetTime) then
       TimeTarget = TargetTimeIn
    else
       TimeTarget = SPTime
    end if

    TimeCollect = 0.0_8
    TimeCollectReset       = 0.0_8
    TimeCollectSearch      = 0.0_8
    TimeCollectGatherCount = 0.0_8
    TimeCollectGatherData  = 0.0_8
    TimeCollectLookup      = 0.0_8
    TimeReorder = 0.0_8
    TimeTri     = 0.0_8
    TimeFind    = 0.0_8
    TimeWrite   = 0.0_8

    RadiusMin = 1.15
    if(present(RadiusMinIn)) RadiusMin = RadiusMinIn

    nTrace = 300
    if(present(nTraceIn)) nTrace = nTraceIn
    if(nTrace < 2) call CON_stop(NameSub//': nTrace must be at least 2')

    Power = 3.0
    if(present(PowerIn)) Power = PowerIn
    if(Power <= 0.0) call CON_stop(NameSub//': PowerIn must be positive')

    RadiusTarget = sqrt(sum(XyzTarget_D**2))
    if(RadiusTarget <= cTiny) then
       if(iProc == 0) call write_empty_connectivity_file(NameTarget, NameDirIn, &
            'target radius is zero or invalid', TimeField, TimeTarget, &
            DoHaveTargetTime)
       RETURN
    end if

    call xyz_to_rlonlat(XyzTarget_D, AuxRadius, LonTarget, LatTarget)

    ProgressInterval = max(1, nTrace/max(1,nConnectivityProgress))

    if(iProc == 0 .and. DoDebugConnectivity) then
       write(*,*) 'SP:CONNECTIVITY trace start: target=', trim(NameTarget), &
            ' target_time=', TimeTarget, ' field_time=', TimeField, &
            ' r_target=', RadiusTarget, ' r_min=', RadiusMin, &
            ' nTrace=', nTrace
    end if

    ! The first generated shell is exactly at the target radius and the last
    ! is RadiusMin. Power > 1 concentrates more shells near the inner boundary.
    allocate(RadiusTrace_I(nTrace))
    call make_powerlaw_radius_array(RadiusTarget, RadiusMin, Power, RadiusTrace_I)

    if(iProc == 0) then
       allocate(LonOut_I(nTrace), LatOut_I(nTrace), ROut_I(nTrace))
       allocate(XOut_I(nTrace), YOut_I(nTrace), ZOut_I(nTrace))
       allocate(WeightOut_DI(3,nTrace), StencilOut_II(3,nTrace))
       LonOut_I = 0.0; LatOut_I = 0.0; ROut_I = 0.0
       XOut_I = 0.0; YOut_I = 0.0; ZOut_I = 0.0
       WeightOut_DI = 0.0; StencilOut_II = 0
    end if

    ! Reuse shell work arrays across all radial shells. Rank 0 keeps the
    ! global-index lookup arrays used by the adaptive stencil propagation.
    ! Each rank keeps only compact local crossing arrays. collect_shell_points
    ! gathers these compact local lists with MPI_Gatherv.
    if(iProc == 0) then
       allocate(XyzAll_DI(3,max(1,nLineAll)), DoLine_I(max(1,nLineAll)))
       allocate(XyzNode_DI(3,max(1,nLineAll)), iLineNode_I(max(1,nLineAll)))
       allocate(DataNode_DI(4,max(1,nLineAll)))
    else
       allocate(XyzAll_DI(3,1), DoLine_I(1))
       allocate(XyzNode_DI(3,1), iLineNode_I(1))
       allocate(DataNode_DI(4,1))
    end if
    allocate(DataLocal_DI(4,max(1,nLine)))
    allocate(nLocalValid_P(max(1,nProc)), iDisp_P(max(1,nProc)), &
         nRecvData_P(max(1,nProc)), iDispData_P(max(1,nProc)))

    nOut = 0
    IsActive = .true.
    StopReason = 'completed requested radial trace'
    iPrevStencil_I = 0
    Weight_I = 0.0

    ! All ranks loop over all shells. Even after rank 0 has stopped tracing,
    ! all ranks keep entering collect_shell_points so the collective reductions
    ! stay matched.
    do iTrace = 1, nTrace
       call get_wall_time_seconds(Time0)
       call collect_shell_points(RadiusTrace_I(iTrace), nValid, XyzAll_DI, &
            DoLine_I, XyzNode_DI, iLineNode_I, DataLocal_DI, DataNode_DI, &
            nLocalValid_P, iDisp_P, nRecvData_P, iDispData_P, TimeCollectPart_I)
       call get_wall_time_seconds(Time1)
       DeltaTime = Time1 - Time0

       if(DoDebugConnectivity) then
          ! The collective step is controlled by the slowest rank, not by rank 0.
          ! Reduce the per-rank elapsed time with MPI_MAX and accumulate that.
          ! The five detail entries split collection into root lookup reset,
          ! local line search, count gather, compact data gather, and root lookup fill.
          TimeReduce_I(1)   = real(DeltaTime)
          TimeReduce_I(2:6) = real(TimeCollectPart_I)
          if(nProc > 1) &
               call MPI_reduce_real_array(TimeReduce_I, 6, MPI_MAX, 0, &
               iComm, iError)
          if(iProc == 0) then
             DeltaTimeMax = real(TimeReduce_I(1), kind=8)
             TimeCollect = TimeCollect + DeltaTimeMax
             TimeCollectReset       = TimeCollectReset       + real(TimeReduce_I(2), kind=8)
             TimeCollectSearch      = TimeCollectSearch      + real(TimeReduce_I(3), kind=8)
             TimeCollectGatherCount = TimeCollectGatherCount + real(TimeReduce_I(4), kind=8)
             TimeCollectGatherData  = TimeCollectGatherData  + real(TimeReduce_I(5), kind=8)
             TimeCollectLookup      = TimeCollectLookup      + real(TimeReduce_I(6), kind=8)
          else
             DeltaTimeMax = DeltaTime
          end if
       else
          DeltaTimeMax = DeltaTime
       end if

       if(iProc == 0 .and. DoDebugConnectivity .and. &
            DeltaTimeMax > SlowConnectivityTime) then
          write(*,*) 'SP:CONNECTIVITY SLOW collect_shell_points: target=', &
               trim(NameTarget), ' shell=', iTrace, ' r=', &
               RadiusTrace_I(iTrace), ' nValid=', nValid, &
               ' dt_max=', DeltaTimeMax
       end if

       if(iProc /= 0) then
          CYCLE
       end if

       if(.not.IsActive) then
          CYCLE
       end if

       if(DoDebugConnectivity .and. &
            (mod(iTrace - 1, ProgressInterval) == 0 .or. iTrace == nTrace)) &
            write(*,*) 'SP:CONNECTIVITY progress: target=', trim(NameTarget), &
            ' shell=', iTrace, '/', nTrace, ' r=', RadiusTrace_I(iTrace), &
            ' nValid=', nValid

       if(nValid < 3) then
          IsActive = .false.
          write(StopReason,'(a,i0,a,f12.5)') &
               'fewer than 3 valid field lines on shell ', iTrace, &
               ', r=', RadiusTrace_I(iTrace)
          CYCLE
       end if

       call get_wall_time_seconds(Time0)
       call put_noncollinear_nodes_first(nValid, XyzNode_DI, iLineNode_I, IsOk)
       call get_wall_time_seconds(Time1)
       DeltaTime = Time1 - Time0
       TimeReorder = TimeReorder + DeltaTime
       if(DoDebugConnectivity .and. DeltaTime > SlowConnectivityTime) &
            write(*,*) 'SP:CONNECTIVITY SLOW reorder: target=', &
            trim(NameTarget), ' shell=', iTrace, ' r=', &
            RadiusTrace_I(iTrace), ' nValid=', nValid, ' dt=', DeltaTime

       if(.not.IsOk) then
          IsActive = .false.
          write(StopReason,'(a,i0,a,f12.5)') &
               'could not find 3 non-collinear shell nodes on shell ', iTrace, &
               ', r=', RadiusTrace_I(iTrace)
          CYCLE
       end if

       ! At the first shell, the mapped point is simply the target direction.
       ! At later shells, it is reconstructed from the previous triangle's same
       ! three global field lines and the previous barycentric weights.
       if(iTrace == 1) then
          XyzPoint_D = XyzTarget_D/RadiusTarget
       else
          if(any(iPrevStencil_I < 1) .or. any(iPrevStencil_I > nLineAll)) then
             IsActive = .false.
             write(StopReason,'(a,i0)') 'previous stencil is invalid on shell ', iTrace
             CYCLE
          end if

          if(.not.all(DoLine_I(iPrevStencil_I))) then
             IsActive = .false.
             write(StopReason,'(a,i0,a,3(1x,i0))') &
                  'previous stencil no longer reaches shell ', iTrace, &
                  '; stencil=', iPrevStencil_I
             CYCLE
          end if

          ! Applying the old weights to the old stencil's intersections on the
          ! new shell is the actual shell-to-shell connectivity propagation step.
          XyzPoint_D = Weight_I(1)*XyzAll_DI(:,iPrevStencil_I(1)) + &
               Weight_I(2)*XyzAll_DI(:,iPrevStencil_I(2)) + &
               Weight_I(3)*XyzAll_DI(:,iPrevStencil_I(3))
          call normalize_unit_vector(XyzPoint_D, IsOk)
          if(.not.IsOk) then
             IsActive = .false.
             write(StopReason,'(a,i0)') 'weighted point collapsed on shell ', iTrace
             CYCLE
          end if
       end if

       ! Rebuild the spherical triangulation on every shell because field-line
       ! positions change with radius and some lines may terminate before this shell.
       allocate(iList_I(6*(nValid-2)), iPointer_I(6*(nValid-2)), iEnd_I(nValid))
       call get_wall_time_seconds(Time0)
       call trmesh(nValid, XyzNode_DI(1,:), XyzNode_DI(2,:), XyzNode_DI(3,:), &
            iList_I, iPointer_I, iEnd_I, iStatus)
       call get_wall_time_seconds(Time1)
       DeltaTime = Time1 - Time0
       TimeTri = TimeTri + DeltaTime
       if(DoDebugConnectivity .and. DeltaTime > SlowConnectivityTime) &
            write(*,*) 'SP:CONNECTIVITY SLOW trmesh: target=', &
            trim(NameTarget), ' shell=', iTrace, ' r=', &
            RadiusTrace_I(iTrace), ' nValid=', nValid, ' dt=', DeltaTime

       if(iStatus /= 0) then
          IsActive = .false.
          write(StopReason,'(a,i0,a,i0,a,f12.5)') &
               'trmesh failed with status ', iStatus, ' on shell ', iTrace, &
               ', r=', RadiusTrace_I(iTrace)
          deallocate(iList_I, iPointer_I, iEnd_I)
          CYCLE
       end if

       call get_wall_time_seconds(Time0)
       call find_triangle_orig(XyzPoint_D, nValid, XyzNode_DI, iList_I, &
            iPointer_I, iEnd_I, WeightNew_I, IsFound, iCompactStencil_I)
       call get_wall_time_seconds(Time1)
       DeltaTime = Time1 - Time0
       TimeFind = TimeFind + DeltaTime
       if(DoDebugConnectivity .and. DeltaTime > SlowConnectivityTime) &
            write(*,*) 'SP:CONNECTIVITY SLOW find_triangle_orig: target=', &
            trim(NameTarget), ' shell=', iTrace, ' r=', &
            RadiusTrace_I(iTrace), ' nValid=', nValid, ' dt=', DeltaTime

       deallocate(iList_I, iPointer_I, iEnd_I)

       if(.not.IsFound) then
          IsActive = .false.
          write(StopReason,'(a,i0,a,f12.5)') &
               'no containing spherical triangle on shell ', iTrace, &
               ', r=', RadiusTrace_I(iTrace)
          CYCLE
       end if

       ! Convert shell-local triangle indices to persistent global line indices.
       ! The resulting stencil and weights both drive the next shell and enter the
       ! output file.
       iGlobalStencil_I = iLineNode_I(iCompactStencil_I)
       iPrevStencil_I   = iGlobalStencil_I
       Weight_I         = WeightNew_I

       ! XyzPoint_D is a unit direction; multiplying it by the shell radius
       ! recovers the Cartesian point in the simulation distance unit, normally Rs.
       nOut = nOut + 1
       ROut_I(nOut) = RadiusTrace_I(iTrace)
       XOut_I(nOut) = RadiusTrace_I(iTrace)*XyzPoint_D(1)
       YOut_I(nOut) = RadiusTrace_I(iTrace)*XyzPoint_D(2)
       ZOut_I(nOut) = RadiusTrace_I(iTrace)*XyzPoint_D(3)
       WeightOut_DI(:,nOut) = Weight_I
       StencilOut_II(:,nOut) = iGlobalStencil_I

       call xyz_to_rlonlat(XyzPoint_D, Norm2, LonOut_I(nOut), LatOut_I(nOut))
       LonOut_I(nOut) = modulo(LonOut_I(nOut)*cRadToDeg, 360.0)
       LatOut_I(nOut) = LatOut_I(nOut)*cRadToDeg

    end do

    call deallocate_shell_arrays(XyzAll_DI, DoLine_I, XyzNode_DI, iLineNode_I)
    if(allocated(DataLocal_DI)) deallocate(DataLocal_DI)
    if(allocated(DataNode_DI)) deallocate(DataNode_DI)
    if(allocated(nLocalValid_P)) deallocate(nLocalValid_P)
    if(allocated(iDisp_P)) deallocate(iDisp_P)
    if(allocated(nRecvData_P)) deallocate(nRecvData_P)
    if(allocated(iDispData_P)) deallocate(iDispData_P)

    if(iProc == 0) then
       call get_wall_time_seconds(Time0)
       call write_connectivity_file(NameTarget, NameDirIn, RadiusTarget, &
            LonTarget*cRadToDeg, LatTarget*cRadToDeg, nOut, ROut_I, &
            LonOut_I, LatOut_I, XOut_I, YOut_I, ZOut_I, StencilOut_II, &
            WeightOut_DI, StopReason, TimeField, TimeTarget, &
            DoHaveTargetTime)
       call get_wall_time_seconds(Time1)
       TimeWrite = TimeWrite + (Time1 - Time0)

       call get_wall_time_seconds(TimeTrace1)
       if(DoDebugConnectivity) then
          write(*,*) 'SP:CONNECTIVITY trace done: target=', trim(NameTarget), &
               ' nOut=', nOut, ' total_dt=', TimeTrace1 - TimeTrace0
          write(*,*) 'SP:CONNECTIVITY timing: target=', trim(NameTarget), &
               ' collect_max=', TimeCollect, ' reorder=', TimeReorder, &
               ' trmesh=', TimeTri, ' find=', TimeFind, ' write=', TimeWrite
          write(*,*) 'SP:CONNECTIVITY collect detail: target=', trim(NameTarget), &
               ' reset=', TimeCollectReset, ' search=', TimeCollectSearch, &
               ' gather_count=', TimeCollectGatherCount, &
               ' gather_data=', TimeCollectGatherData, &
               ' lookup=', TimeCollectLookup
          write(*,*) 'SP:CONNECTIVITY stop: target=', trim(NameTarget), &
               ' reason=', trim(StopReason)
       end if

       deallocate(LonOut_I, LatOut_I, ROut_I, XOut_I, YOut_I, ZOut_I, &
            WeightOut_DI, StencilOut_II)
    end if

    deallocate(RadiusTrace_I)

  end subroutine trace_connectivity_target
  !============================================================================
  subroutine collect_shell_points(Radius, nValid, XyzAll_DI, DoLine_I, &
       XyzNode_DI, iLineNode_I, DataLocal_DI, DataNode_DI, &
       nLocalValid_P, iDisp_P, nRecvData_P, iDispData_P, TimePart_I)

    ! The workhorse.
    ! Intersect every local field line with the requested radial shell. Rank 0
    ! receives compact node arrays for triangulation.
    !
    ! Each rank builds a compact local list of only the shell crossings it owns.
    ! The compact data gathered to rank 0 has four real values per node:
    !   1:3  unit-sphere x/y/z crossing position
    !   4    global field-line index stored as real and converted with nint
    !
    ! Rank 0 then fills XyzNode_DI/iLineNode_I for trmesh, and also fills the
    ! global-index lookup arrays XyzAll_DI/DoLine_I so the existing adaptive
    ! stencil propagation logic can remain unchanged.
    !
    ! TimePart_I returns local wall times for:
    !   1 rank-0 reset of global lookup arrays
    !   2 local field-line search/interpolation
    !   3 MPI gather of local valid counts
    !   4 MPI gatherv of compact node data
    !   5 rank-0 fill of compact and global lookup arrays

    real, intent(in) :: Radius
    integer, intent(out) :: nValid
    real, intent(inout) :: XyzAll_DI(:,:), XyzNode_DI(:,:)
    real, intent(inout) :: DataLocal_DI(:,:), DataNode_DI(:,:)
    logical, intent(inout) :: DoLine_I(:)
    integer, intent(inout) :: iLineNode_I(:)
    integer, intent(inout) :: nLocalValid_P(:), iDisp_P(:)
    integer, intent(inout) :: nRecvData_P(:), iDispData_P(:)
    real(kind=8), intent(out) :: TimePart_I(5)

    integer :: iLine			! local field-line index on this MPI rank
    integer :: iLineAll			! corresponding global field-line index
    integer :: iNode			! node index in the compact gathered array
    integer :: iRank			! process index used to construct gather displacements
    integer :: nLocalValid		! number of valid crossings found on this rank
    real :: Xyz_D(3)			! interpolated crossing, subsequently normalized
    logical :: DoFound			! true when a usable shell crossing exists
    real(kind=8) :: Time0, Time1

    !--------------------------------------------------------------------------
    TimePart_I = 0.0_8
    nValid = 0

    ! Only rank 0 needs the full global-index lookup arrays.
    call get_wall_time_seconds(Time0)
    if(iProc == 0) then
       XyzAll_DI = 0.0
       DoLine_I  = .false.
    end if
    call get_wall_time_seconds(Time1)
    TimePart_I(1) = Time1 - Time0

    ! Build compact local node data.
    call get_wall_time_seconds(Time0)
    nLocalValid = 0
    do iLine = 1, nLine
       if(.not.Used_B(iLine)) CYCLE
       call get_line_position_at_radius(iLine, Radius, Xyz_D, DoFound)
       if(.not.DoFound) CYCLE
       call normalize_unit_vector(Xyz_D, DoFound)
       if(.not.DoFound) CYCLE

       nLocalValid = nLocalValid + 1
       DataLocal_DI(1:3,nLocalValid) = Xyz_D
       DataLocal_DI(4,nLocalValid)   = real(iLineAll0 + iLine)
    end do
    call get_wall_time_seconds(Time1)
    TimePart_I(2) = Time1 - Time0

    ! Gather compact-list sizes. Displacements are zero-based and expressed in
    ! numbers of nodes for iDisp_P, and in numbers of real values for iDispData_P.
    call get_wall_time_seconds(Time0)
    if(nProc > 1) then
       call MPI_Gather(nLocalValid, 1, MPI_INTEGER, nLocalValid_P, 1, &
            MPI_INTEGER, 0, iComm, iError)
    else
       nLocalValid_P(1) = nLocalValid
    end if
    call get_wall_time_seconds(Time1)
    TimePart_I(3) = Time1 - Time0

    if(iProc == 0) then
       nValid = sum(nLocalValid_P(1:nProc))
       iDisp_P(1) = 0
       do iRank = 2, nProc
          iDisp_P(iRank) = iDisp_P(iRank-1) + nLocalValid_P(iRank-1)
       end do
       nRecvData_P(1:nProc) = 4*nLocalValid_P(1:nProc)
       iDispData_P(1:nProc) = 4*iDisp_P(1:nProc)
    end if

    ! Gather all compact node data to rank 0.
    call get_wall_time_seconds(Time0)
    if(nProc > 1) then
       call MPI_Gatherv(DataLocal_DI(1,1), 4*nLocalValid, MPI_REAL, &
            DataNode_DI(1,1), nRecvData_P, iDispData_P, MPI_REAL, 0, &
            iComm, iError)
    else
       if(iProc == 0 .and. nLocalValid > 0) &
            DataNode_DI(:,1:nLocalValid) = DataLocal_DI(:,1:nLocalValid)
    end if
    call get_wall_time_seconds(Time1)
    TimePart_I(4) = Time1 - Time0

    if(iProc /= 0) RETURN

    ! Fill compact trmesh arrays and global-index lookup arrays on rank 0.
    call get_wall_time_seconds(Time0)
    do iNode = 1, nValid
       iLineAll = nint(DataNode_DI(4,iNode))
       if(iLineAll < 1 .or. iLineAll > nLineAll) CYCLE

       XyzNode_DI(:,iNode) = DataNode_DI(1:3,iNode)
       iLineNode_I(iNode)  = iLineAll
       XyzAll_DI(:,iLineAll) = XyzNode_DI(:,iNode)
       DoLine_I(iLineAll) = .true.
    end do
    call get_wall_time_seconds(Time1)
    TimePart_I(5) = Time1 - Time0

  end subroutine collect_shell_points
  !============================================================================
  subroutine get_line_position_at_radius(iLine, Radius, Xyz_D, DoFound)

    ! Use the same line-search routine that existing MFLAMPA fixed-radius
    ! outputs use. This avoids duplicating along-field-line crossing logic.

    integer, intent(in) :: iLine
    real,    intent(in) :: Radius
    real,    intent(out):: Xyz_D(3)
    logical, intent(out):: DoFound

    ! search_line returns iAbove, the first stored line point above Radius,
    ! and Weight, the interpolation fraction between iAbove-1 and iAbove.
    ! In this interface DoPrint also acts as the valid-crossing flag.
    integer :: iAbove
    real :: Weight
    logical :: DoPrint

    !--------------------------------------------------------------------------
    Xyz_D = 0.0
    DoPrint = .true.

    ! iAbove == 1 leaves no lower neighbor and therefore cannot be linearly
    ! interpolated at the requested radius.
    call search_line(iLine, Radius, iAbove, DoPrint, Weight)
    DoFound = DoPrint .and. iAbove /= 1

    if(.not.DoFound) RETURN

    Xyz_D = MHData_VIB(X_:Z_, iAbove-1, iLine)*(1.0 - Weight) + &
         MHData_VIB(X_:Z_, iAbove,   iLine)*Weight

  end subroutine get_line_position_at_radius
  !============================================================================
  subroutine make_powerlaw_radius_array(RadiusTop, RadiusBottom, Power, Radius_I)

    ! The target position is extracted in inwards radial steps.
    ! Generally, the magnetic field becomes more dynamic closer to the Sun.
    ! Hence, the radial array is refined inwards. The exponent can be defined
    ! in the PARAM.in. The recommended value is a power of 3.

    real, intent(in)  :: RadiusTop, RadiusBottom, Power
    real, intent(out) :: Radius_I(:)

    ! s is a uniform 0-to-1 parameter; Weight is its power-law remapping.
    integer :: iTrace, nTrace
    real :: s, Weight

    !--------------------------------------------------------------------------
    nTrace = size(Radius_I)
    if(nTrace == 1) then
       Radius_I(1) = RadiusTop
       RETURN
    end if

    do iTrace = 1, nTrace
       ! For Power > 1, Weight changes slowly near the target and rapidly near
       ! the lower boundary. Because RadiusBottom < RadiusTop, this yields smaller
       ! radial steps near RadiusBottom.
       s = real(iTrace - 1)/real(nTrace - 1)
       Weight = 1.0 - (1.0 - s)**Power
       Radius_I(iTrace) = RadiusTop + (RadiusBottom - RadiusTop)*Weight
    end do

  end subroutine make_powerlaw_radius_array
  !============================================================================
  subroutine normalize_unit_vector(Xyz_D, IsOk)

    real,    intent(inout) :: Xyz_D(3)
    logical, intent(out)   :: IsOk

    real :: Norm

    !--------------------------------------------------------------------------
    Norm = sqrt(sum(Xyz_D**2))
    IsOk = Norm > cTiny
    if(IsOk) Xyz_D = Xyz_D/Norm

  end subroutine normalize_unit_vector
  !============================================================================
  subroutine put_noncollinear_nodes_first(nNode, Xyz_DI, iLine_I, IsOk)

    ! TRMESH requires the first three nodes not to lie on one great circle.
    ! Reorder the compact node list, preserving the compact-to-global line map.

    integer, intent(in)    :: nNode
    real,    intent(inout) :: Xyz_DI(3,nNode)
    integer, intent(inout) :: iLine_I(nNode)
    logical, intent(out)   :: IsOk

    ! i1/i2/i3 enumerate candidate nodes. Cross_D is normal to the first
    ! two vectors, and Triple is their scalar triple product with the third.
    ! A nonzero Triple means the three points do not lie on one great circle.
    ! j2/j3 track indices after earlier swaps move candidates into positions 1-3.
    integer :: i1, i2, i3, j2, j3
    real :: Cross_D(3), Triple

    !--------------------------------------------------------------------------
    IsOk = .false.
    if(nNode < 3) RETURN

    do i1 = 1, nNode - 2
       do i2 = i1 + 1, nNode - 1
          Cross_D = cross_product(Xyz_DI(:,i1), Xyz_DI(:,i2))
          if(sqrt(sum(Cross_D**2)) <= cTiny) CYCLE
          do i3 = i2 + 1, nNode
             Triple = abs(sum(Cross_D*Xyz_DI(:,i3)))
             if(Triple <= cTiny) CYCLE
             j2 = i2
             j3 = i3
             call swap_nodes(1, i1, Xyz_DI, iLine_I)
             if(j2 == 1) j2 = i1
             if(j3 == 1) j3 = i1
             call swap_nodes(2, j2, Xyz_DI, iLine_I)
             if(j3 == 2) j3 = j2
             call swap_nodes(3, j3, Xyz_DI, iLine_I)
             IsOk = .true.
             RETURN
          end do
       end do
    end do

  end subroutine put_noncollinear_nodes_first
  !============================================================================
  function cross_product(A_D, B_D) result(C_D)

    real, intent(in) :: A_D(3), B_D(3)
    real :: C_D(3)

    !--------------------------------------------------------------------------
    C_D(1) = A_D(2)*B_D(3) - A_D(3)*B_D(2)
    C_D(2) = A_D(3)*B_D(1) - A_D(1)*B_D(3)
    C_D(3) = A_D(1)*B_D(2) - A_D(2)*B_D(1)

  end function cross_product
  !============================================================================
  subroutine swap_nodes(iA, iB, Xyz_DI, iLine_I)

    integer, intent(in)    :: iA, iB
    real,    intent(inout) :: Xyz_DI(:,:)
    integer, intent(inout) :: iLine_I(:)

    real :: XyzTmp_D(3)
    integer :: iTmp

    !--------------------------------------------------------------------------
    if(iA == iB) RETURN

    XyzTmp_D = Xyz_DI(:,iA)
    Xyz_DI(:,iA) = Xyz_DI(:,iB)
    Xyz_DI(:,iB) = XyzTmp_D

    iTmp = iLine_I(iA)
    iLine_I(iA) = iLine_I(iB)
    iLine_I(iB) = iTmp

  end subroutine swap_nodes
  !============================================================================
  subroutine deallocate_shell_arrays(XyzAll_DI, DoLine_I, XyzNode_DI, iLineNode_I)

    real, allocatable, intent(inout) :: XyzAll_DI(:,:), XyzNode_DI(:,:)
    logical, allocatable, intent(inout) :: DoLine_I(:)
    integer, allocatable, intent(inout) :: iLineNode_I(:)

    !--------------------------------------------------------------------------
    if(allocated(XyzAll_DI)) deallocate(XyzAll_DI)
    if(allocated(DoLine_I)) deallocate(DoLine_I)
    if(allocated(XyzNode_DI)) deallocate(XyzNode_DI)
    if(allocated(iLineNode_I)) deallocate(iLineNode_I)

  end subroutine deallocate_shell_arrays
  !============================================================================
  subroutine write_connectivity_file(NameTarget, NameDirIn, RadiusTarget, &
       LonTargetDeg, LatTargetDeg, nOut, ROut_I, LonOut_I, LatOut_I, &
       XOut_I, YOut_I, ZOut_I, StencilOut_II, WeightOut_DI, StopReason, &
       TimeField, TimeTarget, DoHaveTargetTime)

    character(len=*), intent(in) :: NameTarget
    character(len=*), optional, intent(in) :: NameDirIn
    real, intent(in) :: RadiusTarget, LonTargetDeg, LatTargetDeg
    integer, intent(in) :: nOut
    real, intent(in) :: ROut_I(:), LonOut_I(:), LatOut_I(:)
    real, intent(in) :: XOut_I(:), YOut_I(:), ZOut_I(:)
    integer, intent(in) :: StencilOut_II(:,:)
    real, intent(in) :: WeightOut_DI(:,:)
    character(len=*), intent(in) :: StopReason
    real, intent(in) :: TimeField, TimeTarget
    logical, intent(in) :: DoHaveTargetTime

    ! StringFieldTime and StringTargetTime are human-readable timestamps.
    ! iOut indexes the successfully traced output rows.
    character(len=300) :: NameFile
    character(len=32) :: StringFieldTime, StringTargetTime
    integer :: iOut

    character(len=*), parameter:: NameSub = 'write_connectivity_file'
    !--------------------------------------------------------------------------
    call make_datetime_string(TimeField, StringFieldTime)
    if(DoHaveTargetTime) then
       call make_datetime_string(TimeTarget, StringTargetTime)
    else
       StringTargetTime = 'not_available'
    end if

    call make_connectivity_file_name(NameTarget, NameDirIn, NameFile, &
         TimeField, TimeTarget, DoHaveTargetTime)

    if(DoDebugConnectivity) then
       write(*,*) 'SP:CONNECTIVITY writing file: ', trim(NameFile)
    else
       write(*,*) 'SP:CONNECTIVITY file: ', trim(NameFile)
    end if

    call open_file(file=NameFile, status='replace', NameCaller=NameSub)

    write(UnitTmp_,'(a)') '# Magnetic Field-Line'
    write(UnitTmp_,'(a,a)') '# target: ', trim(NameTarget)
    write(UnitTmp_,'(a,a)') '# coord_system: ', trim(TypeCoordSystem)
    write(UnitTmp_,'(a,es16.8)') '# field_time_s: ', TimeField
    write(UnitTmp_,'(a,a)') '# field_datetime: ', trim(StringFieldTime)
    if(DoHaveTargetTime) then
       write(UnitTmp_,'(a,es16.8)') '# target_time_s: ', TimeTarget
       write(UnitTmp_,'(a,a)') '# target_datetime: ', trim(StringTargetTime)
    else
       write(UnitTmp_,'(a)') '# target_time_s: not_available'
       write(UnitTmp_,'(a)') '# target_datetime: not_available'
    end if
    write(UnitTmp_,'(a,es16.8)') '# target_r_rs: ', RadiusTarget
    write(UnitTmp_,'(a,es16.8)') '# target_lon_deg: ', modulo(LonTargetDeg, 360.0)
    write(UnitTmp_,'(a,es16.8)') '# target_lat_deg: ', LatTargetDeg
    write(UnitTmp_,'(a,i8)') '# n_point: ', nOut
    write(UnitTmp_,'(a,a)') '# stop_reason: ', trim(StopReason)
    write(UnitTmp_,'(a)') '# columns:'
    write(UnitTmp_,'(a)') '# i r_rs r_au lon_deg lat_deg x_rs y_rs z_rs '// &
         'i_line_1 i_line_2 i_line_3 w1 w2 w3'

    ! Each row stores the traced position, the three global field lines that
    ! form its containing triangle, and the corresponding barycentric weights.
    do iOut = 1, nOut
       write(UnitTmp_, '(i8,1x,7(es18.10,1x),3(i8,1x),3(es18.10,1x))') &
            iOut, ROut_I(iOut), ROut_I(iOut)*cRsToAu, &
            LonOut_I(iOut), LatOut_I(iOut), &
            XOut_I(iOut), YOut_I(iOut), ZOut_I(iOut), &
            StencilOut_II(1:3,iOut), WeightOut_DI(1:3,iOut)
    end do

    call close_file

  end subroutine write_connectivity_file
  !============================================================================
  subroutine write_empty_connectivity_file(NameTarget, NameDirIn, Reason, &
       TimeField, TimeTarget, DoHaveTargetTime)

    character(len=*), intent(in) :: NameTarget
    character(len=*), optional, intent(in) :: NameDirIn
    character(len=*), intent(in) :: Reason
    real, intent(in) :: TimeField, TimeTarget
    logical, intent(in) :: DoHaveTargetTime

    character(len=300) :: NameFile
    character(len=32) :: StringFieldTime, StringTargetTime

    character(len=*), parameter:: NameSub = 'write_empty_connectivity_file'
    !--------------------------------------------------------------------------
    if(iProc /= 0) RETURN

    call make_datetime_string(TimeField, StringFieldTime)
    if(DoHaveTargetTime) then
       call make_datetime_string(TimeTarget, StringTargetTime)
    else
       StringTargetTime = 'not_available'
    end if

    call make_connectivity_file_name(NameTarget, NameDirIn, NameFile, &
         TimeField, TimeTarget, DoHaveTargetTime)
    if(DoDebugConnectivity) then
       write(*,*) 'SP:CONNECTIVITY writing empty file: ', trim(NameFile)
    else
       write(*,*) 'SP:CONNECTIVITY empty file: ', trim(NameFile)
    end if
    call open_file(file=NameFile, status='replace', NameCaller=NameSub)
    write(UnitTmp_,'(a)') '# Magnetic Field-Line'
    write(UnitTmp_,'(a,a)') '# target: ', trim(NameTarget)
    write(UnitTmp_,'(a,a)') '# coord_system: ', trim(TypeCoordSystem)
    write(UnitTmp_,'(a,es16.8)') '# field_time_s: ', TimeField
    write(UnitTmp_,'(a,a)') '# field_datetime: ', trim(StringFieldTime)
    if(DoHaveTargetTime) then
       write(UnitTmp_,'(a,es16.8)') '# target_time_s: ', TimeTarget
       write(UnitTmp_,'(a,a)') '# target_datetime: ', trim(StringTargetTime)
    else
       write(UnitTmp_,'(a)') '# target_time_s: not_available'
       write(UnitTmp_,'(a)') '# target_datetime: not_available'
    end if
    write(UnitTmp_,'(a,i8)') '# n_point: ', 0
    write(UnitTmp_,'(a,a)') '# stop_reason: ', trim(Reason)
    write(UnitTmp_,'(a)') '# columns:'
    write(UnitTmp_,'(a)') '# i r_rs r_au lon_deg lat_deg x_rs y_rs z_rs '// &
         'i_line_1 i_line_2 i_line_3 w1 w2 w3'
    call close_file

  end subroutine write_empty_connectivity_file
  !============================================================================
  subroutine make_connectivity_file_name(NameTarget, NameDirIn, NameFile, &
       TimeField, TimeTarget, DoHaveTargetTime)

    character(len=*), intent(in) :: NameTarget
    character(len=*), optional, intent(in) :: NameDirIn
    character(len=*), intent(out):: NameFile
    real, intent(in) :: TimeField, TimeTarget
    logical, intent(in) :: DoHaveTargetTime

    ! NameDir is normalized to include a trailing slash. StringTargetTime is
    ! included in the filename. StringFieldTime is computed but the current file
    ! format does not include it explicitly.
    character(len=180) :: NameDir
    character(len=32)  :: StringFieldTime, StringTargetTime

    !--------------------------------------------------------------------------
    if(present(NameDirIn)) then
       NameDir = trim(NameDirIn)
    else
       NameDir = 'SP/IO2/'
    end if

    if(len_trim(NameDir) > 0) then
       if(NameDir(len_trim(NameDir):len_trim(NameDir)) /= '/') &
            NameDir = trim(NameDir)//'/'
    end if

    call make_datetime_string(TimeField, StringFieldTime)
    if(DoHaveTargetTime) then
       call make_datetime_string(TimeTarget, StringTargetTime)
    else
       StringTargetTime = 'notime'
    end if

    ! iIter prevents collisions when multiple field states use the same target
    ! timestamp. The current name includes target time and iteration number.
    write(NameFile,'(a,a,a,a,a,a,i6.6,a)') trim(NameDir), &
       'connect_', trim(adjustl(NameTarget)), '_t', &
       trim(StringTargetTime), '_n', iIter, '.dat'

  end subroutine make_connectivity_file_name
  !============================================================================
  subroutine make_datetime_string(TimeRelative, StringTime)

    real, intent(in) :: TimeRelative
    character(len=*), intent(out) :: StringTime

    ! time_real_to_int returns year, month, day, hour, minute, second, and a
    ! seventh sub-second field. The filename/metadata string prints the first six.
    integer :: iTime_I(7)

    !--------------------------------------------------------------------------
    call time_real_to_int(StartTime + TimeRelative, iTime_I)
    write(StringTime,'(i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2)') &
         iTime_I(1:3), '_', iTime_I(4:6)

  end subroutine make_datetime_string
  !============================================================================
  subroutine get_wall_time_seconds(Time)

    real(kind=8), intent(out) :: Time

    integer :: Count			! current system-clock tick
    integer :: CountRate		! ticks per second
    integer :: CountMax			! implementation's maximum tick value

    !--------------------------------------------------------------------------
    call system_clock(Count, CountRate, CountMax)
    if(CountRate > 0) then
       Time = real(Count, kind=8)/real(CountRate, kind=8)
    else
       Time = 0.0_8
    end if

  end subroutine get_wall_time_seconds
  !============================================================================

end module SP_ModConnectivity
!==============================================================================

