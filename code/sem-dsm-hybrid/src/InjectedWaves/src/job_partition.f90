subroutine job_partition(myrank,ndepth_total,nproc,idepth_start,idepth_end)

implicit none

#ifdef USE_MPI
  include "mpif.h"
#endif

#ifdef USE_MPI
   integer ndepth_remained
#endif
integer, intent(in)::myrank,ndepth_total,nproc
integer, intent(out)::idepth_start,idepth_end

integer ::ndepth

#ifdef USE_MPI
   if(ndepth_total.le.nproc) then
      if(myrank.lt.ndepth_total) then
       idepth_start=myrank+1
       idepth_end=myrank+1
       ndepth=1
      else
       idepth_start=0
       idepth_end=0
       ndepth=0
      end if
   else
     ndepth=int(ndepth_total/nproc)
     ndepth_remained=mod(ndepth_total,nproc)
     idepth_start=ndepth*myrank+1
     idepth_end=idepth_start+ndepth-1

     if(myrank.lt.ndepth_remained) then
        ndepth=ndepth+1
        idepth_start=idepth_start+myrank
        idepth_end=idepth_end+myrank+1
     else
        idepth_start=idepth_start+ndepth_remained
        idepth_end=idepth_end+ndepth_remained
     end if
   end if
   write(*,*) "idepth_end",idepth_end
#else 
   ndepth=ndepth_total
   idepth_start=1
   idepth_end=ndepth_total
#endif

   if((idepth_end-idepth_start+1.ne.ndepth).and. &
      (idepth_end.ne.0 .or. idepth_start.ne.0.or. ndepth.ne.0))  stop 'Error in counting depths'
!   if(2*ismooth*ncomp*ntheta*nfrequency.gt.max_array) then
!      stop 'the size of array is larger than max_array'
!   end if

end subroutine job_partition
