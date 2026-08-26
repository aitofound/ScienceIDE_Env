/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief RadHD Disk-Planet interaction problem.

  RadHD version of the Disk-Planet test in Test_Problems/HD/Disk_Planet with
  constant absorption opacity.

  - Configurations #01 and #02: 3D spherical, no irradiation.
  - Configuration #03: 2D polar \f$(r,\phi)\f$, no irradiation.
  - Configuration #04: 3D polar, no irradiation.
  - Configuration #05: 3D spherical, simplified irradiation function.
    
  \authors  J. D. Melon Fuksman (fuksman@mpia.de)
  \date     Aug 13, 2024
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#define MIN_DENSITY 1e-8

static void NormalizeDensity (const Data *d, Grid *g);
#if ROTATING_FRAME == NO
 #define g_OmegaZ  0.0
#endif

/* ********************************************************************* */
void Init (double *us, double x1, double x2, double x3)
/*
 *
 *
 *
 *********************************************************************** */
{
  double r, th, R, z, H, OmegaK, cs;
  double scrh;

  g_gamma = 1.4;

  #if ROTATING_FRAME == YES
  g_OmegaZ  = sqrt(1.0 + g_inputParam[Mplanet]/g_inputParam[Mstar]*CONST_Mearth/CONST_Msun);
  g_OmegaZ *= 2.0*CONST_PI;
  #endif

  #if RADIATION
  g_absorptionCoeff = g_inputParam[DUST_GAS_RATIO]
                    * g_inputParam[DUST_OPACITY]*UNIT_LENGTH*UNIT_DENSITY;
  g_scatteringCoeff = 0.;
  g_radiationConst = 4.*CONST_sigma/CONST_c
										 /(UNIT_VELOCITY*UNIT_VELOCITY)/UNIT_DENSITY ;
                     
  g_idealGasConst = g_inputParam[GasMu]*CONST_amu
                    /CONST_kB*UNIT_VELOCITY*UNIT_VELOCITY ;
  #if RADIATION_NR
  g_radC = CONST_c/UNIT_VELOCITY ;
  g_reducedC = g_inputParam[REDUCED_C]*g_radC ;
  #endif
  #endif
  
#if GEOMETRY == POLAR
  R  = x1;
  #if DIMENSIONS == 2
  z  = 0.0;
  r  = R;
  th = 0.5*CONST_PI;
  #else
  z  = x3;
  r  = sqrt(R*R + z*z);
  th = atan2(R,z);
  #endif
#elif GEOMETRY == SPHERICAL
  r  = x1;
  th = x2;
  R  = r*sin(th);
  z  = r*cos(th);
#endif
  
  H      = 0.05*R;
  OmegaK = 2.0*CONST_PI/(R*sqrt(R));
  cs     = H*OmegaK;
  
  scrh    = (0.5*CONST_PI - th)*r/H;
  us[RHO] = 1.0/(R*sqrt(R))*exp(-0.5*scrh*scrh);
  us[VX1] = us[VX2] = us[VX3] = 0.0;
  us[iVPHI] = R*(OmegaK - g_OmegaZ);
  us[PRS] = us[RHO]*cs*cs;

  #if RADIATION
  us[ENR] = Blackbody(GetTemperature(us[RHO],us[PRS])) ;
  us[FR1] = 0.;
  us[FR2] = 0.;
  us[FR3] = 0.;
  #endif

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

}
/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox *box, int side, Grid *grid) 
/* 
 *
 *********************************************************************** */
{
  int   i, j, k, nv;
  double *x1, *x2, *x3, R, OmegaK, v[256];
  static int do_once = 1;
  
  x1 = grid->x[IDIR];
  x2 = grid->x[JDIR];
  x3 = grid->x[KDIR];

  #if DIMENSIONS == 3
  if (side == 0){
    if (do_once){
      NormalizeDensity(d, grid);
      do_once = 0;
    }
  }
  #endif

  if (side == X1_BEG){
    X1_BEG_LOOP(k,j,i){
      NVAR_LOOP(nv) d->Vc[nv][k][j][i] = d->Vc[nv][k][j][2*IBEG - i - 1];
      d->Vc[VX1][k][j][i] *= -1.0;
      #if GEOMETRY == POLAR
      R = x1[i];
      #elif GEOMETRY == SPHERICAL
      R = x1[i]*sin(x2[j]);
      #endif
      OmegaK = 2.0*CONST_PI/(R*sqrt(R)); 
      d->Vc[iVPHI][k][j][i] = R*(OmegaK - g_OmegaZ);  
      #if RADIATION
      NRAD_LOOP(nv)  d->Vc[nv][k][j][i] = d->Vc[nv][k][j][IBEG];
      if (d->Vc[FR1][k][j][IBEG] > 0.0) {
        d->Vc[FR1][k][j][i] *= -1.0 ;
      }
      #endif
    }
  }

  if (side == X1_END){
    X1_END_LOOP(k,j,i){
      NVAR_LOOP(nv)  d->Vc[nv][k][j][i] = d->Vc[nv][k][j][IEND];
      #if GEOMETRY == POLAR
      R = x1[i];
      #elif GEOMETRY == SPHERICAL
      R = x1[i]*sin(x2[j]);
      d->Vc[iVR][k][j][i]  = 0.0;
      d->Vc[iVTH][k][j][i] = 0.0;
      #endif
      OmegaK = 2.0*CONST_PI/(R*sqrt(R));
      d->Vc[iVPHI][k][j][i] = R*(OmegaK - g_OmegaZ);  
      #if RADIATION
      NRAD_LOOP(nv)  d->Vc[nv][k][j][i] = d->Vc[nv][k][j][IEND];
      if (d->Vc[FR1][k][j][IBEG] < 0.0) {
        d->Vc[FR1][k][j][i] *= -1.0 ;
      }
      #endif
    }
  }

}

/* ************************************************************** */
void NormalizeDensity (const Data *d, Grid *grid)
/*
 *
 * Normalize density and pressure as   rho -> K*rho, where
 *
 *   K = M/(\sum rho*dV)
 *
 **************************************************************** */
{
  int   i, j, k;
  double mc;
        
  mc  = 0.5*g_inputParam[Mdisk]*CONST_Msun;
  mc /= UNIT_DENSITY*UNIT_LENGTH*UNIT_LENGTH*UNIT_LENGTH;
  DOM_LOOP(k,j,i){
    d->Vc[RHO][k][j][i] *= mc;
    #if EOS == IDEAL
    d->Vc[PRS][k][j][i] *= mc;
    #endif
  }
}

#if (BODY_FORCE & VECTOR)
/* ************************************************************************ */
void BodyForceVector(double *v, double *g, double x1, double x2, double x3)
/*
 *
 *
 *
 *************************************************************************** */
{
  g[IDIR] = 0.0;
  g[JDIR] = 0.0;
  g[KDIR] = 0.0;
}
#endif

#if (BODY_FORCE & POTENTIAL)
/* ************************************************************************ */
double BodyForcePotential(double x1, double x2, double x3)
/*
 *
 *
 *
 *************************************************************************** */
{
  double d, R, r, z, th, x, y, phiplanet, rsm;
  double xp, yp, t, phi;

#if GEOMETRY == POLAR
  R  = x1;
  #if DIMENSIONS == 2
  z  = 0.0;
  r  = R;
  th = 0.5*CONST_PI;
  #else
  z  = x3;
  r  = sqrt(R*R + z*z);
  th = atan2(R,z);
  #endif
  
  x  = R*cos(x2);
  y  = R*sin(x2);

#elif (GEOMETRY == SPHERICAL)
  r  = x1;
  th = x2;
  R = r*sin(th);
  z = r*cos(th);
  x = r*sin(th)*cos(x3);
  y = r*sin(th)*sin(x3);
#endif

/* ---------------------------------------------
             planet position
   --------------------------------------------- */

#if ROTATING_FRAME == NO
  double OmegaZ;
  t = g_time;
  if (g_stepNumber == 2) t += g_dt;
  OmegaZ  = sqrt(1.0 + g_inputParam[Mplanet]/g_inputParam[Mstar]*CONST_Mearth/CONST_Msun);
  OmegaZ *= 2.0*CONST_PI;

  xp = cos(OmegaZ*t);
  yp = sin(OmegaZ*t);
#else
  xp = 1.0/sqrt(2.0);  /* initial planet position */
  yp = 1.0/sqrt(2.0); 
#endif

  d = sqrt((x-xp)*(x-xp) + (y-yp)*(y-yp) + z*z);
  rsm = 0.05*R;
  if (d > rsm) phiplanet = g_inputParam[Mplanet]/d;
  else phiplanet = g_inputParam[Mplanet]/d*(pow(d/rsm,4.)-2.*pow(d/rsm,3.)+2.*d/rsm);
  
  phi  = - 4.0*CONST_PI*CONST_PI/g_inputParam[Mstar];
  phi *= (g_inputParam[Mstar]/r + phiplanet*CONST_Mearth/CONST_Msun);

  return phi;
}
#endif
