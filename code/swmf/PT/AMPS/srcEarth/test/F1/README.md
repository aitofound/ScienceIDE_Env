# F1 — zero-field density/flux normalization

F1 is the first density/flux validation test from `validation.docx`. It checks the normalization path before any magnetic shielding physics is introduced.

The run uses:

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

With zero magnetic field and no inner absorbing sphere, every trajectory is a straight line to the outer box. Therefore the expected solution is:

```text
T(E) = 1
J_local(E) = J_boundary(E)
F = 4π ∫ J(E) dE
n = 4π ∫ J(E)/v(E) dE
```

The test samples ten non-origin Cartesian points inside a ±10,000 km box. Because the physical solution is spatially uniform, all density and flux rows should be identical within roundoff.

## Run

From the directory containing `amps`:

```bash
python srcEarth/test/F1/run_F1.py -np 4 -nt 16
```

Useful variants:

```bash
python srcEarth/test/F1/run_F1.py -np 1 -nt 1
python srcEarth/test/F1/run_F1.py --scheduler STATIC --dynamic-chunk 0
python srcEarth/test/F1/run_F1.py --skip-run --workdir test_output/F1_gridless
python srcEarth/test/F1/run_F1.py --dry-run
```

## Output files

The default work directory is `test_output/F1_gridless`. The AMPS run should produce:

```text
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
```

AMPS writes all floating-point values with `std::numeric_limits<double>::max_digits10` precision. This guarantees a lossless text round trip for IEEE-754 `double` values and allows the harness to reconstruct density and flux from the spectrum file without errors caused by six-digit stream formatting.

The harness writes:

```text
AMPS_PARAM_F1.in
F1_amps.log
reference_F1_zero_field.csv
F1_summary.csv
F1_result.json
```

At the end of a completed validation, the runner prints the same final status
format used by the C-test runners:

```text
RESULT: PASS
```

or:

```text
RESULT: FAIL
```

The process exit status is generated from the same Boolean as this printed
result: `0` for PASS and `1` for FAIL. A nonzero AMPS execution status is also
reported as `RESULT: FAIL` and normalized to test-runner exit status `1`; the
original AMPS exit value remains in the preceding diagnostic message. `--dry-run`
only prints the command and exits `0` without claiming that the validation passed.

`F1_summary.csv` uses the columns:

```text
check, check_type, passed, value, expected_value, abs_error, rel_error, rel_tol, abs_tol, units, note
```

## PASS/FAIL checks

F1 applies four groups of checks:

1. `T(E)=1` and `J_local/J_boundary=1` at every point and energy.
2. Density and total flux at every point match the analytical power-law normalization within the default 2% tolerance. This tolerance allows the production trapezoidal energy grid to be compared against the continuous closed-form/high-resolution reference.
3. Flux in the LOW, MID, HIGH, and FULL energy channels matches the closed-form power-law channel integrals.
4. Spatial variation of density and flux across the ten points is zero within roundoff.

The reference file stores only the continuous analytical physical target values. In `F1_summary.csv`, the old ambiguous `reference` column has been replaced by `expected_value`, and a `check_type` column separates `physical_reference` checks from `error_metric` checks. Zero expected values therefore mean that the expected error or deviation is zero, not that the physical density or flux is zero. For such zero-target metrics, conventional relative error is undefined; the harness writes an empty CSV field / JSON `null` for `rel_error` and uses the specified absolute tolerance. This avoids misleading values near `1e294` that resulted from dividing by an artificial `1e-300` denominator.

The script also reconstructs density and flux directly from `gridless_points_spectrum.dat` to verify that the output files are internally consistent. Its relativistic speed calculation derives the proton rest energy from the same `MASS_AMU`, atomic-mass-unit constant, elementary charge, and speed of light used by `DensityGridless.cpp`, so the reconstruction check does not contain a test-harness mass mismatch. The continuous density value in `reference_F1_zero_field.csv` is evaluated with those same constants.

## Numerical accuracy and energy resolution

F1 intentionally retains `DS_NINTERVALS=80` and the default 2% physical-reference tolerance. The solver integrates on a logarithmic energy grid with a trapezoidal rule in energy, while the reference values are continuous closed-form/high-resolution integrals. For the steep `E^-3.5` test spectrum, the 80-interval quadrature is expected to overestimate total flux by about 0.98% and density by about 1.24%; this is discretization error, not a trajectory or normalization defect. Increase `DS_NINTERVALS` when a study requires a smaller continuous-integral error, recognizing that the trajectory count and runtime increase approximately in proportion to the number of energy intervals.

## Code requirement

F1 requires the gridless field evaluator to support:

```text
FIELD_MODEL NONE
```

In this implementation, `NONE` returns `B=(0,0,0)` and skips all Geopack/Tsyganenko initialization. This is intentionally limited to the gridless validation path and gives an exact straight-line reference problem for density/flux normalization.
