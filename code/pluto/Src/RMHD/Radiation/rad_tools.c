/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Some functions used by the radiation module.
  
  Contains some functions used to compute radiation-matter interaction
  terms, and to solve systems of linear equations during the implicit step.
  
  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 12, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */

#include "pluto.h"
double Blackbody ( double temperature )
/*!
 * Return the blackbody intensity corresponding to the input temperature.
 *
 * \param [in]	temperature	Input temperature.
 * 
 * \return	Return (4*PI)B(T)=radiationConst*T^4.
 *********************************************************************** */
{
	double T4 = temperature*temperature;
	T4 = T4*T4;
	return g_radiationConst * T4;
}

double GetTemperature (double rho, double prs )
/*!
 * Return the (ideal) gas temperature corresponding to given gas pressure
 * and density.
 *
 * \param [in]	rho		Input mass density.
 * \param [in]	prs		Input (gas) pressure.
 * 
 * \return	Return T = g_idealGasConst*prs/rho, where g_idealGasConst is the
 *			equilibrium ratio prs/(T*rho) for the considered ideal gas.
 *********************************************************************** */
{
	return g_idealGasConst * prs / rho;
}

void RadFPMatrices (Rad_data * rad_data, double * b, double ** C)
/*!
 * Compute the components of the matrix C and the column vector in eq.
 * (52) of Takahashi et al. 2013, ApJ 772(2):127. Used if RADIATION_IMPL
 * == RADIATION_FIXEDPOINT_RAD.
 *
 * \param [in]      rad_data  	pointer to rad_data structure
 * \param [out]     b         	column vector
 * \param [out]     C           matrix C
 * 
 *********************************************************************** */
{
  int i, j, pos ;
  static int comps = RADIATION_NEQS - 1 ;
  double * primvar ;
  double u[3], u2, gamma, gamma2;
  double D, uD[3], uuD, uF ;
  double rho, B, rhogamma, dt, mdt_rho, coeff1, coeff2 ;
  double abs_op, scat_op, tot_op ;

  /*-- Get primitive variables at the current position --*/
  pos = rad_data-> pos ;
  primvar = rad_data-> pv[pos] ;

  /*-- Set opacities --*/
  #if RADIATION_VAR_OPACITIES == YES
    UserDefOpacities (primvar, &abs_op, &scat_op);
    tot_op = abs_op + scat_op ; 
  #else
    abs_op = g_absorptionCoeff;
    scat_op = g_scatteringCoeff;
    tot_op = g_totalOpacity ;
  #endif

  /*-- Get dt --*/
  dt = rad_data->dt ;

  /*-- Set gamma, gamma^2, u and u^2 --*/
	gamma = primvar[VX1]*primvar[VX1] 
        + primvar[VX2]*primvar[VX2]
				+ primvar[VX3]*primvar[VX3] ;
	u2 = gamma ;
	gamma2 = 1.0/(1.0-gamma) ;
	u2 *= gamma2 ;
  gamma = sqrt(gamma2) ;
	for (i=0; i<comps; i++) u[i] = gamma * primvar[VX1+i] ;

  /*-- Set products of u and the Eddington tensor --*/
  uuD = 0.;
  for (i=0; i<comps; i++ ){
    uD[i] = 0.;
    for (j=0; j<comps; j++ ){
      D = EddTensor(primvar, FR1+j, FR1+i);
      uD[i] += u[j]*D;
      uuD += u[i]*u[j]*D ;
    } 
  }

  /*-- Set some constants used to calculate C and b  --*/
  rho = primvar[RHO] ;
  rhogamma = rad_data->cv[pos][RHO] ;
  mdt_rho = -dt*rho ;
  B = Blackbody( GetTemperature(rho, primvar[PRS]) ) ;
  coeff1 = dt * abs_op * rho * B ;
  coeff2 = gamma * mdt_rho ;

  /*-- Set coefficients of the system C.(E,F^i)=b  --*/ 	
  b[0] = rad_data-> Rini[0] + coeff1 * gamma ;	
  C[0][0] = 1.0 - dt * rhogamma * ( scat_op*( u2 + uuD ) - abs_op );

	for (i=0; i<comps; i++){
    b[i+1] = rad_data-> Rini[i+1] + coeff1 * u[i] ;

    C[0][i+1] = mdt_rho * u[i] * ( abs_op - scat_op * (u2 + gamma2) ) ;
    C[i+1][0] = mdt_rho * ( tot_op * uD[i] + scat_op * u[i] * (gamma2 + uuD) ) ;

    for (j=i; j<comps; j++)
      C[i+1][j+1] = C[j+1][i+1] = - coeff2 * scat_op * 2.0 * u[i] * u[j] ;

    C[i+1][i+1] += 1.0 - coeff2 * tot_op ;
  }
}

int GaussianSolve (double **C, double *b, double *x , int N)
/*!
 * Solve the system of linear equations given by C.x=b, using
 * gaussian elimination with scaled partial pivoting (see e.g.
 * Burden R. L. and Faires J. D., Numerical Analysis).
 *
 * \param [in]		C					matrix of coefficients.
 * \param [in]		b					vector of constant terms.
 * \param [in]		N					dimension of the system.
 * \param [out]		x					vector of solutions.
 * 
 * \return	Returns 1 if the system is solved, and 0 if no unique
 *			solution exists.
 *
 *********************************************************************** */
{
	int i, j, k, p;
	int nrow[N] ; // Row vector, used for permutations of rows.
	double m[N] ; // Vector of coefficients used during the elimination.
	double s[N] ; // Vector of scaling factors.
	double A[N][N+1] ; // Augmented matrix of the system.
	double max_fabsA, sum, scale, q;

/*-- Store C and b values in A --*/
	for (i=0; i<N; i++) {
		for (j=0; j<N; j++) A[i][j] = C[i][j];
		A[i][N] = b[i];
	}

/*-- Initialize row pointer --*/
	for (i=0; i<N; i++) nrow[i] = i;

/*-- Compute scaling factors --*/
	for (i=0; i<N; i++) {
		max_fabsA = 0. ;
		for (j=0; j<N; j++) {
			scale = fabs(A[i][j]) ;
			if ( scale > max_fabsA ) max_fabsA = scale;
		}
		if ( max_fabsA == 0 ) {
			print("! GaussianSolve error: no unique solution exists. \n") ;
			return 1;
		} else {
			s[i] = max_fabsA ;	
		}
	}

/*-------------------------------------
    	Elimination process
  -------------------------------------*/
	for (i=0; i<N-1; i++) {

		p = i;
		max_fabsA = fabs(A[nrow[i]][i]) / s[nrow[i]];	

/*-- Get maximum quotient |A[nrow[i]][j]|/s[nrow[j]] for scaled pivoting --*/	
		for(j=i+1; j<N; j++) {
			q = fabs(A[nrow[j]][i]) / s[nrow[j]] ;
			if( q > max_fabsA ) p = j ;
		}

/*-- Check if solution is unique --*/
		if ( A[nrow[p]][i] == 0. ) {
			print("! GaussianSolve error: no unique solution exists. \n") ; 
			return 1;
		}

/*-- Switch rows --*/
		if ( nrow[i]!=nrow[p] ) {
			j = nrow[i] ;
			nrow[i] = nrow[p] ;
			nrow[p] = j ;
		}

/*-- Elimination --*/
		for (j=i+1; j<N; j++) {
			m[j] = A[nrow[j]][i]/A[nrow[i]][i] ;
			for (k=i; k<N+1; k++) {
				A[nrow[j]][k] -= m[j]*A[nrow[i]][k] ;
			}
		}

	}
/*-------------------------------------
    	End of elimination
  -------------------------------------*/

/*-- Check if solution is unique for the last time --*/
	if ( A[nrow[N-1]][N-1] == 0. ) {
		print("! GaussianSolve error: no unique solution exists. \n") ; 
		return 1;
	}
		
/*-- Backward substitution --*/
	x[N-1] = A[nrow[N-1]][N]/A[nrow[N-1]][N-1] ;

	for (i=N-2; i>=0; i-- ) {
		sum=0;
		for(j=i+1; j<N; j++) {
			sum += A[nrow[i]][j]*x[j] ;
		}
		x[i] = ( A[nrow[i]][N] - sum ) / A[nrow[i]][i] ;
	}

/*-- Output when no errors occur  --*/
	return 0;

}

