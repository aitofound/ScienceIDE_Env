# H2O/cc-pVDZ Bounded Benchmark Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add and publish a reproducible H2O/cc-pVDZ all-electron, no-point-group-symmetry Rust benchmark that executes within a few GiB and never allocates the 1.806-billion-element CI space.

**Architecture:** Introduce a reusable determinant-space estimator and a diagonal-free direct-FCI source-column kernel. A dedicated CLI command runs libcint, RHF, AO-to-MO, link construction, and a bounded number of sparse source columns, then emits human-readable output and versioned JSON without invoking Davidson.

**Tech Stack:** Rust 2024, clap, serde/serde_json, libcint, nalgebra, existing determinant/link machinery, cargo test/Clippy.

## Global Constraints

- Use H2O/cc-pVDZ with all 10 electrons, `MS2 = 0`, and no point-group symmetry.
- Keep the default memory budget at 2 GiB and reject estimates above the selected budget before large link-table construction.
- Never instantiate `DirectFciOperator` or allocate a determinant-dimension vector in the cc-pVDZ benchmark.
- Report executed timings separately from projected full-FCI storage.
- Match the PySCF 2.14.0 RHF reference `-76.025792594904772 Eh` within `1e-8 Eh`.
- Preserve all existing Level 0-4 behavior and tests.

---

### Task 1: Determinant-Space and Memory Estimator

**Files:**
- Create: `src/benchmark.rs`
- Modify: `src/lib.rs`
- Test: unit tests in `src/benchmark.rs`

**Interfaces:**
- Produces: `FciSpaceEstimate::new(norb, nelec, ms2) -> Result<FciSpaceEstimate, BenchmarkError>`
- Produces: checked alpha/beta string counts, determinant dimension, dense-vector bytes, and Davidson storage estimates.

- [ ] **Step 1: Write failing estimator tests**

```rust
#[test]
fn water_ccpvdz_space_is_exact() {
    let estimate = FciSpaceEstimate::new(24, 10, 0).unwrap();
    assert_eq!(estimate.alpha_strings, 42_504);
    assert_eq!(estimate.beta_strings, 42_504);
    assert_eq!(estimate.determinants, 1_806_590_016);
    assert_eq!(estimate.vector_bytes, 14_452_720_128);
}
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `cargo test benchmark::tests::water_ccpvdz_space_is_exact`

Expected: FAIL because `benchmark` and `FciSpaceEstimate` do not exist.

- [ ] **Step 3: Implement checked binomial and memory estimates**

Use `u128` intermediates, validate spin parity, and convert to `u64` only
after checked multiplication. Report one vector, the existing diagonal plus
four initial vectors, and 48 Davidson basis/sigma vectors.

- [ ] **Step 4: Run focused tests**

Run: `cargo test benchmark::tests`

Expected: all benchmark estimator tests PASS.

### Task 2: Diagonal-Free Sparse Source-Column Kernel

**Files:**
- Modify: `src/direct_fci.rs`
- Test: unit tests in `src/direct_fci.rs`

**Interfaces:**
- Produces: `DirectFciKernel::new(problem) -> Result<DirectFciKernel, DirectFciError>`
- Produces: `DirectFciKernel::apply_source_sparse(source) -> Result<SparseColumn, DirectFciError>`
- Consumes: `StringSpace`, absorbed one-body integrals, and the existing link-pair algebra.

- [ ] **Step 1: Write a failing equivalence test**

For `fixtures/h2-sto3g/FCIDUMP`, apply the full operator to a unit vector for
every source and compare every nonzero destination with the corresponding
sparse column to `1e-12`.

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `cargo test direct_fci::tests::sparse_columns_match_full_operator`

Expected: FAIL because `DirectFciKernel` is not defined.

- [ ] **Step 3: Extract the shared kernel**

Move `problem`, alpha/beta `StringSpace`, and `effective_eri` into
`DirectFciKernel`. Keep the complete diagonal only in `DirectFciOperator`.
Implement source application through a destination accumulator so the full
operator and sparse-column path share the same fermionic algebra.

- [ ] **Step 4: Run direct-FCI tests**

Run: `cargo test direct_fci::tests`

Expected: sparse/full equivalence and the existing dense-Hamiltonian checks PASS.

### Task 3: Bounded cc-pVDZ CLI and JSON Artifact

**Files:**
- Modify: `src/molecule.rs`
- Modify: `src/main.rs`
- Create: `fixtures/h2o-ccpvdz-ae/reference.json`
- Test: create `tests/benchmark.rs`

**Interfaces:**
- Produces: `Molecule::h2o_cc_pvdz()`
- Produces CLI: `benchmark h2o-cc-pvdz --sources 16 --max-memory-gib 2 --json-output PATH`
- Consumes: `FciSpaceEstimate`, `DirectFciKernel`, libcint, RHF, and AO-to-MO.

- [ ] **Step 1: Add failing molecule and CLI smoke tests**

Assert the built-in molecule uses `cc-pVDZ`, Angstrom coordinates, charge
zero, and the existing water geometry. Run the CLI with one source in an
ignored live test and assert the JSON schema, dimension, RHF convergence, and
absence of a claimed FCI energy.

- [ ] **Step 2: Run focused tests and verify they fail**

Run: `cargo test h2o_cc_pvdz`

Expected: FAIL because the molecule and CLI command do not exist.

- [ ] **Step 3: Implement the bounded command**

Time each stage with `Instant`. Validate `sources > 0` and a finite positive
memory budget. Estimate memory before constructing link tables. Apply sparse
columns one at a time and retain only aggregate nonzeros/checksum. Serialize
durations in seconds and byte counts as integers.

- [ ] **Step 4: Run unit tests and the optimized live benchmark**

Run:

```bash
cargo test h2o_cc_pvdz
cargo build --release
/usr/bin/time -l target/release/ed_workbench_rs benchmark h2o-cc-pvdz \
  --sources 16 \
  --max-memory-gib 2 \
  --json-output fixtures/h2o-ccpvdz-ae/benchmark-m4.json
```

Expected: RHF converges, the energy differs from the reference by less than
`1e-8 Eh`, the JSON file is written, and peak RSS is below 2 GiB.

### Task 4: Report and Reviewer Reproduction

**Files:**
- Create: `reports/h2o-ccpvdz-all-electron-benchmark.md`
- Modify: `README.md`
- Modify: `scripts/verify-submission.sh`

**Interfaces:**
- Consumes: the optimized benchmark JSON, operating-system peak RSS, hardware
  metadata, and current commit identifier.
- Produces: reviewer-facing results and one copy-paste reproduction command.

- [ ] **Step 1: Write the report**

Include exact physical input, PySCF reference, Rust error, all stage timings,
source-column throughput, peak RSS, full-space memory estimates, and the
statement that no converged cc-pVDZ FCI energy is claimed.

- [ ] **Step 2: Link the report from README**

Add a concise benchmark row and direct links to the report and JSON artifact.

- [ ] **Step 3: Extend submission verification**

Validate that the JSON artifact parses and contains:

```text
norb = 24
nelec = 10
alpha_strings = 42504
beta_strings = 42504
determinants = 1806590016
full_fci_executed = false
```

- [ ] **Step 4: Run the complete verification gate**

Run:

```bash
cargo fmt --check
cargo clippy --all-targets --all-features -- -D warnings
cargo test
bash scripts/verify-submission.sh
git diff --check
```

Expected: every command exits zero.

### Task 5: Submit and Close the Review Loop

**Files:**
- Commit all files above.
- Push `main` to `origin`.
- Reply to QuantumBFS/quantum.harness PR #217.

**Interfaces:**
- Produces: a public commit, benchmark report URL, JSON artifact URL, and a
  reviewer comment containing measured results and the explicit full-FCI
  limitation.

- [ ] **Step 1: Commit verified changes**

Run:

```bash
git add README.md src tests fixtures reports scripts docs
git commit -m "feat: add bounded water cc-pVDZ benchmark"
```

- [ ] **Step 2: Push the public repository**

Run: `git push origin main`

- [ ] **Step 3: Post the measured reviewer reply**

Use `gh pr comment 217 --repo QuantumBFS/quantum.harness --body-file ...` with
links pinned to the pushed commit. State actual timings and peak RSS; do not
describe the sparse-column benchmark as a converged FCI calculation.
