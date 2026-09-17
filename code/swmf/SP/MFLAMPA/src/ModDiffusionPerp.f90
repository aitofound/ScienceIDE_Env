!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModPerpDiffusion

  ! Solve the perpendicular diffusion term in the Parker equation
  use ModMpi
  use ModNumConst, ONLY: cPi, cTwoPi, cTiny
  use ModUtilities, ONLY: CON_stop
  use SP_ModDistribution, ONLY: Distribution_CB, IsDistNeg, check_dist_neg
  use SP_ModGrid, ONLY: nP, nMu, nLine, nLineAll, State_VIB, R_, X_, Z_
  use SP_ModSize, ONLY: nVertexMax
  use SP_ModTime, ONLY: IsSteadyState
  use SP_ModProc, ONLY: nProc, iProc, iComm, iError
  use SP_ModUnit, ONLY: Io2Si_V, Si2Io_V, UnitX_
  use SP_ModTurbulence, ONLY: DPerp_CB

  implicit none

  PRIVATE ! Except

  SAVE

  ! Public members:
  public :: read_param, init, diffuseperp_distribution, &
       interp_source_intersect, interp_source_linenode
  ! Diffusion in space
  interface diffuseperp_distribution
     ! Global time step Dt
     module procedure diffuseperp_distribution_s
     ! DtLocal for (nP, nMu, nVertexMax, nLine)
     module procedure diffuseperp_distribution_c
  end interface diffuseperp_distribution

  ! Whether to include Perpendicular Diffusion
  logical, public :: UseDiffusionPerp = .false.
  character(len=15), public :: TypeDPerp = 'UseDParaRatio'
  real, public      :: DParaRatio = 0.065       ! Simple ratio of Dperp/Dpara
  logical, public :: DoTestDPerp = .false.

  ! Grid of the triangulated mesh for solving perp. term: nR * nTheta * nPhi
  integer, public :: nRPerp, nThetaPerp, nPhiPerp
  real            :: dThetaPerp, dPhiPerp       ! dTheta and dPhi
  real, allocatable:: dRPerpMesh_I(:), dRPerpFace_I(:) ! dR of cell center+face
  real            :: RMinPerp = 1.2, RMaxPerp = 240.0  ! RMin and RMax of Grid
  real            :: dLogRFacePerp = 0.0        ! Geometric Sequence for RMesh
  character(len=15):: ScaleRPerp                ! Scale (Linear/Log) along R_
  integer, allocatable :: iRPerpStart_I(:), iRPerpEnd_I(:)
  real, public, allocatable:: RPerp_C(:), ThetaPerp_C(:), PhiPerp_C(:), &
       RPerp_F(:), ThetaPerp_F(:), PhiPerp_F(:), XyzPerp_CB(:,:,:,:) ! SI units

  ! For the FVM solver on the uniform grid: face areas and cell volumes
  real, allocatable:: dVolumeR_I(:), dRPerpFace2_I(:)   ! radial direction
  real, allocatable:: ThetaPerpSin_C(:), dCosTheta_I(:) ! theta direction
  real, allocatable:: &
       AreaR_IIF(:,:,:), AreaTheta_IFI(:,:,:), AreaPhi_FII(:,:,:)    ! faces
  real, public, allocatable :: Volume_CB(:,:,:)                      ! volume
  real, public, allocatable :: Dt_CB(:,:,:,:)
contains
  !============================================================================
  subroutine read_param(NameCommand)
    use ModReadParam, ONLY: read_var
    character(len=*), intent(in) :: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#DIFFUSIONPERP')
       call read_var('UseDiffusionPerp', UseDiffusionPerp)
       ! Read in perpendicular diffusion information only when using it
       if(UseDiffusionPerp) then
          call read_var('TypeDPerp', TypeDPerp)
          select case(trim(TypeDPerp))
          case('UseDParaRatio')
             ! Simply assume DPerp/DPara = DParaRatio
             call read_var('DParaRatio', DParaRatio)
          case default
             call CON_stop(NameSub//": Unknown type of DPerp")
          end select

          ! Handle triangulated mesh in radial direction
          call read_var('nRPerp', nRPerp)
          call read_var('RMinPerp', RMinPerp)
          call read_var('RMaxPerp', RMaxPerp)
          if(nRPerp<=1 .or. RMinPerp<=0.0 .or. RMaxPerp<RMinPerp) RETURN
          if(nRPerp<nProc) then
             write(*,*) "nRPerp = ", nRPerp, "< nProc = ", nProc, &
                  "now set nRPerp = nProc = ", nProc
          end if

          ! From RMinPerp to RMaxPerp: Distribution of multiple spheres
          call read_var('ScaleRPerp', ScaleRPerp)

          ! Handle lon-lat grid in the triangulated mesh
          call read_var('nThetaPerp', nThetaPerp)
          call read_var('nPhiPerp', nPhiPerp)
          if(nThetaPerp<=0 .or. nPhiPerp<=0) RETURN
          ! Theta: nThetaPerp, dThetaPerp and ThetaPerp_C
          if(mod(nThetaPerp, 2) > 0) then
             write(*,*) "nThetaPerp reads", nThetaPerp, "and becomes", &
                  nThetaPerp+1, "to avoid singularity when theta=0"
             nThetaPerp = nThetaPerp + 1
          end if

          ! Print out test information
          call read_var('DoTestDPerp', DoTestDPerp)
       end if
    case default
       call CON_stop('SP:'//NameSub//': unknown command '//NameCommand)
    end select
  end subroutine read_param
  !============================================================================
  subroutine init
    ! setup the uniform spheres and time stepping in perpendicular diffusion

    integer :: iProcChunk, iRPerpRemainder, iPE
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    ! setup the mesh arrays for triangulation used in perpendicular diffusion
    ! Grid
    if(allocated(RPerp_C)) &
         deallocate(RPerp_C, RPerp_F, &
         dRPerpMesh_I, dRPerpFace_I, XyzPerp_CB)
    allocate(RPerp_C(nRPerp), RPerp_F(nRPerp+1), &
         dRPerpMesh_I(nRPerp-1), dRPerpFace_I(nRPerp), &
         XyzPerp_CB(X_:Z_, nPhiPerp, nThetaPerp, nRPerp))
    XyzPerp_CB = 0.0

    ! Faces and volume used in the FVM solver
    if(allocated(Volume_CB)) &
         deallocate(dVolumeR_I, dRPerpFace2_I, &
         ThetaPerpSin_C, dCosTheta_I, &
         AreaR_IIF, AreaTheta_IFI, AreaPhi_FII, Volume_CB)
    allocate(dVolumeR_I(nRPerp), dRPerpFace2_I(nRPerp), &
         ThetaPerpSin_C(nThetaPerp), dCosTheta_I(nThetaPerp), &
         AreaR_IIF(nPhiPerp, nThetaPerp, nRPerp+1), &
         AreaTheta_IFI(nPhiPerp, nThetaPerp+1, nRPerp), &
         AreaPhi_FII(nPhiPerp+1, nThetaPerp, nRPerp), &
         Volume_CB(nPhiPerp, nThetaPerp, nRPerp))

    ! DPerp coefficient
    if(allocated(DPerp_CB)) deallocate(DPerp_CB)
    allocate(DPerp_CB(nP, nMu, nVertexMax, nLine), stat=iError)
    DPerp_CB = 0.0

    ! Determine chunk size, start and end indices for each processor
    if(allocated(iRPerpStart_I)) deallocate(iRPerpStart_I)
    if(allocated(iRPerpEnd_I)) deallocate(iRPerpEnd_I)
    allocate(iRPerpStart_I(0:nProc-1), iRPerpEnd_I(0:nProc-1))
    iProcChunk = nRPerp/nProc             ! Base chunk size
    iRPerpRemainder = mod(nRPerp, nProc)  ! Leftover elements
    ! Loop over processors to compute start and end indices
    do iPE = 0, nProc-1
       if(iPE < iRPerpRemainder) then
          iRPerpStart_I(iPE) = iPE*(iProcChunk+1) + 1
          iRPerpEnd_I(iPE)   = iRPerpStart_I(iPE) + iProcChunk
       else
          iRPerpStart_I(iPE) = iPE*iProcChunk + iRPerpRemainder+1
          iRPerpEnd_I(iPE)   = iRPerpStart_I(iPE) + iProcChunk-1
       end if
    end do

    ! Initialize the uniform grid
    call init_uniform_grid
    ! Precompute the face area and volume of the uniform grid
    call calc_uniform_grid_area_volume

    ! Allocate Dt_CB
    if(IsSteadyState) allocate(Dt_CB(nP, nMu, nVertexMax, nLine), stat=iError)

  contains
    !==========================================================================
    subroutine init_uniform_grid
      ! Initialize the uniform grid

      use ModCoordTransform, ONLY: sph_to_xyz
      integer :: iRPerp, iThetaPerp, iPhiPerp ! loop variables
      !------------------------------------------------------------------------
      ! R: radial direction
      select case(trim(ScaleRPerp))
      case("Linear", "linear")
         ! Linear (i.e., Uniform) Grid
         dRPerpMesh_I = (RMaxPerp - RMinPerp)/real(nRPerp) ! Same=Const.
         dRPerpFace_I = dRPerpMesh_I(1) ! Same=Const.
         do iRPerp = 1, nRPerp
            RPerp_F(iRPerp) = RMinPerp + (real(iRPerp)-1.0)*dRPerpMesh_I(1)
         end do
         RPerp_F(nRPerp+1) = RMaxPerp
         RPerp_C = 0.5*(RPerp_F(1:nRPerp) + RPerp_F(2:nRPerp+1))
      case("Exp", "exp", "Exponential", "exponential")
         ! Exponential Grid
         dLogRFacePerp = log(RMaxPerp/RMinPerp)/real(nRPerp)
         do iRPerp = 1, nRPerp+1
            RPerp_F(iRPerp) = RMinPerp * exp((real(iRPerp)-1.0)*dLogRFacePerp)
         end do
         RPerp_C = 0.5*(RPerp_F(1:nRPerp) + RPerp_F(2:nRPerp+1))
         dRPerpFace_I = RPerp_F(2:nRPerp+1) - RPerp_F(1:nRPerp)
         dRPerpMesh_I = RPerp_C(2:nRPerp) - RPerp_C(1:nRPerp-1)
      case("HalfExp", "halfexp", "HalfLinear", "halflinear")
         ! Half Exponential + Half Linear Grid -- From Experience
         dLogRFacePerp = log(RMaxPerp/RMinPerp)/real(nRPerp)
         dRPerpMesh_I = (RMaxPerp - RMinPerp)/real(nRPerp) ! Same=Const.
         do iRPerp = 1, nRPerp+1
            RPerp_F(iRPerp) = &
                 0.5*RMinPerp * exp((real(iRPerp)-1.0)*dLogRFacePerp) + &
                 0.5*(RMinPerp + (real(iRPerp)-1.0)*dRPerpMesh_I(1))
         end do
         RPerp_C = 0.5*(RPerp_F(1:nRPerp) + RPerp_F(2:nRPerp+1))
         dRPerpFace_I = RPerp_F(2:nRPerp+1) - RPerp_F(1:nRPerp)
         dRPerpMesh_I = RPerp_C(2:nRPerp) - RPerp_C(1:nRPerp-1)
      case default
         call CON_stop(NameSub// &
              'Error in ScaleRPerp for Perpendicular Diffusion.')
      end select
      dRPerpMesh_I = dRPerpMesh_I*Io2Si_V(UnitX_)
      dRPerpFace_I = dRPerpFace_I*Io2Si_V(UnitX_)
      RPerp_C = RPerp_C*Io2Si_V(UnitX_)
      RPerp_F = RPerp_F*Io2Si_V(UnitX_)

      ! Theta: zenith/latitudinal direction
      dThetaPerp = cPi/real(nThetaPerp)
      if(allocated(ThetaPerp_C)) deallocate(ThetaPerp_C, ThetaPerp_F)
      allocate(ThetaPerp_C(nThetaPerp), ThetaPerp_F(nThetaPerp+1))
      do iThetaPerp = 1, nThetaPerp
         ThetaPerp_C(iThetaPerp) = (real(iThetaPerp)-0.5)*dThetaPerp
      end do
      ThetaPerp_F(2:nThetaPerp) = 0.5*(ThetaPerp_C(1:nThetaPerp-1) + &
           ThetaPerp_C(2:nThetaPerp))
      ThetaPerp_F(1) = ThetaPerp_C(1) - 0.5*dThetaPerp
      ThetaPerp_F(nThetaPerp+1) = ThetaPerp_C(nThetaPerp) + 0.5*dThetaPerp

      ! Phi: azimuthal/longitudinal direction
      dPhiPerp = cTwoPi/real(nPhiPerp)
      if(allocated(PhiPerp_C)) deallocate(PhiPerp_C, PhiPerp_F)
      allocate(PhiPerp_C(nPhiPerp), PhiPerp_F(nPhiPerp+1))
      do iPhiPerp = 1, nPhiPerp
         PhiPerp_C(iPhiPerp) = (real(iPhiPerp)-0.5)*dPhiPerp
      end do
      PhiPerp_F(2:nPhiPerp) = 0.5*(PhiPerp_C(1:nPhiPerp-1) + &
           PhiPerp_C(2:nPhiPerp))
      PhiPerp_F(1) = PhiPerp_C(1) - 0.5*dPhiPerp
      PhiPerp_F(nPhiPerp+1) = PhiPerp_C(nPhiPerp) + 0.5*dPhiPerp

      ! (R, Theta, Phi) => (X, Y, Z) + Parallelization
      do iRPerp = iRPerpStart_I(iProc), iRPerpEnd_I(iProc)
         do iThetaPerp = 1, nThetaPerp
            do iPhiPerp = 1, nPhiPerp
               call sph_to_xyz(RPerp_C(iRPerp), &
                    ThetaPerp_C(iThetaPerp), PhiPerp_C(iPhiPerp), &
                    XyzPerp_CB(:, iPhiPerp, iThetaPerp, iRPerp))
            end do
         end do
      end do
      if(nProc > 1) call MPI_ALLREDUCE(MPI_IN_PLACE, XyzPerp_CB, &
           3*nPhiPerp*nThetaPerp*nRPerp, MPI_REAL, MPI_SUM, iComm, iError)

    end subroutine init_uniform_grid
    !==========================================================================
    subroutine calc_uniform_grid_area_volume
      ! Pre-calculate the face areas and cell volumes

      integer :: i, j
      !------------------------------------------------------------------------
      ! Grids
      ! Radial: RPerp_C, RPerp_F, dRPerpMesh_I, dRPerpFace_I, dRPerpFace2_I
      dRPerpFace2_I = 0.5*(RPerp_F(2:nRPerp+1)**2 - RPerp_F(1:nRPerp)**2)
      ! Theta: ThetaPerp_C, ThetaPerp_F, dThetaPerp
      ThetaPerpSin_C = sin(ThetaPerp_C) ! needed also in FVM internal fluxes
      ! Phi: PhiPerp_C, PhiPerp_F, dPhiPerp -- Ready

      ! Precompute: Face area and cell volume
      dVolumeR_I = (RPerp_F(2:nRPerp+1)**3 - RPerp_F(1:nRPerp)**3)/3.0
      dCosTheta_I = cos(ThetaPerp_F(1:nThetaPerp)) - &
           cos(ThetaPerp_F(2:nThetaPerp+1))
      do i = 1, nRPerp+1
         do j = 1, nThetaPerp
            AreaR_IIF(:,j,i) = RPerp_F(i)**2*dCosTheta_I(j)*dPhiPerp
         end do
      end do
      do i = 1, nRPerp
         do j = 1, nThetaPerp
            Volume_CB(:,j,i) = dVolumeR_I(i)*dCosTheta_I(j)*dPhiPerp
            AreaTheta_IFI(:,j,i) = dRPerpFace2_I(i)* &
                 sin(ThetaPerp_F(j))*dPhiPerp
         end do
         AreaTheta_IFI(:,nThetaPerp+1,i) = dRPerpFace2_I(i)* &
              sin(ThetaPerp_F(nThetaPerp+1))*dPhiPerp
         AreaPhi_FII(:,:,i) = dRPerpFace2_I(i)*dThetaPerp
      end do

    end subroutine calc_uniform_grid_area_volume
    !==========================================================================
  end subroutine init
  !============================================================================
  subroutine diffuseperp_distribution_s(IsFirstCall, dtIn)
    logical, intent(in) :: IsFirstCall
    real, intent(in)    :: dtIn
    real :: dtFake_C(nP, nMu, nVertexMax, nLine)
    !--------------------------------------------------------------------------
    dtFake_C = dtIn
    call diffuseperp_distribution_c(IsFirstCall, dtFake_C)
  end subroutine diffuseperp_distribution_s
  !============================================================================
  subroutine diffuseperp_distribution_c(IsFirstCall, dtIn_CB)
    ! cross-field (perpendicular) diffusion related steps and manipulations

    use SP_ModTriangulate, ONLY: reset_intersect_surf, intersect_surf, &
         build_trmesh, interpolate_trmesh, nTriMesh_I

    ! inputs
    logical, intent(in) :: IsFirstCall
    real, intent(in)    :: dtIn_CB(nP, nMu, nVertexMax, nLine)
    ! loop variables
    integer :: iRPerp, iThetaPerp, iPhiPerp, iPE
    ! VDF and the source term (5D)
    !--------------------------------------------------------------------------
    real, dimension(0:nP+1, nMu, nPhiPerp, nThetaPerp, nRPerp)&
         :: DistrPerp_5D, source_5D
    ! DPerp coefficients (5D)
    real :: DPerp_5D(nP, nMu, nPhiPerp, nThetaPerp, nRPerp)
    ! source term (along lines)
    real :: source_IV(nP, nMu, nLineAll, nRPerp)
    character(len=*), parameter:: NameSub = 'diffuseperp_distribution_c'
    !--------------------------------------------------------------------------
    if(IsFirstCall) then
       ! In the 1st call of DPerp: we set up the skeleton, coefficient, and VDF
       DistrPerp_5D = 0.0; DPerp_5D = 0.0

       ! Step 1: field lines => intersection points on multiple uniform layers
       ! here, iProc is for field lines, not for sub-slices/layers.
       ! In fact, this step of getting the intersection points is needed ONLY
       ! the 1st time, since MHData would be changed every coupling interval.
       call reset_intersect_surf(nRPerp)
       do iRPerp = 1, nRPerp
          call intersect_surf(RPerp_C(iRPerp)*Si2Io_V(UnitX_), &
               0, iRPerp, IsIncludeDPerpIn=.true.)
       end do
       if(nProc > 1) then
          do iPE = 0, nProc-1
             call MPI_BCAST(nTriMesh_I, nRPerp, &
                  MPI_INTEGER, iPE, iComm, iError)
          end do
       end if

       if(iProc == 0) then
          ! Step 2: Get DPerp at the cell center (uniform grid; ONLY 1st time)
          ! Now, iProc is for sub-slices/layers in the `DistrPerp_5D` array
          do iRPerp = 1, nRPerp
             ! intersection points => construct the triangulation skeleton
             call build_trmesh(iRPerp)
             do iThetaPerp = 1, nThetaPerp
                do iPhiPerp = 1, nPhiPerp
                   ! Get DPerp coefficient at the cell center (uniform grid)
                   call interpolate_trmesh(XyzPerp_CB(:,             &
                        iPhiPerp,iThetaPerp,iRPerp)*Si2Io_V(UnitX_), &
                        iRIn = iRPerp, DPerp_II =                    &
                        DPerp_5D(:,:,iPhiPerp,iThetaPerp,iRPerp))
                end do
             end do
          end do
       end if
       ! Broadcast DPerp coefficient to all processes
       if(nProc > 1) then
          do iPE = 0, nProc-1
             call MPI_BCAST(DPerp_5D, nPhiPerp*nThetaPerp*nRPerp* &
                  nP*nMu, MPI_REAL, iPE, iComm, iError)
          end do
       end if
       ! Check the error message from after interpolate_trmesh
       if(iError /= 0) then
          write(*,*) 'iProc = ', iProc, NameSub//&
               ': interpolate_trmesh for DPerp_5D failed, DPerp stopped'
          RETURN
       end if

       ! Output test information
       if(DoTestDPerp) write(*,*) NameSub//'Finish FirstCall: '// &
            'Triangulation skeleton and DPerp Coef in the uniform grid'
    end if

    ! Step 3: Triangulation skeleton => Interpolate the VDF
    if(iProc == 0) then
       do iRPerp = 1, nRPerp
          call build_trmesh(iRPerp)
          do iThetaPerp = 1, nThetaPerp
             do iPhiPerp = 1, nPhiPerp
                ! Get VDF at the cell center in the uniform grid
                call interpolate_trmesh(XyzPerp_CB(:,             &
                     iPhiPerp,iThetaPerp,iRPerp)*Si2Io_V(UnitX_), &
                     iRIn = iRPerp, DistrInterp_II =              &
                     DistrPerp_5D(:,:,iPhiPerp,iThetaPerp,iRPerp))
             end do
          end do
       end do
    end if
    ! Broadcast Dperp coefficient to all processes
    if(nProc > 1) then
       do iPE = 0, nProc-1
          call MPI_BCAST(DistrPerp_5D, nPhiPerp*nThetaPerp*nRPerp* &
               (nP+2)*nMu, MPI_REAL, iPE, iComm, iError)
       end do
    end if

    ! Step 4: Solve the DPerp equation in multiple uniform layers by FVM
    source_5D = 0.0
    call solver_fvm_diffuse3d(DPerp_5D,                 &
         iRPerpStart_I(iProc), iRPerpEnd_I(iProc),      &
         IsSteadyState, dtIn_CB(1,1,1,1), DistrPerp_5D, &
         source_5D(:,:,:,:,iRPerpStart_I(iProc):iRPerpEnd_I(iProc)))
    ! Broadcast source_5D (uniform grid) to all processes
    if(nProc > 1) then
       do iPE = 0, nProc-1
          call MPI_BCAST(source_5D(:,:,:,:,             &
               iRPerpStart_I(iPE):iRPerpEnd_I(iPE)),    &
               (iRPerpEnd_I(iPE)-iRPerpStart_I(iPE)+1)* &
               (nP+2)*nMu*nPhiPerp*nThetaPerp, MPI_REAL, iPE, iComm, iError)
       end do
    end if
    ! Check the error message from after interpolate_trmesh
    if(iError /= 0) then
       write(*,*) 'iProc = ', iProc, NameSub//&
            ': interpolate_trmesh for source_5D failed, DPerp stopped'
       RETURN
    end if

    ! Step 5: Interpolate source back to the nodes along field lines
    ! uniform grid => intersection points
    call interp_source_intersect(source_5D, source_IV)
    ! intersection points => field line nodes
    call interp_source_linenode(source_IV, dtIn_CB)

  end subroutine diffuseperp_distribution_c
  !============================================================================
  subroutine solver_fvm_diffuse3d(DPerp_5D, iRStart, iREnd, &
       IsSteadyStateIn, dtIn, DistrPerp_5D, source_5D)
    ! FVM solver for pure 3d diffusion equation: df/dt = div (DPerp grad_f)

    integer, intent(in) :: iRStart, iREnd
    logical, intent(in) :: IsSteadyStateIn
    real,    intent(in) :: dtIn
    real,    intent(in) :: DPerp_5D(1:nP,nMu,nPhiPerp,nThetaPerp,nRPerp)
    real, intent(inout) :: DistrPerp_5D(0:nP+1, nMu, &
         nPhiPerp, nThetaPerp, iRStart:iREnd)
    real, intent(out):: source_5D(0:nP+1,nMu,nPhiPerp,nThetaPerp,iRStart:iREnd)

    ! Proc index used for exchanging ghost VDF
    integer :: iProcPrev, iProcNext
    ! loop variables
    integer :: i, j, k, iStep
    ! time step and iterations
    real :: dt
    real :: dtLocal_III(nPhiPerp,nThetaPerp,nRPerp)
    integer :: nStep

    ! ghost VDF when sendrecv across prev-current-next processors
    real :: DistrPrevProc_G(0:nP+1, 1:nMu, nPhiPerp, nThetaPerp), &
         DistrNextProc_G(0:nP+1, 1:nMu, nPhiPerp, nThetaPerp)
    real :: sourceIncrement_5D(nP,nMu,nPhiPerp,nThetaPerp,iRStart:iREnd)
    character(len=*), parameter:: NameSub = 'solver_fvm_diffuse3d'
    !--------------------------------------------------------------------------
    iProcPrev = iProc - 1
    iProcNext = iProc + 1
    source_5D = 0.0

    if(IsSteadyStateIn) then
       ! Steady-state mode: Only 1 step, no use of dt indeed
       nStep = 1
       dt = 0.0
    else
       ! Time-accurate mode: Use exact dt; otherwise, no time marching
       ! Stability check: Determine maximal allowed global time step:
       ! dt <= 0.5*min_{i,j,k} [D_{ijk} * (
       !       1/DeltaR_i**2 + 1/(R_i*Delta(theta_j))**2 +
       !       1/(R_i*sin(theta_j)*Delta(phi_k))**2 )]^{-1}
       dtLocal_III = 0.0
       do i = 1, nRPerp
          do j = 1, nThetaPerp
             do k = 1, nPhiPerp
                dtLocal_III(k,j,i) = 0.5/(1.0/dRPerpFace_I(i)**2 + &
                     1.0/(RPerp_C(i)*dThetaPerp)**2 + &
                     1.0/(RPerp_C(i)*ThetaPerpSin_C(j)*dPhiPerp)**2)/ &
                     maxval(DPerp_5D(:,:,k,j,i), mask=(DPerp_5D(:,:,k,j,i)>0))
             end do
          end do
       end do
       dt = minval(dtLocal_III, mask=(dtLocal_III>0))
       nStep = int(dtIn/dt) + 1
       dt = dtIn/real(nStep)

       ! Output test information
       if(DoTestDPerp) &
            write(*,*) 'DPerp: Stable (Used) dt=', dt, 'with nStep=', nStep
    end if

    ! Time stepping: Diffusion in 3D by FVM
    DIFFUSE3D:do iStep = 1, nStep
       ! Exchange ghostVDF: iProcPrev |<=| (Start) iProc (End) |=>| iProcNext
       call exchange_ghostVDF
       ! Compute internal fluxes of the FVM solver = source term of df/dt
       call calc_fvm_netflux
       ! Update source_5D (= df/dt) and DistrPerp_5D
       source_5D(1:nP,:,:,:,:) = source_5D(1:nP,:,:,:,:) + sourceIncrement_5D
       DistrPerp_5D(1:nP,:,:,:,:) = DistrPerp_5D(1:nP,:,:,:,:) + &
            sourceIncrement_5D*dt   ! f^{n+1} = f^{n} + (df/dt)*delta_t
    end do DIFFUSE3D

  contains
    !==========================================================================
    subroutine exchange_ghostVDF
      ! Exchange ghostVDF: iProcPrev |<=| (Start) iProc (End) |=>| iProcNext
      !------------------------------------------------------------------------
      ! Exchange VDF at ghost cells 1: iProc End |=>| iProcNext Start
      if(iProcNext < nProc) then
         call MPI_SENDRECV(DistrPerp_5D(:,:,:,:,iREnd), &
              (nP+2)*nMu*nPhiPerp*nThetaPerp, &
              MPI_REAL, iProcNext, 0, &
              DistrNextProc_G, (nP+2)*nMu*nPhiPerp*nThetaPerp, &
              MPI_REAL, iProcNext, 1, &
              iComm, MPI_STATUS_IGNORE, iError)
      else
         ! zero-gradient (Neumann BC) at outer boundary
         DistrNextProc_G = DistrPerp_5D(:,:,:,:,iREnd)
      end if

      ! Exchange VDF at ghost cells 2: iProcPrev End |<=| iProc Start
      if(iProcPrev >= 0) then
         call MPI_SENDRECV(DistrPerp_5D(:,:,:,:,iRStart), &
              (nP+2)*nMu*nPhiPerp*nThetaPerp, &
              MPI_REAL, iProcPrev, 1, &
              DistrPrevProc_G, (nP+2)*nMu*nPhiPerp*nThetaPerp, &
              MPI_REAL, iProcPrev, 0, &
              iComm, MPI_STATUS_IGNORE, iError)
      else
         ! zero-gradient (Neumann BC) at inner boundary
         DistrPrevProc_G = DistrPerp_5D(:,:,:,:,iRStart)
      end if

    end subroutine exchange_ghostVDF
    !==========================================================================
    subroutine calc_fvm_netflux
      ! calculate internal net fluxes in the FVM solver

      integer :: jPlus, jMinus, kPlus, kMinus
      real    :: VolumeInv
      real, dimension(1:nP, 1:nMu) :: DPerpPlus_II, DPerpMinus_II
      real, dimension(0:nP+1, 1:nMu) :: DistrPlus_II, DistrMinus_II
      !------------------------------------------------------------------------
      sourceIncrement_5D = 0.0
      do i = iRStart, iREnd
         do j = 1, nThetaPerp
            ! Ready for Theta fluxes
            jPlus = min(j+1, nThetaPerp) ! j or nThetaPerp
            jMinus = max(j-1, 1) ! j or 1

            do k = 1, nPhiPerp
               VolumeInv = 1.0/Volume_CB(k,j,i)

               ! Part 1 -- Face_r: Radial fluxes
               ! VDF at i+1 (ip)
               if(i < iREnd) then
                  DistrPlus_II = DistrPerp_5D(:,:,k,j,i+1)
               else
                  DistrPlus_II = DistrNextProc_G(:,:,k,j)
               end if
               ! VDF at i-1 (im)
               if(i > iRStart) then
                  DistrMinus_II = DistrPerp_5D(:,:,k,j,i-1)
               else
                  DistrMinus_II = DistrPrevProc_G(:,:,k,j)
               end if
               ! DPerp coefficient at face(i, i+1)
               if(i < nRPerp) then
                  DPerpPlus_II = 0.5*(Dperp_5D(:,:,k,j,i) &
                       + Dperp_5D(:,:,k,j,i+1))
               else
                  DPerpPlus_II = Dperp_5D(:,:,k,j,i) + &
                       0.5*(Dperp_5D(:,:,k,j,i) - Dperp_5D(:,:,k,j,i-1))
               end if
               ! DPerp coefficient at face(i-1, i)
               if(i > 1) then
                  DPerpMinus_II = 0.5*(Dperp_5D(:,:,k,j,i) &
                       + Dperp_5D(:,:,k,j,i-1))
               else
                  DPerpMinus_II = Dperp_5D(:,:,k,j,i) - &
                       0.5*(Dperp_5D(:,:,k,j,i+1) - Dperp_5D(:,:,k,j,i))
               end if
               ! FVM net flux: sourceIncrement_5D += (flux_ip - flux_im)
               sourceIncrement_5D(:,:,k,j,i) = sourceIncrement_5D(:,:,k,j,i)+ &
                    DPerpPlus_II &        ! + DPerp at face(i, ip)
                    *(DistrPlus_II(1:nP,:) - DistrPerp_5D(1:nP,:,k,j,i))&
                    /dRPerpFace_I(i) &    ! * gradF at face(i, ip)
                    *AreaR_IIF(k,j,i+1) & ! * area of face(i, ip)
                    *VolumeInv            ! / volume of (k, j, i)
               sourceIncrement_5D(:,:,k,j,i) = sourceIncrement_5D(:,:,k,j,i)- &
                    DPerpMinus_II &       ! - DPerp at face(im, i)
                    *(DistrPerp_5D(1:nP,:,k,j,i) - DistrMinus_II(1:nP,:))&
                    /dRPerpFace_I(i) &    ! * gradF at face(im, i)
                    *AreaR_IIF(k,j,i) &   ! * area of face(im, i)
                    *VolumeInv            ! / volume of (k, j, i)

               ! Part 2 -- Face_theta: Theta fluxes
               ! DPerp coefficient at face(j, j+1)
               if(j < nThetaPerp) then
                  DPerpPlus_II = 0.5*(Dperp_5D(:,:,k,j,i) &
                       + Dperp_5D(:,:,k,j+1,i))
               else
                  DPerpPlus_II = Dperp_5D(:,:,k,j,i) + &
                       0.5*(Dperp_5D(:,:,k,j,i) - Dperp_5D(:,:,k,j-1,i))
               end if
               ! DPerp coefficient at face(j-1, j)
               if(j > 1) then
                  DPerpMinus_II = 0.5*(Dperp_5D(:,:,k,j-1,i) &
                       + Dperp_5D(:,:,k,j,i))
               else
                  DPerpMinus_II = Dperp_5D(:,:,k,j,i) - &
                       0.5*(Dperp_5D(:,:,k,j+1,i) - Dperp_5D(:,:,k,j,i))
               end if
               ! FVM net flux: sourceIncrement_5D += (flux_jp - flux_jm)
               sourceIncrement_5D(:,:,k,j,i) = sourceIncrement_5D(:,:,k,j,i)+ &
                    DPerpPlus_II &              ! + DPerp at face(j, jp)
                    *(DistrPerp_5D(1:nP,:,k,jPlus,i)&
                    -DistrPerp_5D(1:nP,:,k,j,i))&
                    /(RPerp_C(i)*dThetaPerp) &  ! * gradF at face(j, jp)
                    *AreaTheta_IFI(k,j+1,i) &   ! * area of face(j, jp)
                    *VolumeInv                  ! / volume of (k, j, i)
               sourceIncrement_5D(:,:,k,j,i) = sourceIncrement_5D(:,:,k,j,i)- &
                    DPerpMinus_II &             ! - DPerp at face(jm, j)
                    *(DistrPerp_5D(1:nP,:,k,j,i)&
                    -DistrPerp_5D(1:nP,:,k,jMinus,i))&
                    /(RPerp_C(i)*dThetaPerp) &  ! * gradF at face(jm, j)
                    *AreaTheta_IFI(k,j,i) &     ! * area of face(jm, j)
                    *VolumeInv                  ! / volume of (k, j, i)

               ! Part 3 -- Face_phi: Phi fluxes
               kPlus = mod(k,nPhiPerp)+1              ! next index with wrap
               kMinus = mod(k-2+nPhiPerp,nPhiPerp)+1  ! prev index with wrap
               ! DPerp coefficient at face(k, k+1)
               DPerpPlus_II = 0.5*(Dperp_5D(:,:,k,j,i) &
                    + Dperp_5D(:,:,kPlus,j,i))
               ! DPerp coefficient at face(k-1, k)
               DPerpMinus_II = 0.5*(Dperp_5D(:,:,kMinus,j,i) &
                    + Dperp_5D(:,:,k,j,i))
               ! FVM net flux: sourceIncrement_5D += (flux_kp - flux_km)
               sourceIncrement_5D(:,:,k,j,i) = sourceIncrement_5D(:,:,k,j,i)+ &
                    DPerpPlus_II &              ! + DPerp at face(k, kp)
                    *(DistrPerp_5D(1:nP,:,kPlus,j,i)& ! * gradF at face(k, kp)
                    -DistrPerp_5D(1:nP,:,k,j,i))&
                    /(RPerp_C(i)*ThetaPerpSin_C(j)*dPhiPerp) &
                    *AreaPhi_FII(kPlus,j,i) &   ! * area of face(k, kp)
                    *VolumeInv                  ! / volume of (k, j, i)
               sourceIncrement_5D(:,:,k,j,i) = sourceIncrement_5D(:,:,k,j,i)- &
                    DPerpMinus_II &             ! - DPerp at face(km, k)
                    *(DistrPerp_5D(1:nP,:,k,j,i)& ! * gradF at face(km, k)
                    -DistrPerp_5D(1:nP,:,kMinus,j,i))&
                    /(RPerp_C(i)*ThetaPerpSin_C(j)*dPhiPerp) &
                    *AreaPhi_FII(kMinus,j,i) &  ! * area of face(km, k)
                    *VolumeInv                  ! / volume of (k, j, i)
            end do
         end do
      end do

    end subroutine calc_fvm_netflux
    !==========================================================================
  end subroutine solver_fvm_diffuse3d
  !============================================================================
  subroutine interp_source_intersect(source_5D, source_IV)
    ! Interpolate the df/dt source term from the uniform grid to
    ! all intersection points of the lines and multiple slices

    use ModCoordTransform, ONLY: xyz_to_rlonlat
    use SP_ModTriangulate, ONLY: XyzOrig_DII
    real, intent(in) :: source_5D(0:nP+1, nMu, nPhiPerp, nThetaPerp, &
         iRPerpStart_I(iProc):iRPerpEnd_I(iProc))
    real, intent(out):: source_IV(nP, nMu, nLineAll, nRPerp)
    integer :: iRPerp, iLine, iPE
    integer :: iTheta1, iTheta2, iPhi1, iPhi2
    real    :: XyzPoint_D(X_:Z_), rPoint, lonPoint, latPoint, phiPoint
    real    :: thetaFrac, phiFrac
    character(len=*), parameter:: NameSub = 'interp_source_intersect'
    !--------------------------------------------------------------------------
    do iRPerp = iRPerpStart_I(iProc), iRPerpEnd_I(iProc)
       do iLine = 1, nLineAll
          ! Extract the intersection point coordinates
          XyzPoint_D = XyzOrig_DII(:, iLine, iRPerp)
          if(sum(XyzPoint_D**2) > cTiny) then
             ! CoordTransform: XyzPoint_D => (rPoint, lonPoint, phiPoint)
             call xyz_to_rlonlat(XyzPoint_D, rPoint, lonPoint, latPoint)
             phiPoint = 0.5*cPi - latPoint

             ! Find the indices of the quadrilateral vertices
             iTheta1 = floor(mod(lonPoint-PhiPerp_C(1), cTwoPi)/dPhiPerp) + 1
             iTheta2 = iTheta1 + 1
             iPhi1 = floor((phiPoint - ThetaPerp_C(1))/dThetaPerp) + 1
             iPhi2 = iPhi1 + 1
             ! Handle periodicity in the theta (longitude) direction
             if(iTheta2 > nPhiPerp) iTheta2 = 1  ! Wrap around
             ! Ensure indices are within bounds
             iTheta1 = max(1, min(iTheta1, nPhiPerp))
             iTheta2 = max(1, min(iTheta2, nPhiPerp))
             iPhi1 = max(1, min(iPhi1, nThetaPerp))
             iPhi2 = max(1, min(iPhi2, nThetaPerp))

             ! Compute fractional distances for interpolation
             thetaFrac = (lonPoint - PhiPerp_C(iTheta1))/dPhiPerp
             phiFrac = (phiPoint - ThetaPerp_C(iPhi1))/dThetaPerp
             ! Perform bilinear interpolation on the uniform LON-LAT grid
             source_IV(:,:,iLine,iRPerp) = (1.0-thetaFrac)*(1.0-phiFrac)* &
                  source_5D(1:nP, :, iTheta1, iPhi1, iRPerp) + &  ! value11
                  (1.0-thetaFrac)*phiFrac* &
                  source_5D(1:nP, :, iTheta1, iPhi2, iRPerp) + &  ! value12
                  thetaFrac * (1.0-phiFrac)* &
                  source_5D(1:nP, :, iTheta2, iPhi1, iRPerp) + &  ! value21
                  thetaFrac*phiFrac* &
                  source_5D(1:nP, :, iTheta2, iPhi2, iRPerp)      ! value22
          else
             source_IV(:,:,iLine,iRPerp) = 0.0
          end if
       end do
    end do

    ! Broadcast source_IV (line intersections, uniform grid) to all processes
    if(nProc > 1) then
       do iPE = 0, nProc-1
          call MPI_BCAST(source_IV(:,:,:,               &
               iRPerpStart_I(iPE):iRPerpEnd_I(iPE)),    &
               (iRPerpEnd_I(iPE)-iRPerpStart_I(iPE)+1)* &
               nP*nMu*nLineAll, MPI_REAL, iPE, iComm, iError)
       end do
    end if
    ! Check the error message from after interpolate_trmesh
    if(iError /= 0) then
       write(*,*) 'iProc = ', iProc, NameSub//&
            ': interpolate_trmesh for source_IV failed, DPerp stopped'
       RETURN
    end if
  end subroutine interp_source_intersect
  !============================================================================
  subroutine interp_source_linenode(source_IV, dtIn_CB)
    ! Interpolate the df/dt source term from the intersection points of the
    ! lines and multiple slices to the original points along the field line

    use SP_ModGrid, ONLY: Used_B, nVertex_B, iLineAll0
    real, intent(in):: source_IV(nP, nMu, nLineAll, nRPerp)
    real, intent(in):: dtIn_CB(nP, nMu, nVertexMax, nLine)
    integer :: iLine, iX, iPoint, iLineAll
    real :: rNode, r1, r2, Weight
    real :: v1_II(nP, nMu), v2_II(nP, nMu)
    character(len=*), parameter:: NameSub = 'interp_source_linenode'
    !--------------------------------------------------------------------------
    ! Loop over all field lines on this processor
    LINE:do iLine = 1, nLine
       if(.not.Used_B(iLine)) CYCLE LINE
       iLineAll = iLine + iLineAll0
       ! Loop over all nodes along the valid field line
       do iX = 1, nVertex_B(iLine)
          ! Extract the radial coordinate of the current node
          rNode = State_VIB(R_, iX, iLine)*Io2Si_V(UnitX_)

          ! Check that rNode is within grid range
          if(rNode <= RPerp_C(nRPerp)) then
             ! Find first index where grid radius exceeds rNode
             iPoint = minloc(RPerp_C(1:nRPerp), DIM=1, &
                  MASK = RPerp_C(1:nRPerp) > rNode)

             if(iPoint > 1) then
                ! rNode is between r1 and r2
                r1 = RPerp_C(iPoint-1)
                r2 = RPerp_C(iPoint)
                ! Linear interpolation between r1 and r2
                Weight = (rNode - r1)/(r2 - r1)
                v1_II = source_IV(:, :, iLineAll, iPoint-1)
                v2_II = source_IV(:, :, iLineAll, iPoint)
                Distribution_CB(1:nP, :, iX, iLine) = &
                     Distribution_CB(1:nP, :, iX, iLine) + &
                     (v1_II+(v2_II-v1_II)*Weight)*dtIn_CB(:, :, iX, iLine)
             else
                ! rNode is at or below first grid point
                Distribution_CB(1:nP, :, iX, iLine) = &
                     Distribution_CB(1:nP, :, iX, iLine) + &
                     source_IV(:, :, iLineAll, 1)*dtIn_CB(:, :, iX, iLine)
             end if
          end if
       end do

       ! Check if the VDF includes negative values after Dperp
       call check_dist_neg(NameSub// &
            ' after perpendicular diffusion', 1, nVertex_B(iLine), iLine)
       if(IsDistNeg) RETURN
    end do LINE
  end subroutine interp_source_linenode
  !============================================================================
end module SP_ModPerpDiffusion
!==============================================================================
