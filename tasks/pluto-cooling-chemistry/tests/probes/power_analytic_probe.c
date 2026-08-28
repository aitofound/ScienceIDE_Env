#include "pluto.h"

double g_maxCoolingRate=0.1,g_minCoolingTemp=10.0,g_smallDensity=1.e-12,g_smallPressure=1.e-12;
double g_time=0.0,g_dt=1.e-3,g_maxMach=0.0,g_domBeg[3]={0},g_domEnd[3]={1,1,1},g_inputParam[32]={0},g_gamma=5.0/3.0;
long int IBEG=0,IEND=0,JBEG=0,JEND=0,KBEG=0,KEND=0,NX1=1,NX2=1,NX3=1,NX1_TOT=1,NX2_TOT=1,NX3_TOT=1,NMAX_POINT=1;
int VXn=1,VXt=2,VXb=3,MXn=1,MXt=2,MXb=3,BXn=4,BXt=5,BXb=6,EXn=0,EXt=0,EXb=0,g_dir=0,g_intStage=0,g_maxIMEXIter=0,g_maxRiemannIter=0,g_maxRootIter=0,g_nprocs=1,g_i=0,g_j=0,g_k=0;
long int g_stepNumber=0,g_usedMemory=0;
void printLog(const char *fmt,...){va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);}
char *Array1D(int n,size_t s){return calloc((size_t)n,s);}char **Array2D(int n,int m,size_t s){char **a=calloc((size_t)n,sizeof(*a));char *p=calloc((size_t)n*(size_t)m,s);for(int i=0;i<n;i++)a[i]=p+(size_t)i*(size_t)m*s;return a;}char ***Array3D(int n,int m,int q,size_t s){char ***a=calloc((size_t)n,sizeof(*a));char **b=calloc((size_t)n*(size_t)m,sizeof(*b));char *p=calloc((size_t)n*(size_t)m*(size_t)q,s);for(int i=0;i<n;i++){a[i]=b+(size_t)i*m;for(int j=0;j<m;j++)a[i][j]=p+((size_t)i*m*q+(size_t)j*q)*s;}return a;}void FreeArray1D(void*p){free(p);}void FreeArray2D(void **p){if(p){free(p[0]);free(p);}}void FreeArray3D(void ***p){if(p){free(p[0][0]);free(p[0]);free(p);}}
int main(void)
{
  double ****v=calloc(NVAR,sizeof(*v));for(int n=0;n<NVAR;n++){v[n]=calloc(1,sizeof(*v[n]));v[n][0]=calloc(1,sizeof(*v[n][0]));v[n][0][0]=calloc(1,sizeof(double));}
  v[RHO][0][0][0]=1.0;v[PRS][0][0][0]=1.0;timeStep dts;memset(&dts,0,sizeof(dts));dts.dt_cool=1.e99;Grid grid;memset(&grid,0,sizeof(grid));
  PowerLawCooling(v,.01,&dts,&grid);double p=v[PRS][0][0][0];
  if(!(isfinite(p)&&p>0.0&&p<1.0&&dts.dt_cool>0.0))return 1;
  printf("case=power-law-analytic initial=1 final=%0.17g dt_cool=%0.17g source=nonzero\n",p,dts.dt_cool);
  for(int n=0;n<NVAR;n++){free(v[n][0][0]);free(v[n][0]);free(v[n]);}free(v);return 0;
}
