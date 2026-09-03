# Reproducibility Notes

This document converts the upstream web material into implementation checks.
It does not replace the cited papers or official API documentation.
The repository-wide mapping from every main artifact to its paper, PySCF
oracle, Rust calculation, or performance measurement is in
[`reports/data-provenance.md`](../reports/data-provenance.md).

## Primary Molecular Target

- Molecule: H2O.
- Geometry: `r_OH = 0.967 Angstrom`, `HOH = 107.6 degrees`.
- Basis: 6-31G.
- Orbitals: restricted Hartree-Fock canonical molecular orbitals.
- Frozen core: oxygen 1s.
- Expected FCI energy: `-76.121174 hartree`.

## Explicit Unit Contract for Committed Fixtures

- Every Cartesian coordinate string in `scripts/oracle/generate.py` is in
  Angstrom and is passed to PySCF with `unit=system.coordinate_unit`.
- PySCF and libcint convert those coordinates to Bohr internally.
- Equilibrium H2 uses `R(H-H)=0.7414 Angstrom`.
- The stretched-H2 fixture uses positions `-0.7` and `+0.7` Angstrom, which
  means `R(H-H)=1.4 Angstrom`.
- Linear H4 has `1.0 Angstrom` between adjacent atoms.
- The equilibrium water fixtures use an O–H distance of 0.967 Angstrom and
  an H–O–H angle of 107.6 degrees.
- The stretched 6-31G frozen-core fixtures preserve the 107.6 degree angle
  and use O–H distances of 1.4505 Angstrom and 1.934 Angstrom.
- Energies, orbital energies, nuclear repulsion, and energy-valued integrals
  are in Hartree. AO overlap, MO coefficients, and CI/CC amplitudes are
  dimensionless.

Extended targets:

- Water/6-31G with frozen oxygen 1s at 1.5 and 2.0 times the equilibrium O–H
  distance. Both Davidson FCI and CC(1)-CC(8) are reproduced and documented
  in `reports/stretched-water.md`.
- Water/DZ, all electrons: `-76.156699 hartree`. This target is now
  reproduced in `fixtures/h2o-dz-ae` and documented in
  `reports/extended-h2o-dz.md`.
- Water/DZP, frozen oxygen 1s: −76.256624 Hartree. This target is now
  reproduced with a converged 28,233,466-determinant Rust calculation and
  documented in `reports/extended-h2o-dzp.md`.
- Water/cc-pVDZ, all ten electrons, C₂ᵥ A1: the
  451,681,246-determinant Rust Davidson calculation converges to
  −76.243218589558566 Hartree with residual 6.602 × 10⁻⁸. The exact input,
  same-geometry PySCF RHF-through-CCSD(T) cross-check, Slurm logs, and
  interpretation are documented in `reports/h2o-ccpvdz-c2v-fci.md`.
- Reproduce the geometry and basis definitions from Bauschlicher 1986 rather
  than substituting similarly named modern basis-library entries without
  comparison.

## Oracle Generation

The upstream recommended path is:

1. Run restricted Hartree-Fock in PySCF.
2. Export molecular-orbital integrals with
   `pyscf.tools.fcidump.from_scf(mf, "FCIDUMP")`.
3. Compute and store HF, FCI, and CCSD reference energies as JSON.
4. Record geometry, basis text/name, charge, spin, frozen orbitals, PySCF
   version, convergence thresholds, and integral checksum alongside every
   fixture.
5. Keep a comparison script that fails when Rust results exceed stated
   tolerances.

Official FCIDUMP documentation says `from_scf` transforms the one- and
two-electron integrals using the supplied SCF orbitals. `read` returns at least
`H1`, `H2`, `ECORE`, `NORB`, `NELEC`, `MS`, `ORBSYM`, and `ISYM`.

## FCIDUMP Parser Contract

Tests should cover these facts explicitly:

- Input orbital indices are one-based; internal indices should be zero-based.
- `k != 0` denotes a two-electron-integral record.
- `k == 0 && j != 0` denotes a one-electron-integral record.
- `k == 0 && j == 0` denotes the core/nuclear energy.
- Two-electron integrals use spatial-orbital Mulliken ordering with eightfold
  permutation symmetry.
- The header includes electron/orbital/spin and optional symmetry metadata.
- PySCF's writer defaults visible in its source are a `1e-15` write threshold
  and `%.16g`-style numeric formatting; fixtures should record any override.

## Determinant and Hamiltonian Verification

- Represent alpha and beta strings independently.
- Lexically rank occupied-orbital combinations without gaps and implement the
  inverse unranking operation.
- Exhaustively compare every H2/H4 matrix element and fermionic sign against a
  dense reference before moving to water.
- Confirm Hermiticity and diagonal elements independently.
- Compare dense `H C` and direct sigma-vector results for random vectors.

## Davidson Checks

- Start with the ground state and diagonal preconditioning.
- Track Ritz energy, residual norm, subspace size, restart count, and
  orthogonality.
- Verify the iterative result against the tiny dense eigensolver.
- Do not claim a published-energy match unless the residual and energy
  tolerances are both reported.

## CC(n) Checks

- Keep amplitudes through excitation level `n`.
- Apply `T` using the same determinant/string machinery.
- Retain the Taylor-series implementation as an independent small-system
  oracle. In production CC, compute `exp(T)|HF>` with the exact
  excitation-rank subset recurrence and compare every H2/H4 coefficient
  against Taylor expansion.
- Evaluate the energy and projected residuals on determinants through level
  `n`.
- Use orbital-energy denominators for Jacobi updates and DIIS acceleration.
- First verify CC(2) against PySCF CCSD, then compare CC(1)-CC(8) on the
  primary H2O/6-31G frozen-core fixture to Hirata 2000 Table 2.
- Use residual norm `< 1e-6` as the published baseline convergence criterion;
  tighter internal tests are encouraged.

## CI(n) and MBPT(n) Published Checks

- Use the same H2O/6-31G frozen-core FCIDUMP, FCI reference energy, geometry,
  active space, and unit contract as CC(n).
- CI(n) is the variational diagonalization in the determinant space containing
  excitations through rank `n`. Warm-start each Davidson solve from the
  preceding rank, require residual norm at most `1e-7`, and verify that the
  energy is non-increasing through CI(8).
- CI(8) spans all 245,025 determinants and is therefore an independent
  full-space check of the Level 1 direct-FCI result.
- MBPT(n) uses the canonical RHF Fock diagonal as `H0` and records both each
  correction and every partial sum through order 20.
- Compare `E(method)-E(FCI)` with the equilibrium CI and MBPT columns of Hirata
  2000 Table 2. The paper prints six decimal places, so compare rounded
  microhartree integers rather than claiming unavailable paper precision.
- The complete committed records are
  `fixtures/h2o-631g-fc/cc_series_results.json` and
  `fixtures/h2o-631g-fc/level3_series_results.json`; their regression tests
  check the settings, order coverage, convergence, arithmetic, and all
  published matches.

## tenferro-rs Evaluation

The dense GEMM-like integral-contraction block may fit tenferro. The
determinant-driven kernels also require operations that deserve explicit
evaluation:

- indexed gather/scatter-add with sign changes;
- mutable sliced views;
- element-wise denominator division;
- in-place axpy, dot, and norm operations;
- predictable column/row-major conversion;
- small-operation dispatch overhead;
- CPU parallelism and reproducible reductions.

Check the current
[supported-operation inventory](https://tensor4all.org/tenferro-rs/design/supported-ops.html)
before calling something missing. Every reported gap should include a minimal
reproducer, sizes, hardware/software versions, expected behavior, observed
behavior, and—when performance-related—a comparison with a reference backend.

## Minimum Provenance Record

Every published result should carry:

- git commit;
- Rust compiler and Cargo.lock;
- Python and PySCF versions;
- OS, CPU, thread count, and relevant BLAS/backend;
- complete molecular input and frozen-orbital definition;
- fixture/integral checksum;
- algorithm thresholds and iteration limits;
- final energy, residual norm, iteration count, and wall time;
- source table/DOI when comparing with literature.
