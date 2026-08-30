#define  PHYSICS                        HD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        TABULATED
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  HANCOCK
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            4

/* -- physics dependent declarations -- */

#define  DUST_FLUID                     NO
#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  INCLUDE_LES                    NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  N0                             0
#define  T_INIT                         1
#define  AMP                            2
#define  T_FLOOR                        3

/* [Beg] user-defined constants (do not change this line) */

#define  UNIT_DENSITY                   (CONST_mp)
#define  UNIT_LENGTH                    (CONST_pc)
#define  UNIT_VELOCITY                  (1.e5)
#define  LIMITER                        VANLEER_LIM
#define  WARNING_MESSAGES               NO

/* [End] user-defined constants (do not change this line) */
