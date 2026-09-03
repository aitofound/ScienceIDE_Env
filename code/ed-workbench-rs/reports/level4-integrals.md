# Level 4 Direct-Integral Validation

Date: 2026-07-27
Coordinate input unit: angstrom
Energy unit: hartree

## Result

Level 4 is complete for the two required STO-3G systems. Both production
commands run this pipeline inside the Rust process:

```text
molecule -> libcint AO integrals -> RHF/DIIS -> AO-to-MO -> direct FCI/Davidson
```

Python and PySCF are not runtime dependencies. PySCF 2.14.0 is used only to
generate committed, independently reproducible oracle data.

## Unit and Geometry Contract

| Quantity | External/input unit | Internal representation |
|---|---|---|
| Cartesian geometry | Angstrom | converted to Bohr by PySCF/libcint |
| total, orbital, and nuclear-repulsion energies | Hartree | `f64` Hartree |
| one- and two-electron integrals | Hartree | `f64` Hartree |
| AO overlap | dimensionless | `f64` |
| MO coefficients and CI/CC amplitudes | dimensionless | `f64` |
| bond angles | degree in metadata | Cartesian vectors internally |

The exact committed geometries are:

| System | Geometry |
|---|---|
| equilibrium H2/STO-3G | `R(H-H)=0.7414 Å` |
| stretched H2/STO-3G | atoms at `z=-0.7 Å` and `z=+0.7 Å`; therefore `R(H-H)=1.4 Å` |
| linear H4/STO-3G | atoms at `z=-1.5,-0.5,0.5,1.5 Å`; adjacent spacing `1.0 Å` |
| H2O/STO-3G | `R(O-H)=0.967 Å`, `angle(H-O-H)=107.6°` |
| H2O/6-31G frozen core | the same `0.967 Å`, `107.6°` water geometry |

Reference JSON files carry `coordinate_unit`, `geometry_parameters`, and
`energy_unit`. AO reference files additionally mark overlap as dimensionless.

## Implementation

- `libcint` crate 0.3.2, with the upstream C library built from source and
  linked statically into the final executable;
- overlap, kinetic, nuclear-attraction, and four-index electron-repulsion
  integrals obtained through `CInt::integrate_row_major`;
- PySCF 2.14.0 STO-3G H/O basis values embedded as NWChem basis text, so the
  checked commands do not need an external basis database;
- closed-shell RHF with symmetric orthogonalization, sorted generalized
  eigenpairs, Coulomb/exchange Fock construction, commutator DIIS, and
  energy/density convergence tests;
- a four-stage AO-to-MO transformation in Mulliken ordering;
- the same `ElectronicProblem`, alpha/beta string space, spin-free sigma
  kernel, and Davidson solver used by the FCIDUMP workflows.

The AO arrays are row-major at the libcint/PySCF boundary. The RHF matrices use
`nalgebra`; the integral tensor retains the explicit
`((p*n + q)*n + r)*n + s` index convention. Unit tests compare the staged
four-index transform with an independent eight-index summation.

## PySCF Comparison

The following are maximum absolute element errors. MO orbital signs are aligned
before comparing MO integrals; this changes no observable.

| System | overlap | AO hcore | AO ERI | RHF total E | orbital energies | MO h1 | MO ERI | FCI total E |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| H2/STO-3G | 4.089e-10 | 5.855e-10 | 2.731e-10 | 3.104e-10 | 3.669e-10 | 3.531e-10 | 1.271e-10 | 1.849e-10 |
| H2O/STO-3G | 4.478e-10 | 4.366e-9 | 4.720e-10 | 5.937e-11 | 7.452e-10 | 1.911e-9 | 3.854e-10 | 1.485e-10 |

The small residual differences come from the Angstrom-to-bohr constant used by
the Rust libcint molecule parser. They are far below the Level 1/2 convergence
tolerances.

## End-to-End Energies

| System | Rust RHF | PySCF RHF | Rust direct FCI | PySCF FCI | FCI error |
|---|---:|---:|---:|---:|---:|
| H2/STO-3G | -0.941480654397353 | -0.941480654707799 | -1.015468249103384 | -1.015468249288245 | 1.849e-10 |
| H2O/STO-3G | -74.962663067690499 | -74.962663067631130 | -75.012918738193051 | -75.012918738044460 | 1.485e-10 |

Release-build timings from the validation machine:

| System | integral build | RHF | AO-to-MO | FCI | determinants |
|---|---:|---:|---:|---:|---:|
| H2/STO-3G | 79.3 ms | 0.022 ms | 0.002 ms | 0.024 ms | 4 |
| H2O/STO-3G | 55.3 ms | 0.099 ms | 0.046 ms | 6.17 ms | 441 |

The first clean release build also compiles libcint from source. That one-time
build cost is not included in the execution timings. On macOS, `otool -L`
confirms that the release executable has no external libcint dylib dependency.

## Reproduction

Run the full paths without Python:

```bash
cargo run --release -- rhf h2o-sto3g
cargo run --release -- direct-integrals-fci h2-sto3g
cargo run --release -- direct-integrals-fci h2o-sto3g
```

Run the element-by-element oracle comparisons:

```bash
cargo test --test level4 -- --nocapture
```

Regenerate the independent oracle only when intentionally updating fixtures:

```bash
.venv/bin/python scripts/oracle/generate.py h2-sto3g h2o-sto3g
```

## Scope Boundary

The checked direct-integral CLI exposes H2 and H2O/STO-3G, the two Level 4
acceptance systems. The library molecule type can carry other geometries and
basis names, but only the embedded STO-3G H/O path has a committed
element-by-element oracle in this repository. Larger-basis production work
still enters through the verified FCIDUMP frontend.
