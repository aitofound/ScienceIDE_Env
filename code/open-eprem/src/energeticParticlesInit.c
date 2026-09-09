/*-------------------------------------------------
 -- EMMREM: energeticParticlesInit.c
 --
 -- Utilities for energetic particle functionality.
 --
 --     initEnergeticParticles( void ) : set initial distributions.
 --
 -- _________________CHANGE HISTORY_____________
 -- _____________END_CHANGE HISTORY_____________
 --------------------------------------------------*/

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

#include <math.h>
#include <float.h>
#include "global.h"
#include "configuration.h"
#include "energeticParticlesInit.h"
#include "energeticParticlesTypes.h"
#include "energeticParticlesBoundary.h"
#include "error.h"

/* Define the energy grid and EP attributes */
void initEnergeticParticlesGrids(void)
{

  Index_t species, energy, mu;
  Scalar_t kin, p, mup, mum;
  Index_t face, row, col, shell;
  const double one = 1.0;
  const double two = 2.0;
  const double half = 0.5;

  /*  T = Kin. Energy/( A * mp c^2 )                          -- */
  /*  p = momentum / ( A * mp * c )                           -- */
  /*  lnp(i) = lnpmin + dlnp*energy                           -- */
  /* kin = config.charge[species]*config.eMin*MEV/(config.mass[species]*MP*C*C); */
  /* reset the config.eMin and config.eMax for Energy/nucleon */
  kin = config.eMin * MEV/(MP*C*C);
  lnpmin = log(sqrt(kin*kin+two*kin));
  kin = config.eMax * MEV/(MP*C*C);
  lnpmax = log(sqrt(kin*kin+two*kin));
  dlnp = (lnpmax - lnpmin)/(NUM_ESTEPS - 1.0);

  for (energy = 0; energy < NUM_ESTEPS; energy++) {
    p = exp(lnpmin + energy * dlnp);
    pgrid[energy]    = p;
    egrid[energy]    = sqrt(one+p*p) - one;
    if (mpi_rank == 0) {
      printf("energy[%d] = %e MeV\n", energy, egrid[energy]*MP*C*C/MEV);
    }
    vgrid[energy]    = p/sqrt(one+p*p);
  }

  for (species =0; species < NUM_SPECIES; species++) {
    for (energy = 0; energy  < NUM_ESTEPS; energy++) {
      p = exp(lnpmin + energy * dlnp);
      rigidity[idx_se(species,energy)] = (config.mass[species]/config.charge[species]) * p * MZERO;
      rigidity[idx_se(species,energy)] = pow(rigidity[idx_se(species,energy)], config.rigidityPower);
    }
  }

 /* The mu grid points are on the mu cell centers. */
  dmu = two / (one * NUM_MUSTEPS);
  for (mu = 0; mu < NUM_MUSTEPS ; mu++) {
    mup = -one + (mu+one)*dmu;
    mum = -one + mu*dmu;
    mugrid[mu] = half*(mup+mum);
  }

  /*-- 4d loop for every node in every shell. --*/
  for (face  = 0;              face  < NUM_FACES;  face++ ) {
    for (row   = 0;              row   < FACE_ROWS;  row++ ) {
      for (col   = 0;              col   < FACE_COLS;  col++ ) {
        for (shell = INNER_SHELL ;   shell < LOCAL_NUM_SHELLS; shell++ ) {
          for (species = 0;       species < NUM_SPECIES;  species++ ) {
            for (energy  = 0;       energy  < NUM_ESTEPS;   energy++ ) {
              for (mu      = 0;       mu      < NUM_MUSTEPS;  mu++ ) {
                eParts[idx_frcsspem(face,row,col,shell,species,energy,mu)] = DBL_MIN;
              }
            }
          }
        }
      }
    }
  }

} /* END initEnergeticParticlesGrids */

/* Define EP attributes on grid */
void initEnergeticParticles(void)
{

  Index_t species, energy, mu;
  Index_t face, row, col, shell;
  Scalar_t mass, abundance;

  /* initializes all node points to the VS distribution */

  /*-- 4d loop for every node in every shell. --*/
  for (face  = 0;              face  < NUM_FACES;  face++  ) {
    for (row   = 0;              row   < FACE_ROWS;  row++   ) {
      for (col   = 0;              col   < FACE_COLS;  col++   ) {
        for (shell = INNER_SHELL ;   shell < LOCAL_NUM_SHELLS; shell++ ) {

          /*-- 3d loop for every species/energy/mu --*/
          for (species = 0;       species < NUM_SPECIES;  species++  ) {
            mass      = config.mass[species];
            abundance = config.abundance[species];
            for (energy  = 0;       energy  < NUM_ESTEPS;   energy++   ) {
              for (mu      = 0;       mu      < NUM_MUSTEPS;  mu++   ) {

                if ((config.useBoundaryFunction > 0) &&
                    (config.boundaryFunctionInitDomain > 0)) {

                  eParts[idx_frcsspem(face,row,col,shell,species,energy,mu)] =
                  abundance * sepSeedFunction(egrid[energy] / mass, grid[idx_frcs(face,row,col,shell)].rmag);

                }
              }
            }
          }
        }
      }
    }
  }



} /* END initEnergeticParticles */

