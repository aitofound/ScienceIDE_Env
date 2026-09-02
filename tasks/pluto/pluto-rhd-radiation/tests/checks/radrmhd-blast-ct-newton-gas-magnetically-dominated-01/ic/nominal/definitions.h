#define  PHYSICS                        RMHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        1
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            13

/* -- physics dependent declarations -- */

#define  EOS                            IDEAL
#define  ENTROPY_SWITCH                 NO
#define  RADIATION                      YES
#define  DIVB_CONTROL                   CONSTRAINED_TRANSPORT

/* -- user-defined parameters (labels) -- */

#define  GAMMA_EOS                      0
#define  COEF_ABSORPTION                1
#define  COEF_SCATTERING                2
#define  CONST_RAD                      3
#define  CONST_IDEALGAS                 4
#define  RHO0                           5
#define  RHO1                           6
#define  ER0                            7
#define  ER1                            8
#define  PR0                            9
#define  PR1                            10
#define  B_0                            11
#define  R0                             12

/* [Beg] user-defined constants (do not change this line) */

#define  SHOCK_FLATTENING               YES
#define  LIMITER                        MC_LIM
#define  CT_EMF_AVERAGE                 ARITHMETIC
#define  ASSIGN_VECTOR_POTENTIAL        YES
#define  RADIATION_NEQS                 3
#define  RADIATION_DIFF_LIMITING        YES
#define  RADIATION_ERROR                1e-3
#define  RADIATION_IMPL                 RADIATION_NEWTON_GAS

/* [End] user-defined constants (do not change this line) */
