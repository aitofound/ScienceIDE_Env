# Preserved check execution metadata

These JSON files are byte-for-byte copies of the legacy rich `tests/checks/<name>/check.json` files restored from accepted SR-hydro source head `863b776f76f5d3bbee7150c7b97997d71fd1ebec`.

Current main (PR #318 contract) reserves direct check-level `check.json` files for the generic `labels` array. The task runners do not read these metadata files, so the direct copies were converted to labels-only JSON while the full execution/evidence metadata remains here under the runtime-hidden `comment/` tree. Existing labels-only files, including the dedicated acceleration check, were left unchanged.
