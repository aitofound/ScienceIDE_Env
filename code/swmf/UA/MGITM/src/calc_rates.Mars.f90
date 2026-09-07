
subroutine calc_rates(iBlock)

! ----------------------------------------------------------------------------
! Modified (01/29/07) : SWB :  Add cp, kt, km formulation from vtgcm2d codes.
!                              Variables from Mars GITM used. CGS to MKS conversion.
! Modified (02/01/07) : SWB :  Add eddy thermal conductivity (reduced by Prandtl)
! Modified (02/06/07) : SWB :  Dimensioned array math redone correctly!
! Modified (02/07/07) : SWB :  RGAS specified (cgs); access arrays explicitly with
!                              all indices inside loops
! Modified (02/20/07) : SWB :  Conversion from cgs to mks for Kt and Km corrected.
!                              Comment out write statements (no longer needed)
! Modified (10/05/07) : SWB :  Scaling-off of Km for spin-up stability fo winds.
! Modified (13/03/26) : SWB :  Sens test of Km (eddy viscosity) for minimum wind impact
! Modified (13/04/18) : SWB :  Standard Km (eddy viscosity) for modest wind impact
! Modified (08/03/17) : SWB :  Rescaling of Eddy Viscosity for doubling of Kzz
! Modified (08/21/18) : SWB :  Molecular viscosity passed (only) via global allocation
! Modified (07/31/21) : SWB :  Rescaling of Eddy Viscosity for halving of Kzz
! ----------------------------------------------------------------------------
  use ModGITM
  use ModRates
  use ModConstants
  use GITM_planet
  use ModInputs
  use ModTime, only: tsimulation

  use ModEUV, only : SunOrbitEccentricity
  use ModSources, only : KappaEddyDiffusion

  implicit none

  integer, intent(in) :: iBlock

  integer :: iAlt, iIon, iSpecies, iError, iiAlt, iLat,iLon

  real, dimension(nLons, nLats, nAlts) :: &
       Tn, Ti, TWork1, TWork2, TWork3, NO2

  real, dimension(-1:nLons+2, -1:nLats+2, -1:nAlts+2) :: &
       Ne, mnd, Te, tmp,invmnd,invNe

  real :: ScaleHeight(nLons, nLats)

  real :: e2

! ------------------------------------------------------------------------------
! cpktkm.F add-ons
  integer :: is
  real ::  rrco2,cpco2,crn,prco
  real, dimension (8) ::  cmrf, com
  real, dimension(-1:nLons+2, -1:nLats+2, -1:nAlts+2) :: &
       po, pco, pco2, pn2, cpmix, ktmix, kmmix, &
       ttot,cokm,co2kt,cokt,tt,co2km

! ------------------------------------------------------------------------------

  logical :: trouble

  call report("calc_rates",2)
  call start_timing("calc_rates")

trouble = .false.

! ------------------------------------------------------------------------------

  cmrf = (/135.8,185.7,230.4,271.0,308.3,343.3,373.7,406.1/)
  com  = (/0.3966,0.7692,1.0776,1.340,1.574,1.787,1.986,2.172/)

! -------------------------------------------------------------------------------
!     write(*,*) '1st, Some Preliminary Diagnostics for Calc_Rates ===----------------+'
!       do iLon = -1,nLons+2
!         do iLat = -1,nLats+2
!           do iAlt = -1,nAlts+2

!             if( .not. (Temperature(iLon,iLat,iAlt,iBlock) < 0.0) .and. &
!                 .not. (Temperature(iLon,iLat,iAlt,iBlock) > 0.0) ) then
!               write(*,*)'Temperature(',iLon,iLat,iAlt,iBlock,') = ',&
!                   Temperature(iLon,iLat,iAlt,iBlock)
!                   trouble = .true.
!             elseif( Temperature(iLon,iLat,iAlt,iBlock) <= -1.0e+300) then
!               write(*,*)'Temperature(',iLon,iLat,iAlt,iBlock,') = ',&
!                   Temperature(iLon,iLat,iAlt,iBlock)
!                   trouble = .true.
!             elseif(Temperature(iLon,iLat,iAlt,iBlock) >= 1.0e+300) then
!               write(*,*)'Temperature(',iLon,iLat,iAlt,iBlock,') = ',&
!                   Temperature(iLon,iLat,iAlt,iBlock)
!                   trouble = .true.
!             elseif(Temperature(iLon,iLat,iAlt,iBlock) <= 0.0) then
!               write(*,*)'Temperature(',iLon,iLat,iAlt,iBlock,') = ',&
!                   Temperature(iLon,iLat,iAlt,iBlock)
!                   trouble = .true.
!             endif

!             do iSpecies = 1,nSpecies !Total
!                  if( .not. (NDensityS(iLon,iLat,iAlt,iSpecies,iBlock) < 0.0) .and. &
!                      .not. (NDensityS(iLon,iLat,iAlt,iSpecies,iBlock) > 0.0) ) then
!               	   write(*,*)'NDensityS(',iLon,iLat,iAlt,iSpecies,iBlock,') = ',&
!                	   NDensityS(iLon,iLat,iAlt,iSpecies,iBlock)
!                    trouble = .true.
!                  elseif(NDensityS(iLon,iLat,iAlt,iSpecies,iBlock) <= -1.0e+300) then
!                    write(*,*)'NDensityS(',iLon,iLat,iAlt,iSpecies,iBlock,') = ',&
!                        NDensityS(iLon,iLat,iAlt,iSpecies,iBlock)
!                    trouble = .true.
!                  elseif(NDensityS(iLon,iLat,iAlt,iSpecies,iBlock) >= 1.0e+300) then
!                    write(*,*)'NDensityS(',iLon,iLat,iAlt,iSpecies,iBlock,') = ',&
!                        NDensityS(iLon,iLat,iAlt,iSpecies,iBlock)
!                    trouble = .true.
!                  elseif(NDensityS(iLon,iLat,iAlt,iSpecies,iBlock) <= 0.0) then
!                    write(*,*)'NDensityS(',iLon,iLat,iAlt,iSpecies,iBlock,') = ',&
!                        NDensityS(iLon,iLat,iAlt,iSpecies,iBlock)
!                    trouble = .true.
!                  endif
!             enddo !iSpecies = 1,nSpeciesTotal

!             if( .not. (NDensity(iLon,iLat,iAlt,iBlock) < 0.0) .and. &
!                 .not. (NDensity(iLon,iLat,iAlt,iBlock) > 0.0) ) then
!               write(*,*)'NDensity(',iLon,iLat,iAlt,iBlock,') = ',&
!                   NDensity(iLon,iLat,iAlt,iBlock)
!                   trouble = .true.
!             elseif( NDensity(iLon,iLat,iAlt,iBlock) <= -1.0e+300) then
!               write(*,*)'NDensity(',iLon,iLat,iAlt,iBlock,') = ',&
!                   NDensity(iLon,iLat,iAlt,iBlock)
!                   trouble = .true.
!             elseif(NDensity(iLon,iLat,iAlt,iBlock) >= 1.0e+300) then
!               write(*,*)'NDensity(',iLon,iLat,iAlt,iBlock,') = ',&
!                   NDensity(iLon,iLat,iAlt,iBlock)
!                   trouble = .true.
!             elseif(NDensity(iLon,iLat,iAlt,iBlock) <= 0.0) then
!               write(*,*)'NDensity(',iLon,iLat,iAlt,iBlock,') = ',&
!                   NDensity(iLon,iLat,iAlt,iBlock)
!                   trouble = .true.
!             endif

!           enddo
!         enddo
!      enddo

! -------------------------------------------------------------------------------


  if(trouble) then
    write(*,*) 'trouble found!!'
    write(*,*) 'Stop GITM'
    stop
  endif

! -------------------------------------------------------------------------------

  if (UseBarriers) call MPI_BARRIER(iCommGITM,iError)
  if (iDebugLevel > 4) write(*,*) "=====> mean major mass", iblock

!write(*,*) '==> calc_rates:  Before NDensityS Statements.'

  where(NDensityS(:,:,:,:,iBlock) < 1.0e3)
    NDensityS(:,:,:,:,iBlock) = 1.0e3
  end where

!write(*,*) '==> calc_rates:  Before IDensityS Statements.'

!  where(IDensityS < 1.0e3)
!    IDensityS = 1.0e3
!  end where

!write(*,*) '==> calc_rates:  Before Ne Set.'

  Ne  = IDensityS(:,:,:,ie_,iBlock)

  ! We add 1 because this is in the denominator a lot, and the corners
  ! don't have anything. Total number density.

  mnd = (NDensity(:,:,:,iBlock)+1.0)
  invmnd = 1/mnd
!write(*,*) '==> calc_rates:  Before MeanMajorMass Calculation.'

  MeanIonMass = 0.0
  MeanMajorMass = 0.0
  do iSpecies = 1, nSpecies
     MeanMajorMass = MeanMajorMass + &
          Mass(iSpecies) * &
          NDensityS(:,:,:,iSpecies,iBlock)
  enddo
  MeanMajorMass = MeanMajorMass*invmnd
!  MMM_3D(1:nLons,1:nLats,1:nAlts,iBlock) = MeanMajorMass(1:nLons,1:nLats,1:nAlts)/AMU

! Once again, in the corners, the meanmajormass is 0.

  where (MeanMajorMass == 0) MeanMajorMass = Mass(1)

  do iIon = 1, nIons - 1
    MeanIonMass = MeanIonMass + &
                  MassI(iIon)*IDensityS(:, :, :, iIon, iBlock)/ &
                  IDensityS(:, :, :, ie_, iBlock)
  enddo

! -------------------------------------------------------------------------------
  TempUnit = MeanMajorMass / Boltzmanns_Constant
! -------------------------------------------------------------------------------

!write(*,*) '==> calc_rates:  Before Mixing Ratio Calculation.'

!   Mixing Ratios needed for Kt and Km calculations below
!   Temperature Arrays needed for Kt amd Km calculation below

 ! do iAlt = 0, nAlts+1

!   Mixing Ratios
!    po(:,:,0:nAlts+1)   = NDensityS(:,:,0:nAlts+1,iO_,iBlock)*invmnd(:,:,0:nAlts+1)
!    pco(:,:,0:nAlts+1)  = NDensityS(:,:,0:nAlts+1,iCO_,iBlock)*invmnd(:,:,0:nAlts+1)
!    pn2(:,:,0:nAlts+1)  = NDensityS(:,:,0:nAlts+1,iN2_,iBlock)*invmnd(:,:,0:nAlts+1)
!    pco2(:,:,0:nAlts+1) = NDensityS(:,:,0:nAlts+1,iCO2_,iBlock)*invmnd(:,:,0:nAlts+1)

! !   Temperature Based Arrays
!    ttot(:,:,0:nAlts+1) = Temperature(:,:,0:nAlts+1,iBlock) * &
!                          TempUnit(:,:,0:nAlts+1)
!    tt(:,:,0:nAlts+1) = ttot(:,:,0:nAlts+1)**0.69

   po   = NDensityS(:,:,:,iO_,iBlock)*invmnd
   pco  = NDensityS(:,:,:,iCO_,iBlock)*invmnd
   pn2  = NDensityS(:,:,:,iN2_,iBlock)*invmnd
   pco2 = NDensityS(:,:,:,iCO2_,iBlock)*invmnd

!   Temperature Based Arrays
   ttot = Temperature(:,:,:,iBlock) * &
                         TempUnit
   tt = ttot**0.69


!  enddo

! -------------------------------------------------------------------------------

  !\
  ! These are needed for the Euv Heating and other thermodynamics:
  !/

!write(*,*) '==> calc_rates:  Before Kt, Km Calculation (after vtgcm2d).'

! -------------------------------------------------------------------------------
!---- KT, KM FORMULATION TAKEN FROM VTGCM INPUT DATASET CODE
!     ( BANKS AND KOCKARTS )
!---- KM=((PO*3.9)+(PN2*3.42))*TT*1.E-06 +(PCO*COKM)+(PCO2*CO2KM)
!---- KT=((PO*75.9)+(PN2*56.))*TT +(PCO*COKT)+(PCO2*CO2KT)
! -------------------------------------------------------------------------------
     do iLon = -1,nLons+2
        do iLat = -1,nLats+2
           do iAlt = 0, nAlts+1

! co2 factors:

          is = int((ttot(iLon,iLat,iAlt)-173.3)/100.)

          if (is <= 1) is = 1
          if (is >= 7) is = 7

          rrco2 = RGAS*AMU/Mass(iCO2_)

          if (ttot(iLon,iLat,iAlt) < 500.)    &
               crn = 1.64-(ttot(iLon,iLat,iAlt)-500.)*2.5e-4
          co2km(iLon,iLat,iAlt)=cmrf(is)+(cmrf(is+1)-cmrf(is))*  &
               (ttot(iLon,iLat,iAlt)- (is*100.+73.3))*0.01

          co2km(iLon,iLat,iAlt)=co2km(iLon,iLat,iAlt)*1.e-06
          cpco2=3.5*RGAS*AMU/Mass(iCO2_)
          co2kt(iLon,iLat,iAlt)=(cpco2-rrco2)*co2km(iLon,iLat,iAlt)*crn
!
! co factors:

          is=int(ttot(iLon,iLat,iAlt)/100.)
          if (is <= 1) is=1
          if (is >= 7) is=7
          if (ttot(iLon,iLat,iAlt) > 400.) prco=0.72
          if (ttot(iLon,iLat,iAlt) > 300. .and. ttot(iLon,iLat,iAlt) < 400.)  &
              prco = 0.73-(ttot(iLon,iLat,iAlt)-350.)*1.5e-04
          if (ttot(iLon,iLat,iAlt) < 300.)  &
                     prco=0.75-(ttot(iLon,iLat,iAlt)-250.)*2.6e-04
          cokm(iLon,iLat,iAlt)=com(is)+(com(is+1)-com(is))*  &
                     (ttot(iLon,iLat,iAlt)-100.*is)*0.01
          cokm(iLon,iLat,iAlt)=cokm(iLon,iLat,iAlt)*1.65e-04
          cokt(iLon,iLat,iAlt)=(3.5*RGAS*  &
                     cokm(iLon,iLat,iAlt))/(28.*prco)
!
! Total mixture kt and km formulation : B&K (1973) from vtgcm2d code
! *******Modified the units (cgs to mks for Mars GIM code)*******
!        kmmix: kg/m/sec units
! *******Corrected units (2/20/07); S. W. Bougher *******

          kmmix(iLon,iLat,iAlt) = ((pn2(iLon,iLat,iAlt)*3.42+   &
            po(iLon,iLat,iAlt)*3.9)*1.0e-06*tt(iLon,iLat,iAlt) +  &
            pco2(iLon,iLat,iAlt)*co2km(iLon,iLat,iAlt)+ &
            pco(iLon,iLat,iAlt)*cokm(iLon,iLat,iAlt))*0.10

          ktmix(iLon,iLat,iAlt) = ((po(iLon,iLat,iAlt)*75.9+  &
            pn2(iLon,iLat,iAlt)*56.)*tt(iLon,iLat,iAlt)+ &
            pco(iLon,iLat,iAlt)*cokt(iLon,iLat,iAlt)+ &
            pco2(iLon,iLat,iAlt)*co2kt(iLon,iLat,iAlt))* &
            1.0E-05




          enddo
        enddo
      enddo
! -------------------------------------------------------------------------------

  if (iDebugLevel > 4) write(*,*) "=====> Before cp and kappatemp", iblock

  do iAlt = 0, nAlts+1

! -------------------------------------------------------------------------------
!
!    if (Is1D .and. UseKappa1DCorrection) then
!       KappaTemp(:,:,iAlt,iBlock) = KappaTemp0 * &
!            (Temperature(1:nLons,1:nLats,iAlt,iBlock) * &
!            TempUnit(1:nLons,1:nLats,iAlt) / &
!            Kappa1DCorrectionFactor)**Kappa1dCorrectionPower
!    else
!       KappaTemp(:,:,iAlt,iBlock) = KappaTemp0 * &
!           (Temperature(1:nLons,1:nLats,iAlt,iBlock) * &
!            TempUnit(1:nLons,1:nLats,iAlt))**0.75
!    endif

        KappaTemp(:,:,iAlt,iBlock) =  ktmix(1:nLons,1:nLats,iAlt)

! -------------------------------------------------------------------------------
! This adds the eddy turbulent conduction Term (scaled by Prandtl number)
! Simplified from Yue's
! Prandtl = 10. Small to Start!
!

     do iLat = 1, nLats
        do iLon = 1, nLons
              KappaTemp(iLon,iLat,iAlt,iBlock) = &
                   KappaTemp(iLon,iLat,iAlt,iBlock) + &
                   KappaEddyDiffusion(iLon,iLat,iAlt,iBlock) * cp(iLon,iLat,iAlt,iBlock) * &
                   Rho(iLon,iLat,iAlt,iBlock)/10.


        enddo
     enddo

! -------------------------------------------------------------------------------

!   Earth GITM formulation for Molecular Viscosity (mks)
!    ViscCoef(:,:,iAlt) = 4.5e-5 * &
!         (Temperature(1:nLons,1:nLats,iAlt,iBlock)*&
!         TempUnit(1:nLons,1:nLats,iAlt)/ 1000.)**(-0.71)
!   MTGCM formulation for Molecular Viscosity requires cgs to mks conversion
!   * Scaling of Molecular Viscosity for spun-up stability
!    ViscCoef(:,:,iAlt) =  kmmix(1:nLons,1:nLats,iAlt)*10.0
!   * No scaling of molecular vciscosity
     ViscCoef(1:nLons,1:nLats,iAlt) =  kmmix(1:nLons,1:nLats,iAlt)

!  * Benchmark: Jan.-March 2013
!    ViscCoef(1:nLons,1:nLats,iAlt)  =  ViscCoef(1:nLons,1:nLats,iAlt)  + 500.0*&
!                          Rho(1:nLons,1:nLats,iAlt,iBlock)*KappaEddyDiffusion(1:nLons,1:nLats,iAlt,iBlock)
!  * Rescaled as KappaEddyDiffusion is Doubled : August 2017
!    ViscCoef(1:nLons,1:nLats,iAlt)  =  ViscCoef(1:nLons,1:nLats,iAlt)  + 250.0*&
!                          Rho(1:nLons,1:nLats,iAlt,iBlock)*KappaEddyDiffusion(1:nLons,1:nLats,iAlt,iBlock)
!  * Rescaled as KappaEddyDiffusion is Doubled plus new Testing : September 2017
!    ViscCoef(1:nLons,1:nLats,iAlt)  =  ViscCoef(1:nLons,1:nLats,iAlt)  + 200.0*&
!                          Rho(1:nLons,1:nLats,iAlt,iBlock)*KappaEddyDiffusion(1:nLons,1:nLats,iAlt,iBlock)
!  * Rescaled for GW scheme: Feb 2020
!    ViscCoef(1:nLons,1:nLats,iAlt)  =  ViscCoef(1:nLons,1:nLats,iAlt)  + 100.0*&
!                          Rho(1:nLons,1:nLats,iAlt,iBlock)*KappaEddyDiffusion(1:nLons,1:nLats,iAlt,iBlock)
! -----------------------------------------------------------------------------------------------------------
!  * Rescaled as KappaEddyDiffusion is Halved plus new Testing : June 2021  (NOGW4 option)
!    ViscCoef(1:nLons,1:nLats,iAlt)  =  ViscCoef(1:nLons,1:nLats,iAlt)  + 400.0*&
!                          Rho(1:nLons,1:nLats,iAlt,iBlock)*KappaEddyDiffusion(1:nLons,1:nLats,iAlt,iBlock)
!  * Rescaled as KappaEddyDiffusion is Halved plus new Testing plus GW drag (on) : September 2021 (YSGW4 option)
     ViscCoef(1:nLons,1:nLats,iAlt)  =  ViscCoef(1:nLons,1:nLats,iAlt)  + 150.0*&
                           Rho(1:nLons,1:nLats,iAlt,iBlock)*KappaEddyDiffusion(1:nLons,1:nLats,iAlt,iBlock)
! -----------------------------------------------------------------------------------------------------------

     Visc_3D(1:nLons,1:nLats,iAlt,iBlock) =  kmmix(1:nLons,1:nLats,iAlt)

! -------------------------------------------------------------------------------
  enddo


  call end_timing("calc_rates")

  if (UseBarriers) call MPI_BARRIER(iCommGITM,iError)
  if (iDebugLevel > 4) write(*,*) "=====> Done with calc_rates"


end subroutine calc_rates

subroutine calc_collisions(iBlock)

  use ModGITM
  use ModRates
  use ModConstants
  use GITM_planet
  use ModInputs
  use ModSources, only : KappaEddyDiffusion

  implicit none

  integer, intent(in) :: iBlock

  real, dimension(nLons, nLats, nAlts) :: e2

  integer :: iError

  real, dimension(-1:nLons+2, -1:nLats+2, -1:nAlts+2) :: &
       Ne, mnd, Te, Tr, Tn, Ti

  !\
  ! Need to get the neutral, ion, and electron temperature
  !/

  Tn = Temperature(:,:,:,iBlock)*&
       TempUnit(:,:,:)
  Ti = ITemperature(:,:,:,iBlock)
  Tr = (Tn+Ti)/2
  ! Set a floor on Tr:
  where (Tr < 200) Tr = 200.0


  mnd = NDensity(:,:,:,iBlock)+1.0
  Ne  = IDensityS(:,:,:,ie_,iBlock)

  !\
  ! -----------------------------------------------------------
  ! Collision Frequencies
  ! -----------------------------------------------------------
  !/

  e_gyro = &
       Element_Charge * B0(:,:,:,iMag_,iBlock) / Mass_Electron

  e2 = Element_Charge * Element_Charge

!
! Ion Neutral Collision Frequency (From Kelley, 1989, pp 460):
!

  if (UseBarriers) call MPI_BARRIER(iCommGITM,iError)
  if (iDebugLevel > 4) write(*,*) "=====> vin",iblock
  IonCollisions = 0.0

  Collisions(:,:,:,iVIN_) = 2.6e-15 * (mnd + Ne)/sqrt(MeanMajorMass/AMU)

 ! O+ with ... (Fox & Hac, 2014)
  IonCollisions(:,:,:,iOP_,iCO2_) = 2.0e-14*nDensityS(:,:,:,iCO2_,iBlock)
  IonCollisions(:,:,:,iOP_,iAr_) = 1.2e-14*nDensityS(:,:,:,iAr_,iBlock)
  IonCollisions(:,:,:,iOP_,iN2_) = 1.8e-14*nDensityS(:,:,:,iN2_,iBlock)
  IonCollisions(:,:,:,iOP_,iO2_) = 1.8e-14*nDensityS(:,:,:,iO2_,iBlock)
  IonCollisions(:,:,:,iOP_,iCO_) = 1.8e-14*nDensityS(:,:,:,iCO_,iBlock)
  IonCollisions(:,:,:,iOP_,iHe_) = 6.4e-15*nDensityS(:,:,:,iHe_,iBlock)
  IonCollisions(:,:,:,iOP_,iN4S_) = 9.0e-15*nDensityS(:,:,:,iN4S_,iBlock)
  IonCollisions(:,:,:,iOP_,iO_) = 6.4e-15*nDensityS(:,:,:,iO_,iBlock)

!!!!! We only advect 1 ion, so we don't need these rates.
!!!!! Update IonCollisions definition in ModGITM if more ions are advected

! ! O2+ ...
!   where (tr > 800.) IonCollisions(:,:,:,iO2P_,iO2_) = &
!        2.59e-17 * NDensityS(:,:,:,iO2_,iBlock) * tr**0.5 * &
!        (1 - 0.073*log10(tr))**2
!   where (tr <= 800) IonCollisions(:,:,:,iO2P_,iO2_) = 8.2e-16 * &
!        NDensityS(:,:,:,iO2_,iBlock)
!
!   IonCollisions(:,:,:,iO2P_,iO_) = 2.31e-16 * NDensityS(:,:,:,iO_,iBlock)
!   IonCollisions(:,:,:,iO2P_,iN2_)   = 4.13e-16 * NDensityS(:,:,:,iN2_,iBlock)
!
!
!   IonCollisions(:,:,:,iO2P_,iN4S_) = 2.64e-16 * NDensityS(:,:,:,iN4S_,iBlock)
!    ! This is (ave CO and N2)??
!   IonCollisions(:,:,:,iO2P_,iCO_)   = 4.25e-16*NDensityS(:,:,:,iCO_,iBlock)
!   IonCollisions(:,:,:,iO2P_,iCO2_)   = 4.25e-16*NDensityS(:,:,:,iCO2_,iBlock)
!
!
! ! N2+ ...
! IonCollisions(:,:,:,iN2P_,iN2_) = &
!        5.14e-17 * NDensityS(:,:,:,iN2_,iBlock) * tr**0.5 * (1 - 0.069*log10(tr))**2
!
!   IonCollisions(:,:,:,iN2P_,iO2_)   = 4.49e-16 * NDensityS(:,:,:,iO2_,iBlock)
!   IonCollisions(:,:,:,iN2P_,iO_) = 2.58e-16 * NDensityS(:,:,:,iO_,iBlock)
!   IonCollisions(:,:,:,iN2P_,iN4S_) = 2.95e-16 * NDensityS(:,:,:,iN4S_,iBlock)
!   ! This is just N2, since NO doesn't exist (ave CO and O2)
!   IonCollisions(:,:,:,iN2P_,iCO_)   = 4.66e-16 * NDensityS(:,:,:,iCO_,iBlock)
!   IonCollisions(:,:,:,iN2P_,iCO2_)   = 4.66e-16 * NDensityS(:,:,:,iCO2_,iBlock)
!
!   ! NO+ (with NO, O, N2, N, O2)
!
!   if (iDebugLevel > 5) write(*,*) "======> no+ ",iblock
!
!   ! This resonant ion-neutral is made up, since NO-NO+ doesn't exist
!   where (tr > 800.) IonCollisions(:,:,:,iNOP_,iNO_) = &
!        2.59e-17 * NDensityS(:,:,:,iNO_,iBlock) * tr**0.5 * (1 - 0.073*log10(tr))**2
!   where (tr <= 800) IonCollisions(:,:,:,iNOP_,iNO_) = 8.2e-16 * NDensityS(:,:,:,iNO_,iBlock)
!
!   IonCollisions(:,:,:,iNOP_,iO_) = 2.44e-16 * NDensityS(:,:,:,iO_,iBlock)
!   IonCollisions(:,:,:,iNOP_,iN2_)   = 4.34e-16 * NDensityS(:,:,:,iN2_,iBlock)
!   IonCollisions(:,:,:,iNOP_,iN4S_) = 2.79e-16 * NDensityS(:,:,:,iN4S_,iBlock)
!   IonCollisions(:,:,:,iNOP_,iO2_)   = 4.27e-16 * NDensityS(:,:,:,iO2_,iBlock)



!
! Electron Neutral Collision Frequency
!

  if (UseBarriers) call MPI_BARRIER(iCommGITM,iError)
  if (iDebugLevel > 4) write(*,*) "=====> ven", iblock

  Te = eTemperature(:,:,:,iBlock)
  where(te == 0.0) te = 1000.0
  Collisions(:,:,:,iVEN_) = 5.4e-16 * (mnd)*sqrt(Te)

!!!!
!!!! Electron Ion Collision Frequency
!!!!
!!!
!!!  if (UseBarriers) call MPI_BARRIER(iCommGITM,iError)
!!!  if (iDebugLevel > 4) write(*,*) "=====> vei", iblock
!!!
!!!  tmp = (34.0 + 4.18*log((TE**3.0)/(Ne*1.0e-6)))
!!!  Collisions(:,:,:,iVEI_) = tmp*Ne*TE**(-3.0/2.0) * 1.0e-6

!!  Collisions(:,:,:,VEI) = (34.0 + 4.18*log((TE**3.0)/(Ne*1.0e-6))) &
!!       * Ne * TE**(-3.0/2.0) * 1.0e-6
!

  i_gyro = Element_Charge * B0(:,:,:,iMag_,iBlock) / MeanIonMass


end subroutine calc_collisions

subroutine calc_viscosity(iBlock)

  use ModGITM

  implicit none

  integer, intent(in) :: iBlock


end subroutine calc_viscosity
