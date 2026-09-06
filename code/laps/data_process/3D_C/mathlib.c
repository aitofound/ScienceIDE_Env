#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "macros.h"

double mean(double *uu, int iv, int nx, int ny, int nz, int nvar)
{
    // iv: variable to be averaged
    double sum;
    size_t N = nx*ny*nz;
    for(int i=0;i<nx;i++)
    {
        for(int j=0;j<ny;j++)
        {
            for(int k=0;k<nz;k++)
            {
                sum += uu[IDXIJ(iv,i,j,k)];
            }
        }
    }
    return sum/N;
}


double *cross(double *vec1, double *vec2)
{
    double *result = (double*)malloc(sizeof(double) * 3);
    memset(result, 0, sizeof(double) * 3);

    result[0] = vec1[1] * vec2[2] - vec1[2] * vec2[1];
    result[1] = vec1[2] * vec2[0] - vec1[0] * vec2[2];
    result[2] = vec1[0] * vec2[1] - vec1[1] * vec2[0];

    return result;
}

double dot(double *vec1, double *vec2, int n)
{
    double result = 0;
    for(int i=0;i<n;i++)
    {
        result += vec1[i] * vec2[i];
    }
    return result;
}

double *vector_perp_to_B0(double *vec, double *B0)
{
    // note: B0 must be unit vector
    double *result = (double*)malloc(sizeof(double) * 3);
    memset(result, 0, sizeof(double) * 3);

    double vdotB = dot(vec,B0,3);

    result[0] = vec[0] - B0[0] * vdotB;
    result[1] = vec[1] - B0[1] * vdotB;
    result[2] = vec[2] - B0[2] * vdotB;

    return result;
}

double norm(double *vec, int n)
{
    double result = 0;
    for(int i=0;i<n;i++)
    {
        result += vec[i] * vec[i];
    }

    return sqrt(result);
}