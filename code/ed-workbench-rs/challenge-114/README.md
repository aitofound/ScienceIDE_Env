# Challenge 114 Verification Workspace

This subfolder tracks work for Quantum Harness
[#114](https://github.com/QuantumBFS/quantum.harness/issues/114):
agentic verification of the experimental Rust tensor library `tenferro-rs`.

It lives inside the #129 Rust ED/FCI workbench because #129 provides realistic
scientific workload patterns that can become #114 benchmark and gap records:

- dense tiny-system Hamiltonian construction;
- sigma-vector construction with indexed accumulation;
- coupled-cluster amplitude updates with element-wise and in-place operations;
- tensor and linear-algebra kernels that may or may not map cleanly to
  `tenferro-rs`.

## Goal

Turn #129 workload needs into reproducible benchmark and correctness records for
the Rust tensor ecosystem.

The first scoped targets are:

1. Small eager tensor loops.
2. Permutation-heavy or indexed-access-heavy contractions.

## Repository Links

- Challenge #114: https://github.com/QuantumBFS/quantum.harness/issues/114
- Challenge #129: https://github.com/QuantumBFS/quantum.harness/issues/129
- tenferro-rs: https://github.com/tensor4all/tenferro-rs
- tenferro-benchmark: https://github.com/tensor4all/tenferro-benchmark
- tensor-ad-oracles: https://github.com/tensor4all/tensor-ad-oracles

## Map

- [docs/challenge-114-brief.md](docs/challenge-114-brief.md) summarizes the
  upstream challenge and the local scope.
- [docs/benchmark-plan.md](docs/benchmark-plan.md) defines the first benchmark
  families.
- [docs/gap-log.md](docs/gap-log.md) records candidate and confirmed gaps.
- [docs/upstream-repos.md](docs/upstream-repos.md) tracks external repositories
  and future PR targets.
- [workloads/](workloads/) describes #129-derived operation patterns.
- [benchmarks/](benchmarks/) holds benchmark family specs and future scripts.
- [profiles/](profiles/) stores hardware/software environment profiles.
- [results/](results/) stores measurement summaries and raw-result indexes.

