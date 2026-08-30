/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Output log file driver.

  The integration log file is divided into a "pre-step"
  and a "post-step" output.

  \authors A. Mignone(andrea.mignone@unito.it)\n

  \date    Apr 11, 2024
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"
#include <sys/stat.h>
#include <sys/types.h>

static FILE *g_flog;
static char log_file_name[512];

static void Particles_Log (Data *, timeStep *, Grid *);

/* ********************************************************************* */
void OutputLogPre(Data *data, timeStep *Dts, Runtime *runtime, Grid *grid)
/*!
 *  Provide log-file information *before* integration starts
 *********************************************************************** */
{
  char *indent = IndentString();
#if GPLUTO_LOG == YES
  print ("> nstep = %d; t = %0e; dt = %0e; %6.1f %%\n",
          g_stepNumber, g_time, g_dt, 100.0*g_time/runtime->tstop);
#else
  print ("step:%d; t = %10.4e; dt = %10.4e; %3.1f %%\n",
           g_stepNumber, g_time, g_dt, 100.0*g_time/runtime->tstop);
#endif
}

/* ********************************************************************* */
void OutputLogPost(Data *data, timeStep *Dts, Runtime *runtime, Grid *grid)
/*!
 *  Provide log-file information *after* integration ends
 *
 *********************************************************************** */
{
  char *indent = IndentString();
  
#if GPLUTO_LOG == YES

    #ifdef GLM_MHD
    print ("  glm_ch = %f\n",glm_ch);
    #endif

    #ifdef PARALLEL
    // double xglob;
    // double  xloc = Dts.invDt_hyp;
    // MPI_Allreduce (&xloc, &xglob, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    // Dts.invDt_hyp = xglob;
    #endif

    int nv;
    double mach[3], mach_max = 0.0, cs;
    double vc_max[NVAR], vc_min[NVAR], vc_glob[NVAR];
    NVAR_LOOP(nv) vc_max[nv] = -1.e18;
    NVAR_LOOP(nv) vc_min[nv] =  1.e18;

    #ifdef HIGH_ORDER
    RBox box;
    int ibeg = grid->lbeg[IDIR], iend = grid->lend[IDIR];
    int jbeg = grid->lbeg[JDIR], jend = grid->lend[JDIR];
    int kbeg = grid->lbeg[KDIR], kend = grid->lend[KDIR];

    RBoxDefine(ibeg, iend, jbeg, jend, kbeg, kend, CENTER, &box);
    ConsToPrim3D (data->Uc, data->Vc, data->flag, &box, grid);
    #endif

    for (int k = grid[0].lbeg[KDIR]; k <= grid[0].lend[KDIR]; k++){
    for (int j = grid[0].lbeg[JDIR]; j <= grid[0].lend[JDIR]; j++){
    for (int i = grid[0].lbeg[IDIR]; i <= grid[0].lend[IDIR]; i++){
      #if HAVE_ENERGY
        cs = data->Vc[PRS][k][j][i] / data->Vc[RHO][k][j][i];
        cs = sqrt(cs);
      #else
        cs = g_isoSoundSpeed;
      #endif
      mach[IDIR] = fabs(data->Vc[VX1][k][j][i]) / cs;
      mach[JDIR] = fabs(data->Vc[VX2][k][j][i]) / cs;
      mach[KDIR] = fabs(data->Vc[VX3][k][j][i]) / cs;
      mach_max = MAX(mach_max, mach[IDIR]);
      // printf("%f\n",mach_max);
      mach_max = MAX(mach_max, mach[JDIR]);
      // printf("%f\n",mach_max);
      mach_max = MAX(mach_max, mach[KDIR]);
      // printf("%f\n",mach_max);
      NVAR_LOOP(nv) {
        vc_max[nv] = MAX(vc_max[nv], data->Vc[nv][k][j][i]);
        vc_min[nv] = MIN(vc_min[nv], data->Vc[nv][k][j][i]);
      }
    }}}
    #ifdef PARALLEL
    double xglob;
    MPI_Allreduce (&mach_max, &xglob, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    mach_max = xglob;

    MPI_Allreduce (vc_min, vc_glob, NVAR, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    NVAR_LOOP(nv) vc_min[nv] = vc_glob[nv];

    MPI_Allreduce (vc_max, vc_glob, NVAR, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    NVAR_LOOP(nv) vc_max[nv] = vc_glob[nv];
    #endif
    print ("  Max(MACH) =  %f\n",mach_max);
    #if PHYSICS==MHD || PHYSICS==RMHD
    #if HAVE_ENERGY
    #ifdef GLM_MHD
    print ("  Max(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e % 0e % 0e\n",
               vc_max[RHO], vc_max[VX1], vc_max[VX2], vc_max[VX3],
                            vc_max[BX1], vc_max[BX2], vc_max[BX3],
                            vc_max[PRS], vc_max[PSI_GLM]);
    print ("  Min(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e % 0e % 0e\n",
               vc_min[RHO], vc_min[VX1], vc_min[VX2], vc_min[VX3],
                            vc_min[BX1], vc_min[BX2], vc_min[BX3],
                            vc_min[PRS], vc_min[PSI_GLM]);
    #else
    print ("  Max(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e % 0e\n",
               vc_max[RHO], vc_max[VX1], vc_max[VX2], vc_max[VX3],
                            vc_max[BX1], vc_max[BX2], vc_max[BX3], vc_max[PRS]);
    print ("  Min(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e % 0e\n",
               vc_min[RHO], vc_min[VX1], vc_min[VX2], vc_min[VX3],
                            vc_min[BX1], vc_min[BX2], vc_min[BX3], vc_min[PRS]);
    #endif
    #else
    print ("  Max(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e\n",
               vc_max[RHO], vc_max[VX1], vc_max[VX2], vc_max[VX3],
                            vc_max[BX1], vc_max[BX2], vc_max[BX3]);
    print ("  Min(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e\n",
               vc_min[RHO], vc_min[VX1], vc_min[VX2], vc_min[VX3],
                            vc_min[BX1], vc_min[BX2], vc_min[BX3]);
    #endif
    #endif

    #if PHYSICS==ResRMHD
    #ifdef GLM_MHD
    print ("  Max(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e % 0e % 0e % 0e % 0e % 0e % 0e\n",
               vc_max[RHO], vc_max[VX1], vc_max[VX2], vc_max[VX3],
               vc_max[BX1], vc_max[BX2], vc_max[BX3],
               vc_min[EX1], vc_min[EX2], vc_min[EX3],
               vc_max[PRS], vc_min[CRG], vc_max[PSI_GLM]);
    print ("  Min(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e % 0e % 0e % 0e % 0e % 0e % 0e\n",
               vc_min[RHO], vc_min[VX1], vc_min[VX2], vc_min[VX3],
               vc_min[BX1], vc_min[BX2], vc_min[BX3],
               vc_min[EX1], vc_min[EX2], vc_min[EX3],
               vc_min[PRS], vc_min[CRG], vc_min[PSI_GLM]);
    #else
    print ("  Max(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e % 0e % 0e  % 0e % 0e\n",
               vc_max[RHO], vc_max[VX1], vc_max[VX2], vc_max[VX3],
               vc_min[EX1], vc_min[EX2], vc_min[EX3],
               vc_max[BX1], vc_max[BX2], vc_max[BX3], vc_max[PRS]);
    print ("  Min(vc)   = % 0e  % 0e  % 0e  % 0e  % 0e  % 0e  % 0e % 0e % 0e  % 0e % 0e\n",
               vc_min[RHO], vc_min[VX1], vc_min[VX2], vc_min[VX3],
               vc_min[EX1], vc_min[EX2], vc_min[EX3],
               vc_min[BX1], vc_min[BX2], vc_min[BX3], vc_min[PRS]);
    #endif
    #endif

    #if PHYSICS==HD || PHYSICS==RHD
    #if HAVE_ENERGY
    print ("  Max(vc)   = % 0e  % 0e  % 0e  % 0e % 0e\n",
               vc_max[RHO], vc_max[VX1], vc_max[VX2], vc_max[VX3], vc_max[PRS]);
    print ("  Min(vc)   = % 0e  % 0e  % 0e  % 0e % 0e\n",
               vc_min[RHO], vc_min[VX1], vc_min[VX2], vc_min[VX3], vc_min[PRS]);
    #else
    print ("  Max(vc)   = % 0e  % 0e  % 0e  % 0e \n",
               vc_max[RHO], vc_max[VX1], vc_max[VX2], vc_max[VX3]);
    print ("  Min(vc)   = % 0e  % 0e  % 0e  % 0e \n",
               vc_min[RHO], vc_min[VX1], vc_min[VX2], vc_min[VX3]);   
    #endif
    #endif

#else

  print ("%s [Mach = %f", indent, g_maxMach);
  if (g_maxRiemannIter > 0){
    print (", NRiemann = %d", g_maxRiemannIter);
  }
  
  #if (PARABOLIC_FLUX & SUPER_TIME_STEPPING)
/*  printLog ("%s [Nsts = %d]\n",indent,Dts->Nsts); */
  print (", Nsts = %d",Dts->Nsts);
  #endif
  #if (PARABOLIC_FLUX & RK_LEGENDRE)
/*  printLog ("%s [Nrkl = %d]\n",indent,Dts->Nrkl); */
  print (", Nrkl = %d",Dts->Nrkl);
  #endif
  #if PHYSICS == ResRMHD
  print (", Nimex = %d",g_maxIMEXIter);
  #endif
  print ("]\n");
  #if (PARTICLES != NO)
  Particles_Log (data, Dts, grid);
  #endif

#endif /* GPLUTO_LOG */

}

/* ********************************************************************* */
void Particles_Log (Data *data, timeStep *Dts, Grid *grid)
/*!
 *  Global MPI reduction operations for PARTICLES Diagnostics
 *
 *********************************************************************** */
{
#if (PARTICLES != NO)
  int n;
  long int np_glob;
  particleNode *CurNode;
  Particle *pp;

  CurNode = data->PHead;
  #if PARTICLES == PARTICLES_CR
  double u2, gamma, c2 = PARTICLES_CR_C*PARTICLES_CR_C;
  #endif
  double kin, kin_glob;
  #if PARTICLES_LP_SPECTRA == YES
  double sEmin, sEmin_glob, sEmax, sEmax_glob;
  sEmin  = 0.0;
  sEmax  = 0.0;
  #endif

  kin = 0.0;
  while(CurNode != NULL){
    pp    = &(CurNode->p);
    #if PARTICLES == PARTICLES_CR

    #if PARTICLES_CR_GC == NO
    u2    = DOT_PRODUCT(pp->speed, pp->speed);
    gamma = sqrt(1.0 + u2/c2);
    kin  += u2/(gamma + 1.0);
    #else
    gamma = pp->speed[JDIR];
    kin   += c2*(gamma - 1.);
    #endif  /* PARTICLES_CR_GC == NO */

    #else  /* Any other particle type */

    kin  += 0.5*DOT_PRODUCT(pp->speed, pp->speed);

    #endif /* PARTICLES == PARTICLES_CR */

    #if PARTICLES_LP_SPECTRA == YES
    sEmin  += pp->eng[0];
    sEmax  += pp->eng[PARTICLES_LP_NEBINS];
    #endif

    CurNode = CurNode->next;
  }

#ifdef PARALLEL
  MPI_Allreduce(&p_nparticles, &np_glob, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allreduce(&kin, &kin_glob, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD); 
  kin = kin_glob/(np_glob+1.e-6);  /* Avoid division by zero when
                                      there're no particles */
  #if PARTICLES_LP_SPECTRA == YES
  MPI_Allreduce(&sEmin, &sEmin_glob, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  sEmin = sEmin_glob/(np_glob+1.e-6);  /* Avoid division by zero when there're no particles */
  MPI_Allreduce(&sEmax, &sEmax_glob, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  sEmax = sEmax_glob/(np_glob+1.e-6);  /* Avoid division by zero when there're no particles */
  #endif
  
#else
  np_glob = p_nparticles;
  kin    /= p_nparticles + 1.e-6;  /* Avoid division by zero when
                                      there're no particles */
   #if PARTICLES_LP_SPECTRA == YES
   sEmin  /= p_nparticles + 1.e-6;
   sEmax  /= p_nparticles + 1.e-6;
   #endif
#endif

/* -- printLog both local and global number of particles -- */
  print ("%s [Nparticles/tot: %ld / %ld; Nsub = %d; <Ek> = %10.4e]\n",
           IndentString(), p_nparticles, np_glob, Dts->Nsub_particles, kin);
#if PARTICLES_CR_GC
  if (data->particles_GC_InvalidCount != 0) {
    print ("! Warning: GC conditions were not met by %d particles.\n",
             data->particles_GC_InvalidCount);
  }
#endif

#if PARTICLES_LP_SPECTRA == YES
  print ("%s [<SpecE_min> = %10.4e, <SpecE_max> = %10.4e]\n",
          IndentString(), sEmin, sEmax);
#endif
#endif
}

/* /////////////////////////////////////////////////////////////////////
    The next set of functions provides basic functionalities to
     
     - set the log file
     - formatted output to the log file through the printLog() and printLog()
       functions
   ///////////////////////////////////////////////////////////////////// */


/* ********************************************************************* */
void LogFileClose(void)
/*!
 * Close a previously opened log file
 *                           
 *********************************************************************** */
{
#ifdef PARALLEL
  if (g_flog != NULL) fclose(g_flog);
#endif
}

/* ********************************************************************* */
void LogFileFlush(void)
/*!
 * Flushes the output buffer of a log file stream.
 *
 *********************************************************************** */
{
#ifdef PARALLEL
  if (g_flog != NULL) fflush (g_flog);
#endif
}

/* ********************************************************************* */
void LogFileOpen(char *log_dir, char *mode)
/*!
 * Open log file name in parallel mode.
 * Each processor has its own log file "pluto.prank.log", unless the
 * MULTIPLE_LOG_FILES flag has been set to FALSE.
 *
 * \param [in] output_dir  the name of the output directory
 * \param [in] cmd         pointer to cmd line option structure.
 *                           
 *********************************************************************** */
{
#ifdef PARALLEL

  sprintf (log_file_name, "%s/pluto.%d.log",log_dir,prank);

  #if MULTIPLE_LOG_FILES == NO
  if (prank != 0) return;
  #endif

  g_flog = fopen(log_file_name, mode);

  /* -- check that we have a valid directory name -- */

  if (g_flog == NULL){
    printf ("! SetLogFile(): %s cannot be written.\n",log_file_name);
    printf ("  Using current directory instead.\n");
    sprintf (log_file_name, "./pluto.%d.log",prank);
    g_flog = fopen(log_file_name, mode);
  }
#endif
}

#ifndef CHOMBO
/* ********************************************************************* */
void print (const char *fmt, ...)
/*!
 * Define print function for the static grid version
 * of PLUTO. The Chombo version is defined in Chombo/amrPLUTO.cpp
 *
 * Note: when MULTIPLE_LOG_FILES == FALSE, only proc #0 can
 *       execute this function.
 *
 *********************************************************************** */
{
#if MULTIPLE_LOG_FILES == NO
  if (prank != 0) return;
#endif

  va_list args;
  va_start(args, fmt);

#ifdef PARALLEL
  vfprintf(g_flog, fmt, args);
#else
  vprintf(fmt, args);
#endif

  va_end(args);
}
#endif

#ifndef CHOMBO
/* ********************************************************************* */
void printLog (const char *fmt, ...)
/*!
 * Define printLog function for the static grid version
 * of PLUTO [All processors will write]
 *
 *********************************************************************** */
{
  va_list args;
  va_start(args, fmt);

#ifdef PARALLEL

  #if MULTIPLE_LOG_FILES == NO
  if (prank != 0) return;
  #endif

  /* --------------------------------------------
      File may not be opened if
      MULTIPLE_LOG_FILES is set to NO.
     -------------------------------------------- */

  if (g_flog == NULL)  g_flog = fopen(log_file_name, "a");
  vfprintf(g_flog, fmt, args);
#else
  vprintf(fmt, args);
#endif

  va_end(args);
}
#endif

/* ********************************************************************* */
char *IndentString()
/*
 *
 *********************************************************************** */
{ 
  static char str[64];

  if      (g_stepNumber < 10)     sprintf (str,"%7s"," "); 
  else if (g_stepNumber < 100)    sprintf (str,"%8s"," ");
  else if (g_stepNumber < 1000)   sprintf (str,"%9s"," ");
  else if (g_stepNumber < 10000)  sprintf (str,"%10s"," ");
  else if (g_stepNumber < 100000) sprintf (str,"%11s"," ");
  else                            sprintf (str,"%12s"," ");

  return str;
}
