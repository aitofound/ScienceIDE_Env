#!/usr/bin/env python3
"""Build the pinned Athena++ variants and execute every inventory check once.

Runs inside the tests/Dockerfile oracle image (or on a host with the pinned
source at ATHENA_SOURCE_DIR).  The check list, the binary roles and their exact
configure commands come from tests/inventory.json, which is derived from
tests/lib/inventory.py; nothing here enumerates checks by hand.

Contract v4 makes this script the trusted producer of run evidence:

* every build keeps its configure/make log and a build record naming the exact
  commands, return codes and the executable digest they produced;
* the whole solve shares one role/run id/nonce, which every check artifact
  carries, so a copied or relabelled result tree cannot pass as a second
  independent execution;
* after the last check it writes ``receipt.json``, which binds the run identity,
  the container/image identity handed in by solution/solve.sh, the process and
  timing facts, and the digest of **every** regular file in the result root.
"""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
import secrets
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SOURCE_COMMIT = "823614c90b594472747a0ac2a699e4a454f300d2"
BUILD_SCHEMA = "athena-sr-mhd-build/v1"
RECEIPT_SCHEMA = "athena-sr-mhd-receipt/v1"


def now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def fingerprint(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build(source: Path, role: str, configure: list[str], jobs: str, evidence: Path) -> tuple[Path, str]:
    build_root = Path(tempfile.mkdtemp(prefix=f"athena-sr-mhd-build-{role}-"))
    tree = build_root / "athena"
    shutil.copytree(source, tree, symlinks=False)
    command = [sys.executable, "-B", *configure]
    make_command = ["make", "-j" + jobs]
    started = now()
    configured = subprocess.run(command, cwd=tree, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    built = subprocess.run(make_command, cwd=tree, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False) if configured.returncode == 0 else None
    log = (b"$ " + " ".join(command).encode() + b"\n" + configured.stdout
           + (b"\n$ " + " ".join(make_command).encode() + b"\n" + built.stdout if built is not None else b""))
    log_path = evidence / f"{role}.log"
    log_path.write_bytes(log)
    if configured.returncode:
        raise SystemExit(f"configure failed for {role} ({' '.join(configure)}) status={configured.returncode}: {configured.stdout.decode('utf-8', 'replace')[-1000:]}")
    if built.returncode:
        raise SystemExit(f"make failed for {role} status={built.returncode}: {built.stdout.decode('utf-8', 'replace')[-1000:]}")
    binary = tree / "bin" / "athena"
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise SystemExit(f"missing built incumbent binary for {role}")
    digest = fingerprint(binary)
    record = {
        "schema": BUILD_SCHEMA, "role": role, "source_commit": SOURCE_COMMIT, "source_dir": str(source), "build_tree": str(tree),
        "configure_argv": list(configure), "configure_returncode": configured.returncode,
        "make_argv": make_command, "make_returncode": built.returncode,
        "binary_path": str(binary), "binary_sha256": digest, "binary_bytes": binary.stat().st_size,
        "log_sha256": hashlib.sha256(log).hexdigest(), "log_bytes": len(log), "started_at": started, "finished_at": now(),
    }
    (evidence / f"{role}.json").write_text(json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    return binary, digest


def receipt(output: Path, identity: dict, checks: list[str], binaries: dict, started_at: str, started: float, source: Path) -> dict:
    entries = []
    for path in sorted(output.rglob("*")):
        if path.is_symlink():
            raise SystemExit("result root must not contain symlinks: " + str(path))
        if path.is_dir():
            continue
        if not path.is_file():
            raise SystemExit("result root must contain regular files only: " + str(path))
        entries.append({"path": path.relative_to(output).as_posix(), "sha256": fingerprint(path), "bytes": path.stat().st_size})
    digest = hashlib.sha256()
    for entry in entries:
        digest.update(entry["path"].encode("utf-8") + b"\0" + entry["sha256"].encode("ascii") + b"\n")
    document = {
        "schema": RECEIPT_SCHEMA, "source_commit": SOURCE_COMMIT,
        "role": identity["role"], "run_id": identity["run_id"], "run_nonce": identity["nonce"],
        "hostname": identity["hostname"], "pid": identity["pid"], "boot_id": identity["boot_id"],
        "container": identity["container"], "container_id": identity["container_id"], "image": identity["image"], "image_id": identity["image_id"],
        "results_root": str(output), "host_results_root": identity["host_results_root"], "source_dir": str(source),
        "started_at": started_at, "finished_at": now(), "elapsed_seconds": time.monotonic() - started,
        "checks": list(checks), "binaries": binaries,
        "file_count": len(entries), "tree_sha256": digest.hexdigest(), "files": entries,
    }
    return document


def container_id() -> str:
    for candidate in (Path("/proc/self/cpuset"), Path("/proc/self/cgroup")):
        if candidate.is_file():
            text = candidate.read_text(encoding="utf-8", errors="replace")
            for token in text.replace("/", " ").replace(":", " ").split():
                if len(token) == 64 and all(character in "0123456789abcdef" for character in token):
                    return token
    return ""


def main() -> int:
    if len(sys.argv) != 1:
        raise SystemExit("solution/solve.sh accepts no positional arguments")
    started_at, started = now(), time.monotonic()
    leaf = Path(__file__).resolve().parents[1]
    source = Path(os.environ.get("ATHENA_SOURCE_DIR", "/opt/athena"))
    output = Path(os.environ.get("ATHENA_ORACLE_DIR", "/app/results"))
    jobs = os.environ.get("ATHENA_MAKE_JOBS", "2")
    if source.is_symlink() or not source.is_dir():
        raise SystemExit("missing exact pinned source directory: " + str(source))
    if output.is_symlink():
        raise SystemExit("oracle output must not be a symlink")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise SystemExit("oracle output must be empty before the one-shot suite")
    boot = Path("/proc/sys/kernel/random/boot_id")
    identity = {
        "role": os.environ.get("ATHENA_RUN_ROLE", "reference-oracle"),
        "run_id": os.environ.get("ATHENA_RUN_ID") or os.environ.get("ATHENA_DOCKER_RUN_ID") or dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ-") + str(os.getpid()),
        "nonce": os.environ.get("ATHENA_RUN_NONCE") or secrets.token_hex(16),
        # These are run identity fields, not per-check subprocess metadata.  The
        # check drivers are separate children, so they receive this stable
        # producer identity through ATHENA_RUN_* below.
        "hostname": socket.gethostname(), "pid": os.getpid(),
        "boot_id": boot.read_text(encoding="utf-8").strip() if boot.is_file() else "",
        "container": os.environ.get("ATHENA_DOCKER_CONTAINER", ""), "container_id": container_id(),
        "image": os.environ.get("ATHENA_DOCKER_IMAGE", ""), "image_id": os.environ.get("ATHENA_DOCKER_IMAGE_ID", ""),
        "host_results_root": os.environ.get("ATHENA_HOST_RESULTS_ROOT", ""),
    }
    inventory = json.loads((leaf / "tests" / "inventory.json").read_text(encoding="utf-8"))
    if inventory.get("source_commit") != SOURCE_COMMIT:
        raise SystemExit("tests/inventory.json does not pin the expected source commit")
    checks = [check["slug"] for check in inventory["checks"]]
    audit = subprocess.run([sys.executable, "-B", str(leaf / "tests" / "lib" / "build_contracts.py"), "--check"],
                           env={**os.environ, "ATHENA_SOURCE_DIR": str(source)}, cwd=leaf, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False, text=True)
    if audit.returncode:
        raise SystemExit("check contracts drift from tests/lib/inventory.py:\n" + audit.stdout[-2000:])

    evidence = Path(tempfile.mkdtemp(prefix="athena-sr-mhd-build-evidence-"))
    binaries: dict[str, tuple[Path, str]] = {}
    for role, spec in inventory["binaries"].items():
        binaries[role] = build(source, role, spec["configure"], jobs, evidence)
    if len({digest for _, digest in binaries.values()}) != len(binaries):
        raise SystemExit("distinct binary roles produced identical executables")
    environment_binaries = {inventory["binaries"][role]["env"]: str(path) for role, (path, _) in binaries.items()}

    for name in checks:
        check_dir = leaf / "tests" / "checks" / name
        environment = dict(os.environ)
        environment.update(environment_binaries)
        environment.update({
            "ATHENA_OUTPUT_DIR": str(output / name), "ATHENA_SOURCE_DIR": str(source), "ATHENA_BUILD_EVIDENCE_DIR": str(evidence),
            "ATHENA_RUN_ROLE": identity["role"], "ATHENA_RUN_ID": identity["run_id"], "ATHENA_RUN_NONCE": identity["nonce"],
            "ATHENA_DOCKER_CONTAINER": identity["container"], "ATHENA_DOCKER_IMAGE": identity["image"], "ATHENA_DOCKER_IMAGE_ID": identity["image_id"],
            "ATHENA_RUN_HOSTNAME": identity["hostname"], "ATHENA_RUN_PID": str(identity["pid"]), "ATHENA_RUN_BOOT_ID": identity["boot_id"],
        })
        completed = subprocess.run(["bash", str(check_dir / "run.sh")], cwd=leaf, env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, text=True)
        if completed.returncode:
            raise SystemExit(f"check {name} failed status={completed.returncode}: {completed.stderr[-1500:]}")
        print(completed.stdout.strip(), flush=True)
    for name in checks:
        if not (output / name / "observables.json").is_file() or not (output / name / "raw").is_dir():
            raise SystemExit(f"check {name} did not retain an index plus its raw evidence tree")
    document = receipt(output, identity, checks, {role: {"sha256": digest, "configure": inventory["binaries"][role]["configure"]} for role, (_, digest) in binaries.items()},
                       started_at, started, source)
    (output / "receipt.json").write_text(json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    output_hashes = {name: fingerprint(output / name / "observables.json") for name in checks}
    real_solver_runs = {}
    for name in checks:
        record = json.loads((output / name / "observables.json").read_text(encoding="utf-8"))
        real_solver_runs[name] = record["execution"]["real_solver_runs"]
    print(json.dumps({
        "source_commit": SOURCE_COMMIT, "check_count": len(checks), "checks": checks,
        "run": {"role": identity["role"], "run_id": identity["run_id"], "nonce": identity["nonce"], "container": identity["container"], "image_id": identity["image_id"]},
        "binary_sha256": {role: digest for role, (_, digest) in binaries.items()},
        "binary_configure": {role: inventory["binaries"][role]["configure"] for role in binaries},
        "build_evidence_dir": str(evidence),
        "output_sha256": output_hashes, "real_solver_runs": real_solver_runs,
        "receipt": {"file_count": document["file_count"], "tree_sha256": document["tree_sha256"]},
        "policy": "reference oracle execution; verdicts are made only by tests/test.sh",
        "speedup_claim": "none",
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
