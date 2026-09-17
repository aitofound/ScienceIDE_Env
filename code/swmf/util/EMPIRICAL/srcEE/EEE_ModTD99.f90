!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModExternalField
  use ModHyperGeometric, ONLY: toroid_p
  implicit none
  ! Calculate amplitudes of n=0 totoidal harmonic field OUDSIDE the filament
  ! Named indexes for the field amplitudes
  integer, parameter  :: Axial_  = 1, Poloidal_ =  2, Toroidal_  = 3
  ! For Kappa2 below this value the field is calculated as external:
  real :: Kappa2ExtMax
  !$acc declare create(Kappa2ExtMax)
contains
  !============================================================================
  subroutine external_field(Kappa2In, Amplitude_I)
    !$acc routine seq
    real, intent(in)    :: Kappa2In
    real, intent(out)   :: Amplitude_I(Axial_:Toroidal_)
    ! Eqs. 35
    !--------------------------------------------------------------------------
    Amplitude_I(Axial_)    =   toroid_p(0, Kappa2In = Kappa2In)
    Amplitude_I(Poloidal_) = 3*toroid_p(1, Kappa2In = Kappa2In)
    Amplitude_I(Toroidal_) = 0.0
  end subroutine external_field
  !============================================================================
end module ModExternalField
!==============================================================================
module ModFormFactors
  use ModHyperGeometric, ONLY: toroid_q, toroid_p, cothu
  implicit none
  ! 1/E^{(0)} and -1/E^{(1)}
  real, parameter :: cFourThirds = 1.3333333333333333, cFourFifths = 0.80
  ! Compute functions needed to describe parabolic current aand its field.
contains
  !============================================================================
  real function parabolic_current(CothU0, KappaPrime2In)
    !$acc routine seq
    real, intent(in)     :: CothU0, KappaPrime2In
    !--------------------------------------------------------------------------
    parabolic_current = CothU0 - cothu(KappaPrime2In)
  end function parabolic_current
  !============================================================================
  real function parabolic_current_e(CothU0, KappaPrime2In)
    !$acc routine seq
    real, intent(in)     :: CothU0, KappaPrime2In
    !--------------------------------------------------------------------------
    parabolic_current_e = cFourThirds*CothU0 + cFourFifths*cothu(KappaPrime2In)
  end function parabolic_current_e
  !============================================================================
  real function d_parabolic_current_e_du(KappaPrime2In)
    !$acc routine seq
    real, intent(in)     :: KappaPrime2In
    !--------------------------------------------------------------------------
    ! 1/(E^{(1)}*\sinh^2u)
    d_parabolic_current_e_du = -cFourFifths*&
         (4*KappaPrime2In/(1 - KappaPrime2In)**2)
  end function d_parabolic_current_e_du
  !============================================================================
  real function norm_uni(KappaPrime2In)
    !$acc routine seq
    real, intent(in)     :: KappaPrime2In
    real :: DToroidQ0DuU0  ! dQ^{-1}_{-1/2}(u_0)/du_0
    ! Norm_uni = -(1/(E^{(0))dQ^{-1}_{-1/2)(u_0)/du_0,  where
    ! dQ^{-1}_{-1/2)(u_0)/du_0 = 3*KappaPrime0/Kappa0**2*Q^{-1}_{1/2}(u_0)
    !--------------------------------------------------------------------------
    DToroidQ0DuU0 = 3*KappaPrime2In*sqrt(1-KappaPrime2In)&
         *toroid_q(1,KappaPrime2In=KappaPrime2In)
    norm_uni= -cFourThirds*DToroidQ0DuU0
  end function norm_uni
  !============================================================================
  real function norm_par(CothU0, KappaPrime2In)
    !$acc routine seq
    real, intent(in)     :: CothU0, KappaPrime2In
    real :: Kappa, Kappa2
    real :: ToroidQ0U0     ! Q^{-1}_{-1/2}(u_0)
    real :: DToroidQ0DuU0  ! dQ^{-1}_{-1/2}(u_0)/du_0
    !--------------------------------------------------------------------------
    Kappa2 = 1 - KappaPrime2In; Kappa = sqrt(Kappa2)
    ! Q^{-1}_{-1/2}(u_0):
    ToroidQ0U0 = Kappa2*Kappa*toroid_q(0,KappaPrime2In=KappaPrime2In)
    ! dQ^{-1}_{-1/2)(u_0)/du_0=3*KappaPrime0/Kappa0**2*Q^{-1}_{1/2}(u_0)
    DToroidQ0DuU0  = &
         3*KappaPrime2In*Kappa*toroid_q(1,KappaPrime2In=KappaPrime2In)
    ! Norm_par = Q^{-1}_{-1/2}(u_0)*di_E(u_0)/du_0 - i_E(u_0)*dQ/du
    norm_par= ToroidQ0U0*d_parabolic_current_e_du(KappaPrime2In)          &
         - DToroidQ0DuU0*parabolic_current_e(CothU0,KappaPrime2In)
  end function norm_par
  !============================================================================
  real function q_0(KappaPrime2In)
    !$acc routine seq
    real, intent(in)     :: KappaPrime2In
    !--------------------------------------------------------------------------
    q_0 = 0.125*toroid_p(0, KappaPrime2In=KappaPrime2In)/&
         toroid_q(0,KappaPrime2In=KappaPrime2In)
  end function q_0
  !============================================================================
end module ModFormFactors
!==============================================================================
module  ModParabolicCurrent
  use ModExternalField, ONLY: &
       Axial_, Poloidal_, Toroidal_, Kappa2ExtMax
  use ModFormFactors
  implicit none
  PRIVATE  ! Except

  ! Constant  factors  to calculate internal field

  ! Value of coth(u^-_0)
  real :: CothU0
  ! Q01*dQ^{-1}_{-1/2}(u^-_0)/du^-_0
  real :: DPsiMinusJEOverDu0Minus
  ! 1/(Q^{-1}_{-1/2}(u_0)*di_E(u_0)/du_0 - i_E(u_0)*dQ/du)
  real :: CurrentFactor
  real :: Q01               ! Field factor for parabolic current
  !$acc declare create(CothU0,DPsiMinusJEOverDu0Minus,CurrentFactor,Q01)

  public:: set_kappaprime0  ! Set constant coefficients for given KappaPrime0
  public:: get_amplitude_int! Internal field amplitudes
  public:: current          ! current profile
contains
  !============================================================================
  subroutine set_kappaprime0(KappaPrime0In)
    real, intent(in)  :: KappaPrime0In
    ! \kappa^\prime^2 at the  boundary
    real :: KappaPrime02
    ! At the boundary of the external field region
    real :: KappaPrime2Ext, KappaExtMax
    real  :: ToroidQ0U0Minus ! Q^{-1}_{-1/2}(u_0)
    !--------------------------------------------------------------------------
    KappaPrime02 = KappaPrime0In**2
    ! External boundary
    KappaPrime2Ext = KappaPrime02
    Kappa2ExtMax = 1 - KappaPrime2Ext
    KappaExtMax  = sqrt(Kappa2ExtMax)

    ! Calculate Coth(u_0)
    CothU0 = cothu(KappaPrime2Ext)
    ! 1/(Q^{-1}_{-1/2}(u_0)*di_E(u_0)/du_0 - i_E(u_0)*dQ/du)
    CurrentFactor = 1/norm_par(CothU0,KappaPrime2Ext)

    ! Q^{-1}_{-1/2}(u_0):
    ToroidQ0U0minus = Kappa2ExtMax*KappaExtMax*&
         toroid_q(0,KappaPrime2In=KappaPrime2Ext)
    ! Eq. ??, constant field factor for parabolic current
    Q01 = q_0(KappaPrime2Ext)                                                &
         - parabolic_current_e(CothU0,KappaPrime2Ext)*CurrentFactor/         &
         ToroidQ0U0Minus
    ! dQ^{-1}_{-1/2)(u_0)/du_0=3*KappaPrime0/Kappa0**2*Q^{-1}_{1/2}(u_0)
    DPsiMinusJEOverDu0Minus  = Q01*                                          &
         3*KappaPrime2Ext*KappaExtMax*toroid_q(1,KappaPrime2In=KappaPrime2Ext)

    !$acc update device(CothU0,DPsiMinusJEOverDu0Minus,CurrentFactor,Q01)
    !$acc update device(Kappa2ExtMax)
  end subroutine set_kappaprime0
  !============================================================================
  subroutine get_amplitude_int(KappaPrime2In, Amplitude_I)
    !$acc routine seq
    real, intent(in)    :: KappaPrime2In
    real, intent(out)   :: Amplitude_I(Axial_:Toroidal_)
    !
    ! Misc
    !
    real :: Kappa, Kappa2, Kappa3, PsiMinusJE, DPsiMinusJEOverDu
    !--------------------------------------------------------------------------
    Kappa2 = 1 - KappaPrime2In; Kappa = sqrt(Kappa2); Kappa3 = Kappa2*Kappa

    PsiMinusJE = Q01*Kappa3*toroid_q(0,KappaPrime2In = KappaPrime2In)
    ! Eqs. 35
    Amplitude_I(Axial_)    = PsiMinusJE + CurrentFactor*                     &
         parabolic_current_e(CothU0,KappaPrime2In)
    DPsiMinusJEOverDu  =                                                     &
         Q01*3*KappaPrime2In*Kappa*toroid_q(1,KappaPrime2In=KappaPrime2In)
    Amplitude_I(Poloidal_) = (DPsiMinusJEOverDu + CurrentFactor*             &
         d_parabolic_current_e_du(KappaPrime2In))*Kappa2/KappaPrime2In
    Amplitude_I(Toroidal_) = sqrt(8*max(0.0, -0.4*(CurrentFactor*            &
         parabolic_current(CothU0,KappaPrime2In))**2 +                       &
         CurrentFactor*(PsiMinusJE*parabolic_current(CothU0,KappaPrime2In) - &
         cFourThirds*(DPsiMinusJEOverDu - DPsiMinusJEOverDu0Minus))  ))
  end subroutine get_amplitude_int
  !============================================================================
  real function current(KappaPrime2In)
    !$acc routine seq
    real, intent(in)    :: KappaPrime2In
    !--------------------------------------------------------------------------
    current = CurrentFactor*parabolic_current(CothU0,KappaPrime2In)
  end function current
  !============================================================================
end module ModParabolicCurrent
!==============================================================================
module ModUniformCurrent
  use ModExternalField, ONLY: &
       Axial_, Poloidal_, Toroidal_, Kappa2ExtMax, external_field
  use ModFormFactors
  implicit none
  PRIVATE  ! Except
  real :: Eps = 0.1
  ! Parabolic current at
  ! KappaPrime0^2*(1 - 2*Eps) < KappaPrime2 < KappaPrime0^2*(1 + 2*Eps)
  ! KappaPrime2Uniform < KappaPrime2 < KappaPrime2Ext = 1 - Kappa2ExtMax
  real :: KappaPrime2Uniform
  !
  ! Constant  factors  to calculate internal field
  !
  real :: CothU0        ! Value of coth(u^-_0)
  real :: DeltaInv      ! 1/(coth(u^-_0) - coth(u^+_0))
  ! Two contributions to the normalization integrals, for u^-_0 and u^+_0
  real :: NormMinus, NormPlus
  ! Normalization coefficient for current
  real :: CurrentFactor
  real :: CurrentE      ! Uniform current
  !
  ! Field factors
  real :: Q01           ! Field factor for parabolic current
  real :: Q1            ! Field factor for uniform current
  !
  ! Constants determining toroidal field:
  !
  real :: DPsiMinusJEOverDu0Minus   ! For parabolic current
  real :: PsiMinusJEUOPlus          ! For uniform current
  real :: Toroidal2                 ! b^2(u^+_0)

  !$acc declare create(Eps, KappaPrime2Uniform, CothU0, DeltaInv, NormMinus)
  !$acc declare create(NormPlus, CurrentFactor, CurrentE, Q01, Q1)
  !$acc declare create(DPsiMinusJEOverDu0Minus, PsiMinusJEUOPlus, Toroidal2)

  public:: set_kappaprime0  ! Set constant coefficients for given KappaPrime0
  public:: get_amplitude_int! Internal field amplitudes
  public:: current          ! current profile
contains
  !============================================================================
  subroutine set_kappaprime0(KappaPrime0In,EpsIn)
    real, intent(in)  :: KappaPrime0In
    real, optional, intent(in) :: EpsIn
    ! \kappa^\prime^2 at the  boundary
    real :: KappaPrime02
    ! At the boundary of the external field region
    real :: KappaPrime2Ext, KappaExtMax
    ! At the boundary of the uniform current region:
    real :: Kappa2Uniform, KappaUniform
    real :: ToroidQ0U0Minus ! Q^{-1}_{-1/2}(u^-_0)
    real :: ToroidQ0U0Plus  ! Q^{-1}_{-1/2}(u^+_0)
    real :: Amplitude_I(Axial_:Toroidal_)
    !--------------------------------------------------------------------------
    Eps = 0.1
    if(present(EpsIn)) Eps = EpsIn
    KappaPrime02 = KappaPrime0In**2
    ! External boundary
    KappaPrime2Ext = (1 + 2*Eps)*KappaPrime02
    Kappa2ExtMax = 1 - KappaPrime2Ext
    KappaExtMax  = sqrt(Kappa2ExtMax)
    ! Boundary of the uniform current region
    KappaPrime2Uniform = KappaPrime02*(1 - 2*Eps)
    Kappa2Uniform = 1 - KappaPrime2Uniform
    KappaUniform = sqrt(Kappa2Uniform)
    if(Eps > 0.0)then
       ! Calculate Coth(u_0)
       CothU0 = cothu(KappaPrime2Ext)
       ! 1/(coth u^-_0 - coth u^+_0)
       DeltaInv = 1/(CothU0 - cothu(KappaPrime2Uniform))
       ! Different contributions to the normalization integral
       NormMinus = DeltaInv*norm_par(CothU0,KappaPrime2Ext)
       NormPlus  = - DeltaInv*norm_par(CothU0,KappaPrime2Uniform) +        &
            norm_uni(KappaPrime2Uniform)
       ! Normalization factor for currents:
       CurrentFactor = 1/(NormMinus + NormPlus)
       ! Normalize currents contributing to the normalization integrals
       NormPlus = CurrentFactor*NormPlus; NormMinus = CurrentFactor*NormMinus
       ! Uniform current
       CurrentE = cFourThirds*CurrentFactor
       ! Q^{-1}_{-1/2}(u_0):
       ToroidQ0U0minus = Kappa2ExtMax*KappaExtMax*&
            toroid_q(0,KappaPrime2In=KappaPrime2Ext)

       ! Constant field factor for parabolic current
       Q01 = q_0(KappaPrime2In=KappaPrime2Ext)*NormMinus                    &
            -  parabolic_current_e(CothU0,KappaPrime2Ext)*DeltaInv*         &
            CurrentFactor/ToroidQ0U0Minus
       ! Constant for toroidal field and parabolic current
       DPsiMinusJEOverDu0Minus  =  3*KappaPrime2Ext*KappaExtMax*(           &
            Q01*toroid_q(1,KappaPrime2In=KappaPrime2Ext) +                  &
            0.125*NormPlus*toroid_p(1,KappaPrime2In = KappaPrime2Ext) )
       ToroidQ0U0Plus = Kappa2Uniform*KappaUniform*                         &
            toroid_q(0,KappaPrime2In=KappaPrime2Uniform)
       Q1 = Q01 + NormPlus*q_0(KappaPrime2Uniform) +                        &
            (parabolic_current_e(CothU0,KappaPrime2Uniform)*DeltaInv*       &
            CurrentFactor - CurrentE)/ToroidQ0U0Plus
       ! Constants determining toroidal field:
       PsiMinusJEUOPlus = Q1*ToroidQ0U0Plus
       call get_amplitude_int(1.0000003*KappaPrime2Uniform,Amplitude_I)
       Toroidal2 = Amplitude_I(Toroidal_)**2
    else
       ToroidQ0U0Plus = Kappa2Uniform*KappaUniform*&
            toroid_q(0,KappaPrime2In=KappaPrime2Uniform)
       ! Eq. 36, constant field factor for uniform current
       CurrentFactor = 1/norm_uni(KappaPrime2Uniform)
       ! Constant current I_E = 1/( (3/4)*Norm_Uni
       CurrentE = cFourThirds*CurrentFactor
       Q1 = q_0(KappaPrime2uniform) - CurrentE/ToroidQ0U0Plus
       ! Constants determining toroidal field:
       PsiMinusJEUOPlus = Q1*ToroidQ0U0Plus
       Toroidal2 = 0.0
    end if

  !$acc update device(Eps, KappaPrime2Uniform, CothU0, DeltaInv, NormMinus)
  !$acc update device(NormPlus, CurrentFactor, CurrentE, Q01, Q1)
  !$acc update device(DPsiMinusJEOverDu0Minus, PsiMinusJEUOPlus, Toroidal2)
  !$acc update device(Kappa2ExtMax)

  end subroutine set_kappaprime0
  !============================================================================
  subroutine get_amplitude_int(KappaPrime2In, Amplitude_I)
    !$acc routine seq
    real, intent(in)    :: KappaPrime2In
    real, intent(out)   :: Amplitude_I(Axial_:Toroidal_)
    !
    ! Misc
    !
    real :: Kappa, Kappa2, Kappa3, PsiMinusJE, DPsiMinusJEOverDu
    !--------------------------------------------------------------------------
    Kappa2 = 1 - KappaPrime2In; Kappa = sqrt(Kappa2); Kappa3 = Kappa2*Kappa
    if(KappaPrime2In > KappaPrime2Uniform)then
       PsiMinusJE = Kappa3*(Q01*toroid_q(0,KappaPrime2In = KappaPrime2In) +  &
            0.125*NormPlus*toroid_p(0,KappaPrime2In = KappaPrime2In) )
       Amplitude_I(Axial_)    = PsiMinusJE + CurrentFactor*DeltaInv*         &
            parabolic_current_e(CothU0,KappaPrime2In)
       DPsiMinusJEOverDu  =  3*KappaPrime2In*Kappa*(                         &
            Q01*toroid_q(1,KappaPrime2In=KappaPrime2In) +                    &
            0.125*NormPlus*toroid_p(1,KappaPrime2In = KappaPrime2In) )
       Amplitude_I(Poloidal_) = (DPsiMinusJEOverDu + CurrentFactor*DeltaInv* &
            d_parabolic_current_e_du(KappaPrime2In))*Kappa2/KappaPrime2In
       Amplitude_I(Toroidal_) = sqrt(8*max(0.0, -0.4*(CurrentFactor*DeltaInv*&
            parabolic_current(CothU0,KappaPrime2In))**2 + DeltaInv*          &
            CurrentFactor*(PsiMinusJE*parabolic_current(CothU0,KappaPrime2In)&
            - cFourThirds*(DPsiMinusJEOverDu - DPsiMinusJEOverDu0Minus))  ))
    else
       PsiMinusJE = Q1*Kappa3*toroid_q(0,KappaPrime2In = KappaPrime2In)
       Amplitude_I(Axial_)    = PsiMinusJE + CurrentE
       Amplitude_I(Poloidal_) = 3*Q1*Kappa3*toroid_q(1,                      &
            KappaPrime2In=KappaPrime2In)
       Amplitude_I(Toroidal_) = sqrt(Toroidal2 + 8*max(0.0,                  &
            CurrentFactor*(PsiMinusJE - PsiMinusJEUOPlus)))
    end if
  end subroutine get_amplitude_int
  !============================================================================
  real function current(KappaPrime2In)
    !$acc routine seq
    real, intent(in)    :: KappaPrime2In
    !--------------------------------------------------------------------------
    if(KappaPrime2In > KappaPrime2Uniform)then
       current = DeltaInv*CurrentFactor*parabolic_current(CothU0,KappaPrime2In)
    else
       current = 1*CurrentFactor
    end if
  end function current
  !============================================================================
end module ModUniformCurrent
!==============================================================================
module ModSurfaceCurrent
  use ModExternalField, ONLY:   &
       Axial_, Poloidal_, Toroidal_, Kappa2ExtMax
  use ModFormFactors
  implicit none
  PRIVATE  ! Except
  ! \kappa^\prime at the  boundary
  real :: KappaPrime0
  !
  ! Constant  factor  to calculate the internal field
  !
  real :: Q0        ! Eq. 31, constant field factor for surface current
  !$acc declare create(KappaPrime0, Q0)

  public:: set_kappaprime0  ! Set constant coefficients for given KappaPrime0
  public:: get_amplitude_int! Internal field amplitudes
contains
  !============================================================================
  subroutine set_kappaprime0(KappaPrime0In)
    real, intent(in)  :: KappaPrime0In
    real :: KappaPrime02
    !--------------------------------------------------------------------------
    KappaPrime0  = KappaPrime0In
    KappaPrime02 = KappaPrime0**2
    Kappa2ExtMax      = 1 - KappaPrime02
    ! Eq. 31, constant field factor for surface current
    Q0 = q_0(KappaPrime2In=KappaPrime02)
    !$acc update device(KappaPrime0, Q0)
  end subroutine set_kappaprime0
  !============================================================================
  subroutine get_amplitude_int(KappaPrime2In, Amplitude_I)
    !$acc routine seq
    real, intent(in)    :: KappaPrime2In
    real, intent(out)   :: Amplitude_I(Axial_:Toroidal_)
    !
    ! Misc
    !
    real :: Kappa2, Kappa3
    !--------------------------------------------------------------------------
    Kappa2 = 1 - KappaPrime2In; Kappa3 = sqrt(Kappa2)*Kappa2

    ! Eqs. 35
    Amplitude_I(Axial_)    =   Q0*Kappa3*toroid_q(0, KappaPrime2In = &
         KappaPrime2In)
    Amplitude_I(Poloidal_) = 3*Q0*Kappa3*toroid_q(1, KappaPrime2In=  &
         KappaPrime2In)
    Amplitude_I(Toroidal_) = 0.0
  end subroutine get_amplitude_int
  !============================================================================
end module ModSurfaceCurrent
!==============================================================================
module ModCurrentFilament
  use ModUniformCurrent, ONLY: &
       uniform_current_field=>get_amplitude_int, uniform_current=>current
  use ModSurfaceCurrent, ONLY: &
       surface_current_field=>get_amplitude_int
  use ModParabolicCurrent, ONLY: &
       parabolic_current_field=>get_amplitude_int, parabolic_current=>current
  use ModExternalField, ONLY: Kappa2ExtMax, external_field, &
       Axial_, Poloidal_, Toroidal_
  implicit none
  !
  ! Radius of toroidal  maagnetic axis and its square
  !
  real  :: rInfty, rInfty2
  ! \kappa^Prime at the  boundary
  real :: KappaPrime0
  !
  ! Logicals determining if we use manufactured  current distrributions
  ! 1. Uniform current form-factor:
  logical :: UseUniformCurrent = .false.
  ! 2. Surface current form-factor
  logical :: UseSurfaceCurrent = .false.
  ! 3. Parabolic current formfactor
  logical :: UseParabolicCurrent = .false.
  ! Inductunce  coefficient; the ratio of the total inductance of the filament
  ! in the SI units ormalized by \mu_0 R_\infty
  real :: Inductance = 0.0

  !$acc declare create(rInfty, rInfty2, KappaPrime0, Inductance)
contains
  !============================================================================
  subroutine set_filament_geometry(rMinor, rMajor, EpsIn)
    use ModUniformCurrent, ONLY: set_uniform_current=>set_kappaprime0
    use ModSurfaceCurrent, ONLY: set_surface_current=>set_kappaprime0
    use ModParabolicCurrent, ONLY: set_parabolic_current=>set_kappaprime0
    use ModHypergeometric, ONLY: l0_ext_inductance

    real, intent(in) :: rMinor, rMajor
    real, optional, intent(in):: EpsIn
    !--------------------------------------------------------------------------
    rInfty2 = rMajor**2 -  rMinor**2; rInfty = sqrt(rInfty2)
    KappaPrime0 = rMinor/(rMajor + rInfty)
    Kappa2ExtMax  = -1.0
    if(UseUniformCurrent)then
       call set_uniform_current(KappaPrime0,EpsIn)
       ! Calculate inductance depending on choice of the current form-factor
       ! Inductance includes external and internal field iductances as well as
       ! the torooidal field inductance:
       !                      external               internal  toroidal
       Inductance = l0_ext_inductance(KappaPrime0**2) + 0.25 + 0.5
    end if
    if(UseSurfaceCurrent)then
       call set_surface_current(KappaPrime0)
       ! Only external inductance matters:
       Inductance = l0_ext_inductance(KappaPrime0**2)
    end if
    if(UseParabolicCurrent)then
       call set_parabolic_current(KappaPrime0)
       ! Only external inductance matters:
       Inductance = l0_ext_inductance(KappaPrime0**2)
    end if
    if(Kappa2ExtMax <= 0.0) then
       ! With no foorm-factor, the field is always external, since for any
       ! \kappa one has \kappa^2 < Kappa_0^2=1
       Kappa2ExtMax = 1.0; rInfty = rMajor; rInfty2  = rInfty**2
       ! Approximate formula expressed in terms of a/R0 ratio
       Inductance =  log(8.0*rMajor/rMinor) - 1.25
       ! Set \kappa^\prime = a/(2R_0)
       KappaPrime0 = 0.5* rMinor / rMajor
    end if
    !$acc update device(rInfty, rInfty2, KappaPrime0, Inductance, Kappa2ExtMax)
  end subroutine set_filament_geometry
  !============================================================================
end module ModCurrentFilament
!==============================================================================
module ModFieldGS
#ifdef _OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use ModCurrentFilament
  use ModNumConst, ONLY:  cTwoPi
  implicit none
  ! Magnitude of the magnetic field at the center of filament:
  real, private :: Bc      ! In SI, Bc = \mu_0 I / (2 R_\infty)
  ! Magnetic field vector at the center of filament: Bc times
  ! the unit vector of the axis direction
  real, private :: Bc_D(3)
  ! The sign of ratio between always positive toroidal current density  and
  ! the toridal magnetic field, which may be either positive or negative
  ! everywhere
  integer, private :: iHelicity

  !$acc declare create(Bc, Bc_D, iHelicity)
contains
  !============================================================================
  subroutine set_filament_field(iHelicityIn, BcIn_D)
    use ModNumConst, ONLY:  cTwoPi
    ! Inputs:
    ! The sign of helicity (+/- 1)
    integer, intent(in)  :: iHelicityIn
    ! Magnetic field vector at the center of filament:
    ! Bc (= \mu_0 I / (2 R_\infty) if the SI system of units is used)
    ! times the unit direction vector of the  axis of symmetry
    real, intent(in) :: BcIn_D(3)
    !--------------------------------------------------------------------------
    iHelicity = iHelicityIn
    Bc_D = BcIn_D
    Bc = norm2(Bc_D)
    !$acc update device(Bc, Bc_D, iHelicity)
  end subroutine set_filament_field
  !============================================================================
  subroutine get_filament_field(R_D, B_D, BetaIn, BPhiOut)
    !$acc routine seq

    ! Caalculates:
    ! 1. With preset
    !      UseUniformCurrent = .true.
    !      call set_filament_geometry(a, R0)
    !      call set_filament_field(Bc, BDir_D)
    !    this subroutine calculates the field vector B_D at the point R_D for
    !    the unoform current form-factor (GS22)
    ! 2. With preset UseUniformCurrent = .false. (default) this subroutine
    !    calculates the external field in the TD solution. It should not be
    !    applied inside the filament
    use ModCoordTransform, ONLY: cross_product
    real, intent(in ) :: R_D(3)
    real, intent(out) :: B_D(3)
    real, optional, intent(in )  ::  BetaIn
    real, optional, intent(out)  ::  BPhiOut
    !
    ! Misc: geometry
    !
    real :: R2, Z2, rPerp2, rPerp, RPlus2, Kappa2, KappaPrime2, RMinus2
    !
    ! Msic: field characteristics
    !
    real :: BcDotR, CommonFactor, BPhi
    real :: Poloidal_D(3)  !  Not a unit vector, its length is Bc*KappaPrime
    real :: Amplitude_I(Axial_:Toroidal_)
    !--------------------------------------------------------------------------
    !
    ! Geometry
    !
    R2 = sum(R_D**2)
    BcDotR  = sum(Bc_D*R_d)
    Z2= (BcDotR/Bc)**2
    rPerp2  = R2 - Z2; rPerp =  sqrt(max(0.0,rPerp2))
    RPlus2 = Z2 +  (rInfty + rPerp)**2
    Poloidal_D = ( 2*BcDotR*R_D + (rInfty2 - R2)*Bc_D )/RPlus2
    Kappa2 = 4*rPerp*rInfty/RPlus2
    if(Kappa2 <= Kappa2ExtMax) then
       ! external field
       call external_field(Kappa2, Amplitude_I)
       CommonFactor = (sqrt(rInfty2/RPlus2))**3
       B_D = CommonFactor*(Bc_D*Amplitude_I(Axial_) + &
            Poloidal_D*Amplitude_I(Poloidal_))
       ! No toroidal external field:
       if(present(BPhiOut)) BPhiOut = 0.0
    else
       ! internal field
       !
       ! Calculate (\kappa^\prime)^2:
       ! 1. Calculate R_-.
       RMinus2 = Z2 +  (rInfty - rPerp)**2
       ! Exprress (\kappa^\prime)^2 via R_- (not via Kappa2 = 1 - KappaPrime2):
       KappaPrime2 = RMinus2 / RPlus2
       ! Field harmonics
       call uniform_current_field(KappaPrime2, Amplitude_I)
       ! Calculate separately the toroidal field:
       BPhi = iHelicity*Amplitude_I(Toroidal_)
       if(present(BetaIn)) BPhi = BPhi/sqrt(1 + BetaIn)
       CommonFactor = (sqrt(rInfty/rPerp))**3
       B_D = CommonFactor*(Bc_D*Amplitude_I(Axial_)    + &
            Poloidal_D*Amplitude_I(Poloidal_) + &
            (BPhi / rPerp)*cross_product(Bc_D, R_D) )
       if(present(BPhiOut)) BPhiOut = CommonFactor*BPhi*Bc
    end if
  end subroutine get_filament_field
  !============================================================================
  subroutine test
    use ModPlotFile
    integer, parameter :: nStep = 400
    integer, parameter :: AxialS_ = 1, PoloidalS_ = 2, &
         AxialU_ = 3, PoloidalU_ = 4,  &
         AxialP_ = 1, PoloidalP_ = 2, AxialLD_ = 3, PoloidalLD_=4
    real   , parameter :: DeltaKappaPrime = 0.00050
    integer            :: iLoop
    real               :: R0, a, Amplitude_I(Axial_:Toroidal_), &
         KappaPrime, KappaPrime2, Kappa2, Kappa3
    real :: Var_VI(AxialS_:PoloidalU_,nStep), Coord_I(nSTep)
    ! Currents:
    integer, parameter :: Uniform_=1, Parabolic_ = 2, Merged_ = 3
    real :: Current_VI(Uniform_:Merged_,nStep)
    real :: Toroidal_VI(Uniform_:Merged_,nStep)
    ! Loop variables
    integer:: i, j
    ! Number of points = (2N+1)*(2N+1)
    integer, parameter :: N = 300, JMin = N/10
    ! Field to show:
    real :: Field_DII(Axial_:Toroidal_, -N:N, -N:N)
    ! Domain size (-ZMax to ZMax)
    real, parameter:: ZMax = 2.0
    ! Grid size
    real, parameter :: DGrid = ZMax/N
    real, parameter :: Lcharge = 0.7
    real :: QCharge
    real :: R_D(3), BStrap_D(3)
    ! Internal field
    ! Parameters on the plasma filament surface at which \kappa^\prime_0 = 0.1
    !--------------------------------------------------------------------------
    a  = 0.20/0.990;  R0 = 1.010/0.990; Current_VI = 0.0; Toroidal_VI = 0.0
    UseUniformCurrent = .true.;  UseSurfaceCurrent = .false.
    call set_filament_geometry(a, R0, 0.0)
    do iLoop = 1, nStep
       !
       ! \kappa^\prime ranges from 0 to \kappa^\prime_0 = 0.1
       KappaPrime     = iLoop*DeltaKappaPrime
       Coord_I(iLoop) = KappaPrime
       Kappa2     = 1 - KappaPrime**2
       if(Kappa2<=Kappa2ExtMax)then
          Kappa3 = sqrt(Kappa2)*Kappa2
          call  external_field(Kappa2In = Kappa2, Amplitude_I=Amplitude_I)
          Var_VI(AxialU_,iLoop)    = 0.125*Kappa3*Amplitude_I(Axial_)
          Var_VI(PoloidalU_,iLoop) = 0.125*Kappa3*KappaPrime*&
               Amplitude_I(Poloidal_)
       else
          !
          ! Internal field from the uniform current
          !
          call uniform_current_field(KappaPrime2In = KappaPrime**2, &
               Amplitude_I=Amplitude_I)
          ! Eqs. 35
          Var_VI(AxialU_,iLoop)    = Amplitude_I(Axial_)
          Var_VI(PoloidalU_,iLoop) = KappaPrime*Amplitude_I(Poloidal_)
          Toroidal_VI(Uniform_,iLoop) = Amplitude_I(Toroidal_)
          Current_VI(Uniform_,iLoop) = uniform_current(KappaPrime**2)
       end if
    end do
    UseUniformCurrent = .false.;  UseSurfaceCurrent = .true.
    call set_filament_geometry(a, R0)
    do iLoop = 1, nStep
       !
       ! \kappa^\prime ranges from 0 to \kappa^\prime_0 = 0.1
       KappaPrime     = iLoop*DeltaKappaPrime
       Coord_I(iLoop) = KappaPrime
       Kappa2     = 1 - KappaPrime**2
       if(Kappa2<=Kappa2ExtMax)then
          Kappa3 = sqrt(Kappa2)*Kappa2
          call  external_field(Kappa2In = Kappa2, Amplitude_I=Amplitude_I)
          Var_VI(AxialS_,iLoop)    = 0.125*Kappa3*Amplitude_I(Axial_)
          Var_VI(PoloidalS_,iLoop) = 0.125*Kappa3*KappaPrime*&
               Amplitude_I(Poloidal_)
       else
          !
          ! Internal field from the uniform current
          !
          call surface_current_field(KappaPrime2In = KappaPrime**2, &
               Amplitude_I=Amplitude_I)
          ! Eqs. 35
          Var_VI(AxialS_,iLoop)    = Amplitude_I(Axial_)
          Var_VI(PoloidalS_,iLoop) = KappaPrime*Amplitude_I(Poloidal_)
       end if
    end do
    call save_plot_file(NameFile='test_fields.out', &
         TypeFileIn='ascii'                     ,&
         NameVarIn=&
         'kappa_prime AxialS PoloidalS AxialU PoloidalU', &
         StringFormatIn = '(5es18.10)'          ,&
         Coord1In_I = Coord_I                   ,&
         VarIn_VI = Var_VI(AxialS_:PoloidalU_,1:nStep))
    UseSurfaceCurrent = .false.;  UseParabolicCurrent = .true.
    call set_filament_geometry(a, R0)
    do iLoop = 1, nStep
       !
       ! \kappa^\prime ranges from 0 to \kappa^\prime_0 = 0.1
       KappaPrime     = iLoop*DeltaKappaPrime
       Coord_I(iLoop) = KappaPrime
       Kappa2     = 1 - KappaPrime**2
       if(Kappa2<=Kappa2ExtMax)then
          Kappa3 = sqrt(Kappa2)*Kappa2
          call  external_field(Kappa2In = Kappa2, Amplitude_I=Amplitude_I)
          Var_VI(AxialP_,iLoop)    = 0.125*Kappa3*Amplitude_I(Axial_)
          Var_VI(PoloidalP_,iLoop) = 0.125*Kappa3*KappaPrime*&
               Amplitude_I(Poloidal_)
       else
          !
          ! Internal field from the parabolic current
          !
          call parabolic_current_field(KappaPrime2In = KappaPrime**2, &
               Amplitude_I=Amplitude_I)
          ! Eqs. 35
          Var_VI(AxialP_,iLoop)    = Amplitude_I(Axial_)
          Var_VI(PoloidalP_,iLoop) = KappaPrime*Amplitude_I(Poloidal_)
          Toroidal_VI(Parabolic_,iLoop) = Amplitude_I(Toroidal_)
          Current_VI(Parabolic_,iLoop) = parabolic_current(KappaPrime**2)
       end if
    end do
    UseParabolicCurrent = .false.;  UseUniformCurrent = .true.
    call set_filament_geometry(a, R0, 0.1)
    do iLoop = 1, nStep
       !
       ! \kappa^\prime ranges from 0 to \kappa^\prime_0 = 0.1
       KappaPrime     = iLoop*DeltaKappaPrime
       Coord_I(iLoop) = KappaPrime
       Kappa2     = 1 - KappaPrime**2
       if(Kappa2<=Kappa2ExtMax)then
          Kappa3 = sqrt(Kappa2)*Kappa2
          call  external_field(Kappa2In = Kappa2, Amplitude_I=Amplitude_I)
          Var_VI(AxialLD_,iLoop)    = 0.125*Kappa3*Amplitude_I(Axial_)
          Var_VI(PoloidalLD_,iLoop) = 0.125*Kappa3*KappaPrime*&
               Amplitude_I(Poloidal_)
       else
          call uniform_current_field(KappaPrime2In = KappaPrime**2, &
               Amplitude_I=Amplitude_I)
          ! Eqs. 35
          Var_VI(AxialLD_,iLoop)    = Amplitude_I(Axial_)
          Var_VI(PoloidalLD_,iLoop) = KappaPrime*Amplitude_I(Poloidal_)
          Toroidal_VI(Merged_,iLoop) = Amplitude_I(Toroidal_)
          Current_VI(Merged_,iLoop) = uniform_current(KappaPrime**2)
       end if
    end do
    call save_plot_file(NameFile='test_currents.out', &
         TypeFileIn='ascii'                     ,&
         NameVarIn=&
         'kappa_prime Uniform Parabolic Merged', &
         StringFormatIn = '(4es18.10)'          ,&
         Coord1In_I = Coord_I                   ,&
         VarIn_VI = Current_VI)
    call save_plot_file(NameFile='test_toroidal.out', &
         TypeFileIn='ascii'                     ,&
         NameVarIn=&
         'kappa_prime Uniform Parabolic Merged', &
         StringFormatIn = '(4es18.10)'          ,&
         Coord1In_I = Coord_I                   ,&
         VarIn_VI = Toroidal_VI)
    call save_plot_file(NameFile='test_parabolic.out', &
         TypeFileIn='ascii'                     ,&
         NameVarIn=&
         'kappa_prime AxialP PoloidalP AxialLD PoloidalLD', &
         StringFormatIn = '(5es18.10)'          ,&
         Coord1In_I = Coord_I                   ,&
         VarIn_VI = Var_VI)
    UseUniformCurrent = .true.;  UseParabolicCurrent = .false.
    ! Set rInfty = 1 and KappaPrime  = 0.1
    call set_filament_geometry(0.20 / 0.990, 1.01 / 0.990, 0.1)
    call set_filament_field(+1, [1.0, 0.0, 0.0])
    do j = -N, N
       do i = -N, N
          call get_filament_field(R_D=[DGrid*i, DGrid*j, 0.0],&
               B_D = Field_DII(:, i, j))
       end do
    end do
    call save_plot_file(NameFile='field_gs22.out', &
         TypeFileIn='ascii', &
         NameVarIn='z r Bz Br BPhi'  , &
         CoordMinIn_D=[-ZMax, -ZMax],&
         CoordMaxIn_D=[ ZMax,  ZMax],&
         StringFormatIn = '(6F12.5)',&
         VarIn_VII = Field_DII )
    ! Test the strapped field:
    BStrap_D =  - (Inductance/cTwoPi)*Bc_D
    do j = -N, N
       do i = -N, N
          Field_DII(:, i, j) = Field_DII(:, i, j) + BStrap_D
       end do
    end do
    call save_plot_file(NameFile='strapped_field.out', &
         TypeFileIn='ascii', &
         NameVarIn='z r Bz Br BPhi'  , &
         CoordMinIn_D=[-ZMax, -ZMax],&
         CoordMaxIn_D=[ ZMax,  ZMax],&
         StringFormatIn = '(6F12.5)',&
         VarIn_VII = Field_DII )
    ! Express magnetic charge in terms of BStrap:
    ! 2L/(L^2 +R_\infty^2)^{3/2}*QCharge = -BStrap
    QCharge = norm2(BStrap_D)*0.50*(Lcharge**2 + 1)**1.50/Lcharge
    ! Test the strapped field created by a pair of magnetic charges:
    do j = JMin, N
       do i = -N/2, N/2
          ! Eliminate uniform BStrap
          Field_DII(:, i, j) = Field_DII(:, i, j) - BStrap_D
          ! Radius vector:
          R_D = [DGrid*i, DGrid*j, 0.0]
          ! Add  strapping field of twoo magnetic charges
          Field_DII(:, i, j) = Field_DII(:, i, j) + &
               QCharge*(R_D - [Lcharge, 0.0, 0.0])/&  ! Positive charge
               norm2(R_D - [Lcharge, 0.0, 0.0])**3 &  ! field
               -QCharge*(R_D + [Lcharge, 0.0, 0.0])/& ! Negative charge
               norm2(R_D + [Lcharge, 0.0, 0.0])**3    ! field
       end do
    end do
    call save_plot_file(NameFile='charge_field.out', &
         TypeFileIn='ascii', &
         NameVarIn='z r Bz Br BPhi'  , &
         CoordMinIn_D=[-0.5*ZMax, 0.1*ZMax],&
         CoordMaxIn_D=[ 0.5*ZMax,  ZMax],&
         StringFormatIn = '(6F12.5)',&
         VarIn_VII = Field_DII(:,-N/2:N/2,JMin:N ) )
  end subroutine test
  !============================================================================
end module ModFieldGS
!==============================================================================
module EEE_ModTD99

#ifdef _OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use ModCoordTransform, ONLY: cross_product
  use ModUtilities, ONLY: CON_stop
  use EEE_ModCommonVariables, ONLY: UseTD, UseTD14, UseTD22, DirCme_D, &
       No2Si_V, Si2No_V, Io2No_V, Io2Si_V, No2Io_V,                    &
       UnitB_, UnitRho_, UnitX_,  UnitU_, UnitP_, UnitTemperature_,    &
       iProc, tStartCme
  use ModHyperGeometric
  use ModConst, ONLY: cMu, cDegToRad, cRadToDeg, cPi, cTwoPi
  use ModFieldGS, ONLY: set_filament_geometry, set_filament_field, &
       get_filament_field, KappaPrime0, UseUniformCurrent, Inductance, rInfty
  implicit none
  save
  private

  public :: init_td99_parameters
  public :: set_parameters_td99
  public :: get_td99_fluxrope
  public :: compute_td99_bqfield
  public :: get_td99_size

  ! Variables related to the position of the flux rope:

  ! Major radius (in fact - R_\infty, the radius of the circumference
  ! at which the toroidal coordinate, u, tends to infinity
  real :: RTube  = 0.0

  ! Minor radius:
  real :: aTube = 0.0

  ! Negative height (depth) of the current tube center with respect to
  ! the photosphere level.
  real :: Depth = 0.0

  ! direction of the magnetic moment of the configuration
  real :: UnitX_D(3)

  ! Magnetic configuration center
  real :: XyzCenter_D(3)
  !$acc declare create(RTube, aTube, Depth, XyzCenter_D, UnitX_D)

  ! magnetic field of the current tube at the center of configuration
  real :: BcTube, BcTubeDim, Itube
  !$acc declare create(BcTube, BcTubeDim, Itube)

  ! Magnetic field at the center of configuration is always parallel,
  ! while starpping field is antiparallel, to the x-axis. Phi-conponent
  ! of the toroidal current is positive. However, the sign of the toroidal
  ! field component may be both positive and negative. To account for
  ! this, we introduce the variable to store the helicity sign.
  integer :: iHelicity = +1
  !$acc declare create(iHelicity)

  ! To set the negative helicity, the input parameter, BcTube, should be
  ! negative. This choice affects only the sign of helicity, but not the
  ! direction of the nagnetic field at the center of configuration.

  !-------------------Parameters of plasma inside the rope------------

  logical :: UsePlasmaBeta = .false.
  ! If ON, the parameter Beta, having a meaning of a constant
  ! gaskinetic-to-magnetic pressure
  ! ratio, is set for the ejecta, as well as the ejecta temperature. In this
  ! case both density and pressure are set inside the flux rope. Otherwise, the
  ! density only is calculated in agreeement with the input parameter of mass.
  real    :: PlasmaBeta = 0.0
  real    :: EjectaTemperature, EjectaTemperatureDim = 5.0e4 ! 50,000 K
  ! If the ejecta is cold ( PlasmaBeta = 0.0), the total mass of ejecta is
  ! set as the initial condition.
  real :: MassSi
  real :: Rho0=0.0
  !$acc declare create(UsePlasmaBeta, PlasmaBeta, EjectaTemperature)
  !$acc declare create(EjectaTemperatureDim, MassSi, Rho0)

  ! There is an alternative way to parameterize the current in the loop:
  ! namely, via the strength of the overarching (strapping) field. With
  ! this choice, the current is determined by the condition of equilibrium
  !`with the given strapping field intensity. The possible options are:
  ! readbstrap (the starapping field intensity is provided in the parameter
  ! file, getbstrap (in this case the field is taken from the MHD solution).
  character(LEN=10) :: TypeBStrap = 'none'

  ! The following logical is set to .true. if TypeBStrap = 'getbstrap'
  logical :: DoGetBStrap = .false.

  ! If .true. the current of the configuration is found from the
  ! equilibrium condition at the given strapping field
  logical :: UseEquilibriumCurrent = .false.

  real    :: bStrapping = 0.0, bStrappingDim = 0.0

  ! MAGNETIC CHARGES

  character(LEN=10) :: TypeCharge = 'none'

  ! magnetic charge and its distance from the current ring
  real :: q = 0.0,  qDistance = 0.0, bQStrappingDim, bQStrapFraction
  !$acc declare create(q, qDistance, bQStrappingDim, bQStrapFraction)

  ! coordinate vectors of the charge location
  real    :: RPlus_D(3), RMinus_D(3)
  real    :: UChargeX = 0.0
  !$acc declare create(RPlus_D, RMinus_D, UChargeX)

  ! If .true. in the initial distribution of the magnetic field the
  ! strapping field from two magnetic charges
  logical :: UseStaticCharge = .false.
  !$acc declare create(UseStaticCharge)

  ! If .true., the magnetic field from two magnetic charges is varied
  ! dynamically, via varied B0Field
  logical :: UseDynamicStrapping = .false.
  !$acc declare create(UseDynamicStrapping)

contains
  !============================================================================
  subroutine set_parameters_td99(NameCommand)
    use EEE_ModCommonVariables, ONLY: &
         XyzCmeApexSi_D, XyzCmeCenterSi_D, DoNormalizeXyz
    use ModReadParam, ONLY: read_var

    real    :: MassDim
    character(len=*), intent(in):: NameCommand

    character(len=*), parameter:: NameSub = 'set_parameters_td99'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case("#CME")
       ! Set defaults:
       UseEquilibriumCurrent = .false.
       DoGetBStrap = .false.
       UseStaticCharge = .false.
       UseDynamicStrapping = .false.
       TypeBStrap = 'none'
       TypeCharge = 'none'
       ! Set negative helicity, if the input is negative:
       call read_var('iHelicity', iHelicity)

       call read_var('RadiusMajor', Rtube)
       call read_var('RadiusMinor', aTube)
       call read_var('Depth',       Depth)
       !
       ! Save coords of the configuration center...
       XyzCmeCenterSi_D = DirCme_D*(1 - Depth)
       !
       ! ...and those of apex:
       XyzCmeApexSi_D = XyzCmeCenterSi_D + DirCme_D*RTube
       ! The coords are dimensionless, set the switch to normalize
       DoNormalizeXyz = .true.
       if(UseTD22)then
          UsePlasmaBeta = .true.
          call read_var('PlasmaBeta', PlasmaBeta)
          call read_var('EjectaTemperature', EjectaTemperatureDim)
       elseif(.not.UseTD14)then
          call read_var('UsePlasmaBeta', UsePlasmaBeta)
          if(UsePlasmaBeta)then
             call read_var('PlasmaBeta', PlasmaBeta)
             call read_var('EjectaTemperature', EjectaTemperatureDim)
          else
             PlasmaBeta = 0.0
             call read_var('MassDim',     MassDim)
             MassSi = MassDim*1.0e-3  ! g to kg
          end if
          ! If UseTD14 all these parameters are not effective
       end if
       call read_var('TypeBStrap', TypeBStrap)

       select case(TypeBStrap)
       case('readbstrap')
          UseEquilibriumCurrent  = .true.
          call read_var('bStrappingDim', bStrappingDim)
          call read_var('TypeCharge', TypeCharge)

       case('getbstrap')
          UseEquilibriumCurrent  = .true.
          DoGetBStrap = .true.
          call read_var('TypeCharge', TypeCharge)

       case('none')
          call read_var('BcTubeDim', BcTubeDim)
       case('steady','moving','cancelflux')
          UseEquilibriumCurrent  = .false.
          TypeCharge = TypeBStrap
          call read_var('BcTubeDim', BcTubeDim)
       case default
          if(iProc==0)call CON_stop(NameSub// ': '//&
               trim(TypeBStrap)//' is unknown TypeBStrap')
       end select

       ! The rest is only read if TypeCharge is not "none"
       if(TypeCharge /= "none")then
          !  if(UseEquilibriumCurrent)then
          ! In this case we read, which fraction of the above
          ! equilibrium strapping field is due to magnetic charges
          call read_var('bQStrapFraction', bQStrapFraction)
          ! else
          ! The magnetude of magnetic charges is characterized in terms
          ! of the strapping field they produce at the apex of flux rope
          !   call read_var('bQStrappingDim', bQStrappingDim)
          ! end if
          call read_var('qDistance',  qDistance)

          select case(TypeCharge)
          case('steady')
             UseStaticCharge = .true.
          case('cancelflux')
             UseDynamicStrapping = .true.
             tStartCme = -1.0
             call read_var('UChargeX', UChargeX)
          case('moving')
             UseStaticCharge = .true.
             UseDynamicStrapping = .true.
             tStartCme = -1.0
             call read_var('UChargeX', UChargeX)
          case default
             if(iProc==0)call CON_stop(&
                  NameSub//': TypeCharge='//trim(TypeCharge)//' is unknown')
          end select
       end if
    case default
       call CON_stop(NameSub//' unknown NameCommand='//NameCommand)
    end select

  end subroutine set_parameters_td99
  !============================================================================
  subroutine init_td99_parameters

    use EEE_ModCommonVariables, ONLY: prefix, OrientationCme,  &
         LongitudeCme, LatitudeCme, BAmbientApexSi_D
    use ModCoordTransform, ONLY: rot_xyz_mercator, rot_matrix_z

    real :: AlphaRope, Linduct, WFRope, FootSepar, ItubeSi

    ! Rotational matrix of coordinate transformation:
    real :: Rotate_DD(3,3)

    character(len=*), parameter:: NameSub = 'init_td99_parameters'
    !--------------------------------------------------------------------------
    Rtube = Rtube*Io2No_V(UnitX_)
    aTube = aTube*Io2No_V(UnitX_)
    Depth = Depth*Io2No_V(UnitX_)
    if(UseTD22) UseUniformCurrent = .true.
    ! Improved computation of inductance
    if(.not.UseTD14)call set_filament_geometry(aTube, Rtube)
    ! Account for finite beta for TD22:
    if(UseTD22)Inductance = Inductance + PlasmaBeta/(1 + PlasmaBeta)
    if (iProc==0) then
       write(*,'(a)') prefix
       if(UseTD14)then
          write(*,'(a)') prefix//&
               '    Twisted Flux Rope Model by Titov, 2014.     '
       elseif(UseTD22)then
          write(*,'(a)') prefix//&
               ' Finite-Beta Twisted Flux Rope Derived from GS Equation, 2022.'
       else
          write(*,'(a)') prefix//&
               '    Twisted Flux Rope Model by Titov & Demoulin, 1999.     '
       end if
       write(*,'(a)') prefix
       write(*,'(a,es13.5,a)') prefix//'Depth  = ', &
            Depth*No2Si_V(UnitX_)/1.0E6,'[Mm]'
       write(*,'(a,es13.5,a)') prefix//'Rtube  = ', &
            Rtube*No2Si_V(UnitX_)/1.0E6,'[Mm]'
       write(*,'(a,es13.5,a)') prefix//'aTube  = ', &
            aTube*No2Si_V(UnitX_)/1.0E6,'[Mm]'
       write(*,'(a,es13.5,a)') prefix//'atube/Rtube = ',aTube/Rtube,'[-]'
       if(UseTD22)then
          write(*,'(a,es13.5,a)') prefix//'rInfty  = ', &
               rInfty*No2Si_V(UnitX_)/1.0E6,'[Mm]'
          write(*,'(a,es13.5,a)') prefix//'KappaPrime0 = ',KappaPrime0,'[-]'
       end if
       write(*,'(a)') prefix
    end if

    ! Computation of the portion of the flux rope current that is above
    ! the solar surface:
    if(UseTD22)then
       ! More accurate formula for R_S = Io2No_V(UnitX_)
       if(rInfty >= Depth)then
          AlphaRope = 2*acos((2.0*(1.0*Io2No_V(UnitX_))*Depth - Depth**2 &
               -rInfty**2)/(2*(Io2No_V(UnitX_) - Depth)*rInfty))
       else
          ! Only valid for testing the entire loop
          AlphaRope = cTwoPi
       end if
    else
       ! Approximate formula at Depth<<R_S and rInfty<<R_S
       AlphaRope  = 2.0*acos(Depth/Rtube)                 ! in [rad]
    end if
    FootSepar  = Rtube*No2Si_V(UnitX_)*sin(0.5*AlphaRope)/1.0e6  ! in [Mm]
    if(iProc==0)then
       write(*,'(a,es13.5,a)') prefix//'Separation of flux rope ends is ',&
            FootSepar,'[Mm],'
       write(*,'(a,es13.5,a)') prefix//'   or ',AlphaRope*cRadToDeg,'[deg].'
       write(*,'(a)') prefix
    end if
    !
    ! Construct the rotational matrix Rotate_DD,
    ! Rotate to the local Mercator map around the CME center and then rotate
    ! about the vertical direction to align x-axis with the direction from
    ! positive to negative spot.

    Rotate_DD  = matmul(rot_xyz_mercator(DirCme_D), &
         rot_matrix_z(OrientationCme*cDegToRad))

    ! In the rotated frame magnetic field at the center of configuration
    ! is antiparallel to x-axis
    UnitX_D = matmul(Rotate_DD, [-1.0, 0.0, 0.0])

    XyzCenter_D = (1 - Depth)*DirCme_D
    if(UseTD22)then
       EjectaTemperature =  EjectaTemperatureDim*Io2No_V(UnitTemperature_)
       if(iProc==0)then
          write(*,'(a,es13.5,a)') prefix//'Beta   =',PlasmaBeta
          write(*,'(a,es13.5,a)') prefix//'Tejecta=',&
               EjectaTemperatureDim,'[K]'
          write(*,'(a)') prefix
       end if
    elseif(.not.UseTD14)then
       if(.not.UsePlasmaBeta)then
          !
          ! Compute the average density inside the flux rope assuming that the
          ! total amount of prominence mass is Mass (=10^16g=10^13kg):
          Rho0  = MassSi/(AlphaRope*Rtube*cPi*aTube**2*No2Si_V(UnitX_)**3)
          !
          ! Convert density from [kg/m^3] to dimensionless units
          Rho0  = Rho0*Si2No_V(UnitRho_)
          if(iProc==0)then
             write(*,'(a,es13.5,a)') prefix//'Mass   = ',MassSi*1.0e3,'[g] '
             write(*,'(a,es13.5,a)') prefix//'Rho0   = ',&
                  Rho0*No2Io_V(UnitRho_),'[g/cm^3]'
             write(*,'(a)') prefix
          end if
       else
          ! Alternatively, if non-zero PlasmaBeta is used, the pressure
          ! is profiled proportionally to the total pressure, and the density
          ! is found from the plasma pressure and constant ejecta temperature:
          EjectaTemperature =  EjectaTemperatureDim*Io2No_V(UnitTemperature_)
          if(iProc==0)then
             write(*,'(a,es13.5,a)') prefix//'Beta   =',PlasmaBeta
             write(*,'(a,es13.5,a)') prefix//'Tejecta=',&
                  EjectaTemperatureDim,'[K]'
             write(*,'(a)') prefix
          end if
       end if
    end if

    if(UseEquilibriumCurrent)then
       if(DoGetBStrap)then
          ! Calculate projection of the MHD field at the apex point
          ! anti-parallel to the X-axis
          bStrapping = - sum(BAmbientApexSi_D*UnitX_D)*Si2No_V(UnitB_)
          if(bStrapping < 0.0)call CON_stop(NameSub//&
               ': Strapping MHD field is negative')
       else
          ! Normalize field read from PARAM.in file
          bStrapping = bStrappingDim*Io2No_V(UnitB_)
       end if
       if(.not.UseTD14)then
          ! Compute the equilibrium toroidal current, Itube, based
          ! on the force balance in direction normal to the surface of
          ! the flux tube.
          ! Itube = 8*cPi*q*qDistance*Rtube &
          !     *(qDistance**2+Rtube**2)**(-1.5) &
          !     /(alog(8.0*Rtube/aTube) -1.5 + Li/2.0)    =
          !     4*cPi*(2*q*qDistance/(qDistance**2 + RTube**2))*RTube/&
          !     /(alog(8.0*Rtube/aTube) -1.5 + Li/2.0)    =
          !     4*cPi*bStrapping*RTube/(alog(8.0*Rtube/aTube) -1.5 + Li/2.0)
          ! From here, we can calculate BcTube = Itube/(2*RTube)
          ! in [-]
          BcTube  = 2*cPi*bStrapping/ Inductance
          ! Invert the equation, BcTubeSi = 0.5 \Mu_0 * ItubeSi/ RTubeSi
          ItubeSi = 2*(BcTube*No2Si_V(UnitB_))*&
               (RTube*No2Si_V(UnitX_))/cMu ![A]
       else
          ! Use equation from TD14:
          Itube =  (4*cPi*RTube*bStrapping) / (log(8*RTube/aTube)-25./24.)
          ItubeSi = Itube*No2Si_V(UnitB_)*No2Si_V(UnitX_)/cMu ![A]
       end if
       if (iProc==0) then
          write(*,'(a,es13.5,a)') prefix//' The strapping field, bStrapping=',&
               bStrapping*No2Io_V(UnitB_),' [Gs] is introduced and'
          write(*,'(a)') prefix//' EQUILIBRIUM value of Itube is computed!!!'
          write(*,'(a)') prefix
          write(*,'(a,es13.5,a)') prefix//' The value of Itube is reset to: ',&
               ItubeSi,' [A]'
          write(*,'(a)') prefix
       endif
    else
       ! Normalize field read from parameter file
       BcTube= BcTubeDim*Io2No_V(UnitB_)
       ! Invert the equation, BcTube = 0.5  * Itube/ RTube
       Itube   = 2*BcTube*RTube
       ItubeSi = Itube*No2Si_V(UnitB_)*No2Si_V(UnitX_)/cMu ![A]
       if(iProc==0)then
          write(*,'(a,es13.5,a)') prefix//'Itube  = ',ItubeSi,'[A]'
          write(*,'(a)') prefix
       end if
       ! Equilibrium bStrapping
       bStrapping = BcTube*Inductance/( 2*cPi)
    end if
    if(iProc==0)then
       write(*,'(a,i2)') prefix//'Sign of helicity is ', iHelicity
       write(*,'(a)') prefix
    end if

    Linduct = cMu*(AlphaRope/cTwoPi)*Rtube*No2Si_V(UnitX_)*&
         Inductance    ! in [H]
    WFRope  = 0.5*Linduct*ItubeSi**2*1.0e7             ! in [ergs]

    if(iProc==0)then
       write(*,'(a,es13.5,a)') prefix//'Free energy of flux rope is ',&
            WFRope,'[erg].'
       write(*,'(a)') prefix
    end if
    ! Set parameters of the filament field
    call set_filament_field(iHelicity, BcTube*UnitX_D)
    if(UseStaticCharge.or.UseDynamicStrapping)then
       ! Strapping field of magnetic charges:
       qDistance = qDistance*Io2No_V(UnitX_)
       ! Invert formula:
       ! bQStrapFraction*bQStrapping = 2*q*qDistance &
       !     *(qDistance**2+Rtube**2)**(-1.5)
       q = bQStrapFraction*bStrapping*(qDistance**2 + Rtube**2)**1.50/&
            (2*qDistance)
       if(iProc==0)then
          write(*,'(a)') prefix//&
               ' The field of point magnetic charges is introduced'
          write(*,'(a,es13.5,a)') prefix//'q      = ', &
               q*No2Si_V(UnitB_)*No2Si_V(UnitX_)**2,'[T m^2]'
          write(*,'(a,es13.5,a)') prefix//'L      = ',&
               qDistance*No2Si_V(UnitX_)/1.0e6,'[Mm]'
          write(*,'(a)') prefix
       end if
       RPlus_D  = XyzCenter_D + qDistance*UnitX_D
       RMinus_D = XyzCenter_D - qDistance*UnitX_D

       if(UseDynamicStrapping)then
          UChargeX = UChargeX*Io2Si_V(UnitU_) &   ! From km/s to m/s
               *Si2No_V(UnitX_)                   ! To R_sun per s
          if(iProc==0)then
             write(*,'(a,es13.5,a)') prefix//&
                  'tStartCme = ',tStartCme,'[s]'
             write(*,'(a,es13.5,a)') prefix//&
                  'Velocity of converging charges = ',&
                  UChargeX,'[R_s/s]'
             write(*,*) prefix
          end if ! iProc==0
       endif  ! UseDynamicStrapping
    end if  ! UseCharge

    !$acc update device(RTube, aTube, Depth, XyzCenter_D, UnitX_D)
    !$acc update device(UsePlasmaBeta, PlasmaBeta, EjectaTemperature)
    !$acc update device(EjectaTemperatureDim, MassSi, Rho0)
    !$acc update device(UseStaticCharge, iHelicity, UseDynamicStrapping)
    !$acc update device(BcTube, BcTubeDim, Itube)

    !$acc update device(q, qDistance, bQStrappingDim, bQStrapFraction)
    !$acc update device(RPlus_D, RMinus_D, UChargeX)

  end subroutine init_td99_parameters
  !============================================================================
  subroutine get_td99_fluxrope(Xyz_D, BFRope_D, RhoFRope, pFluxRope)
    !$acc routine seq

    !    Twisted Magnetic Field Configuration by Titov & Demoulin '99   !
    !                                                                   !
    ! An instability that causes a CME eruption is expected to occur at !
    ! R > L*sqrt(2). For a detailed description of the initial state    !
    ! refer to A&A, 1999, v.351, pp.707-720                             !
    !                                                                   !
    ! ___  This module was written by Ilia Roussev on June 10, 2002 ___ !

    real, intent(in)  :: Xyz_D(3)
    real, intent(out) :: BFRope_D(3)
    real, intent(out) :: RhoFRope
    real, intent(out) :: pFluxRope

    ! Coordinates relative to the configuration center:
    real :: XyzRel_D(3)

    ! Distance to the configuration center squared:
    real :: R2Rel !=sum(XyzRel_D**2)

    real:: xRel
    real:: RMinus, RPlus2, Rperp
    real:: Kappa, Kappa2
    real:: BPhi, B1qField_D(3)

    ! Initialize the TD99 model parameters once::

    !--------------------------------------------------------------------------

    !
    ! Calculate the field of thin toroidal current filament
    !

    !--------------------------------------------------------------------
    ! Assign X,Y,Z coordinates at which to compute the magnetic field::
    !
    ! xRel - coordinate along the axis of symmetry, equal to 0 at the
    ! plane of symmetry.
    ! zzz - is the heliocetric coordinate along the line passing through
    ! the center of configuration, which is at the depth d below the
    ! photosphere level.
    ! yyy coordinate is equal to zero at the axis of symmetry.
    XyzRel_D = Xyz_D - XyzCenter_D
    if(UseTD22)then
       call get_filament_field(&
            R_D = XyzRel_D,         &
            B_D = BFRope_D,         &
            BetaIn = PlasmaBeta,    &
            BPhiOut = BPhi)
       pFluxRope = PlasmaBeta*(0.50*BPhi**2)
       RhoFRope = pFluxRope/EjectaTemperature
    else
       R2Rel = sum(XyzRel_D**2); xRel = sum(XyzRel_D*UnitX_D)

       ! Compute Rperp and TubalDist::

       Rperp = sqrt(R2Rel - xRel**2)
       RMinus = sqrt(xRel**2 + (Rperp - Rtube)**2)
       RPlus2 = (Rperp + Rtube)**2 + xRel**2

       ! Define the model input, Kappa

       Kappa2 = 4.0*Rperp*Rtube/RPlus2; Kappa = sqrt(Kappa2)
       if(.not.UseTD14)then
          call td99(XyzRel_D, R2Rel, xRel, RMinus, RPlus2, Rperp, &
          Kappa, Kappa2, BFRope_D, RhoFRope, pFluxRope)
       else
          call tdm(XyzRel_D, R2Rel, xRel, RMinus, RPlus2, Rperp, &
          Kappa, Kappa2, BFRope_D, RhoFRope, pFluxRope)
       end if
    end if

    ! Add the field of two magnetic charges
    if (UseStaticCharge) then
       call compute_td99_BqField(Xyz_D, B1qField_D)
       BFRope_D = BFRope_D + B1qField_D
    endif

    BFRope_D  = BFRope_D *No2Si_V(UnitB_)
    RhoFRope  = RhoFRope *No2Si_V(UnitRho_)
    pFluxRope = pFluxRope*No2Si_V(UnitP_)

  end subroutine get_td99_fluxrope
  !============================================================================
  subroutine td99(XyzRel_D, R2Rel, xRel, RMinus, RPlus2, Rperp, &
     Kappa, Kappa2, BFRope_D, RhoFRope, pFluxRope)
    !$acc routine seq

    real, intent(in) :: XyzRel_D(3)
    real, intent(in) :: R2Rel, xRel, RMinus, RPlus2, Rperp, Kappa, Kappa2
    real, intent(out) :: BFRope_D(3)
    real, intent(out) :: RhoFRope, pFluxRope

    real:: DKappaDx, DKappaDr
    real:: KappaA, KappaA2, DKappaAdr

    ! Vector potential related variables::
    real:: Ak, dAkdk, D2Akdk2A
    real:: AI, dAIdx, dAIdr
    ! Flux-rope related variables::
    real:: BIPhi_D(3)
    !--------------------------------------------------------------------------
    if (RMinus >= aTube) then
       !
       ! No pressure and density outside flux rope
       RhoFRope = 0.0; pFluxRope = 0.0
       ! Compute the field outside the current torus
       !
       call get_filament_field(XyzRel_D, BFRope_D)
       BIPhi_D = 0.0
    else
       ! Compute the field and density inside the current torus
       ! 1.
       ! Define the model input, KappaA. A given point is characterized by
       ! two coordinates, say, RPepr and RMinus. In this case,
       ! if we denote
       ! Kappa = function(RPerp,RMinus): i.e. Kappa**2 = 4*RPerp*RTube *R_+**2
       ! then
       ! KappaA = function(RPepr,ATube), or
       KappaA2 = 4.0*Rperp*Rtube/(4.0*Rperp*Rtube + aTube**2)
       KappaA  = sqrt(KappaA2)
       ! 2.
       ! The vector potential of the externap field, Ak and its derivative,
       ! dAk/dk, are both prolonged to the tube interior using the following
       ! sewing function (below we use A_k by a factor of Kappa**3 different
       ! from what we used above:
       ! A_i = A_k(KappaA) + dA_k/dKappaA*(Kappa - Kappa_A) (*)
       Ak      = toroid_P(0, Kappa2In=KappaA2)*KappaA**3
       dAkdk   = 3*toroid_p(1,Kappa2In=KappaA2)*KappaA2
       AI = Ak + dAkdk*(Kappa - KappaA)
       ! 3.
       ! Now, we derive the field components in terms of x and rPerp
       ! derivatives. Function A_i depends on x only via Kappa, so that:
       ! dA_i/dx = dA_k/dKappaA*dKappa/dx:
       dKappadx  = 2.0*xRel*Kappa/RPlus2
       dAIdx   = dAkdk*dKappadx
       ! 4.
       ! Analogously we account for the dependence of Kappa on the radial
       ! coordinate:
       ! dA_i/dr = dA_k/dKappaA*dKappaA/dr:
       dKappadr  = Kappa*(Rtube**2 - R2Rel)/RPlus2
       dAIdr   = dAkdk*dKappadr
       ! 5.
       ! Now, we account for the dependence of KappaA on rPerp. From (*), the
       ! contributions from the first derivative dA_k(KappaA)/dKappaA cancel
       ! each other, so that only the second derivative matters, which is
       ! equal to  d^2A_k(KappaA)/dKappaA^2:
       d2Akdk2A = &
            KappaA/(1 - KappaA**2)*(3*toroid_P(0, Kappa2In=KappaA**2) +&
            3*toroid_p(1,Kappa2In=KappaA**2)*(1 + KappaA**2))
       dKappaAdr = KappaA*aTube**2/&
            (4.0*Rperp*Rtube + aTube**2)
       dAIdr = dAIdr + d2Akdk2A*dKappaAdr*(Kappa - KappaA)

       BFRope_D =  BcTube/(Kappa**3)*(Rtube/sqrt(RPlus2))**3*&
            (dAIdx*XyzRel_D + (dAIdr + AI)*UnitX_D)
       ! Compute the toroidal field (BIphix, BIphiy, BIphiz)
       ! produced by the azimuthal current Iphi. This is needed to ensure
       ! that the flux rope configuration is force free.
       BIPhi_D = iHelicity*BcTube*RTube/(cPi*RPerp*aTube**2) &
            *sqrt(2.0*(aTube**2 - RMinus**2))*&
            cross_product(UnitX_D,XyzRel_D)
       ! Add the prominence material inside the flux rope, assuming that the
       ! given total amount of mass

       if (.not.UsePlasmaBeta)then
          ! Cold plasma density is applied with density estimated from
          ! the total mass of eject
          RhoFRope = Rho0*exp(-10.0*(RMinus/aTube)**6)
          pFluxRope = 0
       else
          ! Rescale BIPhi, which is not only a part of total pressure:
          BIPhi_D = BIPhi_D/sqrt(1 + PlasmaBeta)
          pFluxRope = 0.50*sum(BIPhi_D**2)*PlasmaBeta
          RhoFRope = pFluxRope/EjectaTemperature
       end if
    end if
    ! Add the field of the azimuthal current, Iphi::
    ! Compute the field produced by the ring current, Itube, both
    ! inside and outside the torus, BI = BFRope_D(x_:z_)::
    BFRope_D = BFRope_D + BIPhi_D

  end subroutine td99
  !============================================================================
  subroutine tdm(XyzRel_D, R2Rel, xRel, RMinus, RPlus2, Rperp, Kappa, Kappa2, &
     BFRope_D, RhoFRope, pFluxRope)
    !$acc routine seq

    use ModConst, ONLY: cTwoPi, cPi
    use ModHyperGeometric, ONLY: calc_elliptic_int_1kind, &
         calc_elliptic_int_2kind

    real, intent(in) :: XyzRel_D(3)
    real, intent(in) :: R2Rel, xRel, RMinus, RPlus2, Rperp, Kappa, Kappa2
    real, intent(out) :: BFRope_D(3)
    real, intent(out) :: RhoFRope, pFluxRope

    real, parameter :: Delta = 0.1
    real :: bTheta_D(3),  bI_D(3)
    real :: RhoStarF, RhoStarG
    real :: RPerpHat_D(3), ThetaHat_D(3)
    real :: AxialFlux, Xi
    real :: DKappaDx, DKappaDRperp, KstarF, dKSFdX, dKSFdR, &
         KstarG, dKSGdX, dKSGdR
    real :: EllipticKf, EllipticEf, FuncAf, dFuncAf, d2FuncAf, d3FuncAf
    real :: EllipticKg, EllipticEg
    real :: FuncAg, dFuncAg, d2FuncAg, d3FuncAg
    real :: SewingH, dSewingH, SewingF, dSewingF, SewingG, dSewingG
    real :: Ai, dAIdX, dAIdR
    real :: TmpG, dGdX, dGdR, TmpH, dHdR, Afx, dAFRdX, dAFXdR
    !--------------------------------------------------------------------------
    ! In spherical case the straping field magnitude bVert
    ! should be provided

    ! Coordinate unit vectors
    RPerpHat_D = (XyzRel_D - xRel*UnitX_D)/RPerp
    ThetaHat_D = cross_product(UnitX_D, RPerpHat_D)
    !
    ! Toroidal coordinate, usual argument of special functions
    ! describing solution in the totoidal coordinates
    !
    ! Derivatives over x and over RPerp
    DKappaDx = - (xRel*Kappa**3) / (4*RPerp*RTube)
    DKappaDRperp = &
         Kappa**3/(8*RPerp**2*RTube) * (RMinus**2-2*RPerp*(RPerp-RTube))

    AxialFlux = 3./(5*sqrt(2.)) * Itube * aTube

    ! Sewing functions
    Xi = (RMinus - aTube)/(Delta*aTube)
    SewingH = 0.5*(Xi+log(2*cosh(Xi)))
    dSewingH = 0.5*(1+tanh(Xi))
    ! BigTheta = cPi/4*(1+tanh(Xi))
    SewingF = SewingH    ! approximation for parabolic current case
    ! SewingF = SewingH + cF0*exp(cF1*SewingH+cF2*SewingH**2)
    dSewingF = dSewingH
    ! dSewingF = sin(BigTheta)
    ! dSewingF = dSewingH + cF0*exp(cF1*SewingH+cF2*SewingH**2) &
    !     *(cF1*dSewingH+2*cF2*SewingH*dSewingH)
    SewingG = SewingH
    ! SewingG = SewingH - cF0*exp(cG1*SewingH)
    dSewingG = dSewingH
    ! dSewingG = 1-cos(BigTheta)
    ! dSewingG = dSewingH - cF0*exp(cG1*SewingH)*cG1*dSewingH

    ! curly-A function and its derivatives for k_(six-edged-star)
    RhoStarF = aTube*(1 + Delta*SewingF)
    KstarF = sqrt((RPerp*RTube)/(RPerp*RTube + RhoStarF**2/4.))
    dKSFdX = - (xRel*KstarF**3) / (4*RPerp*RTube)*dSewingF*RhoStarF/RMinus
    dKSFdR = KstarF**3/(8*RPerp**2*RTube) &
         * (RhoStarF**2 - 2*RPerp*(RPerp-RTube)*dSewingF*RhoStarF/RMinus)

    call calc_elliptic_int_1kind(KstarF,EllipticKf)
    call calc_elliptic_int_2kind(KstarF,EllipticEf)

    FuncAf = ((2-KstarF**2)*EllipticKf - 2*EllipticEf) / KstarF
    dFuncAf = (2-KstarF**2)/(KstarF**2*(1-KstarF**2)) * EllipticEf &
         - 2/(KstarF**2) * EllipticKf
    d2FuncAf = -(KstarF**4-7*KstarF**2+4)/(KstarF**3*(1-KstarF**2)**2) &
         *EllipticEf - (5*KstarF**2-4)/(KstarF**3*(1-KstarF**2))*EllipticKf
    d3FuncAf = -(2*KstarF**6-31*KstarF**4+33*KstarF**2-12) &
         /(KstarF**4*(1-KstarF**2)**3) * EllipticEf &
         - (19*KstarF**4-27*KstarF**2+12) &
         /(KstarF**4*(1-KstarF**2)**2) * EllipticKf

    ! curly-A function for k_(five-sided-star)
    RhoStarG = aTube*(1 + Delta*SewingG)
    KstarG = sqrt((RPerp*RTube)/(RPerp*RTube+RhoStarG**2/4.))
    dKSGdX = - (xRel*KstarG**3) / (4*RPerp*RTube) * dSewingG*RhoStarG/RMinus
    dKSGdR = KstarG**3/(8*RPerp**2*RTube) &
         * (RhoStarG**2 - 2*RPerp*(RPerp-RTube)*dSewingG*RhoStarG/RMinus)

    call calc_elliptic_int_1kind(KstarG,EllipticKg)
    call calc_elliptic_int_2kind(KstarG,EllipticEg)

    FuncAg = ((2-KstarG**2)*EllipticKg - 2*EllipticEg) / KstarG
    dFuncAg = (2-KstarG**2)/(KstarG**2*(1-KstarG**2)) * EllipticEg - &
         2/(KstarG**2) * EllipticKg
    d2FuncAg = -(KstarG**4-7*KstarG**2+4)/(KstarG**3*(1-KstarG**2)**2) &
         *EllipticEg - (5*KstarG**2-4)/(KstarG**3*(1-KstarG**2))*EllipticKg
    d3FuncAg = -(2*KstarG**6-31*KstarG**4+33*KstarG**2-12) &
         /(KstarG**4*(1-KstarG**2)**3)*EllipticEg &
         - (19*KstarG**4-27*KstarG**2+12) &
         /(KstarG**4*(1-KstarG**2)**2)*EllipticKg

    ! ---- ring current field B_I ----
    !
    ! A phi-component of vector potential
    Ai = Itube/cTwoPi*sqrt(RTube/RPerp)* &
         (FuncAf + dFuncAf*(Kappa - KstarF)+0.5*d2FuncAf*(Kappa - KstarF)**2)
    !
    ! Its partial derivatives
    !
    dAIdX = Itube/cTwoPi*sqrt(RTube/RPerp)*&
         (dFuncAf*DKappaDx + d2FuncAf*DKappaDx*(Kappa - KstarF) &
         + 0.5*d3FuncAf*dKSFdX*(Kappa - KstarF)**2)
    dAIdR = Itube/cTwoPi*sqrt(RTube/RPerp) &
         *(dFuncAf*DKappaDRperp + d2FuncAf*DKappaDRperp*(Kappa-KstarF) &
         + 0.5*d3FuncAf*dKSFdR*(Kappa-KstarF)**2) - Ai/(2*RPerp)
    !
    ! Poloidal magnetic field
    bI_D = - dAIdX*RPerpHat_D + (dAIdR + Ai/RPerp)*UnitX_D

    ! ---- toroidal field B_theta ----

    ! just a temporary variable, same for tmpH below
    TmpG = 3 + 4*dFuncAf*(Kappa-KstarF)

    dGdX = 4*(d2FuncAf*dKSFdX*(Kappa-KstarF)+dFuncAf*(DKappaDx-dKSFdX))
    dGdR = 4*(d2FuncAf*dKSFdR*(Kappa-KstarF)+dFuncAf*(DKappaDRperp-dKSFdR))

    TmpH = (Kappa**3*(xRel**2 + RTube**2 - RPerp**2) - &
         aTube**2*KstarG**3)*dFuncAg + &
         aTube**2*KstarG**3*d2FuncAg*(Kappa-KstarG)
    dHdR = (3*Kappa**2*DKappaDRperp*(xRel**2 + RTube**2 -RPerp**2) - &
         2*Kappa**3*RPerp-&
         3*aTube**2*KstarG**2*dKSGdR)*dFuncAg + &
         (Kappa**3*(xRel**2 + RTube**2 - RPerp**2) - &
         aTube**2*KstarG**3)*d2FuncAg*dKSGdR + &
         aTube**2*( (3*KstarG**2*dKSGdR*(Kappa - KstarG) &
         + KstarG**3*(DKappaDRperp - dKSGdR))*d2FuncAg &
         + KstarG**3*(Kappa - KstarG)*d3FuncAg*dKSGdR )

    Afx = AxialFlux/(4*cPi*RPerp)*sqrt(RTube/RPerp) * &
         ( FuncAg + (aTube**2*KstarG**3)/(4*RPerp*RTube)*dFuncAg &
         + TmpG**(5./2.)/(30*sqrt(3.)) &
         - 0.3 + TmpG**(3./2.)/(12*sqrt(3.)*RPerp*RTube)*TmpH )

    dAFRdX = AxialFlux/(24*sqrt(3.)*cPi*RPerp)/sqrt(RPerp*RTube) * &
         (1.5*sqrt(TmpG)*dGdX*xRel*Kappa**3*dFuncAf + &
         sqrt(TmpG)**3*(xRel*Kappa**3*d2FuncAf*dKSFdX &
         + (Kappa**3+3*xRel*Kappa**2*DKappaDx)*dFuncAf))
    dAFXdR = (AxialFlux*sqrt(RTube))/(4*cPi)*RPerp**(-3./2.) &
         * ( dFuncAg*dKSGdR + &
         aTube**2/(4*RTube)*((3*KstarG**2*RPerp*dKSGdR-KstarG**3) &
         /(RPerp**2)*dFuncAg + &
         KstarG**3/RPerp*d2FuncAg*dKSGdR) &
         + TmpG**(3./2.)/(12*sqrt(3.))*dGdR + &
         1./(12*sqrt(3.)*RTube)*((1.5*sqrt(TmpG)*dGdR*RPerp-TmpG**(3./2.)) &
         /(RPerp**2)*TmpH + &
         TmpG**(3./2.)/RPerp*dHdR) ) - 3./(2*RPerp)*Afx

    bTheta_D = (dAFRdX-dAFXdR) * ThetaHat_D

    ! ---- combine three parts ----
    BFRope_D = bI_D + bTheta_D
    RhoFRope  = 0.0
    pFluxRope = 0.0

  end subroutine tdm
  !============================================================================
  subroutine compute_td99_bqfield(Xyz_D, BqField_D, TimeNow)
    !$acc routine seq

    real, intent(in)  :: Xyz_D(3)
    real, intent(out) :: BqField_D(3)
    real, intent(in), optional :: TimeNow

    ! Variables related to coordinates::
    real:: RPlus, RMinus
    !
    ! coordinate vectors of the charge location
    !
    real    :: RPlusSteady_D(3), RMinusSteady_D(3)
    real    :: RPlusMoving_D(3), RMinusMoving_D(3)
    !--------------------------------------------------------------------------
    ! if UseDynamicStrapping is .false. the call may be only accidental
    if(present(TimeNow).and.(.not.UseDynamicStrapping))RETURN

    ! Compute the locations, RMins_D and RPlus_D, of the two magnetic
    ! charges, -/+q::
    RPlusSteady_D  = Xyz_D - RPlus_D
    RMinusSteady_D = Xyz_D - RMinus_D
    RPlus  = norm2(RPlusSteady_D)
    RMinus = norm2(RMinusSteady_D)

    ! Compute the field of the strapping magnetic field, BqField_D::
    BqField_D = q*(RPlusSteady_D/RPlus**3 - RMinusSteady_D/RMinus**3)
    if (.not.present(TimeNow)) RETURN
    ! In the steady-state location there are charges of the opposite
    ! sign, therefore the calculated field should be flipped
    BqField_D = - BqField_D

    ! When the time is long enough, the moving charges annihilate
    if((TimeNow - tStartCme)*UChargeX >= qDistance)then
       BqField_D = BqField_D*No2Si_V(UnitB_)
       RETURN
    end if

    ! Compute the locations, RMins_D and RPlus_D, of the two magnetic
    ! charges, -/+q::
    RPlusMoving_D  = Xyz_D - &
         (RPlus_D  - (TimeNow - tStartCme)*UChargeX*UnitX_D)
    RMinusMoving_D = Xyz_D - &
         (RMinus_D + (TimeNow - tStartCme)*UChargeX*UnitX_D)
    RPlus  = norm2( RPlusMoving_D)
    RMinus = norm2(RMinusMoving_D)
    BqField_D = (BqField_D + &
         q*(RPlusMoving_D/RPlus**3 - RMinusMoving_D/RMinus**3))*No2Si_V(UnitB_)

  end subroutine compute_td99_BqField
  !============================================================================
  subroutine get_td99_size(SizeXY,  SizeZ)
    real,  intent(out) :: SizeXY,  SizeZ
    !--------------------------------------------------------------------------
    SizeXY = sqrt( (rTube + aTube)**2 - Depth**2 ) ! Horrizontal size
    SizeZ  = rTube +  aTube - Depth                ! Apex height
  end subroutine get_td99_size
  !============================================================================
end module EEE_ModTD99
!==============================================================================
