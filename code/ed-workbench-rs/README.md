# Rewrite It In Rust! — Electronic Structure All the Way to CC(8)

[![CI](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/actions/workflows/ci.yml/badge.svg)](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust)](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/releases)
[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)

**451,681,246-determinant all-electron FCI. Arbitrary-order CC. Direct
integrals. One Rust workbench. RIIR!**

[Quantum Harness challenge #129](https://github.com/QuantumBFS/quantum.harness/issues/129)
asked for an exact-diagonalization workbench in Rust for electronic-structure
method development, with determinant-based arbitrary-order coupled cluster as
the mandatory showcase.

We did not stop at parsing FCIDUMP. We did not stop at a toy Hamiltonian. We
did not stop at CCSD.

We took the full frozen-core H2O/6-31G problem from determinants to
**CC(8)**—then kept climbing through CI, MBPT, direct `libcint` integrals,
RHF, and FCI.

**Rewrite it. Verify it. Push it to the FCI limit.**

## The Scoreboard

| Target | Result |
|---|---|
| Full H2O/6-31G frozen-core space | **245,025 determinants** |
| Direct Davidson FCI | `-76.121174204141980 E_h` |
| Extended H2O/DZP frozen-core space | **28,233,466 determinants** |
| Extended H2O/DZP Davidson FCI | **−76.256624441300147 Hartree** |
| H2O/cc-pVDZ all-electron C₂ᵥ A1 space | **451,681,246 determinants** |
| H2O/cc-pVDZ all-electron Davidson FCI | **−76.243218589558566 Hartree** |
| cc-pVDZ final residual / Slurm wall time | **6.602e-8 / 3:55:43** |
| Hirata 2000 CC table | **CC(1)-CC(8): 8/8 entries matched** |
| Hirata 2000 CI and MBPT tables | **28/28 entries matched** |
| CC(8) distance from FCI | `7.998e-9 E_h` |
| CI(8) distance from FCI | `2.004e-12 E_h` |
| Direct-integral production path | **libcint → RHF → AO-to-MO → FCI, no Python runtime** |

All published comparisons above match the precision printed by the paper.
Every headline result is backed by committed fixtures, machine-readable
evidence, reproduction commands, and a dedicated report.
The [data provenance register](reports/data-provenance.md) distinguishes
paper transcriptions, PySCF oracles, Rust results, and hardware measurements.

## This Is Not a Wrapper

The production algorithms run in Rust:

- FCIDUMP parsing, symmetry handling, and integral storage;
- alpha/beta determinant enumeration and fermionic signs;
- dense tiny-system Hamiltonians used as independent internal oracles;
- matrix-free spin-free sigma contraction;
- Davidson diagonalization with preconditioning and restart;
- block Davidson for several orthogonal low-energy roots;
- determinant-based CI(n), CC(n), MBPT(n), and unitary CC(n);
- exact excitation-rank subset convolution for `exp(T)|HF>`;
- direct `libcint` AO integrals, Rust RHF, DIIS, and AO-to-MO transformation.

PySCF is the independent oracle and fixture generator. It is used to challenge
the Rust implementation with known answers—not to execute the checked
production path.

## Post-Challenge Hardening

The validated challenge results are now a stable numerical floor rather than
the end of the engineering work. v0.2 adds:

- checked occupied/virtual active-space selection with both orbital maps;
- exact checked combinadic count, rank, and inverse-rank operations;
- explicit CC termination and non-finite-state diagnostics;
- schema-versioned `cc-series --json-output` evidence.

The established `freeze_core`, direct-FCI, Davidson, and CC commands remain
compatible. The Hamiltonian and every committed published comparison are
unchanged. See the [v0.2.0 release notes](docs/release-notes-v0.2.0.md).

v0.3 adds a versioned local-NVMe Davidson workspace:

- basis and sigma vectors no longer have to remain resident as a complete
  subspace;
- an interrupted calculation can resume from an atomically committed
  checkpoint;
- FCIDUMP fingerprints and numerical configuration prevent stale resumes;
- truncated, non-finite, unsafe, or incompatible state is rejected;
- a conservative solver-vector memory preflight runs before allocation.

```bash
cargo run --release --locked -- davidson \
  fixtures/h2o-631g-fc/FCIDUMP \
  --workspace /path/to/workspace \
  --checkpoint-every 1 \
  --memory-budget-gib 2 \
  --residual-tolerance 1e-7 \
  --max-iterations 60 \
  --max-subspace 20

cargo run --release --locked -- davidson \
  fixtures/h2o-631g-fc/FCIDUMP \
  --workspace /path/to/workspace \
  --resume \
  --checkpoint-every 1 \
  --memory-budget-gib 2 \
  --residual-tolerance 1e-7 \
  --max-iterations 100 \
  --max-subspace 20
```

Disk backing reduces Davidson subspace residency; it does not eliminate the
requirement that several full vectors fit in memory. This feature by itself
was not a claim of converged H2O/cc-pVDZ all-electron FCI; the later
symmetry-adapted 451,681,246-determinant Slurm calculation is documented
separately. See the
[v0.3.0 release notes](docs/release-notes-v0.3.0.md) and
[checkpoint format](docs/checkpoint-format.md).

v0.4 adds deterministic, budgeted CPU parallelism to direct-FCI sigma:

- fixed source blocks make results independent of Rayon scheduling;
- ordered reduction makes a fixed policy bitwise repeatable;
- memory is preflighted before thread-local vectors are allocated;
- strict mode rejects an insufficient budget; fallback mode explains why it
  used serial execution;
- serial remains the compatibility default.

On the 245,025-determinant primary H2O/6-31G problem, four source blocks and
10 Rayon workers reduced median sigma time from `14.181091542 s` to
`4.381184834 s` across five fresh release processes—a measured **3.236817x**
ratio of medians. Maximum serial/parallel difference was `5.969e-13`.

Raw measurements live in
[`parallel-sigma-m4.json`](fixtures/h2o-631g-fc/parallel-sigma-m4.json).
See the [v0.4.0 release notes](docs/release-notes-v0.4.0.md) and
[incremental validation report](reports/incremental-solver-validation.md).

## The Mission Is Complete

The mandatory #129 path is complete on the primary H2O/6-31G frozen-core
Hamiltonian. The original all-amplitude CC exponential bottleneck was
replaced by an exact excitation-rank subset-convolution recurrence, while the
finite Taylor expansion remains as an independent small-system oracle.

- Direct Davidson FCI converges in the full 245,025-determinant space.
- CC(1)-CC(8) matches all eight equilibrium CC entries in Hirata 2000
  Table 2 at the six decimals printed by the paper.
- CI(1)-CI(8) and MBPT(1)-MBPT(20) match all 28 corresponding Table 2
  entries.
- Independent 1.5 Rₑ and 2.0 Rₑ Hamiltonians converge through CC(8); Rust
  Davidson FCI agrees with PySCF within 2 × 10⁻¹² Hartree at both stretched
  geometries.
- The Level 0 oracle, mandatory Levels 1-2, stretch Levels 3-4, tenferro gap
  list, machine-readable evidence, and upstream reproduction materials are
  committed.

The Kállay 2001 all-electron DZ FCI extension is now reproduced with
1,002,708 determinants in the target symmetry sector: Rust gives
`-76.156699030930056` hartree versus PySCF
`-76.156699030929800` hartree. The frozen-core DZP extension is also complete:
Rust converges the 28,233,466-determinant sector to
−76.256624441300147 Hartree with a residual norm of 9.342 × 10⁻⁸, matching the
six decimals printed by Kállay 2001. No primary 6-31G result is presented as
evidence for a different Hamiltonian. RIIR means rewriting the
implementation—not rewriting the scientific claim.

The all-electron H2O/cc-pVDZ extension is also complete in the C₂ᵥ ground-state
A1 sector. Spatial symmetry reduces the fixed-`Nalpha=Nbeta=5` space from
1,806,590,016 to 451,681,246 determinants without freezing electrons or
orbitals. Slurm job `23008083` converged to
`-76.243218589558566` Hartree with residual `6.602e-8` in 3:55:43. An
independent same-geometry PySCF 2.14.0 calculation through CCSD(T), the exact
FCIDUMP checksum, unedited Slurm logs, and the literature precision boundary
are all committed.

Relative to the reviewer-requested no-point-group-symmetry benchmark, the
only intentional feasibility change in this new calculation is the exact
C₂ᵥ/A1 block reduction. The requested geometry, cc-pVDZ basis, all-electron
treatment, 24 spatial orbitals, ten electrons, singlet `Nalpha=Nbeta=5`
sector, Hamiltonian, and convergence criterion are otherwise unchanged.

## Quick Start

Build and test the Rust workbench:

```bash
cargo build --release
cargo test --locked
```

Reproduce the headline FCI result:

```bash
cargo run --release -- davidson fixtures/h2o-631g-fc/FCIDUMP \
  --residual-tolerance 1e-7 --max-iterations 60 --max-subspace 20
```

Reproduce the all-electron H2O/DZ extension:

```bash
cargo run --release -- davidson fixtures/h2o-dz-ae/FCIDUMP \
  --residual-tolerance 1e-7 --max-iterations 40 --max-subspace 20
```

Reproduce the frozen-core H2O/DZP extension (about 18 minutes and 7 GB):

```bash
cargo run --release -- davidson fixtures/h2o-dzp-fc/FCIDUMP \
  --residual-tolerance 1e-7 --max-iterations 40 --max-subspace 6
```

Reproduce the 2.0 Rₑ stretched-water CC sequence:

```bash
cargo run --release -- cc-series \
  fixtures/h2o-631g-fc-r2p0/FCIDUMP \
  fixtures/h2o-631g-fc-r2p0/reference.json \
  --max-rank 8 --residual-tolerance 1e-6 --max-iterations 100
```

Run every normal submission gate:

```bash
scripts/verify-submission.sh
```

Compute the lowest three H₄ roots and excitation energies:

```bash
cargo run --release -- davidson-roots fixtures/h4-sto3g/FCIDUMP \
  --roots 3 --residual-tolerance 1e-10 --max-subspace 12
```

## The Climb

### Level 0 — Make the Tiny Systems Tell the Truth

Before attacking 245,025 determinants, make every sign, index, and integral
convention prove itself on systems small enough to diagonalize explicitly.

Level 0 is complete for equilibrium and stretched H2 plus linear H4/STO-3G:

- PySCF-generated RHF, FCI, CCSD, FCIDUMP, and provenance/checksum artifacts;
- Rust FCIDUMP parsing with Fortran exponent and one-based Molpro
  `ORBSYM`/`ISYM` support;
- compact determinant enumeration in the requested Abelian symmetry sector;
- Rust alpha/beta determinant enumeration and fermionic operator signs;
- Rust explicit dense Hamiltonian construction and symmetric diagonalization;
- automatic `1e-10`-hartree verification against PySCF FCI.

Run the committed fixtures without Python:

```bash
cargo run -- inspect fixtures/h2-sto3g/FCIDUMP
cargo run -- verify fixtures/h2-equilibrium-sto3g/FCIDUMP \
  fixtures/h2-equilibrium-sto3g/reference.json
cargo run -- verify fixtures/h2-sto3g/FCIDUMP fixtures/h2-sto3g/reference.json
cargo run -- verify fixtures/h4-sto3g/FCIDUMP fixtures/h4-sto3g/reference.json
```

Regenerate the independent oracle only when needed. The project pins Python,
uv, PySCF, and every transitive Python dependency:

```bash
uv sync --locked
uv run --frozen python scripts/oracle/generate.py
```

The exact comparisons live in
[reports/level0-accuracy.md](reports/level0-accuracy.md).

### Level 1 — Stop Building the Matrix

Dense FCI established the truth. Matrix-free FCI made the real target
possible.

Level 1 direct FCI is complete:

- lexical alpha/beta string spaces and signed `E_pq` excitation links;
- matrix-free spin-free sigma contraction without storing the Hamiltonian;
- an independently computed Hamiltonian diagonal;
- Davidson with residual preconditioning and restart;
- H2O/STO-3G dense/direct/Davidson cross-validation;
- the 245,025-determinant frozen-core H2O/6-31G target.

```bash
cargo run --release -- davidson fixtures/h2o-631g-fc/FCIDUMP \
  --residual-tolerance 1e-7 --max-iterations 60 --max-subspace 20
```

The resulting energy is `-76.121174204141980` hartree with residual
`5.044e-8`, matching both PySCF and the published `-76.121174` anchor. See
[reports/level1-direct-fci.md](reports/level1-direct-fci.md).

### Level 2 — Climb from CC(1) to CC(8)

CCSD was a checkpoint, not the finish line.

Level 2 arbitrary-order determinant CC(n) is complete:

- runtime-configurable excitation rank;
- generic normalized cluster substitutions on full-FCI vectors;
- exact ranked subset-convolution construction of `exp(T)|HF>`, checked
  coefficient-by-coefficient against the independent finite Taylor path;
- projected energy and residual equations;
- orbital-denominator updates, DIIS, and determinant-indexed rank warm starts;
- CC(2) validation against PySCF CCSD;
- full CC(1)-CC(8) validation against Hirata 2000 Table 2.

```bash
RAYON_NUM_THREADS=10 cargo run --release -- cc-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-rank 8 --residual-tolerance 1e-6 --max-iterations 100
```

All eight published differences pass at the paper's six-decimal precision.
CC(2) is within `3.025e-10` hartree of PySCF CCSD; CC(8) is within
`7.998e-9` hartree of FCI.

See [reports/level2-cc-accuracy.md](reports/level2-cc-accuracy.md).

### Level 3 — Keep Going: CI, MBPT, and UCC

Once the determinant machinery is generic, one method family is not enough.

All three Level 3 method families are implemented:

- CI(n) as an excitation-rank projected matrix-free Davidson problem;
- arbitrary-order MBPT recursion with order-by-order corrections;
- variational unitary CC(n) using `exp(T-T†)` and BFGS optimization.

On the primary 245,025-determinant water target, every CI(1)-CI(8) and
MBPT(1)-MBPT(20) difference matches Hirata 2000 Table 2 at its printed
precision. CI(8) differs from FCI by `2.004e-12` hartree. H4 CI(4) and H2
UCC(2) additionally reproduce their small-system FCI limits.

```bash
RAYON_NUM_THREADS=10 cargo run --release -- level3-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-ci-rank 8 --max-mbpt-order 20 \
  --ci-residual-tolerance 1e-7 \
  --max-iterations 100 --max-subspace 24
```

See [reports/level3-methods.md](reports/level3-methods.md).

### Level 4 — Remove Python from the Production Path

The final climb starts before FCIDUMP: at the molecular geometry itself.

The direct-integral stack is complete for H2 and H2O/STO-3G:

- Rust calls `libcint` directly for overlap, kinetic, nuclear-attraction, and
  electron-repulsion integrals;
- Rust RHF implements symmetric orthogonalization, Coulomb/exchange Fock
  construction, DIIS, and convergence reporting;
- a staged AO-to-MO transformation feeds the shared matrix-free FCI solver;
- committed PySCF fixtures verify every AO and MO integral, RHF energies,
  orbital energies, and final FCI energies;
- the checked production commands have no Python runtime dependency.

The geometry and numerical units are explicit:

| Quantity | Unit |
|---|---|
| committed Cartesian input coordinates | Angstrom (`Å`) |
| PySCF/libcint internal coordinates | Bohr |
| total, orbital, nuclear-repulsion, and integral energies | Hartree (`E_h`) |
| overlap, MO coefficients, and CI/CC amplitudes | dimensionless |
| geometry angles | degree |

The equilibrium H2 fixture has `R(H-H)=0.7414 Å`. The original stretched-H2
fixture uses `z=-0.7 Å` and `z=+0.7 Å`, so its bond length is
`R(H-H)=1.4 Å`—not `0.7 Å`. Linear H4 has `1.0 Å` adjacent spacing. Both
water fixtures use `R(O-H)=0.967 Å` and `angle(H-O-H)=107.6°`.
The two stretched-water fixtures preserve the 107.6° angle and set both O–H
distances to 1.4505 Å and 1.934 Å, respectively.

```bash
cargo run --release -- rhf h2o-sto3g
cargo run --release -- direct-integrals-fci h2-sto3g
cargo run --release -- direct-integrals-fci h2o-sto3g
```

H2O/STO-3G gives RHF `-74.962663067690499` and direct FCI
`-75.012918738193051` hartree. The FCI error against PySCF is `1.485e-10`
hartree. See [reports/level4-integrals.md](reports/level4-integrals.md).

### Reviewer Follow-Up — H2O/cc-pVDZ, All Electrons, No Point-Group Symmetry

The requested larger-basis benchmark now runs the Rust integral, RHF,
AO-to-MO, determinant-link, and sparse Hamiltonian-column stages under a
2 GiB conservative preflight budget. On the Apple M4 validation machine it
used at most `447.25 MiB` RSS and completed in a median `1.42 s` across five
fresh release processes. Rust RHF differs from PySCF 2.14.0 by `6.230e-11 Eh`.

The exact fixed-`Nalpha=Nbeta=5` space contains `1,806,590,016` determinants.
One full CI vector alone is `13.460145 GiB`; the current 24-pair Davidson
subspace would require `646.086937 GiB`. The bounded command therefore does
not allocate full vectors or claim a converged cc-pVDZ FCI energy.

```bash
cargo run --release -- benchmark h2o-cc-pvdz \
  --sources 16 \
  --memory-budget-gib 2 \
  --json-output fixtures/h2o-ccpvdz-ae/benchmark-m4.json
```

See the
[full benchmark report](reports/h2o-ccpvdz-all-electron-benchmark.md) and
[machine-readable run](fixtures/h2o-ccpvdz-ae/benchmark-m4.json). The
[five-process summary](fixtures/h2o-ccpvdz-ae/benchmark-m4-summary.json)
records every raw timing and RSS observation plus aggregates that are
recomputed in the test suite.

`--memory-budget-gib` is a conservative preflight estimate, not an
operating-system hard memory limit. The v0.1.1 spelling
`--max-memory-gib` remains a supported alias.

The required ecosystem findings are recorded in
[reports/tenferro-gap-list.md](reports/tenferro-gap-list.md). tenferro-rs 0.2.0
already supplies dense tensors, strided views, gather/scatter, division,
reductions, and contractions; the primary determinant-workload gap is
collision-reducing scatter-add with explicit deterministic semantics.

## Verification: Trust, but Recompute

Install the fully locked Python oracle environment once:

```bash
uv sync --locked
```

Then run every normal submission gate—Rust formatting, Clippy, locked tests,
tracked-JSON validation, FCIDUMP checksums, Python unit/geometry tests, and
diff hygiene—with one command:

```bash
scripts/verify-submission.sh
```

Set `PYTHON=python3` when the pinned oracle dependencies are installed in the
active interpreter rather than `.venv`.

## What Got Rewritten

- Parse FCIDUMP files generated from PySCF.
- Enumerate alpha/beta determinant strings.
- Build tiny-system dense Hamiltonians for oracle checks.
- Implement string-based direct FCI and Davidson iteration.
- Implement determinant-based arbitrary-order CC(n).
- Implement CI(n), MBPT(n), and unitary CC(n) from the same operator machinery.
- Compute direct libcint AO integrals, RHF, and AO-to-MO transformations.
- Record Rust ecosystem and `tenferro-rs` gap notes discovered along the way.

## Follow the Evidence

- [Challenge brief](docs/challenge-129-brief.md) — upstream targets,
  deliverables, and grading anchors.
- [Implementation roadmap](docs/implementation-roadmap.md) — the full build
  sequence for this Rust repository.
- [Resources](docs/resources.md) — PySCF, libcint, Psi4NumPy, tenferro-rs,
  papers, and implementation references.
- [Web and GitHub snapshot](docs/web-and-github-snapshot.md) — dated upstream
  issue, PR, dependency, and documentation metadata.
- [Reproducibility notes](docs/reproducibility-notes.md) — conventions, APIs,
  targets, tolerances, and provenance rules.
- [Scientific data provenance](reports/data-provenance.md) — source class,
  Hamiltonian, oracle, literature, Rust output, and interpretation boundaries
  for every main dataset.
- [Standalone reproduction prompt](docs/reproduction-prompt.md) — exact
  revision, checksums, commands, tolerances, expected tables, and failure
  reporting.
- [Submission PR body](docs/submission-pr-body.md) — version-controlled source
  for the upstream solution PR description.
- [v0.2.0 release notes](docs/release-notes-v0.2.0.md) — active-space,
  combinadic, and CC diagnostic hardening.
- [v0.3.0 release notes](docs/release-notes-v0.3.0.md) — restartable,
  disk-backed Davidson.
- [v0.4.0 release notes](docs/release-notes-v0.4.0.md) — deterministic,
  budgeted parallel sigma.
- [v0.5.0 release notes](docs/release-notes-v0.5.0.md): symmetry-resolved
  large-scale FCI, extended exact benchmarks, and audited SCNet evidence.
- [Checkpoint format](docs/checkpoint-format.md) — schema, atomicity,
  validation, and memory planning.
- [Incremental solver validation](reports/incremental-solver-validation.md) —
  v0.2-v0.4 correctness, compatibility, and performance evidence.
- [v0.1.0 release notes](docs/release-notes-v0.1.0.md) — immutable inputs,
  headline values, verification commands, and release scope.
- [Sync log](docs/sync-log.md) — relationship between this workbench and the
  Quantum Harness solution PR.
- [Machine-readable upstream metadata](docs/upstream-metadata.json) — dated
  challenge and registration data.
- [Level 0 report](reports/level0-accuracy.md) — Rust-versus-PySCF tiny-system
  acceptance.
- [Level 1 report](reports/level1-direct-fci.md) — matrix-free Davidson water
  benchmarks.
- [Level 2 report](reports/level2-cc-accuracy.md) — arbitrary-order CC
  convergence and oracle comparisons.
- [Level 3 report](reports/level3-methods.md) — CI(n), MBPT(n), and unitary
  CC(n).
- [Level 4 report](reports/level4-integrals.md) — Python-free direct-integral
  pipeline and element-level PySCF comparisons.
- [Extended H2O/DZ report](reports/extended-h2o-dz.md) — exact historical
  basis/geometry reproduction and million-determinant all-electron FCI.
- [Extended H2O/DZP report](reports/extended-h2o-dzp.md) — compact
  28,233,466-determinant indexing, sigma-kernel scaling, and converged
  frozen-core FCI.
- [Stretched-water report](reports/stretched-water.md) — independent
  Davidson FCI and CC(1)-CC(8) checks at 1.5 Rₑ and 2.0 Rₑ.
- [Multi-root Davidson and UCC report](reports/multiroot-and-ucc.md) —
  dense-verified H₄ excited roots and a full-rank 35-parameter UCC check.
- [H2O/cc-pVDZ benchmark](reports/h2o-ccpvdz-all-electron-benchmark.md) —
  bounded no-point-group-symmetry timings, memory measurements, and the
  explicit full-space scalability boundary.
- [H2O/cc-pVDZ C₂ᵥ full FCI](reports/h2o-ccpvdz-c2v-fci.md) — converged
  451,681,246-determinant all-electron result, same-input PySCF cross-checks,
  Slurm provenance, and scientific interpretation.
- [SCNet HPC benchmark](reports/scnet-hpc-benchmark.md) — offline build gate,
  18-case Davidson robustness matrix, 216-solve repeatability evidence,
  observed concurrency, and the fail-closed 1,008-CPU gang submission.
- [tenferro gap list](reports/tenferro-gap-list.md) — current API coverage and
  proposed upstreamable reproducer work.

## Upstream Challenge

- Challenge issue: https://github.com/QuantumBFS/quantum.harness/issues/129
- Active solution PR: https://github.com/QuantumBFS/quantum.harness/pull/217
- Superseded registration PR: https://github.com/QuantumBFS/quantum.harness/pull/210
- Track folder: `tracks/ed/solutions/WangTheoPhys/`
- Registered team: **Rewrite It In Rust! (RIIR 2607 Hefei)**
- Members: Chenxi Wan, Yedi Shen, Junkai Wang
- Solution directory identifier: `WangTheoPhys`

## License

GNU Affero General Public License v3.0. See [LICENSE](LICENSE).

---

**From FCIDUMP to FCI. From CC(1) to CC(8). From Python oracle to a Rust
production path.**

## Rewrite It In Rust!
