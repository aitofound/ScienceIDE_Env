#!/usr/bin/env python3
"""C10 - POES/MetOp MEPED proton-access-boundary storm validation.

C10 is the routinely executable, public-data POES/MetOp companion to the PAMELA
storm test (C9).  It validates whether the AMPS time-dependent IGRF+T05/TS05
backtracing calculation reproduces the observed equatorward motion, magnetic
local-time (MLT) asymmetry, and recovery of the low-Earth-orbit proton-access
boundary (the "SEP cutoff boundary") during the December 2006 storm.

Observable
----------
For a proton rigidity R, the access boundary is the equatorward-most absolute
AACGM (invariant) latitude at which vertically incident protons of rigidity R
first gain access from the polar cap.  Observationally this is the low-latitude
edge of the POES/MetOp SEM-2 MEPED omnidirectional integral-proton enhancement.
The boundary moves equatorward and becomes more MLT-asymmetric with geomagnetic
activity, and is well described by a displaced/elliptical boundary in the polar
(invariant-colatitude, MLT) plane (Dmitriev et al. 2010).

Unlike PAMELA (a single spacecraft in one orbit plane), every ingredient of the
POES/MetOp comparison is public: the SEM-2 MEPED fluxes, the sub-satellite
position (so no attitude/ephemeris is required), and the field driver reuse the
same checksum-verified five-minute TS05 event driver bundled with C9.

Model side
----------
For each selected storm interval the runner evaluates one or more TS05 field
snapshots on a global geodetic shell at POES altitude (850 km) and evaluates the
four channel-label rigidities corresponding to the nominal lower thresholds of
MEPED P6, P7, P8, and P9: 16, 36, 70, and 140 MeV.  These are integral detector
channels; the assigned rigidities are transparent labels, not monochromatic
instrument responses.
Two numerical cutoff-evaluation products are available, following C9:

* FULL_SCAN uses PENUMBRA_SCAN on the complete shell.  It retains Rc_lower,
  Rc_effective, and Rc_upper and also writes exact access states at the four
  POES/MetOp channel rigidities.  FULL_SCAN supports GRIDLESS and GRIDDED.
* DIRECT_ACCESS is a GRIDDED-only fast product.  It traces only the four exact
  channel rigidities and only inside the configured absolute-geodetic-latitude
  band.  It avoids the 120-rigidity penumbra scan and most low-latitude nodes.

The common primary model observable is ACCESS_T50.  For both products the exact
binary access states are reduced with the same MLT-sector, hemisphere-resolved
transmission calculation.  The boundary is the T=0.5 crossing after weighted
nondecreasing isotonic regression.  On the observational side the reference
uses a background-normalized isotonic T50 rather than an uncorrected
``0.5*polar_flux`` threshold.  The default code-validation gate uses P6/P7 on
independent windows; P8/P9 remain plotted diagnostics until detector-response
folding is implemented.

Boundary extraction
-------------------
For each rigidity, hemisphere, and MLT sector, exact-rigidity longitude profiles
are sampled on a common absolute-AACGM-latitude grid.  The resolved allowed-state
fraction is the modeled transmission.  Insufficiently resolved grid points are
removed, weighted isotonic regression suppresses finite-longitude reversals, and
a boundary is reported only when T=0.5 is explicitly bracketed away from the
latitude-domain edge.  FULL_SCAN additionally retains Rc_lower, Rc_effective,
and Rc_upper contour boundaries as diagnostics.  For each rigidity and
hemisphere the eight MLT boundary latitudes are summarized with a first-harmonic
polar-boundary fit, theta_b(phi) = thetaC + dTheta*cos(phi - phi0).  This compact
three-parameter diagnostic captures mean expansion and first-order asymmetry; it
is not presented as the full five-parameter geometric ellipse used by Dmitriev
et al. (2010).  The fitted centre colatitude
(mean equatorward expansion), asymmetry amplitude (dTheta), and duskward phase
(phi0) are compared with the reference.

Reference
---------
``reference_C10_poes_meped_boundary.csv.gz`` is produced from the real NOAA/NCEI
Level-2 16-second SEM-2 archive by ``build_poes_reference.py``. The papers define
the extraction method but do not contain the complete numerical pass-level
boundary list; a scientific C10 run therefore requires archive-derived rows.

Matplotlib is used only when available to create the per-branch time-series,
scatter, and MLT diagnostic plots, each written in every format named by
``--plot-formats`` (PNG and EPS by default -- a raster copy for quick viewing
and a vector copy suitable for publication); a missing Matplotlib installation
never changes pass/fail.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import logging
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
from dataclasses import asdict, dataclass, field
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Dict, List, Mapping, Optional, Sequence, Tuple

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_REFERENCE = SCRIPT_DIR / "reference_C10_poes_meped_boundary.csv.gz"
def _default_driver_path() -> Path:
    """Locate the checksum-verified C9 December-2006 TS05 driver.

    In an AMPS checkout C10 normally sits next to C9, so the first scientific
    source is the sibling C9 data directory.  A local C10 copy is also accepted
    to make an exported test bundle self-contained after the user copies the
    verified file.
    """

    candidates = (
        SCRIPT_DIR / "data" / "ts05_driving.txt",
        SCRIPT_DIR.parent / "C9" / "data" / "ts05_driving.txt",
        SCRIPT_DIR.parent / "C9" / "ts05_driving.txt",
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


DEFAULT_DRIVER = _default_driver_path()
DEFAULT_TEMPLATE_GRIDLESS = SCRIPT_DIR / "AMPS_PARAM_C10_gridless.in"
DEFAULT_TEMPLATE_MODE3D = SCRIPT_DIR / "AMPS_PARAM_C10_mode3d.in"

PROTON_REST_MEV = 938.272088
SOLVERS = ("GRIDLESS", "GRIDDED", "BOTH")
CUTOFF_EVALUATIONS = ("FULL_SCAN", "DIRECT_ACCESS")
COMPARISON_OBSERVABLES = ("ACCESS_T50", "RC_LOWER", "RC_EFFECTIVE", "RC_UPPER", "ALL")
BUNDLED_DRIVER_SHA256 = "cb3f3f1959763660beb1e26e5a49489b132708944fb91c4e1ee37cfc3a6c4317"

# ROUTINE storm-phase interval midpoints (must exist in the reference grid).
ROUTINE_MIDPOINTS = (
    "2006-12-14T06:00:00Z",  # pre-storm quiet reference
    "2006-12-14T14:00:00Z",  # shock / storm sudden commencement
    "2006-12-14T18:00:00Z",  # early main phase
    "2006-12-14T22:00:00Z",  # main phase
    "2006-12-15T00:00:00Z",  # main-phase deepening
    "2006-12-15T03:00:00Z",  # near minimum cutoff latitude
    "2006-12-15T06:00:00Z",  # early recovery
    "2006-12-15T12:00:00Z",  # recovery
    "2006-12-15T18:00:00Z",  # recovery
    "2006-12-16T04:00:00Z",  # late recovery
)
SMOKE_MIDPOINTS = ("2006-12-14T14:00:00Z", "2006-12-15T03:00:00Z")


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class ReferenceRow:
    midpoint: datetime
    interval_start: datetime
    interval_end: datetime
    rigidity_gv: float
    energy_threshold_mev: float
    channel: str
    hemisphere: str
    mlt_hour: float
    boundary_lat_deg: Optional[float]
    sigma_deg: Optional[float]
    altitude_km: float
    sym_h_nt: float
    missing: bool
    source: str
    n_crossings: int = 0
    satellites: str = ""
    validation_role: str = "LEGACY_DIAGNOSTIC"
    acceptance_eligible: bool = False
    background_corrected: bool = False
    n_distinct_pass_legs: int = 0
    n_distinct_satellites: int = 0
    median_transition_width_deg: Optional[float] = None
    diagnostic_eligible: bool = False
    quality_status: str = "LEGACY_UNSPECIFIED"
    n_aggregate_eligible_crossings: int = 0
    n_cross_channel_outliers: int = 0
    median_transition_support_samples: Optional[float] = None
    median_contrast_to_noise_ratio: Optional[float] = None


@dataclass(frozen=True)
class DriverInfo:
    path: str
    sha256: str
    verified_driver: bool
    n_records: int
    median_cadence_seconds: float
    first_utc: str
    last_utc: str


@dataclass
class ShellRow:
    longitude_deg: float
    latitude_deg: float
    rc_lower_gv: float
    rc_effective_gv: float
    rc_upper_gv: float
    aacgm_latitude_deg: Optional[float] = None
    mlt_hour: Optional[float] = None


@dataclass
class AccessRow:
    """One exact-rigidity shell classification written by AMPS.

    ``access_state`` follows the common Earth cutoff contract:
    0=PHYSICAL_FORBIDDEN, 1=ALLOWED, and 2=UNRESOLVED.  ``allowed`` and
    ``unresolved`` are redundant writer diagnostics and are verified while the
    Tecplot product is parsed.
    """

    longitude_deg: float
    latitude_deg: float
    rigidity_gv: float
    access_state: int
    allowed: int
    unresolved: int
    aacgm_latitude_deg: Optional[float] = None
    mlt_hour: Optional[float] = None


@dataclass
class EllipseFit:
    center_colat_deg: Optional[float]      # mean equatorward expansion (90 - mean|lat|)
    amplitude_deg: Optional[float]         # dTheta, day/night asymmetry semi-amplitude
    phase_mlt_hour: Optional[float]        # MLT of most-equatorward extent (duskward shift)
    rms_residual_deg: Optional[float]


@dataclass
class BoundaryEstimate:
    rigidity_gv: float
    hemisphere: str
    # per-MLT boundary |AACGM latitude|; None where no poleward crossing resolved
    boundary_by_mlt: Dict[float, Optional[float]]
    n_valid_mlt: int
    n_requested_mlt: int
    ellipse: EllipseFit
    observable: str = "ACCESS_T50"


@dataclass
class Metrics:
    # Primary code-validation gate (background-corrected P6/P7 independent cells).
    n_reference: int
    n_valid_model: int
    valid_fraction: float
    mean_bias_deg: float
    mean_abs_error_deg: float
    rmse_deg: float
    max_abs_error_deg: float
    correlation: Optional[float]
    observed_expansion_deg: Optional[float]
    modeled_expansion_deg: Optional[float]
    observed_max_expansion_time_utc: Optional[str]
    modeled_max_expansion_time_utc: Optional[str]
    max_expansion_time_error_minutes: Optional[float]
    observed_mean_asymmetry_deg: Optional[float]
    modeled_mean_asymmetry_deg: Optional[float]
    driver_verified: bool
    scientific_validation_eligible: bool
    passed_numerical_comparison: bool
    passed: bool
    # Diagnostic coverage across every channel and overlapping plotting cell.
    n_all_reference: int = 0
    n_all_valid_model: int = 0
    all_mean_bias_deg: Optional[float] = None
    all_rmse_deg: Optional[float] = None
    unfiltered_all_mean_bias_deg: Optional[float] = None
    unfiltered_all_rmse_deg: Optional[float] = None
    reference_processing_eligible: bool = False
    acceptance_channels: Tuple[str, ...] = ("P6", "P7")
    per_channel_metrics: Dict[str, Dict[str, object]] = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------
def parse_utc(text: str) -> datetime:
    text = text.strip().replace("Z", "")
    return datetime.strptime(text, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=timezone.utc)


def format_utc(dt: datetime, suffix_z: bool = True) -> str:
    base = dt.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
    return base + ("Z" if suffix_z else "")


def kinetic_energy_mev_from_rigidity_gv(rigidity_gv: float) -> float:
    momentum_mev = rigidity_gv * 1000.0
    return math.hypot(momentum_mev, PROTON_REST_MEV) - PROTON_REST_MEV


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def mlt_bin_center(mlt_hour: float, n_bins: int) -> float:
    """Snap a continuous MLT to the nearest reference bin label.

    The reference grid labels bins by their nominal MLT (0, 3, ... , 21 for the
    default eight 3-hour bins), so a node's MLT is assigned to the nearest such
    label modulo 24.
    """
    width = 24.0 / n_bins
    idx = round((mlt_hour % 24.0) / width)
    return round((idx * width) % 24.0, 3)


# ---------------------------------------------------------------------------
# Reference loading
# ---------------------------------------------------------------------------
def load_reference(path: Path) -> List[ReferenceRow]:
    """Load a gzip-compressed C10 reference (or an explicit legacy plain CSV).

    The default and checked-in product is ``*.csv.gz``.  Plain CSV reading is
    retained only for backward-compatible developer fixtures and explicit
    legacy paths.
    """

    rows: List[ReferenceRow] = []
    if path.name.lower().endswith(".gz") and not path.exists():
        legacy_path = path.with_suffix("")
        if legacy_path.exists():
            raise FileNotFoundError(
                "compressed reference %s is missing, but legacy uncompressed %s exists; "
                "regenerate the reference or run: gzip -n -9 %s"
                % (path, legacy_path, legacy_path)
            )
    if path.name.lower().endswith(".gz"):
        stream_context = gzip.open(path, "rt", encoding="utf-8", newline="")
    else:
        stream_context = path.open("r", encoding="utf-8", newline="")
    with stream_context as stream:
        data_lines = [line for line in stream if not line.startswith("#")]
    reader = csv.DictReader(data_lines)
    for record in reader:
        def opt_float(name: str) -> Optional[float]:
            value = record.get(name, "")
            if value is None or value.strip() == "":
                return None
            return float(value)
        missing = str(record.get("missing", "")).strip().upper() in ("TRUE", "1", "YES")
        rows.append(ReferenceRow(
            midpoint=parse_utc(record["interval_midpoint_utc"]),
            interval_start=parse_utc(record["interval_start_utc"]),
            interval_end=parse_utc(record["interval_end_utc"]),
            rigidity_gv=float(record["rigidity_gv"]),
            energy_threshold_mev=float(record.get("energy_threshold_mev", "nan") or "nan"),
            channel=record.get("channel", ""),
            hemisphere=record["hemisphere"].strip().upper(),
            mlt_hour=float(record["mlt_hour"]),
            boundary_lat_deg=None if missing else opt_float("boundary_aacgm_lat_deg"),
            sigma_deg=opt_float("sigma_deg"),
            altitude_km=float(record.get("altitude_km", "850") or "850"),
            sym_h_nt=float(record.get("sym_h_nt", "nan") or "nan"),
            missing=missing,
            source=record.get("source", ""),
            n_crossings=int(float(record.get("n_crossings", "0") or "0")),
            satellites=record.get("satellites", ""),
            validation_role=(record.get("validation_role", "LEGACY_DIAGNOSTIC")
                             or "LEGACY_DIAGNOSTIC").strip().upper(),
            acceptance_eligible=str(record.get("acceptance_eligible", "")).strip().upper()
                                in ("TRUE", "1", "YES"),
            background_corrected=str(record.get("background_corrected", "")).strip().upper()
                                 in ("TRUE", "1", "YES"),
            n_distinct_pass_legs=int(float(record.get("n_distinct_pass_legs", "0") or "0")),
            n_distinct_satellites=int(float(record.get("n_distinct_satellites", "0") or "0")),
            median_transition_width_deg=opt_float("median_transition_width_deg"),
            diagnostic_eligible=str(record.get("diagnostic_eligible", "")).strip().upper()
                                in ("TRUE", "1", "YES"),
            quality_status=(record.get("quality_status", "LEGACY_UNSPECIFIED")
                            or "LEGACY_UNSPECIFIED").strip().upper(),
            n_aggregate_eligible_crossings=int(float(
                record.get("n_aggregate_eligible_crossings", "0") or "0")),
            n_cross_channel_outliers=int(float(
                record.get("n_cross_channel_outliers", "0") or "0")),
            median_transition_support_samples=opt_float("median_transition_support_samples"),
            median_contrast_to_noise_ratio=opt_float("median_contrast_to_noise_ratio"),
        ))
    if not rows:
        raise ValueError("no reference rows parsed from %s" % path)
    return rows


def reference_axes(rows: Sequence[ReferenceRow]) -> Tuple[List[float], List[float], List[str]]:
    rigidities = sorted({round(r.rigidity_gv, 9) for r in rows})
    mlt_bins = sorted({round(r.mlt_hour, 3) for r in rows})
    hemispheres = sorted({r.hemisphere for r in rows})
    return rigidities, mlt_bins, hemispheres


def validate_references(rows: Sequence[ReferenceRow]) -> Tuple[bool, List[str]]:
    """Validate both grid completeness and code-validation eligibility."""

    problems: List[str] = []
    rigidities, mlt_bins, hemispheres = reference_axes(rows)
    midpoints = sorted({r.midpoint for r in rows})
    expected = len(rigidities) * len(mlt_bins) * len(hemispheres)
    for midpoint in midpoints:
        got = sum(1 for r in rows if r.midpoint == midpoint)
        if got != expected:
            problems.append("interval %s has %d rows, expected %d"
                            % (format_utc(midpoint), got, expected))
    for r in rows:
        if not r.missing and r.boundary_lat_deg is not None:
            if not (20.0 <= r.boundary_lat_deg <= 89.0):
                problems.append("boundary latitude out of range: %s %.3f GV %s MLT%.1f = %.2f"
                                % (format_utc(r.midpoint), r.rigidity_gv,
                                   r.hemisphere, r.mlt_hour, r.boundary_lat_deg))
        if r.acceptance_eligible:
            if r.validation_role != "PRIMARY":
                problems.append("diagnostic channel marked acceptance eligible: %s %s"
                                % (format_utc(r.midpoint), r.channel))
            if not r.background_corrected:
                problems.append("acceptance cell is not background corrected: %s %s"
                                % (format_utc(r.midpoint), r.channel))
            if r.n_distinct_pass_legs < 2:
                problems.append("acceptance cell has fewer than two distinct pass legs: %s %s"
                                % (format_utc(r.midpoint), r.channel))
        if r.validation_role == "DIAGNOSTIC" and r.diagnostic_eligible:
            if r.n_aggregate_eligible_crossings < 2:
                problems.append("robust diagnostic cell has fewer than two eligible crossings: %s %s"
                                % (format_utc(r.midpoint), r.channel))
            if r.n_distinct_pass_legs < 2 or r.n_distinct_satellites < 2:
                problems.append("robust diagnostic cell lacks independent pass/satellite support: %s %s"
                                % (format_utc(r.midpoint), r.channel))
            # A robust diagnostic cell may retain one or more rejected P9
            # cross-channel outliers in its provenance while computing the
            # boundary only from the remaining aggregate-eligible crossings.
            # Reject the cell only when the counts imply that an outlier was
            # included in the robust estimate, or when the cell itself is
            # explicitly classified as an outlier-only diagnostic.
            n_ineligible = max(0, r.n_crossings - r.n_aggregate_eligible_crossings)
            if r.n_cross_channel_outliers > n_ineligible:
                problems.append(
                    "cross-channel outlier appears in the robust diagnostic estimate: %s %s"
                    % (format_utc(r.midpoint), r.channel)
                )
            if r.quality_status == "DIAGNOSTIC_CROSS_CHANNEL_OUTLIER":
                problems.append(
                    "outlier-only diagnostic cell marked robust: %s %s"
                    % (format_utc(r.midpoint), r.channel)
                )

    if any(r.quality_status == "LEGACY_UNSPECIFIED" for r in rows):
        problems.append(
            "reference lacks C10 diagnostic-quality metadata; rebuild it with the current "
            "build_poes_reference.py"
        )

    sources = {r.source for r in rows if not r.missing}
    model_only = bool(sources) and all("MODEL" in source.upper() for source in sources)
    archive_only = bool(sources) and all(source.upper().startswith("POES_NCEI") for source in sources)
    if model_only:
        problems.append("reference source is MODEL-kind; rebuild it from the NCEI archive")
    elif not archive_only:
        problems.append("nonmissing reference rows must all have a POES_NCEI source identifier")

    primary_channels = {r.channel for r in rows if r.validation_role == "PRIMARY"}
    if not {"P6", "P7"}.issubset(primary_channels):
        problems.append("reference must identify P6 and P7 as PRIMARY validation channels")
    acceptance_rows = [
        r for r in rows
        if r.acceptance_eligible and not r.missing and r.boundary_lat_deg is not None
    ]
    if not acceptance_rows:
        problems.append(
            "reference has no acceptance-eligible background-corrected P6/P7 cells; "
            "rebuild it with the current build_poes_reference.py"
        )
    return (len(problems) == 0), problems


# ---------------------------------------------------------------------------
# Driver validation (mirrors the C9 checks)
# ---------------------------------------------------------------------------
def driver_records(path: Path) -> List[Tuple[datetime, List[str]]]:
    records: List[Tuple[datetime, List[str]]] = []
    with path.open() as stream:
        for raw in stream:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            fields = line.split()
            if len(fields) < 20:
                continue
            records.append((parse_utc(fields[0]), fields))
    records.sort(key=lambda item: item[0])
    return records


def driver_sym_h_series(path: Path) -> List[Tuple[datetime, float]]:
    """Return five-minute ``(UTC, SYM-H nT)`` samples from driver column 9."""

    result: List[Tuple[datetime, float]] = []
    for when, fields in driver_records(path):
        try:
            value = float(fields[9])
        except (IndexError, ValueError):
            continue
        if math.isfinite(value):
            result.append((when, value))
    if not result:
        raise ValueError("no SYM-H values parsed from TS05 driver %s" % path)
    return result


def driver_scalar_at(series: Sequence[Tuple[datetime, float]], when: datetime) -> float:
    """Linearly interpolate a monotonic five-minute driver scalar series."""

    if not series:
        return float("nan")
    if when <= series[0][0]:
        return float(series[0][1])
    if when >= series[-1][0]:
        return float(series[-1][1])
    lo, hi = 0, len(series) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if series[mid][0] <= when:
            lo = mid
        else:
            hi = mid
    t0, v0 = series[lo]
    t1, v1 = series[hi]
    span = (t1 - t0).total_seconds()
    fraction = 0.0 if span <= 0.0 else (when - t0).total_seconds() / span
    return float(v0 + fraction * (v1 - v0))


def validate_driver(path: Path, required_times: Sequence[datetime]) -> DriverInfo:
    records = driver_records(path)
    if len(records) < 2:
        raise ValueError("driver %s has too few records" % path)
    times = [rec[0] for rec in records]
    gaps = [(b - a).total_seconds() for a, b in zip(times, times[1:])]
    for gap in gaps:
        if gap <= 0.0:
            raise ValueError("driver timestamps are not strictly increasing")
    median_cadence = statistics.median(gaps)
    if not 299.0 <= median_cadence <= 301.0:
        raise ValueError("driver median cadence is not five minutes: %.1f s" % median_cadence)
    if max(gaps) > 600.0:
        raise ValueError("driver has a gap larger than ten minutes: %.1f s" % max(gaps))
    for when in required_times:
        if when < times[0] or when > times[-1]:
            raise ValueError("driver does not cover required time %s" % format_utc(when))
    digest = sha256(path)
    return DriverInfo(
        path=str(path.resolve()),
        sha256=digest,
        verified_driver=(digest == BUNDLED_DRIVER_SHA256),
        n_records=len(records),
        median_cadence_seconds=median_cadence,
        first_utc=format_utc(times[0]),
        last_utc=format_utc(times[-1]),
    )


# ---------------------------------------------------------------------------
# Interval / sample selection
# ---------------------------------------------------------------------------
def selected_midpoints(rows: Sequence[ReferenceRow], args: argparse.Namespace) -> List[datetime]:
    available = sorted({r.midpoint for r in rows})
    if args.timestamps.strip():
        requested = [parse_utc(token) for token in args.timestamps.split(",") if token.strip()]
        chosen = [m for m in available if m in set(requested)]
        missing = sorted(set(requested) - set(chosen))
        if missing:
            raise ValueError("requested timestamps not in reference grid: %s"
                             % ", ".join(format_utc(m) for m in missing))
        return chosen
    if args.profile == "FULL":
        return available
    wanted = ROUTINE_MIDPOINTS if args.profile == "ROUTINE" else SMOKE_MIDPOINTS
    wanted_set = {parse_utc(m) for m in wanted}
    chosen = [m for m in available if m in wanted_set]
    missing = sorted(wanted_set - set(chosen))
    if missing:
        raise ValueError("%s midpoints missing from reference grid: %s"
                         % (args.profile, ", ".join(format_utc(m) for m in missing)))
    return chosen


def interval_sample_times(midpoint: datetime, start: datetime, end: datetime,
                          count: int) -> List[datetime]:
    if count <= 1:
        return [midpoint]
    span = (end - start).total_seconds()
    return [start + timedelta(seconds=span * i / (count - 1)) for i in range(count)]


# ---------------------------------------------------------------------------
# Input rendering + command construction
# ---------------------------------------------------------------------------
def replace_directives(template_text: str, replacements: Mapping[str, str]) -> str:
    lines = []
    for line in template_text.splitlines():
        stripped = line.strip()
        if stripped and not stripped.startswith(("!", "#")):
            key = stripped.split()[0]
            if key in replacements:
                indent = line[:len(line) - len(line.lstrip())]
                lines.append("%s%s %s" % (indent, key, replacements[key]))
                continue
        lines.append(line)
    return "\n".join(lines) + "\n"


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
        "CUTOFF_RIGIDITY_LIST_GV": ",".join("%.12g" % r for r in args.rigidities_gv),
        "CUTOFF_ACCESS_ABS_LAT_MIN": "%.8g" % args.access_abs_lat_min_deg,
        "CUTOFF_ACCESS_ABS_LAT_MAX": "%.8g" % args.access_abs_lat_max_deg,
        "CUTOFF_MAX_TRAJ_TIME": "%.8g" % args.max_trace_time,
        "EPOCH": format_utc(epoch, suffix_z=False),
        "DRIVER_FILE": "ts05_driver.txt",
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


def resolved_dynamic_chunk(args: argparse.Namespace, solver: str) -> int:
    if args.dynamic_chunk > 0:
        return args.dynamic_chunk
    workers = args.nt if solver == "GRIDDED" else 1
    return max(1, 4 * workers)


def command_for(args: argparse.Namespace, amps: Path, solver: str, epoch: datetime) -> List[str]:
    """Build one auditable AMPS command for FULL_SCAN or DIRECT_ACCESS."""
    chunk = resolved_dynamic_chunk(args, solver)
    command = [
        args.mpirun, "-np", str(args.np), str(amps),
        "-mode", "gridless" if solver == "GRIDLESS" else "3d",
        "-i", "AMPS_PARAM_C10.in", "--epoch", format_utc(epoch, suffix_z=False),
        "-cutoff-search", (
            "RIGIDITY_LIST" if args.cutoff_evaluation == "DIRECT_ACCESS"
            else "PENUMBRA_SCAN"
        ),
        "-cutoff-trace-policy", args.cutoff_trace_policy,
    ]
    rigidity_list = ",".join("%.12g" % value for value in args.rigidities_gv)
    if args.cutoff_evaluation == "FULL_SCAN":
        # A nonempty exact-rigidity list requests the companion access-state
        # product while preserving the complete penumbra scan.
        command += [
            "-cutoff-upper-scan-n", str(args.cutoff_scan_n),
            "-cutoff-rigidity-list-gv", rigidity_list,
        ]
    else:
        command += [
            "-cutoff-rigidity-list-gv", rigidity_list,
            "-cutoff-access-abs-lat-min", str(args.access_abs_lat_min_deg),
            "-cutoff-access-abs-lat-max", str(args.access_abs_lat_max_deg),
        ]
    if solver == "GRIDLESS":
        command += ["-gridless-mpi-scheduler", args.scheduler,
                    "-gridless-mpi-dynamic-chunk", str(chunk)]
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
        proc = subprocess.run(command, cwd=str(cwd), stdout=log,
                              stderr=subprocess.STDOUT, check=False)
    return proc.returncode


# ---------------------------------------------------------------------------
# Tecplot penumbra shell parser (shared schema with C9)
# ---------------------------------------------------------------------------
def normalize_tecplot_variable_name(name: str) -> str:
    key = name.strip().strip('"').lower()
    key = re.sub(r"[\s\-\(\)\[\]/]+", "_", key)
    key = re.sub(r"[^0-9a-z_]", "", key).strip("_")
    aliases = {
        "longitude_deg": "lon_deg", "lon": "lon_deg", "longitude": "lon_deg",
        "latitude_deg": "lat_deg", "lat": "lat_deg", "latitude": "lat_deg",
        "rc_lower": "rc_lower_gv", "rc_effective": "rc_effective_gv",
        "rc_upper": "rc_upper_gv",
    }
    return aliases.get(key, key)


def parse_tecplot_shell_penumbra(path: Path) -> List[ShellRow]:
    variables: List[str] = []
    numeric_rows: List[Tuple[int, List[float]]] = []
    with path.open() as stream:
        for line_number, raw in enumerate(stream, start=1):
            text = raw.strip()
            if not text:
                continue
            upper = text.upper()
            if upper.startswith("VARIABLES"):
                variables = [normalize_tecplot_variable_name(v)
                             for v in re.findall(r'"([^"]+)"', text)]
                continue
            if upper.startswith(("TITLE", "ZONE")):
                continue
            try:
                numeric_rows.append((line_number, [float(tok) for tok in text.split()]))
            except ValueError as exc:
                raise ValueError("%s line %d is not a numeric Tecplot row: %s"
                                 % (path, line_number, text)) from exc
    if not variables:
        raise ValueError("Tecplot VARIABLES record not found in %s" % path)
    required = {"lon_deg", "lat_deg", "rc_lower_gv", "rc_effective_gv", "rc_upper_gv"}
    missing = sorted(required - set(variables))
    if missing:
        raise ValueError("%s missing required C10 variables: %s" % (path, ", ".join(missing)))
    index = {name: variables.index(name) for name in required}
    rows: List[ShellRow] = []
    for line_number, values in numeric_rows:
        if len(values) != len(variables):
            raise ValueError("%s line %d has %d columns, VARIABLES defines %d"
                             % (path, line_number, len(values), len(variables)))
        rows.append(ShellRow(
            longitude_deg=float(values[index["lon_deg"]]) % 360.0,
            latitude_deg=float(values[index["lat_deg"]]),
            rc_lower_gv=float(values[index["rc_lower_gv"]]),
            rc_effective_gv=float(values[index["rc_effective_gv"]]),
            rc_upper_gv=float(values[index["rc_upper_gv"]]),
        ))
    if not rows:
        raise ValueError("no shell rows parsed from %s" % path)
    return rows


def add_aacgm_lat_mlt(rows: Sequence[object], epoch: datetime, altitude_km: float) -> None:
    import aacgmv2  # imported lazily; required for the scientific postprocessing
    naive = epoch.astimezone(timezone.utc).replace(tzinfo=None)
    for row in rows:
        try:
            # One geographic->AACGM conversion yields both the invariant latitude
            # and the AACGM longitude; MLT is then derived from the AACGM longitude.
            alat, alon, _ = aacgmv2.convert_latlon(row.latitude_deg, row.longitude_deg,
                                                   altitude_km, naive, method_code="G2A")
            if not (math.isfinite(alat) and math.isfinite(alon)):
                row.aacgm_latitude_deg = None
                row.mlt_hour = None
                continue
            row.aacgm_latitude_deg = float(alat)
            row.mlt_hour = float(aacgmv2.convert_mlt([alon], naive, m2a=False)[0]) % 24.0
        except Exception:
            row.aacgm_latitude_deg = None
            row.mlt_hour = None



# ---------------------------------------------------------------------------
# Exact-rigidity access-state parser and common ACCESS_T50 reduction
# ---------------------------------------------------------------------------
def parse_tecplot_shell_access(path: Path) -> List[AccessRow]:
    """Read a FULL_SCAN companion or DIRECT_ACCESS Tecplot shell product."""
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
                numeric_rows.append((line_number, [float(token) for token in text.split()]))
            except ValueError as exc:
                raise ValueError("%s line %d is not numeric: %s" %
                                 (path, line_number, text)) from exc
    required = {"lon_deg", "lat_deg", "rigidity_gv", "access_state",
                "allowed", "unresolved"}
    if not variables:
        raise ValueError("Tecplot VARIABLES record not found in %s" % path)
    missing = sorted(required - set(variables))
    if missing:
        raise ValueError("%s missing access variable(s): %s" %
                         (path, ", ".join(missing)))
    index = {name: variables.index(name) for name in required}
    rows: List[AccessRow] = []
    for line_number, values in numeric_rows:
        if len(values) != len(variables):
            raise ValueError("%s line %d has %d columns; VARIABLES defines %d" %
                             (path, line_number, len(values), len(variables)))
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
            access_state=state, allowed=allowed, unresolved=unresolved,
        ))
    if not rows:
        raise ValueError("no access rows parsed from %s" % path)
    return rows


def select_common_access_band(rows: Sequence[AccessRow], minimum_deg: float,
                              maximum_deg: float) -> List[AccessRow]:
    """Restrict both products to the geodetic band traced by DIRECT_ACCESS."""
    tolerance = 1.0e-10
    selected = [row for row in rows
                if minimum_deg - tolerance <= abs(row.latitude_deg)
                <= maximum_deg + tolerance]
    if not selected:
        raise ValueError("no access rows remain in %.6g-%.6g deg geodetic band" %
                         (minimum_deg, maximum_deg))
    return selected


def _weighted_isotonic_non_decreasing(values: Sequence[float],
                                       weights: Sequence[float]) -> List[float]:
    """Weighted pool-adjacent-violators regression used by both C9 and C10."""
    if len(values) != len(weights):
        raise ValueError("isotonic values/weights length mismatch")
    blocks: List[List[float]] = []
    for index, (value, weight) in enumerate(zip(values, weights)):
        if weight <= 0.0:
            raise ValueError("isotonic weights must be positive")
        blocks.append([float(index), float(index), float(weight), float(weight * value)])
        while len(blocks) >= 2:
            left, right = blocks[-2], blocks[-1]
            if left[3] / left[2] <= right[3] / right[2] + 1.0e-14:
                break
            blocks[-2:] = [[left[0], right[1], left[2] + right[2], left[3] + right[3]]]
    fitted = [0.0] * len(values)
    for start, end, weight, weighted_sum in blocks:
        mean = weighted_sum / weight
        for index in range(int(start), int(end) + 1):
            fitted[index] = mean
    return fitted


def _access_profile_state(profile: Sequence[AccessRow], latitude_deg: float) -> Optional[float]:
    """Interpolate a resolved 0/1 longitude profile without crossing state 2."""
    usable = sorted((row for row in profile if row.aacgm_latitude_deg is not None),
                    key=lambda row: abs(float(row.aacgm_latitude_deg)))
    if not usable:
        return None
    coordinates = [abs(float(row.aacgm_latitude_deg)) for row in usable]
    for coordinate, row in zip(coordinates, usable):
        if abs(coordinate - latitude_deg) <= 1.0e-10:
            return None if row.access_state == 2 else float(row.access_state)
    if latitude_deg < coordinates[0] or latitude_deg > coordinates[-1]:
        return None
    for lower, upper, x1, x2 in zip(usable, usable[1:], coordinates, coordinates[1:]):
        if not (x1 < latitude_deg < x2):
            continue
        if lower.access_state == 2 or upper.access_state == 2:
            return None
        if abs(x2 - x1) < 1.0e-14:
            return 0.5 * (lower.access_state + upper.access_state)
        fraction = (latitude_deg - x1) / (x2 - x1)
        return ((1.0 - fraction) * lower.access_state + fraction * upper.access_state)
    return None


def _half_crossing(latitude: Sequence[float], transmission: Sequence[float],
                   step_deg: float) -> Optional[float]:
    """Return a physically contiguous T=0.5 crossing.

    Weighted isotonic regression can create a finite half-transmission plateau.
    The boundary is the center of the first contiguous bracketed plateau rather
    than its equatorward edge.  Gaps larger than 1.5 grid steps are never
    bridged, because doing so would convert a coverage failure into an
    apparently precise boundary.
    """
    if len(latitude) != len(transmission) or not latitude:
        return None

    def contiguous(first: int, second: int) -> bool:
        return latitude[second] - latitude[first] <= 1.5 * step_deg + 1.0e-12

    exact = [index for index, value in enumerate(transmission)
             if abs(value - 0.5) <= 1.0e-12]
    if exact:
        groups: List[List[int]] = [[exact[0]]]
        for index in exact[1:]:
            if index == groups[-1][-1] + 1 and contiguous(groups[-1][-1], index):
                groups[-1].append(index)
            else:
                groups.append([index])
        for group in groups:
            left, right = group[0], group[-1]
            has_forbidden = left == 0 or transmission[left - 1] <= 0.5
            has_allowed = (right == len(transmission) - 1
                           or transmission[right + 1] >= 0.5)
            if has_forbidden and has_allowed:
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


def _circular_mean_mlt(values: Sequence[float]) -> Optional[float]:
    if not values:
        return None
    angles = [2.0 * math.pi * (value % 24.0) / 24.0 for value in values]
    x = statistics.fmean(math.cos(value) for value in angles)
    y = statistics.fmean(math.sin(value) for value in angles)
    if abs(x) + abs(y) < 1.0e-14:
        return None
    return (math.atan2(y, x) * 24.0 / (2.0 * math.pi)) % 24.0


def _access_t50_for_mlt(rows: Sequence[AccessRow], rigidity_gv: float,
                        hemisphere: str, mlt_center: float, n_mlt_bins: int,
                        latitude_step_deg: float, min_resolved_fraction: float,
                        minimum_edge_margin_deg: float
                        ) -> Tuple[Optional[float], Dict[str, object], List[Dict[str, object]]]:
    """Calculate one hemisphere/MLT-sector ACCESS_T50 boundary."""
    sign = 1 if hemisphere == "N" else -1
    selected = [row for row in rows
                if math.isclose(row.rigidity_gv, rigidity_gv, rel_tol=0.0, abs_tol=5.0e-9)
                and row.aacgm_latitude_deg is not None and row.mlt_hour is not None
                and (1 if float(row.aacgm_latitude_deg) >= 0.0 else -1) == sign]
    by_longitude: Dict[float, List[AccessRow]] = {}
    for row in selected:
        by_longitude.setdefault(round(row.longitude_deg, 8), []).append(row)
    profiles: Dict[float, List[AccessRow]] = {}
    for longitude, profile in by_longitude.items():
        representative_mlt = _circular_mean_mlt(
            [float(row.mlt_hour) for row in profile if row.mlt_hour is not None])
        if representative_mlt is not None and mlt_bin_center(representative_mlt, n_mlt_bins) == round(mlt_center, 3):
            profiles[longitude] = profile
    if not profiles:
        return None, {"n_longitude_profiles": 0, "t50_bracketed": False}, []

    coordinates = [abs(float(row.aacgm_latitude_deg))
                   for profile in profiles.values() for row in profile
                   if row.aacgm_latitude_deg is not None]
    latitude_min = max(20.0, math.floor(min(coordinates) / latitude_step_deg) * latitude_step_deg)
    latitude_max = min(89.0, math.ceil(max(coordinates) / latitude_step_deg) * latitude_step_deg)
    n_grid = int(round((latitude_max - latitude_min) / latitude_step_deg)) + 1
    valid_latitudes: List[float] = []
    raw_transmission: List[float] = []
    weights: List[float] = []
    resolved_fractions: List[float] = []
    profile_rows: List[Dict[str, object]] = []
    for index in range(max(0, n_grid)):
        latitude = latitude_min + index * latitude_step_deg
        samples = [_access_profile_state(profile, latitude) for profile in profiles.values()]
        resolved = [value for value in samples if value is not None]
        fraction = len(resolved) / float(len(profiles))
        if not resolved or fraction + 1.0e-14 < min_resolved_fraction:
            continue
        valid_latitudes.append(latitude)
        raw_transmission.append(statistics.fmean(resolved))
        weights.append(float(len(resolved)))
        resolved_fractions.append(fraction)
    if len(valid_latitudes) < 2:
        return None, {
            "n_longitude_profiles": len(profiles), "t50_bracketed": False,
            "minimum_resolved_fraction": min(resolved_fractions) if resolved_fractions else 0.0,
        }, profile_rows
    fitted = _weighted_isotonic_non_decreasing(raw_transmission, weights)
    bracketed = fitted[0] < 0.5 - 1.0e-12 and fitted[-1] > 0.5 + 1.0e-12
    boundary = _half_crossing(valid_latitudes, fitted, latitude_step_deg) if bracketed else None
    edge_margin = None
    if boundary is not None:
        edge_margin = min(boundary - valid_latitudes[0], valid_latitudes[-1] - boundary)
        if edge_margin + 1.0e-12 < minimum_edge_margin_deg:
            boundary = None
    unresolved = sum(1 for row in selected if row.access_state == 2)
    for latitude, raw, fit, weight, fraction in zip(
            valid_latitudes, raw_transmission, fitted, weights, resolved_fractions):
        profile_rows.append({
            "rigidity_gv": rigidity_gv, "hemisphere": hemisphere,
            "mlt_hour": round(mlt_center, 3), "abs_aacgm_latitude_deg": latitude,
            "raw_transmission": raw, "isotonic_transmission": fit,
            "n_resolved_longitudes": int(weight),
            "n_total_longitudes": len(profiles),
            "resolved_longitude_fraction": fraction,
        })
    diagnostics = {
        "n_longitude_profiles": len(profiles), "t50_bracketed": bracketed,
        "minimum_resolved_fraction": min(resolved_fractions),
        "mean_resolved_fraction": statistics.fmean(resolved_fractions),
        "unresolved_access_fraction": unresolved / float(len(selected)) if selected else 1.0,
        "boundary_edge_margin_deg": edge_margin,
    }
    return boundary, diagnostics, profile_rows


def estimate_access_t50_boundaries(rows: Sequence[AccessRow],
                                   rigidities: Sequence[float],
                                   mlt_bins: Sequence[float],
                                   hemispheres: Sequence[str], n_mlt_bins: int,
                                   latitude_step_deg: float,
                                   min_resolved_fraction: float,
                                   minimum_edge_margin_deg: float
                                   ) -> Tuple[List[BoundaryEstimate], List[Dict[str, object]]]:
    """Build the common C10 ACCESS_T50 product for either trajectory method."""
    estimates: List[BoundaryEstimate] = []
    all_profiles: List[Dict[str, object]] = []
    for rigidity in rigidities:
        for hemisphere in hemispheres:
            boundary_by_mlt: Dict[float, Optional[float]] = {}
            for mlt in mlt_bins:
                boundary, diagnostics, profiles = _access_t50_for_mlt(
                    rows, rigidity, hemisphere, mlt, n_mlt_bins,
                    latitude_step_deg, min_resolved_fraction,
                    minimum_edge_margin_deg)
                boundary_by_mlt[round(mlt, 3)] = boundary
                for row in profiles:
                    row.update(diagnostics)
                all_profiles.extend(profiles)
            valid = [value for value in boundary_by_mlt.values() if value is not None]
            estimates.append(BoundaryEstimate(
                rigidity_gv=rigidity, hemisphere=hemisphere,
                boundary_by_mlt=boundary_by_mlt, n_valid_mlt=len(valid),
                n_requested_mlt=len(mlt_bins),
                ellipse=fit_ellipse(list(boundary_by_mlt), list(boundary_by_mlt.values())),
                observable="ACCESS_T50",
            ))
    return estimates, all_profiles


def _access_key(row: AccessRow) -> Tuple[float, float, float]:
    return (round(row.longitude_deg % 360.0, 8), round(row.latitude_deg, 8),
            round(row.rigidity_gv, 10))


def compare_access_states(current: Sequence[AccessRow], counterpart: Sequence[AccessRow],
                          minimum_agreement: float, maximum_unresolved_fraction: float
                          ) -> Tuple[Dict[str, object], List[Dict[str, object]]]:
    """Compare matching FULL_SCAN and DIRECT_ACCESS raw classifications."""
    a = {_access_key(row): row for row in current}
    b = {_access_key(row): row for row in counterpart}
    common = sorted(set(a).intersection(b))
    details: List[Dict[str, object]] = []
    resolved = mismatch = unresolved = 0
    for key in common:
        first, second = a[key], b[key]
        if first.access_state == 2 or second.access_state == 2:
            unresolved += 1
        else:
            resolved += 1
            if first.access_state != second.access_state:
                mismatch += 1
                details.append({
                    "difference": "resolved_state_mismatch", "longitude_deg": key[0],
                    "latitude_deg": key[1], "rigidity_gv": key[2],
                    "current_state": first.access_state,
                    "counterpart_state": second.access_state,
                })
    for key in sorted(set(a) - set(b)):
        details.append({"difference": "missing_from_counterpart", "longitude_deg": key[0],
                        "latitude_deg": key[1], "rigidity_gv": key[2]})
    for key in sorted(set(b) - set(a)):
        details.append({"difference": "missing_from_current", "longitude_deg": key[0],
                        "latitude_deg": key[1], "rigidity_gv": key[2]})
    agreement = 1.0 - mismatch / float(resolved) if resolved else 0.0
    unresolved_fraction = unresolved / float(len(common)) if common else 1.0
    passed = (set(a) == set(b) and resolved > 0
              and agreement >= minimum_agreement
              and unresolved_fraction <= maximum_unresolved_fraction)
    return {
        "n_current_rows": len(a), "n_counterpart_rows": len(b),
        "n_common_rows": len(common), "n_resolved_common": resolved,
        "n_resolved_mismatch": mismatch,
        "resolved_access_state_agreement_fraction": agreement,
        "n_unresolved_in_either_product": unresolved,
        "unresolved_in_either_product_fraction": unresolved_fraction,
        "n_missing_from_current": len(set(b) - set(a)),
        "n_missing_from_counterpart": len(set(a) - set(b)),
        "minimum_required_resolved_agreement": minimum_agreement,
        "maximum_allowed_unresolved_fraction": maximum_unresolved_fraction,
        "passed": passed,
    }, details


# ---------------------------------------------------------------------------
# Boundary extraction + ellipse fit
# ---------------------------------------------------------------------------
def _poleward_crossing(profile: Sequence[ShellRow], rigidity_gv: float,
                       rc_attribute: str) -> Optional[float]:
    usable = [row for row in profile
              if row.aacgm_latitude_deg is not None
              and getattr(row, rc_attribute) > 0.0
              and 20.0 <= abs(row.aacgm_latitude_deg) <= 89.0]
    usable.sort(key=lambda row: abs(float(row.aacgm_latitude_deg)))
    for lower, upper in zip(usable, usable[1:]):
        x1 = abs(float(lower.aacgm_latitude_deg))
        x2 = abs(float(upper.aacgm_latitude_deg))
        y1 = getattr(lower, rc_attribute) - rigidity_gv
        y2 = getattr(upper, rc_attribute) - rigidity_gv
        # Moving poleward Rc decreases; boundary is Rc > R -> Rc <= R.
        if y1 > 0.0 and y2 <= 0.0:
            if abs(y2 - y1) < 1.0e-14:
                return 0.5 * (x1 + x2)
            return x1 + (0.0 - y1) * (x2 - x1) / (y2 - y1)
    return None


def fit_ellipse(mlt_hours: Sequence[float], boundary_lats: Sequence[Optional[float]]) -> EllipseFit:
    pts = [(m, lat) for m, lat in zip(mlt_hours, boundary_lats) if lat is not None]
    if len(pts) < 3:
        return EllipseFit(None, None, None, None)
    colat = [90.0 - lat for _, lat in pts]
    phi = [2.0 * math.pi * m / 24.0 for m, _ in pts]
    n = len(pts)
    # Least-squares: colat = c0 + a*cos(phi) + b*sin(phi)
    Scos = sum(math.cos(p) for p in phi)
    Ssin = sum(math.sin(p) for p in phi)
    Scc = sum(math.cos(p) ** 2 for p in phi)
    Sss = sum(math.sin(p) ** 2 for p in phi)
    Scs = sum(math.cos(p) * math.sin(p) for p in phi)
    Sy = sum(colat)
    Syc = sum(y * math.cos(p) for y, p in zip(colat, phi))
    Sys = sum(y * math.sin(p) for y, p in zip(colat, phi))
    A = [[float(n), Scos, Ssin], [Scos, Scc, Scs], [Ssin, Scs, Sss]]
    rhs = [Sy, Syc, Sys]
    sol = _solve3(A, rhs)
    if sol is None:
        center = statistics.fmean(colat)
        return EllipseFit(center, None, None, None)
    c0, a, b = sol
    amplitude = math.hypot(a, b)
    phase = math.atan2(b, a)
    phase_mlt = (phase * 24.0 / (2.0 * math.pi)) % 24.0
    residuals = [y - (c0 + a * math.cos(p) + b * math.sin(p)) for y, p in zip(colat, phi)]
    rms = math.sqrt(statistics.fmean(r * r for r in residuals))
    return EllipseFit(center_colat_deg=c0, amplitude_deg=amplitude,
                      phase_mlt_hour=phase_mlt, rms_residual_deg=rms)


def _solve3(A: List[List[float]], b: List[float]) -> Optional[Tuple[float, float, float]]:
    m = [row[:] + [b[i]] for i, row in enumerate(A)]
    for col in range(3):
        pivot = max(range(col, 3), key=lambda r: abs(m[r][col]))
        if abs(m[pivot][col]) < 1.0e-12:
            return None
        m[col], m[pivot] = m[pivot], m[col]
        piv = m[col][col]
        m[col] = [v / piv for v in m[col]]
        for r in range(3):
            if r != col:
                factor = m[r][col]
                m[r] = [v - factor * mc for v, mc in zip(m[r], m[col])]
    return m[0][3], m[1][3], m[2][3]


def estimate_boundaries(rows: Sequence[ShellRow], rigidities: Sequence[float],
                        mlt_bins: Sequence[float], hemispheres: Sequence[str],
                        rc_attribute: str, n_mlt_bins: int) -> List[BoundaryEstimate]:
    estimates: List[BoundaryEstimate] = []
    for rigidity in rigidities:
        for hemisphere in hemispheres:
            sign = 1.0 if hemisphere == "N" else -1.0
            boundary_by_mlt: Dict[float, Optional[float]] = {}
            for mlt_center in mlt_bins:
                profile = [row for row in rows
                           if row.aacgm_latitude_deg is not None and row.mlt_hour is not None
                           and (1.0 if row.aacgm_latitude_deg >= 0.0 else -1.0) == sign
                           and mlt_bin_center(row.mlt_hour, n_mlt_bins) == round(mlt_center, 3)]
                crossing = _poleward_crossing(profile, rigidity, rc_attribute) if profile else None
                boundary_by_mlt[round(mlt_center, 3)] = crossing
            valid = [v for v in boundary_by_mlt.values() if v is not None]
            ellipse = fit_ellipse(list(boundary_by_mlt.keys()), list(boundary_by_mlt.values()))
            estimates.append(BoundaryEstimate(
                rigidity_gv=rigidity, hemisphere=hemisphere,
                boundary_by_mlt=boundary_by_mlt, n_valid_mlt=len(valid),
                n_requested_mlt=len(mlt_bins), ellipse=ellipse,
                observable=rc_attribute.replace("_gv", "").upper()))
    return estimates


# ---------------------------------------------------------------------------
# Comparison + metrics
# ---------------------------------------------------------------------------
def pearson(x: Sequence[float], y: Sequence[float]) -> Optional[float]:
    if len(x) < 2:
        return None
    mx, my = statistics.fmean(x), statistics.fmean(y)
    sxx = sum((a - mx) ** 2 for a in x)
    syy = sum((b - my) ** 2 for b in y)
    sxy = sum((a - mx) * (b - my) for a, b in zip(x, y))
    if sxx <= 0.0 or syy <= 0.0:
        return None
    return sxy / math.sqrt(sxx * syy)


def _residual_summary(reference: Sequence[float], model: Sequence[float]) -> Dict[str, object]:
    residuals = [m - r for r, m in zip(reference, model)]
    if not residuals:
        return {
            "n_reference": len(reference), "n_valid": 0, "bias_deg": None,
            "mae_deg": None, "rmse_deg": None, "max_abs_error_deg": None,
            "correlation": None,
        }
    return {
        "n_reference": len(reference),
        "n_valid": len(residuals),
        "bias_deg": statistics.fmean(residuals),
        "mae_deg": statistics.fmean(abs(value) for value in residuals),
        "rmse_deg": math.sqrt(statistics.fmean(value * value for value in residuals)),
        "max_abs_error_deg": max(abs(value) for value in residuals),
        "correlation": pearson(reference, model),
    }


def compare(reference_rows: Sequence[ReferenceRow],
            interval_models: Mapping[datetime, Mapping[Tuple[float, str], BoundaryEstimate]],
            driver_info: DriverInfo, args: argparse.Namespace,
            reference_is_model: bool,
            driver_symh: Sequence[Tuple[datetime, float]] = ()) -> Tuple[List[Dict[str, object]], Metrics]:
    """Compare paired cells and keep robust diagnostics separate from sparse rows."""

    detailed: List[Dict[str, object]] = []
    acceptance_ref: List[float] = []
    acceptance_model: List[float] = []
    robust_all_ref: List[float] = []
    robust_all_model: List[float] = []
    unfiltered_ref: List[float] = []
    unfiltered_model: List[float] = []
    channel_all: Dict[str, Tuple[List[float], List[float]]] = {}
    channel_robust: Dict[str, Tuple[List[float], List[float]]] = {}

    rigidities, mlt_bins, hemispheres = reference_axes(reference_rows)
    n_mlt = len(mlt_bins)

    for ref in sorted(reference_rows, key=lambda r: (r.midpoint, r.rigidity_gv, r.hemisphere, r.mlt_hour)):
        if ref.midpoint not in interval_models:
            continue
        est = interval_models[ref.midpoint].get((round(ref.rigidity_gv, 9), ref.hemisphere))
        modeled = None
        if est is not None:
            modeled = est.boundary_by_mlt.get(round(mlt_bin_center(ref.mlt_hour, n_mlt), 3))
        residual = None
        usable = modeled is not None and ref.boundary_lat_deg is not None and not ref.missing
        used_for_acceptance = bool(
            usable and ref.validation_role == "PRIMARY" and ref.acceptance_eligible)
        used_for_diagnostic_summary = bool(
            usable and (
                (ref.validation_role == "PRIMARY" and ref.acceptance_eligible)
                or (ref.validation_role == "DIAGNOSTIC" and ref.diagnostic_eligible)
            )
        )
        if usable:
            observed_value = float(ref.boundary_lat_deg)
            modeled_value = float(modeled)
            residual = modeled_value - observed_value
            unfiltered_ref.append(observed_value)
            unfiltered_model.append(modeled_value)
            pair = channel_all.setdefault(ref.channel, ([], []))
            pair[0].append(observed_value)
            pair[1].append(modeled_value)
            if used_for_diagnostic_summary:
                robust_all_ref.append(observed_value)
                robust_all_model.append(modeled_value)
                robust_pair = channel_robust.setdefault(ref.channel, ([], []))
                robust_pair[0].append(observed_value)
                robust_pair[1].append(modeled_value)
            if used_for_acceptance:
                acceptance_ref.append(observed_value)
                acceptance_model.append(modeled_value)

        sym_h = driver_scalar_at(driver_symh, ref.midpoint) if driver_symh else ref.sym_h_nt
        detailed.append({
            "interval_midpoint_utc": format_utc(ref.midpoint),
            "rigidity_gv": ref.rigidity_gv,
            "channel": ref.channel,
            "validation_role": ref.validation_role,
            "acceptance_eligible": ref.acceptance_eligible,
            "diagnostic_eligible": ref.diagnostic_eligible,
            "quality_status": ref.quality_status,
            "used_for_acceptance": used_for_acceptance,
            "used_for_diagnostic_summary": used_for_diagnostic_summary,
            "background_corrected": ref.background_corrected,
            "hemisphere": ref.hemisphere,
            "mlt_hour": ref.mlt_hour,
            "sym_h_nt": sym_h,
            "observed_boundary_aacgm_deg": ref.boundary_lat_deg,
            "modeled_boundary_aacgm_deg": modeled,
            "residual_deg": residual,
            "sigma_deg": ref.sigma_deg,
            "median_transition_width_deg": ref.median_transition_width_deg,
            "median_transition_support_samples": ref.median_transition_support_samples,
            "median_contrast_to_noise_ratio": ref.median_contrast_to_noise_ratio,
            "n_observed_crossings": ref.n_crossings,
            "n_aggregate_eligible_crossings": ref.n_aggregate_eligible_crossings,
            "n_cross_channel_outliers": ref.n_cross_channel_outliers,
            "n_distinct_pass_legs": ref.n_distinct_pass_legs,
            "n_distinct_satellites": ref.n_distinct_satellites,
            "observing_satellites": ref.satellites,
        })

    acceptance_summary = _residual_summary(acceptance_ref, acceptance_model)
    robust_all_summary = _residual_summary(robust_all_ref, robust_all_model)
    unfiltered_summary = _residual_summary(unfiltered_ref, unfiltered_model)
    per_channel: Dict[str, Dict[str, object]] = {}
    for channel in sorted(set(channel_all).union(channel_robust)):
        all_refs, all_models = channel_all.get(channel, ([], []))
        robust_refs, robust_models = channel_robust.get(channel, ([], []))
        robust_summary = _residual_summary(robust_refs, robust_models)
        all_summary = _residual_summary(all_refs, all_models)
        robust_summary.update({
            "validation_role": "PRIMARY" if channel in ("P6", "P7") else "DIAGNOSTIC",
            "n_unfiltered_valid": all_summary["n_valid"],
            "unfiltered_bias_deg": all_summary["bias_deg"],
            "unfiltered_rmse_deg": all_summary["rmse_deg"],
            "n_excluded_from_robust_summary": int(all_summary["n_valid"]) - int(robust_summary["n_valid"]),
        })
        per_channel[channel] = robust_summary

    n_reference = sum(
        1 for r in reference_rows
        if r.validation_role == "PRIMARY" and r.acceptance_eligible
        and not r.missing and r.boundary_lat_deg is not None
    )
    n_valid = int(acceptance_summary["n_valid"])
    valid_fraction = n_valid / n_reference if n_reference else 0.0
    bias = (float(acceptance_summary["bias_deg"])
            if acceptance_summary["bias_deg"] is not None else float("inf"))
    mae = (float(acceptance_summary["mae_deg"])
           if acceptance_summary["mae_deg"] is not None else float("inf"))
    rmse = (float(acceptance_summary["rmse_deg"])
            if acceptance_summary["rmse_deg"] is not None else float("inf"))
    max_abs = (float(acceptance_summary["max_abs_error_deg"])
               if acceptance_summary["max_abs_error_deg"] is not None else float("inf"))
    correlation = acceptance_summary["correlation"]

    acceptance_rows = [
        r for r in reference_rows
        if r.validation_role == "PRIMARY" and r.acceptance_eligible
    ]
    primary_rigidities = sorted({r.rigidity_gv for r in acceptance_rows})
    low_rigidity = min(primary_rigidities) if primary_rigidities else min(rigidities)
    obs_expansion, obs_time = _expansion_series(acceptance_rows, low_rigidity)
    acceptance_midpoints = {row.midpoint for row in acceptance_rows}
    acceptance_models = {
        midpoint: model for midpoint, model in interval_models.items()
        if midpoint in acceptance_midpoints
    }
    mod_expansion, mod_time = _model_expansion_series(
        acceptance_models, low_rigidity, hemispheres
    )
    time_error = None
    if obs_time is not None and mod_time is not None:
        time_error = abs((mod_time - obs_time).total_seconds()) / 60.0

    obs_asym = _mean_reference_asymmetry(acceptance_rows, mlt_bins)
    mod_asym = _mean_model_asymmetry(acceptance_models)
    reference_processing_eligible = bool(acceptance_rows) and all(
        row.background_corrected and row.n_distinct_pass_legs >= 2
        for row in acceptance_rows if not row.missing
    )

    numerical_pass = (
        valid_fraction >= args.min_valid_fraction
        and rmse <= args.max_rmse_deg
        and abs(bias) <= args.max_abs_bias_deg
        and (correlation is not None and float(correlation) >= args.min_correlation)
        and mod_expansion is not None
        and args.min_expansion_deg <= mod_expansion <= args.max_expansion_deg
        and time_error is not None and time_error <= args.max_expansion_time_error_minutes
    )
    scientific_eligible = (
        driver_info.verified_driver and not reference_is_model and reference_processing_eligible
    )
    passed = numerical_pass and (scientific_eligible or args.allow_model_reference)

    metrics = Metrics(
        n_reference=n_reference, n_valid_model=n_valid, valid_fraction=valid_fraction,
        mean_bias_deg=bias, mean_abs_error_deg=mae, rmse_deg=rmse, max_abs_error_deg=max_abs,
        correlation=None if correlation is None else float(correlation),
        observed_expansion_deg=obs_expansion, modeled_expansion_deg=mod_expansion,
        observed_max_expansion_time_utc=format_utc(obs_time) if obs_time else None,
        modeled_max_expansion_time_utc=format_utc(mod_time) if mod_time else None,
        max_expansion_time_error_minutes=time_error,
        observed_mean_asymmetry_deg=obs_asym, modeled_mean_asymmetry_deg=mod_asym,
        driver_verified=driver_info.verified_driver,
        scientific_validation_eligible=scientific_eligible,
        passed_numerical_comparison=numerical_pass, passed=passed,
        n_all_reference=len(robust_all_ref),
        n_all_valid_model=int(robust_all_summary["n_valid"]),
        all_mean_bias_deg=(None if robust_all_summary["bias_deg"] is None
                           else float(robust_all_summary["bias_deg"])),
        all_rmse_deg=(None if robust_all_summary["rmse_deg"] is None
                      else float(robust_all_summary["rmse_deg"])),
        unfiltered_all_mean_bias_deg=(
            None if unfiltered_summary["bias_deg"] is None
            else float(unfiltered_summary["bias_deg"])),
        unfiltered_all_rmse_deg=(
            None if unfiltered_summary["rmse_deg"] is None
            else float(unfiltered_summary["rmse_deg"])),
        reference_processing_eligible=reference_processing_eligible,
        per_channel_metrics=per_channel,
    )
    return detailed, metrics


def _expansion_series(
    rows: Sequence[ReferenceRow],
    rigidity: float,
) -> Tuple[Optional[float], Optional[datetime]]:
    """Return maximum observed expansion relative to the designated first epoch.

    The baseline is never silently moved forward to the first *available* low-
    rigidity cell.  If the selected profile's first midpoint has no measured
    low-rigidity boundary, the expansion diagnostic is unavailable and the
    numerical gate fails explicitly.  This avoids the same baseline-substitution
    failure mode identified while developing C9.
    """

    selected_midpoints = sorted({row.midpoint for row in rows})
    if not selected_midpoints:
        return None, None
    baseline_midpoint = selected_midpoints[0]

    by_midpoint: Dict[datetime, List[float]] = {}
    for row in rows:
        if (math.isclose(row.rigidity_gv, rigidity)
                and not row.missing
                and row.boundary_lat_deg is not None):
            by_midpoint.setdefault(row.midpoint, []).append(90.0 - row.boundary_lat_deg)
    if baseline_midpoint not in by_midpoint:
        return None, None

    quiet_center = statistics.fmean(by_midpoint[baseline_midpoint])
    best_time: Optional[datetime] = None
    best_expansion: Optional[float] = None
    for midpoint in selected_midpoints:
        colatitudes = by_midpoint.get(midpoint)
        if not colatitudes:
            continue
        expansion = statistics.fmean(colatitudes) - quiet_center
        if best_expansion is None or expansion > best_expansion:
            best_expansion = expansion
            best_time = midpoint
    return best_expansion, best_time


def _model_expansion_series(
    interval_models: Mapping[datetime, Mapping[Tuple[float, str], BoundaryEstimate]],
    rigidity: float,
    hemispheres: Sequence[str],
) -> Tuple[Optional[float], Optional[datetime]]:
    """Return maximum modeled expansion using the same fixed first epoch."""

    selected_midpoints = sorted(interval_models)
    if not selected_midpoints:
        return None, None
    baseline_midpoint = selected_midpoints[0]

    centers: Dict[datetime, List[float]] = {}
    for midpoint, model in interval_models.items():
        values: List[float] = []
        for hemisphere in hemispheres:
            estimate = model.get((round(rigidity, 9), hemisphere))
            if estimate and estimate.ellipse.center_colat_deg is not None:
                values.append(estimate.ellipse.center_colat_deg)
        if values:
            centers[midpoint] = values
    if baseline_midpoint not in centers:
        return None, None

    quiet_center = statistics.fmean(centers[baseline_midpoint])
    best_time: Optional[datetime] = None
    best_expansion: Optional[float] = None
    for midpoint in selected_midpoints:
        values = centers.get(midpoint)
        if not values:
            continue
        expansion = statistics.fmean(values) - quiet_center
        if best_expansion is None or expansion > best_expansion:
            best_expansion = expansion
            best_time = midpoint
    return best_expansion, best_time


def _mean_reference_asymmetry(rows: Sequence[ReferenceRow],
                              mlt_bins: Sequence[float]) -> Optional[float]:
    amps: List[float] = []
    keyed: Dict[Tuple[datetime, float, str], Dict[float, float]] = {}
    for r in rows:
        if r.missing or r.boundary_lat_deg is None:
            continue
        keyed.setdefault((r.midpoint, round(r.rigidity_gv, 9), r.hemisphere), {})[round(r.mlt_hour, 3)] = r.boundary_lat_deg
    for series in keyed.values():
        if len(series) >= 3:
            fit = fit_ellipse(list(series.keys()), list(series.values()))
            if fit.amplitude_deg is not None:
                amps.append(fit.amplitude_deg)
    return statistics.fmean(amps) if amps else None


def _mean_model_asymmetry(interval_models) -> Optional[float]:
    amps: List[float] = []
    for model in interval_models.values():
        for est in model.values():
            if est.ellipse.amplitude_deg is not None:
                amps.append(est.ellipse.amplitude_deg)
    return statistics.fmean(amps) if amps else None


# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------
def _plot_time_value(text: str) -> datetime:
    """Convert a comparison-table timestamp to a matplotlib-compatible datetime."""

    return parse_utc(text)


def _paired_rows(rows: Sequence[Mapping[str, object]]) -> List[Mapping[str, object]]:
    """Return rows containing both observed and modeled values."""

    return [row for row in rows
            if row.get("observed_boundary_aacgm_deg") is not None
            and row.get("modeled_boundary_aacgm_deg") is not None]


def _diagnostic_robust_rows(rows: Sequence[Mapping[str, object]]) -> List[Mapping[str, object]]:
    return [row for row in _paired_rows(rows) if bool(row.get("diagnostic_eligible", False))]


def _mean_and_uncertainty(rows: Sequence[Mapping[str, object]]) -> Tuple[float, float, float]:
    observed = [float(row["observed_boundary_aacgm_deg"]) for row in rows]
    modeled = [float(row["modeled_boundary_aacgm_deg"]) for row in rows]
    sigmas = [float(row["sigma_deg"]) for row in rows if row.get("sigma_deg") is not None]
    sigma_mean = (math.sqrt(sum(value * value for value in sigmas)) / len(sigmas)
                  if sigmas else 0.0)
    return statistics.fmean(observed), statistics.fmean(modeled), sigma_mean


def _plot_rows_by_scope(detailed: Sequence[Mapping[str, object]],
                        include_diagnostics: bool) -> List[Mapping[str, object]]:
    """Select either primary-only rows or the complete primary+diagnostic set."""

    if include_diagnostics:
        return list(detailed)
    return [row for row in detailed
            if str(row.get("validation_role", "")).strip().upper() != "DIAGNOSTIC"]


def _plot_scope_label(include_diagnostics: bool) -> str:
    return "including diagnostic channels" if include_diagnostics else "primary channels only"


def _save_plot_formats(figure, output_base: Path, formats: Sequence[str],
                        log_prefix: str, dpi: int = 150) -> List[Path]:
    """Save one Matplotlib figure to every requested format.

    ``output_base`` carries no suffix; one sibling file is written per entry in
    ``formats`` (``output_base.with_suffix(".png")``, ``".eps"``, ...) from the
    same figure, so the raster and vector copies never drift apart. The return
    value lists only the files actually written; callers must not treat an
    empty result as an error since a single failed format does not stop the
    others.

    EPS/PS have no alpha channel, so the translucent markers and error bars
    used across the C10 plots fall back to opaque fills on that one format.
    Matplotlib reports this through its logger rather than warnings.warn, so
    it is muted here rather than with a warnings filter; it is expected on
    every EPS write and not a sign anything went wrong.
    """
    ps_logger = logging.getLogger("matplotlib.backends.backend_ps")
    previous_level = ps_logger.level
    written: List[Path] = []
    try:
        ps_logger.setLevel(logging.ERROR)
        for fmt in formats:
            destination = output_base.with_suffix("." + fmt.lower().lstrip("."))
            try:
                figure.savefig(destination, dpi=dpi)
            except Exception as exc:
                print("%s: failed to write %s: %s" % (log_prefix, destination, exc),
                      file=sys.stderr)
                continue
            written.append(destination)
    finally:
        ps_logger.setLevel(previous_level)
    return written


def make_plot(detailed: Sequence[Mapping[str, object]], output_base: Path, solver: str,
              include_diagnostics: bool = False,
              formats: Sequence[str] = ("png", "eps")) -> List[Path]:
    """Plot connected paired POES/AMPS time series for the selected channel scope.

    Every available paired mean participates in a line.  In particular, diagnostic
    values are no longer emitted as disconnected sparse markers.  Missing epochs are
    skipped, so matplotlib connects the complete sequence of available values.
    """

    if not formats:
        return []
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.dates as mdates
        import matplotlib.pyplot as plt
    except Exception as exc:  # pragma: no cover
        print("C10 plot skipped (matplotlib unavailable): %s" % exc, file=sys.stderr)
        return []

    plot_rows = _plot_rows_by_scope(detailed, include_diagnostics)
    if not plot_rows:
        print("C10 plot skipped (no rows for %s)" % _plot_scope_label(include_diagnostics),
              file=sys.stderr)
        return []

    rigidities = sorted({float(row["rigidity_gv"]) for row in plot_rows})
    times = sorted({_plot_time_value(str(row["interval_midpoint_utc"])) for row in plot_rows})
    cmap = plt.get_cmap("tab10")
    fig, (ax_top, ax_bottom) = plt.subplots(
        2, 1, figsize=(13, 8.5), sharex=True,
        gridspec_kw={"height_ratios": (2.2, 1.0)},
    )

    for index, rigidity in enumerate(rigidities):
        color = cmap(index % 10)
        role_values = {str(row.get("validation_role", "")).strip().upper()
                       for row in plot_rows
                       if math.isclose(float(row["rigidity_gv"]), rigidity)}
        diagnostic = "DIAGNOSTIC" in role_values and "PRIMARY" not in role_values
        suffix = " (diagnostic)" if diagnostic else ""
        alpha = 0.70 if diagnostic else 1.0

        line_times: List[datetime] = []
        observed_line: List[float] = []
        modeled_line: List[float] = []
        residual_line: List[float] = []
        sigma_line: List[float] = []

        for time_value in times:
            label = format_utc(time_value)
            selected = [row for row in plot_rows
                        if math.isclose(float(row["rigidity_gv"]), rigidity)
                        and row["interval_midpoint_utc"] == label]
            paired = _paired_rows(selected)
            if not paired:
                continue
            obs, mod, sigma = _mean_and_uncertainty(paired)
            line_times.append(time_value)
            observed_line.append(obs)
            modeled_line.append(mod)
            residual_line.append(mod - obs)
            sigma_line.append(sigma)

        if not line_times:
            continue
        ax_top.errorbar(
            line_times, observed_line, yerr=sigma_line, linestyle="--", marker="o",
            markersize=4, linewidth=1.2, color=color, capsize=2, alpha=alpha,
            label=f"POES {rigidity:.3f} GV{suffix}",
        )
        ax_top.plot(
            line_times, modeled_line, linestyle="-", marker="x", markersize=5,
            linewidth=1.4, color=color, alpha=alpha,
            label=f"AMPS {rigidity:.3f} GV{suffix}",
        )
        ax_bottom.plot(
            line_times, residual_line, linestyle="-", marker="o", markersize=4,
            linewidth=1.2, color=color, alpha=alpha,
            label=f"{rigidity:.3f} GV{suffix}",
        )

    ax_top.set_ylabel("Cutoff |AACGM latitude| [deg]")
    ax_top.set_title(
        "C10 %s: paired background-normalized POES/MetOp T50 versus AMPS (%s)"
        % (solver, _plot_scope_label(include_diagnostics)))
    ax_top.grid(alpha=0.3)
    if ax_top.get_legend_handles_labels()[0]:
        ax_top.legend(fontsize=8, ncol=4, loc="best")
    ax_bottom.axhline(0.0, color="black", linewidth=0.9, linestyle="--")
    ax_bottom.set_ylabel("AMPS - POES [deg]")
    ax_bottom.set_xlabel("UTC")
    ax_bottom.grid(alpha=0.3)
    if ax_bottom.get_legend_handles_labels()[0]:
        ax_bottom.legend(fontsize=8, ncol=4, loc="best")
    ax_bottom.xaxis.set_major_formatter(mdates.DateFormatter("%m-%d\n%H:%M"))
    fig.tight_layout()
    written = _save_plot_formats(fig, output_base, formats, "C10 plot")
    plt.close(fig)
    return written


def make_scatter_plot(detailed: Sequence[Mapping[str, object]], output_base: Path, solver: str,
                      include_diagnostics: bool = False,
                      formats: Sequence[str] = ("png", "eps")) -> List[Path]:
    """Plot paired observed/model cells for the selected channel scope."""

    if not formats:
        return []
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as exc:  # pragma: no cover
        print("C10 scatter plot skipped: %s" % exc, file=sys.stderr)
        return []

    plot_rows = _plot_rows_by_scope(detailed, include_diagnostics)
    if not plot_rows:
        print("C10 scatter plot skipped (no rows for %s)"
              % _plot_scope_label(include_diagnostics), file=sys.stderr)
        return []

    rigidities = sorted({float(row["rigidity_gv"]) for row in plot_rows})
    cmap = plt.get_cmap("tab10")
    fig, ax = plt.subplots(figsize=(7.5, 7.0))
    all_values: List[float] = []
    for index, rigidity in enumerate(rigidities):
        points = [row for row in _paired_rows(plot_rows)
                  if math.isclose(float(row["rigidity_gv"]), rigidity)]
        if not points:
            continue
        diagnostic = all(str(row.get("validation_role", "")).strip().upper() == "DIAGNOSTIC"
                         for row in points)
        color = cmap(index % 10)
        suffix = " (diagnostic)" if diagnostic else ""
        x = [float(row["observed_boundary_aacgm_deg"]) for row in points]
        y = [float(row["modeled_boundary_aacgm_deg"]) for row in points]
        all_values.extend(x + y)
        ax.scatter(x, y, s=24, alpha=0.60 if diagnostic else 0.80,
                   color=color, label=f"{rigidity:.3f} GV{suffix}")
    if all_values:
        lower = min(all_values) - 1.0
        upper = max(all_values) + 1.0
        ax.plot([lower, upper], [lower, upper], "k--", linewidth=1.0)
        ax.set_xlim(lower, upper)
        ax.set_ylim(lower, upper)
    ax.set_aspect("equal", adjustable="box")
    ax.set_xlabel("Observed POES boundary [deg AACGM]")
    ax.set_ylabel("AMPS boundary [deg AACGM]")
    ax.set_title("C10 %s: paired T50 comparison (%s)"
                 % (solver, _plot_scope_label(include_diagnostics)))
    ax.grid(alpha=0.3)
    if ax.get_legend_handles_labels()[0]:
        ax.legend(fontsize=8)
    fig.tight_layout()
    written = _save_plot_formats(fig, output_base, formats, "C10 scatter plot")
    plt.close(fig)
    return written


def make_mlt_plot(detailed: Sequence[Mapping[str, object]], output_base: Path, solver: str,
                  include_diagnostics: bool = False,
                  formats: Sequence[str] = ("png", "eps")) -> List[Path]:
    """Plot connected paired means by MLT for the selected channel scope."""

    if not formats:
        return []
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as exc:  # pragma: no cover
        print("C10 MLT plot skipped: %s" % exc, file=sys.stderr)
        return []

    plot_rows = _plot_rows_by_scope(detailed, include_diagnostics)
    if not plot_rows:
        print("C10 MLT plot skipped (no rows for %s)" % _plot_scope_label(include_diagnostics),
              file=sys.stderr)
        return []

    rigidities = sorted({float(row["rigidity_gv"]) for row in plot_rows})
    mlt_values = sorted({float(row["mlt_hour"]) for row in plot_rows})
    cmap = plt.get_cmap("tab10")
    fig, (ax_n, ax_s) = plt.subplots(1, 2, figsize=(13, 5.5), sharey=True)
    for axis, hemisphere in ((ax_n, "N"), (ax_s, "S")):
        for index, rigidity in enumerate(rigidities):
            role_values = {str(row.get("validation_role", "")).strip().upper()
                           for row in plot_rows
                           if math.isclose(float(row["rigidity_gv"]), rigidity)}
            diagnostic = "DIAGNOSTIC" in role_values and "PRIMARY" not in role_values
            line_mlt: List[float] = []
            obs_line: List[float] = []
            mod_line: List[float] = []
            for mlt in mlt_values:
                selected = [row for row in plot_rows
                            if row["hemisphere"] == hemisphere
                            and math.isclose(float(row["rigidity_gv"]), rigidity)
                            and math.isclose(float(row["mlt_hour"]), mlt)]
                paired = _paired_rows(selected)
                if not paired:
                    continue
                obs, mod, _ = _mean_and_uncertainty(paired)
                line_mlt.append(mlt)
                obs_line.append(obs)
                mod_line.append(mod)
            if not line_mlt:
                continue
            color = cmap(index % 10)
            suffix = " (diagnostic)" if diagnostic else ""
            alpha = 0.70 if diagnostic else 1.0
            axis.plot(line_mlt, obs_line, "--o", color=color, markersize=4, alpha=alpha,
                      label=f"POES {rigidity:.3f} GV{suffix}")
            axis.plot(line_mlt, mod_line, "-x", color=color, markersize=5, alpha=alpha,
                      label=f"AMPS {rigidity:.3f} GV{suffix}")
        axis.set_title(f"{hemisphere} hemisphere")
        axis.set_xlabel("Magnetic local time [hour]")
        axis.set_xticks(mlt_values)
        axis.grid(alpha=0.3)
    ax_n.set_ylabel("Mean cutoff |AACGM latitude| [deg]")
    if ax_s.get_legend_handles_labels()[0]:
        ax_s.legend(fontsize=7, ncol=2, loc="best")
    fig.suptitle("C10 %s: paired T50 MLT dependence (%s)"
                 % (solver, _plot_scope_label(include_diagnostics)))
    fig.tight_layout()
    written = _save_plot_formats(fig, output_base, formats, "C10 MLT plot")
    plt.close(fig)
    return written


def diagnostic_flag_rows(detailed: Sequence[Mapping[str, object]],
                         residual_threshold_deg: float = 5.0) -> List[Dict[str, object]]:
    """Return sparse/outlier diagnostic rows for focused inspection."""

    flagged: List[Dict[str, object]] = []
    for row in detailed:
        if str(row.get("validation_role", "")) != "DIAGNOSTIC":
            continue
        reasons: List[str] = []
        if not bool(row.get("diagnostic_eligible", False)):
            reasons.append(str(row.get("quality_status", "DIAGNOSTIC_NOT_ROBUST")))
        if int(row.get("n_cross_channel_outliers", 0) or 0) > 0:
            reasons.append("P9_CROSS_CHANNEL_OUTLIER")
        residual = row.get("residual_deg")
        if residual is not None and abs(float(residual)) > residual_threshold_deg:
            reasons.append("LARGE_MODEL_DATA_RESIDUAL")
        if reasons:
            item = dict(row)
            item["diagnostic_flags"] = ";".join(dict.fromkeys(reasons))
            flagged.append(item)
    return flagged
def write_dict_rows(path: Path, rows: Sequence[Mapping[str, object]]) -> None:
    if not rows:
        path.write_text("")
        return
    keys: List[str] = []
    for row in rows:
        for key in row:
            if key not in keys:
                keys.append(key)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=keys)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


# ---------------------------------------------------------------------------
# Self-test: exercise postprocessing without AMPS
# ---------------------------------------------------------------------------
def synthetic_shell(reference_rows: Sequence[ReferenceRow], midpoint: datetime,
                    args: argparse.Namespace) -> List[ShellRow]:
    """Build a shell whose Rc_lower reproduces the reference boundary exactly.

    Used by --self-test to validate the extraction/ellipse/compare pipeline end
    to end without AMPS.  For each (rigidity, hemisphere, MLT) reference boundary
    it places two bracketing nodes so the poleward Rc_lower==R crossing lands on
    the reference latitude.
    """
    rows: List[ShellRow] = []
    interval = [r for r in reference_rows if r.midpoint == midpoint and not r.missing
                and r.boundary_lat_deg is not None]
    # Group reference boundaries by (hemisphere, MLT) and build ONE monotonic
    # Rc_lower(latitude) profile that crosses each rigidity at its reference
    # latitude, exactly as a real shell exposes a single cutoff per node.
    grouped: Dict[Tuple[str, float], List[Tuple[float, float]]] = {}
    for r in interval:
        grouped.setdefault((r.hemisphere, round(r.mlt_hour, 3)), []).append(
            (r.rigidity_gv, r.boundary_lat_deg))
    for (hemi, mlt), points in grouped.items():
        sign = 1.0 if hemi == "N" else -1.0
        # Anchor node equatorward of the lowest boundary with Rc above all rigidities.
        min_lat = min(lat for _, lat in points)
        max_rig = max(rig for rig, _ in points)
        anchors = [(min_lat - 3.0, max_rig * 1.5)] + [(lat, rig) for rig, lat in points]
        for lat_abs, rc in anchors:
            row = ShellRow(longitude_deg=mlt * 15.0, latitude_deg=sign * lat_abs,
                           rc_lower_gv=rc, rc_effective_gv=rc, rc_upper_gv=rc)
            row.aacgm_latitude_deg = sign * lat_abs
            row.mlt_hour = mlt
            rows.append(row)
    return rows


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=("C10 POES/MetOp MEPED proton-access-boundary code validation. "
                     "P6/P7 background-corrected independent cells gate PASS/FAIL; "
                     "P8/P9 are diagnostics."),
        epilog=("Examples:\n"
                "  python3 run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --profile ROUTINE -np 8 -nt 16\n"
                "  python3 run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --profile ROUTINE -np 8 -nt 16 --mode3d-parallel-field-init\n"
                "  python3 run_C10.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --profile ROUTINE --access-consistency-root test_output/C10_direct -np 8 -nt 16\n"),
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--profile", type=str.upper, choices=("SMOKE", "ROUTINE", "FULL"), default="ROUTINE")
    parser.add_argument("--timestamps", default="", help="Comma-separated interval midpoints; overrides profile")
    parser.add_argument("--interval-samples", type=int, default=1,
                        help="Field snapshots per interval; interval boundary = mean of snapshot medians")
    parser.add_argument("--solver", type=str.upper, choices=SOLVERS, default="BOTH")
    parser.add_argument(
        "--cutoff-evaluation", type=str.upper, choices=CUTOFF_EVALUATIONS,
        default="FULL_SCAN",
        help=("FULL_SCAN performs PENUMBRA_SCAN and writes companion exact-rigidity "
              "states; DIRECT_ACCESS is a GRIDDED-only exact-rigidity latitude-band product"),
    )
    parser.add_argument(
        "--comparison-observable", type=str.upper,
        choices=COMPARISON_OBSERVABLES, default=None,
        help=("ACCESS_T50 is common to FULL_SCAN and DIRECT_ACCESS; RC_* are "
              "FULL_SCAN diagnostics; ALL uses ACCESS_T50 for PASS/FAIL and writes diagnostics"),
    )
    parser.add_argument("--reference", default=str(DEFAULT_REFERENCE))
    parser.add_argument("--driver", default=str(DEFAULT_DRIVER))
    parser.add_argument("--allow-model-reference", action="store_true",
                        help="Legacy-only escape hatch; real archive-derived references are required for scientific eligibility")
    parser.add_argument("--allow-unverified-driver", action="store_true")
    parser.add_argument("--validate-references", action="store_true")
    parser.add_argument("--validate-driver", action="store_true")
    parser.add_argument("--self-test", action="store_true",
                        help="Exercise extraction/ellipse/compare on a synthetic shell (no AMPS)")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-run", action="store_true", help="Reprocess existing raw outputs")
    parser.add_argument("--keep", action="store_true")
    parser.add_argument("--output-root", default="test_output/C10")
    parser.add_argument(
        "--plot-formats", default="png,eps", metavar="FMT[,FMT...]",
        help=("Comma-separated Matplotlib output formats for each branch's "
              "time-series/scatter/MLT comparison plots, e.g. 'png,eps', "
              "'png', or 'png,eps,pdf,svg'; ignored entirely if Matplotlib "
              "is not installed"),
    )
    parser.add_argument("--amps", default="./amps")
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("-np", dest="np", type=int, default=4)
    parser.add_argument("-nt", dest="nt", type=int, default=16)
    parser.add_argument(
        "--mode3d-parallel-field-init", "--parallel-field-init",
        action="store_true",
        help=("Parallelize the GRIDDED background magnetic-field initialization "
              "with POSIX threads. AMPS uses the same worker-thread count supplied "
              "by -nt/--mode3d-threads for the subsequent Mode3D calculations."),
    )
    parser.add_argument("--scheduler", type=str.upper, choices=("STATIC", "BLOCK_CYCLIC", "DYNAMIC"), default="DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0)
    parser.add_argument("--mover", default="")
    parser.add_argument("--mode3d-mesh-res-earth-re", type=float, default=0.02)
    parser.add_argument("--mode3d-mesh-res-boundary-re", type=float, default=2.0)
    parser.add_argument("--mode3d-mesh-coarsening", type=str.upper, choices=("LINEAR", "LOG"), default="LINEAR")
    parser.add_argument("--mode3d-mesh-exponent", type=float, default=1.0)
    parser.add_argument("--cutoff-scan-n", type=int, default=120)
    parser.add_argument("--cutoff-trace-policy", type=str.upper, choices=("ACCURATE", "LEGACY"), default="ACCURATE")
    parser.add_argument("--boundary-cutoff", type=str.lower, choices=("lower", "effective", "upper"), default="effective",
                        help="Legacy alias selecting an RC_* observable when --comparison-observable is omitted")
    parser.add_argument("--t50-grid-step-deg", type=float, default=0.25,
                        help="Absolute-AACGM grid step for ACCESS_T50 fitting")
    parser.add_argument("--t50-min-resolved-profile-fraction", type=float, default=0.66,
                        help="Minimum resolved longitude-profile fraction at a T50 grid point")
    parser.add_argument("--t50-min-edge-margin-deg", type=float, default=1.0,
                        help="Minimum T50 distance from retained latitude-domain edges")
    parser.add_argument(
        "--minimum-diagnostic-paired-cells-for-mean", type=int, default=2,
        help=("Deprecated plotting threshold retained for CLI compatibility; comparison "
              "plots now connect every available paired P8/P9 mean"),
    )
    parser.add_argument("--access-consistency-root", default="",
                        help="Optional output root from the counterpart FULL_SCAN/DIRECT_ACCESS run")
    parser.add_argument("--min-access-state-agreement", type=float, default=0.999)
    parser.add_argument("--max-access-unresolved-fraction", type=float, default=0.01)
    parser.add_argument("--rigidity-min-gv", type=float, default=0.05)
    parser.add_argument("--rigidity-max-gv", type=float, default=1.30)
    parser.add_argument("--altitude-km", type=float, default=850.0)
    parser.add_argument("--shell-lon-res-deg", type=float, default=15.0)
    parser.add_argument("--shell-lat-res-deg", type=float, default=2.0)
    parser.add_argument("--access-abs-lat-min-deg", type=float, default=45.0)
    parser.add_argument("--access-abs-lat-max-deg", type=float, default=85.0)
    parser.add_argument("--max-trace-time", type=float, default=20.0)
    parser.add_argument("--n-mlt-bins", type=int, default=8)
    # Acceptance thresholds
    parser.add_argument("--min-valid-fraction", type=float, default=0.85)
    parser.add_argument("--max-rmse-deg", type=float, default=3.0)
    parser.add_argument("--max-abs-bias-deg", type=float, default=2.0)
    parser.add_argument("--min-correlation", type=float, default=0.80)
    parser.add_argument("--min-expansion-deg", type=float, default=4.0)
    parser.add_argument("--max-expansion-deg", type=float, default=12.0)
    parser.add_argument("--max-expansion-time-error-minutes", type=float, default=180.0)
    args = parser.parse_args(argv)
    if args.interval_samples < 1:
        parser.error("--interval-samples must be >= 1")
    if args.cutoff_evaluation == "FULL_SCAN" and args.cutoff_scan_n < 2:
        parser.error("--cutoff-scan-n must be >= 2 for FULL_SCAN")
    if args.cutoff_evaluation == "DIRECT_ACCESS" and args.solver != "GRIDDED":
        parser.error("DIRECT_ACCESS is implemented only for --solver GRIDDED")
    tokens = list(sys.argv[1:] if argv is None else argv)
    if args.comparison_observable is None:
        if any(token == "--boundary-cutoff" or token.startswith("--boundary-cutoff=")
               for token in tokens):
            args.comparison_observable = "RC_" + args.boundary_cutoff.upper()
        else:
            args.comparison_observable = "ACCESS_T50"
    if (args.cutoff_evaluation == "DIRECT_ACCESS"
            and args.comparison_observable not in ("ACCESS_T50",)):
        parser.error("DIRECT_ACCESS supports only ACCESS_T50")
    if not (0.0 < args.t50_min_resolved_profile_fraction <= 1.0):
        parser.error("--t50-min-resolved-profile-fraction must be in (0,1]")
    if args.minimum_diagnostic_paired_cells_for_mean < 1:
        parser.error("--minimum-diagnostic-paired-cells-for-mean must be >= 1")
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


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def _rc_attribute(name: str) -> str:
    return {"lower": "rc_lower_gv", "effective": "rc_effective_gv", "upper": "rc_upper_gv"}[name]


def _self_test_reference_rows() -> List[ReferenceRow]:
    """Create a complete improved-reference fixture for ``--self-test``.

    The fixture is intentionally independent of the checked-in compressed
    placeholder and of the NOAA archive.  It exercises the reference axes,
    shell-boundary reconstruction, per-channel bookkeeping, and P6/P7
    acceptance gate without requiring external files.
    """

    channel_specs = (
        ("P6", 16.0, 0.174013525, "PRIMARY"),
        ("P7", 36.0, 0.262395866, "PRIMARY"),
        ("P8", 70.0, 0.369131538, "DIAGNOSTIC"),
        ("P9", 140.0, 0.531334344, "DIAGNOSTIC"),
    )
    rows: List[ReferenceRow] = []
    midpoints = [parse_utc(value) for value in ROUTINE_MIDPOINTS]
    for time_index, midpoint in enumerate(midpoints):
        # Smooth synthetic storm expansion followed by recovery.
        phase = time_index / max(1.0, float(len(midpoints) - 1))
        storm_shift = 5.0 * math.sin(math.pi * phase)
        for channel_index, (channel, energy, rigidity, role) in enumerate(channel_specs):
            for hemisphere in ("N", "S"):
                hemisphere_shift = 0.25 if hemisphere == "N" else -0.25
                for mlt in (0.0, 3.0, 6.0, 9.0, 12.0, 15.0, 18.0, 21.0):
                    asymmetry = 0.8 * math.cos(2.0 * math.pi * (mlt - 6.0) / 24.0)
                    boundary = (66.0 - 1.8 * channel_index - storm_shift
                                + hemisphere_shift + asymmetry)
                    rows.append(ReferenceRow(
                        midpoint=midpoint,
                        interval_start=midpoint - timedelta(hours=1),
                        interval_end=midpoint + timedelta(hours=1),
                        rigidity_gv=rigidity,
                        energy_threshold_mev=energy,
                        channel=channel,
                        hemisphere=hemisphere,
                        mlt_hour=mlt,
                        boundary_lat_deg=boundary,
                        sigma_deg=0.5,
                        altitude_km=850.0,
                        sym_h_nt=-50.0,
                        missing=False,
                        source="SELF_TEST_MODEL",
                        n_crossings=2,
                        satellites="SELF_TEST",
                        validation_role=role,
                        acceptance_eligible=(role == "PRIMARY"),
                        background_corrected=True,
                        n_distinct_pass_legs=2,
                        n_distinct_satellites=2,
                        median_transition_width_deg=2.0,
                    ))
    return rows


def _self_test(reference_rows: Sequence[ReferenceRow], args: argparse.Namespace) -> int:
    rigidities, mlt_bins, hemispheres = reference_axes(reference_rows)
    rc_attr = _rc_attribute(args.boundary_cutoff)
    midpoints = selected_midpoints(reference_rows, args)
    interval_models: Dict[datetime, Dict[Tuple[float, str], BoundaryEstimate]] = {}
    for midpoint in midpoints:
        rows = synthetic_shell(reference_rows, midpoint, args)
        estimates = estimate_boundaries(rows, rigidities, mlt_bins, hemispheres, rc_attr, args.n_mlt_bins)
        interval_models[midpoint] = {(round(e.rigidity_gv, 9), e.hemisphere): e for e in estimates}
    subset = [r for r in reference_rows if r.midpoint in set(midpoints)]
    detailed, metrics = compare(subset, interval_models, DriverInfo(
        path="self-test", sha256="", verified_driver=False, n_records=0,
        median_cadence_seconds=300.0, first_utc="", last_utc=""),
        args, reference_is_model=True)
    print("SELF-TEST rmse=%.4f bias=%.4f corr=%s valid=%.3f expansion(mod=%s obs=%s)" % (
        metrics.rmse_deg, metrics.mean_bias_deg,
        None if metrics.correlation is None else "%.4f" % metrics.correlation,
        metrics.valid_fraction,
        None if metrics.modeled_expansion_deg is None else "%.2f" % metrics.modeled_expansion_deg,
        None if metrics.observed_expansion_deg is None else "%.2f" % metrics.observed_expansion_deg))
    ok = (metrics.rmse_deg < 1.0e-6 and abs(metrics.mean_bias_deg) < 1.0e-6
          and metrics.valid_fraction > 0.99)
    print("SELF-TEST %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)

    # Driver validation is intentionally independent of the observational
    # reference.  This permits installation checks before the large NCEI files
    # have been downloaded and before the compressed reference CSV has been
    # generated.
    driver_path = Path(os.environ.get("C10_TS05_DRIVER", args.driver))
    if args.validate_driver:
        info = validate_driver(driver_path, [])
        print(json.dumps(asdict(info), indent=2))
        return 0 if info.verified_driver else 1

    if args.self_test:
        return _self_test(_self_test_reference_rows(), args)

    reference_path = Path(args.reference)
    try:
        reference_rows = load_reference(reference_path)
    except (OSError, ValueError, KeyError) as exc:
        print(
            "C10 reference could not be loaded: %s\n"
            "Generate the archive-derived reference with build_poes_reference.py "
            "as documented in README.md." % exc,
            file=sys.stderr,
        )
        return 2

    reference_ok, reference_problems = validate_references(reference_rows)
    if args.validate_references:
        for problem in reference_problems:
            print("  - %s" % problem)
        print("C10 reference validation: %s" % ("OK" if reference_ok else "PROBLEMS"))
        return 0 if reference_ok else 1
    if not reference_ok and not args.allow_model_reference:
        print("C10 reference is not eligible for the improved code-validation gate:", file=sys.stderr)
        for problem in reference_problems:
            print("  - %s" % problem, file=sys.stderr)
        print("Rebuild reference_C10_poes_meped_boundary.csv.gz with the current "
              "build_poes_reference.py.", file=sys.stderr)
        return 2

    reference_is_model = any("MODEL" in r.source.upper() for r in reference_rows)
    reference_is_archive = all((r.source.upper().startswith("POES_NCEI") or r.missing) for r in reference_rows)
    if not reference_is_archive and not (reference_is_model and args.allow_model_reference):
        print("C10 requires an archive-derived POES_NCEI reference. Rebuild it with "
              "build_poes_reference.py, or use --allow-model-reference only for legacy structural tests.",
              file=sys.stderr)
        return 2
    rigidities, mlt_bins, hemispheres = reference_axes(reference_rows)
    args.rigidities_gv = rigidities

    midpoints = selected_midpoints(reference_rows, args)
    reference_by_midpoint: Dict[datetime, List[ReferenceRow]] = {}
    for r in reference_rows:
        reference_by_midpoint.setdefault(r.midpoint, []).append(r)

    required_times: List[datetime] = []
    for midpoint in midpoints:
        interval = reference_by_midpoint[midpoint]
        start, end = interval[0].interval_start, interval[0].interval_end
        required_times += interval_sample_times(midpoint, start, end, args.interval_samples)
    driver_info = validate_driver(driver_path, required_times)
    driver_symh = driver_sym_h_series(driver_path)
    if not driver_info.verified_driver and not args.allow_unverified_driver:
        print("C10 driver is not the verified bundled TS05 driver; pass "
              "--allow-unverified-driver to proceed (result marked ineligible).",
              file=sys.stderr)
        return 2

    launch_dir = Path.cwd()
    output_root = Path(args.output_root).expanduser()
    if not output_root.is_absolute():
        output_root = (launch_dir / output_root).resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    amps_path = Path(args.amps).expanduser()
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()
    if not args.dry_run and not args.skip_run:
        if not amps_path.exists():
            print("AMPS executable not found: %s" % amps_path, file=sys.stderr)
            return 2
        if not os.access(str(amps_path), os.X_OK):
            print("AMPS executable is not executable: %s" % amps_path, file=sys.stderr)
            return 2
    solvers = ("GRIDLESS", "GRIDDED") if args.solver == "BOTH" else (args.solver,)

    templates = {"GRIDLESS": DEFAULT_TEMPLATE_GRIDLESS, "GRIDDED": DEFAULT_TEMPLATE_MODE3D}
    branch_metrics: Dict[str, Metrics] = {}
    commands_inventory: List[Dict[str, object]] = []

    for solver in solvers:
        solver_root = output_root / solver.lower()
        if solver_root.exists() and not args.keep and not args.skip_run:
            shutil.rmtree(solver_root)
        solver_root.mkdir(parents=True, exist_ok=True)
        driver_copy = solver_root / "ts05_driver.txt"
        if not driver_copy.exists():
            shutil.copy2(driver_path, driver_copy)

        interval_models: Dict[datetime, Dict[Tuple[float, str], BoundaryEstimate]] = {}
        access_consistency_summaries: List[Dict[str, object]] = []
        access_consistency_details: List[Dict[str, object]] = []
        for midpoint in midpoints:
            interval = reference_by_midpoint[midpoint]
            start, end = interval[0].interval_start, interval[0].interval_end
            sample_times = interval_sample_times(midpoint, start, end, args.interval_samples)
            interval_dir = solver_root / midpoint.strftime("%Y%m%dT%H%M%S")
            interval_dir.mkdir(parents=True, exist_ok=True)
            # per (rigidity, hemisphere): accumulate per-MLT boundary across samples
            sample_bins: Dict[Tuple[float, str], Dict[float, List[float]]] = {}

            for sidx, epoch in enumerate(sample_times):
                sample_dir = interval_dir / ("sample_%02d_%s" % (sidx, epoch.strftime("%Y%m%dT%H%M%S")))
                sample_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy2(driver_copy, sample_dir / "ts05_driver.txt")
                input_path = sample_dir / "AMPS_PARAM_C10.in"
                if not args.skip_run:
                    render_input(templates[solver], input_path, epoch, driver_copy, args, solver)
                command = command_for(args, amps_path, solver, epoch)
                inv = {"solver": solver, "interval_midpoint_utc": format_utc(midpoint),
                       "sample_epoch_utc": format_utc(epoch), "cwd": str(sample_dir),
                       "command": command}
                commands_inventory.append(inv)
                print("C10 %s %s sample %d/%d:\n  %s" % (solver.lower(), format_utc(midpoint),
                      sidx + 1, len(sample_times), " ".join(command)))
                if args.dry_run:
                    continue
                if not args.skip_run:
                    rc = run_process(command, sample_dir, sample_dir / "C10_amps.log")
                    if rc != 0:
                        print("AMPS failed (%d) in %s" % (rc, sample_dir), file=sys.stderr)
                        return rc
                diagnostic_estimates: Dict[str, List[BoundaryEstimate]] = {}
                t50_profile_rows: List[Dict[str, object]] = []
                if args.cutoff_evaluation == "DIRECT_ACCESS":
                    access_name = "cutoff_3d_shells_access.dat"
                else:
                    access_name = ("cutoff_gridless_shells_pamela_access.dat"
                                   if solver == "GRIDLESS"
                                   else "cutoff_3d_shells_pamela_access.dat")
                access_path = sample_dir / access_name
                if not access_path.exists():
                    print("C10 exact-rigidity access output not found: %s" % access_path,
                          file=sys.stderr)
                    return 2
                access_rows = select_common_access_band(
                    parse_tecplot_shell_access(access_path),
                    args.access_abs_lat_min_deg, args.access_abs_lat_max_deg)

                if args.access_consistency_root and solver == "GRIDDED":
                    counterpart_root = Path(args.access_consistency_root).expanduser()
                    if not counterpart_root.is_absolute():
                        counterpart_root = (launch_dir / counterpart_root).resolve()
                    relative_sample = sample_dir.relative_to(output_root)
                    counterpart_dir = counterpart_root / relative_sample
                    counterpart_name = ("cutoff_3d_shells_access.dat"
                                        if args.cutoff_evaluation == "FULL_SCAN"
                                        else "cutoff_3d_shells_pamela_access.dat")
                    counterpart_path = counterpart_dir / counterpart_name
                    if counterpart_path.exists():
                        counterpart_rows = select_common_access_band(
                            parse_tecplot_shell_access(counterpart_path),
                            args.access_abs_lat_min_deg, args.access_abs_lat_max_deg)
                        consistency, differences = compare_access_states(
                            access_rows, counterpart_rows,
                            args.min_access_state_agreement,
                            args.max_access_unresolved_fraction)
                    else:
                        consistency = {
                            "counterpart_path": str(counterpart_path),
                            "error": "matching counterpart access file not found",
                            "passed": False,
                        }
                        differences = []
                    consistency.update({
                        "interval_midpoint_utc": format_utc(midpoint),
                        "sample_epoch_utc": format_utc(epoch),
                        "current_cutoff_evaluation": args.cutoff_evaluation,
                        "counterpart_path": str(counterpart_path),
                    })
                    for difference in differences:
                        difference.update({
                            "interval_midpoint_utc": format_utc(midpoint),
                            "sample_epoch_utc": format_utc(epoch),
                        })
                    access_consistency_summaries.append(consistency)
                    access_consistency_details.extend(differences)
                    (sample_dir / "C10_access_consistency.json").write_text(
                        json.dumps(consistency, indent=2) + "\n")
                    write_dict_rows(sample_dir / "C10_access_consistency_differences.csv",
                                    differences)

                add_aacgm_lat_mlt(access_rows, epoch, args.altitude_km)
                access_estimates, t50_profile_rows = estimate_access_t50_boundaries(
                    access_rows, rigidities, mlt_bins, hemispheres, args.n_mlt_bins,
                    args.t50_grid_step_deg,
                    args.t50_min_resolved_profile_fraction,
                    args.t50_min_edge_margin_deg)
                diagnostic_estimates["ACCESS_T50"] = access_estimates

                if args.cutoff_evaluation == "FULL_SCAN":
                    penumbra_name = ("cutoff_gridless_shells_penumbra.dat"
                                     if solver == "GRIDLESS"
                                     else "cutoff_3d_shells_penumbra.dat")
                    penumbra_path = sample_dir / penumbra_name
                    if not penumbra_path.exists():
                        print("C10 penumbra output not found: %s" % penumbra_path,
                              file=sys.stderr)
                        return 2
                    shell_rows = parse_tecplot_shell_penumbra(penumbra_path)
                    add_aacgm_lat_mlt(shell_rows, epoch, args.altitude_km)
                    diagnostic_estimates["RC_LOWER"] = estimate_boundaries(
                        shell_rows, rigidities, mlt_bins, hemispheres,
                        "rc_lower_gv", args.n_mlt_bins)
                    diagnostic_estimates["RC_EFFECTIVE"] = estimate_boundaries(
                        shell_rows, rigidities, mlt_bins, hemispheres,
                        "rc_effective_gv", args.n_mlt_bins)
                    diagnostic_estimates["RC_UPPER"] = estimate_boundaries(
                        shell_rows, rigidities, mlt_bins, hemispheres,
                        "rc_upper_gv", args.n_mlt_bins)

                primary = ("ACCESS_T50" if args.comparison_observable == "ALL"
                           else args.comparison_observable)
                estimates = diagnostic_estimates[primary]
                write_dict_rows(sample_dir / "C10_snapshot_boundaries.csv",
                                [_estimate_row(e) for e in estimates])
                if t50_profile_rows:
                    write_dict_rows(sample_dir / "C10_snapshot_t50_profiles.csv",
                                    t50_profile_rows)
                if args.comparison_observable == "ALL":
                    for observable, diagnostic in diagnostic_estimates.items():
                        write_dict_rows(
                            sample_dir / ("C10_snapshot_boundaries_%s.csv" % observable.lower()),
                            [_estimate_row(e) for e in diagnostic])
                for e in estimates:
                    key = (round(e.rigidity_gv, 9), e.hemisphere)
                    bins = sample_bins.setdefault(key, {})
                    for mlt, val in e.boundary_by_mlt.items():
                        if val is not None:
                            bins.setdefault(mlt, []).append(val)

            if args.dry_run:
                continue

            model: Dict[Tuple[float, str], BoundaryEstimate] = {}
            for rigidity in rigidities:
                for hemi in hemispheres:
                    key = (round(rigidity, 9), hemi)
                    bins = sample_bins.get(key, {})
                    boundary_by_mlt = {round(m, 3): (statistics.fmean(bins[m]) if m in bins and bins[m] else None)
                                       for m in mlt_bins}
                    valid = [v for v in boundary_by_mlt.values() if v is not None]
                    ellipse = fit_ellipse(list(boundary_by_mlt.keys()), list(boundary_by_mlt.values()))
                    model[key] = BoundaryEstimate(
                        rigidity, hemi, boundary_by_mlt, len(valid), len(mlt_bins),
                        ellipse, observable=("ACCESS_T50" if args.comparison_observable == "ALL"
                                             else args.comparison_observable))
            interval_models[midpoint] = model

        if args.dry_run:
            continue

        subset = [r for r in reference_rows if r.midpoint in set(midpoints)]
        detailed, metrics = compare(
            subset, interval_models, driver_info, args, reference_is_model, driver_symh
        )
        branch_access_consistency = None
        if args.access_consistency_root and solver == "GRIDDED":
            total_common = sum(int(row.get("n_common_rows", 0)) for row in access_consistency_summaries)
            total_resolved = sum(int(row.get("n_resolved_common", 0)) for row in access_consistency_summaries)
            total_mismatch = sum(int(row.get("n_resolved_mismatch", 0)) for row in access_consistency_summaries)
            total_unresolved = sum(int(row.get("n_unresolved_in_either_product", 0)) for row in access_consistency_summaries)
            branch_access_consistency = {
                "n_snapshots_compared": len(access_consistency_summaries),
                "n_common_rows": total_common,
                "n_resolved_common": total_resolved,
                "n_resolved_mismatch": total_mismatch,
                "resolved_access_state_agreement_fraction": (
                    1.0 - total_mismatch / float(total_resolved) if total_resolved else 0.0),
                "n_unresolved_in_either_product": total_unresolved,
                "unresolved_in_either_product_fraction": (
                    total_unresolved / float(total_common) if total_common else 1.0),
                "passed": (bool(access_consistency_summaries)
                           and all(bool(row.get("passed")) for row in access_consistency_summaries)),
                "counterpart_output_root": str(args.access_consistency_root),
            }
            metrics.passed = metrics.passed and bool(branch_access_consistency["passed"])
            (solver_root / "C10_access_consistency.json").write_text(
                json.dumps(branch_access_consistency, indent=2) + "\n")
            write_dict_rows(solver_root / "C10_access_consistency_snapshots.csv",
                            access_consistency_summaries)
            write_dict_rows(solver_root / "C10_access_consistency_differences.csv",
                            access_consistency_details)
        write_dict_rows(solver_root / "C10_comparison.csv", detailed)
        write_dict_rows(solver_root / "C10_diagnostic_flags.csv", diagnostic_flag_rows(detailed))
        # Keep the traditional filenames for the uncluttered primary-channel view,
        # and write a second complete set that includes P8/P9 diagnostics.
        plot_paths: List[Path] = []
        plot_paths += make_plot(detailed, solver_root / "C10_comparison", solver,
                                 include_diagnostics=False, formats=args.plot_formats)
        plot_paths += make_plot(detailed, solver_root / "C10_comparison_with_diagnostics", solver,
                                 include_diagnostics=True, formats=args.plot_formats)
        plot_paths += make_scatter_plot(detailed, solver_root / "C10_scatter", solver,
                                         include_diagnostics=False, formats=args.plot_formats)
        plot_paths += make_scatter_plot(detailed, solver_root / "C10_scatter_with_diagnostics", solver,
                                         include_diagnostics=True, formats=args.plot_formats)
        plot_paths += make_mlt_plot(detailed, solver_root / "C10_mlt_comparison", solver,
                                     include_diagnostics=False, formats=args.plot_formats)
        plot_paths += make_mlt_plot(detailed, solver_root / "C10_mlt_comparison_with_diagnostics", solver,
                                     include_diagnostics=True, formats=args.plot_formats)
        if plot_paths:
            print("C10 %s comparison plot(s): %s" % (
                solver, ", ".join(p.name for p in plot_paths)
            ))
        (solver_root / "C10_result.json").write_text(json.dumps({
            "solver": solver, "profile": args.profile,
            "cutoff_evaluation": args.cutoff_evaluation,
            "comparison_observable": args.comparison_observable,
            "boundary_cutoff": args.boundary_cutoff,
            "interval_samples": args.interval_samples,
            "access_state_consistency": branch_access_consistency,
            "t50_postprocessing": {
                "aacgm_grid_step_deg": args.t50_grid_step_deg,
                "minimum_resolved_profile_fraction": args.t50_min_resolved_profile_fraction,
                "minimum_boundary_edge_margin_deg": args.t50_min_edge_margin_deg,
                "common_abs_geodetic_latitude_band_deg": [
                    args.access_abs_lat_min_deg, args.access_abs_lat_max_deg],
            },
            "validation_gate": {
                "primary_channels": ["P6", "P7"],
                "diagnostic_channels": ["P8", "P9"],
                "requires_background_corrected_reference": True,
                "requires_independent_windows": True,
                "diagnostic_means_use_paired_cells_only": True,
                "diagnostic_plot_means_use_all_available_paired_cells": True,
                "diagnostic_plot_lines_connect_all_available_means": True,
                "legacy_minimum_diagnostic_paired_cells_for_mean": (
                    args.minimum_diagnostic_paired_cells_for_mean),
                "sparse_or_cross_channel_outlier_diagnostics_remain_in_C10_diagnostic_flags_csv": True,
            },
            "metrics": asdict(metrics),
            "reference_is_model": reference_is_model,
            "reference_is_archive": reference_is_archive,
        }, indent=2) + "\n")
        branch_metrics[solver] = metrics
        print("C10 %s -> %s (PRIMARY P6/P7 rmse=%.3f bias=%.3f corr=%s; "
              "all-channel diagnostic rmse=%s; expansion=%s)" % (
            solver, "PASS" if metrics.passed else "FAIL", metrics.rmse_deg, metrics.mean_bias_deg,
            None if metrics.correlation is None else "%.3f" % metrics.correlation,
            None if metrics.all_rmse_deg is None else "%.3f" % metrics.all_rmse_deg,
            None if metrics.modeled_expansion_deg is None else "%.2f" % metrics.modeled_expansion_deg))

    (output_root / "C10_commands.json").write_text(json.dumps(commands_inventory, indent=2) + "\n")
    if args.dry_run:
        print("C10 dry-run complete; inputs and commands generated.")
        return 0

    passed = bool(branch_metrics) and all(m.passed for m in branch_metrics.values())
    (output_root / "C10_result.json").write_text(json.dumps({
        "profile": args.profile, "cutoff_evaluation": args.cutoff_evaluation,
        "comparison_observable": args.comparison_observable,
        "solvers": list(branch_metrics),
        "reference": str(reference_path), "reference_is_model": reference_is_model,
        "reference_is_archive": reference_is_archive,
        "driver_verified": driver_info.verified_driver,
        "validation_gate": "P6_P7_BACKGROUND_NORMALIZED_INDEPENDENT_WINDOWS;P8_P9_ROBUST_PAIRED_DIAGNOSTICS",
        "diagnostic_channels": ["P8", "P9"],
        "branches": {s: asdict(m) for s, m in branch_metrics.items()},
        "passed": passed,
    }, indent=2) + "\n")
    print("C10 selected branches -> %s" % ("PASS" if passed else "FAIL"))
    print("Results: %s" % output_root)
    return 0 if passed else 1


def _estimate_row(e: BoundaryEstimate) -> Dict[str, object]:
    row: Dict[str, object] = {"rigidity_gv": e.rigidity_gv,
                              "observable": e.observable,
                              "hemisphere": e.hemisphere,
                              "n_valid_mlt": e.n_valid_mlt, "n_requested_mlt": e.n_requested_mlt,
                              "ellipse_center_colat_deg": e.ellipse.center_colat_deg,
                              "ellipse_amplitude_deg": e.ellipse.amplitude_deg,
                              "ellipse_phase_mlt_hour": e.ellipse.phase_mlt_hour}
    for mlt, val in sorted(e.boundary_by_mlt.items()):
        row["mlt_%04.1f_lat_deg" % mlt] = val
    return row


if __name__ == "__main__":
    raise SystemExit(main())
