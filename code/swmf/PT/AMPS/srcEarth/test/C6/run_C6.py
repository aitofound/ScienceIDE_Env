#!/usr/bin/env python3
"""C6 — global IGRF effective vertical-cutoff validation.

C6 compares AMPS with three independently published world-grid references:

  INITIAL
      Smart & Shea, epoch 2000 printed table (5 deg latitude x 30 deg longitude).

  COMPLETE
      FAA CARI-7 tables for 1965, 1980, 1990, 1995, 2000, and 2010.  The bundled
      archive contains every published one-degree output row.  By default the
      runner evaluates a configurable coarser subset because a six-epoch, one-
      degree, 0.01-GV penumbra scan is a very large production calculation.
      --full-grid selects the complete one-degree comparison.

  MODERN
      Gerontidou et al. grids for 2010, 2015, and 2020.  The author-supplied 2015
      spreadsheet is converted and bundled.  The loader also accepts 2010 and
      2020 CSV/CSV.GZ files through --modern-reference, without substituting a
      different publication or silently fabricating missing values.

All references contain *effective* vertical cutoff rigidity.  C6 therefore
requires the AMPS PENUMBRA_SCAN output column Rc_effective_GV.  Rc_lower and
Rc_upper are retained in the detailed comparison CSV for diagnosing differences
in the access penumbra, but neither is used as the external reference quantity.

The same reference cases can be executed with two independent AMPS field paths:

  GRIDLESS
      Evaluates IGRF directly at every trajectory sample.

  GRIDDED
      Initializes IGRF on the standalone Mode3D AMR mesh and evaluates the
      trajectory field through the production mesh interpolation stencil.

  BOTH
      Runs both paths in separate case directories.  Each path is judged against
      the external table with identical physical settings; the resulting files
      can also be compared directly to isolate mesh/interpolation error.

The runner is intentionally self-contained and has no mandatory non-standard
Python dependency. Matplotlib is used only when available to create diagnostic
plots, written in every format named by ``--plot-formats`` (PNG and EPS by
default -- a raster copy for quick viewing and a vector copy suitable for
publication); missing Matplotlib never changes pass/fail status.
"""

from __future__ import annotations

import argparse
import csv
import gzip
import json
import logging
import math
import re
import shutil
import statistics
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, Iterable, Iterator, List, Mapping, Optional, Sequence, Tuple

TEST_ID = "C6"
TEST_NAME = "Global IGRF effective vertical-cutoff validation (gridless/gridded)"

SOLVER_CHOICES = ("GRIDLESS", "GRIDDED", "BOTH")
MODE3D_MESH_COARSENING_CHOICES = (
    "LINEAR", "LOG", "EXPONENTIAL", "GEOMETRIC", "POWER", "CONSTANT",
)

# The particle mass is written to the AMPS input and reused when converting the
# requested rigidity bracket into CUTOFF_EMIN/CUTOFF_EMAX.  Keeping one constant
# prevents the endpoints from drifting because of inconsistent rest masses.
PROTON_MASS_AMU = 1.007276466621
AMU_KG = 1.66053906660e-27
C_LIGHT_M_S = 299792458.0
EV_J = 1.602176634e-19
MEV_J = 1.0e6 * EV_J

# The common AMPS parameter parser stores DOMAIN_* and R_INNER in kilometers.
# The C6 CLI deliberately exposes the outer-domain half-size in Earth radii
# because that is the natural scale for magnetospheric cutoff tracing.  Convert
# exactly once in the runner before writing the numerical AMPS input.  This
# value matches kEarthRadiusKm in util/amps_param_parser.cpp.
EARTH_RADIUS_KM = 6371.2

CANONICAL_MOVERS = (
    "BORIS", "HC4", "RK2", "RK4", "RK6",
    "GC2", "GC4", "GC6", "HYBRID",
)
MOVER_ALIASES = {
    "BORIS": "BORIS",
    "HC4": "HC4",
    "HIGUERACARY4": "HC4",
    "HIGUERA-CARY4": "HC4",
    "HIGUERA_CARY4": "HC4",
    "HC_YOSHIDA4": "HC4",
    "HC-YOSHIDA4": "HC4",
    "RK2": "RK2",
    "RUNGEKUTTA2": "RK2",
    "RUNGE-KUTTA-2": "RK2",
    "RK4": "RK4",
    "RUNGEKUTTA4": "RK4",
    "RUNGE-KUTTA-4": "RK4",
    "RK6": "RK6",
    "RUNGEKUTTA6": "RK6",
    "RUNGE-KUTTA-6": "RK6",
    "GC2": "GC2",
    "GUIDINGCENTER2": "GC2",
    "GUIDING-CENTER-2": "GC2",
    "GC4": "GC4",
    "GUIDINGCENTER4": "GC4",
    "GUIDING-CENTER-4": "GC4",
    "GC6": "GC6",
    "GUIDINGCENTER6": "GC6",
    "GUIDING-CENTER-6": "GC6",
    "HYBRID": "HYBRID",
    "HYBRID_RK_GC": "HYBRID",
    "HYB": "HYBRID",
}


@dataclass(frozen=True)
class ReferenceRow:
    epoch_year: int
    latitude_deg: float
    longitude_deg_east: float
    altitude_km: float
    rc_effective_gv: float
    source: str


@dataclass(frozen=True)
class CasePlan:
    subtest: str
    epoch_year: int
    reference_path: Path
    reference_rows: Tuple[ReferenceRow, ...]
    shell_lon_res_deg: float
    shell_lat_res_deg: float
    case_name: str


@dataclass
class ModelRow:
    longitude_deg_east: float
    latitude_deg: float
    altitude_km: float
    rc_lower_gv: float
    rc_effective_gv: float
    rc_upper_gv: float
    n_allowed_intervals: int
    n_transitions: int
    n_unresolved: int
    lower_bracket_unresolved: int
    upper_bracket_unresolved: int
    lower_below_range: int
    lower_above_range: int
    upper_below_range: int
    upper_above_range: int


@dataclass
class CaseMetrics:
    solver: str
    subtest: str
    epoch_year: int
    source: str
    reference_path: str
    cutoff_scan_n: int
    scan_spacing_gv: float
    backtrace_charge: str
    trace_limit_policy: str
    n_reference: int
    n_matched: int
    n_valid: int
    n_unresolved: int
    n_point_pass: int
    matched_fraction: float
    valid_fraction: float
    point_pass_fraction: float
    mean_bias_gv: float
    mean_abs_error_gv: float
    rmse_gv: float
    max_abs_error_gv: float
    correlation: Optional[float]
    abs_tolerance_gv: float
    relative_tolerance: float
    required_valid_fraction: float
    required_point_pass_fraction: float
    maximum_rmse_gv: float
    passed: bool


def parse_mover_name(text: str) -> str:
    """Return the canonical AMPS mover token or raise an argparse error."""
    key = str(text).strip().upper()
    mover = MOVER_ALIASES.get(key)
    if mover is None:
        raise argparse.ArgumentTypeError(
            "unknown mover %r; choose one of: %s" %
            (text, ", ".join(CANONICAL_MOVERS))
        )
    return mover


def parse_int_list(text: str, option_name: str) -> List[int]:
    values: List[int] = []
    for token in str(text).split(","):
        token = token.strip()
        if not token:
            continue
        try:
            values.append(int(token))
        except ValueError as exc:
            raise SystemExit("%s contains non-integer value %r" % (option_name, token)) from exc
    if not values:
        raise SystemExit("%s must contain at least one year" % option_name)
    return values


def normalize_lon(lon_deg: float) -> float:
    value = float(lon_deg) % 360.0
    if abs(value) < 1.0e-9 or abs(value - 360.0) < 1.0e-9:
        return 0.0
    return value


def nearly_on_grid(value: float, origin: float, step: float, tol: float = 1.0e-7) -> bool:
    """Return true when value lies on origin + integer*step within roundoff."""
    if not (step > 0.0):
        return False
    q = (value - origin) / step
    return abs(q - round(q)) <= tol


def kinetic_energy_mev_from_rigidity_gv(rigidity_gv: float) -> float:
    """Convert proton rigidity to kinetic energy using the input particle mass."""
    if rigidity_gv < 0.0:
        raise ValueError("rigidity must be non-negative")
    rest_mev = PROTON_MASS_AMU * AMU_KG * C_LIGHT_M_S ** 2 / MEV_J
    momentum_mev_c = rigidity_gv * 1000.0  # Z=+1 proton
    return math.sqrt(momentum_mev_c ** 2 + rest_mev ** 2) - rest_mev


def open_text_auto(path: Path):
    """Open plain CSV or gzip-compressed CSV using one interface."""
    if path.suffix.lower() == ".gz":
        return gzip.open(path, "rt", newline="")
    return path.open("r", newline="")


def load_reference(path: Path) -> List[ReferenceRow]:
    """Load and strictly validate the common C6 reference-table schema."""
    required = {
        "epoch_year", "latitude_deg", "longitude_deg_east",
        "altitude_km", "rc_effective_gv", "source",
    }
    rows: List[ReferenceRow] = []
    with open_text_auto(path) as stream:
        reader = csv.DictReader(stream)
        fields = set(reader.fieldnames or [])
        missing = sorted(required - fields)
        if missing:
            raise RuntimeError("%s is missing columns: %s" % (path, ", ".join(missing)))
        for line_number, raw in enumerate(reader, start=2):
            try:
                row = ReferenceRow(
                    epoch_year=int(raw["epoch_year"]),
                    latitude_deg=float(raw["latitude_deg"]),
                    longitude_deg_east=normalize_lon(float(raw["longitude_deg_east"])),
                    altitude_km=float(raw["altitude_km"]),
                    rc_effective_gv=float(raw["rc_effective_gv"]),
                    source=str(raw["source"]).strip(),
                )
            except (TypeError, ValueError) as exc:
                raise RuntimeError("invalid reference row %s:%d" % (path, line_number)) from exc
            if not (-90.0 <= row.latitude_deg <= 90.0):
                raise RuntimeError("latitude outside [-90,90] in %s:%d" % (path, line_number))
            if not (0.0 <= row.longitude_deg_east < 360.0 + 1.0e-8):
                raise RuntimeError("longitude outside [0,360) in %s:%d" % (path, line_number))
            if not (row.altitude_km >= 0.0 and row.rc_effective_gv >= 0.0):
                raise RuntimeError("negative altitude/cutoff in %s:%d" % (path, line_number))
            rows.append(row)
    if not rows:
        raise RuntimeError("reference file is empty: %s" % path)
    return rows


def reference_key(row: ReferenceRow) -> Tuple[int, float, float, float]:
    return (
        row.epoch_year,
        round(row.altitude_km, 8),
        round(row.latitude_deg, 8),
        round(normalize_lon(row.longitude_deg_east), 8),
    )


def validate_reference_rows(path: Path, rows: Sequence[ReferenceRow]) -> Dict[str, object]:
    """Check uniqueness and report the geographic/epoch coverage of one file."""
    seen: Dict[Tuple[int, float, float, float], int] = {}
    for index, row in enumerate(rows):
        key = reference_key(row)
        if key in seen:
            raise RuntimeError(
                "duplicate reference coordinate in %s at rows %d and %d: %s" %
                (path, seen[key] + 2, index + 2, key)
            )
        seen[key] = index
    return {
        "path": str(path),
        "row_count": len(rows),
        "epochs": sorted({row.epoch_year for row in rows}),
        "latitudes": len({round(row.latitude_deg, 8) for row in rows}),
        "longitudes": len({round(row.longitude_deg_east, 8) for row in rows}),
        "altitudes_km": sorted({row.altitude_km for row in rows}),
        "minimum_rc_effective_gv": min(row.rc_effective_gv for row in rows),
        "maximum_rc_effective_gv": max(row.rc_effective_gv for row in rows),
    }


def filter_reference_grid(
    rows: Sequence[ReferenceRow],
    epoch_year: int,
    lon_step_deg: float,
    lat_step_deg: float,
) -> Tuple[ReferenceRow, ...]:
    """Select rows that coincide with the shell grid generated by AMPS.

    AMPS shell latitude starts at -90 degrees.  This matters for the CARI file,
    whose published table covers -89...+89 degrees: a 10-degree test subset is
    therefore -80,-70,...,+80 rather than -89,-79,...,+81.
    """
    selected = [
        row for row in rows
        if row.epoch_year == epoch_year
        and nearly_on_grid(row.longitude_deg_east, 0.0, lon_step_deg)
        and nearly_on_grid(row.latitude_deg, -90.0, lat_step_deg)
    ]
    selected.sort(key=lambda row: (row.latitude_deg, row.longitude_deg_east))
    if not selected:
        raise RuntimeError(
            "no reference rows remain for epoch %d on lon/lat grid %.12g/%.12g deg" %
            (epoch_year, lon_step_deg, lat_step_deg)
        )
    return tuple(selected)


def discover_modern_references(script_dir: Path, explicit_paths: Sequence[str]) -> List[Path]:
    """Return bundled and user-supplied Gerontidou reference files without duplicates."""
    candidates = [script_dir / "reference_C6_gerontidou_2015.csv"]
    candidates.extend(Path(value).expanduser().resolve() for value in explicit_paths)
    unique: List[Path] = []
    seen = set()
    for path in candidates:
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        if not resolved.exists():
            raise RuntimeError("modern reference file does not exist: %s" % resolved)
        unique.append(resolved)
    return unique


def build_case_plans(args: argparse.Namespace, script_dir: Path) -> Tuple[List[CasePlan], List[Dict[str, object]]]:
    """Load reference archives and expand requested subtests into epoch-specific cases."""
    requested = [args.subtest] if args.subtest != "ALL" else ["INITIAL", "COMPLETE", "MODERN"]
    plans: List[CasePlan] = []
    inventories: List[Dict[str, object]] = []

    if "INITIAL" in requested:
        path = script_dir / "reference_C6_smart_shea_2000.csv"
        rows = load_reference(path)
        inventories.append(validate_reference_rows(path, rows))
        selected = filter_reference_grid(rows, 2000, 30.0, 5.0)
        plans.append(CasePlan("INITIAL", 2000, path, selected, 30.0, 5.0, "initial_2000"))

    if "COMPLETE" in requested:
        path = script_dir / "reference_C6_cari7_1965_2010.csv.gz"
        rows = load_reference(path)
        inventories.append(validate_reference_rows(path, rows))
        available = sorted({row.epoch_year for row in rows})
        years = parse_int_list(args.complete_epochs, "--complete-epochs")
        missing = sorted(set(years) - set(available))
        if missing:
            raise RuntimeError(
                "CARI reference does not contain requested epoch(s): %s; available: %s" %
                (", ".join(map(str, missing)), ", ".join(map(str, available)))
            )
        step = 1.0 if args.full_grid else args.complete_grid_step_deg
        for year in years:
            selected = filter_reference_grid(rows, year, step, step)
            plans.append(CasePlan(
                "COMPLETE", year, path, selected, step, step,
                "complete_%d_%sdeg" % (year, ("1" if step == 1.0 else "%g" % step)),
            ))

    if "MODERN" in requested:
        modern_rows: List[ReferenceRow] = []
        modern_paths = discover_modern_references(script_dir, args.modern_reference)
        path_for_year: Dict[int, Path] = {}
        for path in modern_paths:
            rows = load_reference(path)
            inventories.append(validate_reference_rows(path, rows))
            modern_rows.extend(rows)
            for year in {row.epoch_year for row in rows}:
                if year in path_for_year:
                    raise RuntimeError(
                        "modern epoch %d is present in more than one reference file: %s and %s" %
                        (year, path_for_year[year], path)
                    )
                path_for_year[year] = path
        years = parse_int_list(args.modern_epochs, "--modern-epochs")
        missing = sorted(set(years) - set(path_for_year))
        if missing:
            raise RuntimeError(
                "Gerontidou reference data are missing for epoch(s) %s. "
                "The repository bundles the author-supplied 2015 grid. Add the 2010/2020 "
                "tables with --modern-reference FILE after obtaining authoritative data; "
                "do not substitute CARI values." % ", ".join(map(str, missing))
            )
        for year in years:
            year_rows = [row for row in modern_rows if row.epoch_year == year]
            lons = sorted({round(row.longitude_deg_east, 8) for row in year_rows})
            lats = sorted({round(row.latitude_deg, 8) for row in year_rows})
            if len(lons) < 2 or len(lats) < 2:
                raise RuntimeError("modern epoch %d does not form a two-dimensional grid" % year)
            lon_step = min(b - a for a, b in zip(lons, lons[1:]) if b > a)
            lat_step = min(b - a for a, b in zip(lats, lats[1:]) if b > a)
            selected = filter_reference_grid(year_rows, year, lon_step, lat_step)
            plans.append(CasePlan(
                "MODERN", year, path_for_year[year], selected,
                lon_step, lat_step, "modern_%d" % year,
            ))

    return plans, inventories


def replace_input_directive(text: str, directive: str, value: str) -> str:
    """Replace exactly one AMPS input directive while preserving readability.

    C6 keeps ``AMPS_PARAM_C6_gridless.in`` as a valid numerical baseline rather
    than as a collection of ``__PLACEHOLDER__`` tokens.  This helper therefore
    edits the input structurally: it finds a non-comment line whose first token
    is *directive* and replaces only that directive's value.  Section headers,
    explanatory comments, blank lines, and any trailing ``!`` comment are kept.

    Requiring exactly one match catches misspelled, duplicated, or removed
    directives immediately.  That is safer than unrestricted ``str.replace``:
    a replacement cannot accidentally alter a comment, another parameter name,
    or an unrelated number that happens to have the same textual value.
    """
    source_lines = text.splitlines()
    output_lines: List[str] = []
    matches = 0

    for line in source_lines:
        stripped = line.lstrip()
        if not stripped or stripped.startswith(("!", "#")):
            output_lines.append(line)
            continue

        fields = stripped.split(None, 1)
        if fields[0] != directive:
            output_lines.append(line)
            continue

        matches += 1
        trailing_comment = ""
        if len(fields) == 2 and "!" in fields[1]:
            trailing_comment = "  !" + fields[1].split("!", 1)[1]

        # A fixed-width directive field keeps generated inputs easy to compare
        # with the checked-in baseline while allowing long directive names.
        # Use a field wider than the longest supported directive and always keep at
        # least one separating space.  The earlier 24-character field was too short
        # for CUTOFF_TRACE_LIMIT_POLICY and produced an invalid concatenated token.
        field_width = max(32, len(directive) + 1)
        output_lines.append(("%%-%ds%%s%%s" % field_width) %
                            (directive, value, trailing_comment))

    if matches != 1:
        raise RuntimeError(
            "expected exactly one %s directive in the C6 numerical input; found %d" %
            (directive, matches)
        )

    return "\n".join(output_lines) + "\n"


def validate_numerical_input(text: str, solver: str) -> None:
    """Reject unresolved template variables in a generated C6 input.

    The AMPS input language still uses categorical tokens such as ``IGRF``,
    ``PROTON``, ``LINEAR``, and the logical values ``T``/``F``.  Here
    "numerical input" means that every quantity that is physically numerical
    is written as a literal number and that no runner placeholder or shell
    variable remains for AMPS to interpret.
    """
    active_lines = [
        line for line in text.splitlines()
        if line.strip() and not line.lstrip().startswith(("!", "#"))
    ]
    unresolved = sorted(set(re.findall(
        r"__[A-Za-z0-9_]+__", "\n".join(active_lines)
    )))
    if unresolved:
        raise RuntimeError(
            "generated C6 input contains unresolved placeholder(s): %s" %
            ", ".join(unresolved)
        )

    numeric_directives = {
        "CUTOFF_EMIN", "CUTOFF_EMAX", "CUTOFF_MAX_PARTICLES",
        "CUTOFF_NENERGY", "CUTOFF_UPPER_SCAN_N", "CUTOFF_MAX_TRAJ_TIME",
        "CHARGE", "MASS_AMU", "DOMAIN_X_MIN", "DOMAIN_X_MAX",
        "DOMAIN_Y_MIN", "DOMAIN_Y_MAX", "DOMAIN_Z_MIN", "DOMAIN_Z_MAX",
        "R_INNER", "SPEC_J0", "SPEC_GAMMA", "SPEC_E0", "SPEC_EMIN",
        "SPEC_EMAX", "SHELL_COUNT", "SHELL_ALTS_KM",
        "SHELL_LON_RES_DEG", "SHELL_LAT_RES_DEG", "DT_TRACE",
        "MAX_STEPS", "MAX_TRACE_TIME", "MAX_TRACE_DISTANCE",
    }
    if solver == "GRIDDED":
        numeric_directives.update({
            "MODE3D_MESH_RES_EARTH_RE",
            "MODE3D_MESH_RES_BOUNDARY_RE",
            "MODE3D_MESH_EXPONENT",
        })
    found: Dict[str, str] = {}
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith(("!", "#")):
            continue
        fields = stripped.split()
        if fields and fields[0] in numeric_directives:
            if len(fields) < 2:
                raise RuntimeError("numeric directive %s has no value" % fields[0])
            found[fields[0]] = fields[1]

    missing = sorted(numeric_directives - set(found))
    if missing:
        raise RuntimeError(
            "generated C6 input is missing numeric directive(s): %s" %
            ", ".join(missing)
        )
    for directive, value in sorted(found.items()):
        try:
            number = float(value)
        except ValueError as exc:
            raise RuntimeError(
                "C6 directive %s must contain a numerical literal, got %r" %
                (directive, value)
            ) from exc
        if not math.isfinite(number):
            raise RuntimeError(
                "C6 directive %s must contain a finite numerical literal, got %r" %
                (directive, value)
            )

    # Geometry consistency check.  The parser interprets bare DOMAIN_* and
    # R_INNER values as kilometers.  A previous C6 input wrote the intended
    # values 25 Re and 1 Re as bare numbers 25 and 1; AMPS therefore created a
    # +/-25-km box around Earth.  Every 20-km-altitude shell point started
    # outside that box and was immediately classified as escaped/allowed,
    # producing Rc=Rmin everywhere.  Reject that failure mode before launching
    # a potentially expensive MPI calculation.
    numeric = {key: float(value) for key, value in found.items()}
    outer_half_size_km = min(
        numeric["DOMAIN_X_MAX"], -numeric["DOMAIN_X_MIN"],
        numeric["DOMAIN_Y_MAX"], -numeric["DOMAIN_Y_MIN"],
        numeric["DOMAIN_Z_MAX"], -numeric["DOMAIN_Z_MIN"],
    )
    maximum_shell_radius_km = 6378.137 + float(found["SHELL_ALTS_KM"])
    if outer_half_size_km <= maximum_shell_radius_km:
        raise RuntimeError(
            "C6 outer domain is too small: minimum Cartesian half-size is "
            "%.6g km, but the geodetic shell reaches at least %.6g km. "
            "DOMAIN_* values are kilometers; convert --domain-half-size-re "
            "to km before writing the AMPS input." %
            (outer_half_size_km, maximum_shell_radius_km)
        )
    if not (0.0 < numeric["R_INNER"] < maximum_shell_radius_km):
        raise RuntimeError(
            "C6 R_INNER must be a positive kilometer value below the shell "
            "radius; got %.6g km for a shell radius of at least %.6g km" %
            (numeric["R_INNER"], maximum_shell_radius_km)
        )


def render_input(input_path: Path, output_path: Path, plan: CasePlan,
                 args: argparse.Namespace, solver: str) -> None:
    """Copy the numerical baseline and update case-dependent directives.

    The checked-in input is valid for the default INITIAL/2000 case.  Other C6
    cases differ only in a controlled set of values.  Updating directives by
    name preserves a fully numerical generated input and makes the exact run
    configuration inspectable without knowing Python placeholder syntax.
    """
    text = input_path.read_text()
    values = {
        "CUTOFF_EMIN": "%.15g" % kinetic_energy_mev_from_rigidity_gv(args.rigidity_min_gv),
        "CUTOFF_EMAX": "%.15g" % kinetic_energy_mev_from_rigidity_gv(args.rigidity_max_gv),
        "CUTOFF_UPPER_SCAN_N": str(args.cutoff_scan_n),
        "CUTOFF_TRACE_POLICY": args.cutoff_trace_policy,
        "CUTOFF_BACKTRACE_CHARGE": args.backtrace_charge,
        "CUTOFF_TRACE_LIMIT_POLICY": args.trace_limit_policy,
        "CUTOFF_MAX_TRAJ_TIME": "%.12g" % args.max_trace_time,
        "MASS_AMU": "%.15g" % PROTON_MASS_AMU,
        "EPOCH": "%04d-01-01T00:00:00" % plan.epoch_year,
        # DOMAIN_* and R_INNER are stored by AMPS in kilometers.  Keep the
        # human-facing CLI in Earth radii, but emit only explicit numerical km
        # values in AMPS_PARAM_C6.in.
        "DOMAIN_X_MIN": "%.12g" % (-args.domain_half_size_re * EARTH_RADIUS_KM),
        "DOMAIN_X_MAX": "%.12g" % ( args.domain_half_size_re * EARTH_RADIUS_KM),
        "DOMAIN_Y_MIN": "%.12g" % (-args.domain_half_size_re * EARTH_RADIUS_KM),
        "DOMAIN_Y_MAX": "%.12g" % ( args.domain_half_size_re * EARTH_RADIUS_KM),
        "DOMAIN_Z_MIN": "%.12g" % (-args.domain_half_size_re * EARTH_RADIUS_KM),
        "DOMAIN_Z_MAX": "%.12g" % ( args.domain_half_size_re * EARTH_RADIUS_KM),
        "R_INNER": "%.12g" % EARTH_RADIUS_KM,
        "SHELL_ALTS_KM": "%.12g" % plan.reference_rows[0].altitude_km,
        "SHELL_LON_RES_DEG": "%.12g" % plan.shell_lon_res_deg,
        "SHELL_LAT_RES_DEG": "%.12g" % plan.shell_lat_res_deg,
        "DT_TRACE": "%.12g" % args.dt_trace,
        "ADAPTIVE_DT": args.adaptive_dt,
        "MAX_STEPS": str(args.max_steps),
        "MAX_TRACE_TIME": "%.12g" % args.max_trace_time,
        "MAX_TRACE_DISTANCE": "%.12g" % args.max_trace_distance,
        "TRAP_DETECTION": args.trap_detection,
    }
    if solver == "GRIDDED":
        values.update({
            "MODE3D_MESH_RES_EARTH_RE": "%.12g" % args.mode3d_mesh_res_earth_re,
            "MODE3D_MESH_RES_BOUNDARY_RE": "%.12g" % args.mode3d_mesh_res_boundary_re,
            "MODE3D_MESH_COARSENING": args.mode3d_mesh_coarsening,
            "MODE3D_MESH_EXPONENT": "%.12g" % args.mode3d_mesh_exponent,
        })
    for directive, value in values.items():
        text = replace_input_directive(text, directive, value)

    validate_numerical_input(text, solver)
    text += (
        "\n! C6_SUBTEST              %s\n"
        "! C6_REFERENCE_FILE        %s\n"
        "! C6_REFERENCE_ROWS        %d\n"
        "! C6_RIGIDITY_RANGE_GV     %.12g %.12g\n"
        "! C6_MOVER                 %s\n"
        "! C6_SOLVER                %s\n" %
        (plan.subtest, plan.reference_path.name, len(plan.reference_rows),
         args.rigidity_min_gv, args.rigidity_max_gv, args.mover or "AMPS_DEFAULT",
         solver)
    )
    output_path.write_text(text)


def write_reference_subset(path: Path, rows: Sequence[ReferenceRow]) -> None:
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(asdict(rows[0]).keys()))
        writer.writeheader()
        for row in rows:
            writer.writerow(asdict(row))


def run_command(command: Sequence[str], cwd: Path, log_path: Path) -> int:
    """Run AMPS while mirroring combined stdout/stderr to the terminal and log."""
    with log_path.open("w") as log:
        log.write("Command:\n  %s\n\n" % " ".join(command))
        log.flush()
        process = subprocess.Popen(
            list(command), cwd=str(cwd), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, universal_newlines=True,
        )
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            log.write(line)
            log.flush()
        return process.wait()


def normalize_variable_name(name: str) -> str:
    return name.strip().lower().replace("-", "_").replace(" ", "_")


def parse_tecplot_penumbra(path: Path) -> List[ModelRow]:
    """Read the common C6 columns from a gridless or Mode3D penumbra file."""
    variables: List[str] = []
    numeric_rows: List[List[float]] = []
    with path.open("r") as stream:
        for line in stream:
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.upper().startswith("VARIABLES"):
                variables = [normalize_variable_name(v) for v in re.findall(r'"([^"]+)"', stripped)]
                continue
            if stripped.upper().startswith(("TITLE", "ZONE")):
                continue
            try:
                values = [float(token) for token in stripped.split()]
            except ValueError:
                continue
            numeric_rows.append(values)
    if not variables:
        raise RuntimeError("Tecplot VARIABLES record not found in %s" % path)
    required = {
        "lon_deg", "lat_deg", "rc_lower_gv", "rc_effective_gv", "rc_upper_gv",
        "n_allowed_intervals", "n_transitions", "n_unresolved",
        "lower_bracket_unresolved", "upper_bracket_unresolved",
        "lower_below_range", "lower_above_range", "upper_below_range", "upper_above_range",
    }
    missing = sorted(required - set(variables))
    if missing:
        raise RuntimeError(
            "%s is missing C6 columns %s. Rebuild AMPS with the effective-cutoff C6 patch." %
            (path, ", ".join(missing))
        )
    index = {name: variables.index(name) for name in required}
    rows: List[ModelRow] = []
    for values in numeric_rows:
        if len(values) < len(variables):
            continue
        rows.append(ModelRow(
            longitude_deg_east=normalize_lon(values[index["lon_deg"]]),
            latitude_deg=values[index["lat_deg"]],
            altitude_km=20.0,
            rc_lower_gv=values[index["rc_lower_gv"]],
            rc_effective_gv=values[index["rc_effective_gv"]],
            rc_upper_gv=values[index["rc_upper_gv"]],
            n_allowed_intervals=int(round(values[index["n_allowed_intervals"]])),
            n_transitions=int(round(values[index["n_transitions"]])),
            n_unresolved=int(round(values[index["n_unresolved"]])),
            lower_bracket_unresolved=int(round(values[index["lower_bracket_unresolved"]])),
            upper_bracket_unresolved=int(round(values[index["upper_bracket_unresolved"]])),
            lower_below_range=int(round(values[index["lower_below_range"]])),
            lower_above_range=int(round(values[index["lower_above_range"]])),
            upper_below_range=int(round(values[index["upper_below_range"]])),
            upper_above_range=int(round(values[index["upper_above_range"]])),
        ))
    if not rows:
        raise RuntimeError("no numeric shell rows found in %s" % path)
    return rows


def coordinate_key(lat_deg: float, lon_deg: float) -> Tuple[float, float]:
    return round(lat_deg, 7), round(normalize_lon(lon_deg), 7)


def pearson_correlation(x: Sequence[float], y: Sequence[float]) -> Optional[float]:
    if len(x) < 2 or len(y) != len(x):
        return None
    mx = statistics.fmean(x)
    my = statistics.fmean(y)
    dx = [value - mx for value in x]
    dy = [value - my for value in y]
    denom = math.sqrt(sum(v * v for v in dx) * sum(v * v for v in dy))
    if denom <= 0.0:
        return None
    return sum(a * b for a, b in zip(dx, dy)) / denom


def compare_case(
    plan: CasePlan,
    model_rows: Sequence[ModelRow],
    args: argparse.Namespace,
    solver: str,
) -> Tuple[List[Dict[str, object]], CaseMetrics]:
    """Match coordinates, write per-point diagnostics, and evaluate C6 criteria."""
    model_by_key = {coordinate_key(row.latitude_deg, row.longitude_deg_east): row for row in model_rows}
    detailed: List[Dict[str, object]] = []
    errors: List[float] = []
    references: List[float] = []
    models: List[float] = []
    n_matched = 0
    n_valid = 0
    n_pass = 0
    n_unresolved = 0

    for ref in plan.reference_rows:
        key = coordinate_key(ref.latitude_deg, ref.longitude_deg_east)
        model = model_by_key.get(key)
        if model is None:
            detailed.append({
                **asdict(ref), "solver": solver,
                "matched": False, "valid": False, "passed": False,
                "failure_reason": "missing_model_coordinate",
            })
            continue
        n_matched += 1
        flags = {
            "n_unresolved": model.n_unresolved,
            "lower_bracket_unresolved": model.lower_bracket_unresolved,
            "upper_bracket_unresolved": model.upper_bracket_unresolved,
            "lower_below_range": model.lower_below_range,
            "lower_above_range": model.lower_above_range,
            "upper_below_range": model.upper_below_range,
            "upper_above_range": model.upper_above_range,
        }
        unresolved = (
            model.n_unresolved != 0
            or model.lower_bracket_unresolved != 0
            or model.upper_bracket_unresolved != 0
            or model.rc_effective_gv < 0.0
        )
        if unresolved:
            n_unresolved += 1
        valid = not unresolved
        abs_error = abs(model.rc_effective_gv - ref.rc_effective_gv) if valid else math.nan
        signed_error = model.rc_effective_gv - ref.rc_effective_gv if valid else math.nan
        scale = max(ref.rc_effective_gv, args.relative_floor_gv)
        tolerance = max(args.abs_tol_gv, args.rel_tol * scale)
        point_pass = valid and abs_error <= tolerance
        if valid:
            n_valid += 1
            errors.append(signed_error)
            references.append(ref.rc_effective_gv)
            models.append(model.rc_effective_gv)
            if point_pass:
                n_pass += 1
        detailed.append({
            **asdict(ref),
            "solver": solver,
            "matched": True,
            "valid": valid,
            "rc_lower_model_gv": model.rc_lower_gv,
            "rc_effective_model_gv": model.rc_effective_gv,
            "rc_upper_model_gv": model.rc_upper_gv,
            "signed_error_gv": signed_error,
            "absolute_error_gv": abs_error,
            "relative_error": (signed_error / scale) if valid else math.nan,
            "point_tolerance_gv": tolerance,
            "passed": point_pass,
            "n_allowed_intervals": model.n_allowed_intervals,
            "n_transitions": model.n_transitions,
            **flags,
            "failure_reason": "" if point_pass else ("unresolved" if unresolved else "outside_tolerance"),
        })

    n_reference = len(plan.reference_rows)
    matched_fraction = n_matched / n_reference if n_reference else 0.0
    valid_fraction = n_valid / n_reference if n_reference else 0.0
    pass_fraction = n_pass / n_valid if n_valid else 0.0
    if errors:
        mean_bias = statistics.fmean(errors)
        mae = statistics.fmean(abs(value) for value in errors)
        rmse = math.sqrt(statistics.fmean(value * value for value in errors))
        max_abs = max(abs(value) for value in errors)
    else:
        mean_bias = mae = rmse = max_abs = math.inf
    passed = (
        matched_fraction >= args.required_valid_fraction
        and valid_fraction >= args.required_valid_fraction
        and pass_fraction >= args.required_pass_fraction
        and rmse <= args.rmse_max_gv
    )
    metrics = CaseMetrics(
        solver=solver,
        subtest=plan.subtest,
        epoch_year=plan.epoch_year,
        source=plan.reference_rows[0].source,
        reference_path=str(plan.reference_path),
        # Record the methodological settings in every case result.  These values
        # materially affect effective-cutoff comparability and therefore belong in
        # the provenance rather than only in the generated AMPS input file.
        cutoff_scan_n=args.cutoff_scan_n,
        scan_spacing_gv=(args.rigidity_max_gv - args.rigidity_min_gv) /
                        (args.cutoff_scan_n - 1),
        backtrace_charge=args.backtrace_charge,
        trace_limit_policy=args.trace_limit_policy,
        n_reference=n_reference,
        n_matched=n_matched,
        n_valid=n_valid,
        n_unresolved=n_unresolved,
        n_point_pass=n_pass,
        matched_fraction=matched_fraction,
        valid_fraction=valid_fraction,
        point_pass_fraction=pass_fraction,
        mean_bias_gv=mean_bias,
        mean_abs_error_gv=mae,
        rmse_gv=rmse,
        max_abs_error_gv=max_abs,
        correlation=pearson_correlation(references, models),
        abs_tolerance_gv=args.abs_tol_gv,
        relative_tolerance=args.rel_tol,
        required_valid_fraction=args.required_valid_fraction,
        required_point_pass_fraction=args.required_pass_fraction,
        maximum_rmse_gv=args.rmse_max_gv,
        passed=passed,
    )
    return detailed, metrics


def write_dict_rows(path: Path, rows: Sequence[Mapping[str, object]]) -> None:
    if not rows:
        path.write_text("")
        return
    fieldnames: List[str] = []
    seen = set()
    for row in rows:
        for key in row.keys():
            if key not in seen:
                seen.add(key)
                fieldnames.append(key)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def make_case_plot(
    rows: Sequence[Mapping[str, object]],
    output_base: Path,
    title: str,
    formats: Sequence[str] = ("png", "eps"),
) -> List[Path]:
    """Create a compact reference-vs-model diagnostic when Matplotlib is available.

    ``output_base`` carries no suffix; one sibling file is written per entry in
    ``formats`` (``output_base.with_suffix(".png")``, ``".eps"``, ...) from the
    same figure, so the raster and vector copies never drift apart. The return
    value lists only the files actually written, which is ``[]`` when
    Matplotlib is unavailable, there is no valid data to plot, or every
    requested format failed to save -- callers must not treat an empty result
    as an error.
    """
    valid = [row for row in rows if row.get("valid") is True]
    if not valid or not formats:
        return []
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception:
        return []
    reference = [float(row["rc_effective_gv"]) for row in valid]
    model = [float(row["rc_effective_model_gv"]) for row in valid]
    residual = [m - r for r, m in zip(reference, model)]
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.5))
    axes[0].scatter(reference, model, s=8, alpha=0.65)
    limit = max(reference + model + [1.0])
    axes[0].plot([0.0, limit], [0.0, limit], linestyle="--", linewidth=1)
    axes[0].set_xlabel("Reference effective cutoff [GV]")
    axes[0].set_ylabel("AMPS effective cutoff [GV]")
    axes[0].set_title("Reference vs AMPS")
    axes[0].grid(True, alpha=0.25)
    axes[1].scatter(reference, residual, s=8, alpha=0.65)
    axes[1].axhline(0.0, linestyle="--", linewidth=1)
    axes[1].set_xlabel("Reference effective cutoff [GV]")
    axes[1].set_ylabel("AMPS - reference [GV]")
    axes[1].set_title("Residual")
    axes[1].grid(True, alpha=0.25)
    figure.suptitle(title)
    figure.tight_layout()

    # EPS/PS have no alpha channel, so Matplotlib's translucent markers
    # (alpha=0.65 above) fall back to opaque fills on that one format.
    # Matplotlib reports this through its logger rather than warnings.warn,
    # so it is muted here rather than with a warnings filter; it is expected
    # on every EPS write and not a sign anything went wrong.
    ps_logger = logging.getLogger("matplotlib.backends.backend_ps")
    previous_level = ps_logger.level
    written: List[Path] = []
    try:
        ps_logger.setLevel(logging.ERROR)
        for fmt in formats:
            destination = output_base.with_suffix("." + fmt.lower().lstrip("."))
            try:
                figure.savefig(destination, dpi=160)
            except Exception as exc:
                print("C6 plot: failed to write %s: %s" % (destination, exc), file=sys.stderr)
                continue
            written.append(destination)
    finally:
        ps_logger.setLevel(previous_level)

    plt.close(figure)
    return written


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=TEST_NAME,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        epilog=(
            "Examples:\n"
            "  run_C6.py --subtest INITIAL -np 8\n"
            "  run_C6.py --subtest INITIAL --solver GRIDDED -np 8 -nt 16\n"
            "  run_C6.py --subtest INITIAL --solver BOTH -np 8 -nt 16\n"
            "  run_C6.py --subtest COMPLETE --complete-epochs 2010 --complete-grid-step-deg 5\n"
            "  run_C6.py --subtest COMPLETE --full-grid --complete-epochs 2000\n"
            "  run_C6.py --subtest MODERN --modern-epochs 2015\n"
            "  run_C6.py --subtest MODERN --modern-epochs 2010,2015,2020 "
            "--modern-reference ref_2010.csv --modern-reference ref_2020.csv\n"
        ),
    )
    parser.add_argument("--subtest", choices=("INITIAL", "COMPLETE", "MODERN", "ALL"), default="ALL")
    parser.add_argument(
        "--solver", choices=SOLVER_CHOICES, default="GRIDLESS",
        help=("AMPS field path: GRIDLESS evaluates IGRF directly; GRIDDED uses "
              "the standalone Mode3D AMR mesh and interpolation stencil; BOTH "
              "runs both paths with identical reference/trajectory settings")
    )
    parser.add_argument("-np", type=int, default=4, help="MPI ranks")
    parser.add_argument("-nt", type=int, default=16,
                        help="intra-rank trajectory threads for the selected solver")
    parser.add_argument("--amps", default="./amps", help="AMPS executable, relative to launch directory")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher")
    parser.add_argument("--workdir", default=None, help="output root; default: test_output/C6")
    parser.add_argument("--mover", type=parse_mover_name, default=None, metavar="NAME", help="optional AMPS mover override")
    parser.add_argument("--scheduler", choices=("DYNAMIC", "BLOCK_CYCLIC", "STATIC"), default="DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=1,
                        help="MPI dynamic-scheduler chunk for either solver")

    parser.add_argument(
        "--mode3d-mesh-res-earth-re", type=float, default=0.02, metavar="DRE",
        help=("GRIDDED AMR cell-size target near one Earth radius, in Re; "
              "smaller values reduce interpolation error but increase mesh cost")
    )
    parser.add_argument(
        "--mode3d-mesh-res-boundary-re", type=float, default=2.0, metavar="DRE",
        help="GRIDDED AMR cell-size target at the outer tracing boundary, in Re"
    )
    parser.add_argument(
        "--mode3d-mesh-coarsening", choices=MODE3D_MESH_COARSENING_CHOICES,
        default="LOG",
        help="GRIDDED radial AMR resolution profile"
    )
    parser.add_argument(
        "--mode3d-mesh-exponent", type=float, default=1.0,
        help="GRIDDED POWER-profile exponent; retained numerically for all profiles"
    )

    parser.add_argument("--complete-epochs", default="1965,1980,1990,1995,2000,2010")
    parser.add_argument("--complete-grid-step-deg", type=float, default=10.0,
                        help="sampled CARI shell spacing; the archive remains complete")
    parser.add_argument("--full-grid", action="store_true",
                        help="run CARI at the complete one-degree resolution")
    parser.add_argument("--modern-epochs", default="2015",
                        help="Gerontidou epochs to run; 2015 is bundled")
    parser.add_argument("--modern-reference", action="append", default=[], metavar="CSV",
                        help="additional Gerontidou-format CSV/CSV.GZ, e.g. authoritative 2010 or 2020 grids")

    parser.add_argument("--rigidity-min-gv", type=float, default=0.01)
    parser.add_argument("--rigidity-max-gv", type=float, default=20.0)
    parser.add_argument("--cutoff-scan-n", type=int, default=2000,
                        help="LINEAR scan vertices; 2000 gives 0.01 GV spacing over 0.01..20 GV")
    parser.add_argument("--cutoff-trace-policy", choices=("ACCURATE", "LEGACY"), default="ACCURATE")
    parser.add_argument(
        "--backtrace-charge", choices=("REVERSED", "SAME"), default="REVERSED",
        help=("charge convention for the outward reverse trajectory; REVERSED "
              "matches Smart--Shea/CARI antiparticle backtracing, while SAME "
              "reproduces the historical AMPS convention")
    )
    parser.add_argument(
        "--trace-limit-policy", choices=("FORBIDDEN", "UNRESOLVED"), default="FORBIDDEN",
        help=("classification of time/step/distance caps in PENUMBRA_SCAN; "
              "FORBIDDEN matches finite-trajectory cutoff tables, while UNRESOLVED "
              "is the strict C14-style three-state policy")
    )
    parser.add_argument(
        "--domain-half-size-re", type=float, default=25.0,
        help=("outer Cartesian tracing-box half-size in Earth radii; the "
              "runner converts this value to numerical kilometers in the "
              "AMPS input")
    )
    parser.add_argument("--dt-trace", type=float, default=0.2)
    parser.add_argument("--adaptive-dt", choices=("T", "F"), default="T")
    parser.add_argument("--max-steps", type=int, default=500000)
    parser.add_argument("--max-trace-time", type=float, default=600.0)
    parser.add_argument("--max-trace-distance", type=float, default=400.0)
    parser.add_argument("--trap-detection", choices=("T", "F"), default="T")

    parser.add_argument("--abs-tol-gv", type=float, default=0.50,
                        help="minimum per-point absolute tolerance")
    parser.add_argument("--rel-tol", type=float, default=0.15,
                        help="per-point relative tolerance above --relative-floor-gv")
    parser.add_argument("--relative-floor-gv", type=float, default=0.25,
                        help="reference floor used in the relative tolerance")
    parser.add_argument("--required-valid-fraction", type=float, default=0.98)
    parser.add_argument("--required-pass-fraction", type=float, default=0.85)
    parser.add_argument("--rmse-max-gv", type=float, default=0.60)

    parser.add_argument("--output-file", default=None,
                        help="analyze one explicit PENUMBRA_SCAN output; valid only for one expanded case")
    parser.add_argument(
        "--plot-formats", default="png,eps", metavar="FMT[,FMT...]",
        help=("Comma-separated Matplotlib output formats for each case's "
              "C6_comparison plot, e.g. 'png,eps', 'png', or 'png,eps,pdf,svg'; "
              "ignored entirely if Matplotlib is not installed"),
    )
    parser.add_argument("--skip-run", action="store_true", help="analyze outputs in existing case directories")
    parser.add_argument("--keep", action="store_true", help="do not delete the existing work directory")
    parser.add_argument("--dry-run", action="store_true", help="render inputs and commands without launching AMPS")
    parser.add_argument("--validate-references", action="store_true",
                        help="validate bundled/user reference files and exit")
    return parser.parse_args(argv)


def validate_args(args: argparse.Namespace) -> None:
    if args.np < 1 or args.nt < 1:
        raise SystemExit("-np and -nt must be positive")
    if not (args.complete_grid_step_deg > 0.0):
        raise SystemExit("--complete-grid-step-deg must be positive")
    if not (args.rigidity_max_gv > args.rigidity_min_gv >= 0.0):
        raise SystemExit("require 0 <= rigidity-min < rigidity-max")
    if args.cutoff_scan_n < 2:
        raise SystemExit("--cutoff-scan-n must be at least 2")
    scan_step_gv = (args.rigidity_max_gv - args.rigidity_min_gv) / (args.cutoff_scan_n - 1)
    if scan_step_gv > 0.011:
        print(
            "C6 warning: requested rigidity spacing is %.5g GV; the published "
            "tables use approximately 0.01 GV. Use the default "
            "--cutoff-scan-n 2000 for the reference-resolution validation." % scan_step_gv,
            file=sys.stderr,
        )
    if args.domain_half_size_re <= 1.01:
        raise SystemExit(
            "--domain-half-size-re must exceed the Earth/shell radius; "
            "use a magnetospheric value such as 25"
        )
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be non-negative")
    if args.mode3d_mesh_res_earth_re <= 0.0:
        raise SystemExit("--mode3d-mesh-res-earth-re must be positive")
    if args.mode3d_mesh_res_boundary_re <= 0.0:
        raise SystemExit("--mode3d-mesh-res-boundary-re must be positive")
    if args.mode3d_mesh_exponent <= 0.0:
        raise SystemExit("--mode3d-mesh-exponent must be positive")
    for name in ("required_valid_fraction", "required_pass_fraction"):
        value = getattr(args, name)
        if not (0.0 <= value <= 1.0):
            raise SystemExit("--%s must be in [0,1]" % name.replace("_", "-"))
    if args.output_file and (args.subtest == "ALL" or args.solver == "BOTH"):
        raise SystemExit(
            "--output-file requires one --subtest, one expanded epoch, and one solver"
        )
    supported_plot_formats = {"png", "eps", "pdf", "svg"}
    requested_plot_formats = tuple(dict.fromkeys(
        fmt.strip().lower().lstrip(".") for fmt in args.plot_formats.split(",") if fmt.strip()
    ))
    unsupported_plot_formats = sorted(set(requested_plot_formats) - supported_plot_formats)
    if unsupported_plot_formats:
        raise SystemExit(
            "--plot-formats has unsupported entries: %s (expected any of %s)"
            % (", ".join(unsupported_plot_formats), ", ".join(sorted(supported_plot_formats)))
        )
    args.plot_formats = requested_plot_formats


def selected_solvers(args: argparse.Namespace) -> Tuple[str, ...]:
    """Expand the user-facing BOTH selector into deterministic execution order."""
    if args.solver == "BOTH":
        return ("GRIDLESS", "GRIDDED")
    return (args.solver,)


def build_amps_command(args: argparse.Namespace, amps_path: Path, solver: str) -> List[str]:
    """Construct one solver command while keeping all physical options common.

    Only the field-evaluation/scheduling backend differs between the branches.
    The rigidity scan, mover, trace policy, epoch, charge convention, shell
    geometry, and reference subset remain identical and are supplied through the
    generated numerical input or the common command prefix.
    """
    mode = "gridless" if solver == "GRIDLESS" else "3d"
    command = [
        args.mpirun, "-np", str(args.np), str(amps_path),
        "-mode", mode, "-i", "AMPS_PARAM_C6.in",
        "-cutoff-search", "PENUMBRA_SCAN",
        "-cutoff-upper-scan-n", str(args.cutoff_scan_n),
        "-cutoff-trace-policy", args.cutoff_trace_policy,
    ]
    if args.mover:
        command += ["-mover", args.mover]

    if solver == "GRIDLESS":
        command += [
            "-gridless-mpi-scheduler", args.scheduler,
            "-gridless-mpi-dynamic-chunk", str(args.dynamic_chunk),
            "-density-parallel", "THREADS",
            "-density-threads", str(args.nt),
        ]
    else:
        command += [
            "-mode3d-field-eval", "MESH",
            "-mode3d-parallel", "THREADS",
            "-mode3d-threads", str(args.nt),
            "-mode3d-mpi-scheduler", args.scheduler,
            "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
            "-mode3d-mesh-res-earth-re", "%.12g" % args.mode3d_mesh_res_earth_re,
            "-mode3d-mesh-res-boundary-re", "%.12g" % args.mode3d_mesh_res_boundary_re,
            "-mode3d-mesh-coarsening", args.mode3d_mesh_coarsening,
            "-mode3d-mesh-exponent", "%.12g" % args.mode3d_mesh_exponent,
        ]
    return command


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    validate_args(args)
    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    try:
        plans, inventories = build_case_plans(args, script_dir)
    except Exception as exc:
        print("%s reference setup failed: %s" % (TEST_ID, exc), file=sys.stderr)
        return 2

    solvers = selected_solvers(args)
    if args.output_file and (len(plans) != 1 or len(solvers) != 1):
        print("--output-file requires exactly one expanded C6 case and one solver",
              file=sys.stderr)
        return 2

    default_work_name = TEST_ID if args.subtest == "ALL" else "%s_%s" % (TEST_ID, args.subtest.lower())
    if args.solver == "GRIDDED":
        default_work_name += "_gridded"
    elif args.solver == "BOTH":
        default_work_name += "_both"
    work_root = Path(args.workdir) if args.workdir else Path("test_output") / default_work_name
    if not work_root.is_absolute():
        work_root = (launch_dir / work_root).resolve()

    if args.validate_references:
        work_root.mkdir(parents=True, exist_ok=True)
        report = work_root / "C6_reference_inventory.json"
        report.write_text(json.dumps(inventories, indent=2) + "\n")
        for item in inventories:
            print("%s: %d rows, epochs=%s" %
                  (item["path"], item["row_count"], item["epochs"]))
        print("Reference validation passed: %s" % report)
        return 0

    if not args.skip_run and not args.keep and work_root.exists():
        shutil.rmtree(work_root)
    work_root.mkdir(parents=True, exist_ok=True)
    (work_root / "C6_reference_inventory.json").write_text(
        json.dumps(inventories, indent=2) + "\n"
    )

    amps_path = Path(args.amps)
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()
    commands: List[Dict[str, object]] = []
    all_metrics: List[CaseMetrics] = []
    overall_pass = True

    for plan in plans:
        for solver in solvers:
            # Preserve the historical directory layout for the default one-solver
            # run.  BOTH needs a solver level so the two AMPS executables cannot
            # overwrite identically named inputs, logs, Tecplot files, or plots.
            case_dir = work_root / plan.case_name
            if len(solvers) > 1:
                case_dir = case_dir / solver.lower()

            # The analysis products belong in the case directory even when
            # --skip-run or --output-file supplies an externally generated file.
            case_dir.mkdir(parents=True, exist_ok=True)
            template = script_dir / (
                "AMPS_PARAM_C6_gridless.in" if solver == "GRIDLESS"
                else "AMPS_PARAM_C6_mode3d.in"
            )
            if not args.skip_run:
                render_input(template, case_dir / "AMPS_PARAM_C6.in", plan, args, solver)
                write_reference_subset(case_dir / "reference_C6_selected.csv", plan.reference_rows)

            command = build_amps_command(args, amps_path, solver)
            case_label = "%s/%s" % (plan.case_name, solver.lower())
            commands.append({
                "case": plan.case_name,
                "solver": solver,
                "cwd": str(case_dir),
                "command": command,
            })
            print("%s %s command:\n  %s" % (TEST_ID, case_label, " ".join(command)))

            if not args.skip_run and not args.dry_run:
                rc = run_command(command, case_dir, case_dir / "C6_amps.log")
                if rc != 0:
                    print("AMPS failed for %s with exit code %d" % (case_label, rc),
                          file=sys.stderr)
                    return rc

            if args.dry_run:
                continue

            if args.output_file:
                output_path = Path(args.output_file).expanduser().resolve()
            elif solver == "GRIDLESS":
                output_path = case_dir / "cutoff_gridless_shells_penumbra.dat"
            else:
                output_path = case_dir / "cutoff_3d_shells_penumbra.dat"
            if not output_path.exists():
                print("C6 output not found: %s" % output_path, file=sys.stderr)
                return 2
            try:
                model_rows = parse_tecplot_penumbra(output_path)
                detailed, metrics = compare_case(plan, model_rows, args, solver)
            except Exception as exc:
                print("C6 analysis failed for %s: %s" % (case_label, exc),
                      file=sys.stderr)
                return 2
            write_dict_rows(case_dir / "C6_comparison.csv", detailed)
            (case_dir / "C6_result.json").write_text(
                json.dumps(asdict(metrics), indent=2) + "\n"
            )
            plot_paths = make_case_plot(
                detailed, case_dir / "C6_comparison",
                "%s %s %s epoch %d" %
                (TEST_ID, solver, plan.subtest, plan.epoch_year),
                args.plot_formats,
            )
            if plot_paths:
                print("%s %s comparison plot(s): %s" % (
                    TEST_ID, case_label, ", ".join(p.name for p in plot_paths)
                ))
            all_metrics.append(metrics)
            overall_pass = overall_pass and metrics.passed
            print(
                "%s %s: matched=%d/%d valid=%d pass_fraction=%.3f "
                "MAE=%.3f GV RMSE=%.3f GV max=%.3f GV -> %s" %
                (TEST_ID, case_label, metrics.n_matched, metrics.n_reference,
                 metrics.n_valid, metrics.point_pass_fraction,
                 metrics.mean_abs_error_gv, metrics.rmse_gv,
                 metrics.max_abs_error_gv, "PASS" if metrics.passed else "FAIL")
            )

    (work_root / "C6_commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    if args.dry_run:
        print("C6 dry run complete: %s" % work_root)
        return 0

    summary_rows = [asdict(metrics) for metrics in all_metrics]
    write_dict_rows(work_root / "C6_summary.csv", summary_rows)
    (work_root / "C6_result.json").write_text(json.dumps({
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": overall_pass,
        "cases": summary_rows,
    }, indent=2) + "\n")
    print("%s overall: %s" % (TEST_ID, "PASS" if overall_pass else "FAIL"))
    print("Results: %s" % work_root)
    return 0 if overall_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
