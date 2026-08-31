# Post-removal Docker readiness — Jason Telegram 3137

## Verdict

**PASS for the authorized five-row readiness gate.** Two new fresh bare solves
completed with exit 0 into physically distinct roots. A separate bare verifier
completed with exit 0, reward `1.0`, 5/5 checks passed,
`self_test_mode=true`, and `self_test_ok=true`.

All earlier failed solve-1/2/3 evidence remains byte-identical to the original
artifact inventory. Nothing was deleted, cleaned, truncated, fetched, patched
in Phantom source, tolerance-adjusted, replaced, or hard-filled. The three
removed check packages were moved intact from the active `tests/checks`
projection into preserved evidence.

## Authorized final scope

Jason Telegram 3137 removed exactly these pinned-source-problem rows:

- `radshock-radiation-regression` — retained linux/arm64 run failed 8 genuine
  upstream assertions.
- `test-radiation-eos-full` — pinned source lacks the offline MESA table reached
  by EOS 10.
- `official-test-eos` — same pinned-source MESA blocker.

The surviving authoritative count is exactly **5**, in this order:

1. `raddisc-radiation-regression`
2. `radstar-radiation-regression`
3. `radiativebox-radiation-regression`
4. `testkd-radiation-regression`
5. `official-test-radiation`

No cooling/H2 check or activation script exists in active wiring. No replacement
check was added. The existing comprehensive `official-test-radiation` row now
carries the one structural `acceleration` label; its setup, selector, assertion
source, and acceptance behavior are unchanged.

## Minimal readiness repair and exact diff

The task-local runtime repair is limited to:

1. `solution/solve.sh`: snapshot each Bash pipeline's complete `PIPESTATUS`
   array before assigning individual statuses, preventing assignment-induced
   reset under `set -u`.
2. `tests/build-suites.sh`: default `PHANTOM_BUILD_JOBS` to 1 and retain a
   serialized loop, eliminating the shared `/opt/phantom/build` module-clean
   race without patching Phantom source.
3. Final five-row wiring: remove exactly the three authorized rows from
   `solution/solve.sh`, `tests/test.sh`, and `tests/validate.py`; build only the
   five sole surviving setups; renumber surviving public cases contiguously.

Final script hashes:

- `solution/solve.sh`: `aff3e62901618aa614e36d53a76d92df82b045983aa999dcafb66b9bd8529b33`
- `tests/build-suites.sh`: `d5ad5861f4f66fc61285d75c0bfad9600d7c576c2c29438d4949bdeade4a5787`
- `tests/test.sh`: `9945e4a693334873ad8021cfb591a5eaae802ab9d3ec4f26bf2ac04d380cf488`
- `tests/validate.py`: `004c92442606e7a243f4cab872075fb301043f04a4640195fbeeabf33785d48e`

The complete pre-snapshot-to-final diff, including canonical registry
projections, is `exact-diff-final.patch`:

- SHA-256: `e236e53814bfbfb27a3ebef1558d83d5d59b9db9e5ee46f1d8f11628850b3c78`
- Bytes: `45472`
- Lines: `992`

The removed packages' copied snapshots and moved originals were compared with
`diff -qr` and are byte-identical.

## Projection regeneration and minimum validation

Canonical projections were regenerated after final removal:

```sh
node scripts/gen-index.mjs
```

Exit 0; wrote `registry/index.yaml` and `registry.json`.

Freshness check:

```sh
node scripts/gen-index.mjs --check
```

Exit 0; `registry files up to date`.

Task-only structural validation, run from repository root:

```sh
python3 skills/package-sciaccel-task/scripts/validate-harbor-task.py \
  tasks/phantom/phantom-radiation-thermochemistry
```

Exit 0; one active target passed. An earlier leaf-relative invocation exited 1
because it could not resolve repository-level `code/phantom` and because the
removed row had carried the sole acceleration label. The invocation was
corrected and the existing surviving official radiation row received the
required metadata label without any science change.

The exact changed-content validation then passed with exit 0, proving:

- all solve/test/validator inventories equal the same ordered five rows;
- active check directories equal those five rows;
- public row numbers are contiguous 1–5;
- all selectors remain exactly `["radiation"]` and modes remain
  `upstream-suite`;
- build setups are exactly `raddisc radstar radiativebox testkd test`;
- serial default and two atomic pipeline snapshots are present;
- exactly one acceleration label exists;
- no removed/cooling/H2/activation wiring exists;
- source pin remains exact; and
- all 25 entries in the prior artifact inventory remain byte-identical.

A first custom-parser attempt exited 1 because it expected the one-line Bash
`setups=(...)` declaration to be multiline. The check implementation was fixed;
no source was changed in response, and the corrected validation exited 0.

## Exact solve commands, times, and exits

The environment was exported first so each entry command itself was the required
bare no-argument invocation.

### New solve-5

```sh
export HARBOR_REFERENCE_DIR="$TASK/comment/readiness-docker-20260831T022205Z-em-0b9a/solve-5/reference"
export PHANTOM_DOCKER_LOG_ROOT="$TASK/comment/readiness-docker-20260831T022205Z-em-0b9a/solve-5/docker-logs"
export PHANTOM_DOCKER_RUN_ID='postscope-20260831t025108z-em0b9a-solve5'
./solution/solve.sh
```

- Start: `2026-08-31T02:51:36Z`
- End: `2026-08-31T02:58:08Z`
- Exit: **0**
- Build: `built 5 setup-specific Phantom suites`
- Oracle: all 5 active rows completed
- Run ID: `postscope-20260831t025108z-em0b9a-solve5`
- Image ID: `sha256:b2c87f471ef96f02f1f7866a880ba6bd27c47bc64b4712cdd0985205048afb85`
- Container ID: `57314a1d4a5d43a224e84cfd3ba26f9c8a8d293c9fbf69ea66894cf1268b2a06`
- Container start/finish: `2026-08-31T02:57:50.112011781Z` /
  `2026-08-31T02:58:08.281770331Z`

### New solve-6

```sh
export HARBOR_REFERENCE_DIR="$TASK/comment/readiness-docker-20260831T022205Z-em-0b9a/solve-6/reference"
export PHANTOM_DOCKER_LOG_ROOT="$TASK/comment/readiness-docker-20260831T022205Z-em-0b9a/solve-6/docker-logs"
export PHANTOM_DOCKER_RUN_ID='postscope-20260831t025108z-em0b9a-solve6'
./solution/solve.sh
```

- Start: `2026-08-31T02:59:13Z`
- End: `2026-08-31T02:59:39Z`
- Exit: **0**
- Build: same source image reused through Docker's content cache; the script
  still performed its separate build invocation and created a distinct tag
- Oracle: a new distinct container produced all 5 rows into a new root
- Run ID: `postscope-20260831t025108z-em0b9a-solve6`
- Image ID: `sha256:b2c87f471ef96f02f1f7866a880ba6bd27c47bc64b4712cdd0985205048afb85`
- Container ID: `94a5de9d50f6775b39cc8f46aea17eac0f40a4c439cb04051b2fc7d2b787afa4`
- Container start/finish: `2026-08-31T02:59:21.300400991Z` /
  `2026-08-31T02:59:39.065160624Z`

Both retained containers exited 0, used `network_mode=none`, and carry labels:

- `sciaccel.source=e53ea16758d2a261680506852a528f21270dca1c`
- `sciaccel.check-count=5`
- `sciaccel.task=phantom-radiation-thermochemistry`
- their respective unique lowercase run IDs.

## Separate bare verifier

```sh
export HARBOR_REFERENCE_DIR="$TASK/comment/readiness-docker-20260831T022205Z-em-0b9a/solve-5/reference"
export HARBOR_CANDIDATE_DIR="$TASK/comment/readiness-docker-20260831T022205Z-em-0b9a/solve-6/reference"
export HARBOR_REWARD_FILE="$TASK/comment/readiness-docker-20260831T022205Z-em-0b9a/test-2/reward.json"
export PHANTOM_SELF_TEST=1
export PHANTOM_DOCKER_RUN_ID='postscope-20260831t025108z-em0b9a-test2'
./tests/test.sh
```

- Start/end: `2026-08-31T03:00:07Z`
- Exit: **0**
- Status/outcome: `passed` / `all_checks_passed`
- Reward: **1.0**
- Passed/total: **5/5**
- `self_test_mode`: **true**
- `self_test_ok`: **true**

Assertion-count matches:

| Check | Assertions |
|---|---:|
| `raddisc-radiation-regression` | 6/6 |
| `radstar-radiation-regression` | 6/6 |
| `radiativebox-radiation-regression` | 27/27 |
| `testkd-radiation-regression` | 32/32 |
| `official-test-radiation` | 27/27 |

## Non-aliasing proof

- Solve-5 root: device `16777233`, inode `1237332633`, 11 regular files.
- Solve-6 root: device `16777233`, inode `1237379389`, 11 regular files.
- Roots resolve to different paths and neither contains the other.
- Shared regular-file inode intersection: **0**.
- Verifier success additionally proves its fail-closed lexical, root-overlap,
  symlink, samefile, and shared-inode checks all passed.
- Containers and run IDs are distinct even though identical pinned content
  correctly produced one shared Docker image ID.

Aggregate tree hashes:

- Reference tree: `38c0e9cf82727df845e20c692ae4e56ff89ceacca130d8e5b5a173927f336410`
- Candidate tree: `ebceeb2cd7faa6da7e02e102ec07d0d5c6ee08638ef37221d37f2a62d7b92d06`

## Exposed candidate transcript hashes

- `raddisc-radiation-regression`:
  `c66e0ccb9c9e812f0da86c2fbbd0eb6340c84d423d4a29183c39ea2ac5820c36`
- `radstar-radiation-regression`:
  `be8ed50c25fc16e07b5de4f4c673d948ef5679c0c23a4ebc59f43c406063f5a9`
- `radiativebox-radiation-regression`:
  `d4589786e969cd5ce1b3a4562b61010ebe73a2e5f1b7ac00c7d4935fca08694d`
- `testkd-radiation-regression`:
  `dee0098326c21433ad93d305971f66680e0a827ae26728fc2f81f3e87755a9b5`
- `official-test-radiation`:
  `24dd1fc3dac1f7eab866db04055a73052e4e7be1522909ec6f3294baa57626dd`

## Changed paths

Runtime/wiring:

- `solution/solve.sh`
- `tests/build-suites.sh`
- `tests/test.sh`
- `tests/validate.py`
- surviving `tests/checks/*/case.json` row projections
- `tests/checks/official-test-radiation/check.json` metadata label
- exactly three check directories moved out of active `tests/checks` into
  `scope-removal-jason-3137/moved-from-tests-checks/`

Public/ledger projections:

- `instruction.md`
- `task.toml`
- `target/cpu-docker.json`
- `comment/README.md`
- `comment/finalization-checklist.md`
- `comment/module-coverage.md`
- repository `registry/index.yaml`
- repository `registry.json`

New preserved evidence:

- `comment/readiness-docker-20260831T022205Z-em-0b9a/solve-5/`
- `comment/readiness-docker-20260831T022205Z-em-0b9a/solve-6/`
- `comment/readiness-docker-20260831T022205Z-em-0b9a/test-2/`
- `comment/readiness-docker-20260831T022205Z-em-0b9a/scope-removal-jason-3137/`

## Remaining blockers

None for the requested task-local Docker readiness gate. The leaf remains draft
only because scientific-owner review and a genuinely distinct accelerator
implementation/performance evaluation were not part of this authorized gate.
No commit, push, PR, merge, runtime configuration change, cleanup, or external
contact was performed.
