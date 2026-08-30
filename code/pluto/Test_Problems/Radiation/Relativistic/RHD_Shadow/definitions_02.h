#define  PHYSICS                        RHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            10

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
#define  RHO1                           6
#define  ER0                            7
#define  ER1                            8
#define  R0                             9

/* [Beg] user-defined constants (do not change this line) */

#define  SHOCK_FLATTENING               YES
#define  LIMITER                        VANLEER_LIM
#define  SHOCK_FLATTENING               YES
#define  WARNING_MESSAGES               YES
#define  RADIATION_DIFF_LIMITING        YES
#define  RADIATION_VAR_OPACITIES        YES

/* [End] user-defined constants (do not change this line) */
