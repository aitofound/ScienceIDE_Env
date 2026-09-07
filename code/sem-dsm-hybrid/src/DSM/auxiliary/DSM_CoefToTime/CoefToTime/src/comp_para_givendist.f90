subroutine comp_para_givendist(source_type,distance,l)

use spectotime_par,only: m0,factor,x,y,lsq,plm,plm1,plm2,dpdtheta,d2pd2theta
use spectotime_par,only: dplm1dtheta,dphi,Ylm,dYdphi,dYdtheta,d2Yd2theta,expimp
use constants, only :PI
implicit none

integer ::source_type,l
double precision ::distance
double precision,parameter ::phi=0.0
double precision ::plgndr


!local parameter
  integer :: m,m_abs
  integer :: i

  if(source_type.eq.1.and.m0.ne.2) then
     stop 'Error, source_type=1,so max_m and min_m should be 2 and -2'
  else if(source_type.eq.2.and.m0.ne.1) then
     stop 'Error, source_type=2,so max_m and min_m should be 1 and -1'
  end if

  do m=max0(-l,-m0),min0(l,m0)
   m_abs=iabs(m)
   if ( (l.lt.0).or.(m_abs.gt.l) ) stop 'Error of order l'
            factor(m) = 1.d0
         do i=l-m_abs+1,l+m_abs
           factor(m) = factor(m) * dble(i)
         end do
         factor(m) = dsqrt( dble(2*l+1)/(4.d0*PI) / factor(m) )
         if ( ( m_abs.ne.m ).and.( mod(m_abs,2).eq.1) ) factor(m) = -factor(m)
         lsq=dsqrt(dble(l)*dble(l+1))
! **********************************************************************
! computing the displacement and its derivation
! **********************************************************************
         x=dcos(distance)
         y=dsin(distance)
         expimp(m)=cdexp( dcmplx( 0.d0, dble(m)*phi ) )
         plm(m)=plgndr(l,m_abs,x)
         if(l.eq.0) then
             dpdtheta(m)= dcmplx(0.d0,0.d0)
             d2pd2theta(m)= dcmplx(0.d0,0.d0)
         else if(l.eq.1) then
            if(m_abs.eq.1) then
              dpdtheta(m)=-x
              d2pd2theta(m)=y
            else if(m_abs.eq.0) then
              dpdtheta(m)=-y
              d2pd2theta(m)=-x
            end if
         else if(l.eq.2.and.m_abs.eq.2) then
            dpdtheta(m)=6.d0*x*y
            d2pd2theta(m)=6.d0*(x*x-y*y)
         else
            plm1(m)=plgndr(l,m_abs+1,x)
            dpdtheta(m)=(dble(m_abs)*x/y*plm(m)+plm1(m))
            if(m_abs+2.le.l) then
              plm2(m)=plgndr(l,m_abs+2,x)
              dplm1dtheta(m)=(dble(m_abs+1)*x/y*plm1(m)+plm2(m))
            else
              dplm1dtheta(m)=-dble(l+m_abs+1)*dble(l-m_abs)*plm(m) &
                          -dble(m_abs+1)*x/y*plm1(m)
            end if
            d2pd2theta(m)=dble(m_abs)*x/y*(-plm(m)/(y*x) &
                         + dpdtheta(m))+dplm1dtheta(m)
         end if
         dphi(m)=dcmplx(0.d0,dble(m))
         Ylm(m)=factor(m)*expimp(m)*plm(m)
         dYdtheta(m)=factor(m)*expimp(m)*dpdtheta(m)
         d2Yd2theta(m)=factor(m)*expimp(m)*d2pd2theta(m)
         dYdphi(m)=Ylm(m)*dphi(m)
   end do !end m


end subroutine
