!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module CON_axes

  ! CON uses GSE coordinates for planetary data, because it is convenient
  !     as well as it is inertial except for orbital motion. It connects
  !     the planet and the Sun with the X axis, which makes it the
  !     ideal choice for describing the whole space weather simulation.
  !
  ! \bigskip
  !
  ! {\bf Coordinate system definitions for SWMF}
  !
  ! \bigskip
  !
  ! Coordinate systems with their origin in the center of the planet:
  ! \begin{verbatim}
  ! GEI (Geocentric Equatorial Inertial)
  ! PEI (Planetocentric Equatorial Inertial)
  !
  !   Z parallel with the rotation axis.
  !   X points towards the vernal equinox.
  !   Y completes the right handed coordinate system.
  !   GEI orbits around the Sun.
  !   GEI does not rotate except for the precession and nutation of the
  !       rotation axis of the Earth.
  !   Inertial forces are negligible.
  !
  ! GSE (Geocentric Solar Ecliptic)
  ! PSO (Planetocentric Solar Orbital)
  !
  !   X towards the Sun (S)
  !   Z orthogonal to Orbital/Ecliptic plane pointing "North"
  !   Y opposite of orbital velocity
  !
  !   GSE is rotating around the Z axis with the orbital angular speed.
  !   GSE orbits around the Sun.
  !   The inertial forces can be neglected.

  ! GSM (Geocentric Solar Magnetic)
  ! PSM (Planetocentric Solar Magnetic)
  !
  !   X axis points towards the Sun (as GSE)
  !   Z axis is rotated such that the magnetic axis lies in the X-Z plane
  !   Y completes the right handed coordinate system
  !
  !   GSM is rotating around the Z axis with the orbital angular speed.
  !   GSM is rotating around the X axis back and forth with the projection
  !       of the magnetic axis motion, which depends on the rotational angular
  !       speed and on the angle between the magnetic and rotational axes.
  !   GSM orbits around the Sun.
  !   Inertial forces can be neglected if the magnetic and rotational axes are
  !   (almost) aligned and/or the rotation speed is slow relative to dynamical
  !   time scales.

  ! SMG (Solar MaGnetic Coordinates)
  !
  !   Z is the magnetic axis pointing "North".
  !   Y is orthogonal to the direction to the Sun.
  !   X completes the right handed cooridinate system with X
  !     pointing "towards" the Sun.
  !
  !   SMG wobbles around due to the rotation of the magnetic axis.
  !   SMG orbits around the Sun.
  !   SMG differs from GSM in a rotation around the Y axis.
  !
  !   Inertial forces can be neglected if the magnetic and rotational axes are
  !   (almost) aligned and/or the rotation speed is slow relative to dynamical
  !   time scales.

  ! GEO (GEOgraphic)
  ! PGR (PlanetoGRaphic)
  !
  !   Z is the rotation axis pointing "North".
  !   X goes through the 0 meridian which is defined for the planet.
  !   Y completes the right handed coordinate system.
  !
  !   GEO is a corotating coordinate system.
  !   GEO rotates around the Z axis with the inertial angular speed
  !       of the planet (which is NOT 2*Pi/(24*3600.0) for the Earth).
  !       For the Earth the 0 meridian goes through Greenich. For other planets
  !       the 0 meridian is defined as the half plane which is AngleEquinox
  !       away from the direction of equinox at the time of equinox.
  !   GEO orbits around the Sun.
  !   Inertial forces may or may not be negligible.

  ! MAG (Magnetic coordinates)
  !
  !   Z is the magnetic axis pointing "North".
  !   Y axis is orthogonal to rotational axis pointing towards Omega x Z
  !   X completes the right handed coordinate system.
  !
  !   MAG rotates around the rotational axis which does not coincide with
  !       any of the principal axes.
  !   MAG orbits around the Sun.
  !
  !   Inertial forces may or may not be negligible.

  ! HGI (HelioGraphic Inertial coordinates)
  !
  !   Z is the rotation axis of the Sun pointing "North".
  !   X axis is the intersection of the ecliptic and solar equatorial planes,
  !     which was at 74.367 degrees ecliptic longitude at 12:00 UT 01/01/1900
  !     by default but we allow a rotation around Z by the dLongitudeHgi angle.
  !   Y axis completes the right handed coordinate system.
  !
  !   HGI is a truly inertial system.

  ! HGC (HelioGraphic Corotating coordinates)
  !
  !   Z is the rotation axis of the Sun pointing "North".
  !   X axis rotates with the Carrington rotation with a 25.38 day period
  !     with respect to an inertial frame.
  !     The X axis coincides with the X axis of the HGI system at the
  !     initial time of the simulation.
  !   Y axis completes the right handed coordinate system.
  !
  !   Inertial forces should be taken into account.

  ! HGR (HelioGraphic Rotating coordinates)
  !
  !   Z is the rotation axis of the Sun pointing "North".
  !   X axis rotates with the Carrington rotation with a 25.38 day period
  !     with respect to an inertial frame (and around 27.3 day period
  !     with respect to the direction towards the Earth).
  !     The X axis coincided with the X axis of the HGI system on
  !     January 1 1854 12:00:00, but we allow a rotation by dLongitudeHgr
  !     around the Z axis.
  !   Y axis completes the right handed coordinate system.
  !
  !   Inertial forces should be taken into account.
  !
  ! \end{verbatim}

  use ModKind
  use ModCoordTransform, ONLY: rot_matrix_x, rot_matrix_y, rot_matrix_z, &
       show_rot_matrix, cross_product, dir_to_xyz, xyz_to_dir, xyz_to_lonlat, &
       atan2_check
  use ModTimeConvert, ONLY: time_int_to_real, time_real_to_int, TimeType
  use ModPlanetConst, ONLY: DipoleStrengthPlanet_I, Earth_, iPlanet, &
       get_planet_orbit, igrf_mag_axis
  use CON_planet, ONLY: UseSetMagAxis, UseSetRotAxis, UseAlignedAxes, &
       UseRealMagAxis, UseRealRotAxis, MagAxisThetaGeo, MagAxisPhiGeo, &
       MagAxisTheta, MagAxisPhi, DipoleStrength, RotAxisTheta, RotAxisPhi, &
       UseRotation, RadiusPlanet, OmegaPlanet, OmegaOrbit, OmegaRotation, &
       TypeBField, DoUpdateB0, DtUpdateB0, &
       IsInitializedPlanet, tStart, IsOrbitSet, Orbit, &
       AngleGeiGeoStart, MeridianGeo, &
       is_planet_init, get_rotation_axis_hgi, get_gei_geo_angle, orbit_in_hgi
  use ModNumConst, ONLY: &
       cHalfPi, cRadToDeg, cDegToRad, cTwoPi, cTwoPi8, cUnit_DD, cTiny
  use ModConst, ONLY: rSun, cAU
  use ModUtilities, ONLY: CON_stop, CON_set_do_test
  use CON_star, ONLY:OmegaCarrington=>OmegaStar, &
       tStartCarringtonCoord=>tAlignmentHgrHgi
  ! revision history:
  ! 01Aug03 - Gabor Toth and Aaron Ridley  - initial version
  ! 14Aug03 - Gabor Toth <gtoth@umich.edu> - major revision and extension
  ! 23Mar04 - Gabor Toth eliminated the use of CON_time to make
  !                      CON_axes more indepenedent
  ! 17Jan05 - Ofer Cohen and G. Toth merged in GEOPACK and added functions
  !                      angular_velocity and transform_velocity
  ! 21Jul25 - Gabor Toth Generalized orbit and rotation using IAU values
  !                      and true Kepler solver

  implicit none

  save

  character(len=*), parameter, private :: NameMod='CON_axes'

  integer, parameter, private :: x_=1, y_=2, z_=3

  ! Position and Velocity of Planet in HGI, Planet distance in m
  real :: XyzPlanetHgi_D(3), vPlanetHgi_D(3), PlanetDistance

  ! Rotation axis in HGI is fixed
  real :: RotAxisHgi0_D(3) = 0.0

  ! Offset longitude angle for hgr and hgi systems in degrees and radians
  real :: dLongitudeHgrDeg = 0.0, dLongitudeHgr = 0.0
  real :: dLongitudeHgiDeg = 0.0, dLongitudeHgi = 0.0

  ! Rotational axis in GSE and GSM
  real    :: RotAxis_D(3)      ! Permanent Cartesian components in GSE
  real    :: RotAxisGsm_D(3)   ! Changing  Cartesian components in GSM
  !$acc declare create(RotAxis_D, RotAxisGsm_D)

  ! Subsolar point in GEO
  real :: LonSubsolar, LatSubsolar

  ! Magnetic axis in GEO, GEI and GSE
  real    :: MagAxisGeo_D(3)                         ! Permanent vector in GEO
  real    :: MagAxis0Gei_D(3)  ! Starting position of the magnetix axis in GEI
  real    :: MagAxis_D(3)      ! Current  position of the magnetix axis in GSE
  real    :: MagAxisGsm_D(3)   ! Current  position of the magnetix axis in GSM
  real    :: MagAxisTiltGsm    ! Current  tilt  in GSM
  !$acc declare create(MagAxisTiltGsm, MagAxisGsm_D, MagAxis_D)

  ! Logical tells if the time independent axis parameters have been set
  logical :: DoInitializeAxes = .true.

  ! Coordinate transformation matrices connecting all the systems
  ! The notation follows the convention of contraction of indices
  ! for example GSE -> GSM is done as vGsm_D = matmul(GsmGse_DD, vGse_D)
  ! Most transforms go through GSE, but there are a few extra matrices defined
  real, dimension(3,3) :: &
       SmgGsm_DD,  & ! GSM->SMG
       GeiGeo_DD,  & ! GEO->GEI
       GsmGse_DD,  & ! GSM->GSE
       GseGei_DD,  & ! GEI->GSE
       MagGeo_DD,  & ! GEO->MAG
       HgrHgi_DD,  & ! hgi->hgr
       HgiHgi0_DD, & ! HGI->hgi
       Hgi0Gse_DD, & ! GSE->HGI
       HgiGse_DD,  & ! GSE->hgi
       HgrGse_DD,  & ! GSE->hgr
       HgcHgi_DD,  & ! hgi->hgc
       HgcGse_DD     ! GSE->hgc

  !$acc declare create(SmgGsm_DD, GsmGse_DD, GseGei_DD, GeiGeo_DD, MagGeo_DD)
  !$acc declare create(HgrHgi_DD, HgrGse_DD, HgcHgi_DD, HgcGse_DD)

  ! Remaining coordinate transformation matrices to convert to/from GSE
  real, dimension(3,3) :: &
       SmgGse_DD, GeoGse_DD, MagGse_DD, GseGeo_DD, GseSmg_DD, GeoSmg_DD
  !$acc declare create(SmgGse_DD, GeoGse_DD, MagGse_DD)
  !$acc declare create(GseGeo_DD, GseSmg_DD, GeoSmg_DD)

contains
  !============================================================================
  subroutine init_axes(tStartIn)

    real(Real8_) :: tStartIn

    ! Set the direction and position of the rotation axis in GSE
    ! Set the initial direction and position of the magnetic axis in
    ! GSE, GSM, GEI and GEO systems.
    !
    ! Calculate conversion matrices between MAG-GEO-GEI-GSE systems.

    real :: XyzPlanetHgr_D(3)
    real :: Dipole_D(3)     ! IGRF dipole for Earth
    real :: DipoleStrengthIgrf

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'init_axes'
    !--------------------------------------------------------------------------
    if (.not.DoInitializeAxes) RETURN

    call CON_set_do_test(NameSub, DoTest)
    if(DoTest)write(*,*) NameSub,' starting for iPlanet=', iPlanet

    if(TypeBField == 'NONE') UseRealMagAxis = .false.
    if(DoTest)write(*,*) NameSub, &
         ' UseRealRotAxis, UseSetRotAxis, UseRealMagAxis, UseSetMagAxis=', &
         UseRealRotAxis, UseSetRotAxis, UseRealMagAxis, UseSetMagAxis

    if(UseRealMagAxis .and. .not.UseRealRotAxis) call CON_stop(NameSub// &
         'UseRealMagAxis=T and UseRealRotAxis=F is not allowed')

    tStart = tStartIn

    ! Obtain the orbit elements at start time unless set with #ORBIT
    if(.not.IsOrbitSet) call get_planet_orbit(tStart, Orbit)

    ! Set initial planet position and true HGI-GSE matrix
    call set_hgi_gse_matrix(0.0)

    if(UseRealRotAxis)then
       ! Get rotation axis in HGI
       call get_rotation_axis_hgi(RotAxisHgi0_D)
       ! Keep physical axis orientation independent of optional
       ! HGI longitude offset.
       RotAxis_D = matmul(RotAxisHgi0_D, Hgi0Gse_DD)
       ! Get direction angles in GSE
       call xyz_to_dir(RotAxis_D, RotAxisTheta, RotAxisPhi)
    endif

    ! A negative dLongitudeHgr means align anti-Earth with the -X,Z plane
    ! at the start time, matching the behavior of set_hgi_gse_d_planet.
    if(dLongitudeHgr < 0.0)then
       dLongitudeHgr = modulo( &
            + dLongitudeHgi &
            + atan2(HgiGse_DD(2,1), HgiGse_DD(1,1)) &
            - OmegaCarrington*(tStart - tStartCarringtonCoord), &
            cTwoPi)
       dLongitudeHgrDeg = dLongitudeHgr*cRadToDeg - 360.0
    end if

    if(iPlanet == Earth_ .and. UseRealRotAxis .and. UseRealMagAxis)then
       call igrf_mag_axis(tStart, Dipole_D)

       ! Take the dipole strength with negative magnitude (conventional)
       DipoleStrengthIgrf = -norm2(Dipole_D)

       ! Normalized unit vector (pointing north with negative strength)
       MagAxisGeo_D = Dipole_D/DipoleStrengthIgrf

       ! Calculate magnetic axis angles from the direction vector
       call xyz_to_dir(MagAxisGeo_D, MagAxisThetaGeo, MagAxisPhiGeo)
       ! Copy dipole strength if it is the default
       if(DipoleStrength == DipoleStrengthPlanet_I(Earth_)) &
            DipoleStrength = DipoleStrengthIgrf
       if(DoTest)then
          write(*,*)'IGRF MagAxisThetaGeo, MagAxisPhiGeo=', &
               MagAxisThetaGeo*cRadToDeg, MagAxisPhiGeo*cRadToDeg
          write(*,*)'DipoleStrengthDefault, DipoleStrengthIgrf=', &
               DipoleStrengthPlanet_I(Earth_), DipoleStrength
       end if
    elseif(.not.UseSetRotAxis .and. .not.UseRealRotAxis)then
       ! Rotational axis must be aligned with magnetic axis
       if(UseSetMagAxis)then
          RotAxisTheta = MagAxisTheta
          RotAxisPhi   = MagAxisPhi
          if(DoTest)write(*,*)NameSub,': MagAxisTheta, MagAxisPhi=', &
               MagAxisTheta*cRadToDeg, MagAxisPhi*cRadToDeg
       else
          call CON_stop(NameSub// &
               ' SWMF_ERROR both rotation and magnetic axes'//&
               ' are aligned with the other one?!')
       end if
    end if

    if(DoTest) write(*,*)'RotAxisTheta,RotAxisPhi=',&
         RotAxisTheta*cRadToDeg, RotAxisPhi*cRadToDeg

    ! Using the RotAxisTheta and RotAxisPhi
    ! set the GseGei matrix to convert between GSE and  GEI systems
    call set_gse_gei_matrix

    ! Set initial GEO rotation from IAU W and calibrate it to the requested
    ! subsolar longitude at tStart if #MERIDIAN was provided.
    call set_gei_geo_matrix(0.0)

    ! Calculate initial position for the magnetic axis in GSE and GEI systems
    if(UseRealMagAxis)then
       ! Cartesian coordinates of the magnetic axis unit vector in GEO
       call dir_to_xyz(MagAxisThetaGeo, MagAxisPhiGeo, MagAxis_D)

       if(DoTest)then
          write(*,*)'MagAxisThetaGeo,MagAxisPhiGeo=',&
               MagAxisThetaGeo*cRadToDeg, MagAxisPhiGeo*cRadToDeg
          write(*,*)'MagAxisGeo_D=', MagAxis_D
       end if

       ! GEO --> GEI
       MagAxis0Gei_D = matmul(GeiGeo_DD, MagAxis_D)

       ! GEI --> GSE
       MagAxis_D = matmul(GseGei_DD, MagAxis0Gei_D)

       ! Cartesian vector to spherical direction
       call xyz_to_dir(MagAxis_D, MagAxisTheta, MagAxisPhi)

       if(DoTest)then
          write(*,*)'UseRealMagAxis:'
          write(*,*)'MagAxisGei_D=',MagAxis0Gei_D
          write(*,*)'GseGei_DD='
          call show_rot_matrix(GseGei_DD)
          write(*,*)'MagAxisGse_D=',MagAxis_D
          write(*,*)'MagAxisTheta,MagAxisPhi=',&
               MagAxisTheta*cRadToDeg,MagAxisPhi*cRadToDeg
       end if

    else
       if(.not.UseSetMagAxis)then
          ! Must be aligned with rotational axis
          MagAxisTheta = RotAxisTheta
          MagAxisPhi   = RotAxisPhi
       end if
       ! Convert direction to Cartesian coordinates in GSE
       call dir_to_xyz(MagAxisTheta, MagAxisPhi, MagAxis_D)

       ! Calculate the GEI position too
       ! (in case mag axis is not aligned and rotates)
       MagAxis0Gei_D = matmul(MagAxis_D, GseGei_DD)

       if(DoTest)then
          write(*,*)'Aligned=',.not.UseSetMagAxis,' Set=',UseSetMagAxis
          write(*,*)'MagAxisGei_D=',MagAxis0Gei_D
          write(*,*)'MagAxisGse_D=',MagAxis_D
          write(*,*)'MagAxisTheta,MagAxisPhi=',&
               MagAxisTheta*cRadToDeg,MagAxisPhi*cRadToDeg
       end if

    end if

    ! Obtain the cartesian components of the rotational axis (in GSE)
    call dir_to_xyz(RotAxisTheta, RotAxisPhi, RotAxis_D)

    if(.not.UseRealMagAxis)then
       ! Recalculate the magnetic axis direction in GEO
       MagAxisGeo_D = matmul(MagAxis0Gei_D, GeiGeo_DD)

       call xyz_to_dir(MagAxisGeo_D, MagAxisThetaGeo, MagAxisPhiGeo)

       if(DoTest)write(*,*)'Final MagAxisThetaGeo, MagAxisPhiGeo=',&
            MagAxisThetaGeo*cRadToDeg, MagAxisPhiGeo*cRadToDeg
    else
       ! Set the magnetic direction in Cartesian GEO coordinates
       call dir_to_xyz(MagAxisThetaGeo, MagAxisPhiGeo, MagAxisGeo_D)
    end if

    ! From MagAxisThetaGeo and MagAxisPhiGeo obtain the MAG-GEO matrix
    ! This matrix does not change with simulation time.
    call set_mag_geo_matrix

    ! Set the time dependent axes for the initial time
    call set_axes(0.0, .true.)

    if(DoTest)then
       write(*,*)'Final rotation axis:'
       write(*,*)'RotAxisTheta,RotAxisPhi=',&
            RotAxisTheta*cRadToDeg, RotAxisPhi*cRadToDeg
       write(*,*)'RotAxisGse_D =', RotAxis_D
       write(*,*)'RotAxisGsm_D =', RotAxisGsm_D
       write(*,*)'GsmGse_DD='
       call show_rot_matrix(GsmGse_DD)
       if(sum(XyzPlanetHgi_D**2) > 1e-6)then
          XyzPlanetHgr_D = matmul(HgrHgi_DD, XyzPlanetHgi_D)
          write(*,*)'dLongitudeHgr,dLongitudeHgi=', &
               dLongitudeHgrDeg, dLongitudeHgiDeg
          write(*,*)'XyzPlanetHgi_D/rSun = ', XyzPlanetHgi_D/rSun
          write(*,*)'XyzPlanetHgr_D/rSun = ', XyzPlanetHgr_D/rSun
          write(*,*)'r/AU,HG_lat,HGR_lon,HGI_lon=', &
               PlanetDistance/cAU, &
               asin(XyzPlanetHgi_D(3)/norm2(XyzPlanetHgi_D))*cRadToDeg, &
               atan2_check(XyzPlanetHgr_D(2), XyzPlanetHgr_D(1))*cRadToDeg, &
               atan2_check(XyzPlanetHgi_D(2), XyzPlanetHgi_D(1))*cRadToDeg
          write(*,*)'vPlanetHgi_D/(km/s) = ', vPlanetHgi_D/1000.0
          write(*,*)'HgiGse_DD='
          call show_rot_matrix(HgiGse_DD)
       end if
    end if

    DoInitializeAxes=.false.

    !$acc update device(tStart)
  contains
    !==========================================================================
    subroutine set_mag_geo_matrix

      ! The first rotation is around the Z_GEO axis with MagAxisPhiGeo,
      ! which rotates Y_GEO into Y_MAG.
      ! The second rotation is around the Y_MAG axis with MagAxisThetaGeo,
      ! which rotates Z_GEO into Z_MAG.
      !
      ! This matrix only changes with the slow motion of the magnetix axis
      ! relative to the Earth.
      !------------------------------------------------------------------------
      MagGeo_DD = matmul( &
           rot_matrix_y(-MagAxisThetaGeo), &
           rot_matrix_z(-MagAxisPhiGeo))

    end subroutine set_mag_geo_matrix
    !==========================================================================
  end subroutine init_axes
  !============================================================================
  subroutine set_hgi_gse_matrix(TimeSim)

    ! Set true and rotated HgiGse matrices and
    ! the planet location/velocity in (rotated) HGI

    real, intent(in) :: TimeSim

    real :: GseX_D(3), GseZ_D(3)

    character(len=*), parameter:: NameSub = 'set_hgi_gse_matrix'
    !--------------------------------------------------------------------------
    call orbit_in_hgi(TimeSim, XyzPlanetHgi_D, vPlanetHgi_D)
    PlanetDistance = norm2(XyzPlanetHgi_D)

    ! GSE coordinate unit vectors in HGI
    GseX_D = -XyzPlanetHgi_D/PlanetDistance
    GseZ_D = cross_product(XyzPlanetHgi_D, vPlanetHgi_D) ! orbit normal
    GseZ_D = GseZ_D/norm2(GseZ_D)
    ! GSE->HGI
    Hgi0Gse_DD(:,x_) = GseX_D
    Hgi0Gse_DD(:,y_) = cross_product(GseZ_D, GseX_D)
    Hgi0Gse_DD(:,z_) = GseZ_D
    HgiGse_DD = Hgi0Gse_DD

    if(dLongitudeHgi < 0.0)then
       ! Find the longitude of the planet and set HGI rotation
       ! This is only done once when TimeSim == 0.0
       if(TimeSim /= 0.0) call CON_stop(NameSub//': TimeSim should be 0')
       dLongitudeHgi = modulo(atan2(HgiGse_DD(2,1), HgiGse_DD(1,1)), cTwoPi)
       dLongitudeHgiDeg = dLongitudeHgi*cRadToDeg - 360.0
    end if
    if(dLongitudeHgi > 0.0)then
       ! HGI->hgi matrix
       if(TimeSim == 0.0) HgiHgi0_DD = rot_matrix_z(-dLongitudeHgi)
       ! Rotate the true HGI system to rotated lower case hgi
       HgiGse_DD      = matmul(HgiHgi0_DD, Hgi0Gse_DD)
       XyzPlanetHgi_D = matmul(HgiHgi0_DD, XyzPlanetHgi_D)
       vPlanetHgi_D   = matmul(HgiHgi0_DD, vPlanetHgi_D)
    end if

  end subroutine set_hgi_gse_matrix
  !============================================================================
  subroutine set_gei_geo_matrix(TimeSim)

    ! The rotation is around the Z axis, which is the rotational axis
    ! This matrix changes due to the rotation of the body

    real, intent(in) :: TimeSim
    !--------------------------------------------------------------------------
    if(AngleGeiGeoStart < 0.0)then
       if(MeridianGeo >= 0.0)then
          ! Get the subsolar point in GEI at tStart (GseGei_DD is already set)
          ! XyzSubsolarGei = (1,0,0).GseGei_DD = GseGei_DD(x_,:)
          call xyz_to_lonlat(GseGei_DD(x_,:), LonSubsolar, LatSubsolar)
          AngleGeiGeoStart = modulo(LonSubsolar - MeridianGeo, cTwoPi)
       else
          ! Calculate the initial angle between GEI and GEO X axes
          call get_gei_geo_angle(AngleGeiGeoStart)
       end if
    end if
    GeiGeo_DD = rot_matrix_z(AngleGeiGeoStart + TimeSim*OmegaPlanet)

  end subroutine set_gei_geo_matrix
  !============================================================================
  subroutine set_axes(TimeSim, DoSetAxes)

    real,              intent(in) :: TimeSim
    logical, optional, intent(in) :: DoSetAxes

    ! The magnetic axis as well as the corotating GEO and MAG frames
    ! are rotating around the rotational axis with OmegaPlanet
    ! angular speed. Calculate the position of the axes and the
    ! transformation matrices for the given simulation time TimeSim.
    !
    ! When the optional DoSetAxes argument is present (its value is ignored),
    ! the magnetic axis and the related variables are always set.
    ! This is needed for the initial setting.
    !
    ! Otherwise the update is based on a number of parameters.
    !
    ! If the planet does not rotate, or the magnetic axis is aligned with
    ! the rotation axis, no calculation is performed.
    !
    ! The last simulation time with which an update was done
    ! is stored into TimeSimLast.
    !
    ! If DoUpdateB0 == .false. the magnetic axis is taken to be fixed.
    ! If DtUpdateB0 <= 0.001 (sec) the magnetic axis is updated
    !      if TimeSim differs from TimeSimLast.
    ! If DtUpdateB0 >  0.001 (sec) then the magnetic axis is updated
    !      if int(TimeSim/DtUpdateB0) differs from int(TimeSimLast/DtUpdateB0)
    !

    real :: MagAxisGei_D(3)

    real :: TimeSimLast = -1000.0  ! Last simulation time for any update
    real :: Angle

    ! Reset the helio-centered coordinate transformations if time changed
    logical:: DoTest
    character(len=*), parameter:: NameSub = 'set_axes'
    !--------------------------------------------------------------------------
    if(TimeSim == TimeSimLast .and. .not.present(DoSetAxes)) RETURN
    TimeSimLast = TimeSim

    call CON_set_do_test(NameSub, DoTest)

    if(DoTest)then
       write(*,*) NameSub,'UseAlignedAxes,UseRotation,DoUpdateB0=', &
            UseAlignedAxes, UseRotation, DoUpdateB0
       write(*,*) NameSub,'DtUpdateB0,TimeSim=', &
            DtUpdateB0, TimeSim
    end if

    call set_hgi_gse_matrix(TimeSim)

    if(UseRealRotAxis)then
       ! Physical axis orientation is based on true HGI
       RotAxis_D = matmul(RotAxisHgi0_D, Hgi0Gse_DD)
       ! Get direction angles in GSE
       call xyz_to_dir(RotAxis_D, RotAxisTheta, RotAxisPhi)
       ! Recalculate GSE-GEI matrix
       call set_gse_gei_matrix
    end if

    ! Recalculate the HgrHgi_DD matrix
    ! The negative sign in front of OmegaCarrington comes from that
    ! this matrix transforms from HGI to HGR, so a point at rest
    ! in HGI rotates BACKWARDS in HGR
    Angle = modulo( &
         -OmegaCarrington*(TimeSim + tStart - tStartCarringtonCoord), &
         cTwoPi8)

    ! Modify angle by the offsets
    Angle = Angle + dLongitudeHgi - dLongitudeHgr

    HgrHgi_DD = rot_matrix_z(Angle)

    ! Calculate the HgrGse_DD matrix
    HgrGse_DD = matmul(HgrHgi_DD, HgiGse_DD)

    ! Recalculate the HgcHgi and HgcGse matrixes
    Angle     = -OmegaCarrington*TimeSim
    HgcHgi_DD = rot_matrix_z(Angle)
    HgcGse_DD = matmul(HgcHgi_DD, HgiGse_DD)

    ! Rotate MagAxis0Gei around Z axis to get current position in GEI
    MagAxisGei_D = matmul(rot_matrix_z(OmegaPlanet*TimeSim), MagAxis0Gei_D)

    ! Transform from GEI to GSE
    MagAxis_D = matmul(GseGei_DD, MagAxisGei_D)

    ! Set the angles in GSE
    call xyz_to_dir(MagAxis_D, MagAxisTheta, MagAxisPhi)

    ! Set the transformation matrices

    ! Calculate the rotation matrix to convert between GSE and GSM systems.
    ! This is a rotation around the shared X axis with the angle between the
    ! Z_GSE axis and the magnetic axis projected onto the Y-Z plane.
    !
    ! This matrix changes with simulation time unless
    !    UseRotation=.false. or UseAlignedAxes=.true.
    GsmGse_DD = rot_matrix_x(atan2_check(MagAxis_D(y_), MagAxis_D(z_)))

    ! Calculate the rotation matrix to convert between SMG and GSM systems.
    ! This is a rotation around the Y axis with the magnetic tilt_GSM,
    ! which is -asin(MagAxis_D(x_)
    !
    ! This matrix changes with simulation time unless
    !    UseRotation=.false. or UseAlignedAxes=.true.
    MagAxisTiltGsm = -asin(MagAxis_D(x_))
    SmgGsm_DD = rot_matrix_y(MagAxisTiltGsm)

    ! SMG-GSE transformation matrix
    SmgGse_DD = matmul(SmgGsm_DD, GsmGse_DD)
    GseSmg_DD = transpose(SmgGse_DD)

    ! Calculate GSM coordinates and tilt of the magnetic axis.
    ! and calculate the rotation axis in GSM coordinates.
    ! These are useful to obtain the dipole field and the corotation velocity
    ! in the GSM system.
    MagAxisGsm_D = matmul(GsmGse_DD, MagAxis_D)
    RotAxisGsm_D = matmul(GsmGse_DD, RotAxis_D)

    ! Now calculate the transformation matrices for the rotating systems
    call set_gei_geo_matrix(TimeSim)

    GseGeo_DD = matmul(GseGei_DD, GeiGeo_DD)
    GeoGse_DD = transpose(GseGeo_DD)
    MagGse_DD = matmul(MagGeo_DD, GeoGse_DD)
    GeoSmg_DD = matmul(GeoGse_DD, GseSmg_DD)

    ! Get Subsolar point in GEO
    call xyz_to_lonlat(GeoGse_DD(:,x_), LonSubsolar, LatSubsolar)

    if(DoTest)then
       write(*,*)NameSub,' Lon,LatSubsolar   =', &
            LonSubsolar*cRadToDeg, LatSubsolar*cRadToDeg
       write(*,*)NameSub,' new MagAxis_D     =', MagAxis_D
       write(*,*)NameSub,' new MagAxisTiltGsm=', MagAxisTiltGsm*cRadToDeg
       write(*,*)NameSub,' new RotAxisGsm_D  =', RotAxisGsm_D
    end if

    !$acc update device(RotAxis_D, RotAxisGsm_D)
    !$acc update device(MagAxisTiltGsm, MagAxisGsm_D, MagAxis_D)
    !$acc update device(SmgGsm_DD, GsmGse_DD, GseGei_DD, GeiGeo_DD, MagGeo_DD)
    !$acc update device(HgrHgi_DD, HgrGse_DD, HgcHgi_DD, HgcGse_DD)
    !$acc update device(SmgGse_DD, GeoGse_DD, MagGse_DD)
    !$acc update device(GseGeo_DD, GseSmg_DD, GeoSmg_DD)

  end subroutine set_axes
  !============================================================================
  subroutine get_axes(TimeSim, &
       MagAxisTiltGsmOut, RotAxisGsmOut_D, RotAxisGseOut_D, &
       MagAxisGseOut_D, MagAxisGsmOut_D)

    real, intent(in) :: TimeSim
    real, intent(out), optional :: MagAxisTiltGsmOut
    real, intent(out), optional :: RotAxisGsmOut_D(3)
    real, intent(out), optional :: RotAxisGseOut_D(3)
    real, intent(out), optional :: MagAxisGsmOut_D(3)
    real, intent(out), optional :: MagAxisGseOut_D(3)

    ! Provides various information about the rotation and magnetic axes
    ! through the optional output arguments.
    character(len=*), parameter:: NameSub = 'get_axes'
    !--------------------------------------------------------------------------
    ! Set time independent information
    if(DoInitializeAxes)&
         call CON_stop(NameSub//' ERROR: init_axes has not been called')

    ! Set time dependent information (TimeSim is cashed)
    call set_axes(TimeSim)

    if (present(MagAxisTiltGsmOut)) MagAxisTiltGsmOut = MagAxisTiltGsm
    if (present(RotAxisGsmOut_D))   RotAxisGsmOut_D   = RotAxisGsm_D
    if (present(RotAxisGseOut_D))   RotAxisGseOut_D   = RotAxis_D
    if (present(MagAxisGsmOut_D))   MagAxisGsmOut_D   = MagAxisGsm_D
    if (present(MagAxisGseOut_D))   MagAxisGseOut_D   = MagAxis_D

  end subroutine get_axes
  !============================================================================
  subroutine set_gse_gei_matrix

    ! The GseGei_DD matrix converts between GSE and GEI with two rotations:
    !
    !   rotate around X_GEI with RotAxisTheta      so that Z_GEI->Z_GSE
    !   rotate around Z_GSE with RotAxisPhi + Pi/2 so that X_GEI->X_GSE
    !
    ! The GseGei_DD matrix changes at the order of TimeSimulation/TimeOrbit.
    ! For usual simulations that change can be safely neglected.
    !--------------------------------------------------------------------------
    GseGei_DD = matmul(&
         rot_matrix_z(RotAxisPhi + cHalfPi), &
         rot_matrix_x(RotAxisTheta) &
         )

  end subroutine set_gse_gei_matrix
  !============================================================================
  function transform_matrix(TimeSim, TypeCoordIn, TypeCoordOut) result(Rot_DD)

    real,             intent(in) :: TimeSim      ! Simulation time
    character(len=*), intent(in) :: TypeCoordIn  ! Type of input coord. system
    character(len=*), intent(in) :: TypeCoordOut ! Type of output coord. system

    !RETURN VALUE:
    real :: Rot_DD(3,3)

    ! Calculate the transformation matrix between two coordinate systems.
    ! One should store the transformation matrix and reuse it, because
    ! this general routine is not very efficient. Typical usage:
    ! \begin{verbatim}
    ! real :: IeUa_DD(3,3)
    ! ! Obtain the transformation matrix for the current time
    ! IeUa_DD = transform_matrix(TimeSimulation,'GEO','SMG')
    ! ! transform vectors in UA (GEO system) to IE (SMG system):
    ! VecIe_D = matmul(IeUa_DD,VecUa_D)
    ! ...
    ! \end{verbatim}

    real :: InGse_DD(3,3), OutGse_DD(3,3)
    character(len=*), parameter:: NameSub = 'transform_matrix'
    !--------------------------------------------------------------------------
    if(TypeCoordIn == TypeCoordOut .or. &
         TypeCoordIn == 'SYS' .or. TypeCoordOut == 'SYS')then
       Rot_DD = cUnit_DD
       RETURN
    end if

    ! Set time dependent information
    call set_axes(TimeSim)

    select case(TypeCoordIn)
    case('GSE')
       InGse_DD = cUnit_DD
    case('GSM')
       InGse_DD = GsmGse_DD
    case('SMG')
       InGse_DD = SmgGse_DD
    case('MAG')
       InGse_DD = MagGse_DD
    case('GEO')
       InGse_DD = GeoGse_DD
    case('GEI')
       InGse_DD = transpose(GseGei_DD)
    case('HGI', 'hgi')
       InGse_DD = HgiGse_DD  ! hgi is the rotated HGI
    case('HGC', 'hgc')
       InGse_DD = HgcGse_DD
    case('HGR', 'hgr')
       InGse_DD = HgrGse_DD
    case default
       call CON_stop(NameSub//' unknown TypeCoordIn='//TypeCoordIn)
    end select

    select case(TypeCoordOut)
    case('GSE')
       OutGse_DD = cUnit_DD
    case('GSM')
       OutGse_DD = GsmGse_DD
    case('SMG')
       OutGse_DD = SmgGse_DD
    case('MAG')
       OutGse_DD = MagGse_DD
    case('GEO')
       OutGse_DD = GeoGse_DD
    case('GEI')
       OutGse_DD = transpose(GseGei_DD)
    case('HGI', 'hgi')
       OutGse_DD = HgiGse_DD
    case('HGC', 'hgc')
       OutGse_DD = HgcGse_DD
    case('HGR', 'hgr')
       OutGse_DD = HgrGse_DD
    case default
       call CON_stop(NameSub//' unknown TypeCoordOut='//TypeCoordOut)
    end select

    Rot_DD = matmul(OutGse_DD, transpose(InGse_DD))

    ! Uppercase HGI/HGC/HGR refer to the original frames, so undo the offsets
    if(dLongitudeHgi /= 0.0)then
       if(TypeCoordIn == 'HGI' .or. TypeCoordIn == 'HGC') &
            Rot_DD = matmul(Rot_DD, rot_matrix_z(-dLongitudeHgi))

       if(TypeCoordOut == 'HGI' .or. TypeCoordOut == 'HGC') &
            Rot_DD = matmul(rot_matrix_z(dLongitudeHgi), Rot_DD)
    end if

    if(dLongitudeHgr /= 0.0)then
       if(TypeCoordIn == 'HGR') then
          Rot_DD = matmul(Rot_DD, rot_matrix_z(-dLongitudeHgr))
       elseif(TypeCoordOut == 'HGR') then
          Rot_DD = matmul(rot_matrix_z(dLongitudeHgr), Rot_DD)
       end if
    end if

  end function transform_matrix
  !============================================================================
  function angular_velocity(TimeSim, NameCoord1, NameCoord2In, iFrame) &
       result(Omega_D)

    real,                       intent(in) :: TimeSim      ! Simulation time
    character(len=*),           intent(in) :: NameCoord1   ! 1st coord. system
    character(len=*), optional, intent(in) :: NameCoord2In ! 2nd coord. system
    integer, optional,intent(in) :: iFrame                 ! Frame for result

    !RETURN VALUE:
    real :: Omega_D(3) ! Angular velocity components

    ! This subroutine calculates the angular velocity vector between
    ! two coordinate systems from the transformation matrix between them.
    ! If the second frame is not present in the argument list, the result is
    ! the angular velocity of the first frame relative to an inertial frame.
    ! The angular velocity is given in the moving frame.
    ! When both frames are given, the relative angular rotation is returned.
    ! If iFrame is presemt it defines whether the output angular velocity
    ! is with respect to the first (iFrame=1) or second (iFrame=2) system.
    ! If the iFrame argument is not present, the result is in the first frame.
    ! This means that for example angular\_velocity(t,'GEO') is the same as
    ! angular\_velocity(t,'GEI','GEO',2) which gives the rotation of the
    ! Earth relative to an inertial frame expressed in GEO coordinates.
    ! On the other hand angular\_velocity(t,'GEI','GEO') is the same
    ! as angular\_velocity(t,'GEI','GEO',1) which gives the opposite
    ! (negative) sign for the angular velocity.

    ! Local variables
    character (len=3) :: NameCoord2
    integer ::  iFrameOut
    real    ::  dTimeSim
    real, dimension(3,3) :: Rot_DD, RotMid_DD, RotPlus_DD, RotMinus_DD, dRot_DD

    ! Check optional arguments and set defaults
    character(len=*), parameter:: NameSub = 'angular_velocity'
    !--------------------------------------------------------------------------
    if(present(NameCoord2In))then
       NameCoord2 = NameCoord2In
       if(present(iFrame))then
          if(iFrame /= 1 .and. iFrame /=2)then
             write(*,*) NameSub, ' ERROR iFrame = ',iFrame
             call CON_stop(NameSub // ': invalid value for iFrame = 1 or 2')
          end if
          iFrameOut = iFrame
       else
          ! Default is to provide Omega_D in the output coord. system
          iFrameOut = 1
       end if
    else
       if(NameCoord1(1:1) == 'H' .or. NameCoord1(1:1) == 'h')then
          ! For heliocentric coordinate systems set the inertial frame to HGI
          NameCoord2 = 'hgi'
       else
          ! For planetocentric systems GEI is inertial frame
          NameCoord2 = 'GEI'
       end if
       iFrameOut = 1
    end if

    ! Determine the perturbation of time
    if(precision(TimeSim) >= 12) then
       dTimeSim = max(1.0, 1e-10*TimeSim)
    else
       dTimeSim = max(1000.0, 1e-4*TimeSim)
    end if

    if(NameCoord1 == NameCoord2)then
       ! Nothing to do
       Omega_D = 0.0
       RETURN
    end if

    RotMinus_DD = transform_matrix(TimeSim-dTimeSim, NameCoord1, NameCoord2)
    RotPlus_DD  = transform_matrix(TimeSim+dTimeSim, NameCoord1, NameCoord2)
    dRot_DD = (RotPlus_DD-RotMinus_DD)/(2*dTimeSim)

    RotMid_DD = transform_matrix(TimeSim, NameCoord1, NameCoord2)
    Rot_DD  = matmul(transpose(RotMid_DD), dRot_DD)

    Omega_D = [ Rot_DD(2,3), Rot_DD(3,1), Rot_DD(1,2) ]

    !    write(*,*)'NameCoord1,2=',NameCoord1,NameCoord2
    !    write(*,*)'RotPlus ='; call show_rot_matrix(RotPlus_DD)
    !    write(*,*)'RotMinus='; call show_rot_matrix(RotMinus_DD)
    !    write(*,*)'dRot    ='; call show_rot_matrix(dRot_DD)
    !    write(*,*)'Rot     ='; call show_rot_matrix(Rot_DD)
    !    write(*,*)'Omega_D =', Omega_D

    ! Change sign if called with one coordinate system
    if(.not.present(NameCoord2In)) Omega_D = - Omega_D

    ! Transform into frame 2 if required
    if(iFrameOut == 2) Omega_D = matmul(RotMid_DD, Omega_D)

    where(abs(Omega_D) < 1e-12) Omega_D = 0.00

  end function angular_velocity
  !============================================================================
  function transform_velocity(TimeSim, v1_D, Xyz1_D, &
       NameCoord1, NameCoord2) result(v2_D)

    real,             intent(in) :: TimeSim       ! Simulation time
    real,             intent(in) :: v1_D(3)       ! Velocity in 1st system
    real,             intent(in) :: Xyz1_D(3)     ! Position in 1st system
    character(len=3), intent(in) :: NameCoord1    ! Name of 1st coord. system
    character(len=3), intent(in) :: NameCoord2    ! Name of 2nd coord. system

    !RETURN VALUE:
    real :: v2_D(3)                               ! v2 components

    ! This function transforms the velocity vector from one coordinate system
    ! to another. The input position and velocity should be in SI units and
    ! the output velocity vector is also in SI units.
    ! If the two systems have the same name, then the input and output
    ! velocity vectors are the same.

    ! Local variables
    character (len=3) :: NameCoord1Last = 'XXX', NameCoord2Last = 'XXX'
    real :: TimeSimLast = -1.0

    real, dimension(3)   :: v1Total_D, Omega12_D
    real, dimension(3,3) :: Transform21_DD
    real, dimension(3)   :: XyzPlanet1_D, Xyz2_D, vPlanet1_D, &
         Omega1_D, Omega2_D
    logical :: IsHelioGeo = .false.

    character(len=*), parameter:: NameSub = 'transform_velocity'
    !--------------------------------------------------------------------------
    if(NameCoord1 == NameCoord2)then
       ! If NameCoord1 is the same as NameCoord2 there is no transformation.
       v2_D = v1_D
       RETURN
    end if

    if(.not.(TimeSim == TimeSimLast .and. NameCoord1 == NameCoord1Last &
         .and. NameCoord1 == NameCoord2Last) )then

       ! Store current time and coordinate system names
       TimeSimLast = TimeSim
       NameCoord1Last = NameCoord1
       NameCoord2Last = NameCoord2

       ! Get transformation matrix and angular velocity between frames
       Transform21_DD = transform_matrix(TimeSim, NameCoord1, NameCoord2)

       if(  (NameCoord1(1:1) == 'H' .or. NameCoord1(1:1) == 'h') .eqv. &
            (NameCoord2(1:1) == 'H' .or. NameCoord2(1:1) == 'h')       ) then
          ! Both helio-centric or both planet-centric, no planet speed added
          IsHelioGeo = .false.
          Omega12_D  = angular_velocity(TimeSim, NameCoord1, NameCoord2)
       else
          IsHelioGeo = .true.
          Omega1_D   = angular_velocity(TimeSim, NameCoord1)
          Omega2_D   = angular_velocity(TimeSim, NameCoord2)

          ! Position of the planet in frame 1
          XyzPlanet1_D = matmul(&
               transform_matrix(TimeSim, 'hgi', NameCoord1), XyzPlanetHgi_D)

          ! Speed of the planet in frame 1
          vPlanet1_D = matmul(&
               transform_matrix(TimeSim, 'hgi', NameCoord1), vPlanetHgi_D)

          ! Planet-centric --> Helio-centric
          if(NameCoord2(1:1) == 'H' .or. NameCoord2(1:1) == 'h')then
             ! subtract planet speed and flip planet position
             XyzPlanet1_D = -XyzPlanet1_D
             vPlanet1_D   = -vPlanet1_D
          end if

       end if
    end if

    if(IsHelioGeo)then
       ! Velocity with respect to the inertial frame comoving with Frame 1
       ! and momentarily aligned with Frame 1.

       v1Total_D = v1_D + cross_product(Omega1_D, Xyz1_D)

       ! Position relative to Frame2
       Xyz2_D = matmul( Transform21_DD, (Xyz1_D - XyzPlanet1_D) )

       ! Transform into Frame2 and subtract rotation speed of
       ! Frame2 with respect to the inertial frame, and add
       ! relative velocity of Frame2 with respect to Frame1 (vPlanet1)

       v2_D = matmul(Transform21_DD, v1Total_D - vPlanet1_D) &
            - cross_product(Omega2_D, Xyz2_D)

    else
       ! Omega12_D defines the rotation of Frame2 with respect to Frame1,
       ! so a point at rest in Frame1 should rotate with -Omega12 in Frame2

       v1Total_D = v1_D - cross_product(Omega12_D, Xyz1_D)

       ! Transform total velocity to Frame2
       v2_D = matmul(Transform21_DD, v1Total_D)

    end if

  end function transform_velocity
  !============================================================================
  subroutine test_axes

    ! Do some self consistency checks. Stop with an error message if
    ! test fails. Otherwise write out success.
    real:: MagAxisTilt
    real:: RotAxisGsm_D(3), RotAxisGeo_D(3), Rot_DD(3,3), Result_DD(3,3)
    real:: Omega_D(3), v2_D(3), Result_D(3), Position_D(3)
    real:: Epsilon1, Epsilon2, Epsilon3
    type(TimeType):: TimeStart
    !--------------------------------------------------------------------------
    if(precision(1.0) >= 12)then
       Epsilon1 = 1e-10
       Epsilon2 = 1e-1
       Epsilon3 = 1e-3
    else
       Epsilon1 = 1e-5
       Epsilon2 = 1e+2
       Epsilon3 = 1e+0
    endif

    if(.not.DoInitializeAxes) write(*,*)'test failed: DoInitializeAxes=',&
         DoInitializeAxes,' should be true'

    write(*,'(a)')'Testing init_axes'
    dLongitudeHgi = -1.0
    dLongitudeHgr = 0.0

    ! This happens to be the equinox time in 2000
    TimeStart = TimeType(2000, 3, 20, 7, 35, 0, 0.0, 0.0_Real8_, '')
    call time_int_to_real(TimeStart)
    call init_axes(TimeStart % Time)

    if(tStart /= TimeStart % Time)write(*,*)'test init_axes failed: ',&
         'tStart=',tStart,' should be equal to TimeStart % Time=',&
         TimeStart % Time

    if(DoInitializeAxes) write(*,*)'test init_axes failed: DoInitializeAxes=',&
         DoInitializeAxes,' should be fales'

    write(*,'(a)')'Testing get_axes'

    call get_axes(0.0, MagAxisTilt, RotAxisGsm_D)

    if(abs(MagAxisTilt*cRadToDeg - 7.981905804308643) > 0.00001)write(*,*) &
         'test get_axes failed: MagAxisTilt =',MagAxisTilt*cRadToDeg,&
         ' should be 7.981905804308643 degrees within round off error'

    Result_D = [7.23514970498717E-05, 0.11768330549083, 0.99305117409628]
    if(maxval(abs(RotAxisGsm_D - Result_D)) > 0.00001) &
         write(*,*) 'test get_axes failed: RotAxisGsm_D =',&
         RotAxisGsm_D,' should be equal to ',Result_D, &
         ' within round off errors'

    write(*,'(a)')'Testing transform_matrix'

    Rot_DD = transform_matrix(0.0, 'GSM', 'GEO')
    RotAxisGeo_D = matmul(Rot_DD, RotAxisGsm_D)

    Result_D = [0.0, 0.0, 1.0]
    if(maxval(abs(RotAxisGeo_D - Result_D)) > 0.0001) &
         write(*,*)'test transform_matrix failed: RotAxisGeo_D=',&
         RotAxisGeo_D,' should be be equal to ',Result_D,&
         ' within round off errors'

    Rot_DD = transform_matrix(10.0, 'HGI', 'HGC')
    Result_DD = rot_matrix_z(-10.0*OmegaCarrington)
    if(maxval(abs(Rot_DD - Result_DD)) > Epsilon1) then
       write(*,*)'test transform_matrix failed: HGI->HGC matrix is'
       call show_rot_matrix(Rot_DD)
       write(*,*)'instead of'
       call show_rot_matrix(Result_DD)
    end if

    write(*,'(a)')'Testing show_rot_matrix'
    write(*,'(a)')'HgiGse_DD(0)='; call show_rot_matrix(HgiGse_DD)

    write(*,'(a)')'Testing angular_velocity'

    ! HGI is an inertial system
    Omega_D  = angular_velocity(0.0, 'HGI')
    Result_D = [0., 0., 0.]
    if(maxval(abs(Omega_D - Result_D)) > Epsilon1) &
         write(*,*)'test angular_velocity failed: HGI Omega_D = ',Omega_D,&
         ' should be equal to ',Result_D,' within round off errors'

    ! HGR rotates around its Z axis with the OmegaCarrington
    Omega_D  = angular_velocity(0.0, 'HGR')
    Result_D = [0., 0., OmegaCarrington]
    if(maxval(abs(Omega_D - Result_D)) > Epsilon1) &
         write(*,*)'test angular_velocity failed: HGR Omega_D = ',Omega_D,&
         ' should be equal to ',Result_D,' within round off errors'

    ! HGC rotates around its Z axis with the OmegaCarrington
    Omega_D  = angular_velocity(0.0, 'HGC')
    Result_D = [0., 0., OmegaCarrington]
    if(maxval(abs(Omega_D - Result_D)) > Epsilon1) &
         write(*,*)'test angular_velocity failed: HGC Omega_D = ',Omega_D,&
         ' should be equal to ',Result_D,' within round off errors'

    ! GEI is an inertial system
    Omega_D  = angular_velocity(0.0, 'GEI')
    Result_D = [0., 0., 0.]
    if(maxval(abs(Omega_D - Result_D)) > Epsilon1) &
         write(*,*)'test angular_velocity failed: GEI Omega_D = ',Omega_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! GEO rotates with OmegaPlanet around the Z axis with respect to inertial
    Omega_D  = angular_velocity(0.0, 'GEO')
    Result_D = [0., 0., OmegaPlanet]
    if(maxval(abs(Omega_D - Result_D)) > Epsilon1) &
         write(*,*)'test angular_velocity failed: GEO Omega_D = ',Omega_D,&
         ' should be equal to ',Result_D,' within round off errors'

    ! GEO rotates with OmegaPlanet around the Z axis with respect to GEI
    Omega_D  = angular_velocity(0.0, 'GEI', 'GEO', iFrame=2)
    Result_D = [0., 0., OmegaPlanet]
    if(maxval(abs(Omega_D - Result_D)) > Epsilon1) &
         write(*,*)'test angular_velocity failed: GSE,GEO Omega_D = ',Omega_D,&
         ' should be equal to ',Result_D,' within round off errors'

    ! The GSM rotates around the X axis. At 07:35 UT in the morning
    ! the Northern magnetic pole is on the night side,
    ! so the northern magnetic pole moves towards -Y in GSE,
    ! so GSM rotates with a positive sign around the X axis.
    ! The sign is right, the amplitude is reasonable.

    Omega_D  = angular_velocity(0.0, 'GSE', 'GSM', iFrame=2)
    Result_D = [1.0132187783448925E-05, 0., 0.]
    if(maxval(abs(Omega_D - Result_D)) > Epsilon1) &
         write(*,*)'test angular_velocity failed: GSM Omega_D = ',Omega_D,&
         ' should be equal to ',Result_D,' within round off errors'

    ! This is a general case, we believe the numbers
    Omega_D  = angular_velocity(0.0, 'GSE', 'SMG', iFrame=2)
    Result_D = [1.0034026850411211E-05, 8.6392040399372235E-06, &
         -1.4069592837587461E-06]
    if(maxval(abs(Omega_D - Result_D)) > Epsilon1) &
         write(*,*)'test angular_velocity failed: GSE-SMG Omega_D in SMG= ',&
         Omega_D,' should be equal to ',Result_D,' within round off errors'

    write(*,'(a)')'Testing transform_velocity'

    ! Let's take the [0.,0.,cAU] point with 0 velocity in HGR.
    ! This will correspond to the point matmul([cAU,0.,0.], HgrHgi_DD) in HGI
    ! and it should rotate with [0.,0.,OmegaCarrington] in HGI.

    Position_D = [cAU,0.,0.]
    v2_D = transform_velocity(0., [0.,0.,0.], Position_D, 'hgr', 'hgi')
    Position_D = matmul(Position_D, HgrHgi_DD)
    Result_D = cross_product( [0.,0.,OmegaCarrington], Position_D)

    if(maxval(abs(v2_D - Result_D)) > Epsilon2) &
         write(*,*)'test transform_velocity failed: hgi-hgr v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Let's transform back, the result should be 0
    v2_D = transform_velocity(0., Result_D, Position_D, 'hgi', 'hgr')
    Result_D = [ 0., 0., 0.]
    if(maxval(abs(v2_D - Result_D)) > Epsilon2) &
         write(*,*)'test transform_velocity failed: hgr-hgi v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Let's check vPlanet. A point at rest in HGI should move towards the
    ! +Y axis in GSE (opposite of the motion of the planet)
    ! with roughly 30 km/s for Earth. In March the Earth
    ! is getting farther away from the Sun, so the X component of the
    ! velocity should be a small positive number.
    v2_D = transform_velocity(0., [0., 0., 0.], XyzPlanetHgi_D, 'hgi', 'GSE')
    Result_D = [4.8518332411364236E+02, 2.9900370812848141E+04, 0.]
    if(maxval(abs(v2_D - Result_D)) > Epsilon2) &
         write(*,*)'test transform_velocity failed: hgi-GSE v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Let's transform back, the result should be 0
    v2_D = transform_velocity(0., v2_D, [0., 0., 0.], 'GSE', 'hgi')
    Result_D = [ 0., 0., 0.]
    if(maxval(abs(v2_D - Result_D)) > Epsilon3) &
         write(*,*)'test transform_velocity failed: GSE-hgi back v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Velocity of Earth in GEO should be zero
    v2_D = transform_velocity(0., vPlanetHgi_D, XyzPlanetHgi_D, 'hgi', 'GEO')
    Result_D = [ 0., 0., 0.]
    if(maxval(abs(v2_D - Result_D)) > Epsilon3) &
         write(*,*)'test transform_velocity failed: HGI-GEO v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Velocity of Earth in HGI should be vPlanetHgi_D
    v2_D = transform_velocity(0., [0., 0., 0.], [0., 0., 0.], 'GEO', 'hgi')
    Result_D = vPlanetHgi_D
    if(maxval(abs(v2_D - Result_D)) > Epsilon3) &
         write(*,*)'test transform_velocity failed: GEO-hgi v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Velocity of Earth in HGR should be
    ! HgrHgi_DD.(vPlanetHgi_D - OmegaCarrington x XyzPlanetHgi_D)
    v2_D = transform_velocity(0., [0., 0., 0.], [0., 0., 0.], 'GEO', 'hgr')
    Result_D = matmul(HgrHgi_DD, vPlanetHgi_D &
         - cross_product([0.,0.,OmegaCarrington], XyzPlanetHgi_D))
    if(maxval(abs(v2_D - Result_D)) > Epsilon2) &
         write(*,*)'test transform_velocity failed: GEO-hgr v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Go back
    Position_D = matmul(HgrHgi_DD, XyzPlanetHgi_D)
    v2_D = transform_velocity(0., Result_D, Position_D, 'hgr', 'GEO')
    Result_D = [0., 0., 0.]
    if(maxval(abs(v2_D - Result_D)) > Epsilon2) &
         write(*,*)'test transform_velocity failed: hgr-GEO v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! The center of the Earth is at 0,0,0 and at rest in GSE,
    ! and relative to HGI it moves with the planet speed
    v2_D = transform_velocity(0., [0., 0., 0.], [0., 0., 0.], 'GSE', 'hgi')
    Result_D = vPlanetHgi_D
    if(maxval(abs(v2_D - Result_D)) > Epsilon3) &
         write(*,*)'test transform_velocity failed: GSE-HGI v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! The surface of the Earth towards the Sun is (RadiusPlanet,0,0) in GSE.
    ! We convert this position to GEO and check how fast the surface moves
    ! with respect to GSE. It should rotate with OmegaRotation around the
    ! rotation axis (in GSE) of the Earth approximately.

    Position_D = matmul(GeoGse_DD, [RadiusPlanet, 0., 0.])
    v2_D = transform_velocity(0., [0., 0., 0.], Position_D, 'GEO', 'GSE')
    Result_D = OmegaRotation*cross_product(RotAxis_D, [RadiusPlanet, 0., 0.])
    if(maxval(abs(v2_D - Result_D)) > 1.0) &
         write(*,*)'test transform_velocity failed: GEO-GSE v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Do it again to check cashing
    v2_D = transform_velocity(0., [0., 0., 0.], Position_D, 'GEO', 'GSE')
    if(maxval(abs(v2_D - Result_D)) > 1.0) &
         write(*,*)'test transform_velocity failed: GEO-GSE2 v2_D = ',v2_D, &
         ' should be equal to ',Result_D,' within round off errors'

    ! Test Mars
    write(*,*) 'Testing Mars'
    ! Reset heliographic offsets so Earth-specific settings do not
    ! leak into the Mars checks.
    dLongitudeHgi    = 0.0
    dLongitudeHgiDeg = 0.0
    dLongitudeHgr    = 0.0
    dLongitudeHgrDeg = 0.0
    IsInitializedPlanet = .false.
    if(.not.is_planet_init('Mars')) write(*,*)'is_planet_init("Mars") failed'

    TimeStart = TimeType(2017, 9, 12, 18, 0, 0, 0.0, 0.0_Real8_, '')
    DoInitializeAxes = .true.
    call time_int_to_real(TimeStart)
    call init_axes(TimeStart % Time)
    write(*,"(a,3es21.12)")' XyzPlanetHgi_D=', XyzPlanetHgi_D
    write(*,"(a,3es21.12)")' vPlanetHgi_D=', vPlanetHgi_D
    write(*,"(a,es21.12)") ' Sun-Mars dist=', norm2(XyzPlanetHgi_D)/cAU
    write(*,"(a,es21.12)") ' LonSubsolar=', LonSubsolar*cRadToDeg
    write(*,"(a,es21.12)") ' LatSubsolar=', LatSubsolar*cRadToDeg
    write(*,"(a,3es21.12)")' RotAxis_D=', RotAxis_D

    write(*,*)'Testing MeridianGeo'
    MeridianGeo = 123.0*cDegToRad
    ! Redo initialization with MeridianGeo set
    AngleGeiGeoStart = -1.0
    DoInitializeAxes = .true.
    call init_axes(TimeStart % Time)
    write(*,"(a,es21.12)") ' LonSubsolar=', LonSubsolar*cRadToDeg
    if(abs(LonSubsolar - MeridianGeo) > 1e-5) write(*,*) &
         'MeridianGeo test failed: LonSubsolar=', LonSubsolar*cRadToDeg, &
         ' should be 123 degrees'

    write(*,*)'Testing set_axes(3600.0)'
    call set_axes(3600.0)
    write(*,"(a,3es21.12)")' XyzPlanetHgi_D=', XyzPlanetHgi_D
    write(*,"(a,es21.12)") ' LonSubsolar=', LonSubsolar*cRadToDeg
    if(abs(LonSubsolar - MeridianGeo + OmegaPlanet*3600) > 1e-3) &
        write(*,*) &
        'set_axes test failed: LonSubsolar=', LonSubsolar*cRadToDeg, &
        ' start=123.0, expected shift=', -OmegaPlanet*3600.0*cRadToDeg

  end subroutine test_axes
  !============================================================================
end module CON_axes
!==============================================================================
