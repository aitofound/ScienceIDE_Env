/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief 3D wrapper for conservative/primitive conversion.

  Provide 3D wrappers to the standard 1D conversion functions
  ConsToPrim() and PrimToCons().

  \authors A. Mignone (andrea.mignone@unito.it)
  \date    Jan 27, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
int ConsToPrim3D (Data_Arr U, Data_Arr V, uint16_t ***flag, RBox *box, Grid *grid)
/*!
 *  Convert a 3D array of conservative variables \c U to
 *  an array of primitive variables \c V.
 *  Note that <tt>[nv]</tt> is the fastest running index for \c U 
 *  while it is the slowest running index for \c V.
 *
 * \param [in]     U      pointer to 3D array of conserved variables,
 *                        with array indexing <tt>[k][j][i][nv]</tt>
 * \param [out]    V      pointer to 3D array of primitive variables,
 *                        with array indexing <tt>[nv][k][j][i]</tt>
 * \param [in,out] flag   pointer to 3D array of flags.
 * \param [in]     box    pointer to RBox structure containing the domain
 *                        portion over which conversion must be performed.
 *
 *********************************************************************** */
{
  int   i, j, k, nv;
  int   err = 0, err_loc;
  int   ibeg, iend, jbeg, jend, kbeg, kend;
  int   current_dir;
  static double **v, **u;

  if (v == NULL){
    v = ARRAY_2D(NMAX_POINT, NVAR, double);
    u = ARRAY_2D(NMAX_POINT, NVAR, double);
  }

/* ----------------------------------------------
    Save current sweep direction and by default,
    perform the conversion along X1 stripes
   ---------------------------------------------- */

  current_dir = g_dir; 
  g_dir = IDIR;
  
/* -----------------------------------------------
    Set (beg,end) indices in ascending order for
    proper call to ConsToPrim()
   ----------------------------------------------- */

  ibeg = (box->ibeg <= box->iend) ? (iend=box->iend, box->ibeg):
                                    (iend=box->ibeg, box->iend);
  jbeg = (box->jbeg <= box->jend) ? (jend=box->jend, box->jbeg):
                                    (jend=box->jbeg, box->jend);
  kbeg = (box->kbeg <= box->kend) ? (kend=box->kend, box->kbeg):
                                    (kend=box->kbeg, box->kend);

  for (k = kbeg; k <= kend; k++){ g_k = k;
  for (j = jbeg; j <= jend; j++){ g_j = j;
    #if (defined CHOMBO) && (COOLING == MINEq || COOLING == H2_COOL)
    if (g_intStage == 1) {
      for (i = ibeg; i <= iend; i++) NormalizeIons(U[k][j][i]);
    }  
    #endif

    #ifdef HIGH_ORDER
    err_loc = 0;
    for (i = ibeg; i <= iend; i++){  g_i = i;
      int status;
      status = ConsToPrimLoc (U[k][j][i], v[i], flag[k][j] + i);
      WARNING(
        if (status != 0) Where(i,NULL);
      )
      err_loc = MAX(status, err_loc);
    }
    #else
    err_loc = ConsToPrim (U[k][j], v, ibeg, iend, flag[k][j]);
    #endif

    err = MAX(err, err_loc);
    for (i = ibeg; i <= iend; i++) {
      NVAR_LOOP(nv) V[nv][k][j][i] = v[i][nv];
     
      /////// DEBUG
/*      
      if (flag[k][j][i] & FLAG_CONS2PRIM_FAIL){
        printLog ("! ConsToPrim3D(): failure in zone (i,j,k) = (%d, %d, %d)\n",i,j,k);
        printLog ("  g_intStage      = %d\n", g_intStage);
        printLog ("  proc active     = [%d, %d] [%d, %d] [%d, %d]\n",
                   IBEG, IEND, JBEG, JEND, KBEG, KEND);
        printLog ("  FLAG_FLAT       = %d\n", flag[k][j][i] & FLAG_FLAT);
        printLog ("  FLAG_MINMOD     = %d\n", flag[k][j][i] & FLAG_MINMOD);
        printLog ("  FLAG_HLL        = %d\n", flag[k][j][i] & FLAG_HLL);
        printLog ("  FLAG_ENTROPY    = %d\n", flag[k][j][i] & FLAG_ENTROPY);
        printLog ("  FLAG_SPLIT_CELL = %d\n", flag[k][j][i] & FLAG_SPLIT_CELL);
        printLog ("  FLAG_INTERNAL_BOUNDARY = %d\n", flag[k][j][i] & FLAG_INTERNAL_BOUNDARY);
        printLog ("  FLAG_CONS2PRIM_FAIL = %d\n", flag[k][j][i] & FLAG_CONS2PRIM_FAIL);
        
        NVAR_LOOP(nv){
          printLog ("  U[%d] = %8.3e\n", nv, U[k][j][i][nv]);
        }
        QUIT_PLUTO(1);
      }
*/        
    }

    #if QUIT_ON_FIX == YES
    for (i = ibeg; i <= iend; i++) {
/*
      if ( (flag[k][j][i] & FLAG_NEGATIVE_DENSITY)  ||
           (flag[k][j][i] & FLAG_NEGATIVE_ENERGY)   ||
           (flag[k][j][i] & FLAG_NEGATIVE_PRESSURE) ){
*/
      if ( flag[k][j][i] & FLAG_CONS2PRIM_FAIL){
        printLog ("! Mappers3D(): failure [QUIT_ON_FIX enabled]\n");
        printLog ("  g_stepNumber = %d\n",g_stepNumber);
        printLog ("  g_intStage   = %d\n",g_intStage);

        QUIT_PLUTO(1);  
      }
    }
    #endif    
    
  }}

  g_dir = current_dir;

  #ifdef PARALLEL
  int err_glob;
  MPI_Allreduce (&err, &err_glob, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
  err = err_glob;
  #endif

  return err;
}
/* ********************************************************************* */
void PrimToCons3D (Data_Arr V, Data_Arr U, RBox *box, Grid *grid)
/*!
 *  Convert a 3D array of primitive variables \c V  to
 *  an array of conservative variables \c U.
 *  Note that <tt>[nv]</tt> is the fastest running index for \c U 
 *  while it is the slowest running index for \c V.
 *
 * \param [in]    V     pointer to 3D array of primitive variables,
 *                      with array indexing <tt>[nv][k][j][i]</tt>
 * \param [out]   U     pointer to 3D array of conserved variables,
 *                      with array indexing <tt>[k][j][i][nv]</tt>
 * \param [in]    box   pointer to RBox structure containing the domain
 *                      portion over which conversion must be performed.
 *
 *********************************************************************** */
{
  int   i, j, k, nv;
  int   ibeg, iend, jbeg, jend, kbeg, kend;
  int   current_dir;
  static double **v, **u;

  if (v == NULL) {
    v = ARRAY_2D(NMAX_POINT, NVAR, double);
    u = ARRAY_2D(NMAX_POINT, NVAR, double);
  }

  current_dir = g_dir; /* save current direction */
  g_dir = IDIR;

/* -----------------------------------------------
    Set (beg,end) indices in ascending order for
    proper call to ConsToPrim()
   ----------------------------------------------- */

  ibeg = (box->ibeg <= box->iend) ? (iend=box->iend, box->ibeg):(iend=box->ibeg, box->iend);
  jbeg = (box->jbeg <= box->jend) ? (jend=box->jend, box->jbeg):(jend=box->jbeg, box->jend);
  kbeg = (box->kbeg <= box->kend) ? (kend=box->kend, box->kbeg):(kend=box->kbeg, box->kend);

  for (k = kbeg; k <= kend; k++){ g_k = k;
  for (j = jbeg; j <= jend; j++){ g_j = j;
    for (i = ibeg; i <= iend; i++) {
      NVAR_LOOP(nv) v[i][nv] = V[nv][k][j][i];
    }
    #ifdef HIGH_ORDER
    for (i = ibeg; i <= iend; i++) {
      PrimToConsLoc(v[i],U[k][j][i]);
    }
    #else 
    PrimToCons (v, U[k][j], ibeg, iend);
    #endif
  }}
  g_dir = current_dir; /* restore current direction */

}

#if 0
/* ********************************************************************* */
void SolutionFix(Data_Arr U, int i, int j, int k, Grid *grid)
/*
 * Replace flawed zones with the averaged of neighbours
 *********************************************************************** */
{
  int d;
  int  par_dim[3] = {0, 0, 0};
  int  *lbound = grid->lbound;
  int  *rbound = grid->rbound;
  double uav, dVav;
  static char   ***wb;    /* Weights 1 (active zone) or 0 (physical b.c.) */
  static double ***Uloc;  /* Boundary-filled conservative array */

  DIM_EXPAND(par_dim[0] = grid->nproc[IDIR] > 1;  ,
           par_dim[1] = grid->nproc[JDIR] > 1;  ,
           par_dim[2] = grid->nproc[KDIR] > 1;)
  
/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */
  
  if (Ulox == 0){    
    Uloc = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  
    wb   = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, char);  
  }
  
/* --------------------------------------------------------
   1. Exchange boundary conditions & determine stencil
   -------------------------------------------------------- */
  
  TOT_LOOP(k,j,i) {
    wb[k][j][i] = 1;
    wb[k][j][i] -=    (lbound[IDIR] != 0 && i < IBEG)
                   || (rbound[IDIR] != 0 && i > IEND)
                   || (lbound[JDIR] != 0 && j < JBEG)
                   || (rbound[JDIR] != 0 && j < JEND)
                   || (lbound[KDIR] != 0 && k < KBEG)
                   || (rbound[KDIR] != 0 && k > KEND);
  }  
  
  NVAR_LOOP(nv){
    DOM_LOOP(k,j,i) Uloc[k][jk][i] = U[k][j][i][nv];
    AL_Exchange_dim ((char *)Uloc[0][0], par_dim, SZ);
  }
 
/* --------------------------------------------------------
   2. Replace volume-centered quantities
   -------------------------------------------------------- */
  
   
  NVAR_LOOP(nv){
    uav = dVav = 0.0;
    for (k1 = k-INCLUDE_KDIR; k1 <= k+INCLUDE_KDIR; k1++){
    for (j1 = j-INCLUDE_JDIR; j1 <= j+INCLUDE_JDIR; j1++){
    for (i1 = i-INCLUDE_IDIR; i1 <= i+INCLUDE_IDIR; i1++){
      uav  += Uloc[k1][j1][i1][nv]*grid->dV[k1][j1][i1]*wb[k1][j1][i1];
      dVav += grid->dV[k1][j1][i1]*wb[k1][j1][i1];
    }}}

    U[k][j][i][nv] /= dVav;
  }  
  
/* --------------------------------------------------------
   2. Replace face-centerd quantities [TODO]
   -------------------------------------------------------- */

}
#endif
