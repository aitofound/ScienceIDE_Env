#include "pluto.h"

/* Direct executable closure for the numerical helpers used by cooling, EOS,
 * chemistry table generation, and ODE integration. */
int prank=0;
long int g_usedMemory=0;
int g_nprocs=1, g_maxRootIter=1000;
#ifdef EXTERNAL_MT
extern void external_init_genrand64(unsigned long long seed);
extern unsigned long long external_genrand64_int64(void);
extern double external_genrand64_real1(void);
#endif
void Init(double *v, double x1, double x2, double x3) { (void)x1; (void)x2; (void)x3; v[0]=1.0; }
void PrimToConsLoc(double *v, double *u) { memcpy(u,v,256*sizeof(double)); }
void printLog(const char *fmt, ...)
{
  va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap);
}
void print(const char *fmt, ...)
{
  va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap);
}
char *Array1D(int n,size_t s){return calloc((size_t)n,s);}
char **Array2D(int n,int m,size_t s){char **a=calloc((size_t)n,sizeof(*a));char *p=calloc((size_t)n*(size_t)m,s);for(int i=0;i<n;i++)a[i]=p+(size_t)i*(size_t)m*s;return a;}
char ***Array3D(int n,int m,int q,size_t s){char ***a=calloc((size_t)n,sizeof(*a));char **b=calloc((size_t)n*(size_t)m,sizeof(*b));char *p=calloc((size_t)n*(size_t)m*(size_t)q,s);for(int i=0;i<n;i++){a[i]=b+(size_t)i*m;for(int j=0;j<m;j++)a[i][j]=p+((size_t)i*m*q+(size_t)j*q)*s;}return a;}
void FreeArray1D(void *p){free(p);} void FreeArray2D(void **p){if(p){free(p[0]);free(p);}}
void FreeArray3D(void ***p){if(p){free(p[0][0]);free(p[0]);free(p);}}
static void bad(const char *what,double x){fprintf(stderr,"math probe failure: %s (%g)\n",what,x);exit(1);}
static double root(double x,void *p){(void)p;return x*x-2.0;}
static void rhs(double x,double *y,double *f){(void)x;f[0]=-y[0];}
static double quad(double x,void *p){(void)p;return x*x;}

int main(void)
{
  double **a=ARRAY_2D(2,2,double), **inv=ARRAY_2D(2,2,double), **prod=ARRAY_2D(2,2,double); int ix[2]; double d,b[2]={5,5};
  a[0][0]=3;a[0][1]=1;a[1][0]=1;a[1][1]=2;
  if(!LUDecompose(a,2,ix,&d))bad("LU",0); LUBackSubst(a,2,ix,b);
  if(fabs(b[0]-1)>1e-10||fabs(b[1]-2)>1e-10)bad("LU solution",b[0]);
  double **ai=ARRAY_2D(2,2,double), **ao=ARRAY_2D(2,2,double);ai[0][0]=3;ai[0][1]=1;ai[1][0]=1;ai[1][1]=2;ao[0][0]=3;ao[0][1]=1;ao[1][0]=1;ao[1][1]=2; MatrixInverse(ai,inv,2); MatrixMultiply(ao,inv,prod,2);
  if(fabs(prod[0][0]-1)>1e-8||fabs(prod[1][1]-1)>1e-8)bad("matrix inverse",prod[0][0]);
  double **ga=ARRAY_2D(2,2,double), gx[2], gb[2]={5,5};ga[0][0]=3;ga[0][1]=1;ga[1][0]=1;ga[1][1]=2;GaussianElimination(ga,gx,gb,2);
  if(fabs(gx[0]-1)>1e-10||fabs(gx[1]-2)>1e-10)bad("Gaussian elimination",gx[0]);
  double am[3]={0,1,1},a0[3]={1,2,2},ap[3]={0,1,1},tb[3]={0,4,8},ty[4]={0,0,0,3};TridiagonalSolve(am,a0,ap,tb,ty,3);
  if(fabs(ty[1]-1)>1e-10||fabs(ty[2]-2)>1e-10)bad("tridiagonal",ty[1]);
  double rr; if(Brent(root,NULL,0,2,1e-12,1e-12,&rr)||fabs(rr-sqrt(2.0))>1e-9)bad("Brent",rr);
  if(Ridder(root,NULL,0,2,1e-12,1e-12,&rr)||fabs(rr-sqrt(2.0))>1e-9)bad("Ridder",rr);
  double z[4]; if(QuadraticSolve(1,-3,2,z)!=0)bad("quadratic",0); if(CubicSolve(0,-1,0,z)!=0)bad("cubic",0); if(QuarticSolve(0,0,-1,0,z)!=0)bad("quartic",0);
  double qra[2][2]={{2,1},{1,2}}, qrc[2], qrd[2], qrb[2]={3,3}; int sing=0; double **qrm=ARRAY_2D(2,2,double);qrm[0][0]=2;qrm[0][1]=1;qrm[1][0]=1;qrm[1][1]=2;QRDecompose(qrm,2,qrc,qrd,&sing);RSolve(qrm,2,qrd,qrb);(void)qra;
  double y2[1]={1},y4[1]={1},yc[1]={1}; ODE_Solve(y2,1,0,1,.01,rhs,ODE_RK2);ODE_Solve(y4,1,0,1,.01,rhs,ODE_RK4);ODE_Solve(yc,1,0,1,.01,rhs,ODE_CK45);
  if(fabs(y4[0]-exp(-1))>1e-6||fabs(y2[0]-exp(-1))>2e-4||fabs(yc[0]-exp(-1))>1e-6)bad("ODE",y4[0]);
  if(fabs(GaussQuadrature(quad,NULL,0,1,8,5)-1.0/3.0)>1e-8)bad("quadrature",0);
  RandomSeed(42,0); double unif=RandomNumber(0.0,1.0), gauss=GaussianRandomNumber(0.0,1.0), power=PowerLawRandomNumber(1.0,2.0,2.0); unsigned int seed=SeedGenerator(42); if(!(unif>=0&&unif<=1&&isfinite(gauss)&&power>=1&&power<=2&&seed>0))bad("random",unif);
#ifdef EXTERNAL_MT
  external_init_genrand64(42ULL); double ext1=external_genrand64_real1(); unsigned long long ext2=external_genrand64_int64();
  external_init_genrand64(42ULL); double ext1_repeat=external_genrand64_real1();
  external_init_genrand64(43ULL); double ext1_perturbed=external_genrand64_real1();
  if(!(ext1>=0.0&&ext1<=1.0&&ext2>0ULL&&ext1==ext1_repeat&&ext1!=ext1_perturbed))bad("external MT",ext1);
#endif
  double sx[3]={1,2,3}, sy[3]={1,4,9}, sd[3],sa[3],sbv[3],sc[3];MonotoneSplineCoeffs(sx,sy,sd,3,sa,sbv,sc,sd);SplineCoeffs(sx,sy,2,6,3,sa,sbv,sc,sd);
  if(!isfinite(BesselJ0(1))+!isfinite(BesselJ1(1))+!isfinite(BesselI0(1))+!isfinite(BesselI1(1))+!isfinite(BesselK0(1))+!isfinite(BesselK1(1))+!isfinite(BesselKn(2,1)))bad("Bessel",0);
  int si[4]={4,1,3,2};QuickSort(si,0,3);double sdv[4]={4,1,3,2};SortArray(sdv,4);double vec[3];VectorCartesianComponents(vec,1,2,3);
  Table2D tab;InitializeTable2D(&tab,1,3,3,1,3,3);for(int j=0;j<tab.ny;j++)for(int i=0;i<tab.nx;i++)tab.f[j][i]=tab.x[i]+tab.y[j];double tv; if(Table2DInterpolate(&tab,2,2,&tv)!=0||fabs(tv-4)>1e-8)bad("table interpolation",tv);FinalizeTable2D(&tab);
  FreeArray2D((void**)a);FreeArray2D((void**)inv);FreeArray2D((void**)prod);FreeArray2D((void**)ai);FreeArray2D((void**)ao);FreeArray2D((void**)ga);FreeArray2D((void**)qrm);
#ifdef EXTERNAL_MT
  printf("math_helpers=ok lu=ok roots=ok qr=ok ode=ok quadrature=ok interp=ok table=ok bessel=ok sorting=ok mt_external=ok\n");
#else
  printf("math_helpers=ok lu=ok roots=ok qr=ok ode=ok quadrature=ok interp=ok table=ok bessel=ok sorting=ok\n");
#endif
  return 0;
}
