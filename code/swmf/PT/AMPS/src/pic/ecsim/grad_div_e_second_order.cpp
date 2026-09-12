/**
 * @file    grad_div_e_second_order.cpp
 * @brief   Second-order ∇(∇·E) stencil builders with curl(B)-style signature.
 *
 * API
 * ---
 *   struct cGradDivEStencil { cStencil Ex, Ey, Ez; };
 *
 *   // Compact (nested centered first/second differences)
 *   void PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::SecondOrder
 *        ::InitGradDivEBStencils_compact(cGradDivEStencil* S, double dx, double dy, double dz);
 *
 *   // Wide (symmetric face/edge construction + rotations)
 *   void PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::SecondOrder
 *        ::InitGradDivEBStencils_wide   (cGradDivEStencil* S, double dx, double dy, double dz);
 *
 * Semantics
 * ---------
 *   S has length 3 and holds the rows of G = ∇(∇·E):
 *     S[0] (Gx): [ dxx, dxy, dxz ]
 *     S[1] (Gy): [ dxy, dyy, dyz ]
 *     S[2] (Gz): [ dxz, dyz, dzz ]
 *
 * Notes
 * -----
 * - Both are formally **second order** on uniform grids.
 * - Metric scaling is applied internally:
 *     dxx *= 1/dx^2, dxy *= 1/(dx*dy), …, dzz *= 1/dz^2
 */

/*
================================================================================
GradDivE Discretizations: Second-Order (Compact vs Wide) and Fourth-Order
================================================================================

We discretize the operator G(E) = ∇(∇·E) for E = (Ex,Ey,Ez) on a uniform,
cell-centered, periodic Cartesian grid. The discrete operator is assembled row-
wise as:

  [Gx]   [ Dxx  Dxy  Dxz ] [Ex]
  [Gy] = [ Dxy  Dyy  Dyz ] [Ey]
  [Gz]   [ Dxz  Dyz  Dzz ] [Ez]

All stencils are centered and symmetric. There are two second-order variants
("compact" and "wide") which (after the fix) are algebraically identical, and a
fourth-order variant.

Notation
--------
• Node index (i,j,k), spacings (dx,dy,dz).
• Offsets are (Δi,Δj,Δk) relative to (i,j,k).
• Taps are listed UN-SCALED first, then the metric scale (1/dx², 1/(dx·dy), …)
  is applied at the end.

================================================================================
SECOND-ORDER SCHEMES (Compact and Wide — identical after the fix)
================================================================================

A) Pure second derivatives (Dxx, Dyy, Dzz) — standard centered 3-point
----------------------------------------------------------------------
Unscaled taps (apply indicated metric scale at the end):

  Dxx (scale 1/dx²):
    (+1, 0, 0) : +1
    ( 0, 0, 0) : -2
    (-1, 0, 0) : +1

  Dyy (scale 1/dy²):
    ( 0,+1, 0) : +1
    ( 0, 0, 0) : -2
    ( 0,-1, 0) : +1

  Dzz (scale 1/dz²):
    ( 0, 0,+1) : +1
    ( 0, 0, 0) : -2
    ( 0, 0,-1) : +1

These are O(h²) accurate.

B) Mixed second derivatives (Dxy, Dxz, Dyz) — 4-corner “cross” in the plane
----------------------------------------------------------------------------
Either derived by composing centered first derivatives (COMPACT) or written
explicitly (WIDE). After the fix, both routes give the SAME taps:

  Dxy (scale 1/(dx·dy)):
    (+1,+1,0) : +1/4
    (+1,-1,0) : -1/4
    (-1,+1,0) : -1/4
    (-1,-1,0) : +1/4

  Dxz (scale 1/(dx·dz)):
    (+1,0,+1) : +1/4
    (+1,0,-1) : -1/4
    (-1,0,+1) : -1/4
    (-1,0,-1) : +1/4

  Dyz (scale 1/(dy·dz)):
    (0,+1,+1) : +1/4
    (0,+1,-1) : -1/4
    (0,-1,+1) : -1/4
    (0,-1,-1) : +1/4

Properties (Second-Order)
-------------------------
• Order: O(h²) for all entries.
• Discrete Fourier symbols (θx,θy,θz are grid wavenumbers):
    Dxx ~ -2(1 - cos θx)/dx²
    Dxy ~  (sin θx sin θy)/(dx·dy), etc.
• COMPACT (via centered Dₓ∘Dᵧ) and WIDE (explicit cross) are algebraically
  equivalent; with the corrected wide implementation, numerical outputs match
  to round-off.

================================================================================
FOURTH-ORDER SCHEME
================================================================================

Construction:
• Pure seconds (Dxx, Dyy, Dzz): 1D 5-point centered, 4th-order.
• Mixed seconds (Dxy, Dxz, Dyz): composition of 4th-order centered first
  derivatives in each axis; coefficients form an outer product (5×5 in the
  relevant plane), yielding O(h⁴).

A) Pure second derivatives (4th-order, 5-point)
-----------------------------------------------
Unscaled taps (apply 1/dx², 1/dy², 1/dz² after), shown for x (analogous for y,z):

  Dxx (scale 1/dx²):
    (+2,0,0) : -1/12
    (+1,0,0) : +4/3
    ( 0,0,0) : -5/2
    (-1,0,0) : +4/3
    (-2,0,0) : -1/12

B) First derivatives (4th-order, 5-point) used to build mixed terms
-------------------------------------------------------------------
Unscaled 1D centered first-derivative weights (Δ = [+2,+1,0,-1,-2]):

  c[Δ] = [ +1,  -8,   0,  +8,  -1 ]  (to be scaled by 1/(12·h_axis))

Thus:
  Dx f ≈ (1/(12·dx)) * Σ_p c[p] f_{i+Δp, j,   k}
  Dy f ≈ (1/(12·dy)) * Σ_q c[q] f_{i,   j+Δq, k}
  Dz f ≈ (1/(12·dz)) * Σ_r c[r] f_{i,   j,   k+Δr}

C) Mixed second derivatives (4th-order) by composition (outer product)
----------------------------------------------------------------------
Use the outer product of the 1D weights in the two involved directions:

  Dxy f ≈ (1/(144·dx·dy)) * Σ_p Σ_q c[p]·c[q] · f_{i+Δp, j+Δq, k}
  Dxz f ≈ (1/(144·dx·dz)) * Σ_p Σ_r c[p]·c[r] · f_{i+Δp, j,     k+Δr}
  Dyz f ≈ (1/(144·dy·dz)) * Σ_q Σ_r c[q]·c[r] · f_{i,     j+Δq, k+Δr}

This yields a centered 5×5 stencil in each transverse plane for the mixed terms,
with O(h⁴) accuracy.

D) Assembling ∇(∇·E) rows (all orders)
--------------------------------------
  Gx = (Dxx Ex) + (Dxy Ey) + (Dxz Ez)
  Gy = (Dxy Ex) + (Dyy Ey) + (Dyz Ez)
  Gz = (Dxz Ex) + (Dyz Ey) + (Dzz Ez)

Properties (Fourth-Order)
-------------------------
• Order: O(h⁴) for all entries.
• Symbols:
    Dx (4th) ~ (i/dx) [ (8/6) sin θx  - (1/6) sin 2θx ]
    Dxy(4th) ~ (1/(dx·dy)) · [ (8/6) sin θx - (1/6) sin 2θx ]
                         ·   [ (8/6) sin θy - (1/6) sin 2θy ]
    Dxx(4th) ~ (1/dx²) [ -5/2 + (4/3) cos θx - (1/12) cos 2θx ]
  Improved dispersion and isotropy vs. second-order, especially on coarse grids.

================================================================================
BOUNDARIES / HALOS / IMPLEMENTATION NOTES
================================================================================
• Periodic wrap: use wrap-around indexing; interior order is preserved.
• Halos if non-periodic:
    - 2nd-order pure/mixed: 1-cell halo along involved axes.
    - 4th-order pure:       2-cell halo along the axis.
    - 4th-order mixed:      2-cell halo in BOTH involved axes.
• Scaling: build unscaled taps, then scale by 1/dx², 1/(dx·dy), etc.
• Equivalence (2nd-order): Compact (Dₓ∘Dᵧ) and Wide (explicit cross) now produce
  the same taps and, therefore, the same numerical results (up to round-off).
• Performance: we pre-compose/export taps and apply once. 4th-order has a larger
  but still sparse footprint (5-point on-axis; 5×5 for mixed terms).

================================================================================
SANITY CHECK (Plane-wave)
================================================================================
Let a,b,c ∈ 2πℤ over [0,1] and
  Ex =  sin(ax) cos(by) cos(cz)
  Ey =  cos(ax) sin(by) cos(cz)
  Ez =  cos(ax) cos(by) sin(cz)
Then:
  ∇·E = (a+b+c) cos(ax) cos(by) cos(cz)
  ∇(∇·E) = [ -a(a+b+c) sin(ax) cos(by) cos(cz),
             -b(a+b+c) cos(ax) sin(by) cos(cz),
             -c(a+b+c) cos(ax) cos(by) sin(cz) ]
Expected convergence:
  • Second-order: ~O(h²) in L∞ and relative L².
  • Fourth-order: ~O(h⁴).

================================================================================
MAPPING TO CODE (what each builder creates)
================================================================================
• SecondOrder::InitGradDivEBStencils_compact(S,dx,dy,dz)
    Builds: Dxx/Dyy/Dzz (3-point); Dxy/Dxz/Dyz by composing centered 1st-derivative
    stencils → same 4-corner cross taps after composition; applies metric scaling.

• SecondOrder::InitGradDivEBStencils_wide(S,dx,dy,dz)
    Builds: Dxx/Dyy/Dzz (3-point); Dxy/Dxz/Dyz explicitly as 4-corner cross;
    applies the same metric scaling. (Now identical to Compact.)

• FourthOrder::InitGradDivEBStencils(S,dx,dy,dz)
    Builds: Dxx/Dyy/Dzz (5-point, 4th order); Dxy/Dxz/Dyz via outer-product
    composition of 4th-order 1D first-derivative weights; applies metric scaling.
*/


#include "../pic.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {

// -----------------------------------------------------------------------------
// Public row container (curl-style)
// -----------------------------------------------------------------------------
//struct cGradDivEStencil {
//  cStencil Ex, Ey, Ez;  // columns acting on (Ex, Ey, Ez) for one output row
//};

namespace SecondOrder {

// -----------------------------------------------------------------------------
// Common helpers (used by both flavors)
// -----------------------------------------------------------------------------
static inline void ExportToRows(const cStencil& dxx, const cStencil& dyy, const cStencil& dzz,
                                const cStencil& dxy, const cStencil& dxz, const cStencil& dyz,
                                cGradDivEStencil* S) {
  S[0].Ex = dxx; S[0].Ey = dxy; S[0].Ez = dxz;
  S[1].Ex = dxy; S[1].Ey = dyy; S[1].Ez = dyz;
  S[2].Ex = dxz; S[2].Ey = dyz; S[2].Ez = dzz;

  // Tidy up possible duplicate entries
  S[0].Ex.Simplify(); S[0].Ey.Simplify(); S[0].Ez.Simplify();
  S[1].Ex.Simplify(); S[1].Ey.Simplify(); S[1].Ez.Simplify();
  S[2].Ex.Simplify(); S[2].Ey.Simplify(); S[2].Ez.Simplify();
}

static inline void ScaleMetric(cStencil& dxx, cStencil& dyy, cStencil& dzz,
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

// -----------------------------------------------------------------------------
// Helpers dedicated to the WIDE builder
// -----------------------------------------------------------------------------
namespace Helper_Wide {

// Build 2×2×2 corner average around (0,0,0) shifted to octant (I,J,K)∈{0,1}³
inline void BuildCorner(cStencil& st, int I, int J, int K) {
  for (int di = -1; di <= 0; ++di)
    for (int dj = -1; dj <= 0; ++dj)
      for (int dk = -1; dk <= 0; ++dk)
        st.add(1.0/8.0, I + di, J + dj, K + dk);
}

// Build the 12 edge averages (each a 2×2 slab)
inline void BuildEdges(cStencil edges[12]) {
  struct R { int imin,imax, jmin,jmax, kmin,kmax; };
  static const R rng[12] = {
    { 0,0,-1,0,-1,0},{ 0,0, 0,1,-1,0},{ 0,0, 0,1, 0,1},{ 0,0,-1,0, 0,1},
    {-1,0, 0,0,-1,0},{ 0,1, 0,0,-1,0},{ 0,1, 0,0, 0,1},{-1,0, 0,0, 0,1},
    {-1,0,-1,0, 0,0},{ 0,1,-1,0, 0,0},{ 0,1, 0,1, 0,0},{-1,0, 0,1, 0,0}
  };
  for (int f = 0; f < 12; ++f) {
    edges[f] = cStencil();
    for (int i = rng[f].imin; i <= rng[f].imax; ++i)
      for (int j = rng[f].jmin; j <= rng[f].jmax; ++j)
        for (int k = rng[f].kmin; k <= rng[f].kmax; ++k)
          edges[f].add(1.0/4.0, i, j, k);
  }
}

} // namespace Helper_Wide


// ============================================================================
// Compact (nested centered first/second differences)
// ============================================================================
void InitGradDivEBStencils_compact(cGradDivEStencil* S, double dx, double dy, double dz) {
  // Centered first derivatives (unscaled)
  cStencil Dx, Dy, Dz;
  Dx.add(+0.5, +1, 0, 0); Dx.add(-0.5, -1, 0, 0);
  Dy.add(+0.5,  0,+1, 0); Dy.add(-0.5,  0,-1, 0);
  Dz.add(+0.5,  0, 0,+1); Dz.add(-0.5,  0, 0,-1);

  // Classic 3-pt second derivatives (unscaled)
  cStencil Dxx, Dyy, Dzz;
  Dxx.add(+1.0, +1, 0, 0); Dxx.add(+1.0, -1, 0, 0); Dxx.add(-2.0, 0, 0, 0);
  Dyy.add(+1.0,  0,+1, 0); Dyy.add(+1.0,  0,-1, 0); Dyy.add(-2.0, 0, 0, 0);
  Dzz.add(+1.0,  0, 0,+1); Dzz.add(+1.0,  0, 0,-1); Dzz.add(-2.0, 0, 0, 0);

  // Mixed derivatives via nested centered first differences (unscaled)
  // Dxy = ∂x(∂/∂y) : outer ∂/∂x across i±1 planes of the Dy stencil
  cStencil Dxy; Dxy.AddShifted(Dy, +1, 0, 0, +0.5);
               Dxy.AddShifted(Dy, -1, 0, 0, -0.5);

  // Dxz = ∂x(∂/∂z)
  cStencil Dxz; Dxz.AddShifted(Dz, +1, 0, 0, +0.5);
               Dxz.AddShifted(Dz, -1, 0, 0, -0.5);

  // Dyz = ∂y(∂/∂z)
  cStencil Dyz; Dyz.AddShifted(Dz, 0, +1, 0, +0.5);
               Dyz.AddShifted(Dz, 0, -1, 0, -0.5);

  // Apply metric scaling
  ScaleMetric(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, dx, dy, dz);

  // Export to rows
  ExportToRows(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, S);

  //cache the composite L∞ radius
  for (int i=0;i<3;i++) {
    S[i].Radius = std::max({ S[i].Ex.RadiusLinf(), S[i].Ey.RadiusLinf(), S[i].Ez.RadiusLinf() });
  }
}


// ============================================================================
// Wide (fixed): use standard centered cross-derivative stencils (second order)
// ============================================================================
void InitGradDivEBStencils_wide(cGradDivEStencil* S, double dx, double dy, double dz) {
  // --- Pure seconds (use classic centered 3-point, unscaled) ---
  cStencil Dxx, Dyy, Dzz;
  Dxx.add(+1.0, +1, 0, 0); Dxx.add(+1.0, -1, 0, 0); Dxx.add(-2.0, 0, 0, 0);
  Dyy.add(+1.0,  0,+1, 0); Dyy.add(+1.0,  0,-1, 0); Dyy.add(-2.0, 0, 0, 0);
  Dzz.add(+1.0,  0, 0,+1); Dzz.add(+1.0,  0, 0,-1); Dzz.add(-2.0, 0, 0, 0);

  // --- Mixed seconds via 4-corner ("wide") cross stencils, unscaled ---
  // Dxy: + at (+1,+1), (-1,-1); - at (+1,-1), (-1,+1)
  cStencil Dxy;
  Dxy.add(+0.25, +1, +1, 0);
  Dxy.add(+0.25, -1, -1, 0);
  Dxy.add(-0.25, +1, -1, 0);
  Dxy.add(-0.25, -1, +1, 0);

  // Dxz: + at (+1,0,+1), (-1,0,-1); - at (+1,0,-1), (-1,0,+1)
  cStencil Dxz;
  Dxz.add(+0.25, +1, 0, +1);
  Dxz.add(+0.25, -1, 0, -1);
  Dxz.add(-0.25, +1, 0, -1);
  Dxz.add(-0.25, -1, 0, +1);

  // Dyz: + at (0,+1,+1), (0,-1,-1); - at (0,+1,-1), (0,-1,+1)
  cStencil Dyz;
  Dyz.add(+0.25, 0, +1, +1);
  Dyz.add(+0.25, 0, -1, -1);
  Dyz.add(-0.25, 0, +1, -1);
  Dyz.add(-0.25, 0, -1, +1);

  // --- Metric scaling (∂xx→/dx^2, ∂xy→/(dx dy), …) ---
  ScaleMetric(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, dx, dy, dz);

  // --- Export rows:    Gx: [dxx,dxy,dxz], Gy: [dxy,dyy,dyz], Gz: [dxz,dyz,dzz] ---
  ExportToRows(Dxx, Dyy, Dzz, Dxy, Dxz, Dyz, S);

  //cache the composite L∞ radius
  for (int i=0;i<3;i++) {
    S[i].Radius = std::max({ S[i].Ex.RadiusLinf(), S[i].Ey.RadiusLinf(), S[i].Ez.RadiusLinf() });
  }
}


} // namespace SecondOrder

} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

