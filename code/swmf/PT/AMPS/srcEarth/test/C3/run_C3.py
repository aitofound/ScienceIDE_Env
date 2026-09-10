#!/usr/bin/env python3
"""
C3 — Penumbra / upper-cutoff search validation test.

Run from the directory that contains the AMPS executable:

    python srcEarth/test/C3/run_C3.py -np 4 -nt 16

C3 exercises the high-latitude, outer-shell dipole point that is most sensitive
to non-monotonic allowed/forbidden access and to the old endpoint-binary failure
mode.  The script runs AMPS twice by default:

    1. CUTOFF_SEARCH_ALGORITHM BINARY      (legacy/diagnostic run)
    2. CUTOFF_SEARCH_ALGORITHM UPPER_SCAN  (production validation run)

The PASS/FAIL gate is applied to the UPPER_SCAN result.  The BINARY result is
reported so that an endpoint-collapse to the low search boundary can be spotted,
but it is not a required failure unless --require-binary-collapse is set.
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

TEST_ID = "C3"
TEST_NAME = "Penumbra, Rc_upper, and UPPER_SCAN regression"

RE_KM = 6371.2
PROTON_REST_GEV = 0.9382720813
STORMER_R0_GV = 0.299792458 * 0.25 * 3.12e-5 * (RE_KM * 1000.0)
TARGET_ALT_KM = 9000.0
TARGET_LATS_DEG = (-60.0, 60.0)
TARGET_LONS_DEG = (0.0, 90.0, 180.0, 270.0)
PRIMARY_LON_DEG = 0.0
PRIMARY_LAT_DEG = -60.0
CUTOFF_EMIN_MEV = 1.0


class ShellRow(object):
    def __init__(self, alt_km, lon_deg, lat_deg, rc_num_gv, rc_code_stormer_gv, rel_err_code):
        self.alt_km = alt_km
        self.lon_deg = lon_deg
        self.lat_deg = lat_deg
        self.rc_num_gv = rc_num_gv
        self.rc_code_stormer_gv = rc_code_stormer_gv
        self.rel_err_code = rel_err_code


def stormer_vertical_gv(lat_deg, alt_km):
    """Independent vertical Størmer cutoff reference in GV."""
    r_re = (RE_KM + alt_km) / RE_KM
    c4 = math.cos(math.radians(lat_deg)) ** 4
    return STORMER_R0_GV * c4 / (r_re * r_re)


def proton_rigidity_from_ekin_mev(ekin_mev):
    """Return proton rigidity in GV for kinetic energy per nucleon in MeV/n."""
    e_gev = ekin_mev * 1.0e-3
    return math.sqrt(e_gev * e_gev + 2.0 * e_gev * PROTON_REST_GEV)


def write_reference_csv(path):
    rmin = proton_rigidity_from_ekin_mev(CUTOFF_EMIN_MEV)
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["alt_km", "lon_deg", "lat_deg", "Rc_stormer_GV", "CUTOFF_EMIN_MeV", "Rmin_from_CUTOFF_EMIN_GV", "purpose"])
        for lat in TARGET_LATS_DEG:
            for lon in TARGET_LONS_DEG:
                w.writerow([
                    "%.1f" % TARGET_ALT_KM,
                    "%.1f" % lon,
                    "%.1f" % lat,
                    "%.9e" % stormer_vertical_gv(lat, TARGET_ALT_KM),
                    "%.1f" % CUTOFF_EMIN_MEV,
                    "%.9e" % rmin,
                    "C3 high-latitude penumbra/UPPER_SCAN target",
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
    """Parse AMPS shell cutoff Tecplot output."""
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


def close_to(value, target, tol=1.0e-6):
    return abs(value - target) <= tol


def selected_rows(rows):
    selected = []
    for row in rows:
        if not close_to(row.alt_km, TARGET_ALT_KM):
            continue
        lon = row.lon_deg % 360.0
        for target_lat in TARGET_LATS_DEG:
            if not close_to(row.lat_deg, target_lat):
                continue
            for target_lon in TARGET_LONS_DEG:
                if close_to(lon, target_lon):
                    selected.append((target_lon, target_lat, row))
    return selected


def summarize_algorithm(algorithm, rows, upper_rel_tol, collapse_factor):
    rmin = proton_rigidity_from_ekin_mev(CUTOFF_EMIN_MEV)
    summary = []
    passed = True
    messages = []
    primary_found = False
    for lon, lat, row in selected_rows(rows):
        ref = stormer_vertical_gv(lat, TARGET_ALT_KM)
        rel_err = (row.rc_num_gv - ref) / ref if ref > 0.0 else 0.0
        abs_rel_err = abs(rel_err)
        collapsed = abs(row.rc_num_gv - rmin) / max(rmin, 1.0e-30) < 1.0e-3 or row.rc_num_gv < collapse_factor * rmin
        is_primary = close_to(lon, PRIMARY_LON_DEG) and close_to(lat, PRIMARY_LAT_DEG)
        if is_primary:
            primary_found = True
        row_out = {
            "algorithm": algorithm,
            "alt_km": TARGET_ALT_KM,
            "lon_deg": lon,
            "lat_deg": lat,
            "Rc_num_GV": row.rc_num_gv,
            "Rc_stormer_GV": ref,
            "Rmin_GV": rmin,
            "rel_error": rel_err,
            "abs_rel_error": abs_rel_err,
            "collapsed_to_low_boundary": bool(collapsed),
            "is_primary_point": bool(is_primary),
        }
        summary.append(row_out)
        if algorithm == "UPPER_SCAN":
            if collapsed:
                passed = False
                messages.append(
                    "UPPER_SCAN collapsed near Rmin at lon=%g lat=%g: Rc=%g GV, Rmin=%g GV" %
                    (lon, lat, row.rc_num_gv, rmin)
                )
            if abs_rel_err > upper_rel_tol:
                passed = False
                messages.append(
                    "UPPER_SCAN Størmer residual too large at lon=%g lat=%g: abs_rel_error=%.3e > %.3e" %
                    (lon, lat, abs_rel_err, upper_rel_tol)
                )
    if not summary:
        passed = False
        messages.append("No C3 target rows were found for algorithm %s" % algorithm)
    if algorithm == "UPPER_SCAN" and not primary_found:
        passed = False
        messages.append("Primary C3 point lon=%g lat=%g alt=%g was not found" % (PRIMARY_LON_DEG, PRIMARY_LAT_DEG, TARGET_ALT_KM))
    return summary, passed, messages


def write_csv(rows, path):
    if not rows:
        return
    keys = list(rows[0].keys())
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for row in rows:
            w.writerow(row)


def make_plot(summary_rows, path):
    try:
        import matplotlib.pyplot as plt
    except Exception:
        return
    if not summary_rows:
        return
    # Plot the target high-latitude points: numerical Rc by algorithm and latitude.
    labels = []
    values = []
    refs = []
    for row in summary_rows:
        if abs(row["lon_deg"] - PRIMARY_LON_DEG) > 1.0e-6:
            continue
        labels.append("%s\nlat=%+.0f" % (row["algorithm"], row["lat_deg"]))
        values.append(row["Rc_num_GV"])
        refs.append(row["Rc_stormer_GV"])
    if not labels:
        return
    x = list(range(len(labels)))
    plt.figure(figsize=(8.0, 5.0))
    plt.bar(x, values, label="AMPS Rc")
    plt.plot(x, refs, marker="o", linestyle="--", label="Størmer reference")
    plt.xticks(x, labels, rotation=0)
    plt.ylabel("Vertical cutoff rigidity (GV)")
    plt.title("C3: BINARY vs UPPER_SCAN at alt=%g km, lon=%g deg" % (TARGET_ALT_KM, PRIMARY_LON_DEG))
    plt.grid(True, axis="y", alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(path, dpi=160)
    plt.close()


def run_command(cmd, cwd, log_path):
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
    text = template_path.read_text()
    text = _replace_or_append_keyword(text, "DT_TRACE", str(dt_trace), "#NUMERICAL")
    text = _replace_or_append_keyword(text, "MAX_TRACE_TIME", str(max_trace_time), "#NUMERICAL")
    text = _replace_or_append_keyword(text, "MAX_TRACE_DISTANCE", str(max_trace_distance), "#NUMERICAL")
    text = _replace_or_append_keyword(text, "CUTOFF_MAX_TRAJ_TIME", str(max_trace_time), "#CUTOFF_RIGIDITY")
    text += (
        "\n! ── C3 harness run-time settings, supplied through CLI ─────────\n"
        "! C3_NT                  %d\n"
        "! C3_MPI_SCHEDULER       %s\n"
        "! C3_MPI_DYNAMIC_CHUNK   %d\n"
        "! C3_ALGORITHMS          BINARY, UPPER_SCAN\n"
    ) % (nt, scheduler, dynamic_chunk)
    output_path.write_text(text)


def find_output_file(workdir, mode, user_output):
    if user_output:
        p = Path(user_output)
        if not p.is_absolute():
            p = workdir / p
        return p
    if mode == "3d":
        candidates = ["cutoff_3d_shells_dipole_compare.dat", "cutoff_3d_shells.dat"]
    else:
        candidates = ["cutoff_gridless_shells_dipole_compare.dat", "cutoff_gridless_shells.dat"]
    for name in candidates:
        p = workdir / name
        if p.exists():
            return p
    matches = sorted(workdir.glob("cutoff_*shell*.dat"))
    if matches:
        return matches[0]
    return workdir / candidates[0]


def parse_algorithms(value):
    if value.upper() == "BOTH":
        return ["BINARY", "UPPER_SCAN"]
    out = []
    for item in value.split(","):
        a = item.strip().upper()
        if a:
            if a not in ("BINARY", "UPPER_SCAN"):
                raise SystemExit("Unknown algorithm %s" % a)
            out.append(a)
    if not out:
        raise SystemExit("No algorithms requested")
    return out


def parse_args():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        C3: Penumbra / upper-cutoff search validation

        The test targets the high-latitude dipole-shell point
        lon=0 deg, lat=-60 deg, alt=9000 km, where the analytical vertical
        Størmer cutoff is about 0.160 GV.  Since CUTOFF_EMIN=1 MeV/n
        corresponds to Rmin about 0.043331 GV, an endpoint-binary failure or
        low-rigidity leakage can be diagnosed as a return near the lower search
        boundary rather than near the upper cutoff.

        Defaults: -np 4 and -nt 16.  AMPS is launched with mpirun.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/C3/run_C3.py
        W python srcEarth/test/C3/run_C3.py --mode 3d --mode3d-field-eval ANALYTIC -np 4 -nt 16
        W python srcEarth/test/C3/run_C3.py --mode 3d --mode3d-field-eval MESH -np 4 -nt 16
        W python srcEarth/test/C3/run_C3.py --mode gridless --max-trace-distance 300
        W python srcEarth/test/C3/run_C3.py --algorithms UPPER_SCAN --cutoff-scan-n 200
          python srcEarth/test/C3/run_C3.py --skip-run --workdir test_output/C3_gridless
        """)
    )
    parser.add_argument("-np", type=int, default=4, help="number of MPI processes passed to mpirun; default: 4")
    parser.add_argument("-nt", type=int, default=16, help="number of threads per MPI rank; default: 16")
    parser.add_argument("--mode", choices=["3d", "gridless"], default="3d", help="AMPS mode to validate; default: 3d")
    parser.add_argument("--mode3d-field-eval", "--field-eval", dest="mode3d_field_eval", default="ANALYTIC", choices=["ANALYTIC", "MESH", "GRID_3D"], help="Mode3D field backend passed as -mode3d-field-eval; default: ANALYTIC")
    parser.add_argument("--scheduler", default="DYNAMIC", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], help="MPI scheduler to use for the selected mode; default: DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0, help="dynamic MPI chunk size; 0 means AMPS auto heuristic; default: 0")
    parser.add_argument("--algorithms", default="BOTH", help="BOTH, UPPER_SCAN, BINARY, or comma-separated list; default: BOTH")
    parser.add_argument("--cutoff-scan-n", type=int, default=200, help="number of UPPER_SCAN rigidity samples passed to AMPS; default: 200")
    parser.add_argument("--no-cutoff-search-cli", action="store_true", help="do not pass -cutoff-search/-cutoff-upper-scan-n; useful for older CLI checkouts")
    parser.add_argument("--dt-trace", type=float, default=1.0, help="DT_TRACE written into the generated input; default: 1.0 s")
    parser.add_argument("--max-trace-time", type=float, default=600.0, help="MAX_TRACE_TIME and CUTOFF_MAX_TRAJ_TIME written into the input; default: 600 s")
    parser.add_argument("--max-trace-distance", type=float, default=300.0, help="MAX_TRACE_DISTANCE written into the input in Re; default: 300")
    parser.add_argument("--upper-rel-tol", type=float, default=2.5e-1, help="UPPER_SCAN absolute relative-error tolerance vs Størmer at target points; default: 0.25")
    parser.add_argument("--collapse-factor", type=float, default=2.0, help="Rc < factor*Rmin is flagged as lower-boundary collapse; default: 2")
    parser.add_argument("--require-binary-collapse", action="store_true", help="also require BINARY to show the legacy lower-boundary collapse signature")
    parser.add_argument("--amps", default="./amps", help="AMPS executable path, relative to the launch directory; default: ./amps")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher executable; default: mpirun")
    parser.add_argument("--workdir", default=None, help="directory where the test is run and outputs are written; default: test_output/C3_<mode>")
    parser.add_argument("--output-file", default=None, help="explicit AMPS cutoff shell output file to parse; only useful with one algorithm or --skip-run")
    parser.add_argument("--skip-run", action="store_true", help="do not run AMPS; analyze existing algorithm subdirectories under --workdir")
    parser.add_argument("--keep", action="store_true", help="keep an existing work directory instead of replacing it")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.cutoff_scan_n < 1:
        raise SystemExit("--cutoff-scan-n must be >= 1")
    if args.max_trace_distance < 0.0:
        raise SystemExit("--max-trace-distance must be >= 0")

    algorithms = parse_algorithms(args.algorithms)
    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    if args.mode == "3d":
        template_input = script_dir / "AMPS_PARAM_C3_mode3d.in"
    else:
        template_input = script_dir / "AMPS_PARAM_C3_gridless.in"
    if not template_input.exists():
        template_input = script_dir / "AMPS_PARAM_C3.in"
    reference_csv = script_dir / "reference_C3_penumbra.csv"

    if args.workdir is None:
        if args.mode == "3d":
            args.workdir = "test_output/C3_3d_%s" % args.mode3d_field_eval.lower()
        else:
            args.workdir = "test_output/C3_gridless"
    root_workdir = (launch_dir / args.workdir).resolve()
    if not args.skip_run:
        if root_workdir.exists() and not args.keep:
            shutil.rmtree(root_workdir)
        root_workdir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(reference_csv, root_workdir / "reference_C3_penumbra.csv")
    else:
        if not root_workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % root_workdir)

    all_summary = []
    messages = []
    passed = True
    algo_outputs = {}

    for algorithm in algorithms:
        alg_dir = root_workdir / algorithm.lower()
        if not args.skip_run:
            alg_dir.mkdir(parents=True, exist_ok=True)
            render_input_template(
                template_input,
                alg_dir / "AMPS_PARAM_C3.in",
                args.nt,
                args.scheduler,
                args.dynamic_chunk,
                args.dt_trace,
                args.max_trace_time,
                args.max_trace_distance,
            )
            shutil.copy2(reference_csv, alg_dir / "reference_C3_penumbra.csv")

            amps_path = Path(args.amps)
            if not amps_path.is_absolute():
                amps_path = (launch_dir / amps_path).resolve()
            cmd = [args.mpirun, "-np", str(args.np), str(amps_path), "-mode", args.mode, "-i", "AMPS_PARAM_C3.in"]
            if not args.no_cutoff_search_cli:
                cmd += ["-cutoff-search", algorithm]
                if algorithm == "UPPER_SCAN":
                    cmd += ["-cutoff-upper-scan-n", str(args.cutoff_scan_n)]
            if args.mode == "3d":
                cmd += [
                    "-mode3d-field-eval", args.mode3d_field_eval,
                    "-mode3d-parallel", "THREADS",
                    "-mode3d-threads", str(args.nt),
                    "-mode3d-mpi-scheduler", args.scheduler,
                    "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
                ]
            else:
                cmd += [
                    "-gridless-mpi-scheduler", args.scheduler,
                    "-gridless-mpi-dynamic-chunk", str(args.dynamic_chunk),
                    "-density-parallel", "THREADS",
                    "-density-threads", str(args.nt),
                ]
            print("Running %s/%s in %s" % (TEST_ID, algorithm, alg_dir))
            print(" ".join(cmd))
            rc = run_command(cmd, cwd=alg_dir, log_path=alg_dir / ("C3_%s_amps.log" % algorithm.lower()))
            if rc != 0:
                print("AMPS command failed with exit code %d for %s" % (rc, algorithm), file=sys.stderr)
                return rc
        else:
            # --skip-run expects algorithm subdirectories, but also accepts the root
            # work directory for a single-algorithm manual analysis.
            if not alg_dir.exists() and len(algorithms) == 1:
                alg_dir = root_workdir
            if not alg_dir.exists():
                raise SystemExit("Missing algorithm directory for --skip-run: %s" % alg_dir)

        output_file = find_output_file(alg_dir, args.mode, args.output_file)
        if not output_file.exists():
            print("Expected AMPS output was not found for %s: %s" % (algorithm, output_file), file=sys.stderr)
            return 2
        rows = parse_tecplot_shell_output(output_file)
        summary, alg_passed, alg_messages = summarize_algorithm(algorithm, rows, args.upper_rel_tol, args.collapse_factor)
        all_summary.extend(summary)
        messages.extend(alg_messages)
        passed = passed and alg_passed
        algo_outputs[algorithm] = str(output_file)

    if args.require_binary_collapse and "BINARY" in algorithms:
        binary_collapsed = any(row["algorithm"] == "BINARY" and row["collapsed_to_low_boundary"] for row in all_summary)
        if not binary_collapsed:
            passed = False
            messages.append("--require-binary-collapse was set, but BINARY did not collapse to the low boundary")

    summary_csv = root_workdir / "C3_summary.csv"
    result_json = root_workdir / "C3_result.json"
    plot_png = root_workdir / "C3_penumbra_comparison.png"
    write_csv(all_summary, summary_csv)
    make_plot(all_summary, plot_png)

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
        "algorithms": algorithms,
        "cutoff_scan_n": args.cutoff_scan_n,
        "max_trace_time": args.max_trace_time,
        "max_trace_distance": args.max_trace_distance,
        "upper_rel_tol": args.upper_rel_tol,
        "rmin_gv": proton_rigidity_from_ekin_mev(CUTOFF_EMIN_MEV),
        "target_alt_km": TARGET_ALT_KM,
        "target_lats_deg": TARGET_LATS_DEG,
        "target_lons_deg": TARGET_LONS_DEG,
        "workdir": str(root_workdir),
        "algorithm_outputs": algo_outputs,
        "summary_csv": str(summary_csv),
        "plot_png": str(plot_png) if plot_png.exists() else None,
        "summary": all_summary,
    }
    result_json.write_text(json.dumps(result, indent=2))

    print("\nC3 summary")
    print("==========")
    print("mode=%s np=%d nt=%d scheduler=%s" % (args.mode, args.np, args.nt, args.scheduler))
    if args.mode == "3d":
        print("mode3d_field_eval=%s" % args.mode3d_field_eval)
    print("target alt=%g km; analytical Rc(lat=±60)=%g GV; Rmin=%g GV" % (
        TARGET_ALT_KM,
        stormer_vertical_gv(60.0, TARGET_ALT_KM),
        proton_rigidity_from_ekin_mev(CUTOFF_EMIN_MEV),
    ))
    for row in all_summary:
        flag = "COLLAPSED" if row["collapsed_to_low_boundary"] else ""
        print(
            "%10s lon=%6.1f lat=%6.1f Rc=%10.5e ref=%10.5e rel=% .3e %s" %
            (row["algorithm"], row["lon_deg"], row["lat_deg"], row["Rc_num_GV"], row["Rc_stormer_GV"], row["rel_error"], flag)
        )
    if messages:
        print("\nMessages:")
        for m in messages:
            print("  - " + m)
    print("\nWrote: %s" % summary_csv)
    print("Wrote: %s" % result_json)
    if plot_png.exists():
        print("Wrote: %s" % plot_png)
    print("\nRESULT: %s" % ("PASS" if passed else "FAIL"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
