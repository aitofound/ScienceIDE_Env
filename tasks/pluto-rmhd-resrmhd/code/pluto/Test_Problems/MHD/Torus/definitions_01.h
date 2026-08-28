#define  PHYSICS                        MHD
#define  DIMENSIONS                     3
#define  GEOMETRY                       POLAR
#define  BODY_FORCE                     VECTOR
#define  COOLING                        NO
#define  RECONSTRUCTION                 PARABOLIC
#define  TIME_STEPPING                  RK3
#define  NTRACER                        1
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

#define  RMIN                           0
#define  RMAX                           1
#define  RHO_CUT                        2
#define  BETA                           3
#define  ETA                            4
#define  SCALE_HEIGHT                   5

/* [Beg] user-defined constants (do not change this line) */

#define  ASSIGN_VECTOR_POTENTIAL        YES
#define  CHECK_DIVB_CONDITION           TRUE
#define  CT_EMF_AVERAGE                 UCT_HLL
#define  INCLUDE_JDIR                   NO
#define  LIMITER                        MC_LIM
#define  USE_DIPOLE                     NO

/* [End] user-defined constants (do not change this line) */
