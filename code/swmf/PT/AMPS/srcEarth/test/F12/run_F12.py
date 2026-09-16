#!/usr/bin/env python3
"""
F12 — day/night spatial boundary anisotropy identities.

Run from the directory containing the AMPS executable:

    python srcEarth/test/F12/run_F12.py -np 4 -nt 16

Examples:

    python srcEarth/test/F12/run_F12.py -np 1 -nt 1
    python srcEarth/test/F12/run_F12.py -np 4 -nt 16 --ratio-tol 1e-12
    python srcEarth/test/F12/run_F12.py --nintervals 160 --spectrum-tol 1e-12
    python srcEarth/test/F12/run_F12.py --dry-run
    python srcEarth/test/F12/run_F12.py --skip-run --workdir test_output/F12_gridless

F12 validates exact identities in the gridless ANISOTROPIC spatial boundary model:

  * DAYSIDE_NIGHTSIDE with BA_DAYSIDE_FACTOR=1 and BA_NIGHTSIDE_FACTOR=1 is
    identical to BA_SPATIAL_MODEL=UNIFORM.
  * For zero magnetic field, R_INNER=0, and diagnostic points on the GSM Y-Z
    plane (X=0), the deterministic direction grid is paired under x -> -x.
    Therefore DAY_ONLY=(1,0) and NIGHT_ONLY=(0,1) each produce exactly half
    the UNIFORM result, and their sum reconstructs UNIFORM.
  * A weighted case (2,0.5) has the analytical ratio (2+0.5)/2 = 1.25.

The runner compares density, total flux, channel fluxes, T(E), J_boundary(E),
and J_local(E) between the cases.
"""

from __future__ import print_function
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

TEST_ID = "F12"
TEST_NAME = "Day/night spatial boundary anisotropy identities"
CHANNELS = ["LOW", "MID", "HIGH", "FULL"]

CASES = [
    {"label": "UNIFORM", "spatial_model": "UNIFORM", "day": 1.0, "night": 1.0, "expected_ratio": 1.0},
    {"label": "DN_1_1", "spatial_model": "DAYSIDE_NIGHTSIDE", "day": 1.0, "night": 1.0, "expected_ratio": 1.0},
    {"label": "DAY_ONLY", "spatial_model": "DAYSIDE_NIGHTSIDE", "day": 1.0, "night": 0.0, "expected_ratio": 0.5},
    {"label": "NIGHT_ONLY", "spatial_model": "DAYSIDE_NIGHTSIDE", "day": 0.0, "night": 1.0, "expected_ratio": 0.5},
    {"label": "DAY2_NIGHT05", "spatial_model": "DAYSIDE_NIGHTSIDE", "day": 2.0, "night": 0.5, "expected_ratio": 1.25},
]


def _norm_key(s):
    return s.lower().replace(" ", "_").replace("-", "_").replace("^", "")


def _parse_variables(line):
    return [x.strip() for x in re.findall(r'"([^"]+)"', line)]


def _skip_line(line):
    s = line.strip()
    if not s:
        return True
    u = s.upper()
    return s.startswith("#") or s.startswith("!") or u.startswith("TITLE") or u.startswith("AUXDATA")


def read_table_file(path):
    records = []
    variables = []
    in_zone = False
    with path.open("r", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            u = line.upper()
            if u.startswith("VARIABLES"):
                variables = _parse_variables(line)
                continue
            if u.startswith("ZONE"):
                in_zone = True
                continue
            if _skip_line(line) or not in_zone:
                continue
            parts = line.split()
            if len(parts) != len(variables):
                continue
            try:
                records.append(dict((k, float(v)) for k, v in zip(variables, parts)))
            except ValueError:
                pass
    return variables, records


def read_spectrum_file(path):
    zones = []
    variables = []
    current = None
    with path.open("r", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            u = line.upper()
            if u.startswith("VARIABLES"):
                variables = _parse_variables(line)
                continue
            if u.startswith("ZONE"):
                if current is not None:
                    zones.append(current)
                current = {k: [] for k in variables}
                continue
            if _skip_line(line) or current is None:
                continue
            parts = line.split()
            if len(parts) != len(variables):
                continue
            try:
                for k, v in zip(variables, parts):
                    current[k].append(float(v))
            except ValueError:
                pass
    if current is not None:
        zones.append(current)
    return zones


def find_col(record, candidates):
    norm = {}
    for k in record.keys():
        norm[_norm_key(k)] = k
    for c in candidates:
        key = _norm_key(c)
        if key in norm:
            return norm[key]
    for c in candidates:
        key = _norm_key(c)
        for nk, orig in norm.items():
            if key in nk:
                return orig
    return None


def replace_key(text, key, value):
    return re.sub(r'(?m)^(\s*' + re.escape(key) + r'\s+)\S+', r'\g<1>' + str(value), text)


def render_input(template_path, output_path, args, case):
    text = template_path.read_text()
    for key, value in [
        ("RUN_ID", "F12_" + case["label"]),
        ("MODE3D_THREADS", args.nt),
        ("GRIDLESS_MPI_SCHEDULER", args.scheduler),
        ("GRIDLESS_MPI_DYNAMIC_CHUNK", args.dynamic_chunk),
        ("DS_NINTERVALS", args.nintervals),
        ("DS_MAX_TRAJ_TIME", args.max_trace_time),
        ("MAX_TRACE_TIME", args.max_trace_time),
        ("DT_TRACE", args.dt_trace),
        ("DS_TRANSMISSION_SCAN_N", args.scan_n),
        ("BA_SPATIAL_MODEL", case["spatial_model"]),
        ("BA_DAYSIDE_FACTOR", case["day"]),
        ("BA_NIGHTSIDE_FACTOR", case["night"]),
    ]:
        text = replace_key(text, key, value)
    text += ("\n! F12 harness settings\n"
             "! F12_CASE %s\n"
             "! F12_BA_SPATIAL_MODEL %s\n"
             "! F12_BA_DAYSIDE_FACTOR %.17g\n"
             "! F12_BA_NIGHTSIDE_FACTOR %.17g\n"
             "! F12_EXPECTED_RATIO %.17g\n"
             "! F12_NT %d\n"
             "! F12_GRIDLESS_MPI_SCHEDULER %s\n"
             "! F12_GRIDLESS_MPI_DYNAMIC_CHUNK %d\n"
             "! F12_DS_NINTERVALS %d\n") % (
                 case["label"], case["spatial_model"], case["day"], case["night"],
                 case["expected_ratio"], args.nt, args.scheduler, args.dynamic_chunk, args.nintervals)
    output_path.write_text(text)


def add_check(rows, check, case, quantity, value, expected_value, abs_tol=None, rel_tol=None,
              units="", check_type="error_metric", note=""):
    abs_err = abs(value - expected_value)
    rel_err = abs_err / max(abs(expected_value), 1.0e-300)
    ok = True
    if abs_tol is not None:
        ok = ok and (abs_err <= abs_tol)
    if rel_tol is not None:
        ok = ok and (rel_err <= rel_tol)
    rows.append({
        "check": check,
        "case": case,
        "quantity": quantity,
        "check_type": check_type,
        "passed": ok,
        "value": value,
        "expected_value": expected_value,
        "abs_error": abs_err,
        "rel_error": rel_err,
        "rel_tol": rel_tol,
        "abs_tol": abs_tol,
        "units": units,
        "note": note,
    })
    return ok


def vector_pair_stats(a, b, scale=1.0):
    if len(a) != len(b):
        return float("inf"), float("inf")
    max_abs = 0.0
    max_rel = 0.0
    for x, y in zip(a, b):
        expected = scale * y
        err = abs(x - expected)
        max_abs = max(max_abs, err)
        max_rel = max(max_rel, err / max(abs(expected), 1.0e-300))
    return max_abs, max_rel


def vector_sum_stats(a, b, c):
    if not (len(a) == len(b) == len(c)):
        return float("inf"), float("inf")
    max_abs = 0.0
    max_rel = 0.0
    for x, y, z in zip(a, b, c):
        err = abs((x + y) - z)
        max_abs = max(max_abs, err)
        max_rel = max(max_rel, err / max(abs(z), 1.0e-300))
    return max_abs, max_rel


def flatten_spectrum(case_data, key):
    out = []
    for zone in case_data["spectra"]:
        out.extend(zone[key])
    return out


def load_case(case_dir, label):
    density_path = case_dir / "gridless_points_density.dat"
    spectrum_path = case_dir / "gridless_points_spectrum.dat"
    flux_path = case_dir / "gridless_points_flux.dat"
    missing = [str(p) for p in (density_path, spectrum_path, flux_path) if not p.exists()]
    if missing:
        raise RuntimeError("Missing required AMPS output files for %s: %s" % (label, ", ".join(missing)))

    _, density_rows = read_table_file(density_path)
    _, flux_rows = read_table_file(flux_path)
    spectra = read_spectrum_file(spectrum_path)
    if len(density_rows) == 0:
        raise RuntimeError("No density rows found for %s" % label)
    if not (len(density_rows) == len(flux_rows) == len(spectra)):
        raise RuntimeError("Output size mismatch for %s: density=%d flux=%d spectra=%d" %
                           (label, len(density_rows), len(flux_rows), len(spectra)))

    n_col = find_col(density_rows[0], ["N_m^-3", "N_m3", "density", "n"])
    f_col = find_col(flux_rows[0], ["F_tot_m2s1", "F_total", "F_tot"])
    if n_col is None:
        raise RuntimeError("Could not identify density column for %s" % label)
    if f_col is None:
        raise RuntimeError("Could not identify total-flux column for %s" % label)

    channel_cols = {}
    for ch in CHANNELS:
        channel_cols[ch] = find_col(flux_rows[0], ["F_%s_m2s1" % ch, "F_%s" % ch])
        if channel_cols[ch] is None:
            raise RuntimeError("Could not identify flux channel %s for %s" % (ch, label))

    for iz, z in enumerate(spectra):
        for key in ("E_MeV", "T", "J_boundary_perMeV", "J_local_perMeV"):
            if key not in z:
                raise RuntimeError("Missing spectrum variable %s in case %s zone %d" % (key, label, iz))
        n = len(z["E_MeV"])
        if n < 2 or any(len(z[key]) != n for key in ("T", "J_boundary_perMeV", "J_local_perMeV")):
            raise RuntimeError("Malformed spectrum zone %d in case %s" % (iz, label))

    return {
        "label": label,
        "density": [r[n_col] for r in density_rows],
        "flux_total": [r[f_col] for r in flux_rows],
        "channels": dict((ch, [r[channel_cols[ch]] for r in flux_rows]) for ch in CHANNELS),
        "spectra": spectra,
        "T": flatten_spectrum({"spectra": spectra}, "T"),
        "Jb": flatten_spectrum({"spectra": spectra}, "J_boundary_perMeV"),
        "Jl": flatten_spectrum({"spectra": spectra}, "J_local_perMeV"),
    }


def check_scaled_case(rows, label, data, uniform, expected_ratio, args):
    passed = True
    for quantity, key, units in [
        ("density", "density", "m^-3"),
        ("flux_total", "flux_total", "m^-2 s^-1"),
        ("T", "T", "1"),
        ("J_boundary_perMeV", "Jb", "m^-2 s^-1 sr^-1 MeV^-1"),
        ("J_local_perMeV", "Jl", "m^-2 s^-1 sr^-1 MeV^-1"),
    ]:
        scale = expected_ratio
        # J_boundary is the incident spectrum and must be identical for all cases;
        # the spatial weight only changes T and therefore J_local/density/flux.
        if key == "Jb":
            scale = 1.0
        max_abs, max_rel = vector_pair_stats(data[key], uniform[key], scale=scale)
        tol = args.spectrum_tol if key in ("T", "Jb", "Jl") else args.ratio_tol
        passed = add_check(rows, "scaled_case", label, quantity + "_max_rel_error",
                           max_rel, 0.0, abs_tol=tol, units="1",
                           check_type="exact_identity" if expected_ratio == 1.0 else "analytic_reference",
                           note="%s should equal %.12g times UNIFORM" % (quantity, scale)) and passed
    for ch in CHANNELS:
        max_abs, max_rel = vector_pair_stats(data["channels"][ch], uniform["channels"][ch], scale=expected_ratio)
        passed = add_check(rows, "scaled_case", label, "F_%s_max_rel_error" % ch,
                           max_rel, 0.0, abs_tol=args.ratio_tol, units="1",
                           check_type="analytic_reference" if expected_ratio != 1.0 else "exact_identity",
                           note="channel flux should equal %.12g times UNIFORM" % expected_ratio) and passed
    return passed


def check_complement(rows, day, night, uniform, args):
    passed = True
    for quantity, key in [("density", "density"), ("flux_total", "flux_total"), ("T", "T"), ("J_local_perMeV", "Jl")]:
        tol = args.spectrum_tol if key in ("T", "Jl") else args.ratio_tol
        max_abs, max_rel = vector_sum_stats(day[key], night[key], uniform[key])
        passed = add_check(rows, "complement_sum", "DAY_ONLY+NIGHT_ONLY", quantity + "_max_rel_residual",
                           max_rel, 0.0, abs_tol=tol, units="1", check_type="exact_identity",
                           note="DAY_ONLY + NIGHT_ONLY must reconstruct UNIFORM") and passed
    for ch in CHANNELS:
        max_abs, max_rel = vector_sum_stats(day["channels"][ch], night["channels"][ch], uniform["channels"][ch])
        passed = add_check(rows, "complement_sum", "DAY_ONLY+NIGHT_ONLY", "F_%s_max_rel_residual" % ch,
                           max_rel, 0.0, abs_tol=args.ratio_tol, units="1", check_type="exact_identity",
                           note="one-sided channel fluxes must reconstruct the uniform channel flux") and passed
    return passed


def check_boundary_nonzero(rows, uniform, args):
    values = uniform["Jb"]
    min_jb = min(values) if values else -float("inf")
    present = 1.0 if min_jb > args.min_boundary_j else 0.0
    return add_check(rows, "boundary_spectrum_nonzero", "UNIFORM", "min_J_boundary_positive",
                     present, 1.0, abs_tol=0.0, units="1", check_type="setup_identity",
                     note="incident spectrum must be nonzero so F12 tests spatial weighting, not zero input")


def analyze(workdir, args):
    data = {}
    for case in CASES:
        label = case["label"]
        data[label] = load_case(workdir / label, label)

    uniform = data["UNIFORM"]
    rows = []
    passed = True
    passed = check_boundary_nonzero(rows, uniform, args) and passed

    for case in CASES:
        label = case["label"]
        passed = check_scaled_case(rows, label, data[label], uniform, case["expected_ratio"], args) and passed

    passed = check_complement(rows, data["DAY_ONLY"], data["NIGHT_ONLY"], uniform, args) and passed

    summary_csv = workdir / "F12_summary.csv"
    fieldnames = ["check", "case", "quantity", "check_type", "passed", "value",
                  "expected_value", "abs_error", "rel_error", "rel_tol", "abs_tol", "units", "note"]
    with summary_csv.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for row in rows:
            w.writerow(row)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "n_checks": len(rows),
        "n_failed": sum(1 for r in rows if not r["passed"]),
        "workdir": str(workdir),
        "summary_csv": str(summary_csv),
        "cases": CASES,
        "parameters": {
            "nintervals": args.nintervals,
            "ratio_tol": args.ratio_tol,
            "spectrum_tol": args.spectrum_tol,
            "min_boundary_j": args.min_boundary_j,
        },
        "failed_checks": [r for r in rows if not r["passed"]],
    }
    result_json = workdir / "F12_result.json"
    with result_json.open("w") as f:
        json.dump(result, f, indent=2, sort_keys=True)
        f.write("\n")

    print("F12 %s" % ("PASS" if passed else "FAIL"))
    print("Summary:", summary_csv)
    print("Result :", result_json)
    if not passed:
        print("Failed checks:")
        for r in rows:
            if not r["passed"]:
                print("  - {check} {case} {quantity}: value={value:.6e}, expected={expected_value:.6e}, abs={abs_error:.3e}".format(**r))
    return passed


def parse_args(argv=None):
    p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        F12: day/night spatial boundary anisotropy identities.

        The test uses zero field, no inner absorber, and X=0 diagnostic points so
        the sampled exit directions are exactly paired by x -> -x.  It runs
        UNIFORM, DAYSIDE_NIGHTSIDE(1,1), DAY_ONLY(1,0), NIGHT_ONLY(0,1), and
        DAYSIDE_NIGHTSIDE(2,0.5), then compares the expected exact ratios and
        complement identities in density, flux, channels, T(E), and spectra.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/F12/run_F12.py
          python srcEarth/test/F12/run_F12.py -np 4 -nt 16
          python srcEarth/test/F12/run_F12.py --ratio-tol 1e-12 --spectrum-tol 1e-12
          python srcEarth/test/F12/run_F12.py --dry-run --workdir test_output/F12_dryrun
          python srcEarth/test/F12/run_F12.py --skip-run --workdir test_output/F12_gridless
        """))
    p.add_argument("-np", type=int, default=4, help="number of MPI ranks; default: 4")
    p.add_argument("-nt", type=int, default=16, help="threads per rank; default: 16")
    p.add_argument("--amps", default="./amps", help="AMPS executable path; default: ./amps")
    p.add_argument("--mpirun", default="mpirun", help="MPI launcher; default: mpirun")
    p.add_argument("--workdir", default="test_output/F12_gridless", help="run/output directory")
    p.add_argument("--scheduler", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], default="DYNAMIC")
    p.add_argument("--dynamic-chunk", type=int, default=0)
    p.add_argument("--scan-n", type=int, default=80, help="reserved compatibility option; DIRECT mode ignores it")
    p.add_argument("--nintervals", type=int, default=80, help="DS_NINTERVALS value; default: 80")
    p.add_argument("--dt-trace", type=float, default=0.05, help="DT_TRACE value")
    p.add_argument("--max-trace-time", type=float, default=5.0, help="MAX_TRACE_TIME / DS_MAX_TRAJ_TIME value")
    p.add_argument("--ratio-tol", type=float, default=1.0e-12, help="absolute tolerance on relative ratio/complement residuals")
    p.add_argument("--spectrum-tol", type=float, default=1.0e-12, help="absolute tolerance on T(E)/spectrum relative residuals")
    p.add_argument("--min-boundary-j", type=float, default=0.0, help="minimum accepted boundary spectrum value")
    p.add_argument("--skip-run", action="store_true", help="analyze existing files in --workdir")
    p.add_argument("--keep", action="store_true", help="do not delete an existing workdir")
    p.add_argument("--dry-run", action="store_true", help="render inputs and print AMPS commands without executing them")
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.nintervals < 2:
        raise SystemExit("--nintervals must be >= 2")
    if args.scan_n < 2:
        raise SystemExit("--scan-n must be >= 2")
    if args.ratio_tol < 0.0 or args.spectrum_tol < 0.0:
        raise SystemExit("tolerances must be non-negative")

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template = script_dir / "AMPS_PARAM_F12_gridless.in"
    reference_repo_csv = script_dir / "reference_F12_daynight_step.csv"
    workdir = (launch_dir / args.workdir).resolve()

    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(str(workdir))
        workdir.mkdir(parents=True, exist_ok=True)
        for case in CASES:
            case_dir = workdir / case["label"]
            case_dir.mkdir(parents=True, exist_ok=True)
            render_input(template, case_dir / "AMPS_PARAM_F12.in", args, case)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    if reference_repo_csv.exists() and not args.skip_run:
        shutil.copy2(str(reference_repo_csv), str(workdir / "reference_F12_daynight_step_used.csv"))

    if not args.skip_run:
        amps_path = Path(args.amps)
        if not amps_path.is_absolute():
            amps_path = (launch_dir / amps_path).resolve()
        for case in CASES:
            case_dir = workdir / case["label"]
            cmd = [
                args.mpirun, "-np", str(args.np), str(amps_path),
                "-mode", "gridless",
                "-i", "AMPS_PARAM_F12.in",
                "-mode3d-parallel", "THREADS",
                "-mode3d-threads", str(args.nt),
                "-mode3d-mpi-scheduler", args.scheduler,
                "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
                "-density-mode", "ANISOTROPIC",
                "-density-transmission", "DIRECT",
            ]
            print("Running %s: %s" % (case["label"], " ".join(cmd)))
            if args.dry_run:
                continue
            with (case_dir / "F12_amps.log").open("w") as log:
                log.write("# Command: %s\n" % " ".join(cmd))
                log.flush()
                rc = subprocess.call(cmd, cwd=str(case_dir), stdout=log, stderr=subprocess.STDOUT)
            if rc != 0:
                print("AMPS failed for %s with exit code %d; see %s" %
                      (case["label"], rc, case_dir / "F12_amps.log"))
                return rc
        if args.dry_run:
            print("F12 dry run complete. Generated inputs under %s" % workdir)
            return 0

    ok = analyze(workdir, args)
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
