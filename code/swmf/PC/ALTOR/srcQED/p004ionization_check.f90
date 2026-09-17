module ionization_check_
  implicit none
contains
  !
  function ioniz_check_0(eAbsolute,dt)
    logical :: ioniz_check_0
    real, intent(in) :: eAbsolute,dt
    real :: eLimit
    eLimit=0.07
    ioniz_check_0=.false.
    if(eAbsolute >= eLimit) ioniz_check_0=.true.
  end function ioniz_check_0
  !
  function ioniz_check_(eAbsolute,dt)
    logical :: ioniz_check_
    real, intent(in) :: eAbsolute,dt
    real :: eLimit
    real :: aa1,a1,a2,a22,rrrr,rrrrr
    !
    !eLimit=0.5 !simple half
    call random_number(eLimit) !random
    ioniz_check_=.false.
    !==========================
    aa1=24.4/13.6
    a1=4.*4.16*0.79*100.*dt/3.
    a2=5.14*0.79/(3.2*10.)
    a22=a2*2./3.
    !
    if (eAbsolute .lt. 1.e-5) return
    rrrr=a1*aa1**(5./2.)*a2/eAbsolute*exp(-a22*aa1**(3./2.)/eAbsolute)
    rrrrr=1.-exp(-rrrr)
    !
    !==========================
    if(rrrrr >= eLimit) ioniz_check_=.true.
  end function ioniz_check_
  !
end module ionization_check_
