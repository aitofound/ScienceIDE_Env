module i_chi_
  use constants, only : ZERO,ONE,TWO,THREE,FOUR,FIVE,HALF
  implicit none
contains
  function i_chi(Chi)
    real :: i_chi
    real, intent(in)::Chi
    integer :: i,j
    integer, parameter :: n1X=2000
    integer, parameter :: nT=400
    real :: X10,X11,d1X,T0,T1,dT,c23,r_1,cSum,Argmt
    real, dimension(0:n1X) :: x1,Simp1X,f1
    real, dimension(0:nT)::t,simp,cosh,cosh23
    !
    !Define the kernel of the integral transformation for a given n10
    !print,'statrs spectrum transformation',n10,nInput
    !      
    X11=ONE*25
    X10=ZERO
    !
    !Increase the resolution in the essential interval of 1+\chi*x, 
    !at large \chi 
    if (Chi > 40.0) X11=1000.0/Chi
    !
    !Array passing from X0 to X1:
    !
    d1X=(X11-X10)/n1X
    forall(i=0:n1X) x1(i)=d1X*i+X10
    !
    !Coefficients of the Simpson method: (1,4,2,4,2,4,2...4,2,4,1)*dX/3.0
    !
    Simp1X(0)=TWO
    do i=1,n1X-1 
       Simp1X(i)=ONE*6-Simp1X(i-1)
    end do
    Simp1X(0)=ONE
    Simp1X(n1X)=ONE
    Simp1X=Simp1X*d1X/THREE
    !
    !Initialize function values
    f1=ZERO
    !Initialize array for an integral by t
    T0=ZERO
    T1=ONE*20
    !
    dT=(T1-T0)/nT
    forall(i=0:nT) t(i)=dT*i+T0
    !
    !Coefficients of the Simpson method: (1,4,2,4,2,4,2...4,2,4,1)*dT/3.0
    simp(0)=TWO
    do i=1,nT-1 
       simp(i)=ONE*6-simp(i-1)
    end do
    simp(0)=ONE
    simp(nT)=ONE
    simp=simp*dT/THREE
    !
    cosh=HALF*(exp(t)+exp(-t))
    !
    !Prepare to integrate \int_0^\infty{exp(-x*cosh(t))*cosh(2/3t)dt}
    c23=TWO/THREE
    cosh23=HALF*(exp(c23*t)+exp(-c23*t))
    !
    !Integrate
    do i=0,n1X
       r_1=x1(i)
       !
       !Calculate the function
       !((4/3)+(5/3)chi*r_1+4/3*(chi*r_1)^2)/(1+chi*r_1)^4 
       !K_{2/3)(r_1)r_1
       !To avoid calculation of too small exponential function of large
       !negative arguments limit the value of r_1=r_0/(1-\chi r_0) by 25
       !
       do j=0,nT 
          Argmt=-r_1*cosh(j)+alog(cosh23(j))
          !
          !Truncate an input if less than exp(-30)
          if (Argmt > -30.0) f1(i)=f1(i)+simp(j)*exp(Argmt)
       end do
       !
       !Multiply by 9*sqrt(3)/(8*!pi)=0.620245 and by the above factor
       !
       f1(i)=f1(i)*0.620245*r_1*(FOUR+FIVE*Chi*r_1+FOUR*(Chi*r_1)**2)&
            /(THREE*(ONE+Chi*r_1)**4)
    end do
    !
    !Integrate \int(fdr_1} (should be 1 for Chi=0)
    cSum=ZERO
    do i=0,n1X 
       cSum=cSum+f1(i)*Simp1X(i)
    end do
    !write(*,*)' Chi,cSum=',Chi,cSum
    i_chi=cSum
    !return,cSum
  end function i_chi
end module i_chi_

