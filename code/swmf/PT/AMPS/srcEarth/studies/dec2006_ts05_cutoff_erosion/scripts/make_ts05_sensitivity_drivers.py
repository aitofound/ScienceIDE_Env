#!/usr/bin/env python3
"""Create the two controlled TS05 driver perturbations used by the paper.

The generated files are *not* alternative observations and are not physically
self-consistent solar-wind histories.  They isolate which part of the TS05
parameterization produces modeled memory:

``history_frozen``
    Keep measured instantaneous inputs but replace W1--W6 by their quiet-time
    values.

``instantaneous_frozen``
    Keep W1--W6 time dependent but hold IMF, plasma, SYM-H, tilt, and dynamic
    pressure at the quiet epoch.

Each output retains the original cadence and epoch column and receives a JSON
manifest that states every modified field and SHA-256 digest.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, Iterable, List

from run_morphology import event_landmarks
from study_common import (
    DRIVER_FIELDS, DriverRow, default_output_root, format_utc,
    interpolate_driver, load_config, read_driver, resolve_output_path, sha256,
)


def format_row(row: DriverRow) -> str:
    """Write the exact AMPS 20-column order with stable numerical precision."""
    values = []
    for name in DRIVER_FIELDS:
        value = getattr(row, name)
        if name in ("imf_flag", "sw_flag"):
            values.append(str(int(value)))
        else:
            values.append(f"{float(value):.10g}")
    return format_utc(row.epoch).removesuffix("Z") + " " + " ".join(values)


def replace_fields(row: DriverRow, source: DriverRow, fields: Iterable[str]) -> DriverRow:
    selected = set(fields)
    values = [getattr(source if name in selected else row, name) for name in DRIVER_FIELDS]
    return DriverRow(row.epoch, *values)


def write_driver(path: Path, rows: List[DriverRow], description: str) -> None:
    header = (
        f"# {path.name} -- {description}\n"
        "# CONTROLLED TS05 SENSITIVITY DRIVER; NOT AN OBSERVATIONAL DATA PRODUCT\n"
        "# YYYY-MM-DDTHH:MM:SS Bx By Bz Vx Vy Vz Np Temp SYM-H IMFflag SWflag Tilt Pdyn W1 W2 W3 W4 W5 W6\n"
    )
    path.write_text(header + "\n".join(format_row(row) for row in rows) + "\n",
                    encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--output-root", type=Path,
                        default=default_output_root() / "ts05_sensitivity" / "drivers")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root, config = load_config(args.config)
    source_path = root / config["data"]["driver"]
    rows = read_driver(source_path)
    landmarks = event_landmarks(rows, config)
    quiet = interpolate_driver(rows, landmarks["quiet"])
    output = resolve_output_path(args.output_root)
    output.mkdir(parents=True, exist_ok=True)

    history_fields = ("w1", "w2", "w3", "w4", "w5", "w6")
    instantaneous_fields = (
        "bx_nt", "by_nt", "bz_nt", "vx_km_s", "vy_km_s", "vz_km_s",
        "density_cm3", "temperature_k", "symh_nt", "tilt_rad", "pdyn_npa",
    )
    history_path = output / "ts05_dec2006_history_frozen.txt"
    instant_path = output / "ts05_dec2006_instantaneous_frozen.txt"
    write_driver(history_path,
                 [replace_fields(row, quiet, history_fields) for row in rows],
                 "W1-W6 fixed at the objective quiet epoch")
    write_driver(instant_path,
                 [replace_fields(row, quiet, instantaneous_fields) for row in rows],
                 "instantaneous IMF/plasma/SYM-H/tilt/Pdyn fixed; W1-W6 evolve")
    manifest = {
        "source_driver": str(source_path), "source_sha256": sha256(source_path),
        "quiet_epoch_utc": format_utc(quiet.epoch),
        "warning": "Controlled empirical-model perturbations; not self-consistent magnetospheres",
        "drivers": {
            "history_frozen": {
                "path": str(history_path), "sha256": sha256(history_path),
                "modified_fields": list(history_fields),
            },
            "instantaneous_frozen": {
                "path": str(instant_path), "sha256": sha256(instant_path),
                "modified_fields": list(instantaneous_fields),
            },
        },
    }
    (output / "sensitivity_driver_manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
