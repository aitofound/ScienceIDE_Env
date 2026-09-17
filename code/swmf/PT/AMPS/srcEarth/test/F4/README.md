# F4 — Transmission reconstruction consistency

F4 validates the internal consistency of the gridless SEP density, integral-flux,
and spectrum outputs. It uses a centered aligned dipole,
`DS_TRANSMISSION_MODE SCAN`, and a power-law boundary spectrum at diagnostic
points on the 9000 km shell.

This is a **closure test**, not a high-resolution production calculation and not
an independent validation of the physical transmission function. It checks the
following identities using values written by AMPS:

```text
J_local(E) = T(E) J_boundary(E)
n          = 4π ∫ J_local(E) / v(E) dE
F          = 4π ∫ J_local(E) dE
F_channel  = 4π ∫_channel J_local(E) dE
```

The default points are at longitude 0 and latitudes
`-60, -30, 0, 30, 60 deg`; this includes low-latitude/high-shielding and
higher-latitude/more-open cases.

## Why the original F4 configuration appeared frozen

The original defaults were accidentally closer to a production density run than
a functional closure test:

```text
5 points
120 rigidity/energy samples
24 × 48 = 1152 directions per energy
DS_MAX_PARTICLES = 0                 (no cap)
DS_MAX_TRAJ_TIME = 900 s
MAX_STEPS = 300000
MAX_TRACE_DISTANCE = 0               (disabled)
```

The resulting nominal workload was

```text
5 × 120 × 1152 = 691,200 trajectories
```

Many low-energy dipole trajectories are trapped or near the penumbra and can run
to a time or step limit. Therefore, the actual work can be extremely large.

There were two additional reasons the run looked frozen:

1. The Python runner redirected all AMPS output, including the C++ progress bar,
   exclusively to `F4_amps.log`. Nothing from AMPS was shown in the terminal.
2. With more than one MPI rank, the current gridless collective scheduler
   distributes direction-block tasks among MPI ranks. The `-nt` value does not
   multiply that task concurrency. Thus `-np 4 -nt 16` does not provide 64-way
   trajectory execution in this path.

## Functional-test defaults

F4 now uses bounded defaults appropriate for its actual validation objective:

```text
DS_TRANSMISSION_SCAN_N  32
DS_NINTERVALS           31
DS_MAX_PARTICLES        512       # per point, across all energies
DS_MAX_TRAJ_TIME        120 s
MAX_TRACE_TIME          120 s
MAX_TRACE_DISTANCE      300 Re
MAX_STEPS               100000
TRAP_DETECTION          T
DS_RETRY_UNRESOLVED     F
```

For `SCAN`, the actual spectrum grid contains `DS_TRANSMISSION_SCAN_N` points.
`DS_NINTERVALS` is retained for compatibility with the general density input,
but it does not determine the SCAN grid size.

With 32 energy points and a 512-trajectory cap per point, the direction subset is

```text
floor(512 / 32) = 16 directions per energy,
```

so the default workload is

```text
5 × 32 × 16 = 2,560 trajectories.
```

This is a reduction by a factor of 270 relative to the old nominal workload.
The reduced angular and energy resolution does not weaken the tested closure
identities: AMPS and the runner still reconstruct density and flux from the full
saved F4 spectrum generated in that run.

### Termination policy used by F4

F4 uses the structured trajectory API introduced for F3. Therefore:

- outer-boundary escape is resolved and allowed;
- inner-boundary impact and magnetic trapping are resolved and forbidden;
- time, step, and distance limits remain explicitly unresolved;
- unresolved trajectories are excluded from the physical transmission
  denominator;
- unresolved retries are disabled by default to keep this closure test bounded.

Trap detection is enabled so stable dipole bounce trajectories can be classified
as physically forbidden before reaching a numerical safety limit. The finite
trace-time, trace-distance, and step limits remain active as protection against
near-penumbra or otherwise expensive trajectories.

F4 does not use the legacy Boolean cutoff interpretation in which a numerical
limit is mapped to `false`; that compatibility policy is restricted to cutoff
searches such as C1, C2, C3, and C11.

## Live progress and workload reporting

Before launching AMPS, `run_F4.py` prints:

- number of points;
- number of SCAN energy points;
- number of selected directions per energy;
- total estimated trajectory count;
- number of MPI direction-block tasks;
- the output-log path.

AMPS stdout/stderr is now streamed to the terminal while simultaneously being
written to:

```text
test_output/F4_gridless/F4_amps.log
```

Use `--quiet` to restore log-only behavior.

## Files

```text
srcEarth/test/F4/run_F4.py
srcEarth/test/F4/AMPS_PARAM_F4_gridless.in
srcEarth/test/F4/reference_F4_reconstruction.csv
```

The runner writes, by default:

```text
test_output/F4_gridless/AMPS_PARAM_F4.in
test_output/F4_gridless/F4_amps.log
test_output/F4_gridless/gridless_points_density.dat
test_output/F4_gridless/gridless_points_flux.dat
test_output/F4_gridless/gridless_points_spectrum.dat
test_output/F4_gridless/gridless_termination_summary.dat
test_output/F4_gridless/F4_summary.csv
test_output/F4_gridless/F4_result.json
test_output/F4_gridless/reference_F4_reconstruction_used.csv
```

## Running

From the directory containing the AMPS executable:

```bash
python srcEarth/test/F4/run_F4.py
python srcEarth/test/F4/run_F4.py -np 4 -nt 16
python srcEarth/test/F4/run_F4.py --lats -60,-30,0,30,60
python srcEarth/test/F4/run_F4.py --dry-run --workdir test_output/F4_dryrun
python srcEarth/test/F4/run_F4.py --skip-run --workdir test_output/F4_gridless
```

The `--lats -60,...` form is supported directly even though the value starts
with a minus sign.

### Increasing resolution deliberately

A larger functional run can be requested explicitly:

```bash
python srcEarth/test/F4/run_F4.py \
  --scan-n 64 \
  --max-particles 2048 \
  --max-trace-time 300 \
  --max-steps 300000
```

To reproduce the former unbounded angular workload:

```bash
python srcEarth/test/F4/run_F4.py \
  --scan-n 120 \
  --nintervals 160 \
  --max-particles 0 \
  --max-trace-time 900 \
  --max-steps 300000 \
  --max-trace-distance 0
```

That command can require many hours and should not be used as the routine F4
regression test.

## Pass/fail checks

The expected value is zero for all rows in `F4_summary.csv` because the test
reports residuals/errors, not physical density or flux values. The main checks
are:

1. Saved `J_boundary(E)` matches the input power law.
2. Saved `J_local(E)` equals `T(E)J_boundary(E)`.
3. `T(E)` remains in `[0,1]`.
4. The density file matches direct reconstruction from
   `gridless_points_spectrum.dat`.
5. The total flux and every requested energy-channel flux match the numerical
   quadrature used by AMPS.

### Channel-boundary quadrature

The total-flux integral uses the solver energy nodes directly, so it can be
reconstructed by trapezoidal integration of the saved `J_local(E)` samples.
User-defined channel edges such as 3, 10, 30, 100, and 300 MeV generally do not
coincide with nodes of the logarithmic SCAN grid.  For those endpoints,
`DensityGridless.cpp::FluxIntegrateChannel` performs the following operations:

1. clip the requested channel to the solver energy range;
2. insert the exact lower and upper channel energies into an augmented grid;
3. linearly interpolate **transmission `T(E)`** at each inserted edge;
4. evaluate the configured boundary spectrum `J_boundary(E)` directly at the
   exact edge energy;
5. form `J_local(edge) = T_interp(edge) J_boundary(edge)`;
6. apply trapezoidal integration on the augmented grid.

The F4 runner reproduces that same rule.  It intentionally does **not** linearly
interpolate the saved product `J_local(E)` at channel edges.  In general,

```text
interp[T(E) J_boundary(E)] != interp[T(E)] J_boundary(E_edge),
```

because the power-law boundary spectrum is curved between nodes.  On the compact
32-point logarithmic F4 grid, using the former expression can create artificial
channel residuals of roughly `1e-3` to `2e-2`, even when the AMPS output is
correct.  This was the source of the earlier failures in the 10--30, 30--100,
100--300, and 300--1000 MeV channels at latitudes +/-60 degrees.

The separate `boundary_power_law` and `local_spectrum_closure` checks still
verify that the saved spectrum is correct at every solver node.  The specialized
channel reconstruction only supplies the off-grid endpoint values that are not
present in the spectrum file.

Default tolerances are intentionally tight but allow the precision loss of ASCII
Tecplot-style output.

## What F4 does not establish

F4 does not prove that the calculated transmission function is physically
correct. A systematic error in trajectory classification could still pass F4 if
that same transmission is used consistently in all derived outputs. F3 provides
the independent trajectory/transmission reference validation; F4 verifies that
density, total flux, channel fluxes, and saved spectra are constructed
consistently from the resulting transmission.
