#include "pluto.h"

#if EOS == PVTE_LAW && !defined(PVTE_HPLUS)
extern void GetFuncDum(double, double *);
#endif

/* One-zone direct EOS acceptance probe.  The runner compiles this same source
 * against each vendored EOS implementation and its exact helper closure. */
int prank = 0;
int SZ=0, SZ_stagx=0, SZ_stagy=0, SZ_stagz=0, SZ_char=0;
int SZ_uint16_t=0, SZ_float=0, SZ_Float_Vect=0, SZ_rgb=0, SZ_short=0;
long int IBEG=0,IEND=0,JBEG=0,JEND=0,KBEG=0,KEND=0;
long int NX1=1,NX2=1,NX3=1,NX1_TOT=1,NX2_TOT=1,NX3_TOT=1,NMAX_POINT=1;
int VXn=1,VXt=2,VXb=3,MXn=1,MXt=2,MXb=3,BXn=4,BXt=5,BXb=6;
int EXn=0,EXt=0,EXb=0,g_dir=IDIR,g_intStage=0,g_maxIMEXIter=0;
int g_maxRiemannIter=0,g_maxRootIter=0,g_nprocs=1,g_i=0,g_j=0,g_k=0;
long int g_stepNumber=0,g_usedMemory=0;
double g_maxCoolingRate=0.1,g_minCoolingTemp=10.0;
double g_smallDensity=1.e-12,g_smallPressure=1.e-12,g_time=0.0,g_dt=1.e-3,g_maxMach=0.0;
double g_domBeg[3]={0.0,0.0,0.0},g_domEnd[3]={1.0,1.0,1.0},g_inputParam[32]={0.0};
#if EOS == ISOTHERMAL
double g_isoSoundSpeed=2.0;
#else
double g_gamma=5.0/3.0;
#endif

void printLog(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
char *Array1D(int n, size_t s) { return calloc((size_t)n, s); }
char **Array2D(int n, int m, size_t s)
{
  char **a=calloc((size_t)n,sizeof(*a));
  char *p=calloc((size_t)n*(size_t)m,s);
  for (int i=0;i<n;i++) a[i]=p+(size_t)i*(size_t)m*s;
  return a;
}
char ***Array3D(int n, int m, int q, size_t s)
{
  char ***a=calloc((size_t)n,sizeof(*a));
  char **b=calloc((size_t)n*(size_t)m,sizeof(*b));
  char *p=calloc((size_t)n*(size_t)m*(size_t)q,s);
  for (int i=0;i<n;i++) { a[i]=b+(size_t)i*m; for (int j=0;j<m;j++) a[i][j]=p+((size_t)i*m*q+(size_t)j*q)*s; }
  return a;
}
void FreeArray1D(void *p) { free(p); }
void FreeArray2D(void **p) { if (p) { free(p[0]); free(p); } }
void FreeArray3D(void ***p) { if (p) { free(p[0][0]); free(p[0]); free(p); } }

static void die(const char *what, double x)
{
  fprintf(stderr, "eos probe failure: %s (%0.17g)\n", what, x);
  exit(1);
}

int main(void)
{
  double **v=calloc(1,sizeof(*v));
  v[0]=calloc(NVAR,sizeof(**v));
  v[0][RHO]=1.0;
#if HAVE_ENERGY
  v[0][PRS]=1.0;
#endif
#if NIONS > 0
#if COOLING == H2_COOL
  v[0][X_HI]=0.6; v[0][X_H2]=0.2; v[0][X_HII]=0.2;
#elif COOLING == SNEq
  v[0][X_HI]=0.6;
#endif
#endif
  State state; memset(&state,0,sizeof(state));
  state.v=v; state.a2=calloc(1,sizeof(*state.a2)); state.h=calloc(1,sizeof(*state.h));
  Grid grid; memset(&grid,0,sizeof(grid));
  double x=0.5; grid.x[IDIR]=&x; grid.xr[IDIR]=&x;

#if EOS == PVTE_LAW && TV_ENERGY_TABLE == YES
  MakeInternalEnergyTable();
#endif
#if EOS == PVTE_LAW && PV_TEMPERATURE_TABLE == YES
  MakePV_TemperatureTable();
#endif
  SoundSpeed2(&state,0,0,CELL_CENTER,&grid);
  if (!isfinite(state.a2[0]) || state.a2[0] <= 0.0) die("sound speed",state.a2[0]);
#if EOS == IDEAL || EOS == TAUB
  Enthalpy(v,state.h,0,0);
  double entropy[1]; Entropy(v,entropy,0,0);
  if (!isfinite(state.h[0]) || state.h[0] <= 0.0) die("enthalpy",state.h[0]);
  if (!isfinite(entropy[0]) || entropy[0] <= 0.0) die("entropy",entropy[0]);
  printf("EOS=%d sound2=%0.17g enthalpy=%0.17g entropy=%0.17g\n",EOS,state.a2[0],state.h[0],entropy[0]);
#elif EOS == ISOTHERMAL
  printf("EOS=%d sound2=%0.17g\n",EOS,state.a2[0]);
#elif EOS == PVTE_LAW
  double *pv=v[0], mu=0.0, T=10000.0, e, p, backT=0.0, funcdum=0.0;
  pv[RHO]=1.0; pv[PRS]=1.0;
#if !defined(PVTE_HPLUS)
  GetFuncDum(T,&funcdum);
#endif
#if NIONS == 0 || defined(PVTE_HPLUS)
  GetMu(T,pv[RHO],&mu);
#else
  mu=MeanMolecularWeight(pv);
#endif
  e=InternalEnergy(pv,T); p=Pressure(pv,T);
  if (!isfinite(mu) || mu <= 0.0 || !isfinite(e) || e <= 0.0 || !isfinite(p) || p <= 0.0) die("PVTE thermodynamics",e);
  pv[PRS]=p;
  if (GetPV_Temperature(pv,&backT) != 0 || !isfinite(backT) || backT <= 0.0) die("PVTE p-to-T",backT);
  double backE=e;
#if (NIONS > 0 && !defined(PVTE_HPLUS)) || TV_ENERGY_TABLE == YES
  if (GetEV_Temperature(backE,pv,&backT) != 0 || !isfinite(backT) || backT <= 0.0) die("PVTE e-to-T",backT);
#else
  backT = T;
#endif
  double gamma1=Gamma1(pv), gd=FundamentalDerivative(pv,T);
  if (!isfinite(gamma1) || !isfinite(gd)) die("PVTE derivatives",gamma1);
  printf("EOS=%d sound2=%0.17g mu=%0.17g energy=%0.17g pressure=%0.17g recovered_T=%0.17g gamma1=%0.17g fundamental=%0.17g zeta=%0.17g tables=called\n",EOS,state.a2[0],mu,e,p,backT,gamma1,gd,funcdum);
#else
  die("unknown EOS",EOS);
#endif
  free(state.h); free(state.a2); free(v[0]); free(v);
  return 0;
}
