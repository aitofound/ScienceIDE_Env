#define  PHYSICS                        HD
#define  DIMENSIONS                     1
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 WENO3
#define  TIME_STEPPING                  RK3
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            10

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
#define  T0                             6
#define  V0                             7
#define  REDUCED_C                      8
#define  RAD_C                          9

/* [Beg] user-defined constants (do not change this line) */

#define  WARNING_MESSAGES               YES
#define  RADIATION_DIFF_LIMITING        YES
#define  RADIATION_VAR_OPACITIES        YES

/* [End] user-defined constants (do not change this line) */
