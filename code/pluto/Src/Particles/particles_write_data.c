/* ///////////////////////////////////////////////////////////////////// */
/*!
 \file
 \brief Main driver of particles output data called during the CheckForOutput
        stage.
  
 \authors B. Vaidya (bvaidya@unito.it)\n
          A. Mignone (andrea.mignone@unito.it)\n
  
 \date   Aug 20, 2020
 */
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ************************************************************************* */
void Particles_WriteData(Data *d, Output *output, Grid *grid)
/*!
 *  Main driver of particles output data.
 *   
 *  \param[in]    d         Pointer to the PLUTO data structure.
 *  \param[in]    output    Pointer to the PLUTO Output strcuture.
 *
 **************************************************************************** */
{
  char filename[512];   
  time_t tbeg, tend;

/* ----------------------------------------------
   1. Make sure all particles lie in the active
      computational zones.
      This will be essential for restarting.
   ---------------------------------------------- */

  Particles_Boundary(d, grid);
  Particles_BoundaryExchange(d, grid);

/* ----------------------------------------------
   2. Call the appropriate writing function.
   ---------------------------------------------- */

  output->nfile++;  /* Increment file output number */

  sprintf (filename,"%s/particles.%04d.%s", output->dir, output->nfile,
                                            output->ext);
  print ("> Writing particle file #%d (%s) to disk...\n", 
             output->nfile, output->ext);

#ifdef PARALLEL
  MPI_Barrier (MPI_COMM_WORLD);
  if (prank == 0) time(&tbeg);
#endif

  if (   output->type == PARTICLES_DBL_OUTPUT
      || output->type == PARTICLES_FLT_OUTPUT) { 

    Particles_WriteBinary(d->PHead, 1.0/d->Dts->invDt_particles,
                          output, filename);

  }else if (output->type == PARTICLES_VTK_OUTPUT) { 

    Particles_WriteVTK(d->PHead, output, filename);

  }else if (output->type == PARTICLES_TAB_OUTPUT) { 

    Particles_WriteTab(d->PHead, filename);
    
  }
  
#ifdef PARALLEL
  MPI_Barrier (MPI_COMM_WORLD);
  if (prank == 0){
    time(&tend);
    print ("  [%5.2f sec ]\n",difftime(tend,tbeg));
  }
#endif

}
