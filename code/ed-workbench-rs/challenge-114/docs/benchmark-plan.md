# Benchmark Plan

## First Benchmark Family: Small Eager Loops

Purpose: measure overhead in repeated small operations that appear in direct FCI
and coupled-cluster iteration.

Candidate operations:

| ID | Operation | Source in #129 | Correctness check |
|---|---|---|---|
| eager-dot-axpy | repeated dot, norm, axpy | Davidson residuals and DIIS | compare scalar outputs within dtype tolerance |
| eager-denom-update | element-wise residual / denominator update | CC amplitude iteration | compare output vector to reference |
| eager-mask-update | masked in-place update | excitation-restricted amplitudes | compare updated tensor values |

Required measurement fields:

- workload ID;
- tensor shapes;
- dtype;
- backend name and version;
- warmup count;
- repeat count;
- median runtime;
- peak memory if available;
- hardware profile path.

## Second Benchmark Family: Permutation and Indexed Access

Purpose: test layouts and access patterns that appear when determinant strings,
excitation tables, and integral contractions meet.

Candidate operations:

| ID | Operation | Source in #129 | Correctness check |
|---|---|---|---|
| permute-einsum-2body | transpose/permutation followed by contraction | two-electron integral handling | compare contraction output |
| indexed-scatter-add | gather rows by excitation table and scatter-add signs | sigma-vector construction | compare vector output |
| batched-small-contract | many small matrix products | orbital/integral contraction blocks | compare batch outputs |

## Reference Backends

- `tenferro-rs` for the Rust tensor path under test.
- PyTorch and JAX for independent Python references.
- `faer` or hand-written Rust loops where `tenferro-rs` does not naturally
  express the operation.

## Reporting Rule

Each benchmark result must link to:

- the workload spec;
- the hardware profile;
- the exact command;
- raw timing data;
- a short interpretation.

