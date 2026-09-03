# Multi-root Davidson and Full-rank UCC Checks

Date: 2026-07-28

## Multi-root Davidson

The block-Davidson implementation optimizes several orthogonal Ritz vectors
in one shared subspace. Each root has its own energy, residual, and diagonal
preconditioned correction vector. When the subspace reaches its size limit,
the current Ritz vectors are retained together before expansion resumes.

Linear H₄/STO-3G provides a 36-determinant problem that can also be
diagonalized densely:

| Root | Block-Davidson energy | Dense energy | Excitation energy | Residual norm |
|---:|---:|---:|---:|---:|
| 0 | −2.166387448634757 Hartree | −2.166387448634763 Hartree | 0 Hartree | 3.416 × 10⁻¹¹ |
| 1 | −1.933757233514582 Hartree | −1.933757233514592 Hartree | 0.232630215120175 Hartree | 6.392 × 10⁻¹¹ |
| 2 | −1.719494142631181 Hartree | −1.719494142631180 Hartree | 0.446893306003576 Hartree | 7.856 × 10⁻¹¹ |

All three roots converged in 19 iterations and agree with independent dense
diagonalization within 1 × 10⁻¹⁴ Hartree. Their pairwise overlaps are below
1 × 10⁻¹⁰.

An important regression test protects against missing a low root. Starting
with only one coordinate vector per requested root can leave the initial
block without enough spin/symmetry content; a higher state may then have a
small residual while a lower state is absent. The implementation therefore
starts from up to twice as many lowest-diagonal coordinate vectors as
requested roots. The H₄ test would fail if the third state at
−1.719494142631180 Hartree were skipped.

Run the calculation with:

```bash
cargo run --release -- davidson-roots fixtures/h4-sto3g/FCIDUMP \
  --roots 3 \
  --residual-tolerance 1e-10 \
  --max-iterations 100 \
  --max-subspace 12
```

## Full-rank unitary coupled cluster

The previous UCC acceptance covered only H₂ with three parameters. The new H₄
test uses every excitation through rank four:

| Quantity | Value |
|---|---:|
| Parameters | 35 |
| Hartree–Fock energy | −2.098545936998005 Hartree |
| FCI energy | −2.1663874486347625 Hartree |
| UCC(4) energy | −2.166387448634763 Hartree |
| UCC(4) minus FCI | −4.44 × 10⁻¹⁶ Hartree |
| Gradient norm | 5.605 × 10⁻⁸ |
| BFGS iterations | 22 |

The apparent sub-femtohartree variational undershoot is floating-point
rounding. At meaningful precision, full-rank H₄/UCC reaches the FCI energy and
is lower than the zero-parameter Hartree–Fock state.

Run it with:

```bash
cargo run --release -- ucc fixtures/h4-sto3g/FCIDUMP \
  --rank 4 \
  --gradient-tolerance 1e-7 \
  --max-iterations 100
```

The UCC implementation evaluates finite-difference gradients. Its cost grows
by roughly two energy evaluations per parameter per optimizer step, so this
result validates the ansatz and anti-Hermitian action on a complete small
space; it does not make the present optimizer a production route for the
245,025- or 28,233,466-determinant systems.
