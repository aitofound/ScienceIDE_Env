#define  PHYSICS                        MHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
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
#define  THERMAL_CONDUCTION             RK_LEGENDRE
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  BMAG                           0
#define  THETA                          1
#define  T_IN                           2
#define  T_OUT                          3
#define  RHO_IN                         4
#define  RHO_OUT                        5

/* [Beg] user-defined constants (do not change this line) */

#define  ASSIGN_VECTOR_POTENTIAL        YES
#define  CT_EMF_AVERAGE                 UCT_HLL
#define  LIMITER                        MINMOD_LIM
#define  UNIT_DENSITY                   (1.26*CONST_mH)
#define  UNIT_LENGTH                    (CONST_pc)
#define  UNIT_VELOCITY                  sqrt(g_gamma*CONST_kB*1.e6/(1.26*CONST_mH))

/* [End] user-defined constants (do not change this line) */
