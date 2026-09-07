subroutine do_interpolation(idepth,media_type)
use extract_Injectedwaves_par, only: ndistances_SEM,distances_SEM,min_theta_DSM,&
     max_theta_DSM,dtheta_DSM,depths_acous_DSM,depths_elas_DSM,first_idep_fit_DSM,&
     nfit_dist,nfit_dep,ncomp_disp,ncomp_velo,ncomp_stress,disp_acous_fit_DSM,&
     pressure_fit_DSM,potential_fit_DSM,velo_elas_fit_DSM,stress_fit_DSM,&
     disp_acous_SEM,pressure_SEM,chi_dot_SEM,velo_elas_SEM,stress_SEM,nfrequency,&
     depths_SEM,ntheta_DSM
implicit none

integer, intent(in) :: idepth,media_type

!local parameters
integer ::ndist_workon,idist,idist_in_DSM,ifit_dist,ifit_dep,idist_fit,ifreq,icomp
integer ::first_idist_fit_DSM
double precision ::dist_workon,dep_workon
double precision, dimension(nfit_dep)::xarray_in
double precision, dimension(nfit_dist)::yarray_in
double precision, dimension(nfit_dep,nfit_dist) ::imag_zarray_in,real_zarray_in
double precision ::imag_z_out,real_z_out

if(media_type.eq.1) then
  xarray_in(1:nfit_dep)=depths_acous_DSM(first_idep_fit_DSM:first_idep_fit_DSM+nfit_dep-1)
else
  xarray_in(1:nfit_dep)=depths_elas_DSM(first_idep_fit_DSM:first_idep_fit_DSM+nfit_dep-1)
end if

dep_workon=depths_SEM(idepth)
ndist_workon=ndistances_SEM(idepth)

do idist=1,ndist_workon
  dist_workon=distances_SEM(idepth,idist)
  ! Near-zero SEM distances can fall just below the first DSM distance sample.
  ! Use the first nfit_dist DSM samples on the right side for this endpoint case.
  if(dist_workon.le.min_theta_DSM) then
    first_idist_fit_DSM=1
  else
    first_idist_fit_DSM=int((dist_workon-min_theta_DSM)/dtheta_DSM)+1-int(nfit_dist/2) + 1
    if(first_idist_fit_DSM>ntheta_DSM-nfit_dist+1) first_idist_fit_DSM=ntheta_DSM-nfit_dist+1
    if(first_idist_fit_DSM<1) first_idist_fit_DSM=1
  end if

  do ifit_dist=1,nfit_dist
     idist_fit=first_idist_fit_DSM+ifit_dist-1
     if(idist_fit.le.0 .or. idist_fit.gt.ntheta_DSM) &
        call exit_mpi('work-on distance is out of the DSM distance range!')
     yarray_in(ifit_dist)=min_theta_DSM+(idist_fit-1)*dtheta_DSM
  end do

  ! Fluid media
  if(media_type.eq.1) then
    do ifreq=1,nfrequency
      ! displacement 
      do icomp=1,ncomp_disp
        do ifit_dist=1,nfit_dist
           idist_fit=first_idist_fit_DSM+ifit_dist-1
           do ifit_dep=1,nfit_dep

           imag_zarray_in(ifit_dep,ifit_dist)=aimag(disp_acous_fit_DSM(ifreq,ifit_dep,idist_fit,icomp))
           real_zarray_in(ifit_dep,ifit_dist)=real(disp_acous_fit_DSM(ifreq,ifit_dep,idist_fit,icomp))
           end do
        end do

        call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,imag_zarray_in,&
                                1,dep_workon,dist_workon,imag_z_out)
        call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,real_zarray_in,&
                                1,dep_workon,dist_workon,real_z_out)
        disp_acous_SEM(ifreq,idist,icomp)=cmplx(real_z_out,imag_z_out)
      end do !ncomp_disp

      ! Pressure
      do ifit_dist=1,nfit_dist
           idist_fit=first_idist_fit_DSM+ifit_dist-1
           do ifit_dep=1,nfit_dep
             imag_zarray_in(ifit_dep,ifit_dist)=aimag(pressure_fit_DSM(ifreq,ifit_dep,idist_fit))
             real_zarray_in(ifit_dep,ifit_dist)=real(pressure_fit_DSM(ifreq,ifit_dep,idist_fit))
           end do
      end do
      call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,imag_zarray_in,&
                                1,dep_workon,dist_workon,imag_z_out)
      call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,real_zarray_in,&
                                1,dep_workon,dist_workon,real_z_out)
      pressure_SEM(ifreq,idist)=cmplx(real_z_out,imag_z_out)

      ! chi_dot (parameter name in SEM) or potential (parameter name in DSM)
      do ifit_dist=1,nfit_dist
           idist_fit=first_idist_fit_DSM+ifit_dist-1
           do ifit_dep=1,nfit_dep
             imag_zarray_in(ifit_dep,ifit_dist)=aimag(potential_fit_DSM(ifreq,ifit_dep,idist_fit))
             real_zarray_in(ifit_dep,ifit_dist)=real(potential_fit_DSM(ifreq,ifit_dep,idist_fit))
           end do
      end do
      call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,imag_zarray_in,&
                                1,dep_workon,dist_workon,imag_z_out)
      call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,real_zarray_in,&
                                1,dep_workon,dist_workon,real_z_out)
      chi_dot_SEM(ifreq,idist)=cmplx(real_z_out,imag_z_out)

    end do  !ifreq

  ! Solid media
  else
    do ifreq=1,nfrequency
      ! velocity seismograms
      do icomp=1,ncomp_velo
        do ifit_dist=1,nfit_dist
           idist_fit=first_idist_fit_DSM+ifit_dist-1
           do ifit_dep=1,nfit_dep

           imag_zarray_in(ifit_dep,ifit_dist)=aimag(velo_elas_fit_DSM(ifreq,ifit_dep,idist_fit,icomp))
           real_zarray_in(ifit_dep,ifit_dist)=real(velo_elas_fit_DSM(ifreq,ifit_dep,idist_fit,icomp))
           end do
        end do

        call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,imag_zarray_in,&
                                1,dep_workon,dist_workon,imag_z_out)
        call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,real_zarray_in,&
                                1,dep_workon,dist_workon,real_z_out)
        velo_elas_SEM(ifreq,idist,icomp)=cmplx(real_z_out,imag_z_out)
      end do


      ! stress
      do icomp=1,ncomp_stress
        do ifit_dist=1,nfit_dist
           idist_fit=first_idist_fit_DSM+ifit_dist-1
           do ifit_dep=1,nfit_dep

           imag_zarray_in(ifit_dep,ifit_dist)=aimag(stress_fit_DSM(ifreq,ifit_dep,idist_fit,icomp))
           real_zarray_in(ifit_dep,ifit_dist)=real(stress_fit_DSM(ifreq,ifit_dep,idist_fit,icomp))
           end do
        end do

        call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,imag_zarray_in,&
                                1,dep_workon,dist_workon,imag_z_out)
        call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,xarray_in,yarray_in,real_zarray_in,&
                                1,dep_workon,dist_workon,real_z_out)
        stress_SEM(ifreq,idist,icomp)=cmplx(real_z_out,imag_z_out)
      end do
    end do  !ifreq
  end if
end do

end subroutine do_interpolation

