#define  PHYSICS                        HD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LimO3
#define  TIME_STEPPING                  RK3
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            12

/* -- physics dependent declarations -- */

#define  DUST_FLUID                     NO
#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  INCLUDE_LES                    NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  RADIATION                      YES
#define  ROTATING_FRAME                 NO

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
#define  REDUCED_C                      10
#define  RAD_C                          11

/* [Beg] user-defined constants (do not change this line) */

#define  SHOCK_FLATTENING               MULTID
#define  WARNING_MESSAGES               YES
#define  RADIATION_DIFF_LIMITING        YES
#define  RADIATION_VAR_OPACITIES        YES
#define  RADIATION_IMPL                 RADIATION_NEWTON_NR_RAD
#define  RADIATION_FULL_CONVERGENCE     YES

/* [End] user-defined constants (do not change this line) */
