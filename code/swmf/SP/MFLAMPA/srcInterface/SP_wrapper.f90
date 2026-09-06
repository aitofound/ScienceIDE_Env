!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_wrapper

  use CON_coupler, ONLY: SP_, CON_stop
  use SP_ModMain,  ONLY: run, DoRestart, DoReadMhData
  use SP_ModTime,  ONLY: DataInputTime, SPTime, IsSteadyState, iIter
  use SP_ModProc,  ONLY: iProc, iError

  implicit none

  SAVE
  private ! except

  public:: SP_set_param
  public:: SP_init_session
  public:: SP_run
  public:: SP_save_restart
  public:: SP_finalize

  ! coupling with MHD components
  public:: SP_do_extract_lines
  public:: SP_put_coupling_param
  public:: SP_adjust_lines

  logical :: DoCheck = .true., DoSetGrid = .true.

contains
  !============================================================================
  subroutine SP_do_extract_lines(DoExtract)
    ! Interface routine to be called from super-structure on all PEs

    use CON_coupler, ONLY: i_proc0, i_comm, is_proc0
    use ModMpi
    logical, intent(out):: DoExtract

    character(len=*), parameter:: NameSub = 'SP_do_extract_lines'
    !--------------------------------------------------------------------------
    if(is_proc0(SP_)) DoExtract = .not.DoRestart
    ! when restarting, line data is available, i.e. ready to couple with mh;
    ! get value at SP root and broadcast to all SWMF processors
    call MPI_Bcast(DoExtract, 1, MPI_LOGICAL, i_proc0(SP_), i_comm(), iError)
  end subroutine SP_do_extract_lines
  !============================================================================
  subroutine SP_set_param(CompInfo,TypeAction)

    ! Interface routine to be called from super-structure on all PEs

    use CON_comp_info
    use SP_ModTime,  ONLY: StartTimeJulian, StartTime, time_real_to_julian, &
         iStartTime_I
    use SP_ModMain,  ONLY: check, read_param
    use SP_ModGrid,  ONLY: TypeCoordSystem
    use CON_coupler, ONLY: is_proc
    use SP_ModProc
    use SP_ModUnit,  ONLY: Si2Io_V, Io2Si_V, UnitX_, UnitEnergy_
    use CON_physics, ONLY: get_time
    use CON_bline,   ONLY: BL_set_grid
    use ModTimeConvert, ONLY: time_real_to_int
    type(CompInfoType),intent(inout):: CompInfo
    character(len=*),  intent(in)   :: TypeAction
    real    :: UnitX, EnergyCoeff
    logical :: DoTimeAccurate = .true.

    character(len=*), parameter:: NameSub = 'SP_set_param'
    !--------------------------------------------------------------------------
    select case(TypeAction)
    case('VERSION')
       call put(CompInfo,&
            Use        =.true., &
            NameVersion='MFLAMPA', &
            Version    =0.90)
    case('MPI')
       call get(CompInfo, iComm=iComm, iProc=iProc, nProc=nProc)
    case('STDOUT')
       ! placeholder
    case('CHECK')
       if(.not.DoCheck)RETURN
       DoCheck = .false.
       if(iProc==0)then
          write(*,'(a)')'SP:'
          write(*,'(a)')'SP:  check parameters'
          write(*,'(a)')'SP:'
       end if
       call get_time(tSimulationOut = SPTime, tStartOut = StartTime)
       call time_real_to_julian(StartTime, StartTimeJulian)
       call time_real_to_int(StartTime, iStartTime_I)
       DataInputTime = SPTime
       call check
    case('READ')
       call get_time(DoTimeAccurateOut=DoTimeAccurate)
       IsSteadyState = .not. DoTimeAccurate
       if(iProc==0)write(*,*)'SP: IsSteadyState=',IsSteadyState
       call read_param
    case('GRID')
       if(.not.DoSetGrid)RETURN
       DoSetGrid = .false.
       if(is_proc(SP_))then
          UnitX = Io2Si_V(UnitX_)
          EnergyCoeff = Si2Io_V(UnitEnergy_)
          if(iProc==0)then
             write(*,'(a)')'SP:'
             write(*,'(a)')'SP:  set grid'
             write(*,'(a)')'SP:'
          end if
       end if
       call BL_set_grid(TypeCoordSystem, UnitX, EnergyCoeff)
    case default
       call CON_stop('Cannot call SP_set_param for '//trim(TypeAction))
    end select
  end subroutine SP_set_param
  !============================================================================
  subroutine SP_init_session(iSession,TimeSimulation)

    ! Above two routines are be called from superstructure  on all PEs
    ! The following routines are called  on  PEs of the SP model only

    use SP_ModMain,         ONLY: initialize, DoRestart, DoReadMhData
    use SP_ModGrid,         ONLY: init_grid=>init, nVar, nLon, nLat, &
         State_VIB, MHData_VIB, nVertex_B, FootPoint_VB, Used_B
    use SP_ModOriginPoints, ONLY: ROrigin, LonMin, LonMax, LatMin, LatMax
    use SP_ModSize,         ONLY: nVertexMax
    use SP_ModPlot,          ONLY: init_plot => init
    use CON_bline,          ONLY: BL_init, BL_get_origin_points
    use SP_ModProc,         ONLY: iProc
    integer,  intent(in) :: iSession         ! session number (starting from 1)
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    logical, save:: IsInitialized = .false.
    !--------------------------------------------------------------------------
    if(IsInitialized)then
       call init_plot
       RETURN
    end if
    IsInitialized = .true.

    call init_grid
    if(iProc==0)then
       write(*,'(a)')'BL:'
       write(*,'(a,i2)')'BL: init   session=', iSession
       write(*,'(a)')'BL:'
    end if

    nullify(MHData_VIB) ;  nullify(State_VIB)
    nullify(nVertex_B);  nullify(FootPoint_VB)
    call BL_init(nVertexMax, nLon, nLat,  &
         MHData_VIB, nVertex_B, nVar, State_VIB, FootPoint_VB, Used_B)
    call initialize

    if(.not.(DoRestart.or.DoReadMhData))&
         call BL_get_origin_points(ROrigin, LonMin, LonMax, LatMin, LatMax)
  end subroutine SP_init_session
  !============================================================================
  subroutine SP_run(TimeSimulation, TimeSimulationLimit)
    real,intent(inout) :: TimeSimulation
    real,intent(in)    :: TimeSimulationLimit

    !--------------------------------------------------------------------------
    if(iProc==0)write(*,'(a,es12.5,a,es12.5,a,i6)')'SP:'//      &
         'Call run from SP_run, DataInputTime=', DataInputTime, &
         ' SPTime=', SPTime, ' nStep=', iIter
    if(.not.DoReadMhData)call check_mhdata
    call run(TimeSimulationLimit)
    if(DoReadMhData)then
       TimeSimulation = DataInputTime
    elseif(.not.IsSteadyState)then
       TimeSimulation = TimeSimulationLimit
    end if
  contains
    subroutine check_mhdata
      use CON_bline, only: save_mhd
      use SP_ModGrid, only: Rho_, MhData_VIB, Used_B, nVertex_B, nLine
      integer :: iLine, iVertex
      !------------------------------------------------------------------------
      do iLine = 1, nline
         if(.not.Used_B(iLine))CYCLE
         if(any(MhData_VIB(Rho_,1:nVertex_B(iLine),iLine)<=0.0))then
            call save_mhd(DataInputTime, iLine, 'Zero or negative density')
            Used_B(iLine) = .false.
         end if
      end do
    end subroutine check_mhdata
  end subroutine SP_run
  !============================================================================
  subroutine SP_finalize(TimeSimulation)
    use SP_ModMain, ONLY: finalize
    real,intent(in) :: TimeSimulation
    ! if data are read from files, no special finalization is needed

    !--------------------------------------------------------------------------
    if(.not.(DoReadMhData.or.IsSteadyState))then
       if(iProc==0)write(*,'(a,es12.5,a,es12.5)')'SP:'//                &
            'Call run from SP_finalize, DataInputTime=', DataInputTime, &
            ' SPTime=', SPTime
       call run(TimeSimulation)
    end if
    if(iProc==0)write(*,'(a,es12.5,a,i8)')'SP:'//  &
         'Call SP_finalize, Time=', DataInputTime, &
         ' nStep=', iIter
    call finalize
  end subroutine SP_finalize
  !============================================================================
  subroutine SP_save_restart(TimeSimulation)
    use SP_ModRestart, ONLY: save_restart
    real, intent(in) :: TimeSimulation
    ! if data are read from files, no need for additional run

    !--------------------------------------------------------------------------
    if(.not.(DoReadMhData.or.IsSteadyState))then
       if(iProc==0)write(*,'(a,es12.5,a,es12.5)')'SP:'//                    &
            'Call run from SP_save_restart, DataInputTime=', DataInputTime, &
            ' SPTime=', SPTime
       call run(TimeSimulation)
    end if
    if(iProc==0)write(*,'(a,es12.5,a,i6)')'SP:'//       &
         'Call SP_save_restart, Time=', TimeSimulation, &
         ' nStep=', iIter
    call save_restart
  end subroutine SP_save_restart
  !============================================================================
  subroutine SP_put_coupling_param(Source_, TimeIn)
    use SP_ModGrid, ONLY: copy_old_state
    use CON_bline,  ONLY: Lower_
    integer, intent(in) :: Source_
    real,    intent(in) :: TimeIn
    !--------------------------------------------------------------------------
    if(DataInputTime >= TimeIn)RETURN

    ! New coupling time, get it and save old state
    DataInputTime = TimeIn
    if(Source_==Lower_)then
       call copy_old_state
    else
       call CON_stop("Time in IHSP coupling differs from that in SCSP")
    end if
  end subroutine SP_put_coupling_param
  !============================================================================
  subroutine SP_adjust_lines(Source_)

    ! Called from coupler after the updated grid point lo<cation are
    ! received from the other component (SC, IH). Determines whether some
    ! grid points should be added/deleted

    use SP_ModDistribution, ONLY: offset
    use SP_ModGrid,         ONLY: Used_B
    use CON_bline,          ONLY: &
         iOffset_B, BL_adjust_lines,  Lower_, Upper_, nLine
    integer, intent(in) :: Source_
    integer :: iLine  ! loop variable

    character(len=*), parameter:: NameSub = 'SP_adjust_lines'
    !--------------------------------------------------------------------------
    call BL_adjust_lines(Source_)
    if(Source_ == Lower_)then
       do iLine = 1, nLine
          if(.not.Used_B(iLine))CYCLE
          ! Offset the distribution function array, if needed
          call offset(iLine, iOffset=iOffset_B(iLine))
       end do
    end if
    ! Called after the grid points are received from the
    ! component, nullify offset
    if(Source_ == Upper_) iOffset_B(1:nLine) = 0
  end subroutine SP_adjust_lines
  !============================================================================
end module SP_wrapper
!==============================================================================
