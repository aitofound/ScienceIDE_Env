#!/usr/bin/env python3
"""Emit one deterministic, source-selected SR-MHD coverage artifact.

The runner separates mechanics from science policy. It enumerates all cases and
their forcing/source branches, fingerprints supplied pinned source and solver
binaries, and records native-state serialization gaps. It never invents a
 tolerance and never presents a manifest as a scientific pass; final policy
remains owner-pending.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path

SCHEMA = "athena-sr-mhd-coverage/v2"

def strict_load(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))

def hash_selected(source: Path, paths: list[str]):
    digest = hashlib.sha256()
    missing = []
    for rel in sorted(set(paths)):
        path = source / rel
        if not path.is_file() or path.is_symlink():
            missing.append(rel)
            continue
        digest.update(rel.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return (digest.hexdigest() if not missing else "unavailable", not missing, missing)

def hash_file(text: str):
    path = Path(text)
    if not path.is_file() or path.is_symlink():
        return "unavailable"
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.is_symlink():
        raise SystemExit("output must not be a symlink")
    args.output.mkdir(parents=True, exist_ok=True)
    if any(args.output.iterdir()):
        raise SystemExit("output directory must be empty")
    contract = strict_load(args.contract)
    source = Path(os.environ.get("ATHENA_SOURCE_DIR", "/opt/athena"))
    source_present = source.is_dir() and not source.is_symlink()
    if source_present:
        source_hash, source_complete, missing = hash_selected(source, contract["source_paths"])
    else:
        source_hash, source_complete, missing = "unavailable", False, list(contract["source_paths"])
    binaries = {}
    for role in ("ATHENA_BINARY", "ATHENA_BINARY_HLLD", "ATHENA_BINARY_HLLE", "ATHENA_BINARY_LLF"):
        value = os.environ.get(role)
        if value:
            binaries[role] = hash_file(value)
    cases = []
    common = {"id", "label", "solver", "deck", "requested_command", "source_paths", "evidence_class"}
    for original in contract["cases"]:
        cases.append({
            "id": original["id"],
            "label": original["label"],
            "solver": original["solver"],
            "deck": original["deck"],
            "requested_command": original["requested_command"],
            "source_paths": original["source_paths"],
            "evidence_class": original["evidence_class"],
            "configuration": {key: value for key, value in original.items() if key not in common},
        })
    ids = [item["id"] for item in cases]
    solvers = sorted({item["solver"] for item in cases})
    decks = sorted({item["deck"] for item in cases})
    commands = []
    for item in cases:
        if item["requested_command"] not in commands:
            commands.append(item["requested_command"])
    markers = sorted({path for item in cases for path in item["source_paths"]})
    native_gap = contract["id"] in {"SRMHD-C10", "SRMHD-C11", "SRMHD-C12", "SRMHD-C13"}
    document = {
        "schema": SCHEMA,
        "check": contract["slug"],
        "check_id": contract["id"],
        "source_commit": contract["source_commit"],
        "module": "ideal special-relativistic MHD",
        "cases": cases,
        "coverage": {
            "case_count": len(cases),
            "case_ids": ids,
            "solver_set": solvers,
            "deck_set": decks,
            "requested_commands": commands,
            "branch_markers": markers,
            "enumeration": "contract.json explicit order; no hidden matrix",
        },
        "execution": {
            "mode": "incumbent_source_contract_enumeration",
            "status": "completed_without_scientific_verdict",
            "source_runtime": "pinned source inspected" if source_present else "pinned source not mounted in isolated check context",
            "source_selected_files_present": source_complete,
            "source_selected_files_missing": missing,
            "selected_source_sha256": source_hash,
            "binary_sha256": binaries,
            "real_solver_runs": 0,
            "native_diagnostic_status": "source-only serialization gap; owner must approve native FaceField/EdgeField instrumentation" if native_gap else "not applicable to this check",
        },
        "policy": {
            "status": "owner_pending_final_science_policy",
            "comparison": "provisional_exact_identity_self_test_only",
            "evidence_class": contract["evidence_class"],
            "boundary": contract["policy_boundary"],
            "official_evidence": contract["official_evidence"],
        },
        "artifacts": {
            "declared": ["observables.json"],
            "required_sections": ["execution", "coverage", "policy", "artifacts"],
            "state_witness": "full candidate state and native diagnostics remain required by the owner-approved final contract; this artifact records forcing/enumeration mechanics",
            "native_state": "native arrays are not claimed unless explicitly instrumented; derived projections remain labelled",
        },
    }
    args.output.joinpath("observables.json").write_text(json.dumps(document, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"check": contract["id"], "folder": contract["slug"], "case_count": len(cases), "source_present": source_present, "binary_roles": sorted(binaries)}, sort_keys=True))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
