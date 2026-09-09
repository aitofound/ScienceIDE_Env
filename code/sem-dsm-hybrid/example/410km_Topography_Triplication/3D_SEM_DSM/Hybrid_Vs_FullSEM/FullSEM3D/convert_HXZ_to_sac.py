#!/user/wenbo.wu/.conda/envs/sqhao_seismo/bin/python
"""Convert SPECFEM ASCII HXX/HXY/HXZ semv files to SAC."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from obspy import Trace

DEFAULT_COMPONENTS = ("HXX", "HXY", "HXZ")


def parse_name(path: Path) -> tuple[str, str, str]:
    # Expected: STATION.NETWORK.CHANNEL.semv, e.g. 13.10deg.TR.HXZ.semv
    parts = path.name.split(".")
    if len(parts) < 4 or parts[-1] != "semv":
        raise ValueError(f"unexpected semv filename: {path.name}")
    channel = parts[-2]
    network = parts[-3]
    station = ".".join(parts[:-3])
    return station, network, channel


def convert(path: Path, outdir: Path) -> Path:
    station, network, channel = parse_name(path)
    arr = np.loadtxt(path)
    if arr.ndim != 2 or arr.shape[1] < 2:
        raise ValueError(f"expected two-column time/data file: {path}")

    time = arr[:, 0]
    data = arr[:, 1].astype(np.float32)
    if len(time) < 2:
        raise ValueError(f"not enough samples: {path}")

    dt = float(np.median(np.diff(time)))
    b = float(time[0])

    tr = Trace(data=data)
    tr.stats.delta = dt
    tr.stats.network = network
    tr.stats.station = station
    tr.stats.channel = channel
    tr.stats.sac = {
        "b": b,
        "e": b + dt * (len(data) - 1),
        "o": 0.0,
        "knetwk": network,
        "kstnm": station,
        "kcmpnm": channel,
    }

    outdir.mkdir(parents=True, exist_ok=True)
    outpath = outdir / f"{station}.{network}.{channel}.sac"
    tr.write(str(outpath), format="SAC")
    return outpath


def default_files() -> list[Path]:
    files: list[Path] = []
    for component in DEFAULT_COMPONENTS:
        files.extend(Path(".").glob(f"*.TR.{component}.semv"))
    return sorted(files)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--outdir", type=Path, default=Path("sac"))
    args = parser.parse_args()

    files = args.files if args.files else default_files()
    if not files:
        components = ", ".join(DEFAULT_COMPONENTS)
        raise SystemExit(f"no {components} semv files found")

    for path in files:
        station, network, channel = parse_name(path)
        if not args.files and channel not in DEFAULT_COMPONENTS:
            continue
        outpath = convert(path, args.outdir)
        print(f"wrote {outpath}")


if __name__ == "__main__":
    main()
