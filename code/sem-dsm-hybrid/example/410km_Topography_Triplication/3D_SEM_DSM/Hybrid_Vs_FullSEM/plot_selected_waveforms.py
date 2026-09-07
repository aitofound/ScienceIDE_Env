#!/user/wenbo.wu/.conda/envs/sqhao_seismo/bin/python
"""Plot selected FullSEM3D, DSM1D, and Hybrid_SEM_DSM waveforms.

Default comparison uses the vertical component:
  FullSEM3D/sac/<station>.TR.HXZ.sac
  DSM1D/<station>.TR.bhz
  Hybrid_SEM_DSM/<station>.TR.bhz
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from obspy import read

STATIONS = ("A3", "B3", "13.15deg", "14.15deg")
DATASETS = (
    ("FullSEM3D", Path("FullSEM3D/sac"), "HXZ.sac", "#1f77b4", "full_time_shift", "-", 1.3),
    ("DSM1D", Path("DSM1D"), "bhz", "#d62728", "dsm_time_shift", "-", 1.3),
    ("Hybrid", Path("Hybrid_SEM_DSM"), "bhz", "#2ca02c", "hybrid_time_shift", "--", 2.2),
)
REFERENCE_LABEL = "FullSEM3D"


def sac_time_axis(tr) -> np.ndarray:
    if hasattr(tr.stats, "sac"):
        b = float(getattr(tr.stats.sac, "b", 0.0))
        o = float(getattr(tr.stats.sac, "o", 0.0))
    else:
        b = 0.0
        o = 0.0
    return b - o + np.arange(tr.stats.npts) * tr.stats.delta


def read_filtered(path: Path, freqmin: float, freqmax: float) -> tuple[np.ndarray, np.ndarray]:
    tr = read(str(path))[0]
    tr.detrend("demean")
    tr.detrend("linear")
    tr.taper(max_percentage=0.05, type="hann")
    tr.filter("bandpass", freqmin=freqmin, freqmax=freqmax, corners=1, zerophase=True)
    return sac_time_axis(tr), tr.data.astype(np.float64)


def window_trace(t: np.ndarray, y: np.ndarray, xmin: float, xmax: float, time_shift: float) -> tuple[np.ndarray, np.ndarray]:
    t_shifted = t + time_shift
    keep = (t_shifted >= xmin) & (t_shifted <= xmax)
    if not np.any(keep):
        raise ValueError(f"no samples in requested window {xmin:g}-{xmax:g} s after shift {time_shift:g} s")
    return t_shifted[keep], y[keep]


def station_path(station: str, directory: Path, suffix: str, network: str) -> Path:
    return directory / f"{station}.{network}.{suffix}"


def estimate_shift(ref_t: np.ndarray, ref_y: np.ndarray, target_t: np.ndarray, target_y: np.ndarray, xmin: float, xmax: float) -> float:
    ref_keep = (ref_t >= xmin) & (ref_t <= xmax)
    target_keep = (target_t >= xmin) & (target_t <= xmax)
    if not np.any(ref_keep) or not np.any(target_keep):
        return 0.0

    ref_tw = ref_t[ref_keep]
    ref_yw = ref_y[ref_keep]
    target_tw = target_t[target_keep]
    target_yw = target_y[target_keep]
    dt = max(float(np.median(np.diff(ref_tw))), float(np.median(np.diff(target_tw))))
    if not np.isfinite(dt) or dt <= 0.0:
        return 0.0

    grid = np.arange(xmin, xmax + 0.5 * dt, dt)
    ref_grid = np.interp(grid, ref_tw, ref_yw)
    target_grid = np.interp(grid, target_tw, target_yw)
    ref_grid -= ref_grid.mean()
    target_grid -= target_grid.mean()
    ref_norm = np.linalg.norm(ref_grid)
    target_norm = np.linalg.norm(target_grid)
    if ref_norm == 0.0 or target_norm == 0.0:
        return 0.0

    corr = np.correlate(ref_grid / ref_norm, target_grid / target_norm, mode="full")
    lag = int(np.argmax(corr) - (len(target_grid) - 1))
    return -lag * dt


def plot_station(station: str, args: argparse.Namespace) -> list[str]:
    fig, ax = plt.subplots(figsize=(10, 4.8), constrained_layout=True)
    missing: list[str] = []
    traces: dict[str, tuple[np.ndarray, np.ndarray, str, str, float, str, float]] = {}

    for label, directory, suffix, color, shift_arg, linestyle, linewidth in DATASETS:
        path = station_path(station, directory, suffix, args.network)
        if not path.exists():
            missing.append(f"{label}: {path}")
            continue
        t, y = read_filtered(path, args.freqmin, args.freqmax)
        traces[label] = (t, y, color, shift_arg, getattr(args, shift_arg), linestyle, linewidth)

    auto_shifts: dict[str, float] = {label: 0.0 for label in traces}
    if args.auto_align and REFERENCE_LABEL in traces:
        ref_t, ref_y, _, _, ref_user_shift, _, _ = traces[REFERENCE_LABEL]
        ref_t_shifted = ref_t + ref_user_shift
        for label, (t, y, _, _, user_shift, _, _) in traces.items():
            if label == REFERENCE_LABEL:
                continue
            auto_shifts[label] = estimate_shift(ref_t_shifted, ref_y, t + user_shift, y, args.xmin, args.xmax)

    plotted = 0
    shift_notes: list[str] = []
    for label, (t, y, color, _shift_arg, user_shift, linestyle, linewidth) in traces.items():
        total_shift = user_shift + auto_shifts.get(label, 0.0)
        tw, yw = window_trace(t, y, args.xmin, args.xmax, total_shift)
        if args.normalize:
            scale = np.max(np.abs(yw))
            if scale > 0.0:
                yw = yw / scale
        legend = label
        if total_shift != 0.0:
            legend = f"{label} ({total_shift:+.3g}s)"
        ax.plot(tw, yw, lw=linewidth, color=color, linestyle=linestyle, label=legend)
        plotted += 1
        if total_shift != 0.0:
            shift_notes.append(f"{label}={total_shift:+.3g}s")

    ax.set_title(f"{station} vertical waveforms ({args.freqmin:g}-{args.freqmax:g} Hz, p=1)")
    ax.set_xlabel("Time from SAC origin o (s)")
    ax.set_ylabel("Normalized amplitude" if args.normalize else "Amplitude")
    ax.set_xlim(args.xmin, args.xmax)
    ax.grid(True, alpha=0.3)
    if plotted:
        ax.legend(loc="upper right", frameon=False)
    else:
        ax.text(0.5, 0.5, "No waveforms found", transform=ax.transAxes, ha="center", va="center")

    notes = []
    if missing:
        notes.append("Missing: " + "; ".join(item.split(":", 1)[0] for item in missing))
    if shift_notes:
        notes.append("Shifts: " + "; ".join(shift_notes))
    if notes:
        ax.text(0.01, 0.02, "\n".join(notes), transform=ax.transAxes, ha="left", va="bottom", fontsize=9, color="0.35")

    outpath = Path(args.output_dir) / f"{station}_waveforms.png"
    fig.savefig(outpath, dpi=args.dpi)
    plt.close(fig)

    if args.auto_align and shift_notes:
        print(f"{station} auto/user shifts: " + ", ".join(shift_notes))
    return missing


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stations", nargs="+", default=list(STATIONS))
    parser.add_argument("--network", default="TR")
    parser.add_argument("--xmin", type=float, default=170.0)
    parser.add_argument("--xmax", type=float, default=220.0)
    parser.add_argument("--freqmin", type=float, default=0.3)
    parser.add_argument("--freqmax", type=float, default=0.8)
    parser.add_argument("--output-dir", type=Path, default=Path("."))
    parser.add_argument("--dpi", type=int, default=200)
    parser.add_argument("--full-time-shift", type=float, default=0.0, help="Seconds added to FullSEM3D time axis")
    parser.add_argument("--dsm-time-shift", type=float, default=-1.5, help="Seconds added to DSM1D time axis")
    parser.add_argument("--hybrid-time-shift", type=float, default=-1.5, help="Seconds added to Hybrid time axis")
    parser.add_argument("--auto-align", action="store_true", help="Cross-correlate DSM1D/Hybrid to FullSEM3D in the plotted window")
    parser.add_argument("--normalize", action="store_true", help="Normalize each trace by its own max absolute amplitude")
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    print(
        f"time shifts: FullSEM3D={args.full_time_shift:g} s, "
        f"DSM1D={args.dsm_time_shift:g} s, Hybrid={args.hybrid_time_shift:g} s, "
        f"auto_align={args.auto_align}"
    )
    all_missing: dict[str, list[str]] = {}
    for station in args.stations:
        missing = plot_station(station, args)
        if missing:
            all_missing[station] = missing
        print(f"wrote {args.output_dir / (station + '_waveforms.png')}")

    if all_missing:
        print("missing traces:")
        for station, missing in all_missing.items():
            print(f"  {station}:")
            for item in missing:
                print(f"    {item}")


if __name__ == "__main__":
    main()
