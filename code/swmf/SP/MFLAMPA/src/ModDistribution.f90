!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModDistribution

  ! The module contains the velocity/momentum distribution function
  ! and methods for initializing it as well as the offset routine.

#ifdef OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use ModUtilities, ONLY: CON_stop
  use SP_ModSize, ONLY: nVertexMax
  use SP_ModGrid, ONLY: iLineAll0, nLine, nVertex_B, Used_B, nP, nMu, IsMuAvg

  implicit none

  SAVE

  private ! except

  ! Public members:
  public:: init                  ! Initialize Distribution_CB
  public:: read_param            ! Read momentum grid parameters
  public:: offset                ! Sync. index in State_VIB and Distribution_CB
  public:: check_dist_neg        ! Check any of Distribution_CB is negative
  public:: search_kinetic_energy ! Search the given kinetic energy index

  ! Injection and maximal energy, in kev (or, in Io energy unit)
  ! To be read from the PARAM.in file
  real, public :: EnergyInjIo = 10.0, EnergyMaxIo = 1.0E+07

  ! Injection momentum, mimimum momentum value in Si
  real, public :: MomentumInjSi

  ! Size of the log-momentum mesh. For momentum we use both the
  ! denotaion, P, and a word, momentum - whichever is more convenient
  real, public :: dLogP      ! log(MomentumMaxSi/MomentumInjSi)/nP

  ! uniform distribution of pitch angle
  real, public :: DeltaMu = 2.0/nMu
  real, public :: Mu_F(0:nMu), Mu_C(nMu), DeltaMu3_I(nMu)

  ! P**3/3 and total energy in the IO unit, at face center
  real, public, dimension(-1:nP+1) :: Momentum3_F, EnergyIo_F, GammaLorentz_F
  ! Speed, momentum, kinetic energy at the momentum grid points
  real, public, dimension( 0:nP+1) :: SpeedSi_G, Momentum_G, &
          KinEnergyIo_G, GammaLorentz_G
  ! Volumes of each cell along the momentum axis
  real, public, dimension( 0:nP+1) :: VolumeP_I, VolumeE_I, VolumeE3_I
  real, public, dimension( 0:nP+1) :: Background_I

  !-----------------Grid in the momentum space---------------------------------
  ! iP     0     1                         nP   nP+1
  !        |     |    ....                 |     |
  ! P     P_inj P_inj*exp(dLogP)          P_Max P_Max*exp(dLogP)
  !-----------------Control volumes and faces----------------------------------
  ! Vol  | 0  |  1  |.........          |  nP | nP+1 |
  ! Fcs -1    0     1 ....            nP-1    nP    nP+1
  !----------------------------------------------------------------------------
  ! This is because we put two boundary conditions: the background
  ! value at the right one and the physical condition at the left
  ! one, for the velocity distribution function

  ! Velosity Distribution Function (VDF)
  ! Number of points along the momentum axis is set in ModSize
  ! 1st index - log(momentum)
  ! 2rd index - \mu value = cosine of the pitch angle
  ! 3rd index - particle index along the field line
  ! 4th index - local field line number
  real, public, allocatable :: Distribution_CB(:,:,:,:)

  ! Check if any Distribution_CB < 0 (.true. for such case)
  logical, public :: IsDistNeg = .false.

  ! distribution is initialized to have integral flux:
  real, public :: FluxInitIo = 0.01 ! [PFU]

  ! Logical variable: whether this is the initial call
  logical :: DoInit = .true.
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in) :: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#ENERGYRANGE')
       ! Read energy range for particles, in the unit of NameEnergyUnit
       call read_var('EnergyMin', EnergyInjIo)
       call read_var('EnergyMax', EnergyMaxIo)
       ! check correctness
       if(EnergyInjIo <= 0.0 .or. EnergyMaxIo <= 0.0) call CON_stop( &
            NameSub // ': EnergyInjIo and EnergyInjIo must be positive')
    case('#FLUXINITIAL')
       call read_var('FluxInitIo', FluxInitIo)
       ! check correctness
       if(FluxInitIo <= 0.0) call CON_stop(NameSub // &
            ': flux value must be positive')
    case default
       call CON_stop(NameSub // ' Unknown command ' // NameCommand)
    end select

  end subroutine read_param
  !============================================================================
  subroutine init

    use ModConst, ONLY: cLightSpeed, cRmeProton
    use ModUtilities, ONLY: check_allocate
    use SP_ModProc, ONLY: iError
    use SP_ModUnit, ONLY: kinetic_energy_to_momentum, momentum_to_energy, &
         momentum_to_kinetic_energy, Io2Si_V, Si2Io_V, UnitEnergy_, UnitFlux_
    ! loop variables
    integer :: iLine, iVertex, iP, iMu
    ! maximal momentum
    real :: MomentumMaxSi
    ! local FluxChannel, for converting to NameFluxChannel_I
    real :: FluxChannel
    ! local NameFluxChannel and NameUnitChannel, written in headers
    character(len=3) :: NameFluxChannel, NameUnitChannel
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    if(.not.DoInit) RETURN
    DoInit = .false.

    ! convert energies to momenta
    MomentumInjSi= kinetic_energy_to_momentum(EnergyInjIo*Io2Si_V(UnitEnergy_))
    MomentumMaxSi= kinetic_energy_to_momentum(EnergyMaxIo*Io2Si_V(UnitEnergy_))
    ! grid size in the log momentum space
    dLogP = log(MomentumMaxSi/MomentumInjSi)/real(nP)

    ! Functions to convert the grid index to momentum and energy
    Momentum3_F(-1) = exp(-3*(0.5*dLogP))/3.0 ! P^3/3 at -0.5*dLogP from PInj
    EnergyIo_F(-1) = momentum_to_energy(MomentumInjSi*exp(-0.5*dLogP)) ! SI now
    do iP = 0, nP+1
       Momentum_G(iP)    = MomentumInjSi*exp(iP*dLogP)
       ! For velocity = p*c**2/sqrt(p**2+(m*c**2)**2), at cell center
       SpeedSi_G(iP)     = Momentum_G(iP)*cLightSpeed**2/ &
            momentum_to_energy(Momentum_G(iP))
       ! For P**3/3 at faces and its volume
       Momentum3_F(iP)   = Momentum3_F(iP-1)*exp(3*dLogP)
       VolumeP_I(iP)     = Momentum3_F(iP) - Momentum3_F(iP-1)
       ! Normalize kinetic energy per Unit of energy in SI unit:
       KinEnergyIo_G(iP) = momentum_to_kinetic_energy(Momentum_G(iP)) &
            *Si2Io_V(UnitEnergy_)
       ! For total energy in Si unit temporarily and in IO unit later
       EnergyIo_F(iP)    = momentum_to_energy(Momentum_G(iP)*exp(0.5*dLogP))
       ! Normalize momentum per MomentumInjSi
       Momentum_G(iP)    = Momentum_G(iP)/MomentumInjSi
       Background_I(iP)  = FluxInitIo*Io2Si_V(UnitFlux_)/ & ! Integral flux SI
            (EnergyMaxIo-EnergyInjIo) &  ! Energy range
            /Momentum_G(iP)**2           ! Convert from diff flux to VDF
    end do
    GammaLorentz_G = KinEnergyIo_G*Io2Si_V(UnitEnergy_)/cRmeProton + 1.0
    GammaLorentz_F = EnergyIo_F/cRmeProton
    EnergyIo_F = EnergyIo_F*Si2Io_V(UnitEnergy_) ! SI to IO
    VolumeE_I  = EnergyIo_F(0:nP+1)    - EnergyIo_F(-1:nP)
    VolumeE3_I = EnergyIo_F(0:nP+1)**3 - EnergyIo_F(-1:nP)**3

    ! Calculate all the mu values at cell center and faces
    do iMu = 0, nMu
       Mu_F(iMu) = -1.0 + real(iMu)*DeltaMu
    end do
    Mu_C = 0.5*(Mu_F(0:nMu-1) + Mu_F(1:nMu))
    DeltaMu3_I = Mu_F(1:nMu)**3 - Mu_F(0:nMu-1)**3

    ! Distribution function
    allocate(Distribution_CB(0:nP+1, nMu, nVertexMax, nLine), stat=iError)
    call check_allocate(iError, 'Distribution_CB')

    ! set the initial distribution on all lines
    ! initialization depends on momentum, however, this corresponds
    ! to a constant differential flux (intensity), thus ensuring
    ! uniform backgound while visualizing this quantity
    Distribution_CB = reshape(spread(Background_I, DIM=2, &
         NCOPIES=nMu*nVertexMax*nLine), [nP+2, nMu, nVertexMax, nLine])
    ! (0:nP+1, nMu*nVertexMax*nLine) -> (0:nP+1, 1:nMu, 1:nVertexMax, 1:nLine)
    ! Overall density of the fast particles is of the order
    ! of 10^-6 m^-3. Integral flux is less than 100 per
    ! (m^2 ster s). Differential background flux is constant.
  end subroutine init
  !============================================================================
  subroutine offset(iLine, iOffset)

    use SP_ModGrid, ONLY: NoShock_, BOld_, RhoOld_, uOld_,   &
         ShockOld_, iShock_IB, State_VIB, MHData_VIB, &
         X_, Z_, FootPoint_VB, check_line_ishock
    ! shift in the data arrays is required if the grid point(s) is appended
    ! or removed at the foot point of the magnetic field line. SHIFTED ARE:
    ! State_VIB(/RhoOld_,BOld_), Distribution_CB, as well as ShockOld_
    integer, intent(in)        :: iLine
    integer, intent(in)        :: iOffset
    real :: Alpha, Distance2ToMin, Distance3To2
    character(len=*), parameter:: NameSub = 'offset'
    !--------------------------------------------------------------------------

    if(iOffset==0)RETURN
    if(iOffset==1)then
       State_VIB([RhoOld_,BOld_, uOld_],2:nVertex_B(iLine),iLine) &
            = State_VIB([RhoOld_,BOld_, uOld_],1:nVertex_B(iLine)-1,iLine)
       Distribution_CB(:,:,2:nVertex_B(iLine), iLine)&
            = Distribution_CB(:,:,1:nVertex_B(iLine)-1, iLine)
       ! Extrapolate state vector components and VDF at iVertex=1
       Distance2ToMin = norm2(MHData_VIB(X_:Z_, 2, iLine) - &
            FootPoint_VB(X_:Z_, iLine))
       Distance3To2   = norm2(MHData_VIB(X_:Z_, 3, iLine) - &
            MHData_VIB(X_:Z_, 2, iLine))
       Alpha = Distance2ToMin/(Distance2ToMin + Distance3To2)
       State_VIB([RhoOld_, BOld_, uOld_], 1, iLine) = &
            (Alpha + 1)*State_VIB([RhoOld_, BOld_, uOld_], 2, iLine) &
            -Alpha     *State_VIB([RhoOld_, BOld_, uOld_], 3, iLine)
       Distribution_CB(:,:,1,iLine) = Distribution_CB(:,:,2,iLine) + Alpha* &
            (Distribution_CB(:,:,2,iLine) - Distribution_CB(:,:,3,iLine))
       ! extrapolation may introduced negative values
       ! for strictly positive quantities; such occurences need fixing
       where(State_VIB([RhoOld_,BOld_],1,iLine) <= 0.0)
          State_VIB([RhoOld_,BOld_],1,iLine) = &
               0.01*State_VIB([RhoOld_,BOld_],2,iLine)
       end where
       where(Distribution_CB(:,:,1,iLine) <= 0.0)
          Distribution_CB(:,:,1,iLine) = 0.01*Distribution_CB(:,:,2,iLine)
       end where
    elseif(iOffset < 0)then
       State_VIB([RhoOld_,BOld_, uOld_],1:nVertex_B(iLine),iLine) &
            = State_VIB([RhoOld_,BOld_, uOld_],1-iOffset:nVertex_B(iLine)&
            - iOffset, iLine)
       Distribution_CB(:,:,1:nVertex_B(iLine), iLine) &
            = Distribution_CB(:,:,1-iOffset:nVertex_B(iLine)-iOffset, iLine)
    else
       call CON_stop('No algorithm for iOffset > 1 in '//NameSub)
    end if

    ! if trace shock: iShock should fall betweem 1 and nVertex_B(iLine)
    if(iShock_IB(ShockOld_, iLine)/=NoShock_) &
         iShock_IB(ShockOld_, iLine) = min(nVertex_B(iLine), &
         max(iShock_IB(ShockOld_, iLine) + iOffset, 1))
  end subroutine offset
  !============================================================================
  subroutine check_dist_neg(NameSub, lVertex, rVertex, iLine)

    ! check if any of Distribution_CB(:, :, lVertex:rVertex, iLine) < 0.0
    ! if so, IsDistNeg = .true.; otherwise, IsDistNeg = .false. (normal case)
    use SP_ModGrid, ONLY: iBlock_to_lon_lat
    character(len=*), intent(in):: NameSub  ! String for the module/subroutine
    integer, intent(in) :: lVertex, rVertex ! Start and end indices of iVertex
    integer, intent(in) :: iLine            ! index of line
    integer :: iLon, iLat
    !--------------------------------------------------------------------------
    if(any(Distribution_CB(:, :, lVertex:rVertex, iLine)<0.0)) then
       ! Diagnose the problem: line number and iLon/iLat of the line
       call iBlock_to_lon_lat(iLine, iLon, iLat)
       write(*,*) NameSub//': Distribution_CB < 0 on Line', &
               iLine+iLineAll0, 'with iLon, iLat = ', iLon, iLat
       Used_B(iLine) = .false.
       nVertex_B(iLine) = 0
       IsDistNeg = .true.
    end if
  end subroutine check_dist_neg
  !============================================================================
  subroutine search_kinetic_energy(KinEnergyIo, iKinEnergyOut, &
       IsFound, DistTimesP2_I, dFlux)

    ! performs search in the KinEnergy_I array, for the given kinetic energy
    real,         intent(in) :: KinEnergyIo   ! input kinetic energy, in Io
    integer,      intent(out):: iKinEnergyOut ! result: index, from 1 to nP-1
    logical,      intent(out):: IsFound       ! whether search succeeds
    real,optional,intent(in) :: DistTimesP2_I(nP) ! VDF*Momentum_G**2 for dFlux
    real,optional,intent(out):: dFlux         ! integral flux at output index
    !--------------------------------------------------------------------------

    ! Check whether physical KinEnergyIo_G covers the given kinetic energy
    if(KinEnergyIo < KinEnergyIo_G(1) .or. KinEnergyIo > EnergyMaxIo) then
       IsFound = .false.
       iKinEnergyOut = -1
       RETURN
    end if

    ! KinEnergyIo_G covers the given kinetic energy
    IsFound = .true.
    ! Find index of first kinetic energy in the array above KinEnergyIo
    iKinEnergyOut = maxloc(KinEnergyIo_G(1:nP-1), DIM=1, &
         MASK=KinEnergyIo > KinEnergyIo_G(1:nP-1))

    ! Get integral flux contributed by the bin of iKinEnergyOut, if necessary
    if(present(dFlux)) then
       ! Channel cutoff level is often in the middle of a bin
       ! Here we compute partial flux increments at iKinEnergyOut

       ! The contrubution to integral equals:
       dFlux = &
            (KinEnergyIo_G(iKinEnergyOut+1) - KinEnergyIo) & ! Span
            *0.5*(                           & ! times a half of sum of
            DistTimesP2_I(iKinEnergyOut+1) + & ! the right boundary value +
            ((KinEnergyIo_G(iKinEnergyOut+1) & ! interpolation to EChannel:
            - KinEnergyIo)*DistTimesP2_I(iKinEnergyOut)    & ! from iP
            + (KinEnergyIo - KinEnergyIo_G(iKinEnergyOut)) &
            *DistTimesP2_I(iKinEnergyOut+1)) & ! from iP+1
            / & ! per the total of interpolation weights:
            (KinEnergyIo_G(iKinEnergyOut+1) - KinEnergyIo_G(iKinEnergyOut)))
    end if

  end subroutine search_kinetic_energy
  !============================================================================
end module SP_ModDistribution
!==============================================================================
