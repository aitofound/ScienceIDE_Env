# Primary H2O CC(n) Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete Quantum Harness #129's primary H2O/6-31G frozen-core arbitrary-order CC validation through CC(8), validate the attempted CI and MBPT series against Hirata 2000 Table 2, and deliver reproducible evidence to the open upstream solution PR.

**Architecture:** Replace repeated all-amplitude cluster scans in the CC exponential with an exact excitation-rank subset-convolution recurrence built from alpha/beta string partitions. Add a warm-started CC rank-series API and CLI, validate computed method-minus-FCI differences against a machine-readable transcription of Hirata 2000 Table 2, and commit the resulting accuracy and performance reports.

**Tech Stack:** Rust 2024, existing determinant/string/direct-FCI/DIIS modules, Clap, Serde/serde_json, Python/PySCF oracle tests, Cargo, GitHub CLI.

## Global Constraints

- Primary system is H2O/6-31G with oxygen 1s frozen.
- Geometry is `R(O-H)=0.967 angstrom`, `angle(H-O-H)=107.6 degree`.
- Active space is 12 spatial orbitals, 8 electrons, and 245,025 determinants.
- Energies and energy differences are in Hartree.
- CC convergence requires residual norm at most `1e-6`.
- Published values retain only the six decimal places printed in Hirata 2000 Table 2.
- Python remains oracle/test-only; production CC, CI, and MBPT execution is Rust.
- Preserve all FCIDUMP bytes and existing numerical reference fields.
- Keep the existing single-rank `cc` CLI compatible.
- Every implementation task follows red-green-refactor and ends in a focused commit.

---

### Task 1: Commit the Published Table 2 Reference

**Files:**
- Create: `fixtures/h2o-631g-fc/hirata2000-table2.json`
- Create: `src/published_reference.rs`
- Modify: `src/lib.rs`
- Create: `tests/published_reference.rs`

**Interfaces:**
- Consumes: committed H2O/6-31G reference settings.
- Produces: `HirataTable2::load(path) -> Result<HirataTable2, PublishedReferenceError>`.
- Produces: `HirataTable2::difference(series: SeriesKind, order: usize) -> Option<f64>`.
- Produces: `rounded_published_match(computed: f64, published: f64, decimals: u32) -> bool`.

- [x] **Step 1: Write the failing published-reference tests**

Test exact metadata, all required rank ranges, representative values, and
six-decimal rounded comparison:

```rust
#[test]
fn loads_hirata_table2_equilibrium_series() {
    let table = HirataTable2::load(fixture_path()).unwrap();
    assert_eq!(table.energy_unit, "hartree");
    assert_eq!(table.printed_decimals, 6);
    assert_eq!(table.difference(SeriesKind::Cc, 2), Some(0.001545));
    assert_eq!(table.difference(SeriesKind::Cc, 8), Some(0.0));
    assert_eq!(table.difference(SeriesKind::Ci, 4), Some(0.000175));
    assert_eq!(table.difference(SeriesKind::Mbpt, 10), Some(0.000003));
}

#[test]
fn comparison_respects_the_papers_printed_precision() {
    assert!(rounded_published_match(0.0015446852, 0.001545, 6));
    assert!(!rounded_published_match(0.0015439, 0.001545, 6));
}
```

- [x] **Step 2: Run the tests and verify failure**

Run:

```bash
cargo test --test published_reference
```

Expected: compile failure because `published_reference` does not exist.

- [x] **Step 3: Add the machine-readable Table 2 transcription**

Record the equilibrium method-minus-FCI differences:

```json
{
  "schema_version": 1,
  "citation": "S. Hirata and R. J. Bartlett, Chemical Physics Letters 321, 216-224 (2000)",
  "doi": "10.1016/S0009-2614(00)00387-0",
  "table": 2,
  "page": 222,
  "quantity": "method_total_energy_minus_fci_total_energy",
  "energy_unit": "hartree",
  "printed_decimals": 6,
  "system": {
    "name": "H2O",
    "basis": "6-31G",
    "frozen_orbitals": [0],
    "bond_length_angstrom": 0.967,
    "bond_angle_degree": 107.6,
    "active_spatial_orbitals": 12,
    "active_electrons": 8,
    "determinants": 245025
  },
  "equilibrium": {
    "ci": [0.136671, 0.006858, 0.005854, 0.000175, 0.000103, 0.000001, 0.0, 0.0],
    "mbpt": [0.136671, 0.008215, 0.006577, 0.001300, 0.000583, 0.000178, 0.000085, 0.000022, 0.000014, 0.000003, 0.000002, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    "cc": [0.136671, 0.001545, 0.000449, 0.000012, 0.000003, 0.0, 0.0, 0.0]
  }
}
```

- [x] **Step 4: Implement strict loading and rounded comparison**

Use Serde structs, reject missing orders through array-length checks in
`validate`, and compare by rounding both values to integer microhartree:

```rust
pub fn rounded_published_match(computed: f64, published: f64, decimals: u32) -> bool {
    let scale = 10_f64.powi(decimals as i32);
    (computed * scale).round() == (published * scale).round()
}
```

- [x] **Step 5: Run tests and commit**

Run:

```bash
cargo fmt --check
cargo test --test published_reference
git diff --check
```

Commit:

```bash
git add fixtures/h2o-631g-fc/hirata2000-table2.json src/published_reference.rs src/lib.rs tests/published_reference.rs
git commit -m "Add Hirata Table 2 reference data"
```

---

### Task 2: Build the Ranked Cluster Expansion

**Files:**
- Modify: `src/cluster.rs`
- Test: `src/cluster.rs`

**Interfaces:**
- Consumes: `DeterminantBasis`, `ExcitationSpace`, and `Amplitudes`.
- Produces: `ClusterExpansionPlan::new(basis, space) -> Result<Self, ClusterError>`.
- Produces: `ClusterExpansionPlan::exponential_on_reference(amplitudes) -> Result<Vec<f64>, ClusterError>`.
- Preserves: `ClusterOperator::apply`, `apply_adjoint`, and the Taylor implementation used by UCC.

- [x] **Step 1: Add a failing recurrence-vs-Taylor test**

For H2 and H4, construct deterministic nonzero amplitude vectors at every
rank, compare every wavefunction coefficient, and require maximum error below
`1e-12`:

```rust
for max_rank in 1..=basis.nalpha + basis.nbeta {
    let space = ExcitationSpace::new(&basis, reference, max_rank).unwrap();
    let amplitudes = deterministic_amplitudes(&space);
    let expected = ClusterOperator::new(&basis, &space, &amplitudes)
        .unwrap()
        .exponential_on_reference_taylor(1e-15)
        .unwrap();
    let actual = ClusterExpansionPlan::new(&basis, &space)
        .unwrap()
        .exponential_on_reference(&amplitudes)
        .unwrap();
    assert!(max_error(&actual, &expected) < 1e-12);
}
```

- [x] **Step 2: Add a failing exhaustive sign-factorization test**

On all determinant targets for spaces through four spatial orbitals, compare
each alpha/beta partition's factored phase with
`Excitation::apply(source).1`.

- [x] **Step 3: Run the focused tests and verify failure**

Run:

```bash
cargo test cluster::tests -- --nocapture
```

Expected: compile failure because `ClusterExpansionPlan` is absent.

- [x] **Step 4: Implement spin subset partitions**

Add the private representation:

```rust
#[derive(Debug, Clone, Copy)]
struct SpinPartition {
    amplitude_string: usize,
    source_string: usize,
    rank: usize,
    phase: i8,
}
```

For each target spin string, enumerate equal-cardinality subsets of reference
holes and virtual particles. Store the amplitude string address, complementary
source string address, excitation rank, and phase normalized against the
reference action.

- [x] **Step 5: Implement the ranked recurrence plan**

Add:

```rust
pub struct ClusterExpansionPlan<'a> {
    basis: &'a DeterminantBasis,
    space: &'a ExcitationSpace,
    amplitude_by_determinant: Vec<Option<usize>>,
    alpha_partitions: Vec<Vec<SpinPartition>>,
    beta_partitions: Vec<Vec<SpinPartition>>,
    targets_by_rank: Vec<Vec<usize>>,
}
```

Set the reference coefficient to one. For targets in increasing total
excitation rank, combine alpha and beta partitions, reject total amplitude
rank zero or above `space.max_rank`, and apply:

```rust
wavefunction[target] += amplitude_rank as f64
    * amplitudes.values[amplitude_index]
    * wavefunction[source]
    * phase as f64;
wavefunction[target] /= target_rank as f64;
```

- [x] **Step 6: Run correctness and regression tests**

Run:

```bash
cargo fmt
cargo test cluster::tests
cargo test --test level2
```

Expected: recurrence matches Taylor below `1e-12`; all existing CC results
remain within their current tolerances.

- [x] **Step 7: Commit**

```bash
git add src/cluster.rs
git commit -m "Accelerate cluster exponential by ranked convolution"
```

---

### Task 3: Use the Ranked Expansion in CC and Add Warm-Started Series

**Files:**
- Modify: `src/coupled_cluster.rs`
- Modify: `tests/level2.rs`

**Interfaces:**
- Produces: `solve_cc_series(operator, max_rank, orbital_energies, config) -> Result<Vec<CcSeriesEntry>, CcError>`.
- Produces: `CcSeriesEntry { rank: usize, result: CcResult, elapsed: Duration }`.
- Preserves: `solve_cc` signature and behavior.

- [x] **Step 1: Add failing single-rank equivalence and warm-start tests**

Require:

```rust
let single = solve_cc(&operator, 2, &energies, &config).unwrap();
let series = solve_cc_series(&operator, 2, &energies, &config).unwrap();
assert!((single.energy - series[1].result.energy).abs() < 1e-11);
assert!(series[1].result.iterations.len() <= single.iterations.len());
```

Also reject `max_rank == 0` and a maximum above
`basis.nalpha + basis.nbeta`.

- [x] **Step 2: Run and verify failure**

Run:

```bash
cargo test --test level2 warm_started_cc_series -- --nocapture
```

Expected: compile failure because `solve_cc_series` is absent.

- [x] **Step 3: Route CC energy evaluation through the recurrence**

Build one `ClusterExpansionPlan` before the iteration loop and pass it into
`energy_and_residual`. Remove repeated construction of a Taylor cluster
operator from the CC path; do not alter UCC.

- [x] **Step 4: Implement determinant-indexed warm starts**

Maintain:

```rust
let mut previous_by_determinant = vec![0.0; basis.len()];
```

For a new excitation space, initialize each amplitude from
`previous_by_determinant[excitation.determinant_index]`. After convergence,
write every converged amplitude back by determinant index. Record elapsed
wall-clock duration for each rank.

- [x] **Step 5: Run Level 2 and all unit tests**

Run:

```bash
cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test --test level2
cargo test --lib
```

- [x] **Step 6: Commit**

```bash
git add src/coupled_cluster.rs tests/level2.rs
git commit -m "Add warm-started arbitrary-order CC series"
```

---

### Task 4: Add the CC Series CLI and Published Acceptance

**Files:**
- Modify: `src/main.rs`
- Create: `tests/cc_series.rs`

**Interfaces:**
- Produces CLI:

```text
ed_workbench_rs cc-series FCIDUMP REFERENCE
    --published-reference PUBLISHED_REFERENCE
    --max-rank 8
    --residual-tolerance 1e-6
    --max-iterations 100
```

- Produces one tab-separated result row per rank plus a final
  `published verification: PASS` line.

- [x] **Step 1: Add a failing small-fixture CLI test**

Use H4 without a published reference and require fields for rank, energy,
method-minus-FCI, iterations, residual, elapsed seconds, and convergence.
Exercise the strict published-reference path separately on primary-system
CC(1), which is inexpensive but validates all context checks.

- [x] **Step 2: Run and verify failure**

Run:

```bash
cargo test --test cc_series
```

Expected: Clap rejects the unknown `cc-series` command.

- [x] **Step 3: Implement the command and validation**

Load the normal fixture reference and Hirata table, verify system settings,
run `solve_cc_series`, calculate:

```rust
let difference = result.energy - reference.fci_energy;
let published = table.difference(SeriesKind::Cc, rank).unwrap();
let matches = rounded_published_match(difference, published, table.printed_decimals);
```

Fail the command if any rank does not converge or does not round to the
published value.

- [x] **Step 4: Run CLI tests and existing CLI regressions**

Run:

```bash
cargo test --test cc_series
cargo test --test level0
cargo test --test level2
cargo test --test level4
```

- [x] **Step 5: Commit**

```bash
git add src/main.rs tests/cc_series.rs
git commit -m "Add published CC series verification command"
```

---

### Task 5: Run and Lock the Primary CC(1)-CC(8) Series

**Files:**
- Create: `fixtures/h2o-631g-fc/cc_series_results.json`
- Modify: `tests/cc_series.rs`
- Modify: `reports/level2-cc-accuracy.md`
- Modify: `docs/reproducibility-notes.md`

**Interfaces:**
- Consumes: release `cc-series` output.
- Produces: committed result rows with energies, differences, iterations,
  residuals, timings, and peak-memory measurement.

- [x] **Step 1: Benchmark CC(2) before and after**

Run:

```bash
/usr/bin/time -lp target/release/ed_workbench_rs cc \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --rank 2 --residual-tolerance 1e-6
```

Record the six-second one-iteration pre-change baseline from the design and
the full converged post-change runtime.

- [x] **Step 2: Run the complete primary series**

Run:

```bash
/usr/bin/time -lp target/release/ed_workbench_rs cc-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-rank 8 --residual-tolerance 1e-6 --max-iterations 100
```

Expected: all eight ranks converge and every difference rounds to the Table 2
equilibrium value.

- [x] **Step 3: Commit machine-readable results**

Store command, git commit, hardware description, units, total energies,
method-minus-FCI differences, published differences, errors, iteration counts,
residual norms, elapsed times, and peak memory in
`cc_series_results.json`.

- [x] **Step 4: Turn the primary series into a regression**

Load the committed result file in `tests/cc_series.rs`. Verify rank coverage,
residuals, and published rounded matches without rerunning the full expensive
series in the default test suite. Mark the live calculation as an ignored test
with an exact reproduction command.

- [x] **Step 5: Update the Level 2 report**

Replace the small-system-only completion claim with a primary acceptance
table. Explain the method-minus-FCI sign convention, printed precision,
convergence thresholds, warm-start behavior, runtime, and peak memory.

- [x] **Step 6: Commit**

```bash
git add fixtures/h2o-631g-fc/cc_series_results.json tests/cc_series.rs reports/level2-cc-accuracy.md docs/reproducibility-notes.md
git commit -m "Validate water CC series against Hirata Table 2"
```

---

### Task 6: Validate the Attempted CI and MBPT Series

**Files:**
- Modify: `src/main.rs`
- Create: `fixtures/h2o-631g-fc/level3_series_results.json`
- Create: `tests/level3_primary.rs`
- Modify: `reports/level3-methods.md`

**Interfaces:**
- Consumes: existing `solve_ci` and `solve_mbpt`.
- Produces: primary H2O CI(1)-CI(8) and MBPT(1)-MBPT(20) result records.

- [x] **Step 1: Add published-comparison helpers to CI and MBPT output**

Add a `published-series` CLI command or a shared internal runner that evaluates
the requested series and emits method-minus-FCI differences with the same
precision-aware acceptance used by CC.

- [x] **Step 2: Run MBPT(1)-MBPT(20)**

Run:

```bash
target/release/ed_workbench_rs mbpt \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --order 20
```

Compare every partial-sum difference with the equilibrium MBPT column.

- [x] **Step 3: Run CI(1)-CI(8)**

Run each rank with a residual tolerance no looser than `1e-7`; reuse the
already validated full-rank Davidson result for CI(8) if the implementation
identifies the spaces as identical.

- [x] **Step 4: Commit result data and regression tests**

The test loads `level3_series_results.json`, requires all orders, verifies the
CI variational sequence, residual tolerances, and all six-decimal published
matches.

- [x] **Step 5: Update the Level 3 report and commit**

```bash
git add src/main.rs fixtures/h2o-631g-fc/level3_series_results.json tests/level3_primary.rs reports/level3-methods.md
git commit -m "Validate water CI and MBPT series against Hirata"
```

---

### Task 7: Complete Reproduction and Upstream Solution Materials

**Files:**
- Modify: `README.md`
- Modify: `scripts/oracle/README.md`
- Modify: `docs/sync-log.md`
- Create: `docs/reproduction-prompt.md`
- Modify in upstream PR branch: `tracks/ed/solutions/WangTheoPhys/README.md`
- Create in upstream PR branch: `tracks/ed/solutions/WangTheoPhys/reproduction-prompt.md`

**Interfaces:**
- Produces: one-command local reproduction paths.
- Produces: final challenge solution README and required reproduction prompt.

- [x] **Step 1: Write the reproduction prompt**

The prompt must state the repository, commit, toolchain, fixture checksums,
units, geometry, frozen core, commands, tolerances, expected FCI/CC/CI/MBPT
tables, and how failures are reported. It must not depend on private chat
history.

- [x] **Step 2: Update local documentation**

Explain architecture decisions, exact primary acceptance results, performance,
limitations, the direct-libcint stretch path, and tenferro gap-list link.

- [x] **Step 3: Fetch the upstream PR branch safely**

Use a separate temporary clone or worktree. Confirm PR #217's head branch and
current commit before modifying only
`tracks/ed/solutions/WangTheoPhys/`.

- [x] **Step 4: Update the upstream solution directory**

The README must include:

- challenge and team identity;
- public workbench repository and exact validated commit;
- Level 0-4 architecture;
- primary FCI and CC(n) tables;
- attempted Level 3 tables;
- tenferro gap-list findings;
- reproduction commands and prompt link;
- license and provenance.

- [x] **Step 5: Commit and push both repositories**

Push the workbench first, then update the upstream README to the pushed commit
and push PR #217's branch without force.

---

### Task 8: Final Requirement-by-Requirement Audit

**Files:**
- Modify: `docs/superpowers/plans/2026-07-27-primary-ccn-validation.md`

**Interfaces:**
- Produces: proof that every mandatory deliverable is present and current.

- [x] **Step 1: Run local quality gates**

```bash
cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test
.venv/bin/python -m unittest scripts.oracle.test_units -v
git diff --check
```

- [x] **Step 2: Run numerical acceptance commands**

```bash
RAYON_NUM_THREADS=10 target/release/ed_workbench_rs davidson \
  fixtures/h2o-631g-fc/FCIDUMP \
  --residual-tolerance 1e-7 --max-iterations 60 --max-subspace 20
target/release/ed_workbench_rs cc-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-rank 8 --residual-tolerance 1e-6
target/release/ed_workbench_rs level3-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-ci-rank 8 --max-mbpt-order 20 \
  --ci-residual-tolerance 1e-7 \
  --max-iterations 100 --max-subspace 24
```

The dense `verify` command is intentionally not used for the
245,025-determinant primary space. Full-space CI(8) and the matrix-free
Davidson run independently recover the FCI energy.

- [x] **Step 3: Audit immutable oracle data**

Verify all FCIDUMP checksums, confirm no existing numerical reference field
changed, and validate every committed JSON file with `jq`.

- [x] **Step 4: Audit deliverables**

Confirm:

- Rust Levels 0-2 and oracle harness are present;
- primary FCI and CC(n) accuracy tables exist;
- attempted CI/MBPT/UCC evidence exists;
- tenferro gap list exists;
- upstream PR #217 contains the design README and reproduction prompt;
- all source links, commit IDs, commands, units, and checksums resolve.

- [x] **Step 5: Mark the plan complete, commit, push, and compare refs**

Mark every completed checkbox, commit the completion record, push, and require
local `HEAD`, workbench `origin/main`, and the commit referenced by upstream
PR #217 to agree.
