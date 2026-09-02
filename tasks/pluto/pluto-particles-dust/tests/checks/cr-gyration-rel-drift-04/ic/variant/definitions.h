#define  PHYSICS                        MHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  HANCOCK
#define  NTRACER                        0
#define  PARTICLES                      PARTICLES_CR
#define  USER_DEF_PARAMETERS            3

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

#define  PARTICLE_4VEL                  0
#define  ALPHA                          1
#define  VFLUID_X                       2

/* [Beg] user-defined constants (do not change this line) */

#define  CT_EMF_AVERAGE                 CT_CONTACT
#define  PARTICLES_CR_E_MC              1.0
#define  PARTICLES_CR_C                 10.0
#define  PARTICLES_CR_FEEDBACK          NO
#define  PARTICLES_CR_LARMOR_EPS        0.5

/* [End] user-defined constants (do not change this line) */
