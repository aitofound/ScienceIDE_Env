#!/usr/bin/env python3
"""Run all package-level C16 checks without an AMPS executable.

This wrapper is intentionally independent of the repository working directory.
It compiles the production runner, invokes its synthetic scientific self-test,
and checks the complete ROUTINE dry-run matrix in an isolated temporary folder.
The checks make packaging mistakes (missing files, stale reference tables,
unexpanded input tokens, or an incomplete command matrix) visible before an
expensive MPI validation run is submitted.
"""

from __future__ import print_function

import hashlib
import importlib.util
import json
import py_compile
import re
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path


def sha256(path):
    """Return a source-file checksum so dry-run immutability can be verified."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run_checked(command, cwd):
    """Run a child command and include captured output in any failure."""
    completed = subprocess.run(
        command, cwd=str(cwd), stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, universal_newlines=True)
    if completed.returncode != 0:
        raise RuntimeError(
            "command failed (%d): %s\n%s" %
            (completed.returncode, " ".join(command), completed.stdout))
    return completed.stdout


def main():
    """Compile, self-test, dry-run, and verify the distributable package."""
    test_dir = Path(__file__).resolve().parents[1]
    runner = test_dir / "run_C16.py"
    template = test_dir / "AMPS_PARAM_C16_gridless.in"
    termination_reference = test_dir / "reference_C16_termination_contract.csv"
    probe_reference = test_dir / "reference_C16_probe_expectations.csv"
    driver = test_dir / "data" / "ts05_driver_C16.txt"
    manifest = test_dir / "MANIFEST.sha256"

    required = (runner, template, termination_reference, probe_reference, driver,
                manifest)
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError("missing C16 package files: %s" % ", ".join(missing))

    # Verify the distributable file inventory before executing any code from it.
    # The manifest intentionally omits itself so it has no recursive checksum.
    for line_number, raw in enumerate(manifest.read_text().splitlines(), start=1):
        if not raw.strip():
            continue
        parts = raw.split(None, 1)
        if len(parts) != 2:
            raise RuntimeError("malformed MANIFEST.sha256 line %d" % line_number)
        expected_hash, relative_name = parts
        member = test_dir / relative_name.strip()
        if not member.is_file():
            raise RuntimeError("manifest member is missing: %s" % relative_name)
        if sha256(member) != expected_hash:
            raise RuntimeError("manifest checksum mismatch: %s" % relative_name)

    # py_compile catches syntax/import-time problems without polluting the source
    # directory: its bytecode is written into the temporary test workspace.
    with tempfile.TemporaryDirectory(prefix="c16_package_test_") as raw:
        temp_root = Path(raw)
        py_compile.compile(str(runner), cfile=str(temp_root / "run_C16.pyc"),
                           doraise=True)

        output = run_checked([sys.executable, str(runner), "--self-test"],
                             temp_root)
        if "C16 self-test: PASS" not in output:
            raise RuntimeError("production runner did not report self-test PASS")

        # Source references must remain immutable even when the runner renders
        # and copies default contracts into an output directory.
        source_hashes = {
            path.name: sha256(path)
            for path in (termination_reference, probe_reference)
        }
        workdir = temp_root / "routine_dryrun"
        run_checked([
            sys.executable, str(runner), "--profile", "ROUTINE",
            "--dry-run", "--amps", "./amps", "-np", "4", "-nt", "16",
            "--workdir", str(workdir),
        ], temp_root)

        result = json.loads((workdir / "C16_result.json").read_text())
        records = result.get("run_records", [])
        if len(records) != 9:
            raise RuntimeError("ROUTINE must render 9 runs, got %d" % len(records))
        matrix = Counter((row["case_type"], row["field_model"])
                         for row in records)
        expected = Counter({("baseline", "DIPOLE"): 3,
                            ("baseline", "T05"): 3,
                            ("probe", "DIPOLE"): 3})
        if matrix != expected:
            raise RuntimeError("unexpected ROUTINE matrix: %r" % dict(matrix))

        rendered = sorted(workdir.rglob("AMPS_PARAM_C16.in"))
        if len(rendered) != 9:
            raise RuntimeError("expected 9 rendered decks, got %d" % len(rendered))
        for path in rendered:
            text = path.read_text(errors="replace")
            if re.search(r"__[A-Z0-9_]+__", text):
                raise RuntimeError("unexpanded template token in %s" % path)
            if not re.search(r"^CUTOFF_SAMPLING\s+VERTICAL\s*$",
                             text, re.MULTILINE):
                raise RuntimeError("VERTICAL parser contract missing in %s" % path)

        if len(list(workdir.rglob("ts05_driver_C16.txt"))) != 3:
            raise RuntimeError("each of the three T05 baselines needs a local driver")
        for path in (termination_reference, probe_reference):
            if sha256(path) != source_hashes[path.name]:
                raise RuntimeError("dry run modified source reference %s" % path)

        # Populate the rendered ROUTINE matrix with small, production-shaped
        # synthetic Tecplot products.  This exercises the top-level --skip-run
        # analysis, CSV writers, final unresolved gate, and all adjacent-budget
        # comparisons rather than testing those functions only in isolation.
        module_spec = importlib.util.spec_from_file_location("c16_runner", runner)
        c16 = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(c16)
        baseline_rigidities = [0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0]
        baseline_directions = c16.expected_direction_keys(30.0, 30.0)
        for model in ("dipole", "t05"):
            for budget in (60, 300, 600):
                run_dir = workdir / "boris" / model / ("t%d" % budget)
                for point_id in range(5):
                    def baseline_code(index, selected_budget=budget,
                                      selected_point=point_id):
                        # A few short-budget timeouts resolve at 600 s; all
                        # pre-existing physical states remain unchanged.
                        if index % 17 == 0 and selected_budget < 600:
                            return 3
                        return 0 if (index + selected_point) % 2 == 0 else 1
                    c16._write_synthetic_access(
                        run_dir /
                        ("cutoff_gridless_dir_access_point_%04d.dat" % point_id),
                        baseline_directions, baseline_rigidities, baseline_code)
        probe_codes = {"time_limit": 3, "step_limit": 4,
                       "distance_limit": 5}
        probe_directions = c16.expected_direction_keys(90.0, 90.0)
        for probe_name, code in probe_codes.items():
            probe_file = (workdir / "boris" / "probes" / probe_name /
                          "cutoff_gridless_dir_access_point_0000.dat")
            c16._write_synthetic_access(
                probe_file, probe_directions, [1.0, 10.0],
                lambda index, selected_code=code: selected_code)

        output = run_checked([
            sys.executable, str(runner), "--profile", "ROUTINE",
            "--skip-run", "--workdir", str(workdir),
        ], temp_root)
        if "C16 PASS" not in output:
            raise RuntimeError("synthetic ROUTINE analysis did not report PASS")
        analyzed = json.loads((workdir / "C16_result.json").read_text())
        if not analyzed.get("passed") or len(analyzed.get("convergence", [])) != 4:
            raise RuntimeError("synthetic ROUTINE result/convergence is incomplete")
        required_outputs = (
            "C16_summary.csv", "C16_termination_counts.csv",
            "C16_sample_audit.csv", "C16_convergence.csv",
            "C16_sample_convergence.csv", "C16_probe_summary.csv",
            "C16_commands.json", "C16_result.json",
        )
        missing_outputs = [name for name in required_outputs
                           if not (workdir / name).is_file()]
        if missing_outputs:
            raise RuntimeError("analysis omitted outputs: %s" %
                               ", ".join(missing_outputs))

    print("C16 package self-tests: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print("C16 package self-tests: FAIL: %s" % exc, file=sys.stderr)
        sys.exit(1)
