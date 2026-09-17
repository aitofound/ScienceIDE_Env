subroutine calc_chemistry(iBlock)

!  No override of CO2 densities; explicit calculation

  use ModSizeGITM
  use ModGITM
  use ModInputs, only: iDebugLevel, UseIonChemistry, UseNeutralChemistry, UseEmpiricalIonization, DoCheckForNans
  use ModConstants
  use ModSources
  use ModTime, only : iStep
  use GITM_planet, only : ialtminiono
  use ModUserGITM
  use ModEUV
  use ieee_arithmetic

  implicit none

  integer, intent(in) :: iBlock

  real  :: IonSources(nIons),IonLosses(nIons),nSources(nSpeciesTotal),ISources(nIons)
  real  :: NeutralSources(nSpeciesTotal), NeutralLosses(nSpeciesTotal)
  real  :: ChemicalHeatingSub,Emission(nEmissions)
  real :: Ions(nIons), Neutrals(nSpeciesTotal)

  integer :: iLon,iLat,iAlt,niters,iIon, iNeutral, ivar,nImplicitSourceCalls,iminiono,totalsteps

  real :: dttotal, dtsub, dtMin, dtAve, rr, Reaction, szap
  real :: tli(nIons), tsi(nIons), tln(nSpeciesTotal), tsn(nSpeciesTotal)
  real, dimension(nLons,nLats,nAlts) :: &
   te3d, ti3d, tn3d, te33d, ti33d, tn33d, ti103d, ti93d, ti153d, &
   ti83d, te12d, te227d, te073d, tn2983d, te3m0813d, te3m073d, &
   te3m053d, te3m0853d, te3m0393d, ti3m0393d, ti3m0233d, ti15m023d, &
   ti3m0443d, ti3m0453d, ti10m2123d, ti3m0823d, ti15m0753d, ti3m1163d, &
   ti10m0673d, ti15m0414d, ti3m0523d, ti9m0923d, tn298m0323d, &
   te3m0553d, tn3m2503d, tnm203d, tn3m053d, tn3m053de63d, te12m0563d, &
   tne21843d, tn31m057tn3d

  real :: EmissionTotal(nEmissions), F, r

  real :: te3, ti3, tn3, ti, tn, te3m081, te3m07, te12m056
  real :: te3m05, te3m085, te3m039, ti3m039, ti3m023, ti15m02
  real :: ti3m044, ti3m045, ti10m212, ti3m082, ti15m075, tn31m057tn
  real :: ti3m116, ti10m067, ti15m041, ti3m052, ti9m092, tne2184
  real :: tn298m032, te3m055, tn3m250, tnm20, tn3m05, tn3m053de6

  real :: ionso, ionlo, neuso, neulo
        
  logical :: UseNeutralConstituent(nSpeciesTotal)
  logical :: UseIonConstituent(nIons)

  real :: reactionrate(nReactions)

  !---------------------------------------------------------------------------

  UseNeutralConstituent = .true.
  UseIonConstituent     = .true.

  EmissionTotal = 0.0
  
  ChemicalHeatingRate = 0.0
  ChemicalHeatingSpecies = 0.0

  call report("Chemistry",2)
  call start_timing("calc_chemistry")
  if (iDebugLevel > 3) then
     do iIon = 1, nIons
        write(*,*) "====> start calc_chemistry: Max Ion Density: ", iIon, &
             maxval(IDensityS(1:nLons,1:nLats,(nAlts*4)/5,iIon,iBlock))
     enddo
  endif

  DtMin = Dt

  if (.not. UseIonChemistry) return

  call report("Chemistry", 2)
  call start_timing("calc_chemistry")

  DtAve = 0.0

  nIters = 0

  if (DoCheckForNans) then
    call check_for_nans_ions('before chemistry')
    call check_for_nans_neutrals('before chemistry')
    call check_for_nans_temps('before chemistry')
  endif

  tn3d = Temperature(1:nLons, 1:nLats, 1:nAlts, iBlock)* &
         TempUnit(1:nLons, 1:nLats, 1:nAlts)

  ti3d = iTemperature(1:nLons, 1:nLats, 1:nAlts, iBlock)
  te3d = eTemperature(1:nLons,1:nLats,1:nAlts,iBlock)

  te33d = te3d/300.0
  ti33d = ti3d/300.0
  tn33d = tn3d/300.0

  ti103d = ti3d/1000.0
  ti93d = ti3d/900.0
  ti153d = ti3d/1500.0
  ti83d = ti3d/800.0
  te33d = eTemperature(1:nLons, 1:nLats, 1:nAlts, iBlock)/300.0
  te12d = eTemperature(1:nLons, 1:nLats, 1:nAlts, iBlock)/1200.0
  te227d = -22740.0/eTemperature(1:nLons, 1:nLats, 1:nAlts, iBlock)
  te073d = (250.0/eTemperature(1:nLons, 1:nLats, 1:nAlts, iBlock))**0.7
  tn2983d = tn3d/298.0

  

  te3m0813d = te33d**(0.81)
  te3m073d = te33d**(-0.7)
  te12m0563d = te12d**(-0.56)
  te3m053d = te33d**(-0.5)
  te3m0853d = te33d**(-0.85)
  te3m0393d = te33d**(-0.39)
  ti3m0393d = ti83d**(-0.39)
  ti3m0233d = ti33d**(-0.23)
  ti15m023d = ti153d**(0.2)
  ti3m0443d = ti33d**(-0.44)
  ti3m0453d = ti33d**(-0.45)
  ti10m2123d = ti103d**(2.12)
  ti3m0823d = ti33d**(-0.82)
  ti15m0753d = ti153d**(0.75)
  ti3m1163d = ti33d**(-1.16)
  ti10m0673d = ti103d**(0.67)
  ti15m0414d = ti153d**(0.41)
  ti3m0523d = ti33d**(-0.52)
  ti9m0923d = ti93d**(0.92)
  tn298m0323d = tn2983d**(-0.32)
  te3m0553d = te33d**(-0.55)
  tn3m2503d = tn33d**(-2.5)
  tnm203d = tn3d**(-2)
  tn3m053d = tn33d**(-0.5)
  tn3m053de63d = tn33d**(0.5)*exp(-600/tn3d)
  tne21843d = exp(-2184./tn3d)
  tn31m057tn3d = tn33d*(1.0-0.57/tn3d**0.5)

  do iLon = 1, nLons
    do iLat = 1, nLats

      szap = cos(sza(iLon, iLat, iBlock))
      if (szap < 0.0) szap = 0.0

      do iAlt = 1, nAlts

        NeutralSourcesTotal = 0.0
        NeutralLossesTotal = 0.0

        te3 = te33d(iLon, iLat, iAlt)
        ti3 = ti33d(iLon, iLat, iAlt)
        tn3 = tn33d(iLon, iLat, iAlt)
        ti = iTemperature(iLon, iLat, iAlt, iBlock)
        tn = Temperature(iLon, iLat, iAlt, iBlock)* &
             TempUnit(iLon, iLat, iAlt)
        
        te3m081 = te3m0813d(iLon,iLat,iAlt)
        te3m07 = te3m073d(iLon,iLat,iAlt)
        te12m056 = te12m0563d(iLon,iLat,iAlt)     
        te3m05 = te3m053d(iLon,iLat,iAlt)
        te3m085 = te3m0853d(iLon,iLat,iAlt)
        te3m039 = te3m0393d(iLon,iLat,iAlt)
        ti3m039 = ti3m0393d(iLon,iLat,iAlt)
        ti3m023 = ti3m0233d(iLon,iLat,iAlt)
        ti15m02 = ti15m023d(iLon,iLat,iAlt)
        ti3m044 = ti3m0443d(iLon,iLat,iAlt)
        ti3m045 = ti3m0453d(iLon,iLat,iAlt)
        ti10m212 = ti10m2123d(iLon,iLat,iAlt)
        ti3m082 = ti3m0823d(iLon,iLat,iAlt)
        ti15m075 = ti15m0753d(iLon,iLat,iAlt)
        ti3m116 = ti3m1163d(iLon,iLat,iAlt)
        ti10m067 = ti10m0673d(iLon,iLat,iAlt)
        ti15m041 = ti15m0414d(iLon,iLat,iAlt)
        ti3m052 = ti3m0523d(iLon,iLat,iAlt)
        ti9m092 = ti9m0923d(iLon,iLat,iAlt)
        tn298m032 = tn298m0323d(iLon,iLat,iAlt)
        te3m055 = te3m0553d(iLon,iLat,iAlt)
        tn3m250 = tn3m2503d(iLon,iLat,iAlt)
        tnm20 = tnm203d(iLon,iLat,iAlt)
        tn3m05 = tn3m053d(iLon,iLat,iAlt)
        tn3m053de6 = tn3m053de63d(iLon,iLat,iAlt)
        tne2184 = tne21843d(iLon,iLat,iAlt)
        tn31m057tn = tn31m057tn3d(iLon,iLat,iAlt)

        DtTotal = 0.0
        EmissionTotal = 0.0

        Ions = IDensityS(iLon, iLat, iAlt, :, iBlock)

        Neutrals = NDensityS(iLon, iLat, iAlt, :, iBlock)

        niters = 0

        do while (DtTotal < Dt)

          ChemicalHeatingSub = 0.0
          ChemicalHeatingS = 0
          Emission = 0.0

          DtSub = Dt - DtTotal

          IonSources = 0.0
          NeutralSources = 0.0
          IonLosses = 0.0
          NeutralLosses = 0.0

          !\
          ! Nitrogen Photochemistry:--------------------------------------------------+
          !/
          ! ----------------------------------------------------------
          ! N2 + hv ==> N(4S) + N(2D)   Assume 50% Branching Ratio
          ! N2 + pe ==> N(4S) + N(2D)   Assume 50% Branching Ratio (later)
          ! Triple photodissrate due to fine structure approximation (VTGCM)
          ! ----------------------------------------------------------
          !rr=EuvDissRateS(iLon,iLat,iAlt,iN2_,iBlock) + PEDissRateS(iLon,iLat,iAlt,iN2_,iBlock)
          rr=EuvDissRateS(iLon,iLat,iAlt,iPDN2_N4S_N2D,iBlock)*3.0
          Reaction = rr * Neutrals(iN2_)

          NeutralLosses(iN2_) = NeutralLosses(iN2_) + Reaction
          NeutralSources(iN4S_) = NeutralSources(iN4S_) + 2.*0.5*Reaction
          NeutralSources(iN2D_) = NeutralSources(iN2D_) + 2.*0.5*Reaction
          reactionrate(1) = reaction
          ! ----------------------------------------------------------
          ! N2 + hv ==> N2+
          ! ----------------------------------------------------------

          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPIN2_N2P,iBlock)*Neutrals(iN2_)

          NeutralLosses(iN2_) = NeutralLosses(iN2_) + Reaction
          IonSources(iN2P_) = IonSources(iN2P_) + Reaction
          reactionrate(2) = reaction
          !\
          ! CO2 Photochemistry:-----------------------------------------------------+
          !/
          ! ----------------------------------------------------------
          ! CO2 + hv ==> CO2+
          ! ----------------------------------------------------------
          
          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPICO2_CO2P,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          IonSources(iCO2P_) = IonSources(iCO2P_) + Reaction
          reactionrate(3) = reaction

          ! ----------------------------------------------------------
          ! CO2 + hv ==> O+ + CO
          ! ----------------------------------------------------------

          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPICO2_OP_CO,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          IonSources(iOP_) = IonSources(iOP_) + Reaction
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction
         reactionrate(4) = reaction
          ! ----------------------------------------------------------
          ! CO2 + hv ==> CO+ + O + e
          ! ----------------------------------------------------------

          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPICO2_COP_O,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          IonSources(iCOP_) = IonSources(iCOP_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
         reactionrate(5) = reaction

          ! ----------------------------------------------------------
          ! CO2 + hv ==> CO+ + O+ +2e
          ! ----------------------------------------------------------

          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPICO2_COP_OP,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          IonSources(iCOP_) = IonSources(iCOP_) + Reaction
          IonSources(iOP_) = IonSources(iOP_) + Reaction
         reactionrate(6) = reaction

          ! ----------------------------------------------------------
          ! CO2 + hv ==> C+ + O2 + e
          ! ----------------------------------------------------------

          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPICO2_CP_O2,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          IonSources(iCP_) = IonSources(iCP_) + Reaction
          NeutralSources(iO2_) = NeutralSources(iO2_) + Reaction
         reactionrate(7) = reaction


          ! ----------------------------------------------------------
          ! CO2 + hv ==> C+ + O+ + O +2e
          ! ----------------------------------------------------------

          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPICO2_CP_OP_O,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          IonSources(iCP_) = IonSources(iCP_) + Reaction
          IonSources(iOP_) = IonSources(iOP_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
         reactionrate(8) = reaction


          ! ----------------------------------------------------------
          ! CO2 + hv ==> CO + O
          ! ----------------------------------------------------------

          Reaction = EuvDissRateS(iLon,iLat,iAlt,iPDCO2_CO_O,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction
          reactionrate(9) = reaction

          ! ----------------------------------------------------------
          ! CO2 + hv ==> O2 + C
          ! ----------------------------------------------------------
          Reaction = EuvDissRateS(iLon,iLat,iAlt,iPDCO2_O2_C,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          NeutralSources(iO2_) = NeutralSources(iO2_) + Reaction
          NeutralSources(iC_) = NeutralSources(iC_) + Reaction
          reactionrate(10) = reaction

          ! ----------------------------------------------------------
          ! CO2 + hv ==> 2O + C
          ! ----------------------------------------------------------
          Reaction = EuvDissRateS(iLon,iLat,iAlt,iPDCO2_2O_C,iBlock)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction * 2
          NeutralSources(iC_) = NeutralSources(iC_) + Reaction
          reactionrate(11) = reaction

          ! ----------------------------------------------------------
          ! CO2 impact ionization
          ! ----------------------------------------------------------

          if (UseEmpiricalIonization) then

             Reaction = &
                  (impactIonizationFrequency(iLon,iLat,iAlt,iImpactCO2_X2PI_G,iBlock) + &
                  impactIonizationFrequency(iLon,iLat,iAlt,iImpactCO2_B2Sig,iBlock) + &
                  impactIonizationFrequency(iLon,iLat,iAlt,iImpactCO2_A2PI_U,iBlock)) &
                  *Neutrals(iCO2_)
             IonSources(iCO2P_) = IonSources(iCO2P_) + Reaction
             NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction

          endif

          !\
          ! CO Photochemistry:-----------------------------------------------------+
          !/
          ! ----------------------------------------------------------
          ! CO + hv ==> O + C
          !         ==> O(1D) + C(1D)
          ! ----------------------------------------------------------
          Reaction = EuvDissRateS(iLon,iLat,iAlt,iPDCO_C_O,iBlock)*Neutrals(iCO_)

          NeutralLosses(iCO_) = NeutralLosses(iCO_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          NeutralSources(iC_) = NeutralSources(iC_) + Reaction
          reactionrate(12) = reaction

          ! ----------------------------------------------------------
          ! CO + hv ==> C + O+ + e
          ! ----------------------------------------------------------

          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPICO_C_OP,iBlock)*Neutrals(iCO_)

          NeutralLosses(iCO_) = NeutralLosses(iCO_) + Reaction
          NeutralSources(iC_) = NeutralSources(iC_) + Reaction
          IonSources(iOP_) = IonSources(iOP_) + Reaction
         reactionrate(13) = reaction
          !\
          ! O2 Photochemistry:-----------------------------------------------------+
          !
          !             ! ----------------------------------------------------------
          !             ! O2 + hv ==> O2+   Minor source of O2+  (Mnor)
          !             ! ----------------------------------------------------------
          !
          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPIO2_O2P,iBlock)*Neutrals(iO2_)

          NeutralLosses(iO2_) = NeutralLosses(iO2_) + Reaction
          IonSources(iO2P_) = IonSources(iO2P_) + Reaction
          reactionrate(7) = reaction
         reactionrate(14) = reaction
          ! ----------------------------------------------------------
          ! O2 + hv ==> O + O
          ! ----------------------------------------------------------

          Reaction = EuvDissRateS(iLon,iLat,iAlt,iPDO2_O_O,iBlock)*Neutrals(iO2_)

          NeutralLosses(iO2_) = NeutralLosses(iO2_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + 2.*Reaction
          reactionrate(15) = reaction

          !\
          ! O Photochemistry:-----------------------------------------------------+
          !/
          ! ----------------------------------------------------------
          ! O + hv ==> O+
          ! ----------------------------------------------------------

          Reaction = EuvIonRateS(iLon,iLat,iAlt,iPIO_OP,iBlock)*Neutrals(iO_)

          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          IonSources(iOP_) = IonSources(iOP_) + Reaction
          reactionrate(16) = reaction

          !\
          !-----------End Photochemistry-----------------------------------------------------+
          !\
          !-----------Begin Electron Recombination Chemistry---------------------------------+
          !/
          ! -----------------------------------------------------------
          ! N2+ + e- ==> N4S + N2D  
          ! -----------------------------------------------------------
          rr = 1.01e-13*te3m039

          Reaction = rr * Ions(ie_) * Ions(iN2P_)

          IonLosses(iN2P_) = IonLosses(iN2P_) + Reaction

          NeutralSources(iN4S_) = NeutralSources(iN4S_) + Reaction
          NeutralSources(iN2D_) = NeutralSources(iN2D_) + Reaction
          reactionrate(17) = reaction

          ! -----------------------------------------------------------
          ! N2+ + e- ==> N2D + N2D  
          ! -----------------------------------------------------------
          rr = 1.01e-13*te3m039

          Reaction = rr * Ions(ie_) * Ions(iN2P_)

          IonLosses(iN2P_) = IonLosses(iN2P_) + Reaction
          NeutralSources(iN2D_) = NeutralSources(iN2D_) + 2.*Reaction
          reactionrate(18) = reaction 

          ! -----------------------------------------------------------
          ! O2+ + e- ==> 2O + 6.99 eV  (There are two other excited state branches that we should look at)
          ! -----------------------------------------------------------
          if (ti <= 1200.0) then
             rr = 1.95e-13*te3m07
          else
             rr = 7.39e-14*te12m056
          endif
          Reaction = rr * Ions(ie_) * Ions(iO2P_)

          IonLosses(iO2P_) = IonLosses(iO2P_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + 2.*Reaction
          reactionrate(19) = reaction
          ! -----------------------------------------------------------
          ! CO2+ + e- ==> O + CO
          ! -----------------------------------------------------------

          rr = 3.5e-13*te3m05
          Reaction = rr * Ions(ie_) * Ions(iCO2P_)

          IonLosses(iCO2P_) = IonLosses(iCO2P_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction
          reactionrate(20) = reaction
          ! ----------------------------------------------------------
          ! NO+ + e- ==> O + N2D + 0.38 eV
          ! g = 0.75
          ! -----------------------------------------------------------

          rr =  3.4e-13*te3m05
          Reaction = rr * Ions(ie_) * Ions(iNOP_)

          IonLosses(iNOP_) = IonLosses(iNOP_) + Reaction
          NeutralSources(iN2D_) = NeutralSources(iN2D_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          reactionrate(21) = reaction

          ! -----------------------------------------------------------
          ! NO+ + e- ==> O + N4S + 2.77 eV
          ! (1-g) = 0.25
          ! -----------------------------------------------------------
          rr = 0.6e-13*te3m05
          Reaction = rr * Ions(ie_) * Ions(iNOP_)

          IonLosses(iNOP_) = IonLosses(iNOP_) + Reaction
          NeutralSources(iN4S_) = NeutralSources(iN4S_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          reactionrate(22) = reaction
          !-----------End Electron Recombination Chemistry-------------------------------------------+
          !/
          !-----------Begin Bi-Molecular Ion-Neutral and Neutral-Neutral Chemistry-----------+
          !/
          ! -----------------------------------------------------------
          ! CO2+ + O ==> O2+ + CO  Fast
          ! -----------------------------------------------------------
          rr = 1.64e-16
          Reaction = rr * Neutrals(iO_) * Ions(iCO2P_)

          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          IonLosses(iCO2P_) = IonLosses(iCO2P_) + Reaction
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction
          IonSources(iO2P_) = IonSources(iO2P_) + Reaction
          reactionrate(23) = reaction

          ! -----------------------------------------------------------
          ! CO2+ + O ==> O+ + CO2  Fast
          ! -----------------------------------------------------------
          rr = 9.60e-17
          Reaction =rr * Neutrals(iO_) * Ions(iCO2P_)

          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          IonLosses(iCO2P_) = IonLosses(iCO2P_) + Reaction
          NeutralSources(iCO2_) = NeutralSources(iCO2_) + Reaction
          IonSources(iOP_) = IonSources(iOP_) + Reaction
          reactionrate(24) = reaction

          ! -----------------------------------------------------------
          ! CO2 + O+ ==> O2+ + CO  Fast
          ! -----------------------------------------------------------
          if (ti <= 800) then 
             rr = 1.1e-15
          else
             rr = 1.1e-15*ti3m039
          endif

          Reaction = rr * Neutrals(iCO2_) * Ions(iOP_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          IonLosses(iOP_) = IonLosses(iOP_) + Reaction
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction
          IonSources(iO2P_) = IonSources(iO2P_) + Reaction
          reactionrate(25) = reaction

          ! -----------------------------------------------------------
          ! CO2 + N2+ ==> CO2+ + N2
          ! -----------------------------------------------------------
          rr = 9.00e-16*ti3m023
          Reaction = rr * Neutrals(iCO2_) * Ions(iN2P_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          IonLosses(iN2P_) = IonLosses(iN2P_) + Reaction
          NeutralSources(iN2_) = NeutralSources(iN2_) + Reaction
          IonSources(iCO2P_) = IonSources(iCO2P_) + Reaction
          reactionrate(26) = reaction


          ! -----------------------------------------------------------
          ! O + N2+ ==> NO+ + N2D + 0.70eV
          ! -----------------------------------------------------------
          if (ti <= 1500.0) then
             rr = 1.33e-16*ti3m044
          else
             rr = 6.55e-17*ti15m02
          endif
          Reaction = rr * Neutrals(iO_) * Ions(iN2P_)

          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          IonLosses(iN2P_) = IonLosses(iN2P_) + Reaction
          NeutralSources(iN2D_) = NeutralSources(iN2D_) + Reaction
          IonSources(iNOP_) = IonSources(iNOP_) + Reaction
          reactionrate(27) = reaction

          ! -----------------------------------------------------------
          ! N2 + O+ ==> NO+ + N4S
          !-----------------------------------------------------------
          if (ti <= 1000) then 
             rr = 1.20e-18*ti3m045 
          else
             rr = 7.00e-19*ti10m212
          endif
          Reaction = rr * Neutrals(iN2_) * Ions(iOP_)

          NeutralLosses(iN2_) = NeutralLosses(iN2_) + Reaction
          IonLosses(iOP_) = IonLosses(iOP_) + Reaction
          NeutralSources(iN4S_) = NeutralSources(iN4S_) + Reaction
          IonSources(iNOP_) = IonSources(iNOP_) + Reaction
          reactionrate(28) = reaction

          ! -----------------------------------------------------------
          ! O2+ + N4S ==> NO+ + O + 4.21 eV
          ! -----------------------------------------------------------
          rr = 1.00e-16
          Reaction = rr * Neutrals(iN4S_) * Ions(iO2P_)

          NeutralLosses(iN4S_) = NeutralLosses(iN4S_) + Reaction
          IonLosses(iO2P_) = IonLosses(iO2P_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          IonSources(iNOP_) = IonSources(iNOP_) + Reaction
          reactionrate(29) = reaction

          ! -----------------------------------------------------------
          ! CO2+ + O2 ==> CO2 + O2+
          ! -----------------------------------------------------------
          if (ti <= 1500) then
             rr = 5.50e-17*ti3m082
          else 
             rr = 1.50e-17*ti15m075
          endif
          reaction = rr * Neutrals(iO2_) * Ions(iCO2P_)

          NeutralLosses(iO2_) = NeutralLosses(iO2_) + Reaction
          IonLosses(iCO2P_) = IonLosses(iCO2P_) + Reaction
          NeutralSources(iCO2_) = NeutralSources(iCO2_) + Reaction
          IonSources(iO2P_) = IonSources(iO2P_) + Reaction
          reactionrate(30) = reaction

          ! -----------------------------------------------------------
          !CO2+ + NO ==> NO+ + CO2
          ! -----------------------------------------------------------
          rr = 1.23e-16
          reaction = rr * Neutrals(iNO_) * Ions(iCO2P_)

          NeutralLosses(iNO_) = NeutralLosses(iNO_) + Reaction
          IonLosses(iCO2P_) = IonLosses(iCO2P_) + Reaction
          NeutralSources(iCO2_) = NeutralSources(iCO2_) + Reaction
          IonSources(iNOP_) = IonSources(iNOP_) + Reaction
          reactionrate(31) = reaction

          ! -----------------------------------------------------------
          ! CO2+ + N2D ==> N+ + CO2    Need to add N+
          ! -----------------------------------------------------------
          !              rr = 2.00e-16a
          !              reaction = rr * Neutrals(iN2D_) * Ions(iCO2P_)
          !              NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          !              IonLosses(iCO2P_) = IonLosses(iCO2P_) + Reaction
          !              NeutralSources(iCO2_) = NeutralSources(iCO2_) + Reaction
          !              IonSources(iNP_) = IonSources(iNP_) + Reaction


          ! -----------------------------------------------------------
          ! O2+ + N2D ==> NO+ + O
          ! -----------------------------------------------------------
          rr = 1.8e-16
          reaction = rr * Neutrals(iN2D_) * Ions(iO2P_)

          NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          IonLosses(iO2P_) = IonLosses(iO2P_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          IonSources(iNOP_) = IonSources(iNOP_) + Reaction
          reactionrate(32) = reaction

          ! -----------------------------------------------------------
          ! O2+ + N2D ==> N+ + O2   !!!!! Need to add N+
          ! -----------------------------------------------------------
          !               rr = 8.65e-17
          !               reaction = rr * Neutrals(iN2D_) * Ions(iO2P_)

          !               NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          !               IonLosses(iO2P_) = IonLosses(iO2P_) + Reaction
          !               NeutralSources(iO2_) = NeutralSources(iO2_) + Reaction
          !               IonSources(iNP_) = IonSources(iNP_) + Reaction
          !               reactionrate(25) = reaction

          ! -----------------------------------------------------------
          ! O2+ + N2 ==> NO+ + NO
          ! -----------------------------------------------------------
          rr = 1.00e-21
          reaction = rr * Neutrals(iN2_) * Ions(iO2P_)

          NeutralLosses(iN2_) = NeutralLosses(iN2_) + Reaction
          IonLosses(iO2P_) = IonLosses(iO2P_) + Reaction
          NeutralSources(iNO_) = NeutralSources(iNO_) + Reaction
          IonSources(iNOP_) = IonSources(iNOP_) + Reaction
          reactionrate(33) = reaction


          ! -----------------------------------------------------------
          ! N2+ + O2 ==> N2 + O2+
          ! -----------------------------------------------------------
          if (ti <= 1000) then
             rr = 5.10e-17*ti3m116
          else 
             rr = 1.26e-17*ti10m067 
          endif

          reaction = rr * Neutrals(iO2_) * Ions(iN2P_)
          NeutralLosses(iO2_) = NeutralLosses(iO2_) + Reaction
          IonLosses(iN2P_) = IonLosses(iN2P_) + Reaction
          NeutralSources(iN2_) = NeutralSources(iN2_) + Reaction
          IonSources(iO2P_) = IonSources(iO2P_) + Reaction
          reactionrate(34) = reaction

          ! -----------------------------------------------------------
          ! N2+ + O ==> O+ + N2 + 1.96 eV
          ! -----------------------------------------------------------
          if (ti <= 1500) then
             rr = 7.00e-18*ti3m023
          else 
             rr = 4.83e-18*ti15m041 
          endif

          reaction = rr * Neutrals(iO_) * Ions(iN2P_)
          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          IonLosses(iN2P_) = IonLosses(iN2P_) + Reaction
          NeutralSources(iN2_) = NeutralSources(iN2_) + Reaction
          IonSources(iOP_) = IonSources(iOP_) + Reaction
          reactionrate(35) = reaction

          ! -----------------------------------------------------------
          ! N2+ + NO ==> N2 + NO+ + 6.33 eV
          ! -----------------------------------------------------------
          rr = 3.60e-16
          reaction = rr * Neutrals(iNO_) * Ions(iN2P_)

          NeutralLosses(iNO_) = NeutralLosses(iNO_) + Reaction
          IonLosses(iN2P_) = IonLosses(iN2P_) + Reaction
          NeutralSources(iN2_) = NeutralSources(iN2_) + Reaction
          IonSources(iNOP_) = IonSources(iNOP_) + Reaction
          reactionrate(36) = reaction

          ! -----------------------------------------------------------
          ! O+ + O2 ==> O + O2+
          ! -----------------------------------------------------------
          if (ti <= 900) then 
             rr = 1.6e-17*ti3m052
          else 
             rr = 9.00e-18*ti9m092 
          endif
          reaction = rr * Neutrals(iO2_) * Ions(iOP_)
          NeutralLosses(iO2_) = NeutralLosses(iO2_) + Reaction
          IonLosses(iOP_) = IonLosses(iOP_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          IonSources(iO2P_) = IonSources(iO2P_) + Reaction
          reactionrate(37) = reaction

          ! ----------------------------------------------------------
          ! O2 + C ==> CO + O(4S)
          ! ----------------------------------------------------------
          rr = 4.90e-17 * tn298m032
          reaction = rr * Neutrals(iO2_)*Neutrals(iC_)

          NeutralLosses(iO2_) = NeutralLosses(iO2_) + Reaction 
          NeutralLosses(iC_) = NeutralLosses(iC_) + Reaction 
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction 
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction 
          reactionrate(38) = reaction


          ! ----------------------------------------------------------
          ! CO+ + e ==> C + O
          ! ----------------------------------------------------------
          rr = 1.80e-13*te3m055  !!!Fox and Sung- compare to: 4.82e-6*te**-0.55*1.0e-6
          reaction = rr* Ions(iCOP_) * Ions(ie_)

          IonLosses(iCOP_) = IonLosses(iCOP_) + Reaction 
          NeutralSources(iC_) = NeutralSources(iC_) + Reaction 
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction 
         reactionrate(39) = reaction
          ! ----------------------------------------------------------
          ! CO+ + CO2 -> CO2+ + CO
          ! ----------------------------------------------------------
          rr = 1.10e-15
          reaction = rr * Ions(iCOP_)*Neutrals(iCO2_)

          IonLosses(iCOP_) = IonLosses(iCOP_) + Reaction 
          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction 
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction 
          IonSources(iCO2P_) = IonSources(iCO2P_) + Reaction 
          reactionrate(40) = reaction


          ! ----------------------------------------------------------
          ! C+ + CO2 -> CO+ + CO
          ! ----------------------------------------------------------
          rr = 1.10e-15
          reaction = rr * Ions(iCP_)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction 
          IonLosses(iCP_) = IonLosses(iCP_) + Reaction 
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction 
          IonSources(iCOP_) = IonSources(iCOP_) + Reaction 
          reactionrate(41) = reaction

          ! ----------------------------------------------------------
          ! C+ + CO2 -> CO2+ + C
          ! ----------------------------------------------------------
          rr = 1.10e-16
          reaction = rr * Ions(iCP_)*Neutrals(iCO2_)

          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction 
          IonLosses(iCP_) = IonLosses(iCP_) + Reaction 
          NeutralSources(iC_) = NeutralSources(iC_) + Reaction 
          IonSources(iCO2P_) = IonSources(iCO2P_) + Reaction 
          reactionrate(42) = reaction

          !\
          !-----------End Bi-Molecular Ion-Neutral and Neutral-NeutralChemistry-------------------+
          !/
          !\
          !-----------Ter-Molecular Neutral-Neutral Chemistry-------------------------------+
          !/
          ! -----------------------------------------------------------
          ! O + O + CO2 ==> O2 + CO2
          ! -----------------------------------------------------------

          ! -------------------------------------------------------
          !   These are all listed as possible?
          !          rtO_O_CO2 = 2.75e-32*(200./tn)**3.3*1.e-12
          !          rtO_O_CO2 = 2.75e-32*1.e-12
          !         rtO_O_CO2 = 3.22e-28*(1./tn)**2.0*1.e-12

          rr = 3.22e-40*tnm20
          Reaction = rr*Neutrals(iO_)*Neutrals(iO_)*Neutrals(iCO2_)

          NeutralLosses(iO_) = NeutralLosses(iO_) + 2.*Reaction
          NeutralSources(iO2_) = NeutralSources(iO2_) + Reaction
          reactionrate(43) = reaction
          ! -----------------------------------------------------------
          ! O + O2 + CO2 ==> stuff + CO2
          ! -----------------------------------------------------------
          !          rtO_O2_CO2 = 5.0e-28/tn**2.3*1.e-12
          !          rtO_O2_CO2 = 1.35e-33*1.e-12
          !         rtO_O2_CO2 = 1.4e-33*(300./tn)**2.5*1.e-12

          rr = 1.40e-45*tn3m250
          Reaction = rr *Neutrals(iO_)*Neutrals(iO2_)*Neutrals(iCO2_)

          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          NeutralLosses(iO2_) = NeutralLosses(iO2_) + Reaction
          reactionrate(44) = reaction
          ! -----------------------------------------------------------
          ! O + CO + CO2 ==> 2.*CO2
          ! -----------------------------------------------------------
          !          rtO_CO_CO2 = 6.5e-33*exp(-2180./tn)*1.e-12
          !          rtO_CO_CO2 = 1.6e-32*exp(-2184./tn)*1.e-12
          ! -------------------------------------------------
          rr = 1.6e-44*tne2184
          Reaction = rr *Neutrals(iO_)*Neutrals(iCO_)*Neutrals(iCO2_)

          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          NeutralLosses(iCO_) = NeutralLosses(iCO_) + Reaction
          NeutralSources(iCO2_) = NeutralSources(iCO2_) + Reaction
          reactionrate(45) = reaction

          ! -----------------------------------------------------------
          ! O + N4S + CO2 ==> NO + CO2
          ! -----------------------------------------------------------
          !          rtO_N4S_CO2 = 2.0e-32*rt300tn*1.e-12
          !          rtO_N4S_CO2 = 1.83e-32*rt300tn*1.e-12

          rr = 1.83e-44*tn3m05
          Reaction = rr*Neutrals(iO_)*Neutrals(iN4S_)*Neutrals(iCO2_)

          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          NeutralLosses(iN4S_) = NeutralLosses(iN4S_) + Reaction
          NeutralSources(iNO_) = NeutralSources(iNO_) + Reaction
          reactionrate(46) = reaction
          !\
          !-----------NOX Specific Ion-Neutral and Neutral-Neutral Chemistry-------------------+
          !/
          ! -----------------------------------------------------------
          ! N2D + CO2  ==> NO + CO
          ! -----------------------------------------------------------
          rr = 2.8e-19  !Fox and Sung: 3.6e-19

          Reaction = rr*Neutrals(iN2D_)*Neutrals(iCO2_)

          NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction

          NeutralSources(iNO_) = NeutralSources(iNO_) + Reaction
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction
          reactionrate(47) = reaction
          ! -----------------------------------------------------------
          ! N2D + CO  ==> N4S + CO
          ! -----------------------------------------------------------
          rr = 1.9e-18
          Reaction = rr*Neutrals(iN2D_)*Neutrals(iCO_)

          NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          NeutralSources(iN4S_) = NeutralSources(iN4S_) + Reaction
          reactionrate(48) = reaction
          ! -----------------------------------------------------------
          ! N2D + O  ==> N4S + O + 2.38 eV
          ! -----------------------------------------------------------
          !rr = 6.9e-19 !2.0e-17 was in the old code? Seems way too large
          rr = 2.0e-18 !Used in E-GITM, consistent with range in literature
          Reaction = rr*Neutrals(iN2D_)*Neutrals(iO_)

          NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          NeutralSources(iN4S_) = NeutralSources(iN4S_) + Reaction
          reactionrate(49) = reaction
          ! -----------------------------------------------------------
          ! N2D + O2  ==> N4S + O2 + 3.76 eV
          ! -----------------------------------------------------------
          rr =  9.7e-18*exp(-185./tn)
          Reaction = rr*Neutrals(iN2D_)*Neutrals(iO2_)

          NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          NeutralSources(iN4S_) = NeutralSources(iN4S_) + Reaction
          reactionrate(50) = reaction
          ! -----------------------------------------------------------
          ! N2D + N2  ==> N4S + N2
          ! -----------------------------------------------------------
          rr = 1.7e-20
          Reaction = rr*Neutrals(iN2D_)*Neutrals(iN2_)

          NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          NeutralSources(iN4S_) = NeutralSources(iN4S_) + Reaction
          reactionrate(51) = reaction
          ! -----------------------------------------------------------
          ! NO + N4S  ==> N2 + O + 3.25 eV
          ! -----------------------------------------------------------
          rr  = 2.5e-16*tn3m053de6
          !rr = 3.4e-17 !Fox and sung
          Reaction = rr*Neutrals(iN4S_)*Neutrals(iNO_)

          NeutralLosses(iNO_) = NeutralLosses(iNO_) + Reaction
          NeutralLosses(iN4S_) = NeutralLosses(iN4S_) + Reaction
          NeutralSources(iN2_) = NeutralSources(iN2_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          reactionrate(52) = reaction
          ! -----------------------------------------------------------
          ! N4S + O  ==> NO* + hv (Nightglow reaction)
          ! New code added by S. W. Bougher (170427)
          ! -----------------------------------------------------------

          rr = 1.9e-23 * tn31m057tn
          Reaction = rr*Neutrals(iN4S_)*Neutrals(iO_)

          NeutralLosses(iN4S_) = NeutralLosses(iN4S_) + Reaction
          NeutralLosses(iO_) = NeutralLosses(iO_) + Reaction
          NeutralSources(iNO_) = NeutralSources(iNO_) + Reaction
          reactionrate(53) = reaction
          !             Total NO (delta plus gamma band system) mission (190-260 nm)
          !             Initially Should be ph/m3.sec units  (check)
          !             Convert to ph/cm3.sec units
          Emission(iENOUV_) = Emission(iENOUV_) + Reaction*1.0E-06

          !----------------------------------
          !New reactions
          !----------------------------------
          ! -----------------------------------------------------------
          ! N + CO2  ==> NO + CO
          ! -----------------------------------------------------------
          !rr = 1.7e-22
          rr = 1.7e-25 !see discussion in Fox and Sung 2001 about this reaction
          Reaction = rr * Neutrals(iN4S_)*Neutrals(iCO2_)

          NeutralLosses(iN4S_) = NeutralLosses(iN4S_) + Reaction
          NeutralLosses(iCO2_) = NeutralLosses(iCO2_) + Reaction
          NeutralSources(iNO_) = NeutralSources(iNO_) + Reaction
          NeutralSources(iCO_) = NeutralSources(iCO_) + Reaction
          reactionrate(54) = reaction

          ! -----------------------------------------------------------
          ! N(2D) + NO  ==> N2 + O
          ! -----------------------------------------------------------
          rr = 6.7e-17

          Reaction = rr*Neutrals(iN2D_)*Neutrals(iNO_)

          NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          NeutralLosses(iNO_) = NeutralLosses(iNO_) + Reaction
          NeutralSources(iN2_) = NeutralSources(iN2_) + Reaction
          NeutralSources(iO_) = NeutralSources(iO_) + Reaction
          reactionrate(55) = reaction

          ! -----------------------------------------------------------
          ! N(2D) + e  ==> N(4S) + e + 2.38eV
          ! -----------------------------------------------------------
          rr = 5.5e-16 * te3 ** (0.5)
          Reaction = rr*Neutrals(iN2D_)*Ions(ie_)

          NeutralLosses(iN2D_) = NeutralLosses(iN2D_) + Reaction
          NeutralSources(iN4S_) = NeutralSources(iN4S_) + Reaction
          reactionrate(56) = reaction

          !\
          !-----------Inert Species (zeroed out sources and sinks)
          !-----------Species not calculated yet (zeroed out sources and sinks)
          !/
          ! -----------------------------------------------------------
          ! He, H:  little idea about Aij coefficients
          ! -----------------------------------------------------------
          !------------CO2 infinite reservoir assumption used-------------
          !              NeutralLosses(iCO2_) = 0.0
          !              NeutralSources(iCO2_) = 0.0
          !------------Diffusion and advection only (no chemistry presently) -------
          NeutralLosses(iAr_) = 0.0
          NeutralSources(iAr_) = 0.0
          NeutralLosses(iHe_) = 0.0
          NeutralSources(iHe_) = 0.0
          NeutralLosses(iH_) = 0.0
          NeutralSources(iH_) = 0.0

          !\
          !------------------------------------------------------+
          !-----------End Total Chemistry-------------------------------------------+
          !------------------------------------------------------+

          ReactionRateS(iLon,iLat,iAlt,:,iBlock) = reactionrate
          if (.not. UseIonChemistry) then
             IonSources = 0.0
             IonLosses = 0.0
          else
             do iIon = 1, nIons - 1
                if (.not. UseIonConstituent(iIon)) then
                   IonSources(iIon) = 0.0
                   IonLosses(iIon) = 0.0
                endif
             enddo
          endif

          if (.not. UseNeutralChemistry) then
             NeutralSources = 0.0
             NeutralLosses = 0.0
          else
             do iNeutral = 1, nSpeciesTotal
                if (.not. UseNeutralConstituent(iNeutral)) then
                   NeutralSources(iNeutral) = 0.0
                   NeutralLosses(iNeutral) = 0.0
                endif
             enddo
          endif

          ! Take Implicit time step
          Ions(ie_) = 0.0
          do iIon = 1, nIons - 1
             ionso = IonSources(iIon)
             ionlo = IonLosses(iIon)/(Ions(iIon) + 1.0e-6)
             Ions(iIon) = (Ions(iIon) + ionso*DtSub)/ &
                  (1 + DtSub*ionlo)
             ! sum for e-
             Ions(ie_) = Ions(ie_) + Ions(iIon)
          enddo

          do iNeutral = 1, nSpeciesTotal

             neuso = NeutralSources(iNeutral)
             neulo = NeutralLosses(iNeutral)/(Neutrals(iNeutral) + 0.1)

!!!
             if (Neutrals(iNeutral) == 0) &
                  write(*, *) "Neutral is zero : ", iLon, iLat, iAlt, iNeutral

             Neutrals(iNeutral) = (Neutrals(iNeutral) + neuso*DtSub)/ &
                  (1 + DtSub*neulo)

             NeutralSourcesTotal(ialt, iNeutral) = &
                  NeutralSourcesTotal(ialt, iNeutral) + &
                  NeutralSources(iNeutral)*DtSub

             NeutralLossesTotal(ialt, iNeutral) = &
                  NeutralLossesTotal(ialt, iNeutral) + &
                  NeutralLosses(iNeutral)*DtSub

          enddo
          ChemicalHeatingRate(iLon, iLat, iAlt) = &
               ChemicalHeatingRate(iLon, iLat, iAlt) + &
               ChemicalHeatingSub*DtSub
          ! +            ChemicalHeatingSubI*DtSub


          !         ChemicalHeatingRateIon(iLon,iLat,iAlt) + &

          ChemicalHeatingSpecies(iLon, iLat, iAlt, :) = &
               ChemicalHeatingSpecies(iLon, iLat, iAlt, :) + &
               ChemicalHeatingS*DtSub

          EmissionTotal = EmissionTotal + Emission(:)*DtSub

          DtTotal = DtTotal + DtSub

          if (DtSub < DtMin) DtMin = DtSub

          if (DtSub < 1.0e-9 .and. abs(DtTotal - Dt) > DtSub) then
             write(*, *) "Chemistry is too fast!!", DtSub

             ! Check Ions
             do iIon = 1, nIons
                write(*, *) "Ion Source/Loss : ", &
                     iIon, IonSources(iIon), IonLosses(iIon)
             enddo
             do iNeutral = 1, nSpeciesTotal
                write(*, *) "Neutral Source/Loss : ", iAlt, &
                     iNeutral, NeutralSources(iNeutral), &
                     NeutralLosses(iNeutral), Neutrals(iNeutral)
             enddo

             call stop_gitm("Chemistry is too fast!!")
          endif

          nIters = nIters + 1

        enddo

        IDensityS(iLon, iLat, iAlt, :, iBlock) = Ions
        NDensityS(iLon, iLat, iAlt, :, iBlock) = Neutrals

        Emissions(iLon, iLat, iAlt, :, iBlock) = &
             Emissions(iLon, iLat, iAlt, :, iBlock) + EmissionTotal

        if (DoCheckForNans) then
           do iNeutral = 1, nSpeciesTotal
              if (ieee_is_nan(Neutrals(iNeutral))) then
                 write(*, *) "chemistry : Neutral is nan", iLon, iLat, iAlt, iNeutral
                 call stop_gitm("Must stop now.")
              endif
           enddo
        endif
     enddo ! Alt
  enddo ! Lat
enddo ! Lon     

ChemicalHeatingRate(:, :, :) = &
     ChemicalHeatingRate(:, :, :)*Element_Charge/ &
     TempUnit(1:nLons, 1:nLats, 1:nAlts)/cp(1:nLons, 1:nLats, 1:nAlts, iBlock)/ &
     rho(1:nLons, 1:nLats, 1:nAlts, iBlock)

!  ChemicalHeatingRateIon(:, :, :) = &
!    ChemicalHeatingRateIon(:, :, :)*Element_Charge

ChemicalHeatingSpecies = ChemicalHeatingSpecies*Element_Charge

if (iDebugLevel > 3) then
   do iIon = 1, nIons
      write(*, *) "====> calc_chemistry: Max Ion Density: ", iIon, &
           maxval(IDensityS(1:nLons, 1:nLats, (nAlts*4)/5, iIon, iBlock))
   enddo
endif

if (iDebugLevel > 2) &
     write(*, *) "===> calc_chemistry: Average Dt for this timestep : ", &
     (Dt*nLats*nLons*nAlts)/nIters

call end_timing("calc_chemistry")

end subroutine calc_chemistry












