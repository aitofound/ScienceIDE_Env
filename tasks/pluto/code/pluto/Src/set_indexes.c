/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Perform index permutation and set domain integration indexes.

  The function SetIndexes() performs a cyclic permutation of the
  array indices corresponding to vector components (velocity,
  momentum, magnetic field, etc...).
  Indices are stored as global variables, see globals.h

  \author A. Mignone (andrea.mignone@unito.it)\n
  \date   July 09, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void SetVectorIndices (int dir)
/*!
 * Set vector indices and integration index range.
 *
 * \param [in] dir  the direction index
 *
 *********************************************************************** */
{
  if (dir == IDIR) {   /* -- Order: X-Y-Z  -- */

    VXn = MXn = VX1;
    VXt = MXt = VX2;
    VXb = MXb = VX3;
    #if (PHYSICS == MHD) || (PHYSICS == RMHD) || (PHYSICS == ResRMHD)
    BXn = BX1; 
    BXt = BX2; 
    BXb = BX3;
    #endif
    #if PHYSICS == ResRMHD
    EXn = EX1;
    EXt = EX2;
    EXb = EX3;
    #endif

    #if DUST_FLUID == YES
    VXn_D = MXn_D = VX1_D;
    VXt_D = MXt_D = VX2_D;
    VXb_D = MXb_D = VX3_D;
    #endif

    #if RADIATION
    FRn = FR1;  
    FRt = FR2; 
    FRb = FR3;
    #endif
 
  }else if (dir == JDIR){ /* -- Order: Y-Z-X  -- */

    VXn = MXn = VX2;
    VXt = MXt = VX3;
    VXb = MXb = VX1;
    #if (PHYSICS == MHD) || (PHYSICS == RMHD) || (PHYSICS == ResRMHD)
    BXn = BX2;
    BXt = BX3;
    BXb = BX1;
    #endif

    #if PHYSICS == ResRMHD
    EXn = EX2;
    EXt = EX3;
    EXb = EX1;
    #endif

    #if DUST_FLUID == YES
    VXn_D = MXn_D = VX2_D;
    VXt_D = MXt_D = VX3_D;
    VXb_D = MXb_D = VX1_D;
    #endif

    #if RADIATION
    FRn = FR2; 
    FRt = FR3;  
    FRb = FR1;
    #endif
    
  }else if (dir == KDIR){ /* -- Order: Z-X-Y   -- */

    VXn = MXn = VX3;
    VXt = MXt = VX1;
    VXb = MXb = VX2;
    #if (PHYSICS == MHD) || (PHYSICS == RMHD) || (PHYSICS == ResRMHD)
    BXn = BX3;
    BXt = BX1;
    BXb = BX2;
    #endif

    #if PHYSICS == ResRMHD
    EXn = EX3;
    EXt = EX1;
    EXb = EX2;
    #endif

    #if DUST_FLUID == YES
    VXn_D = MXn_D = VX3_D;
    VXt_D = MXt_D = VX1_D;
    VXb_D = MXb_D = VX2_D;
    #endif

    #if RADIATION
    FRn = FR3; 
    FRt = FR1; 
    FRb = FR2;
    #endif

  }
}

/* ********************************************************************* */
void ResetState (const Data *d, Sweep *sweep, Grid *grid)
/*!
 * Initialize some of the elements of the Sweep structure to zero
 * in order to speed up computations. These includes:
 *
 *    - source term
 *    - left and right eigenvectors
 *    - the maximum eigenvalue ???
 *
 * \param [in] d  pointer to Data structure
 * \param [out] sweep pointer to a Sweep structure
 * \param [in]  grid pointer to an array of Grid structures
 *
 *********************************************************************** */
{
  int i, j, k, nv;
  double v[NVAR], lambda[NVAR];
  double a;

  memset ((void *)sweep->src[0],   '\0', NMAX_POINT*NVAR*sizeof(double));

  memset ((void *)sweep->stateC.Rp[0][0], '\0', NMAX_POINT*NFLX*NFLX*sizeof(double));
  memset ((void *)sweep->stateC.Lp[0][0], '\0', NMAX_POINT*NFLX*NFLX*sizeof(double));
  memset ((void *)sweep->stateL.Rp[0][0], '\0', NMAX_POINT*NFLX*NFLX*sizeof(double));
  memset ((void *)sweep->stateL.Lp[0][0], '\0', NMAX_POINT*NFLX*NFLX*sizeof(double));
  memset ((void *)sweep->stateR.Rp[-1][0], '\0', NMAX_POINT*NFLX*NFLX*sizeof(double));
  memset ((void *)sweep->stateR.Lp[-1][0], '\0', NMAX_POINT*NFLX*NFLX*sizeof(double));
/*
  for (i = 0; i < NMAX_POINT; i++){
    for (j = NVAR; j--;  ) sweep->src[i][j] = 0.0;

    for (j = NFLX; j--;  ){
    for (k = NFLX; k--;  ){
      sweep->Lp[i][j][k] = sweep->Rp[i][j][k] = 0.0;
    }}
  }  
*/
/* ---------------------------------------------------------
     When using Finite Difference methods, we need to find,
     for each characteristic k, its maximum over the 
     whole computational domain (LF global splitting).
   --------------------------------------------------------- */

  #ifdef FINITE_DIFFERENCE
   FD_GetMaxEigenvalues (d, sweep, grid);
  #endif
}
