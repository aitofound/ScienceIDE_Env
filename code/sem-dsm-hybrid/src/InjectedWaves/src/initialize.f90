subroutine initialize()
  use extract_Injectedwaves_par, only:myrank,nproc
  implicit none
#ifdef USE_MPI
  include "mpif.h"
#endif
  !local parameters
  integer ::ier


#ifdef USE_MPI
  call MPI_INIT(ier)
  call MPI_COMM_SIZE(MPI_COMM_WORLD,nproc,ier)
  call MPI_COMM_RANK(MPI_COMM_WORLD,myrank,ier)
  if( ier /= 0 ) call exit_MPI('error MPI initialization')
#else
  nproc = 1
  myrank = 0
#endif

end subroutine
