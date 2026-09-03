# Extended H2O/DZ All-Electron Benchmark

Date: 2026-07-28

## Result

| Quantity | Value |
|---|---:|
| Spatial orbitals | 14 |
| Electrons | 10 |
| Frozen orbitals | none |
| Determinants without spatial symmetry | 4,008,004 |
| Determinants in the target `ISYM=1` sector | 1,002,708 |
| Published FCI energy | -76.156699 Hartree |
| PySCF 2.14.0 FCI energy | -76.156699030929800 Hartree |
| Rust Davidson FCI energy | -76.156699030930056 Hartree |
| Absolute Rust-PySCF difference | 2.56 × 10⁻¹³ Hartree |
| Davidson residual norm | 5.091 × 10⁻⁸ |
| Davidson iterations | 17 |
| Release-mode wall time | 149.15 seconds |

The Rust and PySCF values round to the six decimal places printed for the
Kállay 2001 Table II FCI anchor.

## Reproduced Hamiltonian

This target is not cc-pVDZ and does not use the equilibrium geometry of the
primary 6-31G fixture.

- Geometry from Bauschlicher and Taylor 1986 Table II, in Bohr:
  O at `(0, 0, 0)` and H at `(±1.494187, 0, 1.156923)`.
- Derived bond length: 1.889726334392893 Bohr, approximately 1.000000111 Å.
- Derived bond angle: 104.50000893084858 degrees.
- Oxygen basis from Table I: `(9s5p)/[4s2p]`.
- Hydrogen basis from Table I: `(4s)/[2s]`.
- Restricted-Hartree-Fock canonical orbitals.
- All ten electrons correlated.
- C2v spatial symmetry, with one-based Molpro `ORBSYM` labels and `ISYM=1`.

The exact exponents and contraction coefficients are encoded in
`bauschlicher_1986_basis()` in `scripts/oracle/generate.py` and protected by
Python regression tests. This avoids the small numerical differences between
the paper's printed basis and similarly named modern basis-library entries.

## Reproduction

Regenerate the independent PySCF oracle:

```bash
uv run --frozen python scripts/oracle/generate.py h2o-dz-ae
```

Run the Rust solver:

```bash
cargo run --release -- davidson fixtures/h2o-dz-ae/FCIDUMP \
  --residual-tolerance 1e-7 \
  --max-iterations 40 \
  --max-subspace 20
```

Run the lightweight committed-data test:

```bash
cargo test --test extended_dz committed_dz_oracle_matches_the_published_fci_anchor
```

Run the approximately 150-second live numerical test:

```bash
cargo test --release --test extended_dz live_dz_davidson_matches_pyscf -- --ignored
```

## Sources and Scope

- Kállay and Surján, J. Chem. Phys. 115, 2945 (2001),
  DOI: https://doi.org/10.1063/1.1383290.
- Bauschlicher and Taylor, J. Chem. Phys. 85, 2779 (1986),
  DOI: https://doi.org/10.1063/1.451034.
- Open NASA manuscript for the basis and geometry tables:
  https://ntrs.nasa.gov/api/citations/19860020903/downloads/19860020903.pdf.
- Quantum Harness challenge #129:
  https://github.com/QuantumBFS/quantum.harness/issues/129.

The FCI anchor is validated here. The complete Kállay Table II CC/CI series is
not claimed because the full paper table was not available through the
open-access sources used for this run.
