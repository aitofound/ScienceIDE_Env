# C19A — GOES EPEAD East–West directional-access validation

## 1. Purpose

C19A is the first observational validation of the **directional** geomagnetic-access capability of the AMPS cutoff calculator. Internal symmetry tests such as C17 can verify charge-sign and velocity-reversal consistency, but they do not establish that the calculated directional access agrees with measurements. C19A compares the AMPS directional access function with simultaneous eastward- and westward-looking proton measurements from the GOES-13 and GOES-15 EPEAD instruments. The production path uses the explicit three-state `A(E,Ω)` access cube; a full directional cutoff/penumbra map is generated only when `--cutoff-search PENUMBRA_SCAN` is requested.

The implemented public-data event is the 17 May 2012 SEP/GLE71 event. The default analysis interval is the event decay from 2012-05-17 06:00 UTC through 2012-05-18 06:00 UTC. The prompt onset is excluded because interplanetary beam anisotropy can imitate or obscure a geomagnetic East–West effect.

The primary observable is

```text
log10[(physical EAST background-subtracted flux) /
      (physical WEST background-subtracted flux)]
```

for EPEAD P4 and P5. A negative value means that the eastward-looking detector measured less flux than the westward-looking detector.

C19A is a **broad-aperture observational validation**. The current science chain is explicit: the reference records the exact NOAA flux variables and ephemeris provenance; a time-dependent event spectrum is estimated from the physical-WEST P4/P5 measurements (or supplied independently); both GRIDDED Mode3D and GRIDLESS evaluate the same direct three-state access function `A(E,Ω)` on the directional sky grid; and the postprocessor folds that access through a file-defined detector energy response and the nominal elliptical angular aperture. The current runner also includes fine angular/mesh defaults, external detector-attitude support, a bounded upstream-anisotropy option, explicit finite-rigidity-grid uncertainty, and a high-energy response extension appropriate to the default uncorrected GOES product. The response is still factorized in energy and angle rather than a complete calibrated energy-angle matrix, so the test remains a controlled approximation rather than a full instrument simulator.

## 2. Implemented comparison

| Item | C19A implementation |
|---|---|
| Spacecraft | GOES-13 and GOES-15 |
| Event | 17 May 2012 SEP/GLE71 decay |
| Observation cadence | Public NOAA/NCEI 5-minute EPEAD product |
| Primary channels | P4: 15–40 MeV; P5: 38–82 MeV |
| P4 nominal FOV | ±45° north–south, ±25° equatorial |
| P5 nominal FOV | ±60° north–south, ±30° equatorial |
| Observation | Background-subtracted physical East/West flux ratio |
| Field models | IGRF + T96 and IGRF + T05/TS05 |
| Solvers | GRIDDED, GRIDLESS, or BOTH |
| AMPS product | Default: `DIRECT_ACCESS` three-state `A(E,Ω)` cube only. Optional diagnostic: `PENUMBRA_SCAN` map plus the same direct `A(E,Ω)` companion cube |
| Source spectrum | Default: epoch-dependent power law fit to physical-WEST background-subtracted P4/P5 flux; explicit spectrum CSV and fixed-gamma sensitivity modes are supported |
| Instrument model | File-defined piecewise energy response (`data/epead_response_C19_uncorrected_extended.csv` by default) × nominal elliptical angular aperture; identical direct `A(E,Ω)` fold for GRIDDED and GRIDLESS |

### Event-specific detector orientation

The NOAA files use invariant telemetry labels `E` and `W`; these labels are not themselves guaranteed to identify physical east and west because GOES-13–15 can change yaw orientation. The May 2012 mapping follows Rodriguez et al. (2014), Table 2:

| Spacecraft | Telemetry W | Telemetry E |
|---|---|---|
| GOES-13 | physical EAST | physical WEST |
| GOES-15 | physical WEST | physical EAST |

The mapping is stored in `event_C19_may2012.json`. It must be changed when C19A is extended to a different event.

## 3. Directory contents

```text
srcEarth/test/C19/
├── README.md
├── AMPS_PARAM_C19_gridless.in
├── AMPS_PARAM_C19_mode3d.in
├── C19_trajectory.txt
├── build_goes_reference.py
├── event_C19_may2012.json
├── run_C19.py
├── tests/
│   └── run_self_tests.py
└── data/
    ├── reference_C19_goes_epead_ew.csv.gz
    ├── reference_C19_goes_epead_ew_provenance.json
    ├── epead_response_C19_uncorrected_extended.csv
    ├── epead_response_C19_nominal.csv
    └── ts05_driver_may2012.txt
```

The compact observational reference and the five-minute T05 driver used by this
snapshot are committed with the test so a routine C19 run is reproducible without a
network download.  `build_goes_reference.py` remains the supported way to regenerate
the GOES reference from the public NOAA files; its provenance JSON records the inputs
and hashes.

## 4. Requirements

### AMPS executable

C19A requires an AMPS executable with:

- standalone Earth `gridless` and/or `3d` cutoff calculations;
- `DIRECTIONAL_MAP T` output;
- T96 and T05 field models;
- SPICE enabled and the required kernels loaded;
- trajectory input in `TRAJ_FRAME GEO`;
- MPI; and
- POSIX-thread field initialization only when `--mode3d-parallel-field-init` is requested.

SPICE is required because each GOES latitude/longitude/altitude sample is transformed from ITRF93/GEO to GSM and because the directional-map labels are defined in SM and rotated to GSM at the selected epoch.

### Python

Python 3.8 or newer is supported.  The C19 runner and CSV reference-building path use
the Python standard library for their core workflow.  Reading the native NOAA/NCEI
`goes-l2-orb1m` NetCDF ephemeris requires `xarray`; converting that ephemeris to CSV
removes the NetCDF dependency.  Plot generation is optional and requires Matplotlib:

```bash
python3 -m pip install xarray matplotlib
```

If Matplotlib is unavailable, C19 still writes the CSV/JSON diagnostics and reports that
plot generation was skipped.

### MPI launcher

The runner uses `mpirun` by default, as do the other AMPS test runners. Use `--mpirun <launcher>` only when a different command is required by the local system.

## 5. Obtain and build the observational reference

### 5.1 Public NOAA files

The builder uses these fixed public monthly files from the historical NOAA/SWPC operational-average tree. These exact files preserve the directional `p17ew` E/W-head schema used by C19A. NCEI also publishes a newer Version 1 collection for general GOES 1–15 use; replacing the fixed C19A sources requires revalidating variable definitions, detector-head mapping, quality flags, and the resulting reference.

```text
GOES-13:
https://www.ncei.noaa.gov/data/goes-space-environment-monitor/access/avg/2012/05/goes13/csv/g13_epead_p17ew_5m_20120501_20120531.csv

GOES-15:
https://www.ncei.noaa.gov/data/goes-space-environment-monitor/access/avg/2012/05/goes15/csv/g15_epead_p17ew_5m_20120501_20120531.csv
```

From the AMPS repository root, download the particle files and supply the official NOAA/NCEI one-minute GOES ephemeris for both spacecraft. The reference builder intentionally does **not** silently replace a missing ephemeris with a nominal slot:

```bash
python3 srcEarth/test/C19/build_goes_reference.py --download \
  --goes13-ephemeris /path/to/goes13_goes-l2-orb1m.nc \
  --goes15-ephemeris /path/to/goes15_goes-l2-orb1m.nc
```

Native NOAA NetCDF (`geo_llr`) and CSV ephemeris files are supported. For an explicit legacy reproduction only, `--allow-nominal-ephemeris` restores the nominal-slot fallback and labels every such row `NOMINAL_GEO_SLOT_LEGACY_OVERRIDE`.

This writes:

```text
srcEarth/test/C19/data/cache/g13_epead_p17ew_5m_20120501_20120531.csv
srcEarth/test/C19/data/cache/g15_epead_p17ew_5m_20120501_20120531.csv
srcEarth/test/C19/data/reference_C19_goes_epead_ew.csv.gz
srcEarth/test/C19/data/reference_C19_goes_epead_ew_provenance.json
```

The provenance file records source URLs and SHA-256 values, manifest checksum, background method, directional mapping, exact flux-product policy, exact source flux variables actually selected, ephemeris policy, and generated-reference checksum. The same flux-variable/correction-state fields are also embedded in every reference row, so a sub-selected `C19_reference_used.csv` remains auditable without the sidecar JSON.

To use files downloaded separately, provide both the directional particle products and
the real one-minute ephemeris products:

```bash
python3 srcEarth/test/C19/build_goes_reference.py \
  --goes13-particle /path/to/g13_epead_p17ew_5m_20120501_20120531.csv \
  --goes15-particle /path/to/g15_epead_p17ew_5m_20120501_20120531.csv \
  --goes13-ephemeris /path/to/goes13_orb1m.nc \
  --goes15-ephemeris /path/to/goes15_orb1m.nc
```

### 5.2 Reference construction

The current reference builder removes the previous silent `UNCOR_FLUX → FLUX → COR_FLUX` fallback. The event manifest now declares `goes_flux_product_policy`, and the builder accepts an explicit override:

```bash
--flux-product UNCORRECTED|CORRECTED|AUTO
```

`UNCORRECTED` accepts only `*_UNCOR_FLUX`; `CORRECTED` never falls back to an uncorrected variable; `AUTO` exists only for deliberate legacy reproduction. Each output row records `east_flux_variable`, `west_flux_variable`, and their correction-state labels.

For every spacecraft, channel, and telemetry head, the script calculates the median valid flux over the manifest background interval:

```text
2012-05-16 00:00–12:00 UTC
```

It retains a sample only when:

- both physical directions have finite positive raw flux;
- available quality flags are zero;
- both background-subtracted fluxes are positive; and
- `(raw-background)/background` is at least 3 for both directions by default.

The threshold can be changed explicitly:

```bash
python3 srcEarth/test/C19/build_goes_reference.py --download \
  --min-signal-to-background 2.0
```

Changing this value changes the observational reference and is recorded in the provenance file.

### 5.3 Spacecraft position

Science reference generation requires real spacecraft ephemeris. The builder accepts the official NOAA/NCEI GOES 6–15 one-minute orbit product in native NetCDF form (`goes-l2-orb1m`, variable `geo_llr = [latitude, longitude, geocentric radius]`) or a CSV containing UTC/longitude/latitude and altitude or radius. The nearest sample within 180 s is used.

```bash
python3 srcEarth/test/C19/build_goes_reference.py \
  --goes13-particle /path/to/g13_particle.csv \
  --goes15-particle /path/to/g15_particle.csv \
  --goes13-ephemeris /path/to/goes13_orb1m.nc \
  --goes15-ephemeris /path/to/goes15_orb1m.nc
```

If a selected particle epoch has no real ephemeris within 180 s, reference generation fails. `--allow-nominal-ephemeris` is an explicit legacy override only. The runner can additionally enforce science provenance with `--require-real-ephemeris`.

### 5.4 Reference-builder self-test

```bash
python3 srcEarth/test/C19/build_goes_reference.py --self-test
```

This uses synthetic NOAA-format files to test header discovery, P4/P5 parsing, quality/background filtering, the event-specific E/W mapping, gzip output, and provenance generation. It does not contact NOAA.

## 6. T05/TS05 driver used by C19

This source snapshot contains the validated five-minute event driver directly at:

```text
srcEarth/test/C19/data/ts05_driver_may2012.txt
```

`run_C19.py` uses that file by default.  It independently verifies the
timestamp-plus-19-value schema, strict time ordering, five-minute median cadence,
maximum ten-minute internal gap, finite values, and coverage of all selected
observation epochs before launching AMPS.  The source/archive provenance is retained
in the comments at the top of the committed driver and the runner records its SHA-256
in `C19_result.json`.

If the event driver is regenerated externally, pass the replacement explicitly with
`--driver /path/to/driver.txt`; the same validation is applied before any run.

## 7. Verify the package before a production run

Run all non-network package self-tests:

```bash
python3 srcEarth/test/C19/tests/run_self_tests.py
```

The package test compiles both Python programs, runs their unit/self-tests, performs a
normal BOTH-solver dry run, and performs a P0 dry run that verifies the A/B/C
algorithm-policy matrix, the 400/800/1600-Re trace-budget scaling, and the centered-
dipole Störmer-anchor input.  No AMPS executable or network access is required for
these checks.

The component tests can also be run separately:

```bash
python3 srcEarth/test/C19/build_goes_reference.py --self-test
python3 srcEarth/test/C19/run_C19.py --self-test
```

After generating the public reference and driver, preview the exact AMPS commands and rendered inputs:

```bash
python3 srcEarth/test/C19/run_C19.py \
  --profile SMOKE \
  --solver GRIDDED \
  --models T96,T05 \
  --dry-run \
  --amps ./amps \
  -np 4 -nt 16
```

## 8. Run C19A

### 8.1 Quick smoke run

The SMOKE profile is a **synchronized three-snapshot regression**.  It first finds the
observation epochs for which every requested channel is present for every requested
spacecraft, intersects those epoch sets, and then retains the first, index-middle, and
last epoch from that common set.  If a deliberately shortened reference contains only
one or two common epochs, all of those common epochs are retained.

This differs intentionally from the older C19 behavior, which selected first/middle/last
independently for each spacecraft.  Because GOES-13 and GOES-15 do not have exactly the
same retained time range, independent selection could create five unique magnetic-field
snapshots while each spacecraft comparison panel contained only three model points.  In
the current SMOKE profile, a normal two-spacecraft/two-channel run has the simple
relationship

```text
3 common observation epochs
        =
3 Mode3D field snapshots
        =
3 model epochs for GOES-13 and 3 model epochs for GOES-15
```

The common-epoch requirement also includes **complete requested-channel coverage**.  For
example, with the default `--channels P4,P5`, an epoch is eligible for SMOKE only if both
P4 and P5 exist for every requested spacecraft.  This prevents a missing channel row
from silently shortening one comparison panel.  If no such common epoch exists, SMOKE
stops with an explicit error; use ROUTINE or FULL when independently valid spacecraft
coverage is desired instead.

`--time-step-minutes` does not alter SMOKE selection.  It remains a cadence override for
ROUTINE/FULL only.

```bash
python3 srcEarth/test/C19/run_C19.py \
  --profile SMOKE \
  --solver GRIDDED \
  --models T96,T05 \
  --amps ./amps \
  -np 4 -nt 16
```

For a GRIDDED batch, verify the synchronized selection directly in
`C19_snapshot_epochs.txt` and `C19_batch_manifest.csv`: the former should contain the
three common field epochs, and the latter should contain every requested spacecraft at
each of those epochs.

### 8.2 Routine regression

The ROUTINE profile samples the generated five-minute reference at 60-minute spacing:

```bash
python3 srcEarth/test/C19/run_C19.py \
  --profile ROUTINE \
  --solver GRIDDED \
  --models T96,T05 \
  --amps ./amps \
  -np 4 -nt 16
```

This down-sampling is intentional and is the main reason a default ROUTINE comparison
contains far fewer model points than the committed reference file.  The reference is
five-minute cadence; ROUTINE keeps at most one spacecraft epoch per 60 minutes.  Every
run now prints and records the number of eligible rows before the profile filter and the
fraction actually selected.  Use either

```bash
--profile FULL
```

or equivalently

```bash
--time-step-minutes 0
```

when the purpose is to calculate a model result at every valid reference epoch.  For
GRIDDED mode this no longer implies one process launch per epoch: the default batch path
still reuses one Mode3D mesh per field model and processes all selected snapshots inside
that launch.

The default reference and driver paths are used automatically. Explicit equivalents are:

```bash
--reference srcEarth/test/C19/data/reference_C19_goes_epead_ew.csv.gz
--driver srcEarth/test/C19/data/ts05_driver_may2012.txt
```

### 8.3 Parallel Mode3D field initialization

```bash
python3 srcEarth/test/C19/run_C19.py \
  --profile ROUTINE \
  --solver GRIDDED \
  --models T96,T05 \
  --mode3d-parallel-field-init \
  --amps ./amps \
  -np 4 -nt 16
```

The same `-nt` value controls the Mode3D cutoff thread backend and the number of temporary POSIX workers requested for background-field initialization. The caller also participates in the field initialization, following the implementation documented by C18.

During each T96/T05/TA16/IGRF/DIPOLE snapshot initialization, MPI rank 0 now prints a
**flushed global field-initialization progress bar**.  Its syntax intentionally mirrors
the later cutoff bar used by C19:

```text
[Mode3D field INITIALIZATION] [rank 0/global over 6 MPI ranks] [##############----------------------] 38.9%  (Cell 1373/3530)  ETA 00:04:52
[Mode3D cutoff TRAJECTORY] [rank 0/global over 6 MPI ranks] [##############----------------------] 38.9%  (Task 1373/3530)  ETA 00:05:01
```

Both bars use 36 characters, `#` for completed work, `-` for remaining work, one
decimal place for percent, and the same `HH:MM:SS` ETA.  Field initialization is
intentionally quieter: normal updates are printed every **2 s**, a factor of two less
frequently than the one-second cutoff bar.  The initial and final lines are always
printed, and every line calls `std::cout.flush()` so users following an MPI/batch log
see the update immediately.

The reported field count is a completion count over all MPI ranks.  Temporary pthread
workers never call MPI; they increment only a rank-local atomic counter, while the
original rank thread publishes aggregate deltas with the same MPI-RMA progress-counter
infrastructure used by the cutoff scheduler.  Thus live background-field progress does
not change AMPS' MPI thread-level requirement and does not alter field values or the C19
trajectory calculation.

### 8.4 GRIDLESS/GRIDDED cross-solver run

```bash
python3 srcEarth/test/C19/run_C19.py \
  --profile ROUTINE \
  --solver BOTH \
  --models T96,T05 \
  --amps ./amps \
  -np 4 -nt 16
```

### 8.5 Full five-minute comparison

```bash
python3 srcEarth/test/C19/run_C19.py \
  --profile FULL \
  --solver GRIDDED \
  --models T96,T05 \
  --amps ./amps \
  -np 4 -nt 16
```

GRIDDED mesh reuse is enabled by default. The runner makes one AMPS launch per field
model and cutoff-search configuration, allocates the Mode3D mesh once, and processes
all selected spacecraft/epochs through the multi-snapshot loop documented below.
P4 and P5 already share the same access cube for a spacecraft case and continue to be
folded independently in postprocessing. GRIDLESS remains one launch per selected
`(epoch, spacecraft, field model)` because it does not allocate a persistent field
mesh.

### 8.6 GRIDDED mesh-reuse batching

The historical runner launched Mode3D separately for every spacecraft epoch. Each
process repeated `PIC::InitMPI()`, `amps_init_mesh()`, `amps_init()`, sphere setup, and
distributed block allocation even though the domain and AMR resolution were unchanged.
The default `--gridded-batch AUTO` path now groups all compatible GRIDDED cases by:

```text
(field model, cutoff-search algorithm, mesh/domain settings, numerical controls)
```

For each group the runner writes:

```text
C19_trajectory.txt          all spacecraft locations in stable case order
C19_snapshot_epochs.txt     sorted unique, possibly irregular observation epochs
C19_directional_apertures.dat
C19_batch_manifest.csv      case -> snapshot suffix/local location mapping
AMPS_PARAM_C19.in           TEMPORAL_MODE SNAPSHOT_LIST
```

Mode3D performs the following lifecycle:

```text
initialize MPI/SPICE
allocate AMR tree and distributed blocks once
initialize static sphere/data layout once
for each unique snapshot epoch:
    interpolate the field driver at that epoch
    select only trajectory rows timestamped at that epoch
    remap their LOCATION-qualified apertures to snapshot-local location IDs
    refill mesh B/E and rebuild the compact global tracing arrays
    run cutoff/direct-access work for the active locations
```

The per-epoch location selection is essential. Ordinary `TIME_SERIES` semantics run
the complete output domain at every snapshot; using that mode directly would produce
an incorrect `N_snapshots × N_locations` cross-product. `SNAPSHOT_LIST` instead means
independent timestamped cases and requires at least one matching trajectory row for
every listed epoch.

The mesh topology and allocation are reused, but the magnetic field is intentionally
refilled for every unique epoch because T05/T96 drivers and SM/GSM rotations change
with time. If GOES-13 and GOES-15 have observations at the same epoch, they share both
the mesh allocation and that epoch's field initialization. The compact replicated
B/E/presence vectors are also resized only when their dimensions change and otherwise
cleared in place. Leaf `Temp_ID` values are still reset for every snapshot because that
field is shared AMPS scratch storage and cannot safely be cached across products.

For compatibility/regression comparison, restore the old process layout with:

```bash
python3 srcEarth/test/C19/run_C19.py ... --gridded-batch OFF
```

`SNAPSHOT`, `TIME_SERIES`, GRIDLESS, and all non-C19 input decks retain their existing
behavior; batching is activated only by the explicit C19-generated `SNAPSHOT_LIST`
deck.

### 8.7 Custom cadence or time interval

```bash
python3 srcEarth/test/C19/run_C19.py \
  --profile FULL \
  --time-step-minutes 15 \
  --start 2012-05-17T08:00:00Z \
  --end   2012-05-17T20:00:00Z \
  --solver GRIDDED \
  --models T05 \
  --amps ./amps
```

### 8.8 Direction-mapping diagnostic

The AMPS cutoff implementation and the GOES detector geometry use two different
vector meanings that must not be conflated:

- the AMPS directional-map vector is the **incoming particle arrival/velocity direction** at the observation point; the cutoff code backtraces by launching the reversed vector, and its vertical-arrival direction points toward Earth;
- the GOES EPEAD EAST/WEST direction is a telescope **look direction**. A telescope looking toward `+d` receives particles whose forward-time velocity is approximately `-d`.

Therefore the production C19 mapping is fixed as:

```text
detector look direction = - AMPS directional-map arrival direction
```

This is implemented in one audited helper in `run_C19.py`; it is not a fit parameter.
The runner also reproduces the **old direct-vector comparison** in postprocessing
as `LEGACY_DIRECT_DIAGNOSTIC` and writes both results to
`C19_direction_sense_diagnostic.csv`. The legacy diagnostic never contributes to
acceptance. Its purpose is to make an east/west reversal obvious and to preserve a
direct comparison with older C19 output without rerunning AMPS.

In `C19_aperture_diagnostic.png`, **EAST** and **WEST** are the two physical EPEAD
detector streams used as the numerator and denominator of the observed ratio. They are
not the eastern and western halves of the plotted SM sky, and the colors do not classify
a direction as eastward or westward. Each marker is an AMPS particle-arrival direction
that falls inside the corresponding detector head after the arrival-to-look reversal
above. Circles denote cells selected by the physical EAST head and squares denote cells
selected by the physical WEST head. With the default `SM_PROXY` geometry the two
clusters commonly appear near opposite longitudes; a FILE attitude can place them
elsewhere and they need not be exactly antipodal.


### 8.9 Single current workflow

C19 no longer exposes P0/P1/P2 as alternate execution modes. Those names describe the
development history only. The validated changes are integrated into every ordinary run.
In particular, the runner always renders:

```text
CUTOFF_SEARCH_ALGORITHM   PENUMBRA_SCAN
CUTOFF_TRACE_LIMIT_POLICY UNRESOLVED
```

and both GRIDDED Mode3D and GRIDLESS always request the direct three-state `A(E,Ω)` companion cube used
for the synthetic detector fold. The current production defaults also adopt the finest
settings from the former P2 convergence implementation:

```text
directional grid:       2.5° × 2.5°
near-Earth mesh:        0.025 Re
boundary mesh:          1.0 Re
mover:                  RK4
upstream model:         ISOTROPIC (unless explicitly changed)
```

There is therefore no `--p0-diagnostic` or `--p2-diagnostic` runner flag. A normal
SMOKE/ROUTINE/FULL invocation follows the same code path and, after successful
post-processing, always produces the standard comparison plots. Numerical studies can
still be performed by explicitly changing physical/numerical inputs such as angular
resolution, mesh resolution, trace budget, detector orientation, or anisotropy, but
those changes do not select a different runner implementation.

## 9. Numerical calculation and validity architecture

For each selected spacecraft and epoch, the runner writes a one-line GEO trajectory file:

```text
UTC latitude_deg longitude_deg_east altitude_km
```

AMPS represents detector directions on a regular global SM longitude/latitude grid.
The current C19 default angular resolution is 2.5° × 2.5°. Unless `FULL_SPHERE` is
selected, each observation location is pruned to its own requested instrument apertures;
Mode3D keeps a compact multi-location union only as an internal storage coordinate. In
the default `DIRECT_ACCESS` mode AMPS writes only the explicit three-state access cube on
that location's retained cells.
`PENUMBRA_SCAN` additionally writes a long-form directional cutoff map. The first four
historical variables in that optional map are preserved:

```text
lon_deg lat_deg Rc_GV Emin_MeV
```

and the following diagnostics are appended:

```text
Rc_lower_GV
Rc_effective_GV
Rc_upper_GV
n_transitions
n_allowed_intervals
n_unresolved_samples
lower_bracket_unresolved
upper_bracket_unresolved
lower_below_range / lower_above_range
upper_below_range / upper_above_range
n_trajectory_evaluations
n_outer_boundary_allowed
n_inner_boundary_forbidden
n_magnetically_trapped_forbidden
n_time_limit
n_step_limit
n_distance_limit
max_trace_time_s
max_trace_distance_Re
max_trace_steps
```

Mode3D DIPOLE maps can additionally contain `Rc_stormer_GV` for independent standalone Størmer regression checks.

`Rc_GV` remains the scalar value consumed by existing readers. For a directional
`PENUMBRA_SCAN` map it is `Rc_effective_GV`; the lower and upper cutoffs and topology are
retained explicitly instead of being discarded after the scan.

For each P4/P5 physical detector direction, `run_C19.py`:

1. rotates the observation position from GSM to SM using the driver dipole tilt;
2. constructs the physical detector boresight (or reads the supplied attitude vector);
3. converts the AMPS incoming arrival/velocity direction to the opposite EPEAD telescope look direction;
4. selects regular-grid cells inside the detector's elliptical aperture;
5. reads the three-state access value at each explicitly requested detector-response rigidity;
6. folds `A(E,Ω)` with the event spectrum and detector response, propagating unresolved and finite-rigidity-grid uncertainty;
7. calculates East and West transmission and their E/W ratio only when the response bounds satisfy the configured validity criteria; and
8. independently reduces each access curve to an effective directional cutoff and
   calculates a clearly labelled hard-cutoff E/W diagnostic using the same spectrum,
   response, aperture, attitude, and anisotropy assumptions.

`DIRECT_ACCESS` is independent of the scalar `CUTOFF_SAMPLING` cutoff result even though
the common input syntax retains `CUTOFF_SAMPLING VERTICAL`. In production mode no scalar
cutoff trajectory and no directional cutoff-map/PENUMBRA_SCAN task is scheduled. The
runner constructs the directional geometry needed for aperture folding directly from
the `A(E,Ω)` cube. It also derives a diagnostic finite-support equivalent cutoff from
the already calculated access samples; this does not launch a PENUMBRA_SCAN or alter
the direct observable. By default only regular-grid cells needed by the requested EPEAD
apertures are scheduled; `FULL_SPHERE` remains available as an explicit reference coverage.
The retained cells have exactly the same SM lon/lat coordinates as the corresponding cells
in a full-sphere run. `PENUMBRA_SCAN` remains an explicit diagnostic mode and writes the
full lower/effective/upper cutoff topology in addition to the same direct-access cube.

### 9.1 Three-state cutoff classification

There are two cutoff-classification paths in the current solver. The historical
`UPPER_SCAN` path calls the Boolean `TraceAllowed3D`/gridless equivalent. That Boolean
interface intentionally maps every recognized cutoff-forbidden termination, including
configured time/step/distance caps, to `false`. It does not use the three-state trace
policy.

Both `DIRECT_ACCESS` and `PENUMBRA_SCAN` use the structured three-state classifier for
the samples that feed C19. A trajectory that escapes through the outer boundary is
`ALLOWED`; a physical inner-boundary loss or validated magnetic-trap termination is
`PHYSICAL_FORBIDDEN`; and a configured `TIME_LIMIT`, `STEP_LIMIT`, or `DISTANCE_LIMIT` is
`UNRESOLVED` when the input requests `CUTOFF_TRACE_LIMIT_POLICY UNRESOLVED`.

C19 therefore does **not** attempt to make `UPPER_SCAN` pass by increasing a timeout or by
relabeling its result after the calculation. The production runner defaults to
`DIRECT_ACCESS`; `--cutoff-search PENUMBRA_SCAN` requests the more expensive full cutoff-band
diagnostic while retaining the same direct `A(E,Ω)` companion samples. The historical
Boolean `UPPER_SCAN` path is not selectable from the C19 runner CLI.

### 9.2 Unresolved aperture cells are never silently removed

Earlier C19 folding skipped an unresolved directional cell and divided by the solid angle
of the remaining cells. A mostly unresolved aperture could therefore look like a precise
finite detector response.

The new fold keeps all in-aperture solid-angle weights in the denominator and writes
bounds:

```text
T_min : every unresolved cell is assigned transmission 0
T_max : every unresolved cell is assigned transmission 1
```

The model row contains:

```text
east_transmission_min / east_transmission_max
west_transmission_min / west_transmission_max
east_signal_min / east_signal_max
west_signal_min / west_signal_max
modeled_east_west_ratio_min / modeled_east_west_ratio_max
unresolved_east_fraction
unresolved_west_fraction
east_aperture_status / west_aperture_status
status_reasons
```

A central transmission is reported only when both the unresolved fraction and the
finite-rigidity transition fraction satisfy their configured tolerances. Regardless of
whether that scalar is accepted, lower/upper transmission and synthetic-signal bounds
remain available. Above the unresolved threshold the row is explicitly invalidated as:

```text
EXCESSIVE_UNRESOLVED_EAST_APERTURE
EXCESSIVE_UNRESOLVED_WEST_APERTURE
```

This tolerance is explicit and auditable; unresolved sky cells can no longer disappear
from the detector normalization.

Availability is evaluated independently for EAST and WEST before an overall row status
is derived. `C19_model.csv` and `C19_aperture_availability.csv` retain the complete
per-head pipeline: selected cells, forward-facing cells, geometric aperture cells,
cells with access samples, cells overlapping the channel response, contributing cells,
geometric/contributing solid angle, coverage fraction, bounds, scalar, and status.
`status_reasons` retains every simultaneous failure rather than losing the WEST reason
when an EAST check fails first.

The geometry gates are configurable:

```text
--min-aperture-cell-count 1
--min-aperture-solid-angle-coverage 0.95
```

A resolved physical zero is `PHYSICAL_ZERO` at the per-head level and is never treated
as absent data.

### 9.3 Termination-reason forensics

A cutoff value alone cannot distinguish a physical shielding transition from a finite
trajectory-budget artifact. `PENUMBRA_SCAN` therefore accumulates raw termination counts
for every unique rigidity evaluation used by the coarse scan, transition refinement, and
effective-cutoff integration.

The long-form map records, separately:

```text
OUTER_BOUNDARY_ALLOWED
INNER_BOUNDARY_FORBIDDEN
MAGNETICALLY_TRAPPED_FORBIDDEN
TIME_LIMIT
STEP_LIMIT
DISTANCE_LIMIT
```

The `n_unresolved_samples` topology field refers to unresolved samples on the regular
cutoff-band grid; the termination counters cover all unique trajectory evaluations,
including refinement/integration evaluations. Use the latter when diagnosing why a
particular direction exhausted the trace budget.

### 9.4 Trace-budget and response-weighted frozen-field guardrail

Trace-budget studies, when needed, are performed by rerunning the same current workflow
with explicitly changed `--max-trace-distance-re`, `--max-trace-time`, and `--max-steps`.
The purpose is **not** to keep increasing the budget until every trajectory escapes.
Each C19 epoch is a frozen magnetic-field snapshot, so sufficiently long trajectories
must also be assessed for sensitivity to the static-field approximation.

#### Why the original guardrail was replaced

The earlier implementation used one condition:

```text
max(trace_time over every contributing trajectory) > --frozen-field-warning-seconds
```

and then rejected the complete EAST/WEST scalar as `STATIC_FIELD_TRACE_GUARDRAIL`.
That rule was intentionally conservative, but it had two important defects for C19:

1. **It ignored detector weight.** One long trajectory with negligible `J(E)G(E)` and
   aperture weight could veto an otherwise well-resolved detector signal.
2. **It coupled a physics warning to the numerical endpoint.** With
   `MAX_TRACE_TIME=300 s` and `DT_TRACE=0.25 s`, a final accepted step can report a
   trace slightly above 300 s even though the requested physical budget was 300 s.
   Using 300 s for both the numerical cap and the hard validity threshold could turn
   that one-step overshoot into an automatic rejection.

The current guardrail therefore measures **how much of the synthetic detector response
depends on long trajectories**, and it keeps warning provenance separate from hard
scientific rejection.

#### Response-weighted long-trace definition

For every adjacent DIRECT_ACCESS energy interval, the exact piecewise
`J(E)G(E)` integral is split equally between the two trajectory endpoints, using the
same endpoint-sharing convention as the termination budget.  The contribution is then
multiplied by the sky-cell solid-angle weight and by the configured source anisotropy
factor.  The resulting long-trace fractions are normalized by the complete unshielded
synthetic signal for that detector head.

A trajectory is called long when

```text
trace_time > frozen_field_warning_seconds + frozen_field_time_tolerance_seconds
```

The default tolerance is `0.5*DT_TRACE`; this prevents a single final integration-step
overshoot from being mistaken for physically significant long-duration propagation.
The runner also records the fraction above `2*warning_seconds`.

Per head, C19 reports:

```text
long_trace_response_fraction
very_long_trace_response_fraction
long_trace_allowed_fraction
long_trace_forbidden_fraction
long_trace_unresolved_fraction
trace_time_p50_s
trace_time_p90_s
trace_time_p99_s
```

The trace-time percentiles are detector/spectrum-weighted percentiles, not trajectory-
count percentiles.  For example, `trace_time_p99_s=80` means 99% of the synthetic
detector response is carried by trajectory endpoints with trace duration no larger than
80 s.

#### Warning versus hard rejection

The default controls are:

```text
--frozen-field-warning-seconds 300
--frozen-field-time-tolerance-seconds -1   # AUTO = 0.5*--dt-trace
--max-long-trace-response-fraction 0.05
--max-long-unresolved-response-fraction 0.05
--max-static-field-ratio-bound-width-log10 -1
```

Their roles are intentionally different:

* **Any** nonzero detector-weighted long-trace fraction sets
  `static_field_warning_triggered=true`.  This is provenance only and does not erase an
  otherwise accepted direct scalar.
* If the total long-trace response exceeds
  `--max-long-trace-response-fraction`, C19 sets
  `static_field_response_dominance_warning=true`.  This is still a warning, because a
  long trajectory that is positively resolved as allowed or physically forbidden is not
  equivalent to a numerical timeout.
* A head is hard-guarded when the response fraction that is **both long and unresolved**
  exceeds `--max-long-unresolved-response-fraction`.  This directly targets detector
  signal whose physical access remains unknown after a long frozen-field integration.

The previous `STATIC_FIELD_TRACE_GUARDRAIL` status is therefore replaced by
`STATIC_FIELD_DOMINATED` for a true hard guard.  A quantitatively accepted row can remain
`VALID` or `MODEL_MISMATCH` while carrying
`direct_acceptance_reason=ACCEPTED_WITH_STATIC_FIELD_WARNING` or
`ACCEPTED_WITH_STATIC_FIELD_DOMINANCE_WARNING`.

#### Observable-sensitivity bounds

C19 also computes a conservative frozen-field sensitivity interval.  For every energy
bracket touching a long-duration trajectory, the bracket is temporarily treated as
unknown `[blocked, transmitted]` while all short-duration brackets retain their ordinary
DIRECT_ACCESS bounds.  EAST and WEST are then combined into:

```text
static_field_east_west_ratio_min/max
static_field_log10_east_west_ratio_min/max
static_field_log10_east_west_bound_width
```

This asks the right observable-level question: **how much could the compared E/W ratio
move if the long-duration part of a frozen T05 snapshot were not trustworthy?**  The
optional `--max-static-field-ratio-bound-width-log10` can turn this diagnostic into a
hard acceptance gate.  Its default is negative (disabled) because no event-independent
publication tolerance has yet been calibrated; the bounds are nevertheless always
reported.  A convergence/publication study may enable a justified threshold explicitly.

#### Outputs

The new guardrail quantities are serialized in `C19_comparison.csv`, `C19_model.csv`,
`C19_aperture_availability.csv`, `C19_aperture_termination_budget.csv`, and
`C19_result.json`.  Per-sky-cell long-trace fractions and static-field transmission
bounds are also retained in `C19_aperture_samples.csv`.  The diagnostic figure

```text
C19_static_field_guardrail_<solver>_<field>.png
```

plots EAST/WEST long-trace response fractions, long+unresolved fractions, and the
conservative static-field `log10(E/W)` width versus epoch.

The guardrail does **not** classify a timeout as forbidden and does not replace the
ordinary trajectory-resolution or direct-bound-width gates.  If a substantial fraction
of the detector signal genuinely requires trajectories far longer than the frozen-field
validity interval, the physically stronger next step is a time-dependent background
field along the trajectory rather than an arbitrarily larger static-field timeout.

### 9.5 Independent centered-dipole Størmer regression

GRIDDED-versus-GRIDLESS agreement is useful but is only an internal consistency test; a shared sign or trajectory-classification error could affect both. For independent solver regression, `FIELD_MODEL DIPOLE` directional output can include the analytic Størmer cutoff `Rc_stormer_GV` at the same position and incoming direction as the numerical sky cell. Such a standalone regression checks trajectory/cutoff machinery independently of T05 and GOES and must not be tuned using the May-2012 observations.

### 9.6 AMPS arrival direction versus detector look direction

The production mapping remains explicit and fixed. In the AMPS cutoff source, the
directional-map vector is the physical **ARRIVAL direction**. The backward trajectory is
initialized with the opposite vector. The EPEAD EAST/WEST aperture is described by the
direction the telescope faces, so C19 uses:

```text
detector look direction = - AMPS directional-map arrival direction
```

The runner self-test uses a synthetic asymmetric map to verify the sign. The legacy
direct-vector comparison is still written as `LEGACY_DIRECT_DIAGNOSTIC`, but never enters
acceptance.

### 9.7 Zero transmission is a resolved saturation state, not missing coverage

Exact one-sided zero transmission remains a valid model state when the aperture itself is
resolved:

```text
ZERO_EAST_TRANSMISSION  -> log10(E/W) -> -infinity
ZERO_WEST_TRANSMISSION  -> log10(E/W) -> +infinity
ZERO_BOTH_TRANSMISSION
```

C19 does not manufacture an epsilon. One-sided zeros are excluded from finite MAE/RMSE
but retained in saturation and sign diagnostics.

Additional non-saturation statuses now include:

```text
NO_EAST_APERTURE_CELLS
NO_WEST_APERTURE_CELLS
INSUFFICIENT_EAST_ENERGY_SAMPLES
INSUFFICIENT_WEST_ENERGY_SAMPLES
NO_EAST_RESPONSE_OVERLAP
NO_WEST_RESPONSE_OVERLAP
INCOMPLETE_EAST_SOLID_ANGLE_COVERAGE
INCOMPLETE_WEST_SOLID_ANGLE_COVERAGE
EXCESSIVE_UNRESOLVED_EAST_APERTURE
EXCESSIVE_UNRESOLVED_WEST_APERTURE
UNRESOLVED_EAST_APERTURE
UNRESOLVED_WEST_APERTURE
STATIC_FIELD_DOMINATED
NEGATIVE_TRANSMISSION
NONFINITE_MODELED_RATIO
```

### 9.8 Staged validity gates

C19 no longer calls a run numerically complete merely because all processes returned
successfully. Four stages are reported separately:

```text
execution_complete
trajectory_resolution_passed
instrument_fold_valid
observational_passed
```

`execution_complete` means the requested AMPS runs and postprocessing completed.
`trajectory_resolution_passed` additionally requires each EAST/WEST aperture to satisfy
the unresolved-solid-angle threshold and to avoid a **hard** response-weighted frozen-
field guard.  Warning-only long trajectories do not invalidate trajectory resolution.
`instrument_fold_valid` requires usable aperture geometry and transmission bounds.
`observational_passed` is the provisional GOES agreement gate.

The scientific overall state is:

```text
passed = execution_complete
         AND trajectory_resolution_passed
         AND instrument_fold_valid
         AND observational_passed
```

For compatibility, `C19_result.json` still contains `numerical_complete`, but it is marked
deprecated and now means `execution_complete AND trajectory_resolution_passed`.

### 9.9 Sensitivity and convergence controls

The production trajectory classification defaults to `DIRECT_ACCESS + UNRESOLVED`.
`--cutoff-search PENUMBRA_SCAN` explicitly enables the more expensive full cutoff-band
diagnostic while retaining the same direct `A(E,Ω)` companion cube. Historical Boolean
cutoff modes are not exposed by the C19 runner. Numerical
inputs that remain intentionally configurable for controlled studies are:

```text
--max-unresolved-aperture-fraction
--frozen-field-warning-seconds
--frozen-field-time-tolerance-seconds
--max-long-trace-response-fraction
--max-long-unresolved-response-fraction
--max-static-field-ratio-bound-width-log10
--spectral-index
--dir-lon-res-deg
--dir-lat-res-deg
--cutoff-scan-n
--cutoff-emin-mev
--cutoff-emax-mev
--dt-trace
--max-steps
--max-trace-time
--max-trace-distance-re
--mode3d-mesh-res-earth-re
--mode3d-mesh-res-boundary-re
```

The current workflow integrates three-state trajectory classification, source-variable provenance, real-ephemeris support, event-derived spectra, direct directional rigidity access, detector-response folding, finite-rigidity-grid uncertainty bounds, documented high-energy P4/P5 response support, fine angular/mesh settings, exact-file detector geometry support, and explicit upstream-anisotropy controls. Replacement of the factorized response by a fully calibrated energy-angle response matrix remains a separate instrument-calibration task.

## 9.10 Observational-commensurability implementation

### Exact GOES source-variable provenance

`build_goes_reference.py` uses an explicit flux-product policy and records the exact NOAA column used for each physical EAST/WEST measurement. No correction state can change silently. Legacy committed reference rows that predate exact provenance are read as `LEGACY_UNRECORDED`; regenerate the reference to obtain full provenance.

### Real GOES ephemeris

The reference builder supports the native NOAA/NCEI one-minute NetCDF orbit product and CSV equivalents. Real ephemeris is required by default when rebuilding the reference. The old manifest slot is available only through `--allow-nominal-ephemeris`. For science runs use `run_C19.py --require-real-ephemeris` so a legacy nominal-slot reference cannot accidentally enter publication output.

### Event-derived spectrum

The default `--spectrum-source OBSERVED_WEST` fits

```text
J(E,t) = J0(t) [E/E0]^-gamma(t)
```

from the physical-WEST background-subtracted P4/P5 intensities at each epoch, using channel effective energies stored in the event manifest. If quality filtering removes one channel at an isolated epoch, gamma is interpolated from neighboring two-channel fits and the available WEST channel sets the normalization; this is marked explicitly in `C19_spectrum_used.csv`. An independent upstream spectrum is preferable when available:

```bash
--spectrum-source FILE --spectrum-file spectrum.csv
```

with `utc,gamma[,j0,e0_mev]`. `--spectrum-source FIXED --spectral-index ...` is retained only for sensitivity/legacy reproduction.

### Direct GRIDDED/GRIDLESS `A(E,Ω)` product

C19 supports two current cutoff products selected with `--cutoff-search`:

```text
DIRECT_ACCESS   (default)
    Trace only the requested (direction,rigidity) samples needed by the detector fold.
    No scalar cutoff and no directional PENUMBRA_SCAN map are computed.

PENUMBRA_SCAN
    Compute the full lower/effective/upper cutoff topology for every selected direction
    and, in addition, trace the same requested direct-access rigidity list used by the
    detector fold.
```

With `DIRECT_ACCESS`, `DIRECTIONAL_MAP T`, and a non-empty `CUTOFF_RIGIDITY_LIST_GV`,
both solvers write the direct cube without first calculating a directional cutoff map.
With `PENUMBRA_SCAN`, the same cube is written as a companion product. The solver-specific
file names are:

```text
GRIDDED independent: cutoff_3d_dir_access_loc_000000.dat
GRIDDED batch      : cutoff_3d_dir_access_loc_<snapshot-local-id>_snapshot_<index>_<UTC>.dat
GRIDLESS: cutoff_gridless_dir_access_point_0000.dat
```

Each row is one `(lon,lat,rigidity)` trajectory and contains `rigidity_GV`, corresponding proton `energy_MeV`, `access_state`, `allowed`, and `unresolved`. The current diagnostic schema also records `termination_code`, `trace_time_s`, `trace_distance_Re`, `trace_steps`, retry count, `primary_termination_code`, `primary_trace_time_s`, `trace_extension_count`, `initial_trace_limit_s`, `final_trace_limit_s`, mirror/bounce counts, drift-revolution diagnostics, `drift_mean_radius_change_Re`, trapping mechanism, and momentum-magnitude spread. These columns are deliberately emitted by both GRIDDED and GRIDLESS so an `UNRESOLVED` state can be attributed to a concrete numerical termination and the effect of staged unresolved-only re-tracing can be audited without a second baseline run. States use the same three-state classifier as the production penumbra path:

```text
0 = PHYSICAL_FORBIDDEN
1 = ALLOWED
2 = UNRESOLVED
```

The GRIDDED and GRIDLESS direct-access files are required to carry the same selected sky-cell set and the same response-support endpoints. In dense mode every direction has the same ordered energy/rigidity grid. In adaptive `DIRECT_ACCESS` every direction contains the same mandatory seed rigidities but may contain a different set of refinement nodes; this is intentional and is validated explicitly by the runner. Rigidity is the authoritative grid identifier because it is the value passed to the trajectory solver; the output energy is reconstructed by AMPS from rigidity and the configured particle mass and is retained for detector-response integration. This avoids rejecting a valid cube because Python and C++ physical constants do not round-trip an energy value identically. In `PENUMBRA_SCAN` mode the runner additionally checks the cube against the companion directional map.

In `DIRECT_ACCESS` mode no solver-produced cutoff map exists by design. The runner
constructs a post-processing diagnostic map from the cube's frame, observation position,
and `(lon,lat)` cells. For each direction it evaluates the blocked-area equivalent over
the finite detector-response rigidity support,

```text
Rc_eff = R_min + integral[R_min,R_max] (1 - A(R)) dR .
```

Resolved constant intervals contribute their full forbidden width or zero allowed
width. Any bracket whose physical blocked fraction is not known exactly contributes the
rigorous `[0,dR]` interval to `Rc_lower/Rc_upper`.  Two different central quantities are
now deliberately kept separate:

* `Rc_effective` is a **resolved scalar** and is withheld whenever the source direction
  contains an `UNRESOLVED` trajectory sample;
* `Rc_midpoint_diagnostic` is a **plotting/diagnostic-only** equivalent cutoff.  It uses
  the midpoint contribution `0.5*dR` for a resolved transition bracket and also for an
  unresolved bracket, while the full `[0,dR]` uncertainty remains in the rigorous bounds.

This restores the historical cutoff-rigidity comparison curve without undoing the
Phase-4 direct-access correction.  The midpoint cannot change trajectory state, cannot
make an aperture `VALID`, and is never used by DIRECT_ACCESS acceptance metrics.  An
allowed lowest sample is marked lower-censored and a forbidden highest sample is marked
upper-censored.  The reduction is labelled
`DIRECT_ACCESS_EQUIVALENT_BLOCKED_AREA_MIDPOINT_DIAGNOSTIC_WITH_BOUNDS`; it is valid only
over the configured response support and must not be interpreted as an independently
resolved or unconstrained full-rigidity cutoff scan. Missing endpoints, missing adaptive
seed nodes, duplicate/non-increasing nodes, or truncated direct-access output still fail
post-processing.

The cutoff-based E/W diagnostic replaces each directional access curve by a hard step
at the explicitly labelled midpoint diagnostic, then folds that step through the same
piecewise detector response and the same epoch spectrum as the direct calculation.
`Rc_upper` produces its minimum signal and `Rc_lower` its maximum signal, so the central
curve is always accompanied by rigorous proxy bounds. In `PENUMBRA_SCAN` mode the
reduction uses the AMPS `Rc_lower/Rc_effective/Rc_upper` map directly. This diagnostic is
useful when a broad direct interval has finite nonzero bounds but no accepted scalar; it
never contributes to C19 validity or observational acceptance.

Dense direct access flattens `(location, sky-cell, rigidity)` work so independent trajectories are distributed across MPI ranks and intra-rank workers. Adaptive direct access deliberately changes the top-level scheduling unit to **one sky direction**: that worker first evaluates the mandatory seeds and then makes local midpoint-refinement decisions for that direction. Different directions remain independent and are distributed across MPI ranks/threads. This dependency-aware task unit avoids global synchronization after every refinement level while retaining thousands of parallel direction tasks in a normal C19 aperture. `DIRECT_ACCESS` is the default science product because the detector fold consumes the access samples themselves; `PENUMBRA_SCAN` is retained when full cutoff topology is explicitly required. The two solver files intentionally retain the same public columns and three-state semantics so `run_C19.py` uses one parser and one folding implementation.

A terminology detail is important: `RIGIDITY_LIST` remains the historical shell-oriented Mode3D product. C19 uses the distinct `DIRECT_ACCESS` token for point/trajectory directional access. The common CLI layer preserves these as separate tokens; in particular, `-cutoff-search DIRECT_ACCESS` must **not** be normalized to `RIGIDITY_LIST`. GRIDDED and GRIDLESS implement the same `DIRECT_ACCESS` semantics and the same adaptive-refinement helper.

### Adaptive `DIRECT_ACCESS` algorithm and runtime optimization

The production `DIRECT_ACCESS` calculation now uses **per-direction adaptive rigidity sampling** by default. The optimization changes only *where* the access function is sampled; it does not replace the three-state trajectory classifier, assume a monotonic cutoff, interpolate access as a fractional ramp, or relax any C19 observational validity gate. A dense common-grid implementation remains available as the reference/convergence calculation.

The runner first constructs a small logarithmic seed grid across the complete positive energy support of the selected detector-response file and converts those energies to proton rigidities. The current defaults are:

```text
--adaptive-access-seed-points 12
--adaptive-access-guard-depth 1
--adaptive-access-max-depth 6
```

The corresponding input directives are identical for GRIDDED and GRIDLESS:

```text
CUTOFF_DIRECT_ACCESS_ADAPTIVE              T
CUTOFF_DIRECT_ACCESS_ADAPTIVE_MAX_DEPTH    6
CUTOFF_DIRECT_ACCESS_ADAPTIVE_GUARD_DEPTH  1
```

For each selected sky direction the shared C++ implementation (`util/AdaptiveDirectAccess.h`) performs the following steps:

1. **Evaluate every seed rigidity.** This guarantees common lower/upper response support and coarse global coverage in every direction. Seed states use exactly the normal `PHYSICAL_FORBIDDEN / ALLOWED / UNRESOLVED` trajectory classifier.
2. **Perform mandatory guard probes.** At guard depth 1, the geometric-rigidity midpoint of every adjacent seed interval is evaluated even when both endpoints have the same state. This is a protection against missing a finite access/forbidden pocket whose two coarse endpoints happen to agree. Higher guard depths recursively force additional midpoint levels.
3. **Refine visible state boundaries.** After the mandatory guard probes, an interval is recursively bisected when its two sampled endpoint states differ. This includes `ALLOWED<->PHYSICAL_FORBIDDEN` transitions and resolved `<-> UNRESOLVED` boundaries. The midpoint in rigidity is geometric, `Rmid=sqrt(R1 R2)`, which gives approximately uniform fractional resolution over the positive rigidity range.
4. **Do not explode a purely unresolved interval.** If both endpoints remain `UNRESOLVED`, refinement stops after the configured guard probes. Repeated rigidity subdivision cannot by itself cure a trajectory time/step/path limit; the Python fold already carries that interval as unresolved uncertainty.
5. **Stop at the maximum depth.** The default depth 6 bounds both trajectory work and memory. No monotonic-cutoff assumption is used: if guard/refinement probes reveal multiple state changes, each visible transition is retained and refined independently.
6. **Write only evaluated nodes.** Internally, every MPI rank builds the same deterministic candidate tree. Unvisited candidate slots retain sentinel `-1`, allowing the existing fixed-size `MPI_MAX` reduction. The Tecplot writer omits those `-1` slots, so each direction can contain a different number of actual rigidity samples while the public output schema remains unchanged. Dense mode continues to treat any `-1` state as a fatal producer error.
7. **Fold the sparse samples with explicit uncertainty bounds.** `run_C19.py` sorts the realized samples independently for each direction. Equal `ALLOWED` endpoints transmit the complete interval, equal `PHYSICAL_FORBIDDEN` endpoints block it, and a sampled state change contributes a `[0, full interval]` finite-grid uncertainty bracket. Any interval touching `UNRESOLVED` is carried separately as trace-resolution uncertainty. Thus adaptation does not invent the unknown transition location.

The deterministic candidate tree has

```text
Ncandidate = Nseed + (Nseed - 1) * (2^D - 1)
```

possible nodes for maximum depth `D`. With 12 seeds and depth 6 this is 705 candidate slots per direction, but **only evaluated nodes generate trajectories or output rows**. The fixed candidate array is used only to make MPI reduction deterministic; for roughly 1,760 directions it is only a few megabytes of integer state storage.

With the default guard depth 1, a smooth direction has approximately `12 + 11 = 23` mandatory classifications before any transition-driven refinement. For the representative 1,760-direction case, the mandatory baseline is therefore about 40,480 trajectories, compared with roughly 96,800 trajectories for the former 55-point dense access grid. Directions containing penumbra structure require additional local samples, so the realized speedup is event-dependent and should be measured rather than assumed. `C19_commands.json` records the realized row count and the minimum/mean/maximum number of samples per direction after each completed case.

Because one adaptive top-level task now contains many trajectory integrations, the C19 runner also changes the default dynamic-MPI chunk for adaptive `DIRECT_ACCESS` to approximately **one direction per local worker**. For example, `-nt 16` gives a default adaptive chunk of 16, not the generic GRIDLESS AUTO value of roughly 64 cheap tasks. This keeps all 16 local workers occupied but returns to the global RMA work queue after one wave, reducing the chance that a rank hoards many expensive near-cutoff directions and becomes the end-of-run straggler. An explicit positive `--dynamic-chunk` still overrides this heuristic. Dense GRIDLESS calculations retain the generic AUTO behavior.

#### Why detector-response edges are not forced trajectory nodes

The detector response is piecewise constant in the current C19 response files, and the runner integrates `J(E)G(E)` analytically while splitting an integration interval at every response discontinuity. Therefore an internal response edge does **not** need a separate AMPS trajectory merely to integrate the detector response correctly. The adaptive seed grid spans the entire positive response support; access refinement is driven by magnetic-access state changes. The support endpoints themselves remain mandatory in every direction.

#### Validation and convergence controls

Adaptive sampling is an optimization of a validation test, so it is paired with explicit checks rather than treated as an uncontrolled shortcut:

- `--max-discrete-transition-fraction` limits the detector-response-weighted fraction left inside sampled resolved state-change brackets. If the adaptive tree remains too coarse, the quantitative fold is invalidated rather than silently accepted.
- `--max-unresolved-aperture-fraction` independently limits uncertainty caused by trace time/step/distance limits.
- `--min-aperture-cell-count` and `--min-aperture-solid-angle-coverage` independently reject missing or incomplete EAST/WEST geometry before a ratio is accepted.
- The runner verifies that every adaptive direction contains every requested seed energy and the common response-support endpoints; missing/truncated sparse output is fatal.
- `--no-adaptive-access --access-energy-points 48` restores the dense reference grid for direct adaptive-versus-dense convergence tests.
- `--adaptive-access-max-depth` controls the finest local transition bracket, while `--adaptive-access-guard-depth` controls how aggressively apparently uniform intervals are probed for hidden non-monotonic structure.
- `PENUMBRA_SCAN` intentionally disables adaptive direct access and remains the expensive full cutoff-topology diagnostic/reference path.

A finite guard depth cannot mathematically prove the absence of an arbitrarily narrow even-numbered pair of transitions between all sampled points. For publication-quality validation, representative difficult epochs should therefore be compared against the dense `--no-adaptive-access` calculation (and, when needed, `PENUMBRA_SCAN`). The adaptive path is accepted only when the final detector observable and uncertainty diagnostics are converged to the required tolerance.

#### Execution commands

Adaptive `DIRECT_ACCESS` is the default, so the normal one-line GRIDDED command is:

```bash
python3 srcEarth/test/C19/run_C19.py --profile SMOKE --solver GRIDDED --models T05 --cutoff-search DIRECT_ACCESS --amps ./amps -np 4 -nt 16 --keep
```

The explicit dense direct-access reference is:

```bash
python3 srcEarth/test/C19/run_C19.py --profile SMOKE --solver GRIDDED --models T05 --cutoff-search DIRECT_ACCESS --no-adaptive-access --access-energy-points 48 --amps ./amps -np 4 -nt 16 --keep
```

The full cutoff/penumbra diagnostic is:

```bash
python3 srcEarth/test/C19/run_C19.py --profile SMOKE --solver GRIDDED --models T05 --cutoff-search PENUMBRA_SCAN --amps ./amps -np 4 -nt 16 --keep
```

The same adaptive/dense controls work with `--solver GRIDLESS` or `--solver BOTH` for `DIRECT_ACCESS`.

### Response fold of direct access

For either solver, `run_C19.py` builds a rigidity sampling request over the **complete positive support of the configured response file**. In adaptive `DIRECT_ACCESS` that request is the common seed list and the C++ solver adds direction-specific midpoint/refinement samples; in dense mode it is the complete common grid. The runner then reads the realized direct access cube and evaluates the head transmission schematically as

```text
T_head(t) = ∫dΩ w(Ω) ∫dE J(E,t) G_channel(E) A(E,Ω,t)
            ------------------------------------------------
             ∫dΩ w(Ω) ∫dE J(E,t) G_channel(E)
```

where `w(Ω)` is the nominal elliptical angular-aperture weight and `G_channel(E)` is read from `--detector-response`.

The energy quadrature no longer linearly interpolates the binary access state. For every adjacent pair of checked rigidities:

```text
ALLOWED   -> ALLOWED      whole interval transmitted
FORBIDDEN -> FORBIDDEN    whole interval blocked
ALLOWED   -> FORBIDDEN    transition lies somewhere in the interval
FORBIDDEN -> ALLOWED      transition lies somewhere in the interval
anything  -> UNRESOLVED   physical state of the interval is unresolved
```

For a resolved state change the whole response-weighted interval is carried as a finite-grid uncertainty `[0, full interval]`; it is **not** replaced by the artificial 0.5 contribution produced by trapezoidal interpolation of the 0/1 endpoints. `C19_model.csv` records `discrete_transition_east_fraction` and `discrete_transition_west_fraction`. The default `--max-discrete-transition-fraction 0.05` invalidates a quantitative E/W value when too much detector response lies inside such transition brackets. In adaptive mode, increasing `--adaptive-access-max-depth` and/or `--adaptive-access-guard-depth` is the direct local-grid convergence control; the dense `--no-adaptive-access --access-energy-points ...` path remains the independent reference.

`UNRESOLVED` trajectory states remain a separate uncertainty source and continue to use `--max-unresolved-aperture-fraction`. The lower/upper signal and transmission bounds include both effects, while their diagnostic fractions remain separate so a coarse rigidity grid cannot be confused with a trace-limit problem.

`J(E)G(E)` is integrated analytically over every energy interval for the current power-law spectrum and piecewise-constant response, with integration intervals split internally at exact detector-response edges. Dense reference grids still include response edges explicitly; adaptive sampling does not require those internal edges to be separate trajectory nodes. The old `E*(1±10^-8)` edge-bracketing trajectories remain removed.

The default `epead_response_C19_uncorrected_extended.csv` matches the default **uncorrected** GOES reference more closely than the old primary-only response. It contains the nominal primary bands plus the documented GOES 8–15 secondary proton energy ranges from Rodriguez et al. (2017), Table A2 / the GOES-N EPS calibration handbook:

| Channel | Component | Energy range (MeV) | Published geometrical factor (cm² sr) | C19 relative weight |
|---|---|---:|---:|---:|
| P4 | primary | 15–40 | 0.21 | 1.0 |
| P4 | secondary | 80–115 | 0.038 | 0.180952 |
| P4 | secondary | 115–150 | 0.25 | 1.190476 |
| P5 | primary | 38–82 | 0.36 | 1.0 |
| P5 | secondary | 80–110 | 0.091 | 0.252778 |
| P5 | secondary | 110–150 | 0.57 | 1.583333 |
| P5 | secondary | 150–190 | 0.21 | 0.583333 |

The relative weights are each secondary geometrical factor divided by that channel's primary factor; absolute normalization cancels in the same-channel E/W ratio. With this file the direct-access grid extends automatically to **190 MeV** instead of ending at 82 MeV. The primary-only `epead_response_C19_nominal.csv` remains available for corrected-flux or controlled sensitivity studies.

Important limitation: the published secondary factors are integrated side/rear penetrating-particle responses, not a resolved angular response matrix. C19 currently treats them as an **energy-response extension inside the same factorized angular model**. That is better than assigning zero response above 82 MeV for an uncorrected product, but it is not a substitute for a measured energy-angle response. If a corrected NOAA flux product is selected, the runner warns when a response containing `SECONDARY` components is used because that would tend to double-count the correction.

## 9.11 Current production numerical and physics settings

The former P2 work is now part of the normal C19 implementation rather than a separate
one-epoch diagnostic suite. The runner uses a single production path.

### Angular resolution

The default directional map remains `2.5° × 2.5°`, the finest level from the previous
10°/5°/2.5° convergence ladder.  The runner supports the point-count interface
`--access-angular-points N`, restored after it was accidentally dropped during the later
unresolved-trajectory update.  It is intentionally analogous to the independent
`--access-energy-points` control.

`--access-angular-points N` defines **N longitude cells over 360°** and uses the same
spacing in latitude, so `dtheta = 360/N`.  `N` must be even so the latitude grid reaches
both poles exactly.  Useful convergence values are `72` (5°), `144` (2.5°, production
default-equivalent), and `288` (1.25°).  For example:

```bash
python3 srcEarth/test/C19/run_C19.py ... --access-angular-points 144 --access-energy-points 48
```

The two controls are independent: `--access-angular-points` changes the directional sky
grid, while `--access-energy-points` changes the dense DIRECT_ACCESS rigidity/energy
reference grid (and remains available for `--no-adaptive-access` convergence studies).
The legacy `--dir-lon-res-deg` and `--dir-lat-res-deg` controls are retained for advanced
asymmetric angular grids, but they cannot be combined with `--access-angular-points` in
the same invocation.  If no angular option is supplied, the historical 2.5° × 2.5°
production grid is used.

For reproducibility, the resolved angular source, lon/lat degree spacing, and nominal
full-sphere grid dimensions are recorded in `C19_commands.json` and `C19_result.json`.

### Directional coverage: arbitrary instrument apertures versus full sphere

The regular 2.5° sky grid contains 144 longitude values and 73 latitude values, or
10,512 nominal cells. Earlier C19 versions traced every one of those directions even
though the synthetic observable subsequently uses only directions viewed by the
instrument heads at that epoch. The optimized implementation is now **instrument-
neutral**: AMPS is given one or more physical detector-look vectors and schedules only
the regular-grid cells inside those apertures. In a batched GRIDDED run, `LOCATION=`
ownership is preserved all the way through the Mode3D scheduler, so each spacecraft is
traced only over its own detector apertures.

The current C19 default is:

```text
DIRMAP_COVERAGE      VECTOR_APERTURES
DIRMAP_APERTURE_FILE C19_directional_apertures.dat
```

`run_C19.py` generates `C19_directional_apertures.dat` separately for every spacecraft
and epoch. For an authoritative attitude file, each head can point in an arbitrary
direction and the heads do **not** have to be antipodal. The May-2012 observational
reference still reports the conventional physical EAST/WEST flux ratio, but it also
records `telemetry_head_east` and `telemetry_head_west`. C19 uses those fields to map
each numerator/denominator stream back to the **actual instrument head** (for example
raw head `E` or `W`) and then looks up that head's time-dependent boresight. Thus the
words EAST/WEST do not define the geometry used by AMPS.

Each aperture record has the form:

```text
# name frame bx by bz upx upy upz horizontal_half_angle_deg vertical_half_angle_deg
```

The fields define **one finite instrument field of view**, not one particle trajectory.
Their meanings are:

| Field | Meaning |
|---|---|
| `name` | Arbitrary aperture/head identifier such as `E`, `W`, `HEAD_A`, or `TELESCOPE_1`. The name carries **no geometric meaning**; the vectors determine the pointing. |
| `frame` | Coordinate frame in which both vector triplets are supplied: `SM`, `GSM`, or `LOCAL_SM`. |
| `bx by bz` | Components of the detector **LOOK boresight** vector. This is the direction in the sky toward which the instrument points. |
| `upx upy upz` | Components of a roll/up reference vector. Together with the boresight, this fixes the rotation of a non-circular/elliptical FOV about the boresight. |
| `horizontal_half_angle_deg` | Angular semi-width of the elliptical FOV along its derived horizontal axis, in degrees. |
| `vertical_half_angle_deg` | Angular semi-width of the elliptical FOV along its derived vertical/up axis, in degrees. |

For example,

```text
HEAD_A SM  0.0 1.0 0.0   0.0 0.0 1.0   30.0 60.0
```

defines an aperture whose detector looks toward SM `+Y`, whose nominal up direction is
SM `+Z`, and whose FOV has horizontal and vertical half-angles of 30 degrees and 60
degrees, respectively. The vector magnitudes are not used as physical quantities: the
solver normalizes the boresight and orthonormalizes the supplied up reference. The
boresight must be nonzero, and the up reference must not be parallel (or numerically
nearly parallel) to the boresight.

The supplied up vector need not be exactly perpendicular to the boresight. After both
vectors have been transformed into the directional-map frame, the solver constructs the
aperture basis as

```text
b = normalize(boresight)
v = normalize(up - (up dot b) b)
h = normalize(v cross b)
```

where `b` is the detector look boresight, `v` is the vertical/up aperture axis, and `h`
is the horizontal aperture axis. Thus a small parallel component in an attitude-file up
vector is removed automatically rather than rotating the requested FOV.

A directional-grid cell is converted from the AMPS particle-arrival direction to the
detector-look direction,

```text
look = -arrival
```

and is first required to lie in the forward hemisphere (`look dot b > 0`). The solver
then forms the horizontal and vertical angular offsets

```text
ah = atan2(look dot h, look dot b)
av = atan2(look dot v, look dot b)
```

and retains the grid cell when

```text
(ah / horizontal_half_angle_deg)^2
  + (av / vertical_half_angle_deg)^2 <= 1
```

with `ah` and `av` expressed in degrees. Therefore the two half-angle fields are the
semi-axes of an **elliptical angular aperture**; they are not independent rectangular
`|ah|`/`|av|` cuts.

The supported frames are:

- `SM`: `boresight` and `up` are Cartesian components in the global Solar Magnetic
  frame used to label the directional map.
- `GSM`: the vectors are Cartesian components in GSM. AMPS transforms them into the SM
  directional-map frame at the run epoch before constructing the aperture basis.
- `LOCAL_SM`: the three components are coefficients in the local orthonormal
  `(radial, local-east, local-north)` basis at the observation point. This mode is mainly
  for proxy/engineering configurations and synthetic tests; authoritative instrument
  attitude should normally be supplied as global SM or GSM vectors.

The boresight is explicitly a detector **LOOK** vector. It is **not** the velocity used
to start the backward particle trace. For an incoming particle at the detector, the
AMPS directional-map arrival vector is antiparallel to the telescope look vector. Thus
an aperture row with `b = (0,1,0)` means that the detector looks toward `+Y` in the
selected frame, while the corresponding incoming-particle arrival/velocity direction is
`-Y`. C19 performs this sign conversion internally; aperture files must contain the
physical detector look vector and must not pre-reverse it.

For GOES, identifiers such as `E` and `W` should be interpreted as telemetry/instrument
head names only. They do **not** tell AMPS that a head points geographically east or
west. At every epoch, the actual pointing is determined only by the corresponding
boresight/up vectors. The two heads may be non-antipodal and may change orientation with
time.

The solver does **not** create a new angular lattice for optimized coverage. It first
defines exactly the same regular SM longitude/latitude grid as `FULL_SPHERE`, then keeps
only cells whose detector-look vector is inside at least one configured ellipse. AMPS
directional-map vectors are incoming particle **arrival** directions, so the selector
uses `look = -arrival`, the same convention used by the C19 postprocessor. Retained
cells therefore have exactly the same direction and trajectory as the corresponding
cells in a full-sphere calculation.

For current P4/P5 C19 runs, the pruning envelope uses the widest relevant P5 response
(`30°` horizontal and `60°` vertical half-angle) for each observed detector head. The
actual P4/P5 detector fold still uses the channel-specific response/field-of-view data;
the 30°×60° values only determine which AMPS directions can safely be omitted. With two
roughly opposite GEO heads this typically retains about 1,700--1,900 of the 10,512
2.5° sky cells, but the exact number now follows the actual attitude vectors and need
not be symmetric.

This also explains a remaining, intentional difference between two C19 figures after
the location-aware fix. `C19_directional_cutoff_*_<spacecraft>_<UTC>.png` visualizes the
**spacecraft pruning envelope**, which uses the widest required P4/P5 aperture so one
AMPS map can support both channels. `C19_aperture_diagnostic.png` visualizes one
representative **channel-specific detector fold**. Both should now show only the two
heads belonging to that spacecraft, but their exact lobe widths can differ because the
channel response/FOV is narrower than the conservative pruning envelope.

The runner exposes the choice directly:

```bash
# Fast/default science coverage: use the actual per-epoch instrument look vectors
python3 run_C19.py ... --direction-coverage INSTRUMENT_APERTURES

# Complete sky for diagnostic/full-map comparisons
python3 run_C19.py ... --direction-coverage FULL_SPHERE
```

The same generic selector is available directly in an AMPS input file. Apertures can be
provided through a file:

```text
#CUTOFF_RIGIDITY
DIRECTIONAL_MAP T
DIRMAP_LON_RES  2.5
DIRMAP_LAT_RES  2.5
DIRMAP_COVERAGE VECTOR_APERTURES
DIRMAP_APERTURE_FILE instrument_apertures.dat
```

or inline with repeatable records:

```text
DIRMAP_COVERAGE VECTOR_APERTURES
DIRMAP_APERTURE HEAD_A SM  0.10 0.97 0.22   0.00 0.22 0.98  30 60
DIRMAP_APERTURE HEAD_B GSM 0.20 -0.94 0.27  0.01 0.27 0.96  30 60
```

The AMPS command line provides equivalent controls:

```bash
-cutoff-dirmap-coverage VECTOR_APERTURES \
-cutoff-dirmap-aperture-file instrument_apertures.dat
```

and a repeatable inline form:

```bash
-cutoff-dirmap-aperture "HEAD_A SM 0.10 0.97 0.22 0.00 0.22 0.98 30 60"
```

CLI aperture-file selection is resolved before the file is opened, so it can safely
override a path present in a generic input template. `FULL_SPHERE` in either interface
recovers the complete historical sky-map calculation.

For multiple observation locations in one AMPS launch, Mode3D maintains two related
coordinates:

1. a compact **union cell list** used only as the common storage/MPI-reduction index; and
2. a **per-location list of indices into that union** used by the trajectory scheduler
   and output writers.

This distinction is important for C19. A snapshot containing GOES-13 and GOES-15 may
need four aperture lobes in the union, but the GOES-13 scheduler tasks reference only
its EAST/WEST lobes and the GOES-15 tasks reference only theirs. Expensive particle
traces are therefore proportional to the sum of the per-location aperture sizes, not
the Cartesian product of `N_locations x N_union_directions`. The directional cutoff and
direct-access files for one spacecraft likewise contain only that spacecraft's retained
cells.

A batched aperture row has one optional final association token:

```text
name frame bx by bz ux uy uz hHalfDeg vHalfDeg LOCATION=<global-trajectory-row>
```

Unqualified rows preserve the legacy behavior and apply at every location. During a
`SNAPSHOT_LIST` run, Mode3D removes aperture rows belonging to inactive epochs and
remaps the retained global trajectory row to the snapshot-local location ID. The union
of simultaneously active cells is retained only as an internal storage coordinate;
location-qualified rows do **not** expand another spacecraft's scheduled or written sky
map.

### Location-aware GRIDDED aperture scheduling

The Mode3D implementation intentionally keeps a rectangular union-map storage array so
MPI reductions and diagnostic slot identifiers remain simple and backward compatible.
That does **not** mean every location evaluates every union cell. `ApplyDirectionalMapCoverage3D()`
first constructs a full-grid mask for each location, then forms the compact union and
converts each location mask to compact union indices. A prefix-sum task table gives each
location its own variable task count. The global scheduler decodes a task through that
prefix table and maps the location-local directional ordinal to the correct union cell.

For DIRECT_ACCESS this means an adaptive task is exactly one
`(location, location-owned-direction)` pair. Dense access adds the rigidity index. For
PENUMBRA_SCAN the primary scalar task is followed by that location's directional cells
and, when requested, fixed-rigidity companion samples over those same cells. Unrelated
union slots stay at the MPI sentinel and are ignored by the per-location Tecplot
writers. This design fixes the former four-lobe GOES-13/GOES-15 plot artifact without
changing any retained trajectory direction or angular resolution.

The legacy `SM_PROXY` orientation remains available in C19 only as a fallback data
source. The runner converts that approximation into ordinary `LOCAL_SM` vector-aperture
records before launching AMPS; there is no EAST/WEST special case in the C++ selector.
With `--detector-orientation-source FILE`, the runner instead writes the actual per-head,
per-epoch boresight vectors to the generated aperture file, so optimized coverage is
fully compatible with time-varying spacecraft attitude.

### GRIDDED and GRIDLESS

GRIDDED and GRIDLESS are now observationally equivalent C19 science paths. Both use the same seed grid, the same shared adaptive-refinement algorithm, the same three-state `A(E,Ω)` semantics, and the same selected directional cells; both are folded by the identical spectrum/response/aperture postprocessor. In adaptive mode the realized refinement nodes may differ between directions because they follow the local access topology, but the algorithm and controls are identical in both solvers. The difference is intentionally confined to **magnetic-field evaluation along the trajectory**: GRIDDED interpolates the precomputed Mode3D mesh while GRIDLESS evaluates the configured background model directly. `--solver BOTH` is therefore a true apples-to-apples solver comparison rather than a direct-access-versus-effective-cutoff comparison. The scalar `PENUMBRA_SCAN` maps remain useful diagnostics but no longer define the GRIDLESS observational observable.

### GRIDLESS mesh-free execution and intra-rank threading

Standalone C19 `GRIDLESS` is intentionally **mesh-free**.  The early `-mode gridless`
branch in `srcEarth/main.cpp` initializes MPI and SPICE, parses the input, and calls the
gridless solver directly.  It returns before the historical `amps_init_mesh()` path.
Therefore a standalone T05/T96/T01/TA15/TA16/IGRF/DIPOLE GRIDLESS run does **not**:

- construct/read the AMR tree for the background field;
- allocate Mode3D cell-centered magnetic/electric-field arrays;
- populate a field mesh;
- gather the compact Mode3D field snapshot; or
- interpolate a precomputed field during particle tracing.

Instead, each trajectory step evaluates the selected background field directly at the
particle position.  `PIC::InitMPI()` is still required because it initializes the MPI
runtime; it is not a mesh initialization call.  The live SWMF-coupled build is a separate
case: its "gridless" field evaluator samples the SWMF field stored on the coupled AMPS
mesh and is therefore not the standalone mesh-free path used by C19.

C19 now uses the same two-level parallel design for GRIDLESS cutoff/direct-access work
that Mode3D uses for GRIDDED work:

```text
MPI ranks
  +-- rank/main thread: fetches MPI work chunks; performs MPI/progress/output updates
  |     +-- worker 0: independent trajectory task, private cFieldEvaluator
  |     +-- worker 1: independent trajectory task, private cFieldEvaluator
  |     +-- ...
  |     +-- worker N-1: independent trajectory task, private cFieldEvaluator
  +-- next MPI rank ...
```

The input-file controls are:

For dense GRIDLESS work, `GRIDLESS_MPI_DYNAMIC_CHUNK 0` still means **automatic** and the
common scheduler chooses roughly four inexpensive trajectory tasks per worker. Adaptive
`DIRECT_ACCESS` has a much heavier top-level work unit (one complete sky direction), so
the C19 runner instead resolves its default chunk to the worker count. With 16 threads
the rendered adaptive case therefore contains:

```text
GRIDLESS_PARALLEL             THREADS
GRIDLESS_THREADS              16
GRIDLESS_MPI_SCHEDULER        DYNAMIC
GRIDLESS_MPI_DYNAMIC_CHUNK    16
```

and the command includes:

```bash
-gridless-parallel THREADS \
-gridless-threads 16 \
-gridless-mpi-scheduler DYNAMIC \
-gridless-mpi-dynamic-chunk 16
```

An explicit positive `--dynamic-chunk` overrides this value. A chunk smaller than the
worker count can under-fill the local team; a much larger adaptive chunk can hoard many
expensive directions on one MPI rank and worsen the slow tail.

Only the rank/main thread calls MPI.  Worker threads compute complete flattened cutoff
or `(direction,rigidity)` direct-access tasks and write their result into private batch
slots.  After all workers in the batch join, the rank/main thread updates `RcMin`, the
directional-map arrays, the direct `A(E,Omega)` cube, and the MPI progress counter.
Consequently this implementation does **not** require `MPI_THREAD_MULTIPLE` and does not
put locks/atomics around the large result arrays.

The direct-field model interfaces have process-global Geopack/Tsyganenko snapshot state.
For the normal C19 architecture (one spacecraft/epoch per AMPS process), the epoch and
driver snapshot are frozen.  Worker evaluators are created serially, refreshed to that
same snapshot, and the shared model parameters are installed once before the worker team
starts.  If a general GRIDLESS `TRAJECTORY` input contains multiple distinct epochs in
one process, the code conservatively falls back to **SERIAL intra-rank field evaluation**
while keeping the selected MPI scheduler active.  This prevents multiple worker threads
from trying to represent different process-global Geopack epochs simultaneously.

The normal C19 runner requests `THREADS` explicitly for GRIDLESS.  `-nt N` becomes
`-gridless-threads N`.  With the runner's default `--dynamic-chunk 0`, GRIDLESS leaves
chunk sizing to the C++ auto resolver; an explicit positive `--dynamic-chunk` overrides
that behavior.

### Mode3D mesh

The current GRIDDED defaults are:

```text
--mode3d-mesh-res-earth-re 0.025
--mode3d-mesh-res-boundary-re 1.0
```

These are the finest settings from the previous mesh-convergence ladder. They may be
overridden explicitly for a new convergence study, but there is no separate convergence
runner.

### Detector attitude

The historical `SM_PROXY` basis remains the fallback when no authoritative attitude
file is available. For publication geometry use:

```bash
--detector-orientation-source FILE \
--detector-orientation-file /path/to/epead_orientation.csv
```

Preferred columns are one row per detector head and epoch:

```text
utc,spacecraft,detector,frame,
boresight_x,boresight_y,boresight_z,
aperture_north_x,aperture_north_y,aperture_north_z[,source]
```

`frame` is `SM` or `GSM`; `detector` is the actual instrument/telemetry head ID. For
regenerated GOES-13/15 references, the runner reads the required IDs from
`telemetry_head_east`/`telemetry_head_west`, so an attitude file normally contains rows
named `E` and `W`, not rows whose names assert a geometric direction. The two boresights
are independent and need not be opposite. This matters because the instrument geometry
is whatever each head was actually looking at at that epoch, not a hard-coded
geographic-east/geographic-west pair. Legacy references without telemetry-head fields
fall back to `EAST`/`WEST` IDs, and the old one-row `east_boresight_*` attitude schema is
still accepted only for backward compatibility.

Small explicit `--orientation-yaw-deg` and `--orientation-pitch-deg` offsets can be
applied to quantify pointing sensitivity while keeping the same production workflow.
The same perturbed vectors are used both for AMPS aperture pruning and for the detector
fold, preventing the optimization from selecting a different geometry than the science
calculation.

### Upstream anisotropy

The default source is isotropic. A bounded first-order directional model may be selected
explicitly:

```text
J(E,Ω) = J0(E) [1 + A (u · Ω)],    |A| < 1
```

using `--anisotropy-model DIPOLE`, `--anisotropy-amplitude`, and the directional-map
axis longitude/latitude options. The modeled E/W ratio is formed from the synthetic
detector signals, not merely normalized transmissions, so a selected anisotropic source
is represented consistently.

## 10. Outputs

The normal top-level output directory defaults to:

```text
test_output/C19_goes_epead_ew/
```

Independent GRIDLESS and `--gridded-batch OFF` directories remain:

```text
<solver>/<field-model>/<spacecraft>/<UTC-token>/
```

Default GRIDDED batches use:

```text
gridded/<field-model>/batch_<cutoff-search>/
```

Mode3D appends `_snapshot_<index>_<UTC-token>` to each output stem. Location IDs are
local to that snapshot, not global trajectory rows. `C19_batch_manifest.csv` is the
authoritative mapping used by the postprocessor.

Normal machine-readable products include:

| Product | Contents |
|---|---|
| `C19_commands.json` | Exact launch commands and working directories |
| per-batch `C19_batch_manifest.csv` | Global trajectory row, snapshot index, snapshot-local output location, suffix, spacecraft, epoch, field model, and search mode |
| `C19_reference_used.csv` | Selected observational rows plus actual detector IDs, exact flux-variable/correction-state, and ephemeris provenance |
| `C19_observation_ids.csv` | Chronological compact plot-label lookup (`T01`, `T02`, ...) to exact UTC. The same ID is reused across spacecraft/channel/model markers that belong to the same observation epoch. |
| `C19_spectrum_used.csv` | Epoch-dependent gamma/J0/E0 and measured/interpolated/file source |
| `C19_directional_cutoff.csv` | Per simulated spacecraft epoch and sky cell: cutoff source, rigorous lower/effective/upper Rc, separate equivalent-cutoff midpoint diagnostic, bound width, support censoring, and access-topology counts |
| `C19_detector_response_used.csv` | Exact response intervals/components plus `calibration_state`; publication calibration gate consumes this provenance |
| `C19_access_energy_grid.csv` | Common direct-access seed grid (`ADAPTIVE_SEED`) or dense requested grid (`DENSE_REQUESTED`) supplied identically to GRIDDED and GRIDLESS |
| `cutoff_3d_dir_access_loc_<local><snapshot-suffix>.dat` / per-run `cutoff_gridless_dir_access_point_0000.dat` | Solver-native direct three-state `A(E,Ω)` cubes consumed by the common detector fold |
| `C19_model.csv` / `C19_comparison.csv` | Production direct E/W results and bounds plus the independently labelled cutoff-rigidity proxy ratio/transmissions, spectrum source, response model, unresolved fractions, maximum trace time, search algorithm/policy, and map provenance.  The direct result is serialized at three levels: `direct_calculated_*` (finite direct fold before final acceptance), `direct_bound_midpoint_*` (diagnostic-only midpoint when only finite rigorous bounds exist), and legacy `modeled_*` (accepted scientific scalar only).  Explicit booleans record scalar availability/acceptance, trajectory-resolution and bound-width gates, and the tri-state convergence status (`NOT_TESTED` for a normal run). |
| `C19_model_coverage.csv` | One row for every selected reference row × solver × field model, explicitly identifying `DIRECT_ACCEPTED`, `DIRECT_CALCULATED_NOT_ACCEPTED`, `DIRECT_BOUNDS_ONLY`, cutoff-diagnostic-only, or missing run/post-processing result |
| `C19_aperture_availability.csv` | One row per epoch/head with availability status, coverage, direct bounds/scalar, response-weighted physical/unresolved termination fractions, primary TIME/STEP-limit response, response resolved by staged extension, response still unresolved after extension, final extension budget, direct bound width, unresolved asymmetry, and spectrum provenance |
| `C19_aperture_termination_budget.csv` | Compact one-row-per-head response-weighted termination budget: outer escape, inner loss, bounce/drift trap, time/step/distance limits, plus primary TIME/STEP-limit, resolved-by-extension, still-unresolved-after-extension, extension count/budget, and secular-drift diagnostics |
| `C19_access_classification_by_rigidity.csv` | Stage-A per-case/per-head classification on the mandatory common DIRECT_ACCESS seed rigidities: allowed, physical-forbidden, unresolved, detailed termination fractions, seed coverage, and normalized detector/spectrum weight |
| `C19_trace_budget_sweep.csv` (from `run_C19_convergence.py`) | Phase-1/2/3 distance/time/timestep/mover/drift-recurrence convergence summary for the representative epoch |
| `C19_metrics.csv` | Per-spacecraft and aggregate finite/saturated fractions, sign agreement, bias, MAE, RMSE, correlation, and provisional gate |
| `C19_direction_sense_diagnostic.csv` | Production arrival→look convention and legacy opposite-convention diagnostic |
| `C19_aperture_samples.csv` | Representative aperture cells including lower/effective/upper cutoff, topology, raw termination counts, trace maxima, and transmission |
| `C19_result.json` | Staged validity gates, rigorous-bound compatibility counts, unresolved-asymmetry and staged trace-extension diagnostics, spectrum/calibration/orientation provenance, thresholds, hashes, failures, limitations, plot list, and overall result |
| `C19_trace_extension_<solver>_<field>.png` | Response-weighted before/after staged-extension diagnostic for EAST/WEST, including primary TIME/STEP-limit response, response resolved by extension, response still unresolved, and rigorous `log10(E/W)` bound width |
| `C19_summary.txt` | Human-readable execution/trajectory/fold/observation/overall status |

Every completed C19 run also writes the standard visual products without any special
flag.  Each figure is written twice from the same live Matplotlib figure: a PNG for
quick inspection and an EPS companion with the same basename for publication/vector
work.  EPS is generated directly from Matplotlib (not converted from PNG), so axes,
lines, markers, and observation-ID text remain vector objects.  Because EPS/PostScript
does not support alpha transparency, partially transparent diagnostic artists are
rendered opaque in the EPS copy.

The comparison/scatter/parity family uses compact chronological observation IDs
(`T01`, `T02`, ...) next to plotted observational/model points.  Exact UTC values are
kept out of the figure to avoid crowding and are recovered from
`C19_observation_ids.csv`.  Time-series comparison plots label each GOES observation
point with the same ID, so a point can be traced consistently between the time-series,
scatter, and parity views.

| Plot | Contents |
|---|---|
| `C19_comparison_<solver>_<field>.png` | All selected GOES reference points and the complete direct-information hierarchy: accepted direct scalar, open calculated-but-not-accepted direct scalar, open-diamond direct-bound midpoint when no scalar exists, rigorous direct intervals/censoring, equivalent-cutoff midpoint diagnostic with Rc bounds, and markers for missing AMPS rows |
| `C19_scatter_<solver>_<field>.png` | Data-ranged observed-versus-model comparison using the same canonical row selector as parity/residual: filled accepted direct points, open calculated-but-not-accepted direct points, open-diamond direct-bounds-only midpoints, and open green cutoff-midpoint diagnostics |
| `C19_parity_<solver>_<field>.png` | Common-range parity view of exactly the same four populations as the scatter plot plus the 1:1 line; the shared selector prevents one plot from silently dropping a direct value shown by another |
| `C19_residual_<solver>_<field>.png` | Accepted direct residuals, open calculated-but-not-accepted residuals, rigorous residual intervals for bounds-only rows, and separately styled cutoff-midpoint diagnostic residuals versus time |
| `C19_transmission_<solver>_<field>.png` | EAST/WEST accepted direct scalars, unconditional direct Tmin–Tmax bands, per-head status markers, and separately styled hard-cutoff proxy transmissions |
| `C19_aperture_diagnostic.png` | Representative aperture-cell cutoff/access diagnostic; direct-access cells are colored by the midpoint of their explicit transmission bounds, while the CSV retains both bounds |
| `C19_access_classification_<spacecraft>_<channel>_<UTC>[ _<solver>_<field>].png` | Restored Stage-A rigidity-resolved EAST/WEST diagnostic. The left axis shows geometric solid-angle fractions classified Allowed / Physical forbidden / Unresolved at every common seed rigidity; the dotted right axis shows the normalized detector/spectrum weight. Solver/model suffixes are added only when more than one calculation would otherwise overwrite the same observational case. |
| `C19_directional_cutoff_<solver>_<field>_<spacecraft>_<UTC>.png` | Four panels for every simulated spacecraft epoch: rigorous Rc lower bound, equivalent-cutoff midpoint diagnostic, rigorous Rc upper bound, and retained bound width. GRIDDED files now contain only the directional cells retained for that spacecraft/location (not the union of all spacecraft apertures in the snapshot). Diagnostic-only midpoint cells and support-censored cells are outlined |
| `C19_boundary_spectrum.png` | Assumed incident boundary proton spectrum for every selected epoch over the P4/P5 response support |

Every `.png` entry in the table above has an automatically generated `.eps` companion
with the same basename.

### Optional diagnostic-free publication comparison plots

The standard plots above remain the default because C19 validation must expose rejected
scalars, bounds, cutoff-proxy diagnostics, censoring, and missing-run information.  For a
manuscript/presentation figure, the runner can additionally create a deliberately clean
comparison family without changing or replacing those standard plots:

```bash
python3 srcEarth/test/C19/run_C19.py \
  ...existing run options... \
  --publication GOES13,GOES15
```

`-publication` is accepted as a compatibility alias, including the historical shell form
`-publication =goes13,goes15`.  Values are case-insensitive and may select `GOES13`,
`GOES15`, or both.  The requested publication spacecraft must also be present in the
normal `--spacecraft` selection.  If `--publication` is omitted, **nothing changes** in
the historical/default plot set.

Publication figures use only two scientific populations: the selected GOES observations
and AMPS DIRECT_ACCESS scalars that passed the ordinary C19 acceptance gates.  They
exclude calculated-but-unaccepted direct points, direct-bound midpoints and intervals,
cutoff-rigidity proxy points/bounds, censoring markers, and missing-model/run diagnostics.
No diagnostic quantity is promoted into an accepted science point.  The existing `Txx`
observation annotations are retained and map to exact UTC through
`C19_observation_ids.csv`.  The selected spacecraft are encoded in the basename, e.g.

```text
C19_publication_comparison_gridded_t05_goes13_goes15.png/.eps
C19_publication_scatter_gridded_t05_goes13_goes15.png/.eps
C19_publication_parity_gridded_t05_goes13_goes15.png/.eps
```

This mode is post-processing only, so it can be added to an existing compatible run with
`--skip-run`; the AMPS trajectories do not need to be recomputed.

Exact zero transmission remains a dedicated saturation state rather than an arbitrary
finite substitute. Unresolved cells are carried through lower/upper bounds and the
configured unresolved-aperture validity gate.

Plot families are failure-isolated.  An exception in scatter/parity/residual generation
is recorded in `C19_result.json:plot_generation_errors` and does not prevent the
transmission, directional-cutoff, boundary-spectrum, or aperture-diagnostic figures from
being attempted.  This is intentionally a reporting-only safeguard; a plot failure does
not change any C19 scientific result.

The two core comparison products have an additional recovery guarantee.  The primary
`make_comparison_plots()` renderer remains authoritative, but after it runs C19 invokes a
minimal `make_core_comparison_fallback_plots()` recovery pass.  The fallback writes only
`C19_comparison_<solver>_<field>.png` and `C19_scatter_<solver>_<field>.png`, and only
when the corresponding primary file is missing.  It never overwrites a successfully
rendered figure.  The recovery path consumes only the already constructed `ModelRow`
objects and the shared `direct_plot_groups()` selector, so it works during `--skip-run`
and cannot trigger another AMPS calculation.  Calculated-but-not-accepted DIRECT_ACCESS
values remain open markers, bounds-only rows remain diagnostic midpoints/intervals, and
the cutoff midpoint remains separately labelled.

A related plotting bug was fixed at the same time: selected GOES reference timestamps
and model timestamps are now converted to `datetime` objects before they share an axis.
The previous implementation could put ISO timestamp strings and Python `datetime`
objects on the same Matplotlib x-axis.  Depending on the selected cadence and installed
Matplotlib version, that could cause unit conversion/categorical-axis failure while
building the time-series comparison.  Because the scalar scatter plot was generated
later in the same comparison family, both `C19_comparison_*` and `C19_scatter_*` could
then be absent even though `C19_comparison.csv` contained valid rows.  The homogeneous
axis-unit rule plus the core fallback makes this failure mode auditable and recoverable.

### Direct scalar availability, acceptance, and plotting contract

C19 deliberately distinguishes **calculated**, **accepted**, and **convergence-validated**
DIRECT_ACCESS information.  These states answer different questions and must never be
collapsed into one nullable number:

```text
direct_calculated_log10_east_west_ratio
    finite central value produced by the direct detector fold before final gates

direct_bound_midpoint_log10_east_west_ratio
    plotting-only midpoint of a finite rigorous direct interval when no scalar exists

modeled_log10_east_west_ratio
    accepted scientific direct scalar; this is the legacy metric/acceptance field

direct_convergence_status
    PASS | FAIL | NOT_TESTED (ordinary runs currently report NOT_TESTED)
```

A value may therefore be numerically calculated yet not accepted, for example when
`--max-direct-ratio-bound-width-log10` rejects a broad rigorous interval.  That scalar
remains in the CSV and is drawn with an **open DIRECT_ACCESS marker**.  If the direct
fold cannot construct a scalar because trajectory uncertainty is too large, but finite
rigorous bounds exist, C19 draws an **open diamond at the direct-bound midpoint** and the
full interval.  Neither diagnostic value enters MAE/RMSE/correlation or PASS/FAIL.

`direct_plot_groups()` is the single row classifier used by the time-series/scatter/
parity/residual family.  Individual plot functions must not reimplement status filtering.
`C19_result.json:plot_consistency` records the number of calculated, accepted,
bounds-only, and cutoff-diagnostic rows and verifies that the canonical plotting
populations match the serialized ModelRow fields.  This specifically prevents the
regression where the parity plot showed a direct value while the scatter plot omitted it.

A normal SMOKE/ROUTINE/FULL run does **not** fail or hide a direct scalar merely because
the separate convergence campaign was not executed.  `NOT_TESTED` means exactly that;
convergence studies may later promote a result to a stronger validation state, but they
do not control whether the ordinary calculated value is retained for diagnosis.

## 11. Acceptance behavior

The observational thresholds remain provisional:

```text
finite modeled fraction      >= 0.85
correct E/W sign fraction    >= 0.90
correlation                  >= 0.60
mean absolute log10 error    <= 0.20
RMS log10 error              <= 0.30
```

They are evaluated **after** the trajectory/fold validity stages. Sign agreement remains a direction-
convention/coarse-physics diagnostic and cannot compensate for a poorly resolved or
strongly saturated magnitude comparison.

The summary now reports:

```text
execution complete:       PASS/FAIL
trajectory resolution:    PASS/FAIL
instrument fold:          PASS/FAIL
observational validation: PASS/FAIL
overall:                  PASS/FAIL
acceptance enforcement:   ON/OFF
```

`--enforce-acceptance` controls only shell exit policy; it never changes the scientific
labels. Exit status 2 is reserved for input/launch/output/postprocessing failures. A run
that executes successfully but fails trajectory resolution, detector folding, or GOES
agreement is a scientific FAIL and becomes shell exit status 1 only when
`--enforce-acceptance` is requested.



### GRIDLESS live progress reporting

The threaded GRIDLESS solver uses the same **completed-task** progress semantics as
Mode3D.  Progress is not based on tasks merely fetched from the dynamic MPI queue.
Each successful trajectory worker increments a rank-local atomic completion counter;
the rank/main thread polls that counter every 200 ms and transfers newly completed work
to the global MPI RMA completion counter.  Worker threads never call MPI.

The 200-ms polling cadence is intentionally **not** the stdout cadence.  Rank 0 suppresses
identical progress lines and normally prints only after about 0.1% of the global task set
has newly completed.  For unusually slow trajectories, a line may be printed after 10 s
provided at least one additional task has completed.  Consequently a stalled count such
as `Task 0/98897` is printed once, not once per second.  The final 100% line is also
printed exactly once.

ETA is withheld until at least 0.1% of the work (and at least 32 tasks) has completed and
at least 10 s have elapsed.  This avoids meaningless startup estimates based on one or a
few exceptionally expensive trajectories.

A GRIDLESS run therefore prints an initial line immediately and then lines of the form

```text
[Gridless cutoff TRAJECTORY] [rank 0/global over 4 MPI ranks] [####--------------------------------] 12.3%  (LocEq 0/1, Task 1234/9999)  ETA 00:08:42
```

The important detail is that updates occur **while a threaded MPI chunk is still
running**. Dense GRIDLESS may still use an automatic chunk near 64 trajectory tasks for
16 workers; adaptive C19 normally uses a worker-sized chunk of 16 heavy direction tasks.
In either case the display does not wait for the complete batch to finish before showing
completed work. If rank 0 owns an unusually long task, its main thread still polls the
global RMA counter and displays completions reported by the other MPI ranks.  After the assignment
queue is exhausted, rank 0 continues polling until the global completed-task count
reaches 100%, matching the slow-tail behavior of the GRIDDED Mode3D progress display.

This progress mechanism changes only reporting granularity; it does not alter task
scheduling, trajectory physics, result accumulation, or MPI/thread ownership rules.

## 12. Interpretation and limitations

C19A should be interpreted in layers even though it has only one execution path. The
trajectory/cutoff product must first be resolved and numerically trustworthy; the
synthetic detector fold must then be valid; only after those gates should the GOES
observational comparison be interpreted. A successful GRIDDED C19A supports the more
specific claim:

> With the documented event spectrum, detector response, real spacecraft position, and direct three-state geomagnetic access `A(E,Ω)`, AMPS reproduces the measured GOES EPEAD East–West proton-access ratio for the selected event within the stated validation tolerances.

A systematic sign disagreement deserves special attention before model tuning. If an older C19 result has the opposite sign, inspect `C19_direction_sense_diagnostic.csv`: the production `AMPS_ARRIVAL_TO_DETECTOR_LOOK` mapping should be compared with the legacy direct-vector diagnostic. The production minus sign follows from the distinction between incoming particle direction and telescope look direction and should not be treated as a tunable convention.

C19A alone does not support a claim of exact detector count-rate prediction because:

- the committed default response now includes the documented high-energy secondary proton *energy* response for the uncorrected P4/P5 products, but it is still a factorized proxy rather than the full calibrated energy-angle response matrix;
- the published secondary geometrical factors represent side/rear penetrating-particle response integrated over incidence geometry; C19 currently applies those energy weights with the same nominal angular-aperture model, so component-specific out-of-aperture angular response is not yet reconstructed;
- the two detector heads may retain relative calibration differences;
- `OBSERVED_WEST` is an event-derived spectral approximation; an independent upstream spectrum is preferred when available;
- the production default source is isotropic; anisotropy quantifies a bounded dipole-anisotropy sensitivity, but a measured upstream angular distribution is still preferable when available;
- the prompt event onset remains unsuitable when interplanetary anisotropy is large; and
- provisional observational acceptance thresholds require refinement from multiple events.


A publication-quality extension should replace the current factorized primary+secondary response CSV with the best available calibrated **energy-angle** EPEAD response, use an independent upstream spectrum when possible, quantify detector-head intercalibration and spectral uncertainty, add more events/yaw states, and recheck numerical convergence when changing the selected production settings and additional field models (T96/T05/SWMF as appropriate). Exact ephemeris ingestion, direct response folding, angular/mesh convergence machinery, solver cross-checks, attitude-file support, and anisotropy sensitivity are now implemented rather than deferred; the committed legacy reference remains a nominal-slot artifact and must be regenerated with NOAA ephemeris for a science run.

## 13. Troubleshooting

### Reference file is missing

```text
C19A reference is missing ...
```

Run:

```bash
python3 srcEarth/test/C19/build_goes_reference.py --download \
  --goes13-ephemeris /path/to/goes13_orb1m.nc \
  --goes15-ephemeris /path/to/goes15_orb1m.nc
```

### Driver file is missing

Run:

```bash
python3 srcEarth/test/C19/tools/prepare_official_ts05_driver.py
```

### `mpirun` is not available

Load the system MPI environment or override the launcher:

```bash
--mpirun mpiexec_mpt
```

### GEO trajectory transformation fails

Verify that AMPS was built with SPICE, the required kernels are available, and the epoch is covered. `TRAJ_FRAME GEO` cannot be converted correctly without SPICE.

### Directional-map file is missing

With the default `DIRECT_ACCESS` mode, a directional cutoff-map file is intentionally **not** written and is not required by the runner; aperture geometry is reconstructed from the direct-access cube. If `--cutoff-search PENUMBRA_SCAN` was explicitly requested, verify that the executable supports `DIRECTIONAL_MAP T`, that `DIRMAP_LON_RES` and `DIRMAP_LAT_RES` are positive, and that the cutoff calculation completed.

### Modeled E/W has the opposite sign at nearly every epoch

1. Inspect `C19_direction_sense_diagnostic.csv`.
2. Compare `AMPS_ARRIVAL_TO_DETECTOR_LOOK` with `LEGACY_DIRECT_DIAGNOSTIC` in `C19_summary.txt`.
3. Confirm that the production result uses `AMPS_ARRIVAL_TO_DETECTOR_LOOK`.
4. Check the event-specific telemetry-head-to-physical-look-direction mapping independently.
5. If an older C19 result matches only `LEGACY_DIRECT_DIAGNOSTIC`, that is evidence that the older postprocessor compared AMPS incoming particle direction directly with the telescope look direction.

Do not swap the GOES EAST/WEST labels to compensate. The AMPS-arrival-to-look minus
sign is part of the detector geometry and is now exercised by the runner self-test.

### Many rows report `ZERO_WEST_TRANSMISSION` or `ZERO_EAST_TRANSMISSION`

This is no longer treated as missing aperture coverage. Inspect the per-cell termination diagnostics and
inspect `C19_aperture_samples.csv` and the extended directional-map termination counters. A high apparent cutoff should not be interpreted until `DISTANCE_LIMIT`/`TIME_LIMIT`/`STEP_LIMIT` terminations have been separated from physical loss/trapping and trace-budget sensitivity has been reviewed. If the production
PENUMBRA/UNRESOLVED map is well resolved, then continue with angular/rigidity/spectrum
and instrument-response sensitivity rather than relabeling trace limits as forbidden.

### Many rows report `EXCESSIVE_UNRESOLVED_*`

Inspect the EAST/WEST unresolved solid-angle fractions and the per-cell termination
counts. A large `n_distance_limit`, `n_time_limit`, or `n_step_limit` means the current
finite trace budget did not establish physical access for those rigidity samples. Rerun the same workflow with a controlled larger trace budget to determine whether the unresolved fraction decreases robustly.
Do not convert the unresolved cells to forbidden merely to obtain a finite E/W ratio.

### Rows report `STATIC_FIELD_DOMINATED`

This no longer means merely that one trajectory exceeded 300 s.  Inspect
`C19_aperture_availability.csv` and `C19_static_field_guardrail_*.png`.  A hard guard now
means either (1) the detector-weighted response fraction that is both long-duration and
UNRESOLVED exceeds `--max-long-unresolved-response-fraction`, or (2) an explicitly
enabled `--max-static-field-ratio-bound-width-log10` threshold is exceeded by the
conservative frozen-field E/W sensitivity interval.

Rows with `static_field_warning_triggered=true` but
`static_field_guardrail_triggered=false` retain their accepted direct scalar.  A large
`static_field_response_dominance_warning` should still be reviewed physically, but it is
not treated as equivalent to unresolved trajectory weight.  Do not raise the numerical
trace limit or relabel long trajectories as forbidden merely to remove the warning.

### EAST or WEST is absent from the transmission plot

`C19_aperture_diagnostic.png` can contain nonzero colors even when the comparison has
no accepted direct E/W scalar. For DIRECT_ACCESS, each color is only
`0.5*(transmission_min + transmission_max)` for that sky cell. It visualizes the center
of a rigorously retained interval; it is not an accepted transmission measurement. The
broad-aperture direct scalar is emitted only when both the detector-weighted unresolved
fraction and the detector-weighted resolved-transition-bracket fraction are below their
configured tolerances for **both** heads. Because a ratio of two unaccepted midpoints
would hide potentially large and asymmetric uncertainty, the comparison correctly
leaves the production direct curve absent and plots its ratio bounds/censoring instead.

The dashed `AMPS equivalent-cutoff midpoint (diagnostic only)` curve answers a different
question: what E/W results after deliberately collapsing every directional access curve
to one blocked-area midpoint cutoff?  For an unresolved rigidity bracket the midpoint
uses the explicit `0.5*dR` diagnostic convention while the full `[0,dR]` contribution is
retained in `Rc_lower/Rc_upper`.  Consequently the dashed diagnostic can remain visible
even when the direct scalar is withheld.  This is intentional and is now encoded in a
separate `rc_midpoint_diagnostic_gv` field; it is not `Rc_effective`, never converts a
direct row to `VALID`, and is excluded from direct acceptance metrics.

The comparison also draws an open marker at the midpoint of a finite rigorous direct
log-ratio interval when no direct scalar is accepted.  That marker is visualization only;
the vertical interval remains the scientific statement.

An absent accepted-scalar line is not automatically zero transmission. Inspect
`C19_aperture_availability.csv` in this order:

1. `selected_sky_cells`, `forward_facing_cells`, and `geometric_aperture_cells`
   diagnose work pruning, arrival-to-look mapping, longitude wrapping, attitude, and
   angular resolution.
2. `cells_with_access_samples` and `cells_with_response_overlap` diagnose missing,
   truncated, duplicate/nonmonotonic, or wrong-energy direct-access products. The
   parser makes malformed support and seed grids fatal before folding.

If the *number of timestamps* itself is smaller than expected, inspect
`C19_model_coverage.csv` and the profile summary first.  `SMOKE` intentionally keeps at
most three **common synchronized epochs** across all requested spacecraft/channels;
`--time-step-minutes` does not override that behavior.  `ROUTINE` intentionally samples
each spacecraft's five-minute reference at 60-minute cadence.  Use `--profile FULL` (or
use `--profile ROUTINE --time-step-minutes 0`) to request every valid selected reference
epoch.  A timestamp that was selected but has no ModelRow is shown explicitly in the
comparison plot as a missing run/post-processing marker rather than silently disappearing.
3. `contributing_cells` and `solid_angle_coverage_fraction` diagnose partial aperture
   coverage.
4. `transmission_min/max`, unresolved fraction, and discrete-transition fraction
   distinguish rigorous uncertainty from a physical zero.
5. `east_aperture_status`, `west_aperture_status`, and `status_reasons` retain both
   head diagnoses even when the aggregate status reports only the first failing gate.

The transmission plot always draws available Tmin–Tmax bounds. `PHYSICAL_ZERO` appears
as a real scalar at zero; missing geometry/access/response is shown as a categorical
status marker. The comparison plot does not place invalid rows at an invented log-ratio
value.

The runner self-test includes independent fully allowed and fully forbidden EAST/WEST
cubes, a deliberately asymmetric direction-mapping case, FILE-boresight cases on both
sides of the 0/360 longitude seam, malformed adaptive-grid detection, per-head report
generation, and geometry-only solid-angle convergence at 2.5, 1.25, and 0.625 degrees.
For a science run, repeat the actual AMPS calculation at those angular resolutions and
compare the recorded solid angle and E/W bounds; the self-test validates the machinery,
not event-specific convergence.

### Too many launches

GRIDDED should report one launch per selected field model when
`--gridded-batch AUTO` is active. If it reports one launch per spacecraft epoch, check
for `--gridded-batch OFF`. GRIDLESS intentionally remains case-per-process. For either
solver, use `--profile SMOKE`, a larger `--time-step-minutes`, one spacecraft, or one
model during debugging.

## 14. References and public sources

1. NOAA National Centers for Environmental Information, *NOAA GOES 1–15 Space Environment Monitor (SEM) L1B & L2 Data, Version 0 (superseded)*, operational 5-minute GOES-13/15 EPEAD `p17ew` subset used by C19A, dataset identifier `gov.noaa.ncei.swx:goes_sem-l1b-l2_swpc`, DSI `2086_01`. NCEI recommends Version 1.0 for general use; C19A retains the fixed historical directional files for reproducibility.
2. Rodriguez, J. V., T. G. Onsager, and J. E. Mazur (2010), “The east–west effect in solar proton flux measurements in geostationary orbit: A new GOES capability,” *Geophysical Research Letters*, 37, L07109, doi:10.1029/2010GL042531.
3. Rodriguez, J. V., J. C. Krosschell, and J. C. Green (2014), “Intercalibration of GOES 8–15 solar proton detectors,” *Space Weather*, 12, 92–109, doi:10.1002/2013SW000996.
4. Kress, B. T., J. V. Rodriguez, J. E. Mazur, and M. Engel (2013), “Modeling solar proton access to geostationary spacecraft with geomagnetic cutoffs,” *Advances in Space Research*, 52(11), 1939–1948, doi:10.1016/j.asr.2013.08.019.
5. Rodriguez, J. V. et al. (2017), “On the Use of NOAA GOES Energetic Particle Detector Integral Fluxes,” *Space Weather*, 15, 290–309, doi:10.1002/2016SW001533. Table A2 documents the P4/P5 high-energy secondary proton geometrical factors used by the committed uncorrected-response proxy.
6. Tsyganenko, N. A., and M. I. Sitnov (2005), “Modeling the dynamics of the inner magnetosphere during strong geomagnetic storms,” *Journal of Geophysical Research*, 110, A03208, doi:10.1029/2004JA010798.

Machine-readable BibTeX entries are provided in `references.bib`.


## 2026-08-19 phased DIRECT_ACCESS trajectory-resolution and validation update

This stable-version update implements the staged C19 recovery plan.  The organizing
principle is unchanged: **DIRECT_ACCESS remains the primary C19 observable**.  The test
must first establish the physical fate of the full-orbit trajectories that contribute to
the EAST and WEST detector responses; a cutoff proxy is not allowed to manufacture a
finite observational scalar when those trajectories are unresolved.

The implementation deliberately separates evidence gathering, numerical convergence,
positive trapped-orbit classification, scientific status, and publication-grade
observational inputs.  This ordering prevents a new classifier from hiding the original
failure mechanism and makes every change auditable.

### Phase 0 -- termination instrumentation and response-weighted accounting

Both GRIDDED and GRIDLESS DIRECT_ACCESS writers serialize a diagnostic record for every
trajectory that was actually evaluated, including adaptive refinement nodes.  The
Tecplot rows contain

```text
termination_code
trace_time_s
trace_distance_Re
trace_steps
retry_count
mirror_points
bounce_cycles
drift_revolutions
drift_angle_deg
trap_mechanism
momentum_relative_spread
```

Tecplot POINT rows are numeric, so `termination_code` is the serialized termination
reason.  Each DIRECT_ACCESS file now carries an `AUXDATA TERMINATION_REASON_CODES=...`
map so the raw product is self-describing.  Codes 0--8 retain their historical numeric
values; code 9 is appended rather than inserted so archived products are not silently
reinterpreted:

```text
0 OUTER_BOUNDARY_ALLOWED
1 INNER_BOUNDARY_FORBIDDEN
2 MAGNETICALLY_TRAPPED_FORBIDDEN       # legacy bounce recurrence
3 TIME_LIMIT
4 STEP_LIMIT
5 DISTANCE_LIMIT
6 INVALID_TIME_STEP
7 INVALID_FIELD
8 NUMERICAL_FAILURE
9 DRIFT_TRAPPED_FORBIDDEN              # positive full-orbit drift recurrence
```

`trap_mechanism` remains a compact auxiliary diagnostic (`0=NONE`, `1=BOUNCE`,
`2=DRIFT`).  The explicit termination code 9 is the authoritative distinction between
legacy bounce trapping and the C19 drift-recurrence resolver.

The trajectory records are sparse by design.  Adaptive DIRECT_ACCESS does not evaluate
every node in its deterministic candidate tree; worker threads therefore accumulate
records only for evaluated slots, MPI ranks gather those sparse records, and rank 0 sorts
them by the global flattened `(location, sky-cell, candidate-rigidity)` slot before
writing.  This preserves a one-to-one audit trail without allocating metadata for unused
adaptive candidates.

The Python fold now emits two complementary levels of termination accounting:

* `C19_aperture_samples.csv` remains the detailed per-sky-cell audit product.
* `C19_aperture_termination_budget.csv` is a compact one-row-per-head Phase-0 product.
  It reports response-weighted fractions attributed to
  `OUTER_BOUNDARY_ALLOWED`, `INNER_BOUNDARY_FORBIDDEN`, legacy magnetic/bounce trapping,
  `DRIFT_TRAPPED_FORBIDDEN`, `TIME_LIMIT`, `STEP_LIMIT`, `DISTANCE_LIMIT`, and other
  unresolved/numerical states.
* `C19_aperture_availability.csv` carries the same head-level physical/unresolved
  decomposition together with transmission bounds, aperture coverage, direct E/W bound
  width, and spectrum provenance.

The response-weighted termination budget is a **diagnostic**, not a second access fold.
For an energy interval, half of the exact response-weighted `J(E)G(E)` integral is
attributed to each endpoint termination reason.  This makes the termination budget sum
meaningfully without inventing an unknown transition energy inside an unresolved
interval.  The rigorous DIRECT_ACCESS lower/upper transmission bounds continue to use
the conservative three-state access logic and remain authoritative.

#### Restored rigidity-resolved access-classification diagnostic

Every modeled reference row also writes a rigidity-resolved Stage-A diagnostic to
`C19_access_classification_by_rigidity.csv` and a corresponding
`C19_access_classification_*.png` figure.  This product was intentionally restored after
a packaging regression dropped the plot family even though the direct access cubes still
contained the required trajectory states.  The runner self-test now requires this family
so the regression cannot recur silently.

For each physical EAST/WEST head and each **mandatory common DIRECT_ACCESS seed
rigidity**, the postprocessor reapplies the exact channel aperture geometry and partitions
the retained sky solid angle into

```text
Allowed
Physical forbidden
Unresolved
```

The CSV also preserves the detailed termination fractions at that rigidity:
outer-boundary escape, inner-boundary loss, legacy bounce trapping, drift trapping,
time/step/distance limits, and other termination.  The simple three-curve PNG is kept
intentionally readable; detailed cause attribution remains machine-readable in the CSV.

The classification fractions use **geometric solid-angle weighting** (`cos(latitude)`)
rather than source anisotropy weighting.  This keeps the figure a trajectory/access
diagnostic instead of making its classification depend on an assumed SEP angular
distribution.  Source-weighted classification fractions are nevertheless written as
separate CSV columns for sensitivity studies.

The dotted secondary-axis curve is not an arbitrary response value sampled at isolated
energies.  For every adjacent common seed pair the runner integrates the same exact
`J(E) G(E)` function used by the production fold, assigns half of that interval integral
to each endpoint, and normalizes the endpoint weights to unity.  This makes the curve a
true indication of which rigidity nodes dominate the synthetic detector signal and avoids
grid-spacing artifacts.

Adaptive DIRECT_ACCESS may add different midpoint/refinement nodes in different sky
directions.  Those direction-specific nodes are deliberately **not** mixed into the
classification plot.  Only the mandatory common seeds are shown, because every selected
direction is guaranteed to contain them.  Consequently a change in adaptive refinement
depth cannot change the apparent detector weight simply by creating more samples.

The figure is generated for **every selected epoch × spacecraft × channel × solver ×
field model**, including cases with no accepted direct E/W scalar.  This is essential:
the diagnostic is most useful precisely when the direct fold is inconclusive.  It never
participates in C19 acceptance or changes any trajectory classification.

### Phase 1A -- reproduce and partition the original failure before enabling drift recurrence

The new helper

```text
srcEarth/test/C19/run_C19_convergence.py
```

runs the decisive single-epoch convergence experiment.  Its default target is
GOES-13 P4 at `2012-05-17T06:00:00Z`, GRIDDED/T05.  The distance-budget baseline **turns
DRIFT recurrence off** so the new physics resolver cannot consume the trajectories whose
old failure mechanism is being measured.  By default it evaluates

```text
MAX_TRACE_TIME = 300 s
MAX_TRACE_DISTANCE = 400, 1000, 2500, 8000, 0 Re
TRAP_DRIFT_DETECTION = F
```

where distance `0` means no cumulative-path ceiling.  The extra zero-distance case is
important: it verifies whether a very large finite path ceiling is already equivalent to
a purely time-controlled trace.

Example:

```bash
python3 srcEarth/test/C19/run_C19_convergence.py \
  --amps ./amps --mpirun mpirun -np 4 -nt 16 \
  --output-root test_output/C19_convergence
```

The child C19 products are preserved in separate case directories.  The aggregate
`C19_trace_budget_sweep.csv` and convergence plots track, for EAST and WEST,

* total response-weighted unresolved fraction;
* `DISTANCE_LIMIT`, `TIME_LIMIT`, and `STEP_LIMIT` fractions;
* response weight ending as inner loss, bounce trapping, or drift trapping;
* direct E/W rigorous-bound width;
* overall C19 status.

A falling unresolved fraction as the path budget increases directly demonstrates a
path-budget artifact.  A plateau after the path cap ceases to control means only that a
**long-lived unresolved** population remains at the selected time budget; it does not by
itself prove drift trapping.  Positive trapped classification is reserved for Phase 3.

### Phase 1B / Phase 2 -- use physical trace time as the production budget and test convergence

Historical C19 input used `MAX_TRACE_DISTANCE 400` together with a 300-s time ceiling.
`MAX_TRACE_DISTANCE` is cumulative particle path length, not geocentric radius.  A fixed
path ceiling therefore corresponds to a shorter physical integration time at higher
proton energy and can imprint an artificial energy dependence on `A(E,Omega)`.

The committed production templates now use

```text
MAX_TRACE_TIME       300.0
MAX_TRACE_DISTANCE   0.0
```

so all energies receive the same physical trace-time budget.  The distance ceiling is
retained only as an optional machine-safety/convergence control.  For every requested
energy, `C19_access_energy_grid.csv` records proton beta, speed in Re/s, the time implied
by a finite path ceiling, and the nominal controlling trace limit.

The convergence helper also runs a time-only baseline with drift recurrence disabled.
It explicitly sets `--unresolved-extension-passes 0` for every child case so the x-axis
really is the requested 60/120/300/600-s budget rather than a hidden extended budget:

```text
MAX_TRACE_DISTANCE = 0
MAX_TRACE_TIME     = 60, 120, 300, 600 s
CUTOFF_UNRESOLVED_EXTENSION_PASSES = 0
TRAP_DRIFT_DETECTION = F
```

This distinguishes a path-cap artifact from a result that is still sensitive to the
physical trace duration.  Production acceptance should be based on convergence of the
**observable and its rigorous bounds**, not merely on crossing the 5% unresolved gate.

Optional `--dt-values` and `--mover-values` sweeps are provided for timestep and mover
cross-checks.  The normal C19 reference mover remains RK4; a BORIS value may be requested
when the linked AMPS build provides that mover.  The purpose is numerical cross-checking,
not a silent production-mover change.

### Phase 2B -- unresolved-only staged trace extension (production default)

A global increase of `CUTOFF_MAX_TRAJ_TIME` is a poor first response to a large unresolved
population: every easy escape/impact trajectory pays the extra cost, and increasingly long
traces make the frozen-field T05 approximation less defensible.  C19 therefore keeps the
normal 300-s primary calculation unchanged and extends **only** trajectories whose primary
physical classifier ended at `TIME_LIMIT` or `STEP_LIMIT`.  The committed templates use

```text
CUTOFF_MAX_TRAJ_TIME                 300
CUTOFF_UNRESOLVED_EXTENSION_PASSES   2
CUTOFF_UNRESOLVED_EXTENSION_FACTOR   2.0
```

which yields candidate total-time budgets of 300, 600, and 1200 s.  A trajectory already
classified as outer-boundary allowed, inner-boundary forbidden, bounce trapped, or drift
trapped is never repeated.  `DISTANCE_LIMIT` is deliberately not extended: changing a
cumulative path cap is a different numerical assumption and must remain explicit.

Each extension pass **restarts from the original phase-space seed** with the larger total
time budget.  A true continuation would save CPU time, but would require serializing the
private full-orbit mover and trap-detector state.  Restarting guarantees that an extended
result is the same experiment as an independent run started with the larger `T_max`, and
keeps GRIDDED and GRIDLESS semantics identical.  The final state remains three-state: if
the last pass still reaches a configured safety limit it is still `UNRESOLVED`; no
`timeout -> forbidden` shortcut is introduced.

The extension also enlarges `MAX_STEPS` for the extended pass.  It uses both the requested
time-factor scaling and the observed mean step duration from the preceding trace, with a
25% margin and integer-overflow protection.  This prevents a nominal 600/1200-s time study
from being silently converted into a progressively tighter `STEP_LIMIT` study.  The
normal one-time smaller-step retry for genuine numerical failures remains separate and
continues to be recorded by `retry_count`.

Every new DIRECT_ACCESS row records enough provenance to audit the before/after result:

```text
primary_termination_code
primary_trace_time_s
trace_extension_count
initial_trace_limit_s
final_trace_limit_s
drift_mean_radius_change_Re
```

The first two describe the primary-budget outcome.  `trace_extension_count=0` means no
staged extension was needed; values 1 and 2 identify the 600- and 1200-s C19 passes.
`final_trace_limit_s` records the actual requested budget of the final classifier.  Old
DIRECT_ACCESS files remain readable; absent extension columns are treated as unavailable
provenance rather than guessed historical states.

Post-processing reports detector/spectrum/source weighted convergence rather than only raw
trajectory counts.  The exact `J(E)G(E)` interval integral is split equally between its
two sampled endpoints, exactly as in the existing termination budget.  Per aperture C19
therefore reports

```text
response_primary_trace_limit_fraction
response_resolved_by_trace_extension_fraction
response_unresolved_after_trace_extension_fraction
```

plus the maximum extension pass and final time budget.  These fields appear in
`C19_model.csv`/`C19_comparison.csv`, `C19_aperture_availability.csv`,
`C19_aperture_termination_budget.csv`, and the detailed `C19_aperture_samples.csv`.  The
new `C19_trace_extension_<solver>_<field>.png` plot shows EAST/WEST primary-limit,
resolved-by-extension, and still-unresolved response fractions together with the rigorous
`log10(E/W)` bound width.  It is an isolated plot family, so a failure in this diagnostic
cannot suppress any pre-existing comparison, scatter, transmission, directional-cutoff,
boundary-spectrum, aperture, access-classification, or static-field plot.

The extension is a **classification-convergence tool, not a frozen-field exemption**.
Trajectories lasting longer than `--frozen-field-warning-seconds` still enter the existing
response-weighted frozen-field warning/sensitivity calculation even when the longer pass
physically resolves them.  If a material fraction of detector response still requires
600--1200 s, or remains unresolved at 1200 s, the next physics study should use a
time-dependent/interpolated magnetic background rather than simply increasing the frozen
snapshot duration again.

### Phase 3 -- positive full-orbit drift recurrence for GEO/T05

The original trapped-orbit detector is retained.  Its bounce branch requires repeated
parallel-velocity reversals, several bounce cycles, a stable bounce radial envelope, an
outer-boundary margin, and acceptable momentum-magnitude spread.  That is useful for F3
and ordinary mirroring trajectories, but it can miss near-90-degree-pitch particles at
GEO and can be too restrictive for a strongly non-axisymmetric T05 shell.

C19 therefore adds a second **positive** trapped-orbit resolver.  It deliberately uses
the authoritative full Lorentz trajectory rather than a guiding-centre approximation or
adiabatic invariants: 15--190 MeV proton gyroradii at GEO can be a substantial fraction
of the relevant magnetospheric scale, particularly in the penumbra where the test is
most sensitive.

The drift recurrence algorithm is:

1. Unwrap the particle's signed geocentric azimuth and accumulate the net drift angle.
   Rapid gyromotion that moves azimuth back and forth largely cancels; systematic drift
   accumulates.
2. Divide every complete `2*pi` drift revolution into configurable azimuth-phase bins.
   The default is 24 bins.
3. Within each bin accumulate gyro-insensitive averages of three full-orbit observables:
   geocentric radius `r`, normalized vertical coordinate `z/r`, and
   `cos^2(pitch_angle)`.  The squared pitch cosine removes the sign reversal at mirror
   points while retaining pitch structure.
4. Require each revolution to populate a configured fraction of its phase bins, then
   compare consecutive complete revolutions bin by bin.
5. A radius bin matches when

   ```text
   |r_n-r_(n-1)| <= max(abs_tol, rel_tol * max(|r_n|,|r_(n-1)|))
   ```

   and the same bin must also satisfy the `z/r` and `cos^2(pitch)` tolerances.
6. Require a configured fraction of the common bins to match.  Failed recurrence resets
   the count of consecutive stable revolution comparisons; isolated fortuitous returns
   therefore cannot accumulate over a chaotic trace.
7. Require at least the configured number of complete revolutions, all required
   consecutive turn-to-turn recurrence comparisons, a safe margin from the outer
   boundary, and acceptable momentum-magnitude spread.
8. Only then return `DRIFT_TRAPPED_FORBIDDEN`.

The committed C19 defaults are

```text
TRAP_DETECTION                         T
TRAP_DRIFT_DETECTION                   T
TRAP_MIN_DRIFT_REVOLUTIONS             3
TRAP_DRIFT_RADIAL_GROWTH_TOL_RE        1.0
TRAP_DRIFT_RADIAL_REL_TOL              0.20
TRAP_DRIFT_LATITUDE_TOL                0.20
TRAP_DRIFT_PITCH_COS2_TOL              0.25
TRAP_DRIFT_MAX_MEAN_RADIUS_CHANGE_RE    1.0
TRAP_DRIFT_PROFILE_BINS                24
TRAP_DRIFT_MIN_PROFILE_COVERAGE        0.70
TRAP_DRIFT_MIN_MATCHED_BIN_FRACTION    0.75
TRAP_ENERGY_REL_TOL                     1.0e-4
```

`TRAP_DRIFT_RADIAL_GROWTH_TOL_RE` is retained for input compatibility but is now the
**absolute** radial recurrence tolerance.  The relative tolerance is applied alongside
it, so a T05 shell may have a large day/night excursion and still be recognized if its
azimuth-resolved profile repeats from turn to turn.  This is more meaningful than merely
loosening one global `r_min/r_max` envelope tolerance.

`TRAP_DRIFT_MAX_MEAN_RADIUS_CHANGE_RE` is an additional **secular-drift veto**.  The
mean radius of each completed azimuth profile is computed with equal weight per populated
phase bin, so adaptive full-orbit timesteps cannot overweight one sector.  Consecutive
profiles must satisfy the ordinary detailed recurrence test *and* remain within this mean
radial-change limit.  The gate can only reject a candidate trap classification; it can
never create `DRIFT_TRAPPED_FORBIDDEN`.  Thus it protects against a slowly migrating
quasi-trapped orbit being labelled trapped merely because two detailed profiles happen to
look locally similar.

The momentum gate remains intentionally strict in the committed default.  Phase-0
`momentum_relative_spread` diagnostics must be used to measure the actual RK4 error on
resolved escape, impact, known trapped-dipole, and candidate T05 trajectories before the
tolerance is relaxed.  A BORIS/full-orbit cross-check is preferred when available.

**Critical rule:** neither bounce nor drift recurrence maps a timeout to forbidden.
`TIME_LIMIT`, `STEP_LIMIT`, and `DISTANCE_LIMIT` remain `UNRESOLVED` unless a positive
physical recurrence criterion fired earlier.  This rule is what keeps DIRECT_ACCESS
scientifically distinct from the old equivalent-cutoff midpoint heuristic.

#### Regression protection

Two source-level regressions are included and are compiled/executed by the C19 package
self-test when a C++17 compiler is available:

* `test/C19/tests/test_trap_recurrence.cpp` feeds controlled recurring and secularly
  expanding multi-turn profiles directly to the detector.  It protects the recurrence
  logic from accidental false positives/false negatives.
* `test/C19/tests/test_trap_dipole_full_orbit.cpp` integrates physical relativistic full
  Lorentz orbits in an analytic centered dipole with a self-contained Boris/Cayley step.
  A 15-MeV near-90-degree-pitch GEO proton must establish three-turn drift recurrence; a
  100-MeV radially outward GEO proton must escape without a false trapped verdict.  This
  exercises the detector with an actual orbit rather than hand-made recurrence points.

The analytic-dipole test deliberately carries a tiny local Boris implementation because
the standalone stable `srcEarth` archive does not include the parent AMPS build
configuration needed to link the application mover directly.  These source-level tests
do **not** replace full linked AMPS mover regressions.  Before a production/publication
run, execute the existing DIPOLE/Størmer and F3 tests with the complete AMPS build and
retain full-mover cases for (1) a known escaping orbit, (2) an inner-boundary-loss orbit,
(3) a high-pitch trapped dipole orbit, and (4) a long-lived but ultimately escaping
orbit.  Historical termination codes remain unchanged; the new drift code was appended
specifically to protect archived semantics.

### Phase 3C -- demonstrate where the formerly unresolved response weight went

After the baseline distance/time sweeps, `run_C19_convergence.py` adds a matched case with
`MAX_TRACE_DISTANCE=0`, the selected time budget, and drift recurrence enabled.  The
scientifically convincing diagnostic is not simply that the red X disappears.  It is a
measured transfer such as

```text
before: EAST response dominated by DISTANCE_LIMIT/TIME_LIMIT
 after: EAST response dominated by DRIFT_TRAPPED_FORBIDDEN, residual unresolved < gate
```

while WEST remains physically consistent and the direct E/W bounds converge.  If the
remaining unresolved fraction does not decrease or the result is timestep/mover
sensitive, the case remains inconclusive.

### Phase 4 -- proxy semantics, direct-bound convergence, and scientific status

The cutoff-rigidity proxy remains a **diagnostic reduction**.  For a direct access curve:

1. Fully resolved constant intervals contribute their known blocked area.
2. A finite transition or unresolved bracket contributes the rigorous full `[0,dR]`
   uncertainty to `Rc_lower/Rc_upper`.
3. A **separate plotting-only midpoint** contributes `0.5*dR` for such an uncertain
   bracket so the historical cutoff-proxy diagnostic remains available at every modeled
   epoch.  This midpoint is never stored as `Rc_effective` when unresolved trajectories
   are present and never enters DIRECT_ACCESS acceptance.

The corresponding provenance is
`DIRECT_ACCESS_EQUIVALENT_BLOCKED_AREA_MIDPOINT_DIAGNOSTIC_WITH_BOUNDS`.  A true
`PENUMBRA_SCAN` may still provide its own independently calculated
lower/effective/upper cutoff quantities.

C19 now distinguishes numerical inconclusiveness from an observational disagreement:

```text
INCONCLUSIVE_TRAJECTORY_RESOLUTION   unresolved trajectory weight exceeds the gate
INCONCLUSIVE_DIRECT_BOUND_WIDTH      rigorous direct E/W interval is too wide
MODEL_MISMATCH                       resolved/narrow direct interval excludes observation
VALID                                resolved/narrow direct result is observationally usable
```

The direct ratio is not accepted merely because unresolved weight happens to fall below
5%.  The optional `--max-direct-ratio-bound-width-log10` gate is implemented but defaults
to `-1` (disabled) until a defensible threshold is calibrated from the convergence
sweeps.  When enabled, it is applied to the width of the rigorous `log10(E/W)` interval.
This guards against a small amount of unresolved or finite-grid weight lying exactly
where the detector/spectrum makes the observable highly sensitive.

`C19_result.json` explicitly records

* the number/fraction of observations lying inside available rigorous direct bounds;
* per-case EAST and WEST unresolved fractions;
* their ratio when finite;
* the bounded unresolved asymmetry index

  ```text
  (f_unresolved,E - f_unresolved,W) /
  (f_unresolved,E + f_unresolved,W)
  ```

* response-weighted EAST/WEST distance/time/drift-trapped fractions;
* spectrum-provenance counts.

The unresolved asymmetry is diagnostic only.  EAST >> WEST unresolved weight is
physically suggestive in this event but can never, by itself, make C19 pass.

### Phase 5 -- publication-grade observational hardening

The existing nominal C19 setup remains useful for development and regression, but it is
not silently promoted to publication-grade instrument fidelity.  Three explicit gates
are now available:

```text
--require-real-orientation
--require-independent-spectrum
--require-calibrated-response
```

`--require-real-orientation` rejects the `SM_PROXY` detector attitude and requires
`--detector-orientation-source FILE`.  `--require-independent-spectrum` requires
`--spectrum-source FILE`; it prevents a model-dependent WEST-derived normalization from
entering a publication run.  `--require-calibrated-response` requires every selected
positive response row to declare `calibration_state=CALIBRATED`.

The committed nominal and uncorrected-extended response CSVs now explicitly declare
`calibration_state=NOMINAL_FACTORIZED`.  They therefore continue to work for normal C19
development, but intentionally fail the publication calibration gate.  A calibrated
energy-angle response can be supplied later without weakening the test's provenance
rules.

When publication gates are not requested, the runner still records spectrum provenance
per case:

```text
INDEPENDENT_SPECTRUM
WEST_DERIVED_MODEL_CLEAN
WEST_DERIVED_PARTIALLY_SHIELDED
WEST_DERIVED_UNRESOLVED
FIXED_DIAGNOSTIC
```

A WEST-derived spectrum can therefore be identified as compromised at epochs where the
model says the normalizing WEST head is itself shielded or unresolved.  This flag is
informational in ordinary development mode; an independent spectrum is the preferred
publication path.

### Recommended evidence sequence

Do not enable or tune the new drift classifier first.  The recommended sequence is:

1. Run the Phase-1A distance sweep with drift recurrence off.
2. Run the Phase-1B time sweep with the path cap disabled and drift recurrence off.
3. Confirm that the production choice is insensitive to the numerical path ceiling and
   reasonably converged in trace time.
4. Enable the full-orbit drift recurrence and rerun the matched case.
5. Inspect `C19_aperture_termination_budget.csv`: demonstrate explicitly that the former
   `DISTANCE_LIMIT/TIME_LIMIT` weight becomes a positive physical termination rather than
   simply disappearing.
6. Check direct E/W bound width, timestep sensitivity, and (when supported) RK4/BORIS
   agreement.
7. Run the full DIPOLE/F3 AMPS regressions.
8. Only then interpret `VALID`/`MODEL_MISMATCH` against GOES and, for publication, enable
   the real-attitude/independent-spectrum/calibrated-response gates.

This sequence preserves the central scientific claim of C19: **the measured GOES
East/West asymmetry is compared with a detector-folded, trajectory-resolved AMPS access
calculation.  The cutoff proxy is useful for diagnosis, but it cannot resolve an unknown
trajectory by assumption.**
