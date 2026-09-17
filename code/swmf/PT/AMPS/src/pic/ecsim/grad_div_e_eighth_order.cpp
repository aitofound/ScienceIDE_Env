// =============================================================================
//  ecsim/grad_div_e_eighth_order.cpp
//  Eighth-order ∇(∇·E) stencil builder (3D, uniform Cartesian grid)
// =============================================================================
// PURPOSE
// -----------------------------------------------------------------------------
// Construct G = ∇(∇·E) for a vector field E = (Ex, Ey, Ez) stored at *cell
// centers* on a uniform Cartesian grid. We build three rows (Gx, Gy, Gz)
// mapping the three field components (Ex, Ey, Ez) to curl-free acceleration
// terms, using centered **8th-order** finite-difference operators.
//
// ROW STRUCTURE (same as lower orders)
// -----------------------------------------------------------------------------
//   Row 0 (Gx):  [ dxx, dxy, dxz ]
//   Row 1 (Gy):  [ dxy, dyy, dyz ]
//   Row 2 (Gz):  [ dxz, dyz, dzz ]
// where dxx = ∂²/∂x², dyy = ∂²/∂y², dzz = ∂²/∂z² and dxy = ∂²/(∂x∂y), etc.
//
// DISCRETIZATION (8th‑ORDER CENTRAL, UNIFORM GRID)
// -----------------------------------------------------------------------------
// We assemble metric‑free 1D operators and then form tensor products:
//   1) 1D 8th‑order first derivative (9‑point, offsets m = −4..+4):
//        D1(h) f_i = [  1/280,  −4/105,  1/5,  −4/5,  0,
//                        4/5,   −1/5,   4/105, −1/280 ] · f_{i−4..i+4} / h
//
//   2) 1D 8th‑order second derivative (9‑point, offsets m = −4..+4):
//        D2(h) f_i = [ −1/560,  8/315, −1/5,  8/5, −205/72,
//                        8/5,   −1/5,   8/315, −1/560 ] · f_{i−4..i+4} / h²
//
//   3) Mixed derivatives via tensor‑product composition of D1 operators:
//        dxy ≈ D1x ∘ D1y,  dxz ≈ D1x ∘ D1z,  dyz ≈ D1y ∘ D1z .
//      On a uniform Cartesian grid, composed D1 operators commute and retain
//      **8th‑order** accuracy.
//
// METRIC SCALING
// -----------------------------------------------------------------------------
// Coefficients are assembled metric‑free; we apply scaling at the end:
//   dxx *= 1/dx², dyy *= 1/dy², dzz *= 1/dz²,
//   dxy *= 1/(dx·dy), dxz *= 1/(dx·dz), dyz *= 1/(dy·dz).
//
// ORDER, FOOTPRINT, BOUNDARIES
// -----------------------------------------------------------------------------
// • Formal order: 8th in smooth interior regions.
// • Footprint (L∞ radius): 4 cells (needs 4 ghost layers for centered interior).
// • Near boundaries, one‑sided closures lower the local order as usual.
//
// API
// -----------------------------------------------------------------------------
//   struct cGradDivEStencil { cStencil Ex, Ey, Ez; int Radius = -1; };
//   void PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::EighthOrder
//        ::InitGradDivEBStencils(cGradDivEStencil* Rows, double dx, double dy, double dz);
//
// IMPLEMENTATION NOTES
// -----------------------------------------------------------------------------
// • Mirrors the 6th‑order builder in style (separable construction, AddShifted,
//   Simplify, and per‑row Radius caching). Offsets are integer, base defaults.
// • Symbols are not strictly necessary; keep consistent with existing code.
// • All coefficients use exact rationals to minimize round‑off during assembly.
// =============================================================================
// =============================================================================
// 8/5, −1/5, 8/315, −1/560 ] · f_{i−4..i+4} / h²
//
// 3) Mixed derivatives via tensor‑product composition of D1 operators:
// dxy ≈ D1x ∘ D1y, dxz ≈ D1x ∘ D1z, dyz ≈ D1y ∘ D1z .
// On a uniform Cartesian grid, composed D1 operators commute and retain
// **8th‑order** accuracy.
//
// METRIC SCALING
// -----------------------------------------------------------------------------
// Coefficients are assembled metric‑free; we apply scaling at the end:
// dxx *= 1/dx², dyy *= 1/dy², dzz *= 1/dz²,
// dxy *= 1/(dx·dy), dxz *= 1/(dx·dz), dyz *= 1/(dy·dz).
//
// ORDER, FOOTPRINT, BOUNDARIES
// -----------------------------------------------------------------------------
// • Formal order: 8th in smooth interior regions.
// • Footprint (L∞ radius): 4 cells (needs 4 ghost layers for centered interior).
// • Near boundaries, one‑sided closures lower the local order as usual.
//
// API
// -----------------------------------------------------------------------------
// struct cGradDivEStencil { cStencil Ex, Ey, Ez; int Radius = -1; };
// void PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::EighthOrder
// ::InitGradDivEBStencils(cGradDivEStencil* Rows, double dx, double dy, double dz);
//
// IMPLEMENTATION NOTES
// -----------------------------------------------------------------------------
// • Mirrors the 6th‑order builder in style (separable construction, AddShifted,
// Simplify, and per‑row Radius caching). Offsets are integer, base defaults.
// • Symbols are not strictly necessary; keep consistent with existing code.
// • All coefficients use exact rationals to minimize round‑off during assembly.
// =============================================================================

#include "../pic.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {
namespace EighthOrder {

namespace Helper {

inline void ExportToRows(const cStencil& dxx, const cStencil& dyy, const cStencil& dzz,
                         const cStencil& dxy, const cStencil& dxz, const cStencil& dyz,
                         cGradDivEStencil* Rows) {
  Rows[0].Ex = dxx; Rows[0].Ey = dxy; Rows[0].Ez = dxz;
  Rows[1].Ex = dxy; Rows[1].Ey = dyy; Rows[1].Ez = dyz;
  Rows[2].Ex = dxz; Rows[2].Ey = dyz; Rows[2].Ez = dzz;

  Rows[0].Ex.Simplify(); Rows[0].Ey.Simplify(); Rows[0].Ez.Simplify();
  Rows[1].Ex.Simplify(); Rows[1].Ey.Simplify(); Rows[1].Ez.Simplify();
  Rows[2].Ex.Simplify(); Rows[2].Ey.Simplify(); Rows[2].Ez.Simplify();

  Rows[0].Radius = std::max({Rows[0].Ex.RadiusLinf(), Rows[0].Ey.RadiusLinf(), Rows[0].Ez.RadiusLinf()});
  Rows[1].Radius = std::max({Rows[1].Ex.RadiusLinf(), Rows[1].Ey.RadiusLinf(), Rows[1].Ez.RadiusLinf()});
  Rows[2].Radius = std::max({Rows[2].Ex.RadiusLinf(), Rows[2].Ey.RadiusLinf(), Rows[2].Ez.RadiusLinf()});
}

inline void ScaleMetric(cStencil& dxx, cStencil& dyy, cStencil& dzz,
                        cStencil& dxy, cStencil& dxz, cStencil& dyz,
                        double dx, double dy, double dz) {
  const double i_dx2  = 1.0/(dx*dx);
  const double i_dy2  = 1.0/(dy*dy);
  const double i_dz2  = 1.0/(dz*dz);
  const double i_dxdy = 1.0/(dx*dy);
  const double i_dxdz = 1.0/(dx*dz);
  const double i_dydz = 1.0/(dy*dz);

  dxx *= i_dx2;  dyy *= i_dy2;  dzz *= i_dz2;
  dxy *= i_dxdy; dxz *= i_dxdz; dyz *= i_dydz;
}

inline void BuildD1_8th(cStencil& Dx, cStencil& Dy, cStencil& Dz) {
  // First derivative 8th order: offsets -4..+4
  // coeffs: [ 1/280, -4/105, 1/5, -4/5, 0, +4/5, -1/5, +4/105, -1/280 ] / h
  // x
  Dx.add(+1.0/280.0, -4, 0, 0);
  Dx.add(-4.0/105.0, -3, 0, 0);
  Dx.add(+1.0/5.0,   -2, 0, 0);
  Dx.add(-4.0/5.0,   -1, 0, 0);
  // (0 is zero)
  Dx.add(+4.0/5.0,   +1, 0, 0);
  Dx.add(-1.0/5.0,   +2, 0, 0);
  Dx.add(+4.0/105.0, +3, 0, 0);
  Dx.add(-1.0/280.0, +4, 0, 0);
  // y
  Dy.add(+1.0/280.0,  0,-4, 0);
  Dy.add(-4.0/105.0,  0,-3, 0);
  Dy.add(+1.0/5.0,    0,-2, 0);
  Dy.add(-4.0/5.0,    0,-1, 0);
  Dy.add(+4.0/5.0,    0,+1, 0);
  Dy.add(-1.0/5.0,    0,+2, 0);
  Dy.add(+4.0/105.0,  0,+3, 0);
  Dy.add(-1.0/280.0,  0,+4, 0);
  // z
  Dz.add(+1.0/280.0,  0, 0,-4);
  Dz.add(-4.0/105.0,  0, 0,-3);
  Dz.add(+1.0/5.0,    0, 0,-2);
  Dz.add(-4.0/5.0,    0, 0,-1);
  Dz.add(+4.0/5.0,    0, 0,+1);
  Dz.add(-1.0/5.0,    0, 0,+2);
  Dz.add(+4.0/105.0,  0, 0,+3);
  Dz.add(-1.0/280.0,  0, 0,+4);
}

inline void BuildD2_8th(cStencil& Dxx, cStencil& Dyy, cStencil& Dzz) {
  // Second derivative 8th order: offsets -4..+4
  // coeffs: [ -1/560, 8/315, -1/5, 8/5, -205/72, 8/5, -1/5, 8/315, -1/560 ] / h^2
  // x
  Dxx.add(-1.0/560.0, -4, 0, 0);
  Dxx.add(+8.0/315.0, -3, 0, 0);
  Dxx.add(-1.0/5.0,   -2, 0, 0);
  Dxx.add(+8.0/5.0,   -1, 0, 0);
  Dxx.add(-205.0/72.0, 0, 0, 0);
  Dxx.add(+8.0/5.0,   +1, 0, 0);
  Dxx.add(-1.0/5.0,   +2, 0, 0);
  Dxx.add(+8.0/315.0, +3, 0, 0);
  Dxx.add(-1.0/560.0, +4, 0, 0);
  // y
  Dyy.add(-1.0/560.0, 0,-4, 0);
  Dyy.add(+8.0/315.0, 0,-3, 0);
  Dyy.add(-1.0/5.0,   0,-2, 0);
  Dyy.add(+8.0/5.0,   0,-1, 0);
  Dyy.add(-205.0/72.0,0, 0, 0);
  Dyy.add(+8.0/5.0,   0,+1, 0);
  Dyy.add(-1.0/5.0,   0,+2, 0);
  Dyy.add(+8.0/315.0, 0,+3, 0);
  Dyy.add(-1.0/560.0, 0,+4, 0);
  // z
  Dzz.add(-1.0/560.0, 0, 0,-4);
  Dzz.add(+8.0/315.0, 0, 0,-3);
  Dzz.add(-1.0/5.0,   0, 0,-2);
  Dzz.add(+8.0/5.0,   0, 0,-1);
  Dzz.add(-205.0/72.0,0, 0, 0);
  Dzz.add(+8.0/5.0,   0, 0,+1);
  Dzz.add(-1.0/5.0,   0, 0,+2);
  Dzz.add(+8.0/315.0, 0, 0,+3);
  Dzz.add(-1.0/560.0, 0, 0,+4);
}

} // namespace Helper

void InitGradDivEBStencils(cGradDivEStencil* Rows, double dx, double dy, double dz) {
  using namespace Helper;

  // 1) Build 8th‑order primitives (metric‑free)
  cStencil Dx, Dy, Dz;      // D1 operators
  cStencil Dxx, Dyy, Dzz;   // D2 operators
  BuildD1_8th(Dx, Dy, Dz);
  BuildD2_8th(Dxx, Dyy, Dzz);

  // 2) Mixed derivatives via tensor‑product composition of D1
  cStencil Dxy, Dxz, Dyz;

  // Dxy = Dx ∘ Dy : shift Dy by x‑offsets {-4..+4} scaled by Dx weights
  Dxy.AddShifted(Dy, -4, 0, 0, +1.0/280.0);
  Dxy.AddShifted(Dy, -3, 0, 0, -4.0/105.0);
  Dxy.AddShifted(Dy, -2, 0, 0, +1.0/5.0);
  Dxy.AddShifted(Dy, -1, 0, 0, -4.0/5.0);
  // (0 is zero)
  Dxy.AddShifted(Dy, +1, 0, 0, +4.0/5.0);
  Dxy.AddShifted(Dy, +2, 0, 0, -1.0/5.0);
  Dxy.AddShifted(Dy, +3, 0, 0, +4.0/105.0);
  Dxy.AddShifted(Dy, +4, 0, 0, -1.0/280.0);

  // Dxz = Dx ∘ Dz
  Dxz.AddShifted(Dz, -4, 0, 0, +1.0/280.0);
  Dxz.AddShifted(Dz, -3, 0, 0, -4.0/105.0);
  Dxz.AddShifted(Dz, -2, 0, 0, +1.0/5.0);
  Dxz.AddShifted(Dz, -1, 0, 0, -4.0/5.0);
  Dxz.AddShifted(Dz, +1, 0, 0, +4.0/5.0);
  Dxz.AddShifted(Dz, +2, 0, 0, -1.0/5.0);
  Dxz.AddShifted(Dz, +3, 0, 0, +4.0/105.0);
  Dxz.AddShifted(Dz, +4, 0, 0, -1.0/280.0);

  // Dyz = Dy ∘ Dz : shift Dz by y‑offsets {-4..+4} scaled by Dy weights
  Dyz.AddShifted(Dz, 0, -4, 0, +1.0/280.0);
  Dyz.AddShifted(Dz, 0, -3, 0, -4.0/105.0);
  Dyz.AddShifted(Dz, 0, -2, 0, +1.0/5.0);
  Dyz.AddShifted(Dz, 0, -1, 0, -4.0/5.0);
  Dyz.AddShifted(Dz, 0, +1, 0, +4.0/5.0);
  Dyz.AddShifted(Dz, 0, +2, 0, -1.0/5.0);
  Dyz.AddShifted(Dz, 0, +3, 0, +4.0/105.0);
  Dyz.AddShifted(Dz, 0, +4, 0, -1.0/280.0);

  // 3) Metric scaling
  ScaleMetric(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, dx, dy, dz);

  // 4) Export rows and cache per‑row radii
  ExportToRows(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, Rows);

#ifdef _PIC_STENCIL_VERBOSE_
  std::printf("[Stencil] grad(div(E)) 8th: R∞(row0)=%d  R∞(row1)=%d  R∞(row2)=%d  (expect ~4)\n",
              Rows[0].Radius, Rows[1].Radius, Rows[2].Radius);
#endif
}

} // namespace EighthOrder
} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

