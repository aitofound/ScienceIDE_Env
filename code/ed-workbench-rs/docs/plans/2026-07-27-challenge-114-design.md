# Challenge 114 Subfolder Design

## Purpose

Add a `challenge-114/` subfolder inside the #129 Rust ED/FCI workbench repository.
The subfolder tracks the #114 verification line: converting real #129 workload
patterns into reproducible `tenferro-rs` benchmark, oracle, and gap records.

## Recommended Approach

Use `challenge-114/` as a side workspace, not a separate crate yet. This keeps
the #114 material close to the #129 workloads that motivate it while avoiding a
premature dependency on the public `tenferro-benchmark` and `tensor-ad-oracles`
repository layouts.

## Directory Shape

```text
challenge-114/
  README.md
  docs/
    challenge-114-brief.md
    benchmark-plan.md
    gap-log.md
    upstream-repos.md
  workloads/
    level0-dense-fci/
    sigma-vector/
    amplitude-updates/
  benchmarks/
    permutation-einsum/
    eager-small-loops/
  results/
    README.md
  profiles/
    apple-silicon.json
```

## Data Flow

The #129 implementation produces concrete operation patterns: dense tiny-system
Hamiltonian construction, sigma-vector indexed accumulation, and CC amplitude
updates. The #114 subfolder describes those patterns as workload specs, then
maps each spec to benchmark entries comparing `tenferro-rs` against independent
references such as PyTorch, JAX, and native Rust linear algebra.

## Success Criteria

- A reader can tell why #114 lives inside this #129 repository.
- The first benchmark targets are explicit: small eager loops and
  permutation/indexed-access-heavy operations.
- The gap log has a stable schema for findings before any performance numbers
  exist.
- The hardware profile template is ready to fill with local Apple Silicon
  machine details.

