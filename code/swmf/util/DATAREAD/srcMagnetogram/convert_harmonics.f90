!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
program convert_harmonics

  ! Transform spherical harmonics file into 3D lookup table  for B0  field
  use ModMagnetogram
  use ModMpi, ONLY: MPI_init, MPI_Finalize, MPI_COMM_WORLD
  use ModReadParam
  use ModUtilities
  implicit none
  character(len=lStringLine) :: NameCommand
  integer :: iError
  character(len=*), parameter:: NameSub = 'convert_harmonics'
  !----------------------------------------------------------------------------
  call MPI_init(iError)
  call read_file('HARMONICSGRID.in',iCommIn = MPI_COMM_WORLD)
  call read_init
  call read_echo_set(.false.)

  do
     if(.not.read_line() ) EXIT
     if(.not.read_command(NameCommand)) CYCLE
     select case(NameCommand)
     case("#FACTORB0", "#HARMONICSGRID", "#HARMONICSFILE")
        call read_magnetogram_param(NameCommand)
     case("#END")
        EXIT
     case default
        call CON_stop(NameSub//' invalid NameCommand='//NameCommand)
     end select
  end do
  call init_magnetogram_lookup_table(MPI_COMM_WORLD)
  call MPI_finalize(iError)
end program convert_harmonics
!==============================================================================
