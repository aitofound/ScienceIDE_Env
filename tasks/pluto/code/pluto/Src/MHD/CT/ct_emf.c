/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief  Store or retrieve the Electromotive Force (EMF).              

  This file provides a database functionality for storing or 
  retrieving EMF components and related information at different 
  points and times in the code.

  The CT_Allocate() allocates memory for EMF structure members.
  The CT_StoreEMF() function is called immediately after a 1D Riemann 
  solver during the hydro sweeps in order to save Fluxes at cell
  interfaces and characteristic signal velocities into the emf
  structure for later reuse. 
  The fluxes coming from different sweeps are the different components
  of the advective part (-v X B) of the electric field.

  The function CT_GetEMF() is used to obtain the edge-centered electric 
  field by properly averaging the EMF components previously stored
  at the zone faces during the 1D sweeps.

  \author  A. Mignone (andrea.mignone@unito.it)
           V. Berta   (vittoria.berta@unito.it)\n

  \date    Feb 14, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

void CT_DeconvolveEMF (const Data *data, const EMF *emf, Grid *grid);

/* ********************************************************************* */
void CT_Allocate (EMF *emf0)
/*!
 *  Allocate memory for EMF structure and 
 *  check for incompatible combinations of algorithms.
 *
 *  CT AVERAGE  |E(sweep)|E(pnt)|E(dff)|sgn(vs)|S(L/R)|  vt  |a(L/R)|d(L/R)
 *  ------------|--------|------|------|-------|------|------|------|------
 *  ARITHMETIC  |  x x   |      |      |       |      |      |      |     
 *  CT_CONTACT  |  x x   |      |      |   x   |      |      |      |
 *  CT_FLUX     |        | x  x | x  x |       |      |      |      |
 *  UCT_HLL     |        |      |      |       | x  x | x  x |      |
 *  UCT_HLLD    |        |      |      |       |      | x  x | x  x | x  x
 *  UCT_GFORCE  |        |      |      |       |      | x  x |      | x  x
 *
 *  The "x" represent how many 3D arrays per interface are required.
 *********************************************************************** */
{

#if PARTICLES
  #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES)
    #if (CT_EMF_AVERAGE != ARITHMETIC) && (CT_EMF_AVERAGE != UCT_CONTACT) && \
        (CT_EMF_AVERAGE != CT_FLUX)
      printf ("! CT_Allocate(): EMF average not compatible with particles.\n");
      printf ("  Use ARITHMETIC / CT_CONTACT instead\n");
      QUIT_PLUTO(1);
    #endif
  #endif
#endif

  emf0->ibeg = emf0->iend = 0;
  emf0->jbeg = emf0->jend = 0;
  emf0->kbeg = emf0->kend = 0;


  emf0->dxL  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->dxR  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->axL  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->axR  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

  emf0->dyL  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->dyR  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->ayL  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->ayR  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

  emf0->dzL  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->dzR  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->azL  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->azR  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

/* --------------------------------------------------------
   1. Allocate memory for edge-centered ("e") electric
      and magnetic (only for ResRMHD) fields
   -------------------------------------------------------- */

  DIM_EXPAND(                                                      ;  ,
           emf0->Ex3e = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
           emf0->Ex1e = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->Ex2e = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)

  #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  DIM_EXPAND(                                                      ;  ,
           emf0->Bx3e = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
           emf0->Bx1e = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->Bx2e = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)
  #endif

#if CT_EMF_AVERAGE == CT_CONTACT
  DIM_EXPAND(emf0->svx = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, signed char);  ,
             emf0->svy = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, signed char);  ,
             emf0->svz = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, signed char);)
#endif

/* --------------------------------------------------------
   2. Allocate memory for face-centered electric and
      magentic fields
   -------------------------------------------------------- */

#if    (CT_EMF_AVERAGE == UCT_HLL) \
    || (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == UCT_GFORCE)

  DIM_EXPAND(emf0->SxL = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->SxR = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,

           emf0->SyL = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->SyR = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,

           emf0->SzL = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->SzR = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)
#endif

  DIM_EXPAND(emf0->ezi = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
                                                                  ;  ,
           emf0->eyi = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)
  DIM_EXPAND(                                                       ;  ,
           emf0->ezj = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
           emf0->exj = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)
  DIM_EXPAND(                                                       ;  ,
                                                                  ;  , 
           emf0->exk = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->eyk = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)


  DIM_EXPAND(emf0->ezi_dff = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
                                                                      ;  ,
           emf0->eyi_dff = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)
  DIM_EXPAND(                                                           ;  ,
           emf0->ezj_dff = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,
           emf0->exj_dff = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)
  DIM_EXPAND(                                                           ;  ,
                                                                      ;  , 
           emf0->exk_dff = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->eyk_dff = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)

  #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  DIM_EXPAND(                                                       ;  ,
           emf0->Bzi = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double); 
           emf0->Bzj = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);  ,

           emf0->Bxj = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->Bxk = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

           emf0->Byi = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
           emf0->Byk = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);)

  #endif

  #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  emf0->Frho_i = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->Frho_j = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  emf0->Frho_k = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  #endif

}

#define eps_CT_CONTACT   1.e-6
/* ********************************************************************* */
void CT_StoreUpwindEMF (const Sweep *sweep, EMF *emf, int beg, int end,
                        Grid *grid)
/*!
 * Store EMF components and related information available 
 * during 1D sweeps.
 *
 * \param [in]  sweep  pointer to Sweep structure
 * \param [in]  beg    initial index of computation 
 * \param [in]  end    final   index of computation(j = 0; j < ntot; j++)
 * \param [in]  grid   pointer to Grid structure;
 *
 * \return  This function has no return value.
 *********************************************************************** */
{
  int i, j, k, s;

#if (HALL_MHD) && (CT_EMF_AVERAGE != ARITHMETIC)
  #error HALL_MHD should be used with ARITHMETIC average.
#endif

#if (DIMENSIONS == 3) && !INCLUDE_JDIR  // HOT FIX
  emf->jbeg = emf->jend = 0;
#endif

/* ------------------------------------------------------
     Store emf components or other necessary 1-D data
   ------------------------------------------------------ */

  if (g_dir == IDIR){

    emf->ibeg = beg; emf->iend = end;
    j = g_j;
    k = g_k;
    for (i = beg; i <= end; i++) {
      
      #if    (CT_EMF_AVERAGE == ARITHMETIC)  \
          || (CT_EMF_AVERAGE == CT_CONTACT) || (CT_EMF_AVERAGE == UCT0)
      DIM_EXPAND(emf->ezi[k][j][i] = -sweep->flux[i][BX2];  ,
                                                            ,
                 emf->eyi[k][j][i] =  sweep->flux[i][BX3]; ) 
      #if CT_EMF_AVERAGE == CT_CONTACT
      if      (sweep->flux[i][RHO] >  eps_CT_CONTACT) s = 1;
      else if (sweep->flux[i][RHO] < -eps_CT_CONTACT) s = -1;
      else s = 0;

      emf->svx[k][j][i] = s;
      #endif

      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(emf->Bzi[k][j][i] =  sweep->flux[i][EX2];  ,
                                                            ,
                 emf->Byi[k][j][i] = -sweep->flux[i][EX3]; ) 
      #endif
      #endif

      #if CT_EMF_AVERAGE == CT_FLUX
      DIM_EXPAND(                                                 ;  ,
                 emf->ezi    [k][j][i] = - sweep->pnt_flux[i][BX2];
                 emf->ezi_dff[k][j][i] = - sweep->dff_flux[i][BX2];  ,
                 emf->eyi    [k][j][i] =   sweep->pnt_flux[i][BX3];
                 emf->eyi_dff[k][j][i] =   sweep->dff_flux[i][BX3];)
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(                                                 ;  ,
                 emf->Bzi[k][j][i] =       sweep->pnt_flux[i][EX2]
                                     + 2.0*sweep->dff_flux[i][EX2];  ,
                 emf->Byi[k][j][i] =     - sweep->pnt_flux[i][EX3]
                                     - 2.0*sweep->dff_flux[i][EX3];)
      #endif
      #endif /* CT_EMF_AVERAGE == CT_FLUX */

      #if    (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == UCT_GFORCE) \
          || (CT_EMF_AVERAGE == UCT_HLL)
      DIM_EXPAND(                                                 ;  ,
                 emf->ezi    [k][j][i] = - sweep->pnt_flux[i][BX2];
                 emf->ezi_dff[k][j][i] = - sweep->dff_flux[i][BX2];  ,
                 emf->eyi    [k][j][i] =   sweep->pnt_flux[i][BX3];
                 emf->eyi_dff[k][j][i] =   sweep->dff_flux[i][BX3];)

      emf->SxL[k][j][i]  = sweep->SL[i]; 
      emf->SxR[k][j][i]  = sweep->SR[i];
      emf->dxL[k][j][i]  = sweep->dL[i]; 
      emf->dxR[k][j][i]  = sweep->dR[i];
      emf->axL[k][j][i]  = sweep->aL[i]; 
      emf->axR[k][j][i]  = sweep->aR[i];
      #endif /* (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == UCT_GFORCE) */

      #if (CT_EMF_AVERAGE == CT_MAXWELL)
      DIM_EXPAND(                                             ;  ,
                 emf->ezi[k][j][i] = - sweep->pnt_flux[i][BX2];  ,
                 emf->eyi[k][j][i] =   sweep->pnt_flux[i][BX3];)
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(                                             ;  ,
                 emf->Bzi[k][j][i] =   sweep->pnt_flux[i][EX2];  ,
                 emf->Byi[k][j][i] = - sweep->pnt_flux[i][EX3];)
      #endif
      #endif

      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      emf->Frho_i[k][j][i] = sweep->flux[i][RHO];
      #endif

    }

  }else if (g_dir == JDIR){

    emf->jbeg = beg; emf->jend = end;
    i = g_i;
    k = g_k;
    for (j = beg; j <= end; j++) {
      #if    (CT_EMF_AVERAGE == ARITHMETIC) \
          || (CT_EMF_AVERAGE == CT_CONTACT) \
          || (CT_EMF_AVERAGE == UCT0)
      DIM_EXPAND(                                            ;   ,
               emf->ezj[k][j][i] =  sweep->flux[j][BX1];   ,
               emf->exj[k][j][i] = -sweep->flux[j][BX3]; )
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(                                            ;   ,
               emf->Bzj[k][j][i] = -sweep->flux[j][EX1];   ,
               emf->Bxj[k][j][i] =  sweep->flux[j][EX3]; )
      #endif
      #if CT_EMF_AVERAGE == CT_CONTACT
      if      (sweep->flux[j][RHO] >  eps_CT_CONTACT) s =  1;
      else if (sweep->flux[j][RHO] < -eps_CT_CONTACT) s = -1;
      else s = 0;
      emf->svy[k][j][i] = s;
      #endif
      #endif

      #if CT_EMF_AVERAGE == CT_FLUX
      DIM_EXPAND(                                                   ;  ,
                 emf->ezj    [k][j][i] =   sweep->pnt_flux[j][BX1];
                 emf->ezj_dff[k][j][i] =   sweep->dff_flux[j][BX1];  ,
                 emf->exj    [k][j][i] = - sweep->pnt_flux[j][BX3];
                 emf->exj_dff[k][j][i] = - sweep->dff_flux[j][BX3];)
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(                                                   ;  ,
                 emf->Bzj[k][j][i] =     - sweep->pnt_flux[j][EX1]
                                     - 2.0*sweep->dff_flux[j][EX1];  ,
                 emf->Bxj[k][j][i] =       sweep->pnt_flux[j][EX3]
                                     + 2.0*sweep->dff_flux[j][EX3];)
      #endif
      #endif  /* CT_EMF_AVERAGE == FLUX */
      
      #if    (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == UCT_GFORCE) \
          || (CT_EMF_AVERAGE == UCT_HLL)
      DIM_EXPAND(                                                 ;  ,
                 emf->ezj    [k][j][i] =   sweep->pnt_flux[j][BX1];
                 emf->ezj_dff[k][j][i] =   sweep->dff_flux[j][BX1];  ,
                 emf->exj    [k][j][i] = - sweep->pnt_flux[j][BX3];
                 emf->exj_dff[k][j][i] = - sweep->dff_flux[j][BX3];)
      emf->SyL[k][j][i]  = sweep->SL[j]; 
      emf->SyR[k][j][i]  = sweep->SR[j];
      emf->dyL[k][j][i]  = sweep->dL[j]; 
      emf->dyR[k][j][i]  = sweep->dR[j];
      emf->ayL[k][j][i]  = sweep->aL[j]; 
      emf->ayR[k][j][i]  = sweep->aR[j];
      #endif  /* (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == UCT_GFORCE) */

      #if (CT_EMF_AVERAGE == CT_MAXWELL)
      DIM_EXPAND(                                             ;  ,
                 emf->ezj[k][j][i] =   sweep->pnt_flux[j][BX1];  ,
                 emf->exj[k][j][i] = - sweep->pnt_flux[j][BX3];)
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      DIM_EXPAND(                                             ;  ,
                 emf->Bzj[k][j][i] = - sweep->pnt_flux[j][EX1];  ,
                 emf->Bxj[k][j][i] =   sweep->pnt_flux[j][EX3];)
      #endif
      #endif

      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      emf->Frho_j[k][j][i] = sweep->flux[j][RHO];
      #endif
    }

  }else if (g_dir == KDIR){

    emf->kbeg = beg; emf->kend = end;
    i = g_i;
    j = g_j;
    for (k = beg; k <= end; k++) {
      #if    (CT_EMF_AVERAGE == ARITHMETIC)  \
          || (CT_EMF_AVERAGE == CT_CONTACT) \
          || (CT_EMF_AVERAGE == UCT0)
      emf->eyk[k][j][i] = -sweep->flux[k][BX1]; 
      emf->exk[k][j][i] =  sweep->flux[k][BX2]; 
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      emf->Byk[k][j][i] =  sweep->flux[k][EX1]; 
      emf->Bxk[k][j][i] = -sweep->flux[k][EX2]; 
      #endif
      #if CT_EMF_AVERAGE == CT_CONTACT
      if      (sweep->flux[k][RHO] >  eps_CT_CONTACT) s = 1;
      else if (sweep->flux[k][RHO] < -eps_CT_CONTACT) s = -1;
      else s = 0;
      emf->svz[k][j][i] = s;
      #endif
      #endif

      #if CT_EMF_AVERAGE == CT_FLUX
      emf->eyk    [k][j][i] = - sweep->pnt_flux[k][BX1];
      emf->eyk_dff[k][j][i] = - sweep->dff_flux[k][BX1]; 
      emf->exk    [k][j][i] =   sweep->pnt_flux[k][BX2];
      emf->exk_dff[k][j][i] =   sweep->dff_flux[k][BX2]; 
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      emf->Byk[k][j][i] =   sweep->pnt_flux[k][EX1] + 2.0*sweep->dff_flux[k][EX1]; 
      emf->Bxk[k][j][i] = - sweep->pnt_flux[k][EX2] - 2.0*sweep->dff_flux[k][EX2];
      #endif
      #endif  /* CT_EMF_AVERAGE == CT_FLUX */

      #if    (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == UCT_GFORCE) \
          || (CT_EMF_AVERAGE == UCT_HLL)
      emf->eyk    [k][j][i] = - sweep->pnt_flux[k][BX1];
      emf->eyk_dff[k][j][i] = - sweep->dff_flux[k][BX1]; 
      emf->exk    [k][j][i] =   sweep->pnt_flux[k][BX2];
      emf->exk_dff[k][j][i] =   sweep->dff_flux[k][BX2]; 

      emf->SzL[k][j][i]  = sweep->SL[k]; 
      emf->SzR[k][j][i]  = sweep->SR[k]; 
      emf->dzL[k][j][i]  = sweep->dL[k]; 
      emf->dzR[k][j][i]  = sweep->dR[k];
      emf->azL[k][j][i]  = sweep->aL[k]; 
      emf->azR[k][j][i]  = sweep->aR[k];
      #endif  /* (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == UCT_GFORCE) */

      #if (CT_EMF_AVERAGE == CT_MAXWELL)
      emf->eyk[k][j][i] = - sweep->pnt_flux[k][BX1]; 
      emf->exk[k][j][i] =   sweep->pnt_flux[k][BX2]; 
      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      emf->Byk[k][j][i] =   sweep->pnt_flux[k][EX1]; 
      emf->Bxk[k][j][i] = - sweep->pnt_flux[k][EX2];
      #endif
      #endif

      #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
      emf->Frho_k[k][j][i] = sweep->flux[k][RHO];
      #endif

    }
  }

#if (CT_EMF_AVERAGE == UCT_HLL) 
  #ifdef CTU 
  printLog ("! UCT_HLL average not compatible with CTU schemes (stencil too small)\n");
  QUIT_PLUTO(1);
  #endif
#endif

}

/* ********************************************************************* */
void CT_ComputeEMF (const Data *data, Grid *grid)
/*!
 *  Compute the total electromotive force (EMF) using the 
 *  hyperbolic fluxes stored during the most recent upwind step.
 * 
 * \param [in] d
 * \param [in] grid
 *
 *********************************************************************** */
{
  int  i, j, k;
  EMF  *emf = data->emf;
  
/* ----------------------------------------------
   0. Check if cell-centered emf is needed.
      This may depend on the scheme.
   ---------------------------------------------- */
  
#if    CT_EMF_AVERAGE == CT_CONTACT \
    || CT_EMF_AVERAGE == UCT0 \
    || ((PARTICLES == PARTICLES_CR) && (PHYSICS != ResRMHD))
  CT_ComputeCenterEMF (data);
#endif
  
/* ----------------------------------------------
   1. Select averaging scheme
   ---------------------------------------------- */

#if CT_EMF_AVERAGE == ARITHMETIC

  CT_EMF_ArithmeticAverage (emf, 0.25);

#elif CT_EMF_AVERAGE == CT_CONTACT

  #if (PHYSICS == ResRMHD)
    #error CT_CONTACT not defined for ResRMHD
  #endif

  CT_EMF_ArithmeticAverage (emf, 1.0);
  CT_EMF_IntegrateToCorner (data, emf, grid);
  for (k = emf->kbeg; k <= emf->kend; k++){
  for (j = emf->jbeg; j <= emf->jend; j++){
  for (i = emf->ibeg; i <= emf->iend; i++){      
    #if INCLUDE_JDIR && INCLUDE_KDIR
    emf->Ex1e[k][j][i] *= 0.25;
    #endif
    #if INCLUDE_IDIR && INCLUDE_KDIR
    emf->Ex2e[k][j][i] *= 0.25;
    #endif
    #if INCLUDE_IDIR && INCLUDE_JDIR
    emf->Ex3e[k][j][i] *= 0.25;
    #endif
  }}}

#elif    (CT_EMF_AVERAGE == UCT_HLLD) || (CT_EMF_AVERAGE == UCT_GFORCE) \
      || (CT_EMF_AVERAGE == UCT_HLL)

  #ifdef CTU
    #error ! CT_EMF_AVERAGE average not compatible with CTU schemes (stencil too small)
  #endif

  CT_EMF_Riemann2D(data, emf, grid);

#elif CT_EMF_AVERAGE == CT_FLUX

  #ifdef CTU
    #error ! CT_FLUX average not compatible with CTU schemes (stencil too small)
  #endif
  CT_EMF_Flux(data, emf, grid);

#elif CT_EMF_AVERAGE == CT_MAXWELL

  CT_MaxwellSolver (data, emf, grid);

#elif CT_EMF_AVERAGE == UCT0

  #if (PHYSICS == ResRMHD)
    #error UCT0 not defined for ResRMHD
  #endif

  for (k = emf->kbeg; k <= emf->kend + INCLUDE_KDIR; k++){
  for (j = emf->jbeg; j <= emf->jend + INCLUDE_JDIR; j++){
  for (i = emf->ibeg; i <= emf->iend + INCLUDE_IDIR; i++){       
    #if INCLUDE_JDIR && INCLUDE_KDIR
    emf->exj[k][j][i] *= 2.0;
    emf->exk[k][j][i] *= 2.0;
    emf->exj[k][j][i] -= 0.5*(data->Ex1[k][j][i] + data->Ex1[k][j+1][i]);
    emf->exk[k][j][i] -= 0.5*(data->Ex1[k][j][i] + data->Ex1[k+1][j][i]);
    #endif

    #if INCLUDE_IDIR && INCLUDE_KDIR
    emf->eyi[k][j][i] *= 2.0;
    emf->eyk[k][j][i] *= 2.0;
    emf->eyi[k][j][i] -= 0.5*(data->Ex2[k][j][i] + data->Ex2[k][j][i+1]);
    emf->eyk[k][j][i] -= 0.5*(data->Ex2[k][j][i] + data->Ex2[k+1][j][i]);
    #endif

    #if INCLUDE_IDIR && INCLUDE_JDIR
    emf->ezi[k][j][i] *= 2.0;
    emf->ezj[k][j][i] *= 2.0;
    emf->ezi[k][j][i] -= 0.5*(data->Ex3[k][j][i] + data->Ex3[k][j][i+1]);
    emf->ezj[k][j][i] -= 0.5*(data->Ex3[k][j][i] + data->Ex3[k][j+1][i]);
    #endif
  }}}

  CT_EMF_ArithmeticAverage (emf, 0.25);

#else
  printLog ("! CT_ComputeEMF: unknown EMF average.\n");
  QUIT_PLUTO(1);
#endif

/* --------------------------------------------------------
   In cylindrical coordinates we force E_\phi to be exactly
   0 or a small Br components will appear on the axis
   causing values in the ghost zones to violate the
   antisymmetric condition (to machine accuracy).
   -------------------------------------------------------- */

#if (GEOMETRY == CYLINDRICAL) || (GEOMETRY == POLAR)
if (grid->lbound[IDIR] == AXISYMMETRIC){
  #if GEOMETRY == CYLINDRICAL
  k = 0;
  i = IBEG-1;
  for (j = 0; j < NX2_TOT-1; j++) emf->Ex3e[k][j][i] = 0.0;
  #else
  i = IBEG-1;
  j = 0;
  for (k = 0; k < NX3_TOT-1; k++) emf->Ex2e[k][j][i] = 0.0;
  #endif
}
#endif

/* ----------------------------------------------
   2. Deconvolution of electric field (only for 3D)
   ---------------------------------------------- */

  #if (defined HIGH_ORDER) && (DIMENSIONS == 3)
    CT_DeconvolveEMF (data, emf, grid);
  #endif
}

#if (PHYSICS == MHD) && (RESISTIVITY != NO)
/* ********************************************************************* */
void CT_ResistiveEMF (const Data *data, int op, Grid *grid)
/*!
 * Compute/Add resistive terms to EMF (used during parabolic update
 * or STS operator splitting)
 * 
 * \param [in] d
 * \param [in] op     operation: op = 0 means initialize,
 *                               op = 1 means add.                                
 * \param [in] grid
 *
 * \return a pointer to an edge-centered EMF.
 *********************************************************************** */
{
  int    i, j, k;
  double ***vx, ***vy, ***vz;
  double ***Bx, ***By, ***Bz;
  Data_Arr eta;
  EMF  *emf = data->emf; 

  eta = GetStaggeredEta();

  if (op == 0){
    for (k = emf->kbeg; k <= emf->kend; k++){
    for (j = emf->jbeg; j <= emf->jend; j++){
    for (i = emf->ibeg; i <= emf->iend; i++){
      #if INCLUDE_JDIR && INCLUDE_KDIR
      emf->Ex1e[k][j][i] = eta[IDIR][k][j][i]*data->J[IDIR][k][j][i];
      #endif 
      #if INCLUDE_IDIR && INCLUDE_KDIR
      emf->Ex2e[k][j][i] = eta[JDIR][k][j][i]*data->J[JDIR][k][j][i];
      #endif 
      #if INCLUDE_IDIR && INCLUDE_JDIR
      emf->Ex3e[k][j][i] = eta[KDIR][k][j][i]*data->J[KDIR][k][j][i];
      #endif 
    }}}
  } else if (op == 1){
    for (k = emf->kbeg; k <= emf->kend; k++){
    for (j = emf->jbeg; j <= emf->jend; j++){
    for (i = emf->ibeg; i <= emf->iend; i++){
      #if INCLUDE_JDIR && INCLUDE_KDIR
      emf->Ex1e[k][j][i] += eta[IDIR][k][j][i]*data->J[IDIR][k][j][i];
      #endif 
      #if INCLUDE_IDIR && INCLUDE_KDIR
      emf->Ex2e[k][j][i] += eta[JDIR][k][j][i]*data->J[JDIR][k][j][i];
      #endif 
      #if INCLUDE_IDIR && INCLUDE_JDIR
      emf->Ex3e[k][j][i] += eta[KDIR][k][j][i]*data->J[KDIR][k][j][i];
      #endif 
    }}}
   
  }else{
    printLog ("! CT_ResistiveEFM(): invalid op\n");
    QUIT_PLUTO(1); 
  }
}  
#endif /* RESISTIVITY != NO */

#undef eps_CT_CONTACT

#ifdef HIGH_ORDER
/* ********************************************************************* */
void CT_DeconvolveEMF (const Data *data, const EMF *emf, Grid *grid)
/*!
 *  Deconvolve the edge-centered electric field into 
 *  edge-averaged electric field
 * 
 * \param [in] d
 * \param [in] emf
 * \param [in] grid
 *
 *********************************************************************** */
{
  int  i, j, k;
  static double *d2e, *d2b;

/* --------------------------------------------------------
   0. Allocate memory for edge-centered auxiliary 
      electric fields
   -------------------------------------------------------- */ 

  if (d2e == NULL) {
    d2e = ARRAY_1D(NMAX_POINT, double);
    d2b = ARRAY_1D(NMAX_POINT, double);
  }

/* --------------------------------------------------------
   2. Deconvolution of the edge-centered electric field:
      Edge-centered punct. val -> line-avg integrated val
   -------------------------------------------------------- */ 

/* -- Deconvolution of Ex1s along x -- */

  for (k = KBEG-1; k <= KEND; k++){
  for (j = JBEG-1; j <= JEND; j++){
    for (i = IBEG; i <= IEND; i++) d2e[i] = HO_LAPLX(emf->Ex1e,k,j,i);
    for (i = IBEG; i <= IEND; i++) emf->Ex1e[k][j][i] += d2e[i]/24.0;
    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    for (i = IBEG; i <= IEND; i++) d2b[i] = HO_LAPLX(emf->Bx1e,k,j,i);
    for (i = IBEG; i <= IEND; i++) emf->Bx1e[k][j][i] += d2b[i]/24.0;
    #endif
  }} /* end loop on i,j,k */

/* -- Deconvolution of Ex2s along y -- */

  for (k = KBEG-1; k <= KEND; k++){
  for (i = IBEG-1; i <= IEND; i++){ 
    for (j = JBEG; j <= JEND; j++) d2e[j] = HO_LAPLY(emf->Ex2e,k,j,i);
    for (j = JBEG; j <= JEND; j++) emf->Ex2e[k][j][i] += d2e[j]/24.0;
    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    for (j = JBEG; j <= JEND; j++) d2b[j] = HO_LAPLY(emf->Bx2e,k,j,i);
    for (j = JBEG; j <= JEND; j++) emf->Bx2e[k][j][i] += d2b[j]/24.0;
    #endif
  }} /* end loop on j,i,k */

/* -- Deconvolution of Ex3s along z -- */

  for (i = IBEG-1; i <= IEND; i++){
  for (j = JBEG-1; j <= JEND; j++){
    for (k = KBEG; k <= KEND; k++) d2e[k] = HO_LAPLZ(emf->Ex3e,k,j,i);
    for (k = KBEG; k <= KEND; k++) emf->Ex3e[k][j][i] += d2e[k]/24.0;
    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    for (k = KBEG; k <= KEND; k++) d2b[k] = HO_LAPLZ(emf->Bx3e,k,j,i);
    for (k = KBEG; k <= KEND; k++) emf->Bx3e[k][j][i] += d2b[k]/24.0;
    #endif
  }} /* end loop on k,i,j */

}
#endif /* HIGH_ORDER */

/* ********************************************************************* */
void CT_ComputeCenterEMF(const Data *data)
/*!
 *  Compute cell-center inductive electric field.
 *********************************************************************** */
{
  int i,j,k;
  double vx1, vx2, vx3;
  double Bx1, Bx2, Bx3;
  double qg, scrh;

  vx1 = vx2 = vx3 = 0.0;
  Bx1 = Bx2 = Bx3 = 0.0;

  TOT_LOOP(k,j,i){
    vx1 = data->Vc[VX1][k][j][i];
    vx2 = data->Vc[VX2][k][j][i];
    vx3 = data->Vc[VX3][k][j][i];

    Bx1 = data->Vc[BX1][k][j][i];
    Bx2 = data->Vc[BX2][k][j][i];
    Bx3 = data->Vc[BX3][k][j][i];

  /* -- Compute inductive electric field -- */

    data->Ex1[k][j][i] = (vx3*Bx2 - vx2*Bx3);
    data->Ex2[k][j][i] = (vx1*Bx3 - vx3*Bx1);
    data->Ex3[k][j][i] = (vx2*Bx1 - vx1*Bx2);

  /* -- Add CR Hall term  -- */

    #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_FEEDBACK == YES)
    qg   = data->Vc[RHO][k][j][i]*PARTICLES_CR_E_MC_GAS;
    scrh = 1.0/qg;
    data->Ex1[k][j][i] -= data->Fcr[IDIR][k][j][i]*scrh;
    data->Ex2[k][j][i] -= data->Fcr[JDIR][k][j][i]*scrh;
    data->Ex3[k][j][i] -= data->Fcr[KDIR][k][j][i]*scrh;
    #endif
  }
}
