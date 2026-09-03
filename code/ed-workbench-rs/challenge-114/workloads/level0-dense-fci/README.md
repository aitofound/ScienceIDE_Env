# Workload: Level 0 Dense FCI

Source in #129: Level 0 tiny-system dense Hamiltonian construction for H2 and
linear H4 STO-3G fixtures.

## Operation Pattern

- Build dense Hamiltonian blocks from one- and two-electron integrals.
- Apply small matrix/vector operations for exact diagonalization checks.
- Compare energies and vector operations against PySCF-generated references.

## Benchmark Relevance

This workload can produce small dense contraction and permutation cases that are
easy to validate exactly before scaling to direct FCI.

