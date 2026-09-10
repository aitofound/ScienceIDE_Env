// =============================================================================
//  ecsim/curl_b_eighth_order.cpp
//  Eighth-order curl(B) at cell corners from cell-centered B (ECSIM)
// =============================================================================
// PURPOSE
// -----------------------------------------------------------------------------
// Provide an eighth‑order accurate discrete approximation of the curl of a
// magnetic field B stored at cell centers of a uniform Cartesian mesh and
// evaluated at the cell corner C(i, j, k) = (i*dx, j*dy, k*dz). This mirrors the
// 2nd/4th/6th‑order builders in the ECSIM stencil suite (naming, symbols,
// Base/Export/Simplify usage, Radius computation) so the test harness can pick
// the 8th‑order version exactly like the others.
//
// CONTEXT: DATA LAYOUT & GEOMETRY
// -----------------------------------------------------------------------------
// • Storage: cell‑centered B = (Bx,By,Bz) at integer indices (i,j,k).
// • Target:  curl(B) evaluated at cell corners; application uses periodic wrap
//   and (i,j,k) addresses corners directly.
// • This file constructs all three component rows of the curl operator:
//     (curl B)_x =  ∂Bz/∂y − ∂By/∂z
//     (curl B)_y =  ∂Bx/∂z − ∂Bz/∂x
//     (curl B)_z =  ∂By/∂x − ∂Bx/∂y
//   Signs are embedded by multiplying sub‑stencils by −1 as appropriate.
//
// HIGH‑ORDER CONSTRUCTION (SEPARABLE / TENSOR PRODUCT)
// -----------------------------------------------------------------------------
// Each partial derivative at the corner is constructed as a 3D tensor product
// of three 1D rules evaluated on *integer* sample locations around the corner:
//   • On the two axes transverse to the derivative axis, use *midpoint* (to the
//     half index −1/2) interpolation from cell centers.
//   • On the derivative axis, use a *first derivative* evaluated at the same
//     half index (−1/2). The metric 1/h is applied only along the derivative axis.
// Using eight support points per axis gives 8th‑order accuracy; the support
// radius in L∞ norm is 4 cells on each axis.
//
// 1D BUILDING BLOCKS (EXACT RATIONAL COEFFICIENTS)
// -----------------------------------------------------------------------------
// Offsets per axis: o ∈ {−4,−3,−2,−1,0,+1,+2,+3} relative to the target corner.
// (A) 8th‑order midpoint interpolation at x0 = −1/2 (sums to 1):
//     w_mid8[o] = (1/2048) [ −5, 49, −245, 1225, 1225, −245, 49, −5 ]
// (B) 8th‑order first derivative at x0 = −1/2 (sums to 0), with signs oriented
//     to match the 4th/6th‑order files (the weight at the most positive offset is
//     NEGATIVE):
//     w_dmid8 = [ −5/7168, +49/5120, −245/3072, +1225/1024,
//                 −1225/1024, +245/3072, −49/5120, +5/7168 ]
// The derivative row must be multiplied by the corresponding metric 1/dx, 1/dy,
// or 1/dz. The 3D coefficient at (Δi,Δj,Δk) is the product of the three 1D
// weights, with the derivative row supplying the 1/h factor.
//
// BOUNDARIES & RADIUS
// -----------------------------------------------------------------------------
// Centered 8th‑order requires four ghost layers when used near physical
// boundaries. This file computes and caches Curl[c].Radius = max L∞ radius over
// its sub‑stencils so callers can validate halo depth (the test harness is
// periodic and does not require halos).
//
// IMPLEMENTATION NOTES
// -----------------------------------------------------------------------------
// • Mirrors ecsim/curl_b_fourth_order.cpp style: SetSymbol, SetBase, Simplify,
//   ExportStencil compatibility, no dependence on nonstandard helpers.
// • Base is set to (0,0,0) using cFrac(0,1) to generate integer offsets
//   relative to the corner index used by the test harness during Export/Apply.
// • Signs for curl assembly are applied via s *= −1.0 on the appropriate
//   sub‑stencil (no cStencil::Add API is used).
// • Derivative offset order ed = {+3,+2,+1,0,−1,−2,−3,−4} matches the weight
//   row ordering, as in the 4th/6th‑order implementations.
// =============================================================================

#include "../pic.h"  // cStencil, cFrac, cCurlBStencil

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {
namespace EighthOrder {

// ---- 8‑pt 8th‑order midpoint interpolation weights toward (−1/2)
static inline void midpoint8_w(double w[8]) {
  w[0] = -5.0/2048.0;
  w[1] =  49.0/2048.0;
  w[2] = -245.0/2048.0;
  w[3] =  1225.0/2048.0;
  w[4] =  1225.0/2048.0;
  w[5] = -245.0/2048.0;
  w[6] =  49.0/2048.0;
  w[7] = -5.0/2048.0;
}

// ---- 8‑pt 8th‑order derivative evaluated at half index (−1/2)
// Sign convention aligned with existing 4th/6th‑order files: the first entry
// (most positive offset relative to the corner‑directed half index) is NEGATIVE.
static inline void dmid8_w(double w[8]) {
  w[0] =  -5.0/7168.0;   // +3
  w[1] =   49.0/5120.0;  // +2
  w[2] =  -245.0/3072.0; // +1
  w[3] =  1225.0/1024.0; //  0
  w[4] = -1225.0/1024.0; // -1
  w[5] =   245.0/3072.0; // -2
  w[6] =   -49.0/5120.0; // -3
  w[7] =     5.0/7168.0; // -4
}

/**
 * Assemble one partial derivative stencil at the (−1/2,−1/2,−1/2) corner by
 * tensoring transverse midpoint interpolations with a half‑index derivative.
 *
 * axisDer: 0→d/dx, 1→d/dy, 2→d/dz
 * inv_h:   1/dx, 1/dy, or 1/dz accordingly
 * sym:     symbol (for debug/printing)
 */
static void addEighthOrderPartial(cStencil& S, int axisDer, double inv_h, const char* sym) {
  S = cStencil();
  const cFrac f(0,1);
  S.SetSymbol(sym);
  S.SetBase(f,f,f); // integer offset base (corner addressing at application)

  const int axA = (axisDer + 1) % 3; // first transverse axis
  const int axB = (axisDer + 2) % 3; // second transverse axis

  double wA[8], wB[8], wd[8];
  midpoint8_w(wA);
  midpoint8_w(wB);
  dmid8_w(wd);

  // Transverse midpoint interpolation offsets toward the (−1/2) corner
  const int o[8]  = {-4, -3, -2, -1, 0, 1, 2, 3};

  // Half‑index derivative sample offsets along derivative axis, aligned with
  // dmid8_w ordering: {+3,+2,+1,0,−1,−2,−3,−4}
  const int ed[8] = {+3, +2, +1, 0, -1, -2, -3, -4};

  for (int E = 0; E < 8; ++E) {
    const double wD = wd[E] * inv_h;
    for (int a = 0; a < 8; ++a) {
      for (int b = 0; b < 8; ++b) {
        const double coeff = wD * wA[a] * wB[b];
        int d[3] = {0,0,0};  // (di,dj,dk) in x,y,z
        d[axisDer] = ed[E];
        d[axA]     = o[a];
        d[axB]     = o[b];
        S.add(coeff, d[0], d[1], d[2]);
      }
    }
  }
  S.Simplify();
}

/**
 * Initialize curl(B) stencils for all three components at the (−1/2,−1/2,−1/2)
 * corner (relative to the base cell). Other corners are obtained by index
 * shifts at application time.
 *
 * (curl B)_x = +∂Bz/∂y − ∂By/∂z
 * (curl B)_y = +∂Bx/∂z − ∂Bz/∂x
 * (curl B)_z = +∂By/∂x − ∂Bx/∂y
 *
 * Minus signs are embedded by multiplying the appropriate sub‑stencil by −1.
 */
void InitCurlBStencils(cCurlBStencil* Curl, double dx, double dy, double dz) {
  // (curl B)_x = +∂Bz/∂y − ∂By/∂z
  Curl[0].Bx = cStencil(); // empty holder for symmetry
  Curl[0].Bx.SetSymbol("curl_Bx_from_Bx_8o");
  addEighthOrderPartial(Curl[0].Bz, /*d/dy*/1, 1.0/dy, "curl_Bx_from_Bz_8o");
  addEighthOrderPartial(Curl[0].By, /*d/dz*/2, 1.0/dz, "curl_Bx_from_By_8o");
  Curl[0].By *= -1.0; // apply minus sign
  Curl[0].Radius = std::max({ Curl[0].Bx.RadiusLinf(), Curl[0].By.RadiusLinf(), Curl[0].Bz.RadiusLinf() });

  // (curl B)_y = +∂Bx/∂z − ∂Bz/∂x
  Curl[1].By = cStencil(); // empty holder
  Curl[1].By.SetSymbol("curl_By_from_By_8o");
  addEighthOrderPartial(Curl[1].Bx, /*d/dz*/2, 1.0/dz, "curl_By_from_Bx_8o");
  addEighthOrderPartial(Curl[1].Bz, /*d/dx*/0, 1.0/dx, "curl_By_from_Bz_8o");
  Curl[1].Bz *= -1.0;
  Curl[1].Radius = std::max({ Curl[1].Bx.RadiusLinf(), Curl[1].By.RadiusLinf(), Curl[1].Bz.RadiusLinf() });

  // (curl B)_z = +∂By/∂x − ∂Bx/∂y
  Curl[2].Bz = cStencil(); // empty holder
  Curl[2].Bz.SetSymbol("curl_Bz_from_Bz_8o");
  addEighthOrderPartial(Curl[2].By, /*d/dx*/0, 1.0/dx, "curl_Bz_from_By_8o");
  addEighthOrderPartial(Curl[2].Bx, /*d/dy*/1, 1.0/dy, "curl_Bz_from_Bx_8o");
  Curl[2].Bx *= -1.0;
  Curl[2].Radius = std::max({ Curl[2].Bx.RadiusLinf(), Curl[2].By.RadiusLinf(), Curl[2].Bz.RadiusLinf() });
}

} // namespace EighthOrder
} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

// =============================================================================
// END OF FILE
// =============================================================================

