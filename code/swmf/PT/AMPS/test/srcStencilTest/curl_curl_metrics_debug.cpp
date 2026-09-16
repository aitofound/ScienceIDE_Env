/***************************************************************************************
 * curl_curl_metrics_debug.cpp
 * -------------------------------------------------------------------------------------
 * PURPOSE
 *   Unified diagnostics for metric scaling issues in:
 *     curl_curl(E), grad_div(E), and laplacian(E)  (corner/collocated stencils)
 *
 * WHAT IT PRINTS (per N and per order)
 *   1) Scale probes (least-squares scale vs analytic): s_CC, s_GD, s_LP (expect ~1)
 *   2) Identity residual CC - (GD - LP): Linf, RelL2 (expect tiny)
 *   3) Polynomial Laplacian sanity on f = x^2 + y^2 + z^2: expect 6 (Linf, RelL2 ~ 0)
 *   4) Axis moments of exported Laplacian rows (per axis):
 *        sum w ≈ 0,  sum k w ≈ 0,  sum k^2 w ≈ 2/(d^2)   (catches 1/2-strength D²)
 *   5) (Verbose) Raw stencil prints via cStencil::Print() for visual inspection
 *
 * USAGE
 *   ./stencil_tests curl_curl_metrics_debug           # defaults to N={16,24,32,48,64}
 *   ./stencil_tests curl_curl_metrics_debug 20 40 80  # custom Ns
 *
 * NOTES
 *   - No extra scaling is applied here; we use the stencils as exported.
 *   - Enable DEBUG_PRINT_STENCILS=1 to dump raw cStencil rows to stdout.
 ***************************************************************************************/

#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <functional>

#include "test_harness.h"
#include "test_register.h"
#include "test_force_link_all.h"
#include "pic.h"        // brings ECSIM stencil types and builders
#include "stencil.h"    // for cStencil::Print() (diagnostic dumps)  // :contentReference[oaicite:1]{index=1}

using namespace PIC::FieldSolver::Electromagnetic::ECSIM::Stencil;

#ifndef DEBUG_PRINT_STENCILS
// Flip to 1 to print the full raw stencils via cStencil::Print() (can be verbose)
#define DEBUG_PRINT_STENCILS 1
#endif

namespace {

// ---------------- Helpers ----------------
inline int wrap(int i, int N){ int r=i%N; return r<0? r+N : r; }
struct Vec3 { double x,y,z; };

inline Vec3 analyticE(double x,double y,double z,double a,double b,double c){
  return {
    std::sin(a*x)*std::cos(b*y)*std::cos(c*z),
    std::cos(a*x)*std::sin(b*y)*std::cos(c*z),
    std::cos(a*x)*std::cos(b*y)*std::sin(c*z)
  };
}

inline Vec3 analyticGradDivE(double x,double y,double z,double a,double b,double c){
  const double sx=std::sin(a*x), cx=std::cos(a*x);
  const double sy=std::sin(b*y), cy=std::cos(b*y);
  const double sz=std::sin(c*z), cz=std::cos(c*z);
  const double K=(a+b+c);
  return { K*(-a*sx)*cy*cz, K*cx*(-b*sy)*cz, K*cx*cy*(-c*sz) };
}

inline Vec3 analyticLaplacianE(double x,double y,double z,double a,double b,double c){
  const double sx=std::sin(a*x), cx=std::cos(a*x);
  const double sy=std::sin(b*y), cy=std::cos(b*y);
  const double sz=std::sin(c*z), cz=std::cos(c*z);
  const double Ex=sx*cy*cz, Ey=cx*sy*cz, Ez=cx*cy*sz;
  const double lam=(a*a+b*b+c*c);
  return { -lam*Ex, -lam*Ey, -lam*Ez };
}

inline Vec3 analyticCurlCurlE(double x,double y,double z,double a,double b,double c){
  const Vec3 gd = analyticGradDivE(x,y,z,a,b,c);
  const Vec3 lp = analyticLaplacianE(x,y,z,a,b,c);
  return { gd.x - lp.x, gd.y - lp.y, gd.z - lp.z };
}

static inline double apply_exported(const cStencil::cStencilData& S,
                                    const std::vector<double>& F,
                                    int i,int j,int k, int Nx,int Ny,int Nz) {
  double acc=0.0;
  for (int n=0;n<S.Length;++n){
    const int ii=wrap(i+S.Data[n].i,Nx);
    const int jj=wrap(j+S.Data[n].j,Ny);
    const int kk=wrap(k+S.Data[n].k,Nz);
    const size_t id=(size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
    acc += S.Data[n].a * F[id];
  }
  return acc;
}

inline double dot3(double ax,double ay,double az, double bx,double by,double bz){
  return ax*bx + ay*by + az*bz;
}

// Axis moments of an exported (3D) stencil treating the chosen axis as 1D k.
static void Print1DSecondMoments(const char* tag,
                                 const cStencil::cStencilData& S,
                                 int axis, double d)
{
  double m0=0.0, m1=0.0, m2=0.0;
  for (int n=0; n<S.Length; ++n) {
    const double w = S.Data[n].a;
    const int k = (axis==0? S.Data[n].i : axis==1? S.Data[n].j : S.Data[n].k);
    m0 += w;
    m1 += k * w;
    m2 += (double)k * (double)k * w;
  }
  std::cout << "  [" << tag << " axis=" << axis << "] "
            << "sum w=" << std::scientific << m0
            << "  sum (k w)=" << m1
            << "  sum (k^2 w)=" << m2
            << "   (expect ~ 0, 0, 2/" << (d*d) << ")\n";
}

// Optional: print a separator per N for readability
static void print_big_sep(int N, double dx){
  std::cout << "\n======== N = " << N << " (dx=" << std::setprecision(6) << std::fixed << dx
            << ") ==============================================\n";
}

struct Variant {
  const char* name;
  std::function<void(cGradDivEStencil*, double,double,double)>   build_gd;
  std::function<void(cLaplacianStencil*, double,double,double)>  build_lap;
  std::function<void(cCurlCurlEStencil*, double,double,double)>  build_cc;
};

struct ProbeResults {
  double sCC=0, sGD=0, sLP=0;   // least-squares scales vs analytic (expect ~1)
  double R_linf=0, R_relL2=0;   // identity residual norms for CC - (GD - LP)
  double poly_lap_linf=0, poly_lap_relL2=0; // Laplacian polynomial sanity (vs 6)
};

static ProbeResults
run_probe_for_variant(const Variant& V, int N, double L,
                      double a,double b,double c, bool dump_stencils_now)
{
  const double dx=L/N, dy=L/N, dz=L/N;
  const int Nx=N, Ny=N, Nz=N;
  const size_t NT=(size_t)Nx*Ny*Nz;

  // Build stencils
  cGradDivEStencil GD[3];
  cLaplacianStencil LP;
  cCurlCurlEStencil CC[3];
  V.build_gd(GD,dx,dy,dz);
  V.build_lap(&LP,dx,dy,dz);
  V.build_cc(CC,dx,dy,dz);

#if DEBUG_PRINT_STENCILS
  if (dump_stencils_now) {
    char msg[200];
    
    std::cout << "\n-- RAW cStencil rows via cStencil::Print() -- [ " << V.name << " ]\n";
    
    std::cout << ">>> Laplacian rows (Ex/Ey/Ez)\n";
    sprintf(msg, "[%s] Laplacian Ex: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    LP.Ex.SetSymbol(msg);
    LP.Ex.Print();
    
    sprintf(msg, "[%s] Laplacian Ey: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    LP.Ey.SetSymbol(msg);
    LP.Ey.Print();
    
    sprintf(msg, "[%s] Laplacian Ez: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    LP.Ez.SetSymbol(msg);
    LP.Ez.Print();

    std::cout << ">>> GradDiv rows (Gx:Ex/Ey/Ez; Gy:Ex/Ey/Ez; Gz:Ex/Ey/Ez)\n";
    sprintf(msg, "[%s] GradDiv Gx->Ex: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[0].Ex.SetSymbol(msg);
    GD[0].Ex.Print();
    
    sprintf(msg, "[%s] GradDiv Gx->Ey: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[0].Ey.SetSymbol(msg);
    GD[0].Ey.Print();
    
    sprintf(msg, "[%s] GradDiv Gx->Ez: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[0].Ez.SetSymbol(msg);
    GD[0].Ez.Print();
    
    sprintf(msg, "[%s] GradDiv Gy->Ex: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[1].Ex.SetSymbol(msg);
    GD[1].Ex.Print();
    
    sprintf(msg, "[%s] GradDiv Gy->Ey: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[1].Ey.SetSymbol(msg);
    GD[1].Ey.Print();
    
    sprintf(msg, "[%s] GradDiv Gy->Ez: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[1].Ez.SetSymbol(msg);
    GD[1].Ez.Print();
    
    sprintf(msg, "[%s] GradDiv Gz->Ex: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[2].Ex.SetSymbol(msg);
    GD[2].Ex.Print();
    
    sprintf(msg, "[%s] GradDiv Gz->Ey: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[2].Ey.SetSymbol(msg);
    GD[2].Ey.Print();
    
    sprintf(msg, "[%s] GradDiv Gz->Ez: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    GD[2].Ez.SetSymbol(msg);
    GD[2].Ez.Print();

    std::cout << ">>> CurlCurl rows (CCx:Ex/Ey/Ez; CCy:Ex/Ey/Ez; CCz:Ex/Ey/Ez)\n";
    sprintf(msg, "[%s] CurlCurl CCx->Ex: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[0].Ex.SetSymbol(msg);
    CC[0].Ex.Print();
    
    sprintf(msg, "[%s] CurlCurl CCx->Ey: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[0].Ey.SetSymbol(msg);
    CC[0].Ey.Print();
    
    sprintf(msg, "[%s] CurlCurl CCx->Ez: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[0].Ez.SetSymbol(msg);
    CC[0].Ez.Print();
    
    sprintf(msg, "[%s] CurlCurl CCy->Ex: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[1].Ex.SetSymbol(msg);
    CC[1].Ex.Print();
    
    sprintf(msg, "[%s] CurlCurl CCy->Ey: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[1].Ey.SetSymbol(msg);
    CC[1].Ey.Print();
    
    sprintf(msg, "[%s] CurlCurl CCy->Ez: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[1].Ez.SetSymbol(msg);
    CC[1].Ez.Print();
    
    sprintf(msg, "[%s] CurlCurl CCz->Ex: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[2].Ex.SetSymbol(msg);
    CC[2].Ex.Print();
    
    sprintf(msg, "[%s] CurlCurl CCz->Ey: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[2].Ey.SetSymbol(msg);
    CC[2].Ey.Print();
    
    sprintf(msg, "[%s] CurlCurl CCz->Ez: dx=%e, dy=%e, dz=%e", V.name, dx, dy, dz);
    CC[2].Ez.SetSymbol(msg);
    CC[2].Ez.Print();
  }
#else
  (void)dump_stencils_now; // silence unused
#endif

  // Export
  cStencil::cStencilData
    GxEx,GxEy,GxEz, GyEx,GyEy,GyEz, GzEx,GzEy,GzEz,
    LEx,LEy,LEz,
    CCxEx,CCxEy,CCxEz, CCyEx,CCyEy,CCyEz, CCzEx,CCzEy,CCzEz;

  GD[0].Ex.ExportStencil(&GxEx); GD[0].Ey.ExportStencil(&GxEy); GD[0].Ez.ExportStencil(&GxEz);
  GD[1].Ex.ExportStencil(&GyEx); GD[1].Ey.ExportStencil(&GyEy); GD[1].Ez.ExportStencil(&GyEz);
  GD[2].Ex.ExportStencil(&GzEx); GD[2].Ey.ExportStencil(&GzEy); GD[2].Ez.ExportStencil(&GzEz);

  LP.Ex.ExportStencil(&LEx); LP.Ey.ExportStencil(&LEy); LP.Ez.ExportStencil(&LEz);

  CC[0].Ex.ExportStencil(&CCxEx); CC[0].Ey.ExportStencil(&CCxEy); CC[0].Ez.ExportStencil(&CCxEz);
  CC[1].Ex.ExportStencil(&CCyEx); CC[1].Ey.ExportStencil(&CCyEy); CC[1].Ez.ExportStencil(&CCyEz);
  CC[2].Ex.ExportStencil(&CCzEx); CC[2].Ey.ExportStencil(&CCzEy); CC[2].Ez.ExportStencil(&CCzEz);

  // Immediate diagnostics on the exported Laplacian: axis moments
  // Expect sum w≈0, sum k w≈0, sum k^2 w≈2/(d^2) per axis
  std::cout << "  [Axis moments on exported Laplacian]\n";
  Print1DSecondMoments("LEx", LEx, 0, dx);
  Print1DSecondMoments("LEx", LEx, 1, dy);
  Print1DSecondMoments("LEx", LEx, 2, dz);

  // Allocate fields and references
  std::vector<double> Ex(NT),Ey(NT),Ez(NT);
  std::vector<double> CCx(NT),CCy(NT),CCz(NT);
  std::vector<double> GDx(NT),GDy(NT),GDz(NT);
  std::vector<double> LPx(NT),LPy(NT),LPz(NT);
  std::vector<double> CCxA(NT),CCyA(NT),CCzA(NT);
  std::vector<double> GDAx(NT),GDAy(NT),GDAz(NT);
  std::vector<double> LPAx(NT),LPAy(NT),LPAz(NT);
  std::vector<double> IDx(NT),IDy(NT),IDz(NT);

  // Initialize E and analytics
  for (int k=0;k<Nz;++k){
    const double z=k*dz;
    for (int j=0;j<Ny;++j){
      const double y=j*dy;
      for (int i=0;i<Nx;++i){
        const double x=i*dx;
        const size_t id=(size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;
        const Vec3 E = analyticE(x,y,z,a,b,c);
        Ex[id]=E.x; Ey[id]=E.y; Ez[id]=E.z;

        const Vec3 gd=analyticGradDivE(x,y,z,a,b,c);
        const Vec3 lp=analyticLaplacianE(x,y,z,a,b,c);
        const Vec3 cc={gd.x-lp.x, gd.y-lp.y, gd.z-lp.z};

        GDAx[id]=gd.x; GDAy[id]=gd.y; GDAz[id]=gd.z;
        LPAx[id]=lp.x; LPAy[id]=lp.y; LPAz[id]=lp.z;
        CCxA[id]=cc.x; CCyA[id]=cc.y; CCzA[id]=cc.z;
      }
    }
  }

  // Apply operators (NO extra scaling here!)
  for (int k=0;k<Nz;++k){
    for (int j=0;j<Ny;++j){
      for (int i=0;i<Nx;++i){
        const size_t id=(size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;

        // curl_curl
        CCx[id] = apply_exported(CCxEx,Ex,i,j,k,Nx,Ny,Nz)
                + apply_exported(CCxEy,Ey,i,j,k,Nx,Ny,Nz)
                + apply_exported(CCxEz,Ez,i,j,k,Nx,Ny,Nz);
        CCy[id] = apply_exported(CCyEx,Ex,i,j,k,Nx,Ny,Nz)
                + apply_exported(CCyEy,Ey,i,j,k,Nx,Ny,Nz)
                + apply_exported(CCyEz,Ez,i,j,k,Nx,Ny,Nz);
        CCz[id] = apply_exported(CCzEx,Ex,i,j,k,Nx,Ny,Nz)
                + apply_exported(CCzEy,Ey,i,j,k,Nx,Ny,Nz)
                + apply_exported(CCzEz,Ez,i,j,k,Nx,Ny,Nz);

        // grad_div
        GDx[id] = apply_exported(GxEx,Ex,i,j,k,Nx,Ny,Nz)
                + apply_exported(GxEy,Ey,i,j,k,Nx,Ny,Nz)
                + apply_exported(GxEz,Ez,i,j,k,Nx,Ny,Nz);
        GDy[id] = apply_exported(GyEx,Ex,i,j,k,Nx,Ny,Nz)
                + apply_exported(GyEy,Ey,i,j,k,Nx,Ny,Nz)
                + apply_exported(GyEz,Ez,i,j,k,Nx,Ny,Nz);
        GDz[id] = apply_exported(GzEx,Ex,i,j,k,Nx,Ny,Nz)
                + apply_exported(GzEy,Ey,i,j,k,Nx,Ny,Nz)
                + apply_exported(GzEz,Ez,i,j,k,Nx,Ny,Nz);

        // laplacian
        LPx[id] = apply_exported(LEx,Ex,i,j,k,Nx,Ny,Nz);
        LPy[id] = apply_exported(LEy,Ey,i,j,k,Nx,Ny,Nz);
        LPz[id] = apply_exported(LEz,Ez,i,j,k,Nx,Ny,Nz);

        // identity
        IDx[id] = GDx[id] - LPx[id];
        IDy[id] = GDy[id] - LPy[id];
        IDz[id] = GDz[id] - LPz[id];
      }
    }
  }

  // ---------- Scale probes ----------
  double cc_num_dot=0, cc_ref_dot=0;
  double gd_num_dot=0, gd_ref_dot=0;
  double lp_num_dot=0, lp_ref_dot=0;

  for (size_t id=0; id<NT; ++id) {
    cc_num_dot += dot3(CCx[id],CCy[id],CCz[id], CCxA[id],CCyA[id],CCzA[id]);
    cc_ref_dot += dot3(CCxA[id],CCyA[id],CCzA[id], CCxA[id],CCyA[id],CCzA[id]);

    gd_num_dot += dot3(GDx[id],GDy[id],GDz[id], GDAx[id],GDAy[id],GDAz[id]);
    gd_ref_dot += dot3(GDAx[id],GDAy[id],GDAz[id], GDAx[id],GDAy[id],GDAz[id]);

    lp_num_dot += dot3(LPx[id],LPy[id],LPz[id], LPAx[id],LPAy[id],LPAz[id]);
    lp_ref_dot += dot3(LPAx[id],LPAy[id],LPAz[id], LPAx[id],LPAy[id],LPAz[id]);
  }
  auto safe_ratio=[](double a,double b){ return (b!=0.0)? (a/b) : 0.0; };

  ProbeResults PR;
  PR.sCC = safe_ratio(cc_num_dot, cc_ref_dot);
  PR.sGD = safe_ratio(gd_num_dot, gd_ref_dot);
  PR.sLP = safe_ratio(lp_num_dot, lp_ref_dot);

  // ---------- Identity residual norms ----------
  double R_linf=0, R_l2=0, R_l2ref=0;
  for (size_t id=0; id<NT; ++id){
    const double rx = CCx[id]-IDx[id];
    const double ry = CCy[id]-IDy[id];
    const double rz = CCz[id]-IDz[id];
    R_linf = std::max(R_linf, std::max(std::abs(rx), std::max(std::abs(ry), std::abs(rz))));
    R_l2   += rx*rx + ry*ry + rz*rz;
    const double qx=IDx[id], qy=IDy[id], qz=IDz[id];
    R_l2ref += qx*qx + qy*qy + qz*qz;
  }
  PR.R_linf   = R_linf;
  PR.R_relL2  = (R_l2ref>0)? std::sqrt(R_l2/R_l2ref) : std::sqrt(R_l2);

  // ---------- Polynomial Laplacian sanity: ∇²(x²+y²+z²)=6 ----------
  double S_linf=0, S_l2=0, S_l2ref=0;
  {
    std::vector<double> F(NT);
    for (int k=0;k<Nz;++k){
      const double z=k*dz;
      for (int j=0;j<Ny;++j){
        const double y=j*dy;
        for (int i=0;i<Nx;++i){
          const double x=i*dx;
          F[(size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i] = x*x + y*y + z*z;
        }
      }
    }
    for (int k=0;k<Nz;++k){
      for (int j=0;j<Ny;++j){
        for (int i=0;i<Nx;++i){
          const size_t id=(size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;
          const double num = apply_exported(LEx, F, i,j,k, Nx,Ny,Nz); // Laplacian scalar path
          const double ana = 6.0;
          const double err = num - ana;
          S_linf = std::max(S_linf, std::abs(err));
          S_l2 += err*err;
          S_l2ref += ana*ana;
        }
      }
    }
  }
  PR.poly_lap_linf  = S_linf;
  PR.poly_lap_relL2 = (S_l2ref>0)? std::sqrt(S_l2/S_l2ref) : std::sqrt(S_l2);

  return PR;
}

} // anon

namespace CurlCurlMetricsDebug {

int Run(const std::vector<std::string>& args) {
  const double L=1.0;
  const double a=2.0*M_PI, b=3.0*M_PI, c=5.0*M_PI;

  const Variant variants[] = {
    { "2nd-compact",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ SecondOrder::InitGradDivEBStencils_compact(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ SecondOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ SecondOrder::InitCurlCurlEStencils_compact(CC,dx,dy,dz); }
    },
    { "2nd-wide",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ SecondOrder::InitGradDivEBStencils_wide(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ SecondOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ SecondOrder::InitCurlCurlEStencils_wide(CC,dx,dy,dz); }
    },
    { "4th",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ FourthOrder::InitGradDivEBStencils(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ FourthOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ FourthOrder::InitCurlCurlEStencils(CC,dx,dy,dz); }
    },
    { "6th",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ SixthOrder::InitGradDivEBStencils(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ SixthOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ SixthOrder::InitCurlCurlEStencils(CC,dx,dy,dz); }
    },
    { "8th",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ EighthOrder::InitGradDivEBStencils(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ EighthOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ EighthOrder::InitCurlCurlEStencils(CC,dx,dy,dz); }
    }
  };

  // Parse Ns
  std::vector<int> Ns;
  if (args.empty()) Ns = {16,24,32,48,64};
  else {
    Ns.reserve(args.size());
    for (auto&s:args) Ns.push_back(std::max(8, std::stoi(s)));
  }

  std::cout << "\n=== curl_curl metrics debug ===\n"
            << "Scale probes expect ~1 if metrics are correct; ~N^2 indicates extra 1/Δ^2.\n"
            << "Polynomial Laplacian sanity applies ∇² to f=x^2+y^2+z^2 (expect 6).\n";

  for (int N : Ns) {
    const double dx=L/N;
    print_big_sep(N, dx);

    for (size_t iv=0; iv<sizeof(variants)/sizeof(variants[0]); ++iv) {
      // Control which stencils get printed via cStencil::Print() when DEBUG_PRINT_STENCILS=1
      //
      // Examples of dump_now conditions:
      //   - Print only first variant at smallest N (original, minimal output):
      //       const bool dump_now = (DEBUG_PRINT_STENCILS && N==Ns.front() && iv==0);
      //
      //   - Print ALL variants at smallest N (recommended for comparing orders):
      //       const bool dump_now = (DEBUG_PRINT_STENCILS && N==Ns.front());
      //
      //   - Print ALL variants at ALL N values (very verbose, use sparingly):
      //       const bool dump_now = DEBUG_PRINT_STENCILS;
      //
      //   - Print specific variant(s) only, e.g., 4th and 6th order:
      //       const bool dump_now = (DEBUG_PRINT_STENCILS && N==Ns.front() &&
      //                              (std::string(variants[iv].name) == "4th" ||
      //                               std::string(variants[iv].name) == "6th"));
      //
      //   - Print specific variant at specific N, e.g., 2nd-compact at N=32:
      //       const bool dump_now = (DEBUG_PRINT_STENCILS && N==32 && iv==0);
      //
      // Current setting: print all variants at smallest N
      const bool dump_now = (DEBUG_PRINT_STENCILS && N==Ns.front());

      const ProbeResults PR = run_probe_for_variant(variants[iv], N, L, a,b,c, dump_now);

      std::cout << "[" << variants[iv].name << "]\n"
                << "  Scale probes:  s_CC≈" << std::scientific << std::setprecision(6) << PR.sCC
                << "   s_GD≈" << PR.sGD
                << "   s_LP≈" << PR.sLP
                << "   (expect ~1.0)\n"
                << "  Identity R = CC - (GD - LP):  Linf=" << PR.R_linf
                << "   RelL2=" << PR.R_relL2 << "   (should be tiny if consistent)\n"
                << "  Poly Lap sanity (∇²[x²+y²+z²] vs 6):  Linf=" << PR.poly_lap_linf
                << "   RelL2=" << PR.poly_lap_relL2 << "   (should be ~0)\n";
    }
  }
  return 0;
}

} // namespace CurlCurlMetricsDebug

// Register in the harness and provide a force-link shim (so the test is discoverable)
REGISTER_STENCIL_TEST(CurlCurlMetricsDebug,
  "curl_curl_metrics_debug",
  "Diagnostics: scale probes for CC/GD/Lap vs analytic, identity residual, polynomial Laplacian sanity.");

namespace CurlCurlMetricsDebug {
  void ForceLinkAllTests() {}
}

