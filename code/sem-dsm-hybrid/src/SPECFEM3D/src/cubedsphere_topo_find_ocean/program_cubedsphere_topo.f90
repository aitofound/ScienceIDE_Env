!=================================================================
!                       Cubed_sphere topography
!           --------------------------------
!
!                   author:Wenbo Wu
!                          WHOI
!                          Jan 2025
!
!   This program is to convert the topography in the lat-lon coordinate 
!   system into that in the cubed-sphere coordinate system.

!=================================================================
!########The code converts the topography/bathymetry in the longitude-latitude
!system to that in the cubed-spehre xi-eta system. Then the file
!"your_example_folder/DATA/meshfem3D_files/real_bathymetry_topography" is used
!in xmeshfem3D to classify the elements as elastic type or acoustic type.

!The first input argument is the name of topography model, as a
!function of longitude and latitude. This file has the below format

   ! nlongitude nlatitude
   ! origin_longitude origin_latitude
   ! space_longitude space_latitude
   ! topography(lat0,lon0) topography(lat0,lon0+dlon)
   ! topography(lat0,lon0+2*dlon)
   ! topography(lat0,lon0+3*dlon) ...
   ! topography(lat0+dlat,lon0) topography(lat0+dlat,lon0+dlon) ...
   ! ...
   ! topography(lat0+(nlongitude-1)*dlat,lon0) ...
   ! topography(lat0+(nlongitude-1)*dlat,lon0+(nlon-1)*dlon) 

!The remaining input arguments are
!     arge(2) - nxi
!     arge(3) - neta
!     arge(4) - origin_xi
!     arge(5) - origin_eta
!     arge(6) - space_xi
!     arge(6) - space_eta

!USAGE: ../../bin/xcubedsphere_topo_forRedineOcean
!./DATA/meshfem3D_files/SA_real_topo.dat 540 540 -0.2        -0.2d0      0.01
!0.01

!The output topography/bathymetry is saved in the file 
!your_example_folder/DATA/meshfem3D_files/real_bathymetry_topography.

  program  xcubedsphere_topo

  call cubedsphere_topo()

  end program xcubedsphere_topo
