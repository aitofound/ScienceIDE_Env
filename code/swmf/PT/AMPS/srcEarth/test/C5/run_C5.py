#!/usr/bin/env python3
"""
C5 — Dipole mesh-interpolation convergence validation test.

Run from the directory that contains the AMPS executable:

    python srcEarth/test/C5/run_C5.py -np 4 -nt 16

C5 validates the Mode3D mesh-backed magnetic-field path.  It runs the same
centered aligned dipole cutoff problem with -mode3d-field-eval MESH while
sweeping a small sequence of user-controlled Mode3D mesh-resolution profiles.
The numerical cutoff is compared with the independent analytical vertical
Stoermer cutoff.  An optional analytic baseline run is performed first to verify
that the pusher/cutoff-search path is sound before testing the mesh path.

This script is intentionally self-contained and avoids dependencies other than
Python's standard library.  Matplotlib is used only if available for an optional
summary plot.
"""

import argparse
import csv
import json
import math
import os
import re
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

TEST_ID = "C5"
TEST_NAME = "Dipole mesh-interpolation convergence"

RE_KM = 6371.2
STORMER_R0_GV = 0.299792458 * 0.25 * 3.12e-5 * (RE_KM * 1000.0)
TARGET_ALT_KM = 9000.0
TARGET_LATS_DEG = (-60.0, -30.0, 0.0, 30.0, 60.0)
TARGET_LONS_DEG = tuple(float(i) for i in range(0, 360, 30))

DEFAULT_RES_EARTH = (0.20, 0.10, 0.05)
DEFAULT_RES_BOUNDARY = (1.00, 0.75, 0.50)


class ShellRow(object):
    """One parsed Tecplot shell row."""

    def __init__(self, alt_km, lon_deg, lat_deg, rc_num_gv, rc_code_stormer_gv, rel_err_code):
        self.alt_km = alt_km
        self.lon_deg = lon_deg
        self.lat_deg = lat_deg
        self.rc_num_gv = rc_num_gv
        self.rc_code_stormer_gv = rc_code_stormer_gv
        self.rel_err_code = rel_err_code


class MeshCase(object):
    """One Mode3D mesh-resolution configuration tested by C5."""

    def __init__(self, index, res_earth_re, res_boundary_re, coarsening, exponent, r_boundary_re):
        self.index = index
        self.res_earth_re = res_earth_re
        self.res_boundary_re = res_boundary_re
        self.coarsening = coarsening.upper()
        self.exponent = exponent
        self.r_boundary_re = r_boundary_re

    @property
    def label(self):
        return "mesh_%02d_re%.5g_rb%.5g_%s" % (
            self.index, self.res_earth_re, self.res_boundary_re, self.coarsening.lower()
        )

    def to_dict(self):
        return {
            "index": self.index,
            "label": self.label,
            "res_earth_re": self.res_earth_re,
            "res_boundary_re": self.res_boundary_re,
            "coarsening": self.coarsening,
            "exponent": self.exponent,
            "r_boundary_re": self.r_boundary_re,
        }


def preprocess_negative_option_values(argv):
    """Allow '--lats -60,-30,...' as well as '--lats=-60,-30,...'.

    argparse treats a token beginning with '-' as a new option, so comma lists
    that start with a negative number need normalization before parsing.
    """
    options = {"--lats", "--lons"}
    out = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in options and i + 1 < len(argv):
            b = argv[i + 1]
            if b.startswith("-") and not b.startswith("--"):
                out.append(a + "=" + b)
                i += 2
                continue
        out.append(a)
        i += 1
    return out


def parse_float_list(text, name):
    vals = []
    for item in str(text).split(','):
        item = item.strip()
        if not item:
            continue
        try:
            vals.append(float(item))
        except ValueError:
            raise SystemExit("Could not parse %s value '%s' in '%s'" % (name, item, text))
    if not vals:
        raise SystemExit("%s list is empty" % name)
    return vals


def stormer_vertical_gv(lat_deg, alt_km):
    """Independent analytical vertical Stoermer cutoff in GV."""
    r_re = (RE_KM + alt_km) / RE_KM
    c4 = math.cos(math.radians(lat_deg)) ** 4
    return STORMER_R0_GV * c4 / (r_re * r_re)


def write_reference_csv(path, lats):
    """Write the analytical C5 reference table used by the harness."""
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["alt_km", "lat_deg", "Rc_stormer_GV", "purpose"])
        for lat in lats:
            w.writerow([
                "%.1f" % TARGET_ALT_KM,
                "%.1f" % lat,
                "%.9e" % stormer_vertical_gv(lat, TARGET_ALT_KM),
                "C5 analytical reference for mesh-interpolation convergence",
            ])


def _parse_variables(line):
    names = re.findall(r'"([^"]+)"', line)
    return [n.strip().lower() for n in names]


def _pick_column(variables, candidates, fallback):
    if variables:
        norm = []
        for v in variables:
            norm.append(v.lower().replace(" ", "_").replace("-", "_"))
        for cand in candidates:
            c = cand.lower().replace(" ", "_").replace("-", "_")
            for i, v in enumerate(norm):
                if v == c or c in v:
                    return i
    return fallback


def parse_tecplot_shell_output(path):
    """Parse Mode3D shell cutoff output written by AMPS."""
    rows = []
    current_alt = None
    variables = None
    zone_alt_patterns = [
        re.compile(r'alt[_\s]*km\s*=\s*([0-9eE+\-.]+)', re.IGNORECASE),
        re.compile(r'alt\s*=\s*([0-9eE+\-.]+)\s*km', re.IGNORECASE),
        re.compile(r'altitude\s*=\s*([0-9eE+\-.]+)', re.IGNORECASE),
    ]

    with path.open("r", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            upper = line.upper()
            if upper.startswith("TITLE"):
                continue
            if upper.startswith("VARIABLES"):
                variables = _parse_variables(line)
                continue
            if upper.startswith("ZONE"):
                for pat in zone_alt_patterns:
                    m = pat.search(line)
                    if m:
                        current_alt = float(m.group(1))
                        break
                continue
            if current_alt is None:
                continue

            parts = line.split()
            if len(parts) < 3:
                continue
            try:
                lon_col = _pick_column(variables, ["lon_deg", "longitude", "lon"], 0)
                lat_col = _pick_column(variables, ["lat_deg", "latitude", "lat"], 1)
                rc_col = _pick_column(
                    variables,
                    ["rc_num_gv", "rc_gv", "cutoff_gv", "cutoff_rigidity_gv", "rigidity_gv"],
                    5 if len(parts) > 5 else 2,
                )
                rc_ref_col = _pick_column(
                    variables,
                    ["rc_vert_gv", "rc_stormer_gv", "rc_analytic_gv", "stormer_gv"],
                    6 if len(parts) > 6 else -1,
                )
                rel_col = _pick_column(variables, ["rel_err", "relative_error"], 7 if len(parts) > 7 else -1)
                lon = float(parts[lon_col])
                lat = float(parts[lat_col])
                rc_num = float(parts[rc_col])
                rc_ref = float(parts[rc_ref_col]) if rc_ref_col >= 0 and rc_ref_col < len(parts) else None
                rel = float(parts[rel_col]) if rel_col >= 0 and rel_col < len(parts) else None
            except (ValueError, IndexError):
                continue
            rows.append(ShellRow(current_alt, lon, lat, rc_num, rc_ref, rel))

    if not rows:
        raise RuntimeError("No data rows were parsed from %s" % path)
    return rows


def _nearest_key(value, targets, tol=1.0e-6):
    for t in targets:
        if abs(value - t) <= tol:
            return t
    return None


def _rel_denominator(value):
    return max(abs(value), 1.0e-30)


def proton_rigidity_from_kinetic_mev(T_mev, charge_abs=1.0, mass_mev=938.2720813):
    """Approximate proton rigidity in GV for kinetic energy T_mev.

    This is used only as a diagnostic floor check.  C5 uses CUTOFF_EMIN=1 MeV
    in its template, for which the proton rigidity is about 4.3e-2 GV.  When a
    mesh-backed cutoff profile collapses to this value at all latitudes, the
    test is not observing mesh convergence; it is seeing minimum-energy access.
    """
    if T_mev <= 0.0 or charge_abs <= 0.0:
        return 0.0
    p_mev_c = math.sqrt(max(0.0, T_mev * (T_mev + 2.0 * mass_mev)))
    return p_mev_c / (1000.0 * charge_abs)


def summarize_case(rows, case_kind, case_label, mesh_case, lats, lons):
    """Summarize one AMPS run by latitude and aggregate metrics."""
    messages = []
    lat_summary = []
    selected = []
    for r in rows:
        if abs(r.alt_km - TARGET_ALT_KM) > 1.0e-6:
            continue
        lat_key = _nearest_key(r.lat_deg, lats)
        lon_key = _nearest_key(r.lon_deg % 360.0, lons)
        if lat_key is not None and lon_key is not None:
            selected.append((lon_key, lat_key, r))

    by_lat = {}
    for lon, lat, row in selected:
        by_lat.setdefault(lat, []).append(row)

    abs_rel_values = []
    mid_abs_rel_values = []
    high_abs_rel_values = []
    lon_spread_values = []
    mean_rc_values = []
    mid_mean_rc_values = []
    high_mean_rc_values = []

    for lat in lats:
        items = by_lat.get(lat, [])
        ref = stormer_vertical_gv(lat, TARGET_ALT_KM)
        if len(items) != len(lons):
            messages.append("Expected %d longitudes at lat=%g deg, found %d" % (len(lons), lat, len(items)))
            continue
        rc_values = [x.rc_num_gv for x in items]
        mean_rc = sum(rc_values) / len(rc_values)
        min_rc = min(rc_values)
        max_rc = max(rc_values)
        rel_mean = (mean_rc - ref) / _rel_denominator(ref)
        rel_max_abs = max(abs((x - ref) / _rel_denominator(ref)) for x in rc_values)
        lon_spread = (max_rc - min_rc) / _rel_denominator(ref)
        code_refs = [x.rc_code_stormer_gv for x in items if x.rc_code_stormer_gv is not None]
        if code_refs:
            code_ref_mean = sum(code_refs) / len(code_refs)
            code_ref_rel = abs(code_ref_mean - ref) / _rel_denominator(ref)
        else:
            code_ref_mean = float("nan")
            code_ref_rel = float("nan")

        abs_rel_values.append(abs(rel_mean))
        lon_spread_values.append(lon_spread)
        mean_rc_values.append(mean_rc)
        if abs(lat) <= 30.0:
            mid_abs_rel_values.append(abs(rel_mean))
            mid_mean_rc_values.append(mean_rc)
        else:
            high_abs_rel_values.append(abs(rel_mean))
            high_mean_rc_values.append(mean_rc)

        row = {
            "case_kind": case_kind,
            "case_label": case_label,
            "mesh_index": mesh_case.index if mesh_case is not None else -1,
            "res_earth_re": mesh_case.res_earth_re if mesh_case is not None else float("nan"),
            "res_boundary_re": mesh_case.res_boundary_re if mesh_case is not None else float("nan"),
            "coarsening": mesh_case.coarsening if mesh_case is not None else "ANALYTIC",
            "exponent": mesh_case.exponent if mesh_case is not None else float("nan"),
            "r_boundary_re": mesh_case.r_boundary_re if mesh_case is not None else float("nan"),
            "alt_km": TARGET_ALT_KM,
            "lat_deg": lat,
            "n_lon": len(items),
            "Rc_reference_GV": ref,
            "Rc_code_reference_mean_GV": code_ref_mean,
            "Rc_num_mean_GV": mean_rc,
            "Rc_num_min_GV": min_rc,
            "Rc_num_max_GV": max_rc,
            "rel_err_mean": rel_mean,
            "rel_err_max_abs": rel_max_abs,
            "lon_spread_over_ref": lon_spread,
            "code_reference_rel_err": code_ref_rel,
        }
        lat_summary.append(row)

    def rms(vals):
        if not vals:
            return float("nan")
        return math.sqrt(sum(v * v for v in vals) / len(vals))

    aggregate = {
        "case_kind": case_kind,
        "case_label": case_label,
        "mesh_index": mesh_case.index if mesh_case is not None else -1,
        "res_earth_re": mesh_case.res_earth_re if mesh_case is not None else float("nan"),
        "res_boundary_re": mesh_case.res_boundary_re if mesh_case is not None else float("nan"),
        "coarsening": mesh_case.coarsening if mesh_case is not None else "ANALYTIC",
        "exponent": mesh_case.exponent if mesh_case is not None else float("nan"),
        "r_boundary_re": mesh_case.r_boundary_re if mesh_case is not None else float("nan"),
        "n_latitudes": len(lat_summary),
        "rms_rel_mean_all": rms(abs_rel_values),
        "rms_rel_mean_mid": rms(mid_abs_rel_values),
        "rms_rel_mean_high": rms(high_abs_rel_values),
        "max_abs_rel_mean_all": max(abs_rel_values) if abs_rel_values else float("nan"),
        "max_abs_rel_mean_mid": max(mid_abs_rel_values) if mid_abs_rel_values else float("nan"),
        "max_abs_rel_mean_high": max(high_abs_rel_values) if high_abs_rel_values else float("nan"),
        "max_lon_spread_over_ref": max(lon_spread_values) if lon_spread_values else float("nan"),
        "mean_Rc_GV_all": (sum(mean_rc_values) / len(mean_rc_values)) if mean_rc_values else float("nan"),
        "min_mean_Rc_GV_all": min(mean_rc_values) if mean_rc_values else float("nan"),
        "max_mean_Rc_GV_all": max(mean_rc_values) if mean_rc_values else float("nan"),
        "mean_Rc_GV_mid": (sum(mid_mean_rc_values) / len(mid_mean_rc_values)) if mid_mean_rc_values else float("nan"),
        "mean_Rc_GV_high": (sum(high_mean_rc_values) / len(high_mean_rc_values)) if high_mean_rc_values else float("nan"),
    }
    return lat_summary, aggregate, messages


def write_csv_dicts(rows, path):
    if not rows:
        return
    keys = list(rows[0].keys())
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for row in rows:
            w.writerow(row)


def run_command(cmd, cwd, log_path, dry_run=False):
    """Run AMPS and tee stdout/stderr into a log file."""
    with log_path.open("w") as log:
        log.write("Command:\n  " + " ".join(cmd) + "\n\n")
        if dry_run:
            log.write("DRY RUN: command was not executed.\n")
            print("DRY RUN:", " ".join(cmd))
            return 0
        log.flush()
        proc = subprocess.Popen(cmd, cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            log.write(line)
        return proc.wait()


def render_input_template(template_path, output_path, args, case_note):
    """Copy parser-compatible input and append commented provenance."""
    text = template_path.read_text()
    text += (
        "\n! ── C5 harness run-time settings, supplied through CLI ─────────\n"
        "! C5_CASE                 %s\n"
        "! C5_NT                   %d\n"
        "! C5_MPI_SCHEDULER        %s\n"
        "! C5_MPI_DYNAMIC_CHUNK    %d\n"
        "! C5_CUTOFF_SEARCH        UPPER_SCAN\n"
        "! C5_CUTOFF_SCAN_N        %d\n"
        "! C5_MODE3D_FIELD_EVAL    MESH or ANALYTIC, supplied by command line\n"
    ) % (case_note, args.nt, args.scheduler, args.dynamic_chunk, args.upper_scan_n)
    output_path.write_text(text)


def find_output_file(workdir, user_output=None):
    if user_output:
        p = Path(user_output)
        if not p.is_absolute():
            p = workdir / p
        return p
    for name in ["cutoff_3d_shells_dipole_compare.dat", "cutoff_3d_shells.dat"]:
        p = workdir / name
        if p.exists():
            return p
    matches = sorted(workdir.glob("cutoff_*shell*.dat"))
    if matches:
        return matches[0]
    return workdir / "cutoff_3d_shells_dipole_compare.dat"


def make_plot(lat_summary, aggregate_summary, path):
    try:
        import matplotlib.pyplot as plt
    except Exception:
        return

    mesh_rows = [r for r in lat_summary if r["case_kind"] == "MESH"]
    if not mesh_rows:
        return
    by_case = {}
    for row in mesh_rows:
        by_case.setdefault(row["case_label"], []).append(row)

    plt.figure(figsize=(8.0, 5.0))
    for label, items in sorted(by_case.items()):
        items = sorted(items, key=lambda x: x["lat_deg"])
        lat = [x["lat_deg"] for x in items]
        err = [100.0 * x["rel_err_mean"] for x in items]
        plt.plot(lat, err, marker="o", linestyle="-", label=label)
    plt.axhline(0.0, linewidth=0.8)
    plt.xlabel("Magnetic latitude (deg)")
    plt.ylabel("Mean relative error vs. Størmer (%)")
    plt.title("C5: mesh-backed dipole cutoff convergence")
    plt.grid(True, alpha=0.3)
    plt.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(path, dpi=160)
    plt.close()


def make_rc_plot(lat_summary, path):
    """Plot Rc itself, not only relative error.

    A relative-error plot can hide the most important C5 failure mode: all mesh
    cases collapsing to the minimum scanned rigidity.  The Rc plot makes that
    visible by showing the Størmer reference and the CUTOFF_EMIN floor.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception:
        return

    mesh_rows = [r for r in lat_summary if r["case_kind"] == "MESH"]
    if not mesh_rows:
        return
    by_case = {}
    for row in mesh_rows:
        by_case.setdefault(row["case_label"], []).append(row)

    lats = sorted(set(float(r["lat_deg"]) for r in mesh_rows))
    ref = [stormer_vertical_gv(lat, TARGET_ALT_KM) for lat in lats]
    floor = proton_rigidity_from_kinetic_mev(1.0)

    plt.figure(figsize=(8.0, 5.0))
    plt.plot(lats, ref, marker="s", linestyle="--", label="Størmer reference")
    plt.axhline(floor, linewidth=0.8, linestyle=":", label="1 MeV proton floor")
    for label, items in sorted(by_case.items()):
        items = sorted(items, key=lambda x: x["lat_deg"])
        lat = [x["lat_deg"] for x in items]
        rc = [x["Rc_num_mean_GV"] for x in items]
        plt.plot(lat, rc, marker="o", linestyle="-", label=label)
    plt.xlabel("Magnetic latitude (deg)")
    plt.ylabel("Mean cutoff rigidity Rc (GV)")
    plt.title("C5: mesh-backed dipole cutoff rigidity")
    plt.grid(True, alpha=0.3)
    plt.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(path, dpi=160)
    plt.close()


def make_mesh_cases(args):
    earth = parse_float_list(args.res_earth_series, "--res-earth-series")
    boundary = parse_float_list(args.res_boundary_series, "--res-boundary-series")
    if len(boundary) == 1 and len(earth) > 1:
        boundary = boundary * len(earth)
    if len(earth) != len(boundary):
        raise SystemExit("--res-earth-series and --res-boundary-series must have the same length, or boundary must contain one value")
    cases = []
    for i, (a, b) in enumerate(zip(earth, boundary), start=1):
        if a <= 0.0 or b <= 0.0:
            raise SystemExit("mesh resolutions must be positive")
        cases.append(MeshCase(i, a, b, args.coarsening, args.exponent, args.r_boundary_re))
    return cases


def build_amps_command(args, amps_path, field_eval, mesh_case=None):
    cmd = [
        args.mpirun,
        "-np", str(args.np),
        str(amps_path),
        "-mode", "3d",
        "-i", "AMPS_PARAM_C5.in",
    ]
    if not args.no_cutoff_search_cli:
        cmd += ["-cutoff-search", "UPPER_SCAN", "-cutoff-upper-scan-n", str(args.upper_scan_n)]
    cmd += [
        "-mode3d-field-eval", field_eval,
        "-mode3d-parallel", "THREADS",
        "-mode3d-threads", str(args.nt),
        "-mode3d-mpi-scheduler", args.scheduler,
        "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
    ]
    if field_eval.upper() == "MESH" and mesh_case is not None and not args.use_legacy_mesh_resolution:
        cmd += [
            "-mode3d-mesh-res-earth-re", str(mesh_case.res_earth_re),
            "-mode3d-mesh-res-boundary-re", str(mesh_case.res_boundary_re),
            "-mode3d-mesh-coarsening", mesh_case.coarsening,
            "-mode3d-mesh-exponent", str(mesh_case.exponent),
            "-mode3d-mesh-r-boundary-re", str(mesh_case.r_boundary_re),
        ]
    return cmd


def parse_args(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    argv = preprocess_negative_option_values(argv)
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        C5: Dipole mesh-interpolation convergence

        C5 is the Mode3D MESH counterpart to the C1 analytical dipole benchmark.
        It runs a centered aligned dipole with vertical arrival directions at a
        9000 km shell.  The trajectory field is evaluated from the Mode3D AMR
        mesh, and the resulting cutoff profile is compared with the analytical
        vertical Stoermer cutoff.  A sequence of mesh-resolution profiles is
        tested to verify that mesh-backed results approach the analytical
        reference as the mesh is refined.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/C5/run_C5.py -np 4 -nt 16
          python srcEarth/test/C5/run_C5.py --res-earth-series 0.2,0.1,0.05 --res-boundary-series 1.0,0.75,0.5
          python srcEarth/test/C5/run_C5.py --coarsening LINEAR --res-earth-series 0.1,0.05 --res-boundary-series 0.75,0.5
          python srcEarth/test/C5/run_C5.py --use-legacy-mesh-resolution
          python srcEarth/test/C5/run_C5.py --skip-run --workdir test_output/C5_mode3d_mesh
        """)
    )
    parser.add_argument("-np", type=int, default=4, help="number of MPI processes passed to mpirun; default: 4")
    parser.add_argument("-nt", type=int, default=16, help="number of threads per MPI rank; default: 16")
    parser.add_argument("--scheduler", default="DYNAMIC", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], help="Mode3D MPI scheduler; default: DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0, help="dynamic MPI chunk size; 0 means AMPS auto heuristic; default: 0")
    parser.add_argument("--upper-scan-n", type=int, default=80, help="number of samples for UPPER_SCAN; default: 80")
    parser.add_argument("--no-cutoff-search-cli", action="store_true", help="do not pass -cutoff-search/-cutoff-upper-scan-n; useful for older CLI checkouts")

    parser.add_argument("--res-earth-series", default=",".join(str(x) for x in DEFAULT_RES_EARTH), help="comma list of requested AMR resolution near Earth, in Re; default: %(default)s")
    parser.add_argument("--res-boundary-series", default=",".join(str(x) for x in DEFAULT_RES_BOUNDARY), help="comma list of requested AMR resolution at boundary, in Re; default: %(default)s")
    parser.add_argument("--coarsening", default="LOG", choices=["LINEAR", "LOG", "EXPONENTIAL", "GEOMETRIC", "POWER", "CONSTANT"], help="radial mesh coarsening law; default: LOG")
    parser.add_argument("--exponent", type=float, default=1.5, help="shape parameter used by POWER coarsening; default: 1.5")
    parser.add_argument("--r-boundary-re", type=float, default=35.0, help="radius used by the mesh-resolution profile, in Re; default: 35")
    parser.add_argument("--use-legacy-mesh-resolution", action="store_true", help="do not pass new mesh-resolution CLI controls; test the hard-coded legacy mesh profile")

    parser.add_argument("--lats", default=",".join(str(x).rstrip('0').rstrip('.') for x in TARGET_LATS_DEG), help="comma list of target latitudes to summarize; default: %(default)s")
    parser.add_argument("--lons", default=",".join(str(int(x)) for x in TARGET_LONS_DEG), help="comma list of target longitudes to summarize; default: 0,30,...,330")

    parser.add_argument("--fine-mid-tol", type=float, default=0.05, help="finest-mesh RMS/mean relative-error tolerance for |lat|<=30 deg; default: 0.05")
    parser.add_argument("--fine-high-tol", type=float, default=0.25, help="finest-mesh RMS/mean relative-error tolerance for |lat|>=60 deg; default: 0.25")
    parser.add_argument("--fine-lon-spread-tol", type=float, default=0.10, help="finest-mesh max longitude-spread/ref tolerance; default: 0.10")
    parser.add_argument("--analytic-mid-tol", type=float, default=0.02, help="analytic-baseline mid-latitude tolerance; default: 0.02")
    parser.add_argument("--analytic-high-tol", type=float, default=0.15, help="analytic-baseline high-latitude tolerance; default: 0.15")
    parser.add_argument("--min-improvement", type=float, default=0.05, help="required fractional reduction in mid-latitude RMS from coarsest to finest mesh; default: 0.05")
    parser.add_argument("--no-convergence-gate", action="store_true", help="do not fail if coarsest-to-finest improvement is absent; still report it")
    parser.add_argument("--no-analytic-baseline", action="store_true", help="skip the ANALYTIC baseline run")

    parser.add_argument("--amps", default="./amps", help="AMPS executable path, relative to the launch directory; default: ./amps")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher executable; default: mpirun")
    parser.add_argument("--workdir", default="test_output/C5_mode3d_mesh", help="top-level test work directory; default: %(default)s")
    parser.add_argument("--output-file", default=None, help="explicit AMPS cutoff shell output file name inside each case directory; default: auto-detect")
    parser.add_argument("--skip-run", action="store_true", help="do not run AMPS; analyze existing outputs in --workdir")
    parser.add_argument("--keep", action="store_true", help="keep existing work directory instead of replacing it")
    parser.add_argument("--dry-run", action="store_true", help="write inputs and logs with commands, but do not execute AMPS")
    return parser.parse_args(argv)


def validate_args(args):
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.upper_scan_n < 2:
        raise SystemExit("--upper-scan-n must be >= 2")
    if args.exponent <= 0.0:
        raise SystemExit("--exponent must be > 0")
    if args.r_boundary_re <= 1.0:
        raise SystemExit("--r-boundary-re must be > 1")
    for name in ["fine_mid_tol", "fine_high_tol", "fine_lon_spread_tol", "analytic_mid_tol", "analytic_high_tol"]:
        if getattr(args, name) <= 0.0:
            raise SystemExit("--%s must be > 0" % name.replace('_', '-'))
    if args.min_improvement < 0.0:
        raise SystemExit("--min-improvement must be >= 0")


def main(argv=None):
    args = parse_args(argv)
    validate_args(args)

    lats = parse_float_list(args.lats, "--lats")
    lons = parse_float_list(args.lons, "--lons")
    mesh_cases = make_mesh_cases(args)

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template_input = script_dir / "AMPS_PARAM_C5_mode3d.in"
    if not template_input.exists():
        raise SystemExit("Missing input template: %s" % template_input)

    workdir = (launch_dir / args.workdir).resolve()
    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(workdir)
        workdir.mkdir(parents=True, exist_ok=True)
        write_reference_csv(workdir / "reference_C5_stormer.csv", lats)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)

    amps_path = Path(args.amps)
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()

    lat_summary_all = []
    aggregate_summary = []
    messages = []
    run_records = []

    run_specs = []
    if not args.no_analytic_baseline:
        run_specs.append(("ANALYTIC", "analytic_baseline", None))
    for case in mesh_cases:
        run_specs.append(("MESH", case.label, case))

    for field_eval, label, mesh_case in run_specs:
        case_dir = workdir / label
        if not args.skip_run:
            if case_dir.exists() and not args.keep:
                shutil.rmtree(case_dir)
            case_dir.mkdir(parents=True, exist_ok=True)
            render_input_template(template_input, case_dir / "AMPS_PARAM_C5.in", args, label)
        elif not case_dir.exists():
            raise SystemExit("Missing case directory for --skip-run: %s" % case_dir)

        log_file = case_dir / "C5_amps.log"
        cmd = build_amps_command(args, amps_path, field_eval, mesh_case)
        rc = 0
        if not args.skip_run:
            print("\nRunning C5 case %s in %s" % (label, case_dir))
            print(" ".join(cmd))
            rc = run_command(cmd, cwd=case_dir, log_path=log_file, dry_run=args.dry_run)
            if rc != 0:
                messages.append("AMPS failed for case %s with exit code %d; see %s" % (label, rc, log_file))
                run_records.append({"case_label": label, "field_eval": field_eval, "return_code": rc, "log_file": str(log_file), "parsed": False})
                continue

        if args.dry_run:
            run_records.append({"case_label": label, "field_eval": field_eval, "return_code": rc, "log_file": str(log_file), "parsed": False})
            continue

        output_file = find_output_file(case_dir, args.output_file)
        if not output_file.exists():
            messages.append("Expected AMPS output not found for case %s: %s" % (label, output_file))
            run_records.append({"case_label": label, "field_eval": field_eval, "return_code": rc, "log_file": str(log_file), "parsed": False})
            continue

        try:
            rows = parse_tecplot_shell_output(output_file)
            lat_rows, agg, case_messages = summarize_case(rows, field_eval, label, mesh_case, lats, lons)
        except Exception as exc:
            messages.append("Could not parse/summarize case %s: %s" % (label, exc))
            run_records.append({"case_label": label, "field_eval": field_eval, "return_code": rc, "log_file": str(log_file), "output_file": str(output_file), "parsed": False})
            continue

        for msg in case_messages:
            messages.append("%s: %s" % (label, msg))
        lat_summary_all.extend(lat_rows)
        aggregate_summary.append(agg)
        run_records.append({
            "case_label": label,
            "field_eval": field_eval,
            "return_code": rc,
            "log_file": str(log_file),
            "output_file": str(output_file),
            "parsed": True,
            "aggregate": agg,
        })

    # Apply PASS/FAIL criteria after all runs are summarized.
    passed = True
    if messages:
        # Missing output or AMPS failure messages should fail; diagnostic notes are added below separately.
        passed = False

    analytic_rows = [r for r in aggregate_summary if r["case_kind"] == "ANALYTIC"]
    mesh_aggs = sorted([r for r in aggregate_summary if r["case_kind"] == "MESH"], key=lambda x: x["mesh_index"])

    if not args.no_analytic_baseline:
        if not analytic_rows:
            passed = False
            messages.append("Analytic baseline was requested but no analytic summary was produced")
        else:
            a = analytic_rows[0]
            if a["max_abs_rel_mean_mid"] > args.analytic_mid_tol:
                passed = False
                messages.append(
                    "Analytic baseline mid-latitude max |mean rel error| %.3e exceeds %.3e" %
                    (a["max_abs_rel_mean_mid"], args.analytic_mid_tol)
                )
            if a["max_abs_rel_mean_high"] > args.analytic_high_tol:
                passed = False
                messages.append(
                    "Analytic baseline high-latitude max |mean rel error| %.3e exceeds %.3e" %
                    (a["max_abs_rel_mean_high"], args.analytic_high_tol)
                )

    if not mesh_aggs:
        passed = False
        messages.append("No mesh-backed cases were summarized")
    else:
        # Diagnostic: a very common MESH failure mode is collapse to the lowest
        # scanned rigidity.  With the C5 template CUTOFF_EMIN=1 MeV for protons,
        # that floor is about 4.3e-2 GV.  If all latitudes sit near that value,
        # the mesh-backed field is effectively zero/too weak or the field data are
        # not being read, so the cutoff search declares access at the minimum energy.
        floor_gv = proton_rigidity_from_kinetic_mev(1.0)
        for m in mesh_aggs:
            min_rc = m.get("min_mean_Rc_GV_all", float("nan"))
            max_rc = m.get("max_mean_Rc_GV_all", float("nan"))
            mean_mid = m.get("mean_Rc_GV_mid", float("nan"))
            if (math.isfinite(min_rc) and math.isfinite(max_rc) and
                math.isfinite(mean_mid) and floor_gv > 0.0):
                near_floor = (0.25*floor_gv <= min_rc <= 4.0*floor_gv and
                              0.25*floor_gv <= max_rc <= 4.0*floor_gv)
                collapsed_shape = (max_rc - min_rc) <= max(0.10*floor_gv, 1.0e-6)
                severe_mid_under = m["max_abs_rel_mean_mid"] > 0.80 and mean_mid < 0.20
                if near_floor and (collapsed_shape or severe_mid_under):
                    passed = False
                    messages.append(
                        "%s appears to have collapsed to the CUTOFF_EMIN floor: "
                        "mean Rc range %.4g..%.4g GV, 1 MeV proton floor %.4g GV. "
                        "This is not mesh-convergence behavior; it usually means the "
                        "mesh-interpolated B field is zero/too weak, not populated, or not being read." %
                        (m["case_label"], min_rc, max_rc, floor_gv)
                    )

        if len(mesh_aggs) >= 2:
            # Diagnostic: if all mesh cases produce exactly the same profile, changing
            # the requested mesh resolution is not affecting the actual calculation
            # or all cases are hitting the same cutoff floor.
            mesh_lat_profiles = []
            for m in mesh_aggs:
                rows = sorted([r for r in lat_summary_all if r["case_kind"] == "MESH" and r["case_label"] == m["case_label"]],
                              key=lambda x: x["lat_deg"])
                mesh_lat_profiles.append([r["Rc_num_mean_GV"] for r in rows])
            if mesh_lat_profiles and all(len(p) == len(mesh_lat_profiles[0]) for p in mesh_lat_profiles):
                max_delta = 0.0
                base = mesh_lat_profiles[0]
                for prof in mesh_lat_profiles[1:]:
                    for a,b in zip(base, prof):
                        max_delta = max(max_delta, abs(a-b))
                if max_delta <= 1.0e-8:
                    passed = False
                    messages.append(
                        "All mesh-resolution cases produced identical latitude profiles "
                        "within 1e-8 GV. Check that the new mesh-resolution CLI/input "
                        "options are parsed and that AMPS is not reusing an old mesh/cache."
                    )

        finest = mesh_aggs[-1]
        if finest["max_abs_rel_mean_mid"] > args.fine_mid_tol or finest["rms_rel_mean_mid"] > args.fine_mid_tol:
            passed = False
            messages.append(
                "Finest mesh mid-latitude error too large: max=%.3e rms=%.3e tol=%.3e" %
                (finest["max_abs_rel_mean_mid"], finest["rms_rel_mean_mid"], args.fine_mid_tol)
            )
        if finest["max_abs_rel_mean_high"] > args.fine_high_tol or finest["rms_rel_mean_high"] > args.fine_high_tol:
            passed = False
            messages.append(
                "Finest mesh high-latitude error too large: max=%.3e rms=%.3e tol=%.3e" %
                (finest["max_abs_rel_mean_high"], finest["rms_rel_mean_high"], args.fine_high_tol)
            )
        if finest["max_lon_spread_over_ref"] > args.fine_lon_spread_tol:
            passed = False
            messages.append(
                "Finest mesh longitude spread %.3e exceeds %.3e" %
                (finest["max_lon_spread_over_ref"], args.fine_lon_spread_tol)
            )
        if len(mesh_aggs) >= 2 and not args.no_convergence_gate:
            coarse = mesh_aggs[0]
            e0 = coarse["rms_rel_mean_mid"]
            e1 = finest["rms_rel_mean_mid"]
            if math.isfinite(e0) and math.isfinite(e1) and e0 > 0.0:
                improvement = (e0 - e1) / e0
                if improvement < args.min_improvement:
                    passed = False
                    messages.append(
                        "Mid-latitude RMS did not improve enough from coarsest to finest mesh: "
                        "improvement=%.3e required=%.3e" % (improvement, args.min_improvement)
                    )
            else:
                passed = False
                messages.append("Could not evaluate coarsest-to-finest convergence metric")

    summary_csv = workdir / "C5_latitude_summary.csv"
    aggregate_csv = workdir / "C5_aggregate_summary.csv"
    result_json = workdir / "C5_result.json"
    plot_png = workdir / "C5_mesh_convergence.png"
    rc_plot_png = workdir / "C5_cutoff_profile.png"
    write_csv_dicts(lat_summary_all, summary_csv)
    write_csv_dicts(aggregate_summary, aggregate_csv)
    if not args.dry_run:
        make_plot(lat_summary_all, aggregate_summary, plot_png)
        make_rc_plot(lat_summary_all, rc_plot_png)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "messages": messages,
        "np": args.np,
        "nt": args.nt,
        "scheduler": args.scheduler,
        "dynamic_chunk": args.dynamic_chunk,
        "upper_scan_n": args.upper_scan_n,
        "workdir": str(workdir),
        "mesh_cases": [c.to_dict() for c in mesh_cases],
        "use_legacy_mesh_resolution": args.use_legacy_mesh_resolution,
        "analytic_baseline": not args.no_analytic_baseline,
        "run_records": run_records,
        "latitude_summary_csv": str(summary_csv),
        "aggregate_summary_csv": str(aggregate_csv),
        "plot_png": str(plot_png) if plot_png.exists() else None,
        "rc_plot_png": str(rc_plot_png) if rc_plot_png.exists() else None,
        "emin_floor_proton_1MeV_GV": proton_rigidity_from_kinetic_mev(1.0),
        "stormer_R0_GV": STORMER_R0_GV,
        "latitude_summary": lat_summary_all,
        "aggregate_summary": aggregate_summary,
    }
    result_json.write_text(json.dumps(result, indent=2))

    print("\nC5 summary")
    print("==========")
    print("np=%d nt=%d scheduler=%s" % (args.np, args.nt, args.scheduler))
    if args.use_legacy_mesh_resolution:
        print("mesh resolution: legacy hard-coded localResolution()")
    else:
        print("mesh profiles:")
        for case in mesh_cases:
            print("  %s: earth=%g Re boundary=%g Re coarsening=%s exponent=%g Rb=%g Re" % (
                case.label, case.res_earth_re, case.res_boundary_re, case.coarsening, case.exponent, case.r_boundary_re
            ))
    print("\nAggregate errors:")
    for row in aggregate_summary:
        print(
            "  %-24s %-8s rms_mid=% .3e max_mid=% .3e rms_high=% .3e max_high=% .3e lon_spread=% .3e" %
            (row["case_label"], row["case_kind"], row["rms_rel_mean_mid"], row["max_abs_rel_mean_mid"],
             row["rms_rel_mean_high"], row["max_abs_rel_mean_high"], row["max_lon_spread_over_ref"])
        )
    if messages:
        print("\nMessages:")
        for m in messages:
            print("  - " + m)
    print("\nWrote: %s" % summary_csv)
    print("Wrote: %s" % aggregate_csv)
    print("Wrote: %s" % result_json)
    if plot_png.exists():
        print("Wrote: %s" % plot_png)
    if rc_plot_png.exists():
        print("Wrote: %s" % rc_plot_png)
    print("\nRESULT: %s" % ("PASS" if passed else "FAIL"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
