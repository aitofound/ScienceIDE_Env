#include "SimDomains.h"
// #include "linear_solver_wrapper_c.h" // Calling Fortran solver
#include "LinearSolver.h"

using namespace amrex;

void matvec_E_solver(const double *vecIn, double *vecOut, int iLev) {
  fleksDomains(fleksDomains.selected())
      .pic->update_E_matvec(vecIn, vecOut, iLev);
}

void matvec_divE_accurate(const double *vecIn, double *vecOut, int iLev) {
  fleksDomains(fleksDomains.selected())
      .pic->divE_accurate_matvec(vecIn, vecOut, iLev);
}

void linear_solver_gmres(double tolerance, int nIteration, int nVarSolve,
                         int nDim, int nGrid, double *rhs, double *xLeft,
                         MATVEC fMatvec, int iLev, bool doReport) {

  int nJ = 1, nK = 1, nBlock = 1;
  double precond_matrix_II[1][1];
  precond_matrix_II[0][0] = 0;
  int lTest = (doReport && ParallelDescriptor::MyProc() == 0);

  if (true) {
    MPI_Comm iComm = ParallelDescriptor::Communicator();
    PrecondType TypePrecond = NONE;
    linear_solver_wrapper_hy(fMatvec, iLev, GMRES, tolerance, nIteration,
                             nVarSolve, nDim, nGrid, nJ, nK, nBlock, iComm, rhs,
                             xLeft, TypePrecond, precond_matrix_II[0], lTest);
  } else { // Fortran solver
    /*
    // The shared library matvec requires non-const vecIn due to compatibility
    // issue with AMPS. If one intends to use it, remember to change the
    // definition of `const double *vecIn` to `double vecIn`.
    MPI_Fint iComm = MPI_Comm_c2f(ParallelDescriptor::Communicator());
    //// parameter to choose preconditioner types
    // 0:No precondition; 1: BILU; 2:DILU;
    //[-1,0): MBILU;
    double PrecondParam = 0;
    linear_solver_matvec_c = fMatvec;
    linear_solver_wrapper("GMRES", &tolerance, &nIteration, &nVarSolve, &nDim,
                          &nGrid, &nJ, &nK, &nBlock, &iComm, rhs, xLeft,
                          &PrecondParam, precond_matrix_II[0], &lTest);
    */
  }
}

void linear_solver_wrapper_hy(
    std::function<void(const double *, double *, const int)> matvec, int iLev,
    // Matrix-free operation
    const KrylovType typeSolver,              // Type of Solver
    const double tolerance,                   // Tolerance for the solver
    const int nIteration,                     // Max iteration number
    const int nVar,                           // Number of impl. variables/cell
    const int nDim,                           // Number of spatial dimensions
    const int nI, const int nJ, const int nK, // Number of cells in a block
    const int nBlock,              // Number of impl. blocks on current proc
    const MPI_Comm iComm,          // MPI communicator for processors
    double *rhs_I,                 // RHS std::vector
    double *x_I,                   // Initial guess/solution
    const PrecondType typePrecond, // Parameter for the preconditioner
    double *precondMatrix_II, // Diagonal and super/sub diagonal elements from
                              // the matrix A, which is in the equation Ax = b
    const int lTest) {
  struct LinearSolverParam param;

  param.typePrecond = typePrecond;
  if (param.typePrecond == NONE) {
    param.doPrecond = false;
  } else {
    param.doPrecond = true;
  }

  param.typeKrylov = typeSolver;
  param.typeStop = REL;
  param.errorMax = tolerance;
  param.maxMatvec = nIteration;
  param.nKrylovVector = nIteration;
  param.useInitialGuess = false;

  // CG solver does not have preconditioner.
  assert(!(param.doPrecond && param.typeKrylov == CG));

  int nVarIjk = nVar * nI * nJ * nK; // Number of variables per block

  int nImpl = nVarIjk * nBlock; // Number of variables per processor

  bool DoTest = lTest == 1;

  // Make sure that left preconditioning is used when necessary
  param.typePrecondSide = LEFT;

  // Initialize solution std::vector to zero
  std::fill(x_I, x_I + nImpl, 0.0);

  // Get preconditioning matrix if required.
  // Precondition RHS and initial guess (for symmetric prec only)
  // if (param.doPrecond) WIP

  // Initialize stopping conditions. Solver will return actual values.
  param.nMatvec = param.maxMatvec;
  param.error = param.errorMax;

  // Label the tolerance/error explicitly as relative or absolute
  const char *typeError = param.typeStop == REL ? "relative" : "absolute";

  if (DoTest)
    std::cout << "Before " << param.typeKrylov << " nMatVec, " << typeError
              << " tol: " << param.nMatvec << " " << param.error << std::endl;

  // Solve linear problem
  switch (param.typeKrylov) {
    case GMRES:
      gmres(matvec, iLev, rhs_I, x_I, param.useInitialGuess, nImpl,
            param.nKrylovVector, param.error, param.typeStop, param.nMatvec,
            DoTest, iComm);
      break;
    case BICGSTAB:
      // bicgstab(matvec, rhs_I, x_I, param.useInitialGuess, nImpl,
      //         param.error, param.typeStop, param.nMatvec,
      //         DoTest, iComm);
      break;
    case CG:
      // cg(matvec, rhs_I, x_I, param.useInitialGuess, nImpl,
      //   param.error, param.typeStop, param.nMatvec,
      //   DoTest, iComm);
      break;
    default:
      std::cout << "Unknown solver type = " << param.typeKrylov << std::endl;
  }

  if (DoTest)
    std::cout << "After nMatVec, " << typeError << " error = " << param.nMatvec
              << " " << param.error << std::endl;
}

/*
Initially written by Youcef Saad (May 23, 1985)
Revised by Henk A. van der Vorst and Mike Botchev (Oct 1996)
Rewritten into F90 and parallelized by Gabor Toth (May 2002)
Moved into LinearSolver.f90 for SWMF by Gabor Toth (Dec 2006)
Rewritten into C++ by Hongyang Zhou (Oct 2020)
*/
int gmres(std::function<void(const double *, double *, const int)> matvec,
          int iLev,                // Func for matrix std::vector multiplication
          const double *rhs,       // Right hand side std::vector
          double *sol,             // Initial guess / solution std::vector
          const bool isInit,       // true if sol contains initial guess
          const int n,             // Number of unknowns
          const int nKrylov,       // Size of krylov subspace
          double &tol,             // Required / achieved norm of residual
          const StopType typeStop, // Determine stopping criterion
          int &nIter,              // Maximum/actual number of iterations
          const bool doTest,       // Write debug info if true
          MPI_Comm iComm = MPI_COMM_SELF) // MPI communicator
{
  int info = -1;
  // gives reason for returning:
  //    abs(info)=  0: solution found satisfying given tolerance.
  //                2: no convergence within maximum number of iterations.
  //                3: initial guess satisfies the stopping criterion.
  //   sign(info)=  +: residual decreased
  //                -: residual did not reduce

#ifndef GMRES_VERBOSE
  // Concise mode: only the caller's summary lines are printed.
  (void)doTest; // Avoid the unused-parameter warning.
#endif

#ifdef GMRES_VERBOSE
  if (doTest)
    std::cout << "GMRES tol,nIter:" << tol << " " << nIter << std::endl;
#endif

  int nKrylov1 = nKrylov + 1;
  std::vector<double> c(nKrylov);
  std::vector<double> s(nKrylov);
  std::vector<double> rs(nKrylov1);
  std::vector<double> krylovBuffer(n * (nKrylov + 2));
  std::vector<double> hhBuffer(nKrylov1 * nKrylov);
  auto *Krylov_II = krylovBuffer.data();
  auto *hh = hhBuffer.data();

  double epsmac;
  if (true) // double precision by default
  {
    epsmac = 1e-16;
  } else // single precision
  {
    epsmac = 1e-8;
  }

  double Tol1, ro, ro0;
  int its = 0;

  do // Restart loop
  {
    // Compute initial residual std::vector
    // Krylov_II[1]:=A*sol
    if (isInit || its > 0) {
      matvec(sol, Krylov_II, iLev);
      for (int i = 0; i < n; ++i) {
        Krylov_II[i] = rhs[i] - Krylov_II[i];
      }
    } else {
      // Save a matvec when starting from zero initial condition
      for (int i = 0; i < n; ++i) {
        Krylov_II[i] = rhs[i];
      }
    }
    //-------------------------------------------------------------
    ro = sqrt(dot_product_mpi(Krylov_II, Krylov_II, n, iComm));
    if (ro == 0.0) {
      if (its == 0) {
        info = 3;
      } else {
        info = 0;
      }

      tol = ro;
      nIter = its;
      return info;
    }

    // Set Tol1 for stopping criterion
    if (its == 0) {
      ro0 = ro;
      if (doTest)
        std::cout << "initial residual norm: " << ro0 << std::endl;
      if (typeStop == ABS) {
        Tol1 = tol;
        if (ro <= Tol1) { // Quit if accurate enough
          info = 3;
          tol = ro;
          nIter = its;
#ifdef GMRES_VERBOSE
          if (doTest)
            std::cout << "GMRES: nothing to do. info = " << info;
#endif
          return info;
        }
      } else {
        Tol1 = tol * ro;
      }
    }

    auto coef = 1.0 / ro;
    for (int i = 0; i < n; ++i) {
      Krylov_II[i] *= coef;
    }

    // Initialize 1st term of RHS of Hessenberg system
    rs[0] = ro;
    int i = 0;
    do // KRYLOVLOOP
    {
      int i1;
      its += 1;
      i1 = i + 1;

      // Krylov_II[i1]:=A*Krylov_II[i]
      matvec(&Krylov_II[i * n], &Krylov_II[i1 * n], iLev);

      // Modified Gram-Schmidt
      for (int j = 0; j <= i; ++j) {
        double t =
            dot_product_mpi(&Krylov_II[j * n], &Krylov_II[i1 * n], n, iComm);
        hh[i * nKrylov1 + j] = t;
        for (int k = 0; k < n; ++k) {
          Krylov_II[i1 * n + k] -= t * Krylov_II[j * n + k];
        }
      }
      double cDot = sqrt(
          dot_product_mpi(&Krylov_II[i1 * n], &Krylov_II[i1 * n], n, iComm));
      hh[i * nKrylov1 + i1] = cDot;
      if (cDot != 0.0) {
        cDot = 1.0 / cDot;
        for (int k = 0; k < n; ++k) {
          Krylov_II[i1 * n + k] *= cDot;
        }
      }
      // Done with modified Gram-Schmidt and Arnoldi step.
      // Update factorization of hh.
      // Perform previous transformations on i-th column of h.
      for (int k = 1; k <= i; ++k) {
        int k1 = k - 1;
        double t = hh[i * nKrylov1 + k1];
        hh[i * nKrylov1 + k1] = c[k1] * t + s[k1] * hh[i * nKrylov1 + k];
        hh[i * nKrylov1 + k] = -s[k1] * t + c[k1] * hh[i * nKrylov1 + k];
      }
      double g = sqrt(hh[i * nKrylov1 + i] * hh[i * nKrylov1 + i] +
                      hh[i * nKrylov1 + i1] * hh[i * nKrylov1 + i1]);
      if (g == 0.0)
        g = epsmac;
      // Determine next plane rotation
      c[i] = hh[i * nKrylov1 + i] / g;
      s[i] = hh[i * nKrylov1 + i1] / g;
      rs[i1] = -s[i] * rs[i];
      rs[i] *= c[i];
      // Determine residual norm and test for convergence
      hh[i * nKrylov1 + i] =
          c[i] * hh[i * nKrylov1 + i] + s[i] * hh[i * nKrylov1 + i1];
      ro = std::abs(rs[i1]);
#ifdef GMRES_VERBOSE
      if (doTest) {
        if (typeStop == REL) {
          std::cout << its << " matvecs, "
                    << " ||rn||/||r0|| = " << ro / ro0 << std::endl;
        } else if (typeStop == ABS) {
          std::cout << its << " matvecs, "
                    << " ||rn|| = " << ro << std::endl;
        }
      }
#endif
      i += 1;
    } while (i < nKrylov && (ro > Tol1));

    // Compute solution. First solve upper triangular system.
    // rs := hh(1:i,1:i) ^-1 * rs
    for (int j = i - 1; j >= 0; --j) {
      if (rs[j] != 0.0) {
        rs[j] /= hh[j * nKrylov1 + j];
        for (int k = j - 1; k >= 0; k--) {
          rs[k] -= rs[j] * hh[j * nKrylov1 + k];
        }
      }
    }

    // Done with back substitution.
    // Form linear combination to get solution.
    for (int j = 0; j < i; ++j) {
      for (int k = 0; k < n; ++k) {
        // sol[i] += rs[j] * Krylov_II[i][j];
        sol[k] += rs[j] * Krylov_II[j * n + k];
      }
    }
  } while (ro > Tol1 && its < nIter);

  // eps == tolerance for stopping criterion.
  // Process is stopped as soon as ( ||.|| is the euclidean norm):
  // || current residual||/||initial residual|| <= eps
  // on OUTPUT: actual achieved norm residual (if iabs!=0)
  // or achieved relative residual norm reduction.

  nIter = its;
  tol = tol / Tol1 * ro; // (relative) tolerance achieved

  if (ro < Tol1) {
    info = 0;
  } else if (ro < ro0) {
    info = 2;
  } else {
    info = -2;
  }

  return info;
}
