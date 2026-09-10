/**
 * @file    curlb_stencil_second_order.cpp
 * @brief   Build **second-order** stencils for curl(B) at **cell corners** from **cell-centered** B.
 *
 * ALGORITHM OVERVIEW
 * ------------------
 * Goal: compute (∇×B) at the cell corner located at (i-1/2, j-1/2, k-1/2) while Bx,By,Bz are
 * stored at cell centers (i, j, k). We assemble three stencils (one per curl component);
 * each stencil is further split into the contributions coming from stored Bx, By, and Bz:
 *
 *   (curl B)_x =  +∂Bz/∂y  - ∂By/∂z
 *   (curl B)_y =  +∂Bx/∂z  - ∂Bz/∂x
 *   (curl B)_z =  +∂By/∂x  - ∂Bx/∂y
 *
 * To achieve **second-order accuracy** on a staggered location (the corner), we use a
 * two-step construction for each partial derivative:
 *
 *   1) **Interpolate to the two edges** adjacent to the corner and **parallel** to the derivative’s
 *      normal. The field value on an edge is the arithmetic mean of the two nearest cell centers.
 *
 *        Example for ∂Bz/∂y at corner (i-1/2, j-1/2, k-1/2):
 *          edge@j   : (i-1/2, j,   k-1/2) ← 0.5*(Bz[i,  j,  k] + Bz[i-1, j,   k-1])
 *          edge@j-1 : (i-1/2, j-1, k-1/2) ← 0.5*(Bz[i,  j-1,k] + Bz[i-1, j-1,k-1])
 *
 *   2) **Central-difference across those two edges**:
 *        ∂Bz/∂y ≈ ( Bz|edge@j – Bz|edge@j-1 ) / dy
 *
 * This pattern yields **4 cell-center samples per partial derivative** with coefficients ±(1/2)/Δ.
 * Each curl component is a difference of two such partials and typically involves **6 unique**
 * cell centers (some offsets overlap and are combined by `Simplify()`).
 *
 * PROPERTIES
 * ----------
 * • Staggered target (corner) with cell-centered sources (Bx,By,Bz).
 * • Exactly second order: edge averages are O(h²), central differences are O(h²).
 * • Minimal support: 4 points/partial, compact and boundary-friendly.
 * • Works in 3D; trivially reduces in 2D/1D (the “missing” terms vanish).
 *
 * INTERFACE
 * ---------
 * We fill user-provided storage for all three curl components:
 *
 *   struct cCurlBStencil { cStencil Bx, By, Bz; };
 *
 *   // CurlBStencil[0] -> stencils contributing to (curl B)_x
 *   // CurlBStencil[1] -> stencils contributing to (curl B)_y
 *   // CurlBStencil[2] -> stencils contributing to (curl B)_z
 *   void InitCurlBStencils(cCurlBStencil CurlBStencil[3], double dx, double dy, double dz);
 *
 * IMPLEMENTATION NOTES
 * --------------------
 * • We set the **fractional base** of every stencil to the corner: (i-1/2, j-1/2, k-1/2).
 *   That is done via `SetBase(cFrac(-1,2), cFrac(-1,2), cFrac(-1,2))`.
 * • Offsets passed to `add(a, di, dj, dk)` are relative to the integer base (i,j,k).
 * • Coefficients include the proper metric factors 1/(2*dx), etc. Pass your local spacings
 *   if the mesh is non-unit; anisotropy is supported by distinct dx, dy, dz.
 * • After assembling each component’s contributions, we call `Simplify()` to combine
 *   duplicates when partials overlap.
 *
 * EXAMPLE USAGE
 * -------------
 * #include "curlb_stencil_second_order.hpp"
 * using namespace PIC::FieldSolver::ECSIM::Stencil::SecondOrder;
 *
 * // 1) Build the three curl stencils for a given block’s spacing:
 * cCurlBStencil CurlS[3];
 * InitCurlBStencils(CurlS, dx, dy, dz);
 *
 * // 2) Export compact tables (optional) for fast evaluation:
 * cStencil::cStencilData SxBx, SxBy, SxBz;
 * CurlS[0].Bx.ExportStencil(&SxBx);  // Bx contribution to (curl B)_x (empty here by design)
 * CurlS[0].By.ExportStencil(&SxBy);  // By contribution to (curl B)_x  = -∂By/∂z
 * CurlS[0].Bz.ExportStencil(&SxBz);  // Bz contribution to (curl B)_x  = +∂Bz/∂y
 *
 * // 3) Evaluate at integer index (i,j,k) corresponding to the corner (i-1/2,j-1/2,k-1/2):
 * double curlBx = 0.0;
 * for (int n=0; n<SxBy.Length; ++n) {
 *   const int ii = i + SxBy.Data[n].i;
 *   const int jj = j + SxBy.Data[n].j;
 *   const int kk = k + SxBy.Data[n].k;
 *   curlBx += SxBy.Data[n].a * By[INDEX(ii,jj,kk)];
 * }
 * for (int n=0; n<SxBz.Length; ++n) {
 *   const int ii = i + SxBz.Data[n].i;
 *   const int jj = j + SxBz.Data[n].j;
 *   const int kk = k + SxBz.Data[n].k;
 *   curlBx += SxBz.Data[n].a * Bz[INDEX(ii,jj,kk)];
 * }
 * // Repeat similarly for (curl B)_y using CurlS[1].{Bx,Bz} and for (curl B)_z using CurlS[2].{Bx,By}.
 *
 * BOUNDARIES & AMR
 * ----------------
 * This construction is local and compact. When an edge crosses an outer boundary or
 * AMR interface, handle the missing/remote samples per your BC or prolongation rules.
 * The coefficient pattern stays the same; only the data provider changes.
 */

#include "../pic.h"

namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {
namespace SecondOrder {

void InitCurlBStencils_edge_based(PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::cCurlBStencil* CurlBStencilSecondOrder,
                       double dx, double dy, double dz)
{
  // Corner fractional base: (i-1/2, j-1/2, k-1/2)
  const cFrac fi(0,1), fj(0,1), fk(0,1);

  auto set_base = [&](cStencil& S, const char* sym){
    S = cStencil();
    S.SetSymbol(sym);
    S.SetBase(fi,fj,fk);
  };

  // ---------------------------------------------------------------------------
  // Component 0: (curl B)_x = +∂Bz/∂y - ∂By/∂z
  // ---------------------------------------------------------------------------
  {
    PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::cCurlBStencil& C = CurlBStencilSecondOrder[0];

    // Bx: no contribution
    set_base(C.Bx, "curl_Bx_from_Bx");

    // By: -∂By/∂z  via edge-average + central difference (4 points, ±0.5/dz)
    // z-edges meeting the corner:
    //   edge@k   : 0.5*(By[i,  j,  k  ] + By[i-1,j-1,k  ])
    //   edge@k-1 : 0.5*(By[i,  j,  k-1] + By[i-1,j-1,k-1])
    set_base(C.By, "curl_Bx_from_By");
    C.By.add(+0.5/dz,  0,  0,  0);
    C.By.add(+0.5/dz, -1, -1,  0);
    C.By.add(-0.5/dz,  0,  0, -1);
    C.By.add(-0.5/dz, -1, -1, -1);
    C.By *= -1.0; // minus sign from curl definition

    // Bz: +∂Bz/∂y  via edge-average + central difference (4 points, ±0.5/dy)
    // y-edges meeting the corner:
    //   edge@j   : 0.5*(Bz[i,  j,  k  ] + Bz[i-1,j,  k-1])
    //   edge@j-1 : 0.5*(Bz[i,  j-1,k  ] + Bz[i-1,j-1,k-1])
    set_base(C.Bz, "curl_Bx_from_Bz");
    C.Bz.add(+0.5/dy,  0,  0,  0);
    C.Bz.add(+0.5/dy, -1,  0, -1);
    C.Bz.add(-0.5/dy,  0, -1,  0);
    C.Bz.add(-0.5/dy, -1, -1, -1);

    C.Bx.Simplify(); C.By.Simplify(); C.Bz.Simplify();
  }

  // ---------------------------------------------------------------------------
  // Component 1: (curl B)_y = +∂Bx/∂z - ∂Bz/∂x
  // ---------------------------------------------------------------------------
  {
    PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::cCurlBStencil& C = CurlBStencilSecondOrder[1];

    // Bx: +∂Bx/∂z  (4 points, ±0.5/dz)
    // z-edges:
    //   edge@k   : 0.5*(Bx[i,  j,  k  ] + Bx[i-1,j-1,k  ])
    //   edge@k-1 : 0.5*(Bx[i,  j,  k-1] + Bx[i-1,j-1,k-1])
    set_base(C.Bx, "curl_By_from_Bx");
    C.Bx.add(+0.5/dz,  0,  0,  0);
    C.Bx.add(+0.5/dz, -1, -1,  0);
    C.Bx.add(-0.5/dz,  0,  0, -1);
    C.Bx.add(-0.5/dz, -1, -1, -1);

    // By: no contribution
    set_base(C.By, "curl_By_from_By");

    // Bz: -∂Bz/∂x  (4 points, ±0.5/dx)
    // x-edges:
    //   edge@i   : 0.5*(Bz[i,  j,  k  ] + Bz[i,  j-1,k-1])
    //   edge@i-1 : 0.5*(Bz[i-1,j,  k  ] + Bz[i-1,j-1,k-1])
    set_base(C.Bz, "curl_By_from_Bz");
    C.Bz.add(+0.5/dx,  0,  0,  0);
    C.Bz.add(+0.5/dx,  0, -1, -1);
    C.Bz.add(-0.5/dx, -1,  0,  0);
    C.Bz.add(-0.5/dx, -1, -1, -1);
    C.Bz *= -1.0;

    C.Bx.Simplify(); C.By.Simplify(); C.Bz.Simplify();
  }

  // ---------------------------------------------------------------------------
  // Component 2: (curl B)_z = +∂By/∂x - ∂Bx/∂y
  // ---------------------------------------------------------------------------
  {
    PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::cCurlBStencil& C = CurlBStencilSecondOrder[2];

    // Bx: -∂Bx/∂y  (4 points, ±0.5/dy)
    // y-edges:
    //   edge@j   : 0.5*(Bx[i,  j,  k  ] + Bx[i-1,j,  k-1])
    //   edge@j-1 : 0.5*(Bx[i,  j-1,k  ] + Bx[i-1,j-1,k-1])
    set_base(C.Bx, "curl_Bz_from_Bx");
    C.Bx.add(+0.5/dy,  0,  0,  0);
    C.Bx.add(+0.5/dy, -1,  0, -1);
    C.Bx.add(-0.5/dy,  0, -1,  0);
    C.Bx.add(-0.5/dy, -1, -1, -1);
    C.Bx *= -1.0;

    // By: +∂By/∂x  (4 points, ±0.5/dx)
    // x-edges:
    //   edge@i   : 0.5*(By[i,  j,  k  ] + By[i,  j-1,k-1])
    //   edge@i-1 : 0.5*(By[i-1,j,  k  ] + By[i-1,j-1,k-1])
    set_base(C.By, "curl_Bz_from_By");
    C.By.add(+0.5/dx,  0,  0,  0);
    C.By.add(+0.5/dx,  0, -1, -1);
    C.By.add(-0.5/dx, -1,  0,  0);
    C.By.add(-0.5/dx, -1, -1, -1);

    // Bz: no contribution
    set_base(C.Bz, "curl_Bz_from_Bz");

    C.Bx.Simplify(); C.By.Simplify(); C.Bz.Simplify();
  }
}

} // namespace SecondOrder
} // namespace Stencil
} // namespace ECSIM
} // Electromagnetic 
} // namespace FieldSolver
} // namespace PIC


// ============================================================================
// File: curlb_stencil_second_order.cpp  (excerpt)
// Purpose: Initialize second-order curl(B) stencils at cell corners from
//          cell-centered Bx,By,Bz using face-averaging + central differences.
// Notes:
//   • Builds 2×2 face-averages on opposite faces, then differences them,
//     and scales by 1/direction spacing to approximate the derivatives.
//   • Sets the stencil base to the cell-corner (i-1/2, j-1/2, k-1/2).
//   • Compatible with 1D/2D/3D via _MESH_DIMENSION_.
// ============================================================================
namespace PIC {
namespace FieldSolver {
namespace Electromagnetic {
namespace ECSIM {
namespace Stencil {
namespace SecondOrder {

void InitCurlBStencils_face_based(PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::cCurlBStencil* CurlB,
                       double dx, double dy, double dz)
{
    //using PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::cStencil;
    //using PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::cCurlBStencil;

    cCurlBStencil& curlBx = CurlB[0];
    cCurlBStencil& curlBy = CurlB[1];
    cCurlBStencil& curlBz = CurlB[2];

    // Zero outputs
    curlBx.Bx.Reset(); curlBx.By.Reset(); curlBx.Bz.Reset();
    curlBy.Bx.Reset(); curlBy.By.Reset(); curlBy.Bz.Reset();
    curlBz.Bx.Reset(); curlBz.By.Reset(); curlBz.Bz.Reset();

    // ---- Build each min-face 2x2 average ONCE --------------------------------
    cStencil Xmin, Ymin, Zmin;
    {
        // Xmin (i-1/2): average over (j,k) at i-1
        Xmin.Reset();
        const double w = 0.25;
        Xmin.add(w, -1, -1, -1);
        Xmin.add(w, -1, -1,  0);
        Xmin.add(w, -1,  0, -1);
        Xmin.add(w, -1,  0,  0);
        // Ymin (j-1/2): average over (i,k) at j-1
        Ymin.Reset();
        Ymin.add(w, -1, -1, -1);
        Ymin.add(w,  0, -1, -1);
        Ymin.add(w, -1, -1,  0);
        Ymin.add(w,  0, -1,  0);
        // Zmin (k-1/2): average over (i,j) at k-1
        Zmin.Reset();
        Zmin.add(w, -1, -1, -1);
        Zmin.add(w,  0, -1, -1);
        Zmin.add(w, -1,  0, -1);
        Zmin.add(w,  0,  0, -1);
    }

    // Small lambda to form centered derivative from a base min-face stencil:
    // out = ( shift(base, sdi, sdj, sdk) - base ) * scale
    auto centered_diff = [](cStencil& out, const cStencil& base,
                            int sdi, int sdj, int sdk, double scale) {
        cStencil maxf = base;   // copy MIN → MAX
        maxf.shift(sdi, sdj, sdk);

        maxf -= base;           // (MAX - MIN)
        maxf *= scale;
        out = maxf;
    };


    // ============================ curl_x ======================================
    // curl_x = dBz/dy - dBy/dz
    centered_diff(curlBx.Bz, Ymin, 0, +1, 0, 1.0 / dy);  // ∂Bz/∂y  using Y faces
    centered_diff(curlBx.By, Zmin, 0, 0, +1, 1.0 / dz);  //  ∂By/∂z using Z faces
    curlBx.By *= -1.0;                                   // subtract
    curlBx.Bx.Reset();                                   // no Bx term

    // ============================ curl_y ======================================
    // curl_y = dBx/dz - dBz/dx
    centered_diff(curlBy.Bx, Zmin, 0, 0, +1, 1.0 / dz);  // ∂Bx/∂z using Z faces
    centered_diff(curlBy.Bz, Xmin, +1, 0, 0, 1.0 / dx);  // ∂Bz/∂x using X faces
    curlBy.Bz *= -1.0;                                   // subtract
    curlBy.By.Reset();                                   // no By term

    // ============================ curl_z ======================================
    // curl_z = dBy/dx - dBx/dy
    centered_diff(curlBz.By, Xmin, +1, 0, 0, 1.0 / dx);  // ∂By/∂x using X faces
    centered_diff(curlBz.Bx, Ymin, 0, +1, 0, 1.0 / dy);  // ∂Bx/∂y using Y faces
    curlBz.Bx *= -1.0;                                   // subtract
    curlBz.Bz.Reset();                                   // no Bz term


  // Clean up tiny duplicates / combine entries.
  for (int c=0; c<3; ++c) {
    CurlB[c].Bx.Simplify();
    CurlB[c].By.Simplify();
    CurlB[c].Bz.Simplify();

    //cache the composite L∞ radius
    CurlB[c].Radius = std::max({ CurlB[c].Bx.RadiusLinf(), CurlB[c].By.RadiusLinf(), CurlB[c].Bz.RadiusLinf() });
  }
}

} // namespace SecondOrder
} // namespace Stencil
} // namespace ECSIM
} // namespace Electromagnetic
} // namespace FieldSolver
} // namespace PIC

