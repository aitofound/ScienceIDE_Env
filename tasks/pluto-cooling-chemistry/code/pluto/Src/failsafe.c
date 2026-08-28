#include "pluto.h"

#if FAILSAFE == YES
/* ********************************************************************* */
void FailSafe (Data *d, timeStep *Dts, Grid *grid)
/*!
 *
 * 
 *********************************************************************** */
{
  int  err;
  int  i,j,k,nv;
  int  nretry;

  int    n0  = g_stepNumber; 
  double dt0 = g_dt;
  double t0  = g_time;
 
  static double ***Bss0[3], ***Ess0[3];
  static uint16_t ***flag1;
  static Data_Arr Ucs0, Vcs0;
  
/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */

  if (Ucs0 == NULL){
    Ucs0  = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, NVAR, double);
    Vcs0  = ARRAY_4D(NVAR, NX3_TOT, NX2_TOT, NX1_TOT, double);
    flag1 = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, uint16_t);
    #ifdef STAGGERED_MHD
    DIM_EXPAND(
      Bss0[IDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
      Bss0[JDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
      Bss0[KDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    )
    #endif
    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    DIM_EXPAND(
      Ess0[IDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
      Ess0[JDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
      Ess0[KDIR] = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    )
    #endif
  }

/* --------------------------------------------------------
   1. Backup solution arrays at t = t^n
   -------------------------------------------------------- */

  TOT_LOOP(k,j,i) NVAR_LOOP(nv)   Ucs0[k][j][i][nv] = d->Uc[k][j][i][nv];
  NVAR_LOOP(nv)   TOT_LOOP(k,j,i) Vcs0[nv][k][j][i] = d->Vc[nv][k][j][i];
  #ifdef STAGGERED_MHD
  DIM_LOOP(nv) TOT_LOOP(k,j,i) Bss0[nv][k][j][i] = d->Vs[nv][k][j][i];
  #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  DIM_LOOP(nv) TOT_LOOP(k,j,i) Ess0[nv][k][j][i] = d->Vs[EX1s+nv][k][j][i];
  #endif
  #endif

/* --------------------------------------------------------
   2. Try step, return to caller if err == 0
   -------------------------------------------------------- */

  TOT_LOOP(k,j,i) d->flag[k][j][i] = 0;
  err = Integrate (d, Dts, grid);
  if (err == 0) return;  /* Step succeeded */

/* --------------------------------------------------------
   3. Step did not succeed: enable Fail-safe procedure.
      Here 'err' is a globaly reduced variable already, so
      it has the same value for all processors.
   -------------------------------------------------------- */

  if (err > 0) {
    int invalid_zones=0;
    nretry = 1;
    uint16_t filter = 0;

    print("> FailSafe(): err = %d [g_intStage = %d] --> retrying step\n",
               err, g_intStage);
    filter |= FLAG_CONS2PRIM_FAIL;
    if (FlagCheck(d->flag, FLAG_CONS2PRIM_FAIL)) FlagShow(d, filter);

  /* -- 3a. Prepare new flag from old failures -- */

    TOT_LOOP(k,j,i){

      flag1[k][j][i] = 0;
      if ( (d->flag[k][j][i] & FLAG_CONS2PRIM_FAIL) ) {
        invalid_zones++;
        printLog("  - Flagging zone (%d %d %d)\n",i,j,k);
        printLog("  - U0 = "); ShowState(Ucs0[k][j][i],0);
 
        int ib = i - 2*INCLUDE_IDIR, ie = i + 2*INCLUDE_IDIR;
        int jb = j - 2*INCLUDE_JDIR, je = j + 2*INCLUDE_JDIR;
        int kb = k - 2*INCLUDE_KDIR, ke = k + 2*INCLUDE_KDIR;
	
        ib = MAX(0, ib); ie = MIN(NX1_TOT-1,ie);
	       jb = MAX(0, jb); je = MIN(NX2_TOT-1,je);
	       kb = MAX(0, kb); ke = MIN(NX3_TOT-1,ke);
        int ii, jj, kk;

        flag1[k][j][i] |= FLAG_HLL;
        for (ii = ib; ii <= ie; ii++){
        for (jj = jb; jj <= je; jj++){
        for (kk = kb; kk <= ke; kk++){
          if ( abs(ii-i) <= 1 && abs(jj-j) <= 1 && abs(kk-k) <= 1 ) {
             flag1[k][j][i] |= FLAG_FLAT;
          }
          else flag1[k][j][i] |= FLAG_MINMOD;
       	}}}
      }
    }

  /* -- 3b. Overwrite d->flag with new values  -- */

    TOT_LOOP(k,j,i) d->flag[k][j][i] = flag1[k][j][i];

  /* -- 3c. Exchange data, Reduce and print info -- */

    #ifdef PARALLEL
    AL_Exchange (d->flag[0][0], SZ_uint16_t);
//    MPI_Allreduce (&invalid_zones, &nv, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
//    invalid_zones = nv;
    #endif
    if (invalid_zones > 0) printLog("  Total number of flagged zones = %d\n",invalid_zones);

  /* -- 3d. Restore solutionn array before repeating step -- */

    g_stepNumber = n0;
    g_time       = t0;
    g_dt         = dt0;

    print ("  --> step:%d; t = %10.4e; dt = %10.4e;\n",  g_stepNumber,
                                                         g_time, g_dt);
    LogFileFlush();

    TOT_LOOP(k,j,i) NVAR_LOOP(nv)   d->Uc[k][j][i][nv] = Ucs0[k][j][i][nv];
    NVAR_LOOP(nv)   TOT_LOOP(k,j,i) d->Vc[nv][k][j][i] = Vcs0[nv][k][j][i];
    #ifdef STAGGERED_MHD
    DIM_LOOP(nv) TOT_LOOP(k,j,i) d->Vs[nv][k][j][i] = Bss0[nv][k][j][i];
    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    DIM_LOOP(nv) TOT_LOOP(k,j,i) d->Vs[EX1s+nv][k][j][i] = Ess0[nv][k][j][i];
    #endif
    #endif

  /* -- 3e. Repeat step -- */

    err = Integrate (d, Dts, grid);
    if (err && FlagCheck(d->flag, FLAG_CONS2PRIM_FAIL)) {
      printLog ("  - Final flag:\n");
      FlagShow(d, filter);
      printLog ("  ! Step did not succeed\n");
      QUIT_PLUTO(1);
    }
  }
  
}
#endif  /* FAILSAFE == YES */

/* ********************************************************************* */
void FlagShow(Data *d, uint16_t filter)
/*!
 *  Show which bits of the d->flag structure are set.
 *  The variable filter can be used to select only certain bits.
 *
 * \param [in] d        pointer to PLUTO data structure
 * \param [in] filter   a filter-pass variable used to select printing
 *                      of certain bits only.
 * 
 *********************************************************************** */
{
  int i,j,k;
  int nbits = 8*sizeof(uint16_t);
  int bit_position;
  uint16_t ***flag = d->flag;
  int isBitSet, isFilterSet;

  g_dir = IDIR;
  DOM_LOOP(k,j,i){
    g_j = j;
    g_k = k;

  /* ------------------------------------------------------
     1. Check bits to see which of them are on. 
        Enable show_flag only if they correspond to those
        specified by filter.
     ------------------------------------------------------ */
 
    int show_flag = 0;
    for (bit_position = 0; bit_position < nbits; bit_position++){
      isBitSet    = flag[k][j][i] & (1 << bit_position);
      isFilterSet = filter & (1 << bit_position);
      if (isBitSet && isFilterSet) show_flag = 1;
    }

    if (show_flag){
      printLog ("> FlagShow(): (%d, %d, %d)",i,j,k); Where(i,NULL);
/*
      printLog ("              active flags: ");
      if (flag[k][j][i] & FLAG_NEGATIVE_DENSITY) printLog ("/negative density"); 
      if (flag[k][j][i] & FLAG_NEGATIVE_ENERGY)  printLog ("/negative energy"); 
      if (flag[k][j][i] & FLAG_NEGATIVE_PRESSURE) printLog ("/negative pressure"); 
      if (flag[k][j][i] & FLAG_PRESSURE_FIX_FAIL) printLog ("/press. fix failed"); 
      if (flag[k][j][i] & FLAG_CONS2PRIM_FAIL) printLog ("/cons2prim fail"); 
      if (flag[k][j][i] & FLAG_HLL) printLog ("/hll"); 
      if (flag[k][j][i] & FLAG_MINMOD) printLog ("/minmod"); 
      if (flag[k][j][i] & FLAG_FLAT) printLog ("/flat"); 
      if (flag[k][j][i] & FLAG_SPLIT_CELL) printLog ("/splitCell");
      if (flag[k][j][i] & FLAG_ENTROPY) printLog ("/entropy"); 
*/

      printLog ("            ");
      printLog ("| MM  |");
      printLog (" FL  |");
      printLog (" HLL |");
      printLog (" Entr|");
      printLog (" SC  |");
      printLog (" IB  |");
      printLog (" C2P |");
      printLog (" nP  |");
      printLog (" nE  |");
      printLog (" nD  |");
      printLog ("prfix|");
      printLog ("\n            ");
      for (bit_position = 0; bit_position < nbits; bit_position++){
        printLog ("+-----");
      }
      printLog ("+\n            ");

      for (bit_position = 0; bit_position < nbits; bit_position++){
        isBitSet = flag[k][j][i] & (1 << bit_position);
        if (isBitSet) printLog ("|  *  ");
        else          printLog ("|     ");
      }
      printLog ("|\n");
    }
  }
}

/* ********************************************************************* */
int FlagCheck(uint16_t ***flag, int f)
/*
 *
 *
 *********************************************************************** */
{
  int i,j,k;

  DOM_LOOP(k,j,i){
    if (flag[k][j][i] & f) return 1;	   
  }
  return 0;
}

