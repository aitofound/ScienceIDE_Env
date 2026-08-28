#define  PHYSICS                        RHD
#define  DIMENSIONS                     1
#define  GEOMETRY                       SPHERICAL
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            8

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
#define  RHO0                           5
#define  T0                             6
#define  W0                             7

/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        MC_LIM
#define  RADIATION_NEQS                 3

/* [End] user-defined constants (do not change this line) */
