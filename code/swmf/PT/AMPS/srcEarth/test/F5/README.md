# F5 — Directional anisotropy / PAD mapping

F5 validates the nontrivial pitch-angle-distribution mapping in the gridless
`DENSITY_SPECTRUM` solver with `DS_BOUNDARY_MODE ANISOTROPIC`.

The test runs three parser-supported PAD cases with the same dipole field,
energy grid, direction sampling, and boundary spectrum:

```text
ISOTROPIC
COSALPHA_N,  BA_PAD_EXPONENT = 2
SINALPHA_N,  BA_PAD_EXPONENT = 2
```

The primary reference is exact:

```text
sin^2(alpha) + cos^2(alpha) = 1
```

Therefore, for identical trajectory access decisions,

```text
T_sin(E) + T_cos(E) = T_iso(E)
J_local_sin(E) + J_local_cos(E) = J_local_iso(E)
n_sin + n_cos = n_iso
F_sin + F_cos = F_iso
```

This directly checks that `BA_PAD_MODEL=COSALPHA_N` and
`BA_PAD_MODEL=SINALPHA_N` are applied to the same boundary pitch-angle cosine
and with the correct complementary weights.

The secondary reference is a high-energy straight-line semi-analytic PAD mapping
for three diagnostic points:

```text
TC_EQ8: 8 Re equator, GSM x-axis
TC_PL8: 8 Re magnetic pole, +Z axis
TC_EQ6: 6 Re equator, GSM x-axis
```

Because the default energy range is 5–50 GeV, magnetic bending is small compared
with the 9 Re box size. The runner estimates
`density_aniso / density_isotropic` by straight-line ray tracing from the point
to the box boundary, applying the exact dipole-field pitch-angle function at the
exit point. These semi-analytic ratios are approximate and use looser tolerances
than the exact complement identity.

## Run

From the directory containing the AMPS executable:

```bash
python srcEarth/test/F5/run_F5.py -np 4 -nt 16
```

Useful variants:

```bash
python srcEarth/test/F5/run_F5.py -np 1 -nt 1
python srcEarth/test/F5/run_F5.py -np 4 -nt 16 --complement-tol 1e-7
python srcEarth/test/F5/run_F5.py --scan-n 128 --nintervals 96
python srcEarth/test/F5/run_F5.py --semi-n-theta 240 --semi-n-phi 480
python srcEarth/test/F5/run_F5.py --dry-run
python srcEarth/test/F5/run_F5.py --skip-run --workdir test_output/F5_gridless
```

## Outputs

The runner writes one case directory per PAD model:

```text
test_output/F5_gridless/ISOTROPIC
test_output/F5_gridless/COSALPHA_N_n2
test_output/F5_gridless/SINALPHA_N_n2
```

Each case contains the rendered input file, AMPS log, and the standard gridless
outputs:

```text
gridless_points_density.dat
gridless_points_flux.dat
gridless_points_spectrum.dat
```

The top-level work directory contains:

```text
F5_summary.csv
F5_result.json
reference_F5_pad_mapping_used.csv
```

The repository reference table is:

```text
srcEarth/test/F5/reference_F5_pad_mapping.csv
```

## Notes

F5 is defined specifically for `BA_PAD_EXPONENT=2` because the exact complement
identity uses `sin^2(alpha) + cos^2(alpha) = 1`. Use F11 for the broader PAD
model/exponent identity matrix.
