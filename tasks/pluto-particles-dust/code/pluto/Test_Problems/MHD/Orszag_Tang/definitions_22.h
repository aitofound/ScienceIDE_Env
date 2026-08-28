#define  PHYSICS                        MHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 WENOZ
#define  TIME_STEPPING                  RK4
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            0

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


/* [Beg] user-defined constants (do not change this line) */

#define  ASSIGN_VECTOR_POTENTIAL        YES
#define  CHECK_DIVB_CONDITION           NO
#define  CT_EMF_AVERAGE                 UCT_HLLD
#define  CT_EN_CORRECTION               NO
#define  LIMITER                        MC_LIM
#define  HO_STARTUP_NGAUSS              4
#define  HO_LAP_LIMITER                 NO
#define  QUIT_ON_FIX                    YES
#define  ROTATE                         -1
#define  SHOCK_FLATTENING               NO
#define  WARNING_MESSAGES               YES

/* [End] user-defined constants (do not change this line) */
