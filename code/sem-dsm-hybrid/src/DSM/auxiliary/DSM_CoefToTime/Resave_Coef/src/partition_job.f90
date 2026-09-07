subroutine partition_job(nl_total,nproc,iproc,l_start,&
                         l_end,npack_total,npack_thisproc,&
                         nl_eachpack,nl_remained)

implicit none
integer, intent(in) ::nl_total,nproc,iproc,nl_eachpack
integer, intent(out)::l_start,l_end,npack_total,npack_thisproc,&
                      nl_remained

!local parameter
integer ::npack_eachproc,npack_remained

npack_total=int(nl_total/nl_eachpack)
nl_remained=nl_total-npack_total*nl_eachpack
npack_eachproc=int(npack_total/nproc)
if(npack_eachproc.lt.0) stop 'npack_eachproc<0, try less processors'
npack_remained=npack_total-npack_eachproc*nproc
if(iproc.lt.npack_remained) then
  npack_thisproc=npack_eachproc+1
! the first order is 0, but not 1.
  l_start=iproc*(npack_eachproc+1)*nl_eachpack
  l_end=l_start+(npack_eachproc+1)*nl_eachpack-1
else
  npack_thisproc=npack_eachproc
  l_start=npack_remained*nl_eachpack+iproc*npack_eachproc*nl_eachpack
  l_end=l_start+npack_eachproc*nl_eachpack-1
end if
if(nl_remained.gt.0.and.iproc.eq.nproc-1) then
   npack_thisproc=npack_thisproc+1
   l_end=l_end+nl_remained
end if
if(nl_remained.lt.0) npack_total=npack_total+1
end subroutine
