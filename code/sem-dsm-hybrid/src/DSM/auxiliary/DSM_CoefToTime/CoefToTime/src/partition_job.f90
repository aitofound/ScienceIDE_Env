subroutine  partition_job(itheta_start,itheta_end,ntheta_thisset,&
                          distset_id,iproc,nproc,ntheta_total)
integer, intent(in) ::nproc,ntheta_total,iproc
integer, intent(out) ::itheta_start,itheta_end,distset_id,ntheta_thisset

!local parameters
integer ::ntheta_eachiproc,ntheta_remained

  if(nproc.gt.ntheta_total) stop 'ntheta_total<nproc, use less processors'
  ntheta_eachiproc=int(ntheta_total/nproc)
  ntheta_remained=ntheta_total-ntheta_eachiproc*nproc
  itheta_start=iproc*ntheta_eachiproc+1
  itheta_end=itheta_start+ntheta_eachiproc-1
  if(iproc.lt.ntheta_remained) then
    itheta_start=itheta_start+iproc
    itheta_end=itheta_end+iproc+1
  else
    itheta_start=itheta_start+ntheta_remained
    itheta_end=itheta_end+ntheta_remained
  end if
  ntheta_thisset=itheta_end-itheta_start+1
  distset_id=iproc
end subroutine
