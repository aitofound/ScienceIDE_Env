#!/usr/bin/env python3
"""
plot_4curves.py
---------------
Read two shieldSim sweep outputs:
  - shieldSim_species_proton.dat
  - shieldSim_species_alpha.dat

and generate a single-panel plot with 4 curves:
  - WATER p
  - Si p
  - WATER α
  - Si α

Usage:
    python3 plot_4curves.py

Optional arguments:
    python3 plot_4curves.py proton.dat alpha.dat output.png
"""

import sys
import re
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


def read_dat(fname):
    rows = []
    meta = {
        "title": "Dose vs Shield Thickness",
        "shield": "",
        "events": "",
        "emin_p": "",
        "emax_p": "",
        "emin_a": "",
        "emax_a": "",
        "scorers": [],
    }

    with open(fname) as f:
        for line in f:
            s = line.strip()
            if not s:
                continue

            if s.startswith("#"):
                m = re.search(r"Shield material:\s*(.+)", s, re.I)
                if m:
                    meta["shield"] = m.group(1).strip()

                m = re.search(r"Events per point:\s*(\d+)", s, re.I)
                if m:
                    meta["events"] = m.group(1).strip()

                m = re.search(r"Proton energy range:\s*\[([^,]+),\s*([^\]]+)\]", s, re.I)
                if m:
                    meta["emin_p"], meta["emax_p"] = m.group(1).strip(), m.group(2).strip()

                m = re.search(r"Alpha\s+energy range:\s*\[([^,]+),\s*([^\]]+)\]", s, re.I)
                if m:
                    meta["emin_a"], meta["emax_a"] = m.group(1).strip(), m.group(2).strip()

                m = re.search(r"Scoring volumes:\s*(.+)", s, re.I)
                if m:
                    meta["scorers"] = m.group(1).strip().split()

                continue

            if s.upper().startswith("TITLE"):
                m = re.search(r'"([^"]+)"', s)
                if m:
                    meta["title"] = m.group(1)
                continue

            if s.upper().startswith("VARIABLES") or s.upper().startswith("ZONE"):
                continue

            vals = [float(x) for x in s.split()]
            rows.append(vals)

    if not rows:
        raise RuntimeError(f"No numeric data rows found in {fname}")

    arr = np.array(rows)
    if arr.shape[1] < 4:
        raise RuntimeError(
            f"{fname} must have at least 4 columns: thickness_mm areal_density_gcm2 dose1 dose2"
        )

    x_mm = arr[:, 0]
    dose = arr[:, 2:]  # scorer columns, expected WATER and Si
    return meta, x_mm, dose


def main():
    proton_file = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("shieldSim_species_proton.dat")
    alpha_file = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("shieldSim_species_alpha.dat")
    out_png = Path(sys.argv[3]) if len(sys.argv) > 3 else Path("shieldSim_species_4curves.png")

    if not proton_file.is_file():
        raise FileNotFoundError(f"Proton sweep file not found: {proton_file}")
    if not alpha_file.is_file():
        raise FileNotFoundError(f"Alpha sweep file not found: {alpha_file}")

    meta_p, x_p, dose_p = read_dat(proton_file)
    meta_a, x_a, dose_a = read_dat(alpha_file)

    if len(x_p) != len(x_a) or np.max(np.abs(x_p - x_a)) > 1.0e-12:
        raise RuntimeError("Thickness grids do not match between proton and alpha runs.")

    if dose_p.shape[1] < 2 or dose_a.shape[1] < 2:
        raise RuntimeError(
            "Expected at least two scoring columns in each file (e.g., WATER and Si)."
        )

    scale = 1.0e3  # Gy -> mGy

    plt.figure(figsize=(10, 6))
    plt.plot(x_p, dose_p[:, 0] * scale, "o-", lw=2, ms=5.5, color="#1a73e8", label="WATER p")
    plt.plot(x_p, dose_p[:, 1] * scale, "o-", lw=2, ms=5.5, color="#ea4335", label="Si p")
    plt.plot(x_a, dose_a[:, 0] * scale, "s--", lw=2, ms=5.0, color="#1a73e8", label="WATER α")
    plt.plot(x_a, dose_a[:, 1] * scale, "s--", lw=2, ms=5.0, color="#ea4335", label="Si α")

    title = meta_p["title"] or "GCR Dose vs Shield Thickness"
    if meta_p["shield"]:
        title += f"\nShield: {meta_p['shield']}"

    parts = []
    if meta_p["events"]:
        parts.append(f"{meta_p['events']} events/pt")
    if meta_p["emin_p"] and meta_p["emax_p"]:
        parts.append(f"p: [{meta_p['emin_p']}, {meta_p['emax_p']}] MeV")
    if meta_p["emin_a"] and meta_p["emax_a"]:
        parts.append(f"α: [{meta_p['emin_a']}, {meta_p['emax_a']}] MeV")
    if parts:
        title += "   |   " + "   |   ".join(parts)

    plt.title(title, fontweight="bold")
    plt.xlabel("Shield thickness (mm)")
    plt.ylabel("Dose per primary [mGy]")
    plt.yscale("log")
    plt.grid(True, which="major", alpha=0.3)
    plt.grid(True, which="minor", alpha=0.15, linestyle=":")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_png, dpi=200)
    print(f"Saved plot: {out_png}")
    plt.show()


if __name__ == "__main__":
    main()
