!=====================================================================
!

!module constants

!  include "constants.h"

!end module constants

!=====================================================================
module coupling_mesh_par
use constants

   double precision ::R_TOP_BOUND,ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES, &
                   CENTER_LATITUDE_IN_DEGREES,CENTER_LONGITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH
   integer::NXI_TOPOGRAPHY_TAPER,NETA_TOPOGRAPHY_TAPER,NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY
   integer::COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,COUPLING_IR_TOP,COUPLING_IR_BOTTOM
   double precision ::COUPLING_DEPTH_TOLERENCE,COUPLING_DIST_TOLERENCE
   integer, parameter ::Imain_coupling=1011


   integer ::coupling_nspec_xlow,coupling_nspec_xhigh,coupling_nspec_ylow,&
             coupling_nspec_yhigh,coupling_nspec_rtop,coupling_nspec_rbottom
   integer,dimension(:),allocatable  ::coupling_ispec_xlow,coupling_isubregion_xlow
   integer,dimension(:),allocatable  ::coupling_ispec_xhigh,coupling_isubregion_xhigh
   integer,dimension(:),allocatable  ::coupling_ispec_ylow,coupling_isubregion_ylow
   integer,dimension(:),allocatable  ::coupling_ispec_yhigh,coupling_isubregion_yhigh
   integer,dimension(:),allocatable  ::coupling_ispec_rtop,coupling_isubregion_rtop
   integer,dimension(:),allocatable  ::coupling_ispec_rbottom,coupling_isubregion_rbottom

   double precision, dimension(NDIM,NDIM) :: rotation_matrix

   double precision, dimension(:,:,:,:), allocatable :: xstore_cubedsph,ystore_cubedsph,zstore_cubedsph
   double precision, dimension(:,:), allocatable :: nodes_coords_cubedsph,nodes_coords_cubedsph_old
end module coupling_mesh_par
