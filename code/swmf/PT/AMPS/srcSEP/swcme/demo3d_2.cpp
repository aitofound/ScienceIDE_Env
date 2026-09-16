// demo.cpp — End-to-end demonstration of the solar-wind + CME shock model
// ============================================================================
// What this example shows
// -----------------------
// 1) **How CME “strength” is defined and measured** in this model:
//      • Apex shock compression ratio rc(t)
//      • Fast-mode normal Mach number M_fn(t) (inverted from rc)
//      • Shock excess speed ΔV = V_sh − V_sw
//      • Immediate post-shock B amplification factor |B2|/|B1| at the apex
//      • Field rotation at the shock Δθ = atan(rc*tanψ) − ψ  (ψ: Parker pitch)
// 2) **Time series at 1 AU** (n, V) + apex shock kinematics.
// 3) **Snapshot at t = 36 h**:
//      • Triangulated shock surface (Cone/SSE) + per-triangle metrics
//      • A structured volume box (0.2 AU per side) whose minus-Z face is z=0
//      • Point sampling of plasma + B (with sheath tangential amplification)
//      • 10 random points from the **first triangle** are printed to stdout
//      • 10 random points **per triangle** written to Tecplot (POINT)
//
// Files written
// -------------
//   strength_summary.csv                 : apex “strength” metrics vs time
//   ts_cone.csv                          : time series at 1 AU (n, V components, |V|)
//   shock_cone.csv                       : apex shock radius/speed + sheath/ejecta edges
//   cone_face_origin_tecplot.dat         : Tecplot multi-zone (surface nodal + cell metrics + volume)
//   predefined_points_tecplot.dat        : Tecplot POINT zone (predefined samples)
//   surface_random_samples_tecplot.dat   : Tecplot POINT zone (10 samples per triangle)
//
// Build & run
// -----------
//   g++ -std=c++17 -O3 -march=native demo.cpp swcme3d.cpp -o demo
//   ./demo
//
// Units & conventions
// -------------------
//   Distance: m;  Time: s;  Velocity: m/s;  Density: m^-3;  Magnetic field: Tesla.
//   Inputs ending with *_kms are km/s; n1AU is in cm^-3; B1AU in nT; angles in radians.
//   Sun center is (0,0,0). Parker spiral uses Ω_sun ≈ 2.86533e-6 rad/s.
//
// Physics used (short recap; see swcme3d.hpp/.cpp head comments for equations)
// ----------------------------------------------------------------------------
// • Apex kinematics via Drag-Based Model (DBM):
//     u(t) = (V0 − Vsw) / (1 + Γ (V0 − Vsw) t)
//     r(t) = r0 + Vsw t + (1/Γ) ln(1 + Γ (V0 − Vsw) t)
//     Vsh  = Vsw + u(t)
// • Ambient density: Leblanc et al. (1998) scaled to n(1 AU) = n1AU_cm3.
// • B field: Parker spiral normalized to |B|(1 AU) = B1AU_nT.
// • Sheath/ejecta: shock jump → compressed sheath → depleted ejecta → ambient
//   blended with C^1 smoothsteps (independent edge smoothness).
// • Oblique MHD jump proxy for compression ratio:
//     rc = ((γ+1) M_fn^2) / ((γ−1) M_fn^2 + 2)   (capped ≤ 4 for γ=5/3)
//   with M_fn = U_1n / c_f(θ_Bn).  The code computes rc locally per surface node.
//
// Magnetic field evaluator used here
// ----------------------------------
// `evaluate_cartesian_with_B(...)` returns upstream Parker B everywhere but in
// the **sheath** it amplifies the **tangential** component toward rc at the shock,
// relaxing to upstream at the sheath leading edge. This captures first-order
// rotation/strength right behind a forward shock.
//
// ============================================================================

#include "swcme3d.hpp"   // model API (Params, Model, AU, etc.)
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

using namespace swcme3d;

static inline double hours(double h){ return h * 3600.0; }

// Local copy of the solar rotation rate used by the Parker pitch formula.
// (Matches the value used inside the model implementation.)
static constexpr double OMEGA_SUN = 2.86533e-6; // rad/s

// -----------------------------------------------------------------------------
// Utility: sample a point uniformly inside a triangle using barycentric coords
// -----------------------------------------------------------------------------
struct Vec3 { double x,y,z; };

static inline Vec3 bary_sample(const Vec3& A, const Vec3& B, const Vec3& C,
                               std::mt19937_64& rng)
{
  std::uniform_real_distribution<double> U(0.0, 1.0);
  double r1 = U(rng), r2 = U(rng);
  double s  = std::sqrt(r1);
  double wA = 1.0 - s;
  double wB = s * (1.0 - r2);
  double wC = s * r2;
  return Vec3{ wA*A.x + wB*B.x + wC*C.x,
               wA*A.y + wB*B.y + wC*C.y,
               wA*A.z + wB*B.z + wC*C.z };
}

// -----------------------------------------------------------------------------
// Utility: write a Tecplot POINT zone with plasma + B + shock diagnostics
// -----------------------------------------------------------------------------
static bool write_points_tecplot(const char* path,
                                 const std::vector<double>& X,
                                 const std::vector<double>& Y,
                                 const std::vector<double>& Z,
                                 const std::vector<double>& n,
                                 const std::vector<double>& Vx,
                                 const std::vector<double>& Vy,
                                 const std::vector<double>& Vz,
                                 const std::vector<double>& Bx,
                                 const std::vector<double>& By,
                                 const std::vector<double>& Bz,
                                 const std::vector<double>& rc,
                                 const std::vector<double>& Vsh_n)
{
  const std::size_t N = X.size();
  if (Y.size()!=N || Z.size()!=N || n.size()!=N || Vx.size()!=N || Vy.size()!=N || Vz.size()!=N ||
      Bx.size()!=N || By.size()!=N || Bz.size()!=N || rc.size()!=N || Vsh_n.size()!=N) return false;

  std::FILE* fp = std::fopen(path, "w"); if(!fp) return false;
  std::fprintf(fp, "TITLE = \"Point cloud (plasma + B + shock diagnostics)\"\n");
  std::fprintf(fp, "VARIABLES = \"X\",\"Y\",\"Z\",\"n\",\"Vx\",\"Vy\",\"Vz\",\"Bx\",\"By\",\"Bz\",\"rc\",\"Vsh_n\"\n");
  std::fprintf(fp, "ZONE T=\"points\", N=%zu, F=POINT\n", N);
  for (std::size_t i=0;i<N;++i){
    std::fprintf(fp, "%.9e %.9e %.9e %.9e %.9e %.9e %.9e %.9e %.9e %.9e %.6e %.6e\n",
      X[i],Y[i],Z[i], n[i],Vx[i],Vy[i],Vz[i], Bx[i],By[i],Bz[i], rc[i],Vsh_n[i]);
  }
  std::fclose(fp);
  return true;
}

// -----------------------------------------------------------------------------
// Helper: write an apex “strength” summary over time
// Columns: t_s, rc_apex, Mfn_apex, Vexcess_ms, theta1_rad, B2overB1, dTheta_rad
// -----------------------------------------------------------------------------
static void write_strength_summary_csv(const Params& P, Model& model,
                                       double t0, double t1, double dt,
                                       const char* path)
{
  std::ofstream out(path);
  out << "t_s,rc_apex,Mfn_apex,Vexcess_ms,theta1_rad,B2overB1,dTheta_rad\n";

  const double g = P.gamma_ad;
  const double Vsw = P.V_sw_kms * 1e3;

  for (double t=t0; t<=t1+1e-9; t+=dt){
    StepState S = model.prepare_step(t);

    // Apex direction & local shock diagnostics
    double u_apex[3] = { S.e1[0], S.e1[1], S.e1[2] };
    double Rdir, n_hat[3], rc_apex, Vsh_n_apex;
    model.diagnose_direction(S, u_apex, Rdir, n_hat, rc_apex, Vsh_n_apex);

    // Invert rc → M_fn (fast-mode normal Mach) using RH relation
    double M2 = (rc_apex>1.0)
      ? 2.0*(rc_apex-1.0) / ((g+1.0) - (g-1.0)*rc_apex)
      : 0.0;
    double Mfn_apex = (M2>0.0)? std::sqrt(M2) : 0.0;

    // Shock excess speed at apex (total, not purely normal)
    const double Vexcess = S.V_sh_ms - Vsw;

    // Parker pitch ψ at apex: tanψ = Ω r sinθ / Vsw
    const double psi = std::atan( OMEGA_SUN * S.r_sh_m * P.sin_theta / Vsw );

    // Immediate post-shock B rotation and amplification at the apex
    const double B2_over_B1 = std::sqrt( std::cos(psi)*std::cos(psi)
                                      + rc_apex*rc_apex*std::sin(psi)*std::sin(psi) );
    const double dTheta = std::atan( rc_apex * std::tan(psi) ) - psi;

    out << std::setprecision(10)
        << t << ',' << rc_apex << ',' << Mfn_apex << ','
        << Vexcess << ',' << psi << ',' << B2_over_B1 << ',' << dTheta << '\n';
  }
}

// ============================================================================
// MAIN
// ============================================================================
int main(){
  try{
    // =========================================================================
    // 1) Define CME/shock parameters (Cone/SSE) — with explanatory comments
    // =========================================================================
    Params P;

    // --- Shock front geometry ------------------------------------------------
    P.shape = ShockShape::ConeSSE;           // choices: Sphere, Ellipsoid, ConeSSE
    P.cme_dir[0] = 0.0;                      // apex direction (unit vector)
    P.cme_dir[1] = 0.0;                      // here: +Z (0,0,1)
    P.cme_dir[2] = 1.0;

    // Cone half-angle (radians). Only directions with θ ≤ half_width exist on the cap.
    P.half_width_rad   = 40.0 * (3.14159265358979323846/180.0); // 40°
    // Flanks expand more slowly than the apex: R(θ) = R_apex * cos(θ)^m
    P.flank_slowdown_m = 2.0;

    // --- Apex kinematics (DBM) ----------------------------------------------
    // r0_Rs    : initial shock apex distance in solar radii (you asked for 1.05 R_sun)
    // V0_sh_kms: initial shock apex speed (km/s)
    // V_sw_kms : ambient solar-wind speed (km/s)
    // Gamma_kmInv: drag parameter Γ (1/km). Larger Γ → stronger deceleration/acceleration to V_sw.
    P.r0_Rs       = 1.05;
    P.V0_sh_kms   = 1500.0;
    P.V_sw_kms    = 400.0;
    P.Gamma_kmInv = 0.2e-7;

    // --- Ambient plasma & thermodynamics ------------------------------------
    // n1AU_cm3 rescales Leblanc so that n(1 AU) matches this value (cm^-3).
    P.n1AU_cm3 = 5.0;
    // Adiabatic sound speed uses T_K and gamma_ad.
    P.T_K      = 1.5e5;
    P.gamma_ad = 5.0/3.0;

    // --- Magnetic field (Parker) --------------------------------------------
    // B1AU_nT fixes |B|(1 AU). sin_theta ≈ sin(colatitude) controls winding; 1.0 near equator.
    P.B1AU_nT   = 5.0;
    P.sin_theta = 1.0;

    // --- Sheath / magnetic ejecta parameterization --------------------------
    // Thickness values are defined at 1 AU and scale self-similarly with the current apex radius.
    P.sheath_thick_AU_at1AU = 0.10;   // thicker sheath → longer relaxation
    P.ejecta_thick_AU_at1AU = 0.20;   // ME (flux-rope) radial thickness

    // Edge smoothing half-widths (AU at 1 AU). Keep the shock edge relatively sharp.
    P.edge_smooth_shock_AU_at1AU = 0.01;
    P.edge_smooth_le_AU_at1AU    = 0.03;
    P.edge_smooth_te_AU_at1AU    = 0.03;

    // Sheath compression floor at its inner edge and ramp sharpness across the sheath.
    P.sheath_comp_floor = 1.5;  // keeps sheath > ambient near its LE
    P.sheath_ramp_power = 1.5;  // >1 → faster drop from rc at the shock

    // Target speeds inside sheath/ejecta relative to ambient.
    P.V_sheath_LE_factor = 0.9; // sheath relaxes toward ambient
    P.V_ME_factor        = 0.8; // ejecta typically slower

    // Ejecta density factor (depleted compared to upstream).
    P.f_ME = 0.6;

    // Build the model with these parameters
    Model model(P);

    // =========================================================================
    // 2) “Strength” summary over time (apex metrics): 0–72 h, Δt=1 min
    // =========================================================================
    {
      const double t0=0.0, t1=hours(72.0), dt=60.0;
      write_strength_summary_csv(P, model, t0, t1, dt, "strength_summary.csv");

      // Also print a quick apex strength readout at t=36 h (for convenience)
      StepState S = model.prepare_step(hours(36.0));
      double u_apex[3] = { S.e1[0], S.e1[1], S.e1[2] };
      double Rdir, n_hat[3], rc_apex, Vsh_n_apex;
      model.diagnose_direction(S, u_apex, Rdir, n_hat, rc_apex, Vsh_n_apex);

      const double g = P.gamma_ad;
      double M2 = (rc_apex>1.0)
        ? 2.0*(rc_apex-1.0) / ((g+1.0) - (g-1.0)*rc_apex)
        : 0.0;
      double Mfn_apex = (M2>0.0) ? std::sqrt(M2) : 0.0;
      const double Vsw = P.V_sw_kms*1e3;
      const double psi = std::atan( OMEGA_SUN * S.r_sh_m * P.sin_theta / Vsw );
      const double B2_over_B1 = std::sqrt( std::cos(psi)*std::cos(psi)
                                        + rc_apex*rc_apex*std::sin(psi)*std::sin(psi) );
      const double dTheta = std::atan( rc_apex * std::tan(psi) ) - psi;

      std::cout << "[Strength @ apex, t=36h] "
                << "rc=" << rc_apex
                << ", M_fn=" << Mfn_apex
                << ", Vexcess=" << (S.V_sh_ms - Vsw) << " m/s"
                << ", |B2|/|B1|=" << B2_over_B1
                << ", Δθ=" << dTheta << " rad\n";
    }

    // =========================================================================
    // 3) Time series at 1 AU (+Z). Observer at (0,0,1 AU)
    // =========================================================================
    {
      const double t0=0.0, t1=hours(72.0), dt=60.0;
      const std::size_t Nt = static_cast<std::size_t>((t1-t0)/dt)+1;

      double x_obs[1]={0.0}, y_obs[1]={0.0}, z_obs[1]={AU};

      std::ofstream ts("ts_cone.csv");    ts << "t_s,n_m3,Vx_ms,Vy_ms,Vz_ms,V_mag_ms\n";
      std::ofstream sh("shock_cone.csv"); sh << "t_s,R_sh_AU,V_sh_km_s,rc,R_LE_AU,R_TE_AU\n";

      for (std::size_t k=0;k<Nt;++k){
        const double t=t0+k*dt;
        StepState S = model.prepare_step(t);

        double n,Vx,Vy,Vz;
        model.evaluate_cartesian_fast(S, x_obs,y_obs,z_obs, &n,&Vx,&Vy,&Vz, 1);
        const double Vmag = std::sqrt(Vx*Vx+Vy*Vy+Vz*Vz);

        ts << std::setprecision(10) << t << ','
           << n << ',' << Vx << ',' << Vy << ',' << Vz << ',' << Vmag << '\n';

        sh << std::setprecision(10) << t << ','
           << (S.r_sh_m/AU) << ',' << (S.V_sh_ms/1e3) << ',' << S.rc << ','
           << (S.r_le_m/AU) << ',' << (S.r_te_m/AU) << '\n';
      }
    }

    // =========================================================================
    // 4) Snapshot at t = 36 h: surface triangulation + volume box + bundle
    // =========================================================================
    const double t_mesh = hours(36.0);
    StepState Smesh = model.prepare_step(t_mesh);

    // Build the surface (lat-lon triangulation on the cone cap) and per-triangle metrics
    ShockMesh  surf = model.build_shock_mesh(Smesh, /*nTheta=*/120, /*nPhi=*/240);
    TriMetrics tri;  model.compute_triangle_metrics(surf, tri);

    // Volume box: FULL SIZE = 0.2 AU per side (half = 0.1 AU).
    // Place its **minus-Z face on z=0**, so that face contains the origin (0,0,0).
    BoxSpec B;
    B.hx = 0.1 * AU; B.hy = 0.1 * AU; B.hz = 0.1 * AU; // half-sizes
    B.cx = 0.0;      B.cy = 0.0;      B.cz = B.hz;     // cz − hz = 0 ⇒ minus-Z face at z=0
    B.Ni = 80;       B.Nj = 80;       B.Nk = 80;       // grid resolution (I,J,K)

    // Multi-zone Tecplot dataset: surface nodal, surface cell-centered, and volume
    if (!model.write_tecplot_dataset_bundle(surf, tri, Smesh, B, "cone_face_origin_tecplot.dat"))
      std::cerr << "Failed to write cone_face_origin_tecplot.dat\n";

    // =========================================================================
    // 5) Evaluate plasma + B at a set of predefined points (for quick sanity)
    // =========================================================================
    {
      std::vector<Vec3> Pts = {
        {0.0,        0.0,        0.80*AU},   // inside 1 AU, sunward of shock
        {0.0,        0.0,        0.95*AU},
        {0.0,        0.0,        1.05*AU},   // just beyond 1 AU
        {0.02*AU,    0.00,       1.00*AU},   // slight off-axis
        {-0.02*AU,   0.00,       1.00*AU},
        {0.00,       0.02*AU,    1.00*AU}
      };

      const std::size_t Np = Pts.size();
      std::vector<double> X(Np),Y(Np),Z(Np), n(Np),Vx(Np),Vy(Np),Vz(Np), Bx(Np),By(Np),Bz(Np), rc(Np),Vsh_n(Np);

      for (std::size_t i=0;i<Np;++i){
        X[i]=Pts[i].x; Y[i]=Pts[i].y; Z[i]=Pts[i].z;

        // Plasma + B (Tesla) with sheath tangential amplification
        model.evaluate_cartesian_with_B(Smesh, &X[i],&Y[i],&Z[i],
                                        &n[i],&Vx[i],&Vy[i],&Vz[i],
                                        &Bx[i],&By[i],&Bz[i], 1);

        // Local shock diagnostics (along the LOS direction of this point)
        const double r = std::sqrt(X[i]*X[i]+Y[i]*Y[i]+Z[i]*Z[i]);
        const double invr = (r>0)? 1.0/r : 0.0;
        double udir[3]={X[i]*invr, Y[i]*invr, Z[i]*invr};
        double Rdir, n_hat[3], rc_loc, Vsh_n_loc;
        model.diagnose_direction(Smesh, udir, Rdir, n_hat, rc_loc, Vsh_n_loc);
        rc[i]=rc_loc; Vsh_n[i]=Vsh_n_loc;
      }

      if (!write_points_tecplot("predefined_points_tecplot.dat", X,Y,Z, n,Vx,Vy,Vz, Bx,By,Bz, rc,Vsh_n))
        std::cerr << "Failed to write predefined_points_tecplot.dat\n";
    }

    // =========================================================================
    // 6) Surface sampling: (a) print 10 random points in the **first triangle**
    // =========================================================================
    if (!surf.tri_i.empty()){
      std::mt19937_64 rng(42);
      int ia = surf.tri_i[0]-1, ib = surf.tri_j[0]-1, ic = surf.tri_k[0]-1;
      Vec3 A{surf.x[ia], surf.y[ia], surf.z[ia]};
      Vec3 Bv{surf.x[ib], surf.y[ib], surf.z[ib]};
      Vec3 Cv{surf.x[ic], surf.y[ic], surf.z[ic]};

      std::cout << "Ten random points in first triangle (A="<<ia<<",B="<<ib<<",C="<<ic<<"):\n";
      std::cout << "idx, x, y, z [m], n[m^-3], Vx, Vy, Vz[m/s], Bx, By, Bz[T], rc, Vsh_n[m/s]\n";

      for (int m=0;m<10;++m){
        Vec3 Pp = bary_sample(A,Bv,Cv, rng);

        double n_,Vx_,Vy_,Vz_,Bx_,By_,Bz_;
        model.evaluate_cartesian_with_B(Smesh, &Pp.x,&Pp.y,&Pp.z,
                                        &n_,&Vx_,&Vy_,&Vz_,
                                        &Bx_,&By_,&Bz_, 1);

        const double rP = std::sqrt(Pp.x*Pp.x+Pp.y*Pp.y+Pp.z*Pp.z);
        const double invrP = (rP>0)?1.0/rP:0.0;
        double udir[3]={Pp.x*invrP, Pp.y*invrP, Pp.z*invrP};
        double Rdir, n_hat[3], rc_loc, Vsh_n_loc;
        model.diagnose_direction(Smesh, udir, Rdir, n_hat, rc_loc, Vsh_n_loc);

        std::cout << m << ", "
                  << std::setprecision(10) << Pp.x << ", " << Pp.y << ", " << Pp.z << ", "
                  << n_ << ", " << Vx_ << ", " << Vy_ << ", " << Vz_ << ", "
                  << Bx_ << ", " << By_ << ", " << Bz_ << ", "
                  << rc_loc << ", " << Vsh_n_loc << "\n";
      }
    }

    // =========================================================================
    // 7) Surface sampling: (b) 10 random points **per triangle** → Tecplot POINT zone
    // =========================================================================
    {
      std::mt19937_64 rng(2025);
      const std::size_t Ne = surf.tri_i.size();
      const int SAMPLES_PER_TRI = 10;
      const std::size_t Ns = Ne * SAMPLES_PER_TRI;

      std::vector<double> SX, SY, SZ, Sn, SVx, SVy, SVz, SBx, SBy, SBz, Src, SVshn;
      SX.reserve(Ns); SY.reserve(Ns); SZ.reserve(Ns);
      Sn.reserve(Ns); SVx.reserve(Ns); SVy.reserve(Ns); SVz.reserve(Ns);
      SBx.reserve(Ns); SBy.reserve(Ns); SBz.reserve(Ns);
      Src.reserve(Ns); SVshn.reserve(Ns);

      for (std::size_t e=0;e<Ne;++e){
        int i = surf.tri_i[e]-1, j = surf.tri_j[e]-1, k = surf.tri_k[e]-1;
        Vec3 A{surf.x[i], surf.y[i], surf.z[i]};
        Vec3 Bv{surf.x[j], surf.y[j], surf.z[j]};
        Vec3 Cv{surf.x[k], surf.y[k], surf.z[k]};

        for (int m=0;m<SAMPLES_PER_TRI;++m){
          Vec3 Pp = bary_sample(A,Bv,Cv, rng);

          double n_,Vx_,Vy_,Vz_,Bx_,By_,Bz_;
          model.evaluate_cartesian_with_B(Smesh, &Pp.x,&Pp.y,&Pp.z,
                                          &n_,&Vx_,&Vy_,&Vz_,
                                          &Bx_,&By_,&Bz_, 1);

          const double rP = std::sqrt(Pp.x*Pp.x+Pp.y*Pp.y+Pp.z*Pp.z);
          const double invrP = (rP>0)?1.0/rP:0.0;
          double udir[3]={Pp.x*invrP, Pp.y*invrP, Pp.z*invrP};
          double Rdir, n_hat[3], rc_loc, Vsh_n_loc;
          model.diagnose_direction(Smesh, udir, Rdir, n_hat, rc_loc, Vsh_n_loc);

          SX.push_back(Pp.x);  SY.push_back(Pp.y);  SZ.push_back(Pp.z);
          Sn.push_back(n_);    SVx.push_back(Vx_);  SVy.push_back(Vy_);  SVz.push_back(Vz_);
          SBx.push_back(Bx_);  SBy.push_back(By_);  SBz.push_back(Bz_);
          Src.push_back(rc_loc); SVshn.push_back(Vsh_n_loc);
        }
      }

      if (!write_points_tecplot("surface_random_samples_tecplot.dat",
                                SX,SY,SZ, Sn,SVx,SVy,SVz, SBx,SBy,SBz, Src,SVshn))
        std::cerr << "Failed to write surface_random_samples_tecplot.dat\n";
    }

    std::cout << "Done. Wrote strength_summary.csv, CSV time series, and Tecplot datasets.\n";
    return 0;

  } catch (const std::exception& e){
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

