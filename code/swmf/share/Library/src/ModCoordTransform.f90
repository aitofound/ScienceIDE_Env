!  Copyright (C) AAOA2002 ReOAgents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!
module ModCoordTransform

  ! Contains general subroutines and functions to construct 3 by 3 rotation
  ! matrices around the principal axes, rotation matrix that transform between
  ! Cartesian and spherical vectors, and subroutines that convert between
  ! Cartesian and spherical coordinates and directions.
  !
  ! All functions and subroutines have several versions so that they can
  ! be called with various arguments:
  !
  ! A Cartesian position or vector can be given as
  ! \begin{itemize}
  ! \item 1 array with 3 elements in x, y, z order
  ! \item 3 scalars in x, y, z order
  ! \end{itemize}
  !
  ! A spherical position or vector can be given as
  ! \begin{itemize}
  ! \item 1 array with 3 elements in r, $\theta$, $\phi$ order.
  ! \item 1 array with 3 elements in r, Longitude, Latitude order.
  ! \item 3 scalars in r, $\theta$, $\phi$ order.
  ! \item 3 scalars in r, Longitude, Latitude order.
  ! \end{itemize}
  !
  ! A direction can be given as
  ! \begin{itemize}
  ! \item 1 Cartesian vector with non-zero length
  ! \item 3 Cartesian components in x, y, z order
  ! \item 2 spherical angles in the $\theta$, $\phi$ order
  ! \item 4 scalars in the $\sin\theta, \cos\theta, \sin\phi, \cos\phi$ order
  ! \end{itemize}
  !
  ! A rotational angle can be given as
  ! \begin{itemize}
  ! \item 1 scalar angle $\alpha$ in radians
  ! \item 2 scalars in the $\sin\alpha, \cos\alpha$ order.
  ! \end{itemize}
  !
  ! As convenient utilities, the {\bf cross\_product} and {\bf inverse\_matrix}
  ! functions are provided.
  ! The first returns the cross product of vectors as a 3 element array.
  ! The second returns the inverse of 3 by 3 matrix.
  !
  ! Important note: the Fortran reshape function lists the first 3 elements
  ! of a matrix belonging to the first column (row index changes first), so
  !
  ! A_DD = reshape ( [ a, b, c, &
  !                    d, e, f, &
  !                    g, h, j ], [3,3] )
  !
  ! corresponds to the matrix
  !
  !      (a d g)
  !  A = (b e h)
  !      (c f j)
  !
  ! in normal mathematical notation. (Also see the show_rot_matrix routine).

  use ModNumConst, ONLY: cTwoPi, cHalfPi
  use ModUtilities, ONLY: CON_stop, CON_stop_simple

  implicit none

  save

  private ! except

  public:: rot_matrix     ! 2D rotation matrix (angle)
  public:: rot_matrix_x   ! rotation matrix around X axis (angle)
  public:: rot_matrix_y   ! rotation matrix around Y axis (angle)
  public:: rot_matrix_z   ! rotation matrix around Z axis (angle)
  public:: rot_xyz_sph    ! rotation matrix between Cartesian-spherical (dir)
  public:: rot_xyz_rlonlat! rotation matrix between rlonlat-Cartesian
  public:: rot_xyz_mercator ! rotation matrix to local Mercator map on sphere
  public:: xyz_to_sph     ! convert Cartesian into spherical coordinates
  public:: sph_to_xyz     ! convert spherical into Cartesian coordinates
  public:: xyz_to_rlonlat ! convert Cartesian into Radius-Longitude-Latitude
  public:: rlonlat_to_xyz ! convert Radius-Longitude-Latitude into Cartesian
  public:: xyz_to_dir     ! convert Cartesian vector to spherical direction
  public:: dir_to_xyz     ! convert spher. direction to Cartesian unit vector
  public:: xyz_to_lonlat  ! convert Cartesian vector to Longitude-Latitude
  public:: lonlat_to_xyz  ! convert Longitude-Latitude to Cartesian unit vector
  public:: cross_product  ! return the cross product of two vectors
  public:: inverse_matrix ! return the inverse of a matrix
  public:: determinant    ! return the determinant of a matrix

  public:: show_rot_matrix      ! write out matrix elements in a nice format
  public:: atan2_check          ! compute atan2 even if both x and y are zero
  public:: twopoints_on_sph     ! convert Lon1,2, Lat1,2 to Lon,Lat,Orientation
  public:: test_coord_transform ! unit tester

  ! revision history:
  ! 08Aug03 - Gabor Toth <gtoth@umich.edu> - initial prototype/prolog/code
  ! 29Jun06 - YingJuan - added inverse_matrix function
  ! 02Jun15 - Gabor Toth added support for radius,longitude,latitude

  integer, parameter :: x_=1, y_=2, z_=3

  interface rot_matrix
     module procedure rot_matrix1, rot_matrix2
  end interface rot_matrix

  interface rot_matrix_x
     module procedure rot_matrix_x1, rot_matrix_x2
  end interface rot_matrix_x

  interface rot_matrix_y
     module procedure rot_matrix_y1, rot_matrix_y2
  end interface rot_matrix_y

  interface rot_matrix_z
     module procedure rot_matrix_z1, rot_matrix_z2
  end interface rot_matrix_z

  interface rot_xyz_sph
     module procedure rot_xyz_sph1, rot_xyz_sph2, rot_xyz_sph3, rot_xyz_sph4
  end interface rot_xyz_sph

  interface rot_xyz_rlonlat
     module procedure rot_xyz_rlonlat1, rot_xyz_rlonlat2, &
          rot_xyz_rlonlat3, rot_xyz_rlonlat4
  end interface rot_xyz_rlonlat

  interface rot_xyz_mercator
     module procedure rot_xyz_mercator1, rot_xyz_mercator2, &
          rot_xyz_mercator3, rot_xyz_mercator4
  end interface rot_xyz_mercator

  interface xyz_to_sph
     module procedure xyz_to_sph11, xyz_to_sph13, xyz_to_sph31, xyz_to_sph33
  end interface xyz_to_sph

  interface xyz_to_rlonlat
     module procedure &
          xyz_to_rlonlat11, xyz_to_rlonlat13, &
          xyz_to_rlonlat31, xyz_to_rlonlat33
  end interface xyz_to_rlonlat

  interface sph_to_xyz
     module procedure sph_to_xyz11, sph_to_xyz13, sph_to_xyz31, sph_to_xyz33
  end interface sph_to_xyz

  interface rlonlat_to_xyz
     module procedure &
          rlonlat_to_xyz11, rlonlat_to_xyz13, &
          rlonlat_to_xyz31, rlonlat_to_xyz33
  end interface rlonlat_to_xyz

  interface xyz_to_lonlat
     module procedure xyz_to_lonlat11, xyz_to_lonlat12, xyz_to_lonlat31, &
          xyz_to_lonlat32
  end interface xyz_to_lonlat

  interface xyz_to_dir
     module procedure xyz_to_dir12, xyz_to_dir32, xyz_to_dir14, xyz_to_dir34
  end interface xyz_to_dir

  interface dir_to_xyz
     module procedure dir_to_xyz21, dir_to_xyz23, dir_to_xyz41, dir_to_xyz43
  end interface dir_to_xyz

  interface lonlat_to_xyz
     module procedure lonlat_to_xyz11, lonlat_to_xyz21, lonlat_to_xyz13, &
          lonlat_to_xyz23
  end interface lonlat_to_xyz

  interface cross_product
     module procedure cross_product11, cross_product13, cross_product31,&
          cross_product33
  end interface cross_product

  interface inverse_matrix
     module procedure inverse_matrix33, inverse_matrix_nn
  end interface inverse_matrix

  interface determinant
     module procedure determinant33, determinant_nn
  end interface determinant

  character(len=*), parameter :: NameMod='ModCoordTransform'

contains
  !============================================================================
  subroutine xyz_to_sph11(Xyz_D, Sph_D)
    !$acc routine seq

    real, intent(in) :: Xyz_D(3)
    real, intent(out):: Sph_D(3)
    !--------------------------------------------------------------------------
    call xyz_to_sph(Xyz_D(1), Xyz_D(2), Xyz_D(3), Sph_D(1), Sph_D(2), Sph_D(3))

  end subroutine xyz_to_sph11
  !============================================================================
  subroutine xyz_to_sph13(Xyz_D, r, Theta, Phi)
    !$acc routine seq

    real, intent(in) :: Xyz_D(3)
    real, intent(out):: r,Theta,Phi
    !--------------------------------------------------------------------------
    call xyz_to_sph(Xyz_D(1), Xyz_D(2), Xyz_D(3), r, Theta, Phi)

  end subroutine xyz_to_sph13
  !============================================================================
  subroutine xyz_to_sph31(x, y, z, Sph_D)
    !$acc routine seq

    real, intent(in) :: x, y, z
    real, intent(out):: Sph_D(3)
    !--------------------------------------------------------------------------
    call xyz_to_sph(x, y, z, Sph_D(1), Sph_D(2), Sph_D(3))

  end subroutine xyz_to_sph31
  !============================================================================
  subroutine xyz_to_rlonlat11(Xyz_D, rLonLat_D)
    !$acc routine seq

    real, intent(in) :: Xyz_D(3)
    real, intent(out):: rLonLat_D(3)
    !--------------------------------------------------------------------------
    call xyz_to_sph(Xyz_D(1), Xyz_D(2), Xyz_D(3), &
         rLonLat_D(1), rLonLat_D(3), rLonLat_D(2))

    rLonLat_D(3) = cHalfPi - rLonLat_D(3)

  end subroutine xyz_to_rlonlat11
  !============================================================================
  subroutine xyz_to_rlonlat13(Xyz_D, r, Lon, Lat)
    !$acc routine seq

    real, intent(in) :: Xyz_D(3)
    real, intent(out):: r, Lon, Lat
    !--------------------------------------------------------------------------
    call xyz_to_sph(Xyz_D(1), Xyz_D(2), Xyz_D(3), r, Lat, Lon)
    Lat = cHalfPi - Lat

  end subroutine xyz_to_rlonlat13
  !============================================================================
  subroutine xyz_to_rlonlat31(x, y, z, rLonLat_D)
    !$acc routine seq

    real, intent(in) :: x, y, z
    real, intent(out):: rLonLat_D(3)
    !--------------------------------------------------------------------------
    call xyz_to_sph(x, y, z, rLonLat_D(1), rLonLat_D(3), rLonLat_D(2))
    rLonLat_D(3) = cHalfPi - rLonLat_D(3)

  end subroutine xyz_to_rlonlat31
  !============================================================================
  subroutine xyz_to_rlonlat33(x, y, z, r, Lon, Lat)
    !$acc routine seq

    real, intent(in) :: x, y, z
    real, intent(out):: r, Lon, Lat
    !--------------------------------------------------------------------------
    call xyz_to_sph(x, y, z, r, Lat, Lon)
    Lat = cHalfPi - Lat

  end subroutine xyz_to_rlonlat33
  !============================================================================
  subroutine xyz_to_sph33(x, y, z, r, Theta, Phi)
    !$acc routine seq

    real, intent(in) :: x, y, z

    real, intent(out):: r, Theta, Phi

    ! This is the fundamental routine that converts Cartesian position into
    ! spherical position. All other xyz\_to\_sph varieties call this
    ! subroutine. Here both vectors are given with 3 scalars
    ! (hence the 33 suffix).

    real :: d
    !--------------------------------------------------------------------------
    d     = x**2 + y**2
    r     = sqrt(d + z**2)
    d     = sqrt(d)
    Theta = atan2_check(d, z)
    Phi   = atan2_check(y, x)

  end subroutine xyz_to_sph33
  !============================================================================
  subroutine rlonlat_to_xyz11(rLonLat_D, Xyz_D)
    !$acc routine seq

    real, intent(in) :: rLonLat_D(3)
    real, intent(out):: Xyz_D(3)
    !--------------------------------------------------------------------------
    call sph_to_xyz(rLonLat_D(1), cHalfPi-rLonLat_D(3), rLonLat_D(2), &
         Xyz_D(1), Xyz_D(2), Xyz_D(3))

  end subroutine rlonlat_to_xyz11
  !============================================================================
  subroutine rlonlat_to_xyz31(r, Lon, Lat, Xyz_D)
    !$acc routine seq

    real, intent(in) :: r, Lon, Lat
    real, intent(out):: Xyz_D(3)
    !--------------------------------------------------------------------------
    call sph_to_xyz(r, cHalfPi-Lat, Lon, Xyz_D(1),Xyz_D(2),Xyz_D(3))

  end subroutine rlonlat_to_xyz31
  !============================================================================
  subroutine rlonlat_to_xyz13(rLonLat_D, x, y, z)
    !$acc routine seq

    real, intent(in) :: rLonLat_D(3)
    real, intent(out):: x,y,z
    !--------------------------------------------------------------------------
    call sph_to_xyz(rLonLat_D(1), cHalfPi-rLonLat_D(3), rLonLat_D(2), x, y, z)

  end subroutine rlonlat_to_xyz13
  !============================================================================
  subroutine rlonlat_to_xyz33(r, Lon, Lat, x, y, z)
    !$acc routine seq

    real, intent(in) :: r, Lon, Lat
    real, intent(out):: x, y, z
    !--------------------------------------------------------------------------
    call sph_to_xyz(r, cHalfPi-Lat, Lon, x, y, z)

  end subroutine rlonlat_to_xyz33
  !============================================================================
  subroutine sph_to_xyz11(Sph_D, Xyz_D)
    !$acc routine seq

    real, intent(in) :: Sph_D(3)
    real, intent(out):: Xyz_D(3)
    !--------------------------------------------------------------------------
    call sph_to_xyz(Sph_D(1),Sph_D(2),Sph_D(3),Xyz_D(1),Xyz_D(2),Xyz_D(3))

  end subroutine sph_to_xyz11
  !============================================================================
  subroutine sph_to_xyz31(r, Theta, Phi, Xyz_D)
    !$acc routine seq

    real, intent(in) :: r, Theta, Phi
    real, intent(out):: Xyz_D(3)
    !--------------------------------------------------------------------------
    call sph_to_xyz(r, Theta, Phi, Xyz_D(1), Xyz_D(2), Xyz_D(3))

  end subroutine sph_to_xyz31
  !============================================================================
  subroutine sph_to_xyz13(Sph_D, x, y, z)
    !$acc routine seq

    real, intent(in) :: Sph_D(3)
    real, intent(out):: x, y, z
    !--------------------------------------------------------------------------
    call sph_to_xyz(Sph_D(1), Sph_D(2), Sph_D(3), x, y, z)

  end subroutine sph_to_xyz13
  !============================================================================
  subroutine sph_to_xyz33(r, Theta, Phi, x, y, z)
    !$acc routine seq

    real, intent(in)  :: r, Theta, Phi
    real, intent(out) :: x, y, z

    ! This is the fundamental routine that converts spherical position into
    ! Cartesian position. All other sph\_to\_xyz varieties call this
    ! subroutine. Here both vectors are given with 3 scalars
    ! (hence the 33 suffix).

    ! local variables

    real :: SinTheta
    !--------------------------------------------------------------------------
    SinTheta = sin(Theta)
    x = r*SinTheta*cos(Phi)
    y = r*SinTheta*sin(Phi)
    z = r*cos(Theta)

  end subroutine sph_to_xyz33
  !============================================================================
  subroutine xyz_to_lonlat11(Xyz_D, LonLat_D)
    !$acc routine seq

    real, intent(in) :: Xyz_D(3)
    real, intent(out):: LonLat_D(2)
    !--------------------------------------------------------------------------
    call xyz_to_lonlat(Xyz_D(1), Xyz_D(2), Xyz_D(3), LonLat_D(1), LonLat_D(2))

  end subroutine xyz_to_lonlat11
  !============================================================================
  subroutine xyz_to_lonlat12(Xyz_D, Lon, Lat)
    !$acc routine seq

    real, intent(in) :: Xyz_D(3)
    real, intent(out):: Lon, Lat
    !--------------------------------------------------------------------------
    call xyz_to_lonlat(Xyz_D(1), Xyz_D(2), Xyz_D(3), Lon, Lat)

  end subroutine xyz_to_lonlat12
  !============================================================================
  subroutine xyz_to_lonlat31(x, y, z, LonLat_D)
    !$acc routine seq

    real, intent(in) :: x, y, z
    real, intent(out):: LonLat_D(2)
    !--------------------------------------------------------------------------
    call xyz_to_lonlat(x, y, z, LonLat_D(1), LonLat_D(2))

  end subroutine xyz_to_lonlat31
  !============================================================================
  subroutine xyz_to_lonlat32(x, y, z, Lon, Lat)
    !$acc routine seq

    real, intent(in) :: x, y, z
    real, intent(out):: Lon, Lat
    !--------------------------------------------------------------------------
    Lon = atan2_check(y,x)
    Lat = cHalfPi - atan2_check(sqrt(x**2 + y**2),z)

  end subroutine xyz_to_lonlat32
  !============================================================================
  subroutine xyz_to_dir12(Xyz_D, Theta, Phi)
    !$acc routine seq

    real, intent(in) :: Xyz_D(3)
    real, intent(out):: Theta, Phi
    !--------------------------------------------------------------------------
    call xyz_to_dir(Xyz_D(1), Xyz_D(2), Xyz_D(3), Theta, Phi)

  end subroutine xyz_to_dir12
  !============================================================================
  subroutine xyz_to_dir32(x, y, z, Theta, Phi)
    !$acc routine seq

    real, intent(in) :: x, y, z
    real, intent(out):: Theta, Phi

    ! This is the fundamental routine that converts Cartesian vector into
    ! spherical direction given with angles.
    ! The Cartesian vector can have any positive length.
    !--------------------------------------------------------------------------
    Theta = atan2_check(sqrt(x**2 + y**2), z)
    Phi   = atan2_check(y, x)

  end subroutine xyz_to_dir32
  !============================================================================
  subroutine xyz_to_dir14(Xyz_D, SinTheta, CosTheta, SinPhi, CosPhi)
    !$acc routine seq

    real, intent(in) :: Xyz_D(3)
    real, intent(out):: SinTheta, CosTheta, SinPhi, CosPhi
    !--------------------------------------------------------------------------
    call xyz_to_dir(Xyz_D(1), Xyz_D(2), Xyz_D(3), &
         SinTheta, CosTheta, SinPhi, CosPhi)

  end subroutine xyz_to_dir14
  !============================================================================
  subroutine xyz_to_dir34(x, y, z, SinTheta, CosTheta, SinPhi, CosPhi)
    !$acc routine seq

    real, intent(in) :: x, y, z
    real, intent(out):: SinTheta, CosTheta, SinPhi, CosPhi

    ! This is the fundamental routine that converts Cartesian vector into
    ! spherical direction given with trigonometric functions.
    ! This version is faster (in principle) than the one using angles.
    ! On the other hand here one needs to check the special case
    ! when the vector is parallel with the Z axis.
    ! The Cartesian vector can have any positive length.

    ! local variables

    real :: r, d
    !--------------------------------------------------------------------------
    d = x**2+y**2
    r = sqrt(d+z**2)
    d = sqrt(d)
    SinTheta = d/r
    CosTheta = z/r
    if(d > 0)then
       SinPhi   = y/d
       CosPhi   = x/d
    else
       SinPhi   = 0
       CosPhi   = 0
    end if

  end subroutine xyz_to_dir34
  !============================================================================
  subroutine lonlat_to_xyz11(LonLat_D, Xyz_D)
    !$acc routine seq

    real, intent(in) :: LonLat_D(2)
    real, intent(out):: Xyz_D(3)
    real:: Lon, Lat
    !--------------------------------------------------------------------------
    Lon = LonLat_D(1); Lat = LonLat_D(2)

    call dir_to_xyz(cos(Lat), sin(Lat), sin(Lon), cos(Lon),&
         Xyz_D(1), Xyz_D(2), Xyz_D(3))

  end subroutine lonlat_to_xyz11
  !============================================================================
  subroutine lonlat_to_xyz21(Lon, Lat, Xyz_D)
    !$acc routine seq

    real, intent(in) :: Lon, Lat
    real, intent(out):: Xyz_D(3)
    !--------------------------------------------------------------------------
    call dir_to_xyz(cos(Lat), sin(Lat), sin(Lon), cos(Lon),&
         Xyz_D(1), Xyz_D(2), Xyz_D(3))

  end subroutine lonlat_to_xyz21
  !============================================================================
  subroutine lonlat_to_xyz13(LonLat_D, x, y, z)
    !$acc routine seq

    real, intent(in) :: LonLat_D(2)
    real, intent(out):: x, y, z
    real:: Lon, Lat
    !--------------------------------------------------------------------------
    Lon = LonLat_D(1); Lat = LonLat_D(2)

    call dir_to_xyz(cos(Lat), sin(Lat), sin(Lon), cos(Lon), x, y, z)

  end subroutine lonlat_to_xyz13
  !============================================================================
  subroutine lonlat_to_xyz23(Lon, Lat, x, y, z)
    !$acc routine seq

    real, intent(in) :: Lon, Lat
    real, intent(out):: x, y, z
    !--------------------------------------------------------------------------
    call dir_to_xyz(cos(Lat), sin(Lat), sin(Lon), cos(Lon), x, y, z)

  end subroutine lonlat_to_xyz23
  !============================================================================
  subroutine dir_to_xyz21(Theta, Phi, Xyz_D)
    !$acc routine seq

    real, intent(in) :: Theta, Phi
    real, intent(out):: Xyz_D(3)
    !--------------------------------------------------------------------------
    call dir_to_xyz(sin(Theta), cos(Theta), sin(Phi), cos(Phi),&
         Xyz_D(1), Xyz_D(2), Xyz_D(3))

  end subroutine dir_to_xyz21
  !============================================================================
  subroutine dir_to_xyz23(Theta, Phi, x, y, z)
    !$acc routine seq

    real, intent(in) :: Theta,Phi
    real, intent(out):: x, y, z
    !--------------------------------------------------------------------------
    call dir_to_xyz(sin(Theta), cos(Theta), sin(Phi), cos(Phi), x, y, z)

  end subroutine dir_to_xyz23
  !============================================================================
  subroutine dir_to_xyz41(SinTheta, CosTheta, SinPhi, CosPhi, Xyz_D)
    !$acc routine seq

    real, intent(in) :: SinTheta, CosTheta, SinPhi, CosPhi
    real, intent(out):: Xyz_D(3)
    !--------------------------------------------------------------------------
    call dir_to_xyz(SinTheta, CosTheta, SinPhi, CosPhi, &
         Xyz_D(1), Xyz_D(2), Xyz_D(3))

  end subroutine dir_to_xyz41
  !============================================================================
  subroutine dir_to_xyz43(SinTheta, CosTheta, SinPhi, CosPhi, x, y, z)
    !$acc routine seq

    real, intent(in) :: SinTheta, CosTheta, SinPhi, CosPhi
    real, intent(out):: x, y, z

    ! This is the fundamental routine that converts a spherical direction
    ! into a Cartesian unit vector. The spherical direction can be
    ! given with 4 trigonometric functions or 2 angles.
    ! The Cartesian unit vector can be returned in 1 array or 3 scalars.
    !--------------------------------------------------------------------------
    x = min(1.0, max(-1.0, SinTheta*CosPhi))
    y = min(1.0, max(-1.0, SinTheta*SinPhi))
    z = min(1.0, max(-1.0, CosTheta))

  end subroutine dir_to_xyz43
  !============================================================================
  function rot_matrix1(Angle) result(Rot_DD)

    real, intent(in) :: Angle
    real :: Rot_DD(2,2)
    !--------------------------------------------------------------------------
    Rot_DD = rot_matrix(sin(Angle), cos(Angle))

  end function rot_matrix1
  !============================================================================
  function rot_matrix2(SinAngle, CosAngle) result(Rot_DD)

    real, intent(in) :: SinAngle, CosAngle
    real :: Rot_DD(2,2)
    !--------------------------------------------------------------------------
    Rot_DD(1,1) =  CosAngle
    Rot_DD(1,2) = -SinAngle
    Rot_DD(2,1) =  SinAngle
    Rot_DD(2,2) =  CosAngle

  end function rot_matrix2
  !============================================================================
  function rot_matrix_x1(Angle) result(Rot_DD)
    !$acc routine seq

    real, intent(in) :: Angle
    real :: Rot_DD(3,3)
    !--------------------------------------------------------------------------
    Rot_DD = rot_matrix_x(sin(Angle),cos(Angle))

  end function rot_matrix_x1
  !============================================================================
  function rot_matrix_x2(SinAngle, CosAngle) result(Rot_DD)
    !$acc routine seq

    real, intent(in) :: SinAngle, CosAngle
    real :: Rot_DD(3,3)

    ! This is the fundamental routine that calculates the rotation
    ! matrix around the X axis. Here the rotation angle is given with 2
    ! trigonometric functions. Alternatively it can be given with 1 angle
    ! in radians. The rot\_matrix\_y and rot\_matrix\_z functions are
    ! fully analogous.
    !--------------------------------------------------------------------------
    Rot_DD        =  0
    Rot_DD(y_,y_) =  CosAngle
    Rot_DD(y_,z_) = -SinAngle
    Rot_DD(z_,y_) =  SinAngle
    Rot_DD(z_,z_) =  CosAngle
    Rot_DD(x_,x_) =  1

  end function rot_matrix_x2
  !============================================================================
  function rot_matrix_y1(Angle) result(Rot_DD)
    !$acc routine seq

    real, intent(in) :: Angle
    real             :: Rot_DD(3,3)
    !--------------------------------------------------------------------------
    Rot_DD = rot_matrix_y(sin(Angle), cos(Angle))

  end function rot_matrix_y1
  !============================================================================
  function rot_matrix_y2(SinAngle, CosAngle) result(Rot_DD)
    !$acc routine seq

    real, intent(in) :: CosAngle, SinAngle
    real             :: Rot_DD(3,3)
    !--------------------------------------------------------------------------
    Rot_DD        =  0
    Rot_DD(z_,z_) =  CosAngle
    Rot_DD(z_,x_) = -SinAngle
    Rot_DD(x_,z_) =  SinAngle
    Rot_DD(x_,x_) =  CosAngle
    Rot_DD(y_,y_) =  1

  end function rot_matrix_y2
  !============================================================================
  function rot_matrix_z1(Angle) result(Rot_DD)
    !$acc routine seq

    real, intent(in) :: Angle
    real :: Rot_DD(3,3)
    !--------------------------------------------------------------------------
    rot_DD = rot_matrix_z(sin(Angle),cos(Angle))

  end function rot_matrix_z1
  !============================================================================
  function rot_matrix_z2(SinAngle, CosAngle) result(Rot_DD)
    !$acc routine seq

    real, intent(in) :: SinAngle, CosAngle
    real             :: Rot_DD(3,3)
    !--------------------------------------------------------------------------
    Rot_DD        =  0
    Rot_DD(x_,x_) =  CosAngle
    Rot_DD(x_,y_) = -SinAngle
    Rot_DD(y_,x_) =  SinAngle
    Rot_DD(y_,y_) =  CosAngle
    Rot_DD(z_,z_) =  1

  end function rot_matrix_z2
  !============================================================================
  function rot_xyz_sph1(Xyz_D) result(Rot_DD)

    real, intent(in) :: Xyz_D(3)
    real             :: Rot_DD(3,3)

    real :: SinTheta, CosTheta, SinPhi, CosPhi
    !--------------------------------------------------------------------------
    call xyz_to_dir(Xyz_D(1), Xyz_D(2), Xyz_D(3), &
         SinTheta, CosTheta, SinPhi, CosPhi)

    Rot_DD = rot_xyz_sph(SinTheta,CosTheta,SinPhi,CosPhi)

  end function rot_xyz_sph1
  !============================================================================
  function rot_xyz_sph3(x, y, z) result(Rot_DD)

    real, intent(in) :: x,y,z
    real             :: Rot_DD(3,3)

    real :: SinTheta, CosTheta, SinPhi, CosPhi
    !--------------------------------------------------------------------------
    call xyz_to_dir(x, y, z, SinTheta, CosTheta, SinPhi, CosPhi)

    Rot_DD = rot_xyz_sph(SinTheta, CosTheta, SinPhi, CosPhi)

  end function rot_xyz_sph3
  !============================================================================
  function rot_xyz_sph2(Theta, Phi) result(Rot_DD)

    real, intent(in) :: Theta,Phi
    real             :: Rot_DD(3,3)
    !--------------------------------------------------------------------------
    Rot_DD = rot_xyz_sph4(sin(Theta), cos(Theta), sin(Phi), cos(Phi))

  end function rot_xyz_sph2
  !============================================================================
  function rot_xyz_sph4(SinTheta, CosTheta, SinPhi, CosPhi) result(XyzSph_DD)

    ! This is the fundamental routine that calculates the rotation
    ! matrix between Cartesian and spherical coordinates.
    ! The matrix can be used to convert back and forth like this:
    !     XyzSph_DD = rot_xyz_sph(Xyz_D)
    !     VectorXyz_D = matmul(XyzSph_DD, VectorSph_D)
    !     VectorSph_D = matmul(VectorXyz_D, XyzSph_DD)

    real, intent(in) :: SinTheta, CosTheta, SinPhi, CosPhi
    real             :: XyzSph_DD(3,3)
    !--------------------------------------------------------------------------
    XyzSph_DD = reshape( [ &
         CosPhi*SinTheta, SinPhi*SinTheta,  CosTheta, &
         CosPhi*CosTheta, SinPhi*CosTheta, -SinTheta, &
         -SinPhi,         CosPhi,           0.0 ], &
         [3,3] )

  end function rot_xyz_sph4
  !============================================================================
  function rot_xyz_rlonlat1(Xyz_D) result(Rot_DD)

    real, intent(in) :: Xyz_D(3)
    real             :: Rot_DD(3,3)

    real             :: r, Lon, Lat
    real :: SinLon, CosLon, SinLat, CosLat
    !--------------------------------------------------------------------------
    call xyz_to_rlonlat(Xyz_D,r,lon,lat)

    SinLon = sin(Lon)
    CosLon = cos(Lon)
    SinLat = sin(Lat)
    CosLat = cos(Lat)
    Rot_DD = rot_xyz_rlonlat(SinLon,CosLon,SinLat,CosLat)

  end function rot_xyz_rlonlat1
  !============================================================================
  function rot_xyz_rlonlat3(x, y, z) result(Rot_DD)

    real, intent(in) :: x, y, z
    real             :: Rot_DD(3,3)

    real             :: r, Lon, Lat
    real :: SinLon, CosLon, SinLat, CosLat
    !--------------------------------------------------------------------------
    call xyz_to_rlonlat(x,y,z,r,lon,lat)

    SinLon = sin(Lon)
    CosLon = cos(Lon)
    SinLat = sin(Lat)
    CosLat = cos(Lat)
    Rot_DD = rot_xyz_rlonlat(SinLon,CosLon,SinLat,CosLat)

  end function rot_xyz_rlonlat3
  !============================================================================
  function rot_xyz_rlonlat2(Lon, Lat) result(Rot_DD)

    real, intent(in) :: Lon, Lat
    real             :: Rot_DD(3,3)

    real :: SinLon, CosLon, SinLat, CosLat
    !--------------------------------------------------------------------------
    SinLon = sin(Lon)
    CosLon = cos(Lon)
    SinLat = sin(Lat)
    CosLat = cos(Lat)
    Rot_DD = rot_xyz_rlonlat(SinLon, CosLon, SinLat, CosLat)

  end function rot_xyz_rlonlat2
  !============================================================================
  function rot_xyz_rlonlat4(SinLon, CosLon, SinLat, CosLat) &
       result(XyzRlonlat_DD)

    real, intent(in) :: SinLon, CosLon, SinLat, CosLat
    real             :: XyzRlonlat_DD(3,3)

    ! This is the fundamental routine that calculates the rotation
    ! matrix between Cartesian and R-Lon-Lat coordinates.
    ! The matrix can be used to convert back and forth like this:
    !     XyzRlonlat_DD   = rot_xyz_rlonlat(Xyz_D)
    !     VectorXyz_D     = matmul(XyzRlonlat_DD, VectorRlonlat_D)
    !     VectorRlonlat_D = matmul(VectorXyz_D, XyzRlonlat_DD)
    !--------------------------------------------------------------------------
    XyzRlonlat_DD = reshape ( [ &
         CosLat*CosLon,  CosLat*SinLon,  SinLat, &
         -SinLon,        CosLon,         0.0,     &
         -SinLat*CosLon, -SinLat*SinLon, CosLat ], &
         [3,3] )

  end function rot_xyz_rlonlat4
  !============================================================================
  function rot_xyz_mercator1(Xyz_D) result(Rot_DD)

    real, intent(in) :: Xyz_D(3)
    real             :: Rot_DD(3,3)

    real             :: r, Lon, Lat
    real :: SinLon, CosLon, SinLat, CosLat
    !--------------------------------------------------------------------------
    call xyz_to_rlonlat(Xyz_D,r,lon,lat)

    SinLon = sin(Lon)
    CosLon = cos(Lon)
    SinLat = sin(Lat)
    CosLat = cos(Lat)
    Rot_DD = rot_xyz_mercator(SinLon,CosLon,SinLat,CosLat)

  end function rot_xyz_mercator1
  !============================================================================
  function rot_xyz_mercator3(x, y, z) result(Rot_DD)

    real, intent(in) :: x, y, z
    real             :: Rot_DD(3,3)

    real             :: r, Lon, Lat
    real :: SinLon, CosLon, SinLat, CosLat
    !--------------------------------------------------------------------------
    call xyz_to_rlonlat(x, y, z, r, Lon, Lat)

    SinLon = sin(Lon)
    CosLon = cos(Lon)
    SinLat = sin(Lat)
    CosLat = cos(Lat)
    Rot_DD = rot_xyz_mercator(SinLon,CosLon,SinLat,CosLat)

  end function rot_xyz_mercator3
  !============================================================================
  function rot_xyz_mercator2(Lon, Lat) result(Rot_DD)

    real, intent(in) :: Lon, Lat
    real             :: Rot_DD(3,3)

    real :: SinLon, CosLon, SinLat, CosLat
    !--------------------------------------------------------------------------
    SinLon = sin(Lon)
    CosLon = cos(Lon)
    SinLat = sin(Lat)
    CosLat = cos(Lat)
    Rot_DD = rot_xyz_mercator(SinLon,CosLon,SinLat,CosLat)

  end function rot_xyz_mercator2
  !============================================================================
  function rot_xyz_mercator4(SinLon, CosLon, SinLat, CosLat) &
       result(XyzMercator_DD)

    real, intent(in) :: SinLon, CosLon, SinLat, CosLat
    real :: XyzMercator_DD(3,3)

    ! A vector on the local Mercator map on spherical surface with:
    ! x-axis directed toward local direction to east
    ! y-axis directed toward north
    ! z-axis (usually false) directed upward
    ! can be transformed into Xyz coordinate frame for which:
    ! xy coordinate plane is equatorial plane,
    ! x-axis direction intersects zeroth meridian
    ! z-axis is directed from south to north pole.
    ! Transformation can be done with the formula:
    !
    ! Vector_D in Xyz = matmul(XyzMercator_DD, Vector_D on Mercator map)
    !
    ! In terms of rotation matrices:
    !
    ! XyzMercator_DD = matmul(rot_matrix_z(cPi/2 + Lon),&
    !                         rot_matrix_x(cPi/2 - Lat) )

    ! It could be also obtained from the matrix XyzRlonlat_DD, once the
    ! row/column for R component is passed from first to the third position
    ! (in effect, XyzMercator_DD is (x,y,z) -> (Lon,Lat,R) conversion matrix)
    ! Explicit computation:
    !--------------------------------------------------------------------------
    XyzMercator_DD = reshape ( [ &
         -SinLon,        CosLon,         0.0,     &
         -SinLat*CosLon, -SinLat*SinLon, CosLat,  &
         CosLat*CosLon,  CosLat*SinLon,  SinLat ], &
         [3,3] )

  end function rot_xyz_mercator4
  !============================================================================
  function cross_product11(a_D, b_D) result(c_D)
    !$acc routine seq

    real, intent(in) :: a_D(3), b_D(3)
    real             :: c_D(3)
    !--------------------------------------------------------------------------
    c_D(x_) = a_D(y_)*b_D(z_) - a_D(z_)*b_D(y_)
    c_D(y_) = a_D(z_)*b_D(x_) - a_D(x_)*b_D(z_)
    c_D(z_) = a_D(x_)*b_D(y_) - a_D(y_)*b_D(x_)

  end function cross_product11
  !============================================================================
  function cross_product13(a_D, bX, bY, bZ) result(c_D)
    !$acc routine seq

    real, intent(in) :: a_D(3), bX, bY, bZ
    real             :: c_D(3)
    !--------------------------------------------------------------------------
    c_D(x_) = a_D(y_)*bz - a_D(z_)*by
    c_D(y_) = a_D(z_)*bx - a_D(x_)*bz
    c_D(z_) = a_D(x_)*by - a_D(y_)*bx

  end function cross_product13
  !============================================================================
  function cross_product31(aX, aY, aZ, b_D) result(c_D)
    !$acc routine seq

    real, intent(in) :: aX, aY, aZ, b_D(3)
    real             :: c_D(3)
    !--------------------------------------------------------------------------
    c_D(x_) = aY*b_D(z_) - aZ*b_D(y_)
    c_D(y_) = aZ*b_D(x_) - aX*b_D(z_)
    c_D(z_) = aX*b_D(y_) - aY*b_D(x_)

  end function cross_product31
  !============================================================================
  function cross_product33(aX, aY, aZ, bX, bY, bZ) result(c_D)
    !$acc routine seq

    real, intent(in) :: aX, aY, aZ, bX, bY, bZ
    real             :: c_D(3)
    !--------------------------------------------------------------------------
    c_D(x_) = aY*bZ - aZ*bY
    c_D(y_) = aZ*bX - aX*bZ
    c_D(z_) = aX*bY - aY*bX

  end function cross_product33
  !============================================================================
  function inverse_matrix33(a_DD, SingularLimit, DoIgnoreSingular) result(b_DD)
    !$acc routine seq

    ! Return the inverse of the 3x3 matrix a_DD
    ! The optional SingularLimit gives the smallest value for the determinant
    ! The optional DoIgnoreSingular determines what to do if the
    ! determinant is less than SingularLimit.

    real, intent(in) :: a_DD(3,3)
    real, intent(in), optional :: SingularLimit
    logical, intent(in), optional :: DoIgnoreSingular

    real             :: b_DD(3,3)

    real    :: DetA, Limit
    logical :: DoIgnore

    character(len=*), parameter:: NameSub = 'inverse_matrix33'
    !--------------------------------------------------------------------------
    Limit = 1.e-16
    if(present(SingularLimit)) Limit = SingularLimit
    DoIgnore = .false.
    if(present(DoIgnoreSingular)) DoIgnore = DoIgnoreSingular

    ! Invert the 3x3 matrix:
    b_DD(1,1)=a_DD(2,2)*a_DD(3,3)-a_DD(2,3)*a_DD(3,2)
    b_DD(2,1)=a_DD(2,3)*a_DD(3,1)-a_DD(2,1)*a_DD(3,3)
    b_DD(3,1)=a_DD(2,1)*a_DD(3,2)-a_DD(2,2)*a_DD(3,1)

    b_DD(1,2)=a_DD(1,3)*a_DD(3,2)-a_DD(1,2)*a_DD(3,3)
    b_DD(2,2)=a_DD(1,1)*a_DD(3,3)-a_DD(1,3)*a_DD(3,1)
    b_DD(3,2)=a_DD(1,2)*a_DD(3,1)-a_DD(1,1)*a_DD(3,2)

    b_DD(1,3)=a_DD(1,2)*a_DD(2,3)-a_DD(1,3)*a_DD(2,2)
    b_DD(2,3)=a_DD(1,3)*a_DD(2,1)-a_DD(1,1)*a_DD(2,3)
    b_DD(3,3)=a_DD(1,1)*a_DD(2,2)-a_DD(1,2)*a_DD(2,1)

    DetA= a_DD(1,1)*b_DD(1,1)+a_DD(1,2)*b_DD(2,1)+a_DD(1,3)*b_DD(3,1)

    if(DoIgnore)then
       if(DetA == 0)then
          b_DD = -777.
       else
          b_DD = b_DD/DetA
       end if
    elseif(abs(detA) > Limit*maxval(abs(a_DD)) )then
       b_DD = b_DD/DetA
    else
       write(*,*)'Error in ',NameSub,' for matrix:'
       call show_rot_matrix(a_DD)
       write(*,*)'Determinant=', DetA
       call CON_stop_simple('Singular matrix in '//NameSub)
    end if

  end function inverse_matrix33
  !============================================================================
  function inverse_matrix_nn(n, a_II, SingularLimit, DoIgnoreSingular) &
       result(b_II)

    ! Return the inverse of the n x n matrix a_DD
    ! The optional SingularLimit gives the smallest value for the determinant
    ! The optional DoIgnoreSingular determines what to do if the
    ! determinant is less than SingularLimit.

    integer, intent(in) :: n
    real, intent(in) :: a_II(n,n)
    real, intent(in), optional :: SingularLimit
    logical, intent(in), optional :: DoIgnoreSingular

    real    :: b_II(n,n)

    real    :: DetA, Limit, SignJ, SignI
    logical :: DoIgnore
    integer :: iOrderN_I(n)
    integer :: i , j, iOrder_I(n-1), jOrder_I(n-1)

    character(len=*), parameter:: NameSub = 'inverse_matrix_nn'
    !--------------------------------------------------------------------------
    if(n == 3)then
       b_II = inverse_matrix33(a_II, SingularLimit, DoIgnoreSingular)
       RETURN
    elseif(n==2)then
       b_II(1,1) =  a_II(2,2); b_II(1,2) = -a_II(1,2)
       b_II(2,1) = -a_II(2,1); b_II(2,2) =  a_II(1,1)
       b_II = b_II/(a_II(1,1)*a_II(2,2) - a_II(1,2)*a_II(2,1))
       RETURN
    end if
    Limit = 1.e-16
    if(present(SingularLimit)) Limit = SingularLimit
    DoIgnore = .false.
    if(present(DoIgnoreSingular)) DoIgnore = DoIgnoreSingular
    do i = 1, n
       iOrderN_I(i) = i
    end do
    SignJ = 1
    do j = 1, n  !  Columns of the inverse matrix
       SignJ = -SignJ; SignI = SignJ
       do i = 1, n
          SignI = -SignI

          iOrder_I(1:j-1) = iOrderN_I(1:j-1)
          iOrder_I(j:n-1) = iOrderN_I(j+1:n)

          jOrder_I(1:i-1) = iOrderN_I(1:i-1)
          jOrder_I(i:n-1) = iOrderN_I(i+1:n)

          ! Calculate determinant of a minor and
          ! apply the sign to get a cofactor
          b_II(i,j) = SignI*determinant(a_II(iOrder_I,jOrder_I), n-1)
       end do
    end do
    DetA = sum(a_II(1,1:n)*b_II(1:n,1))

    if(DoIgnore)then
       if(DetA == 0)then
          b_II = -777.
       else
          b_II = b_II/DetA
       end if
    elseif(abs(detA) > Limit*maxval(abs(a_II)) )then
       b_II = b_II/DetA
    else
       write(*,*)'Error in ',NameSub,' for matrix:'
       call show_nbyn_matrix(n, a_II)
       write(*,*)'Determinant=', DetA
       call CON_stop('Singular matrix in '//NameSub)
    end if

  end function inverse_matrix_nn
  !============================================================================
  function determinant33(a_DD) result(Det)

    ! Calculate determinant of 3 by 3 matrix

    real, intent(in)::a_DD(3,3)
    real :: Det
    !--------------------------------------------------------------------------
    Det = a_DD(1,1)*a_DD(2,2)*a_DD(3,3) &
         +a_DD(1,2)*a_DD(2,3)*a_DD(3,1) &
         +a_DD(2,1)*a_DD(3,2)*a_DD(1,3) &
         -a_DD(1,3)*a_DD(2,2)*a_DD(3,1) &
         -a_DD(1,1)*a_DD(2,3)*a_DD(3,2) &
         -a_DD(3,3)*a_DD(1,2)*a_DD(2,1)

  end function determinant33
  !============================================================================
  recursive function determinant_nn(a_II, n) result(Det)

    ! Calculate determinant of N by N matrix

    integer :: n ! size of matrix
    real    :: a_II(n,n)
    real    :: Sub_II(n-1,n-1), Det
    integer :: i, iSign
    !--------------------------------------------------------------------------
    if (n == 1) then
       Det = a_II(1,1)
    elseif(n==2)then
       Det = a_II(1,1)*a_II(2,2) - a_II(1,2)*a_II(2,1)
    elseif(n == 3)then
       Det = determinant33(a_II)
    else
       Det = 0.0
       iSign = 1
       do i = 1, n
          Sub_II(1:n-1,1:i-1) = a_II(2:n,1:i-1)
          Sub_II(1:n-1,i:n-1) = a_II(2:n,i+1:n)

          Det = Det + iSign * a_II(1,i) * determinant_nn(Sub_II, n-1)
          iSign = - iSign
       enddo
    endif

  end function determinant_nn
  !============================================================================
#ifdef _OPENACC
  subroutine show_rot_matrix_acc(Matrix_DD)
    !$acc routine seq nohost
    real, intent(in) :: Matrix_DD(3,3)
    !--------------------------------------------------------------------------
    write(*,*) Matrix_DD(1,1), Matrix_DD(1,2), Matrix_DD(1,3)
    write(*,*) Matrix_DD(2,1), Matrix_DD(2,2), Matrix_DD(2,3)
    write(*,*) Matrix_DD(3,1), Matrix_DD(3,2), Matrix_DD(3,3)

  end subroutine show_rot_matrix_acc
  !============================================================================
#endif
  subroutine show_rot_matrix(Matrix_DD)
    !$acc routine bind(show_rot_matrix_acc)

    real, intent(in) :: Matrix_DD(3,3)
    !--------------------------------------------------------------------------
    write(*,'(3( 3f14.10,/ ))') transpose(Matrix_DD)

  end subroutine show_rot_matrix
  !============================================================================
  subroutine show_nbyn_matrix(n, Matrix_II)

    integer, intent(in) :: n
    real, intent(in) :: Matrix_II(n,n)
    character (len=15) :: NameFormat
    !--------------------------------------------------------------------------
    write(NameFormat,'(a,i1,a,i1,a)')'(',n,'(',n,'f14.10,/ ))'
    write(*,NameFormat)transpose(Matrix_II)

  end subroutine show_nbyn_matrix
  !============================================================================
  real function atan2_check(x, y)
    !$acc routine seq

    real, intent(in) :: x,y
    !--------------------------------------------------------------------------
    if(x==0 .and. y==0)then
       atan2_check = 0
    else
       atan2_check = atan2(x, y)
    end if
    if(atan2_check < 0.0)atan2_check = atan2_check + cTwoPi

  end function atan2_check
  !============================================================================
  subroutine  twopoints_on_sph(Lon1, Lat1,  Lon2, Lat2, &
       Lon, Lat, Orientation, HalfDist, Depth)

    ! For a pair of points (Lon1,Lat1) and (Lon2,Lat2) on a sphere
    ! calculate Orientation, the angle of the direction of the
    ! vevtor from point 1 to point 2 with the local East direction.

    real, intent(in) :: Lon1, Lat1 ! Longitude and latitude of point 1
    real, intent(in) :: Lon2, Lat2 ! Longitude and latitude of point 2
    real, intent(out):: Lon,  Lat  ! Longitude and latitude of mid point
    real, intent(out):: Orientation

    real, optional, intent(out):: HalfDist ! half distance between points
    real, optional, intent(out):: Depth ! Depth of midpoint below surface

    ! Unit vectors pointing to the two points
    real :: Dir1_D(3), Dir2_D(3)
    !--------------------------------------------------------------------------
    call lonlat_to_xyz(Lon1, Lat1, Dir1_D) ! unit vector to point 1
    call lonlat_to_xyz(Lon2, Lat2, Dir2_D) ! unit vector to point 2
    call xyz_to_lonlat(Dir1_D + Dir2_D, Lon, Lat) ! direction to midpoint

    ! Half distance
    if(present(HalfDist)) HalfDist = 0.5*norm2(Dir2_D - Dir1_D)
    ! Depth = sqrt(1 - HalfDist^2) from the Pythagoras theorem:
    if(present(Depth)) Depth = 1 - sqrt(1 - 0.25*sum((Dir2_D - Dir1_D)**2))

    ! Rotate mid point to the +X axis and see where Dir2_D goes
    Dir2_D = matmul(rot_matrix_y(Lat), matmul(rot_matrix_z(-Lon), Dir2_D))

    ! Now the orientation is simply the polar angle of point 2 in the Y-Z plane
    Orientation = atan2(Dir2_D(3), Dir2_D(2))
    ! Keep angle positive
    if(Orientation < 0) Orientation = Orientation + cTwoPi

  end subroutine twopoints_on_sph
  !============================================================================
  subroutine test_coord_transform

    use ModNumConst, ONLY: cPi, cDegToRad, cRadToDeg
    real, parameter:: cTiny = 0.000001
    real, dimension(3):: Xyz_D, Sph_D, rLonLat_D, Xyz2_D
    real:: XyzSph_DD(3,3), XyzRlonlat_DD(3,3), aInv_II(5,5)
    real:: Lon, Lat, Orientation, HalfDist
    real, parameter :: a_II(5,5) = reshape([ 3.0, 7.0, 5.0,21.0, 8.0, &
         16.0, 8.0,17.0,53.0, 7.0,&
         14.0, 6.0,35.0,18.0, 1.0,&
         13.0,19.0, 4.0,22.0,11.0,&
         9.0,21.0, 1.0,23.0,12.0],[5,5])
    ! Transfor Xyz_D to Sph_D by calling subroutine xyz_to_sph
    ! Tranfer Sph_D back to Xyz2_D by calling subroutine sph_to_xyz
    ! Check the difference between the Xyz_D and Xyz2_D.
    !--------------------------------------------------------------------------
    Xyz_D = [0.1, 0.2, 0.3]
    write(*,'(a,3es16.8)')'Xyz_D=', Xyz_D

    call xyz_to_sph(Xyz_D, Sph_D)

    write(*,'(a,3es16.8)')'Sph_D=', Sph_D

    call sph_to_xyz(Sph_D, Xyz2_D)

    write(*,'(a,3es16.8)')'Xyz_D=', Xyz2_D

    if(maxval(abs(Xyz_D-Xyz2_D)) > cTiny) &
         write(*,'(a)')'Error transforming xyz->sph->xyz'

    ! Test xyz_to_rlonlat() and rlonlat_to_xyz()
    call xyz_to_rlonlat(Xyz_D, rLonLat_D)

    write(*,'(a,3es16.8)')'rLonLat_D=', rLonLat_D

    call rlonlat_to_xyz(rLonLat_D, Xyz2_D)

    write(*,'(a,3es16.8)')'Xyz_D=', Xyz2_D

    if(maxval(abs(Xyz_D-Xyz2_D)) > cTiny) &
         write(*,'(a)')'Error transforming xyz->rlonlat->xyz'

    ! test rot_matrix_z()
    write(*,'(a,/,3( 3f14.10,/ ))') 'rot_matrix_z(-Phi)='
    call show_rot_matrix(rot_matrix_z(-Sph_D(3)))

    Xyz_D = matmul(rot_matrix_z(-Sph_D(3)), Xyz_D)

    write(*,'(a,3es16.8)')'rot_matrix_z(-Phi).Xyz_D=', Xyz_D

    Xyz_D = matmul(Xyz_D,rot_matrix_y(Sph_D(2)))

    write(*,'(a,3es16.8)')'Xyz_D.rot_matrix_y(Theta)=', Xyz_D

    if(any(abs(Xyz_D(1:2)) > cTiny)) &
         write(*,'(a)')'Error rotating Xyz_D into Z axis'

    if(abs(Xyz_D(3)-Sph_D(1)) > cTiny) &
         write(*,'(a)')'Error rotating Xyz_D, length changed'

    ! Tese rot_matrix_x(), rot_matrix_y() and rot_matrix_z
    Xyz_D = [0.001, -0.4, 0.35353]
    write(*,'(a,3es16.8)')'Original Xyz=', Xyz_D
    Xyz2_D = matmul(rot_matrix_x(1.), Xyz_D)
    Xyz2_D = matmul(rot_matrix_y(2.), Xyz2_D)
    Xyz2_D = matmul(rot_matrix_z(3.), Xyz2_D)
    Xyz2_D = matmul(rot_matrix_z(-3.), Xyz2_D)
    Xyz2_D = matmul(rot_matrix_y(-2.), Xyz2_D)
    Xyz2_D = matmul(rot_matrix_x(-1.), Xyz2_D)
    write(*,'(a,3es16.8)')'Rotated  Xyz=',Xyz2_D

    if(maxval(abs(Xyz_D-Xyz2_D)) > cTiny) &
         write(*,'(a)')'Error rotating back and forth'

    ! Test rot_xyz_sph()
    write(*,*)
    Xyz_D = [8.0, 0.1, 6.0]
    write(*,'(a,3es16.8)')'Cartesian position=', Xyz_D
    XyzSph_DD = rot_xyz_sph(Xyz_D)
    write(*,'(a)')'XyzSph_DD'; call show_rot_matrix(XyzSph_DD)

    Sph_D = [1.0, 0.0, 0.0]
    write(*,'(a,3es16.8)')'Spherical vector  =', Sph_D
    write(*,'(a,3es16.8)')'Cartesian vector  =', matmul(XyzSph_DD, Sph_D)
    Sph_D = [0.0, 1.0, 0.0]
    write(*,'(a,3es16.8)')'Spherical vector  =', Sph_D
    write(*,'(a,3es16.8)')'Cartesian vector  =', matmul(XyzSph_DD, Sph_D)
    Sph_D = [0.0, 0.0, 1.0]
    write(*,'(a,3es16.8)')'Spherical vector  =', Sph_D
    write(*,'(a,3es16.8)')'Cartesian vector  =', matmul(XyzSph_DD, Sph_D)
    write(*,*)
    write(*,'(a)')'Check inversion: write the inverse of XyzSph_DD:'
    call show_nbyn_matrix(3, inverse_matrix_nn(3, XyzSph_DD))
    write(*,'(a)')'For comparison: write the transposed XyzSph_DD:'
    call show_nbyn_matrix(3, transpose(XyzSph_DD))
    write(*,*)
    write(*,'(a)')'Check inversion: 5 x 5 matrix A:'
    call show_nbyn_matrix(5, a_II)
    aInv_II = inverse_matrix(5, a_II)
    write(*,'(a)')'Check inversion: the inverse matrix AInv:'
    call show_nbyn_matrix(5,aInv_II)
    write(*,'(a)')'Check inversion: the matrix product, A.AInv = diag{1}:'
    call show_nbyn_matrix(5,matmul(aInv_II, a_II))

    ! Test rot_xyz_rlonlat()
    write(*,*)
    Xyz_D = [8.0, 0.1, 6.0]
    write(*,'(a,3es16.8)')'Cartesian position=',Xyz_D
    XyzRlonlat_DD = rot_xyz_rlonlat(Xyz_D)
    write(*,'(a)')'XyzRlonlat_DD'; call show_rot_matrix(XyzRlonlat_DD)

    Rlonlat_D = [1.0, 0.0, 0.0]
    write(*,'(a,3es16.8)')'Rlonlat vector  =', Rlonlat_D
    write(*,'(a,3es16.8)')'Cartesian vector  =', &
         matmul(XyzRlonlat_DD,Rlonlat_D)
    Rlonlat_D = [0.0, 0.0, -1.0]
    write(*,'(a,3es16.8)')'Rlonlat vector  =', Rlonlat_D
    write(*,'(a,3es16.8)')'Cartesian vector  =', &
         matmul(XyzRlonlat_DD,Rlonlat_D)
    Rlonlat_D = [0.0, 1.0, 0.0]
    write(*,'(a,3es16.8)')'Rlonlat vector  =', Rlonlat_D
    write(*,'(a,3es16.8)')'Cartesian vector  =', &
         matmul(XyzRlonlat_DD, Rlonlat_D)
    write(*,*)
    write(*,'(a)')'Check inversion: write the inverse of XyzRlonlat_DD:'
    call show_nbyn_matrix(3, inverse_matrix_nn(3, XyzRlonlat_DD))
    write(*,'(a)')'For comparison: write the transposed XyzRlonlat_DD:'
    call show_nbyn_matrix(3, transpose(XyzRlonlat_DD))

    ! Test rot_xyz_mercator()
    write(*,*)
    Xyz_D = [8.0, 0.1, 6.0]
    write(*,'(a,3es16.8)')'Cartesian position=', Xyz_D
    call xyz_to_rlonlat(Xyz_D, rLonLat_D)
    write(*,'(a)')'XyzMercator_DD'; call show_rot_matrix( &
         rot_xyz_mercator(Xyz_D))
    write(*,'(a)')'Same, but calculated as a product of two rotation matrices:'
    call show_rot_matrix(matmul(rot_matrix_z(cPi/2 + rLonLat_D(2)), &
         rot_matrix_x(cPi/2 - rLonLat_D(3)) ))

    ! Test twopoints_on_sph
    write(*,*)
    write(*,'(a)')'For point 1  with longitude 30  deg and latitude 45 deg'
    write(*,'(a)')'and point 2  with longitude 45  deg and latitude 30 deg'
    write(*,'(a)')'routine twopoints_on_sph provides:'
    call twopoints_on_sph(30*cDegToRad, 45*cDegToRad, 45*cDegToRad, &
         30*cDegToRad, Lon, Lat,  Orientation, HalfDist)
    write(*,'(a,3f10.2)')'Lon, Lat,  Orientation=',&
         Lon*cRadToDeg, Lat*cRadToDeg, Orientation*cRadToDeg
    call rlonlat_to_xyz([1.0, 30*cDegToRad,45*cDegToRad], Xyz_D)
    call rlonlat_to_xyz([1.0, 45*cDegToRad,30*cDegToRad], Xyz2_D)
    call xyz_to_rlonlat(Xyz_D + Xyz2_D, rlonlat_D)
    write(*,'(a)')'For comparison, vector Xyz1_D+Xyz2_D has:'
    write(*,'(a,2f10.2)')'Lon, Lat=',&
         rlonlat_D(2:3)*cRadToDeg
    write(*,'(a)')'Check orientation: in the rotated frame vector'
    write(*,'(a,3es16.8)') &
         '(Xyz2_D - Xyz1_D)/(2*HalfDist) (should be [1,0,0]) =',&
         matmul(Xyz2_D  - Xyz_D, matmul(rot_xyz_mercator(Xyz_D+Xyz2_D),&
         rot_matrix_z(Orientation) ) )/(2*HalfDist)

  end subroutine test_coord_transform
  !============================================================================
end module ModCoordTransform
!==============================================================================
