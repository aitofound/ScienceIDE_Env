# Workload: Sigma Vector

Source in #129: planned direct FCI sigma-vector construction, sigma = H C.

## Operation Pattern

- Precomputed excitation tables select determinant-string transitions.
- Fermion sign factors multiply gathered vector entries.
- Results are accumulated back into output vector slots.

## Benchmark Relevance

This is the most important #129-derived stress test for `tenferro-rs` API
coverage because it mixes indexed gather, sign flips, and scatter-add behavior.

