module fields_initiate
  use constants
  use task_dimension
  use variables_box 
  use variables_data
  use fields, only : E,B !, rMask
  use MPImodule, only : mpiProc,mpiHost
  implicit none
contains
  !
  subroutine field_init_constant__
    integer :: i
    E=ZERO
    B=ZERO
    forall (i=1:3) &
         E(i,:)=fieldInit(i)
    forall (i=1:3) &
         B(i,:)=fieldInit(i+3)
  end subroutine field_init_constant__
  !
  subroutine field_init__(timeIn) !-----------------------------------------
    real,intent(in) :: timeIn ! time in fact
    real :: focusDist ! distance to focal plane
    real :: timeReal ! in fact time distance to focal plane
    real :: xx ! temporal values for x,y in oscillating antenna plane
    real :: ee(2:3) ! (amplitude)*(polarization)*(phase)
    !
    if (mpiProc == mpiHost .and. .false.) then
       write(*,*) ' FIELD 1',timeIn !beamLimit(jBeam,1),beamLimit(jBeam,2) 
    end if
    !
    if(timeIn < beamLimit(jBeam,1) .or. timeIn > beamLimit(jBeam,2)) return
    !
    xx=xMin(1)-dx(1)
    !
    focusDist=sqrt((focus(jBeam,1)-xx)**2) +xMin(1) !new
    timeReal=timeIn+focusDist-focus(jBeam,1)
    !
    if (mpiProc == mpiHost .and. .false.) then
       write(*,*) ' FIELD 11',focusDist,focus(jBeam,1),timeIn, timeReal
    end if
    !
    ee(2)=polarization(jBeam,2) &
         *cos((timeIn+phase(xx+dxHalf(1)) &
         +phaseShift(jBeam))*PI2)
    ee(3)=polarization(jBeam,3) &
         *sin((timeIn+phase(xx+dxHalf(1)) &
         +phaseShift(jBeam))*PI2)
    !
    if (jBeam == 1) E(:, nMask(1) )=ZERO
    !
    if(timeReal > beamLimit(jBeam,2) &
         .or. timeReal < beamLimit(jBeam,1)) return
    E(2,nMask(1))=E(2,nMask(1)) &
         +ee(2)*prof(timeIn,xx+dxHalf(1))
    E(3,nMask(1))=E(3,nMask(1)) &
         +ee(3)*prof(timeIn,xx+dxHalf(1))
    !
    if (mpiProc == mpiHost .and. .false.) then
       write(*,*) ' FIELD 2' &!,beamLimit(jBeam,1),beamLimit(jBeam,2) &
            ,prof(timeIn,xx+dxHalf(1))
    end if
    !
    ! periodic boundary apply ???
    !
    !write(*,*)' sub: E0=',iTime,maxval(E(:,0,:,:))
    !
  end subroutine field_init__
  !
  function prof(timeIn,xx)
    real :: prof
    real, intent(in) :: xx,timeIn
    real :: focusDist,ff(nDim),tmp,tmp2,tmp3
    !
    focusDist=sqrt((focus(jBeam,1)-xx)**2)+xMin(1) !new
    ff=ZERO
    !
    select case(nProfile(jBeam))
       !
    case(1) !Gaussian
       tmp=((-focusDist+focus(jBeam,1)-beamMiddle(jBeam,1)-timeIn) &
            *TWO/beamSize(jBeam,1))**2
       if(tmp <= 30.) ff(1)=exp(-tmp)
       !
    case(2) !Fronts
       tmp=-focusDist+sqrt(focus(jBeam,1)**2+focus(jBeam,2)**2)-timeIn
       !         
       ff(1)=ONE
       !
       if(-tmp < beamLimit(jBeam,1) + beamFront(jBeam,1) ) then 
          tmp2 = ( tmp + beamLimit(jBeam,1) + beamFront(jBeam,1) ) &
               *TWO/beamFront(jBeam,1) 
          ff(1)=exp(-tmp2**2) 
       end if
       !
       if(-tmp > beamLimit(jBeam,2) - beamFront(jBeam,2) ) then 
          tmp2 = ( tmp + beamLimit(jBeam,2) - beamFront(jBeam,2) ) &
               *TWO/beamFront(jBeam,2) 
          ff(1)=exp(-tmp2**2) 
       end if
       !
       !ff(1)=ff(1)*
       !new! to implement!
       tmp3=(beamFrontCoef(jBeam,1)-beamFrontCoef(jBeam,2)) &
            /(-beamLimit(jBeam,1)+beamLimit(jBeam,2) &
            -beamFront(jBeam,1)-beamFront(jBeam,2)) &
            *(tmp+beamLimit(jBeam,1)+beamFront(jBeam,1)) &
            +beamFrontCoef(jBeam,1)
       !write(*,*) '======ff(1)',&
            !-(beamFrontCoef(jBeam,1)-beamFrontCoef(jBeam,2)) &
            !/(-beamLimit(jBeam,1)+beamLimit(jBeam,2) &
            !-beamFront(jBeam,1)-beamFront(jBeam,2)), &
            !tmp,tmp3
       ff(1)=ff(1)*sqrt(tmp3) !I=a^2!
       !
    case(3) !Exponential Fronts 
       tmp=-focusDist+sqrt(focus(jBeam,1)**2+focus(jBeam,2)**2)-timeIn
       !         
       ff(1)=ONE
       !
       if(-tmp < beamLimit(jBeam,1) + beamFront(jBeam,1) ) then 
          tmp2 = ( tmp + beamLimit(jBeam,1) + beamFront(jBeam,1) ) &
               !give exp factor here: ONE,TWO,THREE....
               *1.5 /beamFront(jBeam,1) !factor
          ff(1)=sqrt( exp(-abs(tmp2)) )
       end if
       !
       if(-tmp > beamLimit(jBeam,2) - beamFront(jBeam,2) ) then 
          tmp2 = ( tmp + beamLimit(jBeam,2) - beamFront(jBeam,2) ) &
               !give exp factor here: ONE,TWO,THREE....
               *1.5 /beamFront(jBeam,2)  !factor
          ff(1)=sqrt( exp(-abs(tmp2)) )
       end if
       !
    end select
    !
    prof=product(ff)
    !
  end function prof
  !
  function phase(xx)
    real :: phase
    real, intent(in) :: xx
    real :: focusDist 
    focusDist=sqrt((focus(jBeam,1)-xx)**2)+xMin(1) !new
    phase=focusDist-focus(jBeam,1)
  end function phase
  !
end module fields_initiate
