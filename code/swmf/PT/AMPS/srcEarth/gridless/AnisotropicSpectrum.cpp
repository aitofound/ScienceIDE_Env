//======================================================================================
// AnisotropicSpectrum.cpp
//======================================================================================
//
// IMPLEMENTATION NOTES
// --------------------
// See AnisotropicSpectrum.h for the complete physics derivation, PAD and spatial
// model descriptions, extension paths, and step-by-step algorithm overview.
//
// This file implements EvalAnisotropyFactor: the per-trajectory weight function
// f_aniso = f_PAD(cos_alpha) * f_spatial(x_exit_m) used by the ANISOTROPIC branch
// of ComputeT_atEnergy in DensityGridless.cpp.
//
//======================================================================================
// NUMERICAL SAFEGUARDS
//======================================================================================
//
// cos_alpha CLAMPING
// ------------------
// The cos_alpha input is the dot product v_exit_unit . B_hat(x_exit), where both
// vectors are computed independently. Due to floating-point rounding in the Boris
// pusher and in the field normalisation, the result can occasionally fall slightly
// outside [-1, 1] (e.g., cos_alpha = 1.0000000000000002). We clamp:
//
//   cos_alpha = max(-1.0, min(1.0, cos_alpha))
//
// before using it in std::pow or any sqrt. Without this clamp:
//   - SINALPHA_N computes sin^2 = 1 - cos^2, which would be slightly negative
//     at |cos_alpha| = 1 + epsilon, causing std::pow to return NaN.
//   - COSALPHA_N / BIDIRECTIONAL compute |cos_alpha|^n, which is harmless at
//     |cos_alpha| = 1 + epsilon since we take abs first; but clamping is cheap
//     and documents the invariant.
//
// ZERO-MAGNITUDE FIELD GUARD
// --------------------------
// If the field evaluator returns B = 0 (which can happen in pathological analytic
// field configurations, e.g., exactly on the dipole axis), B_hat is undefined and
// cos_alpha returned by the tracer will be NaN or zero depending on how the
// normalisation was done. The tracer already guards against this (see
// CutoffRigidityGridless.cpp, TraceAllowedImpl), but as a second line of defence:
//   - If cos_alpha is NaN at entry, clamp to 0.0 (corresponding to a perpendicular
//     direction; this is the most neutral choice for a field-aligned PAD).
//
// EXPONENT EDGE CASES
// -------------------
// padExponent = 0.0:
//   sin^0 = 1, |cos|^0 = 1 regardless of alpha. The result is isotropic, matching
//   the ISOTROPIC model. We do NOT special-case this because std::pow(x, 0.0) = 1.0
//   in IEEE 754 for all x including x=0, so it is well-defined.
//
// padExponent < 0.0:
//   Negative exponents are not physically meaningful (they would diverge at
//   sin=0 or cos=0 respectively). The parameter parser validates padExponent >= 0;
//   no in-loop check is needed here.
//
//======================================================================================
// PERFORMANCE NOTES
//======================================================================================
//
// This function is called once per ALLOWED trajectory (not per total trajectory).
// In a typical ANISOTROPIC run with 1152 directions and ~40% allowed fraction, that
// is ~460 calls per energy point per observation point. Each call is O(1) with a
// single std::pow, one abs, and one branch on the spatial model.
//
// std::pow is the most expensive operation here. For integer exponents (n=1,2,4),
// callers can optionally pass them as-is because the compiler will often optimise
// pow(x, 2) into x*x. For non-integer n (e.g., n=1.5), a full log-exp evaluation
// is performed internally by the C runtime.
//
// If profiling shows EvalAnisotropyFactor is a bottleneck (unlikely), a fast-path
// for integer exponents can be added using:
//   int n = (int)padExponent;
//   if (n == padExponent) { return integer_pow(base, n); }
//
//======================================================================================

#include "AnisotropicSpectrum.h"

#include <cmath>
#include <stdexcept>
#include <string>

double EvalAnisotropyFactor(const EarthUtil::AnisotropyParam& par,
                             double cos_alpha,
                             const double x_exit_m[3]) {

  //===========================================================================
  // 1. Pitch-angle distribution factor  f_PAD(cos_alpha)
  //===========================================================================
  // We clamp cos_alpha to [-1, 1] to guard against tiny floating-point
  // excursions that could make sin^2 slightly negative.
  //
  // IMPORTANT ROBUSTNESS NOTE
  //   The header comment for this file already documents the intended NaN
  //   safeguard: if the incoming pitch-angle cosine is NaN, treat it as 0.0
  //   before evaluating the PAD.  Prior to this patch, the code only performed
  //   a numerical clamp using '<' and '>' comparisons.  Those comparisons are
  //   BOTH false for NaN, so a NaN input slipped through unchanged.  In the
  //   COSALPHA_N / BIDIRECTIONAL branches that then produced
  //
  //       pow(fabs(NaN), n) = NaN
  //
  //   and the accumulated anisotropy-weight sum could become NaN as well.
  //
  //   We therefore implement the documented behaviour explicitly:
  //     NaN  -> 0.0  (neutral / perpendicular fallback)
  //   and only then apply the ordinary [-1,1] clamp.
  double ca = 0.0;
  if (std::isnan(cos_alpha)) {
    ca = 0.0;
  }
  else {
    ca = (cos_alpha < -1.0) ? -1.0 : (cos_alpha > 1.0 ? 1.0 : cos_alpha);
  }
  const double sa2 = std::max(0.0, 1.0 - ca*ca);  // sin^2(alpha)
  const double n   = par.padExponent;

  double f_pad = 1.0;

  const std::string& pm = par.padModel;

  if (pm == "ISOTROPIC") {
    // No angular dependence.  f_PAD = 1.
    f_pad = 1.0;
  }
  else if (pm == "SINALPHA_N") {
    // Pancake / perpendicular distribution.
    // f(alpha) = sin^n(alpha) = (sin^2(alpha))^(n/2).
    // Peaks at alpha = 90 deg (particles crossing field lines perpendicularly).
    // At alpha = 0 or 180 deg, f = 0.
    if (n == 0.0) {
      f_pad = 1.0; // sin^0 = 1, degenerates to isotropic
    } else {
      f_pad = std::pow(sa2, 0.5*n);
    }
  }
  else if (pm == "COSALPHA_N") {
    // Field-aligned / beam distribution.
    // f(alpha) = |cos(alpha)|^n.
    // Peaks at alpha = 0 and alpha = 180 deg (field-aligned particles).
    // At alpha = 90 deg, f = 0.
    if (n == 0.0) {
      f_pad = 1.0;
    } else {
      f_pad = std::pow(std::fabs(ca), n);
    }
  }
  else if (pm == "BIDIRECTIONAL") {
    // Bidirectional beam: symmetric enhancement along both field directions.
    // Mathematically f(alpha) = |cos(alpha)|^n (identical to COSALPHA_N).
    // Documenting the model name separately because the physical interpretation
    // differs: a bidirectional beam has equal intensity along +B and -B, which
    // is what |cos| naturally produces.
    if (n == 0.0) {
      f_pad = 1.0;
    } else {
      f_pad = std::pow(std::fabs(ca), n);
    }
  }
  else {
    // Unknown model: the parser validator should have caught this.
    // Throw to ensure misconfigured runs fail fast rather than silently.
    throw std::runtime_error(
      "EvalAnisotropyFactor: unknown BA_PAD_MODEL '" + pm +
      "'. Supported: ISOTROPIC | SINALPHA_N | COSALPHA_N | BIDIRECTIONAL");
  }

  //===========================================================================
  // 2. Spatial modulation factor  f_spatial(x_exit)
  //===========================================================================
  // x_exit_m[0] is the GSM X coordinate (positive = sunward / dayside).
  // Other coordinate components could be used for more complex models.
  //===========================================================================
  double f_spatial = 1.0;

  const std::string& sm = par.spatialModel;

  if (sm == "UNIFORM") {
    // No spatial variation.
    f_spatial = 1.0;
  }
  else if (sm == "DAYSIDE_NIGHTSIDE") {
    // Step function at the GSM X=0 plane (noon-midnight meridian).
    //   GSM x > 0 : dayside / sunward boundary -> BA_DAYSIDE_FACTOR
    //   GSM x <= 0: nightside / tailward        -> BA_NIGHTSIDE_FACTOR
    f_spatial = (x_exit_m[0] > 0.0) ? par.daysideFactor : par.nightsideFactor;
  }
  else {
    throw std::runtime_error(
      "EvalAnisotropyFactor: unknown BA_SPATIAL_MODEL '" + sm +
      "'. Supported: UNIFORM | DAYSIDE_NIGHTSIDE");
  }

  return f_pad * f_spatial;
}
