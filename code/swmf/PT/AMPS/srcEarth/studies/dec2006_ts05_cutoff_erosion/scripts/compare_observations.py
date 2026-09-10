#!/usr/bin/env python3
"""Combine C9 and C10 outputs into publication-ready paired comparisons.

The underlying validation runners remain authoritative for extracting PAMELA
and POES/MetOp cutoffs.  This script does not refit either boundary.  It only
normalizes their already-paired CSV products, recomputes transparent residual
metrics, and writes one table that can feed figures and manuscript tables.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Sequence

from study_common import (
    default_output_root, finite_float, read_csv, residual_metrics, write_csv,
)


def find_unique(root: Path, name: str) -> Path:
    """Find one output by name and reject ambiguous solver trees."""
    matches = sorted(root.rglob(name))
    if not matches:
        raise FileNotFoundError(f"{name} was not found beneath {root}")
    if len(matches) > 1:
        joined = "\n  ".join(str(path) for path in matches)
        raise ValueError(f"multiple {name} files found; select a solver root:\n  {joined}")
    return matches[0]


def truth(value: object) -> bool:
    return str(value).strip().lower() in ("1", "true", "t", "yes")


def normalize_pamela(rows: Sequence[Mapping[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        observed = finite_float(row.get("pamela_cutoff_aacgm_deg"))
        modeled = finite_float(row.get("amps_cutoff_aacgm_deg"))
        residual = None if observed is None or modeled is None else modeled - observed
        result.append({
            "dataset": "PAMELA_TABLE_S1",
            "epoch_utc": row.get("interval_midpoint_utc", ""),
            "interval_start_utc": row.get("interval_start_utc", ""),
            "interval_end_utc": row.get("interval_end_utc", ""),
            "rigidity_gv": finite_float(row.get("rigidity_center_gv")),
            "channel": "",
            "hemisphere": "ABS_MEDIAN_NS",
            "mlt_hour": None,
            "observed_boundary_aacgm_deg": observed,
            "modeled_boundary_aacgm_deg": modeled,
            "model_minus_observation_deg": residual,
            "sigma_plus_deg": finite_float(row.get("pamela_sigma_plus_deg")),
            "sigma_minus_deg": finite_float(row.get("pamela_sigma_minus_deg")),
            "validation_role": "PRIMARY",
            "used_for_primary_metrics": observed is not None and modeled is not None,
            "observation_operator": "ORBIT_SCALE_POLAR_FLUX_T50",
        })
    return result


def normalize_poes(rows: Sequence[Mapping[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        observed = finite_float(row.get("observed_boundary_aacgm_deg"))
        modeled = finite_float(row.get("modeled_boundary_aacgm_deg"))
        residual = None if observed is None or modeled is None else modeled - observed
        result.append({
            "dataset": "NOAA_POES_METOP_SEM2",
            "epoch_utc": row.get("interval_midpoint_utc", ""),
            "interval_start_utc": "",
            "interval_end_utc": "",
            "rigidity_gv": finite_float(row.get("rigidity_gv")),
            "channel": row.get("channel", ""),
            "hemisphere": row.get("hemisphere", ""),
            "mlt_hour": finite_float(row.get("mlt_hour")),
            "observed_boundary_aacgm_deg": observed,
            "modeled_boundary_aacgm_deg": modeled,
            "model_minus_observation_deg": residual,
            "sigma_plus_deg": finite_float(row.get("sigma_deg")),
            "sigma_minus_deg": finite_float(row.get("sigma_deg")),
            "validation_role": row.get("validation_role", ""),
            "used_for_primary_metrics": truth(row.get("used_for_acceptance", "")),
            "observation_operator": "BACKGROUND_NORMALIZED_LEG_T50",
            "quality_status": row.get("quality_status", ""),
            "n_observed_crossings": row.get("n_observed_crossings", ""),
            "n_distinct_pass_legs": row.get("n_distinct_pass_legs", ""),
            "n_distinct_satellites": row.get("n_distinct_satellites", ""),
        })
    return result


def group_metrics(rows: Sequence[Mapping[str, object]], label: str) -> Dict[str, object]:
    paired = [row for row in rows if row.get("used_for_primary_metrics")]
    observed = [float(row["observed_boundary_aacgm_deg"]) for row in paired]
    modeled = [float(row["modeled_boundary_aacgm_deg"]) for row in paired]
    metrics = residual_metrics(observed, modeled)
    metrics.update({"group": label, "n_rows": len(rows), "n_primary_paired": len(paired)})
    return metrics


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--c9-root", type=Path, required=True,
                        help="C9 output root or its selected solver subdirectory")
    parser.add_argument("--c10-root", type=Path, required=True,
                        help="C10 output root or its selected solver subdirectory")
    parser.add_argument(
        "--output-root", type=Path, default=default_output_root() / "comparison"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    pamela_path = find_unique(args.c9_root.resolve(), "C9_comparison.csv")
    poes_path = find_unique(args.c10_root.resolve(), "C10_comparison.csv")
    pamela = normalize_pamela(read_csv(pamela_path))
    poes = normalize_poes(read_csv(poes_path))
    combined = pamela + poes
    output = args.output_root.resolve()
    output.mkdir(parents=True, exist_ok=True)
    write_csv(output / "paired_model_observation.csv", combined)

    summaries: List[Dict[str, object]] = [
        group_metrics(pamela, "PAMELA_ALL_RIGIDITIES"),
        group_metrics(poes, "POES_METOP_P6_P7_PRIMARY"),
    ]
    for rigidity in sorted({row["rigidity_gv"] for row in pamela if row["rigidity_gv"] is not None}):
        subset = [row for row in pamela if row["rigidity_gv"] == rigidity]
        summaries.append(group_metrics(subset, f"PAMELA_R_{float(rigidity):.9g}_GV"))
    for channel in ("P6", "P7", "P8", "P9"):
        subset = [row for row in poes if row["channel"] == channel]
        if channel in ("P8", "P9"):
            for row in subset:
                row["used_for_primary_metrics"] = (
                    row["observed_boundary_aacgm_deg"] is not None
                    and row["modeled_boundary_aacgm_deg"] is not None
                    and str(row.get("validation_role")) == "DIAGNOSTIC"
                )
        summaries.append(group_metrics(subset, f"POES_METOP_{channel}"))
    write_csv(output / "comparison_metrics.csv", summaries)
    (output / "comparison_result.json").write_text(json.dumps({
        "pamela_source": str(pamela_path), "poes_source": str(poes_path),
        "n_pamela_rows": len(pamela), "n_poes_rows": len(poes),
        "n_combined_rows": len(combined), "metrics": summaries,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote normalized comparison products to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
