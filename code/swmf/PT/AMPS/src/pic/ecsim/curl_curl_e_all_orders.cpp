/*==============================================================================
  curl_curl_e_all_orders.cpp
  ------------------------------------------------------------------------------

  PURPOSE
  -------
  Construct the discrete vector operator
      curl_curl(E) = ∇×(∇×E) = ∇(∇·E) − ∇²E
  on a collocated (corner) grid, at 2nd/4th/6th/8th formal order.

  ALGORITHM
  ---------
  This module composes curl_curl from *existing* building blocks:

    1) grad_div_e (vector operator):
       Returns three rows (for Ex,Ey,Ez targets). Each row contains three
       sub-stencils (Ex/Ey/Ez) formed from the pure and mixed second
       derivatives so that the row equals ∇(∇·E) evaluated at the stencil
       location (corners in ECSIM).

    2) laplacian_e (scalar operator, applied component-wise):
       Returns a scalar Laplacian stencil L (Dxx + Dyy + Dzz), which we apply
       to each component of E independently. On uniform grids L.Ex == L.Ey ==
       L.Ez, but we keep component names explicit.

  We then compute, per row r ∈ {x,y,z}:
      CurlCurl[r] = GradDiv[r] − Laplacian applied to the matching component

    r = x:  CurlCurl[0].Ex = GradDiv[0].Ex − L.Ex   (no change to Ey/Ez parts)
    r = y:  CurlCurl[1].Ey = GradDiv[1].Ey − L.Ey
    r = z:  CurlCurl[2].Ez = GradDiv[2].Ez − L.Ez

  Each sub-stencil is simplified after subtraction, and the row's ghost
  requirement (∞-norm radius) is set to max(GradDiv.Radius, Laplacian.Radius)
  for the chosen order.

  COLLLOCATION & BOUNDARIES
  -------------------------
  • Stencils are collocated and location-agnostic; in ECSIM we apply them at
    *corner* nodes for the corner-stored E field.
  • Near physical boundaries you must provide enough ghost layers (radius =
    1/2/3/4 for 2nd/4th/6th/8th) or switch to appropriate one-sided closures.

  SUPPORTED ORDERS & RADII
  ------------------------
     Order     Radius (L∞)     Notes
     -----     ------------     ----------------------------
      2nd           1          7-point Laplacian / 27-point G·Gᵀ core
      4th           2          Wider taps (±2)
      6th           3          Wider taps (±3)
      8th           4          Wider taps (±4)

  NUMERICAL CONSISTENCY
  ---------------------
  • Always build grad_div_e and laplacian_e at the *same* formal order before
    composing curl_curl; mixing orders breaks the identity numerically.
  • The resulting curl_curl operator is the one used in the ECSIM field
    equation:
        (I + a ∇×∇×) E^{n+θ} + M E^{n+θ} = RHS
    with a = (θ c Δt)^2 and M the strictly local particle mass matrix.

  EXAMPLE: Build the 6th-order curl_curl stencil
  ----------------------------------------------
    using namespace PIC::FieldSolver::Electromagnetic::ECSIM::Stencil;

    cCurlCurlEStencil CurlCurl[3];            // 3 rows: target Ex, Ey, Ez
    double dx = ..., dy = ..., dz = ...;

    SixthOrder::InitCurlCurlEStencils(CurlCurl, dx, dy, dz);

    // CurlCurl[0] acts on (Ex,Ey,Ez) to produce (curl curl E)_x at a corner;
    // CurlCurl[1] -> _y; CurlCurl[2] -> _z.
    // CurlCurl[r].Radius is the ghost width needed for row r.

  EXAMPLE: Apply ECSIM operator  (I + a curl_curl) to a local vector E
  --------------------------------------------------------------------
    const double a = sqr(theta * c * dt);

    // For each component row r:
    //   out_r = E_r  +  a * (CurlCurl[r].Ex * Ex  +  CurlCurl[r].Ey * Ey  +  CurlCurl[r].Ez * Ez)
    // where "*" denotes stencil application around the target corner.

    apply_row(out_x, E, CurlCurl[0], a);  // updates Ex-target: uses Ex/Ey/Ez neighbors
    apply_row(out_y, E, CurlCurl[1], a);  // updates Ey-target
    apply_row(out_z, E, CurlCurl[2], a);  // updates Ez-target

    // (Pseudo-utility) apply_row could do:
    //   out_r = E_r;
    //   out_r += a * ( CurlCurl[r].Ex.dot(Ex_neighbors)
    //                + CurlCurl[r].Ey.dot(Ey_neighbors)
    //                + CurlCurl[r].Ez.dot(Ez_neighbors) );

  EXAMPLE: Integrate in a linear-system row assembly
  --------------------------------------------------
    // Inside your row builder for (i,j,k, iVar=r):
    //   1) Add identity: coefficient +1 at (r,r) center offset.
    //   2) For each non-zero in CurlCurl[r].Ex/Ey/Ez:
    //        A += a * weight  at the neighbor corner & component.
    //   3) Keep mass-matrix and RHS terms as before.

  PERFORMANCE NOTES
  -----------------
  • Composition reuses existing optimized builders (grad_div and laplacian),
    avoiding duplicated finite-difference logic.
  • After subtraction, Simplify() compacts zeros and merges duplicates to
    reduce memory traffic in matvecs.

  CAVEATS
  -------
  • On non-uniform grids the laplacian components may differ; keep the explicit
    Ex/Ey/Ez subtraction as written (do not assume equality).
  • Ensure ghost/halo exchange is done before applying the stencil at block
    boundaries, or guard using your block neighbor API.

  ------------------------------------------------------------------------------*/
#include "pic.h"
#include <algorithm>

namespace PIC { namespace FieldSolver { namespace Electromagnetic { namespace ECSIM { namespace Stencil {

namespace {
  inline void ComposeCurlCurlFrom(const cGradDivEStencil G[3],
                                  const cLaplacianStencil& L,
                                  cCurlCurlEStencil* CurlCurl) {
    // Start from grad_div rows
    CurlCurl[0] = G[0];
    CurlCurl[1] = G[1];
    CurlCurl[2] = G[2];

    // Subtract scalar Laplacian from the matching component in each row
    CurlCurl[0].Ex -= L.Ex;  // (curl curl E)_x
    CurlCurl[1].Ey -= L.Ey;  // (curl curl E)_y
    CurlCurl[2].Ez -= L.Ez;  // (curl curl E)_z

    // Simplify sparsity and set ghost width per row
    for (int r = 0; r < 3; ++r) {
      CurlCurl[r].Ex.Simplify();
      CurlCurl[r].Ey.Simplify();
      CurlCurl[r].Ez.Simplify();
      CurlCurl[r].Radius = std::max(G[r].Radius, L.Radius);
    }
  }

  template<class GradDivInitFn, class LapInitFn>
  inline void BuildCurlCurl(GradDivInitFn graddiv_init,
                            LapInitFn      lap_init,
                            cCurlCurlEStencil* CurlCurl,
                            double dx, double dy, double dz) {
    cGradDivEStencil G[3];
    cLaplacianStencil L;
    graddiv_init(G, dx, dy, dz);
    lap_init(&L, dx, dy, dz);
    ComposeCurlCurlFrom(G, L, CurlCurl);
  }
} // anon

// --------------------- 2nd order ---------------------
void SecondOrder::InitCurlCurlEStencils_compact(cCurlCurlEStencil* CurlCurl,
                                                double dx, double dy, double dz) {
  BuildCurlCurl(
    // 1) grad_div initializer: COMPACT
    [](cGradDivEStencil G[3], double dx_, double dy_, double dz_) {
      SecondOrder::InitGradDivEBStencils_compact(G, dx_, dy_, dz_);
    },
    // 2) laplacian initializer (same for compact/wide)
    [](cLaplacianStencil* L, double dx_, double dy_, double dz_) {
      SecondOrder::InitLaplacianStencil(L, dx_, dy_, dz_);
    },
    // out + metrics
    CurlCurl, dx, dy, dz
  );
}

void SecondOrder::InitCurlCurlEStencils_wide(cCurlCurlEStencil* CurlCurl,
                                             double dx, double dy, double dz) {
  BuildCurlCurl(
    // 1) grad_div initializer: WIDE
    [](cGradDivEStencil G[3], double dx_, double dy_, double dz_) {
      SecondOrder::InitGradDivEBStencils_wide(G, dx_, dy_, dz_);
    },
    // 2) laplacian initializer
    [](cLaplacianStencil* L, double dx_, double dy_, double dz_) {
      SecondOrder::InitLaplacianStencil(L, dx_, dy_, dz_);
    },
    // out + metrics
    CurlCurl, dx, dy, dz
  );
}
// --------------------- 4th order ---------------------
void FourthOrder::InitCurlCurlEStencils(cCurlCurlEStencil* CurlCurl,
                                        double dx, double dy, double dz) {
  BuildCurlCurl(
    [](cGradDivEStencil G[3], double dx_, double dy_, double dz_) {
      FourthOrder::InitGradDivEBStencils(G, dx_, dy_, dz_);
    },
    [](cLaplacianStencil* L, double dx_, double dy_, double dz_) {
      FourthOrder::InitLaplacianStencil(L, dx_, dy_, dz_);
    },
    CurlCurl, dx, dy, dz
  );
}

// --------------------- 6th order ---------------------
void SixthOrder::InitCurlCurlEStencils(cCurlCurlEStencil* CurlCurl,
                                       double dx, double dy, double dz) {
  BuildCurlCurl(
    [](cGradDivEStencil G[3], double dx_, double dy_, double dz_) {
      SixthOrder::InitGradDivEBStencils(G, dx_, dy_, dz_);
    },
    [](cLaplacianStencil* L, double dx_, double dy_, double dz_) {
      SixthOrder::InitLaplacianStencil(L, dx_, dy_, dz_);
    },
    CurlCurl, dx, dy, dz
  );
}

// --------------------- 8th order ---------------------
void EighthOrder::InitCurlCurlEStencils(cCurlCurlEStencil* CurlCurl,
                                        double dx, double dy, double dz) {
  BuildCurlCurl(
    [](cGradDivEStencil G[3], double dx_, double dy_, double dz_) {
      EighthOrder::InitGradDivEBStencils(G, dx_, dy_, dz_);
    },
    [](cLaplacianStencil* L, double dx_, double dy_, double dz_) {
      EighthOrder::InitLaplacianStencil(L, dx_, dy_, dz_);
    },
    CurlCurl, dx, dy, dz
  );
}

}}}}} // namespaces

