program main_iono

  use GIMME_iono_grid, ONLY: init_grid_iono, iProc, nProc, iComm
  use GIMME_mag_input, ONLY: init_mag_input, test_input
  use GIMME_plots, ONLY: gimme_plot
  use GIMME_potential, ONLY: get_gimme_potential
  use GIMME_conductance, ONLY: &
       init_conductance, set_conductance, calc_coefficients
  use ModUtilities, ONLY: CON_stop
  use ModMpi
  use CON_planet, ONLY: init_planet_const, set_planet_defaults,is_planet_init

  character(len=100) :: NamePlanet='EARTH'
  integer :: errcode
  logical :: IsPlanetSet
  !----------------------------------------------------------------------------
  ! Initiallize MPI and get number of processors and rank of given processor

  call MPI_INIT(errcode)
  iComm = MPI_COMM_WORLD

  call MPI_COMM_RANK(iComm,iProc,errcode)
  call MPI_COMM_SIZE(iComm,nProc,errcode)

  ! Initialize the planetary constant library and set Earth
  ! as the default planet.
  call init_planet_const

  if (NamePlanet == 'EARTH') then
     call set_planet_defaults
     IsPlanetSet = .true.
  else
     write(*,*) NamePlanet
     IsPlanetSet = is_planet_init(NamePlanet)
  endif

  if (.not.IsPlanetSet) then
     call CON_stop('Planet not set. Stopping GIMME')
  endif

  call init_grid_iono
  call init_conductance
  call set_conductance
  call calc_coefficients
  call init_mag_input
  call test_input
  call get_gimme_potential
  call gimme_plot

end program main_iono
!==============================================================================
