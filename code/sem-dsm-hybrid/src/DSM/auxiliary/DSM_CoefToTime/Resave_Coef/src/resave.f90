program resave
  use resave_par
  implicit none
  integer ::ier

#ifdef USE_MPI
  include "mpif.h"
#endif

#ifdef USE_MPI
  call initialize()
#endif
 
  call read_par()
  call partition_job(nl_total,nproc,myrank,l_start,&
                         l_end,npack_total,npack_thisproc,&
                         nl_eachpack,nl_remained)
  #ifdef USE_MPI
      call MPI_BARRIER(MPI_COMM_WORLD,ier)
  #endif

  call allocate_array(l_start,l_end)
  #ifdef USE_MPI
      call MPI_BARRIER(MPI_COMM_WORLD,ier)
  #endif
  call read_coef(l_start,l_end)
  call save_coef(l_start,l_end)

#ifdef USE_MPI
  call MPI_FINALIZE(ier)
#endif

 
end program
