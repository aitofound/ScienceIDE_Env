Module ModCimiPlanet

  implicit none

  real :: re_m, dipmom, Hiono, rc
  
  !Species Information: Number of Ion Species (Earth=solar wind H+, H+, O+)
  integer,parameter :: nspec=4  ! number of species

  !define the species extensions
  character(len=2) :: NameSpeciesExtension_I(nspec)=(/'sw','_h','_o','_e'/)

  !named index for species order
  integer :: Sw_=1, H_=2, O_=3, e_=4

  !unset indexes needed for GM coupling
  integer :: He_=-1

  
  ! a0,a1,... are coef. of a polynomial which defines the 
  ! exponent of the charge exchange cross section
  real,parameter,dimension(nspec-1)::a0_I = (/-18.767,-18.767,-18.987/)
  real,parameter,dimension(nspec-1)::a1_I = (/-0.11017,-0.11017,-0.10613/)
  real,parameter,dimension(nspec-1)::a2_I = (/-3.8173e-2,-3.8173e-2,-5.4841e-3/)
  real,parameter,dimension(nspec-1)::a3_I = (/-0.1232,-0.1232,-1.6262e-2/)
  real,parameter,dimension(nspec-1)::a4_I = (/-5.0488e-2,-5.0488e-2,-7.0554e-3/)

  !set species Mass in amu
  real,parameter,dimension(nspec)   :: amu_I = (/1.0,1.0,16.0,5.4462e-4/)


  !set density and temp factor
  real, dimension(nspec) :: dFactor_I =(/0.4,0.4,0.2,1.0/)
  real, dimension(nspec) :: tFactor_I =[0.886364, 0.886364, 0.886364, 0.113636]

  !set plot parameters
  character(len=350), parameter :: & 
       NamePlotVar='x y P[nP] SwHpP[nP] HpP[nP] OpP[nP] eP[nP] ' &
       //'Ppar[nP] SwHpPpar[nP] HpPpar[nP] OpPpar[nP] ePpar[nP] ' &
       //'Phot[nP] SwHpPhot[nP] HpPhot[nP] OpPhot[nP] ePhot[nP] ' &
       //'Pparhot[nP] SwHpPparhot[nP] HpPparhot[nP] OpPparhot[nP] ePparhot[nP] ' &
       //'N[/m3] SwHpN[/m3] HpN[/m3] OpN[/m3] eN[/m3] ' &
       //'Beq[T] Vol[m3/Wb] Pot[Volts] FAC[Amp/m2] Lstar[Re] Plas[/m3] ' &
       //'g rbody'

  integer, dimension(nspec+1) :: iPplot_I    =(/1,2,3,4,5/) 
  integer, dimension(nspec+1) :: iPparplot_I =(/6,7,8,9,10/)
  integer, dimension(nspec+1) :: iPhotplot_I =(/11,12,13,14,15/)
  integer, dimension(nspec+1) :: iPparhotplot_I =(/16,17,18,19,20/)
  integer, dimension(nspec+1) :: iNplot_I    =(/21,22,23,24,25/)
  integer, parameter          :: Beq_=26,Vol_=27,Pot_=28, FAC_=29, Lstar_=30, &
                                 Plas_=31, nVar=31

  !set coupling variables for GM
  character(len=*), parameter :: &
       NameVarCouple='Rho Ppar p HpSwRho HpSwPpar HpSwP HpRho HpPpar HpP &
       &OpRho OpPpar OpP HpPsRho HpPsP Pe PePar'

  !number of variables to send to GM
  integer :: nVarImToGm = 16
  
  !set Logplot parameters

!  integer, parameter :: nLogVars = 8
  character(len=500), parameter :: & 
       NamePlotVarLog=&
       'it t dst '// &
       'RbSumSw RcSumSw SwHpDrift SwHpBfield SwHpChargeEx SwHpWaves '// &
       'SwHpStrongDiff SwHpDecay SwHpLossCone SwHpFLC SwHpDriftIn SwHpDriftOut '// &
       'RbSumH RcSumH HpDrift HpBfield HpChargeEx HpWaves '// &
       'HpStrongDiff HpDecay HpLossCone HpFLC HpDriftIn HpDriftOut '// &
       'RbSumO RcSumO OpDrift OpBfield OpChargeEx OpWaves '// &
       'OpStrongDiff OpDecay OpLossCone OpFLC OpDriftIn OpDriftOut '// &
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
