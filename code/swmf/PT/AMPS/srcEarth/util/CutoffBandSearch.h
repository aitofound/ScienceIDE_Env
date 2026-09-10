#ifndef _EARTH_UTIL_CUTOFF_BAND_SEARCH_H_
#define _EARTH_UTIL_CUTOFF_BAND_SEARCH_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

//======================================================================================
// CutoffBandSearch.h
//======================================================================================
//
// Shared, field-independent analysis of a sampled cutoff-rigidity access function.
//
// The Mode3D and gridless solvers use different field evaluators and different private
// trajectory functions, but after one trajectory has been classified at each rigidity
// they need exactly the same topology analysis.  Keeping that analysis in this header
// prevents the two C14 branches from acquiring subtly different definitions of lower
// cutoff, upper cutoff, penumbra width, transition count, or unresolved contamination.
//
// This header deliberately contains no AMPS field or mover dependencies.  Callers supply
// a vector of increasing rigidities and a parallel vector of CutoffSampleState values.
// The result describes the coarse-grid topology.  Solver-specific code then refines a
// resolved FORBIDDEN->ALLOWED bracket with its own trajectory callback.
//======================================================================================

namespace EarthUtil {

// State of one trajectory sample in a full rigidity scan.
//
// PhysicalForbidden is intentionally narrower than the legacy Boolean meaning of
// "false".  It includes physical inner-boundary impact and a positively identified
// magnetic trap.  Time, step, and distance limits are Unresolved here, because C14's
// strict analytical profile must not silently turn a safety cap into a physical cutoff.
enum class CutoffSampleState : int {
  PhysicalForbidden = 0,
  Allowed           = 1,
  Unresolved        = 2
};

inline const char* CutoffSampleStateName(CutoffSampleState s) {
  switch (s) {
    case CutoffSampleState::PhysicalForbidden: return "PHYSICAL_FORBIDDEN";
    case CutoffSampleState::Allowed:           return "ALLOWED";
    case CutoffSampleState::Unresolved:        return "UNRESOLVED";
  }
  return "UNKNOWN";
}

// Coarse-grid topology extracted from one complete rigidity scan.
//
// Index fields refer to the caller-provided increasing rigidity grid.  A valid resolved
// transition bracket always has forbiddenIndex+1 == allowedIndex.  If an unresolved
// sample interrupts the relevant region, the corresponding *BracketUnresolved flag is
// set and the bracket indices remain -1; callers must not refine across that gap.
struct CutoffBandTopology {
  int lowerForbiddenIndex{-1};
  int lowerAllowedIndex{-1};
  int upperForbiddenIndex{-1};
  int upperAllowedIndex{-1};

  int firstAllowedIndex{-1};
  int lastForbiddenIndex{-1};

  int nTransitions{0};
  int nAllowedIntervals{0};
  int nUnresolved{0};

  bool allAllowed{false};
  bool allForbidden{false};
  bool lowerBelowRange{false};
  bool lowerAboveRange{false};
  bool upperBelowRange{false};
  bool upperAboveRange{false};
  bool lowerBracketUnresolved{false};
  bool upperBracketUnresolved{false};
};

// Analyze one complete access sequence on an increasing rigidity grid.
//
// Lower cutoff definition used by C14:
//   the first FORBIDDEN->ALLOWED transition above the low-rigidity region.  All samples
//   below that transition must be physically forbidden.  If Rmin itself is allowed, the
//   lower cutoff lies below the requested bracket and lowerBelowRange is set.  If every
//   resolved sample is forbidden, lowerAboveRange is set because no lower transition was
//   reached before Rmax.
//
// Upper cutoff definition used by C14:
//   the last FORBIDDEN->ALLOWED transition below the continuously allowed high-rigidity
//   branch.  All samples above that transition must be allowed.  If every sample is
//   allowed, upperBelowRange is set; if Rmax is forbidden, upperAboveRange is set.
//
// Unresolved samples are never skipped when they could affect either definition.  For
// example, [FORBIDDEN, UNRESOLVED, ALLOWED] does not provide a valid lower bracket even
// though a forbidden and allowed sample exist on opposite sides of the unresolved one.
inline CutoffBandTopology AnalyzeCutoffBandSamples(
    const std::vector<CutoffSampleState>& states) {
  CutoffBandTopology out;
  const int n = static_cast<int>(states.size());
  if (n == 0) return out;

  bool inAllowedInterval = false;
  for (int i=0; i<n; ++i) {
    const CutoffSampleState s = states[static_cast<std::size_t>(i)];
    if (s == CutoffSampleState::Allowed) {
      if (out.firstAllowedIndex < 0) out.firstAllowedIndex = i;
      if (!inAllowedInterval) {
        ++out.nAllowedIntervals;
        inAllowedInterval = true;
      }
    }
    else {
      inAllowedInterval = false;
      if (s == CutoffSampleState::PhysicalForbidden) out.lastForbiddenIndex = i;
      else ++out.nUnresolved;
    }

    // Count only directly observed resolved state changes.  An unresolved sample breaks
    // the sequence because no physical transition can be inferred through it.
    if (i > 0) {
      const CutoffSampleState a = states[static_cast<std::size_t>(i-1)];
      const CutoffSampleState b = s;
      if (a != CutoffSampleState::Unresolved &&
          b != CutoffSampleState::Unresolved && a != b) {
        ++out.nTransitions;
      }
    }
  }

  out.allAllowed = (out.firstAllowedIndex == 0 && out.lastForbiddenIndex < 0 &&
                    out.nUnresolved == 0);
  out.allForbidden = (out.firstAllowedIndex < 0 && out.lastForbiddenIndex == n-1 &&
                      out.nUnresolved == 0);

  // Lower cutoff topology.
  if (out.firstAllowedIndex == 0) {
    out.lowerBelowRange = true;
  }
  else if (out.firstAllowedIndex > 0) {
    bool cleanForbiddenPrefix = true;
    for (int i=0; i<out.firstAllowedIndex; ++i) {
      if (states[static_cast<std::size_t>(i)] != CutoffSampleState::PhysicalForbidden) {
        cleanForbiddenPrefix = false;
        break;
      }
    }

    if (cleanForbiddenPrefix) {
      out.lowerForbiddenIndex = out.firstAllowedIndex-1;
      out.lowerAllowedIndex = out.firstAllowedIndex;
    }
    else {
      out.lowerBracketUnresolved = true;
    }
  }
  else if (out.nUnresolved > 0) {
    // No allowed sample was found, but unresolved samples prevent the stronger statement
    // that the lower cutoff lies above Rmax.
    out.lowerBracketUnresolved = true;
  }
  else {
    // Every coarse sample is physically forbidden.  No lower transition exists inside
    // the requested bracket, so the lower cutoff is above Rmax rather than numerically
    // equal to an arbitrary endpoint.
    out.lowerAboveRange = true;
  }

  // Upper cutoff topology.
  if (out.lastForbiddenIndex < 0) {
    if (out.nUnresolved > 0) out.upperBracketUnresolved = true;
    else out.upperBelowRange = true;  // every sample is allowed
  }
  else if (out.lastForbiddenIndex == n-1) {
    out.upperAboveRange = true;
  }
  else {
    bool cleanAllowedSuffix = true;
    for (int i=out.lastForbiddenIndex+1; i<n; ++i) {
      if (states[static_cast<std::size_t>(i)] != CutoffSampleState::Allowed) {
        cleanAllowedSuffix = false;
        break;
      }
    }

    if (cleanAllowedSuffix) {
      out.upperForbiddenIndex = out.lastForbiddenIndex;
      out.upperAllowedIndex = out.lastForbiddenIndex+1;
    }
    else {
      out.upperBracketUnresolved = true;
    }
  }

  return out;
}

// Result of refining one already-resolved FORBIDDEN->ALLOWED bracket.
struct RefinedCutoffTransition {
  double cutoff_GV{-1.0};
  int iterations{0};
  bool unresolved{false};
};

// Generic local refinement shared by Mode3D and gridless PENUMBRA_SCAN.
//
// classify(R) must return CutoffSampleState and may throw for a genuine numerical or
// field failure.  The bracket invariant is lo=PhysicalForbidden, hi=Allowed.  If a
// midpoint reaches a configured trajectory limit, refinement stops immediately and
// unresolved=true is returned; the allowed side is retained only as a diagnostic value
// and must not be accepted by STRICT_STORMER.
template <class Classifier>
inline RefinedCutoffTransition RefineCutoffTransition(
    double Rforbid_GV,
    double Rallow_GV,
    Classifier classify,
    double absoluteTolerance_GV = 1.0e-3,
    double relativeTolerance = 1.0e-6) {
  RefinedCutoffTransition out;
  if (!(Rallow_GV > Rforbid_GV)) return out;

  const double tol = std::max(
      absoluteTolerance_GV,
      relativeTolerance*std::max(std::fabs(Rforbid_GV),std::fabs(Rallow_GV)));

  double lo = Rforbid_GV;
  double hi = Rallow_GV;
  while ((hi-lo) > tol) {
    const double mid = 0.5*(lo+hi);
    const CutoffSampleState s = classify(mid);
    ++out.iterations;
    if (s == CutoffSampleState::Allowed) hi = mid;
    else if (s == CutoffSampleState::PhysicalForbidden) lo = mid;
    else {
      out.unresolved = true;
      out.cutoff_GV = hi;
      return out;
    }
  }

  out.cutoff_GV = hi;
  return out;
}

// Result of integrating the resolved allowed/forbidden access bands between the
// lower and upper cutoff boundaries.
//
// The effective vertical cutoff used by Smart--Shea, CARI-7, and Gerontidou is
//
//   Rc_effective = Rc_upper - integral(Rc_lower..Rc_upper) T(R) dR,
//
// where T(R)=1 for an allowed trajectory and T(R)=0 for a physically forbidden
// trajectory.  Thus allowedWidth_GV is the total measure of all allowed islands
// inside the penumbra.  A midpoint that terminates at a numerical safety limit
// makes the integral unresolved; the caller must not silently count it as either
// allowed or forbidden.
struct EffectiveCutoffIntegration {
  double cutoff_GV{-1.0};
  double allowedWidth_GV{0.0};
  int refinedTransitions{0};
  bool unresolved{false};
};

// Integrate the sampled access function between already determined lower and
// upper cutoff boundaries.
//
// REQUIREMENTS
//   * rigidity_GV is strictly increasing and has the same length as states;
//   * lower_GV and upper_GV are resolved physical boundaries with
//     0 <= lower_GV <= upper_GV;
//   * classify(R) returns the physical state at an arbitrary rigidity and may
//     throw for a genuine field or integration failure.
//
// Each coarse interval whose endpoint states differ is assumed to contain one
// transition and is refined by bisection.  Both orientations are supported:
// FORBIDDEN->ALLOWED and ALLOWED->FORBIDDEN.  This is intentionally separate
// from RefineCutoffTransition(), whose invariant is specifically the first
// orientation needed for lower/upper boundary refinement.
//
// The implementation works with LINEAR and LOG input grids because it integrates
// physical rigidity widths after transition refinement rather than counting grid
// cells.  It is field-independent and therefore testable without AMPS, Geopack,
// MPI, or a particle mover.
template <class Classifier>
inline EffectiveCutoffIntegration IntegrateEffectiveCutoff(
    const std::vector<double>& rigidity_GV,
    const std::vector<CutoffSampleState>& states,
    double lower_GV,
    double upper_GV,
    Classifier classify,
    double absoluteTolerance_GV = 1.0e-3,
    double relativeTolerance = 1.0e-6) {
  EffectiveCutoffIntegration out;

  if (rigidity_GV.size() != states.size() || rigidity_GV.size() < 2) return out;
  if (!(lower_GV >= 0.0) || !(upper_GV >= lower_GV)) return out;

  if (upper_GV == lower_GV) {
    out.cutoff_GV = lower_GV;
    return out;
  }

  for (std::size_t i=0; i+1<rigidity_GV.size(); ++i) {
    const double gridLo = rigidity_GV[i];
    const double gridHi = rigidity_GV[i+1];
    if (!(gridHi > gridLo)) return EffectiveCutoffIntegration{};

    // Clip this coarse interval to the physical penumbra.  Intervals entirely
    // below Rc_lower or above Rc_upper make no contribution.
    const double a = std::max(gridLo,lower_GV);
    const double b = std::min(gridHi,upper_GV);
    if (!(b > a)) continue;

    const CutoffSampleState left = states[i];
    const CutoffSampleState right = states[i+1];
    if (left == CutoffSampleState::Unresolved ||
        right == CutoffSampleState::Unresolved) {
      out.unresolved = true;
      return out;
    }

    if (left == right) {
      if (left == CutoffSampleState::Allowed) out.allowedWidth_GV += b-a;
      continue;
    }

    // The coarse endpoints straddle one resolved transition.  Preserve the
    // state on the low-rigidity side as the bisection invariant; this handles
    // either transition orientation with the same code.
    double lo = gridLo;
    double hi = gridHi;
    const CutoffSampleState lowState = left;
    const double tolerance_GV = std::max(
        absoluteTolerance_GV,
        relativeTolerance*std::max(std::fabs(lo),std::fabs(hi)));

    while ((hi-lo) > tolerance_GV) {
      const double mid = 0.5*(lo+hi);
      const CutoffSampleState midState = classify(mid);
      ++out.refinedTransitions;
      if (midState == CutoffSampleState::Unresolved) {
        out.unresolved = true;
        return out;
      }
      if (midState == lowState) lo = mid;
      else hi = mid;
    }

    // The midpoint of the final bracket is symmetric with respect to the two
    // transition orientations.  The caller's boundary refinement separately
    // uses the allowed side when it needs a conservative cutoff value.
    const double transition_GV = 0.5*(lo+hi);
    if (left == CutoffSampleState::Allowed) {
      out.allowedWidth_GV +=
          std::max(0.0,std::min(transition_GV,b)-a);
    }
    else {
      out.allowedWidth_GV +=
          std::max(0.0,b-std::max(transition_GV,a));
    }
  }

  out.cutoff_GV = std::max(
      lower_GV,
      std::min(upper_GV,upper_GV-out.allowedWidth_GV));
  return out;
}

} // namespace EarthUtil

#endif
