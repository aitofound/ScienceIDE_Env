#include <math.h>

#include "baseTypes.h"
#include "global.h"
#include "configuration.h"

/* Mean Free Path in AU, scaled by a power of the radial distance. 

  This function exists primarily as a helper for `meanFreePath`, but it may be
  useful in cases when the caller needs to explicitly pass a radial distance.
*/
Scalar_t meanFreePathR(Index_t species, Index_t energy, Scalar_t rmag)
{
  return rigidity[idx_se(species, energy)] * config.lamo * pow(rmag*config.rScale, config.mfpPower);
}


/* Mean Free Path in AU, scaled by the inverse of magnetic field magnitude.

  This function exists primarily as a helper for `meanFreePath`, but it may be
  useful in cases when the caller needs to explicitly pass a magnetic-field
  magnitude.
*/
Scalar_t meanFreePathB(Index_t species, Index_t energy, Scalar_t Bmag)
{
  return rigidity[idx_se(species, energy)] * config.lamo * pow(config.mhdBsAu/Bmag, 0.5*config.mfpPower);
}


/* Mean Free Path in AU.

  The value of `config.mfpInverseB` determines how to scale the reference mean
  free path: A value of 0 causes this routine to scale the reference value by a
  power (`config.mfpPower`) of the radial distance. A value of 1 causes
  this routine to scale the reference value by the inverse of the magnetic-field
  magnitude.
*/
Scalar_t meanFreePath(Index_t species, Index_t energy, Node_t node)
{

  if (config.mfpInverseB == 1) {
    return meanFreePathB(species, energy, node.mhdBmag);
  } else {
    return meanFreePathR(species, energy, node.rmag);
  }

} /* END meanFreePath */


/* Compute current values of the particle mean free path.

  NB: This requires updated values of MHD.
*/
void updateMFP(void)
{
  Index_t face, row, col, shell, species, energy;
  Node_t node;

  for (face=0; face<NUM_FACES; face++) {
    for (row=0; row<FACE_ROWS; row++) {
      for (col=0; col<FACE_COLS; col++) {
        for (shell=INNER_SHELL; shell<LOCAL_NUM_SHELLS; shell++) {
          node = grid[idx_frcs(face,row,col,shell)];
          for (species=0; species<NUM_SPECIES; species++) {
            for (energy=0; energy<NUM_ESTEPS; energy++) {
              lambdaPara[idx_frcsspe(face,row,col,shell,species,energy)] = meanFreePath(species, energy, node);
            }
          }
        }
      }
    }
  }

} /* END updateMFP */

