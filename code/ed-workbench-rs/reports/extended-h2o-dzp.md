# Extended H₂O/DZP Frozen-Core Benchmark

Date: 2026-07-28

## Result

| Quantity | Value |
|---|---:|
| Spatial orbitals after freezing the core | 24 |
| Active electrons | 8 |
| Determinants without spatial symmetry | 112,911,876 |
| Determinants in the C₂ᵥ target sector | 28,233,466 |
| Published FCI energy | −76.256624 Hartree |
| Rust Davidson FCI energy | −76.256624441300147 Hartree |
| Rust minus published printed value | −4.413 × 10⁻⁷ Hartree |
| Davidson residual norm | 9.342 × 10⁻⁸ |
| Davidson iterations | 20 |
| Release-mode wall time | 1047.48 seconds |
| Maximum resident memory | 6.67 GB |
| Swap count | 0 |

The paper prints six digits after the decimal point. The converged Rust value
rounds to −76.256624 Hartree and therefore matches the published FCI anchor at
the available precision. The difference from the printed number must not be
interpreted as an error against an unprinted high-precision literature value.

## Hamiltonian

This calculation uses the Bauschlicher and Taylor 1986 input reproduced from
the printed tables:

- oxygen at (0, 0, 0) Bohr;
- hydrogen atoms at (±1.494187, 0, 1.156923) Bohr;
- O–H distance 1.889726334392893 Bohr;
- H–O–H angle 104.50000893084858 degrees;
- oxygen (9s5p)/[4s2p] basis plus one d function with exponent 1.2;
- hydrogen (4s)/[2s] basis plus one p function with exponent 0.8;
- restricted-Hartree–Fock canonical molecular orbitals;
- frozen oxygen 1s orbital;
- C₂ᵥ spatial symmetry with one-based Molpro labels and target ISYM = 1.

PySCF 2.14.0 generated the RHF orbitals, active-space integrals, MP2 value,
and independently converged CCSD value. The generator deliberately skipped
PySCF FCI because the active symmetry block contains more than 28 million
determinants. That size guard is recorded as `skipped-size-guard` in
`generation_metadata.json`; the Rust FCI result is stored separately.

## Scaling work required

The previous determinant basis stored every determinant, every (α, β) pair, a
full pair-address table, and a hash map. For this DZP problem, just the first
three arrays would require about 1.58 GB before the hash map, diagonal, or any
Davidson vector was allocated.

The new compact index stores:

- lexically ordered α and β strings;
- each string's symmetry label;
- β-string lists and ranks within each irreducible representation;
- one cumulative offset per α string.

The exact determinant and its compact address are reconstructed when needed.
The complete DZP symmetry index now builds in about 0.01 seconds with a measured
peak resident set of 5.7 MB for the `inspect` command.

The sigma-vector kernel was also changed in three ways:

- same-spin double one-body-link paths are combined into unique transitions;
- the two equivalent opposite-spin contractions are evaluated together;
- dense inputs are split across ten fixed source ranges, accumulated in
  thread-local output vectors, and reduced in a deterministic order.

The standard 245,025-determinant equilibrium Davidson benchmark fell from
25.74 seconds immediately before the kernel optimization to 2.36 seconds
afterward on the same machine and compiler, a speedup of about 10.9 times.
The final energies differ only in the parallel-reduction rounding at roughly
1 × 10⁻¹³ Hartree.

For DZP, operator construction takes 1.45 seconds and one dense σ-vector
application takes 60.01 seconds on ten threads. The isolated σ benchmark peaks
at about 3.15 GB resident memory. The complete 20-iteration Davidson run peaks
at about 6.67 GB and completes without swap on a 16 GB Apple M4 system.

## Reproduction

Regenerate the safe PySCF input and independent lower-level references:

```bash
uv run --frozen python scripts/oracle/generate.py h2o-dzp-fc
```

Inspect the active-space size:

```bash
cargo run --release -- inspect fixtures/h2o-dzp-fc/FCIDUMP
```

Benchmark one dense σ-vector application:

```bash
cargo run --release -- sigma-benchmark fixtures/h2o-dzp-fc/FCIDUMP
```

Repeat the complete Rust FCI calculation:

```bash
cargo run --release -- davidson fixtures/h2o-dzp-fc/FCIDUMP \
  --residual-tolerance 1e-7 \
  --max-iterations 40 \
  --max-subspace 6
```

Run the fast committed-evidence test:

```bash
cargo test --test extended_dzp
```

Repeat the approximately 18-minute live test:

```bash
cargo test --release --test extended_dzp \
  live_dzp_davidson_reproduces_the_published_digits -- --ignored
```

## Sources and precision boundary

- Kállay and Surján, Journal of Chemical Physics 115, 2945 (2001),
  DOI: https://doi.org/10.1063/1.1383290.
- Bauschlicher and Taylor, Journal of Chemical Physics 85, 2779 (1986),
  DOI: https://doi.org/10.1063/1.451034.
- Open NASA manuscript containing the basis and geometry tables:
  https://ntrs.nasa.gov/api/citations/19860020903/downloads/19860020903.pdf.
- Quantum Harness challenge #129:
  https://github.com/QuantumBFS/quantum.harness/issues/129.

Only the six printed FCI decimals are claimed as a literature match. The
additional Rust digits are convergence evidence for this run, not digits
attributed to the published table.
