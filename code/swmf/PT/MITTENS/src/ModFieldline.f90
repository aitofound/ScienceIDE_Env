!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModFieldline

    use PT_ModGrid
    use PT_ModTime, ONLY: PTTime, DataInputTime
    use PT_ModConst, ONLY: cPi, ckeV, cAU, cRsun, cProtonMass, &
                           cElectronCharge, cLightSpeed, cProtonRestEnergy

    use PT_ModSize, ONLY: nVertexMax
    use PT_ModShock, ONLY: dLogRho_II, dLogRhoOld_II, dLogRhoThreshold
    use ModUtilities, ONLY: CON_stop

    implicit none
    save

    integer :: iLine = 1

    character(len=100) :: BoundaryInner = ""
    character(len=100) :: BoundaryOuter = ""
    character(len=100) :: UpstreamDxxModel = ""

    real, allocatable ::  MhdState1(:, :), MhdState2(:,:)

    integer, parameter :: nStateVar    = 8, &
                          dLogRho_     = 1, &
                          BState_      = 2, &
                          dBState_     = 3, &
                          RhoState_    = 4, &
                          UState_      = 5, &
                          dSState_     = 6, &
                          TState_      = 7, &
                          RState_      = 8

    integer :: iShock1, iShock2
    real    :: iShockNow
    integer :: iShock1Up, iShock2Up, iShock1Down, iShock2Down
    integer :: WidthUpNow, WidthDownNow

    logical :: UseConstantDiffusion = .false.
    real    :: DxxConst = 10.0
    real    :: CorrelationLength = 0.03
    real    :: Lambda0 = 0.3
    real    :: ScalingFactor = 1.0

    logical :: UseLogScaling  = .true.

    ! Distance-aware adaptive timestep (#DISTANCETIMESTEP).
    ! Far from the shock the Euler-Maruyama step is exact, so allow steps
    ! whose diffusive displacement is at most sqrt(AlphaDist) of the distance
    ! to the shock center or the line ends (1-cell floor at the center);
    ! everywhere limit the per-step compression to AlphaDist e-folds.
    ! MaxTimeStep (#SDE) remains the global hard cap: raise it when enabling.
    ! One user knob feeds two internally separate factors so they can be
    ! split later without redesign.
    logical :: UseDistanceTimestep = .false.
    real :: AlphaDist = 0.1
    real :: AlphaRamp = 0.1  ! = AlphaDist: distance ramp
    real :: EpsKick   = 0.1  ! = AlphaDist: compression-rate guard

contains
  !============================================================================
    subroutine read_param_fieldline(NameCommand)

        use ModReadParam, ONLY: read_var
        use ModUtilities, ONLY: CON_stop

        character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param_fieldline'
    !--------------------------------------------------------------------------
        select case(NameCommand)
        case('#DIFFUSION')
            call read_var('UpstreamDxxModel', UpstreamDxxModel)
            if(UpstreamDxxModel == "Analytical") &
                call read_var('Lambda0', Lambda0)
            call read_var('CorrelationLength', CorrelationLength)
            call read_var('ScalingFactor', ScalingFactor)
            call read_var('UseConstantDiffusion', UseConstantDiffusion)
            if(UseConstantDiffusion) &
                call read_var('DxxConst', DxxConst)
        case('#BOUNDARY')
            call read_var('BoundaryInner', BoundaryInner)
            call read_var('BoundaryOuter', BoundaryOuter)
        case("#ADVECTION")
            call read_var("UseLogScaling", UseLogScaling)

        case('#DISTANCETIMESTEP')
            call read_var('UseDistanceTimestep', UseDistanceTimestep)
            if(UseDistanceTimestep) then
                call read_var('AlphaDist', AlphaDist)
                AlphaRamp = AlphaDist
                EpsKick   = AlphaDist
            end if

        case default
            call CON_stop(NameSub//' Unknown command '//NameCommand)
        end select

    end subroutine read_param_fieldline
  !============================================================================
    subroutine set_fieldline(iLineIn)
        integer, intent(in) :: iLineIn
        integer :: iVertex, iLagr

        ! set iLine for module
    !--------------------------------------------------------------------------
        iLine = iLineIn

        if(allocated(MhdState1)) deallocate(MhdState1)
        if(allocated(MhdState2)) deallocate(MhdState2)

        allocate(MhdState1(1:nStateVar, 1:nVertexMax))
        allocate(MhdState2(1:nStateVar, 1:nVertexMax))

        MhdState1(RhoState_, :) = State_VIB(RhoOld_, :, iLine)
        MhdState1(BState_, :) = State_VIB(BOld_, :, iLine)
        MhdState1(dBState_, :) = State_VIB(dBOld_, :, iLine)
        MhdState1(dSState_, :) = State_VIB(DOld_, :, iLine)
        MhdState1(UState_, :) = State_VIB(UOld_, :, iLine)
        MhdState1(RState_, :) = State_VIB(ROld_, :, iLine)
        MhdState1(TState_, :) = State_VIB(TOld_, :, iLine)
        MhdState1(dLogRho_, :) = dLogRhoOld_II(:, iLine)

        MhdState2(RhoState_, :) = MhData_VIB(Rho_, :, iLine)
        MhdState2(BState_, :) = State_VIB(B_, :, iLine)
        MhdState2(dBState_, :) = State_VIB(dB_, :, iLine)
        MhdState2(dSState_, :) = State_VIB(D_, :, iLine)
        MhdState2(UState_, :) = State_VIB(U_, :, iLine)
        MhdState2(RState_, :) = State_VIB(R_, :, iLine)
        MhdState2(TState_, :) = MhData_VIB(T_, :, iLine)
        MhdState2(dLogRho_, :) = dLogRho_II(:, iLine)

        iShock1 = iShock_IB(ShockOld_, iLine)
        iShock1Up = iShock_IB(ShockUpOld_, iLine)
        iShock1Down = iShock_IB(ShockDownOld_, iLine)

        iShock2 = iShock_IB(Shock_, iLine)
        iShock2Up = iShock_IB(ShockUp_, iLine)
        iShock2Down = iShock_IB(ShockDown_, iLine)

    end subroutine
  !============================================================================
    subroutine set_iShockNow(Time, TimeFrac)
        real, intent(in) :: Time
        real, optional, intent(out) :: TimeFrac

        ! Interpolate shock location - not an integer!
    !--------------------------------------------------------------------------
        TimeFrac = (Time - PTTime) / (DataInputTime - PTTime)
        iShockNow = (1-TimeFrac) * iShock1 + TimeFrac * iShock2

        ! Interpolate shock widths
        WidthUpNow = int((1-TimeFrac) * (iShock1Up - iShock1) + TimeFrac * (iShock2Up - iShock2))
        WidthDownNow = int((1-TimeFrac) * (iShock1 - iShock1Down) + TimeFrac * (iShock2 - iShock2Down))

        ! check bounds
        WidthDownNow = min(WidthDownNow, &
                           int(iShockNow) - MinLagr(iLine), &
                           iShock1 - MinLagr(iLine))
        WidthUpNow   = min(WidthUpNow, &
                           MaxLagr(iLine) - int(iShockNow), &
                           MaxLagr(iLine) - iShock2)

    end subroutine set_iShockNow
  !============================================================================
    subroutine interpolate_statevar(Time, LagrCoord, Var, InterpValue)

        real, intent(in) :: Time, LagrCoord
        integer, intent(in) :: Var
        real, intent(out) :: InterpValue

        real :: LagrFrac, f1, f2, dLagr, TimeFrac
        integer :: LagrInt
        integer :: iS1, iS2

       ! split LagrCoord into integer and fractional part
    !--------------------------------------------------------------------------
        LagrInt = floor(LagrCoord)
        LagrFrac = LagrCoord - LagrInt

        if(LagrInt < MinLagr(iLine).or.LagrInt+1 > MaxLagr(iLine)) then
            InterpValue = -1.0
            RETURN
        end if

        ! Set current shock location - returns interpolation fraction
        call set_iShockNow(Time, TimeFrac)

        ! this shouldn't happen
        if(TimeFrac > 1.0.or.TimeFrac < 0.0) then
            write(*,*) 'Timefrac: ', TimeFrac, Var, PTTime,  Time, DataInputTime
            call CON_stop("Timefrac in interpolate_statevar out of range")
        end if

        ! distance from shock
        dLagr = LagrCoord - iShockNow

        ! Take log of B, dB, and rho before interpolation - helps at inner boundary
        if(UseLogScaling.and.Var >= BState_.and.Var <= RhoState_) then
            where(MhdState1(Var, :) /= 0) &
                MhdState1(Var, :) = log10(MhdState1(Var, :))
            where(MhdState2(Var, :) /= 0) &
                MhdState2(Var, :) = log10(MhdState2(Var, :))
        end if

        ! Pseudo-particle up or downstream of both shocks
        ! Or the shock did not move (i.e., no shock)
        ! Or interpolating heliocentric distance
        if(LagrCoord < (iShock1-WidthDownNow).or.LagrCoord > (iShock2+WidthUpNow).or. &
          (iShock1 == iShock2).or.(Var == RState_)) then
            ! spatial interpolation at MHD state 1
            f1 = (1-LagrFrac) * MhdState1(Var, LagrInt) + &
                 LagrFrac * MhdState1(Var, LagrInt+1)
            ! spatial interplation at MHD state 2
            f2 = (1-LagrFrac) * MhdState2(Var, LagrInt) + &
                 LagrFrac * MhdState2(Var, LagrInt+1)
            ! interpolation in time
            InterpValue = (1 - TimeFrac) * f1 + TimeFrac * f2

        ! Pseudo-particle inside current shock region - Shock advection
        else if(LagrCoord >= (iShockNow-WidthDownNow).and.(LagrCoord) <= (iShockNow+WidthUpNow)) then
            ! indices of interpolation
            ! iS1 and iS2 are relative integer location of pseudo-particle wrt
            ! iShock1 and iShock2
            ! dLagr becomes the fractional part of iS1 and iS2

            iS1 = int(iShock1 + dLagr)
            iS2 = int(iShock2 + dLagr)
            dLagr = LagrCoord - int(LagrCoord)

            ! spatial interpolation at MHD state 1
            f1 = (1 - dLagr) * MhdState1(Var, iS1) + &
                 dLagr * MhdState1(Var, iS1 + 1)
            ! spatial interplation at MHD state 2
            f2 = (1 - dLagr) * MhdState2(Var, iS2) + &
                 dLagr * MhdState2(Var, iS2+1)

            InterpValue = (1 - TimeFrac) * f1 + TimeFrac * f2

        ! Pseudo-particle downstream of current shock region but
        ! upstream of previous
        else if(LagrCoord < (iShockNow-WidthDownNow)) then
            InterpValue = (1 - LagrFrac) * MhdState2(Var, LagrInt) + &
                          LagrFrac * MhdState2(Var, LagrInt + 1)

        ! Pseudo-particle upstream of current shock region but
        ! downstream of next
        else if(LagrCoord > (iShockNow+WidthUpNow)) then
            InterpValue = (1 - LagrFrac) * MhdState1(Var, LagrInt) + &
                           LagrFrac * MhdState1(Var, LagrInt+1)

        else
            write(*,*) "Error in interpolate_statevar", LagrCoord, iShock1, iShock2, iShockNow, WidthDownNow, WidthUpNow
        end if

        ! Undo log-scaling
        if(UseLogScaling.and.Var >= BState_.and.Var <= RhoState_) then
            where(MhdState1(Var, :) /= 0) &
                MhdState1(Var, :) = 10.0**MhdState1(Var, :)
            where(MhdState2(Var, :) /= 0) &
                MhdState2(Var, :) = 10.0**MhdState2(Var, :)
            InterpValue = 10.0**InterpValue
        end if

    end subroutine interpolate_statevar
  !============================================================================
    subroutine get_dxx(Time, LagrCoord, Momentum, Dxx)
        real, intent(in) :: Time, LagrCoord, Momentum
        real, intent(out) :: Dxx

        real :: R, B, dB, TimeFrac

    !--------------------------------------------------------------------------
        call set_iShockNow(Time, TimeFrac)

        if(UseConstantDiffusion) then
            ! constant Rs**2 is to offset the unit conversion
            ! of dS in the SDE coefficients
            ! only for analytical test!
            Dxx = DxxConst * cRsun**2
            RETURN
        end if

        call interpolate_statevar(Time, LagrCoord, RState_, R)

        ! upstream of shock - selected in PARAM
        if(LagrCoord > (iShockNow + WidthUpNow)) then

            select case(UpstreamDxxModel)
            case("PSP")
                call get_psp_dxx(R, Momentum, Dxx)
            case("MHD")
                call interpolate_statevar(Time, LagrCoord, BState_, B)
                call interpolate_statevar(Time, LagrCoord, dBState_, dB)
                call get_mhd_dxx(R, B, dB, Momentum, Dxx)
            case("Analytical")
                call get_upstream_dxx(R, Momentum, Dxx)
            case default
                call get_psp_dxx(R, Momentum, Dxx)
            end select

        ! downstream of shock - use MHD turbulence
        else
            call interpolate_statevar(Time, LagrCoord, BState_, B)
            call interpolate_statevar(Time, LagrCoord, dBState_, dB)
            call get_mhd_dxx(R, B, dB, Momentum, Dxx)

        end if

    end subroutine get_dxx
  !============================================================================
    subroutine get_mhd_dxx(R, B, dB, Momentum, Dxx)
        real, intent(in) :: R, B, dB, Momentum
        real, intent(out) :: Dxx
        real :: ConstantFactor = 81.0 / (7.0*cPi) * (0.5/cPi)**(2.0/3.0)
        real :: Velocity, Lmax, Btotal, MeanFreePath

        ! relativistic velocity)
    !--------------------------------------------------------------------------
        Velocity = Momentum / sqrt(cProtonMass**2.0 + &
                   (Momentum/cLightSpeed)**2.0)

        ! Calculate mean free path
        Lmax = CorrelationLength*R*cRsun

        Btotal = sqrt(B**2 + dB)
        ! Btotal = B

        MeanFreePath = ConstantFactor * Btotal**2 * &
                        (Momentum*Lmax**2/(Btotal * cElectronCharge))**(1.0/3.0) / dB
        ! Calculate Dxx
        Dxx = MeanFreePath * Velocity / 3.0
        Dxx = max(Dxx, 1.0d4 * cRsun)
        Dxx = Dxx * ScalingFactor

    end subroutine get_mhd_dxx
  !============================================================================
    subroutine get_psp_dxx(R, Momentum, Dxx)
        real, intent(in) :: R, Momentum
        real, intent(out) :: Dxx

        real :: Dxx0, Energy
        ! Chen et al., 2024
    !--------------------------------------------------------------------------
        Dxx0 = 5.16d14 ! [m^2/s]
        Energy = sqrt((Momentum*cLightSpeed)**2 + cProtonRestEnergy**2) - cProtonRestEnergy

        Dxx = Dxx0 * (R * cRsun / cAU )**(1.17) * (Energy / ckeV)**(0.71)

    end subroutine get_psp_dxx
  !============================================================================
    subroutine get_upstream_dxx(R, Momentum, Dxx)
        real, intent(in) :: R, Momentum
        real, intent(out) :: Dxx

        real :: Energy, fac1, fac2, GeV

    !--------------------------------------------------------------------------
        Energy = sqrt((Momentum*cLightSpeed)**2 + cProtonRestEnergy**2) - cProtonRestEnergy
        GeV = 1e6 * ckeV

        fac1 = Energy * (Energy + 2.0 * cProtonRestEnergy) / GeV**2.0
        fac2 = Energy * (Energy + 2.0 * cProtonRestEnergy) / (Energy + cProtonRestEnergy)**2.0

        Dxx = Lambda0 * cLightSpeed * R * cRsun * fac1 ** (1.0/6.0) * sqrt(fac2) / (3.0)

    end subroutine get_upstream_dxx
  !============================================================================
    subroutine get_sde_coeffs_euler(X_I, Time, TimeStep, DriftCoeff, DiffCoeff)
        use PT_ModSize, ONLY: nDim
        use PT_ModProc, ONLY: iProc

        real, intent(in) :: X_I(nDim), Time
        real, intent(out) :: Timestep, DriftCoeff(nDim), DiffCoeff(nDim)

        real :: Momentum, B, Dxx, dS, ShockWidth, TimeFrac
        real :: Bup, DxxUp, dSup
        real :: Bdown, DxxDown, dSdown
        real :: Rho1, RhoOld1, dLogRhodTau
        real :: TimestepDrift, TimestepShock
        real :: DistNearest, VShockLagr
        logical :: IsShockDetected

        integer :: iVertex

    !--------------------------------------------------------------------------
        call set_iShockNow(Time, TimeFrac)

        ! Need to figure out where to put X_I index variables - hardcoded 2 = Momentum_
        Momentum = (3.0*X_I(2))**(1.0/3.0)

        ! Need to figure out where to put X_I index variables - hardcoded 1 = LagrCoord_
        call interpolate_statevar(Time, X_I(1), dLogRho_, dLogRhodTau)

        ! get values at particle current location
        call interpolate_statevar(Time, X_I(1), BState_, B)
        call interpolate_statevar(Time, X_I(1), dSState_, dS)
        call get_dxx(Time, X_I(1), Momentum, Dxx)

        ! get values one lagrangian coordinate upstream of particle location
        call interpolate_statevar(Time, X_I(1) + 1.0, BState_, Bup)
        call interpolate_statevar(Time, X_I(1) + 1.0, dSState_, dSup)
        call get_dxx(Time, X_I(1) + 1.0, Momentum, DxxUp)

        ! get values one lagrangian coordinate upstream of particle location
        ! call interpolate_statevar(Time, X_I(1) - 1.0, BState_, Bdown)
        ! call interpolate_statevar(Time, X_I(1) - 1.0, dSState_, dSdown)
        ! call get_dxx(Time, X_I(1) - 1.0, Momentum, DxxDown)

        ! convert from [Rsun] to [m]
        dS = dS * cRsun
        dSup = dSup * cRsun
        ! dSdown = dSdown * cRsun

        ! lagr coord sde coefficients
        DriftCoeff(1) = B * ((DxxUp / (Bup * dSup)) - (Dxx / (B * dS))) / dS
        DiffCoeff(1) = sqrt(2.0 * Dxx) / dS

        ! momentum sde coefficients
        DriftCoeff(2) = X_I(2) * dLogRhodTau
        DiffCoeff(2) = 0.0

        ! calculate timestep based on coefficients
        ! diffusion >> drift
        ! Maximum spatial step size is less than shockwidth
        ShockWidth = real(WidthUpNow + WidthDownNow)

        ! ShockWidth = 0 means no shock zone is detected on this line
        IsShockDetected = ShockWidth > 0.0

        ! DriftCoeff vanishes for uniform Dxx/(B*dS); the drift-limited
        ! timestep is then unbounded and must not be computed by division
        if(DriftCoeff(1) == 0.0) then
            TimestepDrift = sqrt(huge(Timestep))
        else
            TimestepDrift = DiffCoeff(1)**2.0 / DriftCoeff(1)**2.0
        end if

        if(.not.IsShockDetected) then
            ! no shock on this line: drift criterion only
            Timestep = TimestepDrift

        else if(abs(X_I(1) - real(iShockNow)) <= ShockWidth) then
            ! inside the detected shock zone: the step must also resolve
            ! the zone-crossing scales
            if(DriftCoeff(1) == 0.0) then
                TimestepShock = sqrt(huge(Timestep))
            else
                TimestepShock = ShockWidth / abs(DriftCoeff(1))
            end if
            Timestep = min(TimestepDrift, TimestepShock, &
                           ShockWidth**2.0 / DiffCoeff(1)**2.0)

        else
            ! shock detected, particle outside the zone
            Timestep = TimestepDrift
        end if

        if(UseDistanceTimestep) then
            ! Distance ramp, anchored at the shock CENTER: a step's
            ! diffusive displacement may cover at most sqrt(AlphaRamp) of
            ! the distance to the shock or to the line ends. The detected
            ! zone width is deliberately NOT used as a scale here: for
            ! smooth shocks it can exceed the true kick structure by an
            ! order of magnitude. The 1-cell floor at the center gives the
            ! empirically validated fine-step scale on the kick itself.
            if(IsShockDetected) then
                DistNearest = abs(X_I(1) - real(iShockNow))
            else
                DistNearest = sqrt(huge(Timestep))
            end if
            DistNearest = max(min(DistNearest, &
                                  X_I(1) - real(MinLagr(iLine)), &
                                  real(MaxLagr(iLine)) - X_I(1)), 1.0)
            Timestep = min(Timestep, &
                           AlphaRamp*DistNearest**2/DiffCoeff(1)**2)

            ! Shock motion: no step may outlast half the time for the
            ! front to close the remaining gap
            if(IsShockDetected .and. iShock2 > iShock1) then
                VShockLagr = real(iShock2 - iShock1) &
                             / max(DataInputTime - PTTime, 1e-30)
                Timestep = min(Timestep, DistNearest/(2.0*VShockLagr))
            end if

            ! Compression-rate guard: at most EpsKick e-folds of momentum
            ! change per step, so the linearized kick tracks the
            ! exponential
            if(abs(dLogRhodTau) > 0.0) &
                Timestep = min(Timestep, EpsKick/abs(dLogRhodTau))
        end if

    end subroutine get_sde_coeffs_euler
  !============================================================================
    subroutine calc_thermal_energy(Time, LagrCoord, ThermalEnergy)
        ! Calculate thermal energy density: kb*mp*np*Tp
        ! Temp [keV] Rho [amu/m^3]
        ! Output: ThermalEnergy [J kg / m^3]
        real, intent(in) :: Time, LagrCoord
        real, intent(out) :: ThermalEnergy

        real :: Temp, Rho

    !--------------------------------------------------------------------------
        call interpolate_statevar(Time, LagrCoord, TState_, Temp)
        call interpolate_statevar(Time, LagrCoord, RhoState_, Rho)

        Temp = Temp * ckeV
        ThermalEnergy = Temp * Rho * cProtonMass

    end subroutine calc_thermal_energy
  !============================================================================
    subroutine calc_weight(Time, LagrCoord, Weight)
        ! Statistical weight of each particle: rho*T*dS/B
        real, intent(in)  :: Time, LagrCoord
        real, intent(out) :: Weight

        real :: ThermalEnergy, dSOverB

    !--------------------------------------------------------------------------
        call calc_thermal_energy(Time, LagrCoord, ThermalEnergy)
        call calc_dSOverB(Time, LagrCoord, dSOverB)

        Weight = ThermalEnergy * dSOverB
    end subroutine calc_weight
  !============================================================================
    subroutine get_particle_location(Time, LagrCoord, R)
        real, intent(in) :: Time, LagrCoord
        real, intent(out) :: R
        integer :: iLagr, iVertex, iVertexOld
        real :: iLagrFrac, TimeFrac

    !--------------------------------------------------------------------------
        call interpolate_statevar(Time, LagrCoord, RState_, R)

    end subroutine get_particle_location
  !============================================================================
    subroutine calc_dSOverB(Time, LagrCoord, dSOverB)
        real, intent(in) :: Time, LagrCoord
        real, intent(out) :: dSOverB

        real :: dS, B

    !--------------------------------------------------------------------------
        call interpolate_statevar(Time, LagrCoord, dSState_, dS)
        call interpolate_statevar(Time, LagrCoord, BState_, B)
        dSOverB = dS * cRsun / B

    end subroutine calc_dSOverB
  !============================================================================
    subroutine check_boundary_conditions(Time, LagrCoord, R, IsOutside)
        real, intent(in) :: Time
        real, intent(inout) :: LagrCoord, R
        logical, intent(out) :: IsOutside
        real :: Rtemp
    !--------------------------------------------------------------------------
        IsOutside = .false.

        select case(BoundaryInner)
        case('reflect')
            if(LagrCoord < MinLagr(iLine)) then
                LagrCoord = 2.0 * MinLagr(iLine) - LagrCoord
                ! LagrCoord = MinLagr(iLine) + 1.0
                call interpolate_statevar(Time, LagrCoord, RState_, R)
            end if
        case('absorb')
            if(LagrCoord < MinLagr(iLine)) IsOutside = .true.
        case default
            if(LagrCoord < MinLagr(iLine)) IsOutside = .true.
        end select

        select case(BoundaryInner)
        case('reflect')
            if(LagrCoord > MaxLagr(iLine)) then
                LagrCoord = 2.0 * MaxLagr(iLine) - LagrCoord
                ! LagrCoord = MaxLagr(iLine) - 1.0
                call interpolate_statevar(Time, LagrCoord, RState_, R)
            end if
        case('absorb')
            if(LagrCoord > MaxLagr(iLine)) IsOutside = .true.
        case default
            if(LagrCoord > MaxLagr(iLine)) IsOutside = .true.
        end select

    end subroutine check_boundary_conditions
  !============================================================================
end module PT_ModFieldline!==============================================================================
!==============================================================================
