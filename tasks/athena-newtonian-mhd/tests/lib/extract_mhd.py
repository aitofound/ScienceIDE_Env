#!/usr/bin/env python3
"""Convert native Athena++ MHD TAB output into a complete Harbor artifact.

The native formatted-table product exposes cell-centred primitive and magnetic
values.  The CT companion records the exact face locations/shapes, hashes of a
boundary-aware face reconstruction, discrete div-B statistics, and conserved
sums.  Athena++ does not write a face-field checkpoint in this output mode, so
that limitation is recorded explicitly rather than presented as native CT data.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
from pathlib import Path
from typing import Any

VARIABLES = [
    "x1", "x2", "x3", "rho", "press", "vel1", "vel2", "vel3",
    "Bcc1", "Bcc2", "Bcc3",
]
PRIMITIVES = VARIABLES[3:8]
MAGNETIC = VARIABLES[8:]
SOURCE_FORMAT = {
    "encoding": "UTF-8 JSON converted from Athena++ formatted TAB",
    "dtype": "float64",
    "ordering": "frame then native block-id, k-j-i TAB row order",
    "native_file_pattern": "<problem>.block<gid>.out<id>.<frame>.tab",
}
FRAME_RE = re.compile(r"\.block(\d+)\.out\d+\.(\d{5})\.tab$")
TIME_RE = re.compile(r"time=([^ ]+)")
CYCLE_RE = re.compile(r"cycle=([-+]?\d+)")


def finite(value: float) -> float:
    if not math.isfinite(value):
        raise ValueError("non-finite native value")
    return value


def parse_native(path: Path) -> tuple[int, int, float, int, list[list[float]]]:
    match = FRAME_RE.search(path.name)
    if not match:
        raise ValueError(f"unexpected TAB filename: {path.name}")
    block = int(match.group(1))
    frame = int(match.group(2))
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
    columns: dict[str, int] = {}
    for index, name in enumerate(header):
        if name in columns:
            raise ValueError(f"duplicate TAB column {name}: {path.name}")
        columns[name] = index
    required = {"x1v", "rho", "vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3"}
    if not required.issubset(columns):
        raise ValueError(f"TAB does not contain complete MHD columns: {path.name}")
    rows: list[list[float]] = []
    for line in lines[2:]:
        if not line.strip():
            continue
        tokens = line.split()
        try:
            def number(name: str) -> float:
                return finite(float(tokens[columns[name]]))
            rows.append([
                number("x1v"),
                number("x2v") if "x2v" in columns else 0.0,
                number("x3v") if "x3v" in columns else 0.0,
                number("rho"), number("press") if "press" in columns else number("rho"), number("vel1"),
                number("vel2"), number("vel3"),
                number("Bcc1"), number("Bcc2"), number("Bcc3"),
            ])
        except (IndexError, KeyError, ValueError) as exc:
            raise ValueError(f"invalid TAB row in {path.name}") from exc
    if not rows:
        raise ValueError(f"TAB has no data rows: {path.name}")
    return frame, block, time, cycle, rows


def _float_hash(values: list[float]) -> str:
    digest = hashlib.sha256()
    for value in values:
        digest.update(struct.pack("<d", float(value)))
    return digest.hexdigest()


def _face_values(b: list[list[list[float]]], axis: int, periodic: bool) -> list[list[list[float]]]:
    """Construct face values from a cell-centred array for diagnostics only."""
    nz = len(b)
    ny = len(b[0])
    nx = len(b[0][0])
    if axis == 0:
        out = [[[0.0 for _ in range(nx + 1)] for _ in range(ny)] for _ in range(nz)]
        for k in range(nz):
            for j in range(ny):
                for i in range(1, nx):
                    out[k][j][i] = 0.5 * (b[k][j][i - 1] + b[k][j][i])
                out[k][j][0] = 0.5 * (b[k][j][0] + b[k][j][-1]) if periodic and nx > 1 else b[k][j][0]
                out[k][j][nx] = out[k][j][0] if periodic and nx > 1 else b[k][j][-1]
        return out
    if axis == 1:
        out = [[[0.0 for _ in range(nx)] for _ in range(ny + 1)] for _ in range(nz)]
        for k in range(nz):
            for i in range(nx):
                for j in range(1, ny):
                    out[k][j][i] = 0.5 * (b[k][j - 1][i] + b[k][j][i])
                out[k][0][i] = 0.5 * (b[k][0][i] + b[k][-1][i]) if periodic and ny > 1 else b[k][0][i]
                out[k][ny][i] = out[k][0][i] if periodic and ny > 1 else b[k][-1][i]
        return out
    out = [[[0.0 for _ in range(nx)] for _ in range(ny)] for _ in range(nz + 1)]
    for j in range(ny):
        for i in range(nx):
            for k in range(1, nz):
                out[k][j][i] = 0.5 * (b[k - 1][j][i] + b[k][j][i])
            out[0][j][i] = 0.5 * (b[0][j][i] + b[-1][j][i]) if periodic and nz > 1 else b[0][j][i]
            out[nz][j][i] = out[0][j][i] if periodic and nz > 1 else b[-1][j][i]
    return out


def _flatten(values: list[list[list[float]]]) -> list[float]:
    return [value for plane in values for row in plane for value in row]


def _observable(frame: dict[str, Any], dimensions: list[int], boundary: str, gamma: float) -> dict[str, Any]:
    nx, ny, nz = dimensions
    rows = frame["rows"]
    b = [[[0.0 for _ in range(nx)] for _ in range(ny)] for _ in range(nz)]
    b2 = [[[0.0 for _ in range(nx)] for _ in range(ny)] for _ in range(nz)]
    b3 = [[[0.0 for _ in range(nx)] for _ in range(ny)] for _ in range(nz)]
    periodic = boundary == "periodic"
    mass = 0.0
    momentum = [0.0, 0.0, 0.0]
    energy = 0.0
    for index, row in enumerate(rows):
        k, remainder = divmod(index, nx * ny)
        j, i = divmod(remainder, nx)
        b[k][j][i], b2[k][j][i], b3[k][j][i] = row[8], row[9], row[10]
        rho, pressure = row[3], row[4]
        velocity = row[5:8]
        mass += rho
        for q in range(3):
            momentum[q] += rho * velocity[q]
        energy += pressure / (gamma - 1.0) + 0.5 * rho * sum(v * v for v in velocity) + 0.5 * sum(v * v for v in row[8:11])
    faces = [_face_values(b, 0, periodic), _face_values(b2, 1, periodic), _face_values(b3, 2, periodic)]
    dx = []
    for coordinate in range(3):
        values = [rows[0][coordinate]]
        if coordinate == 0 and nx > 1:
            values.append(rows[1][coordinate])
        elif coordinate == 1 and ny > 1:
            values.append(rows[nx][coordinate])
        elif coordinate == 2 and nz > 1:
            values.append(rows[nx * ny][coordinate])
        dx.append(abs(values[1] - values[0]) if len(values) == 2 else 1.0)
    div_values: list[float] = []
    for k in range(nz):
        for j in range(ny):
            for i in range(nx):
                value = ((faces[0][k][j][i + 1] - faces[0][k][j][i]) / dx[0]
                         + (faces[1][k][j + 1][i] - faces[1][k][j][i]) / dx[1]
                         + (faces[2][k + 1][j][i] - faces[2][k][j][i]) / dx[2])
                div_values.append(value)
    abs_div = [abs(value) for value in div_values]
    face_flat = [_flatten(face) for face in faces]
    face_flat_all = [value for component in face_flat for value in component]
    return {
        "time": frame["time"],
        "cycle": frame["cycle"],
        "face_field": {
            "locations": ["B1f(x1-face)", "B2f(x2-face)", "B3f(x3-face)"],
            "shapes": [[nz, ny, nx + 1], [nz, ny + 1, nx], [nz + 1, ny, nx]],
            "value_count": len(face_flat_all),
            "sha256": _float_hash(face_flat_all),
            "method": "boundary-aware cell-centre averaging; native TAB has no face checkpoint",
        },
        "discrete_div_b": {
            "cell_count": len(div_values),
            "finite": all(math.isfinite(value) for value in div_values),
            "l1": sum(abs_div) / len(abs_div),
            "linf": max(abs_div),
            "max_abs": max(abs_div),
            "spacing": dx,
            "boundary_policy": boundary,
        },
        "conserved_sums": {
            "mass": mass,
            "momentum": momentum,
            "total_energy": energy,
        },
        "ct_update_evidence": {
            "pipeline": [
                "src/hydro/calculate_fluxes.cpp",
                "src/hydro/add_flux_divergence.cpp",
                "src/field/calculate_corner_e.cpp",
                "src/field/ct.cpp",
                "src/task_list/time_integrator.cpp",
            ],
            "compiled_path": "-b Newtonian MHD; multi-step MeshBlock task integration",
            "native_corner_emf_counter": None,
            "native_face_checkpoint": False,
            "status": "self-wiring-evidence-only; final CT equivalence policy is Jason-owned",
        },
    }


def extract(input_dir: Path, output: Path, observable_output: Path, case: str, dimensions: list[int], boundary: str, gamma: float) -> None:
    expected_rows = math.prod(dimensions)
    files = []
    # Select the check-owned output2 stream.  Some upstream decks already emit
    # output1 as TAB too (notably RJ2a); combining both streams would duplicate
    # each block/frame and silently mix schedules.  run_case.py always forces
    # output2 to the task's TAB schema before launching Athena++.
    for path in input_dir.glob("*.tab"):
        if ".out2." not in path.name:
            continue
        parsed = FRAME_RE.search(path.name)
        if parsed:
            files.append((int(parsed.group(2)), int(parsed.group(1)), path))
    if not files:
        raise ValueError("no native Athena++ MHD TAB files found")
    frames: dict[int, dict[str, Any]] = {}
    for frame_number, block, path in sorted(files):
        frame, parsed_block, time, cycle, rows = parse_native(path)
        if frame != frame_number or parsed_block != block:
            raise ValueError("TAB filename parse mismatch")
        entry = frames.setdefault(frame, {"time": time, "cycle": cycle, "blocks": {}})
        if entry["time"] != time or entry["cycle"] != cycle:
            raise ValueError(f"conflicting metadata for frame {frame:05d}")
        if block in entry["blocks"]:
            raise ValueError(f"duplicate block {block} in frame {frame:05d}")
        entry["blocks"][block] = rows
    document_frames = []
    observable_frames = []
    for frame_number in sorted(frames):
        entry = frames[frame_number]
        rows = []
        for block in sorted(entry["blocks"]):
            rows.extend(entry["blocks"][block])
        if len(rows) != expected_rows:
            raise ValueError(f"frame {frame_number:05d} has {len(rows)} rows, expected {expected_rows}")
        frame = {"time": entry["time"], "cycle": entry["cycle"], "rows": rows}
        document_frames.append(frame)
        observable_frames.append(_observable(frame, dimensions, boundary, gamma))
    document = {
        "schema": "athena-mhd-state/v1",
        "case": case,
        "dimensions": dimensions,
        "variables": VARIABLES,
        "primitive_fields": PRIMITIVES,
        "magnetic_fields": MAGNETIC,
        "source_format": SOURCE_FORMAT,
        "frames": document_frames,
    }
    observables = {
        "schema": "athena-mhd-ct-observables/v1",
        "case": case,
        "dimensions": dimensions,
        "frame_count": len(observable_frames),
        "boundary_policy": boundary,
        "gamma": gamma,
        "frames": observable_frames,
        "scientific_status": "exact identity is provisional self-wiring evidence; Jason owns final CT/div-B tolerances",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    observable_output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, allow_nan=False, separators=(",", ":")) + "\n", encoding="utf-8")
    observable_output.write_text(json.dumps(observables, allow_nan=False, separators=(",", ":")) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--observables", required=True, type=Path)
    parser.add_argument("--case", required=True)
    parser.add_argument("--dimensions", required=True)
    parser.add_argument("--boundary", choices=["periodic", "outflow"], required=True)
    parser.add_argument("--gamma", type=float, default=1.666666666666667)
    args = parser.parse_args()
    dimensions = [int(value) for value in args.dimensions.split(",")]
    if len(dimensions) != 3 or any(value <= 0 for value in dimensions):
        raise SystemExit("--dimensions must be three positive integers")
    extract(args.input, args.output, args.observables, args.case, dimensions, args.boundary, args.gamma)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
