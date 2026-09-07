subroutine calcu_rotmatrix_idGreen ()
   use convolution_par
   use constants
   use coupling_SEM_DSM_par,only:nproc,myrank,npack,fft_disp_bound_new,fft_traction_bound_new,&
            normal_vector,coord_bound,c,jacobian,NPOINTS,id_depth_SEM,is_elastic,&
            depths_solid_SEM,depths_fluid_SEM,depths_elas_DSM,ndep_elas_DSM,depths_acous_DSM,&
            ndep_acous_DSM,min_dist_DSM,max_dist_DSM,depth_tolerence
   implicit none

!other variables
   integer ::ista,ipoint,icomp
   double precision,dimension(ncomp) ::vector_z_bound,vector_r_bound,vector_t_bound
   double precision,dimension(ncomp) ::vector_z_sta,vector_r_sta,vector_t_sta
   double precision ::theta,phi,radius,depth
   double precision ::cos_gcarc,gcarc
   double precision ::modu_temp
   double precision, parameter ::mtokm=0.001d0


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

!To be removed
!      if(idep.gt.-8.and.idep.lt.1) idep=1

      do icomp=1,ncomp
         vector_z_bound(icomp)=vector_z_bound(icomp)/radius
      end do
!      statopoint(:)=station_coord(:,ista)-vector_r
      vector_t_bound(1)= station_coord(2,ista)*vector_z_bound(3) - &
                          station_coord(3,ista)*vector_z_bound(2)
      vector_t_bound(2)= -station_coord(1,ista)*vector_z_bound(3)+&
                          station_coord(3,ista)*vector_z_bound(1)
      vector_t_bound(3)=  station_coord(1,ista)*vector_z_bound(2)- &
                          station_coord(2,ista)*vector_z_bound(1)
      modu_temp=dsqrt(dot_product(vector_t_bound,vector_t_bound))
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
      gcarc=dacos(cos_gcarc)*radtodeg
      distance(ipoint,ista)=gcarc

      !check validity
      !The SEM depth be interpolated must be within the DSM
      !database range.
      if(is_elastic(ipoint)) then
          !depth_this_point=depths_solid_SEM(id_depth_SEM(ipoint))
          if(abs(depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))).gt.depth_tolerence) then
!              print *,'depth_SEM',depth,depths_solid_SEM(id_depth_SEM(ipoint)),id_depth_SEM(ipoint)
!              print *,'abs_dif',abs(depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))),depth_tolerence,&
!                    depth,depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))
              stop 'Error, (solid) depth is not consistent with the SEM depth table!'
          end if

          if(depth*mtokm-depths_elas_DSM(1).lt.-depth_tolerence.or. &
             depth*mtokm-depths_elas_DSM(ndep_elas_DSM).gt.depth_tolerence) then
!              print *,'depth_DSM',depth,depths_elas_DSM(1),depths_elas_DSM(ndep_elas_DSM)

             stop 'Error, solid SEM depth is out of the DSM depth range!'
          end if
       else
          !depth_this_point=depths_fluid_SEM(id_depth_SEM(ipoint))
          if(abs(depth*mtokm-depths_fluid_SEM(id_depth_SEM(ipoint))).gt.depth_tolerence) &
              stop 'Error, (fluid) depth is not consistent with the SEM depth table!'
          if(depth*mtokm-depths_acous_DSM(1).lt.-depth_tolerence.or. &
             depth*mtokm-depths_acous_DSM(ndep_acous_DSM).gt.depth_tolerence) &
             stop 'Error, fluid SEM depth is out of the DSM depth range!'
       end if
       
       !as well as the distance
       if(distance(ipoint,ista).lt.min_dist_DSM.or. &
          distance(ipoint,ista).gt.max_dist_DSM) &
           print *,'distance',distance(ipoint,ista),min_dist_DSM,max_dist_DSM
       if(distance(ipoint,ista).lt.min_dist_DSM.or. &
          distance(ipoint,ista).gt.max_dist_DSM) &
         stop 'Error, SEM distance is out of the DSM distance range!'

!      if(myrank.eq.0) print *,'id_Green',gcarc,igcarc,id_Green(ipoint,ista)

    end do
   end do


end subroutine calcu_rotmatrix_idGreen


subroutine calcu_rotmatrix_SrcBox()
   use convolution_par
   use constants
   use coupling_SEM_DSM_par,only:nproc,myrank,npack,fft_disp_bound_new,fft_traction_bound_new,&
            normal_vector,coord_bound,c,jacobian,NPOINTS,id_depth_SEM,is_elastic,&
            depths_solid_SEM,depths_fluid_SEM,depths_elas_DSM_toSrc,ndep_elas_DSM_toSrc,depths_acous_DSM_toSrc,&
            ndep_acous_DSM_toSrc,min_dist_DSM_toSrc,max_dist_DSM_toSrc,depth_tolerence
   implicit none

!other variables
   integer ::ipoint,icomp
   double precision,dimension(ncomp) ::vector_z_bound,vector_r_bound,vector_t_bound
   double precision,dimension(ncomp) ::vector_z_src,vector_r_src,vector_t_src
   double precision ::theta,phi,radius,depth
   double precision ::cos_gcarc,gcarc
   double precision ::modu_temp
   double precision, dimension(ncomp) ::src_coord
   double precision, parameter ::mtokm=0.001d0


    theta=pi/2.d0-source_lat*degtorad
    phi=source_lon*degtorad
    src_coord(1)=dsin(theta)*dcos(phi)
    src_coord(2)=dsin(theta)*dsin(phi)
    src_coord(3)=dcos(theta)
    vector_z_src(:)=src_coord(:)
    do ipoint=1,npoints
      radius=dsqrt(dot_product(coord_bound(:,ipoint),coord_bound(:,ipoint)))
      vector_z_bound(:)=coord_bound(:,ipoint)
      depth=r_earth-radius

!To be removed
!      if(idep.gt.-8.and.idep.lt.1) idep=1

      do icomp=1,ncomp
         vector_z_bound(icomp)=vector_z_bound(icomp)/radius
      end do

      vector_t_bound(1)= src_coord(2)*vector_z_bound(3) - &
                          src_coord(3)*vector_z_bound(2)
      vector_t_bound(2)= -src_coord(1)*vector_z_bound(3)+&
                          src_coord(3)*vector_z_bound(1)
      vector_t_bound(3)=  src_coord(1)*vector_z_bound(2)- &
                          src_coord(2)*vector_z_bound(1)
      modu_temp=dsqrt(dot_product(vector_t_bound,vector_t_bound))
      do icomp=1,ncomp
        vector_t_bound(icomp)=vector_t_bound(icomp)/modu_temp
      end do
      vector_t_src(:)=vector_t_bound(:)

      vector_r_bound(1)=vector_t_bound(2)*vector_z_bound(3)- &
                        vector_t_bound(3)*vector_z_bound(2)
      vector_r_bound(2)=-vector_t_bound(1)*vector_z_bound(3)+ &
                        vector_t_bound(3)*vector_z_bound(1)
      vector_r_bound(3)=vector_t_bound(1)*vector_z_bound(2)- &
                        vector_t_bound(2)*vector_z_bound(1)
      vector_r_src(1)=vector_t_src(2)*vector_z_src(3)- &
                        vector_t_src(3)*vector_z_src(2)
      vector_r_src(2)=-vector_t_src(1)*vector_z_src(3)+ &
                        vector_t_src(3)*vector_z_src(1)
      vector_r_src(3)=vector_t_src(1)*vector_z_src(2)- &
                        vector_t_src(2)*vector_z_src(1)

      rot_matrix_bound_toSrc(1,:,ipoint)=vector_z_bound(:)
      rot_matrix_bound_toSrc(2,:,ipoint)=vector_r_bound(:)
      rot_matrix_bound_toSrc(3,:,ipoint)=vector_t_bound(:)


      cos_gcarc=dot_product(vector_z_src,vector_z_bound)
      gcarc=dacos(cos_gcarc)*radtodeg
      distance_toSrc(ipoint)=gcarc
      if(myrank.eq.40.and.ipoint.eq.3) print *,'dist_Src',vector_z_src,vector_z_bound,gcarc

      !check validity
      !The SEM depth be interpolated must be within the DSM
      !database range.
      if(is_elastic(ipoint)) then
          !depth_this_point=depths_solid_SEM(id_depth_SEM(ipoint))
          if(abs(depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))).gt.depth_tolerence) then
!              print
!              *,'depth_SEM',depth,depths_solid_SEM(id_depth_SEM(ipoint)),id_depth_SEM(ipoint)
!              print
!              *,'abs_dif',abs(depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))),depth_tolerence,&
!                    depth,depth*mtokm-depths_solid_SEM(id_depth_SEM(ipoint))
              stop 'Error, (solid) depth is not consistent with the SEM depth table!'
          end if

          if(depth*mtokm-depths_elas_DSM_toSrc(1).lt.-depth_tolerence.or. &
             depth*mtokm-depths_elas_DSM_toSrc(ndep_elas_DSM_toSrc).gt.depth_tolerence) then
!              print
!              *,'depth_DSM',depth,depths_elas_DSM_toSrc(1),depths_elas_DSM_toSrc(ndep_elas_DSM)

             stop 'Error, solid SEM depth is out of the DSM_source depth range!'
          end if
       else
          !depth_this_point=depths_fluid_SEM(id_depth_SEM(ipoint))
          if(abs(depth*mtokm-depths_fluid_SEM(id_depth_SEM(ipoint))).gt.depth_tolerence) &
              stop 'Error, (fluid) depth is not consistent with the SEM depth table!'
          if(depth*mtokm-depths_acous_DSM_toSrc(1).lt.-depth_tolerence.or. &
             depth*mtokm-depths_acous_DSM_toSrc(ndep_acous_DSM_toSrc).gt.depth_tolerence) &
             stop 'Error, fluid SEM depth is out of the DSM_source depth range!'
       end if

       !as well as the distance
       if(distance_toSrc(ipoint).lt.min_dist_DSM_toSrc.or. &
          distance_toSrc(ipoint).gt.max_dist_DSM_toSrc) &
          print *,'dist_source',distance_toSrc(ipoint),min_dist_DSM_toSrc,max_dist_DSM_toSrc
       if(distance_toSrc(ipoint).lt.min_dist_DSM_toSrc.or. &
          distance_toSrc(ipoint).gt.max_dist_DSM_toSrc) &
         stop 'Error, SEM distance is out of the DSM_source distance range!'

!      if(myrank.eq.0) print *,'id_Green',gcarc,igcarc,id_Green(ipoint,ista)

    end do


end subroutine calcu_rotmatrix_SrcBox
