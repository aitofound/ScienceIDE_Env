!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModAngularSpread

  use ModNumConst, ONLY: cPi, cTwoPi, cTolerance, cSqrtTwo, cDegToRad
  use ModCoordTransform, ONLY: xyz_to_rlonlat
  use SP_ModGrid, ONLY: search_line, iblock_to_lon_lat, nLon, nLat, &
       nLine, MHData_VIB, MagneticFluxAbs_B, X_, Z_, Bx_, Bz_, Used_B
  use SP_ModSize, ONLY: nDim
  use SP_ModTIme, ONLY: iIter
  use ModUtilities, ONLY: CON_stop

  implicit none
  SAVE

  ! Public members
  public:: init
  public:: read_param
  public:: get_magnetic_flux
  public:: get_normalized_spread

  interface get_normalized_spread
     module procedure get_normalized_spread_grid
     module procedure get_normalized_spread_point
  end interface get_normalized_spread

  ! Module contains data and methods used primarily to create flux output
  ! on a sphere (see SP_ModPlot);
  ! ---------------------------------------------------------------------
  ! flux associated with each line is spread over a sphere as follows:
  !  Probability of particle to deviate into a solid angle \Omega is
  !  A * \int\limits_\Omega spread_shape(\Omega^\prime, Dir_D) d\Omega^\prime,
  !  where A is normalization constant, Dir_D is direction to line's footprint;
  ! ---------------------------------------------------------------------
  ! original implementation has
  !  spread_shape = \exp(-0.5 * (\psi/\sigma)^2),
  ! where \sigma is a free parameter,
  !       \psi is angle of arc corresponding to deviation into d\Omega^\prime
  !       from original direction
  ! ---------------------------------------------------------------------
  ! flux spread to some location (lon,lat)is proportional to total of particles
  ! traveling along the line:
  !  flux(lon,lat)*d\Omega = flux_{line}*\omega_{line}*A*spread_shape*d\Omega,
  ! where \omega_{line} is solid angle angle associated with a line
  ! (free parameter in original implementation)

  ! whether ready to use get_normalized_spread for individual locations/grid
  logical, public:: IsReadySpreadPoint= .false.
  logical, public:: IsReadySpreadGrid = .false.

  ! characteristic angular spreads
  real, allocatable:: Sigma_B(:)
  real:: SigmaIo_I(4)
  integer:: iSigmaMode = -1
  integer,parameter:: &
       SigmaConst_     = 0, &
       SigmaLinearLon_ = 1, &
       SigmaLinearLat_ = 2, &
       SigmaBiLinear_  = 3

  ! angular grid for flux spread on a sphere
  integer, public:: nSpreadLon
  integer, public:: nSpreadLat
  real, allocatable, public:: SpreadLon_I(:)
  real, allocatable, public:: SpreadLat_I(:)
  real, parameter:: LatMin = -cPi / 3.0

  ! Reference radius and value of solid angle associated with lines
  real:: RadiusRef = -1.0
  real:: SolidAngleRef = -1.0

  real, allocatable:: Norm_B(:)

contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=100):: StringAux
    integer:: nSigma=-1, iSigma
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#SPREADGRID')
       ! read size of angular grid
       call read_var('nSpreadLon', nSpreadLon)
       call read_var('nSpreadLat', nSpreadLat)
       if(nSpreadLon < 1 .or. nSpreadLat < 1)&
            call CON_stop(NameSub//': spread grid is invalid')
       IsReadySpreadGrid = .true.
    case('#SPREADSIGMA')
       call read_var('SigmaMode', StringAux)
       select case(StringAux)
       case('const')
          ! all line have the same characteristic spread (sigma)
          iSigmaMode = SigmaConst_
          nSigma = 1
       case('linear-lon')
          ! sigma changes linear by lines' longitude index
          iSigmaMode = SigmaLinearLon_
          nSigma = 2
       case('linear-lat')
          ! sigma changes linear by lines' latitude index
          iSigmaMode = SigmaLinearLat_
          nSigma = 2
       case('bilinear')
          ! sigma changes bilinear by lon and lat indexes
          iSigmaMode = SigmaLinearLon_
          nSigma = 4
       case default
          call CON_stop(NameSub//&
               ': unknown mode for setting characteristic angular spread')
       end select
       ! read appropriate number of input values for sigma
       do iSigma = 1, nSigma
          call read_var('Sigma [deg]', SigmaIo_I(iSigma))
          if(SigmaIo_I(iSigma) <= 0.0)&
               call CON_stop(NameSub//': invalid characteristic spread')
          SigmaIo_I(iSigma) = SigmaIo_I(iSigma) * cDegToRad
       end do
    case('#SPREADSOLIDANGLE')
       call read_var('RadiusRef [Rs]', RadiusRef)
       if(RadiusRef < 1.0)&
            call CON_stop(NameSub//' Invalid reference radius')
       call read_var('SolidAngleRef', SolidAngleRef)
       if(SolidAngleRef < 0.0 .or. SolidAngleRef > 4*cPi)&
            call CON_stop(NameSub//' Invalid reference solid angle')
       IsReadySpreadPoint = .true.
    case default
       call CON_stop(NameSub//' Unknown command '//NameCommand)
    end select
  end subroutine read_param
  !============================================================================
  subroutine init

    integer:: iLine, iLon, iLat
    real:: DLon, DLat, AuxLon, AuxLat
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    if(.not.IsReadySpreadPoint) RETURN

    ! compute characterisitc spreads for lines and corresponding normalizations
    allocate(Sigma_B(nLine))
    allocate(Norm_B(nLine))
    do iLine = 1, nLine
       call iblock_to_lon_lat(iLine, iLon, iLat)
       AuxLon = (iLon-1.0) / (nLon-1)
       AuxLat = (iLat-1.0) / (nLat-1)
       select case(iSigmaMode)
       case(SigmaConst_)
          Sigma_B(iLine) = SigmaIo_I(1)
       case(SigmaLinearLon_)
          Sigma_B(iLine) = SigmaIo_I(1) * (1-AuxLon) + SigmaIo_I(2) * AuxLon
       case(SigmaLinearLat_)
          Sigma_B(iLine) = SigmaIo_I(1) * (1-AuxLat) + SigmaIo_I(2) * AuxLat
       case(SigmaBiLinear_)
          Sigma_B(iLine) = &
               SigmaIo_I(1) * (1-AuxLon) * (1-AuxLat) + &
               SigmaIo_I(2) *    AuxLon  * (1-AuxLat) + &
               SigmaIo_I(3) * (1-AuxLon) *    AuxLat  + &
               SigmaIo_I(4) *    AuxLon  *    AuxLat
       case default
          call CON_stop(NameSub//&
               ': invalid mode for setting characteristic angular spread')
       end select
       call get_angular_spread_normalization(Sigma_B(iLine), Norm_B(iLine))
    end do

    ! initialize and fill angular grid
    if(.not.IsReadySpreadGrid) RETURN

    allocate(SpreadLon_I(nSpreadLon))
    allocate(SpreadLat_I(nSpreadLat))

    ! fill grid coords
    DLon = cTwoPi / nSpreadLon
    DLat = cTwoPi / nSpreadLat / 3.0
    do iLon = 1, nSpreadLon
       SpreadLon_I(iLon) = (iLon - 0.5) * DLon
    end do
    do iLat = 1, nSpreadLat
       SpreadLat_I(iLat) = LatMin + (iLat - 0.5) * DLat
    end do

  end subroutine init
  !============================================================================
  subroutine get_magnetic_flux
    ! fill MagneticFluxAbs_B with reference value and radius from PARAM.in

    integer:: iLine
    integer:: iRef
    logical:: IsFoundRef
    real:: Weight
    real:: Xyz_D(nDim)
    !--------------------------------------------------------------------------
    do iLine = 1, nLine
       if(.not.Used_B(iLine)) CYCLE
       call search_line(iLine, RadiusRef, iRef, IsFoundRef, Weight)
       if(IsFoundRef .and. iRef > 1)then
          Xyz_D = &
               MHData_VIB(X_:Z_,iRef-1,iLine) * (1-Weight) + &
               MHData_VIB(X_:Z_,iRef,  iLine) *    Weight
          MagneticFluxAbs_B(iLine) = SolidAngleRef * RadiusRef * abs(&
               sum(Xyz_D*(&
               MHData_VIB(Bx_:Bz_,iRef-1,iLine) * (1-Weight) + &
               MHData_VIB(Bx_:Bz_,iRef,  iLine) *    Weight )))
       else
          ! mark that failed to find magnetic flux with given reference radius
          MagneticFluxAbs_B(iLine) = -1.0
       end if
    end do
  end subroutine get_magnetic_flux
  !============================================================================
  subroutine get_angular_spread_normalization(Sigma, Norm)
    ! get value of integral:
    !   2\pi\int\limits_0^\pi \sin(t) \exp(-0.5*(t/Sigma)^2) dt
    ! CAUTION: function isnt designed to be called repeatedly during run,
    !          computations are time-expensive, originally called only at init

    real, intent(in) :: Sigma ! characteristic scale of spread
    real, intent(out):: Norm  ! result
    ! temporary values used to compute Norm
    real:: DNorm, Aux1, Aux2
    integer:: iTerm
    !--------------------------------------------------------------------------
    ! compute normalization factor for angular distribution
    ! using one of two approximations based on value of AngularSpread
    if(Sigma < cPi)then
       ! for smaller values use approximate formula for complex erf
       ! from Abramowitz and Stegun, 1972
       Aux1 = (cPi / Sigma)**2
       Aux2 = sqrt(cTwoPi) * Sigma * exp(-0.5 * Sigma**2)
       Norm = Aux2 * Sigma / cSqrtTwo
       DNorm = Norm
       iTerm = 0
       do while(DNorm/Norm > cTolerance)
          iTerm = iTerm + 1
          DNorm = 2.0 / iTerm * Aux2 * exp(-0.25*iTerm**2)  * &
               (1 + exp(-0.5*Aux1) / (1+2.0*Aux1/iTerm**2)) * &
               sinh(iTerm * Sigma / cSqrtTwo)
          Norm = Norm + DNorm
       end do
    else
       ! for larger values use Taylor expansion with respect to (1/Sigma)
       Aux1 = cTwoPi
       Aux2 = 4.0 * cPi
       Norm = Aux2
       DNorm = Norm
       iTerm = 0
       do while(abs(DNorm/Norm) > cTolerance)
          iTerm = iTerm + 1
          Aux1 =-Aux1 * 0.5*cPi*cPi / iTerm
          Aux2 = Aux2 * (2*iTerm-1) + Aux1
          DNorm = Aux2 / Sigma**(2*iTerm)
          Norm = Norm + DNorm
       end do
    end if
  end subroutine get_angular_spread_normalization
  !============================================================================
  function angle(Lon1, Lat1, Lon2, Lat2) result(AngleOut)
    ! angle of arc between 2 points given by their lon and lat
    real, intent(in):: Lon1, Lat1
    real, intent(in):: Lon2, Lat2
    real            :: AngleOut
    !--------------------------------------------------------------------------
    AngleOut = acos(min(1.0, max(-1.0, &
         cos(Lat1-Lat2) + cos(Lat1) * cos(Lat2) * (cos(Lon1-Lon2)-1.0) )))
  end function angle
  !============================================================================
  function spread_shape(Angle, Sigma) result(SpreadShape)
    ! value of non-normalized angular spread function;
    ! as in probability of deviation angle < T
    !   Prob = const \int\limits_0^T spread_shape(t,\sigma) \sin(t) dt

    real, intent(in):: Angle
    real, intent(in):: Sigma
    real            :: SpreadShape
    !--------------------------------------------------------------------------
    SpreadShape = exp(- 0.5 * (Angle/Sigma)**2)
  end function spread_shape
  !============================================================================
  subroutine get_normalized_spread_point(&
       iLine, Radius, LonPoint, LatPoint, Spread)
    ! value of normalized angular spread function:
    ! equal to probability of particle deviation to be within some solid angle
    ! centered around LonPoint, LatPoint

    integer, intent(in) :: iLine   ! line index on processor
    real,    intent(in) :: Radius  ! heliocentric radius
    real,    intent(in) :: LonPoint! longitude of location
    real,    intent(in) :: LatPoint! latitude of location
    real,    intent(out):: Spread  ! result

    ! whether to perfrom full computation or reuse result of previous call
    logical:: DoReset
    ! parameters of previous call
    integer, save::  iIterPrevCall = -1
    integer, save:: iBlockPrevCall = -1
    real,    save:: RadiusPrevCall = -1.0
    ! result of previous call
    logical, save:: IsFound
    integer, save:: iVertex
    real,    save:: SolidAngle
    real,    save:: LonLine, LatLine
    ! miscallaneous values
    real:: Aux
    ! interpolation weight between particles on line
    real:: Weight
    ! interpolated coords
    real:: Xyz_D(nDim)

    ! check whether value of magnetic flux has been found for this line
    !--------------------------------------------------------------------------
    if(MagneticFluxAbs_B(iLine) < 0)then
       Spread = 0
       RETURN
    end if
    !---------------------------------------------------------------
    ! to optimize function, sequential and similar calls reuse
    ! results of previous calls,
    ! e.g. multiple calls for same line at the same timestep
    !---------------------------------------------------------------
    ! determine whether to perform full computation
    DoReset = iLine/=iBlockPrevCall .or. Radius/=RadiusPrevCall .or.&
         iIter/= iIterPrevCall

    if(DoReset)then
       ! update parameters that determine similarity of consequitive calls
       iIterPrevCall  = iIter
       iBlockPrevCall = iLine
       RadiusPrevCall = Radius
       call search_line(iLine, Radius, iVertex, IsFound, Weight)
    end if

    if(.not.(IsFound.and.iVertex > 1))then
       ! if location not found, do not apply spread
       Spread = 0
       RETURN
    end if

    if(DoReset)then
       ! spread is computed based on interpolated coordinates and field
       Xyz_D = &
            MHData_VIB(X_:Z_,iVertex-1,iLine) * (1-Weight) + &
            MHData_VIB(X_:Z_,iVertex,  iLine) *    Weight
       call xyz_to_rlonlat(Xyz_D, Aux, LonLine, LatLine)
       ! Aux = B \cdot Xyz
       Aux = abs(sum(Xyz_D * (&
            MHData_VIB(Bx_:Bz_,iVertex-1,iLine) * (1-Weight) + &
            MHData_VIB(Bx_:Bz_,iVertex,  iLine) *    Weight)))
       ! solid angle of line's flux tube section on sphere
       SolidAngle = MagneticFluxAbs_B(iLine) / Aux / Radius
    end if
    ! Aux = angle of arc between line and locaiton of interest
    Aux = angle(LonLine, LatLine, LonPoint, LatPoint)

    Spread = spread_shape(Aux, Sigma_B(iLine))  * SolidAngle / Norm_B(iLine)
  end subroutine get_normalized_spread_point
  !============================================================================
  subroutine get_normalized_spread_grid(iLine, Radius, Spread_II)
    ! value of normalized angular spread on rectangular angular grid
    ! defined by SpreadLon_I and SpreadLat_I;

    integer, intent(in) :: iLine
    real,    intent(in) :: Radius
    real,    intent(out):: Spread_II(nSpreadLon, nSpreadLat)

    integer:: iLon, iLat
    !--------------------------------------------------------------------------
    ! compute spread over grid for current line
    do iLat = 1, nSpreadLat
       do iLon = 1, nSpreadLon
          call get_normalized_spread_point(iLine, Radius, &
               SpreadLon_I(iLon), SpreadLat_I(iLat), Spread_II(iLon, iLat))
       end do
    end do
  end subroutine get_normalized_spread_grid
  !============================================================================
end module SP_ModAngularSpread
!==============================================================================
