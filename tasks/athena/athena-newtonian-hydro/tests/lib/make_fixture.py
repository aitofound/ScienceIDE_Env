"""Deterministic accepted/rejected artifact fixture generator."""
from __future__ import annotations

import argparse
import copy
import json
import math
import os
import shutil
from pathlib import Path
from typing import Any


class DuplicateKey(ValueError):
    pass


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKey(f"duplicate key {key!r}")
        result[key] = value
    return result


def _constant(value: str) -> None:
    raise ValueError(f"non-standard JSON constant {value}")


def _finite_tree(value: Any) -> bool:
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return True
    if isinstance(value, (int, float)):
        try:
            return math.isfinite(float(value))
        except (OverflowError, TypeError, ValueError):
            return False
    if isinstance(value, list):
        return all(_finite_tree(item) for item in value)
    if isinstance(value, dict):
        return all(isinstance(key, str) and _finite_tree(item) for key, item in value.items())
    return False


def strict_load(path: Path) -> dict[str, Any]:
    document = json.loads(
        path.read_text(encoding="utf-8"),
        object_pairs_hook=_pairs,
        parse_constant=_constant,
    )
    if not isinstance(document, dict) or not _finite_tree(document):
        raise ValueError("rubric must be a finite strict JSON object")
    return document


def write(path: Path, document: dict[str, Any], *, allow_nan: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(document, allow_nan=allow_nan, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )


def link_or_copy(source: Path, destination: Path) -> None:
    """Reuse the immutable base fixture without multiplying a large artifact."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.link(source, destination)
    except OSError:
        shutil.copyfile(source, destination)


def replace_once(source: Path, destination: Path, old: bytes, new: bytes) -> None:
    """Make one deterministic value mutation while preserving artifact shape."""
    data = source.read_bytes()
    if data.count(old) < 1:
        raise ValueError(f"fixture token not found: {old!r}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data.replace(old, new, 1))


_STREAM_CHUNK = 1024 * 1024


def replace_once_streaming(source: Path, destination: Path, old: bytes, new: bytes) -> None:
    """Replace one token while copying in bounded chunks, not as a full byte blob."""
    if not old:
        raise ValueError("fixture replacement token must not be empty")
    destination.parent.mkdir(parents=True, exist_ok=True)
    carry = b""
    with source.open("rb") as source_stream, destination.open("wb") as destination_stream:
        while True:
            chunk = source_stream.read(_STREAM_CHUNK)
            if not chunk:
                if carry:
                    destination_stream.write(carry)
                raise ValueError(f"fixture token not found: {old!r}")
            data = carry + chunk
            index = data.find(old)
            if index >= 0:
                destination_stream.write(data[:index])
                destination_stream.write(new)
                destination_stream.write(data[index + len(old):])
                shutil.copyfileobj(source_stream, destination_stream, _STREAM_CHUNK)
                return
            overlap = min(len(old) - 1, len(data))
            if overlap:
                destination_stream.write(data[:-overlap])
                carry = data[-overlap:]
            else:
                destination_stream.write(data)


def replace_first_row_x1(source: Path, destination: Path) -> None:
    """Mutate only frame 0/row 0 x1, even when the artifact is hundreds of MB."""
    marker = b'"rows":[[0.0,'
    replacement = b'"rows":[[0.000001,'
    replace_once_streaming(source, destination, marker, replacement)


def symlink_relative(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    os.symlink(os.path.relpath(source, destination.parent), destination)


def document(rubric: dict[str, Any]) -> dict[str, Any]:
    nx, ny, nz = rubric["dimensions"]
    rows = []
    for k in range(nz):
        for j in range(ny):
            for i in range(nx):
                # Fixed arithmetic, no RNG/clock/network: values are intentionally
                # easy to perturb while retaining the complete eight columns.
                rows.append([
                    float(i), float(j), float(k), 1.0 + i * 1.0e-3,
                    0.9 + j * 1.0e-3, (i - j) * 1.0e-4,
                    (j - k) * 1.0e-4, (k - i) * 1.0e-4,
                ])
    frames = [
        {"time": float(time), "cycle": index, "rows": copy.deepcopy(rows)}
        for index, time in enumerate(rubric["expected_times"])
    ]
    return {
        "schema": rubric["artifact_schema"],
        "case": rubric["case"],
        "dimensions": rubric["dimensions"],
        "variables": rubric["variables"],
        "primitive_fields": rubric["variables"][3:],
        "source_format": rubric["source_format"],
        "frames": frames,
    }


def short_document(base: dict[str, Any], **changes: Any) -> dict[str, Any]:
    """Create an early-reject document without traversing production rows."""
    result = {
        key: base[key]
        for key in (
            "schema", "case", "dimensions", "variables",
            "primitive_fields", "source_format",
        )
    }
    result["frames"] = []
    result.update(changes)
    return result


def populate_accept(base_path: Path, output: Path, case: str) -> None:
    for side in ("reference", "candidate"):
        link_or_copy(base_path, output / case / side / "primitive_tab.json")



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

    populate_accept(base_path, args.output_dir, "accept-identity")
    populate_accept(base_path, args.output_dir, "accept-roundtrip")
    link_or_copy(base_path, args.output_dir / "accept-cycle-diagnostic-different" / "reference" / "primitive_tab.json")
    replace_once(
        base_path,
        args.output_dir / "accept-cycle-diagnostic-different" / "candidate" / "primitive_tab.json",
        b'"cycle":0',
        b'"cycle":100',
    )

    # Pointwise primitive tolerance: 5e-11 is within 1e-12 + 1e-10*max.
    link_or_copy(base_path, args.output_dir / "accept-within-tolerance" / "reference" / "primitive_tab.json")
    replace_once(
        base_path,
        args.output_dir / "accept-within-tolerance" / "candidate" / "primitive_tab.json",
        b"1.0",
        b"1.00000000005",
    )

    # The preserved 1e-6-style near miss is far beyond the approved tolerance.
    link_or_copy(base_path, args.output_dir / "reject-value" / "reference" / "primitive_tab.json")
    replace_once(
        base_path,
        args.output_dir / "reject-value" / "candidate" / "primitive_tab.json",
        b"1.0",
        b"1.000001",
    )
    link_or_copy(base_path, args.output_dir / "reject-over-tolerance" / "reference" / "primitive_tab.json")
    replace_once(
        base_path,
        args.output_dir / "reject-over-tolerance" / "candidate" / "primitive_tab.json",
        b"1.0",
        b"1.000000001",
    )

    # Coordinates are a separate exact-equality requirement.
    link_or_copy(base_path, args.output_dir / "reject-coordinate" / "reference" / "primitive_tab.json")
    replace_first_row_x1(
        base_path,
        args.output_dir / "reject-coordinate" / "candidate" / "primitive_tab.json",
    )

    # Physical positivity applies independently to both reference and candidate.
    link_or_copy(base_path, args.output_dir / "reject-non-positive-rho" / "reference" / "primitive_tab.json")
    replace_once(
        base_path,
        args.output_dir / "reject-non-positive-rho" / "candidate" / "primitive_tab.json",
        b"1.0",
        b"0.0",
    )
    link_or_copy(base_path, args.output_dir / "reject-non-positive-press" / "reference" / "primitive_tab.json")
    replace_once(
        base_path,
        args.output_dir / "reject-non-positive-press" / "candidate" / "primitive_tab.json",
        b"0.9",
        b"0.0",
    )

    link_or_copy(base_path, args.output_dir / "reject-negative-cycle" / "reference" / "primitive_tab.json")
    replace_once(
        base_path,
        args.output_dir / "reject-negative-cycle" / "candidate" / "primitive_tab.json",
        b'"cycle":0',
        b'"cycle":-1',
    )

    # Symlinked artifact directories and files are both rejected explicitly.
    link_or_copy(base_path, args.output_dir / "reject-symlink-directory" / "reference" / "primitive_tab.json")
    symlink_relative(
        args.output_dir / "reject-symlink-directory" / "reference",
        args.output_dir / "reject-symlink-directory" / "candidate",
    )
    link_or_copy(base_path, args.output_dir / "reject-symlink-file" / "reference" / "primitive_tab.json")
    (args.output_dir / "reject-symlink-file" / "candidate").mkdir(parents=True, exist_ok=True)
    symlink_relative(
        args.output_dir / "reject-symlink-file" / "reference" / "primitive_tab.json",
        args.output_dir / "reject-symlink-file" / "candidate" / "primitive_tab.json",
    )

    # These malformed cases intentionally fail conformance before row traversal.
    link_or_copy(base_path, args.output_dir / "reject-shape" / "reference" / "primitive_tab.json")
    write(
        args.output_dir / "reject-shape" / "candidate" / "primitive_tab.json",
        short_document(base, dimensions=[1, 1, 1]),
    )

    nonfinite = short_document(base)
    nonfinite["dimensions"] = [1, 1, 1]
    nonfinite["frames"] = [{
        "time": 0.0,
        "cycle": 0,
        "rows": [[0.0, 0.0, 0.0, 1.0, float("nan"), 0.0, 0.0, 0.0]],
    }]
    link_or_copy(base_path, args.output_dir / "reject-nonfinite" / "reference" / "primitive_tab.json")
    write(args.output_dir / "reject-nonfinite" / "candidate" / "primitive_tab.json", nonfinite, allow_nan=True)

    link_or_copy(base_path, args.output_dir / "reject-metadata" / "reference" / "primitive_tab.json")
    write(
        args.output_dir / "reject-metadata" / "candidate" / "primitive_tab.json",
        short_document(base, case="wrong-case"),
    )

    link_or_copy(base_path, args.output_dir / "reject-extra" / "reference" / "primitive_tab.json")
    link_or_copy(base_path, args.output_dir / "reject-extra" / "candidate" / "primitive_tab.json")
    (args.output_dir / "reject-extra" / "candidate" / "unexpected.txt").write_text(
        "unexpected\n", encoding="utf-8"
    )

    malformed = args.output_dir / "reject-malformed" / "candidate" / "primitive_tab.json"
    malformed.parent.mkdir(parents=True, exist_ok=True)
    malformed.write_text('{"schema":', encoding="utf-8")
    link_or_copy(base_path, args.output_dir / "reject-malformed" / "reference" / "primitive_tab.json")
    print(args.output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
