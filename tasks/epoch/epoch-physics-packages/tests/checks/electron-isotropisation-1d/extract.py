#!/usr/bin/env python3
"""Check electron-isotropisation-1d: turn the EPOCH SDF dumps of one run into the graded text
series this check compares.

Each row is one SDF dump. The graded columns are the domain-mean directional
electron temperatures, the anisotropy (Tx - (Ty+Tz)/2)/T and the
fraction of that anisotropy which has relaxed since the first dump,
1 - a(t)/a(0) -- the isotropisation curve, normalised so that the
finite-sample offset of the initial anisotropic Maxwellian cancels -- plus
the total energy (particles plus field) whose drift the policy bounds.

Standard library and numpy only; self-contained (nothing is imported from
outside this check directory).

    python3 extract.py --run <run directory with NNNN.sdf> --out <OUT_DIR>
"""
from __future__ import annotations

import argparse
import glob
import os
import struct

import numpy as np


# --- Minimal SDF reader: only the block types this check reads ----------------
# EPOCH writes its dumps in the SDF container documented in
# code/epoch/SDF/documentation/sdf_format.tex. This reader decodes exactly two
# block types from the raw bytes: CONSTANT (a scalar, block type 5) and
# PLAIN_VARIABLE (a grid array, block type 3). Every other block -- including
# the run-info block that carries the run date and the machine name -- is
# skipped after its header, so nothing environment-dependent is ever read.
_MAGIC = b"SDF1"
_LE = 16911887
_DTYPE = {1: "<i4", 2: "<i8", 3: "<f4", 4: "<f8", 7: "<u1"}
_SCALAR_FMT = {1: "<i", 2: "<q", 3: "<f", 4: "<d", 7: "<B"}


def read_sdf(path):
    """Return (time, {block name: scalar or numpy array}) for one .sdf file."""
    with open(path, "rb") as fh:
        buf = fh.read()
    if buf[0:4] != _MAGIC:
        raise ValueError("%s: not an SDF file" % path)
    if struct.unpack_from("<i", buf, 4)[0] != _LE:
        raise ValueError("%s: not little-endian SDF" % path)
    if struct.unpack_from("<i", buf, 8)[0] > 1:
        raise ValueError("%s: unsupported SDF version" % path)
    first = struct.unpack_from("<q", buf, 48)[0]
    nblocks = struct.unpack_from("<i", buf, 68)[0]
    header_len = struct.unpack_from("<i", buf, 72)[0]
    time = struct.unpack_from("<d", buf, 80)[0]
    strlen = struct.unpack_from("<i", buf, 96)[0]
    if nblocks == 0:
        raise ValueError("%s: nblocks == 0, the dump was never finished" % path)
    blocks = {}
    loc = first
    for _ in range(nblocks):
        nxt, data_loc = struct.unpack_from("<qq", buf, loc)
        blocktype, datatype, ndims = struct.unpack_from("<iii", buf, loc + 56)
        name = buf[loc + 68:loc + 68 + strlen].split(b"\x00", 1)[0].decode("ascii", "replace").strip()
        meta = loc + header_len
        if blocktype == 5 and datatype in _SCALAR_FMT:          # CONSTANT scalar
            blocks[name] = struct.unpack_from(_SCALAR_FMT[datatype], buf, meta)[0]
        elif blocktype == 3 and datatype in _DTYPE:             # PLAIN_VARIABLE grid
            common = meta + 72
            dims = struct.unpack_from("<%di" % ndims, buf, common)
            count = 1
            for d in dims:
                count *= d
            arr = np.frombuffer(buf, dtype=_DTYPE[datatype], count=count, offset=data_loc)
            blocks[name] = np.array(arr, dtype=np.float64)
        if nxt <= loc:
            break
        loc = nxt
    return time, blocks


def dumps(run_dir):
    paths = sorted(glob.glob(os.path.join(run_dir, "[0-9][0-9][0-9][0-9].sdf")))
    if not paths:
        raise SystemExit("extract.py: no .sdf dumps under %s" % run_dir)
    return paths


def write_table(path, header, rows):
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("# " + "  ".join(header) + "\n")
        for row in rows:
            fh.write("  ".join("%.17e" % v for v in row) + "\n")

HEADER = ["time_s", "electron_energy_J", "field_energy_J", "total_energy_J",
          "temperature_x_eV", "temperature_y_eV", "temperature_z_eV",
          "anisotropy", "anisotropy_relaxed_fraction", "dump_index"]

_KB_OVER_QE = 1.380649e-23 / 1.602176634e-19   # kelvin -> electronvolt


def build_rows(paths):
    raw = []
    for path in paths:
        time, b = read_sdf(path)
        tx = float(b["Derived/Temperature_x/electrons"].mean()) * _KB_OVER_QE
        ty = float(b["Derived/Temperature_y/electrons"].mean()) * _KB_OVER_QE
        tz = float(b["Derived/Temperature_z/electrons"].mean()) * _KB_OVER_QE
        raw.append((time, int(os.path.splitext(os.path.basename(path))[0]),
                    float(b["Total Particle Energy/electrons (J)"]),
                    float(b["Total Field Energy in Simulation (J)"]),
                    tx, ty, tz))
    aniso = [(tx - 0.5 * (ty + tz)) / ((tx + ty + tz) / 3.0)
             for _, _, _, _, tx, ty, tz in raw]
    a0 = aniso[0]
    rows = []
    for (time, dump_index, ee, ef, tx, ty, tz), an in zip(raw, aniso):
        rows.append([time, ee, ef, ee + ef, tx, ty, tz, an, 1.0 - an / a0,
                     dump_index])
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--run", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    rows = build_rows(dumps(a.run))
    os.makedirs(a.out, exist_ok=True)
    write_table(os.path.join(a.out, "series.txt"), HEADER, rows)
    write_table(os.path.join(a.out, "early.txt"), HEADER, rows[1:7])
    write_table(os.path.join(a.out, "tail.txt"), HEADER, rows[-5:])

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
