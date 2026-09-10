#!/usr/bin/env python3
"""
C2 — Dipole longitude and north/south symmetry validation test.

The script is intentionally self-contained so it can be run from the directory
that contains the AMPS executable:

    python srcEarth/test/C2/run_C2.py -np 4 -nt 16

C2 can be run in either standalone Mode3D or gridless mode:

    python srcEarth/test/C2/run_C2.py --mode 3d
    python srcEarth/test/C2/run_C2.py --mode gridless

For Mode3D, the script exposes the field-evaluation backend used by the
trajectory tracer:

    python srcEarth/test/C2/run_C2.py --mode 3d --mode3d-field-eval ANALYTIC

The test uses a centered aligned dipole.  The analytical solution is independent
of longitude and symmetric between +latitude and -latitude.  C2 therefore checks
symmetry of the numerical cutoff field on a 9000 km shell; it also reports the
absolute Størmer residual as a diagnostic but the primary PASS/FAIL gate is
longitude and north/south symmetry.
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
from typing import Dict, Iterable, List, Tuple

TEST_ID = "C2"
TEST_NAME = "Dipole longitude and north/south symmetry"

RE_KM = 6371.2
STORMER_R0_GV = 0.299792458 * 0.25 * 3.12e-5 * (RE_KM * 1000.0)
TARGET_ALT_KM = 9000.0
TARGET_LATS_DEG = (-60.0, -30.0, 0.0, 30.0, 60.0)
TARGET_LONS_DEG = tuple(float(i) for i in range(0, 360, 30))


class ShellRow(object):
    """Container for one parsed Tecplot shell row."""
    def __init__(self, alt_km, lon_deg, lat_deg, rc_num_gv, rc_code_stormer_gv, rel_err_code):
        self.alt_km = alt_km
        self.lon_deg = lon_deg
        self.lat_deg = lat_deg
        self.rc_num_gv = rc_num_gv
        self.rc_code_stormer_gv = rc_code_stormer_gv
        self.rel_err_code = rel_err_code


def stormer_vertical_gv(lat_deg: float, alt_km: float) -> float:
    """Independent analytical vertical Størmer cutoff used by the harness."""
    r_re = (RE_KM + alt_km) / RE_KM
    c4 = math.cos(math.radians(lat_deg)) ** 4
    return STORMER_R0_GV * c4 / (r_re * r_re)


def write_reference_csv(path: Path) -> None:
    """Write the analytical C2 reference table for all checked shell points."""
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["alt_km", "lon_deg", "lat_deg", "abs_lat_deg", "Rc_stormer_GV"])
        for lat in TARGET_LATS_DEG:
            for lon in TARGET_LONS_DEG:
                w.writerow([
                    "%.1f" % TARGET_ALT_KM,
                    "%.1f" % lon,
                    "%.1f" % lat,
                    "%.1f" % abs(lat),
                    "%.9e" % stormer_vertical_gv(lat, TARGET_ALT_KM),
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


def parse_tecplot_shell_output(path: Path) -> List[ShellRow]:
    """Parse Mode3D or gridless shell cutoff output."""
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


def summarize(rows: List[ShellRow], mode: str, mode3d_field_eval: str, lon_tol: float, ns_tol: float,
              high_lat_lon_tol: float, high_lat_ns_tol: float, absolute_tol: float) -> Tuple[List[Dict[str, float]], List[Dict[str, float]], bool, List[str]]:
    """Build longitude and north/south symmetry summaries and apply acceptance tests."""
    messages = []
    passed = True
    selected = []
    for r in rows:
        alt_key = TARGET_ALT_KM if abs(r.alt_km - TARGET_ALT_KM) <= 1.0e-6 else None
        lat_key = _nearest_key(r.lat_deg, TARGET_LATS_DEG)
        lon_key = _nearest_key(r.lon_deg % 360.0, TARGET_LONS_DEG)
        if alt_key is not None and lat_key is not None and lon_key is not None:
            selected.append((lon_key, lat_key, r))

    by_lat = {}
    by_lat_lon = {}
    for lon, lat, r in selected:
        by_lat.setdefault(lat, []).append(r)
        by_lat_lon[(lat, lon)] = r

    lon_summary = []
    for lat in TARGET_LATS_DEG:
        items = by_lat.get(lat, [])
        ref = stormer_vertical_gv(lat, TARGET_ALT_KM)
        if len(items) != len(TARGET_LONS_DEG):
            passed = False
            messages.append("Expected %d longitudes at lat=%g deg, found %d" % (len(TARGET_LONS_DEG), lat, len(items)))
            continue
        rc_values = [x.rc_num_gv for x in items]
        mean_rc = sum(rc_values) / len(rc_values)
        min_rc = min(rc_values)
        max_rc = max(rc_values)
        spread_abs = max_rc - min_rc
        spread_rel_ref = spread_abs / _rel_denominator(ref)
        spread_rel_mean = spread_abs / _rel_denominator(mean_rc)
        abs_rel_mean = abs(mean_rc - ref) / _rel_denominator(ref)
        code_refs = [x.rc_code_stormer_gv for x in items if x.rc_code_stormer_gv is not None]
        code_ref_rel = None
        if code_refs:
            code_ref_mean = sum(code_refs) / len(code_refs)
            code_ref_rel = abs(code_ref_mean - ref) / _rel_denominator(ref)
            if code_ref_rel > 5.0e-3:
                passed = False
                messages.append("AMPS analytic Rc column differs from independent reference by %.3e at lat=%g deg" % (code_ref_rel, lat))
        else:
            code_ref_mean = float("nan")

        tol = high_lat_lon_tol if abs(lat) >= 60.0 else lon_tol
        if spread_rel_ref > tol:
            passed = False
            messages.append(
                "Longitude symmetry failure at lat=%g deg: spread/ref=%.3e exceeds %.3e" %
                (lat, spread_rel_ref, tol)
            )

        if abs_rel_mean > absolute_tol:
            # Absolute Størmer agreement is a diagnostic for C2, but still useful
            # enough to report as a warning.  It is not a PASS/FAIL criterion by
            # default because C1 is the absolute-cutoff gate and C2 isolates
            # symmetry.  The row records it for triage.
            messages.append(
                "Diagnostic: mean Rc differs from Størmer by %.3e at lat=%g deg; C2 primarily gates symmetry" %
                (abs_rel_mean, lat)
            )

        lon_summary.append({
            "mode": mode,
            "mode3d_field_eval": mode3d_field_eval if mode == "3d" else "N/A",
            "alt_km": TARGET_ALT_KM,
            "lat_deg": lat,
            "n_lon": float(len(items)),
            "Rc_reference_GV": ref,
            "Rc_code_reference_mean_GV": code_ref_mean,
            "Rc_num_mean_GV": mean_rc,
            "Rc_num_min_GV": min_rc,
            "Rc_num_max_GV": max_rc,
            "longitude_spread_GV": spread_abs,
            "longitude_spread_over_ref": spread_rel_ref,
            "longitude_spread_over_mean": spread_rel_mean,
            "mean_abs_rel_error_vs_stormer": abs_rel_mean,
            "longitude_tolerance": tol,
        })

    ns_summary = []
    for alat in (30.0, 60.0):
        ref = stormer_vertical_gv(alat, TARGET_ALT_KM)
        pair_diffs = []
        for lon in TARGET_LONS_DEG:
            rp = by_lat_lon.get((alat, lon))
            rm = by_lat_lon.get((-alat, lon))
            if rp is None or rm is None:
                continue
            pair_diffs.append(abs(rp.rc_num_gv - rm.rc_num_gv))
        if len(pair_diffs) != len(TARGET_LONS_DEG):
            passed = False
            messages.append("Missing N/S paired longitudes for |lat|=%g deg" % alat)
            continue
        max_diff = max(pair_diffs)
        mean_diff = sum(pair_diffs) / len(pair_diffs)
        max_rel = max_diff / _rel_denominator(ref)
        mean_rel = mean_diff / _rel_denominator(ref)
        tol = high_lat_ns_tol if alat >= 60.0 else ns_tol
        if max_rel > tol:
            passed = False
            messages.append(
                "North/south symmetry failure at |lat|=%g deg: max_pair_diff/ref=%.3e exceeds %.3e" %
                (alat, max_rel, tol)
            )
        ns_summary.append({
            "mode": mode,
            "mode3d_field_eval": mode3d_field_eval if mode == "3d" else "N/A",
            "alt_km": TARGET_ALT_KM,
            "abs_lat_deg": alat,
            "n_lon_pairs": float(len(pair_diffs)),
            "Rc_reference_GV": ref,
            "ns_pair_diff_mean_GV": mean_diff,
            "ns_pair_diff_max_GV": max_diff,
            "ns_pair_diff_mean_over_ref": mean_rel,
            "ns_pair_diff_max_over_ref": max_rel,
            "ns_tolerance": tol,
        })

    return lon_summary, ns_summary, passed, messages


def write_csv(rows, path):
    if not rows:
        return
    keys = list(rows[0].keys())
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for row in rows:
            w.writerow(row)


def make_plot(rows: List[ShellRow], path: Path) -> None:
    """Generate a compact C2 symmetry plot when matplotlib is available."""
    try:
        import matplotlib.pyplot as plt
    except Exception:
        return

    plt.figure(figsize=(8.0, 5.2))
    for lat in TARGET_LATS_DEG:
        vals = []
        for lon in TARGET_LONS_DEG:
            match = None
            for r in rows:
                if abs(r.alt_km - TARGET_ALT_KM) <= 1.0e-6 and abs(r.lat_deg - lat) <= 1.0e-6 and abs((r.lon_deg % 360.0) - lon) <= 1.0e-6:
                    match = r
                    break
            if match is not None:
                vals.append((lon, match.rc_num_gv))
        if vals:
            xs = [x[0] for x in vals]
            ys = [x[1] for x in vals]
            plt.plot(xs, ys, marker="o", label="lat=%g deg" % lat)
    plt.xlabel("Longitude (deg)")
    plt.ylabel("Vertical cutoff rigidity (GV)")
    plt.title("C2: dipole longitude and north/south symmetry, alt=%g km" % TARGET_ALT_KM)
    plt.grid(True, alpha=0.3)
    plt.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(path, dpi=160)
    plt.close()


def run_command(cmd: List[str], cwd: Path, log_path: Path) -> int:
    with log_path.open("w") as log:
        log.write("Command:\n  " + " ".join(cmd) + "\n\n")
        log.flush()
        proc = subprocess.Popen(cmd, cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            log.write(line)
        return proc.wait()


def _replace_or_append_keyword(text, keyword, value, section_marker=None):
    """Replace a simple active keyword assignment, or append it in a section."""
    pattern = re.compile(r'^(\s*' + re.escape(keyword) + r'\s+)([^!\n]*)(.*)$', re.MULTILINE)
    replacement = r'\g<1>' + str(value) + r'\3'
    if pattern.search(text):
        return pattern.sub(replacement, text, count=1)
    line = "%s               %s\n" % (keyword, value)
    if section_marker and section_marker in text:
        idx = text.find(section_marker)
        next_idx = text.find("\n#", idx + 1)
        if next_idx >= 0:
            return text[:next_idx] + "\n" + line.rstrip() + text[next_idx:]
    return text.rstrip() + "\n" + line


def render_input_template(template_path, output_path, nt, scheduler, dynamic_chunk, dt_trace, max_trace_time, max_trace_distance):
    """Copy a parser-compatible C2 template into the run directory.

    The active input syntax follows the known-working CCMC/RoR-style layout:
    OUTPUT_MODE/SHELL_COUNT/SHELL_ALTS_KM/SHELL_RES_DEG are used rather than
    validation-plan shorthand such as SHELL_LON_DEG or SHELL_LAT_DEG.
    """
    text = template_path.read_text()
    text = _replace_or_append_keyword(text, "DT_TRACE", str(dt_trace), "#NUMERICAL")
    text = _replace_or_append_keyword(text, "MAX_TRACE_TIME", str(max_trace_time), "#NUMERICAL")
    text = _replace_or_append_keyword(text, "MAX_TRACE_DISTANCE", str(max_trace_distance), "#NUMERICAL")
    text = _replace_or_append_keyword(text, "CUTOFF_MAX_TRAJ_TIME", str(max_trace_time), "#CUTOFF_RIGIDITY")
    text += (
        "\n! ── C2 harness run-time settings, supplied through CLI ─────────\n"
        "! C2_NT                  %d\n"
        "! C2_MPI_SCHEDULER       %s\n"
        "! C2_MPI_DYNAMIC_CHUNK   %d\n"
        "! C2_CUTOFF_SEARCH       UPPER_SCAN\n"
        "! C2_CUTOFF_SCAN_N       80\n"
    ) % (nt, scheduler, dynamic_chunk)
    output_path.write_text(text)


def find_output_file(workdir, mode, user_output):
    if user_output:
        p = Path(user_output)
        if not p.is_absolute():
            p = workdir / p
        return p
    if mode == "3d":
        candidates = [
            "cutoff_3d_shells_dipole_compare.dat",
            "cutoff_3d_shells.dat",
        ]
    else:
        candidates = [
            "cutoff_gridless_shells_dipole_compare.dat",
            "cutoff_gridless_shells.dat",
        ]
    for name in candidates:
        p = workdir / name
        if p.exists():
            return p
    matches = sorted(workdir.glob("cutoff_*shell*.dat"))
    if matches:
        return matches[0]
    return workdir / candidates[0]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        C2: Dipole longitude and north/south symmetry

        This test runs a centered aligned dipole cutoff calculation on a 9000 km
        shell with 30-degree angular resolution.  The parser-compatible input
        uses OUTPUT_MODE SHELLS, SHELL_COUNT, SHELL_ALTS_KM, and SHELL_RES_DEG.
        The Python script then post-processes the output at latitudes
        -60, -30, 0, +30, +60 and longitudes 0, 30, ..., 330.

        The primary validation metrics are:
          1. longitude spread of Rc at fixed latitude;
          2. Rc(+latitude, longitude) - Rc(-latitude, longitude).

        AMPS itself is launched through mpirun.  Defaults are -np 4 and -nt 16.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/C2/run_C2.py
        W python srcEarth/test/C2/run_C2.py --mode 3d --mode3d-field-eval ANALYTIC -np 4 -nt 16 --max-trace-distance 300 --cutoff-scan-n 80 
          python srcEarth/test/C2/run_C2.py --mode 3d --mode3d-field-eval MESH -np 4 -nt 16 --max-trace-distance 300 --cutoff-scan-n 80 
          python srcEarth/test/C2/run_C2.py --mode 3d --mode3d-field-eval MESH --mode3d-mesh-res-earth-re 0.02 --mode3d-mesh-res-boundary-re 3 --mode3d-mesh-coarsening LINEAR
        W python srcEarth/test/C2/run_C2.py --mode gridless -np 4 -nt 16 --max-trace-distance 300 --cutoff-scan-n 80 
          python srcEarth/test/C2/run_C2.py --skip-run --workdir test_output/C2_gridless
        """)
    )
    parser.add_argument("-np", type=int, default=4, help="number of MPI processes passed to mpirun; default: 4")
    parser.add_argument("-nt", type=int, default=16, help="number of threads per MPI rank; default: 16")
    parser.add_argument("--mode", choices=["3d", "gridless"], default="3d", help="AMPS mode to validate; default: 3d")
    parser.add_argument("--mode3d-field-eval", "--field-eval", dest="mode3d_field_eval", default="ANALYTIC", choices=["ANALYTIC", "MESH", "GRID_3D"], help="Mode3D field backend passed as -mode3d-field-eval; default: ANALYTIC")
    parser.add_argument("--scheduler", default="DYNAMIC", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], help="MPI scheduler to use for the selected mode; default: DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0, help="dynamic MPI chunk size; 0 means AMPS auto heuristic; default: 0")
    parser.add_argument("--mode3d-mesh-res-earth-re", type=float, default=None, help="optional Mode3D AMR resolution near Earth, in Re; passed to AMPS as -mode3d-mesh-res-earth-re")
    parser.add_argument("--mode3d-mesh-res-boundary-re", type=float, default=None, help="optional Mode3D AMR resolution at the outer boundary, in Re; passed to AMPS as -mode3d-mesh-res-boundary-re")
    parser.add_argument("--mode3d-mesh-coarsening", "--coarsening", dest="mode3d_mesh_coarsening", default=None, choices=["LINEAR", "LOG", "EXPONENTIAL", "GEOMETRIC", "POWER", "CONSTANT"], help="optional AMPS radial AMR coarsening profile; --coarsening is a C1-compatible alias")
    parser.add_argument("--mode3d-mesh-exponent", "--exponent", dest="mode3d_mesh_exponent", type=float, default=None, help="optional AMPS POWER-coarsening exponent, in Re mesh-profile controls")
    parser.add_argument("--mode3d-mesh-r-boundary-re", "--r-boundary-re", dest="mode3d_mesh_r_boundary_re", type=float, default=None, help="optional AMPS mesh-profile outer radius, in Re")
    parser.add_argument("--cutoff-scan-n", type=int, default=80, help="number of UPPER_SCAN rigidity samples passed to AMPS; default: 80")
    parser.add_argument("--no-cutoff-search-cli", action="store_true", help="do not pass -cutoff-search/-cutoff-upper-scan-n; useful for older CLI checkouts")
    parser.add_argument("--dt-trace", type=float, default=1.0, help="DT_TRACE value written into the generated input; default: 1.0 s")
    parser.add_argument("--max-trace-time", type=float, default=600.0, help="MAX_TRACE_TIME and CUTOFF_MAX_TRAJ_TIME written into the generated input; default: 600 s")
    parser.add_argument("--max-trace-distance", type=float, default=0.0, help="MAX_TRACE_DISTANCE written into the generated input in Re; 0 disables the cap; default: 0")
    parser.add_argument("--lon-tol", type=float, default=1.0e-2, help="longitude-spread tolerance for |lat|<=30 as spread/Rc_ref; default: 1e-2")
    parser.add_argument("--ns-tol", type=float, default=1.0e-2, help="north/south-pair tolerance for |lat|=30 as max pair difference/Rc_ref; default: 1e-2")
    parser.add_argument("--high-lat-lon-tol", type=float, default=5.0e-2, help="longitude-spread tolerance for |lat|=60; default: 5e-2")
    parser.add_argument("--high-lat-ns-tol", type=float, default=5.0e-2, help="north/south-pair tolerance for |lat|=60; default: 5e-2")
    parser.add_argument("--absolute-tol", type=float, default=2.5e-1, help="diagnostic-only mean absolute Størmer residual reporting threshold; default: 0.25")
    parser.add_argument("--amps", default="./amps", help="AMPS executable path, relative to the launch directory; default: ./amps")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher executable; default: mpirun")
    parser.add_argument("--workdir", default=None, help="directory where the test is run and outputs are written; default: test_output/C2_<mode>")
    parser.add_argument("--output-file", default=None, help="explicit AMPS cutoff shell output file to parse; default: auto-detect")
    parser.add_argument("--skip-run", action="store_true", help="do not run AMPS; analyze existing output in --workdir")
    parser.add_argument("--keep", action="store_true", help="keep an existing work directory instead of replacing it")
    parser.add_argument("--run-in-place", action="store_true", help="run AMPS in the launch directory rather than in --workdir; output files may be overwritten")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.mode3d_mesh_res_earth_re is not None and args.mode3d_mesh_res_earth_re <= 0.0:
        raise SystemExit("--mode3d-mesh-res-earth-re must be > 0")
    if args.mode3d_mesh_res_boundary_re is not None and args.mode3d_mesh_res_boundary_re <= 0.0:
        raise SystemExit("--mode3d-mesh-res-boundary-re must be > 0")
    if args.mode3d_mesh_exponent is not None and args.mode3d_mesh_exponent <= 0.0:
        raise SystemExit("--mode3d-mesh-exponent must be > 0")
    if args.mode3d_mesh_r_boundary_re is not None and args.mode3d_mesh_r_boundary_re <= 1.0:
        raise SystemExit("--mode3d-mesh-r-boundary-re must be > 1")
    if args.mode == "gridless" and any(x is not None for x in (args.mode3d_mesh_res_earth_re, args.mode3d_mesh_res_boundary_re, args.mode3d_mesh_coarsening, args.mode3d_mesh_exponent, args.mode3d_mesh_r_boundary_re)):
        raise SystemExit("Mode3D mesh-resolution options can only be used with --mode 3d")
    if args.cutoff_scan_n < 1:
        raise SystemExit("--cutoff-scan-n must be >= 1")
    if args.max_trace_distance < 0.0:
        raise SystemExit("--max-trace-distance must be >= 0")

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    if args.mode == "3d":
        template_input = script_dir / "AMPS_PARAM_C2_mode3d.in"
    else:
        template_input = script_dir / "AMPS_PARAM_C2_gridless.in"
    if not template_input.exists():
        template_input = script_dir / "AMPS_PARAM_C2.in"
    reference_csv = script_dir / "reference_C2_stormer_symmetry.csv"

    if args.workdir is None:
        if args.mode == "3d":
            args.workdir = "test_output/C2_3d_%s" % args.mode3d_field_eval.lower()
        else:
            args.workdir = "test_output/C2_gridless"

    workdir = (launch_dir / args.workdir).resolve()
    if not args.skip_run and not args.run_in_place:
        if workdir.exists() and not args.keep:
            shutil.rmtree(workdir)
        workdir.mkdir(parents=True, exist_ok=True)
        render_input_template(template_input, workdir / "AMPS_PARAM_C2.in", args.nt, args.scheduler, args.dynamic_chunk, args.dt_trace, args.max_trace_time, args.max_trace_distance)
        shutil.copy2(reference_csv, workdir / "reference_C2_stormer_symmetry.csv")
    elif args.run_in_place:
        workdir = launch_dir
        render_input_template(template_input, workdir / "AMPS_PARAM_C2.in", args.nt, args.scheduler, args.dynamic_chunk, args.dt_trace, args.max_trace_time, args.max_trace_distance)
        shutil.copy2(reference_csv, workdir / "reference_C2_stormer_symmetry.csv")
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)

    log_file = workdir / "C2_amps.log"
    result_json = workdir / "C2_result.json"
    lon_csv = workdir / "C2_longitude_summary.csv"
    ns_csv = workdir / "C2_north_south_summary.csv"
    plot_png = workdir / "C2_symmetry.png"

    if not args.skip_run:
        amps_path = Path(args.amps)
        if not amps_path.is_absolute():
            amps_path = (launch_dir / amps_path).resolve()
        cmd = [
            args.mpirun,
            "-np", str(args.np),
            str(amps_path),
            "-mode", args.mode,
            "-i", "AMPS_PARAM_C2.in",
        ]
        if not args.no_cutoff_search_cli:
            cmd += ["-cutoff-search", "UPPER_SCAN", "-cutoff-upper-scan-n", str(args.cutoff_scan_n)]
        if args.mode == "3d":
            cmd += [
                "-mode3d-field-eval", args.mode3d_field_eval,
                "-mode3d-parallel", "THREADS",
                "-mode3d-threads", str(args.nt),
                "-mode3d-mpi-scheduler", args.scheduler,
                "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
            ]
            if args.mode3d_mesh_res_earth_re is not None:
                cmd += ["-mode3d-mesh-res-earth-re", str(args.mode3d_mesh_res_earth_re)]
            if args.mode3d_mesh_res_boundary_re is not None:
                cmd += ["-mode3d-mesh-res-boundary-re", str(args.mode3d_mesh_res_boundary_re)]
            if args.mode3d_mesh_coarsening is not None:
                cmd += ["-mode3d-mesh-coarsening", args.mode3d_mesh_coarsening]
            if args.mode3d_mesh_exponent is not None:
                cmd += ["-mode3d-mesh-exponent", str(args.mode3d_mesh_exponent)]
            if args.mode3d_mesh_r_boundary_re is not None:
                cmd += ["-mode3d-mesh-r-boundary-re", str(args.mode3d_mesh_r_boundary_re)]
        else:
            cmd += [
                "-gridless-mpi-scheduler", args.scheduler,
                "-gridless-mpi-dynamic-chunk", str(args.dynamic_chunk),
                "-density-parallel", "THREADS",
                "-density-threads", str(args.nt),
            ]
        print("Running %s in %s" % (TEST_ID, workdir))
        print(" ".join(cmd))
        rc = run_command(cmd, cwd=workdir, log_path=log_file)
        if rc != 0:
            print("AMPS command failed with exit code %d. See %s" % (rc, log_file), file=sys.stderr)
            return rc

    output_file = find_output_file(workdir, args.mode, args.output_file)
    if not output_file.exists():
        print("Expected AMPS output was not found: %s" % output_file, file=sys.stderr)
        return 2

    rows = parse_tecplot_shell_output(output_file)
    lon_summary, ns_summary, passed, messages = summarize(
        rows,
        args.mode,
        args.mode3d_field_eval,
        args.lon_tol,
        args.ns_tol,
        args.high_lat_lon_tol,
        args.high_lat_ns_tol,
        args.absolute_tol,
    )
    write_csv(lon_summary, lon_csv)
    write_csv(ns_summary, ns_csv)
    make_plot(rows, plot_png)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "messages": messages,
        "np": args.np,
        "nt": args.nt,
        "mode": args.mode,
        "mode3d_field_eval": args.mode3d_field_eval if args.mode == "3d" else None,
        "scheduler": args.scheduler,
        "dynamic_chunk": args.dynamic_chunk,
        "mode3d_mesh_res_earth_re": args.mode3d_mesh_res_earth_re if args.mode == "3d" else None,
        "mode3d_mesh_res_boundary_re": args.mode3d_mesh_res_boundary_re if args.mode == "3d" else None,
        "mode3d_mesh_coarsening": args.mode3d_mesh_coarsening if args.mode == "3d" else None,
        "mode3d_mesh_exponent": args.mode3d_mesh_exponent if args.mode == "3d" else None,
        "mode3d_mesh_r_boundary_re": args.mode3d_mesh_r_boundary_re if args.mode == "3d" else None,
        "cutoff_scan_n": args.cutoff_scan_n,
        "max_trace_time": args.max_trace_time,
        "max_trace_distance": args.max_trace_distance,
        "cutoff_search_cli_enabled": not args.no_cutoff_search_cli,
        "workdir": str(workdir),
        "output_file": str(output_file),
        "longitude_summary_csv": str(lon_csv),
        "north_south_summary_csv": str(ns_csv),
        "plot_png": str(plot_png) if plot_png.exists() else None,
        "stormer_R0_GV": STORMER_R0_GV,
        "longitude_summary": lon_summary,
        "north_south_summary": ns_summary,
    }
    result_json.write_text(json.dumps(result, indent=2))

    print("\nC2 summary")
    print("==========")
    print("mode=%s np=%d nt=%d scheduler=%s" % (args.mode, args.np, args.nt, args.scheduler))
    if args.mode == "3d":
        print("mode3d_field_eval=%s" % args.mode3d_field_eval)
        mesh_info = []
        if args.mode3d_mesh_res_earth_re is not None:
            mesh_info.append("res_earth_re=%g" % args.mode3d_mesh_res_earth_re)
        if args.mode3d_mesh_res_boundary_re is not None:
            mesh_info.append("res_boundary_re=%g" % args.mode3d_mesh_res_boundary_re)
        if args.mode3d_mesh_coarsening is not None:
            mesh_info.append("coarsening=%s" % args.mode3d_mesh_coarsening)
        if args.mode3d_mesh_exponent is not None:
            mesh_info.append("exponent=%g" % args.mode3d_mesh_exponent)
        if args.mode3d_mesh_r_boundary_re is not None:
            mesh_info.append("r_boundary_re=%g" % args.mode3d_mesh_r_boundary_re)
        if mesh_info:
            print("mode3d_mesh=" + ", ".join(mesh_info))
    print("altitude=%g km" % TARGET_ALT_KM)
    print("\nLongitude symmetry:")
    for row in lon_summary:
        print(
            "lat=%6.1f deg Rc_mean=%10.5e GV Rc_ref=%10.5e GV "
            "spread/ref=% .3e tol=% .3e abs_mean_err=% .3e" %
            (row['lat_deg'], row['Rc_num_mean_GV'], row['Rc_reference_GV'],
             row['longitude_spread_over_ref'], row['longitude_tolerance'],
             row['mean_abs_rel_error_vs_stormer'])
        )
    print("\nNorth/south symmetry:")
    for row in ns_summary:
        print(
            "|lat|=%6.1f deg max_pair_diff/ref=% .3e tol=% .3e mean_pair_diff/ref=% .3e" %
            (row['abs_lat_deg'], row['ns_pair_diff_max_over_ref'], row['ns_tolerance'], row['ns_pair_diff_mean_over_ref'])
        )
    if messages:
        print("\nMessages:")
        for m in messages:
            print("  - " + m)
    print("\nWrote: %s" % lon_csv)
    print("Wrote: %s" % ns_csv)
    print("Wrote: %s" % result_json)
    if plot_png.exists():
        print("Wrote: %s" % plot_png)
    print("\nRESULT: %s" % ("PASS" if passed else "FAIL"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
