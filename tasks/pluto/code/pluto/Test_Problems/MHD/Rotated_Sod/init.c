/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief MHD rotated shock tube problem.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Nov 27, 2018

  \b Reference: 
     - [MT10] "A second-order unsplit Godunov scheme for cell-centered MHD: The CTU-GLM scheme"
        Mignone \& Tzeferacos, JCP (2010) 229, 2217
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Init (double *v, double x, double y, double z)
/*
 *
 *********************************************************************** */
{
  static int first_call = 1;
  int    jx = g_inputParam[INT_JX];
  int    jy = g_inputParam[INT_JY];
  int    kx = g_inputParam[INT_KX];
  int    kz = g_inputParam[INT_KZ];
  double xm = 0.5*(g_domEnd[IDIR] + g_domBeg[IDIR]); /* Initial discontinuity */
                                                     /* location             */
  double ym = 0.5*(g_domEnd[JDIR] + g_domBeg[JDIR]); /* Initial discontinuity */
                                                     /* location             */
  double xcoord[3];
  
  g_gamma = g_inputParam[GAMMA_EOS];

/* ----------------------------------------------
   0. Initialize Rotation
   ---------------------------------------------- */

  RotateSet(jx,jy,kx,kz);

/* ----------------------------------------------
   1. Rotate coordinate to 1D frame.
      The center of rotation is the point (0.5,0)
      of the original frame.
   ---------------------------------------------- */

  xcoord[0] = x-xm; xcoord[1] = y; xcoord[2] = z;
  RotateVector(xcoord, 1);
  x = xcoord[0]; y = xcoord[1]; z = xcoord[2];
  
/* ----------------------------------------------
   2. Assign initial conditions.
      We use a small negative value (-1.e-12) so
      that the row of zones immediately above
      the midplane line is discontinuous at
      x = 0;
   ---------------------------------------------- */

  if ( x <= 1.e-6 ){   
    v[RHO] = g_inputParam[RHO_LEFT];
    v[VX1] = g_inputParam[VX_LEFT];
    v[VX2] = g_inputParam[VY_LEFT];
    v[VX3] = g_inputParam[VZ_LEFT];
    v[BX1] = g_inputParam[BX_CONST];
    v[BX2] = g_inputParam[BY_LEFT];
    v[BX3] = g_inputParam[BZ_LEFT];
    v[PRS] = g_inputParam[PR_LEFT];
  }else{
    v[RHO] = g_inputParam[RHO_RIGHT];
    v[VX1] = g_inputParam[VX_RIGHT];
    v[VX2] = g_inputParam[VY_RIGHT];
    v[VX3] = g_inputParam[VZ_RIGHT];
    v[BX1] = g_inputParam[BX_CONST];
    v[BX2] = g_inputParam[BY_RIGHT];
    v[BX3] = g_inputParam[BZ_RIGHT];
    v[PRS] = g_inputParam[PR_RIGHT];
  }

  #if DIVIDE_BY_4PI == YES
  v[BX1] /= sqrt(4.0*CONST_PI);
  v[BX2] /= sqrt(4.0*CONST_PI);
  v[BX3] /= sqrt(4.0*CONST_PI);
  #endif

  #if PHYSICS == ResRMHD
  v[EX1] = -(v[VX2]*v[BX3] - v[VX3]*v[BX2]); 
  v[EX2] = -(v[VX3]*v[BX1] - v[VX1]*v[BX3]); 
  v[EX3] = -(v[VX1]*v[BX2] - v[VX2]*v[BX1]); 

  v[CRG] = 0.0;
  #endif

  v[AX1] = 0.0;
  v[AX2] = x*v[BX3];
  v[AX3] = y*v[BX1] - x*v[BX2];

  #ifdef PSI_GLM 
  v[PSI_GLM] = 0.0;
  #endif

/* ----------------------------------------------
   3. Rotate vectors from 1D frame to 2D frame
      [counter-clockwise]
   ---------------------------------------------- */

  RotateVector (v + AX1, -1);
  RotateVector (v + VX1, -1);
  RotateVector (v + BX1, -1);

  #if PHYSICS == ResRMHD
  RotateVector (v + EX1, -1);
  #endif

}

/* ********************************************************************* */
void InitDomain (Data *d, Grid *grid)
/*! 
 * Assign initial condition by looping over the computational domain.
 * Called after the usual Init() function to assign initial conditions
 * on primitive variables.
 * Value assigned here will overwrite those prescribed during Init().
 *
 *
 *********************************************************************** */
{
}

/* ********************************************************************* */
void Analysis (const Data *d, Grid *grid)
/* 
 *
 *
 *********************************************************************** */
{

}
/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox *box, int side, Grid *grid) 
/*! 
 *
 *
 *********************************************************************** */
{
//  RotateBoundary(d, box, side, grid);
}
