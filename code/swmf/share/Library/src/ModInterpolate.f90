!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module ModInterpolate

  ! Calculate second order accurate interpolation for
  !
  ! - a uniform grid with normalized coordinates, or
  ! - non-uniform grid with actual coordinates, or
  ! - any mixture of the two, i.e. only some of the coordinates are uniform
  !
  ! Normalized coordinates mean that the coordinates coincide with the
  ! indexes at the grid points. For uniform grid this is a very fast algorithm.
  ! For non-uniform grid a binary search is needed. The coordinates are assumed
  ! to be either monotone increasing or monotone decreasing.
  !
  ! One can interpolate both scalar and vector valued arrays of
  ! integers, single and double precision reals, and complex numbers.
  !
  ! If the coordinates are outside the allowed ranges and the DoExtrapolate
  ! argument is not present the code stops. If the DoExtrapolate argument
  ! is present and false, the last grid cell value is used. If DoExtrapolate
  ! is present and true, second order extrapolation is used.

  ! Examples of usage:
  !
  ! Cell based 2D uniform grid with ghost cells, scalar valued:
  !
  !     InterpolatedValue = bilinear(Value_II, 0, nI+1, 0, nJ+1, &
  !                         [ (x - x0)/DeltaX, (y - y0)/DeltaY) ] )
  !
  ! Node based 2D grid with x(1)=y(1)=0.0, vector valued:
  !
  !     InterpolatedValue_V = bilinear(Value_VII, nVar, 1, nI, 1, nJ, &
  !                        [ x/DeltaX+1, y/DeltaY+1 ] )
  !
  ! Nonuniform 3D grid with ghost cells, third coordinate is uniform,
  ! scalar valued:
  !
  !     InterpolatedValue = trilinear(Value_III, -1, nI+2, -1, nJ+2, -1, nK+2,&
  !                       [ x, y, (z - z0)/DeltaZ ], x_I, y_I)
  !

  use ModUtilities, ONLY: CON_stop, find_cell
  use ModKind, ONLY: Real4_, Real8_

  implicit none

  private ! except

  public :: interpolate_scalar  ! interpolate real scalar in 1...5D
  public :: interpolate_scalar4 ! single precision scalar
  public :: interpolate_vector  ! interpolate real vector in 1...5D
  public :: interpolate_vector4 ! single precision vector
  public :: linear              ! interpolate integer/real/complex in 1D
  public :: bilinear            ! interpolate integer/real/complex in 2D
  public :: trilinear           ! interpolate integer/real/complex in 3D
  public :: quadlinear          ! interpolate integer/real/complex in 4D
  public :: pentalinear         ! interpolate integer/real/complex in 5D
  public :: find_cell           ! find cell in non-uniform grid
  public :: fit_parabola        ! fit a parabola around an extremum
  public :: test_interpolation  ! unit test

  interface linear
     module procedure linear_scalar, linear_vector
  end interface

  interface linear_real4
     module procedure linear_scalar_real4, linear_vector_real4
  end interface

  interface bilinear
     module procedure bilinear_scalar, bilinear_vector
  end interface

  interface bilinear_real4
     module procedure bilinear_scalar_real4, bilinear_vector_real4
  end interface

  interface trilinear
     module procedure trilinear_scalar, trilinear_vector
  end interface

  interface trilinear_real4
     module procedure trilinear_scalar_real4, trilinear_vector_real4
  end interface

  interface quadlinear
     module procedure quadlinear_scalar, quadlinear_vector
  end interface

  interface quadlinear_real4
     module procedure quadlinear_scalar_real4, quadlinear_vector_real4
  end interface

  interface pentalinear
     module procedure pentalinear_scalar, pentalinear_vector
  end interface

  interface pentalinear_real4
     module procedure pentalinear_scalar_real4, pentalinear_vector_real4
  end interface

contains
  !============================================================================
  real function interpolate_scalar(a_C, nDim, Min_D, Max_D, x_D, &
       x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Interpolate scalar default precision real array of up to 5 dimensions
    ! Index and distance can be returned in 1D only

    real, intent(in):: a_C(*)
    integer, intent(in):: nDim
    integer, intent(in):: Min_D(nDim), Max_D(nDim)

    real,    optional, intent(in):: x_D(nDim)
    real,    optional, intent(in):: x1_I(:), x2_I(:), x3_I(:), x4_I(:), x5_I(:)
    logical, optional, intent(in):: DoExtrapolate
    integer, optional, intent(in):: iCell_D(nDim)
    real,    optional, intent(in):: Dist_D(nDim)

    character(len=*), parameter:: NameSub = 'interpolate_scalar'
    !--------------------------------------------------------------------------
    select case(nDim)
    case(1)
       if(present(iCell_D))then
          interpolate_scalar = linear_scalar( a_C, &
               Min_D(1), Max_D(1), x_D(1), iCell=iCell_D(1), Dist=Dist_D(1))
       else
          interpolate_scalar = linear_scalar( a_C, &
               Min_D(1), Max_D(1), x_D(1), x1_I, DoExtrapolate)
       end if
    case(2)
       interpolate_scalar = bilinear_scalar( a_C, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), &
            x_D, x1_I, x2_I, DoExtrapolate, iCell_D, Dist_D)
    case(3)
       interpolate_scalar = trilinear_scalar( a_C, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
            x_D, x1_I, x2_I, x3_I, DoExtrapolate, iCell_D, Dist_D)
    case(4)
       interpolate_scalar = quadlinear_scalar( a_C, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
            Min_D(4), Max_D(4), &
            x_D, x1_I, x2_I, x3_I, x4_I, DoExtrapolate, iCell_D, Dist_D)
    case(5)
       interpolate_scalar = pentalinear_scalar( a_C, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
            Min_D(4), Max_D(4), Min_D(5), Max_D(5), &
            x_D, x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
    case default
#ifndef _OPENACC
       call CON_stop(NameSub//' nDim should be 1 to 5')
#endif
    end select

  end function interpolate_scalar
  !============================================================================
  real function interpolate_scalar4(a_C, nDim, Min_D, Max_D, x_D, &
       x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Interpolate scalar single precision real array of up to 5 dimensions
    ! Index and distance can be returned in 1D only

    real(Real4_), intent(in):: a_C(*) ! single precision
    integer,      intent(in):: nDim
    integer,      intent(in):: Min_D(nDim), Max_D(nDim)

    real,    optional, intent(in):: x_D(nDim)
    real,    optional, intent(in):: x1_I(:), x2_I(:), x3_I(:), x4_I(:), x5_I(:)
    logical, optional, intent(in):: DoExtrapolate
    integer, optional, intent(in):: iCell_D(nDim)
    real,    optional, intent(in):: Dist_D(nDim)

    character(len=*), parameter:: NameSub = 'interpolate_scalar4'
    !--------------------------------------------------------------------------
    select case(nDim)
    case(1)
       if(present(iCell_D))then
          interpolate_scalar4 = linear_scalar_real4( a_C, &
            Min_D(1), Max_D(1), x_D(1), iCell=iCell_D(1), Dist=Dist_D(1))
       else
          interpolate_scalar4 = linear_scalar_real4( a_C, &
            Min_D(1), Max_D(1), x_D(1), x1_I, DoExtrapolate)
       end if
    case(2)
       interpolate_scalar4 = bilinear_scalar_real4( a_C, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), &
            x_D, x1_I, x2_I, DoExtrapolate, iCell_D, Dist_D)
    case(3)
       interpolate_scalar4 = trilinear_scalar_real4( a_C, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
            x_D, x1_I, x2_I, x3_I, DoExtrapolate, iCell_D, Dist_D)
     case(4)
        interpolate_scalar4 = quadlinear_scalar_real4( a_C, &
             Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
             Min_D(4), Max_D(4), &
             x_D, x1_I, x2_I, x3_I, x4_I, DoExtrapolate, iCell_D, Dist_D)
     case(5)
        interpolate_scalar4 = pentalinear_scalar_real4( a_C, &
             Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
             Min_D(4), Max_D(4), Min_D(5), Max_D(5), &
             x_D, x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
     case default
#ifndef _OPENACC
        call CON_stop(NameSub//' nDim should be 1 to 5')
#endif
    end select

  end function interpolate_scalar4
  !============================================================================
  real function linear_scalar(a_I, iMin, iMax, x, x_I, DoExtrapolate, &
       iCell, Dist)
    !$acc routine seq
    ! Interface for default precision real array

    integer, intent(in) :: iMin, iMax
    real, intent(in) :: a_I(iMin:iMax)
    real,    intent(in), optional:: x, x_I(iMin:), Dist
    logical, intent(in), optional :: DoExtrapolate
    integer, intent(in), optional :: iCell

    integer :: i1, i2
    real    :: Dx1, Dx2

    character(len=*), parameter:: NameSub = 'linear_scalar'
    !--------------------------------------------------------------------------
    if ( present(iCell) .and. present(Dist) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell
       i2 = i1 + 1
       Dx1 = Dist
       Dx2 = 1.0 - Dx1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x, i1, Dx1, x_I, DoExtrapolate, &
            "Called from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1

    end if

    ! Perform interpolation (or extrapolation)
    linear_scalar = Dx2*a_I(i1) + Dx1*a_I(i2)

  end function linear_scalar
  !============================================================================
  real function linear_scalar_real4(a_I, iMin, iMax, x, x_I, DoExtrapolate, &
       iCell, Dist)
    !$acc routine seq
    ! Interface for default precision real array

    integer,      intent(in) :: iMin, iMax
    real(Real4_), intent(in) :: a_I(iMin:iMax)
    real,    intent(in), optional:: x, x_I(iMin:), Dist
    logical, intent(in), optional :: DoExtrapolate
    integer, intent(in), optional :: iCell

    integer :: i1, i2
    real    :: Dx1, Dx2

    character(len=*), parameter:: NameSub = 'linear_scalar_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell) .and. present(Dist) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell
       i2 = i1 + 1
       Dx1 = Dist
       Dx2 = 1.0 - Dx1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x, i1, Dx1, x_I, DoExtrapolate, &
            "Called from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1

    end if

    ! Perform interpolation (or extrapolation)
    linear_scalar_real4 = Dx2*a_I(i1) + Dx1*a_I(i2)
  end function linear_scalar_real4
  !============================================================================
  real function bilinear_scalar( &
       a_II, iMin, iMax, jMin, jMax, Xy_D, x_I, y_I, DoExtrapolate, &
       iCell_D, Dist_D)
    !$acc routine seq
    ! Interface with default precision real array

    integer, intent(in):: iMin, iMax, jMin, jMax
    real, intent(in):: a_II(iMin:iMax,jMin:jMax)
    real,    intent(in), optional:: Xy_D(2), x_I(iMin:), y_I(jMin:), Dist_D(2)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(2)

    integer :: i1, i2, j1, j2
    real    :: Dx1, Dx2, Dy1, Dy2
    character(len=*), parameter:: NameSub = 'bilinear_scalar'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1 = Dist_D(1)
       Dx2 = 1.0 - Dx1

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dy1 = Dist_D(2)
       Dy2 = 1.0 - Dy1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, Xy_D(1), i1, Dx1, x_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, Xy_D(2), j1, Dy1, y_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1
       j2 = j1 + 1; Dy2 = 1.0 - Dy1

    end if

    ! Perform interpolation (or extrapolation)
    bilinear_scalar = Dy2*( Dx2*a_II(i1,j1)   &
         +                  Dx1*a_II(i2,j1))  &
         +            Dy1*( Dx2*a_II(i1,j2)   &
         +                  Dx1*a_II(i2,j2))
  end function bilinear_scalar
  !============================================================================
  real function bilinear_scalar_real4( &
       a_II, iMin, iMax, jMin, jMax, Xy_D, x_I, y_I, DoExtrapolate, &
       iCell_D, Dist_D)
    !$acc routine seq
    ! Interface with single precision array

    integer, intent(in):: iMin, iMax, jMin, jMax
    real(Real4_), intent(in):: a_II(iMin:iMax,jMin:jMax)
    real,    intent(in), optional:: Xy_D(2), x_I(iMin:), y_I(jMin:), Dist_D(2)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(2)

    integer :: i1, i2, j1, j2
    real    :: Dx1, Dx2, Dy1, Dy2
    character(len=*), parameter:: NameSub = 'bilinear_scalar_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1 = Dist_D(1)
       Dx2 = 1.0 - Dx1

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dy1 = Dist_D(2)
       Dy2 = 1.0 - Dy1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, Xy_D(1), i1, Dx1, x_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, Xy_D(2), j1, Dy1, y_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1
       j2 = j1 + 1; Dy2 = 1.0 - Dy1

    end if

    ! Perform interpolation (or extrapolation)
    bilinear_scalar_real4 = Dy2*( Dx2*a_II(i1,j1)   &
         +                  Dx1*a_II(i2,j1))  &
         +            Dy1*( Dx2*a_II(i1,j2)   &
         +                  Dx1*a_II(i2,j2))
  end function bilinear_scalar_real4
  !============================================================================
  real function trilinear_scalar( &
       a_III, iMin, iMax, jMin, jMax, kMin, kMax, Xyz_D, &
       x_I, y_I, z_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate trilinear interpolation of a_III at position Xyz_D

    integer, intent(in):: iMin, iMax, jMin, jMax, kMin, kMax
    real, intent(in):: a_III(iMin:iMax,jMin:jMax,kMin:kMax)
    real,    intent(in), optional:: &
         Xyz_D(3), x_I(iMin:), y_I(jMin:), z_I(kMin:), Dist_D(3)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(3)

    integer :: i1, i2, j1, j2, k1, k2
    real    :: Dx1, Dx2, Dy1, Dy2, Dz1, Dz2

    character(len=*), parameter:: NameSub = 'trilinear_scalar'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1 = Dist_D(1)
       Dx2 = 1.0-Dx1

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dy1 = Dist_D(2)
       Dy2 = 1.0 - Dy1

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dz1 = Dist_D(3)
       Dz2 = 1.0 - Dz1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, Xyz_D(1), i1, Dx1, x_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, Xyz_D(2), j1, Dy1, y_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, Xyz_D(3), k1, Dz1, z_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1
       j2 = j1 + 1; Dy2 = 1.0 - Dy1
       k2 = k1 + 1; Dz2 = 1.0 - Dz1

    end if

    ! Perform interpolation (or extrapolation)
    trilinear_scalar = Dz2*( Dy2*( Dx2*a_III(i1,j1,k1)   &
         +                         Dx1*a_III(i2,j1,k1))  &
         +                   Dy1*( Dx2*a_III(i1,j2,k1)   &
         +                         Dx1*a_III(i2,j2,k1))) &
         +             Dz1*( Dy2*( Dx2*a_III(i1,j1,k2)   &
         +                         Dx1*a_III(i2,j1,k2))  &
         +                   Dy1*( Dx2*a_III(i1,j2,k2)   &
         +                         Dx1*a_III(i2,j2,k2)))
  end function trilinear_scalar
  !============================================================================
  real function trilinear_scalar_real4( &
       a_III, iMin, iMax, jMin, jMax, kMin, kMax, Xyz_D, &
       x_I, y_I, z_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate trilinear interpolation of a_III at position Xyz_D

    integer,      intent(in):: iMin, iMax, jMin, jMax, kMin, kMax
    real(Real4_), intent(in):: a_III(iMin:iMax,jMin:jMax,kMin:kMax)
    real,    intent(in), optional:: &
         Xyz_D(3), x_I(iMin:), y_I(jMin:), z_I(kMin:), Dist_D(3)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(3)

    integer :: i1, i2, j1, j2, k1, k2
    real    :: Dx1, Dx2, Dy1, Dy2, Dz1, Dz2

    character(len=*), parameter:: NameSub = 'trilinear_scalar_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1 = Dist_D(1)
       Dx2 = 1.0-Dx1

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dy1 = Dist_D(2)
       Dy2 = 1.0 - Dy1

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dz1 = Dist_D(3)
       Dz2 = 1.0 - Dz1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, Xyz_D(1), i1, Dx1, x_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, Xyz_D(2), j1, Dy1, y_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, Xyz_D(3), k1, Dz1, z_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1
       j2 = j1 + 1; Dy2 = 1.0 - Dy1
       k2 = k1 + 1; Dz2 = 1.0 - Dz1

    end if

    ! Perform interpolation (or extrapolation)
    trilinear_scalar_real4 = Dz2*( Dy2*( Dx2*a_III(i1,j1,k1)   &
         +                         Dx1*a_III(i2,j1,k1))  &
         +                   Dy1*( Dx2*a_III(i1,j2,k1)   &
         +                         Dx1*a_III(i2,j2,k1))) &
         +             Dz1*( Dy2*( Dx2*a_III(i1,j1,k2)   &
         +                         Dx1*a_III(i2,j1,k2))  &
         +                   Dy1*( Dx2*a_III(i1,j2,k2)   &
         +                         Dx1*a_III(i2,j2,k2)))
  end function trilinear_scalar_real4
  !============================================================================
  real function quadlinear_scalar( &
       a_I4, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, &
       x_D, x1_I, x2_I, x3_I, x4_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate quadlinear interpolation of a_I4 at position x_D

    integer, intent(in):: iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax
    real, intent(in):: a_I4(iMin:iMax,jMin:jMax,kMin:kMax,lMin:lMax)
    real,    intent(in), optional:: &
         x_D(4), x1_I(iMin:), x2_I(jMin:), x3_I(kMin:), x4_I(lMin:), Dist_D(:)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(:)

    integer :: i1, i2, j1, j2, k1, k2, l1, l2
    real    :: Dx1L, Dx1R, Dx2L, Dx2R, Dx3L, Dx3R, Dx4L, Dx4R
    character(len=*), parameter:: NameSub = 'quadlinear_scalar'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1L = Dist_D(1)
       Dx1R = 1.0 - Dx1L

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dx2L = Dist_D(2)
       Dx2R = 1.0 - Dx2L

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dx3L = Dist_D(3)
       Dx3R = 1.0 - Dx3L

       l1 = iCell_D(4)
       l2 = l1 + 1
       Dx4L = Dist_D(4)
       Dx4R = 1.0 - Dx4L

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x_D(1), i1, Dx1L, x1_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, x_D(2), j1, Dx2L, x2_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, x_D(3), k1, Dx3L, x3_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       call find_cell(lMin, lMax, x_D(4), l1, Dx4L, x4_I, DoExtrapolate, &
            "Called for coord4 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx1R = 1.0 - Dx1L
       j2 = j1 + 1; Dx2R = 1.0 - Dx2L
       k2 = k1 + 1; Dx3R = 1.0 - Dx3L
       l2 = l1 + 1; Dx4R = 1.0 - Dx4L

    end if

    quadlinear_scalar = &
         +Dx4R*(Dx3R*(Dx2R*(Dx1R*a_I4(i1,j1,k1,l1)   &
         +                  Dx1L*a_I4(i2,j1,k1,l1))  &
         +            Dx2L*(Dx1R*a_I4(i1,j2,k1,l1)   &
         +                  Dx1L*a_I4(i2,j2,k1,l1))) &
         +      Dx3L*(Dx2R*(Dx1R*a_I4(i1,j1,k2,l1)   &
         +                  Dx1L*a_I4(i2,j1,k2,l1))  &
         +            Dx2L*(Dx1R*a_I4(i1,j2,k2,l1)   &
         +                  Dx1L*a_I4(i2,j2,k2,l1))))&
         +Dx4L*(Dx3R*(Dx2R*(Dx1R*a_I4(i1,j1,k1,l2)   &
         +                  Dx1L*a_I4(i2,j1,k1,l2))  &
         +            Dx2L*(Dx1R*a_I4(i1,j2,k1,l2)   &
         +                  Dx1L*a_I4(i2,j2,k1,l2))) &
         +      Dx3L*(Dx2R*(Dx1R*a_I4(i1,j1,k2,l2)   &
         +                  Dx1L*a_I4(i2,j1,k2,l2))  &
         +            Dx2L*(Dx1R*a_I4(i1,j2,k2,l2)   &
         +                  Dx1L*a_I4(i2,j2,k2,l2))))
  end function quadlinear_scalar
  !============================================================================
  real function quadlinear_scalar_real4( &
       a_I4, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, &
       x_D, x1_I, x2_I, x3_I, x4_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate quadlinear interpolation of a_I4 at position x_D

    integer,      intent(in):: iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax
    real(Real4_), intent(in):: a_I4(iMin:iMax,jMin:jMax,kMin:kMax,lMin:lMax)
    real,    intent(in), optional:: &
         x_D(4), x1_I(iMin:), x2_I(jMin:), x3_I(kMin:), x4_I(lMin:), Dist_D(:)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(:)

    integer :: i1, i2, j1, j2, k1, k2, l1, l2
    real    :: Dx1L, Dx1R, Dx2L, Dx2R, Dx3L, Dx3R, Dx4L, Dx4R
    character(len=*), parameter:: NameSub = 'quadlinear_scalar_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1L = Dist_D(1)
       Dx1R = 1.0 - Dx1L

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dx2L = Dist_D(2)
       Dx2R = 1.0 - Dx2L

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dx3L = Dist_D(3)
       Dx3R = 1.0 - Dx3L

       l1 = iCell_D(4)
       l2 = l1 + 1
       Dx4L = Dist_D(4)
       Dx4R = 1.0 - Dx4L

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x_D(1), i1, Dx1L, x1_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, x_D(2), j1, Dx2L, x2_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, x_D(3), k1, Dx3L, x3_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       call find_cell(lMin, lMax, x_D(4), l1, Dx4L, x4_I, DoExtrapolate, &
            "Called for coord4 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx1R = 1.0 - Dx1L
       j2 = j1 + 1; Dx2R = 1.0 - Dx2L
       k2 = k1 + 1; Dx3R = 1.0 - Dx3L
       l2 = l1 + 1; Dx4R = 1.0 - Dx4L

    end if

    quadlinear_scalar_real4 = &
         +Dx4R*(Dx3R*(Dx2R*(Dx1R*a_I4(i1,j1,k1,l1)   &
         +                  Dx1L*a_I4(i2,j1,k1,l1))  &
         +            Dx2L*(Dx1R*a_I4(i1,j2,k1,l1)   &
         +                  Dx1L*a_I4(i2,j2,k1,l1))) &
         +      Dx3L*(Dx2R*(Dx1R*a_I4(i1,j1,k2,l1)   &
         +                  Dx1L*a_I4(i2,j1,k2,l1))  &
         +            Dx2L*(Dx1R*a_I4(i1,j2,k2,l1)   &
         +                  Dx1L*a_I4(i2,j2,k2,l1))))&
         +Dx4L*(Dx3R*(Dx2R*(Dx1R*a_I4(i1,j1,k1,l2)   &
         +                  Dx1L*a_I4(i2,j1,k1,l2))  &
         +            Dx2L*(Dx1R*a_I4(i1,j2,k1,l2)   &
         +                  Dx1L*a_I4(i2,j2,k1,l2))) &
         +      Dx3L*(Dx2R*(Dx1R*a_I4(i1,j1,k2,l2)   &
         +                  Dx1L*a_I4(i2,j1,k2,l2))  &
         +            Dx2L*(Dx1R*a_I4(i1,j2,k2,l2)   &
         +                  Dx1L*a_I4(i2,j2,k2,l2))))
  end function quadlinear_scalar_real4
  !============================================================================
  real function pentalinear_scalar( &
       a_I5, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, mMin, mMax, &
       x_D, x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate pentalinear interpolation of default prec. a_I5 at position x_D

    integer, intent(in):: &
         iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, mMin, mMax
    real, intent(in):: &
         a_I5(iMin:iMax,jMin:jMax,kMin:kMax,lMin:lMax,mMin:mMax)

    real,    intent(in), optional:: x_D(5), Dist_D(5), &
         x1_I(iMin:), x2_I(jMin:), x3_I(kMin:), x4_I(lMin:), x5_I(mMin:)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional :: iCell_D(5)

    integer :: i1, i2, j1, j2, k1, k2, l1, l2, m1, m2
    real    :: Dx1L, Dx1R, Dx2L, Dx2R, Dx3L, Dx3R, Dx4L, Dx4R, Dx5L, Dx5R

    character(len=*), parameter:: NameSub = 'pentalinear_scalar'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1L = Dist_D(1)
       Dx1R = 1.0 - Dx1L

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dx2L = Dist_D(2)
       Dx2R = 1.0 - Dx2L

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dx3L = Dist_D(3)
       Dx3R = 1.0 - Dx3L

       l1 = iCell_D(4)
       l2 = l1 + 1
       Dx4L = Dist_D(4)
       Dx4R = 1.0 - Dx4L

       m1 = iCell_D(5)
       m2 = m1 + 1
       Dx5L = Dist_D(5)
       Dx5R = 1.0 - Dx5L

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x_D(1), i1, Dx1L, x1_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, x_D(2), j1, Dx2L, x2_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, x_D(3), k1, Dx3L, x3_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       call find_cell(lMin, lMax, x_D(4), l1, Dx4L, x4_I, DoExtrapolate, &
            "Called for coord4 from "//NameSub)

       call find_cell(mMin, mMax, x_D(5), m1, Dx5L, x5_I, DoExtrapolate, &
            "Called for coord5 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx1R = 1.0 - Dx1L
       j2 = j1 + 1; Dx2R = 1.0 - Dx2L
       k2 = k1 + 1; Dx3R = 1.0 - Dx3L
       l2 = l1 + 1; Dx4R = 1.0 - Dx4L
       m2 = m1 + 1; Dx5R = 1.0 - Dx5L

    end if

    pentalinear_scalar = &
         +Dx5R*(Dx4R*(Dx3R*(Dx2R*(Dx1R*a_I5(i1,j1,k1,l1,m1)     &
         +                        Dx1L*a_I5(i2,j1,k1,l1,m1))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k1,l1,m1)     &
         +                        Dx1L*a_I5(i2,j2,k1,l1,m1)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_I5(i1,j1,k2,l1,m1)     &
         +                        Dx1L*a_I5(i2,j1,k2,l1,m1))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k2,l1,m1)     &
         +                        Dx1L*a_I5(i2,j2,k2,l1,m1))))  &
         +      Dx4L*(Dx3R*(Dx2R*(Dx1R*a_I5(i1,j1,k1,l2,m1)     &
         +                        Dx1L*a_I5(i2,j1,k1,l2,m1))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k1,l2,m1)     &
         +                        Dx1L*a_I5(i2,j2,k1,l2,m1)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_I5(i1,j1,k2,l2,m1)     &
         +                        Dx1L*a_I5(i2,j1,k2,l2,m1))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k2,l2,m1)     &
         +                        Dx1L*a_I5(i2,j2,k2,l2,m1))))) &
         +Dx5L*(Dx4R*(Dx3R*(Dx2R*(Dx1R*a_I5(i1,j1,k1,l1,m2)     &
         +                        Dx1L*a_I5(i2,j1,k1,l1,m2))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k1,l1,m2)     &
         +                        Dx1L*a_I5(i2,j2,k1,l1,m2)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_I5(i1,j1,k2,l1,m2)     &
         +                        Dx1L*a_I5(i2,j1,k2,l1,m2))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k2,l1,m2)     &
         +                        Dx1L*a_I5(i2,j2,k2,l1,m2))))  &
         +      Dx4L*(Dx3R*(Dx2R*(Dx1R*a_I5(i1,j1,k1,l2,m2)     &
         +                        Dx1L*a_I5(i2,j1,k1,l2,m2))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k1,l2,m2)     &
         +                        Dx1L*a_I5(i2,j2,k1,l2,m2)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_I5(i1,j1,k2,l2,m2)     &
         +                        Dx1L*a_I5(i2,j1,k2,l2,m2))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k2,l2,m2)     &
         +                        Dx1L*a_I5(i2,j2,k2,l2,m2)))))

  end function pentalinear_scalar
  !============================================================================
  real function pentalinear_scalar_real4( &
       a_I5, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, mMin, mMax, &
       x_D, x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate pentalinear interpolation of single prec. a_I5 at position x_D

    integer, intent(in):: &
         iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, mMin, mMax
    real(Real4_), intent(in):: &
         a_I5(iMin:iMax,jMin:jMax,kMin:kMax,lMin:lMax,mMin:mMax)

    real,    intent(in), optional:: x_D(5), Dist_D(5), &
         x1_I(iMin:), x2_I(jMin:), x3_I(kMin:), x4_I(lMin:), x5_I(mMin:)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional :: iCell_D(5)

    integer :: i1, i2, j1, j2, k1, k2, l1, l2, m1, m2
    real    :: Dx1L, Dx1R, Dx2L, Dx2R, Dx3L, Dx3R, Dx4L, Dx4R, Dx5L, Dx5R

    character(len=*), parameter:: NameSub = 'pentalinear_scalar_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1L = Dist_D(1)
       Dx1R = 1.0 - Dx1L

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dx2L = Dist_D(2)
       Dx2R = 1.0 - Dx2L

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dx3L = Dist_D(3)
       Dx3R = 1.0 - Dx3L

       l1 = iCell_D(4)
       l2 = l1 + 1
       Dx4L = Dist_D(4)
       Dx4R = 1.0 - Dx4L

       m1 = iCell_D(5)
       m2 = m1 + 1
       Dx5L = Dist_D(5)
       Dx5R = 1.0 - Dx5L

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x_D(1), i1, Dx1L, x1_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, x_D(2), j1, Dx2L, x2_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, x_D(3), k1, Dx3L, x3_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       call find_cell(lMin, lMax, x_D(4), l1, Dx4L, x4_I, DoExtrapolate, &
            "Called for coord4 from "//NameSub)

       call find_cell(mMin, mMax, x_D(5), m1, Dx5L, x5_I, DoExtrapolate, &
            "Called for coord5 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx1R = 1.0 - Dx1L
       j2 = j1 + 1; Dx2R = 1.0 - Dx2L
       k2 = k1 + 1; Dx3R = 1.0 - Dx3L
       l2 = l1 + 1; Dx4R = 1.0 - Dx4L
       m2 = m1 + 1; Dx5R = 1.0 - Dx5L

    end if

    pentalinear_scalar_real4 = &
         +Dx5R*(Dx4R*(Dx3R*(Dx2R*(Dx1R*a_I5(i1,j1,k1,l1,m1)     &
         +                        Dx1L*a_I5(i2,j1,k1,l1,m1))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k1,l1,m1)     &
         +                        Dx1L*a_I5(i2,j2,k1,l1,m1)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_I5(i1,j1,k2,l1,m1)     &
         +                        Dx1L*a_I5(i2,j1,k2,l1,m1))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k2,l1,m1)     &
         +                        Dx1L*a_I5(i2,j2,k2,l1,m1))))  &
         +      Dx4L*(Dx3R*(Dx2R*(Dx1R*a_I5(i1,j1,k1,l2,m1)     &
         +                        Dx1L*a_I5(i2,j1,k1,l2,m1))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k1,l2,m1)     &
         +                        Dx1L*a_I5(i2,j2,k1,l2,m1)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_I5(i1,j1,k2,l2,m1)     &
         +                        Dx1L*a_I5(i2,j1,k2,l2,m1))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k2,l2,m1)     &
         +                        Dx1L*a_I5(i2,j2,k2,l2,m1))))) &
         +Dx5L*(Dx4R*(Dx3R*(Dx2R*(Dx1R*a_I5(i1,j1,k1,l1,m2)     &
         +                        Dx1L*a_I5(i2,j1,k1,l1,m2))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k1,l1,m2)     &
         +                        Dx1L*a_I5(i2,j2,k1,l1,m2)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_I5(i1,j1,k2,l1,m2)     &
         +                        Dx1L*a_I5(i2,j1,k2,l1,m2))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k2,l1,m2)     &
         +                        Dx1L*a_I5(i2,j2,k2,l1,m2))))  &
         +      Dx4L*(Dx3R*(Dx2R*(Dx1R*a_I5(i1,j1,k1,l2,m2)     &
         +                        Dx1L*a_I5(i2,j1,k1,l2,m2))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k1,l2,m2)     &
         +                        Dx1L*a_I5(i2,j2,k1,l2,m2)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_I5(i1,j1,k2,l2,m2)     &
         +                        Dx1L*a_I5(i2,j1,k2,l2,m2))    &
         +                  Dx2L*(Dx1R*a_I5(i1,j2,k2,l2,m2)     &
         +                        Dx1L*a_I5(i2,j2,k2,l2,m2)))))
  end function pentalinear_scalar_real4
  !============================================================================
  function interpolate_vector( &
       a_VC, nVar, nDim, Min_D, Max_D, x_D, &
       x1_I, x2_I, x3_I, x4_I, x5_I, &
       x8_D, x8_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Interpolate default precision real vector array of up to 5 dimensions
    ! Double precision coordinate x8_D and x8_I can be used in 1D only
    ! Index and distance can be returned in 1D only

    real, intent(in):: a_VC(*)
    integer, intent(in):: nVar
    integer, intent(in):: nDim
    integer, intent(in):: Min_D(nDim), Max_D(nDim)

    real,    optional, intent(in):: x_D(nDim)
    real,    optional, intent(in):: x1_I(:), x2_I(:), x3_I(:), x4_I(:), x5_I(:)
    real, optional, intent(in):: x8_D(nDim)
    real, optional, intent(in):: x8_I(:)
    logical, optional, intent(in):: DoExtrapolate
    integer, optional, intent(in):: iCell_D(nDim)
    real,    optional, intent(in):: Dist_D(nDim)

    ! return value
    real:: interpolate_vector(nVar)

    character(len=*), parameter:: NameSub = 'interpolate_vector'
    !--------------------------------------------------------------------------
    select case(nDim)
    case(1)
       if(present(iCell_D))then
          interpolate_vector = linear_vector( a_VC, nVar, &
               Min_D(1), Max_D(1), iCell=iCell_D(1), Dist=Dist_D(1))
       elseif(present(x8_D))then
          interpolate_vector = linear_vector( a_VC, nVar, &
               Min_D(1), Max_D(1), x8=x8_D(1), x8_I=x8_I, &
               DoExtrapolate=DoExtrapolate)
       else
          interpolate_vector = linear_vector( a_VC, nVar, &
               Min_D(1), Max_D(1), x_D(1), x1_I, DoExtrapolate=DoExtrapolate)
       end if
    case(2)
       interpolate_vector = bilinear_vector( a_VC, nVar, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), &
            x_D, x1_I, x2_I, DoExtrapolate, iCell_D, Dist_D)
    case(3)
       interpolate_vector = trilinear_vector( a_VC, nVar, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
            x_D, x1_I, x2_I, x3_I, DoExtrapolate, iCell_D, Dist_D)
    case(4)
       interpolate_vector = quadlinear_vector( a_VC, nVar, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
            Min_D(4), Max_D(4), &
            x_D, x1_I, x2_I, x3_I, x4_I, DoExtrapolate, iCell_D, Dist_D)
    case(5)
       interpolate_vector = pentalinear_vector( a_VC, nVar, &
            Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
            Min_D(4), Max_D(4), Min_D(5), Max_D(5), &
            x_D, x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
    case default
#ifndef _OPENACC
       call CON_stop(NameSub//' nDim should be 1 to 5')
#endif
    end select

  end function interpolate_vector
  !============================================================================
  function interpolate_vector4( &
       a_VC, nVar, nDim, Min_D, Max_D, x_D, &
       x1_I, x2_I, x3_I, x4_I, x5_I, &
       x8_D, x8_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Interpolate single precision real vector array of up to 5 dimensions
    ! Double precision coordinate x8_D and x8_I can be used in 1D only
    ! Index and distance can be returned in 1D only

    real(Real4_), intent(in):: a_VC(*) ! single precision
    integer,      intent(in):: nVar
    integer,      intent(in):: nDim
    integer,      intent(in):: Min_D(nDim), Max_D(nDim)

    real,    optional, intent(in):: x_D(nDim)
    real,    optional, intent(in):: x1_I(:), x2_I(:), x3_I(:), x4_I(:), x5_I(:)
    real, optional, intent(in):: x8_D(nDim)
    real, optional, intent(in):: x8_I(:)
    logical, optional, intent(in):: DoExtrapolate
    integer, optional, intent(in):: iCell_D(nDim)
    real,    optional, intent(in):: Dist_D(nDim)

    ! return value
    real:: interpolate_vector4(nVar)

    character(len=*), parameter:: NameSub = 'interpolate_vector4'
    !--------------------------------------------------------------------------
    select case(nDim)
     case(1)
        if(present(iCell_D))then
           interpolate_vector4 = linear_vector_real4( a_VC, nVar, &
                Min_D(1), Max_D(1), iCell=iCell_D(1), Dist=Dist_D(1))
        elseif(present(x8_D))then
           interpolate_vector4 = linear_vector_real4( a_VC, nVar, &
                Min_D(1), Max_D(1), x8=x8_D(1), x8_I=x8_I, &
                DoExtrapolate=DoExtrapolate)
        else
           interpolate_vector4 = linear_vector_real4( a_VC, nVar, &
                Min_D(1), Max_D(1), x_D(1), x1_I, DoExtrapolate=DoExtrapolate)
        end if
     case(2)
        interpolate_vector4 = bilinear_vector_real4( a_VC, nVar, &
             Min_D(1), Max_D(1), Min_D(2), Max_D(2), &
             x_D, x1_I, x2_I, DoExtrapolate, iCell_D, Dist_D)
     case(3)
        interpolate_vector4 = trilinear_vector_real4( a_VC, nVar, &
             Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
             x_D, x1_I, x2_I, x3_I, DoExtrapolate, iCell_D, Dist_D)
     case(4)
        interpolate_vector4 = quadlinear_vector_real4( a_VC, nVar, &
             Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
             Min_D(4), Max_D(4), &
             x_D, x1_I, x2_I, x3_I, x4_I, DoExtrapolate, iCell_D, Dist_D)
     case(5)
        interpolate_vector4 = pentalinear_vector_real4( a_VC, nVar, &
             Min_D(1), Max_D(1), Min_D(2), Max_D(2), Min_D(3), Max_D(3), &
             Min_D(4), Max_D(4), Min_D(5), Max_D(5), &
             x_D, x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
     case default
#ifndef _OPENACC
        call CON_stop(NameSub//' nDim should be 1 to 5')
#endif
    end select

  end function interpolate_vector4
  !============================================================================
  function linear_vector(a_VI, nVar, iMin, iMax, x, x_I, x8, x8_I, &
       DoExtrapolate, iCell, Dist)
    !$acc routine seq
    ! interpolate default precision real a_VI vector array

    integer, intent(in):: nVar, iMin, iMax
    real, intent(in):: a_VI(nVar,iMin:iMax)

    real,         intent(in), optional :: x, x_I(iMin:), Dist
    real, intent(in), optional :: x8, x8_I(iMin:)
    logical,      intent(in), optional :: DoExtrapolate
    integer,      intent(in), optional :: iCell

    ! return value
    real                :: linear_vector(nVar)

    integer :: i1, i2
    real    :: Dx1, Dx2

    character(len=*), parameter:: NameSub = 'linear_vector'
    !--------------------------------------------------------------------------
    if ( present(iCell) .and. present(Dist) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell
       i2 = i1 + 1
       Dx1 = Dist
       Dx2 = 1.0 - Dx1

    else
       ! Locate the point Xyz_D on the grid
       if(present(x8))then
          call find_cell(iMin, iMax, x8, i1, Dx1, x8_I, DoExtrapolate, &
               "Called from "//NameSub)
       else
          call find_cell(iMin, iMax, x, i1, Dx1, x_I, DoExtrapolate, &
               "Called from "//NameSub)
       end if

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1

    end if

    ! Perform interpolation (or extrapolation) for multiple variables
    linear_vector = Dx2*a_VI(:,i1) + Dx1*a_VI(:,i2)

  end function linear_vector
  !============================================================================
  function linear_vector_real4(a_VI, nVar, iMin, iMax, x, x_I, x8, x8_I, &
       DoExtrapolate, iCell, Dist)
    !$acc routine seq
    ! interpolate single precision real a_VI vector array

    integer,      intent(in):: nVar, iMin, iMax
    real(Real4_), intent(in):: a_VI(nVar,iMin:iMax)

    real,         intent(in), optional :: x, x_I(iMin:), Dist
    real, intent(in), optional :: x8, x8_I(iMin:)
    logical,      intent(in), optional :: DoExtrapolate
    integer,      intent(in), optional :: iCell

    ! return value
    real:: linear_vector_real4(nVar)

    integer :: i1, i2
    real    :: Dx1, Dx2

    character(len=*), parameter:: NameSub = 'linear_vector_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell) .and. present(Dist) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell
       i2 = i1 + 1
       Dx1 = Dist
       Dx2 = 1.0 - Dx1

    else
       ! Locate the point Xyz_D on the grid
       if(present(x8))then
          call find_cell(iMin, iMax, x8, i1, Dx1, x8_I, DoExtrapolate, &
               "Called from "//NameSub)
       else
          call find_cell(iMin, iMax, x, i1, Dx1, x_I, DoExtrapolate, &
               "Called from "//NameSub)
       end if

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1

    end if

    ! Perform interpolation (or extrapolation) for multiple variables
    linear_vector_real4 = Dx2*a_VI(:,i1) + Dx1*a_VI(:,i2)
  end function linear_vector_real4
  !============================================================================
  function bilinear_vector( &
       a_VII, nVar, iMin, iMax, jMin, jMax, Xy_D, x_I, y_I, DoExtrapolate, &
       iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate bilinear interpolation of default precision real a_VII

    integer, intent(in):: nVar, iMin, iMax, jMin, jMax
    real, intent(in):: a_VII(nVar, iMin:iMax,jMin:jMax)

    real,    intent(in), optional:: Xy_D(2), x_I(iMin:), y_I(jMin:), Dist_D(2)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(2)

    ! return value
    real                :: bilinear_vector(nVar)

    integer :: i1, i2, j1, j2
    real :: Dx1, Dx2, Dy1, Dy2

    character(len=*), parameter:: NameSub = 'bilinear_vector'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1 = Dist_D(1)
       Dx2 = 1.0 - Dx1

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dy1 = Dist_D(2)
       Dy2 = 1.0 - Dy1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, Xy_D(1), i1, Dx1, x_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, Xy_D(2), j1, Dy1, y_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1
       j2 = j1 + 1; Dy2 = 1.0 - Dy1

    end if

    ! Perform interpolation (or extrapolation) for multiple variables
    bilinear_vector = Dy2*( Dx2*a_VII(:,i1,j1)   &
         +                  Dx1*a_VII(:,i2,j1))  &
         +            Dy1*( Dx2*a_VII(:,i1,j2)   &
         +                  Dx1*a_VII(:,i2,j2))
  end function bilinear_vector
  !============================================================================
  function bilinear_vector_real4( &
       a_VII, nVar, iMin, iMax, jMin, jMax, Xy_D, x_I, y_I, DoExtrapolate, &
       iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate bilinear interpolation of default precision real a_VII

    integer,      intent(in):: nVar, iMin, iMax, jMin, jMax
    real(Real4_), intent(in):: a_VII(nVar, iMin:iMax,jMin:jMax)

    real,    intent(in), optional:: Xy_D(2), x_I(iMin:), y_I(jMin:), Dist_D(2)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(2)

    ! return value
    real                :: bilinear_vector_real4(nVar)

    integer :: i1, i2, j1, j2
    real :: Dx1, Dx2, Dy1, Dy2

    character(len=*), parameter:: NameSub = 'bilinear_vector_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1 = Dist_D(1)
       Dx2 = 1.0 - Dx1

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dy1 = Dist_D(2)
       Dy2 = 1.0 - Dy1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, Xy_D(1), i1, Dx1, x_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, Xy_D(2), j1, Dy1, y_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1
       j2 = j1 + 1; Dy2 = 1.0 - Dy1

    end if

    ! Perform interpolation (or extrapolation) for multiple variables
    bilinear_vector_real4 = Dy2*( Dx2*a_VII(:,i1,j1)   &
         +                  Dx1*a_VII(:,i2,j1))  &
         +            Dy1*( Dx2*a_VII(:,i1,j2)   &
         +                  Dx1*a_VII(:,i2,j2))
  end function bilinear_vector_real4
  !============================================================================
  function trilinear_vector( &
       a_VIII, nVar, iMin, iMax, jMin, jMax, kMin, kMax, Xyz_D, &
       x_I, y_I, z_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate trilinear interpolation of a_III at position Xyz_D

    integer, intent(in):: nVar, iMin, iMax, jMin, jMax, kMin, kMax
    real, intent(in):: a_VIII(nVar,iMin:iMax,jMin:jMax,kMin:kMax)

    real,    intent(in), optional:: &
         Xyz_D(3), x_I(iMin:), y_I(jMin:), z_I(kMin:), Dist_D(3)
    logical, intent(in), optional:: DoExtrapolate
    integer,    intent(in), optional :: iCell_D(3)

    ! return value
    real :: trilinear_vector(nVar)

    integer :: i1, i2, j1, j2, k1, k2
    real    :: Dx1, Dx2, Dy1, Dy2, Dz1, Dz2
    character(len=*), parameter:: NameSub = 'trilinear_vector'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1 = Dist_D(1)
       Dx2 = 1.0 - Dx1

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dy1 = Dist_D(2)
       Dy2 = 1.0 - Dy1

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dz1 = Dist_D(3)
       Dz2 = 1.0 - Dz1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, Xyz_D(1), i1, Dx1, x_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, Xyz_D(2), j1, Dy1, y_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, Xyz_D(3), k1, Dz1, z_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1
       j2 = j1 + 1; Dy2 = 1.0 - Dy1
       k2 = k1 + 1; Dz2 = 1.0 - Dz1

    end if

    trilinear_vector = Dz2*(Dy2*(Dx2*a_VIII(:,i1,j1,k1)   &
         +                       Dx1*a_VIII(:,i2,j1,k1))  &
         +                  Dy1*(Dx2*a_VIII(:,i1,j2,k1)   &
         +                       Dx1*a_VIII(:,i2,j2,k1))) &
         +             Dz1*(Dy2*(Dx2*a_VIII(:,i1,j1,k2)   &
         +                       Dx1*a_VIII(:,i2,j1,k2))  &
         +                  Dy1*(Dx2*a_VIII(:,i1,j2,k2)   &
         +                       Dx1*a_VIII(:,i2,j2,k2)))
  end function trilinear_vector
  !============================================================================
  function trilinear_vector_real4( &
       a_VIII, nVar, iMin, iMax, jMin, jMax, kMin, kMax, Xyz_D, &
       x_I, y_I, z_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate trilinear interpolation of a_III at position Xyz_D

    integer,      intent(in):: nVar, iMin, iMax, jMin, jMax, kMin, kMax
    real(Real4_), intent(in):: a_VIII(nVar,iMin:iMax,jMin:jMax,kMin:kMax)

    real,    intent(in), optional:: &
         Xyz_D(3), x_I(iMin:), y_I(jMin:), z_I(kMin:), Dist_D(3)
    logical, intent(in), optional:: DoExtrapolate
    integer,    intent(in), optional :: iCell_D(3)

    ! return value
    real :: trilinear_vector_real4(nVar)

    integer :: i1, i2, j1, j2, k1, k2
    real    :: Dx1, Dx2, Dy1, Dy2, Dz1, Dz2
    character(len=*), parameter:: NameSub = 'trilinear_vector_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1 = Dist_D(1)
       Dx2 = 1.0 - Dx1

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dy1 = Dist_D(2)
       Dy2 = 1.0 - Dy1

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dz1 = Dist_D(3)
       Dz2 = 1.0 - Dz1

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, Xyz_D(1), i1, Dx1, x_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, Xyz_D(2), j1, Dy1, y_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, Xyz_D(3), k1, Dz1, z_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx2 = 1.0 - Dx1
       j2 = j1 + 1; Dy2 = 1.0 - Dy1
       k2 = k1 + 1; Dz2 = 1.0 - Dz1

    end if

    trilinear_vector_real4 = Dz2*(Dy2*(Dx2*a_VIII(:,i1,j1,k1)   &
         +                       Dx1*a_VIII(:,i2,j1,k1))  &
         +                  Dy1*(Dx2*a_VIII(:,i1,j2,k1)   &
         +                       Dx1*a_VIII(:,i2,j2,k1))) &
         +             Dz1*(Dy2*(Dx2*a_VIII(:,i1,j1,k2)   &
         +                       Dx1*a_VIII(:,i2,j1,k2))  &
         +                  Dy1*(Dx2*a_VIII(:,i1,j2,k2)   &
         +                       Dx1*a_VIII(:,i2,j2,k2)))
  end function trilinear_vector_real4
  !============================================================================
  function quadlinear_vector( &
       a_VI4, nVar, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, x_D, &
       x1_I, x2_I, x3_I, x4_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate quadlinear interpolation of single precision a_I4

    integer, intent(in):: nVar, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax
    real, intent(in):: &
         a_VI4(nVar,iMin:iMax,jMin:jMax,kMin:kMax,lMin:lMax)
    real,    intent(in), optional:: &
         x_D(4), x1_I(iMin:), x2_I(jMin:), x3_I(kMin:), x4_I(lMin:), Dist_D(4)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(4)

    ! return value
    real :: quadlinear_vector(nVar)

    integer :: i1, i2, j1, j2, k1, k2, l1, l2
    real    :: Dx1L, Dx1R, Dx2L, Dx2R, Dx3L, Dx3R, Dx4L, Dx4R
    character(len=*), parameter:: NameSub = 'quadlinear_vector'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1L = Dist_D(1)
       Dx1R = 1.0 - Dx1L

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dx2L = Dist_D(2)
       Dx2R = 1.0 - Dx2L

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dx3L = Dist_D(3)
       Dx3R = 1.0 - Dx3L

       l1 = iCell_D(4)
       l2 = l1 + 1
       Dx4L = Dist_D(4)
       Dx4R = 1.0 - Dx4L

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x_D(1), i1, Dx1L, x1_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, x_D(2), j1, Dx2L, x2_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, x_D(3), k1, Dx3L, x3_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       call find_cell(lMin, lMax, x_D(4), l1, Dx4L, x4_I, DoExtrapolate, &
            "Called for coord4 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx1R = 1.0 - Dx1L
       j2 = j1 + 1; Dx2R = 1.0 - Dx2L
       k2 = k1 + 1; Dx3R = 1.0 - Dx3L
       l2 = l1 + 1; Dx4R = 1.0 - Dx4L

    end if

    quadlinear_vector = &
         +Dx4R*(Dx3R*(Dx2R*(Dx1R*a_VI4(:,i1,j1,k1,l1)   &
         +                  Dx1L*a_VI4(:,i2,j1,k1,l1))  &
         +            Dx2L*(Dx1R*a_VI4(:,i1,j2,k1,l1)   &
         +                  Dx1L*a_VI4(:,i2,j2,k1,l1))) &
         +      Dx3L*(Dx2R*(Dx1R*a_VI4(:,i1,j1,k2,l1)   &
         +                  Dx1L*a_VI4(:,i2,j1,k2,l1))  &
         +            Dx2L*(Dx1R*a_VI4(:,i1,j2,k2,l1)   &
         +                  Dx1L*a_VI4(:,i2,j2,k2,l1))))&
         +Dx4L*(Dx3R*(Dx2R*(Dx1R*a_VI4(:,i1,j1,k1,l2)   &
         +                  Dx1L*a_VI4(:,i2,j1,k1,l2))  &
         +            Dx2L*(Dx1R*a_VI4(:,i1,j2,k1,l2)   &
         +                  Dx1L*a_VI4(:,i2,j2,k1,l2))) &
         +      Dx3L*(Dx2R*(Dx1R*a_VI4(:,i1,j1,k2,l2)   &
         +                  Dx1L*a_VI4(:,i2,j1,k2,l2))  &
         +            Dx2L*(Dx1R*a_VI4(:,i1,j2,k2,l2)   &
         +                  Dx1L*a_VI4(:,i2,j2,k2,l2))))
  end function quadlinear_vector
  !============================================================================
  function quadlinear_vector_real4( &
       a_VI4, nVar, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, x_D, &
       x1_I, x2_I, x3_I, x4_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate quadlinear interpolation of single precision a_I4

    integer, intent(in):: nVar, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax
    real(Real4_), intent(in):: &
         a_VI4(nVar,iMin:iMax,jMin:jMax,kMin:kMax,lMin:lMax)
    real,    intent(in), optional:: &
         x_D(4), x1_I(iMin:), x2_I(jMin:), x3_I(kMin:), x4_I(lMin:), Dist_D(4)
    logical, intent(in), optional:: DoExtrapolate
    integer, intent(in), optional:: iCell_D(4)

    ! return value
    real :: quadlinear_vector_real4(nVar)

    integer :: i1, i2, j1, j2, k1, k2, l1, l2
    real    :: Dx1L, Dx1R, Dx2L, Dx2R, Dx3L, Dx3R, Dx4L, Dx4R
    character(len=*), parameter:: NameSub = 'quadlinear_vector_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1L = Dist_D(1)
       Dx1R = 1.0 - Dx1L

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dx2L = Dist_D(2)
       Dx2R = 1.0 - Dx2L

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dx3L = Dist_D(3)
       Dx3R = 1.0 - Dx3L

       l1 = iCell_D(4)
       l2 = l1 + 1
       Dx4L = Dist_D(4)
       Dx4R = 1.0 - Dx4L

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x_D(1), i1, Dx1L, x1_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, x_D(2), j1, Dx2L, x2_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, x_D(3), k1, Dx3L, x3_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       call find_cell(lMin, lMax, x_D(4), l1, Dx4L, x4_I, DoExtrapolate, &
            "Called for coord4 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx1R = 1.0 - Dx1L
       j2 = j1 + 1; Dx2R = 1.0 - Dx2L
       k2 = k1 + 1; Dx3R = 1.0 - Dx3L
       l2 = l1 + 1; Dx4R = 1.0 - Dx4L

    end if

    quadlinear_vector_real4 = &
         +Dx4R*(Dx3R*(Dx2R*(Dx1R*a_VI4(:,i1,j1,k1,l1)   &
         +                  Dx1L*a_VI4(:,i2,j1,k1,l1))  &
         +            Dx2L*(Dx1R*a_VI4(:,i1,j2,k1,l1)   &
         +                  Dx1L*a_VI4(:,i2,j2,k1,l1))) &
         +      Dx3L*(Dx2R*(Dx1R*a_VI4(:,i1,j1,k2,l1)   &
         +                  Dx1L*a_VI4(:,i2,j1,k2,l1))  &
         +            Dx2L*(Dx1R*a_VI4(:,i1,j2,k2,l1)   &
         +                  Dx1L*a_VI4(:,i2,j2,k2,l1))))&
         +Dx4L*(Dx3R*(Dx2R*(Dx1R*a_VI4(:,i1,j1,k1,l2)   &
         +                  Dx1L*a_VI4(:,i2,j1,k1,l2))  &
         +            Dx2L*(Dx1R*a_VI4(:,i1,j2,k1,l2)   &
         +                  Dx1L*a_VI4(:,i2,j2,k1,l2))) &
         +      Dx3L*(Dx2R*(Dx1R*a_VI4(:,i1,j1,k2,l2)   &
         +                  Dx1L*a_VI4(:,i2,j1,k2,l2))  &
         +            Dx2L*(Dx1R*a_VI4(:,i1,j2,k2,l2)   &
         +                  Dx1L*a_VI4(:,i2,j2,k2,l2))))
  end function quadlinear_vector_real4
  !============================================================================
  function pentalinear_vector( &
       a_VI5, nVar, &
       iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, mMin, mMax, x_D, &
       x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate pentalinear interpolation of default precision a_I5

    integer, intent(in):: &
         nVar, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, mMin, mMax
    real, intent(in):: &
         a_VI5(nVar,iMin:iMax,jMin:jMax,kMin:kMax,lMin:lMax,mMin:mMax)
    real,    intent(in), optional:: x_D(5), Dist_D(5), &
         x1_I(iMin:), x2_I(jMin:), x3_I(kMin:), x4_I(lMin:), x5_I(mMin:)
    logical, intent(in), optional:: DoExtrapolate

    integer, intent(in), optional:: iCell_D(5)

    ! return value
    real :: pentalinear_vector(nVar)

    integer :: i1, i2, j1, j2, k1, k2, l1, l2, m1, m2
    real    :: Dx1L, Dx1R, Dx2L, Dx2R, Dx3L, Dx3R, Dx4L, Dx4R, Dx5L, Dx5R
    character(len=*), parameter:: NameSub = 'pentalinear_vector'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1L = Dist_D(1)
       Dx1R = 1.0 - Dx1L

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dx2L = Dist_D(2)
       Dx2R = 1.0 - Dx2L

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dx3L = Dist_D(3)
       Dx3R = 1.0 - Dx3L

       l1 = iCell_D(4)
       l2 = l1 + 1
       Dx4L = Dist_D(4)
       Dx4R = 1.0 - Dx4L

       m1 = iCell_D(5)
       m2 = m1 + 1
       Dx5L = Dist_D(5)
       Dx5R = 1.0 - Dx5L

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x_D(1), i1, Dx1L, x1_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, x_D(2), j1, Dx2L, x2_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, x_D(3), k1, Dx3L, x3_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       call find_cell(lMin, lMax, x_D(4), l1, Dx4L, x4_I, DoExtrapolate, &
            "Called for coord4 from "//NameSub)

       call find_cell(mMin, mMax, x_D(5), m1, Dx5L, x5_I, DoExtrapolate, &
            "Called for coord5 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx1R = 1.0 - Dx1L
       j2 = j1 + 1; Dx2R = 1.0 - Dx2L
       k2 = k1 + 1; Dx3R = 1.0 - Dx3L
       l2 = l1 + 1; Dx4R = 1.0 - Dx4L
       m2 = m1 + 1; Dx5R = 1.0 - Dx5L

    end if

    pentalinear_vector = &
         +Dx5R*(Dx4R*(Dx3R*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k1,l1,m1)     &
         +                        Dx1L*a_VI5(:,i2,j1,k1,l1,m1))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k1,l1,m1)     &
         +                        Dx1L*a_VI5(:,i2,j2,k1,l1,m1)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k2,l1,m1)     &
         +                        Dx1L*a_VI5(:,i2,j1,k2,l1,m1))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k2,l1,m1)     &
         +                        Dx1L*a_VI5(:,i2,j2,k2,l1,m1))))  &
         +      Dx4L*(Dx3R*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k1,l2,m1)     &
         +                        Dx1L*a_VI5(:,i2,j1,k1,l2,m1))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k1,l2,m1)     &
         +                        Dx1L*a_VI5(:,i2,j2,k1,l2,m1)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k2,l2,m1)     &
         +                        Dx1L*a_VI5(:,i2,j1,k2,l2,m1))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k2,l2,m1)     &
         +                        Dx1L*a_VI5(:,i2,j2,k2,l2,m1))))) &
         +Dx5L*(Dx4R*(Dx3R*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k1,l1,m2)     &
         +                        Dx1L*a_VI5(:,i2,j1,k1,l1,m2))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k1,l1,m2)     &
         +                        Dx1L*a_VI5(:,i2,j2,k1,l1,m2)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k2,l1,m2)     &
         +                        Dx1L*a_VI5(:,i2,j1,k2,l1,m2))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k2,l1,m2)     &
         +                        Dx1L*a_VI5(:,i2,j2,k2,l1,m2))))  &
         +      Dx4L*(Dx3R*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k1,l2,m2)     &
         +                        Dx1L*a_VI5(:,i2,j1,k1,l2,m2))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k1,l2,m2)     &
         +                        Dx1L*a_VI5(:,i2,j2,k1,l2,m2)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k2,l2,m2)     &
         +                        Dx1L*a_VI5(:,i2,j1,k2,l2,m2))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k2,l2,m2)     &
         +                        Dx1L*a_VI5(:,i2,j2,k2,l2,m2)))))
  end function pentalinear_vector
  !============================================================================
  function pentalinear_vector_real4( &
       a_VI5, nVar, &
       iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, mMin, mMax, x_D, &
       x1_I, x2_I, x3_I, x4_I, x5_I, DoExtrapolate, iCell_D, Dist_D)
    !$acc routine seq
    ! Calculate pentalinear interpolation of single precision a_I5

    integer, intent(in):: &
         nVar, iMin, iMax, jMin, jMax, kMin, kMax, lMin, lMax, mMin, mMax
    real(Real4_), intent(in):: &
         a_VI5(nVar,iMin:iMax,jMin:jMax,kMin:kMax,lMin:lMax,mMin:mMax)
    real,    intent(in), optional:: x_D(5), Dist_D(5), &
         x1_I(iMin:), x2_I(jMin:), x3_I(kMin:), x4_I(lMin:), x5_I(mMin:)
    logical, intent(in), optional:: DoExtrapolate

    integer, intent(in), optional:: iCell_D(5)

    ! return value
    real :: pentalinear_vector_real4(nVar)

    integer :: i1, i2, j1, j2, k1, k2, l1, l2, m1, m2
    real    :: Dx1L, Dx1R, Dx2L, Dx2R, Dx3L, Dx3R, Dx4L, Dx4R, Dx5L, Dx5R
    character(len=*), parameter:: NameSub = 'pentalinear_vector_real4'
    !--------------------------------------------------------------------------
    if ( present(iCell_D) .and. present(Dist_D) ) then

       ! Calculate the remaining cell indices and interpolation weights

       i1 = iCell_D(1)
       i2 = i1 + 1
       Dx1L = Dist_D(1)
       Dx1R = 1.0 - Dx1L

       j1 = iCell_D(2)
       j2 = j1 + 1
       Dx2L = Dist_D(2)
       Dx2R = 1.0 - Dx2L

       k1 = iCell_D(3)
       k2 = k1 + 1
       Dx3L = Dist_D(3)
       Dx3R = 1.0 - Dx3L

       l1 = iCell_D(4)
       l2 = l1 + 1
       Dx4L = Dist_D(4)
       Dx4R = 1.0 - Dx4L

       m1 = iCell_D(5)
       m2 = m1 + 1
       Dx5L = Dist_D(5)
       Dx5R = 1.0 - Dx5L

    else

       ! Locate the point Xyz_D on the grid
       call find_cell(iMin, iMax, x_D(1), i1, Dx1L, x1_I, DoExtrapolate, &
            "Called for coord1 from "//NameSub)

       call find_cell(jMin, jMax, x_D(2), j1, Dx2L, x2_I, DoExtrapolate, &
            "Called for coord2 from "//NameSub)

       call find_cell(kMin, kMax, x_D(3), k1, Dx3L, x3_I, DoExtrapolate, &
            "Called for coord3 from "//NameSub)

       call find_cell(lMin, lMax, x_D(4), l1, Dx4L, x4_I, DoExtrapolate, &
            "Called for coord4 from "//NameSub)

       call find_cell(mMin, mMax, x_D(5), m1, Dx5L, x5_I, DoExtrapolate, &
            "Called for coord5 from "//NameSub)

       ! Calculate the remaining cell indices and interpolation weights
       i2 = i1 + 1; Dx1R = 1.0 - Dx1L
       j2 = j1 + 1; Dx2R = 1.0 - Dx2L
       k2 = k1 + 1; Dx3R = 1.0 - Dx3L
       l2 = l1 + 1; Dx4R = 1.0 - Dx4L
       m2 = m1 + 1; Dx5R = 1.0 - Dx5L

    end if

    pentalinear_vector_real4 = &
         +Dx5R*(Dx4R*(Dx3R*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k1,l1,m1)     &
         +                        Dx1L*a_VI5(:,i2,j1,k1,l1,m1))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k1,l1,m1)     &
         +                        Dx1L*a_VI5(:,i2,j2,k1,l1,m1)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k2,l1,m1)     &
         +                        Dx1L*a_VI5(:,i2,j1,k2,l1,m1))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k2,l1,m1)     &
         +                        Dx1L*a_VI5(:,i2,j2,k2,l1,m1))))  &
         +      Dx4L*(Dx3R*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k1,l2,m1)     &
         +                        Dx1L*a_VI5(:,i2,j1,k1,l2,m1))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k1,l2,m1)     &
         +                        Dx1L*a_VI5(:,i2,j2,k1,l2,m1)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k2,l2,m1)     &
         +                        Dx1L*a_VI5(:,i2,j1,k2,l2,m1))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k2,l2,m1)     &
         +                        Dx1L*a_VI5(:,i2,j2,k2,l2,m1))))) &
         +Dx5L*(Dx4R*(Dx3R*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k1,l1,m2)     &
         +                        Dx1L*a_VI5(:,i2,j1,k1,l1,m2))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k1,l1,m2)     &
         +                        Dx1L*a_VI5(:,i2,j2,k1,l1,m2)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k2,l1,m2)     &
         +                        Dx1L*a_VI5(:,i2,j1,k2,l1,m2))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k2,l1,m2)     &
         +                        Dx1L*a_VI5(:,i2,j2,k2,l1,m2))))  &
         +      Dx4L*(Dx3R*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k1,l2,m2)     &
         +                        Dx1L*a_VI5(:,i2,j1,k1,l2,m2))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k1,l2,m2)     &
         +                        Dx1L*a_VI5(:,i2,j2,k1,l2,m2)))   &
         +            Dx3L*(Dx2R*(Dx1R*a_VI5(:,i1,j1,k2,l2,m2)     &
         +                        Dx1L*a_VI5(:,i2,j1,k2,l2,m2))    &
         +                  Dx2L*(Dx1R*a_VI5(:,i1,j2,k2,l2,m2)     &
         +                        Dx1L*a_VI5(:,i2,j2,k2,l2,m2)))))

  end function pentalinear_vector_real4
  !============================================================================
  subroutine fit_parabola(x_I, y_I, &
       xExtremumOut, yExtremumOut, Weight2Out_I, Weight3Out_I)

    ! Given 3 discrete points at x_D and 3 function values y_D
    ! with the middle point being the discrete extrem value,
    ! find the extremum value of the parabola going through the points,
    ! and the 3rd order interpolation weights to interpolate to this point

    real, intent(in)           :: x_I(3)         ! coordinates
    real, intent(in)           :: y_I(3)         ! values
    real, intent(out), optional:: xExtremumOut   ! coordinate of extremum
    real, intent(out), optional:: yExtremumOut   ! value of extremum
    real, intent(out), optional:: Weight2Out_I(3)! weights for 2nd order interp
    real, intent(out), optional:: Weight3Out_I(3)! weights for 3rd order interp

    real:: xE, yE          ! coordinates of extremum
    real:: x1, y1, x3, y3  ! shifted coordinates of points 1 and 3
    real:: s1, s3          ! slopes of 1-2 and 2-3 segments

    real:: Ratio, Area2

    character(len=*), parameter:: NameSub = 'fit_parabola'
    !--------------------------------------------------------------------------
    ! Shift coordinates so that x2 = 0
    x1 = x_I(1) - x_I(2)
    x3 = x_I(3) - x_I(2)

    ! Shift values so that y2 = 0
    y1 = y_I(1) - y_I(2)
    y3 = y_I(3) - y_I(2)

    if(x1 == 0.0 .or. x3 == 0.0)then
       write(*,*) NameSub,': x_I=', x_I,' y_I=', y_I
       call CON_stop(NameSub//' error in coordinates')
    end if

    ! Calculate slopes
    s1 = y1/x1
    s3 = y3/x3

    if(s1*s3 > 0.0)then
       write(*,*) NameSub,': x_I=', x_I,' y_I=', y_I
       call CON_stop(NameSub//' error: midpoint is not an extremum')
    end if

    ! Find the position where the line connecting
    ! the (x1/2, s1) and (x3/2, s3) points intersects the X axis.
    ! This is where the slope of the parabola is zero

    xE = 0.5*x1 + 0.5*(x3 - x1)*s1/(s1 - s3)

    if(present(xExtremumOut)) xExtremumOut = xE + x_I(2)

    if(present(Weight2Out_I))then
       ! Use the two points surrounding xE for linear interpolation
       if(xE > 0.0) then
          Weight2Out_I(1) = 0.0
          Weight2Out_I(3) = xE/x3
          Weight2Out_I(2) = 1.0 - Weight2Out_I(3)
       else
          Weight2Out_I(3) = 0.0
          Weight2Out_I(1) = xE/x1
          Weight2Out_I(2) = 1.0 - Weight2Out_I(1)
       end if
    end if

    if(present(yExtremumOut) .or. present(Weight3Out_I))then
       ! Find the value of the parabola y = a*(x-xE)**2 + yE at the extremum
       ! We can use any 2 of the points to solve for yE.
       if(xE > 0.0)then
          Ratio = xE**2/(xE - x1)**2
          yE = Ratio*y1/(Ratio - 1.0)
       else
          Ratio = xE**2/(x3 - xE)**2
          yE = Ratio*y3/(Ratio - 1.0)
       end if
       if(present(yExtremumOut)) yExtremumOut = yE + y_I(2)

       if(present(Weight3Out_I))then

          ! Calculate 3rd order interpolation weights from the 3 points
          ! to the location of the extremum.
          ! We use the fact that the parabola is an exact solution.

          ! Twice the area of the triangle with sign  (x1,y1) x (x3,y3)
          Area2 = x1*y3 - y1*x3

          ! For points 1 and 3 the weight is the fraction of the triangle
          ! Area(2,3,E)/Area(1,2,3) and Area(1,2,E)/Area(1,2,3)
          Weight3Out_I(1) =  (xE*y3 - yE*x3)/Area2
          Weight3Out_I(3) = -(xE*y1 - yE*x1)/Area2

          ! For point 2 we use that the sum of weights must be 1
          Weight3Out_I(2) = 1.0 - Weight3Out_I(1) - Weight3Out_I(3)
       end if
    end if

  end subroutine fit_parabola
  !============================================================================
  subroutine test_interpolation

    real :: a_I(0:2) = [ 10., 20., 30. ]

    real :: a_II(2,3) = reshape([ 1., 20., 3., 40., 5., 60. ], [2, 3] )

    real :: a_VII(2,2,3) = reshape( &
         [1., 10., 20., 200., 3., 30., 40., 400., 5., 50., 60., 600.], &
         [2, 2, 3])

    real(Real4_) :: a_III(2,2,0:2) = reshape([ &
         1., 20., 3., 40., &
         100., 2000., 300., 4000., &
         10000., 200000., 30000., 400000. ], [2, 2, 3])

    real :: a_VIII(2,2,2,0:2) = reshape([ &
         1., -10., 20., -200., 3., -30., 40., -400., &
         100., -1000., 2000., -20000., 300., -3000., 4000., -40000.,  &
         1e4, -1e5, 2e5, -2e6, 3e4, -3e5, 4e5, -4e6 ], [2, 2, 2, 3])

    real :: x12_I(1:2) = [ 1., 2.]
    real :: x13_I(1:3) = [ 1., 2., 4.]
    real :: x02_I(0:2) = [ 1., 2., 4.]

    integer, parameter:: MinCoord = 1, MaxCoord = 8
    real   :: Coord_I(MinCoord:MaxCoord) = &
         [ 1.0, 2.0, 4.0, 8.0, 16.0, 17.0, 24.0, 25.0 ]
    integer:: nCoord, iSign
    real   :: Coord, dCoord, CoordMin, CoordMax
    integer:: i, iCoord
    logical:: IsInside

    real :: Result, GoodResult, Result_V(2), GoodResult_V(2)

    ! Variables for fit_parabola test
    real:: x_I(3), y_I(3), xMin, yMin, xExtremum, yExtremum
    real:: Weight2_I(3), Weight3_I(3)

    ! Change sign of coordinates to test for increasing and decreasing orders
    character(len=*), parameter:: NameSub = 'test_interpolation'
    !--------------------------------------------------------------------------
    do iSign = 1, -1, -2
       if(iSign == 1)then
          write(*,'(a)')'Testing find_cell for increasing coordinates'
       else
          write(*,'(a)')'Testing find_cell for decreasing coordinates'
       end if

       ! Change number of coordinates to test binary search
       do nCoord = MaxCoord/2, MaxCoord

          ! Search for all integer coordinates
          ! starting below and finishing above the coordinate range

          CoordMin = min(Coord_I(MinCoord), Coord_I(nCoord))
          CoordMax = max(Coord_I(MinCoord), Coord_I(nCoord))

          do i = ceiling(CoordMin) - 1, floor(CoordMax) + 1
             Coord = i
             call find_cell(MinCoord, nCoord, Coord, &
                  iCoord, dCoord, Coord_I, .false., &
                  'Called from '//NameSub, IsInside)

             if(iSign*Coord < iSign*Coord_I(MinCoord))then
                if(IsInside) write(*,*) &
                     'Test failed for nCoord, Coord=', nCoord, Coord, &
                     ', IsInside=T, should be false'
                if(iCoord /= MinCoord) write(*,*)&
                     'Test failed for nCoord, Coord=', nCoord, Coord, &
                     ', iCoord=', iCoord, ' should be ', MinCoord
                if(dCoord /= 0.0) write(*,*) &
                     'Test failed for nCoord, Coord=', nCoord, Coord, &
                     ', dCoord=', dCoord, ' should be 0.0'
                CYCLE
             end if
             if(iSign*Coord > iSign*Coord_I(nCoord))then
                if(IsInside) write(*,*) &
                     'Test failed for nCoord, Coord=', nCoord, Coord, &
                     ', IsInside=T, should be false'
                if(iCoord /= nCoord - 1) write(*,*) &
                     'Test failed for nCoord, Coord=', nCoord, Coord, &
                     ', iCoord=', iCoord, ' should be ', nCoord - 1
                if(dCoord /= 1.0) write(*,*) &
                     'Test failed for nCoord, Coord=', nCoord, Coord, &
                     ', dCoord=', dCoord, ' should be 1.0'
                CYCLE
             end if
             if(.not.IsInside) write(*,*) &
                  'Test failed for nCoord, Coord=', nCoord, Coord, &
                  ', IsInside=F, should be true'

             if(iCoord < MinCoord .or. iCoord > nCoord-1) then
                write(*,*) &
                     'Test failed for nCoord, Coord=', nCoord, Coord, &
                     ', iCoord=', iCoord, ' should be < ', MinCoord, &
                     ' and > ', nCoord - 1
                CYCLE
             end if

             if(iSign*Coord_I(iCoord) > iSign*Coord) write(*,*) &
                  'Test failed for nCoord, Coord=', nCoord, Coord, &
                  ', iSign*Coord_I(iCoord)=', iSign*Coord_I(iCoord), &
                  ' should be <= iSign*Coord'

             if(iSign*Coord_I(iCoord+1) < iSign*Coord) write(*,*)       &
                  'Test failed for nCoord, Coord=', nCoord, Coord, &
                  ', iSign*Coord_I(iCoord+1)=', iSign*Coord_I(iCoord+1), &
                  ' should be >= iSign*Coord'
             if(abs(Coord_I(iCoord) &
                  + dCoord*(Coord_I(iCoord+1) - Coord_I(iCoord)) &
                  - Coord) > 1e-6) write(*,*) &
                  'Test failed for nCoord, Coord=', nCoord, Coord, &
                  ', Coord_I(iCoord:iCoord+1)=', Coord_I(iCoord:iCoord+1), &
                  ', but incorrect dCoord = ', dCoord
          end do
       end do
       ! Change signs of coordinates to test decreasing order
       Coord_I = -Coord_I
    end do

    ! Test for normal conditions.
    write(*,'(a)')'Testing function linear for uniform grid'
    Result = linear(a_I, 0, 2, 1.1)
    GoodResult = 21.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing function linear for non-uniform grid'
    Result = linear(a_I, 0, 2, 2.2, x02_I)
    GoodResult = 21.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing function bilinear for uniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [1.1, 2.2])
    GoodResult = 7.46
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing function bilinear for non-uniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [1.1, 2.2], x12_I, x13_I)
    GoodResult = 7.08
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing function trilinear for uniform grid'
    Result = trilinear_real4(a_III, 1, 2, 1, 2, 0, 2, [1.1, 1.2, 1.3])
    GoodResult = 11236.2
    if(abs(Result - GoodResult) > 1.e-2) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing function trilinear for nonuniform grid'
    Result = trilinear_real4(a_III, 1, 2, 1, 2, 0, 2, [1.1, 1.2, 1.3], &
         x12_I, x12_I, x02_I)
    GoodResult = 112.362
    if(abs(Result - GoodResult) > 1.e-2) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    ! Test out-of-bounds, no extrapolation
    write(*,'(a)')'Testing bilinear out-of-bounds: +X for uniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [3.,1.], DoExtrapolate=.false.)
    GoodResult = 20.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear out-of-bounds: +X for nonuniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [3.,1.], x12_I, x13_I, &
         DoExtrapolate=.false.)
    GoodResult = 20.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear out-of-bounds: -X for uniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [-3.,2.], DoExtrapolate=.false.)
    GoodResult = 3.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear out-of-bounds: -X for nonuniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [-3.,2.], x12_I, x13_I, &
         DoExtrapolate=.false.)
    GoodResult = 3.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear out-of-bounds: +Y for uniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [1.,6.], DoExtrapolate=.false.)
    GoodResult = 5.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear out-of-bounds: +Y for nonuniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [1.,6.], x12_I, x13_I, &
         DoExtrapolate=.false.)
    GoodResult = 5.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear out-of-bounds: -Y for uniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [2.,-3.], DoExtrapolate=.false.)
    GoodResult = 20.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear out-of-bounds: -Y for nonuniform grid'
    Result = bilinear(a_II, 1, 2, 1, 3, [2.,-3.], x12_I, x13_I, &
         DoExtrapolate=.false.)
    GoodResult = 20.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing trilinear out-of-bounds: +Z for uniform grid'
    Result = trilinear_real4(a_III, 1, 2, 1, 2, 0, 2, [1., 1., 2.4], &
         DoExtrapolate=.false.)
    GoodResult = 10000.0
    if(abs(Result - GoodResult) > 1.e-2) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing trilinear out-of-bounds: +Z for nonuniform grid'
    Result = trilinear_real4(a_III, 1, 2, 1, 2, 0, 2, [1., 1., 4.1], &
         x12_I, x12_I, x02_I, DoExtrapolate=.false.)
    GoodResult = 10000.0
    if(abs(Result - GoodResult) > 1.e-6) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing trilinear out-of-bounds: -Z for uniform grid'
    Result = trilinear_real4(a_III, 1, 2, 1, 2, 0, 2, [1., 1., -0.4], &
         DoExtrapolate=.false.)
    GoodResult = 1.0
    if(abs(Result - GoodResult) > 1.e-6) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing trilinear out-of-bounds: -Z for nonuniform grid'
    Result = trilinear_real4(a_III, 1, 2, 1, 2, 0, 2, [1., 1., 0.1], &
         x12_I, x12_I, x02_I, DoExtrapolate=.false.)
    GoodResult = 1.0
    if(abs(Result - GoodResult) > 1.e-2) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    ! Test extrapolation
    write(*,'(a)')'Testing bilinear extrapolation: +X uniform'
    Result = bilinear(a_II, 1, 2, 1, 3, [2.5,1.], DoExtrapolate=.true.)
    GoodResult = 29.5
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear extrapolation: +X nonuniform'
    Result = bilinear(a_II, 1, 2, 1, 3, [2.5,1.], x12_I, x13_I, &
         DoExtrapolate=.true.)
    GoodResult = 29.5
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear extrapolation: -X uniform'
    Result = bilinear(a_II, 1, 2, 1, 3, [.5,1.5], DoExtrapolate=.true.)
    GoodResult = -12.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing bilinear extrapolation: -X nonuniform'
    Result = bilinear(a_II, 1, 2, 1, 3, [.5,1.5], x12_I, x13_I, &
         DoExtrapolate=.true.)
    GoodResult = -12.0
    if(abs(Result - GoodResult) > 1.e-5) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing trilinear extrapolation: +Z uniform'
    Result = trilinear_real4(a_III, 1, 2, 1, 2, 0, 2, [1.3, 1.9, 2.60], &
         DoExtrapolate=.true.)
    GoodResult = 212958.38
    if(abs(Result - GoodResult) > 1.0) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing trilinear extrapolation: +Z nonuniform'
    Result = trilinear_real4(a_III, 1, 2, 1, 2, 0, 2, [1.3, 1.9, 5.2], &
         x12_I, x12_I, x02_I, DoExtrapolate=.true.)
    GoodResult = 212958.38
    if(abs(Result - GoodResult) > 1.0) &
         write(*,*) 'Test failed: Result=',Result,' differs from ',GoodResult

    write(*,'(a)')'Testing function bilinear_vector'
    Result_V = bilinear(a_VII, 2, 1, 2, 1, 3, [1.1, 2.2])
    GoodResult_V = [7.46, 74.6]
    if(any(abs(Result_V - GoodResult_V) > 1.e-5)) &
         write(*,*) 'Test failed: Result=',Result_V,&
         ' differs from ',GoodResult_V

    write(*,'(a)')'Testing function trilinear_vector'
    Result_V = trilinear(a_VIII, 2, 1, 2, 1, 2, 0, 2, [1.1, 1.2, 1.3])
    GoodResult_V = [ 11236.2, -112362.0 ]
    if(any(abs(Result_V - GoodResult_V) > 1.e-2)) write(*,*) &
         'Test failed: Result=', Result_V, ' differs from ', GoodResult_V

    write(*,'(a)')'Testing fit_parabola'
    x_I = [ 3.1, 4.0, 5.0 ]
    xMin = 4.2; yMin = 1.5
    y_I = 0.1*(x_I - xMin)**2 + yMin
    call fit_parabola(x_I, y_I, xExtremum, yExtremum, Weight2_I, Weight3_I)

    ! write(*,*)'x_I, xE=', x_I, xExtremum
    ! write(*,*)'y_I, yE=', y_I, yExtremum
    ! write(*,*)'Weight2_I=', Weight2_I
    ! write(*,*)'Weight3_I=', Weight3_I

    if(abs(xExtremum - xMin) > 1e-6) write(*,*) &
         'Test failed: xExtremum=', xExtremum, ' differs from ', xMin

    if(abs(yExtremum - yMin) > 1e-6) write(*,*) &
         'Test failed: yExtremum=', yExtremum, ' differs from ', yMin

    if(abs(sum(Weight2_I) - 1.0) > 1e-6) write(*,*) &
         'Test failed: sum of Weight2_I=', Weight2_I, ' is not 1'

    if(abs(sum(Weight2_I*x_I) - xMin) > 1e-6) write(*,*) &
         'Test failed: Weight2_I=', Weight2_I, ' sum(Weight2_I*x_I)=', &
         sum(Weight2_I*x_I), ' differs from ', xMin

    if(abs(sum(Weight3_I) - 1.0) > 1e-6) write(*,*) &
         'Test failed: sum of Weight3_I=', Weight3_I, ' is not 1'

    if(abs(sum(Weight3_I*x_I) - xMin) > 1e-6) write(*,*) &
         'Test failed: Weight3_I=', Weight3_I, ' sum(Weight3_I*x_I)=', &
         sum(Weight3_I*x_I), ' differs from ', xMin

    if(abs(sum(Weight3_I*y_I) - yMin) > 1e-6) write(*,*) &
         'Test failed: Weight3_I=', Weight3_I, ' sum(Weight3_I*y_I)=', &
         sum(Weight3_I*y_I), ' differs from ', yMin

  end subroutine test_interpolation
  !============================================================================
end module ModInterpolate
!==============================================================================
