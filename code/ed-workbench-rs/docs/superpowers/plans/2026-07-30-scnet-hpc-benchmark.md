# SCNet HPC Benchmark Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build, validate, and run an auditable 1,008-core task-parallel Davidson robustness matrix on SCNet from the fixed v0.4.0 commit.

**Architecture:** Keep the scientific binary unchanged and add only SCNet orchestration.  A scheduled build/smoke gate creates an immutable release binary; a second Slurm array maps 18 deterministic tolerance/subspace cases onto separate 56-core nodes and writes self-contained evidence directories.

**Tech Stack:** Rust 1.89, Cargo `--locked`, Bash, Slurm, Rayon, GNU time, SHA-256, SCNet `xhacnormalb`.

## Global Constraints

- Pin source commit `48f1964a1b3b88090497e1ffce285fde09c98541`.
- Use account `giggleliu` and partition `xhacnormalb`.
- Store all persistent files below `/work/share/giggleliu/cfys01/quantum-harness-129`.
- Never overwrite or delete an earlier run directory.
- Run compilation and scientific workloads through Slurm, not on a login node.
- Do not describe task-parallel throughput as MPI or single-solve strong scaling.
- Do not claim a converged H2O/cc-pVDZ all-electron FCI energy.

---

### Task 1: Add SCNet orchestration scripts

**Files:**
- Create: `hpc/scnet/common.sh`
- Create: `hpc/scnet/build-smoke.sbatch`
- Create: `hpc/scnet/davidson-robustness.sbatch`
- Create: `hpc/scnet/README.md`

**Interfaces:**
- Consumes: v0.4.0 repository, `Cargo.lock`, SCNet Slurm variables.
- Produces: scheduled build/smoke evidence and isolated Davidson array cases.

- [ ] **Step 1: Add a shared environment contract**

Define the fixed commit, remote root, source directory, Cargo/Rustup homes,
binary path, fixture path, and a `record_environment` helper.  Fail when the
checked-out commit, lockfile, binary, or fixture does not match the expected
state.

- [ ] **Step 2: Add the scheduled build/smoke gate**

Request one 56-core node for at most 30 minutes.  Install/select Rust 1.89,
build and test with `--locked`, run the three small fixture verifications, run
the bounded cc-pVDZ benchmark, and run the primary frozen-core Davidson solve.
Write statuses and hashes even when a command fails.

- [ ] **Step 3: Add the 18-case production array**

Request `--array=0-17%18`, one node, 56 CPUs, 196 GiB, and 15 minutes per task.
Map the array index using:

```bash
tolerances=(1e-6 1e-7 1e-8)
subspaces=(12 16 20 24 32 48)
tolerance="${tolerances[$((SLURM_ARRAY_TASK_ID / 6))]}"
subspace="${subspaces[$((SLURM_ARRAY_TASK_ID % 6))]}"
```

Set `RAYON_NUM_THREADS=56` and run Davidson with 56 source blocks and strict
parallel-memory enforcement.

- [ ] **Step 4: Document the evidence boundary and commands**

Document staging, submission, status inspection, selective resubmission, result
download, and the distinction between task-level 1,008-core concurrency and
single-solve MPI scaling.

- [ ] **Step 5: Validate shell syntax**

Run:

```bash
bash -n hpc/scnet/common.sh
bash -n hpc/scnet/build-smoke.sbatch
bash -n hpc/scnet/davidson-robustness.sbatch
```

Expected: all commands exit zero.

### Task 2: Stage and submit the build/smoke job

**Files:**
- Consume: `hpc/scnet/*.sh`, `hpc/scnet/*.sbatch`
- Produce remotely: `source-v0.4.0/`, `toolchains/`, `runs/<job-id>/build-smoke/`

**Interfaces:**
- Consumes: authorized SSH jump path and SCNet allocation.
- Produces: one build/smoke Slurm job ID and an immutable release binary.

- [ ] **Step 1: Create the versioned remote root**

Create the remote root only if absent.  Clone the repository and detach at the
fixed commit.  Verify `git status --porcelain` is empty.  SCNet provides Git
1.8.3, so run Git from the source directory or use explicit `--git-dir` and
`--work-tree`; do not use the newer `git -C` option.

- [ ] **Step 2: Prefetch the immutable build inputs**

On a connected machine, checksum the official standalone Rust 1.89 Linux
archive and run `cargo vendor --locked --versioned-dirs` for the pinned
manifest.  Upload both to the shared toolchain directory and install Rust at
`toolchains/rust-1.89.0`.  Compute nodes have no external DNS, so the scheduled
build must set `CARGO_NET_OFFLINE=true` and use `cargo build --offline` and
`cargo test --offline`.

Export the pinned source with `git archive`, copy the single compressed vendor
archive, expand it below `$SLURM_TMPDIR`, and set `CARGO_TARGET_DIR` there.
Copy only the final release binary to `artifacts/v0.4.0/` using a job-specific
staging name followed by an atomic rename.

Export libcint `v6.1.2` commit
`8d13863ff481cea27efea5e56c9e4d352cdb8f80` into a non-shallow, one-commit Git
snapshot carrying the identical tree hash
`3de5cd4cf6b7f3fe04d53dfeed3dc85f69eb1133`.  On the compute node, load CMake
3.25, verify and expand the archive locally, set `CINT_SRC` to it, and export
the GCC 11.4 `CC`/`CXX` paths before Cargo starts.

- [ ] **Step 3: Stage orchestration files**

Copy the locally validated files to a versioned remote `hpc/scnet` directory
and verify their SHA-256 digests after transfer.

- [ ] **Step 4: Submit build/smoke**

Run `sbatch --parsable
--export=ALL,QH129_ORCHESTRATION=/absolute/versioned/orchestration
build-smoke.sbatch` from the versioned orchestration directory and record the
returned job ID locally.  The explicit path is required because Slurm executes
a spool copy of the submission script.

- [ ] **Step 5: Verify the gate**

Require Slurm state `COMPLETED`, script status `0`, locked tests passing,
small-system verification passing, bounded cc-pVDZ execution with
`full_fci_executed=false`, and primary energy agreement.

### Task 3: Submit and verify the 1,008-core production array

**Files:**
- Consume remotely: the verified release binary and
  `hpc/scnet/davidson-robustness.sbatch`
- Produce remotely: `runs/<job-id>/davidson-array/000` through `017`
- Create locally after download: `reports/scnet-hpc-benchmark.md`
- Create locally after download: `fixtures/hpc/scnet-davidson-robustness.json`

**Interfaces:**
- Consumes: successful Task 2 evidence.
- Produces: Slurm array job ID, raw logs/manifests, aggregate JSON, and report.

- [ ] **Step 1: Submit at full array concurrency**

Run:

```bash
sbatch --parsable \
  --export=ALL,QH129_ORCHESTRATION=/absolute/versioned/orchestration \
  davidson-robustness.sbatch
```

Require the script header to specify `0-17%18` and 56 CPUs per task.

- [ ] **Step 2: Audit live allocation**

Use `squeue`/`scontrol` to record task count, CPUs per task, node list, start
times, and the maximum observed concurrent CPU allocation.

- [ ] **Step 3: Collect results**

Wait for terminal states, download the result directory without deleting the
remote copy, and verify every recorded hash.

- [ ] **Step 4: Aggregate numerical and performance evidence**

Parse each case into versioned JSON with tolerance, subspace, energy, residual,
iterations, convergence, wall time, node, and exit status.  Report the maximum
energy spread over converged cases and median/minimum/maximum wall time.

- [ ] **Step 5: Run final project gates**

Run:

```bash
cargo fmt --check
cargo clippy --locked --all-targets --all-features -- -D warnings
cargo test --locked --all-targets
bash scripts/verify-submission.sh
git diff --check
```

Expected: every command exits zero.
