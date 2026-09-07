! Copyright (C) 2002 Regents of the University of Michigan,
! portions used with permission
! For more information, see http://csem.engin.umich.edu/tools/swmf

module ModImp

  ! Inner Magnetosphere Precipitation model

  use ModIonosphere, ONLY: IONO_nTheta, IONO_nPsi

  use ModUtilities, ONLY: CON_stop, CON_set_do_test

  implicit none
  save

  real, parameter :: ImEfluxFloor = 1E-3, ImAveEFloor = 1E-6

  logical :: UseSouthTracing = .true.

contains
  !============================================================================
  subroutine imp_gen_fluxes(NameHemiIn, AvgEDiffe_II, AvgEDiffi_II, &
       AvgEMono_II, AvgEBbnd_II, EfluxDiffe_II, EfluxDiffi_II, EfluxMono_II,&
       EfluxBbnd_II, LatIn_II)

    use ModMagnit, ONLY: monoenergetic_flux, broadband_flux, smooth_polar_cap,&
         ConeEfluxDifp, ConeNfluxDifp, ConeEfluxDife, ConeNfluxDife, &
         ratioPe, UseMultipleReflections
    use ModIonosphere, ONLY: IONO_NORTH_JR, IONO_SOUTH_JR, &
         IONO_NORTH_invB, IONO_SOUTH_invB, IONO_NORTH_Poynting, &
         IONO_SOUTH_Poynting, IONO_north_im_boundary, &
         IONO_south_im_boundary, DoPolarCapSmoothing, &
         iono_north_im_aveeElec, iono_south_im_aveeElec, &
         iono_north_im_efluxElec, iono_south_im_eFluxElec, &
         iono_north_im_aveeHydr, iono_south_im_aveeHydr, &
         iono_north_im_efluxHydr, iono_south_im_eFluxHydr, &
         DoUseIMSpectrum, iono_north_im_nElecPrec, iono_south_im_nElecPrec, &
         IONO_North_p, IONO_North_rho, IONO_South_p, IONO_South_rho, &
         DoUseGmPe, IONO_North_Pe, IONO_South_Pe, IONO_north_im_jr, &
         IONO_south_im_jr
    use ModConst, ONLY: cKEV, cProtonMass, cElectronMass, cPi

    real, intent(out), dimension(IONO_nTheta, IONO_nPsi) :: &
         AvgEDiffe_II, AvgEDiffi_II, AvgEMono_II, AvgEBbnd_II, &
         EfluxDiffe_II, EfluxDiffi_II, EfluxMono_II, EfluxBbnd_II

    real, intent(in), dimension(IONO_nTheta, IONO_nPsi) :: LatIn_II

    real, dimension(IONO_nTheta, IONO_nPsi) :: &
         FAC_II, OCFL_II, NfluxDiffe_II, ElectronTemp_II, Potential_II, &
         Poynting_II, ImBoundary_II, MagP_II, MagNp_II, MagPe_II, MagNe_II, &
         NfluxDiffi_II, MhdElectronTemp_II, MhdNfluxDiffe_II, MhdAvgEDiffe_II, &
         MhdEfluxMono_II, MhdAvgEMono_II, ImFac_II, Kc_II

    character(len=*), intent(in) :: NameHemiIn

    character(len=*), parameter:: NameSub = 'imp_gen_fluxes'
    !--------------------------------------------------------------------------
   if (NameHemiIn == 'south') then
      AvgEDiffe_II = iono_south_im_aveeElec ! CIMI gives in keV
      EfluxDiffe_II = iono_south_im_efluxElec / 1000.0 ! mW/m^2 to W/m^2
      AvgEDiffi_II = iono_south_im_aveeHydr
      EfluxDiffi_II = iono_south_im_efluxHydr / 1000.0
    else if (NameHemiIn == 'north') then
      AvgEDiffe_II = iono_north_im_aveeElec
      EfluxDiffe_II = iono_north_im_efluxElec / 1000.0
      AvgEDiffi_II = iono_north_im_aveeHydr
      EfluxDiffi_II = iono_north_im_efluxHydr / 1000.0
    else
       call CON_stop(NameSub//' : unrecognized hemisphere - '//&
                NameHemiIn)
    end if

    if(UseMultipleReflections) then
       where(AvgEDiffe_II >= 0.5 .and. AvgEDiffe_II <= 30.0)
          Kc_II = 3.36 - exp(0.597 - 0.37 * AvgEDiffe_II + 0.00794 * &
            AvgEDiffe_II ** 2)
          EfluxDiffe_II = Kc_II * EfluxDiffe_II
          AvgEDiffe_II = 0.073 + 0.933 * AvgEDiffe_II - 0.0092 * &
            AvgEDiffe_II ** 2
       end where
    end if

    where(Poynting_II < 0) Poynting_II = 0

    if(NameHemiIn == 'north')then
      MagP_II = iono_north_p
      MagNp_II = iono_north_rho / cProtonMass
      FAC_II = IONO_NORTH_JR
      OCFL_II = IONO_NORTH_invB
      Poynting_II = -IONO_NORTH_Poynting
      if (DoUseGMPe) MagPe_II = iono_north_pe
      ImBoundary_II = IONO_north_im_boundary
      ImFac_II = IONO_NORTH_Im_Jr
    else if (NameHemiIn == 'south')then
      MagP_II = iono_south_p
      MagNp_II = iono_south_rho / cProtonMass
      FAC_II = IONO_SOUTH_JR
      OCFL_II = IONO_SOUTH_invB
      Poynting_II = -IONO_SOUTH_Poynting
      if (DoUseGMPe) MagPe_II = iono_south_pe
      ImBoundary_II = IONO_south_im_boundary
      ImFac_II = IONO_SOUTH_Im_Jr
    end if

    if(.not. DoUseGMPe) then
      MagPe_II = MagP_II * ratioPe
      MagNe_II = MagNp_II
    else
      MagNe_II = MagNp_II
    end if

    where(ImBoundary_II > 1) ImBoundary_II = 1

    MhdElectronTemp_II  = MagPe_II / MagNe_II  ! T = P/nk in Joules
    MhdNfluxDiffe_II = ConeNfluxDife * MagNe_II * MhdElectronTemp_II**0.5 / &
                  sqrt(2 * cPi * cElectronMass)  ! units of #/m2/s
    ! units of W/m2
    MhdAvgEDiffe_II = MhdElectronTemp_II / cKEV * ConeEfluxDife/ConeNfluxDife
    MhdElectronTemp_II = MhdElectronTemp_II * ConeEfluxDife/ConeNfluxDife
    ! Calculate mononenergetic precipitation in polar cap using MHD state vars
    call monoenergetic_flux(FAC_II, OCFL_II, MhdNfluxDiffe_II, MhdElectronTemp_II, &
            MhdAvgEDiffe_II, LatIn_II, MhdEfluxMono_II, MhdAvgEMono_II, &
            PotOut_II=Potential_II)

    ! Replace cimi results with MHD where discrete precip > diffuse and potential exists
    where(Potential_II > 0 .and. MhdEfluxMono_II > EfluxDiffe_II)
       EfluxMono_II = MhdEfluxMono_II
       AvgEMono_II  = MhdAvgEMono_II
    elsewhere
       EfluxMono_II = EfluxDiffe_II
       AvgEMono_II  = AvgEDiffe_II
    end where

    call broadband_flux(Poynting_II, EfluxBbnd_II, AvgEBbnd_II)

    if (DoUseIMSpectrum) then
      call CON_stop('IM spectrum not implemented yet')
       !
      ! call imp_spectral_to_UA(NameHemiIn, Potential_II)
    end if

  end subroutine imp_gen_fluxes
  !============================================================================
end module ModImp
!==============================================================================
