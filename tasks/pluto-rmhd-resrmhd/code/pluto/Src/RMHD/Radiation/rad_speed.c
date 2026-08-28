/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file 
  \brief   Compute the outermost wave speeds of the radiation part.

  Rad_Speed() computes an estimate to the leftmost and rightmost
  wave signal speeds of the radiation part included in the radiation
  module, using the information of the input states ::vR and ::vL.

*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#if RADIATION
/* ********************************************************************* */
void Rad_Speed (double **vL, double **vR, Grid *grid, uint16_t *flag,
                double *SrL, double *SrR, int beg, int end)
/*!
 * Compute leftmost (SL) and rightmost (SR) speed for the Riemann fan.
 * 
 * \param [in]     vL    left  state for the Riemann solver
 * \param [in]     vR    right state for the Riemann solver
 * \param [in]     grid  pointer to Grid structure
 * \param [in,out] flag  array of flags
 * \param [out]    SrL   leftmost radiation wave speed
 * \param [out]    SrR   rightmost radiation wave speed
 * \param [in]     beg   starting index of computation
 * \param [in]     end   final index of computation
 *
 *********************************************************************** */
{
  int    i, err1, err2, N, M;
  double vLim, ll, lr;
  static double *sl_min, *sl_max;
  static double *sr_min, *sr_max;

  if (sl_min == NULL){
    sl_min = ARRAY_1D(NMAX_POINT, double);
    sl_max = ARRAY_1D(NMAX_POINT, double);
    sr_min = ARRAY_1D(NMAX_POINT, double);
    sr_max = ARRAY_1D(NMAX_POINT, double);
  }

/* ----------------------------------------------
    Compute maximum and minimum signal velocities
   ---------------------------------------------- */
  MaxRadSpeed (vL, sl_min, sl_max, beg, end);
  MaxRadSpeed (vR, sr_min, sr_max, beg, end);

  for (i = beg; i <= end; i++) {
    #if RADIATION_DIFF_LIMITING
    /*-- Additional limiting on the radiation wave velocities for optically
         thick flows (similar to Sadowski et al., MNRAS 429, 3533–3550 (2013)) --*/
    vLim = LimitRadWaveVelocity(vL,vR,grid,i);
    ll = MIN(sl_min[i], sr_min[i]) ;
    lr = MAX(sl_max[i], sr_max[i]) ;
    SrL[i] = MAX ( ll, -vLim );
    SrR[i] = MIN ( lr , vLim );
    // Used to replace HLLC by HLL whenever speed limiting is applied
    if( ll < -vLim || lr > vLim ) flag[i] |= FLAG_HLL;   
	#else
    SrL[i] = MIN(sl_min[i], sr_min[i]);
    SrR[i] = MAX(sl_max[i], sr_max[i]);
    #endif
  }

}

double LimitRadWaveVelocity (double **vL, double **vR, Grid *grid, int pos)
/*!
 * Takes an input the left and right states, the grid data, and the current
 * position. Returns 4/(3*tau), where tau is the optical depth along the cell.
 *
 *********************************************************************** */
{
  double *stateL = vL[pos], *stateR = vR[pos] ;
  double vel2 = 0.5*( stateL[VXn]*stateL[VXn] + stateR[VXn]*stateR[VXn] ) ;
  double Lgamma_1 = sqrt(1 - vel2) ;
  double dx, q, k ;
  double abs_op, scat_op, tot_op ;

  /*-- Define dx --*/
  #if ( GEOMETRY == CARTESIAN ) || ( GEOMETRY == CYLINDRICAL )
    dx = grid->dx[g_dir][pos];
  #elif GEOMETRY == POLAR
    if ( g_dir == IDIR || g_dir == KDIR ) {
      dx = grid->dx[g_dir][pos] ;
    } else if ( g_dir == JDIR ) {
      dx = grid->x[IDIR][g_i] * grid->dx[JDIR][pos] ;
    }
  #elif GEOMETRY == SPHERICAL
    if ( g_dir == IDIR ) {
      dx = grid->dx[IDIR][pos] ;
    } else if ( g_dir == JDIR ) {
      dx = grid->x[IDIR][g_i] * grid->dx[JDIR][pos] ;
    } else if ( g_dir == KDIR ) {
      dx = grid->x[IDIR][g_i] * sin( grid->x[JDIR][g_j] ) * grid->dx[KDIR][pos] ;
    } 
  #endif

  /*-- Compute optical depth of the cell --*/
  #if RADIATION_VAR_OPACITIES == YES
    UserDefOpacities (stateL, &abs_op, &scat_op); 
    tot_op = 0.5*(abs_op + scat_op);
    UserDefOpacities (stateR, &abs_op, &scat_op);
    tot_op += 0.5*(abs_op + scat_op);
  #else
    abs_op = g_absorptionCoeff ;
    tot_op = g_totalOpacity ;
  #endif
  q = dx * tot_op * (stateL[RHO] + stateR[RHO]) ;

  if ( q < 1e-100 ) return 2.0 ; /*- Small q control. The signal speed
                                     is not limited for small opacities. -*/
  k = 2.666666667*Lgamma_1 ;

  return k/q ;
}


void MaxRadSpeed (double **v, double *cmin, double *cmax, int beg, int end)
/*!
 * Calculate the maximum and minimum wave speeds of the radiation part,
 * and store it in *cmin and *cmax.
 * Maximum and minimum eigenvalues are calculated explicitly as in, e.g., 
 * M. A. Skinner & E. C. Ostriker, ApJS 206, 91 (2013). 
 * The eigenvalues depend only on the values of f=|F_{rad}|/E and 
 * cos(\theta)=Fn/|F_{rad}|. 
 *
 *********************************************************************** */	
{
  int i, index_f, index_c ;
  double fx, f, theta, costheta;
  double * q;
  double sq1, sq1m1, sq2, qq, f2, fc;

  for (i = beg; i <= end; i++) {
		q = v[i];

		fx = q[FRn] ;
    f = sqrt( fx*fx + q[FRt]*q[FRt] + q[FRb]*q[FRb] ) ;

		if (f < 1e-150 ) {

			cmax[i] = 1.0/sqrt(3) ;
			cmin[i] = -cmax[i] ;

		} else {

      costheta = fx/f ;

      f = f/q[ENR] ;	
      if (f > 1.0) f = 1.0 ; 
      f2 = f*f ;      

      qq = fabs(4.0-3.0*f2) ;
      sq1 = sqrt(qq) ;
      sq1m1 = 1.0/sq1 ;
     
      sq2 = 0.666666667*(qq-sq1) + 2.0*costheta*costheta*(2.0-f2-sq1)  ;
      sq2 = sqrt(fabs(sq2)) ;

      fc = f*costheta ;

      cmax[i] = sq1m1*(fc+sq2) ; 
      cmin[i] = sq1m1*(fc-sq2) ; 

		}//End of if (f!=0)

  }//End loop on positions

	return ;
}

#endif
