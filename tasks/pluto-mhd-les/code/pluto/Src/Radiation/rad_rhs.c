#include "pluto.h"

static void RadRightHandSideSource (const Sweep *, timeStep *, int, int, double, Grid *);

#ifdef CH_SPACEDIM   /*  implies Chombo is being used  */
 #define USE_PRS_GRADIENT  YES   
#else
 #ifdef FINITE_DIFFERENCE 
  #define USE_PRS_GRADIENT  NO   /* -- default for Finite Difference schemes -- */
 #else
  #define USE_PRS_GRADIENT  YES   /* -- default, do not change!! -- */
 #endif
#endif

#if RADIATION_NR
/* *********************************************************************** */
void RadRightHandSide (const Sweep *sweep, timeStep *Dts, 
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

  double A, scrh ;

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
  double dtdV ;  
  static double **fA;

/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */

#if GEOMETRY != CARTESIAN
  if (fA == NULL) {
    fA   = ARRAY_2D(NMAX_POINT, NVAR, double);
  }
#endif

  i = g_i;  /* will be redefined during x1-sweep */
  j = g_j;  /* will be redefined during x2-sweep */
  k = g_k;  /* will be redefined during x3-sweep */
  
/* --------------------------------------------------------
   4. Compute total flux for curvilinear coordinates
   -------------------------------------------------------- */

#if GEOMETRY != CARTESIAN

  if (g_dir == IDIR){ 
   for (i = beg-1; i <= end; i++){

     A  = grid->A[IDIR][k][j][i];
     
     NRAD_LOOP(nv) fA[i][nv] = flux[i][nv]*A;
     
     fA[i][iFRPHI] *= fabs(x1p[i]);
     
   }
  }else if (g_dir == JDIR){
    for (j = beg-1; j <= end; j++){ 
          
     A  = grid->A[JDIR][k][j][i];
      
     NRAD_LOOP(nv) fA[j][nv] = flux[j][nv]*A;
     
     #if GEOMETRY == SPHERICAL
      double sp = grid->sp[j];
      fA[j][iFRPHI] *= fabs(sp);
     #endif
     
   }
  }else if (g_dir == KDIR){
    for (k = beg-1; k <= end; k++) {
     
     A  = grid->A[KDIR][k][j][i];
     
     NRAD_LOOP(nv) fA[k][nv] = flux[k][nv]*A;
      
    }
  }
#endif

/* --------------------------------------------------------
   5. Compute right hand side
   -------------------------------------------------------- */

#if GEOMETRY == CARTESIAN

  for (i = beg; i <= end; i++) {
    scrh = dt/dx[i];
    NVAR_LOOP(nv) rhs[i][nv] = 0.;
    NRAD_LOOP(nv) rhs[i][nv] = -scrh*(flux[i][nv] - flux[i-1][nv]);
  }
  
#else
  if (g_dir == IDIR){
    double q;
    for (i = beg; i <= end; i++){

    /* ------------------------------------------
       I1. Build rhs in the x1-dir
       ------------------------------------------ */
           
      dtdV = dt/dV[k][j][i];
      
      NVAR_LOOP(nv) rhs[i][nv] = 0.;
      NRAD_LOOP(nv) rhs[i][nv] = -dtdV*(fA[i][nv] - fA[i-1][nv]);

      rhs[i][iFRPHI] /= fabs(x1[i]);
      
    }

  }else if (g_dir == JDIR){
    
    double **dx_dl = grid->dx_dl[JDIR];
    for (j = beg; j <= end; j++){

    /* ------------------------------------------
       J1. Build rhs in the x2-dir
       ------------------------------------------ */

      dtdV = dt/dV[k][j][i];
      
      NVAR_LOOP(nv) rhs[j][nv] = 0.;
      NRAD_LOOP(nv) rhs[j][nv] = -dtdV*(fA[j][nv] - fA[j-1][nv]);

      #if GEOMETRY == SPHERICAL
      rhs[j][iFRPHI] /= fabs(s[j]);
      #endif
      
    }

  }else if (g_dir == KDIR){

    for (k = beg; k <= end; k++){

    /* ------------------------------------------
       K1. Build rhs in the x3-dir
       ------------------------------------------ */

      dtdV = dt/dV[k][j][i];
      
      NVAR_LOOP(nv) rhs[k][nv] = 0.;
      NRAD_LOOP(nv) rhs[k][nv] = -dtdV*(fA[k][nv] - fA[k-1][nv]);

    }
  }
#endif  /* GEOMETRY == CARTESIAN */

/* --------------------------------------------------------
   6. Add source terms
   -------------------------------------------------------- */

  RadRightHandSideSource (sweep, Dts, beg, end, dt, grid);

   /* --------------------------------------------------------
   7. Reset right hand side in internal boundary zones
   -------------------------------------------------------- */
   
  #if INTERNAL_BOUNDARY == YES
    InternalBoundaryReset(sweep, Dts, beg, end, grid);
  #endif

}

/* *********************************************************************** */
void RadRightHandSideSource (const Sweep *sweep, timeStep *Dts,
                          int beg, int end, double dt, Grid *grid)
/*! 
 *
 * \param [in,out]  state  pointer to State_1D structure
 * \param [in]      Dts    pointer to time step structure
 * \param [in]      beg    initial index of computation
 * \param [in]      end    final   index of computation
 * \param [in]      dt     time increment
 * \param [in]      grid   pointer to Grid structure
 *
 * \return This function has no return value.
 ************************************************************************* */
{
  int    i, j, k, nv;

  const State *stateC = &(sweep->stateC);
  const State *stateL = &(sweep->stateL);
  const State *stateR = &(sweep->stateR);

  double r_1;

  double dtdx, ct;
  double Sr;
  double *x1   = grid->x[IDIR],  *x2  = grid->x[JDIR],  *x3  = grid->x[KDIR];
  double *x1p  = grid->xr[IDIR], *x2p = grid->xr[JDIR], *x3p = grid->xr[KDIR];
  double *x1m  = grid->xl[IDIR], *x2m = grid->xl[JDIR], *x3m = grid->xl[KDIR];
  double *dx1  = grid->dx[IDIR], *dx2 = grid->dx[JDIR], *dx3 = grid->dx[KDIR];
#if GEOMETRY == SPHERICAL
  double *rt   = grid->rt;
#endif
  double **rhs  = sweep->rhs;
  double **vp   = stateL->v;
  double **vm   = stateR->v-1;
  double vc[NVAR];
  
  i = g_i;  /* will be redefined during x1-sweep */
  j = g_j;  /* will be redefined during x2-sweep */
  k = g_k;  /* will be redefined during x3-sweep */
  
  if (g_dir == IDIR){

    for (i = beg; i <= end; i++) {
     
      r_1 = 1.0/x1[i];

      NVAR_LOOP(nv) vc[nv] = 0.5*(vp[i][nv] + vm[i][nv]);
      
      #if RADIATION_IMPL == RADIATION_IMPLICIT_NR
      AddRelTerms (vc, rhs[i], dt);
      #endif
      
      #if GEOMETRY == CYLINDRICAL || GEOMETRY == POLAR
      
      Sr = EddTensor(vc,iFRPHI,iFRPHI) * vc[ENR] ;
      
      rhs[i][iFRR] += dt*Sr*r_1*g_reducedC ;
     
      #elif GEOMETRY == SPHERICAL

      Sr = (EddTensor(vc,iFRTH,iFRTH) + EddTensor(vc,iFRPHI,iFRPHI))
          * vc[ENR] ;
          
      rhs[i][iFRR] += dt*Sr*r_1*g_reducedC ;

      #endif

      #if IRRADIATION && ((!RADIATION_IMPLICIT_NR) || RADIATION_IMEX_SSP2)
      rhs[i][ENG] -= dt*vc[FIR];
      #endif

    }
  }
  #if GEOMETRY == SPHERICAL
  else if (g_dir == JDIR){

    r_1   = 1.0/rt[i];
    
    for (j = beg; j <= end; j++) {
     
      ct = 1.0/tan(x2[j]); 

      NRAD_LOOP(nv) vc[nv] = stateC->v[j][nv];
      
      Sr = ( ct * EddTensor(vc,iFRPHI,iFRPHI)
           - EddTensor(vc,iFRR,iFRTH) ) * vc[ENR] ;
      
      rhs[j][iFRTH] += dt*Sr*r_1*g_reducedC ;
  
    }
    
  }
  #endif

}

void AddRelTerms (double * primvar, double *rhs, double dt){
 /*!
 * Explicitly integrate order v/c source terms
 *
 * \param [in]      primvar  vector of primitive variables
 * \param [in,out]  rhs      vector of right-hand sides
 * \param [in]      dt       time step
 *
 * \return This function has no return value.
 ************************************************************************* */
 
  int i, j ;
  double u[3], u2, gamma, gamma2, D, uD[3], uuD, uF, uFc, G[4] ;
  double B, rho, rhogamma, q ;
  double s, comovEr, comovFr[3] ;

  /*-- Set opacities --*/
  double abs_op, scat_op, tot_op ;
  #if RADIATION_VAR_OPACITIES == YES
  UserDefOpacities (primvar, &abs_op, &scat_op);
  tot_op = abs_op + scat_op ; 
  #else
  abs_op = g_absorptionCoeff;
  scat_op = g_scatteringCoeff;
  tot_op = g_totalOpacity ;
  #endif
  
  /*-- Compute beta --*/
  for (i=0; i<3; i++) u[i] = primvar[VX1+i] / g_radC ;
  
  /*-- Compute products involving beta --*/
  uF = 0.;
  for (i=0; i<3; i++ ){
    uD[i] = 0.;
    uF += u[i]*primvar[FR1+i] ;
    for (j=0; j<3; j++ )
      uD[i] += u[j] * EddTensor(primvar, FR1+j, FR1+i);
  }
  
  /*-- Compute comoving radiation fields --*/
  comovEr = primvar[ENR] - 2.*uF ;
  for (i=0; i<3; i++)
    comovFr[i] = primvar[FR1+i] - primvar[ENR] * (u[i] + uD[i]) ;
    
  /*-- Compute some auxiliary quantities --*/
  rho = primvar[RHO] ;
  B = Blackbody( GetTemperature(rho, primvar[PRS]) ) ;
  q = abs_op * rho * (comovEr - B) ;
  s = tot_op * rho ;
  uFc = 0.; for (i=0; i<3; i++) uFc += u[i] * comovFr[i] ;
    
  /*-- Compute relativistic component of source function in the lab frame --*/
  G[0] = -2.*abs_op*rho*uF + s*uFc ;
  for (i=0; i<3; i++) G[i+1] = q*u[i] - s*primvar[ENR]*(u[i] + uD[i]) ;
  
  /*-- Update rhs --*/
  rhs[ENG] += dt*g_radC*G[0] ;
  rhs[ENR] -= dt*g_reducedC*G[0] ;
  for(i=0; i<3; i++){
    rhs[MX1+i] += dt*G[i+1] ;
    rhs[FR1+i] -= dt*G[i+1]*g_reducedC ;
  }
  
}
#endif