#!/usr/bin/env python3
"""
plot_shield_spectra.py
======================
Visualise shieldSim_spectra.dat produced by the Geant4 shieldSim application.

Usage
-----
    python plot_shield_spectra.py [file] [options]

Energy range options  — set the x-axis window shown in every panel.
Bins outside this range are simply not drawn (x-axis is clipped).
--------------------------------------------------------------------
  --emin  <MeV>     Lower energy bound for both species (default: full range)
  --emax  <MeV>     Upper energy bound for both species (default: full range)
  --emin-p <MeV>    Proton lower bound  (overrides --emin)
  --emax-p <MeV>    Proton upper bound  (overrides --emax)
  --emin-a <MeV>    Alpha lower bound   (overrides --emin)
  --emax-a <MeV>    Alpha upper bound   (overrides --emax)

  When proton and alpha limits differ the x-axis shows the union of
  both ranges so no curve is cut off.

Other options
-------------
  --save             Write figure to disk
  --format           pdf|png|svg|jpg  (default: pdf)
  --dpi              Raster DPI for png/jpg (default: 150)
  --no-show          Skip interactive window

Examples
--------
  python plot_shield_spectra.py

  # Show only 1 MeV – 1 GeV
  python plot_shield_spectra.py --emin 1 --emax 1000

  # Different per-species windows (x-axis shows union)
  python plot_shield_spectra.py --emin-p 10 --emax-p 500 \\
                                --emin-a 50 --emax-a 2000 \\
                                --save --format pdf --no-show
"""

import sys, os, re, argparse
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
def parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Visualise shieldSim Tecplot spectrum output.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("datfile", nargs="?", default="shieldSim_spectra.dat")

    eg = p.add_argument_group("energy range (sets x-axis window)")
    eg.add_argument("--emin",   type=float, default=None, metavar="MeV")
    eg.add_argument("--emax",   type=float, default=None, metavar="MeV")
    eg.add_argument("--emin-p", type=float, default=None, metavar="MeV", dest="emin_p")
    eg.add_argument("--emax-p", type=float, default=None, metavar="MeV", dest="emax_p")
    eg.add_argument("--emin-a", type=float, default=None, metavar="MeV", dest="emin_a")
    eg.add_argument("--emax-a", type=float, default=None, metavar="MeV", dest="emax_a")

    og = p.add_argument_group("output")
    og.add_argument("--save",    action="store_true")
    og.add_argument("--format",  default="pdf", choices=["pdf","png","svg","jpg"])
    og.add_argument("--dpi",     type=int, default=150)
    og.add_argument("--no-show", action="store_true")

    args = p.parse_args(argv)
    # species-specific bounds inherit from global, then override
    if args.emin_p is None: args.emin_p = args.emin
    if args.emax_p is None: args.emax_p = args.emax
    if args.emin_a is None: args.emin_a = args.emin
    if args.emax_a is None: args.emax_a = args.emax
    return args

# ---------------------------------------------------------------------------
# Tecplot parser
# ---------------------------------------------------------------------------
def parse_tecplot(path):
    if not os.path.isfile(path):
        sys.exit(f"ERROR: file not found: {path}")
    meta = {"title":"","zone":"","variables":[],"shield":"","phi":""}
    rows = []
    with open(path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line: continue
            if line.startswith("#"):
                m = re.search(r"Shield[:\s]+(.+)", line, re.I)
                if m: meta["shield"] = m.group(1).strip()
                m = re.search(r"phi\s*=\s*(\d+)", line, re.I)
                if m: meta["phi"] = m.group(1).strip()
                continue
            u = line.upper()
            if u.startswith("TITLE"):
                m = re.search(r'"([^"]+)"', line)
                if m: meta["title"] = m.group(1)
                continue
            if u.startswith("VARIABLES"):
                meta["variables"] = re.findall(r'"([^"]+)"', line)
                continue
            if u.startswith("ZONE"):
                m = re.search(r'T="([^"]+)"', line, re.I)
                if m: meta["zone"] = m.group(1)
                continue
            try:
                vals = [float(v) for v in line.split()]
                if len(vals) == 6: rows.append(vals)
            except ValueError:
                pass
    if not rows:
        sys.exit(f"ERROR: no data rows found in {path}")
    return meta, np.array(rows)

# ---------------------------------------------------------------------------
# X-axis window
# ---------------------------------------------------------------------------
def xlimits(E, args):
    """
    Return (xlo, xhi) for the plot x-axis.
    Takes the union of proton and alpha ranges so neither curve is clipped.
    Falls back to the full data range when no limits are specified.
    """
    xlo = min(
        args.emin_p if args.emin_p is not None else E.min(),
        args.emin_a if args.emin_a is not None else E.min(),
    )
    xhi = max(
        args.emax_p if args.emax_p is not None else E.max(),
        args.emax_a if args.emax_a is not None else E.max(),
    )
    # add a small margin
    return xlo * 0.85, xhi * 1.2

def range_str(args, E):
    """Human-readable description of the selected energy window."""
    lo = min(
        args.emin_p if args.emin_p is not None else E.min(),
        args.emin_a if args.emin_a is not None else E.min(),
    )
    hi = max(
        args.emax_p if args.emax_p is not None else E.max(),
        args.emax_a if args.emax_a is not None else E.max(),
    )
    def fmt(v):
        if v >= 1e3: return f"{v/1e3:.4g} GeV"
        return f"{v:.4g} MeV"
    return f"{fmt(lo)} – {fmt(hi)}"

# ---------------------------------------------------------------------------
# Styling helpers
# ---------------------------------------------------------------------------
PAL = {
    "in_proton":   "#1a73e8",
    "in_alpha":    "#e8710a",
    "out_proton":  "#34a853",
    "out_alpha":   "#ea4335",
    "out_neutron": "#9c27b0",
}

def eaxis(ax):
    ax.set_xscale("log")
    ax.set_xlabel("Kinetic energy (MeV)", fontsize=10)
    ax.tick_params(which="both", direction="in", top=True, right=True, labelsize=9)
    ax.xaxis.set_major_formatter(ticker.FuncFormatter(
        lambda x, _: f"{x:g}" if x < 1e3 else f"{x/1e3:.4g} GeV"))
    ax.grid(True, which="major", ls="-",  lw=0.4, alpha=0.4)
    ax.grid(True, which="minor", ls=":",  lw=0.3, alpha=0.25)

def style_loglog(ax, ylabel=r"$dJ/dE$  [1 / (MeV · event)]"):
    ax.set_yscale("log")
    ax.set_ylabel(ylabel, fontsize=10)
    eaxis(ax)

def data_ylim(arrays, xlo, xhi, E, margin_decades=1.5):
    """
    Y-limits derived only from data that falls within the visible x-window.
    Ignores zero / NaN values so the axis floor is never dominated by empties.
    """
    all_vals = []
    for a in arrays:
        in_window = (E >= xlo) & (E <= xhi)
        vals = a[in_window]
        vals = vals[np.isfinite(vals) & (vals > 0)]
        all_vals.append(vals)
    combined = np.concatenate(all_vals) if all_vals else np.array([])
    if len(combined) == 0:
        return (1e-15, 1e-5)
    yhi = combined.max() * 3.0
    ylo = combined.min() / 10**margin_decades
    return (ylo, yhi)

def fill_below(ax, x, y, color, alpha=0.10):
    """Fill under curve down to the current axis floor (no sentinel values)."""
    yfloor = ax.get_ylim()[0]
    yp = np.where(np.isfinite(y) & (y > yfloor), y, np.nan)
    if np.sum(np.isfinite(yp)) < 2: return
    ax.fill_between(x, yfloor,
                    np.where(np.isfinite(yp), yp, yfloor),
                    color=color, alpha=alpha, linewidth=0)

# ---------------------------------------------------------------------------
# Clean data: replace zeros/negatives with NaN (not epsilon)
# ---------------------------------------------------------------------------
def clean(arr):
    out = arr.copy().astype(float)
    out[out <= 0] = np.nan
    return out

# ---------------------------------------------------------------------------
# Figure
# ---------------------------------------------------------------------------
def make_figure(meta, data, args):
    E    = data[:, 0]
    inP  = clean(data[:, 1])
    inA  = clean(data[:, 2])
    outP = clean(data[:, 3])
    outA = clean(data[:, 4])
    outN = clean(data[:, 5])

    # E^2.7 scaled versions
    sc   = E ** 2.7
    inP_s  = inP  * sc
    inA_s  = inA  * sc
    outP_s = outP * sc
    outA_s = outA * sc

    # Transmission fraction
    with np.errstate(divide="ignore", invalid="ignore"):
        transP = np.where(np.isfinite(inP)  & (inP  > 0), outP / inP,  np.nan)
        transA = np.where(np.isfinite(inA)  & (inA  > 0), outA / inA,  np.nan)

    # X-axis window (union of proton + alpha ranges)
    xlo, xhi = xlimits(E, args)

    # Build figure title
    suptitle = meta["title"] or "GCR Shielding Simulation"
    if meta["shield"]:
        suptitle += f"\nShield: {meta['shield']}"
        if meta["phi"]: suptitle += f"   |   φ = {meta['phi']} MV"
    # Append energy window only if user restricted it
    any_limit = any(v is not None for v in
                    [args.emin_p, args.emax_p, args.emin_a, args.emax_a])
    if any_limit:
        suptitle += f"\nEnergy window: {range_str(args, E)}"

    matplotlib.rcParams.update({"font.family": "sans-serif", "font.size": 10})
    fig, axes = plt.subplots(3, 2, figsize=(12, 13))
    fig.suptitle(suptitle, fontsize=11, fontweight="bold", y=0.999)
    fig.subplots_adjust(hspace=0.44, wspace=0.33,
                        left=0.09, right=0.97, top=0.96, bottom=0.06)

    # Pre-compute y-limits from visible data only
    yl_in   = data_ylim([inP,  inA],            xlo, xhi, E)
    yl_out  = data_ylim([outP, outA, outN],      xlo, xhi, E)
    yl_sc   = data_ylim([inP_s,  inA_s],         xlo, xhi, E)
    yl_outs = data_ylim([outP_s, outA_s],         xlo, xhi, E)
    yl_ov   = data_ylim([inP, inA, outP, outA, outN], xlo, xhi, E)

    # Transmission y-limits: clamp to sensible physical range
    tr_vals = np.concatenate([transP[np.isfinite(transP)],
                               transA[np.isfinite(transA)]])
    if len(tr_vals):
        tr_lo = max(tr_vals.min() / 100, 1e-5)
        tr_hi = min(tr_vals.max() * 3,   2.0)
    else:
        tr_lo, tr_hi = 1e-5, 2.0

    # ---- Row 0 left — input, raw ----------------------------------------
    ax = axes[0, 0]
    ax.set_yscale("log"); ax.set_ylim(*yl_in); ax.set_xlim(xlo, xhi)
    ax.plot(E, inP, color=PAL["in_proton"], lw=1.6, label="Proton")
    ax.plot(E, inA, color=PAL["in_alpha"],  lw=1.6, label="Alpha")
    fill_below(ax, E, inP, PAL["in_proton"])
    fill_below(ax, E, inA, PAL["in_alpha"])
    style_loglog(ax); ax.legend(fontsize=9, framealpha=0.85)
    ax.set_title("Input spectrum — differential flux", fontsize=10)

    # ---- Row 0 right — input, E^2.7 -------------------------------------
    ax = axes[0, 1]
    ax.set_yscale("log"); ax.set_ylim(*yl_sc); ax.set_xlim(xlo, xhi)
    ax.plot(E, inP_s, color=PAL["in_proton"], lw=1.6, label="Proton")
    ax.plot(E, inA_s, color=PAL["in_alpha"],  lw=1.6, label="Alpha")
    fill_below(ax, E, inP_s, PAL["in_proton"])
    fill_below(ax, E, inA_s, PAL["in_alpha"])
    style_loglog(ax, ylabel=r"$E^{2.7}\!\cdot\!dJ/dE$  [arb.]")
    ax.legend(fontsize=9, framealpha=0.85)
    ax.set_title(r"Input — $E^{2.7}$-weighted (GCR standard)", fontsize=10)

    # ---- Row 1 left — output, raw ---------------------------------------
    ax = axes[1, 0]
    ax.set_yscale("log"); ax.set_ylim(*yl_out); ax.set_xlim(xlo, xhi)
    ax.plot(E, outP, color=PAL["out_proton"],  lw=1.6, label="Proton (transmitted)")
    ax.plot(E, outA, color=PAL["out_alpha"],   lw=1.6, label="Alpha (transmitted)")
    ax.plot(E, outN, color=PAL["out_neutron"], lw=1.4, ls="--",
            label="Neutron (secondary)")
    fill_below(ax, E, outP, PAL["out_proton"])
    fill_below(ax, E, outA, PAL["out_alpha"])
    fill_below(ax, E, outN, PAL["out_neutron"])
    style_loglog(ax); ax.legend(fontsize=8.5, framealpha=0.85)
    ax.set_title("Output spectrum — behind shield", fontsize=10)

    # ---- Row 1 right — output, E^2.7 ------------------------------------
    ax = axes[1, 1]
    ax.set_yscale("log"); ax.set_ylim(*yl_outs); ax.set_xlim(xlo, xhi)
    ax.plot(E, outP_s, color=PAL["out_proton"], lw=1.6, label="Proton (transmitted)")
    ax.plot(E, outA_s, color=PAL["out_alpha"],  lw=1.6, label="Alpha (transmitted)")
    fill_below(ax, E, outP_s, PAL["out_proton"])
    fill_below(ax, E, outA_s, PAL["out_alpha"])
    style_loglog(ax, ylabel=r"$E^{2.7}\!\cdot\!dJ/dE$  [arb.]")
    ax.legend(fontsize=8.5, framealpha=0.85)
    ax.set_title(r"Output — $E^{2.7}$-weighted", fontsize=10)

    # ---- Row 2 left — transmission fraction -----------------------------
    ax = axes[2, 0]
    ax.set_yscale("log"); ax.set_ylim(tr_lo, tr_hi); ax.set_xlim(xlo, xhi)
    ax.plot(E, transP, color=PAL["out_proton"], lw=1.6, label="Proton")
    ax.plot(E, transA, color=PAL["out_alpha"],  lw=1.6, label="Alpha")
    ax.axhline(1.0, color="gray", lw=0.8, ls="--", alpha=0.6)
    for level, lbl in [(0.5, "50%"), (0.1, "10%"), (0.01, "1%")]:
        if tr_lo < level < tr_hi:
            ax.axhline(level, color="gray", lw=0.5, ls=":", alpha=0.45)
            ax.text(xlo * 1.15, level * 1.3, lbl,
                    fontsize=7.5, color="gray", va="bottom")
    ax.set_ylabel(r"$T(E)=J_\mathrm{out}/J_\mathrm{in}$", fontsize=10)
    eaxis(ax); ax.legend(fontsize=9, framealpha=0.85)
    ax.set_title("Shield transmission fraction", fontsize=10)

    # ---- Row 2 right — full overlay -------------------------------------
    ax = axes[2, 1]
    ax.set_yscale("log"); ax.set_ylim(*yl_ov); ax.set_xlim(xlo, xhi)
    ax.plot(E, inP,  color=PAL["in_proton"],  lw=1.6, ls="-",  label="Proton — input")
    ax.plot(E, outP, color=PAL["out_proton"], lw=1.6, ls="--", label="Proton — output")
    ax.plot(E, inA,  color=PAL["in_alpha"],   lw=1.6, ls="-",  label="Alpha — input")
    ax.plot(E, outA, color=PAL["out_alpha"],  lw=1.6, ls="--", label="Alpha — output")
    ax.plot(E, outN, color=PAL["out_neutron"],lw=1.4, ls=":",  label="Neutron — secondary")
    style_loglog(ax); ax.legend(fontsize=8.2, framealpha=0.85)
    ax.set_title("Input vs output overlay", fontsize=10)

    if meta["zone"]:
        fig.text(0.5, 0.001, meta["zone"], ha="center", va="bottom",
                 fontsize=8, color="dimgray", style="italic")
    return fig

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main(argv=None):
    args = parse_args(argv)
    meta, data = parse_tecplot(args.datfile)
    E = data[:, 0]

    xlo, xhi = xlimits(E, args)
    n_visible = np.sum((E >= xlo) & (E <= xhi))

    print(f"Loaded {len(data)} bins from: {args.datfile}")
    print(f"  Zone   : {meta['zone']}")
    print(f"  Shield : {meta['shield']}")
    print(f"  Data range    : {E.min():.3g} – {E.max():.4g} MeV")
    print(f"  X-axis window : {range_str(args, E)}  ({n_visible} bins visible)")

    fig = make_figure(meta, data, args)

    if args.save:
        stem = os.path.splitext(args.datfile)[0]
        out  = f"{stem}.{args.format}"
        kw   = {"dpi": args.dpi} if args.format in ("png", "jpg") else {}
        fig.savefig(out, bbox_inches="tight", **kw)
        print(f"Saved: {out}")

    if not args.no_show:
        plt.show()

if __name__ == "__main__":
    main()
