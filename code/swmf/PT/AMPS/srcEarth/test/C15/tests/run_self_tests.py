#!/usr/bin/env python3
"""Verify the complete distributable C15 package without an AMPS executable.

The wrapper compiles the production runner, executes its scientific self-test,
renders the full ROUTINE matrix, performs a synthetic end-to-end analysis, and
then proves that driver-equivalence, sensitivity, and configuration-drift
corruptions are rejected.  It intentionally runs from a temporary directory so
no repository-relative assumption can hide a packaging error.
"""

from __future__ import annotations

import hashlib
import importlib.util
import json
import py_compile
import re
import subprocess
import sys
import tempfile
from collections import Counter
from datetime import datetime
from pathlib import Path


def sha256(path: Path) -> str:
    """Return a streaming content checksum for immutability checks."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run(command, cwd: Path, expected: int = 0) -> str:
    """Execute a child command and expose complete output on an unexpected exit."""
    completed = subprocess.run(
        command, cwd=str(cwd), stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True)
    if completed.returncode != expected:
        raise RuntimeError(
            "command returned %d, expected %d: %s\n%s" %
            (completed.returncode, expected, " ".join(command), completed.stdout))
    return completed.stdout


def write_synthetic_product(module, run_directory: Path, field_value: float,
                            cutoff_value: float, field_epsilon: float = 0.0,
                            access_flip_count: int = 0) -> None:
    """Create production-shaped field and access products expected by C15.

    ``field_epsilon`` places a tiny residual in a component whose counterpart is
    zero, reproducing the near-zero relative-error singularity seen in a real
    midpoint run. ``access_flip_count`` changes a controlled number of boundary
    classifications while keeping the state/allowed/unresolved contract valid.
    """
    module._write_synthetic_numeric(
        run_directory / "amps_3d_initialized_000000.data.dat",
        [(0.0, 0.0, 0.0, field_value, -0.5 * field_value, field_epsilon),
         (1.0, 0.0, 2.0, 2.0 * field_value, -field_value, 0.0)],
        ("x", "y", "z", "Bx", "By", "Bz"))
    transition_index = max(1, min(99, int(round(35.0 + 10.0 * cutoff_value))))
    access_rows = []
    for index in range(100):
        state = 0 if index < transition_index else 1
        if transition_index - access_flip_count <= index < transition_index:
            state = 1
        access_rows.append((
            float(index % 20) * 18.0,
            -60.0 + float(index // 20) * 30.0,
            1.0,
            state,
            state,
            0,
        ))
    module._write_synthetic_numeric(
        run_directory / "cutoff_3d_shells_access.dat",
        access_rows,
        ("lon_deg", "lat_deg", "rigidity_GV", "access_state",
         "allowed", "unresolved"))


def populate_synthetic_matrix(module, records) -> None:
    """Populate a smooth, reproducible matrix that satisfies every C15 gate."""
    first_epoch = datetime.fromisoformat("2012-05-17T05:55:00")
    for record in records:
        run_directory = Path(record["run_directory"])
        epoch = datetime.fromisoformat(record["epoch_utc"])
        minutes = (epoch - first_epoch).total_seconds() / 60.0
        category = record["category"]
        if category == "dipole_control":
            field_value, cutoff_value = 0.7, 1.0
        elif category == "driver_sensitivity":
            field_value, cutoff_value = 3.0, 2.0
        elif category in ("repeat", "scheduler"):
            field_value, cutoff_value = 1.5, 1.5
        else:
            # Full/reference cases at a common epoch are identical.  Across
            # epochs the field changes linearly, giving exact continuity.  A
            # reference midpoint carries a tiny zero-component residual and one
            # boundary-state flip: the revised scientific midpoint gates accept
            # these, while the obsolete all-number comparison would not.
            field_value = 1.0 + 0.1 * minutes
            cutoff_value = 1.0 + 0.1 * minutes
        is_reference_midpoint = (
            category == "driver_reference" and epoch.second == 30)
        write_synthetic_product(
            module, run_directory, field_value, cutoff_value,
            field_epsilon=1.0e-12 if is_reference_midpoint else 0.0,
            access_flip_count=1 if is_reference_midpoint else 0)


def main() -> int:
    """Run package integrity, positive-path, and negative-path checks."""
    test_dir = Path(__file__).resolve().parents[1]
    runner = test_dir / "run_C15.py"
    template = test_dir / "AMPS_PARAM_C15_mode3d.in"
    driver = test_dir / "data" / "ts05_driver_C15.txt"
    interpolation_reference = test_dir / "reference_C15_driver_interpolation.csv"
    acceptance_reference = test_dir / "reference_C15_acceptance_contract.csv"
    manifest = test_dir / "MANIFEST.sha256"
    required = (runner, template, driver, interpolation_reference,
                acceptance_reference, manifest)
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError("missing C15 package files: %s" % ", ".join(missing))
    source_header = next(
        (line.strip() for line in driver.read_text().splitlines()
         if "YYYY-MM-DDTHH:MM:SS" in line), "")
    if " Temp DST IMFflag " not in (source_header + " "):
        raise RuntimeError("source driver does not expose mandatory DST column")
    if "SYM-H" in source_header or "SYM_H" in source_header:
        raise RuntimeError("source driver still uses unsupported SYM-H alias")
    # MANIFEST omits itself to avoid a recursive checksum.  Validate every
    # declared member before importing or executing the production runner.
    for line_number, raw in enumerate(manifest.read_text().splitlines(), start=1):
        if not raw.strip():
            continue
        parts = raw.split(None, 1)
        if len(parts) != 2:
            raise RuntimeError("malformed MANIFEST.sha256 line %d" % line_number)
        expected_hash, relative_name = parts
        member = test_dir / relative_name.strip()
        if not member.is_file() or sha256(member) != expected_hash:
            raise RuntimeError("manifest mismatch: %s" % relative_name)
    source_hashes = {path.name: sha256(path) for path in required}

    with tempfile.TemporaryDirectory(prefix="c15_package_test_") as raw:
        root = Path(raw)
        py_compile.compile(str(runner), cfile=str(root / "run_C15.pyc"),
                           doraise=True)
        output = run([sys.executable, str(runner), "--self-test"], root)
        if "C15 self-test: PASS" not in output:
            raise RuntimeError("production runner did not report self-test PASS")

        output_root = root / "routine"
        run([
            sys.executable, str(runner), "--profile", "ROUTINE", "--dry-run",
            "--amps", "./amps", "-np", "4", "-nt", "16",
            "--output-root", str(output_root),
        ], root)
        result = json.loads((output_root / "C15_result.json").read_text())
        records = result["records"]
        if len(records) != 15:
            raise RuntimeError("ROUTINE must render 15 cases, got %d" % len(records))
        categories = Counter(record["category"] for record in records)
        expected = Counter({
            "driver_full": 5, "driver_reference": 5, "repeat": 1,
            "scheduler": 1, "dipole_control": 2, "driver_sensitivity": 1,
        })
        if categories != expected:
            raise RuntimeError("unexpected ROUTINE matrix: %r" % dict(categories))
        rendered = list(output_root.rglob("AMPS_PARAM_C15.in"))
        if len(rendered) != 15:
            raise RuntimeError("expected 15 rendered inputs, got %d" % len(rendered))
        for path in rendered:
            text = path.read_text(errors="replace")
            if re.search(r"__[A-Z0-9_]+__", text):
                raise RuntimeError("unexpanded template token in %s" % path)
            if not re.search(r"^CUTOFF_SAMPLING\s+VERTICAL\s*$",
                             text, re.MULTILINE):
                raise RuntimeError("missing VERTICAL rigidity-list contract")
        if len(list(output_root.rglob("ts05_driver_C15_case.txt"))) != 13:
            raise RuntimeError("every T05 case must archive a local driver")
        # All copied and generated drivers must expose the exact column name
        # required by AMPS.  This regression guard reproduces the parser-facing
        # boundary that previously let SYM-H/SYM_H reach an expensive run and
        # fail only when AMPS reported a missing DST column.
        for path in output_root.rglob("ts05_driver_C15_case.txt"):
            header = next(
                (line.strip() for line in path.read_text().splitlines()
                 if "YYYY-MM-DDTHH:MM:SS" in line), "")
            if " Temp DST IMFflag " not in (header + " "):
                raise RuntimeError("AMPS-compatible DST column missing in %s" % path)
            if "SYM-H" in header or "SYM_H" in header:
                raise RuntimeError("unsupported SYM-H alias survived in %s" % path)

        module_spec = importlib.util.spec_from_file_location("c15_runner", runner)
        module = importlib.util.module_from_spec(module_spec)
        # dataclasses resolves postponed annotations through sys.modules while
        # the module is executing; register the synthetic import explicitly.
        sys.modules[module_spec.name] = module
        module_spec.loader.exec_module(module)
        populate_synthetic_matrix(module, records)
        reanalyze = [
            sys.executable, str(runner), "--profile", "ROUTINE", "--skip-run",
            "--amps", "./amps", "-np", "4", "-nt", "16",
            "--output-root", str(output_root),
        ]
        output = run(reanalyze, root)
        if "result: PASS" not in output:
            raise RuntimeError("synthetic ROUTINE analysis did not pass")
        analyzed = json.loads((output_root / "C15_result.json").read_text())
        if not analyzed["passed"]:
            raise RuntimeError("synthetic result JSON did not record PASS")
        required_outputs = (
            "C15_driver_equivalence.csv", "C15_reproducibility.csv",
            "C15_driver_sensitivity.csv", "C15_continuity.csv",
            "C15_run_fingerprints.csv", "C15_commands.json",
            "C15_result.json", "C15_summary.txt",
        )
        if any(not (output_root / name).is_file() for name in required_outputs):
            raise RuntimeError("synthetic analysis omitted a documented result file")

        # Corrupt one independently materialized field.  The full/reference pair
        # must fail even though every file still has a valid shape.
        reference_dir = output_root / "t05" / "20120517T055730" / "reference"
        write_synthetic_product(module, reference_dir, 9.0, 1.25)
        output = run(reanalyze, root, expected=1)
        if "driver equivalence failed" not in output:
            if "midpoint field equivalence failed" not in output:
                raise RuntimeError("driver-equivalence corruption was not identified")
        populate_synthetic_matrix(module, records)

        # Change twenty percent of the midpoint access classifications while
        # leaving its field identical.  Boundary tolerance is deliberately
        # finite, but it must not turn a materially different access map into a
        # pass.
        write_synthetic_product(
            module, reference_dir, 1.25, 1.25,
            field_epsilon=1.0e-12, access_flip_count=20)
        output = run(reanalyze, root, expected=1)
        if "midpoint access equivalence failed" not in output:
            raise RuntimeError("gross midpoint access corruption was not identified")
        populate_synthetic_matrix(module, records)

        # Make the perturbed case identical to the anchor.  This simulates an
        # executable that ignores DRIVER_FILE and must fail the sensitivity gate.
        sensitivity_dir = output_root / "checks" / "driver_perturbed"
        write_synthetic_product(module, sensitivity_dir, 1.5, 1.5)
        output = run(reanalyze, root, expected=1)
        if "did not materially change" not in output:
            raise RuntimeError("ignored-driver corruption was not identified")
        populate_synthetic_matrix(module, records)

        # A mismatched worker count defines a different run matrix.  Reanalysis
        # must stop before silently attaching those settings to existing output.
        mismatched = list(reanalyze)
        mismatched[mismatched.index("16")] = "8"
        output = run(mismatched, root, expected=2)
        if "configuration does not match" not in output:
            raise RuntimeError("--skip-run configuration drift was not rejected")

    for path in required:
        if sha256(path) != source_hashes[path.name]:
            raise RuntimeError("package test modified source file %s" % path)
    print("C15 package self-tests: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print("C15 package self-tests: FAIL: %s" % exc, file=sys.stderr)
        sys.exit(1)
