#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import h5py
import numpy as np


EXCLUDE = ("tag", "id", "iter", "step", "time", "timing", "axis")


def arrays(root: Path):
    for path in sorted(root.rglob("*.h5")):
        rel = path.relative_to(root).as_posix()
        with h5py.File(path, "r") as handle:
            found = []
            def visit(name, obj):
                if isinstance(obj, h5py.Dataset) and np.issubdtype(obj.dtype, np.number):
                    key = f"{rel}:{name}".lower()
                    if not any(token in key for token in EXCLUDE):
                        found.append((key, np.asarray(obj[...], dtype=np.float64).ravel()))
            handle.visititems(visit)
            for item in found:
                yield item


def group(key: str) -> str:
    low = key.lower()
    if any(x in low for x in ("/fld", "e1", "e2", "e3", "b1", "b2", "b3", "field")):
        return "field"
    if any(x in low for x in ("/current", "j1", "j2", "j3")):
        return "current"
    if "charge" in low or "rho" in low:
        return "charge"
    if any(x in low for x in ("position", "/x1", "/x2", "/x3")):
        return "position"
    if any(x in low for x in ("momentum", "/p1", "/p2", "/p3", "/u1", "/u2", "/u3")):
        return "momentum"
    return "other"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("mode", choices=("pointwise", "invariants"))
    args = ap.parse_args()
    data = list(arrays(args.root))
    if not data:
        raise SystemExit("no numeric HDF5 production datasets were found")
    args.out.mkdir(parents=True, exist_ok=True)
    if args.mode == "pointwise":
        kept = [a for key, a in data if group(key) in {"field", "current", "charge"}]
        if not kept:
            raise SystemExit("no cell-keyed field, current, or charge datasets were found")
        np.save(args.out / "grid.npy", np.concatenate(kept))
        return 0
    groups = {name: [] for name in ("field", "current", "charge", "position", "momentum", "other")}
    for key, values in data:
        if values.size:
            groups[group(key)].append(values)
    metrics = []
    for name in ("field", "current", "charge", "position", "momentum", "other"):
        vals = np.concatenate(groups[name]) if groups[name] else np.zeros(0, dtype=np.float64)
        if vals.size:
            metrics.append(float(np.linalg.norm(vals)))
        else:
            metrics.append(0.0)
    np.savetxt(args.out / "metrics.txt", np.asarray(metrics, dtype=np.float64)[None, :], fmt="%.17e")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
