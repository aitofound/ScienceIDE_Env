#!/usr/bin/env python3
"""Convert native Athena++ MHD outputs of one run into the v2 Harbor artifacts.

Native inputs (all written by the pinned executable itself):

* formatted TAB files (one per MeshBlock and frame, full-precision
  ``data_format=%25.17e``) carry the cell-centred primitive state and Bcc;
* restart dumps (``.rst``) carry the face-centred fields b.x1f/x2f/x3f after
  the CT update, the conserved density, and the MeshBlock tree;
* the history file (``.hst``) carries volume-integrated conserved totals;
* the problem generator's analytic-error file (``*-errors.dat`` /
  ``carbuncle-diff.dat``) carries the native error/robustness reductions.

Assembly is coordinate/index aware: each TAB row is placed by its own block
index columns and each block by its logical location from the restart dump,
so a multi-MeshBlock run is never treated as a flat block-major list.  Every
derived quantity (discrete div-B, conserved sums, Bcc/face consistency) is a
deterministic function of the emitted arrays that the verifier recomputes
bit-for-bit; none of it encodes a tolerance.
"""
from __future__ import annotations

import array
import base64
import hashlib
import math
import re
import sys
from pathlib import Path
from typing import Any

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from case_spec import MAGNETIC, OBS_SCHEMA, PIPELINE, PRIMITIVES, RunPlan, STATE_SCHEMA, VARIABLES, CheckSpec  # noqa: E402
from native_evidence import EvidenceError, check_parameters  # noqa: E402
from native_restart import parse_restart  # noqa: E402

FRAME_RE = re.compile(r"^(?P<base>.+)\.block(?P<gid>\d+)\.out2\.(?P<frame>\d{5})\.tab$")
RST_RE = re.compile(r"^(?P<base>.+)\.(?P<tag>\d{5}|final)\.rst$")
TIME_RE = re.compile(r"time=([^ ]+)")
CYCLE_RE = re.compile(r"cycle=([-+]?\d+)")
STATE_COLUMNS = {"rho": "rho", "press": "press", "vel1": "vel1", "vel2": "vel2", "vel3": "vel3", "Bcc1": "Bcc1", "Bcc2": "Bcc2", "Bcc3": "Bcc3"}
ERROR_FILES = {"linear_wave": "linearwave-errors.dat", "cpaw": "cpaw-errors.dat", "shock_tube": "shock-errors.dat", "quirk": "carbuncle-diff.dat"}
FACE_NAMES = ("B1f", "B2f", "B3f")


class ExtractError(ValueError):
    pass


# ----------------------------------------------------------------- encoding
def encode(values: array.array) -> str:
    if values.typecode != "d":
        raise ExtractError("only float64 arrays are encoded")
    data = values.tobytes() if sys.byteorder == "little" else _swapped(values).tobytes()
    return base64.b64encode(data).decode("ascii")


def _swapped(values: array.array) -> array.array:
    copy = array.array("d", values)
    copy.byteswap()
    return copy


def decode(text: str, count: int | None = None) -> array.array:
    if not isinstance(text, str):
        raise ExtractError("encoded column must be a string")
    try:
        raw = base64.b64decode(text, validate=True)
    except (ValueError, TypeError) as exc:
        raise ExtractError(f"invalid base64 column: {exc}") from exc
    if len(raw) % 8:
        raise ExtractError("encoded column is not a whole number of float64 values")
    values = array.array("d")
    values.frombytes(raw)
    if sys.byteorder != "little":
        values.byteswap()
    if count is not None and len(values) != count:
        raise ExtractError(f"encoded column has {len(values)} values, expected {count}")
    return values


def sha256_floats(values: array.array) -> str:
    data = values.tobytes() if sys.byteorder == "little" else _swapped(values).tobytes()
    return hashlib.sha256(data).hexdigest()


def finite_array(values: array.array) -> bool:
    return all(math.isfinite(v) for v in values)


# ----------------------------------------------------------------- native parsers
def parse_tab(path: Path) -> dict[str, Any]:
    match = FRAME_RE.match(path.name)
    if not match:
        raise ExtractError(f"unexpected TAB filename: {path.name}")
    lines = path.read_text(encoding="utf-8").splitlines()
    if len(lines) < 3 or not lines[0].startswith("# Athena++ data at "):
        raise ExtractError(f"missing Athena TAB header: {path.name}")
    time_match = TIME_RE.search(lines[0])
    cycle_match = CYCLE_RE.search(lines[0])
    if not time_match or not cycle_match or "variables=prim" not in lines[0]:
        raise ExtractError(f"invalid Athena TAB metadata: {path.name}")
    time_text = time_match.group(1)
    time = float(time_text)
    if not math.isfinite(time):
        raise ExtractError(f"non-finite TAB time: {path.name}")
    header = lines[1].lstrip("#").split()
    columns: dict[str, int] = {}
    for index, name in enumerate(header):
        if name in columns:
            raise ExtractError(f"duplicate TAB column {name}: {path.name}")
        columns[name] = index
    required = {"i", "x1v", "rho", "vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3"}
    if not required.issubset(columns):
        raise ExtractError(f"TAB does not contain complete MHD columns: {path.name}")
    index_names = [name for name in ("i", "j", "k") if name in columns]
    coord_names = [name for name in ("x1v", "x2v", "x3v") if name in columns]
    if len(index_names) != len(coord_names):
        raise ExtractError(f"TAB index/coordinate columns are inconsistent: {path.name}")
    value_names = [name for name in ("rho", "press", "vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3") if name in columns]
    indices = {name: [] for name in index_names}
    coords = {name: array.array("d") for name in coord_names}
    values = {name: array.array("d") for name in value_names}
    for line in lines[2:]:
        if not line.strip():
            continue
        tokens = line.split()
        if len(tokens) != len(header):
            raise ExtractError(f"TAB row width mismatch in {path.name}")
        try:
            for name in index_names:
                indices[name].append(int(tokens[columns[name]]))
            for name in coord_names:
                coords[name].append(float(tokens[columns[name]]))
            for name in value_names:
                values[name].append(float(tokens[columns[name]]))
        except ValueError as exc:
            raise ExtractError(f"invalid TAB row in {path.name}") from exc
    if not values["rho"]:
        raise ExtractError(f"TAB has no data rows: {path.name}")
    for name, column in list(coords.items()) + list(values.items()):
        if not finite_array(column):
            raise ExtractError(f"non-finite native value in column {name}: {path.name}")
    return {
        "path": path, "gid": int(match.group("gid")), "frame": int(match.group("frame")), "time_text": time_text,
        "time": time, "cycle": int(cycle_match.group(1)), "indices": indices, "coords": coords, "values": values,
        "has_press": "press" in values,
    }


def parse_hst(path: Path, inherited_columns: list[str] | None = None) -> dict[str, Any]:
    """Parse a history file; a restarted run appends header-less rows, so it inherits the parent's columns."""
    lines = path.read_text(encoding="utf-8").splitlines()
    if lines and lines[0].startswith("# Athena++ history data"):
        if len(lines) < 2:
            raise ExtractError("truncated Athena history header")
        names = re.findall(r"\[\d+\]=(\S+)", lines[1])
        body = lines[2:]
        header_present = True
    else:
        if inherited_columns is None:
            raise ExtractError("missing Athena history header")
        names = list(inherited_columns)
        body = lines
        header_present = False
    if len(names) < 9 or names[:2] != ["time", "dt"]:
        raise ExtractError("history header lacks the standard columns")
    rows = []
    for line in body:
        if not line.strip() or line.startswith("#"):
            continue
        tokens = line.split()
        if len(tokens) != len(names):
            raise ExtractError("history row width mismatch")
        try:
            row = [float(token) for token in tokens]
        except ValueError as exc:
            raise ExtractError("invalid history row") from exc
        if not all(math.isfinite(value) for value in row):
            raise ExtractError("non-finite history value")
        rows.append(row)
    if not rows:
        raise ExtractError("history file has no rows")
    return {"name": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size, "columns": names, "rows": rows,
            "header_present": header_present}


def parse_error_file(path: Path) -> dict[str, Any]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or not lines[0].startswith("#"):
        raise ExtractError(f"missing header in native error file {path.name}")
    header = lines[0].lstrip("#").strip()
    rows = []
    for line in lines[1:]:
        if not line.strip() or line.startswith("#"):
            continue
        try:
            row = [float(token) for token in line.split()]
        except ValueError as exc:
            raise ExtractError(f"invalid row in native error file {path.name}") from exc
        if not all(math.isfinite(value) for value in row):
            raise ExtractError(f"non-finite native error value in {path.name}")
        rows.append(row)
    if not rows:
        raise ExtractError(f"native error file {path.name} has no rows")
    return {"name": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size, "header": header, "rows": rows}


def time_key(value: float) -> str:
    """Athena++ prints TAB frame times with %e; use that rendering as the join key."""
    return "%e" % value


# ----------------------------------------------------------------- derived quantities
def cell_spacing(mesh_size: dict[str, float], level: int) -> list[float]:
    spacing = []
    for axis in (1, 2, 3):
        n = mesh_size[f"nx{axis}"]
        extent = mesh_size[f"x{axis}max"] - mesh_size[f"x{axis}min"]
        spacing.append(extent / (n * (1 << level)) if n > 1 else extent)
    return spacing


def divergence(faces: dict[str, array.array], shape: list[int], spacing: list[float]) -> array.array:
    """Discrete div B on an (nz, ny, nx) box from its (nz,ny,nx+1)/(nz,ny+1,nx)/(nz+1,ny,nx) faces.

    Degenerate axes (one cell) contribute nothing: Athena++ has no finite
    difference along them.  Summation order is fixed (x1 + x2 + x3 per cell).
    """
    nz, ny, nx = shape
    b1, b2, b3 = faces["B1f"], faces["B2f"], faces["B3f"]
    dx, dy, dz = spacing
    out = array.array("d")
    for k in range(nz):
        for j in range(ny):
            for i in range(nx):
                value = 0.0
                if nx > 1:
                    value += (b1[(k * ny + j) * (nx + 1) + i + 1] - b1[(k * ny + j) * (nx + 1) + i]) / dx
                if ny > 1:
                    value += (b2[(k * (ny + 1) + j + 1) * nx + i] - b2[(k * (ny + 1) + j) * nx + i]) / dy
                if nz > 1:
                    value += (b3[((k + 1) * ny + j) * nx + i] - b3[(k * ny + j) * nx + i]) / dz
                out.append(value)
    return out


def divergence_stats(div: array.array) -> dict[str, Any]:
    if not len(div):
        raise ExtractError("empty divergence array")
    total = 0.0
    largest = 0.0
    for value in div:
        magnitude = abs(value)
        total += magnitude
        if magnitude > largest:
            largest = magnitude
    return {"cell_count": len(div), "l1": total / len(div), "linf": largest, "max_abs": largest}


def face_scale(faces: dict[str, array.array]) -> float:
    largest = 0.0
    for name in FACE_NAMES:
        for value in faces[name]:
            if abs(value) > largest:
                largest = abs(value)
    return largest


def conserved_sums(columns: dict[str, array.array], volume: float, gamma: float, eos: str) -> dict[str, Any]:
    rho, press = columns["rho"], columns["press"]
    v1, v2, v3 = columns["vel1"], columns["vel2"], columns["vel3"]
    b1, b2, b3 = columns["Bcc1"], columns["Bcc2"], columns["Bcc3"]
    mass = 0.0
    momentum = [0.0, 0.0, 0.0]
    kinetic = 0.0
    magnetic = 0.0
    total = 0.0
    for n in range(len(rho)):
        d = rho[n]
        mass += d * volume
        momentum[0] += d * v1[n] * volume
        momentum[1] += d * v2[n] * volume
        momentum[2] += d * v3[n] * volume
        ke = 0.5 * d * (v1[n] * v1[n] + v2[n] * v2[n] + v3[n] * v3[n])
        me = 0.5 * (b1[n] * b1[n] + b2[n] * b2[n] + b3[n] * b3[n])
        kinetic += ke * volume
        magnetic += me * volume
        if eos == "adiabatic":
            total += (press[n] / (gamma - 1.0) + ke + me) * volume
    return {
        "cell_volume": volume, "mass": mass, "momentum": momentum, "kinetic_energy": kinetic, "magnetic_energy": magnetic,
        "total_energy": total if eos == "adiabatic" else None,
    }


def bcc_face_consistency(columns: dict[str, array.array], faces: dict[str, array.array], shape: list[int]) -> float:
    """max |Bcc - average of the two bounding native faces| over the box (diagnostic)."""
    nz, ny, nx = shape
    b1, b2, b3 = faces["B1f"], faces["B2f"], faces["B3f"]
    c1, c2, c3 = columns["Bcc1"], columns["Bcc2"], columns["Bcc3"]
    worst = 0.0
    for k in range(nz):
        for j in range(ny):
            for i in range(nx):
                n = (k * ny + j) * nx + i
                d1 = abs(c1[n] - 0.5 * (b1[(k * ny + j) * (nx + 1) + i] + b1[(k * ny + j) * (nx + 1) + i + 1]))
                d2 = abs(c2[n] - 0.5 * (b2[(k * (ny + 1) + j) * nx + i] + b2[(k * (ny + 1) + j + 1) * nx + i]))
                d3 = abs(c3[n] - 0.5 * (b3[(k * ny + j) * nx + i] + b3[((k + 1) * ny + j) * nx + i]))
                worst = max(worst, d1, d2, d3)
    return worst


def face_shapes(shape: list[int]) -> dict[str, list[int]]:
    nz, ny, nx = shape
    return {"B1f": [nz, ny, nx + 1], "B2f": [nz, ny + 1, nx], "B3f": [nz + 1, ny, nx]}


# ----------------------------------------------------------------- block/frame assembly
def _block_box(tab: dict[str, Any]) -> tuple[list[int], dict[str, array.array], dict[str, array.array]]:
    """Reorder one TAB block into local k-j-i order using its own index columns."""
    indices = tab["indices"]
    n = len(tab["values"]["rho"])
    axes = []
    for name in ("i", "j", "k"):
        if name in indices:
            lo, hi = min(indices[name]), max(indices[name])
            axes.append((name, lo, hi - lo + 1))
        else:
            axes.append((name, 0, 1))
    (_, i0, nx), (_, j0, ny), (_, k0, nz) = axes
    if nx * ny * nz != n:
        raise ExtractError(f"TAB block {tab['gid']} rows do not fill its index box")
    order = [-1] * n
    for row in range(n):
        i = indices["i"][row] - i0 if "i" in indices else 0
        j = indices["j"][row] - j0 if "j" in indices else 0
        k = indices["k"][row] - k0 if "k" in indices else 0
        slot = (k * ny + j) * nx + i
        if order[slot] != -1:
            raise ExtractError(f"TAB block {tab['gid']} repeats cell index ({k},{j},{i})")
        order[slot] = row
    coords = {}
    for axis, name in (("x1", "x1v"), ("x2", "x2v"), ("x3", "x3v")):
        source = tab["coords"].get(name)
        coords[axis] = array.array("d", (source[r] for r in order)) if source is not None else array.array("d", [0.0] * n)
    values = {name: array.array("d", (col[r] for r in order)) for name, col in tab["values"].items()}
    return [nz, ny, nx], coords, values


def _columns(coords: dict[str, array.array], values: dict[str, array.array], eos: str, iso_cs: float) -> dict[str, array.array]:
    columns = {"x1": coords["x1"], "x2": coords["x2"], "x3": coords["x3"]}
    for name in ("rho", "vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3"):
        columns[name] = values[name]
    if eos == "adiabatic":
        if "press" not in values:
            raise ExtractError("adiabatic TAB lacks a press column")
        columns["press"] = values["press"]
    else:
        if "press" in values:
            raise ExtractError("isothermal TAB unexpectedly carries a press column")
        columns["press"] = array.array("d", (d * iso_cs * iso_cs for d in values["rho"]))
    return {name: columns[name] for name in VARIABLES}


def _bind_block(rst_block: dict[str, Any], columns: dict[str, array.array], shape: list[int], gid: int) -> None:
    if rst_block["shape"] != shape:
        raise ExtractError(f"block {gid}: TAB box {shape} differs from restart block shape {rst_block['shape']}")
    if rst_block["density"] != columns["rho"]:
        raise ExtractError(f"block {gid}: TAB density is not bit-identical to the native restart conserved density")


def _block_origin_check(rst: dict[str, Any], block: dict[str, Any], coords: dict[str, array.array], shape: list[int]) -> None:
    """Bind the block's coordinates to its logical location (index of the block at its level)."""
    mesh = rst["mesh_size"]
    level = block["location"]["level"]
    for axis, (n_cells, key) in enumerate(((shape[2], "x1"), (shape[1], "x2"), (shape[0], "x3"))):
        n_mesh = mesh[f"nx{axis + 1}"]
        if n_mesh == 1:
            continue
        extent = mesh[f"x{axis + 1}max"] - mesh[f"x{axis + 1}min"]
        block_extent = extent * n_cells / (n_mesh * (1 << level))
        lo = min(coords[key])
        expected = block["location"][f"lx{axis + 1}"]
        found = math.floor((lo - mesh[f"x{axis + 1}min"]) / block_extent)
        if found != expected:
            raise ExtractError(f"block {block['gid']}: coordinates place it at lx{axis + 1}={found}, restart tree says {expected}")


def assemble_frame(plan: RunPlan, tabs: list[dict[str, Any]], rst: dict[str, Any], gamma: float, eos: str, iso_cs: float) -> tuple[dict[str, Any], dict[str, Any]]:
    """Build the state frame and the observable frame for one output time."""
    if not tabs:
        raise ExtractError("frame has no TAB blocks")
    time_text = tabs[0]["time_text"]
    cycle = tabs[0]["cycle"]
    for tab in tabs:
        if tab["time_text"] != time_text or tab["cycle"] != cycle:
            raise ExtractError(f"conflicting TAB metadata at frame {time_text}")
    if time_key(rst["time"]) != time_text or rst["ncycle"] != cycle:
        raise ExtractError(f"restart dump time/cycle ({time_key(rst['time'])},{rst['ncycle']}) does not match TAB frame ({time_text},{cycle})")
    gids = sorted(tab["gid"] for tab in tabs)
    if gids != list(range(len(gids))) or len(gids) != rst["nbtotal"]:
        raise ExtractError(f"frame {time_text}: TAB blocks {len(gids)} do not match restart block count {rst['nbtotal']}")
    if len(rst["blocks"]) != plan.expected_blocks:
        raise ExtractError(f"frame {time_text}: {len(rst['blocks'])} MeshBlocks, rubric expects {plan.expected_blocks}")
    levels = sorted({block["location"]["level"] for block in rst["blocks"]})
    if levels != list(plan.expected_levels):
        raise ExtractError(f"frame {time_text}: refinement levels {levels}, rubric expects {list(plan.expected_levels)}")
    locations = {(b["location"]["lx1"], b["location"]["lx2"], b["location"]["lx3"], b["location"]["level"]) for b in rst["blocks"]}
    if len(locations) != len(rst["blocks"]):
        raise ExtractError("restart tree repeats a logical location")
    mesh = rst["mesh_size"]
    if [mesh["nx1"], mesh["nx2"], mesh["nx3"]] != list(plan.dimensions) or rst["meshblock"] != list(plan.meshblock):
        raise ExtractError("restart mesh/meshblock dimensions do not match the run plan")
    try:
        # The dump embedded in the checkpoint is the parameter set the pinned
        # executable actually ran with, so the deck, the rubric-derived
        # overrides and every selector are bound from native bytes.
        check_parameters(rst["param_text"], plan)
    except EvidenceError as exc:
        raise ExtractError(f"frame {time_text}: {exc}") from exc
    blocks = []
    for tab in sorted(tabs, key=lambda t: t["gid"]):
        shape, coords, values = _block_box(tab)
        columns = _columns(coords, values, eos, iso_cs)
        rst_block = rst["blocks"][tab["gid"]]
        _bind_block(rst_block, columns, shape, tab["gid"])
        _block_origin_check(rst, rst_block, coords, shape)
        blocks.append({"gid": tab["gid"], "location": rst_block["location"], "shape": shape, "columns": columns,
                       "faces": {name: rst_block[name] for name in FACE_NAMES}})
    time = float(time_text)
    native = {"file": rst["file"], "sha256": rst["sha256"], "bytes": rst["bytes"], "time": rst["time"], "dt": rst["dt"], "ncycle": rst["ncycle"],
              "nbtotal": rst["nbtotal"], "root_level": rst["root_level"], "mesh_size": mesh, "meshblock": rst["meshblock"], "nhydro": rst["nhydro"]}
    if plan.refinement == "none":
        state_frame, obs_frame = _assemble_uniform(plan, blocks, mesh, gamma, eos)
    else:
        state_frame, obs_frame = _assemble_blocks(plan, blocks, mesh, gamma, eos)
    state_frame = {"time": time, "cycle": cycle, **state_frame}
    obs_frame = {"time": time, "cycle": cycle, "native_restart": native, **obs_frame}
    return state_frame, obs_frame


def block_columns_from_global(columns: dict[str, array.array], dims: tuple[int, int, int], meshblock: tuple[int, int, int], location: list[int] | tuple[int, int, int]) -> dict[str, array.array]:
    """Slice one root-level MeshBlock out of global k-j-i columns."""
    nx, ny, nz = dims
    bx, by, bz = meshblock
    i0, j0, k0 = location[0] * bx, location[1] * by, location[2] * bz
    out = {name: array.array("d") for name in columns}
    for k in range(bz):
        for j in range(by):
            base = ((k0 + k) * ny + (j0 + j)) * nx + i0
            for name, col in columns.items():
                out[name].extend(col[base:base + bx])
    return out


def shared_face_consistency(blocks: list[dict[str, Any]]) -> dict[str, Any]:
    """Compare native face values on faces shared by same-level interior neighbours.

    Blocks whose logical locations differ by +1 along exactly one axis share
    the faces of that axis: the lower block's last face plane must equal the
    upper block's first face plane.  Periodic wrap pairs are not compared (the
    boundary policy per axis is a deck fact, not an artifact fact).  This is a
    recomputable observable, not a pass threshold.
    """
    index = {(b["location"]["level"], b["location"]["lx1"], b["location"]["lx2"], b["location"]["lx3"]): b for b in blocks}
    compared = 0
    mismatches = 0
    worst = 0.0
    for block in blocks:
        loc = block["location"]
        nz, ny, nx = block["shape"]
        for axis, name in enumerate(FACE_NAMES):
            key = [loc["level"], loc["lx1"], loc["lx2"], loc["lx3"]]
            key[axis + 1] += 1
            other = index.get(tuple(key))
            if other is None:
                continue
            fz, fy, fx = (nz, ny, nx + 1) if axis == 0 else (nz, ny + 1, nx) if axis == 1 else (nz + 1, ny, nx)
            mine, theirs = block["faces"][name], other["faces"][name]
            for k in range(fz):
                for j in range(fy):
                    for i in range(fx):
                        if (axis == 0 and i != fx - 1) or (axis == 1 and j != fy - 1) or (axis == 2 and k != fz - 1):
                            continue
                        a = mine[(k * fy + j) * fx + i]
                        ii, jj, kk = (0, j, k) if axis == 0 else (i, 0, k) if axis == 1 else (i, j, 0)
                        b = theirs[(kk * fy + jj) * fx + ii]
                        compared += 1
                        if a != b:
                            mismatches += 1
                            worst = max(worst, abs(a - b))
    return {"compared": compared, "mismatches": mismatches, "max_abs_difference": worst}


def _face_blocks(blocks: list[dict[str, Any]]) -> tuple[list[dict[str, Any]], str, float]:
    records = []
    digest = hashlib.sha256()
    scale = 0.0
    for block in blocks:
        loc = block["location"]
        records.append({"gid": block["gid"], "level": loc["level"], "location": [loc["lx1"], loc["lx2"], loc["lx3"]], "shape": block["shape"],
                        "shapes": face_shapes(block["shape"]), "components": {name: encode(block["faces"][name]) for name in FACE_NAMES}})
        concat = _concat(block["faces"])
        digest.update(concat.tobytes() if sys.byteorder == "little" else _swapped(concat).tobytes())
        scale = max(scale, face_scale(block["faces"]))
    return records, digest.hexdigest(), scale


def derived_observables(blocks: list[dict[str, Any]], mesh: dict[str, float], gamma: float, eos: str, global_columns: dict[str, array.array] | None, dims: tuple[int, int, int]) -> dict[str, Any]:
    """Div-B, conserved sums, Bcc/face consistency and shared-face agreement from per-block native faces.

    For uniform runs `global_columns` supplies the state (sums are taken over
    the global lattice); for refined runs each block carries its own columns.
    The identical function is used by the verifier to recompute every value.
    """
    all_div = array.array("d")
    consistency = 0.0
    if global_columns is not None:
        spacing = cell_spacing(mesh, 0)
        sums = conserved_sums(global_columns, spacing[0] * spacing[1] * spacing[2], gamma, eos)
        sums = {**sums, "method": "volume-weighted plain accumulation in global k-j-i order"}
    else:
        totals = {"mass": 0.0, "momentum": [0.0, 0.0, 0.0], "kinetic_energy": 0.0, "magnetic_energy": 0.0, "total_energy": 0.0 if eos == "adiabatic" else None}
    for block in blocks:
        level = block["location"]["level"]
        spacing = cell_spacing(mesh, level)
        all_div.extend(divergence(block["faces"], block["shape"], spacing))
        if global_columns is not None:
            columns = block_columns_from_global(global_columns, dims, (block["shape"][2], block["shape"][1], block["shape"][0]),
                                                (block["location"]["lx1"], block["location"]["lx2"], block["location"]["lx3"]))
        else:
            columns = block["columns"]
            part = conserved_sums(columns, spacing[0] * spacing[1] * spacing[2], gamma, eos)
            totals["mass"] += part["mass"]
            for q in range(3):
                totals["momentum"][q] += part["momentum"][q]
            totals["kinetic_energy"] += part["kinetic_energy"]
            totals["magnetic_energy"] += part["magnetic_energy"]
            if eos == "adiabatic":
                totals["total_energy"] += part["total_energy"]
        consistency = max(consistency, bcc_face_consistency(columns, block["faces"], block["shape"]))
    if global_columns is None:
        sums = {"cell_volume": None, **totals, "method": "volume-weighted per-block accumulation in gid order with level-aware cell volumes"}
    stats = divergence_stats(all_div)
    return {
        "discrete_div_b": {**stats, "method": "native per-block faces in gid order with level-aware uniform spacing; degenerate axes skipped"},
        "conserved_sums": sums,
        "state_binding": {"density_matches_native_restart": True, "bcc_vs_face_average_max_abs": consistency},
        "shared_face_consistency": shared_face_consistency(blocks),
    }


def _assemble_uniform(plan: RunPlan, blocks: list[dict[str, Any]], mesh: dict[str, float], gamma: float, eos: str) -> tuple[dict[str, Any], dict[str, Any]]:
    nx, ny, nz = plan.dimensions
    bx, by, bz = plan.meshblock
    total = nx * ny * nz
    columns = {name: array.array("d", bytes(8 * total)) for name in VARIABLES}
    filled = bytearray(total)
    for block in blocks:
        loc = block["location"]
        if loc["level"] != 0:
            raise ExtractError("uniform run contains a refined block")
        i0, j0, k0 = loc["lx1"] * bx, loc["lx2"] * by, loc["lx3"] * bz
        bnz, bny, bnx = block["shape"]
        if [bnz, bny, bnx] != [bz, by, bx]:
            raise ExtractError(f"block {block['gid']} shape {block['shape']} is not the meshblock size")
        for k in range(bnz):
            for j in range(bny):
                for i in range(bnx):
                    local = (k * bny + j) * bnx + i
                    g = ((k0 + k) * ny + (j0 + j)) * nx + (i0 + i)
                    if g >= total or filled[g]:
                        raise ExtractError(f"block {block['gid']} overlaps or exceeds the global lattice")
                    filled[g] = 1
                    for name in VARIABLES:
                        columns[name][g] = block["columns"][name][local]
    if not all(filled):
        raise ExtractError("global lattice has unfilled cells after block placement")
    if not _lattice_check(columns, plan.dimensions):
        raise ExtractError("assembled coordinates do not form the expected tensor lattice")
    records, digest, scale = _face_blocks(blocks)
    derived = derived_observables(blocks, mesh, gamma, eos, columns, plan.dimensions)
    state_frame = {"layout": "global", "cell_count": total, "columns": {name: encode(columns[name]) for name in VARIABLES}}
    obs_frame = {"face_field": {"source": "native restart checkpoint b.x1f/b.x2f/b.x3f", "layout": "blocks", "blocks": records, "sha256": digest, "max_abs_face": scale}, **derived}
    return state_frame, obs_frame


def _lattice_check(columns: dict[str, array.array], dims: tuple[int, int, int]) -> bool:
    nx, ny, nz = dims
    for axis, name in enumerate(("x1", "x2", "x3")):
        n = dims[axis]
        col = columns[name]
        unique = sorted(set(col))
        if len(unique) != n:
            return False
        for g in range(nx * ny * nz):
            k, rem = divmod(g, nx * ny)
            j, i = divmod(rem, nx)
            if col[g] != unique[(i, j, k)[axis]]:
                return False
    return True


def _assemble_blocks(plan: RunPlan, blocks: list[dict[str, Any]], mesh: dict[str, float], gamma: float, eos: str) -> tuple[dict[str, Any], dict[str, Any]]:
    state_blocks = []
    cell_count = 0
    for block in blocks:
        loc = block["location"]
        cell_count += math.prod(block["shape"])
        state_blocks.append({"gid": block["gid"], "level": loc["level"], "location": [loc["lx1"], loc["lx2"], loc["lx3"]], "shape": block["shape"],
                             "columns": {name: encode(block["columns"][name]) for name in VARIABLES}})
    records, digest, scale = _face_blocks(blocks)
    derived = derived_observables(blocks, mesh, gamma, eos, None, plan.dimensions)
    state_frame = {"layout": "blocks", "cell_count": cell_count, "blocks": state_blocks}
    obs_frame = {"face_field": {"source": "native restart checkpoint b.x1f/b.x2f/b.x3f", "layout": "blocks", "blocks": records, "sha256": digest, "max_abs_face": scale}, **derived}
    return state_frame, obs_frame


def _concat(faces: dict[str, array.array]) -> array.array:
    out = array.array("d")
    for name in FACE_NAMES:
        out.extend(faces[name])
    return out


# ----------------------------------------------------------------- run extraction
def collect_native(run_dir: Path, plan: RunPlan, inherited_columns: list[str] | None = None) -> dict[str, Any]:
    """Inventory the run directory and parse every native product the plan needs."""
    files = sorted(p for p in run_dir.iterdir() if p.is_file() and not p.is_symlink())
    inventory = [{"name": p.name, "bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]
    tabs: dict[str, list[dict[str, Any]]] = {}
    for path in files:
        if FRAME_RE.match(path.name):
            tab = parse_tab(path)
            tabs.setdefault(tab["time_text"], []).append(tab)
    rsts: dict[str, dict[str, Any]] = {}
    checkpoints: list[dict[str, Any]] = []
    for path in files:
        if RST_RE.match(path.name):
            rst = parse_restart(path, plan.eos, plan.nghost)
            checkpoints.append({"file": rst["file"], "sha256": rst["sha256"], "bytes": rst["bytes"], "time": rst["time"], "ncycle": rst["ncycle"]})
            key = time_key(rst["time"])
            if key in rsts:
                # Athena++ writes both <n>.rst and final.rst at tlim; require identical payloads.
                if rsts[key]["blocks"] != rst["blocks"]:
                    raise ExtractError(f"two restart dumps at time {key} disagree")
                continue
            rsts[key] = rst
    hst_files = [p for p in files if p.suffix == ".hst"]
    if len(hst_files) != 1:
        raise ExtractError(f"expected exactly one history file, found {len(hst_files)}")
    error_name = ERROR_FILES[plan.problem]
    error_path = run_dir / error_name
    if not error_path.is_file():
        raise ExtractError(f"native analytic-error file {error_name} was not written")
    return {"inventory": inventory, "tabs": tabs, "rsts": rsts, "checkpoints": checkpoints, "hst": parse_hst(hst_files[0], inherited_columns), "error": parse_error_file(error_path)}


def extract_run(plan: RunPlan, run_dir: Path, gamma: float, eos: str, iso_cs: float, inherited_columns: list[str] | None = None) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    """Return (state_run_body, observables_run_body, native inventory) for one executed run."""
    native = collect_native(run_dir, plan, inherited_columns)
    expected = [time_key(t) for t in plan.expected_times]
    found = sorted(native["tabs"], key=float)
    if found != expected:
        raise ExtractError(f"TAB frames {found} do not match the expected schedule {expected}")
    hst = native["hst"]
    hst_by_time = {}
    for index, row in enumerate(hst["rows"]):
        key = time_key(row[0])
        if key in hst_by_time:
            raise ExtractError(f"history file repeats time {key}")
        hst_by_time[key] = index
    state_frames = []
    obs_frames = []
    for key in expected:
        if key not in native["rsts"]:
            raise ExtractError(f"no native restart dump at frame time {key}")
        if key not in hst_by_time:
            raise ExtractError(f"no history row at frame time {key}")
        state_frame, obs_frame = assemble_frame(plan, native["tabs"][key], native["rsts"][key], gamma, eos, iso_cs)
        row = hst["rows"][hst_by_time[key]]
        obs_frame["native_history"] = {"row_index": hst_by_time[key], "values": dict(zip(hst["columns"], row))}
        state_frames.append(state_frame)
        obs_frames.append(obs_frame)
    state_body = {"run_id": plan.run_id, "mesh": {"dimensions": list(plan.dimensions), "meshblock": list(plan.meshblock), "refinement": plan.refinement,
                                                  "mesh_size": native["rsts"][expected[0]]["mesh_size"]}, "frames": state_frames}
    obs_body = {"run_id": plan.run_id, "frames": obs_frames, "native_history_file": hst, "native_error_file": native["error"], "native_checkpoints": native["checkpoints"]}
    return state_body, obs_body, native


def state_document(case: str, check_id: str, runs: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "schema": STATE_SCHEMA, "case": case, "check_id": check_id, "variables": VARIABLES, "primitive_fields": PRIMITIVES, "magnetic_fields": MAGNETIC,
        "encoding": {"dtype": "float64", "byte_order": "little", "container": "base64", "layout": "one column per variable; cells in global k-j-i order (layout=global) or per MeshBlock in local k-j-i order (layout=blocks)"},
        "runs": runs,
    }


def observables_document(case: str, check_id: str, runs: list[dict[str, Any]], check_level: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema": OBS_SCHEMA, "case": case, "check_id": check_id, "runs": runs,
        "ct_update_evidence": {
            "pipeline": PIPELINE,
            "native_face_checkpoint": True,
            "native_corner_emf_counter": None,
            "status": "native restart-checkpoint face fields and their discrete divergence are the CT evidence; no corner-EMF counter exists in the pinned code; exact identity is provisional self-wiring evidence and final CT/div-B policy is Jason-owned",
        },
        "check_level": check_level,
        "scientific_status": "exact identity is provisional self-wiring evidence; Jason owns final CT/div-B tolerances",
    }


# ----------------------------------------------------------------- check-level evidence
def frames_identical(a: dict[str, Any], b: dict[str, Any]) -> bool:
    """Exact equality of two serialized state frames (encoded columns included)."""
    if a["layout"] != b["layout"] or a["cell_count"] != b["cell_count"]:
        return False
    if a["layout"] == "global":
        return a["columns"] == b["columns"]
    if len(a["blocks"]) != len(b["blocks"]):
        return False
    return all(x["gid"] == y["gid"] and x["level"] == y["level"] and x["shape"] == y["shape"] and x["columns"] == y["columns"]
               for x, y in zip(a["blocks"], b["blocks"]))


def check_level_evidence(spec: "CheckSpec", state_runs: list[dict[str, Any]]) -> dict[str, Any]:
    """Recompute the check-level restart claim from the emitted frames.

    Only the restart reproduction is derived here.  A cross-run/launcher
    determinism map would be a scientific claim about serial/OpenMP/MPI
    equality, which is an owner decision that is still pending, so the slot is
    published as an explicit null instead of an unbound boolean map.
    """
    by_id = {run["run_id"]: run for run in state_runs}
    reproduces: dict[str, bool] = {}
    for plan in spec.runs:
        if not plan.is_restart:
            continue
        run = by_id[plan.run_id]
        parent = by_id[plan.restart_from["run_id"]]
        parent_frames = {time_key(f["time"]): f for f in parent["frames"]}
        reproduces[plan.run_id] = all(
            time_key(f["time"]) in parent_frames and frames_identical(f, parent_frames[time_key(f["time"])])
            for f in run["frames"])
    return {"restart_reproduces_parent": reproduces, "cross_run_determinism_policy": None}
