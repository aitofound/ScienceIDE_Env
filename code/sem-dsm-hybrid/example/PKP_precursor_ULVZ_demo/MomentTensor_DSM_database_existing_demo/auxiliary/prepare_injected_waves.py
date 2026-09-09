#!/usr/bin/env python3
"""Prepare InjectedWaves cut-window metadata for the SEM-DSM demo."""

from __future__ import annotations

import contextlib
import io
import math
import sys
from pathlib import Path

from dsm_direct_pki_timing import angular_distance_deg, core_turning_p_arrival_time, direct_p_arrival_time, parse_dsm_zones, source_to_box_phase_label


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


HERE = Path(__file__).resolve().parent
ROOT_DIR = (HERE / "..").resolve()
PARAM_FILE = ROOT_DIR / "DATA/Par_file_SEM_DSM"
CMT_FILE = ROOT_DIR / "DATA/CMTSOLUTION"
DSM_MODEL_INPUT = ROOT_DIR / "WORK/SPECFEM3D/DATA/dsm_model_input"
DSM_MODEL_BASE = ROOT_DIR / "DATA/dsm_model_base"
SEM_DATABASE_DIR = ROOT_DIR / "WORK/SPECFEM3D/OUTPUT_FILES/DATABASES_MPI"


def clean_number(text: str) -> float:
    return float(text.strip().replace("d", "e").replace("D", "e"))


def read_params(path: Path) -> dict[str, str]:
    params: dict[str, str] = {}
    for line in path.read_text().splitlines():
        body = line.split("#", 1)[0]
        if "=" not in body:
            continue
        key, value = body.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key:
            params[key] = value
    return params


def optional_float(params: dict[str, str], key: str) -> float | None:
    value = params.get(key, "").strip()
    if not value:
        return None
    return clean_number(value)


def param_float(params: dict[str, str], key: str, default: float) -> float:
    value = optional_float(params, key)
    return default if value is None else value


def parse_cmt(path: Path) -> dict[str, float]:
    if not path.is_file():
        die(f"Missing CMTSOLUTION: {path}")
    values: dict[str, float] = {}
    for line in path.read_text().splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        if key in {"latitude", "longitude", "depth"}:
            values[key] = clean_number(value.split()[0])
    missing = sorted({"latitude", "longitude", "depth"} - values.keys())
    if missing:
        die(f"CMTSOLUTION is missing required fields: {', '.join(missing)}")
    return values



def normalize_lon(lon: float) -> float:
    return ((lon + 180.0) % 360.0) - 180.0


def destination_point(lat: float, lon: float, azimuth: float, distance_deg: float) -> tuple[float, float]:
    lat1 = math.radians(lat)
    lon1 = math.radians(lon)
    az = math.radians(azimuth)
    delta = math.radians(distance_deg)

    sin_lat2 = math.sin(lat1) * math.cos(delta) + math.cos(lat1) * math.sin(delta) * math.cos(az)
    lat2 = math.asin(max(-1.0, min(1.0, sin_lat2)))
    y = math.sin(az) * math.sin(delta) * math.cos(lat1)
    x = math.cos(delta) - math.sin(lat1) * math.sin(lat2)
    lon2 = lon1 + math.atan2(y, x)
    return math.degrees(lat2), normalize_lon(math.degrees(lon2))


def move_local(lat: float, lon: float, east_deg: float, north_deg: float) -> tuple[float, float]:
    distance = math.hypot(east_deg, north_deg)
    if distance == 0.0:
        return lat, lon
    azimuth = math.degrees(math.atan2(east_deg, north_deg))
    return destination_point(lat, lon, azimuth, distance)


def table_first_column_values(path: Path, skip_lines: int) -> list[float]:
    if not path.is_file():
        return []
    values: list[float] = []
    for line in path.read_text().splitlines()[skip_lines:]:
        fields = line.split()
        if not fields:
            continue
        try:
            values.append(clean_number(fields[0]))
        except ValueError:
            continue
    return values


def counted_distance_values(path: Path) -> list[float]:
    if not path.is_file():
        return []
    lines = path.read_text().splitlines()
    if not lines:
        return []
    try:
        count = int(clean_number(lines[0].split()[0]))
    except (IndexError, ValueError):
        return []
    values: list[float] = []
    for line in lines[1 : count + 1]:
        fields = line.split()
        if not fields:
            continue
        try:
            values.append(clean_number(fields[0]))
        except ValueError:
            continue
    return values


def extrema(values: list[float]) -> list[tuple[str, float]]:
    if not values:
        return []
    lo = min(values)
    hi = max(values)
    if math.isclose(lo, hi, rel_tol=0.0, abs_tol=1.0e-10):
        return [("min", lo)]
    return [("min", lo), ("max", hi)]


def arrival_time_for_distance_depth(
    zones: list[dict[str, object]],
    earth_radius: float,
    source_depth: float,
    top_depth: float,
    receiver_depth: float,
    distance: float,
    requested_phase: str,
) -> tuple[str, float]:
    phase_name = source_to_box_phase_label(receiver_depth, distance, earth_radius)
    requested = requested_phase.strip().upper()
    if requested and phase_name.upper() != requested:
        die(f"computed branch {phase_name} does not match requested phase {requested}")
    if requested == "PKP":
        return phase_name, core_turning_p_arrival_time(zones, earth_radius, source_depth, receiver_depth, distance)
    if receiver_depth > top_depth and phase_name == "PK":
        top_arrival = direct_p_arrival_time(zones, earth_radius, source_depth, top_depth, distance)
        local_down_time = direct_p_arrival_time(zones, earth_radius, top_depth, receiver_depth, 0.0)
        return phase_name, top_arrival + local_down_time
    return phase_name, direct_p_arrival_time(zones, earth_radius, source_depth, receiver_depth, distance)




def try_arrival_time_for_distance_depth(
    zones: list[dict[str, object]],
    earth_radius: float,
    source_depth: float,
    top_depth: float,
    receiver_depth: float,
    distance: float,
    requested_phase: str,
) -> tuple[tuple[str, float] | None, str]:
    stderr = io.StringIO()
    try:
        with contextlib.redirect_stderr(stderr):
            return (
                arrival_time_for_distance_depth(
                    zones, earth_radius, source_depth, top_depth, receiver_depth, distance, requested_phase
                ),
                stderr.getvalue().strip(),
            )
    except SystemExit:
        message = stderr.getvalue().strip()
        if message.startswith("ERROR:"):
            message = message.split(":", 1)[1].strip()
        return None, message


def sem_table_arrivals(
    zones: list[dict[str, object]],
    earth_radius: float,
    source_depth: float,
    top_depth: float,
    requested_phase: str,
) -> tuple[list[dict[str, float | str]], list[str]]:
    arrivals: list[dict[str, float | str]] = []
    skipped: list[str] = []
    attempted = 0
    for domain in ("elastic", "acoustic"):
        distances = counted_distance_values(SEM_DATABASE_DIR / f"dist_table_{domain}")
        radii = table_first_column_values(SEM_DATABASE_DIR / f"depth_table_{domain}", 2)
        if not distances or not radii:
            skipped.append(f"{domain}_missing_or_empty_table")
            continue
        depths = radii
        for dist_label, distance in extrema(distances):
            for depth_label, receiver_depth in extrema(depths):
                attempted += 1
                name = f"{domain}_dist_{dist_label}_depth_{depth_label}"
                result, message = try_arrival_time_for_distance_depth(
                    zones, earth_radius, source_depth, top_depth, receiver_depth, distance, requested_phase
                )
                if result is None:
                    skipped.append(f"{name}: {message}" if message else name)
                    continue
                phase_name, arrival_time = result
                arrivals.append(
                    {
                        "name": name,
                        "domain": domain,
                        "distance_label": dist_label,
                        "depth_label": depth_label,
                        "depth": receiver_depth,
                        "distance": distance,
                        "time": arrival_time,
                        "phase": phase_name,
                    }
                )
    if attempted > 0 and not arrivals:
        die(f"Could not compute requested source-to-box phase {requested_phase} for any SEM depth/distance-table extrema")
    return arrivals, skipped


def box_corners(params: dict[str, str]) -> list[dict[str, float | str]]:
    center_lat = param_float(params, "CENTER_LATITUDE_IN_DEGREES", 0.0)
    center_lon = param_float(params, "CENTER_LONGITUDE_IN_DEGREES", 0.0)
    center_depth = param_float(params, "CENTER_DEPTH_KM", 0.0)
    depth_block = param_float(params, "DEPTH_BLOCK_KM", 0.0)
    width_xi = param_float(params, "ANGULAR_WIDTH_XI_IN_DEGREES", 0.0)
    width_eta = param_float(params, "ANGULAR_WIDTH_ETA_IN_DEGREES", 0.0)
    gamma = param_float(params, "GAMMA_ROTATION_AZIMUTH", 0.0)

    corners: list[dict[str, float | str]] = []
    cg = math.cos(math.radians(gamma))
    sg = math.sin(math.radians(gamma))
    for xi_sign in (-1.0, 1.0):
        for eta_sign in (-1.0, 1.0):
            xi = xi_sign * width_xi / 2.0
            eta = eta_sign * width_eta / 2.0
            east = xi * cg - eta * sg
            north = xi * sg + eta * cg
            lat, lon = move_local(center_lat, center_lon, east, north)
            for dep_sign, label in ((-1.0, "top"), (1.0, "bottom")):
                depth = center_depth + dep_sign * depth_block / 2.0
                corners.append(
                    {
                        "name": f"xi{xi_sign:+.0f}_eta{eta_sign:+.0f}_{label}",
                        "lat": lat,
                        "lon": lon,
                        "depth": depth,
                    }
                )
    return corners


def main() -> None:
    output_dir = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT_DIR / "WORK/InjectedWaves/DATA"
    if not PARAM_FILE.is_file():
        die(f"Missing parameter file: {PARAM_FILE}")

    params = read_params(PARAM_FILE)
    start_override = optional_float(params, "START_TIME_CUT")
    end_override = optional_float(params, "END_TIME_CUT")

    output_dir.mkdir(parents=True, exist_ok=True)
    metadata = output_dir / "injected_waves_metadata.env"

    if start_override is not None and end_override is not None:
        if end_override <= start_override:
            die(f"END_TIME_CUT ({end_override}) must be larger than START_TIME_CUT ({start_override})")
        metadata.write_text(
            "\n".join(
                [
                    "CUT_WINDOW_SOURCE=USER_SPECIFIED_START_TIME_CUT_END_TIME_CUT",
                    f"TIME_START_CUT={start_override:.3f}",
                    f"TIME_END_CUT={end_override:.3f}",
                    "CORNER_ARRIVAL_COUNT=0",
                ]
            )
            + "\n"
        )
        print(f"Using user-specified cut window: {start_override:.3f} to {end_override:.3f} s")
        return

    if start_override is not None or end_override is not None:
        die("Specify both START_TIME_CUT and END_TIME_CUT, or leave both unset for automatic corner timing")

    cmt = parse_cmt(CMT_FILE)
    phase = params.get("PHASE_SOURCE_TO_BOX", params.get("PHASE_BOX_TO_RECEIVER", "PKP")).strip()
    fallback_buffer = param_float(params, "TIME_BUFFER_CUT", 0.0)
    buffer_before = param_float(params, "TIME_BUFFER_CUT_BEFORE", fallback_buffer)
    buffer_after = param_float(params, "TIME_BUFFER_CUT_AFTER", fallback_buffer)
    earth_radius = param_float(params, "R_EARTH", 6371.0)

    model_path = DSM_MODEL_INPUT if DSM_MODEL_INPUT.is_file() else DSM_MODEL_BASE
    if not model_path.is_file():
        die(f"Could not find DSM model for source-to-box timing: {DSM_MODEL_INPUT} or {DSM_MODEL_BASE}")
    zones = parse_dsm_zones(model_path)

    center_depth = param_float(params, "CENTER_DEPTH_KM", 0.0)
    depth_block = param_float(params, "DEPTH_BLOCK_KM", 0.0)
    top_depth = center_depth - depth_block / 2.0

    arrivals: list[dict[str, float | str]] = []
    skipped: list[str] = []
    for corner in box_corners(params):
        distance = angular_distance_deg(cmt["latitude"], cmt["longitude"], float(corner["lat"]), float(corner["lon"]))
        receiver_depth = float(corner["depth"])
        result, message = try_arrival_time_for_distance_depth(
            zones, earth_radius, cmt["depth"], top_depth, receiver_depth, distance, phase
        )
        if result is None:
            skipped.append(f"{corner['name']}: {message}" if message else str(corner["name"]))
            continue
        phase_name, arrival_time = result
        arrivals.append(
            {
                "name": str(corner["name"]),
                "lat": float(corner["lat"]),
                "lon": float(corner["lon"]),
                "depth": receiver_depth,
                "distance": distance,
                "time": arrival_time,
                "phase": phase_name,
            }
        )

    if not arrivals:
        detail = "; ".join(skipped[:4])
        suffix = f" First skipped entries: {detail}" if detail else ""
        die(f"Could not compute requested source-to-box phase {phase} for any SEM-box corner using {model_path}.{suffix}")

    table_arrivals, table_skipped = sem_table_arrivals(zones, earth_radius, cmt["depth"], top_depth, phase)
    all_arrivals = arrivals + table_arrivals
    corner_min_arrival = min(float(item["time"]) for item in arrivals)
    corner_max_arrival = max(float(item["time"]) for item in arrivals)
    if table_arrivals:
        table_min_arrival = min(float(item["time"]) for item in table_arrivals)
        table_max_arrival = max(float(item["time"]) for item in table_arrivals)
    else:
        table_min_arrival = corner_min_arrival
        table_max_arrival = corner_max_arrival
    min_arrival = min(float(item["time"]) for item in all_arrivals)
    max_arrival = max(float(item["time"]) for item in all_arrivals)
    time_start = max(0.0, min_arrival - buffer_before)
    time_end = max_arrival + buffer_after

    lines = [
        "CUT_WINDOW_SOURCE=DSM_MODEL_PHASE_FILTERED_RAY",
        f"PHASE_SOURCE_TO_BOX={phase}",
        "TRAVEL_TIME_METHOD=DSM_MODEL_PHASE_FILTERED_RAY",
        f"DSM_MODEL_FOR_TIMING={model_path}",
        f"TIME_BUFFER_CUT_BEFORE={buffer_before:.3f}",
        f"TIME_BUFFER_CUT_AFTER={buffer_after:.3f}",
        f"MIN_SOURCE_TO_BOX_ARRIVAL_SEC={min_arrival:.3f}",
        f"MAX_SOURCE_TO_BOX_ARRIVAL_SEC={max_arrival:.3f}",
        f"MIN_CORNER_ARRIVAL_SEC={min_arrival:.3f}",
        f"MAX_CORNER_ARRIVAL_SEC={max_arrival:.3f}",
        f"MIN_CORNER_ONLY_ARRIVAL_SEC={corner_min_arrival:.3f}",
        f"MAX_CORNER_ONLY_ARRIVAL_SEC={corner_max_arrival:.3f}",
        f"MIN_DISTANCE_TABLE_ARRIVAL_SEC={table_min_arrival:.3f}",
        f"MAX_DISTANCE_TABLE_ARRIVAL_SEC={table_max_arrival:.3f}",
        f"TIME_START_CUT={time_start:.3f}",
        f"TIME_END_CUT={time_end:.3f}",
        f"CORNER_ARRIVAL_COUNT={len(arrivals):d}",
        f"CORNER_SKIPPED_COUNT={len(skipped):d}",
        f"DISTANCE_TABLE_ARRIVAL_COUNT={len(table_arrivals):d}",
        f"DISTANCE_TABLE_SKIPPED_COUNT={len(table_skipped):d}",
    ]
    metadata.write_text("\n".join(lines) + "\n")

    corner_table = output_dir / "sem_box_corner_arrivals.txt"
    with corner_table.open("w") as f:
        f.write("# corner lat lon depth_km distance_deg phase arrival_time_s\n")
        for item in arrivals:
            f.write(
                f"{item['name']} {float(item['lat']):.6f} {float(item['lon']):.6f} "
                f"{float(item['depth']):.4f} {float(item['distance']):.6f} "
                f"{item['phase']} {float(item['time']):.3f}\n"
            )
        for name in skipped:
            f.write(f"# skipped_no_arrival {name}\n")

    table_arrival_file = output_dir / "sem_box_distance_table_arrivals.txt"
    with table_arrival_file.open("w") as f:
        f.write("# table_entry domain distance_label depth_label depth_km distance_deg phase arrival_time_s\n")
        for item in table_arrivals:
            f.write(
                f"{item['name']} {item['domain']} {item['distance_label']} {item['depth_label']} "
                f"{float(item['depth']):.4f} {float(item['distance']):.6f} "
                f"{item['phase']} {float(item['time']):.3f}\n"
            )
        for name in table_skipped:
            f.write(f"# skipped_no_arrival {name}\n")

    for name in skipped:
        print(f"Warning: skipped SEM-box corner with no direct timing: {name}")
    for name in table_skipped:
        print(f"Warning: skipped SEM depth/distance-table entry with no direct timing: {name}")

    print(f"Computed cut window from SEM-box source-to-box arrivals: {time_start:.3f} to {time_end:.3f} s")
    print(f"  corner arrivals used: {len(arrivals)}, corners skipped: {len(skipped)}")
    print(f"  distance-table arrivals used: {len(table_arrivals)}, table entries skipped: {len(table_skipped)}")
    print(f"  corner details: {corner_table}")
    print(f"  distance-table details: {table_arrival_file}")


if __name__ == "__main__":
    main()
