/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Header file for the radiation module.

  Set labels, indexes and basic prototyping for the radiation module.

  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 02, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */

#define NRAD_LOOP(n)     for ((n) = ENR;  (n)<=(ENR+3);  (n)++  )
#define NMHD_LOOP(n)     for ((n) = ENR;  (n)--;  ) 

#if RADIATION && (PHYSICS == HD || PHYSICS == MHD)
 #define RADIATION_NR YES
#else
 #define RADIATION_NR NO
#endif

/* *************************************************
     Define convenient and user-friendly 
     pointer labels for geometry setting      
   ************************************************* */

#if GEOMETRY == CYLINDRICAL 

  #define iFRR   FR1
  #define iFRZ   FR2
  #define iFRPHI FR3

#elif GEOMETRY == POLAR 

  #define iFRR   FR1
  #define iFRPHI FR2
  #define iFRZ   FR3

#elif GEOMETRY == SPHERICAL 

  #define iFRR   FR1
  #define iFRTH  FR2
  #define iFRPHI FR3

#endif

/* *************************************************
     Radiation implicit method labels   
   ************************************************* */

 #define RADIATION_FIXEDPOINT_RAD    1
 #define RADIATION_NEWTON_GAS        2
 #define RADIATION_NEWTON_RAD        3
 #define RADIATION_FIXEDPOINT_GAS    4
 #define RADIATION_NEWTON_NR_GAS     5
 #define RADIATION_NEWTON_NR_RAD     6

 #ifndef RADIATION_IMPL // Implicit method
  #if RADIATION_NR
   #define RADIATION_IMPL RADIATION_NEWTON_NR_GAS
  #else
   #define RADIATION_IMPL RADIATION_FIXEDPOINT_RAD
  #endif
 #endif

 #if (RADIATION_IMPL == RADIATION_NEWTON_NR_GAS) || (RADIATION_IMPL == RADIATION_NEWTON_NR_RAD)
  #define RADIATION_IMPLICIT_NR YES
 #else
  #define RADIATION_IMPLICIT_NR NO
 #endif

 #ifndef RADIATION_NEQS // Size of the system solved in the implicit step (RHD/RMHD)
  #define RADIATION_NEQS  4 
 #endif

/* *************************************************
     Constants used by the radiation module
 ************************************************* */

 /*-- Implicit step constants --*/

 #ifndef RADIATION_MAXITER
  #define RADIATION_MAXITER    200
 #endif // Maximum number of iterations allowed for the radiation implicit step

 #ifndef RADIATION_ERROR
  #define RADIATION_ERROR  1e-7  // Error used for convergence of the implicit step
 #endif

 /*-- Minimum energy density set when it goes below 0 --*/  

 #ifndef RADIATION_MIN_ERAD
  #define RADIATION_MIN_ERAD       1e-16
 #endif
 
  /*-- Radiation initial time step (used only if PHYSICS == HD or MHD) --*/  
 #ifndef RADIATION_INITIAL_DT
  #define RADIATION_INITIAL_DT      1e-6
 #endif
 
 /*-- Maximum increment between consecutive time steps
      (used only if PHYSICS == HD or MHD) --*/  
 #ifndef RADIATION_CFL_VAR_MAX
  #define RADIATION_CFL_VAR_MAX      1.1
 #endif
/* *************************************************
     Additional options  
   ************************************************* */

 #ifndef RADIATION_IMEX_SSP2 
  #define RADIATION_IMEX_SSP2 NO
 #endif  /* Apply IMEX-SSP2(2,2,2) scheme [Pareschi, L., & Russo, G. 2005, 
            JSCom, 25, 129]. By default apply IMEX1 in [Melon Fuksman, J. D.,
            & Mignone, A. 2019, ApJS, 242, 20] */

 #ifndef RADIATION_DIFF_LIMITING 
  #define RADIATION_DIFF_LIMITING YES
 #endif  /* Limit the radiation speeds in the diffusion limit following
            Sadowski et al. MNRAS 429, 3533–3550 (2013) */

 #ifndef RADIATION_FULL_CONVERGENCE
  #define RADIATION_FULL_CONVERGENCE NO
 #endif  /* Require convergence of both radiation and internal energy  */

 #ifndef RADIATION_VAR_OPACITIES
  #define RADIATION_VAR_OPACITIES NO
 #endif  /* Define opacity coefficients as functions of the primitive fields.
            A function
                UserDefOpacities (double *v, double *abs_op, double *scat_op)
            must be defined in init.c */  

 #ifndef IRRADIATION
  #define IRRADIATION NO
 #elif IRRADIATION
  #define FIR (FR3 + 1)
  #ifndef IRRADIATION_UPDATE
   #define IRRADIATION_UPDATE NO
  #endif
 #endif /* Store space-dependent irradiation flux divergence.
           Compute at every step if IRRADIATION_UPDATE == YES and only at
           t = 0 otherwise. The irradiation flux must be defined in a
           file named irradiation.c. */
/* ********************************************************************* */
/*! The Rad_data structure contains some information about the
    radiation fields, as well as some auxiliary variables used
    at the implicit radiation step.
   ********************************************************************* */
typedef struct RAD_DATA{
 double dt;           /**< Time step of the implicit step. */
 int pos;          /**< Position where the implicit step is being performed. */
 uint16_t * flag;/**< Array of flags used to tag zones where convertion
                      failed or the HLLC solver needs to be replaced by HLL. */

 double ** pv;        /**< Array of primitive fields. */
 double ** cv;        /**< Array of conserved fields. */

 double * Ttot;       /**< Total (gas+radiation) (0,\mu) components of the
                      stress-energy tensor. */

 double * Rini;       /**< (0,\mu) components of the radiation part of the
                      stress-energy tensor, before the first iteration of the
                      implicit step. */

 double * Rprev;      /**< Auxiliary vector used to compute relative errors.
                      Stores the (0,\mu) components of the radiation part of the
                      stress-energy tensor, computed at the previous iteration. */

 double *u;           /**< Spatial components of the 4-velocity. */
 double u2;           /**< Inner product u_iu^i. */
 double gamma;        /**< Lorentz factor. */
 double gamma2;       /**< Squared Lorentz factor. */

 double exv;          /**< Auxiliary variable used to compute relative errors.
                      Stores the gas pressure of the current iteration if
                      radiation fields are iterated, and the radiation energy
                      density if matter fields are iterated instead. */

 double exv_prev;     /**< Auxiliary variable used to compute relative errors.
                      Stores the gas pressure of the previous iteration if
                      radiation fields are iterated, and the radiation energy
                      density if matter fields are iterated instead. */

 char fill[20];       /**< Fill structure to power of 2 */
} Rad_data;


/* ---- Function prototyping ----  */

int GaussianSolve (double **, double *, double *, int);

void MaxRadSpeed (double **, double *, double *, int, int);
double LimitRadWaveVelocity (double **, double **, Grid *, int);

void LimitRadFlux(double *);
void RadFluxLimFlatten (double *, double *, double *);
void Rad_Speed (double **, double **, Grid *, uint16_t *,
                double *, double *, int, int);

double EddTensor (double *, int , int);
double Blackbody (double);
double GetTemperature (double, double);
int  RadIterToPrim (double *, Rad_data *);
void RadFlux (const State *, int, int);

void RadSourceFunction(double *, double *);
void AddRadSource1(Data_Arr, Data_Arr, RBox *, double);
void AddRadSource2(Data_Arr, Data_Arr, Data_Arr, RBox *, double, double);

void RadFPMatrices (Rad_data *, double *, double **);
void RadNewtonMinusF(Rad_data *, double *, double *);
void RadNewtonJacobian (double *, double *, double **, Rad_data *);

double RadErr (double *, double *, Rad_data *);
void RadImplicitNR(Rad_data *, double **, int, int);
void RadStep (double **, double **, double **, int, int,  uint16_t *, double);
void RadStep3D (Data_Arr, Data_Arr, Data_Arr, uint16_t ***, RBox *, double);

Riemann_Solver *Rad_SetSolver (const char *);

Riemann_Solver Rad_LF_Solver, Rad_HLL_Solver, Rad_HLLC_Solver ; 

void UserDefOpacities (double *, double *, double *);

void RadIrradiationFlux(Data *, Grid *);

int AdvanceRadStep (Data *, double, timeStep *, Grid *);
void UpdateRadStage(Data *, Data_Arr, Data_Arr, double **,
                    double, timeStep *, Grid *);

void RadSubstepping (Data *, double, timeStep *, Grid *);

void AddRelTerms (double *, double *, double );
void RadRightHandSide (const Sweep *, timeStep *,
                       int, int, double, Grid *);
