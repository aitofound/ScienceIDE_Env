# PR463 publication reconciliation (2026-09-09)

This is additive, non-scoring reconciliation metadata for the already published original-run evidence. It does not alter task inputs, solver/source identity, rubric criteria, scientific bounds, executed fingerprints, timestamps, budget, or cache semantics.

## Outcome

- **Publication promotion:** held; no generated record was promoted.
- **Reason for hold:** CI run `34413751245` (head `a3513982ab9f434e2274c00a3f2878f34f1e918b`) failed only at the `self-validation freshness` step in job `102673843366`. The log says the recorded run ending `2026-09-09T22:04:24Z` is stale against the contract files. The run's status JSON reports 13 checks, lint 0 errors/warnings, recorded self-validation `passed`, `budget=exceeded`, `fresh=false`, `generated_files=[]`, and local consent `null`.
- This is execution/publication freshness metadata, not evidence that the solver, verifier, graders, or accepted scientific observations failed. It is nevertheless retained honestly as a CI failure; it is not erased or relabeled.
- A static `sab.py status --task tasks/batsrus/batsrus-solar-corona-awsom --ci-freshness` reproduction returned `fresh=false` (nonzero). The current contract fingerprint is `c73dcb4bd8cfe79bc4da9b2f9b257e915999a2bcf2a1b4571335ec1639161832`; the immutable official run fingerprint remains `dcc419dc11912f5292ec6b3c618298c6d660465c312cec70bc379d34e0ff3b05`. The local consent mismatch is unrelated and was not repaired.

## Equivalence and preservation checks

The official workspace and the PR463 publication clone were compared without running science or changing files. The staged official task workspace has 111 files / 2,962,137 bytes; the clone has 296 files / 5,337,544 bytes. 94 non-rubric/non-pipeline files and 107 non-pipeline files were exact; all 13 rubric JSON files were exact. Selected task and execution inputs were exact, including:

- `task.toml`: `f7902fb4d33ed2f443ae9e556570f0f2b3e95bc1a5622f3483f47f6483c8a2c0`
- `instruction.md`: `4f46d18af5d9a0f3f47d66c0426258cdcc0c9f478b3c390c25289d51d2b67b05`
- `solve.sh`: `4be6db6d5ff4590b5b3a0aac91e8fb43d16c4c71c68d9356660791c047e0881b`
- `environment/Dockerfile`: `e0cf34125645f7fa8dca6a7c88b31dd98f5edda6608c1b66c5dffb7868908aec`
- `tests/Dockerfile`: `22c82a8019d7b99b69e7cbf1f8eabfd6a4a303db355b25bbb1d2e1f694cc2ae2`
- `tests/test.sh`: `c54eb93808418d00e19f321f543fa940532ba65837832489a6d5aba542d76b6a`

The pinned current-main writer audit found the expected writer/tool objects (`taskcmds.py` `8061a...`, `sab.py` `95613...`, utility `2f4d...`), and the 13 validators read the comparison tolerance/file fields while `tests/test.sh` invokes and aggregates them. No changed non-rubric or rubric bytes were found that are safe to promote. The preserved pre/run records also contain a different stale fingerprint (`c03b93b09...`) under the disabled-cache/freshness context; this is not safe to rewrite as an executed fingerprint.

## Original record and receipt preservation

The original official publication remains byte-preserved, including active canonical runtime SHA `498020920c4d6601cce4bf484353ba1d37605d58a62f20b960e9bed6888651fe`, active self-validation SHA `a8b6f4910e3a9c693cb82e4d3ef6681888796996c6c526ca95e0675ca3b7c69a`, and historical runtime/self-validation SHAs `a0d31164c619ff45ce4ba38b944c9f9cafdc577221290a0f7ad75dda7edc1f3f` and `eceaf7149ce4d72fd7515b29ef53206f7e8fcceadeccd0a818227f0b98ce476e`. Receipts, copied hashes, markers, and the 165 large-output inventory-only entries (453,603,517 bytes) are unchanged. The accepted original facts remain 13 NVA/reward 1/verifier 0, 39 real markers, and 167 copied hashes. Preserve `budget=exceeded` (`1736.6s` nominal excluding `924.0s` builds versus 1500 guidance) and cache-disabled truth.

No source repair, budget increase, rerun, Docker/MPI/solver/self-check/verifier/calibration, or raw-output copying was performed. The PR body was not rewritten; its existing attribution/prefix is preserved.
