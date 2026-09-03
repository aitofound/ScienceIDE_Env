# Challenge 129 Brief

Source: https://github.com/QuantumBFS/quantum.harness/issues/129

## Metadata

| Field | Value |
|---|---|
| Upstream issue | QuantumBFS/quantum.harness#129 |
| Title | Exact diagonalization workbench in Rust for electronic structure method development |
| State | Open |
| Labels | challenge, accepted |
| Released by | Guo CHEN, HKUST(GZ) |
| Contact | guochen@hkust-gz.edu.cn |
| Method / track | Exact Diagonalization / `ed` |
| Active solution PR | https://github.com/QuantumBFS/quantum.harness/pull/217 |
| Superseded registration PR | https://github.com/QuantumBFS/quantum.harness/pull/210 |
| Working repo | https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust |
| Registered team | Rewrite It In Rust! (RIIR 2607 Hefei) |
| Members | Chenxi Wan, Yedi Shen, Junkai Wang |
| Solution directory | `tracks/ed/solutions/WangTheoPhys/` |

## Core Objective

Build a Rust reference workbench for determinant-based electronic-structure
exact diagonalization / full configuration interaction (FCI). The mandatory
showcase is arbitrary-order coupled cluster CC(n) implemented on top of the same
determinant and sigma-vector machinery.

The intended research value is a trusted Rust oracle for future optimized
electronic-structure codes, plus a concrete gap list for Rust scientific
computing tools such as `tenferro-rs`.

## Required Ladder

Level 0 - Oracle and tiny dense checks:

- Use PySCF to generate restricted Hartree-Fock results, MO integrals, FCIDUMP
  files, FCI energies, and CCSD energies.
- Suggested systems: H2, H4, water/STO-3G, water/6-31G.
- In Rust, parse FCIDUMP, enumerate alpha/beta strings, build tiny Hamiltonians
  densely, diagonalize, and compare to oracle data.

Level 1 - Direct FCI:

- Precompute single-excitation lists between alpha/beta strings.
- Implement Olsen / Knowles-Handy style sigma-vector construction.
- Implement Davidson ground-state iteration.
- Primary target: water/6-31G with frozen oxygen 1s core.
- Extended target: water/DZ, all electrons.

Level 2 - Arbitrary-order CC(n):

- Implement determinant-based CC using Hirata 2000's projected equations.
- Apply T to full-CI-length vectors using the same determinant machinery.
- Build exp(T)|HF> with a Taylor series.
- Solve amplitude equations with denominator updates and DIIS.
- Verify CC(2) against PySCF CCSD, then compare CC(n) to Hirata 2000 Table 2.

Stretch levels:

- CI(n), MBPT(n), and unitary CC(n) from the same machinery.
- Direct Rust integral path through libcint, removing PySCF as a runtime
  dependency after verification.

## Scale and Convergence Notes

The upstream issue gives the following practical estimates:

| Target | Determinants / memory note |
|---|---|
| Water/6-31G, frozen core | 245,025 determinants; one FCI vector is about 2 MB |
| Water/DZ, all electrons | Roughly 10^6 determinants in the relevant symmetry block; about 4 x 10^6 without spatial symmetry |
| Water/DZP, frozen core | Roughly 2.8 x 10^7 determinants; one vector is a few hundred MB |

Hirata 2000 declares convergence when the residual-vector norm falls below
`1e-6`, which is expected to settle the energy to about `1e-7` hartree.
Equilibrium calculations are mandatory; stretched geometries at 1.5 and 2.0
times the equilibrium bond length are optional hard-mode tests.

## Numerical Anchors

These values are copied from the upstream challenge description as target
sanity checks:

| System | Setting | Published FCI anchor |
|---|---|---|
| Water / 6-31G | frozen oxygen 1s, equilibrium | -76.121174 hartree |
| Water / DZ | all electrons | -76.156699 hartree |
| Water / DZP | frozen oxygen 1s | -76.256624 hartree |

Primary geometry: water equilibrium with r_OH = 0.967 Angstrom and HOH angle
107.6 degrees.

## Deliverables

- Rust implementation through Level 2.
- Oracle files: FCIDUMP data, reference JSON, and verification scripts.
- Accuracy tables against PySCF and published tables.
- `tenferro-rs` gap list.
- Public Quantum Harness PR under `tracks/ed/solutions/WangTheoPhys/`.

The gap list should explicitly cover indexed gather/scatter-add, mutable views
and slicing, element-wise division, in-place BLAS-1 operations, memory layout,
numerical behavior, performance, and API friction. Findings relevant to
tenferro-rs should feed the canonical verification work in upstream issues
[#114](https://github.com/QuantumBFS/quantum.harness/issues/114) and
[#115](https://github.com/QuantumBFS/quantum.harness/issues/115).
