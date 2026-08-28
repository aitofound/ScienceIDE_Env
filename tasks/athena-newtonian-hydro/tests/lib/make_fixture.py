"""Deterministic accepted/rejected artifact fixture generator."""
from __future__ import annotations

import argparse
import copy
import json
import math
import os
import shutil
from pathlib import Path


def strict_load(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def write(path: Path, document: dict, *, allow_nan: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, allow_nan=allow_nan, separators=(",", ":")) + "\n", encoding="utf-8")


def link_or_copy(source: Path, destination: Path) -> None:
    """Reuse the immutable base fixture without multiplying a large production artifact."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.link(source, destination)
    except OSError:
        shutil.copyfile(source, destination)


def replace_once(source: Path, destination: Path, old: bytes, new: bytes) -> None:
    """Make one deterministic value mutation while preserving the full artifact shape."""
    data = source.read_bytes()
    if data.count(old) < 1:
        raise ValueError(f"fixture token not found: {old!r}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data.replace(old, new, 1))


def document(rubric: dict) -> dict:
    nx, ny, nz = rubric["dimensions"]
    rows = []
    for k in range(nz):
        for j in range(ny):
            for i in range(nx):
                # Fixed arithmetic, no RNG/clock/network: values are intentionally
                # easy to perturb while retaining the complete eight columns.
                rows.append([float(i), float(j), float(k), 1.0 + i * 1.0e-3,
                             0.9 + j * 1.0e-3, (i - j) * 1.0e-4,
                             (j - k) * 1.0e-4, (k - i) * 1.0e-4])
    frames = [{"time": float(time), "cycle": index, "rows": copy.deepcopy(rows)}
              for index, time in enumerate(rubric["expected_times"])]
    return {
        "schema": rubric["artifact_schema"], "case": rubric["case"],
        "dimensions": rubric["dimensions"],
        "variables": rubric["variables"],
        "primitive_fields": rubric["variables"][3:],
        "source_format": rubric["source_format"], "frames": frames,
    }


def short_document(base: dict, **changes) -> dict:
    """Create an early-reject document; validators fail before traversing production rows."""
    result = {key: base[key] for key in ("schema", "case", "dimensions", "variables", "primitive_fields", "source_format")}
    result["frames"] = []
    result.update(changes)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rubric", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    rubric = strict_load(args.rubric)
    base = document(rubric)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    base_path = args.output_dir / ".fixture-base-primitive_tab.json"
    write(base_path, base)

    # Accepted cases use hard links (or a copy fallback) to keep fixtures
    # deterministic and practical even for the authoritative 128x64x64 check.
    for case in ("accept-identity", "accept-roundtrip"):
        for side in ("reference", "candidate"):
            link_or_copy(base_path, args.output_dir / case / side / "primitive_tab.json")

    # A full-shape value near-miss exercises pointwise comparison.
    replace_once(base_path,
                 args.output_dir / "reject-value" / "candidate" / "primitive_tab.json",
                 b"1.0", b"1.000001")
    link_or_copy(base_path, args.output_dir / "reject-value" / "reference" / "primitive_tab.json")

    # These malformed cases intentionally fail conformance before row traversal,
    # so they remain small while still exercising their named failure classes.
    link_or_copy(base_path, args.output_dir / "reject-shape" / "reference" / "primitive_tab.json")
    write(args.output_dir / "reject-shape" / "candidate" / "primitive_tab.json",
          short_document(base, dimensions=[1, 1, 1]))

    nonfinite = short_document(base)
    nonfinite["dimensions"] = [1, 1, 1]
    nonfinite["frames"] = [{"time": 0.0, "cycle": 0, "rows": [[0.0, 0.0, 0.0, 1.0, float("nan"), 0.0, 0.0, 0.0]]}]
    link_or_copy(base_path, args.output_dir / "reject-nonfinite" / "reference" / "primitive_tab.json")
    write(args.output_dir / "reject-nonfinite" / "candidate" / "primitive_tab.json", nonfinite, allow_nan=True)

    link_or_copy(base_path, args.output_dir / "reject-metadata" / "reference" / "primitive_tab.json")
    write(args.output_dir / "reject-metadata" / "candidate" / "primitive_tab.json",
          short_document(base, case="wrong-case"))

    link_or_copy(base_path, args.output_dir / "reject-extra" / "reference" / "primitive_tab.json")
    link_or_copy(base_path, args.output_dir / "reject-extra" / "candidate" / "primitive_tab.json")
    (args.output_dir / "reject-extra" / "candidate" / "unexpected.txt").write_text("unexpected\n", encoding="utf-8")

    malformed = args.output_dir / "reject-malformed" / "candidate" / "primitive_tab.json"
    malformed.parent.mkdir(parents=True, exist_ok=True)
    malformed.write_text('{"schema":', encoding="utf-8")
    link_or_copy(base_path, args.output_dir / "reject-malformed" / "reference" / "primitive_tab.json")
    print(args.output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
