#!/usr/bin/env python3
"""Generate a small, redistributable Mozambique deck from analytic fields."""
from __future__ import annotations

import argparse
import json
import math
from datetime import datetime, timezone
from pathlib import Path

import netCDF4
import numpy as np


def replace_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f"expected one occurrence of {old!r}, found {text.count(old)}")
    return text.replace(old, new)


def generate_forcing(out: Path, runoff: float, days: int) -> None:
    out.mkdir(parents=True, exist_ok=True)
    with netCDF4.Dataset(out / "runoff.nc", "w", format="NETCDF4_CLASSIC") as ds:
        ds.createDimension("time", days + 1); ds.createDimension("longitude", 17); ds.createDimension("latitude", 21)
        t = ds.createVariable("time", "f8", ("time",)); t.units = "days since 2019-01-01 00:00:00"; t.calendar = "proleptic_gregorian"; t[:] = np.arange(days + 1)
        ds.createVariable("longitude", "f4", ("longitude",))[:] = np.arange(31.75, 36.0, 0.25, dtype=np.float32)
        ds.createVariable("latitude", "f4", ("latitude",))[:] = np.arange(-16.75, -22.0, -0.25, dtype=np.float32)
        ro = ds.createVariable("ro", "f4", ("time", "latitude", "longitude"))
        y, x = np.mgrid[0:21, 0:17]; spatial = 1.0 + 0.12 * np.sin((x + 1) / 3.0) * np.cos((y + 2) / 4.0)
        for i in range(days + 1): ro[i] = np.float32(runoff) * spatial * (1.0 + 0.08 * math.sin(i * 0.9))
    ntime = days * 144 + 1
    offset = (datetime(2019, 1, 1, tzinfo=timezone.utc) - datetime(1900, 1, 1, tzinfo=timezone.utc)).total_seconds()
    with netCDF4.Dataset(out / "sea_level.nc", "w", format="NETCDF4_CLASSIC") as ds:
        ds.createDimension("time", ntime); ds.createDimension("stations", 15)
        t = ds.createVariable("time", "f8", ("time",)); t.units = "seconds since 1900-01-01 00:00:00"; t.calendar = "proleptic_gregorian"; t[:] = offset + np.arange(ntime) * 600.0
        wl = ds.createVariable("waterlevel", "f4", ("time", "stations"))
        phase = np.arange(15)[None, :] * 0.17; clock = np.arange(ntime)[:, None] * (2.0 * np.pi / 74.52)
        wl[:] = (0.35 + 0.22 * np.sin(clock + phase) + 0.04 * np.sin(clock / 13.0)).astype(np.float32)


def make_deck(source: Path, work: Path, days: int, cpus: int) -> None:
    text = (source / "etc/sealev_boundary/test5-moz_06min_sealev.sh").read_text()
    changes = [
      ('BASE="/home/yamadai/work/CaMa_v401/cmf_v401_test"', f'BASE="{source}"'),
      ('INPDIR="${BASE}/etc/sealev_boundary/test_moz_06min_sealev"', f'INPDIR="{work / "inputs"}"'),
      ("export OMP_NUM_THREADS=16", f"export OMP_NUM_THREADS={cpus}"), ("SPINUP=1", "SPINUP=0"),
      ("EMON=5", "EMON=1"), ("EDAY=1", f"EDAY={days + 1}"), ('LRESTART=".TRUE."', 'LRESTART=".FALSE."'),
      ('CRESTSTO="${INPDIR}/restart2019010100.nc"', 'CRESTSTO=""'),
      ('CROFCDF="${INPDIR}/era5_ro_20190101-20190501_daily_moz.nc"', 'CROFCDF="${INPDIR}/runoff.nc"'),
      ('CSEALEVCDF="${INPDIR}/gtsm_era5_ro_20190101-20190501_moz.nc"', 'CSEALEVCDF="${INPDIR}/sea_level.nc"'),
      ('CVARSOUT="outflw,storge,maxdph,maxflw,flddph,fldsto,fldare,pthout"', 'CVARSOUT="outflw,storge,flddph,fldsto,fldare,pthout"')]
    for old, new in changes: text = replace_once(text, old, new)
    deck = work / "deck.sh"; deck.write_text(text); deck.chmod(0o755)


def extract_outputs(source: Path, out: Path) -> None:
    run_dir = source / "out/test5-moz_06min_sealev"; out.mkdir(parents=True, exist_ok=True)
    for name in ("outflw", "flddph", "fldsto", "pthout"):
        with netCDF4.Dataset(run_dir / f"o_{name}2019.nc") as ds:
            values = np.ma.filled(ds.variables[name][-1], 1.0e20).astype("<f8", copy=False).ravel()
        values[np.abs(values) >= 1.0e19] = 1.0e20; values.tofile(out / f"{name}.f64")


def main() -> None:
    ap = argparse.ArgumentParser(); ap.add_argument("action", choices=("prepare", "extract")); ap.add_argument("--source", type=Path, required=True); ap.add_argument("--work", type=Path, required=True); ap.add_argument("--out", type=Path); ap.add_argument("--params", type=Path); ap.add_argument("--days", type=int, default=5); ap.add_argument("--cpus", type=int, default=1); args = ap.parse_args()
    if args.action == "prepare":
        p = json.loads(args.params.read_text()); generate_forcing(args.work / "inputs", float(p["runoff_m_per_day"]), args.days); make_deck(args.source, args.work, args.days, args.cpus)
    else: extract_outputs(args.source, args.out)


if __name__ == "__main__": main()
