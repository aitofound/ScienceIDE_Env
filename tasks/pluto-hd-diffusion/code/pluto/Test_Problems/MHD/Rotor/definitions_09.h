#define  PHYSICS                        MHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       POLAR
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            1

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  DIVB_CONTROL                   CONSTRAINED_TRANSPORT
#define  BACKGROUND_FIELD               YES
#define  AMBIPOLAR_DIFFUSION            NO
#define  RESISTIVITY                    NO
#define  HALL_MHD                       NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  VEL_0                          0

/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        VANLEER_LIM
#define  CT_EMF_AVERAGE                 ARITHMETIC
#define  ASSIGN_VECTOR_POTENTIAL        YES

/* [End] user-defined constants (do not change this line) */
