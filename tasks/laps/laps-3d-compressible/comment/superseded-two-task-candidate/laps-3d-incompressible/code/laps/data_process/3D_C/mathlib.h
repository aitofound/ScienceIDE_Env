#ifndef _MATHLIB_
#define _MATHLIB_

double mean(double *uu, int iv, int nx, int ny, int nz, int nvar);
double *cross(double *vec1, double *vec2);
double dot(double *vec1, double *vec2, int n);
double *vector_perp_to_B0(double *vec, double *B0);
double norm(double *vec, int n);

#endif