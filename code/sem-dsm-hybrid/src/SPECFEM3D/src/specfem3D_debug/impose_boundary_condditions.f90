subroutine init_impose_BCS()
use impose_1D_BCS
use constants
use specfem_par
use specfem_par_elastic
implicit none

integer ::iface,iface_temp
integer,allocatable, dimension(:,:) ::idepth_readin
integer,allocatable, dimension(:,:) ::idist_readin
integer ::idepth_temp,idepth,idistance
!integer ::idepth_min_ela,idepth_max_ela
integer,dimension(:),allocatable::idepth_used_temp1,idepth_used_temp2
integer,dimension(:),allocatable::idist_used_temp1,idist_used_temp2
integer,dimension(:,:),allocatable ::idep_idist_temp1,idep_idist_temp2
!normalized x_source,y_source,z_source
double precision x_source_norm,y_source_norm,z_source_norm
double precision x,y,z,depth
double precision distance

!local parameters
integer ::idepth_test,idist,idist_temp,ndist_temp,ndepth_temp,media_type
integer ::num_faces_this_mediatype
integer ::iepsilon,ipressure,igll,igll_temp,ispec
integer ::i,j,k,iglob
double precision ::dist_temp,ddepth_dsm_tmp,depth_tole_dsm_tmp
integer ::ier
double precision, parameter ::degtorad=PI/180.0
double precision, parameter ::kmtom=1000.0
character(len=256) file_name

character(len=16) :: id_depth_elas_file
character(len=17) :: id_depth_acous_file
data id_depth_elas_file /'id_depth_elastic'/
data id_depth_acous_file /'id_depth_acoustic'/

character(len=18) :: dist_table_elas_file
character(len=19) :: dist_table_acous_file
data dist_table_elas_file /'dist_table_elastic'/
data dist_table_acous_file /'dist_table_acoustic'/

character(len=19) :: depth_table_elas_file
character(len=20) :: depth_table_acous_file
data depth_table_elas_file /'depth_table_elastic'/
data depth_table_acous_file /'depth_table_acoustic'/

allocate(id_epsilon(NGLLSQUARE,num_abs_boundary_faces))
allocate(id_pressure(NGLLSQUARE,num_abs_boundary_faces))
allocate(id_velo(NGLLSQUARE,num_abs_boundary_faces))
allocate(id_poten_dot(NGLLSQUARE,num_abs_boundary_faces))
if(SINGLE_FORCE_ENZ.eq.0) then
    allocate(moment_zrt(6,NGLLSQUARE,num_abs_boundary_faces))
    allocate(force_zrt(3,1,1))
else
    allocate(moment_zrt(6,1,1))
    allocate(force_zrt(3,NGLLSQUARE,num_abs_boundary_faces))
end if
allocate(zrtToxyz(3,3,NGLLSQUARE,num_abs_boundary_faces))
allocate(idep_idist_impose(2,NGLLSQUARE,num_abs_boundary_faces)) 
!allocate temporary array
allocate(idep_idist_temp1(2,NGLLSQUARE*num_abs_boundary_faces))
allocate(idep_idist_temp2(2,NGLLSQUARE*num_abs_boundary_faces))
allocate(idepth_used_temp1(NGLLSQUARE*num_abs_boundary_faces))
allocate(idepth_used_temp2(NGLLSQUARE*num_abs_boundary_faces))
allocate(idist_used_temp1(NGLLSQUARE*num_abs_boundary_faces))
allocate(idist_used_temp2(NGLLSQUARE*num_abs_boundary_faces))

id_epsilon(:,:)=0
id_pressure(:,:)=0
id_velo(:,:)=0
id_poten_dot(:,:)=0
moment_zrt(:,:,:)=0.d0
force_zrt(:,:,:)=0.d0
zrtToxyz(:,:,:,:)=0.d0

idep_idist_temp1(:,:)=0
idep_idist_temp2(:,:)=0
idepth_used_temp1(:)=0
idepth_used_temp2(:)=0
idist_used_temp1(:)=0
idist_used_temp2(:)=0


!call sync_all()
if(DEBUG_COUPLING) print *,"read parameters file"
open(unit=16,file=INJECTED_WAVEFIELD_PATH(1:len_trim(INJECTED_WAVEFIELD_PATH)) &
       //'/InjectedWaves_Par_file',status='old',action='read')

call read_value_double_precision_tele(16,IGNORE_JUNK,inject_dt,'GreenFuncs.inject_dt',ier)
!print *,'inject_dt=',inject_dt
if(ier /= 0) stop 'Error reading Greens function parameter inject_dt'
call read_value_integer_tele(16,IGNORE_JUNK,inject_npt,'GreenFuncs.inject_npt',ier)
!print *,'inject_npt=',inject_npt
if(ier /= 0) stop 'Error reading Greens function parameter inject_npt'
call read_value_integer_tele(16,IGNORE_JUNK,inject_npack,'GreenFuncs.inject_npack',ier)
!print *,'inject_npack=',inject_npack
if(ier /= 0) stop 'Error reading Greens function parameter inject_npack'
call read_value_integer_tele(16,IGNORE_JUNK,inject_npt_eachpack,'GreenFuncs.inject_npt_eachpack',ier)
!print *,'inject_npt_eachpack',inject_npt_eachpack
if(ier /= 0) stop 'Error reading Greens function parameter inject_npt_eachpack'
call read_value_double_precision_tele(16,IGNORE_JUNK,inject_time_start,'GreenFuncs.inject_time_start',ier)
!print *,'inject_time_start',inject_time_start
if(ier /= 0) stop 'Error reading Greens function parameter inject_time_start'
call read_value_double_precision_tele(16,IGNORE_JUNK,inject_time_end,'GreenFuncs.inject_time_end',ier)
!print *,'inject_time_end',inject_time_end
if(ier /= 0) stop 'Error reading Greens function parameter inject_time_end'

close(16)

!call synchronize_all()
if(deltat*(NSTEP-1).gt.(inject_time_end-inject_time_start)-(int(NFIT/2)+1)*inject_dt&
  .and.myrank.eq.0) &
   call exit_MPI(myrank,'Error,The time length to be simulated is longer &
                 & than the inject wavefield.')
!call synchronize_all()

!read the number of solid/fluid injected depths.
file_name = trim(LOCAL_PATH(1:len_trim(LOCAL_PATH)))// '/' //&
            trim(depth_table_elas_file(1:len_trim(depth_table_elas_file)))
open(17,file=file_name(1:len_trim(file_name)),status='unknown',action='read',form='formatted')
read(17,*) ddepth_dsm_tmp,depth_tole_dsm_tmp
read(17,*) inject_ndep_elas
close(17)

file_name = trim(LOCAL_PATH(1:len_trim(LOCAL_PATH)))// '/' //&
                   trim(depth_table_acous_file(1:len_trim(depth_table_acous_file)))
open(17,file=file_name(1:len_trim(file_name)),status='unknown',action='read',form='formatted')
read(17,*) ddepth_dsm_tmp,depth_tole_dsm_tmp
read(17,*) inject_ndep_acous
close(17)

!read the solid/fluid injected distances.
allocate(ndist_idepth_elas(inject_ndep_elas))
allocate(ndist_idepth_acous(inject_ndep_acous))
do media_type=1,2
!elastic
   if(media_type.eq.1) then
      file_name = trim(LOCAL_PATH(1:len_trim(LOCAL_PATH)))// '/' //&
                  trim(dist_table_elas_file(1:len_trim(dist_table_elas_file)))
      ndepth_temp=inject_ndep_elas
   else
!acoustic
      file_name = trim(LOCAL_PATH(1:len_trim(LOCAL_PATH)))// '/' //&
                   trim(dist_table_acous_file(1:len_trim(dist_table_acous_file)))
      ndepth_temp=inject_ndep_acous
   end if

   if(ndepth_temp.gt.0) then
     open(unit=15,file=file_name(1:len_trim(file_name)),&
        status='old',action='read',form='formatted')
     do idepth=1,ndepth_temp
      read(15,*)ndist_temp
      if(media_type.eq.1) then
          ndist_idepth_elas(idepth)=ndist_temp
          if(DEBUG_COUPLING) print *,'ndist',ndist_temp,idepth
      else
          ndist_idepth_acous(idepth)=ndist_temp
      end if
      do idist=1,ndist_temp
         read(15,*)dist_temp
      end do
     end do !idepth
     close(15)
    end if !ndepth_temp
end do

max_inject_ndist_elas=maxval(ndist_idepth_elas)
max_inject_ndist_acous=maxval(ndist_idepth_acous)

npressure=0;npoten_dot=0;nepsilon=0;nvelo=0;ndisp_fluid=0
ndepth_used_ela=0;ndepth_used_acou=0

idep_idist_temp1(:,:)=0
idep_idist_temp2(:,:)=0
iepsilon=0;ipressure=0


! Only one source allowed
x_source_norm = dsin(theta_source(1))*dcos(phi_source(1))
y_source_norm = dsin(theta_source(1))*dsin(phi_source(1))
z_source_norm = dcos(theta_source(1))

!call sync_all()
if(DEBUG_COUPLING) print *,'do init1'

!read depth id
if(num_abs_boundary_faces.gt.0) then
  allocate(idepth_readin(num_abs_boundary_faces,NGLLSQUARE))
  allocate(idist_readin(num_abs_boundary_faces,NGLLSQUARE))
  idepth_readin(:,:)=0
  idist_readin(:,:)=0
  do media_type=1,2
!elastic
   if(media_type.eq.1) then
      file_name = prname(1:len_trim(prname))//id_depth_elas_file(1:len_trim(id_depth_elas_file))
   else
!acoustic
      file_name = prname(1:len_trim(prname))//id_depth_acous_file(1:len_trim(id_depth_acous_file))
   end if

   open(unit=16,file=file_name(1:len_trim(file_name)),&
        status='old',action='read',form='formatted')
   read(16,*) num_faces_this_mediatype
   do iface=1,num_faces_this_mediatype
     do igll=1,NGLLSQUARE
      read(16,*) iface_temp,igll_temp,idepth_temp,idist_temp
      idepth_readin(iface_temp,igll_temp)=idepth_temp
      idist_readin(iface_temp,igll_temp)=idist_temp
     end do
   end do
 
   close(16)
  end do
end if


   file_name = prname(1:len_trim(prname))//"check_symmetry"
   open(unit=16,file=file_name(1:len_trim(file_name)),&
        action='write',form='formatted')

!call sync_all()

do iface=1,num_abs_boundary_faces
   ispec = abs_boundary_ispec(iface)
   do igll = 1,NGLLSQUARE
       idepth=idepth_readin(iface,igll)
       idistance=idist_readin(iface,igll)
!       print *,'convstartstart',iface,igll,myrank
!       call sync_all()
       ! gets local indices for GLL point
       i = abs_boundary_ijk(1,igll,iface)
       j = abs_boundary_ijk(2,igll,iface)
       k = abs_boundary_ijk(3,igll,iface)
       iglob=ibool(i,j,k,ispec)
       x=xstore(iglob);y=ystore(iglob);z=zstore(iglob)
       write(16,1000) &
              "dep_dist_id",idepth,idistance,x,y,z,iface,igll,myrank
1000   FORMAT(A4,2X,I5.2,I5.2,F12.3,F12.3,F12.3,I5.2,I5.2,I5.2)
!       call sync_all()
       !Convert the source moment_xyz (or force_xyz) in the x-y-z coordinate system to
       !moment_zrt (or force_zrt) in the z-r-t coordinate system.
       if(SINGLE_FORCE_ENZ.eq.0) then
            call convert_zrt_xyz(myrank,SINGLE_FORCE_ENZ,x_source_norm,y_source_norm,&
                         z_source_norm,nu_source(1,1,1),NSOURCES,x,y,z,Mxx(1),Myy(1),Mzz(1),Mxy(1),Mxz(1),Myz(1),&
                         moment_zrt(1,igll,iface),force_zrt(1,1,1),zrtToxyz(1,1,igll,iface),&
                         depth,distance)
       else
            call convert_zrt_xyz(myrank,SINGLE_FORCE_ENZ,x_source_norm,y_source_norm,&
                         z_source_norm,nu_source,NSOURCES,x,y,z,Mxx(1),Myy(1),Mzz(1),Mxy(1),Mxz(1),Myz(1),&
                         moment_zrt(1,1,1),force_zrt(1,igll,iface),zrtToxyz(1,1,igll,iface),&
                         depth,distance)
       end if

       if(myrank.eq.0.and.iface.eq.1.and.igll.eq.13.and.SINGLE_FORCE_ENZ.eq.0.and.DEBUG_COUPLING) &
           print *,'rank0 zrtToxyz', myrank,iface,x,y,z,&
                Mxx(1),Myy(1),Mzz(1),Mxy(1),Mxz(1),Myz(1),&
                depth,moment_zrt(:,igll,iface),zrtToxyz(:,:,igll,iface)

       if(ispec_is_elastic(ispec)) then
        if(distance*180.0/PI>142.60.and.distance*180.0/PI<142.7.and.idepth.eq.inject_ndep_elas.and.&
           DEBUG_COUPLING) then
           print *,'disp_elastic',idepth,idistance,distance*180.0/PI,x,y,z,iface,igll,myrank
        end if

         idep_idist_impose(1,igll,iface)=idepth
         idep_idist_impose(2,igll,iface)=idistance
         if(idepth.gt.inject_ndep_elas.or.idepth.lt.1.or. &
            idistance.gt.ndist_idepth_elas(idepth).or.idistance.lt.1) then
              !print *,'disp_elastic',idepth,idistance,ndist_idepth_elas(idepth),distance*180.0/PI,&
              !     x,y,z,iface,igll,myrank,6371.0-sqrt(x*x+y*y+z*z)/1000.0
              call exit_MPI(myrank,'Error,idepth or idistance is out of range permitted!')
         end if

!Count the depths of injected Green's functions 
!Find whether idepth is in the list idepth_used_temp1. 
!If not, add it to the array
         idepth_test=1
         do while(idepth_test.le.ndepth_used_ela.and.&
                 idepth.ne.idepth_used_temp1(idepth_test)) 
                     idepth_test=idepth_test+1
         end do
         if(idepth_test.gt.ndepth_used_ela) then
             ndepth_used_ela=idepth_test
             idepth_used_temp1(ndepth_used_ela)=idepth
         end if


      
!Find whether idepth and idistance is in the list idep_idist_temp1.
!If not, add it to array
         iepsilon=1
         do while(iepsilon.le.nepsilon.and. &
                   (idepth.ne.idep_idist_temp1(1,iepsilon) &
                   .or.idistance.ne.idep_idist_temp1(2,iepsilon)) )
               iepsilon=iepsilon+1  
         end do
         if(iepsilon.gt.nepsilon) then
               nepsilon=iepsilon
               idep_idist_temp1(1,iepsilon)=idepth
               idep_idist_temp1(2,iepsilon)=idistance
         end if
         if(iepsilon.eq.429.and.myrank.eq.0.and.DEBUG_COUPLING) &
             print *,'iepsilon_idist',idep_idist_temp1(:,iepsilon),iepsilon,&
                  nepsilon,idepth,idistance,iface,igll,myrank
         id_epsilon(igll,iface)=iepsilon
         id_velo(igll,iface)=iepsilon
!acoustic media
       else
         idep_idist_impose(1,igll,iface)=idepth
         idep_idist_impose(2,igll,iface)=idistance
         if(idepth.gt.inject_ndep_acous.or.idepth.lt.1.or. &
            idistance.gt.ndist_idepth_acous(idepth).or.idistance.lt.1) then
              call exit_MPI(myrank,'Error, idepth or idistance is out of range permitted!')
         end if

        if(distance*180.0/PI>142.60.and.distance*180.0/PI<142.7.and.DEBUG_COUPLING) then
         print *,'disp_acoutic',idepth,idistance,distance*180.0/PI,sqrt(x*x+y*y+z*z),iface,igll,myrank
        end if

!Count the depths of Green's function 
!Find whether idepth is in the list idepth_used_temp2. 
!If not, add it to the array
         idepth_test=1
         do while(idepth_test.le.ndepth_used_acou.and.&
                 idepth.ne.idepth_used_temp2(idepth_test))
                     idepth_test=idepth_test+1
         end do
         if(idepth_test.gt.ndepth_used_acou) then
             ndepth_used_acou=idepth_test
             idepth_used_temp2(ndepth_used_acou)=idepth
         end if

!Find whether idepth and idistance is in the list idep_idist_temp2.
!If not, add it to array
         ipressure=1
         do while(ipressure.le.npressure.and.&
                   (idepth.ne.idep_idist_temp2(1,ipressure) &
                   .or.idistance.ne.idep_idist_temp2(2,ipressure)) )
               ipressure=ipressure+1
         end do
         if(ipressure.gt.npressure) then
               npressure=ipressure
               idep_idist_temp2(1,ipressure)=idepth
               idep_idist_temp2(2,ipressure)=idistance
         end if
         id_pressure(igll,iface)=ipressure
         id_poten_dot(igll,iface)=ipressure
       end if
   end do
end do

close(16)


nvelo=nepsilon
npoten_dot=npressure
ndisp_fluid=npressure
if(SINGLE_FORCE_ENZ.eq.0) then
    ncomp_source=6
    !change ncomp_source_selected to selsect moment tensor components you  
    !would like to be involved (e.g. the selected components for a fixed moment tensor). Then
    !only need to compute the selected components' wavefields.

    !/'Mzz','Mrr','Mtt','Mzr','Mzt','Mrt'/
    !only Mzz is selected
    ncomp_source_selected=1
    !The first three Mrr, Mtt and Mpp are selected.
    !ncomp_source_selected=3
    !full six Mij components are selected.
    !ncomp_source_selected=6

    sources_selected(1:ncomp_source_selected)=moment_name(1:ncomp_source_selected)
else

    ncomp_source=3
    !SINGLE_FORCE_ENZ = 1 for East, 2 for North, and 3 for Vertical component.
    if(SINGLE_FORCE_ENZ.eq.3) then
      !vertical component mode, only force 'Fz' is involved.
      ncomp_source_selected=1
      sources_selected(1)=force_name(3)
    else
      !East or North component mode, forces 'Fr' and 'Ft' are involved.
      ncomp_source_selected=2
      sources_selected(1:ncomp_source_selected)=force_name(1:ncomp_source_selected)
    end if
end if
!call sync_all()
if(DEBUG_COUPLING) print *,'do_init_nvelo',nvelo,npoten_dot
!call sync_all()
allocate(idepth_used_ela(ndepth_used_ela))
allocate(idepth_used_acou(ndepth_used_acou))
allocate(idep_idist_impose_elas(2,nepsilon))
allocate(idep_idist_impose_acous(2,npressure))
allocate(impose_pressure(inject_npt_eachpack,ncomp_source,npressure))
allocate(impose_potential_dot(inject_npt_eachpack,ncomp_source,npoten_dot))
allocate(impose_disp_fluid(inject_npt_eachpack,3,ncomp_source,ndisp_fluid))
allocate(impose_epsilon(inject_npt_eachpack,6,ncomp_source,nepsilon))
allocate(impose_velo(inject_npt_eachpack,3,ncomp_source,nvelo))
idepth_used_ela(:)=0
idepth_used_acou(:)=0
idep_idist_impose_elas(:,:)=0
idep_idist_impose_acous(:,:)=0
impose_epsilon(:,:,:,:)=0.0
impose_velo(:,:,:,:)=0.0
!call sync_all()
if(DEBUG_COUPLING) print *,'do init2'
!call sync_all()
do ipressure=1,npressure
   idep_idist_impose_acous(:,ipressure)=idep_idist_temp2(:,ipressure)
end do
do iepsilon=1,nepsilon
   idep_idist_impose_elas(:,iepsilon)=idep_idist_temp1(:,iepsilon)
end do
do idepth=1,ndepth_used_ela
   idepth_used_ela(idepth)=idepth_used_temp1(idepth)
end do
do idepth=1,ndepth_used_acou
   idepth_used_acou(idepth)=idepth_used_temp2(idepth)
end do

deallocate(idepth_readin)
deallocate(idist_readin)
deallocate(idep_idist_temp1)
deallocate(idep_idist_temp2)
deallocate(idepth_used_temp1)
deallocate(idepth_used_temp2)
deallocate(idist_used_temp1)
deallocate(idist_used_temp2)

!call sync_all()
if(DEBUG_COUPLING) print *,'do init22'
!call sync_all()


end subroutine

subroutine convert_zrt_xyz(myrank,SINGLE_FORCE_ENZ,x_source_norm,y_source_norm,&
              z_source_norm,nu_source,NSOURCES,x,y,z,Mxx,Myy,Mzz,Mxy,Mxz,Myz,moment_zrt,&
              force_zrt,zrtToxyz,depth,distance)

use constants
 
implicit none
integer ::myrank,SINGLE_FORCE_ENZ,NSOURCES
double precision ::x_source_norm,y_source_norm,z_source_norm
double precision, dimension(3,3,NSOURCES) :: nu_source
double precision ::x,y,z

double precision ::Mxx,Myy,Mzz,Mxy,Mxz,Myz
real(kind=CUSTOM_REAL), dimension(6)   ::moment_zrt
real(kind=CUSTOM_REAL), dimension(3)   ::force_zrt,force_xyz
double precision, dimension(3,3) ::zrtToxyz
double precision ::depth,distance

!local parameters
double precision ::r,x_norm,y_norm,z_norm
double precision,dimension(3) ::vector_z_source,vector_r_source,vector_t_source
double precision,dimension(3) ::vect_z_boundary,vect_r_boundary,vect_t_boundary
real(kind=CUSTOM_REAL),dimension(3,3) ::moment_xyz,moment_zrt_temp
double precision, dimension(3,3) ::xyzTozrt_source,zrtToxyz_source
double precision ::mod_temp

r=dsqrt(x*x+y*y+z*z)
depth=R_EARTH_SURF-r
x_norm=x/r;y_norm=y/r;z_norm=z/r
if(dabs(x_norm-x_source_norm)<TINYVAL.and.dabs(y_norm-y_source_norm)<TINYVAL) then
  x_norm=x_norm+TINYVAl
end if
vector_z_source(1)=x_source_norm
vector_z_source(2)=y_source_norm
vector_z_source(3)=z_source_norm
vector_t_source(1)= z_norm*vector_z_source(2)- &
                    y_norm*vector_z_source(3) 
vector_t_source(2)= -z_norm*vector_z_source(1) + &
                    x_norm*vector_z_source(3)
vector_t_source(3)= y_norm*vector_z_source(1) - &
                    x_norm*vector_z_source(2)

mod_temp=dsqrt(dot_product(vector_t_source,vector_t_source))
if(mod_temp.eq.TINYVAL) then
    call exit_MPI(myrank,'Error, great circle distance between source &
                  & and boundary element is near ZERO!!')
else
!normalize vector_t_source
    vector_t_source(:)=vector_t_source(:)/mod_temp
end if
vector_r_source(1)= vector_t_source(2)*vector_z_source(3) - &
                         vector_t_source(3)*vector_z_source(2)
vector_r_source(2)= -vector_t_source(1)*vector_z_source(3)+&
                         vector_t_source(3)*vector_z_source(1)
vector_r_source(3)=  vector_t_source(1)*vector_z_source(2)- &
                         vector_t_source(2)*vector_z_source(1)
vect_t_boundary(:)=vector_t_source(:)
vect_z_boundary(1)=x_norm
vect_z_boundary(2)=y_norm
vect_z_boundary(3)=z_norm
distance=dacos(dot_product(vect_z_boundary,vector_z_source)*(1.-TINYVAL))
vect_r_boundary(1)=vect_t_boundary(2)*vect_z_boundary(3)- &
                        vect_t_boundary(3)*vect_z_boundary(2)
vect_r_boundary(2)=-vect_t_boundary(1)*vect_z_boundary(3)+ &
                        vect_t_boundary(3)*vect_z_boundary(1)
vect_r_boundary(3)=vect_t_boundary(1)*vect_z_boundary(2)- &
                        vect_t_boundary(2)*vect_z_boundary(1)


zrtToxyz_source(1,:)=vector_z_source(:)
zrtToxyz_source(2,:)=vector_r_source(:)
zrtToxyz_source(3,:)=vector_t_source(:)

xyzTozrt_source(:,:)=0.d0

!The Green Functions are described in zrt coordinate system.
!rotation matrix used to get traction and velociy in xyz coordinate system
zrtToxyz(1,:)=vect_z_boundary(:)
zrtToxyz(2,:)=vect_r_boundary(:)
zrtToxyz(3,:)=vect_t_boundary(:)
xyzTozrt_source=transpose(zrtToxyz_source)

!Receiver-side coupling and a single force applied
if(SINGLE_FORCE_ENZ.eq.1 .or. SINGLE_FORCE_ENZ.eq.2 .or. SINGLE_FORCE_ENZ.eq.3) then 
   force_xyz=nu_source(SINGLE_FORCE_ENZ,:,1)
   force_zrt(1)=dot_product(force_xyz,vector_z_source)
   force_zrt(2)=dot_product(force_xyz,vector_r_source)
   force_zrt(3)=dot_product(force_xyz,vector_t_source)
   if(myrank.eq.0.and.DEBUG_COUPLING) print *,'projected force_zrt',force_zrt(:),nu_source(SINGLE_FORCE_ENZ,:,1)
else
    !rotate the moment tensor in xyz-coordinate to zrt-coordinate
    moment_xyz(1,1)=Mxx;moment_xyz(1,2)=Mxy;moment_xyz(1,3)=Mxz
    moment_xyz(2,1)=Mxy;moment_xyz(2,2)=Myy;moment_xyz(2,3)=Myz
    moment_xyz(3,1)=Mxz;moment_xyz(3,2)=Myz;moment_xyz(3,3)=Mzz
    moment_zrt_temp=matmul(transpose(xyzTozrt_source),matmul(moment_xyz,xyzTozrt_source))
    moment_zrt(1)=moment_zrt_temp(1,1);moment_zrt(2)=moment_zrt_temp(2,2);moment_zrt(3)=moment_zrt_temp(3,3)
    moment_zrt(4)=moment_zrt_temp(1,2);moment_zrt(5)=moment_zrt_temp(1,3);moment_zrt(6)=moment_zrt_temp(2,3)
end if
end subroutine

subroutine read_1Dboundary_values(ipackage,myrank,SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
use impose_1D_BCS
use constants

implicit none
integer ::ipackage,idepth,myrank
integer ::SINGLE_FORCE_ENZ
character(len=MAX_STRING_LEN) :: INJECTED_WAVEFIELD_PATH

integer ::icomp_source


impose_pressure(:,:,:)=0.0
impose_potential_dot(:,:,:)=0.0
impose_disp_fluid(:,:,:,:)=0.0
impose_epsilon(:,:,:,:)=0.0
impose_velo(:,:,:,:)=0.0

!Note that only the selected source components' waves are read below and others
!remain zero in the whole simulation.

do icomp_source=1,ncomp_source_selected

!elastic media
  do idepth=1,ndepth_used_ela 
!    #########################read stress and velocity##################################
     call read_stress_foridepth(icomp_source,ipackage,idepth,myrank,&
              SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
     call read_velo_foridepth(icomp_source,ipackage,idepth,myrank,&
              SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
  end do  !idepth
!  call synchronize_all()


!asoustic media
  do idepth=1,ndepth_used_acou
!    #########################read potential_dot and displacement#######################
!        call read_pressure_foridepth(icomp_source,ipackage,idepth,&
!                    SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
        call read_pdot_foridepth(icomp_source,ipackage,idepth,myrank,&
                   SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
        call read_disp_fluid_foridepth(icomp_source,ipackage,idepth,myrank,&
                   SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
  end do  !idepth



end do   !icomp_source

end subroutine


!read stress for the source component 'icomp_source' and the depth corresponding
!to idepth_count
subroutine read_stress_foridepth(icomp_source,ipackage_read,idepth_count,&
                       myrank,SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
use  impose_1D_BCS
use  constants
implicit none
integer ::ipackage_read,icomp_source,idepth_count
integer ::myrank
character(len=MAX_STRING_LEN) :: INJECTED_WAVEFIELD_PATH
integer ::SINGLE_FORCE_ENZ

!other parameters
integer ::idepth_read
character(len=80) ::source_name
real(kind=8),dimension(:,:),allocatable ::array_temp_stress
character(len=MAX_STRING_LEN)::file_name,Green_stress_file
integer ::icomp_stress,index_ZRT,itheta,iepsilon
integer ::ista_start,ista_end

allocate(array_temp_stress(inject_npt_eachpack,max_inject_ndist_elas))
idepth_read=idepth_used_ela(idepth_count)
if(SINGLE_FORCE_ENZ.eq.0.or.SINGLE_FORCE_ENZ.eq.3) then
  !For moment tensor (SINGLE_FORCE_ENZ=0), stresses are read and saved in
  !impose_epsilon(:,:,index_ZRT,:), index_ZRT=icomp_source (between 1-6). 
  !For vertical single-force (SINGLE_FORCE_ENZ=3), stresses are read and saved in
  !impose_epsilon(:,:,index_ZRT,:),index_ZRT=1.
  index_ZRT=icomp_source
else 
  !For North or East component of single force, Green's functions of Fr and Ft are needed. 
  !icomp_source=1 selects radial single force, and stresses are read and saved
  !in impose_epsilon(:,:,index_ZRT,:), index_ZRT=2; icomp_source=2 selects
  !transverse single force and index_ZRT=3. impose_epsilon(:,:,index_ZRT,:) with 
  !index_ZRT=1 is zero.
  index_ZRT=icomp_source+1
end if

write(file_name,"('/stress/depth',i5.5'_package',i5.5)") idepth_read,ipackage_read
source_name=sources_selected(icomp_source)
Green_stress_file=INJECTED_WAVEFIELD_PATH(1:len_trim(INJECTED_WAVEFIELD_PATH)) // '/' &
    //trim(source_name(1:len_trim(source_name)))// '/' //trim(file_name(1:len_trim(file_name)))

open(unit=IIN,file=Green_stress_file,form='unformatted',status='old',action='read')

ista_start=1
ista_end=ndist_idepth_elas(idepth_read)
if(DEBUG_COUPLING) print *,'edge_nsta',ista_start,ista_end
!read six components of stress one by one
do icomp_stress=1,6
   if(myrank.eq.0.and.DEBUG_COUPLING) print *,'check_ista_rank0',ista_start,ista_end,idepth_read
   !read stress for this source component and this depth 'idepth_read'
   do itheta=ista_start,ista_end
    read(IIN)array_temp_stress(:,itheta-ista_start+1)
   end do
   !copy the stress to impose_epsilon, if the depth matches idepth_read and the distance id falls
   !in the range (ista_start,ista_end) 
   do iepsilon=1,nepsilon
    if(idep_idist_impose_elas(1,iepsilon).eq.idepth_read.and.&
       idep_idist_impose_elas(2,iepsilon).ge.ista_start.and.&
       idep_idist_impose_elas(2,iepsilon).le.ista_end) then
           impose_epsilon(:,icomp_stress,index_ZRT,iepsilon) &
                      =array_temp_stress(:,idep_idist_impose_elas(2,iepsilon)-ista_start+1)
    end if
   end do
end do
close(IIN)

!check stress
do iepsilon=1,nepsilon
    do icomp_stress=2,2
      if(myrank.eq.0.and.idep_idist_impose_elas(1,iepsilon).eq.idepth_read.and.DEBUG_COUPLING) &
          print *,"max_str_all",maxval(impose_epsilon(:,icomp_stress,index_ZRT,iepsilon)),&
             idepth_read,iepsilon,idep_idist_impose_elas(2,iepsilon),ista_start,ista_end,myrank
    end do
end do

!call synchronize_all()
deallocate(array_temp_stress)
end subroutine

!read velocity seismograms for the source component 'icomp_source' and the depth corresponding
!to idepth_count
subroutine read_velo_foridepth(icomp_source,ipackage_read,idepth_count,&
                       myrank,SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
use  impose_1D_BCS
use  constants
implicit none
integer ::ipackage_read,icomp_source,idepth_count,myrank
character(len=MAX_STRING_LEN) :: INJECTED_WAVEFIELD_PATH
integer ::SINGLE_FORCE_ENZ

!other parameters
integer ::idepth_read
character(len=80) ::source_name
real(kind=8),dimension(:,:),allocatable ::array_temp_velo
character(len=MAX_STRING_LEN)::file_name,Green_velo_file
integer ::icomp_velo,itheta,ivelo,index_ZRT
integer ::ista_start,ista_end

allocate(array_temp_velo(inject_npt_eachpack,max_inject_ndist_elas))
idepth_read=idepth_used_ela(idepth_count)

if(SINGLE_FORCE_ENZ.eq.0.or.SINGLE_FORCE_ENZ.eq.3) then
  !For moment tensor (SINGLE_FORCE_ENZ=0), velocities are read and saved in
  !impose_velo(:,:,index_ZRT,:), index_ZRT=icomp_source (between 1-6). 
  !For vertical single-force (SINGLE_FORCE_ENZ=3), velocities are read and saved in
  !impose_velo(:,:,index_ZRT,:),index_ZRT=1.
  index_ZRT=icomp_source
else
  !For North or East component of single force, Green's functions of Fr and Ft are needed. 
  !icomp_source=1 selects radial single force, and velocities are read and saved
  !in impose_velo(:,:,index_ZRT,:), index_ZRT=2; icomp_source=2 selects
  !transverse single force and index_ZRT=3. impose_velo(:,:,index_ZRT,:) with 
  !index_ZRT=1 is zero.
  index_ZRT=icomp_source+1
end if



write(file_name,"('/velo_solid/depth',i5.5'_package',i5.5)") idepth_read,ipackage_read
source_name=sources_selected(icomp_source)
Green_velo_file=INJECTED_WAVEFIELD_PATH(1:len_trim(INJECTED_WAVEFIELD_PATH)) // '/' &
    //trim(source_name(1:len_trim(source_name)))// '/' //trim(file_name(1:len_trim(file_name)))


open(unit=IIN,file=Green_velo_file,form='unformatted',status='old',action='read')

ista_start=1
ista_end=ndist_idepth_elas(idepth_read)
!read three components of velocity seismograms one by one
do icomp_velo=1,3
   !read velocity seismograms for this source component and this depth 'idepth_read'
   do itheta=ista_start,ista_end
     read(IIN)array_temp_velo(:,itheta-ista_start+1)
     if(idepth_read.eq.41.and.itheta.eq.43.and.DEBUG_COUPLING) &
            print *,'in_velo_0',idepth_read,icomp_velo,array_temp_velo(:,itheta-ista_start+1)
   end do
   do ivelo=1,nvelo
   !copy the velocity seismograms to impose_velo, if the depth matches idepth_read and the
   !distance id falls in the range (ista_start,ista_end)
     if(idep_idist_impose_elas(1,ivelo).eq.idepth_read.and.&
        idep_idist_impose_elas(2,ivelo).ge.ista_start.and.&
        idep_idist_impose_elas(2,ivelo).le.ista_end) then
            impose_velo(:,icomp_velo,index_ZRT,ivelo) &
                       =array_temp_velo(:,idep_idist_impose_elas(2,ivelo)-ista_start+1)
        if(ivelo.eq.429.and.myrank.eq.0.and.DEBUG_COUPLING) &
           print *,"input_velo1",icomp_velo,ivelo,itheta,idep_idist_impose_elas(:,ivelo),&
             impose_velo(1:50,icomp_velo,icomp_source,ivelo)
     end if
   end do !ivelo
end do !icomp_velo
close(IIN)

deallocate(array_temp_velo)
end subroutine


!read pressure for the source component 'icomp_source' and the depth corresponding
!to idepth_count
subroutine read_pressure_foridepth(icomp_source,&
                       ipackage_read,idepth_count,SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
use  impose_1D_BCS
use  constants
implicit none
integer ::ipackage_read,icomp_source,idepth_count
character(len=MAX_STRING_LEN) :: INJECTED_WAVEFIELD_PATH
integer ::SINGLE_FORCE_ENZ

!other parameters
integer ::idepth_read
character(len=80) ::source_name
real(kind=8),dimension(:,:),allocatable ::array_temp_pressure
character(len=MAX_STRING_LEN)::file_name,Green_pressure_file
integer ::itheta,ipressure,index_ZRT
integer ::ista_start,ista_end

 allocate(array_temp_pressure(inject_npt_eachpack,max_inject_ndist_acous))
idepth_read=idepth_used_acou(idepth_count)

if(SINGLE_FORCE_ENZ.eq.0.or.SINGLE_FORCE_ENZ.eq.3) then
  !For moment tensor (SINGLE_FORCE_ENZ=0), pressures are read and saved in
  !impose_pressure(:,:,index_ZRT,:), index_ZRT=icomp_source (between 1-6). 
  !For vertical single-force (SINGLE_FORCE_ENZ=3), pressures are read and saved
  !in impose_pressure(:,:,index_ZRT,:),index_ZRT=1.
  index_ZRT=icomp_source
else
  !For North or East component of single force, Green's functions of Fr and Ft are needed. 
  !icomp_source=1 selects radial single force, and pressures are read and saved
  !in impose_pressure(:,:,index_ZRT,:), index_ZRT=2; icomp_source=2 selects
  !transverse single force and index_ZRT=3. impose_pressure(:,:,index_ZRT,:) with 
  !index_ZRT=1 is zero.
  index_ZRT=icomp_source+1
end if

write(file_name,"('/pressure/depth',i5.5'_package',i5.5)") idepth_read,ipackage_read
source_name=sources_selected(icomp_source)
Green_pressure_file=INJECTED_WAVEFIELD_PATH(1:len_trim(INJECTED_WAVEFIELD_PATH)) // '/' &
    //trim(source_name(1:len_trim(source_name)))// '/' //trim(file_name(1:len_trim(file_name)))

open(unit=IIN,file=Green_pressure_file,form='unformatted',status='old',action='read')

ista_start=1
ista_end=ndist_idepth_acous(idepth_read)
!read pressures for this source component and this depth 'idepth_read'
do itheta=ista_start,ista_end
   read(IIN)array_temp_pressure(:,itheta-ista_start+1)
end do
do ipressure=1,npressure
   !copy the pressures to impose_pressure, if the depth matches idepth_read and the
   !distance id falls in the range (ista_start,ista_end) 
   if(idep_idist_impose_acous(1,ipressure).eq.idepth_read.and.&
      idep_idist_impose_acous(2,ipressure).ge.ista_start.and.&
      idep_idist_impose_acous(2,ipressure).le.ista_end) then
         impose_pressure(:,index_ZRT,ipressure) &
                   =array_temp_pressure(:,idep_idist_impose_acous(2,ipressure)-ista_start+1)
   end if
end do
close(IIN)
deallocate(array_temp_pressure)
end subroutine

!read displacement in fluid for the source component 'icomp_source' and the depth corresponding
subroutine read_disp_fluid_foridepth(icomp_source,ipackage_read,idepth_count,&
                       myrank,SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
use  impose_1D_BCS
use  constants
implicit none
integer ::ipackage_read,icomp_source,idepth_count,myrank
character(len=MAX_STRING_LEN) :: INJECTED_WAVEFIELD_PATH
integer ::SINGLE_FORCE_ENZ

!other parameters
integer ::idepth_read
character(len=80) ::source_name
real(kind=8),dimension(:,:),allocatable ::array_temp_disp
character(len=MAX_STRING_LEN)::file_name,Green_disp_file
integer ::icomp_disp,itheta,idisp,index_ZRT
integer ::ista_start,ista_end

allocate(array_temp_disp(inject_npt_eachpack,max_inject_ndist_acous))
idepth_read=idepth_used_acou(idepth_count)

if(SINGLE_FORCE_ENZ.eq.0.or.SINGLE_FORCE_ENZ.eq.3) then
  !For moment tensor (SINGLE_FORCE_ENZ=0), displacements are read and saved in
  !impose_disp_fluid(:,:,index_ZRT,:), index_ZRT=icomp_source (between 1-6). 
  !For vertical single-force (SINGLE_FORCE_ENZ=3), displacements are read and saved
  !in impose_disp_fluid(:,:,index_ZRT,:),index_ZRT=1.
  index_ZRT=icomp_source
else
  !For North or East component of single force, Green's functions of Fr and Ft are needed. 
  !icomp_source=1 selects radial single force, and displacements are read and saved
  !in impose_disp_fluid(:,:,index_ZRT,:), index_ZRT=2; icomp_source=2 selects
  !transverse single force and index_ZRT=3. impose_disp_fluid(:,:,index_ZRT,:) with 
  !index_ZRT=1 is zero.
  index_ZRT=icomp_source+1
end if


write(file_name,"('/disp_fluid/depth',i5.5'_package',i5.5)") idepth_read,ipackage_read
source_name=sources_selected(icomp_source)
Green_disp_file=INJECTED_WAVEFIELD_PATH(1:len_trim(INJECTED_WAVEFIELD_PATH)) // '/' &
    //trim(source_name(1:len_trim(source_name)))// '/' //trim(file_name(1:len_trim(file_name)))

open(unit=IIN,file=Green_disp_file,form='unformatted',status='old',action='read')

ista_start=1
ista_end=ndist_idepth_acous(idepth_read)
!read three components of displacement one by one
do icomp_disp=1,3
   !read displacement for this source component and this depth 'idepth_read'
   do itheta=ista_start,ista_end
    read(IIN)array_temp_disp(:,itheta-ista_start+1)
    if(idepth_read.eq.1.and.itheta.eq.1.and.DEBUG_COUPLING) &
           print *,'in_disp_0',idepth_read,icomp_disp,array_temp_disp(:,itheta-ista_start+1)
   end do
   !copy the displacement to impose_disp_fluid, if the depth matches idepth_read and the
   !distance id falls in the range (ista_start,ista_end) 
   do idisp=1,ndisp_fluid
      if(idep_idist_impose_acous(1,idisp).eq.idepth_read.and.&
         idep_idist_impose_acous(2,idisp).ge.ista_start.and.&
         idep_idist_impose_acous(2,idisp).le.ista_end) then
             impose_disp_fluid(:,icomp_disp,index_ZRT,idisp) &
                        =array_temp_disp(:,idep_idist_impose_acous(2,idisp)-ista_start+1)
      end if
   end do !idisp
end do !icomp_disp
close(IIN)

if(myrank.eq.0.and.DEBUG_COUPLING) then
   print *,'read impose_disp_fluid'
end if

deallocate(array_temp_disp)
end subroutine

!read potential_dot in fluid for the source component 'icomp_source' and the
!depth corresponding
subroutine read_pdot_foridepth(icomp_source,ipackage_read,idepth_count,myrank,&
                                   SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
use  impose_1D_BCS
use  constants
implicit none
integer ::ipackage_read,icomp_source,idepth_count,myrank
character(len=MAX_STRING_LEN) :: INJECTED_WAVEFIELD_PATH
integer ::SINGLE_FORCE_ENZ

!other parameters
integer ::idepth_read
character(len=80) ::source_name
real(kind=8),dimension(:,:),allocatable ::array_temp_chidot
character(len=MAX_STRING_LEN)::file_name,Green_pdot_file
integer ::icomp_pdot,itheta,ipdot,index_ZRT
integer ::ista_start,ista_end

allocate(array_temp_chidot(inject_npt_eachpack,max_inject_ndist_acous))
idepth_read=idepth_used_acou(idepth_count)

if(SINGLE_FORCE_ENZ.eq.0.or.SINGLE_FORCE_ENZ.eq.3) then
  !For moment tensor (SINGLE_FORCE_ENZ=0), potential_dot are read and saved in
  !impose_potential_dot(:,:,index_ZRT,:), index_ZRT=icomp_source (between 1-6). 
  !For vertical single-force (SINGLE_FORCE_ENZ=3), potential_dot are read and
  !saved in impose_potential_dot(:,:,index_ZRT,:),index_ZRT=1.
  index_ZRT=icomp_source
else
  !For North or East component of single force, Green's functions of Fr and Ft are needed. 
  !icomp_source=1 selects radial single force, and potential_dot are read and saved
  !in impose_potential_dot(:,:,index_ZRT,:), index_ZRT=2; icomp_source=2 selects
  !transverse single force and index_ZRT=3. impose_potential_dot(:,:,index_ZRT,:) with 
  !index_ZRT=1 is zero.
  index_ZRT=icomp_source+1
end if

write(file_name,"('/chi_dot/depth',i5.5'_package',i5.5)") idepth_read,ipackage_read
source_name=sources_selected(icomp_source)
Green_pdot_file=INJECTED_WAVEFIELD_PATH(1:len_trim(INJECTED_WAVEFIELD_PATH)) // '/' &
    //trim(source_name(1:len_trim(source_name)))// '/' //trim(file_name(1:len_trim(file_name)))

open(unit=IIN,file=Green_pdot_file,form='unformatted',status='old',action='read')

ista_start=1
ista_end=ndist_idepth_acous(idepth_read)
!read potential_dot
do icomp_pdot=1,1
   !read potential_dot for this source component and this depth 'idepth_read'
   do itheta=ista_start,ista_end
     read(IIN)array_temp_chidot(:,itheta-ista_start+1)
   end do
   !copy the potential_dot to impose_potential_dot, if the depth matches idepth_read
   !and the distance id falls in the range (ista_start,ista_end) 
   do ipdot=1,npoten_dot
     if(idep_idist_impose_acous(1,ipdot).eq.idepth_read.and.&
        idep_idist_impose_acous(2,ipdot).ge.ista_start.and.&
        idep_idist_impose_acous(2,ipdot).le.ista_end) then
            impose_potential_dot(:,index_ZRT,ipdot) &
                       =array_temp_chidot(:,idep_idist_impose_acous(2,ipdot)-ista_start+1)
     end if
   end do !ipdot
end do !icomp_pdot
close(IIN)

deallocate(array_temp_chidot)


if(myrank.eq.0.and.DEBUG_COUPLING) then
   print *,'read_chi done'
end if
end subroutine



subroutine get_1Dtraction(myrank,SINGLE_FORCE_ENZ,traction_xyz,nx,ny,nz,igll,iface,time)
use impose_1D_BCS
use constants
implicit none

integer SINGLE_FORCE_ENZ
integer igll,iface,myrank
real(kind=CUSTOM_REAL) nx,ny,nz,nz_zrt,nr_zrt,nt_zrt
real(kind=CUSTOM_REAL),dimension(3):: traction_xyz
real(kind=CUSTOM_REAL) ::time

!local parameters
integer ::isigma,icomp_source
real(kind=CUSTOM_REAL),dimension(3)::traction_zrt
real(kind=CUSTOM_REAL),dimension(6):: epsi_impose
real(kind=CUSTOM_REAL),dimension(6):: source_zrt_temp
real(kind=CUSTOM_REAL),dimension(3,3)::zrtToxyz_temp
real(kind=CUSTOM_REAL) ::frac_it1,frac_it2

!For Lagrange interpolation
!integer::icomp_interpo,it_copy,it_fit
!real(kind=CUSTOM_REAL),dimension(NFIT) ::time_forfit,epsilon_forfit
!real(kind=CUSTOM_REAL) ::epsilon_interpo_out
!real(kind=CUSTOM_REAL) ::depsi

frac_it2=(time-t0_this_pack)/inject_dt  - (it_impose-1)
if(frac_it2.lt.-TINYVAL.or.frac_it2.gt.1+TINYVAL) then
   print*,'frac_it2',frac_it2,time,t0_this_pack,(time-t0_this_pack)/inject_dt,(it_impose-1)
   call exit_MPI(myrank,'frac_it2<0 or >1, wrong it_impose, inject_dt or t0_this_pack')
end if
frac_it1=1.0-frac_it2

source_zrt_temp(:)=0.0
!Source as moment tensor
if(SINGLE_FORCE_ENZ.eq.0) then
   source_zrt_temp(1:ncomp_source)=moment_zrt(1:ncomp_source,igll,iface)
!Source as single force
else
   source_zrt_temp(1:ncomp_source)=force_zrt(1:ncomp_source,igll,iface)
end if


zrtToxyz_temp(:,:)=zrtToxyz(:,:,igll,iface)
nz_zrt=nx*zrtToxyz_temp(1,1)+ny*zrtToxyz_temp(1,2)+nz*zrtToxyz_temp(1,3)
nr_zrt=nx*zrtToxyz_temp(2,1)+ny*zrtToxyz_temp(2,2)+nz*zrtToxyz_temp(2,3)
nt_zrt=nx*zrtToxyz_temp(3,1)+ny*zrtToxyz_temp(3,2)+nz*zrtToxyz_temp(3,3)

isigma=id_epsilon(igll,iface)
traction_zrt(:)=0.d0
do icomp_source=1,ncomp_source
  !do icomp_interpo=1,6
  !   do it_fit=1,NFIT
  !       if(inject_npt_eachpack.lt.NFIT) stop 'inject_npt_eachpack should be larger than NFIT'
  !       if(it_impose-NFIT/2.lt.1) then
  !        it_copy=it_fit
  !       else if(it_impose+NFIT/2-1.gt.inject_npt_eachpack) then
  !        it_copy=(inject_npt_eachpack-NFIT)+it_fit
  !       else
  !        it_copy=it_impose+(it_fit-1)-NFIT/2
  !       end if
  !       time_forfit(it_fit)=t0_this_pack+(it_copy-1)*inject_dt
  !       epsilon_forfit(it_fit)=impose_epsilon(it_copy,icomp_interpo,icomp_source,isigma)
  !       print *,'interpo_epsilon',time_forfit(:),epsilon_forfit,time
  !   end do
     !call updown(NFIT,time_forfit,epsilon_forfit,time,epsi_impose(icomp_interpo),depsi)
      !the below interpolation has not been tested yet.
!     call lagrange_value_1d( NFIT-1, time_forfit, epsilon_forfit, 1, time, &
!                             epsilon_interpo_out)
  !   epsi_impose(icomp_interpo)=epsilon_interpo_out
  !end do

  !if(myrank.eq.40.and.iface.eq.1.and.igll.eq.1.and.DEBUG_COUPLING) &
  !   print *,'interpo1',it_impose,it_copy,time,time_forfit(:),epsi_impose(:)


  !The above lines use more accurate but expensive Lagrange interpolations. 
  !They are not used, because it turns out that the below simple linear
  !and less expensive interpolation already provides sufficiently accurate results.
  epsi_impose(1)=frac_it1*impose_epsilon(it_impose,1,icomp_source,isigma)+ &
                 frac_it2*impose_epsilon(it_impose+1,1,icomp_source,isigma)
  epsi_impose(2)=frac_it1*impose_epsilon(it_impose,2,icomp_source,isigma)+ &
                 frac_it2*impose_epsilon(it_impose+1,2,icomp_source,isigma)
  epsi_impose(3)=frac_it1*impose_epsilon(it_impose,3,icomp_source,isigma)+ &
                 frac_it2*impose_epsilon(it_impose+1,3,icomp_source,isigma)
  epsi_impose(4)=frac_it1*impose_epsilon(it_impose,4,icomp_source,isigma)+ &
                 frac_it2*impose_epsilon(it_impose+1,4,icomp_source,isigma)
  epsi_impose(5)=frac_it1*impose_epsilon(it_impose,5,icomp_source,isigma)+ &
                 frac_it2*impose_epsilon(it_impose+1,5,icomp_source,isigma)
  epsi_impose(6)=frac_it1*impose_epsilon(it_impose,6,icomp_source,isigma)+ &
                 frac_it2*impose_epsilon(it_impose+1,6,icomp_source,isigma)
  !if(myrank.eq.40.and.iface.eq.1.and.igll.eq.1.and.DEBUG_COUPLING) &
  !     print *,'interpo2',it_impose,it_copy,time,time_forfit(:),epsi_impose(:)

  traction_zrt(1)=traction_zrt(1)+source_zrt_temp(icomp_source)* &
                         (epsi_impose(1)*nz_zrt+ &
                          epsi_impose(4)*nr_zrt+ &
                          epsi_impose(5)*nt_zrt)
  traction_zrt(2)=traction_zrt(2)+source_zrt_temp(icomp_source)* &
                         (epsi_impose(2)*nr_zrt+ &
                          epsi_impose(4)*nz_zrt+ &
                          epsi_impose(6)*nt_zrt)

  traction_zrt(3)=traction_zrt(3)+source_zrt_temp(icomp_source)* &
                         (epsi_impose(3)*nt_zrt+ &
                          epsi_impose(5)*nz_zrt+ &
                          epsi_impose(6)*nr_zrt)
end do  !icomp_source
 

traction_xyz(1)=traction_zrt(1)*zrtToxyz_temp(1,1)+traction_zrt(2)*zrtToxyz_temp(2,1)+traction_zrt(3)*zrtToxyz_temp(3,1)
traction_xyz(2)=traction_zrt(1)*zrtToxyz_temp(1,2)+traction_zrt(2)*zrtToxyz_temp(2,2)+traction_zrt(3)*zrtToxyz_temp(3,2)
traction_xyz(3)=traction_zrt(1)*zrtToxyz_temp(1,3)+traction_zrt(2)*zrtToxyz_temp(2,3)+traction_zrt(3)*zrtToxyz_temp(3,3)
!if(it_impose.eq.1) print *,'r00_tra',it_impose,traction_xyz(1),traction_xyz(2),&
!                                                  traction_xyz(3),nx,ny,nz,nz_zrt,nr_zrt,nt_zrt

if(myrank.eq.0.and.DEBUG_COUPLING) then
   print *,'traction_xyz done'
end if
end subroutine

subroutine  get_1Dpressure(myrank,SINGLE_FORCE_ENZ,pressure,igll,iface,time)
use impose_1D_BCS
implicit none
!pressure = - chi_dot_dot
integer ::SINGLE_FORCE_ENZ
real(kind=CUSTOM_REAL) ::pressure
integer ::igll,iface,myrank
real(kind=CUSTOM_REAL) ::time

!local parameters
integer ::icomp_source,ipressure
real(kind=CUSTOM_REAL) ::pres_impose
real(kind=CUSTOM_REAL) ::frac_it1,frac_it2
real(kind=CUSTOM_REAL),dimension(6):: source_zrt_temp


frac_it2=(time-t0_this_pack)/inject_dt  - (it_impose-1)
if(frac_it2.lt.-TINYVAL.or.frac_it2.gt.1+TINYVAL) then
   !print*,'frac_it2',frac_it2,time,t0_this_pack,(time-t0_this_pack)/inject_dt,(it_impose-1)
   call exit_MPI(myrank,'Error, frac_it1<0 or >1, wrong it_impose, inject_dt or t0_this_pack')
end if
frac_it1=1.0-frac_it2

source_zrt_temp(:)=0.0
!Source as moment tensor
if(SINGLE_FORCE_ENZ.eq.0) then
   source_zrt_temp(1:ncomp_source)=moment_zrt(1:ncomp_source,igll,iface)
!Source as single force
else
   source_zrt_temp(1:ncomp_source)=force_zrt(1:ncomp_source,igll,iface)
end if

ipressure=id_pressure(igll,iface)
pressure=0.0
do icomp_source=1,ncomp_source
    pres_impose=frac_it1*impose_pressure(it_impose,icomp_source,ipressure)+ &
                frac_it2*impose_pressure(it_impose+1,icomp_source,ipressure)
    pressure=pressure+source_zrt_temp(icomp_source)*pres_impose
end do
end subroutine


subroutine get_1Dvelo(myrank,SINGLE_FORCE_ENZ,velocity,igll,iface,time)
use impose_1D_BCS
use constants
implicit none
!for absorbing boundary
!same as get_1Dtraction
integer ::SINGLE_FORCE_ENZ
real(kind=CUSTOM_REAL),dimension(3):: velocity

integer ::igll,iface,myrank
real(kind=CUSTOM_REAL) ::time

!local parameters
integer ::icomp_source,ivelocity
real(kind=CUSTOM_REAL),dimension(3)::velo_temp
real(kind=CUSTOM_REAL),dimension(3,3)::zrtToxyz_temp
real(kind=CUSTOM_REAL) ::frac_it1,frac_it2
real(kind=CUSTOM_REAL),dimension(6):: source_zrt_temp

!For Lagrange interpolation
!integer::icomp_interpo,it_copy,it_fit
!real(kind=CUSTOM_REAL),dimension(NFIT) ::time_forfit,velo_forfit
!real(kind=CUSTOM_REAL) ::velo_interpo_out
!real(kind=CUSTOM_REAL) ::dvelo

!print *,'para velo'
frac_it2=(time-t0_this_pack)/inject_dt  - (it_impose-1)
if(frac_it2.lt.-TINYVAL.or.frac_it2.gt.1+TINYVAL) then
   !print*,'frac_it2',frac_it2,time,t0_this_pack,(time-t0_this_pack)/inject_dt,(it_impose-1)
   call exit_MPI(myrank,'Error, frac_it1<0 or >1, wrong it_impose, inject_dt or t0_this_pack')
end if
frac_it1=1.0-frac_it2
zrtToxyz_temp(:,:)=zrtToxyz(:,:,igll,iface)

source_zrt_temp(:)=0.0
!Source as moment tensor
if(SINGLE_FORCE_ENZ.eq.0) then
   source_zrt_temp(1:ncomp_source)=moment_zrt(1:ncomp_source,igll,iface)
!Source as single force
else
   source_zrt_temp(1:ncomp_source)=force_zrt(1:ncomp_source,igll,iface)
end if


velocity(:)=0.d0
ivelocity=id_velo(igll,iface)

do icomp_source=1,ncomp_source
  !do icomp_interpo=1,3
  !   do it_fit=1,NFIT
  !       if(inject_npt_eachpack.lt.NFIT) stop 'inject_npt_eachpack should be larger than NFIT'
  !       if(it_impose-NFIT/2.lt.1) then
  !        it_copy=it_fit
  !       else if(it_impose+NFIT/2-1.gt.inject_npt_eachpack) then
  !        it_copy=(inject_npt_eachpack-NFIT)+it_fit
  !       else
  !        it_copy=it_impose+(it_fit-1)-NFIT/2
  !       end if
  !       time_forfit(it_fit)=t0_this_pack+(it_copy-1)*inject_dt
  !       velo_forfit(it_fit)=impose_velo(it_copy,icomp_interpo,icomp_source,ivelocity)
!         print *,'interpo_velo',time_forfit(:),velo_forfit,time
  !   end do
     !call updown(NFIT,time_forfit,velo_forfit,time,velo_temp(icomp_interpo),dvelo)
     !the below interpolation has not been tested yet.
     !call lagrange_value_1d( NFIT-1,time_forfit,velo_forfit,1,time,velo_interpo_out)
  !   velo_temp(icomp_interpo)=velo_interpo_out
  !end do
  !velo_temp(1)=source_zrt_temp(icomp_source)*velo_temp(1)
  !velo_temp(2)=source_zrt_temp(icomp_source)*velo_temp(2)
  !velo_temp(3)=source_zrt_temp(icomp_source)*velo_temp(3)

  !The above lines use more accurate but expensive Lagrange interpolations. 
  !They are not used, because it turns out that the below simple linear
  !and less expensive interpolation already provides sufficiently accurate
  !results.
   velo_temp(1)=source_zrt_temp(icomp_source)* &
                ( frac_it1*impose_velo(it_impose,1,icomp_source,ivelocity)+ &
                  frac_it2*impose_velo(it_impose+1,1,icomp_source,ivelocity) )
   velo_temp(2)=source_zrt_temp(icomp_source)* &
                ( frac_it1*impose_velo(it_impose,2,icomp_source,ivelocity)+ &
                  frac_it2*impose_velo(it_impose+1,2,icomp_source,ivelocity) )
   velo_temp(3)=source_zrt_temp(icomp_source)* &
                ( frac_it1*impose_velo(it_impose,3,icomp_source,ivelocity)+ &
                  frac_it2*impose_velo(it_impose+1,3,icomp_source,ivelocity) )

   velocity(1)=velocity(1)+velo_temp(1)*zrtToxyz_temp(1,1)+velo_temp(2)*zrtToxyz_temp(2,1)+velo_temp(3)*zrtToxyz_temp(3,1)
   velocity(2)=velocity(2)+velo_temp(1)*zrtToxyz_temp(1,2)+velo_temp(2)*zrtToxyz_temp(2,2)+velo_temp(3)*zrtToxyz_temp(3,2)
   velocity(3)=velocity(3)+velo_temp(1)*zrtToxyz_temp(1,3)+velo_temp(2)*zrtToxyz_temp(2,3)+velo_temp(3)*zrtToxyz_temp(3,3)
!   if(myrank.eq.8.and.igll.eq.21.and.iface.eq.118) &
!    if(myrank.eq.0.and.iface.eq.188.and.igll.eq.13.and.DEBUG_COUPLING) &
!        print *,'check get_velo',ivelocity,icomp_source,&
!        it_impose,velocity(2),velo_temp(:),frac_it1,frac_it2,source_zrt_temp(icomp_source),&
!        velocity(1),zrtToxyz_temp(:,1),zrtToxyz_temp(:,2),velo_temp(:),&
!        impose_velo(it_impose:it_impose+1,:,icomp_source,ivelocity)

end do

end subroutine

subroutine get_1Ddisp_fluid(myrank,SINGLE_FORCE_ENZ,displacement,igll,iface,time)
use impose_1D_BCS
use constants
implicit none
!for absorbing boundary
!same to get_1Dpressure
integer ::SINGLE_FORCE_ENZ
real(kind=CUSTOM_REAL),dimension(3):: displacement

integer ::igll,iface,myrank
real(kind=CUSTOM_REAL) ::time

!local parameters
integer ::icomp_source,idisplacement
real(kind=CUSTOM_REAL),dimension(3)::disp_temp
real(kind=CUSTOM_REAL),dimension(3,3)::zrtToxyz_temp
real(kind=CUSTOM_REAL) ::frac_it1,frac_it2
real(kind=CUSTOM_REAL),dimension(9):: source_zrt_temp

!For Lagrange interpolation
!integer::icomp_interpo,it_copy,it_fit
!real(kind=CUSTOM_REAL),dimension(NFIT) ::time_forfit,disp_forfit
!real(kind=CUSTOM_REAL) ::ddisp

!print *,'para velo'
frac_it2=(time-t0_this_pack)/inject_dt  - (it_impose-1)
if(frac_it2.lt.-TINYVAL.or.frac_it2.gt.1+TINYVAL) then
   !print*,'frac_it2',frac_it2,time,t0_this_pack,(time-t0_this_pack)/inject_dt,(it_impose-1)
   call exit_MPI(myrank,'frac_it1<0 or >1, wrong it_impose, inject_dt or t0_this_pack')
end if
frac_it1=1.0-frac_it2
zrtToxyz_temp(:,:)=zrtToxyz(:,:,igll,iface)

source_zrt_temp(:)=0.0
!Source as moment tensor
if(SINGLE_FORCE_ENZ.eq.0) then
   source_zrt_temp(1:ncomp_source)=moment_zrt(1:ncomp_source,igll,iface)
!Source as single force
else
   source_zrt_temp(1:ncomp_source)=force_zrt(1:ncomp_source,igll,iface)
end if


displacement(:)=0.d0
idisplacement=id_poten_dot(igll,iface)

do icomp_source=1,ncomp_source
!  do icomp_interpo=1,3
!     do it_fit=1,NFIT
!         if(inject_npt_eachpack.lt.NFIT) stop 'inject_npt_eachpack should be larger than NFIT'
!         if(it_impose-NFIT/2.lt.1) then
!          it_copy=it_fit
!         else if(it_impose+NFIT/2-1.gt.inject_npt_eachpack) then
!          it_copy=(inject_npt_eachpack-NFIT)+it_fit
!         else
!          it_copy=it_impose+(it_fit-1)-NFIT/2
!         end if
!         time_forfit(it_fit)=t0_this_pack+(it_copy-1)*inject_dt
!         disp_forfit(it_fit)=impose_disp_fluid(it_copy,icomp_interpo,icomp_source,idisplacement)
!         print *,'interpo_disp',time_forfit(:),disp_forfit,time
!     end do
!     call updown(NFIT,time_forfit,disp_forfit,time,disp_temp(icomp_interpo),ddisp)
!  end do
!  disp_temp(1)=source_zrt_temp(icomp_source)*disp_temp(1)
!  disp_temp(2)=source_zrt_temp(icomp_source)*disp_temp(2)
!  disp_temp(3)=source_zrt_temp(icomp_source)*disp_temp(3)

  !The above lines use more accurate but expensive Lagrange interpolations. 
  !They are not used, because it turns out that the below simple linear
  !and less expensive interpolation already provides sufficiently accurate
  !results.
   disp_temp(1)=source_zrt_temp(icomp_source)* &
                ( frac_it1*impose_disp_fluid(it_impose,1,icomp_source,idisplacement)+ &
                  frac_it2*impose_disp_fluid(it_impose+1,1,icomp_source,idisplacement) )
   disp_temp(2)=source_zrt_temp(icomp_source)* &
                ( frac_it1*impose_disp_fluid(it_impose,2,icomp_source,idisplacement)+ &
                  frac_it2*impose_disp_fluid(it_impose+1,2,icomp_source,idisplacement) )
   disp_temp(3)=source_zrt_temp(icomp_source)* &
                ( frac_it1*impose_disp_fluid(it_impose,3,icomp_source,idisplacement)+ &
                  frac_it2*impose_disp_fluid(it_impose+1,3,icomp_source,idisplacement) )

   displacement(1)=displacement(1)+disp_temp(1)*zrtToxyz_temp(1,1)+disp_temp(2)*zrtToxyz_temp(2,1)+disp_temp(3)*zrtToxyz_temp(3,1)
   displacement(2)=displacement(2)+disp_temp(1)*zrtToxyz_temp(1,2)+disp_temp(2)*zrtToxyz_temp(2,2)+disp_temp(3)*zrtToxyz_temp(3,2)
   displacement(3)=displacement(3)+disp_temp(1)*zrtToxyz_temp(1,3)+disp_temp(2)*zrtToxyz_temp(2,3)+disp_temp(3)*zrtToxyz_temp(3,3)
!   if(myrank.eq.8.and.igll.eq.21.and.iface.eq.118) &
!    if(myrank.eq.0.and.iface.eq.107.and.igll.eq.1.and.DEBUG_COUPLING) &
!        print *,'check get_disp',idisplacement,icomp_source,&
!        it_impose,displacement(2),disp_temp(:),frac_it1,frac_it2,source_zrt_temp(icomp_source),&
!        displacement(1),zrtToxyz_temp(:,1),zrtToxyz_temp(:,2),disp_temp(:),&
!        impose_disp_fluid(it_impose:it_impose+1,:,icomp_source,idisplacement)

end do

end subroutine

subroutine get_1Dpotential_dot(myrank,SINGLE_FORCE_ENZ,potential_dot,igll,iface,time)
use impose_1D_BCS
implicit none
!for absorbing boundary
!same as get_1Dpressure
integer ::SINGLE_FORCE_ENZ
real(kind=CUSTOM_REAL) ::potential_dot
integer ::igll,iface,myrank
real(kind=CUSTOM_REAL) ::time

!local parameters
integer ::icomp_source,ipotential_dot
real(kind=CUSTOM_REAL) ::pot_dot_impose
real(kind=CUSTOM_REAL) ::frac_it1,frac_it2
real(kind=CUSTOM_REAL),dimension(6):: source_zrt_temp


frac_it2=(time-t0_this_pack)/inject_dt  - (it_impose-1)
if(frac_it2.lt.-TINYVAL.or.frac_it2.gt.1+TINYVAL) then
   print*,'frac_it2',frac_it2,time,t0_this_pack,(time-t0_this_pack)/inject_dt,(it_impose-1)
   call exit_MPI(myrank,'frac_it2<0 or >1, wrong it_impose, inject_dt or t0_this_pack')
end if
frac_it1=1.0-frac_it2

source_zrt_temp(:)=0.0
!Source as moment tensor
if(SINGLE_FORCE_ENZ.eq.0) then
   source_zrt_temp(1:ncomp_source)=moment_zrt(1:ncomp_source,igll,iface)
!Source as single force
else
   source_zrt_temp(1:ncomp_source)=force_zrt(1:ncomp_source,igll,iface)
end if

ipotential_dot=id_poten_dot(igll,iface)
potential_dot=0.0
do icomp_source=1,ncomp_source
    pot_dot_impose=frac_it1*impose_potential_dot(it_impose,icomp_source,ipotential_dot)+ &
                   frac_it2*impose_potential_dot(it_impose+1,icomp_source,ipotential_dot)

    potential_dot=potential_dot+source_zrt_temp(icomp_source)*pot_dot_impose
end do

end subroutine



SUBROUTINE UPDOWN (N,XI,FI,X,F,DF)
  use constants
!
! Subroutine performing the Lagrange interpolation with the
! upward and downward correction method.  F: interpolated
! value.  DF: error estimated.  Copyright (c) Tao Pang 1997.
!
  IMPLICIT NONE
  INTEGER, PARAMETER :: NMAX=21
  INTEGER, INTENT (IN) :: N
  INTEGER :: I,J,I0,J0,IT,K
!  DOUBLE PRECISION, INTENT (IN) :: X
!  DOUBLE PRECISION, INTENT (OUT) :: F,DF
!  DOUBLE PRECISION :: DX,DXT,DT
!  DOUBLE PRECISION, INTENT (IN), DIMENSION (N) :: XI,FI
!  DOUBLE PRECISION, DIMENSION (NMAX,NMAX) :: DP,DM
  real(kind=CUSTOM_REAL), INTENT (IN) :: X
  real(kind=CUSTOM_REAL), INTENT (OUT) :: F,DF
  real(kind=CUSTOM_REAL) :: DX,DXT,DT
  real(kind=CUSTOM_REAL), INTENT (IN), DIMENSION (N) :: XI,FI
  real(kind=CUSTOM_REAL), DIMENSION (NMAX,NMAX) :: DP,DM
!
  IF (N.GT.NMAX) STOP 'Dimension of the data set is too large.'
    DX = ABS(XI(N)-XI(1))
    DO  I = 1, N
      DP(I,I) = FI(I)
      DM(I,I) = FI(I)
      DXT = ABS(X-XI(I))
      IF (DXT.LT.DX) THEN
        I0 = I
        DX = DXT
      END IF
    END DO
    J0 = I0
!
! Evaluate correction matrices
!
  DO I = 1, N-1
    DO J = 1, N-I
      K = J+I
      DT =(DP(J,K-1)-DM(J+1,K))/(XI(K)-XI(J))
      DP(J,K) = DT*(XI(K)-X)
      DM(J,K) = DT*(XI(J)-X)
    END DO
  END DO
!
! Update the approximation
!
  F = FI(I0)
  IT = 0
  IF(X.LT.XI(I0)) IT = 1
 DO I = 1, N-1
    IF ((IT.EQ.1).OR.(J0.EQ.N)) THEN
      I0 = I0-1
      DF = DP(I0,J0)
      F  = F+DF
      IT = 0
      IF (J0.EQ.N) IT = 1
    ELSE IF ((IT.EQ.0).OR.(I0.EQ.1)) THEN
      J0 = J0+1
      DF = DM(I0,J0)
      F  = F+DF
      IT = 1
      IF (I0.EQ.1) IT = 0
    END IF
  END DO
  DF = ABS(DF)
END SUBROUTINE UPDOWN

!This 1d Lagrange interpolation subroutine is from the below website
!https://people.sc.fsu.edu/~jburkardt/f_src/lagrange_interp_2d/lagrange_interp_2d.html
subroutine lagrange_basis_1d ( nd, xd, ni, xi, lb ) 

!*****************************************************************************80
!
!! LAGRANGE_BASIS_1D evaluates a 1D Lagrange basis.
!
!  Licensing:
!
!    This code is distributed under the GNU LGPL license.
!
!  Modified:
!
!    09 October 2012
!
!  Author:
!
!    John Burkardt
!
!  Parameters:
!
!    Input, integer ( kind = 4 ) ND, the number of data points.
!
!    Input, real ( kind = 8 ) XD(ND), the interpolation nodes.
!
!    Input, integer ( kind = 4 ) NI, the number of evaluation points.
!
!    Input, real ( kind = 8 ) XI(NI), the evaluation points.
!
!    Output, real ( kind = 8 ) LB(NI,ND), the value, at the I-th point XI, 
!    of the Jth basis function.
!
  implicit none

  integer ( kind = 4 ) nd
  integer ( kind = 4 ) ni

  integer ( kind = 4 ) i
  integer ( kind = 4 ) j
  real ( kind = 8 ) lb(ni,nd)
  real ( kind = 8 ) xd(nd)
  real ( kind = 8 ) xi(ni)
  
  do i = 1, ni
    do j = 1, nd
      lb(i,j) = product ( ( xi(i) - xd(1:j-1)  ) / ( xd(j) - xd(1:j-1)  ) ) &
              * product ( ( xi(i) - xd(j+1:nd) ) / ( xd(j) - xd(j+1:nd) ) )
    end do
  end do

  return
end

subroutine lagrange_value_1d ( nd, xd, yd, ni, xi, yi )

!*****************************************************************************80
!
!! LAGRANGE_VALUE_1D evaluates the Lagrange interpolant.
!
!  Discussion:
!
!    The Lagrange interpolant L(ND,XD,YD)(X) is the unique polynomial of
!    degree ND-1 which interpolates the points (XD(I),YD(I)) for I = 1
!    to ND.
!
!    The Lagrange interpolant can be constructed from the Lagrange basis
!    polynomials.  Given ND distinct abscissas, XD(1:ND), the I-th Lagrange 
!    basis polynomial LB(ND,XD,I)(X) is defined as the polynomial of degree 
!    ND - 1 which is 1 at  XD(I) and 0 at the ND - 1 other abscissas.
!
!    Given data values YD at each of the abscissas, the value of the
!    Lagrange interpolant may be written as
!
!      L(ND,XD,YD)(X) = sum ( 1 <= I <= ND ) LB(ND,XD,I)(X) * YD(I)
!
!  Licensing:
!
!    This code is distributed under the GNU LGPL license.
!
!  Modified:
!
!    11 September 2012
!
!  Author:
!
!    John Burkardt
!
!  Parameters:
!
!    Input, integer ( kind = 4 ) ND, the number of data points.
!    ND must be at least 1.
!
!    Input, real ( kind = 8 ) XD(ND), the data points.
!
!    Input, real ( kind = 8 ) YD(ND), the data values.
!
!    Input, integer ( kind = 4 ) NI, the number of interpolation points.
!
!    Input, real ( kind = 8 ) XI(NI), the interpolation points.
!
!    Output, real ( kind = 8 ) YI(NI), the interpolated values.
!
  implicit none

  integer ( kind = 4 ) nd
  integer ( kind = 4 ) ni

  real ( kind = 8 ) lb(ni,nd)
  real ( kind = 8 ) xd(nd)
  real ( kind = 8 ) yd(nd)
  real ( kind = 8 ) xi(ni)
  real ( kind = 8 ) yi(ni)

  call lagrange_basis_1d ( nd, xd, ni, xi, lb )

  yi = matmul ( lb, yd )

  return
end
