subroutine allocate_array_convolution()
use convolution_par
use coupling_SEM_DSM_par,only:npoints_max,myrank,nfit_dep,nfit_dist,disp_elas_DSM,&
       stress_DSM,disp_acous_DSM,pressure_DSM,nfreq_DSM,nloc_elas_DSM,nloc_acous_DSM
use constants

implicit none

allocate(disp(ncomp,nfreq_DSM,nstation))
!if(myrank.eq.0) then
  allocate(final_disp(ncomp,nfreq_DSM,nstation))
!end if
allocate(station_lat(nstation))
allocate(station_lon(nstation))
allocate(station_name(nstation))
allocate(station_coord(ncomp,nstation))


allocate(disp_elas_DSM(nloc_elas_DSM,ncomp,ncomp))
allocate(stress_DSM(nloc_elas_DSM,ncomp_stress,ncomp))
allocate(disp_acous_DSM(nloc_acous_DSM,ncomp,ncomp))
allocate(pressure_DSM(nloc_acous_DSM,ncomp))

allocate(rot_matrix_bound(ncomp,ncomp,npoints_max,nstation))
allocate(rot_matrix_station(ncomp,ncomp,npoints_max,nstation))
!allocate(gcarc(npoints_max,nstation))
!allocate(id_Green(npoints_max,nstation))

allocate(distance(npoints_max,nstation))
allocate(first_idep_fit_DSM(npoints_max,nstation))
allocate(first_idist_fit_DSM(npoints_max,nstation))
allocate(depths_fit_in(nfit_dep))
allocate(dists_fit_in(nfit_dist))
allocate(imag_z_in(nfit_dep,nfit_dist))
allocate(real_z_in(nfit_dep,nfit_dist))

allocate(work_time(32*nfreq_DSM))
allocate(work_spc(16*nfreq_DSM))

disp(:,:,:)=cmplx(0.d0)
if(myrank.eq.0) then
   final_disp(:,:,:)=cmplx(0.d0)
end if
disp_elas_DSM(:,:,:)=cmplx(0.d0)
stress_DSM(:,:,:)=cmplx(0.d0)
disp_acous_DSM(:,:,:)=cmplx(0.d0)
pressure_DSM(:,:)=cmplx(0.d0)
rot_matrix_bound(:,:,:,:)=0.d0
rot_matrix_station(:,:,:,:)=0.d0
!gcarc(:,:)=0.d0
!id_Green(:,:)=0
station_coord(:,:)=0.0


work_time(:)=0.d0
work_spc(:)=cmplx(0.d0)
end subroutine allocate_array_convolution
