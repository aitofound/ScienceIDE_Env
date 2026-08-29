#define  PHYSICS                        HD
#define  DIMENSIONS                     3
#define  GEOMETRY                       POLAR
#define  BODY_FORCE                     POTENTIAL
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            1

/* -- physics dependent declarations -- */

#define  DUST_FLUID                     NO
#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  INCLUDE_LES                    NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  ALPHA                          0

/* [Beg] user-defined constants (do not change this line) */

#define  INCLUDE_JDIR                   NO
#define  LIMITER                        MC_LIM

/* [End] user-defined constants (do not change this line) */
