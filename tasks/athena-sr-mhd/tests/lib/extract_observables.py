#!/usr/bin/env python3
"""Convert native Athena++ TAB output into a strict SR-MHD observable artifact.

The native TAB is the incumbent's public output.  Cell-centred Bcc channels are
retained verbatim; face-field, corner-EMF, and divB records are explicitly
labelled derived projections because the pinned formatted-table writer does not
serialize internal FaceField/EdgeField arrays.
"""
from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path
from typing import Any

VARIABLES = ["x1", "x2", "x3", "rho", "press", "vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3"]
SCHEMA = "athena-sr-mhd-observables/v1"
FRAME_RE = re.compile(r"\.(\d{5})\.tab$")
TIME_RE = re.compile(r"time=([^ ]+)")
CYCLE_RE = re.compile(r"cycle=([-+]?\d+)")


def finite(value: float) -> float:
    if not math.isfinite(value):
        raise ValueError("non-finite TAB value")
    return value


def parse_tab(path: Path) -> tuple[int, float, int, list[list[float]]]:
    match = FRAME_RE.search(path.name)
    if not match:
        raise ValueError(f"unexpected TAB filename: {path.name}")
    lines = path.read_text(encoding="utf-8").splitlines()
    if len(lines) < 3 or not lines[0].startswith("# Athena++ data at "):
        raise ValueError(f"missing Athena TAB header: {path.name}")
    tm, cm = TIME_RE.search(lines[0]), CYCLE_RE.search(lines[0])
    if not tm or not cm or "variables=prim" not in lines[0]:
        raise ValueError(f"TAB must be primitive output with time/cycle metadata: {path.name}")
    header = lines[1].lstrip("#").split()
    columns = {name: index for index, name in enumerate(header)}
    required = {"x1v", "rho", "press", "vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3"}
    if not required.issubset(columns):
        raise ValueError(f"TAB lacks complete SR-MHD primitive/magnetic channels: {path.name}")
    rows: list[list[float]] = []
    for line in lines[2:]:
        if not line.strip():
            continue
        tokens = line.split()
        try:
            def number(name: str) -> float:
                return finite(float(tokens[columns[name]]))
            rows.append([
                number("x1v"), number("x2v") if "x2v" in columns else 0.0,
                number("x3v") if "x3v" in columns else 0.0,
                number("rho"), number("press"), number("vel1"), number("vel2"),
                number("vel3"), number("Bcc1"), number("Bcc2"), number("Bcc3"),
            ])
        except (IndexError, KeyError, ValueError) as exc:
            raise ValueError(f"invalid TAB row in {path.name}") from exc
    if not rows:
        raise ValueError(f"TAB has no data rows: {path.name}")
    return int(match.group(1)), finite(float(tm.group(1))), int(cm.group(1)), rows


def _stats(values: list[float]) -> dict[str, float]:
    if not values or not all(math.isfinite(v) for v in values):
        raise ValueError("observable input is empty or non-finite")
    return {"min": min(values), "max": max(values), "l1_mean": sum(abs(v) for v in values) / len(values)}


def _diagnostics(rows: list[list[float]], dimensions: list[int]) -> dict[str, Any]:
    nx, ny, nz = dimensions
    expected = nx * ny * nz
    if len(rows) != expected:
        raise ValueError(f"TAB rows {len(rows)} != expected {expected}")
    rho, press = [row[3] for row in rows], [row[4] for row in rows]
    velocity_squared = [sum(row[5 + q] ** 2 for q in range(3)) for row in rows]
    bvals = [row[8 + q] for row in rows for q in range(3)]
    lorentz: list[float] = []
    recovery_failures = 0
    for row in rows:
        v2 = sum(row[5 + q] ** 2 for q in range(3))
        if v2 >= 1.0:
            # The upstream shock family contains intentionally extreme states;
            # retain an explicit recovery/admissibility failure count rather
            # than emitting Infinity or silently clipping the state.
            recovery_failures += 1
            lorentz.append(1.0)
        else:
            lorentz.append(1.0 / math.sqrt(1.0 - v2))
    # Cell-centred magnetic projections expose the geometry of the CT state while
    # making the lack of native FaceField serialization explicit.
    grid = [[[rows[(k * ny + j) * nx + i] for i in range(nx)] for j in range(ny)] for k in range(nz)]
    div_values: list[float] = []
    for k in range(nz):
        for j in range(ny):
            for i in range(nx):
                def b(ii: int, jj: int, kk: int, component: int) -> float:
                    ii %= nx; jj %= ny; kk %= nz
                    return grid[kk][jj][ii][8 + component]
                dx = (b(i + 1, j, k, 0) - b(i - 1, j, k, 0)) / 2.0 if nx > 1 else 0.0
                dy = (b(i, j + 1, k, 1) - b(i, j - 1, k, 1)) / 2.0 if ny > 1 else 0.0
                dz = (b(i, j, k + 1, 2) - b(i, j, k - 1, 2)) / 2.0 if nz > 1 else 0.0
                div_values.append(dx + dy + dz)
    emf: list[float] = []
    for row in rows:
        vx, vy, vz, bx, by, bz = row[5:11]
        emf.extend([vy * bz - vz * by, vz * bx - vx * bz, vx * by - vy * bx])
    magnetic_energy = sum(0.5 * sum(row[8 + q] ** 2 for q in range(3)) for row in rows) / expected
    return {
        "finite_state": all(math.isfinite(value) for row in rows for value in row),
        "admissibility": {"all_rho_positive": min(rho) > 0.0, "all_pressure_positive": min(press) > 0.0,
                           "all_sub_luminal": all(v2 < 1.0 for v2 in velocity_squared)},
        "recovery": {"status": "all_emitted_cells_admissible" if recovery_failures == 0 else "emitted_state_contains_nonadmissible_cells",
                     "failed_cells": recovery_failures, "retries_or_fallbacks": 0, "lorentz_factor_max": max(lorentz)},
        "primitive_ranges": {name: _stats([row[index] for row in rows]) for index, name in enumerate(VARIABLES[3:], start=3)},
        "magnetic": {"channels": ["Bcc1", "Bcc2", "Bcc3"], "cell_centered": True,
                     "energy_density_mean": magnetic_energy, "range": _stats(bvals)},
        "face_B": {"status": "derived_projection_from_cell_centered_Bcc",
                    "native_internal_state": "FaceField is updated by constrained transport but not emitted by formatted_table.cpp",
                    "shapes": {"B1": [nz, ny, nx + 1], "B2": [nz, ny + 1, nx], "B3": [nz + 1, ny, nx]}},
        "corner_EMF": {"status": "derived_ideal_E_projection_from_primitive_and_Bcc",
                        "native_internal_state": "corner EMF is computed by calculate_corner_e.cpp and not emitted by formatted_table.cpp",
                        "E_v_cross_B": _stats(emf)},
        "discrete_divB": {"operator": "periodic-centered-cell-Bcc-projection", "l1_mean": sum(abs(v) for v in div_values) / expected,
                          "max_abs": max(abs(v) for v in div_values), "cells": expected},
    }


def read_frames(directory: Path, dimensions: list[int]) -> list[dict[str, Any]]:
    paths = sorted(directory.glob("*.tab"))
    if not paths:
        raise ValueError(f"no TAB files found in {directory}")
    grouped: dict[int, tuple[float, int, list[list[float]]]] = {}
    for path in paths:
        frame, time, cycle, rows = parse_tab(path)
        if frame in grouped:
            old_time, old_cycle, old_rows = grouped[frame]
            if old_time != time or old_cycle != cycle:
                raise ValueError(f"conflicting TAB metadata for frame {frame}")
            old_rows.extend(rows)
        else:
            grouped[frame] = (time, cycle, rows)
    frames = []
    expected = math.prod(dimensions)
    for frame in sorted(grouped):
        time, cycle, rows = grouped[frame]
        if len(rows) != expected:
            raise ValueError(f"frame {frame} has {len(rows)} rows; expected {expected}")
        frames.append({"time": time, "cycle": cycle, "rows": rows, "observables": _diagnostics(rows, dimensions)})
    return frames


def write_artifact(output: Path, check: str, source_commit: str, cases: list[dict[str, Any]]) -> None:
    document = {"schema": SCHEMA, "check": check, "source_commit": source_commit,
                "variables": VARIABLES, "cases": cases}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, allow_nan=False, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", required=True)
    parser.add_argument("--case", required=True)
    parser.add_argument("--dimensions", required=True)
    parser.add_argument("--solver", default="hlld")
    parser.add_argument("--deck", default="")
    args = parser.parse_args()
    dimensions = [int(value) for value in args.dimensions.split(",")]
    if len(dimensions) != 3 or any(value <= 0 for value in dimensions):
        raise SystemExit("dimensions must be three positive integers")
    case = {"name": args.case, "solver": args.solver, "deck": args.deck,
            "dimensions": dimensions, "variables": VARIABLES, "frames": read_frames(args.input, dimensions)}
    write_artifact(args.output, args.check, "823614c90b594472747a0ac2a699e4a454f300d2", [case])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
