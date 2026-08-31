"""Task-local active/output manifest primitives used by future receipts."""
import hashlib
import json
import os
import sys
from pathlib import Path

EXCLUDED_OUTPUT_NAMES = {"oracle-manifest.json", "official-test.json", "buildbot.log", "testgr.log", "build.log", "solve-manifest.json"}

def _hash(path):
    h = hashlib.sha256()
    size = 0
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            size += len(block)
            h.update(block)
    return size, h.hexdigest()

def _entries(paths, base):
    entries = []
    for path in sorted(paths):
        if path.is_symlink():
            raise ValueError("symlink in manifest: " + str(path))
        if not path.is_file():
            raise ValueError("non-file manifest path: " + str(path))
        size, sha = _hash(path)
        entries.append({"path": path.relative_to(base).as_posix(), "size": size, "sha256": sha})
    entries.sort(key=lambda x: x["path"])
    stream = "".join(f"{x['path']}\t{x['size']}\t{x['sha256']}\n" for x in entries)
    return {"entries": entries, "sha256": hashlib.sha256(stream.encode()).hexdigest()}

def _repo_root(task):
    for parent in [task] + list(task.parents):
        if (parent / "registry" / "index.yaml").is_file():
            return parent
    return None

def active_manifest(task_root):
    """Hash authoritative leaf bytes and the generated registry when present."""
    task = Path(task_root).resolve()
    paths = []
    for name in ("task.toml", "instruction.md"):
        paths.append(task / name)
    for name in ("environment", "solution", "target", "tests"):
        base = task / name
        if not base.is_dir():
            raise ValueError("active directory missing: " + name)
        for dirpath, dirnames, filenames in os.walk(base, followlinks=False):
            dirnames[:] = sorted(x for x in dirnames if x not in {"__pycache__", "non-reward"})
            for filename in sorted(filenames):
                path = Path(dirpath) / filename
                if "non-reward" in path.parts or path.is_symlink():
                    if path.is_symlink():
                        raise ValueError("symlink in active tree: " + str(path))
                    continue
                paths.append(path)
    result = _entries(paths, task)
    registry = _repo_root(task)
    if registry is not None:
        rp = registry / "registry" / "index.yaml"
        size, sha = _hash(rp)
        result["registry_projection"] = {"path": "@registry/index.yaml", "size": size, "sha256": sha}
        lines = [f"{x['path']}\t{x['size']}\t{x['sha256']}\n" for x in result["entries"]]
        lines.append(f"@registry/index.yaml\t{size}\t{sha}\n")
        result["sha256"] = hashlib.sha256("".join(sorted(lines)).encode()).hexdigest()
    return result

def output_manifest(output_root):
    """Hash canonical result bytes; raw logs/receipts are separately attested."""
    root = Path(output_root).resolve()
    paths = []
    for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
        for dirname in list(dirnames):
            path = Path(dirpath) / dirname
            if path.is_symlink():
                raise ValueError("symlink output directory: " + str(path))
        for filename in sorted(filenames):
            path = Path(dirpath) / filename
            rel = path.relative_to(root).as_posix()
            if path.is_symlink():
                raise ValueError("symlink output file: " + rel)
            size, sha = _hash(path)
            entry = {"path": rel, "size": size, "sha256": sha}
            if (filename in EXCLUDED_OUTPUT_NAMES or filename.startswith("docker-build-") or filename.startswith("docker-run-") or "/source-logs/" in "/" + rel):
                continue
            paths.append(path)
    return _entries(paths, root)

def main():
    if len(sys.argv) != 3 or sys.argv[1] != "digest":
        raise SystemExit("usage: receipt.py digest TASK_ROOT")
    print(active_manifest(sys.argv[2])["sha256"])

if __name__ == "__main__":
    main()
