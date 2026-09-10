 module output_field
  use constants
  use task_dimension
  use variables_box 
  use variables_data
  use variables_particles, only : nPartCell
  use fields, only : E,B,Z,C,rCommArray
  use variables_out, only : logPrint,format_output
  use MPImodule, only : nameProcOut
  implicit none
contains
  !
  subroutine output_density_(nameSort,chargeOut) !=============density
    !
    real, intent(in) :: chargeOut
    character (len=1), intent(in) :: nameSort
    character (len=20) :: name
    integer :: i1
    !
    rCommArray(1:nMaxProd)=ZERO
    forall(i1=1:nMax(1)) &
         rCommArray(i1)=Z(i1)
    !
    name='z'//nameSort//nameTimeStep//'.'//nameProcOut
    !
    call output_f(14,name,chargeOut)
    !
  end subroutine output_density_
  !
  subroutine output_current__ !!===============================current
    !
    character (len=1) :: nameDim(3)=(/'x','y','z'/)
    character (len=20) :: name
    integer :: k,i1
    !
    !---------C arrays
    do k=1,3
       ! writing to 1D array
       rCommArray(1:nMaxProd)=ZERO
       forall(i1=1:nMax(1)) &
            rCommArray(i1)=C(k,i1)
       !
       name='c'//nameDim(k)//nameTimeStep//'.'//nameProcOut
       !
       call output_f(k+14,name,ZERO)
    end do
    !
  end subroutine output_current__
  !
  subroutine output_field__ !===================================field
    !
    character (len=1) :: nameDim(3)=(/'x','y','z'/)
    character (len=20) :: name
    integer :: k,i1
    !
    !---------E arrays
    do k=1,3
       ! writing to 1D array
       rCommArray(1:nMaxProd)=ZERO
       forall(i1=1:nMax(1)) &
            rCommArray(i1)=E(k,i1)
       !
       name='e'//nameDim(k)//nameTimeStep//'.'//nameProcOut
       !
       call output_f(k+6,name,ZERO)
       !
    end do
    !
    !---------B arrays
    do k=1,3
       ! writing to 1D array
       rCommArray(1:nMaxProd)=ZERO
       forall(i1=1:nMax(1)) &
            rCommArray(i1)=B(k,i1)
       !
       name='b'//nameDim(k)//nameTimeStep//'.'//nameProcOut
       !
       call output_f(k+9,name,ZERO)
       !
    end do
    !
    !---------W array
    call energy_density
    rCommArray(1:nMaxProd)=ZERO
    forall(i1=1:nMax(1)) &
         rCommArray(i1)=Z(i1)
    !
    name='w'//nameTimeStep//'.'//nameProcOut
    !
    call output_f(13,name,ZERO)
    !
  end subroutine output_field__
  !
  subroutine energy_density !====================================energy
    integer :: i1
    Z=ZERO
    forall(i1=1:nMax(1)) Z(i1)=Z(i1) &
         +sum(E(:,i1)**2)+sum(B(:,i1)**2)
  end subroutine energy_density
  !
  subroutine output_f(num,name,chargeOut) !====================output
    integer, intent(in) :: num
    real, intent(in) :: chargeOut
    character (len=20) :: name
    real :: x1,x2
    !
    x1=minval(rCommArray(1:nMaxProd))
    x2=maxval(rCommArray(1:nMaxProd))
    if( (x2-x1) <1.e-5 ) then 
       if(logPrint) &
            write(*,*)'OUTPUT: ',name,' field is not written: ',x1,x2,(x2-x1)
       return
    end if
    !
    if (format_output == 'f') then
       write(*,*)'OUTPUT: ','f_'//name,' ',iTime,x1,x2
       open(unit=num,file='f_'//name,form='formatted')
       write(num,*) iTime,nDim,nMax,dx, nPartCell,chargeOut
       write(num,*) rCommArray(1:nMaxProd)
       close(num)
    else if (format_output == 'u') then
       write(*,*)'OUTPUT: ','u_'//name,' ',iTime,x1,x2
       open(unit=num,file='u_'//name,form='unformatted')
       write(num) iTime,nDim,nMax,dx, nPartCell,chargeOut &
            ,rCommArray(1:nMaxProd)
       close(num)
    end if
    !
  end subroutine output_f
  !
end module output_field
