!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
program PIC
  use ModKind
  use PIC_ModProc,  ONLY: iProc, nProc, iComm, iError
  use ModUtilities, ONLY: remove_file, touch_file
  use PIC_ModMain,  ONLY: nTiming, iStep, tMax, IsLastRead, &
       tSimulation, IsFirstSession 
  use ModReadParam
  use ModMpi
  
  implicit none

  integer:: iSession = 1  
  real(Real8_) :: CpuTimeStart

  !------------------------------------------------
  !\                                                                        
  ! Initialization of MPI/parallel message passing.                        
  !/ 
  call MPI_INIT(iError)
  iComm=MPI_COMM_WORLD
  call MPI_COMM_RANK(iComm, iProc, iError)
  call MPI_COMM_SIZE(iComm, nProc, iError)

  !\
  ! Initialize time which is used to check CPU time
  !/
  CpuTimeStart = MPI_WTIME()

  !\
  ! Delete PIC.SUCCESS and PIC.STOP files if found
  !/
  if(iProc==0)then
     call remove_file('PIC.SUCCESS')
     call remove_file('PIC.STOP')
  end if
  
  !\
  ! Read input parameter file. 
  !/
  call read_file('PARAM.in',iComm)
 
  SESSIONLOOP: do
     call read_init('  ', iSessionIn=iSession)

     if(iProc==0)&
         write(*,*)'----- Starting Session ',iSession,' ------'
     !\
     ! Set and check input parameters for this session
     !/
     call PIC_set_param('READ')
     call PIC_set_param('CHECK')
     !\
     ! Time execution (timing parameters were set by MH_set_parameters)
     !/
     if(IsFirstSession)then
        call timing_start('PIC')
        call timing_start('setup')
     end if
     call PIC_init_session(iSession)
     if(IsFirstSession)then
        call timing_stop('setup')
        if(nTiming > -3) call timing_report_total
        if(iProc==0) write(*,*)'Resetting timing counters after setup.'
        call timing_reset('#all',3)
     end if

     TIMELOOP: do
        if(stop_condition_true())exit TIMELOOP
        if(is_time_to_stop())exit SESSIONLOOP
        call timing_step(iStep + 1)
        
        if(tMax > 0.0)then
           call PIC_advance(tMax)
        else
           call PIC_advance(huge(0.0))
        end if

        call show_progress
     end do TIMELOOP

     if(IsLastRead)exit SESSIONLOOP
     if(iProc==0) &
          write(*,*)'----- End of Session   ',iSession,' ------'   
     iSession=iSession+1
     IsFirstSession = .false.
     if (nTiming > -2) call timing_report
     call timing_reset_all
  end do SESSIONLOOP

  if(iProc==0)then
     write(*,*)
     write(*,'(a)')'    Finished Numerical Simulation'
     write(*,'(a)')'    -----------------------------'
  end if

  if (nTiming > -2) call timing_report

 

  call timing_stop('PIC')

  if(nTiming > -3)call timing_report_total

  !Finish writing to log file
  call PIC_finalize

  !\
  ! Touch PIC.SUCCESS
  !/
  if(iProc==0) call touch_file('PIC.SUCCESS')

  !Finalize MPI
  call MPI_Finalize(iError)
  
contains
  function stop_condition_true() result(IsStopCondition)
    use PIC_ModMain, ONLY: nIter 
    logical :: IsStopCondition
    !--------------------------
    IsStopCondition = .false.

    if(nIter >= 0 .and. iStep >= nIter) IsStopCondition = .true.
    if(tMax > 0.0 .and. tSimulation >= tMax) IsStopCondition = .true.

  end function stop_condition_true
  !===============================
  function is_time_to_stop() result(IsTimeToStop)
    use PIC_ModMain, ONLY: CpuTimeMax, UseStopFile
    logical :: IsTimeToStop
    !---------------------
    IsTimeToStop = .false.

    if(iProc==0)then
       if(CpuTimeMax > 0.0 .and. MPI_WTIME()-CpuTimeStart >= CpuTimeMax)then
          write(*,*)'CPU time exceeded:',CpuTimeMax,MPI_WTIME()-CpuTimeStart
          IsTimeToStop=.true.
       end if
       if(.not.IsTimeToStop .and. UseStopFile) then
          inquire(file='PIC.STOP',exist=IsTimeToStop)
          if (IsTimeToStop) &
               write(*,*)'PIC.STOP file exists: receieved stop signal'
       end if
    end if
    if(nProc==1)return
    call MPI_BCAST(IsTimeToStop,1,MPI_LOGICAL,0,iComm,iError)

  end function is_time_to_stop
  !===================================================================
  subroutine show_progress
    use PIC_ModMain,      ONLY: nProgress1, nProgress2, UseTiming
    use PIC_ModParticles, ONLY: Particle_I, nPType
    use PC_ModPhysics,   ONLY: No2Io_V, UnitT_
    real(Real8_), external :: timing_func_d
    real(Real8_) :: CpuTimePIC,CpuTimeAdvance
    integer:: iSumNP, iSort
    !------------------------------------------------
    !\
    ! Show timing results if required
    !/

    ! Show speed as cells/second/PE/step
    if( UseTiming .and. iProc==0 &
         .and. nProgress1>0 .and. mod(iStep,nProgress1) == 0 ) then
       CpuTimePIC = timing_func_d('sum',1,'PIC','PIC')
       CpuTimeAdvance=timing_func_d('sum',1,'advance','PIC')
       iSumNP = 0
       do iSort = 1,nPType
          iSumNP = iSumNP + Particle_I(iSort)%nParticle
       end do
       write(*,'(a,f11.1,a,f9.1,a,i8,a,1p,e10.4,a)') 'Speed is',&
            iSumNP &
            /max(1.D-10,CpuTimeAdvance),&
            ' p/s/pe after',&
            CpuTimePIC,&
            ' s at N =',iStep, ' (', tSimulation*No2Io_V(UnitT_),')'
    end if

    ! Show timing tables
    if(nTiming>0.and.mod(iStep,nTiming)==0) then
       call timing_report
    else if(nProgress2>0.and.mod(iStep,nProgress2) == 0) then
       call timing_tree(2,2)
    end if

  end subroutine show_progress

end program PIC
