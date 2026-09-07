#!/usr/bin/env python3
"""Prepare DSM box-to-receiver reciprocity inputs for a parameterized SEM-DSM case."""

from __future__ import annotations

import math
import os
import re
import sys
from pathlib import Path

from dsm_direct_pki_timing import angular_distance_deg, direct_p_arrival_time, parse_dsm_zones, phase_label_for_receiver


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


HERE = Path(__file__).resolve().parent
SEM_DIR = (HERE / "..").resolve()
PARAM_FILE = SEM_DIR / "DATA/Par_file_SEM_DSM"
STATION_FILE = SEM_DIR / "DATA/tele_station.txt"
SOURCE_DSM_MODEL_INPUT = SEM_DIR / "WORK/SPECFEM3D/DATA/dsm_model_input"

# Default DATA_DIR, but can be overridden by argument
DATA_DIR = (SEM_DIR / "WORK/DSM/box_to_recv/DATA").resolve()
SEM_DATABASE_DIR = (SEM_DIR / "WORK/SPECFEM3D/OUTPUT_FILES/DATABASES_MPI").resolve()


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


def param_float(params: dict[str, str], key: str, default: float) -> float:
    return clean_number(params.get(key, str(default)))


def optional_param_float(params: dict[str, str], key: str) -> float | None:
    value = params.get(key, "").strip()
    if not value:
        return None
    return clean_number(value)


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


def sem_box_sample_points(params: dict[str, str]) -> list[tuple[float, float]]:
    center_lat = param_float(params, "CENTER_LATITUDE_IN_DEGREES", 0.0)
    center_lon = param_float(params, "CENTER_LONGITUDE_IN_DEGREES", 0.0)
    width_xi = param_float(params, "ANGULAR_WIDTH_XI_IN_DEGREES", 1.0)
    width_eta = param_float(params, "ANGULAR_WIDTH_ETA_IN_DEGREES", 1.0)
    gamma = param_float(params, "GAMMA_ROTATION_AZIMUTH", 0.0)
    nsample = int(round(optional_param_float(params, "DSM_BOX_DISTANCE_SAMPLES") or 11.0))
    nsample = max(2, nsample)
    cg = math.cos(math.radians(gamma))
    sg = math.sin(math.radians(gamma))
    points: list[tuple[float, float]] = []
    seen: set[tuple[float, float]] = set()
    for ixi in range(nsample):
        xi = -0.5 * width_xi + width_xi * ixi / (nsample - 1)
        for ieta in range(nsample):
            eta = -0.5 * width_eta + width_eta * ieta / (nsample - 1)
            east = xi * cg - eta * sg
            north = xi * sg + eta * cg
            lat, lon = move_local(center_lat, center_lon, east, north)
            key = (round(lat, 10), round(lon, 10))
            if key not in seen:
                seen.add(key)
                points.append((lat, lon))
    return points


def counted_sem_distance_blocks(path: Path) -> list[list[float]]:
    if not path.is_file():
        return []
    lines = path.read_text().splitlines()
    blocks: list[list[float]] = []
    i = 0
    while i < len(lines):
        fields = lines[i].split()
        if not fields:
            i += 1
            continue
        try:
            count = int(clean_number(fields[0]))
        except (IndexError, ValueError):
            return []
        i += 1
        if count < 0 or i + count > len(lines):
            return []
        block: list[float] = []
        for line in lines[i : i + count]:
            fields = line.split()
            if not fields:
                continue
            try:
                block.append(clean_number(fields[0]))
            except ValueError:
                return []
        blocks.append(block)
        i += count
    return blocks


def sem_distance_floor(params: dict[str, str], spacing: float) -> float:
    explicit = optional_param_float(params, "DSM_MIN_DISTANCE_DEGREES")
    if explicit is not None:
        print(f"  Using user-specified DSM minimum distance: {explicit:.6f} degrees")
        return max(0.0, explicit)

    candidates: list[float] = []
    for name in ("dist_table_elastic", "dist_table_acoustic", "dist_table_elastic_inner", "dist_table_acoustic_inner"):
        for block in counted_sem_distance_blocks(SEM_DATABASE_DIR / name):
            values = sorted({dist for dist in block if dist >= 0.0})
            if not values:
                continue
            first = values[0]
            if len(values) > 1:
                sem_spacing = values[1] - values[0]
                if sem_spacing > 0.0 and first <= 2.0 * sem_spacing:
                    candidates.append(2.0 * sem_spacing)
                    continue
            if first > 0.0:
                candidates.append(first)

    if candidates:
        floor = min(candidates)
        print(f"  Using SEM distance-table minimum DSM distance: {floor:.6f} degrees")
        return floor

    floor = 2.0 * spacing
    print(f"  SEM distance tables not found; using DSM minimum distance: {floor:.6f} degrees")
    return floor


def parse_stations(path: Path) -> list[dict]:
    if not path.is_file():
        die(f"Missing station file: {path}")

    stations = []
    for line in path.read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) < 4:
            continue
        # Net Sta Lon Lat
        stations.append({
            "net": fields[0],
            "sta": fields[1],
            "lon": clean_number(fields[2]),
            "lat": clean_number(fields[3]),
        })
    return stations


def strip_comment(line: str) -> str:
    return line.split("#", 1)[0].replace("d", "e").replace("D", "e")


def parse_dsm_model(path: Path, earth_radius: float) -> tuple[list[str], list[dict], list[str]]:
    lines = path.read_text().splitlines()
    if len(lines) < 4:
        die(f"DSM model is too short: {path}")
    nzone = int(strip_comment(lines[3]).split()[0])
    zones = []
    idx = 4
    for izone in range(1, nzone + 1):
        if idx + 5 >= len(lines):
            die(f"DSM model ended while reading zone {izone}")
        zone_line = lines[idx]
        fields = [clean_number(v) for v in strip_comment(zone_line).split()]
        if len(fields) < 2:
            die(f"Bad DSM zone line for zone {izone}: {zone_line}")
        coeff_lines = lines[idx + 1 : idx + 6]
        coeffs = [[clean_number(v) for v in strip_comment(line).split()] for line in coeff_lines]
        q_fields = coeffs[-1]
        qmu = q_fields[-2] if len(q_fields) >= 2 else 0.0
        zones.append(
            {
                "id": izone,
                "rmin": fields[0],
                "rmax": fields[1],
                "lines": lines[idx : idx + 6],
                "vph": coeffs[0],
                "vpv": coeffs[1],
                "vsh": coeffs[2],
                "vsv": coeffs[3],
                "qmu": qmu,
            }
        )
        idx += 6
    return lines[:4], zones, lines[idx:]


def eval_poly(coeffs: list[float], radius: float, earth_radius: float) -> float:
    x = radius / earth_radius
    value = 0.0
    power = 1.0
    for coeff in coeffs:
        value += coeff * power
        power *= x
    return value


def min_wave_speed(zone: dict, lo: float, hi: float, earth_radius: float) -> tuple[str, float]:
    fluid = zone["qmu"] < 0.0
    minv = None
    for i in range(21):
        radius = lo + (hi - lo) * i / 20.0
        vp = min(v for v in (eval_poly(zone["vph"], radius, earth_radius), eval_poly(zone["vpv"], radius, earth_radius)) if v > 0.0)
        vs_values = [v for v in (eval_poly(zone["vsh"], radius, earth_radius), eval_poly(zone["vsv"], radius, earth_radius)) if v > 0.0]
        speed = vp if fluid or not vs_values else min(vs_values)
        minv = speed if minv is None else min(minv, speed)
    if minv is None or minv <= 0.0:
        die(f"Could not compute positive wave speed for DSM zone {zone['id']}")
    return ("fluid" if fluid else "solid"), minv


def depth_entries(zones: list[dict], params: dict[str, str], earth_radius: float, freq: float) -> tuple[list[tuple[float, int]], list[tuple[float, int]]]:
    center_depth = param_float(params, "CENTER_DEPTH_KM", 0.0)
    depth_block = param_float(params, "DEPTH_BLOCK_KM", 1.0)
    top_radius = earth_radius - center_depth + depth_block / 2.0
    bottom_radius = earth_radius - center_depth - depth_block / 2.0

    solid: list[tuple[float, int]] = []
    fluid: list[tuple[float, int]] = []
    for zone in zones:
        lo = max(zone["rmin"], bottom_radius)
        hi = min(zone["rmax"], top_radius)
        if hi <= lo:
            continue
        domain, speed = min_wave_speed(zone, lo, hi, earth_radius)
        wavelength = speed / freq
        spacing = wavelength / 5.0
        top_depth = earth_radius - hi
        bottom_depth = earth_radius - lo
        ndepth = max(5, int(math.ceil((bottom_depth - top_depth) / spacing)) + 1)
        values = []
        if ndepth == 1:
            values.append((top_depth, zone["id"]))
        else:
            for idep in range(ndepth):
                depth = top_depth + (bottom_depth - top_depth) * idep / (ndepth - 1)
                values.append((depth, zone["id"]))
        if domain == "fluid":
            fluid.extend(values)
        else:
            solid.extend(values)
    return solid, fluid


def write_depth_file(path: Path, entries: list[tuple[float, int]], ddepth: float, tolerance: float) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    sorted_entries = sorted(entries, key=lambda item: (round(item[0], 8), -item[1]))
    with path.open("w") as f:
        f.write(f"{ddepth:.6g} {tolerance:.6g}\n")
        f.write(f"{len(sorted_entries)}\n")
        for depth, zone_id in sorted_entries:
            f.write(f"{depth:10.4f} {zone_id:d}\n")


def min_angular_spacing(zones: list[dict], lo_radius: float, hi_radius: float, earth_radius: float, freq: float) -> float:
    min_spacing_deg = None
    for zone in zones:
        lo = max(zone["rmin"], lo_radius)
        hi = min(zone["rmax"], hi_radius)
        if hi <= lo:
            continue
        fluid = zone["qmu"] < 0.0
        for i in range(21):
            radius = lo + (hi - lo) * i / 20.0
            vp = min(v for v in (eval_poly(zone["vph"], radius, earth_radius), eval_poly(zone["vpv"], radius, earth_radius)) if v > 0.0)
            vs_values = [v for v in (eval_poly(zone["vsh"], radius, earth_radius), eval_poly(zone["vsv"], radius, earth_radius)) if v > 0.0]
            speed = vp if fluid or not vs_values else min(vs_values)
            wavelength = speed / freq
            spacing_km = wavelength / 5.0
            spacing_deg = spacing_km / (radius * math.pi / 180.0)
            if min_spacing_deg is None or spacing_deg < min_spacing_deg:
                min_spacing_deg = spacing_deg
    if min_spacing_deg is None:
        die("Could not compute angular spacing from DSM model")
    return min_spacing_deg


def write_distance_files(stations: list[dict], params: dict[str, str], zones: list[dict], earth_radius: float, freq: float) -> tuple[float, float, float, int, float, float, float, float, int]:
    center_lat = param_float(params, "CENTER_LATITUDE_IN_DEGREES", 0.0)
    center_lon = param_float(params, "CENTER_LONGITUDE_IN_DEGREES", 0.0)

    station_center_dists = [
        angular_distance_deg(sta["lat"], sta["lon"], center_lat, center_lon)
        for sta in stations
    ]
    if not station_center_dists:
        die("No stations found to compute distance range")
    avg_sta_dist = sum(station_center_dists) / len(station_center_dists)

    # Calculate wavelength-based spacing if not specified.
    spacing = optional_param_float(params, "DSM_DISTANCE_SPACING_DEGREES")
    if spacing is None:
        center_depth = param_float(params, "CENTER_DEPTH_KM", 0.0)
        depth_block = param_float(params, "DEPTH_BLOCK_KM", 1.0)
        top_radius = earth_radius - center_depth + depth_block / 2.0
        bottom_radius = earth_radius - center_depth - depth_block / 2.0
        spacing = min_angular_spacing(zones, bottom_radius, top_radius, earth_radius, freq)
        print(f"  Calculated DSM distance spacing: {spacing:.6f} degrees (based on wavelength)")
    else:
        print(f"  Using user-specified DSM distance spacing: {spacing:.6f} degrees")

    box_points = sem_box_sample_points(params)
    box_distances = [
        angular_distance_deg(sta["lat"], sta["lon"], lat, lon)
        for sta in stations
        for lat, lon in box_points
    ]
    box_min = min(box_distances)
    box_max = max(box_distances)

    explicit_buffer = optional_param_float(params, "DSM_BOX_TO_RECEIVER_DISTANCE_BUFFER_DEGREES")
    if explicit_buffer is None:
        explicit_buffer = optional_param_float(params, "DSM_DISTANCE_MARGIN_DEGREES")
    if explicit_buffer is None:
        distance_buffer = 4.0 * spacing
        print(f"  Using interpolation DSM box-to-receiver distance buffer: {distance_buffer:.6f} degrees")
    else:
        distance_buffer = max(0.0, explicit_buffer)
        print(f"  Using user-specified DSM box-to-receiver distance buffer: {distance_buffer:.6f} degrees")

    min_distance = sem_distance_floor(params, spacing)

    max_distance = optional_param_float(params, "DSM_MAX_DISTANCE_DEGREES")
    if max_distance is None:
        max_distance = 180.0 - spacing
        print(f"  Using default DSM maximum distance: {max_distance:.6f} degrees (180 deg minus one distance spacing)")
    max_distance = min(180.0, max_distance)
    if max_distance <= min_distance:
        die(f"DSM_MAX_DISTANCE_DEGREES ({max_distance:.6f}) must be larger than the minimum DSM distance ({min_distance:.6f})")
    if box_max >= max_distance:
        print(f"  Warning: SEM-box receiver max distance {box_max:.6f} deg is at/above DSM max {max_distance:.6f} deg; using capped range below the antipode")

    dmin = max(min_distance, box_min - distance_buffer)
    dmax = min(max_distance, box_max + distance_buffer)
    if dmax <= dmin:
        dmin = max(min_distance, max_distance - spacing)
        dmax = max_distance

    ndist = int(math.ceil((dmax - dmin) / spacing)) + 1
    distances = [dmin + i * spacing for i in range(ndist) if dmin + i * spacing <= max_distance + 1.0e-9]
    if not distances or distances[-1] < dmax - 1.0e-9:
        distances.append(dmax)
    ndist = len(distances)

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    with (DATA_DIR / "dist_solid_list").open("w") as f:
        f.write(f"{len(distances)}\n")
        for dist in distances:
            f.write(f"{dist:.6f}\n")
    (DATA_DIR / "dist_fluid_list").write_text((DATA_DIR / "dist_solid_list").read_text())

    print(f"  SEM-box receiver distance range: {box_min:.6f}-{box_max:.6f} deg sampled from {len(box_points)} box points and {len(stations)} stations")
    return dmin, dmax, spacing, ndist, avg_sta_dist, box_min, box_max, distance_buffer, len(box_points)



def next_power_nfreq(time_length: float, resolved_freq: float) -> int:
    nfreq = 1
    while nfreq / time_length <= resolved_freq:
        nfreq *= 2
    return nfreq



def write_dsm_model(path: Path, header: list[str], zones: list[dict], tail: list[str], time_length: float, nfreq: int, force_comp: str, params: dict[str, str]) -> None:
    # Single force vector: exp Fz Fr Ft 0 0 0
    # exp=5 is used in the reference script (dyn = 10^-5 N)
    if force_comp == "Fz":
        force = "5 1.0 0.0 0.0 0.0 0.0 0.0"
    elif force_comp == "Fr":
        force = "5 0.0 1.0 0.0 0.0 0.0 0.0"
    elif force_comp == "Ft":
        force = "5 0.0 0.0 1.0 0.0 0.0 0.0"
    else:
        die(f"Unknown force component: {force_comp}")

    output: list[str] = [f"{time_length:.1f}  {nfreq:d}   #time_series_length n_frequency"]
    output.extend(header[1:4])
    for zone in zones:
        output.extend(zone["lines"])

    if len(tail) < 7:
        die("DSM model tail is too short to update source/output-list lines")
    new_tail = list(tail)
    # Source depth is 0.0 for receiver (reciprocity)
    new_tail[0] = "0.0000  0.00  90.00  2  #source_depth (km) source_lat source_lon source_type (2 for single force)"
    new_tail[1] = f"{force}   #exp and source components (Fz, Fr, Ft active)"
    new_tail[2] = "\"DATA/depth_solid_list\" #file listing the Green's function depths in solid media"
    new_tail[3] = "\"DATA/dist_solid_list\" #file listing the Green's function distances in solid media"
    new_tail[4] = "\"DATA/depth_fluid_list\" #file listing the Green's function depths in fluid media"
    new_tail[5] = "\"DATA/dist_fluid_list\" #file listing the Green's function distances in fluid media"
    new_tail[6] = "1 #save displacement (choose 1) or velocity (choose 2) seismograms in solid media"
    output.extend(new_tail)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(output) + "\n")


def main() -> None:
    global DATA_DIR
    force_comp = "Fz"
    if len(sys.argv) > 1:
        DATA_DIR = Path(sys.argv[1]).resolve()
    if len(sys.argv) > 2:
        force_comp = sys.argv[2]
    
    if not PARAM_FILE.is_file():
        die(f"Missing parameter file: {PARAM_FILE}")
    if not SOURCE_DSM_MODEL_INPUT.is_file():
        die(f"Missing DSM model input: {SOURCE_DSM_MODEL_INPUT}")

    params = read_params(PARAM_FILE)
    stations = parse_stations(STATION_FILE)
    earth_radius = param_float(params, "R_EARTH", 6371.0)
    freq = param_float(params, "FREQ_RESOLVED", 1.0)
    phase = params.get("PHASE_BOX_TO_RECEIVER", "P").strip()

    time_length_override = optional_param_float(params, "TIME_LENGTH_DSM_BOX_TO_RECEIVER")

    header, zones, tail = parse_dsm_model(SOURCE_DSM_MODEL_INPUT, earth_radius)
    solid, fluid = depth_entries(zones, params, earth_radius, freq)
    dmin, dmax, dspacing, ndist, avg_sta_dist, box_min_dist, box_max_dist, box_distance_buffer, nbox_points = write_distance_files(stations, params, zones, earth_radius, freq)

    center_depth = param_float(params, "CENTER_DEPTH_KM", 0.0)
    depth_block = param_float(params, "DEPTH_BLOCK_KM", 0.0)
    timing_depths = [center_depth - depth_block / 2.0, center_depth + depth_block / 2.0]
    station_distances = [
        angular_distance_deg(sta["lat"], sta["lon"], param_float(params, "CENTER_LATITUDE_IN_DEGREES", 0.0), param_float(params, "CENTER_LONGITUDE_IN_DEGREES", 0.0))
        for sta in stations
    ]

    if time_length_override is not None:
        time_length = time_length_override
        time_length_source = "USER_SPECIFIED_TIME_LENGTH_DSM_BOX_TO_RECEIVER"
        time_length_source_label = "user-specified TIME_LENGTH_DSM_BOX_TO_RECEIVER"
        arrival_time_metadata = "NOT_COMPUTED"
        arrival_time_label = "not computed"
        selected_phase = "NOT_COMPUTED"
    else:
        timing_zones = parse_dsm_zones(SOURCE_DSM_MODEL_INPUT)
        arrival_items = [
            (direct_p_arrival_time(timing_zones, earth_radius, depth, 0.0, dist), depth, dist)
            for dist in station_distances
            for depth in timing_depths
        ]
        arrival_time, arrival_depth, arrival_dist = max(arrival_items, key=lambda item: item[0])
        selected_phase = phase_label_for_receiver(arrival_depth, earth_radius)
        time_length = math.ceil(arrival_time + 500.0)
        time_length_source = "DSM_MODEL_DIRECT_PKI_RAY_PLUS_500_S"
        time_length_source_label = "DSM model direct P/K/I arrival time + 500 s"
        arrival_time_metadata = f"{arrival_time:.3f}"
        arrival_time_label = f"{arrival_time:.3f} s"

    nfreq = next_power_nfreq(time_length, freq)

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    ddepth = param_float(params, "DSM_DDEPTH_FOR_STRESS_KM", 0.01)
    tolerance = param_float(params, "DSM_DEPTH_TOLERANCE_KM", 0.0025)
    write_depth_file(DATA_DIR / "depth_solid_list", solid, ddepth, tolerance)
    write_depth_file(DATA_DIR / "depth_fluid_list", fluid, ddepth, tolerance)
    
    model_name = f"dsm_model_{force_comp}"
    write_dsm_model(DATA_DIR / model_name, header, zones, tail, time_length, nfreq, force_comp, params)

    (DATA_DIR / f"box_to_recv_metadata_{force_comp}.env").write_text(
        "\n".join(
            [
                f"PHASE_BOX_TO_RECEIVER={phase}",
                f"TRAVEL_TIME_METHOD={time_length_source}",
                f"SELECTED_PHASE={selected_phase}",
                f"AVG_DISTANCE_DEG={avg_sta_dist:.6f}",
                f"MAX_STATION_DISTANCE_DEG={max(station_distances):.6f}",
                f"BOX_CENTER_DEPTH_KM={center_depth:.4f}",
                f"TIMING_BOX_DEPTH_MIN_KM={min(timing_depths):.4f}",
                f"TIMING_BOX_DEPTH_MAX_KM={max(timing_depths):.4f}",
                f"ARRIVAL_TIME_SEC={arrival_time_metadata}",
                f"TIME_SERIES_LENGTH={time_length:.1f}",
                f"TIME_SERIES_LENGTH_SOURCE={time_length_source}",
                f"NFREQUENCY={nfreq:d}",
                f"DSM_MAX_FREQ_HZ={nfreq / time_length:.6f}",
                f"FREQ_RESOLVED_HZ={freq:.6f}",
                f"DISTANCE_MIN_DEG={dmin:.6f}",
                f"DISTANCE_MAX_DEG={dmax:.6f}",
                f"DISTANCE_SPACING_DEG={dspacing:.6f}",
                f"NDISTANCE={ndist:d}",
                f"BOX_DISTANCE_MIN_DEG={box_min_dist:.6f}",
                f"BOX_DISTANCE_MAX_DEG={box_max_dist:.6f}",
                f"BOX_DISTANCE_BUFFER_DEG={box_distance_buffer:.6f}",
                f"BOX_DISTANCE_SAMPLE_POINTS={nbox_points:d}",
                f"NDEPTH_SOLID={len(solid):d}",
                f"NDEPTH_FLUID={len(fluid):d}",
                f"FORCE_COMP={force_comp}",
            ]
        )
        + "\n"
    )

    print(f"Prepared DSM box-to-receiver inputs for {force_comp} in {DATA_DIR}")
    print(f"  phase={phase} (selected {selected_phase}; {time_length_source_label})")
    print(f"  avg_station_center_distance={avg_sta_dist:.6f} deg, max_station_center_distance={max(station_distances):.6f} deg")
    print(f"  box_receiver_distance_range={box_min_dist:.6f}-{box_max_dist:.6f} deg, buffer={box_distance_buffer:.6f} deg")
    print(f"  arrival_time={arrival_time_label}")
    print(f"  time_series_length={time_length:.1f} s ({time_length_source_label})")
    print(f"  nfrequency={nfreq} (max DSM frequency={nfreq / time_length:.6f} Hz)")
    print(f"  solid_depths={len(solid)}, fluid_depths={len(fluid)}, distances={ndist}")


if __name__ == "__main__":
    main()
