# AMPS Earth Energetic-Particle Model


## Selecting the background-field epoch

The global background-field snapshot/reference time is selected in the input file with
`EPOCH` inside `#BACKGROUND_FIELD`:

```text
#BACKGROUND_FIELD
FIELD_MODEL  T96
EPOCH        2010-01-01T00:00:00
```

The recommended format is `YYYY-MM-DDTHH:MM:SS`.  This epoch is used when initializing
Geopack/IGRF coefficients, Tsyganenko dipole tilt, SPICE coordinate rotations, and the
Mode3D field snapshot.  It can be overridden without editing the input file:

```bash
./amps -mode gridless -i AMPS_PARAM.in --epoch 1965-01-01T00:00:00
./amps -mode 3d       -i AMPS_PARAM.in --epoch 2010-01-01T00:00:00
```

The precedence is `--epoch` > input-file `EPOCH` > the compiled default
`2000-01-01T00:00`.  A timestamp containing a space must be quoted in the shell.

This directory contains the Earth/geospace energetic-particle tools used by AMPS in standalone runs and in SWMF-coupled PT runs. The code supports three main execution paths:

1. `-mode gridless` — backward trajectory tracing with direct magnetic-field evaluation.
2. `-mode 3d` — backward trajectory tracing with magnetic fields stored/interpolated on the AMPS AMR mesh.
3. `-mode 3d_forward` — forward Monte Carlo transport with boundary injection and volumetric AMR sampling.

In SWMF-coupled builds, the same mesh-backed Mode3D backward-product logic can be called from the coupled PT time step using the magnetic field imported from SWMF.

The most important distinction is this:

- `gridless` and mesh-backed `3d` cutoff/density/flux are **backward-access / transmissivity** calculations.
- `3d_forward` is a **forward particle transport / residence-time sampling** calculation.

They answer related but not identical questions.

---

## 1. Source layout

Important source files:

```text
srcEarth/main.cpp
    Standalone CLI dispatch for -mode gridless, -mode 3d, and -mode 3d_forward.

srcEarth/util/amps_param_parser.h
srcEarth/util/amps_param_parser.cpp
    Strict AMPS_PARAM.in parser used by the gridless, Mode3D, and 3d_forward paths.

srcEarth/util/cutoff_cli.h
srcEarth/util/cutoff_cli.cpp
    Command-line option parser and help text.

srcEarth/gridless/CutoffRigidityGridless.cpp
srcEarth/gridless/DensityGridless.cpp
srcEarth/gridless/GridlessParticleMovers.cpp
srcEarth/gridless/AnisotropicSpectrum.cpp
    Direct-field, gridless backward tracing, cutoff, density, spectrum, flux,
    and boundary anisotropy support.

srcEarth/3d/Mode3D.cpp
srcEarth/3d/CutoffRigidityMode3D.cpp
srcEarth/3d/DensityMode3D.cpp
srcEarth/3d/Mode3DParallel.cpp
srcEarth/3d/Mode3DParallel.h
srcEarth/3d/ElectricField.cpp
srcEarth/3d/GlobalMagneticField.cpp
    Mesh-backed 3-D backward products: cutoff, directional cutoff maps,
    density/flux, standalone time snapshots, field initialization, global
    replicated B-field materialization, and the shared OpenMP/THREADS/SERIAL
    backend selector used by both cutoff and density/flux.

srcEarth/3d_forward/Mode3DForward.cpp
srcEarth/3d_forward/ForwardParticleMovers.cpp
srcEarth/3d_forward/Density3D.cpp
srcEarth/3d_forward/SphereFlux3D.cpp
    Standalone forward 3-D particle injection, propagation, cell density sampling,
    and inner-sphere impact flux sampling.

srcEarth/3d_forward_swmf/Mode3DForwardSWMF.cpp
srcEarth/3d_forward_swmf/Mode3DForwardSWMF.h
    SWMF-coupled bridge. In coupled builds it supports both historical forward
    injection and the newer Mode3D backward products driven by live SWMF fields.

parallel_affinity.h
parallel_affinity.cpp
    Main AMPS/PIC affinity helpers under namespace PIC::Parallel. These files
    are not srcEarth-local files; they are intended to be compiled by the main
    AMPS build system and made available through the AMPS include path. The
    Mode3D direct-thread cutoff/density backends call these helpers to widen
    the MPI-rank CPU affinity mask before std::thread workers are created.
```

---

## 2. Execution modes

### 2.1 `-mode gridless`

Run command:

```bash
mpirun -np 8 ./amps -mode gridless -i AMPS_PARAM.in
```

The gridless mode does not build or sample a 3-D magnetic-field mesh. Each trajectory step evaluates the selected background magnetic field directly. This mode is useful for fast access studies, analytic/dipole validation, cutoff calculations, and transmissivity-based density/flux calculations.

Supported main products:

```text
CALC_TARGET CUTOFF_RIGIDITY
CALC_TARGET DENSITY_SPECTRUM
```

The gridless density/flux calculation is a backward-tracing calculation. It does not inject particles forward in time. For each observation point and energy, the code samples arrival directions, backtraces each direction, computes the allowed fraction, multiplies the boundary spectrum by that transmissivity, and integrates the result into density and flux.

Gridless cutoff and gridless density/flux now use the same collective MPI scheduling model as standalone Mode3D. The default scheduler is `DYNAMIC`: all MPI ranks, including rank 0, atomically fetch chunks of global gridless work tasks from an MPI one-sided work queue. For cutoff, a work task is one cutoff-sampling direction or one directional-map cell. For density/flux, a work task is a small block of arrival directions for one fixed observation point and energy. Deterministic fallback schedulers are available for regression runs: `BLOCK_CYCLIC` and `STATIC`.

Typical output files:

```text
cutoff_gridless_points.dat
cutoff_gridless_shells.dat
cutoff_gridless_dir_map_point_####.dat

gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
gridless_shell_<ALT>km_density_channels.dat
```

### 2.2 `-mode 3d`

Run command:

```bash
mpirun -np 8 ./amps -mode 3d -i AMPS_PARAM.in
```

Mode3D uses the AMPS AMR mesh as the field backend. The standalone code initializes the requested background magnetic field on mesh cell centers, then materializes a replicated read-only B-field snapshot on every MPI rank so any rank can backtrace through the whole domain.

By default, owner-rank B/E mesh population remains serial.  Add
`-mode3d-parallel-field-init` to use a temporary POSIX-thread team during this
stage.  The team uses the same per-rank count selected by `-mode3d-threads`
(`-density-threads` is the historical alias).  For example,
`-mode3d-threads 16` creates 16 temporary pthread workers, and the calling
MPI-rank thread evaluates one additional equal share.  The owner-cell range is
divided into equal static shares because background-field evaluation cost is
approximately uniform.

Background-field initialization now reports **global completed owner-cell slots** with
the same visual convention used by the cutoff solver, for example:

```text
[Mode3D field INITIALIZATION] [rank 0/global over 6 MPI ranks] [############------------------------] 33.3%  (Cell 1176/3528)  ETA 00:05:21
```

The field and cutoff bars intentionally use the same 36-character body (`#` =
completed, `-` = remaining), percentage formatting, rank-0/global wording, and ETA
format.  Only MPI rank 0 prints.  Normal field-initialization updates are throttled to
one line every **2 s**, half the output frequency of the one-second cutoff progress
bar, because field initialization can visit a very large number of cell slots.  The
0% and 100% lines are always emitted.  Every line is followed by an explicit
`std::cout.flush()` so progress remains live when stdout passes through `mpirun`, a
batch scheduler, `tee`, or a redirected log.

For pthread field initialization, worker threads update only a rank-local atomic
completion counter.  The original MPI-rank thread periodically publishes the newly
completed delta through the existing MPI-RMA `DynamicMpiProgressCounter`; worker
threads never call MPI.  This preserves compatibility with MPI installations that do
not provide `MPI_THREAD_MULTIPLE`.  If rank 0 finishes its own static share before its
workers or another MPI rank, it continues polling and printing global completion until
the exact 100% count is reached.

Because every rank can access the full replicated mesh field, the standalone backward products use a selectable MPI scheduler over global observation locations. The default `DYNAMIC` scheduler uses an MPI one-sided atomic work queue; ranks fetch chunks of locations as soon as they become idle. `BLOCK_CYCLIC` and `STATIC` remain available for deterministic regression/debug runs.

This mode now supports three product selections from the same prepared mesh-field snapshot:

```text
CALC_TARGET CUTOFF_RIGIDITY
CALC_TARGET DENSITY_SPECTRUM
CALC_TARGET CUTOFF_RIGIDITY+DENSITY_SPECTRUM
```

The aliases below are also accepted by the Mode3D product selector:

```text
CALC_TARGET ALL
CALC_TARGET BOTH
```

Mode3D products:

- cutoff rigidity;
- optional directional cutoff sky maps;
- gridless-style density, local spectrum, and integral flux, but using the meshed magnetic field;
- optional energy-channel integral fluxes;
- standalone single-snapshot runs;
- standalone time-series runs over multiple Tsyganenko-driver snapshots.

The expensive Mode3D backward products can use one of three intra-rank
shared-memory backends, selected by the `#NUMERICAL` keywords
`DENSITY_PARALLEL` and `DENSITY_THREADS` or by the standalone CLI options
`-density-parallel` and `-density-threads`. The names are historical: the
same settings now apply to both `CUTOFF_RIGIDITY` and `DENSITY_SPECTRUM`.
Clearer aliases are also accepted: `MODE3D_PARALLEL`, `MODE3D_THREADS`,
`BACKTRACK_PARALLEL`, and `BACKTRACK_THREADS`; similarly, the CLI accepts
`-mode3d-parallel`, `-mode3d-threads`, `-backtrack-parallel`, and
`-backtrack-threads`.

The same products can also use an inter-rank scheduler selected with
`MODE3D_MPI_SCHEDULER` or the CLI option `-mode3d-mpi-scheduler`. The default
`DYNAMIC` scheduler uses an MPI one-sided atomic work queue, while
`BLOCK_CYCLIC` and `STATIC` are deterministic fallback schedules. The optional
`MODE3D_MPI_DYNAMIC_CHUNK` / `-mode3d-mpi-dynamic-chunk` value controls how many
global observation locations a rank fetches at a time.
### Gridless and Mode3D MPI dynamic scheduler

The same inter-rank scheduler controls gridless and standalone Mode3D backward products:

```text
#NUMERICAL
MODE3D_MPI_SCHEDULER      DYNAMIC        # DYNAMIC | BLOCK_CYCLIC | STATIC
MODE3D_MPI_DYNAMIC_CHUNK  64             # 0 means automatic
```

Gridless-specific aliases are accepted and write to the same internal settings:

```text
#NUMERICAL
GRIDLESS_MPI_SCHEDULER      DYNAMIC
GRIDLESS_MPI_DYNAMIC_CHUNK  0              # recommended: auto-size from GRIDLESS_THREADS
GRIDLESS_PARALLEL           THREADS
GRIDLESS_THREADS            16
```

CLI aliases are also accepted:

```bash
mpirun -np 8 ./amps -mode gridless -i AMPS_PARAM.in \
  -gridless-mpi-scheduler DYNAMIC \
  -gridless-parallel THREADS -gridless-threads 16
```

`DYNAMIC` uses `MPI_Fetch_and_op` on a rank-0-owned counter. MPI calls are made only by the rank/main thread; worker threads do not call MPI, so the implementation does not require `MPI_THREAD_MULTIPLE`. For Mode3D cutoff and gridless cutoff, the chunk unit is a flattened trajectory task (primary cutoff, directional-map cell, or direct rigidity-access entry as applicable). For density/flux the unit follows that solver's location/direction work decomposition. Smaller chunks improve load balance for highly variable trajectories; larger chunks reduce MPI scheduling overhead. For threaded GRIDLESS cutoff, `GRIDLESS_MPI_DYNAMIC_CHUNK 0` is preferred because the resolver sizes the fetch from `GRIDLESS_THREADS` (currently about four tasks per worker). Explicit values such as 32, 64, or 128 remain useful for performance studies.

Gridless cutoff and gridless density/flux also keep a live rank-0 progress bar in MPI mode. Because the collective scheduler no longer sends every result through rank 0, progress is tracked with a second MPI one-sided counter that records completed tasks, not assigned chunks. Rank 0 periodically reads that counter. For threaded gridless cutoff the counter is polled frequently, but stdout is deliberately quieter: unchanged counts are suppressed, routine lines are emitted at roughly 0.1% completed-task increments (or after 10 s when at least one new task has finished), startup ETA is withheld until enough completed work exists, and the final 100% line is printed once. The progress unit is therefore the scheduler task: cutoff direction/directional-map task for gridless cutoff, and direction-block task for gridless density/flux.


Typical output files for a single snapshot:

```text
cutoff_3d_points.dat
cutoff_3d_shells.dat
cutoff_3d_dir_map_loc_000000.dat
cutoff_3d_dir_map_loc_000001.dat

mode3d_points_density.dat
mode3d_points_spectrum.dat
mode3d_points_flux.dat
mode3d_shell_<ALT>km_density_flux.dat
```

For time-series standalone runs, a snapshot suffix is inserted before `.dat`, for example:

```text
cutoff_3d_points_snapshot_000000_2024_05_10T00_00_00_000.dat
mode3d_points_density_snapshot_000000_2024_05_10T00_00_00_000.dat
mode3d_points_spectrum_snapshot_000000_2024_05_10T00_00_00_000.dat
mode3d_points_flux_snapshot_000000_2024_05_10T00_00_00_000.dat
cutoff_3d_dir_map_loc_000000_snapshot_000000_2024_05_10T00_00_00_000.dat
```

### 2.3 `-mode 3d_forward`

Run command:

```bash
mpirun -np 8 ./amps -mode 3d_forward -i AMPS_PARAM.in
```

The forward 3-D mode is a particle-in-cell style Monte Carlo calculation. Particles are injected at the outer rectangular boundary, propagated forward in time through the configured B/E fields, and sampled in AMR cells and on the inner absorbing sphere.

This path is not the same as the gridless/Mode3D transmissivity density/flux calculation.

Main products:

- AMR volumetric sampled density in energy bins;
- inner-sphere incident flux maps;
- optional initialized field mesh dump;
- optional particle trajectory records when AMPS is compiled with particle tracking.

Typical output files include AMPS mesh data files plus:

```text
sphere_flux3d.out=####.dat
sphere_flux3d_total.out=####.dat
amps_3dforward_initialized.data.dat     # only with -mode3d-output-initialized
```

The cell-density variables are written into the standard AMPS mesh output by `Density3D.cpp` as variables like:

```text
n_dens[Elo-Ehi_MeV]_m3
n_dens_total_m3
```

---

## 3. SWMF-coupled behavior

In a SWMF-coupled build, the PT component does not normally enter through `./amps -mode 3d`. Instead, the SWMF/PT execution path calls the hooks in:

```text
srcEarth/3d_forward_swmf/Mode3DForwardSWMF.cpp
```

The coupled bridge has two behaviors.

### 3.1 Historical coupled forward mode

If `CALC_TARGET` is not explicitly set to a backward product, the coupled path remains the historical `3d_forward` style mode: particles are injected and propagated forward using fields received from SWMF.

This behavior is preserved intentionally for backward compatibility.

### 3.2 Coupled Mode3D backward-product mode

If `#CALCULATION_MODE / CALC_TARGET` explicitly requests cutoff and/or density/flux, the coupled bridge switches to mesh-backed backward products:

```text
CALC_TARGET CUTOFF_RIGIDITY
CALC_TARGET DENSITY_SPECTRUM
CALC_TARGET CUTOFF_RIGIDITY+DENSITY_SPECTRUM
CALC_TARGET ALL
```

The coupled backward-product path uses the same Mode3D shared-memory backend
as standalone Mode3D. In particular, `DENSITY_PARALLEL THREADS` enables
direct `std::thread` workers for cutoff and density calculations inside each
MPI rank, and `DENSITY_THREADS` sets the worker count.

At every accepted coupled snapshot, the bridge:

1. uses the current SWMF/PT magnetic field in the AMPS coupler buffers;
2. materializes the global cell-centered B field on every MPI rank;
3. runs the requested Mode3D backward products;
4. writes files with a SWMF simulation-time suffix.

Example coupled output names:

```text
cutoff_3d_points.swmf_n000000_t000000000.000s.dat
mode3d_points_density.swmf_n000000_t000000000.000s.dat
mode3d_points_spectrum.swmf_n000000_t000000000.000s.dat
mode3d_points_flux.swmf_n000000_t000000000.000s.dat
amps_coupled_data.swmf_n000000_t000000000.000s.dat
```

The coupled cadence is controlled by:

```text
#TEMPORAL
FIELD_UPDATE_DT  <minutes, floating point allowed>
```

In the coupled case, `FIELD_UPDATE_DT` means: run the expensive backward products approximately every requested number of minutes of SWMF/PT simulation time, using `PIC::SimulationTime::TimeCounter` for the file stamp. Fractional minutes are accepted, for example `FIELD_UPDATE_DT 0.5` requests a 30-second cadence.

---

## 4. Algorithms

### 4.1 Backward trajectory classifier

Both gridless and Mode3D backward products use the same physical classification:

```text
Launch time-reversed particle from observation point x0.

If the trajectory exits the outer domain box first:
    direction is ALLOWED.

If the trajectory hits the inner absorbing sphere first:
    direction is FORBIDDEN.

If the trajectory exceeds max time, max steps, or max trace distance:
    structured cutoff/access paths classify it according to CUTOFF_TRACE_LIMIT_POLICY;
    the current/default UNRESOLVED policy preserves it as numerically UNRESOLVED rather
    than turning a safety cap into physical shielding. Legacy Boolean UPPER_SCAN paths
    may use the explicit FORBIDDEN convention when requested.
```

The outer domain is defined by `#DOMAIN_BOUNDARY`. The inner loss sphere is defined by `R_INNER`.

### 4.2 Cutoff rigidity

For each observation point and arrival direction, the solver searches for the transition between forbidden and allowed access over the rigidity/energy range specified in `#CUTOFF_RIGIDITY`.

For `CUTOFF_SAMPLING VERTICAL`, one local vertical direction is tested.

For `CUTOFF_SAMPLING ISOTROPIC`, the code samples multiple directions and reports an effective/minimum cutoff for the location.

Optional directional sky maps are controlled independently by:

```text
DIRECTIONAL_MAP T
DIRMAP_LON_RES  <deg>
DIRMAP_LAT_RES  <deg>
DIRMAP_COVERAGE FULL_SPHERE | VECTOR_APERTURES
DIRMAP_APERTURE_FILE <file>
DIRMAP_APERTURE <name> <SM|GSM|LOCAL_SM> bx by bz ux uy uz hHalfDeg vHalfDeg [LOCATION=<index>]
```

`FULL_SPHERE` is the backward-compatible default. `VECTOR_APERTURES` keeps the same
regular SM lon/lat lattice but schedules only cells whose detector **LOOK** direction is
inside one or more configured elliptical apertures. There is no mission-specific
EAST/WEST assumption: boresights may point anywhere and need not be antipodal. The AMPS
directional vector is the incoming particle **arrival** direction, so aperture membership
is evaluated with `look = -arrival`.

Apertures can be supplied inline with repeatable `DIRMAP_APERTURE` records or in a file
named by `DIRMAP_APERTURE_FILE`. File rows have the same fields but omit the keyword:

```text
name frame bx by bz ux uy uz hHalfDeg vHalfDeg [LOCATION=<index>]
```

`SM` and `GSM` are global vector frames. `LOCAL_SM` means vector components in the local
`(radial,east,north)` basis at the observation point. The user-supplied up/roll vector
is projected perpendicular to the boresight before constructing the horizontal axis.
Retained cells keep their original full-sphere cell IDs and lon/lat coordinates, so the
optimization changes only the amount of work, not the sampled directions.

The optional zero-based `LOCATION=<index>` token associates an aperture with one row
of a multi-location trajectory. It is used by `SNAPSHOT_LIST` batches so an aperture
is resolved only in the local basis of its own observation. Omitting the token keeps
the historical behavior and applies that aperture at every location.

For standalone Mode3D, location-qualified aperture coverage is now **truly
location-aware**. Internally the solver forms a compact union of all retained sky cells
only as a common storage/MPI-reduction coordinate, but it also retains a per-location
index list into that union. The flattened scheduler visits only the cells owned by the
current location, and each directional Tecplot file writes only those cells. Thus a
GOES-13 location no longer traces or displays GOES-15-only aperture directions in the
same batched snapshot. The change removes the old Cartesian-product overhead while
preserving the exact full-sphere cell IDs and lon/lat values of every retained sample.

The same controls are available on the AMPS command line:

```text
-cutoff-dirmap-coverage FULL_SPHERE|VECTOR_APERTURES
-cutoff-dirmap-aperture-file <file>
-cutoff-dirmap-aperture "<name> <frame> bx by bz ux uy uz hHalfDeg vHalfDeg"
```

The last option is repeatable. A CLI aperture-file path overrides the input-file path
before either file is opened, allowing one generic parameter deck to be reused with
per-epoch attitude files.

Directional maps write one Tecplot file per observation location. Direction labels follow the gridless convention: SM lon/lat when SPICE is available for SM-to-GSM rotation; GSM fallback when SPICE is unavailable. Aperture-limited maps are written as compact POINT zones with explicit lon/lat columns rather than as rectangular I/J zones.

### 4.3 Density, local spectrum, and integral flux

The density/flux calculation in `gridless` and mesh-backed `3d` is transmissivity-based.

For each observation point and energy:

```text
T(E; x0) = allowed_direction_weight / total_direction_count
J_local(E; x0) = T(E; x0) * J_boundary(E)
```

For isotropic boundary mode, each allowed direction has unit weight.

For anisotropic boundary mode, each allowed trajectory is weighted using the boundary pitch-angle and spatial factors evaluated at the outer-boundary exit point.

The omnidirectional number density and flux are then integrated as:

```text
n(x0) = 4*pi * integral[ J_local(E; x0) / v(E) dE ]
F(x0) = 4*pi * integral[ J_local(E; x0) dE ]
```

Additional user-defined energy channels can be integrated using `#ENERGY_CHANNELS`.

#### Transmission-grid option for density/flux

The density/flux calculation must resolve the transmission function, not just a scalar cutoff.  In geomagnetic access calculations the natural independent variable is rigidity, and the allowed/forbidden pattern can contain penumbra intervals.  The cutoff solver handles this with an upper-cutoff rigidity scan.  Density/flux now has the analogous option at the transmissivity level:

```text
DS_TRANSMISSION_MODE     DIRECT | SCAN | ADAPTIVE
DS_TRANSMISSION_SCAN_N   <int>
DS_TRANSMISSION_REFINE_N <int>   # parsed/reserved for future per-location refinement
DS_TRANSMISSION_MAX_N    <int>   # optional cap on scan nodes
DS_TRANSMISSION_SAVE     T|F     # parsed/provenance flag; spectra already save T(E)
```

`DIRECT` is the legacy behavior: `T(E)` is evaluated on the user energy grid defined by `DS_EMIN`, `DS_EMAX`, `DS_NINTERVALS`, and `DS_ENERGY_SPACING`.

`SCAN` builds a log-spaced grid in rigidity between the rigidities corresponding to `DS_EMIN` and `DS_EMAX`, converts each rigidity node back to kinetic energy, and evaluates `T(E)` on that grid before the spectrum, density, and flux integrals are performed.  This is more appropriate near cutoffs because particle access is controlled by `R = pc/|q|`, while the boundary spectrum is tabulated in energy.

`ADAPTIVE` is currently implemented as the same fixed log-rigidity scan as `SCAN`, with `DS_TRANSMISSION_REFINE_N` parsed and documented for future per-location interval refinement.  The fixed scan was chosen first because all output locations then share the same energy axis, which keeps Tecplot output, MPI reductions, and regression comparisons deterministic.

The output density files now also include diagnostic columns derived from the saved transmission curve:

```text
Rc_lower_GV
Rc_effective_GV
Rc_upper_GV
PenumbraWidth_GV
T_high
```

These values are diagnostics only.  The flux and density integrals use the full sampled `T(E)` curve.  `Rc_effective_GV` is computed as the sharp-step rigidity that blocks the same normalized area under `1 - T(R)/T_high`; this is useful for comparing density/flux behavior with cutoff maps while retaining the full transmission function for the actual calculation.

Supporting references and motivation:

- Smart and Shea / Cooke et al. cutoff-rigidity terminology defines lower, upper, and effective cutoffs and emphasizes penumbra/transmission behavior rather than a single universal cutoff.
- Bobik et al. (GeoMag/HelMod, 2013) describe Lorentz backtracing with IGRF plus Tsyganenko T96/T05, classifying trajectories by outer/inner boundary access and estimating rigidity cutoffs.
- Boschini et al. (2013) use backtracing with T96/T05 to reconstruct asymptotic directions and access for AMS-02, including East-West and active/quiet comparisons.
- PAMELA SEP analyses use trajectory reconstruction/asymptotic exposure to infer pitch-angle-dependent SEP spectra and geomagnetic cutoff behavior.

### 4.4 3-D forward Monte Carlo transport

The forward mode uses a different algorithm:

```text
For each forward iteration:
    inject particles through the outer box boundary;
    sample energy from the configured injection distribution;
    sample inward velocity direction;
    assign statistical weight;
    move particles forward through B/E fields;
    accumulate cell residence density and inner-sphere impact flux.
```

This is useful for explicit forward propagation and residence-time diagnostics. It is not a direct replacement for the transmissivity-based density/flux calculation.

---

## 5. Magnetic and electric fields

### 5.1 Gridless magnetic fields

The gridless branch evaluates magnetic fields directly. The source supports:

```text
DIPOLE
T96
T05
```

### 5.2 Standalone Mode3D magnetic fields

Standalone Mode3D initializes the magnetic field on the AMPS AMR mesh. Supported field models include:

```text
DIPOLE
T96
T05
TA16
```

The field is first written into owner-rank DATAFILE cell buffers. Then `GlobalMagneticField::AssembleCellCenteredFieldsForCutoff()` resets and assigns dense `node->Temp_ID` values, gathers owner-cell B/E values into compact global arrays, and verifies exactly one owner contribution per physical cell. No nonlocal AMR blocks or ghost-cell state vectors are allocated. Field evaluation uses the decomposition-independent AMPS row stencil.

### 5.3 SWMF-coupled Mode3D magnetic fields

In coupled mode, the field comes from the SWMF coupler data buffer, not from standalone Tsyganenko/DIPOLE initialization.

The same compact global-field helper is used. B is read from `PIC::CPLR::SWMF::MagneticFieldOffset`; E is reconstructed from `PIC::CPLR::SWMF::BulkVelocityOffset` as `E = -v x B`.

### 5.4 Electric field options

The standalone 3-D field initializer can also populate cell-centered electric fields. Recognized model names include:

```text
NONE
COROTATION
VOLLAND_STERN
COROTATION_VOLLAND_STERN
```

The backward cutoff/density access problem is primarily a magnetic-field access calculation. The forward transport path can use the configured electric field in particle motion.

---

## 6. MPI model

### 6.1 Gridless

Gridless mode does not need a field mesh. Work is distributed over observation locations, energies, and/or directions. The magnetic field is evaluated directly along each trajectory.

### 6.2 Standalone Mode3D

Standalone Mode3D no longer uses independent private MPI domains for cutoff calculations. It uses the normal distributed AMPS mesh initialization and then assembles compact global B/E arrays for tracing.

The intended sequence is:

```text
PIC::InitMPI()
amps_init_mesh()
amps_init()
InitMeshFields()
GlobalMagneticField::AssembleCellCenteredFieldsForCutoff()
RunCutoffRigidity() and/or RunDensityAndFlux()
```

This gives all ranks access to global B/E values through row-stencil interpolation while preserving the normal AMPS block decomposition and avoiding replicated blocks.

### 6.3 SWMF coupled

In coupled mode, compact B/E assembly is repeated for every accepted SWMF/PT snapshot before the backward products are computed.

### 6.4 Intra-rank shared-memory backends for Mode3D backward products

Mode3D cutoff and density/flux products can use three shared-memory backends
inside each MPI rank:

```text
DENSITY_PARALLEL SERIAL
DENSITY_PARALLEL OPENMP
DENSITY_PARALLEL THREADS

# Equivalent clearer aliases:
MODE3D_PARALLEL   THREADS
BACKTRACK_THREADS 8
```

Despite the `DENSITY_` prefix, these settings control both the cutoff and
density/flux backward products. The prefix is kept for compatibility with
older input files and command-line options.

`SERIAL` disables intra-rank parallelism. MPI decomposition over locations is
still used.

`OPENMP` preserves the OpenMP implementation. For density it parallelizes the
energy/direction work where enabled. For cutoff it uses OpenMP over the local
observation-location loop. This backend is useful when the OpenMP runtime and
MPI launcher are known to place threads correctly.

`THREADS` uses direct `std::thread` workers over local observation locations.
The worker scheduling is dynamic inside each MPI rank: a single thread-safe
atomic counter stores the next local location to process, and every worker calls
`fetch_add()` to grab another location as soon as it becomes idle. This avoids
static per-thread chunks, which are inefficient for cutoff calculations because
near-cutoff trajectories can take much longer than quickly escaping or quickly
forbidden trajectories. The direct-thread backend intentionally suppresses
nested OpenMP inside those workers to avoid oversubscription. This backend is
often preferable when OpenMP interpolation stencil state or MPI/OpenMP placement
is problematic.

The worker count is controlled by:

```text
DENSITY_THREADS <N>
```

or by the standalone CLI option:

```bash
-density-threads <N>
```

`N > 1` requests that many workers per MPI rank. `N = 0` asks the code to
choose automatically from environment/runtime information. For production
runs, the number of MPI ranks per node times `DENSITY_THREADS` should not
exceed the number of CPU cores allocated on that node.

For the direct-thread backend, the code calls:

```cpp
PIC::Parallel::SetWideAffinityForScheduler();
```

before the cutoff or density worker pool is created. This reproduces the
manual operation:

```bash
taskset -apc <CPUSET> <rank_pid>
```

for each MPI rank. It does not pin individual worker threads to fixed cores;
it widens the allowed CPU mask of the rank so the Linux scheduler can move
those threads among the allowed CPUs. By default the CPU set is read from:

```text
/sys/devices/system/cpu/online
```

The automatic CPU set can be overridden with either environment variable:

```bash
export AMPS_MODE3D_DENSITY_CPUSET=0-39
export PIC_PARALLEL_CPUSET=0-39
```

If the batch scheduler or MPI runtime restricts a rank to one CPU with a
cgroup/cpuset, affinity widening cannot escape that restriction. The actual
mask can be checked at runtime from the job log through the diagnostic printed
by `PIC::Parallel::PrintCurrentAffinity()`, or manually with:

```bash
grep Cpus_allowed_list /proc/<pid>/status
ps -L -p <pid> -o pid,tid,psr,pcpu,state,comm
```

---

## 7. Time dependence and snapshots

### 7.1 Single-snapshot standalone run

By default, standalone Mode3D uses one magnetic-field snapshot at:

```text
#BACKGROUND_FIELD
EPOCH <UTC>
```

If a driver file is present but `TEMPORAL_MODE` remains `SNAPSHOT`, the table is sampled once at the background-field epoch.

### 7.2 Standalone time-series run

Standalone Mode3D can loop through multiple background-field snapshots using a Tsyganenko driver table. The table can be specified in either of two ways.

Option A:

```text
#TEMPORAL
TS_INPUT_MODE FILE
TS_INPUT_FILE  ts_driving_file.txt
```

Option B:

```text
#BACKGROUND_FIELD
DRIVER_FILE    ts_driving_file.txt
```

Then request a time series:

```text
#TEMPORAL
TEMPORAL_MODE   TIME_SERIES
EVENT_START     2024-05-10T00:00:00
EVENT_END       2024-05-10T12:00:00
FIELD_UPDATE_DT 15
```

For each snapshot, the code interpolates the driver table to the requested epoch, updates the background-field parameters, rebuilds/materializes the mesh field, and runs the requested products.

SPICE must be available for time-series operation because UTC strings and driver-table timestamps are converted to ephemeris time.

### 7.3 Explicit irregular snapshot list and mesh reuse

Standalone Mode3D can process either independent timestamped trajectory cases or a
fixed shell domain at irregular epochs while retaining one allocated AMR mesh:

```text
#TEMPORAL
TEMPORAL_MODE       SNAPSHOT_LIST
SNAPSHOT_LIST_FILE  snapshot_epochs.txt

#OUTPUT_DOMAIN
OUTPUT_MODE         TRAJECTORY
TRAJ_FILE           observation_locations.txt
```

For a static shell campaign, replace the output block with, for example:

```text
#OUTPUT_DOMAIN
OUTPUT_MODE         SHELLS
SHELL_COUNT         2
SHELL_ALTS_KM       475 850
SHELL_LON_RES_DEG   15
SHELL_LAT_RES_DEG   2
```

`snapshot_epochs.txt` contains one ISO-8601 UTC epoch per non-comment line. Epochs are
validated, sorted, and deduplicated. For each epoch Mode3D interpolates the driver,
refills B/E on the existing distributed blocks, and runs the requested products.
For TRAJECTORY it also selects only samples with that timestamp and remaps any
LOCATION-qualified apertures. For SHELLS the complete fixed shell geometry is
evaluated at every epoch and each output receives an index/UTC filename suffix.
`amps_init_mesh()`, `amps_init()`, and sphere/static-data setup occur only once for
the complete list.

This mode deliberately differs from `TIME_SERIES`: the latter evaluates the complete
output domain at every regular field snapshot, while `SNAPSHOT_LIST` represents
independent observations and prevents an `N_snapshot x N_location` cross-product for
TRAJECTORY. Every listed trajectory epoch must have at least one matching sample.
SHELLS intentionally forms an `N_snapshot x N_shell` product. In both cases the mesh
topology is reused, but the field values are rebuilt at every distinct epoch.

Existing `SNAPSHOT`, `TIME_SERIES`, POINTS/SHELLS, GRIDLESS, and coupled execution are
unchanged unless `TEMPORAL_MODE SNAPSHOT_LIST` is explicitly requested.

### 7.4 SWMF-coupled time dependence

In coupled mode, the magnetic-field snapshots come from SWMF itself. `FIELD_UPDATE_DT` is used as the calculation cadence, not as a request to load a Tsyganenko file.

---

## 8. AMPS_PARAM.in sections

The parser is strict: unknown sections or unknown keywords are fatal. This is intentional so typos do not silently produce wrong physics.

### 8.1 `#RUN_INFO`

```text
#RUN_INFO
RUN_ID        my_run_name
PI_NAME       Dr. Jane Smith      # optional provenance metadata
PI_EMAIL      j.smith@example.edu # optional provenance metadata
INSTITUTION   Example Institute   # optional provenance metadata
SCIENCE_GOAL  storm_validation    # optional provenance metadata
```

`RUN_ID` is a user label. The optional metadata keys are accepted for CCMC/Runs-on-Request compatibility and provenance; they do not by themselves alter the calculation.

### 8.2 `#CALCULATION_MODE`

```text
#CALCULATION_MODE
CALC_TARGET        CUTOFF_RIGIDITY
FIELD_EVAL_METHOD  GRIDLESS
```

Supported `CALC_TARGET` values:

```text
CUTOFF_RIGIDITY
DENSITY_SPECTRUM
CUTOFF_RIGIDITY+DENSITY_SPECTRUM
ALL
BOTH
DENSITY_3D      # forward mode validation path
```

Recommended `FIELD_EVAL_METHOD` values:

```text
GRIDLESS   # direct field evaluation
GRID_3D    # mesh-backed standalone 3D
SWMF       # set internally in SWMF-coupled mode
```

Notes:

- `-mode gridless` with density requires `FIELD_EVAL_METHOD GRIDLESS`.
- `-mode 3d` uses the mesh-backed Mode3D driver even if the field method label is not the only selector.
- In SWMF-coupled backward-product mode, `CALC_TARGET` must be explicit; otherwise the bridge preserves historical forward-injection behavior.

### 8.3 `#PARTICLE_SPECIES`

```text
#PARTICLE_SPECIES
SPECIES_NAME       PROTON
SPECIES_CHARGE     1
SPECIES_MASS_AMU   1.0073
```

Aliases are also accepted:

```text
SPECIES
CHARGE
MASS_AMU
```

### 8.4 `#BACKGROUND_FIELD`

```text
#BACKGROUND_FIELD
FIELD_MODEL    T05
EPOCH          2024-05-10T00:00:00
PDYN           2.0
DST            -20.0
IMF_BX         0.0
IMF_BY         0.0
IMF_BZ         -5.0
SW_VX          -400.0
SW_N           5.0
T05_W1         0.0
T05_W2         0.0
T05_W3         0.0
T05_W4         0.0
T05_W5         0.0
T05_W6         0.0
```

Common model-specific keys:

```text
DIPOLE_MOMENT
DIPOLE_TILT or DIPOLE_TILT_DEG
G1 G2 G3
W1..W6 or T05_W1..T05_W6 or TS05_W1..TS05_W6
BZ1..BZ6 or TA15_BZ1..TA15_BZ6 or TA16_BZ1..TA16_BZ6
XIND or TA15_XIND
TA16_COEFF_FILE
DRIVER_FILE or MAGNETIC_DRIVER_FILE
```

Model aliases like `TS05` are normalized to the canonical names used internally.

For gridless density/flux validation, `FIELD_MODEL NONE` is also supported. It
returns a zero magnetic field and skips Geopack/Tsyganenko initialization; this
mode is intended for normalization tests such as F1 rather than physical
geospace production runs.

CCMC/Runs-on-Request style `TS05_*` and `T05_*` aliases are accepted for the same background drivers:

```text
TS05_DST  or T05_DST   -> DST
TS05_PDYN or T05_PDYN  -> PDYN
TS05_BX   or T05_BX    -> IMF_BX
TS05_BY   or T05_BY    -> IMF_BY
TS05_BZ   or T05_BZ    -> IMF_BZ
TS05_VX   or T05_VX    -> SW_VX
TS05_NSW  or T05_NSW   -> SW_N
```

Generic aliases such as `BYIMF`, `BZIMF`, `BXIMF`, `VSW`, `NSW`, and `DEN_P` are also accepted where applicable.

### 8.5 `#ELECTRIC_FIELD`

```text
#ELECTRIC_FIELD
EFIELD_MODEL       NONE
COROTATION_SCALE   1.0
VS_POTENTIAL_KV    60.0
VS_GAMMA           2.0
VS_REFERENCE_L     10.0
VS_SCALE           1.0
EFIELD_RMIN        20.0
EFIELD_LMIN        1.0e-3
```

`EFIELD_MODEL` values:

```text
NONE
COROTATION
VOLLAND_STERN
COROTATION_VOLLAND_STERN
```

### 8.6 `#DOMAIN_BOUNDARY`

```text
#DOMAIN_BOUNDARY
DOMAIN_XMIN   -35 Re
DOMAIN_XMAX    35 Re
DOMAIN_YMIN   -35 Re
DOMAIN_YMAX    35 Re
DOMAIN_ZMIN   -35 Re
DOMAIN_ZMAX    35 Re
R_INNER        1 Re
```

Lengths can be provided using inline units where supported by the parser, for example `Re` or `km`. The solver internally uses SI meters.

The outer box defines escape. `R_INNER` defines the absorbing loss sphere.

Additional CCMC/Runs-on-Request boundary keywords are accepted by the parser:

```text
BOUNDARY_TYPE   BOX | SHUE
SHUE_R0         AUTO or numeric token
SHUE_ALPHA      AUTO or numeric token
DOMAIN_X_TAIL   <length>   # mapped to DOMAIN_XMIN with nightside sign
```

In the current standalone Mode3D implementation, `SHUE_*` values are stored for provenance/future use; the active trajectory-classification boundary remains the parsed Mode3D box/cap unless a Shue-boundary implementation is added later.

### 8.7 `#OUTPUT_DOMAIN`

POINTS example:

```text
#OUTPUT_DOMAIN
OUTPUT_MODE  POINTS
COORDS       GSM
POINTS_BEGIN
  POINT 1.1 Re 0.0 0.0
  POINT 2.0 Re 0.0 0.0
POINTS_END
```

SHELLS example:

```text
#OUTPUT_DOMAIN
OUTPUT_MODE      SHELLS
SHELL_ALTS_KM    500 9000
SHELL_RES_DEG    10
```

TRAJECTORY example:

```text
#OUTPUT_DOMAIN
OUTPUT_MODE  TRAJECTORY
TRAJ_FRAME   GSM
TRAJ_FILE    spacecraft_path.txt
FLUX_DT      1.0
```

Supported output modes depend on the solver:

```text
gridless cutoff:        POINTS, TRAJECTORY, SHELLS
gridless density/flux:  POINTS, TRAJECTORY, SHELLS
Mode3D cutoff:          POINTS, TRAJECTORY, SHELLS
Mode3D density/flux:    POINTS, TRAJECTORY, SHELLS
3d_forward:             AMR mesh sampling + inner sphere; not POINTS/SHELLS transmissivity
```

Coordinate note: the trajectory and newer parser paths can convert some trajectory frames to GSM when SPICE is available, but the physical solvers operate in GSM. Verify coordinate conventions before using non-GSM inputs.

### 8.8 `#NUMERICAL`

```text
#NUMERICAL
DT_TRACE             1.0
MAX_STEPS            300000
MAX_TRACE_TIME       7200.0
MAX_TRACE_DISTANCE   0.0
MODE3D_PARALLEL           THREADS
MODE3D_THREADS            8
MODE3D_MPI_SCHEDULER      DYNAMIC
MODE3D_MPI_DYNAMIC_CHUNK  64
```

Meanings:

```text
DT_TRACE                  initial trajectory time step [s]
MAX_STEPS                 maximum integration steps per trajectory
MAX_TRACE_TIME            maximum integration time per trajectory [s]
MAX_TRACE_DISTANCE        cumulative path-length cap [Re]; <=0 disables it
MODE3D_PARALLEL           SERIAL, OPENMP, or THREADS for Mode3D cutoff/density products
MODE3D_THREADS            shared-memory worker count per MPI rank; 0 means automatic
MODE3D_MPI_SCHEDULER      DYNAMIC, BLOCK_CYCLIC, or STATIC inter-rank scheduler
MODE3D_MPI_DYNAMIC_CHUNK  global locations per MPI atomic fetch; 0 means automatic
```

`MODE3D_PARALLEL` and `MODE3D_THREADS` are the preferred names for the
shared-memory backend. The historical aliases `DENSITY_PARALLEL` and
`DENSITY_THREADS` still work and apply to both Mode3D cutoff and density/flux
calculations. In SWMF-coupled backward-product runs these keywords can be used in
the PT `AMPS_PARAM.in` to select direct-thread or OpenMP execution without
standalone CLI options.

`MODE3D_MPI_SCHEDULER` controls load balance between MPI ranks. `DYNAMIC` is the
default and recommended setting for shell cutoff maps because trajectory cost is
not uniform in longitude/latitude: each rank atomically fetches a chunk of global
locations, processes it, and then fetches the next available chunk. `BLOCK_CYCLIC`
reproduces the previous deterministic partition `rank r = r, r+nRanks, ...`.
`STATIC` assigns one contiguous slab per rank and is mainly for regression tests.
`MODE3D_MPI_DYNAMIC_CHUNK` trades load balance against scheduler overhead; values
near `2–8 * MODE3D_THREADS` are usually reasonable starting points.

Additional CCMC/Runs-on-Request compatibility keys are accepted here:

```text
N_PARTICLES      # stored; also maps to FORWARD_N_PARTICLES for 3d_forward
MAX_BOUNCE       # stored for provenance; not an active Mode3D backtracking limit
PITCH_ISOTROPIC  # stored for provenance; active angular controls are CUTOFF_SAMPLING, DS_BOUNDARY_MODE, and #BOUNDARY_ANISOTROPY
```

Forward-mode controls also accepted here:

```text
FORWARD_N_PARTICLES
FORWARD_N_ITERATIONS
FORWARD_INJECTION_ENERGY
FORWARD_INJECTION_ENERGY_DISTRIBUTION
FORWARD_ENERGY_SAMPLING
FORWARD_INJECTION_SCHEME
```

The parser recognizes forward mover keywords for future use, but the active `3d_forward` mover is currently selected by the command line or falls back to `BORIS`.

### 8.9 `#CUTOFF_RIGIDITY`

```text
#CUTOFF_RIGIDITY
CUTOFF_EMIN            1.0
CUTOFF_EMAX            20000.0
CUTOFF_NENERGY         50
CUTOFF_MAX_PARTICLES   500
CUTOFF_MAX_TRAJ_TIME   60.0
CUTOFF_SEARCH_ALGORITHM UPPER_SCAN
CUTOFF_UPPER_SCAN_N    50
# Mode3D-only alternative: direct states at explicit rigidity values and a
# selected shell latitude band. These controls are ignored by other algorithms.
CUTOFF_RIGIDITY_LIST_GV 0.424,0.498,0.588,0.693,0.817,0.962,1.131
CUTOFF_ACCESS_ABS_LAT_MIN 35
CUTOFF_ACCESS_ABS_LAT_MAX 75
CUTOFF_BACKTRACE_CHARGE SAME
CUTOFF_TRACE_LIMIT_POLICY UNRESOLVED
CUTOFF_SAMPLING        ISOTROPIC
DIRECTIONAL_MAP        T
DIRMAP_LON_RES         30
DIRMAP_LAT_RES         30

# Optional one-point allowed(R) debug scan for dipole/Störmer tests
CUTOFF_DEBUG_RIGIDITY_SCAN T
CUTOFF_DEBUG_SCAN_LON      0.0
CUTOFF_DEBUG_SCAN_LAT     -40.0
CUTOFF_DEBUG_SCAN_ALT    9000.0
CUTOFF_DEBUG_SCAN_N        40
CUTOFF_DEBUG_SCAN_FILE     cutoff_3d_debug_latm40.dat

# Optional trajectory-exit diagnostic.  Either use one selected point, or
# provide CUTOFF_DEBUG_EXIT_LIST_FILE to trace many trajectories in one run.
CUTOFF_DEBUG_EXIT_TRACE      T
CUTOFF_DEBUG_EXIT_LON        0.0
CUTOFF_DEBUG_EXIT_LAT       -60.0
CUTOFF_DEBUG_EXIT_ALT      9000.0
CUTOFF_DEBUG_EXIT_R_GV      -1.0
CUTOFF_DEBUG_EXIT_N          40
CUTOFF_DEBUG_EXIT_LIST_FILE  c4_debug_trajectories.dat
CUTOFF_DEBUG_EXIT_FILE       cutoff_3d_debug_exit_trace.dat
```

Meanings:

```text
CUTOFF_EMIN / CUTOFF_EMAX     energy/range bounds [MeV/n]
CUTOFF_NENERGY                number of samples used by the cutoff search
CUTOFF_MAX_PARTICLES          direction/sample cap per location
CUTOFF_MAX_TRAJ_TIME          cutoff-specific trajectory time cap [s]
CUTOFF_SEARCH_ALGORITHM       UPPER_SCAN (default), PENUMBRA_SCAN, RIGIDITY_LIST, or BINARY
CUTOFF_UPPER_SCAN_N           samples for UPPER_SCAN/PENUMBRA_SCAN; 0/omitted uses CUTOFF_NENERGY
CUTOFF_RIGIDITY_LIST_GV       positive, strictly increasing explicit rigidity values [GV]
CUTOFF_ACCESS_ABS_LAT_MIN/MAX Mode3D RIGIDITY_LIST geodetic |latitude| launch band [deg]
CUTOFF_BACKTRACE_CHARGE       SAME (default) or REVERSED
CUTOFF_TRACE_LIMIT_POLICY     UNRESOLVED (default) or FORBIDDEN
CUTOFF_SAMPLING               VERTICAL or ISOTROPIC
DIRECTIONAL_MAP               write directional sky-map products
DIRMAP_LON_RES/LAT_RES        underlying regular sky-map angular resolution [deg]
DIRMAP_COVERAGE               FULL_SPHERE (default) or VECTOR_APERTURES
DIRMAP_APERTURE_FILE          optional file of arbitrary detector-look apertures
DIRMAP_APERTURE               repeatable inline aperture: name frame b[3] up[3] hHalf vHalf
CUTOFF_DEBUG_RIGIDITY_SCAN    enable one-point TraceAllowed3D(R) diagnostic
CUTOFF_DEBUG_SCAN_LON/LAT/ALT selected spherical-shell point for the diagnostic
CUTOFF_DEBUG_SCAN_N           number of log-spaced R samples; landmarks are added
CUTOFF_DEBUG_SCAN_FILE        diagnostic output file name
CUTOFF_DEBUG_EXIT_TRACE       enable trajectory-exit diagnostic
CUTOFF_DEBUG_EXIT_LON/LAT/ALT selected spherical-shell point for legacy one-point mode
CUTOFF_DEBUG_EXIT_R_GV        one rigidity to trace [GV]; <=0 writes a diagnostic list in one-point mode
CUTOFF_DEBUG_EXIT_N           number of list samples when R_GV<=0 in one-point mode
CUTOFF_DEBUG_EXIT_LIST_FILE   optional many-trajectory input list: lon_deg lat_deg alt_km R_GV [label]
CUTOFF_DEBUG_EXIT_FILE        single combined trajectory-exit diagnostic output file name
```

`RIGIDITY_LIST` is a Mode3D-only primary direct-access product for `OUTPUT_MODE SHELLS`
and `CUTOFF_SAMPLING VERTICAL`. It traces one trajectory for every requested
`(selected shell node, rigidity)` pair and writes
`cutoff_3d_shells_access.dat`. Access states are `0=PHYSICAL_FORBIDDEN`,
`1=ALLOWED`, and `2=UNRESOLVED`. The absolute-latitude limits select geodetic
shell nodes while retaining all configured longitudes and both hemispheres.

P1 adds a separate **directional companion** without changing that primary shell mode.
For `PENUMBRA_SCAN + DIRECTIONAL_MAP=T`, a non-empty `CUTOFF_RIGIDITY_LIST_GV`
requests exact three-state classifications for every `(sky cell, rigidity)` pair and writes
`cutoff_3d_dir_access_loc_######.dat` for each observation location.  Each long-form
row contains `lon_deg`, `lat_deg`, `rigidity_GV`, corresponding proton `energy_MeV`,
`access_state`, `allowed`, and `unresolved`.  The standard directional map remains present
and continues to carry lower/effective/upper penumbra diagnostics.  The companion file
adds the energy-dependent access function needed for synthetic detector folding.
Trace-limit states honor `CUTOFF_TRACE_LIMIT_POLICY`, so
`UNRESOLVED` is preserved rather than converted into physical shielding.  This combination
is supported for POINTS, TRAJECTORY, and SHELLS because the companion is indexed by the
directional sky grid rather than by a geographic shell writer.

The primary `RIGIDITY_LIST` algorithm itself still cannot be combined with
`DIRECTIONAL_MAP`; use the PENUMBRA companion form above when directional fixed-rigidity
access is required.


The default `UPPER_SCAN` cutoff search is penumbra-safe and is used by both standalone `mode 3d` and gridless cutoff. It builds a rigidity grid from `CUTOFF_EMIN`/`CUTOFF_EMAX`, evaluates the trajectory classifier at each grid vertex, scans downward from the highest rigidity to find the highest forbidden sample, and then bisects the final forbidden/allowed transition. `PENUMBRA_SCAN` retains the complete allowed/forbidden sequence and additionally reports lower, effective, and upper cutoff rigidities. Use `CUTOFF_SEARCH_ALGORITHM BINARY` only to reproduce the legacy endpoint-only behavior.

`CUTOFF_BACKTRACE_CHARGE` controls the charge used for the outward trajectory launched by the gridless backward-access calculation. `SAME` preserves the historical AMPS convention and uses the configured species charge. `REVERSED` changes only the trace charge sign, so an outward trajectory for a positive incoming particle is integrated as a negative particle. This is the conventional charge-reversal construction used by the Smart--Shea/CARI trajectory tables. The physical species mass, reported rigidity, and input spectrum are unchanged. The default remains `SAME` so existing tests and production inputs do not change silently; C6 writes `REVERSED` explicitly.

`CUTOFF_TRACE_LIMIT_POLICY` controls how a gridless `PENUMBRA_SCAN` trajectory that reaches `MAX_TRACE_TIME`, `CUTOFF_MAX_TRAJ_TIME`, `MAX_STEPS`, or `MAX_TRACE_DISTANCE` is represented in a cutoff scan. `UNRESOLVED` preserves the strict diagnostic behavior: any such sample invalidates an effective-cutoff integral. `FORBIDDEN` treats a bounded trajectory that did not reach the outer escape boundary before the configured safety limit as forbidden. This matches the finite allowed/forbidden trajectory-table convention used by C6. Genuine numerical failures, invalid field values, and non-finite particle states remain errors under either policy. The default remains `UNRESOLVED`; C6 writes `FORBIDDEN` explicitly.

The command-line cutoff-search form is shared by both modes. For example, both `-mode 3d` and `-mode gridless` honor `-cutoff-search PENUMBRA_SCAN` and `-cutoff-upper-scan-n <N>`. The mode-specific aliases `-mode3d-cutoff-search` and `-gridless-cutoff-search` are accepted for clarity in batch scripts, but they write to the same internal cutoff-search settings. The backtrace-charge and trace-limit policies are currently selected in the input file; the C6 runner exposes them as `--backtrace-charge` and `--trace-limit-policy` and writes the corresponding input directives.

The Mode3D debug scan writes a table with `R_GV`, the actual trajectory-classifier
classification, and the analytic Störmer vertical cutoff when `FIELD_MODEL` is
`DIPOLE`. It is useful when a shell map returns the lower rigidity bound at only
some latitude bands: the table shows whether `Rmin` is truly being classified as
allowed or whether the error comes from MPI/thread output ordering. The same scan
can be enabled from the command line with:

```bash
./amps -mode 3d -i AMPS_PARAM_3d_dipole_shells.in \
  -cutoff-debug-scan 0 -40 9000 \
  -cutoff-debug-scan-file cutoff_3d_debug_latm40.dat
```

The exit diagnostic is a stronger trajectory-classifier test. It repeats the same
vertical backtrace used by the cutoff solver and writes the termination reason and
boundary-crossing geometry. For a trajectory counted as allowed, the row must have
`reason=OUTER_BOX`; time limits, step limits, distance limits, and inner-sphere
hits are forbidden. The file also reports the raw terminal point, the linearly
reconstructed box-crossing point/face, the box overshoot, the relative rigidity
error, and for a centered dipole the relative error in the canonical angular
momentum about the dipole axis, `[r x (p + qA)] . m_hat`. This checks whether an
allowed low-rigidity pocket is a true outer-boundary escape or a
classifier/integration artifact.

Single-point example:

```bash
./amps -mode 3d -i AMPS_PARAM_3d_dipole_shells.in \
  -cutoff-debug-exit 0 -60 9000 \
  -cutoff-debug-exit-r 0.0136992266 \
  -cutoff-debug-exit-file cutoff_3d_debug_exit_latm60_rmin.dat
```

If `-cutoff-debug-exit-r` or `CUTOFF_DEBUG_EXIT_R_GV` is omitted or non-positive,
the single-point diagnostic writes a small rigidity list, including log-spaced
samples and Störmer-neighborhood landmarks when `FIELD_MODEL=DIPOLE`.

Many-trajectory example, used by validation test C4:

```text
# c4_debug_trajectories.dat
# lon_deg lat_deg alt_km R_GV label
0.0 -60.0 9000.0 8.0e-02 low_latm60
0.0 -60.0 9000.0 3.2e-01 high_latm60
0.0   0.0 9000.0 1.28e+00 low_lat0
0.0   0.0 9000.0 5.12e+00 high_lat0
```

```bash
./amps -mode 3d -i AMPS_PARAM_3d_dipole_shells.in \
  -cutoff-debug-exit-list c4_debug_trajectories.dat \
  -cutoff-debug-exit-file cutoff_3d_debug_exit_trace.dat
```

All rows from `CUTOFF_DEBUG_EXIT_LIST_FILE` are traced during the same AMPS run and
written to one combined `CUTOFF_DEBUG_EXIT_FILE`. The diagnostic is performed by
rank 0 before the normal Mode3D MPI location scheduler starts, so the output file
remains single and deterministic even when the run uses multiple MPI ranks and
Mode3D worker threads.

### 8.10 `#DENSITY_SPECTRUM`

```text
#DENSITY_SPECTRUM
DS_EMIN            1.0
DS_EMAX            20000.0
DS_NINTERVALS      50
DS_ENERGY_SPACING  LOG
DS_MAX_PARTICLES   500
DS_MAX_TRAJ_TIME   60.0
DS_BOUNDARY_MODE   ISOTROPIC
DS_TRANSMISSION_MODE     SCAN
DS_TRANSMISSION_SCAN_N   80
```

Meanings:

```text
DS_EMIN / DS_EMAX       density/flux energy bounds [MeV/n]
DS_NINTERVALS           number of energy intervals; number of grid points is +1
DS_ENERGY_SPACING       LOG or LINEAR; used directly only in DIRECT transmission mode
DS_MAX_PARTICLES        total trajectory cap per observation point; 0 means no cap
DS_MAX_TRAJ_TIME        density-specific trajectory time cap [s]
DS_BOUNDARY_MODE        ISOTROPIC or ANISOTROPIC
DS_TRANSMISSION_MODE    DIRECT, SCAN, or ADAPTIVE
DS_TRANSMISSION_SCAN_N  number of log-rigidity nodes used by SCAN/ADAPTIVE; 0 => DS_NINTERVALS+1
DS_TRANSMISSION_REFINE_N parsed/reserved for future per-location adaptive refinement
DS_TRANSMISSION_MAX_N   optional cap on the number of transmission scan nodes
DS_TRANSMISSION_SAVE    parsed/provenance flag; spectrum files already write T(E)
```

For Mode3D density/flux, the direction set follows the gridless convention: a 24 x 48 direction grid is built, then optionally reduced deterministically if `DS_MAX_PARTICLES` is positive.

### 8.11 `#SPECTRUM`

The `#SPECTRUM` section defines the outer-boundary differential intensity used by density/flux and forward injection.

The parser stores the raw key/value section and also builds a typed spectrum view. The runtime spectrum evaluator is in:

```text
srcEarth/boundary/spectrum.cpp
srcEarth/boundary/spectrum.h
```

Typical spectrum inputs include a spectrum type, normalization, energy range, and optional file/time-dependent spectrum settings. Consult `boundary/spectrum.h` and `util/amps_param_parser.cpp` for the complete recognized key list.

### 8.12 `#BOUNDARY_ANISOTROPY`

Required when:

```text
DS_BOUNDARY_MODE ANISOTROPIC
```

Example:

```text
#BOUNDARY_ANISOTROPY
BA_PAD_MODEL          COSALPHA_N
BA_PAD_EXPONENT       2.0
BA_SPATIAL_MODEL      DAYSIDE_NIGHTSIDE
BA_DAYSIDE_FACTOR     1.0
BA_NIGHTSIDE_FACTOR   0.2
```

Recognized PAD models:

```text
ISOTROPIC
SINALPHA_N
COSALPHA_N
BIDIRECTIONAL
```

Recognized spatial models:

```text
UNIFORM
DAYSIDE_NIGHTSIDE
```

Anisotropic mode evaluates weights at the boundary exit point of allowed backtraced trajectories.

### 8.13 `#ENERGY_CHANNELS`

Optional. Adds integral-flux channels to density/flux output.

```text
#ENERGY_CHANNELS
CH_BEGIN
  P10_100      10.0     100.0
  P100_1000   100.0    1000.0
  P1G_10G    1000.0   10000.0
CH_END
```

Channels may overlap. Channels are clipped to the available density/flux energy grid.

### 8.14 `#DENSITY_3D`

Used by `-mode 3d_forward`, not by the transmissivity-based Mode3D density/flux solver.

```text
#DENSITY_3D
DENS_EMIN             1.0
DENS_EMAX             20000.0
DENS_NENERGY          30
DENS_ENERGY_SPACING   LOG
```

This section controls the energy bins used by forward-mode AMR cell-density sampling and inner-sphere flux sampling.

### 8.15 `#PARTICLE_TRAJECTORY`

Used by `-mode 3d_forward` particle trajectory record initialization.

```text
#PARTICLE_TRAJECTORY
INITIALIZE_TRAJECTORIES  T
N_TRAJECTORIES           1000
```

Trajectory output also requires the AMPS particle tracker to be enabled at compile time.

### 8.16 `#TEMPORAL`

Standalone Mode3D time-series example:

```text
#TEMPORAL
TEMPORAL_MODE    TIME_SERIES
EVENT_START      2024-05-10T00:00:00
EVENT_END        2024-05-10T12:00:00
FIELD_UPDATE_DT  15
TS_INPUT_MODE    FILE
TS_INPUT_FILE    ts05_driving_2024_05_10.txt
```

Standalone Mode3D irregular independent-case batch:

```text
#TEMPORAL
TEMPORAL_MODE       SNAPSHOT_LIST
SNAPSHOT_LIST_FILE  snapshot_epochs.txt
```

Coupled SWMF usage:

```text
#TEMPORAL
FIELD_UPDATE_DT  15
```

In standalone Mode3D, `FIELD_UPDATE_DT` is the magnetic driver snapshot spacing in minutes. In SWMF-coupled Mode3D products, it is the calculation cadence in minutes of simulation time.

---

## 9. Command-line options

Main usage:

```bash
./amps -h
./amps -mode gridless   -i AMPS_PARAM.in
./amps -mode 3d         -i AMPS_PARAM.in
./amps -mode 3d_forward -i AMPS_PARAM.in
```

Common options:

```text
-h, --help
    Print help and exit.

-mode <gridless|3d|3d_forward>
    Select standalone execution path.

-i <file>
    AMPS_PARAM input file.

-mover <BORIS|HC4|RK2|RK4|RK6|GC2|GC4|GC6|HYBRID>
    Select backward tracing mover for gridless/Mode3D, or the supported subset for 3d_forward.
    HC4 is a fourth-order Higuera-Cary/Boris composition intended for RK4-like
    orbit accuracy with Boris-like rigidity conservation in E=0 magnetic tracing.


HC4 mover notes:

```text
HC4 = H2(w1 dt) H2(w0 dt) H2(w1 dt)
w1 =  1 / (2 - 2^(1/3))
w0 = -2^(1/3) / (2 - 2^(1/3))
```

Here `H2` is a symmetric relativistic Higuera-Cary/Boris magnetic midpoint
step: half drift, evaluate `B` at the midpoint, rotate momentum with the
Boris/Cayley magnetic rotation, then half drift.  In the current backward
tracing layer `E=0`, so this base step preserves `|p|` to roundoff.  The
Yoshida/Forest-Ruth composition raises the smooth-field order to four without
adding any artificial projection of rigidity or canonical invariants.  The
checked HC4 path subdivides the outer step into positive HC4 macro-steps for
loss-sphere event detection, because the internal fourth-order composition
contains a negative middle substep that should not be interpreted as a physical
trajectory segment.

References: Higuera & Cary (Physics of Plasmas, 2017) for the relativistic
Boris-family mover; Yoshida (Physics Letters A, 1990) and Forest & Ruth
(Physica D, 1990) for fourth-order composition of symmetric second-order maps;
Birdsall & Langdon (1991) for the Boris magnetic rotation.

-density-mode <ISOTROPIC|ANISOTROPIC>
    Override DS_BOUNDARY_MODE for density/flux calculations.

-density-transmission-mode <DIRECT|SCAN|ADAPTIVE>
    Override DS_TRANSMISSION_MODE for both gridless and Mode3D density/flux.

-density-transmission-scan-n <N>
    Override DS_TRANSMISSION_SCAN_N.

-density-transmission-refine-n <N>
    Override DS_TRANSMISSION_REFINE_N.  Parsed and stored for future per-location adaptive refinement.

-density-transmission-max-n <N>
    Override DS_TRANSMISSION_MAX_N.

-max-trace-distance <Re>
    Override MAX_TRACE_DISTANCE.

-mode3d-output-initialized
    Write initialized 3-D field mesh diagnostic.

-mode3d-parallel-field-init
    Parallelize standalone Mode3D owner-rank B/E mesh initialization with POSIX
    threads.  The temporary-worker count is taken from -mode3d-threads; the
    calling MPI-rank thread also evaluates an equal share, so N creates N
    pthread workers plus the caller.  Rank 0 prints a flushed global progress bar
    every 2 s using the same 36-character `#`/`-` convention as cutoff tracing.
    Aliases: -bgfield-init-parallel and -mode3d-parallel-bgfield-init.

-mode3d-field-eval <INTERPOLATION|ANALYTIC>
    For standalone Mode3D, choose whether tracing uses mesh interpolation or direct analytic/background evaluation.

-mode3d-mesh-res-earth-re <dRe>
-mode3d-mesh-res-boundary-re <dRe>
-mode3d-mesh-coarsening <LINEAR|LOG|EXPONENTIAL|POWER|CONSTANT>
-mode3d-mesh-exponent <p>
-mode3d-mesh-r-boundary-re <R>
    Optional standalone Mode3D AMR mesh-resolution profile. If omitted, the historical hard-coded localResolution() function is used unchanged. If enabled, dRe near Earth and dRe at the external boundary define the requested cell-size profile. The boundary radius is inferred from the parsed 3-D domain unless overridden.

-cutoff-debug-scan <lon_deg> <lat_deg> <alt_km>
    In standalone Mode3D cutoff runs, write the one-point rigidity classification diagnostic before the full cutoff map. Optional: -cutoff-debug-scan-n <N> and -cutoff-debug-scan-file <file>.

-cutoff-debug-exit <lon_deg> <lat_deg> <alt_km>
    In standalone Mode3D cutoff runs, write the legacy one-point trajectory-exit diagnostic. Optional: -cutoff-debug-exit-r <R_GV>, -cutoff-debug-exit-n <N>, and -cutoff-debug-exit-file <file>.

-cutoff-debug-exit-list <file>
    In standalone Mode3D cutoff runs, trace all diagnostic trajectories listed in <file> during one AMPS run and write one combined output file. Each non-comment line is: lon_deg lat_deg alt_km R_GV [label].

-cutoff-search <UPPER_SCAN|PENUMBRA_SCAN|BINARY>
    Select the cutoff-search product in both standalone Mode3D and gridless cutoff. UPPER_SCAN returns the upper penumbra edge. PENUMBRA_SCAN evaluates the complete rigidity sequence and enables lower, effective, and upper cutoff values; use it for comparisons with published effective-cutoff tables such as Smart--Shea, CARI-7, and Gerontidou et al. BINARY restores the legacy endpoint-only bisection. Optional: -cutoff-upper-scan-n <N>. Mode-specific aliases are also accepted: -mode3d-cutoff-search, -gridless-cutoff-search, -mode3d-cutoff-search-n, and -gridless-cutoff-search-n.

-density-parallel <OPENMP|THREADS|SERIAL>
    Select the shared-memory backend for Mode3D backward products within each MPI process. Despite the name, this option now applies to both cutoff and density/flux calculations. OPENMP preserves the OpenMP loops. THREADS uses direct std::thread workers over observation locations and suppresses nested OpenMP inside those workers. SERIAL disables intra-rank shared-memory parallelism.

-density-threads <N>
    Number of shared-memory workers per MPI process for Mode3D cutoff and density/flux products. For OPENMP this sets omp_set_num_threads(N). For THREADS this sets the number of std::thread workers. N=0 means automatic.
```

Forward-mode options:

```text
-forward-niter <int>
-forward-nparticles <int>
-forward-injection-energy <SPECTRUM|LOG_UNIFORM>
-forward-injection-emin <MeV/n>
-forward-injection-emax <MeV/n>
-forward-injection-energy-range <Emin> <Emax>
-forward-boundary-dist <ISOTROPIC>
-forward-track-trajectories
-forward-no-track-trajectories
-forward-n-trajectories <int>
```

---

## 10. Example inputs

### 10.1 Cutoff only, standalone Mode3D

```text
#CALCULATION_MODE
CALC_TARGET        CUTOFF_RIGIDITY
FIELD_EVAL_METHOD  GRID_3D

#CUTOFF_RIGIDITY
CUTOFF_EMIN            1.0
CUTOFF_EMAX            20000.0
CUTOFF_NENERGY         50
CUTOFF_MAX_PARTICLES   500
CUTOFF_SAMPLING        ISOTROPIC
DIRECTIONAL_MAP        F
```

Run:

```bash
mpirun -np 8 ./amps -mode 3d -i AMPS_PARAM.in
```

### 10.2 Cutoff plus density/flux from one standalone mesh snapshot

```text
#CALCULATION_MODE
CALC_TARGET        CUTOFF_RIGIDITY+DENSITY_SPECTRUM
FIELD_EVAL_METHOD  GRID_3D

#CUTOFF_RIGIDITY
CUTOFF_EMIN            1.0
CUTOFF_EMAX            20000.0
CUTOFF_NENERGY         50
CUTOFF_MAX_PARTICLES   500
CUTOFF_SAMPLING        ISOTROPIC
DIRECTIONAL_MAP        T
DIRMAP_LON_RES         30
DIRMAP_LAT_RES         30

#DENSITY_SPECTRUM
DS_EMIN            1.0
DS_EMAX            20000.0
DS_NINTERVALS      50
DS_ENERGY_SPACING  LOG
DS_MAX_PARTICLES   500
DS_BOUNDARY_MODE   ISOTROPIC
```

This produces cutoff, directional maps, density, local spectrum, and flux files from the same mesh field.

To use the direct-thread backend for both cutoff and density/flux in this run,
add to `#NUMERICAL`:

```text
#NUMERICAL
DENSITY_PARALLEL THREADS
DENSITY_THREADS  8
```

or use the standalone CLI override:

```bash
mpirun -np 4 ./amps -mode 3d -i AMPS_PARAM.in -density-parallel THREADS -density-threads 8
```

For the `THREADS` backend, each MPI rank attempts to widen its CPU affinity
mask before creating worker threads. If needed, override the detected node CPU
set, for example:

```bash
export AMPS_MODE3D_DENSITY_CPUSET=0-39
```

### 10.3 Density/flux only

```text
#CALCULATION_MODE
CALC_TARGET        DENSITY_SPECTRUM
FIELD_EVAL_METHOD  GRID_3D

#DENSITY_SPECTRUM
DS_EMIN            1.0
DS_EMAX            20000.0
DS_NINTERVALS      50
DS_ENERGY_SPACING  LOG
DS_MAX_PARTICLES   500
DS_BOUNDARY_MODE   ISOTROPIC
```

### 10.4 Standalone Tsyganenko time series

```text
#BACKGROUND_FIELD
FIELD_MODEL     T05
DRIVER_FILE     ts05_driving_2024_05_10.txt
EPOCH           2024-05-10T00:00:00

#TEMPORAL
TEMPORAL_MODE    TIME_SERIES
EVENT_START      2024-05-10T00:00:00
EVENT_END        2024-05-10T12:00:00
FIELD_UPDATE_DT  15
```

The requested products are repeated for every snapshot in the event window.

### 10.5 SWMF-coupled cutoff plus density/flux

In the AMPS/PT `AMPS_PARAM.in` used by the coupled run:

```text
#CALCULATION_MODE
CALC_TARGET        CUTOFF_RIGIDITY+DENSITY_SPECTRUM
FIELD_EVAL_METHOD  SWMF

#TEMPORAL
FIELD_UPDATE_DT    15

#NUMERICAL
DENSITY_PARALLEL   THREADS
DENSITY_THREADS    8
```

The coupled bridge reads `AMPS_PARAM.in`, waits for SWMF field updates, and runs the requested products at the configured cadence. The `DENSITY_PARALLEL` and `DENSITY_THREADS` settings are used by both the cutoff and density portions of the coupled backward-product calculation.

---

## 11. Output-file summary

### Gridless cutoff

```text
cutoff_gridless_points.dat
cutoff_gridless_shells.dat
cutoff_gridless_dir_map_point_####.dat
```

### Gridless density/flux

```text
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
gridless_shell_<ALT>km_density_channels.dat
```

### Mode3D cutoff

```text
cutoff_3d_points.dat
cutoff_3d_shells.dat
cutoff_3d_dir_map_loc_000000.dat
cutoff_3d_points_dipole_compare.dat
cutoff_3d_shells_dipole_compare.dat
cutoff_3d_debug_rigidity_scan.dat
```

The dipole comparison files are diagnostic/benchmark products for DIPOLE runs.
`cutoff_3d_debug_rigidity_scan.dat` is written only when the one-point cutoff
debug scan is enabled.

### Mode3D density/flux

```text
mode3d_points_density.dat
mode3d_points_spectrum.dat
mode3d_points_flux.dat
mode3d_shell_<ALT>km_density_flux.dat
```

### Mode3D standalone time series

```text
*_snapshot_000000_YYYY_MM_DDTHH_MM_SS_000.dat
*_snapshot_000001_YYYY_MM_DDTHH_MM_SS_000.dat
```

### Mode3D SWMF-coupled snapshots

```text
*.swmf_n000000_t000000000.000s.dat
*.swmf_n000001_t000000900.000s.dat
```

### 3d_forward

```text
amps_3dforward_initialized.data.dat
sphere_flux3d.out=####.dat
sphere_flux3d_total.out=####.dat
```

and standard AMPS mesh outputs containing `Density3D` variables.

---

## 12. Mode3D AMR mesh-resolution profile

By default, standalone Mode3D keeps the historical hard-coded AMR resolution function in `main_lib.cpp::localResolution()`. This preserves backward compatibility for existing runs.

For validation runs that need controlled mesh convergence, especially `-mode3d-field-eval MESH`, the AMR resolution can now be specified from the input file or from the CLI. The profile is radial: it interpolates between a requested resolution at the Earth and a requested resolution at the external domain boundary.

Input-file example:

```text
#MODE3D_MESH
MODE3D_MESH_RES_EARTH_RE     0.05      ! requested cell size near r=1 Re
MODE3D_MESH_RES_BOUNDARY_RE  0.50      ! requested cell size at the outer boundary
MODE3D_MESH_COARSENING       LOG       ! LINEAR | LOG | EXPONENTIAL | POWER | CONSTANT
MODE3D_MESH_EXPONENT         1.5       ! used by POWER/EXPONENT only
!MODE3D_MESH_R_BOUNDARY_RE   29.0      ! optional; otherwise inferred from DOMAIN_*
```

Equivalent CLI example:

```bash
./amps -mode 3d -i AMPS_PARAM.in \
  -mode3d-field-eval MESH \
  -mode3d-mesh-res-earth-re 0.05 \
  -mode3d-mesh-res-boundary-re 0.50 \
  -mode3d-mesh-coarsening LOG
```

Coarsening choices:

```text
LINEAR       res(r) = res_Earth + t * (res_boundary - res_Earth)
LOG          geometric interpolation of the resolution; aliases: EXPONENTIAL, GEOMETRIC
POWER        res(r) = res_Earth + (res_boundary - res_Earth) * t^p
CONSTANT     res(r) = res_Earth everywhere in the domain
```

where `t = clamp((r/Re - 1)/(R_boundary/Re - 1), 0, 1)`. The inferred `R_boundary` is the maximum absolute parsed domain face distance. For example, a box with `DOMAIN_X/Y/Z = ±29 Re` uses approximately `R_boundary = 29 Re`.

When the custom profile is active, the mesh cache filename includes the requested profile parameters so that AMPS does not accidentally reuse a mesh generated with a different resolution profile.

This option is mainly intended for C5-style mesh-convergence validation. C1 should still be interpreted as the clean analytical-dipole benchmark when run with `-mode3d-field-eval ANALYTIC`; the MESH branch tests the additional AMR field materialization and interpolation error.

---

## 12.1 IGRF-only Mode3D cutoff validation (C6 GRIDDED)

Standalone Mode3D now supports a pure internal-field mesh for external cutoff
table validation:

```text
#CALCULATION_MODE
CALC_TARGET       CUTOFF_RIGIDITY
FIELD_EVAL_METHOD GRID_3D

#BACKGROUND_FIELD
FIELD_MODEL       IGRF
EPOCH             2000-01-01T00:00:00
```

`ConfigureBackgroundFieldModel()` initializes Geopack for the selected epoch
before mesh population. `InitMeshFields()` then evaluates only
`Geopack::IGRF::GetMagneticField()` at AMR cell centers. No T96/T05/TA16
external contribution is added. Once the distributed field has been gathered,
cutoff worker threads use read-only mesh interpolation and do not call the
Fortran/Geopack evaluator.

For a complete effective-cutoff calculation, select:

```text
#CUTOFF_RIGIDITY
CUTOFF_SEARCH_ALGORITHM     PENUMBRA_SCAN
CUTOFF_SCAN_SPACING         LINEAR
CUTOFF_BACKTRACE_CHARGE     REVERSED
CUTOFF_TRACE_LIMIT_POLICY   FORBIDDEN
CUTOFF_SAMPLING             VERTICAL
```

Mode3D writes:

```text
cutoff_3d_shells_penumbra.dat
```

with the same lower/effective/upper cutoff and topology column names as the
gridless penumbra product. This shared file contract lets
`srcEarth/test/C6/run_C6.py` apply one external-reference comparison to either
solver using `--solver GRIDLESS`, `--solver GRIDDED`, or `--solver BOTH`.

The gridded branch adds interpolation error and must be mesh-converged. The C6
default `MODE3D_MESH_RES_EARTH_RE=0.02` is an initial validation setting, not a
universal production tolerance. Repeat the test with a smaller near-Earth cell
target and confirm that `Rc_effective_GV` and the reference residuals approach
stable values.

---

## 13. Validation and development recommendations

1. Start with `DIPOLE` and `CUTOFF_SAMPLING VERTICAL` when validating a new build.
2. Compare gridless and Mode3D with `-mode3d-field-eval ANALYTIC` to separate interpolation issues from trajectory-integration issues.
3. Then switch to `-mode3d-field-eval INTERPOLATION` to validate the AMR field materialization and interpolation path.
4. For time-series Tsyganenko runs, first run one or two snapshots before launching a long event window.
5. Use `DIRECTIONAL_MAP T` only when needed; it can multiply runtime by the number of sky-map direction cells.
6. In SWMF-coupled runs, verify that output suffixes correspond to the intended SWMF simulation times.
7. Treat `3d_forward` density/flux outputs separately from transmissivity-based outputs; they are produced by different statistics.

---

## 14. Known scope boundaries

The current Earth energetic-particle model is primarily a magnetic-access and test-particle transport tool. The backward products do not yet represent a full radiation-belt transport solver.

Not included in the backward transmissivity products:

- pitch-angle diffusion;
- radial diffusion;
- wave-particle scattering;
- Coulomb collisions;
- atmospheric energy loss;
- self-consistent particle feedback on fields;
- time-varying fields during a single traced trajectory.

The inner boundary is an absorbing sphere, not a detailed atmosphere/ionosphere model.

The anisotropic boundary model is factored and simplified:

```text
J_boundary(E, direction, x_exit) = J_iso(E) * f_PAD(direction, B) * f_spatial(x_exit)
```

It is useful for controlled SEP/radiation access studies but is not a full 5-D boundary distribution.

---

## 14. Troubleshooting

### `DIRECTIONAL_MAP T` does not write files

Check that the run is requesting cutoff:

```text
CALC_TARGET CUTOFF_RIGIDITY
```

or

```text
CALC_TARGET CUTOFF_RIGIDITY+DENSITY_SPECTRUM
```

Also check that both angular resolutions are positive:

```text
DIRMAP_LON_RES > 0
DIRMAP_LAT_RES > 0
```

### Standalone time series does not loop over snapshots

Check:

```text
TEMPORAL_MODE TIME_SERIES
EVENT_START   ...
EVENT_END     ...
FIELD_UPDATE_DT > 0
```

and verify that a driver file is loaded through `TS_INPUT_FILE` or `DRIVER_FILE`.

### Coupled SWMF run still behaves like forward injection

In coupled mode, backward products require explicit `CALC_TARGET`. If `CALC_TARGET` is absent, the coupled bridge preserves the historical forward-injection path.

### SPICE-related failures

Standalone time-series operation and frame transforms require SPICE. Rebuild without `_NO_SPICE_CALLS_` if the run needs UTC-to-ET conversion, driver-file interpolation, SM/GSM directional-map rotation, or trajectory frame conversion.

### Direct-thread backend creates workers but all run on one CPU

If `DENSITY_PARALLEL THREADS` creates the requested worker count but `top -H`
or `ps -L` shows that all worker threads run on the same `PSR` CPU, check the
rank affinity mask:

```bash
grep Cpus_allowed_list /proc/<pid>/status
ps -L -p <pid> -o pid,tid,psr,pcpu,state,comm
```

The direct-thread backend calls `PIC::Parallel::SetWideAffinityForScheduler()`
before creating cutoff/density workers. The dynamic scheduler can only use CPUs
that are present in the MPI rank affinity mask, so the affinity widening remains
important even though work assignment among threads is now dynamic. This is
equivalent to:

```bash
taskset -apc <CPUSET> <pid>
```

and should widen the rank affinity mask when the scheduler allows it. The CPU
set is auto-detected from `/sys/devices/system/cpu/online`, but it can be
overridden with:

```bash
export AMPS_MODE3D_DENSITY_CPUSET=0-39
```

or:

```bash
export PIC_PARALLEL_CPUSET=0-39
```

If `Cpus_allowed_list` remains a single CPU, the batch system or MPI runtime is
still restricting the rank through a cpuset/cgroup, and the code cannot move
threads outside that allowed set.

### No useful density/flux values

Check:

- the outer domain is large enough for allowed trajectories to escape;
- `R_INNER` is reasonable;
- `MAX_TRACE_TIME`, `MAX_STEPS`, and `MAX_TRACE_DISTANCE` are not too restrictive;
- the boundary spectrum is initialized and covers the requested energy range;
- `DS_MAX_PARTICLES` is not so small that each energy gets too few directions.

---

## 15. Minimal command examples

Gridless cutoff:

```bash
mpirun -np 8 ./amps -mode gridless -i cutoff_gridless.in
```

Gridless density/flux:

```bash
mpirun -np 8 ./amps -mode gridless -i density_gridless.in -density-mode ISOTROPIC
```

Standalone mesh-backed cutoff plus density/flux:

```bash
mpirun -np 8 ./amps -mode 3d -i mode3d_products.in
```

Standalone mesh-backed cutoff/density products using direct threads inside each MPI rank:

```bash
mpirun -np 4 ./amps -mode 3d -i mode3d_products.in -density-parallel THREADS -density-threads 8
```

This backend applies to `CUTOFF_RIGIDITY`, `DENSITY_SPECTRUM`, and `CUTOFF_RIGIDITY+DENSITY_SPECTRUM`. Choose the MPI-rank count and thread count so that `ranks_per_node * density_threads` does not exceed the cores allocated per node.

In SWMF-coupled PT runs the standalone CLI is usually not used; the same backend can be selected from the input file with `DENSITY_PARALLEL THREADS` and `DENSITY_THREADS 8` in `#NUMERICAL` or `#DENSITY_SPECTRUM`, or with environment variables `AMPS_MODE3D_DENSITY_PARALLEL=THREADS` and `AMPS_MODE3D_DENSITY_THREADS=8`.

Standalone mesh-backed direct-field cross-check:

```bash
mpirun -np 8 ./amps -mode 3d -i mode3d_products.in -mode3d-field-eval ANALYTIC
```

Forward Monte Carlo 3-D run:

```bash
mpirun -np 8 ./amps -mode 3d_forward -i forward3d.in -forward-niter 1000 -forward-nparticles 1000
```


### References for transmissivity/cutoff terminology and density/flux validation

- Cooke, D. J., Humble, J. E., Shea, M. A., Smart, D. F., Lund, N., Rasmussen, I. L., Byrnak, B., Goret, P., and Petrou, N. (1991), *On cosmic-ray cut-off terminology*, Il Nuovo Cimento C. This is the standard reference for lower/upper/effective cutoff and penumbra terminology.
- Smart, D. F. and Shea, M. A. (2001), *Geomagnetic Cutoff Rigidity Computer Program*, Final Report, Air Force Research Laboratory. This is a practical trajectory-tracing/cutoff reference used by many radiation-environment workflows.
- Smart, D. F. and Shea, M. A. (2009), *Fifty years of progress in geomagnetic cutoff rigidity determinations*, Advances in Space Research, 44, 1107-1123. This review summarizes cutoff-rigidity calculation methods and their historical use.
- Bobik, P. et al. (2013), *GeoMag and HelMod webmodels version for magnetosphere and heliosphere transport of cosmic rays*, arXiv:1307.5196. GeoMag solves the Lorentz equation with IGRF plus Tsyganenko T96/T05 and classifies trajectories by outer/inner boundary access.
- Boschini, M. J. et al. (2013), *Geomagnetic Backtracing: A comparison of Tsyganenko 1996 and 2005 External Field models with AMS-02 data*, arXiv:1307.5192. This supports validation of directional access, asymptotic directions, East-West effects, and T96/T05 model sensitivity.
- Bruno, A. et al. (2014/2016), PAMELA SEP trajectory-analysis papers, including arXiv:1412.1765 and arXiv:1601.05407. These papers use realistic geomagnetic backtracing/asymptotic exposure to reconstruct SEP spectra and pitch-angle distributions.
- Adriani, O. et al. (2016), *PAMELA's measurements of geomagnetic cutoff variations during the 14 December 2006 storm*, arXiv:1602.05509. This is a direct storm-time proton cutoff-latitude validation target.
- Dmitriev, A. V., Jayachandran, P. T., and Tsai, L.-C. (2013), *Elliptical model of cutoff boundaries for the solar energetic particles measured by POES satellites in December 2006*, arXiv:1305.6710. This provides an empirical SEP cutoff-boundary comparison for high-latitude flux penetration.

---

## Validation test suite (`srcEarth/test`)

The directory `srcEarth/test` contains executable regression/validation tests for
the AMPS SEP geospace cutoff, density, and flux products.  Each validation test
is placed in its own subdirectory, for example `srcEarth/test/C1`, and contains:

* a Python driver script that launches AMPS, parses the output, and reports
  PASS/FAIL;
* the AMPS input file used by the test;
* analytical reference values or external reference tables when applicable;
* a local README describing the purpose of the test.

The scripts are intended to be run from the directory containing the `amps`
executable.  AMPS is launched through `mpirun`; every script accepts the common
parallel options:

```bash
-np N       number of MPI ranks, default 4
-nt N       number of threads per MPI rank, default 16
--amps EXE  AMPS executable path, default ./amps
--mpirun EXE MPI launcher, default mpirun
```

The `-np` and `-nt` defaults provide a compact validation
configuration used for the geospace backward-tracing tests: enough MPI ranks to
exercise the inter-rank scheduler and enough threads per rank to exercise the
intra-rank dynamic work queue.

### C1 — Pure dipole vertical Størmer cutoff

Directory: `srcEarth/test/C1`

Driver:

```bash
python srcEarth/test/C1/run_C1.py -np 4 -nt 16
```

Purpose.  C1 validates the most basic cutoff calculation before any IGRF,
Tsyganenko, SWMF, density, or flux validation is attempted.  The test uses
standalone `-mode 3d`, a centered aligned dipole, no electric field, vertical
arrival directions, two spherical shells, and the shared `UPPER_SCAN` cutoff
algorithm.  The reference solution is the analytical vertical Størmer cutoff,

```text
Rc = R0 cos^4(lambda) / r_RE^2,
```

where `lambda` is magnetic latitude and `r_RE` is geocentric radius in Earth
radii.  This test primarily checks rigidity/energy conversion, charge/sign
convention, vertical backtracing, the Mode3D shell output path, and the
consistency of the numerical cutoff with the analytical dipole limit.

Input files:

```text
srcEarth/test/C1/AMPS_PARAM_C1.in          # backward-compatible Mode3D alias
srcEarth/test/C1/AMPS_PARAM_C1_mode3d.in   # parser-compatible Mode3D template
srcEarth/test/C1/AMPS_PARAM_C1_gridless.in # parser-compatible gridless template
```

The active keywords in these C1 templates are kept close to the working
CCMC/RoR-style `AMPS_PARAM_test.in` layout.  Newer validation controls that may
not be accepted as input-file keywords by every checkout are passed by the Python
harness on the AMPS command line.  Examples are `-cutoff-search UPPER_SCAN`,
`-mode3d-field-eval ANALYTIC`, `-mode3d-mpi-scheduler`, and the thread-count
options.  Use `--no-cutoff-search-cli` only as a fallback for older CLI builds.

Reference values:

```text
srcEarth/test/C1/reference_C1_stormer.csv
```

AMPS output parsed by the test:

```text
cutoff_3d_shells_dipole_compare.dat
```

Test artifacts written to the run directory:

```text
C1_amps.log
C1_summary.csv
C1_result.json
C1_stormer_comparison.png   # written when matplotlib is available
```

Acceptance logic.  The script compares the longitude-averaged numerical cutoff
at latitudes ±60°, ±30°, and 0° against the independent Størmer reference at 500
km and 9000 km.  Mid-latitude points are checked more tightly than high-latitude
points because high-latitude access is more sensitive to the finite domain,
penumbra structure, and mesh interpolation.  The analytical `Rc_vert_GV` column
written by AMPS is also compared against the independent reference table to catch
accidental changes in constants or units.

A failing C1 result should be treated as a gate failure: later IGRF,
Tsyganenko, scheduler, density, and event-level tests should not be interpreted
until the pure-dipole cutoff baseline is fixed.

---

## Validation test harness: `srcEarth/test`

Executable validation tests are stored under `srcEarth/test`.  Each test has its
own directory containing a Python driver, AMPS input files, and any analytical or
external reference data needed by the test.  The Python drivers are launched from
the directory containing the `amps` executable and run AMPS through `mpirun`.

Common options used by the test harness are:

```bash
-np <N>     number of MPI ranks passed to mpirun; default 4
-nt <N>     number of threads per MPI rank; default 16
--amps      path to the AMPS executable; default ./amps
--mpirun    MPI launcher; default mpirun
--skip-run  analyze existing output in the test work directory
```

### C1 — pure dipole vertical Størmer cutoff

C1 is the first cutoff-regression test.  It validates the vertical cutoff in a
centered aligned dipole against the analytical Størmer expression
`Rc = R0 cos^4(lambda)/r_RE^2`.  It supports both standalone Mode3D and gridless
execution:

```bash
python srcEarth/test/C1/run_C1.py --mode 3d -np 4 -nt 16
python srcEarth/test/C1/run_C1.py --mode gridless -np 4 -nt 16
```

For Mode3D, C1 also exposes the field-evaluation backend used by the trajectory
tracer:

```bash
python srcEarth/test/C1/run_C1.py --mode 3d --mode3d-field-eval ANALYTIC
python srcEarth/test/C1/run_C1.py --mode 3d --mode3d-field-eval MESH
```

The `ANALYTIC` case passes the AMPS CLI option
`-mode3d-field-eval ANALYTIC`, for example:

```bash
mpirun -np 4 ./amps -mode 3d -i AMPS_PARAM_C1.in \
  -mode3d-field-eval ANALYTIC
```

The C1 input is parser-compatible and close to the working
`AMPS_PARAM_test.in` file: it does not activate `#TEMPORAL`, does not use active
`#END`, and leaves current-code controls such as UPPER_SCAN and dynamic
scheduler selection to the CLI.

This is important because it bypasses mesh interpolation for analytic background
fields such as the centered dipole and the Tsyganenko family, isolating the
particle pusher, cutoff search, and trajectory classifier.  The `MESH` case keeps
the mesh-stored Mode3D path and is therefore the appropriate companion test for
field materialization and interpolation effects.

### C2 — dipole longitude and north/south symmetry

Directory: `srcEarth/test/C2`

Driver:

```bash
python srcEarth/test/C2/run_C2.py -np 4 -nt 16
```

Purpose.  C2 is the symmetry companion to C1.  In a centered aligned dipole, the
vertical Størmer cutoff depends only on magnetic latitude and geocentric radius,
not on longitude, and it is symmetric between `+lambda` and `-lambda`.  C2 runs a
9000 km shell and verifies two quantities:

```text
max_lon(Rc) - min_lon(Rc) at fixed latitude
Rc(+latitude, longitude) - Rc(-latitude, longitude)
```

The input uses the parser-compatible shell syntax supported by the current AMPS
input reader:

```text
#OUTPUT_DOMAIN
OUTPUT_MODE            SHELLS
SHELL_COUNT            1
SHELL_ALTS_KM          9000.0
SHELL_RES_DEG          30
```

The shorthand sometimes used in the validation-plan prose, for example
`SHELL_LON_DEG` or `SHELL_LAT_DEG`, is not used because it is not a recognized
parser keyword.  The Python script extracts the required points from the full
shell output.

Execution examples:

```bash
python srcEarth/test/C2/run_C2.py --mode 3d --mode3d-field-eval ANALYTIC
python srcEarth/test/C2/run_C2.py --mode 3d --mode3d-field-eval MESH
python srcEarth/test/C2/run_C2.py --mode gridless
python srcEarth/test/C2/run_C2.py --mode gridless --max-trace-distance 300
```

Input files:

```text
srcEarth/test/C2/AMPS_PARAM_C2.in          # backward-compatible Mode3D alias
srcEarth/test/C2/AMPS_PARAM_C2_mode3d.in   # parser-compatible Mode3D template
srcEarth/test/C2/AMPS_PARAM_C2_gridless.in # parser-compatible gridless template
```

Reference values:

```text
srcEarth/test/C2/reference_C2_stormer_symmetry.csv
```

AMPS output parsed by the test:

```text
cutoff_3d_shells_dipole_compare.dat
cutoff_gridless_shells_dipole_compare.dat
```

Test artifacts written to the run directory:

```text
C2_amps.log
C2_longitude_summary.csv
C2_north_south_summary.csv
C2_result.json
C2_symmetry.png   # written when matplotlib is available
```

Acceptance logic.  C2 primarily gates symmetry, while C1 remains the absolute
Størmer cutoff gate.  The C2 script reports absolute Størmer residuals as
diagnostics but uses longitude spread and north/south paired differences for
PASS/FAIL.  This makes C2 useful for diagnosing finite-domain, finite-time, or
finite-distance classification artifacts: such artifacts often appear as a
longitude-dependent cutoff on a formally longitude-symmetric dipole field.

### C3 — penumbra, Rc_upper, and UPPER_SCAN regression

Directory: `srcEarth/test/C3`

Driver:

```bash
python srcEarth/test/C3/run_C3.py -np 4 -nt 16
```

Purpose.  C3 checks the cutoff-search algorithm at the high-latitude outer-shell
point that is most sensitive to non-monotonic allowed/forbidden rigidity access:

```text
lon = 0 deg
lat = -60 deg
alt = 9000 km
FIELD_MODEL = DIPOLE
CUTOFF_SAMPLING = VERTICAL
```

For the centered aligned dipole, the analytical vertical Størmer cutoff at this
point is about `0.159984 GV`.  The C3 input sets `CUTOFF_EMIN=1 MeV/n`, which is
`Rmin≈0.043331 GV` for protons.  A result close to this lower rigidity bound is a
lower-boundary collapse or low-rigidity leakage signature and should not be
accepted as the upper cutoff.

The script runs two search algorithms by default:

```text
BINARY      legacy/diagnostic endpoint-binary search
UPPER_SCAN  production search used for validation
```

Only `UPPER_SCAN` is a PASS/FAIL gate by default.  The BINARY run is archived and
reported because it is useful for diagnosing the historical endpoint-binary
failure, but the harness does not require BINARY to fail unless
`--require-binary-collapse` is specified.

Execution examples:

```bash
python srcEarth/test/C3/run_C3.py --mode 3d --mode3d-field-eval ANALYTIC
python srcEarth/test/C3/run_C3.py --mode 3d --mode3d-field-eval MESH
python srcEarth/test/C3/run_C3.py --mode gridless
python srcEarth/test/C3/run_C3.py --algorithms UPPER_SCAN --cutoff-scan-n 200
python srcEarth/test/C3/run_C3.py --mode gridless --max-trace-distance 300
```

Input files:

```text
srcEarth/test/C3/AMPS_PARAM_C3.in          # backward-compatible Mode3D alias
srcEarth/test/C3/AMPS_PARAM_C3_mode3d.in   # parser-compatible Mode3D template
srcEarth/test/C3/AMPS_PARAM_C3_gridless.in # parser-compatible gridless template
```

Reference values:

```text
srcEarth/test/C3/reference_C3_penumbra.csv
```

The C3 templates keep the parser-compatible shell syntax:

```text
#OUTPUT_DOMAIN
OUTPUT_MODE            SHELLS
SHELL_COUNT            1
SHELL_ALTS_KM          9000.0
SHELL_RES_DEG          30
```

and the script extracts `lat=±60 deg` and selected longitudes from the shell
output.  The templates also include an active `MAX_TRACE_DISTANCE` keyword.  This
is deliberate: C3 is sensitive to long-time numerical leakage of quasi-trapped
low-rigidity trajectories, so both time and cumulative-distance caps should be
recorded in the validation artifacts.

AMPS output parsed by the test:

```text
cutoff_3d_shells_dipole_compare.dat
cutoff_gridless_shells_dipole_compare.dat
```

Test artifacts written to the run directory:

```text
C3_summary.csv
C3_result.json
C3_penumbra_comparison.png   # written when matplotlib is available
binary/C3_binary_amps.log
upper_scan/C3_upper_scan_amps.log
```

### C11 — penumbra-pocket BINARY Rmin-collapse regression

Directory: `srcEarth/test/C11`

Driver:

```bash
python srcEarth/test/C11/run_C11.py -np 4 -nt 16
```

Purpose.  C11 isolates the historical endpoint-binary failure that motivated the
penumbra-safe `UPPER_SCAN` cutoff search.  At the high-latitude dipole-shell
point

```text
lon = 0 deg
lat = -60 deg
alt = 9000 km
FIELD_MODEL = DIPOLE
CUTOFF_SAMPLING = VERTICAL
```

an isolated low-rigidity allowed pocket can cause the legacy `BINARY` search to
return the lower search bound.  The C11 input sets `CUTOFF_EMIN=0.05 MeV/n`,
which corresponds to `Rmin≈9.6866e-3 GV` for protons, while the analytical
vertical Størmer upper cutoff at the primary point is about `0.159972 GV`.

The script runs two algorithms by default:

```text
BINARY      legacy endpoint-only search; expected to reproduce the Rmin collapse
UPPER_SCAN  production search; expected to recover the upper Størmer cutoff
```

Unlike C3, where BINARY is mostly diagnostic, C11 deliberately requires the
primary BINARY run to reproduce the known Rmin-collapse signature.  This makes
C11 a code-level regression guard for that specific historical behavior.  If the
legacy BINARY path is removed or intentionally fixed, the test expectation should
be updated with the code change.

Execution examples:

```bash
python srcEarth/test/C11/run_C11.py --mode3d-field-eval ANALYTIC
python srcEarth/test/C11/run_C11.py --mode3d-field-eval MESH
python srcEarth/test/C11/run_C11.py --algorithms UPPER_SCAN --cutoff-scan-n 200
python srcEarth/test/C11/run_C11.py --target-alts 500,9000 --target-lats -60,60
python srcEarth/test/C11/run_C11.py --dry-run
```

Input files and reference table:

```text
srcEarth/test/C11/AMPS_PARAM_C11.in
srcEarth/test/C11/AMPS_PARAM_C11_mode3d.in
srcEarth/test/C11/reference_C11_penumbra_pocket.csv
```

C11 is Mode3D-only because it uses the single-point
`CUTOFF_DEBUG_RIGIDITY_SCAN` diagnostic.  The generated input enables that
diagnostic at the primary point and writes one scan file per algorithm:

```text
binary/C11_binary_debug_rigidity_scan.dat
upper_scan/C11_upper_scan_debug_rigidity_scan.dat
```

The debug scan reports `Rc_selected_GV`, `Rc_endpoint_binary_GV`,
`Rc_upper_scan_GV`, and `Rc_stormer_GV`.  C11 requires the endpoint-binary value
to be near `Rmin` and the UPPER_SCAN value to be close to the Størmer upper
cutoff.

Test artifacts written to the run directory:

```text
C11_summary.csv
C11_result.json
C11_binary_vs_upper_scan.png   # written when matplotlib is available
reference_C11_penumbra_pocket_generated.csv
binary/C11_binary_amps.log
upper_scan/C11_upper_scan_amps.log
```

### C14 — Mode3D/gridless cross-solver consistency

Directory: `srcEarth/test/C14`

Driver:

```bash
python srcEarth/test/C14/run_C14.py -np 4 -nt 16
```

Purpose.  C14 compares the standalone Mode3D and gridless backward cutoff
implementations on the same centered aligned-dipole vertical-cutoff shell
problem.  The two solvers share production algorithms such as `UPPER_SCAN` and
dynamic MPI scheduling, but they still represent fields and locations through
different code paths.  Agreement demonstrates cross-solver consistency rather
than only internal reproducibility.

Default physical setup:

```text
FIELD_MODEL = DIPOLE
CUTOFF_SAMPLING = VERTICAL
CUTOFF_SEARCH = UPPER_SCAN
alt = 500, 9000 km
lon = 0, 30, ..., 330 deg
lat = -60, -30, 0, 30, 60 deg
```

The analytical reference for both solver outputs is the centered-dipole vertical
Størmer cutoff,

```text
Rc = R0 cos^4(lambda) / r_RE^2 .
```

Execution examples:

```bash
python srcEarth/test/C14/run_C14.py -np 4 -nt 16
python srcEarth/test/C14/run_C14.py --mode3d-field-eval MESH -np 4 -nt 16
python srcEarth/test/C14/run_C14.py --scheduler STATIC --dynamic-chunk 0
python srcEarth/test/C14/run_C14.py --lons 0,90,180,270 --lats -60,-30,0,30,60
python srcEarth/test/C14/run_C14.py --dry-run
```

Input files and reference table:

```text
srcEarth/test/C14/AMPS_PARAM_C14_mode3d.in
srcEarth/test/C14/AMPS_PARAM_C14_gridless.in
srcEarth/test/C14/reference_C14_cross_solver.csv
```

Acceptance checks:

```text
Mode3D/gridless relative difference:  < 1% for |lat| <= 30 deg
Mode3D/gridless relative difference:  < 5% for high-latitude points
Mode3D and gridless Størmer residual: < 5% mid-latitude, < 25% high-latitude
longitude spread and north/south residuals remain small for each solver
```

Test artifacts written to the run directory:

```text
C14_summary.csv
C14_result.json
C14_mode3d_vs_gridless.png   # written when matplotlib is available
reference_C14_cross_solver_generated.csv
mode3d/C14_3d_amps.log
gridless/C14_gridless_amps.log
```

### C18 — deterministic POSIX-thread field initialization

Directory: `srcEarth/test/C18`

Driver:

```bash
python srcEarth/test/C18/run_C18.py --profile ROUTINE -np 4
```

Purpose. C18 creates a serial runtime baseline for each standalone Mode3D
magnetic model and compares it with otherwise identical runs using
`-mode3d-parallel-field-init` and 1, 2, 4, 8, and 16 temporary POSIX workers.
The cutoff backend is serial by default, which isolates the mesh B/E
initialization stage. The calling MPI-rank thread participates as one additional
equal work share, so a request for 16 workers produces 17 initialization
participants per rank.

C18 has no checked-in numerical reference solution. It fingerprints the
initialized mesh output and `cutoff_3d_shells_access.dat` from the serial run,
then requires exact equality for every pthread run and repeat. This is a
parallel-equivalence regression rather than a physical model validation; C6,
C7, C9, and C10 retain that role.

Default routine matrix:

```text
models        DIPOLE, IGRF, T96, T05
worker counts 1, 2, 4, 8, 16
repeats       2
```

Useful commands:

```bash
python srcEarth/test/C18/run_C18.py --profile SMOKE -np 2
python srcEarth/test/C18/run_C18.py --self-test
python srcEarth/test/C18/run_C18.py --dry-run
python srcEarth/test/C18/run_C18.py --cutoff-backend THREADS -np 4
```

Test artifacts:

```text
C18_configuration.json
C18_commands.json
C18_comparison.csv
C18_result.json
C18_summary.txt
```

### C4 — parser-safe trace-control and mover convergence

Directory: `srcEarth/test/C4`

Driver:

```bash
python srcEarth/test/C4/run_C4.py -np 4 -nt 16
```

Purpose.  C4 is the parser-safe replacement for the earlier proposed
trajectory-invariant diagnostic.  The current parser does not recognize the
non-production `CUTOFF_DEBUG_EXIT_*` keywords, so C4 uses only supported input
syntax and checks the ordinary cutoff output against the analytical vertical
Størmer solution.

The test runs a centered aligned dipole shell at 9000 km for a sweep of
`DT_TRACE` values.  As the trajectory step is reduced, the numerical cutoff
should remain stable and approach:

```text
Rc = 14.9 cos^4(lambda) / r_RE^2 GV
```

The default checked points are:

```text
alt = 9000 km
lat = -60, -30, 0, +30, +60 deg
lon = 0, 90, 180, 270 deg
DT_TRACE = 1.0, 0.5, 0.25 s
```

Acceptance checks:

```text
finest DT_TRACE run: max |Rc_num - Rc_Stormer| / Rc_Stormer < 5% by default
coarser runs:        max relative error < 25% by default
convergence sanity:  finest run should not be substantially worse than coarsest
```

Execution examples:

```bash
python srcEarth/test/C4/run_C4.py --mode 3d --mode3d-field-eval ANALYTIC
python srcEarth/test/C4/run_C4.py --mode 3d --mode3d-field-eval MESH --dt-sweep 1.0,0.5,0.25
python srcEarth/test/C4/run_C4.py --mode gridless --max-trace-distance 300
python srcEarth/test/C4/run_C4.py --dt-sweep 1.0,0.5,0.25,0.125
python srcEarth/test/C4/run_C4.py --movers BORIS,RK4,RK6 --adaptive-dt F
```

Input files:

```text
srcEarth/test/C4/AMPS_PARAM_C4.in
srcEarth/test/C4/AMPS_PARAM_C4_mode3d.in
srcEarth/test/C4/AMPS_PARAM_C4_gridless.in
```

Reference values:

```text
srcEarth/test/C4/reference_C4_stormer_convergence.csv
```

C4 edits only parser-supported keywords:

```text
FIELD_EVAL_METHOD
CUTOFF_MAX_TRAJ_TIME
SHELL_ALTS_KM
SHELL_RES_DEG
DT_TRACE
MAX_TRACE_TIME
MAX_TRACE_DISTANCE
```

AMPS output parsed by the test:

```text
cutoff_3d_shells_dipole_compare.dat
cutoff_gridless_shells_dipole_compare.dat
```

Test artifacts written to the run directory:

```text
reference_C4_stormer_convergence.csv
C4_summary.csv
C4_case_metrics.csv
C4_result.json
C4_convergence.png   # written when matplotlib is available
dt_*/C4_amps.log
```

`MAX_TRACE_TIME` and `MAX_TRACE_DISTANCE` are part of the validation state.
Too-loose caps can let quasi-trapped low-rigidity trajectories leak to the outer
box, while too-tight caps can stop truly allowed near-cutoff trajectories before
they escape.  C4 exposes both controls explicitly.


### F1 — zero-field density/flux normalization

Directory: `srcEarth/test/F1`

Driver:

```bash
python srcEarth/test/F1/run_F1.py -np 4 -nt 16
```

Purpose.  F1 is the first density/flux normalization test.  It uses a deliberately
trivial transport problem:

```text
FIELD_MODEL          NONE
EFIELD_MODEL         NONE
R_INNER              0.0 km
DS_TRANSMISSION_MODE DIRECT
SPECTRUM_TYPE        POWER_LAW
SPEC_J0              1.0
SPEC_E0              10.0 MeV
SPEC_GAMMA           3.5
SPEC_EMIN/SPEC_EMAX  1.0 / 1000.0 MeV
```

With `B=0` and no absorbing inner sphere, every straight-line trajectory from the
ten requested points exits the outer box.  Therefore the reference solution is
`T(E)=1`, `J_local(E)=J_boundary(E)`, omnidirectional flux `4π∫J(E)dE`, and
density `4π∫J(E)/v(E)dE`.  The test verifies absolute normalization,
energy-channel fluxes, spectrum-file consistency, and zero spatial variation.

Input and reference files:

```text
srcEarth/test/F1/AMPS_PARAM_F1_gridless.in
srcEarth/test/F1/reference_F1_zero_field.csv
```

AMPS output parsed by the test:

```text
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
```

Test artifacts written to the run directory:

```text
F1_amps.log
F1_summary.csv
F1_result.json
```

F1 requires the gridless field evaluator branch for `FIELD_MODEL NONE`, which
returns `B=(0,0,0)` and leaves all Tsyganenko/Geopack state untouched.


### F2 — power-law energy integration

Directory: `srcEarth/test/F2`

Driver:

```bash
python srcEarth/test/F2/run_F2.py -np 4 -nt 16
```

Purpose.  F2 validates the energy-folding part of the density/flux workflow in a zero-field, all-open transport problem.  Since `T(E)=1`, the saved differential spectrum must reproduce the imposed power law and the total/channel fluxes must match the analytical integrals of

```text
J(E) = SPEC_J0 * (E / SPEC_E0)^(-SPEC_GAMMA)
```

over the validation energy bins `1, 3, 10, 30, 100, 300, 1000 MeV`.  In the parser-compatible input file, those bins are represented with `#ENERGY_CHANNELS`; the production quadrature uses a fine log grid, `DS_NINTERVALS=960` by default.  Density is compared with an independent high-resolution reference for `4π∫J(E)/v(E)dE`.

Input and reference files:

```text
srcEarth/test/F2/AMPS_PARAM_F2_gridless.in
srcEarth/test/F2/reference_F2_power_law.csv
```

AMPS output parsed by the test:

```text
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
```

Test artifacts written to the run directory:

```text
F2_amps.log
F2_summary.csv
F2_result.json
```

F2 uses the same `FIELD_MODEL NONE` gridless branch introduced for F1.  The summary table uses `expected_value` and `check_type`, so zero expected values denote zero residuals/error metrics rather than zero physical flux.


### F3 — dipole cutoff-filtered flux

Directory: `srcEarth/test/F3`

Driver:

```bash
python srcEarth/test/F3/run_F3.py -np 4 -nt 16
```

Purpose.  F3 is the first end-to-end magnetic-access flux test in the validation campaign.  It uses `FIELD_MODEL DIPOLE`, `DS_TRANSMISSION_MODE SCAN`, and the same power-law spectrum as F1/F2, but samples explicit points on the 9000 km shell.  The default rendered point set follows the validation-plan latitude profile

```text
lat = -70, -60, -45, -30, 0, 30, 45, 60, 70 deg
lon = 0, 90 deg
```

The external reference is a Størmer hard-cutoff approximation folded analytically with the power-law spectrum:

```text
Rc(lambda,r) = 14.9 cos^4(lambda) / r_RE^2 GV
F_ch = 4*pi*T_open*int_{max(E1,Ecut)}^{E2} J0*(E/E0)^(-gamma) dE
```

Here `Ecut` is obtained by converting `Rc` to proton kinetic energy, and `T_open` is the analytic straight-line open-sky fraction outside the inner absorbing sphere.  This comparison is intentionally approximate: the AMPS run traces the full directional access map and includes penumbra, while the reference collapses access to a vertical step function.  The runner therefore also performs tight exact/internal checks on `J_local(E)=T(E)J_boundary(E)`, density and flux reconstruction from the saved spectrum, longitude symmetry, north/south symmetry, and the expected increase in access toward high absolute latitude.

Input and reference files:

```text
srcEarth/test/F3/AMPS_PARAM_F3_gridless.in
srcEarth/test/F3/reference_F3_dipole_cutoff.csv
```

AMPS output parsed by the test:

```text
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
```

Test artifacts written to the run directory:

```text
F3_amps.log
F3_summary.csv
F3_result.json
reference_F3_dipole_cutoff_used.csv
```


### F4 — transmission reconstruction consistency

Directory: `srcEarth/test/F4`

Driver:

```bash
python srcEarth/test/F4/run_F4.py -np 4 -nt 16
```

Purpose.  F4 is the exact closure test for the gridless density/flux workflow.  It uses `FIELD_MODEL DIPOLE`, `DS_TRANSMISSION_MODE SCAN`, `DS_TRANSMISSION_SAVE T`, and a power-law boundary spectrum at diagnostic points on the 9000 km shell.  The default latitude set is

```text
lat = -60, -30, 0, 30, 60 deg
lon = 0 deg
```

The reference is internal and exact rather than an external cutoff formula:

```text
J_local(E) = T(E) * J_boundary(E)
n           = 4*pi*int J_local(E)/v(E) dE
F           = 4*pi*int J_local(E) dE
F_channel   = 4*pi*int_channel J_local(E) dE
```

The runner reads `gridless_points_spectrum.dat`, reconstructs density and every requested integral-flux channel, and compares those values with `gridless_points_density.dat` and `gridless_points_flux.dat`.  It also verifies that the saved boundary spectrum matches the imposed power law and that `T(E)` remains in `[0,1]`.

Input and reference/check files:

```text
srcEarth/test/F4/AMPS_PARAM_F4_gridless.in
srcEarth/test/F4/reference_F4_reconstruction.csv
```

AMPS output parsed by the test:

```text
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
```

Test artifacts written to the run directory:

```text
F4_amps.log
F4_summary.csv
F4_result.json
reference_F4_reconstruction_used.csv
```

All rows in `F4_summary.csv` are residual/error checks with `expected_value=0.0`.  These zeros mean zero reconstruction error, not zero physical density or flux.


### F5 — directional anisotropy / PAD mapping

Directory: `srcEarth/test/F5`

Driver:

```bash
python srcEarth/test/F5/run_F5.py -np 4 -nt 16
```

Purpose.  F5 validates the parser-supported pitch-angle-distribution mapping in the gridless anisotropic density/spectrum path using a nontrivial `BA_PAD_EXPONENT=2` case.  The runner renders and runs three cases with identical field, spectrum, energy grid, points, and transmission settings:

```text
BA_PAD_MODEL ISOTROPIC
BA_PAD_MODEL COSALPHA_N   BA_PAD_EXPONENT 2
BA_PAD_MODEL SINALPHA_N   BA_PAD_EXPONENT 2
```

The primary reference is the exact complement identity

```text
sin^2(alpha) + cos^2(alpha) = 1
```

which implies, point-by-point and energy-by-energy,

```text
T_sin(E) + T_cos(E) = T_iso(E)
J_local_sin(E) + J_local_cos(E) = J_local_iso(E)
n_sin + n_cos = n_iso
F_sin + F_cos = F_iso
```

F5 also computes a high-energy straight-line semi-analytic PAD reference for the density ratios at three diagnostic points: 8 Re equator, 8 Re magnetic pole, and 6 Re equator.  These ratio checks are looser than the complement identity because they intentionally neglect finite magnetic bending.

Input and reference files:

```text
srcEarth/test/F5/AMPS_PARAM_F5_gridless.in
srcEarth/test/F5/reference_F5_pad_mapping.csv
```

The runner writes one case directory per PAD model under `test_output/F5_gridless`, plus `F5_summary.csv`, `F5_result.json`, and `reference_F5_pad_mapping_used.csv`.

### F11 — anisotropic PAD model sum-check

Directory: `srcEarth/test/F11`

Driver:

```bash
python srcEarth/test/F11/run_F11.py -np 4 -nt 16
```

Purpose.  F11 exercises the parser-supported anisotropic PAD models
`ISOTROPIC`, `SINALPHA_N`, `COSALPHA_N`, and `BIDIRECTIONAL` through
`#BOUNDARY_ANISOTROPY`.  It uses a dipole gridless density/spectrum setup and
checks exact PAD identities rather than an absolute density reference:

```text
BA_PAD_EXPONENT=0  -> every PAD model reduces to isotropic
COSALPHA_N(n)      -> identical to BIDIRECTIONAL(n)
```

The runner compares total density, integral flux channels, and the saved
`spectrum` quantities `T(E)`, `J_boundary(E)`, and `J_local(E)` for those
identity pairs.  The default matrix covers exponents `0, 1, 2, 4, 8` and writes
one rendered-input directory per model/exponent under `test_output/F11_gridless`.

Input and reference files:

```text
srcEarth/test/F11/AMPS_PARAM_F11_gridless.in
srcEarth/test/F11/reference_F11_pad_identities.csv
```


### F12 — day/night spatial boundary anisotropy identities

Directory: `srcEarth/test/F12`

Driver:

```bash
python srcEarth/test/F12/run_F12.py -np 4 -nt 16
```

Purpose.  F12 validates the parser-supported `DAYSIDE_NIGHTSIDE` spatial boundary weighting in the gridless anisotropic density/spectrum path.  It uses `FIELD_MODEL NONE`, `EFIELD_MODEL NONE`, `R_INNER=0`, `BA_PAD_MODEL=ISOTROPIC`, and output points with `X=0`.  In that geometry every sampled trajectory is allowed and the deterministic direction grid is exactly paired under `x -> -x`, so half of the exits are dayside (`x_exit>0`) and half are nightside (`x_exit<=0`).

The exact references are

```text
DAYSIDE_NIGHTSIDE(1,1)   = UNIFORM
DAY_ONLY(1,0)            = 0.5 * UNIFORM
NIGHT_ONLY(0,1)          = 0.5 * UNIFORM
DAY_ONLY + NIGHT_ONLY    = UNIFORM
DAYSIDE_NIGHTSIDE(2,0.5) = 1.25 * UNIFORM
```

The runner checks those identities for total density, omnidirectional integral flux, every requested energy channel, saved `T(E)`, `J_boundary(E)`, and `J_local(E)`.

Input and reference files:

```text
srcEarth/test/F12/AMPS_PARAM_F12_gridless.in
srcEarth/test/F12/reference_F12_daynight_step.csv
```

The runner writes one case directory per spatial model/factor combination under `test_output/F12_gridless`, plus `F12_summary.csv`, `F12_result.json`, and `reference_F12_daynight_step_used.csv`.


### F15 — density normalization from differential flux

Directory: `srcEarth/test/F15`

Driver:

```bash
python srcEarth/test/F15/run_F15.py -np 4 -nt 16
```

Purpose.  F15 isolates the density normalization step in the zero-field, all-open limit.  It runs one TABLE top-hat spectrum per center energy, with default centers `1, 10, 100, 1000 MeV`.  Each case uses `FIELD_MODEL NONE`, `EFIELD_MODEL NONE`, `R_INNER=0`, and `DS_TRANSMISSION_MODE DIRECT`, so `T(E)=1` and `J_local(E)=J_boundary(E)`.

For a finite top-hat with constant directional differential flux `J0` on `[E1,E2]`, the reference flux and density are

```text
F = 4π J0 (E2 - E1)
n = 4π J0/c * [sqrt(E2(E2+2m)) - sqrt(E1(E1+2m))]
```

where energies and the proton rest mass `m` are in MeV.  The density formula directly checks the relativistic `1/v(E)` factor because `v(E)/c = sqrt(E(E+2m))/(E+m)`.

Input and reference files:

```text
srcEarth/test/F15/AMPS_PARAM_F15_gridless.in
srcEarth/test/F15/reference_F15_top_hat.csv
```

The runner writes per-case directories under `test_output/F15_gridless`, each containing the rendered input, the generated TABLE spectrum, AMPS log, and the usual gridless density/spectrum/flux files.  The top-level F15 work directory contains `F15_summary.csv`, `F15_result.json`, and `reference_F15_top_hat_generated.csv`.


### F16 — blocked-access zero-flux regression

Directory: `srcEarth/test/F16`

Driver:

```bash
python srcEarth/test/F16/run_F16.py -np 4 -nt 16
```

Purpose.  F16 checks the exact zero-transmission limit of the gridless density/spectrum workflow.  It uses a nonzero power-law incident spectrum, but every diagnostic point is placed inside the inner absorbing sphere.  Because the trajectory classifier checks `R_INNER` before the first integration step, every direction and energy must be rejected immediately.

The analytical reference is therefore

```text
T(E) = 0
J_local(E) = 0
n = 0
F_total = 0
F_channel = 0
```

This catches leakage through blocked locations, stale transmission arrays, uninitialized channel fluxes, and zero-signal normalization errors.  The runner also verifies that `J_boundary(E)` remains positive so the test is a blocked-access limit rather than a zero-input spectrum case.

Input and reference files:

```text
srcEarth/test/F16/AMPS_PARAM_F16_gridless.in
srcEarth/test/F16/reference_F16_blocked_access.csv
```

The runner writes `F16_summary.csv`, `F16_result.json`, and `reference_F16_blocked_access_used.csv` under `test_output/F16_gridless`.


### ADAPTIVE_DT time-step control

`ADAPTIVE_DT` is a `#NUMERICAL` switch shared by Mode3D and gridless backward
cutoff/density tracing:

```text
ADAPTIVE_DT T   # default: DT_TRACE is the maximum allowed adaptive step
ADAPTIVE_DT F   # fixed-step regression mode: use DT_TRACE directly
```

The command-line override is:

```bash
-adaptive-dt T|F
```

Aliases `-fixed-dt`, `-no-adaptive-dt`, and `-use-adaptive-dt` are also accepted.
Use `ADAPTIVE_DT F` for pusher convergence tests where changing `DT_TRACE` must
change the actual integration step.  Production runs should normally use the
default adaptive mode.


### Run-banner diagnostics

Mode3D and gridless cutoff runs print the trace-control state in the AMPS banner:

```text
Particle mover
ADAPTIVE_DT
DT_TRACE and whether it is a fixed step or maximum allowed step
effective dt rule
MAX_TRACE_TIME
CUTOFF_MAX_TRAJ_TIME
effective cutoff trace-time cap
MAX_TRACE_DISTANCE
MAX_STEPS
adaptive limiter constants when ADAPTIVE_DT=T
```

This is important for interpreting C4.  If `ADAPTIVE_DT=T`, decreasing
`DT_TRACE` may not change the actual pusher step because the gyro-angle or
boundary-distance limiter can already be smaller.  In that case, the useful
convergence check is the mover/cap sensitivity, not the nominal `DT_TRACE`
sequence alone.

### C19 validation wrapper

C19 directional-work optimization (current implementation)
-----------------------------------------------------------

The production runner defaults to `--direction-coverage INSTRUMENT_APERTURES`, which
maps to the mission-neutral AMPS mode `DIRMAP_COVERAGE VECTOR_APERTURES`. For each
spacecraft/epoch, the runner writes `C19_directional_apertures.dat` containing the
physical detector LOOK boresight and roll/up vector for every active head. The
observational reference's telemetry-head provenance (`telemetry_head_east/west`) maps the
legacy physical EAST/WEST flux streams back to their actual instrument head IDs before
attitude lookup. With `--detector-orientation-source FILE` those vectors come from the
supplied time-dependent attitude product; the heads may point anywhere and need not be
antipodal. With the legacy `SM_PROXY` fallback the runner merely writes corresponding
`LOCAL_SM` vector records--the C++ solver itself has no EAST/WEST special case.

For the present GOES P4/P5 comparison, the widest P5 30° horizontal and 60° vertical
half-angles are used as a conservative pruning envelope. The actual channel fold still
uses channel-specific response/FOV information. `--direction-coverage FULL_SPHERE`
remains available for complete-sky diagnostics and bit-for-bit retained-cell regression
comparisons.

The current C19 science observable is `DIRECT_ACCESS` (the runner default).  It traces the
three-state detector-response access function `A(E,Omega)` directly and folds those
samples through the common spectrum/response/aperture postprocessor.  `PENUMBRA_SCAN`
remains an explicitly selectable, more expensive diagnostic that additionally produces
full lower/effective/upper cutoff topology; it is not the default acceptance observable.
The trace-limit policy for C19 remains conservative: a numerical time/step/path limit is
`UNRESOLVED` unless a positive physical escape, loss, bounce-trap, or drift-recurrence
criterion has already fired.

The current uncorrected-flux detector response also includes the documented EPEAD
high-energy secondary proton response (P4 through 150 MeV and P5 through 190 MeV), so
the direct rigidity list is derived from the full positive response support rather than
being truncated at the nominal 82 MeV P5 edge. With the default 48 logarithmic science
nodes plus exact response boundaries, the committed response produces 55 trajectory
energies from 15 to 190 MeV (about 0.168--0.627 GV for protons). The primary-only
response file remains available for corrected-flux/sensitivity work.

Finite rigidity sampling is now treated explicitly. The runner analytically integrates
the power-law SEP spectrum times the piecewise-constant detector response between
sampled energies. If two resolved access endpoints disagree, it does **not** create a
linear 0-to-1 transmission ramp; the whole interval is carried as an access-transition
uncertainty bracket. Intervals touching `UNRESOLVED` are tracked separately as
trajectory-resolution uncertainty. The default
`--max-discrete-transition-fraction 0.05` invalidates a quantitative detector fold when
more than five percent of its response-weighted support lies in unresolved transition
brackets. This makes `--access-energy-points` a measurable convergence control.

The production defaults now use the finest settings from the previous P2 convergence
implementation: a 2.5-degree directional grid and Mode3D mesh resolutions of 0.025 Re
near Earth and 1.0 Re at the outer boundary. Detector FILE orientation and explicit
bounded anisotropy remain ordinary physics inputs to the same workflow. The standard
`C19_comparison_*`, scatter, parity, residual, transmission, and aperture diagnostic
plots require no special runner flag.


### GRIDLESS mesh-free/threaded cutoff execution

Standalone `-mode gridless` returns before `amps_init_mesh()` and evaluates the selected
background model directly along trajectories.  Cutoff and directional direct-access
work support intra-rank `THREADS`, `OPENMP`, or `SERIAL` execution through
`GRIDLESS_PARALLEL`/`GRIDLESS_THREADS`, combined with the existing MPI scheduler.  Only
the rank/main thread calls MPI.  Automatic `GRIDLESS_MPI_DYNAMIC_CHUNK 0` sizes work
fetches from the local worker count.  Threaded direct-field execution requires one
frozen epoch/driver snapshot per process; multi-epoch trajectory inputs fall back to
serial intra-rank evaluation while preserving MPI parallelism.  See `gridless/READ.ME`
and `test/C19/README.md` for the detailed execution contract.

### C19 direct-access trace-budget, recurrence, and convergence controls (2026-08-19)

C19 uses the common three-state trajectory result (`PHYSICAL_FORBIDDEN`, `ALLOWED`,
`UNRESOLVED`) but the direct-access products now serialize the termination evidence
behind every evaluated trajectory.  GRIDDED and GRIDLESS output
`termination_code`, trace time, cumulative path, step/retry counts, mirror/bounce
counts, drift-revolution diagnostics, trapping mechanism, and relative momentum spread.
The DIRECT_ACCESS Tecplot headers carry the stable integer-to-name map as `AUXDATA`.
Historical codes 0--8 are unchanged; code 9 is appended as
`DRIFT_TRAPPED_FORBIDDEN`.

C19 production templates use

```text
MAX_TRACE_TIME       300.0
MAX_TRACE_DISTANCE   0.0
```

A nonpositive `MAX_TRACE_DISTANCE` disables the cumulative path-length ceiling so every
energy receives the same physical trace-time budget.  A fixed path cap is still supported
for machine safety and convergence experiments; because it is traveled path rather than
geocentric radius, its implied physical time depends on particle speed.  The runner
records that implied time in `C19_access_energy_grid.csv`.

The shared `TrajectoryTrapDetector` retains the legacy bounce-recurrence path and adds a
C19 full-orbit drift-recurrence path.  It does **not** classify a timeout as forbidden.
The drift branch unwraps signed geocentric azimuth, divides each completed drift turn into
azimuth-phase bins, and compares turn-to-turn averages of `r`, `z/r`, and
`cos^2(pitch)`.  A positive drift-trapped verdict requires enough complete revolutions,
sufficient profile coverage, consecutive recurrence in a configured fraction of common
bins, an outer-boundary margin, and acceptable momentum-magnitude spread.

Relevant controls are:

```text
TRAP_DRIFT_DETECTION                         T|F
TRAP_MIN_DRIFT_REVOLUTIONS                   integer >= 2   # C19 default 3
TRAP_DRIFT_RADIAL_GROWTH_TOL_RE              nonnegative    # absolute r tolerance
TRAP_DRIFT_RADIAL_REL_TOL                    nonnegative    # relative r tolerance
TRAP_DRIFT_LATITUDE_TOL                      nonnegative    # |Delta(z/r)|
TRAP_DRIFT_PITCH_COS2_TOL                    nonnegative
TRAP_DRIFT_PROFILE_BINS                      integer 8..360
TRAP_DRIFT_MIN_PROFILE_COVERAGE              0 < value <= 1
TRAP_DRIFT_MIN_MATCHED_BIN_FRACTION          0 < value <= 1
TRAP_ENERGY_REL_TOL                           nonnegative
```

The drift branch intentionally continues the full Lorentz trajectory instead of switching
to a guiding-centre approximation.  This is important for C19's 15--190 MeV protons near
GEO, where gyroradii can be non-negligible compared with magnetospheric gradients.  The
azimuth-bin averages reduce gyrophase sensitivity without changing the authoritative
orbit.

`test/C19/run_C19_convergence.py` implements the evidence sequence used to validate the
fix.  Its baseline distance and time sweeps turn drift recurrence **off** first, then add
a matched drift-recurrence case.  This makes it possible to demonstrate directly whether
the former EAST response weight was `DISTANCE_LIMIT`, `TIME_LIMIT`, or a genuinely
recurring bounded orbit.  Optional timestep/mover sweeps support further numerical
cross-checks.  The compact response-weighted result is written as
`C19_aperture_termination_budget.csv`; `C19_aperture_samples.csv` remains the per-cell
audit product.

C19 also restores the Stage-A rigidity-resolved access-classification product.  The
postprocessor writes `C19_access_classification_by_rigidity.csv` and one
`C19_access_classification_*.png` per selected observational case.  At every mandatory
common DIRECT_ACCESS seed rigidity it reapplies the physical EAST/WEST aperture and
reports geometric solid-angle fractions that are allowed, physically forbidden, or
unresolved, with detailed termination fractions retained in the CSV.  A dotted
secondary-axis curve is the normalized endpoint share of the exact `J(E)G(E)` interval
integrals, so the plot shows whether a problematic rigidity interval actually matters to
the detector signal.  Adaptive-only refinement nodes are intentionally excluded from
this diagnostic to keep all sky directions on the same rigidity grid.  The plot is
diagnostic only and is generated even when no direct scalar is accepted.

The direct detector fold also applies a rigorous `log10(E/W)` bound-width convergence
gate.  Excessive unresolved trajectory weight is
`INCONCLUSIVE_TRAJECTORY_RESOLUTION`; a resolved but too-wide direct interval is
`INCONCLUSIVE_DIRECT_BOUND_WIDTH`; only a resolved/narrow interval that excludes the
observation is `MODEL_MISMATCH`.  The direct-derived equivalent-cutoff diagnostic keeps
that conservative acceptance semantics but now also stores a *separate* plotting-only
blocked-area midpoint.  An unresolved interval still contributes its full uncertainty to
the rigorous lower/upper bounds and still suppresses resolved `Rc_effective`; the
diagnostic midpoint uses `0.5*dR` only so the historical cutoff-proxy comparison remains
visible and is never used to classify a trajectory or pass C19.

The C19 postprocessor preserves three DIRECT_ACCESS reporting levels rather than using
one nullable scalar for all purposes.  `direct_calculated_*` is the finite central value
produced by the direct detector fold **before** final scientific acceptance.  A later
bound-width or guardrail rejection does not erase this diagnostic value.  The legacy
`modeled_*` fields remain acceptance-only and therefore continue to define formal
metrics.  If the direct fold cannot construct a scalar but finite rigorous E/W bounds
exist, `direct_bound_midpoint_*` stores the midpoint only for visualization; it never
enters acceptance or residual metrics as an accepted value.

All comparison-family plots use the shared `direct_plot_groups()` classifier.  Accepted
direct scalars are filled markers, calculated-but-not-accepted scalars are open direct
markers, bounds-only rows are open diamonds with their rigorous interval, and the
equivalent-cutoff midpoint remains a separate green diagnostic.  This common selection
contract fixes the earlier inconsistency in which parity could display a direct P5 point
that scatter omitted because each plot had independently filtered `status`.

Core comparison plotting is additionally fail-safe. Reference and model UTCs are both
converted to Python `datetime` values before plotting; mixing ISO strings with datetime
objects had allowed Matplotlib to fail unit conversion/categorical-axis setup in some
multi-epoch post-processing runs. Since the scatter figure followed the time-series
figure inside the same routine, that single exception could remove both
`C19_comparison_*` and `C19_scatter_*`. After the normal renderer, C19 now calls a
minimal fallback that reconstructs only either missing core file from `ModelRow` data.
The fallback does not overwrite successful primary plots, does not alter metrics or
status, and is fully compatible with `--skip-run` because no solver output is recomputed.
`C19_model_coverage.csv` now reports `DIRECT_ACCEPTED`,
`DIRECT_CALCULATED_NOT_ACCEPTED`, `DIRECT_BOUNDS_ONLY`, or diagnostic/missing states, and
`C19_result.json:plot_consistency` records matching serialized-versus-plotted counts.
Ordinary runs use `direct_convergence_status=NOT_TESTED`; a convergence campaign may
subsequently validate the result, but absence of such a campaign is not treated as a
failed calculation and cannot hide the direct scalar.

For publication-oriented runs the runner can enforce real detector attitude, an
independent incident spectrum, and calibrated detector-response provenance through
`--require-real-orientation`, `--require-independent-spectrum`, and
`--require-calibrated-response`.  The committed response CSVs identify themselves as
`NOMINAL_FACTORIZED` and intentionally do not satisfy the calibration gate.


### C19 response-weighted frozen-field validity guardrail (2026-08-21)

C19 traces particles through a magnetic-field snapshot that is frozen at the selected
observation epoch.  Long trajectories therefore carry an additional modelling
uncertainty: even when the Lorentz integration itself is numerically valid, a particle
that requires several minutes to escape or to establish a bounded orbit samples a real
magnetosphere that would have evolved during that interval.

The historical guardrail tested only the longest contributing trajectory,

```text
max(trace_time) > FROZEN_FIELD_WARNING_TIME  -> reject the complete direct scalar
```

which was too conservative for an aperture-integrated detector observable.  It ignored
the `J(E) G(E,Omega) dE dOmega` weight of the long trajectory, so one negligible
sky/energy sample could veto an otherwise well-resolved E/W ratio.  It also used a
warning time numerically equal to the common 300-s trace cap; an RK4 trajectory that
crossed the cap on its final finite step (for example 300.25 s for `DT_TRACE=0.25 s`)
could therefore trigger the physical guard solely because of integration-step
overshoot.

The current implementation replaces that single-trajectory veto with a
**response-weighted sensitivity assessment**.  For every DIRECT_ACCESS aperture fold,
`run_C19.py` apportions the exact interval integral of the assumed incident spectrum and
detector response between the two rigidity endpoints, multiplies it by the same
solid-angle/source-anisotropy factor used by the detector fold, and accumulates the
fraction of detector signal represented by trajectories longer than

```text
T_long = --frozen-field-warning-seconds
         + --frozen-field-time-tolerance-seconds .
```

The default tolerance is the AUTO sentinel `-1`, resolved to `0.5*DT_TRACE`; this keeps a
normal one-step trace-limit overshoot from being interpreted as new physical evidence.
The fold reports total long-trace and >2*T_warning fractions, response-weighted p50/p90/
p99 trace times, and partitions the long response into allowed, physically forbidden,
and still-unresolved populations.

The guardrail deliberately distinguishes three levels:

1. **static-field warning** -- any nonzero detector response comes from a trajectory
   beyond `T_long`.  This is provenance only and does not erase a resolved scalar.
2. **long-trace dominance warning** -- the long-duration response exceeds
   `--max-long-trace-response-fraction` (default 0.05).  This remains advisory because a
   positively resolved long trajectory is not equivalent to trajectory uncertainty.
3. **hard static-field guard** -- the response fraction that is both long-duration and
   still `UNRESOLVED` exceeds `--max-long-unresolved-response-fraction` (default 0.05),
   or an explicitly enabled conservative observable-sensitivity width exceeds
   `--max-static-field-ratio-bound-width-log10`.  The resulting status is
   `STATIC_FIELD_DOMINATED`.

The hard observable-sensitivity diagnostic is constructed without relabelling any
trajectory.  C19 recomputes conservative EAST/WEST signal bounds in which every energy
interval touching a long-duration trajectory is allowed to span `[0, full interval
signal]`.  These bounds are propagated to `static_field_log10_east_west_ratio_min/max`
and their width.  The width gate is disabled by default (`-1`) because an
experiment-independent publication tolerance has not yet been calibrated; it can be
enabled explicitly in convergence/publication studies.

This design preserves the core DIRECT_ACCESS rule: **timeout or long duration is never
converted to physical shielding**.  A long trajectory that is positively classified as
outer-boundary allowed, inner-boundary forbidden, or bounce/drift trapped contributes to
the warning provenance but does not by itself invalidate the scalar.  A long trajectory
that remains unresolved contributes to the hard uncertainty budget.

The implementation is auditable through `C19_aperture_availability.csv`,
`C19_aperture_termination_budget.csv`, and the ModelRow fields written to
`C19_comparison.csv`.  These products include long/very-long response fractions,
allowed/forbidden/unresolved long fractions, response-weighted trace-time percentiles,
static-field signal bounds, warning flags, and hard-guard status.  The dedicated
`C19_static_field_guardrail_<solver>_<field>.png` plot shows EAST/WEST long-response and
long+unresolved fractions together with the conservative static-field E/W bound width.
`C19_result.json` records aggregate counts and maximum observed fractions.

If a scientifically important fraction of the detector signal remains dependent on
long-duration trajectories, the preferred next physics step is not to keep extending a
frozen T05 snapshot indefinitely.  The guardrail is intended to identify precisely when
a time-dependent/interpolated magnetospheric background along the backward trajectory
becomes necessary.


See `test/C19/README.md` for the detailed phased algorithm, acceptance logic, output
schema, and required full AMPS regression sequence.

### C19 unresolved-only staged trajectory extension (2026-08-23)

C19 now reduces the response-weighted unresolved population without weakening the
three-state DIRECT_ACCESS test.  The normal primary trace budget remains unchanged.
Only samples whose **primary** classifier terminated at `TIME_LIMIT` or `STEP_LIMIT`
are retraced at larger total-time budgets.  The controls are

```text
CUTOFF_UNRESOLVED_EXTENSION_PASSES
CUTOFF_UNRESOLVED_EXTENSION_FACTOR
```

and default to zero outside tests that explicitly enable them.  The committed C19
inputs use two factor-of-two passes with `CUTOFF_MAX_TRAJ_TIME=300 s`, giving
300 -> 600 -> 1200 s.  Resolved escape, inner-boundary loss, and positively detected
bounce/drift trapping are never repeated.  `DISTANCE_LIMIT` is intentionally excluded
because a cumulative path cap is an independent safety/convergence assumption and must
not be silently relaxed by a time-convergence feature.

Each extension restarts the full Lorentz trajectory from the original phase-space seed.
A true continuation would be cheaper, but would require serializing private mover and
trap-detector state and could give GRIDDED and GRIDLESS subtly different continuation
semantics.  Restarting makes each extended result directly equivalent to an independent
run with the larger total-time limit.  If the final pass still reaches a trace safety
limit, the state remains `UNRESOLVED`; elapsed time is never reinterpreted as magnetic
shielding.

The extended pass also increases `MAX_STEPS` using both the requested time scaling and
the preceding trace's observed mean timestep, with a 25% safety margin and integer
clamping.  This prevents a larger `T_max` from being defeated by an unchanged step cap.
The pre-existing one-time retry for genuine numerical failures remains separate and is
still represented by `retry_count`.

`TrajectoryResult` and the DIRECT_ACCESS Tecplot product now preserve
`primary_termination_code`, `primary_trace_time_s`, `trace_extension_count`,
`initial_trace_limit_s`, `final_trace_limit_s`, and `drift_mean_radius_change_Re`.
The C19 postprocessor uses the same exact endpoint-attributed `J(E)G(E)` response budget
as the existing termination accounting to report the response fraction that hit a
primary TIME/STEP limit, the fraction physically resolved by staged extension, and the
fraction still unresolved after the final pass.  These diagnostics are serialized in
the model/aperture tables and visualized by
`C19_trace_extension_<solver>_<field>.png`; no existing C19 plot family is replaced.

The drift-shell recurrence classifier also has an optional conservative secular-drift
veto:

```text
TRAP_DRIFT_MAX_MEAN_RADIUS_CHANGE_RE
```

A value of zero disables the veto.  When enabled, detailed azimuth-profile recurrence
must also have sufficiently small change in the mean radial profile between consecutive
completed drift revolutions.  The mean is formed from per-bin means, so adaptive
full-orbit timesteps do not overweight one azimuth sector.  This test can only reject a
candidate `DRIFT_TRAPPED_FORBIDDEN` classification; it cannot create one.

Longer successful traces are still subject to the response-weighted frozen-field
validity diagnostics.  The staged extension improves classification convergence but does
not make a 600--1200 s frozen T05 snapshot physically time dependent.  If important
detector response remains long-duration or unresolved after the final pass, evaluate a
time-dependent/interpolated magnetic background rather than extending the static trace
budget indefinitely.
