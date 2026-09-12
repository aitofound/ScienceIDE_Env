module functions_
  implicit none
contains
  !=======================================================================!
  function cross_product(a,b)
    !=====================================================================!
    real,dimension(3)::cross_product
    real,dimension(3),intent(in)::a,b
    !
    cross_product(1)=a(2)*b(3)-a(3)*b(2)
    cross_product(2)=a(3)*b(1)-a(1)*b(3)
    cross_product(3)=a(1)*b(2)-a(2)*b(1)
    !
  end function cross_product
  !
  !=======================================================================!
  function dot_product(a,b)
    !=====================================================================!
    real::dot_product
    real,dimension(3),intent(in)::a,b
    !
    dot_product=a(1)*b(1)+a(2)*b(2)+a(3)*b(3)
    !
  end function dot_product
  !
end module functions_
