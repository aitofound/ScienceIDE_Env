#define  PHYSICS                        MHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 MP5
#define  TIME_STEPPING                  RK4
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            4

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

#define  EPS                            0
#define  VEL0                           1
#define  PR0                            2
#define  ALPHA_GLM                      3

/* [Beg] user-defined constants (do not change this line) */

#define  CT_EMF_AVERAGE                 UCT_HLLD
#define  ASSIGN_VECTOR_POTENTIAL        NO
#define  SHOW_TIME_STEPS                YES

/* [End] user-defined constants (do not change this line) */
