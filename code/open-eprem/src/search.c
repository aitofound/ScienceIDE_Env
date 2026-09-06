/*-----------------------------------------------
 -- EMMREM: search.c
 --
 -- Grid searching utilities.
 --
 -- A search method based on recursive depth-first graph traversal of the
 -- cube grid stucture as if it was one continuous graph (without concern for
 -- the fact that the cube grid structure is distributed over N_PROCS procs.)
 -- The result is a list of cube-grid nearest-neighbors of each
 -- observer, equally spaced around the grid node whose shell spatial position
 -- is nearest the observer's spatial position. The span of this list of
 -- neighbors is controlled by the parameter MAX_SEARCH_DEPTH. With a sufficient
 -- depth parameter, the list of cube-grid nodes should house shell-grid vertices
 -- that spatially surround the observer's position. The search is repeated
 -- until the observer's nearest-neighbor vertex (NN) does not change between
 -- one search and the next. Thus, although the search depth is fixed, the
 -- repeated searching causes the search to continually move until the actual
 -- NN is found, no matter how remote that vertex might be from the starting
 -- location of the search (the original NN at the start of the search.)
 -- (See findObserversNNandNNlists(). )
 --
 -- This module implements a general, procedure-call/return state machine that
 -- allows for virtual procedures independent of procs.  Calls are done via
 -- messages that spawn tasks executed by the state machine. The state machine
 -- acts as a local (to a proc) sequential finite-state machine execution engine,
 -- with interrupt handling. The interrupts are messages, each of which spawns a
 -- task to be executed locally. Messages are requests for action by the
 -- spawned task, and are sent either locally or to remote procs without
 -- distinction. Requests carry parameters using copy-in/copy-out semantics.
 -- In effect, request parameters are global variables in any call chain. Tasks
 -- also have memory in the form of individual fields. These are equivalent to
 -- local variables in C. (However, there are no automatic variables, nor any
 -- stack or heap memory available for procedures to use for data. Rather, the
 -- local variables are predefined by the Task_t struct.) All procs run the same
 -- state machine simultaneously. Each state machine maintains its own stack of
 -- currently in-execution local tasks. The last task on the stack is an idle
 -- task. Requests carry a copy of a local observer structure. This structure
 -- contains a "currTarget", which is the grid node to be visited. A request to
 -- to call VISIT causes the target to be added to the observer's nearest-
 -- neighbors list, along with the target's current spatial position, r. VISIT()
 -- then makes recursive calls to itself for each of the cube neighbors of the
 -- target. A depth parameter controls terminating the recursion after MAX_-
 -- SEARCH_DEPTH recursive calls. The results of the search are copied to
 -- the local "myObservers" list of observers. Procs start searches for each
 -- of their own observers. The observer's locations (that is, it's nearest
 -- grid node) need not be local, in which case the request is routed to the
 -- proc containing the node. An observer contains a single nearest-neighbor
 -- grid location, which is used as the starting point for the observer's
 -- search (see INIT for search initiation). The nearest-neighbor list is
 -- emptied before a search begins. Procs run their searches in parallel with
 -- those of other procs, but sequentially run through their own list of
 -- observers.
 -- ______________CHANGE HISTORY______________
 -- ______________END CHANGE HISTORY______________
 ------------------------------------------------*/

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

#include <stdlib.h>
#include <math.h>
#include "global.h"
#include "configuration.h"
#include "searchTypes.h"
#include "simCore.h"
#include "geometry.h"
#include "observerOutput.h"
#include "error.h"
#include "timers.h"

/* get all projections on the point observer spheres. */
void getPointObsProjections(void)
{

  Index_t face, col, row;

  for (face = 0; face < NUM_FACES; face++) {
    for (row = 0; row < FACE_ROWS; row++) {
      for (col = 0; col < FACE_COLS; col++) {
        pointObsProjectedStreamData(face, row, col);
      }
    }
  }

} /* END getPointObsProjections */


/* distribution values and radius of projection */
void pointObsProjectedStreamData(Index_t face, Index_t row, Index_t col)
{

  Scalar_t rSph;
  Scalar_t s;

  Index_t ss0 = 0;
  Index_t ss1 = TOTAL_NUM_SHELLS - 1;
  Index_t shellIn, shellOut, pointObserverIndex;
  Index_t species, energy, mu;

  Node_t proj;

  double timer_tmp=0;

  timer_tmp = MPI_Wtime();

  MPI_Gatherv(&grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)],
                  ACTIVE_STREAM_SIZE,
                  Node_T,
                  streamGrid,
                  recvCountGrid,
                  displGrid,
                  Node_T,
                  0,
                  MPI_COMM_WORLD);

  MPI_Gatherv(&eParts[idx_frcsspem(face,row,col,INNER_ACTIVE_SHELL,0,0,0)],
                  ACTIVE_STREAM_SIZE*NUM_SPECIES*NUM_ESTEPS*NUM_MUSTEPS,
                  Scalar_T,
                  ePartsStream,
                  recvCountEparts,
                  displEparts,
                  Scalar_T,
                  0,
                  MPI_COMM_WORLD );

  timer_MPIgatherscatter = timer_MPIgatherscatter + (MPI_Wtime() - timer_tmp);

  if (mpi_rank == 0) {

    for (pointObserverIndex=0; pointObserverIndex<config.numObservers; pointObserverIndex++) {

      rSph = config.obsR[pointObserverIndex] / config.rScale;

      shellIn = ss0;
      while ( ( streamGrid[ ((shellIn < ss1) ? (shellIn+1) : ss1) ].rmag < rSph ) && (shellIn < ss1) ) shellIn++;

      shellOut = ss1;
      while ( ( streamGrid[ ((shellOut > ss0) ? (shellOut-1) : ss0) ].rmag >= rSph ) && (shellOut > ss0) ) shellOut--;

      s = ( (shellOut > shellIn) ?  findIntersection( streamGrid[shellIn].r, streamGrid[shellOut].r, rSph ) : 0.0);

      proj.r.x = streamGrid[shellIn].r.x * (1.0-s) + streamGrid[shellOut].r.x * s;
      proj.r.y = streamGrid[shellIn].r.y * (1.0-s) + streamGrid[shellOut].r.y * s;
      proj.r.z = streamGrid[shellIn].r.z * (1.0-s) + streamGrid[shellOut].r.z * s;

      proj.rmag = sqrt( dotProduct( proj.r, proj.r ) );

      proj.mhdBr = streamGrid[shellIn].mhdBr * (1.0-s) + streamGrid[shellOut].mhdBr * s;
      proj.mhdBtheta = streamGrid[shellIn].mhdBtheta * (1.0-s) + streamGrid[shellOut].mhdBtheta * s;
      proj.mhdBphi = streamGrid[shellIn].mhdBphi * (1.0-s) + streamGrid[shellOut].mhdBphi * s;

      proj.mhdVr = streamGrid[shellIn].mhdVr * (1.0-s) + streamGrid[shellOut].mhdVr * s;
      proj.mhdVtheta = streamGrid[shellIn].mhdVtheta * (1.0-s) + streamGrid[shellOut].mhdVtheta * s;
      proj.mhdVphi = streamGrid[shellIn].mhdVphi * (1.0-s) + streamGrid[shellOut].mhdVphi * s;

      proj.mhdDensity = streamGrid[shellIn].mhdDensity * (1.0-s) + streamGrid[shellOut].mhdDensity * s;

      // find the projections for the energetic particle distributions */
      for (species = 0; species < NUM_SPECIES; species++ ) {
        for (energy = 0; energy < NUM_ESTEPS; energy++ ) {
          for (mu = 0; mu < NUM_MUSTEPS; mu++) {

            if ( (s >= 0.0 ) && (s <= 1.0) ) {
              ePartsProj[idx_frcspemo(face,row,col,species,energy,mu,pointObserverIndex)] =
                ePartsStream[idx_sspem(shellIn,species,energy,mu)]  * (1.0-s) + ePartsStream[idx_sspem(shellOut,species,energy,mu)] * s;
            } else if (s < 0.0) {
              ePartsProj[idx_frcspemo(face,row,col,species,energy,mu,pointObserverIndex)] =
                ePartsStream[idx_sspem(shellIn,species,energy,mu)];
            } else {
              ePartsProj[idx_frcspemo(face,row,col,species,energy,mu,pointObserverIndex)] =
                ePartsStream[idx_sspem(shellOut,species,energy,mu)];
            }

          } // mu loop
        } // energy loop
      } // species loop

      projections[idx_frco(face,row,col,pointObserverIndex)] = proj;

    } // pointObserverIndex loop
  } // mpi_rank == 0

} /* END pointObsProjectedStreamData */


/* Find intersection with sphere between x0 and x1

   x_int = x0 + s * (x1-x0)

   We output s
*/
Scalar_t findIntersection(Vec_t x0, Vec_t x1, Scalar_t r)
{

  Scalar_t r0square;
  Scalar_t s;
  Vec_t dx;
  Scalar_t dx2, b, c;

  dx.x = x1.x-x0.x;
  dx.y = x1.y-x0.y;
  dx.z = x1.z-x0.z;

  r0square = x0.x*x0.x + x0.y*x0.y + x0.z*x0.z;

  dx2 = dotProduct(dx, dx);

  b = dotProduct(x0, dx)/dx2;

  c = (r*r - r0square)/dx2;

  s = -b + sqrt( b*b + c );

  return s;

} /* END findIntersection */

