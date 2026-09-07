#!/user/wenbo.wu/.conda/envs/sqhao_seismo/bin/python
"""Build full SEM-DSM synthetics from 1-D DSM plus 3-D SEM-DSM scattering waves."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from obspy import Trace, read

COMPONENT_MAP = {
    "bhz": "bhz",  # vertical
    "bhn": "bhr",  # north/radial scattering component plus radial 1-D DSM
    "bhe": "bht",  # east/transverse scattering component plus transverse 1-D DSM
}


def read_stf(path: Path, dt: float) -> np.ndarray:
    stf = np.loadtxt(path)
    if stf.ndim != 2 or stf.shape[1] < 2:
        raise ValueError(f"STF must be at least two columns: {path}")
    t_stf = stf[:, 0]
    s_stf = stf[:, 1]
    t_uniform = np.arange(t_stf[0], t_stf[-1] + dt, dt)
    s_interp = np.interp(t_uniform, t_stf, s_stf)
    s_diff = np.gradient(s_interp, dt)
    norm = np.trapezoid(np.abs(s_diff), dx=dt)
    if norm > 0.0:
        s_diff /= norm
    return s_diff


def convolve_data(data: np.ndarray, dt: float, stf: np.ndarray | None, n_total: int) -> np.ndarray:
    if stf is None:
        return data
    conv_full = dt * np.convolve(data, stf, mode="full")
    return conv_full[:n_total]


def trace_from_one_d(tr1: Trace, data: np.ndarray, channel: str) -> Trace:
    out = tr1.copy()
    out.data = data.astype(np.float32)
    out.stats.npts = len(out.data)
    out.stats.delta = tr1.stats.delta
    out.stats.channel = channel.upper()
    out.stats.sac = tr1.stats.sac.copy()
    out.stats.sac.b = tr1.stats.sac.b
    out.stats.sac.o = 0
    return out


def one_d_trace(file_1d: Path, channel: str, stf: np.ndarray | None) -> Trace:
    tr1 = read(str(file_1d))[0]
    data = convolve_data(tr1.data.astype(np.float64), tr1.stats.delta, stf, tr1.stats.npts)
    return trace_from_one_d(tr1, data, channel)


def sum_traces(file_3d: Path, file_1d: Path, shift_3d: float, stf: np.ndarray | None) -> Trace:
    tr3 = read(str(file_3d))[0]
    tr1 = read(str(file_1d))[0]

    dt1 = tr1.stats.delta
    if abs(tr3.stats.delta - dt1) > 1e-6:
        tr3.resample(1.0 / dt1)

    # Positive shift_3d delays the 3-D scattering wave relative to the 1-D DSM trace.
    # Negative shift_3d advances it.
    i3 = int(round(shift_3d / dt1))

    prepad = max(0, -i3)
    n_total = prepad + max(
        tr1.stats.npts,
        i3 + tr3.stats.npts if i3 >= 0 else tr3.stats.npts - i3,
    )

    data_sum = np.zeros(n_total, dtype=np.float64)
    idx1 = prepad
    idx3 = prepad + i3
    data_sum[idx1 : idx1 + tr1.stats.npts] += tr1.data
    data_sum[idx3 : idx3 + tr3.stats.npts] += tr3.data
    data_sum = convolve_data(data_sum, dt1, stf, n_total)

    channel = tr3.stats.channel if tr3.stats.channel else file_3d.suffix[1:].upper()
    return trace_from_one_d(tr1, data_sum, channel)


def station_from_3d(path: Path) -> str:
    return path.name.rsplit(".", 1)[0]


def one_d_station_name(station: str, network: str) -> str:
    return f"{network}_{station}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--one-d-dir", type=Path, default=Path("../DSM/1D_tele_syn/410km_triplication_mij_python/CMTSOLUTION_time_sac"))
    parser.add_argument("--three-d-dir", type=Path, default=Path("../Coupling/seismograms"))
    parser.add_argument("--stf", type=Path, default=Path("FullSEM3D/plot_source_time_function.txt"))
    parser.add_argument("--output-dir", type=Path, default=Path("Hybrid_SEM_DSM"))
    parser.add_argument("--one-d-output-dir", type=Path, default=Path("DSM1D"))
    parser.add_argument("--network", default="TR")
    parser.add_argument("--depth", default="0.00")
    parser.add_argument("--shift-3d", type=float, default=35.0, help="Shift 3-D scattering trace in seconds relative to 1-D; positive delays 3-D")
    parser.add_argument("--t1-3d", type=float, default=None, help="Deprecated old option: converted as shift_3d=-t1_3d")
    parser.add_argument("--no-convolve", action="store_true")
    args = parser.parse_args()

    if args.t1_3d is not None:
        args.shift_3d = -args.t1_3d

    one_d_dir = args.one_d_dir.resolve()
    three_d_dir = args.three_d_dir.resolve()
    output_dir = args.output_dir.resolve()
    one_d_output_dir = args.one_d_output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    one_d_output_dir.mkdir(parents=True, exist_ok=True)

    if not one_d_dir.is_dir():
        raise FileNotFoundError(one_d_dir)
    if not three_d_dir.is_dir():
        raise FileNotFoundError(three_d_dir)

    stf_cache: dict[float, np.ndarray] = {}
    print(f"3-D scattering shift: {args.shift_3d:g} s (positive means delayed relative to 1-D)")
    n_written = 0
    n_one_d_written = 0
    missing: list[str] = []

    for file_3d in sorted(three_d_dir.glob("*.bh?")):
        comp_3d = file_3d.suffix[1:].lower()
        comp_1d = COMPONENT_MAP.get(comp_3d)
        if comp_1d is None:
            continue

        station = station_from_3d(file_3d)
        sta_1d = one_d_station_name(station, args.network)
        file_1d = one_d_dir / f"{sta_1d}_dep{args.depth}.{comp_1d}"
        if not file_1d.exists():
            missing.append(f"{file_3d.name} -> {file_1d}")
            continue

        if args.no_convolve:
            stf = None
        else:
            dt = read(str(file_1d), headonly=True)[0].stats.delta
            stf = stf_cache.setdefault(dt, read_stf(args.stf, dt))

        out_1d = one_d_trace(file_1d, comp_3d, stf)
        out_1d.stats.network = args.network
        out_1d.stats.station = station
        out_1d.stats.channel = comp_3d.upper()
        out_1d_path = one_d_output_dir / f"{station}.{args.network}.{comp_3d}"
        out_1d.write(str(out_1d_path), format="SAC")
        print(f"wrote {out_1d_path}")
        n_one_d_written += 1

        out = sum_traces(file_3d, file_1d, args.shift_3d, stf)
        out.stats.network = args.network
        out.stats.station = station
        out.stats.channel = comp_3d.upper()
        out_path = output_dir / f"{station}.{args.network}.{comp_3d}"
        out.write(str(out_path), format="SAC")
        print(f"wrote {out_path}")
        n_written += 1

    if missing:
        print("missing 1-D matches:")
        for item in missing:
            print(f"  {item}")
    print(f"wrote {n_one_d_written} 1-D DSM SAC files to {one_d_output_dir}")
    print(f"wrote {n_written} hybrid SEM-DSM SAC files to {output_dir}")


if __name__ == "__main__":
    main()
