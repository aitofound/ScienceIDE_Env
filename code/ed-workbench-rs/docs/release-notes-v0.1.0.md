# v0.1.0 — Quantum Harness #129 validated submission

This is the first audited public release of the determinant-based Rust
electronic-structure workbench for
[Quantum Harness #129](https://github.com/QuantumBFS/quantum.harness/issues/129).
It includes the mandatory oracle, direct FCI, Davidson, and arbitrary-order
CC(n) path plus CI(n), MBPT(n), UCC(n), and the direct-libcint Level 4 stack.

## Immutable primary input

H2O/6-31G, canonical RHF orbitals, oxygen 1s frozen,
`R(O-H)=0.967 Å`, `angle(H-O-H)=107.6°`, 12 active spatial orbitals,
8 active electrons, and 245,025 determinants.

Primary FCIDUMP SHA-256:

```text
826dd373a8b6047dff8136168431a803b59d9ef029a074da3b8f74f22603db3e
```

Coordinates are supplied in Angstrom and converted internally to Bohr by
PySCF/libcint. Energies and energy-valued integrals are Hartree.
Wavefunction coefficients, amplitudes, overlaps, and orbital coefficients are
dimensionless.

## Headline results

| Method | Total energy (`E_h`) | Evidence |
|---|---:|---|
| FCI | -76.121174204141980 | Davidson residual `5.044e-8` |
| CC(2) / CCSD | -76.119629519205702 | `3.025e-10 E_h` from PySCF CCSD |
| CC(8) | -76.121174196144139 | `7.998e-9 E_h` from FCI |
| CI(8) | -76.121174204143969 | `2.004e-12 E_h` from FCI |

CC(1)-CC(8), CI(1)-CI(8), and MBPT(1)-MBPT(20) match all 36 equilibrium
method-minus-FCI entries printed in Hirata and Bartlett 2000 Table 2. The
comparison rounds both sides to the six decimal places supplied by the paper.

The primary CC series took 186.94 seconds. The combined CI/MBPT series took
190.08 seconds. Recorded hardware was an arm64 Apple M4 with 16 GiB memory
and 10 Rayon workers. The calculation toolchain was Rust 1.95.0 and Cargo
1.95.0; the tested minimum Rust version for the locked dependency graph is
1.89.

## Verification

Install the pinned Python oracle dependency:

```bash
uv venv --python 3.12 .venv
uv pip install --python .venv/bin/python -r scripts/oracle/requirements.txt
```

Run every normal gate:

```bash
scripts/verify-submission.sh
```

Run the complete live primary series:

```bash
RAYON_NUM_THREADS=10 cargo run --release --locked -- cc-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-rank 8 --residual-tolerance 1e-6 --max-iterations 100

RAYON_NUM_THREADS=10 cargo run --release --locked -- level3-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-ci-rank 8 --max-mbpt-order 20 \
  --ci-residual-tolerance 1e-7 \
  --max-iterations 100 --max-subspace 24
```

The release contains source, `Cargo.lock`, checksummed oracle fixtures,
machine-readable results, and detailed reports. It intentionally does not
ship platform-specific binaries because the static direct-libcint build is
platform-sensitive.

## Detailed evidence

- [FCI](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.1.0/reports/level1-direct-fci.md)
- [CC(1)-CC(8)](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.1.0/reports/level2-cc-accuracy.md)
- [CI/MBPT/UCC](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.1.0/reports/level3-methods.md)
- [Direct libcint/RHF/AO-to-MO](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.1.0/reports/level4-integrals.md)
- [tenferro-rs gap list](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.1.0/reports/tenferro-gap-list.md)
- [Standalone reproduction prompt](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.1.0/docs/reproduction-prompt.md)
- [Upstream solution PR #217](https://github.com/QuantumBFS/quantum.harness/pull/217)

## Scope boundary

Kállay 2001 DZ/DZP calculations and stretched-water hard-mode calculations
are research extensions and are not claimed by this release.

License: GNU Affero General Public License v3.0.
