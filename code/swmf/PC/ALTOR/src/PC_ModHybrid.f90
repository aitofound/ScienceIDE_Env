!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!==========================
module PC_ModHybrid
  use PC_ModSize, ONLY: nHybrid
  implicit none
  logical, parameter:: UseHybrid = nHybrid==1
contains
  subroutine add_predictor_current(QPerVDt_D, W_D, iBlock)
    use PC_ModSize, ONLY: MaxDim, nDim
    use PIC_ModFormfactor
    integer, intent(in):: iBlock
    real,    intent(in):: QPerVDt_D(nDim), W_D(MaxDim)
  end subroutine add_predictor_current
end module PC_ModHybrid
