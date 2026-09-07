subroutine allocate_array(l_start,l_end)
  use resave_par,only:coef_c_top,coef_dcdr_top,coef_c_bot,&
                coef_dcdr_bot,coef_c_read,coef_dcdr_read,&
                m0,ncomp_foridep,nfrequency,ncomp_solid,&
                nl_total,ndepth,ifreq_start,myrank
  implicit none
  integer,intent(in) ::l_start,l_end
  integer ::ier



  allocate(coef_c_top(-m0:m0,ncomp_foridep(1),nfrequency,l_start:l_end))
  allocate(coef_dcdr_top(-m0:m0,ncomp_foridep(1),nfrequency,l_start:l_end))
  allocate(coef_c_bot(-m0:m0,ncomp_foridep(2),nfrequency,l_start:l_end))
  allocate(coef_dcdr_bot(-m0:m0,ncomp_foridep(2),nfrequency,l_start:l_end))
  allocate(coef_c_read(0:nl_total-1,-m0:m0,ndepth,ncomp_solid))
  allocate(coef_dcdr_read(0:nl_total-1,-m0:m0,ndepth,ncomp_solid))
  allocate(ifreq_start(l_start:l_end))


  coef_c_top(:,:,:,:)   =dcmplx(0.d0,0.d0)
  coef_dcdr_top(:,:,:,:)=dcmplx(0.d0,0.d0)
  coef_c_bot(:,:,:,:)   =dcmplx(0.d0,0.d0)
  coef_dcdr_bot(:,:,:,:)=dcmplx(0.d0,0.d0)
  ifreq_start(:)=-1


end subroutine
