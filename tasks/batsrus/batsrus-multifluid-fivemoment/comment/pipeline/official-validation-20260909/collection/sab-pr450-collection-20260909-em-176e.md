# PR450 collection-only final evidence audit

- **Audit:** `sab-pr450-collection-20260909-em-176e`
- **Status:** **FULL_SCIENCE_PASS**
- **Scope:** collection/audit only; this lane did not relaunch, build, solve, selfcheck, run a verifier, retry, or mutate the official run.
- **Publication boundary:** no Git/GitHub writes were performed.

## Exact official run and immutable receipt

- Task: `batsrus-multifluid-fivemoment`; attempt: `authorized-original-science-attempt`.
- PR 450: `OPEN`, `batsrus/multifluid-fivemoment`, head `9ada1287b9608b5402dafb01d37b7556f3b08df3` (read-only GH check matched the staged head; captured in `provenance/gh-pr-head.txt`).
- Remote: `huangzesen@136.114.2.6`; hostname `ale-worker.us-central1-c.c.light-result-467615-p0.internal`; run root `/home/huangzesen/sab-pr450-validation-20260909-em-828e/runroot`; pipeline `/home/huangzesen/sab-pr450-validation-20260909-em-828e/pipeline`.
- Terminal receipt: `/home/huangzesen/sab-pr450-validation-20260909-em-828e/official-terminal-exit-corrected.json`; local `provenance/official-terminal-exit-corrected.json`; SHA-256 `2a55c07e818dc90c8149165369bfa6eeb89081b72a398aa481fccf0b38040d56`.
- Receipt: started `2026-09-09T19:49:14Z`, finished `2026-09-09T20:46:59Z`, `build_rc=0`, `selfcheck_rc=0`, `exit_code=0`.
- Official supervisor log: `/home/huangzesen/sab-pr450-validation-20260909-em-828e/official-supervisor-corrected.log` (SHA-256 `7a8fba8f94b7ec3b05cc309fccefa67a2c5c32989cb48f34c297d10d9f0c79ba`); wrapper: `/home/huangzesen/sab-pr450-validation-20260909-em-828e/remote-supervisor-corrected.sh` (SHA-256 `abe57f6c827a0bfa7702bf0a8c36fdfba2226ec92cef0aabf1d55b9a600f2a23`).
- Receipt commands are exactly one build then one selfcheck; the copied log has one `BUILD_COMMAND`, one `SELFCHECK_COMMAND`, one `BUILD_RC 0`, and one `SELFCHECK_RC 0`.

## Exact execution input / provenance mapping

- Source commit `9dfe746d48aa650b5209c3039f1a8676bc624899`; source tree `2b7ceffe897cc21572847d7aed65d46eeddb4c40`; `origin_main` pin `25823f1add2a1fbd9e14bab4550a1a2428095721`.
- Run-staged packaging skill: version `5.11.10`, blob `9d5b0c78c844b644b4fad618a4f368de7d92f8df`, SHA-256 `56f0445751beb2b46e70703c15d97ea0a459e0dabf9c11388ccd3be03ce56c18`. Run-staged CLI SHA-256 `95613ad3b2ac7eaa4681b3c44dd37e41c44c050c5fb04f320d13494f200119d5` (remote ledger independently records `95613ad3b2ac7eaa4681b3c44dd37e41c44c050c5fb04f320d13494f200119d5`). Staged skill SHA-256 ledger: `56f0445751beb2b46e70703c15d97ea0a459e0dabf9c11388ccd3be03ce56c18`.
- Launch handoff contract fingerprint: `0867917ad3c4`; immutable official self-validation contract fingerprint: `c37174f562ffa629358fc2fcbd978310448eb6105d453407997167916d2b6e5c`. These are recorded separately; no fingerprint was refreshed or hand-authored.
- Consent record: `local`, `consented_on=ale-worker.us-central1-c.c.light-result-467615-p0.internal`, at `2026-09-09T19:48:08Z`, literal executed `human_ref=exact6874ref`.
- Additive authority mapping (record not rewritten): actual message `zhipu-1:6859932159:6874` at `2026-09-09T03:32:43Z`, words “should also rerun”; worker’s literal executed ref remains `exact6874ref`.
- Declared resources: `16` CPUs, `16.0` GB, `900.0` s guidance, `432.0` s declared runtime sum, `14` checks. Official host: `x86_64`, `88` host CPUs, Docker `29.1.3`; each solve used `16` CPUs / `16.0` GB.
- Immutable input manifest: `provenance/preflight-manifest.json`; staged task root `/home/huangzesen/sab-pr450-validation-20260909-em-828e/tasks/batsrus/batsrus-multifluid-fivemoment`.

## Expected denominator

- Derived directly from actual staged `tests/checks/*/check.json` directories, not from an invented expected-check file: **14/14**.
- Exact set: `ex-fivemoment-light`, `ex-gemreconnection-sixmoment`, `ex-shocktube-fivemoment`, `ex-sixmoment-alfven`, `ex-sixmoment-fast`, `ex-sixmoment-light`, `ex-sixmoment-shock`, `fivemoment-alfven`, `fivemoment-langmuir`, `fivemoment-shock`, `kelvinhelmholtz-multiion`, `multifluid`, `multiion`, `twofluidmhd`.
- Set equality across actual check dirs, official self-validation, official reward, altbuild declared/results, and altbuild per-row artifacts: **True**.

## Full-denominator acceptance table

The variant column is the official CLI verifier’s nominal-reference vs variant result. `values` and `over=0` are substantive graded-file evidence, not exit-code-only claims. Altbuild is the official self-validation altbuild record plus its copied per-row artifact. `N≠V` is both the reward row’s `identical:false` and distinct remote final-output hashes.

| check | nominal solve | variant solve | official verifier (variant vs nominal) | altbuild | N≠V |
|---|---:|---:|---|---|:---:|
| `ex-fivemoment-light` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `2109`; over `0`; reason `all graded values within bound` | **PASS**; values `2109`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `ex-gemreconnection-sixmoment` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `3670019`; over `0`; reason `all graded values within bound` | **PASS**; values `3670019`; over `0`; `identical=False`; `all graded values within bound` | **yes** |
| `ex-shocktube-fivemoment` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `847`; over `0`; reason `all graded values within bound` | **PASS**; values `847`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `ex-sixmoment-alfven` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `2509`; over `0`; reason `all graded values within bound` | **PASS**; values `2509`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `ex-sixmoment-fast` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `166408`; over `0`; reason `all graded values within bound` | **PASS**; values `166408`; over `0`; `identical=False`; `all graded values within bound` | **yes** |
| `ex-sixmoment-light` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `2509`; over `0`; reason `all graded values within bound` | **PASS**; values `2509`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `ex-sixmoment-shock` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `20009`; over `0`; reason `all graded values within bound` | **PASS**; values `20009`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `fivemoment-alfven` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `1409`; over `0`; reason `all graded values within bound` | **PASS**; values `1409`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `fivemoment-langmuir` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `1059`; over `0`; reason `all graded values within bound` | **PASS**; values `1059`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `fivemoment-shock` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `16809`; over `0`; reason `all graded values within bound` | **PASS**; values `16809`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `kelvinhelmholtz-multiion` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `294917`; over `0`; reason `all graded values within bound` | **PASS**; values `294917`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `multifluid` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `4868`; over `0`; reason `all graded values within bound` | **PASS**; values `4868`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `multiion` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `4614`; over `0`; reason `all graded values within bound` | **PASS**; values `4614`; over `0`; `identical=True`; `all graded values within bound` | **yes** |
| `twofluidmhd` | exit `0`, marker nonempty | exit `0`, marker nonempty | **PASS**; reward `1.0`; policy `pointwise`; values `3588`; over `0`; reason `all graded values within bound` | **PASS**; values `3588`; over `0`; `identical=True`; `all graded values within bound` | **yes** |

- Official reward artifact: `/home/huangzesen/sab-pr450-validation-20260909-em-828e/runroot/reward.json`; local `runroot/reward.json`; SHA-256 `709ce69beccc6c1ec0968bf85bdb53dd983b1d672db628c8a66bf469740c128b`. Schema keys `candidate_dir, checks, identical_checks, outcome, passed, reference_dir, reward, status, total`; outcome `all_passed`, status `scored`, passed `14/14`, reward `1.0`, identical-check list `[] `.
- Official self-validation artifact (immutable CLI output): `/home/huangzesen/sab-pr450-validation-20260909-em-828e/runroot/self-validation.json`; local `runroot/self-validation.json`; SHA-256 `96c218e2c000efa4bec2655196d61bba420e818ccae31cec77cfa69b202b1f48`; result `passed`; verifier exit `0`; problems `[]`, warnings `[] `.
- Solve evidence: official `solves` contains nominal/variant/altbuild, each exit `0`; copied marker/log evidence is nonempty for **14+14+14** rows. Official solve logs each say `produce: all 14 checks ran`; test log has **14** official `PASS` lines.
- Altbuild: declared set **14/14**, not-declared `[]`; official self-validation says **14/14** measured (12 bit-identical, 2 nonidentical within bound). All 14 rows above have substantive per-row evidence.

## Termination and no-placeholder gate

- Read-only termination check: `provenance/termination-check.txt`. No exact PR450 supervisor/CLI process and no owned container-name match remained. The three recorded container IDs were no longer inspectable (`no such object`); no stop/remove action was taken.
- Generated evidence has no inactive expected row: all expected rows have nonempty nominal/variant/altbuild markers and `final.out` output hashes; no skipped/disabled/fallback/placeholder reward row appears. Static check scripts contain ordinary fallback/disabled branch text, which was not treated as execution evidence; actual solve logs show all 14 rows ran.
- The official corrected wrapper’s guard, one-build/one-selfcheck log markers, receipt rc values, official self-validation and reward are the acceptance evidence; observer completion/exit alone was not used.

## Timings and resource interpretation

- Measured whole official terminal flow: **57m45s (3465.0 s)** from receipt start to finish. This is not the `900` s suite guidance and not a lower-bound forecast.
- Official self-validation wall interval: `2026-09-09T19:50:33Z`–`2026-09-09T20:46:59Z` = `3386.0` s. Nominal suite metadata `311.7` s and nominal build metadata `792.0` s are recorded as generated measurements, not substituted for whole-flow wall time.
- Solve elapsed metadata: nominal `1109.961` s; variant `1232.074` s; altbuild `1033.46` s. Consent plan declares 16 CPU/16 GB/900 s guidance and 432 s runtime sum excluding build.

## Historical reconciliation

- The preflight machine-consent refusal is historical only: `provenance/preflight-failed-terminal-exit-exact.json`, build rc 1, selfcheck null, no science, run root absent, not reused.
- Older 512d evidence is historical/stale for this revision: `provenance/historical-resume-512d.json`; its preserved 14/14 result was not reused.
- Corrected run classification: `single authorized original science attempt after consent-only machine-identity correction; not a retry; executed record remains literal exact6874ref`. The literal executed record remains `exact6874ref`; the additive human-message mapping is not a rewrite.

## Collection contents and publication next step

- Collection directory: `/Users/huangzesen/work/projects/lingtai-space-research/.lingtai/zhipu-1/workspace/sab-pr450-collection-20260909-em-176e`.
- Copied small evidence: official receipt/log/wrapper, reward, immutable self-validation, runtime/module/survey/README metadata, 14 altbuild per-row verifier artifacts, 84 per-row `run.log`/`run.ok` markers, 9 oracle manifests/build/run logs, actual check inputs, and historical provenance.
- No raw `final.out` was copied. Remote ledger `remote-hashes.tsv` retains exact remote paths and SHA-256 for **42** raw final outputs; **True** for `223` copied remote entries. Staged CLI/skill are hash-only by policy.
- Integrity inventories: `collection-file-hashes.json` and `remote-hash-check.json`.
- **Smallest next step:** Parent may accept and publish this exact evidence set for PR450; do not rerun. Publish the two reports plus collection directory/remote-hash ledger, while retaining remote final.out paths and hashes for any reviewer requiring raw output.

