!=====================================================================
!
!               S p e c f e m 3 D  V e r s i o n  2 . 0
!               ---------------------------------------
!
!          Main authors: Dimitri Komatitsch and Jeroen Tromp
!    Princeton University, USA and University of Pau / CNRS / INRIA
! (c) Princeton University / California Institute of Technology and University of Pau / CNRS / INRIA
!                            November 2010
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation; either version 2 of the License, or
! (at your option) any later version.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License along
! with this program; if not, write to the Free Software Foundation, Inc.,
! 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
!
!=====================================================================

! generic tomography file
!
! note: the idea is to use an external, tomography velocity model
!
! most of the routines here are place-holders, please add/implement your own routines
!

  module tomography
  use constants
  !include "constants.h"

  ! for external tomography:
  ! file must be in ../in_data/files/ directory
  ! (regular spaced, xyz-block file in ascii)
  !character (len=80) :: TOMO_FILENAME = 'veryfast_tomography_abruzzo_complete.xyz'
  character (len=80) :: TOMO_FILENAME = 'tomography_model.xyz'
  character (len=80) :: DSM_FILENAME = 'dsm_model_input'

  !DSM input model
  integer ::n_structure_zone
  integer, dimension(:),allocatable::fluid_thiszone
  real(kind=CUSTOM_REAL),dimension(:),allocatable ::rmin_structure_zone,rmax_structure_zone,&
                        qmu_structure_zone,qkappa_structure_zone
  real(kind=CUSTOM_REAL),dimension(:,:),allocatable ::rho_structure_zone,&
               vpv_structure_zone,vph_structure_zone,vsv_structure_zone,&
               vsh_structure_zone,eta_structure_zone




  ! model dimensions
  double precision :: ORIG_X,ORIG_Y,ORIG_Z
  double precision :: END_X,END_Y,END_Z
  double precision :: SPACING_X,SPACING_Y,SPACING_Z

  ! model parameter records
  real(kind=CUSTOM_REAL), dimension (:), allocatable :: vp_tomography,vs_tomography,&
                        rho_tomography,x_tomography,y_tomography,z_tomography

  ! model entries
  integer :: NX,NY,NZ
  integer :: nrecord

  ! min/max statistics
  double precision :: VP_MIN,VS_MIN,RHO_MIN,VP_MAX,VS_MAX,RHO_MAX

  end module tomography


  module tomography_2D
  use constants
  character (len=256) :: TOMO_2D_FILENAME = 'tomography_model_2D.dat'
      ! model dimensions
  double precision :: ORIG_DEPTH_2D,ORIG_Y_2D
  double precision :: END_DEPTH_2D,END_Y_2D
  double precision :: SPACING_DEPTH_2D,SPACING_Y_2D

  ! model parameter records
  real(kind=CUSTOM_REAL), dimension (:), allocatable :: vp_tomography_2D,vs_tomography_2D,rho_tomography_2D
  ! model entries
  integer :: NDEPTH_2D,NY_2D
  integer :: nrecord_2D
  end module tomography_2D


  module sediment
  use constants
  !topography
    double precision ::ORIG_LON,ORIG_LAT
    double precision ::SPACING_LON, SPACING_LAT
    integer :: NLON, NLAT
    real(kind=CUSTOM_REAL), dimension (:,:), allocatable :: topography

  !trench
    integer ::NTRENCH
    double precision,dimension(:),allocatable :: lat_trench,lon_trench
  end module sediment

  module slabdepth_model
  use constants
  double precision :: lat0_slabmodel,lon0_slabmodel,dlat_slabmodel,dlon_slabmodel
  double precision::thickness_onelayer
  integer ::nlayers_slab,nlat_slabmodel,nlon_slabmodel
  double precision,dimension(:,:,:), allocatable ::slabdepth
  double precision,dimension(:,:,:), allocatable ::slabtaper
  end module slabdepth_model

  !used for setting up oceanic lithosphere
  module  bathymetry_model
  integer ::nx_bathymetry,ny_bathymetry
  double precision :: orig_x_bathymetry,orig_y_bathymetry
  double precision :: spacing_x_bathymetry,spacing_y_bathymetry
  double precision,dimension(:,:), allocatable :: bathymetry
  end module bathymetry_model

!
!-------------------------------------------------------------------------------------------------
!

  subroutine model_tomography_broadcast(myrank)

  implicit none

  ! include "constants.h"
  ! include "precision.h"
  ! include 'mpif.h'
  integer :: myrank

  ! all processes read in same file
  ! note: for a high number of processes this might lead to a bottleneck
  call read_model_tomography(myrank)
  !call read_model_tomography_vpvsrho(myrank)

  ! otherwise:

  ! only master reads in model file
  !if(myrank == 0) call read_external_model()
  ! broadcast the information read on the master to the nodes, e.g.
  !call MPI_BCAST(nrecord,1,MPI_INTEGER,0,MPI_COMM_WORLD,ier)
  !if( myrank /= 0 ) allocate( vp_tomography(1:nrecord) )
  !call MPI_BCAST(vp_tomography,size(vp_tomography),CUSTOM_MPI_TYPE,0,MPI_COMM_WORLD,ier)

  end subroutine model_tomography_broadcast

!
!-------------------------------------------------------------------------------------------------
!

subroutine read_model_tomography(myrank)

  use tomography
  use generate_databases_par, only: TOMOGRAPHY_PATH
  use generate_databases_par, only: xstore, ystore, zstore

  implicit none

  integer :: myrank

  ! model parameters read from file (full model)
  real(kind=CUSTOM_REAL) :: ORIG_X_f, ORIG_Y_f, ORIG_Z_f
  real(kind=CUSTOM_REAL) :: END_X_f,  END_Y_f,  END_Z_f
  !real(kind=CUSTOM_REAL) :: SPACING_X, SPACING_Y, SPACING_Z
  integer :: NX_f, NY_f, NZ_f

  ! cropped/chunk parameters (what we will use). These parameters already defined in bathymetry_model.
  !real(kind=CUSTOM_REAL) :: ORIG_X, ORIG_Y, ORIG_Z
  !real(kind=CUSTOM_REAL) :: END_X, END_Y, END_Z
  !integer :: NX, NY, NZ

  !real(kind=CUSTOM_REAL) :: VP_MIN, VP_MAX, VS_MIN, VS_MAX, RHO_MIN, RHO_MAX

  real(kind=CUSTOM_REAL), dimension(:), allocatable :: array_read_tmp

  integer :: ier
  integer :: iz, iy, ix
  integer :: iz_min, iz_max, iy_min, iy_max, ix_min, ix_max
  integer :: irecord_chunk
  real(kind=CUSTOM_REAL) :: z_tmp
  character(len=256) :: filename

  real(kind=CUSTOM_REAL) :: xmin, xmax, ymin, ymax, zmin, zmax
  real(kind=CUSTOM_REAL), parameter :: EXTEND = 3000.0  ! extend range in meters

  ! mesh real bounds
  xmin=minval(xstore); xmax=maxval(xstore)
  ymin=minval(ystore); ymax=maxval(ystore)
  zmin=minval(zstore); zmax=maxval(zstore)

  ! open tomography file
  if (TOMOGRAPHY_PATH(len_trim(TOMOGRAPHY_PATH):len_trim(TOMOGRAPHY_PATH)) == "/") then
      filename = TOMOGRAPHY_PATH(1:len_trim(TOMOGRAPHY_PATH)) // trim(TOMO_FILENAME)
  else
      filename = TOMOGRAPHY_PATH(1:len_trim(TOMOGRAPHY_PATH)) // '/' // trim(TOMO_FILENAME)
  endif

  open(unit=27,file=trim(filename),status='old',action='read',iostat=ier)
  if (ier /= 0) call exit_MPI(myrank,'error reading tomography file')

  ! ---------- READ full (original) model header ----------
  read(27,*) ORIG_X_f, ORIG_Y_f, ORIG_Z_f, END_X_f, END_Y_f, END_Z_f
  read(27,*) SPACING_X, SPACING_Y, SPACING_Z
  read(27,*) NX_f, NY_f, NZ_f
  read(27,*) VP_MIN, VP_MAX, VS_MIN, VS_MAX, RHO_MIN, RHO_MAX

  ! ---------- Compute crop indices ----------
  ix_min = max(1, int((xmin - EXTEND - ORIG_X_f)/SPACING_X) + 1)
  ix_max = min(NX_f, int((xmax + EXTEND - ORIG_X_f)/SPACING_X) + 1)

  iy_min = max(1, int((ymin - EXTEND - ORIG_Y_f)/SPACING_Y) + 1)
  iy_max = min(NY_f, int((ymax + EXTEND - ORIG_Y_f)/SPACING_Y) + 1)

  iz_min = max(1, int((zmin - EXTEND - ORIG_Z_f)/SPACING_Z) + 1)
  iz_max = min(NZ_f, int((zmax + EXTEND - ORIG_Z_f)/SPACING_Z) + 1)
  if(xmin - EXTEND - ORIG_X_f<0.0 .or. xmax + EXTEND - END_X_f>0.0) then
          call exit_MPI(myrank,'tomography model does not fully  cover x-range')
  end if
  if(ymin - EXTEND - ORIG_Y_f<0.0 .or. ymax + EXTEND - END_Y_f>0.0) then
          call exit_MPI(myrank,'tomography model does not fully  cover y-range')
  end if
  if(zmin - EXTEND - ORIG_Z_f<0.0 .or. zmax + EXTEND - END_Z_f>0.0) then
          call exit_MPI(myrank,'tomography model does not fully  cover z-range')
  end if

  ! ---------- Define NEW cropped model domain ----------
  NX = ix_max - ix_min + 1
  NY = iy_max - iy_min + 1
  NZ = iz_max - iz_min + 1

  ORIG_X = ORIG_X_f + (ix_min-1)*SPACING_X
  ORIG_Y = ORIG_Y_f + (iy_min-1)*SPACING_Y
  ORIG_Z = ORIG_Z_f + (iz_min-1)*SPACING_Z

  END_X  = ORIG_X + (NX-1)*SPACING_X
  END_Y  = ORIG_Y + (NY-1)*SPACING_Y
  END_Z  = ORIG_Z + (NZ-1)*SPACING_Z

  ! allocate temporary read buffer for a full row
  allocate(array_read_tmp(NX_f))

  ! allocate final tomography arrays for the CROPPED model
  nrecord = NX * NY * NZ
  allocate(vp_tomography(nrecord), &
           vs_tomography(nrecord), &
           rho_tomography(nrecord), &
           z_tomography(nrecord), stat=ier)
  if (ier /= 0) call exit_MPI(myrank,'Not enough memory for cropped model')

  ! ---------- Read original file but store only the chunk ----------
  do iz = 1, NZ_f
     z_tmp = ORIG_Z_f + (iz-1)*SPACING_Z

     do iy = 1, NY_f
        read(27,*) array_read_tmp(:)

        ! skip rows outside Y or Z bounds
        if (iz < iz_min .or. iz > iz_max) cycle
        if (iy < iy_min .or. iy > iy_max) cycle

        ! compute z-index inside chunk
        do ix = ix_min, ix_max
            irecord_chunk = &
                ( (iz-iz_min)*NY*NX ) + &
                ( (iy-iy_min)*NX ) + &
                (ix-ix_min + 1)

            vp_tomography(irecord_chunk) = array_read_tmp(ix)
            z_tomography(irecord_chunk)  = z_tmp
        end do

     end do
  end do

  close(27)
  deallocate(array_read_tmp)

  if (myrank == 0) then
     write(IMAIN,*) '     Cropped tomography model loaded: ',trim(TOMO_FILENAME)
     write(IMAIN,*) '     Crop NX,NY,NZ = ',NX,NY,NZ
  endif

end subroutine read_model_tomography

!----------------------------read 3-D tomography model----------------------
  subroutine read_model_tomography_vpvsrho(myrank)
  use tomography
  use generate_databases_par, only: TOMOGRAPHY_PATH
  implicit none

  integer :: myrank

  ! local parameters
  real(kind=CUSTOM_REAL) :: x_tomo,y_tomo,z_tomo,vp_tomo,vs_tomo,rho_tomo
  integer :: irecord
  integer ::ier
  character(len=256):: filename

  if (TOMOGRAPHY_PATH(len_trim(TOMOGRAPHY_PATH):len_trim(TOMOGRAPHY_PATH)) == "/") then
      filename = TOMOGRAPHY_PATH(1:len_trim(TOMOGRAPHY_PATH)) // trim(TOMO_FILENAME)
  else
      filename = TOMOGRAPHY_PATH(1:len_trim(TOMOGRAPHY_PATH)) // '/' // trim(TOMO_FILENAME)
  endif
  open(unit=27,file=trim(filename),status='old',action='read',iostat=ier)
  if( ier /= 0 ) call exit_MPI(myrank,'error reading tomography file')

  ! reads in model dimensions
  read(27,*) ORIG_X, ORIG_Y, ORIG_Z, END_X, END_Y, END_Z
  read(27,*) SPACING_X, SPACING_Y, SPACING_Z
  read(27,*) NX, NY, NZ
  read(27,*) VP_MIN, VP_MAX, VS_MIN, VS_MAX, RHO_MIN, RHO_MAX

  nrecord = NX*NY*NZ

  ! allocates model records
  allocate(vp_tomography(1:nrecord), &
          vs_tomography(1:nrecord), &
          rho_tomography(1:nrecord), &
          x_tomography(1:nrecord),&
          y_tomography(1:nrecord),&
          z_tomography(1:nrecord),&
          stat=ier)
  if(ier /= 0) call exit_MPI(myrank,'not enough memory to allocate arrays')

  ! reads in record sections
  do irecord = 1,nrecord
    read(27,*) x_tomo,y_tomo,z_tomo,vp_tomo,vs_tomo,rho_tomo

    ! stores record values
    x_tomography(irecord) = x_tomo
    y_tomography(irecord) = y_tomo
    z_tomography(irecord) = z_tomo
    vp_tomography(irecord) = vp_tomo*1000
    vs_tomography(irecord) = vs_tomo*1000
    rho_tomography(irecord) = rho_tomo*1000
  enddo
  close(27)

  ! user output
  if( myrank == 0 ) then
    write(IMAIN,*) '     tomography model: ',trim(TOMO_FILENAME)
  endif
  end subroutine read_model_tomography_vpvsrho


  subroutine model_tomography_broadcast_2D(myrank)

  implicit none
  integer :: myrank
  ! all processes read in same file
  ! note: for a high number of processes this might lead to a bottleneck
  call read_model_tomography_2D(myrank)
  end subroutine model_tomography_broadcast_2D

  subroutine read_model_tomography_2D(myrank)
  use tomography_2D

  implicit none

  integer :: myrank

  ! local parameters
  real(kind=CUSTOM_REAL) :: depth_tmp,y_tmp,vp_tmp,vs_tmp,rho_tmp
  integer :: irecord,ier
  character(len=256):: filename
  integer ::idepth,iy,irecord_saved
  filename = MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES))//trim(TOMO_2D_FILENAME)
  open(unit=27,file=trim(filename),status='old',action='read',iostat=ier)
  if( ier /= 0 ) call exit_MPI(myrank,'error reading 2D tomography file')

  ! reads in model dimensions
  read(27,*) ORIG_DEPTH_2D, ORIG_Y_2D, END_DEPTH_2D, END_Y_2D
  read(27,*) SPACING_DEPTH_2D, SPACING_Y_2D
  read(27,*) NDEPTH_2D, NY_2D

  print *,'read NX',NDEPTH_2D,NY_2D,SPACING_DEPTH_2D, SPACING_Y_2D

  nrecord_2D = NDEPTH_2D*NY_2D
  ! allocates model records
  allocate(vp_tomography_2D(1:nrecord_2D), &
          vs_tomography_2D(1:nrecord_2D), &
          rho_tomography_2D(1:nrecord_2D),stat=ier)
  if(ier /= 0) call exit_MPI(myrank,'not enough memory to allocate arrays')

  ! reads in record sections
  do irecord = 1,nrecord_2D
    read(27,*) y_tmp,depth_tmp,vp_tmp,vs_tmp,rho_tmp

    ! stores record values
    iy=int((y_tmp-ORIG_Y_2D)/SPACING_Y_2D)+1
    idepth=int((depth_tmp-ORIG_DEPTH_2D)/SPACING_DEPTH_2D)+1
    irecord_saved=(idepth-1)*NY_2D+iy
    vp_tomography_2D(irecord_saved) = vp_tmp
    vs_tomography_2D(irecord_saved) = vs_tmp
    rho_tomography_2D(irecord_saved) = rho_tmp
  enddo


  close(27)
  ! user output
  if( myrank == 0 ) then
    write(IMAIN,*) '     2D tomography model: ',trim(TOMO_2D_FILENAME)
  endif

  end subroutine read_model_tomography_2D



!***********************INCORPORATE SEDIMENT*************************************************
  !*******************read topography(lat,lon) for adding sediment **************
  subroutine read_topography(myrank)
  use sediment

  implicit none

  integer :: myrank
  integer ::ilat
  character(len=256):: filename
  integer ::ier

  filename = MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES))//'latlon_surf_topo.dat'
  open(unit=27,file=trim(filename),status='old',action='read',iostat=ier)
  if( ier /= 0 ) call exit_MPI(myrank,'error reading tomography file')

  ! read topography/bathymetry file
  read(27,*) NLON, NLAT
  read(27,*) ORIG_LON,ORIG_LAT
  read(27,*) SPACING_LON, SPACING_LAT
  allocate(topography(NLAT,NLON))
  do ilat=1,NLAT
     !topography for each latitude, topography(ilat,1:NLON) 
     read(27,*)topography(ilat,:)
  end do
  close(27)
  end subroutine read_topography


  !*******************read trench trace for adding sediment at prism **************
  subroutine read_trench(myrank)
  use sediment

  implicit none

  integer :: myrank
  character(len=256):: filename
  integer :: itrench,ier

  filename = MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES))//'trench.dat'
  open(unit=27,file=trim(filename),status='old',action='read',iostat=ier)
  if( ier /= 0 ) call exit_MPI(myrank,'error reading tomography file')

  read(27,*)NTRENCH
  allocate(lat_trench(NTRENCH))
  allocate(lon_trench(NTRENCH))
  do itrench=1,NTRENCH
      read(27,*)lon_trench(itrench),lat_trench(itrench)
  end do
  close(27)

  end subroutine read_trench

! **********specifiy sediment thickness at (x_eval,y_eval,z_eval), *******************************
! ***********and use emperical formulas of sediment to compute vp,vs and rho**********************
  SUBROUTINE add_sediment_Japan(myrank, x_eval, y_eval, z_eval, vp_final, vs_final, rho_final)
  USE sediment
  USE constants, ONLY:PI,TINYVAL
  IMPLICIT NONE

  ! Arguments
  DOUBLE PRECISION, INTENT(IN) :: x_eval, y_eval, z_eval
  INTEGER, INTENT(IN) :: myrank
  REAL(KIND=CUSTOM_REAL), INTENT(OUT) :: vp_final, vs_final, rho_final

  ! Local Variables
  REAL(KIND=CUSTOM_REAL) :: radius, thetasurf, phisurf
  INTEGER :: ilat, ilon
  REAL(KIND=CUSTOM_REAL) :: topo, dist_min, dist, azim, bazim
  REAL(KIND=CUSTOM_REAL) :: SedimentThickness, DistanceOceanBottom
  REAL(KIND=CUSTOM_REAL) :: frac, DistanceOceanBottomSquare, DistanceOceanBottomCube
  integer ::itrench,itrench_find
  REAL(KIND=CUSTOM_REAL), PARAMETER :: MAX_RADIUS_DIFF = -5000.0

  integer :: int_tmp

  int_tmp=myrank

  ! Step 1: Compute Spherical Coordinates
  radius = DSQRT(x_eval**2 + y_eval**2 + z_eval**2)
  thetasurf = DACOS(z_eval / radius)
  phisurf = DACOS(DBLE(x_eval) / (DBLE(radius) * DSIN(thetasurf))*(1-TINYVAL))
  IF (y_eval < TINYVAL) phisurf = -phisurf

  ! Convert to Degrees
  thetasurf = thetasurf * 180.0 / PI
  phisurf = phisurf * 180.0 / PI

  ! Step 2: Determine Latitude and Longitude
  ilat = INT((90.0 - thetasurf - ORIG_LAT) / SPACING_LAT)
  ilon = INT((phisurf - ORIG_LON) / SPACING_LON)
  topo = topography(ilat, ilon)

  ! Step 3: Check radius Difference
  IF (radius - R_EARTH_SURF - topo > MAX_RADIUS_DIFF) THEN
    dist_min = 180.0

    ! Find Closest Trench
    DO itrench = 1, NTRENCH
      CALL distazbaz(90.0 - thetasurf, phisurf, lat_trench(itrench), lon_trench(itrench), dist, azim, bazim)
      IF (dist < dist_min) THEN
        dist_min = dist
        itrench_find = itrench
      END IF
    END DO

    ! Step 4: Compute Sediment Thickness
    IF (phisurf >= lon_trench(itrench_find)) THEN
      !East of trench
      SedimentThickness = 200.0
    ELSE
      !West of trench

      !Prism
      IF (dist_min <= 0.25) THEN
        SedimentThickness = 5000.0 * dist_min / 0.25 
      !Transition to sediment on shelf
      ELSE IF (dist_min > 0.25 .AND. dist_min <= 0.35 .AND. topo < 0.0) THEN
        frac = (0.35 - dist_min) / 0.1
        SedimentThickness = 5000.0 * frac + 1500.0 * (1 - frac)
      !Sediment on shelf
      ELSE IF (dist_min > 0.35 .AND. topo < 0.0) THEN
        SedimentThickness = 1500.0
      ELSE
        SedimentThickness = 0.0
      END IF
    END IF

    ! Step 5: Determine Material Properties
    !Sediment only present at 28<lat<46 and lon>145 deg 
    IF (radius - R_EARTH_SURF - topo > -SedimentThickness .AND. SedimentThickness > TINYVAL .AND. &
        thetasurf >= 28.0 .AND. thetasurf <= 46.0 .AND. phisurf >= 135.0) THEN

      DistanceOceanBottom = -(radius - R_EARTH_SURF - topo) / 1000.0
      IF (DistanceOceanBottom < 0.0) DistanceOceanBottom = 0.0

      ! Cut-off at Depth of 1.5 km below seafloor
      IF (DistanceOceanBottom > 1.5) DistanceOceanBottom = 1.5

      DistanceOceanBottomSquare = DistanceOceanBottom**2
      DistanceOceanBottomCube = DistanceOceanBottomSquare * DistanceOceanBottom

      !Seismic structure of marine  sediment, see Edwin L. Hmilton (JASA, 1979; JSP, 1976)
      !and Thomas M. Brocher (BSSA, 2010)
      ! Top Sediment Layey
      IF (DistanceOceanBottom * 1000.0 < 1000.0) THEN
        IF (DistanceOceanBottom < 1.0) THEN
          vp_final = 1.68 + DistanceOceanBottom * 1.304 - 0.741 * DistanceOceanBottomSquare + &
                     0.257 * DistanceOceanBottomCube
          IF (vp_final > 3.5) vp_final = 3.5
          vs_final = 0.78 * vp_final - 0.962
          rho_final = 1.53 + 1.395 * DistanceOceanBottom - 0.617 * DistanceOceanBottomSquare
        ELSE
          vp_final = 2.331 + 0.593 * (DistanceOceanBottom - 1.0)
          IF (vp_final > 3.5) vp_final = 3.5
          vs_final = 0.78 * vp_final - 0.962
          rho_final = 2.308 + 0.161 * (DistanceOceanBottom - 1.0)
        END IF

        ! Convert to Final Unit of M/S
        vs_final = vs_final * 1000.0
        vp_final = vp_final * 1000.0
        rho_final = rho_final * 1000.0

        ! Ensure Minimum Vs Value
        IF (vs_final < 500.0) vs_final = 500.0
      END IF
    END IF
  END IF

 END SUBROUTINE add_sediment_Japan
!***********************INCORPORATE SEDIMENT*************************************************

subroutine taper_to_dsm_model(taper,depth,top_taper_depth_start,top_taper_depth_end, &
                bot_taper_depth_start,bot_taper_depth_end,x_cubedsph,y_cubedsph,&
                taper_width_edges,dsm_width_edges)

use tele_coupling_par, only:ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES
use constants, only:PI
implicit none
double precision,intent(out) ::taper
double precision,intent(in) ::depth,x_cubedsph,y_cubedsph

!parameters to define tapering at edges
!'dsm_width_edges' is the most outside width, within which the dsm model is
!used (taper=0.0); Then the taper linearly increases from 0.0 to 1.0 in a width of
!'taper_width_edges'; Move further inside, the taper becomes 1.0 (local or tomography model is used).
double precision,intent(in) ::taper_width_edges,dsm_width_edges

!parameters to define tapering along the depth
!taper is 1.0 (local or tomography model used) above the depth of 'taper_depth_start' and 
! 0.0 (transits to dsm model) below 'taper_depth_end'; Between these two depths, 
!it a linearly decreased taper.
double precision,intent(in) ::top_taper_depth_start,top_taper_depth_end
double precision,intent(in) ::bot_taper_depth_start,bot_taper_depth_end

!local parameters
double precision ::xi0_taperleft_deg,xi1_taperleft_deg,xi0_taperright_deg,xi1_taperright_deg
double precision ::eta0_taperleft_deg,eta1_taperleft_deg,eta0_taperright_deg,eta1_taperright_deg
double precision ::x_cubedsph_shift,y_cubedsph_shift

double precision ::taper_edgex,taper_edgey,taper_depth
!Convert the unit of rad into degree. 
!One degree roughly equals 112km at the free surface and becomes smaller in deeper earth
double precision, parameter::degtorad=PI/180.0


!*******************add taper on the boundaries of simulation box
!*******taper along the depth
if(depth.lt.top_taper_depth_start.or.depth.gt.bot_taper_depth_end) then
   taper_depth=0.0
else if(depth.ge.top_taper_depth_start .and. depth.le.top_taper_depth_end) then
   taper_depth=(depth - top_taper_depth_start)/(top_taper_depth_end - top_taper_depth_start)
else if(depth.ge.bot_taper_depth_start .and. depth.le.bot_taper_depth_end) then
   taper_depth=1.0-(depth - bot_taper_depth_start)/(bot_taper_depth_end - bot_taper_depth_start)
else
   taper_depth=1.0
end if

!taper at the four edges
xi0_taperleft_deg=dsm_width_edges*degtorad
xi1_taperleft_deg=(dsm_width_edges+taper_width_edges)*degtorad
xi0_taperright_deg=(ANGULAR_WIDTH_XI_IN_DEGREES-dsm_width_edges-taper_width_edges)*degtorad
xi1_taperright_deg=(ANGULAR_WIDTH_XI_IN_DEGREES-dsm_width_edges)*degtorad

eta0_taperleft_deg=dsm_width_edges*degtorad
eta1_taperleft_deg=(dsm_width_edges+taper_width_edges)*degtorad
eta0_taperright_deg=(ANGULAR_WIDTH_ETA_IN_DEGREES-dsm_width_edges-taper_width_edges)*degtorad
eta1_taperright_deg=(ANGULAR_WIDTH_ETA_IN_DEGREES-dsm_width_edges)*degtorad

x_cubedsph_shift=x_cubedsph+ANGULAR_WIDTH_XI_IN_DEGREES/2.0*degtorad
taper_edgex=1.0
!taper on the xi-left edge
if(x_cubedsph_shift.lt.xi0_taperleft_deg) then
   taper_edgex=0.0
else if(x_cubedsph_shift.gt.xi0_taperleft_deg.and.x_cubedsph_shift.lt.xi1_taperleft_deg) then
   taper_edgex=(x_cubedsph_shift-xi0_taperleft_deg)/(xi1_taperleft_deg-xi0_taperleft_deg)
end if


!taper on the xi-right edge
if(x_cubedsph_shift.gt.xi1_taperright_deg) then
   taper_edgex=0.0
else if(x_cubedsph_shift.gt.xi0_taperright_deg.and.x_cubedsph_shift.lt.xi1_taperright_deg) then
   taper_edgex=1.0-(x_cubedsph_shift-xi0_taperright_deg)/(xi1_taperright_deg-xi0_taperright_deg)
end if


y_cubedsph_shift=y_cubedsph+ANGULAR_WIDTH_ETA_IN_DEGREES/2.0*degtorad
taper_edgey=1.0
!taper on the eta-left edge
if(y_cubedsph_shift.lt.eta0_taperleft_deg) then
   taper_edgey=0.0
else if(y_cubedsph_shift.gt.eta0_taperleft_deg.and.y_cubedsph_shift.lt.eta1_taperleft_deg) then
   taper_edgey=(y_cubedsph_shift-eta0_taperleft_deg)/(eta1_taperleft_deg-eta0_taperleft_deg)
end if


!taper on the eta-right edge
if(y_cubedsph_shift.gt.eta1_taperright_deg) then
   taper_edgey=0.0
else if(y_cubedsph_shift.gt.eta0_taperright_deg.and.y_cubedsph_shift.lt.eta1_taperright_deg) then
   taper_edgey=1.0-(y_cubedsph_shift-eta0_taperright_deg)/(eta1_taperright_deg-eta0_taperright_deg)
end if

!the final tapering 
taper=taper_edgex*taper_edgey*taper_depth

end subroutine taper_to_dsm_model


!
!-------------------------------------------------------------------------------------------------


  subroutine model_slabdepth_broadcast(myrank)

  implicit none

  ! include "constants.h"
  ! include "precision.h"
  ! include 'mpif.h'
  integer :: myrank

  ! all processes read in same file
  ! note: for a high number of processes this might lead to a bottleneck
  call read_model_slabdepth(myrank)

  ! otherwise:

  ! only master reads in model file
  !if(myrank == 0) call read_model_slabdepth()
  ! broadcast the information read on the master to the nodes, e.g.
  !call MPI_BCAST(nrecord,1,MPI_INTEGER,0,MPI_COMM_WORLD,ier)
  !if( myrank /= 0 ) allocate( slabdepth(1:nrecord) )
  !call
  !MPI_BCAST(slabdepth,size(slabdepth),CUSTOM_MPI_TYPE,0,MPI_COMM_WORLD,ier)
  !repeat it for slabtaper

  end subroutine model_slabdepth_broadcast



  subroutine read_model_slabdepth(myrank)

  use slabdepth_model

  implicit none

  integer :: myrank

  ! local parameters
!  real(kind=CUSTOM_REAL) :: x_tomo,y_tomo,z_tomo,vp_tomo,vs_tomo,rho_tomo
  integer :: ier
  integer ::ilat,ilon,ilayer
  character(len=256):: filename

  !TOMO_FILENAME='DATA/veryfast_tomography_abruzzo_complete.xyz'
  ! probably the simple position for the filename is the constat.h
  ! but it is also possible to include the name of the file in the material file
  ! (therefore in the undef_mat_prop)
  ! if we want more than one tomofile (Examples: 2 file with a differente
  ! resolution
  ! as in los angeles case we need to loop over mat_ext_mesh(1,ispec)...
  ! it is a possible solution )
  !  magnoni 1/12/09
  filename = IN_DATA_FILES(1:len_trim(IN_DATA_FILES))//trim('slabdepth_model')
  open(unit=26,file=trim(filename),status='old',action='read',iostat=ier)
  if( ier /= 0 ) call exit_MPI(myrank,'error reading slab depth file')

  ! reads in model dimensions
  read(26,*) nlayers_slab
  read(26,*) thickness_onelayer
  read(26,*) nlon_slabmodel,nlat_slabmodel
  read(26,*) lon0_slabmodel,lat0_slabmodel
  read(26,*) dlon_slabmodel,dlat_slabmodel

  ! allocates model records
  allocate(slabdepth(nlon_slabmodel,nlat_slabmodel,nlayers_slab), &
           slabtaper(nlon_slabmodel,nlat_slabmodel,nlayers_slab),stat=ier)
  if(ier /= 0) call exit_MPI(myrank,'not enough memory to allocate arrays')

  do ilayer=1,nlayers_slab
   do ilat=1,nlat_slabmodel
    do ilon=1,nlon_slabmodel
      read(26,*)slabdepth(ilon,ilat,ilayer),slabtaper(ilon,ilat,ilayer)
    end do
   end do
  end do


  close(26)

  ! user output
  if( myrank == 0 ) then
    write(IMAIN,*) '     slab depth model: slabdepth_model'
  endif

end subroutine read_model_slabdepth


subroutine  add_slab(x,y,z,x_cubedsph,y_cubedsph,z_cubedsph,vp,vs,rho,myrank)
use slabdepth_model
use tele_coupling_par, only: ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES

implicit none
double precision::x,y,z,x_cubedsph,y_cubedsph,z_cubedsph
double precision::x_cubedsph_shift,y_cubedsph_shift
real(kind=CUSTOM_REAL) ::vp,vs,rho
integer ::myrank

double precision::theta,phi,lat,lon,radi,depth
integer ::ilat_find,ilon_find
!integer ::ilat,ilon
double precision::ilat_realtype,ilon_realtype,ilat_fraction_remained,ilon_fraction_remained
double precision::perturb
double precision::taper_thispoint,dep_slabcoord_thispoint

double precision::depth_slabtop,depth_slabbot
integer ::ilayer_renewed
double precision::depth_renewedlayer,depth_oldlayer
double precision::dep_slabcoord_renewed,dep_slabcoord_old
double precision::taper_renewed,taper_old
double precision::ratio_between_depths


!Used in two layers model
double precision, parameter::slab_thickness_toplayer=80000.0
double precision, parameter::slab_thickness_transition=40000.0
!Used in triangular velocity profile model
double precision, parameter::slab_thickness_triangle=120000.0

double precision, parameter::max_perturb=0.05


!The following parameters are specified, because I just "know" what my model looks
!like. Change these parameters as necessary.
double precision, parameter::dep0_taperbot_km=680000.0
double precision, parameter::dep1_taperbot_km=750000.0

!Convert the unit of rad into degree. 
!One degree roughly equals 112km at free surface and becomes smaller in deeper earth
double precision, parameter::degtorad=PI/180.0
double precision, parameter::taper_four_edges=0.3
double precision, parameter::nohete_four_edges=0.3

double precision ::xi0_taperleft_deg,xi1_taperleft_deg,xi0_taperright_deg,xi1_taperright_deg
double precision ::eta0_taperleft_deg,eta1_taperleft_deg,eta0_taperright_deg,eta1_taperright_deg
double precision ::vp_orig
 double precision ::real_tmp

!The below line is used to avoid possible error reports during compling
!the code. z_cubedsph is not used now, that occationally causes error 
!reports. But it might be usful in the future, so we keep it here.
real_tmp=z_cubedsph

radi=dsqrt(x**2+y**2+z**2)
theta=dacos(z/radi)
if(y<0.0) then
     phi=2*PI-dacos(x*(1.d0-1.e-10)/(radi*dsin(theta)))
else
     phi=dacos(x*(1.d0-1.e-10)/(radi*dsin(theta)))
end if
lat= 180/PI*(PI/2-theta)
lon=phi/PI*180.0
depth=R_EARTH_SURF-radi

ilat_realtype=(lat-lat0_slabmodel)/dlat_slabmodel+1
ilat_find=int(ilat_realtype)
ilat_fraction_remained=ilat_realtype-ilat_find
ilon_realtype=(lon-lon0_slabmodel)/dlon_slabmodel+1
ilon_find=int(ilon_realtype)
ilon_fraction_remained=ilon_realtype-ilon_find

if(ilat_find<1.or.ilat_find>nlat_slabmodel-1) then
   call exit_MPI(myrank,'error, target latitude is out of slab model range')
end if
if(ilon_find<1.or.ilon_find>nlon_slabmodel-1) then
   call exit_MPI(myrank,'error, target longitude is out of slab model range')
end if

! If any of the four corners is undefined (i.e., no slab present), set depth_slabtop and depth_slabbot 
! to a large negative value (-1.e9). This prevents an isolated, incorrect slab from appearing to hang 
! in the mantle due to erroneous interpolation, i.e.,
! A_ratio_close_to_one * 450 km + (-1.e5 km) * (1 - A_ratio_close_to_one).

if(slabdepth(ilon_find,ilat_find,1).lt.-1.e6.or.slabdepth(ilon_find+1,ilat_find,1).lt.-1.e6.or.&
   slabdepth(ilon_find,ilat_find+1,1).lt.-1.e6.or.slabdepth(ilon_find+1,ilat_find+1,1).lt.-1.e6) then
    depth_slabtop=-1.e9
    depth_slabbot=-1.e9
!piecewise linear interpolation to find the depth of slab top interface at this GLL point
else
    depth_slabtop=(slabdepth(ilon_find,ilat_find,1)*(1.0-ilon_fraction_remained)+&
                 slabdepth(ilon_find+1,ilat_find,1)*ilon_fraction_remained)*(1.0-ilat_fraction_remained)+ &
                (slabdepth(ilon_find,ilat_find+1,1)*(1.0-ilon_fraction_remained)+&
                 slabdepth(ilon_find+1,ilat_find+1,1)*ilon_fraction_remained)*ilat_fraction_remained
    depth_slabbot=(slabdepth(ilon_find,ilat_find,nlayers_slab)*(1.0-ilon_fraction_remained)+&
                 slabdepth(ilon_find+1,ilat_find,nlayers_slab)*ilon_fraction_remained)*(1.0-ilat_fraction_remained)+ &
                (slabdepth(ilon_find,ilat_find+1,nlayers_slab)*(1.0-ilon_fraction_remained)+&
                 slabdepth(ilon_find+1,ilat_find+1,nlayers_slab)*ilon_fraction_remained)*ilat_fraction_remained
end if

perturb=0.0
!above slab or slab doesn't exist at all.
if(depth<depth_slabtop.or.depth>depth_slabbot.or.depth_slabtop<-1.e4) then
  perturb=0.0
else 
  ilayer_renewed=1
  depth_renewedlayer=depth_slabtop

  do while(ilayer_renewed.lt.nlayers_slab.and.depth.gt.depth_renewedlayer)
      depth_oldlayer=depth_renewedlayer
      ilayer_renewed=ilayer_renewed+1
      depth_renewedlayer=(slabdepth(ilon_find,ilat_find,ilayer_renewed)*(1.0-ilon_fraction_remained)+&
               slabdepth(ilon_find+1,ilat_find,ilayer_renewed)*ilon_fraction_remained)*(1.0-ilat_fraction_remained)+&
              (slabdepth(ilon_find,ilat_find+1,ilayer_renewed)*(1.0-ilon_fraction_remained)+&
               slabdepth(ilon_find+1,ilat_find+1,ilayer_renewed)*ilon_fraction_remained)*ilat_fraction_remained
  end do
  ratio_between_depths=(depth-depth_oldlayer)/(depth_renewedlayer-depth_oldlayer)
  taper_renewed=(slabtaper(ilon_find,ilat_find,ilayer_renewed)*(1.0-ilon_fraction_remained)+&
               slabtaper(ilon_find+1,ilat_find,ilayer_renewed)*ilon_fraction_remained)*(1.0-ilat_fraction_remained)+&
              (slabtaper(ilon_find,ilat_find+1,ilayer_renewed)*(1.0-ilon_fraction_remained)+&
               slabtaper(ilon_find+1,ilat_find+1,ilayer_renewed)*ilon_fraction_remained)*ilat_fraction_remained
  dep_slabcoord_renewed=thickness_onelayer*(ilayer_renewed-1)

  taper_old=(slabtaper(ilon_find,ilat_find,ilayer_renewed-1)*(1.0-ilon_fraction_remained)+&
               slabtaper(ilon_find+1,ilat_find,ilayer_renewed-1)*ilon_fraction_remained)*(1.0-ilat_fraction_remained)+&
              (slabtaper(ilon_find,ilat_find+1,ilayer_renewed-1)*(1.0-ilon_fraction_remained)+&
               slabtaper(ilon_find+1,ilat_find+1,ilayer_renewed-1)*ilon_fraction_remained)*ilat_fraction_remained
  dep_slabcoord_old=thickness_onelayer*(ilayer_renewed-2)

!piecewise linear interpolation
  taper_thispoint=taper_renewed*ratio_between_depths+taper_old*(1.0-ratio_between_depths)
  dep_slabcoord_thispoint=dep_slabcoord_renewed*ratio_between_depths+dep_slabcoord_old*(1.0-ratio_between_depths)
 
!gradual velocity increase from slab top-bottom to the centre
  if(dep_slabcoord_thispoint<slab_thickness_triangle/2.0+1.e-10) then
     perturb=max_perturb*taper_thispoint*(dep_slabcoord_thispoint/(slab_thickness_triangle/2.0) )
     if(taper_thispoint<0.99.and.depth<420000.0) print *,"error_perturb",dep_slabcoord_thispoint,&
              taper_old,taper_renewed,ilat_realtype,ilon_realtype,lat,lon
     if(taper_thispoint>1.01.and.depth<420000.0) print *,"error_perturb1",dep_slabcoord_thispoint,&
              taper_old,taper_renewed,ilat_realtype,ilon_realtype,lat,lon
  else if(dep_slabcoord_thispoint>slab_thickness_triangle/2.0.and.&
          dep_slabcoord_thispoint<slab_thickness_triangle) then
     perturb=max_perturb*(1.0-(dep_slabcoord_thispoint-slab_thickness_triangle/2.0)/(slab_thickness_triangle/2.0))*&
             taper_thispoint
     if(taper_thispoint<0.99.and.depth<420000.0) print *,"error_perturb",dep_slabcoord_thispoint,&
              taper_old,taper_renewed,ilat_realtype,ilon_realtype,lat,lon
     if(taper_thispoint>1.01.and.depth<420000.0) print *,"error_perturb1",dep_slabcoord_thispoint,&
              taper_old,taper_renewed,ilat_realtype,ilon_realtype,lat,lon
  else
     perturb=0.0
  end if

!oceanic crust Low Velocity Zone
  if(dep_slabcoord_thispoint<7000.0.and.depth<150000.0) then
     perturb=-0.1*taper_thispoint
  end if

!**************ONE TOP layer + underlying TRANSITION layer
!    if(dep_slabcoord_thispoint<slab_thickness_toplayer) then
!       perturb=max_perturb*taper_thispoint
!       if(taper_thispoint<0.99.and.depth<420000.0) print *,"error_perturb",dep_slabcoord_thispoint,&
!                taper_old,taper_renewed,ilat_realtype,ilon_realtype,lat,lon
!       if(taper_thispoint>1.01.and.depth<420000.0) print *,"error_perturb1",dep_slabcoord_thispoint,&
!                taper_old,taper_renewed,ilat_realtype,ilon_realtype,lat,lon
!    else if(dep_slabcoord_thispoint<slab_thickness_toplayer+slab_thickness_transition) then
!       perturb=max_perturb*(1.0-(dep_slabcoord_thispoint-slab_thickness_toplayer)/slab_thickness_transition)*&
!               taper_thispoint
!       if(taper_thispoint<0.99.and.depth<420000.0) print *,"error_perturb",dep_slabcoord_thispoint,&
!                taper_old,taper_renewed,ilat_realtype,ilon_realtype,lat,lon
!       if(taper_thispoint>1.01.and.depth<420000.0) print *,"error_perturb1",dep_slabcoord_thispoint,&
!                taper_old,taper_renewed,ilat_realtype,ilon_realtype,lat,lon
!    end if
    
end if

!add taper on the boundaries of the SEM box
xi0_taperleft_deg=nohete_four_edges*degtorad
xi1_taperleft_deg=(nohete_four_edges+taper_four_edges)*degtorad
xi0_taperright_deg=(ANGULAR_WIDTH_XI_IN_DEGREES-nohete_four_edges-taper_four_edges)*degtorad
xi1_taperright_deg=(ANGULAR_WIDTH_XI_IN_DEGREES-nohete_four_edges)*degtorad

eta0_taperleft_deg=nohete_four_edges*degtorad
eta1_taperleft_deg=(nohete_four_edges+taper_four_edges)*degtorad
eta0_taperright_deg=(ANGULAR_WIDTH_ETA_IN_DEGREES-nohete_four_edges-taper_four_edges)*degtorad
eta1_taperright_deg=(ANGULAR_WIDTH_ETA_IN_DEGREES-nohete_four_edges)*degtorad

!bottom taper
if(depth.gt.dep1_taperbot_km) then
   perturb=0.0
else if(depth.gt.dep0_taperbot_km.and.depth.lt.dep1_taperbot_km) then
   perturb=perturb*(1.0-(depth-dep0_taperbot_km)/(dep1_taperbot_km-dep0_taperbot_km))
end if

x_cubedsph_shift=x_cubedsph+ANGULAR_WIDTH_XI_IN_DEGREES/2.0*degtorad
!xi left taper
if(x_cubedsph_shift.lt.xi0_taperleft_deg) then
   perturb=0.0
else if(x_cubedsph_shift.gt.xi0_taperleft_deg.and.x_cubedsph_shift.lt.xi1_taperleft_deg) then
   perturb=perturb*(1.0+(x_cubedsph_shift-xi1_taperleft_deg)/(xi1_taperleft_deg-xi0_taperleft_deg))
end if


!xi right taper
if(x_cubedsph_shift.gt.xi1_taperright_deg) then
   perturb=0.0
else if(x_cubedsph_shift.gt.xi0_taperright_deg.and.x_cubedsph_shift.lt.xi1_taperright_deg) then
   perturb=perturb*(1.0-(x_cubedsph_shift-xi0_taperright_deg)/(xi1_taperright_deg-xi0_taperright_deg))
end if


y_cubedsph_shift=y_cubedsph+ANGULAR_WIDTH_ETA_IN_DEGREES/2.0*degtorad
!eta left taper
if(y_cubedsph_shift.lt.eta0_taperleft_deg) then
   perturb=0.0
else if(y_cubedsph_shift.gt.eta0_taperleft_deg.and.y_cubedsph_shift.lt.eta1_taperleft_deg) then
   perturb=perturb*(1.0+(y_cubedsph_shift-eta1_taperleft_deg)/(eta1_taperleft_deg-eta0_taperleft_deg))
end if


!eta right taper
if(y_cubedsph_shift.gt.eta1_taperright_deg) then
   perturb=0.0
else if(y_cubedsph_shift.gt.eta0_taperright_deg.and.y_cubedsph_shift.lt.eta1_taperright_deg) then
   perturb=perturb*(1.0-(y_cubedsph_shift-eta0_taperright_deg)/(eta1_taperright_deg-eta0_taperright_deg))
end if

!For 1D benchmark
!perturb=0.0

!For oceanic LVZ and high Vp,Vs,rho in slab
!if(perturb<0.05) print *,'LVZ_oceanic',xi0_taperleft_deg,x_cubedsph_shift,x_cubedsph

vp=vp*(1.0+perturb)
!We know the oceanic crust never has Vp<6200. This imposed condition makes short
!period simulation faster.
if(vp<6400.0) then
   vp_orig=vp/(1.0+perturb)
   perturb=6400.0/vp_orig-1.0
   vp=vp_orig*(1.0+perturb)
end if

vs=vs*(1.0+1.0*perturb)
if(perturb>1.e-3) then
  rho=rho*(1.0+perturb)
else if (perturb<-1.e-3) then
  rho=rho*(1.0-0.04)
else
  rho=rho
end if


end subroutine add_slab

!***********************add oceanic lithosphere*****************************************
!***************************************************************************************
subroutine bathymetry_broadcast(myrank)

  implicit none

  integer :: myrank
  ! all processes read in same file
  ! note: for a high number of processes this might lead to a bottleneck
  call read_bathymetry(myrank)

  ! otherwise:

  ! only master reads in model file
  !if(myrank == 0) call read_bathymetry()
  ! broadcast the information read on the master to the nodes, e.g.
  !call MPI_BCAST(nrecord,1,MPI_INTEGER,0,MPI_COMM_WORLD,ier)
  !if( myrank /= 0 ) allocate(bathymetry(1:nrecord) )
  !call
  !MPI_BCAST(bathymetry,size(bathymetry),CUSTOM_MPI_TYPE,0,MPI_COMM_WORLD,ier)
  !repeat it for taper
end subroutine bathymetry_broadcast

subroutine read_bathymetry(myrank)
  use constants, only:MF_IN_DATA_FILES,IMAIN
  use bathymetry_model

  implicit none

  integer :: myrank

  integer :: ier
  integer ::ix,iy
  character(len=256):: filename

  filename = MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES))//trim('real_bathymetry_topography')
  open(unit=26,file=trim(filename),status='old',action='read',iostat=ier)
  if( ier /= 0 ) call exit_MPI(myrank,'error reading bathymetry file')

  ! reads model dimensions, origin locations and spacings
  read(26,*) nx_bathymetry,ny_bathymetry
  read(26,*) orig_x_bathymetry,orig_y_bathymetry
  read(26,*) spacing_x_bathymetry,spacing_y_bathymetry

  ! allocates model records
  allocate(bathymetry(nx_bathymetry,ny_bathymetry),stat=ier)
  if(ier /= 0) call exit_MPI(myrank,'not enough memory to allocate arrays')

!Note that the order of iy and ix in the below loops must be consistent with that in the 
!file real_bathymetry_topography.
  do iy=1,ny_bathymetry
   do ix=1,nx_bathymetry
      read(26,*) bathymetry(ix,iy)
   end do
  end do

  close(26)

  ! user output
  if( myrank == 0 ) then
    write(IMAIN,*) 'bathymetry model: real_bathymetry_topography'
  endif

end subroutine read_bathymetry

subroutine oceanic_lithosphere_structure(depth,vp_ols,vs_ols,rho_ols)
use constants, only:CUSTOM_REAL
implicit none
double precision, intent(in)  ::depth
real(kind=CUSTOM_REAL),intent(out) ::vp_ols,vs_ols,rho_ols

!upper 700 m oceanic crust
if(depth.le.700.0) then
vp_ols=5000.0
vs_ols=2700.0
rho_ols=2550.0
!middle oceanic crust at 700 m - 2100 m 
else if(depth.le.2100.0) then
vp_ols=6500.0
!vp_ols=5000.0
vs_ols=3700.0
rho_ols=2850.0
!lower oceanic crust at 2100 m - 7000 m 
!else if(depth.le.7000.0) then
else if(depth.le.7000.0) then
vp_ols=7100.0
!vp_ols=5000.0
vs_ols=4050.0
rho_ols=3050.0
!below lithosphere
else
!vp_ols=4040.0
vp_ols=8040.0
vs_ols=4470.0
rho_ols=3310.0
end if
end subroutine oceanic_lithosphere_structure

subroutine oceanic_lithosphere_structure_Blanco(depth,vp_ols,vs_ols,rho_ols)
use constants, only:CUSTOM_REAL
implicit none
double precision, intent(in)  ::depth
real(kind=CUSTOM_REAL),intent(out) ::vp_ols,vs_ols,rho_ols
! Blanco OTF
! Dep_Top  Vp  Vs  Rho
! 0.00  4.85  2.620  2.480
! 1.25  6.40  3.645  2.805
! 2.50  7.00  3.995  3.005
! 7.00  7.72  4.306  3.186
if(depth.le.1250.0) then
vp_ols=4850.0
vs_ols=2620.0
rho_ols=2480.0
else if(depth.le.2500.0) then
vp_ols=6400.0
vs_ols=3645.0
rho_ols=2805.0
else if(depth.le.7000.0) then
vp_ols=7000.0
vs_ols=3995.0
rho_ols=3005.0
else
vp_ols=7720.0
vs_ols=4306.0
rho_ols=3186.0
end if
end subroutine oceanic_lithosphere_structure_Blanco


subroutine add_oceanic_lithosphere(x,y,z,x_cubedsph,y_cubedsph,&
                    vp,vs,rho,myrank)
use bathymetry_model
use tele_coupling_par, only:ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES
use constants, only:CUSTOM_REAL,R_EARTH_SURF,PI
implicit none
double precision, intent(in)::x,y,z,x_cubedsph,y_cubedsph
real(kind=CUSTOM_REAL) ::vp,vs,rho
integer, intent(in) ::myrank

!Specify the two depths (in meters) at which the local/tomography model
!transitions gradually to the DSM model.
!iasp91 has a 35 km thick crust, so let's set it below 35 km, where they have
!closer structures in the continent and oceanic lithospheres.
double precision, parameter ::top_taper_depth_start = -100000.0
double precision, parameter ::top_taper_depth_end   = -100000.0
double precision, parameter ::bot_taper_depth_start = 40000.0
double precision, parameter ::bot_taper_depth_end = 50000.0

!specify the below two widths for making the local-dsm model transitions at the four
!edges of SEM box.
!This width gives the gradual tapering (in unit of degree and 0.1 degree is roughly
!11.2 km at the surface)
double precision, parameter ::taper_width_edges = 0.2 
!DSM model is used in this outermost width (in unit of degree and 0.1 degree is
!roughly 11.2 km at the surface)
double precision, parameter ::dsm_width_edges = 0.3

!local parameters
double precision::radi,depth,depth_from_ocean_bot
double precision ::x_cubedsph_shift,y_cubedsph_shift
real(kind=CUSTOM_REAL) ::bathymetry_this_point
double precision::ix_realtype,iy_realtype,ix_fraction_remain,iy_fraction_remain
integer ::ix_find,iy_find
real(kind=CUSTOM_REAL) ::vp_ols,vs_ols,rho_ols
real(kind=CUSTOM_REAL) ::vp_dsm_copy,vs_dsm_copy,rho_dsm_copy
double precision ::taper
!weightings of dsm model and oceanic lithosphere model
double precision ::w_dsm,w_ols
!Convert the unit of rad into degree. 
double precision, parameter::degtorad=PI/180.0

vp_dsm_copy=vp
vs_dsm_copy=vs
rho_dsm_copy=rho

radi=dsqrt(x**2+y**2+z**2)
depth=R_EARTH_SURF-radi

!find the seafloor depth at this point
!Note that x_cubedsph and y_cubedsph have an unit of rad.
x_cubedsph_shift=x_cubedsph+ANGULAR_WIDTH_XI_IN_DEGREES/2.0*degtorad
ix_realtype=(x_cubedsph_shift-orig_x_bathymetry*degtorad)/(spacing_x_bathymetry*degtorad)+1
ix_find=int(ix_realtype)
ix_fraction_remain=ix_realtype-ix_find
y_cubedsph_shift=y_cubedsph+ANGULAR_WIDTH_ETA_IN_DEGREES/2.0*degtorad
iy_realtype=(y_cubedsph_shift-orig_y_bathymetry*degtorad)/(spacing_y_bathymetry*degtorad)+1
iy_find=int(iy_realtype)
iy_fraction_remain=iy_realtype-iy_find

if(ix_find<1.or.ix_find>nx_bathymetry-1) then
   call exit_MPI(myrank,'error target x_cubedsphere is out of the bathymetry model range')
end if

if(iy_find<1.or.iy_find>ny_bathymetry-1) then
   call exit_MPI(myrank,'error target y_cubedsphere is out of the bathymetry model range')
end if

bathymetry_this_point=(bathymetry(ix_find,iy_find)*(1.0-ix_fraction_remain)+&
                 bathymetry(ix_find+1,iy_find)*ix_fraction_remain)*(1.0-iy_fraction_remain)+ &
                (bathymetry(ix_find,iy_find+1)*(1.0-ix_fraction_remain)+&
                 bathymetry(ix_find+1,iy_find+1)*ix_fraction_remain)*iy_fraction_remain
!Note that negative batheymtry means below the sea level while depth is positive.
depth_from_ocean_bot=depth+bathymetry_this_point
!get the vp, vs and rho of the oceanic lithosphere
!call oceanic_lithosphere_structure(depth_from_ocean_bot,vp_ols,vs_ols,rho_ols)
call oceanic_lithosphere_structure_Blanco(depth_from_ocean_bot,vp_ols,vs_ols,rho_ols)

!make a gradual transition to the dsm model at the bottom and edges of the SEM box.
call taper_to_dsm_model(taper,depth,top_taper_depth_start,top_taper_depth_end, &
                bot_taper_depth_start,bot_taper_depth_end,x_cubedsph,y_cubedsph,&
                taper_width_edges,dsm_width_edges)

w_ols=taper
w_dsm=1.0-w_ols

vp=vp_dsm_copy*w_dsm+vp_ols*w_ols
vs=vs_dsm_copy*w_dsm+vs_ols*w_ols
rho=rho_dsm_copy*w_dsm+rho_ols*w_ols
end subroutine add_oceanic_lithosphere
!***********************add oceanic lithosphere END*************************************
!***************************************************************************************

!
  subroutine  read_DSM_model (myrank)
   use tomography
  implicit none

  integer ::myrank
!local parameters for reading file
  real(kind=CUSTOM_REAL),parameter ::kmtom=1000.0
  real(kind=CUSTOM_REAL),parameter ::gcmcubetokgmcube=1000.0
  real(kind=CUSTOM_REAL):: time_series_length,omega_imag
  integer:: n_frequency,ngrid_r,lmin,lmax
  real(kind=CUSTOM_REAL):: r_freesurf,r_ICB,r_CMB
  real(kind=CUSTOM_REAL)::source_r,source_depth,source_lat,source_lon
  integer ::source_type,nexp,save_velo
  real(kind=CUSTOM_REAL)::source_mt(3,3)
  real(kind=CUSTOM_REAL)::fr,ftheta,fphi
  character(len=80) ::DSM_file_name,tmp_file_name

  integer ::i
  

  DSM_file_name = IN_DATA_FILES(1:len_trim(IN_DATA_FILES))//&
                  trim(DSM_FILENAME)
! opening the temporary file
        open( unit=11, file=DSM_file_name, status='unknown' )
! reading the parameters
! ---- parameters for time series ---
        read(11,*) time_series_length,n_frequency
        read(11,*) omega_imag
! ---- parameters for numerical grids ----
        read(11,*) ngrid_r,lmin,lmax
! ---- parameters for structure ---
        read(11,*) n_structure_zone

!allocate arrays
        allocate(rmin_structure_zone(n_structure_zone))
        allocate(rmax_structure_zone(n_structure_zone))

        allocate(rho_structure_zone(4,n_structure_zone))
        allocate(vpv_structure_zone(4,n_structure_zone))
        allocate(vph_structure_zone(4,n_structure_zone))
        allocate(vsv_structure_zone(4,n_structure_zone))
        allocate(vsh_structure_zone(4,n_structure_zone))
        allocate(eta_structure_zone(4,n_structure_zone))
        allocate(qmu_structure_zone(n_structure_zone))
        allocate(qkappa_structure_zone(n_structure_zone))
        allocate(fluid_thiszone(n_structure_zone))
        r_CMB=0.d0
        r_ICB=0.d0
        do 130 i=1,n_structure_zone
          read(11,*) rmin_structure_zone(i),rmax_structure_zone(i),&
                     rho_structure_zone(1,i),rho_structure_zone(2,i),&
                     rho_structure_zone(3,i),rho_structure_zone(4,i)
          read(11,*) vpv_structure_zone(1,i),vpv_structure_zone(2,i),&
                     vpv_structure_zone(3,i),vpv_structure_zone(4,i)
          read(11,*) vph_structure_zone(1,i),vph_structure_zone(2,i),&
                     vph_structure_zone(3,i),vph_structure_zone(4,i)
          read(11,*) vsv_structure_zone(1,i),vsv_structure_zone(2,i),&
                     vsv_structure_zone(3,i),vsv_structure_zone(4,i)
          read(11,*) vsh_structure_zone(1,i),vsh_structure_zone(2,i),&
                     vsh_structure_zone(3,i),vsh_structure_zone(4,i)
          read(11,*) eta_structure_zone(1,i),eta_structure_zone(2,i),&
                     eta_structure_zone(3,i),eta_structure_zone(4,i),&
                     qmu_structure_zone(i),qkappa_structure_zone(i)

          rmin_structure_zone(i)=rmin_structure_zone(i)*kmtom
          rmax_structure_zone(i)=rmax_structure_zone(i)*kmtom
          rho_structure_zone(:,i)=rho_structure_zone(:,i)*gcmcubetokgmcube
          vpv_structure_zone(:,i)=vpv_structure_zone(:,i)*kmtom
          vph_structure_zone(:,i)=vph_structure_zone(:,i)*kmtom
          vsv_structure_zone(:,i)=vsv_structure_zone(:,i)*kmtom
          vsh_structure_zone(:,i)=vsh_structure_zone(:,i)*kmtom


          if ( ( ( vsv_structure_zone(1,i).eq.0.d0 ).and. &
                ( vsv_structure_zone(2,i).eq.0.d0 ).and. &
                ( vsv_structure_zone(3,i).eq.0.d0 ).and. &
                ( vsv_structure_zone(4,i).eq.0.d0 )      ).or.&
              ( ( vsh_structure_zone(1,i).eq.0.d0 ).and.&
                ( vsh_structure_zone(2,i).eq.0.d0 ).and.&
                ( vsh_structure_zone(3,i).eq.0.d0 ).and.&
                ( vsh_structure_zone(4,i).eq.0.d0 )    ) ) then
                fluid_thiszone(i)=1
          else
                fluid_thiszone(i)=0
          end if

          if(i.ge.2.and.i.lt.n_structure_zone) then
              if(abs(vsv_structure_zone(1,i)).lt.1.e-7.and.&
                abs(vsv_structure_zone(1,i-1)).gt.1.e-7) then
                   r_ICB=rmin_structure_zone(i)
              end if
             if(abs(vsv_structure_zone(1,i)).gt.1.e-7.and.&
                abs(vsv_structure_zone(1,i-1)).lt.1.e-7) then
                  r_CMB=rmin_structure_zone(i)
             end if

          end if
  130   continue
        r_freesurf=rmax_structure_zone(n_structure_zone)
        if(r_CMB.lt.1.e-7.or.r_ICB.lt.1.e-7) then
               stop 'Error in finding r_CMB or r_ICB'
        end if

! ---- parameters for a source ---
        read(11,*) source_depth,source_lat,source_lon,source_type
        if(source_type.eq.1) then
           read(11,*) nexp,&
                  source_mt(1,1),source_mt(2,2),source_mt(3,3),&
                  source_mt(1,2),source_mt(1,3),source_mt(2,3)
        else if(source_type.eq.2) then
           read(11,*) nexp,fr,ftheta,fphi
        else
           stop 'Error of source_type'
        end if
        source_r = rmax_structure_zone(n_structure_zone) - source_depth
        source_mt(1,1) = source_mt(1,1) * ( 10.d0**(nexp-25) )
        source_mt(2,2) = source_mt(2,2) * ( 10.d0**(nexp-25) )
        source_mt(3,3) = source_mt(3,3) * ( 10.d0**(nexp-25) )
        source_mt(1,2) = source_mt(1,2) * ( 10.d0**(nexp-25) )
        source_mt(1,3) = source_mt(1,3) * ( 10.d0**(nexp-25) )
        source_mt(2,3) = source_mt(2,3) * ( 10.d0**(nexp-25) )
        source_mt(2,1) = source_mt(1,2)
        source_mt(3,1) = source_mt(1,3)
        source_mt(3,2) = source_mt(2,3)
        fr=fr*( 10.d0**(nexp-25) )
        ftheta=ftheta*( 10.d0**(nexp-25) )
        fphi=fphi*( 10.d0**(nexp-25) )

! --- parameters for stations ---
        read(11,*)tmp_file_name
        read(11,*)tmp_file_name
        read(11,*)tmp_file_name
        read(11,*)tmp_file_name

        read(11,*)save_velo
        close(11)

        write(IMAIN,*) 'Read DSM_model_input done.',myrank
  end subroutine read_DSM_model


  subroutine model_DSM1D(flag_media,x_eval,y_eval,z_eval,r_middle,&
                             rho_final,vp_final,vs_final,qkappa_atten,qmu_atten,myrank)

  use tomography
  use tele_coupling_par, only :COUPLING_DEPTH_TOLERENCE

  implicit none

  double precision, intent(in) :: x_eval,y_eval,z_eval,r_middle
  real(kind=CUSTOM_REAL), intent(out) ::vp_final,vs_final,rho_final,qkappa_atten,qmu_atten
  integer,intent(in) ::flag_media,myrank

  ! local parameters
  real(kind=CUSTOM_REAL) ::r,temp
  integer ::izone,izone1_r,izone2_r
  integer ::fluid_thispoint

  integer ::int_tmp
  
  int_tmp=myrank

  r=dsqrt(x_eval**2+y_eval**2+z_eval**2)
  
!  if(ELLIPTICITY) then
!     dcost = dcos(ReceiverInfo(i)%theta)
!     p20 = 0.5d0*(3.0d0*dcost*dcost-1.0d0)
!     call spline_evaluation(rspl,espl,espl2,nspl,R_EARTH,ell)
!     Rearth  =  R_EARTH*(1.0d0-(2.0d0/3.0d0)*ell*p20)
!     lat= 180.0*PI*atan(tan(PI/2-theta)/0.99329534d0)
!  else  
!     Rearth  =  R_EARTH
!     lat= 180/PI*(PI/2-theta)
!  end if
!  if(abs(r-r_layer(2)).lt.100) then
!    if(myrank.eq.1) print *,'loca',r,r_middle

!solid media
  if(flag_media.eq.2) then
!For setting the solid walls at the four edges of SEM box
!  if(flag_media.eq.2.or.flag_media.eq.3) then
      fluid_thispoint=0
!fluid media
  else if(flag_media.eq.1) then
      fluid_thispoint=1
  else
      stop 'fluid_thispoint shoud be either 1(acoustic) or 2(elastic).'
  end if


!If the element is fluid media and its depth is shallower than 11 km, 
!We know or assume that it is ocean. This assumption is reasonable in
!our DSM and SEM coupling method (working on the Earth) and
!significantly simplifies the coding. Using this assuption, we do not
!need to set up the top layer of dsm_model_input as an ocean. Then the file
!dsm_model_input can be directly used in DSM computation.

!fluid media in the top 11 km, we assume that it is water.
if(fluid_thispoint.eq.1.and.r.gt.6360000.0) then
!water
   vs_final=0.d0
   vp_final=1500.0
   rho_final=1000.0
   qmu_atten=10000.0
   qkappa_atten=10000.0
else
!non-ocean, it could be crust, mantle, outer core (fluid) or inner core.
  izone2_r=0
  izone1_r=1

!Typically, the GLL point is located inside a layer.
  do izone=1,n_structure_zone
     if(rmin_structure_zone(izone).le.r) then
         izone1_r=izone
     end if
  end do

!Special case, the GLL ploint is sitting on a disconitinuity
  do izone=n_structure_zone,1,-1
     if(abs(rmax_structure_zone(izone)-r).le.COUPLING_DEPTH_TOLERENCE) then
        izone1_r=izone
        if(izone.lt.n_structure_zone) then
          izone2_r=izone+1
          !invalid number
          if(fluid_thispoint.ne.fluid_thiszone(izone2_r)) izone2_r=0
        end if
     end if
  end do

! invalid number
  if(izone2_r.gt.n_structure_zone) izone2_r=0
  if(fluid_thispoint.ne.fluid_thiszone(izone1_r)) izone1_r=0


  if(izone2_r.eq.0.and.izone1_r.eq.0) then
      if(DEBUG_COUPLING) print *,'izone2',izone2_r,r,&
                   fluid_thispoint,rmax_structure_zone(1),&
                   r_middle,x_eval,y_eval,z_eval,180.0/3.14159*asin(z_eval/r),&
                   180.0/3.14159*atan(y_eval/(x_eval+1.e-9))
       stop 'Error, the GLL point does not match any layer of dsm_model_input. &        
             &A possible reason - this GLL point has been defined as fluid media, &
             &but the DSM layer at this depth is solid. Or vice sersa.'
  else if(izone2_r.gt.0.and.izone1_r.gt.0) then
     if(r_middle.lt.r) then
       call cal_PREM_structure(r,rho_structure_zone(1,izone1_r),temp)
       rho_final=temp
       call cal_PREM_structure(r,vpv_structure_zone(1,izone1_r),temp)
       vp_final=temp
       call cal_PREM_structure(r,vsv_structure_zone(1,izone1_r),temp)
       vs_final=temp
       qmu_atten=qmu_structure_zone(izone1_r)
       qkappa_atten=qkappa_structure_zone(izone1_r)
     else 
       call cal_PREM_structure(r,rho_structure_zone(1,izone2_r),temp)
       rho_final=temp
       call cal_PREM_structure(r,vpv_structure_zone(1,izone2_r),temp)
       vp_final=temp
       call cal_PREM_structure(r,vsv_structure_zone(1,izone2_r),temp)
       vs_final=temp
       qmu_atten=qmu_structure_zone(izone2_r)
       qkappa_atten=qkappa_structure_zone(izone2_r)
     end if

  else if(izone1_r.gt.0) then
       call cal_PREM_structure(r,rho_structure_zone(1,izone1_r),temp)
       rho_final=temp
       call cal_PREM_structure(r,vpv_structure_zone(1,izone1_r),temp)
       vp_final=temp
       call cal_PREM_structure(r,vsv_structure_zone(1,izone1_r),temp)
       vs_final=temp
       qmu_atten=qmu_structure_zone(izone1_r)
       qkappa_atten=qkappa_structure_zone(izone1_r)

  else if(izone2_r.gt.0) then
       call cal_PREM_structure(r,rho_structure_zone(1,izone2_r),temp)
       rho_final=temp
       call cal_PREM_structure(r,vpv_structure_zone(1,izone2_r),temp)
       vp_final=temp
       call cal_PREM_structure(r,vsv_structure_zone(1,izone2_r),temp)
       vs_final=temp
       call cal_PREM_structure(r,qmu_structure_zone(izone2_r),temp)
       qmu_atten=qmu_structure_zone(izone2_r)
       qkappa_atten=qkappa_structure_zone(izone2_r)
   else
        stop 'Error in DSM1D model implementation'
  end if
end if
  end subroutine model_DSM1D

  subroutine model_DSM3D(x_eval,y_eval,z_eval,xeval_cubedsph,&
                         yeval_cubedsph,zeval_cubedsph, &
                         rho_final,vp_final,vs_final,qkappa_atten,qmu_atten,myrank)
  use tomography

  implicit none

  double precision, intent(in) :: x_eval,y_eval,z_eval,xeval_cubedsph,&
                                  yeval_cubedsph,zeval_cubedsph
  real(kind=CUSTOM_REAL), intent(out) ::vp_final,vs_final,rho_final
  real(kind=CUSTOM_REAL), intent(in) ::qkappa_atten,qmu_atten
  integer,intent(in) ::myrank

  ! local parameters
  logical, parameter ::sediment_incorporated = .false.
  logical, parameter ::slab_incorporated = .false.
  logical, parameter ::oceanic_lithosphere_incorporated = .false.
  logical, parameter ::tomography_2D_incorporated = .false.
  logical, parameter ::tomography_3D_incorporated = .true.

  real(kind=CUSTOM_REAL) ::tmp_real
!********************************************************************
  !qkappa_atten and qmu_atten are not computed here, but may be added in the future.
  tmp_real=qkappa_atten
  tmp_real=qmu_atten

  if(slab_incorporated) then
   call add_slab(x_eval,y_eval,z_eval,xeval_cubedsph,yeval_cubedsph,zeval_cubedsph,&
                 vp_final,vs_final,rho_final,myrank)
  end if

  if(oceanic_lithosphere_incorporated) then
   call add_oceanic_lithosphere(x_eval,y_eval,z_eval,xeval_cubedsph,yeval_cubedsph,&
                    vp_final,vs_final,rho_final,myrank)
  end if
 
  if(tomography_2D_incorporated) then
   call get_tomography_2D(x_eval,y_eval,z_eval,xeval_cubedsph,yeval_cubedsph, &
                             rho_final,vp_final,vs_final)
  end if

  if(tomography_3D_incorporated) then
   call tomography3D_perturbation(x_eval,y_eval,z_eval,xeval_cubedsph,yeval_cubedsph, &
                             rho_final,vp_final,vs_final)
   !call tomography3D_vpvsrho_latlon(x_eval,y_eval,z_eval, &
   !                          rho_final,vp_final,vs_final)
  end if

  if(sediment_incorporated) then
   call  add_sediment_Japan(myrank,x_eval,y_eval,z_eval,vp_final,vs_final,rho_final)
  end if


  end subroutine model_DSM3D



 subroutine model_tomo3D(x_eval,y_eval,z_eval,r_middle, &
                             rho_final,vp_final,vs_final,qkappa_atten,qmu_atten,imaterial_id)

  use tomography

  implicit none

  double precision, intent(in) :: x_eval,y_eval,z_eval,r_middle
  real(kind=CUSTOM_REAL), intent(out) :: vp_final,vs_final,rho_final
  real(kind=CUSTOM_REAL), intent(out) :: qkappa_atten,qmu_atten
  integer, intent(in) :: imaterial_id


!  integer ::myrank

  ! local parameters
 double precision ::real_tmp

!The below two lines are used to avoid possible error reports during compling
!the code. r_middle and imaterial_id are not used now, that occationally causes error 
!reports. But they might be usful in the future, so we keep them here.
 real_tmp=r_middle
 real_tmp=imaterial_id


  qmu_atten = 80.0
  qkappa_atten=9999.

  if(z_eval.gt.-30000.0) then
     vp_final=6800.0
     vs_final=3900.0
     rho_final=2900.0
  else if(z_eval.gt.-101000.0) then
     vp_final=8110.0
     vs_final=4490.0
     rho_final=3380.0
  else 
     stop 'Error tomography_ningxia'
  end if

  !call tomography3D_perturbation(x_eval,y_eval,z_eval, &
  !                           rho_final,vp_final,vs_final)
  end subroutine model_tomo3D





  subroutine cal_PREM_structure(r,param,final_value)
   use constants
   implicit none

   real(kind=CUSTOM_REAL), intent(in)::r
   real(kind=CUSTOM_REAL), intent(in)::param(4)
   real(kind=CUSTOM_REAL), intent(out) ::final_value
   real(kind=CUSTOM_REAL)::a,rearth_prem
   rearth_prem=6371000.0
   a=r/rearth_prem
   final_value=param(1) &
               + param(2)*a &
               + param(3)*a*a &
               + param(4)*a*a*a  
   if(final_value.lt.1.d0) then
   !  print *,'par',param(:),a
   end if

  end subroutine cal_PREM_structure


!***********************find the perturbation***********************
  subroutine tomography3D_perturbation(x_eval,y_eval,z_eval,xeval_cubedsph,yeval_cubedsph,&
                             rho_final,vp_final,vs_final)
  use tomography
  use tele_coupling_par, only :R_TOP_BOUND

  implicit none

  double precision, intent(in) ::x_eval,y_eval,z_eval,xeval_cubedsph,yeval_cubedsph
  real(kind=CUSTOM_REAL), intent(out) :: vp_final,vs_final,rho_final

  !no top tapering. Set both depths as R_Earth-R_ICB, in unit of km.
  double precision, parameter ::top_taper_depth_start =  10000.0
  double precision, parameter ::top_taper_depth_end =    30000.0
  !bottom tapering. Taper starts at 70 km and ends at 80 km below the ICB.
  double precision, parameter ::bot_taper_depth_start = 50000.0
  double precision, parameter ::bot_taper_depth_end =   140000.0
  !tapering at edges. Taper width and homogeneous-region width, in unit of
  !degree.
  double precision, parameter ::taper_width_edges = 0.5
  double precision, parameter ::dsm_width_edges = 0.5


  ! local parameters
  double precision ::taper,weight_tomo
  integer :: ix,iy,iz
  integer :: p0,p1,p2,p3,p4,p5,p6,p7

  double precision ::x_eval_proj,y_eval_proj,z_eval_proj
  double precision :: spac_x,spac_y,spac_z
  double precision :: gamma_interp_x,gamma_interp_y
  double precision :: gamma_interp_z1,gamma_interp_z2,gamma_interp_z3, &
    gamma_interp_z4,gamma_interp_z5,gamma_interp_z6,gamma_interp_z7,gamma_interp_z8
  real(kind=CUSTOM_REAL) :: vp1,vp2,vp3,vp4,vp5,vp6,vp7,vp8, &
    vs1,vs2,vs3,vs4,vs5,vs6,vs7,vs8,rho1,rho2,rho3,rho4,rho5,rho6,rho7,rho8
  real(kind=CUSTOM_REAL) ::vp_tomo,vs_tomo,rho_tomo
  real(kind=CUSTOM_REAL) :: perturbation
  logical ::abs_vpvsrho


  double precision ::r,depth_below_top
!  double precision ::theta,Phi,lat,lon,Rearth

  r=dsqrt(x_eval**2+y_eval**2+z_eval**2)
!  theta=dacos(z_eval/r)
  if(y_eval<0.0) then
!       Phi=2*PI-dacos(x_eval*(1.d0-1.e-10)/(r*dsin(Theta)))
  else
!       Phi=dacos(x_eval*(1.d0-1.e-10)/(r*dsin(Theta)))
  end if
!  x_eval_proj=r*dcos(Phi)
!  y_eval_proj=r*dsin(Phi)
!  z_eval_proj=0.0
  x_eval_proj=x_eval
  y_eval_proj=y_eval
  z_eval_proj=z_eval

  !lon=Phi/PI*180.0

!  if(ELLIPTICITY) then
!     dcost = dcos(ReceiverInfo(i)%theta)
!     p20 = 0.5d0*(3.0d0*dcost*dcost-1.0d0)
!     call spline_evaluation(rspl,espl,espl2,nspl,R_EARTH,ell)
!     Rearth  =  R_EARTH*(1.0d0-(2.0d0/3.0d0)*ell*p20)
!     lat= 180.0*PI*atan(tan(PI/2-theta)/0.99329534d0)
!  else  
!     Rearth  =  R_EARTH
!     lat= 180/PI*(PI/2-theta)
!  end if

  !lat= 180.0/PI*(PI/2-theta)

  depth_below_top=R_TOP_BOUND-r
  ! determine spacing and cell for linear interpolation
  
  !spac_x = (lon - ORIG_X) / SPACING_X
  !spac_y = (lat - ORIG_Y) / SPACING_Y
  !spac_z = (depth - ORIG_Z) / SPACING_Z
  spac_x = (x_eval_proj - ORIG_X) / SPACING_X
  spac_y = (y_eval_proj - ORIG_Y) / SPACING_Y
  spac_z = (z_eval_proj - ORIG_Z) / SPACING_Z

  ix = int(spac_x)
  iy = int(spac_y)
  iz = int(spac_z)

  gamma_interp_x = spac_x - dble(ix)
  gamma_interp_y = spac_y - dble(iy)

  ! suppress edge effects for points outside of the model SPOSTARE DOPO
  if(ix < 0) then
    ix = 0
    gamma_interp_x = 0.d0
  endif
  if(ix > NX-2) then
    ix = NX-2
    gamma_interp_x = 1.d0
  endif

  if(iy < 0) then
    iy = 0
    gamma_interp_y = 0.d0
  endif
  if(iy > NY-2) then
    iy = NY-2
    gamma_interp_y = 1.d0
  endif

  if(iz < 0) then
     iz = 0
  !   gamma_interp_z = 0.d0
  endif
  if(iz > NZ-2) then
     iz = NZ-2
  !  gamma_interp_z = 1.d0
  endif


  ! define 8 corners of interpolation element
  p0 = ix+iy*NX+iz*(NX*NY)
  p1 = (ix+1)+iy*NX+iz*(NX*NY)
  p2 = (ix+1)+(iy+1)*NX+iz*(NX*NY)
  p3 = ix+(iy+1)*NX+iz*(NX*NY)
  p4 = ix+iy*NX+(iz+1)*(NX*NY)
  p5 = (ix+1)+iy*NX+(iz+1)*(NX*NY)
  p6 = (ix+1)+(iy+1)*NX+(iz+1)*(NX*NY)
  p7 = ix+(iy+1)*NX+(iz+1)*(NX*NY)

  gamma_interp_z1=0.d0
  gamma_interp_z2=0.d0
  gamma_interp_z3=0.d0
  gamma_interp_z4=0.d0

 

  if(z_tomography(p4+1) == z_tomography(p0+1)) then
          gamma_interp_z1 = 1.d0
  else
          !if(abs(z_tomography(p4+1)-z_tomography(p0+1)).lt.1.e-5) &
             !print *,'floating1',z_tomography(p4+1),z_tomography(p0+1),p4,p0
          gamma_interp_z1 = (z_eval-z_tomography(p0+1))/(z_tomography(p4+1)-z_tomography(p0+1))
  endif
  if(gamma_interp_z1 > 1.d0) then
          gamma_interp_z1 = 1.d0
  endif
  if(gamma_interp_z1 < 0.d0) then
          gamma_interp_z1 = 0.d0
  endif


  if(z_tomography(p5+1) == z_tomography(p1+1)) then
          gamma_interp_z2 = 1.d0
  else
          !if(abs(z_tomography(p5+1)-z_tomography(p1+1)).lt.1.e-5) &
             !print *,'floating2',z_tomography(p5+1),z_tomography(p1+1),p5,p1
          gamma_interp_z2 = (z_eval-z_tomography(p1+1))/(z_tomography(p5+1)-z_tomography(p1+1))
  endif
  if(gamma_interp_z2 > 1.d0) then
          gamma_interp_z2 = 1.d0
  endif
  if(gamma_interp_z2 < 0.d0) then
          gamma_interp_z2 = 0.d0
  endif


  if(z_tomography(p6+1) == z_tomography(p2+1)) then
          gamma_interp_z3 = 1.d0
  else
          !if(abs(z_tomography(p6+1)-z_tomography(p2+1)).lt.1.e-5) &
             !print *,'floating3',z_tomography(p6+1),z_tomography(p2+1),p6,p2
          gamma_interp_z3 = (z_eval-z_tomography(p2+1))/(z_tomography(p6+1)-z_tomography(p2+1))
  endif
  if(gamma_interp_z3 > 1.d0) then
          gamma_interp_z3 = 1.d0
  endif
  if(gamma_interp_z3 < 0.d0) then
          gamma_interp_z3 = 0.d0
  endif


  if(z_tomography(p7+1) == z_tomography(p3+1)) then
          gamma_interp_z4 = 1.d0
  else
          !if(abs(z_tomography(p7+1)-z_tomography(p3+1)).lt.1.e-5) &
            !print *,'floating4',z_tomography(p7+1),z_tomography(p3+1),p7,p3
          gamma_interp_z4 = (z_eval-z_tomography(p3+1))/(z_tomography(p7+1)-z_tomography(p3+1))
  endif
  if(gamma_interp_z4 > 1.d0) then
          gamma_interp_z4 = 1.d0
  endif
  if(gamma_interp_z4 < 0.d0) then
          gamma_interp_z4 = 0.d0
  endif

  gamma_interp_z5 = 1. - gamma_interp_z1
  gamma_interp_z6 = 1. - gamma_interp_z2
  gamma_interp_z7 = 1. - gamma_interp_z3
  gamma_interp_z8 = 1. - gamma_interp_z4

  vp1 = vp_tomography(p0+1)
  vp2 = vp_tomography(p1+1)
  vp3 = vp_tomography(p2+1)
  vp4 = vp_tomography(p3+1)
  vp5 = vp_tomography(p4+1)
  vp6 = vp_tomography(p5+1)
  vp7 = vp_tomography(p6+1)
  vp8 = vp_tomography(p7+1)

  vs1 = vs_tomography(p0+1)
  vs2 = vs_tomography(p1+1)
  vs3 = vs_tomography(p2+1)
  vs4 = vs_tomography(p3+1)
  vs5 = vs_tomography(p4+1)
  vs6 = vs_tomography(p5+1)
  vs7 = vs_tomography(p6+1)
  vs8 = vs_tomography(p7+1)

  rho1 = rho_tomography(p0+1)
  rho2 = rho_tomography(p1+1)
  rho3 = rho_tomography(p2+1)
  rho4 = rho_tomography(p3+1)
  rho5 = rho_tomography(p4+1)
  rho6 = rho_tomography(p5+1)
  rho7 = rho_tomography(p6+1)
  rho8 = rho_tomography(p7+1)

  !add taper at the top, bottom and edges of the simulated model to make it
  !smoothly transition to the DSM model
  call taper_to_dsm_model(taper,depth_below_top,top_taper_depth_start,&
               top_taper_depth_end,bot_taper_depth_start,bot_taper_depth_end, &
               xeval_cubedsph,yeval_cubedsph,taper_width_edges,dsm_width_edges)
  weight_tomo=taper

  ! use trilinear interpolation in cell to define Vp Vs and rho
  abs_vpvsrho=.false.
  if(abs_vpvsrho) then
   vp_tomo = &
     vp1*(1.-gamma_interp_x)*(1.-gamma_interp_y)*(1.-gamma_interp_z1) + &
     vp2*gamma_interp_x*(1.-gamma_interp_y)*(1.-gamma_interp_z2) + &
     vp3*gamma_interp_x*gamma_interp_y*(1.-gamma_interp_z3) + &
     vp4*(1.-gamma_interp_x)*gamma_interp_y*(1.-gamma_interp_z4) + &
     vp5*(1.-gamma_interp_x)*(1.-gamma_interp_y)*gamma_interp_z1 + &
     vp6*gamma_interp_x*(1.-gamma_interp_y)*gamma_interp_z2 + &
     vp7*gamma_interp_x*gamma_interp_y*gamma_interp_z3 + &
     vp8*(1.-gamma_interp_x)*gamma_interp_y*gamma_interp_z4

   vs_tomo = &
     vs1*(1.-gamma_interp_x)*(1.-gamma_interp_y)*(1.-gamma_interp_z1) + &
     vs2*gamma_interp_x*(1.-gamma_interp_y)*(1.-gamma_interp_z2) + &
     vs3*gamma_interp_x*gamma_interp_y*(1.-gamma_interp_z3) + &
     vs4*(1.-gamma_interp_x)*gamma_interp_y*(1.-gamma_interp_z4) + &
     vs5*(1.-gamma_interp_x)*(1.-gamma_interp_y)*gamma_interp_z1 + &
     vs6*gamma_interp_x*(1.-gamma_interp_y)*gamma_interp_z2 + &
     vs7*gamma_interp_x*gamma_interp_y*gamma_interp_z3 + &
     vs8*(1.-gamma_interp_x)*gamma_interp_y*gamma_interp_z4

   rho_tomo = &
     rho1*(1.-gamma_interp_x)*(1.-gamma_interp_y)*(1.-gamma_interp_z1) + &
     rho2*gamma_interp_x*(1.-gamma_interp_y)*(1.-gamma_interp_z2) + &
     rho3*gamma_interp_x*gamma_interp_y*(1.-gamma_interp_z3) + &
     rho4*(1.-gamma_interp_x)*gamma_interp_y*(1.-gamma_interp_z4) + &
     rho5*(1.-gamma_interp_x)*(1.-gamma_interp_y)*gamma_interp_z1 + &
     rho6*gamma_interp_x*(1.-gamma_interp_y)*gamma_interp_z2 + &
     rho7*gamma_interp_x*gamma_interp_y*gamma_interp_z3 + &
     rho8*(1.-gamma_interp_x)*gamma_interp_y*gamma_interp_z4

  ! impose minimum and maximum velocity and density if needed
     if(vp_tomo < VP_MIN) vp_tomo = VP_MIN
     if(vs_tomo < VS_MIN) vs_tomo = VS_MIN
     if(rho_tomo < RHO_MIN) rho_tomo = RHO_MIN
     if(vp_tomo > VP_MAX) vp_tomo = VP_MAX
     if(vs_tomo > VS_MAX) vs_tomo = VS_MAX
     if(rho_tomo > RHO_MAX) rho_tomo = RHO_MAX
  
     if(ix.eq.(NX-2).or.ix.eq.0.or.iy.eq.NY-2.or.&
        iy.eq.0.or.iz.eq.NZ-2.or.iz.eq.0) then
         vp_tomo=vp1;vs_tomo=vs1;rho_tomo=rho1
     end if
     vp_final=vp_final*(1.0-weight_tomo)+vp_tomo*weight_tomo
     vs_final=vs_final*(1.0-weight_tomo)+vs_tomo*weight_tomo
     rho_final=rho_final*(1.0-weight_tomo)+rho_tomo*weight_tomo
  else
     perturbation= &
       vp1*(1.-gamma_interp_x)*(1.-gamma_interp_y)*(1.-gamma_interp_z1) + &
       vp2*gamma_interp_x*(1.-gamma_interp_y)*(1.-gamma_interp_z2) + &
       vp3*gamma_interp_x*gamma_interp_y*(1.-gamma_interp_z3) + &
       vp4*(1.-gamma_interp_x)*gamma_interp_y*(1.-gamma_interp_z4) + &
       vp5*(1.-gamma_interp_x)*(1.-gamma_interp_y)*gamma_interp_z1 + &
       vp6*gamma_interp_x*(1.-gamma_interp_y)*gamma_interp_z2 + &
       vp7*gamma_interp_x*gamma_interp_y*gamma_interp_z3 + &
       vp8*(1.-gamma_interp_x)*gamma_interp_y*gamma_interp_z4
    
       !Above 660-km, the perturbation of MORB is much smaller.
       if(depth_below_top<50000.0) then
              weight_tomo=0.2
       end if

       !weight_tomo=1.0
       !For real MORB, its velocity is lower than Pyrolite, not like +/- perturbation.
       !20% accumulated MORB decreases the reflection coefficient. Needs to correct the velocity/density contrast across 660-km.
       perturbation=perturbation-0.009
       vp_final=vp_final*(1+perturbation*weight_tomo)
       vs_final=vs_final*(1+perturbation*weight_tomo)
       rho_final=rho_final*(1+perturbation*weight_tomo)

       if(perturbation.gt.0.3.or.perturbation.lt.-0.3) stop 'Error perturbation'
!       write(*,*) perturbation

  end if

  end subroutine tomography3D_perturbation


!***********************find the perturbation***********************
  subroutine tomography3D_vpvsrho_latlon(x_eval,y_eval,z_eval, &
                             rho_final,vp_final,vs_final)
  use tomography
  use tele_coupling_par, only :R_TOP_BOUND

  implicit none

  double precision, intent(in) :: x_eval,y_eval,z_eval
  real(kind=CUSTOM_REAL), intent(out) :: vp_final,vs_final,rho_final

  ! local parameters
  integer :: ix,iy,iz
  integer :: i,j,k,l
  double precision ::x_eval_proj,y_eval_proj,z_eval_proj
  double precision :: spac_x,spac_y,spac_z
  double precision :: dx,dy,dz
  real :: dist,t_total
  real :: new_vp,new_vs,new_rho
  double precision ::r,depth,r_top
  double precision ::theta,phi,lat,lon

  r=dsqrt(x_eval**2+y_eval**2+z_eval**2)
  theta=dacos(z_eval/r)
  if(y_eval<0.0) then
       phi=2*PI-dacos(x_eval*(1.d0-TINYVAL)/(r*dsin(Theta)))
  else
       phi=dacos(x_eval*(1.d0-TINYVAL)/(r*dsin(Theta)))
  end if
  r_top=R_TOP_BOUND
  lon=phi/PI*180.0
  lat= 180.0/PI*(PI/2-theta)
  depth=r_top-r

  spac_x = (lon - ORIG_X) / SPACING_X
  spac_y = (lat - ORIG_Y) / SPACING_Y
  spac_z = (depth - ORIG_Z) / SPACING_Z

  ix = int(spac_x)
  iy = int(spac_y)
  iz = int(spac_z)

  new_vp = 0
  new_vs = 0
  new_rho = 0
  t_total = 0
  if(ix.gt.0.and.ix.le.NX.and.iy.gt.0.and.iy.le.NY.and.iz.gt.0.and.iz.le.NZ) then
     do i=0,1
        do j=0,1
           do k=0,1
              depth = ORIG_Z+(iz+k)*SPACING_Z
              r = r_top-depth !(ORIG_Z+(iz+k)*SPACING_Z)
              lon  = ORIG_X+(ix+i)*SPACING_X
              lat  = ORIG_Y+(iy+j)*SPACING_Y
              if (lon.gt.0) then
                 phi = lon/180.0*PI
              else
                 phi = 2*PI + lon/180*PI
              end if
              theta = PI/2-lat/180*PI !lat= 180.0/PI*(PI/2-theta)
              x_eval_proj=dcos(theta)*r*dcos(phi) !x_eval
              y_eval_proj=dcos(theta)*r*dsin(phi) !r=dsqrt(x_eval**2+y_eval**2+z_eval**2)
              z_eval_proj=dsin(theta)*r   !theta=dacos(z_eval/r)
              dx = x_eval-x_eval_proj
              dy = y_eval-y_eval_proj
              dz = z_eval-z_eval_proj
              l = (iz+k)*NX*NY+(iy+j)*NX+ix+i+1
              dist = (dx*dx+dy*dy+dz*dz)/1000000
              t_total = t_total+1.0/dist
              new_vp = new_vp+vp_tomography(l)/dist
              new_vs = new_vs+vs_tomography(l)/dist
              new_rho = new_rho + rho_tomography(l)/dist
           enddo
        enddo
     enddo
     new_vp = new_vp/t_total
     new_vs = new_vs/t_total
     new_rho = new_rho/t_total
   
     if(new_vp.lt.0.1.or.new_vs.lt.0.1.or.new_rho.lt.0.1) then
        stop 'Error in vp, vs or rho of 3-D tomography model'
     else
       vp_final=new_vp
       vs_final=new_vs
       rho_final=new_rho
     endif
  else
     stop 'Error, the 3-D tomography model does not cover this position'
  endif
  end subroutine tomography3D_vpvsrho_latlon



  subroutine deallocate_tomography_files()

    use tomography

    implicit none

    ! deallocates models dimensions
    !deallocate(ORIG_X,ORIG_Y,ORIG_Z)
    !deallocate(SPACING_X,SPACING_Y,SPACING_Z)

    ! deallocates models parameter records
    deallocate(vp_tomography)
    deallocate(vs_tomography)
    deallocate(rho_tomography)
    deallocate(z_tomography)

    ! deallocates models entries
    !deallocate(NX,NY,NZ)
    !deallocate(nrecord)

    ! deallocates models min/max statistics
    !deallocate(VP_MIN,VS_MIN,RHO_MIN)
    !deallocate(VP_MAX,VS_MAX,RHO_MAX)

    ! q values
!    if (any(tomo_has_q_values)) then
!      deallocate(qp_tomography)
!      deallocate(qs_tomography)
!    endif
!    deallocate(tomo_has_q_values)

  end subroutine deallocate_tomography_files

    subroutine deallocate_tomography_2D_files()
    use tomography_2D

    implicit none
    deallocate(vp_tomography_2D)
    deallocate(vs_tomography_2D)
    deallocate(rho_tomography_2D)

  end subroutine deallocate_tomography_2D_files


!***********************find the perturbation***********************
  subroutine get_tomography_2D(x,y,z,x_cubedsph,y_cubedsph, &
                             rho_final,vp_final,vs_final)
    use tomography_2D
    implicit none

    double precision, intent(in) :: x,y,z,x_cubedsph,y_cubedsph
    real(kind=CUSTOM_REAL), intent(out) :: vp_final,vs_final,rho_final

    !Specify the two depths (in meters) at which the local/tomography model
    !transitions gradually to the DSM model.

!380 km thick of 660km coupling
!    double precision, parameter ::top_taper_depth_start = 490000.0
!    double precision, parameter ::top_taper_depth_end =   510000.0
!    double precision, parameter ::bot_taper_depth_start = 810000.0
!    double precision, parameter ::bot_taper_depth_end =   830000.0

!300 km thick of 660km coupling
!    double precision, parameter ::top_taper_depth_start = 530000.0
!    double precision, parameter ::top_taper_depth_end =   550000.0
!    double precision, parameter ::bot_taper_depth_start = 770000.0
!    double precision, parameter ::bot_taper_depth_end =   790000.0

!100 km thick of 660km coupling
    double precision, parameter ::top_taper_depth_start = 614000.0
    double precision, parameter ::top_taper_depth_end =   625000.0
    double precision, parameter ::bot_taper_depth_start = 695000.0
    double precision, parameter ::bot_taper_depth_end =   705000.0
    
    !specify the below two widths for making the local/tomography model transitions 
    !to the dsm model at the four edges.
    !This width describes the gradual tapering (in unit of degree and 0.1 degree is
    !roughly 11.2 km at the surface)
    double precision, parameter ::taper_width_edges = 0.2
    !DSM model is implemented in this outermost width (in unit of degree and 0.1 degree is
    !roughly 11.2 km at the surface)
    double precision, parameter ::dsm_width_edges = 0.2

    ! local parameters
    double precision ::taper,weight_tomo
    integer :: idepth,iy
    integer :: p0,p1,p2,p3

    double precision :: spac_depth,spac_y
    double precision :: gamma_interp_depth,gamma_interp_y
    real(kind=CUSTOM_REAL) :: vp1,vp2,vp3,vp4,vs1,vs2,vs3,vs4,&
               rho1,rho2,rho3,rho4
    real(kind=CUSTOM_REAL) ::vp_tomo,vs_tomo,rho_tomo
    logical ::abs_vpvsrho
    double precision ::r,depth_this_pt


    r=dsqrt(x**2+y**2+z**2)
    depth_this_pt=R_EARTH_SURF-r

    spac_depth = (depth_this_pt - ORIG_DEPTH_2D) / SPACING_DEPTH_2D
    spac_y = (y - ORIG_Y_2D) / SPACING_Y_2D

    idepth = int(spac_depth)
    iy = int(spac_y)

    gamma_interp_depth = spac_depth - dble(idepth)
    gamma_interp_y = spac_y - dble(iy)

    ! suppress edge effects for points outside of the model 
    if(idepth < 0) then
      idepth = 0
      gamma_interp_depth = 0.d0
    endif
    if(idepth > NDEPTH_2D-2) then
      idepth = NDEPTH_2D-2
      gamma_interp_depth = 1.d0
    endif

    if(iy < 0) then
      iy = 0
      gamma_interp_y = 0.d0
    endif
    if(iy > NY_2D-2) then
      iy = NY_2D-2
      gamma_interp_y = 1.d0
    endif
  
  
    ! define 4 corners of interpolation element
    p0 = iy+idepth*NY_2D
    p1 = (iy+1)+idepth*NY_2D
    p2 = iy+(idepth+1)*NY_2D
    p3 = (iy+1)+(idepth+1)*NY_2D
  
    vp1 = vp_tomography_2D(p0+1)
    vp2 = vp_tomography_2D(p1+1)
    vp3 = vp_tomography_2D(p2+1)
    vp4 = vp_tomography_2D(p3+1)
  
    vs1 = vs_tomography_2D(p0+1)
    vs2 = vs_tomography_2D(p1+1)
    vs3 = vs_tomography_2D(p2+1)
    vs4 = vs_tomography_2D(p3+1)
  
    rho1 = rho_tomography_2D(p0+1)
    rho2 = rho_tomography_2D(p1+1)
    rho3 = rho_tomography_2D(p2+1)
    rho4 = rho_tomography_2D(p3+1)
  
    ! use trilinear interpolation in cell to define Vp Vs and rho
    vp_tomo = &
       vp1*(1.-gamma_interp_depth)*(1.-gamma_interp_y) + &
       vp2*(1.-gamma_interp_depth)*gamma_interp_y + &
       vp3*gamma_interp_depth*(1.-gamma_interp_y) + &
       vp4*gamma_interp_depth*gamma_interp_y
 
    vs_tomo = &
       vs1*(1.-gamma_interp_depth)*(1.-gamma_interp_y) + &
       vs2*(1.-gamma_interp_depth)*gamma_interp_y + &
       vs3*gamma_interp_depth*(1.-gamma_interp_y) + &
       vs4*gamma_interp_depth*gamma_interp_y
  
    rho_tomo = &
       rho1*(1.-gamma_interp_depth)*(1.-gamma_interp_y) + &
       rho2*(1.-gamma_interp_depth)*gamma_interp_y + &
       rho3*gamma_interp_depth*(1.-gamma_interp_y) + &
       rho4*gamma_interp_depth*gamma_interp_y
  
    !add taper at the top, bottom and edges of the simulated model to make it smoothly 
    !transition to the DSM model
    call taper_to_dsm_model(taper,depth_this_pt,top_taper_depth_start,&
               top_taper_depth_end,bot_taper_depth_start,bot_taper_depth_end, &
               x_cubedsph,y_cubedsph,taper_width_edges,dsm_width_edges)

    weight_tomo=taper
    !weight_tomo=1.0
    abs_vpvsrho=.false.
    if(abs_vpvsrho) then
       vp_final=vp_final*(1.0-weight_tomo)+vp_tomo*weight_tomo
       vs_final=vs_final*(1.0-weight_tomo)+vs_tomo*weight_tomo
       rho_final=rho_final*(1.0-weight_tomo)+rho_tomo*weight_tomo
    else
       vp_final=vp_final*(1.0+vp_tomo*weight_tomo)
       vs_final=vs_final*(1.0+vs_tomo*weight_tomo)
       rho_final=rho_final*(1.0+rho_tomo*weight_tomo)
    end if

  end subroutine get_tomography_2D
