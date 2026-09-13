#!/usr/bin/env python3
"""Extract source-auditable weighted physical scalars from EPOCH SDF dumps.

All coordinates are SI metres, electron number density is m^-3, mean electron
energy is J, E is V/m, B is tesla, and Jx is A/m^2.  These 2-D decks follow
EPOCH's explicit per-unit-out-of-plane-depth convention: Grid/Grid spans x-y,
so dA=dx*dy and the global integrals have units m^-1, J/m, and A.  Only one
scalar per observable and required dump is emitted; no cellwise PIC array is
graded.
"""
from __future__ import annotations
import struct, sys
from pathlib import Path
import numpy as np
PREFIX = 'ramp'
DUMPS = (1, 2)
EPS0, MU0 = 8.8541878128e-12, 1.25663706212e-6
SDF_ENDIANNESS_LE = 16911887
BLOCK_PLAIN_MESH, BLOCK_PLAIN_VARIABLE = 1, 3
DTYPES = {1: ("<i4", 4), 2: ("<i8", 8), 3: ("<f4", 4), 4: ("<f8", 8)}

def _s(buf: bytes, off: int, n: int) -> str:
    return buf[off:off+n].split(b"\x00", 1)[0].decode("ascii", "replace").strip()

def read_blocks(path: Path) -> dict:
    buf = path.read_bytes()
    if buf[:4] != b"SDF1" or struct.unpack_from("<i", buf, 4)[0] != SDF_ENDIANNESS_LE:
        raise ValueError(f"{path}: not a little-endian SDF file")
    nblocks = struct.unpack_from("<i", buf, 68)[0]
    header_len, strlen = struct.unpack_from("<ii", buf, 72)[0], struct.unpack_from("<i", buf, 96)[0]
    loc, out, seen = struct.unpack_from("<q", buf, 48)[0], {}, 0
    while loc and loc < len(buf) and seen < 4 * max(nblocks, 1) + 8:
        seen += 1
        nxt, data_loc = struct.unpack_from("<qq", buf, loc)
        blocktype, datatype, ndims = struct.unpack_from("<iii", buf, loc + 56)
        name, meta = _s(buf, loc + 68, strlen), loc + header_len
        if blocktype == BLOCK_PLAIN_MESH and datatype in DTYPES:
            off = meta + 8*ndims + 32*ndims + 32*ndims + 4 + 16*ndims
            dims = struct.unpack_from(f"<{ndims}i", buf, off)
            dt, size, axes, pos = DTYPES[datatype][0], DTYPES[datatype][1], [], data_loc
            for d in dims:
                axes.append(np.frombuffer(buf, dtype=dt, count=d, offset=pos).astype(np.float64))
                pos += d * size
            out[name] = axes
        elif blocktype == BLOCK_PLAIN_VARIABLE and datatype in DTYPES:
            dims = struct.unpack_from(f"<{ndims}i", buf, meta + 72)
            dt = DTYPES[datatype][0]
            out[name] = np.frombuffer(buf, dtype=dt, count=int(np.prod(dims)), offset=data_loc).astype(np.float64).reshape(dims, order="F")
        if nxt <= loc:
            break
        loc = nxt
    return out

def checked(name: str, value, shape=None, nonnegative=False) -> np.ndarray:
    a = np.asarray(value, dtype=np.float64)
    if shape is not None and a.shape != shape:
        raise ValueError(f"{name}: shape {a.shape}, expected {shape}")
    if not np.all(np.isfinite(a)):
        raise ValueError(f"{name}: non-finite value")
    if nonnegative and np.any(a < 0):
        raise ValueError(f"{name}: negative physical value")
    return a

def geometry(blocks: dict, shape: tuple[int, ...]):
    axes = blocks.get("Grid/Grid")
    if not isinstance(axes, list) or len(axes) != len(shape):
        raise ValueError(f"Grid/Grid: expected {len(shape)} axes for density shape {shape}")
    widths, centers = [], []
    for axis, cells, name in zip(axes, shape, "xyz"):
        a = checked(f"Grid/Grid/{name}", axis)
        if a.ndim != 1 or a.size != cells + 1:
            raise ValueError(f"Grid/Grid/{name}: {a.size} nodes, expected {cells + 1}")
        d = np.diff(a)
        if not np.all(np.isfinite(d)) or np.any(d <= 0):
            raise ValueError(f"Grid/Grid/{name}: non-positive or non-finite cell width")
        widths.append(d)
        centers.append(0.5 * (a[:-1] + a[1:]))
    area = widths[0]
    for d in widths[1:]:
        area = np.multiply.outer(area, d)
    if area.shape != shape or not np.all(np.isfinite(area)) or np.any(area <= 0):
        raise ValueError(f"cell area: invalid shape or positivity for {shape}")
    return centers, area

def scalar(out_dir: Path, name: str, value: float) -> None:
    if not np.isfinite(value):
        raise ValueError(f"{name}: non-finite scalar")
    np.asarray([value], dtype="<f8").tofile(out_dir / name)

def main() -> int:
    run_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    for dump in DUMPS:
        b = read_blocks(run_dir / f"{dump:04d}.sdf")
        rho = checked("Derived/Number_Density/electron", b["Derived/Number_Density/electron"], nonnegative=True)
        shape = rho.shape
        if rho.ndim != 2:
            raise ValueError(f"electron density: expected 2-D, got {shape}")
        (x, y), darea = geometry(b, shape)
        ek = checked("Derived/Average_Particle_Energy/electron", b["Derived/Average_Particle_Energy/electron"], shape, nonnegative=True)
        ex, ey, ez = (checked(f"Electric Field/{q}", b[f"Electric Field/{q}"], shape) for q in ("Ex", "Ey", "Ez"))
        bx, by, bz = (checked(f"Magnetic Field/{q}", b[f"Magnetic Field/{q}"], shape) for q in ("Bx", "By", "Bz"))
        jx = checked("Current/Jx", b["Current/Jx"], shape)
        number = float(np.sum(rho * darea))
        if not np.isfinite(number) or number <= 0:
            raise ValueError(f"electron number: invalid {number}")
        kinetic = float(np.sum(rho * ek * darea))
        em_density = 0.5 * (EPS0*(ex*ex + ey*ey + ez*ez) + (bx*bx + by*by + bz*bz)/MU0)
        em_density = checked("electromagnetic energy density", em_density, shape, nonnegative=True)
        em_energy = float(np.sum(em_density * darea))
        xg, yg = np.meshgrid(x, y, indexing="ij")
        weight = rho * darea
        xbar, ybar = float(np.sum(weight*xg)/number), float(np.sum(weight*yg)/number)
        xv = float(np.sum(weight*(xg-xbar)**2)/number); yv = float(np.sum(weight*(yg-ybar)**2)/number)
        if xv < 0 or yv < 0: raise ValueError("density moment variance: negative")
        values = (("electron_number_per_depth", number), ("electron_kinetic_energy_per_depth", kinetic),
                  ("electromagnetic_energy_per_depth", em_energy), ("electron_x_centroid", xbar),
                  ("electron_y_centroid", ybar), ("electron_x_rms", float(np.sqrt(xv))),
                  ("electron_y_rms", float(np.sqrt(yv))), ("current_x_l1_per_depth", float(np.sum(np.abs(jx)*darea))))
        for name, value in values:
            scalar(out_dir, f"{PREFIX}_{name}_dump{dump:04d}.f64", value)
    return 0
if __name__ == "__main__": raise SystemExit(main())
