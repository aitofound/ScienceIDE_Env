/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Set labels, indexes and prototypes for the RHD module.

  Contains variable names and prototypes for the RHD module

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Dec 02, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */

/* *********************************************************
 Set flow variable indices.
 Extra vector components, when not needed, point to the
 last element (255) of the array stored by startup.c.
 ********************************************************* */

#define  RHO 0
#define  MX1 1
#define  MX2 2
#define  MX3 3
#if HAVE_ENERGY
  #define ENG  4
  #define PRS  ENG
#endif

#define VX1   MX1
#define VX2   MX2
#define VX3   MX3

#if RADIATION
  #define ENR (4 + HAVE_ENERGY)
  #define FR1 (ENR + 1)
  #define FR2 (ENR + 2)
  #define FR3 (ENR + 3)

  #include "../Radiation/radiation.h"

  #define NFLX (8 + HAVE_ENERGY + IRRADIATION)
#else
  #define NFLX (4 + HAVE_ENERGY)
#endif

/* *************************************************
     Now define more convenient and user-friendly 
     pointer labels for geometry setting      
   ************************************************* */

#if GEOMETRY == CYLINDRICAL 

 #define iVR    VX1
 #define iVZ    VX2
 #define iVPHI  VX3

 #define iMR    MX1
 #define iMZ    MX2
 #define iMPHI  MX3

#endif

#if GEOMETRY == POLAR 

 #define iVR    VX1
 #define iVPHI  VX2
 #define iVZ    VX3

 #define iMR    MX1
 #define iMPHI  MX2
 #define iMZ    MX3

#endif

#if GEOMETRY == SPHERICAL 

 #define iVR    VX1
 #define iVTH   VX2
 #define iVPHI  VX3

 #define iMR    MX1
 #define iMTH   MX2
 #define iMPHI  MX3

#endif

/* -----------------------------------------------------
                 Function prototype
   ----------------------------------------------------- */

int  ConsToPrim   (double **, double **, int, int, uint16_t *);
void ConvertTo4vel (double **, int, int);
void ConvertTo3vel (double **, int, int);

void Flux      (const State *, int, int);
void HLL_Speed (const State *, const State *, double *, double *, int, int);
void MaxSignalSpeed (const State *, double *, double *, int, int);
void PrimEigenvectors(const State *, int, int);
void PrimToCons   (double **, double **, int, int);
void PrimRHS    (double *, double *, double, double, double *);
void PrimSource (const State *, double **, int, int, Grid *);

Riemann_Solver TwoShock_Solver, LF_Solver, HLL_Solver, HLLC_Solver;

int RHD_EnergySolve  (double *, double *);
int RHD_EntropySolve (double *, double *);
int RHD_PressureFix  (double *, double *);

void VelocityLimiter(double *, double *, double *);
