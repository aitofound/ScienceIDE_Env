module particles_move
  use task_dimension
  use variables_data, only : xMin,xMax,dx
  use particles
  implicit none
contains
  subroutine move_particles_(n1,n2,ns)
    integer :: n1,n2,ns
    integer :: n,nn
    !
    n = n1 !all particles count
    nn = n1-1 !factical particle count
    !
    ! -------------------------- compressing towards array begin
    do 
       if ( n > n2 ) exit
       if ( (R(1,n) <= xMin(1)+dx(1)*2) .or. (R(1,n) >= xMax(1)) ) then    
          n = n + 1
          cycle
       else
          nn = nn + 1
          if (nn /= n) R(:,nn) = R(:,n)
          n = n + 1
       end if
    end do
    n2 = nn
    ! -------------------------- for ions -> to the array end
    if(ns < 0) then 
       nn=nPartMax-n2+n1
       R(:,nn:nPartMax)=R(:,n1:n2)
       n1=nn
       n2=nPartMax
    end if
  end subroutine move_particles_
  !
end module particles_move
