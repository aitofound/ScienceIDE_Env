# Generated vendored code — do not edit

Everything under `scripts/_vendor/`, the wrapper scripts `scripts/sab.py`,
`scripts/harbor_validate.py` and `scripts/vendor_sync.py`, `SKILL.md`,
`SPEC.html` and `templates/` are a deterministic export from the canonical
repository:

    https://github.com/aitofound/sciaccelbench-pipeline

recorded, with per-file SHA-256 hashes, in `../vendor-manifest.json`.

- Verify offline:   `python3 scripts/vendor_sync.py verify`
- Regenerate:       `python3 scripts/vendor_sync.py sync --canonical <checkout>`

Change the pipeline in the canonical repository, then re-export; hand edits
here will fail verification and be overwritten by the next sync.
