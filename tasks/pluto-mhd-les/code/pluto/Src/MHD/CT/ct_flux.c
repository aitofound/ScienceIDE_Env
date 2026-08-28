/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Define point and diffusive fluxes for CT.

  Compute the "smooth" and "diffusive" terms of the induction
  equations fluxes.
  These will be later reconstructed in the transverse direction
  to obtained the edge-centered electric field in the constrained
  transport (CT) method.

  \authors A. Mignone (andrea.mignone@unito.it)\n

 \b References
    - "Systematic construction of upwind constrained transport schemes for MHD" \n
      Mignone et al, JCP (2021) 109748

  \date  Feb 8, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#define NDIM  (INCLUDE_IDIR + INCLUDE_JDIR + INCLUDE_KDIR)
/* ********************************************************************* */
void CT_Flux(const Sweep *sweep, int beg, int end, Grid *grid)
/*!
 * Compute smooth and diffusive contributions to the flux.
 * The following quantities must be well defined entering this function:
 *
 * - \c sweep->stateL->flux
 * - \c sweep->stateR->flux
 * - \c sweep->flux
 * - \c sweep->SL
 * - \c sweep->SR.
 *
 * \param [in,out]  sweep  pointer to swep structure.
 * \param [in]      beg    initial index of computation 
 * \param [in]      end    final   index of computation
 * \param [in]      grid   pointer to Grid structure
 *
 *********************************************************************** */
{
  int  nv, i;

  const State   *stateL = &(sweep->stateL);
  const State   *stateR = &(sweep->stateR);

  double phit, phib, lambda, u;
  double aR, aL, scrh, Bn;
  double *vL, *uL, *fL;
  double *vR, *uR, *fR;
  double *flux;
  double *SL = sweep->SL, *SaL = sweep->SaL, *pL = stateL->prs;
  double *SR = sweep->SR, *SaR = sweep->SaR, *pR = stateR->prs;
  double *Sc = sweep->Sc;

  #if CT_EMF_AVERAGE == CT_FLUX
  for (i = beg; i <= end; i++){
    fL = stateL->flux[i];
    fR = stateR->flux[i];

    phit = sweep->flux[i][BXt] - 0.5*(fL[BXt] + fR[BXt]);
    phib = sweep->flux[i][BXb] - 0.5*(fL[BXb] + fR[BXb]);

    sweep->pnt_flux[i][BXt] = 0.5*(fL[BXt] + fR[BXt]);
    sweep->pnt_flux[i][BXb] = 0.5*(fL[BXb] + fR[BXb]);
    sweep->dff_flux[i][BXt] = phit;
    sweep->dff_flux[i][BXb] = phib;
  }
  #endif

  #if CT_EMF_AVERAGE == UCT_GFORCE
//    printLog ("! CT_Flux(): should not get here\n");
//    QUIT_PLUTO(1);

  for (i = beg; i <= end; i++){

    double uLW[NVAR];
    double lambda = MAX(fabs(SL[i]), fabs(SR[i]));
    double dtdx = g_dt/grid->dx[g_dir][i];
    double nu   = RuntimeGet()->cfl;
    double om, vxn, vxt, vxb;

    vL = stateL->v[i]; uL = stateL->u[i]; fL = stateL->flux[i];
    vR = stateR->v[i]; uR = stateR->u[i]; fR = stateR->flux[i];

    dtdx = 1.0/lambda;
    #ifndef GFORCE_OMEGA
    #if DIMENSIONS < 3
    om   = 1.0/(1.0 + nu);
    #else
    om   = 1.0/(1.0 + 2*nu);
    #endif
    #else
    om   = GFORCE_OMEGA;
    #endif
    
    NVAR_LOOP(nv) uLW[nv] = 0.5*(uL[nv] + uR[nv]) - 0.5*dtdx*(fR[nv] - fL[nv]);

    vxn = 0.5*(vL[VXn] + vR[VXn]);
    vxt = 0.5*(vL[VXt] + vR[VXt]);
    vxb = 0.5*(vL[VXb] + vR[VXb]);

    double vxn_LW = uLW[MXn]/uLW[RHO];
    double vxt_LW = uLW[MXt]/uLW[RHO];
    double vxb_LW = uLW[MXb]/uLW[RHO];
    double fn = dtdx*vxn_LW;
    sweep->pnt_flux[i][BXt] = -om*(vxt_LW - 0.5*fn*(vR[VXt] - vL[VXt])) - (1.0 - om)*vxt;
    sweep->pnt_flux[i][BXb] = -om*(vxb_LW - 0.5*fn*(vR[VXb] - vL[VXb])) - (1.0 - om)*vxb;
    sweep->dL[i] = 0.5*(om*dtdx*vL[VXn]*vxn_LW + (1.0 - om)*lambda);
    sweep->dR[i] = 0.5*(om*dtdx*vR[VXn]*vxn_LW + (1.0 - om)*lambda);
    sweep->aL[i] = 0.5;
    sweep->aR[i] = 0.5;
  }
  #endif

  #if CT_EMF_AVERAGE == UCT_HLLD
  #if PHYSICS != MHD
    /* The relativistic version is done "in place", directly in the 
       gforce riemann solver */
    #error Cannot use UCT_HLLD in Relativistic MHD
  #endif
  for (i = beg; i <= end; i++){
    vL = stateL->v[i]; uL = stateL->u[i]; fL = stateL->flux[i];
    vR = stateR->v[i]; uR = stateR->u[i]; fR = stateR->flux[i];

    flux = sweep->flux[i];  
    Bn   = sweep->Bn[i];

    int switch_to_hll = 0;
    double chiL, chiR, nuLR, nuL, nuR;
    double eps = 1.e-12*(fabs(SL[i]) + fabs(SR[i]));
    double duL  = SL[i] - vL[VXn];
    double duR  = SR[i] - vR[VXn];

    #if EOS == IDEAL
    if (1) {  /* Recompute speeds ?  */
      double sqrL, sqrR, usL[NFLX], usR[NFLX];
      double *ptL = stateL->prs,  *ptR = stateR->prs;

      scrh  = 1.0/(duR*uR[RHO] - duL*uL[RHO]);
      Sc[i] = (duR*uR[MXn] - duL*uL[MXn] - ptR[i] + ptL[i])*scrh;
      
      usL[RHO] = uL[RHO]*duL/(SL[i] - Sc[i]);
      usR[RHO] = uR[RHO]*duR/(SR[i] - Sc[i]);
      
      sqrL = sqrt(usL[RHO]);
      sqrR = sqrt(usR[RHO]);
      
      SaL[i] = Sc[i] - fabs(Bn)/sqrL;
      SaR[i] = Sc[i] + fabs(Bn)/sqrR;

      if (usL[RHO] < 0.0 || usR[RHO] < 0.0){
        printLog ("! CT_Flux(): rhoL = %10.3e, rhoR = %10.3e\n",usL[RHO], usR[RHO]);
        printLog ("  Waves = %8.3e  %8.3e  %8.3e  %8.3e  %8.3e\n",
               SL[i], SaL[i], Sc[i], SaR[i],SR[i]);
        printLog ("  duL, duR = %12.6e, %12.6e\n",duL, duR);
        QUIT_PLUTO(1);
      }

    }
    chiL  = (vL[VXn] - Sc[i])*(SL[i] - Sc[i])/(SaL[i] + SL[i] - 2.0*Sc[i]);
    chiR  = (vR[VXn] - Sc[i])*(SR[i] - Sc[i])/(SaR[i] + SR[i] - 2.0*Sc[i]);
    #elif EOS == ISOTHERMAL
    scrh    = 1.0/(SR[i] - SL[i]);
    double rho_h   = (uR[RHO]*duR - uL[RHO]*duL)*scrh;
    Sc[i] = 0.5*(SaL[i] + SaR[i]);
    if (1) {  /* Recompute speeds ?  */
      double sqrho_h = sqrt(rho_h);
      SaL[i] = Sc[i] - fabs(Bn)/sqrho_h;
      SaR[i] = Sc[i] + fabs(Bn)/sqrho_h;
    }

    chiL  = (vL[VXn] - Sc[i])*(SL[i] - Sc[i])/(SaL[i] + SL[i] - 2.0*Sc[i]);
    chiR  = (vR[VXn] - Sc[i])*(SR[i] - Sc[i])/(SaR[i] + SR[i] - 2.0*Sc[i]);
    #endif

    nuL  = (SaL[i] + SL[i])/(fabs(SaL[i]) + fabs(SL[i]));
    nuR  = (SaR[i] + SR[i])/(fabs(SaR[i]) + fabs(SR[i]));
    nuLR = (SaL[i] + SaR[i])/(fabs(SaL[i]) + fabs(SaR[i]));

    if ( fabs(SaR[i] - SaL[i]) > 1.e-9*fabs(SR[i]-SL[i])){
      sweep->dL[i] =   0.5*(chiL*nuL - chiL*nuLR)
                     + 0.5*(fabs(SaL[i]) - nuLR*SaL[i]);
  
      sweep->dR[i] =   0.5*(chiR*nuR - chiR*nuLR)
                     + 0.5*(fabs(SaR[i]) - nuLR*SaR[i]);
      sweep->aL[i] = 0.5*(1.0 + nuLR);
      sweep->aR[i] = 0.5*(1.0 - nuLR);
      
    }else{   /* HLLC, degenerate limit Bx -> 0 */
      sweep->dL[i] = 0.5*chiL*nuL + 0.5*fabs(SaL[i]);  
      sweep->dR[i] = 0.5*chiR*nuR + 0.5*fabs(SaR[i]);

      sweep->aL[i] = 0.5;
      sweep->aR[i] = 0.5;
    }

/* HLL diffusion coefficients */
    if (switch_to_hll){
      double alphaR = MAX(0, SR[i]);
      double alphaL = MIN(0, SL[i]);
      scrh = 1.0/(alphaR - alphaL);
      sweep->aL[i] =  alphaR*scrh;
      sweep->aR[i] = -alphaL*scrh;
      sweep->dR[i] = -alphaL*alphaR*scrh;
      sweep->dL[i] =  sweep->dR[i];
    }

/* LF diffusion coefficients */

    #if 0 
    if (0){
      lambda = MAX(fabs(SR[i]), fabs(SL[i]));
      sweep->aL[i] = 0.5;
      sweep->aR[i] = 0.5;
      sweep->dR[i] = 0.5*lambda;
      sweep->dL[i] = sweep->dR[i];
    }
    #endif 

    double aR = MAX(0.0, SR[i]);
    double aL = MIN(0.0, SL[i]);
    scrh = 1.0/(aR - aL);
    
    sweep->pnt_flux[i][BXt] = -(aR*vL[VXt] - aL*vR[VXt])*scrh;
    sweep->pnt_flux[i][BXb] = -(aR*vL[VXb] - aL*vR[VXb])*scrh;
   
    sweep->dff_flux[i][BXt] = 0.0;
    sweep->dff_flux[i][BXb] = 0.0;
  }
  #endif  /* CT_EMF_AVERAGE == UCT_HLLD */


  #if CT_EMF_AVERAGE == UCT_HLL
  for (i = beg; i <= end; i++){
    vL = stateL->v[i]; 
    vR = stateR->v[i];
    
    aR = MAX(0.0, SR[i]);
    aL = MIN(0.0, SL[i]);
    scrh = 1.0/(aR - aL);
    sweep->pnt_flux[i][BXt] = -(aR*vL[VXt] - aL*vR[VXt])*scrh;
    sweep->pnt_flux[i][BXb] = -(aR*vL[VXb] - aL*vR[VXb])*scrh;

    sweep->aL[i] =  aR*scrh;
    sweep->aR[i] = -aL*scrh;
    sweep->dR[i] = -aL*aR*scrh;
    sweep->dL[i] =  sweep->dR[i];
  }
  #endif

}
