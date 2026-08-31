#define  PHYSICS                        RHD
#define  DIMENSIONS                     1
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            9

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 ALWAYS
#define  RADIATION                      NO

/* -- user-defined parameters (labels) -- */

#define  GAMMA_EOS                      0
#define  DN_L                           1
#define  VX_L                           2
#define  VY_L                           3
#define  PR_L                           4
#define  DN_R                           5
#define  VX_R                           6
#define  VY_R                           7
#define  PR_R                           8

/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        MC_LIM

/* [End] user-defined constants (do not change this line) */
