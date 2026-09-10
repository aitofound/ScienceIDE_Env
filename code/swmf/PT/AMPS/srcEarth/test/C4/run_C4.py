#!/usr/bin/env python3
"""
C4 — Mode3D trajectory-exit and invariant diagnostic.

Run from the directory that contains the AMPS executable:

    srcEarth/test/C4/run_C4.py -np 4 -nt 16 --factors=0.5,2.0 --lats=-60,-30,0,30,60

C4 is not a shell-map cutoff-accuracy test.  C1/C2/C3 already cover the
Størmer cutoff and penumbra behavior.  C4 uses the Mode3D
CUTOFF_DEBUG_EXIT_LIST_FILE capability to trace many selected vertical
trajectories in one AMPS run and then checks the terminal classifier and
single-particle invariants in the combined diagnostic file.
"""

from __future__ import print_function

import argparse
import csv
import json
import math
import os
import re
import shlex
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

TEST_ID = "C4"
TEST_NAME = "Mode3D trajectory-exit classifier and invariant diagnostic"

RE_KM = 6371.2
STORMER_R0_GV = 0.299792458 * 0.25 * 3.12e-5 * (RE_KM * 1000.0)
DEFAULT_ALT_KM = 9000.0
DEFAULT_LATS = (-60.0, -30.0, 0.0, 30.0, 60.0)
DEFAULT_LONS = (0.0,)
DEFAULT_FACTORS = (0.5, 2.0)


def stormer_vertical_gv(lat_deg, alt_km):
    """Independent vertical Størmer cutoff reference in GV."""
    r_re = (RE_KM + alt_km) / RE_KM
    c4 = math.cos(math.radians(lat_deg)) ** 4
    return STORMER_R0_GV * c4 / (r_re * r_re)


def parse_float_list(value, name):
    out = []
    for item in str(value).split(","):
        item = item.strip()
        if not item:
            continue
        try:
            out.append(float(item))
        except ValueError:
            raise SystemExit("Could not parse %s value '%s'" % (name, item))
    if not out:
        raise SystemExit("No values supplied for %s" % name)
    return out


def parse_string_list(value, name):
    out = []
    for item in str(value).split(","):
        item = item.strip().upper()
        if item:
            out.append(item)
    if not out:
        raise SystemExit("No values supplied for %s" % name)
    return out


def safe_label_token(value):
    return re.sub(r"[^A-Za-z0-9_+.-]+", "_", str(value))


def number_token(value):
    return safe_label_token(("%.8g" % value).replace("-", "m").replace(".", "p"))


def write_exit_case_list(path, lons, lats, alt_km, factors):
    """Write the many-trajectory list consumed by CUTOFF_DEBUG_EXIT_LIST_FILE."""
    cases = []
    with path.open("w") as f:
        f.write("# C4 trajectory-exit diagnostic list\n")
        f.write("# columns: lon_deg lat_deg alt_km R_GV label\n")
        f.write("# R_GV is absolute rigidity.  The Python harness generated it as factor*Rc_Stormer.\n")
        for lon in lons:
            for lat in lats:
                rc = stormer_vertical_gv(lat, alt_km)
                for factor in factors:
                    r_gv = factor * rc
                    label = "lon%s_lat%s_f%s" % (number_token(lon), number_token(lat), number_token(factor))
                    f.write("%.12g %.12g %.12g %.12e %s\n" % (lon, lat, alt_km, r_gv, label))
                    cases.append({
                        "case_id": len(cases),
                        "label": label,
                        "lon0_deg": lon,
                        "lat0_deg": lat,
                        "alt0_km": alt_km,
                        "factor": factor,
                        "R_GV": r_gv,
                        "Rc_stormer_GV": rc,
                        "expected_allowed": bool(factor >= 1.0),
                    })
    return cases


def write_csv(rows, path):
    if not rows:
        return
    keys = []
    for row in rows:
        for key in row.keys():
            if key not in keys:
                keys.append(key)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for row in rows:
            w.writerow(row)


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


def render_input_template(template_path, output_path, dt_trace, adaptive_dt, max_trace_time, max_trace_distance, alt_km, shell_res):
    text = template_path.read_text()
    text = _replace_or_append_keyword(text, "FIELD_EVAL_METHOD", "GRID_3D", "#CALCULATION_MODE")
    text = _replace_or_append_keyword(text, "DT_TRACE", "%.12g" % dt_trace, "#NUMERICAL")
    text = _replace_or_append_keyword(text, "ADAPTIVE_DT", "T" if adaptive_dt else "F", "#NUMERICAL")
    text = _replace_or_append_keyword(text, "MAX_TRACE_TIME", "%.12g" % max_trace_time, "#NUMERICAL")
    text = _replace_or_append_keyword(text, "MAX_TRACE_DISTANCE", "%.12g" % max_trace_distance, "#NUMERICAL")
    text = _replace_or_append_keyword(text, "CUTOFF_MAX_TRAJ_TIME", "%.12g" % max_trace_time, "#CUTOFF_RIGIDITY")
    text = _replace_or_append_keyword(text, "SHELL_ALTS_KM", "%.12g" % alt_km, "#OUTPUT_DOMAIN")
    text = _replace_or_append_keyword(text, "SHELL_RES_DEG", "%.12g" % shell_res, "#OUTPUT_DOMAIN")
    # Keep the unavoidable production shell calculation very small; C4 analyzes the
    # debug-exit file, not the shell cutoff map.
    text = _replace_or_append_keyword(text, "CUTOFF_DEBUG_EXIT_TRACE", "T", "#CUTOFF_RIGIDITY")
    text = _replace_or_append_keyword(text, "CUTOFF_DEBUG_EXIT_LIST_FILE", "c4_debug_trajectories.dat", "#CUTOFF_RIGIDITY")
    text = _replace_or_append_keyword(text, "CUTOFF_DEBUG_EXIT_FILE", "cutoff_3d_debug_exit_trace.dat", "#CUTOFF_RIGIDITY")
    output_path.write_text(text)


def parse_variables(line):
    return [x.strip() for x in re.findall(r'"([^"]+)"', line)]


def parse_debug_exit_output(path):
    variables = []
    rows = []
    with path.open("r", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            upper = line.upper()
            if upper.startswith("TITLE"):
                continue
            if upper.startswith("VARIABLES"):
                variables = parse_variables(line)
                continue
            if upper.startswith("ZONE"):
                continue
            if not variables:
                continue
            try:
                parts = shlex.split(line)
            except ValueError:
                continue
            if len(parts) < len(variables):
                continue
            row = dict(zip(variables, parts))
            rows.append(row)
    if not rows:
        raise RuntimeError("No trajectory rows parsed from %s" % path)
    return rows


def as_float(row, key, default=float("nan")):
    try:
        return float(row.get(key, default))
    except Exception:
        return default


def as_int(row, key, default=0):
    try:
        return int(float(row.get(key, default)))
    except Exception:
        return default


def evaluate_rows(rows, dR_tol, paxis_tol, fail_on_paxis):
    summary = []
    passed = True
    messages = []
    for row in rows:
        case_id = as_int(row, "case_id", len(summary))
        label = row.get("label", "case%d" % case_id)
        lon = as_float(row, "lon0_deg")
        lat = as_float(row, "lat0_deg")
        alt = as_float(row, "alt0_km")
        r_gv = as_float(row, "R_GV")
        rc = as_float(row, "Rc_stormer_GV")
        allowed = as_int(row, "allowed")
        reason = row.get("reason", "UNKNOWN")
        rel_dr = abs(as_float(row, "rel_dR"))
        rel_dp = abs(as_float(row, "rel_dP_axis"))
        expected_allowed = bool(rc > 0.0 and r_gv >= rc)

        ok_classifier = (allowed == 1 and reason == "OUTER_BOX") if expected_allowed else (allowed == 0 and reason != "OUTER_BOX")
        ok_dr = math.isfinite(rel_dr) and rel_dr <= dR_tol
        ok_dp = True
        if fail_on_paxis:
            ok_dp = math.isfinite(rel_dp) and rel_dp <= paxis_tol
        ok = ok_classifier and ok_dr and ok_dp
        if not ok:
            passed = False
            bits = []
            if not ok_classifier:
                bits.append("classifier expected %s but got allowed=%d reason=%s" % ("OUTER_BOX" if expected_allowed else "not OUTER_BOX", allowed, reason))
            if not ok_dr:
                bits.append("|rel_dR|=%.3e > %.3e" % (rel_dr, dR_tol))
            if not ok_dp:
                bits.append("|rel_dP_axis|=%.3e > %.3e" % (rel_dp, paxis_tol))
            messages.append("%s: %s" % (label, "; ".join(bits)))

        out = {
            "case_id": case_id,
            "label": label,
            "lon0_deg": lon,
            "lat0_deg": lat,
            "alt0_km": alt,
            "R_GV": r_gv,
            "Rc_stormer_GV": rc,
            "R_over_Rc": r_gv / rc if rc > 0.0 else float("nan"),
            "expected_allowed": expected_allowed,
            "allowed": allowed,
            "reason": reason,
            "rel_dR_abs": rel_dr,
            "rel_dP_axis_abs": rel_dp,
            "n_steps": as_int(row, "n_steps"),
            "t_s": as_float(row, "t_s"),
            "path_Re": as_float(row, "path_Re"),
            "face": row.get("face", ""),
            "passed": ok,
        }
        summary.append(out)
    return summary, passed, messages


def normalize_negative_list_args(argv):
    # argparse treats '-60,-30,...' as another option when supplied as
    # '--lats -60,-30,...'.  Convert the common list options to '--lats=...' before
    # parsing so the user does not have to remember the '=' form.
    list_opts = {"--lats", "--target-lats", "--lons", "--target-lons", "--factors", "--dt-sweep"}
    out = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in list_opts and i + 1 < len(argv):
            out.append(a + "=" + argv[i + 1])
            i += 2
        else:
            out.append(a)
            i += 1
    return out


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        C4: Mode3D trajectory-exit classifier and invariant diagnostic

        The harness builds one c4_debug_trajectories.dat file containing all
        requested lon/lat/alt/rigidity cases, launches AMPS once per mover/DT_TRACE
        configuration, and analyzes the single combined cutoff_3d_debug_exit_trace.dat
        file written by AMPS rank 0.
        """),
        epilog=textwrap.dedent("""
        Examples:
          srcEarth/test/C4/run_C4.py
          srcEarth/test/C4/run_C4.py --factors=0.5,2.0 --lats=-60,-30,0,30,60
          srcEarth/test/C4/run_C4.py --adaptive-dt F --dt-sweep=1.0,0.5,0.25
          srcEarth/test/C4/run_C4.py --fail-on-paxis --paxis-tol=1e-5
        """)
    )
    parser.add_argument("-np", type=int, default=4, help="number of MPI processes passed to mpirun; default: 4")
    parser.add_argument("-nt", type=int, default=16, help="number of Mode3D worker threads per MPI rank; default: 16")
    parser.add_argument("--mode", choices=["3d"], default="3d", help="AMPS mode to test; C4 uses Mode3D debug-exit diagnostics")
    parser.add_argument("--mode3d-field-eval", "--field-eval", dest="mode3d_field_eval", default="ANALYTIC", choices=["ANALYTIC", "MESH", "GRID_3D"], help="Mode3D field backend; default: ANALYTIC")
    parser.add_argument("--scheduler", default="DYNAMIC", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], help="MPI scheduler for the normal Mode3D calculation; default: DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0, help="dynamic MPI chunk size; 0 means AMPS auto heuristic; default: 0")
    parser.add_argument("--alt", type=float, default=DEFAULT_ALT_KM, help="trajectory start altitude in km; default: 9000")
    parser.add_argument("--lats", "--target-lats", dest="lats", default=",".join(["%g" % x for x in DEFAULT_LATS]), help="comma-separated start latitudes; default: -60,-30,0,30,60")
    parser.add_argument("--lons", "--target-lons", dest="lons", default=",".join(["%g" % x for x in DEFAULT_LONS]), help="comma-separated start longitudes; default: 0")
    parser.add_argument("--factors", default=",".join(["%g" % x for x in DEFAULT_FACTORS]), help="comma-separated R/Rc_Stormer factors; default: 0.5,2.0")
    parser.add_argument("--dt-sweep", default="0.25", help="comma-separated DT_TRACE values; each value is one AMPS run; default: 0.25")
    parser.add_argument("--movers", default="BORIS", help="comma-separated particle movers; each mover is one AMPS run; default: BORIS")
    parser.add_argument("--adaptive-dt", default="T", choices=["T", "F", "t", "f", "TRUE", "FALSE", "true", "false", "1", "0"], help="ADAPTIVE_DT passed to AMPS; default: T")
    parser.add_argument("--max-trace-time", type=float, default=600.0, help="MAX_TRACE_TIME and CUTOFF_MAX_TRAJ_TIME in seconds; default: 600")
    parser.add_argument("--max-trace-distance", type=float, default=400.0, help="MAX_TRACE_DISTANCE in Re; default: 400")
    parser.add_argument("--shell-res", type=float, default=180.0, help="SHELL_RES_DEG for the unavoidable small production shell calculation; default: 180")
    parser.add_argument("--dR-tol", type=float, default=1.0e-6, help="maximum allowed |rel_dR|; default: 1e-6")
    parser.add_argument("--paxis-tol", type=float, default=1.0e-5, help="maximum allowed |rel_dP_axis| when --fail-on-paxis is set; default: 1e-5")
    parser.add_argument("--fail-on-paxis", action="store_true", help="make canonical P_axis drift a hard failure instead of a recorded diagnostic")
    parser.add_argument("--amps", default="./amps", help="AMPS executable path, relative to launch directory; default: ./amps")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher executable; default: mpirun")
    parser.add_argument("--workdir", default=None, help="directory where the test is run and outputs are written; default: test_output/C4_exit")
    parser.add_argument("--skip-run", action="store_true", help="do not run AMPS; analyze existing case subdirectories under --workdir")
    parser.add_argument("--keep", action="store_true", help="keep existing work directory instead of replacing it")
    return parser.parse_args(normalize_negative_list_args(sys.argv[1:] if argv is None else argv))


def main():
    args = parse_args()
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.max_trace_time <= 0.0:
        raise SystemExit("--max-trace-time must be > 0")
    if args.max_trace_distance < 0.0:
        raise SystemExit("--max-trace-distance must be >= 0")

    adaptive_dt = str(args.adaptive_dt).upper() in ("T", "TRUE", "1")
    dt_values = parse_float_list(args.dt_sweep, "--dt-sweep")
    movers = parse_string_list(args.movers, "--movers")
    lats = parse_float_list(args.lats, "--lats")
    lons = parse_float_list(args.lons, "--lons")
    factors = parse_float_list(args.factors, "--factors")
    for lat in lats:
        if lat < -90.0 or lat > 90.0:
            raise SystemExit("all --lats values must be in [-90,90]")
    for dt in dt_values:
        if dt <= 0.0:
            raise SystemExit("all --dt-sweep values must be > 0")
    for factor in factors:
        if factor <= 0.0:
            raise SystemExit("all --factors values must be > 0")

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template_input = script_dir / "AMPS_PARAM_C4_mode3d.in"
    if not template_input.exists():
        template_input = script_dir / "AMPS_PARAM_C4.in"

    if args.workdir is None:
        args.workdir = "test_output/C4_exit"
    root_workdir = (launch_dir / args.workdir).resolve()
    if not args.skip_run:
        if root_workdir.exists() and not args.keep:
            shutil.rmtree(root_workdir)
        root_workdir.mkdir(parents=True, exist_ok=True)
    elif not root_workdir.exists():
        raise SystemExit("--skip-run requested but workdir does not exist: %s" % root_workdir)

    amps_path = Path(args.amps)
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()

    all_summary = []
    case_metrics = []
    messages = []
    passed = True

    for mover in movers:
        for dt in dt_values:
            dt_token = number_token(dt)
            mover_token = safe_label_token(mover.lower())
            label = "%s_dt_%s" % (mover_token, dt_token)
            case_dir = root_workdir / label
            expected_count = None
            if not args.skip_run:
                case_dir.mkdir(parents=True, exist_ok=True)
                render_input_template(
                    template_input,
                    case_dir / "AMPS_PARAM_C4.in",
                    dt,
                    adaptive_dt,
                    args.max_trace_time,
                    args.max_trace_distance,
                    args.alt,
                    args.shell_res,
                )
                list_cases = write_exit_case_list(case_dir / "c4_debug_trajectories.dat", lons, lats, args.alt, factors)
                expected_count = len(list_cases)
                write_csv(list_cases, case_dir / "C4_expected_cases.csv")
                cmd = [
                    args.mpirun, "-np", str(args.np), str(amps_path),
                    "-mode", "3d",
                    "-i", "AMPS_PARAM_C4.in",
                    "-mover", mover,
                    "-cutoff-search", "UPPER_SCAN",
                    "-cutoff-upper-scan-n", "160",
                    "-adaptive-dt", "T" if adaptive_dt else "F",
                    "-mode3d-field-eval", args.mode3d_field_eval,
                    "-mode3d-parallel", "THREADS",
                    "-mode3d-threads", str(args.nt),
                    "-mode3d-mpi-scheduler", args.scheduler,
                    "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
                    "-cutoff-debug-exit-list", "c4_debug_trajectories.dat",
                    "-cutoff-debug-exit-file", "cutoff_3d_debug_exit_trace.dat",
                ]
                print("Running C4 case %s in %s" % (label, case_dir))
                print(" ".join(cmd))
                rc = run_command(cmd, cwd=case_dir, log_path=case_dir / "C4_amps.log")
                if rc != 0:
                    print("AMPS command failed with exit code %d for %s" % (rc, label), file=sys.stderr)
                    return rc
            else:
                if not case_dir.exists():
                    raise SystemExit("Missing case directory for --skip-run: %s" % case_dir)
                expected_csv = case_dir / "C4_expected_cases.csv"
                if expected_csv.exists():
                    with expected_csv.open("r", newline="") as f:
                        expected_count = sum(1 for _ in csv.DictReader(f))

            output_file = case_dir / "cutoff_3d_debug_exit_trace.dat"
            if not output_file.exists():
                raise RuntimeError("Missing debug-exit output file: %s" % output_file)
            rows = parse_debug_exit_output(output_file)
            summary, case_passed, case_messages = evaluate_rows(rows, args.dR_tol, args.paxis_tol, args.fail_on_paxis)
            if expected_count is not None and len(summary) != expected_count:
                case_passed = False
                case_messages.append("parsed %d trajectory rows, expected %d" % (len(summary), expected_count))
            for row in summary:
                row["case"] = label
                row["mover"] = mover
                row["dt_trace"] = dt
                row["output_file"] = str(output_file)
            all_summary.extend(summary)
            max_dr = max([r["rel_dR_abs"] for r in summary]) if summary else float("nan")
            max_dp = max([r["rel_dP_axis_abs"] for r in summary if math.isfinite(r["rel_dP_axis_abs"])], default=float("nan"))
            n_fail = sum([0 if r["passed"] else 1 for r in summary])
            metrics = {
                "case": label,
                "mover": mover,
                "dt_trace": dt,
                "n_trajectories": len(summary),
                "n_failed": n_fail,
                "max_abs_rel_dR": max_dr,
                "max_abs_rel_dP_axis": max_dp,
                "passed": bool(case_passed),
            }
            case_metrics.append(metrics)
            if not case_passed:
                passed = False
                messages.extend(["%s: %s" % (label, m) for m in case_messages])

    summary_csv = root_workdir / "C4_summary.csv"
    metrics_csv = root_workdir / "C4_case_metrics.csv"
    result_json = root_workdir / "C4_result.json"
    write_csv(all_summary, summary_csv)
    write_csv(case_metrics, metrics_csv)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "messages": messages,
        "np": args.np,
        "nt": args.nt,
        "mode": "3d",
        "mode3d_field_eval": args.mode3d_field_eval,
        "scheduler": args.scheduler,
        "dynamic_chunk": args.dynamic_chunk,
        "alt_km": args.alt,
        "lats_deg": lats,
        "lons_deg": lons,
        "factors": factors,
        "dt_values": dt_values,
        "movers": movers,
        "adaptive_dt": adaptive_dt,
        "max_trace_time": args.max_trace_time,
        "max_trace_distance": args.max_trace_distance,
        "dR_tol": args.dR_tol,
        "paxis_tol": args.paxis_tol,
        "fail_on_paxis": args.fail_on_paxis,
        "workdir": str(root_workdir),
        "summary_csv": str(summary_csv),
        "metrics_csv": str(metrics_csv),
        "case_metrics": case_metrics,
    }
    result_json.write_text(json.dumps(result, indent=2))

    print("\nC4 summary")
    print("==========")
    print("mode=3d np=%d nt=%d scheduler=%s field_eval=%s" % (args.np, args.nt, args.scheduler, args.mode3d_field_eval))
    print("alt=%g km adaptive_dt=%s max_time=%g s max_distance=%g Re" % (args.alt, "T" if adaptive_dt else "F", args.max_trace_time, args.max_trace_distance))
    print("lons=%s lats=%s factors=%s" % (",".join(map(str,lons)), ",".join(map(str,lats)), ",".join(map(str,factors))))
    for row in sorted(case_metrics, key=lambda r: (r.get("mover", ""), r["dt_trace"])):
        print(
            "mover=%-6s  DT_TRACE=%8.4g  n=%3d  failed=%3d  max|rel_dR|=%9.3e  max|rel_dP|=%9.3e  %s" %
            (row.get("mover", ""), row["dt_trace"], row["n_trajectories"], row["n_failed"], row["max_abs_rel_dR"], row["max_abs_rel_dP_axis"], "PASS" if row["passed"] else "FAIL")
        )
    if messages:
        print("\nMessages:")
        for m in messages:
            print("  - " + m)
    print("\nWrote: %s" % summary_csv)
    print("Wrote: %s" % metrics_csv)
    print("Wrote: %s" % result_json)
    print("\nRESULT: %s" % ("PASS" if passed else "FAIL"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
