# Levels 1–4 Shared-Workbench Design

## Completion Scope

This design covers every remaining level in Quantum Harness issue #129:

- Level 1: alpha/beta excitation lists, direct sigma construction, Hamiltonian
  diagonal, Davidson, and the frozen-core H2O/6-31G target.
- Level 2: arbitrary-rank determinant-based CC(n), Taylor exponential,
  projected residuals, denominator updates, and DIIS.
- Level 3: CI(n), MBPT(n), and unitary CC(n), each executable and numerically
  verified rather than represented by placeholder interfaces.
- Level 4: direct Rust libcint integrals, restricted Hartree–Fock with DIIS,
  AO-to-MO transformation, and comparison with the PySCF path.
- Final accuracy/benchmark reports, provenance, and a tenferro-rs gap list.

## Alternatives Considered

Independent per-method implementations would optimize individual milestones
but duplicate indexing and sign logic. Wrapping external programs would produce
numbers quickly but would not constitute a Rust workbench. The selected design
uses one determinant/operator core and treats external packages only as
independent oracles.

## Shared Architecture

The workbench has three layers:

1. `basis`: lexical alpha/beta strings, ranking, excitation graphs, fermionic
   signs, determinant addressing, reference excitation rank.
2. `operators`: molecular Hamiltonian, diagonal, direct sigma, cluster
   operator, adjoint cluster operator, exponentials, and projected spaces.
3. `frontends`: FCIDUMP/PySCF fixtures and direct libcint/RHF/AO-to-MO
   integrals, both producing the same `ElectronicProblem` representation.

Numerical solvers (`Davidson`, `DIIS`, nonlinear CC, perturbation recursion,
and unitary optimization) consume only operator traits. This makes every solver
testable against the Level 0 dense matrix.

## Level 1 Data Flow

Alpha and beta strings have precomputed single-excitation links containing
source/target addresses, removed/added orbitals, and signs. The Olsen-style
sigma implementation contracts these link tables with one- and two-electron
integrals without storing the determinant Hamiltonian. A separate diagonal
kernel supplies the Davidson preconditioner.

Davidson uses modified Gram–Schmidt, a symmetric projected eigenproblem,
diagonal residual preconditioning, configurable convergence, thick restart,
and explicit residual/iteration reporting. Tiny-system random-vector tests
compare direct sigma with the Level 0 dense matrix before molecular targets are
accepted.

Frozen-core preprocessing folds core contributions into the active-space
one-electron Hamiltonian and core energy. The primary target is H2O/6-31G with
the oxygen 1s orbital frozen.

## Levels 2–3 Operator Algebra

Cluster amplitudes are indexed by reference-to-determinant excitation rank.
Applying `T` enumerates occupied-to-virtual substitutions relative to the
Hartree–Fock reference and reuses fermionic operator application. The Taylor
exponential accumulates `T^k/k!` until the finite excitation ceiling or a norm
threshold.

CC(n) calculates `H exp(T)|HF>`, energy, projected residuals through rank `n`,
orbital-denominator Jacobi steps, and DIIS. The implementation accepts any
rank allowed by the finite basis. CC(2) is checked against PySCF CCSD before
higher ranks are accepted.

CI(n) restricts the Davidson vector space by excitation rank. MBPT(n) uses a
Rayleigh–Schrödinger partition with the diagonal Fock operator and reports
order-by-order energy corrections. Unitary CC applies `A = T - T†`, evaluates
the finite matrix exponential action, and variationally optimizes amplitudes.

## Level 4 Integral Frontend

The Rust libcint crate supplies overlap, kinetic, nuclear-attraction, and
electron-repulsion AO integrals. The RHF solver performs symmetric
orthogonalization, density/Fock iteration, electronic and nuclear energy
evaluation, and DIIS. A staged four-index AO-to-MO transform produces the same
Mulliken-ordered spatial integrals accepted by the operator layer.

PySCF fixtures include AO and MO integral checksums/arrays for small molecules.
Acceptance compares individual integrals, RHF energy, orbital energies, and
the final FCI result. PySCF remains an oracle, not a runtime dependency.

## Error Handling and Limits

All public constructors validate dimensions, electron/spin parity, finite
values, and index ranges. Iterative solvers return structured non-convergence
reports rather than silently returning the last iterate. Large-memory targets
perform checked size estimates before allocation.

The initial bit representation supports at most 32 spatial orbitals. This
covers the challenge targets. Production-scale spatial symmetry and distributed
memory are outside the challenge requirements and are documented as future
optimization work.

## Verification Gates

- every Level 1 operator matches Level 0 dense results on H2/H4;
- H2O/STO-3G and frozen-core H2O/6-31G match PySCF FCI and the published
  `-76.121174` hartree anchor under reproduced settings;
- CC(2) matches PySCF CCSD and CC(n) reaches FCI at full rank on tiny systems;
- CI(full rank) equals FCI, MBPT reports reproducible order corrections, and
  unitary CC(full rank) reaches the tiny-system variational target;
- direct-libcint RHF/AO-to-MO results match PySCF references;
- formatting, Clippy, unit/integration tests, numerical CLI checks, provenance,
  and documentation all pass before completion is claimed.

