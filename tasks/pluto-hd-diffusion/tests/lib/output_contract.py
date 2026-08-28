#!/usr/bin/env python3
"""Strict native PLUTO single-file output contract (no trajectory data shipped)."""
from pathlib import Path
import json, math, re, struct, sys

class ContractError(ValueError):
    pass

_FRAME = re.compile(r"^data\.(\d+)\.dbl$")

def parse_native(root):
    root = Path(root)
    if not root.is_dir():
        raise ContractError("artifact directory missing")
    listing = root / "dbl.out"
    if not listing.is_file():
        raise ContractError("missing dbl.out")
    rows = []
    for line_no, line in enumerate(listing.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        fields = line.split()
        if line.lstrip().startswith("#") or len(fields) < 7:
            raise ContractError(f"malformed dbl.out line {line_no}")
        try:
            frame, time, dt, step = int(fields[0]), float(fields[1]), float(fields[2]), int(fields[3])
        except ValueError as exc:
            raise ContractError(f"unparseable dbl.out line {line_no}") from exc
        if frame < 0 or step < 0 or not math.isfinite(time) or not math.isfinite(dt) or dt <= 0:
            raise ContractError(f"invalid metadata line {line_no}")
        if fields[4] != "single_file" or fields[5] != "little":
            raise ContractError(f"non-native mode/endian at frame {frame}")
        names = fields[6:]
        if not names or len(set(names)) != len(names):
            raise ContractError("empty or duplicate variable schema")
        rows.append(dict(frame=frame, time=time, dt=dt, step=step,
                         mode=fields[4], endian=fields[5], variables=names))
    if not rows:
        raise ContractError("empty dbl.out")
    rows.sort(key=lambda row: row["frame"])
    if [row["frame"] for row in rows] != list(range(len(rows))):
        raise ContractError("non-contiguous or duplicate frame numbering")
    names = rows[0]["variables"]
    if any(row["variables"] != names for row in rows):
        raise ContractError("schema changes between frames")
    frames = []
    for row in rows:
        payload_path = root / f"data.{row['frame']:04d}.dbl"
        if not payload_path.is_file():
            raise ContractError(f"missing {payload_path.name}")
        raw = payload_path.read_bytes()
        nvar = len(names)
        if not raw or len(raw) % (8 * nvar):
            raise ContractError(f"{payload_path.name} is not native FP64 payload")
        ncell = len(raw) // (8 * nvar)
        if ncell <= 0:
            raise ContractError(f"{payload_path.name} has no cells")
        values = struct.unpack("<" + "d" * (ncell * nvar), raw)
        columns = [list(values[i*ncell:(i+1)*ncell]) for i in range(nvar)]
        if not all(math.isfinite(value) for col in columns for value in col):
            raise ContractError(f"{payload_path.name} contains NaN/Inf")
        for index, name in enumerate(names):
            if name.lower() in {"rho", "density", "prs", "press", "pressure"}:
                if any(value <= 0 for value in columns[index]):
                    raise ContractError(f"non-positive {name}")
        frames.append(dict(row, payload=dict(ncell=ncell, fields=columns)))
    listed = {f"data.{row['frame']:04d}.dbl" for row in rows}
    extras = [path.name for path in root.iterdir()
              if path.is_file() and _FRAME.match(path.name) and path.name not in listed]
    if extras:
        raise ContractError("unexpected frames: " + ",".join(sorted(extras)))
    observations = root / "runtime_observations.json"
    if not observations.is_file():
        raise ContractError("missing runtime_observations.json")
    try:
        observed = json.loads(observations.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise ContractError("invalid runtime_observations.json") from exc
    if not isinstance(observed, dict) or not isinstance(observed.get("observed"), dict):
        raise ContractError("observations must contain an object")
    return dict(rows=rows, variables=names, ncell=frames[0]["payload"]["ncell"],
                frames=frames, observations=observed)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: output_contract.py ARTIFACT_DIR")
    result = parse_native(sys.argv[1])
    print(json.dumps({"frames": len(result["frames"]), "variables": result["variables"],
                      "ncell": result["ncell"]}, sort_keys=True))
