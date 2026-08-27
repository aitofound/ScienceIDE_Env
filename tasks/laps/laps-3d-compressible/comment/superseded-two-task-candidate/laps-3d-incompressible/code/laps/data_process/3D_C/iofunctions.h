#ifndef _IOFUNCTIONS_
#define _IOFUNCTIONS_

void read_grid(char *filename, int *nx, int *ny, int *nz,
    float **ptrXgrid, float **ptrYgrid, float **ptrZgrid);

void read_parallel_info(char *filename, int *npe, 
    int *iproc, int *jproc, int *nvar);

void read_EBM(char *filename, double **ptr_tEBM, double **ptr_radius,
    double **ptr_Ur, int *nEBM);

void read_output(char *filename, double **ptr_uu, float *t, 
    int nx, int ny, int nz, int nvar);
#endif