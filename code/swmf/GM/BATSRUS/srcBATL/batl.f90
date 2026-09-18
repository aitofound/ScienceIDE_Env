program batl

  ! A simple program running BATL unit tests

  use BATL_lib, ONLY: init_mpi, clean_mpi
  use BATL_unit_test

  implicit none

  integer:: iError
  !----------------------------------------------------------------------------
  call init_mpi

  call test_tree
  call test_geometry
  call test_grid
  call test_pass_cell
  call test_pass_face
  call test_pass_node
  call test_amr
  call test_amr_criteria

  call clean_mpi

end program batl
!==============================================================================

