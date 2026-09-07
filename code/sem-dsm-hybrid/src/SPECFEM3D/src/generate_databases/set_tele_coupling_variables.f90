subroutine set_tele_coupling_variables(myrank,ispec_is_elastic,ispec_is_acoustic,nspec)
  use tele_coupling_par
  use constants, only: DEBUG_COUPLING
  implicit none

  integer ::myrank,nspec
  logical, dimension(nspec) :: ispec_is_acoustic,ispec_is_elastic

!  integer ::j,k,iglob
  integer ::i,iele_coupling,ispec_coupling
  integer ::middle_igll,middle_jgll,middle_kgll
!  double precision:: temp
!  real(kind=CUSTOM_REAL) ::temp1
  logical ::logical_tmp
!The below line are used to avoid possible error reports during compling
!the code. ispec_is_acoustic is not used now, that occationally causes error 
!reports. But they might be usful in the future, so we keep it here.
  logical_tmp=ispec_is_acoustic(1)

  if(DEBUG_COUPLING) print *,'myrank=',myrank,ispec_is_acoustic(1)
  nele_tele_coupling=coupling_nspec_xlow+coupling_nspec_xhigh+coupling_nspec_ylow+coupling_nspec_yhigh+ &
     coupling_nspec_rbottom+coupling_nspec_rtop
  allocate(coupling_ele_property(nele_tele_coupling))
  if(LOW_RESOLUTION) then
    npoints_tele_coupling=nele_tele_coupling
  else
    npoints_tele_coupling=nele_tele_coupling*NGLLX*NGLLZ
  end if

  allocate(normal_vect_coupling(NDIM,npoints_tele_coupling))
  allocate(id_depth_coupling(npoints_tele_coupling))
  allocate(id_dist_coupling(npoints_tele_coupling))

  if(DEBUG_COUPLING) print *,'SUCCESS_set_Var',myrank
  iele_coupling=0
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
  do i=1,coupling_nspec_xlow
     iele_coupling=iele_coupling+1
     ispec_coupling=coupling_ispec_xlow(i)
     coupling_ele_property(iele_coupling)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_coupling)%iregion    =coupling_isubregion_xlow(i)
     coupling_ele_property(iele_coupling)%face_type =1
     coupling_ele_property(iele_coupling)%istart=1;coupling_ele_property(iele_coupling)%iend=1
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_coupling)%jstart=1;coupling_ele_property(iele_coupling)%jend=NGLLY
        coupling_ele_property(iele_coupling)%kstart=1;coupling_ele_property(iele_coupling)%kend=NGLLZ
     else 
        coupling_ele_property(iele_coupling)%jstart=middle_jgll;coupling_ele_property(iele_coupling)%jend=middle_jgll
        coupling_ele_property(iele_coupling)%kstart=middle_kgll;coupling_ele_property(iele_coupling)%kend=middle_kgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_coupling)%is_elastic=.TRUE.
        nele_tele_coupling_elas=nele_tele_coupling_elas+1
     else
        coupling_ele_property(iele_coupling)%is_elastic=.FALSE.
        nele_tele_coupling_acous=nele_tele_coupling_acous+1
     end if
  end do


!the right boundary
  do i=1,coupling_nspec_xhigh
     iele_coupling=iele_coupling+1
     ispec_coupling=coupling_ispec_xhigh(i)
     coupling_ele_property(iele_coupling)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_coupling)%iregion    =coupling_isubregion_xhigh(i)
     coupling_ele_property(iele_coupling)%face_type =2
     coupling_ele_property(iele_coupling)%istart=NGLLX;coupling_ele_property(iele_coupling)%iend=NGLLX
     if(.not.LOW_RESOLUTION) then
       coupling_ele_property(iele_coupling)%jstart=1;coupling_ele_property(iele_coupling)%jend=NGLLY
       coupling_ele_property(iele_coupling)%kstart=1;coupling_ele_property(iele_coupling)%kend=NGLLZ
     else
       coupling_ele_property(iele_coupling)%jstart=middle_jgll;coupling_ele_property(iele_coupling)%jend=middle_jgll
       coupling_ele_property(iele_coupling)%kstart=middle_kgll;coupling_ele_property(iele_coupling)%kend=middle_kgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_coupling)%is_elastic=.TRUE.
        nele_tele_coupling_elas=nele_tele_coupling_elas+1
     else
        coupling_ele_property(iele_coupling)%is_elastic=.FALSE.
        nele_tele_coupling_acous=nele_tele_coupling_acous+1
     end if
  end do

!the forward boundary
  do i=1,coupling_nspec_ylow
     iele_coupling=iele_coupling+1
     ispec_coupling=element_yLow(i)
     coupling_ele_property(iele_coupling)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_coupling)%iregion    =element_yLowReg(i)
     coupling_ele_property(iele_coupling)%face_type =3
     coupling_ele_property(iele_coupling)%jstart=1;coupling_ele_property(iele_coupling)%jend=1
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_coupling)%istart=1;coupling_ele_property(iele_coupling)%iend=NGLLX
        coupling_ele_property(iele_coupling)%kstart=1;coupling_ele_property(iele_coupling)%kend=NGLLZ
     else
        coupling_ele_property(iele_coupling)%istart=middle_igll;coupling_ele_property(iele_coupling)%iend=middle_igll
        coupling_ele_property(iele_coupling)%kstart=middle_kgll;coupling_ele_property(iele_coupling)%kend=middle_kgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_coupling)%is_elastic=.TRUE.
        nele_tele_coupling_elas=nele_tele_coupling_elas+1
     else
        coupling_ele_property(iele_coupling)%is_elastic=.FALSE.
        nele_tele_coupling_acous=nele_tele_coupling_acous+1
     end if
  end do

!the back boundary
  do i=1,coupling_nspec_yhigh
     iele_coupling=iele_coupling+1
     ispec_coupling=coupling_ispec_yhigh(i)
     coupling_ele_property(iele_coupling)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_coupling)%iregion    =coupling_isubregion_yhigh(i)
     coupling_ele_property(iele_coupling)%face_type =4
     coupling_ele_property(iele_coupling)%jstart=NGLLY;coupling_ele_property(iele_coupling)%jend=NGLLY
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_coupling)%istart=1;coupling_ele_property(iele_coupling)%iend=NGLLX
        coupling_ele_property(iele_coupling)%kstart=1;coupling_ele_property(iele_coupling)%kend=NGLLZ
     else
        coupling_ele_property(iele_coupling)%istart=middle_igll;coupling_ele_property(iele_coupling)%iend=middle_igll
        coupling_ele_property(iele_coupling)%kstart=middle_kgll;coupling_ele_property(iele_coupling)%kend=middle_kgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_coupling)%is_elastic=.TRUE.
        nele_tele_coupling_elas=nele_tele_coupling_elas+1
     else
        coupling_ele_property(iele_coupling)%is_elastic=.FALSE.
        nele_tele_coupling_acous=nele_tele_coupling_acous+1
     end if
  end do

!the bottom boundary
  do i=1,coupling_nspec_rbottom
     iele_coupling=iele_coupling+1
     ispec_coupling=coupling_ispec_rbottom(i)
     coupling_ele_property(iele_coupling)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_coupling)%iregion    =coupling_isubregion_rbottom(i)
     coupling_ele_property(iele_coupling)%face_type =5
     coupling_ele_property(iele_coupling)%kstart=1;coupling_ele_property(iele_coupling)%kend=1
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_coupling)%istart=1;coupling_ele_property(iele_coupling)%iend=NGLLX
        coupling_ele_property(iele_coupling)%jstart=1;coupling_ele_property(iele_coupling)%jend=NGLLY
     else
        coupling_ele_property(iele_coupling)%istart=middle_igll;coupling_ele_property(iele_coupling)%iend=middle_igll
        coupling_ele_property(iele_coupling)%jstart=middle_jgll;coupling_ele_property(iele_coupling)%jend=middle_jgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_coupling)%is_elastic=.TRUE.
        nele_tele_coupling_elas=nele_tele_coupling_elas+1
     else
        coupling_ele_property(iele_coupling)%is_elastic=.FALSE.
        nele_tele_coupling_acous=nele_tele_coupling_acous+1
     end if
  end do
!the top boundary
  do i=1,coupling_nspec_rtop
     iele_coupling=iele_coupling+1
     ispec_coupling=coupling_ispec_rtop(i)
     coupling_ele_property(iele_coupling)%ispec_coupling=ispec_coupling
     coupling_ele_property(iele_coupling)%iregion    =coupling_isubregion_rtop(i)
     coupling_ele_property(iele_coupling)%face_type =6
     coupling_ele_property(iele_coupling)%kstart=NGLLZ;coupling_ele_property(iele_coupling)%kend=NGLLZ
     if(.not.LOW_RESOLUTION) then
        coupling_ele_property(iele_coupling)%istart=1;coupling_ele_property(iele_coupling)%iend=NGLLX
        coupling_ele_property(iele_coupling)%jstart=1;coupling_ele_property(iele_coupling)%jend=NGLLY
     else
        coupling_ele_property(iele_coupling)%istart=middle_igll;coupling_ele_property(iele_coupling)%iend=middle_igll
        coupling_ele_property(iele_coupling)%jstart=middle_jgll;coupling_ele_property(iele_coupling)%jend=middle_jgll
     end if
     if(ispec_is_elastic(ispec_coupling)) then
        coupling_ele_property(iele_coupling)%is_elastic=.TRUE.
        nele_tele_coupling_elas=nele_tele_coupling_elas+1
     else
        coupling_ele_property(iele_coupling)%is_elastic=.FALSE.
        nele_tele_coupling_acous=nele_tele_coupling_acous+1
     end if
  end do

  if(iele_coupling.ne.nele_tele_coupling) STOP 'The boundary element copying failed'


  if(LOW_RESOLUTION) then
    npoints_tele_coupling_elas=nele_tele_coupling_elas
    npoints_tele_coupling_acous=nele_tele_coupling_acous
  else
    npoints_tele_coupling_elas=nele_tele_coupling_elas*NGLLX*NGLLZ
    npoints_tele_coupling_acous=nele_tele_coupling_acous*NGLLX*NGLLZ
  end if

  if(mod(npoints_tele_coupling_elas,NPOINTS_PER_PACK).eq.0) then
    npackage_elas=npoints_tele_coupling_elas/NPOINTS_PER_PACK
    allocate(npoints_ipack_elas(npackage_elas))
    npoints_ipack_elas(:)=NPOINTS_PER_PACK
  else 
    npackage_elas=int(npoints_tele_coupling_elas/NPOINTS_PER_PACK)+1
    allocate(npoints_ipack_elas(npackage_elas))
    npoints_ipack_elas(1:npackage_elas-1)=NPOINTS_PER_PACK
    npoints_ipack_elas(npackage_elas)=mod(npoints_tele_coupling_elas,NPOINTS_PER_PACK)
  end if

  if(mod(npoints_tele_coupling,NPOINTS_PER_PACK).eq.0) then
    npackage_elas_acous=npoints_tele_coupling/NPOINTS_PER_PACK
    allocate(npoints_ipack_elas_acous(npackage_elas_acous))
    npoints_ipack_elas_acous(:)=NPOINTS_PER_PACK
  else
    npackage_elas_acous=int(npoints_tele_coupling/NPOINTS_PER_PACK)+1
    allocate(npoints_ipack_elas_acous(npackage_elas_acous))
    npoints_ipack_elas_acous(1:npackage_elas_acous-1)=NPOINTS_PER_PACK
    npoints_ipack_elas_acous(npackage_elas_acous)=mod(npoints_tele_coupling,NPOINTS_PER_PACK)
  end if


  
  if(DEBUG_COUPLING) print *,'SUCCESS2_set_Var',myrank,nele_tele_coupling,npackage_elas,npackage_elas_acous
!  call sync_all()
end subroutine set_tele_coupling_variables
