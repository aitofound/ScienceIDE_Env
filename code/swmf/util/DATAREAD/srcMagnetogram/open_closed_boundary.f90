!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
program open_closed_boundary

  ! Calculate open/closed area/flux

  use ModOpenClosedBoundary
  use ModMpi, ONLY: MPI_init, MPI_Finalize, MPI_COMM_WORLD
  use ModReadParam
  use ModUtilities
  use ModLookupTable, ONLY: read_lookup_table_param
  implicit none
  character(len=lStringLine) :: NameCommand
  character(len=*), parameter:: NameSub = 'open_closed_boundary'
  !----------------------------------------------------------------------------
  call MPI_init(iError)
  call read_file('BOUNDARY.in',iCommIn = MPI_COMM_WORLD)
  call read_init
  call read_echo_set(.false.)
  do
     if(.not.read_line() ) EXIT
     if(.not.read_command(NameCommand)) CYCLE
     select case(NameCommand)
     case('#LOOKUPTABLE')
        call read_lookup_table_param
     case('#OPENCLOSEDBOUNDARY')
        call read_open_closed_boundary(NameCommand)
     case("#END")
        EXIT
     case default
        call CON_stop(NameSub//' invalid NameCommand='//NameCommand)
     end select
  end do
  call init_b0(MPI_COMM_WORLD)
  call get_open_closed_boundary(MPI_COMM_WORLD)
  call MPI_finalize(iError)
end program open_closed_boundary
!==============================================================================
