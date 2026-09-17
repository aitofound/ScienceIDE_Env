subroutine FACs_to_fluxes(NameModelIn, NameHemiIn, AveEOut_II, EFluxOut_II)

  ! The goal here is to convert the ionospheric FAC pattern into a
  ! particle precipitation pattern, which can then be turned into
  ! a conductance pattern.
  ! Input: hemisphere
  ! Output: Average energy and energy flux across NameHemiIn

  use ModNumConst
  use ModConst, ONLY: cEVToK, cElectronCharge
  use ModIonosphere
  use IE_ModMain
  use ModConductance, ONLY: IONO_Min_EFlux, IONO_Min_Ave_E, PolarCapPedCond
  use ModIeRlm

  implicit none

  character(len=*), intent(in) :: NameModelIn, NameHemiIn
  real, dimension(IONO_nTheta, IONO_nPsi), intent(out) :: &
       AveEOut_II, EFluxOut_II

  ! Parameters
  real, parameter :: Hall_to_Ped_Ratio = 1.5

  ! Local variables for hemisphere-specific information:
  real, dimension(IONO_nTheta, IONO_nPsi) :: &
       IonoJrNow, IonoTheta, IonoPsi

  ! Physical variables used by all calculations:
  real :: PolarCap_AveE, PolarCap_EFlux

  ! Variables for auroral oval related calculations:
  real :: distance
  real, dimension(1:IONO_nPsi) :: Strength_of_Oval,               &
       Loc_of_Oval, Width_of_Oval
  logical :: polarcap

  ! RLM3 parameters:
  real :: mult_fac, oval_shift, ave_e_peak, ave_e_peak_lon

  ! Values for RLM4/5 and CMEE calculations:
  real :: dlat, dmlt

  real    :: aurora_peak_lat, eflux_peak_lon
  real    :: aurora_peak_width
  real    :: ef, efp, mfc
  real    :: hal_a0, hal_a1, hal_a2, ped_a0, ped_a1, ped_a2, hall, ped
  real    :: y1, y2, x1, x2
  real    :: day_colat, dusk_colat, midnight_colat, dawn_colat
  real    :: day_fac, dusk_fac, midnight_fac, dawn_fac
  real    :: noon_mid, dusk_dawn
  real    :: mean_colat, dev_colat, night_width
  integer :: jlat, imlt
  integer :: i,j, n, nloc, nHalfSmooth

  logical :: IsPeakFound, IsDone
  real :: Center, Width
  real :: f, MaxP, MaxT, MulFac_ae, MulFac_ef, MinWidth, ThetaOCB, AuroraWidth

  ! Debugging values:
  logical :: DoTest, DoTestMe
  character(len=*), parameter:: NameSub = 'FACs_to_fluxes'
  !----------------------------------------------------------------------------
  call CON_set_do_test(NameSub, DoTest, DoTestMe)

  ! Handle hemisphere information to reduce duplication:
  if(NameHemiIn == 'north')then
     IonoJrNow = IONO_NORTH_JR
     IonoTheta = IONO_NORTH_Theta
     IonoPsi   = IONO_NORTH_Psi
  else if (NameHemiIn == 'south')then
     IonoJrNow = IONO_SOUTH_JR
     IonoTheta = IONO_SOUTH_Theta
     IonoPsi   = IONO_SOUTH_Psi
  else
     call CON_stop(NameSub//' : unrecognized hemisphere - '//NameHemiIn)
  end if

  ! Set minimum background average energy/energy flux
  ! using PolarCapPedCond parameter set in #BACKGROUND.
  if (PolarCapPedCond > 0.0) then
     ! PolarCapHallConductance = Hall_to_Ped_Ratio * PolarCapPedCond
     PolarCap_AveE = (Hall_to_Ped_Ratio/0.45)**(1.0/0.85)
     PolarCap_EFlux = ((PolarCapPedCond*(16.0 + PolarCap_AveE**2) / &
          (40.0*PolarCap_AveE))**2)/1000.0
  else
     PolarCap_AveE  = IONO_Min_Ave_E
     PolarCap_EFlux = IONO_Min_EFlux
  endif

  ! Get location and "strength" of auroral oval by fitting a curve
  ! to the peak J_upward as a function of local time:
  if(UseNewOval)then
     call Create_Auroral_Oval(IonoJrNow, IonoTheta, IonoPsi, &
          Loc_of_Oval, Width_of_Oval,Strength_of_Oval)
  else
     call Determine_Oval_Characteristics(IonoJrNow, IonoTheta, IonoPsi, &
          Loc_of_Oval, Width_of_Oval,Strength_of_Oval)
  end if

  if(NameHemiIn == 'south') Loc_of_Oval = cPi - Loc_of_Oval

  select case(NameModelIn)
  case('RLM3')
     ! Parameterize oval values:
     mult_fac   = 0.25*0.33e7*2.0e-3
     ave_e_peak = max(3.0, PolarCap_AveE)
     oval_shift = 10.0*cDegToRad
     ave_e_peak_lon = 0.0

     ! Set auroral values based on strength and relative
     ! position to approximated oval:
     do i = 1, IONO_nTheta
        do j = 1, IONO_nPsi

           ! Energy Flux calculation:
           distance = IonoTheta(i,j)-(Loc_of_Oval(j)+oval_shift)
           EfluxOut_II(i,j) =                                       &
                Strength_of_Oval(j) * &
                mult_fac * &
                exp(-1.0*(distance/Width_of_Oval(j))**2)

           ! Set lower limit of conductance based on position
           if (distance < 0.0) then ! Poleward of auroral oval
              EfluxOut_II(i,j) = max(EfluxOut_II(i,j), PolarCap_EFlux)
              polarcap = .true.
           else                     ! Equatorward of auroral oval
              EfluxOut_II(i,j) = max(EfluxOut_II(i,j), IONO_Min_EFlux)
              polarcap = .false.
           endif

           ! Average Energy
           AveEOut_II(i,j) = &
                ave_e_peak*exp(-1.0*(distance/Width_of_Oval(j))**2)

           ! Set longitude scaling for average energy.
           if (PolarCap_AveE == IONO_Min_Ave_E) then
              ! Scale based on longitude, peak on ave_e_peak_lon:
              distance = 0.25 +                                 &
                   0.75*(                                       &
                   sin(IonoPsi(i,j)-(ave_e_peak_lon + cHalfPi)) &
                   + 1.0)/2.0
           else
              ! Don't scale with longitude:
              distance = 1.0
           endif

           ! Apply longitude scaling:
           AveEOut_II(i,j) = AveEOut_II(i,j)*distance

           ! Set lower limit of conductance based on position
           if (polarcap) then ! Inside polar cap:
              AveEOut_II(i,j) = &
                   max(AveEOut_II(i,j), PolarCap_AveE)
           else               ! Sub-auroral latitudes:
              AveEOut_II(i,j) = &
                   max(AveEOut_II(i,j), IONO_Min_Ave_E)
           endif

        enddo
     enddo

  case('RLM4','RLM5','CMEE')
     ! Start by intepolating conductance coefficients to IE grid.
     ! Calculate grid spacing for conductance grid
     dlat = (cond_lats(1) - cond_lats(2))*cDegToRad
     dmlt = (cond_mlts(2) - cond_mlts(1))*cPi/12.0

     ! Interpolate conductance coefficients to ionospheric grid.
     LON: do j = 1, IONO_nPsi
        LAT: do i = 1, IONO_nTheta
           ! Set lon/colat index accounting for correct hemisphere
           x1 = mod((IonoPsi(i,j) + cPi), cTwoPi)/dmlt + 1.0
           if(NameHemiIn == 'north')then
              y1 = IonoTheta(i,j)/dlat + 1.0
           else
              y1 = (cPi - IonoTheta(i,j))/dlat + 1.0
           end if

           if (y1 > i_cond_nlats-1) then
              jlat = i_cond_nlats-1
              y1   = 1.0
           else
              jlat = y1
              y1   = 1.0 - (y1 - jlat)
           endif

           y2 = 1.0 - y1
           imlt = x1
           x1   = 1.0 - (x1 - imlt)
           x2   = 1.0 - x1

           ! Perform interpolation
           if (IonoJrNow(i,j) > 0) then

              hal_a0 = x1*y1*hal_a0_up(imlt  ,jlat  ) + &
                   x2*y1*hal_a0_up(imlt+1,jlat  ) + &
                   x1*y2*hal_a0_up(imlt  ,jlat+1) + &
                   x2*y2*hal_a0_up(imlt+1,jlat+1)

              hal_a1 = x1*y1*hal_a1_up(imlt  ,jlat  ) + &
                   x2*y1*hal_a1_up(imlt+1,jlat  ) + &
                   x1*y2*hal_a1_up(imlt  ,jlat+1) + &
                   x2*y2*hal_a1_up(imlt+1,jlat+1)

              hal_a2 = x1*y1*hal_a2_up(imlt  ,jlat  ) + &
                   x2*y1*hal_a2_up(imlt+1,jlat  ) + &
                   x1*y2*hal_a2_up(imlt  ,jlat+1) + &
                   x2*y2*hal_a2_up(imlt+1,jlat+1)

              ped_a0 = x1*y1*ped_a0_up(imlt  ,jlat  ) + &
                   x2*y1*ped_a0_up(imlt+1,jlat  ) + &
                   x1*y2*ped_a0_up(imlt  ,jlat+1) + &
                   x2*y2*ped_a0_up(imlt+1,jlat+1)

              ped_a1 = x1*y1*ped_a1_up(imlt  ,jlat  ) + &
                   x2*y1*ped_a1_up(imlt+1,jlat  ) + &
                   x1*y2*ped_a1_up(imlt  ,jlat+1) + &
                   x2*y2*ped_a1_up(imlt+1,jlat+1)

              ped_a2 = x1*y1*ped_a2_up(imlt  ,jlat  ) + &
                   x2*y1*ped_a2_up(imlt+1,jlat  ) + &
                   x1*y2*ped_a2_up(imlt  ,jlat+1) + &
                   x2*y2*ped_a2_up(imlt+1,jlat+1)

           else

              hal_a0 = x1*y1*hal_a0_do(imlt  ,jlat  ) + &
                   x2*y1*hal_a0_do(imlt+1,jlat  ) + &
                   x1*y2*hal_a0_do(imlt  ,jlat+1) + &
                   x2*y2*hal_a0_do(imlt+1,jlat+1)

              hal_a1 = x1*y1*hal_a1_do(imlt  ,jlat  ) + &
                   x2*y1*hal_a1_do(imlt+1,jlat  ) + &
                   x1*y2*hal_a1_do(imlt  ,jlat+1) + &
                   x2*y2*hal_a1_do(imlt+1,jlat+1)

              hal_a2 = x1*y1*hal_a2_do(imlt  ,jlat  ) + &
                   x2*y1*hal_a2_do(imlt+1,jlat  ) + &
                   x1*y2*hal_a2_do(imlt  ,jlat+1) + &
                   x2*y2*hal_a2_do(imlt+1,jlat+1)

              ped_a0 = x1*y1*ped_a0_do(imlt  ,jlat  ) + &
                   x2*y1*ped_a0_do(imlt+1,jlat  ) + &
                   x1*y2*ped_a0_do(imlt  ,jlat+1) + &
                   x2*y2*ped_a0_do(imlt+1,jlat+1)

              ped_a1 = x1*y1*ped_a1_do(imlt  ,jlat  ) + &
                   x2*y1*ped_a1_do(imlt+1,jlat  ) + &
                   x1*y2*ped_a1_do(imlt  ,jlat+1) + &
                   x2*y2*ped_a1_do(imlt+1,jlat+1)

              ped_a2 = x1*y1*ped_a2_do(imlt  ,jlat  ) + &
                   x2*y1*ped_a2_do(imlt+1,jlat  ) + &
                   x1*y2*ped_a2_do(imlt  ,jlat+1) + &
                   x2*y2*ped_a2_do(imlt+1,jlat+1)

           endif

           ! Use "distance" to turn oval off/on (see above comment).
           if(UseOval)then
              if(NameHemiIn == 'north')then
                 distance = IonoTheta(i,j) - Loc_of_Oval(j)
              else
                 distance = Loc_of_Oval(j) - IonoTheta(i,j)
              endif
           else
              distance = 1E9
           end if

           ! Use distance to determine if inside polar cap
           ! (above auroral oval) or not (sub-auroral colat)
           if (distance > 0.0) then
              polarcap = .false.
           else
              polarcap = .true.
           endif

           if (NameModelIn == 'RLM4') then
              ! Implemented Feb.7, 2007 as modified version of iModel 5 with
              ! new narrower fitting of the auroral oval.  DDZ

              hall=exp(-1.0*(distance/(OvalWidthFactor*Width_of_Oval(j)))**2) * &
                   CondFactor*( &
                   hal_a0+(hal_a1-hal_a0)*exp(-abs(IonoJrNow(i,j)*1.0e9)*hal_a2**2))
              ped =exp(-1.0*(distance/(OvalWidthFactor*Width_of_Oval(j)))**2) * &
                   CondFactor*( &
                   ped_a0+(ped_a1-ped_a0)*exp(-abs(IonoJrNow(i,j)*1.0e9)*ped_a2**2))
           else  ! RLM5 or CMEE
              ! Restrict FAC-related conductance outside auroral oval.
              if (.not.polarcap .and. .not.UseSubOvalCond) then
                 distance = distance/3.0
                 hal_a0 = hal_a0 * exp(-(distance/(OvalWidthFactor*Width_of_Oval(j)))**2)
                 ped_a0 = ped_a0 * exp(-(distance/(OvalWidthFactor*Width_of_Oval(j)))**2)
              endif

              ! No Conductance Below 45 degrees
              if (NameModelIn == 'CMEE') then
                 ! Switch colat->lat, check against boundary latitude:
                 if (abs(90-IonoTheta(i,j)*cRadToDeg) <= LatNoConductanceSI) then
                    hal_a1 = hal_a0
                    ped_a1 = ped_a0
                 end if
              end if

              ! Get A1 coefficients
              hal_a1 = hal_a0 + (OvalStrengthFactor*hal_a1 - hal_a0)*  &
                   exp(-1.0*(distance/(OvalWidthFactor*Width_of_Oval(j)))**2)
              ped_a1 = ped_a0 + (OvalStrengthFactor*ped_a1 - ped_a0)*  &
                   exp(-1.0*(distance/(OvalWidthFactor*Width_of_Oval(j)))**2)

              ! Enforce A1 < A0 (otherwise, negative conductance)
              if (NameModelIn == 'CMEE') then
                 hal_a1 = min(hal_a0, hal_a1)
                 ped_a1 = min(ped_a0, ped_a1)
              end if

              ! Multiply by sqrt(3) to compensate for the 3 times narrower oval
              hall=CondFactor*( &
                   hal_a0-hal_a1*exp(-abs(IonoJrNow(i,j)*1.0e9)*hal_a2**2))
              ped =CondFactor*( &
                   ped_a0-ped_a1*exp(-abs(IonoJrNow(i,j)*1.0e9)*ped_a2**2))

              ! CMEE enhances auroral oval conductance further:
              if (NameModelIn == 'CMEE') then
                 hall=hall + CondFactor*( &
                      FactorHallCMEE*exp(-(distance/(OvalWidthFactor &
                      * Width_of_Oval(j)))**2))
                 ped =ped  + CondFactor*( &
                      FactorPedCMEE*exp(-(distance/(OvalWidthFactor &
                      * Width_of_Oval(j)))**2))
              endif
           end if

           ! Convert to energy flux & average energy via Reverse-Robinson
           if ((hall > 1.0).and.(ped > 0.5)) then
              AveEOut_II( i,j) = ((hall/ped)/0.45)**(1.0/0.85)
              EfluxOut_II(i,j) = (ped*(16.0+AveEOut_II(i,j)**2)/&
                   (40.0*AveEOut_II(i,j)))**2/1000.0
           else
              AveEOut_II( i,j) = IONO_Min_Ave_E
              EfluxOut_II(i,j) = IONO_Min_EFlux
           endif

           ! Set limits on polar cap precipitation:
           if ((PolarCap_AveE > 0.0).and.(polarcap)) then
              AveEOut_II( i,j) = max(AveEOut_II( i,j), PolarCap_AveE)
              EfluxOut_II(i,j) = max(EfluxOut_II(i,j), PolarCap_EFlux)
           endif

        enddo LAT
     enddo LON

  case default
     call CON_stop(NameSub//' Unrecognized empirical model: '//NameModelIn)
  end select

end subroutine FACs_to_fluxes
!==============================================================================
