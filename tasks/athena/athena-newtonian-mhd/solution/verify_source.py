#!/usr/bin/env python3
"""Verify the vendored Athena++ tree against the leaf source manifest."""
from __future__ import annotations
import hashlib
import json
import stat
import sys
from pathlib import Path


def main() -> int:
    source, manifest_path = Path(sys.argv[1]), Path(sys.argv[2])
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("commit") != "823614c90b594472747a0ac2a699e4a454f300d2":
        raise SystemExit("source manifest commit is not the required exact pin")
    entries = manifest.get("files")
    if not isinstance(entries, list) or manifest.get("file_count") != len(entries):
        raise SystemExit("source manifest file list is malformed")
    seen = set()
    for entry in entries:
        rel = entry.get("path")
        if not isinstance(rel, str) or rel in seen or Path(rel).is_absolute() or ".." in Path(rel).parts:
            raise SystemExit(f"invalid manifest path: {rel!r}")
        seen.add(rel)
        path = source / rel
        if not path.is_file() or path.is_symlink():
            raise SystemExit(f"missing or symlinked vendored source file: {rel}")
        data = path.read_bytes()
        if len(data) != entry.get("bytes") or hashlib.sha256(data).hexdigest() != entry.get("sha256"):
            raise SystemExit(f"source hash mismatch: {rel}")
    actual = []
    for path in source.rglob("*"):
        if path.is_file():
            if path.is_symlink():
                raise SystemExit(f"symlinked vendored source file: {path}")
            actual.append(path.relative_to(source).as_posix())
    if set(actual) != seen:
        missing = sorted(seen - set(actual))
        extra = sorted(set(actual) - seen)
        raise SystemExit(f"source closure mismatch; missing={missing[:3]} extra={extra[:3]}")
    print(f"source_commit={manifest['commit']} files={len(entries)} verified=1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
