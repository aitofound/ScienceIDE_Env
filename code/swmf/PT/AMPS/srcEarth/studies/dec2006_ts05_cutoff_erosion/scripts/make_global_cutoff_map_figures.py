#!/usr/bin/env python3
"""Render global cutoff maps and storm-erosion figures from archived tables.

This plotting layer performs no trajectory classification and no cutoff
inversion.  It consumes the canonical full-sphere maps and event-change tables
written by ``postprocess_global_cutoff_maps.py``.  Keeping that separation makes
all publication graphics reproducible without rerunning AMPS.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import pandas as pd

from make_figures import cutoff_change_figures, epoch_cutoff_rigidity_maps
from study_common import default_output_root, resolve_output_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--postprocessing-root", type=Path,
        default=default_output_root() / "global_maps" / "postprocessing",
    )
    parser.add_argument(
        "--output-root", type=Path,
        default=default_output_root() / "global_maps" / "figures",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    postprocessing = resolve_output_path(args.postprocessing_root)
    output = resolve_output_path(args.output_root)
    canonical = postprocessing / "canonical_maps"
    output.mkdir(parents=True, exist_ok=True)

    # One multi-panel PNG per epoch is intentionally kept separate from the
    # compact publication summaries.  A FULL run can contain hundreds of
    # epochs, for which EPS copies of every individual map would be needlessly
    # large.  The event summaries remain available in all three formats.
    epoch_products, panel_records = epoch_cutoff_rigidity_maps(canonical, output)
    summary_products = cutoff_change_figures(
        postprocessing / "global_cutoff_event_change.csv",
        postprocessing / "global_cutoff_change_timeseries.csv",
        output,
    )

    panel_manifest = output / "global_cutoff_map_figure_manifest.csv"
    if panel_records:
        pd.DataFrame(panel_records).to_csv(panel_manifest, index=False)

    source_manifest_path = canonical / "cutoff_rigidity_map_manifest.csv"
    source_count = 0
    if source_manifest_path.is_file() and source_manifest_path.stat().st_size > 0:
        source_count = len(pd.read_csv(source_manifest_path))
    expected_summary = {
        output / f"{stem}{suffix}"
        for stem in (
            "figure_maximum_cutoff_decrease_map",
            "figure_cutoff_decrease_evolution",
        )
        for suffix in (".png", ".eps", ".pdf")
    }
    missing = [str(path) for path in sorted(expected_summary) if not path.is_file()]
    if source_count == 0 or len(panel_records) != source_count:
        missing.append(
            "rendered shell/epoch panel count does not match the canonical map manifest"
        )
    if not epoch_products or any(not path.is_file() for path in epoch_products):
        missing.append("one or more per-epoch global cutoff-map PNG files are absent")

    result = {
        "postprocessing_root": str(postprocessing),
        "output_root": str(output),
        "n_source_shell_epoch_maps": source_count,
        "n_rendered_shell_epoch_panels": len(panel_records),
        "n_epoch_map_png": len(epoch_products),
        "epoch_map_png": [str(path) for path in epoch_products],
        "summary_products": [str(path) for path in summary_products],
        "missing_required_products": missing,
        "passed": not missing,
    }
    (output / "global_cutoff_figure_result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2), flush=True)
    return 0 if not missing else 1


if __name__ == "__main__":
    raise SystemExit(main())
