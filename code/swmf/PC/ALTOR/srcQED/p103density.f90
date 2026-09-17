module density
  use constants
  use fields, only : Z,rCommArray,rCommArrayGlob
  use particles
  use variables_box
  use variables_data
  use variables_out, only : logPrint
  use form_factor, only : form_factor_,iShift1,iShift2
  use MPImodule !, only : MPIreduce_sum_r
  implicit none
  real,dimension(1:nMax(1))::rDensityAux
contains
  subroutine density__(n1,n2)
    integer, intent(in) :: n1,n2
    integer :: i,i1,n,k,ii(nDim)
    real :: w(nDim,-1:1),ww(-1:1) !weighting arrays 1*1D and 1D
    real :: displacement,tmp
    !
    nameLocal='DENSITY:'
    !
    do n=n1,n2
       if(logIoniz .and. (nCharge(n) == 0) ) cycle
       if(any(R(1:nDim,n) < xMin) .or. any(R(1:nDim,n) > xMax)) cycle
       !
       do k=1,nDim
          tmp=R(k,n)*dxInv(k)-HALF !new
          !tmp=(R(k,n)-xMin(k))*dxInv(k)-HALF 
          i=nint(tmp)
          displacement=tmp-real(i) 
          ii(k)=i+1 
          !
          ! 1D formfactor on +1/2 grid 
          !
          w(k,iShift1:iShift2)=form_factor_(displacement)
          !
       end do
       !
       forall (i1=-1:1) &
            ww(i1)=w(1,i1)
       !
       !debuging output
       !if(logPrint) write(*,*) maxval(w),sum(w)
       !if(logPrint) write(*,*) w
       !
       Z(ii(1)-1:ii(1)+1)=&
            Z(ii(1)-1:ii(1)+1)+ww
    end do
    !
    ! writing to 1D array
    rCommArray(1:nMaxProd)=ZERO
    forall(i1=1:nMax(1)) &
         rCommArray(i1)=Z(i1)
    rCommArrayGlob(1:nMaxProd)=ZERO
    !
    ! summation to PE00
    !call MPIreduce_sum_r(rCommArray,rCommArrayGlob,nMaxProd) 
    call MPIreduce_sum_r(Z,rCommArrayGlob,nMaxProd) 
    !
    !-control output
    if(logPrint) then
       write(*,*) nameLocal,'MPIreduce:1 ', maxval(Z),sum(Z)
       write(*,*) nameLocal,'MPIreduce:2 ', &
            maxval(rCommArray(1:nMaxProd)), &
            sum(rCommArray(1:nMaxProd))
       write(*,*) nameLocal,'MPIreduce:3 ', & 
            maxval(rCommArrayGlob(1:nMaxProd)), &
            sum(rCommArrayGlob(1:nMaxProd))
    endif
  end subroutine density__
  !
  !=======================================================================!
  subroutine reduce_density__
    !=====================================================================!
    !
    include "p103.fh"
    !
  end subroutine reduce_density__
  !

end module density
