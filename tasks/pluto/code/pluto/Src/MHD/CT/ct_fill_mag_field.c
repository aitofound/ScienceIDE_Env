/* ///////////////////////////////////////////////////////////////////// */
/*! \file
 * \brief Assign the normal component of the staggered magnetic field
 *        in the ghost zone-faces.
 *  Using the div.B = 0 condition in staggered MHD, this function 
 *  computes the staggered component of magnetic field lying on the 
 *  zone-faces parallel to the boundary specified by "side".
 *  This is preformed by solving the div.B = 0 condition for one 
 *  variable only which in 2-D requires the knowledge of the other 
 *  3 components while in 3-D required the knowledge of the other 5 
 *  staggered components.\n
 * 
 *  Note that this operation is performed in the outermost ghost zones 
 *  only since the face value at IBEG-1 or IEND is supposed to be part 
 *  of the solution and is not changed during this function.
 *  Therefore, only nghost-1 faces are assigned:
 *
 *  \verbatim
 *  +-----+-----+-----+-----+-----+--
 *  |     |     |     |     |     |
 *  |     X     X     |     |     |
 *  |     |     |     |     |     |
 *  +-----+-----+-----+-----+-----+--
 *                    |
 *  <-----------------> BEG
 *   Physical boundary     
 *
 *   X = components assigned in this function.
 * \endverbatim
 *
 * \author A. Mignone (andrea.mignone@unito.it)
 * \date   Apr 25, 2019
 *
   ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************** */
void FillMagneticField (const Data *d, int side, Grid *grid)
/*!
 *
 * \param [in,out]    d  pointer to PLUTO Data structure
 * \param [in]     side  the side
 * \param [in]     grid  pointer to PLUTO Grid structure
 *
 * \todo   replace the loops with more compact macro, such as
 *         X1_BEG_LOOP()...
 *********************************************************************** */
{
  int  ibeg, jbeg, kbeg;
  int  iend, jend, kend;
  int  i,j,k;
  int  di,dj,dk;

  double Bxp, Byp, Bzp;
  double Bxm, Bym, Bzm;
  double dBx = 0.0, dBy = 0.0, dBz = 0.0;
  double Axp, Ayp, Azp;
  double Axm, Aym, Azm;
  double ***Bx, ***By, ***Bz;  /* -- cell centered mag. field -- */

  DIM_EXPAND(Bx = d->Vs[BX1s];  ,
             By = d->Vs[BX2s];  ,
             Bz = d->Vs[BX3s];)

/* ----------------------------------------------------------
   1. Set initial and final index for each side
   ---------------------------------------------------------- */

  ibeg = 0; iend = NX1_TOT-1; di = 1;
  jbeg = 0; jend = NX2_TOT-1; dj = 1;
  kbeg = 0; kend = NX3_TOT-1; dk = 1;

  if (side == X1_BEG) {ibeg = IBEG-1; iend = 0; di = -1;}
  if (side == X1_END)  ibeg = IEND+1;

  if (side == X2_BEG) {jbeg = JBEG-1; jend = 0; dj = -1;}
  if (side == X2_END)  jbeg = JEND+1;

  if (side == X3_BEG) {kbeg = KBEG-1; kend = 0; dk = -1;}
  if (side == X3_END)  kbeg = KEND+1;

/* ----------------------------------------------------------
   2. Loop over cells in the boundary
   ---------------------------------------------------------- */

  for (k = kbeg; dk*k <= dk*kend; k += dk){
  for (j = jbeg; dj*j <= dj*jend; j += dj){
  for (i = ibeg; di*i <= di*iend; i += di){

    DIM_EXPAND(Bxp = Bx[k][j][i]; Bxm = Bx[k][j][i-1];  ,
               Byp = By[k][j][i]; Bym = By[k][j-1][i];  ,
               Bzp = Bz[k][j][i]; Bzm = Bz[k-1][j][i];)

  /* -------------------------------------------------------
       Divergence is written as 

          (Axp*Bxp - Axm*Bxm)  
        + (Ayp*Byp - Aym*Bym)  
        + (Azp*Bzp - Azm*Bzm) = 0

       so that the k-th component can be 
       recovered as
 
      Bkp = Bkm*Akm/Akp + sum_(j != k) (Ajp*Bjp - Ajm*Bjm)/Akp
    ------------------------------------------------------- */

    DIM_EXPAND(Axp = grid->A[IDIR][k][j][i]; Axm = grid->A[IDIR][k][j][i-1];  ,
               Ayp = grid->A[JDIR][k][j][i]; Aym = grid->A[JDIR][k][j-1][i];  ,
               Azp = grid->A[KDIR][k][j][i]; Azm = grid->A[KDIR][k-1][j][i];)

    DIM_EXPAND(dBx = (Axp*Bxp - Axm*Bxm);  ,
               dBy = (Ayp*Byp - Aym*Bym);  ,
               dBz = (Azp*Bzp - Azm*Bzm); )

/* -------------------------------------------------
      Assign a single face magnetic field 
   ------------------------------------------------- */

    if (side == X1_BEG){

      Bx[k][j][i-1] = (Axp*Bxp + dBy + dBz)/Axm;

    }else if (side == X1_END){

      Bx[k][j][i] = (Axm*Bxm - (dBy + dBz))/Axp;

    #if INCLUDE_JDIR
    }else if (side == X2_BEG){

      By[k][j-1][i] = (Ayp*Byp + dBx + dBz)/Aym;

    }else if (side == X2_END){
  
      By[k][j][i] = (Aym*Bym - (dBx + dBz))/Ayp;
    #endif

    #if INCLUDE_KDIR
    }else if (side == X3_BEG){

      Bz[k-1][j][i] = (Azp*Bzp + dBx + dBy)/Azm;

    }else if (side == X3_END){

      Bz[k][j][i] = (Azm*Bzm - (dBx + dBy))/Azp;

    #endif
    }

  }}}

}

#if PHYSICS == ResRMHD 
/* ********************************************************************** */
void FillElectricField (const Data *d, int side, Grid *grid)
/*!
 *
 * \param [in,out]    d  pointer to PLUTO Data structure
 * \param [in]     side  the side
 * \param [in]     grid  pointer to PLUTO Grid structure
 *
 * \todo   replace the loops with more compact macro, such as
 *         X1_BEG_LOOP()...
 *********************************************************************** */
{
  int  ibeg, jbeg, kbeg;
  int  iend, jend, kend;
  int  i,j,k;
  int  di,dj,dk;

  double qdV;
  double Exp, Eyp, Ezp;
  double Exm, Eym, Ezm;
  double dEx = 0.0, dEy = 0.0, dEz = 0.0;
  double *dx1 = grid->dx[IDIR];
  double *dx2 = grid->dx[JDIR];
  double *dx3 = grid->dx[KDIR];
  double Axp, Ayp, Azp;
  double Axm, Aym, Azm;

  double ***Ex, ***Ey, ***Ez;  /* -- staggered magnetic field -- */

  DIM_EXPAND(Ex = d->Vs[EX1s]; ,
             Ey = d->Vs[EX2s]; ,
             Ez = d->Vs[EX3s]; )

/* ----------------------------------------------------------
   1. Set initial and final index for each side
   ---------------------------------------------------------- */

  ibeg = 0; iend = NX1_TOT-1; di = 1;
  jbeg = 0; jend = NX2_TOT-1; dj = 1;
  kbeg = 0; kend = NX3_TOT-1; dk = 1;

  if (side == X1_BEG) {ibeg = IBEG-1; iend = 0; di = -1;}
  if (side == X1_END)  ibeg = IEND+1;

  if (side == X2_BEG) {jbeg = JBEG-1; jend = 0; dj = -1;}
  if (side == X2_END)  jbeg = JEND+1;

  if (side == X3_BEG) {kbeg = KBEG-1; kend = 0; dk = -1;}
  if (side == X3_END)  kbeg = KEND+1;

/* ----------------------------------------------------------
   2. Loop over cells in the boundary
   ---------------------------------------------------------- */

  for (k = kbeg; dk*k <= dk*kend; k += dk){
  for (j = jbeg; dj*j <= dj*jend; j += dj){
  for (i = ibeg; di*i <= di*iend; i += di){

    DIM_EXPAND(Exp = Ex[k][j][i]; Exm = Ex[k][j][i - 1];  ,
               Eyp = Ey[k][j][i]; Eym = Ey[k][j - 1][i];  ,
               Ezp = Ez[k][j][i]; Ezm = Ez[k - 1][j][i];)

  /* -------------------------------------------------------
       Divergence is written as 

          (Axp*Exp - Axm*Exm)  
        + (Ayp*Eyp - Aym*Eym)  
        + (Azp*Ezp - Azm*Ezm) = q*dV

    ------------------------------------------------------- */

    Axp = grid->A[IDIR][k][j][i]; Axm = grid->A[IDIR][k][j][i-1];
    Ayp = grid->A[JDIR][k][j][i]; Aym = grid->A[JDIR][k][j-1][i];
    Azp = grid->A[KDIR][k][j][i]; Azm = grid->A[KDIR][k-1][j][i];

    DIM_EXPAND(dEx = (Axp*Exp - Axm*Exm);  ,
               dEy = (Ayp*Eyp - Aym*Eym);  ,
               dEz = (Azp*Ezp - Azm*Ezm); )

/* -------------------------------------------------
      Assign a single face magnetic field 
   ------------------------------------------------- */

    qdV = d->q[k][j][i]*grid->dV[k][j][i];

    if (side == X1_BEG){

      Ex[k][j][i-1] = (Axp*Exp + dEy + dEz - qdV)/Axm;

    }else if (side == X1_END){

      Ex[k][j][i] = (Axm*Exm - (dEy + dEz) + qdV)/Axp;

    }else if (side == X2_BEG){

      Ey[k][j-1][i] = (Ayp*Eyp + dEx + dEz - qdV)/Aym;

    }else if (side == X2_END){
  
      Ey[k][j][i] = (Aym*Eym - (dEx + dEz) + qdV )/Ayp;

    #if DIMENSIONS == 3
    }else if (side == X3_BEG){

      Ez[k-1][j][i] = (Azp*Ezp + dEx + dEy - qdV)/Azm;

    }else if (side == X3_END){

      Ez[k][j][i] = (Azm*Ezm - (dEx + dEy) + qdV)/Azp;

    #endif
    }

  }}}

}
#endif
