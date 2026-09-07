! construct and save the depth and distance tables for the inner coupling
! boundary
subroutine save_depth_id_inner(myrank,NPROC,nele_tele_coupling,&
                     ispec_is_elastic,&
                     ispec_is_acoustic,nspec,ibool,nglob,xstore_dummy,&
                     ystore_dummy,zstore_dummy,media_type,&
                     IMAIN_OUTPUT,&
                     prname,LOCAL_PATH)
  use tomography, only:rmin_structure_zone,n_structure_zone
  use tele_coupling_par, only:coupling_ele_property,LOW_RESOLUTION,id_depth_coupling,id_dist_coupling,&
          COUPLING_DEPTH_TOLERENCE,COUPLING_DIST_TOLERENCE
  use constants
  implicit none

!  include "constants.h"

  integer ::myrank,NPROC,media_type
  integer :: nele_tele_coupling
  integer :: IMAIN_OUTPUT

  integer nspec,nglob
! mesh coordinates
  integer, dimension(NGLLX,NGLLY,NGLLZ,nspec) :: ibool
  real(kind=CUSTOM_REAL), dimension(nglob) :: xstore_dummy,ystore_dummy,zstore_dummy
!  double precision, dimension(NGLLZ) ::wzgll

  logical, dimension(nspec) :: ispec_is_acoustic,ispec_is_elastic
  character(len=256) prname,LOCAL_PATH


!other parameters
  integer ::npoints_coupling,ipoint,ipoint_iface,ipoint_temp,ipoint_allmedia
  integer ::max_ndep_global,ndepth_table,&
            idepth,ndist_table,idist
  integer ::num_faces_this_media_type
  integer ::size_depth_coupling,size_dist_coupling,max_ndist_iproc,max_ndist_allproc
  integer ::max_ndist_global
  integer, allocatable,dimension(:) ::ndist
  real(kind=CUSTOM_REAL) ::r_thisdepth,r_within_mesh
  integer ::izone,idiscontinuity,iglob_within_mesh
  integer,allocatable,dimension(:) ::depth_id,dist_id
  integer,allocatable,dimension(:) ::face_coupling
  real(kind=CUSTOM_REAL) ::dist_rad
  real(kind=CUSTOM_REAL) ::coupling_min_dist_degree
  real(kind=CUSTOM_REAL), allocatable, dimension(:) ::depth_coupling,dist_coupling
  real(kind=CUSTOM_REAL), allocatable, dimension(:) ::depth_table_global,dist_table_global
  integer, allocatable, dimension(:) ::izone_dep_table,move_idepth,discon_idepth
  integer ::ndiscon_found_deptable
  integer ::ndep_discon_acount
  real(kind=CUSTOM_REAL), allocatable, dimension(:) ::dep_table_discon_acount
  integer, allocatable, dimension(:) ::izone_dep_discon_acount
  integer,dimension(:),allocatable ::idepth_is_discont
  real(kind=CUSTOM_REAL), allocatable, dimension(:) ::dist_thisdepth
  integer,allocatable,dimension(:) ::dist_id_thisdepth
  double precision ::x,y,z,r
  
  double precision,dimension(3) ::coord_sta,coord_coupling
  integer ::i,j,k,igll,iface,iface_this_media,iface_temp,ispec,iglob
  integer ::imax,imin,jmax,jmin,kmax,kmin,di,dj,dk

! parameters for sending and receiving
  integer::iproc,req_send,req_recv

! teleseismic station
  double precision :: lat_sta(1),long_sta(1)
!  double precision :: depth_sta(1)
  double precision ::theta_sta,phi_sta


  character(len=256) file_name
!  character(len=256) clean_LOCAL_PATH
  character(len=27) :: id_depth_elas_file
  character(len=28) :: id_depth_acous_file
  data id_depth_elas_file /'id_depth_elastic_inner'/
  data id_depth_acous_file /'id_depth_acoustic_inner'/

  character(len=29) :: dist_table_elas_file
  character(len=30) :: dist_table_acous_file
  data dist_table_elas_file /'dist_table_elastic_inner'/
  data dist_table_acous_file /'dist_table_acoustic_inner'/

  character(len=30) :: depth_table_elas_file
  character(len=31) :: depth_table_acous_file
  data depth_table_elas_file /'depth_table_elastic_inner'/
  data depth_table_acous_file /'depth_table_acoustic_inner'/

  integer ::int_tmp

!The below two lines are used to avoid possible error reports during compling
!the code. IMAIN_OUTPUT and wzgll are not used now, that occationally causes error 
!reports. But they might be usful in the future, so we keep them here.
  int_tmp=IMAIN_OUTPUT


  num_faces_this_media_type=0
  do iface=1,nele_tele_coupling
     ispec = coupling_ele_property(iface)%ispec_coupling
!    if(DEBUG_COUPLING) print *,'iface',iface,ispec_is_elastic(ispec)
     if(ispec_is_acoustic(ispec).and.media_type.eq.2) then
        num_faces_this_media_type=num_faces_this_media_type+1
     else if(ispec_is_elastic(ispec).and.media_type.eq.1) then
        num_faces_this_media_type=num_faces_this_media_type+1
     end if
  end do
  if(DEBUG_COUPLING) print *,'num_faces_this_media_type',num_faces_this_media_type
  if(num_faces_this_media_type.gt.0) then
     allocate(face_coupling(num_faces_this_media_type))
  else 
       if(media_type.eq.1) then
         file_name = prname(1:len_trim(prname))//id_depth_elas_file(1:len_trim(id_depth_elas_file))
       else
         file_name = prname(1:len_trim(prname))//id_depth_acous_file(1:len_trim(id_depth_acous_file))
       end if

     open(unit=16,file=file_name(1:len_trim(file_name)),& 
          status='unknown',action='write',form='formatted')

     write(16,*) 0

     close(16)
  end if
  if(DEBUG_COUPLING) print *,'num_faces_this_media_type',num_faces_this_media_type,media_type
  iface_temp=0
  do iface=1,nele_tele_coupling
     ispec = coupling_ele_property(iface)%ispec_coupling
     if(ispec_is_acoustic(ispec).and.media_type.eq.2) then
        iface_temp=iface_temp+1
        face_coupling(iface_temp)=iface
        if(DEBUG_COUPLING) print *,'acoustic ispec',iface
     else if(ispec_is_elastic(ispec).and.media_type.eq.1) then
        iface_temp=iface_temp+1
        face_coupling(iface_temp)=iface
     else if(.not.ispec_is_acoustic(ispec).and..not.ispec_is_elastic(ispec)) then
        print *,'media type',ispec_is_acoustic(ispec),ispec_is_elastic(ispec),media_type,&
               .not.ispec_is_acoustic(ispec).and..not.ispec_is_acoustic(ispec)
        call exit_MPI(myrank,'unknown or wrong media type')
     end if
  end do


  if(LOW_RESOLUTION) then
      npoints_coupling=num_faces_this_media_type
  else
      npoints_coupling=NGLLX*NGLLY*num_faces_this_media_type
  end if
  if(DEBUG_COUPLING) print *,'depth_list_inner begin',myrank,npoints_coupling

  if(npoints_coupling.gt.0) then
     size_depth_coupling=npoints_coupling
     size_dist_coupling=npoints_coupling
  else
     size_depth_coupling=1
     size_dist_coupling=1
  end if
  allocate(depth_coupling(size_depth_coupling))
  allocate(depth_id(size_depth_coupling))
  allocate(dist_coupling(size_dist_coupling))
  allocate(dist_id(size_dist_coupling))

  if(DEBUG_COUPLING) print *,'allocate finished',myrank

  lat_sta(1)=0.0
  long_sta(1)=0.0
  theta_sta = PI/2.0d0 - lat_sta(1)*PI/180.0d0
  phi_sta = long_sta(1)*PI/180.0d0
  coord_sta(1)=dsin(theta_sta)*dcos(phi_sta)
  coord_sta(2)=dsin(theta_sta)*dsin(phi_sta)
  coord_sta(3)=dcos(theta_sta)
  if(DEBUG_COUPLING) print *,'sta done'

  ipoint=0
  do iface_this_media=1,num_faces_this_media_type
     iface=face_coupling(iface_this_media)
     ispec = coupling_ele_property(iface)%ispec_coupling
     imin=coupling_ele_property(iface)%istart;imax=coupling_ele_property(iface)%iend
     jmin=coupling_ele_property(iface)%jstart;jmax=coupling_ele_property(iface)%jend
     kmin=coupling_ele_property(iface)%kstart;kmax=coupling_ele_property(iface)%kend
     di=1;dj=1;dk=1

     ipoint_iface=0
     do k=kmin,kmax,dk
      do j=jmin,jmax,dj
       do i=imin,imax,di
           if(LOW_RESOLUTION) then
               ipoint=iface_this_media
           else
               ipoint_iface=ipoint_iface+1
               ipoint=(iface_this_media-1)*NGLLX*NGLLY+ipoint_iface
           end if


           iglob=ibool(i,j,k,ispec)
           x=xstore_dummy(iglob)
           y=ystore_dummy(iglob)
           z=zstore_dummy(iglob)
           r=dsqrt(x**2+y**2+z**2)
           depth_coupling(ipoint)=R_EARTH_SURF-r        
           coord_coupling(1)=x/r;coord_coupling(2)=y/r;coord_coupling(3)=z/r

           dist_rad=dacos(dot_product(coord_coupling,coord_sta)*(1.d0-TINYVAL))
           dist_coupling(ipoint) = dist_rad*180.d0/PI

           if(depth_coupling(ipoint)<100.and.DEBUG_COUPLING) &
              print *,'check_dist_reslult',iface,igll,x,y,z,dist_rad*180.0/3.1415926,myrank
       end do !i
      end do !j
     end do !k
  end do

if(DEBUG_COUPLING) print *,'MAX_NDEPTH_TABLE',MAX_NDEPTH_TABLE
max_ndep_global=MAX_NDEPTH_TABLE
allocate(depth_table_global(max_ndep_global))

if(DEBUG_COUPLING) print *,'find_global_id'
! construct depth table and get depth id
call find_global_id(depth_coupling,size_depth_coupling,npoints_coupling,COUPLING_DEPTH_TOLERENCE,depth_id,&
                      depth_table_global,ndepth_table,max_ndep_global,&
                      NPROC,myrank)

if(myrank.eq.0.and.DEBUG_COUPLING) print *,'ndepth_table',ndepth_table,depth_table_global(:)

ndiscon_found_deptable=0
if(ndepth_table.gt.0) then


 if(media_type.eq.1) then
     file_name = trim(LOCAL_PATH(1:len_trim(LOCAL_PATH)))// '/' // &
                 trim(dist_table_elas_file(1:len_trim(dist_table_elas_file)))
 else
     file_name = trim(LOCAL_PATH(1:len_trim(LOCAL_PATH)))// '/' //&
                 trim(dist_table_acous_file(1:len_trim(dist_table_acous_file)))
 end if
 if(myrank.eq.0) &
     open(113,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')


  allocate(ndist(ndepth_table))
  ndist(:)=0
  dist_id(:)=0
  max_ndist_iproc=1
  ipoint_temp=0
  do idepth=1,ndepth_table
    do ipoint=1,npoints_coupling
        if(depth_id(ipoint).eq.idepth) then
             ndist(idepth)=ndist(idepth)+1
             ipoint_temp=ipoint_temp+1
        end if
    end do
    if(ndist(idepth).gt.max_ndist_iproc) max_ndist_iproc=ndist(idepth)
  end do
  if(npoints_coupling.gt.0.and.ipoint_temp.ne.npoints_coupling) then
        print *,myrank,"Error counting coupling points",ipoint_temp,npoints_coupling
        call exit_MPI(myrank,'Error in counting coupling points')
  end if

  allocate(dist_thisdepth(max_ndist_iproc))
  allocate(dist_id_thisdepth(max_ndist_iproc))
  call  max_all_i(max_ndist_iproc,max_ndist_allproc)

  if(myrank.eq.0) then
    max_ndist_global=NPROC*max_ndist_allproc
  else 
    max_ndist_global=1
  end if
  allocate(dist_table_global(max_ndist_global))


  if(DEBUG_COUPLING) print *,'discon_start',myrank


! take the conscontinuity into account
  allocate(move_idepth(ndepth_table))
  allocate(idepth_is_discont(ndepth_table))
 if(myrank.eq.0) then
  allocate(izone_dep_table(ndepth_table))
  allocate(discon_idepth(ndepth_table))
  discon_idepth(:)=0
  izone_dep_table(:)=0
  move_idepth(:)=0
  ndiscon_found_deptable=0
  idepth_is_discont(:)=0
  do idepth=1,ndepth_table
    r_thisdepth=R_EARTH_SURF-depth_table_global(idepth)
    if(idepth.eq.1) r_thisdepth=r_thisdepth-0.5*COUPLING_DEPTH_TOLERENCE
    if(idepth.eq.ndepth_table) r_thisdepth=r_thisdepth+0.5*COUPLING_DEPTH_TOLERENCE
    do izone=1,n_structure_zone
      if(rmin_structure_zone(izone).le.r_thisdepth) then
         izone_dep_table(idepth)=izone
      end if
      if(idepth.eq.25.and.izone.eq.10.and.DEBUG_COUPLING) print *,'discon detect',&
            rmin_structure_zone(izone),r_thisdepth,depth_table_global(:)
      if(dabs(rmin_structure_zone(izone)-r_thisdepth).lt.0.5*COUPLING_DEPTH_TOLERENCE.and.&
         idepth.gt.1.and.idepth.lt.ndepth_table) then
         idepth_is_discont(idepth)=1
         ndiscon_found_deptable=ndiscon_found_deptable+1
         discon_idepth(ndiscon_found_deptable)=idepth
         izone_dep_table(idepth)=izone
         move_idepth(idepth:ndepth_table)=move_idepth(idepth:ndepth_table)+1
      end if
    end do
  end do
  if(izone_dep_table(1)-izone_dep_table(ndepth_table).ne.ndiscon_found_deptable.and.&
     .not.LOW_RESOLUTION) then
      print *,'check_izone',izone_dep_table(1),izone_dep_table(ndepth_table),&
            ndepth_table,ndiscon_found_deptable
      stop 'Error of discontinuity setting or depth table (inner coupling boundary)'
  end if
  ndep_discon_acount=ndepth_table+ndiscon_found_deptable

  allocate(dep_table_discon_acount(ndep_discon_acount))
  allocate(izone_dep_discon_acount(ndep_discon_acount))
  do idepth=1,ndepth_table
    dep_table_discon_acount(idepth+move_idepth(idepth))=depth_table_global(idepth)
    izone_dep_discon_acount(idepth+move_idepth(idepth))=izone_dep_table(idepth)
  end do


  do idiscontinuity=1,ndiscon_found_deptable
     dep_table_discon_acount(discon_idepth(idiscontinuity)+idiscontinuity-1)= &
          depth_table_global(discon_idepth(idiscontinuity))
     izone_dep_discon_acount(discon_idepth(idiscontinuity)+idiscontinuity-1)= &
          izone_dep_discon_acount(discon_idepth(idiscontinuity))
     izone_dep_discon_acount(discon_idepth(idiscontinuity)+idiscontinuity)= &
          izone_dep_discon_acount(discon_idepth(idiscontinuity))+1
  end do
 end if



  if(myrank.eq.0) then
! send ndep_discon_acount, discon_idepth and  arrays
    do iproc=1,NPROC-1

           req_send=iproc+1000
           call isend_i(idepth_is_discont(1),ndepth_table,&
                         iproc,1,req_send)
           call wait_req(req_send)
           req_send=iproc+1001
           call isend_i(move_idepth(1),ndepth_table,&
                         iproc,1,req_send)
           call wait_req(req_send)
    end do
  else
! receive the arrays
            req_recv=myrank+1000
            call irecv_i(idepth_is_discont(1),ndepth_table,&
                                        0,1,req_recv)
            call wait_req(req_recv)

            req_recv=myrank+1001
            call irecv_i(move_idepth(1),ndepth_table,&
                                        0,1,req_recv)
            call wait_req(req_recv)
  end if


 ! construct distance table and get distance id
! Clamp near-zero distances before constructing IDs so DSM is never sampled at
! zero distance. The minimum table distance is 2*COUPLING_DIST_TOLERENCE;
! using the same value before find_global_id keeps dist_table and dist_id consistent.
  coupling_min_dist_degree = 2.0_CUSTOM_REAL * real(COUPLING_DIST_TOLERENCE,kind=CUSTOM_REAL)
 do idepth=1,ndepth_table 
    if(DEBUG_COUPLING) print *,'construct distance table',idepth
    idist=0
    do ipoint=1,npoints_coupling
      if(depth_id(ipoint).eq.idepth) then
          idist=idist+1
          dist_thisdepth(idist)=max(dist_coupling(ipoint),coupling_min_dist_degree)
      end if
    end do

    call find_global_id(dist_thisdepth,max_ndist_iproc,ndist(idepth),COUPLING_DIST_TOLERENCE,&
                      dist_id_thisdepth,&
                      dist_table_global,ndist_table,max_ndist_global,&
                      NPROC,myrank)
! copy id
    idist=0
    do ipoint=1,npoints_coupling
      if(depth_id(ipoint).eq.idepth) then
          idist=idist+1
          dist_id(ipoint)=dist_id_thisdepth(idist)
      end if
    end do
    if(idist.ne.ndist(idepth)) then
      call exit_MPI(myrank,'Error in counting distnace of coupling points.')
    end if


! save table
  if(myrank.eq.0) then
    r=R_EARTH_SURF-depth_table_global(idepth)
    write(113,*)ndist_table
    do idist=1,ndist_table
!      write(113,*) dist_table_global(idist)/r*(180.d0/PI)
      write(113,*) dist_table_global(idist)
    end do
    if(idepth_is_discont(idepth).eq.1) then
      write(113,*)ndist_table
      do idist=1,ndist_table
         write(113,*) dist_table_global(idist)
      end do

    end if

  end if
 end do  ! do idepth
 if(myrank.eq.0) close(113)

  if(DEBUG_COUPLING) print *,'move_idepth',move_idepth(:),idepth_is_discont(:)
 

 if(.not.LOW_RESOLUTION) then
  ipoint=0
  do iface_this_media=1,num_faces_this_media_type
     iface=face_coupling(iface_this_media)
     ispec = coupling_ele_property(iface)%ispec_coupling
     imin=coupling_ele_property(iface)%istart;imax=coupling_ele_property(iface)%iend
     jmin=coupling_ele_property(iface)%jstart;jmax=coupling_ele_property(iface)%jend
     kmin=coupling_ele_property(iface)%kstart;kmax=coupling_ele_property(iface)%kend
     di=1;dj=1;dk=1

     ipoint_iface=0
     do k=kmin,kmax,dk
      do j=jmin,jmax,dj
       do i=imin,imax,di
           if(LOW_RESOLUTION) then
               ipoint=iface_this_media
           else
               ipoint_iface=ipoint_iface+1
               ipoint=(iface_this_media-1)*NGLLX*NGLLY+ipoint_iface
           end if
          if(idepth_is_discont(depth_id(ipoint)).eq.1) then
            ispec=coupling_ele_property(iface)%ispec_coupling 
            iglob=ibool(i,j,k,ispec)
            x=xstore_dummy(iglob)
            y=ystore_dummy(iglob)
            z=zstore_dummy(iglob)
            r=dsqrt(x**2+y**2+z**2)

            iglob_within_mesh=ibool(2,2,2,ispec)
            x=xstore_dummy(iglob_within_mesh)
            y=ystore_dummy(iglob_within_mesh)
            z=zstore_dummy(iglob_within_mesh)
            r_within_mesh=dsqrt(x**2+y**2+z**2)


            if(i.lt.NGLLX.and.i.gt.1 .and. j.lt.NGLLY.and.j.gt.1 &
              .and.k.lt.NGLLZ.and.k.gt.1 ) &
              stop 'Error, discontinuity is within this mesh!'

            if(r_within_mesh.lt.r) then
              depth_id(ipoint)=depth_id(ipoint)+move_idepth(depth_id(ipoint))
            else
              depth_id(ipoint)=depth_id(ipoint)+move_idepth(depth_id(ipoint)-1)
            end if
          else
              depth_id(ipoint)=depth_id(ipoint)+move_idepth(depth_id(ipoint))
          end if
       end do !do i
      end do ! do j
     end do !do k
   end do  !iface
  end if !LOW_RESOLUTION


  if(media_type.eq.1) then
     file_name = prname(1:len_trim(prname))//id_depth_elas_file(1:len_trim(id_depth_elas_file))
  else
     file_name = prname(1:len_trim(prname))//id_depth_acous_file(1:len_trim(id_depth_acous_file))
  end if


  open(unit=117,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')

  write(117,*) num_faces_this_media_type
  if(DEBUG_COUPLING) print *,'num_faces',myrank,num_faces_this_media_type,media_type
  ipoint=0
  do iface_this_media=1,num_faces_this_media_type
     iface=face_coupling(iface_this_media)
     ispec = coupling_ele_property(iface)%ispec_coupling
     imin=coupling_ele_property(iface)%istart;imax=coupling_ele_property(iface)%iend
     jmin=coupling_ele_property(iface)%jstart;jmax=coupling_ele_property(iface)%jend
     kmin=coupling_ele_property(iface)%kstart;kmax=coupling_ele_property(iface)%kend
     di=1;dj=1;dk=1

     ipoint_iface=0
     do k=kmin,kmax,dk
      do j=jmin,jmax,dj
       do i=imin,imax,di
          if(LOW_RESOLUTION) then
               ipoint=iface_this_media
               ipoint_allmedia=face_coupling(iface_this_media)
          else
               ipoint_iface=ipoint_iface+1
               ipoint=(iface_this_media-1)*NGLLX*NGLLY+ipoint_iface
               ipoint_allmedia=(iface-1)*NGLLX*NGLLY+ipoint_iface
          end if
          write(117,*) face_coupling(iface_this_media),depth_id(ipoint),dist_id(ipoint),dist_coupling(ipoint)
          id_depth_coupling(ipoint_allmedia)=depth_id(ipoint)
          id_dist_coupling(ipoint_allmedia)=dist_id(ipoint)
       end do
      end do
     end do
  end do

  close(117)


  deallocate(ndist)
  deallocate(dist_thisdepth)
  deallocate(dist_id_thisdepth)
  deallocate(dist_table_global)

end if

 ! save depth table
  if(myrank.eq.0) then
     if(media_type.eq.1) then
       file_name = trim(LOCAL_PATH(1:len_trim(LOCAL_PATH)))// '/' // &
                 trim(depth_table_elas_file(1:len_trim(depth_table_elas_file)))
     else
       file_name = trim(LOCAL_PATH(1:len_trim(LOCAL_PATH)))// '/' // &
                 trim(depth_table_acous_file(1:len_trim(depth_table_acous_file)))
     end if
     open(17,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')
!     write(17,*) 0.02,ndepth_table+ndiscon_found_deptable
!     write(17,*) 0.005
     write(17,*) 0.02,0.005
     write(17,*) ndepth_table+ndiscon_found_deptable

!     write(17,*) NGLLZ

     do idepth=1,ndepth_table
      write(17,*)depth_table_global(idepth)/1000.0,izone_dep_table(idepth)
      if(idepth_is_discont(idepth).eq.1) then
        write(17,*)depth_table_global(idepth)/1000.0,izone_dep_table(idepth)-1
      end if
     end do
     close(17)
  end if

 
! save depth and distance id

  if(num_faces_this_media_type.gt.0) deallocate(face_coupling)

  deallocate(depth_coupling)
  deallocate(depth_id)
  deallocate(depth_table_global)
  deallocate(dist_coupling)
  deallocate(dist_id)
  if(ndepth_table.gt.0) then
    deallocate(move_idepth)
    deallocate(idepth_is_discont)
  end if

  if(myrank.eq.0.and.ndepth_table.gt.0) then
    deallocate(discon_idepth)
    deallocate(izone_dep_table)
    deallocate(izone_dep_discon_acount)
    deallocate(dep_table_discon_acount)
  end if

end subroutine



