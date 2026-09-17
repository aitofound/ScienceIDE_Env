#!/usr/bin/env python3
"""
plot_dose_sweep_single_panel.py
===============================
Visualise shieldSim_dose_sweep.dat — dose per primary GCR particle
as a function of shield thickness, produced by the Geant4 shieldSim
application in sweep mode (--sweep).

This version draws all dose columns on a single panel. If the input file
contains separate proton and alpha dose columns, all of them are plotted
together automatically.

Usage
-----
    python plot_dose_sweep_single_panel.py [file] [options]
"""

import sys, os, re, argparse
import numpy as np
import matplotlib
import matplotlib.pyplot as plt

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------
def parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Visualise shieldSim dose-vs-thickness sweep on a single panel.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("datfile", nargs="?", default="shieldSim_dose_sweep.dat")
    p.add_argument("--xaxis",     default="mm",  choices=["mm", "gcm2"],
                   help="X-axis: thickness in mm or areal density g/cm² (default: mm)")
    p.add_argument("--yunit",     default="Gy",  choices=["Gy", "mGy", "uGy"],
                   help="Dose unit on y-axis (default: Gy)")
    p.add_argument("--yscale",    default="linear", choices=["linear", "log"],
                   help="Y-axis scaling (default: linear)")
    p.add_argument("--ref",       type=float, default=None, metavar="Gy",
                   help="Draw a horizontal reference dose line in Gy")
    p.add_argument("--ref-label", default="Limit", dest="ref_label")
    p.add_argument("--save",      action="store_true")
    p.add_argument("--format",    default="pdf", choices=["pdf","png","svg","jpg"])
    p.add_argument("--dpi",       type=int, default=150)
    p.add_argument("--no-show",   action="store_true")
    return p.parse_args(argv)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def clean_label(raw):
    s = raw.strip()
    s = re.sub(r"\s*\[[^\]]+\]\s*$", "", s)
    s = re.sub(r"^Dose_", "", s, flags=re.I)
    s = re.sub(r"_perPrimary$", "", s, flags=re.I)
    s = s.replace("G4_", "")
    s = s.replace("_", " ")

    # Standardize common particle tags for nicer legend text
    s = re.sub(r"\bprotons?\b", "p", s, flags=re.I)
    s = re.sub(r"\bprot\b", "p", s, flags=re.I)
    s = re.sub(r"\balphas?\b", "α", s, flags=re.I)
    s = re.sub(r"\bhelium\b", "α", s, flags=re.I)
    s = re.sub(r"\bhe2\+?\b", "α", s, flags=re.I)
    s = re.sub(r"\bh\+\b", "p", s, flags=re.I)

    return s

# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------
def parse_sweep(path):
    """
    Returns
    -------
    meta : dict
    x    : np.ndarray  shape (N,2)  cols: thickness_mm, areal_density_g_cm2
    dose : np.ndarray  shape (N, n_series)  in Gy
    """
    if not os.path.isfile(path):
        sys.exit(f"ERROR: file not found: {path}")

    meta = {
        "title": "", "shield": "", "events": "",
        "emin_p": "", "emax_p": "", "emin_a": "", "emax_a": "",
        "scorer_names": [],
        "dose_column_labels": [],
    }
    rows = []

    with open(path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue

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
                cols = re.findall(r'"([^"]+)"', line)
                if len(cols) >= 3:
                    meta["dose_column_labels"] = cols[2:]
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
    x    = arr[:, :2]
    dose = arr[:, 2:]

    if not meta["dose_column_labels"]:
        n_series = dose.shape[1]
        if meta["scorer_names"] and len(meta["scorer_names"]) == n_series:
            meta["dose_column_labels"] = [f"Dose_{nm}_perPrimary [Gy]" for nm in meta["scorer_names"]]
        else:
            meta["dose_column_labels"] = [f"Dose_series_{i+1}_perPrimary [Gy]" for i in range(n_series)]

    return meta, x, dose

# ---------------------------------------------------------------------------
# Style mapping
# ---------------------------------------------------------------------------
BASE_COLOURS = {
    "water": "#1a73e8",
    "h2o":   "#1a73e8",
    "si":    "#ea4335",
    "silicon": "#ea4335",
}

def infer_colour(label, idx):
    ll = label.lower()
    for key, val in BASE_COLOURS.items():
        if key in ll:
            return val
    fallback = ["#1a73e8", "#ea4335", "#34a853", "#fb8c00", "#9c27b0", "#00acc1"]
    return fallback[idx % len(fallback)]

def infer_linestyle(label):
    ll = label.lower()
    if any(tok in ll for tok in ["alpha", " α", "α ", "he", "helium"]):
        return "--"
    if any(tok in ll for tok in ["proton", " p", "p ", "h+"]):
        return "-"
    return "-"

def infer_marker(label):
    ll = label.lower()
    if any(tok in ll for tok in ["alpha", " α", "α ", "he", "helium"]):
        return "s"
    return "o"

# ---------------------------------------------------------------------------
# Main figure
# ---------------------------------------------------------------------------
def make_figure(meta, x, dose, args):
    n_series = dose.shape[1]
    labels = [clean_label(s) for s in meta["dose_column_labels"]]

    scale = {"Gy": 1.0, "mGy": 1e3, "uGy": 1e6}[args.yunit]
    ylabel = f"Dose per primary [{args.yunit}]"

    if args.xaxis == "mm":
        xdata  = x[:, 0]
        xlabel = "Shield thickness (mm)"
    else:
        xdata  = x[:, 1]
        xlabel = r"Areal density (g cm$^{-2}$)"

    ref_display = args.ref * scale if args.ref is not None else None

    matplotlib.rcParams.update({"font.family": "sans-serif", "font.size": 10})
    fig, ax = plt.subplots(figsize=(10, 6))
    fig.subplots_adjust(left=0.11, right=0.97, top=0.88, bottom=0.12)

    st = meta["title"] or "GCR Dose vs Shield Thickness"
    if meta["shield"]:
        st += f"\nShield: {meta['shield']}"
    parts = []
    if meta["events"]:
        parts.append(f"{meta['events']} events/pt")
    if meta["emin_p"] and meta["emax_p"]:
        parts.append(f"p: [{meta['emin_p']}, {meta['emax_p']}] MeV")
    if meta["emin_a"] and meta["emax_a"]:
        parts.append(f"α: [{meta['emin_a']}, {meta['emax_a']}] MeV")
    if parts:
        st += "   |   " + "   |   ".join(parts)
    fig.suptitle(st, fontsize=11, fontweight="bold")

    xlo = xdata[0] * (0.90 if xdata[0] > 0 else 1.0)
    xhi = xdata[-1] * 1.05

    for i in range(n_series):
        d = dose[:, i] * scale
        lbl = labels[i]
        ax.plot(
            xdata, d,
            linestyle=infer_linestyle(lbl),
            marker=infer_marker(lbl),
            color=infer_colour(lbl, i),
            lw=2.0, ms=5.5,
            label=lbl,
            zorder=3
        )

    if ref_display is not None:
        ax.axhline(ref_display, color="red", lw=1.2, ls=":", alpha=0.8, zorder=1)
        ytext = ref_display * (1.08 if args.yscale == "linear" else 1.2)
        ax.text(xlo * 1.01 if xlo > 0 else xdata[0], ytext,
                args.ref_label, color="red", fontsize=9, va="bottom")

    ax.set_xlim(xlo, xhi)
    ax.set_xlabel(xlabel, fontsize=11)
    ax.set_ylabel(ylabel, fontsize=11)
    ax.set_yscale(args.yscale)
    ax.tick_params(which="both", direction="in", top=True, right=True, labelsize=10)
    ax.grid(True, which="major", ls="-", lw=0.4, alpha=0.4)
    ax.grid(True, which="minor", ls=":", lw=0.3, alpha=0.25)
    ax.legend(fontsize=9.0, framealpha=0.9, loc="best", ncol=2 if n_series > 2 else 1)

    return fig

# ---------------------------------------------------------------------------
# Console summary
# ---------------------------------------------------------------------------
def print_summary(meta, dose):
    labels = [clean_label(s) for s in meta["dose_column_labels"]]
    print("\n-- Dose at boundaries ------------------------------------------------")
    print(f"  {'Series':<24}  {'D(tmin) [Gy]':>14}  {'D(tmax) [Gy]':>14}")
    for i, nm in enumerate(labels):
        print(f"  {nm:<24}  {dose[0,i]:14.4e}  {dose[-1,i]:14.4e}")
    print()

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main(argv=None):
    args = parse_args(argv)
    meta, x, dose = parse_sweep(args.datfile)

    print(f"Loaded {len(x)} thickness points from: {args.datfile}")
    print(f"  Shield   : {meta['shield']}")
    print(f"  Series   : {[clean_label(s) for s in meta['dose_column_labels']]}")
    print(f"  Events/pt: {meta['events']}")
    print(f"  Thickness: {x[0,0]:.3f} – {x[-1,0]:.3f} mm  "
          f"({x[0,1]:.3f} – {x[-1,1]:.3f} g/cm²)")

    fig = make_figure(meta, x, dose, args)
    print_summary(meta, dose)

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
