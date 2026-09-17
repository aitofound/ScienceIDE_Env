
module ModGITMImplicit

  !DESCRIPTION:
  ! This module implements a point implicit scheme for the implicit
  ! part of the right hand side Rimp = R - Rexp that contributes to the
  ! the implicitly treated variables Uimp, a subset of U=(Uexp, Uimp).
  ! Rimp should depend on the local cell values only: no spatial derivatives!
  !
  ! For a one stage scheme the variables are updated with the following
  ! first order scheme
  !
  ! Uexp^n+1 = Uexp^n + Dt*Rexp(U^n)
  ! Uimp^n+1 = Uimp^n + Dt*Rimp(U^n+1)
  !
  ! For the two stage scheme the following scheme is applied
  !
  ! Uexp^n+1/2 = Uexp^n + Dt/2 * Rexp(U^n)
  ! Uimp^n+1/2 = Uimp^n + Dt/2 * Rimp(U^n+1/2)
  !
  ! Uexp^n+1   = Uexp^n + Dt*Rexp(U^n+1/2)
  ! Uimp^n+1   = Uimp^n + Dt*beta*Rimp(U^n+1) + Dt*(1-beta)*Rimp(U^n)
  !
  ! where beta is in the range 0.5 and 1.0.
  ! The scheme is second order accurate in time for beta = 0.5.
  !
  ! For the general case Rimp is non-linear, and it is linearized as
  !
  ! Rimp(U^n+1/2) = Rimp(Uexp^n+1/2,Uimp^n) + dRimp/dUimp*(Uimp^n+1/2 - Uimp^n)
  ! Rimp(U^n+1)   = Rimp(Uexp^n+1,  Uimp^n) + dRimp/dUimp*(Uimp^n+1   - Uimp^n)
  !
  ! Note that the Jacobian dRimp/dUimp is evaluated at the partially advanced
  ! states (Uexp^n+1/2,Uimp^n) and (Uexp^n+1,Uimp^n) respectively.
  ! If Rimp is linear, the linearization is exact.
  !
  ! Substituting the linearization back into the one-stage and two-stage
  ! schemes yields a linear equation for the differences 
  ! (Uimp^n+1/2 - Uimp^n) and (Uimp^n+1 - Uimp^n), respectively.
  ! Since Rimp depends on the local cell values only, the linear equations
  ! can be solved point-wise for every cell.
  !
  ! The Jacobian can be given analytically by the subroutine passed to
  ! update_point_implicit, or it can be obtained by taking numerical
  ! derivatives of Rimp:
  !
  ! dRimp/dU^w = ((Rimp(Uexp^n+1,Uimp^n+eps^w) - Rimp(Uexp^n+1,Uimp^n))/eps^w
  !                
  ! where eps^w is a small perturbation in the w-th component of Uimp.
  !EOP

  implicit none

  save

  private ! except

  logical, public :: UsePointImplicit  ! Use point impl scheme?
  integer, public, allocatable :: &
       iVarPointImpl_I(:)                        ! Indexes of point impl. vars
  logical, public :: IsPointImplMatrixSet=.false.! Is dS/dU matrix analytic?
  logical, public :: IsPointImplPerturbed=.false.! Is the state perturbed?

  real, public, allocatable :: &
       DsDu_VVC(:,:,:,:,:), &     ! dS/dU derivative matrix
       EpsPointImpl_V(:)          ! absolute perturbation per variable
  real, public    :: EpsPointImpl ! relative perturbation

!  public update_point_implicit    ! do update with point implicit scheme


  ! Local variables
  ! Number of point implicit variables
  integer :: nVarPointImpl   

contains
 
  !===========================================================================
  

  !============================================================================

  subroutine linear_equation_solver(nVar, Matrix_VV, Rhs_V)

    integer, intent(in) :: nVar
    real, intent(inout) :: Matrix_VV(nVar, nVar)
    real, intent(inout) :: Rhs_V(nVar)

    ! This routine solves the system of Nvar linear equations:
    ! 
    !               Matrix_VV*dUCell = Rhs_V.
    ! 
    ! The result is returned in Rhs_V, the matrix is overwritten
    ! with the LU decomposition.
    !
    ! The routine performs a lower-upper (LU) decomposition of the 
    ! square matrix Matrix_VV of rank Nvar and then uses forward and
    ! backward substitution to obtain the solution vector dUCell.
    ! Crout's method with partial implicit pivoting is used to perform
    ! the decompostion.

    integer, parameter :: MAXVAR = 100

    integer :: IL, II, ILMAX, JL, KL, LL, INDX(MAXVAR)
    real    :: SCALING(MAXVAR), LHSMAX, LHSTEMP, TOTALSUM
    real, parameter :: TINY=1.0E-20

    !--------------------------------------------------------------------------
    if(nVar > MAXVAR) call stop_GITM(&
         'ERROR in ModPointImplicit linear solver: MaxVar is too small')

    !\
    ! Loop through each row to get implicit scaling
    ! information.
    !/
    DO IL=1,nVar
       LHSMAX=0.00
       DO JL=1,nVar
          IF (ABS(Matrix_VV(IL,JL)).GT.LHSMAX) LHSMAX=ABS(Matrix_VV(IL,JL))
       END DO
       SCALING(IL)=1.00/LHSMAX
    END DO

    !\
    ! Peform the LU decompostion using Crout's method.
    !/
    DO JL=1,nVar
       DO IL=1,JL-1
          TOTALSUM=Matrix_VV(IL,JL)
          DO KL=1,IL-1
             TOTALSUM=TOTALSUM-Matrix_VV(IL,KL)*Matrix_VV(KL,JL)
          END DO
          Matrix_VV(IL,JL)=TOTALSUM
       END DO
       LHSMAX=0.00
       DO IL=JL,nVar
          TOTALSUM=Matrix_VV(IL,JL)
          DO KL=1,JL-1
             TOTALSUM=TOTALSUM-Matrix_VV(IL,KL)*Matrix_VV(KL,JL)
          END DO
          Matrix_VV(IL,JL)=TOTALSUM
          LHSTEMP=SCALING(IL)*ABS(TOTALSUM)
          IF (LHSTEMP.GE.LHSMAX) THEN
             ILMAX=IL
             LHSMAX=LHSTEMP
          END IF
       END DO
       IF (JL.NE.ILMAX) THEN
          DO KL=1,nVar
             LHSTEMP=Matrix_VV(ILMAX,KL)
             Matrix_VV(ILMAX,KL)=Matrix_VV(JL,KL)
             Matrix_VV(JL,KL)=LHSTEMP
          END DO
          SCALING(ILMAX)=SCALING(JL)
       END IF
       INDX(JL)=ILMAX
       IF (abs(Matrix_VV(JL,JL)).EQ.0.00) Matrix_VV(JL,JL)=TINY
       IF (JL.NE.nVar) THEN
          LHSTEMP=1.00/Matrix_VV(JL,JL)
          DO IL=JL+1,nVar
             Matrix_VV(IL,JL)=Matrix_VV(IL,JL)*LHSTEMP
          END DO
       END IF
    END DO

    !\
    ! Peform the forward and back substitution to obtain
    ! the solution vector.
    !/
    II=0
    DO IL=1,nVar
       LL=INDX(IL)
       TOTALSUM=Rhs_V(LL)
       Rhs_V(LL)=Rhs_V(IL)
       IF (II.NE.0) THEN
          DO JL=II,IL-1
             TOTALSUM=TOTALSUM-Matrix_VV(IL,JL)*Rhs_V(JL)
          END DO
       ELSE IF (TOTALSUM.NE.0.00) THEN
          II=IL
       END IF
       Rhs_V(IL)=TOTALSUM
    END DO
    DO IL=nVar,1,-1
       TOTALSUM=Rhs_V(IL)
       DO JL=IL+1,nVar
          TOTALSUM=TOTALSUM-Matrix_VV(IL,JL)*Rhs_V(JL)
       END DO
       Rhs_V(IL)=TOTALSUM/Matrix_VV(IL,IL)
    END DO

    
  end subroutine linear_equation_solver
  
  subroutine init_pt_implicit(nvar)

    integer, intent(in) :: nvar

    integer :: iVar
    
    if(allocated(iVarPointImpl_I)) deallocate(iVarPointImpl_I)
    allocate(iVarPointImpl_I(nVar))
    
    do iVar = 1, nVar
       iVarPointImpl_I(iVar) = iVar
    enddo

  end subroutine init_pt_implicit
    

end module ModGITMImplicit
