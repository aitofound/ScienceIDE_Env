/*------------------------------------------------------------------------------
 ---- EPREM: main.c
 ----
 -- A spatial grid in 3-d space with grid points (nodes) associated with
 -- magnetic field lines anchored at the sun's surface and freely floating
 -- nodes. The spatial grid is housed in a data structure based on nested cubes
 -- whose surfaces are regularly subdivided into square arrays of square cells.
 -- Each cell defines a grid node (vertex) at its center. Each vertex has four
 -- surface neighbor vertices in the NEWS directions defined by its cell's
 -- edge adjacencies (which wrap at cube edges).
 -- A cube's collection of vertices and their surface neighbor relations
 -- (edges) defines a sub-graph of the entire grid of nested cubes.
 -- A cube houses a "shell" of grid nodes, the nested shells comprise the grid.
 -- Each node has a space position vector, r, defining its location
 -- in 3-d space. Initially, a shell sits at some fixed distance from the
 -- sun's surface. As time
 -- advances, the vertices' positions change with the locally defined flow
 -- field, moving outward from the sun's surface. But, graph adjacencies do
 -- not change.
 --
 -- Periodically, a new set of vertices is generated at the sun's surface.
 -- This causes the old vertex data (the shell) on the inner-most cube to move
 -- outward to the next enclosing cube in the data structure.
 -- That causes a ripple effect of shell data moving outward from cube to cube.
 -- For each proc, its inner-most cube serves as a communications buffer.
 -- Consequently, the inner-most shell on each proc resides on the next-to-
 -- inner-most cube (the inner-most cube just buffers a copy of that shell.)
 -- Each vertex then has two additional neighbor links besides its NEWS
 -- neighbors, streamIn and streamOut, identifying shell node neighbors in
 -- the next inner shell and next outer shell, respectively. (NB--These stream
 -- neighbor links connect the active grid shells. Thus, these stream links
 -- jump over the inner-most cube as that shell is not active in grid updates.)
 -- The "stream" notation indicates that these links are defined along
 -- magnetic field lines. The inner-most cube on the inner-most proc never
 -- has its grid node data updated by the main simulation loop as that shell
 -- serves as a template in spawning new shells.
 --
 -- NB-A good notation would be helpful for separating the notion of the cubic
 -- data structure's elements (vertices and graph edges) from the conceptual
 -- idea of the spatially located shell nodes and their associated data. The
 -- cube structure is static, while the shell nodes stream through it.
 -- Nested shells of nodes "live in" nested sub-divided cube sub-graphs, and
 -- shells migrate outward from cube to cube, eventually disappearing.
 -- Shells have a spatial distribution, initially on the surface of a sphere,
 -- while cubes have a logical 3-d structure consisting six orthogonal square
 -- grids (number of rows = number of columns) connected as if on the faces of
 -- a cube. Cube vertices form the data structure, while a shell node is a
 -- conceptual collection of data that resides temporarily in a cube vertex.
 -- Vertices have adjacent vertices, while shell nodes have neighbors. A
 -- shell node can find its neighbor by using graph edges to get to adjacent
 -- cube vertices, where it will find its neighbor node's information.
 -- Unfortunately, this notation is not adhered to in a consistant fashion,
 -- and a clean separation between the concepts of cubes and shells is not
 -- made.
 --
 -----------------------------------------------------------------------*/

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

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "configuration.h"
#include "energeticParticlesInit.h"
#include "energeticParticles.h"
#include "cubeShellInit.h"
#include "readMHD.h"
#include "mhdInterp.h"
#include "searchTypes.h"
#include "unifiedOutput.h"
#include "observerOutput.h"
#include "flow.h"
#include "float.h"
#include "simCore.h"
#include "timers.h"
#include "meanFreePath.h"

/* Initialize all global timers. */
double timer_diffusestream=0;
double timer_adiabaticfocus=0;
double timer_adiabaticchange=0;
double timer_diffuseshell=0;
double timer_driftshell=0;
double timer_eptotal=0;
double timer_mhd_io=0;
double timer_eprem_io=0;
double timer_init=0;
double timer_other=0;
double timer_step=0;
double timer_wall=0;
double timer_start=0;
double timer_MPIgatherscatter=0;
double timer_MPIsendrecv=0;

/*====================== MAIN =======================================*/
int main(int argc, char *argv[]) {

  int epInit, rciter;
  double timer_tmp;

  // Echo version, if requested.
  for (int i=0; i<argc; i++) {
    if ((strcmp(argv[i], "-V") == 0) || (strcmp(argv[i], "--version") == 0)) {
      printf("EPREM version %s\n", VERSION);
      return(0);
    }
  }

  // Initialize MPI
  initMPI(argc, argv);

  epInit = 0;
  timer_tmp = 0;
  rciter = 0;
  simStarted = 0;

  // Record the starting MPI time and real time.
  if (mpi_rank == 0) time ( &start_time );
  timer_start = MPI_Wtime();

  // Read and set runtime parameters
  initGlobalParameters(argv[1]);

  // Initialize MPI Offsets for Gatherv and Scatterv
  initMPIOffsets();

  // Memory allocation for the main global variables
  allocateGlobalVariables();

  // Initialize MPI Types
  // This also allocates the 1D scale grids (mu, energy, etc).
  initMPITypes();

  // Initialize flags
  flagParamInit();

  // Get the MHD coupling info and file lists
  if (config.mhdCouple > 0){
    mhdFetchCouplingInfo();
    mhdFetchFileList();
  }

  // Initialize time variables
  timeInitialization();

  // Define the streams which will be computed
  defineComputeLines();

  // Set simulation parameters
  simCoreInit();

  // Intialize e-grid and ep attributes
  initEnergeticParticlesGrids();

 // Initialize MHD.
  if (config.mhdCouple > 0) mhdGetInterpData(0.0);
  updateMhd();

  // Initialize mean free path (uses updated MHD).
  updateMFP();

  // Initialize the cube / shell structure
  // and set node positions through backward integration.
  gridStructInit();

  // Create the names for the output files
  buildOutputNames();

  // Initialize unified netCDF output
  if (config.unifiedOutput > 0) initObserverDataNetCDF();

  // Initialize point observer netCDF output
  if (config.numObservers > 0) initPointObserverDataNetCDF();

  // Initialize netCDF file for domain output
  if (config.epremDomain > 0) initDomainDumpNetCDF();

  // Initialize netCDF file for unstructured domain output
  if (config.unstructuredDomain > 0) initUnstructuredDomainDumpNetCDF();

  // Write the simulation params out to the screen and file
  RunParamsOut();

  // Get time taken for initialization:
  timer_init = MPI_Wtime() - timer_start;

  // ---------------------------------------------------------------------------
  // ---------------------------------------------------------------------------
  // ------------------  MAIN SIMULATION LOOP  ---------------------------------
  // ---------------------------------------------------------------------------
  // ---------------------------------------------------------------------------
  if(mpi_rank==0){
    printf(" \n");
    printf("*************************************************\n");
    printf("****SEEDING NODES********************************\n");
    printf("*************************************************\n");
  }

  do
  {
    timer_step = MPI_Wtime();

    rciter+=1;

    // The seeding of nodes takes TOTAL_NUM_SHELLS iterations.
    // Since the t_global might not be exactly simStartTimeDay, we use
    // iteration count here and sync the t_global.

    if (rciter == TOTAL_NUM_SHELLS+1)
    {
      if (simStarted == 0){
        simStarted = 1;
        // Intialize seed population of energetic particles.
        // Since seed function is r-dependent, this needs to be done
        // after pushing all nodes out to their initial positions.
        initEnergeticParticles();
        if (mpi_rank == 0){
          printf(" \n");
          printf("*************************************************\n");
          printf("****STARTING SIMULATION**************************\n");
          printf("*************************************************\n");
          printf("NOTE:  Initializing seed population.\n");
          printf("NOTE:  Syncing t_global [%14.8e] to config.simStartTimeDay [%14.8e]\n",
                 t_global, config.simStartTimeDay);
        }
        t_global = config.simStartTimeDay;
      }
    }

    if (t_global*DAY >= config.epCalcStartTime)
    {
      if (simStarted == 1){
        simStarted = 2;
        if (mpi_rank == 0){
          printf(" \n");
          printf("*************************************************\n");
          printf("****STARTING ENERGETIC PARTICLE CALCULATIONS*****\n");
          printf("*************************************************\n");
        }
      }
    }

    //
    // Set the time step.
    //

    setDt();

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // ------ DISPLAY CURRENT TIME INFO TO SCREEN. -----------------------------
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    if (mpi_rank == 0){
      printf("Step: %06d  TIME: %14.8e  [JD %9.5f]  DTIME: %14.8e  [JD %7.5f]\n",
             rciter, t_global, t_global*DAY, config.tDel, config.tDel*DAY);
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // ------------------- After seeding nodes, reset phi offsets and
    // ------------------- rotate nodes back in phi.
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    if ((config.mhdRotateSolution > 0) && (config.mhdCouple > 0) && (simStarted > 0) && (unwindPhiOffset == 0)) {
      unwindPhiOffset = 1;
      resetDomainOffset();
    // Since we have just moved the nodes to a new position,
    // need to re-interpolate MHD values to current positions.
      updateMhd();
      updateMFP();
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // -------------------  I/O OF CURRENT TIME MHD AND EPART ------------------
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    // NOTE!  This MUST be here and not below mhdGetInterpData for coupled runs.
    //        This is because the I/O and moveNodes() use getMhdNode()
    //        which uses "s_xxx" and the MHD files from the previous step,
    //        which makes this work right because it makes it at the right time.

    if ( (num_loops % config.dumpFreq) == 0 )
    {
      dataDumpIO();
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // ---------------- MOVE NODES USING CURRENT TIME's MHD V ------------------
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    // Rotate the node seed positions and ripple the shells out
    rotSunAndSpawnShell( config.tDel );

    if (config.mhdCouple > 0)
      mhdMoveNodes( config.tDel );
    else
      moveNodes( config.tDel );

    // Phi-shift nodes in co-rotating coronal frame
    // (equivalent to converting corotating frame to inertial with a +Vphi?)
    if ( (config.mhdRotateSolution > 0) && (config.mhdCouple > 0) )
      rotateCoupledDomain( config.tDel );

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // --------- UPDATE MHD FOR TIME+DT AND SAVE CURRENT MHD TO MHD-OLD --------
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    // Load MHD data from files needed to get MHD quantities at time=time+dt.
    if (config.mhdCouple > 0) mhdGetInterpData( config.tDel );
    updateMhd();
    updateMFP();

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // ---------------- UPDATE values used for perpendicular solvers -----------
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    if ( (config.useDrift > 0) || (config.useShellDiffusion > 0) )
    {
      ShellData();
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // ----------------  PARTICLE COMPUTATION ----------------------------------
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    if (t_global*DAY >= config.epCalcStartTime)
    {
      timer_tmp = MPI_Wtime();

      // Re-intialize seed population of energetic particles.
      // Since nodes are pushed before this point, this needs to be done
      // here.
      if (epInit == 0){
        epInit = 1;
        if (mpi_rank == 0) printf("  --> NOTE:  Reinitializing seed population.\n");
        initEnergeticParticles();
      }

      updateEnergeticParticles();

      // For the seed test, re-init seed population.
      if (config.seedFunctionTest > 0){
        initEnergeticParticles();
        if (mpi_rank == 0) printf("  --> NOTE:  Reinitializing seed population.\n");
      }

      timer_eptotal = timer_eptotal + (MPI_Wtime() - timer_tmp);
    }

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // ----------------  INCREMENT TIME->TIME+DT--------------------------------
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------

    t_global += config.tDel;

    // IO LOOP counter.
    num_loops++;


    if (epInit == 1){

    MPI_Reduce(&maxsubcycles_energychange,
               &maxsubcycles_energychangeGlobal,
               1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&maxsubcycles_focusing,
               &maxsubcycles_focusingGlobal,
               1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&min_tau,
               &min_tau_global,
               1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);

    if (mpi_rank == 0){
      printf("  --> Maximum subcycles for Adiabatic Change:   %d \n", maxsubcycles_energychangeGlobal);
      printf("  --> Maximum subcycles for Adiabatic Focusing: %d \n", maxsubcycles_focusingGlobal);
      printf("  --> Minimum MFP timescale (tau): %14.8e    DTIME/TAU: %14.2f\n", min_tau_global, config.tDel/min_tau_global);
    }
    // Reset these values:
    maxsubcycles_energychangeGlobal = 0;
    maxsubcycles_focusingGlobal = 0;
    maxsubcycles_energychange = 0;
    maxsubcycles_focusing = 0;
    min_tau = DBL_MAX;
    min_tau_global = DBL_MAX;

    }

    timer_tmp = MPI_Wtime();
    if (mpi_rank == 0) printf("  --> Compute time for step: %18.4f seconds.\n",timer_tmp-timer_step);

  } while(t_global <= config.simStopTimeDay);

  // ---------------------------------------------------------------------------
  // ---------------------------------------------------------------------------
  // ------------------ CLEAN UP AND EXIT --------------------------------------
  // ---------------------------------------------------------------------------
  // ---------------------------------------------------------------------------

  DumpRunTimes();
  freeMhdArrays();
  freeGlobalArrays();
  freeMPIOffsets();
  freeOutputArrays();
  freeMPI_energeticParticlesTypes();
  freeMPI_cubeShellStruct();
  freeMPI_baseTypes();
  freeConfigArrays();
  config_destroy(&cfg);
  cleanupMPIWindows();
  MPI_Finalize();

  return(0);

}
//============================================================================
