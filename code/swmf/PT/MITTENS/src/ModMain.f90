!  Copyright (C) 2002 Regents of the University of Michigan
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModMain

  use PT_ModProc, ONLY: iProc
  use PT_ModReadMhData, ONLY: DoReadMhData
  use ModUtilities, ONLY: CON_stop

  implicit none

  SAVE
  private ! except

  ! Indicator of stand alone mode
  logical:: IsStandAlone = .false.

  ! Stopping conditions. These variables are only used in stand alone mode.
  real    :: TimeMax     = -1.0, CpuTimeMax = -1.0
  integer :: nIterMax    = -1
  logical :: UseStopFile = .true.
  logical :: IsLastRead  = .false.

  ! Logicals for actions
  !----------------------
  ! run test analytical fieldline
  logical :: DoRunTest = .false.
  ! run the component
  logical :: DoRun = .true.
  ! restart the run
  logical :: DoRestart = .false.
  ! Methods and variables from ModReadMhData
  public  :: DoReadMhData

  ! Methods and variables from this module
  public:: read_param, initialize, finalize, run, check, DoRestart,          &
       IsLastRead, UseStopFile, CpuTimeMax, TimeMax, nIterMax, IsStandAlone, &
       DoRunTest

contains
  !============================================================================
   subroutine read_param
      use PT_ModGrid, ONLY: read_param_grid       => read_param
      use PT_ModOriginPoints, ONLY: read_param_origin     => read_param
      use PT_ModReadMHData, ONLY: read_param_mhdata     => read_param
      use PT_ModTime, ONLY: read_param_time       => read_param
      use PT_ModDistribution, ONLY: read_param_distribution => read_param
      use PT_ModRandom, ONLY: read_param_random     => read_param
      use PT_ModParticle, ONLY: read_param_particle   => read_param
      use PT_ModSolver, ONLY: read_param_solver     => read_param
      use PT_ModShock, ONLY: read_param_shock      => read_param
      use PT_ModFieldline, ONLY: read_param_fieldline
      use PT_ModPlot, ONLY: read_param_plot       => read_param

      ! Read input parameters for PT component
      use ModReadParam, ONLY: &
            read_var, read_line, read_command, i_session_read, read_echo_set

      ! The name of the command
      character (len=100) :: NameCommand
      ! Read the corresponding section of input file
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
      do
         if(.not.read_line() ) then
            IsLastRead = .true.
            EXIT
         end if
         if(.not.read_command(NameCommand)) CYCLE
         select case(NameCommand)
         case('#RESTART')
            call read_var('DoRestart', DoRestart)
            ! read parameters for each module
         case('#ORIGIN')
            if(IsStandAlone) CYCLE
            call read_param_origin
         case('#COORDSYSTEM', '#COORDINATESYSTEM','#FIELDLINEGRID')
            if(i_session_read() /= 1) CYCLE
            call read_param_grid(NameCommand)
         case('#READMHDATA','#MHDATA','#APPENDMHDATA')
            call read_param_mhdata(NameCommand)
         case('#SHOCK')
            call read_param_shock(NameCommand)
         case('#DORUN')
            call read_var('DoRun', DoRun)
         case('#TEST')
            call check_stand_alone
            call read_var('DoRunTest', DoRunTest)
         case('#END')
            call check_stand_alone
            IsLastRead=.true.
            EXIT
         case('#RUN')
            call check_stand_alone
            IsLastRead=.false.
            EXIT
         case('#STOP')
            call check_stand_alone
            call read_var('nIterMax', nIterMax)
            call read_var('TimeMax' , TimeMax)
         case('#CPUTIMEMAX')
            call check_stand_alone
            call read_var('CpuTimeMax', CpuTimeMax)
         case('#CHECKSTOPFILE')
            call check_stand_alone
            call read_var('UseStopFile',UseStopFile)
         case("#STARTTIME",'#NSTEP','#TIMESIMULATION')
            if(i_session_read() /= 1)CYCLE
            call read_param_time(NameCommand)
         case("#SETREALTIME")
            if(i_session_read() /= 1)CYCLE
            call check_stand_alone
            call read_param_time(NameCommand)
         case("#TIMEACCURATE")
            call check_stand_alone
            call read_param_time(NameCommand)
         case("#DISTRIBUTION", "#SDEBINNING")
            call read_param_distribution(NameCommand)
         case("#PARTICLE")
            call read_param_particle(NameCommand)
         case("#INPUTSEED")
            call read_param_random(NameCommand)
         case("#SDE")
            call read_param_solver(NameCommand)
         case("#DIFFUSION", "#BOUNDARY", "#ADVECTION", "#DISTANCETIMESTEP")
            call read_param_fieldline(NameCommand)
         case("#SAVEOUTPUT")
            call read_param_plot(NameCommand)
         case default
            call CON_stop(NameSub//': Unknown command '//NameCommand)
         end select
      end do
   contains
    !==========================================================================
      subroutine check_stand_alone

         ! certain options are only available for stand alone mode;
         ! check whether the mode is used and stop the code if it's no the case
      !------------------------------------------------------------------------
         if(IsStandAlone)RETURN
         call CON_stop(NameSub//': command '//trim(NameCommand)//&
            ' is only allowed in stand alone mode, correct PARAM.in')
      end subroutine check_stand_alone
    !==========================================================================
   end subroutine read_param
  !============================================================================
   subroutine initialize
      use PT_ModReadMhData, ONLY: init_mhdata       => init
      use PT_ModRandom, ONLY: init_seed         => init
      use PT_ModDistribution, ONLY: init_distribution => init
      use PT_ModParticle, ONLY: init_particle     => init
      use PT_ModShock, ONLY: init_shock        => init
      use PT_ModPlot, ONLY: init_plot         => init

      ! initialize the model
    character(len=*), parameter:: NameSub = 'initialize'
    !--------------------------------------------------------------------------
      if(iProc == 0)then
         write(*,'(a)')'PT: initializing'
      end if

      ! Reads in first data file if reading in files, else nothing
      call init_mhdata
      call init_shock ! Init shock arrays

      ! Sets random seeds for all processors
      call init_seed
      ! Initializes phase space bins and counts array
      call init_distribution
      call init_plot
      ! Init particle array and split grid
      call init_particle

      if(iProc == 0) then
         write(*,'(a)')'PT: initialized'
      end if

   end subroutine initialize
  !============================================================================
   subroutine finalize
      use PT_ModReadMhData, ONLY: finalize_mhdata    => finalize
      ! finalize the model
      ! if(IsStandAlone)call stand_alone_final_restart

    character(len=*), parameter:: NameSub = 'finalize'
    !--------------------------------------------------------------------------
      call finalize_mhdata
   end subroutine finalize
  !============================================================================
   subroutine run

      use PT_ModGrid, ONLY: get_other_state_var, copy_old_state, Used_B
      use PT_ModShock, ONLY: get_shock_location, get_dLogRho, &
                                     get_test_shock, sharpen_shock, DoSharpenShock

      use PT_ModReadMhData, ONLY: read_mh_data
      use PT_ModTime, ONLY: PTTime, DataInputTime, iIter
      use PT_ModAdvance, ONLY: advance
      use PT_ModPlot, ONLY: save_analytic_solution, save_shock_location, &
                                     save_plot_all, save_distribution_function
      use PT_ModProc, ONLY: iComm, iError

      ! advance the solution in time
      logical, save :: IsFirstCall = .true.
      real :: TimeLimit ! Time at end of current timestep

      ! write the initial background state to the output file
    !--------------------------------------------------------------------------
      if(IsFirstCall)then
         ! recompute the derived components of state vector, e.g.
         ! magnitude of magnetic field and velocity etc. Smooth if needed.
         if(.not.DoReadMhData) call get_other_state_var

         if(DoRunTest) then
            ! test shock not located at lagr = 1.
            ! this subroutine sets iShock to the appropriate lagr coordinate
            call get_test_shock

            if(DoSharpenShock) call sharpen_shock
            call save_analytic_solution
         end if
         IsFirstCall = .false.
      end if

      ! May need to read background data from files
      if(DoReadMhData)then
         ! copy some variables from the previous time step
         call copy_old_state
         ! Read the background data from file
         call read_mh_data()
         ! Read from file: MHData_VIB(0:nMHData,::) for the time moment
         ! DataInputTime
      end if

      ! recompute the derived components of state vector, e.g.
      ! magnitude of magnetic field and velocity etc
      call get_other_state_var

      if(DataInputTime <= PTTime) RETURN

      call get_dLogRho
      call get_shock_location

      if(DoSharpenShock) call sharpen_shock
      call save_shock_location

      TimeLimit = min(DataInputTime, TimeMax)
      if(iProc == 0) then
         write(*,*) '# ------------------------------------------------ #'
         write(*,'(a, f10.2, f10.2)') 'Advancing from: ', PTTime, TimeLimit
      end if

      ! run the model from PTTime to TimeLimit
      call advance(TimeLimit)

      ! All processors must finish current timestep before saving solution
      call MPI_BARRIER(iComm, iError)

      if(DoRunTest) then
         call save_distribution_function(iIter + 1, TimeLimit)
      else

         call save_plot_all(iIter + 1, TimeLimit)
      end if

      ! update time & iteration counters
      iIter = iIter + 1
      PTTime = TimeLimit

      ! -- restart not implemented yet -- !
      ! call check_save_restart(Dt)
   end subroutine run
  !============================================================================
   subroutine check

      use ModUtilities, ONLY: make_dir
      ! Make output directory

    character(len=*), parameter:: NameSub = 'check'
    !--------------------------------------------------------------------------
   end subroutine check
  !============================================================================
end module PT_ModMain
!==============================================================================
