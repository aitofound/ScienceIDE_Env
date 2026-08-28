/*!
  \file
  \brief X-point test particle acceleration.

  Test-particle acceleration near an X-type magnetic
  reconnection region (see sect. 4.6 of Mignone et al. 2018).

  Configurations #1, #2 correspond, respectively, to the cases
  without and with guide field.
  They follow the normalization indicated in the
  paper (vA = 1 = C/100, t = 1/Omega_L).

  Configurations #3, #4 replicate #1 and #2 with the resistive
  RMHD module and are re-normalized so that C = 1, t = 1/Omega_L.

  Note that, in both cases, EMAG_Z should be interpreted as "cE" and that
  the quantity E/B does not change from classical to
  relativistic, E/B = (v[EX3]/PARTICLES_CR_C)/B0.

  If \f$ v_0, B_0\f$ are the unit velocity and magnetic field in the
  MHD case and \f$ v_1, B_1\f$ in the relativistic case, then initial
  values in code units (denote with a hat) are recovered
  \f[
    \begin{array}{lcl}
      \widehat{(cE)}_0 &=&\DS \frac{cE}{v_0B_0} = \frac{cE}{v_0^2\sqrt{4\pi\rho_0}}
      \\ \noalign{\medskip}
      \widehat{(cE)}_1 &=&\DS \frac{cE}{v_1B_1} = \frac{cE}{v_1^2\sqrt{4\pi\rho_1}}
    \end{array}
  \f]
  Dividing the two expressions yields (\f$\rho_0=\rho_1,\, v_1=100v_0\f$)
  \f[
     \widehat{(cE)}_1 = \widehat{(cE)}_0\frac{v_0^2}{v_1^2}
                      = \frac{\widehat{(cE)}_0}{100^2}
  \f]
  \author A. Mignone (andrea.mignone@unito.it),
          G. Mattia

  \b References
    - "MAGNETOHYDRODYNAMIC-PARTICLE-IN-CELL METHOD FOR COUPLING COSMIC RAYS
       WITH A THERMAL PLASMA: APPLICATION TO NON-RELATIVISTIC SHOCKS"\n
       Bai et al., ApJ (2015) 809, 55

  \date   June 13, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Init (double *v, double x, double y, double z)
/*!
 *
 *
 *********************************************************************** */
{
  double Lx = (g_domEnd[IDIR] - g_domBeg[IDIR])/4.0;
  double Ly = (g_domEnd[JDIR] - g_domBeg[JDIR])/4.0;
  double B0  = g_inputParam[BMAG_0];
  double Bz  = g_inputParam[BMAG_Z];
  double cEz = g_inputParam[EMAG_Z];

  v[RHO] = 1.0;
  v[PRS] = 1.0;

  v[VX1] = 0.0;
  v[VX2] = 0.0;
  v[VX3] = 0.0;

  v[AX1] = 0.0;
  v[AX2] = 0.0;
  v[AX3] = 0.5*B0*(y*y/Ly - x*x/Lx);

  v[BX1] = B0*y/Ly;
  v[BX2] = B0*x/Lx;
  v[BX3] = Bz;

  v[EX1] = 0.0;
  v[EX2] = 0.0;
  v[EX3] = cEz;
  g_smallPressure = 1.e-5;
}

/* ********************************************************************* */
void InitDomain (Data *d, Grid *grid)
/*!
 *
 *
 *********************************************************************** */
{

}

/* ********************************************************************* */
void Analysis (const Data *d, Grid *grid)
/*!
 *
 *********************************************************************** */
{
  static int first_call = 1;
  FILE *fp;

  if (first_call){
    fp = fopen("dt.dat","w");
  }else{
    fp = fopen("dt.dat","a");
  }

  fprintf (fp, "%12.6e  %12.6e\n", g_time, g_dt);
  fclose(fp);
  first_call = 0;

}
#if PHYSICS == MHD
/* ********************************************************************* */
void BackgroundField (double x1, double x2, double x3, double *B0)
/*!
 *********************************************************************** */
{
   B0[0] = 0.0;
   B0[1] = 0.0;
   B0[2] = 0.0;
}
#endif

/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox *box, int side, Grid *grid)
/*!
 *
 *********************************************************************** */
{

}

#if BODY_FORCE != NO
/* ********************************************************************* */
void BodyForceVector(double *v, double *g, double x1, double x2, double x3)
/*!
 *
 *********************************************************************** */
{
  g[IDIR] = 0.0;
  g[JDIR] = 0.0;
  g[KDIR] = 0.0;
}
/* ********************************************************************* */
double BodyForcePotential(double x1, double x2, double x3)
/*!
 * Return the gravitational potential as function of the coordinates.
 *
 * \param [in] x1  position in the 1st coordinate direction \f$x_1\f$
 * \param [in] x2  position in the 2nd coordinate direction \f$x_2\f$
 * \param [in] x3  position in the 3rd coordinate direction \f$x_3\f$
 *
 * \return The body force potential \f$ \Phi(x_1,x_2,x_3) \f$.
 *
 *********************************************************************** */
{
  return 0.0;
}
#endif
