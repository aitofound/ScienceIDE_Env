# F3 — Dipole cutoff-filtered flux

F3 validates the first end-to-end magnetic-access flux calculation in the SEP
validation campaign.  It links the dipole cutoff benchmark to the density/flux
folding path by running gridless density/spectrum in a centered aligned dipole:

```text
FIELD_MODEL               DIPOLE
DS_TRANSMISSION_MODE      SCAN
DS_TRANSMISSION_SCAN_N    100
SPECTRUM_TYPE             POWER_LAW
SPEC_GAMMA                3.5
SPEC_EMIN/SPEC_EMAX       1 / 1000 MeV
```

The validation document describes F3 as a 9000 km shell latitude profile.  The
runner implements this with explicit `POINTS` rather than `SHELLS`, so it can use
the standard `gridless_points_density.dat`, `gridless_points_spectrum.dat`, and
`gridless_points_flux.dat` files.  The default point set is:

```text
alt = 9000 km
lon = 0, 90 deg
lat = -70, -60, -45, -30, 0, 30, 45, 60, 70 deg
```

## Reference solution

For each point, `run_F3.py` computes the vertical Størmer cutoff rigidity

```text
Rc(lambda,r) = 14.9 cos^4(lambda) / r_RE^2 GV
```

then converts that rigidity into a proton kinetic-energy cutoff, `Ecut`.  The
reference flux in each energy channel is

```text
F_ch = 4*pi*T_open*int_{max(E1,Ecut)}^{E2} J0*(E/E0)^(-gamma) dE
```

where `T_open` is the analytic straight-line open-sky fraction outside the inner
absorbing sphere.  This is an approximate external reference: the AMPS result is
computed from a full directional access map and contains penumbra structure,
while the Størmer reference collapses access to a vertical hard cutoff.

The runner also applies tighter internal checks that should be exact apart from
ASCII output precision:

- `J_local(E) = T(E) J_boundary(E)`
- density and flux reconstructed from the saved spectrum match the AMPS files
- centered-dipole longitude symmetry
- centered-dipole north/south symmetry
- total flux increases toward high absolute latitude

## Running

From the directory containing the `amps` executable:

```bash
python srcEarth/test/F3/run_F3.py -np 4
python srcEarth/test/F3/run_F3.py --fast -np 4
python srcEarth/test/F3/run_F3.py --mover BORIS --lons 0 --lats 45 --scan-n 40 --directions-per-energy 1152 -np 18
python srcEarth/test/F3/run_F3.py --mover RK4 --lons 0 --lats 45 --scan-n 40 --directions-per-energy 1152 -np 18
python srcEarth/test/F3/run_F3.py --lons 0,90,180,270
python srcEarth/test/F3/run_F3.py --analytic-flux-tol 0.75 --rc-tol 0.75
python srcEarth/test/F3/run_F3.py --runner-heartbeat-sec 15 --task-heartbeat-sec 15 --slow-task-sec 30
python srcEarth/test/F3/run_F3.py --dry-run --workdir test_output/F3_dryrun
python srcEarth/test/F3/run_F3.py --skip-run --workdir test_output/F3_gridless
```

## Fast regression profile

Use `--fast` for routine regression testing when the full directional-access
calculation is too expensive:

```bash
python srcEarth/test/F3/run_F3.py --fast -np 4
```

The fast profile reduces the dominant traced-trajectory workload by exactly a
factor of sixty:

```text
profile   longitudes   latitudes                         scan N   directions/E   trajectories
full      0,90         -70,-60,-45,-30,0,30,45,60,70     100      1152           2,073,600
fast      0,90         -70,-45,0,45,70                     12       288              34,560
```

The five fast-profile latitudes still provide:

- an equatorial point with the strongest shielding;
- an intermediate-cutoff north/south pair at ±45 degrees;
- a high-latitude north/south pair at ±70 degrees;
- two longitudes for the centered-dipole longitude-symmetry check;
- three absolute-latitude levels for the expected access trend.

The fast profile also uses the existing deterministic `DS_MAX_PARTICLES`
mechanism to retain 288 directions at each rigidity.  The native sky grid is
ordered as 24 zenith levels by 48 azimuths.  Selecting 288 entries therefore
keeps all 24 zenith levels and every fourth azimuth, giving 12 uniformly spaced
azimuths on every zenith ring.  This remains a full-sky directional test rather
than a vertical-only or small-direction shortcut.

The 12 log-rigidity samples and 288-direction sky are deliberately coarse
compared with the full profile, but they remain sufficient for the approximate
Størmer comparison and for detecting major regressions in cutoff filtering,
channel folding, symmetry, or latitude dependence.  Use the full profile for
release validation, angular/energy convergence studies, or investigating a
marginal fast-profile failure.

Explicit `--scan-n`, `--directions-per-energy`, `--lats`, or `--lons` values
override the corresponding `--fast` defaults.  The runner converts the angular
request to `DS_MAX_PARTICLES = scan_n * directions_per_energy`, prints the
resolved profile, trajectory count, and effective speedup before launching
AMPS, and stores the resolved values in the generated input comments and
`F3_result.json`.

F3 gridless tracing is intentionally MPI-only.  The generated input sets
`MODE3D_PARALLEL SERIAL` and one computational thread per MPI rank.  This avoids
concurrent calls into Geopack/Tsyganenko implementations that may use
thread-unsafe Fortran COMMON-block state.  The legacy `-nt` runner option is
accepted for command-line compatibility, but values other than one are ignored
with an explicit diagnostic.  Increase `-np` to allocate more parallel workers.

## Particle mover and adaptive time step

Use `--mover` to select the shared gridless backtracing integrator explicitly:

```text
BORIS  HC4  RK2  RK4  RK6  GC2  GC4  GC6  HYBRID
```

For example:

```bash
python srcEarth/test/F3/run_F3.py --mover BORIS --fast -np 18
python srcEarth/test/F3/run_F3.py --mover RK4 --lons 0 --lats 45 \
  --scan-n 40 --directions-per-energy 1152 -np 18
```

When `--mover` is omitted, the runner does not pass `-mover` and preserves the
AMPS executable's existing default-selection behavior.  The selected value is
printed before launch, written to the generated `AMPS_PARAM_F3.in` comments, and
stored in `F3_result.json`.

The adaptive selector treats `DT_TRACE`, the gyro-angle limit, and the remaining
trace time as **upper bounds**.  The former `100 km / v` minimum-displacement floor
incorrectly increased low-energy full-orbit steps and has been removed.  The former
`0.2*distance_to_boundary/v` upper bound has also been removed: it approached a
boundary geometrically and could prevent the endpoint from ever crossing it.

Every accepted mover step is now checked with an exact intersection of the numerical
trajectory chord against the inner absorbing sphere and the six faces of the outer
Cartesian box.  The first event along the chord determines the physical result.  A
valid small time step is never increased, and an invalid/non-positive step returns an
explicit `INVALID_TIME_STEP` termination instead of continuing with an arbitrary
fallback.  Gridless and Mode3D use the same event policy.


## Boundary events and unresolved trajectories

The production tracer reports one of the following explicit outcomes:

```text
OUTER_BOUNDARY_ALLOWED
INNER_BOUNDARY_FORBIDDEN
MAGNETICALLY_TRAPPED_FORBIDDEN
TIME_LIMIT
STEP_LIMIT
DISTANCE_LIMIT
INVALID_TIME_STEP
INVALID_FIELD
NUMERICAL_FAILURE
```

Outer escape, inner-sphere impact, and conservatively detected magnetic trapping
are resolved physical classifications.  Inner impact and trapping are both forbidden.
All numerical-limit outcomes remain unresolved.  The density/transmission solver defines

```text
N_forbidden = N_inner_forbidden + N_trapped
T_resolved = N_allowed / (N_allowed + N_forbidden)
unresolved_fraction = N_unresolved / N_sampled
```

and never counts an unresolved trajectory as physically forbidden.  With
`--retry-unresolved`, each unresolved direction is repeated once with half
`DT_TRACE`, twice the step limit, and twice the time limit.  The final result and
number of retried directions are saved in:

```text
gridless_termination_summary.dat
```

The file contains one row per point and energy with `N_sampled`, `N_retried`,
`N_inner_forbidden`, `N_trapped`, every numerical termination category, `T_resolved`,
`T_all`, and `unresolved_fraction`.  F3 requires
all count identities to close and fails when any point/energy unresolved fraction
exceeds `--unresolved-tol` (default 0.01).

Useful controls are:

```text
--max-steps N
--max-trace-time S
--boundary-refine-tol-m M
--trap-detection | --no-trap-detection
--trap-min-mirror-points N
--trap-min-bounces N
--trap-outer-margin-re R
--trap-radial-growth-tol-re R
--trap-energy-rel-tol F
--trap-parallel-deadband F
--unresolved-tol F
--retry-unresolved
--no-save-termination-summary
```

`--boundary-refine-tol-m` sets the geometric tolerance used by the exact chord
intersection and by the inward field-evaluation offset at an outer crossing.  The
`BOUNDARY_EVENT_MAX_ITER` input is retained as a reserved compatibility control;
the current line/sphere and line/box intersections are analytic and do not require
an iterative solve.


### Static-field trapped-orbit classification

F3 enables conservative trapping detection because its centered dipole is static.  A
trajectory is classified as `MAGNETICALLY_TRAPPED_FORBIDDEN` only after all of the
following are satisfied:

- repeated sign changes of `p_parallel` provide at least the requested mirror points;
- at least the requested number of complete bounce cycles is observed;
- the radial envelope is stable across the last two cycle comparisons;
- the entire observed orbit remains the requested distance inside the outer box; and
- relative momentum-magnitude variation stays below the configured tolerance.

Elapsed time by itself never proves trapping.  The feature is disabled by default in the
core parser and should remain disabled for time-dependent fields unless a separate
validated policy is provided.  The F3 runner enables it by default and provides
`--no-trap-detection` for convergence and diagnostic comparisons.

The lightweight geometry/status unit test can be run without AMPS:

```bash
python srcEarth/test/F3/run_boundary_unit_test.py
```

## Compatibility implementation: F3 accuracy without breaking cutoff regressions

F3 and the older C-series cutoff tests intentionally do not use the same final
Boolean interpretation, even though they share field models, movers, geometry,
and most of the trajectory kernel.  This distinction is required because F3
must quantify unresolved sampling, while a cutoff search must provide a Boolean
classification at every trial rigidity.

### Structured F3 call path

The F3 density/transmission calculation calls the structured interfaces:

```cpp
Earth::GridlessMode::TraceTrajectoryShared(...)
Earth::GridlessMode::TraceTrajectorySharedEx(...)
```

Mode3D density/diagnostic calculations use the corresponding:

```cpp
Earth::Mode3D::TraceTrajectoryMesh(...)
```

These calls select the internal `StructuredAccurate` integration policy and
return a complete `TrajectoryResult`.  They do not fold trace limits into
forbidden.  The result includes the termination code, elapsed time, cumulative
path length, step count, retry count, trap-detector counters, momentum spread,
and optionally the outer-boundary exit state.

For F3, the categories are accumulated as:

```text
N_allowed    = OUTER_BOUNDARY_ALLOWED
N_forbidden  = INNER_BOUNDARY_FORBIDDEN + MAGNETICALLY_TRAPPED_FORBIDDEN
N_unresolved = TIME_LIMIT + STEP_LIMIT + DISTANCE_LIMIT
             + INVALID_TIME_STEP + INVALID_FIELD + NUMERICAL_FAILURE
```

The physical transmission is then:

```text
T_resolved = N_allowed / (N_allowed + N_forbidden)
```

and the independently reported unresolved fraction is:

```text
unresolved_fraction = N_unresolved / N_sampled
```

This prevents a user-selected numerical cap from masquerading as a physical
loss mechanism.

### Cutoff compatibility call path

The C-series cutoff searches call Boolean interfaces, including the internal
Mode3D/gridless cutoff classifiers and the exported `TraceAllowed*` wrappers.
Those calls select `LegacyCutoffCompatible` integration and interpret:

```text
INNER_BOUNDARY_FORBIDDEN       -> false
MAGNETICALLY_TRAPPED_FORBIDDEN -> false
TIME_LIMIT                     -> false
STEP_LIMIT                     -> false
DISTANCE_LIMIT                 -> false
OUTER_BOUNDARY_ALLOWED         -> true
```

The limit mapping restores the historical cutoff meaning: the particle was not
shown to have access before the configured safety cap.  It does **not** modify
the structured result returned to F3.

Only `INVALID_TIME_STEP`, `INVALID_FIELD`, and `NUMERICAL_FAILURE` are retried by
the Boolean wrappers.  A limit is not retried, because the limit itself is the
configured classification rule and retrying can double the cost without changing
the answer.  If a genuine numerical failure survives the stricter retry, the
calculation still terminates with a diagnostic rather than silently treating a
broken integration as forbidden.

### Why two time-step policies are retained

The F3 corrections changed two historical cutoff stepping behaviors:

1. The `100 km / v` minimum-displacement floor was removed because it could
   increase a timestep that had already been reduced by the gyro-angle accuracy
   requirement.
2. The `0.2 * distance_to_boundary / v` limiter was removed because repeated
   proportional shortening can approach a boundary asymptotically.

Those changes are correct for a resolved density/transmission calculation, but
they also change the detailed Boolean penumbra sampled by old cutoff reference
runs.  The implementation therefore keeps both policies explicitly:

| Detail | F3 `StructuredAccurate` | Cutoff `LegacyCutoffCompatible` |
|---|---|---|
| `DT_TRACE` | Upper bound | Base value, subject to legacy controls |
| Gyro-angle limiter | Upper bound | Applied |
| Remaining trace time | Upper bound | Applied |
| `100 km / v` floor | Removed | Retained |
| `0.2*d_boundary/v` limiter | Removed | Retained |
| Boundary decision | First analytic chord event | Legacy cutoff endpoint path |
| Trace limit | Unresolved | Boolean forbidden |
| Retry on trace limit | Optional F3 accounting retry | No |
| Retry on numerical failure | Reported by structured workflow | One stricter retry |

The policy split is inside the C++ tracer, not in the test harness.  A production
cutoff run and a C-series test therefore exercise the same behavior.

### Exact boundary processing used by F3

For `StructuredAccurate`, every accepted numerical step supplies a segment
`x_before -> x_after`.  The boundary utility computes the first intersection
fraction along that segment with:

- the inner absorbing sphere; and
- the six faces of the outer Cartesian box.

The earliest valid event wins.  This matters when one step is long enough to
cross a surface and end beyond it, or in a pathological case where a segment
could intersect more than one surface.  An outer event can also populate the
interpolated exit state needed by anisotropic boundary spectra.

`BOUNDARY_EVENT_MAX_ITER` remains accepted for input compatibility, but the
current line/sphere and line/box intersections are analytic; they do not use an
iterative root solve.  `BOUNDARY_REFINE_TOL_M` controls geometric tolerances and
the inward offset used when evaluating the field at an outer exit.

### Finite limits remain useful

F3 does not require all safety caps to be disabled.  They protect a production
run from an unexpectedly expensive orbit and quantify how much of the sampled
phase space is unresolved.  In particular:

```text
MAX_TRACE_TIME      limits elapsed integration time
MAX_STEPS           limits accepted mover steps
MAX_TRACE_DISTANCE  limits cumulative traveled path in Re
```

`MAX_TRACE_DISTANCE 0.0` disables only the cumulative path-length cap.  It does
not disable time or step limits.  A trapped particle can accumulate a large path
length while remaining spatially close to Earth, so path length must not be
confused with radial distance.

For F3, a cap should be selected together with `--unresolved-tol`.  A run passes
only when its unresolved fraction is acceptably small; the code does not obtain a
clean transmission merely by relabeling capped trajectories as forbidden.

### Trap detection is an accelerator and physical classifier, not a fallback

The static-dipole trap detector can terminate a confidently trapped orbit before
it consumes a time or distance cap.  Its decision requires multiple independent
conditions: mirror-point count, complete bounce cycles, stable radial envelopes,
outer-boundary margin, and momentum conservation.  A trajectory that does not
meet all conditions remains unresolved if it later reaches a numerical cap.

The Boolean cutoff path can also use the detector, but cutoff compatibility does
not depend on it.  A trapped orbit that is not detected still reaches a finite
cap and returns `false` rather than aborting the scan.

### `UPPER_SCAN` optimization used by the cutoff tests

F3 does not use the scalar cutoff `UPPER_SCAN` for every directional transmission
sample; nevertheless, the F3-compatible source changes must coexist with the
C3/C11 upper-cutoff algorithm and large C2 shell runs.

The coarse rigidity grid is now traversed from `Rmax` downward.  Once the first
forbidden sample is encountered, the immediately higher sample is the allowed
side of the highest transition, and only that pair is bisected.  Lower samples
cannot change `Rc_upper`.  This avoids spending most of a MESH run on trapped
low-rigidity trajectories while the location progress remains at zero.

The descending traversal uses the same logarithmic grid and returns the same
upper bracket as the prior full-grid evaluation.  It changes evaluation order and
cost, not the mathematical definition of the upper cutoff.

### Files and tests covering the implementation

Core implementation:

```text
srcEarth/util/TrajectoryTermination.h
srcEarth/gridless/CutoffRigidityGridless.cpp
srcEarth/gridless/CutoffRigidityGridless.h
srcEarth/3d/CutoffRigidityMode3D.cpp
srcEarth/3d/CutoffRigidityMode3D.h
```

Primary validation:

```text
F3   structured termination accounting and flux folding
C1   absolute dipole cutoff
C2   longitude and north/south symmetry, including MESH
C3   upper-cutoff/penumbra regression
C11  BINARY collapse diagnostic and UPPER_SCAN recovery
```

The standalone F3 unit test verifies the geometry/status primitives without a
full AMPS build.  The full regression suite is still required to validate the
compiled executable, MPI/thread configuration, field backend, and generated
reference products.

## Progress and hang diagnostics

F3 is substantially more expensive than F1/F2 because every energy/rigidity
sample requires a directional access calculation.  With the default 18 points,
100 scan samples, and the 24 x 48 direction grid, the run evaluates approximately
2,073,600 trajectories.  The runner prints this resolved workload estimate before
launching AMPS so an unintentionally large point or scan configuration is visible
immediately.

AMPS output is streamed live to the terminal while also being retained in
`F3_amps.log`.  If AMPS produces no output for the configured interval, the runner
prints a heartbeat containing elapsed time, process ID, log size, the last AMPS
line, and the status of the three expected Tecplot output files.

The gridless MPI driver also supports F3-specific per-rank diagnostics controlled
by the runner:

```text
--runner-heartbeat-sec N   Runner message after N seconds without AMPS output.
--task-heartbeat-sec N     Each MPI rank periodically identifies the task it is
                           about to process: point, energy, and direction block.
--slow-task-sec N          Report a completed MPI task whose wall time is at least N.
```

The default values are 30, 30, and 60 seconds, respectively.  Set any value to
zero to disable that diagnostic.  For example, when diagnosing an apparent stall:

```bash
python srcEarth/test/F3/run_F3.py -np 4 \
  --runner-heartbeat-sec 10 --task-heartbeat-sec 10 --slow-task-sec 20
```

A task-heartbeat line is written *before* the corresponding trajectory block is
started.  Therefore, if one block takes an unusually long time, the last line from
that MPI rank identifies the point index, energy index and value, direction-block
index, and exact direction-index range.  A later `slow-task` line confirms the wall
time if the block eventually completes.

## Files

Repository files:

```text
AMPS_PARAM_F3_gridless.in
reference_F3_dipole_cutoff.csv
run_F3.py
run_boundary_unit_test.py
test_trajectory_boundary.cpp
README.md
```

Run-directory outputs:

```text
AMPS_PARAM_F3.in
F3_amps.log
F3_summary.csv
F3_result.json
reference_F3_dipole_cutoff_used.csv
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
gridless_termination_summary.dat
```
