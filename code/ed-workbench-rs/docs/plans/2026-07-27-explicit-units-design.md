# Explicit Geometry and Energy Units Design

## Goal

Make every geometry and numerical unit in the Python oracle, committed
fixtures, Rust molecule frontend, CLI output, tests, and user documentation
explicit without changing any coordinates, integrals, or energies.

## Chosen Approach

Use explicit metadata rather than comments alone:

- Python `System` carries `coordinate_unit` and machine-readable geometry
  parameters.
- PySCF receives `unit=system.coordinate_unit`.
- Reference JSON records the coordinate unit, energy unit, and named geometry
  parameters while retaining the existing `geometry_angstrom` field for
  compatibility.
- Rust `Molecule` carries a `CoordinateUnit` enum. The libcint TOML input is
  generated from that enum instead of a hard-coded unit string.
- CLI output names both the coordinate and energy units.
- Tests verify the physical distances and angle represented by the committed
  Cartesian coordinates.

## Geometry Metadata

The existing coordinates remain unchanged:

| System | Explicit interpretation |
|---|---|
| H2/STO-3G | Cartesian Å; `R(H-H) = 1.4 Å` |
| linear H4/STO-3G | Cartesian Å; adjacent `R(H-H) = 1.0 Å` |
| H2O/STO-3G | Cartesian Å; `R(O-H) = 0.967 Å`, `angle(H-O-H) = 107.6 degree` |
| frozen-core H2O/6-31G | same H2O geometry and units |

The H2 coordinates `-0.7` and `+0.7` are positions relative to the origin;
their separation is 1.4 Å.

## Unit Contracts

- Input Cartesian coordinates: Angstrom.
- PySCF and libcint internal coordinates: Bohr after parsing.
- Total, orbital, nuclear-repulsion, and one-/two-electron integral energies:
  Hartree.
- AO overlap: dimensionless.
- CI/CC amplitudes and MO coefficients: dimensionless.
- Angles: degree in fixture metadata.

`CoordinateUnit` includes both Angstrom and Bohr so the Rust API cannot silently
mislabel future geometries. The currently committed systems all select
Angstrom.

## Validation

- Python geometry tests construct the PySCF molecule and check H2 distance, H4
  adjacent distances, H2O O-H distances, and the H-O-H angle.
- Rust tests check the unit enum forwarded to libcint and the built-in geometry
  values.
- CLI regression tests require `coordinate unit: angstrom` and
  `energy unit: hartree`.
- Existing element-level AO/MO and energy comparisons must remain unchanged.
- Regenerated FCIDUMP checksums must remain unchanged.

## Documentation

README and the Level 4 report receive a compact unit table and an explicit H2
bond-length warning. The oracle source includes short comments next to each
geometry so the coordinate convention is visible at the definition site.
