!=====================================================================
!

!module constants

!  include "constants.h"

!end module constants

!=====================================================================



module struct_defined
use constants
implicit none
!******************************model for raytrace***************************
type model_1d
!Number of layers
!integer ::Nlay
!double precision,dimension(MAXLAY):: Vp,Vs,Rou,Depth
!double precision,dimension(MAXLAY):: Radius
!urn rayparameter of lth layer, Only P velocity or S velocity involved
!double precision,dimension(MAXLAY):: B,p
!integer :: Ncmb,Nicb;
end type model_1d


!***********impose traction on boundaries and predicted 1D synthetics******
type input_traction_velo
end type
!
!**************************The information needed for the integral**********
Type Variables_bound

integer                 ::ispec_coupling
integer                 ::iregion  !regular(1,3,5) and unregular(2,4)
integer                 ::face_type  !5 or 6 kinds of faces(left,right,top,bottom,back,forward)
integer                 ::iele_elasitc
integer                 ::iele_acoustic
integer                 ::istart,iend,jstart,jend,kstart,kend
logical                 ::is_elastic

!real(kind=CUSTOM_REAL), dimension(:,:,:,:), allocatable ::disp,traction

!double precision,dimension(NGLLX,NGLLY)   ::x,y,z
!double precision,dimension(NGLLX,NGLLY,NGLLZ)   ::sigmaxx,sigmaxy,sigmaxz,sigmayx,sigmayy,&
!                                                  sigmayz,sigmazx,sigmazy,sigmazz 
end type Variables_bound

!***************************************************************************
end module struct_defined

!***********impose traction on boundaries and predicted 1D synthetics******
module impose_1D_BCS
use constants

!single variables are difend as type of double precision.
!array is type of CUSTOM_REAL.

character(len=80),dimension(6) ::moment_name
character(len=80),dimension(3) ::force_name
data moment_name /'Mzz','Mrr','Mtt','Mzr','Mzt','Mrt'/
data force_name /'Fr','Ft','Fz'/

!ipack and it_imposed corresponding to it
integer ::ipack_impose,ipack_old,it_impose

double precision::t0_this_pack

!parameters for input Green functions
double precision ::inject_dt
integer  ::inject_ndep_elas,inject_ndep_acous
integer  ::max_inject_ndist_elas,max_inject_ndist_acous
!double precision ::in_mindep_ela,in_mindep_acou
!double precision ::in_ddep_ela,in_ddep_acou
!double precision ::in_mindist_ela,in_mindist_acou
!double precision ::in_ddist_ela,in_ddist_acou
integer  ::inject_npt
integer  ::inject_npack
integer  ::inject_npt_eachpack
double precision ::inject_time_start,inject_time_end

integer, dimension(:),allocatable ::ndist_idepth_elas,ndist_idepth_acous

!information for each point on each face
! id to find the corresponding traction or velocity in table for each point
integer, dimension(:,:), allocatable ::id_epsilon,id_pressure,id_velo,id_poten_dot
!integer, dimension(:,:,:), allocatable ::radiation_factor
double precision, dimension(:,:,:,:), allocatable ::zrtToxyz
!number of source components. 6 for moment tensor and 3 for single force 
integer ::ncomp_source
!number of source components selected. Not all the source components are needed.
!For example, a fixed moment-tensor injection only needs the selected tensor component
!set; vertical component simulation in a single-force mode only needs the results of
!vertical force 'Fz'; East or North component simulation only needs 'Fr' and 'Ft'.
integer ::ncomp_source_selected
!The source components selected. The maximum number is 6 in a moment tensor
!mode, so we define it with a dimension of 6.
character(len=80),dimension(6) :: sources_selected
real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::moment_zrt
real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::force_zrt


!strain/pressure and velocity/potential_dot to be read
integer ::nepsilon,nvelo
integer ::npressure,npoten_dot,ndisp_fluid
integer,dimension(:,:), allocatable ::impose_depPress,impose_depEpsilon
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
   integer ::nele_tele_couplingElas
   integer ::nele_tele_couplingAcous
   integer ::npoints_bound
   integer ::npoints_BoundElas
   integer ::npoints_BoundAcous
   Type(Variables_bound),dimension(:),allocatable ::coupling_ele_property
   real(kind=CUSTOM_REAL), dimension(:,:,:), allocatable ::disp_bound,traction_bound
!   real(kind=CUSTOM_REAL), dimension(:,:), allocatable ::pressure_bound
   real(kind=CUSTOM_REAL), dimension(:,:), allocatable ::normal_vect_coupling
   integer ::npackage_Elas
   integer, dimension(:), allocatable ::npoints_ipack_Elas
   
! for the rank=0 process
   integer ::Recv_npackage_Elas
   integer, dimension(:),allocatable ::Recv_npoints_ipack_Elas

!   Type(ViaRepresent),pointer             ::RepInfo 

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
