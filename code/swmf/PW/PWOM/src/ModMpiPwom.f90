!extension of ModMpi in share to include PWOM interfaces for particles
module ModMpiPwom
  use ModMpiPwomInterfaces

  use ModMpiOrig, &
       MPI_REAL_ORIG => MPI_REAL, MPI_COMPLEX_ORIG => MPI_COMPLEX

  implicit none

  ! iRealPrec = 1 if the code is compiled with 8 byte reals and 0 otherwise
  integer, parameter :: iRealPrec = (1.00000000011 - 1.0)*10000000000.0

  integer, parameter :: MPI_REAL = &
       iRealPrec*MPI_DOUBLE_PRECISION + (1-iRealPrec)*MPI_REAL_ORIG

  integer, parameter :: MPI_COMPLEX = &
       iRealPrec*MPI_DOUBLE_COMPLEX + (1-iRealPrec)*MPI_COMPLEX_ORIG


end module ModMpiPwom
!==============================================================================
