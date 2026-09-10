# PR453 exact official validation collection — 2026-09-09 — em-5980

**Result: FULL REQUESTED VALIDATION EVIDENCE PASS — 7/7 checks, reward 1.0, self-validation `passed`.**

This is collection only. This worker did not run build, Docker, solver, selfcheck, verifier, calibration, retry, relaunch, or any new scientific execution. The immutable worker receipt field `science_result=not_claimed_launch_handoff_only` was not treated as proof by itself; the generated reward/self-validation rows and raw-evidence audit below were reconciled.

## Exact run and inputs

- Official receipt: `/home/huangzesen/.sciaccel_pipeline/batsrus/launch-receipts/sab-pr453-repair-20260909-em-41ba/launch-receipt.json` (real JSON, `official_attempt=true`, status `official_selfcheck_completed`, build/selfcheck exit `0/0`; copied hash match `True`).
- Runroot: `/home/huangzesen/.sciaccel_pipeline/batsrus/runs/batsrus-outer-heliosphere/repair-20260909-em-41ba-official`; stage/task: `/home/huangzesen/.sciaccel_pipeline/batsrus/stages/sab-pr453-official-20260909-em-41ba` / `/home/huangzesen/.sciaccel_pipeline/batsrus/stages/sab-pr453-official-20260909-em-41ba/tasks/batsrus/batsrus-outer-heliosphere`.
- Time: started `2026-09-09T20:13:30Z`; receipt updated `2026-09-09T21:02:59Z`; self-validation `2026-09-09T20:14:51Z`–`2026-09-09T21:02:59Z`.
- Source: PR453 head `b7b53c64af6666f9548ff76bc075114b221c50d6`; BATSRUS source `9dfe746d48aa650b5209c3039f1a8676bc624899`; immutable CLI execution origin-main record `25823f1add2a1fbd9e14bab4550a1a2428095721`. Captured-main scripts-tree provenance remains `ec1606a4` and was not relabeled.
- Resources: 4 CPU, 8.0 GB, 7 checks, declared windows `[145, 2, 23, 47, 83, 51, 2]`, suite `353 s`, network `disabled`.

Commands recorded in the receipt/self-validation (not re-run here):

```text
build: cd /home/huangzesen/.sciaccel_pipeline/batsrus/stages/sab-pr453-official-20260909-em-41ba && PATH=/home/huangzesen/bin:$PATH python3 /home/huangzesen/.sciaccel_pipeline/batsrus/stages/sab-pr453-official-20260909-em-41ba/skills/package-sciaccel-task/scripts/sab.py task build --task /home/huangzesen/.sciaccel_pipeline/batsrus/stages/sab-pr453-official-20260909-em-41ba/tasks/batsrus/batsrus-outer-heliosphere --which both
selfcheck: cd /home/huangzesen/.sciaccel_pipeline/batsrus/stages/sab-pr453-official-20260909-em-41ba && PATH=/home/huangzesen/bin:$PATH python3 /home/huangzesen/.sciaccel_pipeline/batsrus/stages/sab-pr453-official-20260909-em-41ba/skills/package-sciaccel-task/scripts/sab.py task selfcheck --task /home/huangzesen/.sciaccel_pipeline/batsrus/stages/sab-pr453-official-20260909-em-41ba/tasks/batsrus/batsrus-outer-heliosphere --run-root /home/huangzesen/.sciaccel_pipeline/batsrus/runs/batsrus-outer-heliosphere/repair-20260909-em-41ba-official
solve_nominal: SAB_IC=nominal ./solution/solve.sh
solve_variant: SAB_IC=variant ./solution/solve.sh
solve_altbuild: SAB_IC=altbuild ./solution/solve.sh
verifier: ./tests/test.sh
```

## Actual check schema audit

The exact staged task yielded **7** `tests/checks/*/check.json` files, with these names: `outerhelio, outerhelio-1d, outerhelio2d, outerhelioawsom, outerhelioawsom-restart, outerheliopui, outerheliopui-1d`. Every file was parsed and copied. The actual schema is `['labels']`: `labels` only; `outerhelio` labels `['acceleration']`, the other six labels `[]`. No `expectedcheck.json` exists in the bounded task search and none was invented or assumed.

## Seven-check row reconciliation

| check | labels | graded files | reward row (P / distance / bound fraction / rtol) | altbuild row (P / distance / bound fraction / identical / graded-identical) | evidence N/V/A |
|---|---|---|---|---|---|
| `outerhelio` | `['acceleration']` | `final_y0_mhd.out, final_y0_var.out, log.log` | True / `5.000001124244591e-08` / `3.735969276067536e-05` / `0.0001` | True / `1.0000036354540498e-09` / `1.426378165861291e-07` / `False` / `False` | N: 3 files, nonempty/substantive, run.ok, no failed/skipped; V: 3 files, nonempty/substantive, run.ok, no failed/skipped; A: 3 files, nonempty/substantive, run.ok, no failed/skipped |
| `outerhelio-1d` | `[]` | `final_y0_var.out, log.log` | True / `2.999996695507434e-09` / `2.796243940873101e-06` / `0.0001` | True / `0.0` / `0.0` / `True` / `False` | N: 2 files, nonempty/substantive, run.ok, no failed/skipped; V: 2 files, nonempty/substantive, run.ok, no failed/skipped; A: 2 files, nonempty/substantive, run.ok, no failed/skipped |
| `outerhelio2d` | `[]` | `final_z0_hd.out, interpolated_output.dat, log.log` | True / `2.059999985704053e-06` / `9.603022463007339e-05` / `0.0001` | True / `2.069999993636884e-06` / `9.595667582620532e-05` / `False` / `False` | N: 3 files, nonempty/substantive, run.ok, no failed/skipped; V: 3 files, nonempty/substantive, run.ok, no failed/skipped; A: 3 files, nonempty/substantive, run.ok, no failed/skipped |
| `outerhelioawsom` | `[]` | `final_y0_mhd.out, final_y0_var.out, log.log` | True / `0.0010000001639127731` / `5.078941734606777e-06` / `0.0003` | True / `1.0799999783372982e-09` / `4.744800806608244e-06` / `False` / `False` | N: 3 files, nonempty/substantive, run.ok, no failed/skipped; V: 3 files, nonempty/substantive, run.ok, no failed/skipped; A: 3 files, nonempty/substantive, run.ok, no failed/skipped |
| `outerhelioawsom-restart` | `[]` | `final_1d_var.out, log.log` | True / `0.0010000020265579224` / `4.361160474893128e-06` / `0.0003` | True / `9.99999322030669e-21` / `8.551298744398081e-08` / `False` / `False` | N: 2 files, nonempty/substantive, run.ok, no failed/skipped; V: 2 files, nonempty/substantive, run.ok, no failed/skipped; A: 2 files, nonempty/substantive, run.ok, no failed/skipped |
| `outerheliopui` | `[]` | `final_y0_mhd.out, final_y0_var.out, log.log` | True / `2.0000015865662135e-08` / `3.077921500282243e-06` / `0.0001` | True / `1.000000082740371e-11` / `4.69514549317867e-08` / `False` / `False` | N: 3 files, nonempty/substantive, run.ok, no failed/skipped; V: 3 files, nonempty/substantive, run.ok, no failed/skipped; A: 3 files, nonempty/substantive, run.ok, no failed/skipped |
| `outerheliopui-1d` | `[]` | `final_y0_var.out, log.log` | True / `9.999999999940612e-08` / `0.08324384619817539` / `0.0001` | True / `0.0` / `0.0` / `True` / `False` | N: 2 files, nonempty/substantive, run.ok, no failed/skipped; V: 2 files, nonempty/substantive, run.ok, no failed/skipped; A: 2 files, nonempty/substantive, run.ok, no failed/skipped |

Reward rows in `reward.json` and `self-validation.json.reward.checks` are semantically equal for all seven checks. Every reward row is `passed=true`, `status=passed`, `reason=all graded values within bound`, reward `1.0`. Variant/reference roots are distinct per verifier contract; every check has at least one differing nominal-vs-variant raw-file hash (see JSON). Altbuild is declared and passed for all seven; `not_declared=[]`.

## Aggregate gates and evidence

- Nominal/variant/altbuild: every generated expected raw file is present, nonempty, and has substantive numeric evidence; every mode/check has nonempty `run.ok`; no `run.failed` or `run.skipped` marker. Raw outputs were not copied; their remote paths, byte sizes, SHA-256 hashes, line/numeric-token/substantive statistics are in `workspace/sab-pr453-collection-20260909-em-5980/remote-audit.json` and the report JSON.
- Exact result-directory audit passed for all 21 mode/check combinations: each contains exactly its graded files plus `run.ok` and `run.log`; no unexpected or missing result artifact (`workspace/sab-pr453-collection-20260909-em-5980/result-directory-audit.json`).
- Solves: nominal/variant/altbuild exit `0`; verifier `./tests/test.sh` exit `0`; `reward.total=7`, `passed=7`, reward `1.0`, outcome `all_passed`; self-validation `result=passed`, problems/warnings empty; budget `within`.
- Seven altbuild rows: all `passed=true`; declared list exactly equals the seven actual names; no undeclared checks. Variant behavior: each check has differing raw evidence hashes, while validator rows pass.
- Bounded copied evidence: 76 files, 176,592 bytes; SHA-256 and byte-size comparison against remote originals: **76/76 match**.
- Broad log scan found 21 literal `skip` words only in Debian `update-alternatives` package-man-page warnings in `build.log`; all runtime logs have zero skip/fallback hits. This is retained as an advisory, not a science skip.

## Claim limits / missing gates

- No new scientific execution was performed or authorized in this collection lane; wrapper/CLI absence at the prior verification time is not re-probed or altered.
- This report does not claim GitHub merge/status, independent reproduction, or physical/upstream correctness beyond the official generated task validator.
- Worker completion status is not science proof. Exact receipt, self-validation, reward, seven altbuild rows, and remote raw evidence hashes are the proof bundle.

## Additive artifacts

- `workspace/sab-pr453-collection-20260909-em-5980/remote-receipt/` — receipt and bounded build/selfcheck/terminal logs.
- `workspace/sab-pr453-collection-20260909-em-5980/runroot/` — reward/self-validation/solver/verifier logs and seven altbuild row JSONs.
- `workspace/sab-pr453-collection-20260909-em-5980/task-checks/` — all seven exact staged `check.json` files.
- `workspace/sab-pr453-collection-20260909-em-5980/oracles/` — bounded oracle manifests/build/run logs and run markers; raw science outputs remain remote.
- `workspace/sab-pr453-collection-20260909-em-5980/remote-audit.json` and `local-validation.json` — detailed schemas, remote raw-output hashes/stats, gates, and 76 copied-file hash comparisons.
- `reports/sab-pr453-collection-20260909-em-5980.{md,json}` — this report.
