!***************************************************************************
module struct_defined
implicit none
!***********The information used in the coupling integral**********
Type coupling_face_property

integer                 ::ispec_coupling
integer                 ::iregion  !regular(1,3,5) and unregular(2,4)
integer                 ::face_type  !5 or 6 kinds of face (left,right,top,bottom,back,forward)
integer                 ::iele_elasitc
integer                 ::iele_acoustic
integer                 ::istart,iend,jstart,jend,kstart,kend
logical                 ::is_elastic

end type coupling_face_property
!******************************************************************
end module struct_defined
!***************************************************************************


module tele_coupling_par
   use constants
   use struct_defined
   integer, parameter ::Imain_SEM_Par_Coupling=1001
   integer ::npackages_total_coupling

   integer ::nele_tele_coupling
   integer ::nele_tele_coupling_elas
   integer ::nele_tele_coupling_acous
!  If LOW_RESOLUTION=.true., npoints_tele_coupling=nele_tele_coupling
!  Other wise, npoints_tele_coupling=nele_tele_coupling*NGLLX*NGLLZ. 
   integer ::npoints_tele_coupling
   integer ::npoints_tele_coupling_elas
   integer ::npoints_tele_coupling_acous

   Type(coupling_face_property),dimension(:),allocatable ::coupling_ele_property
   integer, dimension(:), allocatable ::id_depth_coupling
   integer, dimension(:), allocatable ::id_dist_coupling

   real(kind=CUSTOM_REAL), dimension(:,:), allocatable ::normal_vect_coupling
   integer ::npackage_elas,npackage_elas_acous
   integer, dimension(:), allocatable::npoints_ipack_elas,npoints_ipack_elas_acous
   
! for the rank=0 process
   integer ::recv_npackage_elas,recv_npackage_elas_acous
   integer, dimension(:),allocatable::recv_npoints_ipack_elas,recv_npoints_ipack_elas_acous

! used when CPML_conditions is enabled
  double precision, dimension(NDIM,NDIM) :: rotation_matrix_back_cubedsph
! global point coordinates
  real(kind=CUSTOM_REAL), dimension(:), allocatable :: xstore_dummy_cubedsph
  real(kind=CUSTOM_REAL), dimension(:), allocatable :: ystore_dummy_cubedsph
  real(kind=CUSTOM_REAL), dimension(:), allocatable :: zstore_dummy_cubedsph

!  USED IN MESH AND GENERATE_DATABASE
   double precision:: R_TOP_BOUND,ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES,THICKNESS_BLOCK_KM, &
                   CENTER_LATITUDE_IN_DEGREES,CENTER_LONGITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH
   integer::NXI_TOPOGRAPHY_TAPER,NETA_TOPOGRAPHY_TAPER,NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY
   integer::COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,&
            COUPLING_IR_TOP,COUPLING_IR_BOTTOM
   double precision ::COUPLING_DEPTH_TOLERENCE,COUPLING_DIST_TOLERENCE

!  USED IN SOLVER PART
! used to save strain in the receiver-side coupling
   double precision ::LAT_CENTER_STRAIN_SAVED,LON_CENTER_STRAIN_SAVED,RADIUS_STRAIN_SAVED
   double precision ::MIN_DEP_STRAIN_SAVED,MAX_DEP_STRAIN_SAVED
! used to output the strains (receiver-side coupling) or
! displacements/tractions (deep-Earth or source-side coupling).
   logical ::LOW_RESOLUTION
   integer ::NSTEP_BETWEEN_OUTPUTBOUND
   integer ::DECIMATE_COUPLING
   integer ::NPOINTS_PER_PACK

! USED IN MESH, GENERATE_DATABASE and SOLVER
   integer ::coupling_nspec_xlow,coupling_nspec_xhigh,coupling_nspec_ylow,coupling_nspec_yhigh,&
             coupling_nspec_rbottom,coupling_nspec_rtop
   integer,dimension(:),allocatable::coupling_ispec_xlow,coupling_isubregion_xlow,coupling_ispec_xhigh,&
           coupling_isubregion_xhigh,element_yLow,element_yLowReg,coupling_ispec_yhigh, &
           coupling_isubregion_yhigh,coupling_ispec_rbottom,coupling_isubregion_rbottom,&
           coupling_ispec_rtop,coupling_isubregion_rtop

end module tele_coupling_par
