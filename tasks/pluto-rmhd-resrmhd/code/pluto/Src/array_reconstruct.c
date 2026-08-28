/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Reconstruct an array along a given direction.

  General purpose reconstruction routine using limited linear
  interpolation.
  Given a 3D array q, it reconstruct along the "dir" direction.

  \authors A. Mignone (andrea.mignone@unito.it)\n

  \date    Dec 09, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void ArrayReconstruct(double ***Q, uint16_t *flag, int i, int j, int k, int dir,
                      double *qL, double *qR, int rec, Grid *grid)
/*!
 *********************************************************************** */
{
  double dqp, dqm, dq;
  PLM_Coeffs plm_coeffs;
  int rec0 = (rec/10)*10;
  int ntot;
  static double *q;

  if (q == NULL){
    q = ARRAY_1D(NMAX_POINT, double);
  }

  if (dir == IDIR) {
    ntot = NX1_TOT;
    for (i = 0; i < ntot; i++) q[i] = Q[k][j][i];
  } 
 
  if (dir == JDIR) {
    ntot = NX2_TOT;
    for (j = 0; j < ntot; j++) q[j] = Q[k][j][i];
  }

  if (dir == KDIR) {
    ntot = NX3_TOT;
    for (k = 0; k < ntot; k++) q[k] = Q[k][j][i];
  }

/* ----------------------------------------------
   Flat reconstruction
   ---------------------------------------------- */
  
  if (rec0 == FLAT) {
    for (i = 1; i < ntot-1; i++){
      qL[i]   = q[i];
      qR[i-1] = q[i];
    }
    return;
  }

/* ----------------------------------------------
   Linear reconstruction
   ---------------------------------------------- */
  
  if (rec0 == LINEAR) {
    dqm = q[1] - q[0];
    for (i = 1; i < ntot-1; i++){

      #if SHOCK_FLATTENING == MULTID
      if (flag[i] & FLAG_FLAT) {
        qL[i]   = qR[i-1] = q[i];
        continue;
      }
      #endif
      
      dqp = q[i+1] - q[i];
      dqm = q[i]   - q[i-1];

      if      (rec == FLAT_LIM)    dq = 0.0;
      else if (rec == MINMOD_LIM)  dq = MINMOD_LIMITER(dqp, dqm);
      else if (rec == VANLEER_LIM) dq = VANLEER_LIMITER(dqp, dqm);
      else if (rec == MC_LIM)      dq = MC_LIMITER(dqp, dqm);
      else{
        SET_LIMITER (dq, dqp, dqm, 2.0, 2.0);
      }
      qL[i]   = q[i] + 0.5*dq;
      qR[i-1] = q[i] - 0.5*dq;
      
    }
    return;
  }

/* ----------------------------------------------
   PARABOLIC reconstruction
   ---------------------------------------------- */

  if (rec == PARABOLIC) {
    int beg = 1, end = ntot-2;
    for (i = beg; i <= end; i++){
 
      double dqc, Sm1, Sp1, Sp2, SM;
      double ap, am, qp, qm;

      #if SHOCK_FLATTENING == MULTID
      if (flag[i] & FLAG_FLAT) {
        qL[i]   = q[i];
        qR[i-1] = q[i];
        continue;
      }else if (flag[i] & FLAG_MINMOD) {
        dqp = q[i+1] - q[i];
        dqm = q[i] - q[i-1];
        dq = MINMOD_LIMITER(dqp, dqm);
        qL[i]   = q[i] + 0.5*dq;
        qR[i-1] = q[i] - 0.5*dq;
        continue;
      }
      #endif
  
      qp = (    -q[i-1] + 5.0*q[i] + 2.0*q[i+1])/6.0;
      qm = ( 2.0*q[i-1] + 5.0*q[i] -     q[i+1])/6.0;
      
      dqp = qp - q[i];
      dqm = qm - q[i];
  
      dq  = q[i+1] - q[i];
      qp  = q[i] + MINMOD_LIMITER(dqp, dq);
      
      dq  = q[i] - q[i-1];
      qm  = q[i] + MINMOD_LIMITER(dqm, -dq);
       
      dqp = qp - q[i];
      dqm = qm - q[i];
  
      if (dqp*dqm >= 0.0) dqp = dqm = 0.0;
      else{
        if      (fabs(dqp) >= 2.0*fabs(dqm)) dqp = -2.0*dqm;
        else if (fabs(dqm) >= 2.0*fabs(dqp)) dqm = -2.0*dqp;
      }
      qL[i]   = qp = q[i] + dqp; 
      qR[i-1] = qm = q[i] + dqm;
  
    }
    return;
  }

/* ----------------------------------------------
   WENO3 reconstruction
   ---------------------------------------------- */

  if (rec == WENO3) {

    double *qfwd = q;
    static double *qbck;
    if (qbck == NULL) qbck = ARRAY_1D(NMAX_POINT, double);

    for (i = 0; i < ntot; i++) qbck[i] = qfwd[ntot-i-1];
  
    int beg = 1, end = ntot-2;
    for (i = beg; i <= end; i++){
      int ip = i;
      int im = end-(i-beg);
      #if SHOCK_FLATTENING == MULTID
      if (flag[i] & FLAG_FLAT) {
        qL[i]   = qfwd[i];
        qR[i-1] = qfwd[i];
        continue;
      }else if (flag[i] & FLAG_MINMOD) {
        dqp = qfwd[i+1] - qfwd[i];
        dqm = qfwd[i] - qfwd[i-1];
        dq = MINMOD_LIMITER(dqp, dqm);
        qL[i]   = qfwd[i] + 0.5*dq;
        qR[i-1] = qfwd[i] - 0.5*dq;
        continue;
      }
      #endif
  
      qL[i]   = WENO3_Reconstruct (qfwd, grid->dx[dir][i], ip);
      qR[i-1] = WENO3_Reconstruct (qbck, grid->dx[dir][i], im);
    }
    return;
  }

/* ----------------------------------------------
   WENOZ reconstruction
   ---------------------------------------------- */

#if RECONSTRUCTION == WENOZ
  if (rec == WENOZ) {

    int beg = 2, end = ntot-3;
    for (i = beg; i <= end; i++){
      #if SHOCK_FLATTENING == MULTID
      if (flag[i] & FLAG_FLAT) {
        qL[i]   = q[i];
        qR[i-1] = q[i];
        continue;
      }else if (flag[i] & FLAG_MINMOD) {
        dqp = q[i+1] - q[i];
        dqm = q[i] - q[i-1];
        dq = MINMOD_LIMITER(dqp, dqm);
        qL[i]   = q[i] + 0.5*dq;
        qR[i-1] = q[i] - 0.5*dq;
        continue;
      }
      #endif

      qL[i]   = WENOZ_States (q, i, +1);
      qR[i-1] = WENOZ_States (q, i, -1);
    }
    return;
  }
#endif
  
/* ----------------------------------------------
   MP5 reconstruction
   ---------------------------------------------- */

#if RECONSTRUCTION == MP5
  if (rec == MP5) {

    int beg = 2, end = ntot-3;
    for (i = beg; i <= end; i++){
      #if SHOCK_FLATTENING == MULTID
      if (flag[i] & FLAG_FLAT) {
        qL[i]   = q[i];
        qR[i-1] = q[i];
        continue;
      }else if (flag[i] & FLAG_MINMOD) {
        dqp = q[i+1] - q[i];
        dqm = q[i] - q[i-1];
        dq = MINMOD_LIMITER(dqp, dqm);
        qL[i]   = q[i] + 0.5*dq;
        qR[i-1] = q[i] - 0.5*dq;
        continue;
      }
      #endif

      qL[i]   = MP5_States (q, i, +1);
      qR[i-1] = MP5_States (q, i, -1);
    }
    return;
  }
#endif
  
  printLog ("! ArrayReconstruct(): invalid rconstruction type (rec = %d)\n",
               rec);
  QUIT_PLUTO(1);

}
