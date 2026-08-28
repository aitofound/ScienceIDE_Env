/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief FARGO-MHD module header file.

  Contains contains basic definitions and declarations used by the
  FARGO-MHD module.

  \authors A. Mignone    (andrea.mignone@unito.it)\n
           G. Muscianisi (g.muscianisi@cineca.it)
  \date    Aug 21, 2019
  \todo   
*/
/* ///////////////////////////////////////////////////////////////////// */

/**<  Set the order of interpolation during the linear transport step.
      Either 2 or 3. Default is 3. */
#ifndef FARGO_ORDER
 #define FARGO_ORDER    3  
#endif
        
/*! Set how often (in number of steps) the total azimuthal 
    velocity should be averaged.                           */
#ifndef FARGO_NSTEP_AVERAGE
  #ifdef SHEARINGBOX
    #define FARGO_NSTEP_AVERAGE  -1   /* Never average with ShearinBox model */
  #else 
    #define FARGO_NSTEP_AVERAGE  10   /* Default is 10 */
  #endif  
#endif

/*! Used to write total/residual velocity */
#ifndef FARGO_OUTPUT_VTOT
  #define FARGO_OUTPUT_VTOT    NO
#endif

#define  FARGO_W  (NVAR+4)  /* 1,2,3 already taken by vec. potential */

/* ----------------------------------------------------------------
    Note: when both FARGO and SHEARINGBOX modules are used, *ALL* 
          source terms are accounted for by the body_force function 
   ---------------------------------------------------------------- */

/* -- SBEG and SEND are the initial and final 
      indices in the direction of the orbital velocity -- */

#if GEOMETRY != SPHERICAL
 #define SDIR   JDIR  /* Orbital direction. */
 #define SBEG   JBEG  /* Starting index in the orbital dir. */
 #define SEND   JEND  /* Final index in the orbital dir. */
 #define NS     NX2
 #define NS_TOT NX2_TOT
#else
 #define SDIR   KDIR
 #define SBEG   KBEG
 #define SEND   KEND
 #define NS     NX3
 #define NS_TOT NX3_TOT
#endif

/* ------------------------------------------------------------------
             Handy macros for dimensional loops
   ------------------------------------------------------------------ */

#define SDOM_LOOP(s) for ((s) = SBEG; (s) <= SEND; (s)++)
  
#if GEOMETRY == SPHERICAL
  #define FARGO_ARRAY_INDEX(A,s,k,j,i)  A[s][j][i]
#else
  #define FARGO_ARRAY_INDEX(A,s,k,j,i)  A[k][s][i]
#endif 

#if (GEOMETRY == CARTESIAN) || (GEOMETRY == POLAR)
  #define FARGO_VELOCITY3D(w, i,j,k)  w[k][i]
#elif GEOMETRY == SPHERICAL
  #define FARGO_VELOCITY3D(w, i,j,k)  w[j][i]
#endif

/* ----------------------------------------------------------------- 
     Prevent compilation when Fargo and AMR are given simultaneously
   ----------------------------------------------------------------- */

#ifdef CH_SPACEDIM
 #error FARGO and AMR are not compatible
#endif
 
void     FARGO_AverageVelocity(const Data *, Grid *);
void     FARGO_ComputeResidualVelocity(const Data *, double ***, Grid *);
void     FARGO_ComputeTotalVelocity   (const Data *, double ***, Grid *);
void     FARGO_Initialize(void);
void     FARGO_RestartOld(const Data *, Grid *);
void     FARGO_Restart(const Data *, char *, int, int, Grid *);
void     FARGO_ShiftSolution(Data_Arr, Data_Arr, Grid *);
#if PARTICLES
void     FARGO_ShiftParticles(Data *, Grid *, double);
#endif
void     FARGO_Source(Data_Arr, Data_Arr, double, Grid *);
double **FARGO_Velocity(void);
void     FARGO_Write(const Data *, char *, int, Grid *);


