#include "pluto.h"
#include "les.h"

static double ***f;

/* *********************************************************** */
void LES_ViscousFlux (const Data *d, Data_Arr rhs, Grid *grid)
/*
 *
 * PURPOSE:
 *
 *   Compute viscous fluxes and add them to rhs
 *   accrording to the sign of f = N - aux[FMIN]*D,
 *   where N, D > 0.
 *   This is executed only if INCLUDE_LES == YES
 *   (set in les.h)
 *  
 *   
 *  
 *   aux[FMIN] > 0   filter, put viscous terms where
 *                   f > 0.
 *   aux[FMIN] < 0   do not filter, put viscous terms 
 *                   everywhere.
 *
 ************************************************************* */
#define D_DX_I(q)  (q[k][j][i + 1] - q[k][j][i])
#define D_DY_J(q)  (q[k][j + 1][i] - q[k][j][i])
#define D_DZ_K(q)  (q[k + 1][j][i] - q[k][j][i])

#define D_DY_I(q)  (  0.25*(q[k][j + 1][i] + q[k][j + 1][i + 1]) \
                    - 0.25*(q[k][j - 1][i] + q[k][j - 1][i + 1]))

#define D_DZ_I(q)  (  0.25*(q[k + 1][j][i] + q[k + 1][j][i + 1])  \
                    - 0.25*(q[k - 1][j][i] + q[k - 1][j][i + 1]))

#define D_DX_J(q)  (  0.25*(q[k][j][i + 1] + q[k][j + 1][i + 1]) \
                    - 0.25*(q[k][j][i - 1] + q[k][j + 1][i - 1]))

#define D_DZ_J(q)  (  0.25*(q[k + 1][j][i] + q[k + 1][j + 1][i]) \
                    - 0.25*(q[k - 1][j][i] + q[k - 1][j + 1][i]))

#define D_DX_K(q)  (  0.25*(q[k][j][i + 1] + q[k + 1][j][i + 1]) \
                    - 0.25*(q[k][j][i - 1] + q[k + 1][j][i - 1]))

#define D_DY_K(q)  (  0.25*(q[k][j + 1][i] + q[k + 1][j + 1][i]) \
                    - 0.25*(q[k][j - 1][i] + q[k + 1][j - 1][i]))

{
  int  i, j, k, nv;
  double dx, dy, dz;
  double ***vx, ***vy, ***vz, ***rho, ***pr;
  double dvx_dx, dvx_dy, dvx_dz;
  double dvy_dx, dvy_dy, dvy_dz;
  double dvz_dx, dvz_dy, dvz_dz;
  double S, Sxx, Syy, Szz, Sxy, Sxz, Syz;
  double div, nu_t, rhom;
  double wx, wy, wz, qx, qy, qz;
  double fmx, fmy, fmz, fe;
  double dtdx, dtdy, dtdz;
  double nu_max;
  double Cs = 0.1, Csdl2, scrh, dH;
  double Prandtl = 0.71;
  static double **Ux_ave, **Uy_ave, **Uz_ave;
  static double **dUx_ave_dx, **dUx_ave_dz;
  static double **dUy_ave_dx, **dUy_ave_dz;
  static double **dUz_ave_dx, **dUz_ave_dz;
  static double ***H;

  #if INCLUDE_LES == NO
   return;  /* do nothing, just return */
  #endif

  if (f == NULL) {
    f      = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);

    Ux_ave = ARRAY_2D(NX3_TOT, NX1_TOT, double);
    Uy_ave = ARRAY_2D(NX3_TOT, NX1_TOT, double);
    Uz_ave = ARRAY_2D(NX3_TOT, NX1_TOT, double);

    dUx_ave_dx = ARRAY_2D(NX3_TOT, NX1_TOT, double);
    dUx_ave_dz = ARRAY_2D(NX3_TOT, NX1_TOT, double);

    dUy_ave_dx = ARRAY_2D(NX3_TOT, NX1_TOT, double);
    dUy_ave_dz = ARRAY_2D(NX3_TOT, NX1_TOT, double);

    dUz_ave_dx = ARRAY_2D(NX3_TOT, NX1_TOT, double);
    dUz_ave_dz = ARRAY_2D(NX3_TOT, NX1_TOT, double);

    H = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  }

  dx = grid[IDIR].dx[3];
  dy = grid[JDIR].dx[3];
  dz = grid[KDIR].dx[3];

  dtdx = g_dt/dx;
  dtdy = g_dt/dy;
  dtdz = g_dt/dz;

  rho = d->Vc[RHO];
  vx  = d->Vc[VX1];
  vy  = d->Vc[VX2];
  vz  = d->Vc[VX3];
  pr  = d->Vc[PRS];

  Csdl2 = Cs*Cs*pow(dx*dy*dz, 2.0/3.0);

  nu_max = 1.e-12;

/* ------------------------------------------
       compute average velocity along y 
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    !
    ! remember to run PLUTO with the 
    ! -no-x2par switch 
    !
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   ------------------------------------------ */

  for (k = KBEG - 2; k <= KEND + 2; k++){
  for (i = IBEG - 2; i <= IEND + 2; i++){

    Ux_ave[k][i] = Uy_ave[k][i] = Uz_ave[k][i] = 0.0;
    for (j = JBEG; j <= JEND; j++){
      Ux_ave[k][i] += vx[k][j][i];
      Uy_ave[k][i] += vy[k][j][i];
      Uz_ave[k][i] += vz[k][j][i];
    }
    Ux_ave[k][i] /= (double)NX2;
    Uy_ave[k][i] /= (double)NX2;
    Uz_ave[k][i] /= (double)NX2;

  }}

/* ------------------------------------------
    compute average <x> and <z> derivatives 
   ------------------------------------------ */

  for (k = KBEG - 1; k <= KEND + 1; k++){
  for (i = IBEG - 1; i <= IEND + 1; i++){
    dUx_ave_dx[k][i] = 0.5*(Ux_ave[k][i + 1] - Ux_ave[k][i - 1])/dx;
    dUx_ave_dz[k][i] = 0.5*(Ux_ave[k + 1][i] - Ux_ave[k - 1][i])/dz;

    dUy_ave_dx[k][i] = 0.5*(Uy_ave[k][i + 1] - Uy_ave[k][i - 1])/dx;
    dUy_ave_dz[k][i] = 0.5*(Uy_ave[k + 1][i] - Uy_ave[k - 1][i])/dz;

    dUz_ave_dx[k][i] = 0.5*(Uz_ave[k][i + 1] - Uz_ave[k][i - 1])/dx;
    dUz_ave_dz[k][i] = 0.5*(Uz_ave[k + 1][i] - Uz_ave[k - 1][i])/dz;
  }}

/* ----------------------------------------
      compute total averaged 
      integrated vorticity 
   ---------------------------------------- */
/*
  Om2_ave = 0.0;
  for (k = KBEG; k <= KEND; k++){
  for (i = IBEG; i <= IEND; i++){
    wx = dUy_ave_dx[k][i];
    wz = dUy_ave_dz[k][i];
    Om2_ave += wx*wx + wz*wz;
  }}

  #ifdef PARALLEL
   MPI_Barrier(MPI_COMM_WORLD);
   MPI_Allreduce (&Om2_ave, &Om2_ave_glob, 1, MPI_PLUTO_REAL, MPI_SUM, MPI_COMM_WORLD);
   MPI_Barrier(MPI_COMM_WORLD);
  #else 
   Om2_ave_glob = Om2_ave;
  #endif   

  Om2_ave_glob /= (real)(grid[IDIR].np_int_glob*grid[KDIR].np_int_glob);
*/
/* ------------------------------------------
           compute "f" function
   ------------------------------------------ */

  for (k = KBEG - 1; k <= KEND + 1; k++){
  for (j = JBEG - 1; j <= JEND + 1; j++){
  for (i = IBEG - 1; i <= IEND + 1; i++){
    
    dvx_dx = 0.5*(vx[k][j][i + 1] - vx[k][j][i - 1])/dx - dUx_ave_dx[k][i];
    dvy_dx = 0.5*(vy[k][j][i + 1] - vy[k][j][i - 1])/dx - dUy_ave_dx[k][i];
    dvz_dx = 0.5*(vz[k][j][i + 1] - vz[k][j][i - 1])/dx - dUz_ave_dx[k][i];

    dvx_dy = 0.5*(vx[k][j + 1][i] - vx[k][j - 1][i])/dy;
    dvy_dy = 0.5*(vy[k][j + 1][i] - vy[k][j - 1][i])/dy;
    dvz_dy = 0.5*(vz[k][j + 1][i] - vz[k][j - 1][i])/dy;

    dvx_dz = 0.5*(vx[k + 1][j][i] - vx[k - 1][j][i])/dz - dUx_ave_dz[k][i];
    dvy_dz = 0.5*(vy[k + 1][j][i] - vy[k - 1][j][i])/dz - dUy_ave_dz[k][i];
    dvz_dz = 0.5*(vz[k + 1][j][i] - vz[k - 1][j][i])/dz - dUz_ave_dz[k][i];

    wx = dvz_dy - dvy_dz;
    wy = dvx_dz - dvz_dx;
    wz = dvy_dx - dvx_dy;

    qx = wx*dvx_dx + wy*dvx_dy + wz*dvx_dz;
    qy = wx*dvy_dx + wy*dvy_dy + wz*dvy_dz;
    qz = wx*dvz_dx + wy*dvz_dy + wz*dvz_dz;

    f[k][j][i]  = sqrt(qx*qx + qy*qy + qz*qz);
    f[k][j][i] -= (wx*wx + wy*wy + wz*wz)*g_inputParam[FMIN] + 1.e-6;
/*
    if (f[k][j][i] < 1.e-3) f[k][j][i] = -1.0;
    else f[k][j][i] -= (wx*wx + wy*wy + wz*wz)*g_inputParam[FMIN];
*/
/*
printf ("%d %d %d   N = %12.6e, D = %12.6e\n",
     i,j,k, f[k][j][i],(wx*wx + wy*wy + wz*wz) );
*/
           
    for (nv = 0; nv < NVAR; nv++) rhs[nv][k][j][i] = 0.0;
  }}}

/* ----------------------------------------------------
           Compute Specific enthalpy
   ---------------------------------------------------- */

  KTOT_LOOP(k){
  JTOT_LOOP(j){
  ITOT_LOOP(i){
    S =   vx[k][j][i]*vx[k][j][i] 
        + vy[k][j][i]*vy[k][j][i] 
        + vz[k][j][i]*vz[k][j][i];

    H[k][j][i] = pr[k][j][i]*g_gamma/(g_gamma - 1.0)/rho[k][j][i] + 0.5*S;
  }}}


/* ----------------------------------------------------
           Compute Viscous Fluxes 
   ---------------------------------------------------- */

  if (g_dir == IDIR){     

  /* ------------------------
            X  Sweep
     ------------------------ */

    for (k = KBEG    ; k <= KEND; k++){
    for (j = JBEG    ; j <= JEND; j++){
    for (i = IBEG - 1; i <= IEND; i++){

      rhom = 0.5*(rho[k][j][i] + rho[k][j][i + 1]);

      dvx_dx = (D_DX_I(vx))/dx;
      dvy_dx = (D_DX_I(vy))/dx;
      dvz_dx = (D_DX_I(vz))/dx;

      dvx_dy = (D_DY_I(vx))/dy;
      dvy_dy = (D_DY_I(vy))/dy;
      dvz_dy = (D_DY_I(vz))/dy;

      dvx_dz = (D_DZ_I(vx))/dz;
      dvy_dz = (D_DZ_I(vy))/dz;
      dvz_dz = (D_DZ_I(vz))/dz;

      Sxx = 0.5*(dvx_dx + dvx_dx);
      Sxy = 0.5*(dvx_dy + dvy_dx);
      Sxz = 0.5*(dvx_dz + dvz_dx);

      Syy = 0.5*(dvy_dy + dvy_dy);
      Syz = 0.5*(dvy_dz + dvz_dy);
      Szz = 0.5*(dvz_dz + dvz_dz);

      div = dvx_dx + dvy_dy + dvz_dz;

      S =        Sxx*Sxx + Syy*Syy + Szz*Szz 
          + 2.0*(Sxy*Sxy + Sxz*Sxz + Syz*Syz);
      S = sqrt(2.0*S);
      nu_t   = Csdl2*S;
      nu_max = MAX(nu_max, nu_t);

      scrh = dtdx*2.0*rhom*nu_t;
        
      fmx = scrh*(Sxx - div/3.0);
      fmy = scrh*Sxy;
      fmz = scrh*Sxz;

      dH  = dtdx*(H[k][j][i + 1] - H[k][j][i]);
      fe  = rhom*nu_t/Prandtl*dH/dx;

      rhs[MX][k][j][i]     += fmx;
      rhs[MX][k][j][i + 1] -= fmx;

      rhs[MY][k][j][i]     += fmy;
      rhs[MY][k][j][i + 1] -= fmy;

      rhs[MZ][k][j][i]     += fmz;
      rhs[MZ][k][j][i + 1] -= fmz;

      rhs[EN][k][j][i]     += fe;
      rhs[EN][k][j][i + 1] -= fe;

    }}}

  }else if (g_dir == JDIR){

  /* ------------------------
            Y  Sweep
     ------------------------ */

    for (k = KBEG    ; k <= KEND; k++){
    for (j = JBEG - 1; j <= JEND; j++){
    for (i = IBEG    ; i <= IEND; i++){

      rhom = 0.5*(rho[k][j][i] + rho[k][j + 1][i]);

      dvx_dx = (D_DX_J(vx))/dx;
      dvy_dx = (D_DX_J(vy))/dx;
      dvz_dx = (D_DX_J(vz))/dx;

      dvx_dy = (D_DY_J(vx))/dy;
      dvy_dy = (D_DY_J(vy))/dy;
      dvz_dy = (D_DY_J(vz))/dy;

      dvx_dz = (D_DZ_J(vx))/dz;
      dvy_dz = (D_DZ_J(vy))/dz;
      dvz_dz = (D_DZ_J(vz))/dz;

      Sxx = 0.5*(dvx_dx + dvx_dx);
      Sxy = 0.5*(dvx_dy + dvy_dx);
      Sxz = 0.5*(dvx_dz + dvz_dx);

      Syy = 0.5*(dvy_dy + dvy_dy);
      Syz = 0.5*(dvy_dz + dvz_dy);
      Szz = 0.5*(dvz_dz + dvz_dz);

      div = dvx_dx + dvy_dy + dvz_dz;

      S =        Sxx*Sxx + Syy*Syy + Szz*Szz 
          + 2.0*(Sxy*Sxy + Sxz*Sxz + Syz*Syz);
      S = sqrt(2.0*S);
      nu_t   = Csdl2*S;
      nu_max = MAX(nu_max, nu_t);

      scrh = dtdy*2.0*rhom*nu_t;
      
      fmx = scrh*Sxy;
      fmy = scrh*(Syy - div/3.0);
      fmz = scrh*Syz;

      dH  = dtdy*(H[k][j + 1][i] - H[k][j][i]);
      fe  = rhom*nu_t/Prandtl*dH/dy;
 
      rhs[MX][k][j][i]     += fmx;
      rhs[MX][k][j + 1][i] -= fmx;

      rhs[MY][k][j][i]     += fmy;
      rhs[MY][k][j + 1][i] -= fmy;

      rhs[MZ][k][j][i]     += fmz;
      rhs[MZ][k][j + 1][i] -= fmz;

      rhs[EN][k][j][i]     += fe;
      rhs[EN][k][j + 1][i] -= fe;

    }}}

  }else if (g_dir == KDIR){

  /* ------------------------
            Z  Sweep
     ------------------------ */

    for (k = KBEG - 1; k <= KEND; k++){
    for (j = JBEG    ; j <= JEND; j++){
    for (i = IBEG    ; i <= IEND; i++){

      rhom = 0.5*(rho[k + 1][j][i] + rho[k][j][i]);

      dvx_dx = (D_DX_K(vx))/dx;
      dvy_dx = (D_DX_K(vy))/dx;
      dvz_dx = (D_DX_K(vz))/dx;

      dvx_dy = (D_DY_K(vx))/dy;
      dvy_dy = (D_DY_K(vy))/dy;
      dvz_dy = (D_DY_K(vz))/dy;

      dvx_dz = (D_DZ_K(vx))/dz;
      dvy_dz = (D_DZ_K(vy))/dz;
      dvz_dz = (D_DZ_K(vz))/dz;

      Sxx = 0.5*(dvx_dx + dvx_dx);
      Sxy = 0.5*(dvx_dy + dvy_dx);
      Sxz = 0.5*(dvx_dz + dvz_dx);

      Syy = 0.5*(dvy_dy + dvy_dy);
      Syz = 0.5*(dvy_dz + dvz_dy);
      Szz = 0.5*(dvz_dz + dvz_dz);

      div = dvx_dx + dvy_dy + dvz_dz;

      S =        Sxx*Sxx + Syy*Syy + Szz*Szz 
          + 2.0*(Sxy*Sxy + Sxz*Sxz + Syz*Syz);
      S = sqrt(2.0*S);
      nu_t   = Csdl2*S;
      nu_max = MAX(nu_max, nu_t);

      scrh = dtdz*2.0*rhom*nu_t;
      
      fmx = scrh*Sxz;
      fmy = scrh*Syz;
      fmz = scrh*(Szz - div/3.0);

      dH  = dtdz*(H[k + 1][j][i] - H[k][j][i]);
      fe  = rhom*nu_t/Prandtl*dH/dz;

      rhs[MX][k][j][i]     += fmx;
      rhs[MX][k + 1][j][i] -= fmx;

      rhs[MY][k][j][i]     += fmy;
      rhs[MY][k + 1][j][i] -= fmy;

      rhs[MZ][k][j][i]     += fmz;
      rhs[MZ][k + 1][j][i] -= fmz;

      rhs[EN][k][j][i]     += fe;
      rhs[EN][k + 1][j][i] -= fe;

    }}}
  }

/* ---------------------------------------
                Filter 
   --------------------------------------- */

  for (k = KBEG; k <= KEND; k++){
  for (j = JBEG; j <= JEND; j++){
  for (i = IBEG; i <= IEND; i++){
    if (f[k][j][i] <= 0.0){
      rhs[MX][k][j][i] = 0.0;
      rhs[MY][k][j][i] = 0.0;
      rhs[MZ][k][j][i] = 0.0;
      rhs[EN][k][j][i] = 0.0;
    }
  }}}

/* ---------------------------------------------
            Check time step 
   --------------------------------------------- */

  {
    double dtvisc;
    double dxmin;
    
    dxmin = MIN(dx,dy);
    dxmin = MIN(dxmin,dz);
    dtvisc = 0.25*dxmin*dxmin/nu_max;
    if (g_dt > dtvisc){ 
      print (" dt too large %12.6e > %12.6e\n",g_dt, dtvisc);
      QUIT_PLUTO(1);
    }
  }
}
#undef D_DX_I
#undef D_DY_I
#undef D_DZ_I
#undef D_DX_J
#undef D_DY_J
#undef D_DZ_J
#undef D_DX_K
#undef D_DY_K
#undef D_DZ_K


double ***LES_GetFilter()
{
  return (f);
}



