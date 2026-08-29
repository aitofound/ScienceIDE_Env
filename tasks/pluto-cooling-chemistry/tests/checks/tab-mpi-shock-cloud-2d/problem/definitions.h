#define  PHYSICS                        HD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        TABULATED
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        1
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            6

/* -- physics dependent declarations -- */

#define  DUST_FLUID                     NO
#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  INCLUDE_LES                    NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  MACH                           0
#define  N_AMB                          1
#define  T_AMB                          2
#define  CHI                            3
#define  R_CLOUD                        4
#define  T_FLOOR                        5

/* [Beg] user-defined constants (do not change this line) */

#define  UNIT_DENSITY                   (CONST_mp)
#define  UNIT_LENGTH                    (CONST_pc)
#define  UNIT_VELOCITY                  (1.e5)
#define  LIMITER                        MC_LIM
#define  WARNING_MESSAGES               NO

/* [End] user-defined constants (do not change this line) */
