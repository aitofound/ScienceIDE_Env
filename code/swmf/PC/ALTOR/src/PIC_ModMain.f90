!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!==========================
module PIC_ModMain
  use ModNumConst
  use PC_ModSize, ONLY: nDim, MaxBlock, nPType, MaxDim
  implicit none
  SAVE
  !-----------------------
  integer:: nBlockMax, nTotBlocks = 1
  !If UseSharedField==.true. at all processors
  !the field is the same, only particles are distributed.
  logical, parameter:: UseSharedField = .false.
 
  logical:: IsInitialized = .false.

  logical:: UseVectorPotential = .true.

  ! logical::DoAccelerateLight = .false.

  real :: c = cHalf, c2 = 0.25
  real,dimension(MaxDim):: SpeedOfLight_D = 0.0

  !\
  ! Time stepping parameters and values.
  !/
  real :: tSimulation = 0.0
  integer :: iStep = 0
  real :: Dt= 0.0, DtRead = -1.0
  !\
  ! Grid spacing. In the AMR grid they all are block variables
  !/
  real :: CellVolume = 1.0, vInv = 1.0 
  real :: Dx_D(nDim) = 1.0, DxInv_D(nDim) = 1.0
  !\
  ! Macroparticle characteristic: number of electron macroparticles
  ! needed to simulate the "critical" electron density
  !/
  integer:: nPPerCellCrit=1
  !\
  ! Stopping conditions. These variables are only used in stand alone mode.
  !/
  real    :: tMax = -1.0, CpuTimeMax = -1.0
  integer :: nIter = -1
  logical :: UseStopFile = .true.
  logical :: IsLastRead=.false., IsFirstSession = .true.
  !\
  ! Progress variables
  !/
  integer :: nProgress1=10, nProgress2=100

  
  !Timing variables
  logical:: UseTiming = .true.
  integer:: nTiming = -2

  !\
  ! Boundary condition for the electric field
  !/
  !Read by the command
  !#FIELDBC
  !Example:
  !
  !#FIELDBC
  !laserbeam            TypeFieldBC_S(x<0)
  !noreflect            TypeFieldBC_S(x>nX.dx)
  !periodic             TypeFieldBC_S(y<0)
  !periodic             TypeFieldBC_S(y>nY.dy)
  !periodic             TypeFieldBC_S(z<0)
  !periodic             TypeFieldBC_S(z>nZ.dz)
  !
  !
  !Implemented types: periodic, noreflect, laserbeam
  
  character(LEN=10) :: TypeFieldBC_S(1:2*nDim)='periodic'
  
  
  logical:: IsPeriodicField_D(nDim) = .true.
  !\
  ! Initial conditions
  !/
  !\
  ! Uniform
  !/
  logical :: UseUniform = .false.
  integer :: nPPerCellUniform_P(nPType)=0
  !\
  ! Foil
  !/
  logical :: UseFoil = .false.
  integer :: nPPerCellFoil_P(nPType)
  real :: FoilCenter_D(nDim)=0.0
  real :: FoilWidth_D(nDim)=0.0
  real :: AngleFoil = 0.0  
  !\
  ! Thermalization
  !/
  logical :: UseThermalization = .false.
  real    :: uTh_P(nPType) = 0.0
end module PIC_ModMain
