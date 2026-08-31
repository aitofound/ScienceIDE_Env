# Post-authorization fail-closed readiness addendum

## Disposition

**PASS; no remaining readiness blocker.** The daemon remained operational and
continued. All prior reports, evidence, solve roots, images, containers,
transcripts, failed rewards, and `node_modules` remain preserved.

The already-new successful bare solve roots `docker-readiness-solve-5b` and
`docker-readiness-solve-6b` remain the qualifying pair. They were created only
after the 17-check scope correction and winds harness correction, each exited
zero with 17/17 receipts, and are physically distinct. In accordance with the
latest explicit instruction, they were not rerun solely for the verifier-side
classification refinement.

## Exact final winds predicate

`tests/oracle/run-suite.py` and `tests/verify.py` now define and use the
identical full-line predicate:

```text
FAILED: +0 +of +(?:0|[1-9][0-9]*) +0\.0%
```

Only a complete normalized line shaped as the pinned emitted summary
`FAILED: 0 of N 0.0%` is exempt, where `N` is `0` or a canonical positive ASCII
integer. Leading/trailing text or whitespace, leading-zero counts, signed or
floating counts, altered percentages, nonzero summaries, `FAILED command`, and
every other line containing `FAILED` remain rejected. Oracle and verifier also
retain skip/`STOP 666`, nonzero-process, missing-marker, zero-assertion, and
reference/candidate/receipt assertion-count guards.

Current hashes:

```text
592a8583db61f455f258d8fcab1ae23f74cfcfe47b20625fcac0b724d807546d  tests/oracle/run-suite.py
1ae80e829637a9a1f2c1003ef6a614ab25d02b6dc809b1ecc04f7a8e727a224f  tests/verify.py
```

## Exact fail-closed `.in` timestamp normalization

The verifier applies normalization only to `.in` comparison. It requires one
complete pinned Phantom writer header at byte/line position one on each side:

```text
# Runtime options file for Phantom, written DD/MM/YYYY HH:MM:SS.d
```

The date/time syntax is anchored and range-shaped. Missing, duplicate,
malformed, incomplete, moved, prefixed, suffixed, or later malformed writer
headers are rejected. Only that first header is canonicalized; every remaining
byte is compared exactly. Raw `myrun.setup` comparison, receipt hashes and
inventory, `nmax=0`, dump checks, and all wind checks are unchanged.

## Targeted positive/negative validation

Command:

```bash
python3 comment/fail-closed-harness-validation.py
```

Result: exit 0. The preserved test covers:

- three valid zero-failure summaries, including `N=0`;
- 14 rejected `FAILED` forms: nonzero, malformed count/percentage,
  leading/trailing/prefix/suffix text, `FAILED command`, and assertion failure;
- identical oracle/verifier predicate and retained skip/STOP/process/marker and
  assertion guards;
- valid timestamp-only comparison;
- rejected missing, duplicate, malformed, incomplete, moved, prefixed,
  suffixed, and later-malformed headers;
- rejection of a separate non-header byte difference;
- unchanged raw setup-file comparison;
- strict replay over all 14 setup rows and three wind rows in the immutable
  successful roots.

Artifacts:

```text
90b33c5e3382fbc990ea96f36bae3df27ee946afaa5cb27f1571b8a7d9da3178  comment/fail-closed-harness-validation.py
629dc32ec76d5ccdb96d3027a388e8050f09c0d0e3df383820a63b2ed0c341c5  comment/fail-closed-harness-validation.log
```

All fixtures are preserved under
`comment/fail-closed-harness-validation-fixtures/`.

## Dockerfile label preservation and hashes

The solve-4b container's staged Dockerfile was copied read-only to
`comment/docker-readiness-solve-4b.Dockerfile`:

```text
before_sha256=899a1ef78ceb10b35052681d24e1b5388cc8c9e6ca9751ec084d4ecb44790493
before_label=sciaccel.check-count="18"
after_sha256=eca1b5773af6f3b83447f2286f20315f2df4debdf3f13390fa17b050886092fb
after_label=sciaccel.check-count="17"
solve4b_image_label=18
solve5b_image_label=17
solve6b_image_label=17
```

The exact record is
`comment/dockerfile-check-count-label-hashes.txt` (SHA-256
`bbe33d4caa14ba5d7e48fa2ac71c8249e67fbada58c5c86b68cc3a255267b77e`).
Solve-4b remains preserved and is not counted.

## New separate bare self-test — attempt 3

Exact command:

```bash
HARBOR_REFERENCE_DIR="$PWD/comment/docker-readiness-solve-5b" \
HARBOR_CANDIDATE_DIR="$PWD/comment/docker-readiness-solve-6b" \
HARBOR_REWARD_FILE="$PWD/comment/docker-readiness-self-test-5b-vs-6b-attempt-3.reward.json" \
PHANTOM_SELF_TEST=1 \
./tests/test.sh
```

Captured result:

```text
started_utc=2026-08-31T04:12:59Z
ended_utc=2026-08-31T04:12:59Z
elapsed_seconds=0
exit=0
status=passed
passed_checks=17
total_checks=17
reward=1.0
self_test_mode=true
self_test_ok=true
```

Reward SHA-256:
`011ea2b3784018979c58cabedde5196077932a061b09f90ca406a7b9d9d2ff4c`.
The earlier failed self-test/reward and successful attempt-2 evidence were not
modified.

## Immutable-root and non-alias proof

Before and after attempt 3, each root's canonical list of relative path,
device, inode, byte size, and SHA-256 had the same digest:

```text
docker-readiness-solve-5b:
33c7915390a6cb946b5604d1270d0387a1af701efd47c503751e94327f5b4c06
77 files; root inode 1237514849

docker-readiness-solve-6b:
a8ba53afdc143c608033972a197d2b2f40db42cc431fcccfbe4205ac03697951
77 files; root inode 1237628933
```

The roots retain distinct realpaths, root inodes, and container IDs, with zero
shared file inodes. Integrity evidence:
`comment/docker-readiness-self-test-attempt-3-root-integrity.txt` (SHA-256
`54a9c72571089b0795dc2f48f16c53897dad14de72c2b04b93294c8b645a0025`).

## Final static, pin/source, Docker, and non-alias gates

The full task-scoped final gate ran at
`2026-08-31T04:14:01Z` and exited zero. It passed:

- shell and Python syntax;
- task TOML plus 142 task-local JSON documents;
- exactly 17 unique suite/direct-check rows and all pinned source paths;
- exoALMA absent from active authority and wiring preserved only in the archive;
- task-local Harbor validator (`1 active target`);
- shared source pin `e53ea16758d2a261680506852a528f21270dca1c`, tree
  `ae40f54661feb12f0550092fd2188e5738b7b955`, tracked clean;
- 34 receipt hash/inventory/exit/source checks across both roots;
- zero shared file inodes;
- solve-5b/6b image and container labels 17, exact source pin, container exits 0;
- attempt-3 reward 1.0, 17/17, both self-test booleans true;
- byte-identical preservation of all three prior readiness reports.

Gate log:
`comment/docker-readiness-attempt-3-final-gates.log` (SHA-256
`57c40036ef7386f0f2e5429bddd91227ec3d79d6b83469fda042aba68854560f`).

## Preserved reports and cumulative diff

Unchanged prior report hashes:

```text
497a24f5da120a5ca6a442c23cba333ebb47ca7cfe1603f10b7691a899b8f703  comment/docker-readiness-report.md
c1b707a4a89bc3157cc0995b745f791e986b14bb60a5884ded6884ce218bd0f3  comment/docker-readiness-followup-report.md
df0609852cb2720dc90356b44caf81f6e12f57615f6eb47d9be1f6561d2873d0  comment/docker-readiness-17check-final-report.md
```

A new exact cumulative diff, preserving the prior patch, is:

```text
6042832eefca5146e79cc791e62aeef5a675fbede6a892b9f9b5da7fd2be0876  comment/docker-readiness-cumulative-tests-diff-vs-solve1-authorized-final.patch
```

Machine-readable addendum evidence:

```text
0310c5c4019ba1c93942703810db5a693ed6eb5cfe5cacd3bccb2658071a1d1a  comment/docker-readiness-authorized-final-evidence.json
```

## Changed paths in this authorized continuation

Active code changed only:

- `tests/oracle/run-suite.py`
- `tests/verify.py`

New preserved evidence only:

- `comment/fail-closed-harness-validation.py`
- `comment/fail-closed-harness-validation.log`
- `comment/fail-closed-harness-validation-fixtures/`
- `comment/docker-readiness-solve-4b.Dockerfile`
- `comment/dockerfile-check-count-label-hashes.txt`
- attempt-3 command, console, reward, and root-integrity files
- `comment/docker-readiness-attempt-3-final-gates.log`
- `comment/docker-readiness-cumulative-tests-diff-vs-solve1-authorized-final.patch`
- `comment/docker-readiness-authorized-final-evidence.json`
- `comment/docker-readiness-authorized-final-addendum.md`

No shared source, setup/deck, tolerance, runtime recipe, reference artifact,
science policy, or successful solve root changed. Nothing was deleted or
cleaned, and no commit/push/PR/merge/config/auth/external action occurred.
