!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModTransitionRegion

  use ModExactRS
  use ModUtilities, ONLY: CON_stop
  use ModConst, ONLY: cBoltzmann, ceV, cProtonMass, cGravitation, mSun,&
       rSun, cMu
  implicit none
  SAVE
  PRIVATE ! except for a random set of 100 things below

  ! Public members
  public :: init_tr      ! Initialize module, check analytical TR model table
  public :: get_trtable_value     ! Access the TR model table
  public :: integrate_emission    ! Emission from the analytic TR model
  public :: plot_tr               ! Plot emission from the analytic TR model
  public :: read_tr_param         ! Read TR model parameters
  public :: check_tr_param        ! Synchtonize TR and SC model parameters
  public :: allocate_thread_arr   ! Threads originating from face centers
  public :: deallocate_thread_arr ! Deallocate threads
  public :: set_thread            ! Trace a thread from the face center
  public :: advance_thread_expl   ! Advance hd motion on thread
  public :: advance_thread_semi_impl   ! Advance heat conduction on thread
  public :: save_plot_thread      ! Plot state vector distribution on thread

  type OpenThread
     ! The Length along the thread, from the face i to the face i+1
     ! Exceptions: Ds_G(-nCell-1) is the length from
     ! the chromosphere to the face with the index -nCell
     ! Ds_G(0) is the distance from the center of interface
     ! between the threaded gap and the computational domain to
     ! simulate SC, to the point used to interpolate the state variables
     ! at the said interface. In meters
     real, pointer :: Ds_G(:)
     ! Magnetic field intensity (SI) in the points further associated to
     ! being the faces of the control volume scheme
     real, pointer :: B_F(:)
     ! (Dimensionless) spherical coordinates (of the faces)
     real, pointer :: Coord_DF(:,:)
     ! Heliocentric distance at the face center.
     real, pointer :: R_F(:)
     ! CELL (nor face!) centered array of the physical quantities
     ! to be solved for the given thread
     real, pointer :: State_VG(:,:)
     ! Set of primiteve variables, to be used for limiing the variables
     ! at the upper boundary face
     real, pointer :: LimiterState_V(:)
     ! Cell-centered array of B1 (induction) field
     real, pointer :: B1_DG(:,:)
     ! Unit direction vector of the cell-centered magnetic field
     real, pointer :: DirB_DG(:,:)
     real, pointer :: ConservativeOld_VC(:,:)
     real, pointer :: Te_G(:)
     ! Since the arrays are further updated with different routines,
     ! we store the intermediate results of simulation:
     ! 1. Local time step (set be the hydro update procedure)
     real, pointer :: Dt_C(:)
     ! 2. The transition region parameters, solved in
     ! the heat conduction routine, and the global time step
     real :: TeTr = -1.0, uTr = -1.0, PeTr = -1.0, Dt = -1.0
     ! number of cells, the _C arrays has the index range
     ! from -nCell to -1
     integer :: nCell = -1
     real    :: OpenFlux = 0.0 ! [T Rsun**2]
     real    :: Tmax = -1.0
  end type OpenThread
  public :: OpenThread

  ! Named indexes for state variables
  !
  ! State variables:
  ! in the state VG array:+++++++++++++++
  ! 1 - density
  ! 2 - velocity
  ! 3 - ion pressure or 2/3 PerpPressure + 1/3 ParPressure
  ! 3 or 4 Ion parallel pressure
  ! 4 or 5 - electron pressure
  ! 5 or 6 - "Major wave" - propagating from the Sun outward/representative
  ! 6 or 7 - "Minor wave" - propagating toward the Sun/representative
  ! Conservative variables:++++++++++++++
  ! 1 - mass density
  ! 2 - momentum density = density*velocity
  ! 3 - ion energy density = (3/2)Pi + (1/2)*Rho*U**2
  ! 3 or 4 - parallel internal energy
  ! 4 or 5 - electron energy (internal energy = 1.5*electron pressure)
  ! 5 or 6 - "Major wave" - propagating from the Sun outward/representative
  ! 6 or 7 - "Minor wave" - propagating toward the Sun/representative
  integer, public, parameter :: Rho_ = 1, U_ = 2, RhoU_ = 2, P_ = 3, &
       Energy_ = 3, Ppar_ = 3, & ! Ppar_=4 for anisotropic pressure
       Pe_ = Ppar_+1, Wmajor_ = Pe_+1, Wminor_ = Wmajor_+1
  logical, parameter :: UseAnisoPressure = Ppar_/=P_
  real, parameter :: Gamma = 5.0/3.0
  ! The Poynting flux to magnetic field ratio (one of the input parameters
  ! in SI unins)
  real, public :: PoyntingFluxPerBSi = 1.0e6 ! W/(m^2 T)
  real, public :: LperpTimesSqrtBSi = 7.5e4  ! m T^(1/2)
  real :: rMinReflectionTr = 0.0

  ! Normalization as used in the radcool table
  real, parameter :: RadNorm = 1.0E+22

  ! Correction coefficient to transform the units as used in the radcool
  ! table to SI system: erg = 1e-7 J / cm3 = 1e-6 m3 = 0.1
  real, parameter :: Cgs2SiEnergyDens = 0.1

  ! To find the volumetric radiative cooling rate in J/(m^3 s)
  ! the value found from radcool table should be multiplied by
  ! RadcoolSi and by n_e[m^{-3}]*n_i[m^{-3}]
  real, parameter :: Radcool2Si = 1.0e-12 & ! (cm-3=>m-3)**2
                          *Cgs2SiEnergyDens/RadNorm

  ! A constant factor to calculate the electron heat conduction
  real, public :: HeatCondParSi

  real :: cExchangeRateSi

  ! Gravity potential at the solar surface (m^2 s^-2), negative
  real, parameter :: cGravityPotAt1Rs = -cGravitation*mSun/rSun

  ! Coulomb logarithm
  real, public  :: CoulombLog = 20.0

  real, parameter :: cTwoSevenths = 2.0/7.0
  real,   public  :: cTolerance   = 1.0e-6

  ! Correspondent named indexes: meaning of the columns in the table
  integer, parameter, public :: PavrL_ = 1, uHeat_ = 2,       &
       HeatFluxL_ = 3, dHeatFluxLoverDcons_ = 4, Lambda_ = 5, &
       DlogLambdaOverDlogT_ = 6, DuOverDpL_ = 7
  ! Tabulated analytical solution:
  real, public :: TrTable_V(PavrL_:DuOverDpL_)

  ! Control parameter: below TeSiMin the observables are not calculated
  real, public :: TeSiMin = 5.0e4
  real :: ConsSiMax
  ! Average ion charge number and its square root
  real, public :: SqrtZ   = 1.0
  real :: Z = 1.0

  ! Table numbers needed to use lookup table
  integer         :: iTableRadCool = -1
  integer, public :: iTableTr = -1

  ! Chromosphere top boundary, in Rsun
  real, public, parameter :: rChromo = 1.0

  ! By default, this logical is .false. If logical set is true, the energy
  ! flux to/from the first control volume on the thread is accounted for.
  logical, public :: UseChromoEvap  = .false.
  ! To apply CromoEvaporation, the factor below is non-zero.
  real            :: ChromoEvapCoef = 0.0
  ! Parameters of the TR table:
  real, parameter :: TeTrMin = 1.0e4
  real, parameter :: TeTrMax = real(2.9988442312309672D+06)
  integer, parameter :: nPointTe = 310, nPointU = 15
  ! Global array used to fill in the table
  real, allocatable :: Value_VII(:,:,:)
  real            :: DeltaLogTe
  ! Max speed
  real, parameter :: uMax = 100.0, uMin = -40.0
  real, parameter :: DeltaU = (uMax - uMin)/(nPointU - 1)
  ! Needed for initialization:
  logical :: DoInit = .true.
  interface advance_heat_conduction
     module procedure advance_heat_conduction_ta
     module procedure advance_heat_conduction_ss
  end interface advance_heat_conduction
  ! Inner boundary
  ! To be read from the parameter file
  real :: tCorona = 2.0e6, pCorona = 4.0e-3
  ! Dimensionless parameters for stochastic heating
  logical :: UseStochasticHeating = .true.
  real :: StochasticExponent   = 0.21
  real :: StochasticAmplitude  = 0.18
  real :: StochasticExponent2  = 0.21
  real :: StochasticAmplitude2 = 0.0 ! 1.17
  ! Threads are traced from rMax toward rMin
  real,    public :: rMin = rChromo, rMax = 2.5
  integer, public :: nPointMax = 10000
  ! integration step at R = 1, dS propto R for R > 1
  real, public :: dS0 = 0.001
  ! Field unit conversion:
  real, public, parameter :: Si2Gs = 10000.0, Gs2Si = 1.0e-4
  ! Minimum field at the source surface and at the inner boundary
  real, public :: BssMinSi = 0.002*Gs2Si
  real, public :: BMinSi  = 0.0125*Gs2Si

  ! Minimum pressure and density
  real, public :: MinPress = 1.e-14   ! Pa
  real, public :: MinRho = 1.e-23     ! kg/m3
  ! Constant values read from the parameter file,
  ! if the uniform partitioning is assumed for dissipated turbulent energy
  real :: QparPerQtotal, QperpPerQtotal, QePerQtotal
  ! Time stepping
  real, public :: CflLocal = 0.5
  logical :: DoLimitLogVar = .true.
  integer, parameter :: iLogVar_V(3) = & ! Or, iLogVar_V(4) = &
       [Rho_, P_, Pe_] ! or  [Rho_, P_, Ppar_, Pe_]
  character(len=*), parameter:: NameMod = 'ModTransitionRegion'
contains
  !============================================================================
  subroutine init_tr(zIn, TeChromoSi, iComm)
    use ModConst, ONLY: kappa_0_e, te_ti_exchange_rate
    use ModLookupTable, ONLY: i_lookup_table
    !    use BATL_lib,       ONLY: test_start, test_stop, iProc
    ! INPUTS
    ! Average ion charge
    real, intent(in) :: zIn
    ! Temperature on top of chromosphere
    real, intent(in) :: TeChromoSi
    integer, optional, intent(in) :: iComm
    logical:: DoTest
    !   call test_start(NameSub, DoTest)
    character(len=*), parameter:: NameSub = 'init_tr'
    !--------------------------------------------------------------------------
    if(.not.DoInit)RETURN
    DoInit = .false.
    if(UseChromoEvap)then
       ChromoEvapCoef = TeTrMin
    else
       ChromoEvapCoef = 0.0
    end if
    ! Set model parameters
    Z = zIn
    SqrtZ = sqrt(Z)
    TeSiMin = TeChromoSi
    DeltaLogTe = log(TeTrMax/TeTrMin)/(nPointTe - 1)
    ! electron heat conduct coefficient for single charged ions
    ! = 9.2e-12 W/(m*K^(7/2))
    HeatCondParSi   = kappa_0_e(CoulombLog)
    ConsSiMax = cTwoSevenths*HeatCondParSi*TeTrMax**3.50
    cExchangeRateSi = te_ti_exchange_rate(CoulombLog)
    ! Init Riemann solver:
    call exact_rs_set_gamma(Gamma)
    iTableTr = i_lookup_table('TR')
    if(iTableTr<=0)then
      iTableRadCool = i_lookup_table('radcool')
       if(iTableRadCool <=0 )&
            call CON_stop('To create TR table, the radcool table is needed')
       call check_tr_table(iComm=iComm)
    end if
    ! call test
    ! call CON_stop('Check test')
    !  To make a unit test:
    ! 1. Uncomment the lines below as well as the declaration and the use
    ! of DoTest, test_start and test_stop
    ! 2. Add init_tr identifier to the #TEST command for SC component in
    !    Param/PARAM.in.test.SCIH_threadbc
    !    Param/PARAM.in.test.restart.SCIH_threadbc
    ! 3. run test8
    ! In run_test there will be files test_tr.out and test_tr_AiaXrt.out
    ! if(DoTest.and.iProc==0)then
    !   ! Plot test solution
    !   iTableAiaXrt = i_lookup_table('AiaXrt')
    !   if(iTableAiaXrt>0)then
    !      call plot_tr(&
    !           NamePlotFile='test_tr_AiaXrt.out',&
    !           nGrid       = 100,         &
    !           TeSi        = 5.0e5,       &
    !           PeSi        = 5.0e-3,      &
    !           iTable      = iTableAiaXrt)
    !   else
    !      call plot_tr(&
    !           NamePlotFile='test_tr.out',&
    !           nGrid       = 100,         &
    !           TeSi        = 5.0e5,       &
    !           PeSi        = 5.0e-3       )
    !   end if
    ! end if
  end subroutine init_tr
  !============================================================================
  subroutine read_tr_param(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in):: NameCommand
    character(len=*), parameter:: NameSub = 'read_tr_param'
    !--------------------------------------------------------------------------

    select case(NameCommand)
    case('#CHROMOEVAPORATION')
       call read_var('UseChromoEvap',UseChromoEvap)
    case("#STOCHASTICHEATING")
       UseStochasticHeating = .true.
       ! Stochastic heating when Beta_proton is below 1
       call read_var('StochasticExponent', StochasticExponent)
       call read_var('StochasticAmplitude', StochasticAmplitude)
    case("#HIGHBETASTOCHASTIC")
       ! Correction for stochastic heating when Beta_proton is between 1 and 30
       ! KAWs are non-propagating for Beta_proton > 30.
       call read_var('StochasticExponent2', StochasticExponent2)
       call read_var('StochasticAmplitude2', StochasticAmplitude2)
    case("#TURBULENCE")
       call read_var("PoyntingFluxPerBSi",PoyntingFluxPerBSi)
       call read_var("LperpTimesSqrtBSi",LperpTimesSqrtBSi)
       call read_var("rMinReflectionTr", rMinReflectionTr)
    case("#INNERBOUNDARY")
       call read_var('tCorona', tCorona)
       call read_var('pCorona', pCorona)
    case("#MINPRESSTR")
       call read_var('MinPress', MinPress)
    case("#MINRHOTR")
       call read_var('MinRho', MinRho)
    case("#LIMITLOGVAR")
       call read_var('DoLimitLogVar', DoLimitLogVar)
    case('#UNIFORMPARTITION')
       UseStochasticHeating = .false.
       call read_var('QparPerQtotal', QparPerQtotal)
       call read_var('QperpPerQtotal', QperpPerQtotal)
       QePerQtotal = 1 - (QparPerQtotal + QperpPerQtotal)
    case default
       call CON_stop(NameSub//": unknown command="//trim(NameCommand))
    end select
  end subroutine read_tr_param
  !============================================================================
  subroutine check_tr_param(rMaxIn, rMinReflectionTrIn, &
       tCoronaIn, pCoronaIn,                       &
       UseStochasticHeatingIn,                          &
       StochasticExponentIn, StochasticAmplitudeIn,     &
       StochasticExponent2In, StochasticAmplitude2In,   &
       QparPerQtotalIn, QperpPerQtotalIn)
    real, intent(in) :: rMaxIn, rMinReflectionTrIn
    real, intent(in) :: tCoronaIn, pCoronaIn ! Coronal base parameters SI
    ! Dimensionless parameters for turbulent heating
    logical, intent(in) :: UseStochasticHeatingIn
    ! If UseStochastic heating, assign the following:
    real, optional, intent(in) :: StochasticExponentIn
    real, optional, intent(in) :: StochasticAmplitudeIn
    real, optional, intent(in) :: StochasticExponent2In
    real, optional, intent(in) :: StochasticAmplitude2In
    ! Else
    real, optional, intent(in) :: QparPerQtotalIn
    real, optional, intent(in) :: QperpPerQtotalIn
    !--------------------------------------------------------------------------
    rMax = rMaxIn
    rMinReflectionTr = rMinReflectionTrIn
    tCorona = tCoronaIn
    pCorona = pCoronaIn
    UseStochasticHeating = UseStochasticHeatingIn
    if(UseStochasticHeating)then
       if(present(StochasticExponentIn))&
            StochasticExponent = StochasticExponentIn
       if(present(StochasticAmplitudeIn))&
            StochasticAmplitude = StochasticAmplitudeIn
       if(present(StochasticExponent2In))&
            StochasticExponent2 = StochasticExponent2In
       if(present(StochasticAmplitude2In))&
            StochasticAmplitude2 = StochasticAmplitude2In
    else
       if(present(QparPerQtotalIn))QparPerQtotal = QparPerQtotalIn
       if(present(QperpPerQtotalIn))QperpPerQtotal = QperpPerQtotalIn
       QePerQtotal = 1 - (QparPerQtotal + QperpPerQtotal)
    end if
  end subroutine check_tr_param
  !============================================================================
  ! Three routines below solve the convervation laws for field-aligned flow
  ! and tabulate the solution under the following asumptions:
  ! 1. Steady-state (d/dt=0)
  ! 2. Magnetic field is constant along the magnetic field line
  ! 3. Gravity is neglected
  ! 4. No anisotropy and Te=Ti. Ion and electron pressure are expressed
  !    in terms of Pavr: Pi = Pavr/SqrtZ, Pe = Pavr*SqrtZ
  ! 5. No wave pressure and wave heating
  ! ------------------------------
  ! From the mass conservation law:
  ! cProtonMass*uCh*N_ch = cProtonMass*U*N
  ! Combining this with the momentum conservation law, in which
  ! (SqrtZ + 1/SqrtZ)*Pavr + cProtonMass*N_ch*uCh**2*(U/uCh) = &
  !           (SqrtZ + 1/SqrtZ)*P_ch + cProtonMass*N_ch*uCh**2
  ! we have: Pavr/P_ch = 1 + Eps*(1 - U/uCh),
  ! where Eps = cProtonMass*uCh**2/(cBoltzmann*T_ch*(Z + 1)
  ! is the dimensionless coefficient known for given uCh
  ! and, since Pavr/P_ch = (T/T_ch)/(U/uCh),
  ! we have an equation relating U, uCh and T/T_ch
  ! T/T_ch = (1 + Eps) (U/uCh) - Eps*(U/uCh)**2,
  ! from which U can be solved
  ! U/uCh = (T/T_ch)/( (1+Eps)/2 +sqrt( (1+Eps)**2/4 - Eps*(T/T_ch)))
  real function u_over_uch(Te, uCh)
    real, intent(in) :: Te, uCh
    real :: Eps, ToverTtr
    !--------------------------------------------------------------------------
    Eps = cProtonMass*uCh**2/(cBoltzmann*TeTrMin*(Z + 1))
    ToverTtr = Te/TeTrMin
    u_Over_uCh = ToverTtr/(0.5*(1 + Eps) + &
         sqrt(max(0.0, 0.25*(1 + Eps)**2 - Eps*ToverTtr) ) )
  end function u_over_uch
  !============================================================================
  ! Similarly, we can rewrite an equation relating U, uCh and T/T_ch
  !  (U/uCh) + Eps1*(uCh/U)  - Eps1 - T/T_ch = 0,
  ! in terms of Eps1 = cProtonMass*U**2/(cBoltzmann*T_ch*(Z + 1),
  ! expressed in terms of U. From this quadratic equation uCh can be solved
  real function uch_over_u(Te, U)
    real, intent(in) :: Te, U
    real :: Eps1Ttr
    !--------------------------------------------------------------------------
    Eps1Ttr = cProtonMass*U**2/(cBoltzmann*(Z + 1))
    ! if(UseChromoEvap)ChromoEvapCoef = TeTrMin; else ChromoEvapCoef = 0
    uCh_Over_U = ChromoEvapCoef/( 0.5*(Te + Eps1Ttr) + sqrt(&
         max(0.0, 0.25*(Te + Eps1Ttr)**2 - Eps1Ttr*TeTrMin) ) )
  end function uch_over_u
  !============================================================================
  subroutine check_tr_table(TypeFileIn,iComm)
    use ModLookupTable, ONLY: i_lookup_table, &
         init_lookup_table, make_lookup_table, interpolate_lookup_table

    character(LEN=*),optional,intent(in)::TypeFileIn
    integer, optional, intent(in):: iComm

    character(len=5)::TypeFile
    ! Ionization potential for hydrogen
    ! cPotential_II(1,1) from util/CRASH/src/ModIonizPotential.f90:
    real, parameter :: cIonizPotentialH =  13.59844*ceV
    ! Ratio of the ionization energy flux to (uCh*Pch):
    real, parameter :: IonizationLoss = cIonizPotentialH/&
         (cBoltzmann*TeTrMin)
    ! Misc:
    ! Loop variables
    integer            :: iTe, iU
    ! Arguments corresponding to these indexes:
    real   :: TeSi_I(nPointTe), uCh
    integer, parameter :: Vel_ = 8, DuOverDcons_ = 7
    real   :: PavrL_I(nPointTe) ! 1st column of the table, Pavr*length
    real   :: uHeat_I(nPointTe)      ! 2nd column of the table, qHeat/Pch
    real   :: SemiIntUheat_I(1:nPointTe-1) ! uHeat in mid points
    real   :: uHeat2_I(nPointTe)     ! uHeat**2 (present in energy equation)
    real   :: EnthalpyFlux_I(nPointTe) ! (present in energy equation)
    real   :: dHeatFluxLoverDcons_I(nPointTe) ! 4th column, derivative of 3rd
    real   :: dHeatFluxLoverDuCh     ! derivative of 3rd column over uCh
    real   :: Lambda_I(nPointTe)     ! 5th column of the table, rad. loss
    real   :: dLogLambdaOverDlogT_I(nPointTe) ! 6th column, derivative of 5th
    real   :: pOverPch_I(nPointTe)   ! Pressure ratio to Pch
    real   :: U_I(nPointTe)          ! Velocity
    real   :: dUoverDcons_I(nPointTe)! Velocity derivative over Te
    real   :: dUoverDuCh             ! Velocity derivative over uCh
    ! Misc:
    real :: FactorStep, DeltaLogTeCoef, LambdaCgs_V(1)

    character(len=*), parameter:: NameSub = 'check_tr_table'
    !--------------------------------------------------------------------------
    if(present(TypeFileIn))then
       TypeFile = TypeFileIn
    else
       TypeFile = 'real8'
    end if
    ! initialize the TR table
    call init_lookup_table(                                      &
         NameTable = 'TR',                                       &
         NameCommand = 'save',                                   &
         Param_I = [uMin, uMax],                                 &
         NameVar =                                               &
         'logTe u PavrL UHeat FluxL dFluxLoverDcons '//          &
         'Lambda dLogLambdaOverDlogT DuOverDpL uMin uMax',       &
         nIndex_I = [nPointTe,nPointU],                          &
         IndexMin_I = [TeTrMin, uMin],                           &
         IndexMax_I = [TeTrMax, uMax],                           &
         NameFile = 'TR8.dat',                                   &
         TypeFile = TypeFile,                                    &
         StringDescription =                                     &
         'Model for transition region: '//                       &
         '[K] [m/s] [N/m] [m/s] [W/m] [1] [W*m3/(k_B2)] [1] '//  &
         '[(m/s)/(N/m)] [m/s] [m/s]')

    ! The table is now initialized.
    iTableTr = i_lookup_table('TR')
    ! Fill in the array of tabulated values:
    allocate(Value_VII(Vel_,nPointTe,nPointU))
    FactorStep = exp(DeltaLogTe)
    ! Fill in TeSi array
    TeSi_I(1) = TeTrMin
    do iTe = 2, nPointTe
       TeSi_I(iTe) = TeSi_I(iTe-1)*FactorStep
    end do
    ! Fill in LambdaSi
    ! at the moment, radcool not a function of Ne, but need a dummy 2nd
    ! index, and might want to include Ne dependence in table later.
    ! Table variable should be normalized to radloss_cgs * 10E+22
    ! since we don't want to deal with such tiny numbers
    ! real, parameter :: RadNorm = 1.0E+22
    ! real, parameter :: Cgs2SiEnergyDens = &
    !     1.0e-7&   ! erg = 1e-7 J
    !     /1.0e-6    ! cm3 = 1e-6 m3
    ! real, parameter :: Radcool2Si = 1.0e-12 & ! (cm-3=>m-3)**2
    !     /RadNorm*Cgs2SiEnergyDens
    do iTe = 1, nPointTe
       call interpolate_lookup_table(iTableRadCool,&
            TeSi_I(iTe), LambdaCgs_V)
       ! Here, Lambda is not divided by cBoltzmann**2 as in the final table
       Lambda_I(iTe) = LambdaCgs_V(1)*Radcool2Si
    end do
    ! Calculate dLogLambda/DLogTe
    DLogLambdaOverDLogT_I(1) = &
         log(Lambda_I(2)/Lambda_I(1))/DeltaLogTe
    do iTe = 2,nPointTe-1
       DLogLambdaOverDLogT_I(iTe) = &
            log(Lambda_I(iTe+1)/Lambda_I(iTe-1))/(2*DeltaLogTe)
    end do
    DLogLambdaOverDLogT_I(nPointTe) = &
         log(Lambda_I(npointTe)/Lambda_I(nPointTe-1))/DeltaLogTe
    ! Coefficient present below in Eq. (**)
    DeltaLogTeCoef = DeltaLogTe*HeatCondParSi/cBoltzmann**2
    do iU = 1, nPointU
       if(UseChromoEvap)then
          uCh = (iU - 1)*DeltaU + uMin
       else
          uCh = 0.0
       end if
       do iTe = 1, nPointTe
          U_I(iTe) = u_over_uch(TeSi_I(iTe), uCh)*uCh
          ! See above: Pavr/P_ch = 1 + Eps*(1 - U/uCh),
          ! where Eps = cProtonMass*uCh**2/(cBoltzmann*T_ch*(Z + 1)
          pOverPch_I(iTe) = 1 + (uCh - U_I(iTe))*cProtonMass*uCh/&
               (cBoltzmann*TeTrMin*(Z + 1))
          if(pOverPch_I(iTe) < 0.0)then
             write(*,*)'TeSi=',TeSi_I(iTe)
             write(*,*)'uCh=', uCh
             write(*,*)'iU=',iU
             write(*,*)'U_I=',U_I(iTe)
             call CON_stop('Negative pressure')
          end if
          ! Enthalpy Flux
          ! u*Ni*((Z+1)*(5/2)*cBoltzmann*Te + (1/2)*cProtonMass*U**2)
          ! Its ratio to P_ch=SqrtZ*cBoltzmann*Tch*Nch equals
          ! Enthalpy Flux / P_ch = &
          !    uCh*((SqrtZ+1/SqrtZ) (5/2)*(Te/T_ch) + &
          !    cProtonMass*U**2/(2*SqrtZ*cBoltzMann*TeTrMin)
          EnthalpyFlux_I(iTe) = uCh*(2.5*(SqrtZ + 1/SqrtZ)*TeSi_I(iTe) + &
               cProtonMass*U_I(iTe)**2/(2*SqrtZ*cBoltzmann))/TeTrMin
       end do
       ! Energy equation divided by Pch reads:
       ! (d/dx)(EnthalpyFlux - uHeat) = -Lambda*Z*Ni**2/Pch     (*),
       ! where uHeat = |qHeat|/Pch, |qHeat| = kappa_0*Te**3.5*(d(log(Te))/dx)
       ! On multiplying (*) by 2*uHeat we get equation for uHeat2=uHeat**2:
       ! (d/dx)uHeat2 = 2*uHeat*(d/dx)EnthalpyFlux +  2*Lambda*(Pavr/Pch)**2*&
       !         Te**1.5*(kappa_0/cBoltzmann**2)*(d(log(Te))/dx)
       ! On multiplying by yet unknown dx we obtain FD equation
       ! Delta(uHeat2) = 2*uHeat*Delta(EnthalpyFlux) + &
       !     2*Lambda*(Pavr/Pch)**2*Te**1.5*DeltaLogTeCoef   (**)
       ! where Delta(...) = ...(iTe) - ...(iTe-1)
       uHeat_I(1) = max(uCh*IonizationLoss/SqrtZ + EnthalpyFlux_I(1), 0.0)
       uHeat2_I(1) = uHeat_I(1)**2
       do iTe = 2, nPointTe
          SemiIntUheat_I(iTe-1) = sqrt( uHeat2_I(iTe-1) + &
               uHeat_I(iTe-1)*(EnthalpyFlux_I(iTe) - EnthalpyFlux_I(iTe-1)) +&
               pOverPch_I(iTe-1)**2*Lambda_I(iTe-1)*TeSi_I(iTe-1)**1.50    &
               *DeltaLogTeCoef)
          uHeat2_I(iTe) = uHeat2_I(iTe-1) + SemiIntUheat_I(iTe-1)*2*&
               (EnthalpyFlux_I(iTe) - EnthalpyFlux_I(iTe-1)) + &
               (pOverPch_I(iTe-1)**2*Lambda_I(iTe-1)*TeSi_I(iTe-1)**1.50 + &
               pOverPch_I(iTe)**2*Lambda_I(iTe)*TeSi_I(iTe)**1.50)         &
               *DeltaLogTeCoef
          uHeat_I(iTe) = sqrt(uHeat2_I(iTe))
       end do
       PavrL_I(1) = 0
       do iTe = 2, nPointTe
          ! Integrate \int{\kappa_0\Lambda Te**3.5 d(log T)/UHeat}
          PavrL_I(iTe) = PavrL_I(iTe-1) + 0.5*DeltaLogTe*&
               ( TeSi_I(iTe-1)**3.5 + TeSi_I(iTe)**3.5 )&
               /SemiIntUheat_I(iTe-1)
          ! Not multiplied by \kappa_0
       end do
       dHeatFluxLoverDcons_I(1) = &
            (PavrL_I(2)*uHeat_I(2) - &
            PavrL_I(1)*uHeat_I(1))/&
            (DeltaLogTe*TeSi_I(1)**3.5)
       dUoverDcons_I(1) = (U_I(2) - U_I(1))/&
            (DeltaLogTe*HeatCondParSi*TeSi_I(1)**3.5)
       do iTe = 2, nPointTe - 1
          dHeatFluxLoverDcons_I(iTe) = &
               ( PavrL_I(iTe+1)*uHeat_I(iTe+1)   &
               - PavrL_I(iTe-1)*uHeat_I(iTe-1) ) &
               /(2*DeltaLogTe*TeSi_I(iTe)**3.5)
          dUoverDcons_I(iTe) = (U_I(iTe+1) - U_I(iTe-1))/&
               (2*DeltaLogTe*HeatCondParSi*TeSi_I(iTe)**3.5)
       end do
       dHeatFluxLoverDcons_I(nPointTe) = &
            (PavrL_I(nPointTe)*uHeat_I(nPointTe) - &
            PavrL_I(nPointTe-1)*uHeat_I(nPointTe-1))/&
            (DeltaLogTe*TeSi_I(nPointTe)**3.5)
       dUoverDcons_I(nPointTe) = (U_I(nPointTe) - U_I(nPointTe-1))/&
            (DeltaLogTe*HeatCondParSi*TeSi_I(nPointTe)**3.5)
       PavrL_I(:) = PavrL_I(:)*HeatCondParSi
       do iTe = 1, nPointTe
          Value_VII(PAvrL_:Vel_, iTe, iU) = &
               [ PavrL_I(iTe)*pOverPch_I(iTe), &
               uHeat_I(iTe),                 &
               uHeat_I(iTe)*PavrL_I(iTe), &
               dHeatFluxLoverDcons_I(iTe),    &
               Lambda_I(iTe)/cBoltzmann**2,&
               DLogLambdaOverDLogT_I(iTe), &
               dUoverDcons_I(iTe), &
               U_I(iTe)]
       end do
    end do
    if(UseChromoEvap)then
       ! Fill in the velocity derivative
       do iU = 2, nPointU - 1
          uCh = (iU - 1)*DeltaU + uMin
          do iTe = 1, nPointTe
             dHeatFluxLoverDuCh = &
                  (Value_VII(HeatFluxL_,iTe,iU+1) -&
                  Value_VII(HeatFluxL_,iTe,iU-1))/ &
                  (2*DeltaU)
             dUoverDuCh = &
                  (Value_VII(Vel_,iTe,iU+1) - Value_VII(Vel_,iTe,iU-1))/&
                  (2*DeltaU)
             Value_VII(dHeatFluxLoverDcons_,iTe,iU) = &
                  Value_VII(dHeatFluxLoverDcons_,iTe,iU) - &
                  Value_VII(DuOverDcons_,iTe,iU)*dHeatFluxLoverDuCh/&
                  dUoverDuCh
             if(iTe/=1)Value_VII(DuOverDpL_,iTe,iU) = &
                  (Value_VII(Vel_,iTe,iU+1) - Value_VII(Vel_,iTe,iU-1))/&
                  (Value_VII(PavrL_,iTe,iU+1) - Value_VII(PavrL_,iTe,iU-1))
          end do
       end do
       Value_VII(DuOverDpL_,:,1) = 0.0
       Value_VII(DuOverDpL_,:,nPointU) = 0.0
       Value_VII(DuOverDpL_,1,:) = 0.0
    else
       Value_VII(DuOverDpL_,:,:) = 0.0
    end if
    ! Shape the table using the filled in array
    call make_lookup_table(iTableTr, calc_tr_table, iComm)
    deallocate(Value_VII)
  end subroutine check_tr_table
  !============================================================================
  subroutine calc_tr_table(iTableIn, Arg1, Arg2, Value_V)

    integer, intent(in):: iTableIn
    real, intent(in)   :: Arg1, Arg2
    real, intent(out)  :: Value_V(:)
    integer            :: iTe, iU
    logical:: DoTest
    character(len=*), parameter:: NameSub = 'calc_tr_table'
    !--------------------------------------------------------------------------
    iTe = 1 + nint(log(Arg1/TeTrMin)/DeltaLogTe)
    iU = nint( (Arg2 - uMin)/ DeltaU) + 1
    Value_V(:) = Value_VII(PavrL_:DuOverDpL_, iTe, iU)
  end subroutine calc_tr_table
  !============================================================================
  subroutine get_trtable_value(Te, uFace)

    ! For two entries, Te and u on top of the transition region,
    ! the routine calculates TrTable_V array of the tabulated values
    ! If uIn is not provided, it is taken to be 0.
    ! If optional uOut is requested, the limited value of u is provided
    ! calculated with the value uMin < uCh < uMax at the bottom of
    ! transition region.
    ! All inputs and outputs are in SI units
    use ModLookupTable, ONLY: interpolate_lookup_table
    real, intent(in) :: Te  ! Temperature on top of the transition region
    ! Speed on top of the transition region
    real, OPTIONAL, intent(inout) :: uFace
    ! Misc:
    ! Actual table entry, speed at the bottom of TR:
    real :: uCh
    !--------------------------------------------------------------------------
    if(present(uFace))then
       uCh = uch_over_u(Te, uFace)*uFace
       uCh = min(uMax, max(uCh, uMin))
       uFace = uCh*u_over_uch(Te, uCh)
    else
       uCh = 0.0
    end if
    call interpolate_lookup_table(iTableTr, Te, uCh, TrTable_V, &
         DoExtrapolate=.false.)
  end subroutine get_trtable_value
  !============================================================================
  subroutine set_thread(XyzIn_D, FaceArea, OpenThread1, &
       xyz_to_coord, get_field)

    use ModLookupTable, ONLY: interpolate_lookup_table
    ! Origin point coordinates (Xyz)
    real, intent(in) :: XyzIn_D(3)
    ! Face area, to calculate flux
    real, intent(in) :: FaceArea
    ! Thread to set
    type(OpenThread), intent(inout) :: OpenThread1
    interface
       subroutine xyz_to_coord(Xyz_D, Coord_D)
         implicit none
         real, intent(in) :: Xyz_D(3)
         real, intent(out):: Coord_D(3)
       end subroutine xyz_to_coord
       subroutine get_field(Xyz_D, B_D, B1Out_D)
         implicit none
         real, intent(in) :: Xyz_D(3)
         real, intent(out):: B_D(3)
         real, OPTIONAL, intent(out) :: B1Out_D(3)
       end subroutine get_field
    end interface
    ! loop variable
    integer :: iPoint, nCell
    ! Length interval, ! Heliocentric distance
    real :: Ds, R
    ! coordinates, field vector and modulus
    real :: XyzStart_D(3), Xyz_D(3), B_D(3), B, BMax
    !  for ourward directed field, -1 otherwise
    real :: SignBr
    ! Radial field at the source surface
    real :: BrSS
    ! Coordinates and magnetic field in the midpoint
    ! within the framework of the Runge-Kutta scheme
    real :: XyzAux_D(3)!, Baux_D(3)
    ! Aux
    real :: ROld, Aux
    real :: XyzOld_D(3), Dir1_D(3), Dir2_D(3), Dir3_D(3), Dir4_D(3)
    real :: B_F(-nPointMax:0), Length_I(-nPointMax:0), &
         B1_DF(3,-nPointMax:0), DirB_DG(3,-nPointMax:0)
    real :: Coord_DI(3,-nPointMax:0), R_F(-nPointMax:0)
    real :: CosBrMin = -1.0
    integer :: iRefine ! Factor of step refinement near null point
    real :: BLength
    real :: HeatFluxL
    ! Trace the open thread with the given origin point
    character(len=*), parameter:: NameSub = 'set_thread'
    !--------------------------------------------------------------------------
    XyzStart_D = XyzIn_D
    R = norm2(XyzStart_D)
    CosBrMin = -1.0   ! Arbitrary angle between magnetic field and radial dir
    iRefine = 10      ! Step refinement by a factor of 1/10 is allowed
    call get_field(XyzStart_D, B_D, B1_DF(:,0))
    BrSS = sum(XyzStart_D*B_D)/R
    OpenThread1%OpenFlux = FaceArea*BrSS
    ! Unit direction vector with positive radial component
    DirB_DG(:,0) = B_D/sign(norm2(B_D),OpenThread1%OpenFlux)
    SignBr = sign(1.0, BrSS)
    IBEGINLOOP: do
       B = norm2(B_D)
       call xyz_to_coord(XyzStart_D,Coord_DI(:,0))
       iPoint = 0
       B_F(0) = B
       BMax = max(B, BssMinSi)
       Xyz_D = XyzStart_D
       R_F(0) = R
       POINTS: do
          iPoint = iPoint + 1
          ! For the previous point given are Xyz_D, B_D, B
          ! Store R
          ROld = R
          ! Store a point
          XyzOld_D = Xyz_D
          ! Four-stage Runge-Kutta
          BMax = max(BMax, B)
          Ds = Ds0*R*SignBr/min(BMax, iRefine*B)
          Dir1_D = B_D
          if(CosBrMin /= -1.0)call limit_cosbr(Xyz_D, Dir1_D)
          ! 1. Point at the half of length interval:
          XyzAux_D = Xyz_D - 0.50*Ds*Dir1_D
          ! 2. Magnetic field in this point:
          call get_field(XyzAux_D, Dir2_D)
          if(CosBrMin /= -1.0)call limit_cosbr(XyzAux_D, Dir2_D)
          XyzAux_D = Xyz_D - 0.50*Ds*Dir2_D
          call get_field(XyzAux_D, Dir3_D)
          if(CosBrMin /= -1.0)call limit_cosbr(XyzAux_D, Dir3_D)
          XyzAux_D = Xyz_D - Ds*Dir3_D
          call get_field(XyzAux_D, Dir4_D)
          if(CosBrMin /= -1.0)call limit_cosbr(XyzAux_D, Dir4_D)
          ! 3. New grid point:
          Xyz_D = Xyz_D - (Ds/6)*(Dir1_D + 2*Dir2_D + 2*Dir3_D + Dir4_D)
          R = norm2(Xyz_D)
          if(R >  rMax.or.iPoint==nPointMax)then
             R = norm2(XyzStart_D)
             call get_field(XyzStart_D, B_D)
             ! Don't allow magnetic field too much decline from radial dir
             CosBRMin = ((R**2 - rChromo**2)/(nPointMax - 2) &
                  + (R*Ds0)**2)/ (2*rChromo*Ds0)
             if(CosBRMin>0.9)call CON_stop('Increase nPointThreadMax')
             ! Prohibit step refinement
             iRefine = 1
             CYCLE IBEGINLOOP
          end if
          R_F(-iPoint) = R
          call xyz_to_coord(Xyz_D, Coord_DI(:,-iPoint))
          call get_field(Xyz_D, B_D, B1Out_D=B1_DF(:,-iPoint))
          B = norm2(B_D)
          B_F(-iPoint) = B
          Length_I(-iPoint) = norm2(XyzOld_D - Xyz_D)
          DirB_DG(:,-iPoint) = (XyzOld_D - Xyz_D)/Length_I(-iPoint)
          if(R <= rMin + Ds0*max(1 - abs(B)/BminSi,0.0))EXIT IBEGINLOOP
       end do POINTS
    end do IBEGINLOOP
    ! Calculate more accurately the intersection point
    ! with the photosphere surface
    Aux = (ROld - RMin) / (ROld - R)
    Xyz_D = (1 - Aux)*XyzOld_D +  Aux*Xyz_D
    ! Store the last point
    call xyz_to_coord(Xyz_D, Coord_DI(:,-iPoint))
    call get_field(Xyz_D, B_D)
    B = norm2(B_D)
    B_F(-iPoint) = B
    Length_I(-iPoint) = norm2(Xyz_D - XyzOld_D)
    ! R_F(-iPoint) = norm2(Xyz_D)
    R_F(-iPoint) = norm2(Xyz_D)
    nCell = iPoint - 2
    ! Allocate threads if needed
    if(nCell /= OpenThread1%nCell)then
       if(.not.associated(OpenThread1%Ds_G))then
          call allocate_pointer
       else
          call deallocate_pointer
          call allocate_pointer
       end if
    end if
    ! Store results from tracing
    OpenThread1%Ds_G(-nCell:-1) = &
         Length_I(-nCell:-1)*Rsun
    ! Length of the analytical transition region,
    ! combined from the last two intervals
    OpenThread1%Ds_G(-nCell-1) = &
         (Length_I(-iPoint) + Length_I(-iPoint+1))*Rsun
    ! Distance from the "ghost point" to the interface (0 index)
    OpenThread1%Ds_G(0) = 0.0
    OpenThread1%B_F(-iPoint:0) = B_F(-iPoint:0)
    OpenThread1%Coord_DF(:,-iPoint:0) = Coord_DI(:,-iPoint:0)
    allocate(OpenThread1%R_F(-nCell:0))
    OpenThread1%R_F(-nCell:0) = R_F(-nCell:0)
    OpenThread1%B1_DG(:,-nCell:-1) = 0.50*(&
         B1_DF(:,-nCell:-1) + B1_DF(:,1-nCell:0))
    OpenThread1%B1_DG(:,0) = B1_DF(:,0)
    OpenThread1%DirB_DG(:,-nCell:0) = DirB_DG(:,-nCell:0)
    ! Re-initialize state vector if needed
    ! if(nCell /= OpenThread1%nCell)then
       OpenThread1%nCell = nCell
       call init_thread_variables(OpenThread1)
    ! end if
    ! TMax [K], such that the ingoing heat flux to the TR at this
    ! temperature equals the Poynting flux (which is not realistic and means
    ! that the input temperature exceeding TMax assumes that something is
    ! going wrong
    BLength = sum((OpenThread1%B_F(-nCell:-1) + OpenThread1%B_F(1-nCell:0))*&
         0.50*OpenThread1%Ds_G(-nCell:-1))
    HeatFluxL = 2*PoyntingFluxPerBSi*BLength
    call interpolate_lookup_table(iTable=iTableTR, Arg2In=0.0, &
         iVal=HeatFluxL_, &
         ValIn=HeatFluxL,&
         Value_V=TrTable_V,      &
         Arg1Out=OpenThread1%TMax,  &
         DoExtrapolate=.false.)
  contains
    !==========================================================================
    subroutine allocate_pointer
      !------------------------------------------------------------------------
      allocate(OpenThread1%B_F(-iPoint:0))
      allocate(OpenThread1%Ds_G(-nCell-1:0))
      allocate(OpenThread1%Coord_DF(3,-iPoint:0))
      allocate(OpenThread1%Dt_C(-nCell:-1))
      allocate(OpenThread1%R_F(-nCell:0))
      allocate(OpenThread1%Te_G(-nCell:0))
      allocate(OpenThread1%B1_DG(3,-nCell:0))
      allocate(OpenThread1%DirB_DG(3,-nCell:0))
      allocate(OpenThread1%ConservativeOld_VC(Rho_:Wminor_,-nCell:-1))
      allocate(OpenThread1%State_VG(Rho_:Wminor_,-nCell-1:0))
      allocate(OpenThread1%LimiterState_V(Rho_:Wminor_))
    end subroutine allocate_pointer
    !==========================================================================
    subroutine deallocate_pointer
      !------------------------------------------------------------------------
      deallocate(OpenThread1%B_F)
      deallocate(OpenThread1%Ds_G)
      deallocate(OpenThread1%Coord_DF)
      deallocate(OpenThread1%Dt_C)
      deallocate(OpenThread1%R_F)
      deallocate(OpenThread1%Te_G)
      deallocate(OpenThread1%B1_DG)
      deallocate(OpenThread1%DirB_DG)
      deallocate(OpenThread1%ConservativeOld_VC)
      deallocate(OpenThread1%State_VG)
      deallocate(OpenThread1%LimiterState_V)
    end subroutine deallocate_pointer
    !==========================================================================
    subroutine init_thread_variables(OpenThread1)

      type(OpenThread),intent(inout) :: OpenThread1

      real :: rInv_I(-OpenThread1%nCell:0), rFaceInv
      real :: PeFace, TeFace, uFace = 0.0
      real :: Te_I(-OpenThread1%nCell:0)
      real :: Ni_I(-OpenThread1%nCell:0)
      real :: Ti_I(-OpenThread1%nCell:0)
      real :: Pi_I(-OpenThread1%nCell:0), Pe_I(-OpenThread1%nCell:0)
      real :: Ds_I(-OpenThread1%nCell-1:0)
      real :: B_I(-OpenThread1%nCell:0)
      integer :: iTime, nCell

      !------------------------------------------------------------------------
      nCell = OpenThread1%nCell
      B_I = OpenThread1%B_F(-nCell:0)
      Ds_I =  OpenThread1%Ds_G(-nCell-1:0)
      rFaceInv = 1/OpenThread1%R_F(-nCell)
      rInv_I(-nCell:-1) = &
           (1/OpenThread1%R_F(-nCell:-1) + &
           1/OpenThread1%R_F(-nCell+1: 0))*0.50
      rInv_I(0) = 1/(OpenThread1%R_F(0) + (OpenThread1%R_F(0) - &
           OpenThread1%R_F(-1))*min(0.50, Ds_I(0)/Ds_I(-1)) )
      ! initial velocity is zero

      OpenThread1%State_VG(U_,-nCell:0) = 0.0

      Te_I = tCorona; Ti_I = tCorona
      Ni_I = 0.50*pCorona/(cBoltzmann*tCorona)
      do iTime = 1,nCell
         call advance_heat_conduction_ta(nCell, 100.0, Te_I, Ti_I(-nCell:-1),&
              Ni_I(-nCell:-1), Ds_I, uFace, B_I, PeFaceOut = PeFace, &
              TeFaceOut=TeFace, DoLimitTimestep=.true.)
         call barometric_equilibrium(nCell, Te_I, Ti_I, rInv_I, &
              rFaceInv, PeFace, TeFace, Ni_I, Pe_I, Pi_I)
      end do

      OpenThread1%State_VG(P_,-nCell:0) = Pi_I(-nCell:0)
      if(UseAnisoPressure)OpenThread1%State_VG(Ppar_,-nCell:0) = &
           OpenThread1%State_VG(P_,-nCell:0)
      OpenThread1%State_VG(Pe_,-nCell:0) = Pe_I(-nCell:0)

      OpenThread1%State_VG(Wmajor_,-nCell:0) = 1.0
      OpenThread1%State_VG(Wminor_,-nCell:0) = 1.0e-3
      OpenThread1%Te_G(-nCell:0) = Te_I(-nCell:0)
      OpenThread1%State_VG(Rho_,-nCell:0) = cProtonMass*Ni_I(-nCell:0)
      OpenThread1%TeTr = TeFace
      OpenThread1%uTr  = 0.0
      OpenThread1%PeTr = PeFace
      OpenThread1%LimiterState_V = OpenThread1%State_VG(:,0)
    end subroutine init_thread_variables
    !==========================================================================
    subroutine limit_cosbr(Xyz_D, B_D)

      real, intent(in)    :: Xyz_D(3) ! Location
      real, intent(inout) :: B_D(3)   ! Magnetic field to be corrected
      real                :: CosBR, B, DirR_D(3), DirB_D(3)
      !------------------------------------------------------------------------
      DirR_D = Xyz_D/norm2(Xyz_D)
      B = norm2(B_D)
      DirB_D = SignBr*B_D/B
      CosBR = sum(DirB_D*DirR_D)
      if(CosBR>=CosBRMin)RETURN
      DirB_D = (DirB_D - CosBR*DirR_D)*&      ! Tangential componenets
           sqrt((1 - CosBRMin**2)/(1 - CosBR**2))+& ! Reduced in magnitude
           DirR_D*CosBRMin          ! Plus increased radial comp
      B_D = SignBr*B*DirB_D
    end subroutine limit_cosbr
    !==========================================================================
  end subroutine set_thread
  !============================================================================
  subroutine convert_p_tot_to_rho(Ptot, Te, Ti, Rho, wRepresentative)

    ! Solve Rho from quadratic equation
    ! Rho/cProtonMass*cBoltzmann*(Ti + Z*Te) + &
    ! 0.5*PoyntingFluxPerBSi*sqrt(cMu*Rho)*wRepresentative - Ptot = 0
    real, intent(in)  :: Ptot, Te, Ti
    real, intent(out) :: Rho
    real, optional, intent(in) :: wRepresentative

    real :: Misc

    character(len=*), parameter:: NameSub = 'convert_p_tot_to_rho'
    !--------------------------------------------------------------------------
    Misc = PoyntingFluxPerBSi*sqrt(cMu/(16*Ptot))
    if(present(wRepresentative))Misc = Misc*wRepresentative
    Rho = Ptot/(Misc + sqrt(Misc**2 + cBoltzmann*(Z*Te + Ti)/cProtonMass))**2
  end subroutine convert_p_tot_to_rho
  !============================================================================
  subroutine barometric_equilibrium(nPoint, Te_I, Ti_I, rInv_I, &
       rFaceInv, PeFace, TeFace, Ni_I, Pe_I, Pi_I, WaveIn_VI)

    integer, intent(in)    :: nPoint
    real,    intent(in)    :: Te_I(nPoint+1), Ti_I(nPoint+1)
    real,    intent(in)    :: rInv_I(nPoint+1)
    real,    intent(in)    :: rFaceInv, PeFace, TeFace
    real,    intent(out)   :: Ni_I(nPoint+1), Pe_I(nPoint+1), Pi_I(nPoint+1)
    real, optional, intent(in) :: WaveIn_VI(Wmajor_:Wminor_,0:nPoint+1)
    real :: uFace = 0.0, RhoFace, PtotFace
    ! Predicted vars at the midpoint
    real :: PredTe, PredTi, PredRho, PredPtot, PredWave
    ! total pressure and mass density
    real :: Ptot_I(nPoint+1), Rho_I(nPoint+1)
    !  wave intensities
    real :: Wave_VI(Wmajor_:Wminor_,0:nPoint+1)
    ! Loop variable
    integer :: iPoint
    !--------------------------------------------------------------------------
    if(present(WaveIn_VI))then
       Wave_VI = WaveIn_VI
    else
       Wave_VI(Wmajor_,:) = 1.0; Wave_VI(Wminor_,:) = 1.0e-3
    end if
    RhoFace = cProtonMass*PeFace/(Z*cBoltzmann*TeFace)
    PtotFace = 0.5*sum(Wave_VI(:,0))*PoyntingFluxPerBSi*sqrt(cMu*RhoFace) &
         + (1 + 1/Z)*PeFace
    ! Predictor
    PredPtot = PtotFace*exp(cGravityPotAt1Rs*RhoFace/PtotFace*0.5*&
         (rFaceInv - rInv_I(1)))
    PredTe = 0.5*(TeFace + Te_I(1))
    PredTi = 0.5*(TeFace + Ti_I(1))
    PredWave = 0.5*sum(Wave_VI(:,0:1))
    call convert_p_tot_to_rho(PredPtot, PredTe, PredTi, PredRho, PredWave)
    ! Corrector
    Ptot_I(1) = PtotFace*exp(cGravityPotAt1Rs*&
         PredRho/PredPtot*(rFaceInv - rInv_I(1)))
    call convert_p_tot_to_rho(Ptot_I(1), Te_I(1), Ti_I(1), Rho_I(1), &
         sum(Wave_VI(:,1)))
    Ni_I(1) = Rho_I(1)/cProtonMass
    Pi_I(1) = cBoltzmann*Ni_I(1)*Ti_I(1)
    Pe_I(1) = cBoltzmann*Ni_I(1)*Te_I(1)*Z
    do iPoint = 2, nPoint + 1
       ! Predictor
       PredPtot = Ptot_I(iPoint-1)*exp(cGravityPotAt1Rs*&
            Rho_I(iPoint-1)/Ptot_I(iPoint-1)*0.5*&
            (rInv_I(iPoint-1) - rInv_I(iPoint)))
       PredTe = 0.50*(Te_I(iPoint-1) + Te_I(iPoint))
       PredTi = 0.50*(Ti_I(iPoint-1) + Ti_I(iPoint))
       PredWave = 0.5*sum(Wave_VI(:,iPoint-1:iPoint))
       call convert_p_tot_to_rho(PredPtot, PredTe, PredTi, PredRho, &
            PredWave)
       ! Corrector
       Ptot_I(iPoint) = Ptot_I(iPoint-1)*exp(cGravityPotAt1Rs*&
            PredRho/PredPtot*(rInv_I(iPoint-1) - rInv_I(iPoint)))
       call convert_p_tot_to_rho(Ptot_I(iPoint), &
            Te_I(iPoint), Ti_I(iPoint), Rho_I(iPoint),sum(Wave_VI(:,iPoint)))
       Ni_I(iPoint) = Rho_I(iPoint)/cProtonMass
       Pi_I(iPoint) = cBoltzmann*Ni_I(iPoint)*Ti_I(iPoint)
       Pe_I(iPoint) = cBoltzmann*Ni_I(iPoint)*Te_I(iPoint)*Z
    end do
  end subroutine barometric_equilibrium
  !============================================================================
  subroutine allocate_thread_arr(Threads_II, nI, nJ)
    type(OpenThread), allocatable, intent(inout) :: Threads_II(:,:)
    integer, intent(in) :: nI, nJ
    integer :: i, j
    character(len=*), parameter:: NameSub = 'allocate_thread_arr'
    !--------------------------------------------------------------------------
    allocate(Threads_II(nI,nJ))
    do j = 1, nJ; do i = 1, nI
       call init_thread(Threads_II(i,j))
    end do; end do
  contains
    !==========================================================================
    subroutine init_thread(OpenThread1)

      ! Thread to set
      type(OpenThread), intent(inout) :: OpenThread1
      !------------------------------------------------------------------------

      nullify(OpenThread1%ConservativeOld_VC)
      nullify(OpenThread1%Dt_C)
      nullify(OpenThread1%State_VG)
      nullify(OpenThread1%B1_DG)
      nullify(OpenThread1%DirB_DG)
      nullify(OpenThread1%Te_G)
      nullify(OpenThread1%R_F)
      nullify(OpenThread1%Coord_DF)
      nullify(OpenThread1%B_F)
      nullify(OpenThread1%Ds_G)
      nullify(OpenThread1%LimiterState_V)
      OpenThread1%TeTr = -1.0
      OpenThread1%uTr  = -1.0
      OpenThread1%PeTr = -1.0
      OpenThread1%Dt   = -1.0
      OpenThread1%OpenFlux = 0.0
      OpenThread1%nCell = -1
    end subroutine init_thread
    !==========================================================================
  end subroutine allocate_thread_arr
  !============================================================================
  subroutine deallocate_thread_arr(Threads_II, nI, nJ)
    type(OpenThread), allocatable, intent(inout) :: Threads_II(:,:)
    integer, intent(in) :: nI, nJ
    integer :: i, j
    character(len=*), parameter:: NameSub = 'deallocate_thread_arr'
    !--------------------------------------------------------------------------
    do j = 1, nJ; do i = 1, nI
       call deallocate_thread(Threads_II(i,j))
    end do; end do
    deallocate(Threads_II)
  contains
    !==========================================================================
    subroutine deallocate_thread(OpenThread1)

      ! Thread to set
      type(OpenThread), intent(inout) :: OpenThread1
      !------------------------------------------------------------------------
      OpenThread1%TeTr = -1.0
      OpenThread1%uTr  = -1.0
      OpenThread1%PeTr = -1.0
      OpenThread1%Dt   = -1.0
      OpenThread1%nCell = -1
      OpenThread1%OpenFlux = 0.0
      if(.not.associated(OpenThread1%Coord_DF))RETURN
      deallocate(OpenThread1%ConservativeOld_VC)
      deallocate(OpenThread1%Dt_C)
      deallocate(OpenThread1%State_VG)
      deallocate(OpenThread1%B1_DG)
      deallocate(OpenThread1%DirB_DG)
      deallocate(OpenThread1%Te_G)
      deallocate(OpenThread1%R_F)
      deallocate(OpenThread1%Coord_DF)
      deallocate(OpenThread1%B_F)
      deallocate(OpenThread1%Ds_G)
      deallocate(OpenThread1%LimiterState_V)
    end subroutine deallocate_thread
    !==========================================================================
  end subroutine deallocate_thread_arr
  !============================================================================
  subroutine set_thread_inner_bc(TeIn, uIn, pIn, State_V)

    real, intent(in) :: TeIn, uIn, pIn
    real, intent(out):: State_V(Rho_:Wminor_)
    !--------------------------------------------------------------------------
    State_V(Rho_:P_) = [&
         cProtonMass*pIn/(cBoltzmann*TeIn), &
         uIn, &
         pIn ] ! P
    if(UseAnisoPressure)State_V(Ppar_) = pIn
    State_V(Pe_:) = [&
         pIn, & ! Pe
         1.0, & ! Wmajor
         1e-3]  ! Wminor
  end subroutine set_thread_inner_bc
  !============================================================================
  subroutine integrate_emission(TeSi, PeSi, iTable, nVar, Integral_V)

    use ModLookupTable, ONLY: interpolate_lookup_table
    ! INPUTS:
    ! The plasma parameters on top of the transition region:
    real,    intent(in)  :: TeSi, PeSi
    integer, intent(in)  :: iTable, nVar
    real,    intent(out) :: Integral_V(nVar)
    ! The model is parameterized in terms of PAvr=sqrt(Pi*Pe) at Ti=Te
    ! In terms of SqrtZ: PAvr = Pi*SqrtZ = Pe/SqrtZ
    real    :: PAvrSi
    ! 1D Grid across the TR
    ! Number of uniform meshes in the range from TeSiMin to Te on the TR top
    integer, parameter :: nTrGrid = 20
    ! mesh-centered temperature and the spatial length of intervals, in cm!!
    real    :: TeAvrSi_I(nTrGrid + 1), DeltaLCgs_I(nTrGrid + 1)
    ! Gen table values:
    real    :: Value_VI(nVar, nTrGrid +1)
    real    :: DeltaTe      ! Mesh of a temperature
    real    :: PavrL_I(nTrGrid + 1), Te_I(nTrGrid + 1)
    real    :: DeltaPavrL_I(nTrGrid + 1)
    integer ::  i, iVar ! Loop variables
    ! Electron density in particles per cm3:
    real    :: NeCgs, NiCgs
    !--------------------------------------------------------------------------
    PAvrSi = PeSi/SqrtZ
    DeltaTe = (TeSi - TeSiMin)/nTrGrid
    Te_I(1) = TeSiMin
    call get_trtable_value(TeSiMin)
    ! First value is now the product of the thread length in meters times
    ! a geometric mean pressure, so that
    PavrL_I(1) = TrTable_V(PavrL_)
    do i = 1, nTrGrid
       Te_I(i +1) = Te_I(i) + DeltaTe
       call get_trtable_value(Te_I(i + 1))
       PavrL_I(i + 1) = TrTable_V(PavrL_)
       DeltaPavrL_I(i) = PavrL_I(i + 1) - PavrL_I(i)
       TeAvrSi_I(i) = (Te_I(i + 1) + Te_I(i))*0.50
    end do

    TeAvrSi_I(nTrGrid + 1) = TeSi
    DeltaPavrL_I(nTrGrid + 1) = &
         PavrL_I(1) - PavrL_I(nTrGrid + 1)

    ! Now, DeltaL_I is the length interval multiplied by PAvrSi.
    ! Get rid of the latter factor and convert from meters to cm:
    DeltaLCgs_I = DeltaPavrL_I*100.0/PAvrSi

    do i = 1, nTrGrid + 1
       ! Calculate mesh-centered electron density:
       NeCgs = 1.0e-6*PAvrSi*SqrtZ/(cBoltzmann*TeAvrSi_I(i))
       NiCgs = 1.0e-6*PAvrSi/(cBoltzmann*TeAvrSi_I(i)*SqrtZ)
       call interpolate_lookup_table(iTable, TeAvrSi_I(i), NeCgs, &
            Value_VI(:,i), DoExtrapolate=.true.)
       ! Multiply by a scale factor 1e-26 of the table times Ne*Ni*DeltaL
       Value_VI(:,i) = Value_VI(:,i)*NeCgs*NiCgs*1.0e-26*&
            DeltaLCgs_I(i)
    end do

    ! Sum up the contributions from all temperature intervals:
    do iVar = 1, nVar
       Integral_V(iVar) = sum(Value_VI(iVar,:))
    end do

  end subroutine integrate_emission
  !============================================================================
  subroutine advance_thread_expl(iStage, OpenThread1, IsTimeAccurate, &
       RightFace0_V, LeftFace0_V, DtIn)

    integer, intent(in) :: iStage
    type(OpenThread),intent(inout) :: OpenThread1
    logical, intent(in) :: IsTimeAccurate
    real, OPTIONAL, intent(in) :: RightFace0_V(Rho_:Wminor_)
    real, OPTIONAL, intent(out) :: LeftFace0_V(Rho_:Wminor_)
    real, OPTIONAL, intent(in)  :: DtIn

    integer :: nCell ! # of cells = OpenThread1%nCell
    ! _C(ell) - are cell-centered values
    ! _G(host) - same, but with ghost cells
    ! _F(ace) - are face centered
    ! for each cell the left face has the same index as the cell,
    ! the right face has the index by 1 greater
    ! _C arrays have indexes from -nCell to -1, with nPoint-2 being called
    ! nCell, which is the total number of cells
    ! _G arrays have indexes from -nCell-1 to 0
    ! _F arrays have indexes from -nCell to 0
    !
    !
    ! State variables:
    ! in the state VG array:
    ! 1 - density
    ! 2 - velocity
    ! 3 - ion pressure
    ! 3 or 4 - ion parallel pressure
    ! 4 or 5 - electron pressure
    ! 5 or 6 - "Major wave" - propagating from the Sun outward/representative
    ! 6 or 7 - "Minor wave" - propagating toward the Sun/representative
    ! Conservative variables:
    ! 1 - mass density
    ! 2 - momentum density
    ! 3 - ion energy density (kinetic+internal)
    ! 3 or 4 - 0.5*Ppar
    ! 4 or 5 - internal energy of electrons=3/2 Pe
    ! 6 or 6 - "Major wave" - propagating from the Sun outward/representative
    ! 6 or 7 - "Minor wave" - propagating toward the Sun/representative

    ! Conservative variables, in the physical cells only (old)
    real, pointer    :: ConservativeOld_VC(:,:)
    ! State vector:
    real, pointer :: State_VG(:,:)
    ! Electron temperature for semi-implicit scheme
    real, pointer :: Te_G(:)
    ! Local time step for a given cell
    real, pointer :: Dt_C(:)
    ! Cell size along the magnetic field line. In the leftmost ghost cell
    ! there is the length of analytical transition
    real, pointer :: Ds_G(:)
    !
    ! Elements of the control volume scheme:
    !

    ! Fluxes for all variables, thorough a given face
    real :: Flux_VF(Rho_:Wminor_,-OpenThread1%nCell:0)
    ! Face area (divided by the flux)
    real    :: FaceArea_F(-OpenThread1%nCell:0)
    ! Control volume (the inverse of)
    real    :: vInv_C(-OpenThread1%nCell:-1)
    !
    ! What we need for the source calculation:
    !
    ! Velocity divergence
    real :: DivU_C(-OpenThread1%nCell:-1)
    ! Pseudo-divergence of velocity, needed to calculate contributions from
    ! parallel pressures and similar total pressure gradient
    real :: DuDs
    ! Interpolated left and right states at the face
    real    :: pLeft_VF(Rho_:Wminor_,-OpenThread1%nCell:0)
    real    :: pRight_VF(Rho_:Wminor_,-OpenThread1%nCell:0)
    ! Differences across the face, to be limited
    real    :: dVarUp_V(Rho_:Wminor_), dVarDown_V(Rho_:Wminor_)
    ! To get the radial component of spherical coordinates
    real, parameter :: cThird = 1.0/3.0, cTwoThird = 2.0/3.0, Beta = 1.50
    ! Staging:
    integer, parameter :: nStage = 2
    real    :: StageCoef
    ! Misc:
    real :: Source_V(Rho_:Wminor_)
    real :: GravitySource, WavePress, Pperp
    ! Face-centered velocity, densiry and total pressure, from the exct RS
    real :: Un_F(-OpenThread1%nCell:0), Rho_F(-OpenThread1%nCell:0),&
         pTot_F(-OpenThread1%nCell:0), B_F(-OpenThread1%nCell:0)
    real :: Cleft_F(-OpenThread1%nCell:0)
    real :: Cright_F(-OpenThread1%nCell:0)
    real :: VaFace_F(-OpenThread1%nCell:0)
    real :: VaCell_C(-OpenThread1%nCell:-1)
    real :: VaDtOverDs_C(-OpenThread1%nCell:-1)
    real :: DissMajor_C(-OpenThread1%nCell:-1)
    real :: DissMinor_C(-OpenThread1%nCell:-1)
    real :: Heating, DvaDs, Reflection_C(-OpenThread1%nCell:-1)
    ! Calculated using the constants above or by applying apportion_heating
    real :: PparHeating, PperpHeating, PeHeating, pLimit
    ! To fix pTotal
    real :: DeltaPtot
    ! Loop variables:
    integer :: iCell, iFace
    ! Vector of primitives used in limiting
    real :: LimiterState_V(Rho_:Wminor_), DeltaLtd_V(Rho_:Wminor_)
    ! Variables used to limit velocity and mass flux
    real :: uLim, MassFluxLim, pViscous_C(-OpenThread1%nCell:-1)
    real :: PtotLim
    character(len=*), parameter:: NameSub = 'advance_thread_expl'
    !--------------------------------------------------------------------------
    if(present(DtIn).and.(.not.IsTimeAccurate))call CON_stop(&
         NameSub//':DtIn input is only allowed in time accurate mode')
    nCell = OpenThread1%nCell
    Dt_C=>OpenThread1%Dt_C
    State_VG => OpenThread1%State_VG
    ConservativeOld_VC => OpenThread1%ConservativeOld_VC
    Te_G => OpenThread1%Te_G
    ! Mesh size along the line
    Ds_G => OpenThread1%Ds_G
    ! Face centered field, in Si
    B_F = OpenThread1%B_F(-nCell:0)
    ! Face area, in Rsun**2
    FaceArea_F(-nCell:0) = abs(OpenThread1%OpenFlux)/B_F
    ! Control volume (the inverse of) in 1/(m*Rsun**2)
    ! =1/(dS*0.5*(FaceLeft + FaceRight))
    vInv_C(-nCell:-1) = 2/(Ds_G(-nCell:-1)*&
         (FaceArea_F(-nCell:-1) + FaceArea_F(-nCell+1:0)) )
    if(iStage==1)then
       do iCell = -nCell, -1
          !
          ! Initial values for the conserved variables:
          !
          ConservativeOld_VC(:,iCell) = State_VG(:,iCell)
          ! Except:
          ! Momentum density
          ConservativeOld_VC(RhoU_,iCell) = &
               State_VG(U_,iCell)*State_VG(Rho_,iCell)
          ! Energy density
          ConservativeOld_VC(Energy_,iCell) = 1.50*State_VG(P_,iCell)&
               + 0.50*State_VG(Rho_,iCell)*State_VG(U_,iCell)**2
          ! contribution to internal energy from parallel pressure
          if(UseAnisoPressure)ConservativeOld_VC(Ppar_,iCell) = &
               0.50*State_VG(Ppar_,iCell)
          ! "Conserved" electron energy density is 1.5 Pe)
          ConservativeOld_VC(Pe_,iCell) = 1.50*State_VG(Pe_,iCell)
       end do
    end if
    ! Start the 2-stage advance procedure
    !
    StageCoef = 0.50*iStage ! Half time step is applied at first stage
    ! Limit mass flux. If mass flux does not exceed MasFluxLim, then
    ! in steady state the plasma local speed doesn't exceed 0.1*Cs
    ! Limit flux across the low boundary
    call get_trtable_value(OpenThread1%TeTr, OpenThread1%uTr)
    MassFluxLim = 0.1*TrTable_V(uHeat_)*FaceArea_F(-nCell)*cProtonMass*&
         OpenThread1%PeTr/(Z*cBoltzmann*OpenThread1%TeTr)
    do iCell = -nCell, -1
       MassFluxLim = min(MassFluxLim, 0.1*State_VG(Rho_,iCell)*&
            speed_of_sound(State_VG(:,iCell))*&
            min(FaceArea_F(iCell), FaceArea_F(iCell+1)))
    end do
    ! Limit the speed at the lower boundary
    OpenThread1%uTr = min( OpenThread1%uTr, MassFluxLim/&
         (FaceArea_F(-nCell)*cProtonMass*OpenThread1%PeTr/&
         (Z*cBoltzmann*OpenThread1%TeTr) ) )
    !
    ! Boundary conditions:
    !
    ! Apply left BC in the ghostcell #=-nCell-1
    !
    call set_thread_inner_bc(&
         TeIn = OpenThread1%TeTr, &
         uIn  = OpenThread1%uTr,  &
         pIn  = OpenThread1%PeTr, &
         State_V= State_VG(:,-nCell-1))
    ! State_VG(Wmajor_,-nCell-1) = min(max(1.0, 5e-4/B_F(-nCell)), &
    !     1.1e6/PoyntingFluxPerbSi)
    call get_trtable_value(Te_G(0))
    uLim = 0.1*min(TrTable_V(UHeat_), &
         sqrt(cBoltzmann*Te_G(0)/cProtonMass))
    PtotLim = p_tot(State_VG(:,-1)) + cGravityPotAt1Rs*State_VG(Rho_,-1)*&
         0.50*(1/OpenThread1%R_F(-1) - 1/OpenThread1%R_F(0))
    call limit_p_tot(State_VG(:,0), PtotLim)
    if(State_VG(U_,0) > 0.0)then
       State_VG(U_,0) = min(State_VG(U_,0), uLim)
    else
       State_VG(U_,0) = 0.0
    end if
    if(present(LeftFace0_V))then
       LimiterState_V = OpenThread1%LimiterState_V
       call limit_p_tot(LimiterState_V, PtotLim)
       if(LimiterState_V(U_) > 0.0)then
          LimiterState_V(U_) = min(LimiterState_V(U_), uLim)
       else
          LimiterState_V(U_) = 0.0
       end if
    end if
    ! limited interpolation procedure:
    ! 1. logarithm of density/pressure is better to be limited
    if(DoLimitLogVar.and.any(State_VG(iLogVar_V,-nCell-1:0)<=0.0))then
       write(*,*)'-ncell-1=',-nCell - 1
       write(*,*)'Stage=', iStage
       write(*,*)'OpenThread TeTr=', OpenThread1%TeTr
       write(*,*)'OpenThread uTr=', OpenThread1%uTr
       write(*,*)'OpenThread PeTr=', OpenThread1%PeTr
       if(OpenThread1%PeTr <= 0.0)write(*,*)'TrTable_V=', TrTable_V
       do iCell = -nCell-1, 0
          if(any(State_VG(iLogVar_V,iCell)<=0.0))&
               write(*,*)'iCell=',iCell,' state:',&
               State_VG(:,iCell)
       end do
       if(present(RightFace0_V))then
          write(*,*)'RightFace0_V=',RightFace0_V
          write(*,*)'LimiterState_V=',OpenThread1%LimiterState_V
       end if
       call save_plot_thread(OpenThread1,'failed_thread.out')
       call CON_stop('Negative pressure/density')
    end if
    if(DoLimitLogVar)State_VG(iLogVar_V,-nCell-1:0) = log(&
         State_VG(iLogVar_V,-nCell-1:0))
    ! Left boundary:
    pLeft_VF(:,-nCell) = State_VG(:,-nCell-1)
    ! Calculate leftmost unlimited slope. The half of this (as assumed in
    ! the limiter function) approximates the face value
    dVarDown_V = 2*(State_VG(:,-nCell) - State_VG(:,-nCell-1))
    ! Calculate the up slope
    dVarUp_V = State_VG(:,-nCell+1) - State_VG(:,-nCell)
    ! Calculate and apply the limited slopes, to get the limited
    ! reconstructed values:
    pRight_VF(:,-nCell) = State_VG(:,-nCell) - &
         (sign(0.25, dVarUp_V) + sign(0.25, dVarDown_V))* &
         min(Beta*abs(dVarUp_V), abs(dVarDown_V), &
         cThird*abs(2*dVarDown_V+dVarUp_V))
    pLeft_VF(:,-nCell+1) = State_VG(:,-nCell) + &
         (sign(0.25, dVarUp_V) + sign(0.25, dVarDown_V))* &
         min(Beta*abs(dVarUp_V), abs(dVarDown_V), &
         cThird*abs(dVarDown_V+2*dVarUp_V))
    ! Pass through all cells, limit slopes
    do iCell = -nCell+1, -2
       ! Propagate the old up slope to become the down slope
       dVarDown_V = dVarUp_V
       ! Calculate the up slope
       dVarUp_V = State_VG(:,iCell+1) - State_VG(:,iCell)
       ! Calculate and apply the limited slopes, to get the limited
       ! reconstructed values:
       pRight_VF(:,iCell) = State_VG(:,iCell) - &
            (sign(0.25, dVarUp_V) + sign(0.25, dVarDown_V))* &
            min(Beta*abs(dVarUp_V), Beta*abs(dVarDown_V), &
            cThird*abs(2*dVarDown_V+dVarUp_V))
       pLeft_VF(:,iCell+1) = State_VG(:,iCell) + &
            (sign(0.25, dVarUp_V) + sign(0.25, dVarDown_V))* &
            min(Beta*abs(dVarUp_V), Beta*abs(dVarDown_V), &
            cThird*abs(dVarDown_V+2*dVarUp_V))
    end do
    ! Propagate the old up slope to become the down slope
    dVarDown_V = dVarUp_V
    ! Calculate the up slope. The half of this (as assumed in the
    ! limiter function) approximates the face value
    dVarUp_V = min(Ds_G(-1)/(0.50*Ds_G(-1) + Ds_G(0)),1.0)*&
         (State_VG(:,0) - State_VG(:,-1))
    if(present(LeftFace0_V))then
       if(DoLimitLogVar)then
          if(any(LimiterState_V(iLogVar_V)<=0.0))&
               call CON_stop('Negative pressure/density in the limiter state')
          LimiterState_V(iLogVar_V) = log(LimiterState_V(iLogVar_V))
       end if
       ! This is the slope between the rightmost physical cell
       ! on the thread, State_VG(:,-1), and the leftmost state in the
       ! physical cell in the SC, which is passed here as LimiterState_V
       DeltaLtd_V = LimiterState_V - State_VG(:,-1)

       ! Now, we limit dVarUp_V to get the limited-slope-interpolated
       ! value at the boundary face lay between the said two physical states
       dVarUp_V = (sign(0.5, dVarUp_V) + sign(0.5, DeltaLtd_V))*&
            min(abs(dVarUp_V), abs(DeltaLtd_V))
    end if
    ! Calculate and apply the limited slopes, to get the limited
    ! reconstructed values:
    pRight_VF(:,-1) = State_VG(:,-1) - &
         (sign(0.25, dVarUp_V) + sign(0.25, dVarDown_V))* &
         min(Beta*abs(dVarUp_V), Beta*abs(dVarDown_V), &
         cThird*abs(2*dVarDown_V+dVarUp_V))
    pLeft_VF(:,0) = State_VG(:,-1) + &
         (sign(0.25, dVarUp_V) + sign(0.25, dVarDown_V))* &
         min(Beta*abs(dVarUp_V), Beta*abs(dVarDown_V), &
         cThird*abs(dVarDown_V+2*dVarUp_V))
    if(DoLimitLogVar)then
       State_VG(iLogVar_V,-nCell-1:0) = exp(&
            State_VG(iLogVar_V,-nCell-1:0))
       pRight_VF(iLogVar_V,-nCell:-1) = exp(&
            pRight_VF(iLogVar_V,-nCell:-1))
       pLeft_VF(iLogVar_V,-nCell:0) = exp(&
            pLeft_VF(iLogVar_V,-nCell:0))
    end if
    ! 2. Apply right BC on the external boundary (# = 0)
    if(present(RightFace0_V))then
       pRight_VF(:,0) = RightFace0_V
       call limit_p_tot(pRight_VF(:,0), PtotLim)
       if(pRight_VF(U_,0) > 0.0)then
          pRight_VF(U_,0) = min(pRight_VF(U_,0), uLim)
       else
          pRight_VF(U_,0) = 0.0
       end if
    else
       ! First order BC
       pRight_VF(:,0) = State_VG(:,0)
    end if
    ! 2. Save left BC on the external boundary (# = 0)
    if(present(LeftFace0_V))LeftFace0_V =  pLeft_VF(:,0)
    ! Get fluxes
    ! Loop over faces:
    do iFace = -nCell, 0
       call get_thread_flux(iFace, &
            pLeft_VF(:,iFace), pRight_VF(:,iFace), &
            Flux_VF(:,iFace), Cleft_F(iFace), Cright_F(iFace), &
            Un_F(iFace), Rho_F(iFace), pTot_F(iFace))
       VaFace_F(iFace) = B_F(iFace)/sqrt(cMu*Rho_F(iFace))
    end do
    Un_F(0) = max(Un_F(0), -min(uLim, MassFluxLim/&
         (FaceArea_F(0)*Rho_F(0))))
    do iCell = -nCell,-1
       DivU_C(iCell)  = vInv_C(iCell)*(Un_F(iCell+1)*FaceArea_F(iCell+1) &
            - Un_F(iCell)*FaceArea_F(iCell))
       VaCell_C(iCell) = 0.50*(VaFace_F(iCell+1) + VaFace_F(iCell))
       DissMajor_C(iCell) = 2.0*sqrt(PoyntingFluxPerBsi*cMu*&
            State_VG(Wminor_,iCell)*VaCell_C(iCell))/LperpTimesSqrtBsi
       DissMinor_C(iCell) = 2.0*sqrt(PoyntingFluxPerBsi*cMu*&
            State_VG(Wmajor_,iCell)*VaCell_C(iCell))/LperpTimesSqrtBsi
       DvaDs = (VaFace_F(iCell+1) - VaFace_F(iCell))/Ds_G(iCell)
       Reflection_C(iCell) = sign(min(abs(DvaDs),0.50*abs(&
            DissMajor_C(iCell) - DissMinor_C(iCell))),&
            State_VG(Wmajor_,iCell) - State_VG(Wminor_,iCell))*&
            sqrt(State_VG(Wmajor_,iCell)*State_VG(Wminor_,iCell))
    end do
    where(OpenThread1%R_F(-nCell:-1)<rMinReflectionTr)&
         Reflection_C = 0.0
    if(iStage==1)then
       do iCell = -nCell, -1
          ! Get dt
          ! Dt_C(iCell) = CflLocal * OpenThread1%Ds_G / Cmax_I
          ! Cmax is the maximum of the right wave
          ! speed from the left face and the left wave speed
          ! from the right face
          Dt_C(iCell) = CflLocal/max(vInv_C(iCell)* &
               abs(max(Cright_F(iCell)*FaceArea_F(iCell),&
               -Cleft_F(iCell+1)*FaceArea_F(iCell+1))) + & ! CFL +
               max(DissMajor_C(iCell),DissMinor_C(iCell)),&! diss rate
               cooling_rate(RhoSi=State_VG(Rho_,iCell),&  ! or cooling
               PeSi=State_VG(Pe_,iCell)))                 ! rate
       end do
       OpenThread1%Dt = minval(Dt_C(-nCell:-1))
       if(present(DtIn))OpenThread1%Dt = min(OpenThread1%Dt, DtIn)
       if(IsTimeAccurate)Dt_C(-nCell:-1) = OpenThread1%Dt
    end if
    !
    ! Advance the conserved variables to the half or full time step,
    ! depending on the stage
    !
    do iCell = -nCell, -1
       ! Calculate source terms
       ! Wave pressure, convert from the representative functions
       WavePress = 0.50*&
            sum(State_VG(Wmajor_:Wminor_,iCell))*PoyntingFluxPerBsi*&
            sqrt(cMu*State_VG(Rho_,iCell))
       Pperp = State_VG(P_,iCell) + 0.50*(State_VG(P_,iCell) -&
            State_VG(Ppar_,iCell))
       ! Heating and partitioning
       if(UseStochasticHeating)then
          call apportion_heating(&
               ! Inputs, all in SI:
               PparIn = State_VG(Ppar_,iCell)          ,&
               PperpIn= Pperp                          ,&
               PeIn   = State_VG(Pe_,iCell)            ,&
               RhoIn  = State_VG(Rho_,iCell)           ,&
               BIn    = 0.50*(B_F(iCell) + B_F(iCell+1)),&
               WmajorIn = State_VG(Wmajor_,iCell)       ,&
               WminorIn = State_VG(Wminor_,iCell)       ,&
               DissRateMajorIn = DissMajor_C(iCell)     ,&
               DissRateMinorIn = DissMinor_C(iCell)     ,&
               ! Outputs:
               QparPerQtotal     = QparPerQtotal        ,&
               QperpPerQtotal    = QperpPerQtotal       ,&
               QePerQtotal       = QePerQtotal)
       end if
       Heating = &
            (DissMajor_C(iCell)*State_VG(Wmajor_,iCell)&
            + DissMinor_C(iCell)*State_VG(Wminor_,iCell))*&
            PoyntingFluxPerBsi*sqrt(cMu*State_VG(Rho_,iCell))
       PparHeating  = QparPerQtotal *Heating
       PperpHeating = QperpPerQtotal*Heating
       PeHeating    = QePerQtotal   *Heating
       ! Gravity force acceleration, g, projected onto the field direction
       ! Is calculated as the negative of increment in gravity potential
       ! divided by the mesh length.
       GravitySource =  cGravityPotAt1Rs* & ! Calc -\delta Grav.Pot.
            (1/OpenThread1%R_F(iCell) - 1/OpenThread1%R_F(iCell+1))&
            /OpenThread1%Ds_G(iCell)&  ! Divide by \delta s
            *State_VG(Rho_,iCell)      ! Gravity force density, rho*g:
       !
       ! Get full source vector
       !
       Source_V(Rho_:Energy_)     = [0.0, &  ! No density source
            (pTot_F(iCell) - pTot_F(iCell+1))/Ds_G(iCell)  &
            + GravitySource,                               & ! Momentum
            (State_VG(Pe_,iCell) + WavePress)*DivU_C(iCell) + GravitySource*&
            State_VG(U_,iCell) + PparHeating + PperpHeating] ! Energy
       if(UseAnisoPressure)then
          ! Face area difference divided per control volume, is used
          ! the contribution to momentum source  from the pressure anisotropy
          DuDs = (Un_F(iCell+1) - Un_F(iCell))/Ds_G(iCell)
          Source_V(Ppar_)= -State_VG(Ppar_,iCell)*DuDs + PparHeating ! Ppar
          Source_V(RhoU_) = Source_V(RhoU_) + Pperp*vInv_C(iCell)* &
            (FaceArea_F(iCell+1) - FaceArea_F(iCell) )
       end if
       Source_V(Pe_:) = [&
            -State_VG(Pe_,iCell)*DivU_C(iCell) + PeHeating, & ! Electrons
            -DissMajor_C(iCell)*State_VG(Wmajor_,iCell) &
            - Reflection_C(iCell),               & ! Wmajor
            -DissMinor_C(iCell)*State_VG(Wminor_,iCell) &
            + Reflection_C(iCell)]                 ! Wminor
       ! Add fluxes to source
       ! dU = dt / Volume * (F_{i-1/2}*FaceArea_{i-1/2} &
       !      -F_{i+1/2}*FaceArea_{i+1/2}) + Dt*Source
       Source_V = Source_V + vInv_C(iCell)* &
            (Flux_VF(:,iCell)*FaceArea_F(iCell) - &
            Flux_VF(:,iCell+1)*FaceArea_F(iCell+1) )
       ! Apply source
       State_VG(:,iCell) = ConservativeOld_VC(:,iCell) + &
            StageCoef*Dt_C(iCell)*Source_V
    end do
    !
    ! Finalize the stage
    !
    ! Transform to primitives, set minimum pressure and density
    do iCell = -nCell, -1
       State_VG(Rho_,iCell) = max(MinRho, State_VG(Rho_,iCell) )
       State_VG(U_,iCell)   = State_VG(RhoU_,iCell)/State_VG(Rho_,iCell)
       pLimit = max(MinPress,  State_VG(Rho_,iCell)*cBoltzmann*TeSiMin/&
            cProtonMass)
       ! 3/2*P = Energy - 1/2*Rho U**2
       State_VG(P_,iCell)   = max(pLimit, cTwoThird*(State_VG(Energy_,iCell)&
            - 0.5*State_VG(Rho_,iCell)*State_VG(U_,iCell)**2 ))

       ! EnergyPar = 1/2*Ppar
       if(UseAnisoPressure)State_VG(Ppar_,iCell)   = &
            max(pLimit, 2*State_VG(Ppar_,iCell))
       ! Convert electron internal energy, 3/2*Pe, to pressure
       State_VG(Pe_,iCell)  = max(Z*pLimit, cTwoThird*State_VG(Pe_,iCell))
       Te_G(iCell) = State_VG(Pe_,iCell)*cProtonMass/&
            (Z*cBoltzmann*State_VG(Rho_,iCell))
    end do
    ! Apply artificial viscosity
    pViscous_C(-nCell:-1) = 0.25*(State_VG(U_,-nCell-1:-2) + &
         State_VG(U_,-nCell+1:0)) - 0.5*State_VG(U_,-nCell:-1)
    where(State_VG(U_,-nCell:-1) < 0.0) State_VG(U_,-nCell:-1) = &
         State_VG(U_,-nCell:-1) + pViscous_C
    if(iStage==1)RETURN
    ! Semi-implicit stage
    ! First, solve implicit equation
    ! Wmajor_i^{n+1} -Wmajor_i^n +(Va*Dt/Ds)*(-W^{n+1}_{i-1} + W^{n+1)_i)
    do iCell = -nCell, -1
       VaDtOverDs_C(iCell) = VaCell_C(iCell)*Dt_C(iCell)/Ds_G(iCell)
       State_VG(Wmajor_,iCell) = ( State_VG(Wmajor_,iCell) +&
            State_VG(Wmajor_,iCell-1)*VaDtOverDs_C(iCell)) / &
            (1 + VaDtOverDs_C(iCell))
    end do
    ! Second, solve implicit equation
    ! Wminor_i^{n+1} -Wminor_i^n -(Va*Dt/Ds)*(+W^{n+1}_{i+1} - W^{n+1)_i) = 0
    do iCell = -1, -nCell, -1
       State_VG(Wminor_,iCell) = ( State_VG(Wminor_,iCell) +&
            State_VG(Wminor_,iCell+1)*VaDtOverDs_C(iCell)) / &
            (1 + VaDtOverDs_C(iCell))
    end do
    if(any(State_VG(U_,-nCell:-1) < -1.3e5))then
       call save_plot_thread(OpenThread1,'high_downstream_speed.out')
       call CON_stop('See file high_downstream_speed.out')
    end if
  contains
    !==========================================================================
    real function p_tot(State_V)

      real, intent(in) :: State_V(Rho_:Wminor_)
      !------------------------------------------------------------------------
      if(State_V(Rho_)<0.0)then
         write(*,*)'Rho=',State_V(Rho_)
         call CON_stop('Negative density')
      end if
      p_tot = State_V(Ppar_) + State_V(Pe_) + 0.50*sqrt(cMu*State_V(Rho_))*&
           sum(State_V(Wmajor_:Wminor_))*PoyntingFluxPerBsi
    end function p_tot
    !==========================================================================
    real function speed_of_sound(State_V)

      real, intent(in) :: State_V(Rho_:Wminor_)
      ! Contributions to the speed of sound
      real :: C2i, C2e, C2Wmajor, C2Wminor
      ! Misc
      real :: RhoInv, SqrtMuRho, pWaveMajor, pWaveMinor

      !------------------------------------------------------------------------
      RhoInv = 1/State_V(Rho_)
      SqrtMuRho = sqrt(cMu*State_V(Rho_))*PoyntingFluxPerBsi
      ! The contributions to the speed of sound, from electrons and ions
      C2i = Gamma*State_V(Ppar_)*RhoInv
      C2e = Gamma*State_V(Pe_)  *RhoInv
      ! Contributions from wave pressure
      pWaveMajor = 0.50*State_V(Wmajor_)*SqrtMuRho
      C2Wmajor = 0.50*pWaveMajor*RhoInv
      pWaveMinor = 0.50*State_V(Wminor_)*SqrtMuRho
      C2Wminor = 0.50*pWaveMinor*RhoInv
      speed_of_sound = sqrt(C2i + C2e + C2Wmajor + C2WMinor)
    end function speed_of_sound
    !==========================================================================
    subroutine limit_p_tot(State_V, PtotLim)

      real, intent(inout) :: State_V(Rho_:Wminor_) ! State in which to limit P
      real, intent(in)    :: PtotLim ! Limiting value for Ptot
      real :: Ptot                   ! Ptot in State_V
      real :: PtotFactor             ! Increment in pTotal
      real, parameter :: Beta = 1.01 ! Ptot is limited to be below Beta*PtotLim
      !------------------------------------------------------------------------
      Ptot = p_tot(State_V)
      if(Ptot <= Beta*PtotLim) RETURN
      PtotFactor = Beta*PtotLim/Ptot
      State_V(Rho_) = PtotFactor*State_V(Rho_)
      State_V(p_:Pe_)   = PtotFactor*State_V(p_:Pe_)
      State_V(Wmajor_:Wminor_) = sqrt(PtotFactor)*State_V(Wmajor_:Wminor_)
    end subroutine limit_p_tot
    !==========================================================================
    subroutine get_thread_flux(iFace,pLeft_V, pRight_V, &
         Flux_V, Cleft, Cright, UnFace, RhoFace, PtotFace)

      use ModExactRS, ONLY: pressure_function
      use ModLookupTable, ONLY: interpolate_lookup_table
      integer, intent(in):: iFace
      real,   intent(in) :: pLeft_V(Rho_:Wminor_), pRight_V(Rho_:Wminor_)
      real,  intent(out) :: Flux_V(Rho_:Wminor_)
      real,  intent(out) :: Cleft, Cright, UnFace, RhoFace, PtotFace

      real    :: UpwindState_V(Wmajor_:Wminor_), Pe, PeL, PeR, CsL, CsR, &
           DensityRatio, PparFace, ExtraP, ExtraPright, ExtraPleft
      ! To calculate AW flux, if needed
      logical :: UseArtificialWind = .false.
      real, dimension(Rho_:Wminor_) :: ConsL_V, ConsR_V, FluxL_V, FluxR_V
      real :: WeightL, WeightR, Diffusion
      real :: FLD, FR, FRD ! Pressure (F)unctions and its (D)erivatives
      real :: POld, Pavr, uCh   ! Iterative values for pressure
      integer,parameter::nIterMax=10
      integer :: iIter
      real, parameter :: TolP=0.0010, ChangeStart=2.0*TolP
      real :: Change, MaxRhoU
      logical :: UseLimU
      ! Variables from routine guess_p in ModExactRS
      real::ImpedanceL, ImpedanceR, ImpedanceTotalInv
      real, parameter :: cTinyFractionOf=1.0e-8
      character(len=*), parameter:: NameSub = 'get_thread_flux'
      !------------------------------------------------------------------------
      RhoL = pLeft_V(Rho_ )
      PeL  = pLeft_V(Pe_)
      ExtraPleft = PeL + sum(pLeft_V(Wmajor_:Wminor_))*0.5*&
           sqrt(cMu*RhoL)*PoyntingFluxPerBsi
      pL   = pLeft_V(Ppar_) + ExtraPleft
      UnL  = pLeft_V(U_)

      RhoR = pRight_V(Rho_ )
      PeR  = pRight_V(Pe_)
      ExtraPright = PeR + sum(pRight_V(Wmajor_:Wminor_))*0.5*&
           sqrt(cMu*RhoR)*PoyntingFluxPerBsi
      pR   = pRight_V(Ppar_) + ExtraPright
      UnR  = pRight_V(U_)

      ! exact solver
      if(UseAnisoPressure)then
         CsL = sqrt(Gamma*pL/RhoL); CsR = sqrt(Gamma*pR/RhoR)
         Cleft  = min(UnL - CsL, UnR - CsR, 0.0)
         Cright = max(UnL + CsL, UnR + CsR, 0.0)
      elseif(iFace == -nCell)then
         CsR = sqrt(Gamma*pR/RhoR); CsL = sqrt(Gamma*pL/RhoL)
         ! subroutine guess_p from ModExactRS
         ImpedanceL = RhoL*CsL; ImpedanceR = RhoR*CsR
         ImpedanceTotalInv = 1.0/(ImpedanceL+ImpedanceR)
         ! Here, Pavr is the weighted average of the left and right Ptot
         PAvr = ImpedanceTotalInv*(PL*ImpedanceR + PR*ImpedanceL)
         POld = max(cTinyFractionOf*PAvr,  PAvr + &
              (UnL - UnR)*ImpedanceTotalInv*ImpedanceL*ImpedanceR)
         MaxRhoU = MassFluxLim/FaceArea_F(-nCell)
         UseLimU = .false.
         ! Initialize loop:
         Change = ChangeStart; iIter = 0
         do while(Change > TolP .and. iIter < nIterMax)
            if(.not.UseLimU)then
               ! Calculate velocity of the outflow from TR
               call convert_p_tot_to_rho(Pold, OpenThread1%TeTr, &
                    OpenThread1%TeTr, RhoFace, &
                    sum(pLeft_V(Wmajor_:Wminor_)))
               Pavr = (RhoFace/cProtonMass)*cBoltzmann*&
                    OpenThread1%TeTr*SqrtZ
               call interpolate_lookup_table(iTable=iTableTR, &
                    Arg1In=OpenThread1%TeTr  ,&
                    iVal=PavrL_              ,&
                    ValIn=Pavr*Ds_G(-nCell-1),&
                    Value_V=TrTable_V        ,&
                    Arg2Out=uCh              ,&
                    DoExtrapolate=.false.)
               UnFace = u_over_uch(OpenThread1%TeTr, uCh)*uCh
               if(uCh > 0.9999*uMax.or.uCh < uMin + 0.0001*uMax)then
                  call get_trtable_value(OpenThread1%TeTr, UnFace)
                  Pavr = TrTable_V(PavrL_)/Ds_G(-nCell-1)
                  RhoFace = cProtonMass*Pavr/&
                       (cBoltzmann*OpenThread1%TeTr*SqrtZ)
                  UseLimU = .true.
                  FLD = 0.0
               end if
               if(abs(UnFace)*RhoFace > MaxRhoU)then
                  UnFace = sign(MaxRhoU/RhoFace, UnFace)
                  UseLimU = .true.
                  FLD = 0.0
               end if
            end if
            if(.not.UseLimU)then
               WavePress = 0.50*PoyntingFluxPerBsi*&
                    sum(pLeft_V(Wmajor_:Wminor_))*sqrt(cMu*RhoFace)
               FLD = TrTable_V(DuOverDpL_)*Ds_G(-nCell-1)/(& !/(dPtot/dPavr)
                    SqrtZ            + & ! dPe/dPavr
                    1/SqrtZ          + & ! dPi/dPavr
                    0.50*WavePress/Pavr) ! dWavePress/dPavr
            end if
            call pressure_function(FR, FRD, POLD, RhoR, PR, CsR, Cright)
            PtotFace  = POld - (FR + UnR - UnFace)/(-FLD + FRD)
            if(PtotFace<=0.0)PtotFace = 0.10*pOld
            Change = 2.0*abs((PtotFace - POld)/(PtotFace + POld))
            POld   = PtotFace; iIter = iIter + 1
         end do
         if(iIter==nIterMax.and.Change > TolP)then
            write(*,*)'iFace =',iFace,' nCell=',nCell
            write(*,*)'Change=',Change
            write(*,*)'Ptot=',PtotFace,' PL=', PL, ' PR=', PR
            write(*,*)'U=',UnFace,' FR+UnR=',FR+UnR
            write(*,*)'FLD=',FLD,' FRD=',FRD
            call CON_stop('No convergence in '//NameSub)
         end if
         Cright = UnR + Cright
         Cleft = UnFace - sqrt(Gamma*PtotFace/RhoFace)
         OpenThread1%uTr = UnFace
         Pe = Pavr*SqrtZ
         OpenThread1%PeTr = Pe
         PparFace = Pavr/SqrtZ
         UpwindState_V = pLeft_V(Wmajor_:Wminor_)
         UseArtificialWind = .false.
      else
         call exact_rs_pu_star(UseAnotherRS=UseArtificialWind)
         ! Store WL WR
         Cleft = WL; Cright = WR
      end if

      if(UseArtificialWind.or.UseAnisoPressure)then
         ConsL_V = pLeft_V
         ! Except:
         ! Momentum density
         ConsL_V(RhoU_)   = pLeft_V(U_)*pLeft_V(Rho_)
         ! Energy density
         ConsL_V(Energy_) = 1.50*pLeft_V(P_) + &
              0.50*ConsL_V(RhoU_)*pLeft_V(U_)
         if(UseAnisoPressure)ConsL_V(Ppar_) = 0.5*pLeft_V(Ppar_)
         ! "Conserved" electron energy density is a 1.50*Pe
         ConsL_V(Pe_)  = 1.50*pLeft_V(Pe_)
         FluxL_V          = UnL*ConsL_V
         FluxL_V(Energy_) = FluxL_V(Energy_) + PL*UnL

         ConsR_V = pRight_V
         ! Except:
         ! Momentum density
         ConsR_V(RhoU_)   = pRight_V(U_)*pRight_V(Rho_)
         ! Energy density
         ConsR_V(Energy_) = 1.50*pRight_V(P_) + &
              0.50*ConsR_V(RhoU_)*pRight_V(U_)
         ! "Conserved" electron energy density is 1.5*Pe
         ConsR_V(Pe_)  = 1.50*pRight_V(Pe_)
         if(UseAnisoPressure)ConsL_V(Ppar_) = 0.5*ConsL_V(Ppar_)
         FluxR_V          = UnR*ConsR_V
         FluxR_V(Energy_) = FluxR_V(Energy_) + PR*UnR
         WeightL   = Cright/(Cright - Cleft)
         WeightR   = 1.0 - WeightL
         Diffusion = Cright*WeightR

         Flux_V = FluxL_V*WeightL + FluxR_V*WeightR + &
             Diffusion*(ConsL_V - ConsR_V)
         UnFace  = UnL* WeightL + UnR* WeightR
         RhoFace = RhoL*WeightL + RhoR*WeightR
         PtotFace = pL*WeightL + pR*WeightR
         RETURN
      elseif(iFace /= -nCell)then
         call exact_rs_sample(0.0, RhoFace, UnFace, PtotFace)
         if (UnStar  >  0) then
            ! Factor for variable scaling with density
            DensityRatio = RhoFace / pLeft_V(Rho_)
            UpwindState_V = (DensityRatio**(Gamma-0.50))*&
                 pLeft_V(Wmajor_:Wminor_)
            ExtraP = ExtraPleft
            Pe = PeL*(DensityRatio**Gamma)
         else
            DensityRatio = RhoFace / pRight_V(Rho_)
            UpwindState_V = (DensityRatio**(Gamma-0.50))*&
                 pRight_V(Wmajor_:Wminor_)
            ExtraP = ExtraPright
            Pe = PeR*(DensityRatio**Gamma)
         end if
         ! All contributions to pressure except for ion one are assumed
         ! to scale adiabatically with density:
         PparFace = PtotFace - ExtraP*DensityRatio**Gamma
      end if
      ! get the flux

      Flux_V(Rho_) = RhoFace*UnFace
      Flux_V(RhoU_) = RhoFace*UnFace**2
      Flux_V(Pe_) = 1.50*Pe*UnFace
      Flux_V(Energy_) = (0.50*RhoFace*UnFace**2 + 1.5*PparFace + &
           PtotFace)*UnFace
      Flux_V(Wmajor_:Wminor_) = &
           UpwindState_V(Wmajor_:Wminor_)*UnFace
    end subroutine get_thread_flux
    !==========================================================================
  end subroutine advance_thread_expl
  !============================================================================
  subroutine advance_thread_semi_impl(OpenThread1)

    type(OpenThread),intent(inout) :: OpenThread1
    ! Solver for electron heat conduction
    real :: Ti_C(-OpenThread1%nCell:-1)
    real :: N_C(-OpenThread1%nCell:-1)
        ! State vector:
    real, pointer :: State_VG(:,:)
    ! Electron temperature for semi-implicit scheme
    real, pointer :: Te_G(:)
    ! Local time step for a given cell
    real, pointer :: Dt_C(:)
    ! Cell size along the magnetic field line. In the leftmost ghost cell
    ! there is the length of analytical transition
    real, pointer :: Ds_G(:)
    integer :: nCell
    character(len=*), parameter:: NameSub = 'advance_thread_semi_impl'
    !--------------------------------------------------------------------------
    nCell = OpenThread1%nCell
    Dt_C=>OpenThread1%Dt_C
    State_VG => OpenThread1%State_VG
    Te_G => OpenThread1%Te_G
    ! Mesh size along the line
    Ds_G => OpenThread1%Ds_G
    ! Electron heat conduction and losses
    N_C(-nCell:-1) = State_VG(Rho_,-nCell:-1)/cProtonMass
    Ti_C(-nCell:-1) = State_VG(P_,-nCell:-1)/(cBoltzmann*N_C)
    call advance_heat_conduction(&
         nPoint = nCell, &
         Dt_I = Dt_C(-nCell:-1),    &
         Te_I = Te_G(-nCell:0),     &
         Ti_I = Ti_C(-nCell:-1),    &
         Ni_I = N_C(-nCell:-1),     &
         Ds_I = Ds_G(-nCell-1:0),   &
         uFace   = OpenThread1%uTr, &
         B_I  = OpenThread1%B_F(-nCell:0), &
         PeFaceOut = OpenThread1%PeTr, & ! Store state for the top of TR
         TeFaceOut = OpenThread1%TeTr, &
         DoLimitTimestep=.true.)
    State_VG(P_ ,-nCell:-1) = cBoltzmann*N_C*Ti_C(-nCell:-1)
    State_VG(Pe_,-nCell:-1) = cBoltzmann*N_C*Te_G(-nCell:-1)
    if(UseAnisoPressure)&
         State_VG(Ppar_ ,-nCell:-1) = State_VG(P_ ,-nCell:-1)
  end subroutine advance_thread_semi_impl
  !============================================================================
  subroutine solve_tr_face(TeCell, & ! cell-centered, in SI
       uFace,                      & ! face-centered
       LengthTr, DeltaS, & ! Distances photosphere-to-face, face-to-center
       TeFaceOut, HeatFluxOut, DFluxOverDConsOut, &
       PeFaceOut) ! SI, face centered

    ! For given values of Te and u*Pavr/Te at the cell center, all inputs
    ! are in SI and expressed in K and W/(m.K). The distance from the cell
    ! to the face is DeltaS, from the face to photosphere is LengthTr.
    ! All distances are measured along the magnetic field line in meters.
    ! Outputs provided are TeFace, PeFace, uFace, HeatFluxFace, all in SI:
    ! [K], [N/m^2], [m/s], [W/m^2], at the face.

    ! The plasma temperature at the cell center:
    real,    intent(in)    :: TeCell
    ! Velosity at the face
    real,    intent(inout) :: uFace
    ! Distances photosphere-to-face, face-to-center
    real,    intent(in)  :: LengthTr, DeltaS
    ! Parameters at the face, connected by the analytical TR solution to the
    ! photosphere:
    real, optional, intent(out) :: TeFaceOut
    real, optional, intent(out) :: HeatFluxOut, dFluxOverdConsOut
    real, optional, intent(out) :: PeFaceOut

    real    :: TeFace, HeatFluxFace
    real    :: ConsFace, dConsFace, ConsCell, Tolerance
    ! Newton-Rapson coefficients: dConsFace = -Res/dResdCons
    real    :: Res, dResdCons
    integer :: iCounter
    integer, parameter :: iCounterMax=20
    character(len=*), parameter:: NameSub = 'solve_tr_face'
    !--------------------------------------------------------------------------
    ConsCell = cTwoSevenths*HeatCondParSi*TeCell**3.5
    Tolerance = cTolerance*ConsCell
    ! In the first approximation, take TeFace = TeCell
    ConsFace = ConsCell
    TeFace = TeCell
    iCounter = 0
    do
       if(TeFace >= TeTrMax)then
          TeFace = TeTrMax
          call get_trtable_value(TeFace,uFace)
          if(present(TeFaceOut))TeFaceOut = TeFace
          HeatFluxFace = (ConsCell - ConsSiMax)/DeltaS
          if(present(HeatFluxOut))HeatFluxOut = HeatFluxFace
          if(present(dFluxOverdConsOut))dFluxOverdConsOut = 1/DeltaS
          if(present(PeFaceOut))&
               PeFaceOut = TrTable_V(PavrL_)*SqrtZ/LengthTr
          RETURN
       elseif(TeFace <= TeSiMin)then
          TeFace = TeSiMin
          call get_trtable_value(TeFace,uFace)
          if(present(TeFaceOut))TeFaceOut = TeFace
          HeatFluxFace = TrTable_V(HeatFluxL_)/LengthTr
          if(present(HeatFluxOut))HeatFluxOut = HeatFluxFace
          if(present(dFluxOverdConsOut))dFluxOverdConsOut = 0.0
          if(present(PeFaceOut))&
               PeFaceOut = TrTable_V(PavrL_)*SqrtZ/LengthTr
          RETURN
       end if
       call get_trtable_value(TeFace,uFace)
       HeatFluxFace = TrTable_V(HeatFluxL_)/LengthTr
       ! Solve equation HeatFlux = (ConsCell - ConsFace)/DeltaS
       Res = DeltaS*HeatFluxFace + ConsFace - ConsCell
       if(abs(Res) < Tolerance) EXIT
       if(iCounter == iCounterMax)then
          write(*,*)'ChromoEvapCoef=',ChromoEvapCoef
          write(*,*)'Lengthtr',LengthTr
          write(*,*)'TeCell=', TeCell
          write(*,*)'DeltaS=',DeltaS
          call CON_stop(NameSub//': No convergence')
       end if
       ! New iteration:
       iCounter = iCounter + 1
       dResdCons = DeltaS*TrTable_V(dHeatFluxLoverDcons_)/LengthTr + 1
       dConsFace = -Res/dResdCons
       ConsFace = ConsFace + dConsFace
       TeFace = (ConsFace*3.50/HeatCondParSi)**cTwoSevenths
    end do
    if(present(TeFaceOut))TeFaceOut = TeFace
    if(present(HeatFluxOut))HeatFluxOut = HeatFluxFace
    if(present(dFluxOverdConsOut))then
       dFluxOverdConsOut = TrTable_V(dHeatFluxLoverDcons_)/LengthTr
       dFluxOverdConsOut = dFluxOverdConsOut/(1 + DeltaS*dFluxOverdConsOut)
    end if
    if(present(PeFaceOut))&
         PeFaceOut = TrTable_V(PavrL_)*SqrtZ/LengthTr
  end subroutine solve_tr_face
  !============================================================================
  real function cooling_rate(RhoSi, PeSi)
    ! Volumetric radiation energy loss divided by the electron internal energy

    real, intent(in) :: RhoSi, PeSi
    real :: TeSi, NiSi, Cooling

    !--------------------------------------------------------------------------
    NiSi = RhoSi/cProtonMass
    TeSi = PeSi/(cBoltzmann*Z*NiSi)
    call get_trtable_value(TeSi)
    Cooling =TrTable_V(Lambda_)*Z*(cBoltzmann*NiSi)**2
    cooling_rate =  (3.50 + max(0.0, -TrTable_V(DLogLambdaOverDLogT_)))*&
               Cooling/(1.5*PeSi)
  end function cooling_rate
  !============================================================================
  subroutine advance_heat_conduction_ta(nPoint, Dt, Te_I, Ti_I, Ni_I, Ds_I, &
       uFace, B_I, BCellIn_I, PeFaceOut, TeFaceOut, DoLimitTimestep)
    integer, intent(in)    :: nPoint
    real,    intent(in)    :: Dt
    real,    intent(inout) :: Te_I(nPoint+1), Ti_I(nPoint)
    real,    intent(in)    :: Ni_I(nPoint), Ds_I(0:nPoint+1), B_I(nPoint+1)
    real,    intent(inout) :: uFace
    real, optional, intent(in) :: BCellIn_I(nPoint)
    real, optional, intent(out) :: PeFaceOut, TeFaceOut
    logical, optional, intent(in) :: DoLimitTimestep
    ! MISC:
    real :: Dt_I(nPoint)
    !--------------------------------------------------------------------------
    Dt_I = Dt
    call advance_heat_conduction_ss(nPoint, Dt_I, Te_I, Ti_I, Ni_I, Ds_I, &
       uFace, B_I, BCellIn_I, PeFaceOut, TeFaceOut, DoLimitTimestep)
  end subroutine advance_heat_conduction_ta
  !============================================================================
  subroutine advance_heat_conduction_ss(nPoint, Dt_I, Te_I, Ti_I, Ni_I, Ds_I,&
       uFace, B_I, BCellIn_I, PeFaceOut, TeFaceOut, DoLimitTimestep)

    integer, intent(in)    :: nPoint
    real,    intent(in)    :: Dt_I(nPoint)
    real,    intent(inout) :: Te_I(nPoint+1), Ti_I(nPoint)
    real,    intent(in)    :: Ni_I(nPoint), Ds_I(0:nPoint+1), B_I(nPoint+1)
    real,    intent(inout) :: uFace
    real, optional, intent(in) :: BCellIn_I(nPoint)
    real, optional, intent(out) :: PeFaceOut, TeFaceOut
    logical, optional, intent(in) :: DoLimitTimestep

    real :: Res_I(nPoint), bFaceInv_I(1:nPoint+1), BcellInv_I(nPoint), &
         Lower_I(nPoint), Main_I(nPoint), dMain_I(nPoint), Upper_I(nPoint),&
         SpecHeat_I(nPoint), ExchangeRate_I(nPoint),      &
         TeStart_I(nPoint), DeltaEnergy_I(nPoint), dTe_I(nPoint), &
         DtInv_I(nPoint), HeatFlux2Tr, dFluxOverdCons, Cooling
    real, parameter :: QuasiCfl = 0.50
    ! Misc:
    ! Loop variable
    integer :: iPoint, iIter
    integer :: nIterMax = 40
    !--------------------------------------------------------------------------
    BfaceInv_I = 1/B_I
    if(present(BcellIn_I))then
       BcellInv_I = 1/BcellIn_I
    else
       BcellInv_I = 0.50*(BFaceInv_I(1:nPoint) + BFaceInv_I(2:nPoint+1))
    end if
    ! Initialization
    TeStart_I(1:nPoint) = Te_I(1:nPoint)
    SpecHeat_I  = 1.50*cBoltzmann*Ni_I*Ds_I(1:nPoint)*BcellInv_I
    Main_I = 0.0; Upper_I = 0.0; Lower_I = 0.0
    ! Contribution from heat conduction fluxes
    ! Flux linearizations over small dTe
    Upper_I(nPoint) = -BFaceInv_I(nPoint+1)                      &
         *HeatCondParSi*(0.50*(Te_I(nPoint)+Te_I(nPoint+1)))**2.5&
         /(0.50*Ds_I(nPoint) + Ds_I(nPoint+1))
    Upper_I(1:nPoint-1) = -BFaceInv_I(2:nPoint)                  &
         *HeatCondParSi*(0.50*(Te_I(1:nPoint-1)+Te_I(2:nPoint)))**2.5&
         *2/(Ds_I(1:nPoint-1) + Ds_I(2:nPoint))
    Lower_I(2:nPoint) = Upper_I(1:nPoint-1)
    Main_I(2:nPoint) = -Upper_I(2:nPoint) - Lower_I(2:nPoint)
    Main_I(1) = - Upper_I(1)
    do iIter = 1,nIterMax
       ! Right heat fluxes
       Res_I(1:nPoint) = &
            (Te_I(1:nPoint) - Te_I(2:nPoint+1))*Upper_I(1:nPoint)
       ! Add left heat fluxes
       Res_I(2:nPoint) = Res_I(2:nPoint) + &
            (Te_I(2:nPoint) - Te_I(1:nPoint-1))*Lower_I(2:nPoint)
       call solve_tr_face(TeCell =        Te_I(1), &
            uFace = uFace,                         &
            LengthTr      = Ds_I(0),               &
            DeltaS        = 0.50*Ds_I(1),          &
            HeatFluxOut   = HeatFlux2Tr,           &
            DFluxOverDConsOut = dFluxOverdCons,    &
            PeFaceOut = PeFaceOut,                 &
            TeFaceOut = TeFaceOut)
       ! Add left heat flux to the TR
       Res_I(1) = Res_I(1) - HeatFlux2Tr*BFaceInv_I(1)
       ! Radiative cooling, limit timestep
       do iPoint = 1, nPoint
          call get_trtable_value(Te_I(iPoint))
          Cooling =Ds_I(iPoint)*BcellInv_I(iPoint)*Z*&
               TrTable_V(Lambda_)*(cBoltzmann*Ni_I(iPoint))**2
          Res_I(iPoint) = Res_I(iPoint) - Cooling
          ! Limit time step:
          if(iIter==1)then
             if(present(DoLimitTimestep))then
                DtInv_I(iPoint) = max(1/Dt_I(iPoint), (3.50 + max(0.0,&
                     -TrTable_V(DLogLambdaOverDLogT_)))*Cooling/      &
                     (QuasiCfl*Z*Te_I(iPoint)*SpecHeat_I(iPoint)))
             else
                DtInv_I(iPoint) = 1/Dt_I(iPoint)
             end if
          end if
          ! linearized -dCooling/dTe and energy change are included
          ! into iteration-dependent part of the main diagonal. Ensure
          ! the positivity of their total, to guarantee the diagonal
          ! dominance property
         dMain_I(iPoint) = max(0.0, &
              TrTable_V(DLogLambdaOverDLogT_)*Cooling/Te_I(iPoint) + &
              DtInv_I(iPoint)*Z*SpecHeat_I(iPoint))
       end do
       ! Change in the internal energy (to correct the energy source
       ! for the time-accurate mode):
       DeltaEnergy_I(1:nPoint) = Z*SpecHeat_I(1:nPoint)*DtInv_I* &
            (Te_I(1:nPoint) - TeStart_I(1:nPoint))
       Res_I(1:nPoint) = Res_I(1:nPoint) - DeltaEnergy_I(1:nPoint)
       ! Linearize left heat flux to the TR
       dMain_I(1) = dMain_I(1) + dFluxOverdCons*HeatCondParSi*&
            Te_I(1)**2.5*BFaceInv_I(1)
       call tridiag(n=nPoint,  &
            Lower_I=Lower_I(1:nPoint),&
            Main_I=Main_I(1:nPoint) + dMain_I(1:nPoint),&
            Upper_I=Upper_I(1:nPoint),&
            Res_I=Res_I(1:nPoint),  &
            W_I=dTe_I(1:nPoint))
       dTe_I(1:nPoint) = min(Te_I(1:nPoint), max(dTe_I(1:nPoint),&
            TeSiMin - Te_I(1:nPoint), - 0.5*Te_I(1:nPoint)))
       Te_I(1:nPoint) = Te_I(1:nPoint) + dTe_I(1:nPoint)
       if(all(abs(dTe_I(1:nPoint))<cTolerance*Te_I(1:nPoint)))EXIT
       if(iIter==nIterMax)then
          write(*,'(a,i4,a,es12.4,a,es12.4)')'At iIter=',iIter, &
               ' uFace=',uFace,' TeFace=',TeFaceOut
          write(*,'(a)')&
               'iPoint Te Res Heat DeltaE TeOld TeStart TiStart Ni'
          do iPoint = 1, nPoint
             write(*,'(i4,9es12.4)')iPoint - nPoint - 1,Te_I(iPoint),  &
                  Res_I(iPoint), Res_I(iPoint) + DeltaEnergy_I(iPoint),&
                  DeltaEnergy_I(iPoint), Te_I(iPoint) - dTe_I(iPoint),&
                  TeStart_I(iPoint), Ti_I(iPoint), Ni_I(iPoint)
          end do
          write(*,'(a,es12.4)')'Upper boundary, Te=', Te_I(nPoint+1)
          call CON_stop('No convergence in advance_heat_conduction')
       end if
    end do
    ! To treat the electron-ion energy exchange, one can
    ! solve the system of fully implicit equations as follows:
    ! Te^{n+1} - Te^{n} = dt*ExchangeRate*(Ti^{n+1} - Te^{n+1})
    ! Ti^{n+1} - Ti^{n} = - Z*dt*ExchangeRate*(Ti^{n+1} - Te^{n+1})
    ! The sum and difference of these equations read:
    ! Z*Te^{n+1} + Ti^{n+1} = Z*Te^{n} + Ti^{n}
    ! Te^{n+1} - Ti^{n+1} = (Te^{n} - Ti^{n})/[1 + (Z+1)*dt*ExchangeRate]
    ! The ultimate solution is as follows:
    ! Te^{n+1} = Te^{n} + (Ti^{n} - Te^{n})*dt*ExchangeRate/&
    !                   [1 + (Z+1)*dt*ExchangeRate]
    ! Ti^{n+1} = Ti^{n} - Z*(Ti^{n} - Te^{n})*dt*ExchangeRate/&
    !                   [1 + (Z+1)*dt*ExchangeRate]
    ! First, calculate dt*ExchangeRate
    ExchangeRate_I = cExchangeRateSi*Z**2*Ni_I/&
         (DtInv_I*Te_I(1:nPoint)**1.5)
    ! Account for implicit correction. Multiply by the temperature difference
    ExchangeRate_I = ExchangeRate_I/(1 + (Z+1)*ExchangeRate_I)*&
         (Ti_I(1:nPoint) - Te_I(1:nPoint))
    Te_I(1:nPoint) = Te_I(1:nPoint) + ExchangeRate_I
    Ti_I = Ti_I - Z*ExchangeRate_I
  end subroutine advance_heat_conduction_ss
  !============================================================================
  subroutine tridiag(n, Lower_I, Main_I, Upper_I, Res_I, W_I)
    ! Solve tri-diagonal system of equations:
    !  ||m_1 u_1  0....        || ||w_1|| ||r_1||
    !  ||l_2 m_2 u_2...        || ||w_2|| ||r_2||
    !  || 0  l_3 m_3 u_3       ||.||w_3||=||r_3||
    !  ||...                   || ||...|| ||...||
    !  ||.............0 l_n m_n|| ||w_n|| ||r_n||
    ! From: Numerical Recipes, Chapter 2.6, p.40.

    ! Input parameters
    integer, intent(in) :: n
    real,    intent(in) :: Lower_I(n), Main_I(n), Upper_I(n), Res_I(n)
    ! Output parameters
    real,    intent(out):: W_I(n)
    ! Misc
    integer:: j
    real:: Aux,Aux_I(2:n)
    !--------------------------------------------------------------------------
    if(Main_I(1) == 0.0) call CON_stop('Error in tridiag: Main_I(1)=0')
    Aux = Main_I(1)
    W_I(1) = Res_I(1)/Aux
    do j = 2, n
       Aux_I(j) = Upper_I(j-1)/Aux
       Aux = Main_I(j) - Lower_I(j)*Aux_I(j)
       if(Aux == 0.0) then
          write(*,*) 'M_I(j), L_I(j), Aux_I(j) = ',&
               Main_I(j),Lower_I(j),Aux_I(j)
          write(*,*) ' For j=',j
          call CON_stop('Tridiag failed')
       end if
       W_I(j) = (Res_I(j) - Lower_I(j)*W_I(j-1))/Aux
    end do
    do j = n-1, 1, -1
       W_I(j) = W_I(j) - Aux_I(j+1)*W_I(j+1)
    end do

  end subroutine tridiag
  !============================================================================
  subroutine apportion_heating(&
       ! Inputs, all in SI:
    !--------------------------------------------------------------------------
       PparIn, PperpIn, PeIn, RhoIn, BIn, &
       WmajorIn, WminorIn,                &
       DissRateMajorIn, DissRateMinorIn,  &
       ! Outputs:
       QparPerQtotal, QperpPerQtotal, QePerQtotal)

    use ModConst, ONLY: cElectronCharge
    ! Input pressures
    real, intent(in) :: PparIn, PperpIn, PeIn
    ! Input density and magnetic field
    real, intent(in) :: RhoIn, Bin
    ! Input wave energy densities
    real, intent(in) :: WmajorIn, WminorIn
    ! Dissipation rates:
    real, intent(in) :: DissRateMajorIn, DissRateMinorIn
    ! Outputs
    real, intent(out) :: QparPerQtotal, QperpPerQtotal, QePerQtotal

    real :: SqrtRho, Wmajor, Wminor, Qmajor, Qminor, P
    real :: BetaProton, BetaElectron, TeByTp, Vperp, Qtotal
    real :: DampingElectron, DampingPar, DampingPerp, DampingProton
    real :: GyroRadiusTimesB, InvGyroRadius, LperpInvGyroRad
    real :: WmajorGyro, WminorGyro, Wgyro
    real :: CascadeTimeMajor, CascadeTimeMinor, DeltaU, Epsilon, DeltaB, Delta
    real :: Qproton, QminorFraction, QmajorFraction
    real, parameter :: cTwoThird = 2.0/3.0, cThird = 1.0/3.0

#ifndef SCALAR
    character(len=*), parameter:: NameSub = 'apportion_heating'
    !--------------------------------------------------------------------------

    SqrtRho = PoyntingFluxPerBsi*sqrt(cMu*RhoIn)
    ! Recover wave energy densities from representatives
    Wmajor = WmajorIn*SqrtRho
    Wminor = WminorIn*SqrtRho
    ! Energy dissipation rates
    Qmajor = Wmajor*DissRateMajorIn
    Qminor = Wminor*DissRateMinorIn
    Qtotal = Qmajor + Qminor

    P = cTwoThird*PperpIn + cThird*PparIn
    BetaProton = 2.0*cMu*P/(Bin*Bin)
    TeByTp = PeIn/P

    BetaElectron = 2.0*cMu*PeIn/(Bin*Bin)

    DampingElectron = 0.01*sqrt(TeByTp/BetaProton) &
         *(1.0 + 0.17*BetaProton**1.3) &
         /(1.0 +(2800.0*BetaElectron)**(-1.25))
    DampingPar = 0.08*sqrt(sqrt(TeByTp))*BetaProton**0.7 &
               *exp(-1.3/BetaProton)

    ! Stochasting heating contributes to perpendicular ion heating.

    ! Perpendicular ion thermal speed
    Vperp = sqrt(2.0*PperpIn/RhoIn)

    GyroRadiusTimesB = Vperp*cProtonMass/cElectronCharge

    InvGyroRadius = Bin/GyroRadiusTimesB

    LperpInvGyroRad = InvGyroRadius*LperpTimesSqrtBsi/sqrt(Bin)

    WmajorGyro = Wmajor/sqrt(LperpInvGyroRad)
    WminorGyro = Wminor/sqrt(LperpInvGyroRad)

    Wgyro = WmajorGyro + WminorGyro

    ! Cascade timescale at the gyroscale
    CascadeTimeMajor = WmajorGyro/max(Qmajor,1e-30)
    CascadeTimeMinor = WminorGyro/max(Qminor,1e-30)

    ! For protons the following would be DeltaU and DeltaB at ion gyro
    ! radius, except that we assumed that the Alfven ratio is one.
    DeltaU = sqrt(Wgyro/RhoIn)
    DeltaB = sqrt(cMu*Wgyro)

    Epsilon = DeltaU/Vperp
    Delta = DeltaB/Bin

    ! Damping rate for stochastic heating.
    ! It interpolates between the beta<1 and 1<beta<30 version.
    ! This formula is at the moment only suitable for protons.
    DampingPerp = (StochasticAmplitude &
         *exp(-StochasticExponent/max(Epsilon,1e-15)) &
         + StochasticAmplitude2*sqrt(BetaProton) &
         *exp(-StochasticExponent2/max(Delta,1e-15))) &
         *RhoIn*DeltaU**3 &
         *InvGyroRadius/max(Wgyro,1e-15)

    ! Set k_parallel*V_Alfven = 1/t_minor (critical balance)
    DampingElectron = DampingElectron/max(CascadeTimeMinor,1e-30)
    DampingPar = DampingPar/max(CascadeTimeMinor, 1e-30)

    ! Total damping rate around proton gyroscale
    DampingProton = DampingElectron + DampingPar + DampingPerp

    QmajorFraction = DampingProton*CascadeTimeMajor &
         /(1.0 + DampingProton*CascadeTimeMajor)
    QminorFraction = DampingProton*CascadeTimeMinor &
         /(1.0 + DampingProton*CascadeTimeMinor)

    Qproton = (QmajorFraction*Qmajor &
         + QminorFraction*Qminor)/Qtotal

    QparPerQtotal = DampingPar/DampingProton*Qproton

    QperpPerQtotal = DampingPerp/DampingProton*Qproton

    QePerQtotal = 1 - QparPerQtotal - QperpPerQtotal
#endif
  end subroutine apportion_heating
  !============================================================================
  subroutine plot_tr(NamePlotFile, nGrid, TeSi, PeSi, iTable)

    use ModPlotFile, ONLY: save_plot_file
    use ModLookupTable, ONLY: interpolate_lookup_table, get_lookup_table
    use ModUtilities, ONLY: split_string, join_string

    character(LEN=*), intent(in) :: NamePlotFile

    ! Number of grid points. The grid is uniform if Te, but not in X
    integer, intent(in)  :: nGrid

    ! The plasma parameters on top of the transition region:
    real,    intent(in)  :: TeSi, PeSi

    ! The TR is mostly used to account for the integral of the spectral
    ! intensity across the transition region, which is tabulated in the
    ! lookup table. In this routine we can visualize the integrand
    integer, optional, intent(in)  :: iTable

    ! The model is parameterized in terms of PAvr=sqrt(Pi*Pe) at Ti=Te
    ! In terms of SqrtZ: PAvr = Pi*SqrtZ = Pe/SqrtZ
    real    :: PAvrSi

    ! 1D Grid across the TR
    real :: LengthSi_I(nGrid)

    ! Plot variables: Electron temperature and density in particles per cm3
    integer, parameter :: TeSi_ = 1, NeCgs_ = 2
    real, allocatable  :: Value_VI(:,:)
    real               :: DeltaTe      ! Mesh of a temperature
    real               :: PavrL_I(nGrid)
    integer            :: i             ! Loop variable

    ! Ion density in particles per cm3:
    real    :: NiCgs

    ! To work with the emissivity table, if any
    logical :: DoPlotEmissivity !=present(iTable)
    integer :: nValue, nVar

    ! Array for variable names
    integer, parameter:: MaxVar = 200
    character(len=20) :: NameVar_I(MaxVar)
    character(len=500):: NameVarTable, NameVarPlot, NameUnitPlot
    !--------------------------------------------------------------------------
    DoPlotEmissivity = present(iTable)
    if(DoPlotEmissivity)then
       call  get_lookup_table(&
            iTable=iTable,    &
            nValue=nValue,    &
            NameVar=NameVarTable)
       call split_string(NameVarTable, MaxVar, NameVar_I, nVar, &
         UseArraySyntaxIn=.true.)
       call join_string(NameVar_I(3:nValue+2),NameVarTable) ! only var names!
       NameVarPlot = 'Length Te Ne '//trim(NameVarTable)//' TeTop PeTop'
       NameVar_I(1:nValue) = 'Response/m'
       call join_string(NameVar_I(:nValue),NameUnitPlot)
       NameUnitPlot='m K 1/cm3 '//trim(NameUnitPlot)//' K N/m2'
       allocate(Value_VI(TeSi_:NeCgs_+nValue,1:nGrid))
    else
       NameVarPlot = 'Length Te Ne TeTop PeTop'
       NameUnitPlot= 'm K 1/cm3 K N/m2'
       allocate(Value_VI(TeSi_:NeCgs_,1:nGrid))
    end if

    PAvrSi = PeSi/SqrtZ
    DeltaTe = (TeSi - TeSiMin)/(nGrid - 1)
    Value_VI(TeSi_,1) = TeSiMin
    Value_VI(NeCgs_,1) = 1.0e-6*PeSi/(cBoltzmann*TeSiMin)

    call interpolate_lookup_table(iTableTr, TeSiMin, 0.0, TrTable_V, &
         DoExtrapolate=.false.)

    ! First value is now the product of the thread length in meters times
    ! a geometric mean pressure, so that
    PavrL_I(1) = TrTable_V(PavrL_)

    do i = 2, nGrid
       Value_VI(TeSi_,i) = Value_VI(TeSi_,i-1) + DeltaTe
       call interpolate_lookup_table(iTableTr, Value_VI(TeSi_,i), 0.0, &
            TrTable_V, DoExtrapolate=.false.)
       PavrL_I(i) = TrTable_V(PavrL_)
       Value_VI(NeCgs_,i) = 1.0e-6*PeSi/(cBoltzmann*Value_VI(TeSi_,i))
    end do

    ! Now, LPAvrSi is the length interval multiplied by PAvrSi.
    ! Get rid of the latter factor and offset coordinate so that]
    ! LengthSi_I(1) = 0
    LengthSi_I = (PavrL_I - PavrL_I(1))/PAvrSi
    if(DoPlotEmissivity)then
       do i = 1, nGrid
          call interpolate_lookup_table(iTable, Value_VI(TeSi_,i), 0.0, &
            Value_VI(NeCgs_,i), Value_VI(NeCgs_+1:NeCgs_+nValue,i),    &
            DoExtrapolate=.false.)
          ! Multiply response function by: (1) NeCgs*NiCgs;
          ! (2) factor 1e-26 for response functions in CHIANTI tables
          ! (3) factor 100 to convert response/(cm of LoS length) to that per m
          NiCgs = 1.0e-6*PAvrSi/(SqrtZ*cBoltzmann*Value_VI(TeSi_,i))
          Value_VI(NeCgs_+1:NeCgs_+nValue,i) =    &
               Value_VI(NeCgs_+1:NeCgs_+nValue,i)*&
               Value_VI(NeCgs_,i)*NiCgs*          &
               1.0e-26*                           & ! response function per cm
               1.0e2                                ! response function per m
       end do
    end if
    call save_plot_file(&
         NameFile=NamePlotFile,   &
         ParamIn_I = [TeSi, PeSi],&
         NameVarIn = NameVarPlot, &
         nDimIn=1,                &
         Coord1In_I = LengthSi_I, &
         VarIn_VI = Value_VI,     &
         StringHeaderIn = 'Analytical model for transition region: ['//&
         trim(NameUnitPlot)//']')
    deallocate(Value_VI)

  end subroutine plot_tr
  !============================================================================
  subroutine save_plot_thread(OpenThread1, NameFile)

    use ModPlotFile, ONLY: save_plot_file

    type(OpenThread),intent(inout) :: OpenThread1
    character(LEN=*), intent(in) :: NameFile
    integer, parameter :: sWPlus_ = 6, sWminus_ = 7,sTi_=8,  sTe_=9, &
         iPlotVar_V(Rho_:sWminus_) = [Rho_, U_, Ppar_,P_, Pe_, Wmajor_, Wminor_]
    real :: Value_VI(Rho_:sTe_+1,-nPointMax:0), Coord_I(-nPointMax:0)
    real, pointer :: State_VG(:,:)
    integer :: nCell
    !--------------------------------------------------------------------------
    nCell = OpenThread1%nCell
    State_VG => OpenThread1%State_VG
    Value_VI(Rho_:sWminus_,-nCell-1:0) = State_VG(iPlotVar_V,-nCell-1:0)
    if(sign(1.0, OpenThread1%OpenFlux) > 0)then
       Value_VI(sWplus_,-nCell-1:0) = State_VG(Wmajor_,-nCell-1:0)
       Value_VI(sWminus_,-nCell-1:0) = State_VG(Wminor_,-nCell-1:0)
    else
       Value_VI(sWplus_,-nCell-1:0) = State_VG(Wminor_,-nCell-1:0)
       Value_VI(sWminus_,-nCell-1:0) = State_VG(Wmajor_,-nCell-1:0)
    end if
    Value_VI(sTi_,-nCell-1:0) = State_VG(P_,-nCell-1:0)*cProtonMass/&
         (cBoltzmann*State_VG(Rho_,-nCell-1:0))
    Value_VI(sTe_,-nCell:0) = OpenThread1%Te_G(-nCell:0)
    Value_VI(sTe_,-nCell-1) = OpenThread1%TeTr
    Value_VI(sTe_+1,-nCell:-1) = 0.50*Si2Gs*&
         (OpenThread1%B_F(-nCell:-1) + OpenThread1%B_F(-nCell+1:0))
    Value_VI(sTe_+1,0) = Si2Gs*OpenThread1%B_F(0)
    Value_VI(sTe_+1,-nCell-1) = Si2Gs*OpenThread1%B_F(-nCell)
    Coord_I(-nCell:-1) = 0.5*(OpenThread1%R_F(-nCell:-1) +&
         OpenThread1%R_F(-nCell+1:0))
    Coord_I(0) = OpenThread1%R_F(0)
    Coord_I(-nCell-1) = OpenThread1%R_F(-nCell)
    call save_plot_file(NameFile = NameFile,  &
         nDimIn  = 1,                       &
         ParamIn_I= [OpenThread1%TeTr,&
         OpenThread1%uTr,&
         OpenThread1%PeTr,&
         OpenThread1%TMax,&
         sign(1.0,OpenThread1%OpenFlux),&
         OpenThread1%Dt],&
         VarIn_VI= Value_VI(:,-nCell-1:0), &
         TypeFileIn    = 'ascii',           &
         CoordIn_I  = Coord_I(-nCell-1:0), &
         StringHeaderIn  = 'Thread file ', &
         NameUnitsIn  = &
         '[Rsun] '//&
         '[kg/m3] [m/s] [J/m3] [J/m3] [J/m3] [J/m3] [J/m3] [K] [K] [Gs] '//&
         '[K] [m/s] [J/m3] [K] [1] [s]',&
         NameVarIn = 'R '//&
         'Rho U Ppar P Pe Wp Wm Ti Te B'//&
         ' TeTr uTr PeTr TMax SignB Dt' )
  end subroutine save_plot_thread
  !============================================================================
  subroutine test
    real :: PeFace, TeFace, HeatFluxFace, uFace = 0.0
    real :: Te_I(100) = 1.0e5  ! K
    real :: Ni_I(100) = 3.0e14 ! m-3
    real :: Ti_I(100) = 1.0e5  ! K
    real :: Pi_I(100), Pe_I(100)
    real :: Ds_I(0:100) = 1.0e-3*Rsun
    real :: B_I(100) = 5.0e-4      ! T
    integer :: iPoint, iTime
    real :: rFaceInv = 1/1.001, rInv_I(100) ! Both dimensionless
    !--------------------------------------------------------------------------
    call solve_tr_face(TeCell = 1.0e6, &
         uFace = uFace,                   & ! cell-centered, in SI
         LengthTr = 0.05*Rsun,            &
         DeltaS   = 0.0025*Rsun,          & ! Distance face-to-center
         TeFaceOut   = TeFace,            &
         PeFaceOut   = PeFace,            &
         HeatFluxOut = HeatFluxFace)
    write(*,'(a,es13.6)')'TeFace = ',TeFace
    write(*,'(a,es13.6)')'PeFace = ',PeFace
    write(*,'(a,es13.6)')'uFace  = ',uFace
    write(*,'(a,es13.6)')'Heat flux = ', HeatFluxFace
    Te_I(100) = tCorona; Ti_I(100) = tCorona
    Ds_I(100) = 0.5*Ds_I(100)
    write(*,*)'1/rFace=',rFaceInv
    do iPoint = 1, 99
       rInv_I(iPoint) = Rsun/(Rsun + sum(Ds_I(0:iPoint-1)) + 0.5*Ds_I(iPoint))
    end do
    rInv_I(100) = Rsun/(Rsun + sum(Ds_I))
    do iTime = 1,100
       call advance_heat_conduction_ta(99, 100.0, Te_I, Ti_I(1:99), &
            Ni_I(1:99), Ds_I, uFace, B_I, PeFaceOut = PeFace, &
            TeFaceOut=TeFace, DoLimitTimestep=.true.)
       call barometric_equilibrium(99, Te_I, Ti_I, rInv_I, &
       rFaceInv, PeFace, TeFace, Ni_I, Pe_I, Pi_I)
    end do

    write(*,'(a)')'iPoint   Te   Ti    Ni   Pi   Pe   1/R'
    write(*,'(i3,6es14.6)')0, TeFace, TeFace, PeFace/(cBoltzmann*TeFace), &
         PeFace/Z, PeFace, rFaceInv
    do iPoint = 1,100
       write(*,'(i3,6es14.6)')iPoint, Te_I(iPoint), Ti_I(iPoint), &
            Ni_I(iPoint), Pi_I(iPoint), Pe_I(iPoint), rInv_I(iPoint)
    end do
  end subroutine test
  !============================================================================
end module ModTransitionRegion
!==============================================================================
