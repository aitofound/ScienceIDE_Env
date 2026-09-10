#!/usr/bin/env python3
"""Validate and reduce complete-shell AMPS cutoff-rigidity maps.

The trajectory runner writes one R50 table per epoch and altitude.  This
postprocessor turns those tables into an explicitly global, publication-ready
data product.  It validates the configured longitude/latitude lattice, removes
the coordinate duplication at the geographic poles, constructs quiet-relative
storm changes, and records coverage/censoring diagnostics.  It never invents a
cutoff for unresolved or unbracketed cells.

The expensive AMPS calculation is deliberately absent from this file.  The
script can therefore be rerun after a plotting or reduction change without
reinitializing the magnetic field or retracing a particle.
"""

from __future__ import annotations

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Mapping, Sequence, Tuple

from analyze_dynamics import cutoff_map_change_products
from study_common import (
    default_output_root, finite_float, load_config, parse_utc, read_csv,
    resolve_output_path, write_csv,
)


MAP_STATUSES = (
    "BRACKETED", "BELOW_RANGE", "ABOVE_RANGE", "UNBRACKETED", "INCOMPLETE",
)


def _configured_axis(start: float, stop: float, step: float) -> List[float]:
    """Return an inclusive decimal axis without cumulative roundoff drift."""

    count = int(round((stop - start) / step))
    if count < 1 or not math.isclose(start + count * step, stop, abs_tol=1.0e-8):
        raise ValueError(
            f"axis [{start:g}, {stop:g}] is not divisible by step {step:g}"
        )
    return [round(start + index * step, 8) for index in range(count + 1)]


def expected_global_grid(config: Mapping[str, object]) -> Tuple[List[float], List[float]]:
    """Construct the exact nonduplicated GEO grid required by the global run.

    Longitude is periodic, so 360 degrees is not a second grid point.  Latitude
    includes both poles.  A pole is one physical location regardless of how
    many longitudes the shell writer happens to emit there; canonical products
    retain one deterministic representative at longitude zero.
    """

    model = config["model"]  # type: ignore[index]
    lon_step = float(model["shell_longitude_step_deg"])  # type: ignore[index]
    lat_step = float(model["shell_latitude_step_deg"])  # type: ignore[index]
    band = [float(value) for value in model["latitude_band_abs_deg"]]  # type: ignore[index]
    if band != [0.0, 90.0]:
        raise ValueError(
            "global cutoff maps require latitude_band_abs_deg=[0.0, 90.0]"
        )
    longitudes = _configured_axis(0.0, 360.0, lon_step)[:-1]
    latitudes = _configured_axis(-90.0, 90.0, lat_step)
    return longitudes, latitudes


def _coordinate_key(row: Mapping[str, str]) -> Tuple[float, float]:
    """Normalize periodic longitude and round coordinates for stable joins."""

    latitude = round(float(row["latitude_geo_deg"]), 8)
    longitude = round(float(row["longitude_geo_deg"]) % 360.0, 8)
    if math.isclose(abs(latitude), 90.0, abs_tol=1.0e-7):
        longitude = 0.0
        latitude = math.copysign(90.0, latitude)
    return longitude, latitude


def canonicalize_map_rows(rows: Sequence[Mapping[str, str]]) -> Tuple[List[Dict[str, object]], Dict[str, object]]:
    """Collapse repeated pole records while preserving their quality evidence.

    A regular longitude/latitude writer commonly emits every longitude at both
    poles.  Those records describe the same point and must not be counted as
    independent area samples.  The representative is chosen deterministically
    (lowest original longitude).  Status disagreement or numerical R50 spread
    is retained as a pole diagnostic rather than hidden by averaging.
    """

    grouped: Dict[Tuple[float, float], List[Mapping[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[_coordinate_key(row)].append(row)

    canonical: List[Dict[str, object]] = []
    inconsistent_poles = 0
    largest_pole_spread = 0.0
    collapsed = 0
    for (longitude, latitude), candidates in sorted(
            grouped.items(), key=lambda item: (item[0][1], item[0][0])):
        candidates = sorted(
            candidates, key=lambda row: float(row["longitude_geo_deg"]) % 360.0
        )
        representative: Dict[str, object] = dict(candidates[0])
        representative["longitude_geo_deg"] = longitude
        representative["latitude_geo_deg"] = latitude
        representative["n_collapsed_coordinate_records"] = len(candidates)
        statuses = {str(row.get("cutoff_status", "")) for row in candidates}
        values = [
            value for value in (
                finite_float(row.get("cutoff_rigidity_r50_gv"))
                for row in candidates
            ) if value is not None
        ]
        spread = max(values) - min(values) if values else 0.0
        representative["coordinate_status_consistent"] = len(statuses) == 1
        representative["coordinate_cutoff_spread_gv"] = spread
        if len(candidates) > 1:
            collapsed += len(candidates) - 1
            largest_pole_spread = max(largest_pole_spread, spread)
            if len(statuses) != 1:
                inconsistent_poles += 1
        canonical.append(representative)
    return canonical, {
        "n_input_rows": len(rows),
        "n_canonical_rows": len(canonical),
        "n_collapsed_pole_rows": collapsed,
        "n_inconsistent_poles": inconsistent_poles,
        "maximum_pole_cutoff_spread_gv": largest_pole_spread,
    }


def validate_coordinates(rows: Sequence[Mapping[str, object]],
                         longitudes: Sequence[float],
                         latitudes: Sequence[float]) -> List[str]:
    """Return precise coverage errors for a canonical complete-shell map."""

    actual = {
        (round(float(row["longitude_geo_deg"]), 8),
         round(float(row["latitude_geo_deg"]), 8))
        for row in rows
    }
    expected = {
        (longitude, latitude)
        for latitude in latitudes
        for longitude in (
            (0.0,) if math.isclose(abs(latitude), 90.0, abs_tol=1.0e-8)
            else longitudes
        )
    }
    missing = expected - actual
    extra = actual - expected
    failures: List[str] = []
    if missing:
        failures.append(f"missing {len(missing)} configured GEO cells")
    if extra:
        failures.append(f"found {len(extra)} unexpected GEO cells")
    if len(actual) != len(rows):
        failures.append(f"found {len(rows) - len(actual)} duplicate canonical GEO cells")
    return failures


def write_manifest(path: Path, rows: Sequence[Mapping[str, object]]) -> None:
    """Write a stable manifest even when a caller supplies ordinary mappings."""

    write_csv(path, rows)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True,
                        help="Merged, frozen global-map study configuration")
    parser.add_argument(
        "--map-root", type=Path,
        default=default_output_root() / "global_maps" / "morphology",
        help="Raw and first-pass map products written by run_morphology.py",
    )
    parser.add_argument(
        "--output-root", type=Path,
        default=default_output_root() / "global_maps" / "postprocessing",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    _root, config = load_config(args.config)
    map_root = resolve_output_path(args.map_root)
    output = resolve_output_path(args.output_root)
    output.mkdir(parents=True, exist_ok=True)
    canonical_root = output / "canonical_maps"
    canonical_root.mkdir(parents=True, exist_ok=True)

    source_manifest_path = map_root / "cutoff_rigidity_map_manifest.csv"
    if not source_manifest_path.is_file():
        raise SystemExit(f"missing global cutoff-map manifest: {source_manifest_path}")
    source_manifest = read_csv(source_manifest_path)
    if not source_manifest:
        raise SystemExit(f"empty global cutoff-map manifest: {source_manifest_path}")

    longitudes, latitudes = expected_global_grid(config)
    expected_cells = len(longitudes) * (len(latitudes) - 2) + 2
    expected_altitudes = {
        float(value) for value in config["model"]["shell_altitudes_km"]  # type: ignore[index]
    }
    minimum_exact_fraction = float(
        config["global_map_analysis"]["minimum_exact_map_fraction"]  # type: ignore[index]
    )
    canonical_manifest: List[Dict[str, object]] = []
    quality_rows: List[Dict[str, object]] = []
    failures: List[str] = []

    for item in sorted(
            source_manifest,
            key=lambda row: (parse_utc(row["epoch_utc"]), float(row["altitude_km"]))):
        source = map_root / item["map_path"]
        label = f"{item['epoch_utc']}/{item['altitude_km']}km"
        if not source.is_file():
            failures.append(f"{label}: missing source map {source}")
            continue
        raw_rows = read_csv(source)
        canonical, pole_diagnostics = canonicalize_map_rows(raw_rows)
        coverage_failures = validate_coordinates(canonical, longitudes, latitudes)
        failures.extend(f"{label}: {message}" for message in coverage_failures)

        altitude = float(item["altitude_km"])
        if altitude not in expected_altitudes:
            failures.append(f"{label}: unexpected altitude")
        epoch_token = parse_utc(item["epoch_utc"]).strftime("%Y%m%dT%H%M%S")
        destination = (
            canonical_root / f"alt_{altitude:g}km" / epoch_token /
            "global_cutoff_rigidity_map.csv"
        )
        write_csv(destination, canonical)
        counts = {
            status: sum(str(row.get("cutoff_status")) == status for row in canonical)
            for status in MAP_STATUSES
        }
        exact_fraction = counts["BRACKETED"] / len(canonical) if canonical else 0.0
        if exact_fraction < minimum_exact_fraction:
            failures.append(
                f"{label}: exact BRACKETED map fraction {exact_fraction:.3f} is "
                f"below required {minimum_exact_fraction:.3f}"
            )
        canonical_manifest.append({
            "epoch_utc": item["epoch_utc"],
            "altitude_km": altitude,
            "map_path": destination.relative_to(canonical_root).as_posix(),
            "n_spatial_cells": len(canonical),
            **{f"n_{status.lower()}": counts[status] for status in MAP_STATUSES},
            "sampled_rigidity_min_gv": item["sampled_rigidity_min_gv"],
            "sampled_rigidity_max_gv": item["sampled_rigidity_max_gv"],
        })
        quality_rows.append({
            "epoch_utc": item["epoch_utc"], "altitude_km": altitude,
            "source_map": str(source), "canonical_map": str(destination),
            "expected_canonical_cells": expected_cells,
            "actual_canonical_cells": len(canonical),
            "grid_complete": not coverage_failures,
            "exact_bracketed_fraction": exact_fraction,
            **pole_diagnostics,
            **{f"n_{status.lower()}": counts[status] for status in MAP_STATUSES},
        })

    found_altitudes = {float(row["altitude_km"]) for row in canonical_manifest}
    if found_altitudes != expected_altitudes:
        failures.append(
            f"manifest altitudes {sorted(found_altitudes)} do not match configured "
            f"altitudes {sorted(expected_altitudes)}"
        )
    epochs_by_altitude = {
        altitude: {row["epoch_utc"] for row in canonical_manifest
                   if float(row["altitude_km"]) == altitude}
        for altitude in expected_altitudes
    }
    if epochs_by_altitude and len({frozenset(value) for value in epochs_by_altitude.values()}) != 1:
        failures.append("altitude shells do not contain identical epoch sets")

    write_manifest(canonical_root / "cutoff_rigidity_map_manifest.csv", canonical_manifest)
    write_csv(output / "global_map_quality_summary.csv", quality_rows)

    landmarks_path = map_root / "event_landmarks.json"
    if not landmarks_path.is_file():
        failures.append(f"missing event landmarks: {landmarks_path}")
        landmarks = {}
    else:
        landmarks = json.loads(landmarks_path.read_text(encoding="utf-8"))

    if landmarks:
        spatial, evolution, change_summary = cutoff_map_change_products(
            canonical_root,
            # Exclude the entire compression-search interval from the quiet
            # baseline.  Samples between the search-window start and the
            # objective pressure-jump epoch may already contain precursor
            # compression and must not dilute the erosion estimate.
            parse_utc(config["event"]["compression_search_start_utc"]),  # type: ignore[index]
            parse_utc(landmarks["compression"]),
            float(config["global_map_analysis"]["large_decrease_threshold_gv"]),  # type: ignore[index]
        )
    else:
        spatial, evolution = [], []
        change_summary = {
            "status": "NOT_AVAILABLE", "reason": "event landmarks are absent",
            "n_maps": len(canonical_manifest),
        }
    write_csv(output / "global_cutoff_event_change.csv", spatial)
    write_csv(output / "global_cutoff_change_timeseries.csv", evolution)
    (output / "global_cutoff_change_summary.json").write_text(
        json.dumps(change_summary, indent=2) + "\n", encoding="utf-8"
    )

    result = {
        "study_id": config["study_id"],
        "source_map_root": str(map_root),
        "canonical_map_root": str(canonical_root),
        "n_source_maps": len(source_manifest),
        "n_canonical_maps": len(canonical_manifest),
        "n_epochs": len({row["epoch_utc"] for row in canonical_manifest}),
        "n_altitudes": len(found_altitudes),
        "expected_cells_per_map_after_pole_collapse": expected_cells,
        "longitude_step_deg": float(config["model"]["shell_longitude_step_deg"]),  # type: ignore[index]
        "latitude_step_deg": float(config["model"]["shell_latitude_step_deg"]),  # type: ignore[index]
        "mesh_reuse_verified_upstream": True,
        "event_change_status": change_summary.get("status"),
        "failures": failures,
        "passed": not failures,
    }
    (output / "global_map_postprocessing_result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2), flush=True)
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
