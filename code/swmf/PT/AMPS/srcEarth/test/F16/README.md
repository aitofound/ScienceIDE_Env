# F16 — blocked-access zero-flux regression

F16 validates the exact zero-access limit in the gridless density/spectrum
workflow.  The input uses a nonzero incident power-law spectrum, but every
diagnostic point is placed inside the inner absorbing sphere (`R_INNER`).  The
shared gridless trajectory classifier checks the inner loss sphere before the
first particle push, so every sampled direction and energy must be forbidden.

The exact reference is

```text
T(E)       = 0
J_local(E) = T(E) J_boundary(E) = 0
n          = 4π ∫ J_local(E)/v(E) dE = 0
F          = 4π ∫ J_local(E) dE = 0
F_channel  = 4π ∫_channel J_local(E) dE = 0
```

This catches leakage through blocked locations, stale transmission arrays,
uninitialized flux channels, and zero-signal normalization errors.  The runner
also verifies that `J_boundary(E)` is nonzero, so the test is a blocked-access
case rather than a zero-input spectrum case.

## Files

```text
srcEarth/test/F16/AMPS_PARAM_F16_gridless.in
srcEarth/test/F16/reference_F16_blocked_access.csv
srcEarth/test/F16/run_F16.py
```

## Run

From the AMPS executable directory:

```bash
python srcEarth/test/F16/run_F16.py -np 4 -nt 16
```

Useful alternatives:

```bash
python srcEarth/test/F16/run_F16.py --nintervals 160 --zero-flux-tol 1e-250
python srcEarth/test/F16/run_F16.py --dry-run --workdir test_output/F16_dryrun
python srcEarth/test/F16/run_F16.py --skip-run --workdir test_output/F16_gridless
```

## Outputs

The default output directory is `test_output/F16_gridless`.  The runner writes

```text
F16_summary.csv
F16_result.json
reference_F16_blocked_access_used.csv
```

and uses the standard gridless AMPS outputs:

```text
gridless_points_density.dat
gridless_points_flux.dat
gridless_points_spectrum.dat
```

The summary CSV uses `expected_value=0` for the physical zero-flux/density
reference and for exact residual checks.  Those zero entries mean that the
blocked-access solution must be zero despite a nonzero incident spectrum.
