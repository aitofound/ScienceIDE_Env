#define  PHYSICS                        HD
#define  DIMENSIONS                     1
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  HANCOCK
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            0

/* -- physics dependent declarations -- */

#define  DUST_FLUID                     NO
#define  EOS                            PVTE_LAW
#define  ENTROPY_SWITCH                 NO
#define  INCLUDE_LES                    NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */


/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        VANLEER_LIM
#define  UNIT_DENSITY                   (1.e5*CONST_amu)
#define  UNIT_LENGTH                    (167.0*CONST_au)
#define  UNIT_VELOCITY                  5.25e5

/* [End] user-defined constants (do not change this line) */
