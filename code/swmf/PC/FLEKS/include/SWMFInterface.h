#ifndef _SWMFINTERFACE_H_
#define _SWMFINTERFACE_H_

#include <sstream>

#include <mpi.h>

#ifdef _PT_COMPONENT_
extern "C" {
void OH_get_charge_exchange_wrapper(double *rhoIon, double *cs2Ion,
                                    double uIon_D[3], double *rhoNeu,
                                    double *cs2Neu, double uNeu_D[3],
                                    double sourceIon_V[5],
                                    double sourceNeu_V[5]);

void OH_get_charge_exchange_region(int *iRegion, double *r, double *rhoDim,
                                   double *u2Dim, double *uSW2Dim,
                                   double *tempDim, double *tempPu2Dim,
                                   double *mach2, double *machPUI2,
                                   double *machSW2, double *levHP);

void OH_get_solar_wind(double *x, double *y, double *z, double *numDen,
                       double *ur, double *temp, double b[3]);
}
#else
inline void OH_get_charge_exchange_wrapper(double *rhoIon, double *cs2Ion,
                                           double uIon_D[3], double *rhoNeu,
                                           double *cs2Neu, double uNeu_D[3],
                                           double sourceIon_V[5],
                                           double sourceNeu_V[5]) {}

inline void OH_get_charge_exchange_region(int *iRegion, double *r,
                                          double *rhoDim, double *u2Dim,
                                          double *uSW2Dim, double *tempDim,
                                          double *tempPu2Dim, double *mach2,
                                          double *machPUI2, double *machSW2,
                                          double *levHP) {}

inline void OH_get_solar_wind(double *x, double *y, double *z, double *numDen,
                              double *ur, double *temp, double b[3]) {}

#endif

#endif
