#!/usr/bin/env python3
"""Assemble final 3-D SEM-DSM SAC synthetics from 1-D DSM and coupling terms."""

from __future__ import annotations

import argparse
import math
import shutil
import struct
import subprocess
import sys
from array import array
from pathlib import Path


SAC_HEADER_BYTES = 632
SAC_FLOAT_COUNT = 70
SAC_INT_COUNT = 40


def read_key_values(path: Path, separator: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or separator not in line:
            continue
        key, value = line.split(separator, 1)
        values[key.strip()] = value.strip().split()[0]
    return values


def read_cmtsolution(path: Path) -> dict[str, float]:
    raw = read_key_values(path, ":")
    return {
        "latitude": float(raw["latitude"]),
        "longitude": float(raw["longitude"]),
    }


def read_injected_metadata(path: Path) -> dict[str, float]:
    raw = read_key_values(path, "=")
    values: dict[str, float] = {}
    for key, value in raw.items():
        try:
            values[key] = float(value)
        except ValueError:
            continue
    return values


def read_stations(path: Path) -> list[dict[str, float | str]]:
    stations: list[dict[str, float | str]] = []
    for line in path.read_text().splitlines():
        parts = line.split()
        if len(parts) < 4 or parts[0].startswith("#"):
            continue
        name = f"{parts[0]}_{parts[1]}"
        stations.append(
            {
                "name": name,
                "net": parts[0],
                "sta": parts[1],
                "lon": float(parts[2]),
                "lat": float(parts[3]),
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


def read_station_distances(path: Path) -> dict[str, float]:
    out: dict[str, float] = {}
    for line in path.read_text().splitlines():
        parts = line.split()
        if len(parts) < 5 or parts[0].startswith("#"):
            continue
        out[f"{parts[0]}_{parts[1]}"] = float(parts[4])
    if not out:
        raise RuntimeError(f"no station distances found in {path}")
    return out


def read_sac(path: Path) -> tuple[bytearray, array]:
    data = path.read_bytes()
    if len(data) < SAC_HEADER_BYTES or (len(data) - SAC_HEADER_BYTES) % 4 != 0:
        raise RuntimeError(f"{path} does not look like a SAC binary file")
    trace = array("f")
    trace.frombytes(data[SAC_HEADER_BYTES:])
    npts = sac_int(data[:SAC_HEADER_BYTES], 9)
    if npts != len(trace):
        raise RuntimeError(f"{path} has npts={npts}, but {len(trace)} samples")
    return bytearray(data[:SAC_HEADER_BYTES]), trace


def sac_float(header: bytes | bytearray, index: int) -> float:
    return struct.unpack_from("f", header, 4 * index)[0]


def sac_int(header: bytes | bytearray, index: int) -> int:
    return struct.unpack_from("i", header, 4 * SAC_FLOAT_COUNT + 4 * index)[0]


def set_sac_float(header: bytearray, index: int, value: float) -> None:
    struct.pack_into("f", header, 4 * index, float(value))


def set_sac_int(header: bytearray, index: int, value: int) -> None:
    struct.pack_into("i", header, 4 * SAC_FLOAT_COUNT + 4 * index, int(value))


def refresh_sac_header(header: bytearray, trace: array, begin: float) -> bytearray:
    delta = sac_float(header, 0)
    npts = len(trace)
    set_sac_float(header, 5, begin)
    set_sac_float(header, 6, begin + (npts - 1) * delta if npts else begin)
    set_sac_int(header, 9, npts)
    if trace:
        values = list(trace)
        set_sac_float(header, 1, min(values))
        set_sac_float(header, 2, max(values))
        set_sac_float(header, 56, sum(values) / len(values))
    return header


def write_sac(path: Path, header: bytearray, trace: array) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(header) + trace.tobytes())


def copy_tree_clean(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def has_sac_files(path: Path) -> bool:
    return path.is_dir() and any(path.glob("*.bh?"))


def ensure_one_d_sac_dir(root: Path, one_d_dir: Path) -> None:
    if has_sac_files(one_d_dir):
        return

    dsm_root = root / "WORK/DSM/1D_tele_syn"
    components = ("Mzz", "Mrr", "Mtt", "Mzr", "Mzt", "Mrt")
    if not dsm_root.is_dir():
        raise RuntimeError(f"missing 1-D DSM work directory: {dsm_root}")

    print(f"Missing 1-D DSM CMTSOLUTION SAC directory: {one_d_dir}")
    print("Rebuilding it from Step 6 component DSM outputs...")

    for component in components:
        component_dir = dsm_root / component
        sac_dir = component_dir / "OUTPUT_FILES/disp_solid_time_sac"
        if has_sac_files(sac_dir):
            continue
        submit_script = component_dir / f"submit_freq2sac_{component}.cmd"
        if not submit_script.is_file():
            raise RuntimeError(f"missing freq-to-SAC script for {component}: {submit_script}")
        print(f"  Running freq-to-SAC conversion for {component}")
        subprocess.run(["bash", submit_script.name], cwd=component_dir, check=True)
        if not has_sac_files(sac_dir):
            raise RuntimeError(f"freq-to-SAC conversion did not produce SAC files: {sac_dir}")

    synth_script = root / "auxiliary/synthesize_cmtsolution_sac.py"
    if not synth_script.is_file():
        raise RuntimeError(f"missing synthesis helper: {synth_script}")
    print("  Synthesizing CMTSOLUTION 1-D DSM SAC files")
    subprocess.run(
        [sys.executable, str(synth_script), "--input-subdir", "disp_solid_time_sac", "--clean"],
        cwd=root,
        check=True,
    )
    if not has_sac_files(one_d_dir):
        raise RuntimeError(f"synthesis did not produce SAC files: {one_d_dir}")


def interpolate(trace: array, delta: float, start: float, time: float) -> float:
    x = (time - start) / delta
    i = math.floor(x)
    if i < 0 or i >= len(trace):
        return 0.0
    if i == len(trace) - 1:
        return float(trace[i]) if abs(x - i) < 1.0e-6 else 0.0
    frac = x - i
    return float(trace[i]) * (1.0 - frac) + float(trace[i + 1]) * frac


def trim_trace_before(trace: array, delta: float, start: float, cutoff: float) -> tuple[array, float]:
    """Discard samples before cutoff and return the grid-aligned new begin time."""
    if delta <= 0.0:
        raise RuntimeError(f"invalid SAC sample interval: {delta}")
    first = 0
    while first < len(trace) and start + first * delta < cutoff:
        first += 1
    if first == len(trace):
        raise RuntimeError(f"cutoff {cutoff} s leaves no SAC samples")
    return trace[first:], start + first * delta


def scatter_on_1d_grid(
    header_1d: bytearray,
    trace_scatter: array,
    scatter_delta: float,
    scatter_start: float,
    scatter_time_shift: float,
) -> array:
    npts = sac_int(header_1d, 9)
    delta_1d = sac_float(header_1d, 0)
    begin_1d = sac_float(header_1d, 5)
    out = array("f", [0.0]) * npts
    for i in range(npts):
        target_time = begin_1d + i * delta_1d
        source_time = target_time - scatter_time_shift
        if source_time < 0.0:
            continue
        out[i] = interpolate(trace_scatter, scatter_delta, scatter_start, source_time)
    return out


def add_arrays(a: array, b: array) -> array:
    if len(a) != len(b):
        raise RuntimeError(f"cannot add arrays with lengths {len(a)} and {len(b)}")
    out = array("f", [0.0]) * len(a)
    for i, value in enumerate(a):
        out[i] = value + b[i]
    return out


def station_1d_files(one_d_dir: Path, station: dict[str, float | str]) -> dict[str, Path]:
    station_name = str(station["name"])
    sta = str(station.get("sta", ""))
    net = str(station.get("net", ""))
    aliases = [station_name]
    if sta and net:
        aliases.extend([f"{sta}_{net}", f"{net}_{sta}"])

    matches: list[Path] = []
    for alias in aliases:
        matches = sorted(one_d_dir.glob(f"{alias}_dep*.bhz"))
        if matches:
            break
    if not matches and sta:
        matches = sorted(one_d_dir.glob(f"{sta}_*_dep*.bhz"))
    if matches:
        stem = matches[0].name[:-4]
        base = stem[:-4] if stem.endswith(".bhz") else matches[0].stem
    else:
        data_dir = one_d_dir / "DATA"
        if not data_dir.is_dir():
            data_dir = one_d_dir.parent.parent / "DATA"
        depths = read_depths(data_dir / "depth_solid_list")
        distances = read_station_distances(data_dir / "station_distances.txt")
        distance_key = next((alias for alias in aliases if alias in distances), None)
        if distance_key is None:
            raise RuntimeError(f"missing station {station_name} in {data_dir / 'station_distances.txt'}")
        depth, zone = depths[0]
        base = f"L{zone:d}_dep{depth:.2f}_dist{distances[distance_key]:.2f}"
        matches = [one_d_dir / f"{base}.bhz"]
    return {
        "z": matches[0],
        "r": one_d_dir / f"{base}.bhr",
        "t": one_d_dir / f"{base}.bht",
        "base": one_d_dir / f"{base}",
    }


def shifted_scatter_copy(src: Path, dst: Path, scatter_time_shift: float) -> None:
    header, trace = read_sac(src)
    delta = sac_float(header, 0)
    begin = sac_float(header, 5)
    trace, begin = trim_trace_before(trace, delta, begin, 0.0)
    refresh_sac_header(header, trace, begin + scatter_time_shift)
    write_sac(dst, header, trace)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--one-d-dir", default="WORK/DSM/1D_tele_syn/OUTPUT_FILES/CMTSOLUTION_time_sac")
    parser.add_argument("--coupling-dir", default="WORK/Coupling/OUTPUT_FILES")
    parser.add_argument("--metadata", default="WORK/InjectedWaves/DATA/injected_waves_metadata.env")
    parser.add_argument("--stations", default="DATA/tele_station.txt")
    parser.add_argument("--cmt", default="DATA/CMTSOLUTION")
    parser.add_argument("--output-root", default="OUTPUT_FILES")
    args = parser.parse_args()

    root = Path.cwd()
    one_d_dir = (root / args.one_d_dir).resolve()
    coupling_dir = (root / args.coupling_dir).resolve()
    output_root = (root / args.output_root).resolve()
    out_1d = output_root / "1D_DSM"
    out_scatter = output_root / "scatering_wave_Coupling"
    out_3d = output_root / "3D_SEM_DSM"

    metadata = read_injected_metadata((root / args.metadata).resolve())
    scatter_time_shift = metadata["TIME_START_CUT"]
    cmt = read_cmtsolution((root / args.cmt).resolve())
    stations = read_stations((root / args.stations).resolve())

    ensure_one_d_sac_dir(root, one_d_dir)
    if not coupling_dir.is_dir():
        raise RuntimeError(f"missing coupling SAC directory: {coupling_dir}")
    coupling_sacs = sorted(coupling_dir.glob("*.bh?"))
    if not coupling_sacs:
        slurm_logs = sorted(coupling_dir.glob("slurm-coupling-*.out"))
        hint = f"; latest coupling log: {slurm_logs[-1]}" if slurm_logs else ""
        raise RuntimeError(
            f"no coupling SAC files found in {coupling_dir}{hint}. "
            "Rerun step9 and wait for the coupling job to finish successfully before step10."
        )

    output_root.mkdir(parents=True, exist_ok=True)
    copy_tree_clean(one_d_dir, out_1d)
    if out_scatter.exists():
        shutil.rmtree(out_scatter)
    out_scatter.mkdir(parents=True, exist_ok=True)
    if out_3d.exists():
        shutil.rmtree(out_3d)
    out_3d.mkdir(parents=True, exist_ok=True)

    nout = 0
    for src in sorted(coupling_dir.glob("*.bh?")):
        shifted_scatter_copy(src, out_scatter / src.name, scatter_time_shift)

    for station in stations:
        name = str(station["name"])
        files = station_1d_files(one_d_dir, station)
        for key in ("z", "r", "t"):
            if not files[key].exists():
                raise RuntimeError(f"missing 1-D SAC file: {files[key]}")

        header_z, trace_z = read_sac(files["z"])
        header_r, trace_r = read_sac(files["r"])
        header_t, trace_t = read_sac(files["t"])
        write_sac(out_1d / f"{name}.bhz", header_z.copy(), trace_z)
        write_sac(out_1d / f"{name}.bhr", header_r.copy(), trace_r)
        write_sac(out_1d / f"{name}.bht", header_t.copy(), trace_t)
        delta_scatter = None
        scatter_grid: dict[str, array] = {}
        for comp, ext in (("z", "bhz"), ("n", "bhn"), ("e", "bhe")):
            scatter_path = coupling_dir / f"{name}.{ext}"
            if not scatter_path.exists():
                raise RuntimeError(f"missing coupling SAC file: {scatter_path}")
            header_s, trace_s = read_sac(scatter_path)
            delta = sac_float(header_s, 0)
            if delta_scatter is None:
                delta_scatter = delta
            scatter_start = sac_float(header_s, 5)
            scatter_grid[comp] = scatter_on_1d_grid(
                header_z,
                trace_s,
                delta,
                scatter_start,
                scatter_time_shift,
            )

        az = math.radians(azimuth_deg(cmt["latitude"], cmt["longitude"], float(station["lat"]), float(station["lon"])))
        trace_n = array("f", [0.0]) * len(trace_r)
        trace_e = array("f", [0.0]) * len(trace_r)
        for i in range(len(trace_r)):
            trace_n[i] = trace_r[i] * math.cos(az) - trace_t[i] * math.sin(az)
            trace_e[i] = trace_r[i] * math.sin(az) + trace_t[i] * math.cos(az)

        base = Path(str(files["base"])).name
        outputs = {
            "bhz": (header_z, add_arrays(trace_z, scatter_grid["z"])),
            "bhn": (header_z.copy(), add_arrays(trace_n, scatter_grid["n"])),
            "bhe": (header_z.copy(), add_arrays(trace_e, scatter_grid["e"])),
        }
        begin = sac_float(header_z, 5)
        for ext, (header, trace) in outputs.items():
            trace, trimmed_begin = trim_trace_before(
                trace,
                sac_float(header, 0),
                begin,
                scatter_time_shift,
            )
            refresh_sac_header(header, trace, trimmed_begin)
            write_sac(out_3d / f"{base}.{ext}", header, trace)
            nout += 1

    for path in out_1d.glob("L*_dep*_dist*.bh?"):
        path.unlink()

    print(f"Copied 1-D DSM SAC files to {out_1d}")
    print(f"Wrote time-shifted scattering SAC files to {out_scatter}")
    print(f"Wrote {nout} final 3-D SEM-DSM SAC files to {out_3d}")
    print(f"Coupled SAC outputs trimmed at TIME_START_CUT={scatter_time_shift:.6f} s")


if __name__ == "__main__":
    main()
