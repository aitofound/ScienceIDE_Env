!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModAdvancePoisson

  ! High resolution finite volume method for kinetic equations
  ! with Poisson brackets (Sokolov et al., 2023)
  ! See https://doi.org/10.1016/j.jcp.2023.111923

  use SP_ModSize, ONLY: nVertexMax
  use SP_ModGrid, ONLY: nP, nMu, State_VIB
  use SP_ModBc, ONLY: set_momentum_bc, set_VDF2, set_VDF3
  use SP_ModDistribution, ONLY: VolumeP_I, Momentum3_F, &
       Distribution_CB, IsDistNeg, check_dist_neg, dLogP
  use SP_ModDiffusion, ONLY: UseDiffusion, diffuse_distribution
  use SP_ModPerpDiffusion, ONLY: UseDiffusionPerp, Dt_CB
  use ModUtilities, ONLY: CON_stop
  use ModPoissonBracket, ONLY: explicit

  implicit none

  PRIVATE ! Except

  SAVE

  ! Public members:
  public:: read_param                  ! Read parameters
  ! For solving the Parker transport equation
  public:: advect_via_poisson_parker   ! Time-accurate advance through given Dt
  public:: iterate_poisson_parker      ! Local time-stepping for steady states
  ! For solving the focused transport equation with pitch angles
  public:: calc_states_poisson_focused ! Calculate states for the focused Eqn.
  public:: advect_via_poisson_focused  ! Time-accurate advance through given Dt
  public:: iterate_poisson_focused     ! Local time-stepping for steady states

  ! Whether to include the betatron acceleration in focused transport equation
  logical, public :: UseBetatron = .false.
  ! Whether to include the inertial force in focused transport equation
  logical, public :: UseInertialForce = .false.
  ! Whether to include the pitch angle scattering term
  logical, public :: UseMuScattering = .false.
  ! Time derivative over DtFull (linear interpolation for time):
  real, public, dimension(nVertexMax) :: DbuDt_C
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#FOCUSEDTRANSPORT')
       call read_var('UseBetatron',      UseBetatron)
       call read_var('UseInertialForce', UseInertialForce)
       call read_var('UseMuScattering',  UseMuScattering)
    case default
       call CON_stop(NameSub//': Unknown command '//NameCommand)
    end select

  end subroutine read_param
  !============================================================================
  subroutine advect_via_poisson_parker(iLine, nX, iShock, &
       tFinal, CflIn, nOldSi_I, nSi_I, BSi_I, Mass_C)
    ! advect via Possion Bracket scheme: (p**3/3, s_L)
    ! diffuse the distribution function at each time step

    ! INPUTS:
    integer, intent(in) :: iLine, iShock ! Indices of line and shock front
    integer, intent(in) :: nX            ! Number of meshes along s_L axis
    real,    intent(in) :: tFinal        ! Time interval to advance through
    real,    intent(in) :: CflIn         ! Input CFL number
    ! Input variables for diffusion, assuming at the time of tFinal
    real,    intent(in) :: nOldSi_I(nX), nSi_I(nX), BSi_I(nX), Mass_C(nX)
    ! Extended arrays for implementation of the Poisson bracket scheme
    ! VolumeXStart_I: geometric volume when the subroutine starts
    ! VolumeXEnd_I: geometric volume when the subroutine ends
    ! dVolumeXDt_I: time derivative of geometric volume
    real, dimension(0:nX+1) :: VolumeXStart_I, VolumeXEnd_I, dVolumeXDt_I
    ! VolumeOld_G: total control volume of last each iteration
    ! Volume_G: total control volume at the end of each iteration
    ! dVolumeDt_G: total control time derivative
    real, dimension(0:nP+1, 0:nX+1) :: VolumeOld_G, Volume_G, dVolumeDt_G
    ! DeltaHamiltonian for {tau, p**3/3} => \tilde\deltaH
    real :: dHamiltonian01_FX(-1:nP+1, 0:nX+1)
    ! Extended array for distribution function
    real :: VDF_G(-1:nP+2, -1:nX+2)
    ! Advection term
    real :: Source_C(nP, nX)
    ! Time, ranging from 0 to tFinal
    real :: Time
    ! Time step
    real :: Dt
    ! Prediction for the next time step:
    real :: DtNext
    ! Loop variable
    integer :: iX

    ! Now, time-accurate Poisson bracket advection scheme for Parker Eqn.
    character(len=*), parameter:: NameSub = 'advect_via_poisson_parker'
    !--------------------------------------------------------------------------
    IsDistNeg = .false.

    ! Initialize arrays
    ! Geometric volume: for DeltaS/B, with 1 ghost point at the boundary
    ! Start volume
    VolumeXStart_I(1:nX) = Mass_C/nOldSi_I(1:nX)
    VolumeXStart_I(0)    = VolumeXStart_I(1)
    VolumeXStart_I(nX+1) = VolumeXStart_I(nX)
    ! End volume
    VolumeXEnd_I(1:nX)   = Mass_C/nSi_I(1:nX)
    VolumeXEnd_I(0)      = VolumeXEnd_I(1)
    VolumeXEnd_I(nX+1)   = VolumeXEnd_I(nX)
    ! Time derivative
    dVolumeXDt_I         = (VolumeXEnd_I - VolumeXStart_I)/tFinal
    ! Total control volume: initial and time derivative
    do iX = 0, nX+1
       Volume_G(:,iX)    = VolumeP_I*VolumeXStart_I(iX)
       dVolumeDt_G(:,iX) = VolumeP_I*dVolumeXDt_I(iX)
       ! Calculate 1st Hamiltonian function used in the time-dependent
       ! poisson bracket: {f_jk; p**3/3 * DeltaS/B}_{tau, p**3/3}
       dHamiltonian01_FX(:,iX) = -Momentum3_F*dVolumeXDt_I(iX)
    end do
    ! Time initialization
    Time = 0.0

    ! Update Bc for VDF at minimal energy, at nP = 0
    call set_momentum_bc(iLine, nX, nSi_I, iShock, log(nSi_I/nOldSi_I))
    ! Trial time step: Get DtNext
    call set_VDF2(iLine, nX, VDF_G) ! Set the VDF first
    call explicit(nP, nX, VDF_G, Volume_G, Source_C,  &
         dHamiltonian01_FX = dHamiltonian01_FX,       &
         dVolumeDt_G = dVolumeDt_G,                   &
         CFLIn = CflIn, DtOut = DtNext)

    ! Advection by the single-Poisson-bracket scheme
    do
       ! Update Time step
       Dt = min(DtNext, tFinal - Time)
       ! Update Volumes
       VolumeOld_G = Volume_G
       Volume_G    = VolumeOld_G + Dt*dVolumeDt_G

       ! Update Bc for VDF
       call set_VDF2(iLine, nX, VDF_G)
       ! Advance by single-Poisson-bracket scheme
       call explicit(nP, nX, VDF_G, Volume_G, Source_C,  &
            dHamiltonian01_FX = dHamiltonian01_FX,       &
            dVolumeDt_G = dVolumeDt_G,                   &
            DtIn = Dt,         & ! Input time step, which may be reduced
            CFLIn = CflIn,     & ! Input CFL to calculate next time step
            DtOut = DtNext)

       ! May need to correct the volume if the time step has been reduced
       Volume_G = VolumeOld_G + Dt*dVolumeDt_G
       ! Update velocity distribution function
       Distribution_CB(1:nP, 1, 1:nX, iLine) = &
            Distribution_CB(1:nP, 1, 1:nX, iLine) + &
            Source_C(1:nP, 1:nX)
       ! Check if the VDF includes negative values
       call check_dist_neg(NameSub, 1, nX, iLine)
       if(IsDistNeg) RETURN

       ! Diffuse the distribution function
       if(UseDiffusion) then
          call diffuse_distribution(iLine, nX, &
               iShock, nSi_I, BSi_I, Dt, &
               LowerEndSpectrumIn_I=VDF_G(1:nP, 0), &
               UpperEndSpectrumIn_I=VDF_G(1:nP, nX+1))
          ! Check if the VDF includes negative values after diffusion
          call check_dist_neg(NameSub//' after diffusion', 1, nX, iLine)
          if(IsDistNeg) RETURN
       end if

       ! Update time
       Time = Time + Dt
       if(Time > tFinal - 1.0e-8*DtNext) EXIT
    end do

  end subroutine advect_via_poisson_parker
  !============================================================================
  subroutine iterate_poisson_parker(iLine, nX, iShock, CflIn, BSi_I, nSi_I)
    ! Advect via Possion Bracket scheme to the steady state: (p**3/3, s_L)
    ! First advect and then diffuse the VDF by splitting method

    use SP_ModGrid, ONLY: D_, U_
    use SP_ModUnit, ONLY: UnitX_, Io2Si_V
    ! INPUTS:
    integer, intent(in) :: iLine, iShock ! Indices of line and shock front
    integer, intent(in) :: nX            ! Number of meshes along s_L axis
    real,    intent(in) :: CflIn         ! Input CFL number
    ! Input variables for diffusion
    real,    intent(in) :: BSi_I(nX), nSi_I(nX)
    ! Extended arrays for implementation of the Poisson bracket scheme
    ! Array for u=\vec{u}*\vec{B}/|B| and distance between adjacent meshes
    real :: uSi_F(1:nX-1), DsMeshSi_I(1:nX-1)
    ! Array of 1/B at the cell- and face-center
    real :: InvBSi_C(nX), InvBSi_F(nX)
    ! Volume_G: total control volume at the end of each iteration
    real :: Volume_G(0:nP+1, 0:nX+1)
    ! VolumeX_I: geometric volume = distance between two geometric faces
    real :: VolumeX_I(0:nX+1)
    ! u/B=\vec{u}*\vec{B}/|B|**2 variable at face of s_L
    real :: uOverBSi_F(-1:nX+1)
    ! Hamiltonian at cell face: p**3/3 and s_L
    real :: Hamiltonian12_N(-1:nP+1, -1:nX+1)
    ! Extended array for distribution function
    real :: VDF_G(-1:nP+2, -1:nX+2)
    ! Advection term
    real :: Source_C(nP, nX)
    ! Time step
    real :: Dt_C(nP, nX)

    ! Now, steady-state Poisson bracket advection scheme for Parker Eqn.
    character(len=*), parameter:: NameSub = 'iterate_poisson_parker'
    !--------------------------------------------------------------------------

    ! Calculate u/B = \vec{u}*\vec{B} / |\vec{B}|**2 at cell face
    ! uSi_F with the index of "i" is the value at the face between
    ! the mesh "i" and "i+1"
    uSi_F(1:nX-1) = State_VIB(U_, 1:nX-1, iLine)
    ! Calculate 1/B at cell- and face-center
    InvBSi_C         = 1.0/BSi_I
    InvBSi_F(1:nX-1) = 0.5*(InvBSi_C(1:nX-1) + InvBSi_C(2:nX))

    ! u/B with the index of "i" is the value at the face between
    ! the mesh "i" and "i+1"
    ! Average 1/B and multiply by uSi at face centers
    uOverBSi_F(1:nX-1) = uSi_F(1:nX-1)*InvBSi_F(1:nX-1)
    uOverBSi_F(0 )     = uSi_F(1)*InvBSi_C(1)
    uOverBSi_F(-1)     = uOverBSi_F(0)
    uOverBSi_F(nX)     = uSi_F(nX-1)*InvBSi_C(nX)
    uOverBSi_F(nX+1)   = uOverBSi_F(nX)

    ! In M-FLAMPA DsMeshSi_I(i) is the distance between meshes i and i+1
    DsMeshSi_I(1:nX-1) = State_VIB(D_, 1:nX-1, iLine)*Io2Si_V(UnitX_)
    ! Initialize arrays
    VolumeX_I(2:nX-1) = InvBSi_C(2:nX-1)* &
         0.5*(DsMeshSi_I(2:nX-1) + DsMeshSi_I(1:nX-2))
    VolumeX_I(1)      = InvBSi_C(1)*DsMeshSi_I(1)
    VolumeX_I(0)      = VolumeX_I(1)
    VolumeX_I(nX)     = InvBSi_C(nX)*DsMeshSi_I(nX-1)
    VolumeX_I(nX+1)   = VolumeX_I(nX)
    ! Calculate total control volume
    Volume_G(0:nP+1, 0:nX+1) = spread(VolumeP_I, DIM=2, NCOPIES=nX+2)* &
         spread(VolumeX_I, DIM=1, NCOPIES=nP+2)

    ! Hamiltonian_12 = -(u/B)*(p**3/3) at cell face, for {p**3/3, s_L}
    Hamiltonian12_N(-1:nP+1, -1:nX+1) = -spread(Momentum3_F, DIM=2, &
         NCOPIES=nX+3)* spread(uOverBSi_F, DIM=1, NCOPIES=nP+3)

    ! Update bc for at minimal energy, at nP = 0
    call set_momentum_bc(iLine, nX, nSi_I, iShock)
    ! Update Bc for VDF
    call set_VDF2(iLine, nX, VDF_G)
    call explicit(nP, nX, VDF_G, Volume_G, Source_C, &
         Hamiltonian12_N=Hamiltonian12_N, CFLIn=CflIn, &
         IsSteadyState=.true., DtOut_C=Dt_C)

    ! Update velocity distribution function
    Distribution_CB(1:nP, 1, 1:nX, iLine) = &
         Distribution_CB(1:nP, 1, 1:nX, iLine) + Source_C(1:nP, 1:nX)
    ! Check if the VDF includes negative values
    call check_dist_neg(NameSub, 1, nX, iLine)
    if(IsDistNeg) RETURN
    ! Save local time stepping
    if(UseDiffusionPerp) Dt_CB(:, nMu, 1:nX, iLine) = Dt_C

    ! Diffuse the distribution function
    if(UseDiffusion) then
       ! with lower or upper end BCs
       call diffuse_distribution(iLine, nX, &
            iShock, nSi_I, BSi_I, Dt_C(1:nP, 1:nX), &
            LowerEndSpectrumIn_I=VDF_G(1:nP, 0), &
            UpperEndSpectrumIn_I=VDF_G(1:nP, nX+1))
       ! Check if the VDF includes negative values after diffusion
       call check_dist_neg(NameSub//' after diffusion', 1, nX, iLine)
       if(IsDistNeg) RETURN
    end if

  end subroutine iterate_poisson_parker
  !============================================================================
  subroutine calc_states_poisson_focused(iLine, nX, DtFull)
    ! Calculate time derivatives from the input files, used for focused
    ! transport equation, if UseBetatron or UseInertialForce is .true.

    use SP_ModGrid, ONLY: UOld_, U_
    ! Line number and number along grid axis
    integer, intent(in) :: iLine, nX
    ! Time difference over the entire loop: DtFull
    real,    intent(in) :: DtFull
    ! Calculate values with time derivatives: Use linear interpolation later
    ! to get the data of each time step from every to consecutive files
    real :: DbuDt_F(1:nX-1)
    !--------------------------------------------------------------------------
    if(.not.UseInertialForce) RETURN

    ! Now we use the inertial force in focused transport equation
    ! Calculate D(\vec{b}*\vec{u})/DtFull, for Dt in nProgress at face center
    DbuDt_F(1:nX-1) = (State_VIB(U_, 1:nX-1, iLine) - &
         State_VIB(UOld_, 1:nX-1, iLine))/DtFull
    ! Then we interpolate this term to the cell center
    DbuDt_C(2:nX-1) = 0.5*(DbuDt_F(1:nX-2) + DbuDt_F(2:nX-1))
    DbuDt_C(1)      = DbuDt_F(1)    - (DbuDt_C(2)    - DbuDt_F(1))
    DbuDt_C(nX)     = DbuDt_F(nX-1) + (DbuDt_F(nX-1) - DbuDt_C(nX-1))

  end subroutine calc_states_poisson_focused
  !============================================================================
  subroutine advect_via_poisson_focused(iLine, nX, iShock, &
       tFinal, CflIn, nOldSi_I, nSi_I, BOldSi_I, BSi_I, Mass_C)
    ! Advect via multiple Possion Bracket scheme for the focused transport
    ! equation considering pitch angle, may including: adiabatic focusing
    ! (conservation of the magnetic moment), cooling or accleration (with
    ! first-order Fermi accleration, betatron acceleration, by setting
    ! UseBetatron, and inertial forces, by setting UseInertialForce), and
    ! pitch-angle scattering terms, with (p**3/3, mu, s_L).
    ! Here, we diffuse the distribution function at each time step.

    use ModConst, ONLY: cProtonMass, cRmeProton, cLightSpeed
    use SP_ModDiffusion, ONLY: scatter_distribution
    use SP_ModDistribution, ONLY: DeltaMu, Mu_F, DeltaMu3_I, &
         GammaLorentz_F, VolumeE_I, VolumeE3_I
    use SP_ModUnit, ONLY: Si2Io_V, UnitEnergy_
    ! INPUTS:
    integer, intent(in) :: iLine, iShock ! Indices of line and shock front
    integer, intent(in) :: nX            ! Number of meshes along s_L axis
    real,    intent(in) :: tFinal        ! Time interval to advance through
    real,    intent(in) :: CflIn         ! Input CFL number
    ! Input variables at last and next time steps
    real,    intent(in) :: nOldSi_I(nX), nSi_I(nX), &
         BOldSi_I(nX), BSi_I(nX), Mass_C(nX)
    ! Extended arrays for implementation of the Poisson bracket scheme
    ! Loop variables
    integer :: iP, iMu, iX
    ! Inverse magetic field: time derivative, cell- and face-centered values
    real    :: dInvBSiDt_C(nX), InvBSi_C(nX), InvBSi_F(0:nX), dLogRho_I(nX)
    ! ------------ Volumes ------------
    ! VolumeStart_G: geometric volume when the subroutine starts
    ! VolumeX_I: geometric volume at the end of each iteration
    ! dVolumeXDt_I: time derivative of geometric volume
    real, dimension(0:nX+1) :: VolumeXStart_I, VolumeX_I, dVolumeXDt_I
    ! VolumeStart_G: total control volume when the subroutine starts
    ! Volume_G: total control volume at the end of each iteration
    ! dVolumeDt_G: time derivative of total control volume
    real, dimension(0:nP+1, 0:nMu+1, 0:nX+1) :: &
         VolumeStart_G, Volume_G, dVolumeDt_G
    ! ------------ Hamiltonian functions ------------
    ! Poisson bracket with regard to the first and second VARs
    ! considering the case when there are more than one Poisson bracket
    ! and when there is a Poisson bracket with respect to the time
    ! Finally we integrate over the other VARs to get \tilde\deltaH
    real    :: dHamiltonian01_FX(-1:nP+1,  0:nMu+1,  0:nX+1) ! {tau, p**3/3}
    real    :: dHamiltonian02_FY( 0:nP+1, -1:nMu+1,  0:nX+1) ! {tau, mu}
    real    :: Hamiltonian12_N  (-1:nP+1, -1:nMu+1,  0:nX+1) ! {p**3/3, mu}
    real    :: Hamiltonian23_N  ( 0:nP+1, -1:nMu+1, -1:nX+1) ! {mu, s_L}
    ! Extended array for distribution function
    real    :: VDF_G(-1:nP+2, -1:nMu+2, -1:nX+2)
    ! Advection term
    real    :: Source_C(nP, nMu, nX)
    ! Time, ranging from TimeStart to tFinal
    real    :: Time
    ! Inverse of tFinal (DtProgress)
    real    :: InvtFinal
    ! Time step
    real    :: Dt
    ! Prediction of next time step:
    real    :: DtNext

    ! Now, time-accurate Poisson bracket advection scheme for focused Eqn.
    character(len=*), parameter:: NameSub = 'advect_via_poisson_focused'
    !--------------------------------------------------------------------------
    IsDistNeg = .false.
    InvtFinal = 1.0/tFinal
    dLogRho_I = log(nSi_I/nOldSi_I)

    ! Initialize time and arrays
    Time = 0.0
    call init_states_focused

    ! Here we would like to get the first trial of DtNext
    ! Update Bc for VDF at minimal energy, at nP = 0
    call set_momentum_bc(iLine, nX, nSi_I, iShock, dLogRho_I)
    call set_VDF3(iLine, nX, VDF_G) ! Set the VDF first
    call advance_poisson_focused   ! Now we get DtNext

    ! Advect by the multiple-Poisson-bracket scheme
    do
       ! Update Time step
       Dt = min(DtNext, tFinal - Time)

       ! Update Bc for at minimal energy, at nP = 0
       call set_momentum_bc(iLine, nX, nSi_I, iShock, dLogRho_I)
       ! Update Bc for VDF
       call set_VDF3(iLine, nX, VDF_G)
       ! Advance by the multiple-Poisson-bracket scheme
       call advance_poisson_focused(DtIn=Dt)

       ! Update Distribution_CB to the CURRENT time plus Dt, with no
       ! BC setups, only within the range of (1:nP, 1:nMu, 1:nX).
       Distribution_CB(1:nP, 1:nMu, 1:nX, iLine) =      &
            Distribution_CB(1:nP, 1:nMu, 1:nX, iLine) + &
            Source_C(1:nP, 1:nMu, 1:nX)
       ! Check if the VDF includes negative values after scattering
       call check_dist_neg(NameSub, 1, nX, iLine)
       if(IsDistNeg) RETURN

       ! For pitch angle scattering
       if(UseMuScattering) then
          call scatter_distribution(iLine, nX, nSi_I, BSi_I, Dt)
          ! Check if the VDF includes negative values after mu scattering
          call check_dist_neg(NameSub//' after mu scattering', 1, nX, iLine)
          if(IsDistNeg) RETURN
       else if(UseDiffusion) then
          ! For spatial diffusion
          call diffuse_distribution(iLine, nX, &
               iShock, nSi_I, BSi_I, Dt, &
               LowerEndSpectrumIn_II=VDF_G(1:nP, 1:nMu, 0), &
               UpperEndSpectrumIn_II=VDF_G(1:nP, 1:nMu, nX+1))

          ! Check if the VDF includes negative values after spatial diffusion
          call check_dist_neg(NameSub//' after spatial diffusion',1,nX,iLine)
          if(IsDistNeg) RETURN
       end if

       ! Update time
       Time = Time + Dt
       if(Time > tFinal - 1.0e-8*DtNext) EXIT
    end do

  contains
    !==========================================================================
    subroutine init_states_focused

      ! Initialize states: VolumeX_I, Volume_G, 1/B, time derivatives,
      ! and time-related and non-related Hamiltonian functions

      ! Geometric volume when the subroutine ends
      real, dimension(0:nX+1) :: VolumeXEnd_I
      ! Inverse of B when the subroutine starts and ends
      real, dimension(1:nX  ) :: InvBOldSi_I, InvBSi_I

      !------------------------------------------------------------------------
      ! Geometric volume: for DeltaS/B, with 1 ghost point at the boundary
      ! Start volume: 1/n at Time = 0.0
      VolumeXStart_I(1:nX) = Mass_C/nOldSi_I
      VolumeXStart_I(0)    = VolumeXStart_I(1)
      VolumeXStart_I(nX+1) = VolumeXStart_I(nX)
      ! End volume: 1/n at Time = tFinal, i.e., DtProgress
      VolumeXEnd_I(1:nX)   = Mass_C/nSi_I
      VolumeXEnd_I(0)      = VolumeXEnd_I(1)
      VolumeXEnd_I(nX+1)   = VolumeXEnd_I(nX)
      ! Time derivative: D(1/n)/Dt
      dVolumeXDt_I(0:nX+1) = (VolumeXEnd_I - VolumeXStart_I)*InvtFinal

      ! Magnetic-field-related variables:
      ! Start cell-centered 1/B
      InvBOldSi_I = 1.0/BOldSi_I
      ! End cell-centered 1/B
      InvBSi_I    = 1.0/BSi_I
      ! Time derivative: D(1/B)/Dt
      dInvBSiDt_C = (InvBSi_I - InvBOldSi_I)*InvtFinal
      ! Averaged 1/B over the whole time step
      InvBSi_C(1:nX)   = 0.5*(InvBOldSi_I + InvBSi_I)
      ! End Face-centered 1/B
      InvBSi_F(1:nX-1) = (InvBSi_C(2:nX) + InvBSi_C(1:nX-1))*0.5
      InvBSi_F(0 )     = InvBSi_C(1 ) - (InvBSi_C(2 ) - InvBSi_C(1   ))*0.5
      InvBSi_F(nX)     = InvBSi_C(nX) + (InvBSi_C(nX) - InvBSi_C(nX-1))*0.5

      ! Calculate total control volume and time-related Hamiltonian functions
      do iX = 0, nX+1
         ! Total control volume at the very beginning
         VolumeStart_G(:, :, iX) = VolumeXStart_I(iX)*DeltaMu* &
              spread(VolumeP_I, DIM=2, NCOPIES=nMu+2)
         ! Time derivative of total control volume
         dVolumeDt_G(:, :, iX) = dVolumeXDt_I(iX)*DeltaMu* &
              spread(VolumeP_I, DIM=2, NCOPIES=nMu+2)

         ! Calculate 1st Hamiltonian function in the time-dependent Poisson
         ! bracket: {f_jk; (3*mu**2)*(p**3/3)*(DeltaS/B)}_{tau, p**3/3} =>
         ! {f_jk; D(mu**3)*(p**3/3)*(DVx)}_{tau, p**3/3}, as \tilde\DeltaH
         dHamiltonian01_FX(:, 1:nMu, iX) = -dVolumeXDt_I(iX)* &
              spread(DeltaMu3_I, DIM=1, NCOPIES=nP+3)* &
              spread(Momentum3_F, DIM=2, NCOPIES=nMu)
         ! Calculate 2nd Hamiltonian function in the time-dependent Poisson
         ! bracket: {f_jk; mu*(1-mu**2)*(DeltaS/B)}_{tau, mu} =>
         ! {f_jk; mu*(1-mu**2)*(D(p**3/3)*DVx)}_{tau, mu} as \tilde\DeltaH
         dHamiltonian02_FY(:, 0:nMu, iX) = -dVolumeXDt_I(iX)* &
              spread(Mu_F*(1.0-Mu_F**2), DIM=1, NCOPIES=nP+2)* &
              spread(VolumeP_I, DIM=2, NCOPIES=nMu+1)
      end do
      ! BCs for time-related Hamiltonian functions: {f_jk; H}_{tau, ...}
      dHamiltonian01_FX(:,     0, :) = dHamiltonian01_FX(:,     1, :)
      dHamiltonian01_FX(:, nMu+1, :) = dHamiltonian01_FX(:, nMu-1, :)
      dHamiltonian02_FY(:,    -1, :) = dHamiltonian02_FY(:,     1, :)
      dHamiltonian02_FY(:, nMu+1, :) = dHamiltonian02_FY(:, nMu-1, :)

      ! Clean the Hamiltonian function for {f_jk; H_23}_{mu, s_L}
      Hamiltonian23_N = 0.0
      ! Calculate H_23 = Hamiltonian function for {f_jk; H_23}_{mu, s_L}
      call calc_hamiltonian_23

    end subroutine init_states_focused
    !==========================================================================
    subroutine advance_poisson_focused(DtIn)
      ! advance by the multiple-Poisson-bracket scheme for each time step
      ! split as a subroutine here for updating states at small time steps

      real, optional, intent(inout) :: DtIn ! Input time step
      ! Get VolumeX_I, Volume_G and Hamiltonian function at the current time,
      ! and then solve the advection equation by the Poisson bracket scheme
      !------------------------------------------------------------------------

      ! Update the geometric and total control volumes
      VolumeX_I = VolumeXStart_I + Time*dVolumeXDt_I
      Volume_G  = VolumeStart_G  + Time*dVolumeDt_G

      ! Clean the Hamiltonian function for {f_jk; H_12}_{p**3/3, mu},
      ! with time-dependent variables, needed to be updated at each time step
      Hamiltonian12_N = 0.0
      ! Calculate H_12 = Hamiltonian function for {f_jk; H_12}_{p**3/3, mu}
      call calc_hamiltonian_12

      ! Here we have three Lagrangian coordinates: (p**3/3, mu, s_L)
      if(present(DtIn)) then
         call explicit(nP, nMu, nX, VDF_G, Volume_G, Source_C, &
              dHamiltonian01_FX = dHamiltonian01_FX,           &
              dHamiltonian02_FY = dHamiltonian02_FY,           &
              Hamiltonian12_N = Hamiltonian12_N,               &
              Hamiltonian23_N = Hamiltonian23_N,               &
              dVolumeDt_G = dVolumeDt_G,                       &
              DtIn = DtIn, DtOut = DtNext, CFLIn = CflIn)
      else
         ! Not present DtIn for the first call
         call explicit(nP, nMu, nX, VDF_G, Volume_G, Source_C, &
              dHamiltonian01_FX = dHamiltonian01_FX,           &
              dHamiltonian02_FY = dHamiltonian02_FY,           &
              Hamiltonian12_N = Hamiltonian12_N,               &
              Hamiltonian23_N = Hamiltonian23_N,               &
              dVolumeDt_G = dVolumeDt_G,                       &
              DtOut = DtNext, CFLIn = CflIn)
      end if

    end subroutine advance_poisson_focused
    !==========================================================================
    subroutine calc_hamiltonian_12

      ! Calculate the Hamiltonian function for {f_jk, H_12}_{p**3/3, mu}:
      ! We split into two terms for
      !     (1) betatron acceleration: {f_jk; -mu*(1-mu**2)/2 *
      !        p**3/3 * 3*DeltaS/B * [D(1/B)/Dt]/(1/B) }_{p**3/3, mu}; and
      !     (2) inertial force: {f_jk; (1-mu**2)/2 * p**2 * GammaLorentz *
      !        ProtonMass * DeltaS/B * D(\vec{b}*\vec{u})/Dt }_{p**3/3, mu},
      ! then summed as H_12, at the faces of p**3/3 and mu => \tilde\deltaH
      !------------------------------------------------------------------------

      ! For the betatron acceleration
      if(UseBetatron) then
         do iX = 1, nX
            Hamiltonian12_N(:, 0:nMu, iX) = Hamiltonian12_N(:, 0:nMu, iX) - &
                 0.5*spread(Mu_F*(1.0-Mu_F**2), DIM=1, NCOPIES=nP+3)* &
                 3.0*spread(Momentum3_F, DIM=2, NCOPIES=nMu+1)* &
                 dInvBSiDt_C(iX)/InvBSi_C(iX)*VolumeX_I(iX)
         end do
      end if

      ! For the inertial force
      if(UseInertialForce) then
         do iX = 1, nX
            Hamiltonian12_N(:, 0:nMu, iX) = Hamiltonian12_N(:, 0:nMu, iX) + &
                 0.5*spread(1.0-Mu_F**2, DIM=1, NCOPIES=nP+3)* &
                 spread((Momentum3_F*3.0)**(2.0/3.0), DIM=2, NCOPIES=nMu+1)* &
                 spread(GammaLorentz_F, DIM=2, NCOPIES=nMu+1)* &
                 cProtonMass*DbuDt_C(iX)*VolumeX_I(iX)
         end do
      end if

      ! Boundary condition of Hamiltonian_12 function: ghost cells and symmetry
      Hamiltonian12_N(:,     :,    0) = Hamiltonian12_N(:,     :,  1)
      Hamiltonian12_N(:,     :, nX+1) = Hamiltonian12_N(:,     :, nX)
      Hamiltonian12_N(:,    -1,    :) = Hamiltonian12_N(:,     1,  :)
      Hamiltonian12_N(:, nMu+1,    :) = Hamiltonian12_N(:, nMu-1,  :)

    end subroutine calc_hamiltonian_12
    !==========================================================================
    subroutine calc_hamiltonian_23

      ! Calculate the Hamiltonian function for {f_jk, H_23}_{mu, s_L}:
      ! H_23 = (1-mu**2)/(2B)*(dEtot**3-3*ProtonMass**2*c**4*dEtot)/(3*c**2),
      ! at the faces of mu and s_L, and cell center of p**3/3 => \tilde\deltaH

      ! Considering the law of relativity, v = p*c**2/sqrt(p**2+(m*c**2)**2),
      ! calculated as a function of p. At first, the Hamiltonian function is
      ! H_23 = (1-mu**2)/(2B)*v, and we need to take the integral (for
      ! averaging over) of p**3/3. Finally, we derive such a form for H_23.
      ! Note that momentum in this term is non-related to the Lagrangian
      ! coordinates (mu, s_L), so it should be cell-centered for p**3/3.
      !------------------------------------------------------------------------
      do iX = 0, nX
         Hamiltonian23_N(:, 0:nMu, iX) = 0.5*InvBSi_F(iX)* &
              spread(1.0-Mu_F**2, DIM=1, NCOPIES=nP+2)* &
              spread((VolumeE3_I - 3.0*VolumeE_I*(cRmeProton* &
              Si2Io_V(UnitEnergy_))**2)/(3.0*cProtonMass* &
              cLightSpeed**2), DIM=2, NCOPIES=nMu+1)
      end do

      ! Boundary condition of Hamiltonian_23 function: ghost cells and symmetry
      Hamiltonian23_N(:,     :,   -1) = Hamiltonian23_N(:,     :,  1)
      Hamiltonian23_N(:,     :, nX+1) = Hamiltonian23_N(:,     :, nX)
      Hamiltonian23_N(:,    -1,    :) = Hamiltonian23_N(:,     1,  :)
      Hamiltonian23_N(:, nMu+1,    :) = Hamiltonian23_N(:, nMu-1,  :)

    end subroutine calc_hamiltonian_23
    !==========================================================================
  end subroutine advect_via_poisson_focused
  !============================================================================
  subroutine iterate_poisson_focused(iLine, nX, iShock, CflIn, BSi_I, nSi_I)
    ! Advect via Possion Bracket scheme to the steady state: (p**3/3, mu, s_L)
    ! First advect and then diffuse the VDF by splitting method

    use ModConst, ONLY: cProtonMass, cRmeProton, cLightSpeed
    use SP_ModDiffusion, ONLY: scatter_distribution
    use SP_ModDistribution, ONLY: DeltaMu, Mu_F, &
         GammaLorentz_F, VolumeE_I, VolumeE3_I
    use SP_ModGrid, ONLY: D_, U_
    use SP_ModUnit, ONLY: Io2Si_V, Si2Io_V, UnitX_, UnitEnergy_
    ! INPUTS:
    integer, intent(in) :: iLine, iShock ! Indices of line and shock front
    integer, intent(in) :: nX            ! Number of meshes along s_L axis
    real,    intent(in) :: CflIn         ! Input CFL number
    ! Input variables for diffusion
    real,    intent(in) :: BSi_I(nX), nSi_I(nX)
    ! Extended arrays for implementation of the Poisson bracket scheme
    ! Loop variables
    integer :: iX
    ! Inverse magetic field: cell- and face-centered values
    real    :: InvBSi_C(nX), InvBSi_F(0:nX)
    ! Array for u/B=\vec{u}*\vec{B}/|B|**2 at face-center
    real    :: uSi_F(1:nX-1)
    ! Array for cell- and face-center spacing
    real    :: DsMeshSi_I(1:nX-1), DsFaceSi_I(nX)
    ! Array for d(\vec{b}*\vec{u})/d(s_L) for the inertial force at cell center
    real    :: DbuDsSi_C(nX)
    ! Volume_G: total control volume at the end of each iteration
    real    :: Volume_G(0:nP+1, 0:nMu+1, 0:nX+1)
    ! VolumeX_I: geometric volume = distance between two geometric faces
    real    :: VolumeX_I(0:nX+1)
    ! VolumeInvB_I = 1/B at right face - left face
    real    :: VolumeInvB_I(nX)
    ! u/B variable at cell- and face-center
    real    :: uOverBSi_F(-1:nX+1), uOverBSi_C(nX)
    ! Hamiltonian functions at cell face => \tilde\deltaH
    real    :: Hamiltonian12_N(-1:nP+1, -1:nMu+1,  0:nX+1) ! {p**3/3, mu}
    real    :: Hamiltonian13_N(-1:nP+1,  0:nMu+1, -1:nX+1) ! {p**3/3, s_L}
    real    :: Hamiltonian23_N( 0:nP+1, -1:nMu+1, -1:nX+1) ! {mu, s_L}
    ! Extended array for distribution function
    real    :: VDF_G(-1:nP+2, -1:nMu+2, -1:nX+2)
    ! Advection term
    real    :: Source_C(nP, nMu, nX)
    ! Time step
    real    :: Dt_C(nP, nMu, nX)

    ! Now, steady-state Poisson bracket advection scheme for focused Eqn.

    character(len=*), parameter:: NameSub = 'iterate_poisson_focused'
    !--------------------------------------------------------------------------
    ! Initialize arrays
    call init_states_focused
    ! Calculate Hamiltonian functions
    call calc_hamiltonians

    ! Update bc for at minimal energy, at nP = 0
    call set_momentum_bc(iLine, nX, nSi_I, iShock)
    ! Update Bc for VDF
    call set_VDF3(iLine, nX, VDF_G)
    call explicit(nP, nMu, nX, VDF_G, Volume_G, Source_C, &
         Hamiltonian12_N=Hamiltonian12_N, &
         Hamiltonian13_N=Hamiltonian13_N, &
         Hamiltonian23_N=Hamiltonian23_N, &
         CFLIn=CflIn, IsSteadyState=.true., DtOut_C=Dt_C)

    ! Update velocity distribution function
    Distribution_CB(1:nP, 1:nMu, 1:nX, iLine) =      &
         Distribution_CB(1:nP, 1:nMu, 1:nX, iLine) + &
         Source_C(1:nP, 1:nMu, 1:nX)
    ! Check if the VDF includes negative values
    call check_dist_neg(NameSub, 1, nX, iLine)
    if(IsDistNeg) RETURN
    ! Save local time stepping
    if(UseDiffusionPerp) Dt_CB(:, :, 1:nX, iLine) = Dt_C

    ! For pitch angle scattering
    if(UseMuScattering) then
       call scatter_distribution(iLine, nX, nSi_I, BSi_I, Dt_C)
       ! Check if the VDF includes negative values after mu scattering
       call check_dist_neg(NameSub//' after mu scattering', 1, nX, iLine)
       if(IsDistNeg) RETURN
    end if
    if(UseDiffusion) then
       ! For spatial diffusion
       call diffuse_distribution(iLine, nX, &
            iShock, nSi_I, BSi_I, Dt_C, &
            LowerEndSpectrumIn_II= VDF_G(1:nP, 1:nMu, 0), &
            UpperEndSpectrumIn_II= VDF_G(1:nP, 1:nMu, nX+1))
       ! Check if the VDF includes negative values after spatial diffusion
       call check_dist_neg(NameSub//' after spatial diffusion', 1, nX, iLine)
       if(IsDistNeg) RETURN
    end if

  contains
    !==========================================================================
    subroutine init_states_focused

      ! Initialize states: 1/B, u/B, VolumeX_I and Volume_G arrays

      use ModNumConst, ONLY: cTiny
      ! Magnetic-field-related variables:
      ! Cell-centered 1/B
      !------------------------------------------------------------------------
      InvBSi_C = 1.0/BSi_I
      ! Face-centered 1/B
      InvBSi_F(1:nX-1) = (InvBSi_C(2:nX) + InvBSi_C(1:nX-1))*0.5
      InvBSi_F(0 )     = InvBSi_C(1 ) - 0.5*(InvBSi_C(2 ) - InvBSi_C(1   ))
      InvBSi_F(nX)     = InvBSi_C(nX) + 0.5*(InvBSi_C(nX) - InvBSi_C(nX-1))
      ! Volume of 1/B: Delta(1/B) = 1/B at right face - left face
      VolumeInvB_I(1:nX) = InvBSi_F(1:nX) - InvBSi_F(0:nX-1)

      ! Calculate u/B = \vec{u}*\vec{B} / |\vec{B}|**2 at cell face
      ! uSi_F with the index of "i" is the value at the face between
      ! the mesh "i" and "i+1"
      uSi_F(1:nX-1) = State_VIB(U_, 1:nX-1, iLine)
      ! u/B with the index of "i" is the value at the face between
      ! the mesh "i" and "i+1"
      ! Average 1/B and multiply by uSi, at face centers
      uOverBSi_F(1:nX-1) = uSi_F(1:nX-1)*InvBSi_F(1:nX-1)
      uOverBSi_F(0 )     = uSi_F(1)*InvBSi_F(0)
      uOverBSi_F(-1)     = uOverBSi_F(0)
      uOverBSi_F(nX)     = uSi_F(nX-1)*InvBSi_F(nX)
      uOverBSi_F(nX+1)   = uOverBSi_F(nX)
      ! Interpolate to get u/B at cell centers
      uOverBSi_C(1:nX)   = 0.5*(uOverBSi_F(0:nX-1) + uOverBSi_F(1:nX))

      ! In M-FLAMPA DsMeshSi_I(i) is the distance between centers of meshes
      ! i and i+1. Therefore,
      DsMeshSi_I(1:nX-1) = max(State_VIB(D_, 1:nX-1, iLine)* &
           Io2Si_V(UnitX_), cTiny)
      ! Within the framework of finite volume method, the cell volume
      ! is used, which is proportional to the distance between the faces
      ! bounding the volume with an index, i, which is half of sum of
      ! distance between meshes i-1 and i, i.e. DsMeshSi_I(i-1), and that
      ! between meshes i and i+1, which is DsMeshSi_I(i):
      DsFaceSi_I(2:nX-1) = 0.5*(DsMeshSi_I(1:nX-2)+DsMeshSi_I(2:nX-1))
      DsFaceSi_I(1)      = DsMeshSi_I(1)
      DsFaceSi_I(nX)     = DsMeshSi_I(nX-1)

      ! Calculate geometric volume, with one ghost cell at each side
      VolumeX_I(1:nX) = InvBSi_C*DsFaceSi_I(1:nX)
      VolumeX_I(0)    = VolumeX_I(1)
      VolumeX_I(nX+1) = VolumeX_I(nX)

      ! Calculate total control volume
      do iX = 0, nX+1
         Volume_G(:, :, iX) = VolumeX_I(iX)*DeltaMu* &
              spread(VolumeP_I, DIM=2, NCOPIES=nMu+2)
      end do

      ! Calculate d(\vec{b}*\vec{u})/d(s_L), for the inertial force
      if(UseInertialForce) then
         DbuDsSi_C(2:nX-1) = (uSi_F(2:nX-1) - uSi_F(1:nX-2))/DsFaceSi_I(2:nX-1)
         DbuDsSi_C(1)  = DbuDsSi_C(2)    - (DbuDsSi_C(3)    - DbuDsSi_C(2))
         DbuDsSi_C(nX) = DbuDsSi_C(nX-1) + (DbuDsSi_C(nX-1) - DbuDsSi_C(nX-2))
      end if

    end subroutine init_states_focused
    !==========================================================================
    subroutine calc_hamiltonians

      ! Calculate Hamiltonian functions: {f_jk; H_(..)}_{..., ...}
      ! where Coor1 is p**3/3, Coor2 is mu, and Coor3 is s_L.
      !------------------------------------------------------------------------

      ! Clean the Hamiltonian function for {f_jk; H_12}_{p**3/3, mu}
      Hamiltonian12_N = 0.0
      ! Calculate the Hamiltonian function for {f_jk, H_12}_{p**3/3, mu}:
      ! We split into three terms for
      !     (1) adiabatic cooling/heating and focusing terms:
      !        {f_jk; mu*(1-mu**2)/2 * p**3/3 * (rho*u/B)_jk *
      !        d(1/rho)/d(s_L) }_{p**3/3, mu}; and
      !     (2) betatron acceleration: {f_jk; -mu*(1-mu**2)/2 *
      !        p**3/3 * 3*u/B * [d(1/B)/d(s_L)]/(1/B) }_{p**3/3, mu}; and
      !     (3) inertial force: {f_jk; (1-mu**2)/2 * p**2 * GammaLorentz *
      !        ProtonMass * u/B * d(\vec{b}*\vec{u})/d(s_L) }_{p**3/3, mu},
      ! then summed as H_12, at the faces of p**3/3 and mu => \tilde\deltaH

      ! For adiabatic cooling/heating and focusing terms:
      do iX = 1, nX
         Hamiltonian12_N(:, 0:nMu, iX) = Hamiltonian12_N(:, 0:nMu, iX) + &
              0.5*spread(Mu_F*(1.0-Mu_F**2), DIM=1, NCOPIES=nP+3)* &
              3.0*spread(Momentum3_F, DIM=2, NCOPIES=nMu+1)*uOverBSi_C(iX)
      end do

      ! For the betatron acceleration
      if(UseBetatron) then
         do iX = 1, nX
            Hamiltonian12_N(:, 0:nMu, iX) = Hamiltonian12_N(:, 0:nMu, iX) - &
                 0.5*spread(Mu_F*(1.0-Mu_F**2), DIM=1, NCOPIES=nP+3)* &
                 3.0*spread(Momentum3_F, DIM=2, NCOPIES=nMu+1)* &
                 uOverBSi_C(iX)/InvBSi_F(iX)*VolumeInvB_I(iX)
         end do
      end if

      ! For the inertial force
      if(UseInertialForce) then
         do iX = 1, nX
            Hamiltonian12_N(:, 0:nMu, iX) = Hamiltonian12_N(:, 0:nMu, iX) + &
                 0.5*spread(1.0-Mu_F**2, DIM=1, NCOPIES=nP+3)* &
                 spread((Momentum3_F*3.0)**(2.0/3.0), DIM=2, NCOPIES=nMu+1)* &
                 spread(GammaLorentz_F, DIM=2, NCOPIES=nMu+1)* &
                 cProtonMass*uOverBSi_C(iX)*DbuDsSi_C(iX)*VolumeX_I(iX)
         end do
      end if

      ! Boundary condition of Hamiltonian_12 function: ghost cells and symmetry
      Hamiltonian12_N(:,     :,    0) = Hamiltonian12_N(:,     :,  1)
      Hamiltonian12_N(:,     :, nX+1) = Hamiltonian12_N(:,     :, nX)
      Hamiltonian12_N(:,    -1,    :) = Hamiltonian12_N(:,     1,  :)
      Hamiltonian12_N(:, nMu+1,    :) = Hamiltonian12_N(:, nMu-1,  :)

      !------------------------------------------------------------------------
      ! Clean the Hamiltonian function for {f_jk; H_13}_{p**3/3, s_L}
      Hamiltonian13_N = 0.0
      ! Calculate the Hamiltonian function for {f_jk, H_13}_{p**3/3, s_L}:
      ! H_13 = -(u/B)*(p**3/3),
      ! at the faces of p**3/3 and s_L, and cell center of mu => \tilde\deltaH
      do iX = -1, nX+1
         Hamiltonian13_N(:, :, iX) = -DeltaMu* &
              spread(Momentum3_F, DIM=2, NCOPIES=nMu+2)*uOverBSi_F(iX)
      end do

      !------------------------------------------------------------------------
      ! Clean the Hamiltonian function for {f_jk; H_23}_{mu, s_L}
      Hamiltonian23_N = 0.0
      ! Calculate the Hamiltonian function for {f_jk, H_23}_{mu, s_L}:
      ! H_23 = (1-mu**2)/(2B)*(dEtot**3-3*ProtonMass**2*c**4*dEtot)/(3*c**2),
      ! at the faces of mu and s_L, and cell center of p**3/3 => \tilde\deltaH
      do iX = 0, nX
         Hamiltonian23_N(:, 0:nMu, iX) = 0.5*InvBSi_F(iX)* &
              spread(1.0-Mu_F**2, DIM=1, NCOPIES=nP+2)* &
              spread((VolumeE3_I - 3.0*VolumeE_I*(cRmeProton* &
              Si2Io_V(UnitEnergy_))**2)/(3.0*cProtonMass* &
              cLightSpeed**2), DIM=2, NCOPIES=nMu+1)
      end do

      ! Boundary condition of Hamiltonian_23 function: ghost cells and symmetry
      Hamiltonian23_N(:,     :,   -1) = Hamiltonian23_N(:,     :,  1)
      Hamiltonian23_N(:,     :, nX+1) = Hamiltonian23_N(:,     :, nX)
      Hamiltonian23_N(:,    -1,    :) = Hamiltonian23_N(:,     1,  :)
      Hamiltonian23_N(:, nMu+1,    :) = Hamiltonian23_N(:, nMu-1,  :)

    end subroutine calc_hamiltonians
    !==========================================================================
  end subroutine iterate_poisson_focused
  !============================================================================
end module SP_ModAdvancePoisson
!==============================================================================
