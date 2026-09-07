/*-----------------------------------------------
 -- EMMREM: ObserverOutput.c
 --
 -- output obsverver data
 --
 -- ______________CHANGE HISTORY______________
 -- ______________END CHANGE HISTORY______________
 ------------------------------------------------*/

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "geometry.h"
#include "global.h"
#include "configuration.h"
#include "observerOutput.h"
#include "simCore.h"
#include "searchTypes.h"
#include "error.h"
#include "timers.h"

time_t start_time;

/* Prints to a file the run parameters for the code, for each run. */
void RunParamsOut(void)
{

  FILE *rpout;
  
  double radMax;

  /* Here print out the main parameters of the run */

  if (mpi_rank ==0) {
    printf("\n******************************************************************\n");
    printf("*******************  EPREM Version %s  ************************\n",VERSION);
    printf("******************************************************************\n");

    printf("Shells: %d\nProcessors: %d\n",
           TOTAL_NUM_SHELLS,
           N_PROCS);

    if (config.mhdCouple) {

      radMax = config.mhdRadialMax;
      printf("Inner coupling boundary: %f Rs -- Outer coupling boundary: %f Rs\n",
             config.rScale / RSAU,
             radMax);

      printf("Start: %f JD, EP Calc: %f JD, MHD Start: %f JD\n\tEruption: %f JD, Stop: %f JD\n",
             config.simStartTime,
             config.epCalcStartTime,
             config.mhdStartTime,
             config.mhdStartTime + config.preEruptionDuration,
             config.simStopTime);

    } else {

      printf("Inner boundary: %f Rs\n",
             config.rScale / RSAU);

      printf("Start: %f JD, EP Calc: %f JD, Stop: %f JD\n",
             config.simStartTime,
             config.epCalcStartTime,
             config.simStopTime);

    }

    printf("******************************************************************\n\n");
  }


  if (mpi_rank == 0) {

    rpout = fopen("./epremRunParams.dat","w");

    fprintf(rpout,"\n******************************************************************\n");
    fprintf(rpout,"*******************  EPREM Version %s  ************************\n",VERSION);
    fprintf(rpout,"******************************************************************\n");

    if (config.mhdCouple > 0) {

      radMax = config.mhdRadialMax;
      fprintf(rpout, "Inner coupling boundary: %f Rs -- Outer coupling boundary: %f Rs\n",
             config.rScale / RSAU,
             radMax);

      fprintf(rpout,"Start: %f JD, EP Calc: %f JD, MHD Start: %f JD, Eruption: %f JD, Stop: %f JD\n",
             config.simStartTime,
             config.epCalcStartTime,
             config.mhdStartTime,
             config.mhdStartTime + config.preEruptionDuration,
             config.simStopTime);

    } else {

      fprintf(rpout, "Inner coupling boundary: %f Rs\n",
              config.rScale / RSAU);

      fprintf(rpout,"Start: %f JD, EP Calc: %f JD\n",
              config.simStartTime,
              config.epCalcStartTime);

    }

    fprintf(rpout,"******************************************************************\n\n");

    fprintf(rpout,"RUNTIME PARAMETERS:\n");
    fprintf(rpout,"NUM_FACES\n");
    fprintf(rpout,"%i\n",NUM_FACES);
    fprintf(rpout,"FACE_ROWS\n");
    fprintf(rpout,"%i\n",FACE_ROWS);
    fprintf(rpout,"FACE_COLS\n");
    fprintf(rpout,"%i\n",FACE_COLS);
    fprintf(rpout,"LOCAL_NUM_SHELLS\n");
    fprintf(rpout,"%i\n",LOCAL_NUM_SHELLS);
    fprintf(rpout,"NUM_SPECIES\n");
    fprintf(rpout,"%i\n",NUM_SPECIES);
    fprintf(rpout,"NUM_ESTEPS\n");
    fprintf(rpout,"%i\n",NUM_ESTEPS);
    fprintf(rpout,"NUM_MUSTEPS\n");
    fprintf(rpout,"%i\n",NUM_MUSTEPS);
    fprintf(rpout,"N_PROCS\n");
    fprintf(rpout,"%i\n",N_PROCS);
    fprintf(rpout,"TOTAL_NUM_SHELLS\n");
    fprintf(rpout,"%i\n",TOTAL_NUM_SHELLS);
    fclose(rpout);

  } /* if mpi_rank ==0 */

} /* END RunParamsOut */


/* Prints to a file the time run parameters for the code, for each run. */
void DumpRunTimes()
{

  FILE    *rpout;

  double min_tmp, max_tmp, mean, timer_sum;
  int i;
  time_t curr_time;

  double* all_timer_diffusestream;
  double* all_timer_adiabaticfocus;
  double* all_timer_adiabaticchange;
  double* all_timer_diffuseshell;
  double* all_timer_driftshell;
  double* all_timer_eptotal;
  double* all_timer_mhd_io;
  double* all_timer_eprem_io;
  double* all_timer_init;
  double* all_timer_other;
  double* all_timer_wall;
  double* all_timer_MPIgatherscatter;
  double* all_timer_MPIsendrecv;
//
// ****** The barrier here synchs all ranks so that they each
// ****** report a proper wall time.
//
  MPI_Barrier ( MPI_COMM_WORLD );

  timer_wall = MPI_Wtime();
  timer_wall = timer_wall - timer_start;

//
// ****** Sum-up top level timers to computer "other" time.
//
  timer_sum = timer_eptotal + timer_mhd_io + timer_eprem_io + timer_init;

  timer_other = timer_wall - timer_sum;

  all_timer_diffusestream= malloc(N_PROCS*sizeof(double));
  all_timer_adiabaticfocus= malloc(N_PROCS*sizeof(double));
  all_timer_adiabaticchange= malloc(N_PROCS*sizeof(double));
  all_timer_diffuseshell= malloc(N_PROCS*sizeof(double));
  all_timer_driftshell= malloc(N_PROCS*sizeof(double));
  all_timer_eptotal= malloc(N_PROCS*sizeof(double));
  all_timer_mhd_io= malloc(N_PROCS*sizeof(double));
  all_timer_eprem_io= malloc(N_PROCS*sizeof(double));
  all_timer_init= malloc(N_PROCS*sizeof(double));
  all_timer_other= malloc(N_PROCS*sizeof(double));
  all_timer_wall= malloc(N_PROCS*sizeof(double));
  all_timer_MPIgatherscatter= malloc(N_PROCS*sizeof(double));
  all_timer_MPIsendrecv= malloc(N_PROCS*sizeof(double));
//
// ****** Gather all timers form all processors.
//
  MPI_Allgather (&timer_diffusestream,   1,MPI_DOUBLE,all_timer_diffusestream,  1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_adiabaticfocus,  1,MPI_DOUBLE,all_timer_adiabaticfocus, 1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_adiabaticchange, 1,MPI_DOUBLE,all_timer_adiabaticchange,1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_diffuseshell,    1,MPI_DOUBLE,all_timer_diffuseshell,   1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_driftshell,      1,MPI_DOUBLE,all_timer_driftshell,     1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_eptotal,         1,MPI_DOUBLE,all_timer_eptotal,        1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_mhd_io,          1,MPI_DOUBLE,all_timer_mhd_io,         1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_eprem_io,        1,MPI_DOUBLE,all_timer_eprem_io,       1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_init,            1,MPI_DOUBLE,all_timer_init,           1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_other,           1,MPI_DOUBLE,all_timer_other,          1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_wall,            1,MPI_DOUBLE,all_timer_wall,           1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_MPIgatherscatter,1,MPI_DOUBLE,all_timer_MPIgatherscatter,1,MPI_DOUBLE,MPI_COMM_WORLD);
  MPI_Allgather (&timer_MPIsendrecv,     1,MPI_DOUBLE,all_timer_MPIsendrecv,     1,MPI_DOUBLE,MPI_COMM_WORLD);

  
  if (mpi_rank == 0){
    
    time ( &curr_time );
    
    printf(" \n");
    printf("********************************\n");
    printf("****RUN COMPLETE****************\n");
    printf("********************************\n");
    printf("Run started  on %s", asctime(localtime(&start_time)));
    printf("Run finished on %s", asctime(localtime(&curr_time)));
    printf("Run real time duration is %f minutes (or %f hours, or %f days)\n",
         (float)((curr_time-start_time)/60.0),
         (float)((curr_time-start_time)/3600.0),
         (float)((curr_time-start_time)/86400.0));
    printf("Run data time duration is %6.4lf days\n",config.simStopTime-config.simStartTime);
    printf("********************************\n");

    rpout = fopen("./epremRunParams.dat","a");

    fprintf(rpout,"\n");
    fprintf(rpout,"TIMING\n");
    fprintf(rpout,"********************************\n");
    fprintf(rpout,"Run started on %s", asctime(localtime(&start_time)));
    fprintf(rpout,"Run finished on %s", asctime(localtime(&curr_time)));
    fprintf(rpout,"Run real time duration is %f minutes (or %f hours, or %f days)\n",
          (float)((curr_time-start_time)/60.0),
          (float)((curr_time-start_time)/3600.0),
          (float)((curr_time-start_time)/86400.0));
    fprintf(rpout,"Run data time duration is %6.4lf days\n",config.simStopTime-config.simStartTime);
    fprintf(rpout,"********************************\n");

    fprintf(rpout,"-------------------DETAILED TIMING----------------------\n");
    fprintf(rpout,"%-24s  %*s  %*s  %*s\n","Code portion",9,"mean",9,"max",9,"min");
    fprintf(rpout,"--------------------------------------------------------\n");

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_MPIgatherscatter[i]>max_tmp) max_tmp=all_timer_MPIgatherscatter[i];
      if(all_timer_MPIgatherscatter[i]<min_tmp) min_tmp=all_timer_MPIgatherscatter[i];
      mean = mean + all_timer_MPIgatherscatter[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","|->MPI (Gather/Scatter)",mean,max_tmp,min_tmp);
    
    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_MPIsendrecv[i]>max_tmp) max_tmp=all_timer_MPIsendrecv[i];
      if(all_timer_MPIsendrecv[i]<min_tmp) min_tmp=all_timer_MPIsendrecv[i];
      mean = mean + all_timer_MPIsendrecv[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","|->MPI (Send/Recv)",mean,max_tmp,min_tmp);
    
    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_init[i]>max_tmp) max_tmp=all_timer_init[i];
      if(all_timer_init[i]<min_tmp) min_tmp=all_timer_init[i];
      mean = mean + all_timer_init[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","Initialization",mean,max_tmp,min_tmp);
 
    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_eprem_io[i]>max_tmp) max_tmp=all_timer_eprem_io[i];
      if(all_timer_eprem_io[i]<min_tmp) min_tmp=all_timer_eprem_io[i];
      mean = mean + all_timer_eprem_io[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","IO (EPREM)",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_mhd_io[i]>max_tmp) max_tmp=all_timer_mhd_io[i];
      if(all_timer_mhd_io[i]<min_tmp) min_tmp=all_timer_mhd_io[i];
      mean = mean + all_timer_mhd_io[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","IO (MHD)",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_eptotal[i]>max_tmp) max_tmp=all_timer_eptotal[i];
      if(all_timer_eptotal[i]<min_tmp) min_tmp=all_timer_eptotal[i];
      mean = mean + all_timer_eptotal[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","EP Total",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_diffusestream[i]>max_tmp) max_tmp=all_timer_diffusestream[i];
      if(all_timer_diffusestream[i]<min_tmp) min_tmp=all_timer_diffusestream[i];
      mean = mean + all_timer_diffusestream[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","|->EP DiffuseStream",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_adiabaticfocus[i]>max_tmp) max_tmp=all_timer_adiabaticfocus[i];
      if(all_timer_adiabaticfocus[i]<min_tmp) min_tmp=all_timer_adiabaticfocus[i];
      mean = mean + all_timer_adiabaticfocus[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","|->EP Adiabatic Focus",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_adiabaticchange[i]>max_tmp) max_tmp=all_timer_adiabaticchange[i];
      if(all_timer_adiabaticchange[i]<min_tmp) min_tmp=all_timer_adiabaticchange[i];
      mean = mean + all_timer_adiabaticchange[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","|->EP Adiabatic Change",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_diffuseshell[i]>max_tmp) max_tmp=all_timer_diffuseshell[i];
      if(all_timer_diffuseshell[i]<min_tmp) min_tmp=all_timer_diffuseshell[i];
      mean = mean + all_timer_diffuseshell[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","|->EP Diffuse Shell",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_driftshell[i]>max_tmp) max_tmp=all_timer_driftshell[i];
      if(all_timer_driftshell[i]<min_tmp) min_tmp=all_timer_driftshell[i];
      mean = mean + all_timer_driftshell[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","|->EP Drift Shell",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_other[i]>max_tmp) max_tmp=all_timer_other[i];
      if(all_timer_other[i]<min_tmp) min_tmp=all_timer_other[i];
      mean = mean + all_timer_other[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","Other",mean,max_tmp,min_tmp);

    max_tmp=0.0;
    min_tmp=1.0e200;
    mean=0.0;
    for (i=0;i<N_PROCS;i++){
      if(all_timer_wall[i]>max_tmp) max_tmp=all_timer_wall[i];
      if(all_timer_wall[i]<min_tmp) min_tmp=all_timer_wall[i];
      mean = mean + all_timer_wall[i];
    }
    mean = mean/N_PROCS;
    fprintf(rpout,"%-24s  %9.2f  %9.2f  %9.2f\n","Total (Wall)",mean,max_tmp,min_tmp);

    fprintf(rpout,"--------------------------------------------------------\n\n");

    fclose(rpout);
  }

  free(all_timer_diffusestream);
  free(all_timer_adiabaticfocus);
  free(all_timer_adiabaticchange);
  free(all_timer_diffuseshell);
  free(all_timer_driftshell);
  free(all_timer_eptotal);
  free(all_timer_mhd_io);
  free(all_timer_eprem_io);
  free(all_timer_init);
  free(all_timer_other);
  free(all_timer_wall);
  free(all_timer_MPIgatherscatter);
  free(all_timer_MPIsendrecv);

} /* END DumpRunTimes */

