#!/usr/bin/env python3
"""Task-local content, output, and process-provenance helpers.

This module is deliberately local to the Phantom gravity leaf.  It does not
replace Harbor's verifier: it gives the leaf one implementation of the
manifest and physical-identity rules used by its oracle and verifier.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
from typing import Iterable

SOURCE_PIN = "e53ea16758d2a261680506852a528f21270dca1c"
SOURCE_TREE = "ae40f54661feb12f0550092fd2188e5738b7b955"
CATALOG_NAME = "tests/checks.json"
MANIFEST_VERSION = 1


def _digest_records(records: Iterable[str]) -> str:
    payload = "".join(records).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _lstat_regular(path: Path, label: str) -> os.stat_result:
    try:
        info = path.lstat()
    except OSError as exc:
        raise ValueError(f"{label} cannot be lstat'ed: {exc}") from exc
    if stat.S_ISLNK(info.st_mode):
        raise ValueError(f"{label} is a symlink")
    if not stat.S_ISREG(info.st_mode):
        raise ValueError(f"{label} is not a regular file")
    return info


def _walk_regular_files(root: Path, label: str) -> list[Path]:
    """Walk without following links and fail on every link/non-regular entry."""
    if root.is_symlink():
        raise ValueError(f"{label} is a symlink")
    try:
        root_info = root.lstat()
    except OSError as exc:
        raise ValueError(f"{label} cannot be inspected: {exc}") from exc
    if not stat.S_ISDIR(root_info.st_mode):
        raise ValueError(f"{label} is not a directory")
    found: list[Path] = []
    for current, dirs, files in os.walk(root, topdown=True, followlinks=False):
        current_path = Path(current)
        dirs.sort()
        files.sort()
        kept_dirs: list[str] = []
        for name in dirs:
            path = current_path / name
            info = path.lstat()
            if stat.S_ISLNK(info.st_mode):
                raise ValueError(f"{label}/{path.relative_to(root)} is a symlink")
            if not stat.S_ISDIR(info.st_mode):
                raise ValueError(f"{label}/{path.relative_to(root)} is not a directory")
            kept_dirs.append(name)
        dirs[:] = kept_dirs
        for name in files:
            path = current_path / name
            _lstat_regular(path, f"{label}/{path.relative_to(root)}")
            found.append(path)
    return found


def find_repo(task_root: Path) -> Path:
    current = task_root.resolve(strict=False)
    while True:
        if (current / "code/phantom").is_dir() and (current / "registry/index.yaml").is_file():
            return current
        if current.parent == current:
            raise ValueError("cannot locate repository root containing code/phantom and registry/index.yaml")
        current = current.parent


def _relative(repo: Path, path: Path) -> str:
    return path.relative_to(repo).as_posix()


def active_file_paths(task_root: Path) -> list[tuple[str, Path]]:
    """Return all acceptance-affecting files, excluding runtime-hidden comment/."""
    repo = find_repo(task_root)
    paths: dict[str, Path] = {}

    def add_tree(root: Path, label: str, exclude_comment: bool = False) -> None:
        for path in _walk_regular_files(root, label):
            relative_to_root = path.relative_to(root)
            if exclude_comment and relative_to_root.parts and (
                relative_to_root.parts[0] == "comment"
                or relative_to_root.parts[:2] == ("solution", "oracle")
            ):
                continue
            rel = _relative(repo, path)
            if rel in paths and paths[rel] != path:
                raise ValueError(f"duplicate active path {rel}")
            paths[rel] = path

    add_tree(task_root, "task leaf", exclude_comment=True)
    add_tree(repo / "code/phantom", "pinned code/phantom")
    registry = repo / "registry/index.yaml"
    _lstat_regular(registry, "registry/index.yaml")
    paths[_relative(repo, registry)] = registry
    return sorted(paths.items(), key=lambda item: item[0])


def active_manifest(task_root: Path) -> dict:
    repo = find_repo(task_root)
    files = []
    records = []
    for rel, path in active_file_paths(task_root):
        info = _lstat_regular(path, rel)
        digest = _sha256(path)
        files.append({"path": rel, "size": info.st_size, "sha256": digest})
        records.append(f"{rel}\0{info.st_size}\0{digest}\n")
    return {
        "version": MANIFEST_VERSION,
        "root": ".",
        "files": files,
        "digest": _digest_records(records),
        "record_format": "relative-posix-path\\0byte-size\\0sha256\\n",
    }


def _output_files(root: Path) -> list[Path]:
    """List output bytes, excluding this manifest receipt to avoid recursion."""
    files = []
    for path in _walk_regular_files(root, "output root"):
        if path.relative_to(root).as_posix() == "oracle-manifest.json":
            continue
        files.append(path)
    return sorted(files, key=lambda path: path.relative_to(root).as_posix())


def output_manifest(root: Path) -> dict:
    root = Path(root)
    entries = []
    records = []
    for path in _output_files(root):
        relative = path.relative_to(root).as_posix()
        info = _lstat_regular(path, relative)
        digest = _sha256(path)
        entries.append({"path": relative, "size": info.st_size, "sha256": digest})
        records.append(f"{relative}\0{info.st_size}\0{digest}\n")
    return {
        "version": MANIFEST_VERSION,
        "files": entries,
        "digest": _digest_records(records),
        "excluded_paths": ["oracle-manifest.json"],
        "record_format": "relative-posix-path\\0byte-size\\0sha256\\n",
    }


def artifact_manifest(root: Path) -> dict:
    """Manifest of scientific result/stdout bytes, excluding dynamic receipts."""
    root = Path(root)
    entries = []
    records = []
    for path in _output_files(root):
        if path.name not in {"result.json", "stdout.txt"}:
            continue
        relative = path.relative_to(root).as_posix()
        info = _lstat_regular(path, relative)
        digest = _sha256(path)
        entries.append({"path": relative, "size": info.st_size, "sha256": digest})
        records.append(f"{relative}\\0{info.st_size}\\0{digest}\\n")
    return {
        "version": MANIFEST_VERSION,
        "files": entries,
        "digest": _digest_records(records),
        "excluded_paths": ["oracle-manifest.json", "*/execution.json"],
        "record_format": "relative-posix-path\\0byte-size\\0sha256\\n",
    }


def physical_manifest(root: Path) -> dict:
    root = Path(root)
    entries = []
    records = []
    for path in _output_files(root):
        relative = path.relative_to(root).as_posix()
        info = _lstat_regular(path, relative)
        entry = {
            "path": relative,
            "st_dev": info.st_dev,
            "st_ino": info.st_ino,
            "mode": stat.S_IMODE(info.st_mode),
            "size": info.st_size,
        }
        entries.append(entry)
        records.append(json.dumps(entry, sort_keys=True, separators=(",", ":")) + "\n")
    return {
        "version": MANIFEST_VERSION,
        "root": ".",
        "files": entries,
        "digest": _digest_records(records),
        "excluded_paths": ["oracle-manifest.json"],
    }


def _git(repo: Path, *args: str) -> str:
    try:
        proc = subprocess.run(
            ["git", *args], cwd=repo, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, check=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        detail = getattr(exc, "stderr", "") or str(exc)
        raise ValueError(f"git {' '.join(args)} failed: {detail.strip()}") from exc
    return proc.stdout.strip()


def _active_worktree_clean(repo: Path, task_root: Path) -> bool:
    task_rel = _relative(repo, task_root)
    raw = _git(repo, "status", "--porcelain=v1", "--untracked-files=all", "-z", "--", task_rel, "registry/index.yaml", "code/phantom")
    if not raw:
        return True
    # -z records contain a two-byte status, a space, and the path.  Rename
    # records are conservatively considered dirty; comment evidence alone is
    # the only intentionally ignored path.
    records = raw.split("\0")
    for record in records:
        if len(record) < 4:
            continue
        path = record[3:]
        if path.startswith(task_rel + "/comment/"):
            continue
        return False
    return True


def identity(task_root: Path) -> dict:
    task_root = Path(task_root).resolve(strict=False)
    repo = find_repo(task_root)
    source_tree = _git(repo, "rev-parse", "HEAD:code/phantom")
    result = {
        "final_head": _git(repo, "rev-parse", "HEAD"),
        "final_tree": _git(repo, "rev-parse", "HEAD^{tree}"),
        "source_pin": SOURCE_PIN,
        "source_tree": source_tree,
        "active_files_manifest": active_manifest(task_root),
        "catalog_sha256": _sha256(task_root / "tests/checks.json"),
        "working_tree_clean": _active_worktree_clean(repo, task_root),
        "repository_root": str(repo),
        "task_root": str(task_root),
    }
    if source_tree != SOURCE_TREE:
        raise ValueError(f"code/phantom tree mismatch: expected {SOURCE_TREE}, got {source_tree}")
    return result


def write_json_exclusive(path: Path, value: dict) -> None:
    path = Path(path)
    if path.is_symlink() or path.exists():
        raise ValueError(f"refusing existing receipt path: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def _command(args: argparse.Namespace) -> int:
    try:
        if args.action == "identity":
            print(json.dumps(identity(Path(args.root)), sort_keys=True))
        elif args.action == "write-active":
            write_json_exclusive(Path(args.output), identity(Path(args.root))["active_files_manifest"])
        elif args.action == "output":
            print(json.dumps(output_manifest(Path(args.root)), sort_keys=True))
        elif args.action == "physical":
            print(json.dumps(physical_manifest(Path(args.root)), sort_keys=True))
        else:
            raise ValueError(f"unknown action {args.action}")
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"provenance.py: {exc}", file=sys.stderr)
        return 2
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("identity", "write-active", "output", "physical"))
    parser.add_argument("--root", required=True)
    parser.add_argument("--output")
    args = parser.parse_args()
    if args.action == "write-active" and not args.output:
        parser.error("write-active requires --output")
    return _command(args)


if __name__ == "__main__":
    raise SystemExit(main())
