!***************************************************************************
module struct_defined
use constants
implicit none

!***********The information used in the coupling integral**********
Type coupling_face_property

integer                 ::ispec_coupling
integer                 ::iregion  !regular(1,3,5) and unregular(2,4)
integer                 ::face_type  !5 or 6 kinds of faces(left,right,top,bottom,back,forward)
integer                 ::iele_elasitc
integer                 ::iele_acoustic
integer                 ::istart,iend,jstart,jend,kstart,kend
logical                 ::is_elastic

end type coupling_face_property
!******************************************************************
end module struct_defined
!***************************************************************************


!*****variables used to impose 1-D tractions on the coupling boundaries*****
module impose_1D_BCS
use constants

!single variables are difend as type of double precision.
!array is type of CUSTOM_REAL.
character(len=80),dimension(6) ::moment_name
character(len=80),dimension(1) ::explosion_name
character(len=80),dimension(3) ::force_name
data moment_name /'Mzz','Mrr','Mtt','Mzr','Mzt','Mrt'/
data explosion_name /'explosion'/
data force_name /'Fr','Ft','Fz'/

!wavefield package ID and time step ID for imposed wavefield
integer ::ipack_impose,ipack_old,it_impose

!relative starting time of the wavefield for specific pacakge file
!i.e., the starting time of the first package is zero
double precision::t0_this_pack

!parameters for imposed Green's functions
double precision ::inject_dt
integer  ::inject_ndep_elas,inject_ndep_acous
integer  ::max_inject_ndist_elas,max_inject_ndist_acous
!number of total time steps
integer  ::inject_npt
!number of wavefield package
integer  ::inject_npack
!number of time steps for each pacakge
integer  ::inject_npt_eachpack
!absolute starting and ending time of the available wavefield including all the packages
double precision ::inject_time_start,inject_time_end

!number of Green's function distances for deach depth
integer, dimension(:),allocatable ::ndist_idepth_elas,ndist_idepth_acous

!property associated with each point on each face
!id to find the corresponding traction or velocity in the table
integer, dimension(:,:), allocatable ::id_epsilon,id_pressure,id_velo,id_poten_dot
!Matrix for transforming between the SEM Cartesian coordinate system and the DSM
!z-r-t coordinate system.
double precision, dimension(:,:,:,:), allocatable ::zrtToxyz
!number of source components. 6 for moment tensor and 3 for single force 
integer ::ncomp_source
!number of source components selected. Not all the source components are needed.
!For example, injection of an explosion source's wavefield only needs three diagonal 
!components of moment tensor; Vertical component simulation in a single-force mode only 
!needs the results of vertical force 'Fz'; East or North component simulation only needs 
!results of two forces 'Fr' and 'Ft'.
integer ::ncomp_source_selected
!The source components selected. The maximum number is up to 6 for a moment tensor
!source, so we define it with a dimension of 6.
character(len=80),dimension(6) :: sources_selected
!converted source components in the DSM z-r-t coordinate system
real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::moment_zrt
real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::force_zrt


!variables to read the injected wavefield, including strain/velocity (solid
!media) and pressure/potential_dot (fluid media)
!nepsilon is the number of Green's functions in solid media to read, 
!as a function of depth and distance. nvelo=nepsilon
!npressure is for fluid media. npressure=npoten_dot=ndisp_fluid
integer ::nepsilon,nvelo
integer ::npressure,npoten_dot,ndisp_fluid
!integer,dimension(:,:), allocatable ::impose_depPress,impose_depEpsilon
integer,dimension(:),allocatable ::idepth_used_ela,idepth_used_acou
integer :: ndepth_used_ela,ndepth_used_acou
integer,dimension(:,:,:), allocatable ::idep_idist_impose
integer,dimension(:,:), allocatable ::idep_idist_impose_elas,idep_idist_impose_acous
real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::impose_pressure
real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::impose_potential_dot
real(kind=CUSTOM_REAL), dimension(:,:,:,:), allocatable::impose_disp_fluid
real(kind=CUSTOM_REAL), dimension(:,:,:,:), allocatable ::impose_epsilon
real(kind=CUSTOM_REAL), dimension(:,:,:,:), allocatable ::impose_velo
end module impose_1D_BCS



module tele_coupling_par
   use struct_defined
   integer, parameter ::Imain_SEM_Par_Coupling=1001
   integer ::npackages_total_coupling
   

   integer ::nele_tele_coupling
   integer ::nele_tele_coupling_elas
   integer ::nele_tele_coupling_acous
   integer ::npoints_tele_coupling
   integer ::npoints_tele_coupling_elas
   integer ::npoints_tele_coupling_acous
   Type(coupling_face_property),dimension(:),allocatable ::coupling_ele_property
   real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::disp_coupling,traction_coupling
!   real(kind=CUSTOM_REAL), dimension(:,:), allocatable ::pressure_bound
   real(kind=CUSTOM_REAL), dimension(:,:), allocatable ::normal_vect_coupling
   integer ::npackage_elas
   integer, dimension(:), allocatable ::npoints_ipack_elas
   
! for the rank=0 process, but only used in the generate database step
!   integer ::recv_npackage_elas
!   integer, dimension(:),allocatable ::recv_npoints_ipack_elas
!   real(kind=CUSTOM_REAL), dimension(3,3) ::rotation_matrix

!  USED IN MESH AND GENERATE_DATABASE
   double precision:: R_TOP_BOUND,ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES, &
                   CENTER_LATITUDE_IN_DEGREES,CENTER_LONGITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH
   integer::NXI_TOPOGRAPHY_TAPER,NETA_TOPOGRAPHY_TAPER,NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY
   integer::COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,COUPLING_IR_TOP,COUPLING_IR_BOTTOM
   double precision ::COUPLING_DEPTH_TOLERENCE,COUPLING_DIST_TOLERENCE

!  USED IN SOLVER PART
!  specity the region where strains will be stored.
   double precision ::LAT_CENTER_STRAIN_SAVED,LON_CENTER_STRAIN_SAVED,RADIUS_STRAIN_SAVED
   double precision ::MIN_DEP_STRAIN_SAVED,MAX_DEP_STRAIN_SAVED
   integer ::nele_strain_saved,npoints_strain_saved
   logical, dimension(:), allocatable ::save_strain
   integer, dimension(:), allocatable ::iele_strain_saved_ispec
   real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::strain_saved
   integer, dimension(:), allocatable ::ispec_iele_strain_saved

!  USED IN SOLVER PART
   logical ::LOW_RESOLUTION
   integer ::NSTEP_BETWEEN_OUTPUTBOUND
   integer ::DECIMATE_COUPLING
   integer ::NPOINTS_PER_PACK

! USED IN MESH, GENERATE_DATABASE and SOLVER
   integer ::nxLow,nxHigh,nyLow,nyHigh,nrdown,nrtop
   integer,dimension(:),allocatable::element_xLow,element_xLowReg,element_xHigh,&
           element_xHighReg,element_yLow,element_yLowReg,element_yHigh, &
           element_yHighReg,element_rdown,element_rdownReg,element_rtop,element_rtopReg


end module tele_coupling_par
