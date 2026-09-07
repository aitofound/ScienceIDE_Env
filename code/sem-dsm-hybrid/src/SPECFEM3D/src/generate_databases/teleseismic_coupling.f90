subroutine teleseismic_coupling(myrank,kappastore,mustore,&
            ibool,nspec,nglob,NPROC,ispec_is_elastic,ispec_is_acoustic,&
            IMAIN_OUTPUT,prname,LOCAL_PATH,&
            c11store,c12store,c13store,c14store,c15store,c16store,&
            c22store,c23store,c24store,c25store,c26store,c33store,&
            c34store,c35store,c36store,c44store,c45store,c46store,&
            c55store,c56store,c66store,ANISOTROPY,NSPEC_ANISO,&
            xstore_dummy,ystore_dummy,zstore_dummy,xigll,yigll,zigll,&
            wxgll,wygll,wzgll,wgllwgll_xy,&
            wgllwgll_xz,wgllwgll_yz)
 
  use tele_coupling_par
  implicit none
  
  integer ::myrank,nspec,nglob,NPROC,IMAIN_OUTPUT,NSPEC_ANISO
  logical ::ANISOTROPY

  integer, dimension(NGLLX,NGLLY,NGLLZ,nspec) :: ibool
  real(kind=CUSTOM_REAL), dimension(nglob) :: xstore_dummy,ystore_dummy,zstore_dummy

  logical, dimension(nspec) :: ispec_is_acoustic,ispec_is_elastic
  character(len=256) prname,LOCAL_PATH


  real(kind=CUSTOM_REAL), dimension(NGLLX,NGLLY,NGLLZ,nspec)::kappastore,mustore
  real(kind=CUSTOM_REAL), dimension(NGLLX,NGLLY,NGLLZ,NSPEC_ANISO) :: &
            c11store,c12store,c13store,c14store,c15store,c16store, &
            c22store,c23store,c24store,c25store,c26store,c33store, &
            c34store,c35store,c36store,c44store,c45store,c46store, &
            c55store,c56store,c66store

  double precision, dimension(NGLLX)::xigll,wxgll
  double precision, dimension(NGLLY)::yigll,wygll
  double precision, dimension(NGLLZ)::zigll,wzgll
  double precision, dimension(NGLLX,NGLLY) :: wgllwgll_xy
  double precision, dimension(NGLLX,NGLLZ) :: wgllwgll_xz
  double precision, dimension(NGLLY,NGLLZ) :: wgllwgll_yz



!  print *,'ANISOTROPY',ANISOTROPY
  call read_coupling_element(myrank,prname,LOCAL_PATH)
  call set_tele_coupling_variables(myrank,ispec_is_elastic,ispec_is_acoustic,nspec)
  call save_package_list(myrank,LOCAL_PATH,NPROC)

!  print *,'save_bound_depid'
!  call sync_all()
  ! construct and save the depth and distance tables for the inner coupling
  ! boundary
  call save_depth_id_inner(myrank,NPROC,nele_tele_coupling,&
                ispec_is_elastic,ispec_is_acoustic,nspec,ibool,nglob,xstore_dummy,&
                ystore_dummy,zstore_dummy,1,IMAIN_OUTPUT,prname,LOCAL_PATH)
  call save_depth_id_inner(myrank,NPROC,nele_tele_coupling,&
                ispec_is_elastic,ispec_is_acoustic,nspec,ibool,nglob,xstore_dummy,&
                ystore_dummy,zstore_dummy,2,IMAIN_OUTPUT,prname,LOCAL_PATH)
  call get_tele_coupling_surfaces(myrank,xstore_dummy,ystore_dummy,&
            zstore_dummy,ibool,nspec,&
            nglob,kappastore,mustore,LOCAL_PATH,&
            c11store,c12store,c13store,c14store,c15store,c16store,&
            c22store,c23store,c24store,c25store,c26store,c33store,&
            c34store,c35store,c36store,c44store,c45store,c46store,&
            c55store,c56store,c66store,ANISOTROPY,NSPEC_ANISO,&
            xigll,yigll,zigll,wxgll,wygll,wzgll,&
            wgllwgll_xy,wgllwgll_xz,wgllwgll_yz,ispec_is_elastic,&
            ispec_is_acoustic)

end subroutine teleseismic_coupling
