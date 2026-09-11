#!/usr/bin/env python3
"""Run complete-sphere TS05 cutoff mapping and its reproducible reductions.

The global-map workflow is intentionally independent of the PAMELA and
POES/MetOp observation operators.  It reuses the audited morphology execution
engine, but supplies a complete geographic shell and a broad rigidity bracket.
Every AMPS process uses native Mode3D ``SNAPSHOT_LIST`` batching: the AMR mesh
topology is allocated once, both altitude shells use the same epoch-specific
field, and magnetic-field values are rebuilt at each epoch.

Stages may be selected independently.  ``model`` runs AMPS and creates the
first-pass R50 maps, ``postprocess`` validates/canonicalizes the complete sphere
and calculates quiet-relative erosion, and ``figures`` renders the archived
tables.  Selecting only the latter two stages never launches AMPS.
"""

from __future__ import annotations

import argparse
import copy
import json
import os
import shutil
import sys
import time
import math
from pathlib import Path
from typing import Dict, List, Mapping, MutableMapping

from run_study import execute
from study_common import default_output_root, load_config, resolve_output_path


STAGE_ORDER = ("model", "postprocess", "figures")


def deep_merge(base: MutableMapping[str, object],
               overlay: Mapping[str, object]) -> MutableMapping[str, object]:
    """Recursively apply a small global-map overlay to the frozen study config.

    The event window, TS05 driver, numerical field settings, and profile cadence
    remain defined once in ``study.json``.  Only full-sphere controls live in
    the overlay, avoiding a second drifting copy of the December 2006 setup.
    """

    for key, value in overlay.items():
        if (isinstance(value, Mapping) and isinstance(base.get(key), dict)):
            deep_merge(base[key], value)  # type: ignore[index,arg-type]
        else:
            base[key] = copy.deepcopy(value)
    return base


def apply_profile_override(config: MutableMapping[str, object],
                           profile: str) -> MutableMapping[str, object]:
    """Apply a global-map-only profile reduction after the common overlay.

    A full spherical RIGIDITY_LIST calculation is a Cartesian product of shell
    nodes and rigidity samples.  Reusing the publication grid for an execution
    smoke test therefore launches hundreds of thousands of trajectories per
    epoch and defeats the purpose of SMOKE.  The optional profile override
    keeps the scientific FULL/ROUTINE configuration untouched while allowing
    SMOKE to retain the same physical range and workflow contracts at lower
    spatial and rigidity resolution.

    The override is copied into the effective configuration written with the
    results.  Consequently every generated input, downstream quality check,
    and figure is evaluated against the exact grid that was actually run.
    """

    overrides = config.get("global_map_profile_overrides", {})
    if not isinstance(overrides, Mapping):
        raise ValueError("global_map_profile_overrides must be a JSON object")
    selected = overrides.get(profile, {})
    if not isinstance(selected, Mapping):
        raise ValueError(f"global-map override for {profile} must be a JSON object")
    return deep_merge(config, selected)


def workload_estimate(config: Mapping[str, object], profile: str) -> Dict[str, int]:
    """Return the exact fixed-list trajectory count implied by the shell grid.

    Mode3D RIGIDITY_LIST creates one task for every
    ``(longitude, latitude, shell, rigidity)`` tuple at each epoch.  Reporting
    this estimate before AMPS starts makes accidental production-sized SMOKE
    runs obvious and distinguishes task volume from MPI scheduling chunk size.
    """

    model = config["model"]  # type: ignore[index]
    lon_step = float(model["shell_longitude_step_deg"])  # type: ignore[index]
    lat_step = float(model["shell_latitude_step_deg"])  # type: ignore[index]
    n_longitudes = int(round(360.0 / lon_step))
    n_latitudes = int(round(180.0 / lat_step)) + 1
    n_shells = len(model["shell_altitudes_km"])  # type: ignore[index]
    n_rigidities = len(model["rigidities_gv"])  # type: ignore[index]
    profile_config = config["profiles"][profile]  # type: ignore[index]
    explicit_epochs = profile_config.get("explicit_epochs_utc")
    n_epochs = len(explicit_epochs) if explicit_epochs is not None else 0
    per_epoch = n_longitudes * n_latitudes * n_shells * n_rigidities
    return {
        "longitudes": n_longitudes,
        "latitudes": n_latitudes,
        "shells": n_shells,
        "rigidities": n_rigidities,
        "epochs": n_epochs,
        "tasks_per_epoch": per_epoch,
        "tasks_total": per_epoch * n_epochs if n_epochs else 0,
    }


def validate_effective_config(config: Mapping[str, object]) -> None:
    """Reject a configuration that cannot produce a genuine global R50 map."""

    model = config["model"]  # type: ignore[index]
    analysis = config["global_map_analysis"]  # type: ignore[index]
    band = [float(value) for value in model["latitude_band_abs_deg"]]  # type: ignore[index]
    if band != [0.0, 90.0]:
        raise ValueError("the global runner requires latitude coverage 0--90 degrees")
    altitudes = [float(value) for value in model["shell_altitudes_km"]]  # type: ignore[index]
    if len(altitudes) < 1 or len(set(altitudes)) != len(altitudes):
        raise ValueError("global shell altitudes must be nonempty and unique")
    rigidities = sorted(float(value) for value in model["rigidities_gv"])  # type: ignore[index]
    if len(rigidities) != len(set(rigidities)) or any(value <= 0 for value in rigidities):
        raise ValueError("global rigidities must be unique, positive, and ordered")
    if rigidities != [float(value) for value in model["rigidities_gv"]]:  # type: ignore[index]
        raise ValueError("global rigidities must be strictly increasing")
    lon_step = float(model["shell_longitude_step_deg"])  # type: ignore[index]
    lat_step = float(model["shell_latitude_step_deg"])  # type: ignore[index]
    if lon_step <= 0.0 or lat_step <= 0.0:
        raise ValueError("global shell longitude/latitude steps must be positive")
    if (not math.isclose(360.0 / lon_step, round(360.0 / lon_step), abs_tol=1.0e-10)
            or not math.isclose(180.0 / lat_step, round(180.0 / lat_step),
                                abs_tol=1.0e-10)):
        raise ValueError("global shell steps must divide 360 and 180 degrees exactly")
    if min(rigidities) > float(analysis["maximum_expected_rigidity_min_gv"]):  # type: ignore[index]
        raise ValueError("global rigidity floor is too high to resolve polar cutoffs")
    if max(rigidities) < float(analysis["minimum_expected_rigidity_max_gv"]):  # type: ignore[index]
        raise ValueError("global rigidity ceiling is too low to resolve equatorial cutoffs")
    if str(model.get("cutoff_sampling", "VERTICAL")).upper() != "VERTICAL":
        raise ValueError("the published global R50 product is defined for vertical access")


def stage_problem(stage: str, output: Path) -> str | None:
    """Verify scientific output contracts, including native mesh reuse."""

    if stage == "model":
        path = output / "morphology" / "morphology_result.json"
        if not path.is_file():
            return f"missing model result: {path}"
        result = json.loads(path.read_text(encoding="utf-8"))
        if not result.get("passed", False):
            return f"model result reports failures: {result.get('failures', [])}"
        if result.get("mesh_layout") != "BATCHED":
            return "global model did not use BATCHED native mesh reuse"
        if not result.get("mesh_reused_across_shells", False):
            return "altitude shells did not share one Mode3D mesh"
        if int(result.get("n_epochs", 0)) > 1 and not result.get(
                "mesh_reused_across_epochs", False):
            return "multiple epochs were requested but mesh reuse was not recorded"
        if not result.get("magnetic_field_reinitialized_each_epoch", False):
            return "field refresh at every epoch was not recorded"
        expected = int(result.get("n_epochs", 0)) * int(result.get("n_altitudes", 0))
        if int(result.get("n_cutoff_rigidity_maps", 0)) != expected:
            return "model did not write one first-pass map per shell and epoch"
    elif stage == "postprocess":
        path = output / "postprocessing" / "global_map_postprocessing_result.json"
        if not path.is_file():
            return f"missing postprocessing result: {path}"
        result = json.loads(path.read_text(encoding="utf-8"))
        if not result.get("passed", False):
            return f"global-map coverage failed: {result.get('failures', [])}"
    elif stage == "figures":
        path = output / "figures" / "global_cutoff_figure_result.json"
        if not path.is_file():
            return f"missing figure result: {path}"
        result = json.loads(path.read_text(encoding="utf-8"))
        if not result.get("passed", False):
            return f"global-map figures are incomplete: {result.get('missing_required_products', [])}"
    return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", action="append", choices=STAGE_ORDER + ("all",),
                        help="May be repeated; default is all stages")
    parser.add_argument("--profile", choices=("SMOKE", "ROUTINE", "FULL"),
                        default="SMOKE")
    parser.add_argument("--base-config", type=Path,
                        help="Base event configuration; default is config/study.json")
    parser.add_argument("--config-overlay", type=Path,
                        help="Global-map overlay; default is config/global_cutoff_maps.json")
    parser.add_argument("--driver", type=Path,
                        help="Optional TS05 driver override for a sensitivity experiment")
    parser.add_argument("--amps", type=Path, default=Path("./amps"))
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("-np", type=int)
    parser.add_argument("-nt", type=int)
    parser.add_argument("--epochs-per-batch", type=int,
                        help="Native SNAPSHOT_LIST epochs per Mode3D process (minimum 2)")
    parser.add_argument("--keep", action="store_true",
                        help="Reuse only complete raw AMPS batches; regenerate all reductions")
    parser.add_argument("--reuse-raw", action="store_true",
                        help="With --stage model, rebuild maps from existing raw files without AMPS")
    parser.add_argument("--prepare-only", action="store_true",
                        help="Render and inventory BATCHED AMPS inputs without executing them")
    parser.add_argument(
        "--output-root", type=Path,
        default=default_output_root() / "global_maps",
    )
    return parser.parse_args()


def main() -> int:
    started = time.monotonic()
    args = parse_args()
    package_root, base = load_config(args.base_config)
    overlay_path = args.config_overlay or (
        package_root / "config" / "global_cutoff_maps.json"
    )
    with overlay_path.expanduser().resolve().open(encoding="utf-8") as stream:
        overlay = json.load(stream)
    config = deep_merge(copy.deepcopy(base), overlay)
    config = apply_profile_override(config, args.profile)
    validate_effective_config(config)
    workload = workload_estimate(config, args.profile)

    output = resolve_output_path(args.output_root)
    output.mkdir(parents=True, exist_ok=True)
    effective_config = output / "effective_global_cutoff_map_config.json"
    effective_config.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")

    stages = list(args.stage or ["all"])
    if "all" in stages:
        stages = list(STAGE_ORDER)
    stages = [stage for stage in STAGE_ORDER if stage in stages]
    batch_size = (
        args.epochs_per_batch if args.epochs_per_batch is not None
        else int(config["execution"]["epochs_per_batch"])  # type: ignore[index]
    )
    if batch_size < 2:
        raise SystemExit("--epochs-per-batch must be at least 2 to guarantee epoch mesh reuse")

    np_value = args.np if args.np is not None else int(
        config["execution"]["mpi_ranks"]  # type: ignore[index]
    )
    nt_value = args.nt if args.nt is not None else int(
        config["execution"]["threads_per_rank"]  # type: ignore[index]
    )
    amps = args.amps.expanduser()
    if not amps.is_absolute():
        amps = (Path.cwd() / amps).resolve()
    needs_amps = "model" in stages and not (args.prepare_only or args.reuse_raw)
    if needs_amps:
        if not amps.is_file() or not os.access(amps, os.X_OK):
            raise SystemExit(f"AMPS executable is missing or not executable: {amps}")
        if shutil.which(args.mpirun) is None:
            raise SystemExit(f"MPI launcher is unavailable: {args.mpirun}")

    python = sys.executable
    model = [
        python, str(package_root / "scripts" / "run_morphology.py"),
        "--config", str(effective_config), "--profile", args.profile,
        "--amps", str(amps), "--mpirun", args.mpirun,
        "-np", str(np_value), "-nt", str(nt_value),
        "--output-root", str(output / "morphology"),
        "--mesh-layout", "BATCHED", "--epochs-per-batch", str(batch_size),
    ]
    if args.driver:
        model += ["--driver", str(args.driver.expanduser().resolve())]
    if args.keep:
        model.append("--keep")
    if args.reuse_raw:
        model.append("--skip-run")
    if args.prepare_only:
        model.append("--prepare-only")
    commands: Dict[str, List[str]] = {
        "model": model,
        "postprocess": [
            python, str(package_root / "scripts" / "postprocess_global_cutoff_maps.py"),
            "--config", str(effective_config),
            "--map-root", str(output / "morphology"),
            "--output-root", str(output / "postprocessing"),
        ],
        "figures": [
            python, str(package_root / "scripts" / "make_global_cutoff_map_figures.py"),
            "--postprocessing-root", str(output / "postprocessing"),
            "--output-root", str(output / "figures"),
        ],
    }

    manifest = {
        "study_id": config["study_id"], "profile": args.profile,
        "output_root": str(output), "stages": stages,
        "mesh_layout": "BATCHED", "epochs_per_batch": batch_size,
        "mpi_ranks": np_value, "threads_per_rank": nt_value,
        "estimated_workload": workload,
        "reuse_raw": args.reuse_raw, "prepare_only": args.prepare_only,
        "commands": {stage: commands[stage] for stage in stages},
        "return_codes": {}, "stage_elapsed_seconds": {},
    }
    manifest_path = output / "global_cutoff_map_run_manifest.json"
    print("=" * 78, flush=True)
    print("AMPS GLOBAL CUTOFF-RIGIDITY MAPS", flush=True)
    print(f"profile: {args.profile}; stages: {', '.join(stages)}", flush=True)
    print(f"output: {output}", flush=True)
    print(
        f"mesh: native BATCHED SNAPSHOT_LIST; epochs/batch={batch_size}; "
        f"both shells share each epoch field; ranks/threads={np_value}/{nt_value}",
        flush=True,
    )
    grid_description = (
        f"grid: {workload['longitudes']} longitude x {workload['latitudes']} "
        f"latitude x {workload['shells']} shell x {workload['rigidities']} rigidity"
    )
    print(grid_description, flush=True)
    if workload["epochs"]:
        print(
            f"estimated RIGIDITY_LIST trajectories: "
            f"{workload['tasks_per_epoch']:,}/epoch x {workload['epochs']} epoch(s) "
            f"= {workload['tasks_total']:,}",
            flush=True,
        )
    else:
        print(
            f"estimated RIGIDITY_LIST trajectories: "
            f"{workload['tasks_per_epoch']:,}/epoch; epoch count is selected "
            "from cadence and event landmarks by the model stage",
            flush=True,
        )
    print("Observation epochs and observation operators: DISABLED", flush=True)
    print("=" * 78, flush=True)

    overall = 0
    for index, stage in enumerate(stages, start=1):
        if args.prepare_only and stage != "model":
            print(f"[{index}/{len(stages)}] SKIP {stage.upper()}: --prepare-only", flush=True)
            manifest["return_codes"][stage] = None  # type: ignore[index]
            manifest["stage_elapsed_seconds"][stage] = 0.0  # type: ignore[index]
            continue
        stage_started = time.monotonic()
        return_code = execute(
            commands[stage], package_root, output / "logs" / f"global_{stage}.log",
            False, stage=f"global_{stage}", stage_index=index,
            stage_count=len(stages),
        )
        if return_code == 0 and not (args.prepare_only and stage == "model"):
            problem = stage_problem(stage, output)
            if problem:
                print(f"ERROR: {problem}", file=sys.stderr, flush=True)
                return_code = 1
        manifest["return_codes"][stage] = return_code  # type: ignore[index]
        manifest["stage_elapsed_seconds"][stage] = round(  # type: ignore[index]
            time.monotonic() - stage_started, 3
        )
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        if return_code:
            overall = return_code
            print(f"Stopping after {stage.upper()} failure", file=sys.stderr, flush=True)
            break

    manifest["elapsed_seconds"] = round(time.monotonic() - started, 3)
    manifest["passed"] = overall == 0
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("=" * 78, flush=True)
    print(f"GLOBAL MAP WORKFLOW {'PASS' if overall == 0 else 'FAIL'}", flush=True)
    print(f"Run manifest: {manifest_path}", flush=True)
    return overall


if __name__ == "__main__":
    raise SystemExit(main())
