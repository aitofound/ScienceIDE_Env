#ifndef _GLOBAL_
#define _GLOBAL_

struct args_for_thread
{
    int iThread;
    int nx;
    int ny;
    int nz;
    int nvar;
    int nt;
    int method;
    double *arr;
};

#endif