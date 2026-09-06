! Copyright (C) 2002 Regents of the University of Michigan,
! portions used with permission
! For more information, see http://csem.engin.umich.edu/tools/swmf

module ModMagnit

  use ModConst, ONLY: cBoltzmann, cProtonMass, cElectronMass, cElectronCharge, &
          cKToKEV, cKEVToK
  use ModUtilities, ONLY: CON_stop, CON_set_do_test
  use ModIonosphere, ONLY: IONO_nTheta, IONO_nPsi, DoUseGMPe, DoUseGMPpar, &
          DoUseGMPepar
  use ModNumConst, ONLY: cTiny

  implicit none
  save

  ! Unit conversion factors and other constants:
  real, parameter :: cPtoKProt = cKToKEV/cBoltzmann
  real, parameter :: cPtoKElec = cElectronMass/cBoltzmann * cKToKEV
  real, parameter :: cFACFloor = 1.0E-12 ! A/m2

          ! Configuration parameters:
  ! Diffuse auroral parameters
  real :: ratioPe = 1./6. ! Ratio of electron P to proton P.

  ! Loss cone factors set flux in loss cones for proton & electron diffuse,
  ! monoenergetic, and broadband precip. Factors are for Energy Flux and
  ! Number Flux, each. See Mukhopadhyay et al. 2022 for details.
  real :: ConeEfluxDife = 0.217, ConeEfluxDifp = 0.207, &
          ConeEfluxMono = 1.000, ConeEfluxBbnd = 2.247, &
          ConeNfluxDife = 0.055, ConeNfluxDifp = 0.038, &
          ConeNfluxMono = 0.741, ConeNfluxBbnd = 0.494

  ! Lower limit for GM poynting flux
  real :: GmPoyntFloor = 1E-6

  real :: PrecipRatioLimit = 10000000

  ! Logical to control whether to use polar rain in the magnit model
  logical :: UseMagnitPolarRain=.false., UseMultipleReflections=.false.

  contains
  !============================================================================

  subroutine print_magnit_config(iUnitIn)

    integer, intent(in) :: iUnitIn
    !--------------------------------------------------------------------------

    write(iUnitin,'(a)') "MAGNIT Physics-based Aurora"
    write(iUnitin,'(a)') "(Beta-testing Phase)"
    write(iUnitIn,*) '################## CAUTION ##################'
    write(iUnitIn,*) 'MAGNIT is in BETA TESTING and may not be stable.'
    write(iUnitIn,*) 'Proceed with caution. For issues, contact developers.'
    write(iUnitIn,*) '#############################################'
    if(DoUseGmPe)then
      write(iUnitIn,*) "Electron precip obtained directly from Pe."
    else
      write(iUnitIn,*) "Electron precip obtained from LT-flipped protons"
      write(iUnitIn,'(a, f5.3)') "Pe/P ratio set to ", ratioPe
    end if
    if (DoUseGMPpar) write(iUnitIn,*) "Using anisotropic ion pressure."
    if (DoUseGMPepar) write(iUnitIn,*) "Using anisotropic electron pressure."

  end subroutine print_magnit_config
  !============================================================================

  subroutine magnit_gen_fluxes(NameHemiIn, &
      AvgEDiffe_II, AvgEDiffi_II, AvgEMono_II, AvgEBbnd_II, &
      EfluxDiffe_II, EfluxDiffi_II, EfluxMono_II, EfluxBbnd_II, &
      LatIn_II)

    use ModConst, ONLY: cPi, cKEV
    use ModIonosphere, ONLY: IONO_North_p, IONO_North_rho, &
        IONO_South_p, IONO_South_rho, IONO_NORTH_JR, IONO_SOUTH_JR, &
        IONO_NORTH_invB, IONO_SOUTH_invB, IONO_NORTH_Poynting, &
        IONO_SOUTH_Poynting, IONO_North_Pe, IONO_South_Pe, DoPolarCapSmoothing

    ! Given magnetospheric density, pressure, and FACs, calculate diffuse and
    ! monoenergetic precipitating fluxes.
    ! Resulting units are W/m2 and KeV for energy flux
    ! and average energy, respectively.

    character(len=*), intent(in) :: NameHemiIn

    ! Set arrays to hold precip values. Magnetospheric pressure and density
    ! from GM (SI units), Average Energy and Energy Flux (units of KeV and
    ! W/m2, respectively) for each precip type.
    real, intent(out), dimension(IONO_nTheta, IONO_nPsi) :: &
        AvgEDiffe_II, AvgEDiffi_II, AvgEMono_II, AvgEBbnd_II, &
        EfluxDiffe_II, EfluxDiffi_II, EfluxMono_II, EfluxBbnd_II

    ! Import Hemispheric Latitudes for Magnetic Field Calculations
    real, intent(in), dimension(IONO_nTheta, IONO_nPsi) :: LatIn_II

    ! Set arrays to hold magnetospheric values.
    real, dimension(IONO_nTheta, IONO_nPsi) :: &
        MagP_II, MagNp_II, MagPe_II, MagNe_II, NfluxDiffe_II, NfluxDiffi_II, &
        NfluxBbnd_II, OCFL_II, FAC_II=0, Poynting_II=0, &
            ElectronTemp_II=0, OCFL_flip_II=0, Kc_II

    integer :: i,j

    ! Debug variables:
    logical :: DoTestMe, DoTest
    character(len=*), parameter:: NameSub = 'magnit_gen_fluxes'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if(DoTestMe) &
         write(*,*)'IE: '//NameSub//' called for hemisphere='//NameHemiIn

    ! Set values based on hemisphere. Calculate number density.
    if(NameHemiIn == 'north')then
        MagP_II = iono_north_p
        MagNp_II = iono_north_rho / cProtonMass
        FAC_II = IONO_NORTH_JR
        OCFL_II = IONO_NORTH_invB
        Poynting_II = -IONO_NORTH_Poynting
        if (DoUseGMPe) MagPe_II = iono_north_pe
    else if (NameHemiIn == 'south')then
        MagP_II = iono_south_p
        MagNp_II = iono_south_rho / cProtonMass
        FAC_II = IONO_SOUTH_JR
        OCFL_II = IONO_SOUTH_invB
        Poynting_II = -IONO_SOUTH_Poynting
        if (DoUseGMPe) MagPe_II = iono_south_pe
    else
      call CON_stop(NameSub//' : unrecognized hemisphere - '//NameHemiIn)
    end if

    ! Set default/background values - TBD.
    ! If we need OCFLB calculation, we would put it here.

    ! If not using explicit Pe from GM, obtain values from proton values.
    ! Flip across noon midnight, etc. to fill MagPe, MagNe
    ! Scale Pe using ratioPe
    if(.not. DoUseGMPe) then
      do j=1, Iono_nPsi
        do i = 1, Iono_nTheta
            if(OCFL_II(i,j) > 0 .and. OCFL_II(i, IONO_nPsi-j+1) > 0) then
                MagPe_II(i, j) = ratioPe * MagP_II(i, IONO_nPsi-j+1)
                MagNe_II(i, j) = MagNp_II(i, IONO_nPsi-j+1)
                OCFL_flip_II(i, j) = OCFL_II(i, IONO_nPsi-j+1)
            else
                MagPe_II(i,j) = ratioPe * MagP_II(i, j)
                MagNe_II(i,j) = MagNp_II(i, j)
                OCFL_flip_II(i, j) = OCFL_II(i, j)
            end if
        end do
      end do
      OCFL_II = OCFL_flip_II
    else
      MagNe_II = MagNp_II
    end if

    ! Calculate diffuse precipitation: protons.
    AvgEDiffi_II  = MagP_II / MagNp_II  ! Temp = P/nk in Joules
    NfluxDiffi_II = ConeNfluxDifp * MagNp_II * AvgEDiffi_II**0.5 / &
                    sqrt(2 * cPi * cProtonMass)  ! units of #/m2/s
    ! units of W/m2
    EfluxDiffi_II = 2 * NfluxDiffi_II * AvgEDiffi_II * &
            ConeEfluxDifp/ConeNfluxDifp
    ! Recalc to make consistent with ConeFactors (and get units of keV)
    AvgEDiffi_II = EfluxDiffi_II / (NfluxDiffi_II * cKEV)

    ! Calculate diffuse precipitation: electrons.
    ElectronTemp_II  = MagPe_II / MagNe_II  ! T = P/nk in Joules

    NfluxDiffe_II = ConeNfluxDife * MagNe_II * ElectronTemp_II**0.5 / &
                 sqrt(2 * cPi * cElectronMass)  ! units of #/m2/s
    ! units of W/m2
    EfluxDiffe_II = 2 * NfluxDiffe_II * ElectronTemp_II * &
            ConeEfluxDife/ConeNfluxDife
    ! Recalc to make consistent with ConeFactors (and get units of keV)
    AvgEDiffe_II = EfluxDiffe_II / (NfluxDiffe_II * cKEV)

    if(UseMultipleReflections) then
       where(AvgEDiffe_II >= 0.5 .and. AvgEDiffe_II <= 30.0)
          Kc_II = 3.36 - exp(0.597 - 0.37 * AvgEDiffe_II + 0.00794 * &
            AvgEDiffe_II ** 2)
          EfluxDiffe_II = Kc_II * EfluxDiffe_II
          AvgEDiffe_II = 0.073 + 0.933 * AvgEDiffe_II - 0.0092 * &
            AvgEDiffe_II ** 2
       end where
    end if

    ! Smooth area between closed and open field lines
    if(DoPolarCapSmoothing) then
        call smooth_polar_cap(ElectronTemp_II, OCFL_II, NameHemiIn)
        call smooth_polar_cap(NfluxDiffe_II, OCFL_II, NameHemiIn)
        call smooth_polar_cap(EfluxDiffe_II, OCFL_II, NameHemiIn)
        call smooth_polar_cap(AvgEDiffe_II, OCFL_II, NameHemiIn)
        call smooth_polar_cap(EfluxDiffi_II, OCFL_II, NameHemiIn)
        call smooth_polar_cap(AvgEDiffi_II, OCFL_II, NameHemiIn)
    end if

    ! Calculate monoenergetic electron precipitation
    call monoenergetic_flux(FAC_II, OCFL_II, NfluxDiffe_II, ElectronTemp_II, &
            AvgEDiffe_II, LatIn_II, EfluxMono_II, AvgEMono_II)

    where (EfluxMono_II < EfluxDiffe_II + cTiny)
        EfluxMono_II = EfluxDiffe_II
        AvgEMono_II = AvgEDiffe_II
    end where

    where (EfluxMono_II == EfluxDiffe_II .and. OCFL_II < 0)
        EfluxMono_II = MINVAL(EfluxDiffe_II)
        AvgEMono_II = MINVAL(AvgEDiffe_II)
        EfluxDiffe_II = MINVAL(EfluxDiffe_II)
        AvgEDiffe_II = MINVAL(AvgEDiffe_II)
    end where

    call broadband_flux(Poynting_II, EfluxBbnd_II, AvgEBbnd_II)

  end subroutine magnit_gen_fluxes
  !============================================================================
  subroutine monoenergetic_flux(FAC_II, OCFL_II, NfluxDiffe_II, &
          ElectronTemp_II, AvgEDiffe_II, LatIn_II, EfluxMono_II, AvgEMono_II, &
          PotOut_II)

    use ModConst, ONLY: cKEV
    use ModPlanetConst, ONLY: rPlanet_I, IonoHeightPlanet_I, Earth_

    real, intent(out), dimension(IONO_nTheta, IONO_nPsi) :: EfluxMono_II, &
                                                            AvgEMono_II
    real, intent(out), dimension(IONO_nTheta, IONO_nPsi), optional :: PotOut_II

    ! Import Hemispheric Latitudes for Magnetic Field Calculations
    real, intent(in), dimension(IONO_nTheta, IONO_nPsi) :: FAC_II, LatIn_II, &
            NfluxDiffe_II, AvgEDiffe_II, ElectronTemp_II, OCFL_II

    real, dimension(IONO_nTheta, IONO_nPsi) :: PrecipRatio_II=0, &
            Potential_II=0, MirrorRatio_II=0, VExponent_II=0, &
            PotentialTerm_II=0, NfluxMono_II
    !--------------------------------------------------------------------------
    ! Calculate discrete electron precipitation
    ! Calculate Nflux from current
    Potential_II = 0
    NfluxMono_II = FAC_II / cElectronCharge
    ! Calculate Parallel Potential Drop
    ! Calculate ratio of diffuse to monoenergetic precipitation
    PrecipRatio_II = NfluxMono_II / NfluxDiffe_II

    ! Calculate ratio of ionospheric magnetic field to plasma sheet
    ! magnetic field
    MirrorRatio_II = (rPlanet_I(Earth_) / (sin(LatIn_II)**2 * &
            (rPlanet_I(Earth_) + IonoHeightPlanet_I(Earth_))))**3 * &
            sqrt(1 + 3*cos(LatIn_II)**2)

    where(PrecipRatio_II >= MirrorRatio_II)
        NfluxMono_II = NfluxDiffe_II * MirrorRatio_II
        PrecipRatio_II = MirrorRatio_II
     end where

    where(PrecipRatio_II > PrecipRatioLimit)
       PrecipRatio_II = PrecipRatioLimit
    end where

    ! Potential calculations only valid where 1 <= NumCoefficient <= MirrorRatio
    where(1 <= PrecipRatio_II .and. OCFL_II > 0)
      ! Put it all together into potential
      Potential_II = ElectronTemp_II / cElectronCharge * (1 - MirrorRatio_II) &
              * LOG((MirrorRatio_II - PrecipRatio_II) / (MirrorRatio_II - 1))

      ! Split up large calculation into a few steps
      ! Calculate large potential exponent
      VExponent_II = EXP(-cElectronCharge * Potential_II / ((ElectronTemp_II) * &
            (MirrorRatio_II - 1)))
      ! Calculate product term
      PotentialTerm_II = ((1 - VExponent_II) * ConeEfluxMono / &
            (1 + ((1 - 1/MirrorRatio_II) * VExponent_II)) * cElectronCharge * &
            Potential_II) + AvgEDiffe_II * cKEV

      ! Plug into equation for EFlux
      EfluxMono_II = NfluxMono_II * PotentialTerm_II

    elsewhere(1 <= PrecipRatio_II .and. OCFL_II <= 0)
      Potential_II = ElectronTemp_II / cElectronCharge * (PrecipRatio_II - 1)

      ! In polar cap use linear Knight relationship
      ! open field lines = no mirroring/trapped particles
      EfluxMono_II = NfluxDiffe_II * AvgEDiffe_II * cKEV / 2 &
              * (PrecipRatio_II ** 2 + 1)
    elsewhere
      NfluxMono_II = NfluxDiffe_II
      ! Reconstruct Diffuse flux where there is no potential
      EfluxMono_II = NfluxDiffe_II * AvgEDiffe_II * cKEV
      Potential_II = 0
    end where
    if(present(PotOut_II)) PotOut_II = Potential_II

    ! Calculate Avg E in keV
    AvgEMono_II = EfluxMono_II / (NfluxMono_II * cKEV)

  end subroutine monoenergetic_flux
  !============================================================================
  subroutine broadband_flux(Poynting_II, EfluxBbnd_II, AvgEBbnd_II)

    use ModConst, ONLY: cKEV

    real, intent(out), dimension(IONO_nTheta, IONO_nPsi) :: EfluxBbnd_II, &
                                                            AvgEBbnd_II
    real, intent(in), dimension(IONO_nTheta, IONO_nPsi) :: Poynting_II

    integer :: i,j
    real, dimension(IONO_nTheta, IONO_nPsi) :: NfluxBbnd_II=0
    !--------------------------------------------------------------------------
    ! Calculate broadband electron precipitation
    EfluxBbnd_II = 0
    AvgEBbnd_II = 0

    ! Using empirical relationships from Zhang et al. 2015
    where(Poynting_II > GmPoyntFloor)
        EfluxBbnd_II = 2e-3 * (ConeEfluxBbnd * Poynting_II) ** 0.5
        NfluxBbnd_II = 3e13 * (ConeNfluxBbnd * Poynting_II) ** 0.47
        AvgEBbnd_II = EfluxBbnd_II / (NfluxBbnd_II * cKEV)
    end where

  end subroutine broadband_flux
  !============================================================================
  subroutine smooth_polar_cap(var_II, OCFL_II, NameHemiIn, ImBoundary_II)

    use ModIonosphere, ONLY: IONO_nTheta, IONO_nPsi, PCapSmoothingSize

    real, intent(inout), dimension(IONO_nTheta, IONO_nPsi) :: var_II
    real, intent(in),  dimension(IONO_nTheta, IONO_nPsi) :: OCFL_II
    real, intent(in),  dimension(IONO_nTheta, IONO_nPsi), optional :: &
                                                                ImBoundary_II

    character(len=*), intent(in) :: NameHemiIn

    integer :: i, j, closed_max, open_min, diff
    !--------------------------------------------------------------------------
    ! 1D smoothing between open and closed field line regions

    ! If IM boundary is present, use the gap as the space to smooth
    if (present(ImBoundary_II)) then
        do j = 1, Iono_nPsi
            if (NameHemiIn == 'north') then
                open_min = 1
                boundaryIMN: do i = 1, Iono_nTheta
                    if (OCFL_II(i,j) < 0) open_min = i
                    if (ImBoundary_II(i,j) > 0) then
                        closed_max = i
                        EXIT boundaryIMN
                    end if
                end do boundaryIMN
                diff = closed_max - open_min
                ! Leave this MLT if no gap region
                if (diff <= 0) CYCLE
                ! Linearly reconstruct gap
                do i = open_min, closed_max
                    var_II(i,j) = var_II(open_min,j) * real(closed_max-i)/diff &
                                + var_II(closed_max,j) * real(i - open_min)/diff
                end do
            else
                open_min = Iono_nTheta
                boundaryIMS: do i = Iono_nTheta, 1, -1
                    if (OCFL_II(i,j) < 0) open_min = i
                    if (ImBoundary_II(i,j) > 0) then
                        closed_max = i
                        EXIT boundaryIMS
                    end if
                end do boundaryIMS
                diff = open_min - closed_max
                ! Leave this MLT if no gap region
                if (diff <= 0) CYCLE
                ! Linearly reconstruct gap
                do i = closed_max, open_min
                    var_II(i,j) = var_II(open_min,j) * real(i-closed_max)/diff &
                                + var_II(closed_max,j) * real(open_min - i)/diff
                end do
            end if
        end do
    ! If IM boundary is not present, use size in degrees (defaults to 5 degrees)
    else
        diff = CEILING(PCapSmoothingSize * (IONO_nTheta-1) / 90)
        do j = 1, Iono_nPsi
            if (NameHemiIn == 'north') then
                closed_max = 1
                boundaryGMN: do i = 1, Iono_nTheta
                    if (OCFL_II(i,j) < 0) closed_max = i
                    if (OCFL_II(i,j) > 0) EXIT boundaryGMN
                end do boundaryGMN
                ! closed_max = closed_max + diff/2
                closed_max = max(closed_max + 1, 1)
                open_min = max(closed_max - diff, 1)
                ! Linearly reconstruct gap
                do i = open_min, closed_max
                    var_II(i,j) = var_II(open_min,j) * real(closed_max-i)/diff &
                                + var_II(closed_max,j) * real(i-open_min)/diff
                end do
            else
                closed_max = Iono_nTheta
                boundaryGMS: do i = Iono_nTheta, 1, -1
                    if (OCFL_II(i,j) < 0) closed_max = i
                    if (OCFL_II(i,j) > 0) EXIT boundaryGMS
                end do boundaryGMS
                ! closed_max = closed_max - diff/2
                closed_max = min(closed_max - 1, IONO_nTheta)
                open_min = min(closed_max + diff, IONO_nTheta)
                ! Linearly reconstruct gap
                do i = open_min, closed_max, -1
                    var_II(i,j) = var_II(open_min,j) * real(i-closed_max)/diff &
                                + var_II(closed_max,j) * real(open_min-i)/diff
                end do
            end if
        end do
    end if

  end subroutine smooth_polar_cap
  !============================================================================

end module ModMagnit
!==============================================================================
