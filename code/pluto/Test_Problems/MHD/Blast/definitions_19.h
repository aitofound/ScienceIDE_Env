#define  PHYSICS                        MHD
#define  DIMENSIONS                     3
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 WENOZ
#define  TIME_STEPPING                  RK4
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            7

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

#define  P_IN                           0
#define  P_OUT                          1
#define  BMAG                           2
#define  THETA                          3
#define  PHI                            4
#define  RADIUS                         5
#define  GAMMA                          6

/* [Beg] user-defined constants (do not change this line) */

#define  ASSIGN_VECTOR_POTENTIAL        NO
#define  CHAR_LIMITING                  NO
#define  CT_EMF_AVERAGE                 UCT_HLL
#define  CT_EN_CORRECTION               NO
#define  HO_LAP_LIMITER                 HO_RDER_LIM
#define  HO_LAP_LIMITER_BUF             0
#define  HO_LAP_LIMITER_EPS             0.2
#define  HO_ORDER_REDUCTION             LINEAR
#define  HO_PRIMITIVE_BC                YES
#define  HO_STARTUP_NGAUSS              4
#define  LIMITER                        MINMOD_LIM
#define  QUIT_ON_FIX                    NO
#define  SHOCK_FLATTENING               NO
#define  WARNING_MESSAGES               YES

/* [End] user-defined constants (do not change this line) */
