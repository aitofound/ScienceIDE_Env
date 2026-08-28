/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Write / Read orbital velocity to / from disk.

  The function FARGO_Write() is used to write the orbital velocity
  ::wA to disk whenever orbtial velocity is re-computed dynamically
  For restarting purposes, this 2D array is written with ghost zones
  included.

  In parallel, this is achieved by looping the processor's rank over
  the transverse directions (e.g. x, z) of the Cartesian MPI topology.
  Only processors with Cartesian coordinate j = 0 write to disk.
  A new MPI_Datatype is redefined each time in order to account for the
  different array sizes.
  From the MPI cartesian topology

  (0,npz-1)  (1,npz-1)  (2,npz-1)  . . . (npx-1,npz-1) 
      .          .         .
      .          .         .
    (0,2)        .         .
    (0,1)      (1,1)     (2,1)     . . .   (npx-1,1)
    (0,0)      (1,0)     (2,0)     . . .   (npx-1,0)

  interior processor with coordinates (i,k) such that 0 < i < npx-1
  and 0 < k < npz-1 do not include guard cells and have size (NX1*NX3).
  
  If i = 0 and 0 < k < npz-1, the array has size (NX1+nghost)*NX3 and will
  start from the left boundary (start[0] = 0).
  If i = npx-1 and 0 < k < npz-1, the array has size (NX1+nghost)*NX3 and
  include ghost zones on the right (start[0] = nghost[])
  A similar argument applies when k = 0 or k = npz-1 and 0 < i < npx-1.
  At corners [ (0,0), (npx1-1, 0), (0,npz-1), (npx-1, npz-1)] the array
  will have size (NX1+nghost)*(NX3+nghost).
  The start[] array is always = nghost, unless i or k are equal to 0.  

  \authors A. Mignone (andrea.mignone@unito.it)\n
         
  \date    Apr 15, 2021
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void FARGO_Write(const Data *d, char *dir, int nfile, Grid *grid)
/*!
 *  
 *********************************************************************** */
{
  char fname[32];
  int  i,j,k;

  int gsize[3], lsize[3], start[3];

  int coords[3], rank;
  int *nghost      = grid->nghost;
  int *nproc       = grid->nproc;
  int *np_tot_glob = grid->np_tot_glob;
  int *np_tot      = grid->np_tot;
  int *np_int      = grid->np_int;
  int *rank_coord  = grid->rank_coord;

  double **wA = FARGO_Velocity();
  #ifdef PARALLEL
  MPI_File fh;
  MPI_Comm cartcomm;
  #else
  FILE *fh;
  #endif

#if FARGO_NSTEP_AVERAGE > 0

/* ----------------------------------------------
   0. Allocate memory
   ---------------------------------------------- */

  sprintf (fname, "%s/wFargo.%04d.dbl", dir, nfile);
  printLog ("> Writing FARGO orbital speed file %s \n",fname);

/* ----------------------------------------------
   1. Delete and open file for writing
   ---------------------------------------------- */

  #ifdef PARALLEL
  AL_Get_cart_comm(SZ, &cartcomm);

  MPI_File_delete(fname, MPI_INFO_NULL);
  MPI_File_open(cartcomm, fname, 
                MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);

/* ----------------------------------------------
   2. Loop over Cartesian MPI topology. Only
      processors with j = 0 (k = 0) do the
      actual writing in Cartesian/polar (spherical)
      geometries.     
   ---------------------------------------------- */

  #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
  j = 0;
  for (k = 0; k < nproc[KDIR]; k++){
  for (i = 0; i < nproc[IDIR]; i++){
  #elif GEOMETRY == SPHERICAL
  k = 0;
  for (j = 0; j < nproc[JDIR]; j++){
  for (i = 0; i < nproc[IDIR]; i++){
  #endif

  /* --------------------------------------------
     2a. Obtain rank from coordinates [i,j,k]
     -------------------------------------------- */

    MPI_Datatype type_local, type_domain;

    coords[0] = i;
    coords[1] = j;
    coords[2] = k;

    MPI_Cart_rank(cartcomm, coords, &rank);

  /* --------------------------------------------
     2b. Create type_local subarray (the portion
         of the local array that is written)
     -------------------------------------------- */

    #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
    gsize[0] = np_tot[IDIR];
    gsize[1] = np_tot[KDIR];

    lsize[0] = np_int[IDIR] + nghost[IDIR]*( (i == 0) + (i == nproc[IDIR]-1) );
    lsize[1] = np_int[KDIR] + nghost[KDIR]*( (k == 0) + (k == nproc[KDIR]-1) );

    start[0] = (i == 0 ? 0:nghost[IDIR]);
    start[1] = (k == 0 ? 0:nghost[KDIR]);
    #elif GEOMETRY == SPHERICAL
    gsize[0] = np_tot[IDIR];
    gsize[1] = np_tot[JDIR];

    lsize[0] = np_int[IDIR] + nghost[IDIR]*( (i == 0) + (i == nproc[IDIR]-1) );
    lsize[1] = np_int[JDIR] + nghost[JDIR]*( (j == 0) + (j == nproc[JDIR]-1) );

    start[0] = (i == 0 ? 0:nghost[IDIR]);
    start[1] = (j == 0 ? 0:nghost[JDIR]);
    #endif

    MPI_Type_create_subarray (2, gsize, lsize, start, MPI_ORDER_FORTRAN,
                              MPI_DOUBLE, &type_local);
    MPI_Type_commit (&type_local);

  /* --------------------------------------------
     2c. Create type_domain (view) subarray

         With 4 processors along the x-direction,
         for instance:

         (nx+ng) nx  nx  nx+ng

      --> start[0] = nx*i + ng*(i>0) + ng*(i==npx-1)
     -------------------------------------------- */

    #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
    gsize[0] = np_tot_glob[IDIR];
    gsize[1] = np_tot_glob[KDIR];

    start[0] = np_int[IDIR]*i + nghost[IDIR]*(i > 0);
    start[1] = np_int[KDIR]*k + nghost[KDIR]*(k > 0);
    #elif GEOMETRY == SPHERICAL
    gsize[0] = np_tot_glob[IDIR];
    gsize[1] = np_tot_glob[JDIR];

    start[0] = np_int[IDIR]*i + nghost[IDIR]*(i > 0);
    start[1] = np_int[JDIR]*j + nghost[JDIR]*(j > 0);
    #endif

    MPI_Type_create_subarray (2, gsize, lsize, start,
                              MPI_ORDER_FORTRAN, MPI_DOUBLE, &type_domain);
    MPI_Type_commit (&type_domain);

  /* --------------------------------------------
     2d. Set file view and write
     -------------------------------------------- */

    MPI_File_set_view(fh, 0, MPI_DOUBLE, type_domain, "native", MPI_INFO_NULL);
    if (rank == prank){
      MPI_File_write(fh, wA[0], 1, type_local, MPI_STATUS_IGNORE);
int ii,jj,kk;
//KTOT_LOOP(kk) ITOT_LOOP(ii) printLog ("wA[%d, %d] = %12.6e\n",ii,kk,wA[kk][ii]);

    }
    MPI_Type_free(&type_domain);
    MPI_Type_free(&type_local);

  }}
  MPI_File_close(&fh);

  #else

  #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
  long int cnt = NX1_TOT*NX3_TOT;
  #elif GEOMETRY == SPHERICAL
  long int cnt = NX1_TOT*NX2_TOT;
  #endif
  fh = fopen(fname,"w");
  fwrite (wA[0], sizeof(double), cnt, fh);
  fclose (fh);

  #endif   /* PARALLEL */

#endif /* FARGO_NSTEP_AVERAGE > 0 */

}

/* ********************************************************************* */
void FARGO_Restart(const Data *d, char *dir, int nfile, int swap_endian, Grid *grid)
/*!
 * Read average orbital speed from .dbl file and store it in wA[][].
 *
 *
 *********************************************************************** */
{
  char fname[32];
  int  i,j,k;
  int gsize[3], lsize[3], start[3];

  int coords[3], rank;
  int *nproc       = grid->nproc;
  int *np_tot_glob = grid->np_tot_glob;
  int *np_tot      = grid->np_tot;
  int *np_int      = grid->np_int;
  int *rank_coord  = grid->rank_coord;

  double **wA = FARGO_Velocity();
  #ifdef PARALLEL
  MPI_File fh;
  MPI_Comm cartcomm;
  #else
  FILE *fh;
  #endif

#if FARGO_NSTEP_AVERAGE > 0

/* ----------------------------------------------
   1. Open file for reading
   ---------------------------------------------- */

  sprintf (fname, "%s/wFargo.%04d.dbl", dir, nfile);
  printLog ("> Reading FARGO orbital speed file %s \n",fname);

  #ifdef PARALLEL

  AL_Get_cart_comm(SZ, &cartcomm);
  MPI_File_open(cartcomm, fname, MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);

/* ----------------------------------------------
   2. Loop over processors in the x1-x3 ...
   ---------------------------------------------- */

  for (k = 0; k < nproc[KDIR]; k++){
  for (j = 0; j < nproc[JDIR]; j++){
  for (i = 0; i < nproc[IDIR]; i++){

    MPI_Datatype type_view;

    coords[0] = i;
    coords[1] = j;
    coords[2] = k;

    MPI_Cart_rank(cartcomm, coords, &rank);

  /* --------------------------------------------
     2c. Create type_view subarray.
         This will read active+ghost zones.
     -------------------------------------------- */

    #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
    gsize[0] = np_tot_glob[IDIR];
    gsize[1] = np_tot_glob[KDIR];

    lsize[0] = np_tot[IDIR];
    lsize[1] = np_tot[KDIR];

    start[0] = np_int[IDIR]*i;
    start[1] = np_int[KDIR]*k;
    #elif GEOMETRY == SPHERICAL
    gsize[0] = np_tot_glob[IDIR];
    gsize[1] = np_tot_glob[JDIR];

    lsize[0] = np_tot[IDIR];
    lsize[1] = np_tot[JDIR];

    start[0] = np_int[IDIR]*i;
    start[1] = np_int[JDIR]*j;
    #endif

    MPI_Type_create_subarray (2, gsize, lsize, start,
                              MPI_ORDER_FORTRAN, MPI_DOUBLE, &type_view);
    MPI_Type_commit (&type_view);

  /* --------------------------------------------
     2d. Set file view and write
     -------------------------------------------- */

    MPI_File_set_view(fh, 0, MPI_DOUBLE, type_view, "native", MPI_INFO_NULL);
    if (rank == prank){
int ii, jj, kk;

      #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
KTOT_LOOP(kk) ITOT_LOOP(ii) wA[kk][ii] = 0.0;
      MPI_File_read(fh, wA[0], NX1_TOT*NX3_TOT, MPI_DOUBLE, MPI_STATUS_IGNORE);
//KTOT_LOOP(kk) ITOT_LOOP(ii) printLog ("wA[%d, %d] = %12.6e\n",ii,kk,wA[kk][ii]);
      #elif GEOMETRY == SPHERICAL
JTOT_LOOP(jj) ITOT_LOOP(ii) wA[jj][ii] = 0.0;
      MPI_File_read(fh, wA[0], NX1_TOT*NX2_TOT, MPI_DOUBLE, MPI_STATUS_IGNORE);
//JTOT_LOOP(jj) ITOT_LOOP(ii) printLog ("wA[%d, %d] = %f\n",ii,jj,wA[jj][ii]);
      #endif

    }
    MPI_Type_free(&type_view);
  }}}
  MPI_File_close(&fh);

#else

  #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
  long int cnt = NX1_TOT*NX3_TOT;
  #elif GEOMETRY == SPHERICAL
  long int cnt = NX1_TOT*NX2_TOT;
  #endif
  fh = fopen(fname,"r");
  fread (wA[0], sizeof(double), cnt, fh);
  fclose (fh);

#endif

/* ----------------------------------------------
   2. Swap endianity if needed.
   ---------------------------------------------- */

  if (swap_endian){
    int ii,jj,kk;
    #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
    KTOT_LOOP(kk) ITOT_LOOP(ii) {
      SWAP_VAR(wA[kk][ii]);
    }
    #elif GEOMETRY == SPHERICAL
    JTOT_LOOP(jj) ITOT_LOOP(ii) {
      SWAP_VAR(wA[jj][ii]);
    }
    #endif
  }
#endif /* FARGO_NSTEP_AVERAGE > 0 */

/* ----------------------------------------------
   3. If output contains total velocity, subtract
      orbital speed.
   ---------------------------------------------- */

  #if FARGO_OUTPUT_VTOT == YES
  #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR) 
  DOM_LOOP(k,j,i) d->Vc[VX2][k][j][i] -= wA[k][i]; 
  #elif GEOMETRY == SPHERICAL
  DOM_LOOP(k,j,i) d->Vc[VX3][k][j][i] -= wA[j][i];
  #endif
  #endif

}
