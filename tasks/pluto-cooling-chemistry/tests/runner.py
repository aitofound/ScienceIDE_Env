#!/usr/bin/env python3
"""Docker-only source/staging runner for the PLUTO cooling leaf.

This runner intentionally does not compile PLUTO or claim a numerical result:
the approved PR #304 inventory has no IMPLEMENT-NOW row.  It validates the
vendored source boundary in the container, materializes the trusted reference
row-status artifact, and self-tests that artifact against the same explicit
staged/blocked expectations.  Any future implemented row must replace this
staging contract with a reviewed CPU oracle and check-owned policy.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

ROW_STATUS_PATH = Path("/opt/pluto-cooling/row-status.json")
EXPECTED_ARCHIVE_SHA256 = "1ba5527b76d49fdd78ae24dbfbdad085ec83393748f1e618516a9d63bd945787"
EXPECTED_ARCHIVE_BYTES = 16336669
SCHEMA = "pluto-cooling-chemistry/staged-v1"

REQUIRED_SOURCE_PATHS = (
    "Src/Cooling/cooling.h",
    "Src/Cooling/cooling_source.c",
    "Src/Cooling/cooling_ode_solver.c",
    "Src/Cooling/Power_Law",
    "Src/Cooling/Tabulated",
    "Src/Cooling/SNEq",
    "Src/Cooling/MINEq",
    "Src/Cooling/H2_COOL",
    "Test_Problems/MHD/Jet/definitions_07.h",
    "Test_Problems/MHD/Jet/definitions_08.h",
    "Test_Problems/MHD/Jet/definitions_09.h",
    "Test_Problems/MHD/Jet/definitions_18.h",
    "Test_Problems/MHD/Jet/pluto_07.ini",
    "Test_Problems/MHD/Jet/pluto_08.ini",
    "Test_Problems/MHD/Jet/pluto_09.ini",
    "Test_Problems/MHD/Jet/pluto_18.ini",
)


def fail(message: str) -> "NoReturn":
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def load_rows() -> tuple[dict[str, Any], list[dict[str, Any]]]:
    try:
        document = json.loads(ROW_STATUS_PATH.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        fail(f"cannot read approved row inventory: {exc}")
    if not isinstance(document, dict) or document.get("schema") != "pluto-cooling-chemistry/approved-row-status-v1":
        fail("approved row inventory has an unexpected schema")
    rows = document.get("rows")
    implemented_now = document.get("implemented_now")
    if not isinstance(rows, list) or len(rows) != 16:
        fail("approved row inventory must contain exactly 16 rows")
    if implemented_now != []:
        fail("staged runner only accepts the approved zero-row IMPLEMENT-NOW inventory")
    ids: set[str] = set()
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            fail(f"row {index + 1} is not an object")
        row_id = row.get("id")
        if not isinstance(row_id, str) or not row_id or row_id in ids:
            fail(f"row {index + 1} has a missing or duplicate id")
        ids.add(row_id)
        if row.get("implemented") is not False:
            fail(f"row {row_id} is unexpectedly marked implemented")
        if row.get("expected_status") not in {"staged", "blocked"}:
            fail(f"row {row_id} has an unsupported expected status")
        if not isinstance(row.get("live_status"), str) or not isinstance(row.get("reason"), str):
            fail(f"row {row_id} is missing its live status or staging reason")
    return document, rows


def source_receipt(source: Path) -> dict[str, Any]:
    if not source.is_dir():
        fail(f"source root is not a directory: {source}")
    missing = [path for path in REQUIRED_SOURCE_PATHS if not (source / path).exists()]
    if missing:
        fail("vendored PLUTO source is incomplete: " + ", ".join(missing))
    krome = source / "Src" / "Cooling" / "KROME"
    if krome.exists():
        fail("unexpected KROME material is present; the approved archive explicitly lacks it")

    archive = source / ".source" / "pluto-4.4-patch4.tar.gz"
    receipt = source / ".source" / "archive.sha256"
    if not archive.is_file() or not receipt.is_file():
        fail("vendored archive or its provenance receipt is missing")
    archive_bytes = archive.stat().st_size
    if archive_bytes != EXPECTED_ARCHIVE_BYTES:
        fail(f"vendored archive byte count is {archive_bytes}, expected {EXPECTED_ARCHIVE_BYTES}")
    digest = hashlib.sha256()
    with archive.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    archive_sha256 = digest.hexdigest()
    if archive_sha256 != EXPECTED_ARCHIVE_SHA256:
        fail(f"vendored archive hash is {archive_sha256}, expected {EXPECTED_ARCHIVE_SHA256}")
    receipt_text = receipt.read_text(encoding="utf-8")
    if f"archive-sha256  {EXPECTED_ARCHIVE_SHA256}" not in receipt_text:
        fail("archive.sha256 does not carry the approved archive hash")
    return {
        "source_root": str(source),
        "required_paths": list(REQUIRED_SOURCE_PATHS),
        "required_paths_present": True,
        "krome_present": False,
        "archive_bytes": archive_bytes,
        "archive_sha256": archive_sha256,
        "archive_receipt_present": True,
    }


def write_json(path: Path, document: dict[str, Any]) -> None:
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except OSError as exc:
        fail(f"cannot write artifact {path}: {exc}")


def reference_document(source: dict[str, Any], inventory: dict[str, Any], rows: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "schema": SCHEMA,
        "mode": "reference",
        "status": "staged",
        "verdict": "approved-zero-row-staging",
        "numerical_grading": "not-run",
        "implemented_rows": [],
        "staged_rows": [row["id"] for row in rows if row["expected_status"] == "staged"],
        "blocked_rows": [row["id"] for row in rows if row["expected_status"] == "blocked"],
        "source": source,
        "inventory": inventory,
        "rows": rows,
    }


def verify_document(source: dict[str, Any], inventory: dict[str, Any], rows: list[dict[str, Any]], oracle_path: Path) -> dict[str, Any]:
    try:
        oracle = json.loads(oracle_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        fail(f"cannot read Docker-produced reference artifact: {exc}")
    if not isinstance(oracle, dict) or oracle.get("schema") != SCHEMA:
        fail("reference artifact has an unexpected schema")
    if oracle.get("mode") != "reference":
        fail("reference artifact was not produced by the reference container")
    if oracle.get("source") != source:
        fail("reference source receipt differs from the source mounted in the verifier container")
    if oracle.get("rows") != rows:
        fail("reference row inventory differs from the approved inventory")

    staged = [row["id"] for row in rows if row["expected_status"] == "staged"]
    blocked = [row["id"] for row in rows if row["expected_status"] == "blocked"]
    return {
        "schema": SCHEMA,
        "mode": "verifier",
        "status": "self-test-passed",
        "verdict": "expected-staging-state",
        "numerical_grading": "not-run",
        "reward": 0.0,
        "implemented_rows": [],
        "staged_rows": staged,
        "blocked_rows": blocked,
        "source": source,
        "inventory": inventory,
        "checks": {
            "approved_row_count": {"expected": 16, "observed": len(rows), "passed": True},
            "implemented_row_count": {"expected": 0, "observed": 0, "passed": True},
            "reference_matches_inventory": {"passed": True},
            "source_provenance": {"passed": True},
            "candidate_execution": {"status": "not-applicable-until-row-authorized", "passed": True},
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the PLUTO cooling staging contract inside Docker.")
    parser.add_argument("mode", choices=("reference", "verify"))
    parser.add_argument("--source", default="/task/code/pluto", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--oracle", type=Path)
    args = parser.parse_args()

    inventory, rows = load_rows()
    source = source_receipt(args.source)
    if args.mode == "reference":
        result = reference_document(source, inventory, rows)
    else:
        if args.oracle is None:
            fail("verify mode requires --oracle")
        result = verify_document(source, inventory, rows, args.oracle)
    write_json(args.output, result)
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
