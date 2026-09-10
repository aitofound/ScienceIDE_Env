/**
 * @file    grad_div_e_corner.cpp
 * @brief   Convergence test for ∇(∇·E) stencils
 *          (Second/compact, Second/wide, Fourth, Sixth, Eighth).
 *
 * MODIFIED: Data sampled at CELL CORNERS, ∇(∇·E) computed at CELL CORNERS
 *
 * Analytic field (periodic, cell-corner sampling):
 *   Ex = sin(ax) cos(by) cos(cz)
 *   Ey = cos(ax) sin(by) cos(cz)
 *   Ez = cos(ax) cos(by) sin(cz)
 *   ∇(∇·E) =
 *     [ -a(a+b+c) sin(ax) cos(by) cos(cz),
 *       -b(a+b+c) cos(ax) sin(by) cos(cz),
 *       -c(a+b+c) cos(ax) cos(by) sin(cz) ].
 *
 * Notes:
 *   • This test applies exported integerized stencils on a periodic grid.
 *   • Data sampled at CELL CORNERS (i*dx, j*dy, k*dz) - no +0.5 offset
 *   • Operator output also at CELL CORNERS
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <iomanip>
#include <iostream>
#include <fstream>

#include "pic.h"
#include "test_register.h"
#include "test_harness.h"
using namespace PIC::FieldSolver::Electromagnetic::ECSIM::Stencil;

// ---------- Analytic field and ∇(∇·E) ----------
struct Vec3 { double x,y,z; };

static inline Vec3 analyticE(double x, double y, double z, double a, double b, double c) {
  return { std::sin(a*x)*std::cos(b*y)*std::cos(c*z),
           std::cos(a*x)*std::sin(b*y)*std::cos(c*z),
           std::cos(a*x)*std::cos(b*y)*std::sin(c*z) };
}

static inline Vec3 analyticGradDivE(double x, double y, double z, double a, double b, double c) {
  const double s = (a + b + c);
  return { -a*s*std::sin(a*x)*std::cos(b*y)*std::cos(c*z),
           -b*s*std::cos(a*x)*std::sin(b*y)*std::cos(c*z),
           -c*s*std::cos(a*x)*std::cos(b*y)*std::sin(c*z) };
}

// ---------- Utility: periodic wrap ----------
static inline int wrap(int idx, int N) { int r = idx % N; if (r < 0) r += N; return r; }

// ---------- Apply a single exported stencil (integerized taps) ----------
static inline double apply_exported(const cStencil::cStencilData& S,
                                    const std::vector<double>& F,
                                    int i, int j, int k, int Nx, int Ny, int Nz)
{
  double acc = 0.0;
  for (int n = 0; n < S.Length; ++n) {
    const int ii = wrap(i + S.Data[n].i, Nx);
    const int jj = wrap(j + S.Data[n].j, Ny);
    const int kk = wrap(k + S.Data[n].k, Nz);
    const size_t idx = (size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
    acc += S.Data[n].a * F[idx];
  }
  return acc;
}

// ---------- TECplot export (Ordered 3D, BLOCK packing) ----------
static void write_tecplot_block(
    const std::string& filename,
    int Nx, int Ny, int Nz,
    double dx, double dy, double dz,
    const std::vector<double>& Ex,
    const std::vector<double>& Ey,
    const std::vector<double>& Ez,
    const std::vector<double>& Gx_num,
    const std::vector<double>& Gy_num,
    const std::vector<double>& Gz_num,
    const std::vector<double>& Gx_ana,
    const std::vector<double>& Gy_ana,
    const std::vector<double>& Gz_ana)
{
  std::ofstream out(filename.c_str());
  if (!out) { std::cerr << "ERROR: cannot open Tecplot file: " << filename << "\n"; return; }

  out << "TITLE = \"GradDivE test - CELL CORNERS\"\n";
  out << "VARIABLES = \"x\" \"y\" \"z\" "
         "\"Ex\" \"Ey\" \"Ez\" "
         "\"Gx_num\" \"Gy_num\" \"Gz_num\" "
         "\"Gx_ana\" \"Gy_ana\" \"Gz_ana\" "
         "\"Err_mag\"\n";
  out << "ZONE T=\"volume\", I=" << Nx << ", J=" << Ny << ", K=" << Nz
      << ", DATAPACKING=BLOCK\n";

  auto write_block = [&](auto get_value) {
    out.setf(std::ios::scientific); out << std::setprecision(8);
    for (int k=0; k<Nz; ++k)
      for (int j=0; j<Ny; ++j)
        for (int i=0; i<Nx; ++i)
          out << get_value(i,j,k) << "\n";
  };

  // CELL CORNERS: no +0.5 offset
  write_block([&](int i,int,  int){ return i*dx; });
  write_block([&](int,  int j,int){ return j*dy; });
  write_block([&](int,  int,  int k){ return k*dz; });

  // fields
  size_t NT = (size_t)Nx*Ny*Nz;
  auto dump = [&](const std::vector<double>& A){ for(size_t t=0;t<NT;++t) out<<A[t]<<"\n"; };
  dump(Ex); dump(Ey); dump(Ez);
  dump(Gx_num); dump(Gy_num); dump(Gz_num);
  dump(Gx_ana); dump(Gy_ana); dump(Gz_ana);

  // error magnitude
  for (size_t t=0; t<NT; ++t) {
    double ex = Gx_num[t]-Gx_ana[t];
    double ey = Gy_num[t]-Gy_ana[t];
    double ez = Gz_num[t]-Gz_ana[t];
    out << std::sqrt(ex*ex + ey*ey + ez*ez) << "\n";
  }
}

// ---------- Driver: evaluate one operator flavor and compute errors ----------
enum class OpFlavor { SecondCompact, SecondWide, FourthOrder, SixthOrder, EighthOrder };

static void build_stencil_rows(OpFlavor f, cGradDivEStencil S[3], double dx, double dy, double dz) {
  switch (f) {
    case OpFlavor::SecondCompact: SecondOrder::InitGradDivEBStencils_compact(S, dx, dy, dz); break;
    case OpFlavor::SecondWide:    SecondOrder::InitGradDivEBStencils_wide(S,    dx, dy, dz); break;
    case OpFlavor::FourthOrder:   FourthOrder::InitGradDivEBStencils(S,         dx, dy, dz); break;
    case OpFlavor::SixthOrder:    SixthOrder::InitGradDivEBStencils(S,          dx, dy, dz); break;
    case OpFlavor::EighthOrder:   EighthOrder::InitGradDivEBStencils(S,         dx, dy, dz); break;
  }
}

static std::string flavor_name(OpFlavor f) {
  switch (f) {
    case OpFlavor::SecondCompact: return "SecondOrder_Compact";
    case OpFlavor::SecondWide:    return "SecondOrder_Wide";
    case OpFlavor::FourthOrder:   return "FourthOrder";
    case OpFlavor::SixthOrder:    return "SixthOrder";
    case OpFlavor::EighthOrder:   return "EighthOrder";
  }
  return "Unknown";
}

struct ErrStats { double linf, l2; };

static ErrStats run_case(OpFlavor flavor, int N, double Lx, double Ly, double Lz,
                         double a, double b, double c,
                         // Optional outputs
                         std::vector<double>* out_Gx = nullptr,
                         std::vector<double>* out_Gy = nullptr,
                         std::vector<double>* out_Gz = nullptr,
                         std::vector<double>* out_Gx_true = nullptr,
                         std::vector<double>* out_Gy_true = nullptr,
                         std::vector<double>* out_Gz_true = nullptr,
                         bool tecplot_dump = false)
{
  const int Nx=N, Ny=N, Nz=N;
  const double dx=Lx/Nx, dy=Ly/Ny, dz=Lz/Nz;

  // Build stencils
  cGradDivEStencil S[3];
  build_stencil_rows(flavor, S, dx, dy, dz);

  // Export stencils once per row/component (public API)
  cStencil::cStencilData SX[3], SY[3], SZ[3];
  for (int r=0; r<3; ++r) {
    S[r].Ex.ExportStencil(&SX[r]);
    S[r].Ey.ExportStencil(&SY[r]);
    S[r].Ez.ExportStencil(&SZ[r]);
  }

  // Fields
  const size_t NT = (size_t)Nx*Ny*Nz;
  std::vector<double> Ex(NT), Ey(NT), Ez(NT);
  std::vector<double> Gx(NT), Gy(NT), Gz(NT);
  std::vector<double> Gx_true(NT), Gy_true(NT), Gz_true(NT);

  // MODIFIED: Fill E and analytic ∇(∇·E) at CELL CORNERS
  for (int k=0; k<Nz; ++k)
    for (int j=0; j<Ny; ++j)
      for (int i=0; i<Nx; ++i) {
        const size_t idx = (size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;
        // CELL CORNER: no +0.5 offset
        const double x=i*dx, y=j*dy, z=k*dz;
        const Vec3 e = analyticE(x,y,z, a,b,c);
        const Vec3 g = analyticGradDivE(x,y,z, a,b,c);
        Ex[idx]=e.x; Ey[idx]=e.y; Ez[idx]=e.z;
        Gx_true[idx]=g.x; Gy_true[idx]=g.y; Gz_true[idx]=g.z;
      }

  // Apply 3×3 operator rows using exported taps at CELL CORNERS
  for (int k=0; k<Nz; ++k)
    for (int j=0; j<Ny; ++j)
      for (int i=0; i<Nx; ++i) {
        const size_t idx = (size_t)k*Ny*Nx + (size_t)j*Nx + (size_t)i;

        // Row 0 (Gx): dxx*Ex + dxy*Ey + dxz*Ez
        double gx = 0.0;
        gx += apply_exported(SX[0], Ex, i,j,k, Nx,Ny,Nz);
        gx += apply_exported(SY[0], Ey, i,j,k, Nx,Ny,Nz);
        gx += apply_exported(SZ[0], Ez, i,j,k, Nx,Ny,Nz);
        Gx[idx] = gx;

        // Row 1 (Gy): dxy*Ex + dyy*Ey + dyz*Ez
        double gy = 0.0;
        gy += apply_exported(SX[1], Ex, i,j,k, Nx,Ny,Nz);
        gy += apply_exported(SY[1], Ey, i,j,k, Nx,Ny,Nz);
        gy += apply_exported(SZ[1], Ez, i,j,k, Nx,Ny,Nz);
        Gy[idx] = gy;

        // Row 2 (Gz): dxz*Ex + dyz*Ey + dzz*Ez
        double gz = 0.0;
        gz += apply_exported(SX[2], Ex, i,j,k, Nx,Ny,Nz);
        gz += apply_exported(SY[2], Ey, i,j,k, Nx,Ny,Nz);
        gz += apply_exported(SZ[2], Ez, i,j,k, Nx,Ny,Nz);
        Gz[idx] = gz;
      }

  // Errors
  double linf = 0.0, l2_num = 0.0, l2_den = 0.0;
  for (size_t t=0; t<NT; ++t) {
    const double ex = Gx[t]-Gx_true[t];
    const double ey = Gy[t]-Gy_true[t];
    const double ez = Gz[t]-Gz_true[t];
    linf = std::max(linf, std::max(std::abs(ex), std::max(std::abs(ey), std::abs(ez))));
    l2_num += ex*ex + ey*ey + ez*ez;
    l2_den += Gx_true[t]*Gx_true[t] + Gy_true[t]*Gy_true[t] + Gz_true[t]*Gz_true[t];
  }

  if (out_Gx) { *out_Gx = Gx; *out_Gy = Gy; *out_Gz = Gz; }
  if (out_Gx_true) { *out_Gx_true = Gx_true; *out_Gy_true = Gy_true; *out_Gz_true = Gz_true; }

  if (tecplot_dump) {
    const std::string f = "graddivE_corner_" + flavor_name(flavor) + "_N" + std::to_string(N) + ".dat";
    write_tecplot_block(f, Nx,Ny,Nz, dx,dy,dz, Ex,Ey,Ez, Gx,Gy,Gz, Gx_true,Gy_true,Gz_true);
  }

  return ErrStats{ linf, (l2_den>0) ? std::sqrt(l2_num/l2_den) : std::sqrt(l2_num) };
}

// ---------- Pretty print: pointwise comparison ----------
static void print_point_comparison(int N, double Lx,double Ly,double Lz,
                                   double a,double b,double c,
                                   const std::vector<double>& Gx,
                                   const std::vector<double>& Gy,
                                   const std::vector<double>& Gz,
                                   const std::vector<double>& Gx_true,
                                   const std::vector<double>& Gy_true,
                                   const std::vector<double>& Gz_true,
                                   const char* flavor_label)
{
  const int Nx=N, Ny=N, Nz=N;
  const double dx=Lx/Nx, dy=Ly/Ny, dz=Lz/Nz;
  // Choose a point with odd indices to avoid zeros in the trig functions
  // For N=16: (3,5,7) -> (3/16, 5/16, 7/16) gives non-zero field values
  const int ii=3, jj=5, kk=7;
  const size_t idx=(size_t)kk*Ny*Nx + (size_t)jj*Nx + (size_t)ii;
  // CELL CORNER: no +0.5 offset
  const double x=ii*dx, y=jj*dy, z=kk*dz;

  auto line = [&](const char* name, double a_, double n_){
    double err = std::abs(n_ - a_);
    std::cout << "    " << std::left << std::setw(14) << name
              << std::right << std::scientific << std::setprecision(8)
              << std::setw(16) << a_ << "   "
              << std::setw(16) << n_ << "   "
              << std::setprecision(3) << std::setw(8) << err << "\n";
  };

  std::cout << "\n[" << flavor_label << "] Component-wise comparison at interior CORNER point:\n"
            << "  Grid: N=" << N << ", (i,j,k)=(" << ii << "," << jj << "," << kk << ")"
            << ", (x,y,z)=(" << std::fixed << std::setprecision(6)
            << x << ", " << y << ", " << z << ") [CORNER]\n"
            << "  -----------------------------------------------------------------------------\n"
            << "    Component         Analytic               Numerical               AbsErr\n"
            << "  -----------------------------------------------------------------------------\n";

  line("(GradDivE)_x", Gx_true[idx], Gx[idx]);
  line("(GradDivE)_y", Gy_true[idx], Gy[idx]);
  line("(GradDivE)_z", Gz_true[idx], Gz[idx]);
}

// ---------- Convergence table across all flavors ----------
static void print_convergence_combined(const std::vector<int>& Ns,
                                       double Lx, double Ly, double Lz,
                                       double a, double b, double c,
                                       bool tecplot_on_finest = true)
{
  const std::vector<OpFlavor> flavors = {
    OpFlavor::SecondCompact,
    OpFlavor::SecondWide,
    OpFlavor::FourthOrder,
    OpFlavor::SixthOrder,
    OpFlavor::EighthOrder
  };
  const std::vector<const char*> headers = {
    "Second/Compact",
    "Second/Wide",
    "Fourth",
    "Sixth",
    "Eighth"
  };

  struct Row { double linf=0.0, l2=0.0; };
  std::vector<Row> err[5];
  for (int f=0; f<5; ++f) err[f].resize(Ns.size());

  std::cout << "\n=== GradDivE Convergence - CELL CORNER VERSION (Combined run) ===\n"
            << "Domain Lx=" << Lx << ", Ly=" << Ly << ", Lz=" << Lz
            << ", wave numbers a=" << a << ", b=" << b << ", c=" << c << "\n"
            << "Data sampled at CELL CORNERS (i*dx, j*dy, k*dz)\n";

  for (size_t t=0; t<Ns.size(); ++t) {
    const int N = Ns[t];
    const bool dump_tp = tecplot_on_finest && (t == Ns.size()-1);

    struct Arrs { std::vector<double> Gx,Gy,Gz, GxT,GyT,GzT; } out[5];

    for (int f=0; f<5; ++f) {
      ErrStats s = run_case(flavors[f], N, Lx,Ly,Lz, a,b,c,
                            &out[f].Gx, &out[f].Gy, &out[f].Gz,
                            &out[f].GxT,&out[f].GyT,&out[f].GzT,
                            dump_tp);
      err[f][t] = Row{s.linf, s.l2};
    }

    for (int f=0; f<5; ++f) {
      print_point_comparison(N, Lx,Ly,Lz, a,b,c,
                             out[f].Gx, out[f].Gy, out[f].Gz,
                             out[f].GxT,out[f].GyT,out[f].GzT,
                             headers[f]);
    }
  }

  // header row
  std::cout << "\n";
  std::cout << std::string(190, '-') << "\n";
  std::cout << "   N  |  ";
  for (int f=0; f<5; ++f) {
    std::cout << std::setw(14) << headers[f] << " (L_inf)  Ord  "
              << std::setw(14) << headers[f] << " (L2)     Ord";
    if (f != 4) std::cout << "  |  ";
  }
  std::cout << "\n" << std::string(190, '-') << "\n";

  for (size_t t=0; t<Ns.size(); ++t) {
    std::cout << std::setw(5) << Ns[t] << " ";
    for (int f=0; f<5; ++f) {
      double p_inf = 0.0, p_l2 = 0.0;
      if (t > 0) {
        p_inf = std::log2(err[f][t-1].linf / err[f][t].linf);
        p_l2  = std::log2(err[f][t-1].l2   / err[f][t].l2);
      }
      std::cout << " | "
                << std::scientific << std::setprecision(3) << std::setw(12) << err[f][t].linf << " "
                << std::fixed      << std::setprecision(2) << std::setw(5)  << (t==0 ? 0.0 : p_inf) << "  "
                << std::scientific << std::setprecision(3) << std::setw(12) << err[f][t].l2   << " "
                << std::fixed      << std::setprecision(2) << std::setw(5)  << (t==0 ? 0.0 : p_l2);
    }
    std::cout << "\n";
  }

  std::cout << std::string(190, '-') << "\n";
}

// ---------- Entry point ----------
namespace GradDivE {
int Run(const std::vector<std::string>& args) {
  const double Lx = 1.0, Ly = 1.0, Lz = 1.0;
  const double a = 2.0*M_PI, b = 2.0*M_PI, c = 2.0*M_PI;
  std::vector<int> Ns = {16, 24, 32, 48, 64};

  print_convergence_combined(Ns, Lx,Ly,Lz, a,b,c, /*tecplot_on_finest=*/true);

  std::cout << "\nTecplot files (finest grid per operator) - CORNER VERSION:\n"
               "  graddivE_corner_SecondOrder_Compact_N64.dat\n"
               "  graddivE_corner_SecondOrder_Wide_N64.dat\n"
               "  graddivE_corner_FourthOrder_N64.dat\n"
               "  graddivE_corner_SixthOrder_N64.dat\n"
               "  graddivE_corner_EighthOrder_N64.dat\n";
  return 0;
}
} // namespace GradDivE

// ---- GradDivE test registration & force-link shim ----

namespace GradDivE { int Run(const std::vector<std::string>&); }
REGISTER_STENCIL_TEST(GradDivE,
  "grad_div_e",
  "GradDivE stencils at CELL CORNERS: build, apply, and convergence.");

namespace GradDivE { void ForceLinkAllTests() {} }
