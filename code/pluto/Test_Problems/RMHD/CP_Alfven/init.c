/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Circularly polarized Alfven waves for RMHD

  Setup the initial conditions for the large-amplitude circularly
  polarized Alfven wave test, as in section 4.1 of Del Zanna (2007).

  Note: in 2D the solution is rotated around the $z$-axis by
  assuming one  wavelength in both directions.
  This implies that \f$ k_x = 2\pi/L_x\f$ and \f$ k_y = 2\pi/L_y\f$.
  The transformation from the 1D (unrotated) system with primed
  coordinates to the actual computational system is given by
  \f[
      \vec{V}  = R\vec{V}' \qquad{\rm where}\quad
      R = \left(\begin{array}{ccc}
              \cos\alpha  & -\sin\alpha   &  0 \\ \noalign{\medskip}
              \sin\alpha  &  \cos\alpha   &  0 \\ \noalign{\medskip}
                     0  &           0   &  1 \\ \noalign{\medskip}
          \end{array}\right)
  \f]
  while the invers transormation is
  \f[
      \vec{V}'  = R^{-1}\vec{V} \qquad{\rm where}\quad
      R^{-1} = \left(\begin{array}{ccc}
                \cos\alpha  &  \sin\alpha   &  0 \\ \noalign{\medskip}
               -\sin\alpha  &  \cos\alpha   &  0 \\ \noalign{\medskip}
                       0  &           0   &  1 \\ \noalign{\medskip}
          \end{array}\right)
  \f]
  Note that the wave phase
  \f[
      \phi = \vec{k}\cdot\vec{x} = \vec{k}'\cdot\vec{x}'
  \f]
  is invariant under rotations.

  \author A. Mignone (andrea.mignone@unito.it)
          M. Bugli (matteo.bugli@unito.it)
  \date   May 24, 2023

  \b References:
     - Del Zanna et al, A&A (2007) 473, 11-30
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"
#include "rotate.h"

/* ********************************************************************* */
void Init (double *us, double x, double y, double z)
/*
 *
 *
 *
 *********************************************************************** */
{
  static int first_call = 1;

  double Lx = g_domEnd[IDIR] - g_domBeg[IDIR];
  double Ly = g_domEnd[JDIR] - g_domBeg[JDIR];
  double Lz = g_domEnd[KDIR] - g_domBeg[KDIR];
  double kx = 2.0*CONST_PI/Lx;  /* Choose wavevector so that we have one  */
  double ky = 2.0*CONST_PI/Ly;  /* wavelenght in each direction.          */
  double kz = 2.0*CONST_PI/Lz;

/* ------------------------------------
   0. Define rotation shift vectors
      from the domain size.
      Assume jshift_y = kshift_z = 1
      and square cells (dx=dy=dz).
   ------------------------------------ */

  double jshift_y = 1;
  double jshift_x = round(-jshift_y/Ly);
  double kshift_z = 1;
  double kshift_x = round(-kshift_z/Lz);

/* -------------------------------------------------
   1. Define solution in the unrotated (1D) frame
   ------------------------------------------------- */

  us[RHO] = 1.0;
  us[PRS] = 1.0;

  double kmod = sqrt(DIM_EXPAND(kx*kx, + ky*ky, + kz*kz));
  g_gamma = 4.0/3.0;
  double B0 = 1.0;
  double eta = 1.0;

  /* Alfven speed */
  double w  = us[RHO] + g_gamma*us[PRS]/(g_gamma - 1.0);
  double vA  = B0*B0/(w + B0*B0*(1.0 + eta*eta));
  vA /= 0.5*(1.0 + sqrt(1.0 - 4.0*eta*eta*vA*vA));
  vA  = sqrt(vA);

  /* Wave phase (invariant under rotations) */
  double phi = DIM_EXPAND(kx*x, + ky*y, + kz*z);

  us[BX1] = B0;
  us[BX2] = eta*B0*cos(phi);
  us[BX3] = eta*B0*sin(phi);

  us[VX1] = 0.0;
  us[VX2] = -vA*us[BX2]/B0;
  us[VX3] = -vA*us[BX3]/B0;

/* -------------------------------------------------
   2. Rotate vectors
   ------------------------------------------------- */

  RotateSet(jshift_x, jshift_y, kshift_x, kshift_z);

  RotateVector(us + VX1,-1);
  RotateVector(us + BX1,-1);

/* -------------------------------------------------
   3. Compute vector potential
   ------------------------------------------------- */

  double kvec[3]={kx, ky, kz};
  double xvec[3]={x, y, z};
  RotateVector(kvec,1);
  RotateVector(xvec,1);

  phi += 0.5*CONST_PI;
  us[AX1] = 0.0;
  us[AX2] = B0*sin(phi)/kvec[0];
  us[AX3] = B0*cos(phi)/kvec[0] + B0*xvec[1];  /* Assume rho = 1 */
  RotateVector(us + AX1,-1);

  if (first_call == 1){
    print ("o vA    = %18.15e\n",vA);
    print ("o omega = %18.15e\n",kmod*vA);
    print ("o T     = %18.15e\n",2*CONST_PI/(kmod*vA));
    first_call = 0;
  }
}

/* ********************************************************************* */
void InitDomain (Data *d, Grid *grid)
/*!
 * Assign initial condition by looping over the computational domain.
 * Called after the usual Init() function to assign initial conditions
 * on primitive variables.
 * Value assigned here will overwrite those prescribed during Init().
 *
 *
 *********************************************************************** */
{
}

/* ********************************************************************* */
void Analysis (const Data *d, Grid *grid)
/*
 *
 *
 *********************************************************************** */
{
  int    i, j, k, m;
  int    nx1_glob = grid->np_int_glob[IDIR];
  int    nx2_glob = grid->np_int_glob[JDIR];
  int    nx3_glob = grid->np_int_glob[KDIR];
  double v[256];
  double errp = 0.0, errp_glob;
  FILE *fp, *fex;

/*-------------------------------------
   1. Assigning total domain
  ------------------------------------- */

  double *x1 = grid->x[IDIR],  *x2 = grid->x[JDIR],  *x3 = grid->x[KDIR];

  if (g_stepNumber == 0) return;

/*-------------------------------------
   2. converting Vc -> Vpc
  ------------------------------------- */

  #ifdef HIGH_ORDER
  BoundaryConservative(d, grid);
  PointValue(d, grid);
  #endif

/*-------------------------------------
   3. Evaluating punct L1 norm on Vpc
  ------------------------------------- */

  DOM_LOOP(k,j,i) {

    Init(v, x1[i], x2[j], x3[k]);
    #ifdef HIGH_ORDER
    errp += fabs(d->Vpc[BX2][k][j][i] - v[BX2]);
    #else
    errp += fabs(d->Vc[BX2][k][j][i] - v[BX2]);
    #endif
  } /* End loop on i, j, k */

  #ifdef PARALLEL
  MPI_Allreduce (&errp, &errp_glob, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  errp = errp_glob;
  #endif

  errp /= nx1_glob*nx2_glob*nx3_glob;

/* ---- write to disk ---- */

  if (prank == 0) {
   if (nx1_glob == 32){
      time_t time_now;
      time(&time_now);

      fp = fopen("err.dat","w");
    } else {
      fp = fopen("err.dat","a");
    }
    fprintf (fp,"%d  %12.6e \n", nx1_glob, errp);
    fclose(fp);
  }
}
#if PHYSICS == MHD
/* ************************************************************** */
void BACKGROUND_FIELD (real x1, real x2, real x3, real *B0)
/*
 *
 * PURPOSE
 *
 *   Define the component of a static, curl-free background
 *   magnetic field.
 *
 *
 * ARGUMENTS
 *
 *   x1, x2, x3  (IN)    coordinates
 *
 *   B0         (OUT)    vector component of the background field.
 *
 *
 **************************************************************** */
{
   B0[0] = 0.0;
   B0[1] = 0.0;
   B0[2] = 0.0;
}
#endif

/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox *box, int side, Grid *grid)
/*!
 *  Assign user-defined boundary conditions.
 *
 * \param [in/out] d  pointer to the PLUTO data structure containing
 *                    cell-centered primitive quantities (d->Vc) and
 *                    staggered magnetic fields (d->Vs, when used) to
 *                    be filled.
 * \param [in] box    pointer to a RBox structure containing the lower
 *                    and upper indices of the ghost zone-centers/nodes
 *                    or edges at which data values should be assigned.
 * \param [in] side   specifies on which side boundary conditions need
 *                    to be assigned. side can assume the following
 *                    pre-definite values: X1_BEG, X1_END,
 *                                         X2_BEG, X2_END,
 *                                         X3_BEG, X3_END.
 *                    The special value side == 0 is used to control
 *                    a region inside the computational domain.
 * \param [in] grid  pointer to an array of Grid structures.
 *
 *********************************************************************** */
{ }

