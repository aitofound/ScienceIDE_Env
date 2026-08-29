/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Compute right hand side for Tabulated cooling

  \authors A. Mignone (andrea.mignone@unito.it)\n
           M. Sormani\n

 \b References

  \date   Apr 09, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

extern double gCooling_x1, gCooling_x2, gCooling_x3;

/* ***************************************************************** */
void Radiat (double *v, double *rhs)
/*!
 *   Provide r.h.s. for tabulated cooling.
 * 
 ******************************************************************* */
{
  int    klo, khi, kmid;
  static int ntab;
  double  mu, T, Tmid, scrh, dT, prs;
  static double *L_tab, *T_tab, E_cost;
  double nH = UNIT_DENSITY/CONST_amu*H_MASS_FRAC/CONST_AH*v[RHO];
  double ne = nH*(1.0 + 0.5*CONST_AZ*FRAC_Z);
  
/* -------------------------------------------
        Read tabulated cooling function
   ------------------------------------------- */

  if (T_tab == NULL){
    FILE *fcool;
    printLog (" > Reading table from disk...\n");
    fcool = fopen("cooltable.dat","r");
    if (fcool == NULL){
      printLog ("! Radiat: cooltable.dat could not be found.\n");
      QUIT_PLUTO(1);
    }
    L_tab = ARRAY_1D(20000, double);
    T_tab = ARRAY_1D(20000, double);

    ntab = 0;
    while (fscanf(fcool, "%lf  %lf\n", T_tab + ntab, 
                                       L_tab + ntab)!=EOF) {
      ntab++;
    }
    E_cost = UNIT_LENGTH/UNIT_DENSITY/pow(UNIT_VELOCITY, 3.0);
    fclose(fcool);
  }

/* ---------------------------------------------
            Get pressure and temperature 
   --------------------------------------------- */

  prs = v[RHOE]*(g_gamma-1.0);
  if (prs < 0.0) {
    prs     = g_smallPressure;
    v[RHOE] = prs/(g_gamma - 1.0);
  }

  mu  = MeanMolecularWeight(v);
  T   = prs/v[RHO]*KELVIN*mu;

  if (T != T){
    printf ("! Radiat(): Nan found: rho = %12.6e, prs = %12.6e\n",v[RHO], prs);
    QUIT_PLUTO(1);
  }

/*
  if (T < g_minCoolingTemp) { 
    rhs[RHOE] = 0.0;
    return;
  }
*/
/* ----------------------------------------------
        Table lookup by binary search  
   ---------------------------------------------- */

  klo = 0;
  khi = ntab - 1;

  if (T > T_tab[khi] || T < T_tab[klo]){
    rhs[RHOE] = 0.0;
    return;
    QUIT_PLUTO(1);
  }

  while (klo != (khi - 1)){
    kmid = (klo + khi)/2;
    Tmid = T_tab[kmid];
    if (T <= Tmid){
      khi = kmid;
    }else if (T > Tmid){
      klo = kmid;
    }
  }

/* -----------------------------------------------
    Compute r.h.s
   ----------------------------------------------- */

  dT        = T_tab[khi] - T_tab[klo];
  scrh      = L_tab[klo]*(T_tab[khi] - T)/dT + L_tab[khi]*(T - T_tab[klo])/dT;
  rhs[RHOE] = -ne*nH*scrh*E_cost;
  
/* ----------------------------------------------
    Temperature cutoff
   ---------------------------------------------- */

  rhs[RHOE] *= 1.0 - 1.0/cosh( pow( T/g_minCoolingTemp, 12));

}
