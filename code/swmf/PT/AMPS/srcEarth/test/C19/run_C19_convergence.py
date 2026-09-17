#!/usr/bin/env python3
"""C19 trajectory-budget and mover convergence driver.

This script implements the staged evidence chain documented in README.md:

Phase 1A  -- reproduce the historical failure with drift recurrence DISABLED and
             sweep only MAX_TRACE_DISTANCE.  The result partitions the original
             unresolved EAST response into path-limited versus long-lived classes.
Phase 1B  -- with the path cap disabled, sweep MAX_TRACE_TIME to establish whether the
             *observable* and the unresolved response have reached a plateau.
Phase 3C  -- optionally repeat the converged case with the full-orbit drift recurrence
             enabled, showing where TIME/DISTANCE_LIMIT response weight moved after the
             physical resolver was introduced.
Numerical cross-checks -- optional dt and RK4/BORIS sweeps use the same one-epoch case.

The driver never changes multiple numerical controls in one sweep row.  That matters:
if a row changes both the path budget and the trap classifier, the source of an
improvement cannot be identified.  Every child run is an ordinary run_C19.py
invocation and therefore keeps its complete AMPS input deck, command log, direct-access
cube, comparison CSV, and C19_result.json for independent inspection.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Optional, Sequence

HERE = Path(__file__).resolve().parent
RUNNER = HERE / "run_C19.py"
DEFAULT_REFERENCE = HERE / "data" / "reference_C19_goes_epead_ew.csv.gz"
DEFAULT_MANIFEST = HERE / "event_C19_may2012.json"
DEFAULT_DRIVER = HERE / "data" / "ts05_driver_may2012.txt"


def parse_float_list(text: str) -> List[float]:
    values: List[float] = []
    for token in str(text).split(","):
        token = token.strip()
        if not token:
            continue
        value = float(token)
        if value not in values:
            values.append(value)
    if not values:
        raise argparse.ArgumentTypeError("expected at least one comma-separated number")
    return values


def parse_text_list(text: str) -> List[str]:
    values: List[str] = []
    for token in str(text).split(","):
        value = token.strip().upper()
        if value and value not in values:
            values.append(value)
    if not values:
        raise argparse.ArgumentTypeError("expected at least one comma-separated token")
    return values


def run(command: Sequence[str], cwd: Path, dry_run: bool) -> int:
    print("Running:", " ".join(shlex.quote(x) for x in command), flush=True)
    if dry_run:
        return 0
    return subprocess.call(list(command), cwd=str(cwd))


def read_one(path: Path, predicate=None) -> Mapping[str, str]:
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    if predicate is not None:
        rows = [row for row in rows if predicate(row)]
    if len(rows) != 1:
        raise RuntimeError("expected one row in %s, found %d" % (path, len(rows)))
    return rows[0]


def f(row: Mapping[str, str], key: str) -> Optional[float]:
    text = str(row.get(key, "")).strip()
    if not text:
        return None
    try:
        value = float(text)
    except ValueError:
        return None
    return value if math.isfinite(value) else None


def summarize_child(output: Path, label: str, control: str, value: object,
                    drift_enabled: bool, mover: str, dt_trace: float,
                    max_time: float, max_distance: float) -> Dict[str, object]:
    availability = output / "C19_aperture_availability.csv"
    comparison = output / "C19_comparison.csv"
    result_path = output / "C19_result.json"
    if not availability.exists() or not comparison.exists():
        raise RuntimeError("child C19 outputs are missing under %s" % output)

    east = read_one(availability, lambda row: row.get("aperture", "").upper() == "EAST")
    west = read_one(availability, lambda row: row.get("aperture", "").upper() == "WEST")
    comp = read_one(comparison)
    child_result = json.loads(result_path.read_text()) if result_path.exists() else {}

    return {
        "sweep_label": label,
        "control": control,
        "control_value": value,
        "drift_recurrence_enabled": drift_enabled,
        "mover": mover,
        "dt_trace_s": dt_trace,
        "max_trace_time_s": max_time,
        "max_trace_distance_re": max_distance,
        "east_unresolved_fraction": f(east, "unresolved_fraction"),
        "west_unresolved_fraction": f(west, "unresolved_fraction"),
        "east_distance_limit_fraction": f(east, "unresolved_distance_limit_fraction"),
        "east_time_limit_fraction": f(east, "unresolved_time_limit_fraction"),
        "east_step_limit_fraction": f(east, "unresolved_step_limit_fraction"),
        "east_outer_allowed_fraction": f(east, "response_outer_allowed_fraction"),
        "east_inner_forbidden_fraction": f(east, "response_inner_forbidden_fraction"),
        "east_magnetic_trapped_fraction": f(east, "response_magnetic_trapped_fraction"),
        "east_drift_trapped_fraction": f(east, "response_drift_trapped_fraction"),
        "east_response_time_limit_fraction": f(east, "response_time_limit_fraction"),
        "east_response_step_limit_fraction": f(east, "response_step_limit_fraction"),
        "east_response_distance_limit_fraction": f(east, "response_distance_limit_fraction"),
        "east_response_other_fraction": f(east, "response_other_termination_fraction"),
        "west_outer_allowed_fraction": f(west, "response_outer_allowed_fraction"),
        "west_drift_trapped_fraction": f(west, "response_drift_trapped_fraction"),
        "west_response_time_limit_fraction": f(west, "response_time_limit_fraction"),
        "west_response_step_limit_fraction": f(west, "response_step_limit_fraction"),
        "west_response_distance_limit_fraction": f(west, "response_distance_limit_fraction"),
        "west_response_other_fraction": f(west, "response_other_termination_fraction"),
        "unresolved_east_west_ratio": f(comp, "unresolved_east_west_ratio"),
        "unresolved_asymmetry_index": f(comp, "unresolved_asymmetry_index"),
        "direct_ratio": f(comp, "modeled_east_west_ratio"),
        "direct_log10_ratio": f(comp, "modeled_log10_east_west_ratio"),
        "direct_ratio_min": f(comp, "modeled_east_west_ratio_min"),
        "direct_ratio_max": f(comp, "modeled_east_west_ratio_max"),
        "direct_log10_bound_width": f(comp, "modeled_log10_east_west_bound_width"),
        "observed_ratio": f(comp, "observed_east_west_ratio"),
        "observed_inside_rigorous_bounds": comp.get("observed_inside_rigorous_bounds"),
        "status": comp.get("status"),
        "spectrum_provenance_status": comp.get("spectrum_provenance_status"),
        "child_passed": child_result.get("passed"),
        "child_output": str(output),
    }


def write_csv(path: Path, rows: Sequence[Mapping[str, object]]) -> None:
    if not rows:
        path.write_text("")
        return
    fields: List[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def make_plots(rows: Sequence[Mapping[str, object]], outdir: Path) -> List[str]:
    """Generate compact convergence figures if matplotlib is available."""
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 convergence plots skipped: %s" % exc, file=sys.stderr)
        return []

    outputs: List[str] = []
    for control in ("MAX_TRACE_DISTANCE_RE", "MAX_TRACE_TIME_S", "DT_TRACE_S"):
        subset = [row for row in rows if row["control"] == control]
        if not subset:
            continue
        x = [float(row["control_value"]) for row in subset]
        e = [row.get("east_unresolved_fraction") for row in subset]
        w = [row.get("west_unresolved_fraction") for row in subset]
        if all(v is None for v in e + w):
            continue
        fig, ax = plt.subplots(figsize=(7.0, 4.8))
        ax.plot(x, [float(v) if v is not None else float("nan") for v in e], marker="o", label="EAST")
        ax.plot(x, [float(v) if v is not None else float("nan") for v in w], marker="s", label="WEST")
        ax.axhline(0.05, linestyle="--", linewidth=1.0, label="0.05 acceptance gate")
        ax.set_xlabel(control)
        ax.set_ylabel("response-weighted unresolved fraction")
        ax.set_title("C19 Phase-1 trajectory-resolution convergence")
        ax.grid(True, alpha=0.3)
        ax.legend()
        fig.tight_layout()
        path = outdir / ("C19_%s_unresolved.png" % control.lower())
        fig.savefig(path, dpi=160)
        plt.close(fig)
        outputs.append(str(path))

        widths = [row.get("direct_log10_bound_width") for row in subset]
        if any(v is not None for v in widths):
            fig, ax = plt.subplots(figsize=(7.0, 4.8))
            ax.plot(x, [float(v) if v is not None else float("nan") for v in widths], marker="o")
            ax.axhline(0.30, linestyle="--", linewidth=1.0, label="0.30 dex reference guide (gate disabled by default)")
            ax.set_xlabel(control)
            ax.set_ylabel("rigorous log10(E/W) bound width [dex]")
            ax.set_title("C19 observable convergence")
            ax.grid(True, alpha=0.3)
            ax.legend()
            fig.tight_layout()
            path = outdir / ("C19_%s_ratio_bound_width.png" % control.lower())
            fig.savefig(path, dpi=160)
            plt.close(fig)
            outputs.append(str(path))
    return outputs


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="C19 Phase-1/2/3 trajectory convergence sweeps")
    parser.add_argument("--epoch", default="2012-05-17T06:00:00Z")
    parser.add_argument("--spacecraft", default="GOES13")
    parser.add_argument("--channel", default="P4")
    parser.add_argument("--solver", choices=("GRIDDED", "GRIDLESS"), default="GRIDDED")
    parser.add_argument("--model", default="T05")
    parser.add_argument("--distance-values", type=parse_float_list,
                        default=parse_float_list("400,1000,2500,8000,0"))
    parser.add_argument("--time-values", type=parse_float_list,
                        default=parse_float_list("60,120,300,600"))
    parser.add_argument("--dt-values", type=parse_float_list, default=[])
    parser.add_argument("--mover-values", type=parse_text_list, default=[])
    parser.add_argument("--run-distance-sweep", action="store_true", default=True)
    parser.add_argument("--no-distance-sweep", dest="run_distance_sweep", action="store_false")
    parser.add_argument("--run-time-sweep", action="store_true", default=True)
    parser.add_argument("--no-time-sweep", dest="run_time_sweep", action="store_false")
    parser.add_argument("--include-drift-recurrence-case", action="store_true", default=True)
    parser.add_argument("--no-drift-recurrence-case", dest="include_drift_recurrence_case", action="store_false")
    parser.add_argument("--reference", default=str(DEFAULT_REFERENCE))
    parser.add_argument("--event-manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--driver", default=str(DEFAULT_DRIVER))
    parser.add_argument("--detector-response")
    parser.add_argument("--amps", default="./amps")
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("-np", type=int, default=4)
    parser.add_argument("-nt", type=int, default=16)
    parser.add_argument("--output-root", default="test_output/C19_convergence")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    outdir = Path(args.output_root).expanduser().resolve()
    outdir.mkdir(parents=True, exist_ok=True)
    rows: List[Dict[str, object]] = []

    common = [
        sys.executable, str(RUNNER),
        "--profile", "FULL", "--solver", args.solver,
        "--models", args.model, "--spacecraft", args.spacecraft,
        "--channels", args.channel, "--start", args.epoch, "--end", args.epoch,
        "--cutoff-search", "DIRECT_ACCESS", "--gridded-batch", "OFF",
        "--reference", str(Path(args.reference).resolve()),
        "--event-manifest", str(Path(args.event_manifest).resolve()),
        "--driver", str(Path(args.driver).resolve()),
        "--amps", args.amps, "--mpirun", args.mpirun,
        "-np", str(args.np), "-nt", str(args.nt),
        "--mover", "RK4", "--dt-trace", "0.25",
        # This helper measures explicit 60/120/300/600-s (or user-selected) trace
        # budgets.  Disable the production unresolved-only extension here so, e.g., a
        # nominal 60-s convergence point cannot silently retrace unresolved samples to
        # 120/240 s and invalidate the x-axis.  The ordinary run_C19.py production
        # workflow still defaults to the staged extension.
        "--unresolved-extension-passes", "0",
    ]
    if args.detector_response:
        common += ["--detector-response", str(Path(args.detector_response).resolve())]

    def child(label: str, control: str, value: object, extra: Sequence[str],
              drift_enabled: bool, mover: str="RK4", dt: float=0.25,
              max_time: float=300.0, max_distance: float=0.0) -> None:
        child_dir = outdir / label
        cmd = common + ["--output-root", str(child_dir)] + list(extra)
        rc = run(cmd, HERE, args.dry_run)
        if rc != 0:
            raise RuntimeError("child run failed (%s) with exit status %d" % (label, rc))
        if args.dry_run:
            return
        rows.append(summarize_child(child_dir, label, control, value,
                                    drift_enabled, mover, dt, max_time, max_distance))

    # Phase 1A: the historical classifier only.  Vary ONE thing: path budget.
    if args.run_distance_sweep:
        for value in args.distance_values:
            label = "distance_%sRe_no_drift" % ("%g" % value).replace(".", "p")
            child(label, "MAX_TRACE_DISTANCE_RE", value,
                  ["--max-trace-time", "300", "--max-trace-distance-re", str(value),
                   "--no-trap-drift-detection"], False,
                  max_time=300.0, max_distance=float(value))

    # Phase 1B/2: remove the path cap, then vary only physical trace time.
    if args.run_time_sweep:
        for value in args.time_values:
            label = "time_%ss_no_drift" % ("%g" % value).replace(".", "p")
            child(label, "MAX_TRACE_TIME_S", value,
                  ["--max-trace-time", str(value), "--max-trace-distance-re", "0",
                   "--no-trap-drift-detection"], False,
                  max_time=float(value), max_distance=0.0)

    # Phase 3C: same nominal production budget, changing only the positive physical
    # drift-recurrence resolver.  This makes the before/after termination transfer
    # directly visible in the output table.
    if args.include_drift_recurrence_case:
        child("phase3_drift_recurrence", "DRIFT_RECURRENCE", 1,
              ["--max-trace-time", "300", "--max-trace-distance-re", "0",
               "--trap-drift-detection"], True,
              max_time=300.0, max_distance=0.0)

    # Optional timestep convergence with production drift recurrence enabled.
    for value in args.dt_values:
        label = "dt_%ss_drift" % ("%g" % value).replace(".", "p")
        child(label, "DT_TRACE_S", value,
              ["--max-trace-time", "300", "--max-trace-distance-re", "0",
               "--trap-drift-detection", "--dt-trace", str(value)], True,
              dt=float(value), max_time=300.0, max_distance=0.0)

    # Optional mover cross-check.  RK4 is the C19 production/reference mover; BORIS is
    # particularly useful because a magnetic-only Boris step preserves |p| far better
    # over long traces and therefore tests whether the momentum gate is masking physics.
    for mover in args.mover_values:
        label = "mover_%s_drift" % mover.lower()
        child(label, "MOVER", mover,
              ["--max-trace-time", "300", "--max-trace-distance-re", "0",
               "--trap-drift-detection", "--mover", mover], True,
              mover=mover, max_time=300.0, max_distance=0.0)

    if args.dry_run:
        return 0

    table = outdir / "C19_trace_budget_sweep.csv"
    write_csv(table, rows)
    plot_files = make_plots(rows, outdir)
    summary = {
        "test_id": "C19A_CONVERGENCE",
        "epoch": args.epoch,
        "spacecraft": args.spacecraft,
        "channel": args.channel,
        "solver": args.solver,
        "field_model": args.model,
        "n_runs": len(rows),
        "table": str(table),
        "plot_files": plot_files,
        "interpretation": (
            "Distance-sweep rows use drift recurrence OFF and therefore diagnose the "
            "historical trace-budget failure without contaminating it with the new "
            "classifier. Time-sweep rows use MAX_TRACE_DISTANCE=0. The Phase-3 row "
            "changes only the positive full-orbit drift recurrence."),
    }
    (outdir / "C19_convergence_result.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print("Wrote", table)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
