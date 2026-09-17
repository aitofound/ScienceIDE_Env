/**
 * @file    grad_div_e_fourth_order.cpp
 * @brief   Fourth-order ∇(∇·E) stencil builder (curl(B)-style signature).
 *
 * WHAT THIS BUILDS
 * ----------------
 * We construct the 3×3 block operator for G = ∇(∇·E) acting on a vector field E = (Ex,Ey,Ez):
 *   G_x = ∂/∂x(∂Ex/∂x) + ∂/∂x(∂Ey/∂y) + ∂/∂x(∂Ez/∂z)
 *   G_y = ∂/∂y(∂Ex/∂x) + ∂/∂y(∂Ey/∂y) + ∂/∂y(∂Ez/∂z)
 *   G_z = ∂/∂z(∂Ex/∂x) + ∂/∂z(∂Ey/∂y) + ∂/∂z(∂Ez/∂z)
 *
 * Discretely, this resolves into the 6 second derivatives:
 *   pure:  dxx = ∂²/∂x², dyy = ∂²/∂y², dzz = ∂²/∂z²
 *   mixed: dxy = ∂²/(∂x∂y), dxz = ∂²/(∂x∂z), dyz = ∂²/(∂y∂z)
 * exported into rows:
 *   row 0 (Gx): [ dxx, dxy, dxz ],
 *   row 1 (Gy): [ dxy, dyy, dyz ],
 *   row 2 (Gz): [ dxz, dyz, dzz ].
 *
 * DISCRETIZATION (FOURTH-ORDER CENTRAL DIFFERENCES, UNIFORM GRID)
 * --------------------------------------------------------------
 * 1) 1D fourth-order first derivative (5-point, central):
 *      D1(h) f_i  = [ -f_{i+2} + 8 f_{i+1} - 8 f_{i-1} +  f_{i-2} ] / (12 h)
 *    Truncation error O(h^4). We encode the numerator coefficients (±1, ±8, 0) divided by 12
 *    into the stencil; the 1/h metric factor is applied at assembly time (see “Metric scaling”).
 *
 * 2) 1D fourth-order second derivative (5-point, central):
 *      D2(h) f_i  = [ -f_{i+2} + 16 f_{i+1} - 30 f_i + 16 f_{i-1} - f_{i-2} ] / (12 h^2)
 *    Truncation error O(h^4). Again, we encode the 1/12-scaled coefficients into the stencil;
 *    the 1/h^2 factor is applied later.
 *
 * 3) Mixed derivatives via tensor-product composition of 4th-order first derivatives:
 *      dxy ≈ D1x ∘ D1y,   dxz ≈ D1x ∘ D1z,   dyz ≈ D1y ∘ D1z.
 *    Concretely, if Dx has weights wx at x-shifts s ∈ { -2,-1,+1,+2 },
 *    then D1x(D1y f) is realized as:
 *      Dxy = Σ_s wx * (Dy shifted by s in x).
 *    Because D1x and D1y are each 4th-order accurate on a uniform grid and commute for smooth
 *    fields (constant spacing, Cartesian tensor-product), the composed mixed operators remain
 *    4th-order accurate. This also preserves symmetry dxy = dyx at the discrete level.
 *
 * METRIC SCALING & ANISOTROPIC SPACING
 * ------------------------------------
 * We keep the stencils’ coefficients metric-free and apply scaling at the end:
 *   dxx *= 1/dx^2,   dyy *= 1/dy^2,   dzz *= 1/dz^2,
 *   dxy *= 1/(dx*dy), dxz *= 1/(dx*dz), dyz *= 1/(dy*dz).
 * Unequal spacings (dx ≠ dy ≠ dz) are fully supported.
 *
 * ORDER OF ACCURACY & ISOTROPY
 * ----------------------------
 * - Formal order: 4th-order for smooth fields on uniform grids.
 * - The 5-point 1D formulas improve phase/dispersion properties over 2nd-order.
 * - Mixed terms built by 4th-order D1 composition remain 4th-order while maintaining symmetry.
 *
 * BOUNDARIES
 * ----------
 * Near domain boundaries, these 5-point formulas require two ghost cells (or specialized
 * one-sided high-order stencils). If one-sided 1st- or 2nd-order closures are used at the
 * same location, the local order will drop there accordingly. The interior remains 4th-order.
 *
 * VERIFICATION IDEAS
 * ------------------
 * Use an analytic vector field with known ∇(∇·E), e.g.,
 *   Ex = sin(ax) cos(by) cos(cz),  Ey = cos(ax) sin(by) cos(cz),  Ez = cos(ax) cos(by) sin(cz),
 * and measure L∞/L2 errors vs. h for grid refinements; expect slope ≈ 4.
 *
 * API SUMMARY
 * -----------
 *   struct cGradDivEStencil { cStencil Ex, Ey, Ez; };
 *   void PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::FourthOrder
 *        ::InitGradDivEBStencils(cGradDivEStencil* S, double dx, double dy, double dz);
 *
 * DEPENDENCIES
 * ------------
 * - Requires cStencil with: add(), AddShifted(), SwitchAxes(), Simplify(), operator*=.
 * - Requires cGradDivEStencil declaration (included from grad_div_e_second_order.h).
 */

#include "../pic.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {
namespace FourthOrder {

// ======================
// Helpers (definitions)
// ======================
namespace Helper {

void ExportToRows(const cStencil& dxx, const cStencil& dyy, const cStencil& dzz,
                  const cStencil& dxy, const cStencil& dxz, const cStencil& dyz,
                  cGradDivEStencil* S) {
  S[0].Ex = dxx; S[0].Ey = dxy; S[0].Ez = dxz;
  S[1].Ex = dxy; S[1].Ey = dyy; S[1].Ez = dyz;
  S[2].Ex = dxz; S[2].Ey = dyz; S[2].Ez = dzz;

  S[0].Ex.Simplify(); S[0].Ey.Simplify(); S[0].Ez.Simplify();
  S[1].Ex.Simplify(); S[1].Ey.Simplify(); S[1].Ez.Simplify();
  S[2].Ex.Simplify(); S[2].Ey.Simplify(); S[2].Ez.Simplify();
}

void ScaleMetric(cStencil& dxx, cStencil& dyy, cStencil& dzz,
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

// 4th-order 1D first derivatives (unscaled by 1/h).
void BuildD1_4th(cStencil& Dx, cStencil& Dy, cStencil& Dz) {
  // x-direction: [1,-8,0,8,-1]/12 at offsets {-2,-1,0,+1,+2}
  Dx.add(+1.0/12.0, -2, 0, 0);
  Dx.add(-8.0/12.0, -1, 0, 0);
  Dx.add(+8.0/12.0, +1, 0, 0);
  Dx.add(-1.0/12.0, +2, 0, 0);

  Dy.add(+1.0/12.0,  0,-2, 0);
  Dy.add(-8.0/12.0,  0,-1, 0);
  Dy.add(+8.0/12.0,  0,+1, 0);
  Dy.add(-1.0/12.0,  0,+2, 0);

  Dz.add(+1.0/12.0,  0, 0,-2);
  Dz.add(-8.0/12.0,  0, 0,-1);
  Dz.add(+8.0/12.0,  0, 0,+1);
  Dz.add(-1.0/12.0,  0, 0,+2);
}

// 4th-order 1D second derivatives (unscaled by 1/h^2).
void BuildD2_4th(cStencil& Dxx, cStencil& Dyy, cStencil& Dzz) {
  // x-direction: [-1,16,-30,16,-1]/12 at offsets {+2,+1,0,-1,-2}
  Dxx.add(-1.0/12.0, +2, 0, 0);
  Dxx.add(+16.0/12.0, +1, 0, 0);
  Dxx.add(-30.0/12.0,  0, 0, 0);
  Dxx.add(+16.0/12.0, -1, 0, 0);
  Dxx.add(-1.0/12.0, -2, 0, 0);

  Dyy.add(-1.0/12.0,  0,+2, 0);
  Dyy.add(+16.0/12.0, 0,+1, 0);
  Dyy.add(-30.0/12.0, 0, 0, 0);
  Dyy.add(+16.0/12.0, 0,-1, 0);
  Dyy.add(-1.0/12.0, 0,-2, 0);

  Dzz.add(-1.0/12.0,  0, 0,+2);
  Dzz.add(+16.0/12.0, 0, 0,+1);
  Dzz.add(-30.0/12.0, 0, 0, 0);
  Dzz.add(+16.0/12.0, 0, 0,-1);
  Dzz.add(-1.0/12.0, 0, 0,-2);
}

} // namespace Helper


// =====================
// Main builder (4th)
// =====================
void InitGradDivEBStencils(cGradDivEStencil* S, double dx, double dy, double dz) {
  using namespace Helper;

  // 1) Build 4th-order primitives (unscaled)
  cStencil Dx, Dy, Dz;
  cStencil Dxx, Dyy, Dzz;
  BuildD1_4th(Dx, Dy, Dz);
  BuildD2_4th(Dxx, Dyy, Dzz);

  // 2) Mixed derivatives via tensor-product composition of 4th-order D1
  cStencil Dxy, Dxz, Dyz;

  // Dxy = Dx ∘ Dy: shift Dy by x-offsets {-2,-1,+1,+2} and scale by Dx weights
  Dxy.AddShifted(Dy, -2, 0, 0, +1.0/12.0);
  Dxy.AddShifted(Dy, -1, 0, 0, -8.0/12.0);
  Dxy.AddShifted(Dy, +1, 0, 0, +8.0/12.0);
  Dxy.AddShifted(Dy, +2, 0, 0, -1.0/12.0);

  // Dxz = Dx ∘ Dz
  Dxz.AddShifted(Dz, -2, 0, 0, +1.0/12.0);
  Dxz.AddShifted(Dz, -1, 0, 0, -8.0/12.0);
  Dxz.AddShifted(Dz, +1, 0, 0, +8.0/12.0);
  Dxz.AddShifted(Dz, +2, 0, 0, -1.0/12.0);

  // Dyz = Dy ∘ Dz: shift Dz by y-offsets {-2,-1,+1,+2} and scale by Dy weights
  Dyz.AddShifted(Dz, 0, -2, 0, +1.0/12.0);
  Dyz.AddShifted(Dz, 0, -1, 0, -8.0/12.0);
  Dyz.AddShifted(Dz, 0, +1, 0, +8.0/12.0);
  Dyz.AddShifted(Dz, 0, +2, 0, -1.0/12.0);

  // 3) Metric scaling
  ScaleMetric(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, dx, dy, dz);

  // 4) Export rows
  ExportToRows(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, S);

  //cache the composite L∞ radius
  for (int i=0;i<3;i++) {
    S[i].Radius = std::max({ S[i].Ex.RadiusLinf(), S[i].Ey.RadiusLinf(), S[i].Ez.RadiusLinf() });
  }
}

} // namespace FourthOrder
} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

