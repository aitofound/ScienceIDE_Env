#define  PHYSICS                        ResRMHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      PARTICLES_CR
#define  USER_DEF_PARAMETERS            3

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  DIVB_CONTROL                   NO
#define  DIVE_CONTROL                   NO

/* -- user-defined parameters (labels) -- */

#define  BMAG_0                         0
#define  BMAG_Z                         1
#define  EMAG_Z                         2

/* [Beg] user-defined constants (do not change this line) */

#define  ASSIGN_VECTOR_POTENTIAL        YES
#define  UPDATE_VECTOR_POTENTIAL        YES
#define  PARTICLES_CR_C                 (1.0)
#define  PARTICLES_CR_E_MC              100.0
#define  PARTICLES_CR_FEEDBACK          NO
#define  PARTICLES_CR_LARMOR_EPS        0.5
#define  PARTICLES_CR_NSUB              -1
#define  PARTICLES_CR_PREDICTOR         NO
#define  PARTICLES_SHAPE                -1
#define  WARNING_MESSAGES               NO

/* [End] user-defined constants (do not change this line) */
