#include "pluto.h"

/* Execute the cooling source split, serialize the cooling state, reconstruct a
 * fresh Data object, and resume.  This is deliberately a small state
 * checkpoint/restart contract; it does not claim a full Jet deck restart. */
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

void printLog(const char *fmt, ...)
{
  va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap);
}
char *Array1D(int n,size_t s){return calloc((size_t)n,s);}
char **Array2D(int n,int m,size_t s){char **a=calloc((size_t)n,sizeof(*a));char *p=calloc((size_t)n*(size_t)m,s);for(int i=0;i<n;i++)a[i]=p+(size_t)i*(size_t)m*s;return a;}
char ***Array3D(int n,int m,int q,size_t s){char ***a=calloc((size_t)n,sizeof(*a));char **b=calloc((size_t)n*(size_t)m,sizeof(*b));char *p=calloc((size_t)n*(size_t)m*(size_t)q,s);for(int i=0;i<n;i++){a[i]=b+(size_t)i*m;for(int j=0;j<m;j++)a[i][j]=p+((size_t)i*m*q+(size_t)j*q)*s;}return a;}
void FreeArray1D(void *p){free(p);} void FreeArray2D(void **p){if(p){free(p[0]);free(p);}}
void FreeArray3D(void ***p){if(p){free(p[0][0]);free(p[0]);free(p);}}

static void fail(const char *what, double value)
{
  fprintf(stderr,"restart probe failure: %s (%0.17g)\n",what,value);
  exit(1);
}

static double *state_new(void)
{
  double *v=calloc(NVAR_COOLING,sizeof(*v));
  if (v == NULL) fail("state allocation",0.0);
  v[RHO]=1.0; v[PRS]=45.0; v[RHOE]=45.0/(g_gamma-1.0);
#if COOLING == SNEq
  v[X_HI]=0.35;
#elif COOLING == H2_COOL
  v[X_HI]=0.55; v[X_H2]=0.15; v[X_HII]=0.15;
#elif COOLING == MINEq
  for (int n=0;n<NIONS;n++) v[NFLX+n]=0.01;
  v[X_HI]=0.60; v[X_HeI]=0.25; v[X_HeII]=0.05;
#endif
  return v;
}

static void source_step(double *v, double dt)
{
  double ****a=calloc(NVAR_COOLING,sizeof(*a));
  uint16_t ***flag=calloc(1,sizeof(*flag));
  if (a == NULL || flag == NULL) fail("data allocation",0.0);
  for (int n=0;n<NVAR_COOLING;n++) {
    a[n]=calloc(1,sizeof(*a[n])); a[n][0]=calloc(1,sizeof(*a[n][0]));
    if (a[n][0] == NULL) fail("cell allocation",0.0);
    a[n][0][0]=calloc(1,sizeof(double)); *a[n][0][0]=v[n];
  }
  flag[0]=calloc(1,sizeof(*flag[0])); flag[0][0]=calloc(1,sizeof(*flag[0][0]));
  if (flag[0] == NULL || flag[0][0] == NULL) fail("flag allocation",0.0);
  double x=0.5; Grid grid; memset(&grid,0,sizeof(grid)); grid.x[0]=&x; grid.x[1]=&x; grid.x[2]=&x;
  timeStep dts; memset(&dts,0,sizeof(dts)); dts.dt_cool=1.e99;
  Data d; memset(&d,0,sizeof(d)); d.Vc=a; d.flag=flag;
  CoolingSource(&d,dt,&dts,&grid);
  if (!isfinite(dts.dt_cool) || dts.dt_cool <= 0.0) fail("source split dt_cool",dts.dt_cool);
  for (int n=0;n<NVAR_COOLING;n++) {
    if (!isfinite(*a[n][0][0])) fail("source split state",*a[n][0][0]);
    v[n]=*a[n][0][0];
  }
  for (int n=0;n<NVAR_COOLING;n++){free(a[n][0][0]);free(a[n][0]);free(a[n]);}
  free(a); free(flag[0][0]); free(flag[0]); free(flag);
}

static int changed(const double *a, const double *b)
{
  for (int n=0;n<NVAR_COOLING;n++)
    if (fabs(a[n]-b[n]) > 1.e-14*(1.0+fabs(a[n])+fabs(b[n]))) return 1;
  return 0;
}

int main(void)
{
  const double dt=1.e-5;
  double *uninterrupted=state_new(), *restart=state_new(), *initial=state_new();
  source_step(uninterrupted,dt);
  source_step(uninterrupted,dt);
  source_step(restart,dt);
  if (!changed(restart,initial)) fail("first source split made no update",0.0);

  FILE *checkpoint=fopen("cooling-restart-state.bin","wb");
  if (checkpoint == NULL || fwrite(restart,sizeof(double),NVAR_COOLING,checkpoint) != NVAR_COOLING) fail("checkpoint write",0.0);
  if (fclose(checkpoint) != 0) fail("checkpoint close",0.0);
  memset(restart,0,(size_t)NVAR_COOLING*sizeof(*restart));
  checkpoint=fopen("cooling-restart-state.bin","rb");
  if (checkpoint == NULL || fread(restart,sizeof(double),NVAR_COOLING,checkpoint) != NVAR_COOLING) fail("checkpoint read",0.0);
  fclose(checkpoint);
  source_step(restart,dt);
  for (int n=0;n<NVAR_COOLING;n++) {
    double scale=1.0+fabs(uninterrupted[n])+fabs(restart[n]);
    if (fabs(uninterrupted[n]-restart[n]) > 1.e-11*scale) fail("restart differs from uninterrupted split",uninterrupted[n]-restart[n]);
  }
  printf("COOLING=%d restart=serialized_source_split_ok steps=2 checkpoint_fields=%d source_update=executed\n",COOLING,NVAR_COOLING);
  free(initial); free(restart); free(uninterrupted);
  return 0;
}
