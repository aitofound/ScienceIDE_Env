/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Update staggered magnetic field.

  Update face-centered magnetic field in the constrained transport 
  formulation using a discrete version of Stoke's theorem.
  The update consists of a single Euler step:
  \f[
    \mathtt{Bs} \Longleftarrow \mathtt{Bs} + \Delta t R
  \f]
  where \c d->Vs is the main staggered array used by PLUTO, 
  \c Bs is the magnetic field to be updated and \c R is the 
  right hand side already computed during the unsplit integrator.
  \c d->Vs and \c Bs may be the same array or may be different.
  
  \b References
   - "A staggered mesh algorithm using high-order Godunov fluxes to 
      ensure solenoidal magnetic field in MHD simulations"\n
      Balsara \& Spicer, JCP (1999) 149, 270
  
  \author A. Mignone (andrea.mignone@unito.it)
          V. Berta   (vittoria.berta@unito.it)

  \date   Sep 17, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void CT_Update(const Data *d, Data_Arr Rs, double dt, Grid *grid)
/*!
 * Update staggered magnetic field using discrete version of 
 * Stoke's theorem.
 * Only \c d->Vs is updated, while \c Bs is the original array:\n
 * \c d->Vs = \c Bs + \c dt * \c R, where R = curl(E) is the electric field.
 *
 * \param [in,out] d     pointer to PLUTO Data structure. 
 * \param [in]     Rs    array of staggered variables to be updated.
 * \param [in]     dt    step size
 * \param [in]     grid  pointer to Grid structure
 *
 *********************************************************************** */
{
  int  i, j, k, nv;
  int  ibeg, jbeg, kbeg;
  int  iend, jend, kend;
  double curlE, curlB;

  double *dx1 = grid->dx[IDIR], *x1p = grid->xr[IDIR], *x1m = grid->xl[IDIR];
  double *dx2 = grid->dx[JDIR], *x2p = grid->xr[JDIR], *x2m = grid->xl[JDIR];
  double *dx3 = grid->dx[KDIR], *x3p = grid->xr[KDIR], *x3m = grid->xl[KDIR];

  double *x1 = grid->x[IDIR];
  double *x2 = grid->x[JDIR];
  double *x3 = grid->x[KDIR];

  double Ax1, Ax2, Ax3;
  double dV1, dV2 , dV3;

  EMF *emf = d->emf;
  double ***Ex1e = emf->Ex1e;  /* X1-comp. of emf at cell edges (i,j+1/2,k+1/2) */
  double ***Ex2e = emf->Ex2e;  /* X2-comp. of emf at cell edges (i+1/2,j,k+1/2) */
  double ***Ex3e = emf->Ex3e;  /* X3-comp. of emf at cell edges (i+1/2,j+1/2,k) */

  #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  double ***Bx1e = emf->Bx1e;  /* X1-comp. of B at cell edges (i,j+1/2,k+1/2) */
  double ***Bx2e = emf->Bx2e;  /* X2-comp. of B at cell edges (i+1/2,j,k+1/2) */
  double ***Bx3e = emf->Bx3e;  /* X3-comp. of B at cell edges (i+1/2,j+1/2,k) */
  #endif

/* ---- check div.B ---- */

#if CHECK_DIVB_CONDITION == YES
  if (g_intStage == 1) {
    CT_CheckDivB (d->Vs, grid);
  }    
#endif
  
#if UPDATE_VECTOR_POTENTIAL == YES
  VectorPotentialUpdate (d, emf, NULL, grid);
#endif

/* --------------------------------------------------------
   1a. Update Bx1, Ex1 at (i+1/2, j, k) faces
   -------------------------------------------------------- */

  #ifdef HIGH_ORDER 
  ibeg = IBEG-1; iend = IEND;
  jbeg = JBEG;   jend = JEND;
  kbeg = KBEG;   kend = KEND;
  #else 
  ibeg = emf->ibeg;                iend = emf->iend;
  jbeg = emf->jbeg + INCLUDE_JDIR; jend = emf->jend;
  kbeg = emf->kbeg + INCLUDE_KDIR; kend = emf->kend;
  #endif

  for (k = kbeg; k <= kend; k++){
  for (j = jbeg; j <= jend; j++){
  for (i = ibeg; i <= iend; i++){
    
    Ax1 = grid->A[IDIR][k][j][i];

    #if GEOMETRY == CARTESIAN

    curlE = DIM_EXPAND(  0.0                                            , 
                       + dt/dx2[j]*(Ex3e[k][j][i] - Ex3e[k][j-1][i])    ,
                       - dt/dx3[k]*(Ex2e[k][j][i] - Ex2e[k-1][j][i]) ); 

    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    curlB = DIM_EXPAND(  0.0                                            , 
                       + dt/dx2[j]*(Bx3e[k][j][i] - Bx3e[k][j-1][i])    ,
                       - dt/dx3[k]*(Bx2e[k][j][i] - Bx2e[k-1][j][i]) );
    #endif

    #elif GEOMETRY == CYLINDRICAL
    /* ------------------------------------------------
        Note that Ex3 = -E_\phi since cylindrical
        coordinates (R,z) are not right-handed
       ------------------------------------------------- */

    curlE = + dt/dx2[j]*(Ex3e[k][j][i] - Ex3e[k][j-1][i]);

    #elif GEOMETRY == POLAR 
    Ax1 = fabs(x1p[i]);

    curlE = DIM_EXPAND(   0.0                                               ,
                       + dt/(Ax1*dx2[j])*(Ex3e[k][j][i] - Ex3e[k][j-1][i])  , 
                       - dt/dx3[k]      *(Ex2e[k][j][i] - Ex2e[k-1][j][i])); 

    #elif GEOMETRY == SPHERICAL 

    double Ax2p = fabs(sin(x2p[j]));
    double Ax2m = fabs(sin(x2m[j]));

    dV2 = fabs(cos(x2m[j]) - cos(x2p[j]));
    curlE = DIM_EXPAND( 
            0.0                                                           ,
          + dt/(x1p[i]*dV2)*(Ax2p*Ex3e[k][j][i] - Ax2m*Ex3e[k][j-1][i])   ,
          - dt*dx2[j]/(x1p[i]*dV2*dx3[k])*(Ex2e[k][j][i] - Ex2e[k-1][j][i]));

    #endif    
      
    #ifdef HIGH_ORDER 
    CT_ASSIGN(Rs[BX1s][k][j][i]  = -curlE;  ,
              Rs[EX1s][k][j][i]  =  curlB;)
    #else
    CT_ASSIGN(Rs[BX1s][k][j][i] -= curlE;  ,
              Rs[EX1s][k][j][i] += curlB;)   
    #endif

  }}}

/* --------------------------------------------------------
   1b. Update Bx2, Ex2 at (i, j+1/2, k) faces
   -------------------------------------------------------- */

#if INCLUDE_JDIR

  #ifdef HIGH_ORDER 
  ibeg = IBEG;   iend = IEND;
  jbeg = JBEG-1; jend = JEND;
  kbeg = KBEG;   kend = KEND;
  #else 
  ibeg = emf->ibeg + INCLUDE_IDIR; iend = emf->iend;
  jbeg = emf->jbeg;                jend = emf->jend;
  kbeg = emf->kbeg + INCLUDE_KDIR; kend = emf->kend;
  #endif

  for (k = kbeg; k <= kend; k++){
  for (j = jbeg; j <= jend; j++){
  for (i = ibeg; i <= iend; i++){

  /*  Ax2 = grid->A[JDIR][k][j][i];  */
     
    #if GEOMETRY == CARTESIAN

    curlE = DIM_EXPAND(- dt/dx1[i]*(Ex3e[k][j][i] - Ex3e[k][j][i-1])   ,
                                                                       ,   
                       + dt/dx3[k]*(Ex1e[k][j][i] - Ex1e[k-1][j][i]) ); 

    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    curlB = DIM_EXPAND(- dt/dx1[i]*(Bx3e[k][j][i] - Bx3e[k][j][i-1])   ,
                                                                       ,   
                       + dt/dx3[k]*(Bx1e[k][j][i] - Bx1e[k-1][j][i]) ); 
    #endif

    #elif GEOMETRY == CYLINDRICAL
    /* ------------------------------------------------
        Note that Ex3 = -E_\phi since cylindrical
        coordinates (R,z) are not right-handed
       ------------------------------------------------- */

    double scrh = dt/(fabs(x1[i]*dx1[i]));
    curlE = -scrh*(fabs(x1p[i])*Ex3e[k][j][i] - fabs(x1m[i])*Ex3e[k][j][i-1]);

    #elif GEOMETRY == POLAR 

    curlE =  DIM_EXPAND(- dt/dx1[i]*(Ex3e[k][j][i] - Ex3e[k][j][i-1])   , 
                                                                        ,
                        + dt/dx3[k]*(Ex1e[k][j][i] - Ex1e[k-1][j][i]));
 
    #elif GEOMETRY == SPHERICAL 

    Ax2 = fabs(sin(x2p[j]));
    curlE = DIM_EXPAND( 
            - dt/(x1[i]*dx1[i])*(x1p[i]*Ex3e[k][j][i] - x1p[i-1]*Ex3e[k][j][i-1])    ,
                                                                                     ,
            + dt/(x1[i]*Ax2*dx3[k])*(Ex1e[k][j][i] - Ex1e[k-1][j][i]));

    #endif    
      
    #ifdef HIGH_ORDER
    CT_ASSIGN(Rs[BX2s][k][j][i] = -curlE;  ,
              Rs[EX2s][k][j][i] = +curlB;)
    #else
    CT_ASSIGN(Rs[BX2s][k][j][i] -= curlE;  ,
              Rs[EX2s][k][j][i] += curlB;)
    #endif

  }}}
#endif /* INCLUDE_JDIR */

/* --------------------------------------------------------
   1c. Update Bx3 at (i, j, k+1/2) faces
   -------------------------------------------------------- */

#if INCLUDE_KDIR

  #ifdef HIGH_ORDER 
  ibeg = IBEG;   iend = IEND;
  jbeg = JBEG;   jend = JEND;
  kbeg = KBEG-1; kend = KEND;
  #else 
  ibeg = emf->ibeg + INCLUDE_IDIR; iend = emf->iend;
  jbeg = emf->jbeg + INCLUDE_JDIR; jend = emf->jend;
  kbeg = emf->kbeg;                kend = emf->kend;
  #endif

  for (k = kbeg; k <= kend; k++){
  for (j = jbeg; j <= jend; j++){
  for (i = ibeg; i <= iend; i++){
     
    Ax3 = grid->A[KDIR][k][j][i];
    #if GEOMETRY == CARTESIAN

    curlE =  DIM_EXPAND( dt/dx1[i]*(Ex2e[k][j][i] - Ex2e[k][j][i-1])  ,
                       - dt/dx2[j]*(Ex1e[k][j][i] - Ex1e[k][j-1][i])  ,
                       );

    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    curlB =   dt/dx1[i]*(Bx2e[k][j][i] - Bx2e[k][j][i-1])
            - dt/dx2[j]*(Bx1e[k][j][i] - Bx1e[k][j-1][i]);
    #endif

    #elif GEOMETRY == POLAR 
    Ax1 = fabs(x1p[i]);
    dV1 = fabs(x1[i])*dx1[i];
    curlE =  DIM_EXPAND(
              dt/dV1*(fabs(x1p[i])*Ex2e[k][j][i] - fabs(x1m[i])*Ex2e[k][j][i-1])  ,
            - dt/(x1[i]*dx2[j])*(Ex1e[k][j][i] - Ex1e[k][j-1][i]);                ,
             );
            
    #elif GEOMETRY == SPHERICAL 
    curlE = DIM_EXPAND(
              dt/(x1[i]*dx1[i])*(x1p[i]*Ex2e[k][j][i] - x1p[i-1]*Ex2e[k][j][i-1])  ,
            - dt/(x1[i]*dx2[j])*(Ex1e[k][j][i] - Ex1e[k][j-1][i])                  ,
            );
    #endif
      
    #ifdef HIGH_ORDER
    CT_ASSIGN(Rs[BX3s][k][j][i]  = -curlE;  ,
              Rs[EX3s][k][j][i]  = +curlB;)
    #else
    CT_ASSIGN(Rs[BX3s][k][j][i] -= curlE;  ,
              Rs[EX3s][k][j][i] += curlB;)
    #endif

  }}}
#endif /* INCLUDE_KDIR */

/* --------------------------------------------------------
   2. Add q*v source term to Ampere's law (only with ResRMHD)
   -------------------------------------------------------- */

#if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
  int     rec = RECONSTRUCTION;
  double  qv;
  double ***Frho;
  DIM_EXPAND(double ***Ex1s = d->Vs[EX1s];  ,
             double ***Ex2s = d->Vs[EX2s];  ,
             double ***Ex3s = d->Vs[EX3s];)
  static uint16_t *flag;
  static double *q_DL, *q_DR;
  static double ***q_D;
  
  if (q_DL == NULL){
    q_D  = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
    q_DL = ARRAY_1D(NMAX_POINT, double);
    q_DR = ARRAY_1D(NMAX_POINT, double);
    flag = ARRAY_1D(NMAX_POINT, uint16_t);
  }
  
  TOT_LOOP(k,j,i){
    double vx = d->Vc[VX1][k][j][i];
    double vy = d->Vc[VX2][k][j][i];
    double vz = d->Vc[VX3][k][j][i];
    double gamma = 1.0/sqrt(1.0 - vx*vx - vy*vy - vz*vz);
    double D  = d->Vc[RHO][k][j][i]*gamma;
    q_D[k][j][i] = d->q[k][j][i]/D;
  }

/* ----------------------------------------------
   2b. Update x1-face staggered Electric field 
   ---------------------------------------------- */

  Frho = d->emf->Frho_i;
  #if DIMENSIONS >= 1
  for (k = KBEG;   k <= KEND; k++){
  for (j = JBEG;   j <= JEND; j++){
    #if SHOCK_FLATTENING == MULTID
    for (i = IBEG-1; i <= IEND+1; i++) flag[i] = d->flag[k][j][i];
    #endif
    ArrayReconstruct(q_D, flag, i, j, k, IDIR, q_DL, q_DR, rec, grid);
    for (i = IBEG-1; i <= IEND; i++){
      if (Frho[k][j][i] > 0.0)  qv = q_DL[i]*Frho[k][j][i];
      else                      qv = q_DR[i]*Frho[k][j][i];
      
      Rs[EX1s][k][j][i] -= dt*qv;
    }
  }}
  #endif

/* ----------------------------------------------
   2c. Update x2-face staggered Electric field 
   ---------------------------------------------- */

  Frho = d->emf->Frho_j;
  #if DIMENSIONS >= 2
  for (k = KBEG; k <= KEND; k++){
  for (i = IBEG; i <= IEND; i++){
    #if SHOCK_FLATTENING == MULTID
    for (j = JBEG-1; j <= JEND+1; j++) flag[j] = d->flag[k][j][i];
    #endif
    ArrayReconstruct(q_D, flag, i, j, k, JDIR, q_DL, q_DR, rec, grid);
    for (j = JBEG-1; j <= JEND; j++){

      if (Frho[k][j][i] > 0.0)  qv = q_DL[j]*Frho[k][j][i];
      else                      qv = q_DR[j]*Frho[k][j][i];

      Rs[EX2s][k][j][i] -= dt*qv;
    }
  }}
  #endif
  
/* ----------------------------------------------
   2d. Update x3-face staggered Electric field 
   ---------------------------------------------- */

  Frho = d->emf->Frho_k;
  #if DIMENSIONS == 3
  for (j = JBEG; j <= JEND; j++){
  for (i = IBEG; i <= IEND; i++){
    #if SHOCK_FLATTENING == MULTID
    for (k = KBEG-1; k <= KEND+1; k++) flag[k] = d->flag[k][j][i];
    #endif
    ArrayReconstruct(q_D, flag, i, j, k, KDIR, q_DL, q_DR, rec, grid);
    for (k = KBEG-1; k <= KEND; k++){
      
      if (Frho[k][j][i] > 0.0)  qv = q_DL[k]*Frho[k][j][i];
      else                      qv = q_DR[k]*Frho[k][j][i];

      Rs[EX3s][k][j][i] -= dt*qv;
    }
  }}
  #endif

#endif
}

/* ********************************************************************* */
void CT_CheckDivB (double ***bf[], Grid *grid)
/*!
 * Check the divergence-free condition of magnetic field in the 
 * constrained transport formalism.
 * The solenoidal condition is discretized in a finite-volume sense:
 * \f[
 *      \int \nabla\cdot\vec{B}\, d^3x
 *    = \int \vec{B}\cdot d\hvec{S}
 *    = \sum_d \Big(\bar{B}_{d,+}S_{d,+} - \bar{B}_{d,-}S_{d,-} \Big)
 * \f]
 * where \f$S_{d,\pm}\f$ denotes the right (+) or left (-) interface surface
 * areas orthogonal to the \c d direction.
 * Thus in Cartesian coordinates one has
 * \f[
 *      \int \nabla\cdot\vec{B}\, d^3x
 *    =  \Delta y\Delta z \left(B_{x,+} - B_{x,-}\right)
 *     + \Delta x\Delta z \left(B_{y,+} - B_{y,-}\right)
 *     + \Delta x\Delta y \left(B_{z,+} - B_{z,-}\right)
 * \f]
 * while in spherical coordinates (\f$ dS_1 = r^2\,\sin\theta\,d\theta\,d\phi,
 * \, dS_2 = r\sin\theta\, dr\,d\phi,\, dS_3 = r\,dr\,d\theta\f$) we have
 * \f[
 *      \int \nabla\cdot\vec{B}\, d^3x
 *    =  \Delta \mu\Delta\phi
 *         \Big(r^2_+B_{r,+} - r^2_-B_{r,-}\Big)
 *     + \Delta\left(\frac{r^2}{2}\right) \Delta\phi
 *         \Big(  \sin\theta_+B_{\theta,+} - \sin\theta_-B_{\theta,-}\Big)
 *     + \Delta\left(\frac{r^2}{2}\right)\Delta\theta
 *         \Big(B_{\phi,+} - B_{\phi,-}\Big)
 * \f]
 * where \f$\mu = 1-\cos\theta\f$.
 * Notice also that \f$\Delta (r^2/2) = r\Delta r\f$ where \c r is the cell
 * center.
 * 
 * \param [in]  bf    an array of staggered magnetic field components
 * \param [in]  grid  a pointer to Grid structure
 *********************************************************************** */
{
  int i,j,k;
  double divB;
  DIM_EXPAND(double ***Bx1 = bf[0];  ,
             double ***Bx2 = bf[1];  ,
             double ***Bx3 = bf[2];)
  double dBx1, dBx2, dBx3, dbmax=0.0;
  double ***Ax1 = grid->A[IDIR];
  double ***Ax2 = grid->A[JDIR];
  double ***Ax3 = grid->A[KDIR];

/* ---------------------------------------
    Loop over computational domain
   --------------------------------------- */

  TOT_LOOP(k,j,i){
    DIM_EXPAND(dBx1 = Ax1[k][j][i]*Bx1[k][j][i] - Ax1[k][j][i-1]*Bx1[k][j][i-1];  ,
               dBx2 = Ax2[k][j][i]*Bx2[k][j][i] - Ax2[k][j-1][i]*Bx2[k][j-1][i];  ,
               dBx3 = Ax3[k][j][i]*Bx3[k][j][i] - Ax3[k-1][j][i]*Bx3[k-1][j][i];)
    divB = DIM_EXPAND(dBx1, + dBx2, + dBx3);

    dbmax = MAX(dbmax,fabs(divB));
    if (fabs(divB) > 1.e-6) {
      printLog ("! CT_CheckDivB: div(B) = %12.6e, rank = %d, ijk = %d %d %d \n",
              divB, prank, i,j,k);
      DIM_EXPAND(printLog ("  Bx1: %12.6e  %12.6e\n",Bx1[k][j][i],Bx1[k][j][i-1]); ,
                 printLog ("  Bx2: %12.6e  %12.6e\n",Bx2[k][j][i],Bx2[k][j-1][i]); ,
                 printLog ("  Bx3: %12.6e  %12.6e\n",Bx3[k][j][i],Bx3[k-1][j][i]); )
      QUIT_PLUTO(1);
    }
  }
}

#if PHYSICS == ResRMHD  /* Function for the explicit schemes (old) */
/* ********************************************************************* */
void CT_InterfaceCurrent(Data *d, Grid *grid)
/*! 
 *   Comute q/D = div(E) / (rho*gamma) and Current at cell faces.
 *   [Only used in the explicit scheme]
 *********************************************************************** */
{
  int    i,j,k;
  double vx, vy, vz, gamma;
  double Ex, Ey, Ez;
  double Bx, By, Bz;
  double Jx, Jy, Jz;
  double Ev, qv, vXB;
  double v[NVAR];
  double eta = Resistive_eta(v, 0.0,0.0,0.0);
  DIM_EXPAND(double ***Bx1s = d->Vs[BX1s];  ,
             double ***Bx2s = d->Vs[BX2s];  ,
             double ***Bx3s = d->Vs[BX3s];)

  DIM_EXPAND(double ***Ex1s = d->Vs[EX1s];  ,
             double ***Ex2s = d->Vs[EX2s];  ,
             double ***Ex3s = d->Vs[EX3s];)

  double ***vx1 = d->Vc[VX1];
  double ***vx2 = d->Vc[VX2];
  double ***vx3 = d->Vc[VX3];
  double ***q   = d->q;
  double dx, dy, dz;

  for (k = 0; k < NX3_TOT; k++){
  for (j = 0; j < NX2_TOT; j++){
  for (i = 0; i < NX1_TOT; i++){
    dx = grid->dx[IDIR][i];
    dy = grid->dx[JDIR][j];
    dz = grid->dx[KDIR][k];

    d->q[k][j][i] = DIM_EXPAND(  (Ex1s[k][j][i] - Ex1s[k][j][i-1])/dx,
                               + (Ex2s[k][j][i] - Ex2s[k][j-1][i])/dy,
                               + (Ex3s[k][j][i] - Ex3s[k-1][j][i])/dz  );
  }}}
  
/* -- 1a. Compute current at (i+1/2, j, k) faces -- */

  for (k = KBEG;   k <= KEND; k++){
  for (j = JBEG;   j <= JEND; j++){
  for (i = IBEG-1; i <= IEND; i++){
    
    vx = 0.5*(vx1[k][j][i] + vx1[k][j][i+1]);
    vy = 0.5*(vx2[k][j][i] + vx2[k][j][i+1]);
    vz = 0.5*(vx3[k][j][i] + vx3[k][j][i+1]);

    Ex = Ex1s[k][j][i];
    Ey = AVERAGE_XY(Ex2s, k, j-1, i);
    By = AVERAGE_XY(Bx2s, k, j-1, i);
    #if DIMENSIONS == 2
    Ez = AVERAGE_X(d->Vc[EX3], k, j, i);
    Bz = AVERAGE_X(d->Vc[BX3], k, j, i);
    #elif DIMENSIONS == 3
    Ez = AVERAGE_XZ(Ex3s, k-1, j, i);
    Bz = AVERAGE_XZ(Bx3s, k-1, j, i);
    #endif

    gamma = 1.0/sqrt(1.0 - vx*vx - vy*vy - vz*vz);
    vXB   = vy*Bz - vz*By;
    Ev    = Ex*vx + Ey*vy + Ez*vz;
    qv    = 0.5*(q[k][j][i] + q[k][j][i+1])*vx;

    d->J[IDIR][k][j][i] = gamma/eta*(Ex + vXB - Ev*vx) + 0*qv;
  }}} 
  
/* -- 1b. Compute current at (i, j+1/2, k) faces -- */

  for (k = KBEG;   k <= KEND; k++){
  for (j = JBEG-1; j <= JEND; j++){
  for (i = IBEG;   i <= IEND; i++){
    
    vx = 0.5*(vx1[k][j][i] + vx1[k][j+1][i]);
    vy = 0.5*(vx2[k][j][i] + vx2[k][j+1][i]);
    vz = 0.5*(vx3[k][j][i] + vx3[k][j+1][i]);

    Ex = AVERAGE_XY(Ex1s, k, j, i-1);
    Bx = AVERAGE_XY(Bx1s, k, j, i-1);

    Ey = Ex2s[k][j][i];
    #if DIMENSIONS == 2 
    Ez = AVERAGE_Y(d->Vc[EX3], k, j, i);
    Bz = AVERAGE_Y(d->Vc[BX3], k, j, i);
    #elif DIMENSIONS == 3
    Ez = AVERAGE_YZ(Ex3s, k-1, j, i);
    Bz = AVERAGE_YZ(Bx3s, k-1, j, i);
    #endif

    gamma = 1.0/sqrt(1.0 - vx*vx - vy*vy - vz*vz);
    vXB   = vz*Bx - vx*Bz;
    Ev    = Ex*vx + Ey*vy + Ez*vz;
    qv    = 0.5*(q[k][j][i] + q[k][j+1][i])*vy;

    d->J[JDIR][k][j][i] = gamma/eta*(Ey + vXB - Ev*vy) + 0*qv;
  }}} 

#if DIMENSIONS == 3
  
/* -- 1c. Compute current at (i, j, k+1/2) faces -- */

  for (k = KBEG-1; k <= KEND; k++){
  for (j = JBEG;   j <= JEND; j++){
  for (i = IBEG;   i <= IEND; i++){
    
    vx = 0.5*(vx1[k][j][i] + vx1[k+1][j][i]);
    vy = 0.5*(vx2[k][j][i] + vx2[k+1][j][i]);
    vz = 0.5*(vx3[k][j][i] + vx3[k+1][j][i]);

    Ex = AVERAGE_XZ(Ex1s, k, j, i-1);
    Bx = AVERAGE_XZ(Bx1s, k, j, i-1);

    Ey = AVERAGE_YZ(Ex3s, k, j-1, i);
    By = AVERAGE_YZ(Bx3s, k, j-1, i);

    Ez = Ex3s[k][j][i];
    Bz = Bx3s[k][j][i];

    gamma = 1.0/sqrt(1.0 - vx*vx - vy*vy - vz*vz);
    vXB   = vx*By - vy*Bx;
    Ev    = Ex*vx + Ey*vy + Ez*vz;
    qv    = 0.5*(q[k][j][i] + q[k+1][j][i])*vz;

    d->J[KDIR][k][j][i] = gamma/eta*(Ez + vXB - Ev*vz) + 0*qv;
  }}} 
#endif
}

/* ********************************************************************* */
void CT_ResistiveUpdate(const Data *d, double dt, Grid *grid)
/*! 
 *   [Only used in the explicit scheme]
 *********************************************************************** */
{
  int i,j,k;
  double qv;
  DIM_EXPAND(double ***Ex1s = d->Vs[EX1s];  ,
           double ***Ex2s = d->Vs[EX2s];  ,
           double ***Ex3s = d->Vs[EX3s];)
  double ***Jx1s = d->J[IDIR];
  double ***Jx2s = d->J[JDIR];
  double ***Jx3s = d->J[KDIR];
  double ***q    = d->q;
  double ***Frho;

/* -- 1a. Explicit update of x1-face electric field -- */

  Frho = d->emf->Frho_i;
  for (k = KBEG;   k <= KEND; k++){
  for (j = JBEG;   j <= JEND; j++){
  for (i = IBEG-1; i <= IEND; i++){
    
    if (Frho[k][j][i] > 0.0) {
      qv =  q[k][j][i]*Frho[k][j][i];
    } else {
      qv =  q[k][j][i+1]*Frho[k][j][i];
    }  

    Ex1s[k][j][i] -= dt*(Jx1s[k][j][i] + qv);
  }}} 
  
/* -- 1b. Explicit update of x2-face electric field -- */

  Frho = d->emf->Frho_j;
  for (k = KBEG;   k <= KEND; k++){
  for (j = JBEG-1; j <= JEND; j++){
  for (i = IBEG;   i <= IEND; i++){
   
    if (Frho[k][j][i] > 0.0) {
      qv =  q[k][j][i]*Frho[k][j][i];
    } else {
      qv =  q[k][j+1][i]*Frho[k][j][i];
    }
    Ex2s[k][j][i] -= dt*(Jx2s[k][j][i] + qv);
  }}} 

#if DIMENSIONS == 3
  printLog ("! CT_ResistiveUpdate() does not work in 3D\n");
  QUIT_PLUTO(1);
#endif
  
}

#endif /* PHYSICS == ResRMHD */
