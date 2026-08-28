#define  PHYSICS                        RHD
#define  DIMENSIONS                     3
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     VECTOR
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  RK2
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            3

/* -- physics dependent declarations -- */

#define  EOS                            TAUB
#define  ENTROPY_SWITCH                 NO
#define  RADIATION                      NO

/* -- user-defined parameters (labels) -- */

#define  ENRG0                          0
#define  DNST0                          1
#define  MASS                           2

/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        VANLEER_LIM
#define  WARNING_MESSAGES               NO

/* [End] user-defined constants (do not change this line) */
