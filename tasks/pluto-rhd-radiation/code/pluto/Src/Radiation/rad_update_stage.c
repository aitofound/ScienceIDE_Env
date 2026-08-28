/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief   Single stage explicit integration for the radiation fields.

  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  
  \date    Jan 7, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

//#if TRAPEZOIDAL_RULE == NO
#if RADIATION_NR
/* ********************************************************************* */
void UpdateRadStage(Data *d, Data_Arr Uc, Data_Arr Us, double **aflux,
                 double dt, timeStep *Dts_rad, Grid *grid)
/*!
 * 
 * \param [in,out]  d        pointer to PLUTO Data structure
 * \param [in,out]  Uc       zone-centered data array containing conservative variables
 *                           at the previous time step to be updated
 * \param [out]     aflux    interface fluxes needed for refluxing operations 
 *                           (only with AMR)
 * \param [in]      dt       the time step for the current update step
 * \param [in,out]  Dts_rad  pointer to time step structure
 * \param [in]      grid     pointer to Grid structure
 *********************************************************************** */
{
  int  i, j, k;
  int  nv, dir, beg_dir, end_dir;
  int ntot, nbeg, nend;
  int  *ip;

  static Sweep sweep;
  State *stateC = &(sweep.stateC);
  State *stateL = &(sweep.stateL);

  double *inv_dl, dl2;
  static double ***C_dt;
  RBox  sweepBox;

  beg_dir = 0;
  end_dir = DIMENSIONS-1;

/* --------------------------------------------------------
   0. Allocate memory & reset arrays.
      C_dt is an array used to store the inverse time
      step for the hyperbolic solve.
   -------------------------------------------------------- */

  if (stateC->v == NULL){
    MakeState (&sweep);    
    #if DIMENSIONS > 1
    C_dt = ARRAY_3D(NX3_MAX, NX2_MAX, NX1_MAX, double);
    #endif 
  }

#if DIMENSIONS > 1
  if (g_intStage == 1){
    KTOT_LOOP(k) JTOT_LOOP(j){
      memset ((void *)C_dt[k][j],'\0', NX1_TOT*sizeof(double));
    }
  }
#endif 

/* --------------------------------------------------------
   1. Update conservative solution array with hyperbolic 
      terms only.
   -------------------------------------------------------- */

  for (dir = beg_dir; dir <= end_dir; dir++){

    g_dir = dir;  

    #if !INCLUDE_JDIR
    if (g_dir == JDIR) continue;
    #endif

  /* -- 2b. Set integration box for current update -- */

    RBoxDefine(IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &sweepBox);
    RBoxSetDirections (&sweepBox, g_dir);
    SetVectorIndices (g_dir);

    #if (defined STAGGERED_MHD)
    #if    (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == CT_FLUX) \
        || (CT_EMF_AVERAGE == UCT_HLL)  || (CT_EMF_AVERAGE == CT_MAXWELL) \
        || (CT_EMF_AVERAGE == UCT_GFORCE) 
    int ngh = GetNghost();
    RBoxEnlarge (&sweepBox, ngh*(g_dir != IDIR),
                            ngh*(g_dir != JDIR),
                            ngh*(g_dir != KDIR));
    #else
    RBoxEnlarge (&sweepBox, g_dir != IDIR, g_dir != JDIR, g_dir != KDIR);
    #endif
    #endif
    ResetState(d, &sweep, grid);

    ntot = grid->np_tot[g_dir];
    nbeg = *sweepBox.nbeg;
    nend = *sweepBox.nend;
    BOX_TRANSVERSE_LOOP(&sweepBox, k,j,i){
      ip  = sweepBox.n;
      g_i = i;  g_j = j;  g_k = k;
      for ((*ip) = 0; (*ip) < ntot; (*ip)++) {
        NVAR_LOOP(nv) stateC->v[*ip][nv] = d->Vc[nv][k][j][i];
        sweep.flag[*ip] = d->flag[k][j][i];
      }
      
      CheckNaN (stateC->v, 0, ntot-1,0);
      States  (&sweep, nbeg - 1, nend + 1, grid);
      d->radiationRiemannSolver (&sweep, nbeg - 1, nend, Dts_rad->cmax, grid);
    
      RadRightHandSide (&sweep, Dts_rad, nbeg, nend, dt, grid);
    
    /* -- Update:  U = U + dt*R -- */

      for ((*ip) = nbeg; (*ip) <= nend; (*ip)++) { 
        NVAR_LOOP(nv) Uc[k][j][i][nv] += sweep.rhs[*ip][nv];
      }
      // #ifdef CHOMBO // To be replaced with new AMR
      // for ((*ip) = nbeg-1; (*ip) <= nend; (*ip)++) { 
      //   NMHD_LOOP(nv) sweep.flux[*ip][nv] = 0.0;
      // }
      // StoreAMRFlux (sweep.flux, aflux, 0, 0, NVAR-1, nbeg-1, nend, grid);
      // #endif

    /* -- Compute inverse hyperbolic time step - */

      #if DIMENSIONS > 1
      if (g_intStage == 1){
        double q = 1.0;
        inv_dl = GetInverse_dl(grid);
        #if (RING_AVERAGE > 1) && (GEOMETRY == POLAR)
        if (g_dir == JDIR) q = 1.0/grid->ring_av_csize[g_i];
        #elif (RING_AVERAGE > 1) && (GEOMETRY == SPHERICAL)
        if (g_dir == KDIR) q = 1.0/grid->ring_av_csize[g_j];
        #endif
        
        for ((*ip) = nbeg; (*ip) <= nend; (*ip)++) {
          C_dt[k][j][i] += 0.5*(Dts_rad->cmax[(*ip)-1] + Dts_rad->cmax[*ip])*inv_dl[*ip]*q;
        }
      }
      #else
      inv_dl = GetInverse_dl(grid);
      for ((*ip) = nbeg-1; (*ip) <= nend; (*ip)++) { 
        Dts_rad->invDt_hyp = MAX(Dts_rad->invDt_hyp, Dts_rad->cmax[*ip]*inv_dl[*ip]);
      }
      #endif

    }

  }

/* -------------------------------------------------------------------
   2. Reduce dt for dimensionally unsplit schemes.
   ------------------------------------------------------------------- */

#if DIMENSIONS > 1
  if (g_intStage == 1){
    DOM_LOOP(k,j,i) Dts_rad->invDt_hyp = MAX(Dts_rad->invDt_hyp, C_dt[k][j][i]);
    Dts_rad->invDt_hyp /= (double)(INCLUDE_IDIR + INCLUDE_JDIR + INCLUDE_KDIR);
  }
#endif
}
#endif
//#endif /* TRAPEZOIDAL_RULE == NO */
