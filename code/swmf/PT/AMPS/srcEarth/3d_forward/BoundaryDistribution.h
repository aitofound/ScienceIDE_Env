#ifndef _SRC_EARTH_3D_FORWARD_BOUNDARY_DISTRIBUTION_H_
#define _SRC_EARTH_3D_FORWARD_BOUNDARY_DISTRIBUTION_H_

//======================================================================================
// BoundaryDistribution.h
//======================================================================================
//
// PURPOSE
// -------
// Abstract strategy interface for the angular weighting of particles injected
// at the domain boundary in Mode3DForward.
//
// DESIGN
// ------
// Currently only isotropic injection is implemented (IsotropicBoundaryDistribution).
// The virtual interface reserves space for future non-isotropic modes such as:
//   - Field-aligned beam (COSALPHA_N)
//   - Pancake (SINALPHA_N)
//   - Storm-time anisotropic (DAYSIDE_NIGHTSIDE spatial factor)
//
// INJECTION PHYSICS
// -----------------
// For an isotropic external radiation environment with differential intensity J(E)
// [particles / (m^2 s sr MeV)], the one-way flux incident on a surface element
// with inward normal n_hat is:
//
//   dN/dt = π * ∫ J(E) dE  [particles / (m^2 s)]
//
// The directional PDF for an isotropic distribution on the hemisphere is:
//
//   p(v_hat) dΩ = (1/π) cos(θ) dΩ       [cos-weighted hemisphere]
//
// where θ is the angle between v_hat and n_hat.
//
// To sample this distribution, use cosine-weighted hemisphere sampling:
//
//   Given two independent uniform random numbers ξ₁, ξ₂ ∈ [0,1):
//     cos(θ)  = sqrt(ξ₁)
//     sin(θ)  = sqrt(1 − ξ₁)
//     φ       = 2π ξ₂
//
//   Then v_local = (sin(θ)cos(φ), sin(θ)sin(φ), cos(θ))  in face-local coords
//   where the z-axis (third component) is the inward face normal.
//
//======================================================================================

namespace Earth {
namespace Mode3DForward {

// ---------------------------------------------------------------------------
// BoundaryDistributionType — enum for input-file selection
// ---------------------------------------------------------------------------
enum class BoundaryDistributionType {
  Isotropic,     // uniform-intensity boundary, cos-weighted arrival directions
  // Reserved for future anisotropic modes:
  // CosBn,      // streaming-aligned (v || B at boundary)
  // SinBn,      // perpendicular (v ⊥ B at boundary)
  // DaysideNightside  // enhanced dayside flux
};

inline BoundaryDistributionType ParseBoundaryDistributionType(const std::string& s) {
  std::string u = s;
  for (char& c : u) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  if (u == "ISOTROPIC") return BoundaryDistributionType::Isotropic;
  return BoundaryDistributionType::Isotropic; // safe default
}

// ---------------------------------------------------------------------------
// BoundaryDistributionBase — abstract weighting strategy
// ---------------------------------------------------------------------------
//
// Derived classes implement GetAngularWeight() to modulate the injection weight
// for a given direction, position, and local magnetic field. The base weight
// from an isotropic flux already contains the cos(θ) factor from the
// hemisphere sampling; this function provides an additional multiplicative
// anisotropy modulation A(v_hat, B_hat, x).
//
// For isotropic injection, GetAngularWeight() returns 1.0 for all inputs.
//
class BoundaryDistributionBase {
public:
  virtual ~BoundaryDistributionBase() = default;

  // Return the dimensionless angular-weight modulation factor A ≥ 0.
  //
  // Parameters:
  //   x_m[3]        : injection point on domain boundary [m, GSM]
  //   v_hat_in[3]   : unit vector of inward injection direction
  //   B_at_bdry[3]  : background B-field at x_m [T] (may be {0,0,0} if unavailable)
  //   spec          : AMPS species index
  //
  // The physically injected weight is:
  //   w_particle = w_base * GetAngularWeight(...)
  //
  // where w_base = (π * ∫J dE * TotalBoundaryArea * dt) / N_particles_per_iter.
  //
  virtual double GetAngularWeight(const double* x_m,
                                  const double* v_hat_in,
                                  const double* B_at_bdry,
                                  int spec) const = 0;

  // Human-readable name, used in log messages and Tecplot titles.
  virtual const char* TypeName() const = 0;
};

// ---------------------------------------------------------------------------
// IsotropicBoundaryDistribution — uniform boundary (A = 1 everywhere)
// ---------------------------------------------------------------------------
class IsotropicBoundaryDistribution : public BoundaryDistributionBase {
public:
  double GetAngularWeight(const double* /*x_m*/,
                          const double* /*v_hat_in*/,
                          const double* /*B_at_bdry*/,
                          int /*spec*/) const override {
    return 1.0;
  }
  const char* TypeName() const override { return "ISOTROPIC"; }
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
inline BoundaryDistributionBase* MakeBoundaryDistribution(BoundaryDistributionType t) {
  switch (t) {
    case BoundaryDistributionType::Isotropic:
      return new IsotropicBoundaryDistribution();
  }
  return new IsotropicBoundaryDistribution(); // safe fallback
}

} // namespace Mode3DForward
} // namespace Earth

#endif // _SRC_EARTH_3D_FORWARD_BOUNDARY_DISTRIBUTION_H_
