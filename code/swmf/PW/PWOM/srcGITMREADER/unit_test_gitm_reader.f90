program unit_test_gitm_reader
  use ModGitmAtmos, ONLY: unit_test_gitm
!  use ModMPI
  use CON_planet, ONLY: init_planet_const, set_planet_defaults,is_planet_init
  implicit none
  
  integer :: iError
  
  character(len=5) :: NamePlanet = 'EARTH'
  !character(len=7) :: NamePlanet = 'JUPITER'
  logical :: IsPlanetSet=.false.
  !-----------------------------------------------------------------------------

  !****************************************************************************
  ! Initiallize MPI and get number of processors and rank of given processor
  !****************************************************************************

!  write(*,*) 'Initiallizing MPI'

  !---------------------------------------------------------------------------
!  call MPI_INIT(iError)
!  iComm = MPI_COMM_WORLD
!
!  call MPI_COMM_RANK(iComm,iProc,iError)
!  call MPI_COMM_SIZE(iComm,nProc,iError)

  !\
  ! Initialize the planetary constant library and set Earth
  ! as the default planet.
  !/
  write(*,*) 'Initiallizing Planet',NamePlanet

  call init_planet_const

  if (NamePlanet == 'EARTH') then
     call set_planet_defaults
     IsPlanetSet = .true.
  else
     IsPlanetSet = is_planet_init(NamePlanet)
  endif
  
  if (.not.IsPlanetSet) then
     call CON_stop('Planet not set. Stopping unit_test_gitm')
  endif
  
  
  write(*,*) 'running unit_test_gitm'
  call unit_test_gitm
  
end program unit_test_gitm_reader


!============================================================================
! The following subroutines are here so that we can use SWMF library routines
! Also some features available in SWMF mode only require empty subroutines
! for compilation of the stand alone code.
!============================================================================
subroutine CON_stop(StringError)
!  use ModSeMpi, ONLY : iProc,iComm
!  use ModSeState, ONLY : Time
  use ModMpi
  implicit none
  character (len=*), intent(in) :: StringError

  ! Local variables:
  integer :: iError,nError
  !----------------------------------------------------------------------------

!  write(*,*)'Stopping execution! me=',iProc,' at time=',Time,&
!       ' with msg:'
  write(*,*)'Stopping execution! me=with msg:'
  write(*,*)StringError
!  call MPI_abort(iComm, nError, iError)
  call MPI_abort(MPI_COMM_WORLD, nError, iError)
  stop

end subroutine CON_stop

subroutine CON_set_do_test(String,DoTest,DoTestMe)
  implicit none
  character (len=*), intent(in)  :: String
  logical          , intent(out) :: DoTest, DoTestMe

  DoTest = .false.; DoTestMe = .false.

end subroutine CON_set_do_test

subroutine CON_io_unit_new(iUnit)

  use ModIoUnit, ONLY: io_unit_new
  implicit none
  integer, intent(out) :: iUnit

  iUnit = io_unit_new()

end subroutine CON_io_unit_new

