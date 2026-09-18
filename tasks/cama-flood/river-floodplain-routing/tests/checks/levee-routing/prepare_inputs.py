#!/usr/bin/env python3
"""Generate a small, redistributable Mozambique deck from analytic fields."""
from __future__ import annotations

import argparse
import json
import math
from datetime import datetime, timedelta, timezone
from pathlib import Path

import netCDF4
import numpy as np


def replace_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f"expected one occurrence of {old!r}, found {text.count(old)}")
    return text.replace(old, new)


def generate_forcing(out: Path, runoff: float, days: int) -> None:
    out.mkdir(parents=True, exist_ok=True)
    runoff_file = out / "runoff.nc"
    with netCDF4.Dataset(runoff_file, "w", format="NETCDF4_CLASSIC") as ds:
        ds.createDimension("time", days + 1)
        ds.createDimension("longitude", 17)
        ds.createDimension("latitude", 21)
        t = ds.createVariable("time", "f8", ("time",))
        t.units = "days since 2019-01-01 00:00:00"
        t.calendar = "proleptic_gregorian"
        t[:] = np.arange(days + 1, dtype=np.float64)
        lon = ds.createVariable("longitude", "f4", ("longitude",))
        lat = ds.createVariable("latitude", "f4", ("latitude",))
        lon[:] = np.arange(31.75, 36.0, 0.25, dtype=np.float32)
        lat[:] = np.arange(-16.75, -22.0, -0.25, dtype=np.float32)
        ro = ds.createVariable("ro", "f4", ("time", "latitude", "longitude"))
        y, x = np.mgrid[0:21, 0:17]
        spatial = 1.0 + 0.12 * np.sin((x + 1) / 3.0) * np.cos((y + 2) / 4.0)
        for i in range(days + 1):
            ro[i, :, :] = np.float32(runoff) * spatial * (1.0 + 0.08 * math.sin(i * 0.9))

    sea_file = out / "sea_level.nc"
    ntime = days * 144 + 1
    epoch = datetime(1900, 1, 1, tzinfo=timezone.utc)
    start = datetime(2019, 1, 1, tzinfo=timezone.utc)
    offset = (start - epoch).total_seconds()
    with netCDF4.Dataset(sea_file, "w", format="NETCDF4_CLASSIC") as ds:
        ds.createDimension("time", ntime)
        ds.createDimension("stations", 15)
        t = ds.createVariable("time", "f8", ("time",))
        t.units = "seconds since 1900-01-01 00:00:00"
        t.calendar = "proleptic_gregorian"
        t[:] = offset + np.arange(ntime, dtype=np.float64) * 600.0
        wl = ds.createVariable("waterlevel", "f4", ("time", "stations"))
        phase = np.arange(15, dtype=np.float64)[None, :] * 0.17
        clock = np.arange(ntime, dtype=np.float64)[:, None] * (2.0 * np.pi / 74.52)
        wl[:, :] = (0.35 + 0.22 * np.sin(clock + phase) + 0.04 * np.sin(clock / 13.0)).astype(np.float32)


def write_levee_maps(map_dir: Path, height: float, fraction: float) -> None:
    shape = (45, 35)
    np.full(shape, np.float32(height), dtype=np.float32).tofile(map_dir / "task_levhgt.bin")
    np.full(shape, np.float32(fraction), dtype=np.float32).tofile(map_dir / "task_levfrc.bin")


def make_deck(source: Path, work: Path, days: int, cpus: int, levee: bool) -> Path:
    template = (source / "etc/sealev_boundary/test5-moz_06min_sealev.sh").read_text()
    text = replace_once(template, 'BASE="/home/yamadai/work/CaMa_v401/cmf_v401_test"', f'BASE="{source}"')
    text = replace_once(text, 'INPDIR="${BASE}/etc/sealev_boundary/test_moz_06min_sealev"', f'INPDIR="{work / "inputs"}"')
    text = replace_once(text, "export OMP_NUM_THREADS=16", f"export OMP_NUM_THREADS={cpus}")
    text = replace_once(text, "SPINUP=1", "SPINUP=0")
    text = replace_once(text, "EMON=5", "EMON=1")
    text = replace_once(text, "EDAY=1", f"EDAY={days + 1}")
    text = replace_once(text, 'LRESTART=".TRUE."', 'LRESTART=".FALSE."')
    text = replace_once(text, 'CRESTSTO="${INPDIR}/restart2019010100.nc"', 'CRESTSTO=""')
    text = replace_once(text, 'CROFCDF="${INPDIR}/era5_ro_20190101-20190501_daily_moz.nc"', 'CROFCDF="${INPDIR}/runoff.nc"')
    text = replace_once(text, 'CSEALEVCDF="${INPDIR}/gtsm_era5_ro_20190101-20190501_moz.nc"', 'CSEALEVCDF="${INPDIR}/sea_level.nc"')
    text = replace_once(text, 'CVARSOUT="outflw,storge,maxdph,maxflw,flddph,fldsto,fldare,pthout"', 'CVARSOUT="outflw,storge,flddph,fldsto,fldare,pthout,levsto,levdph"')
    if levee:
        text = replace_once(text, 'LDAMOUT=".FALSE."', 'LDAMOUT=".FALSE."\nLLEVEE=".TRUE."')
        text = replace_once(text, 'LDAMOUT  = ${LDAMOUT}', 'LDAMOUT  = ${LDAMOUT}\nLLEVEE   = ${LLEVEE}                  ! true: activate levee routing')
        marker = "#================================================\n# (5) Execute main program"
        block = '''cat >> ${NMLIST} << EOF
&NLEVEE
CLEVFRC = "${FMAP}/task_levfrc.bin"
CLEVHGT = "${FMAP}/task_levhgt.bin"
/
EOF

'''
        text = replace_once(text, marker, block + marker)
    deck = work / "deck.sh"
    deck.write_text(text)
    deck.chmod(0o755)
    return deck


def extract_outputs(run_dir: Path, out_dir: Path, names: list[str]) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    for name in names:
        path = run_dir / f"o_{name}2019.nc"
        if not path.is_file():
            raise FileNotFoundError(path)
        with netCDF4.Dataset(path) as ds:
            values = np.ma.filled(ds.variables[name][-1], 1.0e20).astype("<f8", copy=False).ravel()
        values[np.abs(values) >= 1.0e19] = 1.0e20
        values.tofile(out_dir / f"{name}.f64")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("action", choices=("prepare", "extract"))
    ap.add_argument("--source", type=Path, required=True)
    ap.add_argument("--work", type=Path, required=True)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--params", type=Path)
    ap.add_argument("--days", type=int, default=5)
    ap.add_argument("--cpus", type=int, default=1)
    ap.add_argument("--levee", action="store_true")
    args = ap.parse_args()
    if args.action == "prepare":
        params = json.loads(args.params.read_text())
        generate_forcing(args.work / "inputs", float(params["runoff_m_per_day"]), args.days)
        map_dir = args.source / "etc/sealev_boundary/moz_06min"
        if args.levee:
            write_levee_maps(map_dir, float(params["levee_height_m"]), float(params["levee_fraction"]))
        make_deck(args.source, args.work, args.days, args.cpus, args.levee)
    else:
        names = ["outflw", "flddph", "fldsto", "levsto", "levdph"] if args.levee else ["outflw", "flddph", "fldsto", "pthout"]
        extract_outputs(args.source / "out/test5-moz_06min_sealev", args.out, names)


if __name__ == "__main__":
    main()
