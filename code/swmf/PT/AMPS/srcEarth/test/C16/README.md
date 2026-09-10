# C16 — trajectory termination and access-state closure

## Purpose

C16 validates the interface between the AMPS trajectory mover and the
geomagnetic access classifier. It answers a narrow but safety-critical question:
**does every trajectory termination reason produce the correct three-state
access classification?**

The test prevents false geomagnetic access caused by treating a resource limit
or numerical failure as a physically escaped trajectory. It also checks that an
already resolved classification is stable when the allowed trace time is
increased.

C16 is a model-validation test, not merely a parser test. The routine profile
runs an analytic centered dipole and a driven T05 field at five magnetospheric
locations, over the full directional sky and a seven-rigidity grid. Separate,
small integration probes force each supported resource-limit exit.

## One-line runner command

Run from the AMPS repository root:

```bash
python3 srcEarth/test/C16/run_C16.py --profile ROUTINE --amps ./amps -np 4 -nt 16
```

The runner returns `0` only when every configured C16 gate passes. It returns
`1` for an AMPS execution failure, incomplete or malformed output, a
state/reason inconsistency, a fatal numerical exit, a budget-convergence
regression, a failed deliberate probe, or excessive final unresolved coverage.

## Required AMPS capability

C16 requires a GRIDLESS cutoff executable that supports:

- `CUTOFF_SEARCH_ALGORITHM DIRECT_ACCESS`;
- an explicit `CUTOFF_RIGIDITY_LIST_GV` with
  `CUTOFF_DIRECT_ACCESS_ADAPTIVE F`;
- `CUTOFF_TRACE_LIMIT_POLICY UNRESOLVED`;
- DIPOLE and T05 field models;
- POINTS output and a full-sphere directional grid; and
- the production trajectory diagnostics `access_state`, `termination_code`,
  `trace_time_s`, `trace_distance_Re`, and `trace_steps` in each
  `cutoff_gridless_dir_access_point_####.dat` file.

The input deliberately sets `CUTOFF_SAMPLING VERTICAL`. The current AMPS parser
requires that token whenever `DIRECT_ACCESS` or `CUTOFF_RIGIDITY_LIST_GV` is
used; the `DIRECTIONAL_MAP` and `DIRMAP_*` directives still control the actual
direction-resolved calculation.

C16 does **not** use `CUTOFF_UNRESOLVED_EXTENSION_*`, `CUTOFF_DEBUG_EXIT_*`, or
`TRAP_*` keywords. Those extensions are not necessary for this test and are not
accepted by every supported parser.

## Reference solution

The authoritative reference is an exact invariant contract rather than a table
of platform-dependent cutoff values.

| Code | Termination reason | Required `access_state` | Policy |
|---:|---|---:|---|
| 0 | `OUTER_BOUNDARY_ALLOWED` | 1 (`ALLOWED`) | Physical escape |
| 1 | `INNER_BOUNDARY_FORBIDDEN` | 0 (`FORBIDDEN`) | Physical shielding |
| 2 | `MAGNETICALLY_TRAPPED_FORBIDDEN` | 0 (`FORBIDDEN`) | Physical trapping |
| 3 | `TIME_LIMIT` | 2 (`UNRESOLVED`) | Resource limit; never forbidden/allowed |
| 4 | `STEP_LIMIT` | 2 (`UNRESOLVED`) | Resource limit; never forbidden/allowed |
| 5 | `DISTANCE_LIMIT` | 2 (`UNRESOLVED`) | Resource limit; never forbidden/allowed |
| 6 | `INVALID_TIME_STEP` | 2 (`UNRESOLVED`) | Fatal validation error |
| 7 | `INVALID_FIELD` | 2 (`UNRESOLVED`) | Fatal validation error |
| 8 | `NUMERICAL_FAILURE` | 2 (`UNRESOLVED`) | Fatal validation error |
| 9 | `DRIFT_TRAPPED_FORBIDDEN` | 0 (`FORBIDDEN`) | Physical trapping |

This table is committed as `reference_C16_termination_contract.csv`; the runner
verifies it against its executable mapping before launching AMPS. Codes 6–8 are
required to retain the unresolved state for output closure, but any occurrence
still fails C16 because a model-validation calculation must not contain a fatal
numerical trajectory.

The second exact reference is the budget partial order:

```text
UNRESOLVED at a shorter budget -> UNRESOLVED, ALLOWED, or FORBIDDEN later
ALLOWED at a shorter budget    -> ALLOWED at every longer budget
FORBIDDEN at a shorter budget  -> FORBIDDEN at every longer budget
```

This is stronger than comparing only aggregate percentages: C16 compares the
same point, direction, and rigidity node trajectory by trajectory.

## Routine calculation matrix

The default `ROUTINE` profile uses:

| Quantity | Setting |
|---|---|
| Movers | BORIS |
| Fields | DIPOLE, T05 |
| T05 epoch | 2012-05-17 06:00:00 UTC |
| Observation points | longitude 0°, latitudes −60°, −30°, 0°, 30°, 60° |
| Altitude | 9000 km |
| Rigidities | 0.1, 0.2, 0.5, 1, 2, 5, 10 GV |
| Direction grid | 30° longitude × 30° latitude, full sphere including polar rows |
| Trace-time budgets | 60, 300, 600 s |
| Distance limit | disabled (`0`) in baseline runs |
| Maximum steps | 2,000,000 |
| Final unresolved ceiling | 0.50 per mover/field model |

There are six baseline AMPS invocations. Each contains 2,940 trajectories:
five points × 84 directions × seven rigidities. Three additional DIPOLE probes
contain 24 trajectories each, for 17,712 total trajectories in the default run.

## Deliberate resource-limit probes

The probe reference is committed as `reference_C16_probe_expectations.csv`.

| Probe | Controlled setting | Expected reason/state | Default gate |
|---|---|---|---:|
| Time | `MAX_TRACE_TIME=0.01 s` | code 3 / `UNRESOLVED` | ≥90% of samples |
| Step | `MAX_STEPS=1` | code 4 / `UNRESOLVED` | ≥90% of samples |
| Distance | `MAX_TRACE_DISTANCE=0.01 Re` | code 5 / `UNRESOLVED` | ≥90% of samples |

The probes use the normal production controls and output path. No test-only C++
hook or hidden parser option is needed. Requiring a fraction rather than exactly
100% permits a trajectory that physically reaches a boundary in the same first
integration interval, while still proving that the intended exit branch
dominates.

## Acceptance gates

C16 passes only when all applicable conditions hold:

1. Every point output exists and contains the required diagnostic columns.
2. Every requested direction and rigidity appears exactly once; missing,
   unexpected, duplicated, or non-finite rows fail.
3. Every `termination_code` is known and maps exactly to the required
   `access_state`.
4. Optional redundant `allowed`/`unresolved` columns agree with `access_state`.
5. Trace time, distance, and step diagnostics are finite and non-negative.
6. Resource-limit rows show that the corresponding configured limit was reached,
   within the configured diagnostic slack.
7. Codes 6, 7, and 8 never occur in an accepted AMPS run.
8. Every ordinary baseline includes at least one physical allowed and one
   physical forbidden sample, so both classifier branches are exercised.
9. Increasing the trace-time budget never changes or loses a resolved physical
   classification on the identical sample key.
10. The final-budget unresolved fraction is at or below the profile threshold
    for every mover/model pair.
11. Every enabled deliberate probe meets its expected-code fraction.

The test does not require the unresolved count to decrease by an arbitrary
minimum. Some field/rigidity combinations can remain legitimately unresolved;
the scientific requirements are non-regression and a bounded final fraction.

## Profiles and useful variants

```bash
# Fast DIPOLE package/integration check
python3 srcEarth/test/C16/run_C16.py --profile SMOKE --amps ./amps -np 2 -nt 4

# Higher angular resolution and a 2400-s final budget
python3 srcEarth/test/C16/run_C16.py --profile THOROUGH --amps ./amps -np 8 -nt 16

# Add an independent full-orbit mover
python3 srcEarth/test/C16/run_C16.py --profile ROUTINE --movers BORIS,RK4 --amps ./amps -np 4 -nt 16

# Analyze an already generated matrix
python3 srcEarth/test/C16/run_C16.py --profile ROUTINE --skip-run
```

`--skip-run` must use the same profile and overrides used to generate the
directory, because those options define the expected sample keys and run matrix.
Use `--help` for all grid, budget, probe, scheduler, and tolerance overrides.

## Package verification without AMPS

```bash
python3 srcEarth/test/C16/tests/run_self_tests.py
```

This compiles the runner, executes its synthetic closure/corruption tests,
verifies the complete nine-command ROUTINE dry-run matrix, and performs a full
synthetic `--skip-run` analysis that writes every documented result product. It
also confirms that the source reference files remain immutable. This check does
not validate the physical AMPS trajectory calculation; the one-line ROUTINE
command above does.

## Output files

All top-level products are written under `test_output/C16_exit_closure/` by
default:

| File | Content |
|---|---|
| `C16_summary.csv` | Per-run, per-point coverage and state totals |
| `C16_termination_counts.csv` | Counts/fractions for every code 0–9 in every run |
| `C16_sample_audit.csv` | Full trajectory-level state/reason/diagnostic audit |
| `C16_convergence.csv` | Aggregate result for each adjacent budget pair |
| `C16_sample_convergence.csv` | Exact sample-by-sample budget transitions |
| `C16_probe_summary.csv` | Intended-code coverage for each deliberate probe |
| `C16_commands.json` | Rendered command and run provenance |
| `C16_result.json` | Machine-readable overall result, settings, and messages |
| `reference_C16_*.csv` | Copies of the immutable reference contracts used |

Each AMPS work directory also retains its rendered `AMPS_PARAM_C16.in`, log, and
raw directional-access files. T05 work directories receive a local copy of the
fixed five-minute driver slice.

## Directory contents

```text
srcEarth/test/C16/
├── README.md
├── MANIFEST.sha256
├── AMPS_PARAM_C16_gridless.in
├── run_C16.py
├── reference_C16_termination_contract.csv
├── reference_C16_probe_expectations.csv
├── data/
│   └── ts05_driver_C16.txt
└── tests/
    └── run_self_tests.py
```

## Interpreting a failure

- `lacks required C16 columns` means the executable predates the production
  termination diagnostics or was built without them. Updating only the Python
  runner cannot repair that executable mismatch.
- `state_termination_mismatch` is a classifier defect: the reason and access
  state disagree on the same trajectory.
- `fatal_numerical_termination` identifies a field/mover/integration failure;
  inspect the matching sample and AMPS log before relaxing numerical controls.
- `resolved_state_flip` or `resolved_became_unresolved` means the physical
  classification is not stable under a larger trace budget.
- A high but monotone unresolved fraction indicates insufficient physical trace
  budget or missing trapping classification. Increase the final budget first;
  do not relabel time/step/distance limits as forbidden merely to make C16 pass.
- A probe dominated by a different limit can be investigated with its rendered
  input and trace diagnostics. Do not add unknown `CUTOFF_DEBUG_EXIT_*` keywords
  to a strict parser.
