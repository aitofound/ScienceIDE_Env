# Incremental Solver Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver v0.2-v0.4 incremental releases that generalize active spaces and combinadic addressing, harden CC diagnostics, add restartable disk-backed Davidson, provide deterministic bounded-memory CPU parallelism, and preserve every published numerical result.

**Architecture:** Keep the existing `ElectronicProblem`, `LinearOperator`, and solver entry points as compatibility APIs. Add focused active-space/combinadic modules, a storage-backed Davidson engine selected by explicit run options, and an opt-in direct-FCI execution policy whose memory is preflighted before Rayon work starts.

**Tech Stack:** Rust 1.89, nalgebra 0.35, Rayon 1.11, serde/serde_json 1.0, clap 4.5, thiserror 2.0, libcint 0.3.2, committed PySCF fixtures.

## Global Constraints

- Target one Apple Silicon or Linux workstation with 16-128 GiB RAM and local NVMe.
- Do not add MPI, GPU, selected-CI, PT2, or automatic point-group symmetry.
- Do not claim converged H2O/cc-pVDZ all-electron full FCI.
- Keep existing CLI commands and public compatibility functions working.
- Confine disk writes to an explicit Davidson workspace; never delete an unspecified path.
- Preserve all committed CC(1)-CC(8), CI(1)-CI(8), MBPT(1)-MBPT(20), FCI, RHF, and AO-to-MO results.
- Use test-driven changes and commit each independently reviewable task.

---

## File Map

### Create

- `src/combinadic.rs`: checked counts and rank/unrank without allocation.
- `src/davidson/storage.rs`: in-memory and disk-backed vector stores.
- `src/davidson/checkpoint.rs`: versioned manifest, validation, atomic persistence.
- `tests/active_space_general.rs`: public active-space behavior.
- `tests/davidson_workspace.rs`: disk, resume, corruption, and CLI coverage.
- `tests/parallel_sigma.rs`: deterministic serial/parallel equivalence.
- `docs/checkpoint-format.md`: on-disk schema and compatibility contract.
- `docs/release-notes-v0.2.0.md`: generality and CC-hardening release.
- `docs/release-notes-v0.3.0.md`: restartable Davidson release.
- `docs/release-notes-v0.4.0.md`: parallelism and final validation release.
- `reports/incremental-solver-validation.md`: numerical and performance acceptance.

### Modify

- `src/active_space.rs`: `ActiveSpaceSpec`, maps, validation, compatibility wrapper.
- `src/strings.rs`: checked combinadic addressing.
- `src/determinant.rs`: checked dimension preflight and non-enumerating generation.
- `src/coupled_cluster.rs`: configuration validation, finite-state checks, termination.
- `src/davidson.rs`: storage-independent algorithm and run options.
- `src/direct_fci.rs`: execution policy, memory preflight, ordered parallel reduction.
- `src/operator.rs`: I/O-capable operator error without weakening length checks.
- `src/main.rs`: active-space inspection, CC JSON, Davidson workspace, parallel flags.
- `src/lib.rs`: module exports and crate-level contracts.
- `tests/level1.rs`: unchanged numerical results through both Davidson paths.
- `tests/level2.rs`: CC validation and termination behavior.
- `tests/cc_series.rs`: stable JSON and Table 2 regression.
- `tests/benchmark.rs`: execution metadata and bounded-memory assertions.
- `scripts/verify-submission.sh`: new JSON/schema and fast-regression gates.
- `Cargo.toml`: advance versions at release boundaries.
- `README.md`: commands, release links, limitations, and migration guidance.

---

### Task 1: General Active-Space Selection

**Files:**
- Modify: `src/active_space.rs`
- Create: `tests/active_space_general.rs`
- Modify: `src/lib.rs`

**Interfaces:**
- Consumes: `ElectronicProblem::new`, `ElectronicProblem::{h1,eri,h1_data,eri_data}`.
- Produces: `ActiveSpaceSpec`, `ActiveSpaceResult`, `build_active_space`; preserves `freeze_core`.

- [ ] **Step 1: Write failing public API tests**

Add tests constructing small analytic problems and asserting:

```rust
let result = build_active_space(
    &problem,
    &ActiveSpaceSpec {
        frozen_occupied: vec![0],
        frozen_virtual: vec![3],
    },
).unwrap();
assert_eq!(result.problem.norb, 2);
assert_eq!(result.problem.nelec, problem.nelec - 2);
assert_eq!(result.active_to_original, vec![1, 2]);
assert_eq!(result.original_to_active, vec![None, Some(0), Some(1), None]);
```

Also assert rejection of duplicates, overlap, out-of-range orbitals, every
orbital removed, and an electron/spin sector that no longer fits.

- [ ] **Step 2: Verify the tests fail**

Run:

```bash
cargo test --test active_space_general
```

Expected: compilation failure because the new types and function do not exist.

- [ ] **Step 3: Implement the checked transformation**

Add:

```rust
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ActiveSpaceSpec {
    pub frozen_occupied: Vec<usize>,
    pub frozen_virtual: Vec<usize>,
}

#[derive(Debug, Clone)]
pub struct ActiveSpaceResult {
    pub problem: ElectronicProblem,
    pub active_to_original: Vec<usize>,
    pub original_to_active: Vec<Option<usize>>,
}

pub fn build_active_space(
    problem: &ElectronicProblem,
    spec: &ActiveSpaceSpec,
) -> Result<ActiveSpaceResult, ActiveSpaceError>;
```

Canonicalize cloned lists by sorting, reject duplicates before deduplication,
check disjointness, build both maps, fold only `frozen_occupied` into `ecore`
and `h1`, and subset `h1`, `eri`, and orbital energies.

- [ ] **Step 4: Preserve the compatibility wrapper**

Implement `freeze_core(problem, frozen)` by calling `build_active_space` with
an empty virtual list and returning `.problem`.

- [ ] **Step 5: Run focused and existing tests**

Run:

```bash
cargo test active_space
cargo test --test active_space_general
cargo test --test level1
```

Expected: all pass with unchanged reference energies.

- [ ] **Step 6: Commit**

```bash
git add src/active_space.rs src/lib.rs tests/active_space_general.rs
git commit -m "feat: generalize active-space selection"
```

---

### Task 2: Checked Combinadic Addressing

**Files:**
- Create: `src/combinadic.rs`
- Modify: `src/lib.rs`
- Modify: `src/strings.rs`
- Modify: `src/determinant.rs`

**Interfaces:**
- Consumes: lexical numeric occupation-string ordering.
- Produces: `combination_count`, `rank_occupation`, `unrank_occupation`,
  `CombinadicError`.

- [ ] **Step 1: Write exhaustive and boundary tests**

For `norb=1..=12` and every valid population, enumerate the existing strings
and assert:

```rust
assert_eq!(rank_occupation(bits, norb, nelec).unwrap(), index as u128);
assert_eq!(
    unrank_occupation(index as u128, norb, nelec).unwrap(),
    bits
);
```

Add invalid-population, out-of-range-bit, rank-too-large, `C(64,32)`, and
`usize` allocation-overflow tests.

- [ ] **Step 2: Verify the module is absent**

Run:

```bash
cargo test combinadic
```

Expected: compilation failure for the missing module.

- [ ] **Step 3: Implement checked `u128` combinatorics**

Implement the three public functions from the design. Use checked
multiplication and exact division at each binomial step; return a typed error
instead of wrapping.

- [ ] **Step 4: Replace exponential-range occupation enumeration**

Change `occupation_strings` to allocate exactly the checked combination count
and fill it by unranking `0..count`. Reject counts that do not fit `usize`.
Keep the observable lexical ordering unchanged.

- [ ] **Step 5: Use checked rank for `StringSpace::address`**

Retain the address cache for compatibility/performance checks, but compute and
debug-assert the combinadic rank. Make `StringSpace::rank` delegate to the
checked implementation and convert to `usize`.

- [ ] **Step 6: Verify all determinant and direct-FCI paths**

Run:

```bash
cargo test combinadic
cargo test determinant
cargo test strings
cargo test direct_fci
cargo test --test level0
cargo test --test level1
```

Expected: all pass; determinant enumeration is byte-for-byte unchanged for
committed fixtures.

- [ ] **Step 7: Commit**

```bash
git add src/combinadic.rs src/lib.rs src/strings.rs src/determinant.rs
git commit -m "feat: harden combinadic determinant addressing"
```

---

### Task 3: CC Configuration and Result Hardening

**Files:**
- Modify: `src/coupled_cluster.rs`
- Modify: `src/main.rs`
- Modify: `tests/level2.rs`
- Modify: `tests/cc_series.rs`

**Interfaces:**
- Consumes: existing `CcConfig`, `CcResult`, and series solver.
- Produces: `CcTermination`, serializable iteration/summary records, validated
  configuration.

- [ ] **Step 1: Add failing invalid-config and non-finite tests**

Assert that zero/NaN tolerances, zero iterations, zero DIIS history, and
non-finite orbital energies return typed `CcError` variants. Assert successful
small-system runs return `CcTermination::Converged`.

- [ ] **Step 2: Verify focused failures**

Run:

```bash
cargo test --test level2
```

Expected: failures because configuration validation and termination fields are
not implemented.

- [ ] **Step 3: Add validation and finite-state guards**

Define:

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum CcTermination {
    Converged,
    MaximumIterations,
}
```

Add `termination` to `CcResult`. Validate configuration before constructing
spaces. Reject non-finite energy, residual, updated amplitudes, or DIIS output
with the rank/iteration in the error.

- [ ] **Step 4: Add stable CC-series JSON output**

Add `--json-output <PATH>` to `cc-series`. Write a schema-versioned object
containing system, energy unit, solver configuration, every rank's energy,
residual, iteration count, termination, elapsed time, and published
comparison. Keep current text output unchanged when the option is absent.

- [ ] **Step 5: Verify numerical non-regression**

Run:

```bash
cargo test --test level2
cargo test --test cc_series
cargo test --test published_reference
```

Expected: all pass; committed Table 2 matching remains 8/8.

- [ ] **Step 6: Commit**

```bash
git add src/coupled_cluster.rs src/main.rs tests/level2.rs tests/cc_series.rs
git commit -m "feat: harden coupled-cluster diagnostics"
```

---

### Task 4: Package v0.2.0

**Files:**
- Modify: `Cargo.toml`
- Modify: `Cargo.lock`
- Create: `docs/release-notes-v0.2.0.md`
- Modify: `README.md`
- Modify: `scripts/verify-submission.sh`

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: a self-contained v0.2.0 candidate.

- [ ] **Step 1: Advance package metadata**

Set `Cargo.toml` package version to `0.2.0` and refresh `Cargo.lock` with:

```bash
cargo check
```

- [ ] **Step 2: Document exact compatibility and examples**

Add active-space maps, checked combinadic limits, CC JSON schema, and the
statement that all published energies are unchanged. Change the README release
badge target to the repository releases page rather than v0.1.0.

- [ ] **Step 3: Extend verification**

Make `scripts/verify-submission.sh` run the new integration tests and validate
a generated CC JSON file with `jq` in a temporary directory.

- [ ] **Step 4: Run v0.2 gates**

Run:

```bash
cargo fmt --check
cargo clippy --all-targets --all-features -- -D warnings
cargo test --locked
scripts/verify-submission.sh
```

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add Cargo.toml Cargo.lock README.md docs/release-notes-v0.2.0.md scripts/verify-submission.sh
git commit -m "release: prepare v0.2.0"
```

---

### Task 5: Vector Stores for Davidson

**Files:**
- Create: `src/davidson/storage.rs`
- Modify: `src/davidson.rs`

**Interfaces:**
- Consumes: `Vec<f64>` Davidson vectors.
- Produces: private `VectorStore`, `MemoryVectorStore`, `DiskVectorStore`.

- [ ] **Step 1: Write storage contract tests**

Exercise push/load/replace for both stores. For disk storage, use a unique
directory under `std::env::temp_dir()`, verify exact little-endian file size,
and reject truncated and non-finite vector files.

- [ ] **Step 2: Verify missing storage module**

Run:

```bash
cargo test davidson::storage
```

Expected: compilation failure until the submodule exists.

- [ ] **Step 3: Implement memory storage**

Implement the design's `VectorStore` contract with exact dimension checks and
copying semantics matching the current `Vec<Vec<f64>>`.

- [ ] **Step 4: Implement disk storage**

Use `BufWriter`, `BufReader`, `f64::{to_le_bytes,from_le_bytes}`, explicit file
names `vector-000000.bin`, and `sync_all`. Refuse a nonempty workspace unless
opened for resume.

- [ ] **Step 5: Run storage tests and sanitizing checks**

Run:

```bash
cargo test davidson::storage
cargo clippy --all-targets -- -D warnings
```

Expected: all pass without unsafe code.

- [ ] **Step 6: Commit**

```bash
git add src/davidson.rs src/davidson/storage.rs
git commit -m "feat: add Davidson vector stores"
```

---

### Task 6: Versioned Checkpoint and Resume

**Files:**
- Create: `src/davidson/checkpoint.rs`
- Modify: `src/davidson.rs`
- Create: `tests/davidson_workspace.rs`

**Interfaces:**
- Consumes: vector stores and `LinearOperator`.
- Produces: `DavidsonRunConfig`, `DavidsonWorkspaceConfig`,
  `lowest_eigenpair_with_run_config`.

- [ ] **Step 1: Write interruption/resume tests**

Run a small symmetric operator for two iterations into a disk workspace,
resume with a larger iteration limit, and compare energy, eigenvector phase-
aligned error, residual, and termination with an uninterrupted run.

Add dimension, fingerprint, tolerance, truncated-vector, corrupt-JSON, and
existing-nonresume-workspace rejection tests.

- [ ] **Step 2: Verify tests fail**

Run:

```bash
cargo test --test davidson_workspace
```

Expected: compilation failure for the missing run configuration.

- [ ] **Step 3: Implement checkpoint schema**

Define serde records with `schema_version: 1`, operator fingerprint,
dimension, algorithm configuration, completed iteration, previous energy,
vector counts, last result, scalar type `f64`, and byte order `little`.

- [ ] **Step 4: Implement atomic manifest commits**

Write `checkpoint.json.tmp`, flush and sync it, then rename it to
`checkpoint.json` only after basis and sigma files are synced. On resume,
validate every field and referenced file before iteration begins.

- [ ] **Step 5: Refactor Davidson around stores**

Keep:

```rust
pub fn lowest_eigenpair(
    operator: &impl LinearOperator,
    initial: &[f64],
    config: &DavidsonConfig,
) -> Result<DavidsonResult, DavidsonError>;
```

as an in-memory wrapper. Add:

```rust
pub fn lowest_eigenpair_with_run_config(
    operator: &impl LinearOperator,
    initial: &[f64],
    run: &DavidsonRunConfig,
) -> Result<DavidsonResult, DavidsonError>;
```

Load one stored vector at a time for projected matrix construction, Ritz
assembly, orthogonalization, and restart.

- [ ] **Step 6: Verify equality and compatibility**

Run:

```bash
cargo test davidson
cargo test --test davidson_workspace
cargo test --test level1
```

Expected: disk/resume and in-memory results agree within `1e-12`; old API
tests remain unchanged.

- [ ] **Step 7: Commit**

```bash
git add src/davidson.rs src/davidson/checkpoint.rs tests/davidson_workspace.rs
git commit -m "feat: checkpoint and resume Davidson"
```

---

### Task 7: Davidson Workspace CLI and v0.3.0

**Files:**
- Modify: `src/main.rs`
- Create: `docs/checkpoint-format.md`
- Create: `docs/release-notes-v0.3.0.md`
- Modify: `README.md`
- Modify: `Cargo.toml`
- Modify: `Cargo.lock`
- Modify: `scripts/verify-submission.sh`

**Interfaces:**
- Consumes: Task 6 run API.
- Produces: `davidson` workspace/resume CLI and v0.3.0 candidate.

- [ ] **Step 1: Add failing CLI tests**

Assert `davidson --help` documents:

```text
--workspace <PATH>
--resume
--checkpoint-every <N>
--memory-budget-gib <GIB>
--operator-fingerprint <TEXT>
```

Run H4 once with `--max-iterations 1 --workspace`, then resume to convergence
and compare with the normal command.

- [ ] **Step 2: Add CLI validation and memory preflight**

Require `--workspace` when `--resume` is used. Estimate resident vectors as
input, output, Ritz vector, residual, correction, and one loaded store vector;
reject a budget smaller than that estimate. Hash FCIDUMP bytes by default for
the operator fingerprint.

- [ ] **Step 3: Document the format and limitation**

Document manifest fields, byte order, atomicity, resume compatibility, disk
capacity planning, and that one full vector must fit in RAM.

- [ ] **Step 4: Advance and verify v0.3**

Set version `0.3.0`, refresh the lock file, then run:

```bash
cargo fmt --check
cargo clippy --all-targets --all-features -- -D warnings
cargo test --locked
scripts/verify-submission.sh
```

- [ ] **Step 5: Commit**

```bash
git add src/main.rs tests/davidson_workspace.rs docs/checkpoint-format.md docs/release-notes-v0.3.0.md README.md Cargo.toml Cargo.lock scripts/verify-submission.sh
git commit -m "release: prepare v0.3.0 restartable Davidson"
```

---

### Task 8: Deterministic Parallel Direct FCI

**Files:**
- Modify: `src/direct_fci.rs`
- Modify: `src/operator.rs`
- Create: `tests/parallel_sigma.rs`

**Interfaces:**
- Consumes: `DirectFciKernel::apply_source`, Rayon.
- Produces: `ExecutionPolicy`, `ExecutionReport`, configured
  `DirectFciOperator`.

- [ ] **Step 1: Write serial/parallel and budget tests**

For H2, H4, and H2O/STO-3G, compare serial and parallel sigma vectors within
`1e-12`. Run the same fixed-block policy twice and require
`parallel_a.to_bits() == parallel_b.to_bits()` elementwise. Assert strict
budget rejection and allowed serial fallback.

- [ ] **Step 2: Verify the execution policy is absent**

Run:

```bash
cargo test --test parallel_sigma
```

Expected: compilation failure for missing execution types.

- [ ] **Step 3: Implement policy and preflight**

Define:

```rust
pub enum ExecutionPolicy {
    Serial,
    Parallel {
        blocks: usize,
        memory_budget_bytes: u64,
        allow_serial_fallback: bool,
    },
}
```

Validate nonzero blocks and checked `8 * blocks * dimension` arithmetic before
allocating.

- [ ] **Step 4: Implement ordered block reduction**

Use Rayon indexed parallel iteration over fixed contiguous source ranges.
Each range returns a dense partial vector. Collect results in range order and
sum them sequentially into the caller's output. Preserve the serial path as
the default.

- [ ] **Step 5: Record execution outcome**

Expose a report containing requested/effective mode, block count, Rayon
threads, estimated workspace, fallback reason, and elapsed time. Do not add
global mutable state.

- [ ] **Step 6: Verify correctness and determinism**

Run:

```bash
cargo test direct_fci
cargo test --test parallel_sigma
cargo test --test level1
```

Expected: all pass and the old serial numerical path is unchanged.

- [ ] **Step 7: Commit**

```bash
git add src/direct_fci.rs src/operator.rs tests/parallel_sigma.rs
git commit -m "feat: add deterministic bounded parallel sigma"
```

---

### Task 9: Expanded Validation Matrix

**Files:**
- Modify: `tests/active_space_general.rs`
- Modify: `tests/level1.rs`
- Modify: `tests/level2.rs`
- Modify: `tests/benchmark.rs`
- Create: `reports/incremental-solver-validation.md`

**Interfaces:**
- Consumes: all solver modes.
- Produces: one matrix proving spin, active-space, storage, parallel, and
  published-series behavior.

- [ ] **Step 1: Add programmatic open-shell reference**

Construct a one-electron, three-orbital diagonal Hamiltonian with `NELEC=1`,
`MS2=1`. Assert dense FCI, serial Davidson, disk Davidson, and parallel
Davidson all return the lowest one-electron energy.

- [ ] **Step 2: Add active-space equivalence cases**

Compare a transformed problem against a manually constructed active problem
for frozen occupied, frozen virtual, mixed, and no-op selections.

- [ ] **Step 3: Add full regression matrix**

Run existing committed fixtures through applicable serial, disk, and parallel
paths. Keep expensive H2O/6-31G and CC(8) tests ignored for explicit release
execution but validate their committed JSON in normal CI.

- [ ] **Step 4: Write the validation report**

Record exact commands, tolerances, units, hardware-independent assertions,
and which timings are machine-specific.

- [ ] **Step 5: Run the complete fast matrix**

Run:

```bash
cargo test --locked
```

Expected: all nonignored tests pass.

- [ ] **Step 6: Commit**

```bash
git add tests/active_space_general.rs tests/level1.rs tests/level2.rs tests/benchmark.rs reports/incremental-solver-validation.md
git commit -m "test: expand solver validation matrix"
```

---

### Task 10: Parallel CLI, Benchmark, and v0.4.0

**Files:**
- Modify: `src/main.rs`
- Modify: `src/benchmark.rs`
- Modify: `tests/benchmark.rs`
- Create: `docs/release-notes-v0.4.0.md`
- Modify: `README.md`
- Modify: `Cargo.toml`
- Modify: `Cargo.lock`
- Modify: `scripts/verify-submission.sh`

**Interfaces:**
- Consumes: Task 8 execution policy and Task 9 matrix.
- Produces: final v0.4.0 candidate.

- [ ] **Step 1: Add CLI execution controls**

Add `--parallel-blocks`, `--parallel-memory-budget-gib`, and
`--strict-parallel-memory` to Davidson/direct-integral commands. Default to
serial to preserve v0.1 behavior.

- [ ] **Step 2: Extend structured benchmark output**

Add a schema-versioned execution section with requested/effective policy,
threads, workspace bytes, fallback, sigma checksum, and elapsed seconds.
Update JSON tests and `jq` validation.

- [ ] **Step 3: Advance package metadata and docs**

Set version `0.4.0`. Add release notes covering v0.2-v0.4 migration,
checkpoint examples, parallel-memory equations, and exact non-goals.

- [ ] **Step 4: Run full static and fast gates**

Run:

```bash
cargo fmt --check
cargo clippy --all-targets --all-features -- -D warnings
cargo test --locked
scripts/verify-submission.sh
git diff --check
```

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add src/main.rs src/benchmark.rs tests/benchmark.rs docs/release-notes-v0.4.0.md README.md Cargo.toml Cargo.lock scripts/verify-submission.sh
git commit -m "release: prepare v0.4.0 solver hardening"
```

---

### Task 11: Release-Mode Numerical and Performance Audit

**Files:**
- Modify if measurements change: `reports/incremental-solver-validation.md`

**Interfaces:**
- Consumes: v0.4.0 candidate.
- Produces: final evidence for all seven requested work areas.

- [ ] **Step 1: Build the exact release binary**

Run:

```bash
cargo build --release --locked
```

Expected: successful optimized build.

- [ ] **Step 2: Run the primary H2O/6-31G FCI acceptance**

Run the documented release Davidson command and require energy
`-76.121174204141980 Eh` within `1e-9 Eh` and convergence within the configured
residual threshold.

- [ ] **Step 3: Run live CC(1)-CC(8) acceptance**

Run:

```bash
cargo test --release --test cc_series live_primary_cc_series_matches_hirata_table2 -- --ignored --nocapture
```

Expected: published verification PASS, 8/8.

- [ ] **Step 4: Run checkpoint/resume acceptance**

Use a fresh temporary workspace, stop after a bounded iteration count, resume,
and compare the final energy/residual with uninterrupted in-memory Davidson.

- [ ] **Step 5: Run serial/parallel measurement**

Measure at least five fresh processes for the primary H2O/6-31G sigma path.
Record raw wall time, peak RSS, checksum, effective policy, and medians. Do not
assert a speedup if measurements do not show one.

- [ ] **Step 6: Re-run the bounded cc-pVDZ benchmark**

Run the existing 2 GiB preflight command and confirm:

```text
determinants = 1,806,590,016
full_fci_executed = false
RHF absolute error < 1e-8 Eh
```

- [ ] **Step 7: Complete the evidence report**

Update the report only with measured values and exact commit SHA. State
failures and fallbacks explicitly.

- [ ] **Step 8: Final audit and commit**

Run:

```bash
scripts/verify-submission.sh
git status --short
git diff --check
```

Then commit the measured report:

```bash
git add reports/incremental-solver-validation.md
git commit -m "docs: record incremental solver validation"
```

---

## Completion Audit

The work is complete only when all of the following are evidenced:

- mixed occupied/virtual active-space selection and maps are tested;
- combinadic arithmetic cannot silently overflow;
- CC invalid states are typed errors and Table 2 remains 8/8;
- disk-backed Davidson and interrupted resume match uninterrupted Davidson;
- incompatible/corrupt checkpoints are rejected;
- parallel sigma is repeatable for a fixed policy and matches serial;
- requested parallel memory is preflighted before allocation;
- the expanded open-shell and fixture matrix passes;
- v0.2.0, v0.3.0, and v0.4.0 release notes and metadata are coherent;
- all static, unit, integration, live numerical, and bounded benchmark gates
  pass;
- no claim of converged H2O/cc-pVDZ full FCI appears.
