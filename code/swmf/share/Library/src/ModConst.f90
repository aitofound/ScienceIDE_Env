!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModConst

  ! no ONLY: so all constants can be accessed via ModConst
  use ModNumConst
  use ModUtilities, ONLY: CON_stop

  implicit none

  save

  ! Physical and solar astronomical constants.
  !
  ! All constants for planets, satellites, comets and
  ! other astronomical bodyies are found in ModPlanetConst

  ! Physical constants

  ! Time units
  real, parameter:: cSecondPerYear    = 31536000.0
  real, parameter:: cSecondPerDay     =    86400.0
  real, parameter:: cSecondPerHour    =     3600.0
  real, parameter:: cSecondPerMinute  =       60.0
  real, parameter:: cSecondPerCentury = 36525*cSecondPerDay

  ! Boltzmann constant [J/K]
  real, parameter:: cBoltzmann  = 1.3807E-23

  ! Atomic unit of mass [kg]
  real,parameter:: cAtomicMass = 1.66053E-27

  ! Proton mass [kg]
  real, parameter:: cProtonMass = 1.6726E-27

  ! Electron mass [kg]
  real, parameter:: cElectronMass = 9.1094E-31

  ! Speed of light [m/s]
  real, parameter:: cLightSpeed =  2.9979E+8

  ! Vacuum permeability [H/m]
  real, parameter:: cMu = cPi*4E-7

  ! Vacuum permittivity 8.8542E-12[F/m]
  real, parameter:: cEps = 1.0/cMu/cLightSpeed**2

  ! Vacuum resistance [Ohm]
  real,parameter:: cVacuumResistivity=cLightSpeed*cMu

  ! Fundamental charge [Coulomb]
  real, parameter:: cElectronCharge  = 1.6022E-19

  ! Fundamental charge squared in Joule*m, the Coulomb factor.
  real, parameter:: cElectronChargeSquaredJm = &
       cElectronCharge**2/(4.0*cPi*cEps)
  ! Alternative definition, this is charge of electron in CGSE,
  ! 4.8e-10, squared and converted from erg*cm to J*m=1E9 erg*cm.
  ! Therefore, cElectronChargeSquaredJm = (4.8e-10)**2/1E9. It is
  ! convenient to use this parameter while evaluating non-electric
  ! quantities, such as opacity, collision rate etc, via formulae
  ! written in CGSE. By expressing all even powers of the
  ! elementary charge using this parameter and expressing all the
  ! other quantities in Si, the result will be in Si.

  ! Planck constant  [J*s]
  real(Real8_), parameter :: cPlanckH    = 6.626069311E-34
  real(Real8_), parameter :: cPlanckHBar = cPlanckH /cTwoPi

  !       hplank  -  4.136e-15   Planck's constant (eV sec)
  ! The coefficient to convert the wave frequency, in Hertz,
  ! to the photon energy, in eV.
  real(Real8_), parameter:: cHPlanckEV = cPlanckH/cElectronCharge

  ! Bohr radius = 5.29e-11 [m]
  real(Real8_), parameter:: cBohrRadius = &
       (4.0*cPi*cEps/cElectronMass)* (cPlanckHBar/cElectronCharge)**2

  ! Thomson cross-section, which characterizes
  ! the thomson scattering of a low-energy photon by
  ! a free electron. Is of interest both itself (it determines the
  ! absolute brightness of the coronagraph image and as a
  ! convenient combination of the fundamental constants,
  ! in CGS system, coming to the transport coefficients in
  ! plasmas:
  ! \Sigma_{Thomson}=\frac{8\pi}{3}\left(\frac{e^2}{m_e c^2}\right)^2 [CGS]
  ! ~ 6.65E-25 cm^2
  real, parameter:: cSigmaThomson = 6.65E-29  ![m^2]

  ! Number of particles per mole
  real, parameter:: cAvogadro = 6.022045E+23

  ! Gravitation constant (NRL 1994)
  real, parameter:: cGravitation = 6.6726E-11

  ! Stefan-Boltzmann constant 5.6704E-8[J/s/m^2/K^4]
  real(Real8_), parameter:: cStefan = 2.0*cPi**5/15.0 &
       *(cBoltzmann/cLightSpeed/cPlanckH)**3 &
       *cBoltzmann*cLightSpeed

  ! Radiation constant 7.5657E-16 [J/m^3/K^4],
  ! such that the energy density for the black radiation
  ! equals cRadiation * ( T[K] )^4
  real(Real8_), parameter:: cRadiation = 4.0*cStefan/cLightSpeed

  ! Units for energy.
  real, parameter:: cErg=1.0E-7 ! J

  real, parameter:: cEV  = cElectronCharge
  real, parameter:: cKEV = 1000 * cEV
  real, parameter:: cMEV = 1000 * cKEV
  real, parameter:: cGEV = 1000 * cMEV
  real, parameter:: cTEV = 1000 * cGEV

  real, parameter:: cEVToK  =  cEV / cBoltzmann
  real, parameter:: cKEVToK = cKEV / cBoltzmann
  real, parameter:: cMEVToK = cMEV / cBoltzmann

  real, parameter:: cKToEV  = 1.0 / cEVToK
  real, parameter:: cKToKEV = 1.0 / cKEVToK
  real, parameter:: cKToMEV = 1.0 / cMEVToK

  ! Rydberg constant =13.60 eV.
  ! Sometimes the twice larger constant is referred to as Rydberg
  real(Real8_), parameter:: cRyToEV = (0.50/cElectronMass)*&
       (cPlanckHBar/cBohrRadius)**2/cEV

  ! Here Rme stands for Rest Mass Energy.
  real, parameter:: cRmeProton   = cProtonMass   * cLightSpeed**2
  real, parameter:: cRmeElectron = cElectronMass * cLightSpeed**2

  ! Non-relativistic formulae for gyrofrequencies:
  ! Gyrofrequency = cGyroParticle * |B|
  real, parameter:: cGyroProton   = cElectronCharge / cProtonMass
  real, parameter:: cGyroElectron = cElectronCharge / cElectronMass

  ! Relativistic formulae for gyrofrequencies:
  ! Gyrofrequency = cGyroRel * |B| / Energy
  real, parameter:: cGyroRel = cElectronCharge * cLightSpeed**2

  ! Formula for gyroradius:
  ! Gyroradius = cGyroRadius * momentum / |B|
  real, parameter:: cGyroRadius = 1.0 / cElectronCharge

  ! Solar Astronomical constants
  real, parameter:: cAU = 1.4959787E+11

  real, parameter:: rSun              = 0.696E+9               ! [ m]
  real, parameter:: mSun              = 1.99E+30               ! [kg]
  real, parameter:: RotationPeriodSun = 25.38 * cSecondPerDay  ! [ s]

  ! Time conversion parameters:
  ! mod(iYearBase,4) = 1 and iYearBase > 1582 (start of Gregorian calendar)
  integer, parameter :: iYearBase = 1965 !!! 1585

  ! Julian day of YearBase-01-01 UT00:00:
  ! General formula for Julian day may be found in ModTimeConvert:
  real(Real8_), parameter:: JulianDayBase = 367*iYearBase - &
       ((7*iYearBase)/4) + 1721044.5D0  ! = 0.24387615D+07

  ! Julian day for  0th Carrington Rotation start on
  ! 1853-Oct-13 14:26:17(approx)
  real(Real8_), parameter:: JulianDayCR0Start = 2398140.10155D0

  ! Time difference in seconds between 0th CR start and and the Julian day of
  ! Yeaar Base Jan,1 00:00:00 = -3.509688826080018D+9 [s]
  real(Real8_), parameter:: tStartCarringtonRotation = cSecondPerDay* &
       (JulianDayCR0Start - JulianDayBase)

  ! 27.2753088381330642 days [s]
  real, parameter:: CarringtonSynodicPeriod = cSecondPerDay/&
       (1/25.38D0 - 1/365.2425D0)

contains
  !============================================================================
  real function kappa_0_e(CoulombLog)
    real, intent(in):: CoulombLog

    ! Calculates the coefficient for the electron heat conduction coefficient
    ! in a hydrogen plasma: q=kappa_e_0T^{5/2}\nabla T, the heat flux,q, and
    ! and the electron temperature, T, are both in SI system of units as well as
    ! the scale of length.

    ! Attention!!! For all applications to solar corona and inner heliosphere
    ! it is expected that Coulomb logarithm equals 20. Therefore, the
    ! only legitimate formula for electron heat conduction coefficient is
    !
    ! q=kappa_0_e(20.0)*TeSi^2*sqrt(TeSi)
    !
    ! This is the only place in which the use of CoulombLog=20 is documented.
    !--------------------------------------------------------------------------
    kappa_0_e=3.2*3.0*cTwoPi/CoulombLog &
         *sqrt(cTwoPi*cBoltzmann/cElectronMass)*cBoltzmann &
         *((cEps/cElectronCharge)*(cBoltzmann/cElectronCharge))**2
  end function kappa_0_e
  !============================================================================
  real function te_ti_exchange_rate(CoulombLog)
    real, intent(in):: CoulombLog
    ! In hydrogen palsma, the electron-ion heat exchange is described by
    ! the equation as follows:
    ! dTe/dt = -(Te-Ti)/(tau_{ei})
    ! dTi/dt = +(Te-Ti)/(tau_{ei})
    ! The expression for 1/tau_{ei} may be found in
    ! Lifshitz&Pitaevskii, Physical Kinetics, Eq.42.5
    ! note that in the Russian edition they denote k_B T as Te and
    ! the factor 3 is missed in the denominator:
    ! 1/tau_ei = 2* CoulombLog * sqrt{m_e} (e^2/cEps)**2* Z**2 *Ni /&
    ! ( 3 * (2\pi k_B Te)**1.5 M_p). This exchange rate scales linearly
    ! with the plasma density, therefore, we introduce its ratio to
    ! the particle concentration. We calculate the temperature exchange
    ! rate by multiplying the expression for electron-ion effective
    ! collision rate,
    ! \nu_{ei} = CoulombLog/sqrt(cElectronMass)*  &
    !            ( cElectronCharge**2 / cEps)**2 /&
    !            ( 3 *(cTwoPi*cBoltzmann)**1.50 )* Ne/Te**1.5
    !  and then multiply in by the energy exchange coefficient
    !            (2*cElectronMass/cProtonMass)
    ! The calculation of the effective electron-ion collision rate is
    ! re-usable and can be also applied to calculate the resistivity:
    ! \eta = m \nu_{ei}/(e**2 Ne)

    !--------------------------------------------------------------------------
    te_ti_exchange_rate = &
         CoulombLog/sqrt(cElectronMass)*  &
         ( cElectronCharge**2 / cEps)**2 /&! effective ei collision frequency
         ( 3 *(cTwoPi*cBoltzmann)**1.50 ) &
         *(2*cElectronMass/cProtonMass)    ! *energy exchange per ei collision
  end function te_ti_exchange_rate
  !============================================================================
  real function momentum_to_energy(Momentum, NameParticle)

    real,intent(in):: Momentum
    character(LEN=*),intent(in):: NameParticle

    !--------------------------------------------------------------------------
    select case(NameParticle)
    case('e','Electron','electron','ELECTRON')
       momentum_to_energy=sqrt((Momentum*cLightSpeed)**2+&
            cRmeElectron**2)
    case('p','Proton','proton','PROTON')
       momentum_to_energy=sqrt((Momentum*cLightSpeed)**2+&
            cRmeProton**2)
    case default
       call CON_stop(&
            'Do not know the rest mass energy for '//NameParticle)
    end select
  end function momentum_to_energy
  !============================================================================
  real function momentum_to_kinetic_energy(Momentum, NameParticle)

    real,intent(in):: Momentum
    character(LEN=*),intent(in):: NameParticle

    !--------------------------------------------------------------------------
    select case(NameParticle)
    case('e','Electron','electron','ELECTRON')
       momentum_to_kinetic_energy=(Momentum*cLightSpeed)**2/(&
            sqrt((Momentum*cLightSpeed)**2+cRmeElectron**2) +&
            cRmeElectron)
    case('p','Proton','proton','PROTON')
       momentum_to_kinetic_energy=(Momentum*cLightSpeed)**2/(&
            sqrt((Momentum*cLightSpeed)**2+cRmeProton**2)   +&
            cRmeProton)
    case default
       call CON_stop(&
            'Do not know the rest mass energy for '//NameParticle)
    end select
  end function momentum_to_kinetic_energy
  !============================================================================
  real function energy_to_momentum(Energy, NameParticle)

    real,intent(in):: Energy
    character(LEN=*),intent(in):: NameParticle

    !--------------------------------------------------------------------------
    select case(NameParticle)
    case('e','Electron','electron','ELECTRON')
       energy_to_momentum=sqrt(Energy**2-cRmeElectron**2)/&
            cLightSpeed
    case('p','Proton','proton','PROTON')
       energy_to_momentum=sqrt(Energy**2 - cRmeProton**2)/&
            cLightSpeed
    case default
       call CON_stop(&
            'Do not know the rest mass energy for '//NameParticle)
    end select
  end function energy_to_momentum
  !============================================================================
  real function kinetic_energy_to_momentum(Energy, NameParticle)

    real,intent(in):: Energy
    character(LEN=*),intent(in):: NameParticle

    !--------------------------------------------------------------------------
    select case(NameParticle)
    case('e','Electron','electron','ELECTRON')
       kinetic_energy_to_momentum=sqrt(&
            Energy*(Energy + 2*cRmeElectron))/cLightSpeed
    case('p','Proton','proton','PROTON')
       kinetic_energy_to_momentum=sqrt(&
            Energy*(Energy + 2*cRmeProton  ))/cLightSpeed
    case default
       call CON_stop(&
            'Do not know the rest mass energy for '//NameParticle)
    end select
  end function kinetic_energy_to_momentum
  !============================================================================
  real function energy_in(NameEnergyUnit)

    character(LEN=*),intent(in):: NameEnergyUnit
    !--------------------------------------------------------------------------
    select case (NameEnergyUnit)
    case('J','j','Joule','joule','JOULE')
       energy_in=1.0   ! Do Nothing
    case('K','k','Kelvin','kelvin','KELVIN')
       energy_in=cBoltzmann
    case('ev','eV','EV','Ev')
       energy_in=cEV
    case('kEV','KeV','KEV','kev')
       energy_in=cKEV
    case('mEV','MeV','MEV','mev')
       energy_in=cMEV
    case('gEV','GeV','GEV','gev')
       energy_in=cGEV
    case('tEV','TeV','TEV','tev')
       energy_in=cTEV
    case default
       call CON_stop(&
            'We do not support energy units like: '//&
            'ton of trotil equivalent, '//&
            NameEnergyUnit//' and many other.')
    end select
  end function energy_in
  !============================================================================

end module ModConst
!==============================================================================

