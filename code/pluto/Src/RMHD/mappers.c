/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Convert between primitive and conservative variables.

  The PrimToCons() converts an array of primitive quantities to 
  an array of conservative variables for the RMHD equations.
  
  The ConsToPrim() converts an array of conservative quantities to 
  an array of primitive quantities.
  During the conversion, pressure is normally recovered from total 
  energy using the algorithm outlined in
  - "Equation of sweep in relativistic magnetohydrodynamics: variable versus
     constant adiabatic index"\n
     Mignone \& Mc Kinney, MNRAS (2007) 378, 1118.

  However, if the zone has been tagged with FLAG_ENTROPY, primitive
  variables are recovered by using the conserved entropy rather than
  total energy.
  
  In other words:
  
      if (FLAG_ENTROPY is TRUE)  --> p = p(S)
      else                       --> p = p(E)
  
  If the inversion scheme fails and p cannot be obtained the
  RMHD_PressureFix() function is used.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Jul 03, 2021
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#define  ENERGY_SOLVE   1
#define  ENTROPY_SOLVE  2
#define  PRESSURE_FIX   3
/* ********************************************************************* */
void PrimToCons (double **uprim, double **ucons, int beg, int end)
/*!
 * Convert primitive variables to conservative variables. 
 *
 * \param [in]  uprim array of primitive variables
 * \param [out] ucons array of conservative variables
 * \param [in]  beg   starting index of computation
 * \param [in]  end   final index of computation
 *
 *********************************************************************** */
{
  int   i, nv;
  double  vel2, usq, vB, Bmag2;
  double  vx1, vx2, vx3;
  double  g, g2, wt;
  double  *u, *v;
  static double *h;
  #if EOS == IDEAL
  double gmmr = g_gamma/(g_gamma - 1.0);
  #endif

  if (h == NULL) h = ARRAY_1D(NMAX_POINT, double);

  Enthalpy(uprim, h, beg, end);

  for (i = beg; i <= end; i++) {

    v = uprim[i];
    u = ucons[i];

    vx1  = v[VX1];
    vx2  = v[VX2];
    vx3  = v[VX3];
    vel2 = vx1*vx1 + vx2*vx2 + vx3*vx3;
    g2 = 1.0/(1.0 - vel2);
    g  = sqrt(g2);

    vB    = vx1*v[BX1]    + vx2*v[BX2]    + vx3*v[BX3];
    Bmag2 = v[BX1]*v[BX1] + v[BX2]*v[BX2] + v[BX3]*v[BX3];
    wt    = v[RHO]*h[i]*g2 + Bmag2;

  /* -------------------------------------------------------
       Convert from primitive (v) to conservative (u)   
     ------------------------------------------------------- */

    u[RHO] = g*v[RHO];
    u[MX1] = wt*vx1 - vB*v[BX1];
    u[MX2] = wt*vx2 - vB*v[BX2];
    u[MX3] = wt*vx3 - vB*v[BX3];

    u[BX1] = v[BX1];
    u[BX2] = v[BX2];
    u[BX3] = v[BX3];

    #if RMHD_REDUCED_ENERGY == YES
    #if EOS == IDEAL
    u[ENG]  =   v[PRS]*(g2*gmmr - 1.0) + u[RHO]*g2*vel2/(g + 1.0)
              + 0.5*(Bmag2*(1.0 + vel2) - vB*vB);
    #elif EOS == TAUB
    wt     =   v[PRS]/v[RHO];
    u[ENG] =   v[PRS]*(g2*2.5 - 1.0) 
             + u[RHO]*g2*(2.25*wt*wt + vel2)/(g*sqrt(1.0 + 2.25*wt*wt) + 1.0)
             + 0.5*(Bmag2*(1.0 + vel2) - vB*vB);
    #endif
    #else
    u[ENG]  = v[RHO]*h[i]*g2 - v[PRS] + 0.5*(Bmag2*(1.0 + vel2) - vB*vB);
    #endif  /* RMHD_REDUCED_ENERGY */

    #if NSCL > 0
    NSCL_LOOP(nv) u[nv] = u[RHO]*v[nv];
    #endif    
    
    #ifdef GLM_MHD
    u[PSI_GLM] = v[PSI_GLM]; 
    #endif

	   #if RADIATION
	   u[ENR] = v[ENR];
 	 	 u[FR1] = v[FR1]; 
     u[FR2] = v[FR2]; 
     u[FR3] = v[FR3];
     #if IRRADIATION
     u[FIR] = v[FIR];
     #endif
	   #endif
  }
}

/* ********************************************************************* */
int ConsToPrim (double **ucons, double **uprim, int beg, int end, uint16_t *flag)
/*!
 * Convert from conservative to primitive variables.
 *
 * \param [in]  ucons      array of conservative variables
 * \param [out] uprim      array of primitive variables
 * \param [in]  beg        starting index of computation
 * \param [in]  end        final index of computation
 * \param [in,out] flag    array of flags tagging, in input, zones
 *                         where entropy must be used to recover pressure
 *                         and, on output, zones where conversion was
 *                         not successful.
 * 
 * \return Return 0 if conversion was successful in all zones in the 
 *         range [ibeg,iend].
 *         Return 1 if one or more zones could not be converted correctly
 *         and either pressure, density or energy took non-physical values. 
 *
 *********************************************************************** */
{
  int    i, nv, err;
  int    ifail = 0;
  int    method;
  double *u, *v, scrh, w_1;

  for (i = beg; i <= end; i++) {

    u = ucons[i];
    v = uprim[i];

    v[BX1] = u[BX1];
    v[BX2] = u[BX2];
    v[BX3] = u[BX3];

  /* --------------------------------------------
     1. Set the default solution method
     -------------------------------------------- */

    method = ENERGY_SOLVE;
    #if ENTROPY_SWITCH
    if (flag[i] & FLAG_ENTROPY) method = ENTROPY_SOLVE;
    #endif

  /* --------------------------------------------
     2. Check density and energy positivity 
     -------------------------------------------- */
  
    if (u[RHO] < 0.0) {
      printLog("! ConsToPrim(): negative density (%8.2e), ", u[RHO]);
      Where (i, NULL);
      u[RHO]   = g_smallDensity;
      flag[i] |= FLAG_NEGATIVE_DENSITY;
      flag[i] |= FLAG_CONS2PRIM_FAIL;
    }

    if (u[ENG] < 0.0) {
      WARNING(
        printLog("! ConsToPrim(): negative energy (%8.2e), ", u[ENG]);
        Where (i, NULL);
      )
      flag[i] |= FLAG_NEGATIVE_ENERGY;
      method = PRESSURE_FIX;
    }

    #if RADIATION
    if (u[ENR] < 0.0) {
      WARNING(
        print("! ConsToPrim: Erad < 0 (%8.2e), ", u[ENR]);
        Where (i, NULL);
      )			
      u[ENR]   = RADIATION_MIN_ERAD;
      flag[i] |= FLAG_CONS2PRIM_FAIL;
    }
    #endif

  /* --------------------------------------------
     3a. Attempt to recover primitive var. from
         energy equation
     -------------------------------------------- */

    if (method == ENERGY_SOLVE){
      err = RMHD_EnergySolve(u, v);

      if (err){   /* Try pressure fix */
        WARNING(
          printLog ("! ConsToPrim(): RMHD_EnergySolve() failed,");
          printLog (" err code = %d;", err);
          Where(i,NULL);
        )
        method = PRESSURE_FIX;
      }
    }

  /* --------------------------------------------
     3b. Attempt to recover primitive var. from
         entropy equation.
     -------------------------------------------- */

    #if ENTROPY_SWITCH
    if (method == ENTROPY_SOLVE) {
      err = RMHD_EntropySolve(u, v);
      if (err) {
        WARNING(
          printLog ("! ConsToPrim(): RMHD_EntropySolve() failed,");
          printLog (" err code = %d;", err);
          Where (i, NULL);
        )
        flag[i] |= FLAG_NEGATIVE_PRESSURE;
        method = PRESSURE_FIX;
      }
    } 
    #endif
 
  /* --------------------------------------------
     3c. Apply pressure fix
     -------------------------------------------- */

    if (method == PRESSURE_FIX){
      err = RMHD_PressureFix(u, v);
      if (err){
        printLog ("! ConsToPrim(): RMHD_PressureFix() failed,");
        printLog (" err code = %d;", err);
        Where(i,NULL);
        flag[i] |= FLAG_PRESSURE_FIX_FAIL;
        flag[i] |= FLAG_CONS2PRIM_FAIL;
      }
    }

  /* --------------------------------------------
     4. Check velocity is subluminal (this check
        may not be needed actually...)
     -------------------------------------------- */

    scrh = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
    if (scrh >= 1.0){
      printLog ("! ConsToPrim(): v^2 = %f > 1  (p = %12.6e); ifail = %d",
                 scrh, v[PRS], ifail);
      Where (i, NULL);
      printLog ("!               Flag_Entropy = %d\n", (flag[i] & FLAG_ENTROPY));
      flag[i] |= FLAG_CONS2PRIM_FAIL;
    }

  /* --------------------------------------------
     5. Complete conversion
     -------------------------------------------- */

    #if NSCL > 0 
    NSCL_LOOP(nv) v[nv] = u[nv]/u[RHO];
    #endif

    #ifdef GLM_MHD
    v[PSI_GLM] = u[PSI_GLM]; 
    #endif

    #if RADIATION
    v[ENR] = u[ENR];
    v[FR1] = u[FR1]; 
    v[FR2] = u[FR2]; 
    v[FR3] = u[FR3];
    #if IRRADIATION
    v[FIR] = u[FIR];
    #endif
    #endif

    if (flag[i] & FLAG_CONS2PRIM_FAIL) ifail = 1;
  }
  return ifail;
}
#undef  ENERGY_SOLVE 
#undef  ENTROPY_SOLVE
#undef  PRESSURE_FIX
