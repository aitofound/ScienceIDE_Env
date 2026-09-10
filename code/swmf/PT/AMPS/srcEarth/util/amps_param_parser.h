//======================================================================================
// amps_param_parser.h
//======================================================================================
//
// PURPOSE
// -------
// Self-contained, dependency-free parser for the AMPS_PARAM.in format used by
// the CCMC Runs-on-Request interface for the Geospace energetic particle tools.
// Populates EarthUtil::AmpsParam from a text file; used by both the gridless
// cutoff-rigidity solver and the gridless density/spectrum solver.
//
// DESIGN PRINCIPLES
// -----------------
//   (1) No PIC framework dependencies. This parser can be built and tested standalone.
//   (2) Fail-fast validation: unknown sections/keywords terminate execution through
//       exit(__LINE__,__FILE__,msg) instead of being silently ignored.
//   (3) New sections and keys must be added explicitly to the parser before they can
//       appear in production input files.
//   (4) Clear unit contract: all geometric lengths are expected in km from the caller;
//       the parser documents this but does not enforce conversions (the solvers do).
//
//======================================================================================
// INPUT FILE FORMAT
//======================================================================================
//
// The file is a sequence of named sections. Each section starts with a keyword line
// beginning with '#'. Lines beginning with '!' outside a section header are comments.
// Blank lines are ignored. Within a section, each non-blank, non-comment line is:
//
//   KEY   VALUE   [! optional comment]
//
// The following sections are recognised. Any unrecognised section or keyword is
// treated as a fatal input-file error because it may indicate a typo or an option
// that has not been connected to the model.
//
//   #RUN_INFO
//     RUN_ID      <string>      ! arbitrary run identifier, stored in AmpsParam.runId
//
//   #CALCULATION_MODE
//     CALC_TARGET             CUTOFF_RIGIDITY | DENSITY_SPECTRUM
//     FIELD_EVAL_METHOD       GRIDLESS | GRID_3D
//
//   #CUTOFF_RIGIDITY
//     CUTOFF_EMIN             <double>   ! MeV; lower rigidity scan bound
//     CUTOFF_EMAX             <double>   ! MeV; upper rigidity scan bound
//     CUTOFF_NENERGY          <int>      ! number of rigidity bisection points
//     CUTOFF_MAX_PARTICLES    <int>      ! per-point trajectory cap (optional)
//     CUTOFF_MAX_TRAJ_TIME    <double>   ! per-trajectory time cap [s] (optional)
//     CUTOFF_SAMPLING         VERTICAL | ISOTROPIC
//     DIRECTIONAL_MAP         T|F        ! enable directional cutoff sky-map output
//     DIRMAP_LON_RES          <double>   ! longitude resolution [deg] for sky-map
//     DIRMAP_LAT_RES          <double>   ! latitude resolution [deg] for sky-map
//     DIRMAP_COVERAGE         <string>   ! FULL_SPHERE or VECTOR_APERTURES
//     DIRMAP_APERTURE_FILE    <path>     ! optional arbitrary instrument-aperture vector list
//     DIRMAP_APERTURE         <spec>     ! repeatable inline aperture definition (see below)
//     CUTOFF_SEARCH_ALGORITHM <string>   ! UPPER_SCAN, PENUMBRA_SCAN, RIGIDITY_LIST, or BINARY
//     CUTOFF_UPPER_SCAN_N     <int>      ! samples for UPPER/PENUMBRA scan; 0 => CUTOFF_NENERGY
//     CUTOFF_RIGIDITY_LIST_GV <list>     ! comma/space-separated positive GV values for RIGIDITY_LIST
//     CUTOFF_ACCESS_ABS_LAT_MIN <double>  ! geodetic |latitude| lower bound [deg] for RIGIDITY_LIST
//     CUTOFF_ACCESS_ABS_LAT_MAX <double>  ! geodetic |latitude| upper bound [deg] for RIGIDITY_LIST
//     CUTOFF_SCAN_SPACING     LOG | LINEAR ! rigidity-node spacing; default LOG
//     CUTOFF_DEBUG_RIGIDITY_SCAN T|F     ! write one-point allowed(R) scan
//     CUTOFF_DEBUG_SCAN_*               ! lon/lat/alt/N/file controls for that scan
//     CUTOFF_DEBUG_EXIT_TRACE T|F        ! write trajectory-exit diagnostic
//     CUTOFF_DEBUG_EXIT_*               ! lon/lat/alt/R/N/list/file controls for exit test
//
//   #DENSITY_SPECTRUM
//     DS_EMIN                 <double>   ! MeV/n; lower energy bound
//     DS_EMAX                 <double>   ! MeV/n; upper energy bound
//     DS_NINTERVALS           <int>      ! number of energy intervals (nPoints = +1)
//     DS_ENERGY_SPACING       LOG | LINEAR
//     DS_MAX_PARTICLES        <int>      ! total trajectory cap per obs. point (optional)
//     DS_MAX_TRAJ_TIME        <double>   ! per-trajectory time cap [s] (optional)
//     DS_BOUNDARY_MODE        ISOTROPIC | ANISOTROPIC
//       ISOTROPIC (default): T(E;x0) = N_allowed/N_dirs, uniform boundary spectrum
//       ANISOTROPIC:         T_aniso(E;x0) = (1/N_dirs)*sum_k A_k*f_PAD_k*f_spatial_k
//                            requires #BOUNDARY_ANISOTROPY section
//     DS_TRANSMISSION_MODE    DIRECT | SCAN | ADAPTIVE
//       DIRECT:              use the legacy user energy grid for T(E)
//       SCAN/ADAPTIVE:       use a log-spaced rigidity grid converted to energy
//     DS_TRANSMISSION_SCAN_N  <int>  number of scan nodes; 0 => DS_NINTERVALS+1
//     DS_TRANSMISSION_REFINE_N / MAX_N / SAVE: parsed diagnostic/reserved controls
//     DS_UNRESOLVED_TOL       <double>   ! maximum accepted unresolved fraction [0,1]
//     DS_RETRY_UNRESOLVED     T|F        ! retry unresolved trajectories once
//     DS_SAVE_TERMINATION_SUMMARY T|F    ! write per-point/energy termination counts
//
//   #NUMERICAL trapped-orbit controls (static fields only; disabled by default)
//     TRAP_DETECTION T|F
//     TRAP_MIN_MIRROR_POINTS <int>
//     TRAP_MIN_BOUNCES <int>
//     TRAP_OUTER_MARGIN_RE <double>
//     TRAP_RADIAL_GROWTH_TOL_RE <double>
//     TRAP_ENERGY_REL_TOL <double>
//     TRAP_PARALLEL_DEADBAND <double>
//     TRAP_DRIFT_DETECTION        <T/F>      ! enable frozen-field full-orbit drift recurrence
//     TRAP_MIN_DRIFT_REVOLUTIONS  <int>      ! completed turns required (>=2; C19 uses 3)
//     TRAP_DRIFT_RADIAL_GROWTH_TOL_RE <double> ! absolute profile-bin radius tolerance [Re]
//     TRAP_DRIFT_RADIAL_REL_TOL   <double>   ! relative profile-bin radius tolerance
//     TRAP_DRIFT_LATITUDE_TOL     <double>   ! recurrence tolerance in z/r
//     TRAP_DRIFT_PITCH_COS2_TOL   <double>   ! recurrence tolerance in cos^2(pitch)
//     TRAP_DRIFT_PROFILE_BINS     <int>      ! azimuth bins per full drift revolution
//     TRAP_DRIFT_MIN_PROFILE_COVERAGE <double> ! populated-bin fraction in (0,1]
//     TRAP_DRIFT_MIN_MATCHED_BIN_FRACTION <double> ! recurring common-bin fraction in (0,1]
//
//   #PARTICLE_TRAJECTORY    (optional; used by -mode 3d_forward)
//     INITIALIZE_TRAJECTORIES T|F        ! runtime gate for AMPS trajectory records
//     N_TRAJECTORIES          <int>      ! maximum injected-particle trajectories
//                                        ! >0 also enables INITIALIZE_TRAJECTORIES
//
//   #BOUNDARY_ANISOTROPY     (required when DS_BOUNDARY_MODE = ANISOTROPIC)
//     BA_PAD_MODEL            ISOTROPIC | SINALPHA_N | COSALPHA_N | BIDIRECTIONAL
//     BA_PAD_EXPONENT         <double>   ! n in sin^n or |cos|^n  (default 2.0)
//     BA_SPATIAL_MODEL        UNIFORM | DAYSIDE_NIGHTSIDE
//     BA_DAYSIDE_FACTOR       <double>   ! flux multiplier for GSM x > 0  (default 1.0)
//     BA_NIGHTSIDE_FACTOR     <double>   ! flux multiplier for GSM x <= 0 (default 1.0)
//
//   #PARTICLE_SPECIES
//     SPECIES_NAME            <string>   ! e.g. PROTON, ELECTRON, HE4
//     SPECIES_CHARGE          <int>      ! charge in units of e (sign matters)
//     SPECIES_MASS_AMU        <double>   ! mass in atomic mass units
//
//   #BACKGROUND_FIELD
//     FIELD_MODEL             IGRF | T96 | T01 | T05 | TA15N | TA15B | TA16 | DIPOLE
//     EPOCH                   <UTC datetime>   ! recommended: 2010-01-01T00:00:00
//                                              ! initializes Geopack/IGRF, Tsyganenko
//                                              ! dipole tilt, and frame rotations;
//                                              ! CLI --epoch overrides this value
//     DST                     <double>   ! nT
//     PDYN                    <double>   ! nPa
//     IMF_BY                  <double>   ! nT (GSM Y component of IMF)
//     IMF_BZ                  <double>   ! nT (GSM Z component of IMF)
//     IMF_BX                  <double>   ! nT (reserved; not used by T96/T05)
//     SW_VX                   <double>   ! km/s solar wind x velocity (T05 input)
//     SW_N                    <double>   ! cm^-3 solar wind number density (T05 input)
//     T05_W1 .. T05_W6        <double>   ! T05 storm-time history integrals W1..W6
//     DIPOLE_MOMENT           <double>   ! multiple of Earth dipole moment M_E (DIPOLE model)
//     DIPOLE_TILT             <double>   ! tilt from +Z_GSM toward +X_GSM [deg] (DIPOLE model)
//
//   #DOMAIN_BOUNDARY
//     DOMAIN_XMIN             <double>   ! km (GSM)
//     DOMAIN_XMAX             <double>   ! km
//     DOMAIN_YMIN             <double>   ! km
//     DOMAIN_YMAX             <double>   ! km
//     DOMAIN_ZMIN             <double>   ! km
//     DOMAIN_ZMAX             <double>   ! km
//     R_INNER                 <double>   ! km inner loss sphere radius
//
//   #OUTPUT_DOMAIN
//     OUTPUT_MODE             POINTS | SHELLS
//     COORDS                  GSM | GEO | GSE      ! coordinate label (not transformed)
//     SHELL_ALTITUDES         <double> [<double> ...] ! km above Earth's surface; space-separated
//     SHELL_RES               <double>   ! common angular resolution [deg]
//     SHELL_LON_RES_DEG       <double>   ! optional longitude-specific resolution
//     SHELL_LAT_RES_DEG       <double>   ! optional latitude-specific resolution
//     SHELL_GEOMETRY          SPHERICAL | GEODETIC ! default SPHERICAL
//     POINTS_BEGIN
//       x1 y1 z1
//       x2 y2 z2
//       ...
//     POINTS_END
//     (coordinates in km, GSM by default)
//
//   #NUMERICAL
//     DT_TRACE                <double>   ! trace time step [s]; fixed step when ADAPTIVE_DT=F,
//                                        ! maximum step when ADAPTIVE_DT=T
//     ADAPTIVE_DT             T|F        ! T/default: gyro-limited step plus exact
//                                        ! boundary-event detection; F: fixed DT_TRACE
//                                        ! except for final time-limit trim
//     MAX_STEPS               <int>      ! hard cap on integration steps
//     MAX_TRACE_TIME          <double>   ! hard cap on integration time [s]
//     MAX_TRACE_DISTANCE      <double>   ! hard cap on cumulative trace distance [Re]
//                                        ! 0 or negative => disabled
//     BOUNDARY_EVENT_TOL_M    <double>   ! chord/surface crossing tolerance [m]
//     BOUNDARY_EVENT_MAX_ITER <int>      ! reserved curved-step refinement control
//     MODE3D_PARALLEL         OPENMP | THREADS | SERIAL
//     MODE3D_THREADS          <int>      ! workers per MPI rank; 0 => automatic
//     MODE3D_MPI_SCHEDULER    DYNAMIC | BLOCK_CYCLIC | STATIC
//     MODE3D_MPI_DYNAMIC_CHUNK <int>     ! work items per dynamic MPI chunk; cutoff=trajectory tasks, density=locations; 0 => auto
//
//   #MODE3D_MESH                  (optional; only affects -mode 3d AMR mesh build)
//     MODE3D_MESH_RES_EARTH_RE     <double>   ! requested AMR resolution near 1 Re, in Re
//     MODE3D_MESH_RES_BOUNDARY_RE  <double>   ! requested AMR resolution at outer boundary, in Re
//     MODE3D_MESH_COARSENING       LINEAR | LOG | EXPONENTIAL | POWER | CONSTANT
//     MODE3D_MESH_EXPONENT         <double>   ! POWER/EXPONENT shape parameter; default 1
//     MODE3D_MESH_R_BOUNDARY_RE    <double>   ! optional outer-radius override; default: domain max extent
//
//   #SPECTRUM
//     (stored both as a raw key/value map in AmpsParam.spectrum and as a
//      typed metadata view in AmpsParam.particleSpectrum; the downstream
//      evaluator still uses boundary/spectrum.h InitGlobalSpectrumFromKeyValueMap)
//
//   #ENERGY_CHANNELS    (optional; if present, enables per-channel integral flux output)
//     CH_BEGIN
//       NAME   E1_MeV   E2_MeV   [! optional comment]
//       ...
//     CH_END
//     Example:
//       CH_BEGIN
//         CH1   10.0   100.0    ! 10–100 MeV
//         CH2  100.0  1000.0    ! 100 MeV–1 GeV
//         CH3 1000.0 10000.0    ! 1–10 GeV
//         CH4 10000.0 20000.0   ! 10–20 GeV
//       CH_END
//     Channels may overlap and may extend outside [DS_EMIN, DS_EMAX].
//     If this section is absent, only the total integral flux is computed.
//
//======================================================================================
// TYPE CONVERSION RULES
//======================================================================================
//
// Booleans:     T / TRUE / 1  -> true     F / FALSE / 0 -> false   (case-insensitive)
// Integers:     std::stoi     (raises std::invalid_argument on failure)
// Doubles:      std::stod     (raises std::invalid_argument on failure)
// Strings:      trimmed of leading/trailing whitespace; comment text after '!' removed
// Enumerations: stored as uppercase strings; validated lazily by the consumer
//
//======================================================================================
// UNITS CONTRACT
//======================================================================================
//
// The parser stores distances in whatever unit the file uses (km for Runs-on-Request
// inputs; Re for some legacy files). The dominant convention used in production is km.
// The solvers convert km -> m internally.
//
// IMPORTANT: The parser does NOT transform coordinates between GSM, GEO, and GSE.
// The COORDS keyword is stored as a label only. The gridless solver assumes all
// positions are in GSM and will produce incorrect results if non-GSM coordinates are
// passed without external conversion.
//
//======================================================================================
// ERROR HANDLING
//======================================================================================
//
// ParseAmpsParamFile terminates through exit(__LINE__,__FILE__,msg) for:
//   - File not found / cannot open
//   - Unknown section names
//   - Unknown key names within a recognised section
//   - Malformed numeric values for recognised keys
//   - Invalid physical or structural settings discovered during post-parse validation
//
// Missing optional sections are still allowed; defaults in structs are used.
//
// Post-parse validation (e.g., checking that #BOUNDARY_ANISOTROPY is present when
// DS_BOUNDARY_MODE = ANISOTROPIC) is performed in amps_param_parser.cpp after the
// parse loop, and also at solver startup in DensityGridless.cpp.
//
//======================================================================================

#ifndef _SRC_EARTH_UTIL_AMPS_PARAM_PARSER_H_
#define _SRC_EARTH_UTIL_AMPS_PARAM_PARSER_H_

#include <string>
#include <vector>
#include <map>
#include <stdexcept>

namespace EarthUtil {

  // Simple 3-component container used throughout.
  struct Vec3 {
    double x{0.0}, y{0.0}, z{0.0};
  };

  //====================================================================================
  // ParticleSpectrum / SpectrumTablePoint
  //====================================================================================
  // A typed representation of the #SPECTRUM block.  The raw key/value table is kept
  // as well, but this structure makes the most important spectrum choices available
  // during initialization without requiring downstream code to reinterpret strings.
  //
  // Design goal:
  //   - keep the parser independent from the actual injection implementation;
  //   - validate the commonly used POWER_LAW inputs early;
  //   - preserve all original raw keys for forward compatibility.
  struct SpectrumTablePoint {
    double energy_MeV{0.0};
    double intensity{0.0};
  };

  struct ParticleSpectrum {
    enum class Type { UNKNOWN, POWER_LAW, POWER_LAW_CUTOFF, LIS_FORCE_FIELD, BAND, TABLE };
    enum class TableFormat { UNKNOWN, TWO_COLUMN, TIME_DEPENDENT };

    Type type{Type::UNKNOWN};
    std::string typeName{"UNKNOWN"};
    bool parsed{false};
    TableFormat tableFormat{TableFormat::UNKNOWN};

    // Common power-law style parameters.  These are populated when present and are
    // required for POWER_LAW.  For other spectrum types they remain optional metadata
    // unless a downstream module chooses to use them.
    double J0{0.0};
    double gamma{0.0};
    double E0_MeV{0.0};
    double Emin_MeV{0.0};
    double Emax_MeV{0.0};

    // Optional extra parameters for extended spectrum forms.  They are intentionally
    // generic because the exact downstream evaluator lives outside this parser.
    double cutoffEnergy_MeV{0.0};
    double modulationPotential_MV{0.0};
    double alpha{0.0};
    double beta{0.0};
    double breakEnergy_MeV{0.0};
    std::string tableFile;
    std::string resolvedTableFile;
    std::string selectedSnapshotEpochUTC;
    double selectedSnapshotTimeOffset_s{0.0};
    std::vector<SpectrumTablePoint> tablePoints;

    std::map<std::string,std::string> raw;
  };

  struct DomainBox {
    // NOTE ON UNITS
    //   For Runs-on-Request inputs we treat *all geometric distances* as being
    //   provided in **kilometers** (km):
    //     - DOMAIN_* bounds
    //     - R_INNER
    //     - POINT coordinates in POINTS_BEGIN..END
    //   The gridless cutoff solver converts km -> meters -> Re internally.
    double xMin{-60.0}, xMax{15.0};
    double yMin{-25.0}, yMax{25.0};
    double zMin{-20.0}, zMax{20.0};
    double rInner{2.0}; // [km] inner loss sphere radius

    // Runs-on-Request / CCMC-style inputs may describe the outer boundary with
    // magnetopause-oriented keywords such as BOUNDARY_TYPE=SHUE, SHUE_R0=AUTO,
    // and SHUE_ALPHA=AUTO.  The standalone Mode3D code in this source tree still
    // uses the rectangular/capped domain above for particle classification, but
    // keeping these tokens in the parsed configuration lets the strict parser
    // accept RoR files without silently discarding what the user requested.
    // A future Shue-boundary implementation can consume the stored values here.
    std::string boundaryType{"BOX"};
    std::string shueR0Token;
    std::string shueAlphaToken;

    // Extra boundary metadata recognized for forward compatibility.  This is not
    // the global unknown-key sink: keys placed here are explicitly accepted
    // compatibility options.
    std::map<std::string,std::string> raw;
  };

  //====================================================================================
  // DirectionalAperture
  //====================================================================================
  // Generic finite-field-of-view definition used to prune expensive directional
  // cutoff/access work.  Nothing in this structure assumes EAST/WEST, GOES, or even
  // antipodal telescopes: every aperture carries its own physical LOOK boresight and
  // roll-reference vector.
  //
  // Supported frames:
  //   SM       - vectors are Cartesian components in the global SM frame used to
  //              label the directional map.
  //   GSM      - vectors are Cartesian components in the GSM tracing frame; the
  //              solver rotates them into the map-label frame at the run epoch.
  //   LOCAL_SM - vectors are components in a location-dependent orthonormal basis
  //              (radial, local-east, local-north) constructed from the observation
  //              position and SM +Z.  This is useful for simple proxy/engineering
  //              configurations while keeping the selector itself instrument-neutral.
  //
  // The aperture is elliptical in angular tangent-plane coordinates.  ``up`` defines
  // the vertical/roll direction and need not be exactly perpendicular to boresight;
  // the solver projects and normalizes it before use.  The horizontal axis is derived
  // from the orthonormalized (up,boresight) pair.  Half-angles are in degrees.
  //
  // Input-file inline syntax (repeatable):
  //   DIRMAP_APERTURE  <name> <SM|GSM|LOCAL_SM> bx by bz ux uy uz hHalfDeg vHalfDeg [LOCATION=<index>]
  //
  // File syntax used by DIRMAP_APERTURE_FILE is identical but omits the keyword:
  //   <name> <frame> bx by bz ux uy uz hHalfDeg vHalfDeg [LOCATION=<index>]
  struct DirectionalAperture {
    std::string name;
    std::string frame{"SM"};
    Vec3 boresight{1.0,0.0,0.0};
    Vec3 up{0.0,0.0,1.0};
    double horizontalHalfAngle_deg{30.0};
    double verticalHalfAngle_deg{60.0};

    // Optional zero-based location selector used by batched point/trajectory runs.
    // A negative value preserves the historical meaning: this aperture is available
    // at every output location.  LOCATION=<index> is deliberately an optional final
    // token in the input syntax, so all existing aperture files remain valid.
    //
    // The selector affects work pruning only; it never changes the physical detector
    // fold.  Mode3D remaps a global trajectory index to the snapshot-local index when
    // an explicit snapshot list filters a multi-location trajectory by epoch.
    int locationIndex{-1};
  };

  struct CutoffScan {
    double eMin_MeV{1.0};
    double eMax_MeV{1000.0};
    int nEnergy{50};
    int maxParticlesPerPoint{500};

    // Optional cap on how long we integrate a *single* backtraced trajectory
    // during the cutoff scan.
    //
    // Why this exists:
    //   Cutoff rigidity searches can spend a lot of time on quasi-trapped
    //   or long-lived trajectories. For interactive/production runs you may
    //   want a tighter limit than the global #NUMERICAL MAX_TRACE_TIME.
    //
    // Semantics (kept consistent with other sections like #DENSITY_SPECTRUM):
    //   - If maxTrajTime_s > 0: use it as the cutoff-scan trace time cap [s].
    //   - If maxTrajTime_s <= 0 or omitted: fall back to #NUMERICAL
    //     MAX_TRACE_TIME.
    double maxTrajTime_s{0.0}; // CUTOFF_MAX_TRAJ_TIME

    // Optional unresolved-only trace-budget extension used by strict three-state
    // cutoff products such as C19 DIRECT_ACCESS.  The primary trace always uses
    // CUTOFF_MAX_TRAJ_TIME (or the global MAX_TRACE_TIME fallback).  Only a
    // trajectory that still terminates at TIME_LIMIT or STEP_LIMIT is retraced with
    // a larger total-time budget.  Physical ALLOWED/FORBIDDEN trajectories are never
    // repeated, so the extension cannot change already resolved samples and its CPU
    // cost is proportional to the unresolved population rather than the whole test.
    //
    // The present implementation intentionally RESTARTS the unresolved trajectory
    // from its original phase-space seed for each larger budget.  Restarting is more
    // expensive than serializing the private mover/trap-detector state at the end of
    // the first pass, but it guarantees that every extended result is numerically
    // identical to an ordinary single trace run directly with the larger time limit.
    // This makes the convergence experiment auditable and avoids a hidden continuation
    // state that could differ between GRIDLESS and Mode3D.
    //
    // Example used by C19:
    //   CUTOFF_MAX_TRAJ_TIME                 300
    //   CUTOFF_UNRESOLVED_EXTENSION_PASSES   2
    //   CUTOFF_UNRESOLVED_EXTENSION_FACTOR   2
    // gives 300 s -> 600 s -> 1200 s, but only for samples unresolved at the
    // preceding budget.  DISTANCE_LIMIT is deliberately not extended: a path cap is
    // a separately configured safety policy and should be fixed explicitly rather
    // than silently relaxed by this time-convergence mechanism.
    int unresolvedExtensionPasses{0};       // CUTOFF_UNRESOLVED_EXTENSION_PASSES
    double unresolvedExtensionFactor{2.0};  // CUTOFF_UNRESOLVED_EXTENSION_FACTOR

    // Rigidity-search algorithm for each point/direction.
    //
    // UPPER_SCAN (default): penumbra-safe search.  Sample TraceAllowed(R) on a
    // log-spaced grid, find the highest forbidden sampled rigidity, then refine
    // the final forbidden/allowed transition.  This avoids returning Rmin when
    // low-rigidity allowed pockets exist below the physical upper cutoff.
    //
    // PENUMBRA_SCAN: evaluate one complete rigidity grid, retain explicit
    // ALLOWED / PHYSICAL_FORBIDDEN / UNRESOLVED states, and extract both the
    // lower and upper cutoff from that one scan.  This mode is intended for C14
    // and currently requires VERTICAL sampling so the reported band belongs to
    // one unambiguous arrival direction.
    //
    // BINARY: legacy endpoint-only binary search.  This is faster but assumes
    // TraceAllowed(R) is monotonic and can return Rmin if Rmin happens to be
    // allowed inside a penumbra-like allowed pocket.
    std::string searchAlgorithm{"UPPER_SCAN"}; // CUTOFF_SEARCH_ALGORITHM

    // Number of samples for UPPER_SCAN or PENUMBRA_SCAN.  If <=0, the solver reuses
    // CUTOFF_NENERGY so existing inputs control the scan resolution.
    int upperScanN{0}; // CUTOFF_UPPER_SCAN_N

    // Explicit fixed-rigidity nodes used by access-state validation products.
    //
    // RIGIDITY_LIST traces only these values at the selected latitude-band nodes.
    // PENUMBRA_SCAN may also carry a non-empty list; in that case the complete scan is
    // unchanged and the exact listed access states are written as a companion product.
    // C9 uses that companion file so FULL_SCAN and DIRECT_ACCESS are reduced through
    // the same PAMELA_T50 postprocessor rather than compared with different observables.
    //
    // Input syntax accepts commas and/or whitespace, for example:
    //
    //   CUTOFF_RIGIDITY_LIST_GV  0.424, 0.498, 0.588, 0.693
    //
    // Post-parse validation requires positive, strictly increasing values so the
    // output order is deterministic and accidental duplicate trajectories are caught.
    std::vector<double> rigidityList_GV; // CUTOFF_RIGIDITY_LIST_GV

    // Optional adaptive refinement of the DIRECT_ACCESS rigidity list.
    //
    // The explicit CUTOFF_RIGIDITY_LIST_GV values become the coarse seed grid.  Every
    // seed is traced in every selected sky direction.  The solver then probes geometric
    // midpoints and recursively refines intervals whose endpoint classifications
    // differ (including resolved<->UNRESOLVED boundaries).  UNRESOLVED/UNRESOLVED
    // intervals stop after the guard probes because rigidity subdivision cannot repair
    // a trace time/step/path limit.  A small guard depth forces midpoint
    // probes even when coarse endpoints agree, reducing the chance of missing a narrow
    // penumbral pocket.  This changes only DIRECT_ACCESS; PENUMBRA_SCAN and the shell
    // RIGIDITY_LIST product retain their established fixed-grid behavior.
    //
    // Defaults are deliberately conservative for general AMPS use: adaptive sampling is
    // OFF unless requested.  C19 enables it explicitly in its committed input templates.
    bool directAccessAdaptive{false}; // CUTOFF_DIRECT_ACCESS_ADAPTIVE
    int directAccessAdaptiveMaxDepth{6}; // CUTOFF_DIRECT_ACCESS_ADAPTIVE_MAX_DEPTH
    int directAccessAdaptiveGuardDepth{1}; // CUTOFF_DIRECT_ACCESS_ADAPTIVE_GUARD_DEPTH

    // Optional geodetic absolute-latitude band used only by RIGIDITY_LIST.
    //
    // A full shell remains the backward-compatible default (0--90 degrees).  A test
    // that knows its access boundary is confined to mid/high latitudes can reduce work
    // substantially by selecting, for example, 35 <= |lat_geodetic| <= 75.  Both
    // hemispheres and every configured longitude are retained.  These limits select
    // trajectory launch locations; AACGM conversion and boundary extraction remain a
    // postprocessing responsibility.
    double accessAbsLatMin_deg{0.0};  // CUTOFF_ACCESS_ABS_LAT_MIN
    double accessAbsLatMax_deg{90.0}; // CUTOFF_ACCESS_ABS_LAT_MAX

    // Spacing of the rigidity vertices used by UPPER_SCAN/PENUMBRA_SCAN.
    //
    // LOG (default) preserves the historical behavior and is efficient when one scan
    // must cover several decades in rigidity.  LINEAR is required by validation data
    // sets whose published effective cutoff was computed by summing equal-width
    // rigidity bins (for example the Smart--Shea/CARI/Gerontidou world grids).
    // The default remains LOG so all existing tests and production input files retain
    // exactly their previous sampling grid.
    std::string scanSpacing{"LOG"}; // CUTOFF_SCAN_SPACING: LOG|LINEAR

    // Numerical integration policy used by the Boolean cutoff classifier.
    //
    // LEGACY (default): preserve the historical C-series cutoff behavior,
    // including the boundary-distance limiter and the 100-km minimum travel
    // distance per full-orbit step.  This is the backward-compatible default
    // for existing C1/C2/C3/C11 references.
    //
    // ACCURATE: use the corrected F3 integration policy.  DT_TRACE and the
    // gyro-angle condition are strict upper bounds, no minimum displacement is
    // imposed, and segment/boundary intersections are classified explicitly.
    // Time/step/distance limits are still interpreted as FORBIDDEN by the
    // Boolean cutoff API; only the numerical integration policy changes.
    std::string traceIntegrationPolicy{"LEGACY"}; // CUTOFF_TRACE_POLICY

    // Charge sign used by the numerically forward-integrated reverse trajectory.
    //
    // SAME (backward-compatible default):
    //   Use the species charge exactly as configured in #PARTICLE_SPECIES while
    //   launching the velocity opposite to the arrival direction.  This preserves
    //   historical AMPS cutoff results, but it is not the conventional antiparticle
    //   backtracing construction used by Smart--Shea/CARI trajectory tables.
    //
    // REVERSED:
    //   Negate the species charge for the outward reverse trajectory.  A positive
    //   incoming cosmic-ray trajectory is then reconstructed by launching a negative
    //   particle outward from the observation point, as required by time reversal of
    //   the Lorentz equation.  Rigidity-to-momentum conversion still uses |q|, so only
    //   the curvature orientation changes.
    std::string backtraceChargeConvention{"SAME"}; // CUTOFF_BACKTRACE_CHARGE

    // Classification of a trajectory that reaches a configured time, step, or
    // cumulative-distance safety cap before hitting either physical boundary.
    //
    // UNRESOLVED (backward-compatible PENUMBRA_SCAN default):
    //   Preserve the third state explicitly.  This is the conservative choice for
    //   strict numerical tests such as C14 because a safety cap is not itself a
    //   physical proof that the trajectory is forbidden.
    //
    // FORBIDDEN:
    //   Interpret a non-escaping trajectory at the configured cap as forbidden.
    //   This matches the traditional finite-trajectory cutoff-table convention and
    //   is required for direct Smart--Shea/CARI/Gerontidou effective-cutoff
    //   comparisons.  Genuine invalid-field/time-step/numerical failures remain
    //   errors and are never converted to forbidden.
    std::string traceLimitPolicy{"UNRESOLVED"}; // CUTOFF_TRACE_LIMIT_POLICY

    // Cutoff sampling mode.
    //
    // VERTICAL:
    //   Compute cutoff using only the local "vertical" arrival direction.
    //   In our convention, this is the direction pointing *toward Earth*:
    //     d_vertical = - unit(r0)
    //   where r0 is the GSM position vector of the injection point.
    //
    // ISOTROPIC:
    //   Compute an isotropic cutoff by scanning many arrival directions
    //   (a pre-defined direction grid) and taking the minimum Rc.
    //
    // IMPORTANT NOTE ABOUT DEFINITIONS:
    //   "Isotropic" in this implementation means "min over sampled sky",
    //   not "effective cutoff" based on allowed fraction / penumbra.
    std::string sampling{"ISOTROPIC"}; // CUTOFF_SAMPLING

    // Optional: compute a directional cutoff rigidity "sky-map" for each
    // injection point.
    //
    // When enabled, the solver evaluates Rc as a function of arrival direction
    // on a regular global SM lon/lat grid (GSM fallback when the SM->GSM transform
    // is unavailable). DIRMAP_COVERAGE may retain the complete grid or only cells
    // inside configured instrument look apertures.  In multi-location Mode3D batches,
    // LOCATION-qualified apertures remain local to their associated observation.
    //
    // The intent is to provide a diagnostic product to visualize the
    // directional dependence (penumbra-like structure) and to support
    // scientific interpretation.
    bool directionalMap{false};          // DIRECTIONAL_MAP
    double dirMapLonRes_deg{10.0};       // DIRMAP_LON_RES
    double dirMapLatRes_deg{10.0};       // DIRMAP_LAT_RES

    // Directional-map angular coverage.
    //
    // FULL_SPHERE preserves the historical complete regular lon/lat sky grid.
    // VECTOR_APERTURES keeps the same underlying grid and exact trajectory directions
    // but schedules only cells whose detector LOOK vector lies inside at least one
    // user-defined DirectionalAperture.  Apertures may point anywhere, need not be
    // antipodal, and may be supplied in SM, GSM, or the location-dependent LOCAL_SM
    // basis.  A LOCATION-qualified aperture is scheduled only at that location; an
    // unqualified aperture applies everywhere.  This is strictly a work-selection
    // optimization: retained cells are bit-for-bit the same cells as FULL_SPHERE.
    std::string dirMapCoverage{"FULL_SPHERE"}; // DIRMAP_COVERAGE
    std::string dirMapApertureFile;             // DIRMAP_APERTURE_FILE
    std::vector<DirectionalAperture> dirMapApertures; // repeated DIRMAP_APERTURE

    // Optional diagnostic: run a rigidity classification scan at one user-selected
    // spherical-shell location before the full cutoff map is computed.  This is a
    // debugging/regression aid for centered-dipole validation: the output shows
    // TraceAllowed3D(R) over a range of rigidities, next to the analytic Störmer
    // vertical cutoff.  It helps distinguish a true trajectory-classification problem
    // from MPI/thread output-indexing errors.
    //
    // Input-file keys in #CUTOFF_RIGIDITY:
    //   CUTOFF_DEBUG_RIGIDITY_SCAN  T|F
    //   CUTOFF_DEBUG_SCAN_LON       <deg>
    //   CUTOFF_DEBUG_SCAN_LAT       <deg>
    //   CUTOFF_DEBUG_SCAN_ALT       <km>   (if <0, use first SHELL_ALTITUDES value or 0)
    //   CUTOFF_DEBUG_SCAN_N         <int>  (number of log-spaced points, landmarks added)
    //   CUTOFF_DEBUG_SCAN_FILE      <file>
    bool debugRigidityScan{false};
    double debugScanLon_deg{0.0};
    double debugScanLat_deg{0.0};
    double debugScanAlt_km{-1.0};
    int debugScanN{40};
    std::string debugScanFile{"cutoff_3d_debug_rigidity_scan.dat"};

    // Optional diagnostic: repeat the trajectory classifier at one selected
    // spherical-shell location and write the terminal/exit state.  This is designed
    // for dipole validation of the OUTER_BOX classification: it records the reason
    // for termination, the raw exit point, the linearly reconstructed box-crossing
    // point, rigidity conservation, and the dipole-axis canonical angular-momentum
    // invariant.
    //
    // If CUTOFF_DEBUG_EXIT_R_GV > 0, only that one rigidity is traced.
    // If CUTOFF_DEBUG_EXIT_R_GV <= 0, the diagnostic traces a rigidity list built
    // similarly to CUTOFF_DEBUG_RIGIDITY_SCAN, with CUTOFF_DEBUG_EXIT_N samples.
    //
    // Input-file keys in #CUTOFF_RIGIDITY:
    //   CUTOFF_DEBUG_EXIT_TRACE      T|F
    //   CUTOFF_DEBUG_EXIT_LON        <deg>
    //   CUTOFF_DEBUG_EXIT_LAT        <deg>
    //   CUTOFF_DEBUG_EXIT_ALT        <km>
    //   CUTOFF_DEBUG_EXIT_R_GV       <GV>   (optional; <=0 means scan/list mode)
    //   CUTOFF_DEBUG_EXIT_N          <int>  (used only when R_GV<=0)
    //   CUTOFF_DEBUG_EXIT_LIST_FILE  <file> (optional many-trajectory list)
    //   CUTOFF_DEBUG_EXIT_FILE       <file> (single combined output file)
    //
    // If CUTOFF_DEBUG_EXIT_LIST_FILE is set, the listed trajectories replace the
    // single LON/LAT/ALT/R selection.  Each non-comment line in the list must be:
    //
    //   lon_deg lat_deg alt_km R_GV [label]
    //
    // The diagnostic is written by rank 0 before the normal Mode3D work scheduler,
    // so the output is one deterministic file even when AMPS is launched with many
    // MPI ranks and/or threads.
    bool debugExitTrace{false};
    double debugExitLon_deg{0.0};
    double debugExitLat_deg{0.0};
    double debugExitAlt_km{-1.0};
    double debugExitR_GV{-1.0};
    int debugExitN{40};
    std::string debugExitListFile{""};
    std::string debugExitFile{"cutoff_3d_debug_exit_trace.dat"};
  };

  //====================================================================================
  // Density + spectrum sampling controls (gridless)
  //====================================================================================
  // The gridless density/spectrum workflow (CALC_TARGET = DENSITY_SPECTRUM) uses a
  // dedicated input section:
  //   #DENSITY_SPECTRUM
  //     DS_EMIN           <min energy>      ! MeV/n
  //     DS_EMAX           <max energy>      ! MeV/n
  //     DS_NINTERVALS     <n>               ! number of energy *intervals*
  //     DS_ENERGY_SPACING LOG|LINEAR
  //
  // Notes:
  // - DS_NINTERVALS is stored as "intervals" (not points) because this is the
  //   most robust way to define a grid: Npoints = Nintervals + 1.
  // - Energies are kinetic energy. For PROTON this is MeV per particle.
  //   For ions, MeV/n is commonly used; we keep the unit label but do not
  //   apply any per-nucleon conversion inside the parser.
  struct DensitySpectrumParam {
    double Emin_MeV{1.0};
    double Emax_MeV{1000.0};
    int nIntervals{50};

    // Optional cap on trajectory work per observation point.
    //
    // DS_MAX_PARTICLES limits the *total* number of backtraced trajectories
    // launched from a single observation point across the entire energy grid.
    // In DensityGridless this is enforced by reducing the number of sampled
    // directions per energy:
    //   nDir(E) = min(nDirDefault, floor(DS_MAX_PARTICLES / NenergyPoints)).
    //
    // If omitted or <= 0, no cap is applied.
    int maxParticlesPerPoint{0}; // DS_MAX_PARTICLES

    // Optional cap on how long we integrate a *single* trajectory.
    //
    // DS_MAX_TRAJ_TIME [s] provides an additional (often tighter) time limit
    // on the backtracing integration used to classify ALLOWED/FORBIDDEN.
    // This is useful for density/spectrum calculations because they can trace
    // many more trajectories than a cutoff scan.
    //
    // Semantics:
    //   - If DS_MAX_TRAJ_TIME > 0: use it as the per-trajectory integration
    //     time limit (in seconds).
    //   - If DS_MAX_TRAJ_TIME <= 0 or omitted: fall back to #NUMERICAL
    //     MAX_TRACE_TIME.
    double maxTrajTime_s{0.0}; // DS_MAX_TRAJ_TIME

    enum class Spacing { LOG, LINEAR };
    Spacing spacing{Spacing::LOG};

    int nPoints() const { return (nIntervals > 0) ? (nIntervals + 1) : 0; }

    // DS_BOUNDARY_MODE selects the density/spectrum solver branch.
    //
    // ISOTROPIC   (default, backward-compatible):
    //   T(E; x0) = N_allowed / N_dirs
    //   J_loc(E; x0) = T(E; x0) * J_b(E)
    //   The boundary spectrum J_b is assumed uniform and isotropic; the exit
    //   position and direction of each allowed trajectory are discarded.
    //
    // ANISOTROPIC:
    //   T_aniso(E; x0) = (1/N_dirs) * sum_k [ A_k * f_PAD(cos_alpha_k) * f_spatial(x_k) ]
    //   J_loc(E; x0) = J_b_iso(E) * T_aniso(E; x0)
    //   where cos_alpha_k = v_exit_k . B_hat(x_exit_k) and x_exit_k is the
    //   GSM position where trajectory k crossed the outer domain boundary.
    //   The PAD and spatial modulation models are controlled by #BOUNDARY_ANISOTROPY.
    //   Requires TraceAllowedSharedEx() to return exit state per trajectory.
    std::string boundaryMode{"ISOTROPIC"}; // DS_BOUNDARY_MODE

    //==================================================================================
    // Transmission-function controls for density/flux products.
    //==================================================================================
    // Density/flux calculations do not collapse magnetospheric access to one scalar
    // cutoff rigidity.  The physically relevant object is the transmission function
    // T(E,Omega), because density and flux are energy/direction integrals over the
    // accessible part of phase space.
    //
    // DIRECT (default):
    //   Preserve legacy behavior: evaluate T only on the user energy grid defined by
    //   DS_EMIN, DS_EMAX, DS_NINTERVALS, and DS_ENERGY_SPACING.
    //
    // SCAN:
    //   Replace the integration/output energy nodes by a log-spaced *rigidity* scan.
    //   This is analogous to the cutoff UPPER_SCAN pre-scan: rigidity is the natural
    //   access coordinate, while kinetic energy is only the spectral coordinate.
    //
    // ADAPTIVE:
    //   Currently uses the same production grid as SCAN, but keeps a separate token so
    //   validation decks can be written with the intended semantics.  The per-location
    //   refinement hook can be added later without changing input files.
    std::string transmissionMode{"DIRECT"}; // DS_TRANSMISSION_MODE: DIRECT|SCAN|ADAPTIVE

    // Number of rigidity samples used when transmissionMode is SCAN or ADAPTIVE.
    // If <=0, the code uses nIntervals+1 so existing input decks keep the same number
    // of output points while switching from an energy grid to a rigidity grid.
    int transmissionScanN{0}; // DS_TRANSMISSION_SCAN_N

    // Reserved refinement controls.  They are parsed, validated, documented, and printed
    // so users can standardize input decks now.  The current implementation does not add
    // per-location variable-size refinement records to the production Tecplot output; the
    // fixed rigidity scan is the implemented robust path.
    int transmissionRefineN{0}; // DS_TRANSMISSION_REFINE_N
    int transmissionMaxN{0};    // DS_TRANSMISSION_MAX_N

    // Optional diagnostic flag.  The current writers already save spectrum/transmission
    // curves by default for POINTS and comparison modes; this flag documents the intended
    // control and is used in banners/README.
    bool transmissionSave{false}; // DS_TRANSMISSION_SAVE

    // Explicit unresolved-trajectory policy.  Outer-box escapes, inner-sphere impacts,
    // and (when deliberately enabled for a static field) conservative trapped-orbit
    // detections are physical classifications.  Time/step/distance/invalid/numerical
    // terminations are excluded from the resolved transmissivity denominator.
    double unresolvedTolerance{0.01}; // DS_UNRESOLVED_TOL, fraction in [0,1]
    bool retryUnresolved{false};      // DS_RETRY_UNRESOLVED
    bool saveTerminationSummary{true}; // DS_SAVE_TERMINATION_SUMMARY
  };

  //====================================================================================
  // Anisotropic boundary spectrum parameters (#BOUNDARY_ANISOTROPY section)
  //====================================================================================
  // These parameters control the non-isotropic boundary spectrum used when
  // DS_BOUNDARY_MODE = ANISOTROPIC.
  //
  // The full boundary intensity is factored as:
  //   J_b(E, Omega, x) = J_b_iso(E) * f_PAD(cos_alpha) * f_spatial(x)
  //
  // where:
  //   cos_alpha = v_exit . B_hat(x_exit)   (pitch angle at the domain boundary)
  //   x_exit                               (GSM exit position on the outer boundary)
  //
  // PAD MODELS (BA_PAD_MODEL):
  //   ISOTROPIC      f(alpha) = 1                         (reduces to isotropic)
  //   SINALPHA_N     f(alpha) = sin^n(alpha)              (pancake distribution)
  //   COSALPHA_N     f(alpha) = |cos(alpha)|^n            (field-aligned beam)
  //   BIDIRECTIONAL  f(alpha) = |cos(alpha)|^n            (symmetric about equator;
  //                                                        identical to COSALPHA_N
  //                                                        but documents intent)
  //
  // SPATIAL MODELS (BA_SPATIAL_MODEL):
  //   UNIFORM              f_spatial = 1 everywhere
  //   DAYSIDE_NIGHTSIDE    f_spatial = BA_DAYSIDE_FACTOR  if GSM x > 0
  //                                  = BA_NIGHTSIDE_FACTOR if GSM x <= 0
  //====================================================================================
  struct AnisotropyParam {
    // Pitch angle distribution
    std::string padModel{"ISOTROPIC"};   // BA_PAD_MODEL
    double padExponent{2.0};             // BA_PAD_EXPONENT  (n in sin^n or |cos|^n)

    // Spatial flux modulation
    std::string spatialModel{"UNIFORM"}; // BA_SPATIAL_MODEL
    double daysideFactor{1.0};           // BA_DAYSIDE_FACTOR   (GSM x > 0 multiplier)
    double nightsideFactor{1.0};         // BA_NIGHTSIDE_FACTOR (GSM x <= 0 multiplier)
  };

  struct Species {
    std::string name{"PROTON"};
    int charge_e{1};
    double mass_amu{1.0};
  };

  struct BackgroundField {
    // FIELD_MODEL selects the background magnetic field model evaluated by
    // the gridless tools. Historically only Tsyganenko models were supported
    // (T96/T05). We extend this with an analytic dipole for verification and
    // regression tests.
    //
    // Supported values (aliases are normalised to canonical form by the parser):
    //   Canonical  Accepted aliases        Description
    //   --------   ------------------      ----------------------------------------
    //   "T96"      "TS96", "T96S"          Tsyganenko (1996)
    //   "T05"      "TS05","T05S","T04S",   Tsyganenko-Sitnov (2005)
    //              "TS04"
    //   "T01"      "TS01", "T01S"          Tsyganenko (2001)
    //   "TA15N"                            Tsyganenko-Andreeva (2015) northward
    //   "TA15B"                            Tsyganenko-Andreeva (2015) southward
    //   "TA16"    "TA16RBF"               Tsyganenko-Andreeva (2016)
    //   "IGRF"                             Geopack internal field only (no external model)
    //   "DIPOLE"                           Analytic centered dipole (internal only)
    std::string model{"T96"};

    // --- Dipole-only parameters (FIELD_MODEL = DIPOLE) ---
    // DIPOLE_MOMENT: dipole moment magnitude as a multiple of Earth's canonical
    // dipole moment M_E (default 1.0).
    double dipoleMoment_Me{1.0}; // keyword: DIPOLE_MOMENT

    // DIPOLE_TILT [deg]: dipole tilt angle in degrees measured from +Z_GSM
    // toward +X_GSM (rotation about +Y_GSM). Default 0.0 aligns the dipole
    // with the GSM Z-axis.
    double dipoleTilt_deg{0.0};  // keyword: DIPOLE_TILT (alias: DIPOLE_TILT_DEG)

    // Common parameters
    double dst_nT{-50.0};
    double pdyn_nPa{2.0};
    double imfBy_nT{0.0};
    double imfBz_nT{0.0};

    // Present in RoR files (reserved)
    double imfBx_nT{0.0};
    double swVx_kms{-400.0};
    double swN_cm3{5.0};

    // T01 storm-time activity indices
    double g[3]{0,0,0};

    // T05/TA15/TA16 storm-time history integrals
    double w[6]{0,0,0,0,0,0};

    // TA15 running averages of southward IMF Bz magnitude
    double bzAvg[6]{0,0,0,0,0,0};

    // TA15/TA16 optimal solar-wind coupling index.
    // For TA15 this maps to PARMOD(4); for TA16 it maps to PARMOD(3).
    // Defaults to 0.0 when not provided.
    double xind{0.0};

    // TA16 coefficient file path (TA16_RBF.par or equivalent).
    // Empty string means use the Fortran default (TA16_RBF.par in the CWD).
    // Set via keyword TA16_COEFF_FILE in #BACKGROUND_FIELD.
    std::string ta16CoeffFile;

    // Global background-field snapshot/reference epoch.
    //
    // INPUT FILE
    // ----------
    // The value is selected with the EPOCH keyword inside #BACKGROUND_FIELD:
    //
    //   #BACKGROUND_FIELD
    //   FIELD_MODEL  T96
    //   EPOCH        2010-01-01T00:00:00
    //
    // Recommended representation is an ISO-like UTC timestamp
    // YYYY-MM-DDTHH:MM:SS.  A space separator is also retained as part of the
    // value because the parser stores everything after the key; the command-line
    // form must quote such a value to protect it from shell tokenization.
    //
    // RUNTIME USE
    // -----------
    // This one value is consumed consistently by the standalone field/trajectory
    // workflows.  It determines the Geopack RECALC/IGRF coefficient epoch, the
    // Tsyganenko dipole tilt, SPICE frame rotations, Mode3D mesh-field snapshot, and
    // the reference time used by snapshot driver/spectrum selection.
    //
    // CLI PRECEDENCE
    // --------------
    // main.cpp applies a non-empty -epoch/--epoch value after the input file has
    // been parsed and before any field initialization.  The resulting precedence is
    //
    //   CLI --epoch  >  #BACKGROUND_FIELD EPOCH  >  this compiled default.
    //
    // In TRAJECTORY temporal mode, explicitly time-tagged trajectory samples may
    // still select per-sample epochs; this field remains the global snapshot and
    // fallback/reference epoch.
    std::string epoch{"2000-01-01T00:00"};

    // Optional external driver table file specified directly in #BACKGROUND_FIELD
    // via DRIVER_FILE.
    //
    // Why this lives in BackgroundField instead of Temporal:
    //   Historically, AMPS inputs often describe the magnetic-field model and its
    //   driver source together in one section:
    //
    //     #BACKGROUND_FIELD
    //     FIELD_MODEL   TS05
    //     DRIVER_FILE   ts05_driving_....txt
    //
    //   The gridless implementation already has a generic time-dependent driver
    //   table capability, but before this change it was only activated through
    //   #TEMPORAL TS_INPUT_MODE=FILE / TS_INPUT_FILE=...
    //
    //   Storing the filename here lets the parser honor the more natural
    //   BACKGROUND_FIELD-based syntax while still loading the same shared
    //   TsDriverTable used by the trajectory/gridless solvers.
    //
    // Semantics:
    //   - empty string: no driver table requested from #BACKGROUND_FIELD
    //   - non-empty    : parser must load the file after the full input is read
    //                    and attach it to temporal.driverTable
    //
    // Precedence rules are implemented in amps_param_parser.cpp.
    std::string driverFile;

    // Raw key/value store for forward compatibility.
    std::map<std::string,std::string> raw;
  };


  struct ElectricField {
    // Extensible electric-field model selector for the AMPS-backed 3D path.
    //
    // Supported values in this patch:
    //   - NONE
    //   - COROTATION_VOLLAND_STERN
    //
    // The intent is to keep the parser/API stable while making it easy to
    // add other models later (Weimer, user tables, solver-backed fields, etc.).
    std::string model{"NONE"};

    // Multiplier applied to the nominal corotation electric field.
    double corotationScale{1.0};

    // Volland-Stern-type shielded convection potential parameters. The exact
    // volumetric extension used by the 3D initializer is documented in
    // srcEarth/3d/ElectricField.cpp.
    double vsPotential_kV{60.0};
    double vsGamma{2.0};
    double vsReferenceL{10.0};
    double vsScale{1.0};

    // Numerical guards used by the volumetric initializer.
    double rMin_km{20.0};
    double lMin{1.0e-3};

    std::map<std::string,std::string> raw;
  };


  //====================================================================================
  // SpacecraftTrajectoryPoint / SpacecraftTrajectory
  //====================================================================================
  // To unify the treatment of standalone POINTS and sampled spacecraft trajectories,
  // every spatial sample is represented as a trajectory point carrying:
  //   - timeUTC    : ISO-8601 timestamp string associated with the sample
  //   - xGSM_m     : location in GSM Cartesian coordinates [m]
  //
  // UNIT CONTRACT
  //   The position is stored in SI meters so it can be handed directly to the
  //   particle-tracing routines (Boris pusher, field evaluators) without any
  //   additional unit conversion at the call site.
  //
  //   The legacy OutputDomain::points array (used for Tecplot output and the
  //   inherited POINTS code path) remains in km; RebuildFlattenedPointsFromTrajectories
  //   performs the m -> km division when populating it.
  //
  // For legacy OUTPUT_MODE=POINTS runs, we create one synthetic trajectory per point.
  // Each such trajectory contains exactly one sample and therefore reuses the exact
  // same downstream computation kernel as true multi-point trajectories.
  //
  // IMPORTANT:
  //   The solver physics is always evaluated in GSM. If the trajectory file is given
  //   in another frame (for example GEO or SM), the parser converts it to GSM when
  //   loading the file so all downstream code sees a single consistent representation.
  struct SpacecraftTrajectoryPoint {
    std::string timeUTC;
    Vec3 xGSM_m;   // GSM Cartesian position [m]
  };

  class SpacecraftTrajectory {
  public:
    std::string name;
    std::string sourceFrame{"GSM"};
    std::vector<SpacecraftTrajectoryPoint> samples;

    inline bool empty() const { return samples.empty(); }
    inline std::size_t size() const { return samples.size(); }

    inline void clear() {
      name.clear();
      sourceFrame = "GSM";
      samples.clear();
    }

    inline void AddSample(const std::string& timeUTC, const Vec3& xGSM_m) {
      samples.push_back(SpacecraftTrajectoryPoint{timeUTC, xGSM_m});
    }
  };

  struct OutputDomain {
    // OUTPUT_MODE:
    //   POINTS     - explicit list of individual locations
    //   SHELLS     - spherical shell map(s)
    //   TRAJECTORY - time-ordered sequence loaded from TRAJ_FILE
    std::string mode{"POINTS"};

    // Coordinate label for reporting / backward compatibility with older inputs.
    // For trajectories, TRAJ_FRAME is the authoritative input-frame selector.
    std::string coords{"GSM"};

    // Legacy flattened point list used by the existing solver kernels.
    // For OUTPUT_MODE=TRAJECTORY this is populated automatically from trajectories[0].
    std::vector<Vec3> points;

    // Unified representation: each standalone point is packed into a one-sample
    // trajectory so trajectory and point workflows can share the same code path.
    std::vector<SpacecraftTrajectory> trajectories;

    // Trajectory-specific controls.
    std::string trajFrame{"GSM"};      // TRAJ_FRAME
    std::string trajFile;               // TRAJ_FILE
    double fluxDt_min{1.0};             // FLUX_DT  [min]

    // SHELLS: altitudes in km and angular resolution in degrees.
    //
    // shellRes_deg is the backward-compatible common resolution.  When either of the
    // axis-specific values is positive it overrides the common value on that axis.
    // This permits reference grids such as 5 deg latitude x 30 deg longitude without
    // forcing AMPS to calculate the unused intermediate longitudes.
    std::vector<double> shellAlt_km;
    double shellRes_deg{15.0};
    double shellLonRes_deg{-1.0}; // SHELL_LON_RES_DEG; <=0 -> shellRes_deg
    double shellLatRes_deg{-1.0}; // SHELL_LAT_RES_DEG; <=0 -> shellRes_deg

    // Geometry used to convert shell latitude/longitude/altitude into Cartesian
    // positions and the local vertical direction.
    //
    // SPHERICAL (default) reproduces the historical Earth-radius shell.  GEODETIC
    // uses the WGS-84/GEOPACK reference ellipsoid and the ellipsoid normal as the
    // vertical.  The latter is necessary for direct comparison with published world
    // grids specified as height above the International Reference Ellipsoid.
    std::string shellGeometry{"SPHERICAL"}; // SHELL_GEOMETRY: SPHERICAL|GEODETIC

    // Helper utilities used by the parser and the solvers.
    inline bool HasTrajectorySamples() const {
      for (const auto& tr : trajectories) if (!tr.empty()) return true;
      return false;
    }

    inline void ClearFlattenedPoints() { points.clear(); }

    inline void RebuildFlattenedPointsFromTrajectories() {
      points.clear();
      for (const auto& tr : trajectories) {
        for (const auto& s : tr.samples) {
          // xGSM_m is stored in meters; the legacy points array is in km.
          points.push_back({s.xGSM_m.x / 1000.0,
                            s.xGSM_m.y / 1000.0,
                            s.xGSM_m.z / 1000.0});
        }
      }
    }

    std::map<std::string,std::string> raw;
  };

  struct Numerical {
    double dtTrace_s{1.0};

    // ADAPTIVE_DT
    // -----------
    // true  (default): DT_TRACE is an upper bound; the solvers reduce the actual
    //                  step using gyro-resolution and boundary-distance limiters.
    // false          : DT_TRACE is used as a fixed integration step, except for a
    //                  final trim to the remaining allowed trace time.  This mode
    //                  is mainly for convergence/regression tests where the user
    //                  wants DT_TRACE itself to control the pusher accuracy.
    bool adaptiveDt{true};

    int maxSteps{300000};
    double maxTraceTime_s{7200.0};

    // Boundary-event reconstruction controls shared by gridless and Mode3D
    // backtracers.  Each accepted mover step is treated as a trajectory chord and
    // intersected analytically with the inner sphere and outer Cartesian box.
    // The tolerance is geometric only; it must never be used to increase dt.
    double boundaryEventTolerance_m{1.0};
    int boundaryEventMaxIterations{40}; // reserved for future curved-step refinement

    // Optional conservative trapped-orbit classification for static magnetic fields.
    // It is disabled by default because time-dependent fields require a separate policy.
    // When enabled, repeated mirror points and stable bounce envelopes may terminate a
    // trajectory as physically forbidden instead of letting it expire at a time limit.
    bool trapDetection{false};
    int trapMinMirrorPoints{8};
    int trapMinBounceCycles{4};
    double trapOuterMargin_Re{1.0};
    double trapRadialGrowthTolerance_Re{0.05};
    double trapEnergyRelativeTolerance{1.0e-4};
    double trapParallelDeadband{1.0e-6};

    // Optional drift-shell recurrence branch of the frozen-field trap detector.
    // The historical mirror/bounce detector can miss particles with pitch angles
    // close to 90 degrees because v_parallel may not produce clean sign reversals.
    // C19 enables this branch so repeated azimuthal circulation with a recurring
    // azimuth-resolved full-orbit profile can terminate as a physical trapped state
    // before a safety limit.
    bool trapDriftDetection{false};
    int trapMinDriftRevolutions{3};
    // Full-orbit drift recurrence tolerances.  The historical
    // TRAP_DRIFT_RADIAL_GROWTH_TOL_RE name is retained as the absolute radius
    // tolerance for backward-compatible input decks; the additional relative and
    // phase-space tolerances make the criterion shell-splitting aware in T05.
    double trapDriftRadialGrowthTolerance_Re{1.0};
    double trapDriftRadialRelativeTolerance{0.20};
    double trapDriftLatitudeTolerance{0.20};
    double trapDriftPitchCos2Tolerance{0.25};
    // Additional secular-drift veto for the full-orbit recurrence classifier.
    // <=0 disables the gate globally; C19 enables a finite value explicitly.
    double trapDriftMaxMeanRadiusChange_Re{0.0};
    int trapDriftProfileBins{24};
    double trapDriftMinProfileCoverage{0.70};
    double trapDriftMinMatchedBinFraction{0.75};

    // Compatibility fields for CCMC/Runs-on-Request particle-control keywords.
    // They are parsed so strict validation accepts the input file.  At present
    // Mode3D cutoff/density backtracking does not consume all of them directly;
    // solver-specific particle counts remain controlled by CUTOFF_* and DS_*
    // settings.
    int nParticles{0};          // N_PARTICLES, 0 means not specified
    int maxBounce{0};           // MAX_BOUNCE, 0 means not specified
    bool pitchIsotropic{true};  // PITCH_ISOTROPIC

    // MAX_TRACE_DISTANCE [Re]
    // -----------------------
    // Optional hard cap on the *cumulative path length* traveled by a single
    // backtraced particle trajectory, measured in Earth radii.
    //
    // Why cumulative path length instead of distance-from-launch?
    //   The tracing workflows here (cutoff, density, and anisotropic spectrum)
    //   can encounter quasi-trapped or drifting trajectories that remain near the
    //   launch location while still traveling a very long total arc length.
    //   Using cumulative path length makes this control analogous to
    //   MAX_TRACE_TIME: it limits total tracing work irrespective of whether the
    //   orbit loops back near the start point.
    //
    // Semantics:
    //   maxTraceDistance_Re <= 0 : disabled (backward-compatible default)
    //   maxTraceDistance_Re >  0 : stop tracing once the accumulated segment
    //                              lengths exceed this threshold.
    //
    // Units:
    //   Earth radii (Re), not km and not meters. The solver converts Re -> m
    //   internally at runtime using _EARTH__RADIUS_.
    double maxTraceDistance_Re{0.0};
  };

  struct CalcMode {
    std::string target{"CUTOFF_RIGIDITY"};
    std::string fieldEvalMethod{"GRIDLESS"};

    // True only when CALC_TARGET appears explicitly in #CALCULATION_MODE.
    // The target default remains CUTOFF_RIGIDITY for backward compatibility in
    // standalone tools, but the SWMF coupling bridge uses this flag to avoid
    // accidentally converting old 3d_forward input files into cutoff runs.
    bool targetExplicit{false};
  };

  //====================================================================================
  //====================================================================================
  // Density3DParam — 3D volumetric density sampling for Mode3DForward (#DENSITY_3D)
  //====================================================================================
  //
  // Controls the energy grid used to bin sampled particle number density in the
  // volumetric (per-cell) sampling pass of Mode3DForward.
  //
  // Input file section:
  //   #DENSITY_3D
  //     DENS_EMIN              1.0          ! MeV/n  lower energy bound
  //     DENS_EMAX              20000.0      ! MeV/n  upper energy bound
  //     DENS_NENERGY           30           ! number of energy bins
  //     DENS_ENERGY_SPACING    LOG          ! LOG or LINEAR bin spacing
  //
  // Resulting output variable:
  //   n(E_i) [m^-3 J^-1] — differential number density per Joule in each cell
  //   Integrated over ΔE_i to get the bin-total number density [m^-3].
  //
  struct Density3DParam {
    double Emin_MeV{1.0};      // DENS_EMIN [MeV/n]
    double Emax_MeV{20000.0};  // DENS_EMAX [MeV/n]
    int    nEnergyBins{30};    // DENS_NENERGY

    // Energy-bin spacing (shared enum shape with DensitySpectrumParam::Spacing
    // to reuse the ParseEnergySpacingToken helper in the parser).
    enum class Spacing { LOG, LINEAR };
    Spacing spacing{Spacing::LOG};  // DENS_ENERGY_SPACING
  };

  //====================================================================================
  // Mode3DForwardOptions — runtime controls for the 3D forward particle mode
  //====================================================================================
  //
  // These options are populated from the CLI and/or the input file (where applicable).
  //
  // CLI flags:
  //   -mode 3d_forward                              select this mode
  //   -mode3d-output-initialized                    write initialized mesh Tecplot file
  //   -forward-niter <int>                          override iteration count
  //   -forward-nparticles <int>                     override simulation particles per iteration;
  //                                                 physical weight is recomputed automatically:
  //                                                   W = (π×∫J dE×A_boundary×dt)/N
  //   -forward-boundary-dist <ISOTROPIC|...>        override boundary distribution type
  //   -forward-track-trajectories                    initialize AMPS trajectory records
  //   -forward-no-track-trajectories                 disable trajectory-record initialization
  //   -forward-n-trajectories <int>                  maximum injected-particle trajectories
  //
  // Input file keywords:
  //   #NUMERICAL
  //     FORWARD_N_PARTICLES  <int>   simulation particles injected per iteration
  //   #PARTICLE_TRAJECTORY
  //     INITIALIZE_TRAJECTORIES T|F  runtime trajectory-record gate
  //     N_TRAJECTORIES        <int>  trajectory-record cap; >0 also enables tracking
  //
  struct Mode3DForwardOptions {
    // Write amps_3dforward_initialized.data.dat after InitMeshFields().
    bool outputInitializedFile{false};

    // Total number of forward integration iterations to run.
    // Can be overridden by -forward-niter on the CLI.
    int nIterations{10000};

    // Simulation particles injected from the domain boundary each iteration.
    // The physical weight per particle is calculated from the boundary flux,
    // the time step dt, and this count:
    //   W = (π × ∫J dE × A_boundary × dt) / nParticlesPerIter
    int nParticlesPerIter{1000};

    // Angular distribution type at the domain boundary.
    //   ISOTROPIC (default) — uniform-intensity boundary with cos(θ)-weighted
    //                          directions (see BoundaryDistribution.h for physics).
    // Future values: COSALPHA_N, SINALPHA_N, DAYSIDE_NIGHTSIDE
    // Populated from -forward-boundary-dist CLI flag or the input file.
    std::string boundaryDistType{"ISOTROPIC"};

    // Energy proposal distribution used by Mode3DForward boundary injection.
    //   SPECTRUM     — legacy/default behavior: sample E from J(E)dE and use
    //                  nearly equal statistical weights.
    //   LOG_UNIFORM  — sample E uniformly in log(E) and apply an individual
    //                  particle statistical-weight correction q(E).  This improves
    //                  high-energy statistics for steep SEP spectra while keeping
    //                  the total physical source rate identical to the input
    //                  spectrum.
    //
    // The field is currently set by the CLI.  The parser also accepts optional
    // input-file keys FORWARD_INJECTION_ENERGY[_DISTRIBUTION] /
    // FORWARD_ENERGY_SAMPLING for forward compatibility with future input files.
    std::string injectionEnergyDistribution{"SPECTRUM"};

    // Active particle mover used by the single AMPS-signature 3d_forward mover
    // manager.  Supported values: BORIS, RK4, GC/GC4, HYBRID.
    //
    // CURRENT POLICY: this active value is set from the CLI (-mover) or remains BORIS.
    // The input-file parser does not currently change it.  This is intentional: the
    // 3d_forward branch should have one active selection path until the final input-file
    // keyword syntax and precedence rules are implemented.
    std::string particleMover{"BORIS"};

    // Reserved input-file mover selection.  The parser recognizes future keys such as
    // FORWARD_MOVER / FORWARD_PARTICLE_MOVER / FORWARD_INTEGRATOR and stores the value
    // here, but Mode3DForward::Run() does not apply this field yet.  This reserves the
    // necessary space for future input-file support without creating two competing
    // active selector paths today.
    std::string reservedInputFileParticleMover{""};
    bool hasReservedInputFileParticleMover{false};

    // Runtime gate for AMPS particle-trajectory initialization in 3d_forward.
    // This is an additional model-level control on top of the AMPS compile-time
    // switch _PIC_PARTICLE_TRACKER_MODE_.  No trajectory output is possible unless
    // AMPS is compiled with that switch ON; when it is ON, this flag decides
    // whether newly injected 3d_forward particles request trajectory records.
    //
    // Default is off because trajectory output can be large.  It can be enabled
    // by either:
    //   #PARTICLE_TRAJECTORY / INITIALIZE_TRAJECTORIES T
    //   #PARTICLE_TRAJECTORY / N_TRAJECTORIES <positive int>
    bool initializeParticleTrajectories{false};

    // Maximum number of injected-particle trajectory records initialized by the
    // model.  Default is zero so the parser/configuration path must explicitly
    // enable trajectory initialization; the AMPS callback must not fall back to
    // an independent hard-coded cap.
    int nParticleTrajectories{0};
  };

  // Mode3DOptions — command-line controls specific to the PIC-backed 3D workflow
  //====================================================================================
  struct Mode3DOptions {
    // Write amps_3d_initialized.data.dat after InitMeshFields().
    // Default is false because this is a diagnostic file and can be large.
    bool outputInitializedFile{false};

    // Parallelize standalone Mode3D owner-rank B/E mesh initialization with a
    // temporary POSIX-thread team.  The number of temporary workers is resolved from
    // densityThreads/MODE3D_THREADS; the calling MPI-rank thread also participates.
    // This option is intentionally CLI-only so existing input decks retain the
    // historical serial initialization behavior unless explicitly requested.
    bool parallelFieldInitialization{false};

    // Magnetic-field source used by the Mode3D cutoff tracer.
    // false: AMR interpolation from cell-centered data populated by InitMeshFields().
    // true : direct analytic/background evaluator call via EvaluateBackgroundMagneticFieldSI().
    bool forceAnalyticMagneticField{false};

    // Optional user-defined AMR mesh-resolution profile for standalone -mode 3d.
    //
    // Backward compatibility: meshResolutionProfileActive=false leaves the existing
    // hard-coded main_lib.cpp::localResolution() behavior unchanged.  When active,
    // localResolution() uses a simple radial profile between a requested resolution
    // at r≈1 Re and a requested resolution at the outer 3-D domain boundary.
    //
    // Canonical input-file section/keys:
    //   #MODE3D_MESH
    //     MODE3D_MESH_RES_EARTH_RE     <double>
    //     MODE3D_MESH_RES_BOUNDARY_RE  <double>
    //     MODE3D_MESH_COARSENING       LINEAR|LOG|EXPONENTIAL|POWER|CONSTANT
    //     MODE3D_MESH_EXPONENT         <double>   (used by POWER/EXPONENT)
    //     MODE3D_MESH_R_BOUNDARY_RE    <double>   (optional; auto from domain when <=0)
    //
    // Units: resolutions and the optional boundary radius are stored in km in the
    // parser so Re/km suffixes can be accepted consistently.  Mode3D converts them
    // to SI/Re before mesh construction.
    bool meshResolutionProfileActive{false};
    double meshResolutionEarth_km{0.0};
    double meshResolutionBoundary_km{0.0};
    double meshResolutionOuterRadius_km{0.0};
    std::string meshResolutionCoarsening{"LINEAR"};
    double meshResolutionExponent{1.0};

    // Backward density/spectrum intra-rank parallel backend.
    //   OPENMP  : use the existing OpenMP loops over energies/directions.
    //   THREADS : use direct std::thread workers over observation locations and suppress
    //             nested OpenMP inside those workers.
    //   SERIAL  : no shared-memory parallelism inside each MPI rank.
    // Empty means use the code default (OPENMP unless overridden by environment).
    std::string densityParallelBackend{""};

    // Per-MPI-rank shared-memory thread count used by the selected backend.
    // 0 means automatic: use OMP_NUM_THREADS/omp_get_max_threads for OpenMP-aware
    // builds or hardware_concurrency for the direct std::thread backend.
    int densityThreads{0};

    // Inter-rank scheduler for standalone Mode3D backtracking products.
    //   DYNAMIC      : ranks atomically fetch chunks of global locations with MPI RMA.
    //   BLOCK_CYCLIC : rank r computes r, r+nRanks, r+2*nRanks, ...
    //   STATIC       : rank r computes one contiguous block.
    // Empty means the shared resolver default is used; current default is DYNAMIC.
    std::string mpiScheduler{""};

    // Chunk size, in global observation locations, for MODE3D_MPI_SCHEDULER=DYNAMIC.
    // 0 means automatic, usually several chunks per worker per rank.
    int mpiDynamicChunk{0};
  };

  //====================================================================================
  // EnergyChannel — one user-defined integral-flux channel (#ENERGY_CHANNELS section)
  //====================================================================================
  //
  // PHYSICS
  //   An energy channel [E1, E2] defines the integration range for the omnidirectional
  //   integral particle flux at each observation point x0:
  //
  //     F_ch(x0) = 4π ∫_{E1}^{E2} J_loc(E; x0) dE
  //              = 4π ∫_{E1}^{E2} T(E; x0) · J_b(E) dE          [m^-2 s^-1]
  //
  //   Integral flux differs from number density by the absence of a 1/v(E) weight:
  //
  //     n(x0)    = 4π ∫ J_loc(E; x0) / v(E) dE                   [m^-3]
  //
  //   The 1/v weight makes density sensitive to slow particles. Flux treats all
  //   energies in the channel with equal weight (per unit energy interval).
  //
  // ANALYTIC CLOSED FORM (power-law boundary spectrum, gamma ≠ 1)
  //   For J_b(E) = J0 · (E/E0)^{-gamma}:
  //
  //     F_ch = 4π · T · J0 · E0^gamma / (gamma-1) · ( E1^{1-gamma} − E2^{1-gamma} )
  //
  //   Special case gamma = 2 (the test configuration):
  //     F_ch = 4π · T · J0 · E0² · ( 1/E1 − 1/E2 )
  //
  //   Total flux over the full solver energy range [Emin, Emax] is obtained by
  //   substituting E1=Emin, E2=Emax in the same formula.
  //
  // NUMERICAL IMPLEMENTATION
  //   Channel integrals are computed from the T(E) array already computed by the
  //   density solver. For each channel [E1, E2]:
  //
  //     (1) Find all grid nodes E_i with E1 ≤ E_i ≤ E2. Call this sub-array {E_i}.
  //     (2) Prepend E1 with T linearly interpolated from the two surrounding nodes if
  //         E1 does not coincide with a grid node. Append E2 similarly.
  //     (3) Accumulate F = 4π · Σ_i 0.5·(J_loc_i + J_loc_{i+1})·(E_{i+1} − E_i)
  //         using the trapezoidal rule in MeV (then convert to J internally, or use
  //         a consistent unit system throughout).
  //
  //   Channels may overlap and may extend outside [DS_EMIN, DS_EMAX]; the
  //   integration is silently clipped to the available grid range.
  //   No error is raised for an empty intersection (F_ch = 0 is returned).
  //
  // INPUT FORMAT (#ENERGY_CHANNELS section):
  //   CH_BEGIN
  //     NAME   E1_MeV   E2_MeV   [! optional comment]
  //     ...
  //   CH_END
  //
  //   NAME    — identifier string used in output column headings (F_NAME_m2s1).
  //             Allowed characters: letters, digits, underscores.
  //   E1_MeV  — lower channel boundary [MeV], must be > 0.
  //   E2_MeV  — upper channel boundary [MeV], must be > E1_MeV.
  //====================================================================================
  struct EnergyChannel {
    std::string name;       // identifier  (e.g. "CH1", "P10_100")
    double E1_MeV{0.0};    // lower bound [MeV]
    double E2_MeV{0.0};    // upper bound [MeV]
  };

  //====================================================================================
  // TsDriverRecord — one time-stamped snapshot of Tsyganenko model driving parameters
  //====================================================================================
  // Parsed from the Qin-Denton / ViRBO format (qd_sep2017.txt and equivalents).
  // Column layout (1-based, space-separated):
  //   $1  : ISO-8601 datetime  (e.g. 2017-09-10T00:00:00)
  //   $2–7: year month day hour min sec   (not read; $1 is used for time)
  //   $8  : IMF By  [nT]
  //   $9  : IMF Bz  [nT]
  //   $10 : Vsw     [km/s]
  //   $11 : Den_P   [cm^-3]
  //   $12 : Pdyn    [nPa]
  //   $13–15: G1 G2 G3   (T96 external-field G parameters, not presently used)
  //   $16–23: 8 status flags   (skipped)
  //   $24 : Kp      [dimensionless]
  //   $25 : akp3    [dimensionless]   (not presently used)
  //   $26 : Dst     [nT]
  //   $27–32: Bz1..Bz6  (historic average IMF Bz over 1..6 h; not presently used)
  //   $33–38: W1..W6    (T05 storm-time history integrals)
  //   $39–44: W_status flags  (skipped)
  //
  // Only the physically required fields (By, Bz, Vsw, DenP, Pdyn, Dst, W1..W6)
  // are stored; everything else is parsed and discarded.
  struct TsDriverRecord {
    double et{0.0};          // SPICE ephemeris time of this record [s]
    std::string timeUTC;     // original ISO-8601 string (kept for diagnostics)

    // T96 + T05 shared parameters
    double imfBy_nT{0.0};
    double imfBz_nT{0.0};
    double swVx_kms{-400.0}; // stored as positive magnitude in file; sign applied here
    double swN_cm3{5.0};
    double pdyn_nPa{2.0};
    double dst_nT{0.0};

    // T01 storm-time activity indices G1..G3
    double g[3]{0,0,0};

    // T05/TA15/TA16 storm-time integrals W1..W6
    double w[6]{0,0,0,0,0,0};

    // TA15 running averages BZ1..BZ6
    double bzAvg[6]{0,0,0,0,0,0};

    // TA15 coupling index XIND (optional in generic driver tables).
    double xind{0.0};
  };

  //====================================================================================
  // TsDriverTable — time-ordered table of TsDriverRecords with lookup/interpolation
  //====================================================================================
  //
  // USAGE
  //   After construction the table is populated by ParseAmpsParamFile when
  //   TS_INPUT_MODE = FILE and TS_INPUT_FILE is set in #TEMPORAL.
  //
  //   At runtime (per trajectory point), call:
  //     TsDriverRecord rec = table.Lookup(et);
  //   to get a linearly interpolated snapshot of every driving parameter at the
  //   requested ephemeris time.  The result can be applied directly to
  //   BackgroundField before (re-)initialising Geopack and the Tsyganenko model.
  //
  // INTERPOLATION POLICY
  //   Linear interpolation between the two bracketing records is used for all
  //   continuous parameters (By, Bz, Vsw, DenP, Pdyn, Dst, W1..W6).
  //   If the requested time lies outside the table range, the nearest endpoint
  //   record is returned (clamped) with a one-time stderr warning.
  //
  // THREAD SAFETY
  //   The table is read-only after construction.  Concurrent Lookup() calls from
  //   MPI workers (or OpenMP threads) are safe without any locking.
  class TsDriverTable {
  public:
    TsDriverTable() = default;

    // Returns true if the table contains at least one record.
    inline bool empty() const { return records_.empty(); }
    inline std::size_t size() const { return records_.size(); }

    // Add a record (must be called in chronological order).
    void push_back(const TsDriverRecord& r) { records_.push_back(r); }

    // Linear interpolation at ephemeris time et [s].
    // Clamps to the table endpoints when et is out of range.
    // Exits with a clear error message if the table is empty.
    TsDriverRecord Lookup(double et) const;

    // Apply the parameters from a record into a BackgroundField snapshot.
    // This does NOT update field.epoch (the caller is responsible for setting
    // epoch to the trajectory-point timestamp before calling Geopack::Init).
    static void ApplyToField(const TsDriverRecord& rec, BackgroundField& field);

  private:
    std::vector<TsDriverRecord> records_;
    mutable bool clampWarnedLow_{false};
    mutable bool clampWarnedHigh_{false};
  };



  // Build the 10-element Tsyganenko PARMOD vector plus trailing compatibility slot.
  // Layout depends on the canonical FIELD_MODEL name. Unknown or unsupported
  // models fall back to the shared base set [Pdyn,Dst,By,Bz,0..].
  void BuildTsParmod(const BackgroundField& field,
                     const std::string& model,
                     double parmod[11]);

  //====================================================================================
  // TemporalParam — parameters from the #TEMPORAL section
  //====================================================================================
  // Controls the time-series (trajectory) execution mode: event window, update
  // cadences, and the optional time-varying Tsyganenko driver file.
  struct TemporalParam {
    // TEMPORAL_MODE:
    //   SNAPSHOT      - one field realization at #BACKGROUND_FIELD/EPOCH (legacy).
    //   TIME_SERIES   - regular EVENT_START..EVENT_END cadence.
    //   SNAPSHOT_LIST - explicit, possibly irregular epochs read from
    //                   SNAPSHOT_LIST_FILE.  Standalone Mode3D uses this mode to
    //                   retain one allocated mesh while processing a batch of
    //                   independently timestamped trajectory locations.
    std::string mode{"SNAPSHOT"};

    // Event window [ISO-8601 UTC strings].
    std::string eventStart;
    std::string eventEnd;

    // FIELD_UPDATE_DT [min]: cadence at which the magnetic field model is updated.
    // Fractional values are allowed, e.g. FIELD_UPDATE_DT 0.5 means 30 seconds.
    double fieldUpdateDt_min{5.0};

    // INJECT_DT [min]: cadence at which particles are injected along the trajectory.
    double injectDt_min{30.0};

    // TS_INPUT_MODE: FILE (load from TS_INPUT_FILE) or PARAMS (use #BACKGROUND_FIELD).
    std::string tsInputMode{"PARAMS"};

    // TS_INPUT_FILE: path to a Qin-Denton / ViRBO formatted driver file.
    std::string tsInputFile;

    // Explicit Mode3D snapshot epochs, one ISO-8601 UTC token per non-comment line.
    // This is ignored unless TEMPORAL_MODE=SNAPSHOT_LIST, preserving every existing
    // input deck that does not opt into batched snapshot execution.
    std::string snapshotListFile;

    // Populated by ParseAmpsParamFile when tsInputMode == "FILE".
    TsDriverTable driverTable;
  };

  struct AmpsParam {
    std::string runId{"UNKNOWN"};

    // Non-physics run metadata from #RUN_INFO.  These keys are useful for
    // provenance in Runs-on-Request files but do not change the numerical model.
    std::map<std::string,std::string> runInfo;

    CalcMode calc;
    Mode3DOptions mode3d;
    Mode3DForwardOptions mode3dForward;   // -mode 3d_forward controls
    CutoffScan cutoff;
    DensitySpectrumParam densitySpectrum;
    Density3DParam density3d;             // #DENSITY_3D section
    AnisotropyParam anisotropy;
    Species species;
    BackgroundField field;
    ElectricField efield;
    DomainBox domain;
    OutputDomain output;
    Numerical numerics;
    TemporalParam temporal;   // #TEMPORAL section (time-series mode + driver table)

    std::map<std::string,std::string> spectrum;
    ParticleSpectrum particleSpectrum;  // typed view of #SPECTRUM, built at init time
    std::map<std::string,std::string> outputOptions;

    // User-defined integral-flux channels.  Empty when #ENERGY_CHANNELS is absent;
    // in that case only the total integral flux F_tot is written to the output files.
    std::vector<EnergyChannel> fluxChannels;

    // Retained for ABI/source compatibility with older parser clients.
    // The strict parser no longer populates this map: unknown input sections or
    // keywords terminate execution immediately through exit(__LINE__,__FILE__,msg).
    std::map<std::string,std::string> unknown;
  };

  // Parse and validate the #SPECTRUM section into a typed representation while
  // preserving the original raw key/value table for the downstream spectrum
  // evaluator.
  ParticleSpectrum ParseParticleSpectrum(const std::map<std::string,std::string>& kv);

  // Load a trajectory file and convert every sample into GSM Cartesian coordinates
  // in meters.  This is used by OUTPUT_MODE=TRAJECTORY during initialization.
  SpacecraftTrajectory LoadTrajectoryFileAsGsm(const std::string& fileName,
                                               const std::string& trajFrame);

  // Finalize the point-like output representation after parsing.
  //   POINTS     -> pack standalone points into one-sample synthetic trajectories
  //   TRAJECTORY -> load the trajectory file and flatten it into output.points
  //   SHELLS     -> leave the point/trajectory containers untouched
  void InitializePointLikeOutput(AmpsParam& p);

  // Parse one generic directional-aperture specification and load an optional
  // aperture-list file.  These helpers are public so command-line overrides can use
  // exactly the same validation/normalization rules as AMPS_PARAM input.
  DirectionalAperture ParseDirectionalApertureSpec(const std::string& text);
  std::vector<DirectionalAperture> LoadDirectionalApertureFile(const std::string& fileName);

  // Parse an AMPS_PARAM file. Throws std::runtime_error on hard errors.
  AmpsParam ParseAmpsParamFile(const std::string& fileName);

  // Helper conversions (public because both CLI and solver use them).  Trim is
  // intentionally exported with ToUpper/ToBool: Mode3D's SNAPSHOT_LIST reader must
  // normalize a filename, input rows, and parsed mode tokens with exactly the same
  // whitespace rules as the AMPS_PARAM parser that populated those strings.
  std::string Trim(const std::string& s);
  bool ToBool(const std::string& s);
  std::string ToUpper(std::string s);

}

#endif
