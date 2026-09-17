#ifndef _EARTH_ADAPTIVE_DIRECT_ACCESS_H_
#define _EARTH_ADAPTIVE_DIRECT_ACCESS_H_

//======================================================================================
// AdaptiveDirectAccess.h
//======================================================================================
//
// Shared adaptive rigidity sampler for the directional A(R,Omega) validation product.
//
// Motivation
// ----------
// A dense DIRECT_ACCESS run evaluates the same fixed rigidity list at every sky
// direction.  That is robust but wasteful: most directions are smoothly ALLOWED or
// PHYSICAL_FORBIDDEN across broad portions of the detector-response range, while the
// scientifically interesting structure is concentrated near access transitions and
// unresolved trace-limit brackets.
//
// This helper keeps the validation semantics explicit while reducing trajectory count:
//
//   1. Every user-supplied seed rigidity is always evaluated.  The caller therefore
//      controls the coarse global coverage and must include the detector-response
//      endpoints.
//
//   2. A configurable guard depth probes geometric midpoints even when the two current
//      endpoint states agree.  The default C19 guard depth of one samples the midpoint
//      of every seed interval and substantially reduces the risk of missing a narrow
//      allowed/forbidden pocket whose two coarse endpoints happen to have the same
//      classification.
//
//   3. After the guard probes, refinement continues only when an interval is visibly
//      ambiguous, i.e. its endpoint states differ.  This includes resolved transitions
//      and resolved<->UNRESOLVED boundaries.  An interval whose two endpoints are both
//      UNRESOLVED is *not* recursively exploded to the full tree: repeated rigidity
//      bisection cannot cure a trajectory time/path/step cap, and the Python fold already
//      carries the whole unresolved interval as an uncertainty bound.
//
//   4. The full candidate tree is deterministic and identical on every MPI rank.  A
//      direction evaluates only the nodes it needs; untouched candidate slots remain
//      -1.  This is important because Mode3D and GRIDLESS can MPI_MAX-reduce fixed-size
//      sentinel arrays even though each direction has a different adaptive sample set.
//
// The sampler deliberately does NOT assume monotonic access.  Multiple transitions are
// retained whenever the guard/refinement probes expose them.  The Python C19 fold still
// treats every sampled ALLOWED<->FORBIDDEN interval as an uncertainty bracket instead of
// inventing a fractional transmission ramp.  Thus adaptive sampling reduces work while
// preserving the existing response-weighted convergence gate.
//======================================================================================

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace EarthUtil {

struct AdaptiveDirectAccessGrid {
  std::vector<double> seed_GV;
  std::vector<double> candidate_GV;
  std::vector<std::size_t> seedCandidateIndex;
  int maxDepth{0};
};

inline double AdaptiveDirectAccessMidpointGV(double a,double b) {
  // Geometric midpoint is natural for positive rigidity and preserves comparable
  // fractional resolution across the response span.
  return std::sqrt(a*b);
}

inline void AppendAdaptiveCandidates_(double a,double b,int depth,int maxDepth,
                                      std::vector<double>& values) {
  if (depth>=maxDepth) return;
  const double m=AdaptiveDirectAccessMidpointGV(a,b);
  values.push_back(m);
  AppendAdaptiveCandidates_(a,m,depth+1,maxDepth,values);
  AppendAdaptiveCandidates_(m,b,depth+1,maxDepth,values);
}

inline std::size_t FindAdaptiveDirectAccessNode(const std::vector<double>& grid,
                                                 double value) {
  const auto it=std::lower_bound(grid.begin(),grid.end(),value);
  auto acceptable=[&](std::vector<double>::const_iterator p) {
    if (p==grid.end()) return false;
    const double scale=std::max(1.0,std::fabs(value));
    return std::fabs(*p-value)<=2.0e-13*scale;
  };
  if (acceptable(it)) return static_cast<std::size_t>(it-grid.begin());
  if (it!=grid.begin()) {
    const auto prev=it-1;
    if (acceptable(prev)) return static_cast<std::size_t>(prev-grid.begin());
  }
  throw std::runtime_error("adaptive direct-access candidate node lookup failed");
}

inline AdaptiveDirectAccessGrid BuildAdaptiveDirectAccessGrid(
    const std::vector<double>& seed_GV,int maxDepth) {
  if (seed_GV.size()<2)
    throw std::runtime_error("adaptive direct access requires at least two seed rigidities");
  if (maxDepth<0 || maxDepth>20)
    throw std::runtime_error("adaptive direct-access max depth must be in [0,20]");

  for (std::size_t i=0;i<seed_GV.size();++i) {
    if (!(seed_GV[i]>0.0) || !std::isfinite(seed_GV[i]))
      throw std::runtime_error("adaptive direct-access seed rigidities must be finite and positive");
    if (i>0 && !(seed_GV[i]>seed_GV[i-1]))
      throw std::runtime_error("adaptive direct-access seed rigidities must be strictly increasing");
  }

  AdaptiveDirectAccessGrid result;
  result.seed_GV=seed_GV;
  result.maxDepth=maxDepth;
  result.candidate_GV=seed_GV;
  for (std::size_t i=0;i+1<seed_GV.size();++i)
    AppendAdaptiveCandidates_(seed_GV[i],seed_GV[i+1],0,maxDepth,result.candidate_GV);

  std::sort(result.candidate_GV.begin(),result.candidate_GV.end());
  std::vector<double> unique;
  unique.reserve(result.candidate_GV.size());
  for (double value:result.candidate_GV) {
    if (unique.empty()) {
      unique.push_back(value);
      continue;
    }
    const double scale=std::max(1.0,std::fabs(value));
    if (std::fabs(value-unique.back())>2.0e-13*scale) unique.push_back(value);
  }
  result.candidate_GV.swap(unique);

  result.seedCandidateIndex.reserve(seed_GV.size());
  for (double value:seed_GV)
    result.seedCandidateIndex.push_back(
        FindAdaptiveDirectAccessNode(result.candidate_GV,value));
  return result;
}

// Per-trajectory diagnostics for the sparse DIRECT_ACCESS product.
//
// ``slot`` is the global flattened [location][sky-cell][candidate-rigidity] index.
// Only actually evaluated adaptive nodes create records, so the memory footprint scales
// with the number of trajectories rather than with the full depth-6 candidate tree.
// The struct intentionally contains only POD fields: Mode3D and GRIDLESS gather it as
// raw MPI_BYTE records after all worker threads have joined.
struct DirectAccessSampleDiagnostic {
  std::uint64_t slot{0};
  int terminationCode{-1};
  double traceTime_s{0.0};
  double traceDistance_Re{0.0};
  int steps{0};
  int retryCount{0};

  // Trace-budget convergence provenance.  These fields let the C19 postprocessor
  // distinguish the normal 300-s classification from an unresolved-only extended
  // result without rerunning or guessing from the final trace time.  A value of
  // traceExtensionCount==0 means the normal primary budget was sufficient.
  int primaryTerminationCode{-1};
  double primaryTraceTime_s{0.0};
  int traceExtensionCount{0};
  double initialTraceLimit_s{0.0};
  double finalTraceLimit_s{0.0};

  int mirrorPoints{0};
  int bounceCycles{0};
  int driftRevolutions{0};
  double driftAngle_deg{0.0};
  double driftMeanRadiusChange_Re{0.0};
  int trapMechanism{0};       // 0=None, 1=Bounce, 2=Drift
  double momentumRelativeSpread{0.0};
};

// Diagnostics are gathered with MPI_BYTE rather than a custom MPI datatype.  Keep
// this compile-time guard next to the record definition so adding a non-POD member
// cannot silently make the byte-wise gather invalid.
static_assert(std::is_trivially_copyable<DirectAccessSampleDiagnostic>::value,
              "DirectAccessSampleDiagnostic must remain trivially copyable");

template<class Classifier>
inline int EvaluateAdaptiveDirectAccessDirection(
    const AdaptiveDirectAccessGrid& grid,
    int guardDepth,
    std::vector<int>& states,
    std::size_t base,
    Classifier classify,
    int unresolvedState=2) {
  // `states` is a fixed-size candidate-tree slice owned by one sky direction.  A value
  // of -1 means that candidate rigidity was not needed.  The caller guarantees unique
  // ownership of this slice while the function runs, so no lock is required.
  (void)unresolvedState; // retained in the API for explicit state-semantic documentation
  if (guardDepth<0 || guardDepth>grid.maxDepth)
    throw std::runtime_error("adaptive direct-access guard depth must be in [0,maxDepth]");
  if (base+grid.candidate_GV.size()>states.size())
    throw std::runtime_error("adaptive direct-access state slice exceeds output array");

  int nEvaluations=0;
  // The classifier receives both rigidity and the deterministic candidate index.
  // The second argument lets callers attach termination/trace diagnostics to the same
  // global sparse slot without searching the candidate grid again.
  auto stateAt=[&](std::size_t idx) -> int {
    int& slot=states[base+idx];
    if (slot<0) {
      slot=classify(grid.candidate_GV[idx],idx);
      ++nEvaluations;
    }
    return slot;
  };

  // Always evaluate every coarse seed.  This establishes global coverage independent
  // of the local access topology and guarantees common lower/upper support in every
  // direction for the detector fold.
  for (std::size_t idx:grid.seedCandidateIndex) (void)stateAt(idx);

  std::function<void(double,std::size_t,double,std::size_t,int)> refine;
  refine=[&](double a,std::size_t ia,double b,std::size_t ib,int depth) {
    if (depth>=grid.maxDepth) return;
    const int sa=stateAt(ia);
    const int sb=stateAt(ib);

    const bool guardProbe=(depth<guardDepth);
    // Different states are the only evidence that a boundary lies inside this interval.
    // In particular, UNRESOLVED<->resolved is refined, while UNRESOLVED<->UNRESOLVED
    // stops after the mandatory guard probes instead of creating an expensive full tree.
    const bool visibleAmbiguity=(sa!=sb);
    if (!guardProbe && !visibleAmbiguity) return;

    const double m=AdaptiveDirectAccessMidpointGV(a,b);
    const std::size_t im=FindAdaptiveDirectAccessNode(grid.candidate_GV,m);
    (void)stateAt(im);

    // Re-enter both halves.  Each child independently decides whether another guard
    // probe or ambiguity-driven refinement is warranted.  This detects multiple
    // transitions rather than collapsing the interval to one monotonic cutoff.
    refine(a,ia,m,im,depth+1);
    refine(m,im,b,ib,depth+1);
  };

  for (std::size_t i=0;i+1<grid.seed_GV.size();++i) {
    refine(grid.seed_GV[i],grid.seedCandidateIndex[i],
           grid.seed_GV[i+1],grid.seedCandidateIndex[i+1],0);
  }
  return nEvaluations;
}

inline std::size_t CountAdaptiveDirectAccessSamples(const std::vector<int>& states,
                                                     std::size_t base,
                                                     std::size_t count) {
  if (base+count>states.size())
    throw std::runtime_error("adaptive direct-access sample-count slice exceeds array");
  std::size_t n=0;
  for (std::size_t i=0;i<count;++i) if (states[base+i]>=0) ++n;
  return n;
}

} // namespace EarthUtil

#endif
