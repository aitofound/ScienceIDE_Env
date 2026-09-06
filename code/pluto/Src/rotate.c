/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Rotate a 1D problem

  Given the initial condition in the 1D, non-rotated frame (\f$\Sigma'\f$),
  we rotate quantities by an angle \f$\gamma \f$ around the y-axis and an
  angle \f$\alpha\f$  around the z-axis.
  The resulting transformation leaves scalar quantities invariant and produce
  vector rotations \f$\vec{q} = \tens{R}_{\gamma\alpha}\vec{q}'\f$ where  
  \f[
    \tens{R}_{\gamma\alpha} = \left(\begin{array}{ccc}
       \cos\alpha\cos\gamma  &   -\sin\alpha  &   -\cos\alpha\sin\gamma
       \\ \noalign{\medskip}
       \sin\alpha\cos\gamma  &    \cos\alpha  &   -\sin\alpha\sin\gamma
       \\ \noalign{\medskip}
       \sin\gamma            &       0        &   \cos\gamma
    \end{array}\right)
    \,,\qquad
    \tens{R}^{-1}_{\gamma\alpha} = \left(\begin{array}{ccc}
       \cos\alpha\cos\gamma  &  \sin\alpha\cos\gamma  &  \sin\gamma
       \\ \noalign{\medskip}
        -\sin\alpha          &   \cos\alpha           &    0
       \\ \noalign{\medskip}
       -\cos\alpha\sin\gamma &  -\sin\alpha\sin\gamma  &  \cos\gamma
      \end{array}\right)
  \f]
  where \f$\tan\gamma = \cos\alpha\tan\beta\f$.
  In 2D, simply set \f$\gamma = 0\f$.
    
  In a discrete domain, the angles \f$\alpha\f$ and \f$\beta\f$ must be chosen 
  in such a way that an integer shift of cells satisfies, for any flow
  quantity q, the translational invariance expressed by
  \f$ q(\vec{x}+\vec{s}) = q (\vec{x})\f$ where
  \f$\vec{s} = (n_x\Delta x, n_y \Delta y, n_z \Delta z)\f$ is orthogonal
   to \f$\vec{n}_p\f$:
  \f[
    \vec{n}_p\cdot\vec{s} = n_x\Delta x + n_y\Delta y\tan\alpha
                                        + n_z\Delta z\tan\beta = 0
  \f]
  The mesh spacing does need to be the same in all directions.
  For a given pair of rotation angles, there can be more than one set of
  integers that satisfies this equation.
  For instance, taking \f$\tan\alpha = 1,\;\tan\beta=2 \f$ one may choose
  (if mesh spacing is the same)
  \f$\vec{s} = \pm(1,1,-1) \f$, \f$\vec{s} = \pm (-1,1,0)\f$ (useful at a
  y-boundary) or  \f$\vec{s} = \pm (2,0,-1) \f$ (useful at a z-boundary).
  In practice, we find convenient to provide two pairs of integer numbers,
  \f$(j_x,\, j_y)\f$ and \f$ (k_x,\, k_z)\f$ giving the integer shifts
  along two directions at a time.
  These numbers are used to find the rotation angles:
  \f[
     \left\{\begin{array}{lcl}
      \DS j_x\Delta x + j_y\Delta y \tan\alpha &=& 0  \\ \noalign{\medskip}
      \DS k_x\Delta x + k_z\Delta z \tan\beta  &=& 0  \,,\qquad
    \end{array}\right.
    \qquad\rightarrow\qquad
      \tan\alpha = -\frac{j_x}{j_y}\frac{\Delta x}{\Delta y} ,\quad
      \tan\beta  = -\frac{k_x}{k_z}\frac{\Delta x}{\Delta z}
  \f]
  This choice allows us to assign boundary conditions using only 2 shifts
  (rather than 3): one along the x direction and the other in the direction
  normal to the boundary plane.

  <b>Plane of Discontinuity.</b>  
  A plane of discontinuity has equation \f$x' = 0\f$ in the non-rotated frame.
  Using the inverse transformation we find the equation of the plane in the
  rotated \f$\Sigma\f$-plane:
  \f[
    x' = \left(\tens{R}^{-1}_{\gamma} \vec{x}\right)_x
       =   (\cos\alpha\cos\gamma)\, x
         + (\sin\alpha\sin\gamma)\, y
         + (\sin\gamma) z = 0
    \quad\rightarrow\quad
    \boxed{x + y\tan\alpha + z\tan\beta = 0}
  \f]
  The normal to this plane is the vector \f$\vec{n}_p = (1, \tan\alpha, \tan\beta)\f$.

  <b>Note on periodic domains.</b>
  In a periodic domain, an integer number of wavelengths must be contained
  in each direction.
  Hence the rotation is usuallt applied by specifying
  \f$\tan\alpha = k_y/k_x\f$ and \f$\tan\beta = k_z/k_x\f$ which express
  the ratios between the \f$y\f$- and  \f$z\f$- components of the wavevector
  with the \f$x\f$ component.
  For one wavelength in each direction,
  \f$k_x = 2\pi/L_x\f$, \f$k_y = 2\pi/L_y\f$, \f$k_z = 2\pi/L_z\f$.
  The integer shifts can then be specified using
  \f[
      \tan\alpha = -\frac{j_x}{j_y}\frac{\Delta x}{\Delta y} = \frac{L_x}{L_y} ,
      \qquad
      \tan\beta  = -\frac{k_x}{k_z}\frac{\Delta x}{\Delta z} = \frac{L_x}{L_z}
  \f]

  
  <b>Note on the final time.</b>
  Usually, the rotated domain size differs from the typical 1D configuration
  since solutions are compared by looking at the profiles in the x direction.
  The final time is then adjusted in such a way that the intersection
  of the rotated plane with the x axis travels the same distance covered
  in the 1D case.
  Imagine a point in 1D that travels a distance \f$\delta L_1 = v\delta t_1\f$:
  in 2- or 3-D the same point will be traveling along the direction given by
  \f$\DS \vec{x}_p(t) = \hvec{n}_p vt\f$ where
  \f$ \hvec{n}_p = \vec{n}_p/|\vec{n}_p| \f$.
  In the rotated problem the solution is constant on the plane of equation
  \f$ \hvec{n}_p\cdot(\vec{x}-\vec{x}_p(t)) = 0\f$ and its intersection
  with the x axis is (\f$ y = z = 0 \f$) covers the distance \f$\delta L_1\f$
  \f[
      \hat{n}_x\delta L_1 = v\delta t
      \qquad\rightarrow\qquad
      \delta t = n_x\delta t_1 =
      \frac{\delta t_1}{\sqrt{1 + \tan^2\alpha + \tan^2\beta}}
  \f]

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Apr 05, 2020

  \b Reference: 
     - [MT10] "A second-order unsplit Godunov scheme for cell-centered MHD: The CTU-GLM scheme"
        Mignone \& Tzeferacos, JCP (2010) 229, 2217
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

static int    rot_jx, rot_jy, rot_kx, rot_kz;
static double rot_ta, rot_tb;

/* ********************************************************************* */
double RotateGet_ta(void)
/*!
 * Return the rotation angle tan(alpha)
 *********************************************************************** */
{
  return rot_ta;
}

/* ********************************************************************* */
double RotateGet_tb(void)
/*!
 *
 * Return the rotation angle tan(beta)
 *********************************************************************** */
{
  return rot_tb;
}

/* ********************************************************************* */
void RotateSet(int jx, int jy, int kx, int kz)
/*!
 * For a given choice of the input shifts, initialize the rotation
 * angles rot_ta \f$ = \tan\alpha\f$ and rot_tb \f$ = \tan\beta \f$.
 *********************************************************************** */
{
  static int first_call = 1;
  int    nx=RuntimeGet()->npoint[IDIR];
  int    ny=RuntimeGet()->npoint[JDIR];
  int    nz=RuntimeGet()->npoint[KDIR];
  double ta, ca, sa;
  double tb, cb, sb;
  double tg, cg, sg;
  double x1, y1, z1;

  double dx = (g_domEnd[IDIR] - g_domBeg[IDIR])/nx;
  double dy = (g_domEnd[JDIR] - g_domBeg[JDIR])/ny;
  double dz = (g_domEnd[KDIR] - g_domBeg[KDIR])/nz;
  
/* ----------------------------------------------
   1. Define rotation angles in terms of
      integer shifts. Compute coordinates in
      non-rotated frame (x1, y1, z1).
   ---------------------------------------------- */

  if (jy == 0 || kz == 0){
    printLog ("! RotateSet(): specify a non-zero value for jy and kz\n");
    QUIT_PLUTO(1);
  }
  #if DIMENSIONS == 1
  jx = kx = 0;
  #elif DIMENSIONS == 2
  kx = 0;
  #endif

  ta = -(double)jx/(double)jy*dx/dy;
  ca = 1.0/sqrt(1.0 + ta*ta);
  sa = ta*ca;
  
  tb = -(double)kx/(double)kz*dx/dz;
  cb = 1.0/sqrt(1.0 + tb*tb);
  sb = tb*cb;

  tg = tb*ca;
  cg = 1.0/sqrt(1.0 + tg*tg);
  sg = cg*tg;
  
  rot_ta = ta;
  rot_tb = tb;
  
  rot_jx = jx; rot_jy = jy;
  rot_kx = kx; rot_kz = kz;
  
  if (first_call){
    print ("> RotateSet(): ta = %f; tb = %f\n", ta, tb);
    print ("               tg = %f; sg = %f, cg = %f\n",tg,sg,cg);
    print ("               jx, jy = %d, %d\n",jx,jy);
    print ("               kx, kz = %d, %d\n",kx,kz);
    first_call = 0;
  }
}
/* ********************************************************************* */
void RotateVector(double *v, int s)
/*!
 *  Rotate a vector <v[0], v[1], v[2]>;
 *  - s =  1 --> clockwise rotation
 *               [transform vector to the 1D unrotated frame]
 *  - s = -1 --> counter-clockwise rotation
 *               [transform vector to the 2/3D frame]
 *
 *********************************************************************** */
{
  double vx = v[0]; 
  double vy = v[1];
  double vz = v[2];
  double ta = rot_ta;
  double tb = rot_tb;
  double sa, sb, ca, cb, tg, cg, sg;
 
/* -- Compute tangent, sin() and cos() of alpha and beta -- */

  ca = 1.0/sqrt(ta*ta + 1.0);
  cb = 1.0/sqrt(tb*tb + 1.0);
  sa = ta*ca;
  sb = tb*cb;

  tg = tb*ca;
  cg = 1.0/sqrt(tg*tg + 1.0);
  sg = cg*tg;

/* -- Rotate -- */

  if (s == 1){    /* Clockwise rotation */
    v[0] =  vx*ca*cg + vy*sa*cg + vz*sg;
    v[1] = -vx*sa    + vy*ca;
    v[2] = -vx*ca*sg - vy*sa*sg + vz*cg;
  }else if (s == -1){ /* Counter-clockwise rotation */
    v[0] = vx*ca*cg - vy*sa - vz*ca*sg;
    v[1] = vx*sa*cg + vy*ca - vz*sa*sg;
    v[2] = vx*sg            + vz*cg;
  }else{
    print ("! RotateVector(): invalid value of s = %d\n", s);
    QUIT_PLUTO(1);
  }
}
#if 0 
/* ********************************************************************* */
void RotateBoundaryOld (const Data *d, RBox *box, int side, Grid *grid)
/*
 *
 *********************************************************************** */
{
  int nv, i, j, k;
  int i1,j1,k1;
  int s;
  int jx = rot_jx;
  int jy = rot_jy;
  int kx = rot_kx;
  int kz = rot_kz;
  #ifdef HIGH_ORDER
  Data_Arr Qc = d->Ucb;
  #else
  Data_Arr Qc = d->Vc;
  #endif

  if (side == X2_BEG){  /* -- X2_BEG boundary -- */
    BOX_LOOP(box,k,j,i){
      if (jy == 0){
        i1 = i; j1 = JBEG;
      }else{
        s  = (jy > 0 ? 1:-1);
        i1 = MAX(i + s*jx, 0); i1 = MIN(i1, NX1_TOT-1);
        j1 = MAX(j + s*jy, 0); j1 = MIN(j1, NX2_TOT-1);
      }

      if (box->vpos == CENTER){
        NVAR_LOOP(nv) Qc[nv][k][j][i] = Qc[nv][k][j1][i1];
        #if (PHYSICS == ResRMHD) && (defined STAGGERED_MHD)
        d->q[k][j][i] = d->q[k][j1][i1];
        #endif
      }else if (box->vpos == X1FACE){ 
        #ifdef STAGGERED_MHD
        d->Vs[BX1s][k][j][i] = d->Vs[BX1s][k][j1][i1];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        d->Vs[EX1s][k][j][i] = d->Vs[EX1s][k][j1][i1];
if (i >= 0) d->Vs[EX2s][k][j-1][i] = d->Vs[EX2s][k][j1-1][i1];
        #endif
        #endif 
      }else if (box->vpos == X2FACE){ 
        #ifdef STAGGERED_MHD
        d->Vs[BX2s][k][j][i] = d->Vs[BX2s][k][j1][i1];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        d->Vs[EX2s][k][j][i] = d->Vs[EX2s][k][j1][i1];
        #endif
        #endif 
      }else if (box->vpos == X3FACE){ 
        #if (defined STAGGERED_MHD) && (DIMENSIONS == 3)
        d->Vs[BX3s][k][j][i] = d->Vs[BX3s][k][j1][i1];
        #if PHYSICS == ResRMHD
        d->Vs[EX3s][k][j][i] = d->Vs[EX3s][k][j1][i1];
        #endif
        #endif
      }  
    }
  }

  if (side == X2_END){  /* -- X2_END boundary -- */
    BOX_LOOP(box,k,j,i){
      if (jy == 0){
        i1 = i; j1 = JEND;
      }else{
        s  = (jy > 0 ? 1:-1);
        i1 = MAX(i - s*jx, 0); i1 = MIN(i1, NX1_TOT-1);
        j1 = MAX(j - s*jy, 0); j1 = MIN(j1, NX2_TOT-1);
      }
      
      if (box->vpos == CENTER){
        NVAR_LOOP(nv) Qc[nv][k][j][i] = Qc[nv][k][j1][i1];
        #if (PHYSICS == ResRMHD) && (defined STAGGERED_MHD)
        d->q[k][j][i] = d->q[k][j1][i1];
        #endif
      }else if (box->vpos == X1FACE){ 
        #ifdef STAGGERED_MHD
        d->Vs[BX1s][k][j][i] = d->Vs[BX1s][k][j1][i1];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        d->Vs[EX1s][k][j][i] = d->Vs[EX1s][k][j1][i1];
if (i >= 0) d->Vs[EX2s][k][j][i] = d->Vs[EX2s][k][j1][i1];
        #endif
        #endif 
      }else if (box->vpos == X3FACE){ 
        #if (defined STAGGERED_MHD) && (DIMENSIONS == 3)
        d->Vs[BX3s][k][j][i] = d->Vs[BX3s][k][j1][i1];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        d->Vs[EX3s][k][j][i] = d->Vs[EX3s][k][j1][i1];
        #endif
        #endif
      }  
    }  
  }
/*
k = 0;
for (j = JEND; j < NX2_TOT; j++){
ITOT_LOOP(i){
  printLog ("Ex2s(%d, %d) = %f\n",i,j,d->Vs[EX2s][k][j][i]);
}}
exit(1);
*/

  if (side == X3_BEG){  /* -- X3_BEG boundary -- */
    BOX_LOOP(box,k,j,i){
      if (kz == 0){
        i1 = i; k1 = KBEG;
      }else{
        s  = (kz > 0 ? 1:-1);
        i1 = MAX(i + s*kx, 0); i1 = MIN(i1, NX1_TOT-1);
        k1 = MAX(k + s*kz, 0); k1 = MIN(k1, NX3_TOT-1);
      }
      
      if (box->vpos == CENTER){
        NVAR_LOOP(nv) Qc[nv][k][j][i] = Qc[nv][k1][j][i1];
        #if (PHYSICS == ResRMHD) && (defined STAGGERED_MHD)
        d->q[k][j][i] = d->q[k1][j][i1];
        #endif
      }else if (box->vpos == X1FACE){ 
        #ifdef STAGGERED_MHD
        d->Vs[BX1s][k][j][i] = d->Vs[BX1s][k1][j][i1];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        d->Vs[EX1s][k][j][i] = d->Vs[EX1s][k1][j][i1];
        #endif
        #endif 
      }else if (box->vpos == X2FACE){ 
        #if (defined STAGGERED_MHD) && (DIMENSIONS == 3)
        d->Vs[BX2s][k][j][i] = d->Vs[BX2s][k1][j][i1];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        d->Vs[EX2s][k][j][i] = d->Vs[EX2s][k1][j][i1];
        #endif
        #endif
      }  
    }  
  }

  if (side == X3_END){  /* -- X3_END boundary -- */
    BOX_LOOP(box,k,j,i){
      if (kz == 0){
        i1 = i; k1 = KEND;
      }else{
        s  = (kz > 0 ? 1:-1);
        i1 = MAX(i - s*kx, 0); i1 = MIN(i1, NX1_TOT-1);
        k1 = MAX(k - s*kz, 0); k1 = MIN(k1, NX3_TOT-1);
      }

      if (box->vpos == CENTER){
        NVAR_LOOP(nv) Qc[nv][k][j][i] = Qc[nv][k1][j][i1];
        #if (PHYSICS == ResRMHD) && (defined STAGGERED_MHD)
        d->q[k][j][i] = d->q[k1][j][i1];
        #endif
      }else if (box->vpos == X1FACE){ 
        #ifdef STAGGERED_MHD
        d->Vs[BX1s][k][j][i] = d->Vs[BX1s][k1][j][i1];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        d->Vs[EX1s][k][j][i] = d->Vs[EX1s][k1][j][i1];
        #endif
        #endif 
      }else if (box->vpos == X2FACE){ 
        #if (defined STAGGERED_MHD) && (DIMENSIONS == 3)
        d->Vs[BX2s][k][j][i] = d->Vs[BX2s][k1][j][i1];
        #if (PHYSICS == ResRMHD) && (DIVE_CONTROL == CONSTRAINED_TRANSPORT)
        d->Vs[EX2s][k][j][i] = d->Vs[EX2s][k1][j][i1];
        #endif
        #endif
      }
    } 
  }
}
#endif

/* ********************************************************************* */
void RotateBoundary (double ***Q, RBox *box, int side)
/*
 *
 *********************************************************************** */
{
  int nv, i, j, k;
  int i1,j1,k1;
  int s;
  int jx = rot_jx;
  int jy = rot_jy;
  int kx = rot_kx;
  int kz = rot_kz;
  double icheck;
static char ***assigned;
if (assigned == NULL){
  assigned = ARRAY_3D(NX3_TOT, NX2_TOT, NX1_TOT, char);
}
TOT_LOOP(k,j,i) assigned[k][j][i] = 0;
DOM_LOOP(k,j,i) assigned[k][j][i] = 1;

  if (side == X2_BEG){  /* -- X2_BEG boundary -- */
    BOX_LOOP(box,k,j,i){
      if (jy == 0){
        i1 = i; j1 = JBEG;
      }else{
        s  = (jy > 0 ? 1:-1);
        i1 = MAX(i + s*jx, 0); i1 = MIN(i1, NX1_TOT-1);
        j1 = MAX(j + s*jy, 0); j1 = MIN(j1, NX2_TOT-1);
      }
      Q[k][j][i] = Q[k][j1][i1];

/* icheck = (i1-i) + (j1-j)*rot_ta;
   if (fabs(icheck) < 1.e-6) assigned[k][j][i] = 1; */
    }
  }

  if (side == X2_END){  /* -- X2_END boundary -- */
    BOX_LOOP(box,k,j,i){
      if (jy == 0){
        i1 = i; j1 = JEND;
      }else{
        s  = (jy > 0 ? 1:-1);
        i1 = MAX(i - s*jx, 0); i1 = MIN(i1, NX1_TOT-1);
        j1 = MAX(j - s*jy, 0); j1 = MIN(j1, NX2_TOT-1);
      } 
      Q[k][j][i] = Q[k][j1][i1];
/*  icheck = (i-i1) + (j-j1)*rot_ta;
if (fabs(icheck) < 1.e-6) assigned[k][j][i] = 1;  */

    }  
  }

  if (side == X3_BEG){  /* -- X3_BEG boundary -- */
    BOX_LOOP(box,k,j,i){
      if (kz == 0){
        i1 = i; k1 = KBEG;
      }else{
        s  = (kz > 0 ? 1:-1);
        i1 = MAX(i + s*kx, 0); i1 = MIN(i1, NX1_TOT-1);
        k1 = MAX(k + s*kz, 0); k1 = MIN(k1, NX3_TOT-1);
      }
      Q[k][j][i] = Q[k1][j][i1];
/*  icheck = (i1-i) + (k1-k)*rot_tb;
if (fabs(icheck) < 1.e-6) assigned[k][j][i] = 1;  */

    }  
  }

  if (side == X3_END){  /* -- X3_END boundary -- */
    BOX_LOOP(box,k,j,i){
      if (kz == 0){
        i1 = i; k1 = KEND;
      }else{
        s  = (kz > 0 ? 1:-1);
        i1 = MAX(i - s*kx, 0); i1 = MIN(i1, NX1_TOT-1);
        k1 = MAX(k - s*kz, 0); k1 = MIN(k1, NX3_TOT-1);
      }
      Q[k][j][i] = Q[k1][j][i1];

/*  icheck = (i-i1) + (k-k1)*rot_tb;
if (fabs(icheck) < 1.e-6) assigned[k][j][i] = 1;  */

    } 
  }

  return;

////// CHECK /////// 

if (side == X3_END) {
  BOX_LOOP(box,k,j,i){
    if (!assigned[k][j][i]) {
      printLog("! Value (%d, %d, %d) not assigned\n",i,j,k);

    DOM_LOOP(k1,j1,i1){
      if (assigned[k1][j1][i1]){
        icheck = (i1-i) + (j1-j)*rot_ta + (k1-k)*rot_tb;
        if (fabs(icheck) < 1.e-6){
          printf ("! cell (%d, %d, %d) could have been assigned to (%d, %d, %d)\n",
                  i,j,k, i1,j1,k1);
        }
      }
    }

    }
  } 
QUIT_PLUTO(1);
}

}
