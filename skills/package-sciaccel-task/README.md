# Generated directory — managed by sciaccelbench-pipeline

Everything in `skills/package-sciaccel-task/` — `SKILL.md`, `SPEC.html`,
`templates/`, `scripts/` and `vendor-manifest.json` — is a deterministic export
from the canonical, private repository

    https://github.com/aitofound/sciaccelbench-pipeline

pinned to one commit in `vendor-manifest.json`. **Do not edit these files
here.** `npm run check` (and CI) runs `scripts/vendor_sync.py verify`, which
fails on any hand edit, and the next sync overwrites the directory.

- To change the pipeline: open a PR in the canonical repository, merge it,
  then run `tools/release.py --dest <this checkout> --pr` from there.
- To see whether this copy is current: `python3 scripts/vendor_sync.py status`.
- To verify offline: `python3 scripts/vendor_sync.py verify`.

The command everyone runs is unchanged:
`python3 skills/package-sciaccel-task/scripts/sab.py ...`
