module fields_advance
  use task_dimension
  use variables_box 
  use variables_data
  use variables_out, only : enerFieldE,enerFieldB
  use fields, only : E,B,C, rMask
  use moving_box
  use constants, only : ZERO,ONE
  implicit none
contains !??? which field is absent in 1D: Bx?
  !
  subroutine field_B__ !-----------------------------------------------------
    integer :: i1
    !
    forall(i1=1:nMax(1)) B(1,i1)=B(1,i1)
    !
    forall(i1=1:nMax(1)) B(2,i1)=B(2,i1)&
         +dtdxInvHalf(1)*(E(3,i1)-E(3,i1-1))
    !
    forall(i1=1:nMax(1)) B(3,i1)=B(3,i1)&
         -dtdxInvHalf(1)*(E(2,i1)-E(2,i1-1)) 
    !
  end subroutine field_B__
  !
  subroutine field_E__ !-----------------------------------------------------
    integer :: i1,k
    !
    !write(*,*) 'field_E__: Cx,Cy=',maxval(abs(C(1,:))), maxval(abs(C(2,:)))
    forall(i1=1:nMax(1)) E(1,i1)=E(1,i1)&
         -C(1,i1)*dxWeight(1)
    !
    forall(i1=1:nMax(1)) E(2,i1)=E(2,i1)&
         -dtdxInv(1)*(B(3,i1+1)-B(3,i1)) &
         -C(2,i1)*dxWeight(2)
    !
    forall(i1=1:nMax(1)) E(3,i1)=E(3,i1)&
         +dtdxInv(1)*(B(2,i1+1)-B(2,i1)) &
         -C(3,i1)*dxWeight(3) !???? which weight ?
    !
    if (nMask(1) > 0) then 
       forall(k=2:3,i1=0:nMask(1)-1) E(k,i1)=E(k,i1)*rMask(i1)
       forall(k=2:3,i1=0:nMask(1)-1) E(k,nMax(1)-i1)=E(k,nMax(1)-i1)*rMask(i1)
    end if
    !
  end subroutine field_E__
  !
  subroutine field_bc_B__
    !
    ! periodic boundary in y,z 
    !
    ! absorption in x
    !
    B(:,nMax(1)+1)=dtdxInv(1)*B(:,nMax(1))+(ONE-dtdxInv(1))*B(:,nMax(1)+1)
    !
  end subroutine field_bc_B__ !---------------------------------------------
  !
  subroutine field_bc_E__ 
    !
    ! periodic boundary in y,z 
    !
    ! absorption in x
    !
    E(:,0)=dtdxInv(1)*E(:,1)+(ONE-dtdxInv(1))*E(:,0)
    !
  end subroutine field_bc_E__ !--------------------------------------------- 
    !
  subroutine field_energy__ !-----------------------------------------------
    integer :: k
    !
    forall(k=1:3) enerFieldE(k)=sum( E( k, 1:nMax(1) )**2 ) 
    forall(k=1:3) enerFieldB(k)=sum( B( k, 1:nMax(1) )**2 ) 
    !
  end subroutine field_energy__ !-------------------------------------------
    !
  subroutine move_fields__
    ! simple shift?
    !dimension(1:3, 0:nx+2) :: E,B
    !
    if ( nxMove < 0 ) then 
       E(:, -nxMove:nx+2) = E(:, 0:nx+2+nxMove) 
       B(:, -nxMove:nx+2) = B(:, 0:nx+2+nxMove)
       E(:, 0:-nxMove-1) = ZERO
       B(:, 0:-nxMove-1) = ZERO
    endif
    !  
    if ( nxMove > 0 ) then 
       E(:, 0:nx+2-nxMove) = E(:, nxMove:nx+2) 
       B(:, 0:nx+2-nxMove) = B(:, nxMove:nx+2)
       E(:, nx+3-nxMove:nx+2) = ZERO
       B(:, nx+3-nxMove:nx+2) = ZERO
    endif
    !
  end subroutine move_fields__
  !
  !
end module fields_advance
