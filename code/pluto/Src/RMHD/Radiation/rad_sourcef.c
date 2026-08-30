#include "pluto.h"

void RadSourceFunction(double *primvar, double *G )
/*!
 * Compute the Eulerian frame components of the source function G^\mu,
 * that accounts for the interaction between radiation and matter. 
 *
 * \param [in]   primvar  vector of primitive fields
 * \param [out]  G       	vector that stores the value of G^\mu
 * 
 *********************************************************************** */
{
  int i, j ;
  static int comps = RADIATION_NEQS - 1 ;
  double u[3], u2, gamma, gamma2, D, uD[3], uuD, uF ;
  double B, rho, rhogamma, q ;

  /*-- Set opacities --*/
  double abs_op, scat_op, tot_op ;
  #if RADIATION_VAR_OPACITIES == YES
    UserDefOpacities (primvar, &abs_op, &scat_op);
    tot_op = abs_op + scat_op ; 
  #else
    abs_op = g_absorptionCoeff;
    scat_op = g_scatteringCoeff;
    tot_op = g_totalOpacity ;
  #endif

  /*-- Compute gamma, gamma^2, u and u^2 --*/
  gamma = primvar[VX1]*primvar[VX1]
        + primvar[VX2]*primvar[VX2]
        + primvar[VX3]*primvar[VX3] ;
  u2 = gamma ;
  gamma2 = 1.0/(1.0-gamma) ;
  u2 *= gamma2 ;
  gamma = sqrt(gamma2) ;
  for (i=0; i<comps; i++) u[i] = gamma * primvar[VX1+i] ;

  /*-- Compute products involving the proper velocity --*/
  uuD = 0.;
  uF = 0.;
  for (i=0; i<comps; i++ ){
    uD[i] = 0.;
    uF += u[i]*primvar[FR1+i] ;
    for (j=0; j<comps; j++ ){
      D = EddTensor(primvar, FR1+j, FR1+i);
      uD[i] += u[j]*D;
      uuD += u[i]*u[j]*D ;
    }
  }
  
  /*-- Compute some useful quantities --*/
  rho = primvar[RHO] ;
  rhogamma = rho*gamma ;
  B = Blackbody( GetTemperature(rho, primvar[PRS]) ) ;
  q = - abs_op*rho*B ;

  /*-- Compute source function in the Eulerian frame --*/
  G[0] = q * gamma 
       + rhogamma * ( abs_op - scat_op*( u2 + uuD ) ) * primvar[ENR]
       - rho * ( abs_op  - scat_op * (u2 + gamma2) ) * uF ;

	for (i=0; i<comps; i++)
    G[i+1] = q * u[i] 
           - rho * ( tot_op * uD[i] 
                   + scat_op * u[i] * (gamma2 + uuD) ) * primvar[ENR]  
           + rhogamma * ( tot_op * primvar[FR1+i]
                        + scat_op * 2.0 * uF * u[i] ) ;
}


void AddRadSource1(Data_Arr consv, Data_Arr source, RBox *box, double dt)
/*!
 *  Update the conserved fields using a single source function (used for
 *  IMEX schemes). Conserved fields are updated as U = U + dt*source . 
 *
 * \param [in,out]  consv     Cell-centered conserved fields.
 * \param [in]      source    Cell-centered source terms.
 * \param [in,out]  RBox    	Pointer to RBox structure.
 * \param [in]      dt    	  Time step.
 * 
 *********************************************************************** */
{
  int i , j, k, n;
  static int comps = RADIATION_NEQS - 1 ;
  int ibeg, iend, jbeg, jend, kbeg, kend ;
  double * u, * S ;
  
  ibeg = (box->ibeg <= box->iend) ? (iend=box->iend, box->ibeg):(iend=box->ibeg, box->iend);
  jbeg = (box->jbeg <= box->jend) ? (jend=box->jend, box->jbeg):(jend=box->jbeg, box->jend);
  kbeg = (box->kbeg <= box->kend) ? (kend=box->kend, box->kbeg):(kend=box->kbeg, box->kend);

  for (k = kbeg; k <= kend; k++){ g_k = k;
  for (j = jbeg; j <= jend; j++){ g_j = j;
  for (i = ibeg; i <= iend; i++){ g_i = i;

    u = consv[k][j][i];
    S  = source[k][j][i];

    u[ENG]   += dt*S[0] ;
    u[ENR]   -= dt*S[0] ;
    for (n=0; n<comps; n++ )  {
        u[MX1+n] += dt*S[n+1] ;
        u[FR1+n] -= dt*S[n+1] ;
    }

  }}}
}

void AddRadSource2(Data_Arr consv, Data_Arr source1, Data_Arr source2,
                  RBox *box, double dt1, double dt2)
/*!
 *  Update the conserved fields using two different source functions
 *  (used for IMEX schemes). Conserved fields are updated as U = U +
 *  dt1*source1 + dt2*source2. 
 *
 * \param [in,out]  consv     Cell-centered conserved fields.
 * \param [in]      source1   Cell-centered source terms.
 * \param [in]      source2   Cell-centered source terms.
 * \param [in,out]  RBox    	Pointer to RBox structure.
 * \param [in]      dt1    	  First time step (used with source1).
 * \param [in]      dt2    	  Second time step (used with source2).
 * 
 *********************************************************************** */
{
  int i , j, k, n;
  static int comps = RADIATION_NEQS - 1 ;
  int ibeg, iend, jbeg, jend, kbeg, kend ;
  double * u, * S1, * S2 ;
  
  ibeg = (box->ibeg <= box->iend) ? (iend=box->iend, box->ibeg):(iend=box->ibeg, box->iend);
  jbeg = (box->jbeg <= box->jend) ? (jend=box->jend, box->jbeg):(jend=box->jbeg, box->jend);
  kbeg = (box->kbeg <= box->kend) ? (kend=box->kend, box->kbeg):(kend=box->kbeg, box->kend);

  for (k = kbeg; k <= kend; k++){ g_k = k;
  for (j = jbeg; j <= jend; j++){ g_j = j;
  for (i = ibeg; i <= iend; i++){ g_i = i;

    u = consv[k][j][i];
    S1  = source1[k][j][i];
    S2  = source2[k][j][i];

    u[ENG]   += ( dt1*S1[0] + dt2*S2[0] ) ;
    u[ENR]   -= ( dt1*S1[0] + dt2*S2[0] ) ;
    for (n=0; n<comps; n++ )  {
        u[MX1+n] += ( dt1*S1[n+1] + dt2*S2[n+1] ) ;
        u[FR1+n] -= ( dt1*S1[n+1] + dt2*S2[n+1] ) ;
    }

  }}}
}
