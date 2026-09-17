# C3 — Penumbra, Rc_upper, and UPPER_SCAN regression

C3 validates the cutoff-search algorithm at a high-latitude outer-shell dipole
point where allowed/forbidden rigidity access can become non-monotonic and where
legacy endpoint binary search can return the lower search boundary instead of the
upper cutoff.

The primary target point is:

```text
lon = 0 deg
lat = -60 deg
alt = 9000 km
FIELD_MODEL = DIPOLE
CUTOFF_SAMPLING = VERTICAL
```

The analytical vertical Størmer reference is approximately:

```text
Rc = 0.159984 GV
```

The input uses `CUTOFF_EMIN = 1 MeV/n`, which corresponds to:

```text
Rmin ~= 0.043331 GV for protons
```

A result near `Rmin` is therefore a lower-boundary collapse or low-rigidity
leakage signature, not the correct upper cutoff.

## Files

```text
run_C3.py                         Python test harness
AMPS_PARAM_C3.in                  backward-compatible Mode3D alias
AMPS_PARAM_C3_mode3d.in           parser-compatible Mode3D template
AMPS_PARAM_C3_gridless.in         parser-compatible gridless template
reference_C3_penumbra.csv         analytical Størmer/Rmin reference values
```

The input templates intentionally follow the CCMC/RoR-style parser-compatible
layout used by the working C1 input.  Newer validation controls are passed on the
command line by `run_C3.py` rather than activated as input keywords.

## Basic commands

Run the default Mode3D analytical-field case:

```bash
python srcEarth/test/C3/run_C3.py -np 4 -nt 16
```

Run Mode3D with mesh-backed field interpolation:

```bash
python srcEarth/test/C3/run_C3.py --mode 3d --mode3d-field-eval MESH -np 4 -nt 16
```

Run gridless:

```bash
python srcEarth/test/C3/run_C3.py --mode gridless -np 4 -nt 16
```

Run only the production algorithm:

```bash
python srcEarth/test/C3/run_C3.py --algorithms UPPER_SCAN --cutoff-scan-n 200
```

## AMPS commands issued by the harness

For Mode3D, the script launches commands of this form:

```bash
mpirun -np 4 ./amps -mode 3d -i AMPS_PARAM_C3.in \
  -cutoff-search UPPER_SCAN -cutoff-upper-scan-n 200 \
  -mode3d-field-eval ANALYTIC \
  -mode3d-parallel THREADS -mode3d-threads 16 \
  -mode3d-mpi-scheduler DYNAMIC -mode3d-mpi-dynamic-chunk 0
```

For gridless:

```bash
mpirun -np 4 ./amps -mode gridless -i AMPS_PARAM_C3.in \
  -cutoff-search UPPER_SCAN -cutoff-upper-scan-n 200 \
  -gridless-mpi-scheduler DYNAMIC -gridless-mpi-dynamic-chunk 0 \
  -density-parallel THREADS -density-threads 16
```

## Trace caps

The default C3 input uses:

```text
DT_TRACE               1.0
MAX_TRACE_TIME         600
MAX_TRACE_DISTANCE     300.0
CUTOFF_MAX_TRAJ_TIME   600
```

`MAX_TRACE_DISTANCE` is active because this test is intentionally sensitive to
long-time leakage of quasi-trapped low-rigidity trajectories.  To study the cap
sensitivity, use for example:

```bash
python srcEarth/test/C3/run_C3.py --mode gridless --max-trace-distance 100
python srcEarth/test/C3/run_C3.py --mode gridless --max-trace-distance 300
python srcEarth/test/C3/run_C3.py --mode gridless --max-trace-distance 600
python srcEarth/test/C3/run_C3.py --mode gridless --max-trace-distance 0
```

A good C3 configuration should avoid the low-boundary collapse at `lat=±60` while
not forcing clearly allowed trajectories to be stopped too early.

## Acceptance criteria

By default, PASS/FAIL is applied only to `UPPER_SCAN`:

```text
1. UPPER_SCAN must not return a value near Rmin.
2. UPPER_SCAN must be within --upper-rel-tol of the analytical Størmer value
   at the target high-latitude points.  The default tolerance is 0.25 because
   this point is intentionally sensitive to finite-time and finite-distance
   classification.
```

The `BINARY` run is diagnostic.  It is allowed to pass or fail unless
`--require-binary-collapse` is set.  This avoids making the validation harness
fail when the legacy BINARY path is later improved or removed.

## Outputs

The root C3 run directory contains:

```text
C3_summary.csv
C3_result.json
C3_penumbra_comparison.png     # if matplotlib is available
reference_C3_penumbra.csv
binary/                        # BINARY AMPS run directory, if requested
upper_scan/                    # UPPER_SCAN AMPS run directory, if requested
```

Each algorithm subdirectory contains its own generated `AMPS_PARAM_C3.in`, AMPS
log, and AMPS cutoff output.

## Compatibility with the F3 structured tracer

This cutoff test intentionally uses the Boolean cutoff contract, not F3's
structured density/transmission contract.  The underlying source now has two
explicit internal policies:

```text
StructuredAccurate       structured F3/density trajectories
LegacyCutoffCompatible   Boolean cutoff searches
```

For this test, inner impact, validated trapping, and configured
`TIME_LIMIT`/`STEP_LIMIT`/`DISTANCE_LIMIT` outcomes all classify the trial
rigidity as forbidden (`false`).  A configured limit is not retried.  Only an
invalid step, invalid field, or numerical integration failure receives one
stricter retry and can abort the run if it remains unresolved.

This preserves the finite-cap behavior used to generate the pre-F3 cutoff
references while allowing F3 to retain the same limit outcomes as explicit
unresolved samples.  The distinction is implemented in the C++ Boolean versus
structured APIs; the test runner does not relabel output after the calculation.


### Descending `UPPER_SCAN` definition

`UPPER_SCAN` first requires `Rmax` to be allowed.  It then scans the logarithmic
rigidity grid downward.  The first forbidden sample and the immediately higher
allowed sample bracket the final upper transition; only that interval is
bisected.  If every grid sample is allowed, the routine returns `Rmin`.  If
`Rmax` is forbidden, it returns the legacy negative sentinel indicating that no
allowed upper branch was found inside the search interval.

This is penumbra-safe because it does not assume monotonic access over the full
interval, and it is faster than evaluating all low-rigidity samples after the
upper transition is already known.
