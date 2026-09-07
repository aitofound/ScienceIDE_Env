subroutine calcu_rotmatrix_idGreen ()
   use convolution_par
   use constants
   use coupling_SEM_DSM_par,only:nproc,myrank,npack,fft_disp_bound_new,fft_traction_bound_new,&
            normal_vector,coord_bound,c,jacobian,npoints,id_depth_SEM,is_elastic,&
            depths_solid_SEM,depths_fluid_SEM,depths_elas_DSM,ndep_elas_DSM,depths_acous_DSM,&
            ndep_acous_DSM,min_dist_DSM,max_dist_DSM,ddist_DSM,depth_tolerence,nfit_dist
   implicit none

!other variables
   integer ::ista,ipoint,icomp
   double precision,dimension(ncomp) ::vector_z_bound,vector_r_bound,vector_t_bound
   double precision,dimension(ncomp) ::vector_z_sta,vector_r_sta,vector_t_sta
   double precision ::theta,phi,radius,depth
   double precision ::cos_gcarc,gcarc
   double precision ::modu_temp,lower_dist_tolerance,upper_dist_tolerance
   double precision, parameter ::mtokm=0.001d0


   lower_dist_tolerance=dble(nfit_dist)*ddist_DSM
   upper_dist_tolerance=dble(nfit_dist)*ddist_DSM

   do ista=1,nstation
    theta=pi/2.d0-station_lat(ista)*degtorad
    phi=station_lon(ista)*degtorad
    station_coord(1,ista)=dsin(theta)*dcos(phi)
    station_coord(2,ista)=dsin(theta)*dsin(phi)
    station_coord(3,ista)=dcos(theta)
    vector_z_sta(:)=station_coord(:,ista)
    do ipoint=1,npoints
      radius=dsqrt(dot_product(coord_bound(:,ipoint),coord_bound(:,ipoint)))
      vector_z_bound(:)=coord_bound(:,ipoint)
      depth=r_earth-radius

      do icomp=1,ncomp
         vector_z_bound(icomp)=vector_z_bound(icomp)/radius
      end do
      vector_t_bound(1)= station_coord(2,ista)*vector_z_bound(3) - &
                          station_coord(3,ista)*vector_z_bound(2)
      vector_t_bound(2)= -station_coord(1,ista)*vector_z_bound(3)+&
                          station_coord(3,ista)*vector_z_bound(1)
      vector_t_bound(3)=  station_coord(1,ista)*vector_z_bound(2)- &
                          station_coord(2,ista)*vector_z_bound(1)
      modu_temp=dsqrt(dot_product(vector_t_bound,vector_t_bound))
      if(modu_temp.lt.1.d-12) then
        ! At zero distance the transverse direction is undefined.  Choose a finite
        ! local tangent so the coupling can still use the one-sided DSM distance stencil.
        vector_t_bound(1)=-vector_z_bound(2)
        vector_t_bound(2)= vector_z_bound(1)
        vector_t_bound(3)= 0.d0
        modu_temp=dsqrt(dot_product(vector_t_bound,vector_t_bound))
        if(modu_temp.lt.1.d-12) then
          vector_t_bound(1)=0.d0
          vector_t_bound(2)=1.d0
          vector_t_bound(3)=0.d0
          modu_temp=1.d0
        end if
      end if
      do icomp=1,ncomp
        vector_t_bound(icomp)=vector_t_bound(icomp)/modu_temp
      end do
      vector_t_sta(:)=vector_t_bound(:)

      vector_r_bound(1)=vector_t_bound(2)*vector_z_bound(3)- &
                        vector_t_bound(3)*vector_z_bound(2)
      vector_r_bound(2)=-vector_t_bound(1)*vector_z_bound(3)+ &
                        vector_t_bound(3)*vector_z_bound(1)
      vector_r_bound(3)=vector_t_bound(1)*vector_z_bound(2)- &
                        vector_t_bound(2)*vector_z_bound(1)
      vector_r_sta(1)=vector_t_sta(2)*vector_z_sta(3)- &
                        vector_t_sta(3)*vector_z_sta(2)
      vector_r_sta(2)=-vector_t_sta(1)*vector_z_sta(3)+ &
                        vector_t_sta(3)*vector_z_sta(1)
      vector_r_sta(3)=vector_t_sta(1)*vector_z_sta(2)- &
                        vector_t_sta(2)*vector_z_sta(1)

      rot_matrix_bound(1,:,ipoint,ista)=vector_z_bound(:)
      rot_matrix_bound(2,:,ipoint,ista)=vector_r_bound(:)
      rot_matrix_bound(3,:,ipoint,ista)=vector_t_bound(:)

      rot_matrix_station(1,:,ipoint,ista)=vector_z_sta(:)
      rot_matrix_station(2,:,ipoint,ista)=vector_r_sta(:)
      rot_matrix_station(3,:,ipoint,ista)=vector_t_sta(:)


      cos_gcarc=dot_product(vector_z_sta,vector_z_bound)
      if(cos_gcarc.gt.1.d0) cos_gcarc=1.d0
      if(cos_gcarc.lt.-1.d0) cos_gcarc=-1.d0
      gcarc=dacos(cos_gcarc)*radtodeg
      distance(ipoint,ista)=gcarc

      ! Check validity
      ! The SEM depth interpolated must be within the DSM
      ! database range.
      if(is_elastic(ipoint)) then
          if(abs(depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))).gt.depth_tolerence) then
              print *,'depth_SEM',depth,depths_solid_SEM(id_depth_SEM(ipoint)),id_depth_SEM(ipoint),coord_bound(:,ipoint),&
                 ipoint,myrank
              print *,'abs_dif',abs(depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))),depth_tolerence,&
                    depth,depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))
              stop 'Error, (solid) depth is not consistent with the SEM depth table!'
          end if

          if(depth*mtokm-depths_elas_DSM(1).lt.-depth_tolerence.or. &
             depth*mtokm-depths_elas_DSM(ndep_elas_DSM).gt.depth_tolerence) then
!              print *,'depth_DSM',depth,depths_elas_DSM(1),depths_elas_DSM(ndep_elas_DSM)

             stop 'Error, solid SEM depth is out of the DSM depth range!'
          end if
       else
          if(abs(depth*mtokm-depths_fluid_SEM(id_depth_SEM(ipoint))).gt.depth_tolerence) &
              stop 'Error, (fluid) depth is not consistent with the SEM depth table!'
          if(depth*mtokm-depths_acous_DSM(1).lt.-depth_tolerence.or. &
             depth*mtokm-depths_acous_DSM(ndep_acous_DSM).gt.depth_tolerence) &
             stop 'Error, fluid SEM depth is out of the DSM depth range!'
       end if
       
       ! As well as the distance.  DSM tables may intentionally start at a
       ! small positive distance to avoid singular Green's functions at zero
       ! degrees.  Allow near-zero SEM distances below the table minimum;
       ! do_convolution will use the first/right-sided DSM distance stencil.
       if(distance(ipoint,ista).lt.min_dist_DSM-lower_dist_tolerance.or. &
          distance(ipoint,ista).gt.max_dist_DSM+upper_dist_tolerance) &
         stop 'Error, SEM distance is out of the DSM distance range!'
    end do
   end do


end subroutine calcu_rotmatrix_idGreen
