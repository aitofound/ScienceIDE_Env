# Measurements behind the tolerance prose

Read-only scripts and their reports; none is used by the oracle. Produced by the curator on the x86_64 worker during the 5.11.0 revision (2026-09-06/07).

- `per_column.md`, `per_column_summary.md`: Measurement A, per-column error and magnitude distribution over all 12 checks (run2 selfcheck outputs, 2026-09-06). Basis for the atol-only column list and the cross-file coverage table in `../README.md`.
- `kmimic-window-probe.md`: Measurement B, K-mimic scored on the full upstream window (SAB_LMAX=3500 SAB_KMAX=2) against the two-ulp variant and the -O1 altbuild (2026-09-07). Basis for the shipped graded window; the numbers are also in `tests/checks/kmimic/rubric.json` under `evidence.graded_window`.
- `measure_per_column.py`, `agg_per_column.py`, `measure_kmimic_window.py`, `kmimic_window_probe.sh`: the scripts that produced them.
