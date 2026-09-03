# tenferro-rs Gap Audit for Quantum Harness #129

Audit date: 2026-07-27
Workload: determinant FCI, Davidson, arbitrary-rank CC/CI/MBPT/UCC, RHF, and
AO-to-MO

## Bottom Line

tenferro-rs 0.2.0 already has substantially more of the needed dense-tensor
surface than an early inventory suggested: typed tensors, strided views,
element-wise division, reductions, dot-general/einsum, and backend gather and
scatter traits are documented. It is useful for dense contractions and future
accelerator work.

This implementation does not add tenferro as a dependency. The performance
critical operation in #129 is an irregular, signed, collision-heavy
scatter-add over precomputed determinant excitation links. The documented
0.2.0 `scatter` configuration describes index/window dimensions but exposes no
reduction/combine mode. Expressing the current kernel would therefore require
extra sorting/segmentation or materialized intermediates, while the direct
Rust loop performs one deterministic `output[target] += value`.

## Operation-by-Operation Audit

| Requirement from #129 | Workbench use | tenferro-rs 0.2.0 status | Finding |
|---|---|---|---|
| Indexed gather | Read CI coefficients at source determinant indices | `TensorIndexing::gather` exists with StableHLO-style `GatherConfig` | Available, but verbose for a one-dimensional index list; a convenience `index_select`/`take` API would help |
| Scatter-add with duplicate indices | Accumulate many signed excitation contributions into the same sigma/residual entry | `TensorIndexing::scatter` exists; `ScatterConfig` contains only dimension mapping fields | **Primary missing primitive:** no documented add/reduce combiner or duplicate-index contract |
| Mutable views and slicing | Update Davidson vectors, amplitude blocks, DIIS history, occupied coefficient blocks | typed immutable and mutable views, arbitrary strides, slice metadata, and mutable scalar access are documented | Host mutation is usable; backend operations still often return new tensors, so in-place view-oriented kernels need clearer first-class support |
| Element-wise division | Davidson and CC/MBPT denominator preconditioning | `div` is part of tensor element-wise operations | Available; zero-denominator policy remains application-owned |
| In-place BLAS-1 | repeated `axpy`, dot, norm, scale, normalize | dot/reduction and element-wise building blocks exist; no documented high-level in-place AXPY surface was found | A fused `axpy_into`, `scale_in_place`, and `norm2` vector API would reduce allocations and dispatch overhead |
| Memory layout | PySCF/libcint and FCIDUMP-facing four-index data use explicit row-major indexing; dense linear algebra is column-major | owned tenferro tensors are compact column-major; strided views represent transposes/slices | Semantics are clear, but row-major external buffers need a documented zero-copy adapter recipe and backend coverage tests |
| Sparse excitation tables | signed alpha/beta links and arbitrary-rank substitutions | tenferro is primarily a dense tensor stack | A small CSR/COO-like segmented gather/scatter extension would be more natural than encoding links as dense tensors |
| Deterministic accumulation | reproducible f64 energies and residual norms | backend/device reductions may choose different orders | Define or document deterministic scatter-reduce and reduction modes for oracle workloads |
| Repeated output reuse | every sigma and Davidson iteration reuses allocated vectors | some low-level read/write and read-into contracts exist, while most high-level APIs materialize results | Extend output-buffer APIs consistently across element-wise, reduction, indexing, and contraction families |

## Concrete Workbench Sites

- `src/direct_fci.rs`: signed sigma accumulation into destination determinants;
- `src/cluster.rs`: arbitrary-rank substitution and collision-heavy amplitude
  accumulation;
- `src/davidson.rs`: in-place AXPY, normalization, dot, norm, and diagonal
  division;
- `src/coupled_cluster.rs` and `src/mbpt.rs`: element-wise denominator updates;
- `src/diis.rs`: repeated linear combinations into an existing vector;
- `src/rhf.rs`: dense generalized eigensystems, matrix products, reductions,
  views, and DIIS;
- `src/ao2mo.rs`: four successive dense tensor contractions with an explicit
  boundary layout.

The large Level 1 water/6-31G run has 245,025 determinants. For that workload,
materializing one or more index/value tensors per excitation class would add
memory traffic to a kernel that currently streams compact link records and CI
coefficients.

## Proposed Upstreamable Items

1. Add `scatter_reduce` or `scatter_add` with an explicit duplicate-index
   contract, deterministic CPU behavior, and clearly specified GPU behavior.
2. Add ergonomic one-axis `take`/`index_select` and `index_add` wrappers over
   the StableHLO-compatible general configurations.
3. Provide typed BLAS-1 output-buffer operations:
   `axpy_into(alpha, x, y)`, `scale_in_place`, `dot`, and `norm2`.
4. Make borrowed `TensorRead` and mutable `TensorWrite`/view coverage uniform
   across element-wise, reductions, indexing, and dot/einsum APIs.
5. Add a documented row-major foreign-buffer adapter and tests that prove no
   copy for transpose/stride views where a backend supports them.
6. Add a determinant-sigma reproducer to `tenferro-benchmark`, comparing:
   native scalar accumulation, sorted segmented reduction, and backend
   scatter-add.
7. Add oracle tests for repeated indices, signed updates, empty index sets,
   zero-sized tensors, and deterministic f64 accumulation.

## Minimal Reproducer Shape

A useful upstream benchmark does not need chemistry:

```text
input:   x[n_source]
links:   (source[k], target[k], phase[k], weight[k])
output:  y[n_target] initially zero
kernel:  y[target[k]] += phase[k] * weight[k] * x[source[k]]
```

The test data should contain heavily repeated target indices, sorted and
unsorted links, empty link sets, and both cancellation and reinforcement.
Correctness is checked against the scalar Rust loop. Benchmarks should report
allocation count, bytes moved, throughput, and reproducibility across thread
counts.

## Sources Checked

- Repository: https://github.com/tensor4all/tenferro-rs
- Current guide: https://tensor4all.org/tenferro-rs/
- Tensor API 0.2.0: https://docs.rs/tenferro-tensor/0.2.0/tenferro_tensor/
- `TensorIndexing`:
  https://docs.rs/tenferro-tensor/0.2.0/tenferro_tensor/backend/trait.TensorIndexing.html
- `GatherConfig`:
  https://docs.rs/tenferro-tensor/0.2.0/tenferro_tensor/config/struct.GatherConfig.html
- `ScatterConfig`:
  https://docs.rs/tenferro-tensor/0.2.0/tenferro_tensor/config/struct.ScatterConfig.html
- Memory-order guide:
  https://tensor4all.org/tenferro-rs/guides/memory-order.html
- Related Quantum Harness records:
  https://github.com/QuantumBFS/quantum.harness/issues/114 and
  https://github.com/QuantumBFS/quantum.harness/issues/115

This is an evidence-backed downstream gap list, not a claim that the operations
cannot be prototyped from lower-level primitives. The distinction is whether
the required determinant kernel maps directly, allocation-efficiently, and
with specified duplicate-index semantics to the documented public API.
