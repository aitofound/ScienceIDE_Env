#!/usr/bin/env python3
"""Convert Athena++ formatted primitive TAB files into the Harbor artifact."""
from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path

VARIABLES = ["x1", "x2", "x3", "rho", "press", "vel1", "vel2", "vel3"]
PRIMITIVES = VARIABLES[3:]
FRAME_RE = re.compile(r"\.(\d{5})\.tab$")
TIME_RE = re.compile(r"time=([^ ]+)")
CYCLE_RE = re.compile(r"cycle=([-+]?\d+)")


def finite(value: float) -> float:
    if not math.isfinite(value):
        raise ValueError("non-finite TAB value")
    return value


def parse_file(path: Path, require_all_coordinates: bool = False) -> tuple[int, float, int, list[list[float]]]:
    match = FRAME_RE.search(path.name)
    if not match:
        raise ValueError(f"unexpected TAB filename: {path.name}")
    frame_number = int(match.group(1))
    lines = path.read_text(encoding="utf-8").splitlines()
    if len(lines) < 3 or not lines[0].startswith("# Athena++ data at "):
        raise ValueError(f"missing Athena TAB header: {path.name}")
    time_match = TIME_RE.search(lines[0])
    cycle_match = CYCLE_RE.search(lines[0])
    if not time_match or not cycle_match or "variables=prim" not in lines[0]:
        raise ValueError(f"invalid Athena TAB metadata: {path.name}")
    time = finite(float(time_match.group(1)))
    cycle = int(cycle_match.group(1))
    header = lines[1].lstrip("#").split()
    try:
        columns = {name: index for index, name in enumerate(header)}
    except Exception as exc:
        raise ValueError(f"invalid TAB column header: {path.name}") from exc
    required = {"x1v", "rho", "press", "vel1", "vel2", "vel3"}
    if require_all_coordinates:
        required.update({"x2v", "x3v"})
    if not required.issubset(columns):
        raise ValueError(f"TAB does not contain complete primitive columns: {path.name}")
    rows: list[list[float]] = []
    for line in lines[2:]:
        if not line.strip():
            continue
        tokens = line.split()
        try:
            def number(name: str) -> float:
                return finite(float(tokens[columns[name]]))
            # Athena omits constant dimensions from a 1-D/2-D table.  Their
            # cell-center coordinates are exactly zero for the check decks.
            row = [
                number("x1v"),
                number("x2v") if "x2v" in columns else 0.0,
                number("x3v") if "x3v" in columns else 0.0,
                number("rho"), number("press"), number("vel1"),
                number("vel2"), number("vel3"),
            ]
        except (IndexError, KeyError, ValueError) as exc:
            raise ValueError(f"invalid TAB row in {path.name}") from exc
        rows.append(row)
    if not rows:
        raise ValueError(f"TAB has no data rows: {path.name}")
    return frame_number, time, cycle, rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="directory containing native .tab files")
    parser.add_argument("--output", required=True, type=Path, help="Harbor primitive_tab.json destination")
    parser.add_argument("--case", required=True)
    parser.add_argument("--dimensions", required=True, help="nx1,nx2,nx3")
    args = parser.parse_args()
    dimensions = [int(value) for value in args.dimensions.split(",")]
    if len(dimensions) != 3 or any(value <= 0 for value in dimensions):
        raise SystemExit("--dimensions must be three positive integers")
    expected_rows = dimensions[0] * dimensions[1] * dimensions[2]
    require_all_coordinates = dimensions[1] > 1 or dimensions[2] > 1
    parsed: dict[int, tuple[float, int, list[list[float]]]] = {}
    paths = sorted(args.input.glob("*.tab"))
    if not paths:
        raise SystemExit("no native Athena TAB files found")
    for path in paths:
        frame, time, cycle, rows = parse_file(path, require_all_coordinates=require_all_coordinates)
        if frame in parsed:
            old_time, old_cycle, old_rows = parsed[frame]
            if old_time != time or old_cycle != cycle:
                raise SystemExit(f"conflicting metadata for frame {frame:05d}")
            old_rows.extend(rows)
        else:
            parsed[frame] = (time, cycle, rows)
    frames = []
    for frame_number in sorted(parsed):
        time, cycle, rows = parsed[frame_number]
        if len(rows) != expected_rows:
            raise SystemExit(f"frame {frame_number:05d} has {len(rows)} rows, expected {expected_rows}")
        frames.append({"time": time, "cycle": cycle, "rows": rows})
    document = {
        "schema": "athena-hydro-primitive-tab/v1",
        "case": args.case,
        "dimensions": dimensions,
        "variables": VARIABLES,
        "primitive_fields": PRIMITIVES,
        "source_format": {
            "encoding": "UTF-8 JSON converted from Athena++ formatted TAB",
            "dtype": "float64",
            "ordering": "frame then native k-j-i TAB row order",
            "native_file_pattern": "<problem>.block<gid>.out<id>.<frame>.tab",
        },
        "frames": frames,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, allow_nan=False, separators=(",", ":")) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
