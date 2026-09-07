!=================================================================
!                       Cubedsphere_topo
!           --------------------------------
!
!                   author:Wenbo Wu
!                          WHOI
!                       Jan 2025
!
!~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
!######## This code uses linear interpolation to convert topography 
!from the longitude-latitude system to the cubed-sphere xi-eta coordinate
!system.

!******************************** Input Arguments
!********************************
! (1) The first input argument arg(1) specifies which individual interface we
! are
! working on. All interfaces are listed in the file 
! your_example_folder/DATA/meshfem3D_files/interfaces.dat. 

! (2) The second input argument arg(2) is the name of the topography model as a 
! function of longitude and latitude coordinates.

! (3) The third input argument arg(3) is the reference (baseline) topography of
! this interface.

! (4) The fourth input argument arg(4) specifies the minimum allowed topography.

! (5) The fifth input argument arg(5) specifies the maximum allowed topography.
!     Specifying a minimum and/or maximum allowed topography can be useful in
!     certain cases. 
!     For example, there may be an ocean layer in the simulation, where the 
!     topography/bathymetry is divided into two interfaces. 
!     - The top interface represents real topography on land and 0 meters
!     (minimum allowed topography) in the ocean. 
!     - The second interface represents the ocean bottom and is an artificial
!     interface with a maximum allowed topography beneath the land. 
!     By choosing appropriately truncated topography/bathymetry, we create space
!     for the ocean layer. Otherwise, this layer would be too thin at shallow
!     ocean
!     depths, or the two interfaces might intersect.
!********************************************************************************



!********************************** Example ************************************
! If the file your_example_folder/DATA/meshfem3D_files/interfaces.dat is as
! follows:

! # Number of interfaces
! 3
! .true.                   110 110 -0.2        -0.2      0.05d0    0.05d0
! topo_Moho.dat
! .true.                   110 110 -0.2        -0.2      0.05d0    0.05d0
! topo_Midcrust.dat
! .true.                   320 320 -0.1        -0.1      0.01d0    0.01d0
! topo_free_surf.dat
! 7
! 2
! 2

!~~~~~~~~~~~~~~~~~~~~~ Free Surface Topography ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
! Enter your working folder and run the following command:
! ../../bin/xtopo_lonlat_to_cubedsphere 3
! ./DATA/meshfem3D_files/ETOPO1_topography 0.0 -12000.0 10000.0

! The above command converts the topography file "ETOPO1_topography" from the 
! longitude-latitude coordinate system into the cubed-sphere (xi-eta) coordinate
! system.
! The converted topography is saved in the file:
! your_example_folder/DATA/meshfem3D_files/topo_free_surf.dat
! 
! This file corresponds to the third interface specified in:
! your_example_folder/DATA/meshfem3D_files/interfaces.dat
! (See the example above).
! 
! The reference topography of the free surface is 0.0. Toward the boundaries of
! the SEM box, the topography is tapered to 0.0 to ensure smooth transitions.

!~~~~~~~~~~~~~~~~~ Format of ETOPO1_topography File ~~~~~~~~~~~~~~~~~~~~~~~~~~~
! The file "ETOPO1_topography" should be formatted as follows:
! ---------------------------------------------------------------------------
! nlongitude nlatitude
! origin_longitude origin_latitude
! space_longitude space_latitude
! topography(lat0, lon0)       topography(lat0, lon0 + dlon)
! topography(lat0, lon0 + 2*dlon)
! topography(lat0, lon0 + 3*dlon) ...
! topography(lat0 + dlat, lon0) topography(lat0 + dlat, lon0 + dlon) ...
! ...
! topography(lat0 + (nlatitude-1)*dlat, lon0) ... topography(lat0 +
! (nlatitude-1)*dlat, lon0 + (nlongitude-1)*dlon)


  program  xcubedsphere_topo

  call cubedsphere_topo()

  end program xcubedsphere_topo
