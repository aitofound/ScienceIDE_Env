# F11 — anisotropic PAD model sum-check

F11 validates exact identities in the gridless anisotropic boundary-spectrum
implementation:

1. `BA_PAD_EXPONENT = 0` must reduce every implemented PAD model to isotropic.
   For any direction, `sin^0(alpha) = 1` and `|cos(alpha)|^0 = 1`, so the
   density, integral flux, differential spectrum, and transmissivity should be
   identical to `BA_PAD_MODEL = ISOTROPIC`.
2. `BA_PAD_MODEL = COSALPHA_N` and `BA_PAD_MODEL = BIDIRECTIONAL` must produce
   identical results for the same exponent because both are implemented as
   `f_PAD(alpha) = |cos(alpha)|^n`.

The test deliberately checks identities rather than absolute flux values.  The
absolute density at a dipole point depends on the selected tracing settings,
angular sampling, inner-boundary loss sphere, and finite-domain escape
classification.  The F11 reference solution is therefore a set of exact
zero-difference pairwise comparisons.

## Run

Run from the directory containing the `amps` executable:

```bash
python srcEarth/test/F11/run_F11.py -np 4 -nt 16
python srcEarth/test/F11/run_F11.py -np 1 -nt 1 --models ISOTROPIC,SINALPHA_N,COSALPHA_N,BIDIRECTIONAL --exponents 0,2,4
python srcEarth/test/F11/run_F11.py --dry-run
python srcEarth/test/F11/run_F11.py --skip-run --workdir test_output/F11_gridless
```

The runner creates one subdirectory per PAD model/exponent case under
`test_output/F11_gridless`, for example `COSALPHA_N_n2` and
`BIDIRECTIONAL_n2`.

## Input template

`AMPS_PARAM_F11_gridless.in` is a parser-compatible gridless density/spectrum
input.  The runner edits:

- `BA_PAD_MODEL`
- `BA_PAD_EXPONENT`
- `MODE3D_THREADS`
- `GRIDLESS_MPI_SCHEDULER`
- `GRIDLESS_MPI_DYNAMIC_CHUNK`
- `DS_TRANSMISSION_SCAN_N`

The default model matrix is:

```text
ISOTROPIC_n0
SINALPHA_N_n0, SINALPHA_N_n1, SINALPHA_N_n2, SINALPHA_N_n4, SINALPHA_N_n8
COSALPHA_N_n0, COSALPHA_N_n1, COSALPHA_N_n2, COSALPHA_N_n4, COSALPHA_N_n8
BIDIRECTIONAL_n0, BIDIRECTIONAL_n1, BIDIRECTIONAL_n2, BIDIRECTIONAL_n4, BIDIRECTIONAL_n8
```

## Outputs checked

For every identity pair and every output point, the runner compares:

- total density from `gridless_points_density.dat`
- total integral flux and all named energy-channel fluxes from
  `gridless_points_flux.dat`
- `T(E)`, `J_boundary(E)`, and `J_local(E)` from
  `gridless_points_spectrum.dat`

The main summary files are:

- `F11_summary.csv`
- `F11_result.json`
- `reference_F11_pad_identities_used.csv`

## Acceptance

The default relative tolerance for identity comparisons is `1e-8`.  The spectrum
energy grids must match exactly in length and to within roundoff in energy value.
If very large parallel reductions produce tiny order-dependent differences, use
`--identity-tol` to loosen the threshold explicitly and archive the setting in
the result JSON.
