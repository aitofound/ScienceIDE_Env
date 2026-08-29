#define  PHYSICS                        MHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 PARABOLIC
#define  TIME_STEPPING                  CHARACTERISTIC_TRACING
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

#define  MACH                           0
#define  KPAR                           1
#define  MUPAR                          2
#define  PERT_AMPL                      3

/* [Beg] user-defined constants (do not change this line) */

#define  CHAR_LIMITING                  YES
#define  CT_EMF_AVERAGE                 CT_CONTACT
#define  ASSIGN_VECTOR_POTENTIAL        YES
#define  CHTR_REF_STATE                 3

/* [End] user-defined constants (do not change this line) */
