// demo1d.cpp — illustrates the 1-D SW+CME model usage (sanitized API)
//
// What this does:
//  1) Configures “typical CME” parameters (fast launch).
//  2) Builds a StepState at t_snap.
//  3) Evaluates n, V, Br, Bphi, |B|, divV on a radial grid (0.2–1.5 AU).
//  4) Writes a NaN-safe Tecplot POINT file "profile_1d.dat".
//  5) (Optional) Logs a shock timeseries CSV if EnableIO() is used.

#include <vector>
#include <cstdio>
#include <cmath>
#include "swcme1d.hpp"

int main(){
  using namespace swcme1d;

  // ---- 1) Typical CME setup ----
  Params P;
  P.r0_Rs       = 1.05;     // start just above the surface
  P.V0_sh_kms   = 1800.0;   // fast launch
  P.V_sw_kms    = 400.0;    // ambient wind
  P.Gamma_kmInv = 8e-8;     // DBM drag
  P.n1AU_cm3    = 6.0;      // upstream density at 1 AU
  P.B1AU_nT     = 5.0;      // Parker magnitude at 1 AU
  P.T_K         = 1.2e5;    // proton temperature
  P.sin_theta   = 1.0;      // equatorial pitch (ecliptic)

  // Regions: sharper at shock, softer toward ejecta tail
  P.edge_smooth_shock_AU_at1AU = 0.01;
  P.edge_smooth_le_AU_at1AU    = 0.02;
  P.edge_smooth_te_AU_at1AU    = 0.03;
  P.sheath_comp_floor          = 1.2;
  P.sheath_ramp_power          = 2.0;
  P.V_sheath_LE_factor         = 1.10;
  P.f_ME                       = 0.5;
  P.V_ME_factor                = 0.80;

  Model model(P);

  // Optional CSV logging:
  // EnableIO();
  // swcme1d::Model::write_step_csv_header();

  // ---- 2) Time snapshot ----
  const double t_snap = 36.0 * 3600.0; // 36 hours after launch
  StepState S = model.prepare_step(t_snap);

  std::printf("Snapshot t=%.1f hr: R_sh=%.3f AU, V_sh=%.1f km/s, rc=%.2f\n",
              t_snap/3600.0, S.r_sh_m/AU, S.V_sh_ms/1e3, S.rc);

  // If CSV logging is enabled:
  // swcme1d::Model::write_step_csv_line(S);

  // ---- 3) Radial grid (0.2–1.5 AU) ----
  const std::size_t N = 1200;
  std::vector<double> r(N), n(N), V(N), Br(N), Bphi(N), Bmag(N), divV(N);

  const double rmin = 0.2*AU, rmax = 1.5*AU;
  for (std::size_t i=0;i<N;++i){
    const double s = (i + 0.5) / double(N);
    r[i] = rmin + s*(rmax - rmin);
  }

  // Evaluate all fields (API-sanitized)
  model.evaluate_radii_with_B_div(S, r.data(), n.data(), V.data(),
                                  Br.data(), Bphi.data(), Bmag.data(), divV.data(), N);

  // ---- 4) Tecplot output ----
  if (!model.write_tecplot_radial_profile(S, r.data(), n.data(), V.data(),
                                          Br.data(), Bphi.data(), Bmag.data(), divV.data(),
                                          N, "profile_1d.dat"))
  {
    std::fprintf(stderr,"ERROR: failed to write profile_1d.dat\n");
    return 1;
  }
  std::printf("Wrote Tecplot radial profile: profile_1d.dat (N=%zu)\n", N);

  return 0;
}

