/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Add source terms to the right hand side of relativistic HD/MHD eqns.

  Add source terms to the right hand side of RHD or RMHD equations in
  conservative form.
  These include

  -# Body forces;
  -# Powell's 8-waves source terms;
  

  Care is taken to ensure that gravity components are included even
  when a direction is not active.
  The following table summarizes:
  
  1D (x):
  Sweep| gx | gy | gz |
  -----|----|----|----|
   x   | o  | o  | o  |

  2D (x,y):
  Sweep| gx | gy | gz |
  -----|----|----|----|
   x   | o  |    |    |
   y   |    | o  | o  |

  2D (x,z):
  Sweep| gx | gy | gz |
  -----|----|----|----|
   x   | o  |    |    |
   z   |    | o  | o  |

  3D (x,y,z):
  Sweep| gx | gy | gz |
  -----|----|----|----|
   x   | o  |    |    |
   y   |    | o  |    |
   z   |    |    | o  |
        
  For consistency, the same approach must be used in PrimSource().
  
  \author A. Mignone (andrea.mignone@unito.it)
          L. DelZanna 
  \date   Apr 5, 2024
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#ifndef iMPHI
 #define iMPHI MX2  /* -- for Cartesian coordinates -- */
#endif

/* *********************************************************************** */
void RightHandSideSource (const Sweep *sweep, timeStep *Dts,
                          int beg, int end, double dt, double *phi_p, Grid *grid)
/*! 
 *
 * \param [in,out]  state  pointer to State_1D structure
 * \param [in]      Dts    pointer to time step structure
 * \param [in]      beg    initial index of computation
 * \param [in]      end    final   index of computation
 * \param [in]      dt     time increment
 * \param [in]      phi_p  force potential at interfaces
 * \param [in]      grid   pointer to Grid structure
 *
 * \return This function has no return value.
 ************************************************************************* */
{
  int    i, j, k, nv;

  const State *stateC = &(sweep->stateC);
  const State *stateL = &(sweep->stateL);
  const State *stateR = &(sweep->stateR);

  double r_1, g[3], fg[3], vg;

  double dtdx, scrh, ct;
  double Sm;
  double *x1   = grid->x[IDIR],  *x2  = grid->x[JDIR],  *x3  = grid->x[KDIR];
  double *x1p  = grid->xr[IDIR], *x2p = grid->xr[JDIR], *x3p = grid->xr[KDIR];
  double *x1m  = grid->xl[IDIR], *x2m = grid->xl[JDIR], *x3m = grid->xl[KDIR];
  double *dx1  = grid->dx[IDIR], *dx2 = grid->dx[JDIR], *dx3 = grid->dx[KDIR];
#if GEOMETRY == SPHERICAL
  double *rt   = grid->rt;
  double *sp   = grid->sp;
  double *sm   = grid->sp-1;
  double *s    = grid->s;
  double *dmu  = grid->dmu;
#endif
  double ***dV = grid->dV;
  double **rhs  = sweep->rhs;
  double **flux = sweep->flux;
  double **vp   = stateL->v;
  double **vm   = stateR->v-1;
  double *p;
  double cl;
  double lor2, vel2, vphi, phi_c;
  double *v, *u;
  double Et;

/* --------------------------
      pointer shortcuts
   -------------------------- */

  p  = sweep->press;
  
  i = g_i;  /* will be redefined during x1-sweep */
  j = g_j;  /* will be redefined during x2-sweep */
  k = g_k;  /* will be redefined during x3-sweep */

  PrimToCons(stateC->v, stateC->u, beg, end);  

  if (g_dir == IDIR){


    for (i = beg; i <= end; i++) {
      dtdx = dt/dx1[i];
      v    = stateC->v[i];
      u    = stateC->u[i];

    /* --------------------------------------------
       I1. Add geometrical source term
       -------------------------------------------- */

#if GEOMETRY == CARTESIAN

#elif GEOMETRY == CYLINDRICAL

#elif GEOMETRY == POLAR

#elif GEOMETRY == SPHERICAL

#endif

    /* ----------------------------------------------------
       I3. Include body forces all at once during the first
           sweep.
       ---------------------------------------------------- */

      #if (BODY_FORCE & VECTOR)
      BodyForceVector(v, g, x1[i], x2[j], x3[k]);
      Et = u[ENG];
      #if RMHD_REDUCED_ENERGY == YES
      Et += u[RHO];
      #endif
      rhs[i][MX1] += dt*Et*g[IDIR];
      IF_ENERGY (rhs[i][ENG] += dt*u[MX1]*g[IDIR];)

      /* ----------------------------------------
         Add non-active dimensions during
         this sweep.
         ---------------------------------------- */

      #if !INCLUDE_JDIR && !INCLUDE_KDIR
      rhs[i][MX2] += dt*Et*g[JDIR];
      rhs[i][MX3] += dt*Et*g[KDIR];
      IF_ENERGY (rhs[i][ENG] += dt*u[MX2]*g[JDIR];)
      IF_ENERGY (rhs[i][ENG] += dt*u[MX3]*g[KDIR];)
      #endif

      #endif  /* BODY_FORCE & VECTOR */


      #if (BODY_FORCE & POTENTIAL)
      #error Cannot use BodyForcePotential in relativistic flows
      #endif

      /* ----------------------------------------------------
       I4. Irradiation
       ---------------------------------------------------- */
      #if RADIATION
      #if IRRADIATION && (!RADIATION_IMPLICIT_NR)
      rhs[i][ENG] -= dt*v[FIR];
      #endif
      #endif

    }

  } else if (g_dir == JDIR){

    scrh = dt;
#if GEOMETRY == POLAR
    scrh /= x1[i];
    r_1   = 1.0/x1[i];
#elif GEOMETRY == SPHERICAL
    scrh /= rt[i];
    r_1   = 1.0/rt[i];
#endif
    for (j = beg; j <= end; j++) {
      dtdx = scrh/dx2[j];
      v = stateC->v[j];
      u = stateC->u[j];

#if GEOMETRY != SPHERICAL


#elif GEOMETRY == SPHERICAL


#endif  /* GEOMETRY == SPHERICAL */

    /* ----------------------------------------------------
       J3. Include Body force
       ---------------------------------------------------- */

      #if (BODY_FORCE & VECTOR)
      BodyForceVector(v, g, x1[i], x2[j], x3[k]);

      Et = u[ENG];
      #if RMHD_REDUCED_ENERGY == YES
      Et += u[RHO];
      #endif
      rhs[j][MX2] += dt*Et*g[JDIR];
      IF_ENERGY (rhs[j][ENG] += dt*u[MX2]*g[JDIR];)

      #if !INCLUDE_KDIR
      rhs[j][MX3] += dt*Et*g[KDIR];
      IF_ENERGY (rhs[j][ENG] += dt*u[MX3]*g[KDIR];)
      #endif  /* !INCLUDE_KDIR */
      #endif  /* (BODY_FORCE & VECTOR) */

    }

  }else if (g_dir == KDIR){

    scrh  = dt;
    #if GEOMETRY == SPHERICAL
    scrh *= dx2[j]/(rt[i]*dmu[j]);
    #endif

    for (k = beg; k <= end; k++) {
      dtdx = scrh/dx3[k];
      v    = stateC->v[k];
      u    = stateC->u[k];

    /* ----------------------------------------------------
       K3. Include body forces
       ---------------------------------------------------- */

      #if (BODY_FORCE & VECTOR)
      BodyForceVector(v, g, x1[i], x2[j], x3[k]);
      Et = u[ENG];
      #if RMHD_REDUCED_ENERGY == YES
      Et += u[RHO];
      #endif
      rhs[k][MX3] += dt*Et*g[KDIR];
      IF_ENERGY (rhs[k][ENG] += dt*u[MX3]*g[KDIR];)

      #if !INCLUDE_JDIR
      rhs[k][MX2] += dt*Et*g[JDIR];
      IF_ENERGY (rhs[k][ENG]   += dt*u[MX2]*g[JDIR];)
      #endif
      #endif  /* (BODY_FORCE & VECTOR) */
      
    }
  }

/* --------------------------------------------------
              Powell's source terms
   -------------------------------------------------- */

  #if (PHYSICS == RMHD) && (DIVB_CONTROL == EIGHT_WAVES)
  for (i = beg; i <= end; i++) {
    rhs[i][MX1] += dt*sweep->src[i][MX1];
    rhs[i][MX2] += dt*sweep->src[i][MX2];
    rhs[i][MX3] += dt*sweep->src[i][MX3];

    rhs[i][BX1] += dt*sweep->src[i][BX1];
    rhs[i][BX2] += dt*sweep->src[i][BX2];
    rhs[i][BX3] += dt*sweep->src[i][BX3];
    #if HAVE_ENERGY
    rhs[i][ENG] += dt*sweep->src[i][ENG];
    #endif
  }
  #endif

/* -------------------------------------------------
            Extended GLM source terms
   ------------------------------------------------- */

  #if (defined GLM_MHD) && (GLM_EXTENDED == YES)
   print ("! RightHandSide(): Extended GLM source terms not defined for RMHD\n");
   QUIT_PLUTO(1);
  #endif

}
