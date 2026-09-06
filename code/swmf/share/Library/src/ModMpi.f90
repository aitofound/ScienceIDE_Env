!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
! In Fortran 90 it is customary to use a module instead of including files.
! The ModMpiOrig and ModMpi modules provide interfaces to mpif.h.
!
! In ModMpi the MPI\_REAL parameter is set to the value of the
! MPI\_DOUBLE\_PRECISION parameter if the code is compiled with double
! precision accuracy (Config.pl -double). In this case the iRealPrec
! parameter is set to 1.
!
! If ModMpi is used, it should be compiled with the same precision as the F90
! code using it.
!
! This module also provides simple interfaces to
! MPI\_reduce with MPI\_IN\_PLACE option for real and integer
! scalars and arrays.
!
! revision history:
! 07/02/2003 G. Toth <gtoth@umich.edu> - initial version of ModMpi
! 07/20/2003 G. Toth - change the MPI_REAL definition
! 07/30/2004 G. Toth - updated the description for the modified mpif90.h files.
! 05/13/2011 G. Toth - modified to use original mpif.h file
! 03/10/2016 G. Toth - added MPI_reduce_* routines for in-place use.
module ModMpi
  use ModMpiInterfaces
  use ModMpiModified
end module ModMpi
!==============================================================================
