/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Convert between primitive and conservative variables locally.

  The PrimToConsLoc() converts an array of primitive quantities to 
  an array of conservative variables for the HD equations.
  
  The ConsToPrimLoc() converts an array of conservative quantities to 
  an array of primitive quantities.

  Both of them perform conversion locally for 1D arrays.
  During the conversion, pressure is normally recovered from total 
  energy unless zone has been tagged with FLAG_ENTROPY.
  In this case we recover pressure from conserved entropy:
  
      if (FLAG_ENTROPY is TRUE)  --> p = p(S)
      else                       --> p = p(E)
  
  \author A. Mignone (andrea.mignone@unito.it)
          B. Vaidya
          V. Berta   (vittoria.berta@edu.unito.it)
  \date   Sep 07, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void PrimToConsLoc (double *v, double *u)
/*!
 * Convert from primitive to conservative variables locally. 
 *
 * \param [in]  *v array of local primitive variables
 * \param [out] *u array of local conservative variables
 *
 *********************************************************************** */
{
  int nv, status;
  double rho, rhoe, T, gmm1;

  #if EOS == IDEAL
  gmm1 = g_gamma - 1.0;
  #endif

  u[RHO] = rho = v[RHO];
  u[MX1] = rho * v[VX1];
  u[MX2] = rho * v[VX2];
  u[MX3] = rho * v[VX3];

  #if EOS == IDEAL
  u[ENG] = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
  u[ENG] = 0.5*rho*u[ENG] + v[PRS]/gmm1;
  #elif EOS == PVTE_LAW
  status = GetPV_Temperature(v, &T);
  if (status != 0){
    T          = T_CUT_RHOE;
    v[PRS] = Pressure(v, T);
  }
  rhoe = InternalEnergy(v, T);

  u[ENG] = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
  u[ENG] = 0.5*rho*u[ENG] + rhoe;

  if (u[ENG] != u[ENG]){
    printLog ("! PrimToConsLoc(): E = NaN, rhoe = %8.3e, T = %8.3e\n",rhoe, T);
    QUIT_PLUTO(1);
  }
  #endif   /* EOS == PVTE_LAW */
    
  #if DUST_FLUID == YES
  u[RHO_D] = v[RHO_D];
  u[MX1_D] = v[RHO_D]*v[VX1_D];
  u[MX2_D] = v[RHO_D]*v[VX2_D];
  u[MX3_D] = v[RHO_D]*v[VX3_D];
  #endif

  #if NSCL > 0 
  NSCL_LOOP(nv) u[nv] = rho*v[nv];
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
 * \param [in]     *u      array of conservative variables
 * \param [out]    *v      array of primitive variables
 * \param [in,out] flag    array of flags tagging, in input, zones
 *                         where entropy must be used to recover pressure
 *                         and, on output, zones where conversion was
 *                         not successful.
 * 
 * \return Return 0 if local conversion was successful.
 *         Return 1 if one or more zones could not be converted correctly
 *         and either pressure, density or energy took non-physical values. 
 *
 *********************************************************************** */
{
  int  nv, err, ifail = 0;
  int  use_entropy, use_energy = 1;
  double tau, rho, gmm1, rhoe, T;
  double kin, m2, rhog1;

  #if EOS == IDEAL
  gmm1 = g_gamma - 1.0;
  #endif
   
  m2 = u[MX1]*u[MX1] + u[MX2]*u[MX2] + u[MX3]*u[MX3];
    
/* -- Check density positivity -- */
  
  #if !SCALAR_ADVECTION
  if (u[RHO] < 0.0) {
    printLog("! ConsToPrimLoc: rho < 0 (%8.2e), ", u[RHO]);
    u[RHO]  = g_smallDensity;
    *flag  |= FLAG_CONS2PRIM_FAIL;
    *flag  |= FLAG_NEGATIVE_DENSITY;
    ifail   = 1;
  }
  #endif

/* -- Compute density, velocity and scalars -- */

  v[RHO] = rho = u[RHO];
  tau    = 1.0/u[RHO];
  v[VX1] = u[MX1]*tau;
  v[VX2] = u[MX2]*tau;
  v[VX3] = u[MX3]*tau;

  kin = 0.5*m2/u[RHO];

/* -- Check energy positivity -- */

  #if HAVE_ENERGY
  if (u[ENG] < 0.0) {
    WARNING(
      printLog ("! ConsToPrimLoc: E < 0 (%8.2e), ", u[ENG]);
    )
    u[ENG] = g_smallPressure/gmm1 + kin;
    *flag |= FLAG_CONS2PRIM_FAIL;
//    ifail  = 1;
  }
  #endif

/* -- Compute pressure from total energy or entropy -- */

  #if EOS == IDEAL   
  #if ENTROPY_SWITCH
  use_entropy = (*flag & FLAG_ENTROPY);
  use_energy  = !use_entropy;
  if (use_entropy){
    rhog1 = pow(rho, gmm1);
    v[PRS] = u[ENTR]*rhog1; 
    if (v[PRS] < 0.0){
      WARNING(
        printLog ("! ConsToPrimLoc(): p(S) < 0 (%8.2e, %8.2e), ", v[PRS], u[ENTR]);
      )
      v[PRS] = g_smallPressure;
      *flag |= FLAG_CONS2PRIM_FAIL;
      ifail  = 1;
    }
    u[ENG] = v[PRS]/gmm1 + kin; /* -- redefine energy -- */
  }
  #endif  /* ENTROPY_SWITCH  */

  if (use_energy){
    v[PRS] = gmm1*(u[ENG] - kin);
    if (v[PRS] < 0.0){
      WARNING(
        printLog ("! ConsToPrimLoc(): p(E) < 0 (%8.2e), ", v[PRS]);
      )
      v[PRS] = g_smallPressure;
      u[ENG] = v[PRS]/gmm1 + kin; /* -- redefine energy -- */
      *flag |= FLAG_CONS2PRIM_FAIL;
//      ifail  = 1;
    }
    #if ENTROPY_SWITCH
    u[ENTR] = v[PRS]/pow(rho,gmm1);
    #endif
  }
      
  #elif EOS == PVTE_LAW

/* -- Convert scalars here since EoS may need ion fractions -- */

  #if NSCL > 0                       
  NSCL_LOOP(nv) v[nv] = u[nv]*tau;
  #endif    

  if (u[ENG] != u[ENG]){
    printLog ("! ConsToPrimLoc(): NaN found in energy\n");
    Show(u,0);
    QUIT_PLUTO(1);
  }
  rhoe = u[ENG] - kin; 

  err = GetEV_Temperature (rhoe, v, &T);
  if (err){  /* If something went wrong while retrieving the  */
             /* temperature, we floor \c T to \c T_CUT_RHOE,  */
             /* recompute internal and total energies.        */
    T = T_CUT_RHOE;
    WARNING(  
      printLog ("! ConsToPrimLoc: rhoe < 0 or T < T_CUT_RHOE; "); 
    )
    rhoe   = InternalEnergy(v, T);
    u[ENG] = rhoe + kin; /* -- redefine total energy -- */
    *flag |= FLAG_CONS2PRIM_FAIL;
//    ifail  = 1;
  }
  v[PRS] = Pressure(v, T);
  #endif  /* EOS == PVTE_LAW */

 /* --------------------------
    Dust
  -------------------------- */

  #if DUST_FLUID == YES
  u[RHO_D] = MAX(u[RHO_D], 1.e-50);
  v[RHO_D] = u[RHO_D];
  v[VX1_D] = u[MX1_D]/v[RHO_D];
  v[VX2_D] = u[MX2_D]/v[RHO_D];
  v[VX3_D] = u[MX3_D]/v[RHO_D];
  #endif

  #if NSCL > 0                    
  NSCL_LOOP(nv) v[nv] = u[nv]*tau;
  #endif    
  return ifail;

  #if RADIATION
  if (u[ENR] < 0.0) {
    WARNING(
      printLog("! ConsToPrimLoc(): Erad < 0 (%8.2e), ", u[ENR]);
    )			
    u[ENR]   = RADIATION_MIN_ERAD;
    *flag |= FLAG_CONS2PRIM_FAIL;
  }

  v[ENR] = u[ENR];
  v[FR1] = u[FR1];
  v[FR2] = u[FR2];
  v[FR3] = u[FR3];
  #if IRRADIATION
  v[FIR] = u[FIR];
  #endif
  #endif
}
