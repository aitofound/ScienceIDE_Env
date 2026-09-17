// ============================================================================
// swcme3d.cpp
// ----------------------------------------------------------------------------
// SOLAR WIND + CME FORWARD-SHOCK SEMI-ANALYTICAL MODEL
//
// PHYSICS OVERVIEW
// ================
// 1) Upstream density n_up(r): Leblanc–Dulk–Bougeret (1998)
//    n(r) [cm^-3] ≈ 3.3e5 (r/Rs)^(-2) + 4.1e6 (r/Rs)^(-4) + 8.0e7 (r/Rs)^(-6);
//    We rescale these coefficients to match user n(1 AU) and convert to SI.
//    Implementation:
//      n(r) [m^-3] = C2 * r^-2 + C4 * r^-4 + C6 * r^-6,
//    where constants C2,C4,C6 (SI) are cached in StepState.
//
// 2) Upstream Parker spiral B_up(r,θ):
//    Br ∝ r^-2, Bφ = -Br (Ω r sinθ / Vsw). For fixed latitude (sinθ = Param),
//    we define k_AU = Ω AU sinθ / Vsw, and choose Br(1AU) so that
//    |B|(1AU) = B1AU_nT. Implementation caches Br1AU_T and k_AU in StepState.
//    The vector φ-direction is built from (z × u) × u, normalized.
//
// 3) CME apex kinematics: Drag-Based Model (DBM)
//    Let u = V_sh - V_sw be the excess speed. DBM gives
//      u(t) = u0 / (1 + Γ u0 t),   r(t) = r0 + V_sw t + (ln[1 + Γ u0 t])/Γ,
//    where Γ is the drag parameter (converted to SI).
//
// 4) Shock geometry (radius and normal along direction u):
//    • Sphere: Rdir = r_sh, n = u.
//    • Ellipsoid: implicit x^2/a^2 + y^2/b^2 + z^2/c^2 = 1 with a=r_sh;
//      for a ray x=λ u1, y=λ u2, z=λ u3 (in apex frame) => λ = 1 / sqrt(u1^2/a^2+…).
//      The outward normal ∝ (x/a^2, y/b^2, z/c^2); rotate to global.
//    • ConeSSE: inside half-width, Rdir = r_sh cos^m(θ); otherwise clamped thin flank.
//
// 5) Region structure and smoothing (radial blends):
//    Regions: upstream → shock → sheath → leading edge → magnetic ejecta → trailing edge.
//    We blend with a C^1 smoothstep s(x)=x^2(3-2x) applied to signed distances,
//    with independent widths (w_shock, w_le, w_te), each self-similar ∝ r_sh.
//    Sheath compression at the shock uses a shape:
//      rc_loc = rc_floor + (rc_oblique - rc_floor) * (1 - ξ)^p,  ξ = (Rdir - r)/dr_sheath,
//    where p = sheath_ramp_power ≥ 1; rc_oblique from an oblique-MHD proxy.
//
// 6) Oblique-shock proxy (local):
//    Compute θBn (angle between B and local normal), fast speed c_f, and
//    local normal Mach M_f,n = (V_sh,n - V_sw,n) / c_f. If M_f,n>1, the
//    density jump is rc = ((γ+1)M^2)/((γ-1)M^2+2), limited to ≤ 4.
//
// 7) Magnetic field jump:
//    Across the shock, B_n is continuous, B_t scales ≈ rc. We apply a smooth
//    tangential amplification in the sheath proportionally to the (shock) edge
//    smoothstep and ramp shape.
//
// 8) Divergence of V:
//    ∇·V = (1/r^2) d/dr ( r^2 V_r ). We compute it with a robust centered
//    finite difference at r±dr along the same ray, using evaluate_cartesian_fast.
//
// ─────────────────────────────────────────────────────────────────────────────
// SOLAR WIND + CME FORWARD-SHOCK SEMI-ANALYTICAL MODEL
// ─────────────────────────────────────────────────────────────────────────────
//
// What this file provides
// -----------------------
// • A fast 3D kinematic/phenomenological model of a CME-driven forward shock
//   propagating into a Parker-spiral solar wind. The model returns:
//     - plasma number density n [m^-3]
//     - bulk velocity V = (Vx,Vy,Vz) [m/s]
//     - magnetic field B = (Bx,By,Bz) [Tesla], upstream is Parker; tangential
//       field is amplified smoothly within the sheath according to the local
//       compression proxy.
//     - ∇·V (divergence of bulk speed) [1/s] via a robust radial finite-diff.
//     - a triangulated shock surface mesh with nodal normals, nodal rc, nodal
//       normal shock speed, and per-cell metrics (area, rc_mean, Vsh_n_mean,
//       centroid, geometric normals).
// • Tecplot dataset writers for the shock surface and for a structured volume
//   box near the apex (plus a 2-D face zone). Files are NaN/Inf-sanitized.
//
// Headline physics approximations
// -------------------------------
// A) Ambient plasma density n_up(r):
//    Uses Leblanc, Dulk & Bougeret (1998) density law
//       n(r) [cm^-3] = A (Rs/r)^2 + B (Rs/r)^4 + C (Rs/r)^6
//    with A=3.3e5, B=4.1e6, C=8.0e7 and Rs the solar radius. We scale this law
//    so that n(1 AU) matches user parameter n1AU_cm3. Output is SI [m^-3].
//
// B) Magnetic field B_up(r,θ):
//    Parker spiral (Parker 1958, ApJ 128:664).
//    In spherical coordinates (r,θ,φ) with θ measured from rotation axis
//    (ẑ), and assuming azimuthal symmetry of the solar rotation, we use a
//    purely radial Br and toroidal Bφ component:
//       k_AU = Ω⊙ r sinθ / V_sw,  (dimensionless pitch parameter at r)
//       Br(r) = Br(1 AU) (1 AU / r)^2
//       Bφ(r) = -Br(r) * k_AU
//    A desired |B|(1 AU) = B1AU_nT is enforced by setting Br(1 AU) so that
//    sqrt(Br^2 + Bφ^2) = B1AU_nT (converted to Tesla). We construct Cartesian
//    vectors using local spherical radial and azimuthal directions and the
//    given CME propagation latitude via the parameter sin_theta.
//
// C) CME apex kinematics:
//    Drag-Based Model (DBM), Vršnak et al. (2013), Sol. Phys. 285:295.
//       d(V - Vsw)/dt = - Γ (V - Vsw) |V - Vsw|
//    For constant Γ, the closed form solution yields the apex distance and
//    speed as functions of time t:
//       u0 = V0 - Vsw
//       u(t) = u0 / (1 + Γ u0 t)
//       r(t) = r0 + Vsw t + (1/Γ) ln(1 + Γ u0 t)
//    Inputs: r0 = r0_Rs·Rs, V0 (km/s), Vsw (km/s), Γ (km^-1).

// D) Shock shape & direction-dependent speed:
//    Shape options: Sphere, Ellipsoid (axis ratios), Cone (SSE-like).
//    For cones we allow flank slowdown via exponent on cos(θ) to mimic slower
//    expansion off apex. The apex kinematics set an overall scale; the signed
//    normal component of shock speed Vsh_n is projected along the local
//    outward normal.
//
// E) Local compression proxy rc(u, n̂):
//    We estimate oblique fast-mode Mach number M_f,n from upstream cs and vA
//    (Alfvén speed), using the local Parker B direction (θBn angle) and normal
//    component of shock speed Vsh_n relative to solar wind Vsw. Then a
//    hydrodynamic strong-shock formula (Edmiston & Kennel 1984, JPP 32:429;
//    Priest 2014) maps M_f,n to a proxy compression ratio rc, saturated at 4.
//    This is used to smoothly amplify density across the sheath and to
//    increase tangential B.
//
// F) Sheath / ejecta blending (C^1 smoothing):
//    Three radial transitions along a given direction u:
//      1) Upstream → Sheath ramp at shock (width w_shock_m).
//      2) Sheath → Ejecta ramp at the leading edge r_le = r_sh - dr_sheath.
//      3) Ejecta → Downstream ambient at the trailing edge r_te = r_le - dr_me.
//    Each edge uses a smoothstep s(x)=x^2(3-2x) with its own width. Inside the
//    sheath, density transitions toward rc·n_up with a user power p on the
//    (1-xi) factor to allow sharper/softer fronts. Tangential magnetic field
//    is gradually amplified between 1 and rc across the shock and sheath.
//    Ejecta density is a fraction f_ME of n_up (simple cavity or enhancement).
//
// G) Divergence of V:
//    For each point, we compute ∇·V with a robust radial finite difference
//       ∇·V = (1/r^2) ∂(r^2 V_r)/∂r
//    using ±dr around r with dr = max(dr_min, dr_frac · r). We evaluate V at
//    r±dr along the same unit radial direction.
//
// H) Triangulated shock surface & per-cell metrics:
//    The surface is parameterized by (θ,φ) in the apex-aligned frame. We
//    compute nodal positions, nodal unit normals, nodal rc and Vsh_n. Then,
//    per triangle we compute geometric normal, area, centroid, and the mean
//    of (rc, Vsh_n) across the three vertices.

// I) Output (Tecplot)
//    We write a dataset with VARIABLES (common order across all zones):
//      1  X [m], 2 Y [m], 3 Z [m],
//      4  n [m^-3], 5 Vx [m/s], 6 Vy [m/s], 7 Vz [m/s],
//      8  Bx [T], 9 By [T], 10 Bz [T], 11 divVsw [1/s],
//      12 rc [-], 13 Vsh_n [m/s],
//      14 nx [-], 15 ny [-], 16 nz [-],
//      17 area [m^2], 18 rc_mean [-], 19 Vsh_n_mean [m/s],
//      20 tnx [-], 21 tny [-], 22 tnz [-]  (reserved),
//      23 cx [m], 24 cy [m], 25 cz [m]     (triangle centroids)
//
// Tecplot VARIABLES (global order across all zones)
// -------------------------------------------------
//  1  "X"       [m]   : position
//  2  "Y"       [m]
//  3  "Z"       [m]
//  4  "n"     [m^-3]  : density
//  5  "Vx"    [m/s]   : bulk velocity
//  6  "Vy"    [m/s]
//  7  "Vz"    [m/s]
//  8  "Bx"      [T]   : magnetic field (Parker upstream; Bt amplified in sheath)
//  9  "By"      [T]
// 10  "Bz"      [T]
// 11  "divVsw" [1/s]  : divergence of V (radial FD)
// 12  "rc"      [-]   : compression ratio (nodal on surface zones)
// 13  "Vsh_n"  [m/s]  : normal shock speed (nodal on surface zones)
// 14  "nx"      [-]   : triangle/geometric normal (cell-centered in surface_cells)
// 15  "ny"      [-]
// 16  "nz"      [-]
// 17  "area"   [m^2]  : triangle area (cell-centered in surface_cells)
// 18  "rc_mean"[-]    : mean nodal rc over each triangle (cell-centered)
// 19  "Vsh_n_mean"[m/s]: mean nodal Vsh_n over each triangle (cell-centered)
// 20  "tnx"     [-]   : reserved (0)
// 21  "tny"     [-]
// 22  "tnz"     [-]
// 23  "cx"      [m]   : triangle centroid X (cell-centered)
// 24  "cy"      [m]
// 25  "cz"      [m]

//    Zones:
//      • surface_cells  : FETRIANGLE/BLOCK, with cell-centered metrics
//                         (area, rc_mean, Vsh_n_mean, nx,ny,nz, cx,cy,cz)
//                         and nodal rc, Vsh_n at vertices. This zone appears
//                         first so Tecplot shows non-zero cell metrics by default.

//      • volume_box     : Structured POINT, 3D grid around shock apex.
//      • box_face_minX  : Structured POINT (2D), the plane x = minX of the box.
//    All numeric outputs are passed through finite_or(...) to avoid NaN/Inf.
//
// J) References (short list)
//    • Parker, E.N. (1958), ApJ 128, 664 — Parker spiral magnetic field.
//    • Leblanc, Y.; Dulk, G.A.; Bougeret, J.-L. (1998), Sol. Phys. 183, 165 —
//      empirical coronal/solar-wind density profile.
//    • Vršnak, B. et al. (2013), Sol. Phys. 285, 295 — Drag-based CME propagation.
//    • Edmiston, J.P.; Kennel, C.F. (1984), J. Plasma Phys. 32, 429 — Oblique
//      MHD shock jump conditions (used here as a proxy).
//    • Priest, E. (2014), “Magnetohydrodynamics of the Sun”, CUP — background.
//    • Russell, C.T.; Mulligan, T. (2002), Planet. Space Sci. 50, 527 — CME
//      sheath/ICME structure and in situ signatures.
//    • Manchester, W.B. IV et al. (2005), ApJ 622, 1225 — CME-driven shocks.
//
// Build
// -----
//   g++ -std=c++17 -O3 -march=native demo3d_2.cpp swcme3d.cpp -o demo
//
// Header pairing
// --------------
// This file matches the earlier swcme3d.hpp you’re using. It defines AU, Rs,
// PI in the swcme3d namespace (giving them external linkage so other TUs like
// demo3d_2.cpp can reference them), and implements all methods declared there.
//
// ─────────────────────────────────────────────────────────────────────────────


//
// I/O & VARIABLES (Tecplot):
// --------------------------
// VARIABLES (fixed order across all writers):
//   1:X [m], 2:Y [m], 3:Z [m],
//   4:n [m^-3], 5:Vx [m/s], 6:Vy [m/s], 7:Vz [m/s],
//   8:Bx [T], 9:By [T], 10:Bz [T], 11:divVsw [1/s],
//   12:rc [-], 13:Vsh_n [m/s],
//   14:nx [-], 15:ny [-], 16:nz [-],         // cell geometric normal
//   17:area [m^2], 18:rc_mean [-], 19:Vsh_n_mean [m/s],
//   20:tnx [-], 21:tny [-], 22:tnz [-],      // reserved (unused)
//   23:cx [m], 24:cy [m], 25:cz [m]          // cell centroid
//
// SANITIZATION:
//  • All numeric outputs use finite_or(v, fallback) before printing
//    to avoid NaN/Inf in Tecplot files.
//
// PERFORMANCE NOTES:
//  • StepState caches Leblanc coefficients, Parker constants, and geometry.
//  • No std::pow in hot loops; replaces with multiplies on r^-2, etc.
//  • Smooth edges use cached inv(2w). Vectorization hint via GCC ivdep.
//  • No heap allocations in evaluators; all work is per-point arithmetic.
//
// ----------------------------------------------------------------------------

#include "swcme3d.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>

// Vectorization hint (safe: no loop-carried deps)
#if defined(__GNUC__)
  #define SWCME_IVDEP _Pragma("GCC ivdep")
#else
  #define SWCME_IVDEP
#endif

namespace swcme3d {
  // Physical constants (exported)
  const double AU = 1.495978707e11;
  const double Rs = 6.957e8;
  const double PI = 3.141592653589793;

  // Small helper (in namespace to avoid header pollution)
  inline double clamp01(double v){ return (v<0.0)?0.0:(v>1.0?1.0:v); }
}

// --- file-local helpers ------------------------------------------------------
static inline double finite_or(double v, double fallback=0.0){
  return std::isfinite(v)? v : fallback;
}
static inline void safe_normalize(double v[3]){
  const double m=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  if (m>0){ v[0]/=m; v[1]/=m; v[2]/=m; } else { v[0]=1; v[1]=0; v[2]=0; }
}
static const double MU0 = 4.0e-7*swcme3d::PI;   // μ0 [H/m]
static constexpr double MP  = 1.67262192369e-27; // proton mass [kg]
static constexpr double KB  = 1.380649e-23;      // Boltzmann const [J/K]
static constexpr double OMEGA_SUN = 2.86533e-6;  // solar sidereal rotation [rad/s]

static inline double smoothstep01(double x){
  if (x<=0) return 0;
  if (x>=1) return 1;
  return x*x*(3-2*x);
}

// Efficient Parker spiral using cached Br1AU_T and k_AU
static inline void parker_vec_T_fast(const swcme3d::StepState& S,
                                     const double u[3], double r_m,
                                     double B_out[3]){
  using namespace swcme3d;
  const double r_AU = r_m / AU;

  // Br = Br1AU / r_AU^2; Bphi = -Br * k_AU * r_AU
  const double Br   = S.Br1AU_T / (r_AU*r_AU);
  const double Bphi = - Br * S.k_AU * r_AU;

  // φ-direction from (z × u) × u
  const double zhat[3]={0,0,1};
  double zxu[3] = { zhat[1]*u[2]-zhat[2]*u[1],
                    zhat[2]*u[0]-zhat[0]*u[2],
                    zhat[0]*u[1]-zhat[1]*u[0] };
  double ph[3] = { zxu[1]*u[2]-zxu[2]*u[1],
                   zxu[2]*u[0]-zxu[0]*u[2],
                   zxu[0]*u[1]-zxu[1]*u[0] };
  safe_normalize(ph);

  B_out[0]=Br*u[0]+Bphi*ph[0];
  B_out[1]=Br*u[1]+Bphi*ph[1];
  B_out[2]=Br*u[2]+Bphi*ph[2];
}

// ----------------------------------------------------------------------------
// Model implementation
// ----------------------------------------------------------------------------
namespace swcme3d {

Model::Model(const Params& P): P_(P) {}

StepState Model::prepare_step(double t_s) const {
  StepState S{};

  // 1) Apex-aligned orthonormal basis (e1 along CME apex direction)
  double e1[3]={P_.cme_dir[0],P_.cme_dir[1],P_.cme_dir[2]}; ::safe_normalize(e1);
  double tmp[3]={0,0,1}; if (std::fabs(e1[2])>0.9){ tmp[0]=1; tmp[1]=0; tmp[2]=0; }
  double e2[3]={ e1[1]*tmp[2]-e1[2]*tmp[1],
                 e1[2]*tmp[0]-e1[0]*tmp[2],
                 e1[0]*tmp[1]-e1[1]*tmp[0] }; ::safe_normalize(e2);
  double e3[3]={ e1[1]*e2[2]-e1[2]*e2[1],
                 e1[2]*e2[0]-e1[0]*e2[2],
                 e1[0]*e2[1]-e1[1]*e2[0] }; ::safe_normalize(e3);
  S.e1[0]=e1[0]; S.e1[1]=e1[1]; S.e1[2]=e1[2];
  S.e2[0]=e2[0]; S.e2[1]=e2[1]; S.e2[2]=e2[2];
  S.e3[0]=e3[0]; S.e3[1]=e3[1]; S.e3[2]=e3[2];

  // 2) DBM apex kinematics
  const double r0_m   = P_.r0_Rs * Rs;
  S.V_sw_ms           = P_.V_sw_kms*1e3;
  const double V0_ms  = P_.V0_sh_kms*1e3;
  const double Ginv_m = P_.Gamma_kmInv/1e3; // [1/m]
  double r_sh=0.0, V_sh=0.0;
  {
    const double u0 = V0_ms - S.V_sw_ms;
    const double den= 1.0 + Ginv_m*u0*t_s;
    const double u  = (den!=0.0)? (u0/den) : 0.0;
    r_sh = r0_m + S.V_sw_ms*t_s + std::log(std::max(den,1e-30))/Ginv_m;
    V_sh = S.V_sw_ms + u;
  }
  S.r_sh_m = finite_or(r_sh, r0_m);
  S.V_sh_ms= finite_or(V_sh, V0_ms);
  S.a_m    = S.r_sh_m;

  // 3) Self-similar region widths & derived radii
  const double scaleR = S.r_sh_m / AU;
  S.dr_sheath_m = finite_or(P_.sheath_thick_AU_at1AU * scaleR * AU);
  S.dr_me_m     = finite_or(P_.ejecta_thick_AU_at1AU * scaleR * AU);
  S.w_shock_m   = finite_or(P_.edge_smooth_shock_AU_at1AU * scaleR * AU);
  S.w_le_m      = finite_or(P_.edge_smooth_le_AU_at1AU    * scaleR * AU);
  S.w_te_m      = finite_or(P_.edge_smooth_te_AU_at1AU    * scaleR * AU);
  S.r_le_m = S.r_sh_m - S.dr_sheath_m;
  S.r_te_m = S.r_le_m - S.dr_me_m;

  // Smoothstep helpers
  S.inv2w_sh = (S.w_shock_m>0.0)? 0.5/S.w_shock_m : 0.0;
  S.inv2w_le = (S.w_le_m   >0.0)? 0.5/S.w_le_m    : 0.0;
  S.inv2w_te = (S.w_te_m   >0.0)? 0.5/S.w_te_m    : 0.0;

  // 4) Region target speeds
  S.V_sheath_LE_ms = finite_or(P_.V_sheath_LE_factor * S.V_sw_ms, S.V_sw_ms);
  S.V_ME_ms        = finite_or(P_.V_ME_factor        * S.V_sw_ms, S.V_sw_ms);
  S.V_dn_ms        = S.V_sw_ms;

  // 5) Convenience
  S.inv_dr_sheath  = (S.dr_sheath_m>0.0)? 1.0/S.dr_sheath_m : 0.0;
  S.rc_floor       = (P_.sheath_comp_floor>1.0)? P_.sheath_comp_floor : 1.0;

  // 6) Leblanc coefficients in SI (cached)
  {
    const double A=3.3e5, B=4.1e6, C=8.0e7; // cm^-3 coefficients
    const double Rs2=Rs*Rs, Rs4=Rs2*Rs2, Rs6=Rs4*Rs2;
    const double sAU=Rs/AU;
    const double n1_base=A*sAU*sAU + B*std::pow(sAU,4) + C*std::pow(sAU,6);
    const double leb_scale=(n1_base>0.0)? (P_.n1AU_cm3/n1_base):1.0;
    const double K = leb_scale*1e6; // cm^-3 → m^-3
    S.C2 = K*A*Rs2;
    S.C4 = K*B*Rs4;
    S.C6 = K*C*Rs6;
  }

  // 7) Parker constants for this step
  {
    const double B1AU_T = P_.B1AU_nT*1e-9;
    S.k_AU   = (OMEGA_SUN * AU * P_.sin_theta) / S.V_sw_ms;
    S.Br1AU_T= B1AU_T / std::sqrt(1.0 + S.k_AU*S.k_AU);
  }

  // 8) Geometry caches
  if (P_.shape==ShockShape::Ellipsoid){
    S.a_e    = S.r_sh_m;
    S.b_e    = S.a_e * std::max(1e-3, P_.axis_ratio_y);
    S.c_e    = S.a_e * std::max(1e-3, P_.axis_ratio_z);
    const double a2=S.a_e*S.a_e, b2=S.b_e*S.b_e, c2=S.c_e*S.c_e;
    S.inv_a2=(a2>0)?1.0/a2:0.0; S.inv_b2=(b2>0)?1.0/b2:0.0; S.inv_c2=(c2>0)?1.0/c2:0.0;
  } else if (P_.shape==ShockShape::ConeSSE){
    S.cos_half_width = std::cos(P_.half_width_rad);
    S.flank_m        = std::max(0.0, P_.flank_slowdown_m);
  }

  // 9) Apex diagnostic rc
  {
    double n_hat_apex[3]={e1[0],e1[1],e1[2]};
    double u_apex[3]    ={e1[0],e1[1],e1[2]};
    double rc_ap=1.0,Vn=0.0,th=0.0;
    local_oblique_rc(S,u_apex,n_hat_apex,S.r_sh_m,S.r_sh_m,rc_ap,Vn,th);
    S.rc = finite_or(rc_ap,1.0);
  }
  return S;
}

// Radius and normal along direction (ux,uy,uz)
void Model::shape_radius_normal(const StepState& S,
                                double ux,double uy,double uz,
                                double& Rdir_m,double n_hat[3]) const {
  double u[3]={ux,uy,uz}; ::safe_normalize(u);

  // Decompose in apex frame
  const double u1=u[0]*S.e1[0]+u[1]*S.e1[1]+u[2]*S.e1[2];
  const double u2=u[0]*S.e2[0]+u[1]*S.e2[1]+u[2]*S.e2[2];
  const double u3=u[0]*S.e3[0]+u[1]*S.e3[1]+u[2]*S.e3[2];

  switch(P_.shape){
    case ShockShape::Sphere:{
      Rdir_m=S.r_sh_m; n_hat[0]=u[0]; n_hat[1]=u[1]; n_hat[2]=u[2];
    } break;
    case ShockShape::Ellipsoid:{
      const double denom=(u1*u1)*S.inv_a2+(u2*u2)*S.inv_b2+(u3*u3)*S.inv_c2;
      const double lam=(denom>0)? 1.0/std::sqrt(denom):0.0;
      Rdir_m=lam;
      const double x=lam*u1,y=lam*u2,z=lam*u3;
      double nloc[3]={x*S.inv_a2,y*S.inv_b2,z*S.inv_c2};
      double ng[3]={ nloc[0]*S.e1[0]+nloc[1]*S.e2[0]+nloc[2]*S.e3[0],
                     nloc[0]*S.e1[1]+nloc[1]*S.e2[1]+nloc[2]*S.e3[1],
                     nloc[0]*S.e1[2]+nloc[1]*S.e2[2]+nloc[2]*S.e3[2] };
      ::safe_normalize(ng);
      n_hat[0]=ng[0]; n_hat[1]=ng[1]; n_hat[2]=ng[2];
    } break;
    case ShockShape::ConeSSE:{
      const double theta=std::acos(std::max(-1.0,std::min(1.0,u1)));
      double R=0.0;
      if (theta>P_.half_width_rad){
        R=S.r_sh_m*std::pow(std::max(0.0,S.cos_half_width), S.flank_m);
      } else {
        const double c=std::max(0.0, std::cos(theta));
        R=S.r_sh_m*std::pow(c, S.flank_m);
      }
      Rdir_m=R; n_hat[0]=u[0]; n_hat[1]=u[1]; n_hat[2]=u[2];
    } break;
  }
}

// Local oblique MHD proxy: returns rc, Vsh_n, thetaBn
void Model::local_oblique_rc(const StepState& S, const double u[3], const double n_hat[3],
                             double /*Rdir_m*/, double r_eval_m,
                             double& rc_out, double& Vsh_n_out, double& thetaBn_out) const {
  const double Vsw=S.V_sw_ms;

  // Upstream n and mass density
  const double r=std::max(r_eval_m, 1.05*Rs);
  const double r2=r*r, inv2=1.0/r2, inv4=inv2*inv2, inv6=inv4*inv2;
  const double n_up_m3=finite_or(S.C2*inv2 + S.C4*inv4 + S.C6*inv6, 1e6);
  const double rho=std::max(1e-12,n_up_m3)*MP;

  // Sound speed, upstream B, Alfven speed, θBn
  const double cs=std::sqrt(std::max(0.0,P_.gamma_ad)*KB*std::max(0.0,P_.T_K)/MP);
  double B_up[3]; ::parker_vec_T_fast(S,u,r,B_up);
  const double Bmag=std::sqrt(std::max(0.0,B_up[0]*B_up[0]+B_up[1]*B_up[1]+B_up[2]*B_up[2]));
  const double vA=(rho>0.0)? (Bmag/std::sqrt(MU0*rho)):0.0;

  double b_hat[3]={0,0,0}; if (Bmag>0){ b_hat[0]=B_up[0]/Bmag; b_hat[1]=B_up[1]/Bmag; b_hat[2]=B_up[2]/Bmag; }
  const double cosBn=std::fabs(b_hat[0]*n_hat[0]+b_hat[1]*n_hat[1]+b_hat[2]*n_hat[2]);
  thetaBn_out=finite_or(std::acos(std::max(-1.0,std::min(1.0,cosBn))),0.0);

  // Fast speed
  const double a=vA*vA+cs*cs;
  const double disc=std::max(0.0, a*a - 4.0*cs*cs*vA*vA*cosBn*cosBn);
  const double cf=std::sqrt(0.5*(a+std::sqrt(disc)));

  // Local shock normal speed (shape-dependent)
  double Vsh_dir=S.V_sh_ms;
  if (P_.shape==ShockShape::ConeSSE){
    const double u1=u[0]*S.e1[0]+u[1]*S.e1[1]+u[2]*S.e1[2];
    Vsh_dir=S.V_sh_ms*std::pow(std::max(0.0,u1), std::max(0.0,P_.flank_slowdown_m));
  }
  Vsh_n_out=finite_or(Vsh_dir*(n_hat[0]*u[0]+n_hat[1]*u[1]+n_hat[2]*u[2]),0.0);

  // Upstream normal flow relative to the shock
  const double Vsw_n=Vsw*(u[0]*n_hat[0]+u[1]*n_hat[1]+u[2]*n_hat[2]);
  const double U1n = std::max(0.0, Vsh_n_out - Vsw_n);
  const double Mfn = (cf>0.0)? (U1n/cf):0.0;

  double rc=1.0;
  if (Mfn>1.0){
    const double g=std::max(1.01,P_.gamma_ad), M2=Mfn*Mfn;
    rc=((g+1.0)*M2)/((g-1.0)*M2+2.0);
    if (rc>4.0) rc=4.0;
  }
  rc_out=finite_or(rc,1.0);
}

// n, V evaluator (allocation-free; vectorization-friendly)
void Model::evaluate_cartesian_fast(const StepState& S,
                                    const double* x_m,const double* y_m,const double* z_m,
                                    double* n_m3,double* Vx_ms,double* Vy_ms,double* Vz_ms,
                                    std::size_t N) const {
  const double Vup=S.V_sw_ms;
  const double dr_sheath=S.dr_sheath_m, dr_me=S.dr_me_m;

  SWCME_IVDEP
  for (std::size_t i=0;i<N;++i){
    const double x=x_m[i], y=y_m[i], z=z_m[i];
    const double r2=std::max(1e-12, x*x+y*y+z*z);
    const double r =std::sqrt(r2);
    const double invr=1.0/r;
    double u[3]={x*invr,y*invr,z*invr};

    double Rdir=0.0,n_hat[3]={0,0,1}; shape_radius_normal(S,u[0],u[1],u[2],Rdir,n_hat);

    // Upstream density
    const double inv2=1.0/r2, inv4=inv2*inv2, inv6=inv4*inv2;
    const double n_up=finite_or(S.C2*inv2 + S.C4*inv4 + S.C6*inv6,1e6);

    // Local shock proxies
    double rc_loc=1.0,Vsh_n=0.0,dum=0.0;
    local_oblique_rc(S,u,n_hat,Rdir,(r>Rdir?r:Rdir),rc_loc,Vsh_n,dum);

    // Sheath ramp
    const double xi=(dr_sheath>0.0)? swcme3d::clamp01((Rdir-r)*S.inv_dr_sheath):0.0;
    const double p=(P_.sheath_ramp_power<1.0)? 1.0 : P_.sheath_ramp_power;
    const double ramp=(p==1.0)? (1.0-xi): std::pow(1.0-xi,p);
    const double Csheath=S.rc_floor + (rc_loc-S.rc_floor)*ramp;

    // Region targets
    const double n_sheath=Csheath*n_up;
    const double V_sheath=Vup + (S.V_sheath_LE_ms - Vup)*xi;
    const double n_ejecta=(P_.f_ME>0.0? P_.f_ME:0.0)*n_up;
    const double V_ejecta=S.V_ME_ms;

    // Blend helper (C^1 smoothstep)
    auto edge=[&](double rnow,double r0,double inv2w)->double{
      if (inv2w<=0.0) return (rnow<=r0)? 1.0:0.0;
      return smoothstep01( swcme3d::clamp01(0.5 + (r0-rnow)*inv2w) );
    };

    // upstream → sheath
    double a=(rc_loc>1.0)? edge(r,Rdir,S.inv2w_sh):0.0;
    double n_mix=(1.0-a)*n_up + a*n_sheath;
    double Vmag =(1.0-a)*Vup  + a*V_sheath;

    // sheath → ejecta
    a=(rc_loc>1.0)? edge(r,Rdir-dr_sheath,S.inv2w_le):0.0;
    n_mix=(1.0-a)*n_mix + a*n_ejecta;
    Vmag =(1.0-a)*Vmag + a*V_ejecta;

    // ejecta → downstream
    a=(rc_loc>1.0)? edge(r,Rdir-dr_sheath-dr_me,S.inv2w_te):0.0;
    n_mix=(1.0-a)*n_mix + a*n_up;
    Vmag =(1.0-a)*Vmag + a*Vup;

    // Output (radial flow)
    n_m3[i]=finite_or(n_mix,n_up);
    Vx_ms[i]=finite_or(Vmag*u[0],0.0);
    Vy_ms[i]=finite_or(Vmag*u[1],0.0);
    Vz_ms[i]=finite_or(Vmag*u[2],0.0);
  }
}

// n, V, B evaluator
void Model::evaluate_cartesian_with_B(const StepState& S,
  const double* x_m,const double* y_m,const double* z_m,
  double* n_m3,double* Vx_ms,double* Vy_ms,double* Vz_ms,
  double* Bx_T,double* By_T,double* Bz_T,
  std::size_t N) const {

  const double Vup=S.V_sw_ms;
  const double dr_sheath=S.dr_sheath_m, dr_me=S.dr_me_m;

  SWCME_IVDEP
  for (std::size_t i=0;i<N;++i){
    const double x=x_m[i], y=y_m[i], z=z_m[i];
    const double r2=std::max(1e-12, x*x+y*y+z*z);
    const double r =std::sqrt(r2);
    const double invr=1.0/r;
    double u[3]={x*invr,y*invr,z*invr};

    double Rdir=0.0,n_hat[3]={0,0,1}; shape_radius_normal(S,u[0],u[1],u[2],Rdir,n_hat);

    const double inv2=1.0/r2, inv4=inv2*inv2, inv6=inv4*inv2;
    const double n_up=finite_or(S.C2*inv2 + S.C4*inv4 + S.C6*inv6,1e6);

    double B_up[3]; ::parker_vec_T_fast(S,u,r,B_up);

    double rc_loc=1.0,Vsh_n=0.0,dum=0.0;
    local_oblique_rc(S,u,n_hat,Rdir,(r>Rdir?r:Rdir),rc_loc,Vsh_n,dum);

    const double xi=(dr_sheath>0.0)? swcme3d::clamp01((Rdir-r)*S.inv_dr_sheath):0.0;
    const double p=(P_.sheath_ramp_power<1.0)? 1.0 : P_.sheath_ramp_power;
    const double ramp=(p==1.0)? (1.0-xi): std::pow(1.0-xi,p);
    const double Csheath=S.rc_floor + (rc_loc-S.rc_floor)*ramp;

    const double n_sheath=Csheath*n_up;
    const double V_sheath=Vup + (S.V_sheath_LE_ms - Vup)*xi;
    const double n_ejecta=(P_.f_ME>0.0? P_.f_ME:0.0)*n_up;
    const double V_ejecta=S.V_ME_ms;

    auto edge=[&](double rnow,double r0,double inv2w)->double{
      if (inv2w<=0.0) return (rnow<=r0)? 1.0:0.0;
      return smoothstep01( swcme3d::clamp01(0.5 + (r0-rnow)*inv2w) );
    };

    double a=(rc_loc>1.0)? edge(r,Rdir,S.inv2w_sh):0.0;
    double n_mix=(1.0-a)*n_up + a*n_sheath;
    double Vmag =(1.0-a)*Vup  + a*V_sheath;

    a=(rc_loc>1.0)? edge(r,Rdir-dr_sheath,S.inv2w_le):0.0;
    n_mix=(1.0-a)*n_mix + a*n_ejecta;
    Vmag =(1.0-a)*Vmag + a*V_ejecta;

    a=(rc_loc>1.0)? edge(r,Rdir-dr_sheath-dr_me,S.inv2w_te):0.0;
    n_mix=(1.0-a)*n_mix + a*n_up;
    Vmag =(1.0-a)*Vmag + a*Vup;

    // Magnetic jump model: Bn continuous, Bt amplified ~ rc in sheath
    const double Bn_mag=B_up[0]*n_hat[0]+B_up[1]*n_hat[1]+B_up[2]*n_hat[2];
    double Bn[3]={Bn_mag*n_hat[0],Bn_mag*n_hat[1],Bn_mag*n_hat[2]};
    double Bt[3]={B_up[0]-Bn[0],B_up[1]-Bn[1],B_up[2]-Bn[2]};
    const double a_shock=(rc_loc>1.0)? edge(r,Rdir,S.inv2w_sh):0.0;
    const double amp_t=1.0 + (rc_loc-1.0)*a_shock*ramp;
    double Bv[3]={Bn[0]+amp_t*Bt[0],Bn[1]+amp_t*Bt[1],Bn[2]+amp_t*Bt[2]};

    n_m3[i]=finite_or(n_mix,n_up);
    Vx_ms[i]=finite_or(Vmag*u[0],0.0);
    Vy_ms[i]=finite_or(Vmag*u[1],0.0);
    Vz_ms[i]=finite_or(Vmag*u[2],0.0);
    Bx_T[i] =finite_or(Bv[0],0.0);
    By_T[i] =finite_or(Bv[1],0.0);
    Bz_T[i] =finite_or(Bv[2],0.0);
  }
}

void Model::evaluate_cartesian_with_B_div(const StepState& S,
  const double* x_m,const double* y_m,const double* z_m,
  double* n_m3,double* Vx_ms,double* Vy_ms,double* Vz_ms,
  double* Bx_T,double* By_T,double* Bz_T,double* divVsw,
  std::size_t N,double dr_frac) const {
  evaluate_cartesian_with_B(S,x_m,y_m,z_m,n_m3,Vx_ms,Vy_ms,Vz_ms,Bx_T,By_T,Bz_T,N);
  compute_divV_radial(S,x_m,y_m,z_m,divVsw,N,dr_frac);
}

// Robust radial divergence via (1/r^2) d(r^2 V_r)/dr
void Model::compute_divV_radial(const StepState& S,
  const double* x_m,const double* y_m,const double* z_m,
  double* divV,std::size_t N,double dr_frac) const {
  const double rmin=1.05*Rs, dr_min=1.0e-4*AU;

  SWCME_IVDEP
  for (std::size_t i=0;i<N;++i){
    const double x=x_m[i], y=y_m[i], z=z_m[i];
    const double r2=std::max(1e-12, x*x+y*y+z*z);
    const double r =std::sqrt(r2);
    const double invr=1.0/r;
    const double u[3]={x*invr,y*invr,z*invr};
    const double dr=std::max(dr_min, dr_frac*r);
    const double rp=std::max(rmin,r+dr), rm=std::max(rmin,r-dr);
    const double denom_r=(rp>rm)? (rp-rm): std::max(dr_min,std::abs(dr));

    double xp=rp*u[0], yp=rp*u[1], zp=rp*u[2];
    double xm=rm*u[0], ym=rm*u[1], zm=rm*u[2];
    double n,Vxp,Vyp,Vzp,Vxm,Vym,Vzm;
    evaluate_cartesian_fast(S,&xp,&yp,&zp,&n,&Vxp,&Vyp,&Vzp,1);
    evaluate_cartesian_fast(S,&xm,&ym,&zm,&n,&Vxm,&Vym,&Vzm,1);

    const double Vrp=Vxp*u[0]+Vyp*u[1]+Vzp*u[2];
    const double Vrm=Vxm*u[0]+Vym*u[1]+Vzm*u[2];

    const double num=((rp*rp)*Vrp - (rm*rm)*Vrm)/denom_r;
    const double div_val=num/(r*r);
    divV[i]=finite_or(div_val,0.0);
  }
}

void Model::diagnose_direction(const StepState& S,const double u[3],
  double& Rdir_m,double n_hat[3],double& rc_loc,double& Vsh_n) const {
  shape_radius_normal(S,u[0],u[1],u[2],Rdir_m,n_hat);
  double th=0.0; local_oblique_rc(S,u,n_hat,Rdir_m,Rdir_m,rc_loc,Vsh_n,th);
}

// Build lat–lon mesh on [0,thetaMax]×[0,2π]
ShockMesh Model::build_shock_mesh(const StepState& S,std::size_t nTheta,std::size_t nPhi) const {
  ShockMesh M; if (nTheta<3) nTheta=3; if (nPhi<3) nPhi=3;
  const double PI=swcme3d::PI;
  const double thetaMax=(P_.shape==ShockShape::ConeSSE)? P_.half_width_rad : PI;

  for (std::size_t it=0; it<=nTheta; ++it){
    const double t=thetaMax*(double(it)/double(nTheta));
    const double ct=std::cos(t), st=std::sin(t);
    for (std::size_t ip=0; ip<=nPhi; ++ip){
      const double p=2.0*PI*(double(ip)/double(nPhi)), cp=std::cos(p), sp=std::sin(p);
      double u_loc[3]={ct, st*cp, st*sp};
      double u[3]={ u_loc[0]*S.e1[0]+u_loc[1]*S.e2[0]+u_loc[2]*S.e3[0],
                    u_loc[0]*S.e1[1]+u_loc[1]*S.e2[1]+u_loc[2]*S.e3[1],
                    u_loc[0]*S.e1[2]+u_loc[1]*S.e2[2]+u_loc[2]*S.e3[2] };
      ::safe_normalize(u);

      double Rdir=0.0,n_hat[3]={0,0,1}; shape_radius_normal(S,u[0],u[1],u[2],Rdir,n_hat);

      M.x.push_back(finite_or(Rdir*u[0],0.0));
      M.y.push_back(finite_or(Rdir*u[1],0.0));
      M.z.push_back(finite_or(Rdir*u[2],0.0));
      M.n_hat_x.push_back(finite_or(n_hat[0],0.0));
      M.n_hat_y.push_back(finite_or(n_hat[1],0.0));
      M.n_hat_z.push_back(finite_or(n_hat[2],1.0));

      double rc_loc=1.0,Vsh_n=0.0,th=0.0;
      local_oblique_rc(S,u,n_hat,Rdir,Rdir,rc_loc,Vsh_n,th);
      M.rc.push_back(finite_or(rc_loc,1.0));
      M.Vsh_n.push_back(finite_or(Vsh_n,0.0));
    }
  }
  const std::size_t NvPhi=nPhi+1;
  auto idx=[&](std::size_t it,std::size_t ip){ return int(it*NvPhi+ip); };
  for (std::size_t it=0; it<nTheta; ++it){
    for (std::size_t ip=0; ip<nPhi; ++ip){
      int i00=idx(it,ip), i01=idx(it,ip+1), i10=idx(it+1,ip), i11=idx(it+1,ip+1);
      M.tri_i.push_back(i00+1); M.tri_j.push_back(i10+1); M.tri_k.push_back(i11+1);
      M.tri_i.push_back(i00+1); M.tri_j.push_back(i11+1); M.tri_k.push_back(i01+1);
    }
  }
  return M;
}

void Model::compute_triangle_metrics(const ShockMesh& M, TriMetrics& T) const {
  const std::size_t Ne=M.tri_i.size();
  T.area.assign(Ne,0.0); T.nx.assign(Ne,0.0); T.ny.assign(Ne,0.0); T.nz.assign(Ne,1.0);
  T.cx.assign(Ne,0.0); T.cy.assign(Ne,0.0); T.cz.assign(Ne,0.0);
  T.rc_mean.assign(Ne,1.0); T.Vsh_n_mean.assign(Ne,0.0);

  for (std::size_t e=0;e<Ne;++e){
    int ia=M.tri_i[e]-1, ib=M.tri_j[e]-1, ic=M.tri_k[e]-1;
    double Ax=M.x[ia], Ay=M.y[ia], Az=M.z[ia];
    double Bx=M.x[ib], By=M.y[ib], Bz=M.z[ib];
    double Cx=M.x[ic], Cy=M.y[ic], Cz=M.z[ic];

    double ABx=Bx-Ax, ABy=By-Ay, ABz=Bz-Az;
    double ACx=Cx-Ax, ACy=Cy-Ay, ACz=Cz-Az;

    double nx=ABy*ACz - ABz*ACy;
    double ny=ABz*ACx - ABx*ACz;
    double nz=ABx*ACy - ABy*ACx;
    double twiceA=std::sqrt(std::max(0.0, nx*nx+ny*ny+nz*nz));
    double area=0.5*twiceA;
    if (twiceA>0){ nx/=twiceA; ny/=twiceA; nz/=twiceA; }

    T.area[e]=finite_or(area,0.0);
    T.nx[e]=finite_or(nx,0.0); T.ny[e]=finite_or(ny,0.0); T.nz[e]=finite_or(nz,1.0);
    T.cx[e]=finite_or((Ax+Bx+Cx)/3.0,0.0);
    T.cy[e]=finite_or((Ay+By+Cy)/3.0,0.0);
    T.cz[e]=finite_or((Az+Bz+Cz)/3.0,0.0);
    T.rc_mean[e]=finite_or((M.rc[ia]+M.rc[ib]+M.rc[ic])/3.0,1.0);
    T.Vsh_n_mean[e]=finite_or((M.Vsh_n[ia]+M.Vsh_n[ib]+M.Vsh_n[ic])/3.0,0.0);
  }
}

// --- Tecplot helpers ---------------------------------------------------------
static inline void dump_array_block(std::FILE* fp, const std::vector<double>& a){
  int cnt=0; for(double v:a){ std::fprintf(fp,"%.9e ",finite_or(v)); if(++cnt==8){std::fprintf(fp,"\n"); cnt=0;} } if(cnt) std::fprintf(fp,"\n");
}
static inline void dump_zeros_block(std::FILE* fp, std::size_t count){
  int cnt=0; for(std::size_t e=0;e<count;++e){ std::fprintf(fp,"0 "); if(++cnt==8){std::fprintf(fp,"\n"); cnt=0;} } if(cnt) std::fprintf(fp,"\n");
}

// Surface-only: cell metrics + nodal rc/Vsh_n
bool Model::write_shock_surface_center_metrics_tecplot(
  const ShockMesh& M, const TriMetrics& T_in, const char* path) const {

  TriMetrics T=T_in;
  const std::size_t Nv=M.x.size(), Ne=M.tri_i.size();
  if (T.area.size()!=Ne){ compute_triangle_metrics(M,T); }

  std::FILE* fp=std::fopen(path,"w"); if(!fp) return false;

  std::fprintf(fp,"TITLE=\"Shock surface (cell metrics + nodal rc)\"\n");
  std::fprintf(fp,"VARIABLES=\"X\",\"Y\",\"Z\",\"n\",\"Vx\",\"Vy\",\"Vz\",\"Bx\",\"By\",\"Bz\",\"divVsw\","
                  "\"rc\",\"Vsh_n\",\"nx\",\"ny\",\"nz\",\"area\",\"rc_mean\",\"Vsh_n_mean\",\"tnx\",\"tny\",\"tnz\",\"cx\",\"cy\",\"cz\"\n");
  std::fprintf(fp,"ZONE T=\"surface_cells\", N=%zu, E=%zu, ZONETYPE=FETRIANGLE, DATAPACKING=BLOCK,\n", Nv, Ne);
  std::fprintf(fp,"VARLOCATION=([1-3,12-13]=NODAL, [4-11,14-25]=CELLCENTERED)\n");

  dump_array_block(fp,M.x); dump_array_block(fp,M.y); dump_array_block(fp,M.z);
  dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne);
  dump_zeros_block(fp,Ne);
  dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne);
  dump_zeros_block(fp,Ne);
  dump_array_block(fp,M.rc); dump_array_block(fp,M.Vsh_n);
  dump_array_block(fp,T.nx); dump_array_block(fp,T.ny); dump_array_block(fp,T.nz);
  dump_array_block(fp,T.area); dump_array_block(fp,T.rc_mean); dump_array_block(fp,T.Vsh_n_mean);
  dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne);
  dump_array_block(fp,T.cx); dump_array_block(fp,T.cy); dump_array_block(fp,T.cz);

  for (std::size_t e=0;e<Ne;++e)
    std::fprintf(fp,"%d %d %d\n", M.tri_i[e], M.tri_j[e], M.tri_k[e]);

  std::fclose(fp);
  return true;
}

// Default apex-aligned volume box
BoxSpec Model::default_apex_box(const StepState& S,double half_AU,int N) const {
  BoxSpec B; const double h=half_AU*AU;
  B.hx=h; B.hy=h; B.hz=h;
  const double shift=0.4*h; // move box outward along e1 so shock cuts through
  B.cx=S.a_m*S.e1[0]+shift*S.e1[0];
  B.cy=S.a_m*S.e1[1]+shift*S.e1[1];
  B.cz=S.a_m*S.e1[2]+shift*S.e1[2];
  B.Ni=N; B.Nj=N; B.Nk=N; return B;
}

// Bundle writer: surface_cells + surface_nodal + volume_box + minX face
bool Model::write_tecplot_dataset_bundle(const ShockMesh& M,const TriMetrics& T_in,
                                         const StepState& S,const BoxSpec& B,
                                         const char* path) const {
  TriMetrics T=T_in;
  const std::size_t Nv=M.x.size(), Ne=M.tri_i.size();
  if (T.area.size()!=Ne){ compute_triangle_metrics(M,T); }

  std::FILE* fp=std::fopen(path,"w"); if(!fp) return false;
  auto p=[&](const char* fmt, auto... args){ std::fprintf(fp,fmt,args...); };

  p("TITLE = \"SW+CME dataset\"\n");
  p("VARIABLES = "
    "\"X\",\"Y\",\"Z\","
    "\"n\",\"Vx\",\"Vy\",\"Vz\","
    "\"Bx\",\"By\",\"Bz\",\"divVsw\","
    "\"rc\",\"Vsh_n\","
    "\"nx\",\"ny\",\"nz\",\"area\",\"rc_mean\",\"Vsh_n_mean\","
    "\"tnx\",\"tny\",\"tnz\",\"cx\",\"cy\",\"cz\"\n");

  // Zone 1: surface_cells (FETRIANGLE, BLOCK)
  p("ZONE T=\"surface_cells\", N=%zu, E=%zu, ZONETYPE=FETRIANGLE, DATAPACKING=BLOCK,\n", Nv, Ne);
  p("VARLOCATION=([1-3,12-13]=NODAL, [4-11,14-25]=CELLCENTERED)\n");

  dump_array_block(fp,M.x); dump_array_block(fp,M.y); dump_array_block(fp,M.z);
  dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne);
  dump_zeros_block(fp,Ne);
  dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne);
  dump_zeros_block(fp,Ne);
  dump_array_block(fp,M.rc); dump_array_block(fp,M.Vsh_n);
  dump_array_block(fp,T.nx); dump_array_block(fp,T.ny); dump_array_block(fp,T.nz);
  dump_array_block(fp,T.area); dump_array_block(fp,T.rc_mean); dump_array_block(fp,T.Vsh_n_mean);
  dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne); dump_zeros_block(fp,Ne);
  dump_array_block(fp,T.cx); dump_array_block(fp,T.cy); dump_array_block(fp,T.cz);

  for (std::size_t e=0;e<Ne;++e)
    p("%d %d %d\n", M.tri_i[e], M.tri_j[e], M.tri_k[e]);

  // Zone 2: surface_nodal (FEPOINT)
  p("ZONE T=\"surface_nodal\", N=%zu, E=%zu, F=FEPOINT, ET=TRIANGLE\n", Nv, Ne);
  for (std::size_t i=0;i<Nv;++i){
    p("%.9e %.9e %.9e "
      "%.9e %.9e %.9e %.9e "
      "%.9e %.9e %.9e %.9e "
      "%.9e %.9e "
      "%.9e %.9e %.9e "
      "%.9e %.9e %.9e "
      "%.9e %.9e %.9e "
      "%.9e %.9e %.9e\n",
      finite_or(M.x[i]), finite_or(M.y[i]), finite_or(M.z[i]),
      0.0,0.0,0.0,0.0,
      0.0,0.0,0.0,0.0,
      finite_or(M.rc[i],1.0), finite_or(M.Vsh_n[i],0.0),
      finite_or(M.n_hat_x[i],0.0), finite_or(M.n_hat_y[i],0.0), finite_or(M.n_hat_z[i],1.0),
      0.0,0.0,0.0,
      0.0,0.0,0.0,
      0.0,0.0,0.0
    );
  }
  for (std::size_t e=0;e<Ne;++e)
    p("%d %d %d\n", M.tri_i[e], M.tri_j[e], M.tri_k[e]);

  // Zone 3: volume_box (structured POINT)
  p("ZONE T=\"volume_box\", I=%d, J=%d, K=%d, DATAPACKING=POINT\n", B.Ni,B.Nj,B.Nk);
  for (int kk=0; kk<B.Nk; ++kk){
    const double zk = B.cz + (-B.hz + (2.0*B.hz) * (kk / double(std::max(1,B.Nk-1))));
    for (int jj=0; jj<B.Nj; ++jj){
      const double yj = B.cy + (-B.hy + (2.0*B.hy) * (jj / double(std::max(1,B.Nj-1))));
      for (int ii=0; ii<B.Ni; ++ii){
        const double xi = B.cx + (-B.hx + (2.0*B.hx) * (ii / double(std::max(1,B.Ni-1))));
        double n,Vx,Vy,Vz,Bx,By,Bz,div;
        evaluate_cartesian_with_B(S,&xi,&yj,&zk,&n,&Vx,&Vy,&Vz,&Bx,&By,&Bz,1);
        compute_divV_radial(S,&xi,&yj,&zk,&div,1,1e-3);
        p("%.9e %.9e %.9e "
          "%.9e %.9e %.9e %.9e "
          "%.9e %.9e %.9e %.9e "
          "%.9e %.9e "
          "%.9e %.9e %.9e "
          "%.9e %.9e %.9e "
          "%.9e %.9e %.9e "
          "%.9e %.9e %.9e\n",
          finite_or(xi), finite_or(yj), finite_or(zk),
          finite_or(n), finite_or(Vx), finite_or(Vy), finite_or(Vz),
          finite_or(Bx), finite_or(By), finite_or(Bz), finite_or(div,0.0),
          0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0
        );
      }
    }
  }

  // Zone 4: min-X face (structured 2-D POINT)
  const int I=std::max(2,B.Nj), J=std::max(2,B.Nk);
  const double x0=B.cx-B.hx;
  const double y0=B.cy-B.hy, y1=B.cy+B.hy;
  const double z0=B.cz-B.hz, z1=B.cz+B.hz;
  p("ZONE T=\"box_face_minX\", I=%d, J=%d, DATAPACKING=POINT\n", I, J);
  for (int j=0;j<J;++j){
    const double tz=(J==1)?0.0: double(j)/double(J-1);
    const double z=z0+(z1-z0)*tz;
    for (int i=0;i<I;++i){
      const double ty=(I==1)?0.0: double(i)/double(I-1);
      const double y=y0+(y1-y0)*ty;
      const double x=x0;
      double n,Vx,Vy,Vz,Bx,By,Bz,div;
      evaluate_cartesian_with_B(S,&x,&y,&z,&n,&Vx,&Vy,&Vz,&Bx,&By,&Bz,1);
      compute_divV_radial(S,&x,&y,&z,&div,1,1e-3);
      p("%.9e %.9e %.9e "
        "%.9e %.9e %.9e %.9e "
        "%.9e %.9e %.9e %.9e "
        "%.9e %.9e "
        "%.9e %.9e %.9e "
        "%.9e %.9e %.9e "
        "%.9e %.9e %.9e "
        "%.9e %.9e %.9e\n",
        finite_or(x), finite_or(y), finite_or(z),
        finite_or(n), finite_or(Vx), finite_or(Vy), finite_or(Vz),
        finite_or(Bx), finite_or(By), finite_or(Bz), finite_or(div,0.0),
        0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0
      );
    }
  }

  std::fclose(fp);
  return true;
}

// Standalone 2-D face writer (min-X plane)
bool Model::write_box_face_minX_tecplot_structured(const StepState& S,
                                                   const BoxSpec& B,
                                                   const char* path) const {
  const int I=std::max(2,B.Nj), J=std::max(2,B.Nk);
  const double x0=B.cx-B.hx;
  const double y0=B.cy-B.hy, y1=B.cy+B.hy;
  const double z0=B.cz-B.hz, z1=B.cz+B.hz;

  std::FILE* fp=std::fopen(path,"w"); if(!fp) return false;
  auto p=[&](const char* fmt, auto... args){ std::fprintf(fp,fmt,args...); };

  p("TITLE = \"Box face (minX)\"\n");
  p("VARIABLES = "
    "\"X\",\"Y\",\"Z\","
    "\"n\",\"Vx\",\"Vy\",\"Vz\","
    "\"Bx\",\"By\",\"Bz\",\"divVsw\","
    "\"rc\",\"Vsh_n\","
    "\"nx\",\"ny\",\"nz\",\"area\",\"rc_mean\",\"Vsh_n_mean\","
    "\"tnx\",\"tny\",\"tnz\",\"cx\",\"cy\",\"cz\"\n");
  p("ZONE T=\"box_face_minX\", I=%d, J=%d, DATAPACKING=POINT\n", I, J);

  for (int j=0;j<J;++j){
    const double tz=(J==1)?0.0: double(j)/double(J-1);
    const double z=z0+(z1-z0)*tz;
    for (int i=0;i<I;++i){
      const double ty=(I==1)?0.0: double(i)/double(I-1);
      const double y=y0+(y1-y0)*ty;
      const double x=x0;
      double n,Vx,Vy,Vz,Bx,By,Bz,div;
      evaluate_cartesian_with_B(S,&x,&y,&z,&n,&Vx,&Vy,&Vz,&Bx,&By,&Bz,1);
      compute_divV_radial(S,&x,&y,&z,&div,1,1e-3);
      p("%.9e %.9e %.9e "
        "%.9e %.9e %.9e %.9e "
        "%.9e %.9e %.9e %.9e "
        "%.9e %.9e "
        "%.9e %.9e %.9e "
        "%.9e %.9e %.9e "
        "%.9e %.9e %.9e "
        "%.9e %.9e %.9e\n",
        finite_or(x), finite_or(y), finite_or(z),
        finite_or(n), finite_or(Vx), finite_or(Vy), finite_or(Vz),
        finite_or(Bx), finite_or(By), finite_or(Bz), finite_or(div,0.0),
        0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0, 0.0,0.0,0.0
      );
    }
  }
  std::fclose(fp);
  return true;
}

} // namespace swcme3d

