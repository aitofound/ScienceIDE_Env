Module ModCimiInitialize
  use ModCimiGrid,	ONLY: nm, nk, neng, npit1, np1
  use ModCimiPlanet,	ONLY: nspec
  implicit none
  
  real		:: &
       xmm(nspec,0:nm+1), xk(0:nk+1), dphi, dmm(nspec,nm), dk(nk),&
       dmu(npit1), xjac(nspec,np1,nm)

  logical	:: &
       IsEmptyInitial = .false., IsDataInitial = .false., &
       IsRBSPData = .false., IsGmInitial = .true., &
       IsInitialElectrons = .false., DoLstarInitialization = .false.

  character(11) :: NameFinFile = 'quiet_x.fin'
  character(5)	:: FinFilePrefix = 'xxxxx'

end Module ModCimiInitialize
