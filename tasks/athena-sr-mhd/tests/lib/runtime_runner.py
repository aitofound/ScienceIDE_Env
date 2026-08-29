#!/usr/bin/env python3
"""Execute every explicit SR-MHD contract case with the pinned Athena++ binary.

This runner intentionally records runtime mechanics separately from scientific
policy.  It uses the official source decks (with small, explicitly recorded
runtime overrides so the complete 16-check oracle is operationally bounded),
never edits the vendored source, and never fabricates internal FaceField/EMF
telemetry that the pinned formatted-table output does not expose.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any

from extract_observables import read_frames, VARIABLES

SOURCE_COMMIT = "823614c90b594472747a0ac2a699e4a454f300d2"
SCHEMA = "athena-sr-mhd-coverage/v2"
MODULE = "ideal special-relativistic MHD"
POLICY = "provisional_exact_identity_self_test_only"


def die(message: str) -> None:
    raise SystemExit(message)


def sha256_file(path: Path) -> str:
    if path.is_symlink() or not path.is_file():
        die("expected a regular file: " + str(path))
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical(value: Any) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        die(f"cannot load strict contract {path}: {exc}")


def source_identity(source: Path, contracts: list[dict[str, Any]]) -> tuple[str, bool, list[str]]:
    paths = sorted({relative for contract in contracts for case in contract["cases"] for relative in case["source_paths"]})
    digest = hashlib.sha256()
    missing: list[str] = []
    for relative in paths:
        path = source / relative
        if path.is_symlink() or not path.is_file():
            missing.append(relative)
            continue
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return (digest.hexdigest() if not missing else "unavailable", not missing, missing)


def supplied_binaries() -> dict[str, Path]:
    result: dict[str, Path] = {}
    for role in ("ATHENA_BINARY", "ATHENA_BINARY_HLLD", "ATHENA_BINARY_HLLE", "ATHENA_BINARY_LLF"):
        text = os.environ.get(role)
        if text:
            path = Path(text)
            if path.is_symlink() or not path.is_file() or not os.access(path, os.X_OK):
                die(f"{role} must name a regular executable: {path}")
            result[role] = path
    if "ATHENA_BINARY" not in result:
        die("ATHENA_BINARY (linear HLLD oracle executable) is required")
    return result


def requested_flag(case: dict[str, Any]) -> int:
    # C01-C05 and the wave portion of C12 carry the audited flag in the case ID.
    # Other checks intentionally keep the official flag-0 state while changing
    # reconstruction, integrator, mesh, or diagnostic paths.
    match = re.search(r"(?:flag|recovery)-([0-6])$", case["id"])
    return int(match.group(1)) if match else 0


def runtime_deck(source: Path, case: dict[str, Any]) -> tuple[Path, str]:
    requested = case["deck"]
    if requested == "athinput.linear_wave":
        return source / "inputs/mhd_sr/athinput.linear_wave", "official pinned mhd_sr/athinput.linear_wave"
    if requested.startswith("athinput.mub_"):
        match = re.search(r"mub_([1-4])", requested)
        number = match.group(1) if match else "1"
        return source / f"inputs/mhd_sr/athinput.mub_{number}", f"official pinned mhd_sr/athinput.mub_{number}"
    if requested == "athinput.mub_1..mub_4":
        return source / "inputs/mhd_sr/athinput.mub_1", "official pinned mhd_sr/athinput.mub_1 (representative of explicit deck family)"
    # The owner-edge placeholder is deliberately not treated as a runnable deck.
    # Execute the nearest official SR-MHD linear-wave case and retain the requested
    # placeholder in metadata so the policy gap remains visible.
    return source / "inputs/mhd_sr/athinput.linear_wave", "official pinned mhd_sr/athinput.linear_wave (owner-edge placeholder fallback)"


def is_shock_deck(case: dict[str, Any]) -> bool:
    return case["deck"].startswith("athinput.mub_") or case["deck"] == "athinput.mub_1..mub_4"


def runtime_shape(slug: str, case: dict[str, Any]) -> list[int]:
    if is_shock_deck(case):
        return [32, 1, 1]
    if slug == "11-directional-flux-ct" and case["id"] in {"direction-x2", "direction-x3"}:
        return [8, 8, 8]
    # These are official-derived shapes, bounded for the CPU oracle while the
    # contract retains the official dimensions and source provenance.
    return [8, 4, 4]


def runtime_overrides(slug: str, case: dict[str, Any], dimensions: list[int]) -> list[str]:
    overrides = [
        f"mesh/nx1={dimensions[0]}", f"mesh/nx2={dimensions[1]}", f"mesh/nx3={dimensions[2]}",
        "time/tlim=0.01", "time/nlim=2", "time/ncycle_out=0",
        # The official linear-wave deck declares <meshblock>, while the official
        # shock-tube decks do not.  Passing a nonexistent block is a fatal
        # ParameterInput error in the compiled shock binary, so retain the
        # source-declared block boundary rather than inventing one.
    ]
    if not is_shock_deck(case):
        overrides.extend([f"meshblock/nx1={dimensions[0]}", f"meshblock/nx2={dimensions[1]}", f"meshblock/nx3={dimensions[2]}"])
    overrides.extend([
        "output1/dt=0.01", "output1/file_type=tab", "output1/variable=prim",
    ])
    if not is_shock_deck(case):
        overrides.append("problem/wave_flag=" + str(requested_flag(case)))
    if slug == "11-directional-flux-ct" and case["id"] == "shock-direction-triad":
        # The official gr_linear_wave deck has no shock_dir parameter.  Keep the
        # x2/x3 owner-pinned cases as genuine 3-D compiled runs without injecting
        # a nonexistent block key; the shock triad uses the official shock deck.
        overrides.extend(["problem/shock_dir=1", "problem/xshock=0.0"])
    if slug == "14-reconstruction-matrix":
        value = case["id"].split("reconstruction-xorder-", 1)[-1]
        overrides.append("time/xorder=" + value)
    if slug == "15-time-integrator-matrix":
        value = case["id"].split("integrator-", 1)[-1]
        overrides.append("time/integrator=" + value)
    return overrides


def solver_binary(case: dict[str, Any], slug: str, binaries: dict[str, Path]) -> tuple[str, Path]:
    solver = case.get("solver", "hlld")
    if is_shock_deck(case) or slug in {"06-hlld-shocks", "07-hlle-shocks", "08-llf-shocks", "09-solver-deck-cross-product", "12-recovery-admissibility", "13-floors-fallback-ceiling", "16-mesh-boundary-output"} and "mub" in case["deck"]:
        role = "ATHENA_BINARY_" + solver.upper()
        if role not in binaries:
            die(f"missing solver-specific executable {role}")
        return role, binaries[role]
    return "ATHENA_BINARY", binaries["ATHENA_BINARY"]


def output_inventory(run_dir: Path) -> list[dict[str, str]]:
    files: list[dict[str, str]] = []
    for path in sorted(run_dir.iterdir(), key=lambda item: item.name):
        if path.is_symlink() or not path.is_file():
            continue
        files.append({"path": path.name, "sha256": sha256_file(path)})
    return files


def execute_case(source: Path, slug: str, case: dict[str, Any], binaries: dict[str, Path], cap: float) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    deck, deck_note = runtime_deck(source, case)
    if deck.is_symlink() or not deck.is_file():
        die(f"missing official runtime deck: {deck}")
    dimensions = runtime_shape(slug, case)
    overrides = runtime_overrides(slug, case, dimensions)
    role, binary = solver_binary(case, slug, binaries)
    run_dir = Path(tempfile.mkdtemp(prefix="athena309-v2-case-"))
    command = [str(binary), "-i", str(deck), *overrides]
    config_identity = sha256_bytes(canonical({"contract_case": case, "runtime_deck": str(deck), "overrides": overrides, "dimensions": dimensions}))
    started = time.monotonic()
    timed_out = False
    try:
        completed = subprocess.run(command, cwd=run_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, timeout=cap)
        # subprocess uses a negative return code for signal termination; retain
        # that process outcome as the conventional nonnegative shell status so
        # the JSON identity remains strict and finite.
        exit_status = completed.returncode if completed.returncode >= 0 else 128 + abs(completed.returncode)
        stdout = completed.stdout
        stderr = completed.stderr
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        exit_status = 124
        stdout = exc.stdout or b""
        stderr = exc.stderr or b""
    elapsed = time.monotonic() - started
    files = output_inventory(run_dir)
    frames: list[dict[str, Any]] = []
    parse_error = ""
    if exit_status == 0:
        try:
            frames = read_frames(run_dir, dimensions)
        except Exception as exc:
            parse_error = str(exc)
    expected_probe = (
        (slug == "14-reconstruction-matrix" and case["id"].endswith(("-4", "-4c")))
        or (slug == "15-time-integrator-matrix" and case["id"] == "integrator-ssprk5_4")
    )
    expected_rejection = expected_probe and (exit_status != 0 or bool(parse_error))
    if exit_status != 0 and not expected_rejection:
        die(f"Athena++ runtime failed for {case['id']} status={exit_status}: {stderr.decode('utf-8', 'replace')[-800:]}")
    if exit_status == 0 and parse_error and not expected_rejection:
        die(f"Athena++ output could not be parsed for {case['id']}: {parse_error}")
    runtime = {
        "process_started": True,
        "binary_role": role,
        "binary_path": str(binary),
        "binary_sha256": sha256_file(binary),
        "command": command,
        "cwd": str(run_dir),
        "requested_deck": case["deck"],
        "runtime_deck": str(deck),
        "runtime_deck_note": deck_note,
        "runtime_deck_sha256": sha256_file(deck),
        "config_identity_sha256": config_identity,
        "runtime_dimensions": dimensions,
        "overrides": overrides,
        "exit_status": exit_status,
        "timed_out": timed_out,
        "elapsed_seconds": elapsed,
        "stdout_sha256": sha256_bytes(stdout),
        "stderr_sha256": sha256_bytes(stderr),
        "output_files": files,
        "expected_rejection": expected_rejection,
        "raw_observable_source": "Athena++ native TAB output" if not expected_rejection else "Athena++ process rejection output",
        "frames": frames,
    }
    return runtime, frames


def run_contract(contract: dict[str, Any], source: Path, binaries: dict[str, Path], cap: float) -> dict[str, Any]:
    slug = contract["slug"]
    case_documents: list[dict[str, Any]] = []
    run_records: list[dict[str, Any]] = []
    for original in contract["cases"]:
        runtime, frames = execute_case(source, slug, original, binaries, cap)
        configuration = {key: value for key, value in original.items() if key not in {"id", "label", "solver", "deck", "requested_command", "source_paths", "evidence_class"}}
        configuration["runtime"] = runtime
        case_documents.append({
            "id": original["id"], "label": original["label"], "solver": original["solver"], "deck": original["deck"],
            "requested_command": original["requested_command"], "source_paths": original["source_paths"],
            "evidence_class": original["evidence_class"], "configuration": configuration,
        })
        run_records.append(runtime)
    ids = [item["id"] for item in case_documents]
    solvers = sorted({item["solver"] for item in case_documents})
    decks = sorted({item["deck"] for item in case_documents})
    commands: list[list[str]] = []
    for item in case_documents:
        if item["requested_command"] not in commands:
            commands.append(item["requested_command"])
    markers = sorted({path for item in case_documents for path in item["source_paths"]})
    return {
        "schema": SCHEMA,
        "check": slug,
        "check_id": contract["id"],
        "source_commit": contract["source_commit"],
        "module": MODULE,
        "cases": case_documents,
        "coverage": {
            "case_count": len(case_documents), "case_ids": ids, "solver_set": solvers, "deck_set": decks,
            "requested_commands": commands, "branch_markers": markers,
            "enumeration": "contract.json explicit order; no hidden matrix",
        },
        "execution": {
            "mode": "compiled_athena_runtime",
            "status": "completed_without_scientific_verdict",
            "source_runtime": "pinned source executed",
            "source_selected_files_present": True,
            "source_selected_files_missing": [],
            "selected_source_sha256": "",
            "binary_sha256": {},
            "real_solver_runs": len(run_records),
            "runs": run_records,
            "native_diagnostic_status": "native FaceField/edge-EMF arrays are not serialized by pinned formatted_table.cpp; raw TAB/Bcc observables retained without fabricated telemetry",
        },
        "policy": {
            "status": "owner_pending_final_science_policy",
            "comparison": POLICY,
            "evidence_class": contract["evidence_class"],
            "boundary": contract["policy_boundary"],
            "official_evidence": contract["official_evidence"],
        },
        "artifacts": {
            "declared": ["observables.json"],
            "required_sections": ["execution", "coverage", "policy", "artifacts"],
            "state_witness": "per-case native TAB-derived frames plus exact process/deck/config/output identities",
            "native_state": "not serialized by pinned source; no internal FaceField/EMF values fabricated",
            "output_file_hashes": [{"case_id": case["id"], "files": run_records[index]["output_files"]} for index, case in enumerate(case_documents)],
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source", type=Path, default=None)
    parser.add_argument("--cap", type=float, default=float(os.environ.get("ATHENA_OPERATIONAL_CAP_SECONDS", "120")))
    args = parser.parse_args()
    if not math.isfinite(args.cap) or args.cap <= 0:
        die("runtime cap must be finite and positive")
    if args.output.is_symlink():
        die("output must not be a symlink")
    args.output.mkdir(parents=True, exist_ok=True)
    if any(args.output.iterdir()):
        die("output directory must be empty")
    source = args.source or Path(os.environ.get("ATHENA_SOURCE_DIR", "/opt/athena"))
    if source.is_symlink() or not source.is_dir():
        die("missing pinned source directory: " + str(source))
    contract = load_json(args.contract)
    if not isinstance(contract, dict) or contract.get("source_commit") != SOURCE_COMMIT:
        die("contract does not bind the pinned Athena++ source commit")
    binaries = supplied_binaries()
    # All four role hashes are part of the trace even if a check uses only a
    # subset.  This prevents a solver alias from being hidden in the receipt.
    roles = {role: sha256_file(path) for role, path in binaries.items()}
    source_hash, source_complete, source_missing = source_identity(source, [contract])
    document = run_contract(contract, source, binaries, args.cap)
    document["execution"]["selected_source_sha256"] = source_hash
    document["execution"]["source_selected_files_present"] = source_complete
    document["execution"]["source_selected_files_missing"] = source_missing
    document["execution"]["binary_sha256"] = roles
    destination = args.output / "observables.json"
    destination.write_text(json.dumps(document, allow_nan=False, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    print(json.dumps({"check": contract["id"], "case_count": len(contract["cases"]), "real_solver_runs": document["execution"]["real_solver_runs"], "binary_roles": sorted(roles)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
