/* ///////////////////////////////////////////////////////////////////// */
/*!
 \file
 \brief Placing the particles on the grid and interpolating dervied quantites 
        at particle positions.
 
 \authors   A. Mignone (andrea.mignone@unito.it)\n
            B. Vaidya (bvaidya@unito.it)\n
            D. Mukherjee
            
 \date     Nov 16, 2022
 */
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void Particles_Set(Data *d, Grid *grid)
/*!
 * Initialize particles on the grid and distribute them among
 * processors.
 *
 *  \param [in]    d     Pointer to the PLUTO data structure.
 *  \param [in]    grid  Pointer to the PLUTO grid structure.
 *
 *********************************************************************** */
{ 
#if PARTICLES == PARTICLES_LP
  char particles_type[32] = "LAGRANGIAN";
#elif PARTICLES == PARTICLES_CR
  char particles_type[32] = "COSMIC_RAYS";
#elif PARTICLES == PARTICLES_DUST
  char particles_type[32] = "DUST";  
#endif

  print ("> Particles_Set():\n");

/* ----------------------------------------------------------
   1. Define MPI data type (not strictly necessary) 
   ---------------------------------------------------------- */ 

#ifdef PARALLEL
  Particles_StructDatatype();
#endif  
  p_nrestart = 0;
  
/* ----------------------------------------------------------
   2. Initialize particles on grid
   ---------------------------------------------------------- */ 

  p_nparticles   = 0;
  Particles_Init(d, grid);
  #if PARTICLES == PARTICLES_CR && PARTICLES_CR_GC == YES
  Particles_CR_GC_Convert(d,grid);
  #endif

/* ----------------------------------------------------------
   3. Do some printing
   ---------------------------------------------------------- */ 

  print ("  particles type: %s\n",particles_type); 
  #if (PARTICLES == PARTICLES_CR) && (PARTICLES_CR_GC == YES)
  print ("                 [GC Enabled, TIME_STEPPING = %d]\n", 
          PARTICLES_CR_GC_TIME_STEPPING);
  #else
  print ("                 [TIME_STEPPING = Boris]\n");
  #endif
  print ("  number of particles created [local]: %d\n\n",p_nparticles);  

#if PARTICLES == PARTICLES_LP
  particleNode *CurNode = d->PHead;
  Particle *pl;
  particleProbe probe;
  #if PARTICLES_LP_SPECTRA == YES
  int e;
  
  PARTICLES_LOOP(CurNode, d->PHead){
    pl = &(CurNode->p);
    Particles_LP_FixValue(pl, &probe, d, grid);
    for (e = 0; e < PARTICLES_LP_NEBINS; e++){
       pl->chi[e] /= pl->density; /* Chi is a fraction and not number density */
    }
  }
  #endif
#endif

#if PARTICLES_USE_ARRAY == YES                                    
    Particles_ListToArray(d);                                     
 #endif

#ifdef PARALLEL 
  MPI_Barrier(MPI_COMM_WORLD);
#endif
}
