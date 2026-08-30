#define  PHYSICS                        HD
#define  DIMENSIONS                     2
#define  GEOMETRY                       POLAR
#define  BODY_FORCE                     POTENTIAL
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            8

/* -- physics dependent declarations -- */

#define  DUST_FLUID                     NO
#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  INCLUDE_LES                    NO
#define  THERMAL_CONDUCTION             NO
#define  VISCOSITY                      NO
#define  RADIATION                      YES
#define  ROTATING_FRAME                 YES

/* -- user-defined parameters (labels) -- */

#define  Mstar                          0
#define  Mdisk                          1
#define  Mplanet                        2
#define  Viscosity                      3
#define  GasMu                          4
#define  DUST_OPACITY                   5
#define  DUST_GAS_RATIO                 6
#define  REDUCED_C                      7

/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        VANLEER_LIM
#define  SHOCK_FLATTENING               MULTID
#define  UNIT_LENGTH                    (5.2*CONST_au)
#define  UNIT_DENSITY                   1e-10
#define  UNIT_VELOCITY                  (sqrt(CONST_G*g_inputParam[Mstar]*CONST_Msun/UNIT_LENGTH)/(2.*CONST_PI))

/* [End] user-defined constants (do not change this line) */
