!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModCubicEquation
  use ModMpi, ONLY: &
       iRealPrec ! 1, if the code is compiled with double precision
  use ModNumConst, ONLY: cPi
  use ModUtilities, ONLY: CON_stop
  implicit none
  real, parameter:: cTolerance_I(0:1) = [2.0e-7, 1.0e-14]
  real, parameter:: cTwoPiOver3 = cPi*2.0/3.0
contains
  !============================================================================
  subroutine solve_cubic_equation(Root_I)
    ! Solve roots of cubic equation x^3 + a x^2 + bx + c = 0
    ! Array in: [a, b, c]
    ! Array out [RootMin, RootMid, RootMax]
    real, intent(inout) :: Root_I(3)
    ! Notations of Wolfram MathWorld:
    real :: a2, a1, a0   ! Coeff by second, first, zero power of x (i.e. a,b,c)
    ! Coefficients in the reduced equation
    ! x^3 + p x = q
    real :: p, q
    ! ThirdOfA2 = a2/3
    real :: ThirdOfA2
    ! Output:
    real :: RootMin, RootMid, RootMax
    ! ThirdOfArcCosC = acos(C)/3
    real :: C, ThirdOfArcCosC
    ! CoefByY = 2 sqrt(abs(p)/3)
    real :: CoefByY
    !--------------------------------------------------------------------------
    ! Get coefficients from the input array
    a2 = Root_I(1); a1 = Root_I(2); a0 = Root_I(3)
    ! Calculate coefficients of the reduced equation, x^3 + p x = q
    p = a1 - a2**2/3
    ! Check if the equation may have three real roots:
    if(p >= 0.0)then
       write(*,*)'In the cubic equation with the original coefficients =', &
            Root_I,' the reduced equation has the coefficient p>=0:     ', &
            p
       call CON_stop('Do not use the Vieta formula to solve this equation')
    end if
    q = (9*a1*a2 - 2*a2**3)/27 - a0
    C = 0.5*q*sqrt(27/abs(p)**3); ThirdOfA2 = a2/3
    if(abs(C) > 1)then
       write(*,*)'In the cubic equation with the original coefficients =', &
            Root_I,' the reduced equation has the positive disscriminator ', &
            ' q^2/4 + p^3/27 = ', q**2/4 + p**3/27
       call CON_stop('Do not use the Vieta formula to solve this equation')
    end if
    ThirdOfArcCosC = acos(C)/3
    CoefByY = 2*sqrt(abs(p)/3)
    RootMax = CoefByY*cos( ThirdOfArcCosC )              - ThirdOfA2
    RootMin = CoefByY*cos( ThirdOfArcCosC + cTwoPiOver3) - ThirdOfA2
    RootMid = CoefByY*cos( ThirdOfArcCosC - cTwoPiOver3) - ThirdOfA2
    Root_I  = [RootMin, RootMid, RootMax]
  end subroutine solve_cubic_equation
  !============================================================================
  subroutine test_cubic_equation
    ! Test 1
    ! Roots of the cubic equation x^3 + 2 x^2 -19 x - 20 = 0
    ! Or, equivalently, (x + 5)*(x + 1)*(x - 4)=0
    real, parameter :: Test1_I(3) = [2.0, -19.0, -20.0]
    ! ...are:
    real, parameter :: Ref1_I(3)  = [-5.0, -1.0, 4.0]
    ! Test 2
    ! Roots of the cubic equation x^3      -21 x + 20 = 0
    ! Or, equivalently, (x + 5)*(x - 1)*(x - 4)=0
    real, parameter :: Test2_I(3) = [0.0, -21.0, 20.0]
    ! ...are:
    real, parameter :: Ref2_I(3)  = [-5.0, 1.0, 4.0]
    ! Test 3
    ! Roots of the cubic equation x^3 + 8 x^2  + 11 x - 20 = 0
    ! Or, equivalently, (x + 5)*(x + 4)*(x - 1)=0
    real, parameter :: Test3_I(3) = [8.0, 11.0, -20.0]
    ! ...are:
    real, parameter :: Ref3_I(3)  = [-5.0, -4.0, 1.0]
    ! Test 4
    ! Roots of the cubic equation x^3 - 8 x^2  + 11 x + 20 = 0
    ! Or, equivalently, (x +1)*(x - 4)*(x - 5)=0
    real, parameter :: Test4_I(3) = [-8.0, 11.0, 20.0]
    ! ...are:
    real, parameter :: Ref4_I(3)  = [-1.0, 4.0, 5.0]
    ! Misc:
    real :: Root_I(3)
    !--------------------------------------------------------------------------
    ! Test 1
    Root_I = Test1_I
    call solve_cubic_equation(Root_I)
    call check(Root_I, Ref1_I)
    ! Test 2
    Root_I = Test2_I
    call solve_cubic_equation(Root_I)
    call check(Root_I, Ref2_I)
    ! Test 3
    Root_I = Test3_I
    call solve_cubic_equation(Root_I)
    call check(Root_I, Ref3_I)
    ! Test 4
    Root_I = Test4_I
    call solve_cubic_equation(Root_I)
    call check(Root_I, Ref4_I)
  contains
    !==========================================================================
    subroutine check(RootToCheck_I, Ref_I)
      real, intent(in) :: RootToCheck_I(3), Ref_I(3)
      !------------------------------------------------------------------------
      if(any(abs(RootToCheck_I - Ref_I) > cTolerance_I(iRealPrec)))then
         write(*,*)'test_cubic_equation failed:'
         write(*,*)'Reference solution: ', Ref_I
         write(*,*)'Found solution    : ', RootToCheck_I
      end if
    end subroutine check
    !==========================================================================
  end subroutine test_cubic_equation
  !============================================================================
end module ModCubicEquation
!==============================================================================
