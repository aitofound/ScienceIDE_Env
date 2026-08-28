# Row staging ledger

The row IDs and classifications below are copied from the controlling RHD plan.
Only **IMPLEMENT-NOW** rows have direct Harbor checks in this staging tree.
The validators intentionally fail closed for unmeasured numeric differences;
fixture self-tests exercise shape, finite-value, conservation, and near-miss
rejection without implying a scientific pass bound.

## IMPLEMENT-NOW (8)

1. `rhd-shocktube-01` — `RHD/Shock_Tubes/01`: first ideal-EOS RHD oracle slice.
2. `rhd-shocktube-05-entropy` — `RHD/Shock_Tubes/05`: selective entropy path.
3. `rhd-sedov2d-01-weno` — `RHD/Sedov2D/01`: 2-D WENO/characteristic blast.
4. `rhd-sedov2d-05-taub` — `RHD/Sedov2D/05`: 2-D Taub/PPM/RK3 path.
5. `rhd-jet-01-taub` — `RHD/Jet/01`: cylindrical Taub jet and custom tagging.
6. `rhd-jet-04-taub-entropy` — `RHD/Jet/04`: spherical Taub/entropy path.
7. `rhd-blast3d-03-taub` — `RHD/Blast3D/03`: 3-D Taub body-force path.
8. `rhd-riemann2d-01` — `RHD/Riemann_2D/01`: 2-D interacting-wave path.

## STAGE (9)

- `rad-rhd-shock-01-default`, `rad-rhd-shock-02-fixedpoint`,
  `rad-rhd-shock-03-newton`, `rad-rhd-blast-04-3d`,
  `rad-rhd-shadow-02-opacity`, `rad-hd-shocks-02-newton`,
  `rad-mhd-blast-06-polar`, `rad-hd-disk-planet-02`, and
  `rad-hd-shadow-01-limo3` remain documented only. They require the RHD
  foundation plus independently measured radiation moments, source/exchange
  ledgers, reduced-speed scaling, opacity/limiter behavior, and (where needed)
  custom auxiliary sources. No passing checks are fabricated for them here.

## BLOCK (1)

- `rad-rmhd-blast-01-witness` remains a cross-consumer witness only. It is
  blocked on separate RMHD ownership/build/attribution approval. This task does
  not duplicate or claim `Src/RMHD/Radiation/`, and no check directory exists.

The active Harbor target is deliberately withheld: no CPU target facts or
measurements were supplied. `target/_pending-cpu.json` is disabled until actual
CPU measurements and owner review populate a strict active descriptor.

## Implementation status (continuation session)

All 8 IMPLEMENT-NOW checks now have a complete leaf: `rubric.json`,
`Dockerfile` (per-check candidate runner, staged/never built), `run.sh`,
`validate.py` (delegates to `../../lib/generic_validator.py`, exposing the
two-argument `validate(reference, candidate)` signature Harbor's verifier
imports directly), and `fixtures/make.py` (delegates to
`../../../lib/fixture_builder.py`). `solution/solve.sh` and `tests/test.sh`
exist and are syntactically valid (`bash -n`, plus `py_compile` on every
embedded/associated Python module); neither was executed in this session.
`tests/test.sh` runs each active check's own fixture self-test (identity,
signed-zero arithmetic variation, and 7 reject-* defect fixtures) before
attempting any real HARBOR_REFERENCE_DIR/HARBOR_CANDIDATE_DIR grading, and
exits with Harbor's `unrun`/`missing_artifact_paths` contract (exit 2) when
no real oracle output exists yet, which is the current state.

The official structural validator
(`skills/package-sciaccel-task/scripts/validate-harbor-task.py
tasks/pluto-rhd-radiation`) now reports exactly two violations, both
intentional and deferred rather than fixed:

- `missing-acceleration-label` — none of the 8 built rows is the row the
  controlling plan actually earmarks for the `acceleration` label
  (`rad-rhd-blast-04-3d`, a stage row with no check directory yet); forcing
  the label onto an unrelated, unmeasured 1-D/2-D/3-D RHD check would be an
  unsubstantiated claim, not a real one.
- `no-active-target` — no CPU has measured this tree; adding an active
  `target/*.json` would invent numbers this task explicitly must not invent.

A found-and-fixed defect: `rhd-shocktube-01` and `rhd-shocktube-05-entropy`
originally carried `gamma: 5/3` in `rubric.json`, but their real official
`pluto_01.ini`/`pluto_05.ini` set the runtime `GAMMA_EOS` parameter to 4/3
(confirmed via `Shock_Tubes/init.c:63`); see `comment/build-decision.md` for
the full evidence trail. Both are corrected.

## Full-contract repair (continuation session, PR #303 repair)

This session resolved the two intentional structural-validator violations
above, made `solution/solve.sh`/`tests/test.sh` a real oracle path, and fixed
6 of the 8 rows' `gamma` (see `comment/build-decision.md`'s matching
addendum -- both violations' original refusal reasoning was correct at the
time and is recorded, not deleted, above):

- **`missing-acceleration-label`**: `rad-rhd-blast-04-3d` (the controlling
  plan's original candidate for this label) still has no check directory --
  building it is out of this session's scope. Given Jason's updated
  contract requires an active label now rather than indefinitely deferring
  it, `rhd-blast3d-03-taub` carries it instead (`check.json`): it is the
  only 3-D configuration among the 8 (worse cell-count scaling and 3-D
  reconstruction/boundary cost than the 1-D/2-D rows), it is Taub EOS (a
  nonlinear per-cell closure vs. the ideal branch's closed form), and it
  carries an extra per-cell vector body-force source term -- three stacked,
  genuine, real compute/algorithm reasons, not merely "biggest grid" (it
  is not: `rhd-riemann2d-01` has more cells at its chosen resolution,
  400x400 vs. 32x32x32, but is 2-D ideal-EOS with no source term).
- **`no-active-target`**: `target/pluto-rhd-radiation-cpu.json` is now
  active (no `_` prefix). It invents no performance number: runner/toolchain
  facts are read directly from this leaf's own pinned Dockerfiles and
  archive provenance; every host resource field (vendor, architecture, cpu,
  cores, threads, memory) stays `"UNMEASURED"`/`null` until a real CPU run
  is timed. `target/_pending-cpu.json` is left exactly as it was (disabled,
  historically accurate for when it was written); nothing was deleted.
- **`solution/solve.sh`**: rewritten from workspace-only staging into a real
  compile-with-`setup.py --auto-update`/`make`, run-the-binary, stage-the-
  output oracle path for all 8 rows, aggregating row failures (every row is
  attempted even after an earlier one fails) rather than stopping at the
  first. Build residue and staged oracle output live under
  `solution/workspace/` and `solution/oracle/` respectively, both excluded
  from tracking by a new `solution/.gitignore` (nested one level inside the
  opaque `solution/` subtree, so it does not add to the closed leaf root's
  file list). Not executed in this session -- no compiler/PLUTO/Docker was
  invoked; see the top-level repair's diff audit for what was and was not
  run.
- **`tests/test.sh`**: its fixture self-test (`self_test_fixtures`) is
  unchanged in behavior but factored out to `tests/lib/discriminate.py`, now
  also runnable standalone via `tests/discriminate.sh` -- see that module's
  own docstring for why: `npm run check:validators` scans
  `tasks/<slug>/checks/`, never `tests/checks/`, so this Harbor-schema leaf
  had zero coverage from that repository gate. That repository script is
  unmodified; this is a task-local substitute for the coverage it cannot
  provide here.
- **New fixture variants**: `accept-with-side-files` (proves the
  side-file-tolerance fix accepts a real results directory), `reject-wrong-
  name`, `reject-malformed-grid`, `reject-wrong-endian` (the fabricated
  contract's own retired `little_endian` token, as a direct regression
  guard). All 8 checks' `fixtures.accept`/`fixtures.reject` rubric fields
  and `tests/lib/discriminate.py`'s `ACCEPT_KINDS`/`REJECT_KINDS` were
  updated together.

Static/synthetic self-test after these changes: `tests/lib/discriminate.py`
run directly (`PYTHONDONTWRITEBYTECODE=1`, no Docker/PLUTO/compiler) --
result recorded in this repair's own diff-audit output, not duplicated here.
