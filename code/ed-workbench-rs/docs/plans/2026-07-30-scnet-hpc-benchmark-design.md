# SCNet HPC Benchmark Design

## Objective

Run the fixed v0.4.0 electronic-structure workbench on the authorized SCNet
allocation and produce an auditable Linux/HPC evidence bundle.  The production
experiment is an 18-case H2O/6-31G frozen-core Davidson robustness matrix:

- residual tolerances: `1e-6`, `1e-7`, and `1e-8`;
- maximum subspace sizes: `12`, `16`, `20`, `24`, `32`, and `48`;
- one 56-core node per case;
- 56 fixed ordered source blocks and 56 Rayon workers per case.

At full array concurrency this is 18 independent tasks and 1,008 active CPU
cores.  It is task-parallel ensemble throughput, not a claim that one Davidson
solve is distributed across 1,008 cores.

## Evidence boundary

The current solver uses deterministic shared-memory Rayon parallelism.  It has
no MPI-distributed CI vector.  Therefore this experiment can establish:

- Linux/SCNet portability at commit `48f1964a1b3b88090497e1ffce285fde09c98541`;
- cross-node numerical reproducibility;
- convergence sensitivity to tolerance and Davidson subspace;
- a distribution of end-to-end wall times on 56-core nodes;
- correctness of bounded H2O/cc-pVDZ all-electron source-column execution.

It cannot establish strong scaling of one calculation beyond one node or a
converged H2O/cc-pVDZ all-electron FCI energy.

## Staged execution

### Stage 1: immutable source and toolchain

Clone the public repository into a versioned shared-storage directory, detach
at the exact v0.4.0 commit, verify that `git status` is clean, and install the
official standalone Rust 1.89 Linux toolchain under the same run root.  SCNet
compute nodes have no external DNS, so a connected machine creates a
`cargo vendor --locked` tree and uploads it with the checksum-verified
toolchain; scheduled builds run with Cargo `--offline`.  Record the commit,
lockfile hash, compiler versions, operating system, and build-node metadata.

Cargo compilation uses node-local `$SLURM_TMPDIR`: the job exports the pinned
tree with `git archive`, copies one compressed vendor archive and expands it
locally, and places the entire Cargo target directory on local storage.  Only
the immutable release binary and evidence are returned to the shared
filesystem.  This avoids parallel-filesystem metadata stalls from Cargo's
many small files.

The `libcint-src` build normally performs a Git clone.  The offline bundle
therefore exports the tracked tree from libcint tag `v6.1.2`, upstream commit
`8d13863ff481cea27efea5e56c9e4d352cdb8f80`, into a non-shallow one-commit Git
snapshot tagged `v6.1.2`.  Its tree hash remains
`3de5cd4cf6b7f3fe04d53dfeed3dc85f69eb1133`.  The job verifies the archive and
tree hashes, expands it locally, loads CMake 3.25, and points `CINT_SRC` at
that local repository.

### Stage 2: build and smoke gate

Submit one scheduled build/smoke job.  It must:

1. build the release binary with `--locked`;
2. run all locked tests;
3. verify H2, H4, and H2O/STO-3G fixtures;
4. run one bounded H2O/cc-pVDZ all-electron source-column benchmark;
5. run one H2O/6-31G frozen-core Davidson calculation;
6. record binary/input/output hashes and exit statuses.

Because Slurm copies the submission script into its spool directory, the
versioned orchestration directory is passed explicitly as the absolute
`QH129_ORCHESTRATION` job environment variable.  Jobs never infer it from
`BASH_SOURCE`.

The production array is not submitted until this stage succeeds.

### Stage 3: 1,008-core robustness matrix

Submit a Slurm array `0-17%18`, with one 56-core node per task.  Array index
deterministically maps to one tolerance/subspace pair.  Every task writes to a
separate directory and records:

- Slurm job and array IDs;
- hostname, CPU model, allocated CPUs, and memory;
- Git commit, Cargo.lock hash, binary hash, and FCIDUMP hash;
- solver parameters and start/end timestamps;
- stdout, stderr, GNU time output, and final exit status.

Successful cases must report a finite energy, residual not exceeding the
requested tolerance, and the known energy within the selected tolerance.
Non-convergence is retained as data and must not be silently converted to a
successful result.

## Remote layout

The run root is:

```text
/work/share/giggleliu/cfys01/quantum-harness-129/
  source-v0.4.0/
  toolchains/
  runs/<job-id>/
    build-smoke/
    davidson-array/<array-index>/
```

No work is written to the over-quota user home directory.  A new job gets a
new result directory; existing results are never deleted or overwritten.

## Failure handling

- The scripts use explicit status files written on exit.
- A failed build or smoke gate prevents production submission.
- Each array task is isolated, so one non-convergent case does not erase other
  results.
- Resubmission targets only missing or failed array indices.
- Hash mismatches, unexpected source changes, or a missing binary fail closed.

## Acceptance gates

1. The build/smoke Slurm job exits zero.
2. Locked tests and small-system verification pass on SCNet.
3. The bounded cc-pVDZ artifact says `full_fci_executed = false`.
4. The primary H2O/6-31G Davidson smoke energy agrees with
   `-76.121174204141980 E_h` within its requested residual tolerance.
5. The array allocation is exactly 18 tasks × 56 CPUs = 1,008 CPUs at full
   concurrency.
6. Every completed task has a manifest, status, logs, time report, and hashes.
7. The final aggregate distinguishes convergence failures from infrastructure
   failures and does not claim MPI scaling.
