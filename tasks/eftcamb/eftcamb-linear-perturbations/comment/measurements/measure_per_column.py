#!/usr/bin/env python3
"""Measurement A: per-column error and magnitude distribution.

For each check and each output file type, using this check's own atol, and
for both comparisons (nominal-vs-variant, nominal-vs-altbuild), report per
column: max |r|, the count of entries with |r| <= atol, the max relative
error on entries with |r| > atol, and the max absolute error on entries with
|r| <= atol. Column names are read from the "#"-prefixed header line CAMB
writes (output_file_headers=T in every included base deck) -- measured from
the actual output files, not assumed. This is a standalone read-only script;
it does not touch the validator's pass rule.

Usage:
    python3 measure_per_column.py <leaf> <run_root> <out.md>

<run_root> is a selfcheck run root containing oracle-nominal/results,
oracle-variant/results and (if any check declares one) oracle-altbuild/results.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path


def load_table(path: Path):
    """Return (header_columns_or_None, rows) where rows is a list of float lists."""
    header = None
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#"):
            if header is None:
                header = stripped.lstrip("#").split()
            continue
        rows.append([float(tok.replace("D", "E").replace("d", "e")) for tok in stripped.split()])
    return header, rows


def compare_pair(ref_dir: Path, cand_dir: Path, suffix: str, atol: float):
    """Aggregate per-column stats over every *_<suffix> file present on both sides."""
    ref_files = sorted(p for p in ref_dir.glob(f"*_{suffix}") if p.is_file())
    per_col = {}   # col_index -> dict(name, max_ref_abs, n_le_atol, max_rel_above, max_abs_below)
    n_files = 0
    for rp in ref_files:
        cp = cand_dir / rp.name
        if not cp.is_file():
            continue
        rheader, rrows = load_table(rp)
        _, crows = load_table(cp)
        if len(rrows) != len(crows):
            continue
        n_files += 1
        ncols = min(len(rrows[0]), len(crows[0])) if rrows else 0
        for ci in range(ncols):
            col = per_col.setdefault(ci, {
                "name": (rheader[ci] if rheader and ci < len(rheader) else f"col{ci+1}"),
                "max_ref_abs": 0.0, "n_total": 0, "n_le_atol": 0,
                "max_rel_above": 0.0, "max_abs_below": 0.0,
            })
            for rr, cr in zip(rrows, crows):
                if ci >= len(rr) or ci >= len(cr):
                    continue
                r, c = rr[ci], cr[ci]
                if not (math.isfinite(r) and math.isfinite(c)):
                    continue
                err = abs(c - r)
                col["n_total"] += 1
                col["max_ref_abs"] = max(col["max_ref_abs"], abs(r))
                if abs(r) <= atol:
                    col["n_le_atol"] += 1
                    col["max_abs_below"] = max(col["max_abs_below"], err)
                else:
                    col["max_rel_above"] = max(col["max_rel_above"], err / abs(r))
    return n_files, per_col


def render_check(check: str, file_types: list[dict], run_root: Path, has_altbuild: bool) -> str:
    lines = [f"## {check}\n"]
    nominal = run_root / "oracle-nominal" / "results" / check
    variant = run_root / "oracle-variant" / "results" / check
    altbuild = run_root / "oracle-altbuild" / "results" / check
    for spec in file_types:
        label, suffix, atol = spec["label"], spec["suffix"], float(spec["atol"])
        lines.append(f"### {label} (`*_{suffix}`, atol={atol:g})\n")
        for cmp_name, cand_dir, avail in (
            ("nominal vs variant", variant, variant.is_dir()),
            ("nominal vs altbuild", altbuild, has_altbuild and altbuild.is_dir()),
        ):
            if not avail:
                lines.append(f"- {cmp_name}: not available\n")
                continue
            n_files, per_col = compare_pair(nominal, cand_dir, suffix, atol)
            if not per_col:
                lines.append(f"- {cmp_name}: no comparable files\n")
                continue
            lines.append(f"- {cmp_name} ({n_files} file(s) compared)\n")
            lines.append("  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |")
            lines.append("  |---|---:|---:|---:|---:|")
            for ci in sorted(per_col):
                col = per_col[ci]
                lines.append(
                    f"  | {col['name']} | {col['max_ref_abs']:.4g} | {col['n_le_atol']}/{col['n_total']} "
                    f"| {col['max_rel_above']:.4g} | {col['max_abs_below']:.4g} |"
                )
            lines.append("")
    return "\n".join(lines)


def main() -> int:
    leaf, run_root, out_path = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    checks_dir = leaf / "tests" / "checks"
    out_lines = ["# Measurement A: per-column error and magnitude distribution\n",
                 f"Run root: `{run_root}`\n"]
    for check_dir in sorted(checks_dir.iterdir()):
        if not check_dir.is_dir():
            continue
        rubric = json.loads((check_dir / "rubric.json").read_text(encoding="utf-8"))
        file_types = rubric["comparison"]["files"]
        has_altbuild = not str(rubric.get("altbuild", "")).lower().startswith("none")
        out_lines.append(render_check(check_dir.name, file_types, run_root, has_altbuild))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(out_lines) + "\n", encoding="utf-8")
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
