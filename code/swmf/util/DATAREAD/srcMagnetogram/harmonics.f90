!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
program harmonics

  ! Transform raw magnetogram file into spherical harmonics file

  use ModMagHarmonics

  implicit none
  integer:: iError
  !----------------------------------------------------------------------------
  call read_harmonics_param
  call read_modified_magnetogram
  call calc_harmonics

end program harmonics
!==============================================================================
