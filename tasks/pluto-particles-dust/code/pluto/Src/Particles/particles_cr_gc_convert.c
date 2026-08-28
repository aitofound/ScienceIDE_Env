/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Initial conversion step for GCA

  Convert the three components of the particle four-velocity
  to parallel 4-vel, Lorentz factor and magnetic moment.

  p->speed[IDIR] = upar;
  p->speed[JDIR] = gamma;
  p->speed[KDIR] = mu;

  This function should be called only once, after the main particle
  initialization from Particles_Init().

  \authors A. Mignone (andrea.mignone@unito.it)

  \b References

  \date   Nov 14, 2022
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if PARTICLES_CR_GC == YES   /* Compile only if guiding center is enabled. */

#define EPS_B                   1.e-40
extern double ***gc_cE[3];

/* ********************************************************************* */
void Particles_CR_GC_Convert(Data *data, Grid *grid)
/*!
 *
 * \param [in,out]  d     Data structure (contains particles list)
 * \param [in]      grid  pointer to Grid structure
 *********************************************************************** */
{
  int dir, i, j, k, nfields = 6;
  double ***Fields[nfields], Interpolated_fields[nfields];
  double B[3], cE[3], b[3];
  double Bmag, Emag, Bmag_inv;
  double umag2, omega, gamma, vpar, upar, uperp_mag2, mu;
  const double inv_c  = 1.0/PARTICLES_CR_C;
  const double inv_c2 = inv_c*inv_c;
  const double e_mc  = PARTICLES_CR_E_MC;
  static double ***w;
  particleNode *CurNode;
  Particle *p;

/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */

  if (w == NULL){
    w = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);
  }

  Particles_CR_setGC (data, 0.0, grid);

/* --------------------------------------------------------
   1. Define and compute 3D global arrays for
      GC integration
   -------------------------------------------------------- */

  Fields[0] = data->Vc[BX1];
  Fields[1] = data->Vc[BX2];
  Fields[2] = data->Vc[BX3];
  for (dir = 0; dir < 3; dir++) Fields[dir +  3] = gc_cE[dir];

/* --------------------------------------------------------
   2. Convert particle velocity into parallel velocity
       and magnetic moment. These will be used onwards.
   -------------------------------------------------------- */

  print ("> Particles_CR_Update(): converting particle speed to momentum\n");

  PARTICLES_LOOP(CurNode, data->PHead){
    p = &(CurNode->p);

  /* -- 2A. Compute weights and indices at time n, check if
            particle is in the very last ghost zone -- */

    Particles_GetWeights(p, p->cell, w, grid);
    i = p->cell[IDIR];
    j = p->cell[JDIR];
    k = p->cell[KDIR];

  /* -- 2B. Interpolate electromagnetic fields at time n -- */

    Particles_InterpolateArr(Fields, nfields, w, p->cell, Interpolated_fields);
    for (dir = 0; dir < 3; dir++) B[dir]  = Interpolated_fields[dir];
    for (dir = 0; dir < 3; dir++) cE[dir] = Interpolated_fields[dir];

  /* -- 2C.  Compute important quantities in the lab frame -- */

    Bmag = sqrt(DOT_PRODUCT(B,B));
    Emag = sqrt(DOT_PRODUCT(cE,cE))*inv_c2;

  /* -- Initial magnetic moment can't be 0 -- */

    if(Bmag < EPS_B) {
      printLog("! Particles_CR_Update(): initial B is zero for particle %d.\n", p->id);
      Particles_CR_destroyGCParticle(p, CurNode, data);
      continue;
    }

    Bmag_inv = 1.0/(Bmag + EPS_B);
    b[IDIR] = B[IDIR]*Bmag_inv;
    b[JDIR] = B[JDIR]*Bmag_inv;
    b[KDIR] = B[KDIR]*Bmag_inv;

  /* -- 2D. u is 4-velocity -- */

    umag2  = DOT_PRODUCT(p->speed,p->speed);
    gamma  = sqrt(1.0 + umag2*inv_c2);
    upar   = DOT_PRODUCT(p->speed,b);
    uperp_mag2 = umag2 - upar*upar;

    #if PARTICLES_CR_GC_FULL_OMEGA == YES /* Complete omega expression (Eq. A2) */
    double EB = DOT_PRODUCT(cE, B)*inv_c;
    double scrh = Bmag*Bmag - Emag*Emag;
    omega = e_mc*sqrt(0.5*scrh + 0.5*sqrt(scrh*scrh + 4.*(EB*EB)));
    #else
    omega = e_mc*Bmag;
    #endif
    mu = 0.5*uperp_mag2/omega;

    p->speed[IDIR] = upar;
    p->speed[JDIR] = gamma;
    p->speed[KDIR] = mu;
  }
}
#endif /* PARTICLES_CR_GC == YES  */
