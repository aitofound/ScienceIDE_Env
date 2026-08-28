/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Computes viscous fluxes and source terms for the HD/MHD equations. 

  Compute the stress tensor components (at the i+1/2 face of each cell) and
  adds explicit viscous terms to the energy and momentum equation. It is 
  called in the during the sweep integrators. The stress tensor is given by

  \f[                                                      
    \tens{\Pi} =  \nu_1\Big[\nabla\vec{v} + (\nabla\vec{v})^\intercal\Big]
                + \left(\nu_2 - \frac{2}{3}\nu_1\right)(\nabla\cdot\vec{v}) \tens{I}
  \f]
     
  where \f$\nu_1 = \mu\f$ is the dynamic viscosity and \f$\tens{I}\f$ is
  the unit tensor.
 
  In order to compute the viscous stress tensor, we report here the
  expression for the gradient of a vector in cylindrical and spherical 
  coordinates (see the book "I do like CFD, vol. I" by K. Masatsuka,
  Sec. 1.5.5, Eq. 1.5.35):  

  - Cylindrical:
  \f[
    \nabla\vec{v} = \left[\begin{array}{ccc}
    \DS  \pd{v_r}{r}  & \DS \frac{1}{r}\pd{v_r}{\phi}-\frac{v_\phi}{r}
                      & \DS \pd{v_r}{z}
    \\ \noalign{\medskip}
    \DS \pd{v_\phi}{r} & \DS \frac{1}{r}\pd{v_\phi}{\phi} + \frac{v_r}{r}
                       & \DS \pd{v_\phi}{z}
    \\ \noalign{\medskip}
    \DS \pd{v_z}{r} & \DS \frac{1}{r}\pd{v_z}{\phi}
                       & \DS \pd{v_z}{z}
    \end{array}\right]
    \quad\rightarrow\quad
    \boxed{
    \Pi = \nu_1\left[\begin{array}{ccc}
      \DS  2\pd{v_r}{r}
    & \DS   r\pd{}{r}\left(\frac{v_\phi}{r}\right) + \frac{1}{r}\pd{v_r}{\phi}
    & \DS   \pd{v_r}{z} + \pd{v_z}{r}
    \\ \noalign{\medskip}
      \DS  \Pi_{r\phi}
    & \DS  2\left(\frac{1}{r}\pd{v_\phi}{\phi} + \frac{v_r}{r}\right)
    & \DS \pd{v_\phi}{z} + \frac{1}{r}\pd{v_z}{\phi}
    \\ \noalign{\medskip}
      \DS \Pi_{rz}
    & \DS \Pi_{\phi z}
    & \DS 2\pd{v_z}{z}
    \end{array}\right]
    + \left(\nu_2 - \frac{2}{3}\nu_1\right)(\nabla\cdot\vec{v}) \tens{I}
    }
  \f]
  To avoid the singularity at the origin (when \f$\partial_\phi=0\f$), we
  compute the radial contribution of the divergence as
  \f[
     \frac{1}{r}\pd{(rv_r)}{r} \approx 2\frac{V_{r,i+1}|r_{i+1}| - V_{r,i}|r_{i}|}
                                             {r_{i+1}|r_{i+1}| - r_i|r_{i}|}
  \f]
  The source terms are computed using Eq. (64) of [Mig14],
  \f[
      \left(\frac{T_{\phi\phi}}{r}\right)_i \approx
      \frac{(T_{\phi\phi})_{i-\HALF} + (T_{\phi\phi})_{i+\HALF}}{2r_i}
      \qquad{\rm using}\qquad
      \left.\frac{v_r}{r}\right|_{i+\HALF} = \left(
       \frac{1}{r}\pd{(rv_r)}{r} - \pd{v_r}{r}\right)_{i+\HALF}
  \f]
  
  - Spherical:
  \f[
    \nabla\vec{v} = \left[\begin{array}{ccc}
    \DS  \pd{v_r}{r}  & \DS \frac{1}{r\sin\theta}\pd{v_r}{\phi}-\frac{v_\phi}{r}
                      & \DS \frac{1}{r}\pd{v_r}{\theta} - \frac{v_\theta}{r}
    \\ \noalign{\medskip}
    \DS \pd{v_\phi}{r} & \DS    \frac{1}{r\sin\theta}\pd{v_\phi}{\phi}
                              + \frac{v_r}{r} + \frac{a_\theta}{r\tan\theta}             
                       & \DS \frac{1}{r}\pd{v_\phi}{\theta}
    \\ \noalign{\medskip}
    \DS \pd{v_\phi}{r} & \DS   \frac{1}{r\sin\theta}\pd{v_\theta}{\phi}
                             - \frac{v_\phi}{r\tan\theta}
                       & \DS \frac{1}{r}\pd{v_\theta}{\theta} + \pd{v_r}{r}
    \end{array}\right]
  \f]
  so that
  \f[
    \boxed{
    \Pi = \nu_1\left[\begin{array}{ccc}
      \DS  2\pd{v_r}{r}
    & \DS   r\pd{}{r}\left(\frac{v_\theta}{r}\right) + \frac{1}{r}\pd{v_r}{\theta}
    & \DS   \frac{1}{r\sin\theta}\pd{v_r}{\phi} + r\pd{}{r}\left(\frac{v_\phi}{r}\right)
    \\ \noalign{\medskip}
      \DS  \Pi_{r\theta}
    & \DS  2\left(\frac{1}{r}\pd{v_\theta}{\theta} + \frac{v_r}{r}\right)
    & \DS   \frac{\sin\theta}{r}\pd{}{\theta}\left(\frac{v_\phi}{\sin\theta}\right)
          + \frac{1}{r\sin\theta}\pd{v_\theta}{\phi}
    \\ \noalign{\medskip}
      \DS \Pi_{r\phi}
    & \DS \Pi_{\theta\phi}
    & \DS 2\left(  \frac{1}{r\sin\theta}\pd{v_\phi}{\phi} + \frac{v_r}{r}
                 + \frac{v_\theta\cot\theta}{r}\right)
    \end{array}\right]
    + \left(\nu_2 - \frac{2}{3}\nu_1\right)(\nabla\cdot\vec{v}) \tens{I}
    }
  \f]

\code
restart;

#div := 2*(abs(r[i+1])*V[i+1] - abs(r[i])*V[i])/(r[i+1]*abs(r[i+1]) - r[i]*abs(r[i]));
div := 3*(r[i+1]^2*V[i+1] - r[i]^2*V[i])/(r[i+1]^3 - r[i]^3);

r[i] := -r[i+1]; V[i] := -V[i+1];
simplify(div);
\endcode

  \b References
     - [Mig14] "High-order conservative reconstruction schemes for finite
        volume methods in cylindrical and spherical coordinates"
        A. Mignone, JCP (2014), 270, 784 

  \authors A. Mignone (andrea.mignone@unito.it) \n
           Petros Tzeferacos
  \date    Nov 25, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void ViscousFlux (const Data *d, double **ViF, double **ViS, 
                  double *dcoeff, int beg, int end, Grid *grid)
/*!
 *
 *  \param [in]      V  data array containing cell-centered quantities
 *  \param [in,out]  ViF pointer to viscous fluxes
 *  \param [in,out]  ViS pointer to viscous source terms
 *  \param [in,out]  dcoeff  pointer to diffusion coefficient for dt calculation
 *  \param [in]      beg     integer, index for loop beg
 *  \param [in]      end     integer, index for loop end
 *  \param [in]      grid  pointer to array of Grid structures 
 *
 *  \return This function has no return value.
 ************************************************************************* */
{
  int i,j,k,n,nv;
  double nu1, nu2;
  const double c23 = 2.0/3.0;
  double inv_dx1, inv_dx2, inv_dx3;
  double *x1 = grid->x[IDIR], *dx1 = grid->dx[IDIR];
  double *x2 = grid->x[JDIR], *dx2 = grid->dx[JDIR];
  double *x3 = grid->x[KDIR], *dx3 = grid->dx[KDIR];
  double *x1r = grid->xr[IDIR];
  double *x2r = grid->xr[JDIR];
  double *x3r = grid->xr[KDIR];
  double dxVx, dxVy, dxVz, dyVx, dyVy, dyVz, dzVx, dzVy, dzVz;
  double divV, divr, scrh;
  static double *tau_xx, *tau_xy, *tau_xz,
                *tau_yx, *tau_yy, *tau_yz,
                *tau_zx, *tau_zy, *tau_zz;
  double ***Vx = d->Vc[VX1];
  double ***Vy = d->Vc[VX2];
  double ***Vz = d->Vc[VX3];
  double *r, *th, r_1, dr, s_1, tan_1;
  double vc[NVAR], vi[NVAR]; /* Center and interface values */
  static double *one_dVr, *one_dmu; /*auxillary volume components for r_1 singularity @ cylindrical and spherical*/

  #ifdef FARGO
  static double ***Vphi_tot;
  if (Vphi_tot == NULL) Vphi_tot = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, double);
  
  FARGO_ComputeTotalVelocity (d, Vphi_tot, grid);
  #if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
  Vy = Vphi_tot;
  #elif GEOMETRY == SPHERICAL
  Vz = Vphi_tot;
  #endif
  #endif

/* --------------------------------------------------------
   0. Set pointers to coordinates and grid indices,
      allocate memory
   -------------------------------------------------------- */

  if (tau_xx == NULL){
    tau_xx = ARRAY_1D(NMAX_POINT, double);
    tau_xy = ARRAY_1D(NMAX_POINT, double);
    tau_xz = ARRAY_1D(NMAX_POINT, double);
    tau_yx = ARRAY_1D(NMAX_POINT, double);
    tau_yy = ARRAY_1D(NMAX_POINT, double);
    tau_yz = ARRAY_1D(NMAX_POINT, double);
    tau_zx = ARRAY_1D(NMAX_POINT, double);
    tau_zy = ARRAY_1D(NMAX_POINT, double);
    tau_zz = ARRAY_1D(NMAX_POINT, double);
  }

#if GEOMETRY != CARTESIAN
  r  = grid->x[IDIR]; th = grid->x[JDIR];
  if (one_dVr == NULL){
    one_dVr = ARRAY_1D(NX1_TOT, double);  /* -- intercell (i) and (i+1) volume -- */
    one_dmu = ARRAY_1D(NX2_TOT, double);  /* -- intercell (j) and (j+1) volume -- */
    for (i = 0; i < NX1_TOT - 1; i++){
      one_dVr[i] = r[i+1]*fabs(r[i + 1]) - r[i]*fabs(r[i]);
      one_dVr[i] = 2.0/one_dVr[i];
    }
    for (j = 0; j < NX2_TOT - 1; j++){
      one_dmu[j] = 1.0 - cos(th[j + 1]) - (1.0 - cos(th[j]))*(th[j] > 0.0 ? 1.0:-1.0);
      one_dmu[j] = 1.0/one_dmu[j];
    }
  }
#endif

  i = g_i;
  j = g_j;
  k = g_k;

  dxVx = dxVy = dxVz = 0.0;
  dyVx = dyVy = dyVz = 0.0;
  dzVx = dzVy = dzVz = 0.0;
  
  if (g_dir == IDIR){   

  /* ------------------------------------------------------
     1. Compute derivatives of velocity at x1-zone
        interfaces
     ------------------------------------------------------ */

    inv_dx2 = 1.0/dx2[j]; 
    inv_dx3 = 1.0/dx3[k];
    for (i = beg; i <= end; i++){
      inv_dx1 = 1.0/dx1[i];

    /* -- 1a. Compute face- and cell-centered values  -- */

      NVAR_LOOP(nv) {
        vi[nv] = 0.5*(d->Vc[nv][k][j][i] + d->Vc[nv][k][j][i+1]);
        vc[nv] = d->Vc[nv][k][j][i];
      }
      #ifdef FARGO
      vi[VX1 + SDIR] = 0.5*(Vphi_tot[k][j][i] + Vphi_tot[k][j][i+1]);
      #endif
      
      /* -- 1b. Compute viscosity and velocity derivatives -- */
    
      Visc_nu(vi, x1r[i], x2[j], x3[k], &nu1, &nu2);

      dcoeff[i]  = MAX(nu1, nu2);
      dcoeff[i] /= vi[RHO];

      tau_xx[i] = tau_xy[i] = tau_xz[i] = tau_yx[i]= 
      tau_yy[i] = tau_yz[i] = tau_zx[i] = tau_zy[i]= 
      tau_zz[i] = 0.0;

      #if INCLUDE_IDIR
      dxVx = FDIFF_X1(Vx,k,j,i)*inv_dx1;
      dxVy = FDIFF_X1(Vy,k,j,i)*inv_dx1;
      dxVz = FDIFF_X1(Vz,k,j,i)*inv_dx1;
      #endif
      #if INCLUDE_JDIR
      dyVx = 0.5*(CDIFF_X2(Vx,k,j,i) + CDIFF_X2(Vx,k,j,i+1))*inv_dx2;
      dyVy = 0.5*(CDIFF_X2(Vy,k,j,i) + CDIFF_X2(Vy,k,j,i+1))*inv_dx2;
      dyVz = 0.5*(CDIFF_X2(Vz,k,j,i) + CDIFF_X2(Vz,k,j,i+1))*inv_dx2;
      #endif
      #if INCLUDE_KDIR
      dzVx = 0.5*(CDIFF_X3(Vx,k,j,i) + CDIFF_X3(Vx,k,j,i+1))*inv_dx3;
      dzVy = 0.5*(CDIFF_X3(Vy,k,j,i) + CDIFF_X3(Vy,k,j,i+1))*inv_dx3;
      dzVz = 0.5*(CDIFF_X3(Vz,k,j,i) + CDIFF_X3(Vz,k,j,i+1))*inv_dx3;
      #endif

    /* ------------------------------------------
       1c. Compute stress tensor components and
           geometrical source terms in different
           coordinate systems
       ------------------------------------------ */

      #if GEOMETRY == CARTESIAN      

      divV = DIM_EXPAND(dxVx, + dyVy , + dzVz); 

      tau_xx[i] = 2.0*nu1*dxVx + (nu2 - c23*nu1)*divV; 
      tau_xy[i] = nu1*(dyVx + dxVy); 
      tau_xz[i] = nu1*(dzVx + dxVz); 
      tau_yx[i] = tau_xy[i];
      tau_zx[i] = tau_xz[i];
       
      ViS[i][MX1] = 0.0; 
      ViS[i][MX2] = 0.0; 
      ViS[i][MX3] = 0.0; 
     
      #elif GEOMETRY == CYLINDRICAL

      r = grid->x[IDIR]; dr = grid->dx[IDIR][i];
      inv_dx3 = 0.0;
      r_1 = 1.0/x1[i];
       
      divV = DIM_EXPAND( (Vx[k][j][i+1]*r[i+1] - Vx[k][j][i]*fabs(r[i]))*one_dVr[i],
                        + dyVy, + 0.0); 

      tau_xx[i] = 2.0*nu1*dxVx + (nu2 - (2.0/3.0)*nu1)*divV; 
      tau_xy[i] = nu1*(dyVx + dxVy);  
      tau_xz[i] = nu1*0.5*(r[i]+r[i+1])*inv_dx1*((1./r[i+1])*Vz[k][j][i+1] 
                                               - (1./r[i])*Vz[k][j][i]);
      tau_yx[i] = tau_xy[i]; 
      tau_zx[i] = tau_xz[i]; 
      
      /* -- we calculate at the center cause we don't need it for flux
            but for src, avoiding 1/r->inf at r=r_f=0 -- */

      Visc_nu(vc, x1[i], x2[j], x3[k], &nu1, &nu2);

      divV = DIM_EXPAND(  0.5*(Vx[k][j][i+1]-Vx[k][j][i-1])*inv_dx1 + Vx[k][j][i]*r_1,
                      + 0.5*(Vy[k][j + 1][i]-Vy[k][j - 1][i])*inv_dx2, 
                      + 0.0 ); 
 
      tau_zz[i] = 2.0*nu1*r_1*Vx[k][j][i] + (nu2 - (2.0/3.0)*nu1)*divV;
       
      ViS[i][MX1] = -tau_zz[i]*r_1;
      ViS[i][MX2] = 0.0;
      ViS[i][MX3] = 0.0;

      #elif GEOMETRY == POLAR 
      r_1  = 1.0/x1r[i];

    /* -- Here divr = 1/r*diff(r*vr,r) -- */
    
      divr = 2.0*(Vx[k][j][i+1]*fabs(x1[i+1]) - Vx[k][j][i]*fabs(x1[i]))
                /(x1[i+1]*fabs(x1[i+1]) - x1[i]*fabs(x1[i]));
      divV = DIM_EXPAND(divr, + r_1*dyVy, + dzVz); 

      tau_xx[i] = 2.0*nu1*dxVx + (nu2 - c23*nu1)*divV;
      
    /* -- Compute d(vphi/r) / dr -- */
    
      scrh = x1r[i]*(Vy[k][j][i+1]/x1[i+1] - Vy[k][j][i]/x1[i])*inv_dx1;
      scrh = DIM_EXPAND(scrh, + r_1*dyVx, + 0.0);
      tau_xy[i] = nu1*scrh;
      tau_xz[i] = nu1*(dzVx + dxVz);       
      tau_yx[i] = tau_xy[i];
      tau_zx[i] = tau_xz[i]; 

    /* ------------------------------------------
       To avoid singularity vr/r, we note that
       vr/r = divr - dvr/dr
       ------------------------------------------ */
      
      scrh = DIM_EXPAND(divr - dxVx, + r_1*dyVy, + 0.0);
      tau_yy[i] = 2.0*nu1*scrh + (nu2 - c23*nu1)*divV;

      r_1  = 1.0/x1[i];
      
      ViS[i][MX1] = -0.5*(tau_yy[i-1] + tau_yy[i])*r_1;
      ViS[i][MX2] = 0.0;
      ViS[i][MX3] = 0.0;
                                            
      #elif GEOMETRY == SPHERICAL

      r = grid->xr[IDIR]; th = grid->x[JDIR];
      r_1  = 1.0/x1r[i];
      tan_1= 1.0/tan(th[j]); s_1 = 1.0/sin(th[j]);

      divV = DIM_EXPAND(  2.0*r_1*vi[VX1] + dxVx,
                      + r_1*dyVy + r_1*tan_1*vi[VX2], 
                      + r_1*s_1*dzVz); 
       
      tau_xx[i] = 2.0*nu1*dxVx + (nu2 - (2.0/3.0)*nu1)*divV;
      tau_xy[i] = nu1*(r_1*dyVx + dxVy - r_1*vi[VX2]);
      tau_xz[i] = nu1*(r_1*s_1*dzVx + dxVz - r_1*vi[VX3]);
      tau_yx[i] = tau_xy[i]; 
      tau_yy[i] = 2.0*nu1*(r_1*dyVy + r_1*vi[VX1]) + (nu2 - c23*nu1)*divV; 
      tau_zx[i] = tau_xz[i];
      tau_zz[i] = 2.0*nu1*(r_1*s_1*dzVz + r_1*vi[VX1] + tan_1*r_1*vi[VX2]) 
                        + (nu2 - c23*nu1)*divV;

      r_1  = 1.0/x1[i];
      ViS[i][MX1] = -0.5*(  tau_yy[i-1] + tau_yy[i]
                          + tau_zz[i-1] + tau_zz[i])*r_1;
      ViS[i][MX2] = 0.0;
      ViS[i][MX3] = 0.0;       
               
      #endif  /* -- end #if GEOMETRY -- */

    /* ------------------------------------------
       1d. Compute fluxes at x1-faces
       ------------------------------------------ */

      ViF[i][MX1] = tau_xx[i];
      ViF[i][MX2] = tau_xy[i];
      ViF[i][MX3] = tau_xz[i];

      #if HAVE_ENERGY
      ViF[i][ENG] = vi[VX1]*tau_xx[i] + vi[VX2]*tau_yx[i] + vi[VX3]*tau_zx[i];
      #endif
    }

  }else if (g_dir == JDIR){ 

  /* ------------------------------------------------------
     2. Compute derivatives of velocity at x2-zone
        interfaces
     ------------------------------------------------------ */

    inv_dx1 = 1.0/dx1[i];
    inv_dx3 = 1.0/dx3[k];
    for (j = beg ; j <= end; j++){
      inv_dx2 = 1.0/dx2[j];

    /* -- 2a. Compute face- and cell-centered values  -- */

      NVAR_LOOP(nv) {
        vi[nv] = 0.5*(d->Vc[nv][k][j+1][i] + d->Vc[nv][k][j][i]);
        vc[nv] = d->Vc[nv][k][j][i];
      }
      #ifdef FARGO
      vi[VX1 + SDIR] = 0.5*(Vphi_tot[k][j+1][i] + Vphi_tot[k][j][i]);
      #endif

    /* -- 2b. Compute viscosity and velocity derivatives -- */
    
      Visc_nu(vi, x1[i], x2r[j], x3[k], &nu1, &nu2);
      dcoeff[j]  = MAX(nu1,nu2);
      dcoeff[j] /= vi[RHO];

      tau_xx[j]= tau_xy[j]= tau_xz[j]= tau_yx[j] = tau_yy[j]= 
      tau_yz[j]= tau_zx[j]= tau_zy[j]= tau_zz[j] =0.0;
      
      #if INCLUDE_IDIR
      dxVx = 0.5*(CDIFF_X1(Vx,k,j,i) + CDIFF_X1(Vx,k,j+1,i))*inv_dx1;
      dxVy = 0.5*(CDIFF_X1(Vy,k,j,i) + CDIFF_X1(Vy,k,j+1,i))*inv_dx1;
      dxVz = 0.5*(CDIFF_X1(Vz,k,j,i) + CDIFF_X1(Vz,k,j+1,i))*inv_dx1;
      #endif
      #if INCLUDE_JDIR
      dyVx = FDIFF_X2(Vx,k,j,i)*inv_dx2;
      dyVy = FDIFF_X2(Vy,k,j,i)*inv_dx2;
      dyVz = FDIFF_X2(Vz,k,j,i)*inv_dx2;
      #endif
      #if INCLUDE_KDIR
      dzVx = 0.5*(CDIFF_X3(Vx,k,j,i) + CDIFF_X3(Vx,k,j+1,i))*inv_dx3;
      dzVy = 0.5*(CDIFF_X3(Vy,k,j,i) + CDIFF_X3(Vy,k,j+1,i))*inv_dx3;
      dzVz = 0.5*(CDIFF_X3(Vz,k,j,i) + CDIFF_X3(Vz,k,j+1,i))*inv_dx3;
      #endif

    /* ------------------------------------------
       2c. Compute stress tensor components and
           geometrical source terms in different
           coordinate systems
       ------------------------------------------ */

      #if GEOMETRY == CARTESIAN
      divV = DIM_EXPAND(dxVx, + dyVy, + dzVz); 
       
      tau_xy[j] = nu1*(dyVx + dxVy);
      tau_yx[j] = tau_xy[j];
      tau_yy[j] = 2.0*nu1*dyVy + (nu2 - c23*nu1)*divV;
      tau_yz[j] = nu1*(dyVz + dzVy);
      tau_zy[j] = tau_yz[j];
       
      ViS[j][MX1] = 0.0;
      ViS[j][MX2] = 0.0;
      ViS[j][MX3] = 0.0;

      #elif GEOMETRY == CYLINDRICAL
       
      r = grid->x[IDIR]; th = grid->x[KDIR];
      dr = grid->dx[IDIR][i];
      r_1 = 1.0/grid->x[IDIR][i]; inv_dx3 = 0.0;

      divV = DIM_EXPAND(r_1*vi[VX1] + dxVx, + dyVy, + 0.0); 

      tau_xy[j] = nu1*(dyVx + dxVy);
      tau_yx[j] = tau_xy[j];  
      tau_yy[j] = 2.0*nu1*dyVy + (nu2 - c23*nu1)*divV; 
      tau_yz[j] = nu1*(dyVz); 
      tau_zy[j] = tau_yz[j]; 
       
      ViS[j][MX1] = 0.0;
      ViS[j][MX2] = 0.0;
      ViS[j][MX3] = 0.0;

      #elif GEOMETRY == POLAR 

      r_1  = 1.0/x1[i];
       
      divV = DIM_EXPAND(r_1*vi[VX1] + dxVx, + r_1*dyVy, + dzVz); 

      tau_xy[j] = nu1*(r_1*dyVx + dxVy - r_1*vi[VX2]); 
      tau_yx[j] = tau_xy[j]; 
      tau_yy[j] = 2.0*nu1*(r_1*dyVy + r_1*vi[VX1]) + (nu2 - c23*nu1)*divV;
      tau_yz[j] = nu1*(r_1*dyVz + dzVy); 
      tau_zy[j] = tau_yz[j]; 
     
      ViS[j][MX1] = 0.0;
      ViS[j][MX2] = 0.0;
      ViS[j][MX3] = 0.0;
      
      #elif GEOMETRY == SPHERICAL
       
      r = grid->x[IDIR]; th = grid->xr[JDIR];
      r_1  = 1.0/x1[i];
      tan_1= 1.0/tan(th[j]); s_1 = 1.0/sin(th[j]);

       /*------------------------------------
       !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
         Hotfix for tan_1 at the axis: 
        since we have terms tan_1*smth that 
        cannot be treated with the volume
        trick, we set an if condition 
        for this to go to zero, as it is
        the correct behaviour of the  
        term in question there.
       !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
       --------------------------------------*/

       if (fabs(tan(th[j]))< 1.e-12) {
         tan_1 = 0.0;
         s_1   = 0.0;
       }
       
       th = grid->x[JDIR];
       
       divV = DIM_EXPAND( 2.0*r_1*vi[VX1] + dxVx,
                       + ( sin(th[j + 1])*Vy[k][j + 1][i] - fabs(sin(th[j]))*Vy[k][j][i])*r_1*one_dmu[j], 
                       + r_1*s_1*dzVz); 
      
       th = grid->xr[JDIR];

      tau_xy[j] = nu1*(r_1*dyVx + dxVy - r_1*vi[VX2]); 
      tau_yx[j] = tau_xy[j]; 
      tau_yy[j] = 2.0*nu1*(r_1*dyVy + r_1*vi[VX1]) + (nu2 - c23*nu1)*divV;
      #if INCLUDE_JDIR
      tau_yz[j] = nu1*(r_1*dyVz - tan_1*r_1*vi[VX3]);
      #endif       
      #if INCLUDE_KDIR
      tau_yz[j] = nu1*(s_1*r_1*dzVy + r_1*dyVz - tan_1*r_1*vi[VX3]);
      #endif       /*tau_thitaphi= eta1 (1/rs dphiVthita + 1/r dthitaVphi -1/r cot Vphi)*/
      tau_zy[j] = tau_yz[j]; /*tau_phithita*/
      #if DIMENSIONS <= 2                  
      tau_zz[j] = 2.0*nu1*(r_1*vi[VX1] + tan_1*r_1*vi[VX2]) 
                          + (nu2 - c23*nu1)*divV;
      #endif       
      #if INCLUDE_KDIR
      tau_zz[j] = 2.0*nu1*(r_1*s_1*dzVz + r_1*vi[VX1] + tan_1*r_1*vi[VX2]) 
                  + (nu2 - c23*nu1)*divV;
      #endif       /*tau_phiphi = 2eta1(1/rs dphiVphi + 1/r Vr + 1/r cot Vthita) + (eta2 -2/3 eta1)divV */
                  
      th = grid->x[JDIR]; tan_1= 1.0/tan(th[j]);
      
      ViS[j][MX1] = 0.0;
      ViS[j][MX2] = 0.5*(  tau_yx[j-1] + tau_yx[j])*r_1
                         - tan_1*0.5*(tau_zz[j-1] + tau_zz[j])*r_1;
      ViS[j][MX3] = 0.0;

      #endif  /* -- GEOMETRY -- */
      
    /* ------------------------------------------
       2d. Compute fluxes at x2-faces
       ------------------------------------------ */

      ViF[j][MX1] = tau_yx[j];
      ViF[j][MX2] = tau_yy[j];
      ViF[j][MX3] = tau_yz[j];

      #if HAVE_ENERGY
      ViF[j][ENG] = vi[VX1]*tau_xy[j] + vi[VX2]*tau_yy[j] + vi[VX3]*tau_zy[j];
      #endif

    }      

  }else if (g_dir == KDIR){ 
   
  /* ------------------------------------------------------
     3. Compute derivatives of velocity at x3-zone
        interfaces
     ------------------------------------------------------ */
     
    inv_dx1 = 1.0/dx1[i];
    inv_dx2 = 1.0/dx2[j]; 
    for (k = beg; k <= end; k++){
      inv_dx3 = 1.0/grid->dx[KDIR][k];

    /* -- 3a. Compute face- and cell-centered values  -- */

      NVAR_LOOP(nv) {
        vi[nv] = 0.5*(d->Vc[nv][k][j][i] + d->Vc[nv][k+1][j][i]);
        vc[nv] = d->Vc[nv][k][j][i];
      }
      #ifdef FARGO
      vi[VX1+SDIR] = 0.5*(Vphi_tot[k][j][i] + Vphi_tot[k+1][j][i]);
      #endif

    /* -- 3b. Compute viscosity and velocity derivatives -- */
    
      Visc_nu(vi, x1[i], x2[j], x3r[k], &nu1, &nu2);
      dcoeff[k]  = MAX(nu1, nu2);
      dcoeff[k] /= vi[RHO];

      tau_xx[k] = tau_xy[k] = tau_xz[k]= tau_yx[k]= tau_yy[k]= tau_yz[k]= 
      tau_zx[k] = tau_zy[k] = tau_zz[k]=0.0;

      #if INCLUDE_IDIR
      dxVx = 0.5*(CDIFF_X1(Vx,k,j,i) + CDIFF_X1(Vx,k+1,j,i))*inv_dx1;
      dxVy = 0.5*(CDIFF_X1(Vy,k,j,i) + CDIFF_X1(Vy,k+1,j,i))*inv_dx1;
      dxVz = 0.5*(CDIFF_X1(Vz,k,j,i) + CDIFF_X1(Vz,k+1,j,i))*inv_dx1;
      #endif
      #if INCLUDE_JDIR
      dyVx = 0.5*(CDIFF_X2(Vx,k,j,i) + CDIFF_X2(Vx,k+1,j,i))*inv_dx2;
      dyVy = 0.5*(CDIFF_X2(Vy,k,j,i) + CDIFF_X2(Vy,k+1,j,i))*inv_dx2;
      dyVz = 0.5*(CDIFF_X2(Vz,k,j,i) + CDIFF_X2(Vz,k+1,j,i))*inv_dx2;
      #endif
      #if INCLUDE_KDIR
      dzVx = FDIFF_X3(Vx,k,j,i)*inv_dx3;
      dzVy = FDIFF_X3(Vy,k,j,i)*inv_dx3;
      dzVz = FDIFF_X3(Vz,k,j,i)*inv_dx3;
      #endif

    /* ------------------------------------------
       3c. Compute stress tensor components and
           geometrical source terms in different
           coordinate systems
       ------------------------------------------ */

      #if GEOMETRY == CARTESIAN

      divV = dxVx + dyVy + dzVz;

      tau_xz[k] = nu1*(dzVx + dxVz);  
      tau_yz[k] = nu1*(dyVz + dzVy);
      tau_zx[k] = tau_xz[k];
      tau_zy[k] = tau_yz[k];
      tau_zz[k] = 2.0*nu1*dzVz + (nu2 - c23*nu1)*divV;

      ViS[k][MX1] = 0.0;
      ViS[k][MX2] = 0.0;
      ViS[k][MX3] = 0.0;

      #elif GEOMETRY == POLAR 

      r_1  = 1.0/x1[i];

      divV = DIM_EXPAND(r_1*vi[VX1] + dxVx, + r_1*dyVy, + dzVz); 

      tau_xz[k] = nu1*(dzVx + dxVz); 
      tau_yy[k] = 2.0*nu1*(r_1*dyVy + r_1*vi[VX1]) + (nu2 - c23*nu1)*divV;
      tau_yz[k] = nu1*(r_1*dyVz + dzVy); 
      tau_zx[k] = tau_xz[k]; 
      tau_zy[k] = tau_yz[k]; 
      tau_zz[k] = 2.0*nu1*dzVz + (nu2 - c23*nu1)*divV;

      ViS[k][MX1] = 0.0;
      ViS[k][MX2] = 0.0;
      ViS[k][MX3] = 0.0;
                   
      #elif GEOMETRY == SPHERICAL

      r    = grid->x[IDIR]; th = grid->x[JDIR];
      r_1  = 1.0/grid->x[IDIR][i]; tan_1= 1.0/tan(th[j]); s_1 = 1.0/sin(th[j]);

      divV =   2.0*r_1*vi[VX1] + dxVx + r_1*dyVy + r_1*tan_1*vi[VX2]
                    + r_1*s_1*dzVz; 

      tau_xz[k] = nu1*(r_1*s_1*dzVx + dxVz - r_1*vi[VX3]);  
      tau_yz[k] = nu1*(s_1*r_1*dzVy + r_1*dyVz - tan_1*r_1*vi[VX3]);
      tau_zx[k] = tau_xz[k];
      tau_zy[k] = tau_yz[k];
      tau_zz[k] = 2.0*nu1*(r_1*s_1*dzVz + r_1*vi[VX1] + tan_1*r_1*vi[VX2])
                  + (nu2 - c23*nu1)*divV;

      ViS[k][MX1] = 0.0;
      ViS[k][MX2] = 0.0;
      ViS[k][MX3] = 0.0;
                                     
      #endif  /* -- end #if GEOMETRY -- */

    /* ------------------------------------------
       3d. Compute fluxes at x3-faces
       ------------------------------------------ */

      ViF[k][MX1] = tau_zx[k];
      ViF[k][MX2] = tau_zy[k]; 
      ViF[k][MX3] = tau_zz[k]; 

      #if HAVE_ENERGY
      ViF[k][ENG] = vi[VX1]*tau_xz[k] + vi[VX2]*tau_yz[k] + vi[VX3]*tau_zz[k];
      #endif
    }/*loop*/
  }/*sweep*/
}
