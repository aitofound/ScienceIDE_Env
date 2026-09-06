program unit_test
  use ModPlasmasphere, ONLY: unit_test_plasmasphere
  !use ModMPI
  !use CON_planet, ONLY: init_planet_const, set_planet_defaults,is_planet_init

  integer :: iError
  !character(len=5) :: NamePlanet = 'EARTH'
!  character(len=7) :: NamePlanet = 'JUPITER'
  logical :: IsPlanetSet=.false.  
  !-----------------------------------------------------------------------------
!
!  !****************************************************************************
!  ! Initiallize MPI and get number of processors and rank of given processor
!  !****************************************************************************
!
!  write(*,*) 'Initiallizing MPI'
!
!  !---------------------------------------------------------------------------
!  call MPI_INIT(iError)
!  iComm = MPI_COMM_WORLD
!
!  call MPI_COMM_RANK(iComm,iProc,iError)
!  call MPI_COMM_SIZE(iComm,nProc,iError)
!
!  !\
!  ! Initialize the planetary constant library and set Earth
!  ! as the default planet.
!  !/
!  write(*,*) 'Initiallizing Planet'
!
!  call init_planet_const
!
!  if (NamePlanet == 'EARTH') then
!     call set_planet_defaults
!     IsPlanetSet = .true.
!  else
!     IsPlanetSet = is_planet_init(NamePlanet)
!  endif
!  
!  if (.not.IsPlanetSet) then
!     call CON_stop('Planet not set. Stopping test')
!  endif
!  write(*,*) 'starting test_pusher'
!
!  call timing_active(.true.)
!  call timing_step(0)
!  call timing_start('unit_test_pusher')
!  
!
!  call timing_start('test_pusher')
  call unit_test_plasmasphere
!  call timing_stop('test_pusher')

!  call timing_report
end program unit_test
