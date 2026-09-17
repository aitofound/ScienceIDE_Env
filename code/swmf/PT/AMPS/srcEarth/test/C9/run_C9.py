#!/usr/bin/env python3
"""C9 - PAMELA public-data global-shell cutoff-latitude validation.

C9 compares AMPS with the numerical cutoff AACGM latitudes published in
Adriani et al. (2016), Supporting Information Table S1.  It is deliberately a
public-data test: it does not require the nonpublic Resurs-DK1 attitude,
spacecraft mounting matrix, or event-level PAMELA look directions.

For every selected 94-minute PAMELA interval, the runner evaluates one or more
T05/TS05 magnetic-field snapshots on a 475-km geodetic shell. C9 has two
numerically independent AMPS branches: GRIDLESS directly evaluates the empirical
field along each trajectory, while GRIDDED samples the same field on the Mode3D
AMR mesh and uses mesh interpolation during tracing.

FULL_SCAN uses PENUMBRA_SCAN on the complete shell.  In addition to the
traditional Rc_lower, Rc_effective, and Rc_upper diagnostics, the solver records
the allowed/forbidden state at the seven exact PAMELA rigidity centers.  The
optional GRIDDED-only DIRECT_ACCESS product traces only those seven rigidities in
a configurable latitude band.

Both products are compared with PAMELA through the same PAMELA_T50 observable.
Before AACGM conversion and fitting, both products are restricted to the same
configured absolute-geodetic-latitude band.  This is important because FULL_SCAN
contains the complete shell while DIRECT_ACCESS calculates only the retained
band; fitting unequal latitude domains can otherwise change an isotonic T=0.5
crossing even when the shared access states are identical.  For each rigidity
and hemisphere, C9 interpolates every longitude profile onto a common
absolute-AACGM latitude grid, computes the resolved allowed fraction, fits a
nondecreasing transmission profile with weighted isotonic regression, and
interpolates the latitude where transmission reaches 0.5.  This removes the old
method-dependent comparison in which FULL_SCAN used Rc_effective while
DIRECT_ACCESS used the first binary state transition.  Rc_lower, Rc_effective,
and Rc_upper remain available as diagnostic observables for FULL_SCAN.

An optional access-state consistency gate compares a FULL_SCAN companion product
with a DIRECT_ACCESS product at exactly matching epoch, shell node, and PAMELA
rigidity.  This separates trajectory-classification differences from T50
postprocessing differences without requiring the two expensive calculations to
be launched in the same command.

The default ROUTINE profile uses seven representative Table-S1 intervals and
one magnetic snapshot at each published midpoint.  ``--solver BOTH`` is the
default and executes GRIDLESS and GRIDDED under separate output branches.
FULL selects every interval; ``--interval-samples`` can then approximate the
94-minute observational average with multiple equally spaced field snapshots.

Scientific eligibility requires:
  * the bundled five-minute TS05 event driver supplied for this validation; and
  * the ``aacgmv2`` Python package for epoch-dependent GEO-to-AACGM conversion.

The checked-in ``data/ts05_driving.txt`` file is the default event driver.
Its SHA-256 digest is fixed in the runner so accidental replacement is detected.
An alternate driver is accepted only when it carries official Tsyganenko
provenance, unless ``--allow-unverified-driver`` is supplied explicitly.

Matplotlib is used only when available to create a per-branch PAMELA-vs-AMPS
diagnostic plot, written in every format named by ``--plot-formats`` (PNG and
EPS by default -- a raster copy for quick viewing and a vector copy suitable
for publication); a missing Matplotlib installation never changes pass/fail.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import logging
import math
import os
import re
import shlex
import shutil
import statistics
import subprocess
import sys
from dataclasses import asdict, dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Dict, Iterable, Iterator, List, Mapping, Optional, Sequence, Tuple

TEST_ID = "C9"
TEST_NAME = "PAMELA public-data global-shell cutoff-latitude validation"
PROTON_MASS_AMU = 1.007276466621
AMU_KG = 1.66053906660e-27
C_LIGHT_M_S = 299792458.0
EV_J = 1.602176634e-19
MEV_J = 1.0e6 * EV_J
BUNDLED_DRIVER_SHA256 = "cb3f3f1959763660beb1e26e5a49489b132708944fb91c4e1ee37cfc3a6c4317"
SOLVERS = ("GRIDLESS", "GRIDDED", "BOTH")
CUTOFF_EVALUATIONS = ("FULL_SCAN", "DIRECT_ACCESS")
# PAMELA_T50 is the primary observationally matched quantity.  The three Rc
# contours are retained only for FULL_SCAN sensitivity studies; ALL selects
# PAMELA_T50 for PASS/FAIL while writing every available diagnostic product.
COMPARISON_OBSERVABLES = (
    "PAMELA_T50", "RC_LOWER", "RC_EFFECTIVE", "RC_UPPER", "ALL",
)

PROFILE_TIMES = {
    # Fast end-to-end geometry/output check: shock interval and storm minimum.
    "SMOKE": (
        "2006-12-14T14:31:00Z",
        "2006-12-15T03:03:00Z",
    ),
    # Public-data routine regression: quiet/pre-shock, shock, post-shock,
    # main-phase onset, minimum, recovery, and late recovery.
    "ROUTINE": (
        "2006-12-14T09:49:00Z",
        "2006-12-14T14:31:00Z",
        "2006-12-14T16:05:00Z",
        "2006-12-14T23:55:00Z",
        "2006-12-15T03:03:00Z",
        "2006-12-15T09:19:00Z",
        "2006-12-16T04:08:00Z",
    ),
}


@dataclass(frozen=True)
class ReferenceRow:
    midpoint: datetime
    interval_start: datetime
    interval_end: datetime
    rigidity_min_gv: float
    rigidity_max_gv: float
    rigidity_center_gv: float
    pamela_cutoff_aacgm_deg: Optional[float]
    sigma_plus_deg: Optional[float]
    sigma_minus_deg: Optional[float]
    missing: bool


@dataclass(frozen=True)
class DriverInfo:
    path: str
    sha256: str
    source_kind: str
    source_url: str
    source_archive_sha256: str
    source_member: str
    provenance_complete: bool
    first_epoch: str
    last_epoch: str
    n_records: int
    median_cadence_seconds: float
    maximum_gap_seconds: float
    verified_driver: bool


@dataclass
class ShellRow:
    longitude_deg: float
    latitude_deg: float
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
    aacgm_latitude_deg: Optional[float] = None


@dataclass
class AccessRow:
    """One Mode3D RIGIDITY_LIST trajectory classification.

    ``access_state`` uses the C++ CutoffSampleState numeric contract:
    0=PHYSICAL_FORBIDDEN, 1=ALLOWED, 2=UNRESOLVED.  The duplicated Boolean
    ``allowed`` and ``unresolved`` columns are retained in the AMPS output for
    human readability; the parser verifies that they agree with the state code.
    """

    longitude_deg: float
    latitude_deg: float
    rigidity_gv: float
    access_state: int
    allowed: int
    unresolved: int
    aacgm_latitude_deg: Optional[float] = None


@dataclass
class BoundaryEstimate:
    """One modeled cutoff-latitude estimate and its numerical diagnostics.

    The legacy Rc-contour estimators and the PAMELA_T50 estimator share this
    record so temporal aggregation and acceptance logic cannot accidentally use
    different schemas.  T50-specific diagnostics are ``None`` for Rc contours.
    """

    rigidity_gv: float
    observable: str
    cutoff_aacgm_deg: Optional[float]
    cutoff_mean_deg: Optional[float]
    cutoff_std_deg: Optional[float]
    cutoff_min_deg: Optional[float]
    cutoff_max_deg: Optional[float]
    north_median_deg: Optional[float]
    south_median_deg: Optional[float]
    north_south_difference_deg: Optional[float]
    n_valid_crossings: int
    n_requested_crossings: int
    n_nonmonotonic_profiles: int
    raw_cutoff_aacgm_deg: Optional[float] = None
    isotonic_adjustment_rms: Optional[float] = None
    minimum_resolved_longitude_fraction: Optional[float] = None
    mean_resolved_longitude_fraction: Optional[float] = None
    unresolved_access_fraction: Optional[float] = None
    n_bracketed_hemispheres: Optional[int] = None
    minimum_boundary_edge_margin_deg: Optional[float] = None


@dataclass
class Metrics:
    n_reference: int
    n_valid_model: int
    valid_fraction: float
    mean_bias_deg: float
    mean_abs_error_deg: float
    rmse_deg: float
    max_abs_error_deg: float
    correlation: Optional[float]
    weighted_rms_normalized_residual: float
    fraction_within_1sigma: float
    fraction_within_2sigma: float
    observed_low_rigidity_suppression_deg: Optional[float]
    modeled_low_rigidity_suppression_deg: Optional[float]
    observed_minimum_time_utc: Optional[str]
    modeled_minimum_time_utc: Optional[str]
    minimum_time_error_minutes: Optional[float]
    driver_verified: bool
    scientific_validation_eligible: bool
    passed_numerical_comparison: bool
    passed: bool


def parse_utc(text: str) -> datetime:
    value = str(text).strip().replace("Z", "+00:00")
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt.astimezone(timezone.utc)


def format_utc(dt: datetime, suffix_z: bool = True) -> str:
    value = dt.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
    return value + ("Z" if suffix_z else "")


def kinetic_energy_mev_from_rigidity_gv(rigidity_gv: float) -> float:
    if rigidity_gv < 0.0:
        raise ValueError("rigidity must be non-negative")
    rest_mev = PROTON_MASS_AMU * AMU_KG * C_LIGHT_M_S ** 2 / MEV_J
    momentum_mev_c = rigidity_gv * 1000.0  # proton |Z|=1
    return math.sqrt(momentum_mev_c ** 2 + rest_mev ** 2) - rest_mev


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_reference(path: Path) -> List[ReferenceRow]:
    rows: List[ReferenceRow] = []
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        required = {
            "interval_midpoint_utc", "interval_start_utc", "interval_end_utc",
            "rigidity_min_gv", "rigidity_max_gv",
            "rigidity_geometric_center_gv", "pamela_cutoff_aacgm_deg",
            "sigma_plus_deg", "sigma_minus_deg", "missing",
        }
        missing_columns = required.difference(reader.fieldnames or ())
        if missing_columns:
            raise ValueError("reference CSV is missing columns: %s" %
                             ", ".join(sorted(missing_columns)))
        for record in reader:
            missing = record["missing"].strip().upper() in ("TRUE", "T", "1", "YES")
            def optional_float(name: str) -> Optional[float]:
                value = record[name].strip()
                return None if value == "" else float(value)
            lo = float(record["rigidity_min_gv"])
            hi = float(record["rigidity_max_gv"])
            center = float(record["rigidity_geometric_center_gv"])
            if not math.isclose(center, math.sqrt(lo * hi), rel_tol=0.0, abs_tol=5.0e-9):
                raise ValueError("reference rigidity center is not geometric: %s" % record)
            row = ReferenceRow(
                midpoint=parse_utc(record["interval_midpoint_utc"]),
                interval_start=parse_utc(record["interval_start_utc"]),
                interval_end=parse_utc(record["interval_end_utc"]),
                rigidity_min_gv=lo,
                rigidity_max_gv=hi,
                rigidity_center_gv=center,
                pamela_cutoff_aacgm_deg=optional_float("pamela_cutoff_aacgm_deg"),
                sigma_plus_deg=optional_float("sigma_plus_deg"),
                sigma_minus_deg=optional_float("sigma_minus_deg"),
                missing=missing,
            )
            if missing != (row.pamela_cutoff_aacgm_deg is None):
                raise ValueError("reference missing flag/value disagree: %s" % record)
            rows.append(row)

    # The checked-in Table-S1 transcription has 37 epochs x 7 bins and one
    # explicitly missing datum.  These checks catch accidental truncation,
    # spreadsheet reshaping, or silent replacement by a different table.
    midpoints = sorted({row.midpoint for row in rows})
    bins = sorted({(row.rigidity_min_gv, row.rigidity_max_gv) for row in rows})
    if len(rows) != 259 or len(midpoints) != 37 or len(bins) != 7:
        raise ValueError(
            "unexpected Table-S1 inventory: %d rows, %d epochs, %d bins" %
            (len(rows), len(midpoints), len(bins))
        )
    if sum(row.missing for row in rows) != 1:
        raise ValueError("Table-S1 transcription must contain exactly one missing datum")
    for midpoint in midpoints:
        group = [row for row in rows if row.midpoint == midpoint]
        if len(group) != 7:
            raise ValueError("midpoint %s does not contain seven rigidity bins" % midpoint)
        if any((row.interval_end - row.interval_start) != timedelta(minutes=94)
               for row in group):
            raise ValueError("reference interval is not 94 minutes")
    return rows


def driver_metadata(path: Path) -> Tuple[Dict[str, str], List[Tuple[datetime, List[str]]]]:
    metadata: Dict[str, str] = {}
    records: List[Tuple[datetime, List[str]]] = []
    header_seen = False
    with path.open(encoding="utf-8") as stream:
        for line_number, raw in enumerate(stream, start=1):
            text = raw.strip()
            if not text:
                continue
            if text.startswith("#"):
                payload = text[1:].strip()
                if payload.startswith("C9_DRIVER_"):
                    key, _, value = payload.partition(" ")
                    metadata[key] = value.strip()
                if "YYYY-MM-DDTHH:MM:SS" in payload:
                    header_seen = True
                continue
            fields = text.split()
            if len(fields) != 20:
                raise ValueError(
                    "driver line %d has %d fields; expected timestamp + 19 values" %
                    (line_number, len(fields))
                )
            epoch = parse_utc(fields[0])
            try:
                [float(value) for value in fields[1:]]
            except ValueError as exc:
                raise ValueError("nonnumeric driver value at line %d" % line_number) from exc
            if records and epoch <= records[-1][0]:
                raise ValueError("driver epochs are not strictly increasing at line %d" % line_number)
            records.append((epoch, fields[1:]))
    if not header_seen:
        raise ValueError("driver does not contain the AMPS timestamp/column header")
    if len(records) < 2:
        raise ValueError("driver contains fewer than two numerical records")
    return metadata, records


def validate_driver(path: Path, required_times: Sequence[datetime]) -> DriverInfo:
    metadata, records = driver_metadata(path)
    gaps = [(b[0] - a[0]).total_seconds() for a, b in zip(records, records[1:])]
    median_cadence = statistics.median(gaps)
    maximum_gap = max(gaps)
    if not 299.0 <= median_cadence <= 301.0:
        raise ValueError("driver median cadence is not five minutes: %.1f s" % median_cadence)
    if maximum_gap > 600.0:
        raise ValueError("driver has a gap larger than 10 minutes: %.1f s" % maximum_gap)
    first, last = records[0][0], records[-1][0]
    if required_times:
        if min(required_times) < first or max(required_times) > last:
            raise ValueError(
                "driver coverage %s .. %s does not contain requested epochs %s .. %s" %
                (format_utc(first), format_utc(last), format_utc(min(required_times)),
                 format_utc(max(required_times)))
            )
    file_hash = sha256(path)
    bundled = file_hash.lower() == BUNDLED_DRIVER_SHA256
    source_kind = metadata.get(
        "C9_DRIVER_SOURCE_KIND",
        "BUNDLED_C9_TS05_EVENT" if bundled else "UNVERIFIED",
    )
    source_url = metadata.get("C9_DRIVER_SOURCE_URL", "")
    source_archive_sha256 = metadata.get(
        "C9_DRIVER_SOURCE_ARCHIVE_SHA256", file_hash if bundled else ""
    )
    source_member = metadata.get(
        "C9_DRIVER_SOURCE_MEMBER", path.name if bundled else ""
    )
    official_url = bool(re.match(
        r"^https?://geo\.phys\.spbu\.ru/~tsyganenko/TS05_data_and_stuff/",
        source_url, re.IGNORECASE,
    ))
    archive_hash_valid = bool(re.fullmatch(r"[0-9a-fA-F]{64}", source_archive_sha256))
    member_valid = bool(re.fullmatch(
        r"2006_OMNI_5m_with_TS05_variables\.(?:dat|txt)",
        Path(source_member).name, re.IGNORECASE,
    ))
    official_provenance = official_url and archive_hash_valid and member_valid
    official_verified = (
        source_kind == "TSYGANENKO_TS05_OFFICIAL" and official_provenance
    )
    provenance_complete = bundled or official_provenance
    verified = bundled or official_verified
    return DriverInfo(
        path=str(path.resolve()),
        sha256=file_hash,
        source_kind=source_kind,
        source_url=source_url,
        source_archive_sha256=source_archive_sha256,
        source_member=source_member,
        provenance_complete=provenance_complete,
        first_epoch=format_utc(first),
        last_epoch=format_utc(last),
        n_records=len(records),
        median_cadence_seconds=median_cadence,
        maximum_gap_seconds=maximum_gap,
        verified_driver=verified,
    )


def selected_midpoints(reference: Sequence[ReferenceRow], args: argparse.Namespace) -> List[datetime]:
    available = sorted({row.midpoint for row in reference})
    if args.timestamps:
        selected = [parse_utc(token) for token in args.timestamps.split(",") if token.strip()]
    elif args.profile == "FULL":
        selected = available
    else:
        selected = [parse_utc(token) for token in PROFILE_TIMES[args.profile]]
    missing = [value for value in selected if value not in available]
    if missing:
        raise ValueError("requested timestamps are absent from Table S1: %s" %
                         ", ".join(format_utc(value) for value in missing))
    return selected


def interval_sample_times(rows: Sequence[ReferenceRow], count: int) -> List[datetime]:
    if count < 1:
        raise ValueError("--interval-samples must be >= 1")
    midpoint = rows[0].midpoint
    if count == 1:
        return [midpoint]
    start, end = rows[0].interval_start, rows[0].interval_end
    # Use bin centers rather than exact endpoints.  This represents the full
    # 94-minute interval without over-weighting two endpoint samples that are
    # shared conceptually with neighboring orbital intervals.
    width = (end - start).total_seconds() / count
    return [start + timedelta(seconds=(index + 0.5) * width) for index in range(count)]


def replace_directives(template_text: str, replacements: Mapping[str, str]) -> str:
    remaining = set(replacements)
    output = []
    for raw in template_text.splitlines():
        stripped = raw.lstrip()
        if stripped and not stripped.startswith(("!", "#")):
            key = stripped.split(None, 1)[0].upper()
            if key in replacements:
                indent = raw[:len(raw) - len(stripped)]
                output.append("%s%-36s%s" % (indent, key, replacements[key]))
                remaining.remove(key)
                continue
        output.append(raw)
    if remaining:
        raise ValueError("template does not contain directives: %s" %
                         ", ".join(sorted(remaining)))
    return "\n".join(output) + "\n"


def selected_solvers(value: str) -> Tuple[str, ...]:
    """Expand the requested C9 solver branch selection."""
    return ("GRIDLESS", "GRIDDED") if value == "BOTH" else (value,)


def resolved_dynamic_chunk(args: argparse.Namespace, solver: str) -> int:
    """Resolve the scheduler chunk for one solver branch.

    A zero value is an automatic setting: one shell location for GRIDLESS and
    one location per Mode3D thread for GRIDDED.  A positive value is passed
    through unchanged.  The two branches therefore expose the same user-facing
    control without silently disabling the Mode3D thread pool.
    """
    if args.dynamic_chunk > 0:
        return args.dynamic_chunk
    return 1 if solver == "GRIDLESS" else max(1, args.nt)


def render_input(template: Path, destination: Path, epoch: datetime,
                 driver: Path, args: argparse.Namespace, solver: str) -> None:
    replacements = {
        "CUTOFF_EMIN": "%.15g" % kinetic_energy_mev_from_rigidity_gv(args.rigidity_min_gv),
        "CUTOFF_EMAX": "%.15g" % kinetic_energy_mev_from_rigidity_gv(args.rigidity_max_gv),
        "CUTOFF_SEARCH_ALGORITHM": (
            "RIGIDITY_LIST" if args.cutoff_evaluation == "DIRECT_ACCESS"
            else "PENUMBRA_SCAN"
        ),
        "CUTOFF_UPPER_SCAN_N": str(args.cutoff_scan_n),
        "CUTOFF_RIGIDITY_LIST_GV": ",".join(
            "%.12g" % value for value in args.direct_rigidities_gv
        ),
        "CUTOFF_ACCESS_ABS_LAT_MIN": "%.8g" % args.access_abs_lat_min_deg,
        "CUTOFF_ACCESS_ABS_LAT_MAX": "%.8g" % args.access_abs_lat_max_deg,
        "CUTOFF_MAX_TRAJ_TIME": "%.8g" % args.max_trace_time,
        "EPOCH": format_utc(epoch, suffix_z=False),
        "DRIVER_FILE": str(driver.resolve()),
        "SHELL_ALTS_KM": "%.8g" % args.altitude_km,
        "SHELL_LON_RES_DEG": "%.8g" % args.shell_lon_res_deg,
        "SHELL_LAT_RES_DEG": "%.8g" % args.shell_lat_res_deg,
        "MAX_TRACE_TIME": "%.8g" % args.max_trace_time,
    }
    if solver == "GRIDDED":
        replacements.update({
            "MODE3D_MESH_RES_EARTH_RE": "%.8g" % args.mode3d_mesh_res_earth_re,
            "MODE3D_MESH_RES_BOUNDARY_RE": "%.8g" % args.mode3d_mesh_res_boundary_re,
            "MODE3D_MESH_COARSENING": args.mode3d_mesh_coarsening,
            "MODE3D_MESH_EXPONENT": "%.8g" % args.mode3d_mesh_exponent,
        })
    destination.write_text(replace_directives(template.read_text(), replacements))


def command_for(args: argparse.Namespace, amps: Path, solver: str,
                epoch: datetime) -> List[str]:
    """Build the exact per-snapshot AMPS command shown in logs and provenance.

    The epoch is passed both in the generated input deck and explicitly on the
    command line. The visible CLI override makes five-snapshot runs auditable and
    protects against accidentally launching a sample from the wrong working
    directory while retaining the self-contained input file.
    """
    chunk = resolved_dynamic_chunk(args, solver)
    command = [
        args.mpirun, "-np", str(args.np), str(amps),
        "-mode", "gridless" if solver == "GRIDLESS" else "3d",
        "-i", "AMPS_PARAM_C9.in", "--epoch", format_utc(epoch, suffix_z=False),
        "-cutoff-search", (
            "RIGIDITY_LIST" if args.cutoff_evaluation == "DIRECT_ACCESS"
            else "PENUMBRA_SCAN"
        ),
        "-cutoff-trace-policy", args.cutoff_trace_policy,
    ]
    if args.cutoff_evaluation == "FULL_SCAN":
        # The exact list is saved as a companion access-state product by the
        # updated PENUMBRA_SCAN solvers.  Passing it visibly on the command line
        # makes the observational comparison reproducible from C9_commands.json.
        command += [
            "-cutoff-upper-scan-n", str(args.cutoff_scan_n),
            "-cutoff-rigidity-list-gv",
            ",".join("%.12g" % value for value in args.direct_rigidities_gv),
        ]
    else:
        command += [
            "-cutoff-rigidity-list-gv",
            ",".join("%.12g" % value for value in args.direct_rigidities_gv),
            "-cutoff-access-abs-lat-min", str(args.access_abs_lat_min_deg),
            "-cutoff-access-abs-lat-max", str(args.access_abs_lat_max_deg),
        ]
    if solver == "GRIDLESS":
        command += [
            "-gridless-mpi-scheduler", args.scheduler,
            "-gridless-mpi-dynamic-chunk", str(chunk),
        ]
    else:
        command += [
            "-mode3d-field-eval", "MESH",
            "-mode3d-parallel", "THREADS",
            "-mode3d-threads", str(args.nt),
            "-mode3d-mpi-scheduler", args.scheduler,
            "-mode3d-mpi-dynamic-chunk", str(chunk),
            "-mode3d-mesh-res-earth-re", str(args.mode3d_mesh_res_earth_re),
            "-mode3d-mesh-res-boundary-re", str(args.mode3d_mesh_res_boundary_re),
            "-mode3d-mesh-coarsening", args.mode3d_mesh_coarsening,
            "-mode3d-mesh-exponent", str(args.mode3d_mesh_exponent),
        ]
        if args.mode3d_parallel_field_init:
            command.append("-mode3d-parallel-field-init")
    if args.mover:
        command += ["-mover", args.mover]
    return command


def run_process(command: Sequence[str], cwd: Path, log_path: Path) -> int:
    with log_path.open("w") as log:
        log.write("Command:\n  %s\n\n" % " ".join(command)); log.flush()
        process = subprocess.Popen(
            list(command), cwd=str(cwd), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True,
        )
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line); sys.stdout.flush()
            log.write(line); log.flush()
        return process.wait()


def normalize_tecplot_variable_name(name: str) -> str:
    """Normalize a quoted Tecplot variable name for solver-independent lookup."""
    return name.strip().lower().replace("-", "_").replace(" ", "_")


def parse_tecplot_shell_penumbra(path: Path) -> List[ShellRow]:
    """Read common cutoff columns from either GRIDLESS or GRIDDED output.

    The two AMPS branches intentionally write different diagnostic schemas:

      * GRIDLESS writes 21 columns, including three Størmer-comparison fields;
      * GRIDDED writes 18 columns and omits those gridless-only diagnostics.

    Earlier C9 postprocessing assumed the 21-column GRIDLESS layout for both
    branches.  That made a valid ``cutoff_3d_shells_penumbra.dat`` fail with
    ``expected 21``.  This parser uses the Tecplot ``VARIABLES`` record instead
    of fixed column offsets and therefore accepts either layout while still
    requiring every field used by the scientific C9 comparison.
    """
    variables: List[str] = []
    numeric_rows: List[Tuple[int, List[float]]] = []

    with path.open() as stream:
        for line_number, raw in enumerate(stream, start=1):
            text = raw.strip()
            if not text:
                continue
            upper = text.upper()
            if upper.startswith("VARIABLES"):
                variables = [
                    normalize_tecplot_variable_name(value)
                    for value in re.findall(r'"([^"]+)"', text)
                ]
                continue
            if upper.startswith(("TITLE", "ZONE")):
                continue
            try:
                values = [float(token) for token in text.split()]
            except ValueError as exc:
                raise ValueError(
                    "%s line %d is not a numeric Tecplot row: %s" %
                    (path, line_number, text)
                ) from exc
            numeric_rows.append((line_number, values))

    if not variables:
        raise ValueError("Tecplot VARIABLES record not found in %s" % path)

    required = {
        "lon_deg", "lat_deg", "rc_lower_gv", "rc_effective_gv", "rc_upper_gv",
        "n_allowed_intervals", "n_transitions", "n_unresolved",
        "lower_bracket_unresolved", "upper_bracket_unresolved",
        "lower_below_range", "lower_above_range",
        "upper_below_range", "upper_above_range",
    }
    missing = sorted(required - set(variables))
    if missing:
        raise ValueError(
            "%s is missing required C9 Tecplot variable(s): %s" %
            (path, ", ".join(missing))
        )

    index = {name: variables.index(name) for name in required}
    rows: List[ShellRow] = []
    for line_number, values in numeric_rows:
        if len(values) != len(variables):
            raise ValueError(
                "%s line %d has %d columns, but its VARIABLES record defines %d" %
                (path, line_number, len(values), len(variables))
            )
        rows.append(ShellRow(
            longitude_deg=float(values[index["lon_deg"]]) % 360.0,
            latitude_deg=float(values[index["lat_deg"]]),
            rc_lower_gv=float(values[index["rc_lower_gv"]]),
            rc_effective_gv=float(values[index["rc_effective_gv"]]),
            rc_upper_gv=float(values[index["rc_upper_gv"]]),
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
        raise ValueError("no shell rows parsed from %s" % path)
    return rows


def parse_tecplot_shell_access(path: Path) -> List[AccessRow]:
    """Read the long-form Mode3D RIGIDITY_LIST shell product.

    The parser uses the Tecplot VARIABLES record rather than fixed positions so
    future diagnostic columns can be appended without changing C9. Required
    scientific fields remain strict, and the redundant state/Boolean columns are
    cross-checked to catch a writer or reduction error immediately.
    """
    variables: List[str] = []
    numeric_rows: List[Tuple[int, List[float]]] = []
    with path.open() as stream:
        for line_number, raw in enumerate(stream, start=1):
            text = raw.strip()
            if not text:
                continue
            upper = text.upper()
            if upper.startswith("VARIABLES"):
                variables = [normalize_tecplot_variable_name(value)
                             for value in re.findall(r'"([^"]+)"', text)]
                continue
            if upper.startswith(("TITLE", "ZONE")):
                continue
            try:
                numeric_rows.append(
                    (line_number, [float(token) for token in text.split()])
                )
            except ValueError as exc:
                raise ValueError(
                    "%s line %d is not a numeric Tecplot row: %s" %
                    (path, line_number, text)
                ) from exc

    required = {"lon_deg", "lat_deg", "rigidity_gv", "access_state",
                "allowed", "unresolved"}
    if not variables:
        raise ValueError("Tecplot VARIABLES record not found in %s" % path)
    missing = sorted(required - set(variables))
    if missing:
        raise ValueError("%s is missing required direct-access variable(s): %s" %
                         (path, ", ".join(missing)))
    index = {name: variables.index(name) for name in required}

    rows: List[AccessRow] = []
    for line_number, values in numeric_rows:
        if len(values) != len(variables):
            raise ValueError(
                "%s line %d has %d columns, but VARIABLES defines %d" %
                (path, line_number, len(values), len(variables))
            )
        state = int(round(values[index["access_state"]]))
        allowed = int(round(values[index["allowed"]]))
        unresolved = int(round(values[index["unresolved"]]))
        if state not in (0, 1, 2):
            raise ValueError("%s line %d has invalid access_state=%d" %
                             (path, line_number, state))
        if allowed != (1 if state == 1 else 0):
            raise ValueError("%s line %d has inconsistent allowed flag" %
                             (path, line_number))
        if unresolved != (1 if state == 2 else 0):
            raise ValueError("%s line %d has inconsistent unresolved flag" %
                             (path, line_number))
        rows.append(AccessRow(
            longitude_deg=float(values[index["lon_deg"]]) % 360.0,
            latitude_deg=float(values[index["lat_deg"]]),
            rigidity_gv=float(values[index["rigidity_gv"]]),
            access_state=state,
            allowed=allowed,
            unresolved=unresolved,
        ))
    if not rows:
        raise ValueError("no direct-access shell rows parsed from %s" % path)
    return rows


def select_common_t50_geodetic_band(rows: Sequence[AccessRow],
                                     minimum_abs_latitude_deg: float,
                                     maximum_abs_latitude_deg: float
                                     ) -> List[AccessRow]:
    """Return access rows inside the latitude domain shared by both products.

    FULL_SCAN writes exact PAMELA-rigidity states on the complete shell, whereas
    DIRECT_ACCESS avoids unnecessary trajectories outside a configurable band.
    The scientific T50 reduction must nevertheless see the same spatial domain:
    weighted isotonic regression is a global fit over the retained latitude
    samples, so extra low- or high-latitude rows in only one product can move the
    fitted half-transmission crossing.  Filtering is performed in *geodetic*
    latitude before AACGM conversion because that is the coordinate in which the
    DIRECT_ACCESS work list is selected by the C++ solver.

    The small tolerance retains nodes that lie on a requested band edge after
    decimal text formatting.  An empty selection is an input/configuration error,
    not a missing scientific result, and is therefore reported immediately.
    """
    tolerance = 1.0e-10
    selected = [
        row for row in rows
        if minimum_abs_latitude_deg - tolerance
        <= abs(float(row.latitude_deg))
        <= maximum_abs_latitude_deg + tolerance
    ]
    if not selected:
        raise ValueError(
            "no fixed-rigidity access rows remain in the common T50 geodetic "
            "latitude band %.6g-%.6g deg" %
            (minimum_abs_latitude_deg, maximum_abs_latitude_deg)
        )
    return selected


def _access_row_key(row: AccessRow) -> Tuple[float, float, float]:
    """Construct a stable identity for one access-state classification.

    Both products originate from the same structured shell and exact PAMELA
    rigidity list, but their Tecplot files are written independently.  Rounding
    only the text-level representation prevents harmless binary conversion noise
    from being mistaken for a missing node while remaining far tighter than any
    physical shell or rigidity resolution used by C9.
    """
    return (
        round(float(row.longitude_deg) % 360.0, 8),
        round(float(row.latitude_deg), 8),
        round(float(row.rigidity_gv), 10),
    )


# Input directives that must be identical when FULL_SCAN and DIRECT_ACCESS are
# used as a method-consistency pair.  Product-selection directives are excluded:
# CUTOFF_SEARCH_ALGORITHM and CUTOFF_UPPER_SCAN_N are intentionally different.
# DRIVER_FILE is compared by file digest rather than path because each output root
# keeps its own provenance copy of the same driver.
ACCESS_CONSISTENCY_INPUT_DIRECTIVES = (
    "CALC_TARGET", "FIELD_EVAL_METHOD",
    "CUTOFF_EMIN", "CUTOFF_EMAX", "CUTOFF_MAX_PARTICLES", "CUTOFF_NENERGY",
    "CUTOFF_SAMPLING", "CUTOFF_RIGIDITY_LIST_GV",
    "CUTOFF_ACCESS_ABS_LAT_MIN", "CUTOFF_ACCESS_ABS_LAT_MAX",
    "CUTOFF_SCAN_SPACING", "CUTOFF_TRACE_POLICY", "CUTOFF_BACKTRACE_CHARGE",
    "CUTOFF_TRACE_LIMIT_POLICY", "CUTOFF_MAX_TRAJ_TIME",
    "SPECIES", "CHARGE", "MASS_AMU", "FIELD_MODEL", "EPOCH",
    "MODE3D_MESH_RES_EARTH_RE", "MODE3D_MESH_RES_BOUNDARY_RE",
    "MODE3D_MESH_COARSENING", "MODE3D_MESH_EXPONENT",
    "DOMAIN_X_MIN", "DOMAIN_X_MAX", "DOMAIN_Y_MIN", "DOMAIN_Y_MAX",
    "DOMAIN_Z_MIN", "DOMAIN_Z_MAX", "R_INNER",
    "OUTPUT_MODE", "SHELL_COUNT", "SHELL_ALTS_KM", "SHELL_LON_RES_DEG",
    "SHELL_LAT_RES_DEG", "SHELL_GEOMETRY", "OUTPUT_COORDS",
    "DT_TRACE", "ADAPTIVE_DT", "MAX_STEPS", "MAX_TRACE_TIME",
    "MAX_TRACE_DISTANCE", "TRAP_DETECTION",
)

# CLI controls that affect the shared GRIDDED trajectory calculation.  The two
# products intentionally use different values for -cutoff-search and have
# product-specific scan/band options, so those are not compared here.
ACCESS_CONSISTENCY_COMMAND_OPTIONS = (
    "-np", "-mode", "--epoch", "-cutoff-trace-policy",
    "-cutoff-rigidity-list-gv", "-mode3d-field-eval", "-mode3d-parallel",
    "-mode3d-threads", "-mode3d-mpi-scheduler",
    # Dynamic chunk controls work scheduling only. It may legitimately differ
    # between FULL_SCAN and DIRECT_ACCESS and must not invalidate a comparison
    # when the complete fixed-rigidity state tables agree.
    "-mode3d-mesh-res-earth-re", "-mode3d-mesh-res-boundary-re",
    "-mode3d-mesh-coarsening", "-mode3d-mesh-exponent", "-mover",
)


def _read_input_directives(path: Path) -> Dict[str, str]:
    """Read one generated AMPS input deck into normalized key/value strings."""
    directives: Dict[str, str] = {}
    with path.open() as stream:
        for raw in stream:
            text = raw.strip()
            if not text or text.startswith(("!", "#")):
                continue
            fields = text.split(None, 1)
            if len(fields) == 2:
                directives[fields[0].upper()] = " ".join(fields[1].split())
    return directives


def _driver_digest_from_input(input_path: Path,
                              directives: Mapping[str, str]) -> Optional[str]:
    """Resolve and hash the driver referenced by a generated snapshot input."""
    value = directives.get("DRIVER_FILE")
    if not value:
        return None
    driver_path = Path(value).expanduser()
    if not driver_path.is_absolute():
        driver_path = input_path.parent / driver_path
    return sha256(driver_path.resolve()) if driver_path.exists() else None


def _read_logged_command(path: Path) -> Optional[List[str]]:
    """Recover the exact command archived at the start of C9_amps.log."""
    if not path.exists():
        return None
    lines = path.read_text(errors="replace").splitlines()
    for index, line in enumerate(lines):
        if line.strip() != "Command:":
            continue
        for candidate in lines[index + 1:]:
            if candidate.strip():
                return shlex.split(candidate.strip())
    return None


def _command_option_value(command: Sequence[str], option: str) -> Optional[str]:
    """Return one option value, treating an omitted -mover as the default mover."""
    try:
        index = list(command).index(option)
    except ValueError:
        return "<DEFAULT>" if option == "-mover" else None
    if index + 1 >= len(command):
        return "<MISSING_VALUE>"
    return str(command[index + 1])


def compare_access_run_configuration(
        current_input: Path,
        counterpart_input: Path,
        current_command: Sequence[str],
        counterpart_command: Optional[Sequence[str]],
        ) -> Tuple[Dict[str, object], List[Dict[str, object]]]:
    """Verify that a paired raw-state comparison uses equivalent calculations.

    Exact state agreement is meaningful only when both products use the same
    epoch, driver, shell, exact rigidity list, field mesh, mover, and trajectory
    controls.  This function checks the generated input decks plus the common
    command-line controls.  It deliberately ignores only the numerical product
    selector itself (PENUMBRA_SCAN versus RIGIDITY_LIST) and the full-scan count.
    """
    differences: List[Dict[str, object]] = []
    if not counterpart_input.exists():
        return {
            "configuration_available": False,
            "n_configuration_differences": 1,
            "passed": False,
            "error": "counterpart AMPS_PARAM_C9.in not found",
        }, [{
            "comparison_status": "CONFIGURATION_FILE_MISSING",
            "configuration_field": "AMPS_PARAM_C9.in",
            "current_value": str(current_input),
            "counterpart_value": str(counterpart_input),
        }]

    current_directives = _read_input_directives(current_input)
    counterpart_directives = _read_input_directives(counterpart_input)
    for name in ACCESS_CONSISTENCY_INPUT_DIRECTIVES:
        first = current_directives.get(name)
        second = counterpart_directives.get(name)
        if first != second:
            differences.append({
                "comparison_status": "CONFIGURATION_MISMATCH",
                "configuration_source": "AMPS_PARAM_C9.in",
                "configuration_field": name,
                "current_value": first,
                "counterpart_value": second,
            })

    current_driver = _driver_digest_from_input(current_input, current_directives)
    counterpart_driver = _driver_digest_from_input(
        counterpart_input, counterpart_directives)
    if current_driver != counterpart_driver or current_driver is None:
        differences.append({
            "comparison_status": "CONFIGURATION_MISMATCH",
            "configuration_source": "driver_sha256",
            "configuration_field": "DRIVER_FILE",
            "current_value": current_driver,
            "counterpart_value": counterpart_driver,
        })

    if counterpart_command is None:
        differences.append({
            "comparison_status": "CONFIGURATION_FILE_MISSING",
            "configuration_source": "C9_amps.log",
            "configuration_field": "Command",
            "current_value": "available",
            "counterpart_value": "unavailable",
        })
    else:
        for option in ACCESS_CONSISTENCY_COMMAND_OPTIONS:
            first = _command_option_value(current_command, option)
            second = _command_option_value(counterpart_command, option)
            if first != second:
                differences.append({
                    "comparison_status": "CONFIGURATION_MISMATCH",
                    "configuration_source": "command_line",
                    "configuration_field": option,
                    "current_value": first,
                    "counterpart_value": second,
                })

    return {
        "configuration_available": counterpart_input.exists()
        and counterpart_command is not None,
        "n_configuration_differences": len(differences),
        "current_driver_sha256": current_driver,
        "counterpart_driver_sha256": counterpart_driver,
        "passed": len(differences) == 0,
    }, differences


def compare_fixed_rigidity_access_states(
        current_rows: Sequence[AccessRow],
        counterpart_rows: Sequence[AccessRow],
        current_product: str,
        counterpart_product: str,
        minimum_resolved_agreement: float,
        maximum_unresolved_fraction: float,
        ) -> Tuple[Dict[str, object], List[Dict[str, object]]]:
    """Compare exact access classifications before any AACGM/T50 processing.

    This is the strongest available method-consistency diagnostic.  At a common
    epoch, shell node, rigidity, field representation, mover, and trace policy,
    FULL_SCAN's companion classification and DIRECT_ACCESS should agree because
    both call the same trajectory classifier.  The function compares only exact
    long-form records and deliberately does not involve interpolation, isotonic
    fitting, or PAMELA reference values.

    Resolved agreement excludes rows for which either product reports UNRESOLVED;
    those rows are tracked separately and are limited by an independent gate.
    Missing keys are always a failure because they usually indicate unequal shell
    settings, latitude bands, rigidity lists, or temporal sample epochs.
    """
    def unique_map(rows: Sequence[AccessRow], label: str) -> Dict[Tuple[float, float, float], AccessRow]:
        result: Dict[Tuple[float, float, float], AccessRow] = {}
        for row in rows:
            key = _access_row_key(row)
            if key in result:
                raise ValueError(
                    "%s contains duplicate access-state key lon=%g lat=%g R=%g" %
                    (label, key[0], key[1], key[2])
                )
            result[key] = row
        return result

    current = unique_map(current_rows, current_product)
    counterpart = unique_map(counterpart_rows, counterpart_product)
    current_keys = set(current)
    counterpart_keys = set(counterpart)
    common_keys = sorted(current_keys.intersection(counterpart_keys))
    union_keys = current_keys.union(counterpart_keys)

    n_resolved_common = 0
    n_resolved_mismatch = 0
    n_unresolved_either = 0
    detail_rows: List[Dict[str, object]] = []

    for key in common_keys:
        first = current[key]
        second = counterpart[key]
        if first.access_state == 2 or second.access_state == 2:
            n_unresolved_either += 1
            continue
        n_resolved_common += 1
        if first.access_state != second.access_state:
            n_resolved_mismatch += 1
            detail_rows.append({
                "longitude_deg": key[0],
                "latitude_deg": key[1],
                "rigidity_gv": key[2],
                "comparison_status": "RESOLVED_STATE_MISMATCH",
                "current_product": current_product,
                "current_access_state": first.access_state,
                "counterpart_product": counterpart_product,
                "counterpart_access_state": second.access_state,
            })

    for key in sorted(current_keys - counterpart_keys):
        row = current[key]
        detail_rows.append({
            "longitude_deg": key[0],
            "latitude_deg": key[1],
            "rigidity_gv": key[2],
            "comparison_status": "MISSING_FROM_COUNTERPART",
            "current_product": current_product,
            "current_access_state": row.access_state,
            "counterpart_product": counterpart_product,
            "counterpart_access_state": None,
        })
    for key in sorted(counterpart_keys - current_keys):
        row = counterpart[key]
        detail_rows.append({
            "longitude_deg": key[0],
            "latitude_deg": key[1],
            "rigidity_gv": key[2],
            "comparison_status": "MISSING_FROM_CURRENT",
            "current_product": current_product,
            "current_access_state": None,
            "counterpart_product": counterpart_product,
            "counterpart_access_state": row.access_state,
        })

    resolved_agreement = (
        1.0 - n_resolved_mismatch / float(n_resolved_common)
        if n_resolved_common else 0.0
    )
    unresolved_fraction = (
        n_unresolved_either / float(len(common_keys)) if common_keys else 1.0
    )
    common_coverage = len(common_keys) / float(len(union_keys)) if union_keys else 0.0
    passed = (
        len(current_keys - counterpart_keys) == 0
        and len(counterpart_keys - current_keys) == 0
        and n_resolved_common > 0
        and resolved_agreement + 1.0e-14 >= minimum_resolved_agreement
        and unresolved_fraction <= maximum_unresolved_fraction + 1.0e-14
    )
    summary: Dict[str, object] = {
        "current_product": current_product,
        "counterpart_product": counterpart_product,
        "n_current_rows": len(current),
        "n_counterpart_rows": len(counterpart),
        "n_common_rows": len(common_keys),
        "n_union_rows": len(union_keys),
        "common_key_coverage_fraction": common_coverage,
        "n_missing_from_current": len(counterpart_keys - current_keys),
        "n_missing_from_counterpart": len(current_keys - counterpart_keys),
        "n_resolved_common": n_resolved_common,
        "n_resolved_mismatch": n_resolved_mismatch,
        "resolved_access_state_agreement_fraction": resolved_agreement,
        "n_unresolved_in_either_product": n_unresolved_either,
        "unresolved_in_either_product_fraction": unresolved_fraction,
        "minimum_required_resolved_agreement": minimum_resolved_agreement,
        "maximum_allowed_unresolved_fraction": maximum_unresolved_fraction,
        "passed": passed,
    }
    return summary, detail_rows


def aggregate_access_consistency_summaries(
        summaries: Sequence[Mapping[str, object]],
        minimum_resolved_agreement: float,
        maximum_unresolved_fraction: float,
        ) -> Dict[str, object]:
    """Combine per-snapshot state comparisons using count-weighted metrics."""
    n_resolved = sum(int(row.get("n_resolved_common", 0)) for row in summaries)
    n_mismatch = sum(int(row.get("n_resolved_mismatch", 0)) for row in summaries)
    n_common = sum(int(row.get("n_common_rows", 0)) for row in summaries)
    n_unresolved = sum(
        int(row.get("n_unresolved_in_either_product", 0)) for row in summaries)
    n_missing_current = sum(
        int(row.get("n_missing_from_current", 0)) for row in summaries)
    n_missing_counterpart = sum(
        int(row.get("n_missing_from_counterpart", 0)) for row in summaries)
    n_configuration_differences = sum(
        int(row.get("n_configuration_differences", 0)) for row in summaries)
    configuration_passed = bool(summaries) and all(
        bool(row.get("configuration_passed", False)) for row in summaries)
    agreement = 1.0 - n_mismatch / float(n_resolved) if n_resolved else 0.0
    unresolved_fraction = n_unresolved / float(n_common) if n_common else 1.0
    passed = (
        bool(summaries)
        and all(bool(row.get("passed", False)) for row in summaries)
        and n_missing_current == 0
        and n_missing_counterpart == 0
        and configuration_passed
        and n_configuration_differences == 0
        and n_resolved > 0
        and agreement + 1.0e-14 >= minimum_resolved_agreement
        and unresolved_fraction <= maximum_unresolved_fraction + 1.0e-14
    )
    return {
        "n_snapshots_compared": len(summaries),
        "n_common_rows": n_common,
        "n_resolved_common": n_resolved,
        "n_resolved_mismatch": n_mismatch,
        "resolved_access_state_agreement_fraction": agreement,
        "n_unresolved_in_either_product": n_unresolved,
        "unresolved_in_either_product_fraction": unresolved_fraction,
        "n_missing_from_current": n_missing_current,
        "n_missing_from_counterpart": n_missing_counterpart,
        "configuration_passed": configuration_passed,
        "n_configuration_differences": n_configuration_differences,
        "minimum_required_resolved_agreement": minimum_resolved_agreement,
        "maximum_allowed_unresolved_fraction": maximum_unresolved_fraction,
        "passed": passed,
    }


def add_aacgm_latitudes(rows: Sequence[object], epoch: datetime,
                        altitude_km: float) -> None:
    try:
        import aacgmv2  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "scientific C9 analysis requires aacgmv2; install with "
            "'python3 -m pip install -r srcEarth/test/C9/requirements.txt'"
        ) from exc
    naive_utc = epoch.astimezone(timezone.utc).replace(tzinfo=None)
    for row in rows:
        try:
            converted = aacgmv2.convert_latlon(
                row.latitude_deg, row.longitude_deg, altitude_km,
                naive_utc, method_code="G2A",
            )
            latitude = float(converted[0])
            row.aacgm_latitude_deg = latitude if math.isfinite(latitude) else None
        except Exception:
            # AACGM is ill-defined for some low-latitude points.  C9 only uses
            # the high-latitude cutoff band, so retain the invalid marker and
            # continue rather than corrupting a whole snapshot.
            row.aacgm_latitude_deg = None


def interpolate_crossing(profile: Sequence[ShellRow], rigidity_gv: float,
                         rc_attribute: str = "rc_effective_gv") -> Tuple[Optional[float], int]:
    usable = [
        row for row in profile
        if row.aacgm_latitude_deg is not None
        and getattr(row, rc_attribute) > 0.0
        and 20.0 <= abs(row.aacgm_latitude_deg) <= 85.0
    ]
    usable.sort(key=lambda row: abs(float(row.aacgm_latitude_deg)))
    transitions: List[float] = []
    for lower, upper in zip(usable, usable[1:]):
        x1 = abs(float(lower.aacgm_latitude_deg))
        x2 = abs(float(upper.aacgm_latitude_deg))
        y1 = getattr(lower, rc_attribute) - rigidity_gv
        y2 = getattr(upper, rc_attribute) - rigidity_gv
        # Moving poleward, the physical cutoff transition is Rc > R to Rc <= R.
        if y1 > 0.0 and y2 <= 0.0:
            if abs(y2 - y1) < 1.0e-14:
                transitions.append(0.5 * (x1 + x2))
            else:
                transitions.append(x1 + (0.0 - y1) * (x2 - x1) / (y2 - y1))
    if not transitions:
        return None, 0
    # The first equator-to-pole transition is the operational cutoff boundary.
    # Additional transitions indicate a nonmonotonic longitude profile and are
    # preserved as a diagnostic instead of silently choosing the last one.
    return transitions[0], len(transitions)


def estimate_boundaries(rows: Sequence[ShellRow], rigidities: Sequence[float],
                        rc_attribute: str = "rc_effective_gv") -> List[BoundaryEstimate]:
    longitudes = sorted({round(row.longitude_deg, 8) for row in rows})
    estimates: List[BoundaryEstimate] = []
    for rigidity in rigidities:
        values: List[float] = []
        north: List[float] = []
        south: List[float] = []
        nonmonotonic = 0
        for longitude in longitudes:
            longitude_rows = [row for row in rows
                              if abs(row.longitude_deg - longitude) < 1.0e-6]
            for hemisphere in (1, -1):
                profile = [
                    row for row in longitude_rows
                    if row.aacgm_latitude_deg is not None
                    and (1 if row.aacgm_latitude_deg >= 0.0 else -1) == hemisphere
                ]
                crossing, n_transitions = interpolate_crossing(
                    profile, rigidity, rc_attribute=rc_attribute
                )
                if n_transitions > 1:
                    nonmonotonic += 1
                if crossing is not None:
                    values.append(crossing)
                    (north if hemisphere > 0 else south).append(crossing)
        n_requested = len(longitudes) * 2
        if values:
            primary = statistics.median(values)
            mean = statistics.fmean(values)
            std = statistics.stdev(values) if len(values) > 1 else 0.0
            north_median = statistics.median(north) if north else None
            south_median = statistics.median(south) if south else None
            ns_diff = (north_median - south_median
                       if north_median is not None and south_median is not None else None)
            estimates.append(BoundaryEstimate(
                rigidity_gv=rigidity,
                observable=rc_attribute.replace("_gv", "").upper(),
                cutoff_aacgm_deg=primary,
                cutoff_mean_deg=mean,
                cutoff_std_deg=std,
                cutoff_min_deg=min(values),
                cutoff_max_deg=max(values),
                north_median_deg=north_median,
                south_median_deg=south_median,
                north_south_difference_deg=ns_diff,
                n_valid_crossings=len(values),
                n_requested_crossings=n_requested,
                n_nonmonotonic_profiles=nonmonotonic,
            ))
        else:
            estimates.append(BoundaryEstimate(
                rigidity_gv=rigidity,
                observable=rc_attribute.replace("_gv", "").upper(),
                cutoff_aacgm_deg=None,
                cutoff_mean_deg=None, cutoff_std_deg=None,
                cutoff_min_deg=None, cutoff_max_deg=None,
                north_median_deg=None, south_median_deg=None,
                north_south_difference_deg=None,
                n_valid_crossings=0, n_requested_crossings=n_requested,
                n_nonmonotonic_profiles=nonmonotonic,
            ))
    return estimates


def _weighted_isotonic_non_decreasing(values: Sequence[float],
                                       weights: Sequence[float]) -> List[float]:
    """Return the weighted least-squares nondecreasing fit using PAVA.

    The pool-adjacent-violators algorithm is implemented locally instead of
    adding a SciPy/scikit-learn dependency to the validation runner.  Every
    block stores its inclusive input range, total statistical weight, and
    weighted sum.  Neighboring blocks are merged until their means are ordered.
    Expanding the final blocks gives the unique weighted L2 isotonic solution.
    """
    if len(values) != len(weights):
        raise ValueError("isotonic values and weights must have equal length")
    if not values:
        return []

    blocks: List[List[float]] = []
    for index, (value, weight) in enumerate(zip(values, weights)):
        if not math.isfinite(value) or not math.isfinite(weight) or weight <= 0.0:
            raise ValueError("isotonic inputs must be finite with positive weights")
        # [start_index, end_index, total_weight, weighted_sum]
        blocks.append([float(index), float(index), float(weight), float(weight * value)])
        while len(blocks) >= 2:
            left, right = blocks[-2], blocks[-1]
            left_mean = left[3] / left[2]
            right_mean = right[3] / right[2]
            if left_mean <= right_mean + 1.0e-14:
                break
            blocks[-2:] = [[left[0], right[1], left[2] + right[2], left[3] + right[3]]]

    fitted = [0.0] * len(values)
    for start_index, end_index, total_weight, weighted_sum in blocks:
        mean = weighted_sum / total_weight
        for index in range(int(start_index), int(end_index) + 1):
            fitted[index] = mean
    return fitted


def _profile_state_at_latitude(profile: Sequence[AccessRow],
                               latitude_deg: float) -> Optional[float]:
    """Interpolate one longitude's three-state access profile at |AACGM latitude|.

    Resolved equal-state neighbors define a constant segment.  A resolved 0/1
    pair defines a linear transition, which is the least-assumptive interpolation
    available between discrete latitude nodes.  UNRESOLVED at either side makes
    the requested value unavailable: C9 never interpolates through an unknown
    trajectory classification.
    """
    usable = [row for row in profile if row.aacgm_latitude_deg is not None]
    usable.sort(key=lambda row: abs(float(row.aacgm_latitude_deg)))
    if not usable:
        return None

    coordinates = [abs(float(row.aacgm_latitude_deg)) for row in usable]
    tolerance = 1.0e-10
    for coordinate, row in zip(coordinates, usable):
        if abs(coordinate - latitude_deg) <= tolerance:
            return None if row.access_state == 2 else float(row.access_state)

    if latitude_deg < coordinates[0] or latitude_deg > coordinates[-1]:
        return None
    for lower_index in range(len(usable) - 1):
        x1, x2 = coordinates[lower_index], coordinates[lower_index + 1]
        if not (x1 < latitude_deg < x2):
            continue
        lower, upper = usable[lower_index], usable[lower_index + 1]
        if lower.access_state == 2 or upper.access_state == 2:
            return None
        if abs(x2 - x1) < 1.0e-14:
            return 0.5 * (float(lower.access_state) + float(upper.access_state))
        fraction = (latitude_deg - x1) / (x2 - x1)
        return ((1.0 - fraction) * float(lower.access_state) +
                fraction * float(upper.access_state))
    return None


def _crossing_at_half(latitude: Sequence[float],
                      transmission: Sequence[float],
                      maximum_adjacent_step_deg: Optional[float] = None
                      ) -> Optional[float]:
    """Interpolate a T=0.5 crossing only across contiguous valid coverage.

    Latitude samples can be removed when too few longitudes are resolved.  The
    crossing search must not bridge such a removed interval; doing so would turn
    an explicit coverage failure into an apparently precise boundary.  The
    optional maximum step is therefore used to break the profile into contiguous
    pieces before testing exact plateaus or interpolated crossings.
    """
    if len(latitude) != len(transmission) or not latitude:
        return None

    def contiguous(first: int, second: int) -> bool:
        return (maximum_adjacent_step_deg is None or
                latitude[second] - latitude[first] <=
                1.5 * maximum_adjacent_step_deg + 1.0e-12)

    # A fitted isotonic profile may contain a finite T=0.5 plateau.  Group exact
    # half-transmission nodes by contiguous coverage and return the center of the
    # first physically bracketed group rather than averaging across a data gap.
    exact_indices = [index for index, value in enumerate(transmission)
                     if abs(value - 0.5) <= 1.0e-12]
    if exact_indices:
        groups: List[List[int]] = [[exact_indices[0]]]
        for index in exact_indices[1:]:
            if index == groups[-1][-1] + 1 and contiguous(groups[-1][-1], index):
                groups[-1].append(index)
            else:
                groups.append([index])
        for group in groups:
            left, right = group[0], group[-1]
            has_forbidden_side = (left == 0 or transmission[left - 1] <= 0.5)
            has_allowed_side = (right == len(transmission) - 1 or
                                transmission[right + 1] >= 0.5)
            if has_forbidden_side and has_allowed_side:
                return 0.5 * (latitude[left] + latitude[right])

    for index, (x1, x2, y1, y2) in enumerate(zip(
            latitude, latitude[1:], transmission, transmission[1:])):
        if not contiguous(index, index + 1):
            continue
        if y1 < 0.5 < y2:
            if abs(y2 - y1) < 1.0e-14:
                return 0.5 * (x1 + x2)
            return x1 + (0.5 - y1) * (x2 - x1) / (y2 - y1)
    return None


def _count_resolved_state_changes(profile: Sequence[AccessRow]) -> int:
    """Count physical 0/1 state changes in one longitude profile.

    Rows are ordered equator-to-pole in absolute AACGM latitude.  An unresolved
    trajectory terminates the current resolved segment, because C9 must not infer
    a transition through an unknown state.  A monotonic cutoff profile normally
    contains one resolved 0->1 change.  More than one change indicates an allowed
    island or forbidden gap and is reported as a nonmonotonic longitude profile.
    """
    ordered = sorted(
        (row for row in profile if row.aacgm_latitude_deg is not None),
        key=lambda row: abs(float(row.aacgm_latitude_deg)),
    )
    previous: Optional[int] = None
    changes = 0
    for row in ordered:
        if row.access_state == 2:
            previous = None
            continue
        state = int(row.access_state)
        if previous is not None and state != previous:
            changes += 1
        previous = state
    return changes


def _hemisphere_t50(rows: Sequence[AccessRow], rigidity_gv: float,
                    hemisphere: int, latitude_step_deg: float,
                    min_resolved_fraction: float,
                    minimum_edge_margin_deg: float) -> Tuple[Optional[float],
                                                              Optional[float],
                                                              Dict[str, object],
                                                              List[Dict[str, object]]]:
    """Calculate one hemisphere's longitude-averaged PAMELA_T50 boundary.

    Each longitude is first treated as an independent access profile.  Profiles
    are then sampled on a common absolute-AACGM grid.  The raw transmission is
    the mean of all resolved longitude contributions at that latitude.  Weighted
    isotonic regression removes finite-longitude reversals without choosing the
    first low-latitude allowed island.  Grid points with insufficient resolved
    longitude coverage are excluded before fitting.

    A reported boundary must be physically bracketed by transmission below 0.5
    at the equatorward end and above 0.5 at the poleward end.  It must also lie a
    configurable distance inside the retained AACGM coverage.  These checks stop
    a latitude-band edge or an extrapolated isotonic plateau from masquerading as
    a well-resolved cutoff.
    """
    rigidity_rows = [row for row in rows
                     if math.isclose(row.rigidity_gv, rigidity_gv,
                                     rel_tol=0.0, abs_tol=5.0e-9)
                     and row.aacgm_latitude_deg is not None
                     and (1 if float(row.aacgm_latitude_deg) >= 0.0 else -1) == hemisphere]
    longitudes = sorted({round(row.longitude_deg, 8) for row in rigidity_rows})
    profiles = {
        longitude: [row for row in rigidity_rows
                    if abs(row.longitude_deg - longitude) < 1.0e-6]
        for longitude in longitudes
    }
    profiles = {longitude: profile for longitude, profile in profiles.items() if profile}
    n_profiles = len(profiles)
    # Count profile topology before any longitude averaging.  This diagnostic is
    # intentionally separate from decreases in the raw mean transmission curve:
    # the former identifies physical access islands/gaps at individual longitudes,
    # whereas the latter measures finite-longitude noise in the aggregate profile.
    n_nonmonotonic_profiles = sum(
        1 for profile in profiles.values()
        if _count_resolved_state_changes(profile) > 1
    )
    if n_profiles == 0:
        return None, None, {
            "n_longitude_profiles": 0,
            "minimum_resolved_fraction": 0.0,
            "mean_resolved_fraction": 0.0,
            "unresolved_access_fraction": 1.0,
            "isotonic_adjustment_rms": None,
            "n_raw_decreases": 0,
            "n_nonmonotonic_longitude_profiles": 0,
            "t50_bracketed": False,
            "boundary_edge_margin_deg": None,
            "equatorward_transmission": None,
            "poleward_transmission": None,
        }, []

    all_coordinates = [abs(float(row.aacgm_latitude_deg))
                       for profile in profiles.values() for row in profile
                       if row.aacgm_latitude_deg is not None]
    latitude_min = max(20.0, math.floor(min(all_coordinates) / latitude_step_deg)
                       * latitude_step_deg)
    latitude_max = min(85.0, math.ceil(max(all_coordinates) / latitude_step_deg)
                       * latitude_step_deg)
    n_grid = int(round((latitude_max - latitude_min) / latitude_step_deg)) + 1

    profile_rows: List[Dict[str, object]] = []
    valid_latitudes: List[float] = []
    raw_transmission: List[float] = []
    weights: List[float] = []
    resolved_fractions: List[float] = []
    unresolved_count = sum(1 for row in rigidity_rows if row.access_state == 2)
    total_count = len(rigidity_rows)

    for index in range(max(0, n_grid)):
        latitude = latitude_min + index * latitude_step_deg
        samples = [_profile_state_at_latitude(profile, latitude)
                   for profile in profiles.values()]
        resolved = [value for value in samples if value is not None]
        resolved_fraction = len(resolved) / float(n_profiles)
        if resolved_fraction + 1.0e-14 < min_resolved_fraction or not resolved:
            continue
        valid_latitudes.append(latitude)
        raw_transmission.append(statistics.fmean(resolved))
        weights.append(float(len(resolved)))
        resolved_fractions.append(resolved_fraction)

    if len(valid_latitudes) < 2:
        diagnostics = {
            "n_longitude_profiles": n_profiles,
            "minimum_resolved_fraction": min(resolved_fractions) if resolved_fractions else 0.0,
            "mean_resolved_fraction": statistics.fmean(resolved_fractions) if resolved_fractions else 0.0,
            "unresolved_access_fraction": unresolved_count / float(total_count) if total_count else 1.0,
            "isotonic_adjustment_rms": None,
            "n_raw_decreases": 0,
            "n_nonmonotonic_longitude_profiles": n_nonmonotonic_profiles,
            "t50_bracketed": False,
            "boundary_edge_margin_deg": None,
            "equatorward_transmission": None,
            "poleward_transmission": None,
        }
        return None, None, diagnostics, profile_rows

    isotonic = _weighted_isotonic_non_decreasing(raw_transmission, weights)

    # A crossing is valid only when the fitted transmission is demonstrably on
    # opposite sides of 0.5 at the retained profile ends.  _crossing_at_half()
    # intentionally handles internal gaps, but it cannot decide whether a flat
    # edge at 0.5 is a physical bracket or merely truncated coverage.
    bracketed = isotonic[0] < 0.5 - 1.0e-12 and isotonic[-1] > 0.5 + 1.0e-12
    raw_bracketed = (raw_transmission[0] < 0.5 - 1.0e-12 and
                     raw_transmission[-1] > 0.5 + 1.0e-12)
    raw_boundary = (_crossing_at_half(
        valid_latitudes, raw_transmission, latitude_step_deg)
        if raw_bracketed else None)
    fitted_boundary = (_crossing_at_half(
        valid_latitudes, isotonic, latitude_step_deg)
        if bracketed else None)
    boundary_edge_margin = None
    if fitted_boundary is not None:
        boundary_edge_margin = min(
            fitted_boundary - valid_latitudes[0],
            valid_latitudes[-1] - fitted_boundary,
        )
        if boundary_edge_margin + 1.0e-12 < minimum_edge_margin_deg:
            # Keep the margin diagnostic but invalidate the scientific boundary.
            # A caller can widen the common geodetic band and rerun rather than
            # accepting a cutoff controlled by the selected-domain edge.
            fitted_boundary = None
    adjustment_rms = math.sqrt(statistics.fmean(
        (raw - fitted) ** 2 for raw, fitted in zip(raw_transmission, isotonic)
    ))
    n_raw_decreases = sum(
        1 for first, second in zip(raw_transmission, raw_transmission[1:])
        if second < first - 1.0e-8
    )

    for latitude, raw, fitted, weight, fraction in zip(
            valid_latitudes, raw_transmission, isotonic, weights, resolved_fractions):
        profile_rows.append({
            "rigidity_gv": rigidity_gv,
            "hemisphere": "N" if hemisphere > 0 else "S",
            "abs_aacgm_latitude_deg": latitude,
            "raw_transmission": raw,
            "isotonic_transmission": fitted,
            "n_resolved_longitudes": int(weight),
            "n_total_longitudes": n_profiles,
            "resolved_longitude_fraction": fraction,
        })

    diagnostics = {
        "n_longitude_profiles": n_profiles,
        "minimum_resolved_fraction": min(resolved_fractions),
        "mean_resolved_fraction": statistics.fmean(resolved_fractions),
        "unresolved_access_fraction": unresolved_count / float(total_count) if total_count else 1.0,
        "isotonic_adjustment_rms": adjustment_rms,
        "n_raw_decreases": n_raw_decreases,
        "n_nonmonotonic_longitude_profiles": n_nonmonotonic_profiles,
        "t50_bracketed": bracketed,
        "boundary_edge_margin_deg": boundary_edge_margin,
        "equatorward_transmission": isotonic[0],
        "poleward_transmission": isotonic[-1],
    }
    return fitted_boundary, raw_boundary, diagnostics, profile_rows


def estimate_pamela_t50_boundaries(rows: Sequence[AccessRow],
                                   rigidities: Sequence[float],
                                   latitude_step_deg: float,
                                   min_resolved_fraction: float,
                                   minimum_edge_margin_deg: float
                                   ) -> Tuple[List[BoundaryEstimate],
                                              List[Dict[str, object]]]:
    """Estimate PAMELA-equivalent T=0.5 boundaries from fixed-rigidity access.

    North and south are fitted independently to preserve hemispheric diagnostics.
    A primary value is accepted only when both hemispheres bracket T=0.5; this is
    intentionally stricter than silently extrapolating or accepting one-sided
    coverage.  The final boundary is the median of the two hemisphere estimates
    (identical to their mean for two values).
    """
    estimates: List[BoundaryEstimate] = []
    all_profile_rows: List[Dict[str, object]] = []
    for rigidity in rigidities:
        north, north_raw, north_diag, north_rows = _hemisphere_t50(
            rows, rigidity, +1, latitude_step_deg, min_resolved_fraction,
            minimum_edge_margin_deg)
        south, south_raw, south_diag, south_rows = _hemisphere_t50(
            rows, rigidity, -1, latitude_step_deg, min_resolved_fraction,
            minimum_edge_margin_deg)
        all_profile_rows.extend(north_rows)
        all_profile_rows.extend(south_rows)

        valid = [value for value in (north, south) if value is not None]
        raw_valid = [value for value in (north_raw, south_raw) if value is not None]
        both_valid = len(valid) == 2
        nonmonotonic = (
            int(north_diag["n_nonmonotonic_longitude_profiles"]) +
            int(south_diag["n_nonmonotonic_longitude_profiles"])
        )
        minimum_resolved = min(float(north_diag["minimum_resolved_fraction"]),
                               float(south_diag["minimum_resolved_fraction"]))
        mean_resolved = statistics.fmean([
            float(north_diag["mean_resolved_fraction"]),
            float(south_diag["mean_resolved_fraction"]),
        ])
        unresolved_fraction = statistics.fmean([
            float(north_diag["unresolved_access_fraction"]),
            float(south_diag["unresolved_access_fraction"]),
        ])
        adjustments = [float(value) for value in (
            north_diag["isotonic_adjustment_rms"],
            south_diag["isotonic_adjustment_rms"],
        ) if value is not None]
        n_bracketed_hemispheres = sum(bool(value) for value in (
            north_diag.get("t50_bracketed", False),
            south_diag.get("t50_bracketed", False),
        ))
        edge_margins = [float(value) for value in (
            north_diag.get("boundary_edge_margin_deg"),
            south_diag.get("boundary_edge_margin_deg"),
        ) if value is not None]
        minimum_edge_margin = min(edge_margins) if edge_margins else None

        if both_valid:
            primary = statistics.median(valid)
            estimates.append(BoundaryEstimate(
                rigidity_gv=rigidity,
                observable="PAMELA_T50",
                cutoff_aacgm_deg=primary,
                cutoff_mean_deg=statistics.fmean(valid),
                cutoff_std_deg=statistics.stdev(valid),
                cutoff_min_deg=min(valid),
                cutoff_max_deg=max(valid),
                north_median_deg=north,
                south_median_deg=south,
                north_south_difference_deg=float(north) - float(south),
                n_valid_crossings=2,
                n_requested_crossings=2,
                n_nonmonotonic_profiles=nonmonotonic,
                raw_cutoff_aacgm_deg=(statistics.median(raw_valid)
                                      if len(raw_valid) == 2 else None),
                isotonic_adjustment_rms=(statistics.fmean(adjustments)
                                         if adjustments else None),
                minimum_resolved_longitude_fraction=minimum_resolved,
                mean_resolved_longitude_fraction=mean_resolved,
                unresolved_access_fraction=unresolved_fraction,
                n_bracketed_hemispheres=n_bracketed_hemispheres,
                minimum_boundary_edge_margin_deg=minimum_edge_margin,
            ))
        else:
            estimates.append(BoundaryEstimate(
                rigidity_gv=rigidity,
                observable="PAMELA_T50",
                cutoff_aacgm_deg=None,
                cutoff_mean_deg=None,
                cutoff_std_deg=None,
                cutoff_min_deg=None,
                cutoff_max_deg=None,
                north_median_deg=north,
                south_median_deg=south,
                north_south_difference_deg=None,
                n_valid_crossings=len(valid),
                n_requested_crossings=2,
                n_nonmonotonic_profiles=nonmonotonic,
                raw_cutoff_aacgm_deg=(statistics.median(raw_valid)
                                      if raw_valid else None),
                isotonic_adjustment_rms=(statistics.fmean(adjustments)
                                         if adjustments else None),
                minimum_resolved_longitude_fraction=minimum_resolved,
                mean_resolved_longitude_fraction=mean_resolved,
                unresolved_access_fraction=unresolved_fraction,
                n_bracketed_hemispheres=n_bracketed_hemispheres,
                minimum_boundary_edge_margin_deg=minimum_edge_margin,
            ))
    return estimates, all_profile_rows

def pearson(x: Sequence[float], y: Sequence[float]) -> Optional[float]:
    if len(x) < 2:
        return None
    mx, my = statistics.fmean(x), statistics.fmean(y)
    dx = [value - mx for value in x]
    dy = [value - my for value in y]
    denominator = math.sqrt(sum(value * value for value in dx) *
                            sum(value * value for value in dy))
    return None if denominator == 0.0 else sum(a * b for a, b in zip(dx, dy)) / denominator


def write_dict_rows(path: Path, rows: Sequence[Mapping[str, object]]) -> None:
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
        writer.writeheader(); writer.writerows(rows)


def make_plot(comparison: Sequence[Mapping[str, object]], output_base: Path, solver: str,
              observable: str, formats: Sequence[str] = ("png", "eps")) -> List[Path]:
    """Plot PAMELA and AMPS cutoff latitudes plus their residuals.

    Plotting convention is intentionally rigidity-centric: PAMELA and AMPS use
    the same color for a given rigidity.  Data source is distinguished only by
    line style and marker (PAMELA: dashed/circle; AMPS: solid/x).  This avoids
    the misleading impression that a PAMELA curve and its corresponding AMPS
    curve are unrelated series.

    PAMELA statistical uncertainties are shown as error bars when they are
    present in the comparison table.  The legend is split into a compact
    rigidity-color legend and a two-entry source-style legend so that seven
    rigidity bins do not produce fourteen visually redundant entries.

    ``output_base`` carries no suffix; one sibling file is written per entry in
    ``formats`` (``output_base.with_suffix(".png")``, ``".eps"``, ...) from the
    same figure, so the raster and vector copies never drift apart. The return
    value lists only the files actually written, which is ``[]`` when
    Matplotlib is unavailable, there is no valid data to plot, or every
    requested format failed to save -- callers must not treat an empty result
    as an error.
    """
    try:
        import matplotlib.pyplot as plt  # type: ignore
        from matplotlib.lines import Line2D  # type: ignore
    except ImportError:
        return []

    valid = [row for row in comparison if row.get("amps_cutoff_aacgm_deg") not in (None, "")]
    if not valid or not formats:
        return []

    bins = sorted({float(row["rigidity_center_gv"]) for row in valid})
    figure, axes = plt.subplots(2, 1, figsize=(11, 8), sharex=True)

    # Obtain one color per rigidity from Matplotlib's active color cycle.
    # The same color is then reused for PAMELA, AMPS, and the residual curve.
    color_cycle = plt.rcParams["axes.prop_cycle"].by_key().get("color", [])
    if not color_cycle:
        color_cycle = ["C%d" % index for index in range(max(1, len(bins)))]

    rigidity_handles = []
    for index, rigidity in enumerate(bins):
        color = color_cycle[index % len(color_cycle)]
        subset = sorted(
            [row for row in valid if math.isclose(float(row["rigidity_center_gv"]), rigidity)],
            key=lambda row: str(row["interval_midpoint_utc"]),
        )
        times = [parse_utc(str(row["interval_midpoint_utc"])) for row in subset]
        reference = [float(row["pamela_cutoff_aacgm_deg"]) for row in subset]
        model = [float(row["amps_cutoff_aacgm_deg"]) for row in subset]
        residual = [m - r for m, r in zip(model, reference)]
        sigma_minus = [float(row.get("pamela_sigma_minus_deg") or 0.0) for row in subset]
        sigma_plus = [float(row.get("pamela_sigma_plus_deg") or 0.0) for row in subset]
        label = "%.3f GV" % rigidity

        axes[0].errorbar(
            times, reference, yerr=[sigma_minus, sigma_plus], color=color,
            marker="o", linestyle="--", linewidth=1.4, markersize=4.5,
            capsize=2.0, elinewidth=0.8, alpha=0.95,
        )
        axes[0].plot(
            times, model, color=color, marker="x", linestyle="-",
            linewidth=1.5, markersize=5.0,
        )
        axes[1].plot(
            times, residual, color=color, marker="o", linestyle="-",
            linewidth=1.3, markersize=4.5, label=label,
        )
        rigidity_handles.append(Line2D([0], [0], color=color, linewidth=2.0, label=label))

    axes[0].set_ylabel("cutoff |AACGM latitude| [deg]")
    axes[0].set_title(
        "C9 %s (%s): PAMELA Table S1 versus AMPS global-shell estimate" %
        (solver, observable)
    )
    axes[0].grid(True, alpha=0.3)

    rigidity_legend = axes[0].legend(
        handles=rigidity_handles, title="Rigidity", ncol=4, fontsize=8,
        title_fontsize=8, loc="lower right",
    )
    axes[0].add_artist(rigidity_legend)
    source_handles = [
        Line2D([0], [0], color="black", marker="o", linestyle="--",
               linewidth=1.4, markersize=4.5, label="PAMELA (Table S1)"),
        Line2D([0], [0], color="black", marker="x", linestyle="-",
               linewidth=1.5, markersize=5.0, label="AMPS"),
    ]
    axes[0].legend(handles=source_handles, loc="upper left", fontsize=8)

    axes[1].axhline(0.0, color="black", linestyle="--", linewidth=1)
    axes[1].set_ylabel("AMPS - PAMELA [deg]")
    axes[1].set_xlabel("UTC")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(title="Rigidity", ncol=4, fontsize=8, title_fontsize=8)
    figure.autofmt_xdate()
    figure.tight_layout()

    # EPS/PS have no alpha channel, so Matplotlib's translucent error bars
    # (alpha=0.95 above) fall back to opaque on that one format. Matplotlib
    # reports this through its logger rather than warnings.warn, so it is
    # muted here rather than with a warnings filter; it is expected on every
    # EPS write and not a sign anything went wrong.
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
                print("C9 plot: failed to write %s: %s" % (destination, exc), file=sys.stderr)
                continue
            written.append(destination)
    finally:
        ps_logger.setLevel(previous_level)

    plt.close(figure)
    return written


def compare(reference_rows: Sequence[ReferenceRow], interval_models: Mapping[datetime, Mapping[float, Mapping[str, object]]],
            driver_info: DriverInfo, args: argparse.Namespace) -> Tuple[List[Dict[str, object]], Metrics]:
    detailed: List[Dict[str, object]] = []
    ref_values: List[float] = []
    model_values: List[float] = []
    residuals: List[float] = []
    normalized: List[float] = []
    within1 = within2 = 0

    for ref in sorted(reference_rows, key=lambda row: (row.midpoint, row.rigidity_center_gv)):
        if ref.missing or ref.pamela_cutoff_aacgm_deg is None:
            continue
        model = interval_models.get(ref.midpoint, {}).get(ref.rigidity_center_gv, {})
        value = model.get("cutoff_aacgm_deg")
        value_float = None if value is None else float(value)
        residual = None if value_float is None else value_float - ref.pamela_cutoff_aacgm_deg
        sigma = None
        if residual is not None:
            sigma = ref.sigma_plus_deg if residual >= 0.0 else ref.sigma_minus_deg
            sigma = sigma if sigma and sigma > 0.0 else None
            ref_values.append(ref.pamela_cutoff_aacgm_deg)
            model_values.append(value_float)
            residuals.append(residual)
            if sigma is not None:
                normalized.append(residual / sigma)
                within1 += int(abs(residual) <= sigma)
                within2 += int(abs(residual) <= 2.0 * sigma)
        row: Dict[str, object] = {
            "interval_midpoint_utc": format_utc(ref.midpoint),
            "interval_start_utc": format_utc(ref.interval_start),
            "interval_end_utc": format_utc(ref.interval_end),
            "rigidity_min_gv": ref.rigidity_min_gv,
            "rigidity_max_gv": ref.rigidity_max_gv,
            "rigidity_center_gv": ref.rigidity_center_gv,
            "pamela_cutoff_aacgm_deg": ref.pamela_cutoff_aacgm_deg,
            "pamela_sigma_plus_deg": ref.sigma_plus_deg,
            "pamela_sigma_minus_deg": ref.sigma_minus_deg,
            "amps_cutoff_aacgm_deg": value_float,
            "amps_minus_pamela_deg": residual,
            "normalized_residual": (None if residual is None or sigma is None else residual / sigma),
        }
        row.update({"amps_" + key: value for key, value in model.items()
                    if key != "cutoff_aacgm_deg"})
        detailed.append(row)

    n_reference = len(detailed)
    n_valid = len(residuals)
    valid_fraction = n_valid / n_reference if n_reference else 0.0
    if residuals:
        bias = statistics.fmean(residuals)
        mae = statistics.fmean(abs(value) for value in residuals)
        rmse = math.sqrt(statistics.fmean(value * value for value in residuals))
        max_abs = max(abs(value) for value in residuals)
    else:
        bias = mae = rmse = max_abs = float("inf")
    correlation = pearson(ref_values, model_values)
    weighted_rms = (math.sqrt(statistics.fmean(value * value for value in normalized))
                    if normalized else float("inf"))
    f1 = within1 / len(normalized) if normalized else 0.0
    f2 = within2 / len(normalized) if normalized else 0.0

    low_rigidity = min(row.rigidity_center_gv for row in reference_rows)
    low_ref = [row for row in reference_rows
               if math.isclose(row.rigidity_center_gv, low_rigidity)
               and not row.missing and row.pamela_cutoff_aacgm_deg is not None]
    low_ref.sort(key=lambda row: row.midpoint)
    observed_suppression = modeled_suppression = None
    observed_min_time = modeled_min_time = None
    time_error = None
    if low_ref:
        observed_min = min(low_ref, key=lambda row: float(row.pamela_cutoff_aacgm_deg))
        observed_min_time = format_utc(observed_min.midpoint)
        observed_suppression = (float(low_ref[0].pamela_cutoff_aacgm_deg) -
                                float(observed_min.pamela_cutoff_aacgm_deg))
        low_model = []
        for row in low_ref:
            value = interval_models.get(row.midpoint, {}).get(low_rigidity, {}).get("cutoff_aacgm_deg")
            if value is not None:
                low_model.append((row.midpoint, float(value)))
        # Storm suppression is defined relative to the first selected
        # pre-storm reference epoch, not the first epoch for which T50 happens
        # to be available. Otherwise missing early boundaries silently move the
        # baseline into the storm main phase and produce a misleading small
        # suppression.
        baseline_value = interval_models.get(
            low_ref[0].midpoint, {}
        ).get(low_rigidity, {}).get("cutoff_aacgm_deg")
        if low_model and baseline_value is not None:
            modeled_min = min(low_model, key=lambda item: item[1])
            modeled_min_time = format_utc(modeled_min[0])
            modeled_suppression = float(baseline_value) - modeled_min[1]
            time_error = abs(
                (modeled_min[0] - observed_min.midpoint).total_seconds()
            ) / 60.0

    numerical_pass = (
        valid_fraction >= args.min_valid_fraction
        and rmse <= args.max_rmse_deg
        and abs(bias) <= args.max_abs_bias_deg
        and (correlation is not None and correlation >= args.min_correlation)
        and modeled_suppression is not None
        and args.min_suppression_deg <= modeled_suppression <= args.max_suppression_deg
        and time_error is not None and time_error <= args.max_minimum_time_error_minutes
    )
    scientific_eligible = driver_info.verified_driver
    passed = numerical_pass and (scientific_eligible or args.allow_unverified_driver)
    metrics = Metrics(
        n_reference=n_reference,
        n_valid_model=n_valid,
        valid_fraction=valid_fraction,
        mean_bias_deg=bias,
        mean_abs_error_deg=mae,
        rmse_deg=rmse,
        max_abs_error_deg=max_abs,
        correlation=correlation,
        weighted_rms_normalized_residual=weighted_rms,
        fraction_within_1sigma=f1,
        fraction_within_2sigma=f2,
        observed_low_rigidity_suppression_deg=observed_suppression,
        modeled_low_rigidity_suppression_deg=modeled_suppression,
        observed_minimum_time_utc=observed_min_time,
        modeled_minimum_time_utc=modeled_min_time,
        minimum_time_error_minutes=time_error,
        driver_verified=driver_info.verified_driver,
        scientific_validation_eligible=scientific_eligible,
        passed_numerical_comparison=numerical_pass,
        passed=passed,
    )
    return detailed, metrics


def compare_solver_branches(
    models: Mapping[str, Mapping[datetime, Mapping[float, Mapping[str, object]]]]
) -> Tuple[List[Dict[str, object]], Dict[str, object]]:
    """Compare GRIDDED and GRIDLESS modeled cutoff latitudes.

    This is a diagnostic, not a substitute for the PAMELA comparison.  Both
    branches must independently satisfy the observational acceptance criteria.
    The cross-solver table helps isolate mesh/interpolation effects when one
    branch diverges from the other.
    """
    if "GRIDLESS" not in models or "GRIDDED" not in models:
        return [], {}
    rows: List[Dict[str, object]] = []
    differences: List[float] = []
    gridless = models["GRIDLESS"]
    gridded = models["GRIDDED"]
    for midpoint in sorted(set(gridless).intersection(gridded)):
        for rigidity in sorted(set(gridless[midpoint]).intersection(gridded[midpoint])):
            a = gridless[midpoint][rigidity].get("cutoff_aacgm_deg")
            b = gridded[midpoint][rigidity].get("cutoff_aacgm_deg")
            difference = None if a is None or b is None else float(b) - float(a)
            if difference is not None:
                differences.append(difference)
            rows.append({
                "interval_midpoint_utc": format_utc(midpoint),
                "rigidity_center_gv": rigidity,
                "gridless_cutoff_aacgm_deg": a,
                "gridded_cutoff_aacgm_deg": b,
                "gridded_minus_gridless_deg": difference,
            })
    diagnostics: Dict[str, object] = {
        "n_common": len(differences),
        "mean_gridded_minus_gridless_deg": (
            statistics.fmean(differences) if differences else None
        ),
        "rmse_gridded_minus_gridless_deg": (
            math.sqrt(statistics.fmean(value * value for value in differences))
            if differences else None
        ),
        "maximum_absolute_difference_deg": (
            max(abs(value) for value in differences) if differences else None
        ),
        "acceptance_role": "diagnostic_only; each branch is judged against PAMELA",
    }
    return rows, diagnostics


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  Validate the checked-in PAMELA transcription and bundled driver:
    python3 srcEarth/test/C9/run_C9.py --validate-references --validate-driver

  Preview both routine solver branches:
    python3 srcEarth/test/C9/run_C9.py --solver BOTH --dry-run -np 4 -nt 16

  Run only the GRIDLESS branch:
    python3 srcEarth/test/C9/run_C9.py --solver GRIDLESS --profile ROUTINE -np 16

  Run the fast GRIDDED direct-access product:
    python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable PAMELA_T50 --profile ROUTINE -np 4 -nt 16

  Run the legacy complete PENUMBRA_SCAN product:
    python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable PAMELA_T50 --profile ROUTINE -np 4 -nt 16

  Verify FULL_SCAN against an existing, identically configured DIRECT_ACCESS run:
    python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable PAMELA_T50 --profile ROUTINE --output-root test_output/C9_full_verified --access-consistency-root test_output/C9_direct -np 4 -nt 16

  Run both full-scan branches for every interval with five snapshots:
    python3 srcEarth/test/C9/run_C9.py --solver BOTH --profile FULL --interval-samples 5 -np 8 -nt 16
""",
    )
    script_dir = Path(__file__).resolve().parent
    parser.add_argument("--profile", type=str.upper, choices=("SMOKE", "ROUTINE", "FULL"), default="ROUTINE")
    parser.add_argument("--timestamps", default="", help="Comma-separated Table-S1 midpoints; overrides profile")
    parser.add_argument("--interval-samples", type=int, default=1,
                        help="Equally spaced TS05 snapshots per 94-minute interval")
    parser.add_argument("--solver", type=str.upper, choices=SOLVERS, default="BOTH",
                        help="AMPS field-evaluation branch to run")
    parser.add_argument(
        "--cutoff-evaluation", type=str.upper, choices=CUTOFF_EVALUATIONS,
        default="FULL_SCAN",
        help=("FULL_SCAN uses complete PENUMBRA_SCAN and also records the seven "
              "PAMELA access states; DIRECT_ACCESS traces only those states in a "
              "latitude band"),
    )
    parser.add_argument(
        "--comparison-observable", type=str.upper,
        choices=COMPARISON_OBSERVABLES, default="PAMELA_T50",
        help=("Primary modeled quantity. PAMELA_T50 uses the common longitude-"
              "averaged half-transmission boundary for FULL_SCAN and DIRECT_ACCESS; "
              "RC_* are FULL_SCAN diagnostics; ALL uses PAMELA_T50 for PASS/FAIL "
              "and writes every diagnostic."),
    )
    parser.add_argument("--reference", default=str(script_dir / "reference_C9_pamela_table_s1.csv"))
    bundled_driver = script_dir / "data" / "ts05_driving.txt"
    parser.add_argument(
        "--driver",
        default=os.environ.get("C9_TS05_DRIVER", str(bundled_driver)),
        help=("Five-minute TS05 driver. Defaults to the checked-in C9 event "
              "driver; C9_TS05_DRIVER overrides it."),
    )
    parser.add_argument("--allow-unverified-driver", action="store_true",
                        help="Use an unverified driver, marking result scientifically ineligible")
    parser.add_argument("--validate-references", action="store_true")
    parser.add_argument("--validate-driver", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-run", action="store_true",
                        help="Analyze already existing per-snapshot outputs")
    parser.add_argument("--keep", action="store_true", help="Preserve existing selected solver output trees")
    parser.add_argument("--output-root", default="test_output/C9")
    parser.add_argument(
        "--plot-formats", default="png,eps", metavar="FMT[,FMT...]",
        help=("Comma-separated Matplotlib output formats for each branch's "
              "C9_comparison plot, e.g. 'png,eps', 'png', or 'png,eps,pdf,svg'; "
              "ignored entirely if Matplotlib is not installed"),
    )
    parser.add_argument("--amps", default="./amps")
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("-np", type=int, default=4)
    parser.add_argument("-nt", type=int, default=16,
                        help="Mode3D trajectory threads per MPI process")
    parser.add_argument(
        "--mode3d-parallel-field-init", action="store_true",
        help=("Parallelize Mode3D background magnetic-field initialization with "
              "POSIX threads, using the -nt thread count"),
    )
    parser.add_argument("--scheduler", type=str.upper, choices=("STATIC", "BLOCK_CYCLIC", "DYNAMIC"), default="DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0,
                        help="Scheduler chunk; 0 selects 1 for GRIDLESS and -nt for GRIDDED")
    parser.add_argument("--mover", default="", help="Optional AMPS cutoff mover override")
    parser.add_argument("--mode3d-mesh-res-earth-re", type=float, default=0.02)
    parser.add_argument("--mode3d-mesh-res-boundary-re", type=float, default=2.0)
    parser.add_argument("--mode3d-mesh-coarsening", type=str.upper,
                        choices=("LINEAR", "LOG"), default="LINEAR")
    parser.add_argument("--mode3d-mesh-exponent", type=float, default=1.0)
    parser.add_argument("--cutoff-scan-n", type=int, default=160)
    parser.add_argument("--cutoff-trace-policy", type=str.upper, choices=("ACCURATE", "LEGACY"), default="ACCURATE")
    parser.add_argument("--rigidity-min-gv", type=float, default=0.30)
    parser.add_argument("--rigidity-max-gv", type=float, default=1.35)
    parser.add_argument("--altitude-km", type=float, default=475.0)
    parser.add_argument("--shell-lon-res-deg", type=float, default=30.0)
    parser.add_argument("--shell-lat-res-deg", type=float, default=2.0)
    parser.add_argument(
        "--access-abs-lat-min-deg", type=float, default=35.0,
        help=("Common PAMELA_T50 absolute-geodetic-latitude lower bound; "
              "DIRECT_ACCESS also uses it to reduce the C++ trajectory work list"),
    )
    parser.add_argument(
        "--access-abs-lat-max-deg", type=float, default=75.0,
        help=("Common PAMELA_T50 absolute-geodetic-latitude upper bound; "
              "DIRECT_ACCESS also uses it to reduce the C++ trajectory work list"),
    )
    parser.add_argument("--t50-grid-step-deg", type=float, default=0.25,
                        help="Absolute-AACGM grid step used for PAMELA_T50 fitting")
    parser.add_argument("--t50-min-resolved-longitude-fraction", type=float, default=0.80,
                        help="Minimum resolved longitude coverage retained at a T50 grid point")
    parser.add_argument(
        "--t50-min-edge-margin-deg", type=float, default=1.0,
        help=("Minimum AACGM distance between a T50 boundary and either retained "
              "profile edge; an edge-controlled crossing is marked invalid"),
    )
    parser.add_argument(
        "--access-consistency-root", default="",
        help=("Optional output root from the other cutoff-evaluation mode. "
              "Matching GRIDDED snapshots are compared state-by-state and the "
              "result becomes a numerical acceptance gate."),
    )
    parser.add_argument(
        "--min-access-state-agreement", type=float, default=0.999,
        help="Minimum resolved FULL_SCAN/DIRECT_ACCESS state agreement fraction",
    )
    parser.add_argument(
        "--max-access-unresolved-fraction", type=float, default=0.01,
        help="Maximum fraction unresolved in either compared access product",
    )
    parser.add_argument("--max-trace-time", type=float, default=20.0)
    parser.add_argument("--min-valid-fraction", type=float, default=0.85)
    parser.add_argument("--max-rmse-deg", type=float, default=3.0)
    parser.add_argument("--max-abs-bias-deg", type=float, default=2.0)
    parser.add_argument("--min-correlation", type=float, default=0.80)
    parser.add_argument("--min-suppression-deg", type=float, default=4.0)
    parser.add_argument("--max-suppression-deg", type=float, default=9.0)
    parser.add_argument("--max-minimum-time-error-minutes", type=float, default=100.0)
    args = parser.parse_args(argv)
    if args.interval_samples < 1:
        parser.error("--interval-samples must be >= 1")
    if args.cutoff_evaluation == "FULL_SCAN" and args.cutoff_scan_n < 2:
        parser.error("--cutoff-scan-n must be >= 2 for FULL_SCAN")
    if args.cutoff_evaluation == "DIRECT_ACCESS" and args.solver != "GRIDDED":
        parser.error("DIRECT_ACCESS is implemented only for --solver GRIDDED")
    if (args.cutoff_evaluation == "DIRECT_ACCESS" and
            args.comparison_observable != "PAMELA_T50"):
        parser.error(
            "DIRECT_ACCESS supports only PAMELA_T50; RC_* and ALL require FULL_SCAN")
    if not (0.0 <= args.access_abs_lat_min_deg < args.access_abs_lat_max_deg <= 90.0):
        parser.error("require 0 <= access-abs-lat-min < access-abs-lat-max <= 90")
    if not (0.0 < args.rigidity_min_gv < args.rigidity_max_gv):
        parser.error("require 0 < rigidity-min < rigidity-max")
    if args.shell_lon_res_deg <= 0.0 or args.shell_lat_res_deg <= 0.0:
        parser.error("shell resolutions must be positive")
    if args.t50_grid_step_deg <= 0.0:
        parser.error("--t50-grid-step-deg must be positive")
    if args.t50_min_edge_margin_deg < 0.0:
        parser.error("--t50-min-edge-margin-deg must be non-negative")
    if not (0.0 < args.t50_min_resolved_longitude_fraction <= 1.0):
        parser.error("--t50-min-resolved-longitude-fraction must be in (0,1]")
    if not (0.0 < args.min_access_state_agreement <= 1.0):
        parser.error("--min-access-state-agreement must be in (0,1]")
    if not (0.0 <= args.max_access_unresolved_fraction <= 1.0):
        parser.error("--max-access-unresolved-fraction must be in [0,1]")
    if args.access_consistency_root and args.solver == "GRIDLESS":
        parser.error("--access-consistency-root requires --solver GRIDDED or BOTH")
    if args.dynamic_chunk < 0 or args.np < 1 or args.nt < 1:
        parser.error("-np and -nt must be >= 1; --dynamic-chunk must be >= 0")
    if args.mode3d_mesh_res_earth_re <= 0.0 or args.mode3d_mesh_res_boundary_re <= 0.0:
        parser.error("Mode3D mesh resolutions must be positive")
    if args.mode3d_mesh_exponent <= 0.0:
        parser.error("--mode3d-mesh-exponent must be positive")
    supported_plot_formats = {"png", "eps", "pdf", "svg"}
    requested_plot_formats = tuple(dict.fromkeys(
        fmt.strip().lower().lstrip(".") for fmt in args.plot_formats.split(",") if fmt.strip()
    ))
    unsupported_plot_formats = sorted(set(requested_plot_formats) - supported_plot_formats)
    if unsupported_plot_formats:
        parser.error(
            "--plot-formats has unsupported entries: %s (expected any of %s)"
            % (", ".join(unsupported_plot_formats), ", ".join(sorted(supported_plot_formats)))
        )
    args.plot_formats = requested_plot_formats
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    script_dir = Path(__file__).resolve().parent
    launch_dir = Path.cwd()
    reference_path = Path(args.reference).expanduser().resolve()
    try:
        reference = load_reference(reference_path)
    except Exception as exc:
        print("C9 reference validation failed: %s" % exc, file=sys.stderr)
        return 2

    print("C9 reference: %d rows, 37 intervals, 7 rigidity bins, 1 missing datum" % len(reference))
    if args.validate_references and not args.validate_driver:
        return 0

    try:
        midpoints = selected_midpoints(reference, args)
    except Exception as exc:
        print("C9 selection failed: %s" % exc, file=sys.stderr)
        return 2
    reference_by_midpoint = {
        midpoint: sorted([row for row in reference if row.midpoint == midpoint],
                         key=lambda row: row.rigidity_center_gv)
        for midpoint in midpoints
    }
    # Every Table-S1 interval uses the same seven bins. Derive the direct list from the
    # checked reference rather than duplicating rounded constants in code or templates.
    args.direct_rigidities_gv = sorted({
        row.rigidity_center_gv for midpoint in midpoints
        for row in reference_by_midpoint[midpoint]
    })
    all_sample_times = [
        sample for midpoint in midpoints
        for sample in interval_sample_times(reference_by_midpoint[midpoint], args.interval_samples)
    ]

    if not args.driver:
        print(
            "C9 requires a five-minute TS05 driver. The checked-in default "
            "srcEarth/test/C9/data/ts05_driving.txt is missing; restore it or "
            "set --driver/C9_TS05_DRIVER.",
            file=sys.stderr,
        )
        return 2
    driver_path = Path(args.driver).expanduser().resolve()
    try:
        driver_info = validate_driver(driver_path, all_sample_times)
    except Exception as exc:
        print("C9 driver validation failed: %s" % exc, file=sys.stderr)
        return 2
    print("C9 driver: %d records, %s .. %s, source=%s" %
          (driver_info.n_records, driver_info.first_epoch, driver_info.last_epoch,
           driver_info.source_kind))
    if not driver_info.verified_driver and not args.allow_unverified_driver:
        print(
            "C9 refuses a driver that is neither the checksum-verified bundled "
            "event driver nor an officially provenance-tagged Tsyganenko driver. "
            "Use --allow-unverified-driver only for a software-only exercise.",
            file=sys.stderr,
        )
        return 2
    if args.validate_driver:
        print(json.dumps(asdict(driver_info), indent=2))
        return 0

    # Make the temporal-expansion cost explicit before any solver is launched.
    # ``--interval-samples N`` means N independent epochs for every selected
    # PAMELA interval and every selected solver branch; it never means N runs in
    # total.  Printing the arithmetic here prevents accidental 35- or 70-launch
    # jobs when a user intended a five-sample diagnostic of only one interval.
    solver_branches = selected_solvers(args.solver)
    n_launches = len(midpoints) * args.interval_samples * len(solver_branches)
    print(
        "C9 execution plan: %d interval(s) x %d sample(s)/interval x %d "
        "solver branch(es) = %d AMPS launch(es)" %
        (len(midpoints), args.interval_samples, len(solver_branches), n_launches)
    )

    output_root = Path(args.output_root).expanduser()
    if not output_root.is_absolute():
        output_root = (launch_dir / output_root).resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    driver_copy = output_root / "driver" / "ts05_driver.txt"
    driver_copy.parent.mkdir(parents=True, exist_ok=True)
    if not args.skip_run:
        shutil.copy2(driver_path, driver_copy)
    elif not driver_copy.exists():
        driver_copy = driver_path

    selected_reference = [row for midpoint in midpoints for row in reference_by_midpoint[midpoint]]
    write_dict_rows(output_root / "C9_reference_used.csv", [
        {
            "interval_midpoint_utc": format_utc(row.midpoint),
            "interval_start_utc": format_utc(row.interval_start),
            "interval_end_utc": format_utc(row.interval_end),
            "rigidity_min_gv": row.rigidity_min_gv,
            "rigidity_max_gv": row.rigidity_max_gv,
            "rigidity_center_gv": row.rigidity_center_gv,
            "pamela_cutoff_aacgm_deg": row.pamela_cutoff_aacgm_deg,
            "sigma_plus_deg": row.sigma_plus_deg,
            "sigma_minus_deg": row.sigma_minus_deg,
            "missing": row.missing,
        } for row in selected_reference
    ])
    (output_root / "C9_driver_info.json").write_text(
        json.dumps(asdict(driver_info), indent=2) + "\n"
    )

    amps = Path(args.amps).expanduser()
    if not amps.is_absolute():
        amps = (launch_dir / amps).resolve()

    branch_models: Dict[str, Dict[datetime, Dict[float, Dict[str, object]]]] = {}
    branch_metrics: Dict[str, Metrics] = {}
    branch_results: Dict[str, Dict[str, object]] = {}
    combined_commands: List[Dict[str, object]] = []

    for solver in selected_solvers(args.solver):
        solver_root = output_root / solver.lower()
        if solver_root.exists() and not args.keep and not args.skip_run:
            shutil.rmtree(solver_root)
        solver_root.mkdir(parents=True, exist_ok=True)
        template = script_dir / (
            "AMPS_PARAM_C9_gridless.in" if solver == "GRIDLESS"
            else "AMPS_PARAM_C9_mode3d.in"
        )
        command_inventory: List[Dict[str, object]] = []
        interval_models: Dict[datetime, Dict[float, Dict[str, object]]] = {}
        all_model_rows: List[Dict[str, object]] = []
        # Populated only when --access-consistency-root is supplied.  Per-snapshot
        # summaries are retained so missing epochs or asymmetric sample counts are
        # visible rather than being diluted into one aggregate fraction.
        access_consistency_summaries: List[Dict[str, object]] = []
        access_consistency_details: List[Dict[str, object]] = []

        for midpoint in midpoints:
            interval_rows = reference_by_midpoint[midpoint]
            rigidities = [row.rigidity_center_gv for row in interval_rows]
            sample_times = interval_sample_times(interval_rows, args.interval_samples)
            interval_dir = solver_root / midpoint.strftime("%Y%m%dT%H%M%S")
            interval_dir.mkdir(parents=True, exist_ok=True)
            sample_estimates: Dict[float, List[BoundaryEstimate]] = {value: [] for value in rigidities}

            for sample_index, epoch in enumerate(sample_times):
                sample_dir = interval_dir / ("sample_%02d_%s" %
                                             (sample_index, epoch.strftime("%Y%m%dT%H%M%S")))
                sample_dir.mkdir(parents=True, exist_ok=True)
                input_path = sample_dir / "AMPS_PARAM_C9.in"
                if not args.skip_run:
                    render_input(template, input_path, epoch, driver_copy, args, solver)
                command = command_for(args, amps, solver, epoch)
                inventory_row = {
                    "solver": solver,
                    "interval_midpoint_utc": format_utc(midpoint),
                    "sample_epoch_utc": format_utc(epoch),
                    "cwd": str(sample_dir),
                    "command": command,
                    "resolved_dynamic_chunk": resolved_dynamic_chunk(args, solver),
                    "cutoff_evaluation": args.cutoff_evaluation,
                }
                command_inventory.append(inventory_row)
                combined_commands.append(inventory_row)
                print("C9 %s %s sample %d/%d command:\n  %s" %
                      (solver.lower(), format_utc(midpoint), sample_index + 1,
                       len(sample_times), " ".join(command)))
                if not args.skip_run and not args.dry_run:
                    return_code = run_process(
                        command, sample_dir, sample_dir / "C9_amps.log"
                    )
                    if return_code != 0:
                        print("AMPS failed with exit code %d in %s" %
                              (return_code, sample_dir), file=sys.stderr)
                        return return_code
                if args.dry_run:
                    continue

                # Every primary PAMELA_T50 comparison reads the same long-form
                # fixed-rigidity access schema. DIRECT_ACCESS writes it directly;
                # FULL_SCAN writes a companion file after its complete penumbra scan.
                if args.cutoff_evaluation == "DIRECT_ACCESS":
                    access_name = "cutoff_3d_shells_access.dat"
                else:
                    access_name = (
                        "cutoff_gridless_shells_pamela_access.dat"
                        if solver == "GRIDLESS" else
                        "cutoff_3d_shells_pamela_access.dat"
                    )
                access_path = sample_dir / access_name
                penumbra_path = sample_dir / (
                    "cutoff_gridless_shells_penumbra.dat"
                    if solver == "GRIDLESS" else "cutoff_3d_shells_penumbra.dat"
                )
                try:
                    diagnostic_estimates: Dict[str, List[BoundaryEstimate]] = {}
                    t50_profile_rows: List[Dict[str, object]] = []

                    if access_path.exists():
                        access_rows_all = parse_tecplot_shell_access(access_path)
                        access_rows = select_common_t50_geodetic_band(
                            access_rows_all,
                            args.access_abs_lat_min_deg,
                            args.access_abs_lat_max_deg,
                        )

                        # Optional method-consistency comparison is performed on
                        # the raw geodetic records before AACGM conversion.  The
                        # relative snapshot path enforces the same interval,
                        # temporal sample epoch, shell grid, and solver branch.
                        if args.access_consistency_root and solver == "GRIDDED":
                            counterpart_root = Path(
                                args.access_consistency_root).expanduser().resolve()
                            relative_sample = sample_dir.relative_to(output_root)
                            counterpart_dir = counterpart_root / relative_sample
                            counterpart_name = (
                                "cutoff_3d_shells_access.dat"
                                if args.cutoff_evaluation == "FULL_SCAN" else
                                "cutoff_3d_shells_pamela_access.dat"
                            )
                            counterpart_path = counterpart_dir / counterpart_name
                            current_product = args.cutoff_evaluation
                            counterpart_product = (
                                "DIRECT_ACCESS" if args.cutoff_evaluation == "FULL_SCAN"
                                else "FULL_SCAN"
                            )
                            if counterpart_path.exists():
                                counterpart_rows = select_common_t50_geodetic_band(
                                    parse_tecplot_shell_access(counterpart_path),
                                    args.access_abs_lat_min_deg,
                                    args.access_abs_lat_max_deg,
                                )
                                consistency, consistency_details = (
                                    compare_fixed_rigidity_access_states(
                                        access_rows, counterpart_rows,
                                        current_product, counterpart_product,
                                        args.min_access_state_agreement,
                                        args.max_access_unresolved_fraction,
                                    )
                                )
                            else:
                                # Missing matching snapshot is a hard consistency
                                # failure: it most often means the two runs used
                                # different interval-samples or timestamp selections.
                                consistency = {
                                    "current_product": current_product,
                                    "counterpart_product": counterpart_product,
                                    "counterpart_path": str(counterpart_path),
                                    "n_current_rows": len(access_rows),
                                    "n_counterpart_rows": 0,
                                    "n_common_rows": 0,
                                    "n_union_rows": len(access_rows),
                                    "common_key_coverage_fraction": 0.0,
                                    "n_missing_from_current": 0,
                                    "n_missing_from_counterpart": len(access_rows),
                                    "n_resolved_common": 0,
                                    "n_resolved_mismatch": 0,
                                    "resolved_access_state_agreement_fraction": 0.0,
                                    "n_unresolved_in_either_product": 0,
                                    "unresolved_in_either_product_fraction": 1.0,
                                    "minimum_required_resolved_agreement":
                                        args.min_access_state_agreement,
                                    "maximum_allowed_unresolved_fraction":
                                        args.max_access_unresolved_fraction,
                                    "error": "matching counterpart access file not found",
                                    "passed": False,
                                }
                                consistency_details = []

                            counterpart_input = counterpart_dir / "AMPS_PARAM_C9.in"
                            counterpart_command = _read_logged_command(
                                counterpart_dir / "C9_amps.log")
                            configuration, configuration_details = (
                                compare_access_run_configuration(
                                    input_path, counterpart_input, command,
                                    counterpart_command,
                                )
                            )
                            consistency["configuration_passed"] = bool(
                                configuration["passed"])
                            consistency["configuration_available"] = bool(
                                configuration["configuration_available"])
                            consistency["n_configuration_differences"] = int(
                                configuration["n_configuration_differences"])
                            consistency["configuration"] = configuration
                            consistency["passed"] = (
                                bool(consistency["passed"])
                                and bool(configuration["passed"])
                            )
                            consistency_details.extend(configuration_details)
                            consistency.update({
                                "solver": solver,
                                "interval_midpoint_utc": format_utc(midpoint),
                                "sample_epoch_utc": format_utc(epoch),
                                "current_path": str(access_path),
                                "counterpart_path": str(counterpart_path),
                            })
                            for detail in consistency_details:
                                detail.update({
                                    "solver": solver,
                                    "interval_midpoint_utc": format_utc(midpoint),
                                    "sample_epoch_utc": format_utc(epoch),
                                })
                            access_consistency_summaries.append(consistency)
                            access_consistency_details.extend(consistency_details)
                            (sample_dir / "C9_access_consistency.json").write_text(
                                json.dumps(consistency, indent=2) + "\n")
                            write_dict_rows(
                                sample_dir / "C9_access_consistency_differences.csv",
                                consistency_details,
                            )

                        add_aacgm_latitudes(access_rows, epoch, args.altitude_km)
                        t50_estimates, t50_profile_rows = estimate_pamela_t50_boundaries(
                            access_rows, rigidities,
                            args.t50_grid_step_deg,
                            args.t50_min_resolved_longitude_fraction,
                            args.t50_min_edge_margin_deg,
                        )
                        diagnostic_estimates["PAMELA_T50"] = t50_estimates
                    elif args.comparison_observable in ("PAMELA_T50", "ALL"):
                        raise FileNotFoundError(
                            "%s is required for PAMELA_T50. Rebuild AMPS with the "
                            "C9 T50 update and rerun this snapshot." % access_path
                        )

                    if args.cutoff_evaluation == "FULL_SCAN":
                        if not penumbra_path.exists():
                            raise FileNotFoundError(str(penumbra_path))
                        shell_rows = parse_tecplot_shell_penumbra(penumbra_path)
                        add_aacgm_latitudes(shell_rows, epoch, args.altitude_km)
                        diagnostic_estimates["RC_LOWER"] = estimate_boundaries(
                            shell_rows, rigidities, rc_attribute="rc_lower_gv")
                        diagnostic_estimates["RC_EFFECTIVE"] = estimate_boundaries(
                            shell_rows, rigidities, rc_attribute="rc_effective_gv")
                        diagnostic_estimates["RC_UPPER"] = estimate_boundaries(
                            shell_rows, rigidities, rc_attribute="rc_upper_gv")

                    primary_observable = (
                        "PAMELA_T50" if args.comparison_observable == "ALL"
                        else args.comparison_observable
                    )
                    if primary_observable not in diagnostic_estimates:
                        raise ValueError(
                            "comparison observable %s is unavailable for %s" %
                            (primary_observable, args.cutoff_evaluation)
                        )
                    estimates = diagnostic_estimates[primary_observable]
                except Exception as exc:
                    print("C9 %s postprocessing failed in %s: %s" %
                          (solver, sample_dir, exc), file=sys.stderr)
                    return 2

                write_dict_rows(sample_dir / "C9_snapshot_boundaries.csv",
                                [asdict(estimate) for estimate in estimates])
                if t50_profile_rows:
                    write_dict_rows(sample_dir / "C9_snapshot_t50_profiles.csv",
                                    t50_profile_rows)
                for observable, observable_estimates in diagnostic_estimates.items():
                    write_dict_rows(
                        sample_dir / ("C9_snapshot_boundaries_%s.csv" % observable.lower()),
                        [asdict(estimate) for estimate in observable_estimates],
                    )
                for estimate in estimates:
                    sample_estimates[estimate.rigidity_gv].append(estimate)

            if args.dry_run:
                continue

            interval_model: Dict[float, Dict[str, object]] = {}
            for rigidity in rigidities:
                estimates = sample_estimates[rigidity]
                valid = [estimate for estimate in estimates
                         if estimate.cutoff_aacgm_deg is not None]
                if valid:
                    values = [float(estimate.cutoff_aacgm_deg) for estimate in valid]
                    model = {
                        "cutoff_aacgm_deg": statistics.fmean(values),
                        "temporal_median_deg": statistics.median(values),
                        "temporal_std_deg": statistics.stdev(values) if len(values) > 1 else 0.0,
                        "longitude_hemisphere_std_mean_deg": statistics.fmean(
                            float(estimate.cutoff_std_deg or 0.0) for estimate in valid),
                        "north_south_difference_mean_deg": statistics.fmean(
                            float(estimate.north_south_difference_deg)
                            for estimate in valid
                            if estimate.north_south_difference_deg is not None
                        ) if any(estimate.north_south_difference_deg is not None for estimate in valid) else None,
                        "n_valid_snapshots": len(valid),
                        "n_requested_snapshots": len(estimates),
                        "n_valid_crossings_total": sum(estimate.n_valid_crossings for estimate in valid),
                        "n_requested_crossings_total": sum(estimate.n_requested_crossings for estimate in estimates),
                        "n_nonmonotonic_profiles_total": sum(estimate.n_nonmonotonic_profiles for estimate in estimates),
                        "isotonic_adjustment_rms_mean": statistics.fmean(
                            float(estimate.isotonic_adjustment_rms)
                            for estimate in valid
                            if estimate.isotonic_adjustment_rms is not None
                        ) if any(estimate.isotonic_adjustment_rms is not None for estimate in valid) else None,
                        "minimum_resolved_longitude_fraction": min(
                            float(estimate.minimum_resolved_longitude_fraction)
                            for estimate in valid
                            if estimate.minimum_resolved_longitude_fraction is not None
                        ) if any(estimate.minimum_resolved_longitude_fraction is not None for estimate in valid) else None,
                        "unresolved_access_fraction_mean": statistics.fmean(
                            float(estimate.unresolved_access_fraction)
                            for estimate in valid
                            if estimate.unresolved_access_fraction is not None
                        ) if any(estimate.unresolved_access_fraction is not None for estimate in valid) else None,
                        "minimum_t50_boundary_edge_margin_deg": min(
                            float(estimate.minimum_boundary_edge_margin_deg)
                            for estimate in valid
                            if estimate.minimum_boundary_edge_margin_deg is not None
                        ) if any(estimate.minimum_boundary_edge_margin_deg is not None for estimate in valid) else None,
                        "minimum_bracketed_hemispheres": min(
                            int(estimate.n_bracketed_hemispheres)
                            for estimate in valid
                            if estimate.n_bracketed_hemispheres is not None
                        ) if any(estimate.n_bracketed_hemispheres is not None for estimate in valid) else None,
                    }
                else:
                    model = {
                        "cutoff_aacgm_deg": None,
                        "temporal_median_deg": None,
                        "temporal_std_deg": None,
                        "longitude_hemisphere_std_mean_deg": None,
                        "north_south_difference_mean_deg": None,
                        "n_valid_snapshots": 0,
                        "n_requested_snapshots": len(estimates),
                        "n_valid_crossings_total": 0,
                        "n_requested_crossings_total": sum(estimate.n_requested_crossings for estimate in estimates),
                        "n_nonmonotonic_profiles_total": sum(estimate.n_nonmonotonic_profiles for estimate in estimates),
                        "isotonic_adjustment_rms_mean": None,
                        "minimum_resolved_longitude_fraction": None,
                        "unresolved_access_fraction_mean": None,
                        "minimum_t50_boundary_edge_margin_deg": None,
                        "minimum_bracketed_hemispheres": None,
                    }
                interval_model[rigidity] = model
                all_model_rows.append({
                    "solver": solver,
                    "interval_midpoint_utc": format_utc(midpoint),
                    "rigidity_center_gv": rigidity,
                    **model,
                })
            interval_models[midpoint] = interval_model
            write_dict_rows(interval_dir / "C9_interval_model.csv", [
                {"solver": solver, "rigidity_center_gv": rigidity, **model}
                for rigidity, model in sorted(interval_model.items())
            ])

        (solver_root / "C9_commands.json").write_text(
            json.dumps(command_inventory, indent=2) + "\n"
        )
        if args.dry_run:
            continue

        write_dict_rows(solver_root / "C9_model.csv", all_model_rows)
        detailed, metrics = compare(selected_reference, interval_models, driver_info, args)

        branch_access_consistency: Optional[Dict[str, object]] = None
        if args.access_consistency_root and solver == "GRIDDED":
            branch_access_consistency = aggregate_access_consistency_summaries(
                access_consistency_summaries,
                args.min_access_state_agreement,
                args.max_access_unresolved_fraction,
            )
            branch_access_consistency["counterpart_output_root"] = str(
                Path(args.access_consistency_root).expanduser().resolve())
            branch_access_consistency["current_cutoff_evaluation"] = (
                args.cutoff_evaluation)
            branch_access_consistency["counterpart_cutoff_evaluation"] = (
                "DIRECT_ACCESS" if args.cutoff_evaluation == "FULL_SCAN"
                else "FULL_SCAN")
            write_dict_rows(
                solver_root / "C9_access_consistency_snapshots.csv",
                access_consistency_summaries,
            )
            write_dict_rows(
                solver_root / "C9_access_consistency_differences.csv",
                access_consistency_details,
            )
            (solver_root / "C9_access_consistency.json").write_text(
                json.dumps(branch_access_consistency, indent=2) + "\n")

            # Observational metrics remain unchanged and are reported separately,
            # but the branch cannot receive a final PASS when two nominally
            # identical trajectory-classification paths disagree.
            metrics.passed = metrics.passed and bool(
                branch_access_consistency["passed"])

        for row in detailed:
            row["solver"] = solver
        write_dict_rows(solver_root / "C9_comparison.csv", detailed)
        plot_paths = make_plot(
            detailed, solver_root / "C9_comparison", solver,
            "PAMELA_T50" if args.comparison_observable == "ALL"
            else args.comparison_observable,
            args.plot_formats,
        )
        if plot_paths:
            print("C9 %s comparison plot(s): %s" % (
                solver, ", ".join(p.name for p in plot_paths)
            ))
        branch_result = {
            "test_id": TEST_ID,
            "test_name": TEST_NAME,
            "solver": solver,
            "profile": args.profile,
            "timestamps": [format_utc(value) for value in midpoints],
            "interval_samples": args.interval_samples,
            "altitude_km": args.altitude_km,
            "vertical_incidence": True,
            "rigidity_bin_representation": "geometric center",
            "cutoff_evaluation": args.cutoff_evaluation,
            "comparison_observable": (
                "PAMELA_T50" if args.comparison_observable == "ALL"
                else args.comparison_observable
            ),
            "modeled_boundary_definition": (
                "longitude-averaged, hemisphere-resolved vertical transmission T=0.5 "
                "after weighted isotonic regression"
                if args.comparison_observable in ("PAMELA_T50", "ALL") else
                "%s equals rigidity-bin geometric center" % args.comparison_observable
            ),
            "t50_postprocessing": {
                "aacgm_grid_step_deg": args.t50_grid_step_deg,
                "common_abs_geodetic_latitude_band_deg": [
                    args.access_abs_lat_min_deg, args.access_abs_lat_max_deg],
                "minimum_resolved_longitude_fraction":
                    args.t50_min_resolved_longitude_fraction,
                "minimum_boundary_edge_margin_deg":
                    args.t50_min_edge_margin_deg,
                "requires_both_hemispheres": True,
                "requires_explicit_half_transmission_bracket": True,
                "monotonic_fit": "weighted pool-adjacent-violators isotonic regression",
            } if args.comparison_observable in ("PAMELA_T50", "ALL") else None,
            "pamela_access_rigidities_gv": args.direct_rigidities_gv,
            "direct_access_calculation_abs_geodetic_latitude_band_deg": (
                [args.access_abs_lat_min_deg, args.access_abs_lat_max_deg]
                if args.cutoff_evaluation == "DIRECT_ACCESS" else None
            ),
            "access_state_consistency": branch_access_consistency,
            "shell_longitude_resolution_deg": args.shell_lon_res_deg,
            "shell_latitude_resolution_deg": args.shell_lat_res_deg,
            "cutoff_scan_n": args.cutoff_scan_n,
            "resolved_dynamic_chunk": resolved_dynamic_chunk(args, solver),
            "mode3d": ({
                "threads_per_mpi_process": args.nt,
                "field_evaluation": "MESH",
                "mesh_resolution_earth_re": args.mode3d_mesh_res_earth_re,
                "mesh_resolution_boundary_re": args.mode3d_mesh_res_boundary_re,
                "mesh_coarsening": args.mode3d_mesh_coarsening,
                "mesh_exponent": args.mode3d_mesh_exponent,
            } if solver == "GRIDDED" else None),
            "driver": asdict(driver_info),
            "acceptance": {
                "min_valid_fraction": args.min_valid_fraction,
                "max_rmse_deg": args.max_rmse_deg,
                "max_abs_bias_deg": args.max_abs_bias_deg,
                "min_correlation": args.min_correlation,
                "suppression_range_deg": [args.min_suppression_deg, args.max_suppression_deg],
                "max_minimum_time_error_minutes": args.max_minimum_time_error_minutes,
                "minimum_resolved_access_state_agreement": (
                    args.min_access_state_agreement
                    if args.access_consistency_root and solver == "GRIDDED" else None),
                "maximum_access_unresolved_fraction": (
                    args.max_access_unresolved_fraction
                    if args.access_consistency_root and solver == "GRIDDED" else None),
            },
            "metrics": asdict(metrics),
            "limitations": [
                "Global-shell public-data approximation; not an exact PAMELA orbit/attitude reproduction.",
                ("PAMELA_T50 is calculated identically from fixed-rigidity access "
                 "states for DIRECT_ACCESS and FULL_SCAN over the same configured "
                 "geodetic latitude band; Rc_lower, Rc_effective, and Rc_upper are "
                 "retained as FULL_SCAN diagnostics."),
                ("This run samples each 94-minute interval only at its midpoint."
                 if args.interval_samples == 1 else
                 "This run averages %d samples distributed across each 94-minute interval."
                 % args.interval_samples),
                ("Mode3D result includes field-mesh interpolation error."
                 if solver == "GRIDDED" else
                 "Gridless result directly evaluates the empirical field along trajectories."),
            ],
        }
        (solver_root / "C9_result.json").write_text(
            json.dumps(branch_result, indent=2) + "\n"
        )
        branch_models[solver] = interval_models
        branch_metrics[solver] = metrics
        branch_results[solver] = branch_result
        print(
            "C9 %s: valid=%d/%d RMSE=%.3f deg bias=%+.3f deg corr=%s "
            "suppression=%s deg -> %s%s" %
            (solver.lower(), metrics.n_valid_model, metrics.n_reference,
             metrics.rmse_deg, metrics.mean_bias_deg,
             "n/a" if metrics.correlation is None else "%.3f" % metrics.correlation,
             "n/a" if metrics.modeled_low_rigidity_suppression_deg is None else
             "%.3f" % metrics.modeled_low_rigidity_suppression_deg,
             "PASS" if metrics.passed else "FAIL",
             " (unverified alternate driver; not scientifically eligible)"
             if not metrics.scientific_validation_eligible else "")
        )
        if branch_access_consistency is not None:
            print(
                "C9 %s access consistency: common=%d resolved=%d mismatches=%d "
                "agreement=%.6f unresolved=%.6f config_diff=%d -> %s" %
                (solver.lower(),
                 int(branch_access_consistency["n_common_rows"]),
                 int(branch_access_consistency["n_resolved_common"]),
                 int(branch_access_consistency["n_resolved_mismatch"]),
                 float(branch_access_consistency[
                     "resolved_access_state_agreement_fraction"]),
                 float(branch_access_consistency[
                     "unresolved_in_either_product_fraction"]),
                 int(branch_access_consistency[
                     "n_configuration_differences"]),
                 "PASS" if branch_access_consistency["passed"] else "FAIL")
            )

    (output_root / "C9_commands.json").write_text(
        json.dumps(combined_commands, indent=2) + "\n"
    )
    if args.dry_run:
        print("C9 dry run complete for %s: %s" %
              (", ".join(selected_solvers(args.solver)), output_root))
        return 0

    cross_rows, cross_diagnostics = compare_solver_branches(branch_models)
    if cross_rows:
        write_dict_rows(output_root / "C9_cross_solver_comparison.csv", cross_rows)
        (output_root / "C9_cross_solver_result.json").write_text(
            json.dumps(cross_diagnostics, indent=2) + "\n"
        )

    passed = bool(branch_metrics) and all(value.passed for value in branch_metrics.values())
    top_result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "requested_solver": args.solver,
        "selected_solvers": list(selected_solvers(args.solver)),
        "cutoff_evaluation": args.cutoff_evaluation,
        "comparison_observable": (
            "PAMELA_T50" if args.comparison_observable == "ALL"
            else args.comparison_observable
        ),
        "profile": args.profile,
        "driver": asdict(driver_info),
        "branches": {
            solver: {
                "output_directory": str((output_root / solver.lower()).resolve()),
                "metrics": asdict(metrics),
                "access_state_consistency": branch_results[solver].get(
                    "access_state_consistency"),
            }
            for solver, metrics in branch_metrics.items()
        },
        "cross_solver_diagnostics": cross_diagnostics or None,
        "passed": passed,
    }
    (output_root / "C9_result.json").write_text(
        json.dumps(top_result, indent=2) + "\n"
    )
    print("C9 selected branches -> %s" % ("PASS" if passed else "FAIL"))
    print("Results: %s" % output_root)
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
