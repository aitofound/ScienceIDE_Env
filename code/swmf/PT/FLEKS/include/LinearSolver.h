#ifndef _LINEARSOLVER_H_
#define _LINEARSOLVER_H_

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include <AMReX_ParallelDescriptor.H>

typedef void (*MATVEC)(const double *vecIn, double *vecOut, int n);

enum PrecondType {
  NONE,  // No preconditioner
  MBILU, // Gustaffson modification of diagonal blocks
  DILU,  // LU for diagonal, keep off-diagonal blocks
  BILU,  // LU for diagonal, premultiply U with D^-1
};
enum KrylovType { GMRES, BICGSTAB, CG };
enum class FieldSolverMode { GMRES, NewtonKrylov };
enum StopType { // (||.|| denotes the 2-norm):
  REL,          // relative stopping crit.:||res|| <= Tol*||res0||
  ABS,          // absolute stopping crit.:||res|| <= Tol
  MAX           // maximum  stopping crit.: max(abs(res)) <= Tol
};
enum PrecondSideType { LEFT };

namespace fleks_jfnk {

inline double finite_difference_epsilon(double normBase, double normDirection) {
  if (normDirection <= 0.0) {
    return 0.0;
  }

  // Scale the finite-difference step with the base state and perturbation
  // norms.
  return std::sqrt(std::numeric_limits<double>::epsilon()) * (1.0 + normBase) /
         normDirection;
}

template <typename NonlinearMatvec>
void jacobian_free_matvec(NonlinearMatvec nonlinearMatvec, const double *base,
                          const double *baseMatvec, const double *direction,
                          double *out, double *work, const int n,
                          const int iLev, const double epsilon) {
  if (epsilon == 0.0) {
    std::fill(out, out + n, 0.0);
    return;
  }

  // Evaluate (F(base + epsilon * direction) - F(base)) / epsilon.
  for (int i = 0; i < n; ++i) {
    work[i] = base[i] + epsilon * direction[i];
  }

  nonlinearMatvec(work, out, iLev);

  const double invEpsilon = 1.0 / epsilon;
  for (int i = 0; i < n; ++i) {
    out[i] = (out[i] - baseMatvec[i]) * invEpsilon;
  }
}

} // namespace fleks_jfnk

struct LinearSolverParam {
  bool doPrecond;                  // Do preconditioning
  PrecondSideType typePrecondSide; // Precondition left, right, symmetric
  PrecondType typePrecond;         // Preconditioner type
  KrylovType typeKrylov;           // Krylov solver type
  StopType typeStop;               // Stopping criterion type
  double errorMax;                 // Tolerance for solver
  int maxMatvec;                   // Maximum number of iterations
  int nKrylovVector;               // Number of vectors for GMRES
  bool useInitialGuess;            // Non-zero initial guess
  double error;                    // Actual accuracy achieved
  int nMatvec;                     // Actual number of iterations
};

struct Block {
  int nDim;       // Number of spatial dimensions
  int nVar;       // Number of impl. variables/cell
  int nI, nJ, nK; // Number of cells in a block
  int nBlock;     // Number of impl. blocks on current proc
  double *precondMatrix_II;
};

void matvec_E_solver(const double *vecIn, double *vecOut, int iLev);
void matvec_divE_accurate(const double *vecIn, double *vecOut, int iLev);

void linear_solver_gmres(double tolerance, int nIteration, int nVarSolve,
                         int nDim, int nGrid, double *rhs, double *xLeft,
                         MATVEC fMatvec, int iLev, bool doReport = true);

void linear_solver_wrapper_hy(
    std::function<void(const double *, double *, const int)> matvec, int iLev,
    const KrylovType solverType, const double tolerance, const int nIteration,
    const int nVar, const int nDim, const int nI, const int nJ, const int nK,
    const int nBlock, MPI_Comm iComm, double *Rhs_I, double *x_I,
    const PrecondType TypePrecond, double *precond_matrix, const int lTest);

// hyzhou: eventually we should use this and merge the above into this class!
class LinearSolver {
  int nGrid;
  int nVar;
  int nSolve;
  int nDim;
  double tol;
  int nIter;
  MATVEC fMatvec;
  std::vector<double> rhsBuffer;
  std::vector<double> xLeftBuffer;
  std::vector<double> matvecBuffer;

public:
  double *rhs;
  double *xLeft;
  double *matvec;

  LinearSolver()
      : nGrid(0),
        nVar(0),
        nSolve(0),
        nDim(0),
        tol(1),
        nIter(0),
        fMatvec(nullptr),
        rhs(nullptr),
        xLeft(nullptr),
        matvec(nullptr) {}

  LinearSolver(const LinearSolver &) = delete;
  LinearSolver &operator=(const LinearSolver &) = delete;

  void de_alloc() {
    rhsBuffer.clear();
    xLeftBuffer.clear();
    matvecBuffer.clear();
    rhs = nullptr;
    xLeft = nullptr;
    matvec = nullptr;
  }

  void reset(int in) {
    const int nSolveNew = in * nVar;
    if (in != nGrid || nSolveNew != nSolve) {
      nGrid = in;
      de_alloc();
      nSolve = nSolveNew;
      if (nSolve > 0) {
        rhsBuffer.assign(nSolve, 0.0);
        xLeftBuffer.assign(nSolve, 0.0);
        matvecBuffer.assign(nSolve, 0.0);
        rhs = rhsBuffer.data();
        xLeft = xLeftBuffer.data();
        matvec = matvecBuffer.data();
      }
    }

    for (int i = 0; i < nSolve; ++i) {
      rhs[i] = 0;
      xLeft[i] = 0;
      matvec[i] = 0;
    }
  }

  void set_tol(amrex::Real in) { tol = in; }
  void set_nIter(int in) { nIter = in; }

  void init(int nGridIn, int nVarIn, int nDimIn, MATVEC fIn) {
    nVar = nVarIn;
    nDim = nDimIn;
    fMatvec = fIn;
    reset(nGridIn);
  }

  void solve(int iLev, bool doReport = true) {
    linear_solver_gmres(tol, nIter, nVar, nDim, nGrid, rhs, xLeft, fMatvec,
                        iLev, doReport);
  }

  void solve(std::function<void(const double *, double *, const int)> matvec,
             int iLev, bool doReport = true) {
    int nJ = 1, nK = 1, nBlock = 1;
    double precondMatrix[1] = { 0.0 };
    int lTest = (doReport && amrex::ParallelDescriptor::MyProc() == 0);

    linear_solver_wrapper_hy(matvec, iLev, GMRES, tol, nIter, nVar, nDim, nGrid,
                             nJ, nK, nBlock,
                             amrex::ParallelDescriptor::Communicator(), rhs,
                             xLeft, NONE, precondMatrix, lTest);
  }

  int get_nSolve() const { return nSolve; }
};

int gmres(std::function<void(const double *, double *, const int)> matvec,
          int iLev,                // Func for matrix vector multiplication
          const double *rhs,       // Right hand side vector
          double *sol,             // Initial guess / solution vector
          const bool isInit,       // true if Sol contains initial guess
          const int n,             // Number of unknowns
          const int nKrylov,       // Size of krylov subspace
          double &tol,             // Required / achieved residual
          const StopType typeStop, // Determine stopping criterion
          int &nIter,              // Maximum/actual number of iterations
          const bool doTest,       // Write debug info if true
          MPI_Comm iComm);         // MPI communicator

#endif
