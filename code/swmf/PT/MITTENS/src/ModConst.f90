  !  Copyright (C) 2002 Regents of the University of Michigan,
  !  portions used with permission
  !  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModConst
  use ModNumConst, only: cPi, cTwoPi, cRadToDeg, cDegToRad   
  use ModConst,    only: cAU, ceV, ckeV, cMeV, cMu, &
                         cLightSpeed, cProtonMass, &
                         cElectronCharge, cRsun => rSun, cTiny, &
                         cBoltzmann, cKEVToK

  implicit none
  SAVE
  ! Constants in SI
  real, parameter :: cProtonRestEnergy = cProtonMass * cLightSpeed**2

end module PT_ModConst
!==============================================================================
