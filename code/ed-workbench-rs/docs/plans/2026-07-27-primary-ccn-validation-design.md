# Primary H2O CC(n) Validation Design

Date: 2026-07-27

## Goal

Complete the missing mandatory acceptance path for Quantum Harness challenge
#129: run determinant-based CC(n) through connected octuple excitations for
equilibrium H2O/6-31G with the oxygen 1s core frozen, compare every order with
Hirata and Bartlett, Chemical Physics Letters 321, 216-224 (2000), Table 2,
and deliver reproducible accuracy evidence in the repository and upstream
solution PR.

The already implemented Level 3 CI(n) and MBPT(n) methods will be evaluated on
the same primary system because the challenge deliverables require accuracy
tables for Level 3 series that were attempted.

## Fixed Reproduction Contract

- Geometry: `R(O-H)=0.967 angstrom`, `angle(H-O-H)=107.6 degree`.
- Basis: 6-31G.
- Frozen orbitals: oxygen 1s, orbital index 0.
- Active space: 12 spatial orbitals and 8 electrons.
- Determinants: 245,025 without spatial-symmetry reduction.
- Orbital basis: restricted Hartree-Fock canonical orbitals.
- Energy unit: Hartree.
- CC convergence: residual norm at most `1e-6`; use a tighter energy-change
  threshold where practical.
- Published comparison: method total energy minus the FCI total energy,
  rounded to the `1e-6`-Hartree precision printed in Hirata 2000 Table 2.

## Current Bottleneck

`ClusterOperator::apply` scans every retained cluster amplitude for every
nonzero source determinant. On the primary system the amplitude counts are
1,424 for CC(2), 12,624 for CC(3), and 245,024 for CC(8). A one-iteration
CC(2) benchmark already spends about six seconds after the first amplitude
update; extending the same algorithm to CC(8) is not practical.

## Approaches Considered

### 1. Keep the all-source/all-amplitude scan

This needs the least code, but its cost is proportional to the full
determinant count times the retained-amplitude count. It cannot meet the
octuple-excitation target and is rejected.

### 2. Precompute every cluster transition edge

This makes each application a compact scatter-add. For the primary system,
however, CC(2) already has about 23 million valid edges and full-rank CC has
about 109 million. Storing source, target, amplitude, and phase for all edges
would consume hundreds of megabytes to multiple gigabytes. It also duplicates
information already implicit in the alpha/beta strings and is rejected.

### 3. Ranked subset-convolution expansion with warm starts

This is the selected design. Every determinant is represented by the holes and
particles that distinguish it from the Hartree-Fock reference. A cluster
excitation contributing to a target must be a spin-preserving subset of those
holes and particles. The exponential coefficients obey the exact graded
recurrence

```text
rank(mu) C_mu =
    sum_{nu subset mu, 0 < rank(nu) <= n}
        rank(nu) t_nu C_(mu\nu) phase(nu, mu\nu)
```

which follows from `D exp(T) = (D T) exp(T)` for the excitation-rank grading
operator `D`. Processing targets in increasing excitation rank calculates
`exp(T)|HF>` in one pass without repeated Taylor applications. Alpha and beta
subset partitions and their normalized fermionic signs are precomputed
separately and combined on demand, so no global transition-edge table is
stored.

The CC(n-1) amplitudes initialize their matching entries in CC(n), as
recommended by Hirata 2000. This reduces high-order iteration counts and makes
the full CC(1)-CC(8) series a single reproducible operation.

## Components

### Cluster expansion plan

`src/cluster.rs` gains an immutable plan built from a determinant basis,
Hartree-Fock reference, and excitation space. It stores:

- reference-relative excitation rank for every determinant;
- target determinant indices grouped by excitation rank;
- alpha- and beta-string subset partitions;
- determinant-index to amplitude-index lookup.

The plan evaluates the ranked recurrence for any amplitude vector matching the
same excitation space. The existing general `ClusterOperator::apply` remains
available for unitary CC and independent Taylor-series cross-checks.

### CC series solver

`src/coupled_cluster.rs` gains a series API that evaluates ranks 1 through a
requested maximum. It carries converged amplitudes in a dense,
determinant-indexed warm-start vector between ranks. Each rank still uses the
existing projected residual, orbital denominators, Jacobi update, and DIIS
logic.

The single-rank API remains compatible and uses the same ranked expansion.

### Published reference

`fixtures/h2o-631g-fc/hirata2000-table2.json` records:

- DOI, citation, table number, page, and printed precision;
- the exact molecular settings;
- equilibrium-column CI(1)-CI(8), MBPT(1)-MBPT(20), and CC(1)-CC(8)
  method-minus-FCI differences printed by the paper.

No value with more precision than the paper prints will be invented.

### CLI and report

A `cc-series` command prints one machine-readable row per rank with total
energy, method-minus-FCI difference, published difference, error against the
published rounded value, iterations, residual, elapsed time, and convergence.

The Level 2 and Level 3 reports receive the primary-system tables. The upstream
solution README and reproduction prompt will point to stable commands and
committed artifacts rather than making unsupported completion claims.

## Error Handling

- Reject a maximum CC rank of zero or above the active-electron ceiling.
- Reject warm-start vectors whose determinant-space length is wrong.
- Reject published-reference files whose units, geometry, active-space
  dimensions, method names, or rank coverage do not match the run.
- A series command fails if any requested rank does not converge.
- Published comparison uses interval-aware acceptance: a value printed to six
  decimals represents its rounded interval, so the computed difference must
  round to the same six-decimal value.

## Verification

1. On H2 and H4, compare ranked recurrence coefficients against the existing
   Taylor expansion for deterministic nonzero amplitudes at every supported
   rank.
2. Exhaustively compare precomputed alpha/beta partition signs with direct
   fermionic excitation application on small determinant spaces.
3. Keep all existing CC energy and residual tests unchanged.
4. Verify H2O/6-31G CC(2) against the committed PySCF CCSD total energy.
5. Verify CC(1)-CC(8) method-minus-FCI differences round to Hirata Table 2.
6. Run CI(1)-CI(8) and MBPT(1)-MBPT(20) on the primary fixture and compare
   their published rounded differences.
7. Run formatting, Clippy with warnings denied, the complete Rust and Python
   test suites, numerical fixture checks, and a clean-worktree audit.

## Delivery

The implementation, published reference, generated results, reports, benchmark
notes, design decisions, and reproduction prompt are committed and pushed to
the workbench repository. The open Quantum Harness solution PR #217 is then
updated under `tracks/ed/solutions/WangTheoPhys/` with the final evidence and
reproduction instructions.
