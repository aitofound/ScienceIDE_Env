# ScienceAccelBench PR450 corrected launch handoff — 2026-09-09 — em-828e

## Correction basis

The prior supervisor did not consume the authorized scientific attempt: the current CLI refused before Docker/build/selfcheck because the consent used a remote host string while the CLI's `require_consent` contract compares `consented_on` against `platform.node()` for `where=local`. The exact origin/main v5.11.10 implementation was read before correction:

- `consent_matches`: for `where == "local"`, requires `consented_on == platform.node()`; for a non-local `where`, rejects execution when the recorded machine is the current machine.
- `require_consent`: refuses before Docker if the record is absent, machine-mismatched, or plan-mismatched.
- `task build` calls `require_consent` before `stage_build`; `task selfcheck` calls it before creating its run root.

The failed preflight receipt and one probe were preserved additively and verified before correction:

- `workspace/sab-pr450-validation-20260909-em-828e/preflight-failed-terminal-exit-exact.json`
- `workspace/sab-pr450-validation-20260909-em-828e/preflight-failed-launch-probe-exact.txt`

They record `build_rc=1`, `selfcheck_rc=null`, no PR450 process/container, and an absent run root. No source, task, or repository path was modified.

## Authorized consent-only correction

On the same approved remote `huangzesen@136.114.2.6`, with the same head, resources and pipeline state, the consent record was corrected using exactly:

```text
python3 /home/huangzesen/sab-pr450-validation-20260909-em-828e/skills/package-sciaccel-task/scripts/sab.py task consent --task /home/huangzesen/sab-pr450-validation-20260909-em-828e/tasks/batsrus/batsrus-multifluid-fivemoment --where local --human-ref exact6874ref
```

Verified record:

- `where=local`
- `consented_on=ale-worker.us-central1-c.c.light-result-467615-p0.internal`
- `human_ref=exact6874ref`
- 16 CPUs, 16.0 GB, 14 checks, 432 s declared, 900 s guidance
- run root genuinely absent before launch

This is a machine-identity correction only, not a new science consent, host change, resource change, source patch, calibration, or retry.

## Single authorized launch

A new additive supervisor and terminal paths preserve the prior receipt:

- supervisor: `/home/huangzesen/sab-pr450-validation-20260909-em-828e/remote-supervisor-corrected.sh`
- log: `/home/huangzesen/sab-pr450-validation-20260909-em-828e/official-supervisor-corrected.log`
- terminal receipt: `/home/huangzesen/sab-pr450-validation-20260909-em-828e/official-terminal-exit-corrected.json`
- same run root: `/home/huangzesen/sab-pr450-validation-20260909-em-828e/runroot`

The supervisor runs exactly one official build followed by exactly one official selfcheck:

```text
python3 /home/huangzesen/sab-pr450-validation-20260909-em-828e/skills/package-sciaccel-task/scripts/sab.py task build --task /home/huangzesen/sab-pr450-validation-20260909-em-828e/tasks/batsrus/batsrus-multifluid-fivemoment --which both
python3 /home/huangzesen/sab-pr450-validation-20260909-em-828e/skills/package-sciaccel-task/scripts/sab.py task selfcheck --task /home/huangzesen/sab-pr450-validation-20260909-em-828e/tasks/batsrus/batsrus-multifluid-fivemoment --run-root /home/huangzesen/sab-pr450-validation-20260909-em-828e/runroot
```

Launch details:

- local Shell job: `job-038893a56f2446a7b9c7b658f759e033`, PID `46124`
- remote supervisor PID: `2045897`
- corrected supervisor SHA-256: `abe57f6c827a0bfa7702bf0a8c36fdfba2226ec92cef0aabf1d55b9a600f2a23`
- host: x86_64, 88 CPUs, Docker 29.1.3, `/home/huangzesen/bin/docker`
- probe load: `11.50 10.82 10.97`; available memory `320Gi`

The single post-launch probe at `2026-09-09T19:49:33Z` found the supervisor alive with the official `task build` child. The corrected log was present and showed Docker image build activity. The selfcheck run root and corrected terminal receipt were still absent at probe time, so no scientific result is claimed. Parent owns completion-only watching; this lane does not poll or restart.

Probe artifact: `workspace/sab-pr450-validation-20260909-em-828e/corrected-launch-probe.txt`.

Full machine/source/check/rubric/PARAM/variant/resource hashes remain in `workspace/sab-pr450-validation-20260909-em-828e/preflight-manifest.json`. The previous failed launch report remains historical and is not relabelled as this attempt.
