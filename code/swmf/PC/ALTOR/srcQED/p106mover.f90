module mover
  use constants
  use fields, only : C,E,B !,rCommArray,rCommArrayGlob
  use particles
  use ionization_check_
  use variables_box
  use variables_data
  use variables_out, only : logPrint,rad,nAngle,nAngle2,radAngle
  use variables_out, only : nFreq,nChi,ChiMax,rFreqMax,radChi &
       ,radFreqChi0,radFreqChi1,radFreqChiCos0,radFreqChiCos1
  use MPImodule !, only : MPIallreduce_sum_r
  use form_factor, only : form_factor_,iShift1,iShift2
  use functions_
  implicit none
  real,dimension(nDim,iShift1-1:iShift2+1),save :: w,w2 !form-factor: nDim*1D
  real,dimension(1:3,0:nMax(1)+2)::rCurrentAux
contains
  !
  !===========================================================================!
  subroutine mover__(n1,n2,dtFactorIn,chargeIn, mom,enerKin,nPartWork)
    !=========================================================================!
    integer,intent(in) :: n1,n2
    integer,intent(out) :: nPartWork ! N of particles in the working region
    real,intent(in) :: dtFactorIn,chargeIn
    real,intent(out) :: mom(3),enerKin ! total momentum and kinetic energy
    !
    integer :: i,iDel,n,k,j,iChi
    integer,dimension(nDim) :: ii,ii2 ! particle location on grid
    real,dimension(3) :: eForce,bForce,p,p2, u,du,dp,d !,p3
    real :: displacement,tmp,tmp2,Chi
    real :: gamma2, udp, radDt, q, omegaC, Angle1, Angle2
    logical :: logBirth
    !=========================================================================!
    nameLocal='MOVER:'
    !
    mom=ZERO;  enerKin=ZERO; nPartWork=0 
    !
    do n=n1,n2
       !======================================================================!
       ! Checking boundaries
       !======================================================================!
       if(R(1,n) > xStart) cycle 
       if(any(R(1:nDim,n) < xMin) .or. any(R(1:nDim,n) > xMax)) cycle
       !======================================================================!
       ! Particle form-factors at initial position for Force, Current
       !======================================================================!
       w=ZERO; w2=ZERO 
       !
       do k=1,nDim
          !
          ! particle displacement with respect to the
          ! center of the cell to which it belongs
          !
          !tmp=R(k,n)*dxInv(k)+HALF !new !did not work with moving frame
          tmp=(R(k,n)-xMin(k))*dxInv(k)+HALF !old ?
          i=nint(tmp)
          displacement=tmp-real(i) 
          ii(k)=i 
          !
          ! 1D formfactor on +1/2 grid
          !
          w(k,iShift1:iShift2)=form_factor_(displacement)
          !
          ! particle displacement with respect to the
          ! nearest face, normal to the 'k'th direction 
          !
          tmp=tmp+HALF 
          i=nint(tmp)
          displacement=tmp-real(i)
          ii2(k)=i 
          !
          ! 1D formfactor on +0 grid
          !
          w2(k,iShift1:iShift2)=form_factor_(displacement)
          !
       end do
       !======================================================================!
       ! E-,B- forces
       !======================================================================!
       call force__(eForce(1:3),bForce(1:3),&
            E( 1:3,&
            ii(1)+iShift1-1:ii(1)+iShift2+1 ),&
            B( 1:3,&
            ii(1)+iShift1-1:ii(1)+iShift2+1 ),&
            ii2(1:nDim)-ii(1:nDim) )
       !
       !======================================================================!
       ! Ionization 
       !======================================================================!
       if( nCharge(n) == 0 ) then 
          logBirth=ioniz_check_(sqrt(sum(eForce**2)),dt)
          if( logBirth ) then 
             nCharge(n)=1
          else
             cycle
          end if
       end if
       !
       !======================================================================!
       ! Forces with dtChargeMassInvHalf factor for particle motion
       !======================================================================!
       eForce=eForce*dtFactorIn
       bForce=bForce*dtFactorIn
       !======================================================================!
       ! Particle motion
       !======================================================================!
       !
       p=R(nDim+1:nDim+3,n)+eForce   ! acceleration by eForce within 1st dt/2
       !
       tmp=dot_product(p,p)          ! ( p^{n} )^2
       tmp2=sqrt(ONE+tmp)            ! energy : gamma=sqrt(1+p^2) ->gamma^{n} 
       enerKin=enerKin+tmp/(ONE+tmp2)! kinetic energy: K=p^2/(sqrt(1+p^2)+1)
       !                             ! K^{n}
       d=bForce/tmp2                 ! so that [p x d]= [v x bForce]
       !
       p2 = p + cross_product(p, d)  ! Boris' 1st rotation  
       !
       d=d*TWO/sqrt(ONE+dot_product(d,d)) ! d=2*bForce/sqrt(bForce^2+gamma^2)
       !
       p = p + cross_product(p2, d)& ! Boris' 2nd rotation
            + eForce                 ! acceleration by eForce within 2nd dt/2
       !                             ! p^{n+1/2}
       !=============added to simple exclusion the radiation reaction
       du(1:3)=ZERO
       !
       gamma2=ONE+dot_product(p,p)     ! (gamma^(n+1/2))**2
       !
       u= p/sqrt(gamma2)               ! u^{n+1/2}
       !
       !=================================================================
       include 'p006.fh' !radiation reaction
       !=================================================================
       !
       R(nDim+1:nDim+3,n)=p          ! update momentum
       ! 
       mom=mom+p                     ! total momentum ...
       nPartWork=nPartWork+1         ! ... in nPartWork particles
       ! 
       tmp=dt/sqrt(ONE+dot_product(p,p)) ! dt/gamma factor
       !================= two versions =============!update coordinates
       !RD(1,n)=R(1,n) !:nDim,n) !TEST
       !
       !R(1:nDim,n)=R(1:nDim,n)+p(1:nDim)*tmp ! update coordinates 
       !old=========
       !R(1:nDim,n)=R(1:nDim,n)+p(1:nDim)*dt !?gluck? update coordinates
       !new=========
       !R(1:nDim,n)=R(1:nDim,n)+(u(1:nDim)+du(1:nDim))*dt ! update coordinates
       !=============================================VERSION=2010-12-31
       R(1:nDim,n)=R(1:nDim,n)+p(1:nDim)*tmp+du(1:nDim)*dt
       !=============================================VERSION=2010-12-31
       !
       !RD(2,n)=R(1,n) !:nDim,n) !TEST
       !RD(3,n)=dt !TEST
       !RD(4,n)=ONE/sqrt(ONE+dot_product(p,p))
       !
       !===============================================================
       !
       p(nDim+1:3)=p(nDim+1:3)*tmp ! (nDim+1:3) components for current 
       !
       !====================================================================!
       ! Undate particle form-factor
       !====================================================================!
       w2=ZERO
       !
       do k=1,nDim
          !
          ! updated particle displacement with respect to the
          ! center of the cell to which it belongs 
          !
          !tmp=R(k,n)*dxInv(k)+HALF !new !did not work in moving frame
          tmp=(R(k,n)-xMin(k))*dxInv(k)+HALF !old? !for plasma!not TestParticle
          i=nint(tmp)
          displacement=tmp-real(i)
          !
          ! particle shift
          ! 
          iDel=i-ii(k) 
          ii(k)=ii(k)+min(iDel,0)
          i=max(iDel,0)
          !
          ! updated 1D formfactor on +1/2 grid
          !
          w2(k,i+iShift1:i+iShift2)=form_factor_(displacement)
          !
          if(iDel < 0) w(k,iShift1:iShift2+1)=w(k,iShift1-1:iShift2)
          !
       end do
       !====================================================================!
       ! Current 
       !====================================================================!
       call current__(C(1:3, &
            ii(1)+iShift1: ii(1)+iShift2+1 ),&
            chargeIn, p(nDim+1:3) )
       !====================================================================!
    end do
    !
  end subroutine mover__
  !
  !=======================================================================!
  subroutine force__(eForce,bForce,EE,BB,i)
    !=====================================================================!
    real :: eForce(3),bForce(3)
    real, intent(in), & ! input field component
         dimension(1:3,&
         iShift1-1:iShift2+1) :: EE,BB 
    integer, intent(in) :: i(nDim) ! shift between +1/2 grid and +1 grid
    integer :: i1
    character (len=16) :: nameLocal !location for error report
    !
    nameLocal='FORCE:'
    eForce=ZERO; bForce=ZERO
    !
    do i1=iShift1,iShift2
       !
       eForce(1)=eForce(1) +EE(1, i1+i(1) )&
            *w2(1,i1)
       !
       eForce(2)=eForce(2) +EE(2, i1      )&
            *w (1,i1)
       !
       eForce(3)=eForce(3) +EE(3, i1      )&
            *w (1,i1)
       !
       bForce(1)=bForce(1) +BB(1, i1      )&
            *w (1,i1)
       !
       bForce(2)=bForce(2) +BB(2, i1+i(1) )&
            *w2(1,i1)
       !
       bForce(3)=bForce(3) +BB(3, i1+i(1) )&
            *w2(1,i1)
       !
    end do
    !
  end subroutine force__
  !
  !=======================================================================!
  subroutine current__(CC,chargeIn,pIn)
    !=====================================================================!
    real, intent(in) :: chargeIn,pIn(nDim+1:3)
    real :: &
         CC(1:3, iShift1:iShift2+1)
    integer :: i1
    real, dimension(iShift1:iShift2) :: pp,ppp
    real :: p(nDim)
    !
    pp=-(w2(1,iShift1:iShift2)-w(1,iShift1:iShift2))
    !
    forall(i1=iShift1:iShift2) &
         ppp(i1)=sum(pp(iShift1:i1))*chargeIn
    !            
    CC(1,iShift1+1:iShift2+1)=CC(1,iShift1+1:iShift2+1)&
         +ppp(iShift1:iShift2)
    !
    do i1=iShift1,iShift2+1
       !
       p(1)=HALF*( w(1,i1)+w2(1,i1) )*chargeIn
       !
       CC(2:3,i1)=CC(2:3,i1)+p(1)*pIn(2:3)
    end do
    !
  end subroutine current__
  !
  !=======================================================================!
  subroutine reduce_current__
    !=====================================================================!
    !
    include "p106.fh"
    !
  end subroutine reduce_current__
  !
end module mover
