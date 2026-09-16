#!/usr/bin/env python3
"""
regression_tools.py

Small, dependency-free helper used by tests/run_layer4_tests.sh.

The Layer-4 tests are regression tests rather than first-principles physics
validation tests.  They answer the question: "Did an already accepted set of
shieldSim results change after a code modification?"  The helper therefore
performs two tasks:

  collect  - read the machine-readable run summaries and selected output files
             from a set of deterministic shieldSim runs and write a compact
             JSON baseline/actual-results file.

  compare  - compare an actual-results JSON file against a previously accepted
             baseline JSON file using configurable numerical tolerances.

Why compare a JSON summary instead of the raw Tecplot files?
-----------------------------------------------------------
The normal shieldSim outputs are designed for plotting and post-processing by
users.  They contain long floating-point tables and may acquire additional
columns over time.  Regression tests should be less brittle: they should compare
important integrated quantities such as transmitted-particle counts, TID, DDD,
n_eq, H100/10, and compact LET-spectrum moments.  The run-summary diagnostic
written by the C++ code provides stable keyword rows for those quantities.

Monte Carlo caveat
------------------
Even with a fixed random seed, results can change when Geant4 versions, physics
lists, production cuts, compiler settings, or threading settings change.  Such
changes may be physically acceptable, but they should be reviewed deliberately.
For that reason the comparison tolerances are user-controlled from the shell
script.  The defaults are intended for software-regression screening, not for
claiming physics validation.
"""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import re
import sys
from typing import Any, Dict, List, Tuple


TARGET_FIELDS = [
    "index",
    "name",
    "thickness_mm",
    "TID_Gy_perPrimary",
    "TIDRate_Gy_s",
    "DDD_MeV_g_perPrimary",
    "DDDRate_MeV_g_s",
    "n_eq_cm2_perPrimary",
    "n_eq_rate_cm2_s",
]


def _to_float(text: str) -> float:
    """Convert a token to float and fail with a clear error if it is invalid."""
    try:
        x = float(text)
    except ValueError as exc:
        raise ValueError(f"cannot convert {text!r} to float") from exc
    if not math.isfinite(x):
        raise ValueError(f"non-finite value {text!r}")
    return x


def parse_run_summary(path: pathlib.Path) -> Dict[str, Any]:
    """Parse a shieldSim --dump-run-summary file.

    The summary file can contain one block for a single run or several blocks
    for sweep mode.  Each block is returned as a dictionary with metadata,
    scalar values, counts, and target rows.
    """
    if not path.exists() or path.stat().st_size == 0:
        raise FileNotFoundError(f"missing or empty run summary: {path}")

    runs: List[Dict[str, Any]] = []
    current: Dict[str, Any] | None = None

    for iline, raw in enumerate(path.read_text(errors="replace").splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        tag = fields[0]

        if tag == "begin_run":
            if current is not None:
                raise ValueError(f"nested begin_run in {path}:{iline}")
            current = {"meta": {}, "scalars": {}, "counts": {}, "targets": []}
            continue

        if tag == "end_run":
            if current is None:
                raise ValueError(f"end_run without begin_run in {path}:{iline}")
            runs.append(current)
            current = None
            continue

        if current is None:
            raise ValueError(f"row outside begin/end block in {path}:{iline}: {line}")

        if tag == "meta":
            if len(fields) < 3:
                raise ValueError(f"malformed meta row in {path}:{iline}: {line}")
            # Preserve any spaces after the key, although current C++ rows are
            # whitespace-free.  This makes the parser tolerant of future notes.
            current["meta"][fields[1]] = " ".join(fields[2:])
        elif tag == "scalar":
            if len(fields) != 3:
                raise ValueError(f"malformed scalar row in {path}:{iline}: {line}")
            current["scalars"][fields[1]] = _to_float(fields[2])
        elif tag == "count":
            if len(fields) != 3:
                raise ValueError(f"malformed count row in {path}:{iline}: {line}")
            current["counts"][fields[1]] = _to_float(fields[2])
        elif tag == "target":
            if len(fields) != len(TARGET_FIELDS) + 1:
                raise ValueError(f"malformed target row in {path}:{iline}: {line}")
            row: Dict[str, Any] = {
                "index": int(fields[1]),
                "name": fields[2],
            }
            for name, token in zip(TARGET_FIELDS[2:], fields[3:]):
                row[name] = _to_float(token)
            current["targets"].append(row)
        else:
            raise ValueError(f"unknown summary row type {tag!r} in {path}:{iline}")

    if current is not None:
        raise ValueError(f"summary ended before end_run in {path}")
    if not runs:
        raise ValueError(f"no run blocks in {path}")

    # Do not store the absolute/relative file path in the regression JSON.
    # Users may choose a different RUN_DIR between baseline and comparison runs,
    # and that should not by itself trigger a regression failure.
    return {"runs": runs}


def parse_let_moments(path: pathlib.Path) -> Dict[str, float]:
    """Return compact moments of the charged LET spectrum.

    The LET output table uses the first column as the LET-bin center and the
    fourth column as the total charged-particle differential distribution.  We
    compute an unnormalized integral proxy and a mean LET.  These moments are
    intended only for regression screening; they are not a substitute for a full
    binned spectrum comparison.
    """
    if not path.exists() or path.stat().st_size == 0:
        return {"present": 0.0, "charged_integral": 0.0, "charged_mean": 0.0}

    integral = 0.0
    moment = 0.0
    rows = 0
    for raw in path.read_text(errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("TITLE") or line.startswith("VARIABLES") or line.startswith("ZONE"):
            continue
        fields = line.split()
        if len(fields) < 4:
            continue
        try:
            let_center = float(fields[0])
            charged = float(fields[3])
        except ValueError:
            continue
        if math.isfinite(let_center) and math.isfinite(charged) and charged >= 0.0:
            integral += charged
            moment += let_center * charged
            rows += 1

    mean = moment / integral if integral > 0.0 else 0.0
    return {"present": 1.0, "rows": float(rows), "charged_integral": integral, "charged_mean": mean}


def parse_numeric_table_signature(path: pathlib.Path, max_rows: int = 200000) -> Dict[str, float]:
    """Compute a small numeric signature for a whitespace numeric table.

    This is deliberately generic and conservative.  It does not assume a fixed
    Tecplot column order.  It simply counts numeric rows and accumulates a sum
    over all finite numbers.  The signature is useful for catching accidentally
    empty or wildly changed output files, while detailed comparisons remain
    anchored to the run-summary scalars.
    """
    if not path.exists() or path.stat().st_size == 0:
        return {"present": 0.0, "numeric_rows": 0.0, "numeric_sum": 0.0}

    rows = 0
    total = 0.0
    for raw in path.read_text(errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("TITLE") or line.startswith("VARIABLES") or line.startswith("ZONE"):
            continue
        fields = line.split()
        values = []
        for token in fields:
            # Skip obvious non-numeric tokens but keep normal Fortran/C++ style
            # floating-point values, including scientific notation.
            if not re.search(r"[0-9]", token):
                continue
            try:
                x = float(token)
            except ValueError:
                continue
            if math.isfinite(x):
                values.append(x)
        if values:
            rows += 1
            total += sum(values)
        if rows >= max_rows:
            break
    return {"present": 1.0, "numeric_rows": float(rows), "numeric_sum": total}


def read_manifest(path: pathlib.Path) -> List[Tuple[str, pathlib.Path, pathlib.Path, pathlib.Path, pathlib.Path]]:
    """Read the tab-separated manifest written by run_layer4_tests.sh."""
    rows = []
    for iline, raw in enumerate(path.read_text().splitlines(), start=1):
        if not raw.strip() or raw.startswith("#"):
            continue
        fields = raw.split("\t")
        if len(fields) != 5:
            raise ValueError(f"manifest row {iline} must have 5 tab-separated fields: {raw!r}")
        name, summary, let_file, quantities, spectra = fields
        rows.append((name, pathlib.Path(summary), pathlib.Path(let_file), pathlib.Path(quantities), pathlib.Path(spectra)))
    if not rows:
        raise ValueError(f"manifest contains no cases: {path}")
    return rows


def collect(args: argparse.Namespace) -> None:
    """Collect actual or baseline regression data from a manifest."""
    manifest = pathlib.Path(args.manifest)
    result: Dict[str, Any] = {
        "schema": "shieldSim_layer4_regression_v1",
        "notes": [
            "Generated by tests/regression_tools.py collect.",
            "Values are regression signatures, not independent physics validation references.",
        ],
        "cases": {},
    }

    for name, summary, let_file, quantities, spectra in read_manifest(manifest):
        case = parse_run_summary(summary)
        # Normalize path-like metadata so a baseline created in one checkout can
        # still be used when the project directory is moved.  The spectrum file
        # basename is sufficient to distinguish the deterministic Layer-4 inputs
        # used by run_layer4_tests.sh; the actual physics values are captured in
        # source_norm and the downstream outputs.
        for run in case.get("runs", []):
            meta = run.get("meta", {})
            spec = meta.get("spectrum_file")
            if spec and spec != "builtin":
                meta["spectrum_file"] = pathlib.Path(spec).name
        case["let_moments"] = parse_let_moments(let_file)
        case["quantities_signature"] = parse_numeric_table_signature(quantities)
        case["spectra_signature"] = parse_numeric_table_signature(spectra)
        result["cases"][name] = case

    out = pathlib.Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(f"wrote {out}")


def _is_number(x: Any) -> bool:
    return isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(float(x))


def _compare_values(path: str, expected: Any, actual: Any, rel_tol: float, abs_tol: float, diffs: List[str]) -> None:
    """Recursive tolerant comparison for JSON-compatible objects."""
    if _is_number(expected) and _is_number(actual):
        e = float(expected)
        a = float(actual)
        scale = max(abs(e), abs_tol)
        allowed = max(abs_tol, rel_tol * scale)
        if abs(a - e) > allowed:
            diffs.append(f"{path}: expected {e:.12e}, actual {a:.12e}, allowed ±{allowed:.3e}")
        return

    if isinstance(expected, str) or isinstance(actual, str):
        if expected != actual:
            diffs.append(f"{path}: expected {expected!r}, actual {actual!r}")
        return

    if isinstance(expected, list) and isinstance(actual, list):
        if len(expected) != len(actual):
            diffs.append(f"{path}: expected list length {len(expected)}, actual {len(actual)}")
            return
        for i, (e_item, a_item) in enumerate(zip(expected, actual)):
            _compare_values(f"{path}[{i}]", e_item, a_item, rel_tol, abs_tol, diffs)
        return

    if isinstance(expected, dict) and isinstance(actual, dict):
        e_keys = set(expected)
        a_keys = set(actual)
        for missing in sorted(e_keys - a_keys):
            diffs.append(f"{path}: missing key in actual: {missing}")
        for extra in sorted(a_keys - e_keys):
            # Extra keys are reported because regression baselines should be
            # reviewed deliberately when the output schema changes.
            diffs.append(f"{path}: extra key in actual: {extra}")
        for key in sorted(e_keys & a_keys):
            _compare_values(f"{path}.{key}", expected[key], actual[key], rel_tol, abs_tol, diffs)
        return

    if expected != actual:
        diffs.append(f"{path}: expected {expected!r}, actual {actual!r}")


def compare(args: argparse.Namespace) -> None:
    """Compare actual regression data against an accepted baseline."""
    baseline_path = pathlib.Path(args.baseline)
    actual_path = pathlib.Path(args.actual)
    if not baseline_path.exists():
        raise SystemExit(f"baseline does not exist: {baseline_path}\nRun tests/run_layer4_tests.sh --update-baseline after reviewing a trusted run.")
    if not actual_path.exists():
        raise SystemExit(f"actual results do not exist: {actual_path}")

    baseline = json.loads(baseline_path.read_text())
    actual = json.loads(actual_path.read_text())

    # The top-level notes field is explanatory text and is not part of the
    # numerical regression contract.  Ignore it so wording changes do not fail
    # otherwise identical baselines.
    baseline = dict(baseline)
    actual = dict(actual)
    baseline.pop("notes", None)
    actual.pop("notes", None)

    diffs: List[str] = []
    _compare_values("root", baseline, actual, args.rel_tol, args.abs_tol, diffs)

    if diffs:
        print("Regression comparison failed. Differences:")
        for d in diffs[: args.max_diffs]:
            print("  -", d)
        if len(diffs) > args.max_diffs:
            print(f"  ... {len(diffs) - args.max_diffs} additional differences omitted")
        raise SystemExit(1)

    print("Regression comparison passed")


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Collect and compare shieldSim Layer-4 regression summaries.")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_collect = sub.add_parser("collect", help="collect regression signatures from a manifest")
    p_collect.add_argument("--manifest", required=True, help="tab-separated manifest from run_layer4_tests.sh")
    p_collect.add_argument("--output", required=True, help="JSON file to write")
    p_collect.set_defaults(func=collect)

    p_compare = sub.add_parser("compare", help="compare actual signatures against a baseline")
    p_compare.add_argument("--baseline", required=True, help="accepted baseline JSON")
    p_compare.add_argument("--actual", required=True, help="actual JSON from this run")
    p_compare.add_argument("--rel-tol", type=float, default=0.15, help="relative tolerance for numeric values")
    p_compare.add_argument("--abs-tol", type=float, default=1.0e-30, help="absolute tolerance for numeric values")
    p_compare.add_argument("--max-diffs", type=int, default=40, help="maximum differences to print")
    p_compare.set_defaults(func=compare)

    args = parser.parse_args(argv)
    args.func(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
