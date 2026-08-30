#include "pluto.h"

void Init (double *v, double x1, double x2, double x3)
{
  double vel_sw, cs, R;
  double  rho;

  rho = g_inputParam[RHO_AMB];
  cs  = g_inputParam[CS_AMB];

  vel_sw = g_inputParam[VEL_SW];
  
  v[RHO] = rho;
  v[PRS] = cs*cs*rho/g_gamma;



  #if GEOMETRY == CARTESIAN
   v[VX1] = 0.0;         
   v[VX2] = 0.0;         
   v[VX3] = 0.0;       
  #endif

  v[TRC] = 0.0;
 g_smallPressure = 1.e-5; 
/* g_smallDensity = 1.e-5; */ 
}

/* **************************************************************** */
void Analysis (const Data *d, Grid *grid)
/*
 *
 ****************************************************************** */
{

}

/* ********************************************************************* */
void UserDefBoundary (const Data *d, RBox * box, int side, Grid *grid) 
/*
 * Sets inflow boundary condition at the top boundary (side == X2_END)
 * and the stellar wind region when side == 0.
 *
 *********************************************************************** */
{
  int   i, j, k;
  double *x1, *x2, *x3;
  double  r, r0, r1, cs, vel_sw;
  double  Vwind  = 1.0, rho, vr;

  x1 = grid[IDIR].x;
  x2 = grid[JDIR].x;
  x3 = grid[KDIR].x;

  if (side == 0){
    cs = g_inputParam[CS_CHIOMA];
    TOT_LOOP(k,j,i){
      r0 = 25;
      r1 = 250; 
      r  = sqrt(x1[i]*x1[i] + x2[j]*x2[j] + x3[k]*x3[k]);
    if (r <= r1){
	if(r <= r0){
         vr    = tanh(r/r0/0.1)*Vwind;
         rho   = Vwind*r0*r0*1.e+8/(vr*r*r);
         d->Vc[RHO][k][j][i] = rho;
         d->Vc[PRS][k][j][i] = cs*cs/g_gamma*pow(rho,g_gamma);
         d->flag[k][j][i]   |= FLAG_INTERNAL_BOUNDARY;}
	else{ 	
         vr    = tanh(r/r1/0.1)*Vwind;
         rho   = Vwind*r1*r1*5000/(vr*r*r);
	 d->Vc[RHO][k][j][i] = rho;
         d->Vc[PRS][k][j][i] = cs*cs/g_gamma*pow(rho,g_gamma); 
         d->Vc[VX1][k][j][i] = Vwind*x1[i]/r;
    /*   d->Vc[VX2][k][j][i] = Vwind*x2[j]/r; */
         d->Vc[VX3][k][j][i] = Vwind*x3[k]/r;
            if(x2[j] < 0){d->flag[k][j][i]   |= FLAG_INTERNAL_BOUNDARY;}}
             }
           }
         }
  
  if (side == X1_BEG){  /* -- X1_BEG boundary -- */}

  if (side == X1_END){  /* -- X1_END boundary -- */}

  if (side == X2_BEG){  /* -- X2_BEG boundary -- */
     X2_BEG_LOOP(k,j,i){
	  vel_sw = g_inputParam[VEL_SW];
	  cs = g_inputParam[CS_AMB];
          rho = g_inputParam[RHO_AMB];
	  d->Vc[RHO][k][j][i] = rho;
          d->Vc[VX1][k][j][i] = 0.0;
          d->Vc[VX2][k][j][i] = vel_sw;
          d->Vc[VX3][k][j][i] = 0.0;	
			 }
  }

  if (side == X2_END){  /* -- X2_END boundary -- */
    X2_END_LOOP(k,j,i){}
  }

  if (side == X3_BEG){  /* -- X3_BEG boundary -- */}
  
 
  if (side == X3_END){  /* -- X3_END boundary -- */}
  
}


