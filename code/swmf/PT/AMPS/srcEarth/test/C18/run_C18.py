#!/usr/bin/env python3
"""C18 — deterministic POSIX-thread Mode3D field initialization.

C18 creates a serial runtime baseline for each selected standalone magnetic model
and compares it with otherwise identical runs in which mesh B/E initialization is
parallelized by temporary POSIX threads.  The cutoff backend remains SERIAL by
default, so any difference in the final access map is attributable to field
initialization, field materialization, or stale model state rather than to the
cutoff worker queue.

No checked-in numerical reference is required.  This is a same-executable
parallel-equivalence regression; C6/C7/C9/C10 remain the physical validation
suite for IGRF and Tsyganenko fields.

Example, from the AMPS repository root::

    python3 srcEarth/test/C18/run_C18.py --profile ROUTINE --amps ./amps -np 4
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, Iterable, Iterator, List, Mapping, Optional, Sequence, Tuple

TEST_ID = "C18"
TEST_NAME = "POSIX-thread deterministic background-field initialization"
SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_TEMPLATE = SCRIPT_DIR / "AMPS_PARAM_C18.in"
EARTH_RADIUS_KM = 6371.2
PROTON_REST_MEV = 938.27208816
SUPPORTED_MODELS = ("DIPOLE", "IGRF", "T96", "T05")
DEFAULT_EPOCH = "2006-12-15T03:05:00"
DEFAULT_RIGIDITIES_GV = (0.20, 0.50, 1.00, 2.00)

PROFILE_DEFAULTS = {
    "SMOKE": {
        "models": ("IGRF", "T05"),
        "thread_counts": (1, 4, 16),
        "repeats": 1,
    },
    "ROUTINE": {
        "models": ("DIPOLE", "IGRF", "T96", "T05"),
        "thread_counts": (1, 2, 4, 8, 16),
        "repeats": 2,
    },
    "STRESS": {
        "models": ("IGRF", "T96", "T05"),
        "thread_counts": (1, 2, 4, 8, 16, 32),
        "repeats": 5,
    },
}


@dataclass(frozen=True)
class Fingerprint:
    path_names: Tuple[str, ...]
    numeric_rows: int
    numeric_values: int
    sha256: str
    first_numeric_record: str
    last_numeric_record: str


@dataclass
class RunRecord:
    model: str
    case: str
    requested_workers: int
    actual_participants: int
    repeat: int
    run_directory: str
    command: List[str]
    return_code: int
    field_fingerprint: Optional[Fingerprint]
    cutoff_fingerprint: Optional[Fingerprint]
    field_matches_baseline: bool
    cutoff_matches_baseline: bool
    worker_banner_matches: bool
    passed: bool
    error: str = ""


def parse_csv_tokens(text: str, cast, name: str) -> Tuple:
    values = []
    for raw in str(text).split(","):
        raw = raw.strip()
        if not raw:
            continue
        try:
            values.append(cast(raw))
        except ValueError as exc:
            raise argparse.ArgumentTypeError(
                "invalid %s value %r in %r" % (name, raw, text)
            ) from exc
    if not values:
        raise argparse.ArgumentTypeError("%s list is empty" % name)
    return tuple(values)


def parse_models(text: str) -> Tuple[str, ...]:
    models = tuple(str(value).upper() for value in parse_csv_tokens(text, str, "model"))
    unsupported = sorted(set(models) - set(SUPPORTED_MODELS))
    if unsupported:
        raise argparse.ArgumentTypeError(
            "unsupported C18 model(s): %s; supported: %s" %
            (", ".join(unsupported), ", ".join(SUPPORTED_MODELS))
        )
    return models


def parse_positive_ints(text: str) -> Tuple[int, ...]:
    values = parse_csv_tokens(text, int, "thread count")
    if any(value < 1 for value in values):
        raise argparse.ArgumentTypeError("thread counts must be >= 1")
    if len(set(values)) != len(values):
        raise argparse.ArgumentTypeError("thread counts must be unique")
    return values


def parse_positive_floats(text: str) -> Tuple[float, ...]:
    values = parse_csv_tokens(text, float, "rigidity")
    if any(value <= 0.0 or not math.isfinite(value) for value in values):
        raise argparse.ArgumentTypeError("rigidities must be finite and positive")
    return values


def kinetic_energy_mev_from_rigidity_gv(rigidity_gv: float) -> float:
    momentum_mev = 1000.0 * rigidity_gv
    return math.hypot(momentum_mev, PROTON_REST_MEV) - PROTON_REST_MEV


class ArgumentDefaultsRawHelpFormatter(
        argparse.ArgumentDefaultsHelpFormatter,
        argparse.RawDescriptionHelpFormatter):
    """Preserve command examples while retaining default-value annotations."""


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=TEST_NAME,
        formatter_class=ArgumentDefaultsRawHelpFormatter,
        epilog=(
            "Example, from the AMPS repository root:\n"
            "  python3 srcEarth/test/C18/run_C18.py --profile ROUTINE \\\n"
            "    --amps ./amps -np 4"
        ),
    )
    parser.add_argument("--profile", choices=tuple(PROFILE_DEFAULTS), default="ROUTINE")
    parser.add_argument("--models", type=parse_models, default=None,
                        help="comma-separated standalone field models")
    parser.add_argument("--thread-counts", type=parse_positive_ints, default=None,
                        help="temporary pthread-worker counts to compare")
    parser.add_argument("--repeats", type=int, default=None,
                        help="independent repeats of every pthread case")
    parser.add_argument("-np", "--np", type=int, default=4, help="MPI ranks")
    parser.add_argument("--amps", default="./amps")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher executable")
    parser.add_argument("--template", default=str(DEFAULT_TEMPLATE))
    parser.add_argument("--output-root", default="test_output/C18_parallel_determinism")
    parser.add_argument("--scheduler", choices=("STATIC", "BLOCK_CYCLIC", "DYNAMIC"),
                        default="STATIC")
    parser.add_argument("--dynamic-chunk", type=int, default=32)
    parser.add_argument("--mover", default="RK4")
    parser.add_argument("--epoch", default=DEFAULT_EPOCH)
    parser.add_argument("--rigidities-gv", type=parse_positive_floats,
                        default=DEFAULT_RIGIDITIES_GV)
    parser.add_argument("--shell-alt-km", type=float, default=850.0)
    parser.add_argument("--shell-lon-res-deg", type=float, default=90.0)
    parser.add_argument("--shell-lat-res-deg", type=float, default=10.0)
    parser.add_argument("--access-abs-lat-min-deg", type=float, default=50.0)
    parser.add_argument("--access-abs-lat-max-deg", type=float, default=80.0)
    parser.add_argument("--domain-half-size-re", type=float, default=6.0)
    parser.add_argument("--mode3d-mesh-res-earth-re", type=float, default=0.50)
    parser.add_argument("--mode3d-mesh-res-boundary-re", type=float, default=1.50)
    parser.add_argument("--mode3d-mesh-coarsening",
                        choices=("LINEAR", "LOG", "EXPONENTIAL", "POWER", "CONSTANT"),
                        default="LINEAR")
    parser.add_argument("--mode3d-mesh-exponent", type=float, default=1.0)
    parser.add_argument("--dt-trace", type=float, default=0.2)
    parser.add_argument("--max-steps", type=int, default=250000)
    parser.add_argument("--max-trace-time", type=float, default=12.0)
    parser.add_argument("--max-trace-distance-re", type=float, default=150.0)
    parser.add_argument("--cutoff-backend", choices=("SERIAL", "THREADS"), default="SERIAL",
                        help="SERIAL isolates field initialization; THREADS is an end-to-end stress mode")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-run", action="store_true",
                        help="compare outputs already present under --output-root")
    parser.add_argument("--keep", action="store_true",
                        help="preserve an existing output root before a new run")
    parser.add_argument("--fail-fast", action="store_true")
    parser.add_argument("--self-test", action="store_true",
                        help="exercise CLI and fingerprint comparison without launching AMPS")
    args = parser.parse_args(argv)

    defaults = PROFILE_DEFAULTS[args.profile]
    if args.models is None:
        args.models = defaults["models"]
    if args.thread_counts is None:
        args.thread_counts = defaults["thread_counts"]
    if args.repeats is None:
        args.repeats = defaults["repeats"]

    if args.repeats < 1:
        parser.error("--repeats must be >= 1")
    if args.np < 1:
        parser.error("-np must be >= 1")
    if args.dynamic_chunk < 0:
        parser.error("--dynamic-chunk must be >= 0")
    if not (0.0 <= args.access_abs_lat_min_deg < args.access_abs_lat_max_deg <= 90.0):
        parser.error("access latitude bounds must satisfy 0 <= min < max <= 90")
    positive_values = (
        args.shell_alt_km, args.shell_lon_res_deg, args.shell_lat_res_deg,
        args.domain_half_size_re, args.mode3d_mesh_res_earth_re,
        args.mode3d_mesh_res_boundary_re, args.mode3d_mesh_exponent,
        args.dt_trace, args.max_trace_time, args.max_trace_distance_re,
    )
    if any(value <= 0.0 or not math.isfinite(value) for value in positive_values):
        parser.error("mesh, shell, domain, and trajectory scalar controls must be positive")
    if args.max_steps < 1:
        parser.error("--max-steps must be >= 1")
    return args


def replace_tokens(template_text: str, replacements: Mapping[str, str]) -> str:
    output = template_text
    for key, value in replacements.items():
        token = "__%s__" % key
        if token not in output:
            raise ValueError("template token %s is missing" % token)
        output = output.replace(token, value)
    leftovers = sorted(set(re.findall(r"__[A-Z0-9_]+__", output)))
    if leftovers:
        raise ValueError("unresolved template token(s): %s" % ", ".join(leftovers))
    return output


def render_input(template: Path, destination: Path, model: str,
                 args: argparse.Namespace) -> None:
    rigidities = tuple(sorted(args.rigidities_gv))
    replacements = {
        "FIELD_MODEL": model,
        "EPOCH": args.epoch.replace("Z", ""),
        "CUTOFF_EMIN_MEV": "%.15g" % kinetic_energy_mev_from_rigidity_gv(0.8 * rigidities[0]),
        "CUTOFF_EMAX_MEV": "%.15g" % kinetic_energy_mev_from_rigidity_gv(1.2 * rigidities[-1]),
        "RIGIDITY_LIST_GV": ",".join("%.12g" % value for value in rigidities),
        "ACCESS_LAT_MIN_DEG": "%.12g" % args.access_abs_lat_min_deg,
        "ACCESS_LAT_MAX_DEG": "%.12g" % args.access_abs_lat_max_deg,
        "MAX_TRACE_TIME_S": "%.12g" % args.max_trace_time,
        "MESH_RES_EARTH_RE": "%.12g" % args.mode3d_mesh_res_earth_re,
        "MESH_RES_BOUNDARY_RE": "%.12g" % args.mode3d_mesh_res_boundary_re,
        "MESH_COARSENING": args.mode3d_mesh_coarsening,
        "MESH_EXPONENT": "%.12g" % args.mode3d_mesh_exponent,
        "DOMAIN_HALF_SIZE_KM": "%.12g" % (args.domain_half_size_re * EARTH_RADIUS_KM),
        "SHELL_ALT_KM": "%.12g" % args.shell_alt_km,
        "SHELL_LON_RES_DEG": "%.12g" % args.shell_lon_res_deg,
        "SHELL_LAT_RES_DEG": "%.12g" % args.shell_lat_res_deg,
        "DT_TRACE_S": "%.12g" % args.dt_trace,
        "MAX_STEPS": str(args.max_steps),
        "MAX_TRACE_DISTANCE_RE": "%.12g" % args.max_trace_distance_re,
    }
    destination.write_text(replace_tokens(template.read_text(), replacements))


def build_command(args: argparse.Namespace, amps: Path, requested_workers: int,
                  parallel_field_init: bool) -> List[str]:
    command = [
        args.mpirun, "-np", str(args.np), str(amps),
        "-mode", "3d",
        "-i", "AMPS_PARAM_C18.in",
        "-mode3d-field-eval", "MESH",
        "-mode3d-parallel", args.cutoff_backend,
        "-mode3d-threads", str(requested_workers),
        "-mode3d-mpi-scheduler", args.scheduler,
        "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
        "-mode3d-output-initialized",
        "-cutoff-search", "RIGIDITY_LIST",
        "-cutoff-rigidity-list-gv",
        ",".join("%.12g" % value for value in sorted(args.rigidities_gv)),
        "-cutoff-access-abs-lat-min", str(args.access_abs_lat_min_deg),
        "-cutoff-access-abs-lat-max", str(args.access_abs_lat_max_deg),
        "-cutoff-trace-policy", "ACCURATE",
        "-mover", args.mover,
    ]
    if parallel_field_init:
        command.append("-mode3d-parallel-field-init")
    return command


def run_process(command: Sequence[str], cwd: Path, log_path: Path) -> int:
    with log_path.open("w") as log:
        log.write("Command:\n  %s\n\n" % " ".join(command))
        log.flush()
        process = subprocess.Popen(
            list(command), cwd=str(cwd), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True,
        )
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            log.write(line)
            log.flush()
        return process.wait()


def _numeric_record(line: str) -> Optional[str]:
    text = line.strip()
    if not text or text.startswith(("#", "!")):
        return None
    upper = text.upper()
    if upper.startswith(("TITLE", "VARIABLES", "ZONE", "DATASETAUXDATA", "AUXDATA")):
        return None
    fields = text.replace(",", " ").split()
    if not fields:
        return None
    values: List[str] = []
    try:
        for field in fields:
            value = float(field)
            if not math.isfinite(value):
                raise ValueError("non-finite value %s" % field)
            values.append(value.hex())
    except ValueError:
        return None
    return " ".join(values)


def discover_field_files(run_dir: Path) -> List[Path]:
    candidates = []
    for path in run_dir.iterdir():
        if path.is_file() and path.name.startswith("amps_3d_initialized") \
                and ".data.dat" in path.name:
            candidates.append(path)
    return sorted(candidates, key=lambda path: path.name)


def discover_cutoff_files(run_dir: Path) -> List[Path]:
    primary = run_dir / "cutoff_3d_shells_access.dat"
    if primary.exists():
        return [primary]
    # Retain compatibility with a possible snapshot suffix or rank-file suffix.
    return sorted(run_dir.glob("cutoff_3d_shells_access*.dat*"), key=lambda path: path.name)


def fingerprint_numeric_files(paths: Sequence[Path]) -> Fingerprint:
    if not paths:
        raise FileNotFoundError("no numerical output files were found")
    digest = hashlib.sha256()
    n_rows = 0
    n_values = 0
    first = ""
    last = ""
    for path in sorted(paths, key=lambda item: item.name):
        digest.update(("FILE %s\n" % path.name).encode("utf-8"))
        with path.open(errors="replace") as stream:
            for raw in stream:
                record = _numeric_record(raw)
                if record is None:
                    continue
                if not first:
                    first = record
                last = record
                n_rows += 1
                n_values += len(record.split())
                digest.update(record.encode("ascii"))
                digest.update(b"\n")
    if n_rows == 0:
        raise ValueError("no numeric rows found in %s" % ", ".join(str(p) for p in paths))
    return Fingerprint(
        path_names=tuple(path.name for path in sorted(paths, key=lambda item: item.name)),
        numeric_rows=n_rows,
        numeric_values=n_values,
        sha256=digest.hexdigest(),
        first_numeric_record=first,
        last_numeric_record=last,
    )


def compare_fingerprints(first: Fingerprint, second: Fingerprint) -> bool:
    return (
        first.numeric_rows == second.numeric_rows
        and first.numeric_values == second.numeric_values
        and first.sha256 == second.sha256
    )


def validate_worker_banner(log_path: Path, requested_workers: int,
                           parallel_field_init: bool) -> Tuple[bool, str]:
    if not log_path.exists():
        return False, "log file is missing"
    text = log_path.read_text(errors="replace")
    pattern = re.compile(
        r"Parallel background-field initialization:\s*POSIX threads;\s*"
        r"(\d+) temporary workers \+ caller \((\d+) equal shares per MPI rank\)",
        re.IGNORECASE,
    )
    matches = pattern.findall(text)
    if parallel_field_init:
        expected = (requested_workers, requested_workers + 1)
        if not matches:
            return False, "POSIX field-initialization banner was not found"
        parsed = {(int(a), int(b)) for a, b in matches}
        if expected not in parsed:
            return False, "banner reports %s, expected workers/participants=%s" % (
                sorted(parsed), expected)
        return True, ""
    if matches:
        return False, "serial baseline unexpectedly reported POSIX field initialization"
    return True, ""


def case_directory(root: Path, model: str, case: str,
                   requested_workers: int, repeat: int) -> Path:
    model_dir = root / model.lower()
    if case == "baseline":
        return model_dir / "baseline_serial"
    return model_dir / ("pthreads_nt%02d_repeat%02d" % (requested_workers, repeat))


def execute_case(args: argparse.Namespace, template: Path, amps: Path,
                 output_root: Path, model: str, case: str,
                 requested_workers: int, repeat: int,
                 baseline_field: Optional[Fingerprint],
                 baseline_cutoff: Optional[Fingerprint]) -> RunRecord:
    parallel_field_init = case != "baseline"
    run_dir = case_directory(output_root, model, case, requested_workers, repeat)
    if run_dir.exists() and not args.keep and not args.skip_run:
        shutil.rmtree(run_dir)
    run_dir.mkdir(parents=True, exist_ok=True)
    input_path = run_dir / "AMPS_PARAM_C18.in"
    if not args.skip_run:
        render_input(template, input_path, model, args)
    command = build_command(args, amps, requested_workers, parallel_field_init)
    (run_dir / "C18_command.json").write_text(json.dumps(command, indent=2) + "\n")
    print("C18 %s %s nt=%d repeat=%d command:\n  %s" %
          (model, case, requested_workers, repeat, " ".join(command)))

    return_code = 0
    if not args.skip_run and not args.dry_run:
        return_code = run_process(command, run_dir, run_dir / "C18_amps.log")
    if args.dry_run:
        return RunRecord(
            model=model, case=case, requested_workers=requested_workers,
            actual_participants=(requested_workers + 1 if parallel_field_init else 1),
            repeat=repeat, run_directory=str(run_dir), command=command,
            return_code=0, field_fingerprint=None, cutoff_fingerprint=None,
            field_matches_baseline=True, cutoff_matches_baseline=True,
            worker_banner_matches=True, passed=True,
        )
    if return_code != 0:
        return RunRecord(
            model=model, case=case, requested_workers=requested_workers,
            actual_participants=(requested_workers + 1 if parallel_field_init else 1),
            repeat=repeat, run_directory=str(run_dir), command=command,
            return_code=return_code, field_fingerprint=None, cutoff_fingerprint=None,
            field_matches_baseline=False, cutoff_matches_baseline=False,
            worker_banner_matches=False, passed=False,
            error="AMPS exited with code %d" % return_code,
        )

    errors = []
    try:
        field_fp = fingerprint_numeric_files(discover_field_files(run_dir))
    except Exception as exc:
        field_fp = None
        errors.append("field output: %s" % exc)
    try:
        cutoff_fp = fingerprint_numeric_files(discover_cutoff_files(run_dir))
    except Exception as exc:
        cutoff_fp = None
        errors.append("cutoff output: %s" % exc)

    banner_ok, banner_error = validate_worker_banner(
        run_dir / "C18_amps.log", requested_workers, parallel_field_init)
    if not banner_ok:
        errors.append(banner_error)

    if case == "baseline":
        field_ok = field_fp is not None
        cutoff_ok = cutoff_fp is not None
    else:
        field_ok = field_fp is not None and baseline_field is not None \
            and compare_fingerprints(field_fp, baseline_field)
        cutoff_ok = cutoff_fp is not None and baseline_cutoff is not None \
            and compare_fingerprints(cutoff_fp, baseline_cutoff)
        if not field_ok:
            errors.append("initialized-field fingerprint differs from serial baseline")
        if not cutoff_ok:
            errors.append("cutoff-map fingerprint differs from serial baseline")

    passed = return_code == 0 and field_ok and cutoff_ok and banner_ok
    return RunRecord(
        model=model, case=case, requested_workers=requested_workers,
        actual_participants=(requested_workers + 1 if parallel_field_init else 1),
        repeat=repeat, run_directory=str(run_dir), command=command,
        return_code=return_code, field_fingerprint=field_fp,
        cutoff_fingerprint=cutoff_fp, field_matches_baseline=field_ok,
        cutoff_matches_baseline=cutoff_ok, worker_banner_matches=banner_ok,
        passed=passed, error="; ".join(errors),
    )


def _fingerprint_to_dict(value: Optional[Fingerprint]) -> Optional[Dict[str, object]]:
    return asdict(value) if value is not None else None


def record_to_dict(record: RunRecord) -> Dict[str, object]:
    result = asdict(record)
    result["field_fingerprint"] = _fingerprint_to_dict(record.field_fingerprint)
    result["cutoff_fingerprint"] = _fingerprint_to_dict(record.cutoff_fingerprint)
    return result


def write_csv(path: Path, records: Sequence[RunRecord]) -> None:
    fields = [
        "model", "case", "requested_workers", "actual_participants", "repeat",
        "return_code", "field_numeric_rows", "field_sha256",
        "cutoff_numeric_rows", "cutoff_sha256", "field_matches_baseline",
        "cutoff_matches_baseline", "worker_banner_matches", "passed", "error",
        "run_directory",
    ]
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for record in records:
            writer.writerow({
                "model": record.model,
                "case": record.case,
                "requested_workers": record.requested_workers,
                "actual_participants": record.actual_participants,
                "repeat": record.repeat,
                "return_code": record.return_code,
                "field_numeric_rows": (
                    record.field_fingerprint.numeric_rows if record.field_fingerprint else ""),
                "field_sha256": (
                    record.field_fingerprint.sha256 if record.field_fingerprint else ""),
                "cutoff_numeric_rows": (
                    record.cutoff_fingerprint.numeric_rows if record.cutoff_fingerprint else ""),
                "cutoff_sha256": (
                    record.cutoff_fingerprint.sha256 if record.cutoff_fingerprint else ""),
                "field_matches_baseline": record.field_matches_baseline,
                "cutoff_matches_baseline": record.cutoff_matches_baseline,
                "worker_banner_matches": record.worker_banner_matches,
                "passed": record.passed,
                "error": record.error,
                "run_directory": record.run_directory,
            })


def write_summary(path: Path, args: argparse.Namespace,
                  records: Sequence[RunRecord], passed: bool) -> None:
    lines = [
        "%s — %s" % (TEST_ID, TEST_NAME),
        "result: %s" % ("PASS" if passed else "FAIL"),
        "profile: %s" % args.profile,
        "models: %s" % ",".join(args.models),
        "temporary pthread workers: %s" % ",".join(str(v) for v in args.thread_counts),
        "actual participants per MPI rank: %s" % ",".join(
            "%d->%d" % (v, v + 1) for v in args.thread_counts),
        "repeats: %d" % args.repeats,
        "MPI ranks: %d" % args.np,
        "cutoff backend: %s" % args.cutoff_backend,
        "checked-in numerical reference: none",
        "runtime baseline: serial field initialization and serial cutoff per model",
        "",
    ]
    for record in records:
        lines.append(
            "%s %-9s nt=%-2d repeat=%-2d field=%s cutoff=%s banner=%s %s%s" % (
                record.model, record.case, record.requested_workers, record.repeat,
                "OK" if record.field_matches_baseline else "FAIL",
                "OK" if record.cutoff_matches_baseline else "FAIL",
                "OK" if record.worker_banner_matches else "FAIL",
                "PASS" if record.passed else "FAIL",
                (" — " + record.error) if record.error else "",
            )
        )
    path.write_text("\n".join(lines) + "\n")


def self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="c18_selftest_") as raw:
        root = Path(raw)
        dirs = [root / name for name in ("a", "b", "c")]
        for directory in dirs:
            directory.mkdir()
        a, b, c = [directory / "field.dat" for directory in dirs]
        text = (
            'TITLE="synthetic"\nVARIABLES="x" "y" "z" "Bx"\n'
            'ZONE T="z"\n0 0 0 1.0\n1 0 0 -0.0\n'
        )
        a.write_text(text)
        b.write_text(text.replace("1.0", "1.0000000000000000"))
        c.write_text(text.replace("1.0", "1.0000000000000002"))
        fa = fingerprint_numeric_files([a])
        fb = fingerprint_numeric_files([b])
        fc = fingerprint_numeric_files([c])
        if not compare_fingerprints(fa, fb):
            print("C18 self-test failed: equivalent decimal formatting changed digest",
                  file=sys.stderr)
            return 1
        if compare_fingerprints(fa, fc):
            print("C18 self-test failed: changed field value was not detected",
                  file=sys.stderr)
            return 1
        parsed = parse_args(["--profile", "ROUTINE", "--models", "IGRF,T05",
                             "--thread-counts", "1,2,4,8,16", "--repeats", "2"])
        if parsed.thread_counts != (1, 2, 4, 8, 16) or parsed.models != ("IGRF", "T05"):
            print("C18 self-test failed: CLI list parsing", file=sys.stderr)
            return 1
    print("C18 self-test: PASS")
    return 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        return self_test()

    launch_dir = Path.cwd()
    template = Path(args.template).expanduser()
    if not template.is_absolute():
        template = (launch_dir / template).resolve() if template.exists() else DEFAULT_TEMPLATE
    if not template.exists():
        print("C18 template not found: %s" % template, file=sys.stderr)
        return 2

    amps = Path(args.amps).expanduser()
    if not amps.is_absolute():
        amps = (launch_dir / amps).resolve()
    if not args.dry_run and not args.skip_run and not amps.exists():
        print("AMPS executable not found: %s" % amps, file=sys.stderr)
        return 2

    output_root = Path(args.output_root).expanduser()
    if not output_root.is_absolute():
        output_root = (launch_dir / output_root).resolve()
    if output_root.exists() and not args.keep and not args.skip_run:
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    config = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "profile": args.profile,
        "models": args.models,
        "thread_counts": args.thread_counts,
        "repeats": args.repeats,
        "np": args.np,
        "cutoff_backend": args.cutoff_backend,
        "note": (
            "No checked-in reference solution is used. Each model's serial run is "
            "the runtime baseline for parallel-equivalence comparisons."
        ),
    }
    (output_root / "C18_configuration.json").write_text(
        json.dumps(config, indent=2) + "\n")

    records: List[RunRecord] = []
    for model in args.models:
        baseline = execute_case(
            args, template, amps, output_root, model, "baseline", 1, 1,
            None, None,
        )
        records.append(baseline)
        if not baseline.passed and not args.dry_run:
            if args.fail_fast:
                break
            # Parallel comparisons cannot be interpreted without a baseline.
            continue
        baseline_field = baseline.field_fingerprint
        baseline_cutoff = baseline.cutoff_fingerprint

        for requested_workers in args.thread_counts:
            for repeat in range(1, args.repeats + 1):
                record = execute_case(
                    args, template, amps, output_root, model, "pthreads",
                    requested_workers, repeat, baseline_field, baseline_cutoff,
                )
                records.append(record)
                if args.fail_fast and not record.passed and not args.dry_run:
                    break
            if args.fail_fast and records and not records[-1].passed and not args.dry_run:
                break
        if args.fail_fast and records and not records[-1].passed and not args.dry_run:
            break

    passed = all(record.passed for record in records) and bool(records)
    write_csv(output_root / "C18_comparison.csv", records)
    (output_root / "C18_result.json").write_text(json.dumps({
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "no_checked_in_reference": True,
        "records": [record_to_dict(record) for record in records],
    }, indent=2) + "\n")
    (output_root / "C18_commands.json").write_text(json.dumps([
        {"model": r.model, "case": r.case,
         "requested_workers": r.requested_workers, "repeat": r.repeat,
         "cwd": r.run_directory, "command": r.command}
        for r in records
    ], indent=2) + "\n")
    write_summary(output_root / "C18_summary.txt", args, records, passed)

    print((output_root / "C18_summary.txt").read_text())
    if args.dry_run:
        return 0
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
