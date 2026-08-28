#include "pluto.h"

#if RADIATION
double EddTensor (double *v, int i, int j){
/* --------------------------------------------------------
		Compute the (i,j) component of the Eddington tensor
		using the M1 closure, taking as input the state v.
		(i,j) values must always be within FR1 and FR3.
   -------------------------------------------------------- */

  double ni, nj, f2, chi_2, edt ;

  double flux_module = v[FR1]*v[FR1] + v[FR2]*v[FR2] + v[FR3]*v[FR3] ;

  if (flux_module < 1e-150 || v[ENR] < 1e-150 ) {
    if ( i==j ) return 0.33333333333333 ; 
    return 0.0 ;
  }

  flux_module = sqrt(flux_module) ;

  ni = v[i]/flux_module;
  nj = v[j]/flux_module ;		

	if( flux_module > v[ENR] ) return ni*nj ;

  f2 = flux_module/v[ENR] ;
  f2 = f2*f2 ;
  chi_2 = ( 1.5 + 2.0*f2 )/( 5.0 + 2.0*sqrt(4.0-3.0*f2) ) ;
  edt = (3.0*chi_2-0.5)*ni*nj ;

  if ( i==j ) edt += 0.5 - chi_2 ;

  return edt ;
}
#endif
