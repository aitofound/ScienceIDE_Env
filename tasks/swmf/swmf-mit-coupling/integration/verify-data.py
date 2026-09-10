#!/usr/bin/env python3
"""Verify the approved SWMF_data payload at the source tree's original lookup root.

This is deliberately a small, read-only verifier.  It checks the twelve exact
files by byte count, SHA-256, and Git blob SHA-1; with --source-root it also
checks the pinned Config.pl layout and its $DIR/SWMF_data lookup contract.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

DATA_REPOSITORY = "SWMFsoftware/SWMF_data"
DATA_REVISION = "5c3be59b33d4379c7f4821c1450684e4b25c77e8"
DATA_TREE_REVISION = "27ade17063198ef9c03d8c1b840f015cf01f8f95"
SOURCE_REVISION = "127a73cb13951351d60e7936583f69f39bd0272e"
EXPECTED_COUNT = 12


def digest(path: Path) -> tuple[int, str, str]:
    sha256 = hashlib.sha256()
    payload = bytearray()
    size = 0
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            size += len(chunk)
            sha256.update(chunk)
            payload.extend(chunk)
    git_blob = hashlib.sha1(b"blob " + str(size).encode("ascii") + b"\0" + payload).hexdigest()
    return size, sha256.hexdigest(), git_blob


def fail(message: str) -> dict[str, Any]:
    return {"status": "failed", "error": message}


def verify(manifest_path: Path, data_root: Path, source_root: Path | None) -> dict[str, Any]:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return fail(f"cannot read manifest: {exc}")
    if manifest.get("data_repository") != DATA_REPOSITORY:
        return fail("manifest data repository is not SWMFsoftware/SWMF_data")
    if manifest.get("data_revision") != DATA_REVISION:
        return fail(f"manifest data revision is not {DATA_REVISION}")
    if manifest.get("data_tree_revision") != DATA_TREE_REVISION:
        return fail(f"manifest data tree revision is not {DATA_TREE_REVISION}")
    if manifest.get("source_revision") != SOURCE_REVISION:
        return fail(f"manifest source revision is not {SOURCE_REVISION}")
    records = manifest.get("files")
    if not isinstance(records, list) or len(records) != EXPECTED_COUNT:
        return fail(f"manifest must contain exactly {EXPECTED_COUNT} files")

    config_contract: dict[str, Any] = {"checked": False}
    if source_root is not None:
        root_config = source_root / "Config.pl"
        shared_config = source_root / "share" / "Scripts" / "Config.pl"
        if not root_config.is_file():
            return fail(f"source-root is missing Config.pl: {root_config}")
        if not shared_config.is_file():
            return fail(f"source-root is missing share/Scripts/Config.pl: {shared_config}")
        try:
            config_text = shared_config.read_text(encoding="utf-8", errors="strict")
        except OSError as exc:
            return fail(f"cannot read shared Config.pl: {exc}")
        if '"$DIR/SWMF_data"' not in config_text:
            return fail("shared Config.pl does not contain the pinned $DIR/SWMF_data lookup")
        config_contract = {
            "checked": True,
            "root_entrypoint": "Config.pl",
            "shared_script": "share/Scripts/Config.pl",
            "lookup": "$DIR/SWMF_data",
            "resolved_lookup": str((source_root / "SWMF_data").resolve()),
        }

    rows: list[dict[str, Any]] = []
    for record in records:
        rel = record.get("path")
        if not isinstance(rel, str):
            return fail("manifest record has a non-string path")
        relative = Path(rel)
        if relative.is_absolute() or ".." in relative.parts:
            return fail(f"unsafe manifest path: {rel}")
        path = data_root / relative
        if not path.is_file():
            return fail(f"missing data file at original path: {relative}")
        try:
            size, sha256, git_blob = digest(path)
        except OSError as exc:
            return fail(f"cannot hash {relative}: {exc}")
        expected_size = record.get("bytes")
        expected_sha256 = record.get("sha256")
        expected_blob = record.get("git_blob_sha")
        row = {
            "path": rel,
            "bytes": size,
            "sha256": sha256,
            "git_blob_sha": git_blob,
            "size_match": size == expected_size,
            "sha256_match": sha256 == expected_sha256,
            "git_blob_match": git_blob == expected_blob,
        }
        rows.append(row)
        if not all((row["size_match"], row["sha256_match"], row["git_blob_match"])):
            return {
                "status": "failed",
                "error": f"hash or size mismatch: {relative}",
                "data_root": str(data_root.resolve()),
                "config_contract": config_contract,
                "files_checked": len(rows),
                "files": rows,
            }

    return {
        "status": "passed",
        "data_repository": DATA_REPOSITORY,
        "data_revision": DATA_REVISION,
        "data_tree_revision": DATA_TREE_REVISION,
        "source_revision": SOURCE_REVISION,
        "data_root": str(data_root.resolve()),
        "config_contract": config_contract,
        "files_checked": len(rows),
        "total_bytes": sum(row["bytes"] for row in rows),
        "files": rows,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).with_name("data-manifest.json"))
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--source-root", type=Path, help="isolated source root containing Config.pl and SWMF_data/")
    group.add_argument("--data-root", type=Path, help="directory containing the SWMF_data paths")
    parser.add_argument("--json-out", type=Path, help="also write the stable JSON result to this file")
    args = parser.parse_args(argv)

    source_root = args.source_root.resolve() if args.source_root else None
    data_root = (source_root / "SWMF_data" if source_root else (args.data_root or Path(__file__).with_name("SWMF_data"))).resolve()
    result = verify(args.manifest.resolve(), data_root, source_root)
    text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    print(text, end="")
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text, encoding="utf-8")
    return 0 if result.get("status") == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
