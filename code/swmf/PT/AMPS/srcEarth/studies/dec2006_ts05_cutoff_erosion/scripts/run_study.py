#!/usr/bin/env python3
"""Top-level orchestration for the December 2006 cutoff study.

The default path first creates shared 475/850-km Mode3D products, then lets C9
and C10 apply their separate observation operators to those same raw states.
A failed observation comparison returns a nonzero final status but does not
suppress the comparison, dynamics, or figure diagnostics needed to understand
that failure. Use ``--continue-on-validation-failure`` only to proceed past a
non-observational pipeline failure; the manifest records that choice.
"""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Sequence

from study_common import default_output_root, load_config, resolve_output_path


# The shared morphology calculation now precedes observation reduction because
# its two-altitude products are staged for C9 and C10.  Those stages still run
# their own independent observation operators and acceptance gates; only their
# redundant AMPS launches are removed.
STAGE_ORDER = ("validate", "morphology", "pamela", "poes", "compare", "dynamics", "figures")
OBSERVATIONAL_VALIDATION_STAGES = ("pamela", "poes")


def independent_validation_remains(stage: str, remaining_stages: Sequence[str]) -> bool:
    """Return whether another independent observation validation is pending.

    PAMELA/C9 and POES/C10 use different measurements and observation
    operators.  Failure of one is scientifically important, but it does not
    invalidate executing the other.  This helper supports collecting both
    outcomes before deciding whether the production morphology may proceed.
    """

    return (
        stage in OBSERVATIONAL_VALIDATION_STAGES
        and any(item in OBSERVATIONAL_VALIDATION_STAGES for item in remaining_stages)
    )


def stage_output_problem(stage: str, output: Path,
                         shared_observations: bool) -> str | None:
    """Return a precise error when a successful stage omitted its contract.

    Process exit status alone is insufficient for a publication workflow: a
    solver can return zero but an output filename/parser mismatch can leave no
    scientific table.  These checks run immediately after the relevant child
    exits, while its command and log are still visible on screen.
    """

    if stage == "morphology":
        result_path = output / "morphology" / "morphology_result.json"
        boundary_path = output / "morphology" / "morphology_boundaries.csv"
        map_manifest_path = (
            output / "morphology" / "cutoff_rigidity_map_manifest.csv"
        )
        if not result_path.is_file():
            return f"missing morphology status: {result_path}"
        try:
            result = json.loads(result_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            return f"invalid morphology status {result_path}: {exc}"
        if not result.get("passed", False):
            return f"morphology_result.json reports failure: {result.get('failures', [])}"
        if not boundary_path.is_file() or boundary_path.stat().st_size == 0:
            return f"missing or empty morphology boundary table: {boundary_path}"
        if not map_manifest_path.is_file() or map_manifest_path.stat().st_size == 0:
            return f"missing or empty cutoff-rigidity map manifest: {map_manifest_path}"
        if shared_observations:
            staged_path = output / "morphology" / "staged_observation_products.json"
            try:
                staged = json.loads(staged_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError) as exc:
                return f"invalid staged observation product list {staged_path}: {exc}"
            if not staged:
                return f"no C9/C10 raw products were staged: {staged_path}"
    elif stage == "dynamics":
        required = (
            "cutoff_dynamics_timeseries.csv", "boundary_cell_dynamics.csv",
            "altitude_response.csv", "storm_extrema_summary.csv",
            "cutoff_map_change_summary.json",
            "analysis_availability.json", "dynamics_result.json",
        )
        missing = [
            output / "dynamics" / name for name in required
            if not (output / "dynamics" / name).is_file()
            or (output / "dynamics" / name).stat().st_size == 0
        ]
        if missing:
            return "missing or empty enhanced dynamics products: " + ", ".join(
                map(str, missing)
            )
        # Empty change CSVs are a valid censored outcome when no cell has both
        # a bracketed quiet reference and a usable event estimate. The summary
        # JSON above remains nonempty and records NOT_AVAILABLE; the CSV files
        # must still exist so the outcome is explicit rather than accidental.
        absent = [
            output / "dynamics" / name for name in (
                "cutoff_map_event_change.csv",
                "cutoff_map_change_timeseries.csv",
            ) if not (output / "dynamics" / name).is_file()
        ]
        if absent:
            return "missing cutoff-map change tables: " + ", ".join(map(str, absent))
    elif stage == "figures":
        missing = [
            output / "figures" / f"{stem}{suffix}"
            for stem in (
                "figure_cutoff_degradation", "figure_peak_cutoff_degradation",
                "figure_mlt_cutoff_evolution", "figure_altitude_response",
                "figure_accessible_area",
                "figure_maximum_cutoff_decrease_map",
                "figure_cutoff_decrease_evolution",
            )
            for suffix in (".png", ".eps")
            if not (output / "figures" / f"{stem}{suffix}").is_file()
        ]
        if missing:
            return "missing publication figures: " + ", ".join(map(str, missing))
        manifest_path = output / "figures" / "figure_manifest.json"
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            return f"invalid figure manifest {manifest_path}: {exc}"
        if not manifest.get("epoch_map_contract_passed", False):
            return "per-shell/per-epoch cutoff-map figure coverage is incomplete"
    return None


def execute(
    command: Sequence[str],
    cwd: Path,
    log_path: Path,
    dry_run: bool,
    *,
    stage: str = "command",
    stage_index: int = 1,
    stage_count: int = 1,
) -> int:
    """Execute one study stage while teeing its complete output to the terminal.

    The original orchestrator redirected the child process exclusively to its
    log file.  That was safe for archiving, but a long AMPS calculation then
    appeared completely idle.  The child now writes to a pipe which is copied
    immediately to both destinations.  ``PYTHONUNBUFFERED`` is set for Python
    stage runners so their own progress and AMPS pass-through output arrive as
    soon as they are produced rather than at process exit.

    The function intentionally returns the child's unmodified exit code.  This
    preserves the existing stop/continue policy and makes shell, MPI, and AMPS
    failures visible in both the manifest and the terminal summary.
    """

    command_text = " ".join(shlex.quote(token) for token in command)
    label = stage.upper()
    started = time.monotonic()
    print("=" * 78, flush=True)
    print(f"[{stage_index}/{stage_count}] START {label}", flush=True)
    print(f"[{label}] working directory: {cwd}", flush=True)
    print(f"[{label}] log: {log_path}", flush=True)
    print(f"[{label}] command: {command_text}", flush=True)
    if dry_run:
        print(f"[{stage_index}/{stage_count}] PREPARED {label} (not executed)", flush=True)
        return 0

    log_path.parent.mkdir(parents=True, exist_ok=True)
    child_environment = os.environ.copy()
    child_environment["PYTHONUNBUFFERED"] = "1"
    with log_path.open("w", encoding="utf-8", buffering=1) as log:
        log.write(f"Stage: {stage}\nWorking directory: {cwd}\nCommand: {command_text}\n\n")
        process = subprocess.Popen(
            list(command), cwd=cwd, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1,
            env=child_environment,
        )
        assert process.stdout is not None
        with process.stdout:
            for line in process.stdout:
                # Prefixing every line identifies the responsible stage when
                # AMPS, MPI, and postprocessing messages are interleaved in
                # batch output.
                sys.stdout.write(f"[{label}] {line}")
                sys.stdout.flush()
                log.write(line)
        return_code = process.wait()

    elapsed = time.monotonic() - started
    outcome = "PASS" if return_code == 0 else "FAIL"
    stream = sys.stdout if return_code == 0 else sys.stderr
    print(
        f"[{stage_index}/{stage_count}] {outcome} {label}: "
        f"exit={return_code}, elapsed={elapsed:.1f} s, log={log_path}",
        file=stream, flush=True,
    )
    return return_code


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", action="append", choices=STAGE_ORDER + ("all",),
                        help="May be repeated; default is all stages")
    parser.add_argument("--profile", choices=("SMOKE", "ROUTINE", "FULL"), default="SMOKE")
    parser.add_argument("--amps", type=Path, default=Path("./amps"))
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("-np", type=int)
    parser.add_argument("-nt", type=int)
    parser.add_argument("--prepare-only", action="store_true",
                        help="Validate data and generate commands/inputs without running AMPS")
    parser.add_argument(
        "--continue-on-validation-failure", action="store_true",
        help=("Continue after a non-observational pipeline failure. C9/C10 "
              "acceptance failures always retain FAIL status but do not suppress "
              "downstream diagnostic products."),
    )
    parser.add_argument(
        "--mesh-layout", choices=("BATCHED", "PER_EPOCH", "STANDALONE"),
        type=str.upper, default="BATCHED",
        help=("BATCHED reuses one native Mode3D mesh across up to N epochs and "
              "both shells; PER_EPOCH uses one epoch/two shells per process; "
              "STANDALONE is the one-epoch/one-altitude baseline"),
    )
    parser.add_argument(
        "--epochs-per-batch", type=int,
        help="Override execution.epochs_per_batch for each BATCHED AMPS process",
    )
    parser.add_argument(
        "--keep", action="store_true",
        help=("Reuse a morphology AMPS launch only when every precisely named "
              "raw epoch product for that launch already exists. Postprocessing, "
              "observation reductions, dynamics, and figures are still rerun."),
    )
    parser.add_argument(
        "--independent-observation-runs", action="store_true",
        help=("Run separate C9 and C10 AMPS calculations instead of reducing the "
              "shared 475/850-km morphology products"),
    )
    parser.add_argument(
        "--output-root", type=Path, default=default_output_root(),
        help=("Study output root (default: repository-level "
              "test_output/dec2006_ts05_cutoff_erosion)"),
    )
    return parser.parse_args()


def main() -> int:
    run_started = time.monotonic()
    args = parse_args()
    root, config = load_config()
    execution = config["execution"]
    np_value = args.np if args.np is not None else int(execution["mpi_ranks"])
    nt_value = args.nt if args.nt is not None else int(execution["threads_per_rank"])
    output = resolve_output_path(args.output_root)
    output.mkdir(parents=True, exist_ok=True)
    amps_path = args.amps.expanduser()
    if not amps_path.is_absolute():
        amps_path = (Path.cwd() / amps_path).resolve()
    stages = list(args.stage or ["all"])
    if "all" in stages:
        stages = list(STAGE_ORDER)
    stages = [stage for stage in STAGE_ORDER if stage in stages]

    # Fail before starting the validation pipeline when a requested modeling
    # stage cannot possibly launch.  This catches common relative-path errors
    # (for example ``--amps ../amps`` from the repository root) without first
    # creating dozens of case directories or relying on an opaque MPI status.
    amps_stages = {"morphology"}
    if args.independent_observation_runs:
        amps_stages.update(("pamela", "poes"))
    needs_amps = bool(amps_stages.intersection(stages)) and not args.prepare_only
    if needs_amps:
        if not amps_path.is_file():
            print(f"ERROR: AMPS executable does not exist: {amps_path}", file=sys.stderr)
            print("Pass --amps ./amps when launching from the AMPS repository root.",
                  file=sys.stderr)
            return 2
        if not os.access(amps_path, os.X_OK):
            print(f"ERROR: AMPS file is not executable: {amps_path}", file=sys.stderr)
            return 2
        if shutil.which(args.mpirun) is None:
            print(f"ERROR: MPI launcher is not available: {args.mpirun}", file=sys.stderr)
            return 2

    python = sys.executable
    commands: Dict[str, List[str]] = {
        "validate": [python, str(root / "scripts" / "validate_package.py")],
        "pamela": [
            python, "run_C9.py", "--profile", args.profile,
            "--solver", "GRIDDED", "--cutoff-evaluation", "DIRECT_ACCESS",
            "--comparison-observable", "PAMELA_T50", "--max-trace-time", "300",
            "--output-root", str(output / "C9"), "--amps", str(amps_path),
            "--mpirun", args.mpirun, "-np", str(np_value), "-nt", str(nt_value),
            "--dynamic-chunk", str(execution["dynamic_chunk"]),
            "--mode3d-parallel-field-init",
            "--mover", str(config["model"]["mover"]),
        ],
        "poes": [
            python, "run_C10.py", "--profile", args.profile,
            "--solver", "GRIDDED", "--cutoff-evaluation", "DIRECT_ACCESS",
            "--comparison-observable", "ACCESS_T50", "--max-trace-time", "300",
            "--output-root", str(output / "C10"), "--amps", str(amps_path),
            "--mpirun", args.mpirun, "-np", str(np_value), "-nt", str(nt_value),
            "--dynamic-chunk", str(execution["dynamic_chunk"]),
            "--mode3d-parallel-field-init",
            "--mover", str(config["model"]["mover"]),
        ],
        "morphology": [
            python, str(root / "scripts" / "run_morphology.py"),
            "--profile", args.profile, "--amps", str(amps_path),
            "--mpirun", args.mpirun, "-np", str(np_value), "-nt", str(nt_value),
            "--output-root", str(output / "morphology"),
            "--mesh-layout", args.mesh_layout,
        ],
        "compare": [
            python, str(root / "scripts" / "compare_observations.py"),
            "--c9-root", str(output / "C9" / "gridded"),
            "--c10-root", str(output / "C10" / "gridded"),
            "--output-root", str(output / "comparison"),
        ],
        "dynamics": [
            python, str(root / "scripts" / "analyze_dynamics.py"),
            "--morphology-root", str(output / "morphology"),
            "--output-root", str(output / "dynamics"),
        ],
        "figures": [
            python, str(root / "scripts" / "make_figures.py"),
            "--comparison-root", str(output / "comparison"),
            "--dynamics-root", str(output / "dynamics"),
            "--morphology-root", str(output / "morphology"),
            "--output-root", str(output / "figures"),
            "--require-publication-products",
        ],
    }
    if args.epochs_per_batch is not None:
        commands["morphology"] += ["--epochs-per-batch", str(args.epochs_per_batch)]
    if args.keep:
        # This flag is deliberately limited to the expensive morphology solver.
        # All lightweight reducers are rerun so a parser or figure fix can be
        # applied to preserved AMPS output without retaining stale conclusions.
        commands["morphology"].append("--keep")
    if not args.independent_observation_runs:
        # The morphology runner adds the exact observation midpoints, splits
        # each two-shell Tecplot product strictly by altitude, and stages normal
        # single-shell files under these historical C9/C10 output trees.
        commands["morphology"] += [
            "--include-observation-epochs",
            "--c9-output-root", str(output / "C9"),
            "--c10-output-root", str(output / "C10"),
        ]
        # The shared morphology grid is 15 x 2 degrees.  Passing those values
        # to C9/C10 keeps their saved command/provenance records aligned with
        # the staged raw file even though --skip-run suppresses execution.
        shell_lon = str(config["model"]["shell_longitude_step_deg"])
        shell_lat = str(config["model"]["shell_latitude_step_deg"])
        commands["pamela"] += [
            "--shell-lon-res-deg", shell_lon,
            "--shell-lat-res-deg", shell_lat,
        ]
        commands["poes"] += [
            "--shell-lon-res-deg", shell_lon,
            "--shell-lat-res-deg", shell_lat,
        ]
        if not args.prepare_only:
            commands["pamela"].append("--skip-run")
            commands["poes"].append("--skip-run")
    if args.prepare_only:
        commands["pamela"].append("--dry-run")
        commands["poes"].append("--dry-run")
        commands["morphology"].append("--prepare-only")

    cwd = {
        "validate": root,
        "pamela": root / "vendor" / "C9",
        "poes": root / "vendor" / "C10",
        "morphology": root,
        "compare": root,
        "dynamics": root,
        "figures": root,
    }
    epochs_per_batch = (
        args.epochs_per_batch if args.epochs_per_batch is not None
        else int(execution.get(
            "epochs_per_batch", execution.get("epochs_per_batch_group", 16)
        ))
    )
    record = {
        "study_id": config["study_id"], "profile": args.profile,
        "created_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "prepare_only": args.prepare_only, "stages": stages,
        "mpi_ranks": np_value, "threads_per_rank": nt_value,
        "mesh_layout": args.mesh_layout,
        "epochs_per_batch": epochs_per_batch,
        # Compatibility alias for manifests produced by the earlier
        # directory-grouping implementation.
        "epochs_per_batch_group": epochs_per_batch,
        "observation_products_reused": not args.independent_observation_runs,
        "keep_complete_morphology_batches": args.keep,
        "commands": {stage: commands[stage] for stage in stages},
        "return_codes": {},
        "stage_elapsed_seconds": {},
    }
    print("=" * 78, flush=True)
    print("December 2006 TS05 cutoff-erosion study", flush=True)
    print(f"profile: {args.profile}", flush=True)
    print(f"stages: {', '.join(stages)}", flush=True)
    print(f"MPI ranks / threads per rank: {np_value} / {nt_value}", flush=True)
    print(f"AMPS executable: {amps_path}", flush=True)
    print(f"output root: {output}", flush=True)
    print(f"prepare only: {args.prepare_only}", flush=True)
    print(
        f"mesh layout: {args.mesh_layout}; epochs per AMPS batch: "
        f"{record['epochs_per_batch']}", flush=True,
    )
    print(
        "observation AMPS products: "
        + ("independent C9/C10 launches" if args.independent_observation_runs
           else "shared 475/850-km morphology products"),
        flush=True,
    )

    final_rc = 0
    deferred_validation_failures: List[str] = []
    for stage_index, stage in enumerate(stages, start=1):
        # C9 and C10 are independent observational anchors. Their nonzero
        # acceptance status remains a failure of the overall study, but it does
        # not prevent creation of comparison, dynamics, and figure diagnostics.
        # This distinction is important in development: a model-data bias must
        # be visible in the final plots rather than suppressing those plots.
        # Comparison and inference cannot exist in preparation-only mode because
        # no model output was generated.  Their commands remain in the record if
        # explicitly requested, but are skipped rather than failing on absence.
        if args.prepare_only and stage in ("compare", "dynamics", "figures"):
            print(f"[{stage_index}/{len(stages)}] SKIP {stage.upper()}: --prepare-only",
                  flush=True)
            record["return_codes"][stage] = None
            record["stage_elapsed_seconds"][stage] = 0.0
            continue
        stage_started = time.monotonic()
        rc = execute(
            commands[stage], cwd[stage], output / "logs" / f"{stage}.log", False,
            stage=stage, stage_index=stage_index, stage_count=len(stages),
        )
        if rc == 0 and not args.prepare_only:
            contract_problem = stage_output_problem(
                stage, output, not args.independent_observation_runs
            )
            if contract_problem:
                rc = 3
                print(
                    f"ERROR: {stage.upper()} exited successfully but violated its "
                    f"output contract: {contract_problem}",
                    file=sys.stderr, flush=True,
                )
        record["return_codes"][stage] = rc
        record["stage_elapsed_seconds"][stage] = round(
            time.monotonic() - stage_started, 3
        )
        final_rc = final_rc or rc
        if rc and stage in OBSERVATIONAL_VALIDATION_STAGES:
            deferred_validation_failures.append(stage)
            print(
                f"{stage.upper()} failed its observational acceptance gate. "
                "The remaining stages will still run and the final study status "
                "will remain FAIL so the result cannot be mistaken for a validated run.",
                file=sys.stderr, flush=True,
            )
            continue
        if rc and not args.continue_on_validation_failure:
            print(
                f"Stopping after {stage.upper()} failure. "
                "Use --continue-on-validation-failure only to continue after a "
                "non-observational pipeline failure.",
                file=sys.stderr, flush=True,
            )
            break
    record["passed"] = final_rc == 0
    record["elapsed_seconds"] = round(time.monotonic() - run_started, 3)
    (output / "study_run_manifest.json").write_text(
        json.dumps(record, indent=2) + "\n", encoding="utf-8"
    )
    print("=" * 78, flush=True)
    print(
        f"STUDY {'PASS' if final_rc == 0 else 'FAIL'}: exit={final_rc}, "
        f"elapsed={record['elapsed_seconds']:.1f} s",
        flush=True,
    )
    for stage in stages:
        if stage not in record["return_codes"]:
            status = "NOT RUN"
        elif record["return_codes"][stage] is None:
            status = "SKIPPED"
        elif record["return_codes"][stage] == 0:
            status = "PASS"
        else:
            status = f"FAIL ({record['return_codes'][stage]})"
        elapsed = record["stage_elapsed_seconds"].get(stage)
        elapsed_text = "" if elapsed is None else f", {elapsed:.1f} s"
        print(f"  {stage:<10} {status}{elapsed_text}", flush=True)
    print(f"Run manifest: {output / 'study_run_manifest.json'}", flush=True)
    return final_rc


if __name__ == "__main__":
    raise SystemExit(main())
