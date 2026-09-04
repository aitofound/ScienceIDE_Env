!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModHyperGeometric
  use ModMpi, ONLY: &
       iRealPrec ! 1, if the code is compiled with double precision
  use ModNumConst, ONLY: cPi
  use ModUtilities, ONLY: CON_stop
  implicit none
  PRIVATE ! Except
  public :: toroid_p ! tilde{P}_n function, related to k**3 * (k^\prime)**n
  public :: toroid_q ! tilde{Q}_n function, related to k**3 * (k^\prime)**n
  public :: cothu    ! converts \kappa^\prime^2=\exp(-2u) to \coth (u)
  public :: scr_inductance    ! inductance of a superconducting ring
  public :: l0_ext_inductance ! the external inductance of a ring current
  public :: l0_int_inductance ! self-inductance of a uniform ring current
  public :: l0_tor_inductance ! toroidal field inductane
  public :: calc_elliptic_int_1kind  ! Elliptic integral, of the first kind
  public :: calc_elliptic_int_2kind  ! Elliptic integral, of the second kind
  real, parameter:: cTolerance_I(0:1) = [1.0e-7, 1.0e-15]
  real, parameter:: cEiler    = 0.57721566490
  real, parameter:: cSqrtPi   = 1.7724538509055159
contains
  !============================================================================
  real function psi_semi(n)
    !$acc routine seq
    integer, intent(in) :: n

    ! Definition of psi function: psi(z) = d\log(\Gamma(z))/dz
    ! For semiinteger z:
    ! psi(0.5 + n) = -C - log 4 +\sum_{k=1}^n{1/(k - 0.5)}
    integer :: k

    !--------------------------------------------------------------------------
    psi_semi = -cEiler -log(4.0)
    do k=1, abs(n)
       psi_semi = psi_semi +1.0/(real(k) - 0.50)
    end do
  end function psi_semi
  !============================================================================
  real function psi_int(n)
    !$acc routine seq
    integer, intent(in) :: n

    ! Definition of psi function: psi(z) = d\log(\Gamma(z))/dz
    ! For integer z:
    ! psi(1 + n) = -C +\sum_{k=1}^{n-1}{1.0/k}
    integer :: k

    !--------------------------------------------------------------------------
    psi_int = -cEiler
    do k = 1, n -1
       psi_int = psi_int +1.0/real(k)
    end do
  end function psi_int
  !============================================================================
  real function factorial(n)
    !$acc routine seq
    integer, intent(in) :: n
    ! n! = \Gamma(n+1)
    integer :: k

    !--------------------------------------------------------------------------
    factorial = 1.0
    do k=2, n
       factorial = factorial*real(k)
    end do
  end function factorial
  !============================================================================
  real function gamma_semi(n)
    !$acc routine seq
    integer, intent(in) :: n
    ! \Gamma(0.5 + n)
    integer :: k
    ! n >= -1!
    !--------------------------------------------------------------------------
    gamma_semi = -2.0*cSqrtPi
    do k = 0, n
       gamma_semi = gamma_semi * (real(k) - 0.50)
    end do
  end function gamma_semi
  !============================================================================

  ! Hypergeometric series
  real function hypergeom(A, B, C, Z)
    !$acc routine seq

    ! Input parameters and argument
    real, intent(in) :: A, B, C, Z

    ! Loop variable
    integer:: i

    ! Misc
    real :: aPlusI, bPlusI, cPlusI, rMember
    character(len=*), parameter:: NameSub = 'hypergeom'
    !--------------------------------------------------------------------------
    hypergeom = 1.0
    rMember   = 1.0
    aPlusI = a -1.0
    bPlusI = b -1.0
    cPlusI = c -1.0
    i = 0
    do while(abs(rMember) >= cTolerance_I(iRealPrec))
       i = i +1
       aPlusI = aPlusI + 1.0
       bPlusI = bPlusI + 1.0
       cPlusI = cPlusI + 1.0
       rMember = rMember*aPlusI*bPlusI*z/(cPlusI*i)
       hypergeom = hypergeom + rMember
    end do
  end function hypergeom
  !============================================================================
  function hyper_semi_semi_int(nAIn, nBIn, nCIn, ZIn, OneMinusZIn) &
       RESULT(FuncVal)
    !$acc routine seq
    integer,        intent(in) :: nAIn, nBIn, nCIn
    real, optional, intent(in) :: ZIn, OneMinusZIn
    real    :: Z, OneMinusZ
    real    :: FuncVal
    ! Calculate hypergeometric series F(a, b, c, z), if
    ! semiinteger a = 0.5 + nA, nA = 0, 1, 2
    ! semiinteger b = 0.5 + nB, nB = 0, 1, 2
    ! integer     c = nC
    real :: A, B, C
    integer :: nA, nB, nC
    integer :: n ! Discriminator= c - a -b

    ! Loop variable
    integer :: i

    integer :: iTry

    ! Misc
    real :: aPlusI, bPlusI, cPlusI, rMember, LogFactor

    character(len=*), parameter:: NameSub = 'hyper_semi_semi_int'
    !--------------------------------------------------------------------------

    ! If n = nC - (1 + nA + nB) is less than zero, change the values
    ! of nA, nB and try again
    do iTry = 1, 2
       if(iTry == 1)then
          nA = nAIn; nB = nBIn; nC = nCIn
       else
          nA = nAIn + n; nB = nBIn + n; nC = nCIn
       end if
       n = nC - (1 + nA + nB)

       if(present(ZIn))then
          Z = ZIn; OneMinusZ = 1.0 - Z
       elseif(present(OneMinusZIn))then
          OneMinusZ = OneMinusZIn; Z = 1.0 - OneMinusZ
       else
#ifndef _OPENACC
          call CON_stop(&
               NameSub//': ZIn or OneMinusZIn should be present')
#endif
       end if
       ! Real arguments of the hypergeometric function:
       A =  0.50 + real(nA); B = 0.50 + real(nB); C = real(nC)
       if (abs(z) < 0.50) then

          ! Direct summation of the hypergeometric series, well withing the
          ! convergence radius
          FuncVal = hypergeom(&
               A=A,        &
               B=B,        &
               C=C,        &
               Z=Z)
          RETURN
       end if

       ! The formulae for the "logarithmic case" (integer n)
       ! https://dlmf.nist.gov
       ! strongly depend on the sign of n. Consider case-by-case
       if(n==0)then
          ! Apply Eq. 15.8.10 in https://dlmf.nist.gov:
          LogFactor    = -log(OneMinusZ) + 2.0*psi_int(1) - &
               (psi_semi(nA) + psi_semi(nB))
          rMember      = factorial(nA + nB)/(gamma_semi(nA)*gamma_semi(nB))
          FuncVal = rMember*LogFactor
          aPlusI       = A - 1.0
          bPlusI       = B - 1.0
          i = 0
          do while(abs(rMember) >= cTolerance_I(iRealPrec))
             i = i + 1
             aPlusI = aPlusI + 1.0
             bPlusI = bPlusI + 1.0
             rMember = rMember*aPlusI*bPlusI/i**2*OneMinusZ
             LogFactor = LogFactor + 2.0/real(i) - 1.0/aPlusI - 1.0/bPlusI
             FuncVal = FuncVal + rMember*LogFactor
          end do
       elseif(n<0)then
          CYCLE
          ! Apply Eq. 15.8.12 in https://dlmf.nist.gov:
          !  n = -n
          !  FuncVal = hyper_semi_semi_int(nA - n, nB - n, nC, ZIn, OneMinusZIn)/&
          !       OneMinusZ**n
       else
          ! Apply Eq. 15.8.10 in https://dlmf.nist.gov:
          rMember      = factorial(nC-1)*factorial(n-1)/(&
               gamma_semi(nA + n)*gamma_semi(nB + n))
          FuncVal = rMember
          aPlusI       = A - 1.0
          bPlusI       = B - 1.0
          do i = 1, n-1
             aPlusI = aPlusI + 1.0
             bPlusI = bPlusI + 1.0
             rMember = rMember*aPlusI*bPlusI/(real(i)*real(n - i))*(-OneMinusZ)
             FuncVal = FuncVal + rMember
          end do
          aPlusI = aPlusI + 1.0
          bPlusI = bPlusI + 1.0
          cPlusI = real(n)
          rMember = rMember*aPlusI*bPlusI/cPlusI*(-OneMinusZ)
          LogFactor    = -log(OneMinusZ) + psi_int(1) + psi_int(1 + n) - &
               (psi_semi(nA + n) + psi_semi(nB + n))
          FuncVal = FuncVal + rMember*LogFactor
          i = 0
          do while(abs(rMember) >= cTolerance_I(iRealPrec))
             i = i + 1
             aPlusI = aPlusI + 1.0
             bPlusI = bPlusI + 1.0
             cPlusI = cPlusI + 1.0
             rMember = rMember*aPlusI*bPlusI/(real(i)*cPlusI)*OneMinusZ
             LogFactor = LogFactor + 1.0/real(i) + 1.0/cPlusI - &
                  1.0/aPlusI - 1.0/bPlusI
             FuncVal = FuncVal + rMember*LogFactor
          end do

       end if

       if(iTry == 2) FuncVal = FuncVal/OneMinusZ**n
       RETURN
    end do
  end function hyper_semi_semi_int
  !============================================================================
  !=======================|Toroidal functions|=================================
  !                       v                  v
  !
  ! Calculate functions
  !
  ! sqrt(2\sinh u)P^{-1}_{n - 1/2}(\cosh u)/(k^3(k^\prime)^n)
  ! and
  ! sqrt(2\sinh u)Q^{-1}_{n - 1/2}(\cosh u)/(k^3(k^\prime)^n)
  !
  ! The multiplier, k^3, is present in a definition
  ! of the toroid functions and while calculated detivatives
  ! this multiplier is taken into account, however, it is not
  ! calculated here, to avoid dividing zero by zero at k=0
  !
  ! Herewith, cosh u = (2 - k^2)/(2k^\prime);
  ! sinh u = k^2/(2k^\prime)
  ! k^\prime=sqrt(1 - k^2)=exp(-u)
  !
  real function toroid_p(n, Kappa2In, KappaPrime2In)
    !$acc routine seq
    ! tilde{P} function, integer n > 0 or n=0, related to k**3 * (k^\prime)**n
    integer, intent(in):: n
    real, optional, intent(in) :: Kappa2In, KappaPrime2In
    real :: Kappa2, KappaPrime2
    character(len=*), parameter:: NameSub = 'toroid_p'
    !--------------------------------------------------------------------------
#ifndef _OPENACC
    if(n < 0)call CON_stop(&
         NameSub//': argument n should be non-negative')
#endif
    if(present(Kappa2In))then
       toroid_p = 0.250*hyper_semi_semi_int(1, 1+n, 3, ZIn = Kappa2In)
    elseif(present(KappaPrime2In))then
       toroid_p = 0.250*hyper_semi_semi_int(1, 1+n, 3, &
            OneMinusZIn = KappaPrime2In)
    else
#ifndef _OPENACC
       call CON_stop(&
            NameSub//': Kappa2In or KappaPrime2InIn should be present')
#endif
    end if
  end function toroid_p
  !============================================================================
  real function toroid_q(n, KappaPrime2In, Kappa2In)
    !$acc routine seq
    ! tilde{Q}_n function, integer n > 0, related to k**3 * (k^\prime)**n
    integer, intent(in):: n
    real, optional, intent(in) :: KappaPrime2In, Kappa2In
    character(len=*), parameter:: NameSub = 'toroid_q'
    !--------------------------------------------------------------------------
#ifndef _OPENACC
    if(n < 0)call CON_stop(&
         NameSub//': argument n should be non-negative')
#endif
    if(present(KappaPrime2In))then
       toroid_q = hyper_semi_semi_int(1, 1+n, n+1, ZIn = KappaPrime2In)
    elseif(present(Kappa2In))then
       toroid_q = hyper_semi_semi_int(1, 1+n, n+1, OneMinusZIn = Kappa2In)
    else
#ifndef _OPENACC
       call CON_stop(&
            NameSub//': Kappa2In or KappaPrime2InIn should be present')
#endif
    end if
    toroid_q = -toroid_q*cSqrtPi*gamma_semi(n-1)/factorial(n)
  end function toroid_q
  !============================================================================
  !                           ^                  ^
  !===========================|Toroidal functions|=============================
  !==============================|Inductances|=================================
  !                              v           v
  real function scr_inductance(KappaPrime2)
    !$acc routine seq
    real, intent(in):: KappaPrime2

    ! for a superconducting ring calculate the ratio of inductance
    ! to \mu_0R_\infty as a function of (k^prime)^2. For a given R0 and a,
    ! R_\infty = sqrt(R_0^2-a^2) and k^\prime=a/(R_0+R_\infty)

    ! Inverse of inductances: from a given harmonic harmonic and total
    real:: InvInductance, InvInductanceTotal
    real:: Tolerance

    ! Loop variable
    integer::n
    !--------------------------------------------------------------------------
    InvInductance = toroid_q(0,KappaPrime2In=KappaPrime2)/&
         (0.25*toroid_p(0, KappaPrime2In=KappaPrime2))
    Tolerance = InvInductance*cTolerance_I(iRealPrec)
    InvInductanceTotal = InvInductance
    n = 0
    do while(InvInductance > Tolerance)
       n = n +1
       InvInductance = toroid_q(n, KappaPrime2In=KappaPrime2)/&
            ((0.25 - n*n)*toroid_p(n, KappaPrime2In=KappaPrime2))
       InvInductanceTotal = InvInductanceTotal + 2.0*InvInductance
    end do

    ! Invert and accunf for the common multiplier
    scr_inductance = 2.0*cPi**2/InvInductanceTotal
  end function scr_inductance
  !============================================================================
  real function l0_ext_inductance(KappaPrime2)
    !$acc routine seq
    real, intent(in):: KappaPrime2

    ! calculate the ratio of external inductance
    ! to \mu_0R_\infty as a function of (k^prime_0)^2.
    !--------------------------------------------------------------------------
    l0_ext_inductance = 0.50*toroid_p(0, KappaPrime2In=KappaPrime2)*cPi**2/&
         toroid_q(0,KappaPrime2In=KappaPrime2)
  end function l0_ext_inductance
  !============================================================================
  real function l0_int_inductance(KappaPrime2)
    !$acc routine seq
    real, intent(in):: KappaPrime2

    ! calculate the ratio of self-inductance of a uniform ring current
    ! to \mu_0R_\infty as a function of (k^prime_0)^2.
    real :: CurrentE      ! Uniform current
    real :: Kappa2, Kappa
    real :: ToroidQ0U0    ! Q^{-1}_{-1/2}(u_0)
    real :: DToroidQ0DuU0 ! dQ^{-1}_{-1/2}(u_0)/du_0
    !--------------------------------------------------------------------------
    Kappa2 = 1 - KappaPrime2; Kappa = sqrt(Kappa2)
    ToroidQ0U0 = Kappa2*Kappa*toroid_q(0,KappaPrime2In=KappaPrime2)
    ! dQ^{-1}_{-1/2)(u_0)/du_0 = 3*KappaPrime0/Kappa0**2*Q^{-1}_{1/2}(u_0)
    DToroidQ0DuU0 = 3*KappaPrime2*Kappa*toroid_q(1,KappaPrime2In=KappaPrime2)
    CurrentE = -1/DToroidQ0DuU0
    l0_int_inductance = 4*cPi**2*CurrentE*&
         ( (cothu(KappaPrime2) - 1)*0.75*CurrentE - 1/ToroidQ0U0)
  end function l0_int_inductance
  !============================================================================
  real function l0_tor_inductance(KappaPrime2)
    !$acc routine seq
    real, intent(in):: KappaPrime2

    ! calculate the ratio of self-inductance of a uniform ring current
    ! to \mu_0R_\infty as a function of (k^prime_0)^2.
    real :: CurrentE      ! Uniform current
    real :: Kappa2, Kappa
    real :: ToroidQ0U0    ! Q^{-1}_{-1/2}(u_0)
    real :: DToroidQ0DuU0 ! dQ^{-1}_{-1/2}(u_0)/du_0
    real :: DToroidP0DuU0 ! dP^{-1}_{-1/2}(u_0)/du_0
    !--------------------------------------------------------------------------
    Kappa2 = 1 - KappaPrime2; Kappa = sqrt(Kappa2)
    ToroidQ0U0 = Kappa2*Kappa*toroid_q(0,KappaPrime2In=KappaPrime2)
    ! dQ^{-1}_{-1/2)(u_0)/du_0 = 3*KappaPrime0/Kappa0**2*Q^{-1}_{1/2}(u_0)
    DToroidQ0DuU0 = 3*KappaPrime2*Kappa*toroid_q(1,KappaPrime2In=KappaPrime2)
    ! dP^{-1}_{-1/2)(u_0)/du_0 = 3*KappaPrime0/Kappa0**2*P^{-1}_{1/2}(u_0)
    DToroidP0DuU0 = 3*KappaPrime2*Kappa*toroid_p(1,KappaPrime2In=KappaPrime2)
    CurrentE = -1/DToroidQ0DuU0
    l0_tor_inductance = cPi**2*CurrentE*DToroidP0DuU0*&
         ( (cothu(KappaPrime2) - 1)*0.75*CurrentE*ToroidQ0U0 - 1)
  end function l0_tor_inductance
  !============================================================================
  real function cothu(KappaPrime2In)
    !$acc routine seq
    real, intent(in)     :: KappaPrime2In
    !--------------------------------------------------------------------------
    cothu = 1 + 2*KappaPrime2In/(1 - KappaPrime2In)
  end function cothu
  !============================================================================
  subroutine calc_elliptic_int_1kind(Z, KElliptic)
    !$acc routine seq
    real, intent(in):: Z
    real, intent(out):: KElliptic
    character(len=*), parameter:: NameSub = 'calc_elliptic_int_1kind'
    !--------------------------------------------------------------------------
    ! Calculate 2F1(0.5 +0, 0.5 + 0; 1; Z)
    KElliptic = 0.50*cPi*hyper_semi_semi_int(0, 0, 1, ZIn=Z**2)
  end subroutine calc_elliptic_int_1kind
  !============================================================================
  subroutine calc_elliptic_int_2kind(ArgK,EElliptic)
    !$acc routine seq

    real, intent(in):: ArgK
    real, intent(out):: EElliptic

    ! Loop variable
    integer:: i

    ! Misc
    real :: aPlusI, bPlusI, rMember, LogFactor, ArgKPrime2
    real :: OneOver2n2nMinus1
    character(len=*), parameter:: NameSub = 'calc_elliptic_int_2kind'
    !--------------------------------------------------------------------------
    EElliptic = 0.50*cPi*hyper_semi_semi_int(-1, 0, 1, ZIn=ArgK**2)
  end subroutine calc_elliptic_int_2kind
  !============================================================================
end module ModHyperGeometric
!==============================================================================
