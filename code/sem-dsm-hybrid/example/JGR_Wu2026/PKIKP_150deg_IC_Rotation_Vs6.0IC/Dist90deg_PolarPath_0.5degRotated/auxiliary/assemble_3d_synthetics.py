#!/usr/bin/env python3
"""Add 1-D DSM SAC synthetics and coupling/scattering SAC synthetics."""

from __future__ import annotations

import argparse
import math
import re
import shutil
import struct
from array import array
from pathlib import Path


SAC_HEADER_BYTES = 632
SAC_FLOAT_COUNT = 70
DSM_DISTANCE_RE = re.compile(r"_dist([0-9]+(?:\.[0-9]+)?)\.bhz$")


def read_key_values(path: Path, separator: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or separator not in line:
            continue
        key, value = line.split(separator, 1)
        values[key.strip()] = value.strip().split()[0]
    return values



def read_float_key(path: Path, key: str) -> float:
    values = read_key_values(path, "=")
    if key not in values:
        raise RuntimeError(f"missing {key} in {path}")
    return float(values[key])

def read_cmt(path: Path) -> dict[str, float]:
    raw = read_key_values(path, ":")
    required = ("latitude", "longitude")
    missing = [key for key in required if key not in raw]
    if missing:
        raise RuntimeError(f"missing CMTSOLUTION fields in {path}: {', '.join(missing)}")
    return {key: float(raw[key]) for key in required}


def read_coupling_stations(path: Path) -> dict[str, dict[str, float]]:
    stations: dict[str, dict[str, float]] = {}
    for line in path.read_text().splitlines():
        parts = line.split()
        if len(parts) < 3 or parts[0].startswith("#"):
            continue
        stations[parts[0]] = {"lon": float(parts[1]), "lat": float(parts[2])}
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
    return az + 360.0 if az < 0.0 else az


def sac_float(header: bytes | bytearray, index: int) -> float:
    return struct.unpack_from("f", header, 4 * index)[0]


def sac_int(header: bytes | bytearray, index: int) -> int:
    return struct.unpack_from("i", header, 4 * SAC_FLOAT_COUNT + 4 * index)[0]


def set_sac_float(header: bytearray, index: int, value: float) -> None:
    struct.pack_into("f", header, 4 * index, float(value))


def set_sac_int(header: bytearray, index: int, value: int) -> None:
    struct.pack_into("i", header, 4 * SAC_FLOAT_COUNT + 4 * index, int(value))


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


def refresh_sac_header(header: bytearray, trace: array, begin: float) -> bytearray:
    delta = sac_float(header, 0)
    set_sac_float(header, 5, begin)
    set_sac_float(header, 6, begin + (len(trace) - 1) * delta if trace else begin)
    set_sac_int(header, 9, len(trace))
    if trace:
        vals = list(trace)
        set_sac_float(header, 1, min(vals))
        set_sac_float(header, 2, max(vals))
        set_sac_float(header, 56, sum(vals) / len(vals))
    return header


def write_sac(path: Path, header: bytearray, trace: array) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(header) + trace.tobytes())


def interpolate(trace: array, delta: float, start: float, time: float) -> float:
    x = (time - start) / delta
    i = math.floor(x)
    if i < 0 or i >= len(trace):
        return 0.0
    if i == len(trace) - 1:
        return float(trace[i]) if abs(x - i) < 1.0e-6 else 0.0
    frac = x - i
    return float(trace[i]) * (1.0 - frac) + float(trace[i + 1]) * frac


def trace_on_reference_grid(ref_header: bytearray, trace: array, delta: float, begin: float) -> array:
    npts = sac_int(ref_header, 9)
    ref_delta = sac_float(ref_header, 0)
    ref_begin = sac_float(ref_header, 5)
    out = array("f", [0.0]) * npts
    for i in range(npts):
        out[i] = interpolate(trace, delta, begin, ref_begin + i * ref_delta)
    return out


def add_arrays(a: array, b: array) -> array:
    if len(a) != len(b):
        raise RuntimeError(f"cannot add arrays with lengths {len(a)} and {len(b)}")
    out = array("f", [0.0]) * len(a)
    for i, value in enumerate(a):
        out[i] = value + b[i]
    return out


def choose_1d_prefix(one_d_dir: Path, distance: float, depth_index: int) -> Path:
    matches = sorted(one_d_dir.glob(f"L*_dep*_dist{distance:.2f}.bhz"))
    if not matches:
        raise RuntimeError(f"missing 1-D SAC file matching distance {distance:.2f} in {one_d_dir}")
    if depth_index >= len(matches):
        raise RuntimeError(
            f"depth index {depth_index} out of range for distance {distance:.2f}; "
            f"found {len(matches)} depth(s)"
        )
    return matches[depth_index].with_suffix("")


def choose_1d_prefix_weights(
    one_d_dir: Path, distance: float, depth_index: int
) -> list[tuple[Path, float]]:
    exact = sorted(one_d_dir.glob(f"L*_dep*_dist{distance:.2f}.bhz"))
    if exact:
        if depth_index >= len(exact):
            raise RuntimeError(
                f"depth index {depth_index} out of range for distance {distance:.2f}; "
                f"found {len(exact)} depth(s)"
            )
        return [(exact[depth_index].with_suffix(""), 1.0)]

    distances: list[float] = []
    for path in one_d_dir.glob("L*_dep*_dist*.bhz"):
        match = DSM_DISTANCE_RE.search(path.name)
        if match:
            distances.append(float(match.group(1)))
    distances = sorted(set(distances))
    lower = max((value for value in distances if value < distance), default=None)
    upper = min((value for value in distances if value > distance), default=None)
    if lower is None or upper is None:
        raise RuntimeError(f"missing 1-D SAC file matching distance {distance:.2f} in {one_d_dir}")

    frac = (distance - lower) / (upper - lower)
    return [
        (choose_1d_prefix(one_d_dir, lower, depth_index), 1.0 - frac),
        (choose_1d_prefix(one_d_dir, upper, depth_index), frac),
    ]


def component_path(prefix: Path, ext: str) -> Path:
    return Path(f"{prefix}.{ext}")


def weighted_trace_on_header(
    ref_header: bytearray, weighted_prefixes: list[tuple[Path, float]], ext: str
) -> array:
    npts = sac_int(ref_header, 9)
    out = array("f", [0.0]) * npts
    for prefix, weight in weighted_prefixes:
        header, trace = read_sac(component_path(prefix, ext))
        trace = trace_on_reference_grid(
            ref_header,
            trace,
            sac_float(header, 0),
            sac_float(header, 5),
        )
        for i, value in enumerate(trace):
            out[i] += weight * value
    return out


def parse_distance(station_name: str) -> float:
    if not station_name.startswith("dist"):
        raise RuntimeError(f"cannot parse distance from station name {station_name}")
    return float(station_name[4:])



def infer_scatter_time_shift(root: Path, metadata_arg: str) -> float:
    path = (root / metadata_arg).resolve()
    if not path.is_file():
        print(
            f"Warning: injected-wave metadata not found: {path}; "
            "using scatter time shift 0.0 s"
        )
        return 0.0
    return read_float_key(path, "t_begin_Green")

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--one-d-dir", default="../DSM/1D_tele_syn_Vs6.0IC/PKIKP/OUTPUT_FILES/disp_solid_sac")
    parser.add_argument("--coupling-dir", default="Coupling/OUTPUT_FILES")
    parser.add_argument("--stations", default="Coupling/DATA/STATION")
    parser.add_argument("--cmt", default="SPECFEM3D/DATA/CMTSOLUTION")
    parser.add_argument("--output-dir", default="3D_synthetics")
    parser.add_argument("--depth-index", type=int, default=0)
    parser.add_argument(
        "--injected-metadata",
        default="InjectedWaves/OUTPUT_FILES/InjectedWaves_Par_file",
        help="Injected-wave metadata file containing t_begin_Green.",
    )
    parser.add_argument(
        "--scatter-time-shift",
        type=float,
        default=None,
        help="Override the scattering-wave time shift in seconds.",
    )
    parser.add_argument("--keep-existing", action="store_true")
    args = parser.parse_args()

    root = Path.cwd()
    one_d_dir = (root / args.one_d_dir).resolve()
    coupling_dir = (root / args.coupling_dir).resolve()
    output_dir = (root / args.output_dir).resolve()

    if not one_d_dir.is_dir():
        raise RuntimeError(f"missing 1-D DSM SAC directory: {one_d_dir}")
    if not coupling_dir.is_dir():
        raise RuntimeError(f"missing coupling SAC directory: {coupling_dir}")

    stations = read_coupling_stations((root / args.stations).resolve())
    cmt = read_cmt((root / args.cmt).resolve())
    scatter_time_shift = (
        args.scatter_time_shift
        if args.scatter_time_shift is not None
        else infer_scatter_time_shift(root, args.injected_metadata)
    )

    if output_dir.exists() and not args.keep_existing:
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    nout = 0
    for scatter_z in sorted(coupling_dir.glob("*.bhz")):
        name = scatter_z.name[:-4]
        if name not in stations:
            raise RuntimeError(f"missing station metadata for {name} in {args.stations}")
        distance = parse_distance(name)
        weighted_prefixes = choose_1d_prefix_weights(one_d_dir, distance, args.depth_index)
        header_z, _ = read_sac(component_path(weighted_prefixes[0][0], "bhz"))

        trace_z = weighted_trace_on_header(header_z, weighted_prefixes, "bhz")
        trace_r = weighted_trace_on_header(header_z, weighted_prefixes, "bhr")
        trace_t = weighted_trace_on_header(header_z, weighted_prefixes, "bht")

        station = stations[name]
        az = math.radians(
            azimuth_deg(cmt["latitude"], cmt["longitude"], station["lat"], station["lon"])
        )
        trace_n = array("f", [0.0]) * len(trace_r)
        trace_e = array("f", [0.0]) * len(trace_r)
        for i in range(len(trace_r)):
            trace_n[i] = trace_r[i] * math.cos(az) - trace_t[i] * math.sin(az)
            trace_e[i] = trace_r[i] * math.sin(az) + trace_t[i] * math.cos(az)

        one_d_traces = {"bhz": trace_z, "bhn": trace_n, "bhe": trace_e}
        for ext, one_d_trace in one_d_traces.items():
            scatter_path = coupling_dir / f"{name}.{ext}"
            if not scatter_path.exists():
                raise RuntimeError(f"missing coupling SAC file: {scatter_path}")
            header_s, trace_s = read_sac(scatter_path)
            scatter = trace_on_reference_grid(
                header_z,
                trace_s,
                sac_float(header_s, 0),
                sac_float(header_s, 5) + scatter_time_shift,
            )
            out_trace = add_arrays(one_d_trace, scatter)
            out_header = refresh_sac_header(header_z.copy(), out_trace, sac_float(header_z, 5))
            write_sac(output_dir / f"{name}.{ext}", out_header, out_trace)
            nout += 1

    print(f"Scattering-wave time shift: {scatter_time_shift:.10g} s")
    print(f"Wrote {nout} SAC files to {output_dir}")


if __name__ == "__main__":
    main()
