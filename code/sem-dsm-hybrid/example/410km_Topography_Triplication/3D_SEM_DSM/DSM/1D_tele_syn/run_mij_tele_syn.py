#!/usr/bin/env python3
"""Prepare and optionally submit 1-D DSM Mij synthetics, then synthesize a CMTSOLUTION result.

Default inputs are relative to this script directory:
  stations: ../../DATA/tele_station.txt
  CMT:      ../../SPECFEM3D/CMTSOLUTION, falling back to ../../SPECFEM3D/DATA/CMTSOLUTION
  model:    ../../SPECFEM3D/dsm_model, falling back to ../../SPECFEM3D/DATA/dsm_model_input

The DSM jobs are unit moment-basis runs in DSM order:
  exponent, Mzz, Mrr, Mtt, Mzr, Mzt, Mrt

The synthesis action expects time-domain SAC files converted from each Mij run. It combines
matching SAC traces using CMTSOLUTION coefficients. By default it maps CMTSOLUTION directly
as Mzz=Mrr, Mrr=Mtt, Mtt=Mpp, Mzr=Mrt, Mzt=Mrp, Mrt=Mtp. Use --rotate-cmt-to-zrt if you
want station-dependent rotation into the local source-station ZRT basis.
"""

from __future__ import annotations

import argparse
import math
import shutil
import struct
import subprocess
from array import array
from dataclasses import dataclass
from pathlib import Path

COMPONENTS = ("Mzz", "Mrr", "Mtt", "Mzr", "Mzt", "Mrt")
SAC_EXTENSIONS = (".bhz", ".bhr", ".bht")
SAC_HEADER_BYTES = 632
OUTPUT_SUBDIRS = (
    "coef_cAnddcdr",
    "disp_fluid",
    "disp_solid",
    "stress",
    "potential",
    "pressure",
    "velo_solid",
)


@dataclass(frozen=True)
class Station:
    net: str
    sta: str
    lon: float
    lat: float
    extra: tuple[str, ...] = ()


@dataclass(frozen=True)
class Source:
    lat: float
    lon: float
    depth: float
    mrr: float
    mtt: float
    mpp: float
    mrt: float
    mrp: float
    mtp: float


def script_dir() -> Path:
    return Path(__file__).resolve().parent


def resolve_existing(primary: Path, fallback: Path | None = None) -> Path:
    if primary.exists():
        return primary
    if fallback is not None and fallback.exists():
        return fallback
    if fallback is None:
        raise FileNotFoundError(primary)
    raise FileNotFoundError(f"neither {primary} nor {fallback} exists")


def parse_cmtsolution(path: Path) -> Source:
    fields: dict[str, float] = {}
    for line in path.read_text().splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip().split()
        if value:
            try:
                fields[key] = float(value[0])
            except ValueError:
                pass
    required = ("latitude", "longitude", "depth", "Mrr", "Mtt", "Mpp", "Mrt", "Mrp", "Mtp")
    missing = [key for key in required if key not in fields]
    if missing:
        raise RuntimeError(f"missing CMTSOLUTION fields in {path}: {', '.join(missing)}")
    return Source(
        lat=fields["latitude"],
        lon=fields["longitude"],
        depth=fields["depth"],
        mrr=fields["Mrr"],
        mtt=fields["Mtt"],
        mpp=fields["Mpp"],
        mrt=fields["Mrt"],
        mrp=fields["Mrp"],
        mtp=fields["Mtp"],
    )


def parse_stations(path: Path) -> list[Station]:
    stations: list[Station] = []
    for line in path.read_text().splitlines():
        parts = line.split()
        if not parts or parts[0].startswith("#"):
            continue
        if len(parts) >= 4 and is_float(parts[2]) and is_float(parts[3]):
            stations.append(Station(parts[0], parts[1], float(parts[2]), float(parts[3]), tuple(parts[4:])))
        elif len(parts) >= 3 and is_float(parts[1]) and is_float(parts[2]):
            stations.append(Station("", parts[0], float(parts[1]), float(parts[2]), tuple(parts[3:])))
        else:
            raise RuntimeError(f"cannot parse station line in {path}: {line}")
    if not stations:
        raise RuntimeError(f"no stations found in {path}")
    return stations


def is_float(text: str) -> bool:
    try:
        float(text)
        return True
    except ValueError:
        return False


def angular_distance_deg(src: Source, station: Station) -> float:
    lat1 = math.radians(src.lat)
    lat2 = math.radians(station.lat)
    dlon = math.radians(station.lon - src.lon)
    a = math.sin((lat2 - lat1) / 2.0) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2.0) ** 2
    a = min(1.0, max(0.0, a))
    return math.degrees(2.0 * math.atan2(math.sqrt(a), math.sqrt(1.0 - a)))


def azimuth_deg(src: Source, station: Station) -> float:
    lat1 = math.radians(src.lat)
    lat2 = math.radians(station.lat)
    dlon = math.radians(station.lon - src.lon)
    y = math.sin(dlon) * math.cos(lat2)
    x = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(dlon)
    az = math.degrees(math.atan2(y, x))
    return az + 360.0 if az < 0.0 else az


def structure_end_line(lines: list[str]) -> int:
    if len(lines) < 4:
        raise RuntimeError("DSM model is too short")
    nzone = int(lines[3].split()[0])
    return 4 + 6 * nzone


def patch_dsm_model(lines: list[str], src: Source, moment_vector: str, args: argparse.Namespace) -> list[str]:
    patched = list(lines)
    end = structure_end_line(lines)
    needed = end + 7
    if len(patched) < needed:
        raise RuntimeError(f"DSM model has {len(patched)} lines, expected at least {needed}")

    # Keep the structural model and global parameters unchanged. Replace only source geometry,
    # the Mij vector, and local list-file paths needed by each component run.
    patched[end] = (
        f"  {src.depth:g} {src.lat:g} {src.lon:g} {args.moment_or_force:d} "
        "#source_depth(km) source_lat source_lon source_type"
    )
    patched[end + 1] = f"  {moment_vector} #unit Mij Green function: exp_dyn-cm Mzz Mrr Mtt Mzr Mzt Mrt"
    patched[end + 2] = "  depth_solid_list #file listing the Green's function depths in solid media"
    patched[end + 3] = "  dist_solid_list #file listing the Green's function distances in solid media"
    patched[end + 4] = "  depth_fluid_list #file listing the Green's function depths in fluid media"
    patched[end + 5] = "  dist_fluid_list #file listing the Green's function distances in fluid media"
    patched[end + 6] = (
        "  1 #save displacement (choose 1, for coupling integral) or velocity "
        "(choose 2, for Stacey ABC) seismograms"
    )
    return patched


def write_depth_lists(work: Path, args: argparse.Namespace) -> None:
    ndep = args.ndep_top_layer1
    if ndep < 1:
        raise ValueError("ndep_top_layer1 must be >= 1")
    depths: list[float]
    if ndep == 1:
        depths = [args.top_dep_layer1]
    else:
        step = (args.bot_dep_layer1 - args.top_dep_layer1) / (ndep - 1)
        depths = [args.top_dep_layer1 + i * step for i in range(ndep)]
        depths[-1] = args.bot_dep_layer1
    with (work / "depth_solid_list").open("w") as f:
        f.write(f"{args.ddepth_for_stress:g} {args.depth_tolerance:g}\n")
        f.write(f"{len(depths)}\n")
        for depth in depths:
            f.write(f"{depth:10.4f} {args.id_zone_layer1:d}\n")
    (work / "depth_fluid_list").write_text(f"{args.ddepth_for_stress:g} {args.depth_tolerance:g}\n0\n")


def write_distance_lists(work: Path, src: Source, stations: list[Station]) -> None:
    rows = sorted(((angular_distance_deg(src, sta), sta) for sta in stations), key=lambda item: (item[0], item[1].net, item[1].sta))
    unique: list[float] = []
    seen: set[str] = set()
    with (work / "station_distances.txt").open("w") as sf:
        for dist, sta in rows:
            key = f"{dist:.6f}"
            if key not in seen:
                seen.add(key)
                unique.append(float(key))
            sf.write(f"{sta.net:<8s} {sta.sta:<8s} {sta.lon:14.6f} {sta.lat:14.6f} {dist:16.8f}\n")
    text = f"{len(unique)}\n" + "".join(f"{dist:.6f}\n" for dist in unique)
    (work / "dist_solid_list").write_text(text)
    (work / "dist_fluid_list").write_text(text)


def component_vectors(unit_exp: int) -> dict[str, str]:
    values = {}
    for i, comp in enumerate(COMPONENTS):
        nums = ["0.0"] * 6
        nums[i] = "1.0"
        values[comp] = f"{unit_exp:d} " + " ".join(nums)
    return values


def prepare(args: argparse.Namespace) -> None:
    root = script_dir()
    station_file = resolve_existing(root / args.station_file)
    cmt_file = resolve_existing(root / args.cmt_file, root / args.cmt_fallback)
    model_file = resolve_existing(root / args.model_file, root / args.model_fallback)
    dsmti = resolve_existing(root / args.dsm_solver_dir / "dsmti")
    src = parse_cmtsolution(cmt_file)
    stations = parse_stations(station_file)
    model_lines = model_file.read_text().splitlines()
    work = root / args.work_dir
    work.mkdir(parents=True, exist_ok=True)

    write_depth_lists(work, args)
    write_distance_lists(work, src, stations)

    submit_ids: list[str] = []
    for comp, vector in component_vectors(args.unit_moment_exp).items():
        comp_dir = work / comp
        comp_dir.mkdir(parents=True, exist_ok=True)
        (comp_dir / "DATA").mkdir(exist_ok=True)
        shutil.copy2(dsmti, comp_dir / "dsmti")
        (comp_dir / "dsmti").chmod(0o755)
        for name in OUTPUT_SUBDIRS:
            (comp_dir / "OUTPUT_FILES" / name).mkdir(parents=True, exist_ok=True)
        for name in ("depth_solid_list", "depth_fluid_list", "dist_solid_list", "dist_fluid_list", "station_distances.txt"):
            shutil.copy2(work / name, comp_dir / name)
            shutil.copy2(work / name, comp_dir / "DATA" / name)
        dsm_model = "\n".join(patch_dsm_model(model_lines, src, vector, args)) + "\n"
        (comp_dir / "dsm_model").write_text(dsm_model)
        (comp_dir / "DATA" / "dsm_model").write_text(dsm_model)
        submit_path = comp_dir / f"submit_DSM_{comp}.cmd"
        submit_path.write_text(slurm_script(comp, args))
        if args.submit:
            result = subprocess.run(["sbatch", "--parsable", submit_path.name], cwd=comp_dir, check=True, text=True, stdout=subprocess.PIPE)
            submit_ids.append(result.stdout.strip())
            print(f"submitted {comp}: {submit_ids[-1]}")
        else:
            print(f"prepared {comp}: {comp_dir}")

    write_coefficients(work / "cmtsolution_coefficients.txt", src, args.unit_moment_exp)
    if args.submit and args.write_synthesis_job:
        write_synthesis_submit(work, submit_ids, args)

    print(f"source: depth={src.depth:g} km lat={src.lat:g} lon={src.lon:g}")
    print(f"stations: {len(stations)} from {station_file}")
    print(f"model: {model_file}")
    print(f"basis moment: 1e{args.unit_moment_exp:d} dyn*cm")


def slurm_script(comp: str, args: argparse.Namespace) -> str:
    module_line = f"module load {args.modules}\n" if args.modules else ""
    return f"""#!/bin/bash
#SBATCH --time={args.time_estimate}
#SBATCH --ntasks={args.nproc}
#SBATCH --mem-per-cpu={args.mem_per_cpu}
#SBATCH -J \"DSM_{comp}\"
{args.partition_directive}{args.qos_directive}{module_line}{args.mpirun} ./dsmti < DATA/dsm_model
"""


def write_synthesis_submit(work: Path, dependency_ids: list[str], args: argparse.Namespace) -> None:
    dep = ":".join(dependency_ids)
    dep_line = f"#SBATCH --dependency=afterok:{dep}\n" if dep else ""
    script = f"""#!/bin/bash
#SBATCH --time=01:00:00
#SBATCH --ntasks=1
#SBATCH --mem-per-cpu=4G
#SBATCH -J \"DSM_sum\"
{dep_line}{args.partition_directive}{args.qos_directive}cd {script_dir()}
python3 {Path(__file__).name} synthesize --work-dir {args.work_dir} --cmt-file {args.cmt_file} --cmt-fallback {args.cmt_fallback} --input-subdir {args.input_subdir} --output-dir {args.output_dir}
"""
    path = work / "submit_synthesize.cmd"
    path.write_text(script)
    result = subprocess.run(["sbatch", "--parsable", path.name], cwd=work, check=True, text=True, stdout=subprocess.PIPE)
    print(f"submitted synthesis: {result.stdout.strip()}")


def direct_coefficients(src: Source) -> dict[str, float]:
    return {
        "Mzz": src.mrr,
        "Mrr": src.mtt,
        "Mtt": src.mpp,
        "Mzr": src.mrt,
        "Mzt": src.mrp,
        "Mrt": src.mtp,
    }


def rotated_coefficients(src: Source, station: Station) -> dict[str, float]:
    az = math.radians(azimuth_deg(src, station))
    caz = math.cos(az)
    saz = math.sin(az)
    old = (
        (src.mrr, src.mrt, src.mrp),
        (src.mrt, src.mtt, src.mtp),
        (src.mrp, src.mtp, src.mpp),
    )
    basis = (
        (1.0, 0.0, 0.0),
        (0.0, -caz, saz),
        (0.0, -saz, -caz),
    )

    def mt(i: int, j: int) -> float:
        return sum(basis[i][a] * old[a][b] * basis[j][b] for a in range(3) for b in range(3))

    return {"Mzz": mt(0, 0), "Mrr": mt(1, 1), "Mtt": mt(2, 2), "Mzr": mt(0, 1), "Mzt": mt(0, 2), "Mrt": mt(1, 2)}


def write_coefficients(path: Path, src: Source, unit_exp: int) -> None:
    basis = 10.0 ** unit_exp
    coefs = direct_coefficients(src)
    lines = ["# component coefficient_for_unit_1e%d_dyn_cm value_dyn_cm" % unit_exp]
    for comp in COMPONENTS:
        lines.append(f"{comp} {coefs[comp] / basis:.12e} {coefs[comp]:.12e}")
    path.write_text("\n".join(lines) + "\n")


def station_base(zone: int, depth: float, dist: float) -> str:
    return f"L{zone:d}_dep{depth:.2f}_dist{dist:.2f}"


def output_base(station: Station, depth: float) -> str:
    prefix = f"{station.net}_" if station.net else ""
    return f"{prefix}{station.sta}_dep{depth:.2f}"


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


def read_station_distances(path: Path) -> list[tuple[Station, float]]:
    rows: list[tuple[Station, float]] = []
    for line in path.read_text().splitlines():
        parts = line.split()
        if len(parts) < 5 or parts[0].startswith("#"):
            continue
        rows.append((Station(parts[0], parts[1], float(parts[2]), float(parts[3])), float(parts[4])))
    if not rows:
        raise RuntimeError(f"no station distances found in {path}")
    return rows


def read_sac(path: Path) -> tuple[bytes, array]:
    raw = path.read_bytes()
    if len(raw) < SAC_HEADER_BYTES or (len(raw) - SAC_HEADER_BYTES) % 4:
        raise RuntimeError(f"not a SAC binary file: {path}")
    trace = array("f")
    trace.frombytes(raw[SAC_HEADER_BYTES:])
    return raw[:SAC_HEADER_BYTES], trace


def update_sac_header(header: bytes, trace: array) -> bytes:
    if not trace:
        return header
    patched = bytearray(header)
    values = list(trace)
    struct.pack_into("f", patched, 4, min(values))
    struct.pack_into("f", patched, 8, max(values))
    struct.pack_into("f", patched, 224, sum(values) / len(values))
    return bytes(patched)


def synthesize(args: argparse.Namespace) -> None:
    root = script_dir()
    work = root / args.work_dir
    cmt_file = resolve_existing(root / args.cmt_file, root / args.cmt_fallback)
    src = parse_cmtsolution(cmt_file)
    basis = 10.0 ** args.unit_moment_exp
    depths = read_depths(work / "Mzz" / "DATA" / "depth_solid_list")
    stations = read_station_distances(work / "Mzz" / "DATA" / "station_distances.txt")
    output_dir = work / args.output_dir
    if args.clean and output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    nout = 0
    for depth, zone in depths:
        for station, dist in stations:
            coefs = rotated_coefficients(src, station) if args.rotate_cmt_to_zrt else direct_coefficients(src)
            coefs = {key: value / basis for key, value in coefs.items()}
            base = station_base(zone, depth, dist)
            outbase = output_base(station, depth)
            for ext in SAC_EXTENSIONS:
                header: bytes | None = None
                out: array | None = None
                for comp in COMPONENTS:
                    trace_path = work / comp / "OUTPUT_FILES" / args.input_subdir / f"{base}{ext}"
                    if not trace_path.exists():
                        raise RuntimeError(f"missing component SAC file: {trace_path}")
                    this_header, trace = read_sac(trace_path)
                    if header is None:
                        header = this_header
                        out = array("f", [0.0]) * len(trace)
                    if out is None or len(out) != len(trace):
                        raise RuntimeError(f"inconsistent SAC trace length: {trace_path}")
                    coef = coefs[comp]
                    for i, value in enumerate(trace):
                        out[i] += coef * value
                assert header is not None and out is not None
                (output_dir / f"{outbase}{ext}").write_bytes(update_sac_header(header, out) + out.tobytes())
                nout += 1
    mode = "station-rotated ZRT" if args.rotate_cmt_to_zrt else "direct CMTSOLUTION-to-DSM mapping"
    print(f"wrote {nout} SAC files to {output_dir} using {mode}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command")

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--station-file", default="../../DATA/tele_station.txt")
    common.add_argument("--cmt-file", default="../../SPECFEM3D/CMTSOLUTION")
    common.add_argument("--cmt-fallback", default="../../SPECFEM3D/DATA/CMTSOLUTION")
    common.add_argument("--model-file", default="../../SPECFEM3D/dsm_model")
    common.add_argument("--model-fallback", default="../../SPECFEM3D/DATA/dsm_model_input")
    common.add_argument("--work-dir", default="410km_triplication_mij_python")
    common.add_argument("--unit-moment-exp", type=int, default=7)

    prep = sub.add_parser("prepare", parents=[common], help="prepare per-component DSM directories")
    prep.add_argument("--dsm-solver-dir", default="../../../../../src/DSM/src/DSM_Solver", type=Path)
    prep.add_argument("--submit", action="store_true", help="submit the six DSM jobs with sbatch")
    prep.add_argument("--write-synthesis-job", action="store_true", help="submit a dependent synthesis job after DSM jobs")
    prep.add_argument("--nproc", type=int, default=256)
    prep.add_argument("--time-estimate", default="23:59:00")
    prep.add_argument("--mem-per-cpu", default="4G")
    prep.add_argument("--modules", default="")
    prep.add_argument("--mpirun", default="mpirun")
    prep.add_argument("--partition", default="")
    prep.add_argument("--qos", default="")
    prep.add_argument("--moment-or-force", type=int, default=1)
    prep.add_argument("--depth-tolerance", type=float, default=0.0025)
    prep.add_argument("--ddepth-for-stress", type=float, default=0.01)
    prep.add_argument("--ndep-top-layer1", type=int, default=2)
    prep.add_argument("--id-zone-layer1", type=int, default=10)
    prep.add_argument("--top-dep-layer1", type=float, default=0.0)
    prep.add_argument("--bot-dep-layer1", type=float, default=1.0)
    prep.add_argument("--input-subdir", default="disp_solid_time_sac")
    prep.add_argument("--output-dir", default="CMTSOLUTION_time_sac")
    prep.set_defaults(func=prepare)

    syn = sub.add_parser("synthesize", parents=[common], help="combine converted Mij SAC traces")
    syn.add_argument("--input-subdir", default="disp_solid_time_sac")
    syn.add_argument("--output-dir", default="CMTSOLUTION_time_sac")
    syn.add_argument("--rotate-cmt-to-zrt", action="store_true")
    syn.add_argument("--clean", action="store_true")
    syn.set_defaults(func=synthesize)
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    if args.command is None:
        args = parser.parse_args(["prepare"])
    args.partition_directive = f"#SBATCH --partition={args.partition}\n" if getattr(args, "partition", "") else ""
    args.qos_directive = f"#SBATCH --qos={args.qos}\n" if getattr(args, "qos", "") else ""
    args.func(args)


if __name__ == "__main__":
    main()
