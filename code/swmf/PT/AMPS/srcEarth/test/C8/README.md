# C8 — Realistic-field directional access and East–West validation

> **Errata (2026-09-02):** a ROUTINE run of this test failed with
> `T96/T05 has no informative rigidity with D(+)>=0.01 and D(-)<=-0.01`.
> Two independent defects were found and fixed: an incorrect
> `CUTOFF_BACKTRACE_CHARGE` convention that inverted the sign of the
> East–West result, and an incorrect assumption that AMPS omits the
> directional grid's polar rows, which silently failed the output-
> completeness gate. Neither fix relaxed an acceptance threshold; one gate
> (C8-G11) was added. See **[Section 12, "Errata and fix
> history"](#12-errata-and-fix-history-read-before-changing-acceptance-thresholds)**
> for the full derivation, and the top of `run_C8.py` for the equivalent
> in-code notes.

## 1. Purpose

C8 validates the direction-resolved geomagnetic-access calculation in realistic
T96 and T05 external magnetic fields. It occupies the gap between an analytical
or symmetry-only test and a detector-specific observational validation:

- C17 checks an exact charge/reflection identity in an aligned dipole.
- **C8 checks realistic-field directional access, charge-dependent East–West
  behavior, external-model agreement, and high-rigidity transparency.**
- C19 folds directional access through the GOES EPEAD response and compares it
  with an observed solar-particle event.

C8 is based on the AMS-oriented magnetospheric backtracing analysis of Boschini
et al. (2013), but it does not pretend to reproduce that paper's complete ISS
ephemeris or its millions of generated trajectories. The bundled published
values are traceable context and, when AMPS exports asymptotic exit directions,
a conditional high-rigidity benchmark. The ordinary C8 pass/fail decision uses
quantities present in the current `DIRECT_ACCESS` product.

Primary scientific source:

- M. J. Boschini et al., *Geomagnetic backtracing: a comparison of Tsyganenko
  1996 and 2005 external field models*, 2013,
  <https://arxiv.org/abs/1307.5192>.

## 2. What the test calculates

The runner launches four otherwise identical AMPS calculations:

| Case | External field | Mass | Charge |
|---|---|---:|---:|
| `t96_charge_plus` | T96 | 1.007276466621 amu | +1 |
| `t96_charge_minus` | T96 | 1.007276466621 amu | -1 |
| `t05_charge_plus` | T05 | 1.007276466621 amu | +1 |
| `t05_charge_minus` | T05 | 1.007276466621 amu | -1 |

Using equal masses is important. It changes only the Lorentz-force charge sign
and avoids mixing an East–West effect with species-dependent conversion between
kinetic energy and rigidity. The input selects
`CUTOFF_BACKTRACE_CHARGE REVERSED`, which is the standard cosmic-ray
"antiparticle" backtracing construction: AMPS traces a trajectory forward in
time from the observation point with velocity reversed from the requested
arrival direction, and time-reversing the Lorentz force `q(v x B)` under that
velocity reversal additionally requires negating the charge used in the
integration. `REVERSED` performs that negation; `SAME` does not, and is kept
in AMPS only as a legacy mode for reproducing old archived results. Using
`SAME` here (the pre-2026-09-02 setting) made each run report the East–West
physics of the *opposite* real charge from the one it was labeled with — see
[Section 12](#12-errata-and-fix-history-read-before-changing-acceptance-thresholds)
for the full derivation and how it was diagnosed.

All four cases use:

```text
epoch                         2012-05-17T06:00:00
driver                        data/ts05_driver_C8.txt
field evaluation              GRIDLESS
cutoff algorithm              DIRECT_ACCESS, fixed common grid
trace policy                  ACCURATE
trace-limit policy            UNRESOLVED
electric field                disabled
default mover                 BORIS
observation altitude          400 km
observation GSM longitudes    0, 180 deg
observation GSM latitudes     -51.6, 0, +51.6 deg
rigidities                    1, 2, 5, 10, 20, 50, 100 GV
detector-like FOV             45 deg about local zenith
```

The six fixed GSM points sample two opposing local-time sectors and the central
and limiting latitudes of an ISS-like 51.6-degree inclination. They are a
controlled, reproducible representative sample—not an ISS orbit reconstruction.
An exact orbit study would require ephemeris and attitude records and belongs in
a separate observational extension.

### Direction and detector-look convention

The longitude and latitude in an AMPS directional-access file describe particle
arrival velocity. The corresponding detector look vector is its negative. At
each observation point, C8 constructs:

```text
local zenith = outward radial unit vector
local east   = (-sin(lon), cos(lon), 0)
detector look = -arrival direction
```

Directions within 45 degrees of local zenith enter the virtual aperture. The
horizontal look projection assigns `EAST` or `WEST`; directions close to the
north/south meridian enter a `CENTER` deadband and are retained in the total FOV
but excluded from the East–West lobes.

Each regular longitude/latitude cell is weighted by its exact solid angle,

```text
dOmega = dlon * [sin(lat_upper) - sin(lat_lower)],
```

AMPS's directional grid always includes both poles: it writes one row per
longitude value at latitude -90 and +90, even though every one of those rows
traces the same physical direction (`cos(+/-90 deg)` makes the longitude
value immaterial to the resulting Cartesian direction). C8 treats the regular
(non-polar) latitude bands as plain, unstretched cells and folds each pole
into a reduction separately as a single canonical direction carrying the
exact polar-cap solid angle `2*pi*(1 - cos(half_angle))` with
`half_angle = DIRMAP_LAT_RES/2`. Before doing so, C8 verifies that every
longitude-tagged duplicate AMPS wrote for that pole and rigidity reports the
*same* three-state access classification (gate C8-G11, Section 5.1); a
disagreement is a genuine trace non-determinism defect. The self-test
verifies that the regular cells plus the two exact polar-cap weights sum to
`4*pi` steradians.

This matters beyond bookkeeping: for the two observation points at
`|latitude| = 51.6` degrees, one pole is only `90 - 51.6 = 38.4` degrees from
local zenith — inside the 45-degree aperture — so it is not a negligible
edge case in the FOV/East–West reduction.

## 3. Executable reference solution

C8 deliberately uses relations and bounds rather than a frozen AMPS output file.
A frozen output generated by the same code would be a regression baseline, not
an independent scientific reference.

`reference_C8_expected_physics.csv` is the executable reference solution:

| ID | Expected physical result | Default gate |
|---|---|---|
| C8-R01 | 50 and 100 GV particles are transparent in the zenith FOV | allowed fraction ≥ 0.98 and unresolved ≤ 0.02 |
| C8-R02 | Positive charges have greater access from physical west at transition rigidities | `T_WEST - T_EAST ≥ 0.01` |
| C8-R03 | Negative charges reverse the East–West sign | `T_WEST - T_EAST ≤ -0.01` |
| C8-R04 | T96 and T05 predominantly agree on resolved access states | aggregate mismatch ≤ 0.15 |
| C8-R05 | T96/T05 high-rigidity asymptotic differences are milliradian scale | each RMS ≤ 5 mrad when exit columns are required |
| C8-R06 | Every access state agrees with its termination reason | zero mismatches |

These results are independently interpretable and fail in useful ways: a charge
sign or arrival/look convention error changes C8-R02/R03; a broken outer-boundary
classification changes C8-R01; a field/driver/model error changes C8-R04; and a
trajectory-classification error changes C8-R06. (C8-R02/R03 are precisely the
gates that caught the `CUTOFF_BACKTRACE_CHARGE` defect described in
[Section 12](#12-errata-and-fix-history-read-before-changing-acceptance-thresholds) —
this is that mechanism working as designed, not a false alarm.)

`reference_C8_acceptance_contract.csv` is the machine-readable gate inventory.
The runner validates both reference schemas before an AMPS calculation and copies
them into the result directory. It never rewrites checked-in references.

## 4. Published T96/T05 reference values

`reference_C8_boschini2013.csv` records the primary-source values used to
interpret the model comparison:

| Rigidity / sample | Pressure subset | Latitude RMS | Longitude RMS |
|---|---|---:|---:|
| 20–30 GV | all | 3.5 mrad | 5.0 mrad |
| 20–30 GV | `Pdyn < 4 nPa` | 3.3 mrad | 4.5 mrad |
| 20–30 GV | `Pdyn > 4 nPa` | 6.7 mrad | 8.8 mrad |
| >50 GV | all | 1.5 mrad | 2.3 mrad |
| >50 GV | `Pdyn < 4 nPa` | 1.4 mrad | 2.2 mrad |
| >50 GV | `Pdyn > 4 nPa` | 2.6 mrad | 3.7 mrad |

The paper also reports a 9.5% T96/T05 trajectory-classification difference in
its 0.3–200 GV sample. C8's representative point/grid sample is different, so
9.5% is context rather than an exact target; C8 uses a conservative 15% hard
bound. The paper's IGRF/T05 and IGRF/T96 classification differences (19.8% and
21.5%) are retained in the reference CSV for provenance, but IGRF is outside the
implemented C8 model pair.

## 5. Acceptance logic

### 5.1 Output completeness and uniqueness — hard

For each of four cases and six points, C8 independently derives the complete
requested direction grid and rigidity list. It requires:

- exactly one row per direction and rigidity;
- no missing or unexpected directions;
- no missing or unexpected rigidity nodes;
- finite coordinates and trace diagnostics;
- recognized three-state access and termination values.

At the ROUTINE 15-degree resolution, the true DIRECT_ACCESS output contract
(gate C8-G01) is:

```text
264 regular (non-polar) directions, one row per longitude/latitude cell
 48 polar directions, one row per longitude value at each of lat=-90/+90
312 directions per point in total
  7 rigidities per direction
2184 trajectory samples per point/case
52416 trajectory samples over 4 cases and 6 points
```

AMPS's directional grid always includes both poles (`nLatMap = round(180 /
DIRMAP_LAT_RES) + 1`); it does **not** omit the longitude-degenerate ±90-degree
rows. C8 requires every one of them — the completeness gate is intentionally
just as strict as before, only now checked against the grid AMPS actually
produces rather than an incorrect 264-only assumption. See
[Section 12](#12-errata-and-fix-history-read-before-changing-acceptance-thresholds)
for how the previous, narrower assumption silently failed every ROUTINE run.

**Gate C8-G11 — pole azimuthal degeneracy — hard.** The 24 (at ROUTINE
resolution) longitude-tagged duplicate rows AMPS writes at a given pole all
trace the same physical direction, so they must report the *same* three-state
access classification for a given rigidity. C8 checks this explicitly for
every pole/rigidity/case/point and fails if any duplicate disagrees; a
disagreement indicates trace non-determinism or a numerical defect that a
narrower, pole-excluding audit could never observe. When all duplicates
agree, C8 publishes one canonical (longitude 0) sample per pole for use by
the FOV, East–West, model-comparison, and asymptotic-direction reductions
(Sections 5.4–5.7), each carrying the pole's exact solid-angle weight instead
of a value borrowed from the neighboring regular band.

### 5.2 State/termination closure — hard

The valid producer contract is:

| Termination code | Meaning | Required `access_state` |
|---:|---|---:|
| 0 | outer boundary / allowed | 1 (`ALLOWED`) |
| 1 | inner boundary / forbidden | 0 (`FORBIDDEN`) |
| 2 | magnetically trapped / forbidden | 0 (`FORBIDDEN`) |
| 3 | time limit | 2 (`UNRESOLVED`) |
| 4 | step limit | 2 (`UNRESOLVED`) |
| 5 | distance limit | 2 (`UNRESOLVED`) |
| 6 | invalid time step | 2 (`UNRESOLVED`), but fatal to C8 |
| 7 | invalid field | 2 (`UNRESOLVED`), but fatal to C8 |
| 8 | numerical failure | 2 (`UNRESOLVED`), but fatal to C8 |
| 9 | drift-trapped / forbidden | 0 (`FORBIDDEN`) |

Any mismatch fails. Codes 6–8 fail even when the state is correctly unresolved.
C16 is the exhaustive resource-limit test; C8 enforces the same production
contract on its realistic-field science cases.

### 5.3 Unresolved population — hard

The full requested cube in each case must have unresolved fraction ≤0.25 (as
of the pole-completeness fix, "full requested cube" correctly includes the
polar duplicate rows counted in Section 5.1's `2184` samples/point/case, not
just the `1848`-sample regular grid). This prevents T96/T05 or East–West
comparisons from passing after discarding most difficult trajectories.
Individual EAST/WEST transition lobes must have unresolved fraction ≤0.20 to
enter the East–West gate.

The test does not relabel a timeout as forbidden. If this gate fails, increase
the physical trace budget with `--max-trace-time` or use the THOROUGH profile;
do not weaken state semantics.

### 5.4 High-rigidity transparency — hard

For every model, charge, point, and rigidity ≥50 GV:

```text
FOV allowed solid-angle fraction    >= 0.98
FOV unresolved solid-angle fraction <= 0.02
```

The per-point requirement prevents a global average from hiding one broken
latitude or local-time case.

### 5.5 T96/T05 access agreement — hard

C8 compares identical FOV samples after excluding pairs in which either model
is unresolved. The aggregate resolved state-mismatch fraction must be ≤0.15 for
each charge. Missing common samples always fail. Per-rigidity/per-point counts
remain in `C8_model_comparison.csv` so a marginal aggregate result can be
localized.

### 5.6 Charge-dependent East–West effect — hard

For each field model and rigidity, C8 averages the six point-level aperture
transmissions and defines

```text
D(q) = T_WEST(q) - T_EAST(q).
```

Only nodes with total FOV transmission between 0.05 and 0.95 for both charges
and acceptable lobe resolution are informative. Each field model must contain at
least one informative node satisfying:

```text
D(+1) >= +0.01
D(-1) <= -0.01
```

This is not an exact cell-reflection identity. T96 and T05 include asymmetric
external-current/solar-wind structure, so forcing equality under a geometric
reflection would test a symmetry those models do not possess. C17 remains the
correct test for an exact dipole charge/reflection identity.

This is the gate that first exposed the `CUTOFF_BACKTRACE_CHARGE` defect: with
`SAME` selected, `D(+1)` and `D(-1)` were both the correct magnitude but the
*opposite* sign from the expectation above, because `SAME` made each case
report the East–West physics of the real particle with the opposite charge
(see [Section 12](#12-errata-and-fix-history-read-before-changing-acceptance-thresholds)).
The thresholds `+0.01`/`-0.01` above are unchanged by that fix — only the
upstream `CUTOFF_BACKTRACE_CHARGE` input selection was.

### 5.7 Asymptotic-direction comparison — conditional

The baseline DIRECT_ACCESS schema contains access state and termination
diagnostics but not the asymptotic exit direction. C8 recognizes these explicit
future schemas:

```text
asymptotic_lon_deg, asymptotic_lat_deg
exit_lon_deg, exit_lat_deg
asymptotic_x, asymptotic_y, asymptotic_z
exit_direction_x, exit_direction_y, exit_direction_z
```

With the default `--asymptotic-policy DIAGNOSTIC`, absent columns are reported as
`NOT_AVAILABLE` and do not weaken or fail the access-state validation. If columns
are present, C8 writes T96/T05 differences and RMS summaries.

Use:

```bash
--asymptotic-policy REQUIRE --max-asymptotic-rms-mrad 5
```

after an AMPS producer implements one complete schema. `REQUIRE` fails when the
columns are absent and hard-gates both high-rigidity component RMS values at
5 mrad. This threshold covers the source's 2.3-mrad all-sample longitude RMS and
3.7-mrad high-pressure longitude RMS with finite-grid margin.

## 6. Files

```text
srcEarth/test/C8/
├── README.md
├── AMPS_PARAM_C8_gridless.in
├── run_C8.py
├── reference_C8_expected_physics.csv
├── reference_C8_acceptance_contract.csv
├── reference_C8_boschini2013.csv
└── data/
    └── ts05_driver_C8.txt
```

The driver header intentionally contains `DST`, which the T05 parser requires.
Do not replace it with a table containing only `SYM-H` unless the parser is
explicitly extended to map that column.

## 7. Runner commands

Run the recommended validation from the AMPS repository root:

```bash
python3 srcEarth/test/C8/run_C8.py --profile ROUTINE --amps ./amps -np 4 -nt 16
```

Faster installation/parser check:

```bash
python3 srcEarth/test/C8/run_C8.py --profile SMOKE --amps ./amps -np 2 -nt 4
```

Higher-resolution validation:

```bash
python3 srcEarth/test/C8/run_C8.py --profile THOROUGH --amps ./amps -np 8 -nt 16
```

Render the four input decks and print commands without running AMPS:

```bash
python3 srcEarth/test/C8/run_C8.py --amps ./amps --dry-run
```

Audit already generated output:

```bash
python3 srcEarth/test/C8/run_C8.py --skip-run \
  --workdir test_output/C8_directional_access
```

Runner/reference unit checks, which do not need AMPS:

```bash
python3 srcEarth/test/C8/run_C8.py --self-test
python3 srcEarth/test/C8/run_C8.py --validate-references
```

MPI and threads are safe here: each trajectory is independent, and AMPS's
GRIDLESS scheduler distributes the full direction/rigidity task list. C8 is a
science validation, not a serial determinism test. The same `-np`/`-nt` values
are used for all four cases.

## 8. Profiles and cost

| Profile | Sky resolution | Regular directions/point | Polar directions/point | Total directions/point | Trace budget | Max steps | Purpose |
|---|---:|---:|---:|---:|---:|---:|---|
| SMOKE | 30° | 60 | 24 | 84 | 300 s | 1,000,000 | installation and parser check |
| ROUTINE | 15° | 264 | 48 | 312 | 600 s | 2,000,000 | standard validation |
| THOROUGH | 10° | 612 | 72 | 684 | 1200 s | 4,000,000 | publication/high-resolution follow-up |

"Polar directions/point" is `2 * (360 / DIRMAP_LON_RES)` — one duplicate row
per longitude value at each of the two poles (Section 5.1, gate C8-G01/G11).
This column was `0` before the 2026-09-02 fix, which undercounted the true
AMPS output at every resolution; see
[Section 12](#12-errata-and-fix-history-read-before-changing-acceptance-thresholds).

The scientific thresholds do not relax in cheaper profiles. Consequently,
SMOKE may expose angular-resolution insufficiency; it is not a substitute for a
ROUTINE validation result.

## 9. Output products

The default result directory is `test_output/C8_directional_access`.

| File | Contents |
|---|---|
| `C8_summary.txt` | concise human-readable result and failure messages |
| `C8_result.json` | full machine-readable configuration, runs, cases, and verdict |
| `C8_coverage_summary.csv` | per-case/per-point grid, state/reason, and pole-degeneracy audit |
| `C8_directional_access_summary.csv` | FOV/EAST/WEST/CENTER weighted access fractions |
| `C8_model_comparison.csv` | T96/T05 resolved-pair mismatch counts |
| `C8_east_west_comparison.csv` | charge-odd East–West statistics by model/rigidity |
| `C8_asymptotic_comparison.csv` | optional per-sample exit-direction differences |
| `reference_C8_*.csv` | exact source/reference contracts used by the run |
| `*/C8_AMPS.log` | command and combined stdout/stderr for each AMPS case |

Each case directory also retains its rendered input, copied driver, and six raw
`cutoff_gridless_dir_access_point_####.dat` files.

`C8_coverage_summary.csv` gained a `pole_degeneracy_errors` column (gate
C8-G11) and its `expected_directions`/`actual_directions`/`coverage_errors`
columns now reflect the full 312-direction (ROUTINE) contract instead of the
264-direction regular-only grid; see Section 5.1.
`C8_result.json` gained `requested_pole_direction_tasks_per_point` and
`requested_full_direction_tasks_per_point` alongside the unchanged
`requested_direction_tasks_per_point` (still the regular-grid count).
`reference_C8_acceptance_contract.csv` now lists 11 gates (`C8-G01`..`C8-G11`)
instead of 10.

## 10. Diagnosing failures

### Parser rejects the input

Confirm the input contains `CUTOFF_SAMPLING VERTICAL`. The common parser requires
it for `DIRECT_ACCESS` and explicit rigidity lists. C8 intentionally avoids
`CUTOFF_UNRESOLVED_EXTENSION_PASSES` and related optional C19 keywords because
baseline AMPS parsers may reject them.

### T05 says `DST` is missing

Use the bundled `data/ts05_driver_C8.txt`. Its header contains the required
`DST` column plus IMF, solar-wind, pressure, tilt, and W1–W6 values.

### Many unresolved samples

Inspect termination codes in the raw files and `C8_coverage_summary.csv`.
Increase `--max-trace-time`, or run THOROUGH. Codes 3–5 must remain unresolved;
do not reinterpret them as forbidden. Codes 6–8 indicate numerical defects.

### East–West gate has no informative node

Check `C8_east_west_comparison.csv` and the point-level directional summary.
Possible causes are:

- all requested rigidities are fully blocked or fully transparent;
- EAST/WEST lobes have too many unresolved trajectories;
- arrival direction and detector look were interchanged;
- the explicit particle charge was reversed again inside the cutoff calculation;
- angular resolution is too coarse;
- **`CUTOFF_BACKTRACE_CHARGE` is `SAME` instead of `REVERSED`** — this
  produces a fully sign-inverted `D(+1)`/`D(-1)` pattern (both charges show
  the *magnitude* of a real East–West effect but the *opposite* sign from
  the expectation), which is the specific defect documented in
  [Section 12](#12-errata-and-fix-history-read-before-changing-acceptance-thresholds).
  Diagnostic: if `D(+1) <= -0.01` and `D(-1) >= +0.01` simultaneously (i.e.
  R02/R03 fail in exactly the mirrored direction), suspect this first, before
  the direction-convention or charge-reversal causes above.

Retain 5 and 10 GV in custom grids; those are the most likely transition nodes
near low-Earth-orbit equatorial positions.

### Coverage gate (C8-G01) or pole-degeneracy gate (C8-G11) fails

Check the `coverage_errors` and `pole_degeneracy_errors` columns in
`C8_coverage_summary.csv`, and the corresponding messages in
`C8_summary.txt` (every nonzero count now has an explanatory message; a
silent coverage failure was itself part of the 2026-09-02 defect, see
[Section 12](#12-errata-and-fix-history-read-before-changing-acceptance-thresholds)).

- `extra_directions`/`missing_directions` nonzero for every point at a fixed,
  resolution-independent offset (`2 * 360/DIRMAP_LON_RES`) usually means the
  runner's full-grid contract (`expected_full_direction_keys`) and the
  installed AMPS producer disagree about polar-row behavior — confirm
  `nLatMap = round(180/DIRMAP_LAT_RES) + 1` in `CutoffRigidityGridless.cpp`
  still includes poles the way this runner expects.
- `pole_degeneracy_errors` nonzero means AMPS reported *different* access
  states for two or more longitude-tagged rows at the same pole and rigidity,
  which should be numerically impossible (`cos(+/-90 deg)` makes the traced
  direction independent of longitude). Suspect a nondeterministic mover, an
  uninitialized field-evaluator state that depends on task-processing order,
  or an MPI reduction race in the DIRECT_ACCESS path.

### High-rigidity transparency fails

First distinguish `FORBIDDEN` from `UNRESOLVED`. A forbidden high-rigidity
population often indicates boundary or direction initialization errors; an
unresolved population points to the trace budget, integrator, or field validity.

### T96/T05 mismatch is high

Verify that both cases use the same epoch and copied driver. Then inspect whether
mismatches cluster at cutoff transition nodes or affect high-rigidity samples.
Broad high-rigidity disagreement is a stronger indication of a field/model
integration problem than a narrow penumbral disagreement.

## 11. Limitations and extension path

- The six fixed GSM points are representative, not an ISS ephemeris.
- The virtual aperture is circular and does not model the AMS acceptance matrix.
- The current hard comparison is access classification, not an exact published
  list of asymptotic coordinates.
- External-field model disagreement near a penumbra is physical and is therefore
  bounded statistically rather than required to vanish.

The most valuable future extension is to export an unambiguous asymptotic exit
direction from the DIRECT_ACCESS producer. After that change, run C8 with
`--asymptotic-policy REQUIRE`; no runner redesign is necessary. A second,
separate extension can replace the representative points with time-tagged ISS
ephemeris/attitude data and a full detector response.

## 12. Errata and fix history (read before changing acceptance thresholds)

This section documents two independent defects found by auditing a ROUTINE
C8 run that failed with `T96/T05 has no informative rigidity with
D(+)>=0.01 and D(-)<=-0.01`, and how each was fixed. **Neither fix relaxed an
acceptance threshold or a hard gate's policy.** Both fixes correct the test's
own configuration/bookkeeping so the existing, unchanged thresholds are
evaluated against physically and structurally correct inputs; one gate
(C8-G11) was added as strictly additional rigor. The equivalent notes live at
the top of `run_C8.py` next to the code they describe.

### FIX 1 — backtrace-charge convention (`AMPS_PARAM_C8_gridless.in`)

**Symptom.** `C8_east_west_comparison.csv` showed, at the informative 10 GV
transition node, `west_minus_east_q_plus ≈ -0.06` and
`west_minus_east_q_minus ≈ +0.09` for both T96 and T05 — the correct
*magnitude* for a real East–West effect, but the sign expected of the
*opposite* charge in each case.

**Cause.** AMPS backtraces a particle by integrating forward in time from the
observation point with initial velocity reversed relative to the requested
arrival direction (`v0 = -arrival`, the "d_unit (reversed from arrival)"
convention in `CutoffRigidityGridless.cpp`). Time-reversing the Lorentz force
`q(v x B)` under a velocity reversal additionally requires negating the
charge used in the numerical integration — the standard "antiparticle"
construction used by every cosmic-ray cutoff code (Störmer; Smart & Shea;
Boschini et al. 2013). `CUTOFF_BACKTRACE_CHARGE REVERSED` performs that
negation; `SAME` does not, and AMPS documents `SAME` in-source as a legacy
mode kept only to reproduce old archived results:

```text
// For a physically correct reverse trajectory, time reversal of q v x B
// requires reversing the charge sign as well as reversing the velocity.
// Historical AMPS cutoff calculations reversed only the velocity, so the
// input-selectable convention preserves those archived results while
// allowing reference-table validation to use the conventional antiparticle
// construction.
```

C8 previously selected `SAME` with a comment claiming `REVERSED` would "hide"
the charge sign under test. That reasoning was backwards: under `SAME`, the
run labeled `charge_plus` numerically reproduces the physics of a real
*negative* particle, and `charge_minus` reproduces a real *positive*
particle — exactly the mirrored sign pattern observed. Every other
cutoff-physics test in this suite (C6, C7, C9, C10, C13, C15, C16, C18, C19)
already used `REVERSED`; C8 (and C17, whose reflection identity is
insensitive to absolute sign — see its own README) were the exceptions.

**Fix.** `AMPS_PARAM_C8_gridless.in` now sets
`CUTOFF_BACKTRACE_CHARGE REVERSED`. `run_C8.py --self-test` independently
verifies (a) the rendered input never contains `CUTOFF_BACKTRACE_CHARGE
SAME`, and (b) gate C8-G07's arithmetic (`east_west_comparison`) accepts a
physically correct sign pattern and still rejects the exact mirrored pattern
above — proving the acceptance logic itself was never the defect.

**What did not change.** The East–West thresholds (`D(+1) >= +0.01`,
`D(-1) <= -0.01`, transition-window `[0.05, 0.95]`, lobe-unresolved bound
`0.20`) are identical to before this fix.

### FIX 2 — directional-grid completeness contract (`run_C8.py`)

**Symptom.** Independent of FIX 1, every row of `C8_coverage_summary.csv`
showed `extra_directions: 48` (at ROUTINE resolution) and `passed: 0`, yet
`C8_summary.txt` never mentioned it — `overall_passed` was set to `False`
through a counter that fed no explanatory message.

**Cause.** `expected_direction_keys()` assumed AMPS's DIRECT_ACCESS producer
omits the longitude-degenerate ±90-degree rows. It does not: AMPS's
directional grid unconditionally includes both poles, one row per longitude
value (`nLatMap = round(180/DIRMAP_LAT_RES) + 1  // include poles` in
`CutoffRigidityGridless.cpp`), so a ROUTINE run legitimately emits 24 extra
rows at each pole (48 total) beyond the 264-direction non-polar grid this
runner expected. Because Section 5.1 (gate C8-G01) is a hard "zero coverage
errors" gate, those 48 genuine AMPS rows were flagged as "unexpected
directions," and the resulting `coverage_errors > 0` failed the run — but
`overall_passed` was flipped by a bare counter with no accompanying message,
so the failure was invisible in `C8_summary.txt`.

**Fix**, in three parts, all of which **tighten** rather than relax C8:

1. `expected_full_direction_keys()` (regular grid ∪ `expected_pole_direction_keys()`)
   now defines the true output contract used by the Section 5.1 audit, so
   genuine AMPS rows are no longer misclassified as errors. The "zero
   coverage errors" bar itself is unchanged; only the definition of what
   counts as "expected" was corrected. `run_C8.py --self-test` includes a
   case where the pole rows are deliberately omitted from a synthetic cube
   and confirms the (corrected) gate still rejects it.
2. New hard gate **C8-G11** ("pole azimuthal degeneracy") verifies that the
   longitude-tagged duplicates AMPS writes at each pole report an *identical*
   access state per rigidity, since they all trace the same physical
   direction. A disagreement is a genuine reproducibility defect that the
   pre-fix runner could never detect, because it discarded pole rows before
   auditing them. `run_C8.py --self-test` includes a case with one
   deliberately corrupted duplicate and confirms exactly one violation is
   detected and reported.
3. The FOV/East–West/model-comparison solid-angle reduction previously
   *approximated* each polar cap by stretching the adjacent 15-degree
   latitude band to the pole (because no genuine polar sample existed to
   carry that weight). Now that verified-self-consistent polar samples
   exist, each pole is folded into every reduction (`reduce_fov`,
   `model_comparison`, `asymptotic_comparison`, via the shared
   `fov_direction_terms` helper) as its own direction with the exact
   polar-cap solid angle (`pole_cap_weight_sr`, cross-checked in
   `--self-test` against the independent formula
   `2*pi*(1 - cos(half_angle))`). Total solid angle is unchanged — still
   exactly `4*pi` sr, reapportioned rather than altered — but the number
   compared against the 0.98/0.02/0.15/0.01 thresholds is now the real
   traced value at the two points where a pole falls inside the 45-degree
   aperture, rather than a value copied from a neighboring band.

**What did not change.** No CLI default and no numeric acceptance threshold
in `build_parser()`, `reference_C8_expected_physics.csv`, or
`reference_C8_acceptance_contract.csv` was altered; gate C8-G11 was added as
a strictly additional hard gate (`reference_C8_acceptance_contract.csv` now
lists 11 gates instead of 10).

### Verification performed

Both fixes were validated with `python3 run_C8.py --self-test` and
`--validate-references` (pure Python, no AMPS/MPI required), which now also
exercise: the corrected direction-grid counts and exact `4*pi` sr
conservation; an independent cross-check of the polar-cap solid-angle
formula; a synthetic full (regular+polar) DIRECT_ACCESS cube that passes
coverage and gate C8-G11 cleanly; the same cube with one corrupted polar
duplicate, confirmed to trip gate C8-G11 with an explanatory message; a
cube missing the polar rows entirely, confirmed to still trip gate C8-G01
(proving the completeness check was not weakened); and gate C8-G07 accepting
a physically correct East–West sign pattern while still rejecting the exact
sign-inverted pattern `CUTOFF_BACKTRACE_CHARGE SAME` used to produce.

**Not yet performed:** an actual AMPS/T96/T05 re-run against a real
compiled solver. The self-test suite validates the runner's logic
exhaustively but cannot substitute for re-running
`python3 run_C8.py --profile ROUTINE --amps ./amps -np 4 -nt 16` in a real
AMPS build environment to confirm C8 now reports `PASS`.
