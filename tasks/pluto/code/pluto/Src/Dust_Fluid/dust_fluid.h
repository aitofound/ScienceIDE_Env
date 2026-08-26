/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Set labels, indexes and prototypes for the dust module.

  Contains variable names and prototypes for the dust module

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Feb 25, 2019
  
  TODO:
  
   - Implicit implementation of drag coupling term.
   - More careful characteristic analysis (Miniati 2010)
*/
/* ///////////////////////////////////////////////////////////////////// */

#define NDUST_FLUID 4

#define  RHO_D   (NFLX + NIONS + NTRACER + (ENTROPY_SWITCH != 0))
#define  MX1_D   (RHO_D + 1)
#define  MX2_D   (RHO_D + 2)
#define  MX3_D   (RHO_D + 3)
#define  VX1_D   MX1_D
#define  VX2_D   MX2_D
#define  VX3_D   MX3_D

#if GEOMETRY == CYLINDRICAL
  #define iMPHI_D  MX3_D
#elif GEOMETRY == POLAR
  #define iMPHI_D  MX2_D
#elif GEOMETRY == SPHERICAL
  #define iMPHI_D  MX3_D
#endif


#define NDUST_FLUID_BEG   (RHO_D)
#define NDUST_FLUID_END   (NDUST_FLUID_BEG + NDUST_FLUID - 1)
#define NDUST_FLUID_LOOP(n)    for ((n) = NDUST_FLUID_BEG;  (n) <= NDUST_FLUID_END; (n)++)

void   DustFluid_Solver   (const Sweep *, int, int, double *, Grid *);
void   DustFluid_DragForce(const Sweep *, int, int, double, Grid *);
double DustFluid_StoppingTime(double *, double, double, double);    /* User-supplied */