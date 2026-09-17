# C12 — Particle-mover validation in an analytical dipole

C12 validates GRIDLESS particle-mover dispatch, full-orbit numerical
integration, cutoff search, and parallel output assembly in a centered,
aligned dipole with a closed-form vertical-cutoff reference. Guiding-center
(GC) movers are available as a separate, explicit capability campaign; they
are not silently treated as exact solutions of the full-orbit problem.

The corrected C12 does **not** assume that every mover represents the same
physical approximation. It applies separate contracts to full-orbit and
guiding-center movers and rejects incomplete or malformed output before any
physics comparison is attempted.

## Recommended command

Run from the AMPS repository root:

```bash
python3 srcEarth/test/C12/run_C12.py --profile ROUTINE --amps ./amps -np 4 -nt 16
```

The default ROUTINE matrix has 12 required AMPS cases:

- BORIS, RK2, RK4, and RK6 at `DT_TRACE = 1, 0.5, 0.25 s`;
- two spherical GSM shells in every case: 9000 and 25000 km altitude;
- a 30° longitude by 15° latitude grid, or 312 rows per two-shell product;
- five scientific latitudes: −60°, −30°, 0°, +30°, and +60°.

`-np` and `-nt` may both be greater than one. The generated command uses the
GRIDLESS controls:

```text
-gridless-parallel THREADS
-gridless-threads 16
-gridless-mpi-scheduler DYNAMIC
-gridless-mpi-dynamic-chunk 0
```

The earlier density/flux threading flags have been removed because C12 executes
the GRIDLESS cutoff solver, not a density calculation.

## Scientific reference

For a centered, aligned dipole, the vertical Størmer cutoff is

```text
Rc(latitude,h) = R0 cos(latitude)^4 / r_RE^2,
r_RE = (RE + h) / RE,
R0 = 14.8982941255949 GV,
RE = 6371.2 km.
```

Representative values are:

| Altitude | Latitude | Reference cutoff |
|---:|---:|---:|
| 9000 km | 0° | 2.55954915233 GV |
| 9000 km | ±30° | 1.43974639819 GV |
| 9000 km | ±60° | 0.159971822021 GV |
| 25000 km | 0° | 0.614492742045 GV |
| 25000 km | ±30° | 0.345652167401 GV |
| 25000 km | ±60° | 0.0384057963778 GV |

The checked-in `reference_C12_stormer_movers.csv` contains the complete
seven-mover, two-altitude, five-latitude analytical/diagnostic inventory. The
runner validates its
schema, key uniqueness, exact coverage, finite values, and agreement with an
independent recalculation of the formula. It records the SHA-256 checksum in
`C12_result.json`.

The source reference is immutable during execution. Normal runs, dry runs, and
`--skip-run` never rewrite it. A custom run writes its evaluated analytical
rows to `C12_reference_used.csv` inside the result directory.

## Why full-orbit and guiding-center movers are separated

### Full-orbit movers

BORIS, RK2, RK4, and RK6 integrate the particle gyromotion. Their finest-step
results are compared with Størmer at both shell altitudes and all five target
latitudes. RK4 and RK6 additionally form the default hard same-observable
parity pair. Successive time-step maps and the wider BORIS/RK2/RK4/RK6 spread
are recorded as diagnostics under the default adaptive-step configuration.

### Guiding-center movers

GC2, GC4, and GC6 integrate a reduced approximation. Merely moving the shell
outward does not make this approximation exact at the Størmer cutoff. In a
dipole, `Rc ~ r^-2` and `B ~ r^-3`, so the cutoff-particle gyroradius grows in
proportion to radial distance relative to the system geometry. Near the
magnetic equator, finite-gyroradius effects can therefore remain large.

C12 therefore never uses a GC/Størmer discrepancy as an analytical pass/fail
gate. GC cases are excluded by default and use one of three explicit policies:

- `--gc-policy SKIP` (default): no GC mover may be selected;
- `--gc-policy DIAGNOSTIC`: run selected GC movers and report execution,
  schema, grid closure, sentinel, unresolved, Størmer-context, and GC-parity
  results without changing C12's exit status;
- `--gc-policy REQUIRE`: require GC execution, complete resolved output, and
  selected-GC parity, but still do not claim analytical Størmer equivalence.

For example, this opt-in campaign reproduces all 15 cases while keeping GC
results scientifically labeled:

```bash
python3 srcEarth/test/C12/run_C12.py --profile ROUTINE \
  --movers BORIS,RK2,RK4,RK6,GC2,GC4,GC6 \
  --gc-policy DIAGNOSTIC --amps ./amps -np 4 -nt 16
```

In the supplied AMPS run, GC2 emitted a negative cutoff sentinel, GC6 exited
nonzero, and GC4 produced complete but systematically non-Størmer values. The
DIAGNOSTIC policy preserves all three findings without allowing them to
invalidate the full-orbit model validation. `REQUIRE` intentionally fails on
the GC2/GC6 capability defects until their AMPS-side causes are corrected.

## Complete hard-gate sequence

The machine-readable inventory is in
`reference_C12_acceptance_contract.csv`. The runner evaluates gates in this
order:

1. The source reference and acceptance-contract CSV files must have exact,
   complete, unique, finite schemas.
2. Every AMPS command must return zero.
3. The shell product must declare longitude, latitude, and a recognized cutoff
   column explicitly. Positional column guesses are forbidden.
4. Every numeric row must have exactly the width declared by `VARIABLES`.
5. The output must contain exactly 312 unique coordinates: two altitudes × 12
   longitudes × 13 latitudes. Missing and unexpected coordinates fail.
6. All coordinates and cutoff values must be finite, and cutoffs must be
   nonnegative. Thus `NaN`, infinity, and negative error sentinels fail.
7. If unresolved or below/above-range columns are present, any asserted flag
   fails the case.
8. Finest-step full-orbit rows must pass their mover accuracy-class Størmer
   envelope.
9. Selected RK4 and RK6 cases must pass the high-order parity envelope.
10. Successive-map differences and all-full-orbit parity are recorded as
    diagnostics. Fixed-step convergence becomes hard only when explicitly
    requested with `--adaptive-dt F --require-convergence`.
11. Under `--gc-policy DIAGNOSTIC`, GC failures are labeled and recorded but
    are nonfatal. Under `REQUIRE`, GC execution, structure, resolved output,
    and selected-GC parity become hard; GC/Størmer residuals remain diagnostic.
12. If AMPS prints MPI task-closure diagnostics, `totalTasks` must equal 312 and
    the sum over ranks must equal `totalTasks`.

The process exit status is zero only when all applicable hard gates pass.

## Størmer error envelope

The cutoff search is a finite-time trajectory classifier. A universal
sub-per-mille tolerance would make C12 fail stable, physically acceptable
solutions at small high-latitude cutoffs. The default per-coordinate allowance
is therefore an absolute-plus-relative envelope:

```text
allowed = 0.005 GV + relative_tolerance * max(Rc_reference, 0.05 GV)
```

with:

| Contract | Relative tolerance | Role |
|---|---:|---|
| RK4/RK6, `|latitude| < 50°` | 5% | Hard high-order analytical gate |
| BORIS/RK2, `|latitude| < 50°` | 15% | Hard lower-order analytical gate |
| Every full-orbit mover, `|latitude| >= 50°` | 25% | Hard small-cutoff gate |
| GC versus Størmer | 25% | Diagnostic context only |

The 0.005-GV absolute term and 0.05-GV scale floor protect small cutoffs from an
ill-conditioned relative-only metric while still rejecting collapse to the
lower search boundary at the default 25000-km, ±60° point.

Every tolerance is exposed through the CLI for controlled sensitivity studies.
Changing it changes the validation contract and should be recorded with the
result.

### Evidence used to calibrate the default classes

The supplied 4-rank/16-thread ROUTINE result had complete 312-row output for
all full-orbit cases. At the finest requested cap, every RK4/RK6 analytical
coordinate passed the 5% mid-latitude/25% high-latitude contract. BORIS and RK2
were stable lower-order solutions but reached approximately 11.4% and 8.9%
worst-coordinate relative error, respectively; this is why their declared
mid-latitude class is 15% rather than the high-order 5% class. A conservative
bound formed from the supplied RK4/RK6 per-latitude extrema also passes the
3% mid-latitude/10% high-latitude parity envelope.

This calibration does not waive structural failures, unresolved states, range
flags, negative sentinels, or analytical collapse. It also leaves the tighter
RK4/RK6 gates intact, so a shared deterioration of the accurate movers cannot
be hidden by the lower-order envelope.

## Time-step sensitivity

For three successive time-step caps `(coarse, medium, fine)`, the runner forms
normalized RMS map differences:

```text
D_coarse = RMS((Rc_coarse - Rc_medium) / max(Rc_reference, 0.05 GV))
D_fine   = RMS((Rc_medium - Rc_fine) / max(Rc_reference, 0.05 GV))
```

The reported robust comparison is:

```text
D_fine <= 1.25 D_coarse + 0.01.
```

A strict formal-order estimate is deliberately avoided because a cutoff is a
classification boundary: a small time-step change can move a transition across
the final search tolerance without representing an unstable integrator.
Moreover, with `ADAPTIVE_DT T`, `DT_TRACE` is a cap rather than necessarily the
accepted step; several requested caps can therefore produce the same map.

For this reason the ROUTINE and THOROUGH profiles record the comparison but do
not make it a hard gate. A controlled fixed-step campaign can enable the gate:

```bash
python3 srcEarth/test/C12/run_C12.py --profile ROUTINE \
  --adaptive-dt F --require-convergence --amps ./amps -np 4 -nt 16
```

The runner rejects `--require-convergence` with adaptive stepping so the output
cannot be mislabeled as a numerical-order validation. The finest solution must
always pass its independent analytical gate.

## Cross-mover parity

At the finest time step, each coordinate is compared across selected movers.
The default hard maximum spread for the high-order RK4/RK6 pair is:

```text
RK4/RK6 at |latitude| < 50 degrees:
  0.003 GV + 0.03 * max(Rc_reference, 0.05 GV)

RK4/RK6 at |latitude| >= 50 degrees:
  0.003 GV + 0.10 * max(Rc_reference, 0.05 GV)
```

The wider all-full-orbit spread is diagnostic: 15% below 50° and 25% at
higher latitude, with a 0.005-GV absolute term. This exposes BORIS/RK2 changes
without forcing lower-order methods into the high-order envelope. GC movers
are a separate capability group and become hard only with `--gc-policy
REQUIRE`. Full-orbit and GC results are never mixed into one parity group.

High-order parity is enforced by default and can be made diagnostic with
`--no-enforce-cross-mover` for an explicitly exploratory run.

## Geometry and units

Bare `DOMAIN_*` and `R_INNER` values are interpreted by the common AMPS parser
as kilometres. The runner converts the default 35-Earth-radius box to:

```text
DOMAIN_[XYZ]_MIN = -222992 km
DOMAIN_[XYZ]_MAX = +222992 km
R_INNER          = 6371.2 km
```

It checks before launching AMPS that the box encloses both observation shells
and that `R_INNER` lies inside them. This prevents an immediate outer-boundary
classification caused by an incorrect 35-km box.

The shell is spherical and defined directly in GSM. `DIPOLE_TILT=0` aligns the
dipole axis with GSM Z, making shell latitude the magnetic latitude used by the
analytical formula. C12 deliberately does not use `SHELL_GEOMETRY GEODETIC`,
which would rotate GEO/WGS-84 positions into GSM.

## Profiles

| Profile | Default movers | Full-orbit time steps | Scan samples | Trace time | Purpose |
|---|---|---|---:|---:|---|
| SMOKE | BORIS, RK4, RK6 | 1 s only | 80 | 300 s | Command/schema and analytical sanity |
| ROUTINE | BORIS, RK2, RK4, RK6 | 1, 0.5, 0.25 s | 200 | 600 s | Recommended regular validation; 12 cases |
| THOROUGH | BORIS, RK2, RK4, RK6 | 1, 0.5, 0.25, 0.125 s | 320 | 1200 s | Release sensitivity campaign; 16 cases |

Profiles may be overridden, for example:

```bash
python3 srcEarth/test/C12/run_C12.py \
  --profile ROUTINE --movers BORIS,RK4,RK6 \
  --dt-scales 1,0.5,0.25 --amps ./amps -np 8 -nt 8
```

Hard convergence additionally requires `--adaptive-dt F` and at least three
unique positive time-step scales.

## Offline verification

Run all package, parser, and deliberately damaged-output checks without AMPS:

```bash
python3 srcEarth/test/C12/run_C12.py --self-test
```

or:

```bash
python3 srcEarth/test/C12/tests/run_self_tests.py
```

The self-test verifies:

- complete reference schema, formula, and key coverage;
- the machine-readable acceptance contract;
- multiline Tecplot `VARIABLES` parsing;
- recognition of the modern `rc_upper_gv` schema;
- rejection of truncated, duplicate, and non-finite datasets;
- kilometre domain rendering;
- use of GRIDLESS rather than density thread flags;
- the 12-case full-orbit ROUTINE default and opt-in GC matrix;
- separation of lower- and high-order analytical envelopes;
- exclusion of GC values from the hard Størmer contract; and
- immutability of the source reference.

Validate only the two checked-in reference files:

```bash
python3 srcEarth/test/C12/run_C12.py --validate-references
```

Render every input and command without launching AMPS:

```bash
python3 srcEarth/test/C12/run_C12.py --profile ROUTINE --dry-run
```

## Output products

The default result root is `test_output/C12_gridless`.

| Product | Contents |
|---|---|
| `C12_result.json` | Complete configuration, GC policy, commands, checksums, required/diagnostic case status, convergence, parity, and messages |
| `C12_summary.csv` | Per-case/per-altitude/per-latitude Størmer statistics and gate status |
| `C12_convergence.csv` | Successive normalized RMS map differences for every full-orbit mover |
| `C12_cross_mover.csv` | Hard RK4/RK6 parity plus diagnostic all-full-orbit and optional GC parity |
| `C12_reference_used.csv` | Exact analytical values and tolerances evaluated in this invocation |
| `reference_C12_stormer_movers.csv` | Immutable source reference copied into results |
| `reference_C12_acceptance_contract.csv` | Machine-readable hard/diagnostic gate inventory |
| `<mover>/<dt>/AMPS_PARAM_C12.in` | Fully rendered, rerunnable input deck |
| `<mover>/<dt>/C12_amps.log` | Exact command and combined AMPS/MPI output |
| `<mover>/<dt>/cutoff_gridless_shells*.dat` | Scientific shell product |

Reanalyze existing case directories without rerunning AMPS:

```bash
python3 srcEarth/test/C12/run_C12.py --profile ROUTINE --skip-run
```

The same profile, mover list, time-step scales, grid, altitudes, and tolerances
used to generate those directories must be supplied during reanalysis.

## Interpreting failures

- **Execution or schema failure:** parser/CLI incompatibility, missing mover,
  or an AMPS runtime error—not a physics residual.
- **Grid closure failure:** incomplete MPI assembly, unexpected shell settings,
  stale output, or a changed output coordinate contract.
- **Non-finite/negative failure:** unresolved numerical state or a cutoff error
  sentinel; these values can never pass through IEEE comparison behavior.
- **Størmer failure in all full movers:** shared cutoff-search, field,
  boundary-classification, coordinate, or unit problem.
- **Størmer failure in one full mover:** mover-specific integration or adaptive
  step-control issue.
- **Hard fixed-step convergence failure:** the mover becomes less
  self-consistent as the actual fixed step is reduced. Adaptive-profile
  sensitivity rows are diagnostics, not failures.
- **High-order parity failure:** RK4 and RK6 disagree at the same finest step;
  inspect dispatch, time integration, and cutoff classification.
- **All-full-orbit diagnostic spread:** expected accuracy-class information for
  BORIS/RK2 versus RK4/RK6; the individual Størmer gates remain authoritative.
- **GC diagnostic execution/sentinel failure:** the selected AMPS build does
  not currently provide a complete resolved cutoff product for that GC mover.
- **GC/Størmer discrepancy:** expected evidence about different reduced versus
  full-orbit physics; it is not by itself a model defect.

## Files in the C12 package

```text
run_C12.py
AMPS_PARAM_C12_gridless.in
reference_C12_stormer_movers.csv
reference_C12_acceptance_contract.csv
README.md
tests/run_self_tests.py
```
