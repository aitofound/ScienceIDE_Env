!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModShock

  ! This module contains subroutines for determining the shock location,
  ! and steepening the density and magnetic field strength at shock front.
  use SP_ModGrid, ONLY: nLine, nLineAll, Used_B, nVertex_B, &
       NameVar_V, LagrID_, X_, Z_, State_VIB, MhData_VIB, R_, &
       iShock_IB, NoShock_, Shock_, ShockOld_, check_line_ishock
  use SP_ModSize, ONLY: nVertexMax
  use ModUtilities, ONLY: CON_stop

  implicit none

  SAVE

  PRIVATE ! except

  ! Public members:
  public:: read_param           ! Read parameters
  public:: init                 ! Initialize arrays on the grid
  public:: get_divU             ! get divergence of velocity \vec{u}
  public:: get_shock_location   ! finds shock location on all lines
  public:: steepen_shock        ! steepen the density profile at the shock
  public:: get_shock_skeleton   ! shock surface skeleton for visualization

  ! If the shock wave is traced, the advance algorithms are modified
  logical, public :: DoTraceShock = .true.
  ! divergence of velocity \vec{U}: for determining the shock locations
  real, public, allocatable :: divU_II(:,:)

  ! Shock algorithm parameters:
  real,    public :: dLogRhoThreshold = 0.01 ! Empirical value
  integer, public :: nShockWidth = 10, nShockMargin = 50

  ! Parameters for the shock coordinates
  integer, public, parameter :: nShockVar = 18, &
       ShockID_    = 1, & ! Shock index
       XShock_     = 2, & ! Shock X coordinates
       YShock_     = 3, & ! Shock Y coordinates
       ZShock_     = 4, & ! Shock Z coordinates
       RShock_     = 5, & ! Shock radial distance
       RhoShock_   = 6, & ! Shock number density
       TShock_     = 7, & ! Shock temperature
       UxShock_    = 8, & ! Shock Ux
       UyShock_    = 9, & ! Shock Uy
       UzShock_    =10, & ! Shock Uz
       BxShock_    =11, & ! Shock Bx
       ByShock_    =12, & ! Shock By
       BzShock_    =13, & ! Shock Bz
       Wave1Shock_ =14, & ! Shock Wave1
       Wave2Shock_ =15, & ! Shock Wave2
       LonShock_   =16, & ! Shock longitude
       LatShock_   =17, & ! Shock latitude
       CompRatio_  =18    ! Compression ratio
  real, public, allocatable :: StateShock_VIB(:,:)
  logical, public :: DoSaveStateShock = .false.

  ! Shock variable names
  character(len=10), public, parameter:: NameVarShock_V(ShockID_:CompRatio_) &
       = ['ShockID   ', &
       'XShock    ', &
       'YShock    ', &
       'ZShock    ', &
       'RShock    ', &
       'nShock    ', &
       'TShock    ', &
       'UxShock   ', &
       'UyShock   ', &
       'UzShock   ', &
       'BxShock   ', &
       'ByShock   ', &
       'BzShock   ', &
       'Wave1Shock', &
       'Wave2Shock', &
       'LonShock  ', &
       'LatShock  ', &
       'CompRatio ']

  ! Unit for all the shock variables: Length is in the unit of Rsun
  character(len=6), public :: NameVarShockUnit_V(ShockID_:CompRatio_) = [&
       'none  ', &
       'RSun  ', &
       'RSun  ', &
       'RSun  ', &
       'RSun  ', &
       'amu/m3', &
       'kev   ', &
       'm/s   ', &
       'm/s   ', &
       'm/s   ', &
       'T     ', &
       'T     ', &
       'T     ', &
       'J/m3  ', &
       'J/m3  ', &
       'Deg   ', &
       'Deg   ', &
       'none  ']

  ! Shock skeleton for visualization
  real, public, allocatable :: XyzShockEffUnit_DG(:, :)
  ! Arrays to construct a triangular mesh on a sphere
  logical, public :: IsShockTriMade = .false.
  integer, public :: nShockTriMesh, lidShockTri, ridShockTri
  integer, public, allocatable :: &
       iListShock_I(:), iPointerShock_I(:), iEndShock_I(:)
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#TRACESHOCK')
       call read_var('DoTraceShock', DoTraceShock)
    case('#IDENTIFYSHOCK')
       call read_var('nShockWidth', nShockWidth)
       call read_var('nShockMargin', nShockMargin)
       call read_var('dLogRhoThreshold', dLogRhoThreshold)
    case default
       call CON_stop(NameSub//': Unknown command '//NameCommand)
    end select

  end subroutine read_param
  !============================================================================
  subroutine init

    ! initialize arrays related to the shock

    use ModUtilities, ONLY: check_allocate
    use SP_ModProc, ONLY: iError
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------

    if(allocated(divU_II)) deallocate(divU_II)
    allocate(divU_II(1:nVertexMax, 1:nLine)) ! divU
    call check_allocate(iError, 'divU_II')
    divU_II = 1.0

    ! allocate and assign values only when we save states for the shock
    if(DoSaveStateShock) then
       if(allocated(StateShock_VIB)) deallocate(StateShock_VIB)
       allocate(StateShock_VIB(XShock_:CompRatio_, 1:nLine)) ! States
       call check_allocate(iError, 'StateShock_VIB')
       StateShock_VIB = -1.0

       if(allocated(XyzShockEffUnit_DG)) deallocate(XyzShockEffUnit_DG)
       allocate(XyzShockEffUnit_DG(XShock_:LatShock_, 1:nLineAll+2)) ! Effective
       call check_allocate(iError, 'XyzShockEffUnit_DG')
       XyzShockEffUnit_DG = -1.0
    end if

  end subroutine init
  !============================================================================
  subroutine get_divU(TimeLimit)

    ! divU = dLogRho_I for time-accurate run
    ! divU = B*d(u/B)/ds in steady-state run
    use SP_ModGrid, ONLY: U_, B_, D_, Rho_, RhoOld_
    use SP_ModTime, ONLY: IsSteadyState, SPTime
    use SP_ModUnit, ONLY: Io2Si_V, UnitX_

    ! Maximum possible time
    real, intent(in):: TimeLimit
    ! Local VARs
    ! Inverse of the time step from State_VIB to MHData_VIB
    real :: InvDtFull
    ! Array of u=\vec{u}*\vec{B}/|B| and u/B at the face center
    real :: uSi_F(nVertexMax), uOverBSi_F(0:nVertexMax)
    ! Array of B and 1/B at the cell- and face-center
    real, dimension(nVertexMax) :: BSi_I, InvBSi_C, InvBSi_F
    ! Distance between adjacent meshes and faces
    real, dimension(nVertexMax) :: DsMeshSi_I, DsFaceSi_I
    ! d(u/B)/ds variable at the cell center
    real, dimension(nVertexMax) :: DuOverBDsSi_C
    ! Loop variables
    integer :: iLine, iEnd
    !--------------------------------------------------------------------------

    if(IsSteadyState) then
       do iLine = 1, nLine
          ! go line by line and get divU if active
          if(.not.Used_B(iLine)) then
             iShock_IB(Shock_,iLine) = NoShock_
             CYCLE
          end if
          ! Number of the active particles on the line
          iEnd = nVertex_B(iLine)

          ! Calculate u/B = \vec{u}*\vec{B} / |\vec{B}|**2 at cell face
          ! uSi_F with the index of "i" is the value at the face between
          ! the mesh "i" and "i+1"
          uSi_F(1:iEnd-1) = State_VIB(U_, 1:iEnd-1, iLine)
          ! Get B at cell center
          BSi_I(1:iEnd)   = State_VIB(B_, 1:iEnd, iLine)
          ! Calculate 1/B at cell- and face-center
          InvBSi_C(1:iEnd)   = 1.0/BSi_I(1:iEnd)
          InvBSi_F(1:iEnd-1) = 0.5*(InvBSi_C(1:iEnd-1) + InvBSi_C(2:iEnd))

          ! u/B with the index of "i" is the value at the face between
          ! the mesh "i" and "i+1"
          ! Average 1/B and multiply by uSi at face centers
          uOverBSi_F(1:iEnd-1) = uSi_F(1:iEnd-1)*InvBSi_F(1:iEnd-1)
          uOverBSi_F(0 )       = uSi_F(1)*InvBSi_C(1)
          uOverBSi_F(iEnd)     = uSi_F(iEnd-1)*InvBSi_C(iEnd)

          ! In M-FLAMPA DsMeshSi_I(i) is the distance between meshes i and i+1
          DsMeshSi_I(1:iEnd-1) = State_VIB(D_, 1:iEnd-1, iLine)*Io2Si_V(UnitX_)
          ! Within the framework of finite volume method, the cell volume
          ! is used, which is proportional to the distance between the faces
          ! bounding the volume with an index, i, which is half of sum of
          ! distance between meshes i-1 and i, i.e. DsMeshSi_I(i-1), and that
          ! between meshes i and i+1, which is DsMeshSi_I(i):
          DsFaceSi_I(2:iEnd-1)= 0.5*(DsMeshSi_I(1:iEnd-2)+DsMeshSi_I(2:iEnd-1))
          DsFaceSi_I(1)       = DsMeshSi_I(1)
          DsFaceSi_I(iEnd)    = DsMeshSi_I(iEnd-1)

          ! In each cell, d(u/B)/ds = [(u/B at the right face) - (u/B at the
          ! left face)]/(distance between the left and right faces)
          DuOverBDsSi_C(1:iEnd) = (uOverBSi_F(1:iEnd) - &
               uOverBSi_F(0:iEnd-1))/DsFaceSi_I(1:iEnd)
          ! divergence of plasma \vec{u} = B * d(u/B)/ds at cell center
          divU_II(1:iEnd, iLine) = BSi_I(1:iEnd)*DuOverBDsSi_C(1:iEnd)
       end do
    else
       InvDtFull = 1.0/(TimeLimit - SPTime)
       do iLine = 1, nLine
          ! go line by line and get divU if active
          if(.not.Used_B(iLine)) then
             iShock_IB(Shock_,iLine) = NoShock_
             CYCLE
          end if
          ! Number of the active particles on the line
          iEnd = nVertex_B(iLine)

          ! divergence of plasma \vec{u} = -d(ln(rho))/dt at cell center
          divU_II(1:iEnd, iLine) = -log(MhData_VIB(Rho_,1:iEnd,iLine)/ &
               State_VIB(RhoOld_,1:iEnd,iLine))
       end do
    end if

  end subroutine get_divU
  !============================================================================
  subroutine get_shock_location

    ! find location of a shock wave on a given line (line)
    ! shock front is assumed to be location of max log(Rho/RhoOld)
    use ModNumConst, ONLY: cRadToDeg
    use ModCoordTransform, ONLY: xyz_to_rlonlat
    use SP_ModGrid, ONLY: R_, Rho_, Wave2_, iLineAll0

    ! Do not search too close to the Sun
    real, parameter :: RShockMin = 1.20  ! *RSun
    integer         :: iShockMin
    ! Do not search too close to the heliosphere boundary
    integer :: iShockMax
    ! Misc
    integer :: iShockCandidate
    ! Loop variables
    integer :: iLine, iEnd, iShockForward
    ! Captures whether the previous call left Shock_ at NoShock_ for this
    ! line. Read BEFORE iShock_IB(Shock_) is overwritten in the branches
    ! below. Used to snap ShockOld_ forward on re-detection so the
    ! advection step does not retroactively march through vertices that
    ! were never shocked.
    logical :: WasLost

    character(len=*), parameter:: NameSub = 'get_shock_location'
    !--------------------------------------------------------------------------

    do iLine = 1, nLine
       ! go line by line and get the shock location if active
       if(.not.Used_B(iLine)) CYCLE
       ! Number of the active particles on the line
       iEnd = nVertex_B(iLine)

       ! Capture previous-call lost state for this line BEFORE overwriting
       ! iShock_IB(Shock_) below.
       WasLost = iShock_IB(Shock_, iLine) == NoShock_

       ! if it is time accurate:
       ! shock front is assumed to be location of max log(Rho/RhoOld);
       ! divergence of plasma velocity (negatively) propto dLogRho
       ! if it is steady state:
       ! shock front is assumed to be location of min B*d(u/B)/ds;
       ! divergence of plasma velocity (positively) propto dLogRho
       ! note: shock never moves back
       iShockMin = max(iShock_IB(ShockOld_, iLine), nShockWidth+1)
       iShockMax = iEnd - nShockMargin - 1
       ! get the forward grid index for iShockCandidate
       if (any(State_VIB(R_,iShockMin:iShockMax,iLine) > RShockMin .and. &
            divU_II(iShockMin:iShockMax, iLine) < -dLogRhoThreshold)) then
          ! found a candidate for the shock front
          iShockForward = minloc( &
               divU_II(iShockMin:iShockMax, iLine), DIM=1, MASK= &
               State_VIB(R_,iShockMin:iShockMax,iLine) > RShockMin .and. &
               divU_II(iShockMin:iShockMax, iLine) < -dLogRhoThreshold)
          iShockCandidate = iShockMin - 1 + iShockForward
          iShock_IB(Shock_, iLine) = iShockCandidate
          ! Snap ShockOld_ to (Shock_ - 1) when:
          !  (a) ShockOld_ == NoShock_  -- the very first detection ever on
          !      this line; identify the "old" location with the nearest
          !      lower vertex so advance() does not march from vertex 1
          !  (b) sensor jump -- new Shock_ is > 0.8 Rs ahead of ShockOld_
          !      (real shock at < 5,000 km/s passes < 600 Mm ~ 0.8 Rs per
          !      coupling step; bigger jump means the sensor latched onto a
          !      different feature)
          !  (c) WasLost -- previous step left Shock_ at NoShock_ and we are
          !      now re-detecting. Without this snap, ShockOld_ would still
          !      point at the pre-disappearance position and advance() would
          !      march nProgress = (Shock_ - ShockOld_) sub-steps,
          !      retroactively re-injecting at every vertex in between.
          if(iShock_IB(ShockOld_,iLine)==NoShock_ .or. WasLost .or.   &
               State_VIB(R_,iShock_IB(ShockOld_,iLine),iLine) + 0.8 < &
               State_VIB(R_,iShock_IB(Shock_,iLine),iLine)           )&
               iShock_IB(ShockOld_,iLine) = iShock_IB(Shock_,iLine) - 1
       else
          ! no candidate found -- shock is lost from this line for now.
          ! Set Shock_ to NoShock_ so set_momentum_bc does not inject at
          ! a stale shock location. Preserve ShockOld_ as forward-only
          ! memory of the last known shock position so the next call's
          ! iShockMin = max(ShockOld_, nShockWidth+1) keeps the search
          ! confined to the forward direction and cannot detect a spurious
          ! shock near the inner boundary.
          iShock_IB(Shock_, iLine) = NoShock_
       end if
       ! check_line_ishock: update Used_B(iLine)
       ! call check_line_ishock(iLine)
       ! if(.not.Used_B(iLine)) CYCLE

       ! calculate values only when we save states for the shock
       if(DoSaveStateShock) then
          ! get the coordinates
          StateShock_VIB(XShock_:ZShock_, iLine) = &
               MHData_VIB(X_:Z_, iShockCandidate, iLine)
          call xyz_to_rlonlat(StateShock_VIB(XShock_:ZShock_, iLine), &
               StateShock_VIB(RShock_, iLine), &
               StateShock_VIB(LonShock_, iLine), &
               StateShock_VIB(LatShock_, iLine))
          if(StateShock_VIB(RShock_, iLine) == 0.0) then
             write(*,*) "On the field line, iLineAll=", iLineAll0+iLine
             call CON_Stop(NameSub//": Error of the shock location (R=0.0).")
          end if

          ! convert units for angles
          StateShock_VIB([LonShock_,LatShock_], iLine) = &
               StateShock_VIB([LonShock_,LatShock_], iLine) * cRadToDeg
          ! get MHD VARs
          StateShock_VIB(RhoShock_:Wave2Shock_, iLine) = &
               MHData_VIB(Rho_:Wave2_, iShockCandidate, iLine)
          ! also get compression ratio at shock surface
          StateShock_VIB(CompRatio_, iLine) = &
               maxval(MHData_VIB(Rho_, &
               iShockCandidate-nShockWidth+1:iShockCandidate+1, iLine), &
               MASK=divU_II(iShockCandidate-nShockWidth+1: &
               iShockCandidate+1, iLine) < -dLogRhoThreshold)/ & ! post shock
               minval(MHData_VIB(Rho_, iShockCandidate+1: &
               iShockCandidate+nShockWidth, iLine), &
               MASK=divU_II(iShockCandidate+1:iShockCandidate+nShockWidth, &
               iLine) < -dLogRhoThreshold .and. &
               MHData_VIB(Rho_, iShockCandidate+1: &
               iShockCandidate+nShockWidth, iLine) > 0.0)        ! pre shock
       end if
    end do

  end subroutine get_shock_location
  !============================================================================
  subroutine steepen_shock(iLine, nX, iShock, BSi_I, dLogRhoIn_I)

    ! change the density profile near the shock front
    ! so it becomes steeper for the current line
    use ModConst, ONLY: cTiny
    use SP_ModGrid, ONLY: D_
    use SP_ModUnit, ONLY: Io2Si_V, UnitX_

    ! INPUTs
    integer, intent(in) :: iLine, iShock ! Indices of line and shock front
    integer, intent(in) :: nX            ! Number of meshes along s_L axis
    real, intent(inout) :: BSi_I(nX)     ! Magnetic field strength
    real, optional, intent(inout) :: dLogRhoIn_I(nX) ! for time-accurate run
    ! Local VARs
    real :: DsSi_I(1:nX-1), dLogRho_I(nX)
    real :: dLogRhoExcess_I(iShock-nShockWidth:iShock+nShockWidth-1)
    real :: dLogRhoExcessSum
    !--------------------------------------------------------------------------
    DsSi_I(1:nX-1) = State_VIB(D_, 1:nX-1, iLine)*Io2Si_V(UnitX_)
    ! get dLogRho_I if given; otherwise = -divU
    if(present(dLogRhoIn_I)) then
       dLogRho_I = dLogRhoIn_I
    else
       ! steady state: we do not have dlogrho/dt since dt=0 so we keep divU
       dLogRho_I = -divU_II(1:nX, iLine)
    end if

    ! find the excess of dLogRho within the shock compared
    ! to background averaged over length
    dLogRhoExcess_I = max(0.5*( &
         dLogRho_I(iShock-nShockWidth:iShock+nShockWidth-1) + &
         dLogRho_I(iShock-nShockWidth+1:iShock+nShockWidth)) - &
         dLogRhoThreshold, 0.0)
    ! a jump (dLogRhoExcess>0) in velocity accross the shock wave * \Delta t
    dLogRhoExcessSum = sum(dLogRhoExcess_I* &
         DsSi_I(iShock-nShockWidth:iShock+nShockWidth-1))

    ! check for zero excess
    if(abs(dLogRhoExcessSum) <= cTiny) RETURN
    ! nullify excess within the smoothed shock
    dLogRho_I(iShock-nShockWidth:iShock+nShockWidth) = min( &
         dLogRhoThreshold, dLogRho_I(iShock-nShockWidth:iShock+nShockWidth))
    ! ... and concentrate it at the shock front, applying the whole jump
    ! in the velocity at a single grid point
    dLogRho_I(iShock) = dLogRhoThreshold + &
         dLogRhoExcessSum/DsSi_I(iShock)
    ! also, sharpen the magnetic field magnitude
    ! post shock part
    BSi_I(iShock-nShockWidth+1:iShock+1) = &
         maxval(BSi_I(iShock+1-nShockWidth:iShock+1))
    ! pre shock part
    BSi_I(iShock+1:iShock+nShockWidth  ) = &
         minval(BSi_I(iShock+1:iShock+nShockWidth))

    ! update dLogRhoIn (if given) and divU
    if(present(dLogRhoIn_I)) then
       dLogRhoIn_I = dLogRho_I
       divU_II(1:nX, iLine) = -dLogRhoIn_I
    else
       ! steady state: we do not have dlogrho/dt since dt=0 so we keep dLogRho
       divU_II(1:nX, iLine) = -dLogRho_I
    end if

  end subroutine steepen_shock
  !============================================================================
  subroutine get_shock_skeleton

    ! get the shock wave front surface skeleton for visualization
    use ModCoordTransform, ONLY: xyz_to_rlonlat
    use ModMpi
    use ModTriangulateSpherical, ONLY: trmesh
    use SP_ModGrid, ONLY: iLineAll0
    use SP_ModProc, ONLY: iComm, nProc, iProc, iError
    use SP_ModTriangulate, ONLY: UsePoleTri
    use SP_ModUnit, ONLY: Io2Si_V, UnitX_

    ! Effective points for the shock surface
    integer :: nShockEff
    integer :: iShockEff_I(1:nLineAll)
    ! Spatial coordinates of all used field lines
    real, dimension(XShock_:ZShock_, 1:nLineAll) :: XyzShockUnit_DG
    ! Loop variables
    integer :: iLine, iLineAll, iShockEff

    character(len=*), parameter:: NameSub = 'get_shock_skeleton'
    !--------------------------------------------------------------------------
    ! If there are more than one processors working on the same field line,
    ! we only save the data for the first nLineAll processors.
    if(nProc > nLineAll .and. iProc >= nLineAll) RETURN

    ! Initialization
    XyzShockUnit_DG = 0.0
    XyzShockEffUnit_DG = 0.0; iShockEff_I = 0
    ! Find how many points are effective for the shock front
    nShockEff = count(iShock_IB(Shock_, 1:nLine)>1 .and. Used_B(1:nLine))

    ! Get the shock front location on all processors
    LINE:do iLine = 1, nLine
       if(.not.Used_B(iLine)) CYCLE LINE
       iLineAll = iLineAll0 + iLine

       ! Find how far shock has travelled on this line
       iShockEff_I(iLineAll) = iShock_IB(Shock_, iLine)
       ! Find the location of the shock front on this field line
       ! and project it to the unit sphere
       XyzShockUnit_DG(XShock_:ZShock_, iLineAll) = &
            StateShock_VIB(XShock_:ZShock_, iLine)/ &
            StateShock_VIB(RShock_, iLine)
    end do LINE
    ! Check correctness
    if(nShockEff /= count(iShockEff_I>1)) call CON_Stop(NameSub// &
         ': Incorrect effective nshock points on the processor.')

    ! Gather interpolated coordinates on the source processor
    if(nProc > 1) then
       if(iProc == 0) then
          call MPI_REDUCE(MPI_IN_PLACE, iShockEff_I, &
               nLineAll, MPI_INTEGER, MPI_SUM, 0, iComm, iError)
          call MPI_REDUCE(MPI_IN_PLACE, XyzShockUnit_DG, &
               3*nLineAll, MPI_REAL, MPI_SUM, 0, iComm, iError)
          call MPI_REDUCE(MPI_IN_PLACE, nShockEff, &
               1, MPI_INTEGER, MPI_SUM, 0, iComm, iError)
       else
          call MPI_REDUCE(iShockEff_I, iShockEff_I, &
               nLineAll, MPI_INTEGER, MPI_SUM, 0, iComm, iError)
          call MPI_REDUCE(XyzShockUnit_DG, XyzShockUnit_DG, &
               3*nLineAll, MPI_REAL, MPI_SUM, 0, iComm, iError)
          call MPI_REDUCE(nShockEff, nShockEff, &
               1, MPI_INTEGER, MPI_SUM, 0, iComm, iError)
       end if
    end if
    ! Check correctness
    if(nShockEff /= count(iShockEff_I>1)) call CON_Stop(NameSub// &
         ': Incorrect effective nshock points on the processor.')

    ! Send useful interpolated coordinates to the source processor
    if(iProc == 0) then
       if(nShockEff == nLineAll) then
          ! for all field lines, we get the effective shock surface
          XyzShockEffUnit_DG(XShock_:ZShock_, 2:nLineAll+1) = XyzShockUnit_DG
       else
          ! Otherwise, we will spend some time reorganizing the points
          iShockEff = 1
          do iLineAll = 1, nLineAll
             if(iShockEff_I(iLineAll) > 1) then
                iShockEff = iShockEff + 1
                XyzShockEffUnit_DG(XShock_:ZShock_, iShockEff) = &
                     XyzShockUnit_DG(XShock_:ZShock_, iLineAll)
             end if
          end do
       end if
    end if
    ! Broadcast the coordinates and flags to all processors
    call MPI_BCAST(XyzShockEffUnit_DG, 3*(nLineAll+2), &
         MPI_REAL, 0, iComm, iError)

    ! For poles
    if(UsePoleTri) then
       ! Add two grid nodes at the poles:
       lidShockTri = 1
       ridShockTri = nShockEff + 2
       XyzShockEffUnit_DG(XShock_:ZShock_, lidShockTri) = [0.0, 0.0, -1.0]
       XyzShockEffUnit_DG(XShock_:ZShock_, ridShockTri) = [0.0, 0.0, +1.0]
    else
       lidShockTri = 2
       ridShockTri = nShockEff + 1
    end if

    ! How many points get involved for mesh triangulation
    nShockTriMesh = ridShockTri - lidShockTri + 1
    if(nShockTriMesh >= 3) then
       ! Enough points for mesh triangulation

       ! Allocate and initialize arrays for triangulation and
       ! interpolation; if allocated, first deallocate them
       if(allocated(iListShock_I))    deallocate(iListShock_I)
       if(allocated(iPointerShock_I)) deallocate(iPointerShock_I)
       if(allocated(iEndShock_I))     deallocate(iEndShock_I)
       allocate(iListShock_I(6*(nShockTriMesh-2)), &
            iPointerShock_I(6*(nShockTriMesh-2)), &
            iEndShock_I(nShockTriMesh))
       iListShock_I = 0; iPointerShock_I = 0; iEndShock_I = 0

       ! Construct the Triangular mesh used for interpolation
       call trmesh(nShockTriMesh,                           &
            XyzShockEffUnit_DG(XShock_, lidShockTri:ridShockTri), &
            XyzShockEffUnit_DG(YShock_, lidShockTri:ridShockTri), &
            XyzShockEffUnit_DG(ZShock_, lidShockTri:ridShockTri), &
            iListShock_I, iPointerShock_I, iEndShock_I, iError)
       if(iError /= 0) call CON_Stop(NameSub//': Triangilation failed.')

       ! Then we claim that we made mesh triangulation
       IsShockTriMade = .true.
    else
       ! Not enough points for mesh triangulation
       IsShockTriMade = .false.
    end if

  end subroutine get_shock_skeleton
  !============================================================================
end module SP_ModShock
!==============================================================================
