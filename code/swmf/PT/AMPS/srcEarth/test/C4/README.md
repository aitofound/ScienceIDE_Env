# C4 — Mode3D trajectory-exit classifier and invariant diagnostic

C4 is a low-level trajectory-quality test for the Mode3D backward-tracing kernel.  It is not a global cutoff-map validation test.  C1, C2, and C3 compare cutoff values against the analytical Størmer result and exercise the penumbra-aware cutoff search.  C4 instead asks whether individual trajectories are integrated and classified in a physically credible way before the same mover is trusted by cutoff, density, spectrum, or flux products.

## What C4 tests

For each selected start location and rigidity, the test launches the normal AMPS Mode3D vertical backtrace and saves the detailed terminal state.  The Python harness then checks three things:

1. **Trajectory-exit classification.**  A trajectory with rigidity above the analytical vertical Størmer cutoff should escape through the Mode3D outer box and be reported as `allowed=1` with `reason=OUTER_BOX`.  A trajectory below the cutoff should not be counted as allowed; acceptable forbidden outcomes include inner-sphere loss, time limit, step limit, distance limit, or another non-outer-boundary terminal reason.

2. **Rigidity/energy conservation.**  In the C4 input file the field is a static centered dipole and the electric field is off.  The magnetic force should not change kinetic energy, so the diagnostic records `rel_dR`.  The default hard tolerance is `--dR-tol=1e-6`.

3. **Canonical dipole-axis momentum diagnostic.**  For an aligned centered dipole, the canonical angular momentum about the dipole axis should be approximately conserved.  The diagnostic records `rel_dP_axis`.  This is recorded by default and can be made a hard failure with `--fail-on-paxis --paxis-tol=1e-5`.

The purpose is to catch problems such as incorrect terminal-state classification, artificial energy drift, bad boundary crossing logic, or an inconsistent pusher before those issues are hidden inside a shell map or a density/flux integral.

## Single-run many-trajectory diagnostic

Older versions of the C4 harness restarted AMPS once for every trajectory because the debug-exit interface accepted only one `(lon, lat, alt, R)` case at a time.  The current implementation fixes that by using:

```text
CUTOFF_DEBUG_EXIT_TRACE      T
CUTOFF_DEBUG_EXIT_LIST_FILE  c4_debug_trajectories.dat
CUTOFF_DEBUG_EXIT_FILE       cutoff_3d_debug_exit_trace.dat
```

The list file contains all trajectories to trace in one AMPS run:

```text
# lon_deg lat_deg alt_km R_GV label
0.0 -60.0 9000.0 8.0000e-02 low_latm60
0.0 -60.0 9000.0 3.2000e-01 high_latm60
0.0   0.0 9000.0 1.2800e+00 low_lat0
0.0   0.0 9000.0 5.1200e+00 high_lat0
```

The output is one combined Tecplot-style ASCII file, `cutoff_3d_debug_exit_trace.dat`.  AMPS rank 0 writes this diagnostic before the normal Mode3D MPI location scheduler starts.  Therefore the diagnostic output remains a single file even when AMPS is launched with multiple MPI processes and multiple Mode3D worker threads.

## Running the test

Run from the directory containing the `amps` executable:

```bash
srcEarth/test/C4/run_C4.py -np 4 -nt 16
```

Recommended explicit command:

```bash
srcEarth/test/C4/run_C4.py \
  --factors=0.5,2.0 \
  --lats=-60,-30,0,30,60 \
  --lons=0 \
  --alt=9000 \
  -np 4 \
  -nt 16
```

The harness also accepts the form without `=` for negative latitude lists, for example:

```bash
srcEarth/test/C4/run_C4.py --factors 0.5,2.0 --lats -60,-30,0,30,60
```

The script normalizes that command before calling `argparse` so `-60,-30,...` is not mistaken for an option.

## Important options

```text
--lats             comma-separated start latitudes, default -60,-30,0,30,60
--lons             comma-separated start longitudes, default 0
--factors          rigidities as factors times Rc_Stormer, default 0.5,2.0
--dt-sweep         DT_TRACE values; each value is one AMPS run, default 0.25
--movers           mover list; each mover is one AMPS run, default BORIS; HC4 is accepted
--adaptive-dt      T or F, default T
--dR-tol           hard tolerance on |rel_dR|, default 1e-6
--fail-on-paxis    make |rel_dP_axis| a hard pass/fail quantity
--paxis-tol        tolerance used with --fail-on-paxis, default 1e-5
```


Useful HC4 C4 check:

```bash
srcEarth/test/C4/run_C4.py --movers=HC4,RK4,BORIS --adaptive-dt T
```

HC4 should be interpreted as the high-accuracy full-orbit Boris-family mover: it
uses a fourth-order Yoshida/Forest-Ruth composition of a symmetric
Higuera-Cary/Boris magnetic midpoint step.  In the C4 E=0 dipole diagnostic it
should conserve rigidity like Boris while reducing smooth-orbit phase error toward
RK4-like accuracy.

A command with one mover and one `DT_TRACE` value launches AMPS once and traces all requested trajectories inside that single run.  A mover sweep or a `DT_TRACE` sweep still launches one AMPS run per configuration because the mover and timestep are global run settings.

## Outputs

For each mover/timestep case, the run directory contains:

```text
AMPS_PARAM_C4.in                    rendered AMPS input file
c4_debug_trajectories.dat           many-trajectory input list for AMPS
cutoff_3d_debug_exit_trace.dat      one combined AMPS diagnostic output
C4_expected_cases.csv               expected cases generated by the harness
C4_amps.log                         AMPS stdout/stderr log
```

The top-level C4 work directory contains:

```text
C4_summary.csv       one row per trajectory with pass/fail checks
C4_case_metrics.csv  one row per mover/timestep configuration
C4_result.json       machine-readable test result
```

## Interpreting failures

- `expected OUTER_BOX but got ...`: the high-rigidity trajectory did not escape as expected.  This can indicate a pusher problem, a boundary configuration issue, too small a maximum trace time/distance, or a field-evaluation problem.

- `expected not OUTER_BOX but got OUTER_BOX`: the low-rigidity trajectory was classified as allowed.  This may indicate a true penumbral allowed island, but for the default factor `0.5` it is usually a useful warning that the chosen case is not a clean below-cutoff trajectory.  Try a smaller factor such as `--factors=0.25,2.0` if this occurs systematically.

- `|rel_dR| > tolerance`: the trajectory is gaining or losing rigidity in a static magnetic field.  Try `--adaptive-dt F --dt-sweep=1.0,0.5,0.25,0.125` to separate fixed-step pusher convergence from classifier effects.

- large `rel_dP_axis`: the dipole-axis canonical invariant is drifting.  This is not a hard failure unless `--fail-on-paxis` is set, but it is useful for comparing movers and timestep settings.

## What C4 does not test

C4 does not validate the global cutoff map, longitude symmetry, mesh interpolation convergence, or realistic IGRF/Tsyganenko morphology.  Those are covered by other validation tests.  C4 is intentionally narrower: it verifies that a single backtraced trajectory terminates for the right reason and conserves the quantities it should conserve in the clean dipole, `E=0` limit.
