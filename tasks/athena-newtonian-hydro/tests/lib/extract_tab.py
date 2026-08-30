#!/usr/bin/env python3
"""Convert Athena++ formatted primitive TAB files into the Harbor artifact.

Library and command line share one derivation so the verifier can rebuild the
artifact from native TAB output and compare it with the shipped file.  Row order
inside a frame is the global k-j-i order: cells sorted by their (x3, x2, x1)
cell-centre coordinates across all MeshBlocks.  For a single MeshBlock this is
exactly the native TAB row order.
"""
from __future__ import annotations

import argparse
import json
import math
import re
from dataclasses import dataclass
from pathlib import Path

SCHEMA = "athena-hydro-primitive-tab/v1"
VARIABLES = ["x1", "x2", "x3", "rho", "press", "vel1", "vel2", "vel3"]
PRIMITIVES = VARIABLES[3:]
SOURCE_FORMAT = {
    "encoding": "UTF-8 JSON converted from Athena++ formatted TAB",
    "dtype": "float64",
    "ordering": "frame then global k-j-i order: cells sorted by (x3v, x2v, x1v) across all MeshBlocks",
    "native_file_pattern": "<problem>.block<gid>.out<id>.<frame>.tab",
}
NAME_RE = re.compile(r"^(?P<base>.+)\.block(?P<gid>\d+)\.(?P<out>out\d+)\.(?P<frame>\d{5})\.tab$")
TIME_RE = re.compile(r"time=([^ ]+)")
CYCLE_RE = re.compile(r"cycle=([-+]?\d+)")


@dataclass
class TabFile:
    path: Path
    base: str
    gid: int
    out: str
    frame: int
    time: float
    cycle: int
    columns: list[str]
    rows: list[list[float]]  # eight columns; absent x2v/x3v are 0.0
    time_token: str = ""  # the exact "%e" token Athena++ printed for this frame


def finite(value: float) -> float:
    if not math.isfinite(value):
        raise ValueError("non-finite TAB value")
    return value


def parse_name(path: Path) -> tuple[str, int, str, int]:
    match = NAME_RE.match(path.name)
    if not match:
        raise ValueError(f"unexpected TAB filename: {path.name}")
    return match.group("base"), int(match.group("gid")), match.group("out"), int(match.group("frame"))


def parse_tab(path: Path) -> TabFile:
    base, gid, out, frame = parse_name(path)
    lines = path.read_text(encoding="utf-8").splitlines()
    if len(lines) < 3 or not lines[0].startswith("# Athena++ data at "):
        raise ValueError(f"missing Athena TAB header: {path.name}")
    time_match = TIME_RE.search(lines[0])
    cycle_match = CYCLE_RE.search(lines[0])
    if not time_match or not cycle_match or "variables=prim" not in lines[0]:
        raise ValueError(f"invalid Athena TAB metadata: {path.name}")
    # formatted_table.cpp:85 prints "time=%e"; keep the exact printed token so the
    # verifier can bind the schedule to the representation instead of a tolerance.
    time = finite(float(time_match.group(1)))
    cycle = int(cycle_match.group(1))
    if cycle < 0:
        raise ValueError(f"negative cycle in {path.name}")
    if not lines[1].startswith("#"):
        raise ValueError(f"missing TAB column header: {path.name}")
    header = lines[1].lstrip("#").split()
    if len(set(header)) != len(header):
        raise ValueError(f"duplicate TAB column in {path.name}")
    columns = {name: index for index, name in enumerate(header)}
    required = {"x1v", "rho", "press", "vel1", "vel2", "vel3"}
    if not required.issubset(columns):
        raise ValueError(f"TAB does not contain complete primitive columns: {path.name}")
    rows: list[list[float]] = []
    for line in lines[2:]:
        if not line.strip():
            continue
        if line.startswith("#"):
            raise ValueError(f"unexpected comment inside TAB data: {path.name}")
        tokens = line.split()
        if len(tokens) != len(header):
            raise ValueError(f"TAB row width mismatch in {path.name}")
        try:
            def number(name: str) -> float:
                return finite(float(tokens[columns[name]]))
            row = [
                number("x1v"),
                number("x2v") if "x2v" in columns else 0.0,
                number("x3v") if "x3v" in columns else 0.0,
                number("rho"), number("press"), number("vel1"), number("vel2"), number("vel3"),
            ]
        except (IndexError, KeyError, ValueError) as exc:
            raise ValueError(f"invalid TAB row in {path.name}") from exc
        rows.append(row)
    if not rows:
        raise ValueError(f"TAB has no data rows: {path.name}")
    return TabFile(path, base, gid, out, frame, time, cycle, header, rows, time_match.group(1))


def derive_document(tabs: list[TabFile], case: str, dimensions: list[int], expected_rows: int) -> dict:
    """Build the artifact from parsed TAB files; frames are consecutive from 0."""
    if len(dimensions) != 3 or any(value <= 0 for value in dimensions):
        raise ValueError("dimensions must be three positive integers")
    if expected_rows <= 0:
        raise ValueError("expected_rows must be positive")
    frames: dict[int, list[TabFile]] = {}
    for tab in tabs:
        frames.setdefault(tab.frame, []).append(tab)
    if not frames:
        raise ValueError("no native Athena TAB files found")
    if sorted(frames) != list(range(len(frames))):
        raise ValueError("native frames are not consecutive from 00000")
    output = []
    for frame_number in sorted(frames):
        members = sorted(frames[frame_number], key=lambda tab: tab.gid)
        gids = [tab.gid for tab in members]
        if gids != list(range(len(members))):
            raise ValueError(f"frame {frame_number:05d} MeshBlock ids are not 0..{len(members) - 1}")
        times = {tab.time for tab in members}
        cycles = {tab.cycle for tab in members}
        if len(times) != 1 or len(cycles) != 1:
            raise ValueError(f"conflicting metadata for frame {frame_number:05d}")
        rows = [row for tab in members for row in tab.rows]
        if len(rows) != expected_rows:
            raise ValueError(f"frame {frame_number:05d} has {len(rows)} rows, expected {expected_rows}")
        rows.sort(key=lambda row: (row[2], row[1], row[0]))
        for previous, current in zip(rows, rows[1:]):
            if previous[0] == current[0] and previous[1] == current[1] and previous[2] == current[2]:
                raise ValueError(f"frame {frame_number:05d} contains duplicate cell centres")
        output.append({"time": times.pop(), "cycle": cycles.pop(), "rows": rows})
    return {
        "schema": SCHEMA,
        "case": case,
        "dimensions": list(dimensions),
        "variables": VARIABLES,
        "primitive_fields": PRIMITIVES,
        "source_format": dict(SOURCE_FORMAT),
        "frames": output,
    }


def serialize(document: dict) -> str:
    return json.dumps(document, allow_nan=False, separators=(",", ":")) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="directory containing native .tab files")
    parser.add_argument("--output", required=True, type=Path, help="Harbor primitive_tab.json destination")
    parser.add_argument("--case", required=True)
    parser.add_argument("--dimensions", required=True, help="root mesh nx1,nx2,nx3")
    parser.add_argument("--expected-rows", type=int, default=None, help="cells per frame (default: product of dimensions)")
    args = parser.parse_args()
    dimensions = [int(value) for value in args.dimensions.split(",")]
    if len(dimensions) != 3 or any(value <= 0 for value in dimensions):
        raise SystemExit("--dimensions must be three positive integers")
    expected_rows = args.expected_rows if args.expected_rows is not None else dimensions[0] * dimensions[1] * dimensions[2]
    paths = sorted(path for path in args.input.iterdir() if path.is_file() and not path.is_symlink() and path.suffix == ".tab")
    if not paths:
        raise SystemExit("no native Athena TAB files found")
    try:
        tabs = [parse_tab(path) for path in paths]
        document = derive_document(tabs, args.case, dimensions, expected_rows)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(serialize(document), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
