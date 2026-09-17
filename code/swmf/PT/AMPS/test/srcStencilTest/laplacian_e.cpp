/**
 * @file    laplacian_e.cpp
 * @brief   Convergence test for component-wise Laplacian(E) stencils
 *          at 2nd, 4th, 6th, and 8th order on a periodic, uniform grid.
 *
 * MODIFIED: Data sampled at CELL CORNERS, Laplacian computed at CELL CORNERS
 *
 * Analytic field (periodic, cell-corner sampling):
 *   Ex = sin(ax) cos(by) cos(cz)
 *   Ey = cos(ax) sin(by) cos(cz)
 *   Ez = cos(ax) cos(by) sin(cz)
 *   ∇²E = - (a^2 + b^2 + c^2) * E   (component-wise)
 *
 * What this does:
 *   • Builds ECSIM Laplacian stencils (component-wise) for 2/4/6/8 order.
 *   • Samples field at CELL CORNERS (i*dx, j*dy, k*dz) - no +0.5 offset
 *   • Exports integerized taps and applies them with periodic wrap.
 *   • Reports L_inf and relative L2 errors vs. resolution and estimates order.
 *   • Prints a component-wise numeric vs. analytic comparison at one interior point.
 */

#include <cmath>
#include <vector>
#include <string>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "test_register.h"
#include "test_harness.h"
#include "test_force_link_all.h"
#include "pic.h"  // cStencil, cCurlBStencil, ExportStencil API (and stencil builders)

using namespace PIC::FieldSolver::Electromagnetic::ECSIM::Stencil;

// ---------------------- small utils ----------------------
static inline int wrap(int idx, int N){ int r = idx % N; if (r<0) r += N; return r; }

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

struct Vec3 { double x,y,z; };

static inline Vec3 analyticE(double x,double y,double z,double a,double b,double c) {
  return { std::sin(a*x)*std::cos(b*y)*std::cos(c*z),
           std::cos(a*x)*std::sin(b*y)*std::cos(c*z),
           std::cos(a*x)*std::cos(b*y)*std::sin(c*z) };
}

static inline Vec3 analyticLapE(double x,double y,double z,double a,double b,double c) {
  const double k2 = a*a + b*b + c*c;
  Vec3 e = analyticE(x,y,z,a,b,c);
  return { -k2*e.x, -k2*e.y, -k2*e.z };
}

// ---------------------------- driver helpers ---------------------------------
enum class Order { O2, O4, O6, O8 };

static const char* order_name(Order o) {
  switch(o){ case Order::O2: return "Second"; case Order::O4: return "Fourth";
             case Order::O6: return "Sixth"; case Order::O8: return "Eighth";}
  return "Unknown";
}

static void build_laplacian(Order o, cLaplacianStencil* L, double dx,double dy,double dz) {
  switch(o){
    case Order::O2: SecondOrder ::InitLaplacianStencil(L, dx,dy,dz); break;
    case Order::O4: FourthOrder ::InitLaplacianStencil(L, dx,dy,dz); break;
    case Order::O6: SixthOrder  ::InitLaplacianStencil(L, dx,dy,dz); break;
    case Order::O8: EighthOrder ::InitLaplacianStencil(L, dx,dy,dz); break;
  }
}

// Run a single order at resolution N, return Linf/L2
struct Err { double linf, l2; };

static Err run_one(Order ord, int N,
                   double Lx,double Ly,double Lz,
                   double a,double b,double c,
                   // optional for interior print
                   std::vector<double>* outLx=nullptr,
                   std::vector<double>* outLy=nullptr,
                   std::vector<double>* outLz=nullptr,
                   std::vector<double>* outAx=nullptr,
                   std::vector<double>* outAy=nullptr,
                   std::vector<double>* outAz=nullptr)
{
  const int Nx=N, Ny=N, Nz=N;
  const size_t NT = (size_t)Nx*Ny*Nz;
  const double dx=Lx/Nx, dy=Ly/Ny, dz=Lz/Nz;

  // Build Laplacian(E) component-wise stencils and export
  cLaplacianStencil Lap;
  build_laplacian(ord, &Lap, dx,dy,dz);

  cStencil::cStencilData SEx, SEy, SEz;
  Lap.Ex.ExportStencil(&SEx);
  Lap.Ey.ExportStencil(&SEy);
  Lap.Ez.ExportStencil(&SEz);

  // MODIFIED: Cell-CORNER grids (no +0.5 offset)
  std::vector<double> Ex(NT), Ey(NT), Ez(NT);
  std::vector<double> LxN(NT), LyN(NT), LzN(NT);
  std::vector<double> LxA(NT), LyA(NT), LzA(NT);

  for (int k=0;k<Nz;++k)
    for (int j=0;j<Ny;++j)
      for (int i=0;i<Nx;++i) {
        const size_t id = (size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;
        // CELL CORNER: no +0.5 offset
        const double x=i*dx, y=j*dy, z=k*dz;
        const Vec3 e  = analyticE(x,y,z, a,b,c);
        const Vec3 la = analyticLapE(x,y,z, a,b,c);
        Ex[id]=e.x; Ey[id]=e.y; Ez[id]=e.z;
        LxA[id]=la.x; LyA[id]=la.y; LzA[id]=la.z;
      }

  // Apply numeric Laplacian (periodic) with exported taps at cell corners
  for (int k=0;k<Nz;++k)
    for (int j=0;j<Ny;++j)
      for (int i=0;i<Nx;++i) {
        const size_t id = (size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;
        LxN[id] = apply_exported(SEx, Ex, i,j,k, Nx,Ny,Nz);
        LyN[id] = apply_exported(SEy, Ey, i,j,k, Nx,Ny,Nz);
        LzN[id] = apply_exported(SEz, Ez, i,j,k, Nx,Ny,Nz);
      }

  // Norms (vector combined)
  double linf=0.0, l2num=0.0, l2den=0.0;
  for (size_t t=0;t<NT;++t) {
    const double ex = LxN[t]-LxA[t];
    const double ey = LyN[t]-LyA[t];
    const double ez = LzN[t]-LzA[t];
    linf = std::max(linf, std::max(std::abs(ex), std::max(std::abs(ey), std::abs(ez))));
    l2num += ex*ex + ey*ey + ez*ez;
    l2den += LxA[t]*LxA[t] + LyA[t]*LyA[t] + LzA[t]*LzA[t];
  }

  if (outLx) { *outLx = LxN; *outLy = LyN; *outLz = LzN; }
  if (outAx) { *outAx = LxA; *outAy = LyA; *outAz = LzA; }

  Err e;
  e.linf = linf;
  e.l2   = (l2den>0.0) ? std::sqrt(l2num/l2den) : std::sqrt(l2num);
  return e;
}

static void print_point_compare(int N,double Lx,double Ly,double Lz,
                                const std::vector<double>& LxN,
                                const std::vector<double>& LyN,
                                const std::vector<double>& LzN,
                                const std::vector<double>& LxA,
                                const std::vector<double>& LyA,
                                const std::vector<double>& LzA,
                                const char* label)
{
  const int Nx=N, Ny=N, Nz=N;
  const double dx=Lx/Nx, dy=Ly/Ny, dz=Lz/Nz;
  const int ii=Nx/2, jj=Ny/2, kk=Nz/2;
  const size_t id=(size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
  // CELL CORNER: no +0.5 offset
  const double x=ii*dx, y=jj*dy, z=kk*dz;

  auto line = [&](const char* name, double a_, double n_){
    const double err = std::abs(n_ - a_);
    std::cout << "    " << std::left << std::setw(14) << name
              << std::right << std::scientific << std::setprecision(8)
              << std::setw(16) << a_ << "   "
              << std::setw(16) << n_ << "   "
              << std::setprecision(3) << std::setw(8) << err << "\n";
  };

  std::cout << "\n[" << label << "] Component-wise comparison at interior CORNER point:\n"
            << "  Grid: N="<<N<<", (i,j,k)=("<<ii<<","<<jj<<","<<kk<<")"
            << ", (x,y,z)=("<<std::fixed<<std::setprecision(6)
            << x<<", "<<y<<", "<<z<<") [CORNER]\n"
            << "  -----------------------------------------------------------------------------\n"
            << "    Component         Analytic               Numerical               AbsErr\n"
            << "  -----------------------------------------------------------------------------\n";

  line("(LapE)_x", LxA[id], LxN[id]);
  line("(LapE)_y", LyA[id], LyN[id]);
  line("(LapE)_z", LzA[id], LzN[id]);
}

// --------------------------------- ENTRY ------------------------------------
namespace LaplacianE {

static int Run(const std::vector<std::string>&) {
  const double Lx=1.0, Ly=1.0, Lz=1.0;
  const double a=2.0*M_PI, b=2.0*M_PI, c=2.0*M_PI;
  std::vector<int> Ns = {16, 24, 32, 48, 64};

  const Order orders[] = { Order::O2, Order::O4, Order::O6, Order::O8 };

  struct Row { double linf, l2; };
  std::vector<Row> err[4];
  for (int o=0;o<4;++o) err[o].resize(Ns.size());

  std::cout << "\n=== Laplacian(E) Convergence - CELL CORNER VERSION (2nd, 4th, 6th, 8th) ===\n"
            << "Domain Lx="<<Lx<<", Ly="<<Ly<<", Lz="<<Lz
            << "  wavenumbers a="<<a<<", b="<<b<<", c="<<c<<"\n"
            << "Data sampled at CELL CORNERS (i*dx, j*dy, k*dz)\n";

  for (size_t t=0;t<Ns.size();++t) {
    const int N = Ns[t];
    std::cout << "\nN = " << N << "\n";

    for (int o=0;o<4;++o) {
      std::vector<double> LxN,LyN,LzN, LxA,LyA,LzA;
      Err e = run_one(orders[o], N, Lx,Ly,Lz, a,b,c, &LxN,&LyN,&LzN, &LxA,&LyA,&LzA);
      err[o][t] = Row{e.linf, e.l2};

      std::cout << "  " << std::left << std::setw(10) << order_name(orders[o])
                << " Linf=" << std::scientific << std::setprecision(3) << e.linf
                << "   L2=" << e.l2 << std::fixed << "\n";

      print_point_compare(N, Lx,Ly,Lz, LxN,LyN,LzN, LxA,LyA,LzA, order_name(orders[o]));
    }
  }

  // combined table
  std::cout << "\n--------------------------------------------------------------------------------------------------------------\n";
  std::cout << "   N  | "
            << " Second (L_inf)  Ord   Second (L2)     Ord  |"
            << "  Fourth (L_inf)  Ord   Fourth (L2)     Ord  |"
            << "   Sixth (L_inf)  Ord    Sixth (L2)     Ord  |"
            << "  Eighth (L_inf)  Ord   Eighth (L2)     Ord\n";
  std::cout << "--------------------------------------------------------------------------------------------------------------\n";

  for (size_t t=0;t<Ns.size();++t) {
    std::cout << std::setw(5) << Ns[t] << " |";
    for (int o=0;o<4;++o) {
      double p_inf=0.0, p_l2=0.0;
      if (t>0) {
        const double rN = double(Ns[t]) / double(Ns[t-1]);
        const double den = std::log(rN);
        const double num_inf = err[o][t-1].linf / err[o][t].linf;
        const double num_l2  = err[o][t-1].l2   / err[o][t].l2;
        if (den>0 && num_inf>0) p_inf = std::log(num_inf)/den;
        if (den>0 && num_l2 >0) p_l2  = std::log(num_l2 )/den;
      }
      std::cout << " "
        << std::scientific << std::setprecision(3) << std::setw(12) << err[o][t].linf << " "
        << std::fixed      << std::setprecision(2) << std::setw(5)  << (t? p_inf:0.0) << "  "
        << std::scientific << std::setprecision(3) << std::setw(12) << err[o][t].l2   << " "
        << std::fixed      << std::setprecision(2) << std::setw(5)  << (t? p_l2 :0.0) << " |";
    }
    std::cout << "\n";
  }
  std::cout << "--------------------------------------------------------------------------------------------------------------\n";
  return 0;
}

} // namespace LaplacianE

// ---- Registration & force-link shim ----
namespace LaplacianE { int Run(const std::vector<std::string>&); }
REGISTER_STENCIL_TEST(LaplacianE,
  "laplacian_e_corner",
  "Laplacian(E): component-wise stencils at CELL CORNERS (2nd/4th/6th/8th), periodic convergence.");
namespace LaplacianE { void ForceLinkAllTests() {} }
