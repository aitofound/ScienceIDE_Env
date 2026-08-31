#define  PHYSICS                        RHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 PARABOLIC
#define  TIME_STEPPING                  RK3
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            1

/* -- physics dependent declarations -- */

#define  EOS                            TAUB
#define  ENTROPY_SWITCH                 SELECTIVE
#define  RADIATION                      NO

/* -- user-defined parameters (labels) -- */

#define  SCRH                           0

/* [Beg] user-defined constants (do not change this line) */

#define  LIMITER                        VANALBADA_LIM
#define  WARNING_MESSAGES               YES

/* [End] user-defined constants (do not change this line) */
