#define INCLUDE_LES  NO

void LES_ViscousFlux (const Data *d, Data_Arr flux, Grid *grid);
double ***LES_GetFilter();

