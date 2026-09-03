# Stretched H₂O/6-31G Frozen-Core Benchmark

Date: 2026-07-28

## Result

The equilibrium geometry from the primary challenge was stretched uniformly
while keeping the H–O–H angle fixed. All three Hamiltonians use restricted
Hartree–Fock canonical orbitals, the 6-31G basis, and a frozen oxygen 1s
orbital. Each active problem has 12 spatial orbitals, 8 electrons, and 245,025
determinants.

| Geometry | O–H distance | H–O–H angle | PySCF FCI energy | Rust FCI energy | Difference |
|---|---:|---:|---:|---:|---:|
| Rₑ | 0.967 Å | 107.6° | −76.121174204141970 Hartree | −76.121174204141980 Hartree | −1.0 × 10⁻¹⁴ Hartree |
| 1.5 Rₑ | 1.4505 Å | 107.6° | −75.985788430062040 Hartree | −75.985788430060770 Hartree | +1.28 × 10⁻¹² Hartree |
| 2.0 Rₑ | 1.934 Å | 107.6° | −75.876474181025960 Hartree | −75.876474181027560 Hartree | −1.61 × 10⁻¹² Hartree |

The 1.5 Rₑ Davidson calculation converged in 22 iterations with a residual norm
of 7.846 × 10⁻⁸. The 2.0 Rₑ calculation converged in 31 iterations with a
residual norm of 9.420 × 10⁻⁸. The increased iteration count is consistent
with the harder, more strongly correlated stretched-bond problem.

## Coupled-cluster rank convergence

The table reports each Rust CC(n) energy relative to the independent PySCF FCI
energy. A positive value lies above FCI; a negative value lies below FCI.
Coupled-cluster energy is not variational, so a negative value is allowed.

| Rank | 1.5 Rₑ: CC(n) − FCI | 2.0 Rₑ: CC(n) − FCI |
|---:|---:|---:|
| 1 | +1.97555905581922 × 10⁻¹ Hartree | +2.95881429455505 × 10⁻¹ Hartree |
| 2 | +5.710373623387 × 10⁻³ Hartree | +9.846113576231 × 10⁻³ Hartree |
| 3 | +1.199446007050 × 10⁻³ Hartree | −1.964868835273 × 10⁻³ Hartree |
| 4 | +9.7229494415 × 10⁻⁵ Hartree | +1.01867893250 × 10⁻⁴ Hartree |
| 5 | +1.4623431625 × 10⁻⁵ Hartree | +2.2916592798 × 10⁻⁵ Hartree |
| 6 | +6.94354142 × 10⁻⁷ Hartree | +1.576767460 × 10⁻⁶ Hartree |
| 7 | +4.2488793 × 10⁻⁸ Hartree | +1.3786263 × 10⁻⁸ Hartree |
| 8 | +4.0969809 × 10⁻⁸ Hartree | −5.287632 × 10⁻⁹ Hartree |

Every rank converged below the requested residual threshold of 1 × 10⁻⁶. Rust
CC(2) also agrees with independently converged PySCF CCSD within 3 × 10⁻⁸
Hartree at 1.5 Rₑ and 5 × 10⁻⁹ Hartree at 2.0 Rₑ.

The most informative feature is the 2.0 Rₑ CC3 overshoot: its energy is about
1.965 × 10⁻³ Hartree below FCI. This is not an FCI error. It demonstrates the
nonvariational and nonmonotonic behavior of truncated coupled cluster in a
strongly correlated regime. CC4 returns above FCI, and CC7–CC8 recover the
full-space energy to within 1.4 × 10⁻⁸ Hartree.

## Reproduction

Regenerate the independent PySCF fixtures:

```bash
uv run --frozen python scripts/oracle/generate.py \
  h2o-631g-fc-r1p5 h2o-631g-fc-r2p0
```

Run Rust Davidson FCI for one stretched geometry:

```bash
cargo run --release -- davidson fixtures/h2o-631g-fc-r2p0/FCIDUMP \
  --residual-tolerance 1e-7 \
  --max-iterations 100 \
  --max-subspace 24
```

Run the complete Rust CC sequence:

```bash
cargo run --release -- cc-series \
  fixtures/h2o-631g-fc-r2p0/FCIDUMP \
  fixtures/h2o-631g-fc-r2p0/reference.json \
  --max-rank 8 \
  --residual-tolerance 1e-6 \
  --max-iterations 100
```

Run the fast committed-evidence tests:

```bash
cargo test --test stretched_water
```

The two ignored live tests repeat about seven minutes of release-mode FCI and
CC calculations:

```bash
cargo test --release --test stretched_water -- --ignored
```

## Scope

These are new calculations for the explicit 1.5 Rₑ and 2.0 Rₑ Hamiltonians,
not comparisons to the equilibrium column of Hirata 2000. The committed
machine-readable records include the geometry, units, FCIDUMP checksum,
iteration thresholds, energies, residuals, and wall times. The stretched
fixtures therefore test the Rust implementation outside the single
equilibrium geometry without assigning a literature claim to the wrong
Hamiltonian.
