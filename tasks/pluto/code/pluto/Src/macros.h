/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief PLUTO header file for function-like macros.

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Sep 17, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#ifndef DEBUG
  #define DEBUG FALSE
#endif

#if DEBUG == TRUE
  #define DEBUG_FUNC_BEG(a)                          \
    char d_funcName[64];                             \
    sprintf (d_funcName,"%s",a);                     \
    d_indent += 2;                                   \
    if (d_condition) { printLog("%*c", d_indent, ' ');  \
                       printLog (">>[%s]\n",a );  }

  #define DEBUG_FUNC_END(a)  \
    if (d_condition) {print("%*c", d_indent, ' ');  \
                     printLog("<<[%s]\n",a ); }        \
     d_indent -= 2;                           
  
  #define DEBUG_FUNC_NAME  d_funcName

#else
  #define DEBUG_FUNC_BEG(a)
  #define DEBUG_FUNC_END(a)
  #define DEBUG_FUNC_NAME "Not Set"
#endif

#define POW2(x)   ((x)*(x))
#define POW3(x)   ((x)*(x)*(x))
#define POW4(x)   ((x)*(x)*(x)*(x))
#define SECH(x) ((x) > 30.0 ? 0.0:1.0/cosh(x))

/* #####################################################################
    1. Macros that can be placed at any point in the code
   ##################################################################### */

/*! \name Spatial loop macros.
    The following macros provide a compact way to perform 1D or multi-D
    loops in selected regions of the (local) computational domain.
    The \c *BEG_LOOP and \c *END_LOOP macros are used to loop in the 
    leftmost or rightmost boundary ghost zones in the corresponding
    direction \c I, \c J or \c K.
    The \c *DOM_LOOP macros are used to loop inside the computational
    domain (boundaries excluded) while the \c *TOT_LOOP macros are
    used to loop across the entire domain (inside+boundary).
*/
/**@{ */
#define IBEG_LOOP(i)  for ((i) = IBEG; (i)--;    )  
#define JBEG_LOOP(j)  for ((j) = JBEG; (j)--;    )  
#define KBEG_LOOP(k)  for ((k) = KBEG; (k)--;    )

#define IEND_LOOP(i)  for ((i) = IEND + 1; (i) < NX1_TOT; (i)++)
#define JEND_LOOP(j)  for ((j) = JEND + 1; (j) < NX2_TOT; (j)++)
#define KEND_LOOP(k)  for ((k) = KEND + 1; (k) < NX3_TOT; (k)++)

#define IDOM_LOOP(i)  for ((i) = IBEG; (i) <= IEND; (i)++)
#define JDOM_LOOP(j)  for ((j) = JBEG; (j) <= JEND; (j)++)
#define KDOM_LOOP(k)  for ((k) = KBEG; (k) <= KEND; (k)++)

#define ITOT_LOOP(i)  for ((i) = 0; (i) < NX1_TOT; (i)++)
#define JTOT_LOOP(j)  for ((j) = 0; (j) < NX2_TOT; (j)++)
#define KTOT_LOOP(k)  for ((k) = 0; (k) < NX3_TOT; (k)++)

#define DOM_LOOP(k,j,i)  KDOM_LOOP(k) JDOM_LOOP(j) IDOM_LOOP(i)
#define DOMI_LOOP(k,j,i) KDOM_LOOP(k) JDOM_LOOP(j) \
                         for (i = IBEG-INCLUDE_IDIR; i <= IEND; i++)
#define DOMJ_LOOP(k,j,i) KDOM_LOOP(k) \
                         for (j = JBEG-INCLUDE_JDIR; j <= JEND; j++) IDOM_LOOP(i) 
#define DOMK_LOOP(k,j,i) for (k = KBEG-INCLUDE_KDIR; k <= KEND; k++)\
                         JDOM_LOOP(j) IDOM_LOOP(i)

#define TOT_LOOP(k,j,i) KTOT_LOOP(k) JTOT_LOOP(j) ITOT_LOOP(i)

#define X1_BEG_LOOP(k,j,i) KTOT_LOOP(k) JTOT_LOOP(j) IBEG_LOOP(i)
#define X2_BEG_LOOP(k,j,i) KTOT_LOOP(k) JBEG_LOOP(j) ITOT_LOOP(i)
#define X3_BEG_LOOP(k,j,i) KBEG_LOOP(k) JTOT_LOOP(j) ITOT_LOOP(i)

#define X1_END_LOOP(k,j,i) KTOT_LOOP(k) JTOT_LOOP(j) IEND_LOOP(i)
#define X2_END_LOOP(k,j,i) KTOT_LOOP(k) JEND_LOOP(j) ITOT_LOOP(i)
#define X3_END_LOOP(k,j,i) KEND_LOOP(k) JTOT_LOOP(j) ITOT_LOOP(i)

/**@} */

/*! The BOX_LOOP() macro implements a loop over (i,j,k) in a rectangular 
    portion of the domain with indices defined by the (pointer to)
    RBox structure B. 
    The loop increments (di,dj,dk) are members of the structure which 
    are here initialized to either 1 or -1 depending on whether the 
    lower corner index lies below or above the upper index 
    (e.g. B->ib <= B->ie or not). 
*/
#define BOX_LOOP(B,k,j,i) \
 for ((B)->dk = ((k=(B)->kbeg) <= (B)->kend ? 1:-1); k != (B)->kend+(B)->dk; k += (B)->dk)\
 for ((B)->dj = ((j=(B)->jbeg) <= (B)->jend ? 1:-1); j != (B)->jend+(B)->dj; j += (B)->dj)\
 for ((B)->di = ((i=(B)->ibeg) <= (B)->iend ? 1:-1); i != (B)->iend+(B)->di; i += (B)->di)


#define IBOX_LOOP(B,i) \
 for ((B)->di = ((i=(B)->ibeg) <= (B)->iend ? 1:-1); i != (B)->iend+(B)->di; i += (B)->di)

#define JBOX_LOOP(B,j) \
 for ((B)->dj = ((j=(B)->jbeg) <= (B)->jend ? 1:-1); j != (B)->jend+(B)->dj; j += (B)->dj)

#define KBOX_LOOP(B,k) \
 for ((B)->dk = ((k=(B)->kbeg) <= (B)->kend ? 1:-1); k != (B)->kend+(B)->dk; k += (B)->dk)

#define BOX_TRANSVERSE_LOOP(box,k,j,i)                                     \
  if      (g_dir == IDIR) {(box)->n = &i; (box)->t = &j; (box)->b = &k;}   \
  else if (g_dir == JDIR) {(box)->n = &j; (box)->t = &i; (box)->b = &k;}   \
  else if (g_dir == KDIR) {(box)->n = &k; (box)->t = &i; (box)->b = &j;}   \
  for (*((box)->b) = *(box)->bbeg; *((box)->b) <= *(box)->bend; *((box)->b) +=1) \
  for (*((box)->t) = *(box)->tbeg; *((box)->t) <= *(box)->tend; *((box)->t) +=1)


/*! The FOR_EACH(p, intList) macro implements a loop over the elements
    of the array \c intList->indx (in a similar way to Python lists).

    Example:
    \code
      intList list;
      list.nvar = 3;
      list.indx[0] = 2;
      list.indx[1] = 5;
      list.indx[2] = 17;
      FOR_EACH(nv, &list) printf ("value is = %d\n",nv);
    \endcode
*/
#define FOR_EACH(nv, list)  \
  for ((list)->i = 0, nv = (list)->indx[0]; \
       (list)->i < (list)->nvar; \
       nv = (list)->indx[++((list)->i)])


/*! Faster implementation than stdlib floor() function.
    It returns the largest integer value less than or equal to z.
*/
#define INT_FLOOR(z)   ((int)((z) + 32768.) - 32768)

/*! Return the maximum between two numbers. */
#define MAX(a,b)  ( (a) >= (b) ? (a) : (b) ) 

/*! Return the minimum between two numbers. */
#define MIN(a,b)  ( (a) <= (b) ? (a) : (b) ) 
      
/*! Return the number with the smaller absolute value. */
#define ABS_MIN(a,b)  (fabs(a) < fabs(b) ? (a):(b)) 
                         
/*! Return the sign of x. */
#define DSIGN(x)      ( (x) >= 0.0 ? (1.0) : (-1.0))
#define DSIGN2(x)      ( (x == 0) ? 0.5:((x) > 0.0 ? (1.0) : (-1.0)))

/*! Quick limiting macros (more general ones are found in States/plm_coeffs.h) */
#define MINMOD_LIMITER(a,b)  ((a)*(b) > 0.0 ? (fabs(a) < fabs(b) ? (a):(b)):0.0)
#define VANLEER_LIMITER(a,b) ((a)*(b) > 0.0 ? 2.0*(a)*(b)/((a)+(b)):0.0)
#define MC_LIMITER(a,b)      (MINMOD_LIMITER(0.5*((a)+(b)), 2.0*MINMOD_LIMITER((a),(b))))
#define SWAP_VAR(x) SwapEndian(&x, sizeof(x));

/*! Exit macro. For Chombo it is defined elsewhere. */
/* See
http://stackoverflow.com/questions/11303135/broadcast-message-for-all-processes-to-exitmpi
*/
#ifdef CHOMBO
 #ifdef PARALLEL
   #define QUIT_PLUTO(e_code)   \
          {error("Abort"); MPI_Abort(MPI_COMM_WORLD, e_code); exit(e_code);}
 #else
   #define QUIT_PLUTO(e_code)   \
          {error("Abort"); exit(e_code);}
 #endif 
#else

  #ifdef PARALLEL
   #define QUIT_PLUTO(e_code)                 \
          {printLog ("! Abort\n");            \
           LogFileFlush();                    \
           MPI_Abort(MPI_COMM_WORLD, e_code); \
           MPI_Finalize();                    \
           exit(e_code);}
  #else
   #define QUIT_PLUTO(e_code)   exit(e_code);
  #endif
#endif

/* #####################################################################
    2. Macros that must be placed only *AFTER* definitions.h has been 
       included
   ##################################################################### */

/* *******************************************************
    Expand dimension- or component-dependent expressions
   *******************************************************  */

/*! \def DIM_EXPAND(a,b,c)
    Allows to write dimension-ndependent expressions involving vectors 
    by evaluating as many arguments as the number of DIMENSIONS. 
    The result is that only the first argument will be compiled in 1D, 
    the first two arguments in 2D and all of them in 3D. 
    As an example: 
    \code
    DIM_EXPAND(v[VX1] =  1.0;  ,
               v[VX2] =  5.0;  , 
               v[VX3] = -4.0; )
    \endcode
    becomes
    \code
     v[VX1] = 1.0;   
    \endcode
    when \c DIMENSIONS is equal to 1 or
    \code
     v[VX1] = 1.0;  
     v[VX2] = 5.0;
    \endcode
    when \c DIMENSIONS is equal to 2 or
    \code
     v[VX1] =  1.0;  
     v[VX2] =  5.0;
     v[VX3] = -4.0;
    \endcode
    when \c DIMENSIONS is equal to 3.
*/

/*! \def DIM_SELECT(a,b,c)
    Expand only the 1st, 2nd or 3rd argument based on the value of
    DIMENSIONS.                                                       */

#if DIMENSIONS == 1
 #define INCLUDE_IDIR   TRUE
 #define INCLUDE_JDIR   FALSE
 #define INCLUDE_KDIR   FALSE

 #define DIM_EXPAND(a,b,c)  a
 #define DIM_SELECT(a,b,c)  a
#endif

#if DIMENSIONS == 2
  #define INCLUDE_IDIR   TRUE
  #ifndef INCLUDE_JDIR
    #define INCLUDE_JDIR   TRUE
  #endif
  #ifndef INCLUDE_KDIR
    #define INCLUDE_KDIR   FALSE
  #endif

  #if INCLUDE_JDIR
    #define DIM_EXPAND(a,b,c) a b
    #define DIM_SELECT(a,b,c) b
  #endif

  #if !INCLUDE_JDIR && INCLUDE_KDIR
    #define DIM_EXPAND(a,b,c) a c
    #define DIM_SELECT(a,b,c) c
  #endif

#endif

#if DIMENSIONS == 3
  #define INCLUDE_IDIR   TRUE
  #ifndef INCLUDE_JDIR
    #define INCLUDE_JDIR   TRUE
  #endif
  #ifndef INCLUDE_KDIR
    #define INCLUDE_KDIR   TRUE
  #endif

  #if INCLUDE_JDIR == TRUE
    #define DIM_EXPAND(a,b,c) a b c
    #define DIM_SELECT(a,b,c) c
  #else
    #define DIM_EXPAND(a,b,c) a c
    #define DIM_SELECT(a,b,c) c
  #endif

#endif

/*
       last  inc
1 1 1    3    1
1 1 0    2    1
1 0 1    3    2
1 0 0    1    1
*/
#if DIMENSIONS == 3 || DIMENSIONS == 1
#define DIM_LOOP(d)   for ((d) = 0; (d) < DIMENSIONS; \
                           (d) += 2*INCLUDE_IDIR - INCLUDE_JDIR)
#else
#define DIM_LOOP(d)   for ((d) = 0; \
                           (d) < MAX(3*INCLUDE_KDIR, MAX(INCLUDE_IDIR,2*INCLUDE_JDIR)); \
                           (d) += 2*INCLUDE_IDIR - INCLUDE_JDIR)
#endif


#define DOT_PRODUCT(a,b)  ((a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2])
#define CROSS_X1(a,b)   (a[1]*b[2] - a[2]*b[1])
#define CROSS_X2(a,b)   (a[2]*b[0] - a[0]*b[2])
#define CROSS_X3(a,b)   (a[0]*b[1] - a[1]*b[0])

#if WARNING_MESSAGES == YES
 #define WARNING(a)  a
#else
 #define WARNING(a)
#endif

/*! \name Forward and central finite differences macros.
    The following set of macros provide a compact way to perform 
    two-point, undivided finite difference operations in a specified
    direction.
    Differences can be either \c forward or \c central.
    For instance, \c FDIFF_X2(Q,k,j,i) will compute a forward
    difference of \c Q in the \c y direction, \c (Q[j+1]-Q[j]), while
    \c CDIFF_X3(Q,k,j,i) will compute a central different approximation
    of \c Q in the \c z direction: \c (Q[k+1] - Q[k-1])/2.
*/
/**@{ */

#if INCLUDE_IDIR
  #define FDIFF_X1(Q,k,j,i)      (Q[k][j][i+1] - Q[k][j][i])
  #define CDIFF_X1(Q,k,j,i)  (0.5*(Q[k][j][i+1] - Q[k][j][i-1]))
#else
  #define FDIFF_X1(Q,k,j,i)  (0.0)
  #define CDIFF_X1(Q,k,j,i)  (0.0)
#endif

#if INCLUDE_JDIR
  #define FDIFF_X2(Q,k,j,i)       (Q[k][j+1][i] - Q[k][j][i])
  #define CDIFF_X2(Q,k,j,i)  (0.5*(Q[k][j+1][i] - Q[k][j-1][i]))
#else
  #define FDIFF_X2(Q,k,j,i)  (0.0)
  #define CDIFF_X2(Q,k,j,i)  (0.0)
#endif

#if INCLUDE_KDIR
  #define FDIFF_X3(Q,k,j,i)       (Q[k+1][j][i] - Q[k][j][i])
  #define CDIFF_X3(Q,k,j,i)  (0.5*(Q[k+1][j][i] - Q[k-1][j][i]))
#else
  #define FDIFF_X3(Q,k,j,i)  (0.0)
  #define CDIFF_X3(Q,k,j,i)  (0.0)
#endif

/**@} */

/*! \name Spatial averages macros.
    The following set of macros provide a compact way to perform multi-D
    averages from cell centered values to interfaces.
    For instance, \C AVERAGE_X(q,k,j,i) will simply take the
    arithmetic average betwen q(i) and q(i+1) at the i+1/2 interface.
    Likewise, AVERAGE_YZ(q,k,j,i) will produce an average at the
    j+1/2 and k+1/2 edge.
*/
/**@{ */

#if INCLUDE_IDIR
  #define AVERAGE_X(q,k,j,i)   (0.5*(q[k][j][i] + q[k][j][i+1]))
#else
  #define AVERAGE_X(q,k,j,i)   (q[k][j][i])
#endif

#if INCLUDE_JDIR
  #define AVERAGE_Y(q,k,j,i)   (0.5*(q[k][j][i] + q[k][j+1][i]))
  #define AVERAGE_XY(q,k,j,i)  (0.5*(AVERAGE_X(q,k,j,i) + AVERAGE_X(q,k,j+1,i)));
#else
  #define AVERAGE_Y(q,k,j,i)    (q[k][0][i])
  #define AVERAGE_XY(q,k,j,i)   AVERAGE_X(q,k,0,i)
#endif

#if INCLUDE_KDIR
  #define AVERAGE_Z(q,k,j,i)    (0.5*(q[k][j][i] + q[k+1][j][i]))
  #define AVERAGE_XZ(q,k,j,i)   (0.5*(AVERAGE_X(q,k,j,i) + AVERAGE_X(q,k+1,j,i)))
  #define AVERAGE_YZ(q,k,j,i)   (0.5*(AVERAGE_Y(q,k,j,i) + AVERAGE_Y(q,k+1,j,i)))
  #define AVERAGE_XYZ(q,k,j,i)  (0.5*(AVERAGE_XY(q,k,j,i) + AVERAGE_XY(q,k+1,j,i)))
#else
  #define AVERAGE_Z(q,k,j,i)    (q[0][j][i])
  #define AVERAGE_XZ(q,k,j,i)   AVERAGE_X(q,0,j,i)
  #define AVERAGE_YZ(q,k,j,i)   AVERAGE_Y(q,0,j,i)
  #define AVERAGE_XYZ(q,k,j,i)  AVERAGE_XY(q,0,j,i)
#endif
