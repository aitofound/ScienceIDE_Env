/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Add source terms before the FARGO advection step.
  
  This function is called prior to the FARGO advection algorithm to add
  source terms.
  At present, we use it only for the energy equation in the
  ShearingBox module:
  \f[ 
     \pd{E'}{t} + w\pd{E'}{y} = (B_xB_y - \rho v_xv'_y)\pd{w}{x}
  \f]
  where \f$w = -q\Omega x\f$.
  The discretization follows the algorithm of [GS10], see
  Eq. (51) and (63) of that paper.

  \b Reference
     - [GS10] "Implementation of the shearing box approximation in Athena",
       Stone & Gardiner, ApJS (2010) 189, 142.
 
  \author A. Mignone (andrea.mignone@unito.it)
  \date   Apr 30, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void FARGO_Source(Data_Arr UU, Data_Arr Vs, double dt, Grid *grid)
/*!
 *
 * \param [in,out] UU     array of conserved variables
 * \param [in]     Vs     array of staggered magnetic fields
 * \param [in]     dt     the current time increment
 * \param [in]     grid   pointer to an array of Grid structures 
 *********************************************************************** */
{
#ifdef SHEARINGBOX
  #if HAVE_ENERGY || !INCLUDE_JDIR
  int    i,j,k;
  double scrh, rho, mx, my;
  double *x  = grid->x[IDIR];
  double *xr = grid->xr[IDIR];
  double *dx = grid->dx[IDIR];
  double *dz = grid->dx[KDIR];  
  #if PHYSICS == MHD
  double Bx, By;
  double ***Bxs = Vs[BX1s];
  double ***Bzs = Vs[BX3s];
  double w, wp, wm;
  #endif
  
  scrh = SB_Q*SB_OMEGA;
  DOM_LOOP(k,j,i){
    rho = UU[k][j][i][RHO];
    mx  = UU[k][j][i][MX1];
    my  = UU[k][j][i][MX2];
    #if PHYSICS == MHD
    Bx  = UU[k][j][i][BX1];
    By  = UU[k][j][i][BX2];
    #endif

  /* ------------------------------------------------------
     In the axisymmetric case, By is not a staggered
     component and not treated during FARGO_ShiftSolution().
     Contributions from d(w*Bx)/dx and d(w*Bz)/dz are
     added here.
     ------------------------------------------------------ */
  
    #if (PHYSICS == MHD) && !INCLUDE_JDIR   /* Axisymmetric ShearingBox */
    wp = -scrh*xr[i];
    wm = -scrh*xr[i-1];
    w  = -scrh*x[i];
    
    UU[k][j][i][BX2] += dt*( (wp*Bxs[k][j][i] - wm*Bxs[k][j][i-1])/dx[i]
                            + w*(Bzs[k][j][i] - Bzs[k-1][j][i])/dz[k] );
    #endif

    #if HAVE_ENERGY && (PHYSICS == MHD)
    UU[k][j][i][ENG] += - dt*scrh*Bx*(By - 0.5*dt*Bx*scrh)
                        + dt*scrh*mx*my/rho;
    #endif                        
    #if HAVE_ENERGY && (PHYSICS == HD)
    UU[k][j][i][ENG] += dt*scrh*mx*my/rho;
    #endif                        
    
  }
  #endif
#endif
}
