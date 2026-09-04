#!/usr/bin/env python3
"""Verify or regenerate the vendored sab pipeline export inside this skill.

GENERATED FILE - do not edit by hand. The canonical source is
https://github.com/aitofound/sciaccelbench-pipeline.

    python3 scripts/vendor_sync.py verify
        No-network check: every file listed in vendor-manifest.json exists
        with the recorded SHA-256 and executable bit, and the owned export
        boundary contains no extra files. Exit 1 on any mismatch.

    python3 scripts/vendor_sync.py sync --canonical <checkout> [--revision <rev>]
        Regenerate the export deterministically from a local checkout of the
        canonical repository (runs its tools/export_scienceaccelbench.py),
        then verify. Overwrites owned files only; deletes nothing - stale
        files are reported and left for a human to remove.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

SKILL = Path(__file__).resolve().parents[1]
MANIFEST = SKILL / "vendor-manifest.json"
IGNORED_NAMES = {"__pycache__", ".DS_Store"}
IGNORED_SUFFIXES = {".pyc"}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            h.update(block)
    return h.hexdigest()


def load_manifest() -> dict:
    if not MANIFEST.is_file():
        raise SystemExit(f"vendor_sync: manifest not found: {MANIFEST}")
    return json.loads(MANIFEST.read_text(encoding="utf-8"))


def boundary_files(manifest: dict) -> set[str]:
    """Actual files below the owned export boundary, skill-relative."""
    found: set[str] = set()
    for entry in manifest["export_boundary"]:
        target = SKILL / entry
        if entry.endswith("/"):
            if not target.is_dir():
                continue
            for current, dirnames, filenames in os.walk(target):
                dirnames[:] = [d for d in dirnames if d not in IGNORED_NAMES]
                for name in filenames:
                    if name in IGNORED_NAMES or Path(name).suffix in IGNORED_SUFFIXES:
                        continue
                    found.add((Path(current) / name).relative_to(SKILL).as_posix())
        elif target.is_file():
            found.add(entry)
    return found


def verify() -> int:
    manifest = load_manifest()
    problems: list[str] = []
    listed = manifest["files"]
    for name, meta in sorted(listed.items()):
        path = SKILL / name
        if not path.is_file():
            problems.append(f"missing: {name}")
            continue
        digest = sha256(path)
        if digest != meta["sha256"]:
            problems.append(f"mutated: {name} (sha256 {digest[:12]}… != recorded {meta['sha256'][:12]}…)")
        executable = bool(path.stat().st_mode & 0o111)
        if executable != (meta.get("mode") == "755"):
            problems.append(f"mode: {name} executable bit is {executable}, manifest records {meta.get('mode')}")
    manifest_rel = MANIFEST.relative_to(SKILL).as_posix()
    extras = boundary_files(manifest) - set(listed) - {manifest_rel}
    for name in sorted(extras):
        problems.append(f"extra file in the owned export boundary: {name}")
    if problems:
        print(f"vendor_sync: FAIL ({len(problems)} problem(s)) against {manifest_rel}")
        for p in problems:
            print(f"  - {p}")
        print("Regenerate with: python3 scripts/vendor_sync.py sync --canonical <local sciaccelbench-pipeline checkout>")
        return 1
    print(f"vendor_sync: OK — {len(listed)} generated file(s) match {manifest_rel} "
          f"(upstream {manifest['upstream_repo']} @ {manifest['upstream_revision']}, package {manifest['package_version']})")
    return 0


def sync(canonical: Path, revision: str | None) -> int:
    exporter = canonical / "tools" / "export_scienceaccelbench.py"
    if not exporter.is_file():
        raise SystemExit(f"vendor_sync: not a canonical checkout (no tools/export_scienceaccelbench.py): {canonical}")
    before = load_manifest()["files"] if MANIFEST.is_file() else {}
    cmd = [sys.executable, str(exporter), "--dest", str(SKILL.parents[1])]
    if revision:
        cmd += ["--revision", revision]
    print("$", " ".join(cmd))
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        return rc
    stale = sorted(set(before) - set(load_manifest()["files"]))
    for name in stale:
        if (SKILL / name).exists():
            print(f"vendor_sync: stale generated file no longer in the manifest (left in place, remove by hand): {name}")
    return verify()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="cmd")
    sub.add_parser("verify", help="check the vendored files against vendor-manifest.json (no network)")
    p = sub.add_parser("sync", help="regenerate the export from a local canonical checkout, then verify")
    p.add_argument("--canonical", required=True, help="path to a local checkout of the canonical pipeline repository")
    p.add_argument("--revision", help="revision identifier to record in the manifest")
    args = parser.parse_args(argv)
    if args.cmd in (None, "verify"):
        return verify()
    return sync(Path(args.canonical).expanduser().resolve(), args.revision)


if __name__ == "__main__":
    raise SystemExit(main())
