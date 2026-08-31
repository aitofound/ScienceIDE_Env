#!/usr/bin/env python3
"""Task-local catalog and byte/provenance helpers for Phantom MHD.

This module is deliberately local to the leaf.  It records facts observed by
``solve.sh`` and recomputes the same facts in the normal verifier; it does not
turn a self-authored marker or hash into process attestation.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import stat
import subprocess
from pathlib import Path
from typing import Any, Iterable

PIN = "e53ea16758d2a261680506852a528f21270dca1c"
SOURCE_TREE = "ae40f54661feb12f0550092fd2188e5738b7b955"
SOURCE_REPOSITORY = "https://github.com/danieljprice/phantom"
CATALOG_SCHEMA = "phantom-mhd-nonideal-check-catalog/v2"
MANIFEST_SCHEMA = "phantom-mhd-nonideal-manifest/v1"
RECEIPT_SCHEMA = "phantom-mhd-nonideal-receipt/v1"
_SEGMENT = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")


class ProvenanceError(ValueError):
    """Raised when an active binding or physical artifact is not trustworthy."""


def _reject_constant(value: str) -> None:
    raise ProvenanceError(f"non-finite JSON constant {value!r}")


def _no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ProvenanceError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def load_json(path: Path) -> Any:
    if path.is_symlink() or not path.is_file():
        raise ProvenanceError(f"not a regular JSON file: {path}")
    try:
        with path.open("r", encoding="utf-8") as handle:
            return json.load(
                handle,
                object_pairs_hook=_no_duplicates,
                parse_constant=_reject_constant,
            )
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ProvenanceError(f"cannot load JSON {path}: {exc}") from exc


def _require_string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise ProvenanceError(f"{field} must be a nonempty string")
    return value


def _find_repo_root(task: Path) -> Path:
    for parent in (task, *task.parents):
        if (parent / "code" / "phantom").is_dir() and (parent / "scripts" / "stage-task-source.py").is_file():
            return parent
    raise ProvenanceError(f"cannot locate repository root for task: {task}")


def _check_anchor(root: Path, anchor: str) -> None:
    if not isinstance(anchor, str) or not anchor:
        raise ProvenanceError("source anchor must be a nonempty string")
    source_path = anchor.split(":", 1)[0]
    path = root / "code" / "phantom" / source_path
    if path.is_symlink() or not path.is_file():
        raise ProvenanceError(f"official source anchor is absent: {anchor}")
    if ":" in anchor:
        line_spec = anchor.split(":", 1)[1]
        match = re.fullmatch(r"(\d+)(?:-(\d+))?", line_spec)
        if not match:
            raise ProvenanceError(f"source anchor has invalid line range: {anchor}")
        first = int(match.group(1))
        last = int(match.group(2) or match.group(1))
        if first < 1 or last < first:
            raise ProvenanceError(f"source anchor has invalid line range: {anchor}")
        with path.open("rb") as handle:
            lines = sum(1 for _ in handle)
        if last > lines:
            raise ProvenanceError(f"source anchor exceeds file: {anchor}")


def _validate_official(entry: dict[str, Any], root: Path) -> None:
    official = entry.get("official_test")
    if not isinstance(official, dict):
        raise ProvenanceError(f"{entry.get('id')}: official_test must be an object")
    if official.get("kind") not in {
        "upstream_regression_script",
        "upstream_registered_procedure",
    }:
        raise ProvenanceError(f"{entry.get('id')}: unsupported official_test kind")
    if official.get("source_commit") != PIN or official.get("source_tree") != SOURCE_TREE:
        raise ProvenanceError(f"{entry.get('id')}: source pin/tree mismatch")
    if official.get("source_repository") != SOURCE_REPOSITORY:
        raise ProvenanceError(f"{entry.get('id')}: source repository mismatch")
    registration = official.get("registration")
    if not isinstance(registration, dict):
        raise ProvenanceError(f"{entry.get('id')}: registration must be an object")
    registration_path = _require_string(registration.get("path"), "registration.path")
    registration_file = root / "code" / "phantom" / registration_path
    if registration_file.is_symlink() or not registration_file.is_file():
        raise ProvenanceError(f"{entry.get('id')}: missing registration path {registration_path}")
    _require_string(registration.get("lines"), "registration.lines")
    _require_string(registration.get("target"), "registration.target")
    entrypoint = official.get("entrypoint")
    if not isinstance(entrypoint, dict):
        raise ProvenanceError(f"{entry.get('id')}: entrypoint must be an object")
    if entrypoint.get("program") != "phantomtest":
        raise ProvenanceError(f"{entry.get('id')}: entrypoint must be phantomtest")
    argv = entrypoint.get("argv")
    if not isinstance(argv, list) or len(argv) != 1 or not isinstance(argv[0], str) or not argv[0]:
        raise ProvenanceError(f"{entry.get('id')}: entrypoint argv must contain one selector")
    if official.get("selector") != argv[0]:
        raise ProvenanceError(f"{entry.get('id')}: selector/argv mismatch")
    if official.get("input_mode") != "upstream_untouched":
        raise ProvenanceError(f"{entry.get('id')}: input mode is not upstream_untouched")
    input_paths = official.get("input_paths")
    input_hashes = official.get("input_sha256")
    if not isinstance(input_paths, list) or not all(isinstance(path, str) and path for path in input_paths):
        raise ProvenanceError(f"{entry.get('id')}: input_paths must be a string array")
    if not isinstance(input_hashes, dict) or set(input_hashes) != set(input_paths):
        raise ProvenanceError(f"{entry.get('id')}: input_sha256 must cover input_paths exactly")
    for path in input_paths:
        candidate = root / "code" / "phantom" / path
        if candidate.is_symlink() or not candidate.is_file():
            raise ProvenanceError(f"{entry.get('id')}: missing official input {path}")
        digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
        if input_hashes[path] != digest:
            raise ProvenanceError(f"{entry.get('id')}: official input hash mismatch for {path}")
    source_paths = official.get("source_paths")
    if not isinstance(source_paths, list) or not source_paths:
        raise ProvenanceError(f"{entry.get('id')}: source_paths must be nonempty")
    for anchor in source_paths:
        _check_anchor(root, anchor)
    tolerance = official.get("tolerance_source")
    if not isinstance(tolerance, dict) or tolerance.get("kind") not in {
        "upstream_test",
        "human_approved",
        "none",
    }:
        raise ProvenanceError(f"{entry.get('id')}: invalid tolerance_source")
    if tolerance.get("kind") == "upstream_test":
        _check_anchor(root, tolerance.get("anchor"))
        _require_string(tolerance.get("policy"), "tolerance_source.policy")
    artifact = official.get("expected_artifact_contract")
    if not isinstance(artifact, dict):
        raise ProvenanceError(f"{entry.get('id')}: missing expected artifact contract")
    if artifact.get("required_files") != ["canonical.log", "result.json"]:
        raise ProvenanceError(f"{entry.get('id')}: unexpected required artifact set")
    if artifact.get("byte_comparison") != "exact":
        raise ProvenanceError(f"{entry.get('id')}: canonical byte comparison must remain exact")
    if official.get("candidate_execution") != "required":
        raise ProvenanceError(f"{entry.get('id')}: candidate execution is not required")
    if official.get("provenance") != "runtime-attestation-and-output-manifest":
        raise ProvenanceError(f"{entry.get('id')}: provenance policy mismatch")


def load_catalog(path: Path, root: Path | None = None) -> list[dict[str, Any]]:
    value = load_json(path)
    task_root = root
    repo_root = _find_repo_root(root) if root is not None else None
    if not isinstance(value, dict) or value.get("schema") != CATALOG_SCHEMA:
        raise ProvenanceError("active catalog schema mismatch")
    if value.get("source_commit") != PIN or value.get("source_tree") != SOURCE_TREE:
        raise ProvenanceError("active catalog source identity mismatch")
    if value.get("source_repository") != SOURCE_REPOSITORY:
        raise ProvenanceError("active catalog source repository mismatch")
    entries = value.get("checks")
    if not isinstance(entries, list) or not entries:
        raise ProvenanceError("active catalog checks must be nonempty")
    ids: list[str] = []
    acceleration_found = False
    for entry in entries:
        if not isinstance(entry, dict):
            raise ProvenanceError("catalog entry must be an object")
        check_id = _require_string(entry.get("id"), "catalog id")
        if check_id in ids:
            raise ProvenanceError(f"duplicate catalog id: {check_id}")
        ids.append(check_id)
        if entry.get("folder") != f"checks/{check_id}":
            raise ProvenanceError(f"{check_id}: folder does not match id")
        if entry.get("reward_weight") != 1:
            raise ProvenanceError(f"{check_id}: reward_weight must equal one")
        if task_root is not None:
            check_dir = task_root / "tests" / entry["folder"]
            if check_dir.is_symlink() or not check_dir.is_dir():
                raise ProvenanceError(f"{check_id}: direct check directory is missing or aliased")
            direct = load_json(check_dir / "check.json")
            if set(direct) != {"labels"} or not isinstance(direct["labels"], list):
                raise ProvenanceError(f"{check_id}: direct check.json must be labels-only")
            labels = direct["labels"]
            if (
                not labels
                or any(not isinstance(label, str) or _SEGMENT.fullmatch(label) is None for label in labels)
                or len(labels) != len(set(labels))
            ):
                raise ProvenanceError(f"{check_id}: labels must be unique lower-kebab-case strings")
            acceleration_found = acceleration_found or "acceleration" in labels
            _validate_official(entry, repo_root)
            scenario = load_json(check_dir / "scenario.json")
            if scenario.get("check") != check_id or scenario.get("selector") != entry["official_test"]["selector"]:
                raise ProvenanceError(f"{check_id}: scenario/catalog selector mismatch")
            if scenario.get("source_pin") != PIN or scenario.get("schema") != "phantom-official-test-scenario/v1":
                raise ProvenanceError(f"{check_id}: scenario source/schema mismatch")
            scenario_anchors = scenario.get("official_source_anchors")
            if not isinstance(scenario_anchors, list) or not set(scenario_anchors).issubset(
                set(entry["official_test"]["source_paths"])
            ):
                raise ProvenanceError(f"{check_id}: official source anchors are not fully bound")
    if task_root is not None:
        if not acceleration_found:
            raise ProvenanceError("catalog must expose the exact acceleration label on a direct check")
        checks_dir = task_root / "tests" / "checks"
        actual = sorted(
            child.name for child in checks_dir.iterdir()
            if child.is_dir() and not child.is_symlink()
        )
        if actual != sorted(ids):
            raise ProvenanceError(f"catalog/direct check mismatch: catalog={ids}, directories={actual}")
    return entries


def catalog_ids(entries: Iterable[dict[str, Any]]) -> list[str]:
    return [entry["id"] for entry in entries]


def _git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise ProvenanceError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def _tracked_active_paths(repo: Path, leaf: Path) -> list[str]:
    repo = repo.resolve()
    leaf = leaf.resolve()
    leaf_rel = leaf.relative_to(repo).as_posix()
    raw = subprocess.run(
        ["git", "-C", str(repo), "ls-files", "-z", "--", leaf_rel, "code/phantom", "registry/index.yaml", "registry.json"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if raw.returncode != 0:
        raise ProvenanceError(f"git ls-files failed: {raw.stderr.decode(errors='replace').strip()}")
    paths = []
    for item in raw.stdout.split(b"\0"):
        if not item:
            continue
        path = item.decode("utf-8")
        if path in {"registry/index.yaml", "registry.json"} or path.startswith("code/phantom/"):
            paths.append(path)
        elif path.startswith(leaf_rel + "/") and "/comment/" not in path:
            paths.append(path)
    if not paths:
        raise ProvenanceError("active content manifest has no files")
    return sorted(set(paths))


def _record_file(repo: Path, relative: str) -> dict[str, Any]:
    path = repo / relative
    metadata = path.lstat()
    if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISREG(metadata.st_mode):
        raise ProvenanceError(f"active file is not a regular non-symlink file: {relative}")
    content = path.read_bytes()
    return {
        "path": relative,
        "size": len(content),
        "sha256": hashlib.sha256(content).hexdigest(),
    }


def active_file_manifest(repo: Path, leaf: Path) -> dict[str, Any]:
    repo = repo.resolve()
    leaf = leaf.resolve()
    entries = [_record_file(repo, relative) for relative in _tracked_active_paths(repo, leaf)]
    stream = b"".join(
        f"{entry['path']}\0{entry['size']}\0{entry['sha256']}\n".encode("utf-8")
        for entry in entries
    )
    digest = hashlib.sha256(stream).hexdigest()
    return {
        "schema": "sha256-path-size-digest/v1",
        "entries": entries,
        "digest": digest,
        "record_format": "relative-posix-path\\0byte-size\\0sha256\\n",
    }


def repository_identity(repo: Path, leaf: Path) -> dict[str, Any]:
    repo = repo.resolve()
    leaf = leaf.resolve()
    leaf_rel = leaf.relative_to(repo).as_posix()
    scoped = [leaf_rel, "code/phantom", "registry/index.yaml", "registry.json"]
    status = subprocess.run(
        ["git", "-C", str(repo), "status", "--porcelain=v1", "--untracked-files=no", "--", *scoped],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if status.returncode != 0:
        raise ProvenanceError(f"git status failed: {status.stderr.strip()}")
    return {
        "final_head": _git(repo, "rev-parse", "HEAD"),
        "final_tree": _git(repo, "rev-parse", "HEAD^{tree}"),
        "source_tree": _git(repo, "rev-parse", "HEAD:code/phantom"),
        "worktree_clean": not bool(status.stdout),
        "scoped_status": status.stdout.splitlines(),
    }


def _relative(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix() if path != root else "."


def _walk_physical(root: Path, excluded: set[str]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    root_meta = root.lstat()
    if stat.S_ISLNK(root_meta.st_mode) or not stat.S_ISDIR(root_meta.st_mode):
        raise ProvenanceError(f"output root is not a real directory: {root}")
    physical: list[dict[str, Any]] = []
    files: list[dict[str, Any]] = []
    stack = [root]
    while stack:
        current = stack.pop()
        meta = current.lstat()
        relative = _relative(root, current)
        if stat.S_ISLNK(meta.st_mode):
            raise ProvenanceError(f"output contains symlink: {current}")
        if not stat.S_ISDIR(meta.st_mode) and not stat.S_ISREG(meta.st_mode):
            raise ProvenanceError(f"output contains non-regular artifact: {current}")
        if relative not in excluded:
            physical.append({
                "path": relative,
                "st_dev": meta.st_dev,
                "st_ino": meta.st_ino,
                "mode": stat.S_IMODE(meta.st_mode),
                "size": meta.st_size,
                "kind": "directory" if stat.S_ISDIR(meta.st_mode) else "file",
            })
        if stat.S_ISREG(meta.st_mode):
            if relative in excluded:
                continue
            content = current.read_bytes()
            files.append({
                "path": relative,
                "size": len(content),
                "sha256": hashlib.sha256(content).hexdigest(),
            })
            continue
        children = sorted(os.scandir(current), key=lambda item: item.name, reverse=True)
        for child in children:
            child_path = current / child.name
            child_meta = child_path.lstat()
            if stat.S_ISLNK(child_meta.st_mode):
                raise ProvenanceError(f"output contains symlink: {child_path}")
            stack.append(child_path)
    physical.sort(key=lambda item: item["path"])
    files.sort(key=lambda item: item["path"])
    return physical, files


def output_manifest(root: Path, excluded: set[str] | None = None) -> dict[str, Any]:
    excluded = excluded or set()
    physical, files = _walk_physical(root, excluded)
    stream = b"".join(
        f"{entry['path']}\0{entry['size']}\0{entry['sha256']}\n".encode("utf-8")
        for entry in files
    )
    return {
        "schema": "sha256-path-size-digest/v1",
        "entries": files,
        "digest": hashlib.sha256(stream).hexdigest(),
        "record_format": "relative-posix-path\\0byte-size\\0sha256\\n",
        "excluded": sorted(excluded),
        "physical_identity": physical,
    }


def root_state(root: Path) -> dict[str, Any]:
    existed = root.exists() or root.is_symlink()
    if root.is_symlink():
        raise ProvenanceError(f"output root may not be a symlink: {root}")
    if existed and not root.is_dir():
        raise ProvenanceError(f"output root is not a directory: {root}")
    empty = existed and not any(root.iterdir())
    return {
        "existed_before": existed,
        "empty_before": empty if existed else True,
        "fresh_empty": not existed or empty,
    }


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def json_dump(path: Path, value: Any) -> None:
    with path.open("x", encoding="utf-8") as handle:
        json.dump(value, handle, indent=2, sort_keys=True, allow_nan=False)
        handle.write("\n")
