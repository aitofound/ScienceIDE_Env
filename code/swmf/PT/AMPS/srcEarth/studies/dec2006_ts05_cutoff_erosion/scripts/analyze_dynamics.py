#!/usr/bin/env python3
"""Derive storm-time cutoff morphology, lag, and hysteresis products.

Input rows are the MLT-resolved ACCESS_T50 boundaries written by
``run_morphology.py``.  All calculations are deterministic for the configured
random seed.  The script writes long-form intermediate tables so every figure
or manuscript number can be traced to specific model epochs and boundary cells.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import statistics
from collections import defaultdict
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

import numpy as np

from study_common import (
    DriverRow, default_output_root, finite_float, fit_first_harmonic,
    format_utc, interpolate_driver, load_config, parse_utc, pearson, quantile,
    read_csv, read_driver, resolve_output_path, write_csv,
)


# A lag or recovery curve can be drawn from a sparse workset, but eight SMOKE
# epochs do not provide enough temporal degrees of freedom for a defensible
# storm-response inference. Keeping this threshold in one named constant makes
# the distinction machine-readable and prevents a visually polished SMOKE plot
# from being mistaken for the FULL analysis.
MIN_TEMPORAL_EPOCHS_FOR_INFERENCE = 24
EARTH_RADIUS_KM = 6371.2


def fit_two_mlt_harmonics(mlt_hours: Sequence[float], latitudes_deg: Sequence[float]
                          ) -> Dict[str, float]:
    """Fit the mean plus diurnal and semidiurnal MLT harmonics.

    The first harmonic represents the dominant displaced cutoff oval. The
    second harmonic captures day-night versus dawn-dusk deformation that a
    single displaced oval cannot represent. Eight three-hour MLT sectors give
    enough independent samples for the five coefficients; rank-deficient input
    is rejected rather than silently regularized.
    """

    if len(mlt_hours) != len(latitudes_deg) or len(mlt_hours) < 5:
        raise ValueError("two-harmonic fit requires at least five paired cells")
    angle = 2.0 * math.pi * np.asarray(mlt_hours, dtype=float) / 24.0
    design = np.column_stack((
        np.ones(len(angle)), np.cos(angle), np.sin(angle),
        np.cos(2.0 * angle), np.sin(2.0 * angle),
    ))
    values = np.asarray(latitudes_deg, dtype=float)
    coefficients, _residuals, rank, _singular = np.linalg.lstsq(
        design, values, rcond=None
    )
    if rank < design.shape[1]:
        raise ValueError("singular two-harmonic MLT fit")
    mean, _c1, _s1, c2, s2 = [float(value) for value in coefficients]
    fitted = design @ coefficients
    return {
        "two_harmonic_mean_latitude_deg": mean,
        "second_harmonic_amplitude_deg": math.hypot(c2, s2),
        # The semidiurnal maximum repeats after 12 h; report its first MLT.
        "second_harmonic_phase_mlt_hour": (
            math.atan2(s2, c2) * 24.0 / (4.0 * math.pi)
        ) % 12.0,
        "two_harmonic_fit_rms_deg": float(np.sqrt(np.mean((values - fitted) ** 2))),
    }


def group_rows(rows: Sequence[Mapping[str, str]], keys: Sequence[str]):
    grouped = defaultdict(list)
    for row in rows:
        grouped[tuple(row[key] for key in keys)].append(row)
    return grouped


def bootstrap_correlation(x: Sequence[float], y: Sequence[float], block_length: int,
                          replicates: int, rng: random.Random) -> Tuple[Optional[float], Optional[float]]:
    """Moving-block bootstrap interval for a serially correlated correlation."""
    n = len(x)
    if n < 4 or replicates < 1:
        return None, None
    block_length = max(1, min(block_length, n))
    starts = list(range(0, n - block_length + 1))
    samples: List[float] = []
    for _ in range(replicates):
        indices: List[int] = []
        while len(indices) < n:
            start = rng.choice(starts)
            indices.extend(range(start, start + block_length))
        indices = indices[:n]
        value = pearson([x[i] for i in indices], [y[i] for i in indices])
        if value is not None and math.isfinite(value):
            samples.append(value)
    if not samples:
        return None, None
    return quantile(samples, 0.025), quantile(samples, 0.975)


def morphology_products(rows: Sequence[Mapping[str, str]], config: Mapping[str, object]) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Fit MLT harmonics and compute analyzed-shell access fraction and area."""
    keys = ("epoch_utc", "altitude_km", "rigidity_gv", "hemisphere")
    harmonic_rows: List[Dict[str, object]] = []
    long_rows: List[Dict[str, object]] = []
    lat_min, lat_max = [float(value) for value in config["model"]["latitude_band_abs_deg"]]  # type: ignore[index]
    denominator = math.sin(math.radians(lat_max)) - math.sin(math.radians(lat_min))
    for key, subset in sorted(group_rows(rows, keys).items()):
        valid = []
        for row in subset:
            boundary = finite_float(row.get("boundary_aacgm_lat_deg"))
            mlt = finite_float(row.get("mlt_hour"))
            if boundary is not None and mlt is not None:
                valid.append((mlt, abs(boundary)))
        base = {
            "epoch_utc": key[0], "altitude_km": float(key[1]),
            "rigidity_gv": float(key[2]), "hemisphere": key[3],
            "n_valid_mlt": len(valid), "n_requested_mlt": len(subset),
        }
        if len(valid) >= 3:
            fit = fit_first_harmonic(
                [item[0] for item in valid], [item[1] for item in valid]
            )
            # The modeled shell stops at lat_max.  Clamp the T50 boundary to the
            # analyzed band before integrating the area poleward of the boundary.
            access_terms = []
            for _, latitude in valid:
                bounded = min(lat_max, max(lat_min, latitude))
                access_terms.append(
                    (math.sin(math.radians(lat_max)) - math.sin(math.radians(bounded)))
                    / denominator
                )
            fit["accessible_area_fraction_in_analyzed_band"] = statistics.fmean(access_terms)
            # This is a spherical-shell-equivalent area, not a claim that AACGM
            # coordinates preserve geographic area exactly. It remains a useful
            # common metric for comparing the two modeled altitudes and is always
            # accompanied by the dimensionless fraction above.
            shell_radius_km = EARTH_RADIUS_KM + float(key[1])
            band_area_km2 = 2.0 * math.pi * shell_radius_km ** 2 * denominator
            fit["accessible_area_equivalent_km2"] = (
                fit["accessible_area_fraction_in_analyzed_band"] * band_area_km2
            )
            if len(valid) >= 5:
                fit.update(fit_two_mlt_harmonics(
                    [item[0] for item in valid], [item[1] for item in valid]
                ))
            else:
                fit.update({
                    "two_harmonic_mean_latitude_deg": None,
                    "second_harmonic_amplitude_deg": None,
                    "second_harmonic_phase_mlt_hour": None,
                    "two_harmonic_fit_rms_deg": None,
                })
            base.update(fit)
        else:
            base.update({
                "mean_latitude_deg": None, "amplitude_deg": None,
                "phase_mlt_hour": None, "fit_rms_deg": None,
                "accessible_area_fraction_in_analyzed_band": None,
                "accessible_area_equivalent_km2": None,
                "two_harmonic_mean_latitude_deg": None,
                "second_harmonic_amplitude_deg": None,
                "second_harmonic_phase_mlt_hour": None,
                "two_harmonic_fit_rms_deg": None,
            })
        harmonic_rows.append(base)

    # Boundary speed and quiet-relative erosion are calculated only after all
    # epoch fits exist, preserving the same grouping and sign convention.
    by_series = group_rows(
        [{key: str(value) if value is not None else "" for key, value in row.items()}
         for row in harmonic_rows],
        ("altitude_km", "rigidity_gv", "hemisphere"),
    )
    quiet_limit = parse_utc(config["event"]["compression_search_start_utc"])  # type: ignore[index]
    for _, series in by_series.items():
        series.sort(key=lambda row: parse_utc(row["epoch_utc"]))
        quiet_values = [finite_float(row.get("mean_latitude_deg")) for row in series
                        if parse_utc(row["epoch_utc"]) < quiet_limit]
        quiet_values = [value for value in quiet_values if value is not None]
        quiet_reference = statistics.median(quiet_values) if quiet_values else None
        for index, row in enumerate(series):
            mean_lat = finite_float(row.get("mean_latitude_deg"))
            speed = None
            if 0 < index < len(series) - 1:
                previous = series[index - 1]
                following = series[index + 1]
                previous_lat = finite_float(previous.get("mean_latitude_deg"))
                following_lat = finite_float(following.get("mean_latitude_deg"))
                hours = (parse_utc(following["epoch_utc"]) -
                         parse_utc(previous["epoch_utc"])).total_seconds() / 3600.0
                if previous_lat is not None and following_lat is not None and hours > 0:
                    speed = (following_lat - previous_lat) / hours
            row["quiet_reference_mean_latitude_deg"] = quiet_reference
            row["cutoff_erosion_deg"] = (
                None if mean_lat is None or quiet_reference is None
                else mean_lat - quiet_reference
            )
            row["cutoff_degradation_deg"] = (
                None if row["cutoff_erosion_deg"] is None
                else max(0.0, -float(row["cutoff_erosion_deg"]))
            )
            row["boundary_speed_deg_per_hour"] = speed
            long_rows.append(dict(row))
    return harmonic_rows, long_rows


def boundary_cell_products(rows: Sequence[Mapping[str, str]], driver: Sequence[DriverRow],
                           compression: datetime, main_phase: datetime,
                           config: Mapping[str, object]) -> List[Dict[str, object]]:
    """Add quiet-relative erosion and contemporaneous drivers to every MLT cell.

    Harmonic fits are useful summaries, but the physically important local-time
    deformation remains in the individual boundary cells. This long-form table
    is therefore the authoritative input for MLT maps and permits every plotted
    erosion value to be traced back to one modeled boundary.
    """

    keys = ("altitude_km", "rigidity_gv", "hemisphere", "mlt_hour")
    quiet_limit = parse_utc(
        config["event"]["compression_search_start_utc"]  # type: ignore[index]
    )
    result: List[Dict[str, object]] = []
    for _key, series in sorted(group_rows(rows, keys).items()):
        series = sorted(series, key=lambda row: parse_utc(row["epoch_utc"]))
        quiet = [
            abs(value) for row in series
            if parse_utc(row["epoch_utc"]) < quiet_limit
            for value in [finite_float(row.get("boundary_aacgm_lat_deg"))]
            if value is not None
        ]
        quiet_reference = statistics.median(quiet) if quiet else None
        for row in series:
            epoch = parse_utc(row["epoch_utc"])
            boundary = finite_float(row.get("boundary_aacgm_lat_deg"))
            boundary = abs(boundary) if boundary is not None else None
            erosion = (
                None if boundary is None or quiet_reference is None
                else boundary - quiet_reference
            )
            sampled = interpolate_driver(driver, epoch)
            phase = (
                "PRECOMPRESSION" if epoch < compression else
                "MAIN_PHASE" if epoch <= main_phase else "RECOVERY"
            )
            item: Dict[str, object] = dict(row)
            item.update({
                "boundary_aacgm_abs_lat_deg": boundary,
                "quiet_reference_boundary_deg": quiet_reference,
                "cutoff_erosion_deg": erosion,
                "cutoff_degradation_deg": (
                    None if erosion is None else max(0.0, -erosion)
                ),
                "event_phase": phase,
                "pdyn_npa": sampled.pdyn_npa,
                "bz_nt": sampled.bz_nt,
                "symh_nt": sampled.symh_nt,
                "w1": sampled.w1, "w2": sampled.w2, "w3": sampled.w3,
                "w4": sampled.w4, "w5": sampled.w5, "w6": sampled.w6,
            })
            result.append(item)
    return result


def altitude_response_products(time_series: Sequence[Mapping[str, object]]) -> List[Dict[str, object]]:
    """Pair the lowest and highest modeled shells without mixing other keys."""

    grouped = defaultdict(list)
    for row in time_series:
        grouped[(row["epoch_utc"], row["rigidity_gv"], row["hemisphere"])].append(row)
    output: List[Dict[str, object]] = []
    for key, rows in sorted(grouped.items()):
        valid = sorted(rows, key=lambda row: float(row["altitude_km"]))
        if len(valid) < 2:
            continue
        low, high = valid[0], valid[-1]
        item: Dict[str, object] = {
            "epoch_utc": key[0], "rigidity_gv": float(key[1]),
            "hemisphere": key[2],
            "low_altitude_km": float(low["altitude_km"]),
            "high_altitude_km": float(high["altitude_km"]),
        }
        for source, target in (
            ("mean_latitude_deg", "boundary"),
            ("cutoff_erosion_deg", "erosion"),
            ("accessible_area_fraction_in_analyzed_band", "accessible_fraction"),
        ):
            low_value = finite_float(low.get(source))
            high_value = finite_float(high.get(source))
            item[f"low_{target}"] = low_value
            item[f"high_{target}"] = high_value
            item[f"high_minus_low_{target}"] = (
                None if low_value is None or high_value is None
                else high_value - low_value
            )
        output.append(item)
    return output


def storm_extrema_products(time_series: Sequence[Mapping[str, object]]) -> List[Dict[str, object]]:
    """Summarize peak erosion and morphology for every physical series."""

    grouped = defaultdict(list)
    for row in time_series:
        grouped[(row["altitude_km"], row["rigidity_gv"], row["hemisphere"])].append(row)
    output: List[Dict[str, object]] = []
    for key, rows in sorted(grouped.items()):
        valid = [row for row in rows if finite_float(row.get("mean_latitude_deg")) is not None]
        if not valid:
            continue
        minimum = min(valid, key=lambda row: float(row["mean_latitude_deg"]))
        maximum = max(valid, key=lambda row: float(row["mean_latitude_deg"]))
        degradation = [
            (float(row["cutoff_degradation_deg"]), row)
            for row in valid if finite_float(row.get("cutoff_degradation_deg")) is not None
        ]
        peak_degradation, peak_row = max(
            degradation, default=(0.0, minimum), key=lambda item: item[0]
        )
        output.append({
            "altitude_km": float(key[0]), "rigidity_gv": float(key[1]),
            "hemisphere": key[2], "n_valid_epochs": len(valid),
            "minimum_boundary_deg": float(minimum["mean_latitude_deg"]),
            "minimum_boundary_epoch_utc": minimum["epoch_utc"],
            "maximum_boundary_deg": float(maximum["mean_latitude_deg"]),
            "maximum_boundary_epoch_utc": maximum["epoch_utc"],
            "maximum_cutoff_degradation_deg": peak_degradation,
            "maximum_degradation_epoch_utc": peak_row["epoch_utc"],
            "maximum_first_harmonic_amplitude_deg": max(
                (finite_float(row.get("amplitude_deg")) or 0.0) for row in valid
            ),
            "maximum_second_harmonic_amplitude_deg": max(
                (finite_float(row.get("second_harmonic_amplitude_deg")) or 0.0)
                for row in valid
            ),
        })
    return output


def recovery_products(time_series: Sequence[Mapping[str, object]],
                      main_phase: datetime) -> List[Dict[str, object]]:
    """Estimate nonparametric half and e-fold recovery times after peak erosion."""

    grouped = defaultdict(list)
    for row in time_series:
        grouped[(row["altitude_km"], row["rigidity_gv"], row["hemisphere"])].append(row)

    def crossing_hours(rows, peak_index: int, threshold: float) -> Optional[float]:
        peak_epoch = parse_utc(str(rows[peak_index]["epoch_utc"]))
        previous = rows[peak_index]
        previous_value = float(previous["cutoff_degradation_deg"])
        for current in rows[peak_index + 1:]:
            value = float(current["cutoff_degradation_deg"])
            if value <= threshold < previous_value:
                t0 = (parse_utc(str(previous["epoch_utc"])) - peak_epoch).total_seconds() / 3600.0
                t1 = (parse_utc(str(current["epoch_utc"])) - peak_epoch).total_seconds() / 3600.0
                fraction = (previous_value - threshold) / max(
                    1.0e-15, previous_value - value
                )
                return t0 + fraction * (t1 - t0)
            previous, previous_value = current, value
        return None

    output: List[Dict[str, object]] = []
    for key, rows in sorted(grouped.items()):
        valid = [
            row for row in sorted(rows, key=lambda row: parse_utc(str(row["epoch_utc"])))
            if parse_utc(str(row["epoch_utc"])) >= main_phase
            and finite_float(row.get("cutoff_degradation_deg")) is not None
        ]
        if not valid:
            continue
        peak_index = max(
            range(len(valid)), key=lambda i: float(valid[i]["cutoff_degradation_deg"])
        )
        peak = float(valid[peak_index]["cutoff_degradation_deg"])
        half = crossing_hours(valid, peak_index, 0.5 * peak) if peak > 0 else None
        efold = crossing_hours(valid, peak_index, peak / math.e) if peak > 0 else None
        status = (
            "AVAILABLE" if len(valid) >= 6 and half is not None and efold is not None
            else "DIAGNOSTIC_ONLY"
        )
        output.append({
            "altitude_km": float(key[0]), "rigidity_gv": float(key[1]),
            "hemisphere": key[2], "n_recovery_epochs": len(valid),
            "peak_degradation_deg": peak,
            "peak_epoch_utc": valid[peak_index]["epoch_utc"],
            "half_recovery_hours": half, "efold_recovery_hours": efold,
            "last_degradation_deg": float(valid[-1]["cutoff_degradation_deg"]),
            "status": status,
        })
    return output


def cutoff_map_change_products(
    morphology_root: Path,
    quiet_limit: datetime,
    event_start: datetime,
    decrease_threshold_gv: float = 0.05,
) -> Tuple[List[Dict[str, object]], List[Dict[str, object]], Dict[str, object]]:
    """Reduce per-epoch R50 maps into event-change maps and time series.

    Two passes keep memory bounded for FULL. The first pass constructs an exact
    geographic-cell quiet median from precompression BRACKETED values. The
    second pass calculates quiet-relative changes and retains only one spatial
    maximum per cell plus one area-weighted summary per shell and epoch. Censored
    and unresolved map cells remain in the individual map files and never enter
    numerical change estimates.
    """

    manifest_path = morphology_root / "cutoff_rigidity_map_manifest.csv"
    if not manifest_path.is_file() or manifest_path.stat().st_size == 0:
        return [], [], {
            "status": "NOT_AVAILABLE", "reason": f"missing {manifest_path}",
            "n_maps": 0,
        }
    manifest = read_csv(manifest_path)
    quiet_values: Dict[Tuple[float, float, float], List[float]] = defaultdict(list)

    def map_rows(item: Mapping[str, str]) -> List[Dict[str, str]]:
        path = morphology_root / item["map_path"]
        if not path.is_file():
            raise ValueError(f"cutoff-map manifest references missing file: {path}")
        return read_csv(path)

    for item in manifest:
        epoch = parse_utc(item["epoch_utc"])
        if epoch >= quiet_limit:
            continue
        altitude = float(item["altitude_km"])
        for row in map_rows(item):
            cutoff = finite_float(row.get("cutoff_rigidity_r50_gv"))
            if row.get("cutoff_status") != "BRACKETED" or cutoff is None:
                continue
            key = (altitude, float(row["longitude_geo_deg"]),
                   float(row["latitude_geo_deg"]))
            quiet_values[key].append(cutoff)
    quiet_reference = {
        key: statistics.median(values) for key, values in quiet_values.items()
        if values
    }

    extrema: Dict[Tuple[float, float, float], Dict[str, object]] = {}
    evolution: List[Dict[str, object]] = []
    for item in sorted(manifest, key=lambda row: (
            parse_utc(row["epoch_utc"]), float(row["altitude_km"]))):
        epoch = parse_utc(item["epoch_utc"])
        altitude = float(item["altitude_km"])
        exact_changes: List[float] = []
        conservative_decreases: List[float] = []
        exact_weights: List[float] = []
        conservative_weights: List[float] = []
        locations: List[Tuple[float, bool, Mapping[str, str]]] = []
        n_censored_lower_bounds = 0
        for row in map_rows(item):
            cutoff = finite_float(row.get("cutoff_rigidity_r50_gv"))
            key = (altitude, float(row["longitude_geo_deg"]),
                   float(row["latitude_geo_deg"]))
            reference = quiet_reference.get(key)
            if reference is None:
                continue
            status = row.get("cutoff_status")
            is_lower_bound = False
            if status == "BRACKETED" and cutoff is not None:
                current_for_decrease = cutoff
            elif status == "BELOW_RANGE":
                # R50 is below the sampled floor. Replacing it by the floor
                # yields a conservative *lower bound* on the true decrease; it
                # must not enter exact means or recovery fits.
                current_for_decrease = float(item["sampled_rigidity_min_gv"])
                is_lower_bound = True
                n_censored_lower_bounds += 1
            else:
                continue
            change = current_for_decrease - reference
            decrease = max(0.0, -change)
            weight = max(0.0, math.cos(math.radians(float(row["latitude_geo_deg"]))))
            if not is_lower_bound:
                exact_changes.append(change)
                exact_weights.append(weight)
            conservative_decreases.append(decrease)
            conservative_weights.append(weight)
            locations.append((decrease, is_lower_bound, row))
            if epoch >= event_start:
                record = extrema.setdefault(key, {
                    "altitude_km": altitude,
                    "longitude_geo_deg": key[1],
                    "latitude_geo_deg": key[2],
                    "quiet_reference_cutoff_gv": reference,
                    "n_event_resolved_epochs": 0,
                    "n_event_censored_lower_bounds": 0,
                    "sum_event_change_gv": 0.0,
                    "minimum_event_cutoff_gv_or_upper_bound": current_for_decrease,
                    "maximum_cutoff_decrease_gv": decrease,
                    "maximum_decrease_is_lower_bound": is_lower_bound,
                    "epoch_of_maximum_decrease_utc": format_utc(epoch),
                    "aacgm_latitude_at_maximum_deg": finite_float(
                        row.get("aacgm_latitude_deg")
                    ),
                    "mlt_at_maximum_hour": finite_float(row.get("mlt_hour")),
                })
                if is_lower_bound:
                    record["n_event_censored_lower_bounds"] = int(
                        record["n_event_censored_lower_bounds"]
                    ) + 1
                else:
                    record["n_event_resolved_epochs"] = int(
                        record["n_event_resolved_epochs"]
                    ) + 1
                    record["sum_event_change_gv"] = float(
                        record["sum_event_change_gv"]
                    ) + change
                record["minimum_event_cutoff_gv_or_upper_bound"] = min(
                    float(record["minimum_event_cutoff_gv_or_upper_bound"]),
                    current_for_decrease,
                )
                if decrease > float(record["maximum_cutoff_decrease_gv"]):
                    record.update({
                        "maximum_cutoff_decrease_gv": decrease,
                        "maximum_decrease_is_lower_bound": is_lower_bound,
                        "epoch_of_maximum_decrease_utc": format_utc(epoch),
                        "aacgm_latitude_at_maximum_deg": finite_float(
                            row.get("aacgm_latitude_deg")
                        ),
                        "mlt_at_maximum_hour": finite_float(row.get("mlt_hour")),
                    })
        if conservative_decreases:
            order = sorted(conservative_decreases)
            peak, peak_is_lower_bound, peak_row = max(
                locations, key=lambda pair: pair[0]
            )
            exact_weight_sum = sum(exact_weights)
            conservative_weight_sum = sum(conservative_weights)
            threshold_fraction = (
                sum(weight for decrease, weight in
                    zip(conservative_decreases, conservative_weights)
                    if decrease >= decrease_threshold_gv) / conservative_weight_sum
                if conservative_weight_sum > 0 else None
            )
            # Retain the historical 0.05-GV field for the validation-study
            # figures while adding a generic, explicitly labeled threshold for
            # broader global maps. This is backward compatible with archived
            # tables and prevents a caller-specific threshold from being hidden
            # in a column name.
            historical_fraction = (
                sum(weight for decrease, weight in
                    zip(conservative_decreases, conservative_weights)
                    if decrease >= 0.05) / conservative_weight_sum
                if conservative_weight_sum > 0 else None
            )
            evolution.append({
                "epoch_utc": format_utc(epoch), "altitude_km": altitude,
                "n_cells_with_exact_change": len(exact_changes),
                "n_cells_with_censored_decrease_lower_bound": n_censored_lower_bounds,
                "n_cells_with_quiet_reference": len(conservative_decreases),
                "fraction_of_map_with_quiet_reference": (
                    len(conservative_decreases) /
                    max(1, int(float(item["n_spatial_cells"])))
                ),
                "area_weighted_mean_cutoff_change_gv": (
                    sum(value * weight for value, weight in
                        zip(exact_changes, exact_weights)) /
                    exact_weight_sum if exact_weight_sum > 0 else None
                ),
                "median_cutoff_decrease_gv": statistics.median(
                    conservative_decreases
                ),
                "p90_cutoff_decrease_gv": quantile(order, 0.90),
                "maximum_cutoff_decrease_gv": peak,
                "maximum_decrease_is_lower_bound": peak_is_lower_bound,
                "decrease_statistics_include_censored_lower_bounds": True,
                "maximum_decrease_longitude_geo_deg": float(
                    peak_row["longitude_geo_deg"]
                ),
                "maximum_decrease_latitude_geo_deg": float(
                    peak_row["latitude_geo_deg"]
                ),
                "decrease_threshold_gv": decrease_threshold_gv,
                "area_fraction_decrease_ge_threshold": threshold_fraction,
                "area_fraction_decrease_ge_0p05_gv": historical_fraction,
            })

    spatial: List[Dict[str, object]] = []
    for key, record in sorted(extrema.items()):
        count = int(record.pop("n_event_resolved_epochs"))
        total = float(record.pop("sum_event_change_gv"))
        record["n_event_resolved_epochs"] = count
        record["mean_event_cutoff_change_gv"] = total / count if count else None
        spatial.append(record)
    largest_by_shell = []
    for altitude in sorted({float(row["altitude_km"]) for row in spatial}):
        shell = [row for row in spatial if float(row["altitude_km"]) == altitude]
        if shell:
            largest_by_shell.append(max(
                shell, key=lambda row: float(row["maximum_cutoff_decrease_gv"])
            ))
    summary = {
        "status": "AVAILABLE" if spatial else "NOT_AVAILABLE",
        "n_maps": len(manifest), "n_quiet_reference_cells": len(quiet_reference),
        "n_event_change_cells": len(spatial),
        "quiet_reference_end_utc": format_utc(quiet_limit),
        "event_change_start_utc": format_utc(event_start),
        "spatial_extent_threshold_gv": decrease_threshold_gv,
        "largest_decrease_by_shell": largest_by_shell,
        "interpretation": (
            "Positive maximum_cutoff_decrease_gv means reduced shielding. "
            "Quiet references are BRACKETED medians; event BELOW_RANGE values "
            "contribute conservative lower bounds and are explicitly flagged."
        ),
    }
    return spatial, evolution, summary


def lag_products(harmonics: Sequence[Mapping[str, object]], driver: Sequence[DriverRow],
                 config: Mapping[str, object]) -> List[Dict[str, object]]:
    """Compute driver/mean-cutoff correlations on the configured lag grid."""
    analysis = config["analysis"]  # type: ignore[index]
    min_minutes = int(round(float(analysis["lag_min_hours"]) * 60.0))
    max_minutes = int(round(float(analysis["lag_max_hours"]) * 60.0))
    step = int(analysis["lag_step_minutes"])
    replicates = int(analysis["bootstrap_replicates"])
    block_hours = float(analysis["bootstrap_block_hours"])
    rng = random.Random(int(analysis["random_seed"]))
    variables = ("pdyn_npa", "bz_nt", "symh_nt", "w1", "w2", "w3", "w4", "w5", "w6")
    output: List[Dict[str, object]] = []
    grouped = defaultdict(list)
    for row in harmonics:
        mean = finite_float(row.get("mean_latitude_deg"))
        if mean is not None:
            grouped[(row["altitude_km"], row["rigidity_gv"], row["hemisphere"])].append(row)
    for key, series in sorted(grouped.items()):
        series.sort(key=lambda row: parse_utc(str(row["epoch_utc"])))
        if len(series) > 1:
            spacings = [
                (parse_utc(str(right["epoch_utc"])) - parse_utc(str(left["epoch_utc"]))).total_seconds() / 3600.0
                for left, right in zip(series, series[1:])
            ]
            nominal_hours = statistics.median(value for value in spacings if value > 0)
        else:
            nominal_hours = block_hours
        block_length = max(1, int(round(block_hours / nominal_hours)))
        for variable in variables:
            for lag_minutes in range(min_minutes, max_minutes + 1, step):
                x: List[float] = []
                y: List[float] = []
                for row in series:
                    epoch = parse_utc(str(row["epoch_utc"]))
                    driver_epoch = epoch - timedelta(minutes=lag_minutes)
                    try:
                        sampled = interpolate_driver(driver, driver_epoch)
                    except ValueError:
                        continue
                    x.append(float(getattr(sampled, variable)))
                    y.append(float(row["mean_latitude_deg"]))
                correlation = pearson(x, y)
                # A moving-block confidence interval is meaningful only after
                # the time series has enough temporal structure. SMOKE still
                # records the correlation for pipeline testing, but deliberately
                # omits inferential bounds instead of manufacturing precision
                # from only a handful of event landmarks.
                if len(x) >= MIN_TEMPORAL_EPOCHS_FOR_INFERENCE:
                    ci_low, ci_high = bootstrap_correlation(
                        x, y, block_length, replicates, rng
                    )
                else:
                    ci_low, ci_high = None, None
                output.append({
                    "altitude_km": key[0], "rigidity_gv": key[1],
                    "hemisphere": key[2], "driver_variable": variable,
                    "lag_minutes": lag_minutes,
                    "positive_lag_means_cutoff_follows_driver": True,
                    "n_paired_epochs": len(x), "correlation": correlation,
                    "bootstrap_ci_low": ci_low, "bootstrap_ci_high": ci_high,
                    "bootstrap_block_hours": block_hours,
                    "inference_status": (
                        "AVAILABLE"
                        if len(x) >= MIN_TEMPORAL_EPOCHS_FOR_INFERENCE
                        else "DIAGNOSTIC_ONLY"
                    ),
                })
    return output


def best_lag_products(lags: Sequence[Mapping[str, object]]) -> List[Dict[str, object]]:
    """Select the strongest absolute response for each series and driver.

    The complete lag curve remains authoritative. This compact table is a
    publication aid and explicitly retains sample size and confidence limits,
    avoiding the common mistake of reporting only a visually selected lag.
    """

    grouped = defaultdict(list)
    for row in lags:
        correlation = finite_float(row.get("correlation"))
        if correlation is not None:
            grouped[(row["altitude_km"], row["rigidity_gv"], row["hemisphere"],
                     row["driver_variable"])].append(row)
    output: List[Dict[str, object]] = []
    for key, rows in sorted(grouped.items()):
        selected = max(rows, key=lambda row: abs(float(row["correlation"])))
        output.append({
            "altitude_km": float(key[0]), "rigidity_gv": float(key[1]),
            "hemisphere": key[2], "driver_variable": key[3],
            "best_lag_minutes": int(selected["lag_minutes"]),
            "positive_lag_means_cutoff_follows_driver": True,
            "correlation": float(selected["correlation"]),
            "n_paired_epochs": int(selected["n_paired_epochs"]),
            "bootstrap_ci_low": finite_float(selected.get("bootstrap_ci_low")),
            "bootstrap_ci_high": finite_float(selected.get("bootstrap_ci_high")),
            "status": selected.get("inference_status", "DIAGNOSTIC_ONLY"),
        })
    return output


def analysis_availability_products(
    boundary_rows: Sequence[Mapping[str, str]],
    time_series: Sequence[Mapping[str, object]],
    altitude_response: Sequence[Mapping[str, object]],
    hysteresis: Sequence[Mapping[str, object]],
    recovery: Sequence[Mapping[str, object]],
    cutoff_map_changes: Sequence[Mapping[str, object]],
    study_output_root: Path,
) -> List[Dict[str, object]]:
    """Describe exactly which physical interpretations the products support.

    A SMOKE run is useful for testing spatial reductions and figure generation,
    but it must not be presented as a statistically resolved time-response
    experiment. These rows give downstream scripts a stable three-state
    AVAILABLE/DIAGNOSTIC_ONLY/NOT_AVAILABLE contract.
    """

    epochs = sorted({str(row["epoch_utc"]) for row in boundary_rows})
    altitudes = sorted({float(row["altitude_km"]) for row in boundary_rows})
    rigidities = sorted({float(row["rigidity_gv"]) for row in boundary_rows})
    mlt_sectors = sorted({float(row["mlt_hour"]) for row in boundary_rows})
    temporal_status = (
        "AVAILABLE" if len(epochs) >= MIN_TEMPORAL_EPOCHS_FOR_INFERENCE
        else "DIAGNOSTIC_ONLY"
    )
    sensitivity_result = (
        study_output_root / "ts05_sensitivity" / "comparison" /
        "ts05_sensitivity_result.json"
    )
    directional_files = list(study_output_root.rglob("*direction*access*.csv"))

    def row(name: str, status: str, evidence: str, interpretation: str
            ) -> Dict[str, object]:
        return {
            "analysis": name, "status": status, "n_epochs": len(epochs),
            "evidence": evidence, "interpretation": interpretation,
        }

    return [
        row(
            "spatial_cutoff_rigidity_maps",
            "AVAILABLE" if cutoff_map_changes else "NOT_AVAILABLE",
            f"{len(cutoff_map_changes)} event-change cells with bracketed quiet references",
            "Per-epoch R50 maps and the location/magnitude of maximum storm-time decrease.",
        ),
        row(
            "rigidity_dependence", "AVAILABLE" if len(rigidities) >= 2 else "NOT_AVAILABLE",
            f"{len(rigidities)} rigidities",
            "Boundary and degradation dependence on particle rigidity.",
        ),
        row(
            "altitude_dependence", "AVAILABLE" if altitude_response else "NOT_AVAILABLE",
            f"{len(altitudes)} shells; {len(altitude_response)} paired rows",
            "Difference between identical epoch/rigidity/hemisphere keys at the two shells.",
        ),
        row(
            "mlt_morphology", "AVAILABLE" if len(mlt_sectors) >= 5 else "NOT_AVAILABLE",
            f"{len(mlt_sectors)} MLT sectors",
            "Local-time boundary cells plus first and second harmonic shape metrics.",
        ),
        row(
            "accessible_area", "AVAILABLE" if time_series else "NOT_AVAILABLE",
            f"{len(time_series)} reduced series rows",
            "Fraction and spherical-shell-equivalent area accessible in the latitude band.",
        ),
        row(
            "driver_lag", temporal_status,
            f"{len(epochs)} unique epochs; minimum {MIN_TEMPORAL_EPOCHS_FOR_INFERENCE}",
            "Lag maxima are inferential only for FULL-like temporal coverage.",
        ),
        row(
            "matched_driver_hysteresis",
            temporal_status if hysteresis else "DIAGNOSTIC_ONLY",
            f"{len(hysteresis)} matched summary rows",
            "Main/recovery contrast at similar instantaneous forcing.",
        ),
        row(
            "recovery_timescale",
            ("AVAILABLE" if temporal_status == "AVAILABLE" and
             any(item.get("status") == "AVAILABLE" for item in recovery)
             else "DIAGNOSTIC_ONLY"),
            f"{len(recovery)} candidate series",
            "Half and e-fold recovery after maximum degradation.",
        ),
        row(
            "ts05_driver_attribution",
            "AVAILABLE" if sensitivity_result.is_file() else "NOT_AVAILABLE",
            str(sensitivity_result),
            "Requires full/history-frozen/instantaneous-frozen TS05 sensitivity runs.",
        ),
        row(
            "directional_topology",
            "AVAILABLE" if directional_files else "NOT_AVAILABLE",
            f"{len(directional_files)} directional-access tables",
            "Requires directional or asymptotic access output beyond vertical boundaries.",
        ),
    ]


def hysteresis_products(boundary_rows: Sequence[Mapping[str, str]], driver: Sequence[DriverRow],
                        compression: datetime, main_phase: datetime,
                        config: Mapping[str, object]) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Match main/recovery cells at similar instantaneous forcing conditions."""
    analysis = config["analysis"]  # type: ignore[index]
    symh_tol = float(analysis["hysteresis_symh_tolerance_nt"])
    pdyn_tol = float(analysis["hysteresis_pdyn_fractional_tolerance"])
    bz_tol = float(analysis["hysteresis_bz_tolerance_nt"])
    replicates = int(analysis["bootstrap_replicates"])
    rng = random.Random(int(analysis["random_seed"]) + 1)
    keys = ("altitude_km", "rigidity_gv", "hemisphere", "mlt_hour")
    pairs: List[Dict[str, object]] = []
    for key, rows in sorted(group_rows(boundary_rows, keys).items()):
        valid = []
        for row in rows:
            boundary = finite_float(row.get("boundary_aacgm_lat_deg"))
            if boundary is None:
                continue
            epoch = parse_utc(row["epoch_utc"])
            valid.append((epoch, abs(boundary), interpolate_driver(driver, epoch)))
        # Restrict the main branch to storm development.  Pre-event quiet cells
        # can share SYM-H with late recovery but are not part of the hysteresis
        # loop and would bias the matched contrast toward zero.
        main = [item for item in valid if compression <= item[0] < main_phase]
        recovery = [item for item in valid if item[0] > main_phase]
        available = set(range(len(recovery)))
        for main_epoch, main_lat, main_driver in sorted(main, reverse=True):
            candidates = []
            for index in available:
                rec_epoch, rec_lat, rec_driver = recovery[index]
                symh_difference = abs(rec_driver.symh_nt - main_driver.symh_nt)
                pdyn_fraction = abs(rec_driver.pdyn_npa - main_driver.pdyn_npa) / max(
                    1.0e-9, abs(main_driver.pdyn_npa)
                )
                bz_difference = abs(rec_driver.bz_nt - main_driver.bz_nt)
                if symh_difference <= symh_tol:
                    score = symh_difference / symh_tol + pdyn_fraction / pdyn_tol + bz_difference / bz_tol
                    candidates.append((score, index, pdyn_fraction, bz_difference))
            if not candidates:
                continue
            _, index, pdyn_fraction, bz_difference = min(candidates)
            available.remove(index)
            rec_epoch, rec_lat, rec_driver = recovery[index]
            strict = pdyn_fraction <= pdyn_tol and bz_difference <= bz_tol
            pairs.append({
                "altitude_km": float(key[0]), "rigidity_gv": float(key[1]),
                "hemisphere": key[2], "mlt_hour": float(key[3]),
                "main_epoch_utc": format_utc(main_epoch),
                "recovery_epoch_utc": format_utc(rec_epoch),
                "main_boundary_deg": main_lat, "recovery_boundary_deg": rec_lat,
                "recovery_minus_main_deg": rec_lat - main_lat,
                "main_symh_nt": main_driver.symh_nt,
                "recovery_symh_nt": rec_driver.symh_nt,
                "delta_symh_nt": rec_driver.symh_nt - main_driver.symh_nt,
                "pdyn_fractional_difference": pdyn_fraction,
                "bz_absolute_difference_nt": bz_difference,
                "strict_instantaneous_driver_match": strict,
            })

    summaries: List[Dict[str, object]] = []
    summary_groups = group_rows(
        [{key: str(value) for key, value in row.items()} for row in pairs],
        ("altitude_km", "rigidity_gv", "hemisphere"),
    )
    for key, rows in sorted(summary_groups.items()):
        for label, selected in (
            ("SYMH_ONLY", rows),
            ("STRICT", [row for row in rows if row["strict_instantaneous_driver_match"] == "True"]),
        ):
            values = [float(row["recovery_minus_main_deg"]) for row in selected]
            boot = []
            n_unique_main_epochs = len({row["main_epoch_utc"] for row in selected})
            # Mirror the lag-analysis policy: preserve sparse matched pairs as
            # diagnostics, but do not attach a bootstrap confidence interval
            # until the event is sampled densely enough for temporal inference.
            if values and n_unique_main_epochs >= MIN_TEMPORAL_EPOCHS_FOR_INFERENCE:
                for _ in range(replicates):
                    boot.append(statistics.median(rng.choices(values, k=len(values))))
            summaries.append({
                "altitude_km": float(key[0]), "rigidity_gv": float(key[1]),
                "hemisphere": key[2], "match_definition": label,
                "n_pairs": len(values),
                "n_unique_main_epochs": n_unique_main_epochs,
                "median_recovery_minus_main_deg": statistics.median(values) if values else None,
                "bootstrap_ci_low": quantile(boot, 0.025) if boot else None,
                "bootstrap_ci_high": quantile(boot, 0.975) if boot else None,
                "inference_status": (
                    "AVAILABLE"
                    if n_unique_main_epochs >= MIN_TEMPORAL_EPOCHS_FOR_INFERENCE
                    else "DIAGNOSTIC_ONLY"
                ),
            })
    return pairs, summaries


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path)
    parser.add_argument(
        "--morphology-root", type=Path,
        default=default_output_root() / "morphology",
    )
    parser.add_argument(
        "--output-root", type=Path, default=default_output_root() / "dynamics"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root, config = load_config(args.config)
    morphology_root = resolve_output_path(args.morphology_root)
    boundary_path = morphology_root / "morphology_boundaries.csv"
    if not boundary_path.exists():
        raise SystemExit(f"missing morphology product: {boundary_path}")
    rows = read_csv(boundary_path)
    driver = read_driver(root / config["data"]["driver"])  # type: ignore[index]
    landmarks = json.loads((morphology_root / "event_landmarks.json").read_text())
    compression = parse_utc(landmarks["compression"])
    main_phase = parse_utc(landmarks["main_phase"])

    harmonics, time_series = morphology_products(rows, config)
    boundary_cells = boundary_cell_products(
        rows, driver, compression, main_phase, config
    )
    altitude_response = altitude_response_products(time_series)
    extrema = storm_extrema_products(time_series)
    recovery = recovery_products(time_series, main_phase)
    cutoff_map_changes, cutoff_map_evolution, cutoff_map_summary = (
        cutoff_map_change_products(
            morphology_root,
            parse_utc(config["event"]["compression_search_start_utc"]),  # type: ignore[index]
            compression,
        )
    )
    lags = lag_products(harmonics, driver, config)
    best_lags = best_lag_products(lags)
    pairs, hysteresis = hysteresis_products(
        rows, driver, compression, main_phase, config
    )
    output = resolve_output_path(args.output_root)
    output.mkdir(parents=True, exist_ok=True)
    availability = analysis_availability_products(
        rows, time_series, altitude_response, hysteresis, recovery,
        cutoff_map_changes, output.parent
    )
    write_csv(output / "morphology_harmonics.csv", harmonics)
    write_csv(output / "cutoff_dynamics_timeseries.csv", time_series)
    write_csv(output / "boundary_cell_dynamics.csv", boundary_cells)
    write_csv(output / "altitude_response.csv", altitude_response)
    write_csv(output / "storm_extrema_summary.csv", extrema)
    write_csv(output / "recovery_timescales.csv", recovery)
    write_csv(output / "cutoff_map_event_change.csv", cutoff_map_changes)
    write_csv(output / "cutoff_map_change_timeseries.csv", cutoff_map_evolution)
    (output / "cutoff_map_change_summary.json").write_text(
        json.dumps(cutoff_map_summary, indent=2) + "\n", encoding="utf-8"
    )
    write_csv(output / "lag_correlations.csv", lags)
    write_csv(output / "best_lag_summary.csv", best_lags)
    write_csv(output / "hysteresis_pairs.csv", pairs)
    write_csv(output / "hysteresis_summary.csv", hysteresis)
    write_csv(output / "analysis_availability.csv", availability)
    availability_json = {
        row["analysis"]: {
            "status": row["status"], "n_epochs": row["n_epochs"],
            "evidence": row["evidence"],
            "interpretation": row["interpretation"],
        }
        for row in availability
    }
    (output / "analysis_availability.json").write_text(
        json.dumps(availability_json, indent=2) + "\n", encoding="utf-8"
    )
    status_counts = {
        status: sum(row["status"] == status for row in availability)
        for status in ("AVAILABLE", "DIAGNOSTIC_ONLY", "NOT_AVAILABLE")
    }
    result = {
        "n_input_boundary_rows": len(rows), "n_harmonic_rows": len(harmonics),
        "n_unique_epochs": len({row["epoch_utc"] for row in rows}),
        "minimum_temporal_epochs_for_inference": MIN_TEMPORAL_EPOCHS_FOR_INFERENCE,
        "n_boundary_cell_rows": len(boundary_cells),
        "n_altitude_response_rows": len(altitude_response),
        "n_storm_extrema_rows": len(extrema),
        "n_recovery_rows": len(recovery),
        "n_cutoff_map_event_change_cells": len(cutoff_map_changes),
        "n_cutoff_map_change_timeseries_rows": len(cutoff_map_evolution),
        "n_lag_rows": len(lags), "n_hysteresis_pairs": len(pairs),
        "n_best_lag_rows": len(best_lags),
        "n_hysteresis_summary_rows": len(hysteresis),
        "analysis_status_counts": status_counts,
        "event_landmarks": landmarks,
    }
    (output / "dynamics_result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
