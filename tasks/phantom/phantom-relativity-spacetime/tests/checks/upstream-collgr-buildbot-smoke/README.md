# upstream-collgr-buildbot-smoke

This active row is a source-owned Phantom buildbot setup smoke for `SETUP=collgr`. It executes one registered setup through the complete pinned `scripts/buildbot.sh` procedure. The source generates `myrun.setup` and `myrun.in`, applies its own blank-answer and `nmax = 0` procedure, records build/run/failure milestones, and requires a nonempty `myrun_00000`.

This row is limited to setup/build reachability. Provenance is authoritative in `tests/checks.json`; this folder contains labels and the thin runner/validator.
