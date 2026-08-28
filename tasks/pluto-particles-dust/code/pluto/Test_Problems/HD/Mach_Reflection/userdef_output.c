#include "pluto.h"

/* *************************************************************** */
void ComputeUserVar (const Data *d, Grid *grid)
/*
 *  PURPOSE
 *
 *    Define user-defined output variables
 *
 ***************************************************************** */
{
  int i, j, k;  
  double ***flag;

  flag = GetUserVar("flag");
  DOM_LOOP(k,j,i){
    flag[k][j][i] = d->flag[k][j][i];
  }

}
/* ************************************************************* */
void ChangeOutputVar ()
/* 
 *
 * 
 *************************************************************** */
{ 

}





