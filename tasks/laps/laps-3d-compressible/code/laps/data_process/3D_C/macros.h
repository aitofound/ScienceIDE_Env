#ifndef _MACROS_
#define _MACROS_

#define RHO(i,j,k) (((i) * ny + (j)) * nz + (k))
#define UX(i,j,k) (((nx + (i)) * ny + (j)) * nz + (k))
#define UY(i,j,k) (((2 * nx + (i)) * ny + (j)) * nz + (k))
#define UZ(i,j,k) (((3 * nx + (i)) * ny + (j)) * nz + (k))
#define BX(i,j,k) (((4 * nx + (i)) * ny + (j)) * nz + (k))
#define BY(i,j,k) (((5 * nx + (i)) * ny + (j)) * nz + (k))
#define BZ(i,j,k) (((6 * nx + (i)) * ny + (j)) * nz + (k))
#define P(i,j,k) (((7 * nx + (i)) * ny + (j)) * nz + (k))

#define JX(i,j,k) (((i) * ny + (j)) * nz + (k))
#define JY(i,j,k) (((nx + (i)) * ny + (j)) * nz + (k))
#define JZ(i,j,k) (((2 * nx + (i)) * ny + (j)) * nz + (k))

#define EX(i,j,k) (((i) * ny + (j)) * nz + (k))
#define EY(i,j,k) (((nx + (i)) * ny + (j)) * nz + (k))
#define EZ(i,j,k) (((2 * nx + (i)) * ny + (j)) * nz + (k))

#define IDX(v,i,j,k,n,m,p) ((((v) * (n) + (i)) * (m) + (j)) * (p) + (k))
#define IDXIJ(v,i,j,k) ((((v) * nx + (i)) * ny + (j)) * nz + (k))

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

#endif