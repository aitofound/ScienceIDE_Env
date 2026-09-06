!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModSatellite

  use ModUtilities, ONLY: open_file, close_file, CON_stop
  use SP_ModProc, ONLY: iProc, iComm, iError
  use ModMpi
  use SP_ModSize, ONLY: nDim
  use SP_ModGrid, ONLY: nP, nMu
  use SP_ModTestFunc, ONLY: lVerbose, test_start, test_stop
  use SP_ModGrid, ONLY: TypeCoordSystem
  use SP_ModTime, ONLY: SPTime, StartTime

  implicit none

  save

  private ! Except

  public:: read_param                 ! read satellite-related parameters
  public:: init                       ! Initialize for satellite triangulation
  public:: read_satellite_input_files ! read satellite trajectories
  public:: set_satellite_positions    ! set satellite positions
  public:: write_satellite_file
  public:: get_satellite_position_at_time
  public:: get_satellite_time_range

  ! ----- For saving the distribution function along SATELLITES -----
  integer, public    :: nSat   = 0    ! number of satellites for DistrTraj_
  integer, parameter :: MaxSat = 10   ! Max number of satellites

  ! Names and unit numbers for satellite files
  character(len=50), public:: NameFileSat_I(MaxSat)
  ! Names of the satellite
  character(len=50), public:: NameSat_I(MaxSat)
  ! the output directory
  character(len=*), public, parameter :: NamePlotDir = "SP/IO2/"

  ! Current positions
  real, public:: XyzSat_DI(nDim, MaxSat)

  ! variables to record tracked and current satellite position indices
  logical, public:: UseSatellite               = .false.
  logical, public:: DoTrackSatellite_I(MaxSat) = .false.
  integer, public:: iPointCurrentSat_I(MaxSat) = 1

  ! Local variables
  ! nPointTraj_I/TimeSat_II/XyzSat_DII are the legacy, trimmed
  ! trajectories used by set_satellite_positions. They keep only the last
  ! point before StartTime plus all later points, so time-accurate tracking
  ! still behaves as before.
  integer, public           :: nPointTraj_I(MaxSat)
  real, allocatable         :: XyzSat_DII(:, :, :)
  real, public, allocatable :: TimeSat_II(:, :)

  ! Full trajectories are preserved for steady-state connectivity sampling.
  ! In steady-state runs the requested trajectory window may extend before
  ! StartTime, so the trimming above would otherwise discard valid ephemeris
  ! points. These arrays are not used by the ordinary satellite output path.
  integer, public           :: nPointTrajFull_I(MaxSat) = 0
  real, allocatable         :: XyzSatFull_DII(:, :, :)
  real, public, allocatable :: TimeSatFull_II(:, :)

  ! Unlike ModSatellite in BATSRUS, here the saveplot frequencyis controlled
  ! by #NOUTPUT in SP_ModPlot so we do not read StartTime/EndTime/DtTraj.

  ! Variables for interpolation in a triangular mesh
  integer  :: iStencil_I(3)
  real     :: Weight_I(3) ! weights for interpolation
  real,    public, allocatable :: Distribution_III(:,:,:)
  ! Test triangulation
  logical, public :: DoTestTri = .false.
  real :: XyzLocTestTri_I(nDim)
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var

    character(len=*), intent(in) :: NameCommand
    logical :: DoTest
    integer :: iSat                         ! loop variable for satellites
    ! VARs for SATELLITE, specified to save the VDF along trajectories
    integer :: l1, l2                       ! indices to get satellite name
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    call test_start(NameSub, DoTest)

    select case(NameCommand)
    case("#TESTTRIANGULATE")
       call read_var('TestTriangulate', DoTestTri)
       if(DoTestTri) then
          call read_var('XLocTestTri', XyzLocTestTri_I(1))
          call read_var('YLocTestTri', XyzLocTestTri_I(2))
          call read_var('ZLocTestTri', XyzLocTestTri_I(3))
       end if

    case("#SATELLITE")
       ! Read the number of satellites, for saving the spectrum by DistrTraj_
       call read_var('nSatellite', nSat)
       if(nSat <= 0) RETURN
       UseSatellite = .true.
       if(nSat > MaxSat) call CON_stop(NameSub // ': Number of ' // &
            'output files is too large in #SATELLITE: nSat > MaxSat')

       ! Read satellite input file name and set the satellite name
       do iSat = 1, nSat
          call read_var('NameTrajectoryFile', NameFileSat_I(iSat))
          l1 = index(NameFileSat_I(iSat), '/', back=.true.) + 1
          l2 = index(NameFileSat_I(iSat), '.', back=.true.) - 1
          if(l1-1 <= 0) l1 = 1
          if(l2+1 <= 0) l2 = len_trim(NameFileSat_I(iSat))
          NameSat_I(iSat) = NameFileSat_I(iSat)(l1:l2)
       end do
    case default
       call CON_stop(NameSub//' unknown command='//NameCommand)
    end select

    call test_stop(NameSub, DoTest)
  end subroutine read_param
  !============================================================================
  subroutine init
    ! Initialize arrays for satellite triangulation

    !--------------------------------------------------------------------------
    if(.not.UseSatellite) RETURN

    if(allocated(Distribution_III))deallocate(Distribution_III)
    allocate(Distribution_III(0:nP+1,nMu,nSat))
  end subroutine init
  !============================================================================
  subroutine read_satellite_input_files

    use CON_axes, ONLY: transform_matrix
    use ModTimeConvert, ONLY: time_int_to_real
    use ModIoUnit, ONLY: UnitTmp_, STDOUT_
    use ModKind, ONLY: Real8_
    use SP_ModTime, ONLY: StartTime

    ! One line of input
    character(len=100) :: StringLine

    ! Local VARs
    integer            :: nPoint                ! count of location points
    integer            :: nPointFull            ! full count before trimming
    integer            :: MaxPoint              ! maximum location points
    integer            :: i, iSat               ! loop variables
    integer            :: iOffset               ! last point in which time<0
    integer            :: iTime_I(7)            ! time: YYYY/MM/DD-hh:mm:ss:ms
    real               :: Xyz_D(nDim)           ! location coordinates
    real(Real8_)       :: DateTime              ! date and time
    real, allocatable  :: Time_I(:), Xyz_DI(:,:)! time and location arrays
    character(len=100) :: NameFile              ! readin filenames
    character(len=3)   :: TypeCoordSat = 'HGI'  ! 'HGI' in TRAJECTORY files

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'read_satellite_input_files'
    !--------------------------------------------------------------------------
    call test_start(NameSub, DoTest)

    ! Count maximum number of points by reading all satellite files
    MaxPoint = 0
    if(iProc == 0) then
       SATELLITES1: do iSat = 1, nSat

          ! Open the trajectory files
          NameFile = NameFileSat_I(iSat)
          call open_file(file=NameFile, status="old")
          nPoint = 0

          ! Count the number of points
          READFILE1: do
             read(UnitTmp_, '(a)', iostat=iError) StringLine
             if (iError /= 0) EXIT READFILE1

             if(index(StringLine, '#START')>0) then
                READPOINTS1: do
                   read(UnitTmp_, *, iostat=iError) iTime_I, Xyz_D
                   if(iError /= 0) EXIT READFILE1
                   ! Add new point
                   nPoint = nPoint + 1
                end do READPOINTS1
             end if
          end do READFILE1

          call close_file
          MaxPoint = max(MaxPoint, nPoint)
       end do SATELLITES1
    end if

    ! Tell all processors the maximum number of points
    call MPI_BCAST(MaxPoint, 1, MPI_INTEGER, 0, iComm, iError)

    ! allocate arrays depending on number of points
    allocate(Time_I(MaxPoint), Xyz_DI(nDim, MaxPoint))
    allocate(XyzSat_DII(nDim, MaxPoint, nSat))
    allocate(TimeSat_II(MaxPoint, nSat))

    ! Allocate arrays to preserve the complete, untrimmed trajectory.
    ! These are used by steady-state connectivity sampling, which may
    ! require satellite positions before the simulation StartTime.
    allocate(XyzSatFull_DII(nDim, MaxPoint, nSat))
    allocate(TimeSatFull_II(MaxPoint, nSat))

    ! Read the trajectories
    SATELLITES: do iSat = 1, nSat
       ! Read file on the root processor
       if(iProc == 0) then

          NameFile = NameFileSat_I(iSat)
          if(lVerbose > 0) &
               write(STDOUT_, *) NameSub, " reading: ", trim(NameFile)

          call open_file(file=NameFile, status="old")
          nPoint  = 0
          iOffset = 0
          ! Read the file: read #COOR TypeCoord, #START and points
          READFILE: do
             read(UnitTmp_,'(a)', iostat=iError) StringLine
             if(iError /= 0) EXIT READFILE

             if(index(StringLine, '#COOR')>0) &
                  read(UnitTmp_,'(a)') TypeCoordSat

             if(index(StringLine, '#START')>0) then
                READPOINTS: do
                   read(UnitTmp_,*, iostat=iError) iTime_I, Xyz_D
                   if (iError /= 0) EXIT READFILE
                   ! Add new point
                   nPoint = nPoint + 1
                   ! Store coordinates
                   Xyz_DI(:, nPoint) = Xyz_D
                   ! Convert integer date/time to simulation time
                   call time_int_to_real(iTime_I, DateTime)
                   Time_I(nPoint) = DateTime - StartTime
                   if(Time_I(nPoint) < 0.0) iOffset = nPoint
                enddo READPOINTS
             end if
          end do READFILE

          call close_file
          if(DoTest) write(*,*) NameSub//': nPoint=', nPoint, &
               ' iOffset=', iOffset

          ! Transform the complete trajectory before trimming so that both the
          ! preserved full trajectory and the legacy trimmed trajectory use the
          ! simulation coordinate system.
          if(TypeCoordSystem /= TypeCoordSat) then
             do i = 1, nPoint
                Xyz_DI(:, i) = matmul(transform_matrix(Time_I(i), &
                     TypeCoordSat, TypeCoordSystem), Xyz_DI(:,i))
             end do
          end if

       end if

       ! Broadcast the complete trajectory to all processors before any
       ! trimming is performed.
       call MPI_BCAST(nPoint, 1, MPI_INTEGER, 0, iComm, iError)
       ! Save the original number of trajectory points.
       nPointFull = nPoint
       ! Broadcast the trim location along with the trajectory data.
       call MPI_BCAST(iOffset, 1, MPI_INTEGER, 0, iComm, iError)
       call MPI_BCAST(Time_I, nPointFull, MPI_REAL, 0, iComm, iError)
       call MPI_BCAST(Xyz_DI, nDim*nPointFull, MPI_REAL, 0, iComm, iError)

       ! Store the complete trajectory for routines that need access to the
       ! entire spacecraft ephemeris, including times before StartTime.
       nPointTrajFull_I(iSat) = nPointFull
       TimeSatFull_II(1:nPointFull, iSat) = Time_I(1:nPointFull)
       XyzSatFull_DII(:, 1:nPointFull, iSat) = Xyz_DI(:, 1:nPointFull)

       ! Preserve the legacy behavior by trimming the working copy of the
       ! trajectory. Keep only the last point before StartTime and all later
       ! points so existing time-dependent satellite tracking remains unchanged.
       nPoint = nPointFull
       if(iOffset > 1) then
          i = nPoint - iOffset + 1
          Xyz_DI(:, 1:i) = Xyz_DI(:, iOffset:nPoint)
          Time_I(1:i) = Time_I(iOffset:nPoint)
          nPoint = i
       end if

       nPointTraj_I(iSat) = nPoint

       ! Store trimmed time and positions for satellite iSat on all PE-s
       TimeSat_II(1:nPoint, iSat) = Time_I(1:nPoint)
       XyzSat_DII(:, 1:nPoint, iSat) = Xyz_DI(:, 1:nPoint)
       iPointCurrentSat_I(1:nSat) = 1

       if(DoTest) then
          nPoint = min(10, nPoint)
          write(*,*) NameSub,': tSat=', TimeSat_II(1:nPoint, iSat)
          write(*,*) NameSub,': xSat=', XyzSat_DII(1, 1:nPoint, iSat)
          write(*,*) NameSub,': ySat=', XyzSat_DII(2, 1:nPoint, iSat)
          write(*,*) NameSub,': zSat=', XyzSat_DII(3, 1:nPoint, iSat)
       end if
    end do SATELLITES

    deallocate(Time_I, Xyz_DI)
    call test_stop(NameSub, DoTest)
  end subroutine read_satellite_input_files
  !============================================================================
  subroutine set_satellite_positions(iSat)

    use ModNumConst, ONLY: cTiny
    use SP_ModTime, ONLY: tSimulation => SPTime

    integer, intent(in) :: iSat
    integer :: i, nPoint
    real    :: dTime

    logical :: DoTest
    character(len=*), parameter:: NameSub = 'set_satellite_positions'
    !--------------------------------------------------------------------------
    call test_start(NameSub, DoTest)

    nPoint = nPointTraj_I(iSat)
    if(nPoint > 0) then
       ! Get the index for current time and satellite
       i = iPointCurrentSat_I(iSat)
       if(DoTest) write(*,*) NameSub, ' nPoint, iPoint, TimeSim, TimeSat=', &
            nPoint, i, tSimulation, TimeSat_II(i, iSat)

       do while (i < nPoint .and. TimeSat_II(i, iSat) <= tSimulation)
          i = i + 1
       end do
       iPointCurrentSat_I(iSat) = i
       if(DoTest) write(*,*) NameSub, ' final iPoint=', i

       ! if the index is out of range: do not track this satellite;
       ! otherwise, make a linear interpolation in time for the location
       if( (i == nPoint .and. tSimulation > TimeSat_II(i, iSat))  &
            .or. i == 1 ) then
          DoTrackSatellite_I(iSat) = .false.
          XyzSat_DI(:, iSat) = 0.0
       else
          DoTrackSatellite_I(iSat) = .true.
          dTime = 1.0 - (TimeSat_II(i, iSat) - tSimulation) /     &
               max(TimeSat_II(i, iSat) - TimeSat_II(i-1, iSat), cTiny)
          XyzSat_DI(:, iSat) = dTime * XyzSat_DII(:, i, iSat) +   &
               (1.0 - dTime) * XyzSat_DII(:, i-1, iSat)
       end if

       if(DoTest) then
          write(*,*) NameSub, ' DoTrackSat =', DoTrackSatellite_I(iSat)
          write(*,*) NameSub, ' XyzSat     =', XyzSat_DI(:, iSat)
       end if
    end if

    call test_stop(NameSub, DoTest)
  end subroutine set_satellite_positions
  !============================================================================
  subroutine get_satellite_time_range(iSat, TimeStart, TimeEnd, DoTrack)

    ! Return the time interval covered by satellite iSat. Times are measured
    ! from the simulation StartTime, same as TimeSat_II and SPTime.

    integer, intent(in)  :: iSat        ! Satellite index
    real,    intent(out) :: TimeStart   ! Earliest available trajectory time
    real,    intent(out) :: TimeEnd     ! Latest available trajectory time
    logical, intent(out) :: DoTrack     ! True if a valid time range exists

    integer :: nPoint                   ! Number of trajectory samples

    character(len=*), parameter:: NameSub = 'get_satellite_time_range'
    !--------------------------------------------------------------------------

    ! Default outputs indicate that no valid trajectory was found.
    TimeStart = 0.0
    TimeEnd   = 0.0
    DoTrack   = .false.

    ! Reject invalid satellite indices.
    if(iSat < 1 .or. iSat > nSat) RETURN

    ! Prefer the preserved full trajectory whenever it is available.
    if(allocated(TimeSatFull_II)) then
       nPoint = nPointTrajFull_I(iSat)
       ! A valid trajectory must contain at least one sample.
       if(nPoint >= 1) then
          TimeStart = TimeSatFull_II(1,      iSat)
          TimeEnd   = TimeSatFull_II(nPoint, iSat)
          DoTrack   = TimeEnd >= TimeStart
          RETURN
       end if
    end if

    ! Fallback to trimmed trajectory.
    if(.not.allocated(TimeSat_II)) RETURN

    nPoint = nPointTraj_I(iSat)
    if(nPoint < 1) RETURN

    TimeStart = TimeSat_II(1,      iSat)
    TimeEnd   = TimeSat_II(nPoint, iSat)
    DoTrack   = TimeEnd >= TimeStart

  end subroutine get_satellite_time_range
  !============================================================================
  subroutine get_satellite_position_at_time(iSat, Time, Xyz_D, DoTrack)

    ! Return the interpolated position of satellite iSat at an arbitrary time.
    !
    ! Unlike the normal satellite tracking routines, this does NOT modify any
    ! global tracking state (such as SPTime or iPointCurrentSat_I). Therefore
    ! it can safely be called repeatedly when sampling field lines at many
    ! different times.
    !
    ! If available, the routine uses the preserved *full* satellite trajectory,
    ! which contains the complete ephemeris. Otherwise it falls back to the
    ! legacy trajectory, which may have been trimmed around StartTime.

    integer, intent(in)  :: iSat        ! Satellite index
    real,    intent(in)  :: Time        ! Requested interpolation time
    real,    intent(out) :: Xyz_D(nDim) ! Interpolated satellite position
    logical, intent(out) :: DoTrack     ! True if interpolation succeeded

    integer :: i, nPoint
    real    :: dTime, dTimeDenom
    ! Small tolerance for floating-point comparisons of times.
    real, parameter :: cTinyTime = 1.0e-6
    ! True if the preserved full trajectory exists.
    logical :: DoUseFull

    character(len=*), parameter:: NameSub = 'get_satellite_position_at_time'
    !--------------------------------------------------------------------------
    Xyz_D = 0.0
    DoTrack = .false.

    ! Reject invalid satellite indices.
    if(iSat < 1 .or. iSat > nSat) RETURN

    DoUseFull = allocated(TimeSatFull_II) .and. allocated(XyzSatFull_DII)
    if(DoUseFull) DoUseFull = nPointTrajFull_I(iSat) > 0

    if(DoUseFull) then
       ! Number of points in the preserved trajectory.
       nPoint = nPointTrajFull_I(iSat)

       !---------------------------------------------------------------
       ! Special case: only one trajectory point exists.
       ! Interpolation is impossible, so only allow an exact time match.
       !---------------------------------------------------------------
       if(nPoint == 1) then
          if(abs(Time - TimeSatFull_II(1,iSat)) <= cTinyTime) then
             Xyz_D = XyzSatFull_DII(:,1,iSat)
             DoTrack = .true.
          end if
          RETURN
       end if

       !---------------------------------------------------------------
       ! Requested time lies outside the trajectory.
       !---------------------------------------------------------------
       if(Time < TimeSatFull_II(1,iSat) - cTinyTime) RETURN
       if(Time > TimeSatFull_II(nPoint,iSat) + cTinyTime) RETURN

       !---------------------------------------------------------------
       ! Handle exact matches with the first or last trajectory point.
       ! This avoids unnecessary interpolation.
       !---------------------------------------------------------------
       if(abs(Time - TimeSatFull_II(1,iSat)) <= cTinyTime) then
          Xyz_D = XyzSatFull_DII(:,1,iSat)
          DoTrack = .true.
          RETURN
       end if

       if(abs(Time - TimeSatFull_II(nPoint,iSat)) <= cTinyTime) then
          Xyz_D = XyzSatFull_DII(:,nPoint,iSat)
          DoTrack = .true.
          RETURN
       end if

       !---------------------------------------------------------------
       ! Locate the first trajectory point whose time is greater than or
       ! equal to the requested time.
       ! After this loop:
       !   Time(i-1) <= Time <= Time(i)
       !---------------------------------------------------------------
       do i = 2, nPoint
          if(TimeSatFull_II(i,iSat) >= Time) EXIT
       end do
       if(i > nPoint) RETURN

       ! Time interval between the two surrounding samples.
       dTimeDenom = TimeSatFull_II(i,iSat) - TimeSatFull_II(i-1,iSat)
       ! Avoid division by nearly zero.
       if(abs(dTimeDenom) <= cTinyTime) RETURN

       !---------------------------------------------------------------
       ! Compute interpolation fraction:
       !   dTime = 0  -> lower sample
       !   dTime = 1  -> upper sample
       !---------------------------------------------------------------
       dTime = (Time - TimeSatFull_II(i-1,iSat))/dTimeDenom
       dTime = max(0.0, min(1.0, dTime))

       ! Linear interpolation between the two surrounding positions.
       Xyz_D = (1.0 - dTime)*XyzSatFull_DII(:,i-1,iSat) + &
            dTime*XyzSatFull_DII(:,i,iSat)
       DoTrack = .true.
       RETURN
    end if

    ! Fallback to trimmed trajectory.
    if(.not.allocated(TimeSat_II)) RETURN
    if(.not.allocated(XyzSat_DII)) RETURN

    nPoint = nPointTraj_I(iSat)
    if(nPoint < 1) RETURN

    if(nPoint == 1) then
       if(abs(Time - TimeSat_II(1,iSat)) <= cTinyTime) then
          Xyz_D = XyzSat_DII(:,1,iSat)
          DoTrack = .true.
       end if
       RETURN
    end if

    if(Time < TimeSat_II(1,iSat) - cTinyTime) RETURN
    if(Time > TimeSat_II(nPoint,iSat) + cTinyTime) RETURN

    if(abs(Time - TimeSat_II(1,iSat)) <= cTinyTime) then
       Xyz_D = XyzSat_DII(:,1,iSat)
       DoTrack = .true.
       RETURN
    end if

    if(abs(Time - TimeSat_II(nPoint,iSat)) <= cTinyTime) then
       Xyz_D = XyzSat_DII(:,nPoint,iSat)
       DoTrack = .true.
       RETURN
    end if

    do i = 2, nPoint
       if(TimeSat_II(i,iSat) >= Time) EXIT
    end do
    if(i > nPoint) RETURN

    dTimeDenom = TimeSat_II(i,iSat) - TimeSat_II(i-1,iSat)
    if(abs(dTimeDenom) <= cTinyTime) RETURN

    dTime = (Time - TimeSat_II(i-1,iSat))/dTimeDenom
    dTime = max(0.0, min(1.0, dTime))

    Xyz_D = (1.0 - dTime)*XyzSat_DII(:,i-1,iSat) + &
         dTime*XyzSat_DII(:,i,iSat)
    DoTrack = .true.

  end subroutine get_satellite_position_at_time
  !============================================================================
  subroutine write_satellite_file(IsFirstCall)

    use SP_ModTriangulate, ONLY:  &
         intersect_surf, build_trmesh, interpolate_trmesh
    use SP_ModChannel, ONLY: Flux0_, FluxMax_, distr_to_flux, &
         NameFluxChannel_I, NameFluxUnit_I
    use SP_ModGrid, ONLY: nLine, iLineAll0, X_, Z_, FootPoint_VB
    use ModCoordTransform, ONLY: xyz_to_rlonlat
    use ModIoUnit, ONLY: UnitTmp_
    use ModUtilities, ONLY: cTab

    logical, intent(in) :: IsFirstCall

    ! name of the output file
    character(len=100) :: NameFile
    character(len=400) :: String
    ! loop variables
    integer :: iSat, iLon, iLat, iFlux, iStencil, iLineAll
    ! Footpoint parameters
    real :: XyzFoot_D(3), Rfoot, LonFoot, LatFoot, LonLatFoot_I(2)

    character(len=*), parameter:: NameSub = 'write_satellite_file'
    !--------------------------------------------------------------------------
    if(IsFirstCall)then
       ! write headers to satellite files
       do iSat = 1, nSat
          call set_satellite_positions(iSat)
          ! if this is a test for the triangulation: NOT real SatelliteTraj
          if(DoTestTri) XyzSat_DI(:, iSat) = XyzLocTestTri_I
          ! If we can track the satellite: we do triangulation and interpolation
          ! Otherwise the outputs will be 0.0 but the simulations will not stop
          if(.not.(DoTrackSatellite_I(iSat) .or. DoTestTri))CYCLE
          ! Intersect multiple field lines with the sphere
          call intersect_surf(norm2(XyzSat_DI(:, iSat)))
          if(iProc==0)then
             call build_trmesh()
             ! Interpolate the values to the specific point(s)
             call interpolate_trmesh(XyzSat_DI(:, iSat),   &
                  iStencilOut_I=iStencil_I,   &
                  WeightOut_I=Weight_I)
             if(all(iStencil_I==-1))then
                if(DoTestTri) EXIT
                CYCLE
             end if
             NameFile = trim(NamePlotDir)//'satflux_'//&
                  trim(NameSat_I(iSat))//'.out'
             call open_file(file=NameFile, status='replace', &
                  NameCaller=NameSub)
             String = 'Data along the '//trim(NameSat_I(iSat))//&
                  ' trajectory:'//'[# # # # # s ms # #'
             do iFlux = Flux0_, FluxMax_
                String = trim(String)//' '//trim(NameFluxUnit_I(iFlux))
             end do
             String =trim(String)//' deg deg'
             write(UnitTmp_,'(a)')trim(String)//']'
             String = 'yyyy mm dd HH MM ss ms ilon ilat'
             do iFlux = Flux0_, FluxMax_
                String = trim(String)//' '//trim(NameFluxChannel_I(iFlux))
             end do
             String = trim(String)//' lonfoot latfoot'
             write(UnitTmp_,'(a)')trim(String)
             call close_file
             String = ''
             call get_lon_lat(String)
             read(String,*)iLon, iLat
             NameFile = trim(NamePlotDir)//'LONLAT.'//&
                  trim(NameSat_I(iSat))
             call open_file(file=NameFile, status='replace', &
                  NameCaller=NameSub)
             write(UnitTmp_,'(i3,a)') iLon, cTab//cTab//cTab//'iLon'
             write(UnitTmp_,'(i3,a)') iLat, cTab//cTab//cTab//'iLat'
             write(UnitTmp_,'(a)') '#END'
             call close_file
          end if
          if(DoTestTri) EXIT
       end do
       RETURN
    end if
    ! Save outputs for each satellite
    TRI_SATELLITE: do iSat = 1, nSat
       ! set and get the satellite location
       call set_satellite_positions(iSat)
       ! if this is a test for the triangulation: NOT real SatelliteTraj
       if(DoTestTri) XyzSat_DI(:, iSat) = XyzLocTestTri_I

       ! If we can track the satellite: we do triangulation and interpolation
       ! Otherwise the outputs will be 0.0 but the simulations will not stop
       if(.not.(DoTrackSatellite_I(iSat) .or. DoTestTri))CYCLE TRI_SATELLITE

       ! Intersect multiple field lines with the sphere
       call intersect_surf(norm2(XyzSat_DI(:, iSat)))
       if(iProc==0)then
          call build_trmesh()
          ! Interpolate the values to the specific point(s)
          call interpolate_trmesh(XyzSat_DI(:, iSat),       &
               DistrInterp_II = Distribution_III(:,:,iSat),&
               iStencilOut_I=iStencil_I,   &
               WeightOut_I=Weight_I)
       end if
       ! Save IsTriangleFound, iStencil and Weights
       call MPI_BCAST(iStencil_I,  3, MPI_INTEGER, 0, iComm, iError)
       ! If triangulation fails, iStencil is not assigned
       if(all(iStencil_I==-1))then
          DoTrackSatellite_I(iSat) = .false.
          if(DoTestTri) EXIT
          CYCLE
       end if
       call MPI_BCAST(Weight_I, 3, MPI_REAL, 0, iComm, iError)
       ! Calculate magnetic connectivity
       LonLatFoot_I = 0.0
       do iStencil = 1,3
          iLineAll = iStencil_I(iStencil)
          ! Check if the line is at the given processor
          if(iLineAll <= iLineAll0 .or. iLineAll > iLineAll0 + nLine) CYCLE
          XyzFoot_D = FootPoint_VB(X_:Z_, iLineAll-iLineAll0)
          call xyz_to_rlonlat(XyzFoot_D, Rfoot, LonFoot, LatFoot)
          LonLatFoot_I = LonLatFoot_I + [LonFoot, LatFoot]*Weight_I(iStencil)
       end do
       call MPI_reduce_real_array(LonLatFoot_I, 2, MPI_SUM, 0, iComm, iError)
       if(iProc==0)then
          NameFile = trim(NamePlotDir)//'satflux_'//&
                  trim(NameSat_I(iSat))//'.out'
          call open_file(file=NameFile, position='append', status='unknown', &
               NameCaller=NameSub)
          String=''
          call get_date_time_string(SPTime, String)
          call get_lon_lat(String(len_trim(String)+1:))
          call get_flux(iSat, String(len_trim(String)+1:))
          call get_mag_connectivity(LonLatFoot_I(1), LonLatFoot_I(2),&
               String(len_trim(String)+1:))
          write(UnitTmp_,'(a)') trim(String)
          call close_file
       end if
       if(DoTestTri) EXIT TRI_SATELLITE
    end do TRI_SATELLITE ! iSat
  contains
    !==========================================================================
    subroutine get_date_time_string(Time, StringTime)

      use ModTimeConvert, ONLY: time_real_to_int
      ! the subroutine converts real variable Time into a string,
      ! the structure of the string is 'ddhhmmss',
      ! i.e shows number of days, hours, minutes and seconds
      ! after the beginning of the simulation
      real,              intent(in) :: Time
      character(len=24), intent(out):: StringTime
      integer :: iTime_I(7)
      !------------------------------------------------------------------------
      call time_real_to_int(StartTime+Time, iTime_I)
      write(StringTime,'(i4.4,a,i2.2,a,i2.2,a,i2.2,a,i2.2,a,i2.2,a,i3.3,a)')&
           iTime_I(1),' ',& ! Year
           iTime_I(2),' ',& ! Month
           iTime_I(3),' ',& ! Day
           iTime_I(4),' ',& ! Hour
           iTime_I(5),' ',& ! Minute
           iTime_I(6),' ',& ! Second
           iTime_I(7),' '   ! Millisecond

    end subroutine get_date_time_string
    !==========================================================================
    subroutine get_lon_lat(StringLonLat)

      use SP_ModGrid, ONLY: ilineall_to_lon_lat
      character(len=9), intent(out):: StringLonLat

      integer :: iLon, iLat, iLineAll, iLoc
      !------------------------------------------------------------------------

      iLoc = maxloc(Weight_I, DIM=1)
      iLineAll = iStencil_I(iLoc)
      call ilineall_to_lon_lat(iLineAll, iLon, iLat)
      write(StringLonLat,'(a,i3.3,a,i3.3,a)')' ', iLon, ' ', iLat, ' '
    end subroutine get_lon_lat
    !==========================================================================
    subroutine get_flux(iSat, StringFlux)

      integer, intent(in) :: iSat
      character(len=250), intent(out) :: StringFlux
      real :: Flux_V(Flux0_:FluxMax_)
      integer :: i
      !------------------------------------------------------------------------

      call distr_to_flux(Distribution_III(1:nP, :, iSat),&
           Flux_V)
      write(StringFlux,'(a,20(es12.4,1X))')' ', &
           (Flux_V(i), i=Flux0_,FluxMax_)
    end subroutine get_flux
    !==========================================================================
    subroutine get_mag_connectivity(LonFoot, LatFoot, StringFoot)

      use ModNumConst, ONLY: cRadToDeg
      real, intent(in) :: LonFoot, LatFoot
      character(len=13), intent(out) :: StringFoot
      !------------------------------------------------------------------------

      write(StringFoot,'(a,2(F5.1,1X))')' ', &
           LonFoot*cRadToDeg, LatFoot*cRadToDeg
    end subroutine get_mag_connectivity
    !==========================================================================
  end subroutine write_satellite_file
  !============================================================================
end module SP_ModSatellite
!==============================================================================
