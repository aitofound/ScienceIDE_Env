 module plasma_profiles
  use constants, only : ONE,TWO
  implicit none
contains
  function coordinate(x,len,y0,y1,jProf)
    integer, intent(in) :: jProf
    real :: coordinate
    real, intent(in) :: x,len,y0,y1
    !
    select case(jProf)
       case(0) !uniform
          coordinate=x/y0
       case(1) !linear
          coordinate=len/(y1-y0)*y0 &
               *(-ONE+sqrt(ONE+(y1-y0)/y0**2*TWO*x/len))
       case(2) !exponential
          coordinate=len/log(y1/y0) &
               *log( ONE + x/len/y0*log(y1/y0) )
    end select
  end function coordinate
end module plasma_profiles
