# PR453 official validation evidence (2026-09-09)

This directory publishes the exact evidence from the already-completed official
PR453 run. It is evidence publication only: this commit did not build, solve,
selfcheck, calibrate, verify, rerun, retimestamp, or edit any executed
fingerprint, source head, deck, rubric, check, target, resource, window, or
acceptance rule.

## Immutable execution identity

- Task: `batsrus-outer-heliosphere`; PR453 was `OPEN` at publication and the
  published run executed at head
  `b7b53c64af6666f9548ff76bc075114b221c50d6`.
- Executed BATSRUS source commit: `9dfe746d48aa650b5209c3039f1a8676bc624899`.
  The immutable CLI execution-origin main record is
  `25823f1add2a1fbd9e14bab4550a1a2428095721`; captured-main scripts-tree
  provenance remains `ec1606a4` and is not relabeled.
- The current-main `package-sciaccel-task` skill is v5.11.10, read from remote
  Git commit `1d0c74983e7a191be4e1a8abafc9632d665d8a4d`; its SHA-256 is
  `56f0445751beb2b46e70703c15d97ea0a459e0dabf9c11388ccd3be03ce56c18` and the
  executed CLI SHA-256 is
  `95613ad3b2ac7eaa4681b3c44dd37e41c44c050c5fb04f320d13494f200119d5`.
  These are provenance values, not a retimestamp or relabeling of the run.
- Official receipt source path:
  `/home/huangzesen/.sciaccel_pipeline/batsrus/launch-receipts/sab-pr453-repair-20260909-em-41ba/launch-receipt.json`;
  official runroot:
  `/home/huangzesen/.sciaccel_pipeline/batsrus/runs/batsrus-outer-heliosphere/repair-20260909-em-41ba-official`.

## Acceptance and boundary

The official receipt records build and self-check exits `0/0`, from
`2026-09-09T20:13:30Z` through `2026-09-09T21:02:59Z`. Generated
`self-validation.json` records the three zero-exit nominal, variant and
altbuild solves, self-validation from `2026-09-09T20:14:51Z` through
`2026-09-09T21:02:59Z`, contract fingerprint
`3b7451e130bb5333974d96856246b29c34e11e814b19fc0fc33d3cb239b4d517`, and a
`passed` result with empty problems and warnings. Resources were 4 CPU and
8.0 GB with network disabled and a 900-second guidance budget.

The official science result is **FULL_SCIENCE_PASS**: 7/7 actual staged checks,
reward `1.0`, verifier exit `0`, and 7/7 declared altbuild rows passed. Every
one of the 54 graded nominal/variant/altbuild raw output files is retained at
its exact remote path with SHA-256 and substantive-value statistics; raw science
output bytes are not copied here. The 76 bounded copied-evidence files match
their recorded remote SHA-256 and byte sizes. The exact 21 mode/check result
directories contain their graded file sets plus `run.ok` and `run.log`, with no
failed or skipped marker. The actual check schema is `labels` only: `outerhelio`
has `['acceleration']`, and the other six have `[]`; no `expectedcheck.json` was
invented.

The generated canonical sibling records are `../runtime-metadata.json` and
`../self-validation.json`. The pre-publication records were preserved byte for
byte at `../history/pre-2026-09-09/` before replacement. The full collector
report, receipt, run logs, oracle manifests, marker/log evidence, seven exact
check JSON files, per-row altbuild JSON, audits and integrity inventory are
under `collection/`. See `collection/collection-file-hashes.json` for the
publication collection inventory. Staged CLI and skill text are intentionally
hash-only.

This publication establishes the official generated science evidence, not an
independent reproduction, physical/upstream correctness claim, CI pass, merge,
or human review decision. The collector's receipt field
`science_result=not_claimed_launch_handoff_only` is not used as proof by itself;
the proof bundle is the generated receipt, reward/self-validation rows, raw
path/hash audit, substantive checks, and marker audit.

## CI/review boundary

CI/review state is observed separately after publication and is not inferred
from this science result. No CI retry, merge, or human review action is part of
this publication.
