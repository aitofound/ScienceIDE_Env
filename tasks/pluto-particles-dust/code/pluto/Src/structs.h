/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief PLUTO header file for structure declarations.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   March 23, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */

typedef struct cmdLine_{
  char restart;           /**< Enable restart from double precision binary files */
  char h5restart;         /**< Enable restart from hdf5 files   */
  char prestart;          /**< Enable / disable partice restart */
  char makegrid;          /**< Write grid only                  */
  char write;             /**< Set it 1 or 0 to enable or disable writing. */
  char parallel_dim[3];   /**< Enable/disable domain decomp. in a given direction */
  int nrestart;           /**< The file number used for restart */
  int maxsteps;           /**< The maximum number of steps (unless negative) */
  int jet;                /**< Follow jet evolution in a given direction */
  int nproc[3];           /**< User supplied number of processors */
  int xres;               /**< Change the resolution via command line */
  char fill[26];               /* useless, it makes the struct a power of 2 */
} cmdLine;

/* ********************************************************************* */
/*! The EMF structure is used to pull together all the information
    necessary to build / use the electromotive force used to update
    the staggered components of magnetic field.
   ********************************************************************* */
typedef struct ElectroMotiveForce{

/*! \name Face-centered electric field components.
    Three-dimensional arrays storing the emf components computed
    at cell faces during the dimensional sweeps.
*/
/**@{ */
  double ***exj; /**< Ex flux available at y-faces (j+1/2); */
  double ***exk; /**< Ex flux available at z-faces (k+1/2); */
  double ***eyi; /**< Ey flux available at x-faces (i+1/2); */
  double ***eyk; /**< Ey flux available at z-faces (k+1/2); */
  double ***ezi; /**< Ez flux available at x-faces (i+1/2); */
  double ***ezj; /**< Ez flux available at y-faces (j+1/2); */

  double ***exj_dff; /**< Ex flux available at y-faces (j+1/2); */
  double ***exk_dff; /**< Ex flux available at z-faces (k+1/2); */
  double ***eyi_dff; /**< Ey flux available at x-faces (i+1/2); */
  double ***eyk_dff; /**< Ey flux available at z-faces (k+1/2); */
  double ***ezi_dff; /**< Ez flux available at x-faces (i+1/2); */
  double ***ezj_dff; /**< Ez flux available at y-faces (j+1/2); */
/**@} */

#if PHYSICS == ResRMHD
/*! \name Face-centered magnetic field components.
    Three-dimensional arrays storing the emf components computed
    at cell faces during the dimensional sweeps.
*/
/**@{ */
  double ***Bxj; /**< Bx flux available at y-faces (j+1/2); */
  double ***Bxk; /**< Bx flux available at z-faces (k+1/2); */
  double ***Byi; /**< By flux available at x-faces (i+1/2); */
  double ***Byk; /**< By flux available at z-faces (k+1/2); */
  double ***Bzi; /**< Bz flux available at x-faces (i+1/2); */
  double ***Bzj; /**< Bz flux available at y-faces (j+1/2); */
  double ***Frho_i;
  double ***Frho_j;
  double ***Frho_k;
/**@} */
#endif

  #if CONS_ENG_CORRECTION != NO
  double ***Ez_pnt;
  double ***Bxe;
  double ***Bye;

  double ***Fm_x;
  double ***Fm_y;
  #endif

  signed char ***svx, ***svy, ***svz;

/*! \name Range of existence */
/**@{ */
  int  ibeg, jbeg, kbeg;
  int  iend, jend, kend;
/**@} */

/*! \name Signal velocities and emf coefficients */
/**@{ */
  double ***SxL, ***SxR;
  double ***SyL, ***SyR;
  double ***SzL, ***SzR;
  double ***dxL, ***dxR, ***axL, ***axR;
  double ***dyL, ***dyR, ***ayL, ***ayR;
  double ***dzL, ***dzR, ***azL, ***azR;
/**@} */

/*! \name Edge-averaged electric fields  ("e" = edge)  */
/**@{ */
  double ***Ex1e;
  double ***Ex2e;
  double ***Ex3e;
/**@} */

#if PHYSICS == ResRMHD
/*! \name Edge-averaged magnetic fields  ("e" = edge)  */
/**@{ */
  double ***Bx1e;
  double ***Bx2e;
  double ***Bx3e;
/**@} */
#endif
} EMF;

/* ********************************************************************* */
/*! The PLUTO Grid structure contains information pertaining to the
    computational mesh in a specific 1D coordinate direction.
    Since PLUTO assumes a logically rectangular system of coordinates,
    the whole computational domain is obtained as the cartesian product
    of 2 or 3 grid structures.\n

    In parallel, each processor owns a different portion of the domain
    and the grid structures will be different.
    For this reason, in the following member description, we use the
    word "global" or "local" to refer the the whole computational domain
    or to the sub-domain owned by a single processor.

    Similarly, variables ending with a "glob" suffix are intended to be
    global, i.e., they refer to the whole computational stencil and
    not to the local processor sub-domain.
   ********************************************************************* */

typedef struct Grid_{
  double xbeg[3], xend[3];           /**< Leftmost and rightmost points in local domain. */
  double xbeg_glob[3], xend_glob[3];  /**< Leftmost and rightmost point in the global domain. */
  double *x[3], *x_glob[3];   /**< Cell geometrical central points. */
  double *xr[3], *xr_glob[3]; /**< Cell right interface. */
  double *xl[3], *xl_glob[3]; /**< Cell left interface. */
  double *dx[3], *dx_glob[3]; /**< Cell width.  */
  double *xgc[3];          /**< Cell volumetric centroid
                             (!= x when geometry != CARTESIAN).  */
  double  ***dV;           /**< Cell volume.  */
  double  ***A[3];         /**< Right interface area, A[i] = \f$A_{i+\HALF}\f$. */
  double  **dx_dl[3];      /**< dx/dl (dl = length), used for gradient-like operators */
  double *rt;              /**< In spherical coordinates, gives \tilde{r} */
  double *sp;              /**< In spherical coordinates, gives fabs(sin(th))
                                at a j+1/2 interface */
  double *s;               /**< In spherical coordinates, gives fabs(sin(th))
                                at the cell center */
  double *dmu;             /** < In spherical coordinates, gives the \theta
                                 volume = fabs(cos(th_m) - cos(th_p)) */
  double *inv_dx[3];       /**<      */
  double *inv_dxi[3];      /**< inverse of the distance between the center of
                             two cells, inv_dxi = \f$\DS \frac{2}{\Delta x_i +
                             \Delta x_{i+1}}\f$.     */
  double dl_min[3];      /**<  minimum cell length (e.g. min[dr, r*dth,
                            r*sin(th)*dphi] (GLOBAL DOMAIN).  */
  int np_tot_glob[3]; /**< Total number of points in the global domain
                        (boundaries included). */
  int np_int_glob[3]; /**< Total number of points in the global domain
                        (boundaries excluded). */
  int np_tot[3];      /**< Total number of points in the local domain
                           (boundaries included). */
  int np_int[3];      /**< Total number of points in the local domain
                           (boundaries excluded). */
  int nghost[3];      /**< Number of ghost zones. */
  int lbound[3];      /**< When different from zero, it specifies the boundary
                           condition to be applied at leftmost grid side where
                           the physical boundary is located.
                           Otherwise, it equals zero if the current
                           processor does not touch the leftmost physical boundary.
                           This evantuality (lbound = 0) is possible only
                           in PARALLEL mode.  */
  int rbound[3];      /**< Same as lbound, but for the right edge of the grid. */
  int gbeg[3];        /**< Global start index for the global array. */
  int gend[3];        /**< Global end   index for the global array. */
  int beg[3];         /**< Global start index for the local array. */
  int end[3];         /**< Global end   index for the local array. */
  int lbeg[3];        /**< Local start  index for the local array. */
  int lend[3];        /**< Local end    index for the local array. */
  int uniform[3];     /* = 1 when the grid is cartesian AND uniform everywhere  */
  int nproc[3];       /**< number of processors for this grid. */
  int rank_coord[3];  /**< Parallel coordinate in a Cartesian topology. */
  int level;          /**< The current refinement level (chombo only). */
  int *ring_av_csize; /**< The chunk size when RING_AVERAGE is turned on */
  char fill[344];   /* useless, just to make the structure size a power of 2 */
} Grid;

/* ********************************************************************* */
/*! The RBox (= Rectangular Box) defines a rectangular portion of the
    domain in terms of the grid indices <tt>[ibeg,jbeg,kbeg]</tt> corresponding
    to the lower corner and <tt>[iend,jend,kend]</tt> corresponding to the
    upper corner.
    The integer \c vpos specifies the variable location with respect to
    the grid (e.g. center/staggered).

    With some macros it is possible to sweep along the box by changing the
    direction order (e.g.  yxz rather than xyz), see ::BOX_TRANSVERSE_LOOP.
    In this case the index pointers <tt> n, t, b </tt> (normal, tangent
    and bitangent) and the corresponding lower and upper bounds must be
    set properly using the RBoxSetDirections() function.
    These are normally used as hidden indices inside the macro.

    \note The lower and upper grid indices may also be reversed
          (e.g. <tt> box->ibeg > box->iend </tt>).
           In this case the macro ::BOX_LOOP
          automatically reset the directional increment (\c box->di) to -1.
   ********************************************************************* */

typedef struct RBox_{
  int ibeg; /**< Lower corner index in the x1 direction. */
  int iend; /**< Upper corner index in the x1 direction. */
  int jbeg; /**< Lower corner index in the x2 direction. */
  int jend; /**< Upper corner index in the x2 direction. */
  int kbeg; /**< Lower corner index in the x3 direction. */
  int kend; /**< Upper corner index in the x3 direction. */
  int di;   /**< Directional increment (+1 or -1) when looping over the 1st
                 dimension of the box. Automatically set by the ::BOX_LOOP macro. */
  int dj;   /**< Directional increment (+1 or -1) when looping over the 2nd
                 dimension of the box. Automatically set by the ::BOX_LOOP macro. */
  int dk;   /**< Directional increment (+1 or -1) when looping over the 3rd
                 dimension of the box. Automatically set by the ::BOX_LOOP macro. */
  int vpos; /**< Location of the variable inside the cell. */
  int *n;   /**< Pointer to the normal index when looping along a specified direction.
                 Set manually or using a macro. */
  int *t;   /**< Pointer to tangent index when looping along a specified direction
                 (e.g. \c j when <tt> dir = IDIR </tt>).
                 Set manually or using a macro. */
  int *b;   /**< Pointer to binormal index when looping along a specified direction
                 (e.g. \c k when <tt> dir = IDIR </tt>)
                 Set manually or using a macro. */
  int *nbeg; /**< Pointer to lower index in the normal direction.
                  Set by the RBoxSetDirections() function.  */
  int *nend; /**< Pointer to upper index in the normal direction.
                  Set by the RBoxSetDirections() function.  */
  int *tbeg; /**< Pointer to lower index in the tangent direction.
                  Set by the RBoxSetDirectionsl() function.  */
  int *tend; /**< Pointer to upper index in the tangent direction.
                  Set by the RBoxSetDirections() function.  */
  int *bbeg; /**< Pointer to lower index in the binormal direction.
                  Set by the RBoxSetDirections() function.  */
  int *bend; /**< Pointer to upper index in the binormal direction.
                  Set by the RBoxSetDirections() function.  */
} RBox;

/* ********************************************************************* */
/*! The restart structure contains restart information that must be
    read from disk to restart PLUTO.

    Important: The restart structure should be aligned to a power of 2 to
    prevent (for some compilers) from changing the alignment of the
    structure and therefore troubleshooting when restarting
    from files written on different architectures.
   ********************************************************************* */

typedef struct Restart_{
  int    nstep;
  int    nfile[MAX_OUTPUT_TYPES];
  double t;
  double dt;
  char   fill[40];  /* Align the structure to power of 2 */
} Restart;

/* ********************************************************************* */
/*! The State structure contains one-dimensional vectors of fluid
    quantities, often used during 1D computations (Riemann solver,
    sound speed, etc..),
   ********************************************************************* */

typedef struct State_{
  double **v;      /**< Array of primitive variables    */
  double **u;      /**< Array of conservative variables */
  double **flux;   /**< Array of fluxes                 */
  double **fluxCR; /**< Array of fluxes incudling CR contribution alone  */
  double **lambda; /**< Array of eigenvalues associated to Lp, Rp */
  double **Bbck;   /**< Array of background field components  */
  double *prs;     /**< Array of total pressure (see, flux)  */
  double *a2;      /**< Array of sound speeds squared */
  double *cw;      /**< Array of whistler wave speeds */
  double *h;       /**< Array of enthalpies */
  double **J;      /**< Array of currents (e.g. Hall-MHD, RRMHD) */
  double **cCR;    /**< Cosmic ray velocity times R: cCR = R*u_\CR. */
  double **Fcr;    /**< Cosmic Rays force (used to copy d->Fcr during
                        directional sweeps)                      */
  double ***Lp; /**< Left eigenvectors  (primitive formulation). */
  double ***Rp; /**< Right eigenvectors (primitive formulation). */
  char fill[8]; /* Fill structure to power of 2 */
} State;

/* ********************************************************************* */
/*! This structure contains one-dimensional vectors of conserved
    variables, primitive variables, fluxes and so on, used during
    the reconstruct-Solve-Average strategy.
    It is a frequently passed to the Riemann solver routines, source and
    flux functions, etc.
   ********************************************************************* */

typedef struct Sweep_{
  double **vn;    /**< Cell-centered primitive varables at the base time level,
                      v[i] = \f$ \vec{V}^n_i \f$ . */
  double **flux;      /**< upwind flux computed with the Riemann solver */
  double **tc_flux;   /**< Thermal conduction flux    */

  double *lmax;   /**< Define the maximum k-characteristic speed over the domain */
  double **src;

  double **rhs;     /**< Conservative right hand side */
  double *press;    /**< Upwind pressure term computed with the Riemann solver */
  double *Bn;       /**< Face-centered magentic field, e.g., Bn = Bx(i+1/2) */
  double *En;       /**< Face-centered electric field, e.g., En = Ex(i+1/2) */
  double *SL;       /**< Leftmost  velocity in the Riemann fan at i+1/2 */
  double *SR;       /**< Rightmost velocity in the Riemann fan at i+1/2 */

  #if RADIATION
  double *SrL;     /**< Leftmost interface velocity for radiation fields */
  double *SrR;     /**< Rightmost interface velocity for radiation fields */
  #endif

  double **pnt_flux;
  double **dff_flux;
  double *SaL, *SaR, *Sc; /**< MHD alfven waves, contact wave */
  double *dL, *dR;        /**< Diffusion coefficient for EMF  */
  double *aL, *aR;        /**< Flux averaging coefficients    */
  uint16_t *flag;
  State stateL;
  State stateR;
  State stateC;
  char fill[16];
} Sweep;

typedef struct Table2D_ {
  char **defined;
  int nx;  /**< Number of columns or points in the x direction */
  int ny;  /**< Number of rows    or points in the y direction */
  int nf;
  int interpolation;   /**< LINEAR/SPLINE1  */
  int **i;
  int id;
  double *x;  /**< array of x-values (not uniform) */
  double *y;  /**< array of y-values (not uniform) */
  double *dx; /**< grid spacing array in the x direction (not uniform) */
  double *dy; /**< grid spacing array in the y direction (not uniform) */
  double *lnx; /**< array of log10(x) values (uniform) */
  double *lny; /**< array of log10(y) values (uniform) */
  double **f;

  double **a;  /**< Spline coefficient (x^3) */
  double **b;  /**< Spline coefficient (x^2) */
  double **c;  /**< Spline coefficient (x)   */
  double **d;  /**< Spline coefficiten (1)   */

  double **dfx;
  double **dfy;
  double *fmin;
  double *fmax;
  double *df;
  double lnxmin; /**< lower limit (in log10) in the x-direction */
  double lnxmax; /**< upper limit (in log10) in the x-direction */
  double lnymin; /**< lower limit (in log10) in the y-direction */
  double lnymax; /**< upper limit (in log10) in the y-direction */
  double dlnx;   /**< uniform spacing in log10(x) */
  double dlny;   /**< uniform spacing in log10(y) */
  double dlnx_1;
  double dlny_1;
} Table2D;

/* ********************************************************************* */
/*! The timeStep structure contains essential information for
    determining the time step.
   ********************************************************************* */

typedef struct timeStep_{
  double *cmax;     /**< Maximum signal velocity for hyperbolic eqns. */
  double invDt_hyp;   /**< Inverse of hyperbolic time step,
                         \f$ \lambda/\Delta l\f$.*/
  double invDt_par;   /**< Inverse of parabolic (diffusion)  time step
                         \f$ \eta/\Delta l^2\f$. */
  double invDt_particles; /**< Max inverse dt for particles */
  double omega_particles; /**< Max Larmor frequency for particles */
  double dt_cool;   /**< Cooling time step. */
  double cfl;       /**< Courant number for advection. */
  double cfl_par;   /**< Courant number for diffusion (STS only). */
  double rmax_par;
  double particles_tstart; /**< A copy of runtime->particles_tstart */
  double clock_particles;
  double clock_particles_bound;

  double clock_hyp;
  double clock_par;
  double clock_cooling;
  double clock_tot;

  int    Nsub_particles; /**< Number of sub-cycles in particles */
  int    Nsts;      /**< Maximum number of substeps used in STS. */
  int    Nrkc;      /**< Maximum number of substeps used in RKC. */
  int    Nrkl;      /**< Maximum number of substeps used in RKL. */
} timeStep;

/* ********************************************************************* */
/*! The Output structure contains essential information for I/O.
   ********************************************************************* */

typedef struct Output_{
  int    type;         /**< Output data format (DBL, FLT, VTK, ...). */
  int    nvar;         /**< (Fluid only) Total # of vars that can potentially be written.
                            This is the same for all kind of outputs   */
  int    cgs;          /**< (Fluid only) When set to 1, save data in c.g.s units     */
  int    nfile;        /**< Current number being saved. */
  int    dn;           /**< Step increment between outputs. */
  int    stag_var[MAX_OUTPUT_VARS];  /**< (Fluid only). Centered or staggered
                                           variable - same for all outputs. */
  int    dump_var[MAX_OUTPUT_VARS];  /**< (Fluid only) Include/exclude variables
                                           being written.  */
  int    field_dim[MAX_OUTPUT_VARS]; /**< (Particle only) The dimensionality
                                          of the field being written
                                          (scalar = 1, array > 1) */
  char   mode[32];     /**< (Fluid only) Single or multiple files. */
  char   **var_name;   /**< (Fluid only) Variable names. Same for all output types.  */
  char   ext[8];       /**< File extension (.flt, .dbl, etc...)           */
  char   dir[256];     /**< Output directory name                        */

  double dt;           /**< Time increment between outputs. */
  double dclock;       /**< Time increment in clock hours. */
  double ***V[MAX_OUTPUT_VARS]; /**< (Fluid only) Array of pointers to 3D arrays
                                     to be written - same for all outputs. */
  char   fill[140];    /**< Useless, just to make the structure size a power of 2 */
} Output;

/* ********************************************************************* */
/*! The Runtime structure contains runtime initialization parameters
    read from pluto.ini (or equivalent).
   ********************************************************************* */

typedef struct Runtime_{
  int    npoint[3];           /**< Global number of zones in the interior domain */
  int    left_bound[3];       /**< Array of left boundary types */
  int    right_bound[3];      /**< Array of right boundary types */
  int    grid_is_uniform[3];  /* = 1 when grid is uniform, 0 otherwise */
  int    npatch[5];           /**< The number of grid patches  */
  int    patch_npoint[5][16]; /* number of points per patch */
  int    patch_type[5][16];
  int    log_freq;            /**< The log frequency (\c log) */
  int    user_var;            /**< The number of additional user-variables being
                                 held in memory and written to disk */
  int    anl_dn;               /*  number of step increment for ANALYSIS */
  char   solv_type[64];         /**< The Riemann solver (\c Solver) */
  char   rad_solv_type[64];     /**< The radiation Riemann solver (\c Solver) */
  char   user_var_name[128][128];
  char   output_dir[256];         /**< The name of the output directory.
                                       Default is current directory.
                                       (\c output_dir for static PLUTO,
                                        \c Output_dir for PLUTO-Chombo)  */
  char   log_dir[256];            /**< The name of the output directory
                                       where log files will be written to.
                                       Default is output_dir. */
  Output output[MAX_OUTPUT_TYPES];
  double patch_left_node[5][16];  /*  self-expl. */
  double  cfl;               /**< Hyperbolic cfl number (\c CFL) */
  double  cfl_max_var;       /**< Maximum increment between consecutive time
                                  steps (\c CFL_max_var). */
  double  cfl_par;           /**< (STS) parabolic  cfl number */
  double  rmax_par;          /**< (STS) max ratio between current time
                                step and parabolic time step */
  double  tstop;           /**< The final integration time (\c tstop) */
  double  tfreeze;         /**< The fluid freezing time  (\c tfreeze) */
  double  first_dt;        /**< The initial time step (\c first_dt) */
  double  anl_dt;          /**< Time step increment for Analysis()
                                ( <tt> analysis (double) </tt> )*/

  double particles_tstart;  /**< Time at which particles are integrated */
  int     Nparticles_glob;  /**< Total number of particles in the whole domain */
  int     Nparticles_cell;  /**< Total number of particles per cell */

  double  aux[32];         /* we keep aux inside this structure,
                              since in parallel execution it has
                              to be comunicated to all processors  */
} Runtime;


typedef struct RGB{
  unsigned char r, g, b;
} RGB;

typedef struct Image_{
  int    nrow, ncol;    /* -- image rows and columns -- */
  int    slice_plane;   /* -- one of X12_PLANE, X13_PLANE, X23_PLANE -- */
  int    logscale;      /* -- YES/NO for log scale -- */
  char   *colormap;     /* -- colormap name -- */
  char   basename[32];  /* -- image base name (no extensions) -- */
  unsigned char r[256], g[256], b[256]; /* -- colortable saved here -- */
  RGB    **rgb;         /* -- rgb array containing image values -- */
  double max;           /* -- max image value -- */
  double min;           /* -- min image value -- */
  double slice_coord;   /* -- slice coord orthogonal to slice_plane -- */
} Image;

typedef struct FLOAT_VECT{
  float v1, v2, v3;
} Float_Vect;

/* ********************************************************************* */
/*! The List defines a collection of integer values typically used
    as argument to the FOR_EACH() macro.
    Variables are stored sequentially in such a way that structure
    elements can be conveniently initialized upon declaration:

    intList list = {4, MX1, MX2, MX3, ENG};

    list will have 4 elements so that, using the FOR_EACH macro
    a loop will be done on MX1, MX2, MX3, ENG.
   ********************************************************************* */
typedef struct intList_{
  int nvar;       /**< Number of variables. */
  int indx[2046]; /**< Array of integers containg variables indices. */
  int i;          /**< Internal counter. */
} intList;

/* ********************************************************************* */
/*! The Data structure contains the main solution 3D arrays used by
    the code.
   ********************************************************************* */

typedef struct Data_{
  double ****Vc;    /**< The main four-index data array used for cell-centered
                        primitive variables. The index order is
                        <tt>Vc[nv][k][j][i]</tt> where \c nv gives the variable
                        index while \c k,\c j and \c i are the
                        locations of the cell in the \f$x_3\f$,
                        \f$x_2\f$ and \f$x_1\f$ direction. */
  double ****Uc;    /**< The main four-index data array used for cell-centered
                       conservative variables. The index order is
                       <tt>Uc[k][j][i][nv]</tt> (\c nv fast running index)
                       where \c nv gives the variable index, \c k,\c j and \c i
                       are the locations of the cell in the \f$x_3\f$,
                       \f$x_2\f$ and \f$x_1\f$ direction. */
  double ****Vs;    /**< The main four-index data array used for face-centered
                         staggered magnetic fields.
                         The index order is <tt>Vc[nv][k][j][i]</tt>,
                         where \c nv gives the variable index, \c k,\c j and \c i
                         are the locations of the cell in the \f$x_3\f$,
                         \f$x_2\f$ and \f$x_1\f$ direction. */
  #ifdef HIGH_ORDER
  double ****Upc;   /**< The main four-index data array used for cell-centered
                         punctual conservative variables. The index order is
                        <tt>Upc[k][j][i][nv]</tt> where \c nv gives the variable
                        index while \c k,\c j and \c i are the
                        locations of the cell in the \f$x_3\f$,
                        \f$x_2\f$ and \f$x_1\f$ direction. */
  double ****Vpc;   /**< The main four-index data array used for cell-centered
                         punctual primitive variables. The index order is
                        <tt>Upc[k][j][i][nv]</tt> where \c nv gives the variable
                        index while \c k,\c j and \c i are the
                        locations of the cell in the \f$x_3\f$,
                        \f$x_2\f$ and \f$x_1\f$ direction. */
  double ****Vps;   /**< The main four-index data array used for face-centered
                         punctual staggered variables. The index order is
                        <tt>Vps[nv][k][j][i]</tt> where \c nv gives the variable
                        index while \c k,\c j and \c i are the
                        locations of the face in the \f$x_3\f$,
                        \f$x_2\f$ and \f$x_1\f$ direction.
                        indexing starts at -1 to start at left faces. */
  double ****Ucb;   /**< The main four-index data array used for boundary conditions.
                        The index order is <tt>Upc[nv][k][j][i]</tt> where \c nv gives
                        the variable index while \c k,\c j and \c i are the
                        locations of the cell in the \f$x_3\f$,
                        \f$x_2\f$ and \f$x_1\f$ direction. */

  double ****Ueq_av;   /**< Equilibrium array (!!!!! BETA TEST ONLY !!!!!! ) */
  double ****Ueq_pt;   /**< Equilibrium array (!!!!! BETA TEST ONLY !!!!!! ) */

  char ***ho_lim;   /**< Discontinuity detector switch */

  double ****Vdpc;   /* Used when HO_DIAG_SCHEME == YES */
  double ****Vdmc;
  double ****Vdcp;
  double ****Vdcm;
  double ****Vdpp;   /* Used when HO_DIAG_SCHEME == YES */
  double ****Vdpm;
  double ****Vdmp;
  double ****Vdmm;

  #endif

  double ****Vuser; /**< Array storing user-defined supplementary variables
                         written to disk. */
  double ***Ax1;    /**< Vector potential comp. in the \f$x_1\f$ dir.*/
  double ***Ax2;    /**< Vector potential comp. in the \f$x_2\f$ dir.*/
  double ***Ax3;    /**< Vector potential comp. in the \f$x_3\f$ dir.*/
  double ****J;     /**< Electric current defined as curl(B). */
  double ***Tc;     /**< Dimensionless temperature array (used for TC) */
  double ***q;      /**< Electric charge density (only for ResRMHD)    */
  uint16_t ***flag; /**< Pointer to a 3D array setting useful integration
                         flags that are retrieved during integration. */

  /* -- Particles-related quantities -- */

  struct particleNode_ *PHead;   /* Must use full declaration since particleNode
                                    typdef will come later on. */

  double ****Fcr;   /**< A four-element 3D array used to compute the three
                         components of the force and the energy source term
                         of the CR feedback on the fluid. */
  double ****Jcr;   /**< The CR current density 3D array. */
  double ***qcr;    /**< The CR charge density 3D array.  */

  double ****Fdust; /**< Drag force (dust particles only)   */
  #if TIME_STEPPING == RK3
  double ****Fdust_mid; 
  #endif

  struct Particle_ **pstr;  /**< Used to convert a linked list to array (useful ?) */
  int particles_GC_InvalidCount; /**< Number of particles for which GCA conditions are not fulfilled. */
  double ****Vc0; /* Use by Particles GC module to compute temporal derivatives. */


/* EMF  */
  double ***Ex1; /**< cell-centered emf used in CT averaging or CR particles */
  double ***Ex2; /**< cell-centered emf used in CT averaging or CR particles */
  double ***Ex3; /**< cell-centered emf used in CT averaging or CR particles */

  struct ElectroMotiveForce *emf;

/* Others */
  struct timeStep_  *Dts;

  /* ForcedTurb */
  struct ForcedTurb *Ft;

  void (*fluidRiemannSolver)     (const Sweep *, int, int, double *, Grid *);
  void (*radiationRiemannSolver) (const Sweep *, int, int, double *, Grid *);
  char fill[54];  /* make the structure a power of two.  */
} Data;
