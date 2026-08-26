/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Compute the right hand side of the conservative 
         HD/MHD equations.

  This function constructs the one-dimensional right hand side of 
  the conservative MHD or HD equations in the direction given by ::g_dir 
  in different geometries.
  The right hand side in the \c d direction is computed as a two-point
  flux difference term plus a source term:  
  \f[
      {\cal R}_{\ivec}^{(d)} = 
      -\frac{\Delta t}{\Delta{\cal V}_{\ivec}}\
      \Big[  (A{\cal F})_{\ivec+\HALF\hvec{e}_d} 
            -(A{\cal F})_{\ivec-\HALF\hvec{e}_d} \Big]
         + \Delta t{\cal S}_{\ivec}
  \f] 
   where \f$\ivec = (i,j,k)\f$ while   
   - \f$ A        \f$: interface areas
   - \f$ {\cal V} \f$: cell volume
   - \f$ \F   \f$: interface fluxes
   - \f$ \Delta t \f$: time step
   - \f$ {\cal S} \f$: source term including geometrical terms and 
                       body forces.

  See also the \ref RHS_page.
  The right hand side is assembled through the following steps:
 
  - If either one of FARGO, ROTATION or gravitational potential is used, 
    fluxes are combined to enforce conservation of total angular momentum
    and/or energy, see TotalFlux();
  - initialize rhs with flux differences;

  Source terms are added later in RightHandSideSource().

  For the entropy equation, the dissipative contributions can be recovered
  from the internal energy equation, for which (Boyd, Eq. [3.27]): 
  \f[
      \pd{p}{t} + \vec{v}\cdot\nabla p + \Gamma p \nabla\cdot\vec{v}
    = (\Gamma-1)\left[\nabla\cdot\left(\kappa\nabla T\right)
      + \eta\vec{J}\cdot\vec{J} + \mu\left(\pd{v_i}{x_j}+\pd{v_j}{x_i}
        - \frac{2}{3}\delta_{ij}\nabla\cdot\vec{v}\right)\pd{v_i}{x_j}\right]
  \f] 
  To obtain the corresponding contribution to the (conservative form of)
  entropy equation, just divide the previous one by 
  \f$\rho^{\Gamma-1}\f$: 
  \f[
     \pd{(\rho s)}{t} + \nabla\cdot(\rho s\vec{v}) = 
     (\Gamma-1)\rho^{1-\Gamma}\left[\nabla\cdot\left(\kappa\nabla T\right)
       + \eta\vec{J}^2 + \mu \Pi_{ij}\pd{v_i}{x_j}\right]
  \f]
  See the books by
  - Goedbloed & Poedts, "Principle of MHD", page 165-166
  - Boyd, "The Physics of Plasmas", page 57, Eq. 3.27

  \b References
     - "PLUTO: A Numerical Code for Computational Astrophysics."
       Mignone et al, ApJS (2007) 170, 228
     - "The PLUTO Code for Adaptive Mesh Computations in Astrophysical Fluid
        Dynamics" 
       Mignone et al, ApJS (2012) 198, 7M
   
  \author A. Mignone (andrea.mignone@unito.it)
  \date   July 22, 2019

*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

static void TotalFlux (const Sweep *, double *, double **, int, int, Grid *);

#ifdef CH_SPACEDIM   /*  implies Chombo is being used  */
 #define USE_PRS_GRADIENT  YES   
#else
 #if (defined FINITE_DIFFERENCE) || (defined HIGH_ORDER)
  #define USE_PRS_GRADIENT  NO   /* -- default for Finite Difference schemes -- */
 #else
  #define USE_PRS_GRADIENT  YES   /* -- default, do not change!! -- */
 #endif
#endif

/* *********************************************************************** */
void RightHandSide (const Sweep *sweep, timeStep *Dts, 
                    int beg, int end, double dt, Grid *grid)
/*! 
 *
 * \param [in,out]  sweep  pointer to Sweep structure
 * \param [in]      Dts    pointer to time step structure
 * \param [in]      beg    initial index of computation
 * \param [in]      end    final   index of computation
 * \param [in]      dt     time increment
 * \param [in]      grid   pointer to Grid structure
 *
 * \return This function has no return value.
 * \note    --
 * \todo    --
 ************************************************************************* */
{
  int    i, j, k, nv;
  const State *stateC = &(sweep->stateC);
  const State *stateL = &(sweep->stateL);
  const State *stateR = &(sweep->stateR);

  double **rhs  = sweep->rhs;
  double **flux = sweep->flux;
  double *p     = sweep->press;

  double A, scrh, w;

  double *x1  = grid->x[IDIR],  *x2  = grid->x[JDIR],  *x3  = grid->x[KDIR];
  double *x1p = grid->xr[IDIR], *x2p = grid->xr[JDIR], *x3p = grid->xr[KDIR];
  double *x1m = grid->xl[IDIR], *x2m = grid->xl[JDIR], *x3m = grid->xl[KDIR];
  double *dx1 = grid->dx[IDIR], *dx2 = grid->dx[JDIR], *dx3 = grid->dx[KDIR];
  double *dx   = grid->dx[g_dir];
  #if GEOMETRY != CARTESIAN
  double ***dV = grid->dV;
  #if GEOMETRY == SPHERICAL
  double *s    = grid->s;
  #endif
  #endif
  double dtdV, dtdl;  
  static double **fA, *phi_p;

#ifdef FARGO
  double **wA = FARGO_Velocity();
#endif

/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */

#if GEOMETRY != CARTESIAN
  if (fA == NULL) {
    fA   = ARRAY_2D(NMAX_POINT, NVAR, double);
  }
#endif
  if (phi_p == NULL) phi_p = ARRAY_1D(NMAX_POINT, double);

/* --------------------------------------------------------
   1. Compute fluxes for dust
   -------------------------------------------------------- */

#if DUST_FLUID == YES
  DustFluid_Solver(sweep, beg - 1, end, Dts->cmax, grid);
#endif

  i = g_i;  /* will be redefined during x1-sweep */
  j = g_j;  /* will be redefined during x2-sweep */
  k = g_k;  /* will be redefined during x3-sweep */
  
/* --------------------------------------------------------
   2. Add pressure to normal component of 
      momentum flux if necessary.
   -------------------------------------------------------- */

#ifndef HIGH_ORDER
#if USE_PRS_GRADIENT == NO
  for (i = beg - 1; i <= end; i++) flux[i][MXn] += p[i];
#endif
#endif

/* --------------------------------------------------------
   3. Compute gravitational potential at cell interfaces
      This step must be done before calling TotalFlux.
   --------------------------------------------------------  */

#if (BODY_FORCE & POTENTIAL)
  if (g_dir == IDIR) {
    for (i = beg-1; i <= end; i++) {
      phi_p[i] = BodyForcePotential(x1p[i], x2[j], x3[k]);
    }
  }else if (g_dir == JDIR){
    for (j = beg-1; j <= end; j++) {
      phi_p[j] = BodyForcePotential(x1[i], x2p[j], x3[k]);
    }
  }else if (g_dir == KDIR){
    for (k = beg-1; k <= end; k++) {
      phi_p[k] = BodyForcePotential(x1[i], x2[j], x3p[k]);
    }
  }
#endif
  
/* --------------------------------------------------------
   4. Compute total flux
   -------------------------------------------------------- */

  TotalFlux(sweep, phi_p, fA, beg-1, end, grid);

/* --------------------------------------------------------
   5. Compute right hand side
   -------------------------------------------------------- */

#if GEOMETRY == CARTESIAN
  for (i = beg; i <= end; i++) {
    scrh = dt/dx[i];

    #if RADIATION
    NMHD_LOOP(nv) rhs[i][nv] = -scrh*(flux[i][nv] - flux[i-1][nv]);
    #if IRRADIATION
    rhs[i][FIR] = 0.;
    #endif
    #else
    NVAR_LOOP(nv) rhs[i][nv] = -scrh*(flux[i][nv] - flux[i-1][nv]);
    #endif
    #if USE_PRS_GRADIENT == YES
    rhs[i][MXn] -= scrh*(p[i] - p[i-1]);
    #endif
  }

  #if (defined FARGO && !defined SHEARINGBOX)
  i = g_i;  /* will be redefined during x1-sweep */
  j = g_j;  /* will be redefined during x2-sweep */
  k = g_k;  /* will be redefined during x3-sweep */

  if (g_dir == IDIR){
    for (i = beg; i <= end; i++) {
      w = wA[k][i];
      rhs[i][MX2] -= w*rhs[i][RHO];
      IF_ENERGY(rhs[i][ENG] -= w*(rhs[i][MX2] + 0.5*w*rhs[i][RHO]);)
    }
  }

  if (g_dir == KDIR){
    for (k = beg; k <= end; k++) {
      w = wA[k][i];
      rhs[k][MX2] -= w*rhs[k][RHO];
      IF_ENERGY(rhs[k][ENG] -= w*(rhs[k][MX2] + 0.5*w*rhs[k][RHO]);)
    }
  }
  #endif

#else
  if (g_dir == IDIR){
    double *vg;
    double q;
    for (i = beg; i <= end; i++){

     vg = stateC->v[i];

    /* ------------------------------------------
       I1. Build rhs in the x1-dir
       ------------------------------------------ */
           
      dtdV = dt/dV[k][j][i];
      dtdl = dt/dx1[i];

      #if RADIATION
      NMHD_LOOP(nv) rhs[i][nv] = -dtdV*(fA[i][nv] - fA[i-1][nv]); 
      #if IRRADIATION
      rhs[i][FIR] = 0.;
      #endif
      #else
      NVAR_LOOP(nv) rhs[i][nv] = -dtdV*(fA[i][nv] - fA[i-1][nv]);
      #endif

      #if USE_PRS_GRADIENT == YES
      rhs[i][MXn] -= dtdl*(p[i] - p[i-1]);
      #endif

      #ifdef GLM_MHD
      rhs[i][BXn] = -dtdl*(flux[i][BXn] - flux[i-1][BXn]);
      #endif

#ifndef HIGH_ORDER
      rhs[i][iMPHI] /= fabs(x1[i]);
      IF_DUST_FLUID(rhs[i][iMPHI_D] /= fabs(x1[i]);)

      #if PHYSICS == MHD 
      #if (GEOMETRY == POLAR) || (GEOMETRY == CYLINDRICAL) 
      rhs[i][iBPHI] = -dtdl*(fA[i][iBPHI] - fA[i-1][iBPHI]);
      #elif GEOMETRY == SPHERICAL 
      q = dtdl/x1[i];
      rhs[i][iBTH]  = -q*(fA[i][iBTH]  - fA[i-1][iBTH]);
      rhs[i][iBPHI] = -q*(fA[i][iBPHI] - fA[i-1][iBPHI]);
      #endif
      #endif /* PHYSICS == MHD */
#endif

    /* ------------------------------------------
       I2. Modify rhs to enforce conservation
       ------------------------------------------ */

      #if (defined FARGO) || (ROTATING_FRAME == YES)
      w = 0.0;
      #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
      IF_FARGO         (w += wA[k][i];)
      IF_ROTATING_FRAME(w += g_OmegaZ*x1[i];)
      #elif GEOMETRY == SPHERICAL
      IF_FARGO         (w += wA[j][i];)
      IF_ROTATING_FRAME(w += g_OmegaZ*x1[i]*s[j];)
      #endif

      rhs[i][iMPHI] -= w*rhs[i][RHO];
      IF_ENERGY(rhs[i][ENG] -= w*(rhs[i][iMPHI] + 0.5*w*rhs[i][RHO]);)
      #endif /* (defined FARGO) || (ROTATING_FRAME == YES) */
  
    }  

  }else if (g_dir == JDIR){
    
    double **dx_dl = grid->dx_dl[JDIR];
    for (j = beg; j <= end; j++){

    /* ------------------------------------------
       J1. Build rhs in the x2-dir
       ------------------------------------------ */

      dtdV = dt/dV[k][j][i];
      dtdl = dt/dx2[j]*dx_dl[j][i];
      
      #if RADIATION
      NMHD_LOOP(nv) rhs[j][nv] = -dtdV*(fA[j][nv] - fA[j-1][nv]);
      #if IRRADIATION
      rhs[j][FIR] = 0.;
      #endif
      #else
      NVAR_LOOP(nv) rhs[j][nv] = -dtdV*(fA[j][nv] - fA[j-1][nv]);
      #endif

      #if USE_PRS_GRADIENT == YES
      rhs[j][MXn] -= dtdl*(p[j] - p[j-1]);
      #endif

      #ifdef GLM_MHD
      rhs[j][BXn] = -dtdl*(flux[j][BXn] - flux[j-1][BXn]);
      #endif

      #if GEOMETRY == SPHERICAL
#ifndef HIGH_ORDER
      rhs[j][iMPHI] /= fabs(s[j]);
      IF_DUST_FLUID (rhs[j][iMPHI_D] /= fabs(s[j]);)
      #if PHYSICS == MHD
      rhs[j][iBPHI] = -dtdl*(fA[j][iBPHI] - fA[j-1][iBPHI]);
      #endif  /* PHYSICS == MHD */
#endif

    /* ----------------------------------------------------
       J2. Modify rhs to enforce conservation
       ---------------------------------------------------- */

      #if (defined FARGO) || (ROTATING_FRAME == YES)
      w = 0.0; 
      IF_FARGO         (w += wA[j][i];)
      IF_ROTATING_FRAME(w += g_OmegaZ*x1[i]*s[j];)
      rhs[j][MX3] -= w*rhs[j][RHO];
      IF_ENERGY(rhs[j][ENG]  -= w*(rhs[j][MX3] + 0.5*w*rhs[j][RHO]);)
      #endif

      #endif /* GEOMETRY == SPHERICAL  */
    }

  }else if (g_dir == KDIR){

    double **dx_dl = grid->dx_dl[KDIR];
    for (k = beg; k <= end; k++){

    /* ------------------------------------------
       K1. Build rhs in the x3-dir
       ------------------------------------------ */

      dtdV = dt/dV[k][j][i];
      dtdl = dt/dx3[k]*dx_dl[j][i];

      #if RADIATION
      NMHD_LOOP(nv) rhs[k][nv] = -dtdV*(fA[k][nv] - fA[k-1][nv]);
      #if IRRADIATION
      rhs[k][FIR] = 0.;
      #endif
      #else
      NVAR_LOOP(nv) rhs[k][nv] = -dtdV*(fA[k][nv] - fA[k-1][nv]);
      #endif

      #if USE_PRS_GRADIENT == YES
      rhs[k][MXn] -= dtdl*(p[k] - p[k-1]);
      #endif

      #ifdef GLM_MHD
      rhs[k][BXn] = -dtdl*(flux[k][BXn] - flux[k-1][BXn]);
      #endif

    /* ------------------------------------------------------
       K2. modify rhs to enforce conservation (FARGO only)
           (solid body rotations are not included the
            velocity depends on the cylindrical radius only)
       ------------------------------------------------------ */

      #if (GEOMETRY == POLAR) && (defined FARGO)
      w = wA[k][i];
      rhs[k][MX2] -= w*rhs[k][RHO];
      IF_ENERGY(rhs[k][ENG] -= w*(rhs[k][MX2] + 0.5*w*rhs[k][RHO]);)
      #endif

    }
  }
#endif  /* GEOMETRY == CARTESIAN */

/* --------------------------------------------------------
   6. Add source terms
   -------------------------------------------------------- */

#ifndef HIGH_ORDER
  RightHandSideSource (sweep, Dts, beg, end, dt, phi_p, grid);
#endif

#if SCALAR_ADVECTION == YES
  for (i = beg; i <= end; i++){
    for (nv = 1; nv < NVAR; nv++) sweep->rhs[i][nv] = 0.0;
  }
#endif

/* --------------------------------------------------------
   7. Reset right hand side in internal boundary zones
   -------------------------------------------------------- */
   
#if INTERNAL_BOUNDARY == YES
  InternalBoundaryReset(sweep, Dts, beg, end, grid);
#endif

}

/* ********************************************************************* */
void TotalFlux (const Sweep *sweep, double *phi_p, double **fA,
                int beg, int end, Grid *grid)
/*!
 * Compute the total flux in order to enforce conservation of 
 * angular momentum and energy in presence of FARGO source 
 * terms, rotation or gravitational potential.
 *
 * In Cartesian coordinates, for instance, we modify the energy and
 * the y-momentum flux as:
 * \f{eqnarray*}{
 *    \F^{E}_{i+\HALF} &\quad \to\quad&
 *    \F^{E}_{i+\HALF} + w_{i+\HALF}\left(\HALF w_{i+\HALF}\F^{\rho}_{i+\HALF}
 *     + \F^{m_y}_{i+\HALF}\right) +  \Phi_{i+\HALF}\F^{\rho}_{i+\HALF}                           
 *    \\ \noalign{\medskip}
 *    \F^{m_y}_{i+\HALF} &\quad \to\quad&
 *    \F^{m_y}_{i+\HALF} + w_{i+\HALF}\F^{\rho}_{i+\HALF}
 * \f}
 * where \f$w\f$ is the average orbital velocity computed during the
 * FARGO algorithm and \f$\Phi\f$ is the gravitational potential.
 * This is described in Mignone et al. (A\&A 2012), see appendix there.
 *
 * \note This function does not change the fluxes when the Shearing-box
 *       and FARGO modules are both enabled.
 *       Instead, we incorporate the source terms elsewhere.
 * 
 *  
 * \param [in]     sweep  pointer to Sweep structure;
 * \param [in,out] phi_p  1D array defining the gravitational potential
 *                        at grid interfaces;
 * \param [out]    fA     Total flux;
 * \param [in]     beg    initial index of computation; 
 * \param [in]     end    final   index of computation;
 * \param [in]     grid   pointer to Grid structure;
 *
 * \b Reference
 *    - "A conservative orbital advection scheme for simulations of magnetized
 *        shear flows with the PLUTO code"
 *        Mignone et al., A\&A (2012) 545A, 152M
 *
 *********************************************************************** */
#ifndef iMPHI
 #define iMPHI MX2  /* -- for Cartesian coordinates -- */
#endif
{
  int    i,j,k,nv;
  double wp, R, A;
  double **flux;
  double *x1,  *x2,  *x3;
  double *x1p, *x2p, *x3p;
#ifdef FARGO
  double **wA = FARGO_Velocity();
#endif
#if DUST_FLUID == YES
  #if (defined FARGO) || (defined SHEARINGBOX) || (ROTATING_FRAME == YES)
    #error DUST_FLUID not yet compatibile with either FARGO/SHEARINGBOX/ROT. FRAME
  #endif  
#endif

/* --------------------------------------------------------
   0. Set pointer shortcuts
   -------------------------------------------------------- */

  flux = sweep->flux;
  x1  = grid->x[IDIR]; x1p = grid->xr[IDIR];
  x2  = grid->x[JDIR]; x2p = grid->xr[JDIR];
  x3  = grid->x[KDIR]; x3p = grid->xr[KDIR];

  i = g_i;  /* will be redefined during x1-sweep */
  j = g_j;  /* will be redefined during x2-sweep */
  k = g_k;  /* will be redefined during x3-sweep */

/* --------------------------------------------------------
   1. Compute total flux for the X1-Sweep
   -------------------------------------------------------- */

  if (g_dir == IDIR){ 

    for (i = beg; i <= end; i++){

    /* ------------------------------------------------------
       1a. include flux contributions from FARGO or Rotation 
           Note: ShearingBox terms are not included here but
                 only within the BodyForce() function.
       ------------------------------------------------------ */

      #if (defined FARGO && !defined SHEARINGBOX) || (ROTATING_FRAME == YES)
      wp = 0.0;
      #if GEOMETRY == SPHERICAL
      IF_FARGO(wp = 0.5*(wA[j][i] + wA[j][i+1]);)
      R = x1p[i]*sin(x2[j]);  /* -- cylindrical radius -- */
      #else
      IF_FARGO(wp = 0.5*(wA[k][i] + wA[k][i+1]);)
      R = x1p[i];                   /* -- cylindrical radius -- */
      #endif
      IF_ROTATING_FRAME(wp += g_OmegaZ*R;)
      IF_ENERGY  (flux[i][ENG] += wp*(0.5*wp*flux[i][RHO] + flux[i][iMPHI]);)
      flux[i][iMPHI] += wp*flux[i][RHO];
      #endif
      
    /* -- 1b. Add gravitational potential -- */

      #if (BODY_FORCE & POTENTIAL) && HAVE_ENERGY
      flux[i][ENG] += flux[i][RHO]*phi_p[i];                          
      #endif

    /* -- 1c. Multiply flux x area (non-Cartesian geometries only) -- */
    
      #if GEOMETRY != CARTESIAN      
      A  = grid->A[IDIR][k][j][i];
      NVAR_LOOP(nv) fA[i][nv] = flux[i][nv]*A;

#ifndef HIGH_ORDER
      #if    (GEOMETRY == POLAR) || (GEOMETRY == CYLINDRICAL)
      fA[i][iMPHI] *= fabs(x1p[i]);
      IF_DUST_FLUID(fA[i][iMPHI_D] *= fabs(x1p[i]);)
      #if PHYSICS == MHD
      fA[i][iBPHI] = flux[i][iBPHI];
      #endif
      #endif  /* GEOMETRY != CARTESIAN */
#endif /* HIGH_ORDER */

      #if GEOMETRY == SPHERICAL
      fA[i][iMPHI] *= fabs(x1p[i]);
      IF_DUST_FLUID(fA[i][iMPHI_D] *= fabs(x1p[i]);)
      #if PHYSICS == MHD
      fA[i][iBTH]  = flux[i][iBTH]*x1p[i];  
      fA[i][iBPHI] = flux[i][iBPHI]*x1p[i];
      #endif
      #endif
      #endif /* GEOMETRY != CARTESIAN */
    }
    
/* --------------------------------------------------------
   2. Compute total flux for the X2-Sweep
   -------------------------------------------------------- */

  }else if (g_dir == JDIR){
    for (j = beg; j <= end; j++){ 

    /* ----------------------------------------------------
       2a. include flux contributions from FARGO and
           Rotation 
       ---------------------------------------------------- */

      #if GEOMETRY == SPHERICAL
      #if (defined FARGO) || (ROTATING_FRAME == YES)
      wp = 0.0;
      R  = x1[i]*sin(x2p[j]);
      IF_FARGO   (wp += 0.5*(wA[j][i] + wA[j+1][i]);)
      IF_ROTATING_FRAME(wp += g_OmegaZ*R;)
      IF_ENERGY  (flux[j][ENG] += wp*(0.5*wp*flux[j][RHO] + flux[j][iMPHI]);)
      flux[j][iMPHI] += wp*flux[j][RHO];
      #endif
      #endif

    /* -- 2b. Add gravitational potential -- */

      #if (BODY_FORCE & POTENTIAL) && HAVE_ENERGY
      flux[j][ENG] += flux[j][RHO]*phi_p[j];
      #endif      

    /* -- 2c. Multiply flux x area (non-Cartesian geometries only) -- */
      
      #if GEOMETRY != CARTESIAN      
      A  = grid->A[JDIR][k][j][i];
      NVAR_LOOP(nv) fA[j][nv] = flux[j][nv]*A;
#ifndef HIGH_ORDER
      #if GEOMETRY == SPHERICAL
      double sp = grid->sp[j];
      fA[j][iMPHI] *= fabs(sp);
      IF_DUST_FLUID(fA[j][iMPHI_D] *= fabs(sp);)
      #if PHYSICS == MHD
      fA[j][iBPHI] = flux[j][iBPHI];      
      #endif
      #endif
#endif /* HIGH_ORDER */
      #endif
    }

/* --------------------------------------------------------
   3. Compute total flux for the X3-Sweep
   -------------------------------------------------------- */

  }else if (g_dir == KDIR){
    for (k = beg; k <= end; k++) {

    /* ----------------------------------------------------
       3a. include flux contributions from FARGO
           (polar/cartesian geometries only)
       ---------------------------------------------------- */

      #if (GEOMETRY != SPHERICAL) && (defined FARGO && !defined SHEARINGBOX)
      wp = 0.5*(wA[k][i] + wA[k+1][i]);
      IF_ENERGY(flux[k][ENG] += wp*(0.5*wp*flux[k][RHO] + flux[k][iMPHI]);)
      flux[k][iMPHI] += wp*flux[k][RHO];
      #endif
      
      #if (BODY_FORCE & POTENTIAL) && HAVE_ENERGY
      flux[k][ENG] += flux[k][RHO]*phi_p[k];
      #endif
      
      #if GEOMETRY != CARTESIAN      
      A  = grid->A[KDIR][k][j][i];
      NVAR_LOOP(nv) fA[k][nv] = flux[k][nv]*A;
      #endif
    }
  }
}
