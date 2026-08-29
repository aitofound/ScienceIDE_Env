/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Set default variable names. 

  Create a correspondence between the index of a variable (e.g., RHO, VX1,
  etc...) and its actual name (e.g., "rho", "vx1", etc...).
  These names are used when writing the output file descriptor (.flt, .dbl,
  etc...) only if the variable has been enabled for writing
  (output->dump_var[nv] = YES/NO).

  \authors A. Mignone (andrea.mignone@unito.it)\n

  \date    June 24, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ****************************************************************** */
void SetDefaultVarNames(Output *output)
/*
 *
 *
 ******************************************************************** */
{
  int nv;

/* ----------------------------------------------
    Physics module file names; 
    these pertain to the physics module ONLY
   ---------------------------------------------- */

  strcpy(output->var_name[RHO], "rho");
  strcpy(output->var_name[VX1], "vx1"); 
  strcpy(output->var_name[VX2], "vx2"); 
  strcpy(output->var_name[VX3], "vx3");
  #if HAVE_ENERGY
  strcpy(output->var_name[PRS], "prs");
  #endif
  
  #if PHYSICS == MHD || PHYSICS == RMHD || PHYSICS == ResRMHD
  strcpy(output->var_name[BX1], "Bx1"); 
  strcpy(output->var_name[BX2], "Bx2"); 
  strcpy(output->var_name[BX3], "Bx3");
  #endif
  
  #if PHYSICS == ResRMHD
  strcpy(output->var_name[EX1], "Ex1");
  strcpy(output->var_name[EX2], "Ex2");
  strcpy(output->var_name[EX3], "Ex3");
  #ifdef CRG
  strcpy(output->var_name[CRG], "crg");
  #endif
  #endif

  /* (staggered field names are set in SetOutput) */

  #ifdef GLM_MHD
  strcpy(output->var_name[PSI_GLM], "psi_glm");
  #ifdef PHI_GLM
  strcpy(output->var_name[PHI_GLM], "phi_glm");
  #endif
  #endif
 
/* ----------------------------------------------
    Dust
   ---------------------------------------------- */

#if DUST_FLUID == YES
  strcpy(output->var_name[RHO_D], "rho_d");
  strcpy(output->var_name[VX1_D], "vx1_d");
  strcpy(output->var_name[VX2_D], "vx2_d");
  strcpy(output->var_name[VX3_D], "vx3_d");
#endif
    
/* ----------------------------------------------
    Radiation
   ---------------------------------------------- */

#if RADIATION
  strcpy(output->var_name[ENR], "enr");
  strcpy(output->var_name[FR1], "fr1");
  strcpy(output->var_name[FR2], "fr2");
  strcpy(output->var_name[FR3], "fr3");
  #if IRRADIATION
  strcpy(output->var_name[FIR], "fir");
  #endif
#endif

/* ----------------------------------------------
                   Tracers 
   ---------------------------------------------- */

  NTRACER_LOOP(nv) sprintf (output->var_name[nv],"tr%d",nv - TRC + 1);

  #if ENTROPY_SWITCH
  strcpy(output->var_name[ENTR], "entropy");
  #endif

/* ----------------------------------------------
               Cooling vars
   ---------------------------------------------- */

#if COOLING == MINEq
  {
    static char *ion_name[] = {"X_HI", "X_HeI", "X_HeII" 
                        C_EXPAND("X_CI","X_CII", "X_CIII", "X_CIV", "X_CV")
                        N_EXPAND("X_NI","X_NII", "X_NIII", "X_NIV", "X_NV")
                        O_EXPAND("X_OI","X_OII", "X_OIII", "X_OIV", "X_OV")
                       Ne_EXPAND("X_NeI","X_NeII", "X_NeIII", "X_NeIV", "X_NeV")
                        S_EXPAND("X_SI","X_SII", "X_SIII", "X_SIV", "X_SV")
                       Fe_EXPAND("X_FeI", "X_FeII", "X_FeIII")};

    NIONS_LOOP(nv) strcpy(output->var_name[nv], ion_name[nv-NFLX]);
  }
#elif COOLING == SNEq

   strcpy(output->var_name[X_HI], "X_HI");

#elif COOLING == H2_COOL
  {
    static char *molnames[] = {"X_HI", "X_H2", "X_HII"};
    NIONS_LOOP(nv) strcpy(output->var_name[nv], molnames[nv-NFLX]);
  } 

#elif COOLING == KROME
  /* kromenames are reaction network dependent and are defined in cooling.h */
    NIONS_LOOP(nv) strcpy(output->var_name[nv], kromenames[nv-NFLX]);
#endif

}
