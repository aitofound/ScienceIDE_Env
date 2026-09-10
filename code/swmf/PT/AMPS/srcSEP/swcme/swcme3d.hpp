#pragma once
// ============================================================================
// swcme3d.hpp
// ----------------------------------------------------------------------------
// PUBLIC API for a fast, semi-analytical Solar Wind + CME forward-shock model.
//
// What this header declares
// -------------------------
// • Public constants (AU, Rs, PI) with external linkage.
// • Parameter bundle (Params) with physically-named fields and units.
// • Time-dependent state (StepState) computed from Params at time t.
// • Simple geometric containers: ShockMesh, TriMetrics, BoxSpec.
// • Model class with:
//   - Directional shock geometry: radius and normal.
//   - Oblique MHD proxy for compression ratio rc and normal shock speed Vsh_n.
//   - Field evaluators returning density (n), bulk velocity (V), magnetic
//     field (B) and divergence ∇·V at arbitrary Cartesian points.
//   - Shock surface meshing + per-triangle metrics (area, centroid, normal,
//     rc_mean, Vsh_n_mean).
//   - Tecplot writers (surface, volume box, an optional box face).
//
// Units & conventions
// -------------------
// • Positions in SI meters (m). Velocities in m/s. Density in m^-3.
// • B field in Tesla (1 nT = 1e-9 T).
// • Angles in radians. Time in seconds.
// • The Sun is at the origin (0,0,0).
// • The CME apex direction is the unit vector Params::cme_dir (global frame).
//
// Physics model (brief)
// ---------------------
// • Ambient density n_up(r): Leblanc et al. (1998), scaled to n(1 AU).
// • Parker spiral upstream B (Parker 1958), with |B|(1 AU) set by B1AU_nT.
// • CME apex kinematics via Drag-Based Model (DBM): Vršnak et al. (2013).
// • Shock shape: sphere, ellipsoid, or cone-like SSE (with flank slowdown).
// • Local rc proxy from oblique fast Mach number (Edmiston & Kennel 1984;
//   Priest 2014), limited to ≤ 4.
// • Sheath / ejecta blends using C^1 smoothsteps and independent edge widths.
// • Tangential B amplified smoothly in the sheath (Bn continuous).
// • ∇·V by robust radial finite-difference: (1/r^2) ∂(r^2 V_r)/∂r.
//
// Output (Tecplot)
// ----------------
// VARIABLES order (used consistently across all zones):
//   1: X [m]   2: Y [m]   3: Z [m]
//   4: n [m^-3]      5..7: Vx,Vy,Vz [m/s]
//   8..10: Bx,By,Bz [T]   11: divVsw [1/s]
//   12: rc [-]        13: Vsh_n [m/s]
//   14..16: nx,ny,nz (geom normal; cell-centered in surface_cells)
//   17: area [m^2]    18: rc_mean [-]  19: Vsh_n_mean [m/s]
//   20..22: tnx,tny,tnz (reserved)     23..25: cx,cy,cz [m] (centroid)
//
//
// References (short list)
// -----------------------
// • Parker, E.N. (1958) ApJ 128:664 — Parker spiral.
// • Leblanc, Y.; Dulk, G.A.; Bougeret, J.-L. (1998) Sol. Phys. 183:165 — n(r).
// • Vršnak, B. et al. (2013) Sol. Phys. 285:295 — DBM CME kinematics.
// • Edmiston, J.P.; Kennel, C.F. (1984) J. Plasma Phys. 32:429 — oblique shocks.
// • Priest, E. (2014) Magnetohydrodynamics of the Sun, CUP.
// • Russell, C.T.; Mulligan, T. (2002) PSS 50:527 — CME sheath/ICME structure.
// • Manchester, W.B. IV et al. (2005) ApJ 622:1225 — CME-driven shocks.
//



//
// PHYSICS CONTENTS (Brief):
//  • Upstream solar wind: Parker-spiral magnetic field (Parker 1958) and
//    Leblanc et al. (1998) empirical density profile, both scaled to 1 AU.
//  • CME shock apex kinematics: Drag-Based Model (DBM; Vršnak et al. 2013).
//  • Shock geometry: Sphere / Ellipsoid / Cone-like (SSE-style) parametrizations.
//  • Region structure along field lines (radial sampling from the Sun):
//      upstream → (shock) → sheath → (leading edge) → magnetic ejecta →
//      (trailing edge) → downstream ambient,
//    using C^1 smoothstep blends with independent widths at each interface.
//  • Oblique MHD shock proxy (Edmiston & Kennel 1984; Priest 2014) to estimate
//    (i) normal shock speed Vsh_n and (ii) density compression ratio rc.
//  • Magnetic field jump: normal component continuous, tangential amplified ~ rc.
//  • Mesh generator for the shock surface + triangle metrics (area, normals,
//    centroids, rc_mean, Vsh_n_mean). All saved to Tecplot.
//  • Structured-box samplers for (n, V, B, ∇·V) with NaN/Inf sanitization.
//
// EFFICIENCY (Hot path):
//  • StepState caches Parker/Leblanc constants and geometry helpers per time.
//    No std::pow in loops; density via r^-2, r^-4, r^-6 with cached coeffs.
//  • Smoothstep edges use precomputed 1/(2w). Vectorization hint (GCC ivdep).
//  • Evaluate functions are re-entrant and allocation-free.
//
// UNITS (SI unless stated):
//  - Position [m];  AU and Rs provided
//  - Velocity [m s^-1]
//  - Density n [m^-3]
//  - Magnetic field B [Tesla]
//  - Divergence ∇·V [s^-1]
//  - Time [s]
//
// KEY REFERENCES:
//  - Parker, E.N. (1958), ApJ 128, 664  (Parker spiral)
//  - Leblanc, Y., Dulk, G.A., Bougeret, J.-L. (1998), Sol. Phys. 183, 165  (n(r))
//  - Vršnak, B. et al. (2013), Sol. Phys. 285, 295  (DBM apex kinematics)
//  - Edmiston, J.P., Kennel, C.F. (1984), JPP 32, 429  (oblique shocks)
//  - Priest, E. (2014), Magnetohydrodynamics of the Sun, CUP  (MHD shock theory)
//  - Russell, C.T., Mulligan, T. (2002), PSS 50, 527; Manchester, W. et al. (2005),
//    ApJ 622, 1225  (CME sheath & ejecta phenomenology)
//
// BUILD EXAMPLE:
//   g++ -std=c++17 -O3 -march=native demo3d_2.cpp swcme3d.cpp -o demo
// ============================================================================

#include <vector>
#include <cstddef>

namespace swcme3d {

// ----------------------------------------------------------------------------
// Physical constants (defined in swcme3d.cpp)
// ----------------------------------------------------------------------------
extern const double AU;  // Astronomical Unit [m]
extern const double Rs;  // Solar radius [m]
extern const double PI;  // π

// ----------------------------------------------------------------------------
// Shock geometry selector
//  Sphere: radius r_sh(t).
//  Ellipsoid: axes (a, b, c) aligned with (e1,e2,e3), with b/a=axis_ratio_y,
//             c/a=axis_ratio_z, where e1 is the apex direction.
//  ConeSSE: cone-/shell-like front. Inside half_width, radius scales as
//           r_sh * cos(theta)^m (flank_slowdown_m) with theta from apex axis;
//           outside half_width it is clamped (thin flanks).
// ----------------------------------------------------------------------------
enum class ShockShape { Sphere, Ellipsoid, ConeSSE };

// ----------------------------------------------------------------------------
// Parameter pack (set at construction). All are read-only thereafter.
// Values are consumed by prepare_step(t) which builds the StepState cache.
// ----------------------------------------------------------------------------
struct Params {
  // Geometry & orientation
  ShockShape shape = ShockShape::Sphere;
  double axis_ratio_y = 1.0;               // Ellipsoid b/a (e2-axis)
  double axis_ratio_z = 1.0;               // Ellipsoid c/a (e3-axis)
  double half_width_rad = 40.0*PI/180.0;   // Cone half-angle [rad]
  double flank_slowdown_m = 1.0;           // Cone exponent m ≥ 0

  double cme_dir[3] = {1,0,0};             // Global unit vector for apex direction (e1)
  double sin_theta  = 0.5;                 // sin(colatitude) for Parker pitch (≈ ecliptic: ~1)

  // DBM apex kinematics (Vršnak et al. 2013)
  double r0_Rs       = 1.05;               // initial apex radius [Rs]
  double V0_sh_kms   = 1500;               // initial shock speed [km/s]
  double V_sw_kms    = 400;                // ambient SW speed [km/s]
  double Gamma_kmInv = 1e-7;               // drag parameter Γ [km^-1]

  // Ambient SW scalings at 1 AU
  double n1AU_cm3 = 5.0;                   // upstream density at 1 AU [cm^-3]
  double B1AU_nT  = 5.0;                   // |B|(1 AU) [nT]
  double T_K      = 1.2e5;                 // proton temperature [K]
  double gamma_ad = 5.0/3.0;               // adiabatic index

  // Self-similar radial thicknesses (scale ∝ r_sh)
  double sheath_thick_AU_at1AU = 0.1;      // sheath thickness at 1 AU [AU]
  double ejecta_thick_AU_at1AU = 0.2;      // ejecta thickness at 1 AU [AU]

  // Edge smoothing (C^1 smoothstep) widths, scale ∝ r_sh
  double edge_smooth_shock_AU_at1AU = 0.01; // around the shock front
  double edge_smooth_le_AU_at1AU    = 0.02; // sheath→ejecta leading edge
  double edge_smooth_te_AU_at1AU    = 0.03; // ejecta→downstream trailing edge

  // Target speeds in regions (relative to upstream V_sw)
  double V_sheath_LE_factor = 1.1;        // sheath speed at LE = factor*V_sw
  double V_ME_factor        = 0.8;        // ejecta speed (bulk) = factor*V_sw

  // Sheath shaping
  double sheath_ramp_power  = 2.0;        // ≥1, steeper density jump near shock
  double sheath_comp_floor  = 1.2;        // min compression at shock (≥1)

  // Ejecta density factor (relative to upstream)
  double f_ME = 0.5;                      // n_ejecta = f_ME * n_up
};

// ----------------------------------------------------------------------------
// Time-dependent state for a given time t (returned by prepare_step).
// CACHES everything required by hot loops (n,V,B,divV evaluators).
// ----------------------------------------------------------------------------
struct StepState {
  // Apex-aligned orthonormal frame (e1 ≡ apex dir; e2,e3 transverse)
  double e1[3], e2[3], e3[3];

  // Apex kinematics & scale
  double r_sh_m = 0.0;    // shock apex radius [m]
  double V_sh_ms= 0.0;    // shock apex speed [m/s]
  double a_m    = 0.0;    // reference size (== r_sh_m)

  // Region extents and smoothing widths (in meters)
  double dr_sheath_m = 0.0;   // sheath thickness
  double dr_me_m     = 0.0;   // ejecta thickness
  double w_shock_m   = 0.0;   // smooth width at shock
  double w_le_m      = 0.0;   // smooth width at leading edge
  double w_te_m      = 0.0;   // smooth width at trailing edge
  double r_le_m      = 0.0;   // radius of leading edge (= r_sh - dr_sheath)
  double r_te_m      = 0.0;   // radius of trailing edge (= r_le - dr_me)

  // Target speeds
  double V_sheath_LE_ms = 0.0; // sheath speed at LE
  double V_ME_ms        = 0.0; // ejecta bulk speed
  double V_dn_ms        = 0.0; // downstream ambient (== V_sw)

  // Convenience
  double rc = 1.0;             // diagnostic: apex compression
  double inv_dr_sheath = 0.0;  // 1 / dr_sheath
  double rc_floor = 1.0;       // min compression (from Params)

  // ===== CACHES FOR EFFICIENCY (constant during this step) =====
  // Ambient wind speed
  double V_sw_ms = 0.0;

  // Leblanc density, SI coefficients with r-powers absorbed:
  // n(r) = C2 r^-2 + C4 r^-4 + C6 r^-6 [m^-3]
  double C2 = 0.0, C4 = 0.0, C6 = 0.0;

  // Precompute 1/(2w) to reduce divides in smoothstep arguments
  double inv2w_sh = 0.0, inv2w_le = 0.0, inv2w_te = 0.0;

  // Parker spiral constants for current step (fixed V_sw, latitude)
  // k_AU = Ω AU sinθ / V_sw; Br(1 AU) chosen s.t. |B|(1 AU) = B1AU_nT
  double k_AU = 0.0;
  double Br1AU_T = 0.0;   // Br at 1 AU in Tesla

  // Ellipsoid helpers: a,b,c and 1/a^2,1/b^2,1/c^2
  double a_e=0.0, b_e=0.0, c_e=0.0;
  double inv_a2=0.0, inv_b2=0.0, inv_c2=0.0;

  // Cone helpers
  double cos_half_width=1.0, flank_m=1.0;
};

// ----------------------------------------------------------------------------
// Shock surface mesh and per-triangle metrics
//  - tri_i/j/k are 1-based (Tecplot-friendly).
//  - Nodal arrays are the same length (Nv); tri arrays length is 3*Ne.
// ----------------------------------------------------------------------------
struct ShockMesh {
  // Nodal fields (size Nv)
  std::vector<double> x, y, z;                 // vertex positions [m]
  std::vector<double> n_hat_x, n_hat_y, n_hat_z; // outward unit normals at nodes
  std::vector<double> rc;                      // nodal compression proxy
  std::vector<double> Vsh_n;                   // nodal normal shock speed [m/s]

  // Connectivity (1-based triangle indices)
  std::vector<int> tri_i, tri_j, tri_k;        // size Ne
};

struct TriMetrics {
  // Per-triangle (cell-centered) metrics (size Ne)
  std::vector<double> area;       // [m^2]
  std::vector<double> nx, ny, nz; // unit normal (geometry-based)
  std::vector<double> cx, cy, cz; // centroid [m]
  std::vector<double> rc_mean;    // mean of nodal rc
  std::vector<double> Vsh_n_mean; // mean of nodal Vsh_n [m/s]
};

// ----------------------------------------------------------------------------
// Structured volume box specification for sampling fields in a region.
// The box is centered at (cx,cy,cz), with half-sizes (hx,hy,hz), and samples
// a regular grid Ni×Nj×Nk. You may build an apex-aligned default via
// Model::default_apex_box(...).
// ----------------------------------------------------------------------------
struct BoxSpec {
  double cx=0, cy=0, cz=0;   // box center [m]
  double hx=0, hy=0, hz=0;   // half-sizes [m]
  int Ni=16, Nj=16, Nk=16;   // resolution (≥2 per axis)
};

// ----------------------------------------------------------------------------
// Model class
// ----------------------------------------------------------------------------
class Model {
public:
  explicit Model(const Params&);

  // Build time-dependent state (and caches) for time t_s [s].
  StepState prepare_step(double t_s) const;

  // Shock radius along unit direction u=(ux,uy,uz) and outward normal.
  void shape_radius_normal(const StepState& S,
                           double ux,double uy,double uz,
                           double& Rdir_m,double n_hat[3]) const;

  // Local oblique-shock proxy at radius r_eval_m (≈ shock location along u):
  // outputs: rc (density jump), Vsh_n (normal speed), thetaBn (B–normal angle).
  void local_oblique_rc(const StepState& S, const double u[3], const double n_hat[3],
                        double Rdir_m, double r_eval_m,
                        double& rc_out, double& Vsh_n_out, double& thetaBn_out) const;

  // Evaluate n and V (radial direction) for arrays of Cartesian points.
  void evaluate_cartesian_fast(const StepState& S,
                               const double* x_m,const double* y_m,const double* z_m,
                               double* n_m3,double* Vx_ms,double* Vy_ms,double* Vz_ms,
                               std::size_t N) const;

  // Evaluate n, V, and B (Parker upstream + sheath Bt amplification).
  void evaluate_cartesian_with_B(const StepState& S,
                                 const double* x_m,const double* y_m,const double* z_m,
                                 double* n_m3,double* Vx_ms,double* Vy_ms,double* Vz_ms,
                                 double* Bx_T,double* By_T,double* Bz_T,
                                 std::size_t N) const;

  // Evaluate n, V, B, and div(V) using a robust radial finite-difference.
  void evaluate_cartesian_with_B_div(const StepState& S,
                                     const double* x_m,const double* y_m,const double* z_m,
                                     double* n_m3,double* Vx_ms,double* Vy_ms,double* Vz_ms,
                                     double* Bx_T,double* By_T,double* Bz_T,double* divVsw,
                                     std::size_t N, double dr_frac=1e-3) const;

  // Compute ∇·V via (1/r^2) d(r^2 V_r)/dr with r±dr sampling along the ray.
  void compute_divV_radial(const StepState& S,
                           const double* x_m,const double* y_m,const double* z_m,
                           double* divV,std::size_t N,double dr_frac=1e-3) const;

  // Quick diagnostic at a direction u (unit). Returns Rdir, normal, rc, Vsh_n.
  void diagnose_direction(const StepState& S, const double u[3],
                          double& Rdir_m,double n_hat[3],
                          double& rc_loc,double& Vsh_n) const;

  // Build a lat–lon shock surface with nodal rc and Vsh_n. (nTheta×nPhi grid)
  ShockMesh build_shock_mesh(const StepState& S, std::size_t nTheta, std::size_t nPhi) const;

  // Compute per-triangle metrics (area, normals, centroid, mean rc and Vsh_n).
  void compute_triangle_metrics(const ShockMesh& M, TriMetrics& T) const;

  // Useful default volume box (apex-aligned, shifted outward).
  BoxSpec default_apex_box(const StepState& S,double half_AU,int N) const;

  // --- Tecplot writers (VARIABLES defined exactly below) --------------------
  //
  // VARIABLES (same order in all zones written by these functions):
  //   1: X [m],  2: Y [m],  3: Z [m],
  //   4: n [m^-3],  5: Vx [m/s], 6: Vy [m/s], 7: Vz [m/s],
  //   8: Bx [T], 9: By [T], 10: Bz [T], 11: divVsw [1/s],
  //   12: rc [-], 13: Vsh_n [m/s],
  //   14: nx [-], 15: ny [-], 16: nz [-],           // cell geometric normal
  //   17: area [m^2], 18: rc_mean [-], 19: Vsh_n_mean [m/s],
  //   20: tnx [-], 21: tny [-], 22: tnz [-],        // reserved (e.g., tension dir)
  //   23: cx [m], 24: cy [m], 25: cz [m]            // cell centroid
  //
  // Notes:
  //  • In "surface_cells" (FETRIANGLE, BLOCK), [1–3,12–13] are NODAL, all others
  //    are CELLCENTERED. Nodal arrays are length Nv; cell arrays length Ne.
  //  • In "surface_nodal" (FEPOINT), every line lists all 25 values per node;
  //    rc and Vsh_n are meaningful; nx,ny,nz are filled with nodal normals.
  //  • In "volume_box" (structured POINT), n,V,B,divV are meaningful; surface-
  //    specific quantities (rc, Vsh_n, normals, area, centroids) are zeros.
  //  • All numeric outputs are NaN/Inf-sanitized before printing.
  // --------------------------------------------------------------------------
  bool write_tecplot_dataset_bundle(const ShockMesh& M,const TriMetrics& T,
                                    const StepState& S,const BoxSpec& B,
                                    const char* path) const;

  // Standalone 2-D face (min-X plane of a given BoxSpec) in Tecplot.
  bool write_box_face_minX_tecplot_structured(const StepState& S,
                                              const BoxSpec& B,
                                              const char* path) const;

  // Surface-only writer (cell metrics + nodal rc/Vsh_n) as a single zone.
  bool write_shock_surface_center_metrics_tecplot(const ShockMesh& M,
                                                  const TriMetrics& T,
                                                  const char* path) const;

private:
  Params P_;
};

} // namespace swcme3d

