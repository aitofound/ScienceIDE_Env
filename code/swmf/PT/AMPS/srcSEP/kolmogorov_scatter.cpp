/*
======================================================================================================================
   SEP :: AlfvenTurbulence_Kolmogorov
   Complete, self-contained C++17 implementation for proton scattering/acceleration by slab Alfvén turbulence
   with a Kolmogorov spectrum (W^s(k) ∝ k^(-5/3)), including:
     • SDE-based “small-angle” wave-frame scattering (elastic in wave frame, energy change from boost)
     • Uniform-μ′ wave-frame “hard” scattering (draw μ′ uniformly over the resonant band)
     • Physically correct branch selection for all v, μ using exact resonance gating
     • Built-in RNG hooks: rnd() ∈ [0,1) and Vector3D::Distribution::Normal() ~ N(0,1)

   USAGE SUMMARY
   -------------
   1) Call ScatterStepProton(...) for SDE small-angle scattering per time step (preferred for QLT).
   2) Call ScatterStepProton_UniformMuPrime(...) for a large-angle “hard” scattering (coarse-grained).
   3) Both methods:
        - Input plasma-frame state (v∥, v⊥), |B|, V_A, branch-integrated wave powers W⁺, W⁻ (units of B²),
          k-band [kmin, kmax], and (for SDE) a plasma-frame dt.
        - Internally select the wave branch s ∈ {+1,−1} using branch-resolved D_{μμ} with resonance gates.
        - Boost to the chosen wave frame, do the kick (elastic in wave frame), and boost back (energy change).
        - Return updated plasma-frame kinematics and diagnostics.
   4) Units: SI throughout (B in Tesla, speeds m/s, k in 1/m, time s). If your W⁺/W⁻ are energy densities
      (J/m³), convert once via δB² = 2 μ0 W_energy (SI) and pass δB².

   THEORY CRIB
   -----------
   • Cyclotron resonance (protons, n=+1): ω − k v μ = Ω/γ with Ω = q B / (γ m c); Alfvén branch s gives ω = s k V_A.
     ⇒ k_res^s = Ω / [γ (s V_A − v μ)] with requirement (s V_A − v μ) > 0 and k_res in [kmin,kmax].
   • Plasma-frame QLT pitch-angle diffusion (per branch):
       D_{μμ}^s = (π/2) (Ω²/B²) (1 − μ²) W^s(k_res^s) / |v μ − s V_A|
   • Kolmogorov per branch on [kmin,kmax]:
       W^s(k) = C_s k^(−5/3),  with  C_s = (2/3) W_s_total / (kmin^(−2/3) − kmax^(−2/3)),
     where W_s_total = ∫ W^s(k) dk (units of B²).
   • Wave frame (prime variables): k′_res = Ω /(γ′ v′ |μ′|). For Kolmogorov,
       D′_{μμ}(μ′) = K′_s (1 − μ′²) |μ′|^(2/3),   with
       K′_s = (π/2) (C_s/B²) Ω^(1/3) (γ′ v′)^(5/3) / v′.
     Analytic Itô drift: ∂μ′D′ = K′_s [ (2/3)(1 − μ′²)|μ′|^(−1/3) sgn(μ′) − 2 μ′ |μ′|^(2/3) ].

   NUMERICAL SAFETY
   ----------------
   • SDE small-angle limiter (wave frame): Δt′ ≤ f_sa (1 − μ′²)/D′(μ′), with f_sa ∈ [0.01, 0.1].
   • μ′→0 regularization: replace |μ′| with max(|μ′|, μ_min) in resonance and D′; default μ_min = 1e−2.
   • QLT/gyro-averaging: Prefer dt_plasma ≫ 2π/Ω; not enforced here, but recommended.

   EXAMPLES (see bottom of file for runnable examples and compile command)
   ----------------------------------------------------------------------
   • Single SDE step:
       auto r = ScatterStepProton(vpar, vperp, B, VA, Wplus_B2, Wminus_B2, kmin, kmax, dt);
   • Uniform μ′ step:
       auto r = ScatterStepProton_UniformMuPrime(vpar, vperp, B, VA, Wplus_B2, Wminus_B2, kmin, kmax, dt);

   COMPILE
   -------
     g++ -std=c++17 -O2 -march=native -DNDEBUG sep_alfven_kolmogorov_demo.cpp -o demo

======================================================================================================================
*/

#include <cmath>
#include <algorithm>
#include <limits>
#include <random>
#include <iostream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "sep.h"

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {

/* =============================== RNG HOOKS (self-contained defaults) ================================================
   rnd()              : Uniform random in [0,1)
   Normal()           : Standard normal N(0,1)
   Replace these with your own implementations if you have a centralized RNG.
   We keep them here so this file compiles and runs out-of-the-box.
=============================================================================================================== */
/* inline double rnd() {
    // Thread-local Mersenne Twister; seeded once from std::random_device.
    static thread_local std::mt19937_64 eng{[]{
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd()};
        return std::mt19937_64(seq);
    }()};
    static thread_local std::uniform_real_distribution<double> U01(0.0, 1.0);
    return U01(eng); // ∈ [0,1)
}

namespace Vector3D {
namespace Distribution {
inline double Normal() {
    static thread_local std::mt19937_64 eng{[]{
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd()};
        return std::mt19937_64(seq);
    }()};
    static thread_local std::normal_distribution<double> N01(0.0, 1.0);
    return N01(eng);
}
} // namespace Distribution
} // namespace Vector3D
*/

/* =============================== Result pack for one scattering step ============================================== */
/*struct ScatterResult {
    // Updated PLASMA-frame kinematics
    double vpar_new   = 0.0;  // m/s
    double vperp_new  = 0.0;  // m/s
    double mu_new     = 0.0;  // —
    double gamma_new  = 1.0;  // —

    // Convenience
    double delta_mu   = 0.0;  // mu_new − mu_old

    // Diagnostics
    int    branch     = 0;    // +1 forward, −1 backward, 0 = no-scatter (no resonant power)
    bool   scattered  = false;// true if a wave-frame kick actually occurred
    double dt_used    = 0.0;  // wave-frame dt used for the kick (SDE only)
};
*/


/* =============================== Helpers ========================================================================= */
inline int    sgn(double x) { return (x > 0.0) - (x < 0.0); }



template<typename T>
inline T      clamp(const T& x, const T& lo, const T& hi) { return std::max(lo, std::min(x, hi)); }

// Reflect μ into (−1,1). Single reflection suffices for small steps; clamps inside to avoid ±1 exactly.
inline double reflect_mu(double mu) {
    const double eps = 1e-12;
    if (mu >  1.0) mu =  2.0 - mu;
    if (mu < -1.0) mu = -2.0 - mu;
    return clamp(mu, -1.0 + eps, 1.0 - eps);
}

/* =============================== Kolmogorov normalization per branch ==============================================
   Turbulence per branch s on [kmin,kmax]: W^s(k) = C_s k^(−5/3)
   Normalize C_s by the branch-integrated power W_s_total ≡ ∫_{kmin}^{kmax} W^s(k) dk (units of B²):
     C_s = (2/3) * W_s_total / (kmin^(−2/3) − kmax^(−2/3))
=============================================================================================================== */
inline double kolmo_C(double W_s_total_B2, double kmin, double kmax) {
    const double p = -2.0/3.0;
    const double denom = std::pow(kmin, p) - std::pow(kmax, p);
    return (2.0/3.0) * W_s_total_B2 / denom;
}

/* =============================== Plasma-frame D_{μμ}^s for branch selection =======================================
   QLT with resonance/band gates:
     k_res^s = Ω / [γ (s V_A − v μ)]  (require s V_A − v μ > 0 and k_res ∈ [kmin,kmax]).
     D_{μμ}^s = (π/2)(Ω²/B²)(1−μ²) * W^s(k_res^s) / |v μ − s V_A|
=============================================================================================================== */
struct DmumuBranch {
    double Dmumu = 0.0;  // s^−1
    double kres  = std::numeric_limits<double>::quiet_NaN(); // 1/m (diagnostic)
};

inline DmumuBranch plasma_Dmumu_branch(
    int s,                 // +1 forward, −1 backward
    double v, double mu, double gamma,
    double B, double VA, double Omega,
    double C_s, double kmin, double kmax
) {
    DmumuBranch out{};
    const double gate = s*VA - v*mu;                 // resonance inequality: must be > 0
    if (gate <= 0.0) return out;

    const double kres = Omega / (gamma * gate);      // > 0 by construction here
    if (kres < kmin || kres > kmax) return out;      // out of band

    const double Wk   = C_s * std::pow(kres, -5.0/3.0);
    const double pref = (M_PI * 0.5) * (Omega*Omega)/(B*B);
    const double denom= std::abs(v*mu - s*VA);
    out.Dmumu = pref * (1.0 - mu*mu) * (Wk / denom);
    out.kres  = kres;
    return out;
}

/* =============================== Branch selection (all v, μ) =======================================================
   Compute D_{μμ}^± with exact gating; if both > 0, pick s with P(s)=D^s/(D^+ + D^−); if neither -> s=0.
   This reproduces the familiar “s·μ<0” limit when V_A≪v and W⁺≈W⁻, but remains correct near |μ|≲V_A/v
   and for cross-helicity W⁺≠W⁻.
=============================================================================================================== */
inline int SelectBranchProton(
    double v, double mu, double gamma,
    double B, double VA, double Omega,
    double C_plus, double C_minus,
    double kmin, double kmax
) {
    const DmumuBranch Dp = plasma_Dmumu_branch(+1, v, mu, gamma, B, VA, Omega, C_plus,  kmin, kmax);
    const DmumuBranch Dm = plasma_Dmumu_branch(-1, v, mu, gamma, B, VA, Omega, C_minus, kmin, kmax);
    const double Dsum = Dp.Dmumu + Dm.Dmumu;
    if (Dsum <= 0.0) return 0;
    const double p_plus = Dp.Dmumu / Dsum;
    return (rnd() < p_plus) ? +1 : -1;
}

/* =============================== SDE-based small-angle scattering (preferred) ======================================
   One Monte-Carlo scattering step for a PROTON in Alfvénic Kolmogorov turbulence.
   Inputs: plasma-frame (v∥, v⊥), B, V_A, branch-integrated powers W⁺, W⁻ (B² units), band [kmin,kmax], dt.
   Outputs: new plasma-frame state; diagnostics; internally selects branch and does boost/kick/boost-back.

   NOTE: Ensuring dt_plasma ≫ 2π/Ω is recommended (gyro-averaging for QLT), but not enforced.
=============================================================================================================== */
ScatterResult ScatterStepProton(
    double vpar, double vperp, double B, double VA,
    double W_plus_total_B2, double W_minus_total_B2,
    double kmin, double kmax, double dt_plasma,
    double q,     // proton charge (C)
    double m,   // proton mass (kg)
    double c    // speed of light (m/s)
) {
    ScatterResult R{};

    // Plasma-frame invariants
    const double v     = std::hypot(vpar, vperp);
    const double beta2 = (v*v)/(c*c);
    const double gamma = 1.0/std::sqrt(std::max(1.0 - beta2, 1e-30));
    const double mu    = (v>0.0) ? (vpar/v) : 0.0;
    const double Omega = (q*B)/(gamma*m);

    // Kolmogorov normalizations per branch (from integrated δB²)
    const double C_plus  = kolmo_C(W_plus_total_B2,  kmin, kmax);
    const double C_minus = kolmo_C(W_minus_total_B2, kmin, kmax);

    // Select wave branch
    const int s = SelectBranchProton(v, mu, gamma, B, VA, Omega, C_plus, C_minus, kmin, kmax);
    if (s == 0) {
        // No resonant scattering power in band
        R.vpar_new=vpar; R.vperp_new=vperp; R.mu_new=mu; R.gamma_new=gamma; R.branch=0; R.scattered=false; R.dt_used=0.0;
        return R;
    }
    R.branch = s;

    // Lorentz boost to wave frame (exact along B̂)
    const double u   = s*VA;
    const double c2  = c*c;
    const double gu  = 1.0/std::sqrt(std::max(1.0 - (u*u)/c2, 1e-30));
    const double fac = 1.0 - (u*vpar)/c2;
    const double vpar_p  = (vpar - u)/fac;
    const double vperp_p =  vperp/(gu*fac);
    const double v_p     = std::hypot(vpar_p, vperp_p);
    const double mu_p    = (v_p>0.0) ? (vpar_p/v_p) : 0.0;
    const double gamma_p = gu * gamma * fac;

    // Map dt to wave frame
    double dt_p = gu * fac * dt_plasma;

    // Wave-frame resonance and closed-form Kolmogorov D′_{μμ}
    const double mu_min  = 1e-2; // regularization near μ′=0
    const double mu_eval = (std::abs(mu_p) < mu_min) ? (mu_min * sgn(mu_p==0.0? +1.0 : mu_p)) : mu_p;
    const double k_res_p = Omega / (gamma_p * v_p * std::abs(mu_eval));
    if (k_res_p < kmin || k_res_p > kmax) {
        // No wave-frame resonant k in band → no kick (identity back-transform)
        const double den = 1.0 + (u*vpar_p)/c2;
        const double vpar_new  = (vpar_p + u)/den;
        const double vperp_new =  vperp_p/(gu*den);
        const double v_new     = std::hypot(vpar_new, vperp_new);
        const double mu_new    = (v_new>0.0) ? (vpar_new/v_new) : 0.0;
        const double gamma_new = gu * gamma_p * den;
        R.vpar_new=vpar_new; R.vperp_new=vperp_new; R.mu_new=mu_new; R.gamma_new=gamma_new; R.delta_mu=mu_new - mu; R.scattered=false; R.dt_used=0.0;
        return R;
    }

    const double C_s  = (s==+1) ? C_plus : C_minus;
    const double Ks_p = (M_PI*0.5) * (C_s/(B*B)) * std::pow(Omega, 1.0/3.0)
                      * std::pow(gamma_p * v_p, 5.0/3.0) / std::max(v_p, 1e-300);

    auto Dprime = [&](double muprime)->double {
        const double mabs = std::max(std::abs(muprime), mu_min);
        return Ks_p * (1.0 - muprime*muprime) * std::pow(mabs, 2.0/3.0);
    };
    auto dDprime_dmu = [&](double muprime)->double {
        const int    sgnmu = sgn(muprime==0.0? 1.0 : muprime);
        const double mabs  = std::max(std::abs(muprime), mu_min);
        const double term1 = (2.0/3.0) * (1.0 - muprime*muprime) * std::pow(mabs, -1.0/3.0) * double(sgnmu);
        const double term2 = 2.0 * muprime * std::pow(mabs,  2.0/3.0);
        return Ks_p * (term1 - term2);
    };

    // Small-angle safety: Δt′ ≤ f_sa (1 − μ′²)/D′(μ′)
    const double f_sa    = 0.05;
    const double Dp_here = Dprime(mu_p);
    if (Dp_here > 0.0) {
        const double dt_max = f_sa * (1.0 - mu_p*mu_p) / std::max(Dp_here, 1e-300);
        dt_p = std::min(dt_p, dt_max);
    }
    R.dt_used = dt_p;

    // Itô SDE kick on μ′: Δμ′ = [−∂μ′D′] Δt′ + √(2 D′ Δt′) Ν(0,1)
    const double drift = - dDprime_dmu(mu_p) * dt_p;
    const double diff  = std::sqrt(std::max(2.0 * Dp_here * dt_p, 0.0));
    double mu_p_new    = reflect_mu(mu_p + drift + diff * Vector3D::Distribution::Normal());

    // Elastic in wave frame: update components from μ′_new
    const double vpar_p_new  = v_p * mu_p_new;
    const double vperp_p_new = v_p * std::sqrt(std::max(1.0 - mu_p_new*mu_p_new, 0.0));

    // Back to plasma frame
    const double den        = 1.0 + (u * vpar_p_new)/(c2);
    const double vpar_new   = (vpar_p_new + u)/den;
    const double vperp_new  =  vperp_p_new/(gu*den);
    const double v_new      = std::hypot(vpar_new, vperp_new);
    const double mu_new     = (v_new>0.0) ? (vpar_new/v_new) : 0.0;
    const double gamma_new  = gu * gamma_p * den;

    R.vpar_new=vpar_new; R.vperp_new=vperp_new; R.mu_new=mu_new; R.gamma_new=gamma_new; R.delta_mu=mu_new - mu; R.scattered=true;
    return R;
}

/* =============================== Uniform-μ′ “hard” scattering ======================================================
   Draw μ′ uniformly over the RESONANT band implied by k′∈[kmin,kmax]:
     k′ = Ω/(γ′ v′ |μ′|) ⇒ |μ′| ∈ [ μ_abs_min , μ_abs_max ], where
       μ_abs_min = Ω/(γ′ v′ k_max),  μ_abs_max = min(1, Ω/(γ′ v′ k_min)).
   Choose sign(μ′) uniformly ±1. If the band is empty, no scattering occurs.
   This is a coarse collision operator (rapid isotropization in the wave frame).

   NOTE: We still use the SAME branch-selection logic as SDE (fully correct for all v, μ).
=============================================================================================================== */
ScatterResult ScatterStepProton_UniformMuPrime(
    double vpar, double vperp, double B, double VA,
    double W_plus_total_B2, double W_minus_total_B2,
    double kmin, double kmax, double /*dt_plasma*/,
    double q, // = 1.602176634e-19,     // C
    double m, // = 1.67262192369e-27,   // kg
    double c // = 299792458.0          // m/s
) {
    ScatterResult R{};

    // Plasma-frame invariants
    const double v     = std::hypot(vpar, vperp);
    const double beta2 = (v*v)/(c*c);
    const double gamma = 1.0/std::sqrt(std::max(1.0 - beta2, 1e-30));
    const double mu    = (v>0.0) ? (vpar/v) : 0.0;
    const double Omega = (q*B)/(gamma*m*c);

    // Kolmogorov normalizations (used only for branch selection)
    const double C_plus  = kolmo_C(W_plus_total_B2,  kmin, kmax);
    const double C_minus = kolmo_C(W_minus_total_B2, kmin, kmax);

    // Select branch
    const int s = SelectBranchProton(v, mu, gamma, B, VA, Omega, C_plus, C_minus, kmin, kmax);
    if (s == 0) {
        R.vpar_new=vpar; R.vperp_new=vperp; R.mu_new=mu; R.gamma_new=gamma; R.branch=0; R.scattered=false; R.dt_used=0.0;
        return R;
    }
    R.branch = s;

    // Boost to wave frame
    const double u   = s*VA;
    const double c2  = c*c;
    const double gu  = 1.0/std::sqrt(std::max(1.0 - (u*u)/c2, 1e-30));
    const double fac = 1.0 - (u*vpar)/c2;
    const double vpar_p  = (vpar - u)/fac;
    const double vperp_p =  vperp/(gu*fac);
    const double v_p     = std::hypot(vpar_p, vperp_p);
    const double gamma_p = gu * gamma * fac;

    // Allowed |μ′| band from k′ ∈ [kmin, kmax]
    const double eps_mu = 1e-12;
    const double mu_abs_min = (gamma_p>0.0 && v_p>0.0) ? (Omega/(gamma_p * v_p * kmax)) : std::numeric_limits<double>::infinity();
    const double mu_abs_max = (gamma_p>0.0 && v_p>0.0) ? std::min(1.0 - eps_mu, Omega/(gamma_p * v_p * kmin)) : -1.0;

    if (!(mu_abs_min <= mu_abs_max) || !(mu_abs_min < 1.0)) {
        // No resonant μ′ available in band → no scatter
        const double den = 1.0 + (u * vpar_p)/c2;
        const double vpar_new  = (vpar_p + u)/den;
        const double vperp_new =  vperp_p/(gu*den);
        const double v_new     = std::hypot(vpar_new, vperp_new);
        const double mu_new    = (v_new>0.0) ? (vpar_new/v_new) : 0.0;
        const double gamma_new = gu * gamma_p * den;
        R.vpar_new=vpar_new; R.vperp_new=vperp_new; R.mu_new=mu_new; R.gamma_new=gamma_new; R.delta_mu=mu_new - mu;
        R.scattered=false; R.dt_used=0.0;
        return R;
    }

    // Draw μ′ uniformly over allowed band, with random sign
    const double u01   = rnd();
    const double sign  = (rnd() < 0.5) ? -1.0 : +1.0;
    const double muabs = mu_abs_min + u01 * (mu_abs_max - mu_abs_min);
    double mu_p_new    = sign * clamp(muabs, 0.0, 1.0 - eps_mu);

    // Elastic in wave frame
    const double vpar_p_new  = v_p * mu_p_new;
    const double vperp_p_new = v_p * std::sqrt(std::max(1.0 - mu_p_new*mu_p_new, 0.0));

    // Back to plasma frame
    const double den        = 1.0 + (u * vpar_p_new)/c2;
    const double vpar_new   = (vpar_p_new + u)/den;
    const double vperp_new  =  vperp_p_new/(gu*den);
    const double v_new      = std::hypot(vpar_new, vperp_new);
    const double mu_new     = (v_new>0.0) ? (vpar_new/v_new) : 0.0;
    const double gamma_new  = gu * gamma_p * den;

    R.vpar_new=vpar_new; R.vperp_new=vperp_new; R.mu_new=mu_new; R.gamma_new=gamma_new;
    R.delta_mu = mu_new - mu;
    R.scattered = true; R.dt_used=0.0;
    return R;
}

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP


/* =================================================== DEMOS ==========================================================
   Build: g++ -std=c++17 -O2 -march=native -DNDEBUG sep_alfven_kolmogorov_demo.cpp -o demo

   The examples below show (1) a single SDE step, (2) a short SDE trajectory, (3) a uniform-μ′ step.
   All numbers are illustrative only.
=================================================================================================================== */
#ifdef SEP_ALFV_DEMO_MAIN
int main() {
    using namespace SEP::AlfvenTurbulence_Kolmogorov;
    std::cout.setf(std::ios::scientific);
    std::cout << std::setprecision(6);

    // ----------------------------- Environment (SI units) -----------------------------------------------------------
    const double B   = 5e-9;        // Tesla
    const double VA  = 5.0e5;       // m/s
    const double kmin= 1e-8;        // 1/m
    const double kmax= 1e-4;        // 1/m

    // Branch-integrated turbulence powers in units of B² (Tesla²).
    // If you have energy densities W_energy (J/m^3), convert once: δB² = 2 μ0 W_energy (SI).
    const double Wplus_B2  = 5e-19; // T²
    const double Wminus_B2 = 5e-19; // T²

    // ----------------------------- Initial particle (plasma frame) -------------------------------------------------
    double vpar  = 2.0e7;  // m/s
    double vperp = 3.0e7;  // m/s
    auto speed   = [](double vp, double vn){ return std::hypot(vp, vn); };
    auto mu_now  = [&](){ double v=speed(vpar,vperp); return (v>0.0)? (vpar/v) : 0.0; };

    // ----------------------------- (1) Single SDE step --------------------------------------------------------------
    double dt = 0.1; // s (ensure dt ≫ 2π/Ω for QLT in production)
    auto r1 = ScatterStepProton(vpar, vperp, B, VA, Wplus_B2, Wminus_B2, kmin, kmax, dt);
    std::cout << "[SDE single step]\n"
              << "  branch s       = " << r1.branch << "\n"
              << "  scattered      = " << (r1.scattered ? "true" : "false") << "\n"
              << "  dt_used (wave) = " << r1.dt_used << " s\n"
              << "  mu_old         = " << mu_now() << "\n"
              << "  mu_new         = " << r1.mu_new << "\n"
              << "  delta_mu       = " << r1.delta_mu << "\n"
              << "  gamma_new      = " << r1.gamma_new << "\n\n";
    vpar  = r1.vpar_new;
    vperp = r1.vperp_new;

    // ----------------------------- (2) Short SDE trajectory ---------------------------------------------------------
    std::cout << "[SDE 20-step trajectory]\n";
    for (int i=0;i<20;++i) {
        auto r = ScatterStepProton(vpar, vperp, B, VA, Wplus_B2, Wminus_B2, kmin, kmax, dt);
        vpar  = r.vpar_new;
        vperp = r.vperp_new;
        std::cout << "  step " << std::setw(2) << i+1
                  << "  s=" << std::setw(2) << r.branch
                  << "  mu=" << std::setw(12) << r.mu_new
                  << "  dμ=" << std::setw(12) << r.delta_mu
                  << "  γ="  << std::setw(12) << r.gamma_new
                  << (r.scattered ? "" : "  (no kick)") << "\n";
    }
    std::cout << "\n";

    // ----------------------------- (3) One uniform-μ′ step ----------------------------------------------------------
    // Reset a particle
    vpar  = 1.0e7;
    vperp = 4.0e7;
    auto r2 = ScatterStepProton_UniformMuPrime(vpar, vperp, B, VA, Wplus_B2, Wminus_B2, kmin, kmax, dt);
    std::cout << "[Uniform mu' single step]\n"
              << "  branch s       = " << r2.branch << "\n"
              << "  scattered      = " << (r2.scattered ? "true" : "false") << "\n"
              << "  mu_new         = " << r2.mu_new << "\n"
              << "  delta_mu       = " << r2.delta_mu << "\n"
              << "  gamma_new      = " << r2.gamma_new << "\n";

    return 0;
}
#endif

