/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file 
  \brief  Error computation for the radiation implicit step

  Compute errors between iterations to be used by the convergence
  criterion during the implicit integration of the radiation-matter
  source terms.
  
  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 12, 2019

*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"
#if RADIATION
double RadErr (double * x, double * dx, Rad_data * rad_data)
/*!
 * Compute the sum of the squares of the relative differences of the
 * several fields considered in the implicit step.
 *
 * \param [in]      x         Vector of primitive fields if RADIATION_FIXEDPOINT_RAD
 *                            and conserved fields if RADIATION_FIXEDPOINT_GAS.
 *                            Otherwise, it contains the iterated fields.
 * \param [in]      dx        Vector that stores the variations of
 *                            the iterated fields if Newton's method is used.
 * \param [in,out]  rad_data	Pointer to rad_data structure, used to store
 *                            fields from the previous iteration.
 * 
 *********************************************************************** */
{
  int i ;
  static int comps = RADIATION_NEQS - 1 ;
  double err, er1, erf, mod ;

  #if RADIATION_IMPL == RADIATION_FIXEDPOINT_RAD 

    /*-- Compute sum of squared relative differences --*/
    er1 = fabs(x[ENR] - rad_data-> Rprev[0]) / fabs(rad_data-> Rprev[0]) ;
    err = er1*er1 ;
 
    mod = 0; er1 = 0;
    for (i=0; i<comps; i++){
      mod += rad_data-> Rprev[i+1] * rad_data-> Rprev[i+1];
      erf = x[FR1+i] - rad_data-> Rprev[i+1];
      er1 += erf*erf;
    }  
    err += (mod > 1e-40) ? er1/mod : er1/1e-40 ;

    /*-- Update previous step variables --*/
    rad_data->Rprev[0] = x[ENR] ;
    for (i=0; i<comps; i++) rad_data->Rprev[i+1] = x[FR1+i];
    
    /*-- Update previous additional variables if needed --*/
    #if RADIATION_FULL_CONVERGENCE == YES
    er1 = fabs(x[PRS] - rad_data-> exv_prev) / fabs(rad_data-> exv_prev) ;
    err += er1*er1 ;
    rad_data->exv_prev = x[PRS] ;
    #endif
    
  #elif RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
  
    /*-- Compute sum of squared relative differences --*/
    er1 = fabs(x[0] - rad_data-> Rprev[0]) / fabs(rad_data-> Rprev[0]) ;
    err = er1*er1 ;

    mod = 0; er1 = 0;
    for (i=1; i<comps+1; i++){
      mod += rad_data-> Rprev[i] * rad_data-> Rprev[i];
      erf = x[i] - rad_data-> Rprev[i];
      er1 += erf*erf;
    }  
    err += (mod > 1e-40) ? er1/mod : er1/1e-40 ;
    
    /*-- Update previous step variables --*/
    rad_data->Rprev[0] = x[0] ;
    for (i=1; i<comps+1; i++) rad_data->Rprev[i] = x[i]; 
    
    /*-- Update previous additional variables if needed --*/
    #if RADIATION_FULL_CONVERGENCE == YES
    er1 = fabs(rad_data->exv - rad_data->exv_prev) / fabs(rad_data->exv_prev) ;
    err += er1*er1 ;
    rad_data->exv_prev = rad_data->exv ;
    #endif

  #else

    /*-- Compute sum of squared relative differences --*/
    er1 = dx[0]/fabs(x[0]) ; err = er1*er1 ;
    mod = 0. ;
    er1 = 0. ;
    for (i=0; i<comps; i++){
      mod += x[i]*x[i] ;
      er1 += dx[i]*dx[i] ;
    }
    err += (mod > 1e-40) ? er1/mod : er1/1e-40 ;

    /*-- Update previous additional variables if needed --*/
    #if RADIATION_FULL_CONVERGENCE == YES
    err += fabs(rad_data->exv - rad_data->exv_prev) / fabs(rad_data->exv_prev) ;
    rad_data->exv_prev = rad_data->exv ;
    #endif

  #endif

  return err ;
}
#endif
