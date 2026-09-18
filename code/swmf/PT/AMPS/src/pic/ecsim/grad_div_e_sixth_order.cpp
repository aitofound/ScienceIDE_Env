/**
 * @file    grad_div_e_sixth_order.cpp
 * @brief   Sixth-order ∇(∇·E) stencil builder (3D, uniform Cartesian grid).
 *
 * WHAT THIS BUILDS
 * ----------------
 * We construct G = ∇(∇·E) for E=(Ex,Ey,Ez) as three rows:
 *   row 0 (Gx): [ dxx, dxy, dxz ],
 *   row 1 (Gy): [ dxy, dyy, dyz ],
 *   row 2 (Gz): [ dxz, dyz, dzz ],
 * where dxx=∂²/∂x², dyy=∂²/∂y², dzz=∂²/∂z² and dxy=∂²/(∂x∂y), etc.
 *
 * DISCRETIZATION (SIXTH-ORDER CENTRAL DIFFERENCES, UNIFORM GRID)
 * -------------------------------------------------------------
 * 1) 1D sixth-order first derivative (7-point, central):
 *      D1(h) f_i = [-1,  9, -45, 0, 45,  -9,  1] · f_{i-3..i+3} / (60 h)
 *
 * 2) 1D sixth-order second derivative (7-point, central):
 *      D2(h) f_i = [ 2, -27, 270, -490, 270, -27, 2] · f_{i-3..i+3} / (180 h^2)
 *
 * 3) Mixed derivatives via tensor-product composition:
 *      dxy ≈ D1x ∘ D1y,  dxz ≈ D1x ∘ D1z,  dyz ≈ D1y ∘ D1z.
 *    (On a uniform Cartesian grid, composed D1 operators commute and retain 6th order.)
 *
 * METRIC SCALING
 * --------------
 * Coefficients are assembled metric-free; we apply scaling at the end:
 *   dxx *= 1/dx^2, dyy *= 1/dy^2, dzz *= 1/dz^2,
 *   dxy *= 1/(dx*dy), dxz *= 1/(dx*dz), dyz *= 1/(dy*dz).
 *
 * ORDER, FOOTPRINT, BOUNDARIES
 * ----------------------------
 * - Formal order: 6th in smooth interior regions.
 * - Footprint (L∞ radius): 3 cells (needs 3 ghost layers for centered interior).
 * - Near boundaries, one-sided closures lower the local order as usual.
 *
 * API
 * ---
 *   struct cGradDivEStencil { cStencil Ex, Ey, Ez; int Radius=-1; };
 *   void PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::SixthOrder
 *        ::InitGradDivEBStencils(cGradDivEStencil* Rows [*size 3*],
 *                                double dx, double dy, double dz);
 *
 * Notes:
 * - Mirrors the structure/pattern used in the 4th-order builder. (See that file for context.)
 */

#include "../pic.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {
namespace SixthOrder {

// ======================
// Helpers
// ======================
namespace Helper {

// Export three rows:
//   Rows[0]: Gx gets (dxx, dxy, dxz)
//   Rows[1]: Gy gets (dxy, dyy, dyz)
//   Rows[2]: Gz gets (dxz, dyz, dzz)
inline void ExportToRows(const cStencil& dxx, const cStencil& dyy, const cStencil& dzz,
                         const cStencil& dxy, const cStencil& dxz, const cStencil& dyz,
                         cGradDivEStencil* Rows) {
  Rows[0].Ex = dxx; Rows[0].Ey = dxy; Rows[0].Ez = dxz;
  Rows[1].Ex = dxy; Rows[1].Ey = dyy; Rows[1].Ez = dyz;
  Rows[2].Ex = dxz; Rows[2].Ey = dyz; Rows[2].Ez = dzz;

  Rows[0].Ex.Simplify(); Rows[0].Ey.Simplify(); Rows[0].Ez.Simplify();
  Rows[1].Ex.Simplify(); Rows[1].Ey.Simplify(); Rows[1].Ez.Simplify();
  Rows[2].Ex.Simplify(); Rows[2].Ey.Simplify(); Rows[2].Ez.Simplify();

  // Cache L∞ radius per row (requires cStencil::RadiusLinf()).
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

inline void BuildD1_6th(cStencil& Dx, cStencil& Dy, cStencil& Dz) {
  // First derivative: [-1, 9, -45, 0, 45, -9, 1] / 60 at offsets -3..+3
  // x
  Dx.add(-1.0/60.0, -3, 0, 0);
  Dx.add(+9.0/60.0, -2, 0, 0);
  Dx.add(-45.0/60.0,-1, 0, 0);
  Dx.add(+45.0/60.0,+1, 0, 0);
  Dx.add(-9.0/60.0, +2, 0, 0);
  Dx.add(+1.0/60.0, +3, 0, 0);
  // y
  Dy.add(-1.0/60.0,  0,-3, 0);
  Dy.add(+9.0/60.0,  0,-2, 0);
  Dy.add(-45.0/60.0, 0,-1, 0);
  Dy.add(+45.0/60.0, 0,+1, 0);
  Dy.add(-9.0/60.0,  0,+2, 0);
  Dy.add(+1.0/60.0,  0,+3, 0);
  // z
  Dz.add(-1.0/60.0,  0, 0,-3);
  Dz.add(+9.0/60.0,  0, 0,-2);
  Dz.add(-45.0/60.0, 0, 0,-1);
  Dz.add(+45.0/60.0, 0, 0,+1);
  Dz.add(-9.0/60.0,  0, 0,+2);
  Dz.add(+1.0/60.0,  0, 0,+3);
}

inline void BuildD2_6th(cStencil& Dxx, cStencil& Dyy, cStencil& Dzz) {
  // Second derivative: [2, -27, 270, -490, 270, -27, 2] / 180 at offsets -3..+3
  // x
  Dxx.add(+2.0/180.0, -3, 0, 0);
  Dxx.add(-27.0/180.0,-2, 0, 0);
  Dxx.add(+270.0/180.0,-1,0, 0);
  Dxx.add(-490.0/180.0, 0, 0, 0);
  Dxx.add(+270.0/180.0,+1,0, 0);
  Dxx.add(-27.0/180.0, +2,0, 0);
  Dxx.add(+2.0/180.0,  +3,0, 0);
  // y
  Dyy.add(+2.0/180.0, 0,-3, 0);
  Dyy.add(-27.0/180.0,0,-2, 0);
  Dyy.add(+270.0/180.0,0,-1, 0);
  Dyy.add(-490.0/180.0,0, 0, 0);
  Dyy.add(+270.0/180.0,0,+1, 0);
  Dyy.add(-27.0/180.0,0,+2, 0);
  Dyy.add(+2.0/180.0, 0,+3, 0);
  // z
  Dzz.add(+2.0/180.0, 0, 0,-3);
  Dzz.add(-27.0/180.0,0, 0,-2);
  Dzz.add(+270.0/180.0,0, 0,-1);
  Dzz.add(-490.0/180.0,0, 0, 0);
  Dzz.add(+270.0/180.0,0, 0,+1);
  Dzz.add(-27.0/180.0,0, 0,+2);
  Dzz.add(+2.0/180.0, 0, 0,+3);
}

} // namespace Helper

// =====================
// Main builder (6th)
// =====================
void InitGradDivEBStencils(cGradDivEStencil* Rows, double dx, double dy, double dz) {
  using namespace Helper;

  // 1) Build 6th-order primitives (metric-free)
  cStencil Dx, Dy, Dz;      // D1 operators
  cStencil Dxx, Dyy, Dzz;   // D2 operators
  BuildD1_6th(Dx, Dy, Dz);
  BuildD2_6th(Dxx, Dyy, Dzz);

  // 2) Mixed derivatives via tensor-product composition of D1
  cStencil Dxy, Dxz, Dyz;

  // Dxy = Dx ∘ Dy : shift Dy by x-offsets {-3..+3} scaled by Dx weights
  Dxy.AddShifted(Dy, -3, 0, 0, -1.0/60.0);
  Dxy.AddShifted(Dy, -2, 0, 0, +9.0/60.0);
  Dxy.AddShifted(Dy, -1, 0, 0, -45.0/60.0);
  // (Dx coef at 0 is 0)
  Dxy.AddShifted(Dy, +1, 0, 0, +45.0/60.0);
  Dxy.AddShifted(Dy, +2, 0, 0, -9.0/60.0);
  Dxy.AddShifted(Dy, +3, 0, 0, +1.0/60.0);

  // Dxz = Dx ∘ Dz
  Dxz.AddShifted(Dz, -3, 0, 0, -1.0/60.0);
  Dxz.AddShifted(Dz, -2, 0, 0, +9.0/60.0);
  Dxz.AddShifted(Dz, -1, 0, 0, -45.0/60.0);
  Dxz.AddShifted(Dz, +1, 0, 0, +45.0/60.0);
  Dxz.AddShifted(Dz, +2, 0, 0, -9.0/60.0);
  Dxz.AddShifted(Dz, +3, 0, 0, +1.0/60.0);

  // Dyz = Dy ∘ Dz : shift Dz by y-offsets {-3..+3} scaled by Dy weights
  Dyz.AddShifted(Dz, 0, -3, 0, -1.0/60.0);
  Dyz.AddShifted(Dz, 0, -2, 0, +9.0/60.0);
  Dyz.AddShifted(Dz, 0, -1, 0, -45.0/60.0);
  Dyz.AddShifted(Dz, 0, +1, 0, +45.0/60.0);
  Dyz.AddShifted(Dz, 0, +2, 0, -9.0/60.0);
  Dyz.AddShifted(Dz, 0, +3, 0, +1.0/60.0);

  // 3) Metric scaling
  ScaleMetric(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, dx, dy, dz);

  // 4) Export rows and cache per-row radii
  ExportToRows(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, Rows);

#ifdef _PIC_STENCIL_VERBOSE_
  std::printf("[Stencil] grad(div(E)) 6th: "
              "R∞(row0)=%d  R∞(row1)=%d  R∞(row2)=%d  (expect ~3)\n",
              Rows[0].Radius, Rows[1].Radius, Rows[2].Radius);
#endif
}

} // namespace SixthOrder
} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

