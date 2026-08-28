/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Print useful information about the current computations.

  \author  A. Mignone (andrea.mignone@unito.it)
  \date    May 17, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"
static void CheckConfig();

/* ********************************************************************* */
void ShowConfig (int argc, char *argv[], char *ini_file)
/*!
 *  Write a summary of the selected options
 *
      ___  __   __  ____________ 
     / _ \/ /  / / / /_  __/ __ \
    / ___/ /__/ /_/ / / / / /_/ /
   / /  /____/\____/ /_/  \____/ 
   ============================== 
                                  
 *
 * 
 *                        
 *
 *********************************************************************** */
{
  int  n;
  FILE *fp;
  time_t time_now;
  char  str1[128], str2[128], str3[128], sline[512];
  double *dbl_pnt;  /* For printLoging size of pointer to double */
  int    *int_pnt;  /* For printLoging size of pointer to int */

  CheckConfig();

  print ("\n");
  print ("   ___  __   __  ____________   \n");
  print ("  / _ \\/ /  / / / /_  __/ __ \\ \n");
  print (" / ___/ /__/ /_/ / / / / /_/ /  \n");
  print ("/_/  /____/\\____/ /_/  \\____/   \n");
  print ("=============================    v. %s  \n", PLUTO_VERSION);
  
  print ("\n> System:\n\n");

  if ( (fp = fopen("sysconf.out","r")) != NULL){

    while (fscanf (fp, "%s %s %s\n", str1, str2, str3) != EOF) {
      if (!strcmp(str1,"USER")) 
        print ("  %s:             %s\n",str1, str3);
      else if (!strcmp(str1,"WORKING_g_dir"))
        print ("  %s:      %s\n",str1, str3);
      else if (!strcmp(str1,"SYSTEM_NAME"))
        print ("  %s:      %s\n",str1, str3);
      else if (!strcmp(str1,"NODE_NAME"))
        print ("  %s:        %s\n",str1, str3);
      else if (!strcmp(str1,"ARCH"))
        print ("  %s:             %s\n",str1, str3);
      else if (!strcmp(str1,"BYTE_ORDER"))
        print ("  %s:       %s\n\n",str1, str3);
    }
    fclose(fp);

  }else{
    print ("  sysconf.out file not found... \n\n");
  }
 
  print ("  - Basic data type size (Bytes):\n");
  print ("    o sizeof (char)     = %d\n", sizeof(char));
  print ("    o sizeof (uchar)    = %d\n", sizeof(unsigned char));
  print ("    o sizeof (uint16_t) = %d\n", sizeof(uint16_t));
  print ("    o sizeof (short)    = %d\n", sizeof(short));
  print ("    o sizeof (ushort)   = %d\n", sizeof(unsigned short));
  print ("    o sizeof (int)      = %d\n", sizeof(int));
  print ("    o sizeof (long)     = %d\n", sizeof(long));
  print ("    o sizeof (*int)     = %d\n", sizeof(int_pnt));
  print ("    o sizeof (float)    = %d\n", sizeof(float));
  print ("    o sizeof (double)   = %d\n", sizeof(double));
  print ("    o sizeof (*double)  = %d\n", sizeof(dbl_pnt));
  

  print ("\n  - Structure data type (Bytes):\n");
  print ("    o sizeof (cmdLine)    = %d\n", sizeof(cmdLine));
  print ("    o sizeof (Data)       = %d\n", sizeof(Data));
  print ("    o sizeof (Grid)       = %d\n", sizeof(Grid));
  print ("    o sizeof (Float_Vect) = %d\n", sizeof(Float_Vect));
  print ("    o sizeof (Image)      = %d\n", sizeof(Image));
  print ("    o sizeof (Output)     = %d\n", sizeof(Output));
  print ("    o sizeof (RGB)        = %d\n", sizeof(RGB));
  print ("    o sizeof (Runtime)    = %d\n", sizeof(Runtime));
  print ("    o sizeof (Restart)    = %d\n", sizeof(Restart));
  print ("    o sizeof (timeStep)   = %d\n", sizeof(timeStep));
  print ("    o sizeof (RBox)       = %d\n", sizeof(RBox));
  print ("    o sizeof (State)      = %d\n", sizeof(State));
  print ("    o sizeof (Sweep)      = %d\n", sizeof(Sweep));
#if (PARTICLES != NO)
  print ("    o sizeof (PARTICLE)   = %d\n", sizeof(Particle));
#endif

  time(&time_now);
  print("\n> Local time:       %s\n",asctime(localtime(&time_now)));
      
/* -- printLog command line arguments -- */

  print ("> Cmd line args:    ");
  for (n = 1; n < argc; n++) print ("%s ",argv[n]);
  print ("\n\n");

/* -- printLog problem configuration -- */

  print ("> Header configuration:\n\n");

  if (PHYSICS == HD)        print ("  PHYSICS:          HD\n");
  if (PHYSICS == RHD)       print ("  PHYSICS:          RHD\n");
  if (PHYSICS == MHD)       print ("  PHYSICS:          MHD    [div.B: ");
  if (PHYSICS == RMHD)      print ("  PHYSICS:          RMHD   [div.B: ");
  if (PHYSICS == ResRMHD)   print ("  PHYSICS:          ResMHD [div.B: ");
#if (PHYSICS == MHD) || (PHYSICS == RMHD) || (PHYSICS == ResRMHD)
  #if DIVB_CONTROL == NO
  print ("None]\n");
  #elif DIVB_CONTROL == EIGHT_WAVES
    print ("Powell's 8wave]\n");
  #elif DIVB_CONTROL == DIV_CLEANING
    #if GLM_EXTENDED == NO 
    print ("Divergence Cleaning (GLM)]\n");
    #elif GLM_EXTENDED == YES
    print ("Divergence Cleaning (Extended GLM)]\n");
    #endif
    
  #elif DIVB_CONTROL == CONSTRAINED_TRANSPORT
    print ("CT/");
    #if CT_EMF_AVERAGE == ARITHMETIC
    print ("Ar. average]\n");
    #elif CT_EMF_AVERAGE == CT_CONTACT
    print ("CT_CONTACT]\n");
    #elif CT_EMF_AVERAGE == UCT0
    print ("UCT0]\n");
    #elif CT_EMF_AVERAGE == UCT_HLL
    print ("UCT_HLL]\n");
    #elif CT_EMF_AVERAGE == CT_FLUX
    print ("CT_FLUX]\n");
    #elif CT_EMF_AVERAGE == CT_MAXWELL
    print ("CT_MAXWELL]\n");
    #elif CT_EMF_AVERAGE == UCT_HLLD
    print ("UCT_HLLD]\n");
    #elif CT_EMF_AVERAGE == UCT_GFORCE
    print ("UCT_GFORCE]\n");
    #endif
  #endif
#endif


#if (PHYSICS == ResRMHD)
  print ("                    ResMHD [div.E: ");
  #if DIVE_CONTROL == NO
  print ("None]\n");
  #elif DIVE_CONTROL == EIGHT_WAVES
    print ("Powell's 8wave]\n");
  #elif DIVE_CONTROL == DIV_CLEANING
    #if GLM_EXTENDED == NO 
    print ("Divergence Cleaning (GLM)]\n");
    #elif GLM_EXTENDED == YES
    print ("Divergence Cleaning (Extended GLM)]\n");
    #endif
  #elif DIVE_CONTROL == CONSTRAINED_TRANSPORT
    print ("CT]\n");
  #endif
#endif

  print ("  DIMENSIONS:       %d\n", DIMENSIONS);

  print ("  GEOMETRY:         ");
  if (GEOMETRY == CARTESIAN)    print ("Cartesian\n");
  if (GEOMETRY == CYLINDRICAL)  print ("Cylindrical\n");
  if (GEOMETRY == POLAR)        print ("Polar\n");
  if (GEOMETRY == SPHERICAL)    print ("Spherical\n");

  print ("  BODY_FORCE:       ");
  print (BODY_FORCE == NO ? "NO\n":"EXPLICIT\n");

  #if COOLING == H2_COOL
  print ("  COOLING:          H2_COOL\n");
  #elif COOLING == KROME
  print ("  COOLING:          KROME\n");
  #elif COOLING == MINEq
  print ("  COOLING:          MINEq\n");
  #elif COOLING == POWER_LAW
  print ("  COOLING:          POWER_LAW\n");
  #elif COOLING == SNEq
  print ("  COOLING:          SNEq\n");
  #elif COOLING == TABULATED
  print ("  COOLING:          Tabulated\n");
  #endif

  print ("  RECONSTRUCTION:   ");
  #ifndef FINITE_DIFFERENCE
  if (RECONSTRUCTION == FLAT)      print ("Flat ");
  if (RECONSTRUCTION == LINEAR)    print ("Linear ");
  if (RECONSTRUCTION == LimO3)     print ("LimO3 ");
  if (RECONSTRUCTION == WENO3)     print ("WENO3 ");
  if (RECONSTRUCTION == PARABOLIC) print ("Parabolic ");
  if (RECONSTRUCTION == WENOZ)     print ("WENOZ ");
  if (RECONSTRUCTION == MP5)       print ("MP5 ");
  #ifdef CHAR_LIMITING
  if (CHAR_LIMITING == YES) print (" (Characteristic lim)\n");
  else                      print (" (Primitive lim)\n");
  #endif
  #endif  /* ndef FINITE_DIFFERENCE */
 
  #ifdef FINITE_DIFFERENCE
  if (RECONSTRUCTION == LIMO3_FD)     print ("LimO3 (FD), 3rd order\n");
  if (RECONSTRUCTION == WENO3_FD)     print ("WENO3 (FD), 3rd order\n");
  if (RECONSTRUCTION == WENOZ_FD)     print ("WENOZ (FD) 5th order\n");
  if (RECONSTRUCTION == MP5_FD)       print ("MP5 (FD), 5th order\n");
  #endif

  print ("  TIME STEPPING:    ");
  if (TIME_STEPPING == EULER)            print ("Euler\n");
  if (TIME_STEPPING == RK2)              print ("Runge-Kutta II\n");
  if (TIME_STEPPING == RK3)              print ("Runge_Kutta III\n");
  if (TIME_STEPPING == RK4)              print ("Runge_Kutta IV\n");
  if (TIME_STEPPING == CHARACTERISTIC_TRACING)
                                         print ("Characteristic Tracing\n");
  #if TIME_STEPPING == HANCOCK
  if (PRIMITIVE_HANCOCK == YES) print ("Hancock [Primitive]\n");
  else                          print ("Hancock [Conservative]\n");
  #endif

  print ("  TRACERS:          %d\n", NTRACER);
  print ("  VARIABLES:        %d\n", NVAR);
  print ("  ENTROPY_SWITCH:   %s\n",(ENTROPY_SWITCH != NO ? "ENABLED":"NO"));
  #if PHYSICS == MHD 
  print ("  BACKGROUND_FIELD: %s\n",(BACKGROUND_FIELD == YES ? "YES":"NO"));
  #endif

  print ("  LOADED MODULES:\n");
  #ifdef HIGH_ORDER
  if      (HO_LAP_LIMITER == NO)             sprintf (str1,"NO");
  else if (HO_LAP_LIMITER == HO_JAMESON_LIM) sprintf (str1,"HO_JAMESON_LIM");
  else if (HO_LAP_LIMITER == HO_RDER_LIM)    sprintf (str1,"HO_RDER_LIM");
  else sprintf (str1,"[undef]");

  if      (HO_ORDER_REDUCTION == NO)     sprintf (str2,"NO");
  else if (HO_ORDER_REDUCTION == LINEAR) sprintf (str2,"LINEAR");
  else if (HO_ORDER_REDUCTION == WENO3)  sprintf (str2,"WENO3");
  else sprintf (str2,"[undef]");

  print ("\n  o [HIGH_ORDER]\n");
  print ("     - HO_LAP_LIMITER:         %s\n", str1);
  print ("     - HO_ORDER_REDUCTION:     %s\n", str2);
  #endif

  #ifdef SHEARINGBOX
  print ("\n  o [SHEARINGBOX]\n");
  print ("     - Order:             %d\n", SB_ORDER);
  print ("     - Sym Hydro Flux:    %s\n", 
           (SB_SYMMETRIZE_HYDRO == YES ? "YES":"NO"));
  print ("     - Sym Ey:            %s\n", 
           (SB_SYMMETRIZE_EY == YES ? "YES":"NO"));
  print ("     - Sym Ez:            %s\n", 
           (SB_SYMMETRIZE_EZ == YES ? "YES":"NO"));
  print ("     - Force EMF periods: %s\n", 
           (SB_FORCE_EMF_PERIODS == YES ? "YES":"NO"));
  #endif
  #ifdef FARGO
  print ("\n  o [FARGO]\n");
  print ("     - Order:         %d\n", FARGO_ORDER);
  print ("     - Av. Frequency: %d\n", FARGO_NSTEP_AVERAGE);
  #endif
  print ("\n");

  print ("  ROTATION:         ");
  print(ROTATING_FRAME == YES ? "YES\n":"NO\n");

  print ("  EOS:              ");
  if      (EOS == IDEAL)        print ("Ideal\n");
  else if (EOS == PVTE_LAW)     print ("PVTE_LAW\n");
  else if (EOS == BAROTROPIC)   print ("Barotropic\n");
  else if (EOS == ISOTHERMAL)   print ("Isothermal\n");
  else if (EOS == TAUB)         print ("Taub - TM\n");
  else                          print ("None\n");

  #if PARABOLIC_FLUX != NO
  print ("  DIFFUSION TERMS:");
  #if (RESISTIVITY == EXPLICIT) 
    print ("  Resistivity  [EXPLICIT]\n");  
  #elif (RESISTIVITY == SUPER_TIME_STEPPING)
    print ("  Resistivity  [STS]\n");  
  #elif (RESISTIVITY == RK_LEGENDRE)
    print ("  Resistivity  [RKL]\n");
  #endif

  #if (THERMAL_CONDUCTION == EXPLICIT) 
    print ("  Thermal Conduction [EXPLICIT]\n");  
  #elif (THERMAL_CONDUCTION == SUPER_TIME_STEPPING)
    print ("  Thermal Conduction [STS]\n");  
  #elif (THERMAL_CONDUCTION == RK_LEGENDRE)
    print ("  Thermal Conduction [RKL]\n");
  #endif

  #if (VISCOSITY == EXPLICIT) 
    print ("  Viscosity [EXPLICIT]\n");  
  #elif (VISCOSITY == SUPER_TIME_STEPPING)
    print ("  Viscosity [STS]\n");  
  #elif (VISCOSITY == RK_LEGENDRE)
    print ("  Viscosity [RKL]\n");
  #endif

  #endif

  #if RADIATION
  print ("  RADIATION:        YES\n");
  #if RADIATION_IMEX_SSP2
  print ("    - IMEX scheme:  SSP2(2,2,2)\n");
  #else
  print ("    - IMEX scheme:  IMEX1\n");
  #endif
  #if RADIATION_IMPL == RADIATION_FIXEDPOINT_RAD
    print ("    - Impl. method: FIXEDPOINT_RAD\n");
  #elif RADIATION_IMPL == RADIATION_NEWTON_GAS
    print ("    - Impl. method: NEWTON_GAS\n");
  #elif RADIATION_IMPL == RADIATION_NEWTON_RAD
    print ("    - Impl. method: NEWTON_RAD\n");
  #elif RADIATION_IMPL == RADIATION_FIXEDPOINT_GAS
    print ("    - Impl. method: FIXEDPOINT_GAS\n");
  #elif RADIATION_IMPL == RADIATION_NEWTON_NR_GAS
    print ("    - Impl. method: NEWTON_NR_GAS\n");
  #elif RADIATION_IMPL == RADIATION_NEWTON_NR_RAD
    print ("    - Impl. method: NEWTON_NR_RAD\n");
  #endif
  #endif

  print ("\n");

/* -----------------------------------------------------
    Print runtime configuration info (definitions.h 
    and from pluto.ini)
   ----------------------------------------------------- */
/*   
  print ("> Header file configuration (definitions.h):\n\n");
  print ("  +----------------------------------------------------------\n");
  fp = fopen("definitions.h","r");
  while ( fgets(sline, 512, fp) != NULL ) {
    print ("  | %s",sline);
  }
  fclose(fp);
  print ("  +---------------------------------------------------------\n\n");
*/
  print ("> Runtime configuration (%s):\n\n", ini_file);
  print ("  +----------------------------------------------------------\n");
  fp = fopen(ini_file,"r");
  while ( fgets(sline, 512, fp) != NULL ) {
    print ("  | %s",sline);
  }
  fclose(fp);
  print ("  +---------------------------------------------------------\n");

}

/* ********************************************************************* */
void ShowUnits ()
/*!
 *  Show units when cooling is enabled.
 *
 *
 *********************************************************************** */
{

#if COOLING != NO
  print ("> Cooling Module:    ");
  if (COOLING == SNEq)      print (" SNEq\n");
  if (COOLING == MINEq)     print (" MINEq\n");
  if (COOLING == TABULATED) print (" Tabulated\n");
  if (COOLING == H2_COOL)   print (" H2_COOL \n");
  if (COOLING == KROME)     print (" KROME \n");
#endif

  print ("> Normalization Units:\n\n");
  print ("  [Density]:      %8.3e (gr/cm^3), %8.3e (1/cm^3)\n",
          UNIT_DENSITY,UNIT_DENSITY/CONST_mp);
  print ("  [Pressure]:     %8.3e (dyne/cm^2)\n",
          UNIT_DENSITY*UNIT_VELOCITY*UNIT_VELOCITY);
  print ("  [Velocity]:     %8.3e (cm/s)\n",UNIT_VELOCITY);
  print ("  [Length]:       %8.3e (cm)\n",UNIT_LENGTH);
  print ("  [Temperature]:  %8.3e X (p/rho*mu) (K)\n",KELVIN);
  print ("  [Time]:         %8.3e (sec), %8.3e (yrs) \n",
       UNIT_LENGTH/UNIT_VELOCITY, UNIT_LENGTH/UNIT_VELOCITY/86400./365.);
#if PHYSICS == MHD || PHYSICS == RMHD
  print ("  [Mag Field]:    %8.3e (Gauss)\n",
           UNIT_VELOCITY*sqrt(4.0*CONST_PI*UNIT_DENSITY));
#endif

  print (" \n");
}

/* ********************************************************************* */
void CheckConfig()
/*
 *
 *
 * Check if the selected configuration is 
 * allowed.
 *
 *
 *********************************************************************** */
{
#if DIMENSIONS == 3 

  #if GEOMETRY  == CYLINDRICAL 
  print ("\n! Cylindrical coordinates are only 2D.\n");
  print ("! Use polar instead.\n");
  QUIT_PLUTO(1);
  #endif

  #if GEOMETRY == SPHERICAL 
  #if (TIME_STEPPING == HANCOCK) || (TIME_STEPPING == CHARACTERISTIC_TRACING)
  print ("\n ! Spherical 3D only works with RK integrators\n");
  QUIT_PLUTO(1);
  #endif
  #endif

#endif
}
