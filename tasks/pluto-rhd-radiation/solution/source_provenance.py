#!/usr/bin/env python3
"""Verify and extract the pinned PLUTO archive without a sibling source tree.

The archive is the source of truth for every build.  Extraction is refused into
an existing destination and the resulting regular-file/directory tree is
compared against the archive's path, mode, and content manifest.  The manifest
hash is recorded in solve/run manifests and direct-image build logs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
import sys
import tarfile
from pathlib import PurePosixPath

EXPECTED_SHA256 = "1ba5527b76d49fdd78ae24dbfbdad085ec83393748f1e618516a9d63bd945787"


def sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _name(member: tarfile.TarInfo) -> str:
    name = member.name
    if name.startswith("PLUTO/"):
        name = name[6:]
    elif name == "PLUTO":
        name = ""
    else:
        raise ValueError(f"archive entry outside PLUTO root: {member.name!r}")
    name = name.rstrip("/")
    if not name:
        return ""
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "." in path.parts:
        raise ValueError(f"unsafe archive entry: {member.name!r}")
    return path.as_posix()


def _manifest_lines_from_archive(archive: str) -> tuple[list[str], dict[str, tarfile.TarInfo]]:
    # The pinned tarball contains one historical duplicate regular-file member.
    # Duplicate names are accepted only when type, mode, and bytes are exactly
    # identical; this preserves archive identity while making extraction's
    # last-member behavior explicit and deterministic.
    grouped: dict[str, list[tarfile.TarInfo]] = {}
    with tarfile.open(archive, "r:gz") as stream:
        for member in stream.getmembers():
            name = _name(member)
            if not name:
                continue
            if not (member.isfile() or member.isdir()):
                raise ValueError(f"unsupported archive entry type: {member.name!r}")
            grouped.setdefault(name, []).append(member)
    entries: dict[str, tarfile.TarInfo] = {}
    lines: list[str] = []
    with tarfile.open(archive, "r:gz") as stream:
        for name in sorted(grouped):
            members = grouped[name]
            member = members[0]
            kind = "d" if member.isdir() else "f"
            if member.isfile():
                source = stream.extractfile(member)
                assert source is not None
                digest = hashlib.sha256(source.read()).hexdigest()
            else:
                digest = "-"
            line = f"{kind}\t{name}\t{member.mode & 0o7777:04o}\t{digest}"
            for duplicate in members[1:]:
                if duplicate.isdir() != member.isdir() or (duplicate.mode & 0o7777) != (member.mode & 0o7777):
                    raise ValueError(f"non-identical duplicate archive entry: {name}")
                if duplicate.isfile():
                    duplicate_stream = stream.extractfile(duplicate)
                    assert duplicate_stream is not None
                    if hashlib.sha256(duplicate_stream.read()).hexdigest() != digest:
                        raise ValueError(f"non-identical duplicate archive entry: {name}")
            entries[name] = member
            lines.append(line)
    return lines, entries

def _manifest_lines_from_tree(root: str) -> list[str]:
    if not os.path.isdir(root):
        raise ValueError(f"tree destination is not a directory: {root}")
    entries: dict[str, str] = {}
    for base, dirs, files in os.walk(root, topdown=True, followlinks=False):
        dirs.sort()
        files.sort()
        relbase = os.path.relpath(base, root)
        if relbase == ".":
            relbase = ""
        for directory in dirs:
            path = os.path.join(base, directory)
            rel = os.path.join(relbase, directory).replace(os.sep, "/")
            if os.path.islink(path):
                raise ValueError(f"symlink in extracted source tree: {rel}")
            entries[rel] = f"d\t{rel}\t{stat.S_IMODE(os.stat(path, follow_symlinks=False).st_mode):04o}\t-"
        for filename in files:
            path = os.path.join(base, filename)
            rel = os.path.join(relbase, filename).replace(os.sep, "/")
            if os.path.islink(path):
                raise ValueError(f"symlink in extracted source tree: {rel}")
            entries[rel] = (f"f\t{rel}\t"
                            f"{stat.S_IMODE(os.stat(path, follow_symlinks=False).st_mode):04o}\t"
                            f"{sha256_file(path)}")
    return [entries[name] for name in sorted(entries)]



def _compare_tree(expected_lines: list[str], actual_lines: list[str],
                  archive_entries: dict[str, tarfile.TarInfo]) -> None:
    # Tar archives may omit implicit parent-directory members.  Compare every
    # regular file plus explicitly archived directories; implicit parents are
    # required to exist by those paths but have no independent archive mode.
    explicit = set(archive_entries)
    actual = [line for line in actual_lines
              if line.split("\t", 2)[1] in explicit or line.startswith("f\t")]
    if actual != expected_lines:
        missing = sorted(set(expected_lines) - set(actual))[:5]
        extra = sorted(set(actual) - set(expected_lines))[:5]
        raise ValueError(f"extracted source tree differs from archive (missing={missing}, extra={extra})")

def _digest(lines: list[str]) -> str:
    return hashlib.sha256(("\n".join(lines) + "\n").encode("utf-8")).hexdigest()


def verify_and_extract(archive: str, destination: str, expected: str = EXPECTED_SHA256) -> dict:
    actual = sha256_file(archive)
    if expected and actual != expected:
        raise ValueError(f"archive SHA-256 mismatch: expected {expected}, got {actual}")
    if os.path.exists(destination):
        raise FileExistsError(f"refusing to extract over existing source root: {destination}")
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    os.mkdir(destination)
    expected_lines, entries = _manifest_lines_from_archive(archive)
    with tarfile.open(archive, "r:gz") as stream:
        stream.extractall(destination, filter="data")
    # The archive has one top-level PLUTO directory; move its contents into the
    # requested source root so setup.py/build paths are exactly the archive tree.
    nested = os.path.join(destination, "PLUTO")
    if os.path.isdir(nested):
        for name in os.listdir(nested):
            os.replace(os.path.join(nested, name), os.path.join(destination, name))
        os.rmdir(nested)
    # tar extraction honors the process umask, so restore archive modes before
    # comparing the exact tree manifest (notably historical 0777 directories).
    for name, member in entries.items():
        os.chmod(os.path.join(destination, name), member.mode & 0o7777)
    actual_lines = _manifest_lines_from_tree(destination)
    _compare_tree(expected_lines, actual_lines, entries)
    return {
        "archive_sha256": actual,
        "tree_manifest_sha256": _digest(expected_lines),
        "tree_entries": len(expected_lines),
        "source_root": os.path.abspath(destination),
    }


def verify_existing(archive: str, destination: str, expected: str = EXPECTED_SHA256) -> dict:
    actual = sha256_file(archive)
    if expected and actual != expected:
        raise ValueError(f"archive SHA-256 mismatch: expected {expected}, got {actual}")
    expected_lines, entries = _manifest_lines_from_archive(archive)
    actual_lines = _manifest_lines_from_tree(destination)
    _compare_tree(expected_lines, actual_lines, entries)
    return {
        "archive_sha256": actual,
        "tree_manifest_sha256": _digest(expected_lines),
        "tree_entries": len(expected_lines),
        "source_root": os.path.abspath(destination),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("extract", "verify"))
    parser.add_argument("archive")
    parser.add_argument("destination")
    parser.add_argument("--expected", default=EXPECTED_SHA256)
    args = parser.parse_args()
    try:
        result = (verify_and_extract if args.command == "extract" else verify_existing)(
            args.archive, args.destination, args.expected)
    except (OSError, ValueError, tarfile.TarError) as exc:
        print(f"source provenance failure: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
