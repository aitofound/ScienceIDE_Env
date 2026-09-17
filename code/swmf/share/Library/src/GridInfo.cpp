#include "GridInfo.h"
#include <stdio.h>

 void reset_grid_info(int* gridInfo, int nx, int ny, int nz) {
  const int nInt = ceil(nx * ny * nz / 8.0 / sizeof(int));
  for (int i = 0; i < nInt; i++) {
    gridInfo[i] = 0;
  }
}

 void set_point_status(int* gridInfo, int nx, int ny, int nz, int i,
                             int j, int k, int val) {

  const int nBitInt = sizeof(int) * 8;
  const int nBitShift = i * (ny * nz) + j * (nz) + k;

  const int nIntShift = floor(nBitShift / nBitInt);

  const int nBitShiftLoc = nBitShift - nIntShift * nBitInt;

  int& number = gridInfo[nIntShift];

  if (val == 1) {
    number |= 1 << nBitShiftLoc;
  } else if (val == 0) {
    number &= ~(1 << nBitShiftLoc);
  } else {
    abort();
  }
 }

 int get_point_status(int* gridInfo, int nx, int ny, int nz, int i, int j,
                      int k) {
   if(i>=nx || i<0 || j>=ny || j<0 || k>=nz || k<0) return 0; 
   const int nBitInt = sizeof(int) * 8;
   const int nBitShift = i * (ny * nz) + j * (nz) + k;

   const int nIntShift = floor(nBitShift / nBitInt);

   const int nBitShiftLoc = nBitShift - nIntShift * nBitInt;

   int number = gridInfo[nIntShift];

   int val = (number >> nBitShiftLoc) & 1;
   
   return val;
 }

void get_point_status(int* gridInfo, int nx, int ny, int nz, int i, int j,
		      int k, int *Val){
  *Val = get_point_status(gridInfo, nx, ny, nz, i, j, k); 
}

