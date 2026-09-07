subroutine allocate_array()
   use spectotime_par
   use constants

implicit none

#ifdef USE_MPI
  include "mpif.h"
#endif


   integer nsta_remained

   if(nstation.gt.nproc) stop 'Error, nstation should be larger than nproc'

#ifdef USE_MPI
   nstation=int(nstation_total/nproc)
   nsta_remained=mod(nstation_total,nproc)
   ista_start=nstation*myrank+1
   ista_end=ista_start+nstation-1


   if(myrank.lt.nsta_remained) then
      nstation=nstation+1
      ista_start=ista_start+myrank
      ista_end=ista_end+myrank+1
   else
      ista_start=ista_start+nsta_remained
      ista_end=ista_end+nsta_remained
   end if
#else 
   nstation=nstation_total
   ista_start=1
   ista_end=nstation_total
#endif

   if(ista_end-ista_start+1.ne.nstation) stop 'Error in counting stations'
   if(nstation*nfrequency.gt.max_array) then
      stop 'the size of array is larger than max_array'
   end if



   allocate(station_lat(nstation))
   allocate(station_lon(nstation))
   allocate(station_net(nstation))
   allocate(station_name(nstation))
   allocate(spec(nstation,ncomp,nfrequency))
!   allocate(spec1(nstation,ncomp,nfrequency))
   allocate(work_time(4*ismooth*nfrequency))
   allocate(work_spc(2*ismooth*nfrequency))
end subroutine allocate_array
