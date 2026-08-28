/* ///////////////////////////////////////////////////////////////////// */
/*!
  \file
  \brief Radiation implicit step
  
  Main routines used for the implicit integration of the radiation-matter interaction
  terms. Six different implicit methods are implemented based on iterations of either
  the radiation or the internal energy densities. For stability reasons, the former
  are recommended when E_{rad} < prs, while the latter are recommended when
  E_{rad} > prs.
  
  In nonrelativistic HD and MHD, the following two implicit methods are the fastest:
  
  - RADIATION_NEWTON_NR_GAS: Solves the implicit step following Melon Fuksman et al.
		(2022, Appendix B) via a Newton method iterating the internal energy.
		
  - RADIATION__NEWTON_NR_RAD: Similar to RADIATION_NEWTON_NR_GAS, but iterating the 
		radiation energy instead.
  
  The remaining four methods can be used in both the relativistic and nonrelativistic
  modules:
  	
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
  
  \authors J. D. Melon Fuksman (fuksman@mpia.de)
  \date    Nov 02, 2022
  
	\b References
	 -  Melon Fuksman, J. D., and Mignone, A. 2019, ApJS, 242, 20.
     -  Takahashi, H. R., & Ohsuga, K. 2013, ApJ, 772, 127.
	 -  Melon Fuksman, J. D., Klahr, H., Flock, M., & Mignone, A. 2021, ApJ, 906, 78.
	 -  Melon Fuksman, J. D., & Klahr, H. 2022, ApJ, 936, 16.
	 
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

  const int comps = RADIATION_NEQS - 1 ;
  static Rad_data rad_data;

  static double *primvar, *consvar ;
  static double * x, * dx, * mf, ** J;
	
  /*-- Set flag and time step --*/
  rad_data.flag = flag ;
  rad_data.dt = dt ;

  /*-- Store primitive and conserved variables --*/
  rad_data.pv = uprim ;
  rad_data.cv = ucons ;

  #if RADIATION_IMPLICIT_NR
  	#if !RADIATION_NR
	print("Error: RADIATION_NEWTON_NR_GAS and RADIATION_NEWTON_NR_RAD \n");
	print("only available in the nonrelativistic modules\n");
	QUIT_PLUTO(1);
	#endif
	RadImplicitNR(&rad_data,source,ibeg,iend);
	return;
  #endif

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

  /* ----------------------------
       Main loop on positions
     ---------------------------- */
  for (i = ibeg; i <= iend; i++){
	primvar = uprim[i];
    consvar = ucons[i];
		
    /*-- Store current position --*/
    rad_data.pos = i ;
		
    /*-- Set initial energies conserving Egas + (c/c_r) Erad if
		RADIATION_NR and Egas + Erad otherwise --*/
	#if RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
	// Store (Egas,mgas) in Rini
	rad_data.Rini[0] = rad_data.Ttot[0] = consvar[ENG] ;
	#if RADIATION_NR	
	rad_data.Ttot[0] += (g_radC/g_reducedC)*consvar[ENR] ;
	#else 
	rad_data.Ttot[0] += consvar[ENR] ;
	#endif
		
	#else
	
	// Store (Erad,Frad) in Rini and Rprev
	#if RADIATION_NR	
	rad_data.Rini[0] = consvar[ENR] ;
	rad_data.Ttot[0] = (g_radC/g_reducedC)*consvar[ENR] ;
	#else
	rad_data.Rini[0] = rad_data.Ttot[0] = consvar[ENR] ;
	#endif
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

    /*-- Set fluxes and total momentum conserving mgas + (1/c_r) Frad
		  	 if RADIATION_NR and mgas + Frad otherwise --*/
	  for (j=0; j<comps; j++ ){
		#if RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
		// Store (Egas,mgas) in Rini
		rad_data.Rini[j+1] = rad_data.Ttot[j+1] = consvar[MX1+j] ;
		#if RADIATION_NR	
		rad_data.Ttot[j+1] += consvar[FR1+j]/g_reducedC ;
		#else
		rad_data.Ttot[j+1] += consvar[FR1+j] ;
		#endif
			
		#else
		
		// Store (Erad,Frad) in Rini and Rprev
		#if RADIATION_NR
		rad_data.Rini[j+1] = consvar[FR1+j] ;
		rad_data.Ttot[j+1] = consvar[FR1+j]/g_reducedC ;
		#else
		rad_data.Rini[j+1] = rad_data.Ttot[j+1] = consvar[FR1+j] ;	 
		#endif
		rad_data.Ttot[j+1] += consvar[MX1+j] ;
		
		#if RADIATION_IMPL == RADIATION_FIXEDPOINT_RAD
		rad_data.Rprev[j+1] = consvar[FR1+j] ;
		#endif
			
		#endif
	  }

    /*-- Initial guess for the iterated fields --*/
    #if RADIATION_IMPL == RADIATION_NEWTON_GAS \
		 || RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
		#if !RADIATION_NR
       	gamma = consvar[RHO]/primvar[RHO] ;
		#endif
      	x[0] = primvar[PRS] ;
		#if RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
		rad_data.Rprev[0] = x[0] ; // Store gas pressure in Rprev
		#endif
      	for (j = 0 ; j < comps ; j++ ) {
			#if RADIATION_NR
			x[j+1] = primvar[VX1+j] ;
			#else
			x[j+1] = gamma*primvar[VX1+j] ;
			#endif
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
        if ( GaussianSolve (J, dx, x, RADIATION_NEQS) ) QUIT_PLUTO(1);
				
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
          QUIT_PLUTO(1);
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
		#if RADIATION_NR
		consvar[ENG] = rad_data.Ttot[0] - (g_radC/g_reducedC)*x[0] ;
		#else
		consvar[ENG] = rad_data.Ttot[0] - x[0] ;
		#endif
        for (j=0; j<comps; j++){
          	consvar[FR1+j] = x[j+1] ;
			#if RADIATION_NR  
           	consvar[MX1+j] = rad_data.Ttot[j+1] - x[j+1]/g_reducedC ;
			#else
			consvar[MX1+j] = rad_data.Ttot[j+1] - x[j+1] ;
			#endif
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
		#if RADIATION_NR
		consvar[ENG] = rad_data.Ttot[0] - (g_radC/g_reducedC)*x[0] ;
		#else
		consvar[ENG] = rad_data.Ttot[0] - x[0] ;
		#endif
		for (j=0; j<comps; j++){
			consvar[FR1+j] = x[j+1] ;	
			#if RADIATION_NR
			consvar[MX1+j] = rad_data.Ttot[j+1] - x[j+1]/g_reducedC ;
			#else
			consvar[MX1+j] = rad_data.Ttot[j+1] - x[j+1] ;
			#endif
      }
      ConsToPrim(ucons,uprim,i,i,flag);
    #endif
		
    /*-- Check number of iterations --*/
	  if ( m > RADIATION_MAXITER ) {
      WARNING(
        print("! Radstep: RADIATION_MAXITER reached, err = %e,", err);
        Where (i, NULL);
      )		
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

#if RADIATION_NR
void RadImplicitNR(Rad_data * rad_data, double ** source, int ibeg, int iend){
	
	int i, j, m, pos ;
	double abs_op, scat_op, tot_op, s, dt, err, dx2, cratio ;
	double mtot[3], frad[3], vel[3], vel0[3], vel2, ekin, etot, etek, erad,
		   rho0, A, B, C, gmm1, x, x0, x2, xaux, xaux2, y, y0, y2, f, fp ;
	double **uprim, **ucons, *u, *v ;
	
	#if EOS == IDEAL
	gmm1 = g_gamma - 1.0;
	#endif
	cratio = g_reducedC/g_radC;
	dt = rad_data->dt;

	uprim = rad_data-> pv;
	ucons = rad_data-> cv; 

	#if RADIATION_VAR_OPACITIES == NO
	abs_op = g_absorptionCoeff;
	scat_op = g_scatteringCoeff;
	tot_op = g_totalOpacity ;
	#endif

	// Loop on positions
	for (i = ibeg; i <= iend; i++){
		u = ucons[i];
		v = uprim[i];

		// Get modified total energy and momentum
		ekin = 0.5*(u[MX1]*u[MX1]+u[MX2]*u[MX2]+u[MX3]*u[MX3])/u[RHO];
		#if PHYSICS == MHD
            ekin += 0.5*(u[BX1]*u[BX1] + u[BX2]*u[BX2] + u[BX3]*u[BX3]);
        #endif
		etot = u[ENG] + u[ENR]/cratio ;
		#if IRRADIATION && !RADIATION_IMEX_SSP2
		etot -= dt*u[FIR] ;
		#endif
		etek = etot - ekin ;
		erad = u[ENR]; 
		for (j=0; j<3; j++){
			mtot[j] = u[MX1+j] + u[FR1+j]/g_reducedC ;
			vel0[j]  = v[VX1+j] ;
		}
		
		// Rescale coefficients by the density
		rho0  = u[RHO] ;

		#if RADIATION_IMPL == RADIATION_NEWTON_NR_GAS
		// x = (c/cr)Er-etot+ekin(n+1) with Er = Er(n) as first iteration
		x = -(u[ENG]-ekin)/rho0;
		#if RADIATION_FULL_CONVERGENCE
		y = erad/rho0;
		#endif
		#else
		// x = Er/rho with Er = Er(n) as first iteration
		x = erad/rho0;
		#if RADIATION_FULL_CONVERGENCE
		y = -(u[ENG]-ekin)/rho0;
		#endif
		#endif
		
		// Solve A*x^4 + B*x + C == 0 with Newton's method
		m = 0; err = 1.;
		while(err > RADIATION_ERROR && m++ < RADIATION_MAXITER ){
			// Get opacities
			#if RADIATION_VAR_OPACITIES
			UserDefOpacities (v, &abs_op, &scat_op);
			tot_op = abs_op + scat_op ;
			#endif

			// Get polynomial coeffs. independent on ekin 
			// (do only once if constant opacities)
			#if RADIATION_VAR_OPACITIES == NO
			if(m==1){
			#endif
				s = dt*rho0*g_reducedC*abs_op ;	
				A = s*g_radiationConst*pow(g_idealGasConst*gmm1,4);
				s += 1. ;
				B = -s*cratio*rho0 ;
			#if RADIATION_VAR_OPACITIES == NO
			}
			#endif

			// Update momentum and kinetic energy
			#if RADIATION_VAR_OPACITIES
			for (j=0; j<3; j++){
				frad[j]  = u[FR1+j]/s ;
				u[MX1+j] = mtot[j] - frad[j]/g_reducedC ;
				vel[j]   = u[MX1+j]/u[RHO] ;
			}
			ekin = 0.5*(u[MX1]*u[MX1]+u[MX2]*u[MX2]+u[MX3]*u[MX3])/u[RHO] ;
			vel2 = ekin/u[RHO] ;
			#if PHYSICS == MHD
            	ekin += 0.5*(u[BX1]*u[BX1] + u[BX2]*u[BX2] + u[BX3]*u[BX3]);
        	#endif
			etek = etot - ekin ;
			#endif

			// Compute remaining polynomial coefficient
			C = erad + B/rho0*etek ;	

			// Update x guess with Newton's method
			x0 = x ;
			x2 = x*x ;
			#if RADIATION_IMPL == RADIATION_NEWTON_NR_GAS
			f = A*x2*x2 + B*x + C ;
			fp = 4.*A*x2*x + B ;
			#else
			xaux = x/cratio - etek/rho0 ;
			xaux2 = xaux*xaux ;
			f = erad - s*rho0*x + A*xaux2*xaux2  ;
			fp = - s*rho0 + 4.*A*xaux2*xaux/cratio  ;
			#endif
			x = x - f/fp ;
			
			// Compute pressure error
			dx2 = x-x0 ;
			dx2 *= dx2 ;
			err = (x2 > 1e-40) ? dx2/x2 : dx2/1e-40 ;

			#if RADIATION_FULL_CONVERGENCE
			y0 = y ;
			#if RADIATION_IMPL == RADIATION_NEWTON_NR_GAS
			// Er/rho
			y = (etek/rho0 + x)*cratio ; 
			#else
			// Specific internal energy
			y = etot/rho0 - x/cratio ;
			#endif
			y2 = y*y ;
			dx2 = y-y0 ;
			dx2 *= dx2 ;
			err += (y2 > 1e-40) ? dx2/y2 : dx2/1e-40 ;
			#endif

			// Add velocity error (except for constant opacities)
			#if RADIATION_VAR_OPACITIES
			for (j=0; j<3; j++){
				dx2 = vel[j]-vel0[j] ;
				dx2 *= dx2 ;
				err += (vel2 > 1e-40) ? dx2/vel2 : dx2/1e-40 ;
				vel0[j] = vel[j] ;
			}
			#endif
			
			// Update pressure for opacities calculation
			#if RADIATION_VAR_OPACITIES			
			
			#if RADIATION_IMPL == RADIATION_NEWTON_NR_GAS
			v[PRS] = -gmm1*x*rho0;
			#else
			v[PRS] = gmm1*(etek - x*rho0/cratio);
			#endif
			
			if (v[PRS] < 0.0){
				WARNING(
				printLog ("! RadImplicitNR(): p(E) < 0 (%8.2e), ", v[PRS]);
				Where (i, NULL);
				)
				v[PRS]   = g_smallPressure;
			}
			#endif
		}

		// Check number of iterations
		if ( m > RADIATION_MAXITER ) {
		WARNING(
			print("! Radstep: RADIATION_MAXITER reached, err = %e,", err);
			Where (i, NULL);
		)		
		}
		
		// Update radiation and gas energy
		#if RADIATION_IMPL == RADIATION_NEWTON_NR_GAS
		u[ENG] = ekin - x*rho0 ;
		u[ENR] = (etot - u[ENG])*cratio ; 
		#else
		u[ENR] = x*rho0 ;
		u[ENG] = (etot - u[ENR]/cratio) ; 
		#endif

		// Update radiation flux and momentum
		s = 1. + dt*u[RHO]*g_reducedC*tot_op ;
		for (j=0; j<3; j++){
			mtot[j] = u[MX1+j] + u[FR1+j]/g_reducedC ;
			u[FR1+j] /= s ;
			u[MX1+j] = mtot[j] - u[FR1+j]/g_reducedC ; 
		}

	} // End loop on positions
	
	// Update primitive fields
	ConsToPrim(ucons,uprim,ibeg,iend,rad_data->flag);
	
	// Compute and store source terms if needed
	#if RADIATION_IMEX_SSP2 == YES
	for (i = ibeg; i <= iend; i++) RadSourceFunction(uprim[i],source[i]);	
  	#endif
		
}
#endif

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
 * \param [out]    S      pointer to 3D array of source terms,
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
