#define  PHYSICS                        HD
#define  DIMENSIONS                     2
#define  GEOMETRY                       POLAR
#define  BODY_FORCE                     VECTOR
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            3

/* -- physics dependent declarations -- */

#define  DUST_FLUID                     NO
#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  INCLUDE_LES                    NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 YES

/* -- user-defined parameters (labels) -- */

#define  KPAR                           0
#define  HSCALE                         1
#define  MACH                           2

/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        MC_LIM

/* [End] user-defined constants (do not change this line) */
