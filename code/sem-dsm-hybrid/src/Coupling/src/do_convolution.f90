subroutine do_convolution(ifreq)
use convolution_par
use coupling_SEM_DSM_par,only:nproc,myrank,npack,fft_disp_bound_new,fft_traction_bound_new,&
              normal_vector,coord_bound,c,jacobian,npoints,is_elastic,&
              is_acoustic,id_depth_SEM,depths_solid_SEM,depths_fluid_SEM,disp_elas_DSM,&
              zones_solid_SEM,zones_fluid_SEM,stress_DSM,disp_acous_DSM,pressure_DSM,&
              depths_elas_DSM,zones_elas_DSM,ndep_elas_DSM,depths_acous_DSM,zones_acous_DSM,&
              ndep_acous_DSM,nfit_dep,nfit_dist,ndist_DSM,min_dist_DSM,ddist_DSM

use constants
implicit none

!input/output
integer ::ifreq

!other variables
integer ::ista,ipoint,zone_this_point,idep0_fit_DSM,idist0_fit_DSM
integer ::ifit_dep,ifit_dist
double precision::depth_this_point,dist_this_point
integer ::icomp1,icomp_disp,icomp1_stress,icomp2_stress,icomp_force
integer ::idGreen
!double precision,dimension(ncomp,ncomp,ncomp,ncomp) ::my_c_bound
double precision,dimension(ncomp,ncomp) ::rot1,rot2
complex(kind=8),dimension(ncomp) ::my_disp_bound,my_traction_bound
integer, dimension(ncomp,ncomp) ::icomp_sigma_table
double precision ::real_z_out,imag_z_out
complex(kind=8),dimension(ncomp,ncomp) ::disp_Green_sphe,disp_Green_carte
complex(kind=8),dimension(ncomp,ncomp,ncomp) ::stress_sphe
complex(kind=8),dimension(ncomp,ncomp) ::traction_Green_carte
complex(kind=8) ::term1,term2

! Used for lagrange interpolation scheme
integer ::ifit

!character(len=MAX_STRING_LEN) ::fft_file_check


integer ::ipoints1,ipoints2

data icomp_sigma_table  /1,4,5,4,2,6,5,6,3/


 ipoints1=1
 ipoints2=npoints
 !do ipoint=ipoints1,ipoints2 
 do ipoint=1,npoints
   my_disp_bound(:)=fft_disp_bound_new(ifreq,:,ipoint)
   my_traction_bound(:)=fft_traction_bound_new(ifreq,:,ipoint)
   if(is_elastic(ipoint)) then
      depth_this_point=depths_solid_SEM(id_depth_SEM(ipoint))
   else
      depth_this_point=depths_fluid_SEM(id_depth_SEM(ipoint))
   end if
!   if(myrank.eq.40.and.ipoint.eq.3) then
!     write(fft_file_check,"('OUTPUT_FILES/freq_',i5.5)")ifreq
!     open(unit=40,file=trim(fft_file_check),action='write',form="formatted",status="unknown")
!     write(40,*)my_disp_bound(1)
!     write(40,*)my_disp_bound(2)
!     write(40,*)my_disp_bound(3)
!     write(40,*)my_traction_bound(1)
!     write(40,*)my_traction_bound(2)
!     write(40,*)my_traction_bound(3)
!     close(40)
!   end if

   do ista=1,nstation
     dist_this_point=distance(ipoint,ista)

     ! Get the interpolating DSM depths and distances
     ! Shared for all the frequencies, so only need to do it one time when ifreq=1
     if(ifreq.eq.1) then
        if(is_elastic(ipoint)) then
          zone_this_point=zones_solid_SEM(id_depth_SEM(ipoint))
          call  get_locDSM_fit(depth_this_point,dist_this_point,zone_this_point,nfit_dep,&
                    nfit_dist,min_dist_DSM,ddist_DSM,ndist_DSM,depths_elas_DSM,zones_elas_DSM,&
                    ndep_elas_DSM,idep0_fit_DSM,idist0_fit_DSM)
        else
          zone_this_point=zones_fluid_SEM(id_depth_SEM(ipoint))
          call get_locDSM_fit(depth_this_point,dist_this_point,zone_this_point,nfit_dep,&
                    nfit_dist,min_dist_DSM,ddist_DSM,ndist_DSM,depths_acous_DSM,zones_acous_DSM,&
                    ndep_acous_DSM,idep0_fit_DSM,idist0_fit_DSM)
        end if
        first_idep_fit_DSM(ipoint,ista)=idep0_fit_DSM
        first_idist_fit_DSM(ipoint,ista)=idist0_fit_DSM
     end if

     do ifit_dep=1,nfit_dep
       if(is_elastic(ipoint)) then
         depths_fit_in(ifit_dep)=depths_elas_DSM(first_idep_fit_DSM(ipoint,ista)+ifit_dep-1)
       else
         depths_fit_in(ifit_dep)=depths_acous_DSM(first_idep_fit_DSM(ipoint,ista)+ifit_dep-1)
       end if
     end do
   
     do ifit_dist=1,nfit_dist
       dists_fit_in(ifit_dist)=min_dist_DSM+(first_idist_fit_DSM(ipoint,ista)-1 +ifit_dist-1)*ddist_DSM
     end do
!     if(myrank.eq.34) print *,'idGreen_iproc41',idGreen,ista,normal_vector(:,ipoint)
     do icomp_force=1,ncomp
        !************  displacement ********************************
        do icomp_disp=1,ncomp
          do ifit_dep=1,nfit_dep
             do ifit_dist=1,nfit_dist
                idGreen=(first_idep_fit_DSM(ipoint,ista)-1 + ifit_dep-1)*ndist_DSM + &
                         first_idist_fit_DSM(ipoint,ista)+ifit_dist-1 
                if(is_elastic(ipoint)) then
                  imag_z_in(ifit_dep,ifit_dist)=aimag(disp_elas_DSM(idGreen,icomp_disp,icomp_force))
                  real_z_in(ifit_dep,ifit_dist)=real(disp_elas_DSM(idGreen,icomp_disp,icomp_force))
                else
                  imag_z_in(ifit_dep,ifit_dist)=aimag(disp_acous_DSM(idGreen,icomp_disp,icomp_force))
                  real_z_in(ifit_dep,ifit_dist)=real(disp_acous_DSM(idGreen,icomp_disp,icomp_force))
                end if
             end do
          end do
          call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,depths_fit_in,dists_fit_in,imag_z_in,1,&
                                 depth_this_point,dist_this_point,imag_z_out)
          call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,depths_fit_in,dists_fit_in,real_z_in,1,&
                                 depth_this_point,dist_this_point,real_z_out)
          disp_Green_sphe(icomp_disp,icomp_force)=cmplx(real_z_out,imag_z_out)
        end do !icomp_disp
 

        !**************** stress ***********************************
        if(is_elastic(ipoint)) then
          !************  solid media ************
          do icomp1_stress=1,ncomp
             do icomp2_stress=1,ncomp
                do ifit_dep=1,nfit_dep
                   do ifit_dist=1,nfit_dist
                      idGreen=(first_idep_fit_DSM(ipoint,ista)-1 + ifit_dep-1)*ndist_DSM + &
                         first_idist_fit_DSM(ipoint,ista)+ifit_dist-1

                      imag_z_in(ifit_dep,ifit_dist)=aimag(stress_DSM(idGreen,&
                                             icomp_sigma_table(icomp1_stress,icomp2_stress),icomp_force))
                      real_z_in(ifit_dep,ifit_dist)=real(stress_DSM(idGreen, &
                                             icomp_sigma_table(icomp1_stress,icomp2_stress),icomp_force))
                   end do
                end do
                call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,depths_fit_in,dists_fit_in,imag_z_in,1,&
                                    depth_this_point,dist_this_point,imag_z_out)
                call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,depths_fit_in,dists_fit_in,real_z_in,1,&
                                    depth_this_point,dist_this_point,real_z_out)
                stress_sphe(icomp1_stress,icomp2_stress,icomp_force)=cmplx(real_z_out,imag_z_out)
                !stress_sphe(icomp1,icomp2,icomp_force)= cmplx(0.d0)
             end do !icomp2_stress
          end do !icomp1_stress
        
        else
          !************  fluid media ************
          do ifit_dep=1,nfit_dep
             do ifit_dist=1,nfit_dist
                idGreen=(first_idep_fit_DSM(ipoint,ista)-1 + ifit_dep-1)*ndist_DSM + &
                         first_idist_fit_DSM(ipoint,ista)+ifit_dist-1

                imag_z_in(ifit_dep,ifit_dist)=aimag(pressure_DSM(idGreen,icomp_force))
                real_z_in(ifit_dep,ifit_dist)=real(pressure_DSM(idGreen,icomp_force))
              end do
           end do
           call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,depths_fit_in,dists_fit_in,imag_z_in,1,&
                               depth_this_point,dist_this_point,imag_z_out)
           call lagrange_interp_2d(nfit_dep-1,nfit_dist-1,depths_fit_in,dists_fit_in,real_z_in,1,&
                               depth_this_point,dist_this_point,real_z_out)
           stress_sphe(:,:,icomp_force)= cmplx(0.d0)
           stress_sphe(1,1,icomp_force)= cmplx(real_z_out,imag_z_out)
           stress_sphe(2,2,icomp_force)=stress_sphe(1,1,icomp_force)
           stress_sphe(3,3,icomp_force)=stress_sphe(1,1,icomp_force)
        end if !is_elastic(ipoint)
     end do !icomp_force


     rot1(:,:)=rot_matrix_bound(:,:,ipoint,ista)
     rot2(:,:)=rot_matrix_station(:,:,ipoint,ista)
     disp_Green_carte=matmul(transpose(rot1),matmul(disp_Green_sphe,rot2))
     traction_Green_carte(:,:)=cmplx(0.d0)
     !call copy_matrix_c(c(1,ipoint),my_c_bound(1,1,1,1))

     call calcu_traction_Green_carte1(traction_Green_carte,rot1,rot2, &
                normal_vector(1,ipoint), &
                stress_sphe(1,1,1),ncomp)

     do icomp_force=1,ncomp
       do icomp1=1,ncomp
          term1=disp_Green_carte(icomp1,icomp_force)*my_traction_bound(icomp1)
          term2=-my_disp_bound(icomp1)*traction_Green_carte(icomp1,icomp_force)
          !term2=cmplx(0.d0,0.d0)
          disp(icomp_force,ifreq,ista)=disp(icomp_force,ifreq,ista)+(term1+term2)*jacobian(ipoint)
       end do
     end do
    end do !ista
 end do  !ipoint

end subroutine do_convolution

subroutine get_locDSM_fit(depth_this_point,dist_this_point,zone_this_point,nfit_dep,nfit_dist,&
                min_dist_DSM,ddist_DSM,ndist_DSM,depths_DSM,zones_DSM,ndepths_DSM,&
                first_idep_fit_DSM,first_idist_fit_DSM)
use coupling_SEM_DSM_par, only: depth_tolerence,myrank
implicit none

double precision, intent(in):: depth_this_point,dist_this_point
double precision, intent(in):: min_dist_DSM,ddist_DSM
integer, intent(in) ::ndepths_DSM,ndist_DSM,nfit_dep,nfit_dist,zone_this_point
integer, dimension(ndepths_DSM),intent(in) ::zones_DSM
double precision, dimension(ndepths_DSM),intent(in) ::depths_DSM
integer, intent(out) ::first_idep_fit_DSM,first_idist_fit_DSM

!local parameters
integer ::idep_search,idep_start_this_zone,idep_end_this_zone,ndepths_this_zone

    ! The depths used for interpolation must be in the same zone as depths_SEM(idepth), because
    !(1) If not, the same two depths on the discontinuity cause NaN in the Lagrange interpolation. 
    !(2) Using depths in the same zone gives more accurate interpolation. 

    ! Two steps to get the interpolating DSS depths
    ! Step 1, find out the first and last DSM detphs of the target zone
    idep_search=1
    idep_start_this_zone=0
    idep_end_this_zone=0
    do while(idep_search.le.ndepths_DSM)
       if(zones_DSM(idep_search).eq.zone_this_point) then
           if(idep_search.eq.1) then
             idep_start_this_zone=idep_search
           else if(idep_search.eq.ndepths_DSM) then
             idep_end_this_zone=idep_search
           else
             if(zones_DSM(idep_search).eq.zone_this_point .and. &
                zones_DSM(idep_search-1).ne.zone_this_point) &
                  idep_start_this_zone=idep_search
             if(zones_DSM(idep_search).eq.zone_this_point .and. &
                zones_DSM(idep_search+1).ne.zone_this_point) &
                  idep_end_this_zone=idep_search
           end if
       end if
       idep_search=idep_search+1
    end do !idep_search
    ndepths_this_zone=idep_end_this_zone-idep_start_this_zone+1
    
    if(ndepths_this_zone.lt.nfit_dep) stop 'Error, the number of DSM depths in the interpolating &
       zone is smaller than nfit_dep!'

    ! Step 2, search in these depths to get the first interpolating DSM depth
    if(abs(depth_this_point-depths_DSM(idep_end_this_zone)).lt.depth_tolerence) then
      first_idep_fit_DSM=idep_end_this_zone
    else
      idep_search=idep_start_this_zone
      do while(depths_DSM(idep_search).le.depth_this_point+depth_tolerence.and.&
             idep_search.le.idep_end_this_zone-1)
        idep_search=idep_search+1
      end do
      first_idep_fit_DSM = idep_search-1
    end if
    ! Shift the estimated depth by -nfit_dep/2 such that depths_SEM(idepth) is at
    ! the middle
    first_idep_fit_DSM = first_idep_fit_DSM - int(nfit_dep/2.0)
    if(first_idep_fit_DSM.lt.idep_start_this_zone) first_idep_fit_DSM=idep_start_this_zone
    if(first_idep_fit_DSM.gt.idep_end_this_zone-nfit_dep+1) &
         first_idep_fit_DSM=idep_end_this_zone-nfit_dep+1

    ! Find the first distance index to be used for interpolation.
    ! Near zero distance, DSM Green's functions are evaluated from the first
    ! available positive distances only; this avoids requesting samples below
    ! the table while still letting the interpolant/extrapolant approach zero.
    if(dist_this_point.le.min_dist_DSM) then
      first_idist_fit_DSM=1
    else
      first_idist_fit_DSM=int((dist_this_point-min_dist_DSM)/ddist_DSM)+1 - int(nfit_dist/2) + 1
      if(first_idist_fit_DSM>ndist_DSM-nfit_dist+1) first_idist_fit_DSM=ndist_DSM-nfit_dist+1
      if(first_idist_fit_DSM<1) first_idist_fit_DSM=1
    end if
end subroutine

subroutine copy_matrix_c(c,my_c_bound)

use constants
implicit none

double precision, dimension(N_ELAS_COEF),intent(in) ::c
double precision,dimension(ncomp,ncomp,ncomp,ncomp),intent(out) ::my_c_bound
!other variables
integer ::i,j,k,l
integer ::index1,index2
!integer ::index_temp
integer, dimension(ncomp,ncomp) ::index_table
integer, dimension(6,6) ::table_c66
data index_table  /1,6,5,6,2,4,5,4,3/
data table_c66 /1,2,3,4,5,6,&
                2,7,8,9,10,11,&
                3,8,12,13,14,15,&
                4,9,13,16,17,18,&
                5,10,14,17,19,20,&
                6,11,15,18,20,21/
!       [1 5 6]        [11 12 13]
!       [5 2 4] =      [21 22 23]
!       [6 4 3]        [31 32 33]

do i=1,ncomp
  do j=1,ncomp
    index1=index_table(i,j)
    do k=1,ncomp
      do l=1,ncomp
        index2=index_table(k,l)
!        if(index1.gt.index2) then
!             index_temp=index1
!             index1=index2
!             index2=index_temp
!        end if
        my_c_bound(i,j,k,l)=c(table_c66(index1,index2))
      end do
    end do
  end do
end do


end subroutine





subroutine calcu_traction_Green_carte(traction_Green_carte,rot1,rot2, &
               c,normal_vector,stress_sphe,ncomp)
   implicit none

!input/output
   integer ::ncomp
   complex(kind=8),dimension(ncomp,ncomp),intent(out) ::traction_Green_carte
   double precision,dimension(ncomp,ncomp),intent(in) ::rot1,rot2
   complex(kind=8),dimension(ncomp,ncomp,ncomp),intent(in) ::stress_sphe
   double precision,dimension(ncomp,ncomp,ncomp,ncomp),intent(in) ::c
   double precision, dimension(ncomp),intent(in) ::normal_vector
!other variables
   integer ::i,j,k,l,n
   complex(kind=8),dimension(ncomp,ncomp) ::temp_sphe,temp_carte
   complex(kind=8),dimension(ncomp,ncomp,ncomp) ::stress_cartesian
   complex(kind=8),dimension(ncomp,ncomp) ::traction_temp
   double precision ::devia_factor


   do n=1,ncomp
    temp_sphe(:,:)=stress_sphe(:,:,n)
    temp_carte=matmul(transpose(rot1),matmul(temp_sphe,rot1))
    stress_cartesian(:,:,n)=temp_carte(:,:)
   end do
   traction_temp(:,:)=cmplx(0.d0)
   do i=1,ncomp
    do n=1,ncomp
     do j=1,ncomp
      do k=1,ncomp
       do l=1,ncomp
         if(k.ne.l) then
            devia_factor=1.0
         else
            devia_factor=1.0
         end if
         traction_temp(i,n)=traction_temp(i,n)+c(i,j,k,l)*normal_vector(j)*&
                            stress_cartesian(k,l,n)*devia_factor
       end do
      end do
     end do
    end do
   end do
 
   traction_Green_carte=matmul(traction_temp,rot2)

end subroutine

subroutine calcu_traction_Green_carte1(traction_Green_carte,rot1,rot2, &
               normal_vector,stress_sphe,ncomp)
   implicit none

!input/output
   integer ::ncomp
   complex(kind=8),dimension(ncomp,ncomp),intent(out) ::traction_Green_carte
   double precision,dimension(ncomp,ncomp),intent(in) ::rot1,rot2
   complex(kind=8),dimension(ncomp,ncomp,ncomp),intent(in) ::stress_sphe
   double precision, dimension(ncomp),intent(in) ::normal_vector
!other variables
   integer ::i,j,n
   complex(kind=8),dimension(ncomp,ncomp) ::temp_sphe,temp_carte
   complex(kind=8),dimension(ncomp,ncomp,ncomp) ::stress_cartesian
   complex(kind=8),dimension(ncomp,ncomp) ::traction_temp


   do n=1,ncomp
    temp_sphe(:,:)=stress_sphe(:,:,n)
    temp_carte=matmul(transpose(rot1),matmul(temp_sphe,rot1))
    stress_cartesian(:,:,n)=temp_carte(:,:)
   end do
   traction_temp(:,:)=cmplx(0.d0)
   do i=1,ncomp
    do n=1,ncomp
      do j=1,ncomp
         traction_temp(i,n)=traction_temp(i,n)+normal_vector(j)*&
                            stress_cartesian(i,j,n)
      end do
    end do
   end do

   traction_Green_carte=matmul(traction_temp,rot2)

end subroutine

