# C2 — Dipole longitude and north/south symmetry

C2 checks that the centered aligned dipole calculation does not introduce
spurious longitude dependence or north/south asymmetry.  In the ideal analytical
Størmer problem the vertical cutoff depends only on magnetic latitude and radius:

```text
Rc = R0 cos^4(lambda) / r_RE^2
```

Therefore, on a fixed shell, all longitudes at the same latitude should have the
same cutoff rigidity, and `Rc(+lat, lon)` should match `Rc(-lat, lon)`.

The test can be run in either standalone Mode3D or gridless mode:

```bash
python srcEarth/test/C2/run_C2.py --mode 3d -np 4 -nt 16
python srcEarth/test/C2/run_C2.py --mode gridless -np 4 -nt 16
```

For Mode3D, the script exposes the field-evaluation backend:

```bash
python srcEarth/test/C2/run_C2.py --mode 3d --mode3d-field-eval ANALYTIC
python srcEarth/test/C2/run_C2.py --mode 3d --mode3d-field-eval MESH
```

`ANALYTIC` passes:

```bash
-mode3d-field-eval ANALYTIC
```

and isolates the trajectory pusher, cutoff search, and classifier from mesh
interpolation.  `MESH` validates the mesh-stored Mode3D field path.

The active input syntax is parser-compatible and follows the known-working
CCMC/RoR-style layout.  The shell is specified with:

```text
#OUTPUT_DOMAIN
OUTPUT_MODE            SHELLS
SHELL_COUNT            1
SHELL_ALTS_KM          9000.0
SHELL_RES_DEG          30
```

The validation-plan shorthand `SHELL_LON_DEG` and `SHELL_LAT_DEG` is **not** used
because the parser does not recognize those keywords.  The Python script extracts
and validates the required shell points after AMPS writes the shell output.

The script checks:

```text
latitude set:  -60, -30, 0, +30, +60 deg
longitude set: 0, 30, 60, ..., 330 deg
altitude:      9000 km
```

Common controls:

```bash
-np 4                      # default MPI ranks
-nt 16                     # default threads per rank
--scheduler DYNAMIC        # DYNAMIC, BLOCK_CYCLIC, or STATIC
--dynamic-chunk 0          # 0 = AMPS auto heuristic
--max-trace-time 600       # written to MAX_TRACE_TIME and CUTOFF_MAX_TRAJ_TIME
--max-trace-distance 0     # written to MAX_TRACE_DISTANCE in Re; 0 disables
```

The distance cap is included because finite-time/finite-distance trajectory
classification can affect high-latitude and outer-shell cutoff results.  For C2,
that effect should appear as longitude or north/south asymmetry and will be
reported by the test.

The script writes:

```text
C2_amps.log
C2_longitude_summary.csv
C2_north_south_summary.csv
C2_result.json
C2_symmetry.png                 # if matplotlib is available
```

Reference table:

```text
reference_C2_stormer_symmetry.csv
```

Acceptance logic.  C2 is primarily a symmetry test, not the absolute-cutoff gate.
C1 checks absolute agreement with the Størmer curve.  C2 checks longitude spread
at each latitude and paired north/south differences at every longitude.  Absolute
Størmer residuals are still reported as diagnostics to help identify whether a
symmetry failure is caused by finite-boundary/trace-length effects or a more
basic cutoff error.

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


### Performance of `UPPER_SCAN` for the MESH shell

The production upper-cutoff scan evaluates the rigidity grid from `Rmax`
downward and stops when it brackets the first forbidden sample below the allowed
upper branch.  This avoids tracing all lower-rigidity trapped samples, which are
especially expensive with mesh interpolation.  The logarithmic grid and final
local bisection are unchanged, so the selected `Rc_upper` is the same as a full
grid evaluation; only the order and number of unnecessary traces change.

Because progress is location-based, an older low-to-high scan could appear
frozen at `0.0%` while the first shell point consumed its full time/distance caps.
The descending scan is intended to remove that behavior for the large C2 MESH
case.
