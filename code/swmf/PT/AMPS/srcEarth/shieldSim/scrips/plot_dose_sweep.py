#!/usr/bin/env python3
"""
plot_dose_sweep.py
==================
Visualise shieldSim_dose_sweep.dat — dose per primary GCR particle
as a function of shield thickness, produced by the Geant4 shieldSim
application in sweep mode (--sweep).

Usage
-----
    python plot_dose_sweep.py [file] [options]

Options
-------
  positional             Path to the Tecplot .dat file.
                         Default: shieldSim_dose_sweep.dat
  --xaxis   mm|gcm2      X-axis quantity: thickness in mm or areal
                         density in g/cm² (default: mm).
  --yunit   Gy|mGy|uGy  Dose unit on y-axis (default: Gy).
  --fit                  Overlay a least-squares exponential fit
                         D(t) = D0 * exp(-t/lambda) for each scorer.
                         Fit parameters printed to console.
  --halving              Mark the halving thickness (T_½, where dose
                         drops to 50 % of its value at the first point)
                         with a vertical dashed line.
  --ref=<Gy>             Draw a horizontal reference dose line, e.g.
                         --ref=1e-12 for a mission dose-rate limit.
  --ref-label=<text>     Label for the reference line (default: "Limit").
  --save                 Save figure to disk.
  --format  pdf|png|svg  Output format (default: pdf).
  --dpi     <n>          Raster DPI for png (default: 150).
  --no-show              Skip interactive window.

Output files (with --save)
--------------------------
  shieldSim_dose_sweep.{pdf|png|svg}

Examples
--------
  python plot_dose_sweep.py

  python plot_dose_sweep.py --xaxis=gcm2 --yunit=mGy --fit --halving

  python plot_dose_sweep.py --ref=1e-13 --ref-label="10-year limit" \\
                            --fit --save --format=pdf --no-show
"""

import sys, os, re, argparse
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from scipy.optimize import curve_fit

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------
def parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Visualise shieldSim dose-vs-thickness sweep.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("datfile", nargs="?", default="shieldSim_dose_sweep.dat")
    p.add_argument("--xaxis",     default="mm",  choices=["mm", "gcm2"],
                   help="X-axis: thickness in mm or areal density g/cm² (default: mm)")
    p.add_argument("--yunit",     default="Gy",  choices=["Gy", "mGy", "uGy"],
                   help="Dose unit on y-axis (default: Gy)")
    p.add_argument("--fit",       action="store_true",
                   help="Overlay exponential fit D0*exp(-t/lambda)")
    p.add_argument("--halving",   action="store_true",
                   help="Mark halving thickness (dose = 50%% of first point)")
    p.add_argument("--ref",       type=float, default=None, metavar="Gy",
                   help="Draw a horizontal reference dose line in Gy")
    p.add_argument("--ref-label", default="Limit", dest="ref_label")
    p.add_argument("--save",      action="store_true")
    p.add_argument("--format",    default="pdf", choices=["pdf","png","svg","jpg"])
    p.add_argument("--dpi",       type=int, default=150)
    p.add_argument("--no-show",   action="store_true")
    return p.parse_args(argv)

# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------
def parse_sweep(path):
    """
    Returns
    -------
    meta : dict  keys: title, shield, events, emin_p, emax_p,
                       emin_a, emax_a, scorer_names
    x    : np.ndarray  shape (N,2)  cols: thickness_mm, areal_density_g_cm2
    dose : np.ndarray  shape (N, n_scorers)  in Gy
    """
    if not os.path.isfile(path):
        sys.exit(f"ERROR: file not found: {path}")

    meta = {
        "title": "", "shield": "", "events": "",
        "emin_p": "", "emax_p": "", "emin_a": "", "emax_a": "",
        "scorer_names": [],
    }
    rows = []

    with open(path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue

            # --- comment metadata ---
            if line.startswith("#"):
                m = re.search(r"Shield material:\s*(.+)", line, re.I)
                if m: meta["shield"] = m.group(1).strip()
                m = re.search(r"Events per point:\s*(\d+)", line, re.I)
                if m: meta["events"] = m.group(1).strip()
                m = re.search(r"Proton energy range:\s*\[([^,]+),\s*([^\]]+)\]", line, re.I)
                if m: meta["emin_p"], meta["emax_p"] = m.group(1).strip(), m.group(2).strip()
                m = re.search(r"Alpha\s+energy range:\s*\[([^,]+),\s*([^\]]+)\]", line, re.I)
                if m: meta["emin_a"], meta["emax_a"] = m.group(1).strip(), m.group(2).strip()
                m = re.search(r"Scoring volumes:\s*(.+)", line, re.I)
                if m: meta["scorer_names"] = m.group(1).strip().split()
                continue

            u = line.upper()
            if u.startswith("TITLE"):
                m = re.search(r'"([^"]+)"', line)
                if m: meta["title"] = m.group(1)
                continue
            if u.startswith("VARIABLES"):
                # Extract dose column names: "Dose_<MAT>_perPrimary [Gy]"
                cols = re.findall(r'"([^"]+)"', line)
                if not meta["scorer_names"]:
                    for c in cols:
                        m = re.match(r"Dose_(.+?)_perPrimary", c)
                        if m: meta["scorer_names"].append(m.group(1))
                continue
            if u.startswith("ZONE"):
                continue

            try:
                vals = [float(v) for v in line.split()]
                if len(vals) >= 3:
                    rows.append(vals)
            except ValueError:
                pass

    if not rows:
        sys.exit(f"ERROR: no data rows in {path}")

    arr  = np.array(rows)
    x    = arr[:, :2]                        # thickness_mm, areal_density
    dose = arr[:, 2:]                        # Gy per primary, one col per scorer
    return meta, x, dose

# ---------------------------------------------------------------------------
# Exponential fit
# ---------------------------------------------------------------------------
def exp_fit(xvals, yvals):
    """Fit y = D0 * exp(-x/lam).  Returns (D0, lam) or None on failure."""
    try:
        popt, _ = curve_fit(
            lambda x, D0, lam: D0 * np.exp(-x / lam),
            xvals, yvals,
            p0=[yvals[0], xvals[-1] / 2],
            maxfev=5000,
        )
        return popt  # (D0, lam)
    except Exception:
        return None

# ---------------------------------------------------------------------------
# Colour palette
# ---------------------------------------------------------------------------
COLOURS = ["#1a73e8", "#ea4335", "#34a853", "#fb8c00",
           "#9c27b0", "#00acc1", "#795548", "#607d8b"]

# ---------------------------------------------------------------------------
# Main figure
# ---------------------------------------------------------------------------
def make_figure(meta, x, dose, args):
    n_sc = dose.shape[1]
    names = meta["scorer_names"] if meta["scorer_names"] else [f"Scorer {i}" for i in range(n_sc)]

    # Unit conversion
    scale = {"Gy": 1.0, "mGy": 1e3, "uGy": 1e6}[args.yunit]
    yscale = scale
    ylabel = f"Dose per primary [{args.yunit}]"

    # X-axis data
    if args.xaxis == "mm":
        xdata  = x[:, 0]
        xlabel = "Shield thickness (mm)"
        xfit   = xdata
    else:
        xdata  = x[:, 1]
        xlabel = r"Areal density (g cm$^{-2}$)"
        xfit   = xdata

    # Reference line in display units
    ref_display = args.ref * yscale if args.ref is not None else None

    # Halving thicknesses
    halving = {}
    for i in range(n_sc):
        d = dose[:, i]
        if d[0] > 0:
            target = d[0] * 0.5
            idx = np.searchsorted(-d, -target)
            if 0 < idx < len(xdata):
                # linear interpolation
                x0, x1 = xdata[idx-1], xdata[idx]
                d0, d1 = d[idx-1], d[idx]
                if d0 != d1:
                    halving[i] = x0 + (target - d0) / (d1 - d0) * (x1 - x0)

    # Fit results
    fit_params = {}
    if args.fit:
        for i in range(n_sc):
            result = exp_fit(xfit, dose[:, i])
            if result is not None:
                fit_params[i] = result

    # -----------------------------------------------------------------------
    # Figure layout: 2 rows × 1 col (linear + log)
    # -----------------------------------------------------------------------
    matplotlib.rcParams.update({"font.family": "sans-serif", "font.size": 10})
    fig, (ax_lin, ax_log) = plt.subplots(2, 1, figsize=(9, 10),
                                          sharex=True)
    fig.subplots_adjust(hspace=0.08, left=0.12, right=0.97,
                        top=0.93, bottom=0.09)

    # Build suptitle
    st = meta["title"] or "GCR Dose vs Shield Thickness"
    if meta["shield"]:
        st += f"\nShield: {meta['shield']}"
    parts = []
    if meta["events"]: parts.append(f"{meta['events']} events/pt")
    if meta["emin_p"] and meta["emax_p"]:
        parts.append(f"p: [{meta['emin_p']}, {meta['emax_p']}] MeV")
    if meta["emin_a"] and meta["emax_a"]:
        parts.append(f"α: [{meta['emin_a']}, {meta['emax_a']}] MeV")
    if parts:
        st += "   |   " + "   |   ".join(parts)
    fig.suptitle(st, fontsize=10.5, fontweight="bold")

    # x-axis display limits with small margin
    xlo = xdata[0]  * (0.85 if xdata[0] > 0 else 1.0)
    xhi = xdata[-1] * 1.05
    fit_x_dense = np.linspace(xdata[0], xdata[-1], 300)

    # -----------------------------------------------------------------------
    for ax, logy in [(ax_lin, False), (ax_log, True)]:
        for i in range(n_sc):
            clr = COLOURS[i % len(COLOURS)]
            d   = dose[:, i] * yscale
            lbl = names[i].replace("G4_", "")

            ax.plot(xdata, d, "o-", color=clr, lw=1.8, ms=5,
                    label=lbl, zorder=3)

            # Exponential fit overlay
            if args.fit and i in fit_params:
                D0, lam = fit_params[i]
                d_fit = D0 * np.exp(-fit_x_dense / lam) * yscale
                ax.plot(fit_x_dense, d_fit, "--", color=clr, lw=1.2,
                        alpha=0.7, zorder=2,
                        label=f"{lbl} fit: λ={lam:.2f} {args.xaxis}")

            # Halving thickness marker (only on log panel to avoid clutter)
            if args.halving and i in halving and logy:
                xh = halving[i]
                dh = D0 * np.exp(-xh / lam) * yscale if (args.fit and i in fit_params) \
                     else dose[0, i] * 0.5 * yscale
                ax.axvline(xh, color=clr, lw=1.0, ls=":", alpha=0.7)
                ax.annotate(f"T½={xh:.1f}", xy=(xh, dh),
                            xytext=(xh + (xhi-xlo)*0.03, dh * 1.5),
                            fontsize=8, color=clr,
                            arrowprops=dict(arrowstyle="-", color=clr,
                                            lw=0.7, alpha=0.6))

        # Reference line
        if ref_display is not None:
            ax.axhline(ref_display, color="red", lw=1.2, ls="--", alpha=0.8, zorder=1)
            ax.text(xlo * 1.02, ref_display * 1.15 if not logy else ref_display * 1.5,
                    args.ref_label, color="red", fontsize=8.5, va="bottom")

        ax.set_xlim(xlo, xhi)
        ax.set_ylabel(ylabel, fontsize=10)
        ax.tick_params(which="both", direction="in", top=True, right=True, labelsize=9)
        ax.grid(True, which="major", ls="-",  lw=0.4, alpha=0.4)
        ax.grid(True, which="minor", ls=":",  lw=0.3, alpha=0.25)

        if logy:
            ax.set_yscale("log")
            ax.set_xlabel(xlabel, fontsize=10)
            ax.yaxis.set_minor_locator(ticker.LogLocator(subs="all", numticks=10))
        else:
            ax.set_title("Linear scale", fontsize=9, loc="right", pad=2)

        ax.legend(fontsize=8.5, framealpha=0.88, loc="upper right",
                  ncol=2 if n_sc > 3 else 1)

    ax_log.set_title("Log scale", fontsize=9, loc="right", pad=2)

    return fig, fit_params, halving

# ---------------------------------------------------------------------------
# Console summary of fit parameters
# ---------------------------------------------------------------------------
def print_summary(meta, x, dose, args, fit_params, halving):
    n_sc = dose.shape[1]
    names = meta["scorer_names"] if meta["scorer_names"] else [f"Scorer {i}" for i in range(n_sc)]
    xdata = x[:, 0] if args.xaxis == "mm" else x[:, 1]
    xunit = args.xaxis if args.xaxis == "mm" else "g/cm²"
    print("\n── Dose at boundaries ──────────────────────────────────────────")
    hdr = f"  {'Scorer':<18}  {'D(tmin) [Gy]':>14}  {'D(tmax) [Gy]':>14}"
    print(hdr)
    for i, nm in enumerate(names):
        print(f"  {nm:<18}  {dose[0,i]:14.4e}  {dose[-1,i]:14.4e}")
    if fit_params:
        print("\n── Exponential fit  D(t) = D0 * exp(-t / λ) ───────────────────")
        print(f"  {'Scorer':<18}  {'D0 [Gy]':>14}  {f'λ [{xunit}]':>14}  {'HVT':>10}")
        for i, nm in enumerate(names):
            if i in fit_params:
                D0, lam = fit_params[i]
                hvt = lam * 0.6931
                print(f"  {nm:<18}  {D0:14.4e}  {lam:14.3f}  {hvt:10.3f} {xunit}")
    if halving:
        print(f"\n── Halving thickness (T½, dose = 50 % of first point) ───────────")
        for i, nm in enumerate(names):
            if i in halving:
                print(f"  {nm:<18}  T½ = {halving[i]:.3f} {xunit}")
    print()

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main(argv=None):
    args = parse_args(argv)

    # scipy is optional — warn if --fit requested but unavailable
    try:
        from scipy.optimize import curve_fit  # noqa: F401 (already imported above)
    except ImportError:
        if args.fit:
            print("WARNING: scipy not found — --fit disabled. "
                  "Install with: pip install scipy")
            args.fit = False

    meta, x, dose = parse_sweep(args.datfile)

    print(f"Loaded {len(x)} thickness points from: {args.datfile}")
    print(f"  Shield   : {meta['shield']}")
    print(f"  Scorers  : {meta['scorer_names']}")
    print(f"  Events/pt: {meta['events']}")
    print(f"  Thickness: {x[0,0]:.3f} – {x[-1,0]:.3f} mm  "
          f"({x[0,1]:.3f} – {x[-1,1]:.3f} g/cm²)")

    fig, fit_params, halving = make_figure(meta, x, dose, args)
    print_summary(meta, x, dose, args, fit_params, halving)

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
