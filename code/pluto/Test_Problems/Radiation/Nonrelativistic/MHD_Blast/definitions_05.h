#define  PHYSICS                        MHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            12

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
#define  RADIATION                      YES
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  P_IN                           0
#define  P_OUT                          1
#define  BMAG                           2
#define  THETA                          3
#define  PHI                            4
#define  RADIUS                         5
#define  GAMMA                          6
#define  COEF_ABSORPTION                7
#define  COEF_SCATTERING                8
#define  CONST_RAD                      9
#define  CONST_IDEALGAS                 10
#define  REDUCED_C                      11

/* [Beg] user-defined constants (do not change this line) */

#define  CHAR_LIMITING                  YES
#define  LIMITER                        VANLEER_LIM
#define  CT_EMF_AVERAGE                 ARITHMETIC
#define  CT_EN_CORRECTION               YES
#define  ASSIGN_VECTOR_POTENTIAL        YES
#define  WARNING_MESSAGES               YES
#define  RADIATION_IMPL                 RADIATION_NEWTON_RAD
#define  RADIATION_INITIAL_DT           1e-7
#define  RADIATION_FULL_CONVERGENCE     YES

/* [End] user-defined constants (do not change this line) */
