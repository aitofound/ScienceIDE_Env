#!/usr/bin/env python3
"""Compare shared-mesh and standalone December-2006 morphology products.

The optimization is acceptable only if it changes process lifecycle, not the
scientific result.  This program compares the exact three-state access maps and
the derived ACCESS_T50 boundaries from two completed morphology roots.  A
typical validation campaign runs SMOKE once with ``--mesh-layout BATCHED`` and
once with ``--mesh-layout STANDALONE``, then supplies those two roots here.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from typing import Dict, List, Mapping, Sequence, Tuple

from run_morphology import load_c10_module
from study_common import load_config, resolve_output_path, write_csv


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shared-root", type=Path, required=True)
    parser.add_argument("--standalone-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--minimum-state-agreement", type=float, default=1.0)
    parser.add_argument("--maximum-unresolved-fraction-difference", type=float,
                        default=0.0)
    parser.add_argument("--maximum-boundary-difference-deg", type=float,
                        default=1.0e-10)
    return parser.parse_args()


def load_result(root: Path) -> Mapping[str, object]:
    path = root / "morphology_result.json"
    if not path.exists():
        raise FileNotFoundError(f"missing morphology result: {path}")
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("prepare_only"):
        raise ValueError(f"{path} describes a preparation-only run")
    if not value.get("passed"):
        raise ValueError(f"{path} does not describe a successful morphology run")
    return value


def split_product(root: Path, epoch_token: str, altitude_km: float) -> Path:
    """Find exactly one strict, single-shell product for a logical case.

    Layout-specific directory names are intentionally ignored.  The ``split``
    directory and epoch token are part of the stable output contract shared by
    BATCHED, PER_EPOCH, and STANDALONE modes.  Requiring one match catches stale
    overlapping runs instead of comparing an arbitrary first file.
    """

    name = f"cutoff_3d_shells_access_{altitude_km:g}km.dat"
    matches = sorted(root.glob(f"**/split/{epoch_token}/{name}"))
    if len(matches) != 1:
        raise ValueError(
            f"expected one {epoch_token}/{name} beneath {root}; found {len(matches)}"
        )
    return matches[0]


def access_key(row: object) -> Tuple[float, float, float]:
    return (round(float(row.longitude_deg) % 360.0, 9),
            round(float(row.latitude_deg), 9),
            round(float(row.rigidity_gv), 9))


def compare_access(c10, shared_path: Path, standalone_path: Path
                   ) -> Mapping[str, object]:
    """Return closure, state agreement, and unresolved diagnostics."""

    shared_rows = c10.parse_tecplot_shell_access(shared_path)
    standalone_rows = c10.parse_tecplot_shell_access(standalone_path)
    shared = {access_key(row): row for row in shared_rows}
    standalone = {access_key(row): row for row in standalone_rows}
    if len(shared) != len(shared_rows) or len(standalone) != len(standalone_rows):
        raise ValueError("duplicate access-map coordinate/rigidity key")
    common = sorted(set(shared).intersection(standalone))
    resolved_common = [key for key in common
                       if shared[key].access_state != 2
                       and standalone[key].access_state != 2]
    mismatches = sum(shared[key].access_state != standalone[key].access_state
                     for key in resolved_common)
    agreement = (1.0 - mismatches / len(resolved_common)
                 if resolved_common else 0.0)
    shared_unresolved = sum(row.access_state == 2 for row in shared_rows)
    standalone_unresolved = sum(row.access_state == 2 for row in standalone_rows)
    return {
        "n_shared": len(shared_rows),
        "n_standalone": len(standalone_rows),
        "n_common": len(common),
        "n_missing_from_shared": len(set(standalone) - set(shared)),
        "n_missing_from_standalone": len(set(shared) - set(standalone)),
        "n_resolved_common": len(resolved_common),
        "n_resolved_state_mismatch": mismatches,
        "resolved_state_agreement": agreement,
        "shared_unresolved_fraction": shared_unresolved / len(shared_rows),
        "standalone_unresolved_fraction": standalone_unresolved / len(standalone_rows),
        "unresolved_fraction_difference": abs(
            shared_unresolved / len(shared_rows)
            - standalone_unresolved / len(standalone_rows)
        ),
    }


def read_boundaries(path: Path) -> Dict[Tuple[str, float, float, str, float], float]:
    rows: Dict[Tuple[str, float, float, str, float], float] = {}
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            key = (
                row["epoch_utc"], round(float(row["altitude_km"]), 6),
                round(float(row["rigidity_gv"]), 9), row["hemisphere"],
                round(float(row["mlt_hour"]), 6),
            )
            if key in rows:
                raise ValueError(f"duplicate morphology boundary key in {path}: {key}")
            rows[key] = float(row["boundary_aacgm_lat_deg"])
    return rows


def main() -> int:
    args = parse_args()
    root, config = load_config()
    c10 = load_c10_module(root)
    shared_root = resolve_output_path(args.shared_root)
    standalone_root = resolve_output_path(args.standalone_root)
    output_root = resolve_output_path(args.output_root)
    output_root.mkdir(parents=True, exist_ok=True)
    shared_result = load_result(shared_root)
    standalone_result = load_result(standalone_root)

    if shared_result.get("mesh_layout") == "STANDALONE":
        raise ValueError("--shared-root points to a STANDALONE run")
    if standalone_result.get("mesh_layout") != "STANDALONE":
        raise ValueError("--standalone-root is not a STANDALONE run")
    if shared_result.get("profile") != standalone_result.get("profile"):
        raise ValueError("shared and standalone profiles differ")

    # Use the saved driver-at-epochs file as the authoritative logical workset.
    with (shared_root / "driver_at_model_epochs.csv").open(newline="") as stream:
        epoch_rows = list(csv.DictReader(stream))
    epoch_column = "epoch_utc" if "epoch_utc" in epoch_rows[0] else "epoch"
    epochs = [row[epoch_column].replace("-", "").replace(":", "").replace("Z", "")
              for row in epoch_rows]
    altitudes = [float(value) for value in config["model"]["shell_altitudes_km"]]

    details: List[Dict[str, object]] = []
    passed = True
    for epoch_token in epochs:
        # Tokens from format_utc contain a literal T and already have punctuation
        # removed, matching the runner's directory naming convention.
        for altitude in altitudes:
            shared_path = split_product(shared_root, epoch_token, altitude)
            standalone_path = split_product(standalone_root, epoch_token, altitude)
            row = dict(compare_access(c10, shared_path, standalone_path))
            row.update({
                "epoch_token": epoch_token, "altitude_km": altitude,
                "shared_path": str(shared_path),
                "standalone_path": str(standalone_path),
            })
            row_passed = (
                row["n_shared"] == row["n_standalone"] == row["n_common"]
                and row["resolved_state_agreement"] >= args.minimum_state_agreement
                and row["unresolved_fraction_difference"]
                    <= args.maximum_unresolved_fraction_difference
            )
            row["passed"] = row_passed
            passed = passed and bool(row_passed)
            details.append(row)

    shared_boundaries = read_boundaries(shared_root / "morphology_boundaries.csv")
    standalone_boundaries = read_boundaries(
        standalone_root / "morphology_boundaries.csv"
    )
    boundary_common = set(shared_boundaries).intersection(standalone_boundaries)
    boundary_missing = len(set(standalone_boundaries) - set(shared_boundaries))
    boundary_extra = len(set(shared_boundaries) - set(standalone_boundaries))
    maximum_boundary_difference = max(
        (abs(shared_boundaries[key] - standalone_boundaries[key])
         for key in boundary_common), default=float("inf")
    )
    boundary_passed = (
        boundary_missing == 0 and boundary_extra == 0
        and math.isfinite(maximum_boundary_difference)
        and maximum_boundary_difference <= args.maximum_boundary_difference_deg
    )
    passed = passed and boundary_passed

    write_csv(output_root / "mesh_reuse_access_comparison.csv", details)
    result = {
        "passed": passed,
        "shared_layout": shared_result.get("mesh_layout"),
        "standalone_layout": standalone_result.get("mesh_layout"),
        "profile": shared_result.get("profile"),
        "n_access_cases": len(details),
        "minimum_state_agreement": args.minimum_state_agreement,
        "maximum_unresolved_fraction_difference_allowed":
            args.maximum_unresolved_fraction_difference,
        "n_boundary_shared": len(shared_boundaries),
        "n_boundary_standalone": len(standalone_boundaries),
        "n_boundary_common": len(boundary_common),
        "n_boundary_missing_from_shared": boundary_missing,
        "n_boundary_missing_from_standalone": boundary_extra,
        "maximum_boundary_difference_deg": maximum_boundary_difference,
        "maximum_boundary_difference_allowed_deg":
            args.maximum_boundary_difference_deg,
        "boundary_comparison_passed": boundary_passed,
    }
    (output_root / "mesh_reuse_equivalence.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
