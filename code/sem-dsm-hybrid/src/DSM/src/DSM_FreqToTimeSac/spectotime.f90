program spectotime

   use spectotime_par
   implicit none
#ifdef USE_MPI
  include "mpif.h"
#endif

   integer ::ier
#ifdef USE_MPI
   call initialize()
#endif
   call read_para()
   call allocate_array()
   if (myrank.eq.0) print *,"Read FFT..."
   call read_station_spec()
   if (myrank.eq.0) print *,"Do IFFT and save seismograms..."
   call convspc(nstation,ncomp,ista_start,time_series_length,nfrequency, &
          omega_imag,spec,source_depth,source_lat,source_lon, &
          station_lat,station_lon,work_spc,work_time )
!   call save_time()
#ifdef USE_MPI
   call MPI_FINALIZE(ier)
#endif
   if (myrank.eq.0) print *,"Job completed!"

end program
