/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Average staggered magnetic field to zone center.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   March 15, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void CT_AverageStaggeredFieldsOld (double ****Vs, double ****UU, RBox *box,
                                   Grid *grid)
/*!
 * Average staggered magnetic field components to form a zone-centered 
 * field, e.g., \f$ \av{B_i} = (B_{i+1/2} + B_{i-1/2})/2\f$.
 * In cylindrical coordinates the volume-averaged radial component 
 * is obtained as
 * \f[
 *   \av{B_R} = \frac{1}{\Delta V} \int B(R) R dR 
 *    \qquad\mathrm{where}\qquad
 *     B(R) =   B_{R,i+\HALF} \frac{R - R_{i-\HALF}}{\Delta R} 
 *            + B_{R,i-\HALF} \frac{R_{i+\HALF} - R}{\Delta R}
 * \f] 
 * this yields
 * \f[
 *   \av{B_R} =  \frac{R_{i+\HALF} + 2R_{i-\HALF}}{3(R_{i+\HALF}+R_{i-\HALF})}
 *                B_{R,i-\HALF}
 *             + \frac{2R_{i+\HALF} + R_{i-\HALF}}{3(R_{i+\HALF}+R_{i-\HALF})}
 *                B_{R,i-\HALF}
 * \f]
 * Simlar expressions hold in spherical coordinates. 
 * Check with the following MAPLE script:
 * \code
   restart;
   J     := sin(xi); # J = xi, xi^2, sin(xi)
   Br    := (xi - a)/Delta*B[R] + (b - xi)/Delta*B[L];
   Bav   := int (Br*J, xi=a..b) / int (J, xi=a..b):
   Delta := b-a; 
   Bav   := simplify(Bav);
   cp := coeff(Bav, B[R]);
   cm := coeff(Bav, B[L]);
   
   # Check symmetry propetries (sign must not change)
    
   eval([cm,cp],{a=0,b=1}); 
   eval([cm,cp],{a=-1,b=0});
 * \endcode
 * 
 * The averaging is done inside an arbitrary rectangular box.
 * The box may include boundary cells only during the predictor 
 * step of the CT-CTU algorithm, whereas is useless for RK time stepping.
 *
 * When the CT_EN_CORRECTION flag is enabled, we also redefine the
 * zone total energy using the newly formed cell-centered field, i.e.,
 *
 *  \f[ 
 *   E_i \to E_i - \frac{\mathbf{B}_i^2}{2} + \frac{<\mathbf{B}_i^2>}{2}
 *  \f]
 *
 * \param [in]   Vs    array of staggered fields
 * \param [out]  UU    array of conservative variables
 * \param [in]   box   pointer to RBox structure 
 * \param [in]   grid  pointer to Grid structure
 ************************************************************************ */
{
  int i, j, k;
  double b2_old, b2_new;
  double Bx1_ave, Bx2_ave, Bx3_ave;
  double cp, cm, rp, rm;
  double *x1r = grid->xr[IDIR], *x1l = grid->xl[IDIR];
  #if GEOMETRY == SPHERICAL
  double dp, dm, thp, thm;
  double *x2r = grid->xr[JDIR], *x2l = grid->xl[JDIR];
  #endif
  DIM_EXPAND(double ***Bx1s = Vs[BX1s];  ,
             double ***Bx2s = Vs[BX2s];  ,
             double ***Bx3s = Vs[BX3s]; )
  #if PHYSICS == ResRMHD
  double Ex1_ave, Ex2_ave, Ex3_ave;
  DIM_EXPAND(double ***Ex1s = Vs[EX1s];   , 
             double ***Ex2s = Vs[EX2s];   ,
             double ***Ex3s = Vs[EX3s];)
  #endif  
  
/* ---------------------------------------------------------
   1. Loop over all zones in the given box.
   --------------------------------------------------------- */

  BOX_LOOP(box,k,j,i){

  /* ------------------------------------------------------
     1a. Obtain zone-centered field from staggered values
     ------------------------------------------------------ */

    #if GEOMETRY == CARTESIAN 
    DIM_EXPAND(Bx1_ave = 0.5*(Bx1s[k][j][i] + Bx1s[k][j][i-1]);  ,
               Bx2_ave = 0.5*(Bx2s[k][j][i] + Bx2s[k][j-1][i]);  ,
               Bx3_ave = 0.5*(Bx3s[k][j][i] + Bx3s[k-1][j][i]); )

    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    DIM_EXPAND(Ex1_ave = 0.5*(Ex1s[k][j][i] + Ex1s[k][j][i-1]);  ,
               Ex2_ave = 0.5*(Ex2s[k][j][i] + Ex2s[k][j-1][i]);  ,
               Ex3_ave = 0.5*(Ex3s[k][j][i] + Ex3s[k-1][j][i]); )
    #endif

    #elif GEOMETRY == CYLINDRICAL

    rp = x1r[i];
    rm = x1l[i];
    cm = (rp + 2.0*rm)/(3.0*(rp + rm));
    cp = 1.0 - cm; 
    Bx1_ave  = cm*Bx1s[k][j][i-1] + cp*Bx1s[k][j][i];
    Bx2_ave  = 0.5*(Bx2s[k][j-1][i] + Bx2s[k][j][i]);

    #elif GEOMETRY == POLAR

    rp = x1r[i];
    rm = x1l[i];
    cm = (rp + 2.0*rm)/(3.0*(rp + rm));
    cp = 1.0 - cm;

    DIM_EXPAND(Bx1_ave = cm*Bx1s[k][j][i-1] + cp*Bx1s[k][j][i];   ,
               Bx2_ave = 0.5*(Bx2s[k][j-1][i] + Bx2s[k][j][i]);   ,
               Bx3_ave = 0.5*(Bx3s[k-1][j][i] + Bx3s[k][j][i]);)

    #elif GEOMETRY == SPHERICAL
    rp = x1r[i];
    rm = x1l[i];

    cm =  (3.0*rm*rm + 2.0*rm*rp + rp*rp)
         /(4.0*(rm*rm + rm*rp + rp*rp));
    cp = 1.0 - cm;
    
    thp = x2r[j];
    thm = x2l[j];
 
    dm  = (thm - thp)*cos(thm) - sin(thm) + sin(thp);
    dm /= (thm - thp)*(cos(thm) - cos(thp));
    dp  = 1.0 - dm;
    DIM_EXPAND(Bx1_ave  = cm*Bx1s[k][j][i-1] + cp*Bx1s[k][j][i];  ,
               Bx2_ave  = dm*Bx2s[k][j-1][i] + dp*Bx2s[k][j][i];  ,
               Bx3_ave  = 0.5*(Bx3s[k-1][j][i] + Bx3s[k][j][i]);)
    #endif
   
  /* ------------------------------------------------------
     1b. Apply energy correction step if necessary 
     ------------------------------------------------------ */
 
    #if HAVE_ENERGY && (CT_EN_CORRECTION == YES)
    b2_old = DIM_EXPAND(  UU[k][j][i][BX1]*UU[k][j][i][BX1], 
                        + UU[k][j][i][BX2]*UU[k][j][i][BX2],  
                        + UU[k][j][i][BX3]*UU[k][j][i][BX3]);
    #if PHYSICS == ResRMHD
    b2_old += DIM_EXPAND(  UU[k][j][i][EX1]*UU[k][j][i][EX1], 
                         + UU[k][j][i][EX2]*UU[k][j][i][EX2],  
                         + UU[k][j][i][EX3]*UU[k][j][i][EX3]);
    #endif

    b2_new = DIM_EXPAND(Bx1_ave*Bx1_ave, + Bx2_ave*Bx2_ave, + Bx3_ave*Bx3_ave);
    #if PHYSICS == ResRMHD
    b2_new += DIM_EXPAND(Ex1_ave*Ex1_ave, + Ex2_ave*Ex2_ave, + Ex3_ave*Ex3_ave);
    #endif

    UU[k][j][i][ENG] += 0.5*(b2_new - b2_old);
    #endif 

  /* ------------------------------------------------------
     1c. Replace zone-centered field with staggered average
     ------------------------------------------------------ */

    DIM_EXPAND(UU[k][j][i][BX1] = Bx1_ave;  ,
               UU[k][j][i][BX2] = Bx2_ave;  ,
               UU[k][j][i][BX3] = Bx3_ave; )

    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    DIM_EXPAND(UU[k][j][i][EX1] = Ex1_ave;  ,
               UU[k][j][i][EX2] = Ex2_ave;  ,
               UU[k][j][i][EX3] = Ex3_ave; )
    #endif
  }
}

/* ********************************************************************* */
void CT_AverageStaggeredFields (const Data *d, int apply_ct_en_correction, 
                                RBox *box, Grid *grid)
/*!
 * Average staggered magnetic field components to form a zone-centered 
 * field, e.g., \f$ \av{B_i} = (B_{i+1/2} + B_{i-1/2})/2\f$.
 * In cylindrical coordinates the volume-averaged radial component 
 * is obtained as
 * \f[
 *   \av{B_R} = \frac{1}{\Delta V} \int B(R) R dR 
 *    \qquad\mathrm{where}\qquad
 *     B(R) =   B_{R,i+\HALF} \frac{R - R_{i-\HALF}}{\Delta R} 
 *            + B_{R,i-\HALF} \frac{R_{i+\HALF} - R}{\Delta R}
 * \f] 
 * this yields
 * \f[
 *   \av{B_R} =  \frac{R_{i+\HALF} + 2R_{i-\HALF}}{3(R_{i+\HALF}+R_{i-\HALF})}
 *                B_{R,i-\HALF}
 *             + \frac{2R_{i+\HALF} + R_{i-\HALF}}{3(R_{i+\HALF}+R_{i-\HALF})}
 *                B_{R,i-\HALF}
 * \f]
 * Simlar expressions hold in spherical coordinates. 
 * Check with the following MAPLE script:
 * \code
   restart;
   J     := sin(xi); # J = xi, xi^2, sin(xi)
   Br    := (xi - a)/Delta*B[R] + (b - xi)/Delta*B[L];
   Bav   := int (Br*J, xi=a..b) / int (J, xi=a..b):
   Delta := b-a; 
   Bav   := simplify(Bav);
   cp := coeff(Bav, B[R]);
   cm := coeff(Bav, B[L]);
   
   # Check symmetry propetries (sign must not change)
    
   eval([cm,cp],{a=0,b=1}); 
   eval([cm,cp],{a=-1,b=0});
 * \endcode
 * 
 * The averaging is done inside an arbitrary rectangular box.
 * The box may include boundary cells only during the predictor 
 * step of the CT-CTU algorithm, whereas is useless for RK time stepping.
 *
 * When the CT_EN_CORRECTION flag is enabled, we also redefine the
 * zone total energy using the newly formed cell-centered field, i.e.,
 *
 *  \f[ 
 *   E_i \to E_i - \frac{\mathbf{B}_i^2}{2} + \frac{<\mathbf{B}_i^2>}{2}
 *  \f]
 *
 * \param [in]   d                      pointer to PLUTO data structure
 * \param [in]  apply_ct_en_correction  a flag that enable or disable the 
                                        ct energy correction step when 
                                        CT_EN_CORRECTION == YES
 * \param [in]   box                    pointer to RBox structure 
 * \param [in]   grid                   pointer to Grid structure
 ************************************************************************ */
{
  int i, j, k;
  double b2_old, b2_new;
  double Bx1_ave, Bx2_ave, Bx3_ave;
  double cp, cm, rp, rm;
  double *x1r = grid->xr[IDIR], *x1l = grid->xl[IDIR];
  #if GEOMETRY == SPHERICAL
  double dp, dm, thp, thm;
  double *x2r = grid->xr[JDIR], *x2l = grid->xl[JDIR];
  #endif
  DIM_EXPAND(double ***Bx1s = d->Vs[BX1s];  ,
             double ***Bx2s = d->Vs[BX2s];  ,
             double ***Bx3s = d->Vs[BX3s]; )
  #if PHYSICS == ResRMHD
  double Ex1_ave, Ex2_ave, Ex3_ave;
  DIM_EXPAND(double ***Ex1s = d->Vs[EX1s];   , 
             double ***Ex2s = d->Vs[EX2s];   ,
             double ***Ex3s = d->Vs[EX3s];)
  #endif  
  double ****UU = d->Uc;

/* ---------------------------------------------------------
   0. Allocate memory for cell-centered field (RMHD only)
   --------------------------------------------------------- */

  #if PHYSICS == RMHD
  static double ****Bc;
  if (Bc == NULL) Bc = ARRAY_4D(NX3_TOT, NX2_TOT, NX1_TOT, 3, double);
  #endif

/* ---------------------------------------------------------
   1. Loop over all zones in the given box.
   --------------------------------------------------------- */

// #if HO_FIELD_AVERAGE == 2
// FILE *fp;
// if (g_stepNumber == 0) fp = fopen ("xfrac.dat","w");
// else                   fp = fopen ("xfrac.dat","a");
// double xfrac = 0.0;
// double norm = fabs((box->iend - box->ibeg + 1)*(box->jend - box->jbeg + 1)*1.0);
// #endif

  BOX_LOOP(box,k,j,i){

  /* ------------------------------------------------------
     1a. Obtain zone-centered field from staggered values
     ------------------------------------------------------ */

    #if GEOMETRY == CARTESIAN 

    DIM_EXPAND(Bx1_ave = 0.5*(Bx1s[k][j][i] + Bx1s[k][j][i-1]);  ,
               Bx2_ave = 0.5*(Bx2s[k][j][i] + Bx2s[k][j-1][i]);  ,
               Bx3_ave = 0.5*(Bx3s[k][j][i] + Bx3s[k-1][j][i]); )

    #if HO_FIELD_AVERAGE == 2
//double pm0 = Bx1_ave*Bx1_ave + Bx2_ave*Bx2_ave;

    int active = DIM_EXPAND( (i > 0 && i < NX1_TOT-1) , 
                            *(j > 0 && j < NX2_TOT-1) ,
                            *(k > 0 && k < NX3_TOT-1) );
    if (active){
      int ip = i,    jp = j,    kp = k;
      int im = ip-1, jm = jp-1, km = kp-1;

      double dxByR, dxByL, dyBxR, dyBxL;
      double dyBzR, dyBzL, dzByR, dzByL;
      double dzBxR, dzBxL, dxBzR, dxBzL;

      dxByR = dxByL = dyBxR = dyBxL = 0.0;
      dyBzR = dyBzL = dzByR = dzByL = 0.0;
      dzBxR = dzBxL = dxBzR = dxBzL = 0.0;

      #if DIMENSIONS >= 2
      dxByR = 0.5*(Bx2s[k][jp][i+1] - Bx2s[k][jp][i-1]); 
      dxByL = 0.5*(Bx2s[k][jm][i+1] - Bx2s[k][jm][i-1]); 
      dyBxR = 0.5*(Bx1s[k][j+1][ip] - Bx1s[k][j-1][ip]); 
      dyBxL = 0.5*(Bx1s[k][j+1][im] - Bx1s[k][j-1][im]); 
      #endif
      #if DIMENSIONS == 3
      dzByR = 0.5*(Bx2s[k+1][jp][i] - Bx2s[k-1][jp][i]); 
      dzByL = 0.5*(Bx2s[k+1][jm][i] - Bx2s[k-1][jm][i]); 
      dzBxR = 0.5*(Bx1s[k+1][j][ip] - Bx1s[k-1][j][ip]); 
      dzBxL = 0.5*(Bx1s[k+1][j][im] - Bx1s[k-1][j][im]); 

      dxBzR = 0.5*(Bx3s[kp][j][i+1] - Bx3s[kp][j][i-1]); 
      dxBzL = 0.5*(Bx3s[km][j][i+1] - Bx3s[km][j][i-1]); 
      dyBzR = 0.5*(Bx3s[kp][j+1][i] - Bx3s[kp][j-1][i]); 
      dyBzL = 0.5*(Bx3s[km][j+1][i] - Bx3s[km][j-1][i]); 
      #endif

      DIM_EXPAND(Bx1_ave += (dxByR + dxBzR - dxByL - dxBzL)/12.0;   ,
                 Bx2_ave += (dyBzR + dyBxR - dyBzL - dyBxL)/12.0;   ,
                 Bx3_ave += (dzBxR + dzByR - dzBxL - dzByL)/12.0;)
    }

//double pm1 = Bx1_ave*Bx1_ave + Bx2_ave*Bx2_ave;
//if (pm1 > pm0) xfrac += 1.0/norm;
    #endif

    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    DIM_EXPAND(Ex1_ave = 0.5*(Ex1s[k][j][i] + Ex1s[k][j][i-1]);  ,
               Ex2_ave = 0.5*(Ex2s[k][j][i] + Ex2s[k][j-1][i]);  ,
               Ex3_ave = 0.5*(Ex3s[k][j][i] + Ex3s[k-1][j][i]); )
    #endif

    #elif GEOMETRY == CYLINDRICAL

    rp = x1r[i];
    rm = x1l[i];
    cm = (rp + 2.0*rm)/(3.0*(rp + rm));
    cp = 1.0 - cm; 
    Bx1_ave  = cm*Bx1s[k][j][i-1] + cp*Bx1s[k][j][i];
    Bx2_ave  = 0.5*(Bx2s[k][j-1][i] + Bx2s[k][j][i]);

    #elif GEOMETRY == POLAR

    rp = x1r[i];
    rm = x1l[i];
    cm = (rp + 2.0*rm)/(3.0*(rp + rm));
    cp = 1.0 - cm;

    DIM_EXPAND(Bx1_ave = cm*Bx1s[k][j][i-1] + cp*Bx1s[k][j][i];   ,
               Bx2_ave = 0.5*(Bx2s[k][j-1][i] + Bx2s[k][j][i]);   ,
               Bx3_ave = 0.5*(Bx3s[k-1][j][i] + Bx3s[k][j][i]);)

    #elif GEOMETRY == SPHERICAL
    rp = x1r[i];
    rm = x1l[i];

    cm =  (3.0*rm*rm + 2.0*rm*rp + rp*rp)
         /(4.0*(rm*rm + rm*rp + rp*rp));
    cp = 1.0 - cm;
    
    thp = x2r[j];
    thm = x2l[j];
 
    dm  = (thm - thp)*cos(thm) - sin(thm) + sin(thp);
    dm /= (thm - thp)*(cos(thm) - cos(thp));
    dp  = 1.0 - dm;
    DIM_EXPAND(Bx1_ave  = cm*Bx1s[k][j][i-1] + cp*Bx1s[k][j][i];  ,
               Bx2_ave  = dm*Bx2s[k][j-1][i] + dp*Bx2s[k][j][i];  ,
               Bx3_ave  = 0.5*(Bx3s[k-1][j][i] + Bx3s[k][j][i]);)
    #endif
   
  /* ------------------------------------------------------
     1b. Apply energy correction step if necessary 
     ------------------------------------------------------ */
 
    #if HAVE_ENERGY && (CT_EN_CORRECTION == YES)
    if (apply_ct_en_correction){
      b2_old = DIM_EXPAND(  UU[k][j][i][BX1]*UU[k][j][i][BX1], 
                          + UU[k][j][i][BX2]*UU[k][j][i][BX2],  
                          + UU[k][j][i][BX3]*UU[k][j][i][BX3]);
      #if PHYSICS == ResRMHD
      b2_old += DIM_EXPAND(  UU[k][j][i][EX1]*UU[k][j][i][EX1], 
                           + UU[k][j][i][EX2]*UU[k][j][i][EX2],  
                           + UU[k][j][i][EX3]*UU[k][j][i][EX3]);
      #endif

      b2_new = DIM_EXPAND(Bx1_ave*Bx1_ave, + Bx2_ave*Bx2_ave, + Bx3_ave*Bx3_ave);
      #if PHYSICS == ResRMHD
      b2_new += DIM_EXPAND(Ex1_ave*Ex1_ave, + Ex2_ave*Ex2_ave, + Ex3_ave*Ex3_ave);
      #endif

      UU[k][j][i][ENG] += 0.5*(b2_new - b2_old);
    }
    #endif 

  /* ------------------------------------------------------
     1c. Replace zone-centered field with staggered average
     ------------------------------------------------------ */

    #if PHYSICS == RMHD
    Bc[k][j][i][0] = UU[k][j][i][BX1];   /* Save zone-centered field for later */
    Bc[k][j][i][1] = UU[k][j][i][BX2];
    Bc[k][j][i][2] = UU[k][j][i][BX3];
    #endif

    DIM_EXPAND(UU[k][j][i][BX1] = Bx1_ave;  ,
               UU[k][j][i][BX2] = Bx2_ave;  ,
               UU[k][j][i][BX3] = Bx3_ave; )

    #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
    DIM_EXPAND(UU[k][j][i][EX1] = Ex1_ave;  ,
               UU[k][j][i][EX2] = Ex2_ave;  ,
               UU[k][j][i][EX3] = Ex3_ave; )
    #endif
  }

// #if HO_FIELD_AVERAGE == 2
// if (xfrac < 0.0){
//   printf ("! ERRROR: xfrac = %f < 0\n",xfrac);
//   exit(1);
// }
// if (g_intStage == 1 && xfrac > 1.e-6){
//   printf ("xfrac (pm1 > pm0) = %f\n", xfrac);
//   fprintf (fp,"%f   %f\n",g_time, xfrac);
// }
// fclose(fp);
// #endif

  if (!apply_ct_en_correction) return;

/* ---------------------------------------------------------
   2. Implement the extra correction step as in Marti (2015),
      to complete the relativistic correction of momentum 
      & energy.
   --------------------------------------------------------- */

#if (PHYSICS == RMHD) && (CT_EN_CORRECTION == YES)
  int nv;
  int err = ConsToPrim3D (UU, d->Vc, d->flag, box, grid);
  double vc[NVAR];

  BOX_LOOP(box,k,j,i){
    double *uc = UU[k][j][i];  /* New field */
    double *bc = Bc[k][j][i];  /* Old field */

    NVAR_LOOP(nv) vc[nv] = d->Vc[nv][k][j][i];

    double Bc2 = bc[0]*bc[0]     + bc[1]*bc[1]     + bc[2]*bc[2];
    double Bf2 = vc[BX1]*vc[BX1] + vc[BX2]*vc[BX2] + vc[BX3]*vc[BX3];
    double vBc = DOT_PRODUCT(vc + VX1, bc);
    double vBf = DOT_PRODUCT(vc + VX1, vc + BX1);
    double v2  = DOT_PRODUCT(vc + VX1, vc + VX1);

    uc[MX1] += -(Bc2 - Bf2)*vc[VX1] + bc[0]*vBc - vc[BX1]*vBf;
    uc[MX2] += -(Bc2 - Bf2)*vc[VX2] + bc[1]*vBc - vc[BX2]*vBf;
    uc[MX3] += -(Bc2 - Bf2)*vc[VX3] + bc[2]*vBc - vc[BX3]*vBf;

    uc[ENG] += -0.5*v2*(Bc2 - Bf2) + 0.5*(vBc*vBc - vBf*vBf);
  }
#endif

}
