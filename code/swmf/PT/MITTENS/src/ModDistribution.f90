!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModDistribution
    use ModKind
    use PT_ModConst
    use PT_ModProc, ONLY: iProc, iComm, iError
    use PT_ModUnit, ONLY: kinetic_energy_to_momentum

    implicit none
    SAVE

    integer :: nEnergyBins, nLagrBins
    integer :: InjEnergyIndex
    real :: eBinMin, eBinMax
    real :: BinTimeWindow, TotalWeight, InjEnergy, InjPcubed, InjCoeff
    ! Array for storing the counts
    real, allocatable :: Counts_II(:,:)
    ! Bins
    real, allocatable :: EnergyBin_I(:), PcubedBin_I(:), LagrBin_I(:)
    ! How many total injected real protons
    ! integral of f(p_inj) over phase space)
    real :: IntegralOverfInj

    logical :: DoBinEntireFieldline
    real :: BinCenterRs
    integer :: BinWidthLagr

contains
  !============================================================================
    subroutine read_param(NameCommand)

        use ModReadParam, ONLY: read_var
        use ModUtilities, ONLY: CON_stop

        character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
        select case(NameCommand)

        case("#SDEBINNING")
            call read_var('DoBinEntireFieldline', DoBinEntireFieldline)
            if(DoBinEntireFieldline) then
                call read_var('nLagrBins', nLagrBins)
            else
                nLagrBins = 1
                call read_var('BinCenterRs', BinCenterRs)
                call read_var('BinWidthLagr', BinWidthLagr)
            end if
            call read_var('BinTimeWindow', BinTimeWindow)

        case('#DISTRIBUTION')

            call read_var('nEnergyBins', nEnergyBins)
            call read_var('eBinMin', eBinMin)
            call read_var('eBinMax', eBinMax)
            call read_var('InjEnergy', InjEnergy)
            call read_var('InjCoeff', InjCoeff)

            ! Convert to joules
            eBinMin = eBinMin * ckeV
            eBinMax = eBinMax * ckeV
            InjEnergy = InjEnergy * ckeV
            InjPcubed = (kinetic_energy_to_momentum(InjEnergy)**3.0) / 3.0

        case default
            call CON_stop(NameSub//' Unknown command '//NameCommand)
        end select
    end subroutine read_param
  !============================================================================
    subroutine init

        integer :: iBin
        real :: dLogE, dL

        ! allocate bin and solution arrays
    !--------------------------------------------------------------------------
        allocate(Counts_II(nEnergyBins, nLagrBins)); Counts_II = 0.0
        allocate(EnergyBin_I(nEnergyBins+1))
        allocate(PcubedBin_I(nEnergyBins+1))
        allocate(LagrBin_I(nLagrBins+1))

        ! energy bins equally space in log10
        dLogE = (log10(eBinMax) - log10(eBinMin)) / (nEnergyBins)
        EnergyBin_I(1) = eBinMin
        PcubedBin_I(1) = kinetic_energy_to_momentum(eBinMin)**3.0 / 3.0
        do iBin = 2, nEnergyBins+1
            EnergyBin_I(iBin) = &
                10.0**(log10(eBinMin)+ dLogE*(iBin-1))
            PcubedBin_I(iBin) = &
                kinetic_energy_to_momentum(EnergyBin_I(iBin))**3.0 / 3.0
        end do

        ! index corresponding to bin of injected distribution function
        InjEnergyIndex = minloc(InjEnergy - EnergyBin_I, &
                                mask = (InjEnergy - EnergyBin_I >= 0), dim = 1)

        ! initialize total weight and particle number integral
        TotalWeight = 0.0
        IntegralOverfInj = 0.0

    end subroutine init
  !============================================================================
    subroutine set_lagr_bins(iLine)
        ! Set the lagrangian phase space bins for this timestep
        ! Bins the entire fieldline
        use PT_ModGrid, ONLY: MinLagr, MaxLagr, State_VIB, R_

        integer, intent(in) :: iLine

        integer :: iBin
        real :: dL

    !--------------------------------------------------------------------------
        if(nLagrBins == 1) then
            iBin = minloc(BinCenterRs - State_VIB(R_, :, iLine), &
                          mask = (BinCenterRs -  State_VIB(R_, :, iLine) > 0), dim = 1)
            LagrBin_I(1) = iBin - BinWidthLagr
            LagrBin_I(2) = iBin + 1 + BinWidthLagr
        else
            dL = real(MaxLagr(iline) - 1 - MinLagr(iLine)) / real(nLagrBins)
            do iBin = 1, nLagrBins+1
                LagrBin_I(iBin) = real(MinLagr(iLine)) + dL * (iBin - 1)
            end do
        end if

    end subroutine set_lagr_bins
  !============================================================================
    subroutine update_integral_over_finj(Time, LagrInject)

        ! Equation 124 of Sokolov et al., 2023 "High resolution finite...".
        ! Integrate injection distribution function over phase space.
        ! Integration over momentum occurs over energy range:
        !       [InjEnergy, InjEnergy + 1]
        ! dSl assumed to be 1.
        ! Factor of 4pi*Psi does not need to be
        !   included as it will cancel during normalization.

        ! TODO: This will need to be iLine-dependent

        use PT_ModFieldline, ONLY: calc_thermal_energy, calc_dSOverB
        use PT_ModUnit, ONLY: kinetic_energy_to_momentum
        real, intent(in) :: Time, LagrInject

        real :: pInj1, pInj2, fPinj
        real :: dSOverB, ThermalEnergy

        ! this is kb*rho*T
    !--------------------------------------------------------------------------
        call calc_thermal_energy(Time, LagrInject, ThermalEnergy)
        call calc_dSOverB(Time, LagrInject, dSOverB)

        ! Inject particles with p**-5 distribution commonly seen in
        ! suprathermal tail.
        ! Assume this injection takes place from InjEnergy -> InjEnergy + 1 keV
        pInj1 = kinetic_energy_to_momentum(InjEnergy)
        pInj2 = kinetic_energy_to_momentum(InjEnergy + ckeV)

        ! Definite integral of finj p^-5 d(p^3/3) from p1 to p2
        fPinj = -0.5 * InjCoeff * ThermalEnergy / cPi * &
                (pInj2**(-2.0) - pInj1**(-2.0))

        IntegralOverfInj = IntegralOverfInj + fPinj * dSOverB

    end subroutine update_integral_over_finj
  !============================================================================
    subroutine increase_total_weight(Weight)
        real, intent(in) :: Weight
    !--------------------------------------------------------------------------
        TotalWeight = TotalWeight + Weight
    end subroutine increase_total_weight
  !============================================================================
    subroutine bin_particle(LagrCoord, Momentum, Weight)
        ! Bin in time, energy, and location
        real, intent(in) :: LagrCoord, Momentum, Weight
        integer :: iL, iE, i

        ! if particle is outside bounds of phase space bins - return
    !--------------------------------------------------------------------------
        if(LagrCoord < LagrBin_I(1).or.LagrCoord >= LagrBin_I(nLagrBins+1)) RETURN
        if(Momentum < PcubedBin_I(1).or.Momentum >= PcubedBin_I(nEnergyBins+1)) RETURN
        !--------------------------------------------------------------------------
        ! index of bins particle is in
        iL = minloc(LagrCoord - LagrBin_I, mask = (LagrCoord - LagrBin_I >= 0), dim = 1)
        iE = minloc(Momentum - PcubedBin_I, mask = (Momentum - PcubedBin_I >= 0), dim = 1)

        ! Increase the count in this bin by the weight of particle
        Counts_II(iE, iL) = Counts_II(iE, iL) + Weight

    end subroutine bin_particle
  !============================================================================
    subroutine calculate_distribution_function(Time, DistFunc_II)
        ! Conversion of SDE solution --> PDE solution (F = dS/B * f)
        ! PDF of SDE trajectories in phase space = solution of FP equation
        ! PDF of SDE = P(traj in bin) divided by phase space bin volume
        ! P(traj in bin) = probability = sum of weights in bin / totalWeight
        ! Renormalization of the distribution function is necessary
        ! Renormalize by ensuring the integral over phase space of
        ! f_solution = integral over phase space of f_inject

        use PT_ModFieldline, ONLY: calc_dSOverB
        use PT_ModUnit, ONLY: kinetic_energy_to_momentum

        real, intent(in) :: Time
        real, intent(out) :: DistFunc_II(nEnergyBins, nLagrBins)
        real :: dP, dLagr, p1, p2
        real :: BinVolume, dSOverB
        integer :: iE, iL

        ! Special case at start of run if no particles have been injected
    !--------------------------------------------------------------------------
        if(TotalWeight == 0) then
            DistFunc_II = 0.0
            RETURN
        end if

        do iL = 1, nLagrBins
            dLagr = LagrBin_I(iL+1) - LagrBin_I(iL)
            do iE = 1, nEnergyBins

                call calc_dSOverB(Time, LagrBin_I(iL) + 0.5 * dLagr, &
                                  dSOverB)

                dP = PcubedBin_I(iE+1) - PcubedBin_I(iE)
                BinVolume = dLagr * dP
                ! probability / phase space bin volume
                DistFunc_II(iE, iL) = Counts_II(iE, iL) / &
                                      (BinVolume * TotalWeight)

                ! Multiple by (B/dS) to convert from F -> f
                DistFunc_II(iE, iL) = DistFunc_II(iE, iL) / dSOverB

            end do
        end do

        ! renormalize such that the integral over phase space is conserved
        DistFunc_II = DistFunc_II * IntegralOverfInj

    end subroutine calculate_distribution_function
  !============================================================================
    subroutine calculate_flux(Time, Flux_II)
        use PT_ModUnit, ONLY: kinetic_energy_to_momentum
        real, intent(in) :: Time
        real, intent(out) :: Flux_II(nEnergyBins, nLagrBins)

        real :: m2cm = 100.0 ! meters -> cm
        real :: j2MeV =  6.242e12 ! joules -> MeV
        real :: AvgEnergy, AvgMomentum
        real :: DistFunc_II(nEnergyBins, nLagrBins)

        integer :: iP

        ! Updates Counts_II = f
    !--------------------------------------------------------------------------
        call calculate_distribution_function(Time, DistFunc_II)

        ! Convert j = f*p^2 + conversion s.t. j = [pfu / MeV]
        do iP = 1, nEnergyBins
            AvgEnergy = 0.5 * (EnergyBin_I(iP+1) + EnergyBin_I(iP))
            AvgMomentum = kinetic_energy_to_momentum(AvgEnergy)
            Flux_II(iP, :) = DistFunc_II(iP, :) * AvgMomentum**2.0 / &
                             (m2cm**2.0 * j2MeV)
        end do

    end subroutine calculate_flux
  !============================================================================
    subroutine calculate_integral_flux(Spectra, MinEnergy, IntFlux)
        real, intent(in) :: Spectra(:), MinEnergy
        real, intent(out) :: IntFlux
        real :: dE(nEnergyBins)

    !--------------------------------------------------------------------------
        dE = EnergyBin_I(2:nEnergyBins+1) - EnergyBin_I(1:nEnergyBins)
        IntFlux = sum(Spectra * dE / 1000.0, mask = (EnergyBin_I > MinEnergy))

    end subroutine calculate_integral_flux
  !============================================================================
end module PT_ModDistribution!==============================================================================
