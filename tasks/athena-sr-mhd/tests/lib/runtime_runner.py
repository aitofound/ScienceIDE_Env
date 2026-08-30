#!/usr/bin/env python3
"""Execute every case of one SR-MHD check contract and retain its raw evidence.

The runner performs no inference: the deck, the override list, the binary role,
the mesh, the output blocks, the expected frame schedule and the operational
cap are read verbatim from ``config/contract.json`` (itself derived from
``tests/lib/inventory.py``).  The pinned Athena++ executables are supplied by
the trusted oracle (or a candidate) through the environment variables named in
the contract.

Contract v4 changes what a result is.  Each case now retains its **raw bytes**
under ``raw/``: the exact native TAB/VTK/error outputs, stdout and stderr, the
deck actually read, and a controlled execution record; each check additionally
retains the configure/make evidence of every build role it used and the
complete identity of the pinned source tree.  ``observables.json`` is only an
index over those bytes: every value in it is recomputed by the verifier from
the retained raw files with the shared helpers in ``contract_tools``.  No
scientific verdict is made here; that is the validator's job.
"""
from __future__ import annotations

import argparse
import datetime as dt
import math
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import contract_tools as ct  # noqa: E402

SCHEMA = "athena-sr-mhd-contract-run/v4"
CONTRACT_SCHEMA = "athena-sr-mhd-contract/v4"
EXECUTION_SCHEMA = "athena-sr-mhd-execution/v4"
RAW_SCHEMA = "athena-sr-mhd-raw/v1"
BUILD_SCHEMA = "athena-sr-mhd-build/v1"
NORMAL_TERMINATION = "time limit"  # src/main.cpp:616-621 with the pinned decks' nlim=-1


def die(message: str) -> None:
    raise SystemExit("runtime_runner: " + message)


def now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def load_contract(path: Path) -> dict[str, Any]:
    try:
        contract = ct.strict_json_load(path)
    except Exception as exc:
        die(f"cannot load contract {path}: {exc}")
    if not isinstance(contract, dict) or contract.get("schema") != CONTRACT_SCHEMA or contract.get("source_commit") != ct.SOURCE_COMMIT:
        die("contract does not carry the v4 schema and the pinned source commit")
    return contract


def run_identity() -> dict[str, Any]:
    """Read one stable producer identity supplied by the trusted solve driver.

    The check drivers are separate child processes, so their own PIDs are not
    run identity.  The oracle supplies its stable hostname/PID/boot tuple to
    every child; the root receipt and every check then bind to the same run.
    """
    role = os.environ.get("ATHENA_RUN_ROLE", "").strip()
    run_id = os.environ.get("ATHENA_RUN_ID", "").strip()
    nonce = os.environ.get("ATHENA_RUN_NONCE", "").strip()
    hostname = os.environ.get("ATHENA_RUN_HOSTNAME", "").strip()
    pid_text = os.environ.get("ATHENA_RUN_PID", "").strip()
    boot_id = os.environ.get("ATHENA_RUN_BOOT_ID")
    if not role or not run_id:
        die("set ATHENA_RUN_ROLE and ATHENA_RUN_ID: every result tree must name its producing run")
    if len(nonce) < 32 or any(character not in "0123456789abcdef" for character in nonce):
        die("set ATHENA_RUN_NONCE to the >=32 hex-character nonce of this single execution")
    if not hostname or not pid_text.isdigit() or int(pid_text) <= 0 or boot_id is None:
        die("set ATHENA_RUN_HOSTNAME, ATHENA_RUN_PID and ATHENA_RUN_BOOT_ID from the single producing driver")
    return {"role": role, "run_id": run_id, "nonce": nonce, "hostname": hostname, "pid": int(pid_text),
            "boot_id": boot_id,
            "container": os.environ.get("ATHENA_DOCKER_CONTAINER", ""), "image": os.environ.get("ATHENA_DOCKER_IMAGE", ""),
            "image_id": os.environ.get("ATHENA_DOCKER_IMAGE_ID", "")}


def supplied_binaries(contract: dict[str, Any]) -> dict[str, dict[str, Any]]:
    binaries: dict[str, dict[str, Any]] = {}
    for role, spec in contract["binaries"].items():
        text = os.environ.get(spec["env"])
        if not text:
            die(f"missing executable for binary role {role}: set {spec['env']}")
        path = Path(text)
        if path.is_symlink() or not path.is_file() or not os.access(path, os.X_OK):
            die(f"{spec['env']} must name a regular executable: {path}")
        binaries[role] = {"path": path, "sha256": ct.sha256_file(path), "configure": list(spec["configure"])}
    if len({item["sha256"] for item in binaries.values()}) != len(binaries):
        die("distinct binary roles must be distinct executables (one build cannot alias another solver/problem/NGHOST configuration)")
    return binaries


def build_evidence(contract: dict[str, Any], binaries: dict[str, dict[str, Any]], output: Path) -> dict[str, Any]:
    """Copy the configure/make evidence of every used build role into raw/build/."""
    directory = os.environ.get("ATHENA_BUILD_EVIDENCE_DIR", "").strip()
    if not directory:
        die("set ATHENA_BUILD_EVIDENCE_DIR: every retained result must carry the configure/make evidence of its builds")
    root = Path(directory)
    if root.is_symlink() or not root.is_dir():
        die("ATHENA_BUILD_EVIDENCE_DIR must be a real directory: " + str(root))
    index: dict[str, Any] = {}
    for role, item in sorted(binaries.items()):
        record_path, log_path = root / f"{role}.json", root / f"{role}.log"
        if record_path.is_symlink() or not record_path.is_file() or log_path.is_symlink() or not log_path.is_file():
            die(f"build evidence for role {role} is incomplete ({record_path.name}, {log_path.name})")
        record = ct.strict_json_load(record_path)
        if not isinstance(record, dict) or record.get("schema") != BUILD_SCHEMA or record.get("role") != role:
            die(f"build record for role {role} is not an {BUILD_SCHEMA} record for that role")
        if record.get("configure_argv") != list(contract["binaries"][role]["configure"]):
            die(f"build record for role {role} does not carry the contract configure command")
        if record.get("binary_sha256") != item["sha256"]:
            die(f"build record for role {role} does not describe the supplied executable")
        if record.get("configure_returncode") != 0 or record.get("make_returncode") != 0:
            die(f"build record for role {role} does not report a successful configure/make")
        if record.get("log_sha256") != ct.sha256_file(log_path):
            die(f"build log for role {role} is not the log named by its record")
        destination = output / "raw" / "build"
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(record_path, destination / f"{role}.json")
        shutil.copyfile(log_path, destination / f"{role}.log")
        index[role] = {"record": {"path": f"raw/build/{role}.json", "sha256": ct.sha256_file(destination / f"{role}.json")},
                       "log": {"path": f"raw/build/{role}.log", "sha256": ct.sha256_file(destination / f"{role}.log")}}
    return index


def resolve_deck(case: dict[str, Any], check_dir: Path, source: Path) -> Path:
    deck = case["deck"]
    if deck["kind"] == "source":
        path = source / deck["path"]
    elif deck["kind"] == "check":
        path = check_dir / deck["path"]
    else:
        die("unknown deck kind for case " + case["id"])
    if path.is_symlink() or not path.is_file():
        die(f"missing deck for case {case['id']}: {path}")
    observed = ct.sha256_file(path)
    if observed != deck["sha256"]:
        die(f"deck identity mismatch for case {case['id']}: {path} sha256 {observed} != contract {deck['sha256']}")
    return path


def retain_source(source: Path, contract: dict[str, Any], output: Path) -> dict[str, Any]:
    manifest = ct.source_manifest(source)
    selected = sorted({relative for case in contract["cases"] for relative in case["source_paths"]})
    present = {entry["path"] for entry in manifest["files"]}
    missing = [relative for relative in selected if relative not in present]
    if missing:
        die("pinned source is missing declared files: " + ", ".join(missing))
    manifest["selected_paths"] = selected
    manifest["selected_source_sha256"] = ct.selected_source_digest(source, selected)
    destination = output / "raw" / "source"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "manifest.json").write_text(ct.strict_json_dump(manifest), encoding="utf-8")
    return {"index": {"path": "raw/source/manifest.json", "sha256": ct.sha256_file(destination / "manifest.json")},
            "selected_source_sha256": manifest["selected_source_sha256"], "tree_sha256": manifest["tree_sha256"], "file_count": manifest["file_count"]}


def execute_case(contract: dict[str, Any], case: dict[str, Any], check_dir: Path, source: Path,
                 binaries: dict[str, dict[str, Any]], output: Path, identity: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
    deck = resolve_deck(case, check_dir, source)
    binary = binaries[case["binary_role"]]
    run_dir = Path(tempfile.mkdtemp(prefix="athena-sr-mhd-case-"))
    command = [str(binary["path"]), "-i", str(deck), *case["overrides"]]
    started_at, started = now(), time.monotonic()
    timed_out = False
    try:
        completed = subprocess.run(command, cwd=run_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, timeout=float(case["cap_seconds"]))
        exit_status = completed.returncode if completed.returncode >= 0 else 128 + abs(completed.returncode)
        stdout, stderr = completed.stdout, completed.stderr
    except subprocess.TimeoutExpired as exc:
        timed_out, exit_status = True, 124
        stdout, stderr = exc.stdout or b"", exc.stderr or b""
    elapsed = time.monotonic() - started
    finished_at = now()

    raw_dir = output / "raw" / "cases" / case["id"]
    (raw_dir / "outputs").mkdir(parents=True, exist_ok=True)
    (raw_dir / "deck").mkdir(parents=True, exist_ok=True)
    (raw_dir / "stdout.log").write_bytes(stdout)
    (raw_dir / "stderr.log").write_bytes(stderr)
    deck_name = case["deck"]["path"].split("/")[-1]
    shutil.copyfile(deck, raw_dir / "deck" / deck_name)
    for path in sorted(run_dir.iterdir()):
        if path.is_symlink() or not path.is_file():
            die(f"case {case['id']} produced a non-regular run-directory entry: {path.name}")
        shutil.copyfile(path, raw_dir / "outputs" / path.name)
    output_files = ct.file_entries(raw_dir / "outputs", ct.regular_files(raw_dir / "outputs"))

    fatal = ct.extract_fatal(stdout, stderr)
    execution: dict[str, Any] = {
        "schema": EXECUTION_SCHEMA, "check": contract["slug"], "check_id": contract["id"], "case": case["id"],
        "contract_sha256": ct.sha256_canonical(contract), "source_commit": contract["source_commit"],
        "role": identity["role"], "run_id": identity["run_id"], "run_nonce": identity["nonce"],
        "hostname": identity["hostname"], "pid": identity["pid"], "boot_id": identity["boot_id"],
        "container": identity["container"], "image": identity["image"], "image_id": identity["image_id"],
        "process_started": True, "binary_role": case["binary_role"], "binary_path": str(binary["path"]), "binary_sha256": binary["sha256"],
        "configure_command": binary["configure"], "command": command, "cwd": str(run_dir),
        "deck": {"kind": case["deck"]["kind"], "path": case["deck"]["path"], "sha256": case["deck"]["sha256"], "resolved": str(deck), "raw": f"deck/{deck_name}"},
        "overrides": list(case["overrides"]), "dimensions": list(case["dimensions"]), "meshblock": list(case["meshblock"]),
        "started_at": started_at, "finished_at": finished_at, "elapsed_seconds": elapsed,
        "exit_status": exit_status, "timed_out": timed_out,
        "stdout_sha256": ct.sha256_bytes(stdout), "stderr_sha256": ct.sha256_bytes(stderr), "stdout_bytes": len(stdout), "stderr_bytes": len(stderr),
        "fatal_message": fatal, "output_files": output_files, "expectation": case["expectation"], "completion": None,
    }

    if case["expectation"] == "rejection":
        if not fatal or case["rejection_marker"] not in fatal:
            die(f"case {case['id']} was expected to be rejected with '{case['rejection_marker']}' but the process reported: {fatal or 'no fatal message'} (exit {exit_status})")
        # A guard that fires inside the first task-list stage (e.g. ssprk5_4)
        # runs after the t=0 output; only later frames prove an update ran.
        advanced = sorted({info["frame"] for entry in output_files for info in [ct.parse_output_name(entry["path"])] if info and info["frame"] > 0})
        if advanced:
            die(f"case {case['id']} was expected to be rejected but produced advanced output frames {advanced}")
        execution["rejection_observed"] = True
        execution["initial_frames_before_rejection"] = sorted({info["out"] for entry in output_files for info in [ct.parse_output_name(entry["path"])] if info})
        evidence: dict[str, Any] = {"frames": [], "error_file": None, "diagnostics": {}}
    else:
        if timed_out:
            die(f"case {case['id']} exceeded its declared cap of {case['cap_seconds']} s")
        if exit_status != 0 or fatal:
            die(f"case {case['id']} failed: exit {exit_status}; {fatal or stderr.decode('utf-8', 'replace')[-600:]}")
        try:
            native = ct.native_evidence(contract, case, raw_dir / "outputs")
        except ValueError as exc:
            die(str(exc))
        termination = ct.parse_termination(stdout.decode("utf-8", "replace"))
        if termination is None:
            die(f"case {case['id']} stdout carries no native termination block")
        check_termination(contract, case, termination, native["final_cycle"])
        execution["completion"] = termination
        evidence = native["evidence"]
    (raw_dir / "execution.json").write_text(ct.strict_json_dump(execution), encoding="utf-8")
    raw_index = {
        "execution": {"path": f"raw/cases/{case['id']}/execution.json", "sha256": ct.sha256_file(raw_dir / "execution.json")},
        "stdout": {"path": f"raw/cases/{case['id']}/stdout.log", "sha256": execution["stdout_sha256"]},
        "stderr": {"path": f"raw/cases/{case['id']}/stderr.log", "sha256": execution["stderr_sha256"]},
        "deck": {"path": f"raw/cases/{case['id']}/deck/{deck_name}", "sha256": case["deck"]["sha256"]},
        "outputs": [{"path": f"raw/cases/{case['id']}/outputs/{entry['path']}", "sha256": entry["sha256"], "bytes": entry["bytes"]} for entry in output_files],
    }
    return {"id": case["id"], "runtime": execution, "evidence": evidence}, raw_index


def check_termination(contract: dict[str, Any], case: dict[str, Any], termination: dict[str, Any], final_cycle: int) -> None:
    """Cross-check the native stdout termination block against contract and frames."""
    tolerance = float(contract["stdout_time_relative_tolerance"])
    expected_time = float(case["frames"][-1])
    if termination["terminated_on"] != NORMAL_TERMINATION:
        die(f"case {case['id']} terminated on '{termination['terminated_on']}', a completed contract case must terminate on the {NORMAL_TERMINATION}")
    if termination["nlim"] != -1:
        die(f"case {case['id']} ran with nlim={termination['nlim']}; the pinned decks declare nlim=-1")
    for key in ("time", "tlim"):
        if abs(termination[key] - expected_time) > tolerance * max(1.0, abs(expected_time)):
            die(f"case {case['id']} stdout reports {key}={termination[key]}, contract expects {expected_time}")
    if termination["cycle"] != final_cycle:
        die(f"case {case['id']} stdout reports cycle {termination['cycle']}, native frames report {final_cycle}")
    expected_zone_cycles = final_cycle * math.prod(case["dimensions"])
    if termination["zone_cycles"] != expected_zone_cycles:
        die(f"case {case['id']} stdout reports zone-cycles {termination['zone_cycles']}, cycles x mesh cells is {expected_zone_cycles}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source", type=Path, default=None)
    args = parser.parse_args()
    if args.output.is_symlink():
        die("output must not be a symlink")
    args.output.mkdir(parents=True, exist_ok=True)
    if any(args.output.iterdir()):
        die("output directory must be empty")
    source = args.source or Path(os.environ.get("ATHENA_SOURCE_DIR", "/opt/athena"))
    if source.is_symlink() or not source.is_dir():
        die("missing pinned source directory: " + str(source))
    contract = load_contract(args.contract)
    check_dir = args.contract.resolve().parents[1]
    if check_dir.name != contract["slug"]:
        die(f"contract slug {contract['slug']} does not match its check directory {check_dir.name}")
    identity = run_identity()
    binaries = supplied_binaries(contract)
    builds = build_evidence(contract, binaries, args.output)
    source_index = retain_source(source, contract, args.output)
    records, raw_cases = [], {}
    for case in contract["cases"]:
        record, raw_index = execute_case(contract, case, check_dir, source, binaries, args.output, identity)
        records.append(record)
        raw_cases[case["id"]] = raw_index
    document = {
        "schema": SCHEMA, "check": contract["slug"], "check_id": contract["id"], "source_commit": contract["source_commit"], "module": contract["module"],
        "contract_sha256": ct.sha256_canonical(contract),
        "binaries": {role: {"sha256": item["sha256"], "configure": item["configure"], "path": str(item["path"])} for role, item in binaries.items()},
        "selected_source_sha256": source_index["selected_source_sha256"],
        "run": {"role": identity["role"], "run_id": identity["run_id"], "nonce": identity["nonce"], "hostname": identity["hostname"], "pid": identity["pid"],
                "boot_id": identity["boot_id"], "container": identity["container"], "image": identity["image"], "image_id": identity["image_id"],
                "results_root": str(args.output.resolve().parent), "check_root": str(args.output.resolve())},
        "raw_evidence": {"schema": RAW_SCHEMA, "source": source_index["index"], "source_tree_sha256": source_index["tree_sha256"],
                         "source_file_count": source_index["file_count"], "builds": builds, "cases": raw_cases},
        "execution": {"mode": "compiled_athena_runtime", "real_solver_runs": sum(1 for record in records if record["runtime"]["process_started"]),
                      "completed_runs": sum(1 for record in records if record["runtime"]["expectation"] == "run"),
                      "observed_rejections": sum(1 for record in records if record["runtime"].get("rejection_observed"))},
        "narrowed_rows": contract["narrowed_rows"],
        "cases": records,
    }
    text = ct.strict_json_dump(document)
    if not ct.finite_tree(document):
        die("artifact contains a non-finite value")
    (args.output / "observables.json").write_text(text, encoding="utf-8")
    raw_files = ct.regular_files(args.output / "raw")
    print(ct.strict_json_dump({"check": contract["id"], "cases": len(records), "real_solver_runs": document["execution"]["real_solver_runs"],
                               "raw_files": len(raw_files), "raw_bytes": sum(path.stat().st_size for path in raw_files),
                               "bytes": len(text.encode("utf-8"))}), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
