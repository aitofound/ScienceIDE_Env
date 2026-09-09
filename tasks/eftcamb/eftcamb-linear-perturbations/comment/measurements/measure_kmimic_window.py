#!/usr/bin/env python3
"""Measurement B: K-mimic full-window probe scoring.

Scores one nominal-vs-<other> pair of kmimic check output directories with
the check's own comparison rule (abs(c-r) <= atol[file] + rtol*|r|, per-file
atol from rubric.json), and reports failing counts, max bound_fraction,
per-table max relative error, and the coordinate (ell for angular tables,
k/h for matterpower/transfer_out) range where failures sit. Standalone,
read-only: does not touch the check's validate.py or rubric.

Usage:
    python3 measure_kmimic_window.py <rubric.json> <ref_dir> <cand_dir> <label>
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path


def load_table(path: Path):
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        rows.append([float(tok.replace("D", "E").replace("d", "e")) for tok in stripped.split()])
    return rows


def main() -> int:
    rubric_path, ref_dir, cand_dir, label = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4]
    rubric = json.loads(rubric_path.read_text(encoding="utf-8"))
    comparison = rubric["comparison"]
    rtol = float(comparison["rtol"])
    print(f"# {label}: {ref_dir} vs {cand_dir}\n")
    overall_worst_fraction = 0.0
    overall_failing = 0
    overall_total = 0
    for spec in comparison["files"]:
        file_label, suffix, atol = spec["label"], spec["suffix"], float(spec["atol"])
        ref_files = sorted(p for p in ref_dir.glob(f"*_{suffix}") if p.is_file())
        n_total = 0
        n_failing = 0
        worst_fraction = 0.0
        max_rel_above = 0.0
        fail_coords = []
        for rp in ref_files:
            cp = cand_dir / rp.name
            if not cp.is_file():
                continue
            rrows, crows = load_table(rp), load_table(cp)
            if len(rrows) != len(crows):
                print(f"  {file_label}: {rp.name} row-count mismatch ({len(rrows)} vs {len(crows)}), skipped")
                continue
            for rr, cr in zip(rrows, crows):
                coord = rr[0] if rr else None
                ncols = min(len(rr), len(cr))
                row_failed = False
                for ci in range(1, ncols):
                    r, c = rr[ci], cr[ci]
                    if not (math.isfinite(r) and math.isfinite(c)):
                        continue
                    err = abs(c - r)
                    bound = atol + rtol * abs(r)
                    frac = err / bound if bound else float("inf")
                    n_total += 1
                    worst_fraction = max(worst_fraction, frac)
                    if abs(r) > atol:
                        max_rel_above = max(max_rel_above, err / abs(r))
                    if err > bound:
                        row_failed = True
                if row_failed:
                    n_failing += 1
                    if coord is not None:
                        fail_coords.append(coord)
        overall_total += n_total
        overall_failing += n_failing
        overall_worst_fraction = max(overall_worst_fraction, worst_fraction)
        coord_range = f"{min(fail_coords):.6g} to {max(fail_coords):.6g}" if fail_coords else "none"
        print(f"  {file_label} (atol={atol:g}, rtol={rtol:g}): {n_failing} failing row(s) of {n_total} value(s) checked, "
              f"max_bound_fraction={worst_fraction:.6g}, max_rel_error(|r|>atol)={max_rel_above:.6g}, "
              f"failing coordinate range={coord_range}")
    print(f"\n  TOTAL: {overall_failing} failing rows, {overall_total} values checked, overall max_bound_fraction={overall_worst_fraction:.6g}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
