// ecsim/laplacian_all_orders.cpp
//
// Component-wise finite-difference Laplacian stencils for E = (Ex, Ey, Ez)
// Orders implemented: 2nd, 4th, 6th, 8th
//
// ──────────────────────────────────────────────────────────────────────────────
// DETAILED HEAD COMMENT
// ──────────────────────────────────────────────────────────────────────────────
// Overview
// --------
// This file implements the discrete Laplacian operator ∇² acting on each
// component of a vector field E = (Ex, Ey, Ez) on a *uniform Cartesian grid*.
// The operator is built in a *metric-free* form first (pure index-space
// coefficients), and the physical metrics are applied at the end through explicit
// scaling by 1/dα² (with α ∈ {x, y, z}). The resulting component-wise operator
// is identical for all components on a uniform grid, so we assign the same
// assembled scalar Laplacian stencil to Ex, Ey, and Ez.
//
// At a high level:
//
//   1) Build 1D second-derivative stencils D2_x, D2_y, D2_z along each axis.
//   2) Apply metric scaling: D2_x ← D2_x / dx², etc.
//   3) Assemble the scalar Laplacian  L = D2_x + D2_y + D2_z.
//   4) Assign L to each vector component: (Ex, Ey, Ez).
//   5) Cache the L∞ stencil radius in Out->Radius for downstream ghost handling.
//
// The approach mirrors the pattern used for GradDiv(E) construction in the
// codebase: coefficients are added via cStencil::add( coeff, i, j, k ), then
// algebraically reduced via cStencil::Simplify(), and radii are deduced via
// RadiusLinf() to communicate ghost-layer requirements.
//
//
// Discretization Details
// ----------------------
// For a scalar field φ, on a uniform grid the Laplacian splits into the sum
// of *pure* second derivatives:
//
//     ∇² φ ≈ D_xx φ + D_yy φ + D_zz φ
//
// Each D_αα is realized by a 1D *central* finite-difference approximation of
// f'' along the α-axis with a chosen formal order of accuracy. We never use
// mixed terms for the Laplacian itself (those appear for GradDiv mixed blocks);
// this makes the Laplacian assembly simply “add the three pure second-derivative
// stencils.”
//
// The implemented orders use the following *metric-free* 1D coefficients
// (where h is the grid spacing along that axis and is handled later):
//
//   2nd-order (radius 1; classic 3-point):
//     f''(0) ≈ ( +1·f[-1]  -2·f[0]  +1·f[+1] ) / h²
//
//   4th-order (radius 2; 5-point):
//     f''(0) ≈ ( -1  ·f[-2] +16 ·f[-1] -30 ·f[0] +16 ·f[+1] -1  ·f[+2] ) / (12 h²)
//
//   6th-order (radius 3; 7-point):
//     f''(0) ≈ (  2  ·f[-3] -27 ·f[-2] +270·f[-1] -490·f[0]
//                +270·f[+1] -27 ·f[+2] +  2 ·f[+3] ) / (180 h²)
//
//   8th-order (radius 4; 9-point):
//     f''(0) ≈ ( -1/560·f[-4] + 8/315·f[-3] - 1/5 ·f[-2] + 8/5 ·f[-1]
//                -205/72·f[0]
//                + 8/5 ·f[+1] - 1/5 ·f[+2] + 8/315·f[+3] - 1/560·f[+4] ) / h²
//
// For each order, we instantiate three axis-aligned copies (Dxx, Dyy, Dzz) by
// placing the above coefficients along (±i,0,0), (0,±j,0), (0,0,±k) lines.
//
//
// Metric Handling (dx, dy, dz)
// ----------------------------
// The stencils are assembled *without* metrics, so they remain reusable and
// easy to verify symbolically. Immediately prior to summation, we scale each
// axis operator:
//
//     Dxx ← Dxx * (1/dx²),  Dyy ← Dyy * (1/dy²),  Dzz ← Dzz * (1/dz²).
//
// After this, the scalar Laplacian is L = Dxx + Dyy + Dzz. We then simplify L
// (algebraic combination of like offsets) and assign L to Out->Ex, Out->Ey,
// and Out->Ez.
//
//
// Stencil Radius and Ghost Cells
// ------------------------------
// The formal L∞ radius (ghost width) needed by the operator equals the largest
// 1D radius used by any axis derivative (and the three axes share the same
// radius per order). Concretely:
//
//   - 2nd order: radius = 1 (needs ±1 in each axis)
//   - 4th order: radius = 2
//   - 6th order: radius = 3
//   - 8th order: radius = 4
//
// We compute and cache this in Out->Radius using RadiusLinf() on the axis
// stencils. Downstream applications should restrict the apply-region to the
// interior nodes that have at least Out->Radius ghost layers available.
//
//
// Application Semantics
// ---------------------
// The Laplacian acts component-wise on E. For a cell-centered field E, the
// discrete result is:
//
//   (∇²E)_x = apply( Out->Ex, Ex )
//   (∇²E)_y = apply( Out->Ey, Ey )
//   (∇²E)_z = apply( Out->Ez, Ez )
//
// On a uniform grid, the three component stencils are identical (we still
// store all three to match the cLaplacianStencil signature used elsewhere).
//
// Boundary conditions are not embedded here; the test harness (or the AMPS
// application) is responsible for providing compatible ghost layers (periodic,
// Dirichlet, Neumann, etc.) consistent with the chosen order’s radius.
//
//
// Verification & Convergence Testing
// ----------------------------------
// Manufactured solution recommended for periodic domains:
//
//   Choose a, b, c ∈ ℝ (wavenumbers).
//   Let   Ex = sin(ax) cos(by) cos(cz),
//         Ey = cos(ax) sin(by) cos(cz),
//         Ez = cos(ax) cos(by) sin(cz).
//
// Then  ∇²E = −(a² + b² + c²) E  component-wise.
//
// Testing procedure:
//   1) For a series of grids (N = 16, 32, 64, ...), build the Laplacian of the
//      desired order and apply to the analytic E.
//   2) Compare to the analytic target −(a² + b² + c²) E.
//   3) Compute L∞ and L2 errors over the interior region supported by the
//      stencil (exclude boundary cells fewer than Out->Radius away).
//   4) Estimate convergence order from error ratios between successive grids.
//      Expected slopes: ≈ 2, 4, 6, 8 for the respective operators.
//
//
// Numerical Pitfalls & Notes
// --------------------------
// * The 8th-order stencil uses larger-magnitude center weight (−205/72), so in
//   single precision it can be comparatively less robust on very noisy data;
//   prefer double precision for testing high-order stencils.
// * Mixed derivatives are not part of the Laplacian; do not reuse GradDiv’s
//   mixed blocks here.
// * Non-uniform grids are *not* supported by these fixed-coefficient stencils;
//   for stretched grids you need variable-coefficient operators or SBP/compact
//   schemes.
// * Always call Simplify() before storing to reduce duplicate offsets that
//   arise when axis stencils are summed.
//
//
// Performance Considerations
// --------------------------
// * All stencils are short and separable by axis; memory traffic typically
//   dominates runtime. For repeated applications, consider fusing scaling and
//   application, or precomputing a compact “apply plan” from the simplified
//   stencil.
// * For GPU execution, remember to ensure coalesced access along the fastest-
//   moving dimension and avoid redundant loads of neighboring cells when
//   applying multiple components.
//
//
// Extensibility
// -------------
// * The pattern here generalizes to higher even orders by inserting the
//   appropriate 1D f'' coefficients and extending the radius.
// * If different operators are needed per component (e.g., anisotropic metrics
//   or face vs. cell staggering), split the assignment and build per-component
//   variants instead of sharing L.
//
// ──────────────────────────────────────────────────────────────────────────────
// End of detailed head comment.
// ──────────────────────────────────────────────────────────────────────────────

#include <algorithm>
#include "../pic.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {

// ----------------------- 2nd ORDER (classic 6-point in 3D) -------------------
namespace SecondOrder {

// 1D second derivative: [1, -2, 1] at shifts ±1 and 0 (metric-free)
static inline void BuildD2_2nd(cStencil& Dxx, cStencil& Dyy, cStencil& Dzz) {
  // x-line
  Dxx.add(+1.0, -1, 0, 0);
  Dxx.add(-2.0,  0, 0, 0);
  Dxx.add(+1.0, +1, 0, 0);

  // y-line
  Dyy.add(+1.0, 0, -1, 0);
  Dyy.add(-2.0, 0,  0, 0);
  Dyy.add(+1.0, 0, +1, 0);

  // z-line
  Dzz.add(+1.0, 0, 0, -1);
  Dzz.add(-2.0, 0, 0,  0);
  Dzz.add(+1.0, 0, 0, +1);
}

void InitLaplacianStencil(cLaplacianStencil* Out, double dx, double dy, double dz) {
  cStencil Dxx, Dyy, Dzz;

  BuildD2_2nd(Dxx, Dyy, Dzz);

  // Metric scaling
  Dxx *= 1.0/(dx*dx);
  Dyy *= 1.0/(dy*dy);
  Dzz *= 1.0/(dz*dz);

  // Assemble L = Dxx + Dyy + Dzz  (use AddShifted to add whole stencils)
  cStencil L = Dxx;
  L.AddShifted(Dyy, 0,0,0, 1.0);
  L.AddShifted(Dzz, 0,0,0, 1.0);
  L.Simplify();

  // Component-wise assignment (identical operator)
  Out->Ex = L;
  Out->Ey = L;
  Out->Ez = L;

  // Ghost radius requirement
  Out->Radius = std::max({Dxx.RadiusLinf(), Dyy.RadiusLinf(), Dzz.RadiusLinf()});
}

} // namespace SecondOrder


// ----------------------- 4th ORDER (5-point 1D) ------------------------------
namespace FourthOrder {

// 1D 4th-order second derivative coefficients (radius 2):
// [-1, 16, -30, 16, -1] / 12 at shifts -2,-1,0,+1,+2 (metric-free)
static inline void BuildD2_4th(cStencil& Dxx, cStencil& Dyy, cStencil& Dzz) {
  // x-line
  Dxx.add(-1.0/12.0, -2, 0, 0);
  Dxx.add(+16.0/12.0, -1, 0, 0);
  Dxx.add(-30.0/12.0,  0, 0, 0);
  Dxx.add(+16.0/12.0, +1, 0, 0);
  Dxx.add(-1.0/12.0, +2, 0, 0);

  // y-line
  Dyy.add(-1.0/12.0, 0, -2, 0);
  Dyy.add(+16.0/12.0, 0, -1, 0);
  Dyy.add(-30.0/12.0, 0,  0, 0);
  Dyy.add(+16.0/12.0, 0, +1, 0);
  Dyy.add(-1.0/12.0, 0, +2, 0);

  // z-line
  Dzz.add(-1.0/12.0, 0, 0, -2);
  Dzz.add(+16.0/12.0, 0, 0, -1);
  Dzz.add(-30.0/12.0, 0, 0,  0);
  Dzz.add(+16.0/12.0, 0, 0, +1);
  Dzz.add(-1.0/12.0, 0, 0, +2);
}

void InitLaplacianStencil(cLaplacianStencil* Out, double dx, double dy, double dz) {
  cStencil Dxx, Dyy, Dzz;

  BuildD2_4th(Dxx, Dyy, Dzz);

  // Metric scaling
  Dxx *= 1.0/(dx*dx);
  Dyy *= 1.0/(dy*dy);
  Dzz *= 1.0/(dz*dz);

  // Assemble L
  cStencil L = Dxx;
  L.AddShifted(Dyy, 0,0,0, 1.0);
  L.AddShifted(Dzz, 0,0,0, 1.0);
  L.Simplify();

  Out->Ex = L;
  Out->Ey = L;
  Out->Ez = L;

  Out->Radius = std::max({Dxx.RadiusLinf(), Dyy.RadiusLinf(), Dzz.RadiusLinf()}); // expect 2
}

} // namespace FourthOrder


// ----------------------- 6th ORDER (7-point 1D) ------------------------------
namespace SixthOrder {

// 1D 6th-order second derivative (radius 3):
// [  2,  -27,  270, -490, 270,  -27,   2 ] / 180  at shifts -3..+3
static inline void BuildD2_6th(cStencil& Dxx, cStencil& Dyy, cStencil& Dzz) {
  // x-line
  Dxx.add(+2.0/180.0,  -3, 0, 0);
  Dxx.add(-27.0/180.0, -2, 0, 0);
  Dxx.add(+270.0/180.0,-1, 0, 0);
  Dxx.add(-490.0/180.0, 0, 0, 0);
  Dxx.add(+270.0/180.0,+1, 0, 0);
  Dxx.add(-27.0/180.0, +2, 0, 0);
  Dxx.add(+2.0/180.0,  +3, 0, 0);

  // y-line
  Dyy.add(+2.0/180.0, 0, -3, 0);
  Dyy.add(-27.0/180.0,0, -2, 0);
  Dyy.add(+270.0/180.0,0, -1, 0);
  Dyy.add(-490.0/180.0,0,  0, 0);
  Dyy.add(+270.0/180.0,0, +1, 0);
  Dyy.add(-27.0/180.0,0, +2, 0);
  Dyy.add(+2.0/180.0, 0, +3, 0);

  // z-line
  Dzz.add(+2.0/180.0, 0, 0, -3);
  Dzz.add(-27.0/180.0,0, 0, -2);
  Dzz.add(+270.0/180.0,0, 0, -1);
  Dzz.add(-490.0/180.0,0, 0,  0);
  Dzz.add(+270.0/180.0,0,  0, +1);
  Dzz.add(-27.0/180.0,0,  0, +2);
  Dzz.add(+2.0/180.0, 0,  0, +3);
}

void InitLaplacianStencil(cLaplacianStencil* Out, double dx, double dy, double dz) {
  cStencil Dxx, Dyy, Dzz;

  BuildD2_6th(Dxx, Dyy, Dzz);

  // Metric scaling
  Dxx *= 1.0/(dx*dx);
  Dyy *= 1.0/(dy*dy);
  Dzz *= 1.0/(dz*dz);

  // Assemble L
  cStencil L = Dxx;
  L.AddShifted(Dyy, 0,0,0, 1.0);
  L.AddShifted(Dzz, 0,0,0, 1.0);
  L.Simplify();

  Out->Ex = L;
  Out->Ey = L;
  Out->Ez = L;

  Out->Radius = std::max({Dxx.RadiusLinf(), Dyy.RadiusLinf(), Dzz.RadiusLinf()}); // expect 3
}

} // namespace SixthOrder


// ----------------------- 8th ORDER (9-point 1D) ------------------------------
namespace EighthOrder {

// Standard 8th-order central second derivative (radius 4):
// f'' ≈ ( -1/560 f_{-4} + 8/315 f_{-3} - 1/5 f_{-2} + 8/5 f_{-1}
//         -205/72 f_0
//         + 8/5 f_{+1} - 1/5 f_{+2} + 8/315 f_{+3} - 1/560 f_{+4} ) / h^2
static inline void BuildD2_8th(cStencil& Dxx, cStencil& Dyy, cStencil& Dzz) {
  // x-line
  Dxx.add(-1.0/560.0, -4, 0, 0);
  Dxx.add( 8.0/315.0, -3, 0, 0);
  Dxx.add(-1.0/5.0,   -2, 0, 0);
  Dxx.add( 8.0/5.0,   -1, 0, 0);
  Dxx.add(-205.0/72.0, 0, 0, 0);
  Dxx.add( 8.0/5.0,   +1, 0, 0);
  Dxx.add(-1.0/5.0,   +2, 0, 0);
  Dxx.add( 8.0/315.0, +3, 0, 0);
  Dxx.add(-1.0/560.0, +4, 0, 0);

  // y-line
  Dyy.add(-1.0/560.0, 0, -4, 0);
  Dyy.add( 8.0/315.0, 0, -3, 0);
  Dyy.add(-1.0/5.0,   0, -2, 0);
  Dyy.add( 8.0/5.0,   0, -1, 0);
  Dyy.add(-205.0/72.0,0,  0, 0);
  Dyy.add( 8.0/5.0,   0, +1, 0);
  Dyy.add(-1.0/5.0,   0, +2, 0);
  Dyy.add( 8.0/315.0, 0, +3, 0);
  Dyy.add(-1.0/560.0, 0, +4, 0);

  // z-line
  Dzz.add(-1.0/560.0, 0, 0, -4);
  Dzz.add( 8.0/315.0, 0, 0, -3);
  Dzz.add(-1.0/5.0,   0, 0, -2);
  Dzz.add( 8.0/5.0,   0, 0, -1);
  Dzz.add(-205.0/72.0,0, 0,  0);
  Dzz.add( 8.0/5.0,   0, 0, +1);
  Dzz.add(-1.0/5.0,   0, 0, +2);
  Dzz.add( 8.0/315.0, 0, 0, +3);
  Dzz.add(-1.0/560.0, 0, 0, +4);
}

void InitLaplacianStencil(cLaplacianStencil* Out, double dx, double dy, double dz) {
  cStencil Dxx, Dyy, Dzz;

  BuildD2_8th(Dxx, Dyy, Dzz);

  // Metric scaling
  Dxx *= 1.0/(dx*dx);
  Dyy *= 1.0/(dy*dy);
  Dzz *= 1.0/(dz*dz);

  // Assemble L
  cStencil L = Dxx;
  L.AddShifted(Dyy, 0,0,0, 1.0);
  L.AddShifted(Dzz, 0,0,0, 1.0);
  L.Simplify();

  Out->Ex = L;
  Out->Ey = L;
  Out->Ez = L;

  Out->Radius = std::max({Dxx.RadiusLinf(), Dyy.RadiusLinf(), Dzz.RadiusLinf()}); // expect 4
}

} // namespace EighthOrder

} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

