
subroutine calc_GITM_sources(iBlock)

  ! Ridley fixes: cvs update  (120503)
  ! Enable Emissions to be calculated elsewhere (170427)

  use ModInputs
  use ModSources
  use ModGITM
  use ModTime, only : tSimulation,istep
  use ModUserGITM

  implicit none

  integer, intent(in) :: iBlock

  integer :: iAlt, iError, iDir, iLat, iLon, iSpecies
  integer :: iiAlt, iiLat, iiLon
  real :: tmp(nLons, nLats, nAlts)
  real :: tmp2(nLons, nLats, 0:nAlts+1)
  real :: tmp3(nLons, nLats, 0:nAlts+1)
  real :: RhoI(nLons, nLats, nAlts)
  real :: ScaleHeight(-1:nLons+2, -1:nLats+2, -1:nAlts+2)

  real :: nVel(1:nAlts, nSpecies)
  real :: NF_Eddy(1:nAlts), NF_NDen(1:nAlts), NF_Temp(1:nAlts)
  real :: NF_NDenS(1:nAlts,1:nSpecies), NF_EddyRatio(1:nAlts,1:nSpecies)
  real :: NF_Gravity(1:nAlts)
  real :: NF_GradLogCon(1:nAlts,1:nSpecies)
  real :: Prandtl(nLons,nLats,0:nalts+1)
  real :: EIMIZ(nReactions_EIM)
  real, dimension(1:nLons, 1:nLats, 1:nAlts, 4) ::  BLocal
  logical :: IsFirstTime = .true.

  ! Potential Temperature
  ! Used in New Eddy Conduction Calculations:  Bell 1-15-2009
  real :: Theta(nLons, nLats, -1:nAlts+2)
  real :: GammaScale(nLons, nLats, -1:nAlts+2)
  real :: P0(nLons, nLats, -1:nAlts+2)

  !! Eddy Velocity Terms
  real :: ConS(nLons,nLats,-1:nAlts+2,1:nSpecies)
  real :: LogConS(nLons,nLats,-1:nAlts+2,1:nSpecies)
  real :: GradLogConS(nLons,nLats,1:nAlts,1:nSpecies)

  call report("calc_GITM_sources",1)

  ! calc_rate is used to determine reaction rates, heating coefficients,
  ! ion-neutral collision frequency, lambdas for ion drag, etc.

  if (iDebugLevel > 4) write(*,*) "=====> going into calc_rates", iproc

  ChemicalHeatingRate = 0.0
  ChemicalHeatingSpecies = 0.0
  call calc_eddy_diffusion_coefficient(iBlock)
  call calc_rates(iBlock)
  call calc_collisions(iBlock)

  RhoI = IDensityS(1:nLons,1:nLats,1:nAlts,ie_,iBlock) * &
       MeanIonMass(1:nLons,1:nLats,1:nAlts)

  !\
  ! Gravity is a source term which is calculated in initialize.f90
  !/

  !\
  ! ---------------------------------------------------------------
  ! These terms are for Neutral Temperature
  ! ---------------------------------------------------------------
  !/

  !\
  ! Solar Heating -------------------------------------------------
  !/

  if (iDebugLevel > 4) write(*,*) "=====> solar heating", iproc

  if (UseSolarHeating .or. UseIonChemistry) then

     ! So far, calc_physics only has stuff that is needed for solar
     ! euv, such as solar zenith angles, and local time.

     call calc_physics(iBlock)

     call euv_ionization_heat(iBlock)

  endif

  if (.not. UseSolarHeating) EuvHeating = 0.0

  ! The auroral heating is specified below, after the aurora is described
  ! in get_potential

  !\
  ! Joule Heating --------------------------------------------------
  !/

  ! If you have turned off IonDrag, then Joule Heating should NOT be
  ! included either.  This is because the Neutrals can become HIGHLY
  ! seperated from the ions, causing MASSIVE Joule heating!!!

  if (UseJouleHeating .and. UseIonDrag) then

     tmp = Collisions(1:nLons,1:nLats,1:nAlts,iVIN_) * &
          RhoI(1:nLons,1:nLats,1:nAlts)/ &
          Rho(1:nLons,1:nLats,1:nAlts,iBlock)

     ! After reading the Thayer paper, this term needs to be taken
     ! out, since it is approximately 1/2, which is the contribution
     ! from ions heating the neutrals through conduction.
     ! * &
     !     (MeanIonMass(1:nLons,1:nLats,1:nAlts)/AMU) /  &
     !     (MeanIonMass(1:nLons,1:nLats,1:nAlts)/AMU + &
     !     MeanMajorMass(1:nLons,1:nLats,1:nAlts)/AMU)

     JouleHeating = 0.0

     do iDir = 1, 3

        JouleHeating(:,:,:) = JouleHeating(:,:,:) + tmp * &
             (IVelocity(1:nLons,1:nLats,1:nAlts,iDir,iBlock) - &
             Velocity(1:nLons,1:nLats,1:nAlts,iDir,iBlock))**2

     enddo

     JouleHeating = JouleHeating / &
          TempUnit(1:nLons,1:nLats,1:nAlts) / &
          cp(:,:,1:nAlts,iBlock)

  else

     JouleHeating = 0.0

  endif

  !\
  ! Conduction ----------------------------------------------------
  !/

  if (iDebugLevel > 4) write(*,*) "=====> conduction", iproc

  if(UseConduction)then

     tmp2 = Rho(1:nLons, 1:nLats,0:nAlts+1, iBlock) * &
          cp(1:nLons, 1:nLats,0:nAlts+1, iBlock)

     Prandtl = 0.0

     call calc_conduction(iBlock, &
          Temperature(1:nLons, 1:nLats,-1:nAlts+2, iBlock) * &
          TempUnit(1:nLons, 1:nLats,-1:nAlts+2), &
          KappaTemp(1:nLons, 1:nLats,0:nAlts+1, iBlock), &
          tmp2, &
          MoleConduction)

     Conduction = MoleConduction/TempUnit(1:nLons, 1:nLats,1:nAlts)

     if (UseTurbulentCond) then

        if (UseUpdatedTurbulentCond) then

           do iAlt = -1,nAlts+2
              P0(1:nLons,1:nLats,iAlt) = Pressure(1:nLons,1:nLats,0,iBlock)
           enddo

           !! Set the Exponent for the Potential Temperature as (Gamma - 1/Gamma)
           !! Must span the range from -1 to nAlts + 2  (same as Theta)

           do iLon = 1, nLons
              do iLat = 1, nLats
                 do iAlt = 1,nAlts
                    GammaScale(iLon,iLat,iAlt) = &
                         (Gamma(iLon,iLat,iAlt,iBlock)-1.0) / &
                         Gamma(iLon,iLat,iAlt,iBlock)
                 enddo
              enddo
           enddo

           do iAlt = -1,0
              GammaScale(1:nLons,1:nLats,iAlt) = &
                   GammaScale(1:nLons,1:nLats,1)
           enddo

           do iAlt = nAlts,nAlts+2
              GammaScale(1:nLons,1:nLats,iAlt) = &
                   GammaScale(1:nLons,1:nLats,nAlts)
           enddo

           Theta(1:nLons,1:nLats,-1:nAlts+2) = &
                Temperature(1:nLons,1:nLats,-1:nAlts+2,iBlock) * &
                TempUnit(1:nLons,1:nLats,-1:nAlts+2)*&
                (P0(1:nLons,1:nLats,-1:nAlts+2)/ &
                Pressure(1:nLons,1:nLats,-1:nAlts+2,iBlock))**&
                GammaScale(1:nLons,1:nLats,-1:nAlts+2)

           !! Prandtl is the Eddy Heat Conduction Coefficient After Hickey et al [2000]

           Prandtl = &
                KappaEddyDiffusion(1:nLons,1:nLats,0:nAlts+1,iBlock) * &
                Rho(1:nLons,1:nLats,0:nAlts+1,iBlock)

           tmp2(1:nLons,1:nLats,0:nAlts+1) = &
                Rho(1:nLons,1:nLats,0:nAlts+1,iBlock)/&
                Gamma(1:nLons,1:nLats,0:nAlts+1,iBlock)

           call calc_conduction(&
                iBlock, &
                Theta,   &
                Prandtl, &
                tmp2,    &
                EddyCond)

           !! Eddy Scaling is Set in UAM.in.  Defaults to 1.0

           EddyCondAdia = &
                (1.0/EddyScaling)* &
                (Temperature(1:nLons,1:nLats,1:nAlts,iBlock) / &
                Theta(1:nLons,1:nLats,1:nAlts))*EddyCond

        else  !! Use The Old Version

           Prandtl = &
                KappaEddyDiffusion(1:nLons, 1:nLats,0:nAlts+1, iBlock) * &
                Rho(1:nLons, 1:nLats,0:nAlts+1, iBlock) * &
                Cp(1:nLons, 1:nLats,0:nAlts+1, iBlock)

           call calc_conduction(iBlock, &
                Temperature(1:nLons, 1:nLats,-1:nAlts+2, iBlock) * &
                TempUnit(1:nLons, 1:nLats,-1:nAlts+2), &
                Prandtl, &
                tmp2, &
                EddyCond)

           Conduction = Conduction + &
                EddyCond/TempUnit(1:nLons, 1:nLats,1:nAlts)

           tmp3 = &
                Prandtl      / &
                Gamma(1:nLons,1:nLats, 0:nAlts+1,iBlock) / &
                Rho(1:nLons, 1:nLats,0:nAlts+1, iBlock)  / &
                Cp(1:nLons, 1:nLats,0:nAlts+1, iBlock)

           call calc_conduction(iBlock, &
                Pressure(1:nLons, 1:nLats,-1:nAlts+2,iBlock), &
                tmp3, &
                tmp2, &
                EddyCondAdia)

           Conduction = Conduction - &
                EddyCondAdia/TempUnit(1:nLons, 1:nLats,1:nAlts)

        endif  !! UseUpdatedTurbulentCond Check
     endif  ! THE USETurbulentCond Check

  else
     Conduction = 0.0
  end if

  !\
  ! ---------------------------------------------------------------
  ! These terms are for Neutral Winds
  ! ---------------------------------------------------------------
  !/

  if (UseIonDrag) then

     tmp = Collisions(1:nLons,1:nLats,1:nAlts,iVIN_)*&
          RhoI/Rho(1:nLons,1:nLats,1:nAlts,iBlock)

     do iDir = 1, 3
        IonDrag(:,:,:,iDir) = tmp * &
             (IVelocity(1:nLons,1:nLats,1:nAlts,iDir,iBlock) - &
             Velocity(1:nLons,1:nLats,1:nAlts,iDir,iBlock))
     enddo

     ! F_iondrag = rho_i/rho * Vis * (Ui-Un)
     ! where Vis = Vin *(Ns/N)

     do iSpecies = 1, nSpecies
        tmp = Collisions(1:nLons,1:nLats,1:nAlts,iVIN_)*&
             RhoI / &
             (Mass(iSpecies) * &
             NDensityS(1:nLons,1:nLats,1:nAlts,iSpecies,iBlock)) * &
             (NDensityS(1:nLons,1:nLats,1:nAlts,iSpecies,iBlock) / &
             NDensity(1:nLons,1:nLats,1:nAlts,iBlock))

        VerticalIonDrag(:,:,:,iSpecies) = tmp * &
             (IVelocity(1:nLons,1:nLats,1:nAlts,iUp_,iBlock) - &
             VerticalVelocity(1:nLons,1:nLats,1:nAlts,iSpecies,iBlock))

     enddo

  else

     IonDrag = 0.0
     VerticalIonDrag = 0.0

  endif


  !\
  ! Viscosity ----------------------------------------------------
  !/

  if (iDebugLevel > 4) write(*,*) "=====> conduction", iproc

  if(UseViscosity)then

     call calc_viscosity(iBlock)

     call calc_conduction(iBlock, &
          Velocity(1:nLons, 1:nLats,-1:nAlts+2, iNorth_, iBlock), &
          ViscCoef(1:nLons, 1:nLats,0:nAlts+1), &
          Rho(1:nLons, 1:nLats,0:nAlts+1, iBlock), &
          Viscosity(1:nLons, 1:nLats,1:nAlts, iNorth_))

     call calc_conduction(iBlock, &
          Velocity(1:nLons, 1:nLats,-1:nAlts+2, iEast_, iBlock), &
          ViscCoef(1:nLons, 1:nLats,0:nAlts+1), &
          Rho(1:nLons, 1:nLats,0:nAlts+1, iBlock), &
          Viscosity(1:nLons, 1:nLats,1:nAlts, iEast_))

     Viscosity(:,:,:,iUp_) = 0.0

  else
     Viscosity = 0.0
  end if

  !\
  ! ---------------------------------------------------------------
  ! These terms are for Ion Densities
  ! ---------------------------------------------------------------
  !/

  if (iDebugLevel > 4) write(*,*) "=====> IonChemistry", iproc, UseIonChemistry

  if (iDebugLevel > 4) write(*,*) "=====> calc_ion_v", iproc

  if (isEarth) then
     call get_potential(iBlock)
     call calc_efield(iBlock)
     call aurora(iBlock)
     if (UseAuroralHeating) then
        AuroralHeating = AuroralHeatingRate(:,:,:,iBlock) / &
             TempUnit(1:nLons,1:nLats,1:nAlts) / cp(:,:,1:nAlts,iBlock) / &
             rho(1:nLons,1:nLats,1:nAlts, iBlock)
     else
        AuroralHeating = 0.0
     endif
  endif

  call calc_ion_v(iBlock)

  ! This includes Radiative Cooling....
  RadCooling = 0.0
  call calc_planet_sources(iBlock)

  ! The Emissions array was never set. Should this be here or earlier ????
  ! Emissions(:,:,:,:,iBlock) = 0.0

  if (UseEmpiricalIonization) then
     !Nightside impact ionization

     if (floor((tSimulation-dt)/dtImpactIonization) /= &
          floor((tsimulation)/dtImpactIonization) .or. IsFirstTime) then
        IsFirstTime = .false.

        attenuationFactor = 0.0
        impactionizationFrequency = 0.0

        BLocal = B0(1:nLons,1:nLats,1:nAlts,1:4,iBlock)

        ! A weighted number density is used to remove atmospheric absorption 
        ! effects prior to calculating the ionization frequencies. We need 
        ! back this out so that absorption is included. 

        do ilon = 1, nlons
           do ilat = 1, nlats
              do ialt = 1, nalts
                 if (Altitude_GB(iLon,iLat,iAlt,iBlock) >= minval(EIMAltitude) &
                      .and. Altitude_GB(iLon,iLat,iAlt,iBlock) <= maxval(EIMAltitude)) then

                    call interpolateEIM(Altitude_GB(iLon,iLat,iAlt,iBlock),Blocal(iLon,iLat,iAlt,iUp_),&
                         Blocal(iLon,iLat,iAlt,iMag_),FieldType(ilon,ilat,iAlt,iBlock),EIMIZ)

                    ! Lastly, apply attenuation factor to re-apply effects of 
                    ! atmospheric attenuation. Note units should be in cm.

                    weightedNDensity = log10(sum(integratedCrossSectionS * &
                         nDensityS(ilon,ilat,ialt,1:nSpecies,iblock)/1e6) / &
                         sum(integratedCrossSectionS))

                    !Limit the minimum weigthed density 
                    weightedNDensity = max(5.55,weightedNDensity)

                    attenuationFactor = 10**(eimAttenFactor(:,1) + &
                         eimAttenFactor(:,2) * weightedNDensity**1 + &
                         eimAttenFactor(:,3) * weightedNDensity**2 + &
                         eimAttenFactor(:,4) * weightedNDensity**3 + &
                         eimAttenFactor(:,5) * weightedNDensity**4 + &
                         eimAttenFactor(:,6) * weightedNDensity**5)

                    ! ! EIM is in units of log(#/s)
                    ! There are several ionization frequencies for each neutral species due 
                    ! to different reactions. They are kept separate here for ease of inspection.
                    ! However, each species has only 1 attenuation factor.

                    impactionizationFrequency(ilon,ilat,ialt,iImpactCO2_X2PI_G:iImpactCO2_A2PI_U,iBlock) = &
                         (10**EIMIZ)*attenuationFactor(iCO2_)

                 endif
              enddo
           end do
        end do
     endif
  endif

  call calc_chemistry(iBlock)

  ChemicalHeatingRate(:,:,:) = &
       ChemicalHeatingRate(:,:,:) * Element_Charge / &
       TempUnit(1:nLons,1:nLats,1:nAlts) / cp(1:nLons,1:nLats,1:nAlts,iBlock)/&
       rho(1:nLons,1:nLats,1:nAlts,iBlock)

  ChemicalHeatingSpecies = ChemicalHeatingSpecies * Element_Charge
end subroutine calc_GITM_sources
