#define  PHYSICS                        RMHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  HANCOCK
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            4

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  RADIATION                      NO
#define  DIVB_CONTROL                   DIV_CLEANING

/* -- user-defined parameters (labels) -- */

#define  SIGMA_TOR                      0
#define  SIGMA_POL                      1
#define  VEL0                           2
#define  MACH                           3

/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        FOURTH_ORDER_LIM

/* [End] user-defined constants (do not change this line) */
