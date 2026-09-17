/***************************************************************************************
 * curl_curl_e.cpp  (C++11/C++14 compatible)
 * -------------------------------------------------------------------------------------
 * PURPOSE
 *   Validate and cross-verify the discrete operator
 *
 *       curl_curl(E) = ∇×(∇×E) = ∇(∇·E) − ∇²E
 *
 *   at CORNER nodes (collocated stencils) for orders:
 *   • 2nd (compact & wide)  • 4th  • 6th  • 8th
 *
 * WHAT THIS TEST COMPUTES
 *   Numerical operators, all applied to the same periodic vector field E(x,y,z):
 *     1) CC_num    := curl_curl(E)        (built from cCurlCurlEStencil rows)
 *     2) GD_num    := grad_div(E)         (built from cGradDivEStencil rows)
 *     3) Lap_num   := ∇²E                 (built from cLaplacianStencil, per component)
 *     4) ID_num    := GD_num − Lap_num    (pure numerical identity reconstruction)
 *     5) CC_direct := curl(curl(E))       (direct finite-difference curl applied twice, matching stencil order)
 *
 *   Analytic references at each grid point (x,y,z):
 *     • CC_ana   = ∇(∇·E) − ∇²E
 *     • GD_ana   = ∇( (a+b+c) cos(ax)cos(by)cos(cz) )
 *     • Lap_ana  = −(a²+b²+c²) * E    (component-wise)
 *
 * CONVERGENCE SUMMARY
 *   After running across Ns = {16,24,32,48,64}, we print tables with
 *   L∞ and L2 (RMS) errors for both stencil-based and direct curl(curl(E)) methods.
 ***************************************************************************************/

#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <utility>
#include <functional>

#include "test_harness.h"
#include "test_register.h"
#include "test_force_link_all.h"
#include "pic.h"

using namespace PIC::FieldSolver::Electromagnetic::ECSIM::Stencil;

namespace {

inline int wrap(int i, int N){ int r = i % N; return (r<0)? r+N : r; }
struct Vec3 { double x,y,z; };

inline Vec3 analyticE(double x, double y, double z, double a, double b, double c){
  Vec3 v;
  v.x = std::sin(a*x)*std::cos(b*y)*std::cos(c*z);
  v.y = std::cos(a*x)*std::sin(b*y)*std::cos(c*z);
  v.z = std::cos(a*x)*std::cos(b*y)*std::sin(c*z);
  return v;
}

inline Vec3 analyticCurlCurlE(double x,double y,double z,double a,double b,double c){
  const double sx = std::sin(a*x), cx = std::cos(a*x);
  const double sy = std::sin(b*y), cy = std::cos(b*y);
  const double sz = std::sin(c*z), cz = std::cos(c*z);

  const double Ex = sx*cy*cz;
  const double Ey = cx*sy*cz;
  const double Ez = cx*cy*sz;

  const double K   = (a + b + c);
  const double dDiv_dx = K * (-a*sx)*cy*cz;
  const double dDiv_dy = K * cx*(-b*sy)*cz;
  const double dDiv_dz = K * cx*cy*(-c*sz);

  const double lam = (a*a + b*b + c*c);
  const double lapx = -lam * Ex;
  const double lapy = -lam * Ey;
  const double lapz = -lam * Ez;

  return { dDiv_dx - lapx, dDiv_dy - lapy, dDiv_dz - lapz };
}

inline Vec3 analyticGradDivE(double x,double y,double z,double a,double b,double c){
  const double sx = std::sin(a*x), cx = std::cos(a*x);
  const double sy = std::sin(b*y), cy = std::cos(b*y);
  const double sz = std::sin(c*z), cz = std::cos(c*z);
  const double K = (a + b + c);
  return { K * (-a*sx)*cy*cz, K * cx*(-b*sy)*cz, K * cx*cy*(-c*sz) };
}

inline Vec3 analyticLaplacianE(double x,double y,double z,double a,double b,double c){
  const double sx = std::sin(a*x), cx = std::cos(a*x);
  const double sy = std::sin(b*y), cy = std::cos(b*y);
  const double sz = std::sin(c*z), cz = std::cos(c*z);
  const double Ex = sx*cy*cz;
  const double Ey = cx*sy*cz;
  const double Ez = cx*cy*sz;
  const double lam = (a*a + b*b + c*c);
  return { -lam*Ex, -lam*Ey, -lam*Ez };
}

// ---------- Direct numerical curl operators (various orders) ----------
// 2nd order centered
inline Vec3 numericalCurl_2nd(const std::vector<double>& Vx,
                              const std::vector<double>& Vy,
                              const std::vector<double>& Vz,
                              int i, int j, int k,
                              int Nx, int Ny, int Nz,
                              double dx, double dy, double dz)
{
  auto idx = [&](int ii, int jj, int kk) -> size_t {
    return (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
  };

  const double dVz_dy = (Vz[idx(i, wrap(j+1,Ny), k)] - Vz[idx(i, wrap(j-1,Ny), k)]) / (2.0*dy);
  const double dVy_dz = (Vy[idx(i, j, wrap(k+1,Nz))] - Vy[idx(i, j, wrap(k-1,Nz))]) / (2.0*dz);
  const double dVx_dz = (Vx[idx(i, j, wrap(k+1,Nz))] - Vx[idx(i, j, wrap(k-1,Nz))]) / (2.0*dz);
  const double dVz_dx = (Vz[idx(wrap(i+1,Nx), j, k)] - Vz[idx(wrap(i-1,Nx), j, k)]) / (2.0*dx);
  const double dVy_dx = (Vy[idx(wrap(i+1,Nx), j, k)] - Vy[idx(wrap(i-1,Nx), j, k)]) / (2.0*dx);
  const double dVx_dy = (Vx[idx(i, wrap(j+1,Ny), k)] - Vx[idx(i, wrap(j-1,Ny), k)]) / (2.0*dy);

  return { dVz_dy - dVy_dz, dVx_dz - dVz_dx, dVy_dx - dVx_dy };
}

// 4th order centered
inline Vec3 numericalCurl_4th(const std::vector<double>& Vx,
                              const std::vector<double>& Vy,
                              const std::vector<double>& Vz,
                              int i, int j, int k,
                              int Nx, int Ny, int Nz,
                              double dx, double dy, double dz)
{
  auto idx = [&](int ii, int jj, int kk) -> size_t {
    return (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
  };

  // 4th order: f'(x) = [-f(x+2h) + 8f(x+h) - 8f(x-h) + f(x-2h)] / (12h)
  const double dVz_dy = (-Vz[idx(i, wrap(j+2,Ny), k)] + 8.0*Vz[idx(i, wrap(j+1,Ny), k)]
                        -8.0*Vz[idx(i, wrap(j-1,Ny), k)] + Vz[idx(i, wrap(j-2,Ny), k)]) / (12.0*dy);
  const double dVy_dz = (-Vy[idx(i, j, wrap(k+2,Nz))] + 8.0*Vy[idx(i, j, wrap(k+1,Nz))]
                        -8.0*Vy[idx(i, j, wrap(k-1,Nz))] + Vy[idx(i, j, wrap(k-2,Nz))]) / (12.0*dz);
  const double dVx_dz = (-Vx[idx(i, j, wrap(k+2,Nz))] + 8.0*Vx[idx(i, j, wrap(k+1,Nz))]
                        -8.0*Vx[idx(i, j, wrap(k-1,Nz))] + Vx[idx(i, j, wrap(k-2,Nz))]) / (12.0*dz);
  const double dVz_dx = (-Vz[idx(wrap(i+2,Nx), j, k)] + 8.0*Vz[idx(wrap(i+1,Nx), j, k)]
                        -8.0*Vz[idx(wrap(i-1,Nx), j, k)] + Vz[idx(wrap(i-2,Nx), j, k)]) / (12.0*dx);
  const double dVy_dx = (-Vy[idx(wrap(i+2,Nx), j, k)] + 8.0*Vy[idx(wrap(i+1,Nx), j, k)]
                        -8.0*Vy[idx(wrap(i-1,Nx), j, k)] + Vy[idx(wrap(i-2,Nx), j, k)]) / (12.0*dx);
  const double dVx_dy = (-Vx[idx(i, wrap(j+2,Ny), k)] + 8.0*Vx[idx(i, wrap(j+1,Ny), k)]
                        -8.0*Vx[idx(i, wrap(j-1,Ny), k)] + Vx[idx(i, wrap(j-2,Ny), k)]) / (12.0*dy);

  return { dVz_dy - dVy_dz, dVx_dz - dVz_dx, dVy_dx - dVx_dy };
}

// 6th order centered
inline Vec3 numericalCurl_6th(const std::vector<double>& Vx,
                              const std::vector<double>& Vy,
                              const std::vector<double>& Vz,
                              int i, int j, int k,
                              int Nx, int Ny, int Nz,
                              double dx, double dy, double dz)
{
  auto idx = [&](int ii, int jj, int kk) -> size_t {
    return (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
  };

  // 6th order: f'(x) = [f(x+3h) - 9f(x+2h) + 45f(x+h) - 45f(x-h) + 9f(x-2h) - f(x-3h)] / (60h)
  auto deriv6 = [&](const std::vector<double>& V, int i0, int j0, int k0, 
                    int di, int dj, int dk, double dh) -> double {
    return (V[idx(wrap(i0+3*di,Nx), wrap(j0+3*dj,Ny), wrap(k0+3*dk,Nz))]
           -9.0*V[idx(wrap(i0+2*di,Nx), wrap(j0+2*dj,Ny), wrap(k0+2*dk,Nz))]
           +45.0*V[idx(wrap(i0+di,Nx), wrap(j0+dj,Ny), wrap(k0+dk,Nz))]
           -45.0*V[idx(wrap(i0-di,Nx), wrap(j0-dj,Ny), wrap(k0-dk,Nz))]
           +9.0*V[idx(wrap(i0-2*di,Nx), wrap(j0-2*dj,Ny), wrap(k0-2*dk,Nz))]
           -V[idx(wrap(i0-3*di,Nx), wrap(j0-3*dj,Ny), wrap(k0-3*dk,Nz))]) / (60.0*dh);
  };

  const double dVz_dy = deriv6(Vz, i, j, k, 0, 1, 0, dy);
  const double dVy_dz = deriv6(Vy, i, j, k, 0, 0, 1, dz);
  const double dVx_dz = deriv6(Vx, i, j, k, 0, 0, 1, dz);
  const double dVz_dx = deriv6(Vz, i, j, k, 1, 0, 0, dx);
  const double dVy_dx = deriv6(Vy, i, j, k, 1, 0, 0, dx);
  const double dVx_dy = deriv6(Vx, i, j, k, 0, 1, 0, dy);

  return { dVz_dy - dVy_dz, dVx_dz - dVz_dx, dVy_dx - dVx_dy };
}

// 8th order centered
inline Vec3 numericalCurl_8th(const std::vector<double>& Vx,
                              const std::vector<double>& Vy,
                              const std::vector<double>& Vz,
                              int i, int j, int k,
                              int Nx, int Ny, int Nz,
                              double dx, double dy, double dz)
{
  auto idx = [&](int ii, int jj, int kk) -> size_t {
    return (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
  };

  // 8th order: f'(x) = [-f(x+4h) + 8f(x+3h) - 36f(x+2h) + 112f(x+h) - 112f(x-h) + 36f(x-2h) - 8f(x-3h) + f(x-4h)] / (280h)
  auto deriv8 = [&](const std::vector<double>& V, int i0, int j0, int k0, 
                    int di, int dj, int dk, double dh) -> double {
    return (-V[idx(wrap(i0+4*di,Nx), wrap(j0+4*dj,Ny), wrap(k0+4*dk,Nz))]
           +8.0*V[idx(wrap(i0+3*di,Nx), wrap(j0+3*dj,Ny), wrap(k0+3*dk,Nz))]
           -36.0*V[idx(wrap(i0+2*di,Nx), wrap(j0+2*dj,Ny), wrap(k0+2*dk,Nz))]
           +112.0*V[idx(wrap(i0+di,Nx), wrap(j0+dj,Ny), wrap(k0+dk,Nz))]
           -112.0*V[idx(wrap(i0-di,Nx), wrap(j0-dj,Ny), wrap(k0-dk,Nz))]
           +36.0*V[idx(wrap(i0-2*di,Nx), wrap(j0-2*dj,Ny), wrap(k0-2*dk,Nz))]
           -8.0*V[idx(wrap(i0-3*di,Nx), wrap(j0-3*dj,Ny), wrap(k0-3*dk,Nz))]
           +V[idx(wrap(i0-4*di,Nx), wrap(j0-4*dj,Ny), wrap(k0-4*dk,Nz))]) / (280.0*dh);
  };

  const double dVz_dy = deriv8(Vz, i, j, k, 0, 1, 0, dy);
  const double dVy_dz = deriv8(Vy, i, j, k, 0, 0, 1, dz);
  const double dVx_dz = deriv8(Vx, i, j, k, 0, 0, 1, dz);
  const double dVz_dx = deriv8(Vz, i, j, k, 1, 0, 0, dx);
  const double dVy_dx = deriv8(Vy, i, j, k, 1, 0, 0, dx);
  const double dVx_dy = deriv8(Vx, i, j, k, 0, 1, 0, dy);

  return { dVz_dy - dVy_dz, dVx_dz - dVz_dx, dVy_dx - dVx_dy };
}

static inline double apply_exported(const cStencil::cStencilData& S,
                                    const std::vector<double>& F,
                                    int i, int j, int k,
                                    int Nx, int Ny, int Nz)
{
  double acc = 0.0;
  for (int n=0; n<S.Length; ++n) {
    const int ii = wrap(i + S.Data[n].i, Nx);
    const int jj = wrap(j + S.Data[n].j, Ny);
    const int kk = wrap(k + S.Data[n].k, Nz);
    const size_t idx = (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
    acc += S.Data[n].a * F[idx];
  }
  return acc;
}

static void print_point_curlcurl(int N, double L,
                                 const std::vector<double>& CCx,
                                 const std::vector<double>& CCy,
                                 const std::vector<double>& CCz,
                                 const std::vector<double>& CCxA,
                                 const std::vector<double>& CCyA,
                                 const std::vector<double>& CCzA,
                                 const std::vector<double>& IDx,
                                 const std::vector<double>& IDy,
                                 const std::vector<double>& IDz,
                                 const std::vector<double>& CCx_dir,
                                 const std::vector<double>& CCy_dir,
                                 const std::vector<double>& CCz_dir,
                                 const char* flavor_label)
{
  const double dx = L/N;
  const int ii = N/4, jj = N/3, kk = (2*N)/5;
  const int Nx=N, Ny=N, Nz=N;
  const size_t idx = (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
  const double x = ii*dx, y = jj*dx, z = kk*dx;

  auto line = [&](const char* name, double a_, double n_, double n2, double ndir){
    const double err1 = std::abs(n_  - a_);
    const double err2 = std::abs(n2 - a_);
    const double err3 = std::abs(ndir - a_);
    std::cout << "    " << std::left << std::setw(16) << name
              << std::right << std::scientific << std::setprecision(8)
              << std::setw(16) << a_  << "   "
              << std::setw(16) << n_  << "   "
              << std::setw(16) << n2  << "   "
              << std::setw(16) << ndir << "   "
              << std::setprecision(3) << std::setw(10) << err1
              << std::setw(12) << err2
              << std::setw(12) << err3 << "\n";
  };

  std::cout << "\n[" << flavor_label << "] curl_curl(E) @ interior point\n"
            << "  N=" << N << ", (i,j,k)=(" << ii << "," << jj << "," << kk << ")"
            << ", (x,y,z)=(" << std::fixed << std::setprecision(6)
            << x << ", " << y << ", " << z << ")\n"
            << "  ----------------------------------------------------------------------------------------------------------------------------\n"
            << "    Component          Analytic            Num CurlCurl         Num (GD-Lap)        Num (Direct)      |Err(CC)|    |Err(ID)|    |Err(Dir)|\n"
            << "                        CC_ana               CC_num               ID_num              CC_direct      |CC-CC_ana|  |ID-CC_ana|  |Dir-CC_ana|\n"
            << "  ----------------------------------------------------------------------------------------------------------------------------\n";

  line("(CurlCurlE)_x", CCxA[idx], CCx[idx], IDx[idx], CCx_dir[idx]);
  line("(CurlCurlE)_y", CCyA[idx], CCy[idx], IDy[idx], CCy_dir[idx]);
  line("(CurlCurlE)_z", CCzA[idx], CCz[idx], IDz[idx], CCz_dir[idx]);
}

static void print_point_gd(int N, double L,
                           const std::vector<double>& GDx,
                           const std::vector<double>& GDy,
                           const std::vector<double>& GDz,
                           const std::vector<double>& GDAx,
                           const std::vector<double>& GDAy,
                           const std::vector<double>& GDAz,
                           const char* flavor_label)
{
  const double dx = L/N;
  const int ii = N/4, jj = N/3, kk = (2*N)/5;
  const int Nx=N, Ny=N, Nz=N;
  const size_t idx = (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;

  auto line = [&](const char* name, double a_, double n_){
    const double err = std::abs(n_  - a_);
    std::cout << "    " << std::left << std::setw(16) << name
              << std::right << std::scientific << std::setprecision(8)
              << std::setw(16) << a_  << "   "
              << std::setw(16) << n_  << "   "
              << std::setprecision(3) << std::setw(10) << err << "\n";
  };

  std::cout << "\n[" << flavor_label << "] grad_div(E) @ interior point\n"
            << "  --------------------------------------------------------------------------------------------------------------\n"
            << "    Component          Analytic GD         Numerical GD         |Err(GD)|\n"
            << "                         GD_ana              GD_num            |GD-GD_ana|\n"
            << "  --------------------------------------------------------------------------------------------------------------\n";

  line("(GradDivE)_x", GDAx[idx], GDx[idx]);
  line("(GradDivE)_y", GDAy[idx], GDy[idx]);
  line("(GradDivE)_z", GDAz[idx], GDz[idx]);
}

static void print_point_lap(int N, double L,
                            const std::vector<double>& LPx,
                            const std::vector<double>& LPy,
                            const std::vector<double>& LPz,
                            const std::vector<double>& LPAx,
                            const std::vector<double>& LPAy,
                            const std::vector<double>& LPAz,
                            const char* flavor_label)
{
  const double dx = L/N;
  const int ii = N/4, jj = N/3, kk = (2*N)/5;
  const int Nx=N, Ny=N, Nz=N;
  const size_t idx = (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;

  auto line = [&](const char* name, double a_, double n_){
    const double err = std::abs(n_  - a_);
    std::cout << "    " << std::left << std::setw(16) << name
              << std::right << std::scientific << std::setprecision(8)
              << std::setw(16) << a_  << "   "
              << std::setw(16) << n_  << "   "
              << std::setprecision(3) << std::setw(10) << err << "\n";
  };

  std::cout << "\n[" << flavor_label << "] laplacian(E) @ interior point\n"
            << "  --------------------------------------------------------------------------------------------------------------\n"
            << "    Component          Analytic Lap        Numerical Lap        |Err(Lap)|\n"
            << "                         Lap_ana             Lap_num           |Lap-Lap_ana|\n"
            << "  --------------------------------------------------------------------------------------------------------------\n";

  line("(LaplacianE)_x", LPAx[idx], LPx[idx]);
  line("(LaplacianE)_y", LPAy[idx], LPy[idx]);
  line("(LaplacianE)_z", LPAz[idx], LPz[idx]);
}

struct ErrStats { double linf=0.0, l2abs=0.0, l2rel=0.0; };

template<typename BuildGradDiv, typename BuildLap, typename BuildCurlCurl, typename CurlFunc>
ErrStats run_one_order(const char* label,
                       BuildGradDiv build_grad_div,
                       BuildLap     build_lap,
                       BuildCurlCurl build_curl_curl,
                       CurlFunc     curl_func,
                       int N, double L,
                       double a, double b, double c,
                       ErrStats* direct_err = nullptr)
{
  const double dx=L/N, dy=L/N, dz=L/N;
  const int Nx=N, Ny=N, Nz=N;
  const size_t NT = (size_t)Nx*Ny*Nz;

  std::vector<double> Ex(NT), Ey(NT), Ez(NT);
  for (int k=0; k<Nz; ++k){
    const double z=k*dz;
    for (int j=0; j<Ny; ++j){
      const double y=j*dy;
      for (int i=0; i<Nx; ++i){
        const double x=i*dx;
        const Vec3 E = analyticE(x,y,z,a,b,c);
        const size_t id = (size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;
        Ex[id]=E.x; Ey[id]=E.y; Ez[id]=E.z;
      }
    }
  }

  cGradDivEStencil  G[3];
  cLaplacianStencil Ls;
  cCurlCurlEStencil CC[3];

  build_grad_div(G, dx,dy,dz);
  build_lap(&Ls, dx,dy,dz);
  build_curl_curl(CC, dx,dy,dz);

  cStencil::cStencilData
    GxEx,GxEy,GxEz, GyEx,GyEy,GyEz, GzEx,GzEy,GzEz,
    LEx,LEy,LEz,
    CCxEx,CCxEy,CCxEz, CCyEx,CCyEy,CCyEz, CCzEx,CCzEy,CCzEz;

  G[0].Ex.ExportStencil(&GxEx); G[0].Ey.ExportStencil(&GxEy); G[0].Ez.ExportStencil(&GxEz);
  G[1].Ex.ExportStencil(&GyEx); G[1].Ey.ExportStencil(&GyEy); G[1].Ez.ExportStencil(&GyEz);
  G[2].Ex.ExportStencil(&GzEx); G[2].Ey.ExportStencil(&GzEy); G[2].Ez.ExportStencil(&GzEz);

  Ls.Ex.ExportStencil(&LEx); Ls.Ey.ExportStencil(&LEy); Ls.Ez.ExportStencil(&LEz);

  CC[0].Ex.ExportStencil(&CCxEx); CC[0].Ey.ExportStencil(&CCxEy); CC[0].Ez.ExportStencil(&CCxEz);
  CC[1].Ex.ExportStencil(&CCyEx); CC[1].Ey.ExportStencil(&CCyEy); CC[1].Ez.ExportStencil(&CCyEz);
  CC[2].Ex.ExportStencil(&CCzEx); CC[2].Ey.ExportStencil(&CCzEy); CC[2].Ez.ExportStencil(&CCzEz);

  // Compute curl(E) first for direct curl(curl(E))
  std::vector<double> curlEx(NT), curlEy(NT), curlEz(NT);
  for (int k=0; k<Nz; ++k){
    for (int j=0; j<Ny; ++j){
      for (int i=0; i<Nx; ++i){
        const size_t id = (size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;
        Vec3 curl = curl_func(Ex, Ey, Ez, i, j, k, Nx, Ny, Nz, dx, dy, dz);
        curlEx[id] = curl.x;
        curlEy[id] = curl.y;
        curlEz[id] = curl.z;
      }
    }
  }

  std::vector<double> CCx(NT), CCy(NT), CCz(NT);
  std::vector<double> IDx(NT), IDy(NT), IDz(NT);
  std::vector<double> GDx(NT), GDy(NT), GDz(NT);
  std::vector<double> LPx(NT), LPy(NT), LPz(NT);
  std::vector<double> CCxA(NT), CCyA(NT), CCzA(NT);
  std::vector<double> GDAx(NT), GDAy(NT), GDAz(NT);
  std::vector<double> LPAx(NT), LPAy(NT), LPAz(NT);
  std::vector<double> CCx_dir(NT), CCy_dir(NT), CCz_dir(NT);

  double linfA=0.0, l2A=0.0, l2refA=0.0;
  double linfI=0.0, l2I=0.0, l2refI=0.0;
  double linfGD=0.0, l2GD=0.0, l2refGD=0.0;
  double linfLP=0.0, l2LP=0.0, l2refLP=0.0;
  double linfDir=0.0, l2Dir=0.0;

  for (int k=0; k<Nz; ++k){
    const double z=k*dz;
    for (int j=0; j<Ny; ++j){
      const double y=j*dy;
      for (int i=0; i<Nx; ++i){
        const double x=i*dx;
        const size_t id = (size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;

        const double CCx_num =
            apply_exported(CCxEx, Ex,i,j,k,Nx,Ny,Nz)
          + apply_exported(CCxEy, Ey,i,j,k,Nx,Ny,Nz)
          + apply_exported(CCxEz, Ez,i,j,k,Nx,Ny,Nz);
        const double CCy_num =
            apply_exported(CCyEx, Ex,i,j,k,Nx,Ny,Nz)
          + apply_exported(CCyEy, Ey,i,j,k,Nx,Ny,Nz)
          + apply_exported(CCyEz, Ez,i,j,k,Nx,Ny,Nz);
        const double CCz_num =
            apply_exported(CCzEx, Ex,i,j,k,Nx,Ny,Nz)
          + apply_exported(CCzEy, Ey,i,j,k,Nx,Ny,Nz)
          + apply_exported(CCzEz, Ez,i,j,k,Nx,Ny,Nz);
        CCx[id]=CCx_num; CCy[id]=CCy_num; CCz[id]=CCz_num;

        Vec3 curlcurl = curl_func(curlEx, curlEy, curlEz, i, j, k, Nx, Ny, Nz, dx, dy, dz);
        CCx_dir[id] = curlcurl.x;
        CCy_dir[id] = curlcurl.y;
        CCz_dir[id] = curlcurl.z;

        const double GDx_num =
            apply_exported(GxEx, Ex,i,j,k,Nx,Ny,Nz)
          + apply_exported(GxEy, Ey,i,j,k,Nx,Ny,Nz)
          + apply_exported(GxEz, Ez,i,j,k,Nx,Ny,Nz);
        const double GDy_num =
            apply_exported(GyEx, Ex,i,j,k,Nx,Ny,Nz)
          + apply_exported(GyEy, Ey,i,j,k,Nx,Ny,Nz)
          + apply_exported(GyEz, Ez,i,j,k,Nx,Ny,Nz);
        const double GDz_num =
            apply_exported(GzEx, Ex,i,j,k,Nx,Ny,Nz)
          + apply_exported(GzEy, Ey,i,j,k,Nx,Ny,Nz)
          + apply_exported(GzEz, Ez,i,j,k,Nx,Ny,Nz);
        GDx[id]=GDx_num; GDy[id]=GDy_num; GDz[id]=GDz_num;

        const double LPx_num = apply_exported(LEx, Ex,i,j,k,Nx,Ny,Nz);
        const double LPy_num = apply_exported(LEy, Ey,i,j,k,Nx,Ny,Nz);
        const double LPz_num = apply_exported(LEz, Ez,i,j,k,Nx,Ny,Nz);
        LPx[id]=LPx_num; LPy[id]=LPy_num; LPz[id]=LPz_num;

        IDx[id] = GDx_num - LPx_num;
        IDy[id] = GDy_num - LPy_num;
        IDz[id] = GDz_num - LPz_num;

        const Vec3 CCa  = analyticCurlCurlE(x,y,z,a,b,c);
        const Vec3 GDa  = analyticGradDivE (x,y,z,a,b,c);
        const Vec3 LPa  = analyticLaplacianE(x,y,z,a,b,c);
        CCxA[id]=CCa.x; CCyA[id]=CCa.y; CCzA[id]=CCa.z;
        GDAx[id]=GDa.x; GDAy[id]=GDa.y; GDAz[id]=GDa.z;
        LPAx[id]=LPa.x; LPAy[id]=LPa.y; LPAz[id]=LPa.z;

        {
          const double ex = CCx_num-CCa.x, ey = CCy_num-CCa.y, ez = CCz_num-CCa.z;
          linfA = std::max(linfA, std::max(std::abs(ex), std::max(std::abs(ey), std::abs(ez))));
          l2A  += ex*ex + ey*ey + ez*ez;
          l2refA += CCa.x*CCa.x + CCa.y*CCa.y + CCa.z*CCa.z;
        }
        {
          const double ex = curlcurl.x-CCa.x, ey = curlcurl.y-CCa.y, ez = curlcurl.z-CCa.z;
          linfDir = std::max(linfDir, std::max(std::abs(ex), std::max(std::abs(ey), std::abs(ez))));
          l2Dir += ex*ex + ey*ey + ez*ez;
        }
        {
          const double ex = CCx_num-IDx[id], ey = CCy_num-IDy[id], ez = CCz_num-IDz[id];
          linfI = std::max(linfI, std::max(std::abs(ex), std::max(std::abs(ey), std::abs(ez))));
          l2I  += ex*ex + ey*ey + ez*ez;
          const double rx = IDx[id], ry = IDy[id], rz = IDz[id];
          l2refI += rx*rx + ry*ry + rz*rz;
        }
        {
          const double ex = GDx_num-GDa.x, ey = GDy_num-GDa.y, ez = GDz_num-GDa.z;
          linfGD = std::max(linfGD, std::max(std::abs(ex), std::max(std::abs(ey), std::abs(ez))));
          l2GD  += ex*ex + ey*ey + ez*ez;
          l2refGD += GDa.x*GDa.x + GDa.y*GDa.y + GDa.z*GDa.z;
        }
        {
          const double ex = LPx_num-LPa.x, ey = LPy_num-LPa.y, ez = LPz_num-LPa.z;
          linfLP = std::max(linfLP, std::max(std::abs(ex), std::max(std::abs(ey), std::abs(ez))));
          l2LP  += ex*ex + ey*ey + ez*ez;
          l2refLP += LPa.x*LPa.x + LPa.y*LPa.y + LPa.z*LPa.z;
        }
      }
    }
  }

  const double relL2A  = (l2refA  > 0.0) ? std::sqrt(l2A  / l2refA ) : std::sqrt(l2A);
  const double relL2I  = (l2refI  > 0.0) ? std::sqrt(l2I  / l2refI ) : std::sqrt(l2I);
  const double relL2GD = (l2refGD > 0.0) ? std::sqrt(l2GD / l2refGD) : std::sqrt(l2GD);
  const double relL2LP = (l2refLP > 0.0) ? std::sqrt(l2LP / l2refLP) : std::sqrt(l2LP);

  const double l2absA = std::sqrt(l2A / double(NT));
  const double l2absDir = std::sqrt(l2Dir / double(NT));

  std::cout << "  " << std::left << std::setw(12) << label
            << " CC vs analytic:   Linf=" << std::scientific << std::setprecision(3) << linfA
            << "   RelL2=" << relL2A << "\n";
  std::cout << "  " << std::left << std::setw(12) << ""
            << " CC vs (GD-Lap):   Linf=" << linfI
            << "   RelL2=" << relL2I << "\n";
  std::cout << "  " << std::left << std::setw(12) << ""
            << " Direct CC vs ana: Linf=" << linfDir
            << "   L2(RMS)=" << l2absDir << "\n";
  std::cout << "  " << std::left << std::setw(12) << ""
            << " GD vs analytic:   Linf=" << linfGD
            << "   RelL2=" << relL2GD << "\n";
  std::cout << "  " << std::left << std::setw(12) << ""
            << " Lap vs analytic:  Linf=" << linfLP
            << "   RelL2=" << relL2LP << "\n";

  print_point_curlcurl(N, L, CCx, CCy, CCz, CCxA, CCyA, CCzA, IDx, IDy, IDz, 
                       CCx_dir, CCy_dir, CCz_dir, label);
  print_point_gd(N, L, GDx, GDy, GDz, GDAx, GDAy, GDAz, label);
  print_point_lap(N, L, LPx, LPy, LPz, LPAx, LPAy, LPAz, label);

  ErrStats s; s.linf = linfA; s.l2abs = l2absA; s.l2rel = relL2A;
  
  if (direct_err) {
    direct_err->linf = linfDir;
    direct_err->l2abs = l2absDir;
    direct_err->l2rel = 0.0;
  }
  
  return s;
}

} // anon

namespace CurlCurlE {

struct Variant {
  const char* name;
  std::function<void(cGradDivEStencil*, double, double, double)>   build_grad_div;
  std::function<void(cLaplacianStencil*, double, double, double)>  build_lap;
  std::function<void(cCurlCurlEStencil*, double, double, double)>  build_curl_curl;
  std::function<Vec3(const std::vector<double>&, const std::vector<double>&, const std::vector<double>&,
                     int, int, int, int, int, int, double, double, double)> curl_func;
};

int Run(const std::vector<std::string>&) {
  const double L = 1.0;
  const double a = 2.0*M_PI, b = 3.0*M_PI, c = 5.0*M_PI;

  const Variant variants[] = {
    { "2nd-compact",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ SecondOrder::InitGradDivEBStencils_compact(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ SecondOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ SecondOrder::InitCurlCurlEStencils_compact(CC,dx,dy,dz); },
      numericalCurl_2nd
    },
    { "2nd-wide",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ SecondOrder::InitGradDivEBStencils_wide(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ SecondOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ SecondOrder::InitCurlCurlEStencils_wide(CC,dx,dy,dz); },
      numericalCurl_2nd
    },
    { "4th",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ FourthOrder::InitGradDivEBStencils(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ FourthOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ FourthOrder::InitCurlCurlEStencils(CC,dx,dy,dz); },
      numericalCurl_4th
    },
    { "6th",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ SixthOrder::InitGradDivEBStencils(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ SixthOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ SixthOrder::InitCurlCurlEStencils(CC,dx,dy,dz); },
      numericalCurl_6th
    },
    { "8th",
      [](cGradDivEStencil G[3], double dx,double dy,double dz){ EighthOrder::InitGradDivEBStencils(G,dx,dy,dz); },
      [](cLaplacianStencil* Ls, double dx,double dy,double dz){ EighthOrder::InitLaplacianStencil(Ls,dx,dy,dz); },
      [](cCurlCurlEStencil CC[3], double dx,double dy,double dz){ EighthOrder::InitCurlCurlEStencils(CC,dx,dy,dz); },
      numericalCurl_8th
    }
  };

  std::vector<int> Ns = {16, 24, 32, 48, 64};

  std::cout << "\n=== Corner curl_curl(E): analytic vs numerical, identity, and GD/Lap verification ===\n"
            << "Domain L=" << L << ", wavenumbers a=2π, b=3π, c=5π\n";

  struct Row { double linf=0.0, l2=0.0; };
  std::vector<Row> second_rows, second_direct_rows;
  std::vector<Row> fourth_rows, fourth_direct_rows;
  std::vector<Row> sixth_rows, sixth_direct_rows;
  std::vector<Row> eighth_rows, eighth_direct_rows;
  
  auto push_row = [](std::vector<Row>& vec, const ErrStats& s){
    Row r; r.linf = s.linf; r.l2 = s.l2abs; vec.push_back(r);
  };

  for (size_t t = 0; t < Ns.size(); ++t) {
    const int N = Ns[t];

    std::cout << "\n======== N = " << N << " ============================================================\n";

    ErrStats s2c, s2c_dir, s4, s4_dir, s6, s6_dir, s8, s8_dir;

    for (size_t v=0; v<sizeof(variants)/sizeof(variants[0]); ++v) {
      ErrStats direct_stats;
      ErrStats res = run_one_order(variants[v].name,
                                   variants[v].build_grad_div,
                                   variants[v].build_lap,
                                   variants[v].build_curl_curl,
                                   variants[v].curl_func,
                                   N, L, a, b, c,
                                   &direct_stats);

      const std::string name = variants[v].name;
      if      (name == "2nd-compact") { s2c = res; s2c_dir = direct_stats; }
      else if (name == "4th")         { s4  = res; s4_dir = direct_stats; }
      else if (name == "6th")         { s6  = res; s6_dir = direct_stats; }
      else if (name == "8th")         { s8  = res; s8_dir = direct_stats; }
    }

    push_row(second_rows, s2c);
    push_row(second_direct_rows, s2c_dir);
    push_row(fourth_rows, s4);
    push_row(fourth_direct_rows, s4_dir);
    push_row(sixth_rows, s6);
    push_row(sixth_direct_rows, s6_dir);
    push_row(eighth_rows, s8);
    push_row(eighth_direct_rows, s8_dir);

    std::cout << "=====================================================================================\n";
  }

  auto safe_ord = [](double e_prev, double e_curr, int N_prev, int N_curr)->double{
    if (e_prev<=0.0 || e_curr<=0.0) return 0.0;
    const double rN = double(N_curr)/double(N_prev);
    return std::log(e_prev/e_curr) / std::log(rN);
  };

  auto print_convergence_table = [&](const char* title,
                                     const std::vector<Row>& rows,
                                     const std::vector<Row>& direct_rows,
                                     const char* order_label) {
    std::cout << "\n=== " << title << " ===\n";
    std::cout << "-------------------------------------------------------------------------\n"
              << "   N  |  Stencil (L_inf)  Ord   Stencil (L2)    Ord  |"
              << "  Direct (L_inf)  Ord   Direct (L2)     Ord  |\n"
              << "-------------------------------------------------------------------------\n";
    
    for (size_t i=0; i<Ns.size(); ++i) {
      const int N = Ns[i];
      const Row& rs = rows[i];
      const Row& rd = direct_rows[i];
      
      double os_inf=0, os_l2=0, od_inf=0, od_l2=0;
      if (i>0) {
        os_inf = safe_ord(rows[i-1].linf, rs.linf, Ns[i-1], N);
        os_l2  = safe_ord(rows[i-1].l2,   rs.l2,   Ns[i-1], N);
        od_inf = safe_ord(direct_rows[i-1].linf, rd.linf, Ns[i-1], N);
        od_l2  = safe_ord(direct_rows[i-1].l2,   rd.l2,   Ns[i-1], N);
      }
      
      std::cout << std::setw(5) << N << " | "
                << std::scientific << std::setprecision(3)
                << std::setw(13) << rs.linf << " "
                << std::fixed << std::setprecision(2) << std::setw(4) << os_inf << "   "
                << std::scientific << std::setprecision(3) << std::setw(12) << rs.l2 << "   "
                << std::fixed << std::setprecision(2) << std::setw(5) << os_l2 << " | "
                << std::scientific << std::setprecision(3) << std::setw(13) << rd.linf << " "
                << std::fixed << std::setprecision(2) << std::setw(4) << od_inf << "   "
                << std::scientific << std::setprecision(3) << std::setw(12) << rd.l2 << "   "
                << std::fixed << std::setprecision(2) << std::setw(5) << od_l2 << " |\n";
    }
    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << "Order: " << order_label << "\n";
  };

  print_convergence_table("CONVERGENCE: 2nd Order (Compact)",
                         second_rows, second_direct_rows, "2nd");
  print_convergence_table("CONVERGENCE: 4th Order",
                         fourth_rows, fourth_direct_rows, "4th");
  print_convergence_table("CONVERGENCE: 6th Order",
                         sixth_rows, sixth_direct_rows, "6th");
  print_convergence_table("CONVERGENCE: 8th Order",
                         eighth_rows, eighth_direct_rows, "8th");

  return 0;
}

} // namespace CurlCurlE

REGISTER_STENCIL_TEST(CurlCurlE,
  "curl_curl_e",
  "Direct curl_curl(E) vs analytic and (grad_div−laplacian); also verify GD and Lap vs analytic.");

namespace CurlCurlE {
  void ForceLinkAllTests() {}
}
