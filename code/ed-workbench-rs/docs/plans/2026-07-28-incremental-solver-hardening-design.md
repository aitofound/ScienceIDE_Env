# Incremental Solver Hardening Design

## Status

Approved on 2026-07-28 for implementation as post-PR incremental releases.
The existing PR #217 and its v0.1.1 evidence remain unchanged.

## Objective

Turn the validated challenge workbench into a more general, restartable, and
predictably parallel single-node solver without changing the published
CC(1)-CC(8), FCI, CI, or MBPT reference results.

The target platform is a single Apple Silicon or Linux workstation with:

- 16-128 GiB of RAM;
- local NVMe storage;
- a multi-core CPU;
- no MPI dependency.

H2O/cc-pVDZ all-electron remains a bounded resource and kernel benchmark. The
design does not claim that its 1,806,590,016-determinant full FCI problem is
converged. Out-of-core Davidson targets spaces for which a small number of
full vectors fit in RAM but the complete Davidson basis does not.

## Release Structure

### v0.2.0: Generality and Numerical Contracts

1. Replace the frozen-core-only helper with a checked `ActiveSpaceSpec` that
   selects frozen occupied and frozen virtual orbitals.
2. Return the original-to-active orbital map with the transformed problem.
3. Preserve `freeze_core` as a compatibility wrapper.
4. Replace unchecked combinatorial arithmetic with checked rank, unrank, and
   dimension functions.
5. Add open-shell, frozen-virtual, invalid-selection, overflow, and
   rank/unrank property tests.
6. Harden CC configuration and iteration reporting without changing the
   validated equations or default numerical path.

### v0.3.0: Restartable Davidson

1. Keep `lowest_eigenpair` as the in-memory compatibility API.
2. Add a Davidson workspace API supporting in-memory and disk-backed vector
   stores.
3. Store basis and sigma vectors as versioned little-endian binary files.
4. Store iteration state as a versioned JSON manifest written atomically.
5. Validate dimension, operator fingerprint, numerical configuration, vector
   sizes, and finite values before resuming.
6. Add CLI flags for workspace, resume, checkpoint cadence, and an explicit
   memory budget.
7. Keep only a bounded number of full vectors resident while projecting,
   orthogonalizing, and restarting. Disk backing reduces subspace residency;
   it does not remove the requirement that one input, output, residual, and
   work vector fit in memory.

### v0.4.0: Deterministic Single-Node Parallelism

1. Add an execution policy to the direct-FCI operator.
2. Partition source determinants into fixed ordered blocks.
3. Accumulate each block into a thread-local dense vector only after a
   conservative memory preflight.
4. Reduce partial vectors in block order so repeated runs with the same
   execution policy are bitwise reproducible.
5. Fall back to the serial kernel when the requested parallel workspace would
   exceed the configured budget.
6. Record the selected execution policy, thread count, workspace estimate,
   fallback status, timing, and checksum in benchmark output.
7. Add serial-versus-parallel correctness tests and an ignored release-mode
   scaling benchmark.

## Architecture

### Active-space transformation

`ActiveSpaceSpec` contains disjoint lists:

```rust
pub struct ActiveSpaceSpec {
    pub frozen_occupied: Vec<usize>,
    pub frozen_virtual: Vec<usize>,
}
```

`build_active_space` validates and canonicalizes the lists, folds only the
doubly occupied orbitals into `ecore` and `h1`, removes both occupied and
virtual frozen orbitals from `h1`, `eri`, and orbital energies, and returns:

```rust
pub struct ActiveSpaceResult {
    pub problem: ElectronicProblem,
    pub active_to_original: Vec<usize>,
    pub original_to_active: Vec<Option<usize>>,
}
```

The transformation keeps `MS2` unchanged and subtracts two electrons for each
frozen occupied orbital. It rejects overlapping lists, duplicate entries,
out-of-range orbitals, too many frozen occupied orbitals, removal of every
orbital, and a resulting electron/spin sector that does not fit the active
space.

### Checked combinatorics

Combination counts are computed in `u128` with greatest-common-divisor
reduction and converted to `usize` only at an allocation boundary.

The public functions are:

```rust
pub fn combination_count(n: usize, k: usize) -> Result<u128, CombinadicError>;
pub fn rank_occupation(bits: u64, norb: usize, nelec: usize)
    -> Result<u128, CombinadicError>;
pub fn unrank_occupation(rank: u128, norb: usize, nelec: usize)
    -> Result<u64, CombinadicError>;
```

`StringSpace` continues to cache strings and links for the present direct-FCI
kernel, but its address is derived from the checked combinadic rank and tested
against the stored lexical order. Invalid bit patterns and dimensions return
errors instead of overflowing or panicking.

### CC hardening

The equations, ranked subset convolution, warm starts, DIIS, and published
comparison remain unchanged. The changes are limited to:

- validation of finite positive tolerances and nonzero iteration/history
  limits;
- an explicit termination reason;
- finite-value checks after energy, residual, and amplitude updates;
- a stable serializable summary for every CC rank;
- regression tests proving all committed H2O/6-31G Table 2 values remain
  unchanged at their published precision.

### Davidson vector storage

The Davidson iteration uses a private `VectorStore` abstraction:

```rust
trait VectorStore {
    fn dimension(&self) -> usize;
    fn len(&self) -> usize;
    fn push(&mut self, vector: &[f64]) -> Result<(), DavidsonError>;
    fn load(&self, index: usize, output: &mut [f64]) -> Result<(), DavidsonError>;
    fn replace_all(&mut self, vectors: &[&[f64]]) -> Result<(), DavidsonError>;
}
```

`MemoryVectorStore` preserves the existing fast path. `DiskVectorStore` uses
one file per vector so restart truncation and replacement are atomic at the
manifest level. All disk paths stay inside an explicitly supplied workspace.
The implementation never deletes an unspecified or broad path.

The checkpoint manifest records:

- schema version;
- operator fingerprint and dimension;
- tolerances and subspace limits;
- completed iteration and previous Ritz energy;
- basis and sigma vector counts;
- last energy, residual norm, and convergence state;
- vector byte order and scalar type.

Resume rejects stale or incompatible workspaces. A checkpoint is committed by
writing a temporary manifest, syncing it, and renaming it over the previous
manifest after all referenced vector files have been synced.

### Parallel sigma

The serial `LinearOperator::apply` behavior remains the default. Parallel
execution is opt-in through an execution policy stored in
`DirectFciOperator`.

For `B` fixed source blocks and dimension `D`, the conservative additional
workspace is `8 * B * D` bytes plus vector headers. The policy is accepted
only when this estimate fits the caller's budget. Each block is accumulated
independently with Rayon. The resulting block vectors are collected in source
order and added to the output in that same order.

This strategy favors deterministic correctness and bounded single-node
operation over maximum thread count. It is appropriate for the current
245,025-determinant primary problem. It is not the final design for billion-
determinant distributed FCI.

## Data Flow

```text
FCIDUMP or direct integrals
        |
        v
ElectronicProblem
        |
        +--> ActiveSpaceSpec --> ActiveSpaceResult
        |
        v
DirectFciOperator + ExecutionPolicy
        |
        v
Davidson
   |                 |
   v                 v
memory vectors    disk workspace
   |                 |
   +------ Ritz / residual / restart ------+
                                          |
                                          v
                            result + checkpoint summary
```

CC continues to consume the same validated `DirectFciOperator`; it gains
stronger configuration validation and structured iteration results, not a new
Hamiltonian implementation.

## Error Handling

- No invalid orbital list, combination overflow, non-finite CC state,
  incompatible checkpoint, truncated vector, or memory-budget rejection may
  panic.
- Every failure includes the rejected value and the expected constraint.
- Existing compatibility APIs retain their current successful behavior.
- Disk writes are confined to the explicit workspace and use atomic manifest
  replacement.
- A parallel-memory preflight rejection falls back to serial only when the
  policy explicitly allows fallback; strict mode returns an error.

## Verification

### Fast CI

- Existing 71 unit and integration tests continue to pass.
- Active-space identity, frozen-core, frozen-virtual, mixed selection,
  open-shell, and error cases.
- Exhaustive rank/unrank round trips for small spaces plus large-boundary and
  overflow cases.
- In-memory versus disk-backed Davidson equality on H2, H4, and H2O/STO-3G.
- Interrupted checkpoint versus uninterrupted Davidson equality.
- Corrupt and incompatible checkpoint rejection.
- Serial versus parallel sigma within `1e-12`, plus bitwise repeatability for
  a fixed policy.
- Existing CC(1)-CC(8), CI(1)-CI(8), MBPT(1)-MBPT(20), RHF, and AO-to-MO
  regression gates.

### Release validation

- `cargo fmt --check`;
- `cargo clippy --all-targets --all-features -- -D warnings`;
- `cargo test --locked`;
- `scripts/verify-submission.sh`;
- release-mode H2O/6-31G FCI and CC series;
- bounded H2O/cc-pVDZ benchmark;
- a checkpoint/resume demonstration using a temporary workspace;
- serial and parallel timing/RSS/checksum report on the validation machine.

## Documentation and Compatibility

Each release receives:

- release notes with exact numerical changes and non-changes;
- CLI examples for new flags;
- a schema description for checkpoint and JSON output;
- migration notes preserving old commands;
- updated resource and unit statements;
- a statement that out-of-core storage is not a claim of converged
  H2O/cc-pVDZ full FCI.

The README release badge will link to the releases page rather than a fixed
old tag. `Cargo.toml` advances with each release; published tags are never
moved.

## Explicit Non-Goals

- MPI or multi-node execution;
- GPU acceleration;
- selected CI or PT2;
- automatic point-group symmetry;
- converged H2O/cc-pVDZ all-electron full FCI;
- replacing the independent PySCF oracle.
