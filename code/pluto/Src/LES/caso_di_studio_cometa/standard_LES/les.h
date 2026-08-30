#define INCLUDE_LES  YES

void LES_ViscousFlux (const Data *d, Data_Arr flux, Grid *grid);
double ***LES_GetFilter();

