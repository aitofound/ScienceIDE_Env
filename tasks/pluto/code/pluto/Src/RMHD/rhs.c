#include "pluto.h"

#ifdef CH_SPACEDIM   /*  implies Chombo is being used  */
 #define USE_PR_GRADIENT  YES   
#else
 #ifdef FINITE_DIFFERENCE 
  #define USE_PR_GRADIENT  NO   /* -- default for Finite Difference schemes -- */
 #else
  #define USE_PR_GRADIENT  YES   /* -- default, do not change!! -- */
 #endif
#endif

/* *********************************************************** */
void RightHandSide (const Sweep *sweep, timeStep *Dts, 
                    int beg, int end, double dt, Grid *grid)
/* 
 *   Compute right hand side of the MHD equations in different geometries,
 *   taking contributions from one direction at a time. 
 *   The right hand side (rhs) consists of the following contributions:
 * 
 *    rhs = dt/dV * (Ap*Fp - Am*Fm) + dt*S
 * 
 *   where 
 *
 *    Ap, Am:  interface areas, 
 *    Fp, Fm:  interface fluxes,
 *    dt:      time step
 *    S:       source term including 
 *             * geometrical source terms
 *             * gravity
 *  
 *   In order to compute rhs, this function takes the following steps:
 *
 *    - initialize rhs with flux differences  (#1)
 *    - add geometrical source terms          (#2)
 *    - add gravity                           (#4)
 *
 * LAST MODIFIED
 * 
 *   Apr 29, 2020 by A. Mignone (andrea.mignone@unito.it)
 *
 ************************************************************* */
{
  int    i, j, k, nv;

  const State *stateC = &(sweep->stateC);

  double dtdx, dtdV, scrh, dV1, dmu;
  double *x1  = grid->x[IDIR],  *x2  = grid->x[JDIR],  *x3  = grid->x[KDIR];
  double *x1p = grid->xr[IDIR], *x2p = grid->xr[JDIR], *x3p = grid->xr[KDIR];
  double *x1m = grid->xl[IDIR], *x2m = grid->xl[JDIR], *x3m = grid->xl[KDIR];
  double *dx1 = grid->dx[IDIR], *dx2 = grid->dx[JDIR], *dx3 = grid->dx[KDIR];

  double **flux, **rhs, *p, *v;
  double cl;
  double g[3];
  static double **fA, *h;
  
#if GEOMETRY != CARTESIAN
  if (fA == NULL) {
    fA = ARRAY_2D(NMAX_POINT, NVAR, double);
    h  = ARRAY_1D(NMAX_POINT, double);
  }
#endif

/* --------------------------
      pointer shortcuts
   -------------------------- */

  rhs  = sweep->rhs;
  flux = sweep->flux;
  p    = sweep->press;
  
  i = g_i;  /* will be redefined during x1-sweep */
  j = g_j;  /* will be redefined during x2-sweep */
  k = g_k;  /* will be redefined during x3-sweep */

/* ------------------------------------------------
     Add pressure to normal component of 
     momentum flux if necessary.
   ------------------------------------------------ */

  #if USE_PR_GRADIENT == NO
   for (i = beg - 1; i <= end; i++) flux[i][MXn] += p[i];
  #endif

#if GEOMETRY == CARTESIAN
/* ***********************************************************
   
        Compute right-hand side of the RMHD/RHD 
        equations in CARTESIAN geometry.
    
   *********************************************************** */
{
  double x, y, z;

  if (g_dir == IDIR){

  /* ****************************************************
      Cartesian x-direction,

       - initialize rhs with flux differences (I1)
       - enforce conservation of total angular
         momentum and/or energy               (I3)
       - add gravity                          (I4)
     **************************************************** */

    y = x2[j];
    z = x3[k];
    for (i = beg; i <= end; i++) {
      x    = x1[i];
      dtdx = dt/dx1[i];

    /* -----------------------------------------------
       I1. initialize rhs with flux difference
       ----------------------------------------------- */

      NVAR_LOOP(nv) rhs[i][nv] = -dtdx*(flux[i][nv] - flux[i-1][nv]);
      #if USE_PR_GRADIENT == YES
      rhs[i][MX1] -= dtdx*(p[i] - p[i-1]);
      #endif
      #if IRRADIATION
      rhs[i][FIR] = 0.;
      #endif

    }
  } else if (g_dir == JDIR){

  /* ****************************************************
      Cartesian y-direction,

       - initialize rhs with flux differences (J1)
       - add gravity                          (J4)
     **************************************************** */

    x = x1[i];
    z = x3[k];
    for (j = beg; j <= end; j++) {
      y    = x2[j];
      dtdx = dt/dx2[j];

    /* -----------------------------------------------
       J1. initialize rhs with flux difference
       ----------------------------------------------- */

      NVAR_LOOP(nv) rhs[j][nv] = -dtdx*(flux[j][nv] - flux[j-1][nv]);
      #if USE_PR_GRADIENT == YES
      rhs[j][MX2] -= dtdx*(p[j] - p[j-1]);
      #endif
      #if IRRADIATION
      rhs[j][FIR] = 0.;
      #endif

    }

  }else if (g_dir == KDIR){

  /* ****************************************************
      Cartesian z-direction,

       - initialize rhs with flux differences (K1)
       - enforce conservation of total angular
         momentum and/or energy               (K3)
       - add gravity                          (K4)
     **************************************************** */

    x = x1[i];
    y = x2[j];
    for (k = beg; k <= end; k++) {
      z    = x3[k];
      dtdx = dt/dx3[k];

    /* -----------------------------------------------
       K1. initialize rhs with flux difference
       ----------------------------------------------- */

      NVAR_LOOP(nv) rhs[k][nv] = -dtdx*(flux[k][nv] - flux[k-1][nv]);
      #if USE_PR_GRADIENT == YES
       rhs[k][MX3] -= dtdx*(p[k] - p[k-1]);
      #endif
      #if IRRADIATION
      rhs[k][FIR] = 0.;
      #endif
    }
  }
}
#elif GEOMETRY == CYLINDRICAL

/* ***********************************************************
   
        Compute right-hand side of the RMHD/RHD 
        equations in CYLINDRICAL geometry.
    
   *********************************************************** */
{
  double R, z, phi; 
  double R1, R_1;
  double vB, vel2, lor2, wt, mphi, B2;

  #if RADIATION
  double Sr;
  #endif

  if (g_dir == IDIR) {  

  /* ****************************************************
      Cylindrical radial direction:
      multiply fluxes times interface area
     **************************************************** */

    z   = x2[g_j];
    phi = 0.0;
    for (i = beg - 1; i <= end; i++){ 
      R = fabs(x1p[i]);

      fA[i][RHO]   = flux[i][RHO]*R;
      fA[i][iMR]   = flux[i][iMR]*R;    
      fA[i][iMZ]   = flux[i][iMZ]*R;    
      fA[i][iMPHI] = flux[i][iMPHI]*R*R;
      #if PHYSICS == RMHD
      fA[i][iBR]   = flux[i][iBR]*R;
      fA[i][iBZ]   = flux[i][iBZ]*R;
      fA[i][iBPHI] = flux[i][iBPHI]*R;
      #endif
      #if HAVE_ENERGY
      fA[i][ENG] = flux[i][ENG]*R;
      #endif
      #ifdef GLM_MHD
      fA[i][PSI_GLM] = flux[i][PSI_GLM]*R;
      #endif
      #if RADIATION
      fA[i][ENR]    = flux[i][ENR]*R;
      fA[i][iFRR]   = flux[i][iFRR]*R;
      fA[i][iFRZ]   = flux[i][iFRZ]*R;
      fA[i][iFRPHI] = flux[i][iFRPHI]*R*R; 
      #endif
      NSCL_LOOP(nv) fA[i][nv] = flux[i][nv]*R;
    }

  /* ****************************************************
      Cylindrical radial direction,

       - initialize rhs with flux differences (I1)
       - add source terms                     (I2)
       - add gravity                          (I4)
     **************************************************** */

    Enthalpy (stateC->v, h, beg, end);
    for (i = beg; i <= end; i++){ 
      R    = x1[i];
      dtdV = dt/(fabs(x1[i])*dx1[i]);
      dtdx = dt/dx1[i];
      R_1  = 1.0/R;

    /* -----------------------------------------------
       I1. initialize rhs with flux difference
       ----------------------------------------------- */

      rhs[i][RHO]   = - dtdV*(fA[i][RHO] - fA[i-1][RHO]);
      rhs[i][iMR]   = - dtdV*(fA[i][iMR]   - fA[i-1][iMR]);
      rhs[i][iMZ]   = - dtdV*(fA[i][iMZ]   - fA[i-1][iMZ]);
      rhs[i][iMPHI] = - dtdV*(fA[i][iMPHI] - fA[i-1][iMPHI])*fabs(R_1);
      #if USE_PR_GRADIENT == YES
      rhs[i][iMR] -= dtdx*(p[i] - p[i-1]);  
      #endif
      #if PHYSICS == RMHD
      rhs[i][iBR]   = - dtdV*(fA[i][iBR]   - fA[i-1][iBR]);
      rhs[i][iBZ]   = - dtdV*(fA[i][iBZ]   - fA[i-1][iBZ]);
      rhs[i][iBPHI] = - dtdV*(fA[i][iBPHI] - fA[i-1][iBPHI]);
      #ifdef GLM_MHD
      rhs[i][iBR]     = - dtdx*(flux[i][iBR]   - flux[i-1][iBR]);
      rhs[i][PSI_GLM] = - dtdV*(fA[i][PSI_GLM] - fA[i-1][PSI_GLM]);
      #endif
      #endif /* PHYSICS == RMHD */
      #if HAVE_ENERGY
      rhs[i][ENG] = -dtdV*(fA[i][ENG] - fA[i-1][ENG]);
      #endif
      #if RADIATION
      rhs[i][ENR]    = - dtdV*(fA[i][ENR]    - fA[i-1][ENR]);
      rhs[i][iFRR]   = - dtdV*(fA[i][iFRR]   - fA[i-1][iFRR]);
      rhs[i][iFRZ]   = - dtdV*(fA[i][iFRZ]   - fA[i-1][iFRZ]); 
      rhs[i][iFRPHI] = - dtdV*(fA[i][iFRPHI] - fA[i-1][iFRPHI])*fabs(R_1);
      #if IRRADIATION
      rhs[i][FIR] = 0.;
      #endif
      #endif
      NSCL_LOOP(nv) rhs[i][nv] = -dtdV*(fA[i][nv] - fA[i-1][nv]);
      
    /* -------------------------------------------------------
       I2. Add source terms
           [for Bphi we use the non-conservative formulation
            with the source term since it has been found to 
            be more stable at low resolution in toroidal jet
            simulations]
       ------------------------------------------------------- */

      v     = stateC->v[i];
      vel2  = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
      lor2  = 1.0/(1.0 - vel2);
      #if PHYSICS == RMHD
      vB   = v[VX1]*v[BX1] + v[VX2]*v[BX2] + v[VX3]*v[BX3];
      B2   = v[BX1]*v[BX1] + v[BX2]*v[BX2] + v[BX3]*v[BX3];
      wt   = v[RHO]*h[i]*lor2 + B2;
      mphi = wt*v[iVPHI] - vB*v[iBPHI]; 
      #elif PHYSICS == RHD
      wt   = v[RHO]*h[i]*lor2;
      mphi = wt*v[iVPHI]; 
      #endif       
      
      rhs[i][iMR] += dt*mphi*v[iVPHI]*R_1;
      #if PHYSICS == RMHD
      rhs[i][iMR]   -= dt*(v[iBPHI]/lor2 + vB*v[iVPHI])*v[iBPHI]*R_1;
      rhs[i][iBPHI] -= dt*(v[iVPHI]*v[iBR] - v[iBPHI]*v[iVR])*R_1;
      #endif

      #if RADIATION
      Sr = EddTensor(v,iFRPHI,iFRPHI) * v[ENR];
      rhs[i][iFRR] += dt*Sr*R_1;
      #endif

    }
     
  } else if (g_dir == JDIR) { 

  /* ****************************************************
      Cylindrical vertical direction:

       - initialize rhs with flux differences (J1)
       - add gravity                          (J4)
     **************************************************** */

    R   = x1[i];
    phi = 0.0;
    for (j = beg; j <= end; j++){ 
      z    = x2[j];   
      dtdx = dt/dx2[j];

    /* -----------------------------------------------
       J1. initialize rhs with flux difference
       ----------------------------------------------- */

      NVAR_LOOP(nv) rhs[j][nv] = -dtdx*(flux[j][nv] - flux[j-1][nv]);
      #if USE_PR_GRADIENT == YES
      rhs[j][iMZ] += - dtdx*(p[j] - p[j-1]);
      #endif
      #if IRRADIATION
      rhs[j][FIR] = 0.;
      #endif

    }
  }
}

#elif GEOMETRY == POLAR

/* ***********************************************************
   
        Compute right-hand side of the RMHD/RHD 
        equations in POLAR geometry.
    
   *********************************************************** */
{
  double R, z, phi; 
  double R1, R_1;
  double vB, vel2, lor2, wt, mphi, B2;

  #if RADIATION
  double frad2, Sr;
  #endif
   
  if (g_dir == IDIR) { 

  /* ****************************************************
      Polar radial direction:
      multiply fluxes times interface area
     **************************************************** */

    phi = x2[j];
    z   = x3[k];
    for (i = beg - 1; i <= end; i++) { 
      R = fabs(x1p[i]);

      fA[i][RHO]   = flux[i][RHO]*R;
      fA[i][iMR]   = flux[i][iMR]*R; 
      fA[i][iMPHI] = flux[i][iMPHI]*R*R;
      fA[i][iMZ]   = flux[i][iMZ]*R;      
      #if PHYSICS == RMHD
      fA[i][iBR]   = flux[i][iBR]*R;
      fA[i][iBPHI] = flux[i][iBPHI]*R;
      fA[i][iBZ]   = flux[i][iBZ]*R;
      #endif
      #if HAVE_ENERGY
      fA[i][ENG] = flux[i][ENG]*R;
      #endif
      #ifdef GLM_MHD
      fA[i][PSI_GLM] = flux[i][PSI_GLM]*R;
      #endif
      #if RADIATION
      fA[i][ENR]    = flux[i][ENR]*R;
      fA[i][iFRR]   = flux[i][iFRR]*R; 
      fA[i][iFRPHI] = flux[i][iFRPHI]*R*R;
      fA[i][iFRZ]   = flux[i][iFRZ]*R;
      #endif
      NSCL_LOOP(nv) fA[i][nv] = flux[i][nv]*R;
    }

  /* ****************************************************
      Polar radial direction,

       - initialize rhs with flux differences (I1)
       - add source terms                     (I2)
       - enforce conservation of total angular
         momentum and/or energy               (I3)
       - add gravity                          (I4)
     **************************************************** */

    Enthalpy (stateC->v, h, beg, end);
    for (i = beg; i <= end; i++) {
      R    = x1[i];
      dtdV = dt/(fabs(x1[i])*dx1[i]);
      dtdx = dt/dx1[i];
      R_1  = 1.0/x1[i];

    /* -----------------------------------------------
       I1. initialize rhs with flux difference
       ----------------------------------------------- */

      rhs[i][RHO]   = - dtdV*(fA[i][RHO]   - fA[i-1][RHO]);
      rhs[i][iMR]   = - dtdV*(fA[i][iMR]   - fA[i-1][iMR]); 
      rhs[i][iMPHI] = - dtdV*(fA[i][iMPHI] - fA[i-1][iMPHI])*fabs(R_1);
      rhs[i][iMZ]   = - dtdV*(fA[i][iMZ]   - fA[i-1][iMZ]);
      #if USE_PR_GRADIENT == YES
      rhs[i][iMR] -= dtdx*(p[i] - p[i-1]);  
      #endif
      #if PHYSICS == RMHD
      rhs[i][iBR]   = -dtdV*(fA[i][iBR]   - fA[i-1][iBR]); 
      rhs[i][iBPHI] = -dtdV*(fA[i][iBPHI] - fA[i-1][iBPHI]);
      rhs[i][iBZ]   = -dtdV*(fA[i][iBZ]   - fA[i-1][iBZ]);
      #ifdef GLM_MHD
      rhs[i][iBR]     = -dtdx*(flux[i][iBR]   - flux[i-1][iBR]);
      rhs[i][PSI_GLM] = -dtdV*(fA[i][PSI_GLM] - fA[i-1][PSI_GLM]);
      #endif
      #endif /* PHYSICS == RMHD */ 
      #if HAVE_ENERGY
      rhs[i][ENG] = -dtdV*(fA[i][ENG] - fA[i-1][ENG]);
      #endif 
      #if RADIATION
      rhs[i][ENR]    = - dtdV*(fA[i][ENR]    - fA[i-1][ENR]);
      rhs[i][iFRR]   = - dtdV*(fA[i][iFRR]   - fA[i-1][iFRR]);
      rhs[i][iFRPHI] = - dtdV*(fA[i][iFRPHI] - fA[i-1][iFRPHI])*fabs(R_1);
      rhs[i][iFRZ]   = - dtdV*(fA[i][iFRZ]   - fA[i-1][iFRZ]);
      #if IRRADIATION
      rhs[i][FIR] = 0.;
      #endif
      #endif
      NSCL_LOOP(nv)  rhs[i][nv] = -dtdV*(fA[i][nv] - fA[i-1][nv]);

    /* -------------------------------------------------------
       I2. Add source terms
           [for Bphi we use the non-conservative formulation
            with the source term since it has been found to 
            be more stable at low resolution in toroidal jet
            simulations]
       ------------------------------------------------------- */

      v     = stateC->v[i];
      vel2  = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
      lor2  = 1.0/(1.0 - vel2);
      #if PHYSICS == RMHD
      vB   = v[VX1]*v[BX1] + v[VX2]*v[BX2] + v[VX3]*v[BX3];
      B2   = v[BX1]*v[BX1] + v[BX2]*v[BX2] + v[BX3]*v[BX3];
      wt   = v[RHO]*h[i]*lor2 + B2;
      mphi = wt*v[iVPHI] - vB*v[iBPHI]; 
      #elif PHYSICS == RHD
      wt   = v[RHO]*h[i]*lor2;
      mphi = wt*v[iVPHI]; 
      #endif       
      
      rhs[i][iMR] += dt*mphi*v[iVPHI]*R_1;
      #if PHYSICS == RMHD
      rhs[i][iMR]   -= dt*(v[iBPHI]/lor2 + vB*v[iVPHI])*v[iBPHI]*R_1;
      rhs[i][iBPHI] -= dt*(v[iVPHI]*v[iBR] - v[iBPHI]*v[iVR])*R_1;
      #endif

      #if RADIATION
      Sr = EddTensor(v,iFRPHI,iFRPHI) * v[ENR] ;
      rhs[i][iFRR] += dt*Sr*R_1;
      #endif
 
    }
     
  } else if (g_dir == JDIR) {

  /* ****************************************************
      Polar azimuthal direction:

       - initialize rhs with flux differences (J1)
       - add gravity                          (J4)
     **************************************************** */

    R = x1[i];
    z = x3[k];
    scrh = dt/R;
    for (j = beg; j <= end; j++){ 
      phi  = x2[j];
      dtdx = scrh/dx2[j];

    /* ------------------------------------------------
       J1. Compute equations rhs for phi-contributions
       ------------------------------------------------ */

      NVAR_LOOP(nv) rhs[j][nv] = -dtdx*(flux[j][nv] - flux[j-1][nv]);
      rhs[j][iMPHI] -= dtdx*(p[j] - p[j-1]);
      #if IRRADIATION
      rhs[j][FIR] = 0.;
      #endif

    }

  } else if (g_dir == KDIR) { 

  /* ****************************************************
      Polar vertical direction:

       - initialize rhs with flux differences (K1)
       - enforce conservation of total angular
         momentum and/or energy               (K3)
       - add gravity                          (K4)
     **************************************************** */

    R   = x1[i];
    phi = x2[j];
    for (k = beg; k <= end; k++){ 
      z    = x3[k];
      dtdx = dt/dx3[k];

    /* -----------------------------------------------
       K1. initialize rhs with flux difference
       ----------------------------------------------- */

      NVAR_LOOP(nv) rhs[k][nv] = -dtdx*(flux[k][nv] - flux[k-1][nv]);
      rhs[k][iMZ] -= dtdx*(p[k] - p[k-1]);
      #if IRRADIATION
      rhs[k][FIR] = 0.;
      #endif

    }
  }
}
#elif GEOMETRY == SPHERICAL

/* ***********************************************************
   
        Compute right-hand side of the RMHD/RHD 
        equations in SPHERICAL geometry.
    
   *********************************************************** */
{
  double r, th, phi;
  double r2, r3, r_1;
  double s, s2, ct, s_1;
  double vB, vel2, lor2, wt, B2;
  double mth = 0.0, mphi = 0.0;

  #if RADIATION
  double Sr;
  #endif

  if (g_dir == IDIR) { 
    double Sm;

  /* ****************************************************
      Spherical radial direction: 
      multiply fluxes by interface area 
     **************************************************** */

    th  = x2[j]; s = sin(th);
    phi = x3[k];
    for (i = beg - 1; i <= end; i++){
      r  = x1p[i];
      r2 = r*r; 
      r3 = r2*r;

      fA[i][RHO]   = flux[i][RHO]*r2;
      fA[i][iMR]   = flux[i][iMR]*r2;
      fA[i][iMTH]  = flux[i][iMTH]*r2;
      fA[i][iMPHI] = flux[i][iMPHI]*r3;

      #if PHYSICS == RMHD
      fA[i][iBR]   = flux[i][iBR]*r2;
      fA[i][iBTH]  = flux[i][iBTH]*r;
      fA[i][iBPHI] = flux[i][iBPHI]*r;
      #endif
      #if HAVE_ENERGY
      fA[i][ENG] = flux[i][ENG]*r2;
      #endif
      #ifdef GLM_MHD
      fA[i][PSI_GLM] = flux[i][PSI_GLM]*r2;
      #endif
      #if RADIATION
      fA[i][ENR]    = flux[i][ENR]*r2;
      fA[i][iFRR]   = flux[i][iFRR]*r2; 
      fA[i][iFRTH]  = flux[i][iFRTH]*r2;
      fA[i][iFRPHI] = flux[i][iFRPHI]*r3;     
      #endif
      NSCL_LOOP(nv) fA[i][nv] = flux[i][nv]*r2;
    } 

  /* ****************************************************
      Spherical radial direction:

       - initialize rhs with flux differences (I1)
       - add source terms                     (I2)
       - enforce conservation of total angular 
         momentum and/or energy               (I3)
       - add gravity                          (I4)
     **************************************************** */

    Enthalpy (stateC->v, h, beg, end);
    for (i = beg; i <= end; i++) { 
      r    = x1[i];
      dV1  = (x1p[i]*x1p[i]*x1p[i] - x1m[i]*x1m[i]*x1m[i])/3.0;
      dtdV = dt/dV1;
      dtdx = dt/dx1[i];
      r_1  = 1.0/r;

    /* -----------------------------------------------
       I1. initialize rhs with flux difference
       ----------------------------------------------- */

      rhs[i][RHO]   = - dtdV*(fA[i][RHO] - fA[i-1][RHO]);
      rhs[i][iMR]   = - dtdV*(fA[i][iMR] - fA[i-1][iMR])
                      - dtdx*(p[i] - p[i-1]);   
      rhs[i][iMTH]  = -dtdV*(fA[i][iMTH]  - fA[i-1][iMTH]); 
      rhs[i][iMPHI] = -dtdV*(fA[i][iMPHI] - fA[i-1][iMPHI])*r_1; 
      #if PHYSICS == RMHD
      rhs[i][iBR]   = -dtdV*(fA[i][iBR]   - fA[i-1][iBR]); 
      rhs[i][iBTH]  = -dtdx*(fA[i][iBTH]  - fA[i-1][iBTH])*r_1;
      rhs[i][iBPHI] = -dtdx*(fA[i][iBPHI] - fA[i-1][iBPHI])*r_1;
      #ifdef GLM_MHD
      rhs[i][iBR]     = -dtdx*(flux[i][iBR]   - flux[i-1][iBR]);
      rhs[i][PSI_GLM] = -dtdV*(fA[i][PSI_GLM] - fA[i-1][PSI_GLM]);
      #endif
      #endif  /* PHYSICS == RMHD */
      #if HAVE_ENERGY
      rhs[i][ENG] = -dtdV*(fA[i][ENG] - fA[i-1][ENG]);
      #endif
      #if RADIATION
      rhs[i][ENR]    = -dtdV*(fA[i][ENR]    - fA[i-1][ENR]);
      rhs[i][iFRR]   = -dtdV*(fA[i][iFRR]   - fA[i-1][iFRR]); 
      rhs[i][iFRTH]  = -dtdV*(fA[i][iFRTH]  - fA[i-1][iFRTH]);
      rhs[i][iFRPHI] = -dtdV*(fA[i][iFRPHI] - fA[i-1][iFRPHI])*r_1;    
      #if IRRADIATION
      rhs[i][FIR] = 0.;
      #endif    
      #endif      
      
      NSCL_LOOP(nv)  rhs[i][nv] = -dtdV*(fA[i][nv] - fA[i-1][nv]);

    /* ----------------------------------------------------
       I2. Add source terms 
       ---------------------------------------------------- */
  
      v = stateC->v[i];

      vel2 = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
      lor2 = 1.0/(1.0 - vel2);
      #if PHYSICS == RMHD
      vB   = v[VX1]*v[BX1] + v[VX2]*v[BX2] + v[VX3]*v[BX3];
      B2   = v[BX1]*v[BX1] + v[BX2]*v[BX2] + v[BX3]*v[BX3];
      wt   = v[RHO]*h[i]*lor2 + B2;
      mth  = wt*v[iVTH]  - vB*v[iBTH];
      mphi = wt*v[iVPHI] - vB*v[iBPHI];
      #elif PHYSICS == RHD
      wt   = v[RHO]*h[i]*lor2;
      mth  = wt*v[iVTH]; 
      mphi = wt*v[iVPHI];
      #endif

      Sm = mth*v[iVTH] + mphi*v[iVPHI];
      #if PHYSICS == RMHD 
      Sm +=  - (v[iBTH]/lor2  + vB*v[iVTH])*v[iBTH]
             - (v[iBPHI]/lor2 + vB*v[iVPHI])*v[iBPHI];
      #endif
      rhs[i][iMR] += dt*Sm*r_1;

      #if RADIATION
      Sr = (EddTensor(v,iFRTH,iFRTH) + EddTensor(v,iFRPHI,iFRPHI)) * v[ENR] ;
      rhs[i][iFRR] += dt*Sr*r_1;
      #endif

    }

  } else if (g_dir == JDIR) {
    double Sm;

  /* ****************************************************
      Spherical meridional direction:
      multiply fluxes by zone-interface area
     **************************************************** */

    r   = x1[i];
    phi = x3[k];
    for (j = beg - 1; j <= end; j++){
      s  = fabs(sin(x2p[j]));
      s2 = s*s;

      fA[j][RHO] = flux[j][RHO]*s;
      fA[j][iMR]   = flux[j][iMR]*s; 
      fA[j][iMTH]  = flux[j][iMTH]*s;
      fA[j][iMPHI] = flux[j][iMPHI]*s2;
      #if PHYSICS == RMHD
      fA[j][iBR]   = flux[j][iBR]*s;
      fA[j][iBTH]  = flux[j][iBTH]*s;
      fA[j][iBPHI] = flux[j][iBPHI];
      #endif
      #if HAVE_ENERGY
      fA[j][ENG] = flux[j][ENG]*s;
      #endif
      #ifdef GLM_MHD
      fA[j][PSI_GLM] = flux[j][PSI_GLM]*s;
      #endif
      #if RADIATION
      fA[j][ENR]    = flux[j][ENR]*s;
      fA[j][iFRR]   = flux[j][iFRR]*s;
      fA[j][iFRTH]  = flux[j][iFRTH]*s;
      fA[j][iFRPHI] = flux[j][iFRPHI]*s2;      
      #endif
      NSCL_LOOP(nv) fA[j][nv] = flux[j][nv]*s;
    }

  /* ****************************************************
      Spherical meridional direction:

       - initialize rhs with flux differences (J1)
       - add source terms                     (J2)
       - enforce conservation of total angular
         momentum and/or energy               (J3)
       - add gravity                          (J4)
     **************************************************** */
    
    dV1  = (x1p[i]*x1p[i]*x1p[i] - x1m[i]*x1m[i]*x1m[i])/3.0;
    r_1 = 0.5*(x1p[i]*x1p[i] - x1p[i-1]*x1p[i-1])/dV1;

    Enthalpy (stateC->v, h, beg, end);
    for (j = beg; j <= end; j++){
      th   = x2[j];
      dmu  = fabs(cos(x2m[j]) - cos(x2p[j]));
      dtdV = dt/dmu*r_1;
      dtdx = dt/dx2[j]*r_1;      
      s    = sin(th);
      s_1  = 1.0/s;   
      ct   = cos(th)*s_1;         /* = cot(theta)  */

    /* -----------------------------------------------
       J1. initialize rhs with flux difference
       ----------------------------------------------- */

      rhs[j][RHO] = -dtdV*(fA[j][RHO] - fA[j-1][RHO]);
      rhs[j][iMR]   = - dtdV*(fA[j][iMR] - fA[j-1][iMR]);
      rhs[j][iMTH]  = - dtdV*(fA[j][iMTH] - fA[j-1][iMTH])
                      - dtdx*(p[j] - p[j-1]);    
      rhs[j][iMPHI] = - dtdV*(fA[j][iMPHI] - fA[j-1][iMPHI])*fabs(s_1);
      #if PHYSICS == RMHD
      rhs[j][iBR]   = -dtdV*(fA[j][iBR]   - fA[j-1][iBR]);
      rhs[j][iBTH]  = -dtdV*(fA[j][iBTH]  - fA[j-1][iBTH]);
      rhs[j][iBPHI] = -dtdx*(fA[j][iBPHI] - fA[j-1][iBPHI]);
      #ifdef GLM_MHD
      rhs[j][iBTH]    = -dtdx*(flux[j][iBTH] - flux[j-1][iBTH]);
      rhs[j][PSI_GLM] = -dtdV*(fA[j][PSI_GLM] - fA[j-1][PSI_GLM]);
      #endif
      #endif /* PHYSICS == RMHD */
      #if HAVE_ENERGY
       rhs[j][ENG] = -dtdV*(fA[j][ENG] - fA[j-1][ENG]);
      #endif
      #if RADIATION
      rhs[j][ENR]    = -dtdV*(fA[j][ENR] - fA[j-1][ENR]);
      rhs[j][iFRR]   = -dtdV*(fA[j][iFRR]   - fA[j-1][iFRR]);
      rhs[j][iFRTH]  = -dtdV*(fA[j][iFRTH]  - fA[j-1][iFRTH]);
      rhs[j][iFRPHI] = -dtdV*(fA[j][iFRPHI] - fA[j-1][iFRPHI])*fabs(s_1);   
      #if IRRADIATION
      rhs[j][FIR] = 0.;
      #endif   
      #endif   
      
       NSCL_LOOP(nv)  rhs[j][nv] = -dtdV*(fA[j][nv] - fA[j-1][nv]);

    /* ----------------------------------------------------
       J2. Add source terms
       ---------------------------------------------------- */
       
      v    = stateC->v[j];
      vel2 = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
      lor2 = 1.0/(1.0 - vel2);
      #if PHYSICS == RMHD
      vB  = v[VX1]*v[BX1] + v[VX2]*v[BX2] + v[VX3]*v[BX3];
      B2  = v[BX1]*v[BX1] + v[BX2]*v[BX2] + v[BX3]*v[BX3];
      wt  = v[RHO]*h[j]*lor2 + B2;
      mth  = wt*v[iVTH]  - vB*v[iBTH]; 
      mphi = wt*v[iVPHI] - vB*v[iBPHI];
      #elif PHYSICS == RHD
      wt   = v[RHO]*h[j]*lor2;
      mth  = wt*v[iVTH]; 
      mphi = wt*v[iVPHI];
      #endif

      Sm = - mth*v[iVR] + ct*mphi*v[iVPHI];
      #if PHYSICS == RMHD
      Sm +=      (v[iBTH]/lor2  + vB*v[iVTH])*v[iBR]
            - ct*(v[iBPHI]/lor2 + vB*v[iVPHI])*v[iBPHI];
      #endif
      rhs[j][iMTH] += dt*Sm*r_1;

      #if RADIATION
      Sr = ( ct * EddTensor(v,iFRPHI,iFRPHI)
           - EddTensor(v,iFRR,iFRTH) ) * v[ENR] ;
      rhs[j][iFRTH] += dt*Sr*r_1;
      #endif

    }

  } else if (g_dir == KDIR) {

  /* ****************************************************
      Spherical azimuthal direction:

       - initialize rhs with flux differences (K1)
       - add gravity                          (K4)
     **************************************************** */

    r  = x1[i];
    th = x2[j];
    dV1  = (x1p[i]*x1p[i]*x1p[i] - x1m[i]*x1m[i]*x1m[i])/3.0;
    dmu  = fabs(cos(x2m[j]) - cos(x2p[j]));
    r_1  = 0.5*(x1p[i]*x1p[i] - x1p[i-1]*x1p[i-1])/dV1;
    scrh = dt*r_1*dx2[j]/dmu;

    for (k = beg; k <= end; k++) {
      phi  = x3[k];
      dtdx = scrh/dx3[k];

    /* ------------------------------------------------
       K1.  initialize rhs with flux difference
       ------------------------------------------------ */

      NVAR_LOOP(nv) rhs[k][nv] = -dtdx*(flux[k][nv] - flux[k-1][nv]);
      rhs[k][iMPHI] -= dtdx*(p[k] - p[k-1]); 
      #if IRRADIATION
      rhs[k][FIR] = 0.;
      #endif

    }
  }
}
#endif  /* GEOMETRY == SPHERICAL */

/* --------------------------------------------------------
   6. Add source terms
   -------------------------------------------------------- */

  RightHandSideSource (sweep, Dts, beg, end, dt, NULL, grid);

/* --------------------------------------------------
    Reset right hand side in internal boundary zones
   -------------------------------------------------- */
   
  #if INTERNAL_BOUNDARY == YES
  InternalBoundaryReset(sweep, Dts, beg, end, grid);
  #endif

}
