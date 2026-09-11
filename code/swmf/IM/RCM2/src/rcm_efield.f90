module RCM_efield

  use ModUtilities, ONLY: CON_stop
  use RCM_variables
  use RCM_routines
  use RCM_gmresm, ONLY: define_pde_matrix, gmresm, gmresm_matvec, &
       gmresm_unwrap_pde_solution

  implicit none

  private

  public:: compute_efield
  
contains
  !============================================================================
  subroutine compute_efield

    ! a separate RCM routine to handle solving for
    ! ionospheric potential including setting the
    ! boundary condition (at high-L boundary)

    INTEGER :: j, info_bicgstab
    !--------------------------------------------------------------------------
    IF (ipot == 4) then    ! GMRES

       !  Note: new_cfive thinks c5w is the old c5w without d
       !  denominator. Before activating winds, check this.  5/29/99

       if (IsPartofFramework) then

          DO j = 1-n_gc, jsize+n_gc
             vbnd(j) = v(imin_j(j),j)
          END DO

       else

          CALL Get_potential_on_bndy ()

       end if

       CALL Comput_c5_wind
       CALL New_cfive
       CALL New_coeff
       CALL Comput_lowlat_boundary
       CALL Define_pde_matrix ()
       CALL Gmresm (Gmresm_Matvec, nij_pde, x0_pde, b_mtrx_pde, &
            tol_gmres, iter_max_gmres)
       CALL Gmresm_unwrap_pde_solution ()
       !
    ELSE IF(ipot == 5) then    ! BICGSTAB
       !
       CALL Comput_c5_wind
       CALL New_cfive
       CALL New_coeff
       CALL Comput_lowlat_boundary
       CALL Define_pde_matrix ()
       iter_bicgstab = iter_max_bicgstab
       tol_bicgstab = 3.0E-4
       CALL RCM_bicgstab (Gmresm_Matvec, b_mtrx_pde, x0_pde, SIZE(x0_pde), &
            tol_bicgstab, 'rel', iter_bicgstab, info_bicgstab)
       CALL Gmresm_unwrap_pde_solution ()
       if (info_bicgstab /= 0 ) then
          print*,info_bicgstab
          call CON_stop('IM_ERROR: bicgstab failed')
       end if
       !
    ELSE IF (ipot == 6) THEN
       !
       ! Potential should have been set elsewhere,
       ! here we assume that array V holds the necesssary
       ! values.
       !
    ELSE
       !
       call CON_STOP('IM_ERROR: invalid value for ipot')
       !
    END IF

  end subroutine compute_efield

  !============================================================================

  subroutine get_potential_on_bndy

    INTEGER (iprec) :: j
    REAL    (rprec) :: r_eq, p_eq
    !
    IF (ipcp_type == 11) THEN
       DO j = 1, jsize
          vbnd (j) = -vdrop * SIN(aloct(1,j)-vdrop_phase*HTR ) / &
               2.0_rprec * 1.0E+3_rprec
       END DO
       !
    ELSE IF (ipcp_type == 13) THEN
       !
       !       Maynard and Chen [JGR, 1975]:
       call CON_stop('Incorrect function: p_eq = Gntrp_2d_ang(..) ')

       DO j = 1, jsize
          r_eq = Gntrp_2d (rmin, isize, jsize, bndloc(j), REAL(j,rprec))
          p_eq = Gntrp_2d (pmin, isize, jsize, bndloc(j), REAL(j,rprec))
          ! p_eq = Gntrp_2d_ang (pmin, isize, jsize, bndloc(j), REAL(j,rprec))
          vbnd (j) = (92.4_rprec / r_eq - &
               A_coeff_MC(Kp)*r_eq**2*SIN(p_eq)) * 1000.0_rprec
       END DO
       !
    ELSE
       call CON_stop('ERROR in IM/RCM2/src/rcm_comput.f90:'// &
            'VBOUND: IPCP_TYPE NOT IMPLEMENTED')
    END IF
    vbnd (0) = vbnd (jsize)
    vbnd (-1) = vbnd (jsize-1)
    vbnd (jsize+1) = vbnd (1)
    vbnd (jsize+2) = vbnd (2)

  CONTAINS
    FUNCTION A_coeff_MC (Kp)

      REAL (rprec), INTENT (IN) :: Kp
      REAL (rprec) :: A_coeff_MC
      A_coeff_MC = 0.045_rprec / (1.0_rprec-0.159_rprec*Kp+0.0093_rprec*Kp**2)**3

    END FUNCTION A_coeff_MC

  end subroutine Get_potential_on_bndy
  !===========================================================================
  subroutine RCM_bicgstab(matvec,rhs,qx,n,tol,typestop,iter,info)

    ! Simple BiCGstab(\ell=1) iterative method
    ! Modified by G.Toth from the \ell<=2 version written
    ! by M.A.Botchev, Jan.'98. 
    ! Parallelization for the BATS-R-US code (2000-2001) by G. Toth
    !
    ! This is the "vanilla" version of BiCGstab(\ell) as described
    ! in PhD thesis of D.R.Fokkema, Chapter 3.  It includes two enhancements 
    ! to BiCGstab(\ell) proposed by G.Sleijpen and H.van der Vorst in
    ! 1) G.Sleijpen and H.van der Vorst "Maintaining convergence 
    !    properties of BiCGstab methods in finite precision arithmetic",
    !    Numerical Algorithms, 10, 1995, pp.203-223
    ! 2) G.Sleijpen and H.van der Vorst "Reliable updated residuals in
    !    hybrid BiCG methods", Computing, 56, 1996, 141-163
    !
    ! {{ This code is based on:
    ! subroutine bistbl v1.0 1995
    !
    ! Copyright (c) 1995 by D.R. Fokkema.
    ! Permission to copy all or part of this work is granted,
    ! provided that the copies are not made or distributed
    ! for resale, and that the copyright notice and this
    ! notice are retained.  }}

    ! This subroutine determines the solution of A.QX=RHS, where
    ! the matrix-vector multiplication with A is performed by
    ! the subroutine 'proj_matvec'. For symmetric matrix use the 
    ! more efficient proj_cg algorithm!

    use ModMpiOrig

    ! Arguments

    interface
       subroutine matvec(a,b,n)
         use RCM_variables, ONLY: rprec
         ! Calculate b = M.a where M is the matrix
         integer, intent(in) :: n
         real(rprec), intent(in) ::  a(n)
         real(rprec), intent(out) :: b(n)
       end subroutine matvec
    end interface
    !        subroutine for matrix vector multiplication

    integer, intent(in) :: n
    !        on input:  number of unknowns

    real(rprec), intent(inout) :: rhs(n)
    !        on input:  right-hand side vector.
    !        on output: residual vector.

    real(rprec), intent(out):: qx(n)
    !       on output: solution vector.

    real(rprec), intent(inout) :: tol
    !       on input:  required (relative) 2-norm or maximum norm of residual
    !       on output: achieved (relative) 2-norm or maximum norm of residual

    character (len=3), intent(in) :: typestop
    !      Determine stopping criterion (||.|| denotes the 2-norm):
    !      typestop='rel'    -- relative stopping crit.: ||res|| <= tol*||res0||
    !      typestop='abs'    -- absolute stopping crit.: ||res|| <= tol
    !      typestop='max'    -- maximum  stopping crit.: max(abs(res)) <= tol

    ! NOTE for typestop='rel' and 'abs': 
    !            To save computational work, the value of 
    !            residual norm used to check the convergence inside the main 
    !            iterative loop is computed from 
    !            projections, i.e. it can be smaller than the true residual norm
    !            (it may happen when e.g. the 'matrix-free' approach is used).
    !            Thus, it is possible that the true residual does NOT satisfy
    !            the stopping criterion ('rel' or 'abs').
    !            The true residual norm (or residual reduction) is reported on 
    !            output in parameter TOL -- this can be changed to save 1 MATVEC
    !            (see comments at the end of the subroutine)

    integer, intent(inout) :: iter
    !       on input:  maximum number of iterations to be performed.
    !       on output: actual  number of iterations done.

    integer, intent(out)   :: info
    !       Gives reason for returning:
    !     abs(info)=  0 - solution found satisfying given tolerance.
    !                 1 - iteration aborted due to division by very small value.
    !                 2 - no convergence within maximum number of iterations.
    !                 3 - initial guess satisfies the stopping criterion.
    !    sign(info)=  + - residual decreased
    !                 - - residual did not reduce

    ! Local parameters

    integer, parameter :: qz_=1,zz_=3,y0_=5,yl_=6,qy_=7

    ! Local variables (only 4 big vectors are needed):

!!! Automatic arrays !!!
    real(rprec), dimension(n):: bicg_r, bicg_u, bicg_r1, bicg_u1

    real(rprec) :: rwork(2,7)

    logical GoOn, rcmp, xpdt
    integer nmv
    real(rprec) alpha, beta, omega, rho0, rho1, sigma
    real(rprec) varrho, hatgamma
    real(rprec) assumedzero, rnrm0, rnrm, rnrmMax0, rnrmMax
    real(rprec) mxnrmx, mxnrmr, kappa0, kappal

    logical :: oktest=.true.

    !---------------------------------------------------------------------------
    if(oktest)write(*,*)'RCM_bicgstab tol,iter:',tol,iter

    info = 0

    if (tol<=0.0) call CON_stop('Error in RCM_BiCGSTAB: tolerance < 0')
    if (iter<=1)  call CON_stop('Error in RCM_BiCGSTAB: maxmatvec < 2')

    !
    !     --- Initialize first residual
    !
    ! assumedzero = 1.e-16
    assumedzero = 2.e-7
    bicg_u=0.0
    bicg_r=rhs
    qx    =0.0

    nmv = 0
    !
    !     --- Initialize iteration loop
    !

    rnrm0 = sqrt( dot_product_mpi(bicg_r,bicg_r,n))

    rnrm = rnrm0
    if(oktest) print *,'initial rnrm:',rnrm

    mxnrmx = rnrm0
    mxnrmr = rnrm0
    rcmp = .false.
    xpdt = .false.

    alpha = 0.0
    omega = 1.0
    sigma = 1.0
    rho0 =  1.0
    !
    !     --- Iterate
    !
    select case(typestop)
    case('rel')
       GoOn = rnrm>tol*rnrm0 .and. nmv<iter
       assumedzero = assumedzero*rnrm0
       rnrmMax = 0
       rnrmMax0 = 0

    case('abs')
       GoOn = rnrm>tol       .and. nmv<iter
       assumedzero = assumedzero*rnrm0
       rnrmMax = 0
       rnrmMax0 = 0

    case('max')
       rnrmMax0 = maxval_abs_mpi(bicg_r,n)
       rnrmMax  = rnrmMax0
       if(oktest) print *,'initial rnrmMax:',rnrmMax
       GoOn = rnrmMax>tol    .and. nmv<iter
       assumedzero = assumedzero*rnrmMax
    case default
       call CON_stop('Error in RCM_BiCGSTAB: unknown typestop value')
    end select

    if (.not.GoOn) then
       if(oktest) print *,'RCM_BiCGSTAB: nothing to do. info = ',info
       iter = nmv
       info = 3
       RETURN
    end if

    do while (GoOn)
       !
       !     =====================
       !     --- The BiCG part ---
       !     =====================
       !
       rho0 = -omega*rho0

       rho1 = dot_product_mpi(rhs,bicg_r,n)

       if (abs(rho0)<assumedzero**2) then
          info = 1
          RETURN
       endif
       beta = alpha*(rho1/rho0)
       rho0 = rho1
       bicg_u = bicg_r - beta*bicg_u

       call matvec(bicg_u,bicg_u1,n)
       nmv = nmv+1

       sigma=dot_product_mpi(rhs,bicg_u1,n)

       if (abs(sigma)<assumedzero**2) then
          info = 1
          RETURN
       endif

       alpha = rho1/sigma

       qx     = qx     + alpha*bicg_u
       bicg_r = bicg_r - alpha*bicg_u1

       call matvec(bicg_r,bicg_r1,n)
       nmv = nmv+1

       rnrm = sqrt( dot_product_mpi(bicg_r,bicg_r,n) )

       mxnrmx = max (mxnrmx, rnrm)
       mxnrmr = max (mxnrmr, rnrm)

       !DEBUG
       if(oktest)&
                                !!        write(*,*)'rho0, rho1, beta, sigma, alpha, rnrm:',&
                                !!        rho0, rho1, beta, sigma, alpha, rnrm
            
                                !
                                !  ==================================
                                !  --- The convex polynomial part ---
                                !  ================================== 
                                !
                                !    --- Z = R'R a 2 by 2 matrix
                                ! i=1,j=0
            rwork(1,1) = dot_product_mpi(bicg_r,bicg_r,n)

       ! i=1,j=1
       rwork(2,1) = dot_product_mpi(bicg_r1,bicg_r,n)
       rwork(1,2) = rwork(2,1) 

       ! i=2,j=1
       rwork(2,2) = dot_product_mpi(bicg_r1,bicg_r1,n)

       !
       !   --- tilde r0 and tilde rl (small vectors)
       !
       rwork(1:2,zz_:zz_+1)   = rwork(1:2,qz_:qz_+1)
       rwork(1,y0_) = -1.0
       rwork(2,y0_) = 0.0

       rwork(1,yl_) = 0.0
       rwork(2,yl_) = -1.0
       !
       !   --- Convex combination
       !
       rwork(1:2,qy_) = rwork(1,yl_)*rwork(1:2,qz_) + &
            rwork(2,yl_)*rwork(1:2,qz_+1)

       kappal = sqrt( sum( rwork(1:2,yl_)*rwork(1:2,qy_) ) )

       rwork(1:2,qy_) = rwork(1,y0_)*rwork(1:2,qz_) + &
            rwork(2,y0_)*rwork(1:2,qz_+1)

       kappa0 = sqrt( sum( rwork(1:2,y0_)*rwork(1:2,qy_) ) )

       varrho = sum( rwork(1:2,yl_)*rwork(1:2,qy_) )  
       varrho = varrho / (kappa0*kappal)

       hatgamma = sign(1.0,varrho)*max(abs(varrho),0.7) * (kappa0/kappal)

       rwork(1:2,y0_) = -hatgamma*rwork(1:2,yl_) + rwork(1:2,y0_)

       !
       !    --- Update
       !
       omega = rwork(2,y0_)

       bicg_u = bicg_u - omega*bicg_u1

       qx     = qx     + omega*bicg_r

       bicg_r = bicg_r - omega*bicg_r1

       rwork(1:2,qy_) = rwork(1,y0_)*rwork(1:2,qz_) + &
            rwork(2,y0_)*rwork(1:2,qz_+1)

       rnrm = sqrt( sum( rwork(1:2,y0_)*rwork(1:2,qy_) ) )

       select case(typestop)
       case('rel')
          GoOn = rnrm>tol*rnrm0 .and. nmv<iter
          !       if(oktest) print *, nmv,' matvecs, ', ' ||rn||/||r0|| =',rnrm/rnrm0

       case('abs')
          GoOn = rnrm>tol       .and. nmv<iter
          if(oktest) print *, nmv,' matvecs, ||rn|| =',rnrm

       case('max')
          rnrmMax = maxval_abs_mpi(bicg_r,n)

          GoOn = rnrmMax>tol    .and. nmv<iter
          if(oktest) print *, nmv,' matvecs, max(rn) =',rnrmMax
       end select

    end do
    !
    !     =========================
    !     --- End of iterations ---
    !     =========================

    select case(typestop)
    case('rel')
       if (rnrm>tol*rnrm0) info = 2
       tol = rnrm/rnrm0

    case('abs')
       if (rnrm>tol) info = 2
       tol = rnrm

    case('max')
       if (rnrmMax>tol) info = 2
       tol = rnrmMax
    end select

    if((typestop/='max'.and.rnrm>rnrm0).or.(typestop=='max'.and.rnrmMax&
         >rnrmMax0)) info=-info

    iter = nmv

  contains

    !============================================================================
    real(rprec) function maxval_abs_mpi(a,n)

      integer, intent(in) :: n
      real(rprec), intent(in)    :: a(n)

      integer :: ira
      real(rprec) :: local_max_abs, global_max_abs
      !--------------------------------------------------------------------------

      local_max_abs = maxval(abs(a))

      call MPI_allreduce(local_max_abs, global_max_abs, 1, MPI_REAL, MPI_MAX, &
           iComm, ira)

      maxval_abs_mpi = global_max_abs

    end function maxval_abs_mpi

    !============================================================================
    real(rprec) function dot_product_mpi(a,b,n)

      integer, intent(in) :: n
      real(rprec), intent(in)    :: a(n), b(n)

      integer :: ira
      real(rprec) :: local_dot_product, global_dot_product
      !--------------------------------------------------------------------------

      local_dot_product = sum(a*b)

      call MPI_allreduce(local_dot_product, global_dot_product, 1, MPI_REAL, &
           MPI_SUM, iComm, ira)

      dot_product_mpi = global_dot_product

    end function dot_product_mpi

    !==========================================================================
  end subroutine RCM_bicgstab
  
end module RCM_efield
