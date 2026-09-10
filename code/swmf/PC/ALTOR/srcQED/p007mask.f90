module mask_
  use constants, only : ZERO,ONE
  use variables_box, only : nMask
  use fields, only : rMask
  implicit none
contains
  !
  subroutine mask
    integer :: i,iMax
    iMax=nMask(1)
    forall (i=0:iMax -1) &
         rMask(i) = ONE - ( (iMax+ZERO-i)/(iMax+ZERO) )**2
  end subroutine mask
  !
end module mask_
