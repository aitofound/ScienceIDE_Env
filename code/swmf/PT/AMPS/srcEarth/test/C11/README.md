# C11 — Penumbra-pocket regression: BINARY Rmin-collapse defect

C11 isolates the cutoff-search defect that motivated the penumbra-safe
`UPPER_SCAN` algorithm.  At a high-latitude dipole-shell point, the allowed/
forbidden access sequence can contain a low-rigidity allowed pocket followed by a
forbidden island and then the true high-rigidity allowed region.  The legacy
endpoint-only `BINARY` search can therefore return the lower search boundary
`Rmin`, while `UPPER_SCAN` should return the final upper cutoff.

The primary target is:

```text
lon = 0 deg
lat = -60 deg
alt = 9000 km
FIELD_MODEL = DIPOLE
CUTOFF_SAMPLING = VERTICAL
CUTOFF_EMIN = 0.05 MeV/n
```

For protons, `CUTOFF_EMIN = 0.05 MeV/n` corresponds to:

```text
Rmin ~= 9.6866e-3 GV
```

The analytical vertical Størmer reference at the primary point is:

```text
Rc ~= 0.159972 GV
```

So a returned cutoff near `Rmin` is the expected legacy BINARY-collapse
signature, not the physical upper cutoff.

## Files

```text
run_C11.py
AMPS_PARAM_C11.in
AMPS_PARAM_C11_mode3d.in
reference_C11_penumbra_pocket.csv
```

`AMPS_PARAM_C11.in` is a backward-compatible alias for the Mode3D template.
The test is Mode3D-only because the single-point `CUTOFF_DEBUG_RIGIDITY_SCAN`
diagnostic is implemented in the Mode3D cutoff path.

## Basic commands

Run the default test, which launches both BINARY and UPPER_SCAN:

```bash
python srcEarth/test/C11/run_C11.py -np 4 -nt 16
```

Run only the production search:

```bash
python srcEarth/test/C11/run_C11.py --algorithms UPPER_SCAN --cutoff-scan-n 200
```

Run with mesh-backed Mode3D field interpolation:

```bash
python srcEarth/test/C11/run_C11.py --mode3d-field-eval MESH -np 4 -nt 16
```

Render inputs and print the AMPS commands without executing:

```bash
python srcEarth/test/C11/run_C11.py --dry-run
```

Analyze an existing run tree:

```bash
python srcEarth/test/C11/run_C11.py --skip-run --workdir test_output/C11_mode3d
```

## AMPS commands issued by the harness

For each algorithm, the script launches a command of this form:

```bash
mpirun -np 4 ./amps -mode 3d -i AMPS_PARAM_C11.in \
  -cutoff-search UPPER_SCAN -cutoff-upper-scan-n 200 \
  -mode3d-field-eval ANALYTIC \
  -mode3d-parallel THREADS -mode3d-threads 16 \
  -mode3d-mpi-scheduler DYNAMIC -mode3d-mpi-dynamic-chunk 0
```

The BINARY diagnostic run uses the same command with:

```text
-cutoff-search BINARY
```

## Debug scan

The input enables:

```text
CUTOFF_DEBUG_RIGIDITY_SCAN T
CUTOFF_DEBUG_SCAN_LON      0.0
CUTOFF_DEBUG_SCAN_LAT     -60.0
CUTOFF_DEBUG_SCAN_ALT    9000.0
CUTOFF_DEBUG_SCAN_N       200
```

Each algorithm directory receives a debug file named:

```text
C11_binary_debug_rigidity_scan.dat
C11_upper_scan_debug_rigidity_scan.dat
```

The debug file contains `Rc_selected_GV`, `Rc_endpoint_binary_GV`,
`Rc_upper_scan_GV`, and `Rc_stormer_GV`.  C11 requires
`Rc_endpoint_binary_GV` to be near `Rmin` and `Rc_upper_scan_GV` to be close to
the Størmer value.

## Acceptance criteria

By default C11 requires:

```text
1. BINARY reproduces the Rmin-collapse signature at the primary point.
2. UPPER_SCAN does not collapse to Rmin.
3. UPPER_SCAN is within --upper-rel-tol of the analytical Størmer cutoff.
4. The Mode3D debug scan independently reports the same endpoint-BINARY and
   UPPER_SCAN behavior at the primary point.
```

The shell output is also checked at `lat=+60 deg` and at altitudes `500 km` and
`9000 km`.  These repeat points are diagnostics by default for BINARY.  To require
BINARY collapse at every target point, use:

```bash
python srcEarth/test/C11/run_C11.py --require-binary-collapse-all
```

## Outputs

The top-level C11 run directory contains:

```text
C11_summary.csv
C11_result.json
C11_binary_vs_upper_scan.png       # if matplotlib is available
reference_C11_penumbra_pocket_generated.csv
binary/
upper_scan/
```

Each algorithm subdirectory contains its rendered `AMPS_PARAM_C11.in`, AMPS log,
shell cutoff output, and C11 debug scan file.

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


### Interaction with the BINARY diagnostic

The compatibility policy applies equally to the BINARY and UPPER_SCAN trial
trajectories; therefore, differences reported by C11 remain differences in the
search algorithm rather than differences in trajectory termination semantics.
BINARY retains its endpoint-monotonicity defect for the diagnostic run.
UPPER_SCAN scans from `Rmax` downward, brackets the final upper transition, and
refines only that bracket.
