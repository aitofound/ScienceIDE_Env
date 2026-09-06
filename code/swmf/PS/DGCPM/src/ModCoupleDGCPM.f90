!  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModCoupleDGCPM

! Couple related variables only.

use ModKind, only: Real8_
use ModSizeDGCPM, only: nthetacells, nphicells

logical :: isCoupled = .false.

! Variables for PS-IE coupling:
integer :: nThetaIe=0, nPhiIe=0
real (Real8_), allocatable :: IePot_II(:,:), IeTheta_I(:), IePhi_I(:)

! Variables for PS-GM coupling:
integer :: iGmFluidCouple=0  ! Index of fluid to couple to (0=>total fluid)
real    :: tempPlas = 2      ! Temperature of plasmasphere in eV

end Module ModCoupleDGCPM

