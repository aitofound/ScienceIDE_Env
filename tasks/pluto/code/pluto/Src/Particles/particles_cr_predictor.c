/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Predictor step for CR particles
 
  \authors A. Mignone (andrea.mignone@unito.it)\n

  \b References
    - "MAGNETOHYDRODYNAMIC-PARTICLE-IN-CELL METHOD FOR COUPLING COSMIC RAYS 
       WITH A THERMAL PLASMA: APPLICATION TO NON-RELATIVISTIC SHOCKS"\n
       Bai et al., ApJ (2015) 809, 55

  \date  May 08, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if PARTICLES_CR_FEEDBACK == TRUE
/* ********************************************************************* */
void Particles_CR_Predictor(Data *data, double dt, Grid *grid)
/*!
 * Advance particle velocity and speed by dt using an implicit-explicit
 * 1st order scheme.
 * Compute Fcr at F(t+dt), then restore particles into their original
 * position.
 *
 * \param data  [in/out]   PLUTO data array
 * \param dt    [in]       Time step to be achieved
 * \param grid  [in]       pointer to Grid structure
 *
 *********************************************************************** */
{
  static int nparticles = -1;
  int    i, j, k, dir;
  double vg[3], vp[3], B[3], E[3];
  double gamma, gamma_old, u2, um[3], up[3];
  double h, qg, den;
  const double c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  static double ***w, **Ainv;
  particleNode *curNode;
  Particle *p;

/* --------------------------------------------------------
   0. Allocate memory.
   -------------------------------------------------------- */

  if (w == NULL){
    w    = ARRAY_BOX (-1, 1, -1, 1, -1, 1, double);
    Ainv = ARRAY_2D(3,3,double);
  }

/* --------------------------------------------------------
   1. Advance particles with 1st order method
   -------------------------------------------------------- */

  h = dt*PARTICLES_CR_E_MC; 
  PARTICLES_LOOP(curNode, data->PHead){

    p = &(curNode->p);

  /* --------------------------------------------
     1a. Get weights and interpolate electric
         field at particle position.
     -------------------------------------------- */

    Particles_GetWeights(p, p->cell, w, grid);

    E[IDIR] = Particles_Interpolate(data->Ex1, w, p->cell); 
    E[JDIR] = Particles_Interpolate(data->Ex2, w, p->cell); 
    E[KDIR] = Particles_Interpolate(data->Ex3, w, p->cell);

  /* --------------------------------------------
     1b. Kick step: evolve four-velocity using
         electric field only:

         um^* = u^n + h/2*(cE)^n
     -------------------------------------------- */
   
    u2 = DOT_PRODUCT(p->speed, p->speed);
    gamma_old = sqrt(1.0 + u2/c2);
    for (dir = 0; dir < 3; dir++) {
      um[dir] = p->speed[dir] + h*E[dir];
    }
    u2    = DOT_PRODUCT(um, um);
    gamma = sqrt(1.0 + u2/c2);

  /* -- 1c. Save initial coordinates and 4-velocities

            p->coord_old = x^n
            p->speed_old = v^n

            and update spatial position: x^* = x^n + dt*vm^ -- */
  
    for (dir = 0; dir < 3; dir++){
      p->coord_old[dir] = p->coord[dir];
      p->speed_old[dir] = p->speed[dir];
      p->coord[dir]    += dt*um[dir]/gamma;
    }

  /* -- 1d. Get weights and magnetic field at x^* -- */

    Particles_GetWeights(p, p->cell, w, grid);  

    B[IDIR] = Particles_Interpolate(data->Vc[BX1], w, p->cell); 
    B[JDIR] = Particles_Interpolate(data->Vc[BX2], w, p->cell); 
    B[KDIR] = Particles_Interpolate(data->Vc[BX3], w, p->cell);

  /* -- 1e. Rotate: add magnetic field contribution -- */

    B[IDIR] *= h/gamma;
    B[JDIR] *= h/gamma;
    B[KDIR] *= h/gamma;

    den = 1.0 + B[IDIR]*B[IDIR] + B[JDIR]*B[JDIR] + B[KDIR]*B[KDIR];

    Ainv[IDIR][IDIR] =      1.0 + B[IDIR]*B[IDIR];
    Ainv[IDIR][JDIR] =  B[KDIR] + B[IDIR]*B[JDIR];
    Ainv[IDIR][KDIR] = -B[JDIR] + B[IDIR]*B[KDIR];

    Ainv[JDIR][IDIR] = -B[KDIR] + B[JDIR]*B[IDIR];
    Ainv[JDIR][JDIR] =      1.0 + B[JDIR]*B[JDIR];
    Ainv[JDIR][KDIR] =  B[IDIR] + B[JDIR]*B[KDIR];

    Ainv[KDIR][IDIR] =  B[JDIR] + B[KDIR]*B[IDIR];
    Ainv[KDIR][JDIR] = -B[IDIR] + B[KDIR]*B[JDIR];
    Ainv[KDIR][KDIR] =      1.0 + B[KDIR]*B[KDIR];

  /* -- 1f. Advance coordinate and velocity by dt: -- */

    for (dir = 0; dir < 3; dir++){
      up[dir]  = DOT_PRODUCT(Ainv[dir],um);
      up[dir] /= den;
    }
    u2    = DOT_PRODUCT(up,up);
    gamma = sqrt(1.0 + u2/c2);

    for (dir = 0; dir < 3; dir++){
      p->speed[dir] = up[dir];
      p->coord[dir] = p->coord_old[dir] + dt*p->speed_old[dir]/gamma_old;
    }
  }
  
/* --------------------------------------------------------
   2. Compute Fcr(x^{n+1/2}, v^{n+1/2})
   -------------------------------------------------------- */

  Particles_CR_ComputeForce(data->Vc, data, grid);
  
/* --------------------------------------------------------
   3. Restore coordinates and velocities
   -------------------------------------------------------- */

  PARTICLES_LOOP(curNode, data->PHead){
    p = &(curNode->p);
    for(dir = 0; dir < 3; dir++) {
      p->speed[dir] = p->speed_old[dir];
      p->coord[dir] = p->coord_old[dir];
    }
  }
}
#endif /* PARTICLES_CR_FEEDBACK == TRUE */
