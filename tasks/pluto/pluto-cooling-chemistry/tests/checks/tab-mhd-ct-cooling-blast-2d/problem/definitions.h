#define  PHYSICS                        MHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        TABULATED
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            6

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  DIVB_CONTROL                   CONSTRAINED_TRANSPORT
#define  BACKGROUND_FIELD               NO
#define  AMBIPOLAR_DIFFUSION            NO
#define  RESISTIVITY                    NO
#define  HALL_MHD                       NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  N_AMB                          0
#define  T_AMB                          1
#define  T_IN                           2
#define  R_IN                           3
#define  BETA_AMB                       4
#define  T_FLOOR                        5

/* [Beg] user-defined constants (do not change this line) */

#define  UNIT_DENSITY                   (CONST_mp)
#define  UNIT_LENGTH                    (CONST_pc)
#define  UNIT_VELOCITY                  (1.e5)
#define  LIMITER                        VANLEER_LIM
#define  CT_EMF_AVERAGE                 ARITHMETIC
#define  CT_EN_CORRECTION               YES
#define  ASSIGN_VECTOR_POTENTIAL        YES
#define  WARNING_MESSAGES               NO

/* [End] user-defined constants (do not change this line) */
