#!/usr/bin/env python3
"""Build the public-data C19A GOES-13/15 EPEAD east-west reference.

The default event is the 17 May 2012 SEP/GLE71 decay interval described by
``event_C19_may2012.json``.  The script reads NOAA/NCEI monthly 5-minute
``epead_p17ew`` CSV files, resolves the invariant telemetry E/W head labels to the
conventional physical EAST/WEST ratio used in the May-2012 literature, estimates a
pre-event background independently for each detector head, and writes a compact
checksum-traceable reference table used by ``run_C19.py``. The table deliberately
preserves ``telemetry_head_east``/``telemetry_head_west`` so the runner can map each
flux stream back to the actual instrument head and use its time-dependent look vector;
C19 does not infer geometry from the words EAST/WEST.

Examples
--------
Download the NOAA files and build the checked reference::

    python3 srcEarth/test/C19/build_goes_reference.py --download

Build from files already downloaded locally::

    python3 srcEarth/test/C19/build_goes_reference.py \
      --goes13-particle g13_epead_p17ew_5m_20120501_20120531.csv \
      --goes15-particle g15_epead_p17ew_5m_20120501_20120531.csv

Exercise the parser and orientation/background logic without network access::

    python3 srcEarth/test/C19/build_goes_reference.py --self-test

The routine C19A reference intentionally uses a nominal GEO position when an
optional ephemeris CSV is not supplied.  NOAA provides one-minute ephemeris
products separately; pass them with ``--goes13-ephemeris`` and
``--goes15-ephemeris`` for publication runs.
"""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import io
import json
import math
import os
import statistics
import sys
import tempfile
import urllib.request
from bisect import bisect_left
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_MANIFEST = SCRIPT_DIR / "event_C19_may2012.json"
DEFAULT_OUTPUT = SCRIPT_DIR / "data" / "reference_C19_goes_epead_ew.csv.gz"
DEFAULT_PROVENANCE = SCRIPT_DIR / "data" / "reference_C19_goes_epead_ew_provenance.json"
DEFAULT_CACHE = SCRIPT_DIR / "data" / "cache"

NOAA_BASE = (
    "https://www.ncei.noaa.gov/data/goes-space-environment-monitor/access/"
    "avg/2012/05"
)
EPHEMERIS_DIRECTORY_URLS = {
    # NOAA/NCEI official GOES 6--15 one-minute orbit product.  C19 accepts the
    # native NetCDF files from these directories (``geo_llr`` = latitude,
    # longitude, geocentric radius) as well as an explicitly supplied CSV.
    "GOES13": "https://www.ncei.noaa.gov/data/goes-space-environment-monitor/access/sat_locations/goes13/",
    "GOES15": "https://www.ncei.noaa.gov/data/goes-space-environment-monitor/access/sat_locations/goes15/",
}

# Flux-column selection is deliberately explicit in P1.1.  The old C19 builder
# silently preferred *_UNCOR_FLUX and then fell through several other variable
# names.  That made two references built from different NOAA products look
# identical in provenance even though their detector correction state differed.
FLUX_PRODUCT_POLICIES = ("UNCORRECTED", "CORRECTED", "AUTO")

SOURCE_URLS = {
    "GOES13": {
        "particle": NOAA_BASE + "/goes13/csv/g13_epead_p17ew_5m_20120501_20120531.csv",
        "orientation": NOAA_BASE + "/goes13/csv/g13_epead_orientation_flag_1m_20120501_20120531_v1.0.0.csv",
    },
    "GOES15": {
        "particle": NOAA_BASE + "/goes15/csv/g15_epead_p17ew_5m_20120501_20120531.csv",
        "orientation": NOAA_BASE + "/goes15/csv/g15_epead_orientation_flag_1m_20120501_20120531_v1.0.0.csv",
    },
}

OUTPUT_FIELDS = [
    "utc", "spacecraft", "channel", "energy_min_mev", "energy_max_mev",
    "telemetry_head_east", "telemetry_head_west",
    # P1.1 provenance travels with every science row rather than living only in a
    # sidecar JSON.  This makes C19_reference_used.csv self-describing after rows
    # have been sub-selected or copied away from the original reference archive.
    "flux_product_policy", "east_flux_variable", "west_flux_variable",
    "east_flux_correction_state", "west_flux_correction_state",
    "east_flux_raw", "west_flux_raw", "east_background", "west_background",
    "east_flux_background_subtracted", "west_flux_background_subtracted",
    "east_west_ratio", "log10_east_west_ratio",
    "longitude_deg_east", "latitude_deg", "altitude_km", "position_source",
    "east_quality_flag", "west_quality_flag", "east_num_points", "west_num_points",
    "quality_status",
]


def parse_utc(text: str) -> datetime:
    value = str(text).strip().strip('"').replace("Z", "+00:00")
    if not value:
        raise ValueError("empty UTC string")
    # NOAA files normally use ISO text; accept compact and space-separated forms too.
    candidates = [value]
    if " " in value and "T" not in value:
        candidates.append(value.replace(" ", "T", 1))
    for candidate in candidates:
        try:
            dt = datetime.fromisoformat(candidate)
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=timezone.utc)
            return dt.astimezone(timezone.utc)
        except ValueError:
            pass
    for fmt in ("%Y-%m-%d %H:%M:%S.%f", "%Y-%m-%d %H:%M:%S",
                "%Y%m%d%H%M%S", "%Y/%m/%d %H:%M:%S"):
        try:
            return datetime.strptime(value, fmt).replace(tzinfo=timezone.utc)
        except ValueError:
            pass
    raise ValueError("unsupported UTC value: %r" % text)


def format_utc(dt: datetime) -> str:
    return dt.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def normalize_name(name: str) -> str:
    return "".join(ch for ch in str(name).upper() if ch.isalnum() or ch == "_")


def read_noaa_csv(path: Path) -> Tuple[List[str], List[Dict[str, str]]]:
    """Read a NOAA CSV that may contain a metadata preamble before the header."""
    text = path.read_text(encoding="utf-8-sig", errors="replace")
    lines = text.splitlines()
    header_index = None
    for index, line in enumerate(lines):
        fields = [normalize_name(part.strip().strip('"')) for part in next(csv.reader([line]))]
        if any(name in ("TIME_TAG", "TIME", "TIMESTAMP", "DATETIME") for name in fields):
            header_index = index
            break
    if header_index is None:
        raise ValueError("could not find a time-bearing CSV header in %s" % path)
    reader = csv.DictReader(io.StringIO("\n".join(lines[header_index:])))
    if reader.fieldnames is None:
        raise ValueError("CSV has no field names: %s" % path)
    normalized_fields = [normalize_name(name) for name in reader.fieldnames]
    rows: List[Dict[str, str]] = []
    for raw in reader:
        row = {
            normalize_name(key): ("" if value is None else str(value).strip())
            for key, value in raw.items() if key is not None
        }
        if any(value != "" for value in row.values()):
            rows.append(row)
    if not rows:
        raise ValueError("CSV has no data rows: %s" % path)
    return normalized_fields, rows


def first_present(row: Mapping[str, str], names: Sequence[str]) -> Optional[str]:
    value, _ = first_present_with_name(row, names)
    return value


def first_present_with_name(
        row: Mapping[str, str], names: Sequence[str],
        ) -> Tuple[Optional[str], Optional[str]]:
    """Return both a value and the exact normalized source-column name.

    P1.1 needs the latter as much as the former.  A numeric flux without its NOAA
    variable name is not sufficient provenance because ``*_UNCOR_FLUX``, ``*_FLUX``
    and ``*_COR_FLUX`` are not interchangeable detector products.
    """
    for name in names:
        key = normalize_name(name)
        value = row.get(key)
        if value is not None and str(value).strip() != "":
            return str(value).strip(), key
    return None, None


def parse_optional_float(text: Optional[str]) -> Optional[float]:
    if text is None:
        return None
    try:
        value = float(str(text).strip())
    except (TypeError, ValueError):
        return None
    if not math.isfinite(value) or value <= -1.0e30 or value in (-99999.0, -9999.0):
        return None
    return value


def parse_optional_int(text: Optional[str]) -> Optional[int]:
    value = parse_optional_float(text)
    return None if value is None else int(round(value))


def time_from_row(row: Mapping[str, str]) -> datetime:
    value = first_present(row, ("time_tag", "time", "timestamp", "datetime", "date_time"))
    if value is None:
        raise ValueError("row has no recognized time field")
    return parse_utc(value)


def flux_candidates(channel: str, head: str, policy: str) -> Tuple[str, ...]:
    """Return permitted NOAA flux variables for one explicit product policy.

    ``UNCORRECTED`` is the historical p17ew C19 choice and accepts only the
    uncorrected directional variable. ``CORRECTED`` never falls back to UNCOR and
    prefers an explicitly named corrected variable; generic ``*_FLUX`` is accepted
    second because some NOAA revisions use that name for the documented science
    flux. ``AUTO`` preserves the old permissive search only for deliberate legacy
    reproduction and is recorded loudly in provenance.
    """
    base = "%s%s" % (channel.upper(), head.upper())
    policy = str(policy).upper()
    if policy == "UNCORRECTED":
        return (base + "_UNCOR_FLUX",)
    if policy == "CORRECTED":
        return (base + "_COR_FLUX", base + "_FLUX", base + "_AVERAGE_FLUX")
    if policy == "AUTO":
        return (base + "_UNCOR_FLUX", base + "_FLUX", base + "_COR_FLUX",
                base + "_AVERAGE_FLUX", base)
    raise ValueError("unsupported GOES flux product policy: %s" % policy)


def flux_correction_state(variable_name: Optional[str]) -> str:
    name = (variable_name or "").upper()
    if "_UNCOR_FLUX" in name:
        return "UNCORRECTED"
    if "_COR_FLUX" in name:
        return "CORRECTED"
    if name:
        return "PRODUCT_DEFINED"
    return "UNKNOWN"


def quality_candidates(channel: str, head: str) -> Tuple[str, ...]:
    base = "%s%s" % (channel.upper(), head.upper())
    return (base + "_QUAL_FLAG", base + "_QUALITY_FLAG", base + "_FLAG")


def points_candidates(channel: str, head: str) -> Tuple[str, ...]:
    base = "%s%s" % (channel.upper(), head.upper())
    return (base + "_NUM_PTS", base + "_NPTS", base + "_NUM_POINTS")


def load_particle_rows(
        path: Path, channels: Sequence[str], flux_product_policy: str,
        ) -> List[Dict[str, object]]:
    """Read directional particle rows with exact flux-variable provenance."""
    _, records = read_noaa_csv(path)
    result: List[Dict[str, object]] = []
    for row_number, record in enumerate(records, start=1):
        try:
            epoch = time_from_row(record)
        except ValueError:
            continue
        item: Dict[str, object] = {"utc": epoch, "row_number": row_number}
        for channel in channels:
            for head in ("E", "W"):
                raw_flux, variable_name = first_present_with_name(
                    record, flux_candidates(channel, head, flux_product_policy))
                item["%s_%s_flux" % (channel, head)] = parse_optional_float(raw_flux)
                item["%s_%s_flux_variable" % (channel, head)] = variable_name
                item["%s_%s_flux_correction_state" % (channel, head)] = \
                    flux_correction_state(variable_name)
                item["%s_%s_quality" % (channel, head)] = parse_optional_int(
                    first_present(record, quality_candidates(channel, head)))
                item["%s_%s_points" % (channel, head)] = parse_optional_int(
                    first_present(record, points_candidates(channel, head)))
        result.append(item)
    if not result:
        raise ValueError("no time-tagged particle rows parsed from %s" % path)
    result.sort(key=lambda row: row["utc"])
    return result


def _load_ephemeris_csv(path: Path) -> List[Tuple[datetime, float, float, float]]:
    _, records = read_noaa_csv(path)
    result: List[Tuple[datetime, float, float, float]] = []
    for record in records:
        try:
            epoch = time_from_row(record)
        except ValueError:
            continue
        lon = parse_optional_float(first_present(record, (
            "longitude_deg_east", "longitude", "lon", "subsatellite_longitude",
            "geographic_longitude", "sat_lon", "east_longitude",
        )))
        lat = parse_optional_float(first_present(record, (
            "latitude_deg", "latitude", "lat", "subsatellite_latitude",
            "geographic_latitude", "sat_lat",
        )))
        alt = parse_optional_float(first_present(record, (
            "altitude_km", "altitude", "alt_km", "height_km", "sat_alt",
        )))
        if alt is None:
            radius = parse_optional_float(first_present(record, (
                "radius_km", "geocentric_radius_km", "geo_radius_km", "radius",
            )))
            if radius is not None:
                alt = radius - 6378.137
        if lon is None or lat is None or alt is None:
            continue
        lon = ((lon + 180.0) % 360.0) - 180.0
        result.append((epoch, lon, lat, alt))
    return result


def _load_ephemeris_netcdf(path: Path) -> List[Tuple[datetime, float, float, float]]:
    """Read the NOAA/NCEI GOES 6--15 1-minute ``goes-l2-orb1m`` product.

    NCEI documents ``geo_llr`` as [latitude(deg), longitude(deg), radius(km)].
    Xarray is preferred because it decodes the CF-style time coordinate.  The
    import is deliberately local so the reference builder still has no NetCDF
    dependency when a CSV ephemeris is supplied.
    """
    try:
        import xarray as xr  # type: ignore
    except Exception as exc:
        raise RuntimeError(
            "reading NOAA NetCDF ephemeris requires xarray (or convert the file to CSV): %s"
            % exc)
    ds = xr.open_dataset(path, decode_times=True)
    try:
        if "geo_llr" not in ds:
            raise ValueError("NOAA ephemeris %s does not contain geo_llr" % path)
        geo = ds["geo_llr"].values
        time_values = ds["time"].values if "time" in ds else None
        if time_values is None:
            raise ValueError("NOAA ephemeris %s does not contain time" % path)
        if getattr(geo, "ndim", 0) != 2:
            raise ValueError("NOAA ephemeris geo_llr must be two-dimensional")
        # NOAA files normally store [time,3].  Accept [3,time] as a defensive
        # compatibility path without guessing any different component order.
        if geo.shape[1] == 3:
            rows = geo
        elif geo.shape[0] == 3:
            rows = geo.T
        else:
            raise ValueError("NOAA ephemeris geo_llr has unexpected shape %r" % (geo.shape,))
        result: List[Tuple[datetime, float, float, float]] = []
        for t, llr in zip(time_values, rows):
            try:
                # numpy.datetime64 -> ns since Unix epoch; string conversion also
                # handles cftime-like values returned by unusual files.
                text = str(t)
                if text.endswith("Z"):
                    epoch = parse_utc(text)
                else:
                    epoch = parse_utc(text.replace(" ", "T") + ("Z" if "+" not in text else ""))
            except Exception:
                continue
            try:
                lat, lon, radius_km = (float(llr[0]), float(llr[1]), float(llr[2]))
            except Exception:
                continue
            if not all(math.isfinite(v) for v in (lat, lon, radius_km)):
                continue
            lon = ((lon + 180.0) % 360.0) - 180.0
            result.append((epoch, lon, lat, radius_km - 6378.137))
        return result
    finally:
        ds.close()


def load_ephemeris(path: Optional[Path]) -> List[Tuple[datetime, float, float, float]]:
    """Load a real ephemeris; native NOAA NetCDF and CSV are both supported."""
    if path is None:
        return []
    suffix = path.suffix.lower()
    result = (_load_ephemeris_netcdf(path) if suffix in (".nc", ".nc4", ".cdf")
              else _load_ephemeris_csv(path))
    result.sort(key=lambda item: item[0])
    if not result:
        raise ValueError("no usable ephemeris rows parsed from %s" % path)
    return result


def nearest_ephemeris(
        records: Sequence[Tuple[datetime, float, float, float]], epoch: datetime,
        maximum_separation_seconds: float = 180.0,
        ) -> Optional[Tuple[float, float, float]]:
    if not records:
        return None
    times = [row[0] for row in records]
    index = bisect_left(times, epoch)
    choices = []
    if index < len(records):
        choices.append(records[index])
    if index > 0:
        choices.append(records[index - 1])
    best = min(choices, key=lambda row: abs((row[0] - epoch).total_seconds()))
    if abs((best[0] - epoch).total_seconds()) > maximum_separation_seconds:
        return None
    return (best[1], best[2], best[3])


def median(values: Iterable[float]) -> float:
    finite = [float(value) for value in values if math.isfinite(float(value))]
    if not finite:
        raise ValueError("cannot calculate median of an empty sequence")
    return statistics.median(finite)


def download(url: str, destination: Path) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": "AMPS-C19A/1.0"})
    with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as out:
        while True:
            block = response.read(1024 * 1024)
            if not block:
                break
            out.write(block)
    temporary.replace(destination)
    return destination


def source_local_path(spacecraft: str, kind: str, cache_dir: Path) -> Path:
    return cache_dir / Path(SOURCE_URLS[spacecraft][kind]).name


def resolve_sources(args: argparse.Namespace, manifest: Mapping[str, object]) -> Dict[str, Dict[str, Optional[Path]]]:
    cache_dir = Path(args.cache_dir).expanduser().resolve()
    explicit = {
        "GOES13": {
            "particle": Path(args.goes13_particle).expanduser().resolve() if args.goes13_particle else None,
            "ephemeris": Path(args.goes13_ephemeris).expanduser().resolve() if args.goes13_ephemeris else None,
        },
        "GOES15": {
            "particle": Path(args.goes15_particle).expanduser().resolve() if args.goes15_particle else None,
            "ephemeris": Path(args.goes15_ephemeris).expanduser().resolve() if args.goes15_ephemeris else None,
        },
    }
    result: Dict[str, Dict[str, Optional[Path]]] = {}
    for spacecraft in ("GOES13", "GOES15"):
        particle = explicit[spacecraft]["particle"]
        if particle is None:
            particle = source_local_path(spacecraft, "particle", cache_dir)
            if args.download:
                print("Downloading %s particle data..." % spacecraft)
                download(SOURCE_URLS[spacecraft]["particle"], particle)
        if not particle.exists():
            raise FileNotFoundError(
                "%s particle file is missing: %s\nUse --download or pass --%s-particle."
                % (spacecraft, particle, spacecraft.lower()))
        result[spacecraft] = {
            "particle": particle,
            "ephemeris": explicit[spacecraft]["ephemeris"],
        }
    return result


def build_reference(
        manifest_path: Path,
        sources: Mapping[str, Mapping[str, Optional[Path]]],
        output_path: Path,
        provenance_path: Path,
        minimum_signal_to_background: float,
        flux_product_policy: Optional[str] = None,
        allow_nominal_ephemeris: bool = False,
        ) -> Tuple[List[Dict[str, object]], Dict[str, object]]:
    manifest = json.loads(manifest_path.read_text())
    analysis_start = parse_utc(manifest["analysis_start_utc"])
    analysis_end = parse_utc(manifest["analysis_end_utc"])
    background_start = parse_utc(manifest["background_start_utc"])
    background_end = parse_utc(manifest["background_end_utc"])
    channels = sorted(manifest["channels"])
    manifest_policy = str(manifest.get("goes_flux_product_policy", "UNCORRECTED")).upper()
    flux_product_policy = str(flux_product_policy or manifest_policy).upper()
    if flux_product_policy not in FLUX_PRODUCT_POLICIES:
        raise ValueError("unsupported flux product policy %s; choose %s" %
                         (flux_product_policy, ",".join(FLUX_PRODUCT_POLICIES)))

    output_rows: List[Dict[str, object]] = []
    source_records: Dict[str, object] = {}

    for spacecraft in ("GOES13", "GOES15"):
        config = manifest["spacecraft"][spacecraft]
        particle_path = sources[spacecraft]["particle"]
        assert particle_path is not None
        particle_rows = load_particle_rows(particle_path, channels, flux_product_policy)
        ephemeris_path = sources[spacecraft].get("ephemeris")
        ephemeris_rows = load_ephemeris(ephemeris_path)

        mapping = {
            str(head).upper(): str(direction).upper()
            for head, direction in config["telemetry_head_to_physical_direction"].items()
        }
        physical_to_head = {direction: head for head, direction in mapping.items()}
        if set(physical_to_head) != {"EAST", "WEST"}:
            raise ValueError("%s manifest does not map exactly one head to EAST and WEST" % spacecraft)

        backgrounds: Dict[Tuple[str, str], float] = {}
        for channel in channels:
            for head in ("E", "W"):
                values = []
                for row in particle_rows:
                    epoch = row["utc"]
                    if not (background_start <= epoch <= background_end):
                        continue
                    flux = row.get("%s_%s_flux" % (channel, head))
                    quality = row.get("%s_%s_quality" % (channel, head))
                    if flux is None or float(flux) <= 0.0:
                        continue
                    if quality not in (None, 0):
                        continue
                    values.append(float(flux))
                if not values:
                    raise ValueError(
                        "%s %s%s has no valid samples in background interval %s .. %s"
                        % (spacecraft, channel, head, format_utc(background_start),
                           format_utc(background_end)))
                backgrounds[(channel, head)] = median(values)

        for particle in particle_rows:
            epoch = particle["utc"]
            if not (analysis_start <= epoch <= analysis_end):
                continue
            position = nearest_ephemeris(ephemeris_rows, epoch)
            if position is None:
                if not allow_nominal_ephemeris:
                    raise ValueError(
                        "%s has no real ephemeris within 180 s of %s. P1.2 requires "
                        "NOAA one-minute ephemeris; pass --allow-nominal-ephemeris only "
                        "for explicit legacy/reproducibility runs." %
                        (spacecraft, format_utc(epoch)))
                position = (
                    float(config["nominal_longitude_deg_east"]),
                    float(config["nominal_latitude_deg"]),
                    float(config["nominal_altitude_km"]),
                )
                position_source = "NOMINAL_GEO_SLOT_LEGACY_OVERRIDE"
            else:
                position_source = "NOAA_ONE_MINUTE_EPHEMERIS"

            for channel in channels:
                east_head = physical_to_head["EAST"]
                west_head = physical_to_head["WEST"]
                east_raw = particle.get("%s_%s_flux" % (channel, east_head))
                west_raw = particle.get("%s_%s_flux" % (channel, west_head))
                east_quality = particle.get("%s_%s_quality" % (channel, east_head))
                west_quality = particle.get("%s_%s_quality" % (channel, west_head))
                east_points = particle.get("%s_%s_points" % (channel, east_head))
                west_points = particle.get("%s_%s_points" % (channel, west_head))
                east_flux_variable = particle.get("%s_%s_flux_variable" % (channel, east_head))
                west_flux_variable = particle.get("%s_%s_flux_variable" % (channel, west_head))
                east_correction_state = particle.get("%s_%s_flux_correction_state" % (channel, east_head))
                west_correction_state = particle.get("%s_%s_flux_correction_state" % (channel, west_head))
                if east_raw is None or west_raw is None:
                    continue
                if float(east_raw) <= 0.0 or float(west_raw) <= 0.0:
                    continue
                if east_quality not in (None, 0) or west_quality not in (None, 0):
                    continue

                east_background = backgrounds[(channel, east_head)]
                west_background = backgrounds[(channel, west_head)]
                east_net = float(east_raw) - east_background
                west_net = float(west_raw) - west_background
                if east_net <= 0.0 or west_net <= 0.0:
                    continue
                east_sbr = east_net / max(east_background, 1.0e-30)
                west_sbr = west_net / max(west_background, 1.0e-30)
                if min(east_sbr, west_sbr) < minimum_signal_to_background:
                    continue

                ratio = east_net / west_net
                channel_config = manifest["channels"][channel]
                output_rows.append({
                    "utc": format_utc(epoch),
                    "spacecraft": spacecraft,
                    "channel": channel,
                    "energy_min_mev": float(channel_config["energy_min_mev"]),
                    "energy_max_mev": float(channel_config["energy_max_mev"]),
                    "telemetry_head_east": east_head,
                    "telemetry_head_west": west_head,
                    "flux_product_policy": flux_product_policy,
                    "east_flux_variable": east_flux_variable or "UNKNOWN",
                    "west_flux_variable": west_flux_variable or "UNKNOWN",
                    "east_flux_correction_state": east_correction_state or "UNKNOWN",
                    "west_flux_correction_state": west_correction_state or "UNKNOWN",
                    "east_flux_raw": float(east_raw),
                    "west_flux_raw": float(west_raw),
                    "east_background": east_background,
                    "west_background": west_background,
                    "east_flux_background_subtracted": east_net,
                    "west_flux_background_subtracted": west_net,
                    "east_west_ratio": ratio,
                    "log10_east_west_ratio": math.log10(ratio),
                    "longitude_deg_east": position[0],
                    "latitude_deg": position[1],
                    "altitude_km": position[2],
                    "position_source": position_source,
                    "east_quality_flag": "" if east_quality is None else east_quality,
                    "west_quality_flag": "" if west_quality is None else west_quality,
                    "east_num_points": "" if east_points is None else east_points,
                    "west_num_points": "" if west_points is None else west_points,
                    "quality_status": "VALID",
                })

        source_records[spacecraft] = {
            "particle": {
                "path": str(particle_path.resolve()),
                "url": SOURCE_URLS[spacecraft]["particle"],
                "sha256": sha256(particle_path),
            },
            "ephemeris": None if ephemeris_path is None else {
                "path": str(ephemeris_path.resolve()),
                "sha256": sha256(ephemeris_path),
                "ncei_directory_url": EPHEMERIS_DIRECTORY_URLS[spacecraft],
            },
            "physical_direction_mapping": mapping,
            "flux_product_policy": flux_product_policy,
            "selected_flux_variables": sorted({
                str(row.get("%s_%s_flux_variable" % (channel, head)))
                for row in particle_rows for channel in channels for head in ("E", "W")
                if row.get("%s_%s_flux_variable" % (channel, head))
            }),
            "backgrounds": {
                "%s_%s" % key: value for key, value in sorted(backgrounds.items())
            },
        }

    output_rows.sort(key=lambda row: (row["utc"], row["spacecraft"], row["channel"]))
    if not output_rows:
        raise ValueError("no C19A reference rows survived the quality/background filters")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with gzip.open(output_path, "wt", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        for row in output_rows:
            writer.writerow(row)

    provenance: Dict[str, object] = {
        "test_id": "C19A",
        "reference_kind": "NOAA_GOES13_15_EPEAD_DIRECTIONAL_PROTON_RATIO",
        "created_utc": format_utc(datetime.now(timezone.utc)),
        "event_manifest_path": str(manifest_path.resolve()),
        "event_manifest_sha256": sha256(manifest_path),
        "reference_path": str(output_path.resolve()),
        "reference_sha256": sha256(output_path),
        "n_rows": len(output_rows),
        "minimum_signal_to_background": minimum_signal_to_background,
        "background_method": "per-spacecraft/channel/telemetry-head median in manifest background interval",
        "ratio_definition": "(physical east raw-background)/(physical west raw-background)",
        "flux_product_policy": flux_product_policy,
        "position_policy": ("NOAA one-minute ephemeris required within 180 s" if not allow_nominal_ephemeris
                            else "NOAA one-minute ephemeris preferred; explicit legacy nominal-slot fallback enabled"),
        "allow_nominal_ephemeris": bool(allow_nominal_ephemeris),
        "instrument_model_scope": "directional P4/P5 science flux with exact source-variable provenance; synthetic response folding is performed by run_C19.py",
        "sources": source_records,
        "references": [
            "NOAA GOES 1-15 SEM archive, dataset DSI 2086_01",
            "Rodriguez et al. (2010), doi:10.1029/2010GL042531",
            "Rodriguez et al. (2014), doi:10.1002/2013SW000996",
        ],
    }
    provenance_path.parent.mkdir(parents=True, exist_ok=True)
    provenance_path.write_text(json.dumps(provenance, indent=2, sort_keys=True) + "\n")
    return output_rows, provenance


def write_synthetic_noaa(path: Path, manifest: Mapping[str, object], spacecraft: str) -> None:
    config = manifest["spacecraft"][spacecraft]
    # Telemetry W/E values are chosen so the physical direction mapping can be tested.
    fields = [
        "time_tag",
        "P4E_NUM_PTS", "P4E_QUAL_FLAG", "P4E_UNCOR_FLUX",
        "P4W_NUM_PTS", "P4W_QUAL_FLAG", "P4W_UNCOR_FLUX",
        "P5E_NUM_PTS", "P5E_QUAL_FLAG", "P5E_UNCOR_FLUX",
        "P5W_NUM_PTS", "P5W_QUAL_FLAG", "P5W_UNCOR_FLUX",
    ]
    rows = []
    for day, hour, event in ((16, 0, False), (16, 6, False), (17, 6, True), (17, 7, True)):
        epoch = datetime(2012, 5, day, hour, tzinfo=timezone.utc)
        if not event:
            e4, w4, e5, w5 = 1.0, 2.0, 0.5, 1.0
        elif spacecraft == "GOES13":
            # GOES13: telemetry W is physical east; E is physical west.
            e4, w4, e5, w5 = 21.0, 11.0, 11.0, 6.0
        else:
            # GOES15: telemetry E is physical east; W is physical west.
            e4, w4, e5, w5 = 11.0, 21.0, 6.0, 11.0
        rows.append([
            format_utc(epoch), 60, 0, e4, 60, 0, w4,
            60, 0, e5, 60, 0, w5,
        ])
    with path.open("w", newline="") as stream:
        stream.write("# synthetic C19A NOAA fixture\n")
        writer = csv.writer(stream)
        writer.writerow(fields)
        writer.writerows(rows)



def write_synthetic_ephemeris(path: Path, spacecraft: str) -> None:
    """Create a minimal one-minute-style CSV fixture for the strict P1.2 path."""
    longitude = -75.0 if spacecraft == "GOES13" else -135.0
    fields = ["time_tag", "longitude_deg_east", "latitude_deg", "altitude_km"]
    rows = []
    start = datetime(2012, 5, 16, 0, 0, tzinfo=timezone.utc)
    end = datetime(2012, 5, 17, 8, 0, tzinfo=timezone.utc)
    epoch = start
    while epoch <= end:
        rows.append([format_utc(epoch), longitude + 0.01 * math.sin(epoch.hour), 0.02, 35786.0])
        epoch += timedelta(minutes=1)
    with path.open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(fields)
        writer.writerows(rows)

def self_test() -> int:
    manifest = json.loads(DEFAULT_MANIFEST.read_text())
    with tempfile.TemporaryDirectory(prefix="C19A_builder_selftest_") as temporary:
        root = Path(temporary)
        sources: Dict[str, Dict[str, Optional[Path]]] = {}
        for spacecraft in ("GOES13", "GOES15"):
            path = root / (spacecraft.lower() + ".csv")
            write_synthetic_noaa(path, manifest, spacecraft)
            ephemeris = root / (spacecraft.lower() + "_ephemeris.csv")
            write_synthetic_ephemeris(ephemeris, spacecraft)
            sources[spacecraft] = {"particle": path, "ephemeris": ephemeris}
        output = root / "reference.csv.gz"
        provenance = root / "provenance.json"
        rows, info = build_reference(
            DEFAULT_MANIFEST, sources, output, provenance,
            minimum_signal_to_background=1.0,
            flux_product_policy="UNCORRECTED",
            allow_nominal_ephemeris=False,
        )
        if len(rows) != 8:
            raise AssertionError("expected 8 synthetic rows, got %d" % len(rows))
        for row in rows:
            if not (0.0 < float(row["east_west_ratio"]) < 1.0):
                raise AssertionError("orientation mapping did not produce EAST/WEST < 1: %s" % row)
            expected_east_head = ("W" if row["spacecraft"] == "GOES13" else "E")
            if row["telemetry_head_east"] != expected_east_head:
                raise AssertionError("event-specific head mapping is wrong: %s" % row)
            if not str(row["east_flux_variable"]).endswith("_UNCOR_FLUX"):
                raise AssertionError("P1.1 exact flux-variable provenance is missing: %s" % row)
            if row["position_source"] != "NOAA_ONE_MINUTE_EPHEMERIS":
                raise AssertionError("P1.2 self-test did not use real ephemeris path: %s" % row)
        if not output.exists() or not provenance.exists():
            raise AssertionError("self-test did not create reference products")
        if info["n_rows"] != 8:
            raise AssertionError("provenance row count is wrong")
    print("C19A reference-builder self-test: PASS")
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build the C19A GOES-13/15 EPEAD east-west reference table")
    parser.add_argument("--event-manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
    parser.add_argument("--provenance-output", default=str(DEFAULT_PROVENANCE))
    parser.add_argument("--cache-dir", default=str(DEFAULT_CACHE))
    parser.add_argument("--download", action="store_true",
                        help="download the NOAA monthly particle files into --cache-dir")
    parser.add_argument("--flux-product", choices=("MANIFEST",) + FLUX_PRODUCT_POLICIES,
                        default="MANIFEST",
                        help="P1.1 exact NOAA flux product; default follows event manifest and never silently changes correction state")
    parser.add_argument("--allow-nominal-ephemeris", action="store_true",
                        help="explicit legacy override: permit manifest GEO-slot position when real NOAA ephemeris is missing; science references should not use this")
    parser.add_argument("--goes13-particle")
    parser.add_argument("--goes15-particle")
    parser.add_argument("--goes13-ephemeris",
                        help="NOAA one-minute GOES-13 ephemeris (.nc native goes-l2-orb1m or CSV)")
    parser.add_argument("--goes15-ephemeris",
                        help="NOAA one-minute GOES-15 ephemeris (.nc native goes-l2-orb1m or CSV)")
    parser.add_argument("--min-signal-to-background", type=float, default=3.0,
                        help="minimum (raw-background)/background for both heads; default: 3")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.min_signal_to_background < 0.0:
        parser.error("--min-signal-to-background must be non-negative")
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        return self_test()
    manifest_path = Path(args.event_manifest).expanduser().resolve()
    manifest = json.loads(manifest_path.read_text())
    sources = resolve_sources(args, manifest)
    output_path = Path(args.output).expanduser().resolve()
    provenance_path = Path(args.provenance_output).expanduser().resolve()
    rows, provenance = build_reference(
        manifest_path, sources, output_path, provenance_path,
        minimum_signal_to_background=args.min_signal_to_background,
        flux_product_policy=(None if args.flux_product == "MANIFEST" else args.flux_product),
        allow_nominal_ephemeris=args.allow_nominal_ephemeris,
    )
    print("C19A reference: %d rows" % len(rows))
    print("  %s" % output_path)
    print("  SHA-256: %s" % provenance["reference_sha256"])
    print("  provenance: %s" % provenance_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
