
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <time.h>
#include <cstdlib>
#include <algorithm>
#include <cctype>


#include <sys/time.h>
#include <sys/resource.h>

//$Id$



//the particle class
#include "pic.h"
#include "constants.h"
#include "Earth.h"

#include "GeopackInterface.h"
#include "T96Interface.h"
#include "T05Interface.h"
#include "TA16Interface.h"

// Gridless cutoff rigidity CLI/runner
#include "util/cutoff_cli.h"
#include "util/amps_param_parser.h"
#include "gridless/CutoffRigidityGridless.h"
#include "gridless/DensityGridless.h"
#include "gridless/GridlessParticleMovers.h"
#include "3d/Mode3D.h"
#include "3d_forward/Mode3DForward.h"
#include "3d_forward/ForwardParticleMovers.h"

namespace {

void InitStandaloneSpiceBeforeParamParsing(const char* modeName) {
#ifndef _NO_SPICE_CALLS_
#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
  // The standalone command-line modes parse AMPS_PARAM.in before the normal AMPS
  // mesh/PIC initialization path is entered.  That parser can already need SPICE:
  // for example, Tsyganenko driver files are converted from UTC strings to SPICE
  // ephemeris time while ParseAmpsParamFile() is reading #TEMPORAL or
  // #BACKGROUND_FIELD/DRIVER_FILE.  Without furnishing the SPICE kernels first,
  // str2et_c() may fail even though the later Mode3D/Mode3DForward runtime would
  // initialize SPICE before tracing particles.
  //
  // Do this only in standalone/non-SWMF builds.  In the live AMPS--SWMF coupling
  // configuration, SWMF owns the coupled runtime initialization sequence and may
  // furnish SPICE kernels through its own component setup.  Calling the standalone
  // SPICE initialization here would be both unnecessary and potentially confusing
  // because -mode 3d is not the normal coupled-SWMF entry point.
  (void)modeName;
  Exosphere::Init_SPICE();
#else
  (void)modeName;
#endif
#else
  (void)modeName;
#endif
}


std::string TrimCliWhitespace(const std::string& value) {
  // Return a copy with leading and trailing C-locale whitespace removed.
  //
  // The AMPS parameter parser has an internal Trim() helper, but that helper is
  // intentionally private to amps_param_parser.cpp and is therefore not part of
  // the EarthUtil public interface.  Keep the CLI merge code self-contained
  // instead of depending on an implementation-only parser function.
  //
  // Cast each character to unsigned char before std::isspace().  Passing a
  // negative signed-char value directly to the C character-classification
  // functions has undefined behavior.
  const auto isNotSpace = [](unsigned char ch) {
    return std::isspace(ch) == 0;
  };

  const auto begin = std::find_if(value.begin(), value.end(), isNotSpace);
  if (begin == value.end()) return std::string();

  const auto end = std::find_if(value.rbegin(), value.rend(), isNotSpace).base();
  return std::string(begin, end);
}

std::vector<double> ParseCutoffRigidityListCli_(const std::string& text,
                                                 std::string& errorMessage) {
  // Parse the command-line counterpart of CUTOFF_RIGIDITY_LIST_GV.
  //
  // ParseCli() stores this option as text to keep the generic CLI layer independent
  // of scientific validation.  The merge stage accepts commas and whitespace, rejects
  // a partial parse, and enforces the same positive/strictly-increasing contract as the
  // AMPS input parser.  A strict parser prevents a malformed list from silently dropping
  // one of the requested validation rigidities.
  std::string normalized=text;
  std::replace(normalized.begin(),normalized.end(),',',' ');

  std::vector<double> values;
  std::size_t pos=0;
  while (pos<normalized.size()) {
    while (pos<normalized.size() &&
           std::isspace(static_cast<unsigned char>(normalized[pos]))) ++pos;
    if (pos>=normalized.size()) break;

    std::size_t consumed=0;
    double value=0.0;
    try {
      value=std::stod(normalized.substr(pos),&consumed);
    }
    catch (const std::exception&) {
      errorMessage="cannot parse rigidity list near '"+normalized.substr(pos)+"'";
      return {};
    }
    if (consumed==0) {
      errorMessage="cannot parse rigidity list near '"+normalized.substr(pos)+"'";
      return {};
    }
    pos+=consumed;
    if (pos<normalized.size() &&
        !std::isspace(static_cast<unsigned char>(normalized[pos]))) {
      errorMessage="unexpected character in rigidity list near '"+normalized.substr(pos)+"'";
      return {};
    }
    values.push_back(value);
  }

  if (values.empty()) {
    errorMessage="rigidity list is empty";
    return {};
  }
  for (std::size_t i=0;i<values.size();++i) {
    if (!(values[i]>0.0) || !std::isfinite(values[i])) {
      errorMessage="all rigidity-list values must be finite and > 0 GV";
      return {};
    }
    if (i>0 && !(values[i]>values[i-1])) {
      errorMessage="rigidity-list values must be strictly increasing";
      return {};
    }
  }
  return values;
}

bool ApplyEpochCli(const EarthUtil::CliOptions& cli,
                   EarthUtil::AmpsParam& p,
                   const char* modeName) {
  //=================================================================================
  // Apply the global background-field epoch selected on the command line.
  //=================================================================================
  // The input parser already supports
  //
  //   #BACKGROUND_FIELD
  //   EPOCH  YYYY-MM-DDTHH:MM:SS
  //
  // and stores that value in p.field.epoch.  The CLI layer intentionally stores
  // --epoch separately in CliOptions because ParseCli() runs before the input file is
  // read and must remain independent of the physics/parser implementation.  This
  // helper is the single merge point between those two configuration sources.
  //
  // PRECEDENCE
  // ----------
  //   non-empty CLI --epoch  >  input-file EPOCH  >  BackgroundField default
  //
  // INITIALIZATION ORDER
  // --------------------
  // main() calls this helper immediately after ParseAmpsParamFile() and before the
  // selected solver is launched.  As a result, the final p.field.epoch is already
  // fixed when the runtime later performs any of the following operations:
  //
  //   * Geopack::Init() / RECALC_08 coefficient and dipole-tilt initialization;
  //   * T96/T01/T05/TA15/TA16 model initialization;
  //   * SPICE frame-rotation construction;
  //   * Mode3D mesh magnetic-field materialization;
  //   * gridless, backward-3D, or forward-3D trajectory calculations;
  //   * snapshot spectrum/driver-table selection that uses the global reference time.
  //
  // The timestamp is deliberately not reinterpreted or rewritten here.  Geopack and
  // SPICE already consume the same string elsewhere in the code, and preserving it
  // avoids introducing a second date parser with subtly different accepted formats.
  // The recommended user-facing representation is ISO-like UTC
  // YYYY-MM-DDTHH:MM:SS; timestamps containing spaces must be quoted in the shell.
  //
  // For TRAJECTORY temporal mode, individual trajectory records may carry their own
  // per-sample epochs.  This option sets the global snapshot/reference epoch and does
  // not erase those explicitly time-tagged samples.
  //=================================================================================

  if (cli.epoch.empty()) return true;

  const std::string epoch = TrimCliWhitespace(cli.epoch);
  if (epoch.empty()) {
    std::cerr << "Error: --epoch for "
              << ((modeName!=nullptr && *modeName!='\0') ? modeName : "standalone mode")
              << " must contain a non-empty UTC timestamp.\n";
    return false;
  }

  p.field.epoch = epoch;
  return true;
}

bool ApplyCutoffMoverCli(const EarthUtil::CliOptions& cli) {
  if (cli.mover.empty()) return true;

  MoverType moverType;
  if (!ParseMoverType(cli.mover, moverType)) {
    std::cerr << "Error: unknown mover option -mover " << cli.mover
              << ". Allowed: BORIS, HC4, RK2, RK4, RK6, GC2, GC4, GC6, HYBRID"
              << std::endl;
    return false;
  }

  SetDefaultMoverType(moverType);
  return true;
}


#ifndef AMPS_EARTH_RADIUS_KM
static const double kCliEarthRadiusKm = 6371.2;
#else
static const double kCliEarthRadiusKm = AMPS_EARTH_RADIUS_KM;
#endif

bool ApplyMode3DMeshResolutionCli(const EarthUtil::CliOptions& cli,
                                  EarthUtil::AmpsParam& p) {
  const bool anyCliMesh =
      cli.mode3dMeshResEarth_Re > 0.0 ||
      cli.mode3dMeshResBoundary_Re > 0.0 ||
      cli.mode3dMeshOuterRadius_Re > 0.0 ||
      !cli.mode3dMeshCoarsening.empty() ||
      cli.mode3dMeshExponent > 0.0;

  if (!anyCliMesh) return true;

  p.mode3d.meshResolutionProfileActive = true;

  if (cli.mode3dMeshResEarth_Re > 0.0)
    p.mode3d.meshResolutionEarth_km = cli.mode3dMeshResEarth_Re * kCliEarthRadiusKm;
  if (cli.mode3dMeshResBoundary_Re > 0.0)
    p.mode3d.meshResolutionBoundary_km = cli.mode3dMeshResBoundary_Re * kCliEarthRadiusKm;
  if (cli.mode3dMeshOuterRadius_Re > 0.0)
    p.mode3d.meshResolutionOuterRadius_km = cli.mode3dMeshOuterRadius_Re * kCliEarthRadiusKm;
  if (!cli.mode3dMeshCoarsening.empty())
    p.mode3d.meshResolutionCoarsening = EarthUtil::ToUpper(cli.mode3dMeshCoarsening);
  if (cli.mode3dMeshExponent > 0.0)
    p.mode3d.meshResolutionExponent = cli.mode3dMeshExponent;

  // If the CLI is the first place where the mesh profile is requested, both endpoint
  // resolutions are required.  If the input file already supplied one endpoint, the
  // CLI can override only the other.
  if (!(p.mode3d.meshResolutionEarth_km > 0.0)) {
    std::cerr << "Error: Mode3D mesh profile requires an Earth-side resolution. "
              << "Use --mode3d-mesh-res-earth-re <dRe> or "
              << "MODE3D_MESH_RES_EARTH_RE in #MODE3D_MESH.\n";
    return false;
  }
  if (!(p.mode3d.meshResolutionBoundary_km > 0.0)) {
    std::cerr << "Error: Mode3D mesh profile requires an outer-boundary resolution. "
              << "Use --mode3d-mesh-res-boundary-re <dRe> or "
              << "MODE3D_MESH_RES_BOUNDARY_RE in #MODE3D_MESH.\n";
    return false;
  }

  const std::string c = EarthUtil::ToUpper(p.mode3d.meshResolutionCoarsening);
  if (!(c=="LINEAR" || c=="LIN" ||
        c=="LOG" || c=="LOGARITHMIC" || c=="GEOMETRIC" ||
        c=="EXP" || c=="EXPONENTIAL" ||
        c=="POWER" || c=="POW" || c=="EXPONENT" || c=="POLYNOMIAL" ||
        c=="CONSTANT" || c=="CONST" || c=="UNIFORM")) {
    std::cerr << "Error: unknown --mode3d-mesh-coarsening '"
              << p.mode3d.meshResolutionCoarsening
              << "'. Valid values: LINEAR, LOG/EXPONENTIAL, POWER, CONSTANT.\n";
    return false;
  }
  if (!(p.mode3d.meshResolutionExponent > 0.0)) {
    std::cerr << "Error: --mode3d-mesh-exponent must be > 0.\n";
    return false;
  }

  return true;
}


bool ApplyCommonBackwardCli(const EarthUtil::CliOptions& cli,
                            EarthUtil::AmpsParam& p,
                            const char* modeName) {
  //=================================================================================
  // Apply CLI overrides shared by standalone Mode3D and gridless backward products.
  //=================================================================================
  // Historically these options were added first for Mode3D and were applied only in
  // the `-mode 3d` branch below.  That was confusing after the gridless cutoff code
  // was updated to use the same UPPER_SCAN rigidity search and the same MPI scheduler
  // helper as Mode3D: the parser accepted generic flags such as `-cutoff-search` and
  // gridless aliases such as `-gridless-mpi-scheduler`, but in `-mode gridless` those
  // values were not copied into AmpsParam before the solver was launched.
  //
  // Keep the command-line semantics here, in one place, so that the following flags
  // have identical meaning in both standalone execution paths:
  //
  //   -cutoff-search <UPPER_SCAN|BINARY>
  //   -cutoff-upper-scan-n <N>
  //   -adaptive-dt <T|F>
  //   -max-trace-distance <Re>
  //   -mode3d-parallel / -gridless-parallel <OPENMP|THREADS|SERIAL>
  //   -mode3d-threads / -gridless-threads <N>
  //   -mode3d-mpi-scheduler / -gridless-mpi-scheduler <DYNAMIC|BLOCK_CYCLIC|STATIC>
  //   -mode3d-mpi-dynamic-chunk / -gridless-mpi-dynamic-chunk <N>
  //
  // The actual algorithms are still implemented in the solvers.  This routine only
  // validates user-facing tokens and stores the normalized values in AmpsParam.
  // Returning false lets main() print a clean error and stop before MPI work starts.
  //=================================================================================

  const std::string modeLabel = (modeName != nullptr && *modeName != '\0') ? modeName : "standalone";

  // Density boundary-spectrum mode.  Keep this in the common helper because both
  // gridless and Mode3D density/flux solvers read the same DensitySpectrumParam.
  // The old CLI parser accepted -density-mode but the standalone dispatch path did
  // not consistently copy it into AmpsParam; applying it here makes the flag effective
  // in both backward modes.
  if (!cli.densityMode.empty()) {
    const std::string dm = EarthUtil::ToUpper(cli.densityMode);
    if (dm=="ISOTROPIC" || dm=="ISO" || dm=="UNIFORM") {
      p.densitySpectrum.boundaryMode = "ISOTROPIC";
    }
    else if (dm=="ANISOTROPIC" || dm=="ANISO") {
      p.densitySpectrum.boundaryMode = "ANISOTROPIC";
    }
    else {
      std::cerr << "Error: unknown -density-mode '" << cli.densityMode
                << "' for " << modeLabel
                << ". Valid values: ISOTROPIC or ANISOTROPIC.\n";
      return false;
    }
  }

  // Density/flux transmission-function sampling.  DIRECT keeps the legacy user energy
  // grid.  SCAN and ADAPTIVE switch the density/flux backtracing grid to a log-spaced
  // rigidity scan.  This is analogous to the cutoff upper scan, but density/flux keeps
  // the full T(E) curve and integrates it instead of reducing it to one cutoff number.
  if (!cli.densityTransmissionMode.empty()) {
    const std::string tm = EarthUtil::ToUpper(cli.densityTransmissionMode);
    if (tm=="DIRECT" || tm=="LEGACY") p.densitySpectrum.transmissionMode = "DIRECT";
    else if (tm=="SCAN" || tm=="RIGIDITY_SCAN" || tm=="RIGIDITY")
      p.densitySpectrum.transmissionMode = "SCAN";
    else if (tm=="ADAPTIVE" || tm=="ADAPT")
      p.densitySpectrum.transmissionMode = "ADAPTIVE";
    else {
      std::cerr << "Error: unknown -density-transmission-mode '"
                << cli.densityTransmissionMode
                << "' for " << modeLabel
                << ". Valid values: DIRECT, SCAN, ADAPTIVE.\n";
      return false;
    }
  }
  if (cli.densityTransmissionScanN > 0)
    p.densitySpectrum.transmissionScanN = cli.densityTransmissionScanN;
  if (cli.densityTransmissionRefineN > 0)
    p.densitySpectrum.transmissionRefineN = cli.densityTransmissionRefineN;
  if (cli.densityTransmissionMaxN > 0)
    p.densitySpectrum.transmissionMaxN = cli.densityTransmissionMaxN;
  if (p.densitySpectrum.transmissionMaxN > 0 &&
      p.densitySpectrum.transmissionScanN > p.densitySpectrum.transmissionMaxN) {
    std::cerr << "Error: density transmission scan N ("
              << p.densitySpectrum.transmissionScanN
              << ") exceeds max N (" << p.densitySpectrum.transmissionMaxN
              << ") for " << modeLabel << ".\n";
    return false;
  }

  // Shared-memory backend for backward trajectory products.  The storage names in
  // AmpsParam are historical (mode3d.densityParallelBackend/densityThreads), but the
  // same settings are intentionally consumed by BOTH standalone Mode3D and GRIDLESS.
  // The CLI parser accepts generic aliases (-density-parallel/-density-threads),
  // Mode3D aliases, and the clearer GRIDLESS aliases (-gridless-parallel/
  // -gridless-threads).  Applying them here rather than inside the Mode3D branch is
  // essential: otherwise GRIDLESS accepts the CLI flags but silently runs serially.
  if (!cli.densityParallelBackend.empty()) {
    const std::string backend = EarthUtil::ToUpper(cli.densityParallelBackend);
    if (backend=="OPENMP" || backend=="OMP" || backend=="THREADS" ||
        backend=="THREAD" || backend=="STD_THREAD" || backend=="STD_THREADS" ||
        backend=="SERIAL" || backend=="NONE") {
      p.mode3d.densityParallelBackend = backend;
    }
    else {
      std::cerr << "Error: unknown shared-memory backend '"
                << cli.densityParallelBackend << "' for " << modeLabel
                << ". Valid values: OPENMP, THREADS, SERIAL.\n";
      return false;
    }
  }
  if (cli.densityThreads > 0) p.mode3d.densityThreads = cli.densityThreads;

  // Inter-rank scheduler for backward trajectory products.  Both Mode3D and gridless
  // call Earth::Mode3D::ResolveMpiScheduler(), which reads p.mode3d.mpiScheduler even
  // when the solver is gridless.  The name is historical; the setting now means the
  // generic MPI scheduler for backward cutoff/density calculations.
  if (!cli.mode3dMpiScheduler.empty()) {
    const std::string sched = EarthUtil::ToUpper(cli.mode3dMpiScheduler);
    if (sched=="DYNAMIC" || sched=="DYN" || sched=="QUEUE" ||
        sched=="WORK_QUEUE" || sched=="WORKQUEUE" ||
        sched=="BLOCK_CYCLIC" || sched=="BLOCKCYCLIC" || sched=="CYCLIC" ||
        sched=="ROUND_ROBIN" || sched=="ROUNDROBIN" ||
        sched=="STATIC" || sched=="CONTIGUOUS" || sched=="BLOCK" || sched=="SLAB") {
      p.mode3d.mpiScheduler = sched;
    }
    else {
      std::cerr << "Error: unknown MPI scheduler value '"
                << cli.mode3dMpiScheduler
                << "' for " << modeLabel
                << ". Valid values: DYNAMIC, BLOCK_CYCLIC, STATIC.\n";
      return false;
    }
  }

  // Chunk size for DYNAMIC MPI scheduling.  A positive command-line value overrides
  // the input deck.  A zero or absent CLI value keeps the input/default behavior; the
  // resolver later interprets non-positive values as "automatic" and chooses a chunk
  // size proportional to the number of workers.
  if (cli.mode3dMpiDynamicChunk > 0) {
    p.mode3d.mpiDynamicChunk = cli.mode3dMpiDynamicChunk;
  }

  // Select the rigidity-search product used by the standalone cutoff calculation.
  //
  // UPPER_SCAN preserves the historical scalar upper-cutoff product.  It searches a
  // coarse rigidity grid for the highest forbidden sample and then refines only the
  // final forbidden/allowed transition.
  //
  // PENUMBRA_SCAN is intentionally a distinct algorithm, not an alias for UPPER_SCAN.
  // It evaluates the complete increasing rigidity grid, preserves ALLOWED,
  // PHYSICAL_FORBIDDEN, and UNRESOLVED states, and derives lower, effective, and upper
  // cutoff quantities from the same access sequence.  C6 requires this mode because
  // the Smart--Shea, CARI-7, and Gerontidou reference tables contain effective cutoff
  // rigidity rather than only the upper edge of the penumbra.
  //
  // BINARY retains the legacy endpoint-only bisection for regression/debugging.  The
  // option is shared by Mode3D and gridless; each solver still validates whether the
  // requested product is supported for its sampling configuration.
  if (!cli.cutoffSearchAlgorithm.empty()) {
    const std::string alg = EarthUtil::ToUpper(cli.cutoffSearchAlgorithm);
    if (alg=="UPPER_SCAN" || alg=="UPPERSCAN" || alg=="UPPER" ||
        alg=="SCAN") {
      p.cutoff.searchAlgorithm = "UPPER_SCAN";
    }
    else if (alg=="PENUMBRA_SCAN" || alg=="PENUMBRASCAN" ||
             alg=="PENUMBRA" || alg=="FULL_SCAN" ||
             alg=="CUTOFF_BAND" || alg=="BAND") {
      p.cutoff.searchAlgorithm = "PENUMBRA_SCAN";
    }
    else if (alg=="RIGIDITY_LIST" || alg=="FIXED_RIGIDITY" ||
             alg=="FIXED_RIGIDITIES" || alg=="ACCESS_LIST") {
      p.cutoff.searchAlgorithm = "RIGIDITY_LIST";
    }
    else if (alg=="DIRECT_ACCESS") {
      // DIRECT_ACCESS is a distinct POINTS/TRAJECTORY directional product.  Do not
      // normalize it to the shell-only RIGIDITY_LIST mode: C19 relies on this token
      // to skip scalar/PENUMBRA cutoff searches and calculate A(R,Omega) directly.
      p.cutoff.searchAlgorithm = "DIRECT_ACCESS";
    }
    else if (alg=="BINARY" || alg=="ENDPOINT_BINARY" ||
            alg=="LEGACY_BINARY" || alg=="LEGACY") {
      p.cutoff.searchAlgorithm = "BINARY";
    }
    else {
      std::cerr << "Error: unknown cutoff-search algorithm '"
                << cli.cutoffSearchAlgorithm
                << "' for " << modeLabel
                << ". Valid values: UPPER_SCAN, PENUMBRA_SCAN, DIRECT_ACCESS, RIGIDITY_LIST, or BINARY.\n";
      return false;
    }
  }

  // RIGIDITY_LIST traces a compact explicit set instead of scanning the entire bracket.
  // A non-empty command-line list overrides the input deck, following the same precedence
  // rule as the other cutoff controls.  Latitude limits select launch nodes only; both
  // hemispheres and every configured shell longitude remain represented.
  if (!cli.cutoffRigidityListGV.empty()) {
    std::string error;
    const std::vector<double> values=ParseCutoffRigidityListCli_(
        cli.cutoffRigidityListGV,error);
    if (values.empty()) {
      std::cerr << "Error: invalid -cutoff-rigidity-list-gv for " << modeLabel
                << ": " << error << ".\n";
      return false;
    }
    p.cutoff.rigidityList_GV=values;
  }
  if (cli.cutoffDirectAccessAdaptive>=0)
    p.cutoff.directAccessAdaptive=(cli.cutoffDirectAccessAdaptive!=0);
  if (cli.cutoffDirectAccessAdaptiveMaxDepth>=0)
    p.cutoff.directAccessAdaptiveMaxDepth=cli.cutoffDirectAccessAdaptiveMaxDepth;
  if (cli.cutoffDirectAccessAdaptiveGuardDepth>=0)
    p.cutoff.directAccessAdaptiveGuardDepth=cli.cutoffDirectAccessAdaptiveGuardDepth;
  if (p.cutoff.directAccessAdaptiveGuardDepth>p.cutoff.directAccessAdaptiveMaxDepth) {
    std::cerr << "Error: DIRECT_ACCESS adaptive guard depth exceeds max depth for "
              << modeLabel << ".\n";
    return false;
  }
  if (p.cutoff.directAccessAdaptive && p.cutoff.searchAlgorithm!="DIRECT_ACCESS") {
    std::cerr << "Error: adaptive direct access was requested for " << modeLabel
              << " but cutoff search is " << p.cutoff.searchAlgorithm << ".\n";
    return false;
  }

  if (cli.cutoffAccessAbsLatMin_deg>=0.0)
    p.cutoff.accessAbsLatMin_deg=cli.cutoffAccessAbsLatMin_deg;
  if (cli.cutoffAccessAbsLatMax_deg>=0.0)
    p.cutoff.accessAbsLatMax_deg=cli.cutoffAccessAbsLatMax_deg;

  // Directional-map angular coverage.  The optimized path is deliberately generic:
  // it consumes arbitrary instrument look vectors and never assumes EAST/WEST,
  // antipodal heads, or any particular mission.  CLI definitions replace the
  // input-file aperture list only when an explicit aperture-file override is given;
  // repeatable direct CLI specifications are then appended.
  if (!cli.cutoffDirMapCoverage.empty()) {
    const std::string coverage=EarthUtil::ToUpper(cli.cutoffDirMapCoverage);
    if (coverage=="FULL_SPHERE" || coverage=="FULL" || coverage=="SPHERE")
      p.cutoff.dirMapCoverage="FULL_SPHERE";
    else if (coverage=="VECTOR_APERTURES" || coverage=="APERTURES" ||
             coverage=="INSTRUMENT_APERTURES" || coverage=="VECTOR" || coverage=="VECTORS")
      p.cutoff.dirMapCoverage="VECTOR_APERTURES";
    else {
      std::cerr << "Error: unknown directional-map coverage '"
                << cli.cutoffDirMapCoverage << "' for " << modeLabel
                << ". Valid values: FULL_SPHERE or VECTOR_APERTURES.\n";
      return false;
    }
  }

  try {
    // Resolve CLI precedence before opening any aperture file.  An explicit CLI file
    // replaces the input-deck file and its inline aperture definitions; this makes it
    // possible to use a generic committed AMPS_PARAM template while selecting the
    // epoch-specific instrument attitude file entirely from the command line.
    if (!cli.cutoffDirMapApertureFile.empty()) {
      p.cutoff.dirMapApertureFile=cli.cutoffDirMapApertureFile;
      p.cutoff.dirMapApertures.clear();
    }

    // Load the final selected file exactly once, after the CLI path override (if any)
    // has been applied.  Input-file inline DIRMAP_APERTURE records are retained when
    // no CLI file replacement was requested, and file records are appended to them.
    if (EarthUtil::ToUpper(p.cutoff.dirMapCoverage)=="VECTOR_APERTURES" &&
        !p.cutoff.dirMapApertureFile.empty()) {
      const auto fromFile=EarthUtil::LoadDirectionalApertureFile(
          p.cutoff.dirMapApertureFile);
      p.cutoff.dirMapApertures.insert(p.cutoff.dirMapApertures.end(),
                                      fromFile.begin(),fromFile.end());
    }

    // Repeatable CLI DIRMAP_APERTURE specifications are always appended last.  This is
    // useful for adding a calibration/diagnostic look direction without modifying the
    // mission attitude file.
    for (const std::string& spec : cli.cutoffDirMapApertureSpecs)
      p.cutoff.dirMapApertures.push_back(EarthUtil::ParseDirectionalApertureSpec(spec));
  }
  catch (const std::exception& e) {
    std::cerr << "Error: invalid directional aperture configuration for " << modeLabel
              << ": " << e.what() << "\n";
    return false;
  }

  {
    const std::string coverage=EarthUtil::ToUpper(p.cutoff.dirMapCoverage);
    if (!(coverage=="FULL_SPHERE" || coverage=="VECTOR_APERTURES")) {
      std::cerr << "Error: DIRMAP_COVERAGE must be FULL_SPHERE or VECTOR_APERTURES for "
                << modeLabel << ".\n";
      return false;
    }
    if (coverage=="VECTOR_APERTURES" && p.cutoff.dirMapApertures.empty()) {
      std::cerr << "Error: VECTOR_APERTURES requires at least one DIRMAP_APERTURE "
                << "or DIRMAP_APERTURE_FILE record for " << modeLabel << ".\n";
      return false;
    }
  }

  if (EarthUtil::ToUpper(p.cutoff.searchAlgorithm)=="RIGIDITY_LIST") {
    if (p.cutoff.rigidityList_GV.empty()) {
      std::cerr << "Error: RIGIDITY_LIST requires -cutoff-rigidity-list-gv or "
                << "CUTOFF_RIGIDITY_LIST_GV for " << modeLabel << ".\n";
      return false;
    }
    if (!(p.cutoff.accessAbsLatMin_deg>=0.0 &&
          p.cutoff.accessAbsLatMax_deg<=90.0 &&
          p.cutoff.accessAbsLatMax_deg>p.cutoff.accessAbsLatMin_deg)) {
      std::cerr << "Error: RIGIDITY_LIST requires 0 <= access latitude min < max <= 90 "
                << "degrees for " << modeLabel << ".\n";
      return false;
    }
  }

  // Number of coarse rigidity samples used by UPPER_SCAN or PENUMBRA_SCAN before
  // local transition refinement.  The historical keyword/CLI name is retained for
  // backward compatibility.  A positive CLI value overrides CUTOFF_UPPER_SCAN_N from
  // the input file for either scan algorithm.
  if (cli.cutoffUpperScanN > 0) {
    p.cutoff.upperScanN = cli.cutoffUpperScanN;
  }

  // Boolean cutoff trajectory integration policy.  Keep LEGACY as the global
  // default for backward compatibility, while allowing tests such as C14 to
  // request the corrected F3-quality integrator explicitly.
  if (!cli.cutoffTracePolicy.empty()) {
    const std::string policy=EarthUtil::ToUpper(cli.cutoffTracePolicy);
    if (policy=="LEGACY" || policy=="COMPATIBLE" ||
        policy=="LEGACY_CUTOFF" || policy=="C_SERIES") {
      p.cutoff.traceIntegrationPolicy="LEGACY";
    }
    else if (policy=="ACCURATE" || policy=="STRUCTURED" ||
             policy=="F3" || policy=="UPPER_BOUNDED") {
      p.cutoff.traceIntegrationPolicy="ACCURATE";
    }
    else {
      std::cerr << "Error: unknown cutoff trace policy '"
                << cli.cutoffTracePolicy << "' for " << modeLabel
                << ". Valid values: LEGACY or ACCURATE.\n";
      return false;
    }
  }

  // Trajectory time-step policy.  In adaptive mode DT_TRACE is a maximum step and
  // the pusher may reduce it using gyro/boundary criteria.  In fixed-step mode
  // DT_TRACE is used directly (except for trimming the last step to the remaining
  // trace-time cap).  This switch is useful for pusher/time-step convergence tests.
  if (cli.adaptiveDt >= 0) {
    p.numerics.adaptiveDt = (cli.adaptiveDt != 0);
  }

  // Optional cumulative path-length cap in Earth radii.  Keep this next to
  // ADAPTIVE_DT because both are numerical trajectory-control overrides shared by
  // Mode3D and gridless backward products.
  if (cli.maxTraceDistance_Re >= 0.0) {
    p.numerics.maxTraceDistance_Re = cli.maxTraceDistance_Re;
  }

  return true;
}

} // namespace

extern int nZenithElements;
extern int nAzimuthalElements;

void amps_init();
void amps_init_mesh();
void amps_time_step();

#define _NIGHTLY_TEST__CUTOFF_  0
#define _NIGHTLY_TEST__LEGACY_ 1

#ifndef _NIGHTLY_TEST_
#define _NIGHTLY_TEST_ _NIGHTLY_TEST__CUTOFF_
#endif


#undef _USER_DEFINED_INTERNAL_BOUNDARY_SPHERE_MODE_ 
#define _USER_DEFINED_INTERNAL_BOUNDARY_SPHERE_MODE_  _USER_DEFINED_INTERNAL_BOUNDARY_SPHERE_MODE_ON_ 


//output directional cutoff rigidity and min energy 
void PrintDirectionalRegidityCutoffTitle(FILE* fout) {
  fprintf(fout,"TITLE=\"%s\"","DirectionalRegidityCutoff");
}

void PrintDirectionalRegidityCutoffVariableList(FILE* fout) {
  if (Earth::ModelMode!=Earth::CutoffRigidityMode) return;

  for (int i=0;i<PIC::nTotalSpecies;i++) fprintf(fout,", \"Cutoff Rigidity[%i]\", \"Min Energy(spec=%i)[MeV]\"",i,i);
}

void PrintDirectionalRigidityCutoffDataStateVector(FILE* fout,long int nZenithPoint,long int nAzimuthalPoint,long int *SurfaceElementsInterpolationList,long int SurfaceElementsInterpolationListLength,cInternalSphericalData *Sphere,int spec,CMPI_channel* pipe,int ThisThread,int nTotalThreads) {
  int nInterpolationElement,nSurfaceElement,iZenith,iAzimuth;
  double InterpolationNormalization=0.0,InterpolationCoefficient;

  double CutoffRigidity=0.0;
  double InterpolatedInjectedParticleNumber=0.0,normInterpolatedInjectedParticleNumber=0.0;
  int InjectedParticleNumber;

  if (Earth::ModelMode!=Earth::CutoffRigidityMode) return;

  for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
    double t;

    CutoffRigidity=-1.0;

    for (nInterpolationElement=0;nInterpolationElement<SurfaceElementsInterpolationListLength;nInterpolationElement++) {
      nSurfaceElement=SurfaceElementsInterpolationList[nInterpolationElement];
      Sphere->GetSurfaceElementIndex(iZenith,iAzimuth,nSurfaceElement);

      t=Sphere->minRigidity[spec][nSurfaceElement];
 
      if (PIC::ThisThread!=0) {
        pipe->send(t);
      }
      else {
        if ((t>=0.0) && ((CutoffRigidity<0.0)||(t<CutoffRigidity)) ) CutoffRigidity=t;

        for (int thread=1;thread<PIC::nTotalThreads;thread++) {
          t=pipe->recv<double>(thread);

          if ((t>=0.0) && ((CutoffRigidity<0.0)||(t<CutoffRigidity)) ) CutoffRigidity=t;
        }
      }
    }

    if (PIC::ThisThread==0) {
      double momentum=-1.0,energy=-1.0; 

      if (CutoffRigidity>0.0) {
        momentum=CutoffRigidity*fabs(PIC::MolecularData::GetElectricCharge(spec))*1.0E9/SpeedOfLight;
        energy=Relativistic::Momentum2Energy(momentum,PIC::MolecularData::GetMass(spec))*J2MeV; 
      }

      fprintf(fout," %e  %e ",CutoffRigidity,energy);
    }
  }
}




void SampleIndividualLocations(int nMaxIterations) {
  int IterationCounter=0,localParticleGenerationFlag=0,globalParticleGenerationFlag;

  bool ShortTrajectoryFound=false;
  int ShortTrajectoryIndex,ShortTrajectoryIndexAll;


    PIC::SamplingMode=_TEMP_DISABLED_SAMPLING_MODE_;

  //estimate the total flux and rigidity in a set of the defined locations
  if (Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength!=0) {
    int LacalParticleNumber,GlobalParticleNumber;
    int nIngectedParticlePerIteration=Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations/Earth::CutoffRigidity::IndividualLocations::nParticleInjectionIterations;
    int nTotalInjectedParticles=0;

    if (nIngectedParticlePerIteration==0) nIngectedParticlePerIteration=Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations;

    if (PIC::ThisThread==0) {
      cout << "nTotalTestParticlesPerLocations=" << Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations << endl;
      cout << "nParticleInjectionIterations=" << Earth::CutoffRigidity::IndividualLocations::nParticleInjectionIterations << endl;
      cout << "nIngectedParticlePerIteration=" << nIngectedParticlePerIteration << endl;
    }

    PIC::Mover::BackwardTimeIntegrationMode=_PIC_MODE_ON_;
    Earth::CutoffRigidity::DomainBoundaryParticleProperty::EnableSampleParticleProperty=true;

    if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleDomainBoundaryParticleProperty==true) Earth::CutoffRigidity::DomainBoundaryParticleProperty::Allocate(std::max(1,Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength));

    if (Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable.IsAllocated()==false) {
      Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable.init(PIC::nTotalSpecies,std::max(1,Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength));
    }

    Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable=-1.0;

    //allocate spherical objects for sampling directional rigidity cutoff
    cInternalSphericalData::SetGeneralSurfaceMeshParameters(150,150);
    Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable=new cInternalSphericalData[Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength];

    for (int i=0;i<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;i++) {
      double x[3]={0.0,0.0,0.0};

      Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].SetSphereGeometricalParameters(x,1.0);
      Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].TotalSurfaceElementNumber=Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].GetTotalSurfaceElementsNumber();  

      Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].PrintDataStateVector=PrintDirectionalRigidityCutoffDataStateVector;
      Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].PrintVariableList=PrintDirectionalRegidityCutoffVariableList; 
      Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].PrintTitle=PrintDirectionalRegidityCutoffTitle;

      double *xp=Earth::CutoffRigidity::IndividualLocations::xTestLocationTable[i];

      sprintf(Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].TitleMessage,
        "Directional rigidity cutoff: x=%e,%e,%e[m] (%e,%e,%e)R_Earth",
         xp[0],xp[1],xp[2],xp[0]/_RADIUS_(_EARTH_),xp[1]/_RADIUS_(_EARTH_),xp[2]/_RADIUS_(_EARTH_)); 


      Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].minRigidity=new double* [PIC::nTotalSpecies];
      Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].minRigidity[0]=new double[PIC::nTotalSpecies*Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].GetTotalSurfaceElementsNumber()];

      for (int spec=1;spec<PIC::nTotalSpecies;spec++) {
        Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].minRigidity[spec]=Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].minRigidity[spec-1]+Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].GetTotalSurfaceElementsNumber();
      }

      for (int ii=0;ii<PIC::nTotalSpecies*Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].TotalSurfaceElementNumber;ii++) {
        Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[i].minRigidity[0][ii]=-1.0;
      }
    }

    int nInjectionRounds=0;
    int iInjectedParticle;
    
    double dRlog=log(Earth::CutoffRigidity::IndividualLocations::MaxInjectionRigidityLimit/Earth::CutoffRigidity::IndividualLocations::MinInjectionRigidityLimit)/
        Earth::CutoffRigidity::IndividualLocations::nRigiditySearchIntervals;
        
    do {
      //reset the partilce generation flag
      localParticleGenerationFlag=0;
      
      //redefine nTotalTestParticlesPerLocations in case of Earth::CutoffRigidity::IndividualLocations::InjectionMode==Earth::CutoffRigidity::IndividualLocations::_rigidity_mesh
      if (Earth::CutoffRigidity::IndividualLocations::InjectionMode==Earth::CutoffRigidity::IndividualLocations::_rigidity_grid_injection) {
        Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations=Earth::CutoffRigidity::IndividualLocations::nRigiditySearchIntervals*
            Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[0].GetTotalSurfaceElementsNumber();
      }

      //Inject new particles
      if (nTotalInjectedParticles<Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations) {
        nTotalInjectedParticles+=nIngectedParticlePerIteration;
        nInjectionRounds++;
        
        int nParticles2Inject=nIngectedParticlePerIteration;
       
        if (nTotalInjectedParticles>Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations) {
          nParticles2Inject=Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations-(nTotalInjectedParticles-nIngectedParticlePerIteration);
        }

        //inject the new portion of the particles
        for (int spec=0;spec<PIC::nTotalSpecies;spec++) for (int iLocation=0;iLocation<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iLocation++) {
          double x[3],v[3];
          int idim,iCell,jCell,kCell;
          cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode;

          for (idim=0;idim<3;idim++) x[idim]=Earth::CutoffRigidity::IndividualLocations::xTestLocationTable[iLocation][idim];
          startNode=PIC::Mesh::mesh->findTreeNode(x);

          if (startNode->Thread==PIC::ThisThread) {
            //generate a new particle velocity
            double mass,speed,energy,rigidity,momentum;

            static double logMinEnergyLimit=log(Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit);
            static double logMaxEnergyLimit=log(Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit);

            mass=PIC::MolecularData::GetMass(spec);

            if (PIC::Mesh::mesh->FindCellIndex(x,iCell,jCell,kCell,startNode,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

            for (int iNewParticle=0;iNewParticle<nParticles2Inject;iNewParticle++) {
              double momentum,rigidity;
              int iR,t;
              long int nZenithElement,nAzimuthalElement;
              
              iInjectedParticle=(nInjectionRounds-1)*nIngectedParticlePerIteration+iNewParticle;

              switch(Earth::CutoffRigidity::IndividualLocations::InjectionMode) {
              case Earth::CutoffRigidity::IndividualLocations::_rigidity_grid_injection:                
                //determine the velocity direction
                iR=iInjectedParticle/Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].GetTotalSurfaceElementsNumber();
                t=iInjectedParticle%Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].GetTotalSurfaceElementsNumber();
                
                nZenithElement=t/Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].nAzimuthalSurfaceElements;
                nAzimuthalElement=t%Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].nAzimuthalSurfaceElements;
                
                Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].GetSurfaceElementRandomDirection(v,nZenithElement,nAzimuthalElement);
                
                //determine the velocity vector
                rigidity=Earth::CutoffRigidity::IndividualLocations::MinInjectionRigidityLimit*exp(dRlog*(iR+rnd()));

                if (rigidity>Earth::CutoffRigidity::IndividualLocations::MaxInjectionRigidityLimit) {
                  exit(__LINE__,__FILE__,"Error: the rigidity value exeeds the limit");
                }
       
                momentum=rigidity*fabs(PIC::MolecularData::GetElectricCharge(spec))*1.0E9/SpeedOfLight;
                speed=Relativistic::Momentum2Speed(momentum,mass);
                
                Vector3D::MultiplyScalar(speed,v);
                break;
                
              case Earth::CutoffRigidity::IndividualLocations::_energy_injection:
                energy=exp(logMinEnergyLimit+rnd()*(logMaxEnergyLimit-logMinEnergyLimit));

                speed=Relativistic::E2Speed(energy,mass);
                Vector3D::Distribution::Uniform(v,speed);
                Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].GetSurfaceElementProjectionIndex(v,nZenithElement,nAzimuthalElement);
                break;
              case Earth::CutoffRigidity::IndividualLocations::_rigidity_injection:
                //def:          rigidity=(charge>0.0) ? momentum*SpeedOfLight/charge/1.0E9 : 0.0; //cutoff rigidity in SI -> GV (Moraal-2013-SSR)
                rigidity=Earth::CutoffRigidity::IndividualLocations::MinInjectionRigidityLimit+
                         rnd()*(Earth::CutoffRigidity::IndividualLocations::MaxInjectionRigidityLimit-Earth::CutoffRigidity::IndividualLocations::MinInjectionRigidityLimit);

                if (rigidity>Earth::CutoffRigidity::IndividualLocations::MaxInjectionRigidityLimit) {
                  exit(__LINE__,__FILE__,"Error: the rigidity value exeeds the limit");
                }

                
                momentum=rigidity*fabs(PIC::MolecularData::GetElectricCharge(spec))*1.0E9/SpeedOfLight;
                speed=Relativistic::Momentum2Speed(momentum,mass);
                Vector3D::Distribution::Uniform(v,speed);
                Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].GetSurfaceElementProjectionIndex(v,nZenithElement,nAzimuthalElement);
                break;
              }


              //at this point, 'v' point into the direction from where the particles came. Hence, the velocity of the particles sgould be -v
              Vector3D::MultiplyScalar(-1.0,v);
   

              //generate a new particle
              long int newParticle=PIC::ParticleBuffer::GetNewParticle(startNode->block->FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)]);
              PIC::ParticleBuffer::byte *newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

              PIC::ParticleBuffer::SetV(v,newParticleData);
              PIC::ParticleBuffer::SetX(x,newParticleData);
              PIC::ParticleBuffer::SetI(spec,newParticleData);

              //set the particle generation flag
              localParticleGenerationFlag=1;

              //apply condition of tracking the particle
              if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
                PIC::ParticleTracker::InitParticleID(newParticleData);
                PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,newParticleData,(void*)startNode);
              }

              *((int*)(newParticleData+Earth::CutoffRigidity::ParticleDataOffset::OriginLocationIndex))=iLocation;
              *((double*)(newParticleData+Earth::CutoffRigidity::ParticleDataOffset::OriginalSpeed))=sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);

              //set the initial value of the integrate path length
              if (Earth::CutoffRigidity::IntegratedPathLengthOffset!=-1) {
                *((double*)(newParticleData+Earth::CutoffRigidity::IntegratedPathLengthOffset))=0.0;
              }

              //set initial value for the integration time 
              if (Earth::CutoffRigidity::IntegratedTimeOffset!=-1) {
                *((double*)(newParticleData+Earth::CutoffRigidity::IntegratedTimeOffset))=0.0;
              }

              //set the velocity direction ID
              if (Earth::CutoffRigidity::ParticleDataOffset::OriginalVelocityDirectionID!=-1) {
                int id;
               // long int nZenithElement,nAzimuthalElement;

                //Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].GetSurfaceElementProjectionIndex(v,nZenithElement,nAzimuthalElement);
                id=Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].GetLocalSurfaceElementNumber(nZenithElement,nAzimuthalElement); 

                *((int*)(newParticleData+Earth::CutoffRigidity::ParticleDataOffset::OriginalVelocityDirectionID))=id;
              }
   

              //set up the particle rigidity
              if (Earth::CutoffRigidity::InitialRigidityOffset!=-1) {
                double momentum,charge,rigidity;

                charge=fabs(PIC::MolecularData::GetElectricCharge(spec));

                momentum=Relativistic::Speed2Momentum(speed,mass);
                rigidity=(charge>0.0) ? momentum*SpeedOfLight/charge/1.0E9 : 0.0; //cutoff rigidity in SI -> GV (Moraal-2013-SSR) 

                //rigidity=(charge>0.0) ? momentum/charge : 0.0;


                if (Earth::CutoffRigidity::IndividualLocations::InjectionMode==Earth::CutoffRigidity::IndividualLocations::_rigidity_grid_injection) {
                  if (rigidity>Earth::CutoffRigidity::IndividualLocations::MaxInjectionRigidityLimit) {
                    exit(__LINE__,__FILE__,"Error: the rigidity value exeeds the limit");
                  }
                }

                *((double*)(newParticleData+Earth::CutoffRigidity::InitialRigidityOffset))=rigidity;
              }

              //save the original location of the particle
              if (Earth::CutoffRigidity::InitialLocationOffset!=-1) {
                memcpy(newParticleData+Earth::CutoffRigidity::InitialLocationOffset,x,3*sizeof(double));
              }


            }
          }
        }
      }


      //preform the next iteration
      amps_time_step();

    //search for particles that left geospace
    if (Earth::GeospaceFlag::offset!=-1) {
      //loop through all cells
      for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];
        PIC::Mesh::cDataBlockAMR *block;
        PIC::Mesh::cDataCenterNode *CenterNode;
        long int ParticleList,ptr;
        int i,j,k,nd;
        char *offset;

        block=node->block;
        if ((block=node->block)==NULL) continue;

        for (k=0;k<_BLOCK_CELLS_Z_;k++) {
          for (j=0;j<_BLOCK_CELLS_Y_;j++) {
            for (i=0;i<_BLOCK_CELLS_X_;i++) {
              nd=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);

              if ((CenterNode=node->block->GetCenterNode(nd))==NULL) continue;
              offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::CenterNodeAssociatedDataOffsetBegin+PIC::CPLR::DATAFILE::MULTIFILE::CurrDataFileOffset;

              if (*((double*)(offset+Earth::GeospaceFlag::offset))==0.0) {
                //the cell is outside of the geospace: search for particles and removed those found

                ParticleList=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

                while (ParticleList!=-1) {
                  ptr=ParticleList;
                  ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);

                  //account for the particle in calculation of the rigidity
                  double x[3],v[3];

                  PIC::ParticleBuffer::GetV(v,ptr);
                  PIC::ParticleBuffer::GetX(x,ptr);

                  Earth::CutoffRigidity::ProcessOutsideDomainParticles(ptr,x,v,-1,node);
                  PIC::ParticleBuffer::DeleteParticle(ptr);
                }

                block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=-1;
              }
            }
          }
        }
      }
    }

        //search for particles that spend too much time in the simulation
        ShortTrajectoryFound=false;

      for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];
        PIC::Mesh::cDataBlockAMR *block;
        PIC::Mesh::cDataCenterNode *CenterNode;
        long int ParticleList,ptr;
        int i,j,k,nd;
        char *offset;
        double x[3];

        block=node->block;
        if ((block=node->block)==NULL) continue;


        for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
          for (int j=0;j<_BLOCK_CELLS_Y_;j++) {
            for (int i=0;i<_BLOCK_CELLS_X_;i++) {
              long int ParticleList=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

              while (ParticleList!=-1) {
                ptr=ParticleList;
                ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);

                double t,l,v[3];
                PIC::ParticleBuffer::byte *ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

                t=*((double*)(ParticleData+Earth::CutoffRigidity::IntegratedTimeOffset));
                PIC::ParticleBuffer::GetX(x,ptr);

                if (t*t*Vector3D::DotProduct(v,v)>Earth::CutoffRigidity::MaxIntegrationLength*Earth::CutoffRigidity::MaxIntegrationLength) {
                  PIC::ParticleBuffer::DeleteParticle(ptr,block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
                }
                else {
                 *((double*)(ParticleData+Earth::CutoffRigidity::IntegratedTimeOffset))+=PIC::ParticleWeightTimeStep::GlobalTimeStep[PIC::ParticleBuffer::GetI(ptr)];
                 if (Earth::CutoffRigidity::SearchShortTrajectory==true) ShortTrajectoryFound=true;
              }
            }
         }
       }
     }
   }

      static int LastDataOutputFileNumber=-1;

      if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
        PIC::RequiredSampleLength*=2;
        if (PIC::RequiredSampleLength>50000) PIC::RequiredSampleLength=50000;


        LastDataOutputFileNumber=PIC::DataOutputFileNumber;
        if (PIC::Mesh::mesh->ThisThread==0) cout << "The new sample length is " << PIC::RequiredSampleLength << endl;
      }


      //get the total number of particles in the system
      LacalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
      MPI_Allreduce(&LacalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

      //determine whether any particle has been generated during the current iteration
      MPI_Allreduce(&localParticleGenerationFlag,&globalParticleGenerationFlag,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

      if (globalParticleGenerationFlag!=0) {
        //at least one particle has been generated -> reset the iteration counter
        IterationCounter=0;
      }
      else {
        //increment the iteration counter
        IterationCounter++;
      }


    ShortTrajectoryIndex=(ShortTrajectoryFound==true) ? 1 : 0;
    MPI_Allreduce(&ShortTrajectoryIndex,&ShortTrajectoryIndexAll,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

    if (PIC::ThisThread==0) cout << "IterationCounter=" << IterationCounter << ", nMaxIterations=" << nMaxIterations << ", GlobalParticleNumber=" << GlobalParticleNumber << ", ShortTrajectoryIndexAll=" << ShortTrajectoryIndexAll << endl;


    }
    while (((GlobalParticleNumber!=0)&&(IterationCounter<nMaxIterations)) || (ShortTrajectoryIndexAll>0));

    //delete all particles that still present in the system
    PIC::ParticleBuffer::DeleteAllParticles();

    //print out the results 
    //output directional regidity cutoff
    for (int iLocation=0;iLocation<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iLocation++) {
      char fname[200];

      sprintf(fname,"%s/directional-rigidity-cutoff--point-%i.dat",PIC::OutputDataFileDirectory,iLocation);
      Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].PrintSurfaceData(fname,0,true);
    }

    int nel=Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable.GetElementNumber();
    double *buff=new double [PIC::nTotalThreads*nel];
    
    MPI_Gather(Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable.GetBufferPointer(),nel,MPI_DOUBLE,buff,nel,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR);

if (PIC::nTotalSpecies!=1) exit(__LINE__,__FILE__,"Error: heen to be generalized from more then 1 species");

    for (int el=0;el<nel;el++) {
      for (int thread=1;thread<PIC::nTotalThreads;thread++) {
        if ((buff[el]<=0.0)||(buff[el]>buff[el+thread*nel])) buff[el]=buff[el+thread*nel];
      }
    }

    if (PIC::ThisThread==0) {
      memcpy(Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable.GetBufferPointer(),buff,nel*sizeof(double));
    }

    delete [] buff;

    MPI_Bcast(Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable.GetBufferPointer(),nel,MPI_DOUBLE,0,MPI_GLOBAL_COMMUNICATOR); 
  
    if (PIC::ThisThread==0) {
      ofstream fout("res.dat"); 

      cout << "iloc\tspec\tcutoff rigidity\n";
      fout << "iloc\tspec\tcutoff rigidity\n";

      for (int iloc=0;iloc<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iloc++) {
        for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
          cout << iloc << "\t" << spec << "\t" << Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable(spec,iloc) << endl; 
          fout << iloc << "\t" << spec << "\t" << Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable(spec,iloc) << endl;
        }
      }

      if (Earth::CutoffRigidity::IndividualLocations::InjectionMode==Earth::CutoffRigidity::IndividualLocations::_energy_injection) {
        cout << "iloc\tspec\tflux\n";
        fout << "iloc\tspec\tflux\n";

        for (int iloc=0;iloc<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iloc++) {
          for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
           cout << iloc << "\t" << spec << "\t" <<
             Earth::CutoffRigidity::IndividualLocations::SampledFluxTable(spec,iloc)*4.0*Pi/Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations << endl; 

           fout << iloc << "\t" << spec << "\t" <<
             Earth::CutoffRigidity::IndividualLocations::SampledFluxTable(spec,iloc)*4.0*Pi/Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations << endl;
          }
        }
      }

      fout.close();
    }


    //determine the flux and eneregy spectra of the energetic particles in the poins of the observation
    if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleDomainBoundaryParticleProperty==true) {
      double v[3],KineticEnergy,Speed,DiffFlux,dSurface,norm;
      int offset,iTestsLocation,spec,i,j,iface,iTable,jTable,Index,iBit,iByte,iE;

      const int nTotalEnergySpectrumIntervals=25;
      const double logMinEnergyLimit=log10(Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit);
      const double logMaxEnergyLimit=log10(Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit);
      const double dE=(logMaxEnergyLimit-logMinEnergyLimit)/nTotalEnergySpectrumIntervals;

      //allocate the data buffers
      //TotalFlux[iLocation][spec]
      //EnergySpectrum[iLocation][spec][iEnergyInterval]

      array_3d<double> EnergySpectrum(Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength,PIC::nTotalSpecies,nTotalEnergySpectrumIntervals);
      array_2d<double> TotalFlux(Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength,PIC::nTotalSpecies);

      EnergySpectrum=0.0;
      TotalFlux=0.0;

      //calculate the flux and energey spectrum
      for (iTestsLocation=0;iTestsLocation<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iTestsLocation++) {
        for (spec=0;spec<PIC::nTotalSpecies;spec++) {
          for (iface=0;iface<6;iface++) {
            //surface area of the element of the surface mesh that covers the boundary of the computational domain
            double lx,ly,lz;

            switch (iface) {
            case 0:case 1:
              ly=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;
              lz=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;

              dSurface=ly*lz;
              break;
            case 2:case 3:
              lx=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;
              lz=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;

              dSurface=lx*lz;
              break;
            case 4:case 5:
              lx=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;
              ly=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;

              dSurface=ly*lz;
            }

            //loop through the mesh that covers face 'iface' on the computational domain
            for (iTable=0;iTable<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;iTable++) {
              for (jTable=0;jTable<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;jTable++) {
                Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable(spec,iTestsLocation,iface,iTable,jTable).Gather();

                for (iByte=0;iByte<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable(spec,iTestsLocation,iface,iTable,jTable).FlagTableLength[0];iByte++) for (iBit=0;iBit<8;iBit++) {
                  Index=iBit+8*iByte;

                  if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable(spec,iTestsLocation,iface,iTable,jTable).Test(Index)==true) {
                    //at least one particle that corrsponds to 'Index' has been detected. Add a contribution of such particles to the total energy spectrum and flux as observed at the point of the observation 'iTestsLocation'

                    Earth::CutoffRigidity::DomainBoundaryParticleProperty::ConvertVelocityVectorIndex2Velocity(spec,v,iface,Index);
                    Speed=Vector3D::Length(v);
                    KineticEnergy=Relativistic::Speed2E(Speed,PIC::MolecularData::GetMass(spec));

                    //probability density that particles has velocity 'v'
                    DiffFlux=GCR_BADAVI2011ASR::Hydrogen::GetDiffFlux(KineticEnergy);

                    //determine the contributio of the particles into the 'observed' flux and energy spectrum
                    TotalFlux(iTestsLocation,spec)+=DiffFlux*dSurface;

                    //determine contribution of the particles to the energy flux
                    iE=(log10(KineticEnergy)-logMinEnergyLimit)/dE;
                    if (iE<0) iE=0;
                    if (iE>=nTotalEnergySpectrumIntervals) iE=nTotalEnergySpectrumIntervals-1;

                    EnergySpectrum(iTestsLocation,spec,iE)+=DiffFlux*dSurface;
                  }
                }
              }
            }
          }

          //normalize the energy spectrum
          for (iE=0,norm=0.0;iE<nTotalEnergySpectrumIntervals;iE++) norm+=EnergySpectrum(iTestsLocation,spec,iE)*dE;
          if (norm>0) for (iE=0;iE<nTotalEnergySpectrumIntervals;iE++) EnergySpectrum(iTestsLocation,spec,iE)/=norm;
        }
      }


      //output sampled particles flux and energy spectrum
      if (PIC::ThisThread==0) {
        //sampled energy spectrum
        FILE *fout;
        int spec,iTestsLocation,iE;
        char fname[400];

        for (spec=0;spec<PIC::nTotalSpecies;spec++) {
          sprintf(fname,"%s/EnergySpectrum[s=%i].dat",PIC::OutputDataFileDirectory,spec);
          fout=fopen(fname,"w");

          fprintf(fout,"VARIABLES=\"log10(Kinetic Energy[MeV]\"");
          for (iTestsLocation=0;iTestsLocation<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iTestsLocation++) fprintf(fout,", \"Spectrum (iTestsLocation=%i)\"",iTestsLocation);
          fprintf(fout,"\n");

          for (iE=0;iE<nTotalEnergySpectrumIntervals;iE++) {
            double log10e=logMinEnergyLimit+iE*dE;
            double e=pow(10,log10e);

            e*=J2MeV;
            fprintf(fout,"%e  ",e);

            for (iTestsLocation=0;iTestsLocation<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iTestsLocation++) fprintf(fout,"%e  ",EnergySpectrum(iTestsLocation,spec,iE));
            fprintf(fout,"\n");
          }

          fclose(fout);
        }

        //The total energetic particle flux
        for (spec=0;spec<PIC::nTotalSpecies;spec++) for (iTestsLocation=0;iTestsLocation<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iTestsLocation++) {
          printf("spec=%i, iTestsLocation=%i: Flux=%e\n",spec,iTestsLocation,TotalFlux(iTestsLocation,spec));
        }
      }
    }
    else {
      //non-uniform distribution of the injected particles
      if (PIC::ThisThread==0) printf("%i, %s: Error: not implemented\n",__LINE__,__FILE__); //exit(__LINE__,__FILE__,"Error: not implemented");
      return; 
    }

  }


/*
  //output directional regidity cutoff
  for (int iLocation=0;iLocation<Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength;iLocation++) {
    char fname[200];

    sprintf(fname,"directional-rigidity-cutoff--point-%i.dat",iLocation); 
    Earth::CutoffRigidity::IndividualLocations::SamplingSphereTable[iLocation].PrintSurfaceData(fname,0,true);
  }
*/

  //release sampling buffers
 // Earth::CutoffRigidity::DomainBoundaryParticleProperty::Deallocate();
}



void SampleSphericalMaplLocations(double Radius,int nMaxIterations) {
  int IterationCounter=0,localParticleGenerationFlag=0,globalParticleGenerationFlag;
  int iLocation;
  double x[3]={0.0,0.0,0.0},v[3];

  PIC::Mover::BackwardTimeIntegrationMode=_PIC_MODE_ON_;


  int nTotalInjectedParticlePerPoint=25;

//  Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations=2000;

  cInternalSphericalData Sphere;
  Sphere.SetGeneralSurfaceMeshParameters(nZenithElements,nAzimuthalElements);
  Sphere.SetSphereGeometricalParameters(x,Radius);

  //  Earth::CutoffRigidity::DomainBoundaryParticleProperty::Allocate(nZenithElements*nAzimuthalElements);

  double Speed,DiffFlux,dSurface,norm,KineticEnergy;
  int offset,iTestsLocation,spec,i,j,iface,iTable,jTable,Index,iBit,iByte,iE;


  const int nTotalEnergySpectrumIntervals=50;
  const double logMinEnergyLimit=log10(Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit);
  const double logMaxEnergyLimit=log10(Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit);
  const double dE=(logMaxEnergyLimit-logMinEnergyLimit)/nTotalEnergySpectrumIntervals;


  array_2d<double> TotalFlux(nZenithElements*nAzimuthalElements,PIC::nTotalSpecies);
  array_3d<double> EnergySpectrum(nZenithElements*nAzimuthalElements,PIC::nTotalSpecies,nTotalEnergySpectrumIntervals);

  //set the values of the buffers to zero
  TotalFlux=0.0;
  EnergySpectrum=0.0;


  //estimate the total flux and rigidity in a set of the defined locations
  int LacalParticleNumber,GlobalParticleNumber=0;
  int nIngectedParticlePerIteration=Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations/Earth::CutoffRigidity::IndividualLocations::nParticleInjectionIterations;
  int nTotalInjectedParticles=0;
  bool ShortTrajectoryFound=false;
  int ShortTrajectoryIndex,ShortTrajectoryIndexAll;

  if (nIngectedParticlePerIteration==0) nIngectedParticlePerIteration=Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations;

  PIC::Mover::BackwardTimeIntegrationMode=_PIC_MODE_ON_;
  Earth::CutoffRigidity::DomainBoundaryParticleProperty::EnableSampleParticleProperty=true;

  //allocate the sampling buffer
  if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleDomainBoundaryParticleProperty==true) {
    Earth::CutoffRigidity::DomainBoundaryParticleProperty::Allocate(nAzimuthalElements*nAzimuthalElements);
  }

  if (Earth::CutoffRigidity::CutoffRigidityTable.IsAllocated()==true) {
    Earth::CutoffRigidity::CutoffRigidityTable.Deallocate(); 
    Earth::CutoffRigidity::InjectedParticleMap.Deallocate();
    Earth::CutoffRigidity::MaxEnergyInjectedParticles.Deallocate();
  }

  Earth::CutoffRigidity::MaxEnergyInjectedParticles.init(PIC::nTotalSpecies,nZenithElements*nAzimuthalElements);
  Earth::CutoffRigidity::MaxEnergyInjectedParticles=0.0;

  Earth::CutoffRigidity::CutoffRigidityTable.init(PIC::nTotalSpecies,nZenithElements*nAzimuthalElements);
  Earth::CutoffRigidity::CutoffRigidityTable=-1.0;

  Earth::CutoffRigidity::InjectedParticleMap.init(PIC::nTotalSpecies,nZenithElements*nAzimuthalElements);
  Earth::CutoffRigidity::InjectedParticleMap=0;

  if (PIC::ThisThread==0) {
    cout << "nIngectedParticlePerIteration=" << nIngectedParticlePerIteration << endl;
    cout << "nTotalTestParticlesPerLocations=" << Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations << endl;
    cout << "nParticleInjectionIterations=" << Earth::CutoffRigidity::IndividualLocations::nParticleInjectionIterations << endl;
  }

  //deallocate the individual location sampling buffer
  Earth::CutoffRigidity::IndividualLocations::CutoffRigidityTable.Deallocate();


  do { //while there are particles in the system
    localParticleGenerationFlag=0;


    int sumTotalInjectedParticles;
    MPI_Allreduce(&nTotalInjectedParticles,&sumTotalInjectedParticles,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

    if (sumTotalInjectedParticles<Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations*nZenithElements*nAzimuthalElements*PIC::nTotalSpecies) {
      //inject new particles in case it is needed
      localParticleGenerationFlag=1;

      //inject the new portion of the particles
      for (int spec=0;spec<PIC::nTotalSpecies;spec++) for (int iZenithElement=0;iZenithElement<nZenithElements;iZenithElement++) for (int iAzimutalElement=0;iAzimutalElement<nAzimuthalElements;iAzimutalElement++) {
        int idim,iCell,jCell,kCell;
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode;

        iLocation=Sphere.GetLocalSurfaceElementNumber(iZenithElement,iAzimutalElement);

        //generate a new particle velocity
        double mass,speed,energy,rigidity,momentum;
        int nd;

        static double logMinEnergyLimit=log(Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit);
        static double logMaxEnergyLimit=log(Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit);

        mass=PIC::MolecularData::GetMass(spec);

        for (int iNewParticle=0;iNewParticle<nIngectedParticlePerIteration;iNewParticle++) {
          Sphere.GetSurfaceElementRandomPoint(x,iZenithElement,iAzimutalElement);
          startNode=PIC::Mesh::mesh->findTreeNode(x);

          if (startNode->Thread!=PIC::ThisThread) continue;

          if ((nd=PIC::Mesh::mesh->FindCellIndex(x,iCell,jCell,kCell,startNode,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

#if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
          PIC::Mesh::cDataCenterNode *cell;

          cell=startNode->block->GetCenterNode(nd);

          if (cell==NULL) exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
          if (cell->Measure<=0.0) exit(__LINE__,__FILE__,"Error: the cell measure is not initialized");
#endif

          //generate energy of the new particle
//          energy=exp(logMinEnergyLimit+rnd()*(logMaxEnergyLimit-logMinEnergyLimit));

 //         speed=Relativistic::E2Speed(energy,mass);
 //         Vector3D::Distribution::Uniform(v,speed);


              double momentum,rigidity;

              switch(Earth::CutoffRigidity::IndividualLocations::InjectionMode) {
              case Earth::CutoffRigidity::IndividualLocations::_energy_injection:
                energy=exp(logMinEnergyLimit+rnd()*(logMaxEnergyLimit-logMinEnergyLimit));

                speed=Relativistic::E2Speed(energy,mass);
                Vector3D::Distribution::Uniform(v,speed);
                break;
              case Earth::CutoffRigidity::IndividualLocations::_rigidity_injection:
                rigidity=Earth::CutoffRigidity::IndividualLocations::MinInjectionRigidityLimit+
                         rnd()*(Earth::CutoffRigidity::IndividualLocations::MaxInjectionRigidityLimit-Earth::CutoffRigidity::IndividualLocations::MinInjectionRigidityLimit);


  //def:          rigidity=(charge>0.0) ? momentum*SpeedOfLight/charge/1.0E9 : 0.0; //cutoff rigidity in SI -> GV (Moraal-2013-SSR)  


                momentum=rigidity*fabs(PIC::MolecularData::GetElectricCharge(spec))*1.0E9/SpeedOfLight;
                speed=Relativistic::Momentum2Speed(momentum,mass);
                Vector3D::Distribution::Uniform(v,speed);
                break;
              }


          //generate a new particle
          long int newParticle=PIC::ParticleBuffer::GetNewParticle(startNode->block->FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)]);
          PIC::ParticleBuffer::byte *newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

          nTotalInjectedParticles++;
          Earth::CutoffRigidity::InjectedParticleMap(spec,iLocation)=1+Earth::CutoffRigidity::InjectedParticleMap(spec,iLocation); 

          if (x[0]*v[0]+x[1]*v[1]+x[2]*v[2]<0.0) v[0]=-v[0],v[1]=-v[1],v[2]=-v[2]; 

          PIC::ParticleBuffer::SetV(v,newParticleData);
          PIC::ParticleBuffer::SetX(x,newParticleData);
          PIC::ParticleBuffer::SetI(spec,newParticleData);

          if (energy>Earth::CutoffRigidity::MaxEnergyInjectedParticles(spec,iLocation)) Earth::CutoffRigidity::MaxEnergyInjectedParticles(spec,iLocation)=energy*J2MeV;

          //set the particle generation flag
          localParticleGenerationFlag=1;

          //apply condition of tracking the particle
          if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
            PIC::ParticleTracker::InitParticleID(newParticleData);
            PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,newParticleData,(void*)startNode);
          }

          *((int*)(newParticleData+Earth::CutoffRigidity::ParticleDataOffset::OriginLocationIndex))=iLocation;
          *((double*)(newParticleData+Earth::CutoffRigidity::ParticleDataOffset::OriginalSpeed))=sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);

          //set the initial value of the integrate path length
          if (Earth::CutoffRigidity::IntegratedPathLengthOffset!=-1) {
            *((double*)(newParticleData+Earth::CutoffRigidity::IntegratedPathLengthOffset))=0.0;
          }

          //set initial value for the integration time
          if (Earth::CutoffRigidity::IntegratedTimeOffset!=-1) {
            *((double*)(newParticleData+Earth::CutoffRigidity::IntegratedTimeOffset))=0.0;
          }


          //set up the particle rigidity
          if (Earth::CutoffRigidity::InitialRigidityOffset!=-1) {
            double momentum,charge,rigidity;

            charge=fabs(PIC::MolecularData::GetElectricCharge(spec));

            momentum=Relativistic::Speed2Momentum(speed,mass);
            rigidity=(charge>0.0) ? momentum*SpeedOfLight/charge/1.0E9 : 0.0; //cutoff rigidity in SI -> GV (Moraal-2013-SSR)  

            *((double*)(newParticleData+Earth::CutoffRigidity::InitialRigidityOffset))=rigidity;
          }

          //save the original location of the particle
          if (Earth::CutoffRigidity::InitialLocationOffset!=-1) {
            memcpy(newParticleData+Earth::CutoffRigidity::InitialLocationOffset,x,3*sizeof(double));
          }

        }  //all particle are allocated for the currect iteration

      } //loop throught he sphere
    } // generate new particles if needed


    //preform the next iteration
    amps_time_step();

    //search for particles that left geospace 
    if (Earth::GeospaceFlag::offset!=-1) {
      //loop through all cells  
      for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode]; 
        PIC::Mesh::cDataBlockAMR *block;
        PIC::Mesh::cDataCenterNode *CenterNode;
        long int ParticleList,ptr;
        int i,j,k,nd;
        char *offset;

        block=node->block;
        if ((block=node->block)==NULL) continue;

        for (k=0;k<_BLOCK_CELLS_Z_;k++) {
          for (j=0;j<_BLOCK_CELLS_Y_;j++) {
            for (i=0;i<_BLOCK_CELLS_X_;i++) {
              nd=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);

              if ((CenterNode=node->block->GetCenterNode(nd))==NULL) continue;
              offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::CenterNodeAssociatedDataOffsetBegin+PIC::CPLR::DATAFILE::MULTIFILE::CurrDataFileOffset;

              if (*((double*)(offset+Earth::GeospaceFlag::offset))==0.0) {
                //the cell is outside of the geospace: search for particles and removed those found 
                 
                ParticleList=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

                while (ParticleList!=-1) { 
                  ptr=ParticleList;
                  ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);

                  //account for the particle in calculation of the rigidity
                  double x[3],v[3]; 

                  PIC::ParticleBuffer::GetV(v,ptr);
                  PIC::ParticleBuffer::GetX(x,ptr);

                  Earth::CutoffRigidity::ProcessOutsideDomainParticles(ptr,x,v,-1,node);
                  PIC::ParticleBuffer::DeleteParticle(ptr);
                }

                block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=-1; 
              }
            }
          }
        }

        //search for particles that spend too much time in the simulation 
        ShortTrajectoryFound=false;

        for (k=0;k<_BLOCK_CELLS_Z_;k++) {
          for (j=0;j<_BLOCK_CELLS_Y_;j++) {
            for (i=0;i<_BLOCK_CELLS_X_;i++) {
              ParticleList=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

              while (ParticleList!=-1) {
                ptr=ParticleList;
                ParticleList=PIC::ParticleBuffer::GetNext(ParticleList);

                double t,l,v[3];
                PIC::ParticleBuffer::byte *ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

                t=*((double*)(ParticleData+Earth::CutoffRigidity::IntegratedTimeOffset));
                PIC::ParticleBuffer::GetX(x,ptr);

                if (t*t*Vector3D::DotProduct(v,v)>Earth::CutoffRigidity::MaxIntegrationLength*Earth::CutoffRigidity::MaxIntegrationLength) {
                  PIC::ParticleBuffer::DeleteParticle(ptr,block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]); 
                } 
                else {
                 *((double*)(ParticleData+Earth::CutoffRigidity::IntegratedTimeOffset))+=PIC::ParticleWeightTimeStep::GlobalTimeStep[PIC::ParticleBuffer::GetI(ptr)];
                 if (Earth::CutoffRigidity::SearchShortTrajectory==true) ShortTrajectoryFound=true;
              }
            }
         }
       }
     } 
   }
}

    static int LastDataOutputFileNumber=-1;

    if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
      PIC::RequiredSampleLength*=2;
      if (PIC::RequiredSampleLength>50000) PIC::RequiredSampleLength=50000;


      LastDataOutputFileNumber=PIC::DataOutputFileNumber;
      if (PIC::Mesh::mesh->ThisThread==0) cout << "The new sample length is " << PIC::RequiredSampleLength << endl;
    }


    //get the total number of particles in the system
    LacalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
    MPI_Allreduce(&LacalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

    //determine whether any particle has been generated during the current iteration
    MPI_Allreduce(&localParticleGenerationFlag,&globalParticleGenerationFlag,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

    if (globalParticleGenerationFlag!=0) {
      //at least one particle has been generated -> reset the iteration counter
      IterationCounter=0;
    }
    else {
      //increment the iteration counter
      IterationCounter++;
    }

    ShortTrajectoryIndex=(ShortTrajectoryFound==true) ? 1 : 0;
    MPI_Allreduce(&ShortTrajectoryIndex,&ShortTrajectoryIndexAll,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

    if (PIC::ThisThread==0) cout << "IterationCounter=" << IterationCounter << ", nMaxIterations=" << nMaxIterations << ", GlobalParticleNumber=" << GlobalParticleNumber << ", ShortTrajectoryIndexAll=" << ShortTrajectoryIndexAll << endl;

  }
  while (((GlobalParticleNumber!=0)&&(IterationCounter<nMaxIterations))||(ShortTrajectoryIndexAll>0));


  //calculate the flux and energey spectrum
  /*
#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel for schedule(dynamic,1) default (none)  \
    private (iSphereIndex,spec,iface,dSurface,iTable,jTable,iByte,iBit,Index,Speed,KineticEnergy,DiffFlux,iE,norm,v) \
    shared (iSphereIndexMin,iSphereIndexMax,Earth::CutoffRigidity::DomainBoundaryParticleProperty::dX,Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection, \
        Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable,TotalFlux,EnergySpectrum)
#endif
   */



  TotalFlux=0.0;
  EnergySpectrum=0.0;


  if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleDomainBoundaryParticleProperty==true) for (int iZenithElement=0;iZenithElement<nZenithElements;iZenithElement++) for (int iAzimutalElement=0;iAzimutalElement<nAzimuthalElements;iAzimutalElement++) {
    iLocation=Sphere.GetLocalSurfaceElementNumber(iZenithElement,iAzimutalElement);

    for (spec=0;spec<PIC::nTotalSpecies;spec++) {
      for (iface=0;iface<6;iface++) {
        //surface area of the element of the surface mesh that covers the boundary of the computational domain
        double lx,ly,lz;

        switch (iface) {
        case 0:case 1:
          ly=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;
          lz=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;

          dSurface=ly*lz;
          break;
        case 2:case 3:
          lx=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;
          lz=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;

          dSurface=lx*lz;
          break;
        case 4:case 5:
          lx=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;
          ly=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;

          dSurface=ly*lz;
        }


        //loop through the mesh that covers face 'iface' on the computational domain
        for (iTable=0;iTable<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;iTable++) {
          for (jTable=0;jTable<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection;jTable++) {
            Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable(spec,iLocation,iface,iTable,jTable).Gather();

            for (iByte=0;iByte<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable(spec,iLocation,iface,iTable,jTable).FlagTableLength[0];iByte++) for (iBit=0;iBit<8;iBit++) {
              Index=iBit+8*iByte;

              if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable(spec,iLocation,iface,iTable,jTable).Test(Index)==true) {
                //at least one particle that corrsponds to 'Index' has been detected. Add a contribution of such particles to the total energy spectrum and flux as observed at the point of the observation 'iTestsLocation'

                Earth::CutoffRigidity::DomainBoundaryParticleProperty::ConvertVelocityVectorIndex2Velocity(spec,v,iface,Index);
                Speed=Vector3D::Length(v);
                KineticEnergy=Relativistic::Speed2E(Speed,PIC::MolecularData::GetMass(spec));

                //probability density that particles has velocity 'v'
                DiffFlux=GCR_BADAVI2011ASR::Hydrogen::GetDiffFlux(KineticEnergy);

                //determine the contributio of the particles into the 'observed' flux and energy spectrum
                TotalFlux(iLocation,spec)+=DiffFlux*dSurface;

                //determine contribution of the particles to the energy flux
                iE=(log10(KineticEnergy)-logMinEnergyLimit)/dE;
                if (iE<0) iE=0;
                if (iE>=nTotalEnergySpectrumIntervals) iE=nTotalEnergySpectrumIntervals-1;

                EnergySpectrum(iLocation,spec,iE)+=DiffFlux*dSurface;
              }
            }
          }
        }
      }


    }
  }


  //output the calculated map
  CMPI_channel pipe(1000000);
  FILE *fout2d_total_flux,*fout2d_rigidity,**fout2d_spectrum;

  if (PIC::ThisThread==0) {
    //sampled energy spectrum
    int spec,iTestsLocation,iE;
    char fname[400];

    pipe.openRecvAll();

    fout2d_spectrum=new FILE* [PIC::nTotalSpecies];

    //open the output file
    sprintf(fname,"%s/CutoffRigidityMap[R=%e].dat",PIC::OutputDataFileDirectory,Radius);
    fout2d_rigidity=fopen(fname,"w");
    fprintf(fout2d_rigidity,"VARIABLES=\"Lon\", \"Lat\"");

    sprintf(fname,"%s/TotalFluxMap[R=%e].dat",PIC::OutputDataFileDirectory,Radius);
    fout2d_total_flux=fopen(fname,"w");
    fprintf(fout2d_total_flux,"VARIABLES=\"Lon\", \"Lat\"");

    for (spec=0;spec<PIC::nTotalSpecies;spec++) {
      fprintf(fout2d_rigidity,",  \"Cutoff Rigidity [GV] (s=%i)\", \"Injected Particle Number (s=%i)\", \"Max energy injected particles (s=%i)\"",spec,spec,spec);
      fprintf(fout2d_total_flux,",  \"Total Flux [1/(s*m^2)] (s=%i)\"",spec);
    }

    fprintf(fout2d_rigidity,"\nZONE I=%i, J=%i, DATAPACKING=POINT\n",nAzimuthalElements+1,nZenithElements+1);
    fprintf(fout2d_total_flux,"\nZONE I=%i, J=%i, DATAPACKING=POINT\n",nAzimuthalElements+1,nZenithElements+1);

    //files to output the energy spectrum
    for (spec=0;spec<PIC::nTotalSpecies;spec++) {
      sprintf(fname,"%s/EnergySpectrumMap[R=%e].s=%i.dat",PIC::OutputDataFileDirectory,Radius,spec);
      fout2d_spectrum[spec]=fopen(fname,"w");
      fprintf(fout2d_spectrum[spec],"VARIABLES=\"Lon\", \"Lat\"");

      for (iE=0;iE<nTotalEnergySpectrumIntervals;iE++) {
        fprintf(fout2d_spectrum[spec],",  \"(%e[MeV]<E<%e[MeV])\"",pow(10.0,iE*dE+logMinEnergyLimit)*J2MeV,pow(10.0,(iE+1)*dE+logMinEnergyLimit)*J2MeV);
      }

      fprintf(fout2d_spectrum[spec],"\nZONE I=%i, J=%i, DATAPACKING=POINT\n",nAzimuthalElements+1,nZenithElements+1);
    }
  }
  else {
    pipe.openSend(0);
  }

  //interpolate and print the state vector
  long int InterpolationList[nZenithElements*nAzimuthalElements],InterpolationListLength=0;
  int AzimuthalShift=nAzimuthalElements/2;

  for (int iZenith=0;iZenith<nZenithElements+1;iZenith++) for (int iAzimuthalIn=0;iAzimuthalIn<nAzimuthalElements+1;iAzimuthalIn++) { 
//for (int iAzimuthal=0;iAzimuthal<nAzimuthalElements;iAzimuthal++) {

    int iAzimuthal=iAzimuthalIn+AzimuthalShift;
    bool shft_flag=true;

    if (iAzimuthal>=nAzimuthalElements) {
      iAzimuthal-=nAzimuthalElements;
      shft_flag=false;
    }

    if (iAzimuthalIn==nAzimuthalElements) {
      shft_flag=false;
      iAzimuthal=+AzimuthalShift;
    } 
   
    Sphere.GetSurfaceCoordinate(x,iZenith,iAzimuthal);

    if (PIC::ThisThread==0) {
      double lon,lat;
      Sphere.GetSurfaceLonLatNormal(lon,lat,iZenith,iAzimuthal);

      if (shft_flag==true) lon-=360.0;

      fprintf(fout2d_rigidity,"%e %e ",lon,lat);
      fprintf(fout2d_total_flux,"%e %e ",lon,lat);

      for (int spec=0;spec<PIC::nTotalSpecies;spec++) fprintf(fout2d_spectrum[spec],"%e %e ",lon,lat);
    }


    //prepare the interpolation stencil
    InterpolationListLength=0;

    if (iZenith==0) {
      InterpolationList[InterpolationListLength++]=Sphere.GetLocalSurfaceElementNumber(0,iAzimuthal);
      InterpolationList[InterpolationListLength++]=Sphere.GetLocalSurfaceElementNumber(0,((iAzimuthal>0) ? iAzimuthal-1 : nAzimuthalElements-1));
    }
    else if (iZenith==nZenithElements) {
      InterpolationList[InterpolationListLength++]=Sphere.GetLocalSurfaceElementNumber(nZenithElements-1,iAzimuthal);
      InterpolationList[InterpolationListLength++]=Sphere.GetLocalSurfaceElementNumber(nZenithElements-1,((iAzimuthal>0) ? iAzimuthal-1 : nAzimuthalElements-1));
    }
    else {
      int iA,iZ,A[2],Z[2];

      Z[0]=iZenith-1,Z[1]=iZenith;

      A[0]=(iAzimuthal!=0) ? iAzimuthal-1 : nAzimuthalElements-1;
      A[1]=iAzimuthal;

      for (iA=0;iA<2;iA++) for (iZ=0;iZ<2;iZ++) InterpolationList[InterpolationListLength++]=Sphere.GetLocalSurfaceElementNumber(Z[iZ],A[iA]);
    }

    //prepare and print the interpolated value of the cutoff rigidity
    //loop throught all species
    for (spec=0;spec<PIC::nTotalSpecies;spec++) {
      //loop therough elements of the interpolation stencil
      double minElementRigidity=Earth::CutoffRigidity::CutoffRigidityTable(spec,InterpolationList[0]);
      double norm=0.0,flux_total=0.0,sum_area=0.0;
      array_1d<double> LocalEnergySpectrum(nTotalEnergySpectrumIntervals);

      double interpolated_number_injected_partilces=0.0;
      int number_injected_partilces=0;
      double max_injected_particle_energy=0.0;

      LocalEnergySpectrum=0.0;

      for (int el=0;el<InterpolationListLength;el++) {
        //loop through all MPI processes

        if (PIC::ThisThread==0) {
          //this process will output data
          double t;

          //the totalsurface area
          sum_area+=Sphere.GetSurfaceElementArea(InterpolationList[el]);
          flux_total+=TotalFlux(InterpolationList[el],spec);
          number_injected_partilces=Earth::CutoffRigidity::InjectedParticleMap(spec,InterpolationList[el]);

          max_injected_particle_energy=Earth::CutoffRigidity::MaxEnergyInjectedParticles(spec,InterpolationList[el]);

          //collect cutoff regidiry from other MPI processes
          for (int thread=1;thread<PIC::nTotalThreads;thread++) {
            number_injected_partilces+=pipe.recv<int>(thread);

            pipe.recv(t,thread);
            if (t>max_injected_particle_energy) max_injected_particle_energy=t;

            pipe.recv(t,thread);
            if ( (t>0.0) && ((minElementRigidity<0.0)||(t<minElementRigidity)) )  minElementRigidity=t;
          }

          interpolated_number_injected_partilces+=number_injected_partilces*Sphere.GetSurfaceElementArea(InterpolationList[el]);

          //recieve the energy specgtrum
          for (iE=0,norm=0.0;iE<nTotalEnergySpectrumIntervals;iE++)  LocalEnergySpectrum(iE)+=EnergySpectrum(InterpolationList[el],spec,iE);

          for (int thread=1;thread<PIC::nTotalThreads;thread++) for (iE=0,norm=0.0;iE<nTotalEnergySpectrumIntervals;iE++) {
            pipe.recv(t,thread);

            LocalEnergySpectrum(iE)+=t;
          }
        }
        else {
          //send the number of injected model particles 
          pipe.send(Earth::CutoffRigidity::InjectedParticleMap(spec,InterpolationList[el]));

          //send the max energy of the injected particles 
          pipe.send(Earth::CutoffRigidity::MaxEnergyInjectedParticles(spec,InterpolationList[el]));

          //send cutoff rigidity to the "root" MPI process
          pipe.send(Earth::CutoffRigidity::CutoffRigidityTable(spec,InterpolationList[el]));

          //send ebergy spectrum to the root
          for (iE=0,norm=0.0;iE<nTotalEnergySpectrumIntervals;iE++) pipe.send(EnergySpectrum(InterpolationList[el],spec,iE));

        }
      }

      //the interpolated value is calcualted by the root MPI process
      if (PIC::ThisThread==0) {

        //normalize the energy spectrum
        for (iE=0,norm=0.0;iE<nTotalEnergySpectrumIntervals;iE++) norm+=LocalEnergySpectrum(iE)*(pow((iE+1)*dE+logMinEnergyLimit,10)-pow(iE*dE+logMinEnergyLimit,10));
        if (norm>0.0) for (iE=0;iE<nTotalEnergySpectrumIntervals;iE++) LocalEnergySpectrum(iE)/=norm;


        fprintf(fout2d_rigidity,"  %e  %e  %e",minElementRigidity,interpolated_number_injected_partilces/sum_area,max_injected_particle_energy);
        fprintf(fout2d_total_flux,"  %e",flux_total/sum_area);

        for (iE=0;iE<nTotalEnergySpectrumIntervals;iE++) fprintf(fout2d_spectrum[spec],"  %e",LocalEnergySpectrum(iE));
      }
    }

    //the cutoff rigidity is computed for all speces at the given point in the map
    if (PIC::ThisThread==0) {
      fprintf(fout2d_rigidity,"\n");
      fprintf(fout2d_total_flux,"\n");

      for (spec=0;spec<PIC::nTotalSpecies;spec++) fprintf(fout2d_spectrum[spec],"\n");
    }
  }

  //close the pipe nad the file
  if (ThisThread==0) {
    fclose(fout2d_rigidity);
    fclose(fout2d_total_flux);

    for (spec=0;spec<PIC::nTotalSpecies;spec++) fclose(fout2d_spectrum[spec]);

    pipe.closeRecvAll();
  }
  else pipe.closeSend();



  //delete all particles that still present in the system
  PIC::ParticleBuffer::DeleteAllParticles();
//  Earth::CutoffRigidity::CutoffRigidityTable.Deallocate();
  Earth::CutoffRigidity::CutoffRigidityTable=0.0;
}


void CutoffRigidityCalculation(int nMaxIterations) {
  //disable sampling
  PIC::Sampling::RuntimeSamplingSwitch=false;

  //estimate the total flux and rigidity at a sphere

  if (Earth::RigidityCalculationMode==Earth::_sphere) {
    if (Earth::RigidityCalculationSphereRadius==0.0) {
      exit(__LINE__,__FILE__,"The radius was not set");
    }
    else {
      SampleSphericalMaplLocations(Earth::RigidityCalculationSphereRadius,nMaxIterations);
    }
  }
  else {
    SampleIndividualLocations(nMaxIterations);
  }


  /*
  //start forward integration
  //enable sampling
  PIC::Sampling::RuntimeSamplingSwitch=true;
  PIC::ParticleBuffer::DeleteAllParticles();
  Earth::ForwardParticleModeling(nMaxIterations);

  //estimate the cutoff rigidity and energy spectrum in individual locations
  PIC::ParticleBuffer::DeleteAllParticles();
  SampleIndividualLocations(nMaxIterations);
  */
}

void CutoffRigidityCalculation_Legacy(int nTotalIterations) {
  int LastDataOutputFileNumber=PIC::DataOutputFileNumber;

  if (Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::ActiveFlag==true) {
    PIC::Mover::BackwardTimeIntegrationMode=_PIC_MODE_ON_;
    Earth::CutoffRigidity::DomainBoundaryParticleProperty::EnableSampleParticleProperty=true;

    //particles will be injected only in the near Earth's region
    Earth::BoundingBoxInjection::BoundaryInjectionMode=false;
    Earth::CutoffRigidity::ParticleInjector::ParticleInjectionMode=true;

    for (long int niter=0;(niter<nTotalIterations)&&(LastDataOutputFileNumber<Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::LastActiveOutputCycleNumber);niter++) {
      amps_time_step();

      if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
        PIC::RequiredSampleLength*=2;
        if (PIC::RequiredSampleLength>50000) PIC::RequiredSampleLength=50000;


        LastDataOutputFileNumber=PIC::DataOutputFileNumber;
        if (PIC::Mesh::mesh->ThisThread==0) cout << "The new sample length is " << PIC::RequiredSampleLength << endl;
      }
    }

    Earth::CutoffRigidity::DomainBoundaryParticleProperty::Gather();
    Earth::CutoffRigidity::DomainBoundaryParticleProperty::SmoothSampleTable();
    PIC::Mover::BackwardTimeIntegrationMode=_PIC_MODE_OFF_;

    //partices will be injected from the boundary of the domain
    Earth::BoundingBoxInjection::BoundaryInjectionMode=true;
    Earth::CutoffRigidity::ParticleInjector::ParticleInjectionMode=false;
    Earth::CutoffRigidity::DomainBoundaryParticleProperty::ApplyInjectionPhaseSpaceLimiting=true;
    Earth::CutoffRigidity::DomainBoundaryParticleProperty::EnableSampleParticleProperty=false;
  }

  //time step with the forward integration
  for (long int niter=0;niter<nTotalIterations;niter++) {
    amps_time_step();
    
    if (PIC::Mesh::mesh->ThisThread==0) {
      time_t TimeValue=time(NULL);
      tm *ct=localtime(&TimeValue);
      printf(": (%i/%i %i:%i:%i), Iteration: %ld  (current sample length:%ld, %ld interations to the next output)\n",
       ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec,niter,
       PIC::RequiredSampleLength,
       PIC::RequiredSampleLength-PIC::CollectingSampleCounter);
    }

     if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
       PIC::RequiredSampleLength*=2;
       if (PIC::RequiredSampleLength>50000) PIC::RequiredSampleLength=50000;


       LastDataOutputFileNumber=PIC::DataOutputFileNumber;
       if (PIC::Mesh::mesh->ThisThread==0) cout << "The new sample length is " << PIC::RequiredSampleLength << endl;
     }
  }
}


int main(int argc,char **argv) {

  //===================================================================================
  // New lightweight CLI pre-parser
  //===================================================================================
  // The historical srcEarth/main.cpp is a large driver for PIC-based workflows.
  // For the CCMC Runs-on-Request interface we also need a "gridless" execution
  // mode that bypasses the mesh/PIC setup and directly evaluates analytic field
  // models (T96/T05) for cutoff rigidity.
  //
  // Requirement: support
  //   -h
  //   -mode 3d|gridless
  //   -i <input>
  //
  // We intentionally place this block at the *beginning* of main(). If -mode
  // gridless is selected, we execute the gridless solver and then return.
  //===================================================================================
  try {
    EarthUtil::CliOptions cli = EarthUtil::ParseCli(argc,argv);

    if (cli.help) {
      std::cout << EarthUtil::HelpMessage(argv[0]);
      return 0;
    }

    if (!cli.mode.empty()) {
      std::string m = EarthUtil::ToUpper(cli.mode);
      if (m=="GRIDLESS") {
        if (cli.inputFile.empty()) {
          std::cerr << "Error: -mode gridless requires -i <input-file>\n";
          std::cerr << EarthUtil::HelpMessage(argv[0]);
          return 1;
        }

	PIC::InitMPI();
	Exosphere::Init_SPICE();

        // Standalone GRIDLESS is deliberately mesh-free.  PIC::InitMPI() above only
        // initializes the MPI runtime; this branch never calls amps_init_mesh(), never
        // builds/reads the AMR tree, and never allocates/populates the Mode3D magnetic-
        // field mesh.  Magnetic fields are evaluated directly along each trajectory.
        // Keep this branch returning before the historical initialization path below.
        if (PIC::ThisThread==0) {
          std::cout << "[gridless] AMR mesh initialization: SKIPPED "
                    << "(direct field evaluation along trajectories)\n";
          std::cout.flush();
        }

        EarthUtil::AmpsParam p = EarthUtil::ParseAmpsParamFile(cli.inputFile);

        // Establish the final global epoch before any field or trajectory runtime is
        // entered.  A non-empty CLI value overrides #BACKGROUND_FIELD / EPOCH.
        if (!ApplyEpochCli(cli,p,"gridless")) return 1;

        // Apply command-line overrides that are common to the standalone backward
        // products before dispatching to the gridless cutoff or density/flux solver.
        // This is important for options such as -cutoff-search and
        // -gridless-mpi-scheduler: they are parsed by the generic CLI layer, but the
        // solver sees them only after they are copied into AmpsParam here.
        if (!ApplyCommonBackwardCli(cli,p,"gridless")) return 1;

	//set up mover type used in the calculation 
        if (!ApplyCutoffMoverCli(cli)) return 1;


        // Dispatch gridless workflows by CALC_TARGET.
        // - CUTOFF_RIGIDITY   : existing gridless cutoff tool
        // - DENSITY_SPECTRUM  : energy-grid transmissivity + density integration
        const std::string target = EarthUtil::ToUpper(p.calc.target);
        if (target=="CUTOFF_RIGIDITY") {
          Earth::GridlessMode::RunCutoffRigidity(p);
	  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
	  MPI_Finalize();
	  return EXIT_SUCCESS;
        }
        if (target=="DENSITY_SPECTRUM") {
          if (EarthUtil::ToUpper(p.calc.fieldEvalMethod)!="GRIDLESS") {
            throw std::runtime_error("-mode gridless with CALC_TARGET=DENSITY_SPECTRUM requires FIELD_EVAL_METHOD=GRIDLESS");
          }
          Earth::GridlessMode::RunDensityAndSpectrum(p);
	  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
	  MPI_Finalize();
          return EXIT_SUCCESS;
        }

        throw std::runtime_error("Unsupported CALC_TARGET for -mode gridless: '"+p.calc.target+"'");
      }
      if (m=="3D") {
        if (cli.inputFile.empty()) {
          std::cerr << "Error: -mode 3d requires -i <input-file>\n";
          std::cerr << EarthUtil::HelpMessage(argv[0]);
          return 1;
        }
        // Initialize SPICE before reading AMPS_PARAM.in when this executable has
        // SPICE support and is not running as an SWMF-coupled component.  This is
        // needed because the parser itself may load time-dependent Tsyganenko
        // driving files and convert their UTC timestamps to SPICE ET.
        InitStandaloneSpiceBeforeParamParsing("3d");

        EarthUtil::AmpsParam p = EarthUtil::ParseAmpsParamFile(cli.inputFile);

        // Apply --epoch before Mode3D initializes the background field or populates
        // the AMR mesh.  This guarantees the mesh and analytic evaluator use the same
        // epoch selected by the user.
        if (!ApplyEpochCli(cli,p,"mode 3d")) return 1;

        // Apply command-line overrides shared by Mode3D and gridless backward
        // calculations.  This includes the penumbra-safe cutoff-search controls
        // and the dynamic MPI scheduler controls.
        if (!ApplyCommonBackwardCli(cli,p,"mode 3d")) return 1;
        if (!ApplyMode3DMeshResolutionCli(cli,p)) return 1;

        // Mode3D-specific CLI overrides. Defaults are deliberately conservative:
        // do not write the potentially large initialized-mesh Tecplot file, and
        // use AMR interpolation during tracing unless direct background-field
        // evaluation is requested explicitly.
        p.mode3d.outputInitializedFile = cli.mode3dOutputInitialized;
        p.mode3d.parallelFieldInitialization =
            cli.mode3dParallelFieldInitialization;
        if (!cli.mode3dFieldEval.empty()) {
          const std::string fieldEval = EarthUtil::ToUpper(cli.mode3dFieldEval);
          if (fieldEval=="ANALYTIC" || fieldEval=="DIRECT") {
            p.mode3d.forceAnalyticMagneticField = true;
          }
          else if (fieldEval=="INTERPOLATION" || fieldEval=="INTERPOLATED" ||
                   fieldEval=="MESH" || fieldEval=="AMR") {
            p.mode3d.forceAnalyticMagneticField = false;
          }
          else {
            std::cerr << "Error: unknown Mode3D field-evaluation option -mode3d-field-eval "
                      << cli.mode3dFieldEval
                      << ". Valid values: INTERPOLATION or ANALYTIC.\n";
            return 1;
          }
        }

        // Generic MPI-scheduler CLI overrides are applied by ApplyCommonBackwardCli()
        // immediately after parsing AMPS_PARAM.in.

        // Optional single-point Mode3D cutoff diagnostic.  This does not change the
        // main cutoff map; it only writes a rigidity-classification scan before the
        // full calculation so numerical/bracketing issues can be isolated.
        if (cli.cutoffDebugScan) {
          p.cutoff.debugRigidityScan = true;
          p.cutoff.debugScanLon_deg  = cli.cutoffDebugScanLon_deg;
          p.cutoff.debugScanLat_deg  = cli.cutoffDebugScanLat_deg;
          p.cutoff.debugScanAlt_km   = cli.cutoffDebugScanAlt_km;
        }
        if (cli.cutoffDebugScanN > 0) {
          p.cutoff.debugScanN = cli.cutoffDebugScanN;
        }
        if (!cli.cutoffDebugScanFile.empty()) {
          p.cutoff.debugScanFile = cli.cutoffDebugScanFile;
        }
        if (cli.cutoffDebugExit) {
          p.cutoff.debugExitTrace = true;
          p.cutoff.debugExitLon_deg = cli.cutoffDebugExitLon_deg;
          p.cutoff.debugExitLat_deg = cli.cutoffDebugExitLat_deg;
          p.cutoff.debugExitAlt_km = cli.cutoffDebugExitAlt_km;
        }
        if (cli.cutoffDebugExitR_GV > 0.0) {
          p.cutoff.debugExitR_GV = cli.cutoffDebugExitR_GV;
        }
        if (cli.cutoffDebugExitN > 0) {
          p.cutoff.debugExitN = cli.cutoffDebugExitN;
        }
        if (!cli.cutoffDebugExitListFile.empty()) {
          p.cutoff.debugExitTrace = true;
          p.cutoff.debugExitListFile = cli.cutoffDebugExitListFile;
        }
        if (!cli.cutoffDebugExitFile.empty()) {
          p.cutoff.debugExitFile = cli.cutoffDebugExitFile;
        }
        // Generic cutoff-search CLI overrides are applied by ApplyCommonBackwardCli()
        // immediately after parsing AMPS_PARAM.in so the same -cutoff-search and
        // -cutoff-upper-scan-n flags affect both -mode 3d and -mode gridless.

        // Keep the trajectory integrator identical to the gridless path when
        // the user selects a mover on the command line.
        if (!ApplyCutoffMoverCli(cli)) return 1;

        Earth::Mode3D::Run(p);
	MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
	MPI_Finalize();
        return EXIT_SUCCESS;
      }
      if (m=="3D_FORWARD") {
        if (cli.inputFile.empty()) {
          std::cerr << "Error: -mode 3d_forward requires -i <input-file>\n";
          std::cerr << EarthUtil::HelpMessage(argv[0]);
          return 1;
        }
        // Same early-SPICE requirement as -mode 3d: 3-D forward runs can also use
        // the standalone background-field parser, and future/diagnostic inputs may
        // contain timestamped driver tables.
        InitStandaloneSpiceBeforeParamParsing("3d_forward");

        EarthUtil::AmpsParam p = EarthUtil::ParseAmpsParamFile(cli.inputFile);

        // Forward transport must use the same CLI-overridden epoch during mesh-field
        // initialization, injection-coordinate transforms, and particle propagation.
        if (!ApplyEpochCli(cli,p,"mode 3d_forward")) return 1;
        if (!ApplyMode3DMeshResolutionCli(cli,p)) return 1;

        // ---- CLI overrides for 3d_forward ----
        // Shared flag: diagnostic initialized-mesh Tecplot file.
        p.mode3dForward.outputInitializedFile = cli.mode3dOutputInitialized;

        // Reuse the same field-evaluation CLI switch as -mode 3d.  The new
        // 3d_forward RK4/GC/HYBRID movers use this flag to choose between AMR
        // interpolation of the initialized 3-D mesh and direct analytic calls to
        // Earth::Mode3D::EvaluateBackgroundMagneticFieldSI().
        if (!cli.mode3dFieldEval.empty()) {
          const std::string fieldEval = EarthUtil::ToUpper(cli.mode3dFieldEval);
          if (fieldEval=="ANALYTIC" || fieldEval=="DIRECT") {
            p.mode3d.forceAnalyticMagneticField = true;
          }
          else if (fieldEval=="INTERPOLATION" || fieldEval=="INTERPOLATED" ||
                   fieldEval=="MESH" || fieldEval=="AMR") {
            p.mode3d.forceAnalyticMagneticField = false;
          }
          else {
            std::cerr << "Error: unknown field-evaluation option -mode3d-field-eval "
                      << cli.mode3dFieldEval
                      << ". Valid values: INTERPOLATION or ANALYTIC.\n";
            return 1;
          }
        }

        // Number of forward iterations.
        if (cli.forward3dNiter > 0) p.mode3dForward.nIterations = cli.forward3dNiter;

        // Simulation particles injected per iteration (determines physical weight W).
        // W = (pi * integral_J * A_boundary * dt) / nParticlesPerIter
        // A positive sentinel means the CLI flag was supplied; <= 0 means use
        // the input-file default stored in p.mode3dForward.nParticlesPerIter.
        if (cli.forward3dNparticles > 0)
          p.mode3dForward.nParticlesPerIter = cli.forward3dNparticles;

        // Boundary distribution type (default ISOTROPIC; extensible).
        if (!cli.forward3dBoundaryDist.empty())
          p.mode3dForward.boundaryDistType = EarthUtil::ToUpper(cli.forward3dBoundaryDist);

        // Energy proposal distribution for the forward boundary source.
        // SPECTRUM keeps the legacy branch; LOG_UNIFORM oversamples the
        // high-energy tail and applies individual particle weight corrections.
        if (!cli.forward3dInjectionEnergyDistribution.empty())
          p.mode3dForward.injectionEnergyDistribution =
              EarthUtil::ToUpper(cli.forward3dInjectionEnergyDistribution);

        // Optional CLI override for the 3d_forward particle-energy limits.
        // In forward mode, #DENSITY_3D is treated as the authoritative particle
        // energy grid: it controls density-output bins, the DENS_EMAX time-step
        // estimate, and the boundary injection/integration range used inside
        // Mode3DForward::Run().  Therefore the CLI limit override updates the
        // effective density3d range rather than editing only #SPECTRUM.
        if (cli.forward3dInjectionEmin_MeV > 0.0)
          p.density3d.Emin_MeV = cli.forward3dInjectionEmin_MeV;
        if (cli.forward3dInjectionEmax_MeV > 0.0)
          p.density3d.Emax_MeV = cli.forward3dInjectionEmax_MeV;

        if (!(p.density3d.Emin_MeV > 0.0) ||
            !(p.density3d.Emax_MeV > p.density3d.Emin_MeV)) {
          std::cerr << "Error: 3d_forward energy limits require 0 < Emin < Emax "
                    << "(MeV/n). Current values: Emin=" << p.density3d.Emin_MeV
                    << ", Emax=" << p.density3d.Emax_MeV << "\n";
          return 1;
        }

        // Particle mover for 3d_forward.  The active selection path is currently CLI
        // only: -mover sets the concrete method used internally by the single
        // AMPS-facing 3d_forward mover manager.  The input-file parser has reserved
        // storage for future FORWARD_MOVER-style keywords, but those values are not
        // applied here yet to avoid two competing selector paths.
        if (!cli.mover.empty()) {
          p.mode3dForward.particleMover = EarthUtil::ToUpper(cli.mover);
        }

        Exosphere::Init_SPICE();
        Earth::Mode3DForward::Run(p);
        MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
        MPI_Finalize();
        return EXIT_SUCCESS;
      }

      // Other values are handled by the legacy path below.
    }
  }
  catch (std::exception& e) {
    std::cerr << "CLI error: " << e.what() << "\n";
    std::cerr << EarthUtil::HelpMessage(argv[0]);
    return 1;
  }

  static int LastDataOutputFileNumber=0;

  Earth::CutoffRigidity::SampleRigidityMode=true;

  //read post-compile input file  
  if (PIC::PostCompileInputFileName!="") {
    Earth::Parser::ReadFile(PIC::PostCompileInputFileName,Earth::Parser::_reading_mode_pre_init);
  } 

  amps_init_mesh();

  if (PIC::PostCompileInputFileName!="") {
    Earth::Parser::ReadFile(PIC::PostCompileInputFileName,Earth::Parser::_reading_mode_pre_init);
  }

  Earth::CutoffRigidity::Init_BeforeParser();
  Earth::CutoffRigidity::AllocateCutoffRigidityTable();

  amps_init();

/*
  //read post-compile input file
  if (PIC::PostCompileInputFileName!="") {
    Earth::Parser::ReadFile(PIC::PostCompileInputFileName,Earth::Parser::_reading_mode_post_init);
  }
*/



  if (Earth::ModelMode==Earth::BoundaryInjectionMode) {
    int nIterations,nTotalIterations=100000001;
    double et;

    if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_)  nTotalIterations=3200;

    //load parameters of T05 model
    if (Earth::BackgroundMagneticFieldT05Data!="") {
      T05::LoadDataFile(Earth::BackgroundMagneticFieldT05Data.c_str()); 
    }

    // TA16: the coefficient file (TA16_RBF.par) must be in the current working
    // directory, or set via TA16::SetCoeffFileName() before amps_init() is called.
    // This mirrors the TA15 pattern: there is no separate data-file loading step
    // here for TA16 — the Fortran default (TA16_RBF.par in CWD) is used unless
    // the caller has already invoked TA16::SetCoeffFileName explicitly.

    //init the simulation start time 
    str2et_c(Exosphere::SimulationStartTimeString,&et); 

    int iEtInterval=-1;

    for (int i=0;i<T05::Data.size()-1;i++) {  
      if ((T05::Data[i].et<=et)&&(et<=T05::Data[i+1].et)) {
        iEtInterval=i;
        break;
      }
    }

    if (iEtInterval==-1) exit(__LINE__,__FILE__,"Error: the time intervais was not found");

    T05::SetSolarWindPressure_nano(T05::Data[iEtInterval].Pdyn);
    T05::SetDST_nano(T05::Data[iEtInterval].SYMH);
    T05::SetBYIMF_nano(T05::Data[iEtInterval].BYGSM);
    T05::SetBZIMF_nano(T05::Data[iEtInterval].BZGSM);

    T05::SetW1(T05::Data[iEtInterval].W1);
    T05::SetW2(T05::Data[iEtInterval].W2);
    T05::SetW3(T05::Data[iEtInterval].W3);
    T05::SetW4(T05::Data[iEtInterval].W4);
    T05::SetW5(T05::Data[iEtInterval].W5);
    T05::SetW6(T05::Data[iEtInterval].W6);

    // TA16 initial parameter set.
    // The T05 driver file (cT05Data) carries Pdyn, SYMH, BYGSM and W1..W6
    // which map directly onto TA16's PARMOD(1..2,4..10).  XIND (PARMOD(3))
    // is not present in the standard T05 driver format; it defaults to 0.0.
    // Extend cT05Data with an XIND column and wire it here once it is available.
    #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__TA16_
    TA16::SetSolarWindPressure_nano(T05::Data[iEtInterval].Pdyn);
    TA16::SetSymHc_nano(T05::Data[iEtInterval].SYMH);
    TA16::SetXIND(0.0);
    TA16::SetBYIMF_nano(T05::Data[iEtInterval].BYGSM);
    #endif

    Earth::InitMagneticField();

  //read post-compile input file
  if (PIC::PostCompileInputFileName!="") {
    Earth::Parser::ReadFile(PIC::PostCompileInputFileName,Earth::Parser::_reading_mode_post_init);
  }


    //time step
    for (long int niter=0;niter<nTotalIterations;niter++) {
      static int LastDataOutputFileNumber=-1;

      PIC::TimeStep();

      et+=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
      int iEtInterval_prev=iEtInterval;
      
      for (;iEtInterval<T05::Data.size()-1;iEtInterval++) {  
        if ((T05::Data[iEtInterval].et<=et)&&(et<=T05::Data[iEtInterval+1].et)) {
          break;
        }
      }

      if (iEtInterval_prev!=iEtInterval) {
        //update magnetic field model
        T05::SetSolarWindPressure_nano(T05::Data[iEtInterval].Pdyn);
        T05::SetDST_nano(T05::Data[iEtInterval].SYMH);
        T05::SetBYIMF_nano(T05::Data[iEtInterval].BYGSM);
        T05::SetBZIMF_nano(T05::Data[iEtInterval].BZGSM);
        T05::SetIMF_nano(T05::Data[iEtInterval].BXGSM,T05::Data[iEtInterval].BYGSM,T05::Data[iEtInterval].BZGSM);
    
        T05::SetW1(T05::Data[iEtInterval].W1);
        T05::SetW2(T05::Data[iEtInterval].W2);
        T05::SetW3(T05::Data[iEtInterval].W3);
        T05::SetW4(T05::Data[iEtInterval].W4);
        T05::SetW5(T05::Data[iEtInterval].W5);
        T05::SetW6(T05::Data[iEtInterval].W6);

        // TA16 time-step update — mirrors T05 above.
        #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__TA16_
        TA16::SetSolarWindPressure_nano(T05::Data[iEtInterval].Pdyn);
        TA16::SetSymHc_nano(T05::Data[iEtInterval].SYMH);
        TA16::SetXIND(0.0);
        TA16::SetBYIMF_nano(T05::Data[iEtInterval].BYGSM);
        #endif

        Earth::InitMagneticField();
      }


      if ((PIC::DataOutputFileNumber!=0)&&(PIC::DataOutputFileNumber!=LastDataOutputFileNumber)) {
        PIC::RequiredSampleLength*=2;
        if (PIC::RequiredSampleLength>1000) PIC::RequiredSampleLength=1000;


        LastDataOutputFileNumber=PIC::DataOutputFileNumber;
        if (PIC::Mesh::mesh->ThisThread==0) cout << "The new sample length is " << PIC::RequiredSampleLength << endl;
      }

      if (PIC::Mesh::mesh->ThisThread==0) {
        time_t TimeValue=time(NULL);
        tm *ct=localtime(&TimeValue);

        printf(": (%i/%i %i:%i:%i), Iteration: %ld  (current sample length:%ld, %ld interations to the next output)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec,niter,PIC::RequiredSampleLength,PIC::RequiredSampleLength-PIC::CollectingSampleCounter);
      }
    }
  }
  else if (Earth::ModelMode==Earth::CutoffRigidityMode) {
    if (_PIC_NIGHTLY_TEST_MODE_ == _PIC_MODE_ON_) {
      //execute the nightly test routine

      int niter=(Earth::CutoffRigidity::nTotalIterations!=-1) ? Earth::CutoffRigidity::nTotalIterations : 10000;

      switch (_NIGHTLY_TEST_) {
      case _NIGHTLY_TEST__CUTOFF_:
        if (PIC::ThisThread==0) cout << "_NIGHTLY_TEST_=_NIGHTLY_TEST__CUTOFF_" << endl;

        CutoffRigidityCalculation(niter);
        break;
      case _NIGHTLY_TEST__LEGACY_:
        if (PIC::ThisThread==0) cout << "_NIGHTLY_TEST_=_NIGHTLY_TEST__LEGACY_" << endl;

        CutoffRigidityCalculation_Legacy(0);
        break;
       }

      //output the particle statistics of the test run
      char fname[300];
      sprintf(fname,"%s/test_Earth.dat",PIC::OutputDataFileDirectory);
      PIC::RunTimeSystemState::GetMeanParticleMicroscopicParameters(fname);
    }
    else {
      int niter=(Earth::CutoffRigidity::nTotalIterations!=-1) ? Earth::CutoffRigidity::nTotalIterations : 10000;

      CutoffRigidityCalculation(niter);
    }
  }
  else exit(__LINE__,__FILE__,"Error: the option is not recognized");


  //finish the run
  MPI_Finalize();
  cout << "End of the run:" << PIC::nTotalSpecies << endl;
  
  return EXIT_SUCCESS;
}
