/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Radiation implicit step
  
  Main routines used for the implicit integration of the radiation-matter interaction
  terms. Four different implicit methods are here implemented:
  	
  - RADIATION_FIXEDPOINT_RAD: Solves the implicit step following Takahashi & Oshuga
		(2013), by iterating the radiation fields.
		
  - RADIATION_FIXEDPOINT_GAS: fixed-point algorithm based on iterations of the MHD
		fields.

  - RADIATION_NEWTON_GAS: Solves the implicit step by means of the Newton's method,
    performing iterations of the matter fields (p_g,u^i), i.e., the matter
    pressure and spatial components of its 4-velocity. The components of the Jacobian
    are computed numerically by carrying small variations of these fields.

  - RADIATION_NEWTON_RAD: Same as RADIATION_NEWTON_RAD, but performing iterations of
		the radiation fields.

	In all methods the convergence criterion considers both radiation and MHD fields
	unless RADIATION_FULL_CONVERGENCE == NO, in which case only the variation of the
	iterated fields is controlled. 
  
  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Dec 03, 2020
  
	\b References
	 -  Melon Fuksman, J. D., and Mignone, A. 2019, "A Radiative
			Transfer Module for Relativistic Magnetohydrodynamics in the
			PLUTO Code" ApJS, 242, 20.
	 -  Takahashi, H. R., & Ohsuga, K. 2013, "A Numerical Treatment of
			Anisotropic Radiation Fields Coupled With Relativistic Resistive
			Magnetofluids", ApJ, 772, 127.
*/
/* ///////////////////////////////////////////////////////////////////// */

#include "pluto.h"
#if RADIATION
/* ********************************************************************* */
void RadStep (double **uprim, double **ucons, double **source,
              int ibeg, int iend, uint16_t *flag, double dt)
/*!
 * Perform the implicit step along a direction determined during RadStep3D.
 * Update both primitive and conserved quantities. Primitive and conserved
 * fields must be consistent before every call to this function.
 *
 * \param [in,out]	uprim	  array of primitive variables
 * \param [in,out]	ucons	  array of conservative variables
 * \param [in,out]	source	array of source terms
 * \param [in]  	  ibeg	  starting index of computation
 * \param [in] 		  iend	 	final index of computation
 * \param [in,out] 	flag    array of flags
 * \param [in]      dt      time step
 * 
 *********************************************************************** */
{
  int i, j, m ;
  double err, gamma ;

  static int comps = RADIATION_NEQS - 1 ;
  static Rad_data rad_data;

  static double *primvar, *consvar ;
  static double * x, * dx, * mf, ** J;

  if (rad_data.Ttot == NULL){
    rad_data.Ttot = ARRAY_1D(RADIATION_NEQS, double);
    rad_data.Rini = ARRAY_1D(RADIATION_NEQS, double);
    rad_data.Rprev = ARRAY_1D(RADIATION_NEQS, double);
    #if RADIATION_IMPL == RADIATION_NEWTON_GAS \
		 || RADIATION_IMPL == RADIATION_NEWTON_RAD \
		 || RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
    rad_data.u = ARRAY_1D(3, double);
    #endif

    x  = ARRAY_1D(RADIATION_NEQS, double);
    dx = ARRAY_1D(RADIATION_NEQS, double);
    mf = ARRAY_1D(RADIATION_NEQS, double);
    J  = ARRAY_2D(RADIATION_NEQS, RADIATION_NEQS, double);
  }

  /*-- Set flag and time step --*/
  rad_data.flag = flag ;
  rad_data.dt = dt ;

  /*-- Store primitive and conserved variables --*/
  rad_data.pv = uprim ;
  rad_data.cv = ucons ;

  /* ----------------------------
       Main loop on positions
     ---------------------------- */
  for (i = ibeg; i <= iend; i++){
	  primvar = uprim[i];
    consvar = ucons[i];

    /*-- Store current position --*/
    rad_data.pos = i ;

    /*-- Set initial energies --*/
		#if RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
			// Store (Egas,mgas) in Rini
		  rad_data.Rini[0] = rad_data.Ttot[0] = consvar[ENG] ;
      rad_data.Ttot[0] += consvar[ENR] ;
		#else
			// Store (Erad,Frad) in Rini and Rprev
			rad_data.Rini[0] = rad_data.Ttot[0] = consvar[ENR] ;
			rad_data.Ttot[0] += consvar[ENG] ;
			#if RADIATION_IMPL == RADIATION_FIXEDPOINT_RAD
			rad_data.Rprev[0] = consvar[ENR] ;
			#endif
		#endif

    /*-- Set initial extra variables if full convergence is imposed --*/
    #if (RADIATION_IMPL == RADIATION_NEWTON_RAD || RADIATION_IMPL == RADIATION_FIXEDPOINT_RAD) \
     && RADIATION_FULL_CONVERGENCE == YES
    rad_data.exv_prev = primvar[PRS] ;
    #elif (RADIATION_IMPL == RADIATION_NEWTON_GAS || RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS ) \
			 && RADIATION_FULL_CONVERGENCE == YES
    rad_data.exv_prev = primvar[ENR] ;
    #endif

    /*-- Set fluxes and total momentum --*/
	  for (j=0; j<comps; j++ ){
			#if RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
				// Store (Egas,mgas) in Rini
				rad_data.Rini[j+1] = rad_data.Ttot[j+1] = consvar[MX1+j] ;
	      rad_data.Ttot[j+1] += consvar[FR1+j] ;
			#else
				// Store (Erad,Frad) in Rini and Rprev
				rad_data.Rini[j+1] = rad_data.Ttot[j+1] = consvar[FR1+j] ;
				rad_data.Ttot[j+1] += consvar[MX1+j] ;
				#if RADIATION_IMPL == RADIATION_FIXEDPOINT_RAD
				rad_data.Rprev[j+1] = consvar[FR1+j] ;
				#endif
			#endif
	  }

    /*-- Initial guess for the iterated fields --*/
    #if RADIATION_IMPL == RADIATION_NEWTON_GAS \
		 || RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
      gamma = consvar[RHO]/primvar[RHO] ;
      x[0] = primvar[PRS] ;
			#if RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
				rad_data.Rprev[0] = x[0] ; // Store gas pressure in Rprev
			#endif
      for (j = 0 ; j < comps ; j++ ) {
				x[j+1] = gamma*primvar[VX1+j] ;
				#if RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
					rad_data.Rprev[j+1] = x[j+1] ; // Store proper velocity in Rprev
				#endif
			}
    #elif RADIATION_IMPL == RADIATION_NEWTON_RAD
      x[0] = consvar[ENR] ;
      for (j = 0 ; j < comps ; j++ ) x[j+1] = consvar[FR1+j] ;
    #endif
		
  /* -----------------------------------------------------------------
      Implicit step until convergence or maximum number of iterations
     ----------------------------------------------------------------- */
	  m = 0 ; err = 1.0 ;
    while ( err > RADIATION_ERROR && m++ < RADIATION_MAXITER ){

			/*********************************
				    Update iterated fields
			 *********************************/
      #if RADIATION_IMPL == RADIATION_FIXEDPOINT_RAD

        /*-- Set coefficients of the system C.(E,F^i)^{(m+1)} == b  --*/ 	
        RadFPMatrices (&rad_data, dx, J);
				
        /*-- Update iterated fields and store them in x --*/
        if ( GaussianSolve (J, dx, x, RADIATION_NEQS) ) { 
				
				QUIT_PLUTO(1);
				
				}
				
			#elif RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
			
			  /*-- Compute source terms and update iterated fields --*/
        RadNewtonMinusF(&rad_data, x, mf);
				
      #else

        /*-- Compute -F and the Jacobian --*/
        RadNewtonJacobian (x, mf, J, &rad_data) ; 

        /*-- Solve the system J*dx == -F --*/
        if ( GaussianSolve (J, mf, dx, RADIATION_NEQS) ){
          WARNING(Where (i, NULL);)
          for (j=0; j < RADIATION_NEQS ; j++ ) dx[j] = mf[j];
        //  QUIT_PLUTO(1);
        }

        /*-- Update iterated fields --*/
        for (j=0; j < RADIATION_NEQS ; j++ ) x[j] += dx[j];

      #endif
			
			/*********************************
				       Error calculation
			 *********************************/
      #if RADIATION_IMPL == RADIATION_FIXEDPOINT_RAD
			
        /*-- Update conserved variables --*/
        consvar[ENR] = x[0] ;
        consvar[ENG] = rad_data.Ttot[0] - x[0] ;
        for (j=0; j<comps; j++){
          consvar[FR1+j] = x[j+1] ;	
          consvar[MX1+j] = rad_data.Ttot[j+1] - x[j+1] ;
        }
				
				/*-- Update primitive variables --*/
        ConsToPrim (ucons, uprim, i, i, flag);

        /*-- Compute relative differences --*/
        err = RadErr(primvar, NULL, &rad_data) ;
				
			#elif RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS

			  /*-- Compute relative differences --*/
        err = RadErr(x, NULL, &rad_data) ;
			
      #else

        /*-- Compute relative differences --*/
        err = RadErr(x, dx, &rad_data) ;

      #endif

		}//End of iterations   

    /*-- Final update if needed --*/
    #if RADIATION_IMPL == RADIATION_NEWTON_GAS
      if ( RadIterToPrim (x, &rad_data) ) QUIT_PLUTO(1);
    #elif RADIATION_IMPL == RADIATION_NEWTON_RAD
      consvar[ENR] = x[0] ;
      consvar[ENG] = rad_data.Ttot[0] - x[0] ;
      for (j=0; j<comps; j++){
        consvar[FR1+j] = x[j+1] ;	
        consvar[MX1+j] = rad_data.Ttot[j+1] - x[j+1] ;
      }
      ConsToPrim(ucons,uprim,i,i,flag);
    #endif

    /*-- Check number of iterations --*/
	  if ( m > RADIATION_MAXITER ) {
      WARNING(
        print("! Radstep error : RADIATION_MAXITER reached, err = %e,", err);
        Where (i, NULL);
      )		
	    // QUIT_PLUTO(1);
	  }

    /*-- Compute and store source terms if needed --*/
    #if RADIATION_IMEX_SSP2 == YES
    RadSourceFunction(primvar,source[i]);					
    #endif

  }//End of loop on positions
	
  /*-- Update conserved fields if needed --*/
  #if RADIATION_IMPL == RADIATION_NEWTON_GAS
  PrimToCons(uprim,ucons,ibeg,iend);
  #endif
}

/* ********************************************************************* */
void RadStep3D (Data_Arr U, Data_Arr V, Data_Arr S,
                uint16_t ***flag, RBox *box, double dt)
/*!
 *  Perform the implicit step for the matter-radiation interaction.
 *  Update both conserved and primitive variables. 
 *
 * \param [in]     U      pointer to 3D array of conserved variables,
 *                        with array indexing <tt>[k][j][i][nv]</tt>
 * \param [out]    V      pointer to 3D array of primitive variables,
 *                        with array indexing <tt>[nv][k][j][i]</tt>
 * \param [out]    V      pointer to 3D array of source terms,
 *                        with array indexing <tt>[nv][k][j][i]</tt>
 * \param [in,out] flag   pointer to 3D array of flags.
 * \param [in]     box    pointer to RBox structure containing the domain
 *                        portion over which conversion must be performed.
 * \param [in]     dt     current time step.
 *
 *********************************************************************** */
{
  int   i, j, k, nv ;
  int   ibeg, iend, jbeg, jend, kbeg, kend ;
  int   current_dir ;
  static double **v, **u ;

/* ----------------------------------------------
    Allocate u and v (conserved and primitive
	  variables) and set global constants.
   ---------------------------------------------- */
  if (v == NULL){
    v = ARRAY_2D(NMAX_POINT, NVAR, double);
    u = ARRAY_2D(NMAX_POINT, NVAR, double);
  }

/* ----------------------------------------------
    Save current sweep direction and by default,
    perform the step along X1 stripes
   ---------------------------------------------- */

  current_dir = g_dir; 
  g_dir = IDIR;
  
/* -----------------------------------------------
    Set (beg,end) indices in ascending order for
    proper call to RadStep()
   ----------------------------------------------- */

  ibeg = (box->ibeg <= box->iend) ? (iend=box->iend, box->ibeg):(iend=box->ibeg, box->iend);
  jbeg = (box->jbeg <= box->jend) ? (jend=box->jend, box->jbeg):(jend=box->jbeg, box->jend);
  kbeg = (box->kbeg <= box->kend) ? (kend=box->kend, box->kbeg):(kend=box->kbeg, box->kend);

  for (k = kbeg; k <= kend; k++){ g_k = k;
  for (j = jbeg; j <= jend; j++){ g_j = j;

    for (i = ibeg; i <= iend; i++) NVAR_LOOP(nv) v[i][nv] = V[nv][k][j][i];

    #if RADIATION_IMEX_SSP2 == YES
    RadStep (v, U[k][j], S[k][j], ibeg, iend, flag[k][j], dt);
    #else
    RadStep (v, U[k][j], NULL, ibeg, iend, flag[k][j], dt);
    #endif 

    for (i = ibeg; i <= iend; i++) { 
  	  for (nv = NFLX; nv--; ) V[nv][k][j][i] = v[i][nv];   
	  }

  }}

  g_dir = current_dir;

}
#endif
