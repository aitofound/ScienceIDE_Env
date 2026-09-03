# Quantum Harness #129 Reproduction Prompt

Use this prompt as a standalone instruction set for reproducing and auditing
the `WangTheoPhys` Rust exact-diagonalization workbench. Do not rely on chat
history or regenerate committed oracle data unless a separate oracle-generation
check is explicitly requested.

## Source and validated revision

- Repository:
  `https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust`
- Validated release:
  `v0.5.0`
- License: GNU Affero General Public License v3.0.
- Primary implementation language: Rust; Python/PySCF is oracle-only.

Clone the repository, check out the exact release above, and keep
`Cargo.lock` unchanged:

```bash
git clone \
  https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust.git
cd quantum-harness-129-workbench-rust
git checkout v0.5.0
cargo build --release --locked
```

The recorded primary calculation used:

```text
rustc 1.95.0 (59807616e 2026-04-14) (Homebrew)
cargo 1.95.0 (f2d3ce0bd 2026-03-21) (Homebrew)
Apple M4, arm64, 16 GiB RAM, 10 Rayon threads
```

A compatible stable Rust toolchain should reproduce the numerical
acceptance. Record any toolchain or hardware difference in the audit output.

## Immutable primary input

The primary system is H2O/6-31G in restricted-Hartree-Fock canonical
orbitals:

```text
R(O-H)              0.967 angstrom
angle(H-O-H)        107.6 degree
frozen core         oxygen 1s, spatial orbital index 0
active orbitals     12 spatial orbitals
active electrons    8 (4 alpha, 4 beta)
determinants        245,025
energy unit         hartree
```

The coordinate strings given to PySCF are in angstrom; PySCF/libcint converts
them internally to bohr. Energies, orbital energies, nuclear repulsion, and
one- and two-electron integrals are in hartree. CI coefficients, CC
amplitudes, AO overlaps, and MO coefficients are dimensionless.

Before running a calculation, verify:

```bash
shasum -a 256 fixtures/h2o-631g-fc/FCIDUMP
```

The required SHA-256 is:

```text
826dd373a8b6047dff8136168431a803b59d9ef029a074da3b8f74f22603db3e
```

Do not accept results from a different FCIDUMP as a reproduction of these
tables.

## Quality and fixture checks

Run:

```bash
cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test --locked
git diff --check
find fixtures -name '*.json' -print0 | xargs -0 -n1 jq empty
```

If Python 3.12, `uv`, and the pinned PySCF oracle environment are available,
also run:

```bash
uv venv --python 3.12 .venv
uv pip install --python .venv/bin/python \
  -r scripts/oracle/requirements.txt
.venv/bin/python -m unittest scripts.oracle.test_units -v
```

Normal Rust tests validate the committed full-series artifacts. The live
primary CC and CI/MBPT integration tests are marked ignored because each takes
about three minutes on the recorded machine.

## FCI acceptance

Run the matrix-free Davidson solver:

```bash
RAYON_NUM_THREADS=10 target/release/ed_workbench_rs davidson \
  fixtures/h2o-631g-fc/FCIDUMP \
  --residual-tolerance 1e-7 \
  --max-iterations 60 --max-subspace 20
```

Expected ground-state energy:

```text
-76.12117420414197 hartree
```

Require residual norm at most `1e-7`, absolute error at most `1e-8` hartree
against the committed PySCF FCI oracle, and agreement with the Hirata 2000
Table 2 caption value `-76.121174` at its printed precision. Do not use the
dense `verify` command for this 245,025-determinant space.

## v0.5.0 H2O/cc-pVDZ acceptance

The v0.5.0 extension uses the same requested water geometry, spherical
cc-pVDZ basis, all ten electrons, 24 spatial orbitals, and singlet
`Nalpha=Nbeta=5` sector as the earlier no-point-group-symmetry benchmark.
The only intentional feasibility change is exact reduction to the C₂ᵥ A1
block.

The resulting determinant space and accepted result are:

| Quantity | Value |
|---|---:|
| determinants without point-group reduction | 1,806,590,016 |
| determinants in C₂ᵥ A1 | 451,681,246 |
| Rust Davidson FCI | `-76.243218589558566 E_h` |
| residual norm | `6.602e-8` |
| iterations | 21 |
| Slurm state | `COMPLETED`, exit `0:0` |
| wall time | `3:55:43` |
| Slurm step MaxRSS | `222.257 GiB` |

The production FCIDUMP SHA-256 is:

```text
b55d1bcb04f6889e5b5dff1336412c5f7118b5bdb8461d504764f2a704cd6255
```

Run the committed evidence check:

```bash
cargo test --locked --test ccpvdz_fci_result
```

The multi-hour production solve does not need to be repeated during a normal
audit. If the required single-node memory is available, the submitted
configuration is preserved in `hpc/xh5/production.slurm`. The same-input
PySCF 2.14.0 hierarchy through CCSD(T), unedited Slurm logs, exact input, and
resource accounting are committed under `fixtures/h2o-ccpvdz-ae`.

## CC(1)-CC(8) acceptance

Run:

```bash
RAYON_NUM_THREADS=10 target/release/ed_workbench_rs cc-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-rank 8 \
  --residual-tolerance 1e-6 \
  --max-iterations 100
```

Require the command to exit successfully, every rank to converge with final
residual norm at most `1e-6`, and the final line to report published
verification `PASS`.

| Rank | Expected total energy | Expected `E(CC)-E(FCI)` | Table 2 |
|---:|---:|---:|---:|
| 1 | -75.984502842520712 | 0.136671361621254 | 0.136671 |
| 2 | -76.119629519205702 | 0.001544684936263 | 0.001545 |
| 3 | -76.120725652588177 | 0.000448551553788 | 0.000449 |
| 4 | -76.121162423556896 | 0.000011780585069 | 0.000012 |
| 5 | -76.121170991020350 | 0.000003213121616 | 0.000003 |
| 6 | -76.121174144494702 | 0.000000059647263 | 0.000000 |
| 7 | -76.121174198217162 | 0.000000005924804 | 0.000000 |
| 8 | -76.121174196144139 | 0.000000007997826 | 0.000000 |

CC(2) is CCSD in this challenge, not the approximate method named CC2. It
must agree with the independent PySCF CCSD value
`-76.119629518903210` within `1e-8` hartree.

The paper values above have only six digits after the decimal point.
Acceptance therefore rounds the computed and published
`E(method)-E(FCI)` values to integer microhartree before comparing them; it
must not treat the printed values as higher-precision references.

## CI(1)-CI(8) and MBPT(1)-MBPT(20) acceptance

Run:

```bash
RAYON_NUM_THREADS=10 target/release/ed_workbench_rs level3-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-ci-rank 8 --max-mbpt-order 20 \
  --ci-residual-tolerance 1e-7 \
  --max-iterations 100 --max-subspace 24
```

Require the command to exit successfully, all CI solves to converge with
residual norm at most `1e-7`, the CI energy to be variationally non-increasing,
CI(8) to agree with FCI within `1e-8` hartree, and the final published
verification to be `PASS`.

Expected `E(CI)-E(FCI)` by rank:

```text
0.136671361621538
0.006857789058358
0.005853940551802
0.000174843492616
0.000103257047385
0.000001416471065
0.000000369418771
-0.000000000002004
```

Expected `E(MBPT)-E(FCI)` partial-sum differences for orders 1 through 20:

```text
0.136671361621538  0.008214955387700  0.006577356213612
0.001299545129385  0.000582527596691  0.000178442636724
0.000084838275626  0.000022489352006  0.000013644732491
0.000003004694747  0.000002247016397  0.000000374837001
0.000000394209707  0.000000029304132  0.000000075118919
-0.000000004699885 0.000000015783101 -0.000000003661626
0.000000003714234 -0.000000001489667
```

Compare both series with the corresponding equilibrium columns of Hirata and
Bartlett, Chemical Physics Letters 321, 216-224 (2000), Table 2,
DOI `10.1016/S0009-2614(00)00387-0`, using the six-decimal rule above.

## Evidence and failure reporting

Authoritative committed evidence:

- `fixtures/h2o-631g-fc/reference.json`
- `fixtures/h2o-631g-fc/hirata2000-table2.json`
- `fixtures/h2o-631g-fc/cc_series_results.json`
- `fixtures/h2o-631g-fc/level3_series_results.json`
- `reports/level1-direct-fci.md`
- `reports/level2-cc-accuracy.md`
- `reports/level3-methods.md`
- `reports/level4-integrals.md`
- `reports/tenferro-gap-list.md`
- `fixtures/h2o-ccpvdz-ae/FCIDUMP.c2v`
- `fixtures/h2o-ccpvdz-ae/fci-c2v-xh5-result.json`
- `fixtures/h2o-ccpvdz-ae/pyscf-crosscheck.json`
- `fixtures/h2o-ccpvdz-ae/xh5/production-23008083.out`
- `fixtures/h2o-ccpvdz-ae/xh5/production-23008083.err`
- `fixtures/hpc/scnet-2026-07-30.json`
- `reports/h2o-ccpvdz-c2v-fci.md`
- `reports/scnet-hpc-benchmark.md`
- `reports/data-provenance.md`

For any failure, report the checked-out commit, `rustc -V`, `cargo -V`,
operating system, CPU, memory, `RAYON_NUM_THREADS`, FCIDUMP SHA-256, complete
command, exit code, method/rank/order, energy, difference from its oracle,
residual norm, iteration count, and stderr. Distinguish a numerical mismatch
from a mismatch caused by different geometry, basis, frozen-core choice,
units, fixture bytes, or paper-rounding policy.

The Kállay 2001 DZ and DZP calculations are separate extended Hamiltonians
with their own inputs, results, and precision limits. Do not describe the
primary 6-31G results as validation of those systems, and do not describe the
C₂ᵥ result as a completed no-point-group-symmetry calculation.
