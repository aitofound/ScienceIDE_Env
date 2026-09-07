subroutine set_Variables_Bound()
  use specfem_par
  use specfem_par_elastic
  use specfem_par_acoustic
  use specfem_par_poroelastic
  use tele_coupling_par
  use constants, only:DEBUG_COUPLING
  implicit none


!  integer ::j,k,iglob
  integer ::i,iele_Bound,ispec_coupling
  integer ::middle_igll,middle_jgll,middle_kgll
  if(DEBUG_COUPLING) print *,'myrank=',myrank
  nele_tele_coupling=nxLow+nxHigh+nyLow+nyHigh+nrdown+nrtop
!  nele_tele_coupling=nrdown
  allocate(coupling_ele_property(nele_tele_coupling))
  if(LOW_RESOLUTION) then
    npoints_Bound=nele_tele_coupling
  else
    npoints_Bound=nele_tele_coupling*NGLLX*NGLLZ
  end if


   

  allocate(disp_bound(NSTEP_BETWEEN_OUTPUTBOUND,NDIM,npoints_Bound))
  allocate(traction_bound(NSTEP_BETWEEN_OUTPUTBOUND,NDIM,npoints_Bound))
  allocate(normal_vect_coupling(NDIM,npoints_Bound))

  if(DEBUG_COUPLING) print *,'SUCCESS_set_Var',myrank
!  call ViaRepresent_default()
  iele_Bound=0
  if(mod(NGLLX,2).eq.0) then
     middle_igll=NGLLX/2
  else
     middle_igll=(NGLLX-1)/2+1
  end if
  if(mod(NGLLY,2).eq.0) then
     middle_jgll=NGLLY/2
  else
     middle_jgll=(NGLLY-1)/2+1
  end if
  if(mod(NGLLZ,2).eq.0) then
     middle_kgll=NGLLZ/2
  else 
     middle_kgll=(NGLLZ-1)/2+1
  end if

!the left boundary
  do i=1,nxLow
     iele_Bound=iele_Bound+1
     ispec_coupling=element_xLow(i)
     coupling_ele_property(iele_Bound)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_Bound)%iregion    =element_xLowReg(i)
     coupling_ele_property(iele_Bound)%face_type =1
     coupling_ele_property(iele_Bound)%istart=1;coupling_ele_property(iele_Bound)%iend=1
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_Bound)%jstart=1;coupling_ele_property(iele_Bound)%jend=NGLLY
        coupling_ele_property(iele_Bound)%kstart=1;coupling_ele_property(iele_Bound)%kend=NGLLZ
     else 
        coupling_ele_property(iele_Bound)%jstart=middle_jgll;coupling_ele_property(iele_Bound)%jend=middle_jgll
        coupling_ele_property(iele_Bound)%kstart=middle_kgll;coupling_ele_property(iele_Bound)%kend=middle_kgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_Bound)%is_elastic=.TRUE.
        nele_tele_couplingElas=nele_tele_couplingElas+1
     else
        coupling_ele_property(iele_Bound)%is_elastic=.FALSE.
        nele_tele_couplingAcous=nele_tele_couplingAcous+1
     end if
  end do


!the right boundary
  do i=1,nxHigh
     iele_Bound=iele_Bound+1
     ispec_coupling=element_xHigh(i)
     coupling_ele_property(iele_Bound)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_Bound)%iregion    =element_xHighReg(i)
     coupling_ele_property(iele_Bound)%face_type =2
     coupling_ele_property(iele_Bound)%istart=NGLLX;coupling_ele_property(iele_Bound)%iend=NGLLX
     if(.not.LOW_RESOLUTION) then
       coupling_ele_property(iele_Bound)%jstart=1;coupling_ele_property(iele_Bound)%jend=NGLLY
       coupling_ele_property(iele_Bound)%kstart=1;coupling_ele_property(iele_Bound)%kend=NGLLZ
     else
       coupling_ele_property(iele_Bound)%jstart=middle_jgll;coupling_ele_property(iele_Bound)%jend=middle_jgll
       coupling_ele_property(iele_Bound)%kstart=middle_kgll;coupling_ele_property(iele_Bound)%kend=middle_kgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_Bound)%is_elastic=.TRUE.
        nele_tele_couplingElas=nele_tele_couplingElas+1
     else
        coupling_ele_property(iele_Bound)%is_elastic=.FALSE.
        nele_tele_couplingAcous=nele_tele_couplingAcous+1
     end if
  end do

!the forward boundary
  do i=1,nyLow
     iele_Bound=iele_Bound+1
     ispec_coupling=element_yLow(i)
     coupling_ele_property(iele_Bound)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_Bound)%iregion    =element_yLowReg(i)
     coupling_ele_property(iele_Bound)%face_type =3
     coupling_ele_property(iele_Bound)%jstart=1;coupling_ele_property(iele_Bound)%jend=1
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_Bound)%istart=1;coupling_ele_property(iele_Bound)%iend=NGLLX
        coupling_ele_property(iele_Bound)%kstart=1;coupling_ele_property(iele_Bound)%kend=NGLLZ
     else
        coupling_ele_property(iele_Bound)%istart=middle_igll;coupling_ele_property(iele_Bound)%iend=middle_igll
        coupling_ele_property(iele_Bound)%kstart=middle_kgll;coupling_ele_property(iele_Bound)%kend=middle_kgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_Bound)%is_elastic=.TRUE.
        nele_tele_couplingElas=nele_tele_couplingElas+1
     else
        coupling_ele_property(iele_Bound)%is_elastic=.FALSE.
        nele_tele_couplingAcous=nele_tele_couplingAcous+1
     end if
  end do

!the back boundary
  do i=1,nyHigh
     iele_Bound=iele_Bound+1
     ispec_coupling=element_yHigh(i)
     coupling_ele_property(iele_Bound)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_Bound)%iregion    =element_yHighReg(i)
     coupling_ele_property(iele_Bound)%face_type =4
     coupling_ele_property(iele_Bound)%jstart=NGLLY;coupling_ele_property(iele_Bound)%jend=NGLLY
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_Bound)%istart=1;coupling_ele_property(iele_Bound)%iend=NGLLX
        coupling_ele_property(iele_Bound)%kstart=1;coupling_ele_property(iele_Bound)%kend=NGLLZ
     else
        coupling_ele_property(iele_Bound)%istart=middle_igll;coupling_ele_property(iele_Bound)%iend=middle_igll
        coupling_ele_property(iele_Bound)%kstart=middle_kgll;coupling_ele_property(iele_Bound)%kend=middle_kgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_Bound)%is_elastic=.TRUE.
        nele_tele_couplingElas=nele_tele_couplingElas+1
     else
        coupling_ele_property(iele_Bound)%is_elastic=.FALSE.
        nele_tele_couplingAcous=nele_tele_couplingAcous+1
     end if
  end do

!the bottom boundary
  do i=1,nrdown
     iele_Bound=iele_Bound+1
     ispec_coupling=element_rdown(i)
     coupling_ele_property(iele_Bound)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_Bound)%iregion    =element_rdownReg(i)
     coupling_ele_property(iele_Bound)%face_type =5
     coupling_ele_property(iele_Bound)%kstart=1;coupling_ele_property(iele_Bound)%kend=1
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_Bound)%istart=1;coupling_ele_property(iele_Bound)%iend=NGLLX
        coupling_ele_property(iele_Bound)%jstart=1;coupling_ele_property(iele_Bound)%jend=NGLLY
     else
        coupling_ele_property(iele_Bound)%istart=middle_igll;coupling_ele_property(iele_Bound)%iend=middle_igll
        coupling_ele_property(iele_Bound)%jstart=middle_jgll;coupling_ele_property(iele_Bound)%jend=middle_jgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_Bound)%is_elastic=.TRUE.
        nele_tele_couplingElas=nele_tele_couplingElas+1
     else
        coupling_ele_property(iele_Bound)%is_elastic=.FALSE.
        nele_tele_couplingAcous=nele_tele_couplingAcous+1
     end if
  end do
!the top boundary
  do i=1,nrtop
     iele_Bound=iele_Bound+1
     ispec_coupling=element_rtop(i)
     coupling_ele_property(iele_Bound)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_Bound)%iregion    =element_rtopReg(i)
     coupling_ele_property(iele_Bound)%face_type =6
     coupling_ele_property(iele_Bound)%kstart=NGLLZ;coupling_ele_property(iele_Bound)%kend=NGLLZ
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_Bound)%istart=1;coupling_ele_property(iele_Bound)%iend=NGLLX
        coupling_ele_property(iele_Bound)%jstart=1;coupling_ele_property(iele_Bound)%jend=NGLLY
     else
        coupling_ele_property(iele_Bound)%istart=middle_igll;coupling_ele_property(iele_Bound)%iend=middle_igll
        coupling_ele_property(iele_Bound)%jstart=middle_jgll;coupling_ele_property(iele_Bound)%jend=middle_jgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_Bound)%is_elastic=.TRUE.
        nele_tele_couplingElas=nele_tele_couplingElas+1
     else
        coupling_ele_property(iele_Bound)%is_elastic=.FALSE.
        nele_tele_couplingAcous=nele_tele_couplingAcous+1
     end if
  end do

  if(iele_Bound.ne.nele_tele_coupling) STOP 'The boundary element copying failed'


  if(LOW_RESOLUTION) then
    npoints_BoundElas=nele_tele_couplingElas
    npoints_BoundAcous=nele_tele_couplingAcous
  else
    npoints_BoundElas=nele_tele_couplingElas*NGLLX*NGLLZ
    npoints_BoundAcous=nele_tele_couplingAcous*NGLLX*NGLLZ
  end if

  if(mod(npoints_BoundElas,NPOINTS_PER_PACK).eq.0) then
    npackage_Elas=npoints_BoundElas/NPOINTS_PER_PACK
    allocate(npoints_ipack_Elas(npackage_Elas))
    npoints_ipack_Elas(:)=NPOINTS_PER_PACK
  else 
    npackage_Elas=int(npoints_BoundElas/NPOINTS_PER_PACK)+1
    allocate(npoints_ipack_Elas(npackage_Elas))
    npoints_ipack_Elas(1:npackage_Elas-1)=NPOINTS_PER_PACK
    npoints_ipack_Elas(npackage_Elas)=mod(npoints_BoundElas,NPOINTS_PER_PACK)
  end if



  if(DEBUG_COUPLING) print *,'SUCCESS1_set_Var',myrank,npackage_Elas
  
end subroutine set_Variables_Bound




subroutine elements_strain_saved()
  use tele_coupling_par
  use specfem_par
  use constants
  use specfem_par_elastic

  implicit none
  integer ::ispec,i,j,k

  double precision ::r,depth,x,y,z
  double precision ::x_refe_this_depth,y_refe_this_depth,z_refe_this_depth,distance
  double precision ::sin_lat,cos_lat,sin_lon,cos_lon
  double precision, parameter ::mtokm=0.001

  integer ::iele_strain_saved
  character(len=256) ::final_LOCAL_PATH,clean_LOCAL_PATH,dir_this_proc
  logical ::dir_exist
  integer ::ier,ispec_tmp,imin,imax,jmin,jmax,kmin,kmax


  allocate(save_strain(NSPEC_AB))
  allocate(iele_strain_saved_ispec(NSPEC_AB))

  sin_lat=sin(LAT_CENTER_STRAIN_SAVED*PI/180.0)
  cos_lat=cos(LAT_CENTER_STRAIN_SAVED*PI/180.0)
  sin_lon=sin(LON_CENTER_STRAIN_SAVED*PI/180.0)
  cos_lon=cos(LON_CENTER_STRAIN_SAVED*PI/180.0)
  nele_strain_saved=0
  do ispec=1,NSPEC_AB
    save_strain(ispec)=.false.
    ! elastic simulations
    if(ispec_is_elastic(ispec)) then
       i=(NGLLX+1)/2;j=(NGLLY+1)/2;k=(NGLLZ+1)/2
       x=xstore(ibool(i,j,k,ispec))
       y=ystore(ibool(i,j,k,ispec))
       z=zstore(ibool(i,j,k,ispec))
       r=dsqrt(x**2+y**2+z**2)
       depth=(R_EARTH_SURF-r)*mtokm
       x_refe_this_depth=r*cos_lat*cos_lon
       y_refe_this_depth=r*cos_lat*sin_lon
       z_refe_this_depth=r*sin_lat
       distance=dsqrt((x-x_refe_this_depth)**2+(y-y_refe_this_depth)**2+(z-z_refe_this_depth)**2)*mtokm
       !print
       !*,"coord-strain_saved",x,y,z,x_refe_this_depth,y_refe_this_depth,z_refe_this_depth
       !print
       !*,'dist-depth',distance,RADIUS_STRAIN_SAVED,depth,MIN_DEP_STRAIN_SAVED,MAX_DEP_STRAIN_SAVED
       if(distance<RADIUS_STRAIN_SAVED.and.depth>=MIN_DEP_STRAIN_SAVED.and.depth<=MAX_DEP_STRAIN_SAVED) then
           save_strain(ispec)=.true.
           nele_strain_saved=nele_strain_saved+1
       end if
    end if
  end do
  print *,'nele_strain_saved',nele_strain_saved,myrank

!alllocate memories
  allocate(ispec_iele_strain_saved(nele_strain_saved))

!fill out the list ispec_iele_strain_saved
  iele_strain_saved=0
  iele_strain_saved_ispec(:)=0
  do ispec=1,NSPEC_AB
    if(save_strain(ispec)) then
       iele_strain_saved=iele_strain_saved+1
       ispec_iele_strain_saved(iele_strain_saved)=ispec
       iele_strain_saved_ispec(ispec)=iele_strain_saved
       if(myrank.eq.0) then
          print *,'iele_strain_saved_ispec',ispec,iele_strain_saved_ispec(ispec)
       end if
    end if
  end do

!create the folders for saving the strains
  if(nele_strain_saved.gt.0) then
!  if( USE_OUTPUT_FILES_PATH ) then
!      final_LOCAL_PATH = OUTPUT_FILES_PATH(1:len_trim(OUTPUT_FILES_PATH))!
!      //'/'
!  else
      ! suppress white spaces if any
      clean_LOCAL_PATH = adjustl(LOCAL_PATH)
      ! create full final local path
      final_LOCAL_PATH = clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))! //'/'
!  endif

     dir_this_proc=trim(final_LOCAL_PATH)

     write(dir_this_proc,"(a,'iproc',i4.4,'/')")trim(dir_this_proc),myrank

     !create the folders
!     call system('mkdir -p '// adjustl(trim( dir_this_proc ) ) )
!     call system('echo  > '// adjustl(trim( dir_this_proc )
!     )//'/test_folder_exist.txt' )
     !wait until the folder is created
!     dir_exist=.false.
!     do while (.not.dir_exist)
!WENBO tried the below function, but it only works for ifort and not gfortran.
!        inquire(directory=adjustl(trim(dir_this_proc)),exist=dir_exist)

!        inquire(file=adjustl(trim(dir_this_proc))//'/test_folder_exist.txt',exist=dir_exist)
!     end do
!delete the testing file.
!     call system('rm  '// adjustl(trim( dir_this_proc
!     ))//'/test_folder_exist.txt' )


     npoints_strain_saved=0
     open(unit=301,file=trim(dir_this_proc)//"coords_strain_saved.txt",status='unknown',&
         action='write',form='formatted',iostat=ier)
     do iele_strain_saved=1,nele_strain_saved
        ispec_tmp=ispec_iele_strain_saved(iele_strain_saved)
        if(LOW_RESOLUTION) then
           imin=(NGLLX+1)/2;imax=imin
           jmin=(NGLLY+1)/2;jmax=jmin
           kmin=(NGLLZ+1)/2;kmax=kmin
        else
           imin=2;imax=NGLLX
           jmin=2;jmax=NGLLY
           kmin=2;kmax=NGLLZ
        end if

        !The order of k,j,i should be consistent with that in saving
        !strain_saved in compute_forces_elastic_Dev.f90 and
        !compute_forces_elastic_noDev.f90
        do k=kmin,kmax
            do j=jmin,jmax
                do i=imin,imax
                   npoints_strain_saved=npoints_strain_saved+1
                   write(301,*) xstore(ibool(i,j,k,ispec_tmp)),ystore(ibool(i,j,k,ispec_tmp)),&
                        zstore(ibool(i,j,k,ispec_tmp))
                end do
            end do
        end do
     end do
     close(301)
  end if

  if(LOW_RESOLUTION.and.npoints_strain_saved.ne.nele_strain_saved) &
     call exit_mpi(myrank,'Error, npoints_strain_saved should be equal to nele_strain_saved in LOW_RESOLUTION mode!')
  if(.not.LOW_RESOLUTION.and.npoints_strain_saved.ne.(NGLLX-1)*(NGLLY-1)*(NGLLZ-1)*nele_strain_saved) &
     call exit_mpi(myrank,'Error, npoints_strain_saved should be equal to &
                           (NGLLX1)*(NGLLY-1)*(NGLLZ-1)*nele_strain_saved in LOW_RESOLUTION mode!')


  allocate(strain_saved(NSTEP_BETWEEN_OUTPUTBOUND,6,npoints_strain_saved))

!  deallocate(save_strain)


end subroutine

