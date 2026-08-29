/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Collection of functions for debugging purposes.
  
  \author A. Mignone (andrea.mignone@unito.it)
  \date   Sep 14, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h" 

/* ********************************************************************* */
int CheckNaN (double **u, int ibeg, int iend, const char *str)
/*!
 * Check whether the array \c u contains Not-a-Number
 *  (NaN). QUIT if true.
 *
 * \param [in]  u      pointer to an array of type u[i][nv]
 * \param [in]  ibeg   starting index 
 * \param [in]  ibeg   ending  index 
 * \param [in]  str    a reference string 
 *
 *********************************************************************** */
{
  int i, nv;

  for (i = ibeg; i <= iend; i++) {
  for (nv = 0; nv < NVAR; nv++) {
    if (isnan(u[i][nv])) {
      printLog ("> CheckNan() [%s]: NaN found", str);
      Where (i,NULL);
      Show (u, i);
      QUIT_PLUTO(1);
    }
  }}
  return 0;
}

/* ********************************************************************* */
int CheckData (Data_Arr U, Data_Arr V, Data_Arr Vs, int positivity_check,
               RBox *box, const char *str)
/*
 * Check whether the array \c U, \c V or \c Vs contain
 * Not-a-Number (NaN), negative density or pressures.
 *
 * \param [in]  U       an array of conserved variables of the type
 *                      U[k][j][i][nv]; Ignored when U == NULL
 * \param [in]  V       an array of primitive variables of the type
 *                      V[nv][k][j][i]; Ignored when V == NULL
 * \param [in]  Vs      an array of staggered fields of the type
 *                      Vs[nv][k][j][i]; Ignored when Vs == NULL
 * \param [in]  positivity_check   an integer (0/1) that enables 
 *                                 check of density / energy / pressure
 * \param [in]  box     the box to be checked
 * \param [in]  str     a reference string to be printed.
 *
 * \return Return 0 if no problem has been found. Otherwise return
 *         1: for nan, 2: for negative density and 3: for negative
 *         pressure or energy.
 *********************************************************************** */
{
  int nv, i,j,k;
  int err = 0;
  char *err_message[] = {" ",
                         "nan found",
                         "negative density",
                         "negative pressure or energy"};

/* --------------------------------------------------------
   2. Check nan in conserved variables
   -------------------------------------------------------- */

  if (U != NULL){
    BOX_LOOP(box, k,j,i){
      g_j = j;
      g_k = k;
      err = 0;
      NVAR_LOOP(nv) if (isnan(U[k][j][i][nv])) err = 1;

      if (positivity_check && U[k][j][i][RHO] <= 0.0) err = 2;
      #if HAVE_ENERGY
      if (positivity_check && U[k][j][i][ENG] <= 0.0) err = 3;
      #endif
      
      if (err){
        printLog ("! CheckData() [%s]: %s in conservative array\n", 
                   str, err_message[err]);
        Where (i,NULL);
        for (nv = 0; nv < NVAR; nv++){
          printLog ("  U[%d] = %8.3e\n",nv,U[k][j][i][nv]);
        }
        return err;
      }
    }
  }

/* --------------------------------------------------------
   2. Check nan in primitive variables
   -------------------------------------------------------- */

  if (V != NULL){
    g_dir = IDIR;
    BOX_LOOP(box, k,j,i){
      g_j = j;
      g_k = k;
      err = 0;
      NVAR_LOOP(nv){
        if (isnan(V[nv][k][j][i])) err = 1;
      }

      if (positivity_check && V[RHO][k][j][i] <= 0.0) err = 2;
      #if HAVE_ENERGY
      if (positivity_check && V[PRS][k][j][i] <= 0.0) err = 3;
      #endif

      if (err > 0){
        printLog ("! CheckData() [%s]: %s in primitive array\n",
                   str, err_message[err]);
        Where (i,NULL);
        for (nv = 0; nv < NVAR; nv++){
          printLog ("  V[%d] = %8.3e\n",nv, V[nv][k][j][i]);
        }
        return err;
      }
    }
  }

/* --------------------------------------------------------
   3. Check nan in staggered fields
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD
  if (Vs != NULL) {
    RBox stag_box;

    stag_box.ibeg = box->ibeg-1; stag_box.iend = box->iend;
    stag_box.jbeg = box->jbeg;   stag_box.jend = box->jend;
    stag_box.kbeg = box->kbeg;   stag_box.kend = box->kend;

  /* -- 3a. Check nan in BX1s, EX1s -- */

    err = 0;
    BOX_LOOP(&stag_box, k,j,i){
      g_j = j; g_k = k;
      if (isnan(Vs[BX1s][k][j][i])) err = 1;
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      if (isnan(Vs[EX1s][k][j][i])) err = 1;
      #endif

      if (err){
        printLog ("! CheckData() [%s]: nan found in staggered array\n", str);
        Where (i,NULL);
        printLog ("! Vs[BX1s] = %8.3e\n",Vs[BX1s][k][j][i]);
        #if PHYSICS == ResRMHD
        printLog ("! Vs[EX1s] = %8.3e\n",Vs[EX1s][k][j][i]);
        #endif
        return err;
      }
    }

  /* -- 3b. Check nan in BX2s, EX2s -- */

    #if INCLUDE_JDIR
    stag_box.ibeg = box->ibeg;   stag_box.iend = box->iend;
    stag_box.jbeg = box->jbeg-1; stag_box.jend = box->jend;
    stag_box.kbeg = box->kbeg;   stag_box.kend = box->kend;

    err = 0;
    BOX_LOOP(&stag_box, k,j,i){
      g_j = j; g_k = k;
      if (isnan(Vs[BX2s][k][j][i])) err = 1;
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      if (isnan(Vs[EX2s][k][j][i])) err = 1;
      #endif

      if (err){
        printLog ("! CheckData() [%s]: nan found in staggered array\n", str);
        Where (i,NULL);
        printLog ("! Vs[BX2s] = %8.3e\n",Vs[BX2s][k][j][i]);
        #if PHYSICS == ResRMHD
        printLog ("! Vs[EX2s] = %8.3e\n",Vs[EX2s][k][j][i]);
        #endif
        return err;
      }
    }
    #endif

  /* -- 3c. Check nan in BX3s, EX3s -- */
    
    #if INCLUDE_KDIR
    stag_box.ibeg = box->ibeg;   stag_box.iend = box->iend;
    stag_box.jbeg = box->jbeg;   stag_box.jend = box->jend;
    stag_box.kbeg = box->kbeg-1; stag_box.kend = box->kend;

    err = 0;
    BOX_LOOP(&stag_box, k,j,i){
      g_j = j; g_k = k;
      if (isnan(Vs[BX3s][k][j][i])) err = 1;
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      if (isnan(Vs[EX3s][k][j][i])) err = 1;
      #endif

      if (err){
        printLog ("! CheckData() [%s]: nan found in staggered array\n", str);
        Where (i,NULL);
        printLog ("! Vs[BX3s] = %8.3e\n",Vs[BX3s][k][j][i]);
        #if PHYSICS == ResRMHD
        printLog ("! Vs[EX3s] = %8.3e\n",Vs[EX3s][k][j][i]);
        #endif
        return err;
      }
    }
    #endif /* INCLUDE_KDIR */
  } /* Vs != NULL */
#endif /* STAGGERED_MHD */

  return 0;
}


/* ********************************************************************* */
void ShowData (Data_Arr U, Data_Arr V, Data_Arr Vs, RBox *box, const char *str)
/*
 * Print explicit values of U, V or Vs in the box.
 *
  * \param [in]  U       an array of conserved variables of the type
 *                      U[k][j][i][nv]; Ignored when U == NULL
 * \param [in]  V       an array of primitive variables of the type
 *                      V[nv][k][j][i]; Ignored when V == NULL
 * \param [in]  Vs      an array of staggered fields of the type
 *                      Vs[nv][k][j][i]; Ignored when Vs == NULL
 * \param [in]  box     the box to be checked
 * \param [in]  str     a reference string to be printed.
*********************************************************************** */
{
  int i,j,k,nv;
  double v[256];

/* --------------------------------------------------------
   1. Print value of conserved variables
   -------------------------------------------------------- */

  if (U != NULL){
    BOX_LOOP(box,k,j,i){
      printLog ("ShowData() [%s]: U(%d, %d, %d) = \n", str,i,j,k);
      ShowState(U[k][j][i], 2);
    }
  }

/* --------------------------------------------------------
   2. Print value of primitive variables
   -------------------------------------------------------- */

  if (V != NULL){
    BOX_LOOP(box,k,j,i){
      printLog ("ShowData() [%s]: V(%d, %d, %d) = \n", str,i,j,k);
      NVAR_LOOP(nv) v[nv] = V[nv][k][j][i];
      ShowState(v, 1);
    }
  }

/* --------------------------------------------------------
   3. Print value of staggered fields
   -------------------------------------------------------- */

#ifdef STAGGERED_MHD
  if (Vs != NULL){
    RBox stag_box;

    stag_box.ibeg = box->ibeg-1; stag_box.iend = box->iend;
    stag_box.jbeg = box->jbeg;   stag_box.jend = box->jend;
    stag_box.kbeg = box->kbeg;   stag_box.kend = box->kend;

    BOX_LOOP(&stag_box,k,j,i){
      CT_ASSIGN( printLog ("ShowData() [%s]: Vs[BX1s](%d, %d, %d) = %12.6e\n",
                            str, i,j,k,Vs[BX1s][k][j][i]);   ,
                 printLog ("                 Vs[EX1s](%d, %d, %d) = %12.6e\n",
                            i,j,k,Vs[EX1s][k][j][i]);) 
    } 

    #if INCLUDE_JDIR
    stag_box.ibeg = box->ibeg;   stag_box.iend = box->iend;
    stag_box.jbeg = box->jbeg-1; stag_box.jend = box->jend;
    stag_box.kbeg = box->kbeg;   stag_box.kend = box->kend;

    BOX_LOOP(&stag_box,k,j,i){
      CT_ASSIGN( printLog ("ShowData() [%s]: Vs[BX2s](%d, %d, %d) = %12.6e\n",
                            str, i,j,k,Vs[BX2s][k][j][i]);   ,
                 printLog ("                 Vs[EX2s](%d, %d, %d) = %12.6e\n",
                             i,j,k,Vs[EX2s][k][j][i]);)
    } 
    #endif

    #if INCLUDE_KDIR
    stag_box.ibeg = box->ibeg;   stag_box.iend = box->iend;
    stag_box.jbeg = box->jbeg;   stag_box.jend = box->jend;
    stag_box.kbeg = box->kbeg-1; stag_box.kend = box->kend;

    BOX_LOOP(&stag_box,k,j,i){
      CT_ASSIGN( printLog ("ShowData() [%s]: Vs[BX3s](%d, %d, %d) = %12.6e\n",
                            str, i,j,k,Vs[BX3s][k][j][i]);   ,
                 printLog ("                 Vs[EX3s](%d, %d, %d) = %12.6e\n",
                            i,j,k,Vs[EX3s][k][j][i]);)
    } 
    #endif

  }

#endif

}

/* ********************************************************************* */
void Show (double **a, int ip)
/*! 
 * Print the component of the array \c a at grid index \c ip  
 *
 *********************************************************************** */
{
  int nv, ix, iy, iz;

  if (g_dir == IDIR) {
    printLog ("X-sweep");
    ix = ip;
    iy = g_j;
    iz = g_k;
  } else if (g_dir == JDIR) {
    printLog ("Y-sweep");
    ix = g_i;
    iy = ip;
    iz = g_k;
  } else if (g_dir == KDIR) {
    printLog ("Z-sweep");
    ix = g_i;
    iy = g_j;
    iz = ip;
  }

  DIM_SELECT( printLog (" (%d)> ", ix);     ,
            printLog (" (%d,%d)> ", ix, iy);  ,
            printLog (" (%d,%d,%d)> ", ix, iy, iz);  )

  for (nv = 0; nv < NVAR; nv++) {
    printLog ("%8.3e  ", a[ip][nv]);
  }
  printLog ("\n");
}

/* ********************************************************************* */
void ShowMatrix(double **A, int n, double eps)
/*!
 * Make a nice printLoging of a 2D square  matrix \c A[0..n-1][0..n-1]
 * Entries with values below eps will display "0.0"
 *
 *********************************************************************** */
{
  int k1,k2;

  printLog ("----------------------------------------------------------------\n");
  for (k1 = 0; k1 < n; k1++){
    for (k2 = 0; k2 < n; k2++){
      printLog ("%12.3e   ", fabs(A[k1][k2]) > eps ? A[k1][k2]:0.0);
    }
    printLog ("\n");
  }
  printLog ("----------------------------------------------------------------\n");
}

/* ********************************************************************* */
void ShowState (double *vc, int mode)
/*! 
 * Print the components of the array v.
 * Here mode = 1 (-1) if the array contains primitive variables;
 * Here mode = 2 (-2) if the array contains conservative variables;
 * Here mode = 3      if the array contains fluxes;
 *
 * A positive value of mode will produce code-ready vertical layout 
 * while a negative value will produce horizontal layout.
 *********************************************************************** */
{
  int nv;
/*
  if (g_dir == IDIR) {
    printLog ("X1-sweep\n");
  } else if (g_dir == JDIR) {
    printLog ("X2-sweep\n");
  } else if (g_dir == KDIR) {
    printLog ("X3-sweep\n");
  }
*/
  if (mode == 1){  /* Primitive variables, vertical layout */
    printLog ("  vc[RHO] = %18.12e;\n", vc[RHO]);
    printLog ("  vc[VX1] = %18.12e;\n", vc[VX1]);
    printLog ("  vc[VX2] = %18.12e;\n", vc[VX2]);
    printLog ("  vc[VX3] = %18.12e;\n", vc[VX3]);
    #if HAVE_ENERGY
    printLog ("  vc[PRS] = %18.12e;\n", vc[PRS]);
    #endif
    #if ENTROPY_SWITCH != NO
    printLog ("  vc[ENTR] = %18.12e;\n", vc[ENTR]);
    #endif
  
    #if PHYSICS == MHD || PHYSICS == RMHD || PHYSICS == ResRMHD
    printLog ("  vc[BX1] = %18.12e;\n", vc[BX1]);
    printLog ("  vc[BX2] = %18.12e;\n", vc[BX2]);
    printLog ("  vc[BX3] = %18.12e;\n", vc[BX3]);
    #endif
    #if PHYSICS == ResRMHD
    printLog ("  vc[EX1] = %18.12e;\n", vc[EX1]);
    printLog ("  vc[EX2] = %18.12e;\n", vc[EX2]);
    printLog ("  vc[EX3] = %18.12e;\n", vc[EX3]);
    #ifdef CRG
    printLog ("  vc[CRG] = %18.12e;\n", vc[CRG]);
    #endif
    #endif
   
    #ifdef PHI_GLM
    printLog ("  vc[PHI_GLM] = %18.12e;\n", vc[PHI_GLM]);
    #endif
    #ifdef PSI_GLM
    printLog ("  vc[PSI_GLM] = %18.12e;\n", vc[PSI_GLM]);
    #endif
    printLog ("\n");

  } else if (mode == -1) { /* Primitive variables, horizontal layout */
    printLog ("  rho = %+8.3e; vx1 = %+8.3e; vx2 = %+8.3e; vx3 = %+8.3e",
               vc[RHO], vc[VX1], vc[VX2], vc[VX3]);
    #if HAVE_ENERGY
    printLog ("  prs = %+8.3e", vc[PRS]);
    #endif
    #if ENTROPY_SWITCH != NO
    printLog ("  entr = %18.12e;\n", vc[ENTR]);
    #endif
    #if PHYSICS == MHD || PHYSICS == RMHD || PHYSICS == ResRMHD
    printLog ("  Bx1 = %+8.3e; Bx2 = %+8.3e; Bx3 = %+8.3e;\n",
                 vc[BX1], vc[BX1], vc[BX3]);  
    #endif             
  }else if (mode == 2){
    printLog ("  uc[RHO] = %18.12e;\n", vc[RHO]);
    printLog ("  uc[MX1] = %18.12e;\n", vc[MX1]); 
    printLog ("  uc[MX2] = %18.12e;\n", vc[MX2]); 
    printLog ("  uc[MX3] = %18.12e;\n", vc[MX3]);
    #if HAVE_ENERGY
    printLog ("  uc[ENG] = %18.12e;\n", vc[ENG]);
    #endif
    #if ENTROPY_SWITCH != NO
    printLog ("  uc[ENTR] = %18.12e;\n", vc[ENTR]);
    #endif

    #if PHYSICS == MHD || PHYSICS == RMHD || PHYSICS == ResRMHD
    printLog ("  uc[BX1] = %18.12e;\n", vc[BX1]);
    printLog ("  uc[BX2] = %18.12e;\n", vc[BX2]);
    printLog ("  uc[BX3] = %18.12e;\n", vc[BX3]);
    #endif
    #if PHYSICS == ResRMHD
    printLog ("  uc[EX1] = %18.12e;\n", vc[EX1]);
    printLog ("  uc[EX2] = %18.12e;\n", vc[EX2]);
    printLog ("  uc[EX3] = %18.12e;\n", vc[EX3]);
    #ifdef CRG
    printLog ("  uc[CRG] = %18.12e;\n", vc[CRG]);
    #endif
    #endif
   
    #ifdef PHI_GLM
    printLog ("  uc[PHI_GLM] = %18.12e;\n", vc[PHI_GLM]);
    #endif
    #ifdef PSI_GLM
    printLog ("  uc[PSI_GLM] = %18.12e;\n", vc[PSI_GLM]);
    #endif
    printLog ("\n");
    
  }else if (mode == -2){
    printLog ("  rho = %+8.3e; mx1 = %+8.3e; mx2 = %+8.3e; mx3 = %+8.3e",
               vc[RHO], vc[MX1], vc[MX2], vc[MX3]);
    #if HAVE_ENERGY
    printLog ("  eng = %+8.3e", vc[ENG]);
    #endif
    #if ENTROPY_SWITCH != NO
    printLog ("  entr = %+8.3e", vc[ENTR]);
    #endif
    #if PHYSICS == MHD || PHYSICS == RMHD || PHYSICS == ResRMHD
    printLog ("  Bx1 = %+8.3e; Bx2 = %+8.3e; Bx3 = %+8.3e;",
                 vc[BX1], vc[BX1], vc[BX3]);  
    #endif
    printLog("\n");

  }else if (mode == 3){   /* -- Flux mode -- */

    printLog ("  F[RHO] = %18.12e;\n", vc[RHO]);
    printLog ("  F[MX1] = %18.12e;\n", vc[MX1]); 
    printLog ("  F[MX2] = %18.12e;\n", vc[MX2]); 
    printLog ("  F[MX3] = %18.12e;\n", vc[MX3]);
    #if HAVE_ENERGY
    printLog ("  F[ENG] = %18.12e;\n", vc[ENG]);
    #endif
    #if ENTROPY_SWITCH != NO
    printLog ("  F[ENTR] = %18.12e;\n", vc[ENTR]);
    #endif

    #if PHYSICS == MHD || PHYSICS == RMHD || PHYSICS == ResRMHD
    printLog ("  F[BX1] = %18.12e;\n", vc[BX1]);
    printLog ("  F[BX2] = %18.12e;\n", vc[BX2]);
    printLog ("  F[BX3] = %18.12e;\n", vc[BX3]);
    #endif
    #if PHYSICS == ResRMHD
    printLog ("  F[EX1] = %18.12e;\n", vc[EX1]);
    printLog ("  F[EX2] = %18.12e;\n", vc[EX2]);
    printLog ("  F[EX3] = %18.12e;\n", vc[EX3]);
    #ifdef CRG
    printLog ("  F[CRG] = %18.12e;\n", vc[CRG]);
    #endif
    #endif
   
    #ifdef PHI_GLM
    printLog ("  F[PHI_GLM] = %18.12e;\n", vc[PHI_GLM]);
    #endif
    #ifdef PSI_GLM
    printLog ("  F[PSI_GLM] = %18.12e;\n", vc[PSI_GLM]);
    #endif
    printLog ("\n");
    
  }else{
    printLog ("! Invalid mode specified in ShowState()\n");
  }
}

/* ********************************************************************* */
void ShowVector (double *v, int n)
/*! 
 * Print the first n components of the vector v[]  
 *
 *********************************************************************** */
{
  int k;

  for (k = 0; k < n; k++)  printLog ("%+12.6e  ", v[k]);
  printLog ("\n");
}

/* ********************************************************************* */
void Trace (double xx)
/*!
 * Print a number xx and the number of times it has been called.
 *
 *********************************************************************** */
{
  static int ik;

  printLog ("Trace ------> %f ,  %d\n", xx, ++ik);
}

/* ********************************************************************* */
void Where (int i, Grid *grid)
/*!
 *  Print the location of a particular zone (i,j,k)
 *  in the computational domain.
 *
 *  
 *  \note This function must be initialized before using it 
 *        to store grid information. This is done  by calling 
 *        Where(i, grid) the very first time.
 *        Subsequent calls can be then done by simply using 
 *        Where(i,NULL). 
 *
 *********************************************************************** */
{
  int    ii=0, jj=0, kk=0;
  double x1, x2, x3;
  static Grid *grid_copy;

/* --------------------------------------------------
    Keep a local copy of grid for subsequent calls
   -------------------------------------------------- */
 
  if (grid != NULL){
    grid_copy = grid;
    return;
  }

  #ifdef CH_SPACEDIM
   if (g_intStage < 0) return; /* HOT FIX used by CHOMBO
                             (g_intStage = -1) when writing HDF5 file */
  #endif

/* -- ok, proceed normally -- */
  
  if (g_dir == IDIR){
    DIM_EXPAND(ii = i;, jj = g_j;, kk = g_k;)
  }else if (g_dir == JDIR){
    DIM_EXPAND(ii = g_i;, jj = i;, kk = g_k;)
  }else if (g_dir == KDIR){
    DIM_EXPAND(ii = g_i;, jj = g_j;, kk = i;)
  }

  DIM_EXPAND(
    x1 = grid_copy->x[IDIR][ii];  ,
    x2 = grid_copy->x[JDIR][jj];  ,
    x3 = grid_copy->x[KDIR][kk];
  )

  printLog ("  @step = %d (stage = %d);", g_stepNumber, g_intStage);
  DIM_SELECT(
    printLog (" [i = %d], [x1 = %f]", ii, x1);  ,

    printLog (" [i,j = %d, %d], [x1,x2 =  %f, %f]", ii, jj, x1, x2);  ,

    printLog (" [i,j,k = %d, %d, %d], [x1,x2,x3 = %f, %f, %f]", ii, jj, kk,
               x1, x2, x3);
  )

  #ifdef CHOMBO
  printLog (", Level = %d\n", grid_copy->level);
  return;
  #endif
  printLog ("\n");
}
