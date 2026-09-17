!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModGrid

  ! Multi-line grid, D.Borovikov & I.Sokolov, Dec,17, 2017.
  ! Dec.23 2017: exclude fluxes from the state vector.
  ! Dec.25 2017: standard init and read_param
  ! Dec.25 2017: rename nVarRead=>nMhData, add NoShock_ param.

#ifdef OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use ModUtilities, ONLY: CON_stop
  use SP_ModSize, ONLY: nVertexMax, nP => nMomentum, &
       nMu => nPitchAngle, IsMuAvg => IsPitchAngleAverage
  use SP_ModProc, ONLY: nProc, iProc, iError

  implicit none

  SAVE

  PRIVATE ! except

  ! Public members:
  public:: read_param          ! read parameters related to grid
  public:: init                ! Initialize arrays on the grid
  public:: init_stand_alone    ! Initialize arrays on the grid
  public:: copy_old_state      ! save old arrays before getting new ones
  public:: get_other_state_var ! Auxiliary components of state vector
  public:: search_line         ! find particle index corresponding to radius
  public:: check_line_ishock   ! check if iShock exceeds iEnd of the field line
  public:: nP                  ! Total number of points in the momentum grid
  public:: nMu                 ! Number of points in the pitch angle (mu) grid
  public:: IsMuAvg             ! If .true., VDF is omnidirectional

  ! Coordinate system and geometry
  character(len=3), public :: TypeCoordSystem = 'HGR'

  ! Grid info
  ! Angular grid at origin surface
  integer, public :: nLat = 4  ! Number of lines along Lat grid
  integer, public :: nLon = 4  ! Number of lines along Lon grid
  integer, public:: iLatOffset = 0     ! Offset of line index along Lat grid
  integer, public:: iLonOffset = 0     ! Offset of line index along Lon grid
  ! Whether to read/write MH_data in appended format (set via #APPENDMHDATA)
  logical, public:: UseAppendedRead  = .false.
  logical, public:: UseAppendedWrite = .false.

  ! Total number of magnetic field lines on all PEs (a product of nLat*nLon)
  integer, public :: nLineAll = 16

  ! We do MPI on field lines:
  ! All nodes are enumerated. Last node number on the previous proc = iProc-1,
  ! equals (iProc*nLineAll)/nProc. Store this:
  integer, public :: iLineAll0
  ! The nodes on a given PE have node numbers ranging from iLineAll0 +1 to
  ! iNodeLast = ((iProc+1)*nLineAll)/nProc. The iLine index to enumerate
  ! lines on a given proc ranges from 1 to iNodeLast.
  ! nLine = nNodeLast - iLineAll0 is the number of lines (blocks) on this
  ! processor. For iLine = 1:nLine, iLineAll = iLineAll0+1:iNodeLast.
  integer, public :: nLine
  ! If there are extra nodes, we use the first nLineAll processors and keep
  ! others on the last line. Meanwhile, we keep sending the warning messages.

  ! Number of particles (vertexes, Lagrangian meshes) per line (line):
  integer, public, pointer :: nVertex_B(:)

  ! Function converting line number to lon-lat location of the line
  public :: iblock_to_lon_lat
  public :: ilineall_to_lon_lat

  ! Array for current and present location of shock wave
  integer, public, parameter:: nShockParam = 2,  &
       Shock_   = 1, & ! Current location of a shock wave
       ShockOld_= 2    ! Old location of a shock wave
  integer, public, allocatable:: iShock_IB(:,:)
  integer, public, parameter:: NoShock_ = 1

  ! Information about the magnetic field line foot point:
  ! the Lagrangian (0) and Cartesian (1:3) coordinates, and
  integer, public, parameter:: & ! init length of segment 1-2:
       Length_ = 4               ! control appending new particles
  real, public, pointer :: FootPoint_VB(:,:)

  ! Magnetic flux (absolute value) associated with lines
  real, public, allocatable:: MagneticFluxAbs_B(:)

  ! Logical to mark unusable lines
  logical, public, pointer :: Used_B(:)

  ! MHD state vector;
  ! 1st index - identification of variable (LagrID_:Wave2_)
  ! 2nd index - particle index along the field line
  ! 3rd index - local line number
  real, public, pointer :: MhData_VIB(:,:,:)

  ! Aux state vector;
  ! 1st index - identification of variable (D_:BOld_)
  ! 2nd index - particle index along the field line
  ! 3rd index - local line number
  real, public, pointer :: State_VIB(:,:,:)

  ! Number of variables in the state vector and the identifications
  integer, public, parameter :: nMhData = 13, nVar = 21, &
       LagrID_     = 0, & ! Lagrangian id           ^saved/   ^set to 0
       X_          = 1, & !                         |read in  |in copy_
       Y_          = 2, & ! Cartesian coordinates   |restart  |old_stat
       Z_          = 3, & !                         v/        |saved to
       Rho_        = 4, & ! Background plasma density         |mhd1
       T_          = 5, & ! Background temperature            |
       Ux_         = 6, & !                                   |may be
       Uy_         = 7, & ! Background plasma bulk velocity   |read from
       Uz_         = 8, & !                                   |mhd1
       Bx_         = 9, & !                                   |or
       By_         =10, & ! Background magnetic field         |received
       Bz_         =11, & !                                   |from
       Wave1_      =12, & !                                   |coupler
       Wave2_      =13, & !-Alfven wave turbulence            v
                                !-
       R_          =14, & ! Heliocentric distance          ^derived from
       D_          =15, & ! Distance to the next particle  |MHdata in
       S_          =16, & ! Distance from the foot point   |get_other_
       U_          =17, & ! Plasma speed along field line  |state_var
       B_          =18, & ! Magnitude of magnetic field    v
       RhoOld_     =19, & ! Background plasma density      ! copy_
       UOld_       =20, & ! Background plasma bulk speed   ! old_
       BOld_       =21    ! Magnitude of magnetic field    ! state

  ! variable names
  character(len=10), public, parameter:: NameVar_V(LagrID_:nVar) &
       = ['LagrID    ', &
       'X         ', &
       'Y         ', &
       'Z         ', &
       'Rho       ', &
       'T         ', &
       'Ux        ', &
       'Uy        ', &
       'Uz        ', &
       'Bx        ', &
       'By        ', &
       'Bz        ', &
       'Wave1     ', &
       'Wave2     ', &
       'R         ', &
       'D         ', &
       'S         ', &
       'U         ', &
       'B         ', &
       'RhoOld    ', &
       'UOld      ', &
       'BOld      ' ]

  ! Logical variable: whether this is the initial call
  logical:: DoInit = .true.

  ! whether to use smoothing of length along lines,
  ! e.g. when random walking lines are used
  ! logical:: DoSmooth = .false.
  ! size of groups used for smoothing
  ! integer:: nSmooth = -1

  ! Test position and momentum
  integer, public :: iPTest = 1, iParticleTest = 99, iNodeTest = 1
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in):: NameCommand      ! From PARAM.in
    integer :: nPCheck = nP, nMuCheck = nMu         ! Check nP and nMu
    integer :: nParticleCheck, nLonCheck, nLatCheck ! Check nLon/nLat/nParticle
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#MOMENTUMGRID')
       call read_var('nP', nPCheck)
       if(nP/=nPCheck) then
          if(iProc==0) write(*,'(a,i6,a,i6)') NameSub//' '//     &
               'Code is configured with nMomentum=', nP,         &
               ' while value read from PARAM.in is nP=', nPCheck
          call CON_stop('Modify PARAM.in or reconfigure SP/MFLAMPA')
       end if
    case('#PITCHANGLEGRID')
       call read_var('nMu', nMuCheck)
       if(nMu/=nMuCheck) then
          if(iProc==0) write(*,'(a,i6,a,i6)') NameSub//' '//     &
               'Code is configured with nMu=', nMu,              &
               ' while value read from PARAM.in is nMu=', nMuCheck
          call CON_stop('Modify PARAM.in or reconfigure SP/MFLAMPA')
       end if
    case('#CHECKGRIDSIZE')
       call read_var('nVertexMax', nParticleCheck)
       call read_var('nLat',       nLatCheck)
       call read_var('nLon',       nLonCheck)
       if(iProc==0 .and. any([nLon, nLat] /= [nLonCheck,nLatCheck])) &
            write(*,'(a,2I5)') 'nLon, nLat are reset to ', nLonCheck, nLatCheck
       nLat = nLatCheck
       nLon = nLonCheck
       nLineAll = nLon*nLat
       if(nParticleCheck > nVertexMax)then
          if(iProc==0) write(*,*) &
               'nVertexMax is too small, use ./Config.pl -g=', nParticleCheck
          call CON_stop('Code stopped')
       end if
    case('#COORDSYSTEM', '#COORDINATESYSTEM')
       call read_var('TypeCoordSystem', TypeCoordSystem, IsUpperCase=.true.)
       ! case('#DOSMOOTH')
       ! call read_var('DoSmooth', DoSmooth)
       ! if(DoSmooth)then
       ! call read_var('nSmooth', nSmooth)
       ! if(nSmooth < 1)&
       ! call CON_stop(NameSub//': Invalid setting for line smoothing')
       ! end if
    case('#GRIDNODE')
       call read_var('nLat', nLat)
       call read_var('nLon', nLon)
       nLineAll = nLat * nLon
    case('#GRIDNODEOFFSET')
       call read_var('iLatOffset', iLatOffset)
       call read_var('iLonOffset', iLonOffset)
    case('#TESTPOS')
       call read_var('iNodeTest',     iNodeTest)
       call read_var('iParticleTest', iParticleTest)
       call read_var('iPTest',        iPTest)
    case default
       call CON_stop(NameSub//' Unknown command '//NameCommand)
    end select

  end subroutine read_param
  !============================================================================
  subroutine init

    ! allocate the grid used in this model
    use ModUtilities, ONLY: check_allocate
    use SP_ModProc, ONLY: warn_more_proc
    integer :: iNodeLast                 ! last line on this node

    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    if(.not.DoInit) RETURN
    DoInit = .false.

    ! check consistency of nLat and nLon
    if(nLat <= 0 .or. nLon <= 0) &
         call CON_stop(NameSub//': Origin surface grid is invalid')
    ! check consistency of nP and nMu
    if(nP <= 0 .or. nMu <= 0) &
         call CON_stop(NameSub//': Momentum and/or Mu grids are invalid')

    ! distribute nodes between processors
    if(nLineAll >= nProc) then
       iLineAll0 = ( iProc   *nLineAll)/nProc
       iNodeLast = ((iProc+1)*nLineAll)/nProc
       nLine = iNodeLast-iLineAll0
    else
       ! there is one processor for each field line: we keep
       ! iProc = 0~nLineAll-1 working and others for the last line
       ! we also send the warning message for this over-request
       nLine = 1
       if(iProc < nLineAll) then
          iLineAll0 = iProc
       else
          ! Some work/trial has been done, but just partially. One can refer
          ! to the code version (915'th commit) on August 22, 2024.
          iLineAll0 = nLineAll-1
          call warn_more_proc
          write(*,*) "Here we keep iProc's >", nLineAll, 'on the last line.'
       end if
    end if

    ! allocate data and grid containers
    allocate(iShock_IB(nShockParam, nLine), stat=iError)
    call check_allocate(iError, NameSub//'iShock_IB')
    iShock_IB = NoShock_

    allocate(MagneticFluxAbs_B(nLine), stat=iError)
    call check_allocate(iError, NameSub//'MagneticFluxAbs_B')
    MagneticFluxAbs_B = -1

  end subroutine init
  !============================================================================
  subroutine init_stand_alone

    ! allocate the grid used in this model
    integer :: iVertex
    character(len=*), parameter:: NameSub = 'init_stand_alone'
    !--------------------------------------------------------------------------
    ! Allocate here if stand alone
    allocate(MhData_VIB(LagrID_:nMhData, 1:nVertexMax, nLine))

    ! initialize MhData
    MhData_VIB(1:nMhData, :, :) = 0.0

    ! reset lagrangian ids
    do iVertex = 1, nVertexMax
       MhData_VIB(LagrID_, iVertex, 1:nLine) = real(iVertex)
    end do

    ! Allocate auxiliary State vector
    allocate(State_VIB(nMhData+1:nVar, 1:nVertexMax, nLine))
    State_VIB = -1
    allocate(nVertex_B(nLine))
    nVertex_B = 0
    allocate(FootPoint_VB(LagrID_:Length_, nLine))
    FootPoint_VB = -1
    allocate(Used_B(nLine)); Used_B = .true.

  end subroutine init_stand_alone
  !============================================================================
  subroutine iblock_to_lon_lat(iBlockIn, iLonOut, iLatOut)

    ! return angular grid's indexes corresponding to this line
    ! local: input is the local number of line (block)
    integer, intent(in) :: iBlockIn
    integer, intent(out):: iLonOut
    integer, intent(out):: iLatOut

    integer :: iLineAll
    ! Get node number from line number
    !--------------------------------------------------------------------------
    call ilineall_to_lon_lat(&
         iLineAll = iBlockIn + iLineAll0, &
         iLonOut = iLonOut, iLatOut = iLatOut)
  end subroutine iblock_to_lon_lat
  !============================================================================
  subroutine ilineall_to_lon_lat(iLineAll, iLonOut, iLatOut)

    ! return angular grid's indexes corresponding to this line
    ! global: input is the global number of line
    integer, intent(in) :: iLineAll
    integer, intent(out):: iLonOut
    integer, intent(out):: iLatOut
    !--------------------------------------------------------------------------
    iLatOut = 1 + iLatOffset + (iLineAll - 1)/nLon
    iLonOut = iLineAll - nLon*(iLatOut - 1 - iLatOffset) + iLonOffset

  end subroutine ilineall_to_lon_lat
  !============================================================================
  subroutine copy_old_state

    ! copy current state to old state for all field lines
    integer:: iEnd, iLine
    !--------------------------------------------------------------------------
    do iLine = 1, nLine
       if(.not.Used_B(iLine))then
          MhData_VIB(1:nMhData, :, iLine) = 0.0
          ! nVertex_B(iLine) = 1
          CYCLE
       end if
       iEnd = nVertex_B(iLine)
       ! Only propagate Shock_ into ShockOld_ when Shock_ is a real shock
       ! position. If Shock_ is NoShock_ (shock currently lost from this
       ! line), preserve ShockOld_ as forward-only memory of the last known
       ! shock position; otherwise the next get_shock_location call would
       ! search from iShockMin = max(NoShock_, nShockWidth+1) = nShockWidth+1
       ! and could latch onto a spurious feature near the inner boundary,
       ! pumping the V1 injection BC.
       if(iShock_IB(Shock_, iLine) /= NoShock_) &
            iShock_IB(ShockOld_,iLine) = iShock_IB(Shock_, iLine)
       State_VIB(RhoOld_, 1:iEnd, iLine) = MhData_VIB(Rho_, 1:iEnd, iLine)
       State_VIB(UOld_, 1:iEnd-1, iLine) = State_VIB(U_, 1:iEnd-1, iLine)
       State_VIB(BOld_, 1:iEnd, iLine)   = State_VIB(B_, 1:iEnd, iLine)
       ! reset variables read from file or received via coupler
       MhData_VIB(1:nMhData, 1:iEnd, iLine) = 0.0
       ! Lagrangian coordinate is kept only
    end do

  end subroutine copy_old_state
  !============================================================================
  subroutine get_other_state_var

    integer:: iLine, iVertex, iEnd
    integer:: iAux1, iAux2
    real   :: XyzAux1_D(x_:z_), XyzAux2_D(x_:z_)
    character(len=*), parameter:: NameSub = 'get_other_state_var'
    !--------------------------------------------------------------------------
    LINE: do iLine = 1, nLine
       if(.not.Used_B(iLine)) CYCLE LINE
       iEnd = nVertex_B(iLine)
       do iVertex = 1, iEnd
          ! magnetic field
          State_VIB(B_,iVertex, iLine) = &
               norm2(MhData_VIB(Bx_:Bz_,iVertex,iLine))
          ! distances between particles
          ! NOTE: the iVertex'th value is the distance from
          ! i'th to (i+1)'th nodes
          ! if(.not.DoSmooth)then
          if(iVertex /= nVertex_B(iLine))then
             State_VIB(D_, iVertex, iLine) = norm2(&
                  MhData_VIB(X_:Z_, iVertex  , iLine) - &
                  MhData_VIB(X_:Z_, iVertex+1, iLine))
             ! plasma velocity projection onto the direction of
             ! vector from X_:Z_(iVertex) to X_:Z_(iVertex+1)
             ! which is the direction of the magnetic field at the face
             if(State_VIB(D_, iVertex, iLine) > 0.0)then
                State_VIB(U_, iVertex, iLine) = 0.50*&
                     sum( (MhData_VIB(Ux_:Uz_,iVertex,iLine) & ! 0.5(u_{i+1}&
                     + MhData_VIB(Ux_:Uz_,iVertex+1,iLine) )*& ! +u_i) \cdot
                     (MhData_VIB(X_:Z_, iVertex+1,iLine) &! (X_:Z_(iVertex+1)-&
                     - MhData_VIB(X_:Z_, iVertex, iLine)) ) & ! X_:Z_(iVertex))
                     /State_VIB(D_, iVertex, iLine)
             else
                write(*,*)'----- Diagnostic Information -----'
                write(*,*)'iEnd =', iEnd, 'iLine=', iLine
                write(*,*)'iLineAll=', iLineAll0+iLine, 'iVertex=', iVertex
                write(*,*)'Xyz (iVertex)=', MHData_VIB(X_:Z_,iVertex,iLine)
                write(*,*)'iVertex+1=',  iVertex+1
                write(*,*)'Xyz (iVertex+1)=', MHData_VIB(X_:Z_,iVertex+1,iLine)
                call CON_stop(NameSub//': zero size of mesh')
             end if
          end if
          ! else
          ! smoothing is done by groups:
          ! nSmooth particles are aggeregated into single effective one,
          ! find length increment between effective particles are used
          ! to find length increment between regular particles
          ! iAux1 = nSmooth * max(1, min(&
          !     iVertex/nSmooth,nVertex_B(iLine)/nSmooth-1))
          ! iAux2 = iAux1 + nSmooth
          ! XyzAux1_D = sum(MhData_VIB(&
          !     X_:Z_,iAux1-nSmooth+1:iAux1,iLine),DIM=2)/nSmooth
          ! XyzAux2_D = sum(MhData_VIB(&
          !     X_:Z_,iAux2-nSmooth+1:iAux2,iLine),DIM=2)/nSmooth
          ! State_VIB(D_, iVertex, iLine) = &
          !     sqrt(sum((XyzAux2_D - XyzAux1_D)**2)) / nSmooth
          ! end if
          ! distance from the beginning of the line
          if(iVertex == 1)then
             State_VIB(S_, iVertex, iLine) = 0.0
          else
             State_VIB(S_, iVertex, iLine) = &
                  State_VIB(S_, iVertex-1, iLine) + &
                  State_VIB(D_, iVertex-1, iLine)
          end if
          ! Heliocentric distance
          State_VIB(R_, iVertex, iLine) = &
               norm2(MhData_VIB(X_:Z_, iVertex, iLine))
       end do
    end do LINE

  end subroutine get_other_state_var
  !============================================================================
  subroutine search_line(iLine, Radius, iXOut, IsFound, Weight)

    ! performs search along given line
    ! for FIRST location ABOVE given heliocentric radius;
    ! if found, IsFound is set to .true. (.false. otherwise) and iXout is set
    ! to index where the particle's spatial coordinate is just above Radius
    integer,      intent(in) :: iLine   ! line/line index
    real,         intent(in) :: Radius  ! heliocentric distance to find
    integer,      intent(out):: iXout   ! result: index just above Radius
    logical,      intent(out):: IsFound ! result: whether search succeeds
    real,optional,intent(out):: Weight  ! interpolation weight at output index

    !--------------------------------------------------------------------------
    ! check whether line reaches the given radial distance
    if(State_VIB(R_, nVertex_B(iLine), iLine) < Radius) then
       ! mark failure to find location
       IsFound = .false.
       iXOut = -1
       RETURN
    end if

    ! line reaches the given radial distance
    IsFound = .true.
    ! find index of first particle above Radius
    iXOut = minloc(State_VIB(R_, 1:nVertex_B(iLine), iLine), &
         DIM=1, MASK=State_VIB(R_, 1:nVertex_B(iLine), iLine) > Radius)

    ! get interpolation weight, if necessary
    if(present(Weight)) then
       if(iXOut > 1) then
          Weight = (Radius - State_VIB(R_, iXOut-1, iLine)) / &
               (State_VIB(R_, iXOut,  iLine) - State_VIB(R_, iXOut-1, iLine))
       else
          ! iXOut == 1, right at the first point
          Weight = 1.0
       end if
    end if

  end subroutine search_line
  !============================================================================
  subroutine check_line_ishock(iLine)

    ! check if the shock front is beyond the last coordinate of this field line
    ! in case that we accidentally have fewer particles on this field line
    integer, intent(in) :: iLine            ! index of line
    !--------------------------------------------------------------------------
    if(iShock_IB(Shock_, iLine) > nVertex_B(iLine)) Used_B(iLine) = .false.

  end subroutine check_line_ishock
  !============================================================================
end module SP_ModGrid
!==============================================================================
