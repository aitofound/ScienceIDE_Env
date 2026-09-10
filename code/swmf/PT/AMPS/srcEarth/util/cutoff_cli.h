//======================================================================================
// cutoff_cli.h
//======================================================================================
//
// PURPOSE
// -------
// Command-line interface (CLI) for the Earth energetic particle gridless tools.
// Parses argc/argv into a CliOptions struct that the main executable uses to
// configure the solver run. Intentionally kept independent of the solver and parser
// so it can be unit-tested in isolation.
//
//======================================================================================
// SUPPORTED OPTIONS
//======================================================================================
//
//   -h | --help
//       Print help text and exit. No other processing is done.
//
//   -mode <string>
//       Select the solver execution mode.
//       Recognised values:
//         3d        Run the full PIC-backed 3D solver.
//         gridless  Run the gridless field-evaluation solver (Tsyganenko + Boris).
//       Required. Error if absent and -h is not given.
//
//   -i <path>
//       Path to the AMPS_PARAM-format input file. Parsed by ParseAmpsParamFile.
//       Required unless -h is given.
//
//   -epoch | --epoch <UTC>
//       Override #BACKGROUND_FIELD / EPOCH after the input file is parsed and
//       before any Geopack, Tsyganenko, SPICE-frame, mesh-field, or trajectory
//       initialization occurs.  Recommended syntax:
//         YYYY-MM-DDTHH:MM:SS
//       Example:
//         --epoch 2010-01-01T00:00:00
//       If the timestamp contains spaces, quote it in the shell.  If omitted, the
//       input-file EPOCH value is used; if the input file also omits EPOCH, the
//       existing default 2000-01-01T00:00 remains unchanged.
//
//   -mover <string>
//       Select the particle integration algorithm.
//       Recognised values (case-insensitive):
//         BORIS   Relativistic Boris pusher (default; recommended for all production runs)
//         RK2     Runge-Kutta 2nd order (Heun)
//         RK4     Runge-Kutta 4th order (classical)
//         RK6     Runge-Kutta 6th order
//         GC2     Guiding-center equations integrated with RK2
//         GC4     Guiding-center equations integrated with RK4
//         GC6     Guiding-center equations integrated with RK6
//         HYBRID  Switch per step between RK4 and GC4 when the local motion is
//                 sufficiently adiabatic for guiding-center transport
//       In gridless and backward 3D modes, all listed values are handled by the
//       shared GridlessParticleMovers layer. In 3d_forward mode, the AMPS-signature
//       mover manager currently supports BORIS, RK4, GC/GC4, and HYBRID. HC4 is available in gridless/backward 3-D tracing.
//       If omitted, the default mover (BORIS) is used.
//       The string is stored as-is; translation to the concrete mover is done by the caller.
//
//   -density-mode <string>
//       Override the DS_BOUNDARY_MODE key from the input file.
//       Recognised values (case-insensitive):
//         ISOTROPIC     Use uniform isotropic boundary spectrum (original behavior).
//         ANISOTROPIC   Use pitch-angle- and spatially-weighted boundary spectrum.
//                       Requires a #BOUNDARY_ANISOTROPY section in the input file,
//                       or the solver will throw at startup.
//       If omitted, the input file value (or its default ISOTROPIC) is used.
//       CLI override is useful for:
//         - Comparing isotropic and anisotropic results on the same input file
//           without editing the file: run twice with -density-mode ISOTROPIC and
//           -density-mode ANISOTROPIC.
//         - Automated test scripts that exercise both branches from a single
//           test input file.
//
//   -mode3d-output-initialized
//       In -mode 3d, write amps_3d_initialized.data.dat after mesh field
//       initialization. The default is to skip this potentially large diagnostic file.
//
//   -mode3d-parallel-field-init
//       In standalone -mode 3d, populate owner-rank B/E mesh cells with POSIX
//       threads.  The number of temporary pthread workers is taken from
//       -mode3d-threads (-density-threads alias).  The calling MPI-rank thread
//       also evaluates an equal share, so N requests N workers plus the caller.
//
//   -mode3d-field-eval <INTERPOLATION|ANALYTIC>
//       In -mode 3d, select how the magnetic field is evaluated during tracing.
//       INTERPOLATION (default) uses the AMR cell-centered interpolation stencil.
//       ANALYTIC calls the same background-field function used to initialize the
//       mesh cell centers.
//
//   -density-parallel <OPENMP|THREADS|SERIAL>
//       Select the shared-memory backend for backward trajectory products inside each
//       MPI process.  Aliases include -mode3d-parallel and -gridless-parallel.
//       THREADS uses direct std::thread workers; SERIAL disables intra-rank parallelism.
//
//   -density-threads <int>
//       Number of shared-memory workers per MPI process.  Aliases include
//       -mode3d-threads and -gridless-threads.  GRIDLESS cutoff uses this worker count
//       to size automatic dynamic-MPI chunks as well as the local worker pool.
//
//   -mode3d-mpi-scheduler <DYNAMIC|BLOCK_CYCLIC|STATIC>
//       Select the inter-rank scheduler for standalone Mode3D and gridless cutoff/density runs.
//
//   -mode3d-mpi-dynamic-chunk <int>
//       Number of dynamic MPI work items per fetch. Mode3D cutoff uses flattened trajectory tasks; Mode3D density/flux uses locations. 0 means automatic.
//
//   -cutoff-search <UPPER_SCAN|PENUMBRA_SCAN|RIGIDITY_LIST|BINARY>
//       Select the cutoff-rigidity product. UPPER_SCAN returns the upper penumbra
//       edge; PENUMBRA_SCAN evaluates the full access sequence; RIGIDITY_LIST is a
//       Mode3D-only direct-access product at explicitly supplied rigidity values;
//       BINARY is the legacy endpoint method.
//       Mode-specific aliases are also accepted, including
//       -mode3d-cutoff-search and -gridless-cutoff-search.
//
//   -cutoff-upper-scan-n <int>
//       Number of log-spaced rigidity samples used by UPPER_SCAN before the final
//       forbidden/allowed bisection.  If omitted, the solver reuses CUTOFF_NENERGY.
//
//   -cutoff-rigidity-list-gv <comma-separated-list>
//       Explicit positive rigidity values traced by RIGIDITY_LIST, in GV.
//
//   -cutoff-access-abs-lat-min/max <deg>
//       Restrict the Mode3D RIGIDITY_LIST launch grid to a geodetic absolute-latitude
//       band while retaining both hemispheres and all configured longitudes.
//
//   -cutoff-dirmap-coverage <FULL_SPHERE|VECTOR_APERTURES>
//       Select angular coverage of DIRECTIONAL_MAP/direct A(E,Omega) tasks. FULL_SPHERE
//       retains the historical complete sky. VECTOR_APERTURES keeps only regular-grid
//       cells inside arbitrary finite-FOV look-direction apertures. LOCATION-qualified
//       records in an aperture file remain local to their associated Mode3D location.
//
//   -cutoff-dirmap-aperture-file <path>
//       Replace the input-file aperture list with the definitions in <path>.
//
//   -cutoff-dirmap-aperture "<name> <frame> bx by bz ux uy uz hHalf vHalf"
//       Add one arbitrary aperture from the CLI. May be repeated. Frames: SM, GSM,
//       LOCAL_SM. The boresight is the physical detector LOOK direction.
//
//   -adaptive-dt <T|F>
//       Override #NUMERICAL ADAPTIVE_DT.  T/default means DT_TRACE is a maximum
//       adaptive-step bound; F means fixed-step tracing with DT_TRACE.
//
//   -max-trace-distance <double>
//       Override #NUMERICAL MAX_TRACE_DISTANCE from the input file.
//       Units: Earth radii (Re) of cumulative traced path length.
//       Semantics:
//         value > 0   enable the hard cumulative-distance cap
//         value = 0   disable the cap
//       This mirrors MAX_TRACE_TIME, but limits total geometric distance traveled
//       by a trajectory rather than elapsed integration time.
//
//   -mode3d-mesh-res-earth-re <double>
//   -mode3d-mesh-res-boundary-re <double>
//   -mode3d-mesh-coarsening <LINEAR|LOG|EXPONENTIAL|POWER|CONSTANT>
//   -mode3d-mesh-exponent <double>
//   -mode3d-mesh-r-boundary-re <double>
//       Optional standalone -mode 3d AMR mesh-resolution profile.  If omitted,
//       the current hard-coded main_lib.cpp localResolution() profile is used.
//
//======================================================================================
// USAGE EXAMPLES
//======================================================================================
//
//   Basic cutoff-rigidity run (isotropic, Boris pusher):
//     ./amps -mode gridless -i run.in
//
//   Density/spectrum, anisotropic boundary, override from command line:
//     ./amps -mode gridless -i run.in -density-mode ANISOTROPIC
//
//   Select a different background-field epoch without editing run.in:
//     ./amps -mode gridless -i run.in --epoch 2010-01-01T00:00:00
//
//   Density/spectrum, RK4 mover for comparison:
//     ./amps -mode gridless -i run.in -mover RK4
//
//   Print help:
//     ./amps -h
//
//======================================================================================
// ERROR HANDLING
//======================================================================================
//
// ParseCli throws std::runtime_error for:
//   - An option flag that requires an argument but none follows (e.g., "-i" at end)
//   - An unrecognised option flag (e.g., "-xyz")
//
// It does NOT throw for:
//   - Missing required options (the main executable checks CliOptions after parsing)
//   - Unrecognised values for -mover or -density-mode (passed through as strings;
//     validated by the solver at startup)
//
//======================================================================================

#ifndef _SRC_EARTH_UTIL_CUTOFF_CLI_H_
#define _SRC_EARTH_UTIL_CUTOFF_CLI_H_

#include <string>
#include <vector>  // CliOptions::cutoffDirMapApertureSpecs is a repeatable list of CLI aperture specifications.

namespace EarthUtil {

  struct CliOptions {
    bool help{false};
    std::string mode{""};
    std::string inputFile{""};

    // -epoch | --epoch <UTC>
    // ---------------------------------------------------------------------
    // Optional command-line override for #BACKGROUND_FIELD / EPOCH.
    //
    // Why this value is stored separately from AmpsParam here:
    //   ParseCli() intentionally knows nothing about the input-file parser or
    //   field-model implementation.  It records only what the user supplied on
    //   the command line.  main.cpp parses AMPS_PARAM.in first and then applies
    //   this non-empty string to p.field.epoch.  Consequently the precedence is:
    //
    //       CLI --epoch  >  #BACKGROUND_FIELD EPOCH  >  compiled default
    //
    // The override is applied before any field initialization, so the selected
    // time is used consistently by Geopack RECALC/IGRF coefficients, Tsyganenko
    // dipole tilt, SPICE frame rotations, mesh field materialization, and
    // backward/forward trajectory products.  Empty means that no CLI override
    // was supplied and the parsed input/default value must remain unchanged.
    std::string epoch{""};

    // Particle mover selection.
    // NOTE: This is intentionally a *string* here to keep the CLI independent of the
    // gridless integrator implementation. The executable can translate this string into
    // a concrete enum (MoverType) or the 3d_forward manager selection.
    //
    // Supported values (case-insensitive):
    //   BORIS           : classic relativistic Boris pusher (legacy default)
    //   HC4           : fourth-order Higuera-Cary/Boris composition; RK4-like accuracy with Boris-like rigidity conservation
    //   RK2/RK4/RK6   : explicit full-orbit Runge-Kutta movers of order 2/4/6
    //   GC2/GC4/GC6   : guiding-center movers integrated with RK2/RK4/RK6
    //   HYBRID        : per-step switch between RK4 and GC4 using a local
    //                   adiabaticity criterion rho/L_eff
    //
    // If empty, the executable should use its default setting.
    std::string mover{""};

    // -density-mode ISOTROPIC|ANISOTROPIC
    // Overrides DS_BOUNDARY_MODE from the input file.
    // ISOTROPIC   : uniform isotropic boundary (original behavior).
    // ANISOTROPIC : pitch-angle-dependent and spatially non-uniform boundary;
    //               requires a #BOUNDARY_ANISOTROPY section in the input file.
    std::string densityMode{""};

    // -density-transmission-mode DIRECT|SCAN|ADAPTIVE
    // Controls the energy/rigidity nodes on which density/flux transmissivity T is
    // evaluated.  DIRECT keeps the legacy DS energy grid.  SCAN/ADAPTIVE build a
    // log-spaced rigidity grid converted back to kinetic energy for spectrum folding.
    std::string densityTransmissionMode{""};

    // -density-transmission-scan-n <int>
    // Number of rigidity scan points for SCAN/ADAPTIVE mode. 0 means no CLI override.
    int densityTransmissionScanN{0};

    // -density-transmission-refine-n <int> and -density-transmission-max-n <int>
    // Parsed for forward-compatible input decks; current production output uses the
    // fixed scan grid so all locations have the same Tecplot energy axis.
    int densityTransmissionRefineN{0};
    int densityTransmissionMaxN{0};

    // -mode3d-output-initialized
    // Boolean flag. When true, Mode3D::Run writes the initialized AMR mesh fields
    // to amps_3d_initialized.data.dat. The default is false to avoid creating this
    // large diagnostic file unless explicitly requested.
    bool mode3dOutputInitialized{false};

    // -mode3d-parallel-field-init
    // Boolean flag. When present, standalone Mode3D initializes owner-rank AMR
    // cell-centered B/E fields with a temporary POSIX-thread team.  The number
    // of temporary pthread workers is the same value selected by
    // -mode3d-threads/-density-threads.  The calling MPI-rank thread also
    // evaluates an equal share, so N requests N workers plus the caller.
    bool mode3dParallelFieldInitialization{false};

    // -mode3d-field-eval <INTERPOLATION|ANALYTIC>
    // Optional Mode3D magnetic-field evaluation override.
    // Empty or INTERPOLATION uses the AMR interpolation stencil.
    // ANALYTIC calls Earth::Mode3D::EvaluateBackgroundMagneticFieldSI directly.
    std::string mode3dFieldEval{""};

    // -density-parallel <OPENMP|THREADS|SERIAL>
    // Optional shared-memory backend override for Mode3D/GRIDLESS backward products.
    // Empty string means: use default/environment.
    std::string densityParallelBackend{""};

    // -density-threads <int>
    // Number of shared-memory workers per MPI process. 0 means automatic/default.
    int densityThreads{0};

    // -mode3d-mpi-scheduler <DYNAMIC|BLOCK_CYCLIC|STATIC>
    // Inter-rank scheduler for standalone Mode3D and gridless backtracking products.
    // Empty string means: use input-file/default value.
    std::string mode3dMpiScheduler{""};

    // -mode3d-mpi-dynamic-chunk <int>
    // Number of locations/tasks fetched per MPI atomic request in DYNAMIC mode.
    // 0 means: no CLI override / automatic when used from input defaults.
    int mode3dMpiDynamicChunk{0};

    // -cutoff-debug-scan <lon_deg> <lat_deg> <alt_km>
    // Optional Mode3D cutoff diagnostic.  When enabled, rank 0 writes a rigidity
    // classification table for the selected spherical-shell location before the
    // full cutoff calculation starts.  This is useful for dipole/Störmer tests and
    // for diagnosing endpoint/bracketing problems.
    bool cutoffDebugScan{false};
    double cutoffDebugScanLon_deg{0.0};
    double cutoffDebugScanLat_deg{0.0};
    double cutoffDebugScanAlt_km{-1.0};
    int cutoffDebugScanN{0};
    std::string cutoffDebugScanFile{""};

    // -cutoff-debug-exit <lon_deg> <lat_deg> <alt_km>
    // -cutoff-debug-exit-list <file>
    // Optional Mode3D trajectory-exit diagnostic.  It writes the terminal reason,
    // raw exit point, reconstructed boundary-crossing point, rigidity conservation,
    // and dipole canonical-momentum invariant.  The list form traces many
    // lon/lat/alt/R cases in one AMPS run and still produces one output file.
    bool cutoffDebugExit{false};
    double cutoffDebugExitLon_deg{0.0};
    double cutoffDebugExitLat_deg{0.0};
    double cutoffDebugExitAlt_km{-1.0};
    double cutoffDebugExitR_GV{-1.0};
    int cutoffDebugExitN{0};
    std::string cutoffDebugExitListFile{""};
    std::string cutoffDebugExitFile{""};

    // -cutoff-search <UPPER_SCAN|PENUMBRA_SCAN|BINARY>
    // Optional override for #CUTOFF_RIGIDITY / CUTOFF_SEARCH_ALGORITHM.
    // UPPER_SCAN, PENUMBRA_SCAN, and BINARY are normalized and validated by
    // ApplyCommonBackwardCli() in srcEarth/main.cpp. PENUMBRA_SCAN must remain a
    // distinct value because it requests a complete access sequence and an
    // effective-cutoff product, not merely an upper-cutoff transition.
    // This generic option, together with -mode3d-cutoff-search and
    // -gridless-cutoff-search aliases, is applied in both standalone backward modes.
    // Empty means use the input-file/default value.
    std::string cutoffSearchAlgorithm{""};

    // -cutoff-upper-scan-n <int>
    // Optional override for #CUTOFF_RIGIDITY / CUTOFF_UPPER_SCAN_N.
    // Applies to both Mode3D and gridless UPPER_SCAN/PENUMBRA_SCAN searches.
    // 0 means no CLI override.
    int cutoffUpperScanN{0};

    // -cutoff-rigidity-list-gv <comma-separated-list>
    // Raw CLI text is retained here and parsed only when merging CLI overrides into
    // AmpsParam.  Keeping it as text allows ParseCli() to remain independent of the
    // scientific validation rules (positive, strictly increasing rigidity values).
    std::string cutoffRigidityListGV{""};

    // Optional DIRECT_ACCESS adaptive-refinement overrides.  Sentinels preserve the
    // input-deck values when the corresponding CLI option is absent.
    int cutoffDirectAccessAdaptive{-1};       // -1=no override, 0=off, 1=on
    int cutoffDirectAccessAdaptiveMaxDepth{-1};
    int cutoffDirectAccessAdaptiveGuardDepth{-1};

    // Optional Mode3D RIGIDITY_LIST geodetic absolute-latitude band overrides.
    // Negative sentinels mean no CLI override; zero is a valid lower bound.
    double cutoffAccessAbsLatMin_deg{-1.0};
    double cutoffAccessAbsLatMax_deg{-1.0};

    // Optional directional-map coverage override.  Empty means use the input deck.
    // VECTOR_APERTURES is a pure work-selection optimization over the existing regular
    // lon/lat grid; it does not alter the angular resolution of retained cells.
    std::string cutoffDirMapCoverage{""};
    std::string cutoffDirMapApertureFile{""};
    std::vector<std::string> cutoffDirMapApertureSpecs; // repeatable direct CLI specs

    // -cutoff-trace-policy <LEGACY|ACCURATE>
    // Optional override for #CUTOFF_RIGIDITY / CUTOFF_TRACE_POLICY.
    // The default remains LEGACY so existing cutoff regression tests retain
    // their historical numerical trajectory policy.  ACCURATE selects the
    // corrected upper-bound-only timestep and exact boundary-event handling
    // used by the structured F3 tracer.
    std::string cutoffTracePolicy{""};

    // -adaptive-dt T|F
    // Optional CLI override for #NUMERICAL ADAPTIVE_DT.
    // Sentinel convention:
    //   -1 : no CLI override supplied; use input file/default value
    //    0 : fixed-step tracing; DT_TRACE is used directly
    //    1 : adaptive-step tracing; DT_TRACE is the maximum allowed step
    int adaptiveDt{-1};

    // -max-trace-distance <double>
    // Optional CLI override for #NUMERICAL MAX_TRACE_DISTANCE.
    //
    // Units:
    //   Earth radii (Re) of *cumulative* traced path length.
    //
    // Sentinel convention:
    //   < 0   : no CLI override was supplied; use the input file value
    //   = 0   : explicitly disable the cumulative-distance cap
    //   > 0   : enable/override the cap
    double maxTraceDistance_Re{-1.0};

    // Optional CLI override for the standalone Mode3D AMR mesh-resolution profile.
    // These values are expressed in Earth radii for CLI simplicity.  The parser
    // stores the input-file equivalents internally in km, so main.cpp converts the
    // CLI values before merging them into AmpsParam.
    //
    // Sentinel convention for the numeric fields:
    //   < 0 : no CLI override supplied
    //   > 0 : apply/override that value
    double mode3dMeshResEarth_Re{-1.0};
    double mode3dMeshResBoundary_Re{-1.0};
    double mode3dMeshOuterRadius_Re{-1.0};
    std::string mode3dMeshCoarsening{""};
    double mode3dMeshExponent{-1.0};

    // -----------------------------------------------------------------------
    // -mode 3d_forward specific options
    // -----------------------------------------------------------------------

    // -forward-niter <int>
    //   Override Mode3DForwardOptions::nIterations from the input file.
    //   Sentinel: < 0 means no CLI override (use input file value).
    int forward3dNiter{-1};

    // -forward-nparticles <int>
    //   Override Mode3DForwardOptions::nParticlesPerIter from the input file.
    //   Sets the number of simulation particles injected at the domain boundary each
    //   iteration. The physical particle weight is then automatically derived as:
    //     W = (π × ∫J(E)dE × A_boundary × dt) / nParticlesPerIter
    //   Sentinel: < 0 means no CLI override (use input file value, default 1000).
    int forward3dNparticles{-1};

    // -forward-boundary-dist <ISOTROPIC|...>
    //   Override Mode3DForwardOptions::boundaryDistType.
    //   Empty string means: use the input file default (ISOTROPIC).
    std::string forward3dBoundaryDist{""};

    // -forward-injection-energy <SPECTRUM|LOG_UNIFORM>
    //   Select the energy proposal distribution used by the 3d_forward outer-boundary
    //   particle source.
    //     SPECTRUM    : legacy/default; sample E from the physical J(E)dE CDF.
    //     LOG_UNIFORM : sample E uniformly in log(E) and correct each particle with
    //                   an individual statistical-weight factor q(E).
    //   Empty string means: use the input-file/default Mode3DForwardOptions value.
    std::string forward3dInjectionEnergyDistribution{""};

    // -forward-injection-emin <MeV/n>, -forward-injection-emax <MeV/n>
    //   Optional CLI overrides for the 3d_forward particle-energy limits.  These
    //   limits intentionally update the effective #DENSITY_3D range, because in
    //   forward mode that range now controls both the density-output energy grid and
    //   the boundary injection/integration range.
    //   Sentinel: < 0 means no CLI override.
    double forward3dInjectionEmin_MeV{-1.0};
    double forward3dInjectionEmax_MeV{-1.0};
  };

  // Parse argc/argv. Throws std::runtime_error for malformed inputs.
  CliOptions ParseCli(int argc,char** argv);

  // Return formatted help message.
  std::string HelpMessage(const char* progName);

}

#endif
