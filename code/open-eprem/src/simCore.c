/*-----------------------------------------------
 -- EMMREM: simCore.c
 --
 -- General simulation routines.
 --
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

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "global.h"
#include "configuration.h"
#include "simCore.h"
#include "geometry.h"
#include "energeticParticles.h"
#include "flow.h"
#include "mhdInterp.h"
#include "readMHD.h"
#include "unifiedOutput.h"
#include "searchTypes.h"
#include "cubeShellStruct.h"
#include "observerOutput.h"
#include "error.h"
#include "timers.h"

Time_t        t_global;             /*-- Simulation current time.         --*/
Time_t        t_observer_del;       /*-- Time since last observer search and print --*/
Time_t        t_sun_del;            /*-- Time since sun was last rotated. --*/
Time_t        t_counter;            /*-- Time since last counter time update -- */
Radian_t      azi_sun;              /*-- Sun's current azimuth/longitude. --*/
Time_t        t_init;
Index_t       num_loops;

// flags
Index_t weInitializedEPs;
Index_t mhdGridStatus;
Index_t sync_hel=0;
Index_t hdf5_input=0;              // Set=1 if hdf5 input files (RMC move this)

void simCoreInit(void)
{

  t_observer_del = 0.0; /*-- Time since observer search and print, initially.--*/
  t_sun_del = 0.0; /*-- Time since sun was last rotated, initially.--*/

  t_counter = 0.0;
  azi_sun   = config.aziSunStart;  /*-- Sun's suface initial azimuth.   --*/

  t_init = TOTAL_NUM_SHELLS * config.tDel; /*-- time for seeding nodes --*/

  t_global = config.simStartTimeDay - t_init;

  num_loops = 0;

} /* END simCoreInit */


void flagParamInit(void)
{

  phiOffset = 0.0;
  phiHelOffset = 0.0;

  weInitializedEPs = 0;
  unwindPhiOffset = 0;

  mhdGridStatus = MHD_DEFAULT;

  mhdFileIndex0 = 0;
  mhdFileIndex1 = 0;
  mhdEqFileFlag = 0;
  mhdMallocFlag = 0;

  unifiedOutputInit = 0;
  pointObserverOutputInit = 0;
  domainDumpInit = 0;
  unstructuredDomainInit = 0;

  observerTimeSlice = 0;
  pointObserverTimeSlice = 0;
  domainTimeSlice = 0;
  unstructuredDomainTimeSlice = 0;



} /* END flagParamInit */


void timeInitialization(void)
{

  Scalar_t mhdDt;
  Scalar_t mhdRunDuration;

  if (config.mhdCouple) {

    mhdRunDuration = (mhdTime[config.mhdNumFiles - 1] - mhdTime[0])*DAY;
    mhdDt = mhdTime[1] - mhdTime[0];

    if (config.useMhdSteadyStateDt > 0)
      config.tDel = mhdDt;

    if (config.mhdInitTimeStep == 0.0)
      config.mhdInitTimeStep = config.tDel;

    // NOTE! For now, do not allow particle equilibrium.

    config.simStartTime = config.mhdStartTime;
    // Do not do any particle calculations until CME is about to erupt.
    config.epCalcStartTime = config.mhdStartTime + config.preEruptionDuration;

    config.unifiedOutputTime = config.mhdStartTime + 0.5 * mhdDt * DAY;
    config.pointObserverOutputTime = config.mhdStartTime + 0.5 * mhdDt * DAY;
    config.streamFluxOutputTime = config.mhdStartTime + 0.5 * mhdDt * DAY;
    config.epremDomainOutputTime = config.mhdStartTime + 0.5 * mhdDt * DAY;

    config.simStopTime = config.mhdStartTime + mhdRunDuration;

  } else {

    config.pointObserverOutputTime = config.simStartTime + 0.5 * config.tDel * DAY;
    config.unifiedOutputTime = config.simStartTime + 0.5 * config.tDel * DAY;
    config.epremDomainOutputTime = config.simStartTime + 0.5 * config.tDel * DAY;

  }

  config.simStartTimeDay = config.simStartTime / DAY;
  config.simStopTimeDay  = config.simStopTime / DAY;

} /* END timeInitialization */


/* Gather single stream from the cooresponding shells across all MPI ranks. */
void update_stream_from_shells(Index_t iterIndex)
{

  Index_t proc, workIndex, num_calls;

  double timer_tmp = 0;

  MPI_Request request_grid[N_PROCS];
  MPI_Request request_eparts[N_PROCS];
  MPI_Request request_lambda[N_PROCS];

  timer_tmp = MPI_Wtime();

  num_calls = 0;

  for (proc = 0; proc < N_PROCS; proc++)
  {

    workIndex = proc + N_PROCS * iterIndex;

    if (workIndex < NUM_STREAMS)
    {
      num_calls++;

      MPI_Igatherv(&eParts[idx_frcsspem(computeLines[workIndex][0],
                                        computeLines[workIndex][1],
                                        computeLines[workIndex][2],
                                        INNER_ACTIVE_SHELL,0,0,0)],
                   ACTIVE_STREAM_SIZE*NUM_SPECIES*NUM_ESTEPS*NUM_MUSTEPS,
                   Scalar_T,
                   ePartsStream,
                   recvCountEparts,
                   displEparts,
                   Scalar_T,
                   proc,
                   MPI_COMM_WORLD,
                   &request_eparts[proc]);

      MPI_Igatherv(&lambdaPara[idx_frcsspe(computeLines[workIndex][0],
                                           computeLines[workIndex][1],
                                           computeLines[workIndex][2],
                                           INNER_ACTIVE_SHELL,0,0)],
                   ACTIVE_STREAM_SIZE*NUM_SPECIES*NUM_ESTEPS,
                   Scalar_T,
                   lambdaParaStream,
                   recvCountLambda,
                   displLambda,
                   Scalar_T,
                   proc,
                   MPI_COMM_WORLD,
                   &request_lambda[proc]);

      MPI_Igatherv(&grid[idx_frcs(computeLines[workIndex][0],
                                  computeLines[workIndex][1],
                                  computeLines[workIndex][2],
                                  INNER_ACTIVE_SHELL)],
                   ACTIVE_STREAM_SIZE,
                   Node_T,
                   streamGrid,
                   recvCountGrid,
                   displGrid,
                   Node_T,
                   proc,
                   MPI_COMM_WORLD,
                   &request_grid[proc]);

    }

  }

  MPI_Waitall(num_calls, request_eparts, MPI_STATUSES_IGNORE);
  MPI_Waitall(num_calls, request_grid,   MPI_STATUSES_IGNORE);
  MPI_Waitall(num_calls, request_lambda, MPI_STATUSES_IGNORE);

  timer_MPIgatherscatter = timer_MPIgatherscatter
                           + (MPI_Wtime() - timer_tmp);

} /* END update_stream_from_shells */


/* Scatter single stream to the cooresponding shells across all MPI ranks. */
void update_shells_from_stream(Index_t iterIndex)
{

  Index_t proc, workIndex, num_calls;

  double timer_tmp = 0;

  MPI_Request request_grid[N_PROCS];
  MPI_Request request_eparts[N_PROCS];
  MPI_Request request_lambda[N_PROCS];

  timer_tmp = MPI_Wtime();

  num_calls = 0;

  for (proc = 0; proc < N_PROCS; proc++)
  {

    workIndex = proc + N_PROCS * iterIndex;

    if (workIndex < NUM_STREAMS)
    {
      num_calls++;

      MPI_Iscatterv(ePartsStream,
                    recvCountEparts,
                    displEparts,
                    Scalar_T,
                    &eParts[idx_frcsspem(computeLines[workIndex][0],
                                         computeLines[workIndex][1],
                                         computeLines[workIndex][2],
                                         INNER_ACTIVE_SHELL,0,0,0)],
                    ACTIVE_STREAM_SIZE*NUM_SPECIES*NUM_ESTEPS*NUM_MUSTEPS,
                    Scalar_T,
                    proc,
                    MPI_COMM_WORLD,
                    &request_eparts[proc]);

      MPI_Iscatterv(lambdaParaStream,
                    recvCountLambda,
                    displLambda,
                    Scalar_T,
                    &lambdaPara[idx_frcsspe(computeLines[workIndex][0],
                                            computeLines[workIndex][1],
                                            computeLines[workIndex][2],
                                            INNER_ACTIVE_SHELL,0,0)],
                    ACTIVE_STREAM_SIZE*NUM_SPECIES*NUM_ESTEPS,
                    Scalar_T,
                    proc,
                    MPI_COMM_WORLD,
                    &request_lambda[proc]);

      MPI_Iscatterv(streamGrid,
                    recvCountGrid,
                    displGrid,
                    Node_T,
                    &grid[idx_frcs(computeLines[workIndex][0],
                                   computeLines[workIndex][1],
                                   computeLines[workIndex][2],
                                   INNER_ACTIVE_SHELL)],
                    ACTIVE_STREAM_SIZE,
                    Node_T,
                    proc,
                    MPI_COMM_WORLD,
                    &request_grid[proc]);

    }

  }

  MPI_Waitall(num_calls, request_eparts, MPI_STATUSES_IGNORE);
  MPI_Waitall(num_calls, request_grid,   MPI_STATUSES_IGNORE);

  timer_MPIgatherscatter = timer_MPIgatherscatter
                           + (MPI_Wtime() - timer_tmp);

} /* END update_shells_from_stream */


void updateStreamValues(Index_t iterIndex)
{
// This routine seems to set ds and mhdB+/- on streamGrid that then updates
// eGrid in the gatherv after diffusion - then they are used in adiabatic focusing.
// CAREFUL!!!   This MUST be called/updated before focusing!
//  This whole thing seems to be done since only here do we have access across
//  proc boundaries along the stream...
// This routine basically acts as a "seam" for the streams across shell bounderies
// But only for those values needed in the shell-stage (focusing).
//
  Index_t workIndex;

  Vec_t rMinus, r, rPlus;
  Index_t shellMinus, shell, shellPlus;
  Scalar_t dsPlus, dsMinus;

  workIndex = mpi_rank + N_PROCS * iterIndex;

  // workIndex is not used in this loop.
  // However there may be procesors with some empty streams.
  // Those procs do not need to compute these things so we put this conditional here
  // to avoid needless computation.

  if (workIndex < NUM_STREAMS)
  {

    for (shell = 0; shell < TOTAL_NUM_SHELLS; shell++)
    {

      if (shell == 0)
      {

        shellPlus = shell + 1;

        rPlus.x = streamGrid[shellPlus].r.x * config.rScale;
        rPlus.y = streamGrid[shellPlus].r.y * config.rScale;
        rPlus.z = streamGrid[shellPlus].r.z * config.rScale;

        r.x = streamGrid[shell].r.x * config.rScale;
        r.y = streamGrid[shell].r.y * config.rScale;
        r.z = streamGrid[shell].r.z * config.rScale;

        streamGrid[shell].ds = sqrt((rPlus.x - r.x) * (rPlus.x - r.x) +
                                    (rPlus.y - r.y) * (rPlus.y - r.y) +
                                    (rPlus.z - r.z) * (rPlus.z - r.z));

        streamGrid[shell].mhdBmagMinus = streamGrid[shell].mhdBmag;
        streamGrid[shell].mhdBmagPlus = streamGrid[shellPlus].mhdBmag;

      }
      else if ( shell == (TOTAL_NUM_SHELLS - 1) )
      {

        shellMinus = shell - 1;

        r.x = streamGrid[shell].r.x * config.rScale;
        r.y = streamGrid[shell].r.y * config.rScale;
        r.z = streamGrid[shell].r.z * config.rScale;

        rMinus.x = streamGrid[shellMinus].r.x * config.rScale;
        rMinus.y = streamGrid[shellMinus].r.y * config.rScale;
        rMinus.z = streamGrid[shellMinus].r.z * config.rScale;

        streamGrid[shell].ds = sqrt((r.x - rMinus.x) * (r.x - rMinus.x) +
                                    (r.y - rMinus.y) * (r.y - rMinus.y) +
                                    (r.z - rMinus.z) * (r.z - rMinus.z));

        streamGrid[shell].mhdBmagMinus = streamGrid[shellMinus].mhdBmag;
        streamGrid[shell].mhdBmagPlus = streamGrid[shell].mhdBmag;

      }
      else
      {

        shellMinus = shell - 1;
        shellPlus = shell + 1;

        rPlus.x = streamGrid[shellPlus].r.x * config.rScale;
        rPlus.y = streamGrid[shellPlus].r.y * config.rScale;
        rPlus.z = streamGrid[shellPlus].r.z * config.rScale;

        r.x = streamGrid[shell].r.x * config.rScale;
        r.y = streamGrid[shell].r.y * config.rScale;
        r.z = streamGrid[shell].r.z * config.rScale;

        rMinus.x = streamGrid[shellMinus].r.x * config.rScale;
        rMinus.y = streamGrid[shellMinus].r.y * config.rScale;
        rMinus.z = streamGrid[shellMinus].r.z * config.rScale;

        dsPlus = sqrt((rPlus.x - r.x) * (rPlus.x - r.x) +
                      (rPlus.y - r.y) * (rPlus.y - r.y) +
                      (rPlus.z - r.z) * (rPlus.z - r.z));

        dsMinus = sqrt((r.x - rMinus.x) * (r.x - rMinus.x) +
                       (r.y - rMinus.y) * (r.y - rMinus.y) +
                       (r.z - rMinus.z) * (r.z - rMinus.z));

        streamGrid[shell].ds = 0.5 * (dsPlus + dsMinus);

        streamGrid[shell].mhdBmagMinus = streamGrid[shellMinus].mhdBmag;
        streamGrid[shell].mhdBmagPlus = streamGrid[shellPlus].mhdBmag;
      }

    }

  }

} /* END updateStreamValues */


/* Rotate the sun for t_sun_del seconds around its polar axis and spawn a new
   inner shell. Consequently, existing shells must first be pushed outward to
   the next cube. The innermost shell on the innermost proc (proc-0) is saved as
   a template from which new shells are spawned into cube-1. (For other procs,
   cube-0 buffers shells moving into cube-1.) We want to give the new shell its
   azi(muth) and (x,y) for each grid node, but we also want to preserve the
   inner shell as a template and boundary condition. So, the order of work is to
   ripple out the shells, which makes room for a new shell and clones the inner
   shell to cube-1, then give the new shell its angular position depending on
   the sun's current longitudinal (azimuth) position. At proc boundaries, the
   next shell to ripple out has to be recieved from the next proc inward from
   the current proc. Initially, all shells effectively reside on the sun's
   surface. Initializing the simulation requires first spawning all the active
   shells as part of the initialization process, or some other means of getting
   the simulation started is required. That is, this routine is not responsible
   for the correct state of the shells.
*/
void rotSunAndSpawnShell(Time_t t_sun_del)
{
  int proc_left, proc_right;
  int RIPPLEF_TAG=1;
  int RIPPLEG_TAG=2;
  MPI_Request req[4];

  Index_t face, row, col, species, energy, mu;

  Scalar_t * sendBuffF;
  Scalar_t * recvBuffF;
  Node_t *   sendBuffG;
  Node_t *   recvBuffG;

  double timer_tmp=0;

  timer_tmp = MPI_Wtime();

  // allocate the memory for the send and receive buffers
  sendBuffF = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)NUM_FACES*RCSPEM);
  recvBuffF = (Scalar_t *) malloc(sizeof(Scalar_t)*(int)NUM_FACES*RCSPEM);
  sendBuffG = (Node_t *)   malloc(sizeof(Node_t)*(int)NUM_FACES*RC);
  recvBuffG = (Node_t *)   malloc(sizeof(Node_t)*(int)NUM_FACES*RC);

  proc_left  = mpi_rank - 1;
  proc_right = mpi_rank + 1;

  if ( mpi_rank == OUTER_PROC ){
    proc_right = MPI_PROC_NULL;
  }

  if ( mpi_rank == INNER_PROC ){
    proc_left = MPI_PROC_NULL;
  }

  /*
  ****** Load send buffers.
  */

  if (proc_right != MPI_PROC_NULL){

    for (face = 0; face < NUM_FACES; face++) {
      for (row = 0; row < FACE_ROWS; row++) {
        for (col = 0; col < FACE_COLS; col++) {

          sendBuffG[idx_frc(face,row,col)]
          = grid[idx_frcs(face,row,col,OUTER_SHELL)];

          for (species = 0; species < NUM_SPECIES; species++) {
            for (energy = 0; energy < NUM_ESTEPS; energy++) {
              for (mu = 0; mu < NUM_MUSTEPS; mu++){

                sendBuffF[idx_frcspem(face,row,col,species,energy,mu)]
                = eParts[idx_frcsspem(face,row,col,OUTER_SHELL,species,energy,mu)];

              }
            }
          }
        }
      }
    }

  }

  MPI_Irecv(&recvBuffG[0],
           NUM_FACES*RC,
           Node_T,
           proc_left,
           RIPPLEG_TAG,
           MPI_COMM_WORLD,
           &req[0]);

  MPI_Irecv(&recvBuffF[0],
           NUM_FACES*RCSPEM,
           Scalar_T,
           proc_left,
           RIPPLEF_TAG,
           MPI_COMM_WORLD,
           &req[1]);

  MPI_Isend(&sendBuffG[0],
           NUM_FACES*RC,
           Node_T,
           proc_right,
           RIPPLEG_TAG,
           MPI_COMM_WORLD,
           &req[2]);

  MPI_Isend(&sendBuffF[0],
           NUM_FACES*RCSPEM,
           Scalar_T,
           proc_right,
           RIPPLEF_TAG,
           MPI_COMM_WORLD,
           &req[3]);

  MPI_Waitall(4,req,MPI_STATUSES_IGNORE);

  /*
  ****** Unload data from recieve buffers.
  */

  if (proc_left != MPI_PROC_NULL){

    for (face = 0; face < NUM_FACES; face++) {
      for (row = 0; row < FACE_ROWS; row++) {
        for (col = 0; col < FACE_COLS; col++) {

          grid[idx_frcs(face,row,col,INNER_SHELL)]
          = recvBuffG[idx_frc(face,row,col)];

          for (species = 0; species < NUM_SPECIES; species++) {
            for (energy = 0; energy < NUM_ESTEPS; energy++) {
              for (mu = 0; mu < NUM_MUSTEPS; mu++){

                eParts[idx_frcsspem(face,row,col,INNER_SHELL,species,energy,mu)]
                = recvBuffF[idx_frcspem(face,row,col,species,energy,mu)];

              }
            }
          }
        }
      }
    }
  }

  /* Free temporary buffers */
  free(sendBuffF);
  free(recvBuffF);
  free(sendBuffG);
  free(recvBuffG);

  timer_MPIsendrecv = timer_MPIsendrecv + (MPI_Wtime() - timer_tmp);

  /*
  ****** Rotate sun.
  */
  azi_sun += (config.omegaSun * t_sun_del);
  /*-- azimuth is in [-PI,PI] --*/
  if (azi_sun > PI) azi_sun -= TWO_PI;

  /*
  ****** Ripple the shells out.
  */
  rippleShellsOut();

  if (mpi_rank == INNER_PROC){
    spawnNewShell();
  }

} /* END rotSunAndSpawnShell */


/* This is only done on the inner proc. The inner shell of the inner proc acts
   as a template to spawn a new shell. The next shell out has already had all its
   data overwriten with the inner shell's data when this function is called; that
   is, rippleShellsOut() should have been called just before this. This function
   simply adjusts the node positions to account for the sun's rotation since
   initialization of the inner shell.
*/
void spawnNewShell(void)
{
  Index_t  face, row, col;
  Radian_t azi;
  Radian_t zen;
  Coord_t  x, y, z;
  Scalar_t rmag;

  /*-- Update azimuths of all nodes in new shell. Recall that         --*/
  /*-- the new shell inherited its node positions from the inner      --*/
  /*-- shell which acts as a template. That template's node positions --*/
  /*-- are in the positions assigned to them at initialization time   --*/
  /*-- and have not been keeping up with the sun's rotations.         --*/

  for (face  = 0; face  < REAL_FACES;  face++ ){
    for (row   = 0; row   < FACE_ROWS;  row++  ){
      for (col   = 0; col   < FACE_COLS;  col++  ){

        /*-- Get copies of new shell's position data  --*/
        /*-- for use in computing updates.            --*/
        azi    = grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)].azi;
        zen    = grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)].zen;
        rmag   = grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)].rmag;

        /*-- Add sun's current azimuth to node's initial azimuth.  --*/
        /*-- The overall effect rotates all the nodes from their   --*/
        /*-- initial position on the sun's surface to their proper --*/
        /*-- position relative to the sun's current position.      --*/
        azi += azi_sun;
        if (azi > PI) azi -= TWO_PI;  /*-- azimuth is in [-PI,PI] --*/

        x = rmag * sin(zen) * cos(azi);  /*-- Adjust x-coord to new pos. --*/
        y = rmag * sin(zen) * sin(azi);  /*-- Adjust y-coord to new pos. --*/
        z = rmag * cos(zen);  /*-- Adjust y-coord to new pos. --*/

        grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)].zen   = zen;
        grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)].azi   = azi;
        grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)].r.x   = x;
        grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)].r.y   = y;
        grid[idx_frcs(face,row,col,INNER_ACTIVE_SHELL)].r.z   = z;

      }
    }
  }

} /* END spawnNewShell */


/* Copy nodes from shell to shell sequentially starting from the outer shell and
   moving to the inner shell. The outer shell's node data gets clobbered, the inner
   shell's remains unchanged. Moves all shell data (but not cube adjacencies) out
   one cube.
*/
void rippleShellsOut(void)
{
  Index_t  face, row, col, shell;
  Neighbor_t n, e, w, s, streamIn, streamOut;

  for (shell = OUTER_SHELL; shell > INNER_SHELL;  shell--){
    for (face  = 0;            face  < NUM_FACES;    face++ ){
      for (row   = 0;            row   < FACE_ROWS;    row++  ){
        for (col   = 0;            col   < FACE_COLS;    col++  ){

          n = grid[idx_frcs(face,row,col,shell)].n;
          e = grid[idx_frcs(face,row,col,shell)].e;
          w = grid[idx_frcs(face,row,col,shell)].w;
          s = grid[idx_frcs(face,row,col,shell)].s;
          streamIn = grid[idx_frcs(face,row,col,shell)].streamIn;
          streamOut = grid[idx_frcs(face,row,col,shell)].streamOut;

          memcpy(&grid[idx_frcs(face,row,col,shell)], &grid[idx_frcs(face,row,col,shell-1)],
                 sizeof(Node_t));
          memcpy(&eParts[idx_frcsspem(face,row,col,shell,0,0,0)], &eParts[idx_frcsspem(face,row,col,shell-1,0,0,0)],
                 sizeof(Scalar_t) * NUM_SPECIES * NUM_ESTEPS * NUM_MUSTEPS);

          grid[idx_frcs(face,row,col,shell)].n = n;
          grid[idx_frcs(face,row,col,shell)].e = e;
          grid[idx_frcs(face,row,col,shell)].w = w;
          grid[idx_frcs(face,row,col,shell)].s = s;
          grid[idx_frcs(face,row,col,shell)].streamIn = streamIn;
          grid[idx_frcs(face,row,col,shell)].streamOut = streamOut;

        }
      }
    }
  }

} /* END rippleShellsOut */


/* Move nodes in space by flow. Does not alter inner shell. */
void moveNodes(Scalar_t dt)
{
  Index_t   face, row, col, shell, shell0;
  Vec_t     r, r_del;
  Scalar_t  rmag;
  Node_t node;

  /* INNER_ACTIVE_SHELL on innermost proc left on inner boundary */
  /* on all other procs, the INNER_ACTIVE_SHELL moves forward    */

  if (mpi_rank == INNER_PROC) {
    shell0 = INNER_ACTIVE_SHELL + 1; }
  else {
    shell0 = INNER_ACTIVE_SHELL;}

  for (shell = shell0; shell < LOCAL_NUM_SHELLS; shell++){
    for (face  = 0;             face  < NUM_FACES;  face++ ){
      for (row   = 0;             row   < FACE_ROWS;  row++  ){
        for (col   = 0;             col   < FACE_COLS;  col++  ){

          node = grid[idx_frcs(face,row,col,shell)];

          r     = node.r;
          rmag  = node.rmag;

          r_del = delrFlow( r, rmag, node, dt);

          // store the current position as rOld
          grid[idx_frcs(face,row,col,shell)].rOld = r;

          r.x  += r_del.x;
          r.y  += r_del.y;
          r.z  += r_del.z;
          rmag  = sqrt( (r.x*r.x) + (r.y*r.y) + (r.z*r.z) );

          grid[idx_frcs(face,row,col,shell)].r     = r;
          grid[idx_frcs(face,row,col,shell)].rmag  = rmag;

        }
      }
    }
  }

} /* END moveNodes */


/* Set the time step. */
void setDt()
{

  Index_t i, idx;

  if ((config.mhdCouple > 0) && (config.mhdCoupledTime > 0)){

    idx = 0;
//
// First get the coronal DT.
//
    if (t_global < mhdTime[0]){
      if (config.useMhdSteadyStateDt > 0){
        config.tDel = mhdTime[1] - mhdTime[0];
      //  if(mpi_rank==0) printf("SetDT0:   DT:  %8.2e\n",config.tDel);
      }
    } else if (t_global >= mhdTime[config.mhdNumFiles-1]){

      config.tDel = mhdTime[config.mhdNumFiles-1] - mhdTime[config.mhdNumFiles-2];
     // if(mpi_rank==0) printf("SetDT1:   DT:  %8.2e\n",config.tDel);
    } else {
//
// Find the closest time index in mhdTimes to the current t_global time.
// Then set the time step to be the next step in mhdTimes.
//
      for (i=1; i<config.mhdNumFiles; i++){
        if (mhdTime[i] >= t_global ){
           idx = i;
           break;
        }
      }
   /*   if(mpi_rank==0) printf("SetDT:  first idx: %d\n",idx);
      if(mpi_rank==0) printf("SetDT:  t_global: %8.2e\n",t_global);
      if(mpi_rank==0) printf("SetDT:  mhdTime[%03d]: %8.2e\n",idx-1,mhdTime[idx-1]);
      if(mpi_rank==0) printf("SetDT:  mhdTime[%03d]: %8.2e\n",idx,mhdTime[idx]);
      if(mpi_rank==0) printf("SetDT:  |t-mhd[idx-1]|: %8.2e\n",fabs(t_global - mhdTime[idx-1]));
      if(mpi_rank==0) printf("SetDT:  |t-mhd[idx]|: %8.2e\n",fabs(t_global - mhdTime[idx]));*/

      if ((idx == config.mhdNumFiles-1) ||
          (fabs(t_global - mhdTime[idx-1]) < fabs(t_global - mhdTime[idx]))){
        idx=idx-1;
      }
     //  if(mpi_rank==0) printf("SetDT:  revised idx: %d\n",idx);

     // if(mpi_rank==0) printf("SetDT:   mhdTime[%03d]: %8.2e  mhdTime[%03d]: %8.2e \n",idx,mhdTime[idx],idx+1,mhdTime[idx+1]);
      config.tDel = mhdTime[idx+1] - mhdTime[idx];
     // if(mpi_rank==0) printf("SetDT:   DT:  %8.2e\n",config.tDel);
    }
  } // if(mhdCoupleTime)

} /* END setDt */

