# PR450 official validation evidence (2026-09-09)

This directory publishes the exact evidence from the already-completed official
PR450 run. It is evidence publication only: this commit did not build, solve,
selfcheck, calibrate, verify, rerun, retimestamp, or fingerprint-edit the
scientific receipt.

## Immutable execution identity

- Task: `batsrus-multifluid-fivemoment`; PR 450 was `OPEN` at publication and
  the published run executed at head `9ada1287b9608b5402dafb01d37b7556f3b08df3`.
- Executed source commit: `9dfe746d48aa650b5209c3039f1a8676bc624899`;
  executed source-tree fingerprint: `2b7ceffe897cc21572847d7aed65d46eeddb4c40`;
  executed `origin_main` pin: `25823f1add2a1fbd9e14bab4550a1a2428095721`.
- Executed packaging skill: v5.11.10, SHA-256
  `56f0445751beb2b46e70703c15d97ea0a459e0dabf9c11388ccd3be03ce56c18`;
  executed CLI SHA-256:
  `95613ad3b2ac7eaa4681b3c44dd37e41c44c050c5fb04f320d13494f200119d5`.
  These are the actual executed values, not relabeled to this publication
  commit or to a new `main`.
- Executed consent remains literal `human_ref=exact6874ref`; the separate
  authority explanation is `collection/provenance/authority-mapping.json`.

## Acceptance and boundary

The corrected official terminal receipt is
`collection/provenance/official-terminal-exit-corrected.json` (SHA-256
`2a55c07e818dc90c8149165369bfa6eeb89081b72a398aa481fccf0b38040d56`). It
records one build and one selfcheck, both exit 0, from
`2026-09-09T19:49:14Z` through `2026-09-09T20:46:59Z`. The generated current
pipeline records are the canonical sibling files
`../runtime-metadata.json` and `../self-validation.json`; the raw runroot
copies are retained below for cross-checking.

The official science result is **FULL_SCIENCE_PASS**: 14/14 actual staged
checks, reward `1.0`, verifier exit `0`, no problems or warnings, 14/14
altbuild rows, 42 raw `final.out` outputs retained at their exact remote paths
and hashes, and `223` copied remote SHA-256 entries matching. The compact
per-row verifier evidence is under `collection/perrow/`; nominal, variant and
altbuild marker/log evidence is under `collection/oracle-metadata/`.
`collection/remote-hashes.tsv` and `collection/remote-hash-check.json` retain
the raw-output boundary; no raw `final.out` is copied here.

The 2026-09-09 run is distinct from the historical 2026-09-05 records. Those
records were preserved before canonical replacement at
`../history/pre-2026-09-09/`. The report's historical preflight refusal and
older 512d run remain historical and are not reused.

## Publication contents

`collection/` contains the two exact collection reports, generated current
receipt metadata, official supervisor/wrapper provenance, reward and
self-validation logs, three oracle manifests/build-run logs, 14 altbuild
per-row artifacts, 84 per-row run markers/logs, the consent input, and
integrity inventories. The collection's copied task inputs and generated
README are already present in the task contract/current comment; they are not
duplicated here. Staged CLI/skill text is intentionally hash-only. See
`collection/collection-file-hashes.json` for the source collection inventory.

This publication establishes the science evidence, not a merge or a human
review decision.

## CI/review readiness (observed once; separate from science)

The static Harbor check passed with rc `0` and one active target. The recorded
`python3 skills/package-sciaccel-task/scripts/sab.py status --task
... --ci-freshness` observation returned rc `1` with `fresh=false`; it was not a
new selfcheck and was not treated as a scientific failure. The immutable official
record carries final contract fingerprint
`c37174f562ffa629358fc2fcbd978310448eb6105d453407997167916d2b6e5c`; the current
PR contract computes to
`0867917ad3c42eb3db45d0942a107183ef4b39135ad129b5c09e8ef4fdc4d3eb`. The
collection's `Dockerfile` maps byte-for-byte to the public
`environment/Dockerfile`, and its `tests-Dockerfile` maps byte-for-byte to the
public `tests/Dockerfile`. The remaining 14 differences are only selfcheck-written
rubric evidence/timestamps (spread, bound fraction, floor, and altbuild time);
no source, tolerance, or rubric edit is made here.

Accordingly, `FULL_SCIENCE_PASS` is preserved as the official science result,
while CI/review readiness is **HOLD — STALE_INPUT_MISMATCH**. No fresh CI pass,
merge, or human review decision is claimed, and no rerun was authorized.


## Follow-up generated-evidence reconciliation (2026-09-09)

A read-only audit corrected the earlier conservative freshness HOLD. The original
comparison's 16 observations were: one false Dockerfile mismatch caused by mapping
collection `task-tests/Dockerfile` to public `tests/Dockerfile`, one staging alias
(`tests-Dockerfile`) not mapped by that comparison, and 14 rubric-file deltas.
The role-correct mapping is byte-identical: collection `Dockerfile` and
`environment-Dockerfile` -> public `environment/Dockerfile` (SHA-256
`4e57f0fb692cc5a16579d2239d112f357240f79276570d78882802a91cde7e5a`), and
collection `tests-Dockerfile` -> public `tests/Dockerfile` (SHA-256
`1897b9ede9be85e67e601b2d3c9bcfd6949264d64b9b6d7523e7bc0a2593744b`). All 84
other copied non-rubric task-input files and `task.toml` are also byte-identical.

The exact 14 rubric files (42 scalar leaves total) differ only at these JSON
paths, with no change to any value outside them:

- `evidence.altbuild.at`: generated `now()` timestamp (official 2026-09-09 time
  versus historical 2026-09-05 time).
- `evidence.altbuild.graded_identical`: generated boolean from the official
  altbuild graded-output comparison (`false` in all 14 official rows; absent in
  the historical records).
- `evidence.floor_how`: generated measurement/provenance sentence, whose date
  and bit-identical wording reflect the official run.

Thus `comparison.atol`, `comparison.rtol`, `comparison.files[*]` (paths and
formats), all rubric policy/observable/activation fields, all acceptance bounds,
all runtime knobs, and the numeric evidence fields (`floor`,
`floor_bound_fraction`, `self_validation_spread`, and
`self_validation_bound_fraction`) are unchanged. The 14 historical rubric files
are preserved under `pipeline/history/pre-2026-09-09/rubrics/`; canonical files
now contain the exact official emitted bytes only.

Pinned writer proof: `skills/package-sciaccel-task/scripts/_vendor/sciaccel_pipeline/taskcmds.py`
SHA-256 `8061a66664382215ad42069487284cc49082a3c81d708a3944b15fbdd699a4b2`. It
records the initial fingerprint at line 235; writes
`self_validation_spread`/`self_validation_bound_fraction` at lines 295-307;
grades altbuild through the check's validator and writes `floor`, `floor_how`,
`altbuild` (including `graded_identical`) and `floor_bound_fraction` at lines
339-364; then recomputes the final fingerprint at lines 377-381 and writes the
canonical self-validation at lines 382-384. Runtime metadata follows at lines
396-405.

Pinned reader proof: all 14 `tests/checks/*/validate.py` implementations have
the same normalized reader tail SHA-256
`6f74b07b07aee825959a9100378ec3b27ae9dd40f06039c96c54e5a921830f42`; each reads
only `rubric["comparison"]`, `comparison["atol"]`,
`comparison.get("rtol")`, and `comparison["files"]` (lines 112-116), then
computes `bound = atol + rtol * abs(reference)` (line 140). No validator reads
`evidence.*`. The pinned pipeline grader invokes that validator with
`--rubric rubric.json` (writer lines 190-210) and consumes only its generated
result fields; `evidence.floor` and the other evidence fields are written after
grading and are not grading criteria.

After copying only these exact emitted generated-evidence bytes, the non-scientific
status gate returned exit `0`, `fresh=true`, `checks=14`, `lint_errors=0`, and
`generated_files=[]`; this is an input-freshness result, not a CI pass, scientific
rerun, or human review decision.
