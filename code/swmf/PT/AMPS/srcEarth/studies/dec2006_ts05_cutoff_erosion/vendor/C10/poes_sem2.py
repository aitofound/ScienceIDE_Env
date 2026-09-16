#!/usr/bin/env python3
"""Utilities for the C10 POES/MetOp SEM-2 reference-data pipeline.

This module contains the parts of C10 that are independent of AMPS:

* reading the historical NOAA/NCEI 16-second Level-2 SEM-2 files;
* converting sub-satellite positions to AACGM latitude and magnetic local time;
* separating the data into individual polar passes;
* estimating a low-latitude background and a polar-cap plateau for each pass leg;
* fitting a monotonic background-normalized transmission profile;
* finding the T=0.25, 0.50, and 0.75 boundaries and transition width; and
* aggregating accepted crossings into a reference grid while preserving
  independent-pass and validation-role metadata.

The code intentionally keeps the measurement-level product.  A binned reference
CSV is convenient for a regression test, but the individual boundary crossings
are the actual observational reference and must not be discarded.

Historical-file support
-----------------------
The December 2006 Level-2 archive is available in both ASCII and CDF.  NOAA's
column names have varied slightly among products, so both readers use alias
sets and report the available columns when a required quantity cannot be found.
The ASCII reader is the preferred path because it has no binary-library
requirements and its fields are documented by NCEI's ``readme_16s_ascii.txt``.

Scientific boundary definition
------------------------------
Dmitriev et al. (2010) define a cutoff latitude from the transition between the
equatorward background and the polar-cap SEP intensity.  The production C10
reference uses the background-normalized transmission
``T=(F-F_background)/(F_polar-F_background)`` and locates T=0.5 on a monotonic
isotonic fit to each inbound and outbound leg.  T25 and T75 are retained as
quality diagnostics and define the transition width.  The older uncorrected
``0.5*F_polar`` crossing remains available only as a diagnostic compatibility
mode.

Important limitation
--------------------
MEPED P6-P9 are integral channels, not monochromatic rigidity measurements.  C10
assigns each channel the proton rigidity corresponding to its nominal lower
energy threshold.  This is a transparent and reproducible first-order mapping,
but it does not replace convolution with the full detector response and an
assumed SEP spectrum.  The channel, threshold, and mapping method are written
into every reference row and into the provenance manifest.
"""
from __future__ import annotations

import contextlib
import csv
import gzip
import hashlib
import io
import json
import math
import re
import statistics
import sys
from dataclasses import asdict, dataclass, replace
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Dict, Iterable, Iterator, List, Mapping, Optional, Sequence, Tuple

PROTON_REST_MEV = 938.27208816

# The four omnidirectional channels used by Dmitriev et al. for the high-energy
# proton part of the cutoff analysis.  The rigidity is calculated from the
# nominal lower threshold and is therefore a channel label, not an assertion
# that the detector has a delta-function response at this rigidity.
CHANNEL_THRESHOLDS_MEV: Mapping[str, float] = {
    "P6": 16.0,
    "P7": 36.0,
    "P8": 70.0,
    "P9": 140.0,
}

# P6 and P7 are used for the default code-validation gate.  P8 and P9 remain
# valuable diagnostics, but their broad integral response and subcommutated
# historical archive representation require response folding before they can be
# used as equal-weight quantitative validation channels.
CHANNEL_VALIDATION_ROLE: Mapping[str, str] = {
    "P6": "PRIMARY",
    "P7": "PRIMARY",
    "P8": "DIAGNOSTIC",
    "P9": "DIAGNOSTIC",
}

CHANNEL_VARIABLE_ALIASES: Mapping[str, Tuple[str, ...]] = {
    "P6": ("mepomp6", "mep_omni_p6", "mep_omni_flux_p6", "omni_p6", "p6"),
    "P7": ("mepomp7", "mep_omni_p7", "mep_omni_flux_p7", "omni_p7", "p7"),
    "P8": ("mepomp8", "mep_omni_p8", "mep_omni_flux_p8", "omni_p8", "p8"),
    "P9": ("mepomp9", "mep_omni_p9", "mep_omni_flux_p9", "omni_p9", "p9"),
}

SATELLITE_ALIASES: Mapping[str, str] = {
    "n15": "NOAA-15", "noaa15": "NOAA-15", "noaa-15": "NOAA-15",
    "n16": "NOAA-16", "noaa16": "NOAA-16", "noaa-16": "NOAA-16",
    "n17": "NOAA-17", "noaa17": "NOAA-17", "noaa-17": "NOAA-17",
    "n18": "NOAA-18", "noaa18": "NOAA-18", "noaa-18": "NOAA-18",
    "m02": "MetOp-02", "metop02": "MetOp-02", "metop-02": "MetOp-02",
    "metopa": "MetOp-02", "metop-a": "MetOp-02",
}


@dataclass(frozen=True)
class Observation:
    """One valid 16-second SEM-2 Level-2 record."""

    time_utc: datetime
    satellite: str
    geographic_lat_deg: float
    geographic_lon_deg: float
    altitude_km: float
    aacgm_lat_deg: float
    mlt_hour: float
    l_value: Optional[float]
    flux_by_channel: Mapping[str, float]
    source_file: str
    source_sha256: str


@dataclass(frozen=True)
class BoundaryCrossing:
    """One measured cutoff crossing on one polar-pass leg.

    Production rows use a background-normalized monotonic transmission fit.
    The extra diagnostics make it possible to distinguish a code failure from a
    poor or contaminated observational transition.
    """

    event_id: str
    satellite: str
    channel: str
    validation_role: str
    energy_threshold_mev: float
    assigned_rigidity_gv: float
    mapping_method: str
    crossing_method: str
    hemisphere: str
    pass_id: str
    leg: str
    crossing_time_utc: datetime
    geographic_lat_deg: float
    geographic_lon_deg: float
    altitude_km: float
    aacgm_lat_deg: float
    mlt_hour: float
    polar_plateau_flux: float
    half_plateau_flux: float
    low_latitude_flux: Optional[float]
    normalized_t25_aacgm_lat_deg: Optional[float]
    normalized_t50_aacgm_lat_deg: Optional[float]
    normalized_t75_aacgm_lat_deg: Optional[float]
    transition_width_deg: Optional[float]
    isotonic_rms: Optional[float]
    plateau_to_background_ratio: Optional[float]
    boundary_uncertainty_deg: float
    n_polar_samples: int
    n_background_samples: int
    n_leg_samples: int
    quality_flags: str
    source_file: str
    source_sha256: str
    transition_support_samples: int = 0
    contrast_to_noise_ratio: Optional[float] = None
    cross_channel_delta_p8_p9_deg: Optional[float] = None
    cross_channel_outlier: bool = False
    aggregate_eligible: bool = True


@dataclass(frozen=True)
class ReferenceCell:
    """One window/channel/hemisphere/MLT-bin observational reference cell."""

    event_id: str
    interval_midpoint_utc: datetime
    interval_start_utc: datetime
    interval_end_utc: datetime
    rigidity_gv: float
    energy_threshold_mev: float
    channel: str
    validation_role: str
    acceptance_eligible: bool
    hemisphere: str
    mlt_hour: float
    boundary_aacgm_lat_deg: Optional[float]
    sigma_deg: Optional[float]
    altitude_km: float
    n_crossings: int
    n_distinct_pass_legs: int
    n_distinct_satellites: int
    median_transition_width_deg: Optional[float]
    background_corrected: bool
    satellites: str
    missing: bool
    source: str
    source_ref: str
    notes: str
    diagnostic_eligible: bool = False
    quality_status: str = "UNSPECIFIED"
    n_aggregate_eligible_crossings: int = 0
    n_cross_channel_outliers: int = 0
    median_transition_support_samples: Optional[float] = None
    median_contrast_to_noise_ratio: Optional[float] = None


def normalize_name(name: str) -> str:
    """Return a conservative lowercase identifier for alias matching."""

    return re.sub(r"[^a-z0-9]+", "", str(name).strip().lower())


def proton_rigidity_gv_from_kinetic_energy_mev(energy_mev: float) -> float:
    """Relativistic proton rigidity for charge number |Z|=1."""

    if energy_mev < 0.0:
        raise ValueError("kinetic energy must be non-negative")
    momentum_mev_c = math.sqrt(energy_mev * (energy_mev + 2.0 * PROTON_REST_MEV))
    return momentum_mev_c / 1000.0


def sha256_file(path: Path) -> str:
    """Calculate a streaming SHA-256 digest for provenance records."""

    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def infer_satellite(path: Path) -> str:
    """Infer the spacecraft from a historical NCEI file name.

    Historical files normally contain ``n15``, ``n16``, ``n17``, ``n18``, or
    ``m02``.  A hard failure is preferable to silently mixing an unidentified
    spacecraft into the reference product.
    """

    token = normalize_name(path.name)
    for alias, canonical in SATELLITE_ALIASES.items():
        if normalize_name(alias) in token:
            return canonical
    raise ValueError(f"cannot infer satellite from file name: {path.name}")


def _open_text(path: Path) -> io.TextIOBase:
    """Open plain or gzip-compressed historical ASCII data as text."""

    if path.suffix.lower() == ".gz":
        return io.TextIOWrapper(gzip.open(path, "rb"), encoding="utf-8", errors="replace")
    return path.open("r", encoding="utf-8", errors="replace")


def _find_alias(columns: Sequence[str], aliases: Sequence[str], required: bool = True) -> Optional[str]:
    """Return the original column name matching one of ``aliases``."""

    normalized = {normalize_name(column): column for column in columns}
    for alias in aliases:
        key = normalize_name(alias)
        if key in normalized:
            return normalized[key]
    if required:
        raise KeyError(
            "none of aliases %s is present; available columns are: %s"
            % (", ".join(aliases), ", ".join(columns))
        )
    return None


def _float_or_none(value: object) -> Optional[float]:
    """Parse a numeric field while rejecting archive fill and impossible values."""

    try:
        result = float(str(value).strip())
    except (TypeError, ValueError):
        return None
    if not math.isfinite(result):
        return None
    # Historical SEM-2 products use negative fill values, and an old unpacker
    # could emit the unphysical count 19988488 for missing P8/P9 records.
    if result < 0.0 or result >= 1.0e7:
        return None
    return result


def _parse_ascii_header(lines: Sequence[str]) -> Tuple[int, List[str]]:
    """Locate the Level-2 column-name line in a small prefix of an ASCII file."""

    required = {"year", "sslat", "sslon"}
    for index, raw in enumerate(lines):
        stripped = raw.strip().lstrip("#").strip()
        if not stripped:
            continue
        columns = re.split(r"[\s,]+", stripped)
        normalized = {normalize_name(column) for column in columns}
        if required.issubset(normalized) and any(normalize_name(c) == "mepomp6" for c in columns):
            return index, columns
    raise ValueError(
        "could not locate the 16-second Level-2 header. Expected columns such "
        "as year, sslat, sslon, and mepomp6."
    )


def read_ascii_level2(path: Path, default_altitude_km: float = 850.0) -> List[Dict[str, object]]:
    """Read one historical NCEI Level-2 16-second ASCII file.

    The return value intentionally contains geographic coordinates and raw
    channel values only.  AACGM conversion is applied in a separate function so
    it can be tested and replaced independently.
    """

    with _open_text(path) as stream:
        raw_lines = stream.readlines()
    header_index, columns = _parse_ascii_header(raw_lines[:200])

    year_col = _find_alias(columns, ("year", "yr"))
    month_col = _find_alias(columns, ("mo", "month"))
    day_col = _find_alias(columns, ("dy", "day"))
    hour_col = _find_alias(columns, ("hr", "hour"))
    minute_col = _find_alias(columns, ("mi", "minute"))
    second_col = _find_alias(columns, ("second", "sec"))
    lat_col = _find_alias(columns, ("sslat", "satlat", "latitude"))
    lon_col = _find_alias(columns, ("sslon", "satlon", "longitude"))
    altitude_col = _find_alias(columns, ("alt", "altitude", "satalt", "height"), required=False)
    mlt_col = _find_alias(columns, ("mlt", "magneticlocaltime"), required=False)
    l_col = _find_alias(columns, ("lval", "lshell", "lvalue"), required=False)
    channel_columns = {
        channel: _find_alias(columns, aliases)
        for channel, aliases in CHANNEL_VARIABLE_ALIASES.items()
    }

    rows: List[Dict[str, object]] = []
    for line_number, raw in enumerate(raw_lines[header_index + 1 :], start=header_index + 2):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        fields = re.split(r"[\s,]+", stripped)
        if len(fields) < len(columns):
            # Truncated daily files are documented in the archive.  Skipping a
            # malformed record and recording file-level provenance is safer than
            # shifting every later column by accident.
            continue
        record = dict(zip(columns, fields[: len(columns)]))
        try:
            second = float(record[second_col])
            whole_second = int(math.floor(second))
            microsecond = int(round((second - whole_second) * 1.0e6))
            when = datetime(
                int(float(record[year_col])), int(float(record[month_col])),
                int(float(record[day_col])), int(float(record[hour_col])),
                int(float(record[minute_col])), whole_second, microsecond,
                tzinfo=timezone.utc,
            )
            lat = float(record[lat_col])
            lon = float(record[lon_col])
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f"{path}:{line_number}: invalid time/position record: {exc}") from exc

        flux = {channel: _float_or_none(record[column]) for channel, column in channel_columns.items()}
        rows.append({
            "time_utc": when,
            "geographic_lat_deg": lat,
            "geographic_lon_deg": lon,
            "altitude_km": float(record[altitude_col]) if altitude_col and _float_or_none(record[altitude_col]) is not None else default_altitude_km,
            "archive_mlt_hour": _float_or_none(record[mlt_col]) if mlt_col else None,
            "l_value": _float_or_none(record[l_col]) if l_col else None,
            "flux_by_channel": flux,
        })
    return rows


def _read_cdf_variable(cdf: object, variable: str) -> object:
    """Small adapter around cdflib's variable getter for easier testing."""

    return cdf.varget(variable)  # type: ignore[attr-defined]


def read_cdf_level2(path: Path, default_altitude_km: float = 850.0) -> List[Dict[str, object]]:
    """Read one historical Level-2 CDF file using ``cdflib``.

    CDF naming differs among processing generations, so this reader discovers
    variables through the same alias mechanism as the ASCII reader.  For the
    December-2006 test the ASCII format remains the recommended and best-tested
    route; CDF support is provided for users who maintain a CDF mirror.
    """

    try:
        import cdflib  # type: ignore
        import numpy as np  # type: ignore
    except ImportError as exc:  # pragma: no cover - optional dependency
        raise RuntimeError("CDF input requires cdflib and numpy") from exc

    cdf = cdflib.CDF(str(path))
    info = cdf.cdf_info()
    variables = list(getattr(info, "zVariables", []) or []) + list(getattr(info, "rVariables", []) or [])
    if not variables and isinstance(info, dict):
        variables = list(info.get("zVariables", [])) + list(info.get("rVariables", []))

    def alias(names: Sequence[str], required: bool = True) -> Optional[str]:
        return _find_alias(variables, names, required=required)

    epoch_name = alias(("epoch", "time", "datetime", "timestamp"), required=False)
    lat_name = alias(("sslat", "satlat", "latitude"))
    lon_name = alias(("sslon", "satlon", "longitude"))
    alt_name = alias(("alt", "altitude", "satalt", "height"), required=False)
    mlt_name = alias(("mlt", "magneticlocaltime"), required=False)
    l_name = alias(("lval", "lshell", "lvalue"), required=False)
    channel_names = {channel: alias(names) for channel, names in CHANNEL_VARIABLE_ALIASES.items()}

    if epoch_name:
        epochs = _read_cdf_variable(cdf, epoch_name)
        times = cdflib.cdfepoch.to_datetime(epochs)
        times = [
            value.astype("datetime64[us]").astype(datetime).replace(tzinfo=timezone.utc)
            if hasattr(value, "astype") else value.replace(tzinfo=timezone.utc)
            for value in times
        ]
    else:
        y = _read_cdf_variable(cdf, alias(("year", "yr")))
        mo = _read_cdf_variable(cdf, alias(("mo", "month")))
        d = _read_cdf_variable(cdf, alias(("dy", "day")))
        h = _read_cdf_variable(cdf, alias(("hr", "hour")))
        mi = _read_cdf_variable(cdf, alias(("mi", "minute")))
        sec = _read_cdf_variable(cdf, alias(("second", "sec")))
        times = [
            datetime(int(y[i]), int(mo[i]), int(d[i]), int(h[i]), int(mi[i]), int(float(sec[i])), tzinfo=timezone.utc)
            for i in range(len(y))
        ]

    latitudes = np.asarray(_read_cdf_variable(cdf, lat_name)).reshape(-1)
    longitudes = np.asarray(_read_cdf_variable(cdf, lon_name)).reshape(-1)
    altitudes = np.asarray(_read_cdf_variable(cdf, alt_name)).reshape(-1) if alt_name else None
    mlts = np.asarray(_read_cdf_variable(cdf, mlt_name)).reshape(-1) if mlt_name else None
    lvals = np.asarray(_read_cdf_variable(cdf, l_name)).reshape(-1) if l_name else None
    channels = {name: np.asarray(_read_cdf_variable(cdf, variable)).reshape(-1) for name, variable in channel_names.items()}

    rows: List[Dict[str, object]] = []
    for index, when in enumerate(times):
        rows.append({
            "time_utc": when,
            "geographic_lat_deg": float(latitudes[index]),
            "geographic_lon_deg": float(longitudes[index]),
            "altitude_km": float(altitudes[index]) if altitudes is not None and _float_or_none(altitudes[index]) is not None else default_altitude_km,
            "archive_mlt_hour": _float_or_none(mlts[index]) if mlts is not None else None,
            "l_value": _float_or_none(lvals[index]) if lvals is not None else None,
            "flux_by_channel": {channel: _float_or_none(values[index]) for channel, values in channels.items()},
        })
    return rows


def read_level2_file(path: Path, default_altitude_km: float = 850.0) -> List[Dict[str, object]]:
    """Dispatch to the ASCII or CDF reader based on the file suffix."""

    lower = path.name.lower()
    if lower.endswith((".txt", ".txt.gz", ".asc", ".asc.gz", ".dat", ".dat.gz")):
        return read_ascii_level2(path, default_altitude_km)
    if lower.endswith(".cdf"):
        return read_cdf_level2(path, default_altitude_km)
    raise ValueError(f"unsupported Level-2 file type: {path}")


def add_aacgm_coordinates(raw_rows: Sequence[Mapping[str, object]], path: Path) -> List[Observation]:
    """Convert geographic positions to AACGM latitude and MLT.

    ``aacgmv2.get_aacgm_coord`` returns AACGM latitude, AACGM longitude, and
    magnetic local time.  Records outside the library's supported domain are
    skipped instead of being assigned archive magnetic coordinates generated by
    a different field model; this keeps model and observation postprocessing
    internally consistent.
    """

    try:
        import aacgmv2  # type: ignore
    except ImportError as exc:  # pragma: no cover - environment dependent
        raise RuntimeError("AACGM conversion requires aacgmv2") from exc

    satellite = infer_satellite(path)
    digest = sha256_file(path)
    observations: List[Observation] = []
    for row in raw_rows:
        when = row["time_utc"]
        lat = float(row["geographic_lat_deg"])
        lon = float(row["geographic_lon_deg"])
        altitude = float(row["altitude_km"])
        try:
            aacgm_lat, _aacgm_lon, mlt = aacgmv2.get_aacgm_coord(lat, lon, altitude, when)
        except Exception:
            continue
        if not all(math.isfinite(float(value)) for value in (aacgm_lat, mlt)):
            continue
        clean_flux = {
            channel: float(value)
            for channel, value in dict(row["flux_by_channel"]).items()
            if value is not None and math.isfinite(float(value)) and float(value) > 0.0
        }
        observations.append(Observation(
            time_utc=when,
            satellite=satellite,
            geographic_lat_deg=lat,
            geographic_lon_deg=lon,
            altitude_km=altitude,
            aacgm_lat_deg=float(aacgm_lat),
            mlt_hour=float(mlt) % 24.0,
            l_value=None if row.get("l_value") is None else float(row["l_value"]),
            flux_by_channel=clean_flux,
            source_file=path.name,
            source_sha256=digest,
        ))
    return observations


def load_observations(paths: Sequence[Path], default_altitude_km: float = 850.0) -> List[Observation]:
    """Read, convert, concatenate, and time-sort multiple daily files.

    Zero-byte files are skipped with a warning because an interrupted transfer
    must not prevent use of the remaining constellation.  A non-empty malformed
    file still raises an exception so scientific corruption is never hidden.
    """

    observations: List[Observation] = []
    for path in paths:
        if path.stat().st_size == 0:
            print(f"warning: skipping empty Level-2 source file: {path}", file=sys.stderr)
            continue
        raw = read_level2_file(path, default_altitude_km)
        observations.extend(add_aacgm_coordinates(raw, path))
    observations.sort(key=lambda row: (row.satellite, row.time_utc))
    return observations


def split_polar_passes(
    observations: Sequence[Observation],
    minimum_abs_lat_deg: float = 45.0,
    maximum_gap_seconds: float = 90.0,
    required_polar_abs_lat_deg: float = 75.0,
) -> List[List[Observation]]:
    """Split observations into contiguous single-hemisphere polar passes."""

    passes: List[List[Observation]] = []
    current: List[Observation] = []
    previous: Optional[Observation] = None

    def finish() -> None:
        nonlocal current
        if current and max(abs(row.aacgm_lat_deg) for row in current) >= required_polar_abs_lat_deg:
            passes.append(current)
        current = []

    for row in observations:
        if abs(row.aacgm_lat_deg) < minimum_abs_lat_deg:
            finish()
            previous = row
            continue
        hemisphere = 1 if row.aacgm_lat_deg >= 0.0 else -1
        discontinuity = False
        if previous is not None:
            discontinuity = (
                row.satellite != previous.satellite
                or (1 if previous.aacgm_lat_deg >= 0.0 else -1) != hemisphere
                or (row.time_utc - previous.time_utc).total_seconds() > maximum_gap_seconds
            )
        if discontinuity:
            finish()
        current.append(row)
        previous = row
    finish()
    return passes


def _interpolate_circular_hour(a: float, b: float, fraction: float) -> float:
    """Linearly interpolate MLT while respecting the 24/0-hour seam."""

    delta = ((b - a + 12.0) % 24.0) - 12.0
    return (a + fraction * delta) % 24.0


def _interpolate_crossing(
    a: Observation,
    b: Observation,
    fa: float,
    fb: float,
    target: float,
) -> Tuple[float, datetime, float, float, float, float, float]:
    """Interpolate location and time between two records bracketing ``target``."""

    if math.isclose(fa, fb):
        fraction = 0.5
    else:
        fraction = (target - fa) / (fb - fa)
    fraction = min(1.0, max(0.0, fraction))
    duration = b.time_utc - a.time_utc
    when = a.time_utc + timedelta(seconds=duration.total_seconds() * fraction)
    aacgm_lat = a.aacgm_lat_deg + fraction * (b.aacgm_lat_deg - a.aacgm_lat_deg)
    geo_lat = a.geographic_lat_deg + fraction * (b.geographic_lat_deg - a.geographic_lat_deg)
    lon_delta = ((b.geographic_lon_deg - a.geographic_lon_deg + 180.0) % 360.0) - 180.0
    geo_lon = ((a.geographic_lon_deg + fraction * lon_delta + 180.0) % 360.0) - 180.0
    altitude = a.altitude_km + fraction * (b.altitude_km - a.altitude_km)
    mlt = _interpolate_circular_hour(a.mlt_hour, b.mlt_hour, fraction)
    return fraction, when, geo_lat, geo_lon, altitude, aacgm_lat, mlt


def _find_leg_crossing(leg: Sequence[Observation], channel: str, target: float) -> Optional[Tuple[Observation, Observation, float, float]]:
    """Legacy equatorward-most adjacent pair bracketing an absolute flux level."""

    candidates: List[Tuple[float, Observation, Observation, float, float]] = []
    for first, second in zip(leg[:-1], leg[1:]):
        f1 = first.flux_by_channel.get(channel)
        f2 = second.flux_by_channel.get(channel)
        if f1 is None or f2 is None:
            continue
        if (f1 - target) * (f2 - target) <= 0.0 and not math.isclose(f1, f2):
            equatorward_lat = min(abs(first.aacgm_lat_deg), abs(second.aacgm_lat_deg))
            candidates.append((equatorward_lat, first, second, f1, f2))
    if not candidates:
        return None
    _, first, second, f1, f2 = min(candidates, key=lambda item: item[0])
    return first, second, f1, f2


def _isotonic_nondecreasing(values: Sequence[float], weights: Optional[Sequence[float]] = None) -> List[float]:
    """Weighted pool-adjacent-violators fit with no third-party dependency."""

    if not values:
        return []
    if weights is None:
        weights = [1.0] * len(values)
    if len(weights) != len(values):
        raise ValueError("isotonic weights and values must have the same length")

    blocks: List[List[float]] = []  # [start, end, weighted_mean, total_weight]
    for index, (value, weight) in enumerate(zip(values, weights)):
        if not math.isfinite(value) or not math.isfinite(weight) or weight <= 0.0:
            raise ValueError("isotonic input must contain finite values and positive weights")
        blocks.append([float(index), float(index), float(value), float(weight)])
        while len(blocks) >= 2 and blocks[-2][2] > blocks[-1][2]:
            right = blocks.pop()
            left = blocks.pop()
            total_weight = left[3] + right[3]
            mean = (left[2] * left[3] + right[2] * right[3]) / total_weight
            blocks.append([left[0], right[1], mean, total_weight])

    fitted = [0.0] * len(values)
    for first, last, mean, _weight in blocks:
        for index in range(int(first), int(last) + 1):
            fitted[index] = mean
    return fitted


def _monotonic_level_crossing(
    latitudes: Sequence[float],
    fitted: Sequence[float],
    level: float,
) -> Optional[Tuple[float, int, float]]:
    """Interpolate a level crossing in a nondecreasing fitted profile.

    Returns ``(latitude, upper_index, fraction)`` where ``fraction`` is measured
    from ``upper_index-1`` to ``upper_index``.  Edge-only values are rejected so
    the result is always explicitly bracketed by two measured records.
    """

    if len(latitudes) != len(fitted) or len(latitudes) < 2:
        return None
    for upper in range(1, len(fitted)):
        lower = upper - 1
        y0, y1 = fitted[lower], fitted[upper]
        if y0 <= level <= y1 and not math.isclose(y0, y1):
            fraction = (level - y0) / (y1 - y0)
            x = latitudes[lower] + fraction * (latitudes[upper] - latitudes[lower])
            return x, upper, fraction
    return None


def _interpolate_observations(
    first: Observation,
    second: Observation,
    fraction: float,
) -> Tuple[datetime, float, float, float, float, float]:
    """Interpolate time and location using a precomputed profile fraction."""

    fraction = min(1.0, max(0.0, fraction))
    duration = second.time_utc - first.time_utc
    when = first.time_utc + timedelta(seconds=duration.total_seconds() * fraction)
    geo_lat = first.geographic_lat_deg + fraction * (second.geographic_lat_deg - first.geographic_lat_deg)
    lon_delta = ((second.geographic_lon_deg - first.geographic_lon_deg + 180.0) % 360.0) - 180.0
    geo_lon = ((first.geographic_lon_deg + fraction * lon_delta + 180.0) % 360.0) - 180.0
    altitude = first.altitude_km + fraction * (second.altitude_km - first.altitude_km)
    aacgm_lat = first.aacgm_lat_deg + fraction * (second.aacgm_lat_deg - first.aacgm_lat_deg)
    mlt = _interpolate_circular_hour(first.mlt_hour, second.mlt_hour, fraction)
    return when, geo_lat, geo_lon, altitude, aacgm_lat, mlt


def _robust_scale(values: Sequence[float]) -> float:
    """Return a robust one-sigma scale, with a standard-deviation fallback."""

    finite = [float(value) for value in values if math.isfinite(float(value))]
    if len(finite) < 2:
        return 0.0
    center = statistics.median(finite)
    mad = statistics.median(abs(value - center) for value in finite)
    scale = 1.4826 * mad
    if scale > 0.0:
        return scale
    return statistics.stdev(finite) if len(finite) > 1 else 0.0


def _append_quality_flag(flags: str, flag: str) -> str:
    parts = [item for item in str(flags).split(";") if item]
    if flag not in parts:
        parts.append(flag)
    return ";".join(parts)


def flag_p8_p9_cross_channel_outliers(
    crossings: Sequence[BoundaryCrossing],
    sigma_multiplier: float = 4.0,
    minimum_pairs: int = 6,
    fallback_max_separation_deg: float = 6.0,
) -> List[BoundaryCrossing]:
    """Flag anomalously equatorward P9 crossings using same-pass P8/P9 pairs.

    The comparison is observational only: for each pass leg, ``delta`` is the
    absolute-AACGM P8 boundary minus the P9 boundary.  The upper limit is the
    larger of a conservative fixed separation and a robust median-plus-MAD
    threshold.  Flagged P9 rows remain in the pass-level CSV but are excluded
    from robust binned diagnostics.
    """

    keyed: Dict[Tuple[str, str], Dict[str, BoundaryCrossing]] = {}
    for row in crossings:
        if row.channel in ("P8", "P9"):
            keyed.setdefault((row.pass_id, row.leg), {})[row.channel] = row
    deltas = [
        abs(pair["P8"].aacgm_lat_deg) - abs(pair["P9"].aacgm_lat_deg)
        for pair in keyed.values() if "P8" in pair and "P9" in pair
    ]
    if deltas:
        center = statistics.median(deltas)
        robust_sigma = _robust_scale(deltas)
        robust_limit = center + sigma_multiplier * robust_sigma
        upper_limit = max(fallback_max_separation_deg, robust_limit)
    else:
        center = robust_sigma = 0.0
        upper_limit = fallback_max_separation_deg
    # With very few pairs, do not allow a small accidental MAD to tighten the
    # fixed, physically conservative diagnostic limit.
    if len(deltas) < minimum_pairs:
        upper_limit = fallback_max_separation_deg

    result: List[BoundaryCrossing] = []
    for row in crossings:
        pair = keyed.get((row.pass_id, row.leg), {})
        if row.channel == "P9" and "P8" in pair and "P9" in pair:
            delta = abs(pair["P8"].aacgm_lat_deg) - abs(pair["P9"].aacgm_lat_deg)
            outlier = delta > upper_limit
            flags = _append_quality_flag(row.quality_flags, "P8_P9_CROSS_CHANNEL_CHECKED")
            if outlier:
                flags = _append_quality_flag(flags, "P9_CROSS_CHANNEL_OUTLIER")
            row = replace(
                row,
                quality_flags=flags,
                cross_channel_delta_p8_p9_deg=delta,
                cross_channel_outlier=outlier,
                aggregate_eligible=(row.aggregate_eligible and not outlier),
            )
        result.append(row)
    return result


def summarize_cross_channel_quality(crossings: Sequence[BoundaryCrossing]) -> Dict[str, object]:
    """Summarize P8/P9 pass-pair diagnostics for manifests and logs."""

    checked = [row for row in crossings
               if row.channel == "P9" and row.cross_channel_delta_p8_p9_deg is not None]
    deltas = [float(row.cross_channel_delta_p8_p9_deg) for row in checked]
    return {
        "n_p8_p9_pass_leg_pairs": len(checked),
        "n_p9_cross_channel_outliers": sum(row.cross_channel_outlier for row in checked),
        "median_p8_minus_p9_boundary_deg": statistics.median(deltas) if deltas else None,
        "robust_sigma_p8_minus_p9_boundary_deg": _robust_scale(deltas) if deltas else None,
    }


def _background_normalized_leg_crossing(
    leg: Sequence[Observation],
    channel: str,
    plateau: float,
    minimum_abs_lat_deg: float,
    polar_plateau_abs_lat_deg: float,
    minimum_background_samples: int,
    minimum_leg_samples: int,
    minimum_plateau_to_low_ratio: float,
    maximum_transition_width_deg: float,
    maximum_isotonic_rms: float,
    minimum_edge_margin_deg: float,
) -> Optional[Dict[str, object]]:
    """Fit one leg and return a background-normalized T50 plus diagnostics."""

    valid = [row for row in leg if channel in row.flux_by_channel]
    if len(valid) < minimum_leg_samples:
        return None

    background_rows = [
        row for row in valid
        if abs(row.aacgm_lat_deg) <= minimum_abs_lat_deg + 5.0
    ]
    if len(background_rows) < minimum_background_samples:
        return None
    background_values = [row.flux_by_channel[channel] for row in background_rows]
    background = statistics.median(background_values)
    if not math.isfinite(background) or background < 0.0 or plateau <= background:
        return None
    ratio = plateau / max(background, 1.0e-30)
    if ratio < minimum_plateau_to_low_ratio:
        return None

    ordered = sorted(valid, key=lambda row: abs(row.aacgm_lat_deg))
    latitudes = [abs(row.aacgm_lat_deg) for row in ordered]
    denominator = plateau - background
    normalized = [(row.flux_by_channel[channel] - background) / denominator for row in ordered]
    clipped = [min(1.5, max(-0.5, value)) for value in normalized]
    fitted = _isotonic_nondecreasing(clipped)
    rms = math.sqrt(statistics.fmean((a - b) ** 2 for a, b in zip(clipped, fitted)))
    if rms > maximum_isotonic_rms:
        return None

    t25 = _monotonic_level_crossing(latitudes, fitted, 0.25)
    t50 = _monotonic_level_crossing(latitudes, fitted, 0.50)
    t75 = _monotonic_level_crossing(latitudes, fitted, 0.75)
    if t25 is None or t50 is None or t75 is None:
        return None
    lat25, _, _ = t25
    lat50, upper, fraction = t50
    lat75, _, _ = t75
    transition_width = lat75 - lat25
    if transition_width <= 0.0 or transition_width > maximum_transition_width_deg:
        return None
    if (lat50 - latitudes[0] < minimum_edge_margin_deg
            or latitudes[-1] - lat50 < minimum_edge_margin_deg):
        return None

    # Count actual records supporting the central transition, rather than only
    # the two records used for T50 interpolation.  P8/P9 profiles with one jump
    # can otherwise look precise despite weak statistics.
    transition_support = sum(0.20 <= value <= 0.80 for value in fitted)
    polar_values = [
        row.flux_by_channel[channel] for row in valid
        if abs(row.aacgm_lat_deg) >= polar_plateau_abs_lat_deg
    ]
    background_noise = _robust_scale(background_values)
    polar_noise = _robust_scale(polar_values)
    combined_noise = math.hypot(background_noise, polar_noise)
    contrast_to_noise = (
        (plateau - background) / combined_noise if combined_noise > 0.0 else float("inf")
    )

    first, second = ordered[upper - 1], ordered[upper]
    when, geo_lat, geo_lon, altitude, aacgm_lat, mlt = _interpolate_observations(
        first, second, fraction
    )
    local_spacing = abs(latitudes[upper] - latitudes[upper - 1])
    uncertainty = max(
        0.25,
        0.5 * local_spacing,
        0.25 * transition_width,
        0.5 * rms * transition_width,
    )
    return {
        "first": first,
        "when": when,
        "geo_lat": geo_lat,
        "geo_lon": geo_lon,
        "altitude": altitude,
        "aacgm_lat": aacgm_lat,
        "mlt": mlt,
        "background": background,
        "ratio": ratio,
        "t25": lat25,
        "t50": lat50,
        "t75": lat75,
        "transition_width": transition_width,
        "isotonic_rms": rms,
        "uncertainty": uncertainty,
        "n_background_samples": len(background_rows),
        "n_leg_samples": len(ordered),
        "transition_support_samples": transition_support,
        "contrast_to_noise_ratio": contrast_to_noise,
        "threshold_flux": background + 0.5 * (plateau - background),
    }


def extract_boundary_crossings(
    observations: Sequence[Observation],
    event_id: str = "STORM_2006_12",
    minimum_abs_lat_deg: float = 45.0,
    polar_plateau_abs_lat_deg: float = 75.0,
    minimum_polar_samples: int = 4,
    minimum_plateau_to_low_ratio: float = 2.0,
    minimum_leg_samples: int = 4,
    minimum_background_samples: int = 3,
    crossing_method: str = "BACKGROUND_NORMALIZED_ISOTONIC",
    maximum_transition_width_deg: float = 15.0,
    maximum_isotonic_rms: float = 0.35,
    minimum_edge_margin_deg: float = 0.5,
    minimum_primary_transition_samples: int = 2,
    minimum_diagnostic_transition_samples: int = 3,
    minimum_diagnostic_contrast_to_noise: float = 3.0,
    p8_p9_outlier_sigma: float = 4.0,
    p8_p9_minimum_pairs: int = 6,
    p8_p9_fallback_max_separation_deg: float = 6.0,
) -> List[BoundaryCrossing]:
    """Extract quality-controlled cutoff boundaries from all valid polar passes.

    ``BACKGROUND_NORMALIZED_ISOTONIC`` is the production method.  The legacy
    ``HALF_POLAR_PLATEAU`` mode is retained only to diagnose the effect of the
    historical uncorrected threshold.
    """

    crossing_method = crossing_method.upper()
    if crossing_method not in ("BACKGROUND_NORMALIZED_ISOTONIC", "HALF_POLAR_PLATEAU"):
        raise ValueError("unknown crossing_method: %s" % crossing_method)

    passes = split_polar_passes(
        observations,
        minimum_abs_lat_deg=minimum_abs_lat_deg,
        required_polar_abs_lat_deg=polar_plateau_abs_lat_deg,
    )
    crossings: List[BoundaryCrossing] = []

    for pass_index, polar_pass in enumerate(passes):
        peak_index = max(range(len(polar_pass)), key=lambda i: abs(polar_pass[i].aacgm_lat_deg))
        legs = {
            "INBOUND": polar_pass[: peak_index + 1],
            "OUTBOUND": polar_pass[peak_index:],
        }
        hemisphere = "N" if polar_pass[peak_index].aacgm_lat_deg >= 0.0 else "S"
        pass_id = "%s_%s_%04d" % (
            polar_pass[0].satellite.replace("-", ""),
            polar_pass[peak_index].time_utc.strftime("%Y%m%dT%H%M%S"),
            pass_index,
        )

        for channel, threshold_mev in CHANNEL_THRESHOLDS_MEV.items():
            polar_fluxes = [
                row.flux_by_channel[channel]
                for row in polar_pass
                if abs(row.aacgm_lat_deg) >= polar_plateau_abs_lat_deg
                and channel in row.flux_by_channel
            ]
            if len(polar_fluxes) < minimum_polar_samples:
                continue
            plateau = statistics.median(polar_fluxes)
            if not math.isfinite(plateau) or plateau <= 0.0:
                continue

            for leg_name, leg in legs.items():
                flags: List[str] = []
                if channel in ("P8", "P9"):
                    flags.extend([
                        "DIAGNOSTIC_INTEGRAL_RESPONSE_NOT_FOLDED",
                        "P8_P9_SUBCOMMUTATED_ARCHIVE_CHANNEL",
                    ])

                if crossing_method == "BACKGROUND_NORMALIZED_ISOTONIC":
                    fit = _background_normalized_leg_crossing(
                        leg=leg,
                        channel=channel,
                        plateau=plateau,
                        minimum_abs_lat_deg=minimum_abs_lat_deg,
                        polar_plateau_abs_lat_deg=polar_plateau_abs_lat_deg,
                        minimum_background_samples=minimum_background_samples,
                        minimum_leg_samples=minimum_leg_samples,
                        minimum_plateau_to_low_ratio=minimum_plateau_to_low_ratio,
                        maximum_transition_width_deg=maximum_transition_width_deg,
                        maximum_isotonic_rms=maximum_isotonic_rms,
                        minimum_edge_margin_deg=minimum_edge_margin_deg,
                    )
                    if fit is None:
                        continue
                    first = fit["first"]
                    flags.append("BACKGROUND_NORMALIZED_T50")
                    threshold_flux = float(fit["threshold_flux"])
                    background = float(fit["background"])
                    ratio = float(fit["ratio"])
                    lat25 = float(fit["t25"])
                    lat50 = float(fit["t50"])
                    lat75 = float(fit["t75"])
                    width = float(fit["transition_width"])
                    isotonic_rms = float(fit["isotonic_rms"])
                    uncertainty = float(fit["uncertainty"])
                    n_background = int(fit["n_background_samples"])
                    n_leg = int(fit["n_leg_samples"])
                    transition_support = int(fit["transition_support_samples"])
                    contrast_to_noise = float(fit["contrast_to_noise_ratio"])
                    required_support = (minimum_primary_transition_samples
                                        if CHANNEL_VALIDATION_ROLE[channel] == "PRIMARY"
                                        else minimum_diagnostic_transition_samples)
                    aggregate_eligible = transition_support >= required_support
                    if transition_support < required_support:
                        flags.append("INSUFFICIENT_TRANSITION_SUPPORT")
                    if (CHANNEL_VALIDATION_ROLE[channel] == "DIAGNOSTIC"
                            and contrast_to_noise < minimum_diagnostic_contrast_to_noise):
                        aggregate_eligible = False
                        flags.append("LOW_DIAGNOSTIC_CONTRAST_TO_NOISE")
                    when = fit["when"]
                    geo_lat = float(fit["geo_lat"])
                    geo_lon = float(fit["geo_lon"])
                    altitude = float(fit["altitude"])
                    aacgm_lat = float(fit["aacgm_lat"])
                    mlt = float(fit["mlt"])
                else:
                    if len(leg) < minimum_leg_samples:
                        continue
                    half_level = 0.5 * plateau
                    bracket = _find_leg_crossing(leg, channel, half_level)
                    if bracket is None:
                        continue
                    first, second, f1, f2 = bracket
                    (_fraction, when, geo_lat, geo_lon, altitude,
                     aacgm_lat, mlt) = _interpolate_crossing(first, second, f1, f2, half_level)
                    spacing = abs(abs(second.aacgm_lat_deg) - abs(first.aacgm_lat_deg))
                    uncertainty = max(0.25, 0.5 * spacing)
                    background_rows = [
                        row for row in leg
                        if channel in row.flux_by_channel
                        and abs(row.aacgm_lat_deg) <= minimum_abs_lat_deg + 5.0
                    ]
                    background = (statistics.median(row.flux_by_channel[channel] for row in background_rows)
                                  if background_rows else None)
                    ratio = (plateau / max(background, 1.0e-30)
                             if background is not None else None)
                    threshold_flux = half_level
                    lat25 = lat50 = lat75 = width = isotonic_rms = None
                    n_background = len(background_rows)
                    n_leg = len(leg)
                    transition_support = 0
                    contrast_to_noise = None
                    aggregate_eligible = False
                    flags.append("LEGACY_UNCORRECTED_HALF_POLAR_PLATEAU")

                crossings.append(BoundaryCrossing(
                    event_id=event_id,
                    satellite=first.satellite,
                    channel=channel,
                    validation_role=CHANNEL_VALIDATION_ROLE[channel],
                    energy_threshold_mev=threshold_mev,
                    assigned_rigidity_gv=proton_rigidity_gv_from_kinetic_energy_mev(threshold_mev),
                    mapping_method="NOMINAL_INTEGRAL_CHANNEL_LOWER_THRESHOLD",
                    crossing_method=crossing_method,
                    hemisphere=hemisphere,
                    pass_id=pass_id,
                    leg=leg_name,
                    crossing_time_utc=when,
                    geographic_lat_deg=geo_lat,
                    geographic_lon_deg=geo_lon,
                    altitude_km=altitude,
                    aacgm_lat_deg=aacgm_lat,
                    mlt_hour=mlt,
                    polar_plateau_flux=plateau,
                    half_plateau_flux=threshold_flux,
                    low_latitude_flux=background,
                    normalized_t25_aacgm_lat_deg=lat25,
                    normalized_t50_aacgm_lat_deg=lat50,
                    normalized_t75_aacgm_lat_deg=lat75,
                    transition_width_deg=width,
                    isotonic_rms=isotonic_rms,
                    plateau_to_background_ratio=ratio,
                    boundary_uncertainty_deg=uncertainty,
                    n_polar_samples=len(polar_fluxes),
                    n_background_samples=n_background,
                    n_leg_samples=n_leg,
                    quality_flags=";".join(flags),
                    source_file=first.source_file,
                    source_sha256=first.source_sha256,
                    transition_support_samples=transition_support,
                    contrast_to_noise_ratio=contrast_to_noise,
                    aggregate_eligible=aggregate_eligible,
                ))
    crossings = flag_p8_p9_cross_channel_outliers(
        crossings,
        sigma_multiplier=p8_p9_outlier_sigma,
        minimum_pairs=p8_p9_minimum_pairs,
        fallback_max_separation_deg=p8_p9_fallback_max_separation_deg,
    )
    crossings.sort(key=lambda row: (row.crossing_time_utc, row.channel, row.hemisphere, row.satellite, row.leg))
    return crossings


def nearest_mlt_bin(mlt_hour: float, bin_centers: Sequence[float]) -> float:
    """Return the circularly nearest configured MLT-bin center."""

    return min(bin_centers, key=lambda center: abs(((mlt_hour - center + 12.0) % 24.0) - 12.0))


def hourly_window_midpoints(start: datetime, end: datetime, step_hours: float = 1.0) -> List[datetime]:
    """Generate whole-step UTC window centers spanning an event interval."""

    first = start.replace(minute=0, second=0, microsecond=0)
    if first < start:
        first += timedelta(hours=step_hours)
    step = timedelta(hours=step_hours)
    result: List[datetime] = []
    current = first
    while current <= end:
        result.append(current)
        current += step
    return result


def aggregate_crossings(
    crossings: Sequence[BoundaryCrossing],
    event_start: datetime,
    event_end: datetime,
    window_hours: float = 2.0,
    step_hours: float = 1.0,
    mlt_bin_centers: Sequence[float] = tuple(float(v) for v in range(0, 24, 3)),
    minimum_crossings_per_cell: int = 2,
    minimum_diagnostic_crossings_per_cell: int = 2,
    minimum_distinct_pass_legs_per_cell: int = 2,
    minimum_diagnostic_distinct_pass_legs_per_cell: int = 2,
    minimum_diagnostic_distinct_satellites_per_cell: int = 2,
    acceptance_window_stride_hours: float = 2.0,
    source_ref: str = "Dmitriev et al. (2010), doi:10.1029/2010JA015380",
) -> List[ReferenceCell]:
    """Aggregate crossings without hiding sparse or anomalous diagnostics.

    Every cell with at least one measured crossing retains a boundary estimate.
    P6/P7 control PASS/FAIL only when their independent-window requirements are
    met.  P8/P9 receive a separate ``diagnostic_eligible`` flag requiring robust
    pass-leg and multi-satellite support and excluding cross-channel outliers.
    Sparse diagnostic estimates remain in the reference and comparison CSV but
    are not connected into the main diagnostic curves.
    """

    half_window = timedelta(hours=0.5 * window_hours)
    midpoints = hourly_window_midpoints(event_start, event_end, step_hours)
    cells: List[ReferenceCell] = []

    for midpoint in midpoints:
        start = midpoint - half_window
        end = midpoint + half_window
        offset_hours = (midpoint - event_start).total_seconds() / 3600.0
        q = offset_hours / acceptance_window_stride_hours
        independent_window = math.isclose(q, round(q), rel_tol=0.0, abs_tol=1.0e-8)

        for channel, threshold_mev in CHANNEL_THRESHOLDS_MEV.items():
            rigidity = proton_rigidity_gv_from_kinetic_energy_mev(threshold_mev)
            role = CHANNEL_VALIDATION_ROLE[channel]
            for hemisphere in ("N", "S"):
                for mlt_center in mlt_bin_centers:
                    selected_all = [
                        row for row in crossings
                        if row.channel == channel
                        and row.hemisphere == hemisphere
                        and start <= row.crossing_time_utc < end
                        and math.isclose(nearest_mlt_bin(row.mlt_hour, mlt_bin_centers), mlt_center)
                    ]
                    selected_eligible = [row for row in selected_all if row.aggregate_eligible]
                    eligible_pass_legs = {row.pass_id + ":" + row.leg for row in selected_eligible}
                    eligible_satellites = {row.satellite for row in selected_eligible}
                    all_pass_legs = {row.pass_id + ":" + row.leg for row in selected_all}
                    all_satellites = {row.satellite for row in selected_all}

                    primary_robust = (
                        role == "PRIMARY"
                        and len(selected_eligible) >= minimum_crossings_per_cell
                        and len(eligible_pass_legs) >= minimum_distinct_pass_legs_per_cell
                    )
                    diagnostic_robust = (
                        role == "DIAGNOSTIC"
                        and len(selected_eligible) >= minimum_diagnostic_crossings_per_cell
                        and len(eligible_pass_legs) >= minimum_diagnostic_distinct_pass_legs_per_cell
                        and len(eligible_satellites) >= minimum_diagnostic_distinct_satellites_per_cell
                    )
                    acceptance_eligible = (
                        primary_robust and independent_window
                        and all(row.crossing_method == "BACKGROUND_NORMALIZED_ISOTONIC"
                                for row in selected_eligible)
                    )
                    diagnostic_eligible = bool(diagnostic_robust)

                    # Prefer quality-eligible crossings, but preserve a sparse
                    # diagnostic estimate when no robust set exists.
                    estimate_rows = selected_eligible if selected_eligible else selected_all
                    latitudes = [abs(row.aacgm_lat_deg) for row in estimate_rows]
                    altitudes = [row.altitude_km for row in estimate_rows]
                    widths = [row.transition_width_deg for row in estimate_rows
                              if row.transition_width_deg is not None]
                    supports = [row.transition_support_samples for row in estimate_rows]
                    cnr_values = [float(row.contrast_to_noise_ratio) for row in estimate_rows
                                  if row.contrast_to_noise_ratio is not None
                                  and math.isfinite(float(row.contrast_to_noise_ratio))]
                    if estimate_rows:
                        boundary = statistics.median(latitudes)
                        if len(latitudes) > 1:
                            robust_sigma = 1.4826 * statistics.median(
                                abs(value - boundary) for value in latitudes)
                        else:
                            robust_sigma = estimate_rows[0].boundary_uncertainty_deg
                        sigma = max(
                            0.25,
                            robust_sigma,
                            statistics.fmean(row.boundary_uncertainty_deg for row in estimate_rows),
                        )
                        altitude = statistics.fmean(altitudes)
                    else:
                        boundary = sigma = None
                        altitude = 850.0

                    n_outliers = sum(row.cross_channel_outlier for row in selected_all)
                    if not selected_all:
                        quality_status = "MISSING"
                    elif acceptance_eligible:
                        quality_status = "PRIMARY_ACCEPTANCE"
                    elif role == "PRIMARY":
                        quality_status = "PRIMARY_PLOT_ONLY"
                    elif diagnostic_eligible:
                        quality_status = "DIAGNOSTIC_ROBUST"
                    elif n_outliers:
                        quality_status = "DIAGNOSTIC_CROSS_CHANNEL_OUTLIER"
                    else:
                        quality_status = "DIAGNOSTIC_SPARSE"

                    satellites = ";".join(sorted(all_satellites))
                    median_width = statistics.median(widths) if widths else None
                    notes = (
                        "Background-normalized isotonic T50; median of pass-leg crossings; "
                        "nominal lower-threshold rigidity."
                    )
                    if role == "DIAGNOSTIC":
                        notes += (
                            " P8/P9 are diagnostic-only until detector-response folding; "
                            "robust curves require multiple pass legs and satellites."
                        )
                    if quality_status in ("DIAGNOSTIC_SPARSE", "DIAGNOSTIC_CROSS_CHANNEL_OUTLIER"):
                        notes += " Retained in detailed output but excluded from robust diagnostic means."
                    if role == "PRIMARY" and not acceptance_eligible and estimate_rows:
                        notes += " Retained for plotting but excluded from the independent-window acceptance gate."

                    cells.append(ReferenceCell(
                        event_id="STORM_2006_12",
                        interval_midpoint_utc=midpoint,
                        interval_start_utc=start,
                        interval_end_utc=end,
                        rigidity_gv=rigidity,
                        energy_threshold_mev=threshold_mev,
                        channel=channel,
                        validation_role=role,
                        acceptance_eligible=acceptance_eligible,
                        hemisphere=hemisphere,
                        mlt_hour=float(mlt_center),
                        boundary_aacgm_lat_deg=boundary,
                        sigma_deg=sigma,
                        altitude_km=altitude,
                        n_crossings=len(selected_all),
                        n_distinct_pass_legs=len(all_pass_legs),
                        n_distinct_satellites=len(all_satellites),
                        median_transition_width_deg=median_width,
                        background_corrected=bool(estimate_rows) and all(
                            row.crossing_method == "BACKGROUND_NORMALIZED_ISOTONIC"
                            for row in estimate_rows
                        ),
                        satellites=satellites,
                        missing=not bool(estimate_rows),
                        source="POES_NCEI_LEVEL2_16SEC",
                        source_ref=source_ref,
                        notes=notes,
                        diagnostic_eligible=diagnostic_eligible,
                        quality_status=quality_status,
                        n_aggregate_eligible_crossings=len(selected_eligible),
                        n_cross_channel_outliers=n_outliers,
                        median_transition_support_samples=(
                            statistics.median(supports) if supports else None),
                        median_contrast_to_noise_ratio=(
                            statistics.median(cnr_values) if cnr_values else None),
                    ))
    return cells


def _format_utc(value: datetime) -> str:
    return value.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def write_crossings_csv(crossings: Sequence[BoundaryCrossing], output: Path) -> None:
    """Write the primary, pass-level observational product."""

    output.parent.mkdir(parents=True, exist_ok=True)
    rows: List[Dict[str, object]] = []
    for crossing in crossings:
        row = asdict(crossing)
        row["crossing_time_utc"] = _format_utc(crossing.crossing_time_utc)
        rows.append(row)
    fieldnames = list(rows[0].keys()) if rows else [field.name for field in BoundaryCrossing.__dataclass_fields__.values()]
    with output.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


@contextlib.contextmanager
def _open_reference_text_output(output: Path):
    """Open a reference destination as deterministic UTF-8 text.

    Gzip normally embeds the current wall-clock time in its header.  Setting
    ``mtime=0`` and omitting the original filename makes repeated builds from
    identical measurements byte-for-byte reproducible, so the SHA-256 recorded
    by the reference summary is meaningful.
    """

    if output.name.lower().endswith(".gz"):
        with output.open("wb") as raw_stream:
            with gzip.GzipFile(
                filename="", mode="wb", fileobj=raw_stream, mtime=0
            ) as compressed_stream:
                with io.TextIOWrapper(
                    compressed_stream, encoding="utf-8", newline=""
                ) as text_stream:
                    yield text_stream
    else:
        with output.open("w", encoding="utf-8", newline="") as text_stream:
            yield text_stream


def write_reference_csv(cells: Sequence[ReferenceCell], output: Path, manifest_sha256: str = "") -> None:
    """Write the windowed reference consumed by ``run_C10.py``.

    The production C10 reference is stored as
    ``reference_C10_poes_meped_boundary.csv.gz``.  A ``*.gz`` destination is
    written directly as deterministic UTF-8 gzip-compressed CSV; plain CSV
    remains supported for small developer fixtures.
    """

    output.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "event_id", "interval_midpoint_utc", "interval_start_utc", "interval_end_utc",
        "rigidity_gv", "energy_threshold_mev", "channel", "validation_role",
        "acceptance_eligible", "hemisphere", "mlt_hour",
        "boundary_aacgm_lat_deg", "sigma_deg", "altitude_km", "n_crossings",
        "n_distinct_pass_legs", "n_distinct_satellites",
        "median_transition_width_deg", "background_corrected",
        "diagnostic_eligible", "quality_status",
        "n_aggregate_eligible_crossings", "n_cross_channel_outliers",
        "median_transition_support_samples", "median_contrast_to_noise_ratio",
        "satellites", "missing", "source", "source_ref", "notes",
    ]
    with _open_reference_text_output(output) as stream:
        stream.write("# C10 real POES/MetOp SEM-2 MEPED cutoff-boundary reference\n")
        stream.write("# reference_kind=POES_NCEI_LEVEL2_16SEC\n")
        stream.write("# reference_schema=C10_POES_T50_V3\n")
        stream.write("# boundary_definition=background_normalized_isotonic_T50\n")
        stream.write("# rigidity_mapping=nominal_integral_channel_lower_threshold\n"
                     "# validation_gate=P6_P7_primary_independent_windows;P8_P9_diagnostic\n")
        if manifest_sha256:
            stream.write(f"# provenance_manifest_sha256={manifest_sha256}\n")
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        for cell in cells:
            writer.writerow({
                "event_id": cell.event_id,
                "interval_midpoint_utc": _format_utc(cell.interval_midpoint_utc),
                "interval_start_utc": _format_utc(cell.interval_start_utc),
                "interval_end_utc": _format_utc(cell.interval_end_utc),
                "rigidity_gv": f"{cell.rigidity_gv:.9f}",
                "energy_threshold_mev": f"{cell.energy_threshold_mev:.3f}",
                "channel": cell.channel,
                "validation_role": cell.validation_role,
                "acceptance_eligible": "TRUE" if cell.acceptance_eligible else "FALSE",
                "hemisphere": cell.hemisphere,
                "mlt_hour": f"{cell.mlt_hour:.1f}",
                "boundary_aacgm_lat_deg": "" if cell.boundary_aacgm_lat_deg is None else f"{cell.boundary_aacgm_lat_deg:.5f}",
                "sigma_deg": "" if cell.sigma_deg is None else f"{cell.sigma_deg:.5f}",
                "altitude_km": f"{cell.altitude_km:.3f}",
                "n_crossings": cell.n_crossings,
                "n_distinct_pass_legs": cell.n_distinct_pass_legs,
                "n_distinct_satellites": cell.n_distinct_satellites,
                "median_transition_width_deg": ("" if cell.median_transition_width_deg is None
                                                else f"{cell.median_transition_width_deg:.5f}"),
                "background_corrected": "TRUE" if cell.background_corrected else "FALSE",
                "diagnostic_eligible": "TRUE" if cell.diagnostic_eligible else "FALSE",
                "quality_status": cell.quality_status,
                "n_aggregate_eligible_crossings": cell.n_aggregate_eligible_crossings,
                "n_cross_channel_outliers": cell.n_cross_channel_outliers,
                "median_transition_support_samples": (
                    "" if cell.median_transition_support_samples is None
                    else f"{cell.median_transition_support_samples:.3f}"),
                "median_contrast_to_noise_ratio": (
                    "" if cell.median_contrast_to_noise_ratio is None
                    else f"{cell.median_contrast_to_noise_ratio:.5f}"),
                "satellites": cell.satellites,
                "missing": "TRUE" if cell.missing else "FALSE",
                "source": cell.source,
                "source_ref": cell.source_ref,
                "notes": cell.notes,
            })


def write_manifest(
    source_files: Sequence[Path],
    crossings: Sequence[BoundaryCrossing],
    cells: Sequence[ReferenceCell],
    configuration: Mapping[str, object],
    output: Path,
) -> str:
    """Write a complete machine-readable provenance manifest and return its SHA-256."""

    output.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "product": "C10_POES_NCEI_REFERENCE",
        "created_utc": _format_utc(datetime.now(timezone.utc)),
        "configuration": dict(configuration),
        "source_files": [
            {"path": str(path), "name": path.name, "sha256": sha256_file(path), "size_bytes": path.stat().st_size}
            for path in source_files
        ],
        "channel_mapping": {
            channel: {
                "nominal_lower_threshold_mev": energy,
                "assigned_rigidity_gv": proton_rigidity_gv_from_kinetic_energy_mev(energy),
                "validation_role": CHANNEL_VALIDATION_ROLE[channel],
            }
            for channel, energy in CHANNEL_THRESHOLDS_MEV.items()
        },
        "n_crossings": len(crossings),
        "n_reference_cells": len(cells),
        "n_nonmissing_reference_cells": sum(not cell.missing for cell in cells),
        "n_acceptance_eligible_cells": sum(cell.acceptance_eligible for cell in cells),
        "n_diagnostic_eligible_cells": sum(cell.diagnostic_eligible for cell in cells),
        "n_sparse_diagnostic_cells": sum(
            cell.quality_status in ("DIAGNOSTIC_SPARSE", "DIAGNOSTIC_CROSS_CHANNEL_OUTLIER")
            for cell in cells),
        "n_cross_channel_outlier_cells": sum(cell.n_cross_channel_outliers > 0 for cell in cells),
        "cross_channel_quality": summarize_cross_channel_quality(crossings),
        "n_primary_crossings": sum(row.validation_role == "PRIMARY" for row in crossings),
        "n_diagnostic_crossings": sum(row.validation_role == "DIAGNOSTIC" for row in crossings),
    }
    output.write_text(json.dumps(payload, indent=2) + "\n")
    return sha256_file(output)
