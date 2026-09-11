#!/usr/bin/env python3
"""Validate frozen data, input decks, and vendored observation operators."""

from __future__ import annotations

import csv
import gzip
import json
import subprocess
import sys
from pathlib import Path
from typing import Dict, List

from study_common import load_config, read_driver, sha256
from run_global_cutoff_maps import (
    apply_profile_override, deep_merge, validate_effective_config,
    workload_estimate,
)


def count_csv(path: Path) -> int:
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8", newline="") as stream:
        content = (line for line in stream if not line.startswith("#"))
        return sum(1 for _ in csv.DictReader(content))


def required_columns(path: Path, names: set[str]) -> None:
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(line for line in stream if not line.startswith("#"))
        missing = names - set(reader.fieldnames or ())
    if missing:
        raise ValueError(f"{path}: missing columns {sorted(missing)}")


def validate_input(path: Path) -> None:
    """Check parser-critical directives and reject previously problematic keys."""
    text = path.read_text(encoding="utf-8")
    required = {
        "CALC_TARGET": "CUTOFF_RIGIDITY",
        "CUTOFF_SAMPLING": "VERTICAL",
        "CUTOFF_SEARCH_ALGORITHM": "RIGIDITY_LIST",
        "FIELD_MODEL": "T05",
        "OUTPUT_MODE": "SHELLS",
        "SHELL_GEOMETRY": "GEODETIC",
    }
    active: Dict[str, str] = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith(("!", "#")):
            continue
        parts = line.split(None, 1)
        if len(parts) == 2:
            active[parts[0]] = parts[1].strip()
    for key, value in required.items():
        if active.get(key) != value:
            raise ValueError(f"{path}: expected {key} {value}, found {active.get(key)!r}")
    forbidden = {
        "CUTOFF_UNRESOLVED_EXTENSION_PASSES",
        "CUTOFF_UNRESOLVED_EXTENSION_FACTOR",
    }
    present = forbidden.intersection(active)
    if present:
        raise ValueError(f"{path}: unsupported parser keyword(s): {sorted(present)}")


def run_check(command: List[str], cwd: Path) -> None:
    completed = subprocess.run(command, cwd=cwd, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if completed.returncode != 0:
        raise ValueError(f"command failed: {' '.join(command)}\n{completed.stdout}")
    print(completed.stdout.strip())


def validate_native_batched_shell_support(root: Path) -> None:
    """Verify that the installed Mode3D source matches the runner contract.

    BATCHED is a cross-component feature: Python emits ``SNAPSHOT_LIST`` with
    ``OUTPUT_MODE SHELLS``, while C++ must accept that combination and allocate
    the mesh before entering the epoch loop.  Checking both markers here turns
    an accidentally mixed old/new installation into an immediate validation
    error instead of a long run that produces no addressable products.
    """

    source_path = root.parents[1] / "3d" / "Mode3D.cpp"
    if not source_path.is_file():
        raise ValueError(f"native Mode3D source is missing: {source_path}")
    source = source_path.read_text(encoding="utf-8")
    required = (
        'if (outputMode=="SHELLS") return snap;',
        'if (outputMode=="SHELLS") return;',
    )
    missing = [marker for marker in required if marker not in source]
    if missing:
        raise ValueError(
            "Mode3D does not enable SNAPSHOT_LIST for SHELLS; missing marker(s): "
            + ", ".join(missing)
        )
    mesh_position = source.find("  amps_init_mesh();   // build")
    loop_position = source.find(
        "for (std::size_t iSnapshot=0; iSnapshot<snapshotEpochs.size();"
    )
    if mesh_position < 0 or loop_position < 0 or mesh_position >= loop_position:
        raise ValueError("Mode3D mesh allocation is not outside the snapshot loop")


def main() -> int:
    root, config = load_config()
    provenance = json.loads((root / "data" / "provenance.json").read_text())
    paths = {
        "pamela_table_s1.csv": root / config["data"]["pamela_reference"],
        "poes_metop_meped_boundaries.csv.gz": root / config["data"]["poes_reference"],
        "ts05_dec2006_5min.txt": root / config["data"]["driver"],
    }
    problems: List[str] = []
    for name, path in paths.items():
        expected = provenance[name]
        try:
            actual = sha256(path)
            if actual != expected["sha256"]:
                raise ValueError(f"digest {actual} != {expected['sha256']}")
            if name.endswith((".csv", ".csv.gz")):
                rows = count_csv(path)
            else:
                rows = len(read_driver(path))
            if rows != int(expected["rows"]):
                raise ValueError(f"row count {rows} != {expected['rows']}")
            print(f"PASS {name}: {rows} rows, sha256={actual}")
        except Exception as exc:
            problems.append(f"{name}: {exc}")

    try:
        required_columns(paths["pamela_table_s1.csv"], {
            "interval_midpoint_utc", "rigidity_geometric_center_gv",
            "pamela_cutoff_aacgm_deg", "sigma_plus_deg", "sigma_minus_deg",
        })
        required_columns(paths["poes_metop_meped_boundaries.csv.gz"], {
            "interval_midpoint_utc", "rigidity_gv", "channel", "hemisphere",
            "mlt_hour", "boundary_aacgm_lat_deg", "validation_role",
            "acceptance_eligible",
        })
    except Exception as exc:
        problems.append(str(exc))

    for altitude in (475, 850):
        try:
            validate_input(root / "inputs" / f"AMPS_PARAM_DEC2006_{altitude}km.in")
            print(f"PASS AMPS_PARAM_DEC2006_{altitude}km.in")
        except Exception as exc:
            problems.append(str(exc))
    try:
        multishell = root / "inputs" / "AMPS_PARAM_DEC2006_multishell.in"
        validate_input(multishell)
        active = {}
        for raw in multishell.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if line and not line.startswith(("!", "#")):
                parts = line.split(None, 1)
                if len(parts) == 2:
                    active[parts[0]] = parts[1].strip()
        if active.get("SHELL_COUNT") != "2":
            raise ValueError("multi-shell template must declare SHELL_COUNT 2")
        if [float(value) for value in active.get("SHELL_ALTS_KM", "").split()] \
                != [475.0, 850.0]:
            raise ValueError("multi-shell template must declare 475 and 850 km")
        if "TEMPORAL_MODE" in active or "SNAPSHOT_LIST_FILE" in active:
            raise ValueError(
                "checked-in multi-shell template must remain single-snapshot; "
                "the runner inserts temporal batching only in generated decks"
            )
        print("PASS AMPS_PARAM_DEC2006_multishell.in")
    except Exception as exc:
        problems.append(str(exc))

    try:
        validate_native_batched_shell_support(root)
        print("PASS native Mode3D SNAPSHOT_LIST+SHELLS mesh-reuse contract")
    except Exception as exc:
        problems.append(str(exc))

    # The global map file is intentionally a small overlay, not a second copy
    # of the event configuration. Validate the merged configuration here so a
    # narrowed latitude band or inadequate rigidity bracket cannot reach a
    # multi-hour production launch.
    try:
        overlay = json.loads(
            (root / "config" / "global_cutoff_maps.json").read_text(
                encoding="utf-8"
            )
        )
        merged = deep_merge(json.loads(json.dumps(config)), overlay)
        for profile in ("SMOKE", "ROUTINE", "FULL"):
            effective = apply_profile_override(
                json.loads(json.dumps(merged)), profile
            )
            validate_effective_config(effective)
            if int(effective["execution"]["epochs_per_batch"]) < 2:
                raise ValueError(
                    f"global-map {profile} epochs_per_batch must be at least two"
                )
        smoke_work = workload_estimate(
            apply_profile_override(json.loads(json.dumps(merged)), "SMOKE"),
            "SMOKE",
        )
        if smoke_work["tasks_total"] != 15504:
            raise ValueError(
                f"global-map SMOKE workload drifted to {smoke_work['tasks_total']} tasks"
            )
        print("PASS global cutoff-map profile grids and mesh-reuse configuration")
    except Exception as exc:
        problems.append(f"global cutoff-map configuration: {exc}")

    # Exercise the original validators as a defense against a study-level check
    # accidentally becoming less strict than C9 or C10.
    try:
        run_check([sys.executable, "run_C9.py", "--validate-references"], root / "vendor" / "C9")
        run_check([sys.executable, "run_C9.py", "--validate-driver"], root / "vendor" / "C9")
        run_check([sys.executable, "run_C10.py", "--validate-references"], root / "vendor" / "C10")
        run_check([sys.executable, "run_C10.py", "--validate-driver"], root / "vendor" / "C10")
        run_check([sys.executable, "run_C10.py", "--self-test"], root / "vendor" / "C10")
    except Exception as exc:
        problems.append(str(exc))

    if problems:
        print("PACKAGE VALIDATION FAILED", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1
    print("PACKAGE VALIDATION PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
