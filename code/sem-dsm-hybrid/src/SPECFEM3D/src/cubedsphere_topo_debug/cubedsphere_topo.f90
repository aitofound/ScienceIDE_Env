!######## This code uses linear interpolation to convert topography 
!from the longitude-latitude system to the cubed-sphere xi-eta coordinate
!system.

!******************************** Input Arguments
!********************************
! (1) The first input argument arg(1) specifies which individual interface we are
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
!     for the ocean layer. Otherwise, this layer would be too thin at shallow ocean
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

!~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


  subroutine cubedsphere_topo
  implicit none
  include "constants.h"
!==========================================================



! auxiliary variables to generate the mesh
  integer ix,iy
  double precision x_current,y_current
  double precision x,y,z
  integer ilat,ilon
  double precision lat,lon

! parameters read from parameter file
  integer NEX_XI,NEX_ETA,NPROC_XI,NPROC_ETA,UTM_PROJECTION_ZONE


  double precision UTM_X_MIN,UTM_X_MAX,UTM_Y_MIN,UTM_Y_MAX
  double precision Z_DEPTH_BLOCK 
  double precision LATITUDE_MIN,LATITUDE_MAX,LONGITUDE_MIN,LONGITUDE_MAX
  double precision R_TOP_BOUND,ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES, &
                   CENTER_LATITUDE_IN_DEGREES,CENTER_LONGITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH
  ! rotation matrix from Euler angles
  double precision, dimension(NDIM,NDIM) :: rotation_matrix

  logical SUPPRESS_UTM_PROJECTION,USE_REGULAR_MESH
  logical CUBED_SPHERE_PROJECTION

! Mesh files for visualization
  logical CREATE_ABAQUS_FILES,CREATE_DX_FILES

! doublings parameters
  integer NDOUBLINGS
  integer, dimension(2) :: ner_doublings

  character(len=256) OUTPUT_FILES,LOCAL_PATH 

! parameters deduced from parameters read from file
  integer NPROC,NEX_PER_PROC_XI,NEX_PER_PROC_ETA

! this for all the regions
  integer NSPEC_AB,NGLOB_AB,NSPEC2D_A_XI,NSPEC2D_B_XI, &
               NSPEC2D_A_ETA,NSPEC2D_B_ETA, &
               NSPEC2DMAX_XMIN_XMAX,NSPEC2DMAX_YMIN_YMAX, &
               NSPEC2D_BOTTOM,NSPEC2D_TOP, &
               NPOIN2DMAX_XMIN_XMAX,NPOIN2DMAX_YMIN_YMAX

  double precision min_elevation,max_elevation


  !parameter for coupling
  integer ::NXI_TOPOGRAPHY_TAPER,NETA_TOPOGRAPHY_TAPER,NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY
  integer ::COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,COUPLING_IR_TOP,COUPLING_IR_BOTTOM
  integer ::TeleEle_ixLow,TeleEle_ixHigh,TeleEle_iyLow,TeleEle_iyHigh,TeleEle_irTop,TeleEle_irBot
  integer ::TeleEle_nxLow,TeleEle_nxHigh,TeleEle_nyLow,TeleEle_nyHigh,TeleEle_nrTop,TeleEle_nrBot
  
  double precision::belowx_notopo,belowx_taper,abovex_notopo,abovex_taper
  double precision::belowy_notopo,belowy_taper,abovey_notopo,abovey_taper
  double precision::xtaper_width,ytaper_width


! interfaces parameters
  double precision,dimension(:,:),pointer:: xy_interface
  double precision,dimension(:,:),pointer:: topo_latlon_interface
  double precision,dimension(:),pointer:: orig_x_interface,orig_y_interface
  double precision :: orig_lat_interface,orig_lon_interface
  double precision,dimension(:),pointer:: spacing_x_interface,spacing_y_interface
  double precision:: spacing_lat_interface,spacing_lon_interface
  character(len=70) INTERFACES_FILE,CAVITY_FILE
  character(len=70), dimension(:), pointer::interface_file
  character(len=70)::topo_latlon_interface_file
  integer i_interface ! ipoint_current
  integer number_of_interfaces
  integer ::latlon_number_of_interfaces
  integer, dimension(:),pointer :: nx_interface,ny_interface
  integer nlat_interface,nlon_interface
  integer ::ith_interface_workon
  double precision:: topography_basement
  double precision:: max_topography_truncation,min_topography_truncation

! subregions parameters
  integer NSUBREGIONS
  integer, dimension(:,:), pointer :: subregions

! material properties
  integer NMATERIALS
  double precision , dimension(:,:), pointer :: material_properties


!arguments 
  integer ::iarg
  character(len=MAX_STRING_LEN) :: arg(9)



! ************** PROGRAM STARTS HERE **************
 do iarg = 1, command_argument_count()
    call get_command_argument(iarg,arg(iarg))
 enddo

 read(arg(1),*) ith_interface_workon
 topo_latlon_interface_file=arg(2)
 read(arg(3),*) topography_basement
 read(arg(4),*) min_topography_truncation
 read(arg(5),*) max_topography_truncation


 if(topography_basement>max_topography_truncation) then
   print *,"Error, check your inputs, min_topography_truncation>max_topography_truncation!!"
   call exit()
 end if

 if(topography_basement>max_topography_truncation) then
   print *, 'The topography is truncated below',max_topography_truncation,&
        ' meters, but the topography_basement is even higher than the that.'
   print *,"Error, check your input, topography_basement>max_topography_truncation!!"
   call exit()
 end if

 if(topography_basement<min_topography_truncation) then
   print *, 'The topography is truncated above', min_topography_truncation,&
         ' meters, but the topography_basement is even lower than the that.'
   print *,"Error, check your input, topography_basement>max_topography_truncation!!"
   call exit()
 end if


! get the base pathname for output files
!  call get_value_string(OUTPUT_FILES_BASE, 'OUTPUT_FILES', OUTPUT_FILES_BASE(1:len_trim(OUTPUT_FILES_BASE)))

 open(unit=IMAIN,file=OUTPUT_FILES_BASE(1:len_trim(OUTPUT_FILES_BASE))//'/output_topo_convert.txt',status='unknown')

 write(IMAIN,*)
 write(IMAIN,*) '******************************************'
 write(IMAIN,*) '*** Specfem3D MPI Topography_convert - f90 version ***'
 write(IMAIN,*) '******************************************'
 write(IMAIN,*)



  call read_parameter_file(LATITUDE_MIN,LATITUDE_MAX,LONGITUDE_MIN,LONGITUDE_MAX, &
        UTM_X_MIN,UTM_X_MAX,UTM_Y_MIN,UTM_Y_MAX,Z_DEPTH_BLOCK,CUBED_SPHERE_PROJECTION,&
        NEX_XI,NEX_ETA,NPROC_XI,NPROC_ETA,UTM_PROJECTION_ZONE,LOCAL_PATH,SUPPRESS_UTM_PROJECTION,&
        INTERFACES_FILE,CAVITY_FILE,NSUBREGIONS,&
        USE_REGULAR_MESH,NDOUBLINGS,ner_doublings)



  if(.not.CUBED_SPHERE_PROJECTION) stop 'This program is only for the CUBED SPHERE PROJECTION=.TRUE.'


  write(IMAIN,*)
  write(IMAIN,*) 'Reading the taper information from file ', &
       MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES)) &
       //'Coupling_Par_file'

  call read_parameter_coupling(IMAIN,R_TOP_BOUND,NXI_TOPOGRAPHY_TAPER,NETA_TOPOGRAPHY_TAPER,&
         NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY,COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,&
         COUPLING_IR_TOP,COUPLING_IR_BOTTOM,NEX_XI,NEX_ETA,10000000,NDOUBLINGS,USE_REGULAR_MESH,&
         ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES,&
         CENTER_LATITUDE_IN_DEGREES,CENTER_LONGITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH,& 
         rotation_matrix,ner_doublings,0)
!**********************notopography and taper range*************************
         xtaper_width=NXI_TOPOGRAPHY_TAPER*ANGULAR_WIDTH_XI_IN_DEGREES/NEX_XI
         belowx_notopo=NXI_NO_TOPOGRAPHY*ANGULAR_WIDTH_XI_IN_DEGREES/NEX_XI
         belowx_taper= belowx_notopo+xtaper_width
         abovex_notopo=ANGULAR_WIDTH_XI_IN_DEGREES*(1.0-dble(NXI_NO_TOPOGRAPHY)/NEX_XI)
         abovex_taper=abovex_notopo-xtaper_width

         ytaper_width=NETA_TOPOGRAPHY_TAPER*ANGULAR_WIDTH_ETA_IN_DEGREES/NEX_ETA
         belowy_notopo=NETA_NO_TOPOGRAPHY*ANGULAR_WIDTH_ETA_IN_DEGREES/NEX_ETA
         belowy_taper=belowy_notopo+ytaper_width
         abovey_notopo=ANGULAR_WIDTH_ETA_IN_DEGREES*(1.0-dble(NETA_NO_TOPOGRAPHY)/NEX_ETA)
         abovey_taper=abovey_notopo-ytaper_width
  write(IMAIN,*)
  write(IMAIN,*) 'Taper information'
  write(IMAIN,*) 'Below x=',belowx_notopo,'and above ',abovex_notopo,' topography is zero.'
  write(IMAIN,*) 'For x:',belowx_notopo,'-',belowx_taper,'deg and',abovex_taper,'-',abovex_notopo,&
                 'deg is topography tapering range.'
  write(IMAIN,*) 'Below y=',belowy_notopo,'and above ',abovey_notopo,' topography is zero.'
  write(IMAIN,*) 'For y',belowy_notopo,'-',belowy_taper,'deg and',abovey_taper,'-',abovey_notopo,&
                 'deg is topography tapering range.'


!*****************************************************************************


! get interface data from external file to count the spectral elements along Z
  write(IMAIN,*) 'Reading xy_interface data from file ',&
        MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES))//INTERFACES_FILE(1:len_trim(INTERFACES_FILE)), &
        ' to count the spectral elements'

  open(unit=IIN,file=MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES))//INTERFACES_FILE,status='old')


! read number of interfaces
  call read_value_integer(IIN,DONT_IGNORE_JUNK,number_of_interfaces,'NINTERFACES')
  if(number_of_interfaces < 1) stop 'not enough interfaces (minimum is 1, for topography)'

  allocate(interface_file(number_of_interfaces))
  allocate(nx_interface(number_of_interfaces))
  allocate(ny_interface(number_of_interfaces))
  allocate(orig_x_interface(number_of_interfaces))
  allocate(orig_y_interface(number_of_interfaces))
  allocate(spacing_x_interface(number_of_interfaces))
  allocate(spacing_y_interface(number_of_interfaces))


! loop on all the interfaces
  do i_interface = 1,number_of_interfaces

     call read_interface_parameters(IIN,SUPPRESS_UTM_PROJECTION,interface_file(i_interface), &
          nx_interface(i_interface),ny_interface(i_interface),orig_x_interface(i_interface),orig_y_interface(i_interface),&
          spacing_x_interface(i_interface),spacing_y_interface(i_interface))
     write(IMAIN,*) 'the ',i_interface,'interface: nx_interface=',nx_interface(i_interface),'ny_interface=',ny_interface(i_interface)

     if((nx_interface(i_interface) < 2) .or.(ny_interface(i_interface) < 2)) stop 'not enough interface points (minimum is 2x2)'
  enddo


  close(IIN)



  min_elevation = +HUGEVAL
  max_elevation = -HUGEVAL


!*********************************************************************************
!*******************read the input topography(longitude,latitude) model************
    ! loop on all the points describing this interface
    open(unit=45,file=topo_latlon_interface_file,status='old')
    read(45,*) nlon_interface,nlat_interface

    allocate(topo_latlon_interface(nlat_interface,nlon_interface))
    read(45,*) orig_lon_interface,orig_lat_interface
    read(45,*) spacing_lon_interface,spacing_lat_interface

!The most inner loop for lat and next for lon
    do ilon=1,nlon_interface
        do ilat=1,nlat_interface
!            call read_value_double_precision(45,DONT_IGNORE_JUNK,topo_latlon_interface(ilat,ilon),'Z_INTERFACE_TOP')
!           read(45,*) topo_latlon_interface(ilat,ilon)
        enddo
    enddo

!The most inner loop for lat and next for lon
    do ilat=1,nlat_interface
        do ilon=1,nlon_interface
!            call
!            read_value_double_precision(45,DONT_IGNORE_JUNK,topo_latlon_interface(ilat,ilon),'Z_INTERFACE_TOP')
!           read(45,*) topo_latlon_interface(ilat,ilon)
        enddo
    enddo

! goes from LatMin to LatMax
    do ilat=1,nlat_interface
       read(45,*) topo_latlon_interface(ilat,:)
    enddo

! goes from LatMax to LatMin
!    do ilat=nlat_interface,1,-1
!       read(45,*) topo_latlon_interface(ilat,:)
!    enddo


    close(45)


!******************************************************************************
!**************Convert topography(lon,lat) into topography(xi,eta) ************
  do i_interface=ith_interface_workon,ith_interface_workon  !1,number_of_interfaces
    allocate(xy_interface(nx_interface(i_interface),ny_interface(i_interface)))
    !print *,'nx ny',nx_interface(i_interface),ny_interface(i_interface)
    do ix=1,nx_interface(i_interface)
      do iy=1,ny_interface(i_interface)
        x_current=dble(ix-1)*spacing_x_interface(i_interface)+orig_x_interface(i_interface)
        y_current=dble(iy-1)*spacing_y_interface(i_interface)+orig_y_interface(i_interface)

        call cubedspheretoxyz(x_current,y_current,x,y,z,rotation_matrix,R_TOP_BOUND,&
                              ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES)
        call xyztolatlon(x,y,z,R_TOP_BOUND,lat,lon)
        call search_topo(lat,lon,xy_interface(ix,iy),topo_latlon_interface,nlat_interface,nlon_interface,&
                         orig_lat_interface,orig_lon_interface,spacing_lat_interface,spacing_lon_interface)
!************************add taper*******************************
        if(x_current.le.belowx_notopo.or.y_current.le.belowy_notopo.or.&
           x_current.ge.abovex_notopo.or.y_current.ge.abovey_notopo) then
            xy_interface(ix,iy)=topography_basement
        else
           if(x_current.le.belowx_taper) &
             xy_interface(ix,iy)=topography_basement+ &
                (xy_interface(ix,iy)-topography_basement)*(x_current-belowx_notopo)/xtaper_width
           if(x_current.ge.abovex_taper) &
             xy_interface(ix,iy)=topography_basement+ &
                (xy_interface(ix,iy)-topography_basement)*(abovex_notopo-x_current)/xtaper_width
           if(y_current.le.belowy_taper) &
             xy_interface(ix,iy)=topography_basement+ &
                (xy_interface(ix,iy)-topography_basement)*(y_current-belowy_notopo)/ytaper_width
           if(y_current.ge.abovey_taper) &
             xy_interface(ix,iy)=topography_basement+ &
                (xy_interface(ix,iy)-topography_basement)*(abovey_notopo-y_current)/ytaper_width
        end if

!*******************************Truncating topography higher than max_topography_truncation******
        if(xy_interface(ix,iy)>max_topography_truncation) xy_interface(ix,iy)=max_topography_truncation
        if(xy_interface(ix,iy)<min_topography_truncation) xy_interface(ix,iy)=min_topography_truncation


      end do !loop for iy
    end do  !loop for ix

    call save_topo(nx_interface(i_interface),ny_interface(i_interface),xy_interface,interface_file(i_interface))
    deallocate(xy_interface)
    deallocate(topo_latlon_interface)
  end do !loop for i_interface


  end subroutine cubedsphere_topo

