/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief LimO3 reconstruction.

  Provide a three-point stencil, third-order reconstruction algorithm 
  based on the limiter function of Cada & Torrilhon

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Jun 10, 2024

  \b References
     - "Compact third-order limiter functions for finite volume
        methods", Cada & Torrilhon, JCP (2009) 228, 4118.
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"
static double LimO3Func (double, double, double);
static double LimO3Func2 (double, double, double);
static double LimO3Func_gPLUTO (double *, int, double);

/* ********************************************************************* */
void States (const Sweep *sweep, int beg, int end, Grid *grid)
/* 
 *
 *
 *********************************************************************** */
{
  int    k, nv, i;

  const State *stateC = &(sweep->stateC);
  const State *stateL = &(sweep->stateL);
  const State *stateR = &(sweep->stateR);

  double **v  = stateC->v;
  double **vp = stateL->v;
  double **vm = stateR->v-1;
  double **up = stateL->u;
  double **um = stateR->u-1;

  double dmm;
  double *dvp, *dvm, *dx;
  double **L, **R, *lambda;
  double dwp[NVAR], dwp_lim[NVAR];
  double dwm[NVAR], dwm_lim[NVAR];
  double dvpR, dvmR;
  static double **dv;

/* --------------------------------------------------------
   0. Allocate memory, set pointer shortcuts 
   -------------------------------------------------------- */

  if (dv == NULL) {
    dv = ARRAY_2D(NMAX_POINT, NVAR, double);
  }

  v  = stateC->v;
  vp = stateL->v;
  vm = stateR->v-1;

  #if (INTERNAL_BOUNDARY == YES) && (INTERNAL_BOUNDARY_REFLECT == YES)
  FluidInterfaceBoundary(sweep, beg, end, grid);
  #endif
  
  #if RECONSTRUCT_4VEL
  ConvertTo4vel (stateC->v, beg-1, end+1);
  #endif
   
  dx = grid->dx[g_dir];

/* --------------------------------------------------------
   1. compute slopes and left and right interface values 
   -------------------------------------------------------- */

#if CHAR_LIMITING == NO  /* ---------------------------------------
                             Limiter on primitive variables
                            ----------------------------------------  */
  for (i = beg-1; i <= end; i++){
  for (nv = NVAR; nv--; ){
    dv[i][nv] = v[i+1][nv] - v[i][nv];
  }}

  for (i = beg; i <= end; i++){
    dvp = dv[i];   
    dvm = dv[i-1]; 

    #if SHOCK_FLATTENING == MULTID    
    if (sweep->flag[i] & FLAG_MINMOD){  
      for (nv = NVAR; nv--;    ) {
         dmm = MINMOD_LIMITER(dvp[nv], dvm[nv]);
         vp[i][nv] = v[i][nv] + 0.5*dmm;
         vm[i][nv] = v[i][nv] - 0.5*dmm;
       }
       continue;
     }
    #endif

    for (nv = NVAR; nv--; ){
      // vp[i][nv] = v[i][nv] + 0.5*dvp[nv]*LimO3Func(dvp[nv], dvm[nv], dx[i]);
      // vm[i][nv] = v[i][nv] - 0.5*dvm[nv]*LimO3Func(dvm[nv], dvp[nv], dx[i]);
      vp[i][nv] = v[i][nv] + LimO3Func2(dvp[nv], dvm[nv], dx[i]);
      vm[i][nv] = v[i][nv] - LimO3Func2(dvm[nv], dvp[nv], dx[i]);
    }
  }       

/*
static double *F;
if (F == NULL) F = ARRAY_1D(NMAX_POINT, double);
NVAR_LOOP(nv){
  for (i = beg-1; i <= end+1; i++) F[i] = v[i][nv];

  double vp_new, vm_new;
  for (i = beg; i <= end; i++){
    vp_new = v[i][nv] + LimO3Func2(F+i, 1, dx[i]);
    vm_new = v[i][nv] - LimO3Func2(F+i,-1, dx[i]);

if (fabs(vp_new-vp[i][nv]) > 1.e-2 || fabs(vm_new-vm[i][nv]) > 1.e-2){
printf ("i = %03d; v = %12.6e; vp     = %12.6e; vm     = %12.6e\n", 
          i, v[i][nv], vp[i][nv], vm[i][nv]);
printf ("                            vp_new = %12.6e; vm_new = %12.6e\n", vp_new, vm_new);
exit(1);
}
vp[i][nv] = vp_new;
vm[i][nv] = vm_new;


  }
}
*/

#else       /* --------------------------------------------
                Limiter on characteristic variables
               --------------------------------------------  */

  SoundSpeed2 (stateC, beg, end, CELL_CENTER, grid);
  PrimEigenvectors (stateC, beg, end);
  i = beg-1;
  NVAR_LOOP(nv) dv[i][nv] = v[i+1][nv] - v[i][nv];

  for (i = beg; i <= end; i++){

    NVAR_LOOP(nv) dv[i][nv] = v[i+1][nv] - v[i][nv];
    L      = stateC->Lp[i];
    R      = stateC->Rp[i];
    lambda = stateC->lambda[i];
    dvp = dv[i];  
    dvm = dv[i-1];
    
  /* -------------------------------
      project undivided differences 
      onto characteristic space
     ------------------------------- */
     
    PrimToChar(L, dvp, dwp);
    PrimToChar(L, dvm, dwm);

    #if SHOCK_FLATTENING == MULTID    
    if (sweep->flag[i] & FLAG_MINMOD){  
      for (nv = NVAR; nv--;    ) {
        dmm = MINMOD_LIMITER(dvp[nv], dvm[nv]);
        vp[i][nv] = v[i][nv] + 0.5*dmm;
        vm[i][nv] = v[i][nv] - 0.5*dmm;
      }
      continue;
    }
    #endif

  /* -----------------------------
      limit undivided differences
     ----------------------------- */

    for (k = NFLX; k--; ){
      dwp_lim[k] = dwp[k]*LimO3Func(dwp[k], dwm[k], dx[i]);
      dwm_lim[k] = dwm[k]*LimO3Func(dwm[k], dwp[k], dx[i]);
    }
    for (nv = NFLX; nv--;   ){
      dvpR = dvmR = 0.0;
      #ifdef STAGGERED_MHD
      if (nv == BXn) continue;
      #endif
      for (k = NFLX; k--; ){
        dvpR += dwp_lim[k]*R[nv][k];
        dvmR += dwm_lim[k]*R[nv][k];
      }
      vp[i][nv] = v[i][nv] + 0.5*dvpR;
      vm[i][nv] = v[i][nv] - 0.5*dvmR;
    }

  /* -------------------------------------- 
      Compute limited slopes for tracers
      exploiting the simple characteristic 
      structure, L=R=diag(1).
     -------------------------------------- */            

    #if NFLX != NVAR
    for (nv = NFLX; nv < NVAR; nv++ ){
      dvpR = dvp[nv]*LimO3Func(dvp[nv], dvm[nv], dx[i]);
      dvmR = dvm[nv]*LimO3Func(dvm[nv], dvp[nv], dx[i]);
      vp[i][nv] = v[i][nv] + 0.5*dvpR;
      vm[i][nv] = v[i][nv] - 0.5*dvmR;
    }
    #endif
  }
   
#endif /* CHAR_LIMITING == YES */
   
/* --------------------------------------------------------
   2. Since the third-order limiter is not TVD, we 
      need to ensure positivity of density and pressure 
   -------------------------------------------------------- */

  for (i = beg; i <= end; i++){
    dvp = dv[i];   
    dvm = dv[i-1]; 
    if (vp[i][RHO] < 0.0 || vm[i][RHO] < 0.0){
      dmm = MINMOD_LIMITER(dvp[RHO], dvm[RHO]);
      vp[i][RHO] = v[i][RHO] + 0.5*dmm;
      vm[i][RHO] = v[i][RHO] - 0.5*dmm;
    }

    #if HAVE_ENERGY
    if (vp[i][PRS] < 0.0 || vm[i][PRS] < 0.0){
      dmm = MINMOD_LIMITER(dvp[PRS], dvm[PRS]);
      vp[i][PRS] = v[i][PRS] + 0.5*dmm;
      vm[i][PRS] = v[i][PRS] - 0.5*dmm;
    }
    #endif

    #if ENTROPY_SWITCH
    if (vp[i][ENTR] < 0.0 || vm[i][ENTR] < 0.0){
      dmm = MINMOD_LIMITER(dvp[ENTR], dvm[ENTR]);
      vp[i][ENTR] = v[i][ENTR] + 0.5*dmm;
      vm[i][ENTR] = v[i][ENTR] - 0.5*dmm;
    }
    #endif

    #if RADIATION
    if (vp[i][ENR] < 0.0 || vm[i][ENR] < 0.0){
      dmm = MINMOD_LIMITER(dvp[ENR], dvm[ENR]);
      vp[i][ENR] = v[i][ENR] + 0.5*dmm;
      vm[i][ENR] = v[i][ENR] - 0.5*dmm;
    }
    #endif

  /* -- relativistic limiter --*/

    #if (PHYSICS == RHD) || (PHYSICS == RMHD) || (PHYSICS == ResRMHD)
    VelocityLimiter (v[i], vp[i], vm[i]);
    #endif
    #if RADIATION
    RadFluxLimFlatten (v[i], vp[i], vm[i]);
    #endif
  }

/* --------------------------------------------------------
    3. Shock flattening 
   -------------------------------------------------------- */

#if SHOCK_FLATTENING == ONED 
  Flatten (sweep, beg, end, grid);
#endif

/* --------------------------------------------------------
   4. Assign face-centered magnetic field
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD
  for (i = beg - 1; i <= end; i++) {
    stateL->v[i][BXn] = stateR->v[i][BXn] = sweep->Bn[i];
    #if PHYSICS == ResRMHD
    stateL->v[i][EXn] = stateR->v[i][EXn] = sweep->En[i];
    #endif
  }
#endif

/* --------------------------------------------------------
   5. Evolve L/R states and center value by dt/2
   -------------------------------------------------------- */

#if TIME_STEPPING == CHARACTERISTIC_TRACING
  CharTracingStep(sweep, beg, end, grid);
#elif TIME_STEPPING == HANCOCK
  HancockStep(sweep, beg, end, grid);
#endif

/* --------------------------------------------------------
   6. Convert back to 3-velocity
   -------------------------------------------------------- */

#if RECONSTRUCT_4VEL
  ConvertTo3vel (v, beg-1, end+1);
  ConvertTo3vel (vp, beg, end);
  ConvertTo3vel (vm, beg, end);  
#endif

/* --------------------------------------------------------
   7. Obtain L/R states in conservative variables
   -------------------------------------------------------- */

  PrimToCons (vp, up, beg, end);
  PrimToCons (vm, um, beg, end);
}

/* ********************************************************************* */
double LimO3Func (double dvp, double dvm, double dx)
/*!
 *  Implement the 3rd-order limiter function, 
 *  Eq. [3.22]
 *
 *
 *********************************************************************** */
{
  double r = 0.1;
  double a,b,c,q, th, lim;
  double eta, psi, eps = 1.e-12;

  th  = dvm/(dvp + 1.e-16);

  q = (2.0 + th)/3.0;

  a = MIN(1.5,2.0*th);
  a = MIN(q,a);
  b = MAX(-0.5*th,a);
  c = MIN(q,b);
  psi = MAX(0.0,c);

  eta = r*dx;
  eta = (dvm*dvm + dvp*dvp)/(eta*eta);
  if ( eta <= 1.0 - eps) {
    lim = q;
  }else if (eta >= 1.0 + eps){
    lim = psi;
  }else{
    psi =   (1.0 - (eta - 1.0)/eps)*q
          + (1.0 + (eta - 1.0)/eps)*psi;
    lim = 0.5*psi;
  }
  return (lim);
}

/* ********************************************************************* */
double LimO3Func2 (double dvp, double dvm, double dx)
/*!
 *  Implement the 3rd-order limiter function, 
 *  Eq. [3.22]
 *
 *
 *********************************************************************** */
{
  double sp = (dvp >= 0.0 ? 1.0:-1.0); 
  double advp = fabs(dvp);
  double q = (2.0*dvp + dvm)/3.0;
  double q1   = (2.0*fabs(dvp) + sp*dvm)/3.0;
  double dvm1 = sp*dvm; 
  double a, b, c, psi, r = 0.1;
  double eta, eps = 1.e-12, lim;

  a = MIN(1.5*advp, 2.0*dvm1);
  a = MIN(q1,a);
  b = MAX(-0.5*dvm1,a);
  c = MIN( q1,b);
  psi = sp*MAX(0.0,c);

  eta = r*dx;
  eta = (dvm*dvm + dvp*dvp)/(eta*eta);
  if ( eta <= 1.0 - eps) {
    lim = q;
  }else if (eta >= 1.0 + eps){
    lim = psi;
  }else{
    psi =   (1.0 - (eta - 1.0)/eps)*q
          + (1.0 + (eta - 1.0)/eps)*psi;
    lim = 0.5*psi;
  }
  lim *= 0.5;

  return lim;
}

/* ********************************************************************* */
double LimO3Func_gPLUTO (double *v, int dir, double dx)
/*!
 *  Implement the 3rd-order limiter function, 
 *  Eq. [3.22]
 *
 *********************************************************************** */
{
  double r = 0.1;
  double a,b,c,q, th, lim;
  double eta, psi, eps = 1.e-12;
  int ip = 0 + dir;
  int im = 0 - dir;

  double dvp = v[ip] - v[0];
  double dvm = v[0] - v[im];



// Singularity-free version

double lim2;
  double sp = (dvp >= 0.0 ? 1.0:-1.0); 
  double advp = fabs(dvp);
  q = (2.0*dvp + dvm)/3.0;
  double q1   = (2.0*fabs(dvp) + sp*dvm)/3.0;
  double dvm1 = sp*dvm; 

  a = MIN(1.5*advp, 2.0*dvm1);
  a = MIN(q1,a);
  b = MAX(-0.5*dvm1,a);
  c = MIN( q1,b);
  psi = sp*MAX(0.0,c);

  eta = r*dx;
  eta = (dvm*dvm + dvp*dvp)/(eta*eta);
  if ( eta <= 1.0 - eps) {
    lim2 = q;
  }else if (eta >= 1.0 + eps){
    lim2 = psi;
  }else{
    psi =   (1.0 - (eta - 1.0)/eps)*q
          + (1.0 + (eta - 1.0)/eps)*psi;
    lim2 = 0.5*psi;
  }
  lim2 *= 0.5*dir;

return lim2;




for (int j = -50; j < 50; j++){
  double dvm = sin(j*0.4+0.423);
  double dvp = j*cos(j*0.75-1876675);
  // double sden = (den >= 0.0 ? 1.0:-1.0);
  // double frac = num/(den+1.e-40);
  // double f1  = MAX(1.0, frac);
  // double f2  = sden*MAX(sden*num, fabs(den));
  // printf ("num, den = %12.6e, %12.6e,   f1 = %12.6e; f2 = %12.6e\n", 
  //             num, den, den*f1, f2);


  th  = dvm/(dvp + 1.e-40);

  q = (2.0 + th)/3.0;

  a = MIN(1.5,2.0*th);
  a = MIN(q,a);
  b = MAX(-0.5*th,a);
  c = MIN(q,b);
  psi = MAX(0.0,c);

  eta = r*dx;
  eta = (dvm*dvm + dvp*dvp)/(eta*eta);
  if ( eta <= 1.0 - eps) {
    lim = q;
  }else if (eta >= 1.0 + eps){
    lim = psi;
  }else{
    psi =   (1.0 - (eta - 1.0)/eps)*q
          + (1.0 + (eta - 1.0)/eps)*psi;
    lim = 0.5*psi;
  }
  lim *= 0.5*dir*dvp;

// Singularity-free version

double lim2;
  double sp = (dvp >= 0.0 ? 1.0:-1.0); 
  double advp = fabs(dvp);
  q = (2.0*dvp + dvm)/3.0;

//if (dvp > 0.0){
if (0){
  a = MIN(1.5*dvp, 2.0*dvm);
  a = MIN(q,a);
  b = MAX(-0.5*dvm,a);
  c = MIN(q,b);
  psi = MAX(0.0,c);
}else{
  a = MIN(1.5*advp, 2.0*sp*dvm);
  a = MIN(sp*q,a);
  b = MAX(-sp*0.5*dvm,a);
  c = MIN( sp*q,b);
  psi = sp*MAX(0.0,c);
}

  eta = r*dx;
  eta = (dvm*dvm + dvp*dvp)/(eta*eta);
  if ( eta <= 1.0 - eps) {
    lim2 = q;
  }else if (eta >= 1.0 + eps){
    lim2 = psi;
  }else{
    psi =   (1.0 - (eta - 1.0)/eps)*q
          + (1.0 + (eta - 1.0)/eps)*psi;
    lim2 = 0.5*psi;
  }
  lim2 *= 0.5*dir;

printf ("j = %d; (dvp, dvm) = (%12.6e, %12.6e);  lim = %12.6e; lim2 = %12.6e; diff = %12.6e\n",
            j,  dvp, dvm, lim,lim2, lim-lim2);

}


exit(1);

  return (lim);
}
