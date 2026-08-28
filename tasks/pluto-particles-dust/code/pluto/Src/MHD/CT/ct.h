/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Header file for Constrained-Transport (CT) module.

  Provides macros, function prototypes and structure definitions for 
  the constrained transport (CT) MHD module to control the divergence-free 
  condition.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Sep 16, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#define STAGGERED_MHD

/* ---- set labels for CT_EMF_AVERAGE ---- */

#define ARITHMETIC        1
#define UCT0              2
#define UCT_CONTACT       3
#define CT_CONTACT        3
#define UCT_HLL           4
#define CT_FLUX           5
#define UCT_GFORCE        6
#define UCT_HLLD          7
#define CT_MAXWELL        8

/* ---- staggered component labels ---- */

#define BX1s  0
#if INCLUDE_JDIR
  #define BX2s  1
#endif
#if INCLUDE_KDIR
  #define BX3s  2
#endif

#if (PHYSICS == ResRMHD)
  #define EX1s  (DIMENSIONS)
  #if INCLUDE_JDIR
    #define EX2s  (EX1s + 1)
  #endif
  #if INCLUDE_KDIR
    #define EX3s  (EX2s + 1)
  #endif
#endif

/* ---- backward compatibility ---- */

#define BXs BX1s
#define BYs BX2s
#define BZs BX3s

#define FACE_EMF   11
#define EDGE_EMF   12

/* *********************************************************************
   Macros
   ********************************************************************* */

/*! \name CT_ASSIGN macro.
    The following macros is used to avoid too many pre-processor 
    directives when the same instruction has to replicated for 
    the electric field in ResRMHD. 
    When both B and E use constrained transport, the macro evaluates
    both argument. Otherwise only the 1st one is used.
*/
/**@{ */
#if PHYSICS == ResRMHD && DIVE_CONTROL == CONSTRAINED_TRANSPORT
  #define CT_ASSIGN(a, b)    a   b
#else  
  #define CT_ASSIGN(a, b)    a
#endif

/* *********************************************************************
   Default values
   ********************************************************************* */

#ifndef CT_EMF_AVERAGE
  #if PHYSICS == ResRMHD
    #define  CT_EMF_AVERAGE         CT_MAXWELL
  #else
    #define  CT_EMF_AVERAGE         UCT_HLL
  #endif  
#endif

/* *********************************************************************
   Energy correction switch
   ********************************************************************* */
#ifndef  CT_EN_CORRECTION
 #define  CT_EN_CORRECTION          NO
#endif

/*       Now define more convenient and user-friendly 
         pointer labels for geometry setting      */

#if GEOMETRY == CYLINDRICAL 
 #define iBRs    BX1s
 #define iBZs    BX2s
 #define iBPHIs  BX3s
#endif

#if GEOMETRY == POLAR 
 #define iBRs    BX1s
 #define iBPHIs  BX2s
 #define iBZs    BX3s
#endif

#if GEOMETRY == SPHERICAL 
 #define iBRs    BX1s
 #define iBTHs   BX2s
 #define iBPHIs  BX3s
#endif

/* ***********************************************************
    \cond REPEAT_FUNCTION_DOCUMENTATION_IN_HEADER_FILES
    Function prototyping
   *********************************************************** */

void CT_Allocate (EMF *);   
void CT_AverageStaggeredFieldsOld (double ****Vs, double ****UU, RBox *, Grid *);
void CT_AverageStaggeredFields (const Data *, int, RBox *,  Grid *);

void CT_CheckDivB (double ***b[], Grid *);
void CT_ComputeEMF (const Data *, Grid *);
void CT_ComputeCenterEMF(const Data *);
void CT_EMF_ArithmeticAverage (const EMF *, const double);
void CT_EMF_IntegrateToCorner (const Data *, const EMF *, Grid *);
void CT_EMF_Riemann2D (const Data *, const EMF *, Grid *);
void CT_EMF_Flux(const Data *d, const EMF *emf, Grid *grid);
void CT_Flux(const Sweep *, int, int, Grid *);
void CT_GetEMF (const Data *, Grid *);
#if PHYSICS == ResRMHD
void CT_ComputeCharge(const Data *, RBox *, Grid *);
void CT_InterfaceCurrent(Data *, Grid *);
void FillElectricField (const Data *, int, Grid *);
void CT_IMEXImplicitUpdate(Data *, Data_Arr, double, Grid *);
void CT_MaxwellSolver(const Data *, const EMF *, Grid *);
void CT_ResistiveUpdate(const Data *, double, Grid *);
#endif
void CT_ResistiveEMF (const Data *, int, Grid *);
void CT_StoreUpwindEMF    (const Sweep *, EMF *, int, int, Grid *);
void CT_Update(const Data *, Data_Arr, double, Grid *);

void FillMagneticField (const Data *, int, Grid *); 

/* \endcond */
 
