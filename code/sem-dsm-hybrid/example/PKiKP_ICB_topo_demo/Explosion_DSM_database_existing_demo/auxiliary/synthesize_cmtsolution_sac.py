#!/usr/bin/env python3
"""
Build CMTSOLUTION synthetics from DSM moment-component SAC files.

The component SAC files are assumed to be computed for unit basis moments
with the exponent used in run.cmd. The default basis moment is 1.e7 dyn*cm
(1 N*m), matching the current run.cmd basis setup.
"""

from __future__ import annotations

import argparse
import math
import re
import shutil
import struct
from array import array
from pathlib import Path


COMPONENT_DIRS = ("Mzz", "Mrr", "Mtt", "Mzr", "Mzt", "Mrt")
SAC_EXTENSIONS = (".bhz", ".bhr", ".bht")
SAC_HEADER_BYTES = 632


def read_key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if ":" in line:
            key, value = line.split(":", 1)
            values[key.strip()] = value.strip().split()[0]
    return values


def read_cmtsolution(path: Path) -> dict[str, float]:
    raw = read_key_values(path)
    required = ("latitude", "longitude", "depth", "Mrr", "Mtt", "Mpp", "Mrt", "Mrp", "Mtp")
    missing = [key for key in required if key not in raw]
    if missing:
        raise RuntimeError(f"missing CMTSOLUTION fields in {path}: {', '.join(missing)}")
    return {key: float(raw[key]) for key in required}


def read_single_force_enz(path: Path) -> int:
    pattern = re.compile(r"^\s*SINGLE_FORCE_ENZ\s*=\s*([-+]?\d+)")
    for line in path.read_text().splitlines():
        match = pattern.match(line)
        if match:
            return int(match.group(1))
    raise RuntimeError(f"failed to find SINGLE_FORCE_ENZ in {path}")


def read_depths(path: Path) -> list[tuple[float, int]]:
    rows: list[tuple[float, int]] = []
    for iline, line in enumerate(path.read_text().splitlines(), start=1):
        parts = line.split()
        if iline <= 2 or not parts:
            continue
        rows.append((float(parts[0]), int(parts[1])))
    if not rows:
        raise RuntimeError(f"no depths found in {path}")
    return rows


def read_stations(path: Path) -> list[dict[str, float | str]]:
    stations: list[dict[str, float | str]] = []
    seen: set[tuple[str, str, float]] = set()
    for line in path.read_text().splitlines():
        parts = line.split()
        if len(parts) < 5 or parts[0].startswith("#"):
            continue
        key = (parts[0], parts[1], round(float(parts[4]), 8))
        if key in seen:
            continue
        seen.add(key)
        stations.append(
            {
                "net": parts[0],
                "sta": parts[1],
                "lon": float(parts[2]),
                "lat": float(parts[3]),
                "dist": float(parts[4]),
            }
        )
    if not stations:
        raise RuntimeError(f"no stations found in {path}")
    return stations


def azimuth_deg(src_lat: float, src_lon: float, sta_lat: float, sta_lon: float) -> float:
    lat1 = math.radians(src_lat)
    lat2 = math.radians(sta_lat)
    dlon = math.radians(sta_lon - src_lon)
    y = math.sin(dlon) * math.cos(lat2)
    x = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(dlon)
    az = math.degrees(math.atan2(y, x))
    if az < 0.0:
        az += 360.0
    return az


def rotate_cmt_to_zrt(cmt: dict[str, float], az_deg: float) -> dict[str, float]:
    """Rotate CMTSOLUTION tensor from (r, theta, phi) to station-dependent (z, r, t)."""
    az = math.radians(az_deg)
    caz = math.cos(az)
    saz = math.sin(az)

    # Old CMTSOLUTION basis: radial up, theta south, phi east.
    old = (
        (cmt["Mrr"], cmt["Mrt"], cmt["Mrp"]),
        (cmt["Mrt"], cmt["Mtt"], cmt["Mtp"]),
        (cmt["Mrp"], cmt["Mtp"], cmt["Mpp"]),
    )

    # New basis vectors in the old basis.
    # z: vertical/radial up
    # r: horizontal great-circle direction from source to station
    # t: z x r, completing a right-handed z-r-t basis
    basis = (
        (1.0, 0.0, 0.0),
        (0.0, -caz, saz),
        (0.0, -saz, -caz),
    )

    def mt(i: int, j: int) -> float:
        return sum(basis[i][a] * old[a][b] * basis[j][b] for a in range(3) for b in range(3))

    return {
        "Mzz": mt(0, 0),
        "Mrr": mt(1, 1),
        "Mtt": mt(2, 2),
        "Mzr": mt(0, 1),
        "Mzt": mt(0, 2),
        "Mrt": mt(1, 2),
    }


def station_base(zone: int, depth: float, dist: float) -> str:
    return f"L{zone:d}_dep{depth:.2f}_dist{dist:.2f}"


def output_base(station: dict[str, float | str], depth: float) -> str:
    return f"{station['net']}_{station['sta']}_dep{depth:.2f}"


def read_sac(path: Path) -> tuple[bytes, array]:
    data = path.read_bytes()
    if len(data) < SAC_HEADER_BYTES or (len(data) - SAC_HEADER_BYTES) % 4 != 0:
        raise RuntimeError(f"{path} does not look like a SAC binary file")
    header = data[:SAC_HEADER_BYTES]
    trace = array("f")
    trace.frombytes(data[SAC_HEADER_BYTES:])
    return header, trace


def write_sac(path: Path, header: bytes, trace: array) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + trace.tobytes())


def update_sac_depmin_depmax_depmen(header: bytes, trace: array) -> bytes:
    if not trace:
        return header
    values = list(trace)
    depmin = min(values)
    depmax = max(values)
    depmen = sum(values) / len(values)
    patched = bytearray(header)
    # SAC float header indices: depmin=1, depmax=2, depmen=56.
    struct.pack_into("f", patched, 4 * 1, depmin)
    struct.pack_into("f", patched, 4 * 2, depmax)
    struct.pack_into("f", patched, 4 * 56, depmen)
    return bytes(patched)


def combine_for_base(
    root: Path,
    base: str,
    ext: str,
    coefficients: dict[str, float],
    input_subdir: str,
    component_dirs: tuple[str, ...] = COMPONENT_DIRS,
) -> tuple[bytes, array]:
    header: bytes | None = None
    output: array | None = None

    for comp in component_dirs:
        coef = coefficients.get(comp, 0.0)
        if coef == 0.0:
            continue
        sac_path = root / comp / "OUTPUT_FILES" / input_subdir / f"{base}{ext}"
        if not sac_path.exists():
            raise RuntimeError(f"missing component SAC file: {sac_path}")
        this_header, trace = read_sac(sac_path)
        if header is None:
            header = this_header
            output = array("f", [0.0]) * len(trace)
        if output is None or len(output) != len(trace):
            raise RuntimeError(f"inconsistent trace length in {sac_path}")
        for i, value in enumerate(trace):
            output[i] += coef * value

    if header is None or output is None:
        # Use the first requested component as a template for an exactly zero result.
        template_component = component_dirs[0]
        template = root / template_component / "OUTPUT_FILES" / input_subdir / f"{base}{ext}"
        if not template.exists():
            raise RuntimeError(f"missing template SAC file: {template}")
        header, trace = read_sac(template)
        output = array("f", [0.0]) * len(trace)

    return update_sac_depmin_depmax_depmen(header, output), output


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Build CMTSOLUTION time-domain SAC synthetics from DSM moment-basis SAC files."
    )
    parser.add_argument("--cmt", default="DATA/CMTSOLUTION")
    parser.add_argument("--sem-par-file", default="WORK/SPECFEM3D/DATA/Par_file")
    parser.add_argument("--reference-component", default="Mrr")
    parser.add_argument("--dsm-root", default="WORK/DSM/1D_tele_syn")
    parser.add_argument("--input-subdir", default="disp_solid_time_sac")
    parser.add_argument("--output-dir", default="OUTPUT_FILES/CMTSOLUTION_time_sac")
    parser.add_argument("--basis-moment-dyn-cm", type=float, default=1.0e7)
    parser.add_argument("--clean", action="store_true", help="Remove existing files in output-dir before writing.")
    args = parser.parse_args()

    run_root = Path.cwd()
    dsm_root = (run_root / args.dsm_root).resolve()
    cmt_path = (run_root / args.cmt).resolve()
    par_path = (run_root / args.sem_par_file).resolve()
    ref_dir = dsm_root / args.reference_component
    depth_path = ref_dir / "DATA" / "depth_solid_list"
    station_path = ref_dir / "DATA" / "station_distances.txt"
    output_dir = Path(args.output_dir)
    if not output_dir.is_absolute():
        output_dir = dsm_root / output_dir

    cmt = read_cmtsolution(cmt_path)
    source_mode = read_single_force_enz(par_path)
    if source_mode not in (-1, 0):
        raise RuntimeError(
            f"SINGLE_FORCE_ENZ={source_mode} is not supported here; expected -1 explosion or 0 full moment tensor"
        )
    if source_mode == -1 and args.reference_component == "Mrr":
        ref_dir = dsm_root / "explosion"
        depth_path = ref_dir / "DATA" / "depth_solid_list"
        station_path = ref_dir / "DATA" / "station_distances.txt"
    depths = read_depths(depth_path)
    stations = read_stations(station_path)

    if args.clean and output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    nout = 0
    nskip = 0
    for depth, zone in depths:
        for station in stations:
            dist = float(station["dist"])
            input_base = station_base(zone, depth, dist)
            out_base = output_base(station, depth)
            template = ref_dir / "OUTPUT_FILES" / args.input_subdir / f"{input_base}{SAC_EXTENSIONS[0]}"
            if not template.exists():
                nskip += 1
                print(f"Skipping station row without converted SAC template: {template}")
                continue

            if source_mode == -1:
                # Step 6 computes the complete explosion moment tensor in one DSM run.
                coefficients = {"explosion": 1.0}
                component_dirs = ("explosion",)
                mode_text = "explosion"
            else:
                az = azimuth_deg(cmt["latitude"], cmt["longitude"], float(station["lat"]), float(station["lon"]))
                rotated = rotate_cmt_to_zrt(cmt, az)
                coefficients = {key: value / args.basis_moment_dyn_cm for key, value in rotated.items()}
                component_dirs = COMPONENT_DIRS
                mode_text = "full moment tensor"

            for ext in SAC_EXTENSIONS:
                header, trace = combine_for_base(dsm_root, input_base, ext, coefficients, args.input_subdir, component_dirs)
                write_sac(output_dir / f"{out_base}{ext}", header, trace)
                nout += 1

    print(f"Source mode: {mode_text} (SINGLE_FORCE_ENZ={source_mode})")
    print(f"Basis moment: {args.basis_moment_dyn_cm:.6e} dyn*cm")
    print(f"Wrote {nout} SAC files to {output_dir}")
    if nskip:
        print(f"Skipped {nskip} stale station/depth rows without converted SAC files")


if __name__ == "__main__":
    main()
