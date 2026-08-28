#include "pluto.h"

/* Direct executable acceptance probe for one cooling family.  The runner
 * compiles this translation unit once for every vendored family and links the
 * exact family sources plus the shared source/ODE/math closure. */
double g_maxCoolingRate = 0.1;
double g_minCoolingTemp = 10.0;
double g_smallDensity = 1.e-12;
double g_smallPressure = 1.e-12;
double g_time = 0.0, g_dt = 1.e-3, g_maxMach = 0.0;
double g_domBeg[3] = {0.0,0.0,0.0}, g_domEnd[3] = {1.0,1.0,1.0};
double g_inputParam[32] = {0.0};
double g_gamma = 5.0/3.0;
long int IBEG=0,IEND=0,JBEG=0,JEND=0,KBEG=0,KEND=0;
long int NX1=1,NX2=1,NX3=1,NX1_TOT=1,NX2_TOT=1,NX3_TOT=1;
long int NMAX_POINT=1;
int VXn=1,VXt=2,VXb=3,MXn=1,MXt=2,MXb=3,BXn=4,BXt=5,BXb=6;
int EXn=0,EXt=0,EXb=0,g_dir=0,g_intStage=0,g_maxIMEXIter=0,g_maxRiemannIter=0,g_maxRootIter=0,g_nprocs=1;
long int g_stepNumber=0,g_usedMemory=0;
int g_i=0,g_j=0,g_k=0;

void printLog(const char *fmt, ...){ va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap); }
char *Array1D(int n, size_t s){ return calloc((size_t)n,s); }
char **Array2D(int n, int m, size_t s){ char **a=calloc((size_t)n,sizeof(*a)); char *p=calloc((size_t)n*(size_t)m,s); for(int i=0;i<n;i++) a[i]=p+(size_t)i*(size_t)m*s; return a; }
char ***Array3D(int n, int m, int q, size_t s){ char ***a=calloc((size_t)n,sizeof(*a)); char **b=calloc((size_t)n*(size_t)m,sizeof(*b)); char *p=calloc((size_t)n*(size_t)m*(size_t)q,s); for(int i=0;i<n;i++){a[i]=b+(size_t)i*m; for(int j=0;j<m;j++) a[i][j]=p+((size_t)i*m*q+(size_t)j*q)*s;} return a; }
void FreeArray1D(void *p){ free(p); }
void FreeArray2D(void **p){ if(p){free(p[0]);free(p);} }
void FreeArray3D(void ***p){ if(p){free(p[0][0]);free(p[0]);free(p);} }

static void fail(const char *what, double value){ fprintf(stderr,"family probe failure: %s (%g)\n",what,value); exit(1); }
static void finite_vec(const char *what, const double *v){ for(int n=0;n<NVAR_COOLING;n++) if(!isfinite(v[n])) fail(what,v[n]); }
static int changed_vec(const double *a, const double *b){
  for(int n=0;n<NVAR_COOLING;n++) if(fabs(a[n]-b[n]) > 1.e-14*(1.0+fabs(a[n])+fabs(b[n]))) return 1;
  return 0;
}
static double *make_state(void){
  double *v=calloc(NVAR_COOLING,sizeof(*v));
  v[RHO]=1.0;
  v[PRS]=45.0; v[RHOE]=45.0/(g_gamma-1.0); /* ~10^4 K for the Jet units */
#if COOLING == SNEq
  v[X_HI]=0.35;
#elif COOLING == H2_COOL
  v[X_HI]=0.55; v[X_H2]=0.15; v[X_HII]=0.15;
#elif COOLING == MINEq
  for(int n=0;n<NIONS;n++) v[NFLX+n]=0.01;
  v[X_HI]=0.60; v[X_HeI]=0.25; v[X_HeII]=0.05;
#elif COOLING == TABULATED
  /* no chemical fields */
#endif
  return v;
}
static void copy_vec(double *dst, const double *src){ memcpy(dst,src,(size_t)NVAR_COOLING*sizeof(*dst)); }
static double ****make_data(const double *v){
  double ****a=calloc(NVAR_COOLING,sizeof(*a));
  for(int n=0;n<NVAR_COOLING;n++){a[n]=calloc(1,sizeof(*a[n]));a[n][0]=calloc(1,sizeof(*a[n][0]));a[n][0][0]=calloc(1,sizeof(double));*a[n][0][0]=v[n];}
  return a;
}
static void free_data(double ****a){for(int n=0;n<NVAR_COOLING;n++){free(a[n][0][0]);free(a[n][0]);free(a[n]);}free(a);}
static double root_fn(double x, void *unused){(void)unused; return x*x-2.0;}

static void exercise_helpers(void){
  double **a=ARRAY_2D(2,2,double); int indx[2]; double d, b[2]={5.0,5.0};
  a[0][0]=3.0; a[0][1]=1.0; a[1][0]=1.0; a[1][1]=2.0;
  if(!LUDecompose(a,2,indx,&d)) fail("LU decomposition",0.0); LUBackSubst(a,2,indx,b);
  if(fabs(b[0]-1.0)>1.e-12 || fabs(b[1]-2.0)>1.e-12) fail("LU solution",b[0]);
  double r=0.0; if(Brent(root_fn,NULL,0.0,2.0,1.e-12,1.e-12,&r)!=0 || fabs(r-sqrt(2.0))>1.e-9) fail("Brent root",r);
  if(Ridder(root_fn,NULL,0.0,2.0,1.e-12,1.e-12,&r)!=0 || fabs(r-sqrt(2.0))>1.e-9) fail("Ridder root",r);
  FreeArray2D((void **)a);
}

static void exercise_odes(const double *initial){
  intList vars; vars.nvar=NIONS+1; vars.indx[0]=RHOE;
  for(int n=0;n<NIONS;n++) vars.indx[n+1]=NFLX+n;
  double *v=calloc(NVAR_COOLING,sizeof(*v)), *k=calloc(NVAR_COOLING,sizeof(*k)), *out=calloc(NVAR_COOLING,sizeof(*out));
  copy_vec(v,initial); Radiat(v,k); finite_vec("ODE input rhs",k);
#define RUN_ODE(label,call) do { copy_vec(v,initial); Radiat(v,k); copy_vec(out,v); double next=(call); (void)next; finite_vec(label,out); if(!changed_vec(out,initial)) fail(label,0.0); } while(0)
  RUN_ODE("RKF12", SolveODE_RKF12(v,k,out,1.e-5,&vars));
  RUN_ODE("RKF23", SolveODE_RKF23(v,k,out,1.e-5,&vars));
  RUN_ODE("RK4",   SolveODE_RK4  (v,k,out,1.e-5,&vars));
  RUN_ODE("CK45",  SolveODE_CK45 (v,k,out,1.e-5,2.e-5,&vars));
#if COOLING == MINEq
  RUN_ODE("ROS34", SolveODE_ROS34(v,k,out,1.e-5,2.e-5));
#endif
#undef RUN_ODE
  double **J=ARRAY_2D(NIONS+1,NIONS+1,double); copy_vec(v,initial); Radiat(v,k); Numerical_Jacobian(v,J);
  int jac_nonzero=0;
  for(int i=0;i<NIONS+1;i++) for(int j=0;j<NIONS+1;j++) { if(!isfinite(J[i][j])) fail("numerical Jacobian",J[i][j]); if(fabs(J[i][j]) > 1.e-14) jac_nonzero=1; }
  if(!jac_nonzero) fail("numerical Jacobian all-zero",0.0);
  FreeArray2D((void **)J); free(out); free(k); free(v);
}

int main(void){
  exercise_helpers();
  double *v=make_state(), *rhs=calloc(NVAR_COOLING,sizeof(*rhs));
  double *eq=make_state();
  Radiat(v,rhs); finite_vec("Radiat rhs",rhs);
  if(rhs[RHOE] > 1.e-20) fail("positive thermal source",rhs[RHOE]);
  double *vp=make_state(), *rhs_p=calloc(NVAR_COOLING,sizeof(*rhs_p));
  vp[RHO] *= 1.25; vp[PRS] *= 1.10; vp[RHOE] = vp[PRS]/(g_gamma-1.0);
  Radiat(vp,rhs_p); finite_vec("perturbed Radiat rhs",rhs_p);
  if(!changed_vec(rhs,rhs_p)) fail("Radiat perturbation sensitivity",0.0);
#if COOLING == SNEq || COOLING == MINEq || COOLING == H2_COOL
  double ne=CompEquil(1.0,10000.0,eq);
  if(!(isfinite(ne) && ne >= 0.0)) fail("equilibrium electron density",ne);
  for(int n=NFLX;n<NFLX+NIONS;n++) if(!(eq[n]>=0.0 && eq[n]<=1.0)) fail("equilibrium ion",eq[n]);
#else
  double ne=0.0;
#endif
  double rate=GetMaxRate(v,rhs,10000.0);
  if(!isfinite(rate) || rate < 0.0) fail("maximum rate",rate);
  exercise_odes(v);
  double ****a=make_data(v);
  uint16_t ***flag=calloc(1,sizeof(*flag)); flag[0]=calloc(1,sizeof(*flag[0])); flag[0][0]=calloc(1,sizeof(*flag[0][0]));
  double x0=0.5; Grid grid; memset(&grid,0,sizeof(grid)); grid.x[0]=&x0; grid.x[1]=&x0; grid.x[2]=&x0;
  timeStep dts; memset(&dts,0,sizeof(dts)); dts.dt_cool=1.e99;
  Data d; memset(&d,0,sizeof(d)); d.Vc=a; d.flag=flag;
  CoolingSource(&d,1.e-5,&dts,&grid);
  double p=a[PRS][0][0][0];
  if(!(isfinite(p) && p > 0.0 && dts.dt_cool > 0.0)) fail("source update",p);
  printf("COOLING=%d NIONS=%d rhs_energy=%0.17g rate=%0.17g equilibrium_ne=%0.17g final_pressure=%0.17g dt_cool=%0.17g helpers=ok odes=ok\n",COOLING,NIONS,rhs[RHOE],rate,ne,p,dts.dt_cool);
  free(flag[0][0]);free(flag[0]);free(flag); free_data(a);free(eq);free(rhs_p);free(vp);free(rhs);free(v);return 0;
}
