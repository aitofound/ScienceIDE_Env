/* ---------------------------------------------------------------------
   LES module header file 
   --------------------------------------------------------------------- */

#ifndef LES_FMIN
  #define LES_FMIN   0.5      /* blah blah  */
#endif

void LES_ViscousFlux (const Data *d, Data_Arr flux, Grid *grid);
double ***LES_GetFilter();

