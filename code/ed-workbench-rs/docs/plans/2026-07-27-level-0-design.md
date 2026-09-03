# Level 0 Design

## Scope

Complete the Level 0 dependency ladder from Quantum Harness issue #129:

- generate independent PySCF RHF, FCI, CCSD, and FCIDUMP oracle artifacts;
- parse FCIDUMP in Rust;
- enumerate alpha/beta determinant strings in Rust;
- build a transparent dense molecular electronic Hamiltonian in Rust;
- diagonalize it in Rust for tiny systems;
- compare Rust energies with the committed PySCF references.

Python is confined to `scripts/oracle/`. The Rust executable and tests consume
committed fixtures and do not require Python or PySCF at runtime.

## Architecture

`scripts/oracle/generate.py` defines reproducible H2 and linear H4/STO-3G test
systems, runs PySCF RHF/FCI/CCSD, exports FCIDUMP, and writes provenance-rich
`reference.json` files. The first required Rust path is:

```text
FCIDUMP -> parsed spatial integrals -> spin-orbital determinants
         -> Slater-Condon matrix elements -> symmetric dense matrix
         -> lowest eigenvalue -> reference comparison
```

The determinant implementation uses a `u64` spin-orbital occupation bitset.
This deliberately favors a simple, auditable Level 0 oracle over the optimized
alpha/beta string factorization required by Level 1. Alpha and beta spatial
strings are still exposed and tested, so Level 1 can reuse their ordering.

## Components

- `src/fcidump.rs`: header and integral parsing, Fortran exponent support,
  eightfold spatial-integral lookup, and input validation.
- `src/determinant.rs`: fixed-population bit-string enumeration, alpha/beta
  product basis, excitation degree, and fermionic operator application.
- `src/hamiltonian.rs`: spin-orbital one-/two-body matrix elements and explicit
  dense Hamiltonian construction.
- `src/dense_fci.rs`: symmetric eigendecomposition and lowest eigenvalue.
- `src/main.rs`: `inspect`, `dense-fci`, and `verify` commands.
- `tests/`: parser, determinant, Hamiltonian, CLI, and fixture regression tests.

## Conventions

- FCIDUMP indices are converted from one-based input to zero-based Rust values.
- Spatial two-electron integrals use chemists'/Mulliken notation `(pq|rs)`.
- Spin-orbital ordering is all alpha spatial orbitals followed by all beta
  spatial orbitals.
- The Hamiltonian uses
  `H = sum_pq h_pq a†_p a_q + 1/2 sum_pqrs (pq|rs) a†_p a†_r a_s a_q`.
- Nuclear/core energy is added to every diagonal determinant matrix element.
- The dense matrix is constructed from second-quantized operator application,
  avoiding an independently hand-specialized Slater-Condon branch table.

## Errors and Validation

Malformed headers, invalid orbital indices, inconsistent electron counts,
unsupported systems above 64 spin orbitals, and non-finite integrals return
structured Rust errors. CLI failures exit nonzero with context. Verification
reports the Rust energy, PySCF reference, absolute error, and tolerance.

## Testing and Completion

- synthetic FCIDUMP tests cover record classification, symmetry, `D` exponents,
  and malformed data;
- determinant tests cover combinatorial counts, ordering, rank/unrank
  round-trips, spin counts, and fermionic signs;
- H2/H4 tests establish Hermiticity and dense-vs-reference FCI energies;
- CLI integration tests cover `inspect`, `dense-fci`, and `verify`;
- `cargo test` must pass without a Python installation;
- regenerating fixtures with the pinned oracle environment must reproduce
  reference energies within `1e-10` hartree.

