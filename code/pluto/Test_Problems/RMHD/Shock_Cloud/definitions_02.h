#define  PHYSICS                        RMHD
#define  DIMENSIONS                     2
#define  GEOMETRY                       CARTESIAN
#define  BODY_FORCE                     NO
#define  COOLING                        NO
#define  RECONSTRUCTION                 LINEAR
#define  TIME_STEPPING                  HANCOCK
#define  NTRACER                        0
#define  PARTICLES                      NO
#define  USER_DEF_PARAMETERS            0

/* -- physics dependent declarations -- */

#define  EOS                            TAUB
#define  ENTROPY_SWITCH                 CHOMBO_REGRID
#define  RADIATION                      NO
#define  DIVB_CONTROL                   NO

/* -- user-defined parameters (labels) -- */


/* [Beg] user-defined constants (do not change this line) */

#define  SHOCK_FLATTENING               MULTID
#define  LIMITER                        VANLEER_LIM
#define  CHOMBO_REF_VAR                 RHO

/* [End] user-defined constants (do not change this line) */
