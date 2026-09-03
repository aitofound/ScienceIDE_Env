# Benchmark Follow-Up Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the three approved benchmark polish items by committing a verified five-process summary JSON, making the CLI memory-budget semantics precise without breaking old commands, and updating PR #217.

**Architecture:** Treat the five existing fresh-process measurements as immutable observations and validate their aggregate JSON from tests. Change only the canonical clap option name while preserving the old spelling as an alias, then patch the live PR body by appending content to its current value.

**Tech Stack:** Rust 2024, clap, serde_json, Bash/jq verification, GitHub CLI/API.

## Global Constraints

- Do not move or recreate the published `v0.1.1` tag.
- Preserve compatibility with `--max-memory-gib`.
- Use `--memory-budget-gib` as the canonical documented spelling.
- State that the budget is a conservative preflight estimate, not an OS hard limit.
- Preserve all existing PR body content and append the benchmark section exactly once.
- Do not claim a converged cc-pVDZ full-FCI calculation.

---

### Task 1: Five-Process Machine-Readable Summary

**Files:**
- Create: `fixtures/h2o-ccpvdz-ae/benchmark-m4-summary.json`
- Modify: `tests/benchmark.rs`
- Modify: `scripts/verify-submission.sh`

**Interfaces:**
- Consumes: five independent process measurements documented in `reports/h2o-ccpvdz-all-electron-benchmark.md`.
- Produces: schema-versioned raw runs and recomputable aggregates.

- [ ] **Step 1: Add a failing artifact test**

Load `benchmark-m4-summary.json`, require five runs, collect each numeric field,
sort with `f64::total_cmp`, and assert that the stored median/max values equal
the recomputed values within `1e-12`.

- [ ] **Step 2: Run the focused test and verify failure**

Run: `cargo test --test benchmark five_process_summary_recomputes_exactly`

Expected: FAIL because the summary artifact does not exist.

- [ ] **Step 3: Add the summary JSON**

Record runs 1-5 exactly as published, including:

```text
wall_seconds
peak_rss_bytes
ao_integrals_seconds
rhf_seconds
ao_to_mo_seconds
link_tables_seconds
sparse_columns_seconds
contributions_per_second
checksum
```

- [ ] **Step 4: Extend submission verification**

Use `jq` to require schema version 1, artifact kind
`h2o-ccpvdz-five-process-summary`, exactly five runs, measured commit
`025a6dd27836f2e9011ef63ee35630a667bdd786`, and maximum RSS below 2 GiB.

- [ ] **Step 5: Run focused tests**

Run:

```bash
cargo test --test benchmark five_process_summary_recomputes_exactly
jq empty fixtures/h2o-ccpvdz-ae/benchmark-m4-summary.json
```

Expected: both commands PASS.

### Task 2: Precise Backward-Compatible CLI Semantics

**Files:**
- Modify: `src/main.rs`
- Modify: `src/benchmark.rs`
- Modify: `tests/benchmark.rs`
- Modify: `README.md`
- Modify: `reports/h2o-ccpvdz-all-electron-benchmark.md`

**Interfaces:**
- Produces canonical option `--memory-budget-gib`.
- Preserves visible alias `--max-memory-gib`.
- Keeps JSON field `memory_budget_bytes`.

- [ ] **Step 1: Add failing help and alias tests**

Invoke `benchmark --help` and assert the help contains:

```text
--memory-budget-gib
--max-memory-gib
not an operating-system hard memory limit
```

Invoke both spellings with `0.5` GiB and require the same estimate-exceeds-budget error.

- [ ] **Step 2: Run focused tests and verify failure**

Run: `cargo test --test benchmark memory_budget_cli_is_precise_and_backward_compatible`

Expected: FAIL because the canonical option and help text do not yet exist.

- [ ] **Step 3: Implement the option rename and alias**

Use clap attributes:

```rust
#[arg(
    long = "memory-budget-gib",
    visible_alias = "max-memory-gib",
    default_value_t = 2.0,
    help = "Reject when the conservative preflight estimate exceeds this budget; not an operating-system hard memory limit"
)]
memory_budget_gib: f64,
```

Rename `BoundedBenchmarkConfig::max_memory_gib` to `memory_budget_gib`.

- [ ] **Step 4: Update canonical documentation**

Replace documented `--max-memory-gib` commands with
`--memory-budget-gib`, while keeping one compatibility note for the old alias.

- [ ] **Step 5: Run focused tests**

Run:

```bash
cargo test --test benchmark memory_budget_cli_is_precise_and_backward_compatible
cargo test benchmark::tests
```

Expected: all tests PASS.

### Task 3: Documentation, PR Body, and Submission

**Files:**
- Modify: `README.md`
- Modify: `reports/h2o-ccpvdz-all-electron-benchmark.md`
- External: QuantumBFS/quantum.harness PR #217 body

**Interfaces:**
- Consumes: committed summary JSON URL and `v0.1.1` release URL.
- Produces: discoverable summary link and reviewer-facing benchmark section.

- [ ] **Step 1: Link the summary artifact**

Add `benchmark-m4-summary.json` beside the single-run JSON in README and the
benchmark report. State that aggregate fields are recomputed in tests.

- [ ] **Step 2: Run the complete local quality gate**

Run:

```bash
cargo fmt --check
cargo clippy --all-targets --all-features -- -D warnings
cargo test --locked
bash scripts/verify-submission.sh
git diff --check
```

Expected: every command exits zero.

- [ ] **Step 3: Commit and push main**

Run:

```bash
git add README.md docs fixtures reports scripts src tests
git commit -m "docs: complete benchmark follow-up polish"
git push origin main
```

- [ ] **Step 4: Patch PR #217 body**

Read the current body from GitHub, replace only the `v0.1.0` Release row and
checklist source reference with `v0.1.1`, append a
`Reviewer follow-up benchmark` section when absent, and PATCH the complete
preserved body through the GitHub API.

- [ ] **Step 5: Verify remote completion**

Confirm:

```text
origin/main == HEAD
v0.1.1^{} == 025a6dd27836f2e9011ef63ee35630a667bdd786
PR body contains the follow-up heading exactly once
PR body links v0.1.1 and benchmark-m4-summary.json
latest CI for the new main commit succeeds
```
