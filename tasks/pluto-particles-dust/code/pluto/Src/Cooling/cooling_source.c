/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Integrate cooling and reaction source terms.

  Solve the system of ordinary differential equations describing
  optically thin cooling and ionization network (if any).
  We use an adaptive step size integration method and follow the 
  strategy outlined in section 5.1 of Tesileanu et al. (2008) for handling
  stiff cells.

  On output, a time-step estimate for the next time level is computed using
  the relative or absolute variation obtained during the integration of the ODE 
  (from t(n) --> t(n+1))  
  \f[
     \Delta t_c = \min_{ijk}\left[\frac{\Delta t^n M_R}{\epsilon}\right]
     \,\quad\rm{where}\quad
     \epsilon = \max\left(\left|\frac{p^{n+1}}{p^n} - 1\right|,\,
                |X^{n+1}-X^n|\right)
  \f]
  where \f$ M_R \f$ is the maximum cooling rate (defined by the global variable  
  ::g_maxCoolingRate) and X are the chemical species.
  
  \b References
     - "Simulating radiative astrophysical flows with the PLUTO code:
        a non-equilibrium, multi-species cooling function" \n
       Tesileanu, Mignone and Massaglia, A&A (2008) 488, 429

  \authors A. Mignone (andrea.mignone@unito.it)\n
           O. Tesileanu
           B. Vaidya
  \date    April 09, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************
    Global coordinates available in the Radiat() function
   ******************************************************** */

double gCooling_x1, gCooling_x2, gCooling_x3;

/* ********************************************************************* */
void CoolingSource (const Data *d, double dt, timeStep *Dts, Grid *grid)
/*!
 * Integrate cooling and reaction source terms.
 *
 * \param [in,out]  d     pointer to Data structure
 * \param [in]     dt     the time step to be taken
 * \param [out]    Dts    pointer to the Time_Step structure
 * \param [in]     grid   pointer to an array of Grid structures
 *
 *********************************************************************** */
{
  int  nv, k, j, i, stiff, status;
  double err, scrh, min_tol = 2.e-5;
  double mu0, T0, T1, mu1, prs;
  double v0[NVAR_COOLING], v1[NVAR_COOLING], k1[NVAR_COOLING];
  double maxrate;
  intList var_list;
 
/* --------------------------------------------------------
   0. Create list of time-dependent variables for this
      step. Initialize coefficients.
   -------------------------------------------------------- */

  var_list.nvar    = NIONS+1;
  var_list.indx[0] = RHOE;
  for (nv = 0; nv < NIONS; nv++) var_list.indx[nv+1] = NFLX+nv;

  for (nv = NVAR_COOLING; nv--;  ) {
    v0[nv] = v1[nv] = k1[nv] = 0.0;
  }

/* -------------------------------------------------------- 
   1. Main loop on interior computational zones
   --------------------------------------------------------  */

  DOM_LOOP(k,j,i){
  
  /* ------------------------------------------------------
     1A. Define global coordinates.
         (useful for spatial-dependent cooling
     ------------------------------------------------------ */ 

    gCooling_x1 = grid->x[IDIR][i];
    gCooling_x2 = grid->x[JDIR][j];
    gCooling_x3 = grid->x[KDIR][k];

  /* ------------------------------------------------------
     1B. Skip integration if cell has been tagged with 
      FLAG_INTERNAL_BOUNDARY or FLAG_SPLIT_CELL 
      (AMR only).
     ------------------------------------------------------ */ 

    #if INTERNAL_BOUNDARY == YES
    if (d->flag[k][j][i] & FLAG_INTERNAL_BOUNDARY) continue;
    #endif
    if (d->flag[k][j][i] & FLAG_SPLIT_CELL) continue;
    NVAR_LOOP(nv) v0[nv] = v1[nv] = d->Vc[nv][k][j][i];

  /* ------------------------------------------------------
     1C. Compute temperature and internal energy from
         density, pressure and concentrations.
     ------------------------------------------------------ */
    
    prs = v0[PRS];
    mu0 = MeanMolecularWeight(v0);
    T0  = v0[PRS]/v0[RHO]*KELVIN*mu0;

    #if EOS == IDEAL
    v0[RHOE] = v1[RHOE] = prs/(g_gamma-1.0);
    #else
    v1[RHOE] = v0[RHOE] = InternalEnergy(v0, T0);
    #endif

    if (T0 <= 0.0){
      printLog ("! CoolingSource(): negative initial temperature\n");
      printLog (" %12.6e  %12.6e\n",v0[RHOE], v0[RHO]);
      printLog (" at: %f %f\n",gCooling_x1, gCooling_x2);
      QUIT_PLUTO(1);
    }

  /* ------------------------------------------------------
     1D. Integrate the ODEs from t1 = 0...dt with
         increments dt1.

         - if the rhs is not stiff, try with a lower order
           method (e.g. RK2);
         - otherwise switch to Cash-Karp 5th order with
           adaptive step size dt1.
     ------------------------------------------------------ */

    Radiat(v0, k1);

    stiff = (fabs(k1[RHOE]) > 0.5/dt ? 1:0);
    if (!stiff){
      err  = SolveODE_RKF12 (v0, k1, v1, dt, &var_list);
      err /= min_tol;
    }

    if (stiff || err > 1.0){  /* Adaptive step size */
      double dt1, dts=dt;
      double t1   = 0.0;  /* t1 = 0, ..., dt */ 
      int    done = 0;

      dt1 = 0.5/(fabs(k1[RHOE]) + 1.0/dt);   /* Rough estimate to initial dt1 */
      do {
        
      /* -- Adjust time step dt1 so we do not overshoot dt -- */

        if ( (t1+dt1) >= dt) {
          dt1 = dt - t1;
          done = 1;
        }

        dts = SolveODE_CK45 (v0, k1, v1, dt1, min_tol, &var_list);
        t1 += dt1;  
        dt1 = dts;

        if (done) break;
        for (nv = NVAR_COOLING; nv--;  ) v0[nv] = v1[nv];
        Radiat(v0, k1);

      }while (!done);
    }

  /* ------------------------------------------------------
     1E. As asafety measure, constrain the ions to
         lie in [0,1] and pressure to be positive.
     ------------------------------------------------------ */

    NIONS_LOOP(nv){
      v1[nv] = MAX(v1[nv], 0.0);
      v1[nv] = MIN(v1[nv], 1.0);
    }
    #if COOLING == H2_COOL
     v1[X_H2] = MIN(v1[X_H2], 0.5);
    #endif
    
  /* -- pressure must be positive -- */

    mu1 = MeanMolecularWeight(v1);
    #if EOS == IDEAL
    prs = v1[RHOE]*(g_gamma - 1.0);
    T1  = prs/v1[RHO]*KELVIN*mu1;
    #elif EOS == PVTE_LAW
    status = GetEV_Temperature(v1[RHOE], v1, &T1);
    prs    = v1[RHO]*T1/(KELVIN*mu1);
    #endif

    if (prs < 0.0) prs = g_smallPressure;

  /* ------------------------------------------------------
     1F. If gas cooled below lower threshold, redefine
         pressure so that T = g_minCoolingTemp.
     ------------------------------------------------------ */

    if (T1 < g_minCoolingTemp && T0 > g_minCoolingTemp)
      prs = g_minCoolingTemp*v1[RHO]/(KELVIN*mu1);

  /* ------------------------------------------------------
     1G. Suggest next time step based on 
         fractional variaton.
     ------------------------------------------------------ */

    err = fabs(prs/d->Vc[PRS][k][j][i] - 1.0);

    #if COOLING == MINEq
    for (nv = NFLX; nv < NFLX + NIONS - Fe_IONS; nv++) err = MAX(err, fabs(d->Vc[nv][k][j][i] - v1[nv]));
    #else
    NIONS_LOOP(nv) err = MAX(err, fabs(d->Vc[nv][k][j][i] - v1[nv]));
    #endif

    scrh = dt*g_maxCoolingRate/err;   
    Dts->dt_cool = MIN(Dts->dt_cool, scrh);

  /* ------------------------------------------------------
     1H. Update main solution array
     ------------------------------------------------------ */

    d->Vc[PRS][k][j][i] = prs;
    NIONS_LOOP(nv) d->Vc[nv][k][j][i] = v1[nv];

  } /* -- End DOM_LOOP(k,j,i) -- */
}
 
/* ********************************************************************* */
void Numerical_Jacobian (double *v, double **J)
/*!
 *  Compute Jacobian matrix numerically and 
 *  get eigenvector and eigenvalue decomposition
 *
 *  The purpose of this function is to detect
 *  stiffness. 
 *
 *  Note: This function is EXTREMELY time consuming.
 *
 *********************************************************************** */
{
  int  n, nv, k, l;
  double eps;
  double vpp[NVAR_COOLING], vp[NVAR_COOLING], vm[NVAR_COOLING], vmm[NVAR_COOLING];
  double Rpp[NVAR_COOLING], Rp[NVAR_COOLING], Rm[NVAR_COOLING], Rmm[NVAR_COOLING];

  eps = 1.e-5;
  n = NIONS + 1;

/* -- partial derivs with respect to pressure -- */

  for (nv = 0; nv < NVAR; nv++){
    vp[nv] = vm[nv] = v[nv];
  }
  vp[RHOE] *= 1.0 + eps;
  vm[RHOE] *= 1.0 - eps;
  
  Radiat (vp, Rp);
  Radiat (vm, Rm);

  for (k = 0; k < n - 1; k++){
    J[k][n - 1] = (Rp[k + NFLX] - Rm[k + NFLX])/(2.0*v[RHOE]*eps);
  }
  J[n - 1][n - 1] = (Rp[RHOE] - Rm[RHOE])/(2.0*v[RHOE]*eps);

/* -- partial deriv with respect to ions -- */

  for (l = 0; l < n - 1; l++){

    for (nv = 0; nv < NVAR; nv++){
      vp[nv] = vm[nv] = v[nv];
    }
    vp [l + NFLX] = v[l + NFLX] + eps;
    vm [l + NFLX] = v[l + NFLX] - eps;
                      
    vp [l + NFLX] = MIN(vp [l + NFLX], 1.0);
    vm [l + NFLX] = MAX(vm [l + NFLX], 0.0);
                         
    Radiat (vp , Rp);
    Radiat (vm , Rm);

    for (k = 0; k < n - 1; k++){
      J[k][l] = (Rp[k + NFLX] - Rm[k + NFLX])/(vp[l + NFLX] - vm[l + NFLX]); 
    }
    J[n - 1][l] = (Rp[RHOE] - Rm[RHOE])/(vp[l + NFLX] - vm[l + NFLX]);
  }
}
