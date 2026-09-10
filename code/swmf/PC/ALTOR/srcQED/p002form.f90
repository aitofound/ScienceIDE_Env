module form_factor
  use constants
  implicit none
  integer, parameter :: iShift1=-1,iShift2=1
contains
  function form_factor_(displacement)
    real :: form_factor_(-1:1)
    real, intent(in) :: displacement
    real :: d1,d2,d3
    !
    d1=displacement
    d2=d1*d1
    d3=HALF*d2
    d1=HALF*d1
    !
    form_factor_(-1)=EIGHTH-d1+d3
    form_factor_( 0)=THREE_FOURTH-d2
    form_factor_( 1)=EIGHTH+d1+d3   
    !
  end function form_factor_
  !
end module form_factor
