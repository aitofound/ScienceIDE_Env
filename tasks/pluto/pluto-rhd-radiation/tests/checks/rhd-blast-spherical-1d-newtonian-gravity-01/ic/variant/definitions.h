#define  PHYSICS                        RHD
#define  DIMENSIONS                     1
#define  GEOMETRY                       SPHERICAL
#define  BODY_FORCE                     VECTOR
#define  COOLING                        NO
#define  RECONSTRUCTION                 PARABOLIC
#define  TIME_STEPPING                  RK3
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

#define  ARTIFICIAL_VISC                0.0
#define  SHOCK_FLATTENING               NO

/* [End] user-defined constants (do not change this line) */
