!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module EEE_ModCommonVariables

  use ModConst
  use ModMpi
  use ModUtilities, ONLY: CON_stop

  implicit none
  save

  ! Named indices for directions
  integer, parameter :: x_=1,y_=2,z_=3

  ! prefix for writing EEE output
  character(len=5), parameter :: prefix='EEE: '

  ! My processor number
  integer :: iProc = 0

  ! Physics variables global to EEE
  real :: g = 5./3., inv_g = 3./5., gm1 = 2./3., inv_gm1 = 3./2.
  real :: Gbody = -cGravitation*mSun*cProtonMass/(cBoltzmann*1.5e6)
  !$acc declare create(Gbody)

  ! Named indexes for I/O variable units
  integer, parameter :: nIoUnit = 15

  integer, parameter :: UnitX_           = 1
  integer, parameter :: UnitU_           = 2
  integer, parameter :: UnitRho_         = 3
  integer, parameter :: UnitT_           = 4
  integer, parameter :: UnitN_           = 5
  integer, parameter :: UnitP_           = 6
  integer, parameter :: UnitB_           = 7
  integer, parameter :: UnitRhoU_        = 8
  integer, parameter :: UnitEnergyDens_  = 9
  integer, parameter :: UnitPoynting_    = 10
  integer, parameter :: UnitJ_           = 11
  integer, parameter :: UnitElectric_    = 12
  integer, parameter :: UnitTemperature_ = 13
  integer, parameter :: UnitDivB_        = 14
  integer, parameter :: UnitAngle_       = 15

  ! Conversion between units: e.g. VarSi = VarNo*No2Si_V(UnitVar_)
  ! The following should always be true: No2Si_V*Si2Io_V = No2Io_V
  real, dimension(nIoUnit) :: &
       Io2Si_V, Si2Io_V, Io2No_V, No2Io_V, Si2No_V, No2Si_V
  !$acc declare create(Io2Si_V, Si2Io_V, Io2No_V, No2Io_V, Si2No_V, No2Si_V)

  ! Switch on CME (boundary and/or initial conditions)
  logical:: UseCme = .false.
  !$acc declare create(UseCme)

  ! Use Gibbson-Law, Titov-Demoulin, spheromak flux ropes
  logical:: UseGL  = .false., UseTD = .false., UseSpheromak = .false., &
       UseMagCone = .false.
  !$acc declare create(UseGL, UseTD, UseSpheromak, UseMagCone)

  ! Use Titov-Demoulin 2014 or 2022
  logical:: UseTD14 = .false., UseTD22 = .false.
  !$acc declare create(UseTD14, UseTD22)

  ! Logicals to add the CMEs
  logical:: DoAddGL = .false., DoAddTD = .false., DoAddSpheromak = .false., &
       DoAddMagCone = .false.
  !$acc declare create(DoAddGL, DoAddTD, DoAddSpheromak, DoAddMagCone)

  ! Use shear-flow boundary condition, use arcade magnetic field
  logical:: UseShearFlow = .false., UseArch = .false.

  ! Use CMS nonlinear force free model
  logical:: UseCms = .false.

  ! Add flux rope as an initial condition or apply boundary conditions only
  logical:: DoAddFluxRope = .false.

  ! CME location and orientation
  real :: LongitudeCme = 0.0, LatitudeCme = 0.0, OrientationCme = 0.0

  ! Direction vector from the heliocenter to the CME center:
  real :: DirCme_D(3) = 0.0

  ! Coordinate vectors of the CME center, apex, and bottom
  logical :: DoNormalizeXyz = .false.
  real :: XyzCmeCenterSi_D(3) = 0.0, XyzCmeApexSi_D(3) = 0.0
  real :: XyzCmeBottomSi_D(3) = 0.0
  real :: rCmeApexInvSi = 0.0
  !$acc declare create(rCmeApexInvSi)

  ! The AMBIENT magnetic field at these points
  real :: bAmbientCenterSi_D(3) = 0.0, bAmbientApexSi_D(3) = 0.0
  real :: bAmbientBottomSi_D(3) = 0.0

  ! Starting time of CME eruption
  real    :: tStartCme = -1.0
  !$acc declare create(tStartCme)

  ! Decay time of CME boundary conditions
  real    :: tDecayCmeDim = -1.0, tDecayCme = -1.0
  !$acc declare create(tDecayCmeDim, tDecayCme)

  ! Plotting options:
  ! Lineaar resolution, in terms of the unit of length.  For solar
  ! applications, the magnetogram resolution of 1 degree, corresponds
  ! to the linear resolution of about 1/60
  real, parameter :: DXyzPlot = 0.0150

  ! Extended plot domain size in terms of the real size of configuration
  real, parameter :: ExtensionFactor = 1.50

end module EEE_ModCommonVariables
!==============================================================================
