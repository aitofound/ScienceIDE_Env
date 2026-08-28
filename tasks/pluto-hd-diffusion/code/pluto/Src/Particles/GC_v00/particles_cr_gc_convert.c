/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Initial conversion step for GCA

  Convert the three components of the particle four-velocity 
  to parallel 4-vel, Lorentz factor and magnetic moment.
   
  This function should be called only once, after the main particle 
  initialization from Particles_Init().

  \authors A. Mignone (andrea.mignone@unito.it)

  \b References

  \date   June 01, 2022
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if PARTICLES_CR_GC == YES   /* Compile only if guiding center is enabled. */

#define EPS_B                   1.e-40

/* ********************************************************************* */
void Particles_CR_GC_Convert(Data *data, Grid *grid)
/*!
 * 
 * \param [in,out]  d     Data structure (contains particles list)
 * \param [in]      grid  pointer to Grid structure
 *********************************************************************** */
{ 
  int dir, i, j, k, nfields = 3;
  double ***Fields[nfields], Interpolated_fields[nfields];
  double B[3], b[3], Bmag, Bmag_inv;
  double umag2, gamma, vpar, upar, uperp_mag2, mu;
  const double c2   = PARTICLES_CR_C*PARTICLES_CR_C;
  const double e_mc = PARTICLES_CR_E_MC;
  static double ***w;
  particleNode *CurNode;
  Particle *p;

/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */

  if (w == NULL){
    w = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);
  }

/* --------------------------------------------------------
   1. Define and compute 3D global arrays for
      GC integration
   -------------------------------------------------------- */

  Fields[0] = data->Vc[BX1];
  Fields[1] = data->Vc[BX2];
  Fields[2] = data->Vc[BX3];
 
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

  /* -- 2C.  Compute important quantities in the lab frame -- */
  
    Bmag = sqrt(DOT_PRODUCT(B,B));

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
    gamma  = sqrt(1.0 + umag2/c2);
    upar   = DOT_PRODUCT(p->speed,b);
    uperp_mag2 = umag2 - upar*upar;
    
    mu = 0.5*uperp_mag2*Bmag_inv/c2;  // Missing factor mc/e ??  This is mu0, check...
          
mu = 1.0/e_mc*Bmag_inv*0.5*uperp_mag2;  // Missing factor mc/e ??  This is mu0, check...

    p->speed[IDIR] = upar;
    p->speed[JDIR] = gamma;
    p->speed[KDIR] = mu;
  }
}
#endif /* PARTICLES_CR_GC == YES  */
