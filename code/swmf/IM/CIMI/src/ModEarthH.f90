Module ModCimiPlanet

  implicit none

  real :: re_m, dipmom, Hiono, rc
  
  !Species Information: Number of Ion Species (Earth=H+)
  integer,parameter :: nspec=2  ! number of species

  !define the species extensions
  character(len=2) :: NameSpeciesExtension_I(nspec)=(/'_h','_e'/)

  !named index for species order
  integer :: H_=1, e_=2

  !unset indexes needed for GM coupling
  integer :: He_=-1, Sw_=-1, O_=-1

  ! a0,a1,... are coef. of a polynomial which defines the 
  ! exponent of the charge exchange cross section
  real,parameter,dimension(nspec-1) :: a0_I = (/-18.767/)
  real,parameter,dimension(nspec-1) :: a1_I = (/-0.11017/)
  real,parameter,dimension(nspec-1) :: a2_I = (/-3.8173e-2/)
  real,parameter,dimension(nspec-1) :: a3_I = (/-0.1232/)
  real,parameter,dimension(nspec-1) :: a4_I = (/-5.0488e-2/)

  !set species Mass in amu
  real,parameter,dimension(nspec)   :: amu_I = (/1.0,5.4462e-4/)


  !set density and temp factor
  real, dimension(nspec) :: dFactor_I =(/1.0,1.0/)
  real, dimension(nspec) :: tFactor_I =[0.886364,0.113636]

  !set plot parameters
  character(len=300), parameter :: & 
       NamePlotVar='x y P[nP] HpP[nP] eP[nP] Phot[nP] ' &
       //'Ppar[nP] HpPpar[nP] ePpar[nP] ' &
       //'HpPhot[nP] ePhot[nP] Pparhot[nP] HpPparhot[nP] ePparhot[nP] ' &
       // 'N[/m3] HpN[/m3] eN[/m3] Beq[T] Vol[m3/Wb] Pot[Volts] FAC[Amp/m2] ' & 
       // 'Lstar[Re] Plas[/m3] g rbody'

  integer, dimension(nspec+1) :: iPplot_I    =(/1,2,3/) 
  integer, dimension(nspec+1) :: iPparplot_I =(/4,5,6/)
  integer, dimension(nspec+1) :: iPhotplot_I =(/7,8,9/) 
  integer, dimension(nspec+1) :: iPparhotplot_I =(/10,11,12/)
  integer, dimension(nspec+1) :: iNplot_I    =(/12,14,15/)
  integer, parameter          :: Beq_=16,Vol_=17,Pot_=18, FAC_=19, Lstar_=20,&
                                 Plas_=21, nVar=21

  !set coupling variables for GM
  character(len=*), parameter :: &
       NameVarCouple = 'Rho Ppar p HpRho HpPpar HpP HpPsRho HpPsP Pe PePar'

  !number of variables to send to GM
  integer :: nVarImToGm = 10
  
  !set Logplot parameters
  character(len=500), parameter :: & 
       NamePlotVarLog=&
       'it t dst '// &
       'RbSumH RcSumH HpDrift HpBfield HpChargeEx HpWaves '// &
       'HpStrongDiff HpDecay HpLossCone HpFLC HpDriftIn HpDriftOut '// &
       'RbSume RcSume  eDrift  eBfield  eChargeEx  eWaves '// &
       ' eStrongDiff  eDecay  eLossCone eFLC eDriftIn  eDriftOut'

  public :: init_cimi_planet_const

contains

  subroutine init_cimi_planet_const

    use ModPlanetConst,	ONLY: Earth_, DipoleStrengthPlanet_I, rPlanet_I

    ! ionosphere altitude in km
    Hiono = 120.0

    ! earth's radius (m)
    re_m = rPlanet_I( Earth_ )

    ! earth's dipole moment
    dipmom = ABS( DipoleStrengthPlanet_I( Earth_ ) * re_m ** 3 )

    ! ionosphere distance in RE
    rc = ( re_m + Hiono * 1000. ) / re_m
    
  end subroutine init_cimi_planet_const
  
end Module ModCimiPlanet
