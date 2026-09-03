# Explicit Geometry and Energy Units Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make coordinate, angle, integral, and energy units explicit and automatically checked without changing any numerical result.

**Architecture:** Python oracle systems carry coordinate-unit and geometry-parameter metadata that is serialized into fixtures. Rust molecules carry a typed coordinate unit that is forwarded to libcint, while CLI and documentation expose the complete unit contract.

**Tech Stack:** Python 3.12, PySCF 2.14.0, Rust 2024, libcint 0.3.2, `unittest`, Cargo integration tests.

## Global Constraints

- Preserve every existing Cartesian coordinate and basis.
- Preserve every FCIDUMP checksum and numerical energy.
- Use Angstrom for all committed input geometries.
- Use Hartree for energies and energy-valued integrals.
- Keep Python/PySCF as an oracle-only dependency.

---

### Task 1: Python Unit Metadata and Geometry Tests

**Files:**
- Modify: `scripts/oracle/generate.py`
- Create: `scripts/oracle/test_units.py`
- Regenerate: `fixtures/*/reference.json`
- Regenerate: `fixtures/h2-sto3g/ao_reference.json`
- Regenerate: `fixtures/h2o-sto3g/ao_reference.json`

**Interfaces:**
- Produces: `System.coordinate_unit: str`
- Produces: `System.geometry_parameters: tuple[tuple[str, float, str], ...]`
- Produces JSON fields `coordinate_unit`, `geometry_parameters`, and `overlap_unit`

- [x] **Step 1: Write failing geometry/unit tests**

Create `scripts/oracle/test_units.py` using `unittest`. Construct each PySCF
molecule with `unit=system.coordinate_unit`; assert H2 distance `1.4`, each H4
adjacent distance `1.0`, both water O-H distances `0.967`, water angle `107.6`,
and `coordinate_unit == "Angstrom"`.

- [x] **Step 2: Run the tests and verify failure**

Run:

```bash
.venv/bin/python -m unittest scripts.oracle.test_units -v
```

Expected: failure because `System.coordinate_unit` is not defined.

- [x] **Step 3: Add explicit Python metadata**

Extend `System` with:

```python
coordinate_unit: str = "Angstrom"
geometry_parameters: tuple[tuple[str, float, str], ...] = ()
```

Populate the exact parameters from the approved design, pass
`unit=system.coordinate_unit` to `gto.M`, and serialize parameters as:

```python
[
    {"name": name, "value": value, "unit": unit}
    for name, value, unit in system.geometry_parameters
]
```

Add `coordinate_unit` and `energy_unit` to AO references and
`overlap_unit="dimensionless"`.

- [x] **Step 4: Regenerate and validate fixtures**

Run:

```bash
.venv/bin/python scripts/oracle/generate.py
.venv/bin/python -m unittest scripts.oracle.test_units -v
```

Expected: all geometry tests pass; FCIDUMP files have no diff.

### Task 2: Typed Rust Coordinate Units

**Files:**
- Modify: `src/molecule.rs`
- Modify: `src/libcint_frontend.rs`
- Modify: `src/ao2mo.rs`
- Test: unit tests in `src/molecule.rs`
- Test: `tests/level4.rs`

**Interfaces:**
- Produces: `CoordinateUnit::{Angstrom, Bohr}`
- Produces: `CoordinateUnit::libcint_name(self) -> &'static str`
- Produces: `Molecule.coordinate_unit: CoordinateUnit`
- Produces: `AoIntegrals.coordinate_unit: CoordinateUnit`
- Produces: `libcint_frontend::ENERGY_UNIT: &str`

- [x] **Step 1: Add failing Rust assertions**

Update molecule and Level 4 tests to require Angstrom on built-ins and returned
AO integrals. Add assertions that `Angstrom` formats as `angstrom`, `Bohr`
formats as `bohr`, and both produce the same names for libcint.

- [x] **Step 2: Run targeted tests and verify failure**

Run:

```bash
cargo test molecule::tests --lib
cargo test --test level4
```

Expected: compile failure because `CoordinateUnit` and unit fields do not
exist.

- [x] **Step 3: Implement the typed unit contract**

Define the enum and `Display` implementation in `molecule.rs`, add the unit to
`Molecule::new`, and select Angstrom in all built-ins. In
`compute_ao_integrals`, replace the hard-coded TOML unit with
`molecule.coordinate_unit.libcint_name()`, copy the unit into `AoIntegrals`,
and declare:

```rust
pub const ENERGY_UNIT: &str = "hartree";
```

Update synthetic `AoIntegrals` construction in AO-to-MO tests.

- [x] **Step 4: Run targeted tests**

Run:

```bash
cargo test molecule::tests --lib
cargo test --test level4 -- --nocapture
```

Expected: all targeted tests pass with unchanged numerical errors.

### Task 3: CLI and Documentation

**Files:**
- Modify: `src/main.rs`
- Modify: `tests/level4.rs`
- Modify: `README.md`
- Modify: `reports/level4-integrals.md`
- Modify: `docs/sync-log.md`

**Interfaces:**
- CLI output includes `coordinate unit: angstrom`
- CLI output includes `energy unit: hartree`

- [x] **Step 1: Strengthen the CLI regression test**

Require both unit lines in `direct_integrals_cli_runs_without_python`.

- [x] **Step 2: Run the CLI test and verify failure**

Run:

```bash
cargo test --test level4 direct_integrals_cli_runs_without_python
```

Expected: assertion failure because the unit lines are not printed.

- [x] **Step 3: Expose units and document the contract**

Print both unit lines from the RHF and direct-integral FCI commands. Add a unit
table and the explicit H2 `R(H-H)=1.4 Å` note to README and the Level 4 report.
Record the metadata change in the sync log.

- [x] **Step 4: Run CLI and documentation checks**

Run:

```bash
cargo test --test level4 direct_integrals_cli_runs_without_python
target/release/ed_workbench_rs direct-integrals-fci h2-sto3g
git diff --check
```

Expected: test passes and CLI prints both units.

### Task 4: Full Numerical Regression and Delivery

**Files:**
- Modify: `docs/superpowers/plans/2026-07-27-explicit-units.md`

- [x] **Step 1: Run quality gates**

Run:

```bash
cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test
```

Expected: zero warnings and all tests pass.

- [x] **Step 2: Confirm numerical and checksum stability**

Run:

```bash
target/release/ed_workbench_rs verify fixtures/h2-sto3g/FCIDUMP fixtures/h2-sto3g/reference.json
target/release/ed_workbench_rs direct-integrals-fci h2o-sto3g
git diff -- fixtures/h2-sto3g/FCIDUMP fixtures/h4-sto3g/FCIDUMP fixtures/h2o-sto3g/FCIDUMP fixtures/h2o-631g-fc/FCIDUMP
```

Expected: verification passes, H2O direct FCI remains
`-75.012918738193051` Hartree, and FCIDUMP diff is empty.

- [x] **Step 3: Mark the plan complete, commit, and push**

Stage only the explicit-unit changes, commit them, push `main`, and verify that
the local and remote commit IDs match.
