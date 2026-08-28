/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Convert between primitive and conservative variables locally.

  The PrimToConsLoc() converts an array of primitive quantities to 
  an array of conservative variables for the MHD equations.
  
  The ConsToPrimLoc() converts an array of conservative quantities to 
  an array of primitive quantities.

  Both of them perform conversion locally for 1D arrays.
  During the conversion, pressure is normally recovered from total 
  energy unless zone has been tagged with FLAG_ENTROPY.
  In this case we recover pressure from conserved entropy:
  
      if (FLAG_ENTROPY is TRUE)  --> p = p(S)
      else                       --> p = p(E)
  
  \author A. Mignone (andrea.mignone@unito.it)
          V. Berta   (vittoria.berta@edu.unito.it)
  \date   Aug 26, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void PrimToConsLoc (double *v, double *u)
/*!
 * Convert primitive variables in conservative variables. 
 *
 * \param [in]  *v array of local primitive variables
 * \param [out] *u array of local conservative variables
 *
 *********************************************************************** */
{
  int  nv, status;
  double rhoe, kinb2, T, gmm1;

#if EOS == IDEAL
  gmm1 = g_gamma - 1.0;
#endif
  
  u[RHO] = v[RHO];
        
  u[MX1] = v[RHO]*v[VX1];
  u[MX2] = v[RHO]*v[VX2];
  u[MX3] = v[RHO]*v[VX3];

  u[BX1] = v[BX1];
  u[BX2] = v[BX2];
  u[BX3] = v[BX3];

  kinb2  = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
  kinb2  = v[RHO]*kinb2  + v[BX1]*v[BX1] + v[BX2]*v[BX2] + v[BX3]*v[BX3];
  kinb2 *= 0.5;

  #if EOS == IDEAL
  u[ENG] = kinb2 + v[PRS]/gmm1;
  #elif EOS == PVTE_LAW
  status = GetPV_Temperature(v, &T);
  if (status != 0){
    T      = T_CUT_RHOE;
    v[PRS] = Pressure(v, T);
  }
  rhoe   = InternalEnergy(v, T);
  u[ENG] = rhoe + kinb2;

  if (u[ENG] != u[ENG]){
    printLog("! PrimToConsLoc(): KE:%12.6e uRHO : %12.6e, m2 : %12.6e \n",
                rhoe, v[RHO], u[ENG]);
    QUIT_PLUTO(1);
  }
  #endif
    
  #ifdef GLM_MHD
  u[PSI_GLM] = v[PSI_GLM]; 
  #endif
  #if NSCL > 0 
  NSCL_LOOP(nv) u[nv] = v[RHO]*v[nv];
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
 * Convert from conservative to primitive variables.
 *
 * \param [in]     *u     array of local conservative variables
 * \param [out]    *v     array of local primitive variables
 * \param [in,out] flag   array of flags tagging, in input, zones
 *                        where entropy must be used to recover pressure
 *                        and, on output, zones where conversion was
 *                        not successful.
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
  double b2, m2, kinb2, rhog1;

#if EOS == IDEAL
  gmm1 = g_gamma - 1.0;
#endif

  m2 = u[MX1]*u[MX1] + u[MX2]*u[MX2] + u[MX3]*u[MX3];
  b2 = u[BX1]*u[BX1] + u[BX2]*u[BX2] + u[BX3]*u[BX3];

  /* -- Check density positivity -- */
  
  if (u[RHO] < 0.0) {
    printLog("! ConsToPrimLoc(): negative density (%8.2e), ", u[RHO]);
    u[RHO] = g_smallDensity;
    *flag |= FLAG_NEGATIVE_DENSITY;
    *flag |= FLAG_CONS2PRIM_FAIL;
    ifail  = 1;
  }

  /* -- Compute density, velocity, mag. field and scalars -- */

  v[RHO] = rho = u[RHO];
  tau    = 1.0/u[RHO];
  v[VX1] = u[MX1]*tau;
  v[VX2] = u[MX2]*tau;
  v[VX3] = u[MX3]*tau;

  v[BX1] = u[BX1];
  v[BX2] = u[BX2];
  v[BX3] = u[BX3];

  kinb2 = 0.5*(m2*tau + b2);    

  /* -- Check energy positivity -- */

  #if HAVE_ENERGY
  if (u[ENG] < 0.0) {
    WARNING(
      printLog("! ConsToPrimLoc(): negative energy (%8.2e), ", u[ENG]);
    )
    u[ENG]  = g_smallPressure/gmm1 + kinb2;
    *flag  |= FLAG_NEGATIVE_ENERGY;
    *flag  |= FLAG_CONS2PRIM_FAIL;
    ifail   = 1;
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
             printLog("! ConsToPrimLoc(): negative p(S) (%8.2e, %8.2e), ",
                       v[PRS], u[ENTR]);
         )
      v[PRS] = g_smallPressure;
      *flag |= FLAG_CONS2PRIM_FAIL;
      ifail  = 1;
    }
    u[ENG] = v[PRS]/gmm1 + kinb2; /* -- recompute energy -- */
  }
  #endif  /* ENTROPY_SWITCH */

  if (use_energy){
    v[PRS] = gmm1*(u[ENG] - kinb2);
    if (v[PRS] < 0.0){
      WARNING(
        printLog("! ConsToPrimLoc(): negative p(E) (%8.2e), ", v[PRS]);
      )
      v[PRS] = g_smallPressure;
      u[ENG] = v[PRS]/gmm1 + kinb2; /* -- recompute energy -- */
      *flag |= FLAG_NEGATIVE_PRESSURE;
      *flag |= FLAG_CONS2PRIM_FAIL;
      ifail  = 1;
    }
    #if ENTROPY_SWITCH
    u[ENTR] = v[PRS]/pow(rho, gmm1);  /* -- Recompute entropy -- */
    #endif
  }

  #if NSCL > 0
  NSCL_LOOP(nv) v[nv] = u[nv]*tau;
  #endif    

#elif EOS == ISOTHERMAL

  #if NSCL > 0
  NSCL_LOOP(nv) v[nv] = u[nv]*tau;
  #endif    
 
#elif EOS == PVTE_LAW

  /* -- Convert scalars here since EoS may need ion fractions -- */

  #if NSCL > 0                       
  NSCL_LOOP(nv) v[nv] = u[nv]*tau;
  #endif    

  if (u[ENG] != u[ENG]){
    printLog ("! ConsToPrimLoc(): NaN found\n");
    ShowState(u, 0);
    QUIT_PLUTO(1);
  }
  rhoe  = u[ENG] - kinb2; 

  err = GetEV_Temperature (rhoe, v, &T);
  if (err){  /* If something went wrong while retrieving the  */
             /* temperature, we floor \c T to \c T_CUT_RHOE,  */
             /* recompute internal and total energies.        */
    T = T_CUT_RHOE;
    WARNING(  
      printLog ("! ConsToPrimLoc(): rhoe < 0 or T < T_CUT_RHOE; ");
    )
    rhoe   = InternalEnergy(v, T);
    u[ENG] = rhoe + kinb2; /* -- redefine total energy -- */
    *flag |= FLAG_CONS2PRIM_FAIL;
    ifail  = 1;
  }
  v[PRS] = Pressure(v, T);

#endif  /* EOS  */

  #ifdef GLM_MHD
  v[PSI_GLM] = u[PSI_GLM]; 
  #endif

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

  return ifail;
}
