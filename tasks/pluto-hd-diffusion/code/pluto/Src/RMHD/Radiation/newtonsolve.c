/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file 
  \brief  Newton's method, main functions

  Functions used to compute the Jacobian matrix during Newton's method, and
  to update the iterated fields in case RADIATION_FIXEDPOINT_GAS == YES.
  
  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 12, 2019
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if RADIATION
void RadNewtonJacobian (double * x, double * mf, double ** J, Rad_data * rad_data)
/*!
 * Compute and store in J the Jacobian matrix (dF^i/dx^j) of the system F(x) == 0,
 * calculated using difference quotients with small variations of the variables.
 * Store in mf the current value of -F(x).
 *
 * \param [in]      x       	vector of iterated fields
 * \param [in,out]  mf	      vector that stores the value of -F(x)
 * \param [in,out]  J	        Jacobian matrix
 * \param [in,out]  rad_data	pointer to rad_data structure, used in this case
 *                            to store the initial values of the radiation fields
 *                            as well as primitive fields
 * 
 *********************************************************************** */
{
  int i, j ;
  static double * y, * mf2, * dvars ; 
  double vel2, gvel;

  if(y == NULL){
    y = ARRAY_1D(RADIATION_NEQS, double);
    mf2 = ARRAY_1D(RADIATION_NEQS, double);
    dvars = ARRAY_1D(RADIATION_NEQS, double);
  }

  /*- Define increments to compute derivatives -*/
  for (i=0 ; i < RADIATION_NEQS ; i++ )
    dvars[i] = (fabs(x[i]) > 1e-20) ? -1e-4*x[i] : 1e-20 ;

  /*- Store the current value of -F(x) -*/
  RadNewtonMinusF(rad_data,x,mf);

  /*- Update auxiliary variables for error computation -*/
  #if RADIATION_FULL_CONVERGENCE == YES
    #if RADIATION_IMPL == RADIATION_NEWTON_GAS
    rad_data->exv = rad_data->pv[rad_data->pos][ENR] ; 
    #elif RADIATION_IMPL == RADIATION_NEWTON_RAD
    rad_data->exv = rad_data->pv[rad_data->pos][PRS] ;
    #endif
  #endif

  /*- Compute and store the elements of the Jacobian -*/
  for (i=0 ; i < RADIATION_NEQS ; i++ ) y[i] = x[i] ;

  for (j=0; j< RADIATION_NEQS ; j++ ){
    y[j] += dvars[j] ;

    RadNewtonMinusF(rad_data,y,mf2);

    for (i=0 ; i < RADIATION_NEQS ; i++ ) J[i][j] = ( mf[i] - mf2[i] ) / dvars[j] ;
    y[j] = x[j] ;
  }

}

void RadNewtonMinusF(Rad_data * rad_data, double * x, double * mf )
/*!
 * If Newton's method is used, compute and store in mf the components
 * of -F, being F the function whose roots are being looked for. Otherwise,
 * if RADIATION_FIXEDPOINT_GAS, perform a fixed-point iteration.
 *
 * \param [in,out]  rad_data	pointer to rad_data structure
 * \param [in]      x       	vector of iterated fields
 * \param [in,out]  mf	      vector that stores the value of -F(x)
 * 
 *********************************************************************** */
{
  int i, j, pos ;
  static int comps = RADIATION_NEQS - 1 ;
  double * u, u2, gamma, gamma2 ;
  double D, uD[3], uuD, uF ;
  double B, rhogamma, q, dt ;

  double abs_op, scat_op, tot_op ;

  double *v, *consv ;

  pos = rad_data-> pos ;
  v = rad_data-> pv[pos];
  consv = rad_data-> cv[pos];  

  u = rad_data-> u;
  dt = rad_data-> dt ;

  #if RADIATION_IMPL == RADIATION_NEWTON_GAS || RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS

    /*-- Convert iterated fields to primitive fields --*/
    RadIterToPrim (x, rad_data);

    /*-- Compute 4-velocity and derived quantities --*/
    gamma = rad_data-> gamma;
    gamma2 = rad_data-> gamma2;
    u2 = rad_data-> u2;

  #else

    /*-- Compute conserved fields --*/
    consv[ENR] = x[0];
    consv[ENG] = rad_data->Ttot[0] - x[0];
    for (i=0; i<comps; i++){
      consv[FR1+i] = x[i+1] ;
      consv[MX1+i] = rad_data->Ttot[i+1] - x[i+1] ;
    }

    /*-- Convert conserved to primitive fields --*/
    ConsToPrim (rad_data->cv, rad_data->pv, pos, pos, rad_data->flag);

    /*-- Compute 4-velocity and derived quantities --*/
    gamma = consv[RHO]/v[RHO] ;
    gamma2 = gamma*gamma ;
    u2 = gamma2 - 1.0 ;

    for (i=0; i<comps; i++) u[i] = gamma*v[VX1+i] ;

  #endif

  /*-- Set opacities --*/
  #if RADIATION_VAR_OPACITIES == YES
    UserDefOpacities (v, &abs_op, &scat_op);
    tot_op = abs_op + scat_op ; 
  #else
    abs_op = g_absorptionCoeff;
    scat_op = g_scatteringCoeff;
    tot_op = g_totalOpacity ;
  #endif

  /*-- Compute products involving the proper velocity --*/
  uuD = 0.;
  uF = 0.;
  for (i=0; i<comps; i++ ){
    uD[i] = 0.;
    uF += u[i]*v[FR1+i] ;
    for (j=0; j<comps; j++ ){
      D = EddTensor(v, FR1+j, FR1+i);
      uD[i] += u[j]*D;
      uuD += u[i]*u[j]*D ;
    }
  }
  
  /*-- Compute some useful quantities --*/
  rhogamma = consv[RHO] ;
  B = Blackbody( GetTemperature(v[RHO], v[PRS]) ) ;
  q = - abs_op*v[RHO]*B ;

  /*-- Compute -F[x] or update iterated fields if
       RADIATION_FIXEDPOINT_GAS is used --*/
  mf[0] = q * gamma 
       + rhogamma * ( abs_op - scat_op*( u2 + uuD ) ) * v[ENR]
       - v[RHO] * ( abs_op  - scat_op * (u2 + gamma2) ) * uF ;
       
  #if RADIATION_IMPL != RADIATION_FIXEDPOINT_GAS
    mf[0] = rad_data->Rini[0] - v[ENR] - dt*mf[0] ;
  #else
    consv[ENG] = rad_data-> Rini[0] + dt*mf[0] ;
    consv[ENR] = rad_data-> Ttot[0] - consv[ENG] ;
  #endif

	for (i=0; i<comps; i++){
    mf[i+1] = q * u[i] 
            - v[RHO] * ( tot_op * uD[i] 
                       + scat_op * u[i] * (gamma2 + uuD) ) * v[ENR]  
            + rhogamma * ( tot_op * v[FR1+i] 
                         + scat_op * 2.0 * uF * u[i] ) ;

    #if RADIATION_IMPL != RADIATION_FIXEDPOINT_GAS
      mf[i+1] = rad_data->Rini[i+1] - v[FR1+i] - dt*mf[i+1] ;
    #else
      consv[MX1+i] = rad_data-> Rini[i+1] + dt*mf[i+1] ;
      consv[FR1+i] = rad_data-> Ttot[i+1] - consv[MX1+i] ;
    #endif
  }
  
  #if RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
    /*-- Update Erad for error computation --*/
    #if RADIATION_FULL_CONVERGENCE == YES
    rad_data-> exv = consv[ENR] ;
    #endif
    
    /*-- Update iterated fields --*/
    ConsToPrim (rad_data-> cv, rad_data-> pv, pos, pos, rad_data-> flag) ;
    x[0] = v [PRS] ;
    gamma = consv[RHO]/v[RHO] ;
    for (i=0; i<comps; i++) x[i+1] = gamma * v[VX1+i] ;
  #endif

}


int RadIterToPrim (double *x, Rad_data *rad_data)
/*!
 * Conver iterated to primitive fields if iterations are carried on MHD fields
 *
 * \param [in]      x       	vector of iterated fields
 * \param [in,out]  rad_data	pointer to rad_data structure that contains
 *                            the primitive fields and the current position
 * \return Returns 0 if success, and 1 if prs == NaN is found.
 * 
 *********************************************************************** */
{
  int i, pos ;
  static int comps = RADIATION_NEQS - 1 ;
  double * v ;
  double * u = rad_data-> u ; 
  double u2, gamma, gamma2, gm1 ;
  double rhogamma, rhoh, vel2, vB, B2 ;
  double th, th2, wt ;
  #if EOS == IDEAL
    double gmmr = g_gamma/(g_gamma - 1.0) ;
  #endif

  /*-- Get primitive variables at the current position --*/
  pos = rad_data-> pos ;
  v = rad_data-> pv[pos] ;

  /*-- Set pressure --*/
  v[PRS] = x[0];

  /*-- Check p != NaN --*/
  if (v[PRS] != v[PRS]) {
    WARNING(
      print("! RadIterToPrim: NaN found while setting pressure, ");
      Where (pos, NULL);
    )
    return 1 ;
  } 

  /*-- Check p > 0 --*/
  if (v[PRS] < 0.0) {
    WARNING(
      print("! RadIterToPrim: negative pressure (%8.2e) during implicit step, ", v[PRS]);
      Where (pos, NULL);
    )			
    v[PRS]   = 1.e-20;
  } 
  
  /*-- Set 4-velocity and Lorentz factor --*/
  u2 = 0. ;
  for (i=0; i<comps; i++){
    u[i] = x[i+1] ;             
    u2 += u[i]*u[i];
  }
  gamma2 = u2 + 1.0 ;
  gamma = sqrt(gamma2) ;
  gm1 = 1.0/gamma ;

  /*-- Store gamma, gamma2 and u2 --*/
  rad_data-> u2 = u2 ;
  rad_data-> gamma = gamma ;
  rad_data-> gamma2 = gamma2 ;

  /*-- Set coordinate velocity --*/
  for (i=0; i<comps; i++) v[VX1+i] = gm1 * u[i] ;
    
  /*-- Set density --*/
  rhogamma = rad_data->cv[pos][RHO] ;
  v[RHO] = gm1 * rhogamma ;

  /*-- Set enthalpy density --*/
  #if EOS == IDEAL
    rhoh = v[RHO] + gmmr*v[PRS] ;
  #elif EOS == TAUB
    rhoh = 2.5*v[PRS] + sqrt(2.25*v[PRS]*v[PRS] + v[RHO]*v[RHO]);
  #endif

  /*-- Set radiation fields --*/
  #if PHYSICS == RHD

  v[ENR] = rad_data-> Ttot[0] - rhoh*gamma2 + v[PRS] ;
  for (i=0; i<comps; i++)
    v[FR1+i] = rad_data-> Ttot[i+1] - rhoh*gamma*u[i] ;

  #elif PHYSICS == RMHD

  vel2  = v[VX1]*v[VX1] + v[VX2]*v[VX2] + v[VX3]*v[VX3];
  vB    = v[VX1]*v[BX1] + v[VX2]*v[BX2] + v[VX3]*v[BX3];
  B2    = v[BX1]*v[BX1] + v[BX2]*v[BX2] + v[BX3]*v[BX3];

    #if RMHD_REDUCED_ENERGY == YES
      #if EOS == IDEAL
      v[ENR] = rad_data-> Ttot[0] - v[PRS]*(gamma2*gmmr - 1.0)
             - rhogamma*u2/(gamma+1.0) - 0.5*(B2*(1.0 + vel2) - vB*vB) ;
      #elif EOS == TAUB
      th = v[PRS]/v[RHO];
      th2 = th*th;
      v[ENR] = rad_data-> Ttot[0] - v[PRS]*(gamma2*2.5 - 1.0) 
             - rhogamma*(2.25*th2*gamma2 + u2)/(gamma*sqrt(1.0 + 2.25*th2) + 1.0)
             - 0.5*(B2*(1.0 + vel2) - vB*vB) ;
      #endif
    #else
      v[ENR] = rad_data-> Ttot[0] - rhoh*gamma2 + v[PRS]
             - 0.5*(B2*(1.0 + vel2) - vB*vB) ;
    #endif

  wt = rhoh*gamma2 + B2 ;
  for (i=0; i<comps; i++)
    v[FR1+i] = rad_data-> Ttot[i+1] - wt*v[VX1+i] + vB*v[BX1+i] ;

  #endif

  /*-- Check Er > 0 --*/
  if (v[ENR] < 0.0) {
    WARNING(
      print("! RadIterToPrim: negative radiation energy (%8.2e) during implicit step, ", v[ENR]);
      Where (pos, NULL);
    )			
    v[ENR]   = RADIATION_MIN_ERAD;
  } 

  return 0 ;
}
#endif
