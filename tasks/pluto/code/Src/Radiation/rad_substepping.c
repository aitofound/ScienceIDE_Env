/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Substepping integration routines
  
  Integrate the inhomogeneous radiation transport equations
  by carrying IMEX integrations with dt=dt_rad until t+dt_hydro
  is reached.

  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  
  \date    Jan 13, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if RADIATION_NR
/* ********************************************************************* */
void RadSubstepping (Data *d, double dt, timeStep *Dts, Grid *grid)
/*! 
 *
 *  \param [in,out] d   pointer to PLUTO Data structure containing 
 *                      the solution array updated from the most 
 *                      recent call
 *  \param[in]      dt  the time step used to integrate the source 
 *                      terms
 *  \param[in]     Dts  pointer to the time step structure
 *  \param[in]    grid  pointer to an array of grid structures
 *
 *********************************************************************** */
{
  static timeStep Dts_rad ;
  static double dt_rad ;
  double t, t_new, dt_hydro, dt_rad_next, dt_rad_save ;
  double xloc, xglob ;
  int idim ;
  
  if (Dts_rad.cmax == NULL){
    Dts_rad.cmax      = ARRAY_1D(NMAX_POINT, double);
    Dts_rad.invDt_hyp = 0.0;  
    dt_rad = RADIATION_INITIAL_DT ;
  }
  
  dt_hydro = dt ;
  t = 0. ;
  
  while(t < dt_hydro){
    
    /*-- Update t_new --*/
    t_new = t + dt_rad ;
    
    /*-- Save current time step --*/
    dt_rad_save = dt_rad ;
    
    /*-- Correct time step if needed --*/
    if ( t_new > dt_hydro ){
      dt_rad = dt_hydro - t ;
      t += dt_hydro ;
    } else {
      t = t_new ;
    }
    
    /*-- IMEX step --*/
    AdvanceRadStep (d, dt_rad, &Dts_rad, grid);
    
    /*-- Take the maximum of invDt_hyp across all processors --*/
    #ifdef PARALLEL
     xloc = Dts_rad.invDt_hyp;
     MPI_Allreduce (&xloc, &xglob, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
     Dts_rad.invDt_hyp = xglob; 
    #endif
    
    /*-- Update time step --*/
    dt_rad_next = Dts->cfl / Dts_rad.invDt_hyp ;
    
    // Constant time step if RADIATION_CFL_VAR_MAX == 1.0, update otherwise
    if (RADIATION_CFL_VAR_MAX != 1.0){
      dt_rad = MIN(dt_rad_next, RADIATION_CFL_VAR_MAX*dt_rad_save);
    } else {
      dt_rad = dt_rad_save ;
    }
    
    // Exit if dt_rad_next is too small
    if (dt_rad_next < RADIATION_INITIAL_DT*1.e-9){
      #ifndef CHOMBO // Quick fix for compilation issue
      char *str = IndentString();
      #endif
      print ("! RadSubstepping(): dt is too small (%12.6e).\n", dt_rad_next);
      #ifndef CHOMBO
      print ("! %s [dt(rad)       = cfl x %10.4e]\n",str,1.0/Dts_rad.invDt_hyp);
      #endif
      print ("! Cannot continue.\n");
      QUIT_PLUTO(1);
    }
    
    /*-- Reset hyperbolic time step --*/
    Dts_rad.invDt_hyp = 0.0;
    
  }
  
  return ;
 }
 #endif