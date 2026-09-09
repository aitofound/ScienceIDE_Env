/* The Earth-Moon-Mars Radiation Environment Module (EMMREM) software is */
/* free software; you can redistribute and/or modify the EMMREM sotware */
/* or any part of the EMMREM software under the terms of the GNU General */
/* Public License (GPL) as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any later */
/* version. Software that uses any portion of the EMMREM software must */
/* also be released under the GNU GPL license (version 2 of the GNU GPL */
/* license or a later version). A copy of this GNU General Public License */
/* may be obtained by writing to the Free Software Foundation, Inc., 59 */
/* Temple Place, Suite 330, Boston MA 02111-1307 USA or by viewing the */
/* license online at http://www.gnu.org/copyleft/gpl.html. */

#include <stdio.h>
#include <math.h>
#include <float.h>

#include "global.h"
#include "configuration.h"
#include "energeticParticles.h"
#include "energeticParticlesBoundary.h"
#include "cubeShellStruct.h"
#include "simCore.h"
#include "flow.h"
#include "error.h"


/* Boundary Condition on a Streamline. Independent of mu, currently. */
void EPBoundaryData(Index_t species, Index_t energy, Scalar_t rmag, Scalar_t *dist, Index_t shellIndex)

{

  if (  (config.useBoundaryFunction > 0) && (shellIndex <= 2) )
  {

    *dist = sepSeedFunction(egrid[energy], rmag);

  }

} /* END EPBoundaryData */


void seedPopulationCheck(Index_t iterIndex)
{

  Index_t workIndex, shell, species, energy, mu;

  Scalar_t distFunction;


  workIndex = mpi_rank + N_PROCS * iterIndex;

  if (workIndex < NUM_STREAMS) {

    for (shell = 0; shell < TOTAL_NUM_SHELLS; shell++) {
      for (species = 0; species < NUM_SPECIES; species++) {
        for (energy = 0; energy < NUM_ESTEPS; energy++) {

          distFunction = sepSeedFunction( egrid[energy], streamGrid[shell].rmag);
          for (mu = 0; mu < NUM_MUSTEPS; mu++) {
            if (ePartsStream[idx_sspem(shell,species,energy,mu)] < distFunction) {
              ePartsStream[idx_sspem(shell,species,energy,mu)] = distFunction;
            }
          }

        }
      }
    }

  }

}
/* END seedPopulationCheck */


Scalar_t sepSeedFunction(Scalar_t energy, Scalar_t r)
{

  Scalar_t normJ;
  Scalar_t normJ0;
  Scalar_t normRadius, radialTerm;
  Scalar_t normE0, powerLawTerm;
  Scalar_t normEc, expTerm;
  Scalar_t normf;

  const double one   = 1.0;                // 1.0 with appropriate precision
  const double two   = 2.0;                // 2.0 with appropriate precision
  const double Enorm = MEV / (MP * C * C); // Energy-normalization factor

  /* Define local variables corresponding to runtime options. */
  Scalar_t J0     = config.boundaryFunctAmplitude;
  Scalar_t beta   = config.boundaryFunctBeta;
  Scalar_t r0     = config.boundaryFunctR0;
  Scalar_t gamma  = config.boundaryFunctGamma;
  Scalar_t E0     = config.boundaryFunctE0;
  Scalar_t Ec     = config.boundaryFunctEcutoff;
  Scalar_t rScale = config.rScale;

  /* Define normalized variables. */
  normJ0     = J0 * (MP * C) / (MHD_DENSITY_NORM * MEV);
  normRadius = r0 / rScale;
  normE0     = E0 * Enorm;
  normEc     = Ec * Enorm;

  /* Define terms in the analytic spectrum. */
  radialTerm   = pow( (r / normRadius), -beta );  // radial power-law dependence
  powerLawTerm = pow( (energy / normE0), -gamma); // energetic power-law below Ec
  expTerm      = exp( -(energy / normEc) );       // exponential fall-off above Ec

  /* Compute the flux spectrum. */
  normJ = normJ0 * radialTerm * powerLawTerm * expTerm;

  /* Convert flux to a distribution function. */
  normf = normJ / (two * energy);

  /* Floor the distribution at DBL_MIN, to prevent log(f) = -inf. */
  if (normf < DBL_MIN) normf = DBL_MIN;

  return normf;

} /* END sepSeedFunction */





