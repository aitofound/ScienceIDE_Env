# Check-label contract discrepancy

Checked against the pinned pipeline revision `5836eae4c724264bce6600a75843b08f00be5ab4` (skill version 5.17.6).

- Repository root `CONTRIBUTING.md`, lines 67--68, says `check.json` has only `labels` and exactly one check carries `acceleration`; lines 149--152 repeat that review expectation.
- Pinned `sciaccelbench-pipeline/skill/package-sciaccel-task/SPEC.html`, line 335, says `custom` marks a check not backed by an official test, no other label is required, and an older `acceleration` label is ignored. Its revision history at line 394 states that version 5.17.0 removed the acceleration-label requirement.
- The current repository validator sources `scripts/validate.mjs` and `scripts/validate-core.mjs` contain no leaf-level `check.json`, `labels`, or `acceleration` enforcement. The pinned pipeline lint accepts all four official-test-backed checks with `labels: []`.

Resolution: follow the newer pinned normative SPEC and observed current validators, so official-test-backed checks keep `labels: []`. Do not invent an `acceleration` label solely to satisfy stale root prose. The root documentation should be reconciled separately by its maintainers; this task does not modify the pinned pipeline or unrelated root contract files.
