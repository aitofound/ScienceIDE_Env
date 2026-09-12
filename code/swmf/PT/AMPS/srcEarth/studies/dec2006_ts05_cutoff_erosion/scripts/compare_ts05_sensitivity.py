#!/usr/bin/env python3
"""Compare the three TS05 parameterization-sensitivity morphology products."""

from __future__ import annotations

import argparse
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Mapping, Tuple

from study_common import finite_float, read_csv, write_csv


KEYS = ("epoch_utc", "altitude_km", "rigidity_gv", "hemisphere", "mlt_hour")


def keyed(path: Path) -> Dict[Tuple[str, ...], Mapping[str, str]]:
    result = {}
    for row in read_csv(path):
        key = tuple(row[name] for name in KEYS)
        if key in result:
            raise ValueError(f"duplicate morphology key in {path}: {key}")
        result[key] = row
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    suite = args.suite_root.resolve()
    variants = {
        name: keyed(suite / name / "morphology_boundaries.csv")
        for name in ("full", "history_frozen", "instantaneous_frozen")
    }
    keys = set(variants["full"])
    for name, rows in variants.items():
        if set(rows) != keys:
            raise SystemExit(
                f"{name} key coverage differs from full: "
                f"missing={len(keys - set(rows))}, extra={len(set(rows) - keys)}"
            )
    details: List[Dict[str, object]] = []
    for key in sorted(keys):
        values = {
            name: finite_float(rows[key].get("boundary_aacgm_lat_deg"))
            for name, rows in variants.items()
        }
        details.append({
            **dict(zip(KEYS, key)),
            "full_boundary_deg": values["full"],
            "history_frozen_boundary_deg": values["history_frozen"],
            "instantaneous_frozen_boundary_deg": values["instantaneous_frozen"],
            "full_minus_history_frozen_deg": (
                None if values["full"] is None or values["history_frozen"] is None
                else values["full"] - values["history_frozen"]
            ),
            "full_minus_instantaneous_frozen_deg": (
                None if values["full"] is None or values["instantaneous_frozen"] is None
                else values["full"] - values["instantaneous_frozen"]
            ),
        })
    grouped = defaultdict(list)
    for row in details:
        grouped[(row["altitude_km"], row["rigidity_gv"], row["hemisphere"])].append(row)
    summaries: List[Dict[str, object]] = []
    for key, rows in sorted(grouped.items()):
        item: Dict[str, object] = {
            "altitude_km": float(key[0]), "rigidity_gv": float(key[1]),
            "hemisphere": key[2], "n_cells": len(rows),
        }
        for name in ("full_minus_history_frozen_deg", "full_minus_instantaneous_frozen_deg"):
            values = [float(row[name]) for row in rows if row[name] is not None]
            item[f"{name}_n"] = len(values)
            item[f"{name}_mean"] = statistics.fmean(values) if values else None
            item[f"{name}_rms"] = (
                math.sqrt(statistics.fmean(value * value for value in values))
                if values else None
            )
            item[f"{name}_max_abs"] = max(map(abs, values)) if values else None
        summaries.append(item)
    output = (args.output_root or (suite / "comparison")).resolve()
    output.mkdir(parents=True, exist_ok=True)
    write_csv(output / "ts05_sensitivity_differences.csv", details)
    write_csv(output / "ts05_sensitivity_summary.csv", summaries)
    (output / "ts05_sensitivity_result.json").write_text(json.dumps({
        "n_common_cells": len(details), "n_summary_rows": len(summaries),
        "warning": "Differences diagnose the TS05 parameterization, not causal magnetospheric states",
    }, indent=2) + "\n", encoding="utf-8")
    print(f"TS05 sensitivity comparison: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
