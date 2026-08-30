#include "macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void read_grid(char *filename, int *nx, int *ny, int *nz,
    float **ptrXgrid, float **ptrYgrid, float **ptrZgrid)
{
    FILE *fpRead = fopen(filename, "rb");
    // Check if the file was opened successfully
    if (fpRead == NULL) {
        printf("Error: Could not open file: %s.\n", filename);
        return;
    }

    float skip;

    // read nx,ny,nz
    fread(&skip, sizeof(float), 1, fpRead);

    fread(&skip, sizeof(float), 1, fpRead);
    *nx = (int)skip;

    fread(&skip, sizeof(float), 1, fpRead);
    *ny = (int)skip;

    fread(&skip, sizeof(float), 1, fpRead);
    *nz = (int)skip;

    skip = fread(&skip, sizeof(float), 1, fpRead);


    // allocate arrays
    *ptrXgrid = malloc((*nx) * sizeof(float));
    memset(*ptrXgrid, 0, (*nx) * sizeof(float));

    *ptrYgrid = malloc((*ny) * sizeof(float));
    memset(*ptrYgrid, 0, (*ny) * sizeof(float));

    *ptrZgrid = malloc((*nz) * sizeof(float));
    memset(*ptrZgrid, 0, (*nz) * sizeof(float));

    // read arrays
    fread(&skip, sizeof(float), 1, fpRead);

    for(int i=0;i<(*nx);i++)
    {
        fread(&skip, sizeof(float), 1, fpRead);
        (*ptrXgrid)[i] = skip;
    }
    for(int i=0;i<(*ny);i++)
    {
        fread(&skip, sizeof(float), 1, fpRead);
        (*ptrYgrid)[i] = skip;
    }
    for(int i=0;i<(*nz);i++)
    {
        fread(&skip, sizeof(float), 1, fpRead);
        (*ptrZgrid)[i] = skip;
    }

    fread(&skip, sizeof(float), 1, fpRead);


    fclose(fpRead);
}


void read_parallel_info(char *filename, int *npe, 
    int *iproc, int *jproc, int *nvar)
{
    FILE *fpRead = fopen(filename,"rb");
    // Check if the file was opened successfully
    if (fpRead == NULL) {
        printf("Error: Could not open file: %s.\n", filename);
        return;
    }

    float skip;

    fread(&skip, sizeof(float), 1, fpRead);

    fread(&skip, sizeof(float), 1, fpRead);
    *npe = (int)skip; 

    fread(&skip, sizeof(float), 1, fpRead);
    *iproc = (int)skip;

    fread(&skip, sizeof(float), 1, fpRead);
    *jproc = (int)skip;

    fread(&skip, sizeof(float), 1, fpRead);
    *nvar = (int)skip;

    fread(&skip, sizeof(float), 1, fpRead);
    

    fclose(fpRead);
}


void read_EBM(char *filename, double **ptr_tEBM, double **ptr_radius,
    double **ptr_Ur, int *nEBM)
{
    FILE *fpRead;

    // get lines of files
    fpRead = fopen(filename,"r");

    // Check if the file was opened successfully
    if (fpRead == NULL) {
        printf("Error: Could not open file: %s.\n", filename);
        return;
    }

    int nRows = 0;
    char ch;
    // Count the number of rows in the file
    while ((ch = fgetc(fpRead)) != EOF) {
        if (ch == '\n') {
            nRows++;
        }
    }

    *nEBM = nRows;
    // Close the file
    fclose(fpRead);

    // allocate array
    *ptr_tEBM = malloc(sizeof(double) * nRows);
    memset(*ptr_tEBM, 0, sizeof(double) * nRows);

    *ptr_radius = malloc(sizeof(double) * nRows);
    memset(*ptr_radius, 0, sizeof(double) * nRows);

    *ptr_Ur = malloc(sizeof(double) * nRows);
    memset(*ptr_Ur, 0, sizeof(double) * nRows);

    // open the file again to read data

    fpRead = fopen(filename,"r");

    double t,radius,Ur;

    
    for(int i=0;i<nRows;i++)
    {
        fscanf(fpRead, "  %lf  %lf  %lf", &t, &radius, &Ur);
        (*ptr_tEBM)[i] = t;
        (*ptr_radius)[i] = radius;
        (*ptr_Ur)[i] = Ur;
    }

    fclose(fpRead);
}

void read_output(char *filename, double **ptr_uu, float *t, 
    int nx, int ny, int nz, int nvar)
{
    size_t sizeTot = nx*ny*nz*nvar;

    // allocate memory and initialize to zero
    *ptr_uu = malloc(sizeTot * sizeof(double));
    memset(*ptr_uu,0,sizeof(double) * sizeTot);

    // open the file
    FILE *fpRead = fopen(filename,"rb");

    // Check if the file was opened successfully
    if (fpRead == NULL) {
        printf("Error: Could not open file: %s.\n", filename);
        return;
    }

    // read time
    float skip;
    fread(&skip, sizeof(float), 1, fpRead);

    fread(&skip,sizeof(float),1,fpRead);
    *t = skip;
    fread(&skip, sizeof(float), 1, fpRead);

    // read main array
    double skipdb;
    for (int ivar=0; ivar<nvar; ivar++)
    {
        for (int iz=0; iz<nz; iz++)
        {
            for (int iy=0; iy<ny; iy++)
            {
                for (int ix=0; ix<nx; ix++)
                {
                    fread(&skipdb, sizeof(double), 1, fpRead);

                    (*ptr_uu)[IDXIJ(ivar,ix,iy,iz)] = skipdb;
                }
            }
        }
    }


    fclose(fpRead);
}