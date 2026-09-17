!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModSize

  implicit none

  private ! except
  public :: nDim, nMomentum, nVertexMax, nPitchAngle, IsPitchAngleAverage

  ! Dimensionality
  integer, parameter:: nDim = 3

  ! Max possible index of a particle on a line set by Config.pl
  integer, parameter:: nVertexMax  = 1000

  ! number of points along the phase coords (see ModAdvance);
  integer, parameter:: nMomentum   = 100
  integer, parameter:: nPitchAngle = 1
  logical, parameter:: IsPitchAngleAverage = 1 == nPitchAngle

end module SP_ModSize
!==============================================================================
