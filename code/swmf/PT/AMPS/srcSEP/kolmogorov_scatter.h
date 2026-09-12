#ifndef SEP_ALFVEN_TURBULENCE_KOLMOGOROV_HPP
#define SEP_ALFVEN_TURBULENCE_KOLMOGOROV_HPP
/*
======================================================================================================================
   SEP :: AlfvenTurbulence_Kolmogorov — Header
   Proton scattering & stochastic acceleration by slab Alfvén turbulence with a Kolmogorov spectrum

   WHAT THIS MODULE PROVIDES
   -------------------------
   Public API to perform ONE Monte-Carlo scattering step for a relativistic proton moving through
   field-aligned (slab) Alfvénic turbulence, assuming a Kolmogorov spectrum W^s(k) ∝ k^(−5/3) on a
   finite k-band [kmin, kmax]. Two scattering modes are exposed:

     1) ScatterStepProton(...) — SDE “small-angle” scattering:
        • Picks the interacting Alfvén branch s ∈ {+1, −1} using branch-resolved QLT D_{μμ} with
          exact resonance gating (valid for all speeds v and pitch angles μ).
        • Boosts to the selected wave frame (u = s V_A), performs an elastic pitch-angle kick μ′
          drawn from an Itô SDE using CLOSED-FORM D′_{μμ} and ∂μ′D′_{μμ} for Kolmogorov spectra.
        • Boosts back to the plasma frame; the energy change (γ_new ≠ γ_old) arises solely from
          the frame transform (second-order Fermi effect ∝ V_A²/v²).

     2) ScatterStepProton_UniformMuPrime(...) — uniform-μ′ “hard” scattering:
        • Same branch selection as above.
        • In the wave frame, draws μ′ uniformly over the RESONANT band implied by the k′ range.
          This is a coarse, large-angle scattering operator (rapid isotropization in wave frame).

   An additional utility is provided:

     • SelectBranchProton(...) — computes D_{μμ}^± in the plasma frame (with exact resonance/band
       gates) and returns the chosen branch s by probability weighting. Use this if you need to
       separate branch choice from the scattering step.

   PHYSICS (crib notes)
   --------------------
   • Cyclotron resonance (proton, n=+1):
       ω − k v μ = Ω/γ,   with Ω = q B/(γ m c) and Alfvén ω = s k V_A (s = ±1).
       ⇒ k_res^s = Ω / [γ (s V_A − v μ)], require (s V_A − v μ) > 0 and k_res ∈ [kmin, kmax].
   • Plasma-frame QLT pitch-angle diffusion (per branch):
       D_{μμ}^s = (π/2) (Ω²/B²) (1 − μ²) W^s(k_res^s) / |v μ − s V_A|.
   • Kolmogorov per branch:
       W^s(k) = C_s k^(−5/3) on [kmin,kmax], with
       C_s = (2/3) W_s_total / (kmin^(−2/3) − kmax^(−2/3)),
       where W_s_total ≡ ∫_{kmin}^{kmax} W^s(k) dk has UNITS OF B² (e.g., Tesla²).
       If you instead have ENERGY densities (J/m³), convert ONCE: δB_s² = 2 μ0 W_energy (SI)
       and pass δB_s² as W_s_total.

   WAVE-FRAME closed forms (used inside ScatterStepProton):
   --------------------------------------------------------
       k′_res = Ω / (γ′ v′ |μ′|).
       D′_{μμ}(μ′) = K′_s (1 − μ′²) |μ′|^{2/3},
       K′_s = (π/2) (C_s/B²) Ω^{1/3} (γ′ v′)^{5/3} / v′.
       ∂μ′D′ = K′_s [ (2/3)(1 − μ′²)|μ′|^{−1/3} sgn(μ′) − 2 μ′ |μ′|^{2/3} ].

   NUMERICAL SAFETY
   ----------------
   • Small-angle limiter in wave frame (ScatterStepProton only):
       Δt′ ≤ f_sa (1 − μ′²)/D′(μ′), with f_sa ∈ [0.01, 0.10] (implementation uses 0.05).
   • Regularize μ′→0: use |μ′| ≥ μ_min (default 1e−2) in k′_res and D′.
   • QLT/gyro-averaging: Prefer dt_plasma ≫ 2π/Ω (not enforced by the API).
   • Branch selection is ALWAYS done with the exact resonance gate (s V_A − v μ > 0) and band check.

   UNITS
   -----
   SI throughout: B (Tesla), velocities (m/s), k (1/m), time (s), q (C), m (kg), c (m/s).
   W_plus_total_B2, W_minus_total_B2 must be in UNITS OF B² (e.g., Tesla²).

   RNG HOOKS
   ---------
   The implementation expects:
     • double rnd();                                   // Uniform in [0,1)
     • double Vector3D::Distribution::Normal();        // Standard normal N(0,1)
   The reference implementation (.cpp) provides default thread-local generators. If your project has
   centralized RNGs, provide your own definitions of these two symbols.

   QUICK START (pseudo-code)
   -------------------------
     using namespace SEP::AlfvenTurbulence_Kolmogorov;

     // Plasma-frame state (SI)
     double vpar = 2.0e7, vperp = 3.0e7;
     double B = 5e-9, VA = 5e5;
     double kmin = 1e-8, kmax = 1e-4;
     double Wplus_B2 = 5e-19, Wminus_B2 = 5e-19;
     double dt = 0.1;

     // SDE step
     auto r = ScatterStepProton(vpar, vperp, B, VA, Wplus_B2, Wminus_B2, kmin, kmax, dt);
     vpar = r.vpar_new; vperp = r.vperp_new;

     // Uniform-μ′ step
     auto r2 = ScatterStepProton_UniformMuPrime(vpar, vperp, B, VA, Wplus_B2, Wminus_B2, kmin, kmax, dt);

   BUILD
   -----
   • Add this header to your include path and link against the corresponding implementation .cpp.
   • Or make the implementation header-only by inlining the definitions in your project (not provided here).

   © 2025 — Provided “as is”; verify against your unit system and validation data.
======================================================================================================================
*/

#include <cstddef>

namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {

// ---------------------------------------------------------------------------------------------------------------------
// Result bundle for a single scattering step
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Return structure containing updated plasma-frame kinematics and diagnostics.
 */
struct ScatterResult {
    // Updated PLASMA-frame kinematics
    double vpar_new;    ///< parallel speed (m/s)
    double vperp_new;   ///< perpendicular speed (m/s)
    double mu_new;      ///< pitch-angle cosine (dimensionless)
    double gamma_new;   ///< Lorentz factor (dimensionless)

    // Convenience
    double delta_mu;    ///< mu_new − mu_old

    // Diagnostics
    int    branch;      ///< +1 (forward), −1 (backward), 0 (no scattering)
    bool   scattered;   ///< true if a wave-frame kick actually occurred
    double dt_used;     ///< wave-frame dt used in the SDE step (0 for uniform-μ′ mode)
};

// ---------------------------------------------------------------------------------------------------------------------
// PUBLIC API
// ---------------------------------------------------------------------------------------------------------------------

/**
 * @brief Select the interacting Alfvén branch for a proton using full QLT resonance gating and weighting.
 *
 * Computes branch-resolved plasma-frame D_{μμ}^± (with exact inequality s V_A − v μ > 0 and
 * k_res ∈ [kmin,kmax]); if both branches are eligible, chooses s ∈ {+1,−1} with probability
 * P(s) = D^s / (D^+ + D^−). Returns 0 if neither branch is eligible.
 *
 * @param v        Particle speed in plasma frame (m/s)
 * @param mu       Pitch-angle cosine in plasma frame (dimensionless)
 * @param gamma    Lorentz factor in plasma frame (dimensionless)
 * @param B        Magnetic field magnitude (Tesla)
 * @param VA       Alfvén speed (m/s)
 * @param Omega    Relativistic proton gyrofrequency Ω = q B / (γ m c) (rad/s)
 * @param C_plus   Kolmogorov normalization for W^+(k) = C_+ k^(−5/3) on [kmin,kmax] (units of B²·m^(2/3))
 * @param C_minus  Kolmogorov normalization for W^−(k) = C_- k^(−5/3) on [kmin,kmax] (units of B²·m^(2/3))
 * @param kmin     Minimum wavenumber of turbulence band (1/m)
 * @param kmax     Maximum wavenumber of turbulence band (1/m)
 * @return int     +1, −1, or 0 (no eligible branch)
 */
int SelectBranchProton(double v, double mu, double gamma,
                       double B, double VA, double Omega,
                       double C_plus, double C_minus,
                       double kmin, double kmax);

/**
 * @brief One SDE (“small-angle”) Monte-Carlo scattering step for a PROTON in slab Alfvénic Kolmogorov turbulence.
 *
 * Algorithm:
 *  1) Select branch s with SelectBranchProton (internally).
 *  2) Boost to wave frame (u = s V_A).
 *  3) Itô kick on μ′ using closed-form D′_{μμ} and analytic ∂μ′D′_{μμ} for Kolmogorov; apply small-angle limiter.
 *  4) Boost back to plasma frame. Energy change arises from frame transform (no explicit D_pp needed).
 *
 * @param vpar                Input plasma-frame parallel speed (m/s)
 * @param vperp               Input plasma-frame perpendicular speed (m/s)
 * @param B                   Magnetic field magnitude (Tesla)
 * @param VA                  Alfvén speed (m/s)
 * @param W_plus_total_B2     Branch-integrated power ∫ W^+(k) dk on [kmin,kmax] (UNITS OF B², e.g., Tesla²)
 * @param W_minus_total_B2    Branch-integrated power ∫ W^−(k) dk on [kmin,kmax] (UNITS OF B²)
 * @param kmin                Minimum wavenumber (1/m)
 * @param kmax                Maximum wavenumber (1/m)
 * @param dt_plasma           Timestep in plasma frame (s). Prefer dt ≫ 2π/Ω for QLT.
 * @param q                   Proton charge (C). Default: 1.602176634e−19
 * @param m                   Proton mass (kg).   Default: 1.67262192369e−27
 * @param c                   Speed of light (m/s). Default: 299792458.0
 * @return ScatterResult      Updated plasma-frame state and diagnostics
 */
ScatterResult ScatterStepProton(double vpar, double vperp, double B, double VA,
                                double W_plus_total_B2, double W_minus_total_B2,
                                double kmin, double kmax, double dt_plasma,
                                double q, // = 1.602176634e-19,
                                double m, // = 1.67262192369e-27,
                                double c); // = 299792458.0);

/**
 * @brief One “hard” scattering step that draws μ′ UNIFORMLY over the RESONANT band in the wave frame.
 *
 * This operator models rapid pitch-angle randomization in the wave frame:
 *    k′ = Ω/(γ′ v′ |μ′|) ∈ [kmin,kmax] ⇒ |μ′| ∈ [ Ω/(γ′ v′ k_max),  min(1, Ω/(γ′ v′ k_min)) ].
 * The sign of μ′ is chosen ±1 with equal probability. If the band is empty, no scattering occurs.
 * Branch selection is identical to the SDE method (full QLT gating/weighting).
 *
 * @param vpar                Input plasma-frame parallel speed (m/s)
 * @param vperp               Input plasma-frame perpendicular speed (m/s)
 * @param B                   Magnetic field magnitude (Tesla)
 * @param VA                  Alfvén speed (m/s)
 * @param W_plus_total_B2     Branch-integrated power ∫ W^+(k) dk on [kmin,kmax] (UNITS OF B²)
 * @param W_minus_total_B2    Branch-integrated power ∫ W^−(k) dk on [kmin,kmax] (UNITS OF B²)
 * @param kmin                Minimum wavenumber (1/m)
 * @param kmax                Maximum wavenumber (1/m)
 * @param dt_plasma           (Unused here; kept for signature symmetry)
 * @param q                   Proton charge (C). Default: 1.602176634e−19
 * @param m                   Proton mass (kg).   Default: 1.67262192369e−27
 * @param c                   Speed of light (m/s). Default: 299792458.0
 * @return ScatterResult      Updated plasma-frame state and diagnostics
 */
ScatterResult ScatterStepProton_UniformMuPrime(double vpar, double vperp, double B, double VA,
                                               double W_plus_total_B2, double W_minus_total_B2,
                                               double kmin, double kmax, double dt_plasma,
                                               double q,
                                               double m,
                                               double c);

// ---------------------------------------------------------------------------------------------------------------------
// OPTIONAL UTILS (declare here if you plan to call from outside; otherwise keep them private in your .cpp)
// ---------------------------------------------------------------------------------------------------------------------

/**
 * @brief Kolmogorov normalization C_s for W^s(k) = C_s k^(−5/3) on [kmin,kmax], given the branch-integrated power.
 *
 * @param W_s_total_B2  ∫ W^s(k) dk on [kmin,kmax] (UNITS OF B², e.g., Tesla²)
 * @param kmin          Band minimum (1/m)
 * @param kmax          Band maximum (1/m)
 * @return double       C_s (units: B²·m^(2/3))
 *
 * @note This is provided for convenience (e.g., diagnostics). The main APIs accept W_s_total_B2
 *       and compute C_s internally; you do not need to call this explicitly.
 */
double kolmo_C(double W_s_total_B2, double kmin, double kmax);

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP

#endif // SEP_ALFVEN_TURBULENCE_KOLMOGOROV_HPP

