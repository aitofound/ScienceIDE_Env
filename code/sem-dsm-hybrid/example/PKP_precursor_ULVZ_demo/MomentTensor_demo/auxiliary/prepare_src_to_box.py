#!/usr/bin/env python3
"""Prepare DSM source-to-box inputs for the PKP precursor ULVZ demo."""

from __future__ import annotations

import math
import os
import re
import sys
from pathlib import Path

from dsm_direct_pki_timing import angular_distance_deg, direct_p_arrival_time, parse_dsm_zones, source_to_box_phase_label


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


HERE = Path(__file__).resolve().parent
SEM_DIR = (HERE / "..").resolve()
PARAM_FILE = SEM_DIR / "DATA/Par_file_SEM_DSM"
CMT_FILE = SEM_DIR / "DATA/CMTSOLUTION"
STATION_FILE = SEM_DIR / "DATA/tele_station.txt"
SOURCE_DSM_MODEL_INPUT = SEM_DIR / "WORK/SPECFEM3D/DATA/dsm_model_input"

# Default DATA_DIR, but can be overridden by argument
DATA_DIR = (SEM_DIR / "WORK/DSM/src_to_box/DATA").resolve()
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


def parse_cmt(path: Path) -> dict[str, float]:
    if not path.is_file():
        die(f"Missing CMTSOLUTION: {path}")

    values: dict[str, float] = {}
    wanted = {"latitude", "longitude", "depth", "Mrr", "Mtt", "Mpp", "Mrt", "Mrp", "Mtp"}
    for line in path.read_text().splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        if key in wanted:
            values[key] = clean_number(value.split()[0])

    missing = sorted({"latitude", "longitude", "depth"} - values.keys())
    if missing:
        die(f"CMTSOLUTION is missing required fields: {', '.join(missing)}")
    return values


def parse_stations(path: Path) -> list[dict[str, float | str]]:
    if not path.is_file():
        return []
    stations: list[dict[str, float | str]] = []
    for line in path.read_text().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = line.split()
        if len(fields) < 4:
            continue
        stations.append({"net": fields[0], "sta": fields[1], "lon": clean_number(fields[2]), "lat": clean_number(fields[3])})
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
    depth_block = param_float(params, "DEPTH_BLOCK_KM", 0.0)
    timing_depths = [center_depth - depth_block / 2.0, center_depth + depth_block / 2.0]
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
    with path.open("w") as f:
        f.write(f"{ddepth:.6g} {tolerance:.6g}\n")
        f.write(f"{len(entries)}\n")
        for depth, zone_id in entries:
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


def write_distance_files(center_dist: float, params: dict[str, str], zones: list[dict], earth_radius: float, freq: float) -> tuple[float, float, float, int]:
    width_xi = param_float(params, "ANGULAR_WIDTH_XI_IN_DEGREES", 1.0)
    width_eta = param_float(params, "ANGULAR_WIDTH_ETA_IN_DEGREES", 1.0)

    # Calculate wavelength-based spacing if not specified
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
    
    margin = optional_param_float(params, "DSM_DISTANCE_MARGIN_DEGREES")
    if margin is None:
        margin = 0.8 * max(width_xi, width_eta)
    min_distance = sem_distance_floor(params, spacing)
    max_distance = optional_param_float(params, "DSM_MAX_DISTANCE_DEGREES")
    if max_distance is None:
        max_distance = 180.0 - spacing
        print(f"  Using default DSM maximum distance: {max_distance:.6f} degrees (180 deg minus one distance spacing)")
    max_distance = min(180.0, max_distance)
    if max_distance <= min_distance:
        die(f"DSM_MAX_DISTANCE_DEGREES ({max_distance:.6f}) must be larger than the minimum DSM distance ({min_distance:.6f})")
    if center_dist >= max_distance:
        print(f"  Warning: center distance {center_dist:.6f} deg is at/above DSM max {max_distance:.6f} deg; using capped two-sided range below the antipode")
    dmin = max(min_distance, min(center_dist - margin, max_distance - spacing))
    dmax = min(max_distance, center_dist + margin)
    if dmax <= dmin:
        dmin = max(min_distance, max_distance - spacing)
        dmax = max_distance
    ndist = int(math.ceil((dmax - dmin) / spacing)) + 1
    distances = [dmin + i * spacing for i in range(ndist)]
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    with (DATA_DIR / "dist_solid_list").open("w") as f:
        f.write(f"{len(distances)}\n")
        for dist in distances:
            f.write(f"{dist:.6f}\n")
    (DATA_DIR / "dist_fluid_list").write_text((DATA_DIR / "dist_solid_list").read_text())
    return dmin, dmax, spacing, ndist


def next_power_nfreq(time_length: float, resolved_freq: float) -> int:
    nfreq = 1
    while nfreq / time_length <= resolved_freq:
        nfreq *= 2
    return nfreq



def source_components(source_mode: str) -> list[str]:
    if source_mode == "-1":
        return ["explosion"]
    if source_mode == "0":
        return ["Mzz", "Mrr", "Mtt", "Mzr", "Mzt", "Mrt"]
    if source_mode == "1":
        return ["Fr"]
    if source_mode == "2":
        return ["Ft"]
    if source_mode == "3":
        return ["Fz"]
    die(f"Unsupported SINGLE_FORCE_ENZ for source-to-box DSM setup: {source_mode}")



def cmt_moment_vector(cmt: dict[str, float]) -> str:
    keys = ("Mrr", "Mtt", "Mpp", "Mrt", "Mrp", "Mtp")
    missing = [key for key in keys if key not in cmt]
    if missing:
        die(f"CMTSOLUTION is missing moment fields required for SINGLE_FORCE_ENZ=-1: {', '.join(missing)}")
    max_abs = max(abs(cmt[key]) for key in keys)
    if max_abs == 0.0:
        die("CMTSOLUTION moment tensor is zero; cannot use SINGLE_FORCE_ENZ=-1")
    tol = 1.0e-6 * max_abs
    if (
        abs(cmt["Mrr"] - cmt["Mtt"]) > tol
        or abs(cmt["Mtt"] - cmt["Mpp"]) > tol
        or abs(cmt["Mrt"]) > tol
        or abs(cmt["Mrp"]) > tol
        or abs(cmt["Mtp"]) > tol
    ):
        die(
            "SINGLE_FORCE_ENZ=-1 requires an explosion source: "
            "Mrr, Mtt, and Mpp must be equal and non-zero; "
            "Mrt, Mrp, and Mtp must be zero. "
            "Use SINGLE_FORCE_ENZ=0 for a general moment tensor."
        )
    exponent = math.floor(math.log10(max_abs))
    scale = 10.0 ** exponent
    comps = [cmt[key] / scale for key in keys]
    return " ".join([str(exponent), *(f"{value:.12g}" for value in comps)])

def source_vector(source_name: str, params: dict[str, str], cmt: dict[str, float]) -> tuple[int, str]:
    source_mode = params.get("SINGLE_FORCE_ENZ", "-1").strip()
    allowed = source_components(source_mode)
    if source_name not in allowed:
        die(f"Source component {source_name} is not valid for SINGLE_FORCE_ENZ={source_mode}; expected one of: {', '.join(allowed)}")

    if source_name == "explosion":
        return 1, "7 1.0 1.0 1.0 0.0 0.0 0.0"

    moment_tensor_vectors = {
        "Mzz": "7 1.0 0.0 0.0 0.0 0.0 0.0",
        "Mrr": "7 0.0 1.0 0.0 0.0 0.0 0.0",
        "Mtt": "7 0.0 0.0 1.0 0.0 0.0 0.0",
        "Mzr": "7 0.0 0.0 0.0 1.0 0.0 0.0",
        "Mzt": "7 0.0 0.0 0.0 0.0 1.0 0.0",
        "Mrt": "7 0.0 0.0 0.0 0.0 0.0 1.0",
    }
    if source_name in moment_tensor_vectors:
        return 1, moment_tensor_vectors[source_name]

    single_force_vectors = {
        "Fz": "5 1.0 0.0 0.0 0.0 0.0 0.0",
        "Fr": "5 0.0 1.0 0.0 0.0 0.0 0.0",
        "Ft": "5 0.0 0.0 1.0 0.0 0.0 0.0",
    }
    if source_name in single_force_vectors:
        return 2, single_force_vectors[source_name]

    die(f"Unknown source component: {source_name}")


def write_dsm_model(path: Path, header: list[str], zones: list[dict], tail: list[str], time_length: float, nfreq: int, cmt: dict[str, float], params: dict[str, str], source_name: str) -> None:
    moment_or_force, force = source_vector(source_name, params, cmt)
    output: list[str] = [f"{time_length:.1f}  {nfreq:d}   #time_series_length n_frequency"]
    output.extend(header[1:4])
    for zone in zones:
        output.extend(zone["lines"])

    if len(tail) < 7:
        die("DSM model tail is too short to update source/output-list lines")
    new_tail = list(tail)
    new_tail[0] = f"{cmt['depth']:.4f}  0.00  90.00  {moment_or_force:d}  #source_depth (km) source_lat source_lon source_type"
    new_tail[1] = f"{force}   #exp and source components"
    new_tail[2] = "\"DATA/depth_solid_list\" #file listing the Green's function depths in solid media"
    new_tail[3] = "\"DATA/dist_solid_list\" #file listing the Green's function distances in solid media"
    new_tail[4] = "\"DATA/depth_fluid_list\" #file listing the Green's function depths in fluid media"
    new_tail[5] = "\"DATA/dist_fluid_list\" #file listing the Green's function distances in fluid media"
    new_tail[6] = "2 #save displacement (choose 1) or velocity (choose 2) seismograms in solid media"
    output.extend(new_tail)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(output) + "\n")


def main() -> None:
    global DATA_DIR
    source_name = None
    if len(sys.argv) > 1:
        DATA_DIR = Path(sys.argv[1]).resolve()
    if len(sys.argv) > 2:
        source_name = sys.argv[2]
    
    if not PARAM_FILE.is_file():
        die(f"Missing parameter file: {PARAM_FILE}")
    if not SOURCE_DSM_MODEL_INPUT.is_file():
        die(f"Missing DSM model input: {SOURCE_DSM_MODEL_INPUT}")

    params = read_params(PARAM_FILE)
    source_mode = params.get("SINGLE_FORCE_ENZ", "-1").strip()
    sources = source_components(source_mode)
    if source_name is None:
        source_name = sources[0]
    if source_name not in sources:
        die(f"Source component {source_name} is not valid for SINGLE_FORCE_ENZ={source_mode}; expected one of: {', '.join(sources)}")

    cmt = parse_cmt(CMT_FILE)
    earth_radius = param_float(params, "R_EARTH", 6371.0)
    freq = param_float(params, "FREQ_RESOLVED", 1.0)
    phase = params.get("PHASE_SOURCE_TO_BOX", "PKP").strip()
    center_lat = param_float(params, "CENTER_LATITUDE_IN_DEGREES", 0.0)
    center_lon = param_float(params, "CENTER_LONGITUDE_IN_DEGREES", 0.0)
    center_depth = param_float(params, "CENTER_DEPTH_KM", 0.0)
    depth_block = param_float(params, "DEPTH_BLOCK_KM", 0.0)
    timing_depths = [center_depth - depth_block / 2.0, center_depth + depth_block / 2.0]

    time_length_override = optional_param_float(params, "TIME_LENGTH_DSM_SOURCE_TO_BOX")
    timing_sources = [
        {"net": "CMT", "sta": "source", "lat": cmt["latitude"], "lon": cmt["longitude"]}
    ]
    timing_source_depth = cmt["depth"]
    timing_distances = [
        angular_distance_deg(float(src["lat"]), float(src["lon"]), center_lat, center_lon)
        for src in timing_sources
    ]
    center_dist = sum(timing_distances) / len(timing_distances)
    min_timing_dist = min(timing_distances)
    max_timing_dist = max(timing_distances)

    timing_zones = parse_dsm_zones(SOURCE_DSM_MODEL_INPUT)
    arrival_items = [
        (direct_p_arrival_time(timing_zones, earth_radius, timing_source_depth, depth, dist), depth, dist)
        for dist in timing_distances
        for depth in timing_depths
    ]
    arrival_time, arrival_depth, arrival_dist = max(arrival_items, key=lambda item: item[0])
    selected_phase = source_to_box_phase_label(arrival_depth, arrival_dist, earth_radius)

    if time_length_override is not None:
        time_length = time_length_override
        time_length_source = "USER_SPECIFIED_TIME_LENGTH_DSM_SOURCE_TO_BOX"
        time_length_source_label = "user-specified TIME_LENGTH_DSM_SOURCE_TO_BOX"
    else:
        time_length = math.ceil(arrival_time + 500.0)
        time_length_source = "DSM_MODEL_DIRECT_PKI_RAY_PLUS_500_S"
        time_length_source_label = "DSM model direct P/K/I arrival time + 500 s"

    nfreq = next_power_nfreq(time_length, freq)

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    (DATA_DIR / "dsm_model_input").write_text(SOURCE_DSM_MODEL_INPUT.read_text())

    header, zones, tail = parse_dsm_model(SOURCE_DSM_MODEL_INPUT, earth_radius)
    solid, fluid = depth_entries(zones, params, earth_radius, freq)
    ddepth = param_float(params, "DSM_DDEPTH_FOR_STRESS_KM", 0.01)
    tolerance = param_float(params, "DSM_DEPTH_TOLERANCE_KM", 0.0025)
    write_depth_file(DATA_DIR / "depth_solid_list", solid, ddepth, tolerance)
    write_depth_file(DATA_DIR / "depth_fluid_list", fluid, ddepth, tolerance)
    dmin, dmax, dspacing, ndist = write_distance_files(center_dist, params, zones, earth_radius, freq)
    write_dsm_model(DATA_DIR / f"dsm_model_{source_name}", header, zones, tail, time_length, nfreq, cmt, params, source_name)
    if len(sources) == 1:
        write_dsm_model(DATA_DIR / "dsm_model", header, zones, tail, time_length, nfreq, cmt, params, source_name)

    (DATA_DIR / "src_to_box_metadata.env").write_text(
        "\n".join(
            [
                f"PHASE_SOURCE_TO_BOX={phase}",
                f"TRAVEL_TIME_METHOD={time_length_source}",
                f"SELECTED_PHASE={selected_phase}",
                f"SOURCE_DEPTH_KM={cmt['depth']:.4f}",
                f"SOURCE_LAT={cmt['latitude']:.6f}",
                f"SOURCE_LON={cmt['longitude']:.6f}",
                f"BOX_CENTER_LAT={center_lat:.6f}",
                f"BOX_CENTER_LON={center_lon:.6f}",
                f"CENTER_DISTANCE_DEG={center_dist:.6f}",
                f"TIMING_SOURCE_COUNT={len(timing_sources):d}",
                f"TIMING_SOURCE_DEPTH_KM={timing_source_depth:.4f}",
                f"TIMING_RECEIVER_DEPTH_MIN_KM={min(timing_depths):.4f}",
                f"TIMING_RECEIVER_DEPTH_MAX_KM={max(timing_depths):.4f}",
                f"TIMING_DISTANCE_MIN_DEG={min_timing_dist:.6f}",
                f"TIMING_DISTANCE_MAX_DEG={max_timing_dist:.6f}",
                f"ARRIVAL_TIME_SEC={arrival_time:.3f}",
                f"TIME_SERIES_LENGTH={time_length:.1f}",
                f"TIME_SERIES_LENGTH_SOURCE={time_length_source}",
                f"NFREQUENCY={nfreq:d}",
                f"DSM_MAX_FREQ_HZ={nfreq / time_length:.6f}",
                f"FREQ_RESOLVED_HZ={freq:.6f}",
                f"DISTANCE_MIN_DEG={dmin:.6f}",
                f"DISTANCE_MAX_DEG={dmax:.6f}",
                f"DISTANCE_SPACING_DEG={dspacing:.6f}",
                f"NDISTANCE={ndist:d}",
                f"NDEPTH_SOLID={len(solid):d}",
                f"NDEPTH_FLUID={len(fluid):d}",
                f"SOURCE_MODE={source_mode}",
                f"SOURCE_COMPONENT={source_name}",
                f"SOURCE_COMPONENTS=\"{' '.join(sources)}\"",
            ]
        )
        + "\n"
    )

    print(f"Prepared DSM source-to-box inputs in {DATA_DIR}")
    print(f"  source_component={source_name} (SINGLE_FORCE_ENZ={source_mode})")
    print(f"  phase={phase} (selected {selected_phase}; {time_length_source_label})")
    print(f"  source_depth={cmt['depth']:.4f} km")
    print(f"  center_distance={center_dist:.6f} deg")
    print(f"  timing_sources={len(timing_sources)} depth={timing_source_depth:.4f} km distance_range={min_timing_dist:.6f}-{max_timing_dist:.6f} deg")
    print(f"  arrival_time={arrival_time:.3f} s")
    print(f"  time_series_length={time_length:.1f} s ({time_length_source_label})")
    print(f"  nfrequency={nfreq} (max DSM frequency={nfreq / time_length:.6f} Hz)")
    print(f"  solid_depths={len(solid)}, fluid_depths={len(fluid)}, distances={ndist}")


if __name__ == "__main__":
    main()
