!  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf

subroutine calc_neutral_friction_dt(DtIn, oVel, EddyCoef_1d, NDensity_1d, NDensityS_1d, &
                                 GradLogCon, EddyCoefRatio_1d, Temp, Gravity_1d )

  use ModGITM
  use ModSources
  use ModPlanet, only: Diff0, DiffExp, IsEarth
  use ModInputs, only: UseNeutralFriction, UseBoquehoAndBlelly, UseEddyCorrection

  implicit none

  real,intent(in) :: DtIn
  real,intent(inout) :: oVel(1:nAlts,1:nSpecies)
  real,intent(in) :: EddyCoef_1d(1:nAlts)
  real,intent(in) :: NDensity_1d(1:nAlts)
  real,intent(in) :: NDensityS_1d(1:nAlts,1:nSpecies)
  real,intent(in) :: GradLogCon(1:nAlts,1:nSpecies)
  real,intent(inout) :: EddyCoefRatio_1d(1:nAlts,1:nSpecies)
  real,intent(in) :: Temp(1:nAlts)
  real,intent(in) :: Gravity_1d(1:nAlts)

  integer :: iSpecies, jSpecies
  real :: CoefMatrix(nSpecies, nSpecies), kTOverM
  real :: Matrix(nSpecies, nSpecies)
  real :: Vel(nSpecies), Parity
  integer :: iPivot(nSpecies)

! Added by Jared 11-29-2007
  real :: TempDij
  real :: InvDij(nSpecies)
  real :: Dij(nSpecies)
  real :: denscale
  real :: mscale
  real :: mms
  real :: mmwos(nSpecies)

  real :: EddyDiffCorrection(nSpecies)

  integer :: iAlt

  call report("calc_neutral_friction",4)

  if (.not.UseNeutralFriction) return

  EddyCoefRatio_1d(1:nAlts,1:nSpecies) = 0.0

  do iAlt = 1, nAlts

     Vel = oVel(iAlt,1:nSpecies)
     CoefMatrix = 0.0

     mms = 0.0
     mmwos = 0.0
     InvDij = 0.0

     EddyDiffCorrection(1:nSpecies) = 0.0

     do iSpecies = 1, nSpecies

        InvDij(iSpecies) = 0.0

        kTOverM = Boltzmanns_Constant * Temp(iAlt) / Mass(iSpecies)

        denscale = 1.0/NDensity_1d(iAlt) 

        do jSpecies = 1, nSpecies

           if (jSpecies == iSpecies) cycle

! \
! Please note that TempDij is the Dij binary coefficients
! Based upon the formulation by Banks and Kokarts.
! These coefficients demand that 
! (1) NDensity be in cm^-3 (hence the 1.0e-06) factor below
! (2) Additionally, the Dij's are in cm^2/s, thus the 1.0e-04 factor

           TempDij = (1.0e-04)*&              ! Scales the Dij from cm^2/s -> m^2/s
                (   Diff0(iSpecies,jSpecies)*( Temp(iAlt)**DiffExp(iSpecies,jSpecies) )   ) / &
                (   NDensity_1d(iAlt)*(1.0e-06) )     ! Converts to #/cm^-3

           if (UseBoquehoAndBlelly) then
              CoefMatrix(iSpecies, jSpecies) = &
                   kTOverM * denscale * NDensityS_1d(iAlt, jSpecies) / &
                   (TempDij+EddyCoef_1d(iAlt))
           else 
              CoefMatrix(iSpecies, jSpecies) = &
                   kTOverM * denscale * NDensityS_1d(iAlt, jSpecies) / &
                   TempDij
           endif

           InvDij(iSpecies) = InvDij(iSpecies) + &
                denscale*NDensityS_1d(iAlt, jSpecies)/ &
                ( TempDij )

        enddo  ! End DO over jSpecies

         if (UseBoquehoAndBlelly) then
            !write(*,*) 'UseBoquehoAndBlelly =', UseBoquehoAndBlelly
            EddyCoefRatio_1d(iAlt,iSpecies) =  &
                 DtIn * EddyCoef_1d(iAlt)/&
                 (1.0/InvDij(iSpecies) + EddyCoef_1d(iAlt))*&
                 GradLogCon(iAlt,iSpecies) 
         else
            !write(*,*) 'UseBoquehoAndBlelly =', UseBoquehoAndBlelly
            EddyCoefRatio_1d(iAlt,iSpecies) =  &
                 -DtIn * (Boltzmanns_Constant*Temp(iAlt)/Mass(iSpecies))*&
                      InvDij(iSpecies)*EddyCoef_1d(iAlt)*GradLogCon(iAlt,iSpecies) 
         endif

     enddo  !End DO Over iSpecies

     Vel(1:nSpecies) = Vel(1:nSpecies) + EddyCoefRatio_1d(iAlt,1:nSpecies)

     Matrix = -DtIn*CoefMatrix

     do iSpecies = 1, nSpecies
        Matrix(iSpecies,iSpecies) = &
             1.0 + DtIn*(sum(CoefMatrix(iSpecies,:)))
     enddo

     call ludcmp(Matrix, nSpecies, nSpecies, iPivot, Parity)
     call lubksb(Matrix, nSpecies, nSpecies, iPivot, Vel)

     oVel(iAlt, 1:nSpecies) = Vel(1:nSpecies) 

  enddo

end subroutine calc_neutral_friction_dt
