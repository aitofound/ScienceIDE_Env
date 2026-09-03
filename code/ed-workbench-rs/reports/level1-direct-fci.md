# Level 1 Direct FCI Report

Date: 2026-07-27

## Acceptance Results

| System | Determinants | Solver | Rust energy (Hartree) | PySCF FCI (Hartree) | Absolute error (Hartree) |
|---|---:|---|---:|---:|---:|
| Equilibrium H2/STO-3G, H-H = 0.7414 Å | 4 | direct Davidson | -1.137270174660904 | -1.137270174660904 | 2.220 × 10⁻¹⁶ |
| Stretched H2/STO-3G, H-H = 1.4 Å | 4 | direct Davidson | -1.015468249288245 | -1.015468249288245 | < 1e-12 |
| Linear H4/STO-3G | 36 | direct Davidson | -2.166387448634769 | -2.166387448634763 | < 1e-11 |
| H2O/STO-3G | 441 | dense | -75.012918738044235 | -75.012918738044462 | 2.274e-13 |
| H2O/STO-3G | 441 | direct Davidson | -75.012918738044576 | -75.012918738044462 | 1.137e-13 |
| H2O/6-31G, frozen O 1s | 245,025 | direct Davidson | -76.121174204141980 | -76.121174204141980 | < 1e-12 |

The primary result also reproduces the issue #129 published anchor
`-76.121174` hartree.

## Primary Run

```bash
cargo run --release -- davidson fixtures/h2o-631g-fc/FCIDUMP \
  --residual-tolerance 1e-7 \
  --max-iterations 60 \
  --max-subspace 20
```

Observed on the snapshot machine:

```text
energy: -76.121174204141980
residual norm: 5.044e-8
iterations: 16
converged: true
wall time: 20.24 s
maximum resident set size: 85,966,848 bytes
```

## Algorithm

The operator uses alpha and beta lexical string spaces. Each string has signed
one-body `E_pq` links. The Hamiltonian's one-body part is absorbed into a
symmetrized two-index-pair tensor, and sigma applies two link contractions:

```text
sigma = sum_pqrs g[pqrs] E_pq E_rs C + Ecore C
```

No determinant Hamiltonian matrix or sparse matrix is stored. Davidson uses a
separately evaluated Slater–Condon diagonal as its preconditioner.

## Provenance

- geometry: `rOH = 0.967 Angstrom`, `HOH = 107.6 degrees`;
- basis: 6-31G;
- canonical restricted-HF molecular orbitals;
- frozen orbital: canonical MO 0 (oxygen 1s);
- active space: 12 spatial orbitals, 8 electrons, MS2=0;
- PySCF: 2.14.0;
- Rust convergence: residual below `1e-7`.
