Module ModGmCimi

  use ModCimiGrid,  ONLY: nLat => np, nLon => nt
  use ModCimiPlanet,ONLY: nspec

  implicit none

  real, allocatable :: StateLine_VI(:,:),StateBmin_IIV(:,:,:)
  integer :: iLineIndex_II(nLon,1:nLat),nPoint, nVarBmin
  integer :: TotalRho_=-1, TotalP_=-1, TotalPpar_=-1, TotalPe_=-1
  integer, dimension(nspec-1) :: iBufferRho_I 
  integer, dimension(nspec-1) :: iBufferP_I
  integer, dimension(nspec-1) :: iBufferPpar_I   
  real :: rBodyGm=2.5

  ! index of conjugate lat and lon in StateBmin array
  integer :: ConjugateLat_=4,ConjugateLon_=5
  logical :: UseTotalRhoGm = .false.
  logical :: UseTotalPGm   = .false.
  logical :: UseTotalPparGm= .false.
  logical :: UseMultiRhoGm = .false.
  logical :: UseMultiPGm   = .false.
  logical :: UseMultiPparGm= .false.
  logical :: DoFeedbackPs  = .false.
  logical :: UsePeGm       = .false.
  !number of variables extacted along field
  integer,parameter :: nVar=4

  real :: KpGm=-1.0, AeGm=-1.0
  logical :: UseGmKp=.false.,UseGmAe=.false.
  
  real :: Den_IC(nspec,nLat,nLon) = 0.0, Temp_IC(nspec,nLat,nLon) = 0.0, &
       Temppar_IC(nspec,nLat,nLon) = 0.0
  integer :: iLatMin=22 !Minimum latitude in MHD boundary
  
  logical :: UseGm                  = .false.
  logical :: DoneGmCoupling         = .false.

!  public :: init_gm_cimi
!contains
!  subroutine init_gm_cimi
!    integer :: iSpecies
!    !----------------------------------------------------------------------
!    do iSpecies =1,nspec-1
!       AveDen_I(iSpecies) = 6+iSpecies
!       AveP_I(iSpecies)=6+nspec-1+iSpecies
!    enddo
!  end subroutine init_gm_cimi
  
end Module ModGmCimi
