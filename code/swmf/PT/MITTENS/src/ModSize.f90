  !  Copyright (C) 2002 Regents of the University of Michigan,
  !  portions used with permission
  !  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModSize

  implicit none

  private ! except
  public :: nDim, nVertexMax

  ! Dimensionality
  integer, parameter:: nDim = 2

  ! Max possible index of a particle on a line set by Config.pl
  integer, parameter:: nVertexMax  = 6000

end module PT_ModSize
!==============================================================================
