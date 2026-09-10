#!/usr/bin/env python3
"""C12 — particle-mover validation in an analytical dipole.

C12 deliberately separates numerical contracts that test the same physical
observable from capability probes that do not:

* Full-orbit movers are checked against the analytical vertical Stoermer
  cutoff. RK4 and RK6 additionally form the default hard cross-mover pair;
  BORIS and RK2 remain independently useful lower-order accuracy checks.
* Guiding-center movers do not compute the exact full-orbit Stoermer problem.
  They can be executed as diagnostics (or required only as an explicit
  capability policy), but their cutoff values are never passed off as an
  analytical Stoermer validation.
* Time-step map differences are recorded by default. They become a hard gate
  only for fixed-step runs, because an adaptive controller may leave the
  requested DT_TRACE cap inactive and make three nominal levels identical.

A scientific result is considered only after the complete two-shell coordinate
inventory has been parsed with an explicit schema, all coordinates are unique,
and every numerical value is finite. This prevents truncated or malformed
output from producing a false PASS.
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
import textwrap
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Dict, List, Mapping, Optional, Sequence, Tuple


TEST_ID = "C12"
TEST_NAME = "Particle-mover validation in an analytical dipole"
SCRIPT_DIR = Path(__file__).resolve().parent
INPUT_TEMPLATE = SCRIPT_DIR / "AMPS_PARAM_C12_gridless.in"
REFERENCE_PATH = SCRIPT_DIR / "reference_C12_stormer_movers.csv"
CONTRACT_PATH = SCRIPT_DIR / "reference_C12_acceptance_contract.csv"

EARTH_RADIUS_KM = 6371.2
PROTON_REST_MEV = 938.2720813
STORMER_R0_GV = 0.299792458 * 0.25 * 3.12e-5 * (EARTH_RADIUS_KM * 1000.0)

FULL_ORBIT_MOVERS = ("BORIS", "RK2", "RK4", "RK6")
GC_MOVERS = ("GC2", "GC4", "GC6")
ALL_MOVERS = FULL_ORBIT_MOVERS + GC_MOVERS

# The recommended profiles validate the four full-orbit implementations only.
# GC movers are opt-in because they solve a reduced equation of motion and the
# supplied AMPS build shows that GC cutoff support is not uniform across GC2,
# GC4, and GC6. ``--gc-policy`` makes that opt-in contract explicit.
PROFILE_DEFAULTS = {
    "SMOKE": {
        "movers": "BORIS,RK4,RK6", "dt_scales": "1",
        "cutoff_scan_n": 80, "max_trace_time": 300.0,
        "require_convergence": False,
    },
    "ROUTINE": {
        "movers": ",".join(FULL_ORBIT_MOVERS), "dt_scales": "1,0.5,0.25",
        "cutoff_scan_n": 200, "max_trace_time": 600.0,
        "require_convergence": False,
    },
    "THOROUGH": {
        "movers": ",".join(FULL_ORBIT_MOVERS), "dt_scales": "1,0.5,0.25,0.125",
        "cutoff_scan_n": 320, "max_trace_time": 1200.0,
        "require_convergence": False,
    },
}


@dataclass(frozen=True)
class RunSpec:
    """One immutable AMPS invocation in the C12 case matrix."""

    mover: str
    family: str
    dt_scale: float
    dt_seconds: float
    label: str
    is_finest: bool


@dataclass(frozen=True)
class ShellRow:
    """One coordinate-keyed row from a GRIDLESS cutoff shell product."""

    altitude_km: float
    longitude_deg: float
    latitude_deg: float
    cutoff_gv: float
    unresolved_count: int = 0
    range_flag_count: int = 0

    @property
    def key(self) -> Tuple[float, float, float]:
        return coordinate_key(self.altitude_km, self.longitude_deg,
                              self.latitude_deg)


@dataclass
class ParsedShell:
    """Strictly parsed shell data plus schema provenance."""

    rows: List[ShellRow]
    variables: List[str]
    cutoff_column: str
    unresolved_columns: List[str]
    range_flag_columns: List[str]


@dataclass
class CaseResult:
    """Execution, structural, and scientific state for one AMPS case."""

    spec: RunSpec
    case_dir: str
    command: List[str]
    log_file: str
    return_code: Optional[int] = None
    elapsed_seconds: Optional[float] = None
    output_file: Optional[str] = None
    parsed_rows: int = 0
    expected_rows: int = 0
    cutoff_column: Optional[str] = None
    mpi_total_tasks: Optional[int] = None
    mpi_sum_tasks: Optional[int] = None
    passed: bool = True
    messages: List[str] = field(default_factory=list)
    summary_rows: List[Dict[str, object]] = field(default_factory=list)
    cutoff_map: Dict[Tuple[float, float, float], float] = field(
        default_factory=dict, repr=False)

    def json_record(self) -> Dict[str, object]:
        record = asdict(self)
        # Tuple-keyed maps are used only while evaluating convergence/parity.
        # Per-latitude results are already in C12_summary.csv.
        record.pop("cutoff_map", None)
        return record


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def stormer_vertical_gv(latitude_deg: float, altitude_km: float) -> float:
    """Return the centered, aligned-dipole vertical cutoff in GV."""

    radius_re = (EARTH_RADIUS_KM + float(altitude_km)) / EARTH_RADIUS_KM
    return (STORMER_R0_GV *
            math.cos(math.radians(float(latitude_deg))) ** 4 /
            radius_re ** 2)


def rigidity_from_kinetic_mev(kinetic_mev: float) -> float:
    """Convert proton kinetic energy in MeV to rigidity in GV."""

    energy = float(kinetic_mev)
    return math.sqrt(energy * (energy + 2.0 * PROTON_REST_MEV)) / 1000.0


def normalize_longitude(longitude_deg: float) -> float:
    value = float(longitude_deg) % 360.0
    if abs(value - 360.0) < 1.0e-8 or abs(value) < 1.0e-8:
        value = 0.0
    return round(value, 8)


def coordinate_key(altitude_km: float, longitude_deg: float,
                   latitude_deg: float) -> Tuple[float, float, float]:
    """Canonicalize coordinates before uniqueness and coverage checks."""

    return (round(float(altitude_km), 6),
            normalize_longitude(longitude_deg),
            round(float(latitude_deg), 8))


def parse_csv_floats(text: str, label: str) -> List[float]:
    values: List[float] = []
    for token in str(text).replace(";", ",").split(","):
        token = token.strip()
        if not token:
            continue
        try:
            value = float(token)
        except ValueError as exc:
            raise SystemExit("Invalid %s value %r" % (label, token)) from exc
        if not math.isfinite(value):
            raise SystemExit("%s values must be finite" % label)
        values.append(value)
    if not values:
        raise SystemExit("No values supplied for %s" % label)
    return values


def parse_movers(text: str) -> List[str]:
    movers: List[str] = []
    for token in str(text).replace(";", ",").split(","):
        mover = token.strip().upper()
        if not mover:
            continue
        if mover not in ALL_MOVERS:
            raise SystemExit("Unsupported mover %r; allowed values are %s" %
                             (mover, ",".join(ALL_MOVERS)))
        if mover in movers:
            raise SystemExit("Duplicate mover %s" % mover)
        movers.append(mover)
    if not movers:
        raise SystemExit("No movers requested")
    return movers


def exact_grid_count(span_deg: float, resolution_deg: float,
                     label: str) -> int:
    """Require a resolution that closes the periodic or pole-to-pole grid."""

    if not math.isfinite(resolution_deg) or resolution_deg <= 0.0:
        raise SystemExit("%s must be finite and positive" % label)
    count = int(round(span_deg / resolution_deg))
    if count < 1 or abs(count * resolution_deg - span_deg) > 1.0e-8:
        raise SystemExit("%s=%g must divide %g degrees exactly" %
                         (label, resolution_deg, span_deg))
    return count


def expected_coordinate_set(altitudes_km: Sequence[float],
                            lon_resolution_deg: float,
                            lat_resolution_deg: float
                            ) -> set[Tuple[float, float, float]]:
    """Construct the independent shell inventory expected from AMPS."""

    longitude_count = exact_grid_count(360.0, lon_resolution_deg,
                                       "shell longitude resolution")
    latitude_intervals = exact_grid_count(180.0, lat_resolution_deg,
                                          "shell latitude resolution")
    return {
        coordinate_key(altitude, lon_i * lon_resolution_deg,
                       -90.0 + lat_i * lat_resolution_deg)
        for altitude in altitudes_km
        for lon_i in range(longitude_count)
        for lat_i in range(latitude_intervals + 1)
    }


def normalize_variable_name(name: str) -> str:
    """Map Tecplot labels with spaces/units onto stable snake-case names."""

    normalized = name.strip().lower().replace("°", "deg")
    normalized = re.sub(r"[\[\](){}]", "_", normalized)
    normalized = re.sub(r"[^a-z0-9]+", "_", normalized)
    return normalized.strip("_")


def find_column(variables: Sequence[str], aliases: Sequence[str],
                label: str, required: bool = True) -> Optional[int]:
    """Resolve only explicit aliases; positional fallbacks are forbidden."""

    for alias in (normalize_variable_name(item) for item in aliases):
        if alias in variables:
            return variables.index(alias)
    if required:
        raise RuntimeError(
            "Tecplot output lacks required %s column; accepted=%s; available=%s" %
            (label, ",".join(aliases), ",".join(variables)))
    return None


ZONE_ALT_PATTERNS = (
    re.compile(r"alt[_\s-]*km\s*=\s*([0-9eE+\-.]+)", re.IGNORECASE),
    re.compile(r"alt(?:itude)?\s*=\s*([0-9eE+\-.]+)\s*km", re.IGNORECASE),
    re.compile(r"altitude[_\s-]*km\s*[:=]\s*([0-9eE+\-.]+)", re.IGNORECASE),
)


def zone_altitude_km(line: str) -> Optional[float]:
    for pattern in ZONE_ALT_PATTERNS:
        match = pattern.search(line)
        if match:
            return float(match.group(1))
    return None


def parse_tecplot_shell(path: Path) -> ParsedShell:
    """Parse a shell product with strict schema and row-width closure.

    AMPS may write VARIABLES on one line or continue it over quoted lines. The
    old C12 parser understood only the first form and silently skipped malformed
    rows. Both declarations are accepted here, but every numerical row must
    match the declared width exactly.
    """

    variables: List[str] = []
    numeric_records: List[Tuple[int, Optional[float], List[float]]] = []
    reading_variables = False
    current_altitude: Optional[float] = None

    with path.open("r", encoding="utf-8", errors="replace") as stream:
        for line_number, raw in enumerate(stream, start=1):
            line = raw.strip()
            if not line:
                continue
            upper = line.upper()
            if upper.startswith("VARIABLES"):
                if variables:
                    raise RuntimeError("%s contains multiple VARIABLES blocks" % path)
                reading_variables = True
                variables.extend(normalize_variable_name(item)
                                 for item in re.findall(r'"([^"]+)"', line))
                continue
            if reading_variables:
                quoted = re.findall(r'"([^"]+)"', line)
                if quoted and not upper.startswith("ZONE"):
                    variables.extend(normalize_variable_name(item)
                                     for item in quoted)
                    continue
                reading_variables = False
            if upper.startswith("ZONE"):
                current_altitude = zone_altitude_km(line)
                continue
            if upper.startswith(("TITLE", "AUXDATA", "DATASETAUXDATA", "#", "!")):
                continue
            if not variables:
                raise RuntimeError("%s:%d data precedes VARIABLES" %
                                   (path, line_number))
            try:
                values = [float(token)
                          for token in line.replace(",", " ").split()]
            except ValueError as exc:
                raise RuntimeError("%s:%d contains a non-numeric data row" %
                                   (path, line_number)) from exc
            if len(values) != len(variables):
                raise RuntimeError("%s:%d row width %d does not match %d VARIABLES" %
                                   (path, line_number, len(values), len(variables)))
            numeric_records.append((line_number, current_altitude, values))

    if not variables or not numeric_records:
        raise RuntimeError("%s lacks VARIABLES or complete numerical rows" % path)
    if len(set(variables)) != len(variables):
        raise RuntimeError("%s has duplicate normalized variable names" % path)

    lon_index = find_column(variables,
                            ("lon_deg", "longitude_deg", "longitude", "lon"),
                            "longitude")
    lat_index = find_column(variables,
                            ("lat_deg", "latitude_deg", "latitude", "lat"),
                            "latitude")
    alt_index = find_column(variables,
                            ("alt_km", "altitude_km", "altitude"),
                            "altitude", required=False)
    # UPPER_SCAN locates the upper transition. Prefer an explicit upper cutoff,
    # then accept the historical scalar names. rc_effective_gv is last-resort
    # compatibility for current generic shell products.
    cutoff_index = find_column(
        variables,
        ("rc_upper_gv", "rc_num_gv", "cutoff_rigidity_gv", "cutoff_gv",
         "rc_effective_gv"),
        "cutoff rigidity")
    assert lon_index is not None and lat_index is not None and cutoff_index is not None
    cutoff_column = variables[cutoff_index]
    unresolved_names = [name for name in (
        "n_unresolved", "unresolved_count", "lower_bracket_unresolved",
        "upper_bracket_unresolved") if name in variables]
    range_names = [name for name in (
        "lower_below_range", "lower_above_range", "upper_below_range",
        "upper_above_range", "below_range", "above_range") if name in variables]

    rows: List[ShellRow] = []
    seen: set[Tuple[float, float, float]] = set()
    for line_number, zone_altitude, values in numeric_records:
        longitude, latitude = values[lon_index], values[lat_index]
        cutoff = values[cutoff_index]
        if alt_index is not None:
            altitude = values[alt_index]
            if zone_altitude is not None and abs(altitude - zone_altitude) > 1.0e-5:
                raise RuntimeError("%s:%d altitude column disagrees with ZONE" %
                                   (path, line_number))
        elif zone_altitude is not None:
            altitude = zone_altitude
        else:
            raise RuntimeError("%s:%d lacks altitude column/ZONE" %
                               (path, line_number))
        if not all(math.isfinite(value)
                   for value in (altitude, longitude, latitude, cutoff)):
            raise RuntimeError("%s:%d contains a non-finite scientific value" %
                               (path, line_number))
        if not -90.0 - 1.0e-8 <= latitude <= 90.0 + 1.0e-8:
            raise RuntimeError("%s:%d latitude is outside [-90,90]" %
                               (path, line_number))
        if cutoff < 0.0:
            raise RuntimeError("%s:%d contains negative cutoff sentinel %.9g" %
                               (path, line_number, cutoff))
        unresolved = sum(max(0, int(round(values[variables.index(name)])))
                         for name in unresolved_names)
        range_flags = sum(abs(int(round(values[variables.index(name)])))
                          for name in range_names)
        row = ShellRow(altitude, longitude, latitude, cutoff,
                       unresolved, range_flags)
        if row.key in seen:
            raise RuntimeError("%s:%d duplicates coordinate %r" %
                               (path, line_number, row.key))
        seen.add(row.key)
        rows.append(row)
    return ParsedShell(sorted(rows, key=lambda item: item.key), variables,
                       cutoff_column, unresolved_names, range_names)


def validate_shell_coverage(rows: Sequence[ShellRow],
                            expected: set[Tuple[float, float, float]]) -> None:
    """Reject missing, unexpected, or duplicate shell coordinates."""

    actual = {row.key for row in rows}
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing or extra or len(actual) != len(rows):
        raise RuntimeError(
            "shell-grid closure failed: expected=%d actual=%d missing=%d "
            "extra=%d; first_missing=%s first_extra=%s" %
            (len(expected), len(actual), len(missing), len(extra),
             missing[:3], extra[:3]))


def load_reference(path: Path) -> Tuple[List[Dict[str, str]], str]:
    """Validate the immutable, checked-in analytical reference table."""

    required_columns = {
        "mover", "mover_family", "alt_km", "lat_deg", "Rc_stormer_GV",
        "required_for_pass", "abs_tol_GV", "rel_tol", "relative_floor_GV",
        "note",
    }
    with path.open("r", newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if set(reader.fieldnames or ()) != required_columns:
            raise RuntimeError("%s has an incorrect schema" % path)
        rows = list(reader)
    expected_keys = {
        (mover, altitude, latitude)
        for mover in ALL_MOVERS
        for altitude in (9000.0, 25000.0)
        for latitude in (-60.0, -30.0, 0.0, 30.0, 60.0)
    }
    actual_keys = set()
    for line_number, row in enumerate(rows, start=2):
        mover = row["mover"].upper()
        family = row["mover_family"].upper()
        altitude = float(row["alt_km"])
        latitude = float(row["lat_deg"])
        stored = float(row["Rc_stormer_GV"])
        if mover not in ALL_MOVERS:
            raise RuntimeError("%s:%d unsupported mover %s" %
                               (path, line_number, mover))
        expected_family = "GC" if mover in GC_MOVERS else "FULL_ORBIT"
        if family != expected_family:
            raise RuntimeError("%s:%d inconsistent mover family" %
                               (path, line_number))
        key = (mover, altitude, latitude)
        if key in actual_keys:
            raise RuntimeError("%s:%d duplicate reference key %r" %
                               (path, line_number, key))
        actual_keys.add(key)
        computed = stormer_vertical_gv(latitude, altitude)
        if not math.isfinite(stored) or abs(stored - computed) > 5.0e-9:
            raise RuntimeError("%s:%d Stoermer value differs from formula" %
                               (path, line_number))
        required = row["required_for_pass"].strip().upper() in ("T", "TRUE", "1")
        # Stoermer is an analytical reference only for the full-orbit
        # equations. GC rows are retained for transparent diagnostics but are
        # never marked as required analytical comparisons.
        expected_required = expected_family == "FULL_ORBIT"
        if required != expected_required:
            raise RuntimeError("%s:%d inconsistent required_for_pass" %
                               (path, line_number))
        for column in ("abs_tol_GV", "rel_tol", "relative_floor_GV"):
            value = float(row[column])
            if not math.isfinite(value) or value < 0.0:
                raise RuntimeError("%s:%d invalid %s" %
                                   (path, line_number, column))
        if expected_family == "GC" or abs(latitude) >= 50.0:
            expected_relative = 0.25
        elif mover in ("BORIS", "RK2"):
            expected_relative = 0.15
        else:
            expected_relative = 0.05
        if (abs(float(row["abs_tol_GV"]) - 0.005) > 1.0e-12 or
                abs(float(row["relative_floor_GV"]) - 0.05) > 1.0e-12 or
                abs(float(row["rel_tol"]) - expected_relative) > 1.0e-12):
            raise RuntimeError("%s:%d tolerance differs from the default contract" %
                               (path, line_number))
    if actual_keys != expected_keys:
        raise RuntimeError("%s coverage mismatch: missing=%d extra=%d" %
                           (path, len(expected_keys - actual_keys),
                            len(actual_keys - expected_keys)))
    return rows, sha256_file(path)


def load_acceptance_contract(path: Path) -> Tuple[List[Dict[str, str]], str]:
    required = {"gate_id", "classification", "scope", "default", "description"}
    with path.open("r", newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if set(reader.fieldnames or ()) != required:
            raise RuntimeError("%s has an incorrect acceptance-contract schema" % path)
        rows = list(reader)
    identifiers = [row["gate_id"] for row in rows]
    if not rows or len(identifiers) != len(set(identifiers)):
        raise RuntimeError("%s has no rows or duplicate gate identifiers" % path)
    if any(row["classification"] not in ("HARD", "DIAGNOSTIC") for row in rows):
        raise RuntimeError("%s has an invalid gate classification" % path)
    return rows, sha256_file(path)


def format_number(value: float) -> str:
    return "%.15g" % float(value)


def dt_label(value: float) -> str:
    return "dt_" + format_number(value).replace("-", "m").replace(".", "p")


def build_run_specs(movers: Sequence[str], base_dt: float,
                    dt_scales: Sequence[float]) -> List[RunSpec]:
    """Build convergence cases for full movers and one finest GC case."""

    ordered_scales = sorted(set(float(value) for value in dt_scales), reverse=True)
    finest_scale = min(ordered_scales)
    specs: List[RunSpec] = []
    for mover in movers:
        family = "GC" if mover in GC_MOVERS else "FULL_ORBIT"
        mover_scales = [finest_scale] if family == "GC" else ordered_scales
        for scale in mover_scales:
            dt_seconds = base_dt * scale
            specs.append(RunSpec(mover, family, scale, dt_seconds,
                                 dt_label(dt_seconds),
                                 abs(scale - finest_scale) <= 1.0e-14))
    return specs


def render_input(template: Path, destination: Path, spec: RunSpec,
                 args: argparse.Namespace) -> None:
    """Render one parser-safe AMPS input with explicit kilometre geometry."""

    domain_half_km = args.domain_half_size_re * EARTH_RADIUS_KM
    replacements = {
        "__CUTOFF_EMIN_MEV__": format_number(args.cutoff_emin),
        "__CUTOFF_EMAX_MEV__": format_number(args.cutoff_emax),
        "__MAX_TRACE_TIME__": format_number(args.max_trace_time),
        "__DOMAIN_MIN_KM__": format_number(-domain_half_km),
        "__DOMAIN_MAX_KM__": format_number(domain_half_km),
        "__R_INNER_KM__": format_number(args.r_inner_km),
        "__SHELL_COUNT__": "2",
        "__SHELL_ALTS_KM__": "%s %s" %
            (format_number(args.full_alt), format_number(args.gc_alt)),
        "__SHELL_LON_RES_DEG__": format_number(args.shell_lon_res_deg),
        "__SHELL_LAT_RES_DEG__": format_number(args.shell_lat_res_deg),
        "__DT_TRACE__": format_number(spec.dt_seconds),
        "__ADAPTIVE_DT__": args.adaptive_dt,
        "__MAX_STEPS__": str(args.max_steps),
        "__MAX_TRACE_DISTANCE_RE__": format_number(args.max_trace_distance),
        "__MOVER__": spec.mover,
        "__DT_SCALE__": format_number(spec.dt_scale),
    }
    text = template.read_text(encoding="utf-8")
    for token, replacement in replacements.items():
        text = text.replace(token, replacement)
    unresolved = sorted(set(re.findall(r"__[A-Z0-9_]+__", text)))
    if unresolved:
        raise RuntimeError("Unresolved C12 input tokens: %s" % ", ".join(unresolved))
    destination.write_text(text, encoding="utf-8")


def build_command(spec: RunSpec, args: argparse.Namespace,
                  amps_path: Path) -> List[str]:
    """Return the exact GRIDLESS cutoff command for one case."""

    command = [args.mpirun]
    command.extend(args.mpirun_arg)
    command += [
        "-np", str(args.np), str(amps_path), "-mode", "gridless",
        "-i", "AMPS_PARAM_C12.in", "-mover", spec.mover,
        "-gridless-parallel", "THREADS", "-gridless-threads", str(args.nt),
        "-gridless-mpi-scheduler", args.scheduler,
        "-gridless-mpi-dynamic-chunk", str(args.dynamic_chunk),
        "-cutoff-search", "UPPER_SCAN",
        "-cutoff-upper-scan-n", str(args.cutoff_scan_n),
        "-cutoff-trace-policy", "ACCURATE",
    ]
    return command


def run_command(command: Sequence[str], cwd: Path,
                log_path: Path) -> Tuple[int, float]:
    started = time.monotonic()
    with log_path.open("w", encoding="utf-8") as log:
        log.write("Command:\n  %s\n\n" % " ".join(command))
        log.flush()
        process = subprocess.Popen(
            list(command), cwd=str(cwd), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, universal_newlines=True)
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            log.write(line)
        return_code = process.wait()
    return return_code, time.monotonic() - started


def parse_mpi_task_closure(log_path: Path) -> Tuple[Optional[int], Optional[int]]:
    """Read scheduler task-closure diagnostics when AMPS reports them."""

    if not log_path.is_file():
        return None, None
    text = log_path.read_text(encoding="utf-8", errors="replace")
    total_match = re.search(r"totalTasks\s*\(expected\)\s*=\s*(\d+)", text)
    sum_match = re.search(r"sum\(all rank tasks\)\s*=\s*(\d+)", text)
    return ((int(total_match.group(1)) if total_match else None),
            (int(sum_match.group(1)) if sum_match else None))


def find_output(case_dir: Path, explicit_name: Optional[str]) -> Path:
    if explicit_name:
        path = Path(explicit_name)
        return path if path.is_absolute() else case_dir / path
    preferred = (
        case_dir / "cutoff_gridless_shells_dipole_compare.dat",
        case_dir / "cutoff_gridless_shells.dat",
        case_dir / "cutoff_gridless_shells_penumbra.dat",
    )
    for candidate in preferred:
        if candidate.is_file():
            return candidate
    matches = sorted(case_dir.glob("cutoff*gridless*shell*.dat"))
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        raise RuntimeError("ambiguous shell products; use --output-file: %s" %
                           ", ".join(path.name for path in matches))
    raise RuntimeError("no GRIDLESS shell cutoff product in %s" % case_dir)


def tolerance_for_coordinate(mover: str, latitude: float,
                             args: argparse.Namespace) -> Tuple[float, float, float]:
    """Return the declared accuracy envelope for one mover/latitude.

    BORIS and RK2 are intentionally assigned a wider mid-latitude envelope
    than RK4/RK6. This preserves their value as independent lower-order
    regressions without weakening the high-order analytical validation.
    GC values are diagnostic only, so their tolerance is used for reporting
    and never establishes full-orbit physical correctness.
    """

    if mover in GC_MOVERS:
        return args.gc_abs_tol_gv, args.gc_rel_tol, args.relative_floor_gv
    if abs(latitude) >= args.high_lat_threshold_deg:
        relative = args.stormer_high_lat_rel_tol
    elif mover in ("BORIS", "RK2"):
        relative = args.low_order_stormer_rel_tol
    else:
        relative = args.stormer_rel_tol
    return args.stormer_abs_tol_gv, relative, args.relative_floor_gv


def science_required(spec: RunSpec, altitude: float, latitude: float,
                     args: argparse.Namespace) -> bool:
    """Identify rows that are valid analytical full-orbit comparisons."""

    if not spec.is_finest:
        return False
    # A guiding-center trajectory is a different reduced model, even at the
    # outward shell. Comparing it with Stoermer remains informative, but it
    # cannot be a hard analytical gate for the GC implementation.
    return spec.family == "FULL_ORBIT"


def case_required_for_pass(spec: RunSpec, args: argparse.Namespace) -> bool:
    """Return whether structural/execution failure of this case is fatal."""

    if spec.family == "FULL_ORBIT":
        return True
    return args.gc_policy == "REQUIRE"


def evaluate_case(result: CaseResult, parsed: ParsedShell,
                  expected: set[Tuple[float, float, float]],
                  args: argparse.Namespace) -> None:
    """Apply structural closure and the mover-family scientific contract."""

    validate_shell_coverage(parsed.rows, expected)
    result.parsed_rows = len(parsed.rows)
    result.expected_rows = len(expected)
    result.cutoff_column = parsed.cutoff_column
    result.cutoff_map = {row.key: row.cutoff_gv for row in parsed.rows}
    unresolved_rows = [row for row in parsed.rows if row.unresolved_count > 0]
    range_rows = [row for row in parsed.rows if row.range_flag_count > 0]
    if unresolved_rows:
        result.passed = False
        result.messages.append("%d rows report unresolved brackets" %
                               len(unresolved_rows))
    if range_rows:
        result.passed = False
        result.messages.append("%d rows report cutoff range flags" % len(range_rows))

    by_key = {row.key: row for row in parsed.rows}
    longitude_count = exact_grid_count(360.0, args.shell_lon_res_deg,
                                       "shell longitude resolution")
    for altitude in (args.full_alt, args.gc_alt):
        for latitude in args.target_lats:
            rows = [by_key[coordinate_key(altitude,
                                          index * args.shell_lon_res_deg,
                                          latitude)]
                    for index in range(longitude_count)]
            reference = stormer_vertical_gv(latitude, altitude)
            abs_errors = [abs(row.cutoff_gv - reference) for row in rows]
            signed_errors = [row.cutoff_gv - reference for row in rows]
            abs_tol, rel_tol, floor = tolerance_for_coordinate(
                result.spec.mover, latitude, args)
            allowed = abs_tol + rel_tol * max(reference, floor)
            required = science_required(result.spec, altitude, latitude, args)
            passed = max(abs_errors) <= allowed
            if required and not passed:
                result.passed = False
                result.messages.append(
                    "%s Stoermer gate failed at alt=%g km lat=%g deg: "
                    "max_abs=%.6g GV allowed=%.6g GV" %
                    (result.spec.mover, altitude, latitude,
                     max(abs_errors), allowed))
            status = (("PASS" if passed else "FAIL") if required else
                      ("CONVERGENCE_SAMPLE" if not result.spec.is_finest
                       else "DIAGNOSTIC_ONLY"))
            result.summary_rows.append({
                "case": "%s/%s" % (result.spec.mover, result.spec.label),
                "mover": result.spec.mover,
                "mover_family": result.spec.family,
                "dt_seconds": result.spec.dt_seconds,
                "alt_km": altitude,
                "lat_deg": latitude,
                "n_lon": len(rows),
                "Rc_reference_GV": reference,
                "Rc_mean_GV": sum(row.cutoff_gv for row in rows) / len(rows),
                "Rc_min_GV": min(row.cutoff_gv for row in rows),
                "Rc_max_GV": max(row.cutoff_gv for row in rows),
                "mean_signed_error_GV": sum(signed_errors) / len(signed_errors),
                "max_abs_error_GV": max(abs_errors),
                "allowed_error_GV": allowed,
                "required_for_pass": required,
                "status": status,
            })


def target_coordinate_keys(args: argparse.Namespace) -> List[Tuple[float, float, float]]:
    """Return all configured longitude/target-latitude keys on both shells."""

    longitude_count = exact_grid_count(360.0, args.shell_lon_res_deg,
                                       "shell longitude resolution")
    keys = []
    for altitude in (args.full_alt, args.gc_alt):
        for latitude in args.target_lats:
            for index in range(longitude_count):
                keys.append(coordinate_key(altitude,
                                           index * args.shell_lon_res_deg,
                                           latitude))
    return keys


def normalized_map_difference(left: Mapping[Tuple[float, float, float], float],
                              right: Mapping[Tuple[float, float, float], float],
                              keys: Sequence[Tuple[float, float, float]],
                              floor_gv: float) -> float:
    """RMS map difference normalized by the local analytical cutoff scale."""

    terms = []
    for altitude, longitude, latitude in keys:
        key = coordinate_key(altitude, longitude, latitude)
        scale = max(stormer_vertical_gv(latitude, altitude), floor_gv)
        terms.append(((left[key] - right[key]) / scale) ** 2)
    return math.sqrt(sum(terms) / len(terms))


def evaluate_convergence(case_results: Sequence[CaseResult],
                         args: argparse.Namespace
                         ) -> Tuple[List[Dict[str, object]], bool, List[str]]:
    """Apply a classifier-aware three-level self-convergence check.

    Strict formal order estimates are unstable when a cutoff transition crosses
    a search tolerance. Instead, the fine/medium RMS map difference must not be
    materially larger than the medium/coarse difference. An additive slack
    prevents roundoff-level changes from becoming false failures.
    """

    rows: List[Dict[str, object]] = []
    messages: List[str] = []
    passed = True
    keys = target_coordinate_keys(args)
    for mover in FULL_ORBIT_MOVERS:
        cases = [case for case in case_results
                 if case.spec.mover == mover and case.cutoff_map]
        cases.sort(key=lambda case: case.spec.dt_seconds, reverse=True)
        if len(cases) < 3:
            continue
        differences = [
            normalized_map_difference(cases[index].cutoff_map,
                                      cases[index + 1].cutoff_map,
                                      keys, args.relative_floor_gv)
            for index in range(len(cases) - 1)]
        for index in range(len(differences) - 1):
            coarse_difference, fine_difference = differences[index:index + 2]
            allowed = (args.convergence_ratio_limit * coarse_difference +
                       args.convergence_abs_slack)
            ok = fine_difference <= allowed
            if args.require_convergence and not ok:
                passed = False
                messages.append(
                    "%s self-convergence failed: fine RMS %.6g > %.6g "
                    "(coarse RMS %.6g)" %
                    (mover, fine_difference, allowed, coarse_difference))
            rows.append({
                "mover": mover,
                "coarse_dt_seconds": cases[index].spec.dt_seconds,
                "medium_dt_seconds": cases[index + 1].spec.dt_seconds,
                "fine_dt_seconds": cases[index + 2].spec.dt_seconds,
                "coarse_medium_normalized_rms": coarse_difference,
                "medium_fine_normalized_rms": fine_difference,
                "allowed_medium_fine_rms": allowed,
                "required_for_pass": args.require_convergence,
                "passed": ok,
            })
    return rows, passed, messages


def evaluate_cross_mover(case_results: Sequence[CaseResult],
                         args: argparse.Namespace
                         ) -> Tuple[List[Dict[str, object]], bool, List[str]]:
    """Apply a hard high-order parity gate and wider diagnostic comparisons.

    RK4/RK6 are the only default hard parity group. Mixing BORIS and RK2 into
    that tight gate confounds expected method-order differences with dispatch
    errors. The wider all-full-orbit comparison is still written so changes
    in lower-order behavior remain visible. GC parity is never a Stoermer
    validation and becomes hard only under ``--gc-policy REQUIRE``.
    """

    rows: List[Dict[str, object]] = []
    messages: List[str] = []
    passed = True
    finest = {case.spec.mover: case for case in case_results
              if case.spec.is_finest and case.cutoff_map}
    full_keys = target_coordinate_keys(args)
    groups = (
        ("FULL_ORBIT_HIGH_ORDER", [m for m in ("RK4", "RK6") if m in finest],
         full_keys, args.full_cross_abs_tol_gv, args.full_cross_rel_tol,
         args.full_cross_high_lat_rel_tol, args.enforce_cross_mover),
        ("FULL_ORBIT_ALL", [m for m in FULL_ORBIT_MOVERS if m in finest],
         full_keys, args.stormer_abs_tol_gv, args.low_order_cross_rel_tol,
         args.stormer_high_lat_rel_tol, False),
        ("GC_CAPABILITY", [m for m in GC_MOVERS if m in finest],
         full_keys, args.gc_cross_abs_tol_gv, args.gc_cross_rel_tol,
         args.gc_cross_rel_tol,
         args.enforce_cross_mover and args.gc_policy == "REQUIRE"),
    )
    for (family, movers, keys, abs_tol, mid_rel_tol, high_rel_tol,
         required) in groups:
        if len(movers) < 2:
            continue
        grouped: Dict[Tuple[float, float], List[Tuple[float, float]]] = {}
        for altitude, longitude, latitude in keys:
            key = coordinate_key(altitude, longitude, latitude)
            values = [finest[mover].cutoff_map[key] for mover in movers]
            spread = max(values) - min(values)
            grouped.setdefault((altitude, latitude), []).append((spread, longitude))
        for (altitude, latitude), spreads in sorted(grouped.items()):
            reference = stormer_vertical_gv(latitude, altitude)
            rel_tol = (high_rel_tol
                       if abs(latitude) >= args.high_lat_threshold_deg
                       else mid_rel_tol)
            allowed = abs_tol + rel_tol * max(reference, args.relative_floor_gv)
            maximum, worst_longitude = max(spreads)
            ok = maximum <= allowed
            if required and not ok:
                passed = False
                messages.append(
                    "%s mover parity failed at alt=%g km lat=%g deg: "
                    "spread=%.6g GV allowed=%.6g GV" %
                    (family, altitude, latitude, maximum, allowed))
            rows.append({
                "mover_family": family, "movers": ",".join(movers),
                "alt_km": altitude, "lat_deg": latitude,
                "worst_lon_deg": worst_longitude,
                "max_spread_GV": maximum, "allowed_spread_GV": allowed,
                "required_for_pass": required, "passed": ok,
            })
    return rows, passed, messages


def write_csv(path: Path, rows: Sequence[Mapping[str, object]],
              fieldnames: Optional[Sequence[str]] = None) -> None:
    if not rows and fieldnames is None:
        return
    columns = list(fieldnames or rows[0].keys())
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        for row in rows:
            writer.writerow({column: row.get(column, "") for column in columns})


def write_reference_used(path: Path, movers: Sequence[str],
                         args: argparse.Namespace) -> None:
    """Write a run-local derived table without modifying the source fixture."""

    rows: List[Dict[str, object]] = []
    for mover in movers:
        family = "GC" if mover in GC_MOVERS else "FULL_ORBIT"
        for altitude in (args.full_alt, args.gc_alt):
            for latitude in args.target_lats:
                abs_tol, rel_tol, floor = tolerance_for_coordinate(
                    mover, latitude, args)
                reference = stormer_vertical_gv(latitude, altitude)
                required = family == "FULL_ORBIT"
                rows.append({
                    "mover": mover, "mover_family": family,
                    "alt_km": altitude, "lat_deg": latitude,
                    "Rc_stormer_GV": reference,
                    "required_for_pass": required, "abs_tol_GV": abs_tol,
                    "rel_tol": rel_tol, "relative_floor_GV": floor,
                    "allowed_error_GV": abs_tol + rel_tol * max(reference, floor),
                })
    write_csv(path, rows)


def safe_replace_workdir(path: Path, launch_dir: Path, keep: bool) -> None:
    """Remove only a narrow, explicitly named test-output directory."""

    forbidden = {Path("/").resolve(), launch_dir.resolve(), SCRIPT_DIR.resolve(),
                 SCRIPT_DIR.parent.resolve()}
    if path.resolve() in forbidden or len(path.resolve().parts) < 4:
        raise SystemExit("Refusing unsafe C12 work directory: %s" % path)
    if path.exists() and not keep:
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)


def resolve_profile(args: argparse.Namespace) -> None:
    defaults = PROFILE_DEFAULTS[args.profile]
    args.movers = parse_movers(args.movers or defaults["movers"])
    args.dt_scales = parse_csv_floats(args.dt_scales or defaults["dt_scales"],
                                      "--dt-scales")
    if any(value <= 0.0 for value in args.dt_scales):
        raise SystemExit("--dt-scales values must be positive")
    if len(args.dt_scales) != len(set(args.dt_scales)):
        raise SystemExit("--dt-scales values must be unique")
    if args.cutoff_scan_n is None:
        args.cutoff_scan_n = defaults["cutoff_scan_n"]
    if args.max_trace_time is None:
        args.max_trace_time = defaults["max_trace_time"]
    if args.require_convergence is None:
        args.require_convergence = defaults["require_convergence"]
    selected_gc = [mover for mover in args.movers if mover in GC_MOVERS]
    if selected_gc and args.gc_policy == "SKIP":
        raise SystemExit(
            "GC movers were selected but --gc-policy is SKIP; use "
            "--gc-policy DIAGNOSTIC to record capability results or REQUIRE "
            "to make GC execution/schema/resolution failures fatal")
    if args.shell_res is not None:
        args.shell_lon_res_deg = args.shell_res
        args.shell_lat_res_deg = args.shell_res
    args.target_lats = parse_csv_floats(args.lats, "--lats")
    if len(args.target_lats) != len(set(args.target_lats)):
        raise SystemExit("--lats values must be unique")
    if any(abs(value) > 90.0 for value in args.target_lats):
        raise SystemExit("--lats values must be within [-90,90]")
    if not 0.0 <= args.high_lat_threshold_deg <= 90.0:
        raise SystemExit("--high-lat-threshold-deg must be within [0,90]")
    exact_grid_count(360.0, args.shell_lon_res_deg, "--shell-lon-res-deg")
    lat_intervals = exact_grid_count(180.0, args.shell_lat_res_deg,
                                     "--shell-lat-res-deg")
    grid_lats = [-90.0 + i * args.shell_lat_res_deg
                 for i in range(lat_intervals + 1)]
    for latitude in args.target_lats:
        if not any(abs(latitude - item) <= 1.0e-8 for item in grid_lats):
            raise SystemExit("latitude %g is not on the %.9g-degree shell grid" %
                             (latitude, args.shell_lat_res_deg))
    for name in ("np", "nt", "base_dt", "cutoff_emin", "cutoff_emax",
                 "max_trace_time", "max_trace_distance", "max_steps",
                 "domain_half_size_re", "r_inner_km", "cutoff_scan_n",
                 "relative_floor_gv"):
        if float(getattr(args, name)) <= 0.0:
            raise SystemExit("--%s must be positive" % name.replace("_", "-"))
    for name in ("stormer_abs_tol_gv", "stormer_rel_tol",
                 "low_order_stormer_rel_tol",
                 "stormer_high_lat_rel_tol", "gc_abs_tol_gv", "gc_rel_tol",
                 "full_cross_abs_tol_gv", "full_cross_rel_tol",
                 "full_cross_high_lat_rel_tol", "low_order_cross_rel_tol",
                 "gc_cross_abs_tol_gv", "gc_cross_rel_tol",
                 "convergence_ratio_limit", "convergence_abs_slack"):
        value = float(getattr(args, name))
        if not math.isfinite(value) or value < 0.0:
            raise SystemExit("--%s must be finite and nonnegative" %
                             name.replace("_", "-"))
    if args.cutoff_emin >= args.cutoff_emax:
        raise SystemExit("--cutoff-emin must be below --cutoff-emax")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be nonnegative")
    if args.full_alt <= 0.0 or args.gc_alt <= 0.0:
        raise SystemExit("shell altitudes must be positive")
    if abs(args.full_alt - args.gc_alt) <= 1.0e-6:
        raise SystemExit("--full-alt and --gc-alt must identify different shells")
    maximum_radius_km = EARTH_RADIUS_KM + max(args.full_alt, args.gc_alt)
    if args.domain_half_size_re * EARTH_RADIUS_KM <= maximum_radius_km:
        raise SystemExit("outer box must enclose both shells")
    if not 0.0 < args.r_inner_km < EARTH_RADIUS_KM + min(args.full_alt, args.gc_alt):
        raise SystemExit("--r-inner-km must be positive and inside both shells")
    if args.require_convergence:
        if args.adaptive_dt != "F":
            raise SystemExit(
                "hard time-step convergence requires --adaptive-dt F; with "
                "adaptive stepping, DT_TRACE may be an inactive cap and the "
                "nominal refinement sequence is not an order test")
        selected_full = [m for m in args.movers if m in FULL_ORBIT_MOVERS]
        if selected_full and len(args.dt_scales) < 3:
            raise SystemExit("hard convergence requires at least three --dt-scales")
    minimum_rigidity = rigidity_from_kinetic_mev(args.cutoff_emin)
    maximum_rigidity = rigidity_from_kinetic_mev(args.cutoff_emax)
    references = [stormer_vertical_gv(latitude, altitude)
                  for altitude in (args.full_alt, args.gc_alt)
                  for latitude in args.target_lats]
    if minimum_rigidity >= min(references):
        raise SystemExit("lower rigidity %.6g is not below reference %.6g" %
                         (minimum_rigidity, min(references)))
    if maximum_rigidity <= max(references):
        raise SystemExit("upper rigidity %.6g is not above reference %.6g" %
                         (maximum_rigidity, max(references)))


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="C12 strict particle-mover validation in an analytical dipole",
        epilog=textwrap.dedent("""
        Recommended routine validation from the AMPS repository root:
          python3 srcEarth/test/C12/run_C12.py --profile ROUTINE --amps ./amps -np 4 -nt 16

        Offline package and damaged-output checks:
          python3 srcEarth/test/C12/run_C12.py --self-test

        Reanalyze completed case directories without launching AMPS:
          python3 srcEarth/test/C12/run_C12.py --profile ROUTINE --skip-run
        """))
    parser.add_argument("--profile", choices=sorted(PROFILE_DEFAULTS), default="ROUTINE")
    parser.add_argument("--movers", default=None)
    parser.add_argument("--dt-scales", default=None)
    parser.add_argument("--base-dt", type=float, default=1.0)
    parser.add_argument("--adaptive-dt", choices=("T", "F"), default="T")
    parser.add_argument(
        "--gc-policy", choices=("SKIP", "DIAGNOSTIC", "REQUIRE"),
        default="SKIP",
        help="GC contract: exclude by default, record nonfatal capability "
             "results, or require GC execution/schema/resolution")
    parser.add_argument("-np", type=int, default=4)
    parser.add_argument("-nt", type=int, default=16)
    parser.add_argument("--amps", default="./amps")
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("--mpirun-arg", action="append", default=[])
    parser.add_argument("--scheduler", choices=("DYNAMIC", "BLOCK_CYCLIC", "STATIC"), default="DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0)
    parser.add_argument("--workdir", default="test_output/C12_gridless")
    parser.add_argument("--output-file", default=None)
    parser.add_argument("--skip-run", action="store_true")
    parser.add_argument("--keep", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--validate-references", action="store_true")
    parser.add_argument("--full-alt", type=float, default=9000.0)
    parser.add_argument("--gc-alt", type=float, default=25000.0)
    parser.add_argument("--lats", default="-60,-30,0,30,60")
    parser.add_argument("--shell-lon-res-deg", type=float, default=30.0)
    parser.add_argument("--shell-lat-res-deg", type=float, default=15.0)
    parser.add_argument("--shell-res", type=float, default=None,
                        help="legacy override applied to both shell resolutions")
    parser.add_argument("--domain-half-size-re", type=float, default=35.0)
    parser.add_argument("--r-inner-km", type=float, default=EARTH_RADIUS_KM)
    parser.add_argument("--cutoff-emin", type=float, default=0.05, help="MeV/n")
    parser.add_argument("--cutoff-emax", type=float, default=20000.0, help="MeV/n")
    parser.add_argument("--cutoff-scan-n", type=int, default=None)
    parser.add_argument("--max-trace-time", type=float, default=None)
    parser.add_argument("--max-trace-distance", type=float, default=400.0, help="Re")
    parser.add_argument("--max-steps", type=int, default=4000000)
    parser.add_argument("--stormer-abs-tol-gv", type=float, default=0.005)
    parser.add_argument("--stormer-rel-tol", type=float, default=0.05)
    parser.add_argument("--low-order-stormer-rel-tol", type=float, default=0.15)
    parser.add_argument("--stormer-high-lat-rel-tol", type=float, default=0.25)
    parser.add_argument("--high-lat-threshold-deg", type=float, default=50.0)
    parser.add_argument("--relative-floor-gv", type=float, default=0.05)
    parser.add_argument("--gc-abs-tol-gv", type=float, default=0.005)
    parser.add_argument("--gc-rel-tol", type=float, default=0.25)
    parser.add_argument("--full-cross-abs-tol-gv", type=float, default=0.003)
    parser.add_argument("--full-cross-rel-tol", type=float, default=0.03)
    parser.add_argument("--full-cross-high-lat-rel-tol", type=float, default=0.10)
    parser.add_argument("--low-order-cross-rel-tol", type=float, default=0.15)
    parser.add_argument("--gc-cross-abs-tol-gv", type=float, default=0.003)
    parser.add_argument("--gc-cross-rel-tol", type=float, default=0.10)
    parser.add_argument("--convergence-ratio-limit", type=float, default=1.25)
    parser.add_argument("--convergence-abs-slack", type=float, default=0.01)
    parser.set_defaults(require_convergence=None, enforce_cross_mover=True)
    parser.add_argument("--require-convergence", dest="require_convergence", action="store_true")
    parser.add_argument("--no-require-convergence", dest="require_convergence", action="store_false")
    parser.add_argument("--enforce-cross-mover", dest="enforce_cross_mover", action="store_true")
    parser.add_argument("--no-enforce-cross-mover", dest="enforce_cross_mover", action="store_false")
    return parser.parse_args(argv)


def make_synthetic_tecplot(path: Path, altitudes: Sequence[float],
                           lon_resolution: float, lat_resolution: float,
                           *, duplicate: bool = False,
                           nonfinite: bool = False,
                           truncate: bool = False) -> None:
    """Create a modern-schema fixture used only by the built-in self-test."""

    longitude_count = exact_grid_count(360.0, lon_resolution, "synthetic longitude")
    latitude_intervals = exact_grid_count(180.0, lat_resolution, "synthetic latitude")
    lines = ['TITLE = "C12 synthetic"', "VARIABLES =", '  "lon_deg"',
             '  "lat_deg"', '  "rc_upper_gv"', '  "n_unresolved"']
    for altitude in altitudes:
        lines.append('ZONE T="Alt_km=%g"' % altitude)
        for lat_index in range(latitude_intervals + 1):
            latitude = -90.0 + lat_index * lat_resolution
            for lon_index in range(longitude_count):
                longitude = lon_index * lon_resolution
                cutoff = stormer_vertical_gv(latitude, altitude)
                value = "nan" if nonfinite else "%.17g" % cutoff
                lines.append("%.17g %.17g %s 0" %
                             (longitude, latitude, value))
    if truncate:
        lines.pop()
    if duplicate:
        lines.append(lines[-1])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_self_test() -> int:
    """Exercise references, parser rejection paths, coverage, and commands."""

    source_reference_hash = sha256_file(REFERENCE_PATH)
    reference_rows, reference_hash = load_reference(REFERENCE_PATH)
    contract_rows, contract_hash = load_acceptance_contract(CONTRACT_PATH)
    checks: List[Tuple[str, bool]] = [
        ("reference_schema_formula_coverage", len(reference_rows) == 70),
        ("acceptance_contract", len(contract_rows) >= 8),
        ("reference_hash_available", bool(reference_hash and contract_hash)),
        ("stormer_equatorial_anchor",
         abs(stormer_vertical_gv(0.0, 0.0) - STORMER_R0_GV) < 1.0e-12),
    ]
    with tempfile.TemporaryDirectory(prefix="C12_self_test_") as temporary:
        root = Path(temporary)
        altitudes = (9000.0, 25000.0)
        expected = expected_coordinate_set(altitudes, 180.0, 90.0)
        good = root / "good.dat"
        make_synthetic_tecplot(good, altitudes, 180.0, 90.0)
        parsed = parse_tecplot_shell(good)
        validate_shell_coverage(parsed.rows, expected)
        checks.append(("multiline_variables_and_modern_rc_schema",
                       len(parsed.rows) == len(expected) and
                       parsed.cutoff_column == "rc_upper_gv"))
        for name, options, fragment in (
                ("truncated", {"truncate": True}, "closure failed"),
                ("duplicate", {"duplicate": True}, "duplicates coordinate"),
                ("nonfinite", {"nonfinite": True}, "non-finite")):
            fixture = root / (name + ".dat")
            make_synthetic_tecplot(fixture, altitudes, 180.0, 90.0, **options)
            rejected = False
            try:
                candidate = parse_tecplot_shell(fixture)
                validate_shell_coverage(candidate.rows, expected)
            except RuntimeError as error:
                rejected = fragment in str(error)
            checks.append((name + "_rejected", rejected))
        args = parse_args(["--profile", "ROUTINE", "--movers", "BORIS,RK6,GC4",
                           "--gc-policy", "DIAGNOSTIC"])
        resolve_profile(args)
        specs = build_run_specs(args.movers, args.base_dt, args.dt_scales)
        rendered = root / "AMPS_PARAM_C12.in"
        render_input(INPUT_TEMPLATE, rendered, specs[0], args)
        rendered_text = rendered.read_text(encoding="utf-8")
        checks.append(("kilometre_domain",
                       re.search(r"^DOMAIN_X_MAX\s+222992$", rendered_text,
                                 re.MULTILINE) is not None and
                       re.search(r"^R_INNER\s+6371\.2$", rendered_text,
                                 re.MULTILINE) is not None))
        joined = " ".join(build_command(specs[0], args, Path("/tmp/amps")))
        checks.append(("gridless_thread_flags",
                       "-gridless-parallel THREADS" in joined and
                       "-gridless-threads 16" in joined and
                       "-density-parallel" not in joined))
        checks.append(("routine_case_matrix", len(specs) == 7))
        routine_args = parse_args(["--profile", "ROUTINE"])
        resolve_profile(routine_args)
        routine_specs = build_run_specs(routine_args.movers,
                                        routine_args.base_dt,
                                        routine_args.dt_scales)
        checks.append(("routine_defaults_to_12_full_orbit_cases",
                       len(routine_specs) == 12 and
                       all(spec.family == "FULL_ORBIT"
                           for spec in routine_specs)))
        checks.append(("gc_stormer_is_never_a_hard_gate",
                       not science_required(specs[-1], 25000.0, 60.0, args)))
        checks.append(("gc_diagnostic_case_is_nonfatal",
                       not case_required_for_pass(specs[-1], args)))
        required_gc_args = parse_args(["--profile", "SMOKE", "--movers", "GC4",
                                       "--gc-policy", "REQUIRE"])
        resolve_profile(required_gc_args)
        required_gc_spec = build_run_specs(
            required_gc_args.movers, required_gc_args.base_dt,
            required_gc_args.dt_scales)[0]
        checks.append(("gc_require_makes_capability_case_fatal",
                       case_required_for_pass(required_gc_spec,
                                              required_gc_args)))
        low_order = tolerance_for_coordinate("BORIS", 30.0, args)[1]
        high_order = tolerance_for_coordinate("RK4", 30.0, args)[1]
        checks.append(("mover_accuracy_classes",
                       abs(low_order - 0.15) < 1.0e-15 and
                       abs(high_order - 0.05) < 1.0e-15))
        # A hard convergence label is valid only when the requested DT_TRACE
        # values are the actual fixed integration steps. Confirm that the CLI
        # refuses to overstate adaptive-cap sensitivity as an order test.
        rejected_adaptive_hard_gate = False
        try:
            invalid_args = parse_args(["--profile", "ROUTINE",
                                       "--require-convergence"])
            resolve_profile(invalid_args)
        except SystemExit as error:
            rejected_adaptive_hard_gate = "--adaptive-dt F" in str(error)
        checks.append(("adaptive_hard_convergence_rejected",
                       rejected_adaptive_hard_gate))
    checks.append(("source_reference_immutable",
                   sha256_file(REFERENCE_PATH) == source_reference_hash))
    failed = [name for name, passed in checks if not passed]
    for name, passed in checks:
        print("%-48s %s" % (name, "PASS" if passed else "FAIL"))
    print("C12 self-test: %s (%d checks)" %
          ("PASS" if not failed else "FAIL", len(checks)))
    if failed:
        print("Failed checks: %s" % ", ".join(failed))
        return 1
    return 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        return run_self_test()
    resolve_profile(args)
    reference_rows, reference_hash = load_reference(REFERENCE_PATH)
    contract_rows, contract_hash = load_acceptance_contract(CONTRACT_PATH)
    if args.validate_references:
        print("C12 references: PASS")
        print("  analytical rows: %d" % len(reference_rows))
        print("  acceptance gates: %d" % len(contract_rows))
        print("  reference sha256: %s" % reference_hash)
        print("  contract sha256:  %s" % contract_hash)
        return 0

    launch_dir = Path.cwd().resolve()
    work_root = Path(args.workdir)
    if not work_root.is_absolute():
        work_root = launch_dir / work_root
    work_root = work_root.resolve()
    if args.skip_run:
        if not work_root.is_dir():
            raise SystemExit("--skip-run work directory does not exist: %s" % work_root)
    else:
        safe_replace_workdir(work_root, launch_dir, args.keep)

    # References are copied into results and never regenerated in source.
    if not args.skip_run:
        shutil.copy2(REFERENCE_PATH, work_root / REFERENCE_PATH.name)
        shutil.copy2(CONTRACT_PATH, work_root / CONTRACT_PATH.name)
    write_reference_used(work_root / "C12_reference_used.csv", args.movers, args)

    amps_path = Path(args.amps)
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()
    specs = build_run_specs(args.movers, args.base_dt, args.dt_scales)
    expected = expected_coordinate_set((args.full_alt, args.gc_alt),
                                       args.shell_lon_res_deg,
                                       args.shell_lat_res_deg)
    case_results: List[CaseResult] = []
    all_messages: List[str] = []
    for spec in specs:
        case_dir = work_root / spec.mover.lower() / spec.label
        if not args.skip_run:
            case_dir.mkdir(parents=True, exist_ok=True)
            render_input(INPUT_TEMPLATE, case_dir / "AMPS_PARAM_C12.in", spec, args)
        command = build_command(spec, args, amps_path)
        log_path = case_dir / "C12_amps.log"
        result = CaseResult(spec, str(case_dir), command, str(log_path),
                            expected_rows=len(expected))
        case_results.append(result)
        print("[%s/%s] %s" % (spec.mover, spec.label, " ".join(command)))
        if args.dry_run:
            continue
        if not args.skip_run:
            result.return_code, result.elapsed_seconds = run_command(
                command, case_dir, log_path)
            if result.return_code != 0:
                result.passed = False
                result.messages.append("AMPS exited with code %d" % result.return_code)
                prefix = ("" if case_required_for_pass(spec, args)
                          else "DIAGNOSTIC ")
                all_messages.append("%s%s/%s: AMPS exited with code %d" %
                                    (prefix, spec.mover, spec.label,
                                     result.return_code))
                continue
        try:
            output = find_output(case_dir, args.output_file)
            result.output_file = str(output)
            parsed = parse_tecplot_shell(output)
            evaluate_case(result, parsed, expected, args)
            result.mpi_total_tasks, result.mpi_sum_tasks = parse_mpi_task_closure(log_path)
            if (result.mpi_total_tasks is not None and
                    result.mpi_total_tasks != len(expected)):
                result.passed = False
                result.messages.append("AMPS totalTasks=%d, expected=%d" %
                                       (result.mpi_total_tasks, len(expected)))
            if (result.mpi_total_tasks is not None and
                    result.mpi_sum_tasks is not None and
                    result.mpi_sum_tasks != result.mpi_total_tasks):
                result.passed = False
                result.messages.append("MPI task sum %d differs from total %d" %
                                       (result.mpi_sum_tasks,
                                        result.mpi_total_tasks))
        except Exception as error:
            result.passed = False
            result.messages.append(str(error))
        prefix = ("" if case_required_for_pass(spec, args)
                  else "DIAGNOSTIC ")
        all_messages.extend("%s%s/%s: %s" %
                            (prefix, spec.mover, spec.label, message)
                            for message in result.messages)

    if args.dry_run:
        print("C12 dry run complete: prepared %d AMPS cases in %s" %
              (len(specs), work_root))
        return 0

    convergence_rows, convergence_passed, convergence_messages = (
        evaluate_convergence(case_results, args))
    cross_rows, cross_passed, cross_messages = evaluate_cross_mover(
        case_results, args)
    all_messages.extend(convergence_messages)
    all_messages.extend(cross_messages)
    overall_passed = (all(case.passed or
                          not case_required_for_pass(case.spec, args)
                          for case in case_results) and
                      convergence_passed and cross_passed)
    summary_rows = [row for case in case_results for row in case.summary_rows]
    write_csv(work_root / "C12_summary.csv", summary_rows)
    write_csv(work_root / "C12_convergence.csv", convergence_rows, (
        "mover", "coarse_dt_seconds", "medium_dt_seconds", "fine_dt_seconds",
        "coarse_medium_normalized_rms", "medium_fine_normalized_rms",
        "allowed_medium_fine_rms", "required_for_pass", "passed"))
    write_csv(work_root / "C12_cross_mover.csv", cross_rows, (
        "mover_family", "movers", "alt_km", "lat_deg", "worst_lon_deg",
        "max_spread_GV", "allowed_spread_GV", "required_for_pass", "passed"))
    result_json = {
        "test_id": TEST_ID, "test_name": TEST_NAME, "passed": overall_passed,
        "profile": args.profile, "messages": all_messages,
        "configuration": {
            "movers": args.movers, "np": args.np, "nt": args.nt,
            "gc_policy": args.gc_policy,
            "dt_scales": args.dt_scales, "base_dt_seconds": args.base_dt,
            "adaptive_dt": args.adaptive_dt, "full_alt_km": args.full_alt,
            "gc_alt_km": args.gc_alt, "target_lats_deg": args.target_lats,
            "shell_lon_res_deg": args.shell_lon_res_deg,
            "shell_lat_res_deg": args.shell_lat_res_deg,
            "expected_shell_rows_per_case": len(expected),
            "cutoff_scan_n": args.cutoff_scan_n,
            "max_trace_time_seconds": args.max_trace_time,
            "require_convergence": args.require_convergence,
            "enforce_cross_mover": args.enforce_cross_mover,
        },
        "references": {
            "stormer_R0_GV": STORMER_R0_GV,
            "reference_file": str(work_root / REFERENCE_PATH.name),
            "reference_sha256": reference_hash,
            "acceptance_contract_file": str(work_root / CONTRACT_PATH.name),
            "acceptance_contract_sha256": contract_hash,
        },
        "cases": [dict(case.json_record(),
                       required_for_pass=case_required_for_pass(case.spec, args),
                       reported_status=("PASS" if case.passed else "FAIL")
                       if case_required_for_pass(case.spec, args)
                       else ("DIAGNOSTIC_PASS" if case.passed
                             else "DIAGNOSTIC_FAIL"))
                  for case in case_results],
        "convergence": convergence_rows, "cross_mover": cross_rows,
    }
    (work_root / "C12_result.json").write_text(
        json.dumps(result_json, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")

    print("\nC12 — particle-mover validation in an analytical dipole")
    print("result: %s" % ("PASS" if overall_passed else "FAIL"))
    print("profile: %s; cases: %d; shell rows/case: %d; ranks/threads: %d/%d" %
          (args.profile, len(case_results), len(expected), args.np, args.nt))
    for case in case_results:
        if case_required_for_pass(case.spec, args):
            status = "PASS" if case.passed else "FAIL"
        else:
            status = "DIAGNOSTIC_PASS" if case.passed else "DIAGNOSTIC_FAIL"
        print("%-10s %-12s %s rows=%d/%d" %
              (case.spec.mover, case.spec.label,
               status,
               case.parsed_rows, case.expected_rows))
    if all_messages:
        print("\nMessages:")
        for message in all_messages:
            print("- " + message)
    print("\nC12 results: %s" % work_root)
    return 0 if overall_passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
