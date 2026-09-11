/**
 *  =============================================================================
 *  ecsim/curl_b_fourth_order.cpp
 *  Fourth-order curl(B) at cell corners from cell-centered B
 *  =============================================================================
 *
 *  GEOMETRY & TARGET
 *  -----------------------------------------------------------------------------
 *  Input storage:   Bx, By, Bz are stored at cell centers on a uniform Cartesian
 *                   grid with spacings (dx, dy, dz). A cell center is the integer
 *                   index location (i, j, k).
 *
 *  Output location: (∇×B) is evaluated at *cell corners*, e.g.
 *                     C(i-1/2, j-1/2, k-1/2)
 *                   The stencils constructed here are for the corner "behind"
 *                   the base cell (i, j, k): (-1/2, -1/2, -1/2). Stencils for
 *                   the other seven corners are obtained by index shifts outside
 *                   this file.
 *
 *  Curl components at a corner p:
 *      (curl B)_x(p) =  ∂Bz/∂y (p)  − ∂By/∂z (p)
 *      (curl B)_y(p) =  ∂Bx/∂z (p)  − ∂Bz/∂x (p)
 *      (curl B)_z(p) =  ∂By/∂x (p)  − ∂Bx/∂y (p)
 *
 *
 *  HIGH-LEVEL CONSTRUCTION (TENSOR PRODUCT)
 *  -----------------------------------------------------------------------------
 *  Each partial derivative at the corner is built as a 3D tensor product of:
 *    (A) 4th-order *midpoint interpolation* in the two axes transverse to the
 *        derivative axis (to move from cell centers to the corner-directed half
 *        indices in those directions), and
 *    (B) 4th-order *half-index derivative* along the derivative axis (to evaluate
 *        the derivative at the corner-directed half index).
 *
 *  The 1D operators are combined by outer products, producing a separable 3D
 *  stencil with support 4×4×4 (64 taps) per partial derivative. Two partials
 *  are added (with signs) per curl component.
 *
 *
 *  1D BUILDING BLOCKS (EXACT WEIGHTS & OFFSETS)
 *  -----------------------------------------------------------------------------
 *  Notation: We construct the corner at (-1/2,-1/2,-1/2) relative to (i,j,k).
 *  Offsets below are integer index shifts applied to *cell-centered* samples.
 *
 *  (A) Midpoint interpolation (4-pt, 4th order):
 *      Purpose: move a cell-centered quantity to the half index (·−1/2) in a
 *               transverse direction (toward the chosen corner).
 *
 *      Offsets used (array o[4]):   {-2, -1, 0, +1}
 *      Weights (applied independently per transverse axis):
 *          w_mid = (1/16) * [ -1,  9,  9, -1 ]
 *
 *      Properties:
 *        • Exact for polynomials up to degree 3 → truncation O(h^4).
 *        • Weight sum = 1, so constants are preserved.
 *        • Center of mass at -1/2 about the integer plane.
 *
 *  (B) Half-index derivative (4-pt, 4th order):
 *      Purpose: take ∂/∂n at the half index (·−1/2) along the derivative axis n.
 *
 *      Offsets used (array ed[4]):  {+1, 0, -1, -2}
 *      Weights (then scaled by inv_h = 1/h_n):
 *          w_dmid = (1/24) * [ -1,  27,  -27,  1 ]   (overall factor 1/h_n outside)
 *
 *      Properties:
 *        • Exact for polynomials up to degree 3 → truncation O(h^4).
 *        • Weight sum = 0, so constants differentiate to 0.
 *        • Antisymmetric around the midpoint; reproduces linear slopes exactly.
 *
 *  The two 1D operators are *tensorized* to yield 3D coefficients for each
 *  partial derivative at the corner.
 *
 *
 *  MAPPING TO 3D PARTIALS (EXAMPLE)
 *  -----------------------------------------------------------------------------
 *  To compute ∂Bz/∂y at the corner:
 *    • Transverse axes are x and z: apply midpoint interpolation in x and z using
 *      o = {-2,-1,0,1} with weights w_mid per axis (their tensor product).
 *    • Derivative axis is y: apply half-index derivative using ed = {+1,0,-1,-2}
 *      with weights w_dmid * (1/dy).
 *
 *  The 3D coefficient for a sample at offset (dx, dy, dz) is:
 *      coeff(dx,dy,dz) = w_dmid(sel_y) * (1/dy) * w_mid(sel_x) * w_mid(sel_z)
 *  with sel_x, sel_y, sel_z chosen by matching dx∈o, dy∈ed, dz∈o.
 *
 *  Other partials follow by cyclic permutation of axes.
 *
 *
 *  CURL ASSEMBLY & SIGNS
 *  -----------------------------------------------------------------------------
 *  For each output component c ∈ {0:x, 1:y, 2:z}, we store three sub-stencils
 *  S[c].Bx, S[c].By, S[c].Bz (contributions from the respective B-components).
 *
 *  The minus signs from the curl definition are *embedded* in the sub-stencils at
 *  construction time (we multiply the appropriate sub-stencil by -1). The .Bx
 *  sub-stencil is an empty holder when not needed, for symmetry.
 *
 *    (curl B)_x :  +∂Bz/∂y   −∂By/∂z   →  S[0].Bz (d/dy), S[0].By (−d/dz)
 *    (curl B)_y :  +∂Bx/∂z   −∂Bz/∂x   →  S[1].Bx (d/dz), S[1].Bz (−d/dx)
 *    (curl B)_z :  +∂By/∂x   −∂Bx/∂y   →  S[2].By (d/dx), S[2].Bx (−d/dy)
 *
 *  During application:  (curl B)_c = S[c].Bx ⊗ Bx + S[c].By ⊗ By + S[c].Bz ⊗ Bz.
 *  No extra sign handling is required at apply time.
 *
 *
 *  ACCURACY, SUPPORT, AND SCALING
 *  -----------------------------------------------------------------------------
 *  • Formal order: 4th order (O(h^4)) for smooth fields; exact for constants and
 *    cubic polynomials under the 1D operators used.
 *  • Support:       Each partial derivative uses 4×4×4 = 64 taps; each curl
 *    component combines two partials (up to 128 taps before merging). Overlaps
 *    are merged by S.Simplify().
 *  • Non-isotropic grids: dx, dy, dz may differ; each derivative axis is scaled
 *    by inv_h = 1/d(axis) only along that axis, preserving correctness.
 *
 *
 *  BOUNDARY & GHOST-CELL REQUIREMENTS
 *  -----------------------------------------------------------------------------
 *  The stencil touches up to two cells away in each transverse direction and up
 *  to +1 downstream / −2 upstream along the derivative axis (relative to corner
 *  direction). Ensure the *corner index domain* on which curl(B) is evaluated
 *  has corresponding ghost/halo coverage for B (cell-centered).
 *
 *
 *  SHIFTING TO OTHER CORNERS
 *  -----------------------------------------------------------------------------
 *  This file builds the stencils for the (-1/2,-1/2,-1/2) corner. Stencils for
 *  other corners are obtained by index shifts in the application code.
 *
 *
 *  CONSISTENCY CHECKS (RECOMMENDED)
 *  -----------------------------------------------------------------------------
 *  1) Constants → zero curl:   derivative weight sums are 0; midpoint sums are 1.
 *  2) Linear in derivative axis → exact slope at the half-index.
 *  3) Sine test:  For B = (sin z, sin x, sin y),
 *       ∇×B = (cos y − cos z,  cos z − cos x,  cos x − cos y).
 *     Under refinement h→h/2, max-norm error should drop by ~16× (4th order).
 *
 *  IMPLEMENTATION NOTES
 *  -----------------------------------------------------------------------------
 *  - `addFourthOrderPartial(S, axisDer, inv_h, sym)` assembles one partial using:
 *      o  = {-2,-1,0,1}      // transverse midpoint offsets
 *      ed = {+1,0,-1,-2}     // derivative half-index offsets
 *    and the weights above (midpoint and half-index derivative).
 *  - After tensoring and summing, `S.Simplify()` collapses duplicate offsets.
 *  - `InitCurlBStencils(Curl, dx,dy,dz)` builds three `cCurlBStencil` entries
 *    (x,y,z components) and embeds minus signs as required by curl.
 *
 *  References / Derivation:
 *    • Midpoint interpolation: 4-point Lagrange basis at x = −1/2 for nodes
 *      {−2,−1,0,+1} ⇒ (1/16)[−1,9,9,−1].
 *    • Half-index derivative: derivative of the same Lagrange basis at x = −1/2
 *      ⇒ (1/24)[−1,27,−27,1] (multiply by 1/h along the derivative axis).
 *  =============================================================================
 */

#include "../pic.h"  // brings in cStencil, cFrac, and cCurlBStencil (project headers)

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {
namespace FourthOrder {

// ---- 4-pt 4th-order midpoint interpolation weights: {m-2,m-1,m,m+1} → value at (m-1/2)
static inline void midpoint_w(double w[4]) {
  w[0] = -1.0/16.0;
  w[1] =  9.0/16.0;
  w[2] =  9.0/16.0;
  w[3] = -1.0/16.0;
}

/**
 * 4-pt, 4th-order derivative evaluated at a half-index (midpoint),
 * using integer samples at offsets {+1, 0, -1, -2} relative to the corner-directed midpoint.
 * When multiplied by inv_h, this yields (1/(24*h)) * [-1, 27, -27, 1].
 */
static inline void dmid_w(double w[4]) {
  w[0] = -1.0/24.0; // +1
  w[1] = +27.0/24.0; //  0
  w[2] = -27.0/24.0; // -1
  w[3] = + 1.0/24.0; // -2
}

/**
 * Assemble one partial derivative stencil at the (-1/2,-1/2,-1/2) corner
 * by tensoring transverse midpoint interpolations with a half-index derivative.
 *
 * axisDer: 0→d/dx, 1→d/dy, 2→d/dz
 * inv_h:   1/dx, 1/dy, or 1/dz accordingly
 * sym:     symbol (for debug/printing)
 */
static void addFourthOrderPartial(cStencil& S, int axisDer, double inv_h, const char* sym) {
  S = cStencil();
  const cFrac f(0,1);
  S.SetSymbol(sym);
  S.SetBase(f,f,f); // base at integer cell center; offsets are integers

  const int axA = (axisDer + 1) % 3; // first transverse axis
  const int axB = (axisDer + 2) % 3; // second transverse axis

  double wA[4], wB[4], wd[4];
  midpoint_w(wA);
  midpoint_w(wB);
  dmid_w(wd);

  // Transverse midpoint interpolation toward the (-1/2) corner
  const int o[4]  = {-2, -1,  0,  1};

  // Half-index derivative samples along derivative axis, aligned with dmid_w
  // order {+1, 0, -1, -2}
  const int ed[4] = {+1,  0, -1, -2};

  for (int E = 0; E < 4; ++E) {
    const double wD = wd[E] * inv_h;
    for (int a = 0; a < 4; ++a) {
      for (int b = 0; b < 4; ++b) {
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
 * Initialize curl(B) stencils for all three components at the (-1/2,-1/2,-1/2) corner.
 * Other corners are obtained by index shifts at application time.
 *
 * (curl B)_x = +∂Bz/∂y − ∂By/∂z
 * (curl B)_y = +∂Bx/∂z − ∂Bz/∂x
 * (curl B)_z = +∂By/∂x − ∂Bx/∂y
 *
 * NOTE: Minus signs are embedded here by multiplying the appropriate sub-stencil by -1.
 */
void InitCurlBStencils(cCurlBStencil* Curl, double dx, double dy, double dz) {
  // (curl B)_x = +∂Bz/∂y − ∂By/∂z
  Curl[0].Bx = cStencil(); // empty holder for symmetry
  Curl[0].Bx.SetSymbol("curl_Bx_from_Bx_4o");
  addFourthOrderPartial(Curl[0].Bz, /*d/dy*/1, 1.0/dy, "curl_Bx_from_Bz_4o");
  addFourthOrderPartial(Curl[0].By, /*d/dz*/2, 1.0/dz, "curl_Bx_from_By_4o");
  Curl[0].By *= -1.0; // apply the minus sign

  //cache the composite L∞ radius
  Curl[0].Radius = std::max({ Curl[0].Bx.RadiusLinf(), Curl[0].By.RadiusLinf(), Curl[0].Bz.RadiusLinf() });

  // (curl B)_y = +∂Bx/∂z − ∂Bz/∂x
  Curl[1].By = cStencil(); // empty holder
  Curl[1].By.SetSymbol("curl_By_from_By_4o");
  addFourthOrderPartial(Curl[1].Bx, /*d/dz*/2, 1.0/dz, "curl_By_from_Bx_4o");
  addFourthOrderPartial(Curl[1].Bz, /*d/dx*/0, 1.0/dx, "curl_By_from_Bz_4o");
  Curl[1].Bz *= -1.0;

  //cache the composite L∞ radius
  Curl[1].Radius = std::max({ Curl[1].Bx.RadiusLinf(), Curl[1].By.RadiusLinf(), Curl[1].Bz.RadiusLinf() });

  // (curl B)_z = +∂By/∂x − ∂Bx/∂y
  Curl[2].Bz = cStencil(); // empty holder
  Curl[2].Bz.SetSymbol("curl_Bz_from_Bz_4o");
  addFourthOrderPartial(Curl[2].By, /*d/dx*/0, 1.0/dx, "curl_Bz_from_By_4o");
  addFourthOrderPartial(Curl[2].Bx, /*d/dy*/1, 1.0/dy, "curl_Bz_from_Bx_4o");
  Curl[2].Bx *= -1.0;

  //cache the composite L∞ radius
  Curl[2].Radius = std::max({ Curl[2].Bx.RadiusLinf(), Curl[2].By.RadiusLinf(), Curl[2].Bz.RadiusLinf() });
}

} // namespace FourthOrder
} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

