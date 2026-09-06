!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module BATL_geometry

  use BATL_size, ONLY: MaxDim, nDim
  use ModUtilities, ONLY: CON_stop, CON_stop_simple
#ifdef _OPENACC
  use ModUtilities, ONLY: norm2
#endif

  implicit none

  SAVE

  private ! except

  public:: init_geometry  ! initialize the module
  public:: clean_geometry ! clean up storage
  public:: xyz_to_coord   ! convert XYZ coordinates to generalized coordinates
  public:: coord_to_xyz   ! convert generalized coordinates to XYZ coordinates
  public:: radius_to_gen  ! convert radial coordinate to generalized coordinate
  public:: gen_to_radius  ! convert generalized coordinate to radial coordinate
  public:: rot_to_cart    ! Rotate a vector/matrix from rotated to Cartesian
  public:: set_high_geometry
  interface rot_to_cart
     module procedure rot_to_cart_vector, rot_to_cart_matrix
  end interface
  public:: cart_to_rot    ! Rotate a vector/matrix from Cartesian to rotated
  interface cart_to_rot
     module procedure cart_to_rot_vector, cart_to_rot_matrix
  end interface

  character(len=20), public:: TypeGeometryBatl = 'cartesian'

  real, public:: CoordMin_D(MaxDim)   = -0.5    ! Min gen. coords of domain
  real, public:: CoordMax_D(MaxDim)   =  0.5    ! Max gen. coords of domain
  real, public:: DomainSize_D(MaxDim) =  1.0    ! CoordMax - CoordMin
  !$acc declare create(CoordMin_D, CoordMax_D, DomainSize_D)

  ! Cell size of the root blocks in either the first coordinate,
  ! or the Phi direction in degrees if IsLogRadius or IsGenRadius is true.
  real, public:: CellSizeRoot = -1.0

  ! Cartesian, cylindrical or spherical coordinates
  logical, public:: IsCartesianGrid   = .true.  ! Cartesian grid (possibly RZ)
  logical, public:: IsCartesian       = .true.  ! Normal Cartesian geometry
  logical, public:: IsRotatedCartesian= .false. ! Rotated Cartesian grid
  logical, public:: IsRzGeometry      = .false. ! RZ geometry (x is symm. axis)
  logical, public:: IsCubedSphere     = .false. ! cubed sphere
  logical, public:: IsRoundCube       = .false. ! square/cube stretched
  logical, public:: IsCylindrical     = .false. ! cylindrical: r, phi, z
  logical, public:: IsSpherical       = .false. ! spherical: r, theta, phi
  logical, public:: IsRLonLat         = .false. ! spherical: r, lon, lat
  logical, public:: IsCylindricalAxis = .false. ! r=0 boundary for cylindrical
  logical, public:: IsSphericalAxis   = .false. ! theta=0 and pi boundaries
  logical, public:: IsLatitudeAxis    = .false. ! |lat|=pi/2 boundaries
  logical, public:: IsAnyAxis         = .false. ! true if any of the above 3 is
  logical, public:: IsLogRadius       = .false. ! logarithmic radial coordinate
  logical, public:: IsGenRadius       = .false. ! stretched radial coordinate
  logical, public:: IsNegativePhiMin  = .false. ! PhiMin < 0 domain boundary

  ! Use geometry transform desighed for 5th order accuracy FD method.
  logical, public:: UseHighFDGeometry = .false.

  ! Periodicity of the domain per dimension
  logical, public:: IsPeriodic_D(MaxDim) = .false.

  ! Periodicity of the coordinates in physical space per dimension
  logical, public:: IsPeriodicCoord_D(MaxDim) = .false.

  ! Index names for Cartesian components (not limited by nDim)
  integer, parameter, public:: x_=1, y_=2, z_=3

  ! Index names for general coordinate.
  integer, parameter, public:: Xi_=1, Eta_=2, Zeta_=3

  ! The following index names will be set in init_geometry
  integer, public:: r_=-1, Phi_=-1, Theta_=-1, Lon_=-1, Lat_=-1

  ! General radial coordinates
  integer, public:: nRgen = -1    ! number of elements in LogRgen_I
  real,    public, allocatable:: LogRgen_I(:)  ! array of log(r) values

  ! Rotation matrix from generalized to X,Y,Z coordinates
  real, public:: GridRot_DD(MaxDim,MaxDim)
  real, public:: rRound0, rRound1

  ! This is needed for the roundcube geometry
  real, public, parameter:: SqrtNDim = sqrt(real(nDim))

  ! For cubed sphere need to know which of the 6 panels is used
  integer, public:: iCubedSphere = 1

  !$acc declare create(TypeGeometryBatl, IsCartesianGrid, IsCartesian)
  !$acc declare create(IsRzGeometry, IsRotatedCartesian, GridRot_DD)
  !$acc declare create(IsSpherical, IsRLonLat, IsCylindrical)
  !$acc declare create(IsCylindricalAxis, IsSphericalAxis, IsLatitudeAxis)
  !$acc declare create(IsAnyAxis, IsLogRadius, IsGenRadius, nRgen, LogRgen_I)
  !$acc declare create(IsPeriodic_D, IsPeriodicCoord_D, IsNegativePhiMin)
  !$acc declare create(UseHighFDGeometry)
  !$acc declare create(r_, Phi_, Theta_, Lon_, Lat_)
  !$acc declare create(rRound0, rRound1, IsRoundCube)
  !$acc declare create(IsCubedSphere, iCubedSphere)

contains
  !============================================================================
  subroutine init_geometry(TypeGeometryIn, IsPeriodicIn_D, RgenIn_I, &
       UseFDFaceFluxIn)

    use ModNumConst, ONLY: i_DD
    use ModCoordTransform, ONLY: rot_matrix_z

    character(len=*), optional, intent(in):: TypeGeometryIn
    logical,          optional, intent(in):: IsPeriodicIn_D(nDim)
    real,             optional, intent(in):: RgenIn_I(:)
    logical,          optional, intent(in):: UseFDFaceFluxIn

    ! Initialize geometry for BATL
    !
    ! TypeGeometry can be
    !    'cartesian'
    !    'rotatedcartesian'
    !    'rz'
    !    'roundcube'
    !    'cylindrical'
    !    'cylindrical_lnr'
    !    'cylindrical_genr'
    !    'spherical'
    !    'spherical_lnr'
    !    'spherical_genr'
    !    'rlonlat'
    !    'rlonlat_lnr'
    !    'rlonlat_genr'
    !
    ! IsPeriodic_D defines periodicity for each dimension
    !
    ! RgenIn_I defines a mapping from a general index space to the radial
    ! coordinate. The index space is mapped to the 0-1 interval so the
    ! first element RgenIn_I corresponds to 0.0, and the last element to 1.0.
    ! The interpolation is done in log(R), so we get a logarithmic radial
    ! grid within each interval.

    character(len=*), parameter:: NameSub = 'init_geometry'
    !--------------------------------------------------------------------------
    TypeGeometryBatl = 'cartesian'
    if(present(TypeGeometryIn)) TypeGeometryBatl = TypeGeometryIn

    IsPeriodic_D = .false.
    if(present(IsPeriodicIn_D)) IsPeriodic_D(1:nDim) = IsPeriodicIn_D

    ! Logicals are useful for efficient code
    IsCartesian        = TypeGeometryBatl(1:9)  == 'cartesian'
    IsRotatedCartesian = TypeGeometryBatl(1:16) == 'rotatedcartesian'
    IsRzGeometry       = TypeGeometryBatl(1:2)  == 'rz'
    IsSpherical        = TypeGeometryBatl(1:3)  == 'sph'
    IsRLonLat          = TypeGeometryBatl(1:3)  == 'rlo'
    IsCylindrical      = TypeGeometryBatl(1:3)  == 'cyl'
    IsRoundCube        = TypeGeometryBatl(1:5)  == 'round'
    IsCubedSphere      = TypeGeometryBatl(1:5)  == 'cubed'

    IsLogRadius   = index(TypeGeometryBatl,'lnr')  > 0
    IsGenRadius   = index(TypeGeometryBatl,'genr') > 0

    ! Grid is Cartesian (even in RZ geometry)
    IsCartesianGrid = IsCartesian .or. IsRzGeometry

    ! Set up a rotation matrix
    if(IsRotatedCartesian)then
       ! Rotate around the Z axis with atan(3/4)
       GridRot_DD = rot_matrix_z(0.6, 0.8)
    else
       ! Just in case it would be used
       GridRot_DD = i_DD
    end if

    r_ = -1; Phi_ = -1; Theta_ = -1; Lon_ = -1; Lat_ = -1
    if(IsRzGeometry)then
       r_ = 2
    elseif(IsCylindrical)then
       r_ = 1; Phi_ = 2
    elseif(IsSpherical)then
       r_ = 1; Theta_ = 2; Phi_ = 3
    elseif(IsRLonLat .or. IsCubedSphere)then
       r_ = 1; Phi_ = 2; Theta_ = 3; Lon_ =2; Lat_ = 3
    end if

    nRgen = -1
    if(allocated(LogRgen_I)) deallocate(LogRgen_I)
    if(IsGenRadius)then
       if(.not.present(RgenIn_I)) call CON_stop_simple(NameSub// &
            ': RgenIn_I argument is missing for TypeGeometryBatl=' &
            //TypeGeometryBatl)
       ! Store general radial coordinate table
       nRgen = size(RgenIn_I)
       allocate(LogRgen_I(nRgen))
       LogRgen_I = alog(RgenIn_I)
    end if

    call set_high_geometry(UseFDFaceFluxIn)

  end subroutine init_geometry
  !============================================================================
  subroutine set_high_geometry(UseFDFaceFluxIn)
    logical, optional, intent(in):: UseFDFaceFluxIn
    !--------------------------------------------------------------------------
    if(present(UseFDFaceFluxIn)) then
       if(UseFDFaceFluxIn .and. .not. IsCartesian) UseHighFDGeometry = .true.
    endif
  end subroutine set_high_geometry
  !============================================================================
  subroutine clean_geometry

    ! Release storage
    !--------------------------------------------------------------------------
    nRgen = -1
    if(allocated(LogRgen_I)) deallocate(LogRgen_I)

  end subroutine clean_geometry
  !============================================================================
  subroutine xyz_to_coord(XyzIn_D, CoordOut_D)
    !$acc routine seq

    use ModCoordTransform, ONLY: atan2_check, xyz_to_sph, xyz_to_rlonlat
    use ModNumConst, ONLY: cTwoPi

    real, intent(in) :: XyzIn_D(MaxDim)
    real, intent(out):: CoordOut_D(MaxDim)

    real:: x, y, r2, Dist1, Dist2, Coef1, Coef2

    character(len=*), parameter:: NameSub = 'xyz_to_coord'
    !--------------------------------------------------------------------------
    if(IsCartesianGrid)then
       CoordOut_D = XyzIn_D
       RETURN
    elseif(IsRotatedCartesian)then
       CoordOut_D = matmul(XyzIn_D, GridRot_DD)
    elseif(IsCylindrical)then
       x = XyzIn_D(x_); y = XyzIn_D(y_)
       CoordOut_D(r_)   = sqrt(x**2 + y**2)
       CoordOut_D(Phi_) = atan2_check(y, x)
       CoordOut_D(z_)   = XyzIn_D(z_)
    elseif(IsSpherical)then
       call xyz_to_sph(XyzIn_D, CoordOut_D)
    elseif(IsRLonLat)then
       call xyz_to_rlonlat(XyzIn_D, CoordOut_D)
    elseif(IsRoundCube)then
       r2 = sum(XyzIn_D**2)
       if (r2 > 0.0) then
          ! L1 and L2 distance
          Dist1 = maxval(abs(XyzIn_D))
          Dist2 = sqrt(r2)
          if (rRound1 > rRound0 ) then
             ! The rounded grid is outside of the non-distorted part
             if (Dist1 > rRound0) then
                ! Outside the undistorted region
                ! Assume Coord = w * Xyz and Replace Xyz in coord_to_xyz
                ! We have a quadratic equation of w.
                ! w^2 - Coef1*w
                ! - 4*Dist1/(rRound1-rRound0)*(SqrtNDim*Dist1/Dist2 - 1) = 0

                Coef1 = -1 &
                     + rRound0/(rRound1-rRound0)*(dist1*SqrtNDim/Dist2 - 1)
                Coef2 = Coef1**2 &
                     + 4*Dist1/(rRound1-rRound0)*(SqrtNDim*Dist1/Dist2 - 1)
                CoordOut_D = XyzIn_D/(-Coef1 + sqrt(Coef2))*2
             else
                ! No distortion
                CoordOut_D = XyzIn_D
             end if

          else
             ! The rounded (distorted) grid is inside of the non-distorted part
             if (Dist2 < rRound1) then
                ! Solving w^2-w+Coef1 = 0
                Coef1 = Dist1/rRound1*(1 - Dist1/Dist2)
                CoordOut_D = XyzIn_D / (1+sqrt(1-4*Coef1))*2
             else
                ! Solving w^2+Coef1*w+Coef2 = 0
                Coef1 = -1 + (1 - Dist1/Dist2)/(rRound0-rRound1)*rRound0
                Coef2 = -(1 - Dist1/Dist2)/(rRound0 - rRound1)*Dist1
                Coef2 = (-Coef1 + sqrt(Coef1**2 - 4*Coef2))*0.5
                CoordOut_D = XyzIn_D / Coef2
             end if
          end if

       else
          CoordOut_D = 0.0
       end if
    elseif(IsCubedSphere)then
       CoordOut_D(1) = norm2(XyzIn_D)
       if(CoordOut_D(1) > 0)then
          if (XyzIn_D(1) >= maxval(abs(XyzIn_D(2:3)))) then
             ! x >= |y|, |z|
             iCubedSphere = 1
             CoordOut_D(2) = atan2(XyzIn_D(2), XyzIn_D(1))
             CoordOut_D(3) = atan2(XyzIn_D(3), XyzIn_D(1))
          else
             call CON_stop_simple(NameSub// &
                  ' cubed sphere is only implemented for one panel')
          end if
       else
          CoordOut_D(2:3) = 0.0
       end if
    else
       call CON_stop_simple(NameSub// &
            ' not yet implemented for TypeGeometryBatl=', TypeGeometryBatl)
    end if

    if(IsNegativePhiMin)then
       ! Allow for negative Phi angles
       if(CoordOut_D(Phi_) > CoordMax_D(Phi_)) &
            CoordOut_D(Phi_) = CoordOut_D(Phi_) - cTwoPi
    end if

    if(IsLogRadius)then
       CoordOut_D(1) = log(max(CoordOut_D(1), 1e-30))
    elseif(IsGenRadius)then
       call radius_to_gen(CoordOut_D(1))
    end if

  end subroutine xyz_to_coord
  !============================================================================
  subroutine coord_to_xyz(CoordIn_D, XyzOut_D)
    !$acc routine seq

    use ModCoordTransform, ONLY: sph_to_xyz, rlonlat_to_xyz

    real, intent(in) :: CoordIn_D(MaxDim)
    real, intent(out):: XyzOut_D(MaxDim)

    real:: r, r2, Phi, Coord_D(MaxDim), Dist1, Dist2, Weight, TanLon, TanLat

    character(len=*), parameter:: NameSub = 'coord_to_xyz'
    !--------------------------------------------------------------------------
    if(IsCartesianGrid)then
       XyzOut_D = CoordIn_D
       RETURN
    elseif(IsRotatedCartesian)then
       XyzOut_D = matmul(GridRot_DD, CoordIn_D)
       RETURN
    endif

    Coord_D = CoordIn_D
    if(IsLogRadius)then
       Coord_D(1) = exp(Coord_D(1))
    elseif(IsGenRadius)then
       call gen_to_radius(Coord_D(1))
    end if

    if(IsCylindrical)then
       r = Coord_D(r_);
       Phi = Coord_D(Phi_)
       XyzOut_D(1) = r*cos(Phi)
       XyzOut_D(2) = r*sin(Phi)
       XyzOut_D(3) = Coord_D(3)
    elseif(IsSpherical)then
       call sph_to_xyz(Coord_D, XyzOut_D)
    elseif(IsRLonLat)then
       call rlonlat_to_xyz(Coord_D, XyzOut_D)
    elseif(IsRoundCube)then
       r2 = sum(Coord_D**2)
       ! L1 and L2 distances from origin
       ! L1 distance is constant on the surface of a cube
       ! L2 distance is constant on the surface of a sphere
       Dist1 = maxval(abs(Coord_D))
       Dist2 = sqrt(r2)

       if (r2 > 0.0) then
          if (rRound0 < rRound1) then
             ! Non-distorted grid inside, round grid outside
             Weight = (Dist1 - rRound0)/(rRound1 - rRound0)
          elseif (Dist1 < rRound1) then
             ! the rounded grid is inside and the point is inside rRound1
             ! The distortion is 0 at the origin and maximum at Dist1=rRound1.
             Weight = Dist1/rRound1
          else
             ! the rounded grid is inside and the point is outside rRound1
             Weight = (rRound0 - Dist1)/(rRound0 - rRound1)
          endif
          ! Limit weight to be in the [0,1] interval
          Weight = min(1., max(0.0, Weight))

          if (rRound0 < rRound1) then
             ! Expand coordinate outward
             ! For a fully rounded grid we expand the generalized coordinate
             ! by Dist1*SqrtNDim/Dist2, so along the main diagonals there is
             ! no stretch and along the axes the expansion is SqrtNDim.
             ! For the partially rounded grid the expansion factor is reduced.
             ! The minimum expansion factor is 1 in the non-distorted region.
             XyzOut_D = (1 + Weight*(Dist1*SqrtNDim/Dist2 - 1)) * Coord_D
          else
             ! Contract coordinate inward
             ! In this case the grid is contracted along
             ! the main diagonals by a factor up to sqrt(nDim)
             ! and there is no contraction along the axes
             XyzOut_D = (1 + Weight*(Dist1/Dist2 - 1)) * Coord_D
          end if

       else
          XyzOut_D = 0.0
       end if
    elseif(IsCubedSphere)then
       if(Coord_D(1) > 0.0)then
          r = Coord_D(1)
          TanLon = tan(Coord_D(2))
          TanLat = tan(Coord_D(3))
          select case(iCubedSphere)
          case(1)
             XyzOut_D(1) = r/sqrt(1 + TanLon**2 + TanLat**2)
             XyzOut_D(2) = TanLon*XyzOut_D(1)
             XyzOut_D(3) = TanLat*XyzOut_D(1)
          case default
             call CON_stop_simple(NameSub// &
                  ' cubed sphere is only implemented for one panel')
          end select
       else
          XyzOut_D = 0.0
       end if
    else
       call CON_stop_simple(NameSub// &
            ' not yet implemented for TypeGeometryBatl=', TypeGeometryBatl)
    end if

  end subroutine coord_to_xyz
  !============================================================================
  subroutine radius_to_gen(r)
    !$acc routine seq

    use ModInterpolate, ONLY: find_cell

    ! Convert true radial coordinate to general coordinate
    real, intent(inout):: r

    integer:: i
    real:: dCoord

    ! interpolate the logarithm of r
    character(len=*), parameter:: NameSub = 'radius_to_gen'
    !--------------------------------------------------------------------------
    call find_cell(0, nRgen-1, alog(r), &
         i, dCoord, LogRgen_I, DoExtrapolate=.true.)

    ! r is the real coordinate scaled to the 0-1 interval
    r = (i + dCoord)/(nRgen - 1)

  end subroutine radius_to_gen
  !============================================================================
  subroutine gen_to_radius(r)
    !$acc routine seq
    use ModInterpolate, ONLY: linear

    ! Convert generalized radial coordinate to true radial coordinate
    real, intent(inout):: r

    character(len=*), parameter:: NameSub = 'gen_to_radius'
    !--------------------------------------------------------------------------
    ! interpolate the LogRgen_I array for the general coordinate
    r = exp(linear(LogRgen_I, 0, nRgen-1, r*(nRgen-1), DoExtrapolate=.true.))

  end subroutine gen_to_radius
  !============================================================================
  function cart_to_rot_vector(a_D)

    ! Rotate a vector from Cartesian to RotatedCartesian frame

    real, intent(in):: a_D(MaxDim)
    real:: cart_to_rot_vector(MaxDim)
    !--------------------------------------------------------------------------
    if(IsRotatedCartesian)then
       cart_to_rot_vector = matmul(GridRot_DD, a_D)
    else
       cart_to_rot_vector = a_D
    end if

  end function cart_to_rot_vector
  !============================================================================
  function cart_to_rot_matrix(a_DD)

    ! Rotate a matrix from Cartesian to RotatedCartesian frame

    real, intent(in):: a_DD(MaxDim,MaxDim)
    real:: cart_to_rot_matrix(MaxDim,MaxDim)
    !--------------------------------------------------------------------------
    if(IsRotatedCartesian)then
       cart_to_rot_matrix = matmul(GridRot_DD, &
            matmul(a_DD, transpose(GridRot_DD)))
    else
       cart_to_rot_matrix = a_DD
    end if
  end function cart_to_rot_matrix
  !============================================================================
  function rot_to_cart_vector(a_D)

    ! Rotate a vector from RotatedCartesian to Cartesian frame

    real, intent(in):: a_D(MaxDim)
    real:: rot_to_cart_vector(MaxDim)
    !--------------------------------------------------------------------------
    if(IsRotatedCartesian)then
       rot_to_cart_vector = matmul(a_D, GridRot_DD)
    else
       rot_to_cart_vector = a_D
    end if

  end function rot_to_cart_vector
  !============================================================================
  function rot_to_cart_matrix(a_DD)

    ! Rotate a matrix from RotatedCartesian to Cartesian frame

    real, intent(in):: a_DD(MaxDim,MaxDim)
    real:: rot_to_cart_matrix(MaxDim,MaxDim)
    !--------------------------------------------------------------------------
    if(IsRotatedCartesian)then
       rot_to_cart_matrix = matmul(transpose(GridRot_DD), &
            matmul(a_DD, GridRot_DD))
    else
       rot_to_cart_matrix = a_DD
    end if
  end function rot_to_cart_matrix
  !============================================================================
end module BATL_geometry
!==============================================================================
