# Phantom dust/growth final Docker readiness report — 20260831T034616Z

## Decision

**GREEN.** The final active authority contains **exactly 13 checks**. Two new,
independent bare solves of that same 13-row inventory exited 0; a separate
candidate verifier exited 0 with reward `1.0`, `self_test_mode=true`, and
`self_test_ok=true`. The pinned shared Phantom source is clean. No blocker
remains.

Worktree:
`/Users/huangzesen/work/projects/very_long_alfven_wave/.lingtai/codex/workspace/sab_phantom_dust_growth_20260830_1730`

Primary predecessor evidence (`E`):
`tasks/phantom/phantom-dust-growth/comment/docker-readiness-20260831t023425z-em91fd`

Final verifier/static/identity evidence (`F`):
`tasks/phantom/phantom-dust-growth/comment/final-readiness-20260831t033940z-eme510`

## Reconstructed inventory and authorized removals

The mechanically reconstructed transitions are `20 -> 18 -> 17 -> 15 -> 14 -> 13`.
The predecessor's last older checkpoint described the 17-row state after the
explicit Dustybox removal; the **four post-checkpoint removals** were
`dustybox-implicit-two-fluid`, `growth-dustybox-interpolation`,
`dustgaussvel-porosity-short`, and `wind-carbon-nucleation-short`.

| transition | exact removed active row(s) | pinned-upstream proof | immutable evidence (SHA-256) |
|---|---|---|---|
| 20 -> 18 | `coala-smoluchowski-initialisation`; `coala-zero-step-conservation` | Both required `SETUP=coaladisc`; pinned `build/Makefile_setups` does not register it, and the preserved build says `SETUP='coaladisc' not recognised`. | `E/upstream-invalid-coaladisc.txt` — `3b9cfbdfc505f018ce1f5396356e9bfdebad72234522110ea787f73d6b58966e`; original complete build at `comment/docker-readiness-20260831T022200Z-em870e/reference-a/docker-build-readiness-20260831t022200z-em870e-a.log` |
| 18 -> 17 | `dustybox-explicit-two-fluid` | The row required `DUSTYBOX (explicit)`, while pinned `src/tests/test_dust.f90:173-176` and completed pinned suite output emit `DUSTYBOX (explicit drag)`. | `E/upstream-invalid-dustybox-explicit-two-fluid.txt` — `4495ed1ebe122cc685f630bfee905d7c401077a83c897f1513ad8d34bef8788d`; `E/solve-c-upstream-dust.log` |
| 17 -> 15 | `dustybox-implicit-two-fluid`; `growth-dustybox-interpolation` | The first required nonexistent `(implicit)` rather than pinned `(implicit drag)`; the second required nonexistent `(explicit)` and pinned `test_growth.f90:67-70` calls that same Dustybox routine, which emits `(explicit drag)`. | `E/upstream-invalid-remaining-dustybox-markers.txt` — `0593b3b67c49a8ed21afa34780ec0f514f05ce10583774d682da0df3e811490f`; failed solve D retained |
| 15 -> 14 | `dustgaussvel-porosity-short` | Fresh solve E ran pinned `/opt/bin/phantom-porosity dustporosity.in`; the untouched executable rejected the first input (`tsmincgs not found`, unknown `grainsizemin`, incomplete input), rewrote it, and requested a retry. | `E/upstream-invalid-dustgaussvel-porosity-short.txt` — `09f81c24a4a7d2326219dff2a0fef4b0eebd743fe5fefa7410b307c7019659db`; rewritten input SHA-256 `da08fb0d47e1ccddf21b7595f20ad50419de851a98c1e7def3dc1258ebc3b472`; solve E root/image/container retained |
| 14 -> 13 | `wind-carbon-nucleation-short` | Fresh solve F ran pinned `/opt/bin/phantom-wind dustnucleation.in`; pinned wind injection computed `time_between_spheres=3.1478971610999693`, with `tmax=0.001`, then terminated `FATAL ERROR! inject_wind: no shell ejection : tmax < time_between_spheres`. | `E/upstream-invalid-wind-carbon-nucleation-short.txt` — `7864cf9376b073c607824bd232db826f971243127e7959f7907caef2711fdabe`; solve F root/image/container retained |

The exact removed-path ledger is `E/removed-paths.txt` (SHA-256
`37db4458fb997a02a4beccc028bb1cd014fe22ea638892487345a171e83b4656`);
all 21 pre-removal package-file hashes are in `E/removed-check-files.sha256`
(SHA-256 `63f2cfa3a8a116b2ab64dcd6363cfe45ecfc793362747cf5da24fd17eb93a29b`).
Every removal follows Jason Telegram #3137: remove the exact bad pinned-upstream
row and its sole-use branch-local wiring/derived authority references. No row was
patched, renamed, replaced, tolerance-weakened, or count-hard-filled. All failed
roots, logs, images, and containers remain preserved.

## Final 13 rows

1. `drag-initialisation`
2. `epstein-zero-slip-regime`
3. `epstein-stokes-transition`
4. `drag-conservation-explicit`
5. `drag-conservation-implicit`
6. `dustydiffuse-one-fluid`
7. `growth-initialisation-matrix`
8. `farmingbox-growth-two-fluid`
9. `farmingbox-growth-one-fluid`
10. `farmingbox-fragmentation-two-fluid`
11. `farmingbox-fragmentation-one-fluid` (the sole `acceleration` label)
12. `bowen-dust-radiative-wind`
13. `dustsettle-short-two-fluid`

`tests/checks.json`, the 13 sibling directories, `instruction.md`, `task.toml`,
`target/cpu-docker.json`, and generated `registry/index.yaml` agree on this
authority.

## Two successful bare solves

With:

```sh
L=/Users/huangzesen/work/projects/very_long_alfven_wave/.lingtai/codex/workspace/sab_phantom_dust_growth_20260830_1730/tasks/phantom/phantom-dust-growth
E="$L/comment/docker-readiness-20260831t023425z-em91fd"
F="$L/comment/final-readiness-20260831t033940z-eme510"
```

The exact expanded solve commands were:

```sh
HARBOR_REFERENCE_DIR="$E/reference-g" \
PHANTOM_DOCKER_RUN_ID=readiness-20260831t033001z-em91fd-g \
PHANTOM_DOCKER_IMAGE=sciaccel-phantom-dust-growth-oracle-readiness-20260831t033001z-em91fd-g \
PHANTOM_DOCKER_CONTAINER=sciaccel-phantom-dust-growth-oracle-readiness-20260831t033001z-em91fd-g \
./solution/solve.sh

HARBOR_REFERENCE_DIR="$E/reference-h" \
PHANTOM_DOCKER_RUN_ID=readiness-20260831t033003z-em91fd-h \
PHANTOM_DOCKER_IMAGE=sciaccel-phantom-dust-growth-oracle-readiness-20260831t033003z-em91fd-h \
PHANTOM_DOCKER_CONTAINER=sciaccel-phantom-dust-growth-oracle-readiness-20260831t033003z-em91fd-h \
./solution/solve.sh
```

| solve | UTC interval / elapsed | exit | root | image ID | retained container ID |
|---|---|---:|---|---|---|
| g | `03:30:01Z`–`03:38:20Z`; `498.889009208 s` | 0 | absolute `E/reference-g`; dev/inode `16777233:1237582103` | `sha256:33ad180a680372fea4ca1f05f33191bcd6fc2451a3ec820631ef1db14d18460d` | `33e09df88db156ff6b6c51bdae4e5fa5454248b9a9966dad19e90c6d9b1454b0` |
| h | `03:30:03Z`–`03:38:28Z`; `505.113168000 s` | 0 | absolute `E/reference-h`; dev/inode `16777233:1237583882` | `sha256:dceb13ddcb65a86717d177c5b756f1e89860bdf79a7e680fd3e54ff05a047dec` | `bd9f821c77bc9c8f7dff7c0d778a404bb020155b4b40614f8be59957d9f74c70` |

Both retained containers are distinct, terminal `exited`, not running, and have
container exit 0. The inherited private job
`job-110ec770e9f6425c85503da70f47ddca` completed normally after the predecessor
supervisor timeout; `E/final-pair-exits.txt` records both exits 0. No duplicate
solve was launched.

Key solve evidence hashes:

- g manifest `0c10518c8b2ec45fe88f43473cc7d8e81c478ab7066848ff64a090ef5479935d`;
  console `7570be894bbc1a1d3c134641394cbe3115b69ec8fa7d19826400a43c6d65e33a`;
  meta `6111eeaf27d296c6a26004e39608dcf0393e5c2172578daedd4ed337851ce231`.
- h manifest `9328dc62cf72b97e54b99fa5e4b6266754a49be31914e925ae07e3b55a590cb3`;
  console `39df03a93f19fa104965ce889e231d94b86b1dad5a0bf2043b427df04f1b05ee`;
  meta `bd0907f39fee47d6a9b5d14315944cf5635c1f9ba5475d5e80924c4ec8e70e64`.
- pair exits `71020d98e10776c9c14852f231fab1d51fb658142d24cadf9b28b1921fa66a4f`.

## Separate candidate verifier

Exact command:

```sh
HARBOR_REFERENCE_DIR="$E/reference-g" \
HARBOR_CANDIDATE_DIR="$E/reference-h" \
HARBOR_REWARD_FILE="$F/verifier/reward.txt" \
./tests/test.sh
```

It ran at `2026-08-31T03:40:22Z` for `0.424768042 s`, exited **0**, printed 13
`PASS` rows, and emitted exactly:

```json
{"failures": [], "passed": 13, "reward": 1.0, "self_test_mode": true, "self_test_ok": true, "total": 13}
```

The reward file is exactly `1`. Verifier console SHA-256 is
`2320a89f81617eb5e3e0d48c5d1767b9ef21e57bc0b1bd921f70ee0a25e02d47`;
meta SHA-256 is
`4b7a579cecc5f5fc3d21230358158d039f16116ae4ce1cd9e3b83eb53d94169d`.

`F/identity/nonalias-and-runtime.json` mechanically checks all 26 corresponding
`result.json`/`run.log` pairs: realpaths and device/inodes are distinct, none is
a symlink/hardlink alias, manifest run/image/container identities differ, and
manifest identities cross-check the two retained Docker objects. Its SHA-256 is
`67fdcb027b054486e0aa047af2608e966fd9a2bb7ad89f47f86578d4a60572df`.

## Pin, tree, registry, and static gates

- Declared Phantom pin: `e53ea16758d2a261680506852a528f21270dca1c`.
- Superproject HEAD: `c3d9989debcf0d1cbac64d3f1264cbf1094665f2`.
- Exact tracked `HEAD:code/phantom` tree: `ae40f54661feb12f0550092fd2188e5738b7b955`.
- `git diff -- code/phantom` is empty and source-scoped status is clean.
- Task, target, `checks.json`, all 13 rubrics, and both oracle manifests form 18
  independently checked pin authorities, all exact.
- `node scripts/gen-index.mjs` was content-reproducible: `registry/index.yaml`
  stayed `3fa17cf82527cf782bd09ac85da0d29fc428f600cc441aca7044ff97905c8e2a`;
  `registry.json` stayed
  `3aff732ef6a19ae0452d6c8489984d4e502908bc9886c367689f0b794e83c18f`.
- Exact leaf audit: 51 active non-comment files, 28 JSON, 16 Python, 3 shell,
  13 unique rows/directories, 13 pinned rubrics, exactly one acceleration label.
- Shell and in-memory Python syntax gates passed.
- Harbor leaf validator: `PASS ... (1 active target)`.
- `BASE_REF=origin/main node scripts/validate.mjs`: `ok — 16 tasks, no violations`.
- Removed-name active-scope search: zero references outside append-only `comment/`.
- Allowed-scope and `git diff --check` gates passed; unrelated untracked
  `node_modules` was preserved.

The authoritative static receipt is
`F/static/final-static-gates-attempt2.console.log` (SHA-256
`f957b7da3159fdffb477a294f3cdea2b52220036b62c740f3908bb040f7924ae`),
with exit 0. The earlier administrative audit attempt is preserved as
`final-static-gates-attempt1-failclosed.console.log`; it searched append-only
comment evidence and mishandled collapsed porcelain paths. It did not expose a
task-harness or scientific failure, changed no task authority, and was corrected
only in the external audit invocation before the authoritative exit-0 rerun.

## Safety / blocker

No pinned `code/phantom` file, expected scientific output, source meaning, or
tolerance was changed. No commit, push, PR, configuration/auth/runtime change,
contact, deletion, or cleanup occurred. All prior and current evidence, roots,
images, containers, the branch/worktree, and `node_modules` remain preserved.

**Blocker: none. Final gate: evidence-green.**
