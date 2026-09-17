/**
 *  =============================================================================
 *  ecsim/curl_b_sixth_order.cpp
 *  Sixth-order curl(B) at cell corners from cell-centered B
 *  =============================================================================
 *
 *  GEOMETRY & TARGET
 *  -----------------------------------------------------------------------------
 *  Input storage:   Bx, By, Bz at cell centers on a uniform Cartesian grid
 *                   with spacings (dx, dy, dz). A cell center is integer (i,j,k).
 *
 *  Output location: (∇×B) evaluated at *cell corners*, e.g.
 *                     C(i-1/2, j-1/2, k-1/2)
 *                   This file builds the three curl components for the corner
 *                   "behind" the base cell (i,j,k): (-1/2,-1/2,-1/2).
 *                   Other corners are obtained by index shifts outside.
 *
 *  Curl components at a corner p:
 *      (curl B)_x(p) =  ∂Bz/∂y (p)  − ∂By/∂z (p)
 *      (curl B)_y(p) =  ∂Bx/∂z (p)  − ∂Bz/∂x (p)
 *      (curl B)_z(p) =  ∂By/∂x (p)  − ∂Bx/∂y (p)
 *
 *  HIGH-LEVEL CONSTRUCTION (TENSOR PRODUCT)
 *  -----------------------------------------------------------------------------
 *  Each partial derivative at the corner is built as a separable 3D stencil:
 *    (A) 6th-order *midpoint interpolation* along the two transverse axes to
 *        reach the corner-directed half index (·−1/2) in those axes, and
 *    (B) 6th-order *half-index derivative* along the derivative axis evaluated
 *        at the same corner-directed half index.
 *
 *  1D BUILDING BLOCKS (EXACT WEIGHTS & OFFSETS)
 *  -----------------------------------------------------------------------------
 *  Target point (per axis): x0 = −1/2 relative to the integer grid.
 *  Nodes used: integers m ∈ { −3, −2, −1, 0, +1, +2 } (6 points).
 *
 *  (A) 6th-order midpoint interpolation (to x0 = −1/2).
 *      Offsets o[6] = { −3, −2, −1, 0, +1, +2 }
 *      Weights (sum to 1, exact ≤ degree 5):
 *          w_mid6 = [  3/256,  −25/256,  75/128,  75/128,  −25/256,  3/256 ]
 *
 *  (B) 6th-order half-index first derivative at x0 = −1/2.
 *      Offsets ed[6] = { −3, −2, −1, 0, +1, +2 }
 *      Weights (sum to 0, exact ≤ degree 5), multiply by inv_h = 1/h_axis:
 *          w_dmid6 = [ −3/640,  25/384,  −75/64,  75/64,  −25/384,  3/640 ]
 *
 *  The 3D coefficient for a sample at offset (dx, dy, dz) is the product of the
 *  corresponding 1D weights; scaling by the appropriate 1/h is applied only
 *  along the derivative axis.
 *
 *  CURL ASSEMBLY & SIGNS
 *  -----------------------------------------------------------------------------
 *    (curl B)_x :  +∂Bz/∂y   − ∂By/∂z   →  S[0].Bz (d/dy), S[0].By (−d/dz)
 *    (curl B)_y :  +∂Bx/∂z   − ∂Bz/∂x   →  S[1].Bx (d/dz), S[1].Bz (−d/dx)
 *    (curl B)_z :  +∂By/∂x   − ∂Bx/∂y   →  S[2].By (d/dx), S[2].Bx (−d/dy)
 *  Minus signs are embedded in the sub-stencils here; no special handling at
 *  application time is needed.
 *
 *  ACCURACY, SUPPORT, AND SCALING
 *  -----------------------------------------------------------------------------
 *  • Formal order: 6th (O(h^6)) in smooth interior regions.
 *  • Support:       Each partial uses 6×6×6 = 216 taps; each curl component
 *    combines two partials (≤432 taps before merging). S.Simplify() collapses
 *    overlaps. L∞ radius is 3 (needs 3 ghost layers).
 *  • Non-isotropic grids: dx, dy, dz may differ; only the derivative axis uses
 *    inv_h = 1/d(axis). Transverse midpoint weights are dimensionless.
 *
 *  BOUNDARY & GHOST-CELL REQUIREMENTS
 *  -----------------------------------------------------------------------------
 *  The stencil touches up to 3 cells transverse and up to ±3 along the derivative
 *  axis around the half index. Ensure the corner domain has corresponding halos.
 *
 *  SHIFTING TO OTHER CORNERS
 *  -----------------------------------------------------------------------------
 *  This file builds the stencils for the (−1/2,−1/2,−1/2) corner. Stencils for
 *  other corners are obtained by index shifts in the application code.
 *
 *  IMPLEMENTATION PARALLELS
 *  -----------------------------------------------------------------------------
 *  Mirrors the structure/conventions of the existing 4th-order file for easy
 *  integration and maintenance. See that file for context and usage. :contentReference[oaicite:1]{index=1}
 */

#include "../pic.h"  // brings in cStencil, cFrac, and cCurlBStencil

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {
namespace SixthOrder {

// ---- 6-pt 6th-order midpoint interpolation weights to half index (·−1/2)
static inline void midpoint_w6(double w[6]) {
  w[0] =  +3.0/256.0;   // -3
  w[1] = -25.0/256.0;   // -2
  w[2] = +75.0/128.0;   // -1
  w[3] = +75.0/128.0;   //  0
  w[4] = -25.0/256.0;   // +1
  w[5] =  +3.0/256.0;   // +2
}

// ---- 6-pt 6th-order first-derivative weights at half index (·−1/2); multiply by inv_h
static inline void dmid_w6(double w[6]) {
  w[0] =  -3.0/640.0;   // -3
  w[1] = +25.0/384.0;   // -2
  w[2] =  -75.0/64.0;   // -1
  w[3] =  +75.0/64.0;   //  0
  w[4] = -25.0/384.0;   // +1
  w[5] =   3.0/640.0;   // +2
}

/**
 * Assemble one 6th-order partial derivative stencil at the (-1/2,-1/2,-1/2) corner
 * by tensoring transverse midpoint interpolations with a half-index derivative.
 *
 * axisDer: 0→d/dx, 1→d/dy, 2→d/dz
 * inv_h:   1/dx, 1/dy, or 1/dz accordingly
 * sym:     symbol (for debug/printing)
 */
static void addSixthOrderPartial(cStencil& S, int axisDer, double inv_h, const char* sym) {
  S = cStencil();
  const cFrac zero(0,1);
  S.SetSymbol(sym);
  S.SetBase(zero, zero, zero); // base at integer cell center; offsets are integers

  const int axA = (axisDer + 1) % 3; // first transverse axis
  const int axB = (axisDer + 2) % 3; // second transverse axis

  double wA[6], wB[6], wd[6];
  midpoint_w6(wA);
  midpoint_w6(wB);
  dmid_w6(wd);

  // Transverse midpoint interpolation toward the (-1/2) corner
  const int o[6]  = { -3, -2, -1,  0, +1, +2 };

  // Half-index derivative samples along derivative axis (same index ordering)
  const int ed[6] = { -3, -2, -1,  0, +1, +2 };

  for (int E = 0; E < 6; ++E) {
    const double wD = wd[E] * inv_h;   // apply metric scaling only along derivative axis
    for (int a = 0; a < 6; ++a) {
      for (int b = 0; b < 6; ++b) {
        const double coeff = wD * wA[a] * wB[b];

        int d[3] = {0,0,0};  // (di,dj,dk) in x,y,z
        d[axisDer] = ed[E];
        d[axA]     = o[a];
        d[axB]     = o[b];

        if (coeff != 0.0) S.add(coeff, d[0], d[1], d[2]);
      }
    }
  }
  S.Simplify();
}

/**
 * Initialize 6th-order curl(B) stencils for all three components at the
 * (-1/2,-1/2,-1/2) corner. Other corners are obtained by index shifts
 * at application time.
 *
 * (curl B)_x = +∂Bz/∂y − ∂By/∂z
 * (curl B)_y = +∂Bx/∂z − ∂Bz/∂x
 * (curl B)_z = +∂By/∂x − ∂Bx/∂y
 *
 * NOTE: Minus signs are embedded here by multiplying the appropriate
 *       sub-stencil by -1.
 */
void InitCurlBStencils(cCurlBStencil* Curl, double dx, double dy, double dz) {
  // (curl B)_x = +∂Bz/∂y − ∂By/∂z
  Curl[0].Bx = cStencil(); // empty holder for symmetry
  Curl[0].Bx.SetSymbol("curl_Bx_from_Bx_6o");
  addSixthOrderPartial(Curl[0].Bz, /*d/dy*/1, 1.0/dy, "curl_Bx_from_Bz_6o");
  addSixthOrderPartial(Curl[0].By, /*d/dz*/2, 1.0/dz, "curl_Bx_from_By_6o");
  Curl[0].By *= -1.0; // minus sign

  // Cache composite L∞ radius
  Curl[0].Radius = std::max({ Curl[0].Bx.RadiusLinf(), Curl[0].By.RadiusLinf(), Curl[0].Bz.RadiusLinf() });

  // (curl B)_y = +∂Bx/∂z − ∂Bz/∂x
  Curl[1].By = cStencil(); // empty holder
  Curl[1].By.SetSymbol("curl_By_from_By_6o");
  addSixthOrderPartial(Curl[1].Bx, /*d/dz*/2, 1.0/dz, "curl_By_from_Bx_6o");
  addSixthOrderPartial(Curl[1].Bz, /*d/dx*/0, 1.0/dx, "curl_By_from_Bz_6o");
  Curl[1].Bz *= -1.0;

  Curl[1].Radius = std::max({ Curl[1].Bx.RadiusLinf(), Curl[1].By.RadiusLinf(), Curl[1].Bz.RadiusLinf() });

  // (curl B)_z = +∂By/∂x − ∂Bx/∂y
  Curl[2].Bz = cStencil(); // empty holder
  Curl[2].Bz.SetSymbol("curl_Bz_from_Bz_6o");
  addSixthOrderPartial(Curl[2].By, /*d/dx*/0, 1.0/dx, "curl_Bz_from_By_6o");
  addSixthOrderPartial(Curl[2].Bx, /*d/dy*/1, 1.0/dy, "curl_Bz_from_Bx_6o");
  Curl[2].Bx *= -1.0;

  Curl[2].Radius = std::max({ Curl[2].Bx.RadiusLinf(), Curl[2].By.RadiusLinf(), Curl[2].Bz.RadiusLinf() });
}

} // namespace SixthOrder
} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

