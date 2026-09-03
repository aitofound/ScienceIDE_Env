# Gap Log

This log records candidate and confirmed `tenferro-rs` gaps discovered from
#129-derived workloads.

## Schema

| Field | Meaning |
|---|---|
| ID | Stable gap identifier |
| Status | candidate, measured, attributed, upstreamed, closed |
| Workload | Link to workload or benchmark spec |
| Operation | Operation family being tested |
| Backend comparison | tenferro-rs vs reference backend |
| Evidence | Result file, profiler output, or oracle mismatch |
| Suspected cause | GEMM backend, dispatch overhead, memory layout, missing fusion, API friction, or unknown |
| Upstream target | tenferro-rs, tenferro-benchmark, tensor-ad-oracles, or local only |
| Next action | Concrete next step |

## Candidate Gaps

| ID | Status | Workload | Operation | Backend comparison | Evidence | Suspected cause | Upstream target | Next action |
|---|---|---|---|---|---|---|---|---|
| gap-001 | candidate | `workloads/sigma-vector` | indexed scatter-add with signs | tenferro-rs vs Rust loop | none yet | API friction / indexed accumulation | tenferro-benchmark | write minimal workload spec |
| gap-002 | candidate | `workloads/amplitude-updates` | element-wise denominator update | tenferro-rs vs faer/vector loop | none yet | in-place element-wise ergonomics | tenferro-benchmark | define shapes and dtype |
| gap-003 | candidate | `workloads/level0-dense-fci` | permutation before contraction | tenferro-rs vs JAX/PyTorch | none yet | memory layout / permutation overhead | tenferro-benchmark | extract small reproducible case |

