/*!
 \file
 \brief Initialize test particles for the Xpoint problem.
 
 Particles are initialized using both global and local (cell-by-cell)
 methods.
 Velocities are set to follow a Maxwellian distribution with
 <em> sigma = 0.1*c/100.0 </em>.
 
 \authors A. Mignone (andrea.mignone@unito.it)\n
 \date    June 13, 2019
 */
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Particles_Init(Data *d, Grid *grid)
/*!
 *  \param [in]    d       Pointer to the PLUTO data structure.
 *  \param [in]    grid    Pointer to the PLUTO grid structure.
 *
 *********************************************************************** */
{
  int i,j,k, np, dir;
  int np_glob = RuntimeGet()->Nparticles_glob;
  int np_cell = RuntimeGet()->Nparticles_cell;
  double xbeg[3], xend[3], vp2, vp, mu, sigma;
  double gamma, c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  Particle p;

  mu    = 0.0;
  sigma = 0.1;

/* ------------------------------------------------------
   Seed random number sequence with the same seed,
   since all procs will loop thorugh all particles
   ------------------------------------------------------ */

  RandomSeed(0,1024*32768);

/* ------------------------------------------------------
   1. GLobal initialization
   ------------------------------------------------------ */

  if (np_glob > 0){  
    print ("! Particles_Init(): use cell by cell initialization\n");
    QUIT_PLUTO(1);
  }

/* ------------------------------------------------------
   2. Local initialization
   ------------------------------------------------------ */

  if (np_cell > 0){
    DOM_LOOP(k,j,i){

      xbeg[IDIR] = grid->xl[IDIR][i]; xend[IDIR] = grid->xr[IDIR][i];
      xbeg[JDIR] = grid->xl[JDIR][j]; xend[JDIR] = grid->xr[JDIR][j];
      xbeg[KDIR] = grid->xl[KDIR][k]; xend[KDIR] = grid->xr[KDIR][k];

    /* -- Loop on particles -- */
  
      for (np = 0; np < np_cell; np++){
        
        Particles_LoadUniform(np, np_cell, xbeg, xend, p.coord);
        p.speed[IDIR] = GaussianRandomNumber(mu, sigma);
        p.speed[JDIR] = GaussianRandomNumber(mu, sigma);
        p.speed[KDIR] = GaussianRandomNumber(mu, sigma);

        p.speed[IDIR] *= PARTICLES_CR_C/100.0;
        p.speed[JDIR] *= PARTICLES_CR_C/100.0;
        p.speed[KDIR] *= PARTICLES_CR_C/100.0;

        vp2 = DOT_PRODUCT(p.speed,p.speed);
        vp  = sqrt(vp2);
        if (vp > PARTICLES_CR_C){
          print ("! vp = %12.6e > C = %12.6e \n",vp, PARTICLES_CR_C);
          QUIT_PLUTO(1);
        }

        gamma = 1.0/sqrt(1.0 - vp2/c2);
        p.speed[IDIR] *= gamma;
        p.speed[JDIR] *= gamma;
        p.speed[KDIR] *= gamma;
 
        p.color = 1.0; 
//double R = sqrt(p.coord[IDIR]*p.coord[IDIR] + p.coord[JDIR]*p.coord[JDIR]);
//if (R < 1.1e3 && R > 9.9e2)  
         Particles_Insert (&p, d, PARTICLES_CREATE, grid);
      }
    }
  }

  Particles_SetID(d->PHead);
}

/* ********************************************************************* */
void Particles_Inject(Data *data, Grid *grid)
/*!
 *  Inject particles as you wish.
 *
 *  \param [in]  data    Pointer to the PLUTO data structure.
 *  \param [in]  grid    Pointer to the PLUTO grid structure.
 *********************************************************************** */
{
}

/* ********************************************************************* */
void Particles_UserDefBoundary(Data *d, int side, Grid *grid)
/*
 *
 *********************************************************************** */
{
  int    dir;
  double xbeg[3], xend[3];
  particleNode *curr = d->PHead, *next;
  Particle *p;
  
  for (dir = 0; dir < 3; dir++) {
    xbeg[dir] = grid->xbeg_glob[dir];
    xend[dir] = grid->xend_glob[dir];
  }

  if (side == X1_BEG){
    dir = IDIR;
    while (curr != NULL){
      p    = &(curr->p);  
      next = curr->next; 
      if (p->coord[dir] < xbeg[dir]) Particles_Destroy (curr,d);
      curr = next;
    }
  }

  if (side == X1_END){
    dir = IDIR;
    while (curr != NULL){
      p    = &(curr->p);  
      next = curr->next; 
      if (p->coord[dir] > xend[dir]) Particles_Destroy (curr,d);
      curr = next;
    }
  }

  if (side == X2_BEG){
    dir = JDIR;
    while (curr != NULL){
      p    = &(curr->p);  
      next = curr->next; 
      if (p->coord[dir] < xbeg[dir]) Particles_Destroy (curr,d);
      curr = next;
    }
  }

  if (side == X2_END){
    dir = JDIR;
    while (curr != NULL){
      p    = &(curr->p);  
      next = curr->next; 
      if (p->coord[dir] > xend[dir]) Particles_Destroy (curr,d);
      curr = next;
    }
  }

  if (side == X3_BEG){
    dir = KDIR;
    while (curr != NULL){
      p    = &(curr->p);  
      next = curr->next; 
      if (p->coord[dir] < xbeg[dir]) Particles_Destroy (curr,d);
      curr = next;
    }
  }

  if (side == X3_END){
    dir = KDIR;
    while (curr != NULL){
      p    = &(curr->p);  
      next = curr->next; 
      if (p->coord[dir] > xend[dir]) Particles_Destroy (curr,d);
      curr = next;
    }
  }
}
