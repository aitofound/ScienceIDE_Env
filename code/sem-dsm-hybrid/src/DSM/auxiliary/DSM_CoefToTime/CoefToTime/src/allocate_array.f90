!subroutine initial_para()

!end subroutine


subroutine allocate_array()
   use spectotime_par
   use constants

implicit none

#ifdef USE_MPI
  include "mpif.h"
#endif

   if(2*ncomp_disp*ntheta_thisset*npt_save*ndepth.gt.max_array) then
      stop 'the size of array is larger than max_array'
   end if
   if(ndepth*ncomp_strain*ntheta_thisset*nfrequency.gt.max_array) then
      stop 'the size of array is larger than max_array'
   end if

 
   allocate(theta(ntheta_thisset))
   if(fluid) then
        allocate(r_fluid(ndepth))
        allocate(rho_fluid(ndepth))
        allocate(coef_disp_fluid(-m0:m0,ndepth,nfrequency,nl_eachpack))
        allocate(coef_dcdr_fluid(-m0:m0,ndepth,nfrequency,nl_eachpack))
        allocate(disp_fluid_lm(3,-m0:m0))
        allocate(pressure_lm(-m0:m0))
        allocate(chi_dot_lm(-m0:m0))
        allocate(spec_disp_fluid(ncomp_disp,ntheta_thisset,ndepth,nfrequency))
        allocate(spec_pressure(ntheta_thisset,ndepth,nfrequency))
        allocate(spec_chi_dot(ntheta_thisset,ndepth,nfrequency))

        allocate(array_pressure(npt_save,ndepth,ntheta_thisset))
        allocate(array_chi_dot(npt_save,ndepth,ntheta_thisset))
        allocate(array_disp_fluid(npt_save,ndepth,ntheta_thisset,ncomp_disp))
        spec_disp_fluid(:,:,:,:)=dcmplx(0.d0,0.d0)
        spec_pressure(:,:,:)=dcmplx(0.d0,0.d0)
        spec_chi_dot(:,:,:)=dcmplx(0.d0,0.d0)

   else
        allocate(r_solid(ndepth))
        allocate(A(ndepth))
        allocate(C(ndepth))
        allocate(F(ndepth))
        allocate(L(ndepth))
        allocate(N(ndepth))
        allocate(coef_disp_solid(-m0:m0,3,ndepth,nfrequency,nl_eachpack))
        allocate(coef_dcdr_solid(-m0:m0,3,ndepth,nfrequency,nl_eachpack))
        allocate(disp_solid_lm(3,-m0:m0))
        allocate(strain_lm(6,-m0:m0))
        allocate(spec_disp_solid(ncomp_disp,ntheta_thisset,ndepth,nfrequency))
        allocate(spec_strain(ncomp_strain,ntheta_thisset,ndepth,nfrequency))
        allocate(array_disp_solid(npt_save,ndepth,ntheta_thisset,ncomp_disp))
        allocate(array_strain(npt_save,ndepth,ntheta_thisset,ncomp_strain))
        spec_disp_solid(:,:,:,:)=dcmplx(0.d0,0.d0)
        spec_strain(:,:,:,:)=dcmplx(0.d0,0.d0)
   end if

   allocate(ifreq_start(nl_eachpack))
   allocate(work_time(4*ismooth*nfrequency))
   allocate(work_spc(2*ismooth*nfrequency))


end subroutine allocate_array
