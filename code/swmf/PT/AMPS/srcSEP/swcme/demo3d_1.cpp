// demo.cpp — usage of swcme3d (Cone/SSE case) with a box whose face contains (0,0,0)
//
// Build:
//   g++ -std=c++17 -O3 -march=native demo.cpp swcme3d.cpp -o demo
//
// Run:
//   ./demo
//
// Outputs:
//   • Time series at 1 AU (CSV):        ts_cone.csv
//   • Shock kinematics (CSV):           shock_cone.csv
//   • Tecplot dataset (3 zones):        cone_face_origin_tecplot.dat
//
// Notes:
//   - Densities are in m^-3; velocities in m/s.
//   - The Tecplot file has:
//       Zone 1: shock surface (nodal, FEPOINT)
//       Zone 2: shock surface (cell-centered metrics, BLOCK)
//       Zone 3: structured POINT volume box
//   - In this example, the volume box has **size 0.2 AU per side**
//     (half-size = 0.1 AU) and its **minus-Z face lies on z=0**, so that face
//     contains the point (0,0,0). The box is axis-aligned (no rotation).

#include "swcme3d.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <string>

using namespace swcme3d;

static inline double hours(double h){ return h * 3600.0; }

int main(){
  try {
    // -----------------------------
    // Configure a Cone/SSE shock
    // -----------------------------
    Params P;
    P.shape = ShockShape::ConeSSE;

    // Apex direction along +Z (so the cone points "up" the Z axis)
    P.cme_dir[0] = 0.0;
    P.cme_dir[1] = 0.0;
    P.cme_dir[2] = 1.0;

    // Cone opening: half-width ≈ 40 degrees; simple flank slowdown exponent
    P.half_width_rad   = 40.0 * (3.14159265358979323846/180.0);
    P.flank_slowdown_m = 2.0;

    // (Other Params defaults in swcme3d.hpp control DBM, smoothing, etc.)

    // Build model
    Model model(P);

    // -------------------------------------------------------
    // Time series at 1 AU on +Z axis (observer at x=y=0, z=1 AU)
    // -------------------------------------------------------
    const double t0 = 0.0, t1 = hours(72.0), dt = 60.0;
    const std::size_t Nt = static_cast<std::size_t>((t1 - t0)/dt) + 1;

    double x_obs[1] = { 0.0 };
    double y_obs[1] = { 0.0 };
    double z_obs[1] = { AU   };

    std::ofstream ts("ts_cone.csv");
    ts << "t_s,n_m3,Vx_ms,Vy_ms,Vz_ms,V_mag_ms\n";

    std::ofstream sh("shock_cone.csv");
    sh << "t_s,R_sh_AU,V_sh_km_s,rc,R_LE_AU,R_TE_AU\n";

    for (std::size_t k=0; k<Nt; ++k){
      const double t = t0 + k*dt;
      StepState S = model.prepare_step(t);

      double n,Vx,Vy,Vz;
      model.evaluate_cartesian_fast(S, x_obs, y_obs, z_obs, &n,&Vx,&Vy,&Vz, 1);
      const double Vmag = std::sqrt(Vx*Vx + Vy*Vy + Vz*Vz);

      ts << std::setprecision(10) << t << ','
         << std::setprecision(10) << n << ','
         << std::setprecision(10) << Vx << ','
         << std::setprecision(10) << Vy << ','
         << std::setprecision(10) << Vz << ','
         << std::setprecision(10) << Vmag << '\n';

      sh << std::setprecision(10) << t << ','
         << std::setprecision(10) << (S.r_sh_m/AU) << ','
         << std::setprecision(10) << (S.V_sh_ms/1e3) << ','
         << std::setprecision(10) << S.rc << ','
         << std::setprecision(10) << (S.r_le_m/AU) << ','
         << std::setprecision(10) << (S.r_te_m/AU) << '\n';
    }

    // -------------------------------------------------------
    // Visualization snapshot at 6 h:
    // - build shock surface
    // - compute cell metrics
    // - define volume BoxSpec with size 0.2 AU and a face through (0,0,0)
    // -------------------------------------------------------
    const double t_mesh = hours(6.0);
    StepState Smesh = model.prepare_step(t_mesh);

    // Dense-ish triangulation (keep multiples of 2 and ≥3 respectively)
    ShockMesh surf = model.build_shock_mesh(Smesh, /*nTheta=*/120, /*nPhi=*/240);

    TriMetrics tri;
    model.compute_triangle_metrics(surf, tri);

    // --- BOX: size 0.2 AU => half-size = 0.1 AU on each axis ---
    // We want **one face to contain the origin**. Choose the minus-Z face.
    // Condition for minus-Z face to be at z=0 is: cz - hz = 0  ⇒  cz = hz.
    // Also ensure origin lies within the face rectangle by setting cx=cy=0
    // with |cx| ≤ hx and |cy| ≤ hy automatically satisfied.
    BoxSpec B;
    const double half_AU = 0.1; // half-size (AU) => full size = 0.2 AU
    B.hx = half_AU * AU;
    B.hy = half_AU * AU;
    B.hz = half_AU * AU;

    // Place the center so that the minus-Z face (z = cz - hz) is exactly 0
    B.cx = 0.0;
    B.cy = 0.0;
    B.cz = B.hz; // => z_minus_face = 0; face contains (0,0,0)

    // Resolution of the structured grid (I,J,K). Adjust to taste.
    B.Ni = 80;
    B.Nj = 80;
    B.Nk = 80;

    // Write a combined Tecplot dataset (surface + cell metrics + volume box)
    if (!model.write_tecplot_dataset_bundle(surf, tri, Smesh, B, "cone_face_origin_tecplot.dat")) {
      std::cerr << "Failed to write cone_face_origin_tecplot.dat\n";
    }

    std::cout << "Done. Wrote ts_cone.csv, shock_cone.csv, cone_face_origin_tecplot.dat\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

