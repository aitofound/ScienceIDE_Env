#!/usr/bin/env python3
"""Write native-format Athena++ evidence bytes for the adversarial fixtures.

The graded verifier reparses every rewarded observable from the retained
native bytes, so a fixture that only fabricates JSON can no longer exercise it.
This module writes the *formats* the pinned code writes -- formatted TAB
tables (`src/outputs/formatted_table.cpp`), the history file
(`src/outputs/history.cpp`), the problem generators' analytic-error files, the
restart dump with its embedded parameter dump
(`src/outputs/restart.cpp` + `ParameterInput::ParameterDump`), the end-of-run
stdout of `src/main.cpp`, the `-m` mesh/rank report of
`Mesh::OutputMeshStructure`, and the `athena -c` banner of `ShowConfig()` --
with synthetic values.

Nothing here is physics and nothing here is a scientific claim: the numbers are
smooth synthetic fields.  Its purpose is to drive the byte-level verifier and
its adversarial rejections without Docker, and to cross-check the readers in
`extract_mhd.py`/`native_restart.py`/`native_evidence.py` against the layout of
the pinned writers.  The real-execution gate remains the two-solve Docker
campaign.
"""
from __future__ import annotations

import array
import re
import struct
import sys
from pathlib import Path
from typing import Any

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from case_spec import DATA_FORMAT, CheckSpec, RunPlan  # noqa: E402
from extract_mhd import FACE_NAMES, face_shapes, time_key  # noqa: E402

NGHOST_PAD = 4  # RegionSize is nine doubles + three ints padded to 88 bytes
BLOCK_RE = re.compile(r"^<([A-Za-z0-9_]+)>\s*$")
HST_COLUMNS = ["time", "dt", "mass", "1-mom", "2-mom", "3-mom", "1-KE", "2-KE", "3-KE", "tot-E", "1-ME", "2-ME", "3-ME"]
ERROR_HEADERS = {
    "linear_wave": "# Nx1  Nx2  Nx3  Ncycle  RMS-L1-Error  d_L1  M1_L1  M2_L1  M3_L1  E_L1   B1c_L1  B2c_L1  B3c_L1  Largest-Max/L1  d_max  M1_max  M2_max  M3_max  E_max   B1c_max  B2c_max  B3c_max",
    "cpaw": "# Nx1  Nx2  Nx3  Ncycle  RMS-L1-Error  d_L1  M1_L1  M2_L1  M3_L1  E_L1   B1c_L1  B2c_L1  B3c_L1  Largest-Max/L1  d_max  M1_max  M2_max  M3_max  E_max   B1c_max  B2c_max  B3c_max",
    "shock_tube": "# Nx1  Nx2  Nx3  Ncycle  RMS-L1-Error  d_L1  M1_L1  M2_L1  M3_L1  E_L1   B1c_L1  B2c_L1  B3c_L1",
    "quirk": "# |s_odd - s_even|",
}


# ----------------------------------------------------------------- parameter dump
def deck_blocks(text: str) -> dict[str, dict[str, str]]:
    """Parse an athinput deck into ordered blocks of name/value pairs."""
    blocks: dict[str, dict[str, str]] = {}
    current: str | None = None
    for line in text.splitlines():
        header = BLOCK_RE.match(line.strip())
        if header:
            current = header.group(1)
            blocks.setdefault(current, {})
            continue
        if current is None or "=" not in line:
            continue
        body = line.split("#", 1)[0]
        if "=" not in body:
            continue
        name, value = body.split("=", 1)
        blocks[current][name.strip()] = value.strip()
    return blocks


def parameter_dump(spec: CheckSpec, plan: RunPlan, parent: RunPlan | None = None) -> str:
    """Render the running parameter set the pinned executable would dump.

    Overrides are applied exactly as `ParameterInput::ModifyFromCmdline` does,
    including its rule that an unknown block or parameter is a fatal error, so
    a rubric selector that does not exist in the check deck fails here too.  A
    restarted run inherits its parent's parameter set from the checkpoint
    (`ParameterInput::LoadFromFile` on the restart file) before its own
    restart-safe overrides are applied.
    """
    blocks = deck_blocks(spec.deck.read_text(encoding="utf-8"))
    for override in ((parent.overrides() if parent is not None else []) + plan.overrides()):
        key, value = override.split("=", 1)
        block, name = key.split("/", 1)
        if block not in blocks:
            raise ValueError(f"deck has no <{block}> block for override {override!r}")
        if name not in blocks[block]:
            raise ValueError(f"deck block <{block}> has no parameter {name!r} for override {override!r}")
        blocks[block][name] = value
    lines = ["#------------------------- PAR_DUMP -------------------------"]
    for block, entries in blocks.items():
        lines.append(f"<{block}>")
        for name, value in entries.items():
            lines.append(f"{name:<24}= {value}")
    lines.append("#------------------------- PAR_DUMP -------------------------")
    lines.append("<par_end>")
    return "\n".join(lines) + "\n"


# ----------------------------------------------------------------- formatted TAB
def write_tab(path: Path, plan: RunPlan, time: float, cycle: int, coords: dict[str, array.array],
              columns: dict[str, array.array], nghost: int) -> None:
    """One MeshBlock/frame table in the pinned `%25.17e` formatted-table layout."""
    bx, by, bz = plan.meshblock
    names = ["rho"] + (["press"] if plan.eos == "adiabatic" else []) + ["vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3"]
    head = ["# Athena++ data at time=%e" % time + "  cycle=%d" % cycle + "  variables=prim \n", "#"]
    if bx > 1:
        head.append(" i       x1v     ")
    if by > 1:
        head.append(" j       x2v     ")
    if bz > 1:
        head.append(" k       x3v     ")
    for name in names:
        head.append(f"    {name}      ")
    head.append("\n")
    out = [head[0], "".join(head[1:])]
    for k in range(bz):
        for j in range(by):
            for i in range(bx):
                n = (k * by + j) * bx + i
                row = []
                if bx > 1:
                    row.append("%04d" % (i + nghost) + DATA_FORMAT % coords["x1"][n])
                if by > 1:
                    row.append(" %04d" % (j + nghost) + DATA_FORMAT % coords["x2"][n])
                if bz > 1:
                    row.append(" %04d" % (k + nghost) + DATA_FORMAT % coords["x3"][n])
                for name in names:
                    row.append(DATA_FORMAT % columns[name][n])
                row.append("\n")
                out.append("".join(row))
    path.write_text("".join(out), encoding="utf-8")


# ----------------------------------------------------------------- restart dump
def _ghost_extent(n: int, nghost: int) -> int:
    return n + 2 * nghost if n > 1 else 1


def _pack_block(plan: RunPlan, nghost: int, density: array.array, faces: dict[str, array.array], nhydro: int) -> bytes:
    """Lay one MeshBlock out exactly as `RestartOutput::WriteOutputFile` does."""
    bx, by, bz = plan.meshblock
    n1, n2, n3 = _ghost_extent(bx, nghost), _ghost_extent(by, nghost), _ghost_extent(bz, nghost)
    is_ = nghost if bx > 1 else 0
    js = nghost if by > 1 else 0
    ks = nghost if bz > 1 else 0
    cells = n3 * n2 * n1
    values = array.array("d", bytes(8 * nhydro * cells))
    for k in range(bz):
        for j in range(by):
            for i in range(bx):
                values[((ks + k) * n2 + (js + j)) * n1 + (is_ + i)] = density[(k * by + j) * bx + i]
    payload = values
    for name, (fz, fy, fx), (gz, gy, gx) in (
        ("B1f", face_shapes([bz, by, bx])["B1f"], (n3, n2, n1 + 1)),
        ("B2f", face_shapes([bz, by, bx])["B2f"], (n3, n2 + 1, n1)),
        ("B3f", face_shapes([bz, by, bx])["B3f"], (n3 + 1, n2, n1)),
    ):
        block = array.array("d", bytes(8 * gz * gy * gx))
        for k in range(fz):
            for j in range(fy):
                for i in range(fx):
                    block[((ks + k) * gy + (js + j)) * gx + (is_ + i)] = faces[name][(k * fy + j) * fx + i]
        payload = payload + block
    data = payload.tobytes() if sys.byteorder == "little" else _swapped(payload)
    return data


def _swapped(values: array.array) -> bytes:
    copy = array.array("d", values)
    copy.byteswap()
    return copy.tobytes()


def write_restart(path: Path, spec: CheckSpec, plan: RunPlan, time: float, dt: float, ncycle: int,
                  mesh_size: dict[str, Any], blocks: list[dict[str, Any]], nghost: int,
                  parent: RunPlan | None = None) -> None:
    text = parameter_dump(spec, plan, parent).encode("utf-8")
    nhydro = 4 if plan.eos == "isothermal" else 5
    bx, by, bz = plan.meshblock
    n1, n2, n3 = _ghost_extent(bx, nghost), _ghost_extent(by, nghost), _ghost_extent(bz, nghost)
    cells = n3 * n2 * n1
    faces = n3 * n2 * (n1 + 1) + n3 * (n2 + 1) * n1 + (n3 + 1) * n2 * n1
    datasize = 8 * (nhydro * cells + faces)
    header = bytearray(text)
    header += struct.pack("<ii", len(blocks), 0)
    region = struct.pack("<9d3i", mesh_size["x1min"], mesh_size["x2min"], mesh_size["x3min"],
                         mesh_size["x1max"], mesh_size["x2max"], mesh_size["x3max"],
                         mesh_size["x1rat"], mesh_size["x2rat"], mesh_size["x3rat"],
                         mesh_size["nx1"], mesh_size["nx2"], mesh_size["nx3"])
    header += region + b"\x00" * NGHOST_PAD
    header += struct.pack("<2d", time, dt)
    header += struct.pack("<i", ncycle)
    header += struct.pack("<Q", datasize)
    for block in blocks:
        location = block["location"]
        header += struct.pack("<qqqi", location["lx1"], location["lx2"], location["lx3"], location["level"]) + b"\x00" * NGHOST_PAD
        header += struct.pack("<d", 1.0)
    body = bytearray()
    for block in blocks:
        payload = _pack_block(plan, nghost, block["columns"]["rho"], block["faces"], nhydro)
        if len(payload) != datasize:
            raise ValueError(f"packed MeshBlock is {len(payload)} bytes, header declares {datasize}")
        body += payload
    path.write_bytes(bytes(header) + bytes(body))


# ----------------------------------------------------------------- history/error/logs
def history_columns(eos: str) -> list[str]:
    return [name for name in HST_COLUMNS if not (eos == "isothermal" and name == "tot-E")]


def write_history(path: Path, eos: str, rows: list[list[float]], header: bool) -> None:
    lines = []
    if header:
        lines.append("# Athena++ history data\n")
        lines.append("# " + "".join(f"[{index + 1}]={name}     " for index, name in enumerate(history_columns(eos))) + "\n")
    for row in rows:
        lines.append("".join(DATA_FORMAT % value for value in row) + "\n")
    path.write_text("".join(lines), encoding="utf-8")


def write_error_file(path: Path, problem: str, plan: RunPlan, cycle: int, seed: float) -> None:
    if problem == "quirk":
        path.write_text(ERROR_HEADERS[problem] + "\n" + "%e\n" % (0.01 + seed * 1e-4), encoding="utf-8")
        return
    values = [float(plan.dimensions[0]), float(plan.dimensions[1]), float(plan.dimensions[2]), float(cycle)]
    values += [1.0e-7 + seed * 1e-9 + 1e-9 * index for index in range(9)]
    path.write_text(ERROR_HEADERS[problem] + "\n" + "  ".join("%e" % value for value in values) + "\n", encoding="utf-8")


def run_stdout(plan: RunPlan, final_time: float, final_cycle: int) -> str:
    lines = ["\nSetup complete, entering main loop...\n\n",
             "\nTerminating on time limit\n",
             "time=%g cycle=%d\n" % (final_time, final_cycle),
             "tlim=%g nlim=-1\n" % plan.tlim,
             "\nzone-cycles = 123456\n", "cpu time used  = 1.5\n", "zone-cycles/cpu_second = 82304\n"]
    if plan.launcher == "openmp":
        lines.append("\nomp wtime used = 0.9\nzone-cycles/omp_wsecond = 137173\n")
    return "".join(lines)


def mesh_probe_stdout(plan: RunPlan, tree: list[tuple[int, int, int, int]], ranks_of: dict[int, int]) -> str:
    root_blocks = [n // b for n, b in zip(plan.dimensions, plan.meshblock)]
    per_level: dict[int, int] = {}
    for level, *_ in tree:
        per_level[level] = per_level.get(level, 0) + 1
    per_rank: dict[int, int] = {rank: 0 for rank in range(plan.ranks)}
    for rank in ranks_of.values():
        per_rank[rank] += 1
    lines = ["\n", "Root grid = %d x %d x %d MeshBlocks\n" % tuple(root_blocks),
             "Total number of MeshBlocks = %d\n" % len(tree),
             "Number of physical refinement levels = %d\n" % (max(per_level) if per_level else 0),
             "Number of logical  refinement levels = %d\n" % (max(per_level) if per_level else 0)]
    for level in sorted(per_level):
        lines.append("  Physical level = %d (logical level = %d): %d MeshBlocks, cost = %d\n"
                     % (level, level, per_level[level], per_level[level]))
    lines.append("Number of parallel ranks = %d\n" % plan.ranks)
    for rank in sorted(per_rank):
        lines.append("  Rank = %d: %d MeshBlocks, cost = %d\n" % (rank, per_rank[rank], per_rank[rank]))
    return "".join(lines)


def mesh_structure(tree: list[tuple[int, int, int, int]], ranks_of: dict[int, int]) -> str:
    lines = []
    for gid, (level, lx1, lx2, lx3) in enumerate(tree):
        lines.append("#MeshBlock %d on rank=%d with cost=%g\n" % (gid, ranks_of[gid], 1.0))
        lines.append("#  Logical level %d, location = (%d %d %d)\n" % (level, lx1, lx2, lx3))
        lines.append("0 0 0\n\n\n")
    return "".join(lines)


def launcher_probe_log(ranks: int) -> str:
    return "".join("rank=%d size=%d pid=%d host=fixture-host\n" % (rank, ranks, 4000 + rank) for rank in range(ranks))


def show_config(plan: RunPlan) -> str:
    lines = ["This Athena++ executable is configured with:\n"]
    for key, value in plan.expected_show_config.items():
        lines.append("  %-28s%s\n" % (key + ":", value))
    lines.append("  %-28s%s\n" % ("Radiative Transfer:", "OFF"))
    lines.append("  %-28s%s\n" % ("Compiler:", "g++"))
    lines.append("  %-28s%s\n" % ("Compilation command:", "g++ -O2 -g0"))
    return "".join(lines)
