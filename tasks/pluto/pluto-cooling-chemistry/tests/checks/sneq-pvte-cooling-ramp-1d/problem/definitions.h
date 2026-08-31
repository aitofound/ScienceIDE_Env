#define  PHYSICS                        HD
#define  DIMENSIONS                     1
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        SNEq
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            4

/* -- physics dependent declarations -- */

#define  DUST_FLUID                     NO
#define  EOS                            PVTE_LAW
#define  ENTROPY_SWITCH                 NO
#define  INCLUDE_LES                    NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  T_LO                           0
#define  T_HI                           1
#define  N_REF                          2
#define  T_FLOOR                        3

/* [Beg] user-defined constants (do not change this line) */

#define  UNIT_DENSITY                   (10.0*CONST_amu)
#define  UNIT_LENGTH                    (1.e17)
#define  UNIT_VELOCITY                  (1.e5)
#define  LIMITER                        MC_LIM
#define  WARNING_MESSAGES               NO

/* [End] user-defined constants (do not change this line) */
