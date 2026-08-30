#include "iofunctions.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "macros.h"
#include "statistics.h"
#include <pthread.h>
#include "global.h"

int main(int argc, char **argv)
{
    time_t start_time, end_time;
    double timeConsumption;

    char *filenameGrid = "./output/grid.dat";
    char *filenameParallel = "./output/parallel_info.dat";
    char *filenameEBM = "./output/EBM_info.dat";
    char filename_uu[100];

    int nx, ny, nz, nvar, npe, iproc, jproc, nEBM;

    float t,*xgrid, *ygrid, *zgrid;
    double *tEBM, *radius, *Ur;

    double R0,Ur0;

    double *uu;

    int nThread = 3;
    pthread_t *thread_handles;


    
    int method = 1;
    int nt = 0;

    if(argc>=2)
    {
        nt = atoi(argv[1]);
    }
    if (argc>=3)
    {
        method = atoi(argv[2]);
    }


    // read grid data----------------------
    read_grid(filenameGrid, &nx, &ny, &nz, &xgrid, &ygrid, &zgrid);

    printf("nx = %d, ny = %d, nz = %d\n", nx,ny,nz);
    //-------------------------------------

    // read parallel info--------------------
    read_parallel_info(filenameParallel, &npe, &iproc, &jproc, &nvar);

    printf("nvar = %d, npe = %d, iproc = %d, jproc = %d\n",
         nvar, npe, iproc, jproc);
    //-------------------------------------

    // read EBM info------------------------
    read_EBM(filenameEBM, &tEBM, &radius,&Ur, &nEBM);
    R0 = radius[0];
    Ur0 = Ur[0];
    printf("R0 = %.3f, Ur0 = %.3f, nEBM = %d\n",R0, Ur0, nEBM);


    
    // read output data-------------------
    sprintf(filename_uu,"./output/out%03d.dat",nt);

    printf("Begin to Read File: %s\n", filename_uu);
    start_time = time(NULL);

    read_output(filename_uu, &uu, &t, nx, ny, nz, nvar);

    end_time = time(NULL);
    timeConsumption = (double) difftime(end_time, start_time);
    printf("End reading file, time consumption = %.3E sec\n", timeConsumption);

    printf("t = %.5f\n", t);
    //-------------------------------------
    
    // arguments for each thread
    struct args_for_thread *args;
    args = (struct args_for_thread*)malloc(sizeof(struct args_for_thread) * nThread);
    for(int thread=0;thread<nThread;thread++)
    {
        args[thread].iThread = thread;
        args[thread].nx = nx;
        args[thread].ny = ny;
        args[thread].nz = nz;
        args[thread].nvar = nvar;
        args[thread].arr = uu;
        args[thread].nt = nt;
        args[thread].method = method;
    }

    // create thread handles;
    thread_handles = (pthread_t *)malloc(sizeof(pthread_t) * nThread);

    for(int thread=0;thread<nThread;thread++)
    {
        //calculate angles between (u,b) and (z+,z-)
        // theta_ub_zpm_perp_only(uu, nt, 1, nx, ny, nz, nvar);

        pthread_create(&thread_handles[thread], NULL, 
            pthread_theta_ub_zpm_perp_only, (void *)&args[thread]);
    }

    for(int thread=0;thread<nThread;thread++)
    {
        pthread_join(thread_handles[thread],NULL);
    }
    

    // free all arrays
    free(uu);
    free(xgrid);
    free(ygrid);
    free(zgrid);
    free(tEBM);
    free(radius);
    free(Ur);
    free(thread_handles);
    free(filenameGrid);
    free(filenameParallel);
    free(filenameEBM);
    free(filename_uu);


    return 0;
}