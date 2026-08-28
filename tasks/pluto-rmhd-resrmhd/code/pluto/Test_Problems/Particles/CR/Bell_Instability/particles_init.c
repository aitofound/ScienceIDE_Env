/* ///////////////////////////////////////////////////////////////////// */
/*!
 \file
 \brief Initialize particles for the Bell instability test-
 
 \authors A. Mignone (andrea.mignone@unito.it)\n
 
 \date    Aug 17, 2020
 
 \b References: \n
    - [MVBM18] "A PARTICLE MODULE FOR THE PLUTO CODE: I - AN IMPLEMENTATION OF
                THE MHD-PIC EQUATIONS", Mignone etal.ApJS (2018)  [ Sec. 4.4 ]
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"
#include "rotate.h"

#ifndef LINEAR_SETUP
 #define LINEAR_SETUP TRUE
#endif

/* ********************************************************************* */
void Particles_Init(Data *d, Grid *grid)
/*!
 *  Sets initial conditions on particles.
 *
 *  \param [in]    d       Pointer to the PLUTO data structure.
 *  \param [in]    grid    Pointer to the PLUTO grid structure.
 *
 *********************************************************************** */
{
  int i,j,k, np, dir;
  int np_glob = RuntimeGet()->Nparticles_glob;
  int np_cell = RuntimeGet()->Nparticles_cell;
  double xbeg[3], xend[3], vbeg[3], vend[3];
  double gamma, c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  Particle p;

/* --------------------------------------------------------------
    Global initialization
   -------------------------------------------------------------- */

  if (np_glob > 0){
    print ("! Use cell-by-cell initialization\n");
    QUIT_PLUTO(1);
  }

/* --------------------------------------------------------------
    Cell by cell initialization
   -------------------------------------------------------------- */

  if (np_cell > 0){
    static int first_call = 1;
    double rho_p = 2.e6*g_inputParam[EPSILON]/np_cell;
    double uCR = 1.0/g_inputParam[EPSILON];  /* = vA/epsilon */
    double Jcr = PARTICLES_CR_E_MC*rho_p*uCR;   /* = Jcr/c      */ 

    DOM_LOOP(k,j,i){

      xbeg[IDIR] = grid->xl[IDIR][i]; xend[IDIR] = grid->xr[IDIR][i];
      xbeg[JDIR] = grid->xl[JDIR][j]; xend[JDIR] = grid->xr[JDIR][j];
      xbeg[KDIR] = grid->xl[KDIR][k]; xend[KDIR] = grid->xr[KDIR][k];

  /* -- Loop on particles -- */
  
      for (np = 0; np < np_cell; np++){
        Particles_LoadUniform(np, np_cell, xbeg, xend, p.coord);
        p.speed[IDIR] = uCR;
        p.speed[JDIR] = 0.0;
        p.speed[KDIR] = 0.0;

      /* -- Assign four-velocity -- */

        gamma = DOT_PRODUCT(p.speed, p.speed)/c2;
        gamma = 1.0/sqrt(1.0 - gamma);
        p.speed[IDIR] *= gamma;
        p.speed[JDIR] *= gamma;
        p.speed[KDIR] *= gamma;

        p.mass        = rho_p*grid->dV[k][j][i];
        p.color       = rho_p;
        #if LINEAR_SETUP == TRUE
        RotateVector(p.speed, -1);
        #endif

        Particles_Insert (&p, d, PARTICLES_CREATE, grid);
      }
    }

    if (first_call){
      double lambda = 4.0*CONST_PI/Jcr;
      double R;

      R = PARTICLES_CR_E_MC*rho_p;
      R = R/(R + PARTICLES_CR_E_MC_GAS);
      print ("> Particles_Init():\n");
      print ("  ------------------------------------ \n");
      print ("  normal               = [%f, %f, %f]\n", p.speed[IDIR]/uCR,
                                                        p.speed[JDIR]/uCR,
                                                        p.speed[KDIR]/uCR);
      print ("  gyration radius      = %8.3e\n", uCR/PARTICLES_CR_E_MC);
      print ("  most unstable lambda = %f  (k = %f)\n", lambda, 2.0*CONST_PI/lambda);
      print ("  R                    = %8.3e\n", R);
      print ("  Lambda               = %8.3e\n", R/g_inputParam[EPSILON]);
      print ("  rho (CR)             = %8.3e\n", rho_p);
      print ("  ------------------------------------ \n");
    }
  }

  Particles_SetID(d->PHead);
}
/* ********************************************************************* */
void Particles_Inject(Data *data, Grid *grid)
/*!
 *  Sets user-defined boundary conditions on particles.
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
