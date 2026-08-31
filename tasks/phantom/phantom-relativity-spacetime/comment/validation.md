# Validation record

No Docker, Phantom build/setup/evolution/test binary, oracle solve, verifier self-test, runtime measurement, accelerator run, network operation, or external operation was executed. Docker solve/self-test and scientific accelerator calibration remain parent-owned validation gates. No `runtime-metadata.json` exists.

## Source truth and resulting scope

Pinned official commit: `e53ea16758d2a261680506852a528f21270dca1c`.

- `git -C code/phantom cat-file -p "$PIN:data/binarybh"`: **exit 0**; the pinned tree has exactly one entry, `README`.
- `git -C code/phantom show "$PIN:data/binarybh/README"`: **exit 0**; the README says the directory needs `cbwaves.txt` by default.
- `git -C code/phantom cat-file -e "$PIN:data/binarybh/cbwaves.txt"`: **exit 128 (expected absence)**.

Therefore the task-owned synthetic trajectory and its entire `tests/checks/binarybh/` check were removed rather than treated as official upstream input. No replacement or count-filler was added. The surviving ordered authority is:

1. `grtde`
2. `collgr`
3. `srpolytrope`
5. `grbondi-inject`
6. `srshock`
7. `gr-testparticles`
8. `srblast`
9. `grstar`
10. `testgr`
11. `flrw`

The scorer remains dynamically equal-weighted as `passed / len(checks)`; there is no hard-filled check and no per-check weight field. Coverage is five evolve checks, four setup checks, and one upstream-test check.

## Canonical registry generation

- `node scripts/gen-index.mjs`: **exit 0** (`registry/index.yaml`: 9 tasks; `registry.json`: 1 runnable).
- Generated projection for this draft: `checks: 10`, `targets: 1`, `cells: 10`.
- `node scripts/gen-index.mjs --check`: **exit 0** (`registry files up to date`).

## No-Docker static/repository gates

All final-state gates below passed:

- `bash -n tasks/phantom/phantom-relativity-spacetime/tests/test.sh tasks/phantom/phantom-relativity-spacetime/tests/run-check.sh tasks/phantom/phantom-relativity-spacetime/solution/solve.sh tasks/phantom/phantom-relativity-spacetime/tests/checks/*/run.sh`: **exit 0** (14 scripts).
- Inline Python parse gate (`ast.parse` every task Python file, `json.loads` every task JSON file, `tomllib.loads` `task.toml`): **exit 0** (16 Python, 34 JSON, 1 TOML).
- Inline consistency/source audit: **exit 0**. It verified all four executable inventories have the exact same 10 IDs and order; rows/headings are contiguous; package directory set matches; mode counts are 6/4/1; exactly one `acceleration` label exists; each setup is in pinned `build/Makefile_setups`; all 40 rubric-owned source paths exist at the pin; `binarybh` cannot be regenerated; target/Docker/count/score claims agree; and every `check.json` contains labels only (no weight override).
- `python3 skills/package-sciaccel-task/scripts/validate-harbor-task.py tasks/phantom/phantom-relativity-spacetime`: **exit 0** (`1 active target`).
- `python3 skills/package-sciaccel-task/scripts/validate-harbor-task.py --all tasks`: **exit 0** (all 9 discovered Harbor leaves passed).
- `npm run check:validators`: **exit 0** (`1 checks, every validator discriminates`). This repository command exercises the registered synthetic fixture validator, not this leaf's unrun numerical oracle.
- `BASE_REF=origin/main npm run check`: **exit 0** (`16 tasks, no violations`; all Harbor leaves passed).
- `git diff --check` plus `git diff --no-index --check /dev/null <file>` for every path in `comment/changed-files.txt`, with the expected clean-difference exit 1 accepted only when diagnostic output is empty: **exit 0** (84 task files).

The first strict consistency-audit attempt exited 1 because `grbondi-inject/rubric.json` recorded `src/setup/setup_bondiinject.F90`, while the pinned Git tree stores `src/setup/setup_bondiinject.f90` (the pinned Makefile spells it uppercase, which works on the existing case-insensitive checkout). The rubric and retained generator were corrected to the exact pinned-tree path; the complete audit then exited 0. A preliminary whitespace wrapper also exited 1 with no whitespace diagnostic because it incorrectly treated the normal `git diff --no-index` “files differ” status as a content failure; the corrected wrapper described above exited 0.
