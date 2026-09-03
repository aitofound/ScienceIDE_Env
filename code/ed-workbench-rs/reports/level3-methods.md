# Level 3 CI, MBPT, and Unitary CC Report

Date: 2026-07-27

## Primary H2O/6-31G frozen-core acceptance

The primary calculation uses `R(O-H)=0.967 Å`,
`angle(H-O-H)=107.6°`, canonical RHF orbitals, and the oxygen 1s orbital
frozen. The resulting active problem has 12 spatial orbitals, 8 electrons,
and 245,025 determinants. All energies and energy differences below are in
hartree.

The comparison quantity is `E(method)-E(FCI)`, where the committed FCI energy
is `-76.12117420414197`. Hirata and Bartlett Table 2 prints six digits after
the decimal point, so acceptance compares the computed and published
differences after rounding both to six decimal places. It does not infer
unprinted precision from the paper.

### CI(1)-CI(8)

| Rank | Determinants | Total energy | `E(CI)-E(FCI)` | Published | Iterations | Residual |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 65 | -75.984502842520428 | 0.136671362 | 0.136671 | 2 | 1.044e-9 |
| 2 | 1,425 | -76.114316415083607 | 0.006857789 | 0.006858 | 12 | 3.214e-8 |
| 3 | 12,625 | -76.115320263590164 | 0.005853941 | 0.005854 | 13 | 3.860e-8 |
| 4 | 55,325 | -76.120999360649350 | 0.000174843 | 0.000175 | 14 | 3.279e-8 |
| 5 | 135,069 | -76.121070947094580 | 0.000103257 | 0.000103 | 12 | 7.133e-8 |
| 6 | 208,765 | -76.121172787670901 | 0.000001416 | 0.000001 | 12 | 5.601e-8 |
| 7 | 240,125 | -76.121173834723194 | 0.000000369 | 0.000000 | 9 | 5.060e-8 |
| 8 | 245,025 | -76.121174204143969 | -0.000000000002 | 0.000000 | 8 | 4.078e-8 |

Every row passes the published six-decimal comparison. The CI energy is
variationally non-increasing with rank, and CI(8) differs from the committed
full-CI result by only `2.004e-12` hartree.

### MBPT(1)-MBPT(20)

The partition is the canonical RHF Fock diagonal. The reported energy is the
partial sum through the named perturbation order.

| Order | Energy correction | Partial energy | `E(MBPT)-E(FCI)` | Published |
|---:|---:|---:|---:|---:|
| 1 | -2.842170943e-14 | -75.984502842520428 | 0.136671362 | 0.136671 |
| 2 | -1.284564062e-1 | -76.112959248754265 | 0.008214955 | 0.008215 |
| 3 | -1.637599174e-3 | -76.114596847928354 | 0.006577356 | 0.006577 |
| 4 | -5.277811084e-3 | -76.119874659012581 | 0.001299545 | 0.001300 |
| 5 | -7.170175327e-4 | -76.120591676545274 | 0.000582528 | 0.000583 |
| 6 | -4.040849600e-4 | -76.120995761505242 | 0.000178443 | 0.000178 |
| 7 | -9.360436110e-5 | -76.121089365866339 | 0.000084838 | 0.000085 |
| 8 | -6.234892362e-5 | -76.121151714789960 | 0.000022489 | 0.000022 |
| 9 | -8.844619517e-6 | -76.121160559409475 | 0.000013645 | 0.000014 |
| 10 | -1.064003775e-5 | -76.121171199447218 | 0.000003005 | 0.000003 |
| 11 | -7.576783532e-7 | -76.121171957125568 | 0.000002247 | 0.000002 |
| 12 | -1.872179402e-6 | -76.121173829304965 | 0.000000375 | 0.000000 |
| 13 | 1.937270473e-8 | -76.121173809932259 | 0.000000394 | 0.000000 |
| 14 | -3.649055740e-7 | -76.121174174837833 | 0.000000029 | 0.000000 |
| 15 | 4.581479282e-8 | -76.121174129023046 | 0.000000075 | 0.000000 |
| 16 | -7.981879862e-8 | -76.121174208841850 | -0.000000005 | 0.000000 |
| 17 | 2.048299195e-8 | -76.121174188358864 | 0.000000016 | 0.000000 |
| 18 | -1.944472983e-8 | -76.121174207803591 | -0.000000004 | 0.000000 |
| 19 | 7.375864516e-9 | -76.121174200427731 | 0.000000004 | 0.000000 |
| 20 | -5.203908072e-9 | -76.121174205631633 | -0.000000001 | 0.000000 |

All 20 partial sums pass the published six-decimal comparison.

The combined primary CI/MBPT run took 190.08 seconds on an Apple M4 using 10
Rayon threads: 111.729 seconds for CI and 77.230 seconds for MBPT. Maximum
resident-set size was 129,646,592 bytes. The complete machine-readable record,
including checksum, toolchain, tolerances, per-order timings, and exact
unrounded values, is
[`fixtures/h2o-631g-fc/level3_series_results.json`](../fixtures/h2o-631g-fc/level3_series_results.json).

Run the acceptance calculation with:

```bash
RAYON_NUM_THREADS=10 cargo run --release -- level3-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-ci-rank 8 --max-mbpt-order 20 \
  --ci-residual-tolerance 1e-7 \
  --max-iterations 100 --max-subspace 24
```

## Small-system cross-checks

### CI(n)

Linear H4/STO-3G:

| Method | Energy (hartree) | Davidson iterations | Residual |
|---|---:|---:|---:|
| CI(1) | -2.098545936998035 | 3 | 4.441e-16 |
| CI(2) | -2.165031841780534 | 10 | 2.008e-15 |
| CI(4) | -2.166387448634764 | 11 | 4.306e-10 |

The sequence is variationally non-increasing. CI(4), the full excitation rank
for four electrons, agrees with PySCF FCI to better than `1e-14` hartree.

### MBPT(n)

H2/STO-3G, using the canonical RHF Fock diagonal as `H0`:

| Order | Energy correction | Partial sum |
|---:|---:|---:|
| 1 | 1.110223024625157e-16 | -0.941480654707799 |
| 2 | -3.908905485819417e-2 | -0.980569709565993 |
| 3 | -2.071042716569801e-2 | -1.001280136731691 |
| 4 | -9.772141028901954e-3 | -1.011052277760593 |
| 5 | -3.905112572958080e-3 | -1.014957390333551 |
| 6 | -1.131558429877173e-3 | -1.016088948763428 |

The second-order partial sum matches the committed PySCF MP2 oracle. Higher
orders are reported individually so convergence or divergence is visible.

### Unitary CC(n)

H2/STO-3G UCC(2):

```text
energy: -1.015468249288246 hartree
PySCF FCI: -1.015468249288245 hartree
gradient norm: 1.543e-9
iterations: 4
parameters: 3
```

The implementation applies the anti-Hermitian generator `T-T†`, evaluates its
Taylor exponential action, and minimizes the normalized variational energy
using deterministic BFGS with a line search and finite-difference gradient.

The stronger H4/STO-3G full-rank check uses 35 UCC(4) parameters and converges
in 22 BFGS iterations to −2.166387448634763 Hartree with a gradient norm of
5.605 × 10⁻⁸. This agrees with FCI to floating-point precision. See
[`reports/multiroot-and-ucc.md`](multiroot-and-ucc.md) for the complete record
and scope boundary.

## Commands

```bash
cargo run --release -- ci fixtures/h4-sto3g/FCIDUMP --rank 4
cargo run --release -- mbpt \
  fixtures/h2-sto3g/FCIDUMP fixtures/h2-sto3g/reference.json --order 6
cargo run --release -- ucc fixtures/h2-sto3g/FCIDUMP --rank 2
cargo run --release -- ucc fixtures/h4-sto3g/FCIDUMP --rank 4
```

CI and MBPT are practical on the direct-FCI spaces supported by Level 1.
The present UCC implementation forms `T†` transparently and uses numerical
gradients, so it is intentionally a small-system reference rather than a
large-system production optimizer.
