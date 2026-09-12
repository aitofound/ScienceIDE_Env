# F2 — power-law energy integration

F2 is the second density/flux validation test from `validation.docx`. It verifies the energy folding used for differential flux, integral flux, and density when the imposed boundary spectrum has an analytical power-law form.

The run uses the same trivial transport setup as F1:

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

With `B=0` and no inner absorbing sphere, the transmission is exactly:

```text
T(E) = 1
J_local(E) = J_boundary(E)
```

The validation-plan energy bins

```text
1, 3, 10, 30, 100, 300, 1000 MeV
```

are represented in the parser-compatible AMPS input as `#ENERGY_CHANNELS`:

```text
BIN01_1_3        1.0      3.0
BIN02_3_10       3.0     10.0
BIN03_10_30     10.0     30.0
BIN04_30_100    30.0    100.0
BIN05_100_300  100.0    300.0
BIN06_300_1000 300.0   1000.0
FULL             1.0   1000.0
```

The production quadrature uses a fine log-spaced energy grid, `DS_NINTERVALS=960` by default, so the AMPS trapezoidal integral can be compared directly with the closed-form power-law flux integral. Density uses the same energy grid and is compared with an independent high-resolution log-quadrature reference for `4π∫J(E)/v(E)dE`.

## Run

From the directory containing `amps`:

```bash
python srcEarth/test/F2/run_F2.py -np 4 -nt 16
```

Useful variants:

```bash
python srcEarth/test/F2/run_F2.py -np 1 -nt 1
python srcEarth/test/F2/run_F2.py --nintervals 1920 --integration-tol 5e-5
python srcEarth/test/F2/run_F2.py --skip-run --workdir test_output/F2_gridless
python srcEarth/test/F2/run_F2.py --dry-run
```

## Output files

The default work directory is `test_output/F2_gridless`. The AMPS run should produce:

```text
gridless_points_density.dat
gridless_points_spectrum.dat
gridless_points_flux.dat
```

The harness writes:

```text
AMPS_PARAM_F2.in
F2_amps.log
reference_F2_power_law.csv
F2_summary.csv
F2_result.json
```

`F2_summary.csv` uses the columns:

```text
check, check_type, passed, value, expected_value, abs_error, rel_error, rel_tol, abs_tol, units, note
```

## PASS/FAIL checks

F2 applies five groups of checks:

1. `T(E)=1` and `J_local/J_boundary=1` at every saved energy node.
2. Saved differential `J_boundary(E)` and `J_local(E)` match the imposed power law at every saved energy node.
3. Total density and total integral flux match analytical or high-resolution references.
4. Integral flux in the validation energy bins matches the closed-form power-law integral.
5. Density reconstructed over the same bins matches the independent high-resolution density reference, and file-integrated fluxes match reconstruction from `gridless_points_spectrum.dat`.

The reference file stores physical target values. Error metrics in `F2_summary.csv` may have `expected_value=0`; these are expected deviations or residuals, not zero physical flux or density.
