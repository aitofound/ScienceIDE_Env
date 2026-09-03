# Implementation Roadmap

This roadmap translates Quantum Harness #129 into repo-sized milestones.

## Phase 0 - Project Shape

- Keep the Rust engine under `src/`.
- Put Python oracle generation scripts under `scripts/oracle/`.
- Put generated FCIDUMP and small reference JSON fixtures under `fixtures/`.
- Put benchmark and accuracy tables under `reports/`.
- Keep implementation notes and design records under `docs/`.

## Phase 1 - FCIDUMP and Determinant Basis

- Implement an FCIDUMP parser for spatial-orbital one- and two-electron
  integrals.
- Preserve Mulliken integral ordering and 1-based input indexing at the parser
  boundary, then convert to zero-based internal indices.
- Represent alpha and beta occupation strings as integer bitsets.
- Implement combination ranking/unranking for lexical determinant addressing.
- Add exhaustive tests for H2 and H4 basis enumeration.

## Phase 2 - Dense Tiny-System Oracle

- Generate PySCF fixtures for H2 and H4.
- Build explicit Hamiltonian matrices in Rust for tiny systems.
- Diagonalize densely using a small linear algebra backend.
- Compare Rust energies to PySCF FCI energies in CI.

## Phase 3 - String-Based Direct FCI

- Precompute excitation lists and fermion signs.
- Implement sigma = H C without storing H.
- Add Davidson iteration with diagonal preconditioning.
- Validate against dense tiny-system results before running water.

## Phase 4 - Determinant-Based CC(n)

- Store cluster amplitudes by excitation level.
- Apply T to FCI-length vectors.
- Build exp(T)|HF> by Taylor expansion with a norm cutoff.
- Evaluate projected residuals.
- Use denominator updates and DIIS acceleration.
- Verify CC(2) against PySCF CCSD, then compare higher orders to Hirata 2000.

## Phase 5 - Reporting and Gaps

- Produce a reproducible accuracy table for FCI and CC(n).
- Record each operation that does or does not map cleanly to `tenferro-rs`.
- Keep a public-facing summary ready for the Quantum Harness PR.

