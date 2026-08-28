/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Build the right hand side for the viscosity operator

  Compute the one-dimensional right hand side for the viscous
  operator in the direction given by ::g_dir.

  \authors A. Mignone (andrea.mignone@unito.it)\n

 \b References

  \date   Aug 21, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void ViscousRHS (const Data *d, Data_Arr dU, double *dcoeff,
                 double **aflux, double dt, int beg, int end, Grid *grid)
/*!
 * \param [in]   d           pointer to PLUTO Data structure
 * \param [out]  dU          a 4D array containing conservative variables
 *                           increment
 * \param [out]  dcoeff      1D array of diffusion coefficients   
 * \param [out]  aflux       pointer to 2D array for AMR re-fluxing
 *                           operations
 * \param [in]   dt          the current time-step                            
 * \param [in]   beg,end     initial and final interface indices
 * \param [in]   grid        pointer to Grid structure.
 *********************************************************************** */
{
  int i, j, k, nv;
  double *x1  = grid->x[IDIR],   *x2 = grid->x[JDIR],   *x3  = grid->x[KDIR];
  double *dx1 = grid->dx[IDIR], *dx2 = grid->dx[JDIR], *dx3  = grid->dx[KDIR];
  double *x1p = grid->xr[IDIR], *x2p = grid->xr[JDIR],  *x3p = grid->xr[KDIR];
  double *x1m = grid->xl[IDIR], *x2m = grid->xl[JDIR],  *x3m = grid->xl[KDIR];
  double *s   = grid->s, *sp = grid->sp;
  double A, dtdV, wp, w;
  double rhs[NVAR];  
  static double **ViF, **ViS, **fxA, **src;
  intList var_list;
  #if HAVE_ENERGY 
  var_list.nvar = 4;
  var_list.indx[i=0] = MX1;
  var_list.indx[++i] = MX2;
  var_list.indx[++i] = MX3;
  var_list.indx[++i] = ENG;
  #else
  var_list.nvar = 3;
  var_list.indx[i=0] = MX1;
  var_list.indx[++i] = MX2;
  var_list.indx[++i] = MX3;
  #endif

/* --------------------------------------------------------
   0. Initialize flux and src to zero
   --------------------------------------------------------- */

  if (ViF == NULL){
    ViF     = ARRAY_2D(NMAX_POINT, NVAR, double);
    ViS     = ARRAY_2D(NMAX_POINT, NVAR, double); 
    fxA     = ARRAY_2D(NMAX_POINT, NVAR, double); 
    src     = ARRAY_2D(NMAX_POINT, NVAR, double); 
  }

  for (i = beg; i <= end; i++) NVAR_LOOP(nv) {
    fxA[i][nv] = src[i][nv] = 0.0;
    ViS[i][nv] = 0.0;  /* This resets ViS[ENG] = 0, since it's not
                        * always done in viscous flux */
  }

  i = g_i; j = g_j; k = g_k;
  ViscousFlux (d, ViF, ViS, dcoeff, beg-1, end, grid);

  #ifdef FARGO
  double **wA = FARGO_Velocity();
  #endif

  if (g_dir == IDIR){

  /* --------------------------------------------------------
     1. Compute fluxes & sources in the X1 direction 
     -------------------------------------------------------- */

    for (i = beg-1; i <= end; i++){
      A = grid->A[IDIR][k][j][i];
      FOR_EACH(nv, &var_list){
        fxA[i][nv] = A*ViF[i][nv];
        src[i][nv] = ViS[i][nv];
      }

    /* -- 1a. Correct energy flux in rotating frame -- */

      #if HAVE_ENERGY && (ROTATING_FRAME == YES)
      #if GEOMETRY == POLAR || GEOMETRY == CYLINDRICAL
      wp = g_OmegaZ*x1p[i];
      #elif GEOMETRY == SPHERICAL
      wp = g_OmegaZ*x1p[i]*s[j];
      #endif
      fxA[i][ENG] += wp*fxA[i][iMPHI];
      #endif 

      #ifdef iMPHI
      fxA[i][iMPHI] *= fabs(x1p[i]);
      #endif
    }

  /* -- 1b. Build rhs in the X1-direction and update -- */

    for (i = beg; i <= end; i++){
      dtdV = dt/grid->dV[k][j][i];
      FOR_EACH(nv, &var_list){
        rhs[nv] = dtdV*(fxA[i][nv] - fxA[i-1][nv]) + dt*src[i][nv];
      }  
      #ifdef iMPHI
      rhs[iMPHI] /= fabs(x1[i]);
      #endif

    /* -- 1c. Correct energy rhs in rotating frame or fargo -- */
  
      #if HAVE_ENERGY
      w = 0.0;

      #ifdef FARGO
      #if GEOMETRY == SPHERICAL
      w = wA[j][i];
      #else
      w = wA[k][i];
      #endif
      #endif

      #if ROTATING_FRAME == YES
      #if GEOMETRY == POLAR || GEOMETRY == CYLINDRICAL
      w += g_OmegaZ*x1[i];
      #elif GEOMETRY == SPHERICAL
      w += g_OmegaZ*x1[i]*s[j];
      #endif
      #endif /* ROTATING_FRAME == YES */

      #if GEOMETRY == CARTESIAN 
      rhs[ENG] -= w*rhs[MX2];
      #else
      rhs[ENG] -= w*rhs[iMPHI];
      #endif
      #endif /* HAVE_ENERGY */
 
    /* -- 1d. Update -- */

      FOR_EACH(nv, &var_list) dU[k][j][i][nv] += rhs[nv];
    }

  }else if (g_dir == JDIR){

  /* --------------------------------------------------------
     2. Compute fluxes & sources in the X2 direction 
     -------------------------------------------------------- */

    for (j = beg-1; j <= end; j++){
      A = grid->A[JDIR][k][j][i];
      FOR_EACH(nv, &var_list){
        fxA[j][nv] = A*ViF[j][nv];
        src[j][nv] = ViS[j][nv];
      }

    /* -- 2a. Correct energy flux in rotating frame -- */

      #if (GEOMETRY == SPHERICAL) && HAVE_ENERGY && (ROTATING_FRAME == YES) 
      wp = g_OmegaZ*x1[i]*sp[j];
      fxA[j][ENG] += wp*fxA[j][iMPHI];
      #endif 

      #if (GEOMETRY == SPHERICAL)
      fxA[j][iMPHI] *= fabs(sp[j]);
      #endif
    }

  /* -- 2b. Build rhs in the X2-direction and update -- */

    for (j = beg; j <= end; j++){
      dtdV = dt/grid->dV[k][j][i];
      FOR_EACH(nv, &var_list){
        rhs[nv] = dtdV*(fxA[j][nv] - fxA[j-1][nv]) + dt*src[j][nv];
      }

      #if (GEOMETRY == SPHERICAL)
      rhs[iMPHI] /= fabs(s[j]);
      #endif

    /* -- 2c. Correct energy rhs in rotating frame or fargo -- */

      #if HAVE_ENERGY
      w = 0.0;

      #ifdef FARGO
      #if GEOMETRY == SPHERICAL
      w = wA[j][i];
      #else
      w = wA[k][i];
      #endif
      #endif

      #if ROTATING_FRAME == YES
      #if GEOMETRY == SPHERICAL
      w += g_OmegaZ*x1[i]*s[j];
      #endif
      #endif /* ROTATING_FRAME == YES */

      #if GEOMETRY == CARTESIAN 
      rhs[ENG] -= w*rhs[MX2];
      #else
      rhs[ENG] -= w*rhs[iMPHI];
      #endif
      #endif /* HAVE_ENERGY */

    /* -- 2d. Update -- */
    
      FOR_EACH(nv, &var_list) dU[k][j][i][nv] += rhs[nv];
    }

  }else if (g_dir == KDIR){

  /* --------------------------------------------------------
     3a. Compute fluxes & sources in the X3 direction 
     -------------------------------------------------------- */

    for (k = beg-1; k <= end; k++){
      A = grid->A[KDIR][k][j][i];
      FOR_EACH(nv, &var_list){
        fxA[k][nv] = A*ViF[k][nv];
        src[k][nv] = ViS[k][nv];
      }
    }

  /* -- 3b. Build rhs in the X3-direction -- */

    for (k = beg; k <= end; k++){
      dtdV = dt/grid->dV[k][j][i];

      FOR_EACH(nv, &var_list){
        rhs[nv] = dtdV*(fxA[k][nv] - fxA[k-1][nv]) + dt*src[k][nv];
      }  

      #if HAVE_ENERGY
      w = 0.0;

      #ifdef FARGO
      #if GEOMETRY == SPHERICAL
      w = wA[j][i];
      #else
      w = wA[k][i];
      #endif
      #endif

      #if GEOMETRY == CARTESIAN 
      rhs[ENG] -= w*rhs[MX2];
      #else
      rhs[ENG] -= w*rhs[iMPHI];
      #endif
      
      #endif /* HAVE_ENERGY */

      FOR_EACH(nv, &var_list)  dU[k][j][i][nv] += rhs[nv];
    }
  }

/* --------------------------------------------------------
   4. Store AMR fluxes
   -------------------------------------------------------- */

  #ifdef CHOMBO
  StoreAMRFlux (ViF, aflux, -1, MX1, MX3, beg-1, end, grid);
  #if HAVE_ENERGY
  StoreAMRFlux (ViF, aflux, -1, ENG, ENG, beg-1, end, grid);
  #endif
  #endif
}
