#!/usr/bin/env python3
"""Parse a native Athena++ restart dump (.rst) written by src/outputs/restart.cpp.

The restart file is the only native Athena++ output in this build that carries
the face-centred magnetic field (FaceField b.x1f/x2f/x3f) after the CT update,
plus the MeshBlock tree (LogicalLocation per block, root level, mesh size).
The layout is read exactly as written by RestartOutput::WriteOutputFile:

    <parameter dump text>...<par_end>\n
    int32 nbtotal, int32 root_level, RegionSize (9 doubles, 3 int32, padded to
    88 bytes), double time, double dt, int32 ncycle, uint64 datasize,
    [user Mesh data: must be absent for the supported problem generators],
    nbtotal x (LogicalLocation {3 x int64, int32 level, pad} + double cost),
    nbtotal x datasize bytes of block data in gid order:
        hydro u[NHYDRO][ncells3][ncells2][ncells1]
        b.x1f[ncells3][ncells2][ncells1+1]
        b.x2f[ncells3][ncells2+1][ncells1]
        b.x3f[ncells3+1][ncells2][ncells1]

Everything is verified by size arithmetic; nothing is guessed.  The parser is a
task-local reader with no numerical policy of its own.
"""
from __future__ import annotations

import array
import hashlib
import re
import struct
import sys
from pathlib import Path
from typing import Any

NGHOST = 2
PAR_END = b"<par_end>"
REGION_SIZE_BYTES = 88
LOGICAL_LOCATION_BYTES = 32


class RestartFormatError(ValueError):
    pass


def block_param(text: str, block: str, name: str) -> str:
    """Read one parameter value from the dumped parameter text.

    `ParameterInput::ParameterDump` writes `<block>` on its own line and then
    `name<pad>= value<pad># comment` for every parameter of the *running*
    parameter set, so this is the executed configuration as the pinned
    executable saw it, including command-line overrides.
    """
    section = re.search(rf"(?ms)^<{re.escape(block)}>\s*$(.*?)(?=^<|\Z)", text)
    if section is None:
        raise RestartFormatError(f"parameter dump lacks <{block}> block")
    match = re.search(rf"(?m)^\s*{re.escape(name)}\s*=\s*(\S+)", section.group(1))
    if match is None:
        raise RestartFormatError(f"parameter dump lacks {block}/{name}")
    return match.group(1)


def _slice(values: array.array, base: int, dims: tuple[int, int, int], k0: int, k1: int, j0: int, j1: int, i0: int, i1: int) -> array.array:
    """Extract an inclusive [k0..k1][j0..j1][i0..i1] box from a flat (n3,n2,n1) array."""
    n3, n2, n1 = dims
    out = array.array("d")
    for k in range(k0, k1 + 1):
        for j in range(j0, j1 + 1):
            start = base + (k * n2 + j) * n1 + i0
            out.extend(values[start:start + (i1 - i0 + 1)])
    return out


def parse_restart(path: Path, eos: str, nghost: int = NGHOST) -> dict[str, Any]:
    """Parse one restart dump into a JSON-ready dictionary of native evidence.

    ``nghost`` is the build's ghost-cell count (configure --nghost); it fixes
    the ghost-inclusive array extents and is verified by the size arithmetic.
    """
    if not isinstance(nghost, int) or nghost < 1:
        raise RestartFormatError("nghost must be a positive integer")
    data = path.read_bytes()
    loc = data.find(PAR_END)
    if loc < 0:
        raise RestartFormatError("restart dump has no <par_end> marker")
    text = data[:loc].decode("utf-8", errors="replace")
    off = loc + len(PAR_END) + 1  # "<par_end>\n" as read by ParameterInput::LoadFromFile
    try:
        nbtotal, root_level = struct.unpack_from("<ii", data, off)
        off += 8
        region = struct.unpack_from("<9d3i", data, off)
        off += REGION_SIZE_BYTES
        time, dt = struct.unpack_from("<2d", data, off)
        off += 16
        (ncycle,) = struct.unpack_from("<i", data, off)
        off += 4
        (datasize,) = struct.unpack_from("<Q", data, off)
        off += 8
    except struct.error as exc:
        raise RestartFormatError(f"restart header truncated: {exc}") from exc
    if nbtotal <= 0 or root_level < 0 or datasize <= 0 or datasize % 8:
        raise RestartFormatError("restart header values are not plausible")
    mesh_size = {
        "x1min": region[0], "x2min": region[1], "x3min": region[2],
        "x1max": region[3], "x2max": region[4], "x3max": region[5],
        "x1rat": region[6], "x2rat": region[7], "x3rat": region[8],
        "nx1": region[9], "nx2": region[10], "nx3": region[11],
    }
    listsize = LOGICAL_LOCATION_BYTES + 8
    expected_total = off + nbtotal * (listsize + datasize)
    if len(data) != expected_total:
        raise RestartFormatError(
            f"restart size {len(data)} != header+list+data {expected_total}; user Mesh data or a foreign layout is present"
        )
    block_nx = tuple(int(block_param(text, "meshblock", f"nx{d}")) for d in (1, 2, 3))
    if any(n <= 0 for n in block_nx) or any(mesh_size[f"nx{d}"] % block_nx[d - 1] for d in (1, 2, 3)):
        raise RestartFormatError("meshblock size does not tile the mesh")
    nx1, nx2, nx3 = block_nx
    ncells1 = nx1 + 2 * nghost
    ncells2 = nx2 + 2 * nghost if nx2 > 1 else 1
    ncells3 = nx3 + 2 * nghost if nx3 > 1 else 1
    cells = ncells3 * ncells2 * ncells1
    faces = ncells3 * ncells2 * (ncells1 + 1) + ncells3 * (ncells2 + 1) * ncells1 + (ncells3 + 1) * ncells2 * ncells1
    doubles = datasize // 8
    if (doubles - faces) % cells:
        raise RestartFormatError(f"block data size is not hydro cells plus three face fields for nghost={nghost}")
    nhydro = (doubles - faces) // cells
    expected_nhydro = 4 if eos == "isothermal" else 5
    if nhydro != expected_nhydro:
        raise RestartFormatError(f"restart block carries NHYDRO={nhydro}, expected {expected_nhydro} for {eos} MHD")
    locations = []
    for _ in range(nbtotal):
        lx1, lx2, lx3, level = struct.unpack_from("<qqqi", data, off)
        off += LOGICAL_LOCATION_BYTES
        (cost,) = struct.unpack_from("<d", data, off)
        off += 8
        if level < root_level:
            raise RestartFormatError("block level below root level")
        locations.append({"lx1": lx1, "lx2": lx2, "lx3": lx3, "level": level - root_level, "cost": cost})
    is_, ie = nghost, nghost + nx1 - 1
    js, je = (nghost, nghost + nx2 - 1) if nx2 > 1 else (0, 0)
    ks, ke = (nghost, nghost + nx3 - 1) if nx3 > 1 else (0, 0)
    blocks = []
    for gid in range(nbtotal):
        values = array.array("d")
        values.frombytes(data[off:off + datasize])
        if sys.byteorder != "little":
            values.byteswap()
        off += datasize
        base_u = 0
        base_x1f = base_u + nhydro * cells
        base_x2f = base_x1f + ncells3 * ncells2 * (ncells1 + 1)
        base_x3f = base_x2f + ncells3 * (ncells2 + 1) * ncells1
        density = _slice(values, base_u, (ncells3, ncells2, ncells1), ks, ke, js, je, is_, ie)
        b1f = _slice(values, base_x1f, (ncells3, ncells2, ncells1 + 1), ks, ke, js, je, is_, ie + 1)
        b2f = _slice(values, base_x2f, (ncells3, ncells2 + 1, ncells1), ks, ke, js, je + 1, is_, ie)
        b3f = _slice(values, base_x3f, (ncells3 + 1, ncells2, ncells1), ks, ke + 1, js, je, is_, ie)
        blocks.append({
            "gid": gid,
            "location": locations[gid],
            "shape": [nx3, nx2, nx1],
            "density": density,
            "B1f": b1f,
            "B2f": b2f,
            "B3f": b3f,
        })
    return {
        "file": path.name,
        "param_text": text,
        "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data),
        "time": time,
        "dt": dt,
        "ncycle": ncycle,
        "nbtotal": nbtotal,
        "root_level": root_level,
        "mesh_size": mesh_size,
        "meshblock": list(block_nx),
        "nhydro": nhydro,
        "nghost": nghost,
        "blocks": blocks,
    }


if __name__ == "__main__":
    import json
    document = parse_restart(Path(sys.argv[1]), sys.argv[2] if len(sys.argv) > 2 else "adiabatic", int(sys.argv[3]) if len(sys.argv) > 3 else NGHOST)
    summary = {k: v for k, v in document.items() if k not in ("blocks", "param_text")}
    summary["block_count"] = len(document["blocks"])
    summary["levels"] = sorted({b["location"]["level"] for b in document["blocks"]})
    summary["first_block_face_counts"] = [len(document["blocks"][0][key]) for key in ("B1f", "B2f", "B3f")]
    print(json.dumps(summary, indent=1))
