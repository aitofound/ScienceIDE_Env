#include "macros.h"
#include "mathlib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "global.h"

void theta_ub_zpm_perp_only(double *uu, int nt, int method, 
    int nx, int ny, int nz, int nvar, int axis)
{
    // method = 1  or method =2 is the same: will do both.
    time_t start_time, end_time;
    double timeConsumption;

    int naxis;
    int i,j,k,i1,j1,k1;

    char filename[100];
    FILE *fpWrite;

    int l[3];
    double ux,uy,uz,vax,vay,vaz;
    double zpx,zpy,zpz,zmx,zmy,zmz;
    double vec1[3],vec2[3];

    double *vec1_perp,*vec2_perp,vec1_perp_abs,vec2_perp_abs,
        *cross_vec1_perp_vec2_perp,cross_vec1_perp_vec2_perp_abs;
    double numerator, denominator;

    double theta_ave_ub, theta_ave_zpm;
    double numerator_ave_ub, denominator_ave_ub,
        numerator_ave_zpm, denominator_ave_zpm;
    size_t n_accum_ub_method1, n_accum_zpm_method1, 
        n_accum_ub_method2, n_accum_zpm_method2;

    double *angle_ub_lx_method1, *angle_zpm_lx_method1,
        *angle_ub_lx_method2, *angle_zpm_lx_method2;


    if (method != 1 && method != 2 && method!=3)
    {
        printf("In function theta_ub_zpm_perp_only(): method must be 1-3!!!\n");
        exit(0);
    }


    if(axis==0)
    {
        printf("Begin calculating angles along lx...\n");
        naxis = nx;
    }
    else if(axis==1)
    {
        printf("Begin calculating angles along ly...\n");
        naxis = ny;
    }
    else if(axis==2)
    {
        printf("Begin calculating angles along lz...\n");
        naxis = nz;
    }
    else
    {
        printf("Error in function theta_ub_zpm_perp_only(): axis must be 0,1, or 2!!!\n");
        exit(0);
    }


    // allocate arrays
    angle_ub_lx_method1 = (double*)malloc(sizeof(double) * naxis);
    angle_zpm_lx_method1 = (double*)malloc(sizeof(double) * naxis);
    angle_ub_lx_method2 = (double*)malloc(sizeof(double) * naxis);
    angle_zpm_lx_method2 = (double*)malloc(sizeof(double) * naxis);

    memset(angle_ub_lx_method1, 0, sizeof(double) * naxis);
    memset(angle_zpm_lx_method1, 0, sizeof(double) * naxis);
    memset(angle_ub_lx_method2, 0, sizeof(double) * naxis);
    memset(angle_zpm_lx_method2, 0, sizeof(double) * naxis);

    // calculate average fields
    printf("Calculate average fields ... ");
    start_time = time(NULL);

    double rho0 = mean(uu, 0, nx, ny, nz, nvar);
    double Bx0 = mean(uu, 4, nx, ny, nz, nvar);
    double By0 = mean(uu, 5, nx, ny, nz, nvar);
    double Bz0 = mean(uu, 6, nx, ny, nz, nvar);

    printf("rho0 = %.5E, B0 = (%.5E, %.5E, %.5E)\n", rho0, Bx0, By0, Bz0);

    double B0abs = sqrt(Bx0*Bx0 + By0*By0 + Bz0*Bz0);
    double b0[3]; // unit vector
    b0[0] = Bx0/B0abs;
    b0[1] = By0/B0abs;
    b0[2] = Bz0/B0abs;

    end_time = time(NULL);
    timeConsumption = difftime(end_time, start_time);
    printf(" time consumption = %.3E sec\n", timeConsumption);



    // begin calculate angle along one direction
    start_time = time(NULL);

    l[0] = 0;
    l[1] = 0;
    l[2] = 0;

    printf("set l = (%d,%d,%d) \n", l[0],l[1],l[2]);
    // no need to calculate |l| = 0 cases.
    for(int ix=1; ix<naxis/2+1; ix++)
    {
        if(axis==0)
        {
            printf("lx = %d/%d  \n",ix,naxis);
        }
        else if(axis==1)
        {
            printf("ly = %d/%d  \n",ix,naxis);
        }
        else if(axis==2)
        {
            printf("lz = %d/%d  \n",ix,naxis);
        }

        l[axis] = ix;

        theta_ave_ub = 0;
        theta_ave_zpm = 0;

        numerator_ave_ub = 0;
        denominator_ave_ub = 0;
        numerator_ave_zpm = 0;
        denominator_ave_zpm = 0;

        n_accum_ub_method1 = 0;
        n_accum_zpm_method1 = 0;

        n_accum_ub_method2 = 0;
        n_accum_zpm_method2 = 0;


        for(i=0;i<nx;i++)
        {
            // printf("i = %d/%d  ",i,nx);
            i1 = (i + l[0])%nx;
            for(j=0; j<ny; j++)
            {
                j1 = (j + l[1])%ny;
                for(k=0; k<nz;k++)
                {
                    k1 = (k + l[2])%nz; 

                    // calculate increment of u,va,z+,z-
                    ux = uu[IDXIJ(1,i1,j1,k1)] - uu[IDXIJ(1,i,j,k)];
                    uy = uu[IDXIJ(2,i1,j1,k1)] - uu[IDXIJ(2,i,j,k)];
                    uz = uu[IDXIJ(3,i1,j1,k1)] - uu[IDXIJ(3,i,j,k)];

                    vax = (uu[IDXIJ(4,i1,j1,k1)] - uu[IDXIJ(4,i,j,k)])/sqrt(rho0);
                    vay = (uu[IDXIJ(5,i1,j1,k1)] - uu[IDXIJ(5,i,j,k)])/sqrt(rho0);
                    vaz = (uu[IDXIJ(6,i1,j1,k1)] - uu[IDXIJ(6,i,j,k)])/sqrt(rho0);

                    zpx = ux - vax;
                    zpy = uy - vay;
                    zpz = uz - vaz;

                    zmx = ux + vax;
                    zmy = uy + vay;
                    zmz = uz + vaz;


                    // begin calculate angle - ub
                    vec1[0] = ux;
                    vec1[1] = uy; 
                    vec1[2] = uz;
                    vec2[0] = vax;
                    vec2[1] = vay;
                    vec2[2] = vaz;

                    vec1_perp = vector_perp_to_B0(vec1,b0);
                    vec2_perp = vector_perp_to_B0(vec2,b0);

                    vec1_perp_abs = norm(vec1_perp,3);
                    vec2_perp_abs = norm(vec2_perp,3);

                    if(method==1 || method==2)
                    {
                        cross_vec1_perp_vec2_perp = cross(vec1_perp,vec2_perp);
                        cross_vec1_perp_vec2_perp_abs = norm(cross_vec1_perp_vec2_perp,3);

                        // (method==1)  use arcsin to calculate each angle, then average
                        if(vec1_perp_abs>0 && vec2_perp_abs>0)
                        {
                            numerator = cross_vec1_perp_vec2_perp_abs;
                            denominator = vec1_perp_abs * vec2_perp_abs;

                            theta_ave_ub += asin(numerator/denominator);
                            n_accum_ub_method1 += 1;
                        }

                        // (method==2) Use arcsin, but calcualte average numerator and denominator first
                        numerator_ave_ub += cross_vec1_perp_vec2_perp_abs;
                        denominator_ave_ub += vec1_perp_abs * vec2_perp_abs;
                        n_accum_ub_method2 += 1;
                        
                    }
                    else if (method==3)
                    {
                        // use dot product, but this may be not good: theta and 180-theta should
                        // be the same, but arccos(dot-product) gives different results.
                    }

                    // free 
                    free(vec1_perp);
                    free(vec2_perp);
                    free(cross_vec1_perp_vec2_perp);


                    // begin calculate angle - (z+,z-)
                    vec1[0] = zpx;
                    vec1[1] = zpy; 
                    vec1[2] = zpz;
                    vec2[0] = zmx;
                    vec2[1] = zmy;
                    vec2[2] = zmz;

                    vec1_perp = vector_perp_to_B0(vec1,b0);
                    vec2_perp = vector_perp_to_B0(vec2,b0);

                    vec1_perp_abs = norm(vec1_perp,3);
                    vec2_perp_abs = norm(vec2_perp,3);

                    if(method==1 || method==2)
                    {
                        cross_vec1_perp_vec2_perp = cross(vec1_perp,vec2_perp);
                        cross_vec1_perp_vec2_perp_abs = norm(cross_vec1_perp_vec2_perp,3);

                        // (method==1)  use arcsin to calculate each angle, then average
                        if(vec1_perp_abs>0 && vec2_perp_abs>0)
                        {
                            numerator = cross_vec1_perp_vec2_perp_abs;
                            denominator = vec1_perp_abs * vec2_perp_abs;

                            theta_ave_zpm += asin(numerator/denominator);
                            n_accum_zpm_method1 += 1;
                        }

                        // (method==2) Use arcsin, but calcualte average numerator and denominator first
                        numerator_ave_zpm += cross_vec1_perp_vec2_perp_abs;
                        denominator_ave_zpm += vec1_perp_abs * vec2_perp_abs;
                        n_accum_zpm_method2 += 1;
                    }
                    else if (method==3)
                    {
                        // use dot product, but this may be not good: theta and 180-theta should
                        // be the same, but arccos(dot-product) gives different results.
                    }

                    // free 
                    free(vec1_perp);
                    free(vec2_perp);
                    free(cross_vec1_perp_vec2_perp);
                }
            }
        }
    

        // finalize calculated angles...
        // (method==1)
        theta_ave_ub = theta_ave_ub / n_accum_ub_method1;
        theta_ave_zpm = theta_ave_zpm / n_accum_zpm_method1;

        angle_ub_lx_method1[ix] = theta_ave_ub;
        angle_zpm_lx_method1[ix] = theta_ave_zpm;
        
        // (method==2)
        angle_ub_lx_method2[ix] = asin(numerator_ave_ub/denominator_ave_ub);
        angle_zpm_lx_method2[ix] = asin(numerator_ave_zpm/denominator_ave_zpm);
    } 


    end_time = time(NULL);
    timeConsumption = difftime(end_time, start_time);
    printf("Thread %d job finished, time consumption = %.3E sec\n", axis, timeConsumption);

    // write angle(u,b) - method 1
    if(axis==0)
    {
        sprintf(filename, "./output/theta_ub_perp_only_lx_method%1d_%03d.dat",
         1, nt);
    }
    else if(axis==1)
    {
        sprintf(filename, "./output/theta_ub_perp_only_ly_method%1d_%03d.dat",
         1, nt);
    }
    else if(axis==2)
    {
        sprintf(filename, "./output/theta_ub_perp_only_lz_method%1d_%03d.dat",
         1, nt);
    }
    fpWrite = fopen(filename,"wb");
    fwrite(angle_ub_lx_method1, sizeof(double), naxis, fpWrite);
    fclose(fpWrite);

    // write angle(u,b) - method 2
    if(axis==0)
    {
        sprintf(filename, "./output/theta_ub_perp_only_lx_method%1d_%03d.dat",
         2, nt);
    }
    else if(axis==1)
    {
        sprintf(filename, "./output/theta_ub_perp_only_ly_method%1d_%03d.dat",
         2, nt);
    }
    else if(axis==2)
    {
        sprintf(filename, "./output/theta_ub_perp_only_lz_method%1d_%03d.dat",
         2, nt);
    }
    fpWrite = fopen(filename,"wb");
    fwrite(angle_ub_lx_method2, sizeof(double), naxis, fpWrite);
    fclose(fpWrite);


    // write angle(z+,z-) -- method 1
    if(axis==0)
    {
        sprintf(filename, "./output/theta_zpm_perp_only_lx_method%1d_%03d.dat",
         1, nt);
    }
    else if(axis==1)
    {
        sprintf(filename, "./output/theta_zpm_perp_only_ly_method%1d_%03d.dat",
         1, nt);
    }
    else
    {
        sprintf(filename, "./output/theta_zpm_perp_only_lz_method%1d_%03d.dat",
         1, nt);
    }
    
    fpWrite = fopen(filename,"wb");
    fwrite(angle_zpm_lx_method1, sizeof(double), naxis, fpWrite);
    fclose(fpWrite);

    // write angle(z+,z-) -- method 2
    if(axis==0)
    {
        sprintf(filename, "./output/theta_zpm_perp_only_lx_method%1d_%03d.dat",
         2, nt);
    }
    else if(axis==1)
    {
        sprintf(filename, "./output/theta_zpm_perp_only_ly_method%1d_%03d.dat",
         2, nt);
    }
    else
    {
        sprintf(filename, "./output/theta_zpm_perp_only_lz_method%1d_%03d.dat",
         2, nt);
    }
    
    fpWrite = fopen(filename,"wb");
    fwrite(angle_zpm_lx_method2, sizeof(double), naxis, fpWrite);
    fclose(fpWrite);


    // free arrays
    free(angle_ub_lx_method1);
    free(angle_zpm_lx_method1);
    free(angle_ub_lx_method2);
    free(angle_zpm_lx_method2);
    return;
}

void *pthread_theta_ub_zpm_perp_only(void *args)
{
    struct args_for_thread parameter = *((struct args_for_thread *)args);
 
    int iThread, axis, nt, method;
    int nx,ny,nz,nvar;
    double *arr;

    iThread = parameter.iThread;
    nx = parameter.nx;
    ny = parameter.ny;
    nz = parameter.nz;
    nvar = parameter.nvar;
    nt = parameter.nt;
    method = parameter.method;

    arr = parameter.arr;

    axis = iThread;


    printf("Thread %d will process axis %d\n", iThread, axis);
    printf("(nx,ny,nz,nvar)=(%d,%d,%d,%d)\n",nx,ny,nz,nvar);
    printf("nt = %d, method = %d\n",nt, method);

    theta_ub_zpm_perp_only(arr, nt, method, nx, ny, nz, nvar, axis);

    return NULL;
}