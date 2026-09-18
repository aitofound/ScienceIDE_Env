!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModBc

  ! The module sets up boundary conditions
  use ModNumConst, ONLY: cPi
  use ModCosmicRay, ONLY: local_interstellar_spectrum, &
       TypeLisBc, UseModulationPot, ModulationPot
  use SP_ModDistribution, ONLY: Distribution_CB, Momentum_G, &
       MomentumInjSi, Background_I
  use SP_ModGrid, ONLY: nP, nMu, MhData_VIB, NoShock_, T_, X_, Z_
  use SP_ModShock, ONLY: nShockWidth
  use SP_ModUnit, ONLY: kinetic_energy_to_momentum,  &
       UnitEnergy_, UnitX_, Io2Si_V
  use ModUtilities, ONLY: CON_stop
  implicit none

  SAVE

  PRIVATE ! except

  ! Public members:
  public:: read_param
  public:: set_momentum_bc   ! Set the Distribution_CB Bc at min/max energy
  public:: set_upper_end_bc  ! Set the UpperEndBc_I Bc at the far end point
  public:: set_lower_end_bc  ! Set the LowerEndBc_I Bc at the near-Sun end
  public:: set_VDF2, set_VDF3! Set the VDF Bc, typically with 2 ghost cells
  ! Boundary condition at the injection energy
  ! Injection efficiency and assumed spectral index with the energy
  ! range k_B*T_i < Energy < EnergyInjection, to be read from PARAM.in
  real, public      :: CoefInj = 0.25, SpectralIndex = 5.0
  real, parameter   :: CoefInjTiny = 2.5E-11 ! Before Nov 2023: set to be 0.0
  ! Whether to account for shock compression in injection efficiency
  logical           :: UseComprhoScale = .false.
  real, parameter   :: ComprhoMax = 4.0 ! Max compression ratio in theory

  ! Momentum BC
  character(LEN=6)  :: TypeMomentumMinBc = 'inject'
  character(LEN=6)  :: TypeMomentumMaxBc = 'escape'
  ! Type of lower end BC: float, escape, inject
  character(LEN=6), public  :: TypeLowerEndBc = 'float'
  ! Type of upper end BC: float, escape, lism
  character(LEN=6), public  :: TypeUpperEndBc = 'escape'
  real, public      :: UpperEndBc_I(1:nP), LowerEndBc_I(1:nP)
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    use ModUtilities, ONLY: lower_case

    character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#MOMENTUMBC')
       call read_var('TypeMomentumMinBc', TypeMomentumMinBc)
       if(TypeMomentumMinBc=='inject')then
          call read_var('SpectralIndex', SpectralIndex)
          call read_var('EfficiencyInj', CoefInj)
       end if
       call read_var('TypeMomentumMaxBc', TypeMomentumMaxBc)
    case('#INJECTION')
       call read_var('UseComprhoScale', UseComprhoScale)
    case('#LOWERENDBC')
       ! Read the type of LowerEndBc and Select
       call read_var('TypeLowerEndBc', TypeLowerEndBc)
       call lower_case(TypeLowerEndBc)
    case('#UPPERENDBC')
       ! Read the type of UppenEndBc and Select
       call read_var('TypeUpperEndBc', TypeUpperEndBc)
       call lower_case(TypeUpperEndBc)
       select case(trim(TypeUpperEndBc))
       case('float', 'escape')
          ! Do nothing
       case('lism')
          ! We want to read the type of LIS here
          call read_var('TypeLisBc', TypeLisBc)
          call lower_case(TypeLisBc)
          ! Read whether using ModulationPot to get GCR spectrum at ~1 AU
          call read_var('UseModulationPot', UseModulationPot)
          if(UseModulationPot) call read_var('ModulationPot', ModulationPot)
       case default
          call CON_stop(NameSub//&
               ': Unknown type of upper end BC '//TypeUpperEndBc)
       end select
    case default
       call CON_stop(NameSub//': Unknown command '//NameCommand)
    end select
  end subroutine read_param
  !============================================================================
  subroutine set_momentum_bc(iLine, nX, nSi_I, iShock, dLogRho_I)
    ! Set boundary conditions for Distribution_CB on grid point,
    ! on the given field line.

    integer, intent(in) :: iLine, nX, iShock
    real,    intent(in) :: nSi_I(1:nX)
    real, optional, intent(in) :: dLogRho_I(1:nX)
    ! local variables
    integer :: iVertex     ! loop variable
    real    :: MomentumSi  ! Momentum for the thermal energy k_BTi
    real    :: CoefInjLocal, DistributionBc

    character(len=*), parameter:: NameSub = 'set_momentum_bc'
    !--------------------------------------------------------------------------
    select case(trim(TypeMomentumMinBc))
    case('inject')
       do iVertex = 1, nX
          ! injection(Ti, Rho), see Sokolov et al., 2004, eq (3)
          ! f = CoefInj/2/pi * N / (2*m*T_p)^(3/2) * ((2*m*T_p)^(1/2)/p_inj)^5
          !   = CoefInj/2/pi * N / p^3 * (p/p_inj)^5
          ! where p = sqrt(2*m*T_p) is the momentum of thermal ion
          CoefInjLocal = CoefInjTiny
          MomentumSi   = kinetic_energy_to_momentum( &
               MhData_VIB(T_,iVertex,iLine)*Io2Si_V(UnitEnergy_))

          DistributionBc = (SpectralIndex-3)/(4*cPi)   &
               * MomentumInjSi**2*Io2Si_V(UnitEnergy_) &
               * nSi_I(iVertex)/MomentumSi**3          &
               * (MomentumSi/MomentumInjSi)**SpectralIndex

          if(UseComprhoScale .and. present(dLogRho_I)) then
             ! If dLogRho_I is present, it means we are in the time-accurate
             ! advection step, and we need to account for the effect of shock
             ! compression on the injection efficiency. We assume that the
             ! injection efficiency is scaled with the density compression
             ! ratio, which is exp(dLogRho_I). The maximum compression ratio
             ! in theory is 4 for strong shock, so there is a scaling.
             if(iShock /= NoShock_ .and. iVertex <= iShock+nShockWidth .and. &
                  iVertex >= iShock-nShockWidth) CoefInjLocal = CoefInj* &
                  exp(exp(dLogRho_I(iVertex))-ComprhoMax)
          else
             if(iShock /= NoShock_ .and. iVertex <= iShock+nShockWidth .and. &
                  iVertex >= iShock-nShockWidth) CoefInjLocal = CoefInj
          end if

          Distribution_CB(0, :, iVertex, iLine) = DistributionBc*CoefInjLocal
       end do
    case('float', 'escape')
       ! Neumann BC: Assume gradient = 0
       Distribution_CB(0,:,1:nX,iLine) = Distribution_CB(1,:,1:nX,iLine)
    case('grad')
       ! Neumann BC: Use the same spectral power P0 <-> P1 <-> P2
       ! Since P_{iP-1}, P_{iP} and P_{iP+1} is a geometric sequence, it gives
       ! P_{iP+1} = P_{iP}**2 / P_{iP-1} and f_{iP+1} = f_{iP}**2 / f_{iP-1}.
       Distribution_CB(0,:,1:nX,iLine) = &
            Distribution_CB(1,:,1:nX,iLine)**2 &
            /Distribution_CB(2,:,1:nX,iLine)
    case default
       call CON_stop(NameSub// &
            ': Unknown type of BC at minimal momentum '//TypeMomentumMinBc)
    end select

    select case(trim(TypeMomentumMaxBc))
    case('escape')
       Distribution_CB(nP+1,:,1:nX,iLine) = Background_I(nP+1)
    case('float')
       ! Neumann BC: Assume gradient = 0
       Distribution_CB(nP+1,:,1:nX,iLine) = Distribution_CB(nP,:,1:nX,iLine)
    case('grad')
       ! Neumann BC: Use the same spectral power nP-1 <-> nP <-> nP+1
       ! Since P_{iP-1}, P_{iP} and P_{iP+1} is a geometric sequence, it gives
       ! P_{iP+1} = P_{iP}**2 / P_{iP-1} and f_{iP+1} = f_{iP}**2 / f_{iP-1}.
       Distribution_CB(nP+1,:,1:nX,iLine) = &
            Distribution_CB(nP,:,1:nX,iLine)**2 &
            /Distribution_CB(nP-1,:,1:nX,iLine)
    case default
       call CON_stop(NameSub// &
            ': Unknown type of momentum max BC '//TypeMomentumMaxBc)
    end select

  end subroutine set_momentum_bc
  !============================================================================
  subroutine set_upper_end_bc(iLine, nX)
    ! Set boundary condition at the last grid point on the given field line.
    ! Assign the calculated BC to UpperEndBc_I.

    integer, intent(in) :: iLine, nX
    real :: XyzSi_D(3)                          ! Where to set BC
    character(len=*), parameter:: NameSub = 'set_upper_end_bc'
    !--------------------------------------------------------------------------
    select case(trim(TypeUpperEndBc))
    case('float')
       UpperEndBc_I = Distribution_CB(1:nP, nMu, nX, iLine)
    case('escape')
       UpperEndBc_I = Background_I(1:nP)
    case('lism')
       XyzSi_D = MhData_VIB(X_:Z_, nX, iLine)*Io2Si_V(UnitX_)
       call local_interstellar_spectrum(     &
            nP = nP,                         &  ! # of grid points
            MomentumSi_I = Momentum_G(1:nP)* &
            MomentumInjSi,                   &  ! momentum (SI) in grid points
            XyzSi_D = XyzSi_D,               &  ! Coords
            DistTimesP2Si_I = UpperEndBc_I)
       ! Now, in UpperEndBc_I there is Distribution[Si]*Momentum[Si]**2
       ! Our Momentum_G is MomentumSi_I/MomentumInjSi
       ! So, UpperEndBc_I is Distribution[Si]*MomentumInjSi**2*Momentum_G**2
       ! The distribution used in our code is
       ! Distribution[Si]*MomentumInjSi**2*Io2Si_V(UnitEnergy_)
       UpperEndBc_I = (UpperEndBc_I/Momentum_G(1:nP)**2)*Io2Si_V(UnitEnergy_)
    case default
       call CON_stop(NameSub// &
            ': Unknown type of upper end BC '//TypeUpperEndBc)
    end select
  end subroutine set_upper_end_bc
  !============================================================================
  subroutine set_lower_end_bc(iLine)
    ! Set boundary condition at the zeroth grid point on the given field line.
    ! Assign the calculated BC to LowerEndBc_I.

    integer, intent(in) :: iLine
    character(len=*), parameter:: NameSub = 'set_lower_end_bc'
    !--------------------------------------------------------------------------
    select case(trim(TypeLowerEndBc))
    case('inject')
       LowerEndBc_I = Distribution_CB(0, 1, 1, iLine) &
            /Momentum_G(1:nP)**SpectralIndex
    case('float')
       LowerEndBc_I = Distribution_CB(1:nP, nMu, 1, iLine)
    case('escape')
       LowerEndBc_I = Background_I(1:nP)
    case default
       call CON_stop(NameSub//&
            ': Unknown type of lower end BC '//TypeLowerEndBc)
    end select
  end subroutine set_lower_end_bc
  !============================================================================
  subroutine set_VDF2(iLine, nX, VDF_G)
    ! We need the VDF on the extended grid with two layers of ghost cells, to
    ! solve the second-order accurate scheme. Add solution in physical cells
    ! and in a single layer of the ghost cells along the momentum coordinate,
    ! plus two layers of ghost cells along the pitch-angle coordinates.
    ! This function is for the 2-dimensional VDF, with momentum (P^3/3)
    ! and line coordinates (s_L).

    integer, intent(in) :: iLine     ! Indices of line and shock
    integer, intent(in) :: nX        ! Number of meshes along s_L axis
    real, intent(inout) :: VDF_G(-1:nP+2, -1:nX+2)
    real :: VDF3_G(-1:nP+2, -1:nMu+2, -1:nX+2) ! nMu = 1 for VDF2
    !--------------------------------------------------------------------------

    call set_VDF3(iLine, nX, VDF3_G)
    VDF_G = VDF3_G(:, nMu, :) ! nMu = 1 for VDF2

  end subroutine set_VDF2
  !============================================================================
  subroutine set_VDF3(iLine, nX, VDF_G)
    ! We need the VDF on the extended grid with two layers of ghost cells, to
    ! solve the second-order accurate scheme. Add solution in physical cells
    ! and in a single layer of the ghost cells along the momentum coordinate,
    ! plus two layers of ghost cells along the pitch-angle coordinates.
    ! This function is for the 3-dimensional VDF, with momentum (P^3/3),
    ! pitch angle (mu), and line coordinates (s_L).

    integer, intent(in) :: iLine     ! Indices of line and shock
    integer, intent(in) :: nX        ! Number of meshes along s_L axis
    real, intent(inout) :: VDF_G(-1:nP+2, -1:nMu+2, -1:nX+2)
    !--------------------------------------------------------------------------
    ! Copy distribution with one GC along the momentum coordinate
    VDF_G(0:nP+1, 1:nMu, 1:nX) = Distribution_CB(:, :, 1:nX, iLine)

    ! Calculate the LowerEndBc_I(1:nP):
    call set_lower_end_bc(iLine)
    ! Apply LowerEndBc_I(1:nP) in 0th GC along line coordinate
    VDF_G(1:nP, 1:nMu, 0) = spread(max(LowerEndBc_I, &
         Background_I(1:nP)), DIM=2, NCOPIES=nMu)
    ! Fill in GCs along momentum in 0th GC along line coordinate
    VDF_G(0   , 1:nMu, 0) = max(VDF_G(0, 1:nMu, 1), Background_I(0))
    VDF_G(nP+1, 1:nMu, 0) = max(VDF_G(nP+1,1:nMu,1), Background_I(nP+1))

    ! Calculate the UpperEndBc_I(1:nP):
    call set_upper_end_bc(iLine, nX)
    ! Apply UpperEndBc_I(1:nP) in (nX+1)th GC along line coordinate
    VDF_G(1:nP, 1:nMu, nX+1) = spread(max(UpperEndBc_I, &
         Background_I(1:nP)), DIM=2, NCOPIES=nMu)
    ! Fill in GCs along momentum in (nX+1)th GC along line coordinate
    VDF_G(0   , 1:nMu, nX+1) = max(VDF_G(0, 1:nMu, nX), Background_I(0))
    VDF_G(nP+1, 1:nMu, nX+1) = max(VDF_G(nP+1,1:nMu,nX), Background_I(nP+1))

    ! Add a second layer of the ghost cells along the line coordinate:
    VDF_G(0:nP+1, 1:nMu,   -1) = VDF_G(0:nP+1, 1:nMu,    0)
    VDF_G(0:nP+1, 1:nMu, nX+2) = VDF_G(0:nP+1, 1:nMu, nX+1)
    if(nMu > 1)then
       ! Add two layer of the ghost cells along the line coordinate:
       VDF_G(0:nP+1,     0, :) = VDF_G(0:nP+1,    1,  :)
       VDF_G(0:nP+1,    -1, :) = VDF_G(0:nP+1,    0,  :)
       VDF_G(0:nP+1, nMu+1, :) = VDF_G(0:nP+1,   nMu, :)
       VDF_G(0:nP+1, nMu+2, :) = VDF_G(0:nP+1, nMu+1, :)
    end if
    ! Add a second layer of the ghost cells along the momentum coordinate:
    VDF_G(  -1, :, :) = VDF_G(   0, :, :)
    VDF_G(nP+2, :, :) = VDF_G(nP+1, :, :)

  end subroutine set_VDF3
  !============================================================================
end module SP_ModBc
!==============================================================================
