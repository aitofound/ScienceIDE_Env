#include "pluto.h"
#include "les.h"

/* *************************************************************** */
void ComputeUserVar (const Data *d, Grid *grid)
/*
 *
 *  PURPOSE
 *
 *    Define user-defined output variables
 *
 *
 *
 ***************************************************************** */
{
  int i, j, k;  
  double ***H, ***f;

  f = GetUserVar("H");
  H = LES_GetFilter();
  if (H == NULL) DOM_LOOP(k,j,i) f[k][j][i] = 0.0;
  else{
    DOM_LOOP(k,j,i) f[k][j][i] = H[k][j][i];
  }

}
/* ************************************************************* */
void ChangeDumpVar ()
/* 
 *
 * 
 *************************************************************** */
{ 
  Image *image;

}





