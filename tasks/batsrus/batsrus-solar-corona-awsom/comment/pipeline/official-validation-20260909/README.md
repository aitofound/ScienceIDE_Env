# PR463 original official-run evidence

This is an additive publication of the already-authorized **original** PR463 run. It is evidence and provenance, not a new computation. No build, Docker, solver, verifier, or self-check was run while preparing this publication. The large scientific result payloads remain remote-only; this subtree contains their bounded inventory/content audit, not those payloads.

## Immutable executed identity

| field | recorded value |
|---|---|
| pull request / branch | #463 / `batsrus/solar-corona-awsom` |
| task | `tasks/batsrus/batsrus-solar-corona-awsom` |
| exact original head | `71b554ec8e5c5146deba5030a1b40d49fd68317b` |
| base at execution | `25823f1add2a1fbd9e14bab4550a1a2428095721` |
| BATSRUS source pin | `9dfe746d48aa650b5209c3039f1a8676bc624899` |
| source-tree fingerprint | `452b3eb778286cd87a7fd100eeee3ccf46975c5dd52304c52d23145107837347` |
| immutable run contract | `dcc419dc11912f5292ec6b3c618298c6d660465c312cec70bc379d34e0ff3b05` |
| package skill | version `5.11.10`; validated from base commit `25823f1add2a1fbd9e14bab4550a1a2428095721`; reported package-skill object `1d0c74983e7a191be4e1a8abafc9632d665d8a4` |
| CLI execution | `25823f1add2a1fbd9e14bab4550a1a2428095721`; blob `9d5b0c78c844b644b4fad618a4f368de7d92f8df`; SHA-256 `95613ad3b2ac7eaa4681b3c44dd37e41c44c050c5fb04f320d13494f200119d5` |
| remote | approved `huangzesen@136.114.2.6`; actual host `ale-worker.us-central1-c.c.light-result-467615-p0.internal`; read-only run base `/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353` |

The host inventory reports 88 CPUs, but the execution allocation was **8 CPU / 16 GB**. The host's 88-CPU count is not an allocation claim.

## Official result and bounded audit

- Official self-check ran `2026-09-09T19:52:37Z`--`2026-09-09T22:04:24Z`; terminal exit was `0`.
- The expected set has 13 checks. The independent collection audit found 13/13 rows, 39/39 nominal/variant/alternative-build `run.ok` markers, required outputs nonempty and numeric, and 13/13 real pointwise graded/reward rows.
- The real verifier recorded **0 mismatches and 13/13 PASS** lines. All 13 alternative-build records passed and had `bound_fraction <= 1`. Reward was `1.0`, outcome `all_passed`; self-validation recorded `result=passed` and `problems=[]`.
- Exactly 167 selected modest receipt files were hash-checked byte-for-byte (`mismatches=0`, `extras=0`). The receipt hash manifest and comparison are in `collection/`.
- The remote-only inventory covers 165 result files / 453,603,517 bytes. They were content-inspected for nonempty numeric data, but are not copied here.

The exact per-row report, JSON audit, marker/log receipts, reward/self-validation, verifier log, alternative-build records, oracle manifests, consent record, and hash inventories are under `collection/`. No rubric or task input is rewritten by this publication.

## Runtime, budget, and cache note

The recorded nominal suite runtime was **1736.6 s excluding 924.0 s of builds**, above the **1500.0 s guidance budget**; the official record therefore says `budget=exceeded`. This is a budget warning, not a science failure: no checks were dropped, and no claim is made that the run met the guidance. Nominal/variant/altbuild elapsed values were 2672.968 s / 2276.987 s / 2946.039 s.

Every recorded check log contains the producer marker:

```text
SAB_BUILD_CACHE=disabled reason=missing solve-scoped source fingerprint or cache root
```

No cache reuse or speedup is demonstrated or claimed. Static/freshness review of the pinned task driver found:

- **Verified gate:** each check `run.sh` enables its build cache only when both `SAB_BUILD_CACHE_ROOT` and `SAB_SOURCE_FINGERPRINT` are nonempty (the gate is around line 71; the cache path is solve-scoped under those values).
- **Verified source of the intended scope:** pinned `tests/test.sh` computes a source fingerprint around lines 40--65 and a fresh cache root around line 66, then captures only environment-visible `SAB_*` names around line 68 before invoking each check through `env -i` (lines 81--84). Those two assignments are shell variables, not exported variables, so they are not included by that environment capture unless supplied by the caller.
- **Verified observed behavior:** the official recorded command/receipts show the vars were absent at every check, matching the disabled marker.
- **Probable cause, stated narrowly:** the non-exporting driver path (possibly compounded by orchestration not injecting the solve-scoped values into the container) explains the omission. The outer orchestration environment was not observed, so no stronger parent-layer cause is asserted. No runtime, config, environment, budget, window, core-count, or code change is made here.

## Additive preservation and scope

Before installing the exact new official `runtime-metadata.json` and `self-validation.json` at their canonical paths, the prior canonical bytes were retained at `pipeline/history/pre-2026-09-09/`. The old SHA-256 values are recorded there and remain available for comparison. The new canonical bytes are also retained under `canonical/` in this publication. No old fingerprint, timestamp, or source ID is silently replaced.

This publication intentionally does not promote changed rubric criteria, copy new bounds, or alter task inputs. PR450 follow-up reconciliation and PR453 metadata reconciliation were used only as publication procedure references.

No full build, self-check, verifier, solver, Docker run, rerun, cache experiment, or cleanup was performed for publication.
