# C15 — T05 driver interpolation and epoch reproducibility

## Purpose

C15 validates the time-dependent input path of the AMPS T05/TS05 magnetic-field
model. It tests whether `DRIVER_FILE` values are selected and linearly
interpolated at the requested `EPOCH`, whether the initialized field varies
smoothly in time, and whether identical epochs are reproducible across repeats
and MPI cutoff schedulers.

This test isolates field-time handling from the external observational tests.
C9 and C19 validate physical access against PAMELA and GOES products; C15 checks
that those calculations receive the intended T05 state at each epoch.

## One-line runner command

Run from the AMPS repository root:

```bash
python3 srcEarth/test/C15/run_C15.py --profile ROUTINE --amps ./amps -np 4 -nt 16
```

C15 returns exit code `0` only when all field, access, interpolation,
reproducibility, sensitivity, and continuity gates pass.

## Why C15 uses independent snapshots

Earlier validation-plan sketches proposed keywords such as:

```text
TEMPORAL_MODE TIME_SERIES
FIELD_UPDATE_DT
TS_INPUT_MODE
```

Those sketches do not define a portable test of the current standalone parser.
C15 instead launches a separate, frozen-field Mode3D calculation for every
epoch using the established `FIELD_MODEL T05`, `EPOCH`, and `DRIVER_FILE`
contract already used by C9 and C19. This makes every selected epoch explicit,
independently reproducible, and directly inspectable.

The test does not claim to validate a future multi-epoch orchestration loop. It
validates the production snapshot operation on which such a loop must rely.

## Independent interpolation reference

For every selected epoch, the runner calculates the expected 19-value T05 state
from the bundled five-minute driver:

```text
Bx By Bz Vx Vy Vz Np Temp DST IMFflag SWflag Tilt Pdyn W1 W2 W3 W4 W5 W6
```

`DST` is the required executable-facing column name. Do not replace it with
`SYM-H` or `SYM_H`: the AMPS T05 table parser does not treat those spellings as
aliases. The runner validates the exact header before launching any case and
uses the same canonical header for full, reference, repeat, scheduler, and
perturbed drivers.

It then performs two otherwise identical T05 runs:

1. **Full-driver run:** AMPS receives the complete five-minute driver.
2. **Materialized-reference run:** AMPS receives a three-row driver whose center
   row is exactly at the requested epoch and contains the independently
   calculated state. Identical rows at ±5 minutes preserve normal driver
   coverage and cadence.

At an exact driver timestamp, this validates row selection. At an intermediate
timestamp, it validates linear interpolation. The acceptance predicate depends
on which operation is being tested:

**Exact driver nodes.** The complete initialized-field products are compared
value by value using:

```text
relative tolerance = 5×10⁻¹²
absolute tolerance = 1×10⁻¹⁵ (native field-output units)
```

The corresponding complete access products use `10⁻⁸` relative and `10⁻¹⁰`
absolute numeric tolerances. Exact nodes have no interpolation-order ambiguity,
so this intentionally remains a strict whole-product contract.

**Intermediate epochs.** Python and AMPS can form the same linear driver value
through algebraically different floating-point expressions. Last-bit residuals
are physically negligible but make component-relative errors singular wherever
`Bx`, `By`, or `Bz` crosses zero. C15 therefore compares the complete magnetic
vector field using:

```text
whole-field relative L2 residual             <= 1×10⁻⁷
maximum local |delta B| / RMS(|B|)           <= 1×10⁻⁵
mesh-coordinate mismatches                   = 0
```

For the midpoint access product, C15 parses `access_state`, `allowed`, and
`unresolved` by name and deliberately excludes trace-time, trace-distance, and
other trajectory diagnostics. The default scientific gates are:

```text
resolved state agreement                     >= 95%
one-sided unresolved fraction                <= 5%
absolute allowed-fraction difference         <= 5 percentage points
grid/key mismatches                           = 0
```

This finite midpoint allowance applies only to the independently materialized
interpolation comparison. Trajectories close to a cutoff separatrix can change
classification after a last-bit field perturbation. Exact-node comparisons,
same-epoch repeats, and scheduler comparisons remain strict; C15 therefore does
not use this allowance to hide nondeterminism or a wrong stored-row selection.

The committed `reference_C15_driver_interpolation.csv` documents the five
ROUTINE states independently of AMPS. The runner recomputes and verifies the
table against `data/ts05_driver_C15.txt` before launching anything.

## Routine matrix

The `ROUTINE` profile selects:

| Quantity | Setting |
|---|---|
| T05 epochs | 05:55:00, 05:57:30, 06:00:00, 06:02:30, 06:05:00 UTC on 17 May 2012 |
| Exact/midpoint pairs | Five full-driver plus five materialized-reference runs |
| Repeat anchor | 06:00:00 UTC, two total executions |
| Scheduler comparison | DYNAMIC versus STATIC |
| Dipole controls | First and last selected epochs |
| Driver sensitivity | One perturbed-driver run at 06:00:00 UTC |
| Mode3D cutoff backend | THREADS, 16 workers by default |
| Mover | RK4 |
| Observation shells | 500 km and 9000 km |
| Shell resolution | 30° |
| Rigidities | 0.5, 1, 2, and 5 GV |
| Field mesh | 0.5 Re near Earth, 1.5 Re at boundary |
| Domain | ±8 Re |

The routine profile generates **15 AMPS invocations**. Every invocation archives
its complete deck, command, driver when applicable, initialized-field output,
access product, and log.

## Acceptance gates

### 1. Complete outputs

Every case must exit successfully and produce:

- one or more `amps_3d_initialized*.data.dat*` field files; and
- `cutoff_3d_shells_access.dat` or a compatible suffixed variant.

Every numeric value must be finite. Missing or empty products fail C15.

Mode3D initialized-field output is a finite-element Tecplot product. For a zone
such as:

```text
ZONE N=172587, E=158976, DATAPACKING=POINT, ZONETYPE=FEBRICK
```

C15 reads exactly the `N` point-data rows and then skips the `E` eight-node
FEBRICK connectivity records. Connectivity contains mesh node indices rather
than VARIABLES-width field samples and is never included in field norms. A
declared finite-element zone using non-`POINT` packing, a truncated `N` block,
or a malformed point row fails with a specific parser error.

### 2. Full-driver/reference equivalence

At exact driver nodes, complete field and access products are compared
numerically. At intermediate epochs, Bx/By/Bz are compared using the vector-norm
gates above, while exact grid coverage and the scientific access classifications
are compared with unresolved-aware aggregate limits. This separation tests the
intended driver state without turning non-authoritative trajectory diagnostics
or near-zero component division into the acceptance predicate.

### 3. Exact epoch repeat

The repeated 06:00 UTC calculation must have the same canonical binary64
fingerprints for both field and access output. Decimal text spellings that
represent the same binary64 value are treated as equal.

### 4. Scheduler independence

The DYNAMIC and STATIC calculations at the same epoch must have exactly equal
field and access fingerprints. `THOROUGH` also tests `BLOCK_CYCLIC`.

### 5. Dipole epoch invariance

A centered, aligned DIPOLE is calculated at the first and last selected epoch.
Both field and access products must be exactly equal. This control detects
spurious epoch dependence in mesh setup, coordinates, or output bookkeeping.

### 6. Driver sensitivity

C15 constructs a second driver at 06:00 UTC with materially perturbed IMF By/Bz,
Dst, dynamic pressure, and W1–W6 while retaining the same epoch and fallback
input values. The initialized T05 field must change with a changed-value relative
RMS of at least `10⁻⁴`.

This gate is important: full-driver and reference-driver runs could agree if an
executable ignored both files. The sensitivity case proves that `DRIVER_FILE`
actually controls the T05 field.

### 7. Temporal continuity

For each intermediate epoch, the runner compares the initialized field with the
adjacent exact-node fields. It reports:

- normalized deviation from the time-linear field;
- left and right time-scaled field-change rates; and
- the ratio of those rates.

The default gates are normalized curvature ≤0.25 and scaled step ratio ≤3. The
endpoint field difference must be nonzero. This catches held rows, wrong-time
updates, and discontinuous field changes without assuming that the nonlinear T05
field itself is exactly linear in its driver parameters.

## Reference files

| File | Role |
|---|---|
| `data/ts05_driver_C15.txt` | Five-minute source driver and fixed event window |
| `reference_C15_driver_interpolation.csv` | Exact expected driver states for the ROUTINE epochs |
| `reference_C15_acceptance_contract.csv` | Human-readable scientific gates and defaults |

The runtime result also writes `C15_driver_reference_used.csv`. For a custom
driver or custom epoch list, this generated table is the authoritative reference
used by that run; the checked-in table continues to protect the default package.

## Output products

The default output root is `test_output/C15_t05_time/`:

| File | Content |
|---|---|
| `C15_configuration.json` | Driver checksum and complete matrix-defining settings |
| `C15_commands.json` | Working directory and command for every invocation |
| `C15_driver_reference_used.csv` | Independently selected/interpolated driver values |
| `C15_run_fingerprints.csv` | Per-case field and cutoff fingerprints |
| `C15_driver_equivalence.csv` | Whole-product full/reference residuals |
| `C15_reproducibility.csv` | Repeat, scheduler, and dipole exact comparisons |
| `C15_driver_sensitivity.csv` | Perturbed-driver response magnitude |
| `C15_continuity.csv` | Field curvature and scaled step-rate diagnostics |
| `C15_result.json` | Machine-readable overall result and failure messages |
| `C15_summary.txt` | Concise human-readable status |

`--skip-run` verifies that matrix-defining settings match the archived
`C15_configuration.json` before reanalysis. Analysis tolerances may be changed;
epochs, driver checksum, ranks, threads, schedulers, rigidities, repeats, and mesh
settings may not silently drift.

## Profiles and variants

```bash
# Smaller three-epoch integration check
python3 srcEarth/test/C15/run_C15.py --profile SMOKE --amps ./amps -np 2 -nt 4

# Recommended validation
python3 srcEarth/test/C15/run_C15.py --profile ROUTINE --amps ./amps -np 4 -nt 16

# Nine epochs, finer mesh/shell, three repeats, and two scheduler cross-checks
python3 srcEarth/test/C15/run_C15.py --profile THOROUGH --amps ./amps -np 8 -nt 16

# Reanalyze an existing ROUTINE matrix
python3 srcEarth/test/C15/run_C15.py --profile ROUTINE --skip-run -np 4 -nt 16
```

Use `--help` for custom epochs, driver, rigidities, mesh controls, and tolerances.
A custom epoch list must include the anchor and the exact bracketing driver nodes
needed for every requested intermediate-epoch continuity check.

## Package verification without AMPS

```bash
python3 srcEarth/test/C15/tests/run_self_tests.py
```

This compiles the runner, validates the source driver and committed reference,
checks interpolation and numeric-comparison utilities, renders all 15 ROUTINE
cases, and performs a complete synthetic analysis. It also proves that corrupted
driver equivalence, an ignored driver, and mismatched `--skip-run` settings are
rejected.

This package test does not replace a real T05 field calculation. The one-line
ROUTINE command is the physical end-to-end validation.

## Directory contents

```text
srcEarth/test/C15/
├── README.md
├── MANIFEST.sha256
├── AMPS_PARAM_C15_mode3d.in
├── run_C15.py
├── reference_C15_driver_interpolation.csv
├── reference_C15_acceptance_contract.csv
├── data/
│   └── ts05_driver_C15.txt
└── tests/
    └── run_self_tests.py
```

## Interpreting failures

- **Full/reference mismatch at exact epoch:** driver row selection, parsing, or
  field initialization disagrees with the source row.
- **Midpoint field-vector failure:** the discrepancy is field-scale significant;
  investigate held/nearest-row selection, the interpolation weight, or a field
  update at the wrong time.
- **Field row width does not match VARIABLES:** verify that the runner is schema
  4 or newer. Earlier C15 revisions incorrectly treated FEBRICK connectivity
  records as field rows after the declared `N` point records.
- **Midpoint access-classification failure with a passing field gate:** inspect
  unresolved trajectories and separatrix sensitivity. Do not loosen the exact
  repeat or scheduler gates.
- **Field matches but cutoff differs:** investigate trajectory determinism,
  unresolved states, task ordering, and mover controls.
- **Repeat or scheduler mismatch:** the same frozen field/access calculation is
  not deterministic under the tested execution configuration.
- **Dipole control mismatch:** epoch dependence has entered a supposedly static
  path; inspect coordinate/mesh initialization before diagnosing T05.
- **Sensitivity failure:** `DRIVER_FILE` may be ignored or overridden by static
  fallback parameters.
- **Continuity failure:** inspect `C15_continuity.csv` together with the generated
  driver reference. Do not hide a discontinuity by loosening interpolation
  equivalence or replacing a time/driver state without documenting the policy.
