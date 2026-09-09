# ScienceAccelBench PR463 original-run collection — 2026-09-09 (em759c)

## Collection result

**Evidence audit complete.** This is the already-authorized original PR463 run only; no computation was launched or repeated during collection. The remote terminal exited `0`, official self-validation recorded `result=passed`, reward `1.0`, and the independent 13-row audit found no missing, inactive, failed, skipped, disabled, or fallback check. Parent owns final validation/publication.

- PR #463, `batsrus/solar-corona-awsom`, task `tasks/batsrus/batsrus-solar-corona-awsom`; exact head `71b554ec8e5c5146deba5030a1b40d49fd68317b` (base `origin/main` `25823f1add2a1fbd9e14bab4550a1a2428095721`).
- Approved remote: `huangzesen@136.114.2.6` / `ale-worker.us-central1-c.c.light-result-467615-p0.internal`; run base `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353`.
- Expected checks: **13**; independently audited: **13**; missing: `[]`; inactive: `[]`; failed: `[]`.
- Recorded reward: **1.0** (`13/13`, outcome `all_passed`); reward is preserved as `null` if unavailable.

## Exact executed identity and resources

| item | recorded value | source locator |
|---|---|---|
| head / base | `71b554ec8e5c5146deba5030a1b40d49fd68317b` / `25823f1add2a1fbd9e14bab4550a1a2428095721` | `reports/sab-pr463-launch-20260909-em7353.json` |
| source pin / tree fingerprint | `9dfe746d48aa650b5209c3039f1a8676bc624899` / `452b3eb778286cd87a7fd100eeee3ccf46975c5dd52304c52d23145107837347` | launch receipt `source` |
| CLI | skill `5.11.10`, commit `25823f1add2a1fbd9e14bab4550a1a2428095721`, blob `9d5b0c78c844b644b4fad618a4f368de7d92f8df`, SHA-256 `95613ad3b2ac7eaa4681b3c44dd37e41c44c050c5fb04f320d13494f200119d5` | launch receipt `skill_cli` |
| plan | `c73dcb4bd8cf`; 8 CPU, 16.0 GB, 13 checks, network `disabled` | launch receipt `plan.current` |
| contracts | task.toml `f7902fb4d33ed2f443ae9e556570f0f2b3e95bc1a5622f3483f47f6483c8a2c0`; target `fec36b64e17d0893720e55b74a78c61d4e0f5cfc796322ff92f1a3b04a53c132`; run contract `dcc419dc11912f5292ec6b3c618298c6d660465c312cec70bc379d34e0ff3b05` | launch receipt and copied pipeline self-validation |
| host | `ale-worker.us-central1-c.c.light-result-467615-p0.internal`, `Linux 6.17.0-1022-gcp`, `x86_64`, host 88 CPUs | launch receipt / runtime metadata |
| images | oracle `sha256:d02eee86875181b8df6fdf1fdd5684d758ec8ad9e9039101db8dec891b0b8030`; environment `sha256:15cf0a152ff6e284c122df9efb783f0c26df89fdaf83e74af36e11cfb4bb83f` | launch receipt `task_build.images` |

Declared original windows (seconds, expected order): `124,19,106,389,141,17,34,63,1,1,1,1,224`; declared suite excluding builds `1121`, guidance budget `1500` seconds.

## Execution, exits, and time locators

- Official command: `/usr/bin/python3 skills/package-sciaccel-task/scripts/sab.py task selfcheck --task tasks/batsrus/batsrus-solar-corona-awsom --run-root /mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/runroot-selfcheck-20260909-em7353`; wrapper PID `2158549`, CLI PID `2158554`.
- Actual official start/finish: `2026-09-09T19:52:37Z` → `2026-09-09T22:04:24Z`; terminal receipt `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/terminal.exit`.
- Required build command exited `0` (`2026-09-09T19:47:19Z` → `2026-09-09T19:48:48Z`), log `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/logs/task-build.log`.
- Real verifier: `./tests/test.sh` exited `0` (`2026-09-09T21:15:07Z` → `2026-09-09T21:15:13Z`, 5.487 s), log `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/runroot-selfcheck-20260909-em7353/test.log`.
- `nominal` solve: exit `0`, `2026-09-09T19:52:37Z` → `2026-09-09T20:37:10Z`, elapsed `2672.968` s; runroot `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/runroot-selfcheck-20260909-em7353/oracle-nominal`; oracle image ID `sha256:d02eee86875181b8df6fdf1fdd5684d758ec8ad9e9039101db8dec891b0b8030`.
- `variant` solve: exit `0`, `2026-09-09T20:37:10Z` → `2026-09-09T21:15:07Z`, elapsed `2276.987` s; runroot `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/runroot-selfcheck-20260909-em7353/oracle-variant`; oracle image ID `sha256:b826cb4d2cf2a293be598bae62c8de6507946ae8e274229bba6755a6f8c3f7fc`.
- `altbuild` solve: exit `0`, `2026-09-09T21:15:13Z` → `2026-09-09T22:04:19Z`, elapsed `2946.039` s; runroot `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/runroot-selfcheck-20260909-em7353/oracle-altbuild`; oracle image ID `sha256:ae32ed26462a1b9a85c063c76127d8940ffe44aa175335c4d9e88a8976f387d6`.
- Official log ends `OFFICIAL_WRAPPER_END=2026-09-09T22:04:24Z EXIT=0`. It contains `SELF-VALIDATION PASSED: 13 checks, reward 1.0` and one explicit budget warning.

## Independent 13-check audit (N / V / A)

N = nominal, V = variant, A = required alternative build. Every row is backed by physical `run.ok` and `run.log` for all three initials, actual remote result inventory/hash, verifier PASS line, and altbuild JSON.

| check | reward/status | N/V/A markers | result files N/V/A | numeric bytes / tokens N/V/A | verifier / altbuild | audit |
|---|---|---|---:|---:|---|---|
| `awsom-bvector` | `1.0` / `passed` | Y / Y / Y | 5 / 5 / 5 | 3697189 / 204966 / 3697189 / 204965 / 3697190 / 204965 | `PASS` / `PASS` | **PASS** |
| `awsom-gpu` | `1.0` / `passed` | Y / Y / Y | 5 / 5 / 5 | 19368020 / 1074005 / 19368020 / 1074005 / 19368021 / 1074005 | `PASS` / `PASS` | **PASS** |
| `awsom-large-gpu` | `1.0` / `passed` | Y / Y / Y | 4 / 4 / 4 | 34897956 / 1932493 / 34897956 / 1932493 / 34897957 / 1932493 | `PASS` / `PASS` | **PASS** |
| `awsom-signb` | `1.0` / `passed` | Y / Y / Y | 5 / 5 / 5 | 18264970 / 1012127 / 18264970 / 1012127 / 18264971 / 1012127 | `PASS` / `PASS` | **PASS** |
| `awsom` | `1.0` / `passed` | Y / Y / Y | 5 / 5 / 5 | 4804168 / 266139 / 4804168 / 266139 / 4804168 / 266139 | `PASS` / `PASS` | **PASS** |
| `awsomr` | `1.0` / `passed` | Y / Y / Y | 5 / 5 / 5 | 2565763 / 142267 / 2565763 / 142267 / 2565764 / 142267 | `PASS` / `PASS` | **PASS** |
| `ex-corona-1dwedge` | `1.0` / `passed` | Y / Y / Y | 4 / 4 / 4 | 354488 / 21967 / 354488 / 21967 / 354489 / 21967 | `PASS` / `PASS` | **PASS** |
| `ex-corona-2dwedge` | `1.0` / `passed` | Y / Y / Y | 4 / 4 / 4 | 37730493 / 2089374 / 37730493 / 2089374 / 37730494 / 2089374 | `PASS` / `PASS` | **PASS** |
| `magnetogram-fdips-wedge` | `1.0` / `passed` | Y / Y / Y | 3 / 3 / 3 | 1030483 / 56715 / 1030483 / 56715 / 1030482 / 56715 | `PASS` / `PASS` | **PASS** |
| `magnetogram-fdips` | `1.0` / `passed` | Y / Y / Y | 4 / 4 / 4 | 3269382 / 179958 / 3269382 / 179958 / 3269381 / 179958 | `PASS` / `PASS` | **PASS** |
| `magnetogram-harmonics` | `1.0` / `passed` | Y / Y / Y | 3 / 3 / 3 | 3704 / 278 / 3704 / 278 / 3703 / 278 | `PASS` / `PASS` | **PASS** |
| `magnetogram-potential` | `1.0` / `passed` | Y / Y / Y | 3 / 3 / 3 | 961826 / 52935 / 961826 / 52935 / 961825 / 52935 | `PASS` / `PASS` | **PASS** |
| `stitch` | `1.0` / `passed` | Y / Y / Y | 5 / 5 / 5 | 24252729 / 1344585 / 24252729 / 1344585 / 24252730 / 1344585 | `PASS` / `PASS` | **PASS** |

Per-check conclusions:
- **`awsom-bvector`:** reward `1.0`, policy `pointwise`, distance `2.6353690698950882e-11`, bound fraction `3.5872184202650566e-07`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `2`; altbuild passed `True` (graded-identical `None`).
- **`awsom-gpu`:** reward `1.0`, policy `pointwise`, distance `7.194081280429483e-11`, bound fraction `1.8710311222127154e-07`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `3`; altbuild passed `True` (graded-identical `None`).
- **`awsom-large-gpu`:** reward `1.0`, policy `pointwise`, distance `4.991199265773049e-12`, bound fraction `4.6181210475791805e-06`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `4`; altbuild passed `True` (graded-identical `None`).
- **`awsom-signb`:** reward `1.0`, policy `pointwise`, distance `4.8516109968908285e-11`, bound fraction `3.1109806718030705e-05`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `5`; altbuild passed `True` (graded-identical `None`).
- **`awsom`:** reward `1.0`, policy `pointwise`, distance `6.357214175705476e-11`, bound fraction `3.7755914130069546e-05`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `1`; altbuild passed `True` (graded-identical `None`).
- **`awsomr`:** reward `1.0`, policy `pointwise`, distance `7.674712589554732e-11`, bound fraction `7.212647845075739e-05`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `6`; altbuild passed `True` (graded-identical `None`).
- **`ex-corona-1dwedge`:** reward `1.0`, policy `pointwise`, distance `3.0830702445843406e-13`, bound fraction `3.0830698957571174e-09`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `7`; altbuild passed `True` (graded-identical `None`).
- **`ex-corona-2dwedge`:** reward `1.0`, policy `pointwise`, distance `3.8491199519713386e-11`, bound fraction `2.330091160583692e-05`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `8`; altbuild passed `True` (graded-identical `None`).
- **`magnetogram-fdips-wedge`:** reward `1.0`, policy `pointwise`, distance `3.408470485417687e-06`, bound fraction `0.0018203653118744158`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `10`; altbuild passed `True` (graded-identical `None`).
- **`magnetogram-fdips`:** reward `1.0`, policy `pointwise`, distance `2.0985010227106104e-10`, bound fraction `0.0001102513338069578`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `9`; altbuild passed `True` (graded-identical `None`).
- **`magnetogram-harmonics`:** reward `1.0`, policy `pointwise`, distance `2.2918892353946595e-09`, bound fraction `0.0011459446176973298`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `11`; altbuild passed `True` (graded-identical `None`).
- **`magnetogram-potential`:** reward `1.0`, policy `pointwise`, distance `3.3970767820996097e-06`, bound fraction `0.0016985383912053657`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `12`; altbuild passed `True` (graded-identical `None`).
- **`stitch`:** reward `1.0`, policy `pointwise`, distance `4.082843135725844e-11`, bound fraction `1.1997728059215713e-06`; all declared N/V/A outputs are nonempty and numeric; N/V/A markers present; verifier PASS line `13`; altbuild passed `True` (graded-identical `None`).

Exact per-check marker text, result names, SHA-256s, sizes, numeric counts, and first/last numeric lines are retained in the fresh workspace JSONL artifacts and copied marker/log files; no large scientific output was duplicated locally.

## Reward/self-validation schema audit

- Reward top keys: `candidate_dir, checks, identical_checks, outcome, passed, reference_dir, reward, status, total`; actual checks dictionary exact set of 13 names, `total=13`, `passed=13`, `status=scored`, `outcome=all_passed`; embedded reward equals standalone: `True`.
- Self-validation top keys: `altbuild, budget, budget_s, build_seconds_nominal, check_run_seconds_nominal, checks, consent, contract_fingerprint, finished_at, host, knob_overrides, problems, resources, result, reward, solves, started_at, suite_seconds_nominal, task, verifier, warnings`; `result=passed`, `problems=[]`, `warnings=['suite run time 1737s on the nominal solve (builds 924s excluded), above the 1500s budget with 88 cores; the budget is guidance: agree the strategy with the human (raise suite_budget_s, shorten windows, more cores), never drop checks']`.
- N/V/A declaration: checks list exact set `True`; altbuild declared all 13 `True`; not declared `[]`.
- Verifier log `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/runroot-selfcheck-20260909-em7353/test.log`: `13` PASS lines; exact expected set `True`.

## Large-output inventory (remote-only)

- Remote result payloads: **165 files / 453603517 bytes**; all nonempty `True` and all numeric `True`; `127` have multiple numeric lines. Hashed/content-inspected remotely, not copied.
- Hash manifest: `workspace/sab-pr463-collection-20260909-em759c/large-output-manifest.jsonl`; bounded numeric-content audit: `workspace/sab-pr463-collection-20260909-em759c/large-output-content-audit.jsonl`; remote paths remain under `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353/runroot-selfcheck-20260909-em7353/oracle-{nominal,variant,altbuild}/results/...`.

## Exact receipt hash verification

- **167/167** selected modest receipts copied into `workspace/sab-pr463-collection-20260909-em759c/remote/` match remote SHA-256 byte-for-byte; zero mismatches and zero extras.
- Selected receipts include exits, wrappers/logs, pipeline consent/comment metadata, reward/self-validation JSON, solve/verifier logs, 13 altbuild JSONs, 3 oracle manifests, and all 117 per-check N/V/A `run.log`, `run.ok`, and numerical `log.log` files where produced.
- Remote hash manifest: `workspace/sab-pr463-collection-20260909-em759c/remote-hashes.sha256`; comparison: `workspace/sab-pr463-collection-20260909-em759c/hash-compare.txt`.

## Caveat and boundary

- Nominal suite runtime `1736.6` s excluding `924.0` s builds, above `1500.0` s guidance budget; no check was dropped.
- Every run log contains `SAB_BUILD_CACHE=disabled reason=missing solve-scoped source fingerprint or cache root`. This is the producer cache-mode marker only; independent audit found no check-level skip/disable/fallback marker. No cache-reuse or speedup claim is made.
- No source/task/PR edits, commit/push, cleanup, rerun, build invocation, science invocation, Docker/MPI/solver/verifier/selfcheck invocation, or external contact occurred during collection.

## Fresh local artifacts

- Reports: `reports/sab-pr463-collection-20260909-em759c.md` and `.json`.
- Fresh workspace: `workspace/sab-pr463-collection-20260909-em759c/`; exact modest receipts under `remote/`; large outputs inventory/hash-only.
- Exact identity source: `reports/sab-pr463-launch-20260909-em7353.md` and `.json` (preserved; used only to recover executed identity and inputs).
