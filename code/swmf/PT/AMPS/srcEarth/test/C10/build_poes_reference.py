#!/usr/bin/env python3
"""Build the real-measurement C10 POES/MetOp cutoff-boundary reference.

This command consumes a local mirror of the NOAA/NCEI historical SEM-2 Level-2
16-second files.  It never substitutes an empirical or synthetic reference.
The output is split into three products:

``C10_poes_boundary_crossings.csv``
    The primary observational product.  Each row is one inbound or outbound
    satellite-pass crossing of 50 percent of the polar-cap channel flux.

``reference_C10_poes_meped_boundary.csv.gz``
    A two-hour-window, one-hour-step, 3-hour-MLT-bin table used by ``run_C10.py``.
    Empty cells are retained and explicitly marked ``missing=TRUE``.  P6/P7
    background-normalized cells on non-overlapping windows form the default
    code-validation gate; P8/P9 remain diagnostic until response folding is
    implemented.

``C10_reference_manifest.json``
    Source-file SHA-256 values, extraction settings, channel-to-rigidity mapping,
    and row counts.  This file is required for reproducibility and should be
    archived together with the compressed reference CSV.

The December-2006 literature provides the method, fitted model, and figures, but
not the complete pass-level numerical crossing list.  Consequently, a real C10
reference must be regenerated from the NCEI archive; digitizing a paper figure
would not be an equivalent observational reference.
"""
from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import List, Optional, Sequence

from poes_sem2 import (
    aggregate_crossings,
    extract_boundary_crossings,
    load_observations,
    sha256_file,
    summarize_cross_channel_quality,
    write_crossings_csv,
    write_manifest,
    write_reference_csv,
)

SUPPORTED_SUFFIXES = (".txt", ".txt.gz", ".asc", ".asc.gz", ".dat", ".dat.gz", ".cdf")


def parse_utc(text: str) -> datetime:
    """Parse a command-line UTC timestamp and normalize it to UTC."""

    value = text.strip().replace("Z", "+00:00")
    result = datetime.fromisoformat(value)
    if result.tzinfo is None:
        result = result.replace(tzinfo=timezone.utc)
    return result.astimezone(timezone.utc)


def discover_source_files(input_dir: Path) -> List[Path]:
    """Recursively find supported Level-2 files while excluding manifests.

    Zero-byte files are returned so ``main`` can report and record them before
    passing only non-empty products to the scientific parser.
    """

    files = [
        path for path in input_dir.rglob("*")
        if path.is_file()
        and path.name.lower() != "download_manifest.json"
        and any(path.name.lower().endswith(suffix) for suffix in SUPPORTED_SUFFIXES)
    ]
    return sorted(files)


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input-dir", type=Path, default=Path("data/reference_source"),
                        help="Directory containing downloaded NCEI Level-2 files")
    parser.add_argument("--event-start", type=parse_utc, default=parse_utc("2006-12-14T00:00:00Z"))
    parser.add_argument("--event-end", type=parse_utc, default=parse_utc("2006-12-16T12:00:00Z"))
    parser.add_argument("--default-altitude-km", type=float, default=850.0,
                        help="Used only when a Level-2 product lacks an altitude column")
    parser.add_argument("--minimum-pass-abs-lat-deg", type=float, default=45.0)
    parser.add_argument("--polar-plateau-abs-lat-deg", type=float, default=75.0)
    parser.add_argument("--minimum-polar-samples", type=int, default=4)
    parser.add_argument("--minimum-background-samples", type=int, default=3)
    parser.add_argument("--minimum-plateau-to-low-ratio", type=float, default=2.0)
    parser.add_argument("--minimum-leg-samples", type=int, default=4)
    parser.add_argument(
        "--crossing-method", type=str.upper,
        choices=("BACKGROUND_NORMALIZED_ISOTONIC", "HALF_POLAR_PLATEAU"),
        default="BACKGROUND_NORMALIZED_ISOTONIC",
        help="Production background-corrected T50 or legacy diagnostic threshold",
    )
    parser.add_argument("--maximum-transition-width-deg", type=float, default=15.0)
    parser.add_argument("--maximum-isotonic-rms", type=float, default=0.35)
    parser.add_argument("--minimum-edge-margin-deg", type=float, default=0.5)
    parser.add_argument("--window-hours", type=float, default=2.0,
                        help="Published Dmitriev-style fit/aggregation window")
    parser.add_argument("--step-hours", type=float, default=1.0,
                        help="Step between adjacent reference-window centers")
    parser.add_argument("--mlt-bin-hours", type=float, default=3.0)
    parser.add_argument("--minimum-crossings-per-cell", type=int, default=2,
                        help="Minimum crossings for primary P6/P7 cells")
    parser.add_argument("--minimum-diagnostic-crossings-per-cell", type=int, default=2,
                        help="Minimum quality-eligible P8/P9 crossings for a robust diagnostic cell")
    parser.add_argument("--minimum-distinct-pass-legs-per-cell", type=int, default=2,
                        help="Minimum independent pass legs for primary P6/P7 cells")
    parser.add_argument("--minimum-diagnostic-distinct-pass-legs-per-cell", type=int, default=2)
    parser.add_argument("--minimum-diagnostic-distinct-satellites-per-cell", type=int, default=2)
    parser.add_argument("--minimum-primary-transition-samples", type=int, default=2)
    parser.add_argument("--minimum-diagnostic-transition-samples", type=int, default=3)
    parser.add_argument("--minimum-diagnostic-contrast-to-noise", type=float, default=3.0,
                        help="Robust plateau-minus-background contrast/noise ratio for P8/P9")
    parser.add_argument("--p8-p9-outlier-sigma", type=float, default=4.0)
    parser.add_argument("--p8-p9-minimum-pairs", type=int, default=6)
    parser.add_argument("--p8-p9-fallback-max-separation-deg", type=float, default=6.0)
    parser.add_argument("--acceptance-window-stride-hours", type=float, default=2.0,
                        help="Non-overlapping midpoint stride used by the validation gate")
    parser.add_argument("--crossings-output", type=Path, default=Path("C10_poes_boundary_crossings.csv"))
    parser.add_argument(
        "--reference-output", type=Path,
        default=Path("reference_C10_poes_meped_boundary.csv.gz"),
        help="gzip-compressed CSV consumed directly by run_C10.py; must end in .csv.gz",
    )
    parser.add_argument("--manifest-output", type=Path, default=Path("C10_reference_manifest.json"))
    parser.add_argument("--summary-output", type=Path, default=Path("C10_reference_summary.json"))
    parser.add_argument("--validate-only", action="store_true",
                        help="Read and extract data but do not overwrite output files")
    args = parser.parse_args(argv)
    if args.event_end <= args.event_start:
        parser.error("--event-end must be later than --event-start")
    if args.mlt_bin_hours <= 0.0 or 24.0 / args.mlt_bin_hours < 1.0:
        parser.error("--mlt-bin-hours must define at least one bin")
    if not args.reference_output.name.lower().endswith(".csv.gz"):
        parser.error(
            "--reference-output must end in .csv.gz; "
            "the production C10 reference is kept gzip-compressed"
        )
    if args.minimum_polar_samples < 1:
        parser.error("--minimum-polar-samples must be >= 1")
    if args.minimum_background_samples < 1:
        parser.error("--minimum-background-samples must be >= 1")
    if args.minimum_leg_samples < 2:
        parser.error("--minimum-leg-samples must be >= 2")
    if args.minimum_crossings_per_cell < 1:
        parser.error("--minimum-crossings-per-cell must be >= 1")
    if args.minimum_diagnostic_crossings_per_cell < 1:
        parser.error("--minimum-diagnostic-crossings-per-cell must be >= 1")
    if args.minimum_distinct_pass_legs_per_cell < 1:
        parser.error("--minimum-distinct-pass-legs-per-cell must be >= 1")
    if args.minimum_diagnostic_distinct_pass_legs_per_cell < 1:
        parser.error("--minimum-diagnostic-distinct-pass-legs-per-cell must be >= 1")
    if args.minimum_diagnostic_distinct_satellites_per_cell < 1:
        parser.error("--minimum-diagnostic-distinct-satellites-per-cell must be >= 1")
    if args.minimum_primary_transition_samples < 1 or args.minimum_diagnostic_transition_samples < 1:
        parser.error("minimum transition-sample requirements must be >= 1")
    if args.minimum_diagnostic_contrast_to_noise < 0.0:
        parser.error("--minimum-diagnostic-contrast-to-noise must be >= 0")
    if args.p8_p9_outlier_sigma < 0.0 or args.p8_p9_minimum_pairs < 1:
        parser.error("P8/P9 robust outlier settings are invalid")
    if args.p8_p9_fallback_max_separation_deg <= 0.0:
        parser.error("--p8-p9-fallback-max-separation-deg must be > 0")
    if args.window_hours <= 0.0 or args.step_hours <= 0.0:
        parser.error("--window-hours and --step-hours must be > 0")
    if args.acceptance_window_stride_hours <= 0.0:
        parser.error("--acceptance-window-stride-hours must be > 0")
    if args.maximum_transition_width_deg <= 0.0:
        parser.error("--maximum-transition-width-deg must be > 0")
    if args.maximum_isotonic_rms < 0.0:
        parser.error("--maximum-isotonic-rms must be >= 0")
    if args.minimum_edge_margin_deg < 0.0:
        parser.error("--minimum-edge-margin-deg must be >= 0")
    if args.crossing_method == "HALF_POLAR_PLATEAU":
        print(
            "warning: HALF_POLAR_PLATEAU is a legacy diagnostic and does not produce "
            "acceptance-eligible reference cells", file=sys.stderr,
        )
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    discovered_source_files = discover_source_files(args.input_dir)
    if not discovered_source_files:
        print(
            f"No supported Level-2 files were found below {args.input_dir}.\n"
            "Run download_poes_sem2.py first or copy the NCEI daily files into that directory.",
            file=sys.stderr,
        )
        return 2

    empty_source_files = [path for path in discovered_source_files if path.stat().st_size == 0]
    source_files = [path for path in discovered_source_files if path.stat().st_size > 0]
    for path in empty_source_files:
        print(f"warning: ignoring zero-byte Level-2 file: {path}", file=sys.stderr)
    if not source_files:
        print(
            "All discovered Level-2 files are empty. Remove them and rerun "
            "download_poes_sem2.py --overwrite.",
            file=sys.stderr,
        )
        return 2

    print(
        f"Reading {len(source_files)} non-empty NCEI Level-2 files"
        f" ({len(empty_source_files)} empty file(s) skipped)..."
    )
    observations = load_observations(source_files, args.default_altitude_km)
    observations = [row for row in observations if args.event_start <= row.time_utc <= args.event_end]
    if not observations:
        print("No valid observations fall inside the requested event interval.", file=sys.stderr)
        return 2

    crossings = extract_boundary_crossings(
        observations,
        minimum_abs_lat_deg=args.minimum_pass_abs_lat_deg,
        polar_plateau_abs_lat_deg=args.polar_plateau_abs_lat_deg,
        minimum_polar_samples=args.minimum_polar_samples,
        minimum_background_samples=args.minimum_background_samples,
        minimum_plateau_to_low_ratio=args.minimum_plateau_to_low_ratio,
        minimum_leg_samples=args.minimum_leg_samples,
        crossing_method=args.crossing_method,
        maximum_transition_width_deg=args.maximum_transition_width_deg,
        maximum_isotonic_rms=args.maximum_isotonic_rms,
        minimum_edge_margin_deg=args.minimum_edge_margin_deg,
        minimum_primary_transition_samples=args.minimum_primary_transition_samples,
        minimum_diagnostic_transition_samples=args.minimum_diagnostic_transition_samples,
        minimum_diagnostic_contrast_to_noise=args.minimum_diagnostic_contrast_to_noise,
        p8_p9_outlier_sigma=args.p8_p9_outlier_sigma,
        p8_p9_minimum_pairs=args.p8_p9_minimum_pairs,
        p8_p9_fallback_max_separation_deg=args.p8_p9_fallback_max_separation_deg,
    )
    if not crossings:
        print(
            "No boundary crossings were extracted. Inspect the source columns and relax quality "
            "settings only after confirming that the selected days contain an SEP enhancement.",
            file=sys.stderr,
        )
        return 3

    n_bins = max(1, round(24.0 / args.mlt_bin_hours))
    mlt_bins = tuple(round(index * 24.0 / n_bins, 6) for index in range(n_bins))
    cells = aggregate_crossings(
        crossings,
        event_start=args.event_start,
        event_end=args.event_end,
        window_hours=args.window_hours,
        step_hours=args.step_hours,
        mlt_bin_centers=mlt_bins,
        minimum_crossings_per_cell=args.minimum_crossings_per_cell,
        minimum_diagnostic_crossings_per_cell=args.minimum_diagnostic_crossings_per_cell,
        minimum_distinct_pass_legs_per_cell=args.minimum_distinct_pass_legs_per_cell,
        minimum_diagnostic_distinct_pass_legs_per_cell=(
            args.minimum_diagnostic_distinct_pass_legs_per_cell),
        minimum_diagnostic_distinct_satellites_per_cell=(
            args.minimum_diagnostic_distinct_satellites_per_cell),
        acceptance_window_stride_hours=args.acceptance_window_stride_hours,
    )

    configuration = {
        "event_start_utc": args.event_start.isoformat(),
        "event_end_utc": args.event_end.isoformat(),
        "default_altitude_km": args.default_altitude_km,
        "minimum_pass_abs_lat_deg": args.minimum_pass_abs_lat_deg,
        "polar_plateau_abs_lat_deg": args.polar_plateau_abs_lat_deg,
        "minimum_polar_samples": args.minimum_polar_samples,
        "minimum_background_samples": args.minimum_background_samples,
        "minimum_plateau_to_low_ratio": args.minimum_plateau_to_low_ratio,
        "minimum_leg_samples": args.minimum_leg_samples,
        "crossing_method": args.crossing_method,
        "maximum_transition_width_deg": args.maximum_transition_width_deg,
        "maximum_isotonic_rms": args.maximum_isotonic_rms,
        "minimum_edge_margin_deg": args.minimum_edge_margin_deg,
        "window_hours": args.window_hours,
        "step_hours": args.step_hours,
        "mlt_bins": mlt_bins,
        "minimum_crossings_per_cell": args.minimum_crossings_per_cell,
        "minimum_diagnostic_crossings_per_cell": args.minimum_diagnostic_crossings_per_cell,
        "minimum_distinct_pass_legs_per_cell": args.minimum_distinct_pass_legs_per_cell,
        "minimum_diagnostic_distinct_pass_legs_per_cell": (
            args.minimum_diagnostic_distinct_pass_legs_per_cell),
        "minimum_diagnostic_distinct_satellites_per_cell": (
            args.minimum_diagnostic_distinct_satellites_per_cell),
        "minimum_primary_transition_samples": args.minimum_primary_transition_samples,
        "minimum_diagnostic_transition_samples": args.minimum_diagnostic_transition_samples,
        "minimum_diagnostic_contrast_to_noise": args.minimum_diagnostic_contrast_to_noise,
        "p8_p9_outlier_sigma": args.p8_p9_outlier_sigma,
        "p8_p9_minimum_pairs": args.p8_p9_minimum_pairs,
        "p8_p9_fallback_max_separation_deg": args.p8_p9_fallback_max_separation_deg,
        "acceptance_window_stride_hours": args.acceptance_window_stride_hours,
        "boundary_definition": "background_normalized_isotonic_T50",
        "validation_gate": "P6_P7_primary_independent_windows",
        "diagnostic_channels": ["P8", "P9"],
        "empty_source_files_skipped": [str(path) for path in empty_source_files],
        "papers_are_not_numerical_reference": True,
    }

    summary = {
        "n_source_files_discovered": len(discovered_source_files),
        "n_source_files": len(source_files),
        "n_empty_source_files_skipped": len(empty_source_files),
        "empty_source_files_skipped": [str(path) for path in empty_source_files],
        "n_observations": len(observations),
        "n_crossings": len(crossings),
        "n_reference_cells": len(cells),
        "n_nonmissing_reference_cells": sum(not cell.missing for cell in cells),
        "n_acceptance_eligible_cells": sum(cell.acceptance_eligible for cell in cells),
        "n_diagnostic_eligible_cells": sum(cell.diagnostic_eligible for cell in cells),
        "n_sparse_diagnostic_cells": sum(
            cell.quality_status in ("DIAGNOSTIC_SPARSE", "DIAGNOSTIC_CROSS_CHANNEL_OUTLIER")
            for cell in cells),
        "n_cross_channel_outlier_cells": sum(cell.n_cross_channel_outliers > 0 for cell in cells),
        "cross_channel_quality": summarize_cross_channel_quality(crossings),
        "n_primary_crossings": sum(row.validation_role == "PRIMARY" for row in crossings),
        "n_diagnostic_crossings": sum(row.validation_role == "DIAGNOSTIC" for row in crossings),
        "reference_compression": "gzip",
        "satellites": sorted({row.satellite for row in observations}),
        "channels": sorted({row.channel for row in crossings}),
        "first_observation_utc": min(row.time_utc for row in observations).isoformat(),
        "last_observation_utc": max(row.time_utc for row in observations).isoformat(),
    }
    print(json.dumps(summary, indent=2))

    if args.validate_only:
        return 0

    write_crossings_csv(crossings, args.crossings_output)
    manifest_sha = write_manifest(source_files, crossings, cells, configuration, args.manifest_output)
    write_reference_csv(cells, args.reference_output, manifest_sha)
    summary["crossings_output"] = str(args.crossings_output)
    summary["reference_output"] = str(args.reference_output)
    summary["manifest_output"] = str(args.manifest_output)
    summary["manifest_sha256"] = manifest_sha
    summary["reference_sha256"] = sha256_file(args.reference_output)
    args.summary_output.write_text(json.dumps(summary, indent=2) + "\n")

    print(f"crossings: {args.crossings_output}")
    print(f"reference: {args.reference_output}")
    print(f"manifest:  {args.manifest_output}")
    print(f"summary:   {args.summary_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
