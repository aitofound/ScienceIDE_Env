!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module ModConductance

  use ModIonosphere
  use ModMagnit, ONLY: magnit_gen_fluxes
  use ModImp, ONLY: imp_gen_fluxes

  implicit none
  save

  ! Parameters:
  real, parameter :: &
       IONO_Min_EFlux = 0.1e-16, &  ! W/m2
       IONO_Min_Ave_E = 0.5,     &  ! keV
       SigmaPar       = 1000.0      ! Parallel conductance, Siemens

  ! Logicals to control what conductance sources are used.
  logical :: DoUseEuvCond=.true., DoUseAurora=.true., DoUseDiffI=.true., &
       DoUseDiffE=.true., DoUseMono=.true., DoUseBbnd=.true., &
       UsePrecipSmoothing=.true., IsImCoupled=.false., IsUaCoupled=.false., &
       DoCoupleUaConductance=.true.

  ! Upper latitude limits for IPE conductances
  real:: LatFullIpe = 90.0, LatFullRim = 91.0

  ! Name of auroral model to use, defaults to 'RLM5'
  character(len=8) :: NameAuroraMod = 'RLM5'

  ! Name of Conductance Relationships used
  character(len=4) :: eCondRel = 'robi', iCondRel = 'gala', KernelType = 'gaus'

  ! Set Kernel Size
  integer :: KernelSize = 3

  ! Background & constant conductance values:
  real :: f107_flux=-1, SigmaHalConst=0, SigmaPedConst=0, &
       StarLightCond=1.0,  & ! replaces starlightpedconductance
       PolarCapPedCond=0.25, & ! replaces PolarCapPedConductance
       KernelSpread=1.0, & ! Sets spread of Gaussian kernel for smoothing
       eCondLimit=10000., & ! Electron Conductance energy limit in keV
       eLimitScale=10. ! Sets scaling factor for above eCondLimit

  ! Floor values for GM density and pressure, SI units:
  real, parameter :: GmRhoFloor = 1E-21, GmPFloor = 1E-13, GMPeFloor = 1E-13

  ! Arrays to hold components of conductance - 1 array
  ! for each source of conductance.  Long term storage for output files.
  ! THESE ARE ONLY NEEDED FOR PUTTING OUTPUT IN OLD FILE TYPES.
  ! If we make new output files that are written here, we could avoid this.
  real, dimension(IONO_nTheta,IONO_nPsi) :: &
       SigHalEuv_NORTH=0.0, SigHalEuv_SOUTH=0.0, &
       SigPedEuv_NORTH=0.0, SigPedEuv_SOUTH=0.0

contains
  !============================================================================
  subroutine generate_conductance(NameHemiIn)
    ! Calculate conductance from all sources and sum into the IONO_*_Sigma[PH]
    ! arrays, and compute the spatial derivatives.
    ! Arguments:
    !     NameHemiIn -- Select hemisphere (north, south).

    use ModProcIE, ONLY: iProc

    ! Dummy variables:
    character(len=5), intent(in) :: NameHemiIn

    ! Local variables:
    integer :: iBlockNow, nTheta, nPsi, i, j

    ! Local containers for grid in current hemisphere:
    real, dimension(IONO_nTheta, IONO_nPsi) :: psi, theta

    ! Local containers for precipitation:
    real, dimension(IONO_nTheta, IONO_nPsi) :: &
         AvgEDiffe_II=0.0, EfluxDiffe_II=0.0,  &
         AvgEDiffi_II=0.0, EfluxDiffi_II=0.0,  &
         AvgEMono_II=0.0,  EfluxMono_II=0.0,   &
         AvgEBbnd_II=0.0,  EfluxBbnd_II=0.0

    ! Local container for hemispheric grid spacings:
    real, dimension(IONO_nTheta) :: dTheta
    real, dimension(IONO_nPsi)   :: dPsi

    ! Local containers for conductance components:
    real, dimension(IONO_nTheta,IONO_nPsi) ::      &
         SigmaHalEuv_II =0.0, SigmaPedEuv_II =0.0, &
         SigmaHalBbnd_II=0.0, SigmaPedBbnd_II=0.0, &
         SigmaHalDiffi_II=0.0,SigmaPedDiffi_II=0.0,&
         SigmaHalDiffe_II=0.0,SigmaPedDiffe_II=0.0,&
         SigmaHalMono_II =0.0,SigmaPedMono_II =0.0

    ! Local containers for spatial derivatives:
    real, dimension(IONO_nTheta,IONO_nPsi) :: &
         SigmaH, SigmaP, &
         SigmaThTh, SigmaThPs, SigmaPsPs, &
         dSigmaThTh_dTheta, dSigmaThTh_dPsi, dSigmaThPs_dTheta, &
         dSigmaThPs_dPsi, dSigmaPsPs_dTheta, dSigmaPsPs_dPsi

    ! Intermediate values:
    real, dimension(IONO_nTheta,IONO_nPsi)  :: &
         sn, cs, sn2, cs2, cs3, cs4, C

    ! IPE related
    integer:: iTheta
    real:: Lat, WeightIpe, WeightRim

    ! Debug variables:
    logical :: DoTestMe, DoTest
    character(len=*), parameter:: NameSub = 'generate_conductance'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if(DoTestMe) &
         write(*,*)'IE: '//NameSub//' called for hemisphere='//NameHemiIn

    ! Set some convenience variables to prevent very long lines:
    nPsi = IONO_nPsi
    nTheta = IONO_nTheta

    ! Configure approach to match hemisphere:
    if(NameHemiIn == 'north') then
       iBlockNow = 1
       dTheta = dTheta_North
       dPsi   = dPsi_North
    else if (NameHemiIn == 'south') then
       iBlockNow = 2
       dTheta = dTheta_South
       dPsi   = dPsi_South
    else
       call CON_stop(NameSub//': Unrecognized hemisphere -- '//NameHemiIn)
    end if

    ! Create temporary arrays to hold colat/lon for current hemisphere:
    if(NameHemiIn == 'north')then
       theta = IONO_NORTH_Theta
       psi = IONO_NORTH_Psi
    else
       theta = IONO_SOUTH_Theta
       psi = IONO_SOUTH_Psi
    end if

    ! Calculate each source/contribution to conductance individually:
    ! Add EUV conductance
    if (DoUseEuvCond) &
         call calc_euv_cond(NameHemiIn, SigmaHalEuv_II, SigmaPedEuv_II)

    ! Add aurora conductances.  Aurora models obtain/calculate average energy
    ! and energy flux for diffuse, discrete, and broadband precipitation
    ! (all default to zero). Each "case" clause should fill the appropriate
    ! conductance and precipitation arrays (either here or in subroutine calls)
    if (DoUseAurora) then
       ! Obtain average energy and energy flux values based on
       ! selected aurora model:
       select case(NameAuroraMod)
       case('RLM3', 'RLM4', 'RLM5', 'CMEE')!'FAC2FLUX')
          ! RLM-family of conductance models, including the
          ! Conductance Model for Extreme Events:
          ! RLM/CMEE fills the mono-energetic array ONLY
          call facs_to_fluxes(NameAuroraMod, NameHemiIn, &
               AvgEMono_II, EfluxMono_II)
          ! Convert average energy/energy flux into conductance:
          call flux_to_sigma(IONO_nTheta, IONO_nPsi, AvgEMono_II, &
               1000.*EFluxMono_II, SigmaHalMono_II, SigmaPedMono_II)

       case('MAGNIT', 'IMP')
           select case(NameAuroraMod)
           case('MAGNIT')
              ! MAGNIT sets precipitating fluxes.
              call magnit_gen_fluxes(NameHemiIn, &
                   AvgEDiffe_II, AvgEDiffi_II, AvgEMono_II, AvgEBbnd_II, &
                   EfluxDiffe_II, EfluxDiffi_II, EfluxMono_II, EfluxBbnd_II, &
                   theta)
           case ('IMP') ! New IM Precip Model
              if (IsImCoupled) call imp_gen_fluxes(NameHemiIn, &
                       AvgEDiffe_II, AvgEDiffi_II, AvgEMono_II, AvgEBbnd_II, &
                       EfluxDiffe_II, EfluxDiffi_II, EfluxMono_II, &
                       EfluxBbnd_II, theta)
           end select

           if(DoTest) then
              if (NameHemiIn == 'north') then
                 write(*,*)'Ion Energy Flux', &
                     MAXVAL(EfluxDiffi_II),MINVAL(EfluxDiffi_II)
                 write(*,*)'Ion Average Energy', &
                     MAXVAL(AvgEDiffi_II),MINVAL(AvgEDiffi_II)
                 write(*,*)'Electron Energy Flux', &
                     MAXVAL(EfluxDiffe_II),MINVAL(EfluxDiffe_II)
                 write(*,*)'Electron Average Energy', &
                     MAXVAL(AvgEDiffe_II),MINVAL(AvgEDiffe_II)
                 write(*,*)'Discrete Energy Flux', &
                     MAXVAL(EfluxMono_II),MINVAL(EfluxMono_II)
                 write(*,*)'Discrete Average Energy', &
                     MAXVAL(AvgEMono_II),MINVAL(AvgEMono_II)
                 write(*,*)'Broadband Energy Flux', &
                     MAXVAL(EfluxBbnd_II),MINVAL(EfluxBbnd_II)
                 write(*,*)'Broadband Average Energy', &
                     MAXVAL(AvgEBbnd_II),MINVAL(AvgEBbnd_II)
                 end if
              end if

          ! Convert fluxes to conductances:
          ! Diffuse flux is only calculated separately if the total electron
          ! flux stored in Mono is not calculated. Mono should likely be
          ! Renamed to Electron
          if (DoUseDiffE .and. .not. DoUseMono) then
              call flux_to_sigma(IONO_nTheta, IONO_nPsi, AvgEDiffe_II, &
                   1000.*EFluxDiffe_II, SigmaHalDiffe_II, SigmaPedDiffe_II, &
                      eCondRel)
          end if
          if (DoUseMono .and. DoUseDiffE) then
              call flux_to_sigma(IONO_nTheta, IONO_nPsi, AvgEMono_II, &
                   1000.*EFluxMono_II, SigmaHalMono_II, SigmaPedMono_II, &
                      eCondRel)
          end if
          if (DoUseDiffI) then
            call flux_to_sigma(IONO_nTheta, IONO_nPsi, AvgEDiffi_II, &
                   1000.*EFluxDiffi_II, SigmaHalDiffi_II, SigmaPedDiffi_II, &
                   iCondRel, theta)
          end if
          if (DoUseBbnd) then
              call flux_to_sigma(IONO_nTheta, IONO_nPsi, AvgEBbnd_II, &
                   1000.*EfluxBbnd_II, SigmaHalBbnd_II, SigmaPedBbnd_II, &
                      eCondRel)
          end if

          if(DoTest) then
              if (NameHemiIn == 'north') then
              write(*,*)'Ion Hall Conductance', MAXVAL(SigmaHalDiffi_II), &
                      MINVAL(SigmaHalDiffi_II)
              write(*,*)'Electron Hall Conductance', MAXVAL(SigmaHalDiffe_II), &
                      MINVAL(SigmaHalDiffe_II)
              write(*,*)'Discrete Hall Conductance', MAXVAL(SigmaHalMono_II),MINVAL(SigmaHalMono_II)
              write(*,*)'Broadband Hall Conductance', MAXVAL(SigmaHalBbnd_II),MINVAL(SigmaHalBbnd_II)
              end if
           end if
       case default
          call CON_stop(NameSub//': Unrecognized auroral model - ' &
               //NameAuroraMod)
       end select
    end if

    ! Sum conductance into the correct hemisphere.
    if(NameHemiIn == 'north')then
       if (IsUaCoupled .and. DoCoupleUaConductance) then
          IONO_NORTH_SigmaH = IONO_NORTH_UA_SigmaH
          IONO_NORTH_SigmaP = IONO_NORTH_UA_SigmaP
          if(DoTest) &
               write(*,*) "UA Conductance is being used in the northern "//&
                    "hemisphere"
       else
          IONO_NORTH_SigmaH = sqrt(SigmaHalConst**2 + SigmaHalEuv_II**2 + &
                (2*StarLightCond)**2 + SigmaHalMono_II**2 + SigmaHalDiffe_II**2 &
                + SigmaHalDiffi_II**2)
          IONO_NORTH_SigmaP = sqrt(SigmaPedConst**2 + SigmaPedEuv_II**2 + &
                StarLightCond**2 + SigmaPedMono_II**2 + SigmaPedDiffe_II**2 &
                + SigmaPedDiffi_II**2)
          ! Add broadband conductance:
          IONO_NORTH_SigmaH = IONO_NORTH_SigmaH + SigmaHalBbnd_II
          IONO_NORTH_SigmaP = IONO_NORTH_SigmaP + SigmaPedBbnd_II

          ! Combine with IPE conductance
          if(allocated(IONO_NORTH_SigmaH_IPE) .and. LatFullIpe > 0.0)then
            if(LatFullIpe >= 90.0)then
                ! Set all the conductances
                IONO_NORTH_SigmaH = IONO_NORTH_SigmaH_IPE
                IONO_NORTH_SigmaP = IONO_NORTH_SigmaP_IPE
            else
                do iTheta = 1, IONO_nTheta
                  Lat = 90.0 - IONO_NORTH_Theta(iTheta,1)*cRadToDeg
                  WeightRim = max(0.0, min(1.0, &
                        (Lat - LatFullIpe)/(LatFullRim - LatFullIpe)))
                  WeightIpe = 1 - WeightRim
                  IONO_NORTH_SigmaH(iTheta,:) = &
                        WeightRim*IONO_NORTH_SigmaH(iTheta,:) + &
                        WeightIpe*IONO_NORTH_SigmaH_IPE(iTheta,:)
                  IONO_NORTH_SigmaP(iTheta,:) = &
                        WeightRim*IONO_NORTH_SigmaP(iTheta,:) + &
                        WeightIpe*IONO_NORTH_SigmaP_IPE(iTheta,:)
                end do
            end if
            ! write(*,*) NameSub,' IPE NORTH SigmaH max,min=', &
            !     maxval(IONO_NORTH_SigmaH), minval(IONO_NORTH_SigmaH)
            ! write(*,*) NameSub,' IPE NORTH SigmaP max,min=', &
            !    maxval(IONO_NORTH_SigmaP), minval(IONO_NORTH_SigmaP)
          endif
       end if

       ! Store Average energy and energy flux:
       IONO_NORTH_EFlux = EfluxMono_II
       IONO_NORTH_Ave_E = AvgEMono_II
       IONO_NORTH_MONO_EFlux = EfluxMono_II
       IONO_NORTH_MONO_Ave_E = AvgEMono_II
       IONO_NORTH_DIFFI_EFlux = EfluxDiffi_II
       IONO_NORTH_DIFFI_Ave_E = AvgEDiffi_II
       IONO_NORTH_DIFFE_EFlux = EfluxDiffe_II
       IONO_NORTH_DIFFE_Ave_E = AvgEDiffe_II
       IONO_NORTH_BBND_EFlux = EfluxBbnd_II
       IONO_NORTH_BBND_Ave_E = AvgEBbnd_II
       ! Place values into convenience arrays to calculate derivatives:
       SigmaH = IONO_NORTH_SigmaH
       SigmaP = IONO_NORTH_SigmaP

    else
       if (IsUaCoupled) then
          IONO_SOUTH_SigmaH = IONO_SOUTH_UA_SigmaH
          IONO_SOUTH_SigmaP = IONO_SOUTH_UA_SigmaP
          if(DoTest) &
               write(*,*) "UA Conductance is being used in the southern "//&
                    "hemisphere"
       else
          IONO_SOUTH_SigmaH = sqrt(SigmaHalConst**2 + SigmaHalEuv_II**2 + &
                (2*StarLightCond)**2 + SigmaHalMono_II**2 + SigmaHalDiffe_II**2 &
                + SigmaHalDiffi_II**2)
          IONO_SOUTH_SigmaP = sqrt(SigmaPedConst**2 + SigmaPedEuv_II**2 + &
                StarLightCond**2 + SigmaPedMono_II**2 + SigmaPedDiffe_II**2 &
                + SigmaPedDiffi_II**2)
          ! Add broadband conductance:
          IONO_SOUTH_SigmaH = IONO_SOUTH_SigmaH + SigmaHalBbnd_II
          IONO_SOUTH_SigmaP = IONO_SOUTH_SigmaP + SigmaPedBbnd_II

          ! Combine with IPE conductance
          if(allocated(IONO_SOUTH_SigmaH_IPE) .and. LatFullIpe > 0.0)then
              if(LatFullIpe >= 90.0)then
                ! Set all the conductances
                IONO_SOUTH_SigmaH = IONO_SOUTH_SigmaH_IPE
                IONO_SOUTH_SigmaP = IONO_SOUTH_SigmaP_IPE
              else
                ! Transition between IPE and RIM conductances
                do iTheta = 1, IONO_nTheta
                    Lat = IONO_SOUTH_Theta(iTheta,1)*cRadToDeg - 90.0
                    WeightRim = max(0.0, min(1.0, &
                        (Lat - LatFullIpe)/(LatFullRim - LatFullIpe)))
                    WeightIpe = 1 - WeightRim
                    IONO_SOUTH_SigmaH(iTheta,:) = &
                        WeightRim*IONO_SOUTH_SigmaH(iTheta,:) + &
                        WeightIpe*IONO_SOUTH_SigmaH_IPE(iTheta,:)
                    IONO_SOUTH_SigmaP(iTheta,:) = &
                        WeightRim*IONO_SOUTH_SigmaP(iTheta,:) + &
                        WeightIpe*IONO_SOUTH_SigmaP_IPE(iTheta,:)
                end do
              end if
              ! write(*,*) NameSub,' !!! IPE SOUTH SigmaH max,min=', &
              !     maxval(IONO_SOUTH_SigmaH), minval(IONO_SOUTH_SigmaH)
              ! write(*,*) NameSub,' !!! IPE SOUTH SigmaP max,min=', &
              !     maxval(IONO_SOUTH_SigmaP), minval(IONO_SOUTH_SigmaP)
          endif
       end if

       ! Store Average energy and energy flux:
       IONO_SOUTH_EFlux = EfluxMono_II
       IONO_SOUTH_Ave_E = AvgEMono_II
       IONO_SOUTH_MONO_EFlux = EfluxMono_II
       IONO_SOUTH_MONO_Ave_E = AvgEMono_II
       IONO_SOUTH_DIFFI_EFlux = EfluxDiffi_II
       IONO_SOUTH_DIFFI_Ave_E = AvgEDiffi_II
       IONO_SOUTH_DIFFE_EFlux = EfluxDiffe_II
       IONO_SOUTH_DIFFE_Ave_E = AvgEDiffe_II
       IONO_SOUTH_BBND_EFlux = EfluxBbnd_II
       IONO_SOUTH_BBND_Ave_E = AvgEBbnd_II
       ! Place values into convenience arrays to calculate derivatives:
       SigmaH = IONO_SOUTH_SigmaH
       SigmaP = IONO_SOUTH_SigmaP
    end if

    ! Calculate of-diagonal conductance values.
    ! Start with some "helper" values:
    sn = sin(Theta)
    cs = cos(Theta)
    sn2= sn**2
    cs2 = cs**2
    cs3 = 1.00 + 3.00*cs2
    cs4 = sqrt(cs3)
    C = 4.00*SigmaPar*cs2 + SigmaP*sn2

    ! Off-diagonal vals:
    SigmaThTh = SigmaPar*SigmaP*cs3/C
    SigmaThPs = 2.00*SigmaPar*SigmaH*cs*cs4/C
    SigmaPsPs = SigmaP + SigmaH*SigmaH*sn2/C

    ! Intialize derivatives to zero
    dSigmaThTh_dTheta = 0.00
    dSigmaThTh_dPsi   = 0.00
    dSigmaThPs_dTheta = 0.00
    dSigmaThPs_dPsi   = 0.00
    dSigmaPsPs_dTheta = 0.00
    dSigmaPsPs_dPsi   = 0.00

    ! Spatial derivatives of conductance.  Note that dTheta and dPsi
    ! are defined to be the separation over two grid cells, so all
    ! derivatives are central difference formula correct.
    ! Poles and equator are excluded from this calculation.
    ! d/dTheta derivatives:
    do j=1, nPsi
       dSigmaThTh_dTheta(2:nTheta-1,j) = &
            (SigmaThTh(3:nTheta,j) - SigmaThTh(1:nTheta-2,j)) / dTheta(2:nTheta-1)
       dSigmaThPs_dTheta(2:nTheta-1,j) = &
            (SigmaThPs(3:nTheta,j) - SigmaThPs(1:nTheta-2,j)) / dTheta(2:nTheta-1)
       dSigmaPsPs_dTheta(2:nTheta-1,j) = &
            (SigmaPsPs(3:nTheta,j) - SigmaPsPs(1:nTheta-2,j)) / dTheta(2:nTheta-1)
    end do

    ! d/dPsi derivatives:
    do i=2, nTheta-1
       dSigmaThTh_dPsi(i,2:nPsi-1) = &
            (SigmaThTh(i,3:nPsi)-SigmaThTh(i,1:nPsi-2)) &
            / dPsi(2:nPsi-1)
       dSigmaThPs_dPsi(i,2:nPsi-1) = &
            (SigmaThPs(i,3:nPsi)-SigmaThPs(i,1:nPsi-2)) &
            / dPsi(2:nPsi-1)
       dSigmaPsPs_dPsi(i,2:nPsi-1) = &
            (SigmaPsPs(i,3:nPsi)-SigmaPsPs(i,1:nPsi-2)) &
            / dPsi(2:nPsi-1)
    end do
    ! Longitude edge cases (compensating for ghost cell) j=1 and j=nPsi:
    dSigmaThTh_dPsi(2:nTheta-1,1) = &
         (SigmaThTh(2:nTheta-1,2)-SigmaThTh(2:nTheta-1,nPsi-1)) / dPsi(1)
    dSigmaThPs_dPsi(2:nTheta-1,1) = &
         (SigmaThPs(2:nTheta-1,2)-SigmaThPs(2:nTheta-1,nPsi-1)) / dPsi(1)
    dSigmaPsPs_dPsi(2:nTheta-1,1) = &
         (SigmaPsPs(2:nTheta-1,2)-SigmaPsPs(2:nTheta-1,nPsi-1)) / dPsi(1)

    dSigmaThTh_dPsi(2:nTheta-1,nPsi) = &
         (SigmaThTh(2:nTheta-1,2)-SigmaThTh(2:nTheta-1,nPsi-1)) / dPsi(nPsi)
    dSigmaThPs_dPsi(2:nTheta-1,nPsi) = &
         (SigmaThPs(2:nTheta-1,2)-SigmaThPs(2:nTheta-1,nPsi-1)) / dPsi(nPsi)
    dSigmaPsPs_dPsi(2:nTheta-1,nPsi) = &
         (SigmaPsPs(2:nTheta-1,2)-SigmaPsPs(2:nTheta-1,nPsi-1)) / dPsi(nPsi)

    ! Place derivative results into correct hemisphere:
    if(NameHemiIn == 'north')then
       ! Off-diagonal terms:
       IONO_NORTH_SigmaThTh = SigmaThTh
       IONO_NORTH_SigmaThPs = SigmaThPs
       IONO_NORTH_SigmaPsPs = SigmaPsPs
       ! Spatial derivative terms:
       IONO_NORTH_dSigmaThTh_dTheta = dSigmaThTh_dTheta
       IONO_NORTH_dSigmaThPs_dTheta = dSigmaThPs_dTheta
       IONO_NORTH_dSigmaPsPs_dTheta = dSigmaPsPs_dTheta
       IONO_NORTH_dSigmaThTh_dPsi = dSigmaThTh_dPsi
       IONO_NORTH_dSigmaThPs_dPsi = dSigmaThPs_dPsi
       IONO_NORTH_dSigmaPsPs_dPsi = dSigmaPsPs_dPsi
    else
       ! Off-diagonal terms:
       IONO_SOUTH_SigmaThTh = SigmaThTh
       IONO_SOUTH_SigmaThPs = SigmaThPs
       IONO_SOUTH_SigmaPsPs = SigmaPsPs
       ! Spatial derivative terms:
       IONO_SOUTH_dSigmaThTh_dTheta = dSigmaThTh_dTheta
       IONO_SOUTH_dSigmaThPs_dTheta = dSigmaThPs_dTheta
       IONO_SOUTH_dSigmaPsPs_dTheta = dSigmaPsPs_dTheta
       IONO_SOUTH_dSigmaThTh_dPsi = dSigmaThTh_dPsi
       IONO_SOUTH_dSigmaThPs_dPsi = dSigmaThPs_dPsi
       IONO_SOUTH_dSigmaPsPs_dPsi = dSigmaPsPs_dPsi

    end if

  end subroutine generate_conductance
  !============================================================================
  subroutine calc_euv_cond(NameHemiIn, CondHalOut_II, CondPedOut_II)

    ! For a given hemisphere, calculate the EUV-driven conductance using
    ! Moen & Brekke, 1992.  Add scattering across the terminator as from
    ! Rasmussen and Schunk model B (JGR, Vol. 92, pp. 4491-4504, 1987).
    ! The two approaches are blended at a set solar zenith angle (blendAngle).
    ! Following Ridley_serial's parallelization via hemisphere, act only on 1
    ! hemisphere at a time.
    ! Starlight conductance is included in this calculation.

    ! use IE_ModSize
    use ModNumConst, ONLY: cDegToRad
    use IE_ModMain, ONLY: CosThetaTilt, SinThetaTilt

    character(len=*), intent(in) :: NameHemiIn
    real, intent(out), dimension(IONO_nTheta,IONO_nPsi) :: &
         CondHalOut_II, CondPedOut_II

    real, parameter :: cBlendAngle = 70.0*cDegToRad
    real :: f107p53, f107p49, cos_limit, meeting_value_p, meeting_value_h
    real, dimension(IONO_nTheta,IONO_nPsi) :: cos_SZA, X_II, Y_II, Z_II, &
         SigmaH_EUV, SigmaH_SCAT, SigmaP_EUV, SigmaP_SCAT

    logical :: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'calc_euv_cond'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    ! Grab coordinates/locations based on hemisphere.
    if(NameHemiIn == 'north') then
       X_II = IONO_NORTH_X
       Y_II = IONO_NORTH_Y
       Z_II = IONO_NORTH_Z
    else if (NameHemiIn == 'south') then
       X_II = IONO_SOUTH_X
       Y_II = IONO_SOUTH_Y
       Z_II = IONO_SOUTH_Z
    else
       call CON_stop(NameSub//': Unrecognized hemisphere = '//NameHemiIn)
    endif

    ! Calculate the cosine of the solar zenith angle:
    cos_SZA = (X_II*CosThetaTilt - Z_II*SinThetaTilt) &
         / sqrt(X_II**2 + Y_II**2 + Z_II**2)

    ! We are going to need F10.7 ^ 0.53 and F10.7 ^ 0.49 a lot,
    ! So, let's just store them straight away:
    f107p53 = f107_flux**0.53
    f107p49 = f107_flux**0.49

    ! Point at which to merge two functions:
    cos_limit = cos(cBlendAngle)

    ! "meeting values" are M&B at SZA=blendAngle degrees.
    meeting_value_p = f107p49*(0.34*cos_limit+0.93*sqrt(cos_limit))
    meeting_value_h = f107p53*(0.81*cos_limit+0.54*sqrt(cos_limit))

    dayside: where (cos_SZA > 0)
       SigmaH_EUV=f107p53*(0.81*cos_SZA+0.54*sqrt(cos_SZA)) ! M & B.
       SigmaP_EUV=f107p49*(0.34*cos_SZA+0.93*sqrt(cos_SZA))
       SigmaH_SCAT = 1.00 ! Scattered directly from R & S
       SigmaP_SCAT = 0.50
       blend: where (cos_SZA < cos_limit)
          SigmaH_EUV = (SigmaH_EUV + &
               meeting_value_h * exp(-((cos_SZA-cos_limit)**2.0)*15.0))/2.0
          SigmaP_EUV = (SigmaP_EUV + &
               meeting_value_p * exp(-((cos_SZA-cos_limit)**2.0)*15.0))/2.0
       endwhere blend
    elsewhere ! nightside:
       SigmaH_EUV = meeting_value_h * exp(-((cos_SZA-cos_limit)**2.0)*15.0)
       SigmaP_EUV = meeting_value_p * exp(-((cos_SZA-cos_limit)**2.0)*15.0)
       SigmaH_SCAT = 1.00*(10.00**cos_SZA) ! Follows R & S
       SigmaP_SCAT = 0.50*(10.00**cos_SZA) ! Follows R & S
    endwhere dayside

    ! Sum the EUV and scattering conductances together:
    CondHalOut_II = sqrt(SigmaH_EUV**2 + SigmaH_SCAT**2)
    CondPedOut_II = sqrt(SigmaP_EUV**2 + SigmaP_SCAT**2)

  end subroutine calc_euv_cond
  !============================================================================
  subroutine smooth_lagrange_polar(a_II, iLatSize, jLonSize, tolIn)

    ! Use a simple sliding window technique to smooth a 2D array in polar
    ! coordinates such that periodicity and continuity are enforced across
    ! the LON=360/0 boundary. The max and min latitude points are not
    ! considered.  Only points that are more than *tol* percent greater than
    ! the average of their neighbors are changed.
    !
    ! Arguments
    !
    ! a_II(iLatSize,jLonSize): A two-dimentional array to smooth in place.
    ! iLatSize, jLonSize     : The size of a_II in lat/lon space

    integer, intent(in)        :: iLatSize, jLonSize
    real, intent(inout)        :: a_II(iLatSize, jLonSize)
    real, intent(in), optional :: tolIn

    integer                    :: i, j
    real                       :: aSmooth_II(iLatSize, jLonSize), tol

    ! Set default tolerance to 15% of original value:
    !--------------------------------------------------------------------------
    if (present(tolIn)) then
       tol = tolIn
    else
       tol=0.15
    endif

    ! Construct smoothed values
    ! No smoothing at poles:
    aSmooth_II(1,:) = a_II(1,:)
    aSmooth_II(iLatSize,:) = a_II(iLatSize,:)
    ! Smooth all non-pole points:
    colat: do i = 2, iLatSize-1
       ! Perform averaging at longitude boundaries (azimuthal periodicity)
       aSmooth_II(i,1) = (                  & ! At lon=0 degrees
            sum(a_II(i-1:i+1, jLonSize)) +  &
            sum(a_II(i-1:i+1, 2))        +  &
            a_II(i-1,1) + a_II(i+1,1)       &
            ) / 8.0

       aSmooth_II(i,jLonSize) = (              & ! At lon=360 degrees
            sum(a_II(i-1:i+1, jLonSize -1)) +  &
            sum(a_II(i-1:i+1, 1))           +  &
            a_II(i-1,jLonSize)              +  &
            a_II(i+1,jLonSize) ) / 8.0

       ! Smooth over all non-longitude boundary points:
       lon: do j = 2, jLonSize-1
          aSmooth_II(i,j) = (             & ! For all other (i,j)
               sum(a_II(i-1:i+1, j-1)) +  & ! left of point i,j
               sum(a_II(i-1:i+1, j+1)) +  & ! right of point i,j
               a_II(i+1,j) + a_II(i-1,j)  & ! above and below
               )/ 8.0
       end do lon

    end do colat

    ! Apply smoothing to discrete_nf only where smoothing changes values
    ! by tolerance percentage *tol* or more from their original value:
    where (a_II<=ABS(aSmooth_II - a_II) /tol) a_II = aSmooth_II

  end subroutine smooth_lagrange_polar
  !============================================================================
  subroutine imodel_legacy(iModelIn, f10In, StarLightIn, PolarCapIn)

    ! Create mapping between new conductance-related PARAMs and legacy
    ! #IONOSPHERE param with the "imodel" parameter.  This maintains
    ! backwards compatability.

    integer, intent(in) :: iModelIn
    real, intent(in) :: f10In, StarLightIn, PolarCapIn

    logical :: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'imodel_legacy'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    if(DoTestMe) then
       write(*,*)NameSub // ' Debug Information:'
       write(*,*)'   iModelIn = ', iModelIn
       write(*,*)'   StarLightIn, PolarCapIn =', StarLightIn, PolarCapIn
    end if

    select case(iModelIn)
       case(0) ! Constant conductance, Pedersen only; no background
          SigmaPedConst = StarLightIn
          SigmaHalConst = 0
          StarLightCond  = 0.0
          PolarCapPedCond= 0.0
          DoUseEuvCond = .false.
          DoUseAurora  = .false.
       case(1) ! Constant conductance; no background
          SigmaHalConst  = PolarCapIn
          SigmaPedConst  = StarLightIn
          StarLightCond  = 0.0
          PolarCapPedCond= 0.0
          DoUseEuvCond = .false.
          DoUseAurora  = .false.
       case(2) ! Constant conductance + dayside conductance:
          ! Note that in the legacy model, StarLightPedConductance is
          ! used to set constant hall and ped; hall is multiplied by 2.
          StarLightCond  = 0.0
          PolarCapPedCond= 0.0
          SigmaHalConst = StarLightIn * 2.0
          SigmaPedConst = StarLightIn
          DoUseEuvCond = .true.
          f107_flux = f10In
       case(3)
          DoUseAurora     = .true.
          NameAuroraMod   = 'RLM3'
          DoUseEuvCond    = .true.
          StarLightCond   = StarLightIn
          PolarCapPedCond = PolarCapIn
          f107_flux       = f10In
       case(4)
          DoUseAurora     = .true.
          NameAuroraMod   = 'RLM4'
          DoUseEuvCond    = .true.
          StarLightCond   = StarLightIn
          PolarCapPedCond = PolarCapIn
          f107_flux       = f10In
       case(5)
          DoUseAurora     = .true.
          NameAuroraMod   = 'RLM5'
          DoUseEuvCond    = .true.
          StarLightCond   = StarLightIn
          PolarCapPedCond = PolarCapIn
          f107_flux       = f10In
       case(6) ! Legacy conductance from MHD; DOES NOT FUNCTION.
          call CON_stop(NameSub//': iModel 6 no longer available.')
       case(7) ! Legacy conductance from RCM; DOES NOT FUNCTION.
          call CON_stop(NameSub//': iModel 7 no longer available.')
       case(8)
       case default
          write(*,*) NameSub//': iModel = ', iModelIn
          call CON_stop(NameSub//': Invalid imodel value')
    end select

    if(DoTestMe) then
       write(*,*)'   SigmaHalConst, SigmaPedConst = ', &
            SigmaHalConst, SigmaPedConst
       write(*,*)'   StarLightCond, PolarCapPedCond = ', &
            StarLightCond, PolarCapPedCond
       write(*,*)'   F10.7 Flux = ', f107_flux
       write(*,*)'   DoUseEuvCond = ', DoUseEuvCond
       write(*,*)'   DoUseAurora = ', DoUseAurora
       write(*,*)'   NameAuroraModel = ', NameAuroraMod
       write(*,*)'   iModelIn = ', iModelIn
    end if

  end subroutine imodel_legacy
  !============================================================================
  subroutine flux_to_sigma(nLatIn, nLonIn, AveEIn_II, eFluxIn_II, &
       SigmaHOut_II, SigmaPOut_II, NameModelIn, LatIn_II)

    ! Convert average energy and energy flux to conductance using one of
    ! several empirical relationships. Both average energy and energy flux
    ! should be given in units of keV and mW/m2 (ergs/cm2), respectively,
    ! on 2D lat/lon grids.
    !
    !    Empirical models implemented:
    !    ----------------------------------
    !    robi :: Robinson et al., 1987 (electrons)
    !    gala :: Galand and Richmond, 2001 (ions)
    !    kaep :: Kaeppler et al., 2015 (electrons)

    use ModPlanetConst, ONLY: rPlanet_I, IonoHeightPlanet_I, &
            DipoleStrengthPlanet_I, Earth_
    ! Arguments:
    integer, intent(in) :: nLatIn, nLonIn
    real, intent(in),  dimension(nLatIn, nLonIn) :: AveEIn_II, eFluxIn_II
    real, intent(out), dimension(nLatIn, nLonIn) :: SigmaHOut_II, SigmaPOut_II
    character(len=4), intent(in), optional :: NameModelIn
    real, dimension(IONO_nTheta,IONO_nPsi), intent(in), optional :: LatIn_II

    ! Local variables:
    character(len=4) :: NameModel
    real, dimension(IONO_nTheta,IONO_nPsi):: BDipole_II=0, cond_Eflux_II=0, &
            cond_AvgE_II=0

    logical :: DoTest, DoTestMe
    ! Set up defaults: ions and Robinson et al. 1987
    character(len=*), parameter:: NameSub = 'flux_to_sigma'
    !--------------------------------------------------------------------------
    if (present(NameModelIn)) then
        NameModel = NameModelIn
    else
        NameModel = 'robi'
    end if

    SigmaPOut_II = 0
    SigmaHOut_II = 0
    cond_Eflux_II = eFluxIn_II
    cond_AvgE_II = AveEIn_II

    if (UsePrecipSmoothing) then
        call polar_convolution(cond_Eflux_II, IONO_nTheta, IONO_nPsi)
        call polar_convolution(cond_AvgE_II, IONO_nTheta, IONO_nPsi)
    end if

    select case(NameModel)
    case('robi')
       SigmaPOut_II = sqrt(cond_Eflux_II) * (40. * cond_AvgE_II) &
           / (16. + cond_AvgE_II**2)
       SigmaHOut_II = 0.45 * SigmaPOut_II * cond_AvgE_II**0.85

       ! Robinson formulas are only valid for 2-40 keV range,
       ! we limit as such and scale down results at higher energy values
       where(cond_AvgE_II > eCondLimit)
           SigmaPOut_II = SigmaPOut_II * EXP((eCondLimit - cond_AvgE_II) &
                   /eLimitScale)
           SigmaHOut_II = SigmaHOut_II * EXP((eCondLimit - cond_AvgE_II) &
                   /eLimitScale)
       end where
    case('gala')
        BDipole_II = -DipoleStrengthPlanet_I(Earth_) * &
                (rPlanet_I(Earth_)/(rPlanet_I(Earth_) + &
                IonoHeightPlanet_I(Earth_)))**3 &
                * sqrt(1 + 3*(sin(LatIn_II)**2))
        SigmaPOut_II = 5.7 * sqrt(cond_Eflux_II) * &
                (BDipole_II / 54e-6)**(-1.45)
        SigmaHOut_II = 2.6 * cond_AvgE_II**0.3 * sqrt(cond_Eflux_II) &
            * (BDipole_II / 54e-6)**(-1.90)
    case('kaep')
        SigmaPOut_II = sqrt(cond_Eflux_II) * (40. * cond_AvgE_II) &
                / (16. + AveEIn_II**2)
        SigmaHOut_II = 0.57 * SigmaPOut_II * cond_AvgE_II**0.53
    end select

   end subroutine flux_to_sigma
  !============================================================================
  subroutine polar_convolution(a_II, nLat, nLon)

    ! Use a Gaussian convolution to smooth a 2D array in polar coordinates
    ! such that periodicity and continuity are enforced across the LON=360/0
    ! boundary.

    use ModConst, ONLY: cPi

    integer, intent(in)        :: nLat, nLon
    real, intent(inout), dimension(nLat,nLon) :: &
            a_II

    integer :: i, j
    real :: sigma
    real, dimension(nLat,nLon + 2 * KernelSize)  :: Sample_II
    real, dimension(1 + 2 * KernelSize, 1 + 2 * KernelSize) :: &
            kernel_II

    logical :: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'polar_convolution'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    Sample_II(:, 1 + KernelSize:nLon + KernelSize) = a_II
    Sample_II(:, 1:KernelSize) = a_II(:, nLon - KernelSize + 1:nLon)
    Sample_II(:, nLon + KernelSize + 1:nLon + 2 * KernelSize) = &
            a_II(:, 1:KernelSize)

    select case(KernelType)
        case('gaus')
            sigma = KernelSpread
            do i = 1, 1 + 2 * KernelSize
                do j = 1, 1 + 2 * KernelSize
                    kernel_II(i, j) = 1 / (2 * cPi * sigma**2) * &
                            exp(-((i - KernelSize - 1)**2 + &
                                    (j - KernelSize - 1)**2) / (2 * sigma**2))
                end do
            end do
    case('bxcr')
            kernel_II = 1.0
    end select

    kernel_II = kernel_II / sum(kernel_II)

    if (DoTest) write(*,*)'kernel_II = ', kernel_II

    a_II = 0.0
    do i = 1, nLat - 2 * KernelSize
        do j = 1, nLon
            a_II(i + KernelSize, j) = sum(kernel_II * &
                    Sample_II(i:i + 2 * KernelSize, j:j + 2 * KernelSize))
        end do
    end do

  end subroutine polar_convolution
  !============================================================================
end module ModConductance
!==============================================================================
