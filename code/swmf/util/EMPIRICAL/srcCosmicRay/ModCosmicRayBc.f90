!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModCosmicRay
  implicit none
  interface local_interstellar_spectrum
     module procedure local_interstellar_spectrum_s
     module procedure local_interstellar_spectrum_a
  end interface local_interstellar_spectrum

  ! Decide which model for LIS to use (usoskin or corti)
  character(LEN=18), public :: TypeLisBc = 'usoskin' 
  logical, public :: UseModulationPot = .false.
  real,    public :: ModulationPot = 0.0             ! phi, in the unit of MV
contains
  !============================================================================
  subroutine local_interstellar_spectrum_s(MomentumSi, XyzSi_D,    & ! Input
       DistTimesP2Si,                                              & ! Output
       A, Z)                                                       ! Optional

    ! INPUTS
    real, intent(in) :: MomentumSi    ! Particle momentum in Si units
    real, intent(in) :: XyzSi_D(3)    ! Where to set BC
    ! OUTPUT
    real, intent(out):: DistTimesP2Si ! Distribution function*p**2, Si
    ! OPTIONAL
    real, OPTIONAL, intent(in) :: A   ! To convert to energy per nuc
    real, OPTIONAL, intent(in) :: Z   ! To convert to rigidity
    ! LOCAL VARS
    real :: FakeMomentumArr_I(1), FakeDistTimesP2_I(1)
    !--------------------------------------------------------------------------
    FakeMomentumArr_I(1) = MomentumSi
    call local_interstellar_spectrum_a(1,FakeMomentumArr_I, XyzSi_D, & ! Input
         FakeDistTimesP2_I,                                          & ! Output
         A, Z)                                                       ! Optional
    DistTimesP2Si = FakeDistTimesP2_I(1)
  end subroutine local_interstellar_spectrum_s
  !============================================================================
  subroutine local_interstellar_spectrum_a(nP,MomentumSi_I, XyzSi_D, & ! Input
       DistTimesP2Si_I,                                              & ! Output
       A, Z)                                                         ! Optional
    ! Formulae for LISM flux by Burger et al. 2000 (doi: 10.1029/2000JA000153)
    ! and modified by Usoskin et al. 2005 (doi: 10.1029/2005JA011250)
    ! Here we refer to Eq.(2) in Usoskin et al. 2005 as default
    ! We also include Eq.(13) in Corti et al. 2019 for potential use

    use ModConst, ONLY: cGeV, cLightSpeed, cRmeProton
    use ModUtilities, ONLY: CON_stop
    ! INPUTS
    integer, intent(in) :: nP  ! The number of meshes in the logarithmic grid
    real, intent(in) :: MomentumSi_I(1:nP)   ! Particle momenta in Si units
    real, intent(in) :: XyzSi_D(3)           ! Where to set BC
    ! OUTPUT
    real, intent(out):: DistTimesP2Si_I(1:nP)! Distribution function*p**2, Si
    ! OPTIONAL
    real, OPTIONAL, intent(in) :: A          ! To convert to energy per nuc
    real, OPTIONAL, intent(in) :: Z          ! To convert to rigidity
    ! LOCAL VARS
    real :: EnergyGn_I(1:nP)                 ! E_K/A in Gev per nucleon
    real :: RigidityGv_I(1:nP)               ! R(E_K/A) in the unit of GV
    real :: EnergyLossGn                     ! Phi=Z*e/A*phi, avg. energy loss
    real :: DistLisToUpperBc_I(1:nP)         ! Factor: LIS -> ~1 AU Spectrum
    real :: DistTimesP2Gn_I(1:nP) ! Distribution function*p**2, [.../(GeV/nuc)]
    real :: Ai, Zi                ! Default values for A and Z if not present
    real, parameter :: cRmeProtonGeV = cRmeProton/cGeV
    ! For Eq.(13) in Corti et al., 2019 (doi: 10.3847/1538-4357/aafac4)
    integer         :: iFit                  ! loop variables
    real, parameter :: cFit = 5658.0, Gamma0Fit = 1.669
    real, parameter :: RigidityFit(3) = [0.572, 6.2, 540.0]
    real, parameter :: sFit(3) = [1.78, 3.89, 1.53]
    real, parameter :: DeltaFit(3) = [-4.117, -0.423, -0.26]
    !--------------------------------------------------------------------------
    Ai = 1.0; Zi = 1.0
    if(present(A)) Ai = A
    if(present(Z)) Zi = Z

    ! p|_[Si] -> E_K/A [GeV/nuc]
    EnergyGn_I = (sqrt((MomentumSi_I*cLightSpeed)**2 + &
         (Ai*cRmeProton)**2) - Ai*cRmeProton)/(cGeV*Ai)
    ! E_K/A [GeV/nuc] -> R(E_K/A) [GV]
    RigidityGv_I = energyGn_to_rigidityGv_a(EnergyGn_I)
    ! DistLisToUpperBc_I: Set UpperEndBc_I as LIS or GCR Spectrum at ~1 AU
    if(UseModulationPot) then
       ! Theoretical basis: the force field approximation in
       ! Gleeson and Axford [1968] and McCracken et al. [2004a].
       ! At the time, ModulationPot can only be set to constant value,
       ! in MV, and read from the parameter file
       ! TBD - the value of ModulationPot below may be calculated for
       ! different choices of model. The expected implementation is
       !
       ! ModulationPot = get_modulation_pot(XyzSi_D)
       !
       ! Once the modulation potential ModulationPot (phi[MV]) is known,
       ! the averaged energy loss of cosmic rays inside the heliosphere
       ! as given by Phi = (Z*e/A)*phi=Zi/Ai*ModulationPot*cMeV/cGeV
       ! to get the energy loss in [GeV/nuc]
       EnergyLossGn = Zi/Ai * ModulationPot * 1.0E-3
       DistLisToUpperBc_I = RigidityGv_I**2
       ! If UseModulationPot: we need j_LIS(R(E_K/A + Phi))
       RigidityGv_I = energyGn_to_rigidityGv_a(EnergyGn_I + EnergyLossGn)
       DistLisToUpperBc_I = DistLisToUpperBc_I/RigidityGv_I**2
    else
       DistLisToUpperBc_I = 1.0
    end if

    ! Calculate LIS based on the input of TypeLisBc
    select case(trim(TypeLisBc))
    case('usoskin')
       ! For Eq.(2) in Usoskin et al. 2005 (doi: 10.1029/2005JA011250)
       ! Formula Input: R(E_K/A + Phi)[GV]; Output: j_LIS[.../(GeV/nuc)]
       DistTimesP2Gn_I = 1.9E+4*RigidityGv_I**(-2.78)/ &
            (1.0+0.4866*RigidityGv_I**(-2.51))
    case('corti')
       ! For Eq.(13) in Corti et al. 2019 (doi: 10.3847/1538-4357/aafac4)
       ! Formula Input: R(E_K)[GV]; Output: j_LIS[.../(GV)]
       DistTimesP2Gn_I = cFit*(RigidityGv_I**Gamma0Fit)
       do iFit = 1, size(sFit)
          DistTimesP2Gn_I = DistTimesP2Gn_I *                         &
               ((1.0 + (RigidityGv_I/RigidityFit(iFit))**sFit(iFit))/ &
               (1.0 + RigidityFit(iFit)**(-sFit(iFit))))              &
               **(DeltaFit(iFit)/sFit(iFit))
       end do
       ! j_LIS[.../(GV)] -> j_LIS[.../(GeV/nuc)]
       DistTimesP2Gn_I = DistTimesP2Gn_I *             &
            Ai/abs(Zi)*(EnergyGn_I+cRmeProtonGeV)/     &
            sqrt(EnergyGn_I*(EnergyGn_I+2*cRmeProtonGeV))
    case default
       call CON_stop('Unknown type of LIS of: '//TypeLisBc)
    end select

    ! j_LIS[.../(GeV/nuc)] -> p^2*f|_{infty}[Si] -> LIS or IH spectrum
    DistTimesP2Si_I = DistTimesP2Gn_I/(Ai*cGeV)*DistLisToUpperBc_I
    ! Check any of DistLisToUpperBc_I is negative
    if (any(DistTimesP2Si_I<0.0)) call CON_stop('Negative DistUpperBc')

  contains
    !==========================================================================
    function energyGn_to_rigidityGv_a(EnergyGn_I)
      ! Turn given EnergyGn_I into RigidityGv_I
      real, intent(in) :: EnergyGn_I(1:nP)
      real :: energyGn_to_rigidityGv_a(1:nP)
      !------------------------------------------------------------------------
      energyGn_to_rigidityGv_a = Ai/abs(Zi)*           &
           sqrt(EnergyGn_I*(EnergyGn_I+2*cRmeProtonGeV))
    end function energyGn_to_rigidityGv_a
    !==========================================================================
  end subroutine local_interstellar_spectrum_a
  !============================================================================
end module ModCosmicRay
!==============================================================================
