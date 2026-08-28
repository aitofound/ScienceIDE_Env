/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Set boundary conditions.

  The Boundary() function sets both internal and physical boundary 
  conditions on one or more sides of the computational domain.
  It is used to fill ghost zones of both cell-centered and face-centered 
  data arrays.\n
  The type of boundary conditions at the leftmost or rightmost side of a 
  given grid is specified by the integers <c> grid.lbound[dir] </c> or 
  <c> grid.rbound[dir] </c>, respectively.
  When this value is different from zero, the local processor borders 
  the physical boundary and the admissible values for \c lbound or \c 
  rbound are OUTFLOW, REFLECTIVE, AXISYMMETRIC, EQTSYMMETRIC, PERIODIC, 
  SHEARING or USERDEF.
  Conversely, when this value is zero (internal boundary), two neighboring
  processors that share the same side need to fill ghost zones by exchanging 
  data values. 
  This step is done here only for parallel computations on static grids.
  
  Predefined physical boundary conditions are handled by the 
  following functions:
  
  - OutflowBoundary()
  - ReflectiveBoundary()
  - PeriodicBoundary()


  \note For staggered mesh+periodic b.c, the leftmost value of \e each
        processor is overwritten with a value NX1 zones to the right:
        

      -->Before:

         proc #0     proc #1     proc #2     proc #3
        |.......|   |.......|   |.......|   |.......|
        A       B   C       D   E       F   G       H

      --> PeriodicBoundary()
      --> After:

         proc #0     proc #1     proc #2     proc #3
        |.......|   |.......|   |.......|   |.......|
        H       B   B       D   D       F   F       H

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Apr 08, 2024
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

static void FlipSign (int, int, int *);
                           
/* ********************************************************************* */
void Boundary (const Data *d, int iarr, Grid *grid)
/*!
 * Set boundary conditions on one or more sides of the computational
 * domain.
 *
 * \param [in,out] d  pointer to PLUTO Data structure containing the 
 *                    solution array (including centered and staggered
 *                    fields)
 * \param [in]   iarr specifies on which array b.c. have to be set
 *        - iarr = 0  (set on d->Vc a nd d->Vs)
 *        - iarr = 1  (set on d->Vpc and d->Vps)
 *
 * \param [in]  grid   pointer to grid structure.
 *********************************************************************** */
{
  int i,j,k;
  int  is, nv;
  int  side[6] = {X1_BEG, X1_END, X2_BEG, X2_END, X3_BEG, X3_END};
  int  type[6], sbeg, send, vsign[NVAR];
  int  *lbound = grid->lbound;
  int  *rbound = grid->rbound;
  int  par_dim[3] = {0, 0, 0};
  int  ib,ie,jb,je,kb,ke;
  int  isb, jsb, ksb;  /* Indices for staggered components */
  int  ise, jse, kse;

  RBox center_box, x1face_box, x2face_box, x3face_box;

  Data_Arr Vc;
  double ***Vs[6];

/* --------------------------------------------------------
   0a. Set pointer to main boundary array.
   -------------------------------------------------------- */

  Vc = d->Vc;
  #ifdef STAGGERED_MHD
  CT_ASSIGN(  for (nv = 0; nv < 3; nv++) Vs[nv] = d->Vs[nv];  ,
              for (nv = 3; nv < 6; nv++) Vs[nv] = d->Vs[nv];)
  #endif

#ifdef HIGH_ORDER
  if (iarr == 0){        /* Assign b.c. on Vc  &  Vs  */
    Vc = d->Vc;
    #ifdef STAGGERED_MHD
    for (nv = 0; nv < 3; nv++) Vs[nv] = d->Vs[nv];
    #endif
  }else if (iarr == 1){   /* Assign b.c. on point-values */
    Vc = d->Vpc;
    #ifdef STAGGERED_MHD
    for (nv = 0; nv < 3; nv++) Vs[nv] = d->Vps[nv];
    #endif
  }else{
    printLog ("! Boundary(): invail iarr number \n");
    QUIT_PLUTO(1);
  }
#endif

/* --------------------------------------------------------
   0b. Check the number of processors in each direction
   -------------------------------------------------------- */

  DIM_EXPAND(par_dim[0] = grid->nproc[IDIR] > 1;  ,
             par_dim[1] = grid->nproc[JDIR] > 1;  ,
             par_dim[2] = grid->nproc[KDIR] > 1;)

/* --------------------------------------------------------
   1. Call userdef internal boundary with side == 0
   --------------------------------------------------------  */

#if INTERNAL_BOUNDARY == YES
  UserDefBoundary (d, NULL, 0, grid);
#endif
  
#if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  RBoxDefine (IBEG, IEND, JBEG, JEND, KBEG, KEND, CENTER, &center_box);
  CT_ComputeCharge (d, &center_box, grid);
#endif

/* --------------------------------------------------------
   2. Exchange data between processors
   -------------------------------------------------------- */
   
#ifdef PARALLEL
  MPI_Barrier (MPI_COMM_WORLD);
  NVAR_LOOP(nv) AL_Exchange_dim ((char *)Vc[nv][0][0], par_dim, SZ);
  #ifdef STAGGERED_MHD 
  DIM_EXPAND(
    AL_Exchange_dim ((char *)(Vs[BX1s][0][0] - 1), par_dim, SZ_stagx);  ,
    AL_Exchange_dim ((char *) Vs[BX2s][0][-1]    , par_dim, SZ_stagy);  ,
    AL_Exchange_dim ((char *) Vs[BX3s][-1][0]    , par_dim, SZ_stagz);)

  #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  AL_Exchange_dim ((char *)d->q[0][0], par_dim, SZ); 
  DIM_EXPAND(
    AL_Exchange_dim ((char *)(Vs[EX1s][0][0] - 1), par_dim, SZ_stagx);  ,
    AL_Exchange_dim ((char *) Vs[EX2s][0][-1]    , par_dim, SZ_stagy);  ,
    AL_Exchange_dim ((char *) Vs[EX3s][-1][0]    , par_dim, SZ_stagz);)
  #endif
  
  #endif
  MPI_Barrier (MPI_COMM_WORLD);
#endif

/* ---------------------------------------------------------
   3. Boundaries are imposed on ALL sides: a loop from 
      sbeg = 0 to send = 2*3 - 1 is performed. 
   -------------------------------------------------------- */ 

  sbeg = 0;
  send = 2*3 - 1;

/* --------------------------------------------------------
   4. Main loop on computational domain sides
   -------------------------------------------------------- */

  type[0] = lbound[IDIR]*INCLUDE_IDIR; type[1] = rbound[IDIR]*INCLUDE_IDIR;
  type[2] = lbound[JDIR]*INCLUDE_JDIR; type[3] = rbound[JDIR]*INCLUDE_JDIR;
  type[4] = lbound[KDIR]*INCLUDE_KDIR; type[5] = rbound[KDIR]*INCLUDE_KDIR;

  for (is = sbeg; is <= send; is++){

    if (type[is] == 0) continue;  /* No physical boundary or non-active  *
                                   * dimension: skip                     */
  /* ------------------------------------------------------
     5. Define boundary boxes, sweeping direction. 
     ------------------------------------------------------ */

    ib = 0; ie = NX1_TOT-1;
    jb = 0; je = NX2_TOT-1;
    kb = 0; ke = NX3_TOT-1;

    isb = -1; ise = ie;  /* Set default indices for normal staggered directions. */
    jsb = -1; jse = je;  /* Changed below when the boundary box is to the left */
    ksb = -1; kse = ke;  /* or to the right of a given direction. */ 

    if      (side[is] == X1_BEG) {
      ib  = IBEG-1; ie  = 0;
      isb = ib-1;   ise = ie-1;
    } else if (side[is] == X1_END) {
      ib  = IEND+1; ie  = NX1_TOT-1;
      isb = ib;     ise = ie;
    } else if (side[is] == X2_BEG) {
      jb  = JBEG-1; je  = 0;
      jsb = jb-1;   jse = je-1;
    } else if (side[is] == X2_END) {
      jb  = JEND+1; je  = NX2_TOT-1;
      jsb = jb;     jse = je;
    } else if (side[is] == X3_BEG) {
      kb  = KBEG-1; ke  = 0;
      ksb = kb-1;   kse = ke-1;
    } else if (side[is] == X3_END) {
      kb  = KEND+1; ke  = NX3_TOT-1;
      ksb = kb;     kse = ke;
    }
    
    RBoxDefine (ib, ie, jb, je, kb, ke, CENTER, &center_box);

    #ifdef STAGGERED_MHD

    /* -- Define RBoxes for staggered fields -- */

    RBoxDefine (isb, ise,  jb, je,  kb,  ke,  X1FACE, &x1face_box); 
    RBoxDefine (ib,   ie, jsb, jse, kb,  ke,  X2FACE, &x2face_box); 
    RBoxDefine (ib,   ie,  jb, je,  ksb, kse, X3FACE, &x3face_box); 

    #endif /* STAGGERED_MHD */
    
  /* ------------------------------------------------------
     6. Apply boundary conditions.
     ------------------------------------------------------ */

    if (type[is] == OUTFLOW) {

    /* ----------------------------------------------------
       6a. [OUTFLOW] Boundary Conditions.
       ---------------------------------------------------- */

      NVAR_LOOP(nv) OutflowBoundary (Vc[nv], &center_box, side[is]);

    /* -- Assign b.c. on transverse components in staggered MHD -- */
    
      #ifdef STAGGERED_MHD
      #if INCLUDE_IDIR
      if (side[is] != X1_BEG && side[is] != X1_END) {
        CT_ASSIGN(OutflowBoundary (Vs[BX1s], &x1face_box, side[is]);  ,
                  OutflowBoundary (Vs[EX1s], &x1face_box, side[is]); )
      }
      #endif

      #if INCLUDE_JDIR
      if (side[is] != X2_BEG && side[is] != X2_END) {
        CT_ASSIGN(OutflowBoundary (Vs[BX2s], &x2face_box, side[is]);  ,
                  OutflowBoundary (Vs[EX2s], &x2face_box, side[is]); )
      }
      #endif

      #if INCLUDE_KDIR
      if (side[is] != X3_BEG && side[is] != X3_END) {  
        CT_ASSIGN(OutflowBoundary (Vs[BX3s], &x3face_box, side[is]); ,
                  OutflowBoundary (Vs[EX3s], &x3face_box, side[is]); )
      }
      #endif

    /* ------------------------------------------
       Recover normal magnetic field and average
       it to cell center. Transverse components
       are assigned consistently with
       cell-centered quantities.
       ------------------------------------------ */

      FillMagneticField (d, side[is], grid);
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      OutflowBoundary (d->q, &center_box, side[is]);
      FillElectricField (d, side[is], grid);
      #endif
      #endif  /* STAGGERED_MHD */

    }else if (  (type[is] == REFLECTIVE) 
             || (type[is] == AXISYMMETRIC)
             || (type[is] == EQTSYMMETRIC)){ 

    /* ----------------------------------------------------
       6b. [REFLECTIVE/AXISYMMETRIC/EQTSYMMETRIC]
           Boundary Conditions.
       ---------------------------------------------------- */
    
      FlipSign (side[is], type[is], vsign);
      NVAR_LOOP(nv) ReflectiveBoundary (Vc[nv], vsign[nv], 0, &center_box, side[is]);

    /* -- Assign b.c. on both normal & transverse components in staggered MHD -- */

      #ifdef STAGGERED_MHD
      int stag[3]={0,0,0};
      if (side[is] == X1_BEG || side[is] == X1_END) stag[IDIR] = 1;
      if (side[is] == X2_BEG || side[is] == X2_END) stag[JDIR] = 1;
      if (side[is] == X3_BEG || side[is] == X3_END) stag[KDIR] = 1;

      DIM_EXPAND(
        ReflectiveBoundary(Vs[BX1s], vsign[BX1], stag[IDIR], &x1face_box, side[is]);  ,
        ReflectiveBoundary(Vs[BX2s], vsign[BX2], stag[JDIR], &x2face_box, side[is]);  ,
        ReflectiveBoundary(Vs[BX3s], vsign[BX3], stag[KDIR], &x3face_box, side[is]);)

      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(
        ReflectiveBoundary(Vs[EX1s], vsign[EX1], stag[IDIR], &x1face_box, side[is]);  ,
        ReflectiveBoundary(Vs[EX2s], vsign[EX2], stag[JDIR], &x2face_box, side[is]);  ,
        ReflectiveBoundary(Vs[EX3s], vsign[EX3], stag[KDIR], &x3face_box, side[is]);)
      #endif
      #endif  /* STAGGERED_MHD */

    }else if (type[is] == PERIODIC){  /* -- Periodic B.C. (serial or 1 proc) -- */

    /* ----------------------------------------------------
       6c. [PERIODIC] Boundary Conditions.
           Assigned only if the direction is not parallel
           (par_dim[is/2] == NO).
           NOTE: for staggered meshes we overwrite the
           leftmost face value with the rightmost one, as
           done in parallel.
       ---------------------------------------------------- */

      #ifdef STAGGERED_MHD
      if (side[is] == X1_BEG) RBoxDefine (ib, ie-1, jb, je  , kb, ke,   X1FACE, &x1face_box);
      if (side[is] == X2_BEG) RBoxDefine (ib, ie,   jb, je-1, kb, ke,   X2FACE, &x2face_box);
      if (side[is] == X3_BEG) RBoxDefine (ib, ie,   jb, je  , kb, ke-1, X3FACE, &x3face_box);
      #endif

      if (!par_dim[is/2]) {
        NVAR_LOOP(nv)  PeriodicBoundary(Vc[nv], &center_box, side[is]);
        #ifdef STAGGERED_MHD
        DIM_EXPAND(PeriodicBoundary(Vs[BX1s], &x1face_box, side[is]);  ,
                   PeriodicBoundary(Vs[BX2s], &x2face_box, side[is]);  ,
                   PeriodicBoundary(Vs[BX3s], &x3face_box, side[is]);)
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        DIM_EXPAND(PeriodicBoundary(Vs[EX1s], &x1face_box, side[is]);  ,
                   PeriodicBoundary(Vs[EX2s], &x2face_box, side[is]);  ,
                   PeriodicBoundary(Vs[EX3s], &x3face_box, side[is]);)
        #endif
        #endif
      }

    }else if (type[is] == POLARAXIS){  /* -- Singular axis condition -- */

    /* ----------------------------------------------------
       6d. [POLARAXIS] Boundary Conditions.
       ---------------------------------------------------- */

      #if GEOMETRY == POLAR
      if (side[is] != X1_BEG){
        printLog ("! Boundary(): polaraxis can only be assigned at an X1 boundary\n");
        QUIT_PLUTO(1);
      }
      if (grid->nproc[JDIR] != 1){
        printLog ("! Boundary(): polaraxis b.c. does not allow ");
        printLog ("parallelization in phi dir");
        QUIT_PLUTO(1);
      }
      #elif GEOMETRY == SPHERICAL
      if (side[is] != X2_BEG && side[is] != X2_END){
        printLog ("! Boundary(): polaraxis can only be assigned at an X2 boundary\n");
        QUIT_PLUTO(1);
      }
      if (grid->nproc[KDIR] != 1){
        printLog ("! Boundary(): polaraxis b.c. does not allow ");
        printLog ("parallelization in phi dir");
        QUIT_PLUTO(1);
      }
      #endif

      PolarAxisBoundary(d, &center_box, side[is]);

    }else if (type[is] == SHEARING) {

    /* ----------------------------------------------------
       6e. [SHEARING] Boundary Conditions.
           SHEARING-BOX boundary condition is
           implemented as

           1) apply periodic b.c. for all variables
              (except staggered BX)
           2) Perform spatial shift in the y-direction
       ---------------------------------------------------- */
    
      #ifdef SHEARINGBOX
      if (side[is] != X1_BEG && side[is] != X1_END){
        printLog ("! Boundary(): shearingbox can only be assigned at an X1 boundary\n");
        QUIT_PLUTO(1);
      }
      if (grid->nproc[IDIR] == 1){
        NVAR_LOOP(nv) PeriodicBoundary(Vc[nv], &center_box, side[is]);
        #ifdef STAGGERED_MHD
        DIM_EXPAND(                                                    ;  ,
                   PeriodicBoundary(Vs[BX2s], &x2face_box, side[is]);  ,
                   PeriodicBoundary(Vs[BX3s], &x3face_box, side[is]);)
        #endif
      }
      SB_Boundary (d, side[is], grid);

     /* -- assign normal component of staggered B 
           using the div.B = 0 condition           -- */

      #ifdef STAGGERED_MHD
      FillMagneticField (d, side[is], grid);  
      #if PHYSICS == ResRMHD
      printLog ("! Boundary(): shearingbox module not compatible with ResRMHD\n");
      QUIT_PLUTO(1);
      #endif
      #endif
      #else
      printLog ("! Boundary(): shearingbox module not loaded\n");
      QUIT_PLUTO(1);
      #endif  /* #ifdef SHEARINGBOX */

    }else if (type[is] == ROTATED) { 

    /* ----------------------------------------------------
       6f. [ROTATED] Boundary Conditions.
       ---------------------------------------------------- */

      NVAR_LOOP(nv) RotateBoundary(Vc[nv], &center_box, side[is]);

      #ifdef STAGGERED_MHD
      DIM_EXPAND(RotateBoundary(Vs[BX1s], &x1face_box, side[is]);  ,
                 RotateBoundary(Vs[BX2s], &x2face_box, side[is]);  ,
                 RotateBoundary(Vs[BX3s], &x3face_box, side[is]);)
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(RotateBoundary(Vs[EX1s], &x1face_box, side[is]);  ,
                 RotateBoundary(Vs[EX2s], &x2face_box, side[is]);  ,
                 RotateBoundary(Vs[EX3s], &x3face_box, side[is]);)
      #endif
      #endif

    }else if (type[is] == USERDEF) { 

    /* ----------------------------------------------------
       6g. [USERDEF] Boundary Conditions.
       ---------------------------------------------------- */
    
      #ifdef GLM_MHD
      BOX_LOOP(&center_box, k, j, i) {
        Vc[PSI_GLM][k][j][i] = 0.0;
        #ifdef PHI_GLM
        Vc[PHI_GLM][k][j][i] = 0.0;
        #endif
      }  
      #endif

      UserDefBoundary (d, &center_box, side[is], grid);
      #ifdef STAGGERED_MHD
      DIM_EXPAND(UserDefBoundary (d, &x1face_box, side[is], grid);  , /* Only 2 needed */
                 UserDefBoundary (d, &x2face_box, side[is], grid);  ,
                 UserDefBoundary (d, &x3face_box, side[is], grid);)

      /* -- assign normal component of staggered B 
            using the div.B = 0 condition           -- */

      FillMagneticField (d, side[is], grid);
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      FillElectricField (d, side[is], grid);
      #endif
      #endif

    } /* end if (type[is]) */

  /* ------------------------------------------------------
     7. Redefine cell-center field in ghost zones from
         staggered components.
         Note that this defines cell-centered d->Uc
         and then we need to further copy on Vc.
     ------------------------------------------------------ */

    #ifdef STAGGERED_MHD
    #ifdef HIGH_ORDER
    if (type[is] != OUTFLOW) continue;
    #endif
    CT_AverageStaggeredFields (d, 0, &center_box, grid);
    BOX_LOOP(&center_box, k,j,i){
      DIM_EXPAND(Vc[BX1][k][j][i] = d->Uc[k][j][i][BX1];  ,
                 Vc[BX2][k][j][i] = d->Uc[k][j][i][BX2];  ,
                 Vc[BX3][k][j][i] = d->Uc[k][j][i][BX3];)
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(Vc[EX1][k][j][i] = d->Uc[k][j][i][EX1];  ,
                 Vc[EX2][k][j][i] = d->Uc[k][j][i][EX2];  ,
                 Vc[EX3][k][j][i] = d->Uc[k][j][i][EX3];)
      #endif
    }               
    #endif   /* STAGGERED_MHD             */

  } /* end for (is = sbeg, send) */

/* -------------------------------------------------------- 
   8. Compute entropy for the next time level
   -------------------------------------------------------- */

#if ENTROPY_SWITCH
  ComputeEntropy (d, grid);
#endif

#if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  RBoxDefine (0, NX1_TOT-1, 0, NX2_TOT-1, 0, NX3_TOT-1, CENTER, &center_box);
  CT_ComputeCharge (d, &center_box, grid);
#endif

}

/* ********************************************************************* */
void FlipSign (int side, int type, int *vsign)
/*!
 * Reverse the sign of vector components with respect to axis side. 
 * Depending on type, one needs to symmetrize or anti-symmetrize:
 *
 * - REFLECTIVE:    \n
 *   o   Vn -> -Vn,  Bn -> -Bn,  En ->  En  \n
 *   o   Vp ->  Vp,  Bp ->  Bp,  Ep -> -Ep  \n 
 *   o   Vt ->  Vt,  Bt ->  Bt,  Et -> -Et
 *
 * - AXISYMMETRIC:  \n
 *   o   Vn -> -Vn,  Bn -> -Bn,  En -> -En \n
 *   o   Vp ->  Vp,  Bp ->  Bp,  Ep ->  Ep \n
 *   o   Vt -> -Vt,  Bt -> -Bt,  Et -> -Et
 *
 * - EQTSYMMETRIC:  \n 
 *   o   Vn -> -Vn,  Bn ->  Bn,  En -> -En  \n
 *   o   Vp ->  Vp,  Bp -> -Bp,  Ep ->  Ep  \n
 *   o   Vt ->  Vt,  Bt -> -Bt,  Et ->  Et
 *
 * where (n) is the normal components, (p) and (t)
 * are the transverse (or poloidal and toroidal for
 * cylindrical and spherical coordinates) components.
 * 
 * If the radiation module is used, the transformation law
 * for the radiation flux is the same as for the velocities.
 * 
 * \param [in]  side  boundary side
 * \param [in]  type boundary condition type
 * \param [out] vsign an array of values (+1 or -1) giving the sign
 *********************************************************************** */
{
  int nv;
  
/* ----------------------------------------------------------
    get normal (n), tangent (t) and bitangent (b) vector 
    components. The ordering is the same as in SetIndexes()
   ---------------------------------------------------------- */

  if (side == X1_BEG || side == X1_END){
    SetVectorIndices (IDIR);
  }else if (side == X2_BEG || side == X2_END){
    SetVectorIndices (JDIR);
  }else if (side == X3_BEG || side == X3_END){
    SetVectorIndices (KDIR);
  }

/* ---------------------------------------
     decide which variable flips sign
   --------------------------------------- */

  NVAR_LOOP(nv) vsign[nv] = 1.0;

  vsign[VXn] = -1.0;
  #if (PHYSICS == MHD) || (PHYSICS == RMHD)  || (PHYSICS == ResRMHD)
  vsign[BXn] = -1.0;
  #endif
  #if DUST_FLUID == YES
  vsign[VXn_D] = -1.0;
  #endif
  
  if (type == REFLECTIVE){
    #if PHYSICS == ResRMHD
    vsign[EXt] = -1.0;
    vsign[EXb] = -1.0;
    #endif 
    #if RADIATION
    vsign[FRn] = -1.0;
    #endif
  }  

  if (type == AXISYMMETRIC){

    #if GEOMETRY != CARTESIAN
    vsign[iVPHI] = -1.0;
    #if (PHYSICS == MHD) || (PHYSICS == RMHD)  || (PHYSICS == ResRMHD)
    vsign[iBPHI] = -1.0;
    #endif
    #endif

    #if DUST_FLUID == YES
    vsign[VXt_D] = -1.0;
    #endif

    #if PHYSICS == ResRMHD
    vsign[EXn] = -1.0;
    vsign[EXt] = -1.0;
    #endif 

    #if RADIATION
    vsign[FRn] = -1.0;
    vsign[FRt] = -1.0;
    #endif
  }

  if (type == EQTSYMMETRIC){
    #if (PHYSICS == MHD) || (PHYSICS == RMHD)  || (PHYSICS == ResRMHD)
    vsign[BXn] =  1.0;
    vsign[BXt] = -1.0;
    vsign[BXb] = -1.0;
    #ifdef GLM_MHD 
    vsign[PSI_GLM] = -1.0;
    #endif
    #if PHYSICS == ResRMHD
    vsign[EXn] = -1.0;
    #endif 
    #endif
    #if RADIATION
    vsign[FRn] = -1.0;
    #endif
  }
}

/* ********************************************************************* */
void OutflowBoundary (double ***q, RBox *box, int side)
/*! 
 * Impose zero-gradient boundary conditions on 'q' on 
 * the boundary side specified by 'side'.
 * The input array 'q' must not represent the normal component
 * of a staggered magnetic fied.
 *
 * \param [in,out] q     a 3D array requiring ghost zone filling
 * \param [in]     box   pointer to a RBox structure defining the
 *                       extent of the boundary region
 * \param [in]     side  the side of the computational domain.
 *********************************************************************** */
{
  int  i, j, k;

  if (side == X1_BEG) { 

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k][j][IBEG];   

  }else if (side == X1_END){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k][j][IEND];   

  }else if (side == X2_BEG){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k][JBEG][i];

  }else if (side == X2_END){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k][JEND][i];

  }else if (side == X3_BEG){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[KBEG][j][i];

  }else if (side == X3_END){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[KEND][j][i];

  }
}

/* ********************************************************************* */
void PeriodicBoundary (double ***q, RBox *box, int side)
/*!
 * Implements periodic boundary conditions in serial mode or when 
 * one processor only handle the periodic direction.
 *
 * \param [in,out] q     a 3D array requiring ghost zone filling
 * \param [in]     box   pointer to a RBox structure defining the
 *                       extent of the boundary region
 * \param [in]     side  the side of the computational domain.
 *********************************************************************** */
{
  int  i, j, k;

  if (side == X1_BEG){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k][j][i + NX1];

  }else if (side == X1_END){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k][j][i - NX1];

  }else if (side == X2_BEG){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k][j + NX2][i];

  }else if (side == X2_END){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k][j - NX2][i];

  }else if (side == X3_BEG){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k + NX3][j][i];

  }else if (side == X3_END){

    BOX_LOOP(box,k,j,i) q[k][j][i] = q[k - NX3][j][i];

  }
}

/* ********************************************************************* */
void ReflectiveBoundary (double ***q, int s, int stag, RBox *box, int side)
/*!
 * Make symmetric (s = 1) or anti-symmetric (s = -1) profiles 
 * with respect to the boundary plane specified by box->side.
 * The sign is set by the FlipSign() function. 
 *
 * \param [in,out] q     a 3D array requiring ghost zone filling
 * \param [in]     s     an integer taking only the values +1 (symmetric 
 *                       profile) or -1 (antisymmetric profile)
 * \param [in]     stag  an integer taking the values 0 (centered with
 *                       respect to the boundary) or 1 (staggered with respect
 *                       to the boundary).
 * \param [in]     box   pointer to a RBox structure defining the
 *                       extent of the boundary region
 * \param [in]     side  the side of the computational domain.
 *********************************************************************** */
{
  int   i, j, k;
  int   i0, j0, k0;

  if (side == X1_BEG) {   

    i0 = 2*IBEG - 1 - stag;
    BOX_LOOP(box,k,j,i) q[k][j][i] = s*q[k][j][i0-i];
/*
if (stag == 1){
  int ibeg = MIN(box->ibeg, box->iend);
  int iend = MAX(box->ibeg, box->iend);
  int jbeg = MIN(box->jbeg, box->jend);
  int jend = MAX(box->jbeg, box->jend);
  int kbeg = MIN(box->kbeg, box->kend);
  int kend = MAX(box->kbeg, box->kend);

  for (k = kbeg; k <= kend; k++){
  for (j = jbeg; j <= jend; j++){
  for (i = ibeg; i <= iend; i++){
//    printf ("i <--> i0 = %d <--> %d  j,k = %d  %d\n",i,i0-i,j,k);
  }}}
}
*/
  }else if (side == X1_END){  

    i0 = 2*IEND + 1 - stag;
    BOX_LOOP(box,k,j,i) q[k][j][i] = s*q[k][j][i0-i];

  }else if (side == X2_BEG){  

    j0 = 2*JBEG - 1 - stag;
    BOX_LOOP(box,k,j,i) q[k][j][i] = s*q[k][j0-j][i];

  }else if (side == X2_END){

    j0 = 2*JEND + 1 - stag;
    BOX_LOOP(box,k,j,i) q[k][j][i] = s*q[k][j0-j][i];

  }else if (side == X3_BEG){

    k0 = 2*KBEG - 1 - stag;
    BOX_LOOP(box,k,j,i) q[k][j][i] = s*q[k0-k][j][i];

  }else if (side == X3_END){

    k0 = 2*KEND + 1 - stag;
    BOX_LOOP(box,k,j,i) q[k][j][i] = s*q[k0-k][j][i];

  }
}

/* ********************************************************************* */
void PolarAxisBoundary(const Data *d, RBox *box, int side)
/*!
 * Boundary conditions on singular axis.
 *
 *********************************************************************** */
{
  int i,j,k, nv;
  int j1,i1,k1;

#ifdef STAGGERED_MHD
  print ("! PolarAxisBoundary(): not implemented for STAGGERED_MHD\n");
  QUIT_PLUTO(1);
#endif

#if GEOMETRY == POLAR
  if (side == X1_BEG){  /* -- X1_BEG boundary -- */
    if (box->vpos == CENTER) {
      BOX_LOOP(box,k,j,i){
        j1 = j + NX2/2;
        if (j1 > JEND) j1 -= NX2;
        i1 = 2*IBEG - i - 1;  /* Mirror point at on other side of ring */   
        NVAR_LOOP(nv) d->Vc[nv][k][j][i] = d->Vc[nv][k][j1][i1];
        d->Vc[VX1][k][j][i] *= -1.0;
        d->Vc[VX2][k][j][i] *= -1.0;
        #if PHYSICS == MHD
        d->Vc[BX1][k][j][i] *= -1.0;
        d->Vc[BX2][k][j][i] *= -1.0;
        #endif
        #if RADIATION
        d->Vc[FR1][k][j][i] *= -1.0;
        d->Vc[FR2][k][j][i] *= -1.0;
        #endif
      }
    }else if (box->vpos == X1FACE){
      BOX_LOOP(box,k,j,i){  }
    }else if (box->vpos == X2FACE){
      BOX_LOOP(box,k,j,i){  }
    }else if (box->vpos == X3FACE){
      BOX_LOOP(box,k,j,i){  }
    }
    
  }
#endif

#if GEOMETRY == SPHERICAL
  if (side == X2_BEG || side == X2_END){  /* -- X1_BEG boundary -- */
    if (box->vpos == CENTER) {
      BOX_LOOP(box,k,j,i){
        k1 = k + NX3/2;
        if (k1 > KEND) k1 -= NX3;
        if (side == X2_BEG) j1 = 2*JBEG - j - 1; /* Mirror point on other side of ring */
        if (side == X2_END) j1 = 2*JEND - j + 1; /* Mirror point on other side of ring */
        NVAR_LOOP(nv) d->Vc[nv][k][j][i] = d->Vc[nv][k1][j1][i];
        d->Vc[VX2][k][j][i] *= -1.0;
        d->Vc[VX3][k][j][i] *= -1.0;
        #if PHYSICS == MHD
        d->Vc[BX2][k][j][i] *= -1.0;
        d->Vc[BX3][k][j][i] *= -1.0;
        #endif
        #if RADIATION
        d->Vc[FR2][k][j][i] *= -1.0;
        d->Vc[FR3][k][j][i] *= -1.0;
        #endif
      }
    }else if (box->vpos == X1FACE){
      BOX_LOOP(box,k,j,i){  }
    }else if (box->vpos == X2FACE){
      BOX_LOOP(box,k,j,i){  }
    }else if (box->vpos == X3FACE){
      BOX_LOOP(box,k,j,i){  }
    }
    
  }
#endif


  return;
}
