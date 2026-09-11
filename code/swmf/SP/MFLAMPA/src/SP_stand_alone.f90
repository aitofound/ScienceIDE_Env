!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
program MFLAMPA

  use ModKind, ONLY: Real8_
  use SP_ModProc, ONLY: iComm, nProc, iProc, iError
  use ModUtilities, ONLY: remove_file, touch_file
  use SP_ModTime, ONLY: iIter, init_time  => init
  use SP_ModPlot, ONLY:        init_plot  => init
  use SP_ModTiming, ONLY: nTiming
  use SP_ModMain, ONLY: IsLastRead,     &
       IsStandAlone, TimeMax, nIterMax,   &
       SP_read_param => read_param,       &
       SP_check      => check,            &
       SP_initialize => initialize,       &
       SP_run        => run,              &
       SP_finalize   => finalize
  use SP_ModGrid, ONLY: init_stand_alone, init_grid => init
  use ModReadParam, ONLY: read_file, read_init
  use CON_planet, ONLY: init_planet_const, set_planet_defaults
  use ModMpi

  implicit none

  integer      :: iSession = 1
  real(Real8_) :: CpuTimeStart
  logical      :: IsFirstSession = .true.

  ! Initialization of MPI/parallel message passing.
  !----------------------------------------------------------------------------
  call MPI_INIT(iError)
  iComm = MPI_COMM_WORLD
  call MPI_COMM_RANK(iComm, iProc, iError)
  call MPI_COMM_SIZE(iComm, nProc, iError)

  ! Initialize time which is used to check CPU time
  CpuTimeStart = MPI_WTIME()

  ! Delete MFLAMPA.SUCCESS and MFLAMPA.STOP files if found
  if(iProc==0) then
     call remove_file('MFLAMPA.SUCCESS')
     call remove_file('MFLAMPA.STOP')
  end if
  ! Initialize the planetary constant library and set Earth
  ! as the default planet.
  call init_planet_const
  call set_planet_defaults
  ! Mark the run as a stand alone
  IsStandAlone = .true.

  ! Read PARAM.in file. Provide default restart file for #RESTART
  call read_file('PARAM.in', iComm)

  SESSIONLOOP: do
     call read_init('  ', iSessionIn=iSession)

     if(iProc==0)&
          write(*,*) '----- Starting Session ',iSession,' ------'

     ! Set and check input parameters for this session
     call SP_read_param ! Identical to SP_set_param('READ')
     call SP_check      ! Similar to SP_set_param('CHECK'), but see init_time

     if(IsFirstSession) then
        ! Time execution (timing parameters set by SP_read_param)
        call timing_start('MFLAMPA')
        call timing_start('setup')
        call init_grid        ! Similar to SP_set_param('GRID')
        call init_stand_alone ! Distinctions from SWMF (CON_bline) version
        call SP_initialize    ! Similar to SP_init_session, NO origin points
        call init_time        ! StartTime, StartTimeJulian from StartTime_I
        ! DataInputTime=0, SPTime=0 unless set in PARAM.in or restart.H
        ! In SWMF, SPTime is set in SP_set_param('CHECK'), DataInput time is
        ! set either in coupling or in reading the MHD data from file.
        call timing_stop('setup')
        if(nTiming > -3) call timing_report_total
        if(iProc==0) &
             write(*,*) 'Resetting timing counters after setup.'
        call timing_reset('#all', 3)
     else
        call init_plot
     end if

     TIMELOOP: do
        if(stop_condition_true()) EXIT TIMELOOP
        if(is_time_to_stop()) EXIT SESSIONLOOP
        call timing_step(iIter + 1)

        if(TimeMax > 0.0) then
           call SP_run(TimeMax)
        else
           call SP_run(huge(0.0))
        end if

        call show_progress
     end do TIMELOOP

     if(IsLastRead) EXIT SESSIONLOOP
     if(iProc==0) &
          write(*,*) '----- End of Session   ',iSession,' ------'
     iSession = iSession + 1
     IsFirstSession = .false.
     if(nTiming > -2) call timing_report
     call timing_reset_all
  end do SESSIONLOOP

  if(iProc==0) then
     write(*,*)
     write(*,'(a)') '    Finished Numerical Simulation'
     write(*,'(a)') '    -----------------------------'
  end if
  if(nTiming > -2) call timing_report
  call timing_stop('MFLAMPA')
  if(nTiming > -3) call timing_report_total

  ! Finish writing to log file
  call SP_finalize

  ! Touch MFLAMPA.SUCCESS
  if(iProc==0) call touch_file('MFLAMPA.SUCCESS')

  ! Finalize MPI
  call MPI_FINALIZE(iError)
contains
  !============================================================================
  function stop_condition_true() result(IsStopCondition)

    use SP_ModMain, ONLY: nIterMax
    use SP_ModTime, ONLY: SPTime
    logical :: IsStopCondition
    !--------------------------------------------------------------------------
    IsStopCondition = .false.

    if(nIterMax >= 0   .and.  iIter >=nIterMax) IsStopCondition = .true.
    if( TimeMax >  0.0 .and. SPTime >= TimeMax) IsStopCondition = .true.

  end function stop_condition_true
  !============================================================================
  function is_time_to_stop() result(IsTimeToStop)

    use SP_ModMain, ONLY: CpuTimeMax, UseStopFile
    logical :: IsTimeToStop
    !--------------------------------------------------------------------------
    IsTimeToStop = .false.

    if(iProc==0) then
       if(CpuTimeMax > 0.0 .and. MPI_WTIME()-CpuTimeStart >= CpuTimeMax) then
          write(*,*) 'CPU time exceeded:', CpuTimeMax, MPI_WTIME()-CpuTimeStart
          IsTimeToStop = .true.
       end if
       if(.not.IsTimeToStop .and. UseStopFile) then
          inquire(file='MFLAMPA.STOP',exist=IsTimeToStop)
          if(IsTimeToStop) &
               write(*,*) 'MFLAMPA.STOP file exists: received stop signal'
       end if
    end if
    if(nProc==1) RETURN
    call MPI_BCAST(IsTimeToStop, 1, MPI_LOGICAL, 0, iComm, iError)

  end function is_time_to_stop
  !============================================================================
  subroutine show_progress

    use SP_ModTiming, ONLY: UseTiming
    real(Real8_), external :: timing_func_d
    real(Real8_) :: CpuTimeMFLAMPA, CpuTimeAdvance
    integer:: nProgress1 = 0, nProgress2 = 10
    !--------------------------------------------------------------------------

    ! Show timing results if required
    ! Show speed as cells/second/PE/step
    if( UseTiming .and. iProc==0 &
         .and. nProgress1>0 .and. mod(iIter,nProgress1) == 0 ) then
       CpuTimeMFLAMPA = timing_func_d('sum',1,'MFLAMPA','MFLAMPA')
       CpuTimeAdvance = timing_func_d('sum',1,'advance','MFLAMPA')
       ! placeholder
    end if

    ! Show timing tables
    if(nTiming>0 .and. mod(iIter,nTiming)==0) then
       call timing_report
    else if(nProgress2>0 .and. mod(iIter,nProgress2)==0) then
       call timing_tree(2,2)
    end if

  end subroutine show_progress
  !============================================================================

end program MFLAMPA
!==============================================================================
