#!/usr/bin/env python3
"""Fail-closed validator for compiled Athena++ SR-MHD v2 artifacts.

The self-test comparison is intentionally exact identity, while runtime
requirements are independent: every case must carry a started Athena++ process,
positive real_solver_runs, and traceable command/deck/config/output/binary
identities.  Scientific tolerances remain owner-pending by contract.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
from typing import Any

SCHEMA = "athena-sr-mhd-coverage/v2"
MODULE = "ideal special-relativistic MHD"
POLICY = "provisional_exact_identity_self_test_only"
TOP = {"schema", "check", "check_id", "source_commit", "module", "cases", "coverage", "execution", "policy", "artifacts"}
CASE = {"id", "label", "solver", "deck", "requested_command", "source_paths", "evidence_class", "configuration"}
SHA_LEN = 64


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for key, value in pairs:
        if key in out:
            raise ValueError("duplicate JSON key: " + key)
        out[key] = value
    return out


def _constant(value: str) -> None:
    raise ValueError("non-finite JSON constant: " + value)


def _load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_pairs, parse_constant=_constant)


def _finite(value: Any) -> bool:
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return True
    if isinstance(value, (int, float)):
        try:
            return math.isfinite(float(value))
        except (TypeError, ValueError, OverflowError):
            return False
    if isinstance(value, list):
        return all(_finite(item) for item in value)
    if isinstance(value, dict):
        return all(isinstance(key, str) and _finite(item) for key, item in value.items())
    return False


def _sha(value: Any) -> bool:
    return isinstance(value, str) and len(value) == SHA_LEN and all(char in "0123456789abcdef" for char in value)


def _artifact(directory: Path) -> tuple[Path | None, str]:
    if directory.is_symlink() or not directory.is_dir():
        return None, "artifact directory missing or symlinked"
    try:
        names = sorted(path.name for path in directory.iterdir())
    except OSError as exc:
        return None, "cannot inspect artifact directory: " + str(exc)
    if names != ["observables.json"]:
        return None, "artifact directory must contain exactly observables.json"
    path = directory / "observables.json"
    if path.is_symlink() or not path.is_file():
        return None, "artifact must be a regular file"
    return path, ""


def _runtime(runtime: Any, expected_case: dict[str, Any], label: str) -> str | None:
    if not isinstance(runtime, dict):
        return label + ": configuration.runtime missing"
    required = {
        "process_started", "binary_role", "binary_path", "binary_sha256", "command", "cwd",
        "requested_deck", "runtime_deck", "runtime_deck_note", "runtime_deck_sha256",
        "config_identity_sha256", "runtime_dimensions", "overrides", "exit_status", "timed_out",
        "elapsed_seconds", "stdout_sha256", "stderr_sha256", "output_files", "expected_rejection",
        "raw_observable_source", "frames",
    }
    if set(runtime) != required:
        return label + ": runtime identity keys malformed"
    if runtime["process_started"] is not True:
        return label + ": compiled process was not started"
    if runtime["requested_deck"] != expected_case["deck"]:
        return label + ": requested deck identity mismatch"
    if not all(_sha(runtime[key]) for key in ("binary_sha256", "runtime_deck_sha256", "config_identity_sha256", "stdout_sha256", "stderr_sha256")):
        return label + ": runtime SHA256 identity malformed"
    command = runtime["command"]
    if not isinstance(command, list) or len(command) < 3 or not all(isinstance(item, str) and item for item in command):
        return label + ": exact runtime command missing"
    if command[1] != "-i" or command[2] != runtime["runtime_deck"]:
        return label + ": command does not bind the recorded runtime deck"
    dimensions = runtime["runtime_dimensions"]
    if not isinstance(dimensions, list) or len(dimensions) != 3 or any(type(value) is not int or value <= 0 for value in dimensions):
        return label + ": runtime dimensions malformed"
    if not isinstance(runtime["overrides"], list) or not all(isinstance(item, str) and item for item in runtime["overrides"]):
        return label + ": runtime overrides malformed"
    if type(runtime["exit_status"]) is not int or runtime["exit_status"] < 0:
        return label + ": exit status malformed"
    if type(runtime["timed_out"]) is not bool or not isinstance(runtime["elapsed_seconds"], (int, float)) or not math.isfinite(float(runtime["elapsed_seconds"])) or runtime["elapsed_seconds"] < 0:
        return label + ": runtime timing malformed"
    files = runtime["output_files"]
    if not isinstance(files, list):
        return label + ": output file hash list malformed"
    for item in files:
        if not isinstance(item, dict) or set(item) != {"path", "sha256"} or not isinstance(item["path"], str) or not item["path"] or not _sha(item["sha256"]):
            return label + ": output file hash entry malformed"
    if not isinstance(runtime["frames"], list):
        return label + ": native TAB observable frames missing"
    if runtime["exit_status"] == 0 and not runtime["expected_rejection"] and not runtime["frames"]:
        return label + ": successful solver run has no parsed native frames"
    if runtime["exit_status"] == 0 and not runtime["expected_rejection"] and not files:
        return label + ": successful solver run has no output file hashes"
    return None


def _check(document: Any, contract: dict[str, Any], label: str) -> str | None:
    if not isinstance(document, dict) or set(document) != TOP:
        return label + ": top-level keys malformed"
    if not _finite(document):
        return label + ": non-finite value"
    for key, expected in (("schema", SCHEMA), ("check", contract["slug"]), ("check_id", contract["id"]), ("source_commit", contract["source_commit"]), ("module", MODULE)):
        if document.get(key) != expected:
            return label + ": identity metadata mismatch for " + key
    cases = document["cases"]
    expected_cases = contract["cases"]
    if not isinstance(cases, list) or len(cases) != len(expected_cases):
        return label + ": case count mismatch"
    if [item.get("id") if isinstance(item, dict) else None for item in cases] != [item["id"] for item in expected_cases]:
        return label + ": explicit case order/IDs mismatch"
    for item, expected in zip(cases, expected_cases):
        if not isinstance(item, dict) or set(item) != CASE:
            return label + ": case keys malformed"
        for key in ("id", "label", "solver", "deck", "evidence_class"):
            if item[key] != expected[key]:
                return label + ": contract case metadata mismatch for " + key
        if item["requested_command"] != expected["requested_command"]:
            return label + ": requested configure command mismatch"
        if item["source_paths"] != expected["source_paths"]:
            return label + ": source-path binding mismatch"
        if not isinstance(item["configuration"], dict) or "runtime" not in item["configuration"]:
            return label + ": runtime configuration missing"
        issue = _runtime(item["configuration"]["runtime"], expected, label + " case " + expected["id"])
        if issue:
            return issue
    coverage = document["coverage"]
    expected_ids = [item["id"] for item in expected_cases]
    if not isinstance(coverage, dict) or coverage.get("case_count") != len(cases) or coverage.get("case_ids") != expected_ids or coverage.get("enumeration") != "contract.json explicit order; no hidden matrix":
        return label + ": coverage enumeration contradicts cases"
    if coverage.get("solver_set") != sorted({item["solver"] for item in cases}) or coverage.get("deck_set") != sorted({item["deck"] for item in cases}):
        return label + ": solver/deck enumeration mismatch"
    commands: list[list[str]] = []
    for item in cases:
        if item["requested_command"] not in commands:
            commands.append(item["requested_command"])
    if coverage.get("requested_commands") != commands:
        return label + ": command enumeration mismatch"
    execution = document["execution"]
    required_execution = {"mode", "status", "source_runtime", "source_selected_files_present", "source_selected_files_missing", "selected_source_sha256", "binary_sha256", "real_solver_runs", "runs", "native_diagnostic_status"}
    if not isinstance(execution, dict) or set(execution) != required_execution:
        return label + ": execution keys malformed"
    if execution["mode"] != "compiled_athena_runtime" or execution["status"] != "completed_without_scientific_verdict" or execution["source_runtime"] != "pinned source executed":
        return label + ": execution status does not prove compiled runtime"
    if execution["source_selected_files_present"] is not True or execution["source_selected_files_missing"] != [] or not _sha(execution["selected_source_sha256"]):
        return label + ": pinned source identity is incomplete"
    binaries = execution["binary_sha256"]
    if not isinstance(binaries, dict) or not binaries or any(not isinstance(key, str) or not _sha(value) for key, value in binaries.items()):
        return label + ": binary SHA256 map is incomplete"
    runs = execution["runs"]
    if not isinstance(runs, list) or len(runs) != len(cases) or any(not isinstance(run, dict) or run.get("process_started") is not True for run in runs):
        return label + ": one started solver record is required per case"
    if type(execution["real_solver_runs"]) is not int or execution["real_solver_runs"] != sum(run.get("process_started") is True for run in runs) or execution["real_solver_runs"] <= 0:
        return label + ": real_solver_runs is not a positive traceable count"
    policy = document["policy"]
    if not isinstance(policy, dict) or policy.get("status") != "owner_pending_final_science_policy" or policy.get("comparison") != POLICY:
        return label + ": policy boundary malformed"
    artifacts = document["artifacts"]
    if not isinstance(artifacts, dict) or artifacts.get("declared") != ["observables.json"] or artifacts.get("required_sections") != ["execution", "coverage", "policy", "artifacts"]:
        return label + ": artifact contract malformed"
    return None


def validate(reference_dirs, candidate_dirs):
    try:
        check_dir = Path(__file__).resolve().parents[1] / "checks"
        # The caller loads this shared module directly; recover the check slug
        # from each artifact and bind it to that check's immutable contract.
        if len(reference_dirs) != 1 or len(candidate_dirs) != 1:
            return {"passed": False, "policy": POLICY, "reason": "exactly one reference and candidate required"}
        ref_path, ref_reason = _artifact(Path(reference_dirs[0]))
        cand_path, cand_reason = _artifact(Path(candidate_dirs[0]))
        if ref_path is None or cand_path is None:
            return {"passed": False, "policy": POLICY, "reason": "reference: " + ref_reason if ref_path is None else "candidate: " + cand_reason}
        reference = _load(ref_path)
        candidate = _load(cand_path)
        if not isinstance(reference, dict) or not isinstance(candidate, dict):
            return {"passed": False, "policy": POLICY, "reason": "artifact is not an object"}
        slug = reference.get("check")
        if not isinstance(slug, str) or not slug:
            return {"passed": False, "policy": POLICY, "reason": "reference check slug missing"}
        contract_path = check_dir / slug / "config" / "contract.json"
        if not contract_path.is_file():
            return {"passed": False, "policy": POLICY, "reason": "unknown check contract: " + slug}
        contract = _load(contract_path)
        for label, document in (("reference", reference), ("candidate", candidate)):
            issue = _check(document, contract, label)
            if issue:
                return {"passed": False, "policy": POLICY, "reason": issue}
        same = reference == candidate
        return {
            "passed": same,
            "policy": POLICY,
            "case_count": len(reference["cases"]),
            "real_solver_runs": reference["execution"]["real_solver_runs"],
            "reason": "provisional exact-identity self-test comparison passed" if same else "candidate differs from exact runtime artifact",
            "warnings": ["exact identity is accepted only for the explicit oracle self-test; final science tolerances remain owner-pending"] if same else [],
        }
    except Exception as exc:
        return {"passed": False, "policy": POLICY, "reason": "validator exception: " + str(exc)}
