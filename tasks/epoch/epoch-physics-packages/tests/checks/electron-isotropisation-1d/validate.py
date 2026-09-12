#!/usr/bin/env python3
"""Validate one canonical physical-time series and rubric-declared windows.

The rubric may retain convenience early/mid/tail files emitted by an extractor,
but they are not validator inputs.  Both runs must first provide one complete
``series.txt`` with the exact declared header, columns, row count, finite rows,
and dump-index/time grid.  Dump indices are exact physical output identities;
timestamps use a rubric-declared one-timestep envelope so harmless changes in
floating-point evaluation of EPOCH's timestep do not reject a correct port.
Only then are inclusive index windows sliced in memory from that canonical
table for the existing invariant statistics.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np


def load_table(path: Path, spec: dict) -> np.ndarray:
    fmt = spec.get("format", "text")
    if fmt == "text":
        data = np.loadtxt(path, comments=spec.get("comments", "#"),
                          skiprows=int(spec.get("skip_rows", 0)), ndmin=2)
        return np.asarray(data, dtype=np.float64)
    if fmt == "npy":
        data = np.asarray(np.load(path), dtype=np.float64)
        return np.atleast_2d(data)
    if fmt in ("f64", "f32"):
        dtype = np.float64 if fmt == "f64" else np.float32
        data = np.fromfile(path, dtype=dtype,
                           offset=int(spec.get("skip_header_bytes", 0)))
        return np.atleast_2d(data.astype(np.float64))
    raise ValueError(f"unknown format {fmt!r} for {path}")


def _check_text_header(path: Path, spec: dict) -> int:
    header = spec.get("header")
    if not isinstance(header, list) or not header or not all(isinstance(x, str) for x in header):
        raise ValueError("exact text header declaration is missing")
    lines = path.read_text(encoding="utf-8").splitlines()
    expected = "# " + "  ".join(header)
    if not lines or lines[0] != expected:
        raise ValueError("text header does not exactly match rubric")
    comments = str(spec.get("comments", "#"))
    if any(line.strip() and line.startswith(comments) for line in lines[1:]):
        raise ValueError("unexpected extra text header/comment")
    return len(header)


def expected_grid(file_spec: dict) -> np.ndarray:
    """Construct the complete declared physical grid, without sorting it."""
    if "values_s" in file_spec:
        values = file_spec["values_s"]
        expected = np.asarray(values, dtype=np.float64)
        if expected.ndim != 1:
            raise ValueError("values_s must be one-dimensional")
        return expected
    count = int(file_spec["count"])
    start = float(file_spec["start_s"])
    step = float(file_spec["step_s"])
    if count < 1:
        raise ValueError("count must be positive")
    return start + step * np.arange(count, dtype=np.float64)


def _grid_spec(file_spec: dict, default_tolerance: float) -> tuple[np.ndarray, int, float]:
    expected = expected_grid(file_spec)
    count = int(file_spec.get("count", expected.size))
    tolerance = float(file_spec.get("tolerance_s", default_tolerance))
    if count != expected.size:
        raise ValueError("expected values/count disagree")
    if not np.isfinite(tolerance) or tolerance <= 0.0:
        raise ValueError("tolerance_s must be finite and > 0")
    if expected.size == 0 or not np.all(np.isfinite(expected)):
        raise ValueError("expected grid must be finite and nonempty")
    if expected.size > 1 and not np.all(np.diff(expected) > 0.0):
        raise ValueError("expected grid must be strictly increasing")
    return expected, count, tolerance


def _matches(actual: np.ndarray, expected: np.ndarray, tolerance: float) -> bool:
    return bool(np.allclose(actual, expected, rtol=0.0, atol=tolerance, equal_nan=False))


def _validate_domains(declaration: dict, table: np.ndarray, label: str) -> list[str]:
    """Enforce explicitly declared physical domains on canonical columns."""
    header = declaration.get("header")
    domains = declaration.get("column_domains")
    if not isinstance(header, list):
        return ["time_grid.header declaration is missing"]
    if not isinstance(domains, dict) or not domains:
        return ["time_grid.column_domains declaration is missing or empty"]
    failures: list[str] = []
    for name, spec in domains.items():
        if name not in header:
            failures.append(f"time_grid column domain {name}: column is absent from header")
            continue
        if not isinstance(spec, dict) or not set(spec).issubset({"min", "max"}) or not spec:
            failures.append(f"time_grid column domain {name}: invalid declaration")
            continue
        values = table[:, header.index(name)]
        try:
            if "min" in spec:
                minimum = float(spec["min"])
                if not np.isfinite(minimum) or np.any(values < minimum):
                    failures.append(f"time_grid series.txt: {label}: {name} is below its physical minimum")
            if "max" in spec:
                maximum = float(spec["max"])
                if not np.isfinite(maximum) or np.any(values > maximum):
                    failures.append(f"time_grid series.txt: {label}: {name} exceeds its physical maximum")
        except (TypeError, ValueError, OverflowError) as exc:
            failures.append(f"time_grid column domain {name}: invalid bound: {exc}")
    return failures


def _validate_windows(declaration: dict, count: int) -> list[str]:
    failures: list[str] = []
    windows = declaration.get("windows")
    if not isinstance(windows, dict) or not windows:
        return ["time_grid.windows declaration is missing or empty"]
    series = windows.get("series")
    if not isinstance(series, dict):
        failures.append("time_grid.windows.series declaration is missing")
    for name, spec in windows.items():
        if not isinstance(spec, dict):
            failures.append(f"time_grid window {name}: declaration is not an object")
            continue
        try:
            start = int(spec["start_index"])
            end = int(spec["end_index"])
            window_count = int(spec["count"])
        except (KeyError, TypeError, ValueError, OverflowError) as exc:
            failures.append(f"time_grid window {name}: invalid declaration: {exc}")
            continue
        if start < 0 or end < start or end >= count or window_count != end - start + 1:
            failures.append(f"time_grid window {name}: indices are outside canonical series")
    if isinstance(series, dict) and (series.get("start_index") != 0
                                     or series.get("end_index") != count - 1
                                     or series.get("count") != count):
        failures.append("time_grid.windows.series must cover the complete canonical series")
    return failures


def validate_grids(rubric: dict, roots: tuple[Path, Path]) -> tuple[dict, list[str]]:
    declaration = rubric.get("time_grid")
    if not isinstance(declaration, dict):
        return {}, ["time_grid declaration is missing"]
    files = declaration.get("files")
    if not isinstance(files, dict) or set(files) != {"series.txt"}:
        return {}, ["time_grid.files must declare only canonical series.txt"]
    if declaration.get("canonical_series") != "series.txt":
        return {}, ["time_grid.canonical_series must be series.txt"]
    try:
        time_column = int(declaration.get("time_column", 0))
        dump_index_column = int(declaration["dump_index_column"])
        default_tolerance = float(declaration.get("tolerance_s", 0.0))
    except (KeyError, TypeError, ValueError, OverflowError) as exc:
        return {}, [f"invalid time_grid declaration: {exc}"]
    if time_column < 0 or dump_index_column < 0 or time_column == dump_index_column:
        return {}, ["time_grid time/dump-index columns must be distinct and nonnegative"]
    file_spec = files["series.txt"]
    failures: list[str] = []
    try:
        expected, count, tolerance = _grid_spec(file_spec, default_tolerance)
    except (KeyError, TypeError, ValueError, OverflowError) as exc:
        return {}, [f"time_grid series.txt: invalid expected grid: {exc}"]
    failures.extend(_validate_windows(declaration, count))
    tables: dict[str, dict[str, np.ndarray]] = {}
    grids: dict[str, dict[str, np.ndarray]] = {"series.txt": {}}
    for label, root in zip(("reference", "candidate"), roots):
        path = root / "series.txt"
        try:
            expected_columns = _check_text_header(path, {**file_spec, "header": declaration["header"]})
            table = load_table(path, file_spec)
        except (OSError, ValueError, TypeError, EOFError) as exc:
            failures.append(f"time_grid series.txt: {label}: cannot load: {exc}")
            continue
        tables.setdefault("series.txt", {})[label] = table
        if table.ndim != 2 or table.shape[1] != expected_columns:
            failures.append(f"time_grid series.txt: {label}: expected exactly {expected_columns} columns")
            continue
        if table.shape[1] <= time_column:
            failures.append(f"time_grid series.txt: {label}: no time column {time_column}")
            continue
        if not np.all(np.isfinite(table)):
            failures.append(f"time_grid series.txt: {label}: canonical series contains non-finite values")
            continue
        failures.extend(_validate_domains(declaration, table, label))
        actual = table[:, time_column]
        grids["series.txt"][label] = actual
        if actual.size != count:
            failures.append(f"time_grid series.txt: {label}: expected {count} rows, got {actual.size}")
            continue
        if table.shape[1] <= dump_index_column:
            failures.append(f"time_grid series.txt: {label}: no dump-index column {dump_index_column}")
            continue
        dump_indices = table[:, dump_index_column]
        if not np.array_equal(dump_indices, np.arange(count, dtype=np.float64)):
            failures.append(f"time_grid series.txt: {label}: dump indices are not the complete 0..{count - 1} sequence")
            continue
        if actual.size > 1 and not np.all(np.diff(actual) > 0.0):
            failures.append(f"time_grid series.txt: {label}: timestamps are not strictly increasing")
            continue
        if not _matches(actual, expected, tolerance):
            failures.append(f"time_grid series.txt: {label}: timestamps leave the declared one-timestep envelope")
    ref = grids["series.txt"].get("reference")
    cand = grids["series.txt"].get("candidate")
    if (ref is not None and cand is not None and ref.size == cand.size
            and not _matches(ref, cand, 2.0 * tolerance)):
        failures.append("time_grid series.txt: candidate/reference timestamps differ by more than two timestep envelopes")
    return tables, failures


def load_column(table: np.ndarray, spec: dict) -> np.ndarray:
    column = int(spec.get("column", 0))
    if table.ndim != 2 or table.shape[1] <= column:
        raise ValueError(f"column {column} is unavailable")
    return table[:, column].astype(np.float64)


STATS = {
    "final": lambda v: float(v[-1]),
    "mean": lambda v: float(v.mean()),
    "max": lambda v: float(v.max()),
    "min": lambda v: float(v.min()),
}


def main() -> int:
    ap = argparse.ArgumentParser()
    for flag in ("--reference", "--candidate", "--rubric", "--out"):
        ap.add_argument(flag, required=True)
    a = ap.parse_args()
    try:
        rubric = json.loads(Path(a.rubric).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"cannot load rubric: {exc}", file=sys.stderr)
        return 2
    reference, candidate = Path(a.reference), Path(a.candidate)
    tables, failures = validate_grids(rubric, (reference, candidate))
    details, distance, bound_fraction = {}, 0.0, 0.0
    # Grid and canonical-series failure is terminal: no statistic may be
    # computed from an unvalidated or independently supplied reduced table.
    if not failures:
        declaration = rubric["time_grid"]
        windows = declaration["windows"]
        comparison = rubric.get("comparison", {})
        trajectory_l1 = comparison.get("trajectory_l1", False)
        if not isinstance(trajectory_l1, bool):
            failures.append("comparison.trajectory_l1 must be boolean")
            trajectory_l1 = False
        for inv in comparison.get("invariants", []):
            name = inv["name"]
            if inv.get("file") != declaration["canonical_series"]:
                failures.append(f"{name}: invariant does not identify canonical series.txt")
                continue
            window_name = inv.get("window")
            window = windows.get(window_name) if isinstance(window_name, str) else None
            if not isinstance(window, dict):
                failures.append(f"{name}: canonical-series window is missing")
                continue
            try:
                start = int(window["start_index"])
                end = int(window["end_index"])
                window_count = int(window["count"])
            except (KeyError, TypeError, ValueError, OverflowError) as exc:
                failures.append(f"{name}: invalid canonical-series window: {exc}")
                continue
            if end - start + 1 != window_count:
                failures.append(f"{name}: canonical-series window count is inconsistent")
                continue
            series = {}
            try:
                for label in ("reference", "candidate"):
                    table = tables["series.txt"][label]
                    series[label] = load_column(table[start:end + 1], inv)
            except (KeyError, ValueError, TypeError, IndexError) as exc:
                failures.append(f"{name}: cannot load canonical-series window: {exc}")
                continue
            if any(values.size != window_count or values.size == 0
                   or not np.all(np.isfinite(values)) for values in series.values()):
                failures.append(f"{name}: non-finite, empty, or incomplete canonical-series window")
                continue
            mode = inv.get("mode", "agreement")
            if mode == "agreement":
                stat = inv.get("statistic", "final")
                if stat not in STATS:
                    failures.append(f"{name}: unknown statistic {stat!r}")
                    continue
                ref_v, cand_v = STATS[stat](series["reference"]), STATS[stat](series["candidate"])
                bound = float(inv.get("atol", 0.0)) + float(inv.get("rtol", 0.0)) * abs(ref_v)
                err = abs(cand_v - ref_v)
                distance = max(distance, err / abs(ref_v) if ref_v else err)
                frac = (err / bound) if bound > 0 else (0.0 if err == 0 else float("inf"))
                bound_fraction = max(bound_fraction, frac)
                details[name] = {"mode": mode, "statistic": stat, "reference": ref_v,
                                 "candidate": cand_v, "abs_error": err, "bound": bound,
                                 "bound_fraction": frac}
                if err > bound:
                    failures.append(f"{name}: |{cand_v:.6e} - {ref_v:.6e}| = {err:.3e} exceeds bound {bound:.3e}")
                if trajectory_l1:
                    trajectory_error = float(np.mean(np.abs(series["candidate"] - series["reference"])))
                    trajectory_scale = float(np.mean(np.abs(series["reference"])))
                    trajectory_bound = float(inv.get("atol", 0.0)) + float(inv.get("rtol", 0.0)) * trajectory_scale
                    trajectory_fraction = (trajectory_error / trajectory_bound) if trajectory_bound > 0 else (0.0 if trajectory_error == 0 else float("inf"))
                    distance = max(distance, trajectory_error / trajectory_scale if trajectory_scale else trajectory_error)
                    bound_fraction = max(bound_fraction, trajectory_fraction)
                    details[name].update({"trajectory_mean_abs_error": trajectory_error,
                                          "trajectory_reference_mean_abs": trajectory_scale,
                                          "trajectory_bound": trajectory_bound,
                                          "trajectory_bound_fraction": trajectory_fraction})
                    if trajectory_error > trajectory_bound:
                        failures.append(f"{name}: trajectory mean absolute error {trajectory_error:.3e} exceeds bound {trajectory_bound:.3e}")
            elif mode == "drift":
                limit = float(inv["max_relative_drift"])
                drifts = {}
                for label, values in series.items():
                    x0 = values[0]
                    drifts[label] = float(np.max(np.abs(values - x0)) / (abs(x0) if x0 != 0 else 1.0))
                frac = (max(drifts.values()) / limit) if limit > 0 else (0.0 if max(drifts.values()) == 0 else float("inf"))
                bound_fraction = max(bound_fraction, frac)
                details[name] = {"mode": mode, "max_relative_drift": drifts,
                                 "bound": limit, "bound_fraction": frac}
                for label, drift in drifts.items():
                    if drift > limit:
                        failures.append(f"{name}: {label} drifts {drift:.3e} relative, above {limit:.3e}")
            else:
                failures.append(f"{name}: unknown mode {mode!r}")
    passed = not failures
    result = {"passed": passed, "policy": "invariants", "distance": distance,
              "bound_fraction": bound_fraction, "invariants": details,
              "reason": "all invariants within bound" if passed else "; ".join(failures)}
    try:
        Path(a.out).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except OSError as exc:
        print(f"cannot write result: {exc}", file=sys.stderr)
        return 2
    print(result["reason"], file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
