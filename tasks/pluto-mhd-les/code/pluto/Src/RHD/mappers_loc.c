/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Convert between primitive and conservative variables.

  The PrimToCons() converts an array of primitive quantities to 
  an array of conservative variables for the RHD equations.
  
  The ConsToPrim() converts an array of conservative quantities to 
  an array of primitive quantities.
  During the conversion, pressure is normally recovered from total 
  energy unless zone has been tagged with FLAG_ENTROPY.
  In this case we recover pressure from conserved entropy:
  
      if (FLAG_ENTROPY is TRUE)  --> p = p(S)
      else                       --> p = p(E)
  
  \author A. Mignone (andrea.mignone@unito.it)
          V. Berta   (vittoria.berta@edu.unito.it)
  \date   AUg 26, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#define  ENERGY_SOLVE   1
#define  ENTROPY_SOLVE  2
#define  PRESSURE_FIX   3
/* ********************************************************************* */
void PrimToConsLoc  (double *v, double *u)
/*!
 * Convert primitive variables to conservative variables. 
 *
 * \param [in]  *v  array of local primitive variables
 * \param [out] *u  array of local conservative variables
 *
 *********************************************************************** */
{
  int  nv;
  double  rhoh_g2, scrh, g;
  double  theta;
  double  beta_fix = 0.9999;
  static double  h;
     
  /* -- Enthalpy -- */
  
  theta = v[PRS]/v[RHO]; 

  #if EOS == IDEAL
  double gmmr = g_gamma/(g_gamma - 1.0);
  h = 1.0 + gmmr*theta;
  #elif EOS == TAUB
  h = 2.5*theta + sqrt(2.25*theta*theta + 1.0);
  #endif 

  g = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];

  if (g >= 1.0){
    WARNING( 
      printLog ("! u^2 > 1 (%f) in PrimToConsLoc \n", scrh);
    )

    g = beta_fix/sqrt(g);
    v[VX1] *= g;
    v[VX2] *= g;
    v[VX3] *= g;

    g = beta_fix*beta_fix;
  }
  g    = 1.0/(1.0 - g);
  scrh = rhoh_g2 = v[RHO]*h*g;
  g    = sqrt(g);

  u[RHO] = v[RHO]*g;
  u[MX1] = scrh*v[VX1];
  u[MX2] = scrh*v[VX2];
  u[MX3] = scrh*v[VX3];
  u[ENG] = rhoh_g2 - v[PRS];

  #if NSCL > 0    
  NSCL_LOOP(nv) u[nv] = v[nv]*u[RHO];
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

/* ********************************************************************* */
int ConsToPrimLoc (double *u, double *v, uint16_t *flag)
/*!
 * Convert from conservative to primitive variables locally.
 *
 * \param [in]     *u     array of local conservative variables
 * \param [out]    *v     array of local primitive variables
 * \param [in,out] flag   array of flags tagging, in input, zones
 *                        where entropy must be used to recover pressure
 *                        and, on output, zones where conversion was
 *                        not successful.
 * 
 * \return Return 0 if conversion was successful.
 *         Return 1 if one or more zones could not be converted correctly
 *         and either pressure, density or energy took non-physical values. 
 *
 *********************************************************************** */
{
  int    nv, err;
  int    ifail = 0;
  char   method;

  /* --------------------------------------------
     1. Set the default solution method
     -------------------------------------------- */

  method = ENERGY_SOLVE;
  #if ENTROPY_SWITCH
  if (*flag & FLAG_ENTROPY) method = ENTROPY_SOLVE;
  #endif

  /* --------------------------------------------
     2. Check density and rad energy positivity 
     -------------------------------------------- */
  
  if (u[RHO] < 0.0) {
    printLog("! ConsToPrim(): negative density (%8.2e), ", u[RHO]);
    u[RHO]   = g_smallDensity;
    *flag |= FLAG_CONS2PRIM_FAIL;
    *flag |= FLAG_NEGATIVE_DENSITY;
    ifail    = 1;
  }
    
  #if RADIATION
  if (u[ENR] < 0.0) {
    WARNING(
      print("! ConsToPrim(): Erad < 0 (%8.2e), ", u[ENR]);
    )
    u[ENR] = RADIATION_MIN_ERAD;
    *flag |= FLAG_CONS2PRIM_FAIL;
//     ifail    = 1;
  }
  #endif

  /* --------------------------------------------
     3a. Obtain pressure by inverting 
         the entropy equation.
     -------------------------------------------- */

  #if ENTROPY_SWITCH
  if (method == ENTROPY_SOLVE) {
    err = RHD_EntropySolve(u, v);
    if (err) {
      WARNING(
        printLog ("! ConsToPrim() <- RHD_EntropySolve() failed, ");
        printLog ("err code = %d. Trying pressure fix... \n", err);
      )
      method = PRESSURE_FIX;
    }
  }  /* end if entropy_solve */
  #endif
 
  /* --------------------------------------------
     3b. Obtain pressure by inverting 
         the energy equation.
     -------------------------------------------- */

  if (method == ENERGY_SOLVE){
    err = RHD_EnergySolve(u, v);
    if (err){
      WARNING(
        printLog ("! ConsToPrim(): RHD_EnergySolve() failed,");
        printLog (" err code = %d; ", err);
      )
      method = PRESSURE_FIX;
    }
  } /* end if energy_solve */

  /* --------------------------------------------
     3c. Apply pressure fix if necessary
     -------------------------------------------- */

  if (method == PRESSURE_FIX){
    err = RHD_PressureFix(u, v);
    if (err){
      printLog ("! ConsToPrim() <- RHD_PressureFix() failed, ");
      printLog ("err code = %d; \n", err);
      ShowState (u,0);          
      QUIT_PLUTO(1);  /* No more options  (!!) */
    }
      
    /* -- Pressure has changed, redefine total energy & entropy -- */
      
    *flag |= FLAG_CONS2PRIM_FAIL;
//      ifail    = 1;
  } /* end if pressure_fix */

  /* --------------------------------------------
     4. Complete conversion 
     -------------------------------------------- */

  #if NSCL > 0     
  NSCL_LOOP(nv)  v[nv] = u[nv]/u[RHO];
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

  return ifail;
}
#undef  ENERGY_SOLVE 
#undef  ENTROPY_SOLVE
#undef  PRESSURE_FIX
