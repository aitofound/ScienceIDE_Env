#!/usr/bin/env python3
"""
Plot one zone from a Tecplot ASCII POINT-format file.

This script was written for AMPS cutoff shell files such as:
  cutoff_3d_shells.swmf_n000000_t00000000.000s.dat

Supported Tecplot subset:
  TITLE=...
  VARIABLES="lon_deg","lat_deg","Rc_GV","Emin_MeV"
  ZONE T="alt_km=9000" I=... J=... F=POINT
  data rows...

The script is intentionally compatible with older Python 3 versions
(e.g., Python 3.6 on many HPC systems): it does not use list[str],
str | Path, dataclasses, or postponed annotations.
"""

import argparse
import re
import sys
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


class Zone(object):
    def __init__(self, title, i, j, data):
        self.title = title
        self.i = i
        self.j = j
        self.data = data


def _parse_variables(line):
    # Handles: VARIABLES="lon_deg","lat_deg","Rc_GV","Emin_MeV"
    variables = re.findall(r'"([^"]+)"', line)
    if not variables:
        # Fallback for less strict files: VARIABLES = lon lat Bx
        _, rhs = line.split("=", 1)
        variables = [v.strip().strip(",") for v in rhs.replace(",", " ").split()]
    return variables


def _parse_zone_header(line):
    title_match = re.search(r'T\s*=\s*"([^"]*)"', line, re.IGNORECASE)
    i_match = re.search(r'\bI\s*=\s*(\d+)', line, re.IGNORECASE)
    j_match = re.search(r'\bJ\s*=\s*(\d+)', line, re.IGNORECASE)

    title = title_match.group(1) if title_match else ""
    if i_match is None or j_match is None:
        raise ValueError("ZONE line does not contain both I= and J=: {}".format(line.strip()))

    return title, int(i_match.group(1)), int(j_match.group(1))


def read_tecplot_point_file(path):
    """Read a Tecplot ASCII file with POINT zones."""
    path = Path(path)
    lines = path.read_text(errors="replace").splitlines()

    variables = None
    zones = []
    k = 0

    while k < len(lines):
        line = lines[k].strip()

        if not line:
            k += 1
            continue

        if line.upper().startswith("VARIABLES"):
            variables = _parse_variables(line)
            k += 1
            continue

        if line.upper().startswith("ZONE"):
            if variables is None:
                raise ValueError("Found ZONE before VARIABLES.")

            title, ni, nj = _parse_zone_header(line)
            nrows = ni * nj
            rows = []

            k += 1
            while k < len(lines) and len(rows) < nrows:
                data_line = lines[k].strip()
                if data_line and not data_line.upper().startswith("ZONE"):
                    values = [float(x) for x in data_line.split()]
                    if len(values) != len(variables):
                        raise ValueError(
                            "Expected {} values but found {} on line {}: {}".format(
                                len(variables), len(values), k + 1, data_line
                            )
                        )
                    rows.append(values)
                k += 1

            if len(rows) != nrows:
                raise ValueError(
                    "Zone '{}' expected {} rows from I={}, J={}, but found {}.".format(
                        title, nrows, ni, nj, len(rows)
                    )
                )

            zones.append(Zone(title=title, i=ni, j=nj, data=np.asarray(rows, dtype=float)))
            continue

        k += 1

    if variables is None:
        raise ValueError("No VARIABLES line found.")
    if not zones:
        raise ValueError("No ZONE blocks found.")

    return variables, zones


def select_zone(zones, zone_spec):
    """Select zone by 0-based index, exact title, or case-insensitive title substring."""
    try:
        idx = int(zone_spec)
        if idx < 0 or idx >= len(zones):
            raise IndexError
        return idx, zones[idx]
    except ValueError:
        pass
    except IndexError:
        raise ValueError("Zone index {} is out of range: valid range is 0..{}".format(zone_spec, len(zones)-1))

    exact = [i for i, z in enumerate(zones) if z.title == zone_spec]
    if len(exact) == 1:
        i = exact[0]
        return i, zones[i]

    needle = zone_spec.lower()
    matches = [i for i, z in enumerate(zones) if needle in z.title.lower()]
    if len(matches) == 1:
        i = matches[0]
        return i, zones[i]
    if not matches:
        raise ValueError("No zone title matches '{}'. Use --list-zones to see available zones.".format(zone_spec))

    match_text = ", ".join("{}: {}".format(i, zones[i].title) for i in matches)
    raise ValueError("Zone selector '{}' is ambiguous. Matches: {}".format(zone_spec, match_text))


def reshape_zone(zone, variables, lon_name, lat_name, value_name):
    try:
        lon_idx = variables.index(lon_name)
        lat_idx = variables.index(lat_name)
        val_idx = variables.index(value_name)
    except ValueError:
        raise ValueError("Unknown variable. Available variables: {}".format(", ".join(variables)))

    # Tecplot POINT ordering for I,J zones is normally I-fastest, J-slowest.
    lon = zone.data[:, lon_idx].reshape(zone.j, zone.i)
    lat = zone.data[:, lat_idx].reshape(zone.j, zone.i)
    val = zone.data[:, val_idx].reshape(zone.j, zone.i)

    return lon, lat, val


def plot_zone(path, zone_spec, value_name, lon_name, lat_name, output, show, contour, levels, dpi, cmap=None):
    variables, zones = read_tecplot_point_file(path)
    zone_index, zone = select_zone(zones, zone_spec)
    lon, lat, val = reshape_zone(zone, variables, lon_name, lat_name, value_name)

    fig, ax = plt.subplots(figsize=(9, 4.8))

    # Preserve the historical behavior when --cmap is omitted: Matplotlib
    # chooses the colormap from its active configuration (normally viridis).
    # When a name is supplied, resolve it here so an invalid name produces a
    # clear command-line error before Matplotlib starts drawing the plot.
    plot_kwargs = {}
    if cmap is not None:
        plot_kwargs["cmap"] = plt.get_cmap(cmap)

    if contour:
        m = ax.contourf(lon, lat, val, levels=levels, **plot_kwargs)
    else:
        # shading='auto' supports both center-like and edge-like coordinates.
        m = ax.pcolormesh(lon, lat, val, shading="auto", **plot_kwargs)

    cbar = fig.colorbar(m, ax=ax)
    cbar.set_label(value_name)

    ax.set_xlabel(lon_name)
    ax.set_ylabel(lat_name)
    ax.set_title("{}; zone {}: {}".format(value_name, zone_index, zone.title))
    ax.set_xlim(float(np.nanmin(lon)), float(np.nanmax(lon)))
    ax.set_ylim(float(np.nanmin(lat)), float(np.nanmax(lat)))
    fig.tight_layout()

    out_path = Path(output) if output else None
    if out_path is not None:
        fig.savefig(str(out_path), dpi=dpi, bbox_inches="tight")
        print("Wrote {}".format(out_path))

    if show:
        plt.show()
    else:
        plt.close(fig)

    return out_path


def main():
    help_text = """
Plot one zone from an AMPS/Tecplot ASCII POINT-format file.

The script reads Tecplot files with VARIABLES and ZONE blocks, selects one zone
by index or title, reshapes the I x J POINT data, and plots one selected
variable as a longitude-latitude map. It was written for AMPS cutoff shell
outputs such as cutoff_3d_shells*.dat.
"""

    examples = """
Examples:
  List all variables and zones:
    ./plot_tecplot_zone.py -f cutoff_3d_shells.swmf_n000000_t00000000.000s.dat --list-zones

  Plot zone 0 using the default variable Rc_GV:
    ./plot_tecplot_zone.py -f cutoff_3d_shells.swmf_n000000_t00000000.000s.dat -z 0

  Plot the zone whose title contains alt_km=7000:
    ./plot_tecplot_zone.py -f cutoff_3d_shells.swmf_n000000_t00000000.000s.dat -z alt_km=7000 --var Emin_MeV

  Save a plot without opening an interactive window:
    ./plot_tecplot_zone.py -f cutoff_3d_shells.swmf_n000000_t00000000.000s.dat -z 1 --var Rc_GV -o Rc_zone1.png --no-show

  Use filled contours instead of pcolormesh:
    ./plot_tecplot_zone.py -f cutoff_3d_shells.swmf_n000000_t00000000.000s.dat -z 1 --var Rc_GV --contour --levels 40

  Select a Matplotlib colormap:
    ./plot_tecplot_zone.py -f cutoff_3d_shells.swmf_n000000_t00000000.000s.dat -z 1 --var Rc_GV --cmap plasma

  Reverse a colormap by adding the _r suffix:
    ./plot_tecplot_zone.py -f cutoff_3d_shells.swmf_n000000_t00000000.000s.dat -z 1 --var Rc_GV --cmap coolwarm_r

Common colormap names:
  viridis, plasma, inferno, magma, cividis, coolwarm, seismic, jet

Common variables in cutoff shell files:
  lon_deg     longitude coordinate
  lat_deg     latitude coordinate
  Rc_GV       cutoff rigidity in GV
  Emin_MeV    equivalent minimum proton energy in MeV
"""

    parser = argparse.ArgumentParser(
        description=help_text,
        epilog=examples,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("-f", "--file", required=True, help="Input Tecplot ASCII file.")
    parser.add_argument(
        "-z",
        "--zone",
        default="0",
        help=(
            "Zone to plot: 0-based zone index, exact zone title, or unique title substring. "
            "Example: 0 or alt_km=7000. Default: 0."
        ),
    )
    parser.add_argument("--var", default="Rc_GV", help="Variable to plot. Default: Rc_GV.")
    parser.add_argument("--lon", default="lon_deg", help="Longitude variable name. Default: lon_deg.")
    parser.add_argument("--lat", default="lat_deg", help="Latitude variable name. Default: lat_deg.")
    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help="Output image file, e.g. zone0_rc.png. If omitted, the plot is shown interactively.",
    )
    parser.add_argument("--list-zones", action="store_true", help="List zones and variables, then exit.")
    parser.add_argument("--contour", action="store_true", help="Use contourf instead of pcolormesh.")
    parser.add_argument(
        "--cmap",
        default=None,
        metavar="NAME",
        help=(
            "Matplotlib colormap name, for example viridis, plasma, inferno, "
            "coolwarm, or seismic. Add the _r suffix to reverse a map. "
            "If omitted, preserve Matplotlib's current default colormap."
        ),
    )
    parser.add_argument(
        "--levels",
        type=int,
        default=32,
        help="Number of contour levels when --contour is used. Default: 32.",
    )
    parser.add_argument("--dpi", type=int, default=200, help="Output image DPI. Default: 200.")
    parser.add_argument("--no-show", action="store_true", help="Do not open an interactive window. Useful when also using -o.")

    args = parser.parse_args()

    try:
        variables, zones = read_tecplot_point_file(args.file)

        if args.list_zones:
            print("Variables:")
            for v in variables:
                print("  {}".format(v))
            print("\nZones:")
            for i, z in enumerate(zones):
                print("  {}: {}   I={} J={} N={}".format(i, z.title, z.i, z.j, z.i*z.j))
            return 0

        show = not args.no_show and args.output is None
        plot_zone(
            path=args.file,
            zone_spec=args.zone,
            value_name=args.var,
            lon_name=args.lon,
            lat_name=args.lat,
            output=args.output,
            show=show,
            contour=args.contour,
            levels=args.levels,
            dpi=args.dpi,
            cmap=args.cmap,
        )
        return 0

    except Exception as exc:
        print("ERROR: {}".format(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
