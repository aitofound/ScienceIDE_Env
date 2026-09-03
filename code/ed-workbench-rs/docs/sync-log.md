# Sync Log

## 2026-07-27

Created the workbench repository privately during development, then published
it for reproducible challenge review:

- Repository: `JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust`
- Visibility: public as of 2026-07-27
- License: AGPL-3.0
- Local path: `/Users/thomasjwang/Documents/GitHub/quantum-harness-129-ed-workbench-rust`

Initialized Rust scaffold:

- `Cargo.toml`
- `Cargo.lock`
- `src/main.rs`
- `README.md`

Created public Quantum Harness registration:

- PR: https://github.com/QuantumBFS/quantum.harness/pull/210
- Branch: `challenge/ed-wangtheophys-rust-workbench`
- Team folder: `tracks/ed/solutions/WangTheoPhys/`
- Registered team: Rewrite It In Rust! (RIIR 2607 Hefei)
- Members: Chenxi Wan, Yedi Shen, Junkai Wang
- Solution directory identifier: WangTheoPhys
- Catalog issue: https://github.com/QuantumBFS/quantum.harness/issues/129

The original registration PR #210 was later closed and superseded by the
active solution PR
[#217](https://github.com/QuantumBFS/quantum.harness/pull/217), using the same
head branch and solution directory.

## 2026-07-30

Prepared the v0.5.0 public release and PR #217 synchronization:

- preserved every primary H2O/6-31G FCI, CC, CI, and MBPT acceptance result;
- published the converged 451,681,246-determinant all-electron H2O/cc-pVDZ
  C₂ᵥ A1 calculation;
- recorded that symmetry reduction is the only intentional feasibility
  change and that every other reviewer-requested condition remains unchanged;
- archived the exact FCIDUMP, PySCF cross-check, Slurm logs, resource
  accounting, and data provenance;
- integrated the SCNet offline build, robustness, repeatability, and observed
  560-CPU evidence with its explicit thousand-CPU claim boundary;
- prepared a current PR body and a version-controlled progress comment that
  point reviewers to the v0.5.0 release rather than the earlier v0.1.1
  boundary.

Synced useful upstream context into this repository:

- Challenge metadata and deliverables in `docs/challenge-129-brief.md`.
- Implementation milestones in `docs/implementation-roadmap.md`.
- GitHub, tool, and literature resource index in `docs/resources.md`.

Second-pass web and GitHub sync:

- Re-read the complete upstream issue through the GitHub API.
- Captured PR #210 state, commits, changed file, branch, registered team, and
  mergeability.
- Corrected the registered team to `Rewrite It In Rust! (RIIR 2607 Hefei)` with
  members Chenxi Wan, Yedi Shen, and Junkai Wang. `WangTheoPhys` remains the
  solution-directory identifier.
- Added #114/#115 only as narrow context for #129's required tenferro-rs gap
  reporting.
- Added current repository metadata and release discovery for PySCF, libcint,
  Psi4NumPy, tenferro-rs, Quantum Package, faer, and argmin.
- Added official PySCF, libcint-crate, and tenferro documentation entry points.
- Added `docs/web-and-github-snapshot.md` and
  `docs/reproducibility-notes.md`.
- Expanded `docs/upstream-metadata.json` with source timestamps and states.

Completed the Level 0 tiny-system oracle loop:

- Added a pinned PySCF 2.14.0 generator under `scripts/oracle/`.
- Committed deterministic H2 and linear H4/STO-3G FCIDUMP/reference fixtures.
- Added Rust FCIDUMP, determinant, dense Hamiltonian, eigensolver, reference,
  and CLI modules.
- Verified Rust dense FCI against PySCF to `0.0` hartree for H2 and
  `6.217e-15` hartree for H4.
- Added checksum verification, CLI regression tests, and
  `reports/level0-accuracy.md`.

Completed Levels 1-4:

- Added matrix-free spin-free direct FCI and Davidson, including the
  245,025-determinant frozen-core H2O/6-31G target.
- Added arbitrary-rank determinant CC(n), CI(n), MBPT(n), and unitary CC(n).
- Added the direct Rust `libcint -> RHF/DIIS -> AO-to-MO -> FCI` pipeline for
  H2 and H2O/STO-3G.
- Added complete AO/MO integral fixtures and element-by-element PySCF
  comparisons.
- Re-checked the current tenferro-rs 0.2.0 repository, tensor API, indexing
  trait, and memory-order documentation.
- Recorded the final indexed scatter-add, mutable/output-buffer, BLAS-1,
  layout, numerical, performance, and API findings in
  `reports/tenferro-gap-list.md`.

Made geometry and numerical units explicit:

- Added `coordinate_unit` and machine-readable geometry parameters to every
  oracle system and reference JSON.
- Recorded that equilibrium H2 uses `R(H-H)=0.7414 Angstrom`, stretched H2
  uses `R(H-H)=1.4 Angstrom`, linear H4 uses `1.0 Angstrom` adjacent spacing,
  and water uses `R(O-H)=0.967 Angstrom` with `HOH=107.6 degrees`.
- Added a typed Rust `CoordinateUnit` propagated into libcint and printed both
  coordinate and Hartree energy units in the CLI.
- Added Python and Rust geometry regressions so coordinate positions cannot be
  confused with bond lengths.

Completed the primary published-table acceptance:

- Transcribed the equilibrium CI, MBPT, and CC columns of Hirata 2000 Table 2
  with DOI, page, settings, printed precision, and a strict loader.
- Replaced the production CC exponential with an exact ranked
  subset-convolution recurrence while retaining Taylor expansion as an
  independent small-system oracle.
- Added warm-started `cc-series` and `level3-series` commands with
  precision-aware published verification.
- Ran CC(1)-CC(8), CI(1)-CI(8), and MBPT(1)-MBPT(20) on the
  245,025-determinant H2O/6-31G frozen-core target; all 36 paper entries pass
  at the six decimal places actually printed.
- Committed exact machine-readable values, performance and environment
  records, regression tests, accuracy reports, and the standalone
  `docs/reproduction-prompt.md`.

Completed the final submission audit:

- Published the dedicated AGPL-3.0 workbench and confirmed anonymous HTTP
  access returns `200`.
- Re-ran primary matrix-free Davidson FCI:
  `-76.121174204141980 hartree`, residual `5.044e-8`, 16 iterations.
- Previously completed live primary runs remain locked in committed
  machine-readable records: CC(1)-CC(8) in 186.94 seconds and combined
  CI(1)-CI(8)/MBPT(1)-MBPT(20) in 190.08 seconds on an Apple M4 with 10 Rayon
  workers.
- `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`,
  `cargo test --locked`, `git diff --check`, and five Python geometry/unit
  regressions all passed. The Rust suite reported 57 passing tests and two
  intentionally ignored long live-primary tests; both corresponding live
  calculations were executed separately.
- Validated every tracked JSON document with `jq`, checked all five FCIDUMP
  files against their recorded SHA-256 values, and confirmed no pre-existing
  FCIDUMP or numerical reference JSON changed during the final implementation.
- Confirmed solution PR
  [#217](https://github.com/QuantumBFS/quantum.harness/pull/217) is open and
  mergeable and contains both the full design README and standalone
  reproduction prompt.
