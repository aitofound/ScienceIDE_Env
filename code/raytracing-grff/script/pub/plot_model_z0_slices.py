#!/usr/bin/env python
"""Plot MAS model slices at z=0 for Ne, Te, and |B|.

This script samples the MAS model on an x-y grid at fixed z=0 (all in R_sun),
then writes a 3-panel figure similar to LOS_2D_slices.png:
- log10(Ne [cm^-3])
- log10(Te [K])
- log10(|B| [G])
"""

import argparse
import re
import sys
from pathlib import Path

import astropy.units as u
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from psipy.io.mas import _read_mas
from psipy.model import MASOutput
from psipy.model.variable import Variable

try:
    import xarray as xr
except Exception:  # pragma: no cover
    xr = None

_ROOT = Path(__file__).resolve().parents[2]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))
from raytracingGRFF.coords import PHI0_EARTH_CORONA2298, cart_to_mas_lonlat

R_MIN = 0.999999
R_SURFACE = 1.02   # solar radius in R_sun
DISK_R_MAX = 1.02  # for sqrt(x^2+y^2) <= this, sample on sphere instead of z=z0
PHI0_OFFSET_DEFAULT = PHI0_EARTH_CORONA2298


def load_mas_var_filtered(model, var_name):
    """Load MAS variable, filtering files to {var}{3digits}.hdf pattern."""
    if xr is None:
        return model[var_name]

    directory = Path(model.path)
    all_files = sorted(directory.glob(f"{var_name}*"))
    pattern = re.compile(rf"^{var_name}\d{{3}}\.hdf$")
    filtered_files = [str(f) for f in all_files if f.name and pattern.match(f.name)]

    if not filtered_files:
        return model[var_name]

    data = [_read_mas(f, var_name) for f in filtered_files]
    var_data = data[0] if len(data) == 1 else xr.concat(data, dim="time")
    unit_info = model.get_unit(var_name)
    var_unit = unit_info[0] * unit_info[1]
    return Variable(var_data, var_name, var_unit, model.get_runit())


def sample_plane(model, n_pix=256, extent=1.44, z0=0.0, phi0_offset=PHI0_OFFSET_DEFAULT):
    """Sample Ne, Te, and |B| over x,y in [-extent, extent].

    For sqrt(x^2+y^2) <= 1.01 we sample on the solar surface z = sqrt(R^2 - x^2 - y^2)
    (R=1); otherwise at z=z0.
    """
    temp_var = "te" if "te" in model.variables else "t"

    rho_var = load_mas_var_filtered(model, "rho")
    te_var = load_mas_var_filtered(model, temp_var)
    br_var = load_mas_var_filtered(model, "br")
    bt_var = load_mas_var_filtered(model, "bt")
    bp_var = load_mas_var_filtered(model, "bp")

    x = np.linspace(-extent, extent, n_pix)
    y = np.linspace(-extent, extent, n_pix)
    xx, yy = np.meshgrid(x, y, indexing="xy")
    zz = np.full_like(xx, z0)
    # Inside/near disk: sample on solar surface z = sqrt(R^2 - x^2 - y^2) instead of z=z0
    r_xy = np.sqrt(xx**2 + yy**2)
    on_disk = r_xy <= DISK_R_MAX
    z_surf = np.sqrt(np.maximum(R_SURFACE**2 - xx**2 - yy**2, 0.0))
    zz[on_disk] = z_surf[on_disk]

    lon_deg, lat_deg, r = cart_to_mas_lonlat(xx, yy, zz, phi0_offset=phi0_offset)
    valid = np.isfinite(r) & (r >= R_MIN)

    r_arr = (r[valid] * u.R_sun)
    lon_arr = (lon_deg[valid] * u.deg)
    lat_arr = (lat_deg[valid] * u.deg)

    ne = np.full(xx.shape, np.nan, dtype=float)
    te = np.full(xx.shape, np.nan, dtype=float)
    bmag = np.full(xx.shape, np.nan, dtype=float)

    ne_s = rho_var.sample_at_coords(lon_arr, lat_arr, r_arr)
    te_s = te_var.sample_at_coords(lon_arr, lat_arr, r_arr)
    br_s = br_var.sample_at_coords(lon_arr, lat_arr, r_arr)
    bt_s = bt_var.sample_at_coords(lon_arr, lat_arr, r_arr)
    bp_s = bp_var.sample_at_coords(lon_arr, lat_arr, r_arr)

    ne[valid] = np.asarray(ne_s.to(u.cm**-3).value)
    te[valid] = np.asarray(te_s.to(u.K).value)
    br_g = np.asarray(br_s.to(u.G).value)
    bt_g = np.asarray(bt_s.to(u.G).value)
    bp_g = np.asarray(bp_s.to(u.G).value)
    bmag[valid] = np.sqrt(br_g**2 + bt_g**2 + bp_g**2)

    return x, y, ne, te, bmag


def _safe_log10(a):
    out = np.full_like(a, np.nan, dtype=float)
    m = np.isfinite(a) & (a > 0.0)
    out[m] = np.log10(a[m])
    return out


def plot_slices(x, y, ne, te, bmag, z0, out_path):
    """Save Ne/Te/B z=0 plane slices in a 1x3 panel figure."""
    extent = [x[0], x[-1], y[0], y[-1]]

    ne_log = _safe_log10(ne)
    te_log = _safe_log10(te)
    b_log = _safe_log10(bmag)

    fig, axes = plt.subplots(1, 3, figsize=(10, 5))

    vmax = np.nanpercentile(ne_log, 99.9)
    vmin = np.nanpercentile(ne_log, 1)

    print(f"N_e: vmin={vmin}, vmax={vmax}")
    im1 = axes[0].imshow(ne_log, origin="lower", extent=extent, aspect="equal", cmap="viridis", vmin=vmin, vmax=vmax)
    axes[0].set_xlabel("x ($R_\odot$)")
    axes[0].set_ylabel("y ($R_\odot$)")
    axes[0].set_title(f"$N_e$ at z={z0:.2f} $R_\odot$")
    plt.colorbar(im1, ax=axes[0], label="$N_e$ ($cm^{-3}$) [log10]", location="top", orientation="horizontal", pad=0.08)
    axes[0].text(0.02, 0.98, "(a)", transform=axes[0].transAxes, va="top", ha="left", fontsize=12, fontweight="bold", color="white")

    vmax = np.nanpercentile(te_log, 99.9)
    vmin = np.nanpercentile(te_log, 0.1)
    im2 = axes[1].imshow(te_log, origin="lower", extent=extent, aspect="equal", cmap="plasma", vmin=vmin, vmax=vmax)
    axes[1].set_xlabel("x ($R_\odot$)")
    #axes[1].set_ylabel("y ($R_\odot$)")
    axes[1].set_title(f"$T_e$ at z={z0:.2f} $R_\odot$")
    plt.colorbar(im2, ax=axes[1], label="$T_e$ (K) [log10]", location="top", orientation="horizontal", pad=0.08)
    axes[1].text(0.02, 0.98, "(b)", transform=axes[1].transAxes, va="top", ha="left", fontsize=12, fontweight="bold", color="white")

    vmax = np.nanmax(b_log)
    vmin = np.nanmin(b_log)
    im3 = axes[2].imshow(b_log, origin="lower", extent=extent, aspect="equal", cmap="hot", vmin=vmin, vmax=vmax)
    axes[2].set_xlabel("x ($R_\odot$)")
    #axes[2].set_ylabel("y ($R_\odot$)")
    axes[2].set_title(f"|B| at z={z0:.2f} $R_\odot$")
    plt.colorbar(im3, ax=axes[2], label="|B| (G) [log10]", location="top", orientation="horizontal", pad=0.08)
    axes[2].text(0.02, 0.98, "(c)", transform=axes[2].transAxes, va="top", ha="left", fontsize=12, fontweight="bold", color="white")

    plt.tight_layout()
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description="Plot MAS Ne/Te/|B| slices on z=0 plane")
    parser.add_argument("-m", "--model-path", default="../../corona2298", help="Path to MAS model directory")
    parser.add_argument("-n", "--n-pix", type=int, default=256, help="Grid size in x and y")
    parser.add_argument("-f", "--extent", type=float, default=2.5,
                        help="Half-width in R_sun for x,y in [-extent, extent]")
    parser.add_argument("--z", type=float, default=0.0, help="Slice z location in R_sun")
    parser.add_argument("--phi0-offset", type=float, default=PHI0_OFFSET_DEFAULT,
                        help=f"Carrington longitude at disk center (deg, default: {PHI0_OFFSET_DEFAULT})")
    parser.add_argument("-o", "--out", default="LOS_2D_slices_z0.pdf", help="Output figure path")
    args = parser.parse_args()

    model = MASOutput(args.model_path)

    required = {"rho", "br", "bt", "bp"}
    missing = sorted(required - set(model.variables))
    if missing:
        raise ValueError(f"Missing required MAS variables: {missing}")
    if not ({"te", "t"} & set(model.variables)):
        raise ValueError("No temperature variable found; expected 'te' or 't'.")

    x, y, ne, te, bmag = sample_plane(
        model=model,
        n_pix=args.n_pix,
        extent=args.extent,
        z0=args.z,
        phi0_offset=args.phi0_offset,
    )
    plot_slices(x, y, ne, te, bmag, z0=args.z, out_path=args.out)
    print(f"Saved z={args.z:.3f} R_sun slices to {args.out}")


if __name__ == "__main__":
    main()
