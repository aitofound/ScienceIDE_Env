!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!--------------------------------------------------------------!
module PC_ModSize
  use PC_BATL_size, ONLY: nX=>nI, nY=>nJ, nZ=>nK, jDim_, kDim_
  use PC_BATL_size, ONLY: nPType=>nKindParticle, nDim, MaxDim
  use PC_BATL_size, ONLY: nBlock, MaxBlock
  integer, parameter :: x_=1, y_=2, z_=3
  integer, parameter :: nElectronMax=10000000
  integer, parameter :: nCell_D(MaxDim) = (/nX, nY, nZ/)

  !\
  ! Paramaters of hybrid simulations
  !/
  integer, parameter :: nHybrid = 0
end module PC_ModSize
