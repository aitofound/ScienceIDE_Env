# Incremental Solver Validation

Date: 2026-07-28

Scope: post-v0.1.1 hardening of active spaces, determinant addressing, CC
diagnostics, Davidson storage/restart, validation coverage, deterministic CPU
parallelism, and release documentation.

## Acceptance summary

| Area | Acceptance evidence | Result |
|---|---|---|
| CC(1)-CC(8) hardening | typed configuration/state errors, explicit termination, JSON output, committed Table 2 regression | PASS |
| Matrix-free FCI/Davidson | old in-memory API plus disk vector store and resume | PASS |
| Active space | occupied and virtual freezing, maps, open-shell preservation, invalid-selection tests | PASS |
| Combination rank/unrank | exhaustive small spaces and exact `C(64,32)` boundary | PASS |
| Numerical matrix | closed shell, open shell, memory, disk, serial, parallel, dense/reference fixtures | PASS |
| Parallelization | fixed ordered blocks, preflight, strict rejection/fallback, reproducibility, primary timing | PASS |
| Documentation/release | v0.2-v0.4 notes, checkpoint schema, README migration and scope statements | PASS |

## v0.2: generality and numerical contracts

### Active-space transformation

The general transformation accepts disjoint `frozen_occupied` and
`frozen_virtual` lists and returns:

- the transformed `ElectronicProblem`;
- `active_to_original`;
- `original_to_active`.

The tests cover identity, frozen core, frozen virtual, mixed removal,
duplicate occupied/virtual entries, overlap, range errors, too many frozen
electrons, removal of all orbitals, and a `NELEC=3`, `MS2=1` open-shell
problem reduced to `NELEC=1`, `MS2=1`.

The established `freeze_core` entry point delegates to the general
transformation and produces identical arrays.

### Combinadic boundary

`combination_count`, `rank_occupation`, and `unrank_occupation` use checked
`u128` arithmetic. Exhaustive tests reproduce numeric lexical order through
12 orbitals. The largest balanced `u64` occupation space is exact:

```text
C(64,32) = 1,832,624,140,942,590,534
```

The last valid rank round-trips. Invalid populations, external bits,
out-of-range ranks, more than 64 orbitals, count overflow, and allocation
boundaries are typed errors.

### Coupled cluster

CC rejects invalid tolerances, iteration count, DIIS history, and
exponential threshold before iteration. Energy, residual, updated amplitudes,
and DIIS output are checked for finite values. Results distinguish
`converged` from `maximum_iterations`.

`cc-series --json-output` records the configuration, every rank, energy,
residual, iteration count, termination, elapsed time, and optional published
comparison. The established committed H2O/6-31G result still matches all eight
Hirata and Bartlett 2000 CC entries at printed precision.

## v0.3: restartable Davidson

The default `lowest_eigenpair` API remains in memory. The new run API can
store basis and sigma vectors in a local workspace:

```text
checkpoint.json
basis/generation-N/vector-N.bin
sigma/generation-N/vector-N.bin
results/result-N.bin
```

The manifest and binary contract are documented in
[`docs/checkpoint-format.md`](../docs/checkpoint-format.md).

Acceptance tests prove:

- a one-iteration interruption resumes to the uninterrupted energy,
  residual, and phase-aligned eigenvector;
- an operator fingerprint mismatch is rejected;
- a truncated vector is rejected;
- a fresh run refuses a nonempty workspace and preserves its unrelated file;
- CLI help exposes every workspace and memory option;
- the old H2, H4, H2O/STO-3G, CI, and direct-integral callers remain valid.

Disk backing changes subspace residency, not vector size. The conservative
solver-vector estimate is:

```text
disk workspace:  7 × dimension × 8 bytes
memory storage:  (2 × max_subspace + 6) × dimension × 8 bytes
```

It excludes operator/link/integral storage, allocator overhead, projected
linear algebra, and filesystem cache.

## v0.4: deterministic parallel sigma

### Correctness and memory policy

Parallel direct FCI divides source determinants into fixed contiguous blocks.
Every Rayon task accumulates one dense partial vector. Partial vectors are
reduced sequentially in block order.

For a dimension `D` and `B` effective blocks, preflight workspace is:

```text
8 × B × D bytes
```

Strict mode rejects an insufficient budget before partial-vector allocation.
Fallback mode records the reason and runs the serial kernel. The default
remains serial.

H2, H4, and H2O/STO-3G tests show:

- serial/parallel maximum error below `1e-11`;
- two fixed-policy parallel executions are bitwise identical;
- zero blocks are rejected;
- strict budget rejection and explicit fallback both work.

An analytic one-electron doublet (`NELEC=1`, `MS2=1`) gives
`-0.9 E_h` through in-memory serial Davidson and disk-backed parallel
Davidson.

### Primary H2O/6-31G measurement

Committed evidence:
[`fixtures/h2o-631g-fc/parallel-sigma-m4.json`](../fixtures/h2o-631g-fc/parallel-sigma-m4.json)

Environment:

| Item | Value |
|---|---|
| CPU | Apple M4 |
| architecture | arm64 |
| memory | 16 GiB |
| operating system | macOS 15.6 |
| Rust/Cargo | 1.95.0 / 1.95.0 |
| profile | release |
| Rayon workers | 10 |
| source blocks | 4 |
| determinants | 245,025 |
| input | dense deterministic `sin(index * 31 + 7)` |

Five fresh release processes:

| Run | Serial sigma (s) | Parallel sigma (s) | Maximum difference |
|---:|---:|---:|---:|
| 1 | 15.844449750 | 4.557638541 | `5.969e-13` |
| 2 | 14.011385375 | 4.381184834 | `5.969e-13` |
| 3 | 15.857972792 | 4.009908542 | `5.969e-13` |
| 4 | 14.047785334 | 4.137299041 | `5.969e-13` |
| 5 | 14.181091542 | 4.761762208 | `5.969e-13` |
| **Median** | **14.181091542** | **4.381184834** | **`5.969e-13` maximum** |

The ratio of medians is **3.236817x**. This is a measured fixed-workload
result, not a claim of linear scaling. Serial runs before parallel in each
process, so cache and thermal order are explicit rather than hidden.

The four partial vectors require a preflighted 7,840,800 bytes. A separate
timed process containing serial followed by parallel sigma reached 48,709,632
bytes maximum RSS. This process RSS is not a per-mode decomposition.

## Preserved published results

The hardening releases do not change the primary numerical claims:

| Quantity | Value |
|---|---:|
| H2O/6-31G frozen-core determinants | 245,025 |
| direct Davidson FCI | `-76.121174204141980 E_h` |
| CC(2) | `-76.119629519205702 E_h` |
| CC(8) | `-76.121174196144139 E_h` |
| CC Table 2 | 8/8 matched |
| CI/MBPT Table 2 | 28/28 matched |

The live CC(1)-CC(8) release gate remains the authoritative recomputation.

## Explicit scalability boundary

H2O/cc-pVDZ all-electron without point-group symmetry contains
1,806,590,016 determinants. One full `f64` vector is 13.460145 GiB. The
bounded benchmark continues to execute integrals, RHF, AO-to-MO, link tables,
and sampled sparse columns without allocating a full CI vector.

Neither disk-backed subspace storage nor four-block parallel sigma is
presented as a converged full-FCI solution for that billion-determinant
problem.

## Commands

Fast and static gates:

```bash
cargo fmt --check
cargo clippy --all-targets --all-features -- -D warnings
cargo test --locked
scripts/verify-submission.sh
```

Primary parallel measurement:

```bash
RAYON_NUM_THREADS=10 cargo test --release --test parallel_sigma \
  primary_water_parallel_sigma_measurement -- --ignored --nocapture
```

Primary CC recomputation:

```bash
RAYON_NUM_THREADS=10 cargo test --release --test cc_series \
  live_primary_cc_series_matches_hirata_table2 -- --ignored --nocapture
```

## Final release-mode audit

The final v0.4 source state was audited with live calculations in addition to
the normal static and fast gates.

### Parallel primary Davidson FCI

The optimized binary ran:

```bash
target/release/ed_workbench_rs davidson \
  fixtures/h2o-631g-fc/FCIDUMP \
  --residual-tolerance 1e-7 \
  --max-iterations 60 \
  --max-subspace 20 \
  --parallel-blocks 4 \
  --parallel-memory-budget-gib 2 \
  --strict-parallel-memory
```

Result:

| Quantity | Value |
|---|---:|
| energy | `-76.121174204142051 E_h` |
| difference from committed FCI | `7.1e-14 E_h` |
| residual norm | `5.044e-8` |
| Davidson iterations | 16 |
| effective sigma mode | parallel, 4 source blocks |
| parallel sigma workspace | `0.007302 GiB` |
| pure-binary wall time | `41.90 s` |
| maximum RSS | `102,285,312 bytes` |

### Live CC(1)-CC(8)

The final release test recomputed every CC rank and returned:

```text
live_primary_cc_series_matches_hirata_table2 ... ok
```

All eight published comparisons passed. This run took `751.74 s` of test wall
time, while `/usr/bin/time` recorded only `278.23 s` of user CPU time. The
large wall/user discrepancy shows that this run was heavily affected by local
scheduling or frequency conditions. It is retained as a numerical acceptance
result and does not replace the separately committed primary timing baseline.

### Bounded H2O/cc-pVDZ

The final release-mode ignored integration test passed in `1.94 s` after
build:

```text
live_cc_pvdz_benchmark_is_bounded_and_matches_pyscf_rhf ... ok
```

It reconfirmed the all-electron/no-symmetry dimensions, bounded memory path,
RHF convergence and PySCF agreement, and `full_fci_executed = false`.
