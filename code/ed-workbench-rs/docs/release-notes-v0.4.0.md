# v0.4.0 — Deterministic bounded-memory parallel sigma

v0.4.0 completes the planned single-node solver-hardening series.

## Parallel direct FCI

The direct-FCI operator now accepts an explicit execution policy:

```rust
let operator = operator.with_execution_policy(ExecutionPolicy::Parallel {
    blocks: 4,
    memory_budget_bytes: 2 * 1024_u64.pow(3),
    allow_serial_fallback: false,
})?;
```

Source determinants are partitioned into fixed ordered blocks. Each Rayon task
accumulates a private dense vector, and partial vectors are reduced in block
order. A fixed policy is therefore bitwise repeatable regardless of task
scheduling.

The implementation preflights `8 × blocks × dimension` bytes before
allocation. Strict mode returns a typed error; fallback mode records why it
used the serial kernel.

The CLI exposes:

```text
--parallel-blocks
--parallel-memory-budget-gib
--strict-parallel-memory
```

on `davidson` and `direct-integrals-fci`. The default remains serial.

## Measured primary result

On an Apple M4 with 10 Rayon workers, four blocks, and the
245,025-determinant H2O/6-31G frozen-core problem:

| Kernel | Median of five fresh processes |
|---|---:|
| serial sigma | 14.181091542 s |
| parallel sigma | 4.381184834 s |
| ratio of medians | 3.236817x |

Maximum serial/parallel difference was `5.969e-13`. The four partial vectors
used a preflighted 7,840,800-byte workspace. Raw runs and aggregates are
committed in
[`fixtures/h2o-631g-fc/parallel-sigma-m4.json`](../fixtures/h2o-631g-fc/parallel-sigma-m4.json).

## Expanded validation

- H2, H4, and H2O/STO-3G serial/parallel sigma comparisons;
- bitwise repeatability for a fixed block policy;
- strict memory rejection and explicit serial fallback;
- analytic open-shell doublet through memory/serial and disk/parallel paths;
- machine-readable measurement aggregation test;
- every pre-existing FCI, CC, CI, MBPT, UCC, RHF, and integral test.

See the
[`incremental solver validation report`](../reports/incremental-solver-validation.md)
for the complete v0.2-v0.4 evidence.

## Scope boundary

This release targets one multi-core CPU workstation. It does not add MPI,
GPU, selected CI, PT2, or point-group symmetry. It does not claim converged
H2O/cc-pVDZ all-electron full FCI.
