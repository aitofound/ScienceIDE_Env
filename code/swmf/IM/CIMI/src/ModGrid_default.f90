Module ModCimiGrid
 
  use ModCimiPlanet,   ONLY: nspec

  implicit none
  
  logical, parameter :: DoUseUniformLGrid = .false.
  logical :: DoVerboseLatGrid = .false.
  
  ! define dimensions of CIMI grids
  integer,parameter :: np1=51,nt1=48,npit1=18!,nspec1=1  
  integer,parameter :: nm=48,nk=29 ! dimension of CIMI magnetic moment and K

  integer,parameter :: np=51    ! dimension of the CIMI latitude grid
  integer,parameter :: nt=48    ! dimension of the CIMI local-time grid
  integer	    :: neng=15  ! dimension of the CIMI energy grid
  integer,parameter :: npit=18  ! dimension of the CIMI pitch-angle grid

  ! These have to be initialized so that IM_set_grid does not fail on non-IM PEs
  real:: xlat(np) = 0.0, phi(nt1)=0.0

  real,parameter, dimension(0:52):: xlat_data=(/&
       11.812,13.777,15.742,17.705,19.665,21.622,23.576,25.527,27.473, &
       29.414,31.350,33.279,35.200,37.112,39.012,40.897,42.763,44.604, &
       46.409,48.163,49.837,51.382,52.725,53.823,54.720,55.488,56.175, &
       56.812,57.413,57.990,58.547,59.090,59.622,60.144,60.659,61.168, &
       61.671,62.170,62.666,63.159,63.649,64.137,64.624,65.109,65.593, &
       66.077,66.560,67.043,67.526,68.009,68.492,68.975,69.458/)


  real :: xlatr(np), xmlt(nt), dlat(np1), sinAo(npit)

  real :: varL(0:np+1)=0.,&
          dvarL=1.,&
          Lfactor(0:np+1) = 0.,&
          Lfactor1(0:np+1) = 0.,&
          xlatmin=9.8403398252184129,&
          xlatmax=72.4356255492731975,&
          varNpower=1.5
  logical :: DoDefineVarNpower = .false.

  real, allocatable :: d4Element_C(:,:,:,:) !4D element (dlat*dphi*dmm*dk)

  ! Defines the outer boundary maximum distance.
  real	  :: rb = 10.0
  
  ! Define MPI parameters affecting grid
  integer :: iProc, nProc, iComm, nLonPar, MinLonPar, MaxLonPar
  integer, allocatable :: nLonPar_P(:),nLonBefore_P(:)
  integer ::iProcLeft, iProcRight, iLonLeft, iLonRight
  integer :: iProcMidnight, iLonMidnight

  ! Define parameters for specifying control over Energy grid
  real :: MinIonEnergy=0.1, MaxIonEnergy=316.22777, ElectronGridRatio=10.
  logical :: UseLogEGrid = .true.

  ! Define parameters for specifying Energy grid corresponding to RBSP
  ! flux (MagEIS and REPT) instruments
  integer, parameter :: nRBSPEnergy = 30
  real, dimension(1:nRBSPEnergy) :: &
       energy_RBSP = (/  19.5,   32.7,    50.0,    71.8,    98.1, &
                        129.0,  146.0,   166.0,   207.0,   232.0, &
                        334.0,  450.0,   580.0,   722.0,   877.0, &
                       1040.0, 1200.0,  1740.0,  2300.0,  2520.0, &
                       2850.0, 3600.0,  3770.0,  4500.0,  5600.0, &
                       7150.0, 8800.0, 11650.0, 22550.0, 59450.0 /)
  logical :: UseRBSPGrid = .false.

contains

!-----------------------------------------------------------------------------
 subroutine init_lat
 ! This subroutine initializes and defines latitude grid
!-----------------------------------------------------------------------------
 use ModNumConst,    ONLY: cDegToRad
 implicit none

 integer i

 ! CIMI xlat grid for non-uniform grid
 do i=1,np
    xlat(i)=xlat_data(i)
    ! dlat in radian
    dlat(i)=0.5*(xlat_data(i+1)-xlat_data(i-1))*cDegToRad    
 enddo
 xlatr(:)=xlat(:)*cDegToRad

 end subroutine init_lat
!-----------------------------------------------------------------------------
     
end Module ModCimiGrid


