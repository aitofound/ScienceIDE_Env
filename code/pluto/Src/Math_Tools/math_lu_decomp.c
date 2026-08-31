/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief  Functions for LU decomposition and matrix inversion.
  \author A. Mignone (andrea.mignone@unito.it)
  \date   June 24, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
int LUDecompose (double **a, int n, int *indx, double *d)
/*!   
 * Perform LU decomposition. Adapted from Numerical Recipes.
 * On output, replaces the matrix a[0..n-1][0..n-1] with its LU 
 * decomposition. indx[0..n-1] is a row vector that records the row 
 * permutation while d = 1 (-1) if the number of raw interchanges
 * was even (odd). 
 * This function should be used in combination with LUBackSubst() to
 * solve linear system of equations.
 * 
 *********************************************************************** */
#define TINY 1.0e-20;
{
  int i, imax, j, k;
  static int max_size = 0;
  double big, dum, sum, temp;
  static double *vv;

  if (n > max_size) {
    if (vv != NULL) {
      FreeArray1D(vv);
      g_usedMemory -= sizeof(double)*max_size;
    }
    vv = ARRAY_1D (n, double);
    max_size = n;
  }

  *d = 1.0;
  for (i = 0; i < n; i++) {
    big = 0.0;
    for (j = 0; j < n; j++)
      if ((temp = fabs (a[i][j])) > big)
        big = temp;
    if (big == 0.0) {
/*      printLog ("! Singular matrix in routine LUDecompose - (i=%d, j=%d)",i,j); */
      return (0);
    }
    vv[i] = 1.0 / big;
  }
  for (j = 0; j < n; j++) {
    for (i = 0; i < j; i++) {
      sum = a[i][j];
      for (k = 0; k < i; k++)
        sum -= a[i][k] * a[k][j];
      a[i][j] = sum;
    }
    big = 0.0;
    for (i = j; i < n; i++) {
      sum = a[i][j];
      for (k = 0; k < j; k++)
        sum -= a[i][k] * a[k][j];
      a[i][j] = sum;
      if ((dum = vv[i] * fabs (sum)) >= big) {
        big = dum;
        imax = i;
      }
    }
    if (j != imax) {
      for (k = 0; k < n; k++) {
        dum = a[imax][k];
        a[imax][k] = a[j][k];
        a[j][k] = dum;
      }
      *d = -(*d);
      vv[imax] = vv[j];
    }
    indx[j] = imax;
    if (a[j][j] == 0.0)
      a[j][j] = TINY;
    if (j != n - 1) {
      dum = 1.0 / (a[j][j]);
      for (i = j + 1; i < n; i++)
        a[i][j] *= dum;
    }
  }
  return (1); /* -- success -- */
}
#undef TINY

/* ********************************************************************* */
void LUBackSubst (double **a, int n, int *indx, double b[])
/*!
 * Solve a linear system of the type <tt>Ax = b</tt>, where \c A is 
 * a square matrix, \c x is the array of unknowns and \c b is given.
 * This function must be called after LUDecompose() 
 * (adapted from Numerical Recipes).
 * For the 1st time, use:
 * \code
 *   LUDecompose (A,n,indx,&d);
 *   LUBackSubst (A,n,indx,b);
 * \endcode
 * The answer \c x will be given back in \c b and the original matrix \c A 
 * will be destroyed.
 * For all subsequent time, to solve the same system wth a different 
 * right-hand side \c b, use 
 * \code
 *   LUBackSubst(A,n,indx,b);
 * \endcode
 * where \c A is the matrix that has been decomposed by LUDecompose() and
 * \em not the original matrix \c A.
 * 
 *********************************************************************** */
{
  int i, ii = 0, ip, j;
  double sum;

  for (i = 0; i < n; i++) {
    ip = indx[i];
    sum = b[ip];
    b[ip] = b[i];
    if (ii)
      for (j = ii - 1; j <= i - 1; j++)
        sum -= a[i][j] * b[j];
    else if (sum)
      ii = i + 1;
    b[i] = sum;
  }
  for (i = n - 1; i >= 0; i--) {
    sum = b[i];
    for (j = i + 1; j < n; j++) sum -= a[i][j]*b[j];
    b[i] = sum / a[i][i];
  }
}

/* ********************************************************************* */
void MatrixInverse (double **A, double **Ainv, int n)
/*!
 * Find the inverse of a matrix.
 * ! Important: the matrix A is destroyed on output !
 *
 *********************************************************************** */
{
  int    i, j, *indx;
  double d, *col; 
  
  indx = ARRAY_1D(n, int);
  col  = ARRAY_1D(n, double);
  
  LUDecompose (A,n,indx,&d); 
  for(j = 0; j < n;j++) {
    for (i = 0; i < n; i++) col[i]=0.0; 
    col[j] = 1.0;
    LUBackSubst(A,n,indx,col); 
    for(i = 0; i < n; i++) Ainv[i][j]=col[i];
  }
  FreeArray1D(indx);
  FreeArray1D(col);
  g_usedMemory -= (sizeof(int) + sizeof(double))*n;
}

/* ********************************************************************* */
void MatrixMultiply (double **A, double **B, double **C, int n)
/*!
 * Multiply two matrices, C = A.B
 *
 *********************************************************************** */
{
  int i,j,k;
  
  for (i = 0; i < n; i++){    
  for (j = 0; j < n; j++){    
    C[i][j] = 0.0;
    for (k = 0; k < n; k++){
      C[i][j] += A[i][k]*B[k][j];
    }
  }}
  
}

/* ********************************************************************* */
void TridiagonalSolve(double *am, double *a0, double *ap, double *b,
                      double *y, int n)
/*!
 *  Use recursion formula to solve a tridiagonal system of the type
 *
 *    am[i]*y[i-1] + a0[i]*y[i] + ap[i]*y[i+1] = b[i]
 *
 * from i = 1..n-1.
 * Boundary coefficients must have been imposed at y[0] and y[n]
 * On output y[] is replaced with the updated solution.
 *********************************************************************** */
{
  int i;
  double alpha[n], beta[n], gamma;

  alpha[n-1] = 0.0;
  beta[n-1]  = y[n];
  for (i = n-1; i > 0; i--){
    gamma      = -1.0/(a0[i] + ap[i]*alpha[i]);
    alpha[i-1] = gamma*am[i];
    beta[i-1]  = gamma*(ap[i]*beta[i] - b[i]);
  }

  for (i = 0; i < n-1; i++){
    y[i+1] = alpha[i]*y[i] + beta[i];
  }

/* Verify solution of tridiagonal matrix */
/*
  double res;
  for (i = 1; i < n; i++){
    res = am[i]*phi[i-1] + a0[i]*phi[i] + ap[i]*phi[i+1] - b[i];
    if (fabs(res) > 1.e-9){
      cout << "! TridiagonalSolve: not correct" << endl;
      exit(1);
    }
  }
*/
}






/* ********************************************************************* */
void GaussianElimination (double **A, double *x, double *b, int n)
/*!
 * Solve a system Ax = b by Gaussian elimination (no LU decomp)
 *********************************************************************** */
{
  int i, j, k, imax;
  double Amax, tmp;

  for (k = 0; k < n-1; k++){        // Loop over the Gk's

  /* -- Apply Partial pivoting here -- */

    Amax = fabs(A[k][k]);
    imax = k;
    for (i = k+1; i < n; i++) {
      tmp = fabs(A[i][k]);
      if (tmp > Amax) {
        imax = i;
        Amax = tmp;
      }
    }
    if (k != imax) SwapRows (A, b, k, imax, n);

  /* -- Loop over rows -- */

    for (i = k+1; i < n; i++){ 
      tmp = A[i][k]/A[k][k];
      for (j = k+1; j < n; j++) A[i][j] -= tmp*A[k][j];
      A[i][k] = 0.0;
      b[i]   -= tmp*b[k];
    }
  }

/* --  Backsubstitution -- */

  for (i = n-1; i >= 0; i--){
    tmp = b[i];
    for (j = n-1; j > i; j--) tmp -= x[j]*A[i][j];
    x[i] = tmp/A[i][i];
  }
}


/* ********************************************************************* */
void SwapRows (double **A, double *b, int i1, int i2, int n)
/*
 * Swap rows i1 and i2 in the matrix A[][] and vector b[]
 * A[][] is assumed to be a square matrix of dimension n 
 *********************************************************************** */
{
  int j;
  double tmp;

  for (j = 0; j < n; j++){
    tmp = A[i1][j];
    A[i1][j] = A[i2][j];
    A[i2][j] = tmp;
  }

  tmp   = b[i1];
  b[i1] = b[i2];
  b[i2] = tmp;
}

