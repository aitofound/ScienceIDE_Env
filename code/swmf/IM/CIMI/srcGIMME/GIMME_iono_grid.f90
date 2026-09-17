module GIMME_iono_grid

  use ModKind, ONLY: Real8_

  implicit none
  private

  ! array to hold the solution
  real,  public, allocatable :: Potential_G(:,:)
  ! take from module when linking to share
  real, parameter :: cPi=3.14159, cTwoPi = 6.28319

  ! Define the maximum array size (not all theta array is always used)
  ! default is 2deg resultion
  integer, public :: MaxTheta = 90
  integer, public :: MaxPhi = 180

  ! define range of grid used
  integer, public :: nTheta = 45 ! default use only northern hemisphere
  ! integer, public :: nTheta = 80 ! default use only northern hemisphere
  integer, public :: nPhi   = 180

  ! Define if entire sphere is used or just half. When using full sphere we
  ! need define a point that will be ground (zero potential).
  ! When half sphere the entire equatorward edge is deemed to be ground.
  logical, public :: UseFullSphere=.false.
  real,    public :: iThetaGround = -2,iPhiGround = -2

  ! Range of theta and phi grids
  real, public :: ThetaMax = cPi
  real, public :: PhiMax   = cTwoPi

  ! iono radius
  real, public :: Riono=6475.0e3
  real, public :: rPlanet=6375.0e3
  ! grid spacing
  real, public :: dTheta, dPhi

  ! grid values
  real, public,allocatable :: Theta_G(:),Phi_G(:)

  ! TimeValues
  real, public :: TimeSimulation=0.0
  real(Real8_), public::  StartTime !,CurrentTime ! needed for SWMF stuff

  integer, public           ::  iStartTime_I(7) = [1976,3,28,0,0,0,0]
  integer, public           ::  iCurrentTime_I(7) = [1976,6,28,0,0,0,0]
  integer, public, parameter::  Year_=1, Month_=2, Day_=3, Hour_=4, &
       Minute_=5,Second_=6, mSecond_=7
  integer, public :: IYD = 76183, iDOY=183
  logical, public :: IsTimeAccurate

  ! MPI values
  integer, public :: iProc,nProc,iComm

  logical, public :: IsStandAlone=.true.

  public :: init_grid_iono
  public :: convert_lat_lon

contains
  !============================================================================
  subroutine init_grid_iono

    use ModTimeConvert, ONLY: time_int_to_real
    use CON_axes,       ONLY: init_axes
    integer :: iPhi, iTheta
    !--------------------------------------------------------------------------
    allocate (Theta_G(0:MaxTheta+1),Phi_G(0:MaxPhi+1))

    ! set dTheta and dPhi
    dTheta = ThetaMax / (MaxTheta+1)
    dPhi = PhiMax / MaxPhi

    ! fill the phi grid
    do iPhi = 1,MaxPhi
       Phi_G(iPhi)=(iPhi-1)*dPhi
    enddo
    ! set ghost cells
    ! Phi_G(0) = Phi_G(MaxPhi)
    ! Phi_G(MaxPhi+1) = Phi_G(1)

    Phi_G(0) = Phi_G(1)-dphi
    Phi_G(MaxPhi+1) = Phi_G(MaxPhi)+dPhi

    ! fill the theta grid
    do iTheta = 1,MaxTheta
       Theta_G(iTheta)=(iTheta)*dTheta
    enddo
    ! set ghost cells
    Theta_G(0) = dTheta

    if (UseFullSphere) then
       Theta_G(MaxTheta+1) = Theta_G(MaxTheta)
    else
       Theta_G(MaxTheta+1) = Theta_G(MaxTheta)+dTheta
    endif

    ! determine if we use the full sphere or only part of the sphere
    if (UseFullSphere) then
       nTheta=MaxTheta
       ! ground point is equator at midnight
       iThetaGround = nTheta/2
       iPhiGround   = nPhi/2
    endif

    ! Set the Time parameters

    ! set the time for coordinate transformation
    call time_int_to_real(iStartTime_I,StartTime)

    iDOY  = julianday(iStartTime_I(1),iStartTime_I(2),iStartTime_I(3))
    IYD=mod(iStartTime_I(Year_),100)*1000+iDOY

    ! Set axes for coord transform when in standalone mode
    if (IsStandAlone) call init_axes(StartTime)

    ! set the array to hold the potential solution
    if (.not.allocated(Potential_G))allocate(Potential_G(0:nTheta+1,0:nPhi+1))

  end subroutine init_grid_iono
  !============================================================================
  subroutine convert_lat_lon(SmTheta, SmPhi, GeoLat, &
       GeoLon, GmLat, GmLon)
    use CON_axes, ONLY: transform_matrix
    use ModNumConst, ONLY: cPi, cTwoPi, cDegToRad, cRadToDeg
    real, intent(in) :: SmTheta, SmPhi
    real, intent(out):: GeoLat, GeoLon, GmLat, GmLon
    real             :: SmGg_DD(3,3), SmGm_DD(3,3), theta, phi
    real             :: XyzSm_D(3), XyzGg_D(3), XyzGm_D(3)
    character(len=*),  parameter :: NameIeCoord='SMG'

    ! Get polar angle (theta) and azimuthal angle (phi)
    !--------------------------------------------------------------------------
    theta = SmTheta
    phi   = SmPhi

    ! Get xyzSm_D from SmTheta and SmPhi
    xyzSm_D(1) = sin(theta)*cos(phi)
    xyzSm_D(2) = sin(theta)*sin(phi)
    xyzSm_D(3) = cos(theta)

    ! get equivalent geographic coords Hemisphere 1

    ! Get transform matrix
    SmGg_DD = &
         transform_matrix(TimeSimulation, 'SMG', 'GEO')

    ! Transform xyzSm_D to XyzGg_DI
    XyzGg_D = matmul( SmGg_DD, XyzSm_D)

    ! Calculate GeoLat and GeoLon
    GeoLon = modulo(atan2(XyzGg_D(2), XyzGg_D(1)), cTwoPi) * cRadToDeg
    GeoLat = 90.0 - (acos(max(-1.0,min(1.0, XyzGg_D(3))))*cRadToDeg)

    ! get equivalent geomagnetic coords

    ! Get transform matrix
    SmGm_DD = &
         transform_matrix(TimeSimulation, 'SMG', 'MAG')

    ! Transform xyzSm_D to XyzGg_DI
    XyzGm_D = matmul( SmGm_DD, XyzSm_D)

    ! Calculate GeoLat and GeoLon
    GmLon = modulo(atan2(XyzGm_D(2), XyzGm_D(1)), cTwoPi) * cRadToDeg
    GmLat = 90.0 - (acos(max(-1.0,min(1.0, XyzGm_D(3))))*cRadToDeg)

  end subroutine convert_lat_lon
  !============================================================================
  integer function julianday(year, mon, day) result(Julian_Day)

    integer :: i
    integer, dimension(1:12) :: dayofmon
    integer :: year, mon, day

    !--------------------------------------------------------------------------
    dayofmon(1) = 31
    dayofmon(2) = 28
    dayofmon(3) = 31
    dayofmon(4) = 30
    dayofmon(5) = 31
    dayofmon(6) = 30
    dayofmon(7) = 31
    dayofmon(8) = 31
    dayofmon(9) = 30
    dayofmon(10) = 31
    dayofmon(11) = 30
    dayofmon(12) = 31

    if (mod(year,4) == 0) dayofmon(2) = dayofmon(1) + 1
    Julian_Day = 0
    do i = 1, mon-1
       Julian_Day = Julian_Day + dayofmon(i)
    enddo
    Julian_Day = Julian_Day + day

  end function julianday
  !============================================================================
end module GIMME_Iono_Grid
!==============================================================================
