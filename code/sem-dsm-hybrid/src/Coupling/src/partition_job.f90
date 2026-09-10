subroutine partition_job()
use coupling_SEM_DSM_par
use constants

implicit none

integer ::npack_total_per_group,npack_total_last_group,npack_last_group_per_proc
integer ::igroup

#ifdef USE_MPI
  include "mpif.h"
#endif

#ifdef USE_MPI
   integer npack_remained
#endif

   if(npackage_SEM.lt.nproc) stop 'Error, npack should be larger than nproc'

#ifdef USE_MPI
   npack_total_per_group=nproc*npack_per_proc
   ngroups=int(npackage_SEM/npack_total_per_group)
   if(ngroups.eq.0) then
      ngroups=1
      npack_total_last_group=npackage_SEM
   else
      if(mod(npackage_SEM,npack_total_per_group*ngroups)>0) then
         npack_total_last_group=mod(npackage_SEM,npack_total_per_group*ngroups)
         ngroups=ngroups+1
      else 
         npack_total_last_group=npack_total_per_group
      end if
   end if

   allocate(npack(ngroups))
   allocate(ipack_start(ngroups))
   allocate(ipack_end(ngroups))

   do igroup=1,ngroups
      if(igroup.lt.ngroups) then
        npack(igroup)=npack_per_proc
        ipack_start(igroup)=npack_total_per_group*(igroup-1)+npack(igroup)*myrank+1
        ipack_end(igroup)=ipack_start(igroup)+npack(igroup)-1
      else
        npack_last_group_per_proc=int(npack_total_last_group/nproc)
        npack_remained=mod(npack_total_last_group,nproc)
        ipack_start(igroup)=npack_total_per_group*(igroup-1)+npack_last_group_per_proc*myrank+1
        ipack_end(igroup)=ipack_start(igroup)+npack_last_group_per_proc-1

        if(myrank.lt.npack_remained) then
           npack(igroup)=npack_last_group_per_proc+1
           ipack_start(igroup)=ipack_start(igroup)+myrank
           ipack_end(igroup)= ipack_end(igroup)+myrank+1
        else
           npack(igroup)=npack_last_group_per_proc
           ipack_start(igroup)=ipack_start(igroup)+npack_remained
           ipack_end(igroup)=ipack_end(igroup)+npack_remained
        end if
      end if
   end do
#else 
   ngroups=1
   allocate(npack(ngroups))
   allocate(ipack_start(ngroups))
   allocate(ipack_end(ngroups))
   npack(ngroups)=npackage_SEM
   ipack_start(ngroups)=1
   ipack_end(ngroups)=npackage_SEM
#endif

   do igroup=1,ngroups
      if(ipack_end(igroup)-ipack_start(igroup)+1.ne.npack(igroup)) stop 'Error in counting packages'
      if(npack(igroup).gt.max_npackages) then
         stop 'the size of array is larger than max_array'
      end if
  end do


end subroutine partition_job
