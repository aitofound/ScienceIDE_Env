#!/usr/bin/env python3
"""Shared, dependency-light utilities for the December 2006 study.

The production runners intentionally keep the AMPS execution and the scientific
postprocessing separate.  This module contains only deterministic operations
needed by both sides: UTC handling, TS05-driver parsing/interpolation, small
statistics helpers, CSV writing, and the first-harmonic boundary fit.  Keeping
these functions here prevents the observation and morphology scripts from
quietly adopting different time or sign conventions.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import statistics
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple


UTC = timezone.utc

# Keep the run label in one place. Every executable script imports this value
# rather than independently spelling an output directory, which prevents the
# observation, morphology, sensitivity, and plotting stages from drifting into
# different trees after the study is installed in the AMPS repository.
STUDY_RUN_NAME = "dec2006_ts05_cutoff_erosion"


def parse_utc(value: str) -> datetime:
    """Parse ISO-8601 text and always return an aware UTC datetime."""
    text = value.strip()
    if text.endswith("Z"):
        text = text[:-1] + "+00:00"
    result = datetime.fromisoformat(text)
    if result.tzinfo is None:
        result = result.replace(tzinfo=UTC)
    return result.astimezone(UTC)


def format_utc(value: datetime) -> str:
    """Use one canonical timestamp representation in every generated table."""
    return value.astimezone(UTC).strftime("%Y-%m-%dT%H:%M:%SZ")


def package_root() -> Path:
    """Return the study-package root independent of the launch directory."""
    return Path(__file__).resolve().parents[1]


def codebase_root() -> Path:
    """Locate the AMPS repository root, with a safe standalone fallback.

    In the intended installation
    ``AMPS/srcEarth/studies/dec2006_ts05_cutoff_erosion``, the first ancestor
    containing a ``srcEarth`` directory is the repository root. The packaged
    study is also usable before installation; in that case no such ancestor
    exists and the launch directory is used. This makes the default intuitive
    both for Git checkouts and for an independently extracted archive.
    """
    root = package_root()
    for candidate in (root, *root.parents):
        if (candidate / "srcEarth").is_dir():
            return candidate.resolve()
    return Path.cwd().resolve()


def default_output_root() -> Path:
    """Return the shared repository-level output directory for this study."""
    return codebase_root() / "test_output" / STUDY_RUN_NAME


def resolve_output_path(path: Path) -> Path:
    """Resolve a user output path relative to the launch directory.

    Generated subcommands receive absolute paths, so later working-directory
    changes cannot redirect output into the vendored C9 or C10 directories.
    """
    return path.expanduser().resolve()


def load_config(path: Optional[Path] = None) -> Tuple[Path, Dict[str, object]]:
    """Load the JSON configuration and return both its root and content."""
    root = package_root()
    config_path = path or (root / "config" / "study.json")
    config_path = config_path.expanduser().resolve()
    with config_path.open(encoding="utf-8") as stream:
        config = json.load(stream)
    return root, config


def sha256(path: Path) -> str:
    """Calculate a streaming SHA-256 digest without loading large files."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


@dataclass(frozen=True)
class DriverRow:
    """One normalized record from the 20-column AMPS TS05 driver table."""

    epoch: datetime
    bx_nt: float
    by_nt: float
    bz_nt: float
    vx_km_s: float
    vy_km_s: float
    vz_km_s: float
    density_cm3: float
    temperature_k: float
    symh_nt: float
    imf_flag: int
    sw_flag: int
    tilt_rad: float
    pdyn_npa: float
    w1: float
    w2: float
    w3: float
    w4: float
    w5: float
    w6: float


DRIVER_FIELDS = (
    "bx_nt", "by_nt", "bz_nt", "vx_km_s", "vy_km_s", "vz_km_s",
    "density_cm3", "temperature_k", "symh_nt", "imf_flag", "sw_flag",
    "tilt_rad", "pdyn_npa", "w1", "w2", "w3", "w4", "w5", "w6",
)


def read_driver(path: Path) -> List[DriverRow]:
    """Read and strictly validate the AMPS-formatted five-minute TS05 driver.

    Unknown or partial rows are rejected.  Silent column shifts in a driver file
    can produce physically plausible but incorrect fields, so the strict width
    check is part of the scientific provenance contract.
    """
    rows: List[DriverRow] = []
    with path.open(encoding="utf-8") as stream:
        for line_number, raw in enumerate(stream, start=1):
            text = raw.strip()
            if not text or text.startswith("#"):
                continue
            tokens = text.split()
            if len(tokens) != 20:
                raise ValueError(
                    f"{path}:{line_number}: expected 20 fields, found {len(tokens)}"
                )
            values = [float(value) for value in tokens[1:]]
            rows.append(DriverRow(
                parse_utc(tokens[0]), *values[:9], int(values[9]), int(values[10]),
                *values[11:]
            ))
    if len(rows) < 2:
        raise ValueError(f"{path}: fewer than two driver rows")
    for previous, current in zip(rows, rows[1:]):
        seconds = (current.epoch - previous.epoch).total_seconds()
        if seconds <= 0:
            raise ValueError(f"{path}: driver epochs are not strictly increasing")
        if seconds > 900:
            raise ValueError(f"{path}: unsupported driver gap of {seconds:g} s")
    return rows


def interpolate_driver(rows: Sequence[DriverRow], epoch: datetime) -> DriverRow:
    """Linearly interpolate instantaneous driver quantities at ``epoch``.

    Quality flags are conservatively combined with ``min`` so a bad endpoint
    cannot be hidden.  Extrapolation is forbidden because TS05 history inputs
    outside the supplied table are undefined for this reproducible experiment.
    """
    epoch = epoch.astimezone(UTC)
    if epoch < rows[0].epoch or epoch > rows[-1].epoch:
        raise ValueError(f"epoch {format_utc(epoch)} is outside driver coverage")
    for left, right in zip(rows, rows[1:]):
        if epoch == left.epoch:
            return left
        if left.epoch < epoch <= right.epoch:
            if epoch == right.epoch:
                return right
            fraction = (epoch - left.epoch).total_seconds() / (
                right.epoch - left.epoch
            ).total_seconds()
            numeric = []
            for name in DRIVER_FIELDS:
                if name in ("imf_flag", "sw_flag"):
                    numeric.append(min(getattr(left, name), getattr(right, name)))
                else:
                    numeric.append(
                        getattr(left, name)
                        + fraction * (getattr(right, name) - getattr(left, name))
                    )
            return DriverRow(epoch, *numeric)
    return rows[-1]


def driver_dict(row: DriverRow) -> Dict[str, object]:
    """Convert a driver record to a CSV/JSON-friendly ordered mapping."""
    result: Dict[str, object] = {"epoch_utc": format_utc(row.epoch)}
    for name in DRIVER_FIELDS:
        result[name] = getattr(row, name)
    return result


def write_csv(path: Path, rows: Sequence[Mapping[str, object]]) -> None:
    """Write a union-schema CSV atomically enough for batch postprocessing."""
    path.parent.mkdir(parents=True, exist_ok=True)
    fields: List[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    with path.open("w", newline="", encoding="utf-8") as stream:
        if not fields:
            stream.write("")
            return
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read an ordinary CSV as dictionaries."""
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def finite_float(value: object) -> Optional[float]:
    """Return a finite float or ``None`` for missing/nonfinite input."""
    if value is None or str(value).strip() in ("", "None", "nan", "NaN"):
        return None
    result = float(value)
    return result if math.isfinite(result) else None


def pearson(x: Sequence[float], y: Sequence[float]) -> Optional[float]:
    """Pearson correlation with explicit constant-series handling."""
    if len(x) != len(y) or len(x) < 2:
        return None
    mx, my = statistics.fmean(x), statistics.fmean(y)
    dx = [value - mx for value in x]
    dy = [value - my for value in y]
    denominator = math.sqrt(sum(value * value for value in dx)
                            * sum(value * value for value in dy))
    if denominator == 0.0:
        return None
    return sum(a * b for a, b in zip(dx, dy)) / denominator


def residual_metrics(observed: Sequence[float], modeled: Sequence[float]) -> Dict[str, object]:
    """Return paired residual metrics using model minus observation signs."""
    if len(observed) != len(modeled):
        raise ValueError("observed and modeled arrays have unequal length")
    residuals = [model - obs for obs, model in zip(observed, modeled)]
    if not residuals:
        return {"n": 0, "bias_deg": None, "mae_deg": None,
                "rmse_deg": None, "correlation": None}
    return {
        "n": len(residuals),
        "bias_deg": statistics.fmean(residuals),
        "mae_deg": statistics.fmean(abs(value) for value in residuals),
        "rmse_deg": math.sqrt(statistics.fmean(value * value for value in residuals)),
        "correlation": pearson(observed, modeled),
    }


def solve_3x3(matrix: Sequence[Sequence[float]], rhs: Sequence[float]) -> Tuple[float, float, float]:
    """Solve a nonsingular 3x3 system with partial-pivot Gaussian elimination."""
    augmented = [list(row) + [float(value)] for row, value in zip(matrix, rhs)]
    for column in range(3):
        pivot = max(range(column, 3), key=lambda row: abs(augmented[row][column]))
        if abs(augmented[pivot][column]) < 1.0e-12:
            raise ValueError("singular first-harmonic fit")
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        scale = augmented[column][column]
        augmented[column] = [value / scale for value in augmented[column]]
        for row in range(3):
            if row == column:
                continue
            factor = augmented[row][column]
            augmented[row] = [
                value - factor * base
                for value, base in zip(augmented[row], augmented[column])
            ]
    return tuple(augmented[row][3] for row in range(3))  # type: ignore[return-value]


def fit_first_harmonic(mlt_hours: Sequence[float], latitudes_deg: Sequence[float],
                       weights: Optional[Sequence[float]] = None) -> Dict[str, float]:
    """Fit latitude = mean + c*cos(omega*MLT) + s*sin(omega*MLT).

    The reported phase is the MLT at which the fitted latitude is most
    poleward.  The amplitude is nonnegative.  At least three distinct MLT
    sectors are required, which prevents a visually suggestive but singular fit.
    """
    if len(mlt_hours) != len(latitudes_deg) or len(mlt_hours) < 3:
        raise ValueError("first-harmonic fit requires at least three paired cells")
    w = list(weights) if weights is not None else [1.0] * len(mlt_hours)
    if len(w) != len(mlt_hours):
        raise ValueError("harmonic weights have wrong length")
    rows = []
    for mlt in mlt_hours:
        angle = 2.0 * math.pi * mlt / 24.0
        rows.append((1.0, math.cos(angle), math.sin(angle)))
    normal = [[sum(weight * a[i] * a[j] for weight, a in zip(w, rows))
               for j in range(3)] for i in range(3)]
    target = [sum(weight * a[i] * value
                  for weight, a, value in zip(w, rows, latitudes_deg))
              for i in range(3)]
    mean, cosine, sine = solve_3x3(normal, target)
    amplitude = math.hypot(cosine, sine)
    phase = (math.atan2(sine, cosine) * 24.0 / (2.0 * math.pi)) % 24.0
    fitted = [mean + cosine * row[1] + sine * row[2] for row in rows]
    rms = math.sqrt(statistics.fmean(
        (value - prediction) ** 2
        for value, prediction in zip(latitudes_deg, fitted)
    ))
    return {
        "mean_latitude_deg": mean,
        "amplitude_deg": amplitude,
        "phase_mlt_hour": phase,
        "fit_rms_deg": rms,
    }


def quantile(values: Sequence[float], probability: float) -> float:
    """Linear-interpolated sample quantile used for bootstrap intervals."""
    if not values:
        raise ValueError("quantile of empty sequence")
    ordered = sorted(values)
    position = max(0.0, min(1.0, probability)) * (len(ordered) - 1)
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction
