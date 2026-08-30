#define  PHYSICS                        RMHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 WENOZ
#define  TIME_STEPPING                  RK3
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            8

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  RADIATION                      NO
#define  DIVB_CONTROL                   CONSTRAINED_TRANSPORT

/* -- user-defined parameters (labels) -- */

#define  RHO_IN                         0
#define  PRS_IN                         1
#define  RHO_OUT                        2
#define  PRS_OUT                        3
#define  BMAG                           4
#define  THETA                          5
#define  PHI                            6
#define  RADIUS                         7

/* [Beg] user-defined constants (do not change this line) */

#define  CT_EMF_AVERAGE                 CT_CONTACT
#define  CT_ENERGY_CORRECTION           NO
#define  CHECK_DIVB_CONDITION           NO
#define  ENABLE_HLLEM                   YES
#define  LIMITER                        VANLEER_LIM

/* [End] user-defined constants (do not change this line) */
