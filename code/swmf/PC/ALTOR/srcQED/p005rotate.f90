module rotate_
  use constants, only : PI
  implicit none
contains
  !=========================================================================!
  function rotate(x,y, n, theta, x0,y0)
    integer, intent(in) :: n
    real,dimension(2) :: rotate(2,n)
    real, intent(in) :: x(n),y(n)
    real, intent(in) :: theta, x0, y0
    real ::  sin_th, cos_th !rotation of the target
    !??
    sin_th=sin(theta*PI/180.); cos_th=cos(theta*PI/180.)
    !
    rotate(1,:)=x0+(x-x0)*cos_th+(y-y0)*sin_th
    rotate(2,:)=y0+(y-y0)*cos_th-(x-x0)*sin_th
    !
  end function rotate
  !
  !=========================================================================!
  function curve(x, y, n, radius, x0,y0)
    integer, intent(in) :: n
    real,dimension(2) :: curve(n)
    real, intent(in) :: x(n), y(n)
    real, intent(in) :: radius, x0, y0
    integer :: i
    real :: tmp
    !
    do i=1,n        
       tmp=abs(y(i)-y0)
       if ( tmp < radius ) then 
          tmp=radius-sqrt(radius**2-tmp**2)
       else
          tmp=radius
       end if
       curve(i)=x(i)+tmp
    end do
    !
  end function curve
  !
end module rotate_
