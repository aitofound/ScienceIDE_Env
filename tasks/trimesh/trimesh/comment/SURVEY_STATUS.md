> Current batch3 supersedes the historical counts below: 141 inventory rows, 75 suitable, 3 exclusions, 2 deduplications, 61 pending; seven authored native-validated checks, canonical saved lint 7/0. All examples reviewed but not implemented. See TASK_IMPLEMENTATION_BATCH3.md. No Docker calibration or final tolerance approval.

# Official-test survey status

The formal pipeline survey enumerates all 116 official pytest files (758 collected nodes retained in `official_nodes`) and all 25 top-level official examples for whole-module scope `.`.

As of 2026-09-17 01:40 EDT, 32 rows have substantive source/observable/runtime decisions and are marked suitable. The other 109 rows remain provisional pending review; their `suitable: false` values must not be presented as final exclusions or as coverage by an existing check. See `TASK_SURVEY_PROGRESS.md` for evidence, caveats, and runnable continuation steps.

The existing `proximity-distance` scaffold is an early partial implementation. Its 12-triangle/512-query default is a correctness smoke test, not yet a meaningful acceleration workload, and its tolerance remains provisional pending calibrated execution and human approval.

