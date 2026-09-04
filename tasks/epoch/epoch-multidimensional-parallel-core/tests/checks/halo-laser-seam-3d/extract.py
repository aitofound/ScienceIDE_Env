#!/usr/bin/env python3
"""Extract this check's graded arrays from EPOCH's SDF dumps.

Self-contained: standard library plus numpy, no imports from outside this
check directory. Decodes the SDF binary format directly from the bytes the
pinned EPOCH writes, following the layout in
code/epoch/SDF/documentation/sdf_format.tex and the pinned Fortran writer
under code/epoch/SDF/FORTRAN/src. Only the block types this deck produces
are decoded: plain variable (3, the assembled global field and derived grid
arrays), constant (5, the reduced scalars), array (6) and cpu-split (20, the
per-axis rank partition ladder and the per-species per-rank particle counts).

The SDF header carries a run date and a machine name, so the dumps are never
compared as bytes; each graded block is written out as a raw little-endian
float64 array, one file per block per dump, named as rubric.json lists it.

    python3 extract.py <run directory> <output directory>
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import numpy as np

MAGIC = b"SDF1"
ENDIANNESS_LE = 16911887
BT_PLAIN_VARIABLE, BT_CONSTANT, BT_ARRAY, BT_CPU_SPLIT = 3, 5, 6, 20
# SDF datatype code -> (numpy dtype string, bytes per element)
DATATYPES = {1: ("<i4", 4), 2: ("<i8", 8), 3: ("<f4", 4), 4: ("<f8", 8), 7: ("<u1", 1)}


def read_sdf(path: Path) -> dict:
    """Return {block name: float64 ndarray} for one .sdf dump."""
    buf = path.read_bytes()
    if buf[:4] != MAGIC:
        raise ValueError(f"{path}: no SDF1 magic")
    if struct.unpack_from("<i", buf, 4)[0] != ENDIANNESS_LE:
        raise ValueError(f"{path}: not a little-endian SDF file")
    first_block = struct.unpack_from("<q", buf, 48)[0]
    nblocks = struct.unpack_from("<i", buf, 68)[0]
    header_length = struct.unpack_from("<i", buf, 72)[0]
    string_length = struct.unpack_from("<i", buf, 96)[0]
    if nblocks == 0:
        raise ValueError(f"{path}: nblocks is zero, the dump was not finished")
    blocks: dict = {}
    location = first_block
    for _ in range(nblocks):
        next_location, data_location = struct.unpack_from("<qq", buf, location)
        blocktype, datatype, ndims = struct.unpack_from("<iii", buf, location + 56)
        raw_name = buf[location + 68:location + 68 + string_length]
        name = raw_name.split(b"\x00", 1)[0].decode("ascii", "replace").strip()
        meta = location + header_length
        data = None
        if datatype in DATATYPES:
            if blocktype == BT_PLAIN_VARIABLE:
                # mult(8) + units(32) + mesh id(32) = 72 bytes, then dims
                dims = struct.unpack_from(f"<{ndims}i", buf, meta + 72)
                data = _array(buf, data_location, datatype, dims)
            elif blocktype == BT_CONSTANT:
                # the value lives in the metadata section, not the data section
                fmt = {"<i4": "<i", "<i8": "<q", "<f4": "<f", "<f8": "<d",
                       "<u1": "<B"}[DATATYPES[datatype][0]]
                data = np.array([struct.unpack_from(fmt, buf, meta)[0]], dtype=np.float64)
            elif blocktype == BT_ARRAY:
                dims = struct.unpack_from(f"<{ndims}i", buf, meta)
                data = _array(buf, data_location, datatype, dims)
            elif blocktype == BT_CPU_SPLIT:
                # write_cpu_split_meta (SDF/FORTRAN/src/sdf_output_ru.f90):
                # geometry INTEGER(i4) then dims ndims*INTEGER(i4); the payload
                # is sum(dims) elements written one axis after another.
                dims = struct.unpack_from(f"<{ndims}i", buf, meta + 4)
                data = _array(buf, data_location, datatype, (sum(dims),))
        if data is not None:
            blocks[name] = data
        if next_location <= location:
            break
        location = next_location
    return blocks


def _array(buf: bytes, offset: int, datatype: int, dims) -> np.ndarray:
    dtype, itemsize = DATATYPES[datatype]
    count = 1
    for d in dims:
        count *= int(d)
    raw = buf[offset:offset + count * itemsize]
    if len(raw) != count * itemsize:
        raise ValueError(f"short SDF payload: wanted {count * itemsize} bytes, found {len(raw)}")
    # SDF writes multi-dimensional arrays in Fortran order; the graded
    # comparison is elementwise over the whole array, so it is kept flat.
    return np.array(np.frombuffer(raw, dtype=dtype, count=count), dtype=np.float64)


# (dump index, SDF block name, graded file stem). The two graded
# dumps are 0001 and 0002; the rank partition ladder is taken
# from dump 0002.
GRADED = [
    (1, "Electric Field/Ex", "ex"),
    (1, "Electric Field/Ey", "ey"),
    (1, "Magnetic Field/Bz", "bz"),
    (2, "Electric Field/Ex", "ex"),
    (2, "Electric Field/Ey", "ey"),
    (2, "Magnetic Field/Bz", "bz"),
    (2, "CPUs/Original rank", "cpu_rank"),
]


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    run_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    wanted: dict = {}
    for dump, block, key in GRADED:
        wanted.setdefault(int(dump), []).append((block, key))
    for dump, items in sorted(wanted.items()):
        source = run_dir / f"{dump:04d}.sdf"
        blocks = read_sdf(source)
        for block, key in items:
            if block not in blocks:
                raise SystemExit(f"extract.py: {source} has no block {block!r}; "
                                 f"present: {sorted(blocks)}")
            arr = np.ascontiguousarray(blocks[block], dtype=np.float64)
            arr.tofile(out_dir / f"{key}_{dump:04d}.f64")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
