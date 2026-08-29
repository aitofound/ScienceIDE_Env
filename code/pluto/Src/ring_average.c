/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Ring-average method to treat singularity at a polar axis

  Implement the ring-average technique by Zhang et al. (2019) JCP, to
  remove the severe time step restriction for multi-dimensional
  curvilinear finite-volume MHD solvers near a polar axis.
  The Ring Average technique is implemented as a post-processing step
  and thus requires no changes to the existing data structure,
  grid definition or numerical methods in the original MHD solver.

  \b Reference:
    - "Conservative averaging-reconstruction techniques (Ring Average)
       for 3-D finite-volume MHD solvers with axis singularity",
       Zhang et al, JCP (2019) 376, 276-294
       
  \authors A. Mignone (andrea.mignone@unito.it)
  \date    Jun 16, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if RING_AVERAGE > 1


/* ********************************************************************* */
void RingAverageCons(Data *d, Grid *grid)
/*!
 * Average conservative variable inside a chunk.
 *
 *********************************************************************** */
{
  int    nv, i,j,k;
  int    *csize = grid->ring_av_csize;
  double uav[NVAR], dVav;
  double ***dV = grid->dV;

/* ----------------------------------------------
   0. Make sure phi grid is a power of 2 and that
      nphi is dividable by RING_AVERAGE
   ---------------------------------------------- */

  #if GEOMETRY == POLAR
  if (csize[IBEG] == 1) return;  /* Not a physical boundary */
  #elif GEOMETRY == SPHERICAL
  if (   csize[JBEG] == 1
      && csize[JEND] == 1) return;  /* Not a physical boundary */
  #else
    printLog ("! RING_AVERAGE cannot be used in this geometry \n");
    QUIT_PLUTO(1);
  #endif

/* ----------------------------------------------
   1. Average conservative variables over all
      rings. Average is done only when
      chunk size > 1.
   ---------------------------------------------- */

  #if GEOMETRY == POLAR
  i = IBEG;  /* 1st ring */
  while (csize[i] > 1){
    int j1;
    for (k = KBEG; k <= KEND; k++){
    for (j = JBEG; j <= JEND; j += csize[i]){

      dVav = 0.0;
      NVAR_LOOP(nv) uav[nv] = 0.0;
      for (j1 = j; j1 < j+csize[i]; j1++) {
        dVav += dV[k][j1][i];
        NVAR_LOOP(nv) uav[nv] += d->Uc[k][j1][i][nv]*dV[k][j1][i];
      }

    /* ------------------------------------------
       Replace all zones with average value
       ------------------------------------------ */

      for (j1 = j; j1 < j+csize[i]; j1++) {
        NVAR_LOOP(nv) d->Uc[k][j1][i][nv] = uav[nv]/dVav;
      }
    }}
    i++;   /* Proceed to next ring */
  }
  #elif GEOMETRY == SPHERICAL
  j = JBEG;    /* 1st ring, North pole */
  while (csize[j] > 1){
    int k1;
    for (i = IBEG; i <= IEND; i++){
    for (k = KBEG; k <= KEND; k += csize[j]){

      dVav = 0.0;
      NVAR_LOOP(nv) uav[nv] = 0.0;
      for (k1 = k; k1 < k+csize[j]; k1++) {
        dVav += dV[k1][j][i];
        NVAR_LOOP(nv) uav[nv] += d->Uc[k1][j][i][nv]*dV[k1][j][i];
      }

    /* ------------------------------------------
       Replace all zones with average value
       ------------------------------------------ */

      for (k1 = k; k1 < k+csize[j]; k1++) {
        NVAR_LOOP(nv) d->Uc[k1][j][i][nv] = uav[nv]/dVav;
      }
    }}
    j++;   /* Proceed to next ring */
  }

  j = JEND;    /* 1st ring, south pole */
  while (csize[j] > 1){
    int k1;
    for (i = IBEG; i <= IEND; i++){
    for (k = KBEG; k <= KEND; k += csize[j]){

      dVav = 0.0;
      NVAR_LOOP(nv) uav[nv] = 0.0;
      for (k1 = k; k1 < k+csize[j]; k1++) {
        dVav += dV[k1][j][i];
        NVAR_LOOP(nv) uav[nv] += d->Uc[k1][j][i][nv]*dV[k1][j][i];
      }

    /* ------------------------------------------
       Replace all zones with average value
       ------------------------------------------ */

      for (k1 = k; k1 < k+csize[j]; k1++) {
        NVAR_LOOP(nv) d->Uc[k1][j][i][nv] = uav[nv]/dVav;
      }
    }}
    j--;   /* Proceed to next ring */
  }
  #endif

#if 0
{
  RBox box;
  static double *q;
  if ( q == NULL ) q = ARRAY_1D(NMAX_POINT, double);

  RBoxDefine (IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &box);

  ConsToPrim3D (d->Uc, d->Vc, d->flag, &box, grid);
  i = IBEG;  /* 1st ring */
  while (csize[i] > 1){
    g_i = i; 
    NVAR_LOOP(nv){
      for (k = KBEG; k <= KEND; k++){
        JDOM_LOOP(j) q[j] = d->Vc[nv][k][j][i];

        RingAverageReconstructNew(q, JBEG-1, JEND+1, grid);
        JDOM_LOOP(j) d->Vc[nv][k][j][i] = q[j];
      }
    }
    i++;
  }
  PrimToCons3D (d->Vc, d->Uc, &box, grid);
}
#endif
}

/* ********************************************************************* */
void RingAverageReconstruct (Sweep *sweep, int beg, int end, Grid *grid)
/*
 Example for NPHI = 2:

  j  = [0,1][2,3][4,5][6,7]
  ja =  0,1, 2    3    4

  ja = (j-JBEG)/NPHI + JBEG
 
restart;
A := 3*(Q[L] - Q[R] - 2*Q[c]):
B := 2*(3*Q[c]  - Q[R] - 2*Q[L]):
C := Q[L]:
Qr := k -> A/N^2*(3*k*(k-1) + 1) + B/(2*N)*(2*k-1) + C;
Qr(1);


restart;
P := x -> A*x^2 + B*x + C;
eq1 := P(0) = Q[m];
eq2 := P(1) = Q[p];
eq3 := int(P(x), x=0..1) = Q[c];
assign(solve ({eq1, eq2, eq3}, {A,B,C}));

x := (k-1/2)/N;
Qr := k -> P((k-1/2)/N);

 *********************************************************************** */
{
  int nv;
  int i, j, k, ja;
  int chunk_size;   /* Size of a single chunk */ 
  int nphi;         /* No. of zones in the phi direction (original grid) */
  int cbeg, cend;   /* Initial and final active indices on chunked grid  */  
  int dbeg, dend;   /* Initial and final active indices on original grid */ 
  int nchunks;      /* Number of chunks (= grid size on reduced grid) */
  int ngh  = GetNghost();
  int imax;
  State *stateC = &(sweep->stateC);
  State *stateL = &(sweep->stateL);
  State *stateR = &(sweep->stateR);

  double dvap, dvam, dva;
  double **v  = stateC->v;
  double **vp = stateL->v;
  double **vm = stateR->v-1;
  double **up = stateL->u;
  double **um = stateR->u-1;
  static double **va, **vap, **vam;

  #if RING_AVERAGE_REC == 1
  return;
  #endif

/* ----------------------------------------------
   0a. Memory allocation
   ---------------------------------------------- */

  if (va == NULL){
    va  = ARRAY_2D(NMAX_POINT, NVAR, double);
    vap = ARRAY_2D(NMAX_POINT, NVAR, double);
    vam = ARRAY_2D(NMAX_POINT, NVAR, double);
  }

/* ----------------------------------------------
   1a. Find the number of chunks on this ring
   ---------------------------------------------- */

  #if GEOMETRY == POLAR
  chunk_size = grid->ring_av_csize[g_i];
  dbeg = JBEG;
  dend = JEND;
  nphi = NX2;
  #elif GEOMETRY == SPHERICAL
  chunk_size = grid->ring_av_csize[g_j];
  dbeg = KBEG;
  dend = KEND;
  nphi = NX3;
  #endif
  nchunks = nphi/chunk_size; //  >> (NPHI_AVERAGE - (g_i-IBEG));
  cbeg = dbeg;
  cend = dbeg + nchunks - 1;

  if (chunk_size == 1) return; /* No need to reconstruct */

/*
print (">> AverageReconstruct: i = %d, chunk_size = %d, nphi = %d, abeg, aend = %d, %d\n",
         g_i, chunk_size, nphi, abeg, aend);
*/

/* ----------------------------------------------
   1b. Assign chunk averages on va;
   ---------------------------------------------- */

#if RECONSTRUCT_4VEL == YES
  ConvertTo4vel (v, beg-1, end+1);
#endif

  for (j = dbeg; j <= dend; j += chunk_size){
    ja = (j - dbeg)/chunk_size + dbeg;
    NVAR_LOOP(nv) va[ja][nv] = v[j][nv];    
  }

/* ----------------------------------------------
   1c. Set periodic b.c.
   ---------------------------------------------- */

  for (j = 0; j < cbeg; j++){
    NVAR_LOOP(nv) va[j][nv] = va[j+nchunks][nv];
  }
  for (j = cend+1; j <= cend + ngh; j++){
    NVAR_LOOP(nv) va[j][nv] = va[j-nchunks][nv];
  }

/* ----------------------------------------------
   2. Reconstruct on reduced grid
   ---------------------------------------------- */

#if RING_AVERAGE_REC == 1
  for (j = cbeg-1; j <= cend+1; j++){
    NVAR_LOOP(nv){
      vap[j][nv] = va[j][nv];
      vam[j][nv] = va[j][nv];
    }
  }
#elif RING_AVERAGE_REC == 2 

  for (j = cbeg-1; j <= cend+1; j++){
    NVAR_LOOP(nv){
      dvap = va[j+1][nv] - va[j][nv];
      dvam = va[j][nv] - va[j-1][nv];
      dva  = VANLEER_LIMITER(dvap, dvam);
      vap[j][nv] = va[j][nv] + 0.5*dva;
      vam[j][nv] = va[j][nv] - 0.5*dva;
    }
  }

#elif RING_AVERAGE_REC == 5
  static double *qfwd, *qbck;
  if (qfwd == NULL){
    qfwd = ARRAY_1D(NMAX_POINT, double);
    qbck = ARRAY_1D(NMAX_POINT, double);  
  }

  int nphi_tot = nchunks + 2*ngh;
  NVAR_LOOP(nv){
    for (j = 0; j < nphi_tot; j++){
      qfwd[j] = va[j][nv];
      qbck[j] = va[nphi_tot-j-1][nv];
    }

    for (j = cbeg-1; j <= cend; j++){
      int jp = j;
      int jm = cend - (j-cbeg);
      vap[j][nv] = MP5_Reconstruct (qfwd, 0.0, jp);
      vam[j][nv] = MP5_Reconstruct (qbck, 0.0, jm);
    }
  }

#else
  #error Invalid RING_AVERAGE_REC value
#endif

/* ----------------------------------------------
   3. Assign values on original grid
   ---------------------------------------------- */

  double A, B, C;
  for (j = dbeg; j <= dend; j++){
    
    ja = (j - dbeg)/chunk_size + dbeg;
    k  = (j - dbeg)%chunk_size + 1;  /* Local grid index inside chunk,  1 <= k <= chunk_size */
//printf ("j = %d, ja = %d, k = %d\n",j, ja, k);


if (k > chunk_size || k < 1){
  printf ("ERROR \n");
  printf ("j = %d, ja = %d, k = %d\n",j, ja, k);
  QUIT_PLUTO(1);
}
    double xp = k/(double)chunk_size;
    double xm = (k-1.0)/(double)chunk_size;
    NVAR_LOOP(nv){
      A =  3.0*((vap[ja][nv] + vam[ja][nv])  - 2.0*va[ja][nv]);
      B = -4.0*vam[ja][nv] - 2.0*vap[ja][nv] + 6.0*va[ja][nv];
      C = vam[ja][nv];
      vp[j][nv] = A*xp*xp + B*xp + C;
      vm[j][nv] = A*xm*xm + B*xm + C;
    }
    #if (PHYSICS == RHD) || (PHYSICS == RMHD) || (PHYSICS == ResRMHD)
    VelocityLimiter (v[j], vp[j], vm[j]);
    #endif
    #if RADIATION
    RadFluxLimFlatten (v[j], vp[j], vm[j]);
    #endif
  }

/* ----------------------------------------------
   3b. Set periodic b.c. on original grid
   ---------------------------------------------- */

  for (j = 0; j < dbeg; j++){
    NVAR_LOOP(nv) {
      vp[j][nv] = vp[j + nphi][nv];
      vm[j][nv] = vm[j + nphi][nv];
    }
  }
  for (j = dend+1; j <= dend+ngh; j++){
    NVAR_LOOP(nv) {
      vp[j][nv] = vp[j - nphi][nv];
      vm[j][nv] = vm[j - nphi][nv];
    }  
  }

/*
FILE *fp;
fp = fopen("average.dat","w");
for (j = beg; j <= end; j++){
  fprintf (fp, "%d  %f  %f  %f\n",j, v[j][RHO], vm[j][RHO], vp[j][RHO]);
}
fclose(fp);
exit(1);
*/

/* ----------------------------------------------
   4. Convert back to 3-velocity
   ---------------------------------------------- */

#if RECONSTRUCT_4VEL
  ConvertTo3vel (v, beg-1, end+1);
  ConvertTo3vel (vp, beg, end);
  ConvertTo3vel (vm, beg, end);
#endif

/* ----------------------------------------------
   5. Obtain L/R states in conservative variables
   ---------------------------------------------- */

  PrimToCons (vp, up, beg, end);
  PrimToCons (vm, um, beg, end);
}




/* ********************************************************************* */
void RingAverageReconstructNew (double *v, int beg, int end, Grid *grid)
/*
 *
 * Form sub-cell averages
 *

restart;
P := x -> A*x^2 + B*x + C;
eq1 := P(0) = Q[m];
eq2 := P(1) = Q[p];
eq3 := int(P(x), x=0..1) = Q[c];
#assign(solve ({eq1, eq2, eq3}, {A,B,C}));
print ("A,B,C = "): A, B, C;

Q := k -> int(P(x), x=(k-1)/N..k/N)/(1/N);  # Sub-cell average


# -- Verify --
simplify(sum(Q(k), k = 1..N));


coeff(simplify( Q(k)), A);   # Comment the assign statement before
coeff(simplify( Q(k)), B);   # to have more compact expressions
coeff(simplify( Q(k)), C);   #

 *********************************************************************** */
{
  int i, j, k, ja;
  int chunk_size;   /* Size of a single chunk */ 
  int nphi;         /* No. of zones in the phi direction (original grid) */
  int cbeg, cend;   /* Initial and final active indices on chunked grid  */  
  int dbeg, dend;   /* Initial and final active indices on original grid */ 
  int nchunks;      /* Number of chunks (= grid size on reduced grid) */
  int ngh  = GetNghost();
  int imax;

  double dvcp, dvcm, dvc;
  static double *vc, *vcp, *vcm;

  #if RING_AVERAGE_REC == 1
  return;
  #endif

/* ----------------------------------------------
   0a. Memory allocation
   ---------------------------------------------- */

  if (vc == NULL){
    vc  = ARRAY_1D(NMAX_POINT, double);
    vcp = ARRAY_1D(NMAX_POINT, double);
    vcm = ARRAY_1D(NMAX_POINT, double);
  }

/* ----------------------------------------------
   1a. Find the number of chunks on this ring
   ---------------------------------------------- */

  #if GEOMETRY == POLAR
  chunk_size = grid->ring_av_csize[g_i];
  dbeg = JBEG;
  dend = JEND;
  nphi = NX2;
  #elif GEOMETRY == SPHERICAL
  chunk_size = grid->ring_av_csize[g_j];
  dbeg = KBEG;
  dend = KEND;
  nphi = NX3;
  #endif
  nchunks = nphi/chunk_size; //  >> (NPHI_AVERAGE - (g_i-IBEG));
  cbeg = dbeg;
  cend = dbeg + nchunks - 1;

  if (chunk_size == 1) return; /* No need to reconstruct */

/*
print (">> AverageReconstructNew: i = %d, chunk_size = %d, nphi = %d, cbeg, cend = %d, %d\n",
         g_i, chunk_size, nphi, cbeg, cend);
*/

/* ----------------------------------------------
   1b. Assign chunk averages on va;
   ---------------------------------------------- */

  for (j = dbeg; j <= dend; j += chunk_size){
    ja = (j - dbeg)/chunk_size + dbeg;
    vc[ja] = v[j];    
  }

/* ----------------------------------------------
   1c. Set periodic b.c.
   ---------------------------------------------- */

  for (j = 0; j < cbeg; j++){
    vc[j] = vc[j+nchunks];
  }
  for (j = cend+1; j <= cend + ngh; j++){
    vc[j] = vc[j-nchunks];
  }

/* ----------------------------------------------
   2. Reconstruct on reduced grid
   ---------------------------------------------- */

#if RING_AVERAGE_REC == 1
  for (j = cbeg-1; j <= cend+1; j++){
    vcp[j] = vc[j];
    vcm[j] = vc[j];
  }
#elif RING_AVERAGE_REC == 2 

  for (j = cbeg; j <= cend; j++){
    dvcp = vc[j+1] - vc[j];
    dvcm = vc[j] - vc[j-1];
    dvc  = VANLEER_LIMITER(dvcp, dvcm);
    vcp[j] = vc[j] + 0.5*dvc;
    vcm[j] = vc[j] - 0.5*dvc;
  }

#elif RING_AVERAGE_REC == 5
  static double *qfwd, *qbck;
  if (qfwd == NULL){
    qfwd = ARRAY_1D(NMAX_POINT, double);
    qbck = ARRAY_1D(NMAX_POINT, double);  
  }

  int nphi_tot = nchunks + 2*ngh;
  for (j = 0; j < nphi_tot; j++){
    qfwd[j] = vc[j];
    qbck[j] = vc[nphi_tot-j-1];
  }

  for (j = cbeg-1; j <= cend; j++){
    int jp = j;
    int jm = cend - (j - cbeg);
//    vcp[j] = PPM_Reconstruct (qfwd, 0.0, jp);
//    vcm[j] = PPM_Reconstruct (qbck, 0.0, jm);
    vcp[j] = MP5_Reconstruct (qfwd, 0.0, jp);
    vcm[j] = MP5_Reconstruct (qbck, 0.0, jm);
  }

#else
  #error Invalid RING_AVERAGE_REC value
#endif

/* ----------------------------------------------
   3. Assign volume-averaged values on the
      original grid
   ---------------------------------------------- */

  double A, B, C;
  double N  = chunk_size;
  double N2 = N*chunk_size; 

  for (j = dbeg; j <= dend; j++){
    
    ja = (j - dbeg)/chunk_size + dbeg;
    k  = (j - dbeg)%chunk_size + 1;  /* Local grid index inside chunk,  1 <= k <= chunk_size */
//printf ("j = %d, ja = %d, k = %d\n",j, ja, k);


if (k > chunk_size || k < 1){
  printf ("ERROR \n");
  printf ("j = %d, ja = %d, k = %d\n",j, ja, k);
  QUIT_PLUTO(1);
}
    
    A =  3.0*(vcp[ja] + vcm[ja]) - 6.0*vc[ja];
    B = -4.0*vcm[ja] - 2.0*vcp[ja] + 6.0*vc[ja];
    C = vcm[ja];
    v[j] =   A*(6.0*k*(k-1.0) + 2.0)/(6.0*N2)
           + B*(k-0.5)/N + C;
  }

  for (j = 0; j < dbeg; j++){
    v[j] = v[j + nphi];
  }
  for (j = dend+1; j <= dend+ngh; j++){
    v[j] = v[j - nphi];
  }





// Verify
for (j = dbeg; j <= dend; j += chunk_size){
  double vv = 0.0;
  for (int j1 = j; j1 < j+chunk_size; j1++){
    vv += v[j1];
  }
  vv /= chunk_size;

  ja = (j - dbeg)/chunk_size + dbeg;
  if (fabs(vc[ja] - vv) > 1.e-7){
    printf ("! ReconstructNew: vc = %f; vv = %f\n",vc[ja], vv);
    exit(1);
  }
}



/*
FILE *fp;
fp = fopen("average.dat","w");
for (j = beg; j <= end; j++){
  fprintf (fp, "%d  %f  %f  %f\n",j, v[j][RHO], vm[j][RHO], vp[j][RHO]);
}
fclose(fp);
exit(1);
*/

}


/* ********************************************************************* */
void RingAverageSize(Grid *grid)
/*!
 * - Check requirements for using RING_AVERAGE.
 * - Determine the size of chunks on each ring, as a function of
 *   R (in polar) or \theta (spherical).
 *
 *********************************************************************** */
{
  int i,j, k, csize;

  #if GEOMETRY == POLAR

/* --------------------------------------------------------
   1a. Check requirements:
       - NX2 must be a power of 2
       - NX2 must be divisible by RING_AVERAGE
       - phi direction cannot be parallel
   -------------------------------------------------------- */

  //j = NX2;
  //while (j != 1) {
  //  if (j%2 != 0) {
  //    printLog ("! RingAverageCons(): NX2 is not a power of 2\n");
  //    QUIT_PLUTO(1);
  //  }
  //  j /= 2;  
  //}

  if (NX2%RING_AVERAGE != 0){
    print ("! RingAverageCons(): NX2 not divisible by RING_AVERAGE\n");
    QUIT_PLUTO(1);
  }

  if (grid->nproc[JDIR] > 1){
    print ("! RingAverageSize(): RING_AVERAGE requires nprocs(phi) = 1\n");
    QUIT_PLUTO(1);
  }

/* --------------------------------------------------------
   1b. Determine chunk size
   -------------------------------------------------------- */

  if (grid->lbound[IDIR] == POLARAXIS){
    csize = RING_AVERAGE;
    for (i = IBEG; i <= IEND; i++){
      grid->ring_av_csize[i] = csize;
      if (csize > 1) csize >>= 1;   /* Same as csize /= 2 */
    }
  }else{
    for (i = IBEG; i <= IEND; i++) grid->ring_av_csize[i] = 1; 
  }

  #elif GEOMETRY == SPHERICAL

/* --------------------------------------------------------
   2a. Check requirements:
       - NX3 must be a power of 2
       - NX3 must be divisible by RING_AVERAGE
       - phi direction cannot be parallel
   -------------------------------------------------------- */

  //k = NX3;
  //while (k != 1) {
  //  if (k%2 != 0) {
  //    print ("! RingAverageCons(): NX3 is not a power of 2\n");
  //    QUIT_PLUTO(1);
  //  }
  //  k /= 2;  
  //}

  if (NX3%RING_AVERAGE != 0){
    print ("! RingAverageCons(): NX3 not divisible by RING_AVERAGE\n");
    QUIT_PLUTO(1);
  }

  if (grid->nproc[KDIR] > 1){
    print ("! RING_AVERAGE requires nprocs(phi) = 1\n");
    QUIT_PLUTO(1);
  }

/* --------------------------------------------------------
   2b. Determine chunk size
   -------------------------------------------------------- */

  /* -- Are close to the north pole ? -- */

  if (grid->lbound[JDIR] == POLARAXIS){
    csize = RING_AVERAGE;
    for (j = JBEG; j <= JEND; j++){    
      grid->ring_av_csize[j] = csize;
      if (csize > 1) csize >>= 1;   /* Same as csizeN /= 2 */
    }
  }else{
    for (j = JBEG; j <= JEND; j++) grid->ring_av_csize[j] = 1;
  }

  /* -- Are close to the south pole ? -- */

  if (grid->rbound[JDIR] == POLARAXIS){
    csize = RING_AVERAGE;
    for (j = JEND; j >= JBEG; j--){    
      grid->ring_av_csize[j] = MAX(grid->ring_av_csize[j], csize);
      if (csize > 1) csize >>= 1;   /* Same as csizeS /= 2 */
    }
  }else{
    for (j = JEND; j >= JBEG; j--) {
      grid->ring_av_csize[j] = MAX(grid->ring_av_csize[j], 1);
    }
  }

//for (j = JBEG; j <= JEND; j++) printf ("j = %d, csize = %d\n", j, grid->ring_av_csize[j]);
//exit(1);
  #endif

}
#endif /* RING_AVERAGE > 1 */
