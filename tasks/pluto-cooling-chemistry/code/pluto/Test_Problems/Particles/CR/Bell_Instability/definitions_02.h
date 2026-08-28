#define  PHYSICS                        MHD
#define  DIMENSIONS                     1
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  HANCOCK
#define  NTRACER                        0
#define  PARTICLES                      PARTICLES_CR
#define  USER_DEF_PARAMETERS            2

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  DIVB_CONTROL                   NO
#define  BACKGROUND_FIELD               NO
#define  AMBIPOLAR_DIFFUSION            NO
#define  RESISTIVITY                    NO
#define  HALL_MHD                       NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  ROTATING_FRAME                 NO

/* -- user-defined parameters (labels) -- */

#define  EPSILON                        0
#define  BPERP_AMPL                     1

/* [Beg] user-defined constants (do not change this line) */

#define  CT_EMF_AVERAGE                 ARITHMETIC
#define  LIMITER                        MC_LIM
#define  PARTICLES_CR_C                 1.e6
#define  PARTICLES_CR_E_MC              (1.e-6*2.0*CONST_PI)
#define  PARTICLES_CR_E_MC_GAS          1.e6
#define  PARTICLES_CR_NSUB              4
#define  PARTICLES_DEPOSIT              INTEGER
#define  PRIMITIVE_HANCOCK              FALSE
#define  VTK_TIME_INFO                  TRUE

/* [End] user-defined constants (do not change this line) */
