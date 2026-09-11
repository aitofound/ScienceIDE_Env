# F15 — density normalization from differential flux

F15 validates the unit conversion from isotropic directional differential flux to number density:

```text
F = 4π ∫ J(E) dE
n = 4π ∫ J(E) / v(E) dE
```

The test uses zero-field transport, so the transmission is exactly one:

```text
FIELD_MODEL          NONE
EFIELD_MODEL         NONE
R_INNER              0.0 km
DS_TRANSMISSION_MODE DIRECT
SPECTRUM_TYPE        TABLE
```

The runner executes four independent narrow top-hat TABLE spectra centered at

```text
1, 10, 100, 1000 MeV
```

by default.  For each center energy `E0`, the default full width is `0.02 E0`, so the active interval is

```text
E1 = E0 * (1 - 0.01)
E2 = E0 * (1 + 0.01)
J(E) = J0 inside [E1,E2]
J(E) = 0 outside [E1,E2] because SPEC_EMIN/SPEC_EMAX clip the TABLE spectrum
```

For a finite top-hat, the analytical references are

```text
F = 4π J0 (E2 - E1)
```

and, for protons,

```text
n = 4π J0/c * [sqrt(E2(E2 + 2m)) - sqrt(E1(E1 + 2m))]
```

where `E1`, `E2`, and `m` are in MeV.  The runner derives `m` from the
same `MASS_AMU = 1.007276466621`, atomic-mass-unit constant, elementary
charge, and speed of light used by `DensityGridless.cpp` (approximately
`938.272088161 MeV`).  This follows from

```text
v(E)/c = sqrt(E(E+2m)) / (E+m)
```

so the test directly exercises the relativistic `1/v(E)` density factor.


## Spectrum-endpoint roundoff protection

F15 deliberately places `SPEC_EMIN` and `SPEC_EMAX` exactly on the two TABLE
spectrum nodes.  The density solver evaluates the spectrum after converting an
energy from MeV to Joules and back to MeV.  Values such as `0.99` and `99` MeV
can return a few floating-point ulps below the original lower bound.  A strict
`E < SPEC_EMIN` comparison would therefore set the first spectrum node to zero
and remove half of the first trapezoidal integration cell, producing an
approximately `0.388%` common error in both flux and density.

`cSpectrum::GetSpectrum()` and `GetSpectrumPerMeV()` now accept and clamp only
roundoff-sized endpoint excursions (32 machine epsilons at the local energy
scale).  Energies genuinely outside the configured support still return zero.
This behavior is part of what F15 protects: broad-spectrum tests such as F1,
F2, and F4 do not necessarily expose a one-node endpoint loss.

## Run

From the directory containing `amps`:

```bash
python srcEarth/test/F15/run_F15.py -np 4 -nt 16
```

Useful variants:

```bash
python srcEarth/test/F15/run_F15.py -np 1 -nt 1
python srcEarth/test/F15/run_F15.py --nintervals 256 --density-tol 1e-6
python srcEarth/test/F15/run_F15.py --width-frac 0.01
python srcEarth/test/F15/run_F15.py --centers 1,10,100,1000
python srcEarth/test/F15/run_F15.py --dry-run
python srcEarth/test/F15/run_F15.py --skip-run --workdir test_output/F15_gridless
```

## Output files

The default work directory is `test_output/F15_gridless`.  The runner creates one subdirectory per center energy:

```text
E1MeV/
E10MeV/
E100MeV/
E1000MeV/
```

Each case directory contains:

```text
AMPS_PARAM_F15.in
F15_top_hat_spectrum.dat
F15_amps.log
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
```

The top-level F15 work directory contains:

```text
reference_F15_top_hat_generated.csv
F15_summary.csv
F15_result.json
```

The repository reference for the default case set is:

```text
srcEarth/test/F15/reference_F15_top_hat.csv
```

`F15_summary.csv` uses the same convention as the other F tests:

```text
case, center_MeV, E1_MeV, E2_MeV, point, check, check_type, passed,
value, expected_value, abs_error, rel_error, rel_tol, abs_tol, units, note
```

Zero `expected_value` entries appear only for residual/error metrics such as `T(E)-1`, `J_local/J_boundary-1`, file-vs-spectrum reconstruction error, or center/off-center spread.  Physical references such as density, flux, and effective speed are nonzero.

## PASS/FAIL checks

For each top-hat case and for both spatial points, F15 checks:

1. Total integral flux against `4πJ0(E2-E1)`.
2. Total density against the closed-form `4π∫J(E)/v(E)dE` reference.
3. Effective speed `F/n` against the analytical finite-top-hat value.
4. The finite-width monoenergetic ratio against the analytical correction to `F/v(E0)`.
5. `T(E)=1`, `J_local/J_boundary=1`, and constant top-hat `J_boundary(E)=J0`.
6. Density and flux reconstructed from `gridless_points_spectrum.dat` against AMPS integrated output.
7. Center and near-boundary point agreement in the zero-field box.
