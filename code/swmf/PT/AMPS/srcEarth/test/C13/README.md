# C13 — GRIDLESS MPI scheduler correctness and load distribution

C13 validates that AMPS GRIDLESS cutoff calculations are independent of MPI
rank count, inter-rank scheduling policy, and DYNAMIC chunk size. It also checks
that every scheduled trajectory task is accounted for exactly once.

The test is useful for model validation because it does not stop at scheduler
bookkeeping. A centered-dipole branch is anchored to the analytical vertical
Størmer cutoff, and a driven T05 branch exercises the production empirical-field
and driver-table paths. Therefore a set of mutually consistent but physically
wrong parallel maps cannot pass the complete test.

## Recommended one-line command

Run from the AMPS repository root after compiling `./amps`:

```bash
python3 srcEarth/test/C13/run_C13.py --profile ROUTINE --amps ./amps -nt 1
```

`-nt 1` is intentional: C13 isolates **inter-rank MPI scheduling**. `-nt > 1`
turns C13 into a hybrid MPI/thread thread-safety stress test. It must not be used
merely as an acceleration option unless T05 is already known to be thread-safe:
a four-thread validation run found schedule-dependent T05 changes as large as
15.6 GV and 163 integer-flag mismatches. C18 covers field initialization, while
C13 additionally exposes thread safety during coordinate-dependent T05 tracing.

The full planned campaign is:

```bash
python3 srcEarth/test/C13/run_C13.py --profile THOROUGH --amps ./amps -nt 1
```

Offline package verification requires no AMPS executable:

```bash
python3 srcEarth/test/C13/run_C13.py --self-test
```

## Validation question

C13 answers four distinct questions:

1. **Was all work executed exactly once?** The expected task count, the AMPS
   all-rank sum, and an independent sum of the per-rank lines must be identical.
2. **Did the scheduler change the scientific result?** Every DIPOLE and T05
   shell product is compared with the corresponding one-rank DYNAMIC baseline.
3. **Is the common baseline physically meaningful?** The one-rank centered
   dipole is compared with the analytical Størmer vertical cutoff.
4. **How did the work distribute and scale?** C13 reports task counts, whole-job
   speedup, parallel efficiency, and optional explicit per-rank timing.

Task-count balance and runtime balance are not the same observable. Trajectories
near a cutoff or trapping region can cost far more than promptly escaping
trajectories. C13 reports whole-job time and consumes explicit per-rank timing
when AMPS emits it. Task-count imbalance is diagnostic by default: a DYNAMIC
rank that finishes inexpensive trajectories sooner should receive more tasks.

## Scientific configuration

Every case uses the same physical/numerical problem; only scheduler parameters
and MPI rank count change:

- proton charge `+1`, mass `1.007276466621 amu`;
- BORIS mover by default;
- vertical incidence on one spherical GSM global shell at 500 km;
- `PENUMBRA_SCAN` with an accurate trace policy;
- 0.05–15 GV rigidity bracket (converted to parser-facing MeV/n by the runner);
- reversed-charge backtracing;
- trace-limit results preserved as `UNRESOLVED`;
- no electric field;
- a ±35 Earth-radius Cartesian outer box and a 1-Earth-radius inner boundary;
- parser-supported `GRIDLESS_MPI_SCHEDULER`,
  `GRIDLESS_MPI_DYNAMIC_CHUNK`, `GRIDLESS_PARALLEL`, and `GRIDLESS_THREADS`.

The two field branches are:

| Branch | Purpose | Reference |
|---|---|---|
| DIPOLE | Deterministic analytic control | External vertical Størmer solution plus exact schedule/rank equivalence |
| T05 | Production empirical-field and driver-reader exercise | One-rank T05 result, with scheduler/rank equivalence at relative tolerance `1e-7` |

The T05 epoch is `2012-05-17T06:00:00`. The bundled five-minute driver contains
the exact epoch and neighboring records. Its header deliberately uses `DST`,
which is the column name required by the current AMPS driver parser; it does not
use the unsupported `SYM-H` alias.

The DIPOLE branch uses a longer profile-specific trace budget (600 s in ROUTINE)
than T05 (300 s in ROUTINE). This follows the established Størmer-control setup
without doubling the cost of the much slower empirical-field matrix.

## Analytical reference solution

For an untilted centered dipole, the vertical cutoff used by C13 is

\[
R_c(\lambda,h)=R_0\frac{\cos^4\lambda}{r_{RE}^2},
\qquad
r_{RE}=\frac{R_E+h}{R_E},
\]

with

\[
R_0=0.299792458\,\frac{1}{4}(3.12\times10^{-5}\,\mathrm{T})R_E
=14.8982941256\ \mathrm{GV},
\qquad R_E=6371.2\ \mathrm{km}.
\]

At 500 km this gives 12.8089591671 GV at the magnetic equator,
7.20503953151 GV at ±30°, and 0.800559947946 GV at ±60°. The complete canonical
10° table is in `reference_C13_stormer_vertical.csv`; the self-test recalculates
every value rather than trusting the CSV blindly.

The shell is defined directly in GSM. Because `DIPOLE_TILT=0` aligns the dipole
axis with GSM Z, the shell latitude is the magnetic latitude in the equation.
C13 deliberately does not use `SHELL_GEOMETRY GEODETIC`: that would construct a
GEO/WGS-84 shell and rotate it to GSM, making GEO latitude an invalid substitute
for magnetic latitude at the selected epoch.

The default external-reference gate uses the moderate-latitude band
`30° <= |latitude| <= 45°` and `Rc >= 0.5 GV`. These rows are resolved and agree
with Størmer in the production PENUMBRA_SCAN configuration. The equator and
±60° are retained in `C13_stormer_comparison.csv` as diagnostics but are not
C13 gates: in the tested full-orbit scan they do not produce resolved brackets,
and ±15° shows a finite-orbit/effective-cutoff discrepancy larger than the
scan-aware tolerance. Full-latitude Størmer validation remains the purpose of
C1/C12; C13 needs a stable independent physics anchor for its scheduler matrix.
The allowed point error is

\[
0.10\ \mathrm{GV}
+1.5\,\Delta R_{scan}
+0.05\max(R_c,0.5\ \mathrm{GV}).
\]

At least 95% of required rows must be resolved/valid, and at least 95% of valid
rows must satisfy this scan-aware tolerance. The discretization term prevents a
coarse SMOKE scan from being judged against an impossible sub-bin tolerance;
THOROUGH automatically obtains a tighter absolute gate from its smaller scan
step.

## Execution matrix

The runner constructs the matrix before launching AMPS:

- DYNAMIC with automatic chunk selection (`chunk=0`) runs at every profile rank.
- BLOCK_CYCLIC and STATIC run at the largest profile rank.
- DYNAMIC with each explicit profile chunk runs at the largest profile rank.
- The complete matrix is repeated independently for DIPOLE and, where enabled,
  T05.

This changes one scheduling dimension at a time while avoiding redundant jobs.

| Profile | Models | Rank counts | Shell resolution | Scan nodes | Explicit chunks | Cases |
|---|---|---:|---:|---:|---:|---:|
| SMOKE | DIPOLE | 1, 4 | 60° × 30° | 32 | 1, 2 | 6 |
| ROUTINE | DIPOLE, T05 | 1, 4, 8 | 30° × 15° | 80 | 1, 4 | 14 |
| THOROUGH | DIPOLE, T05 | 1, 4, 8, 16 | 10° × 10° | 160 | 1, 8 | 16 |

Custom rank lists, grids, scan counts, schedulers, and chunks are supported. A
one-rank DYNAMIC case must remain present because it is the numerical baseline.

## Pass/fail contract

The checked-in machine-readable inventory is
`reference_C13_acceptance_contract.csv`. Default hard gates are:

1. Every AMPS invocation exits with status zero.
2. Every log contains a complete GRIDLESS task-distribution block.
3. `totalTasks (expected)`, `sum(all rank tasks)`, and the independent sum of
   `rank N: X tasks` are equal and positive.
4. Exactly one task-count line exists for every requested rank `0..np-1`.
5. AMPS `totalTasks` equals the independently calculated shell-coordinate count:
   `(360/lon_step) × (180/lat_step + 1)`; ROUTINE therefore requires 156 tasks.
6. The DIPOLE one-rank baseline passes the Størmer validity and accuracy gates.
7. Every DIPOLE case has exactly the same canonical binary64 cutoff product and
   integer flags as the one-rank DYNAMIC DIPOLE baseline.
8. Every T05 case has identical coordinate coverage and integer flags, and its
   three cutoff values agree with the one-rank DYNAMIC T05 baseline to
   `abs_error <= 1e-12 GV + 1e-7 × scale`.

The canonical product contains, sorted by longitude and latitude:

- `Rc_lower_GV`;
- `Rc_effective_GV`;
- `Rc_upper_GV`;
- unresolved count;
- lower/upper bracket-unresolved flags;
- optional below/above-range flags when the executable emits them.

Comparisons are keyed by coordinate. Missing, extra, or duplicate grid cells are
hard failures; output row order cannot hide a scheduling defect.

### Timing policy

Whole-job elapsed time is always measured. If AMPS emits all-rank lines such as
`rank N wall time = ... s`, C13 also records each rank and computes
`max(rank time)/mean(rank time)`.

Timing is diagnostic by default because runtime is sensitive to host load,
process placement, MPI implementation, and filesystem state. To make explicit
per-rank timing a hard local-platform gate, add for example:

```bash
--max-rank-time-imbalance 1.30
```

With a positive limit, missing/partial rank-time diagnostics are failures. This
option does **not** infer times from task counts.

The legacy-compatible `--max-dynamic-task-imbalance` option also defaults to
zero (diagnostic only). A positive value enables a local task-count gate, but
that is generally not recommended for DYNAMIC scheduling. Explicit chunks are
profile-scaled to the spatial inventory: ROUTINE uses 1 and 4 because it has
only 156 tasks over at most 8 ranks. Chunks 32 and 128 would serialize large
fractions of this problem and are useful only as deliberate coarse-chunk stress
cases.

## Files

| File | Role |
|---|---|
| `run_C13.py` | Matrix generation, execution, strict parsers, references, comparisons, summaries, self-test |
| `AMPS_PARAM_C13_gridless.in` | Commented parser-compatible input template |
| `data/ts05_driver_C13.txt` | Fixed five-minute T05 driver with canonical `DST` header |
| `reference_C13_stormer_vertical.csv` | Independent analytic solution at 500 km and 10° latitude spacing |
| `reference_C13_acceptance_contract.csv` | Human/machine-readable gate inventory |
| `tests/run_self_tests.py` | Working-directory-independent offline test wrapper |

## Result products

The default result root is `test_output/C13_gridless_scheduler`:

| Product | Contents |
|---|---|
| `C13_result.json` | Complete configuration, commands, checksums, fingerprints, diagnostics, comparisons, failures |
| `C13_summary.csv` | One row per case, including speedup, efficiency, and geometry/task closure |
| `C13_rank_distribution.csv` | One row per case/rank with task count and optional rank time |
| `C13_stormer_comparison.csv` | Per-coordinate DIPOLE reference, errors, tolerances, unresolved flags, and exclusions |
| `reference_C13_*.csv` | Exact reference copies used by the run |
| `<model>/<case>/AMPS_PARAM_C13.in` | Immutable rendered input |
| `<model>/<case>/C13_amps.log` | Combined AMPS/MPI output and exact command |
| `<model>/<case>/cutoff_gridless_shells_penumbra.dat` | Scientific cutoff product |

Exit status is `0` only when every required case and hard gate passes, `1` for a
validation failure, and `2`/argument-parser failure for malformed invocation or
package configuration.

## Useful commands

Inspect the SMOKE commands and rendered inputs without launching AMPS:

```bash
python3 srcEarth/test/C13/run_C13.py --profile SMOKE --amps ./amps --dry-run
```

Reanalyze an existing ROUTINE result tree without overwriting it:

```bash
python3 srcEarth/test/C13/run_C13.py --profile ROUTINE --skip-run
```

Run the full rank envelope with hybrid MPI/thread execution as an additional
thread-safety stress check (currently expected to expose the T05 race until the
T05 implementation is repaired):

```bash
python3 srcEarth/test/C13/run_C13.py --profile THOROUGH --amps ./amps -nt 16
```

Override only the rank envelope while retaining the ROUTINE physics problem:

```bash
python3 srcEarth/test/C13/run_C13.py --profile ROUTINE --ranks 1,2,4,8,16 --amps ./amps -nt 1
```

## Interpretation and non-goals

- A **task-closure failure** points to lost, duplicated, or incompletely reported
  work in the GRIDLESS inter-rank scheduler.
- A **DIPOLE fingerprint failure** shows nondeterministic or schedule-dependent
  scientific output in an analytic deterministic field.
- A **T05 tolerance failure** shows schedule/rank sensitivity in the empirical
  production path. If it occurs only for `-nt > 1`, it is strong evidence of
  T05 mutable shared state or a non-thread-safe cache, not acceptable roundoff.
  It does not by itself prove that the one-rank T05 field is physically correct.
- A **Størmer failure** indicates a physical/numerical cutoff problem shared by
  the common DIPOLE calculation, not merely a scheduler problem.
- A high DYNAMIC task-count imbalance may be healthy work stealing; interpret it
  together with elapsed time. A low value does not prove good wall-time balance.
- C13 reports speedup but does not establish a portable performance threshold,
  validate POSIX-thread field initialization (C18), or validate Mode3D scheduling.

This separation is deliberate: correctness gates remain reproducible across
machines, while performance evidence is retained without turning transient host
contention into a model-validation failure.
