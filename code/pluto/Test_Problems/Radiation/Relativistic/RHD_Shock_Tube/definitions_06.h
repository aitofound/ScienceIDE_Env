#define  PHYSICS                        RHD
#define  DIMENSIONS                     1
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            13

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  RADIATION                      YES

/* -- user-defined parameters (labels) -- */

#define  GAMMA_EOS                      0
#define  COEF_ABSORPTION                1
#define  COEF_SCATTERING                2
#define  CONST_RAD                      3
#define  CONST_IDEALGAS                 4
#define  RHOL                           5
#define  RHOR                           6
#define  PL                             7
#define  PR                             8
#define  UL                             9
#define  UR                             10
#define  ERL                            11
#define  ERR                            12

/* [Beg] user-defined constants (do not change this line) */

#define  SHOCK_FLATTENING               YES
#define  LIMITER                        MC_LIM
#define  RADIATION_IMPL                 RADIATION_NEWTON_GAS
#define  RADIATION_FULL_CONVERGENCE     YES
#define  RADIATION_NEQS                 2
#define  WARNING_MESSAGES               YES

/* [End] user-defined constants (do not change this line) */
