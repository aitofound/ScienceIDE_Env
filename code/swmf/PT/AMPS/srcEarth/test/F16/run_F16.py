#!/usr/bin/env python3
"""
F16 — blocked-access zero-flux regression.

Run from the directory containing the AMPS executable:

    python srcEarth/test/F16/run_F16.py -np 4 -nt 16

F16 places every diagnostic point inside R_INNER.  The shared gridless trajectory
classifier checks the inner absorbing sphere before the first particle push, so
all sampled directions and energies must be forbidden:

    T(E)       = 0
    J_local(E) = T(E) * J_boundary(E) = 0
    n          = 0
    F          = 0

The incident boundary spectrum remains nonzero, which makes this a true blocked
access test rather than a zero-input spectrum test.
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

TEST_ID = "F16"
TEST_NAME = "Blocked-access zero-flux regression"

CHANNELS = ["LOW", "MID", "HIGH", "FULL"]


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
        norm[k.lower().replace(" ", "_").replace("-", "_").replace("^", "")] = k
    for c in candidates:
        key = c.lower().replace(" ", "_").replace("-", "_").replace("^", "")
        if key in norm:
            return norm[key]
    for c in candidates:
        key = c.lower().replace(" ", "_").replace("-", "_").replace("^", "")
        for nk, orig in norm.items():
            if key in nk:
                return orig
    return None


def replace_key(text, key, value):
    return re.sub(r'(?m)^(\s*' + re.escape(key) + r'\s+)\S+', r'\g<1>' + str(value), text)


def render_input(template_path, output_path, args):
    text = template_path.read_text()
    for key, value in [
        ("MODE3D_THREADS", args.nt),
        ("GRIDLESS_MPI_SCHEDULER", args.scheduler),
        ("GRIDLESS_MPI_DYNAMIC_CHUNK", args.dynamic_chunk),
        ("DS_NINTERVALS", args.nintervals),
        ("DS_MAX_TRAJ_TIME", args.max_trace_time),
        ("MAX_TRACE_TIME", args.max_trace_time),
        ("DT_TRACE", args.dt_trace),
        ("R_INNER", args.r_inner_km),
    ]:
        text = replace_key(text, key, value)
    text += ("\n! F16 harness settings\n"
             "! F16_NT %d\n"
             "! F16_R_INNER_KM %.17g\n"
             "! F16_GRIDLESS_MPI_SCHEDULER %s\n"
             "! F16_GRIDLESS_MPI_DYNAMIC_CHUNK %d\n"
             "! F16_DS_NINTERVALS %d\n") % (
                 args.nt, args.r_inner_km, args.scheduler, args.dynamic_chunk, args.nintervals)
    output_path.write_text(text)


def add_check(rows, check, quantity, value, expected_value, abs_tol=None, rel_tol=None,
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


def analyze(workdir, args):
    density_path = workdir / "gridless_points_density.dat"
    spectrum_path = workdir / "gridless_points_spectrum.dat"
    flux_path = workdir / "gridless_points_flux.dat"
    missing = [str(p) for p in (density_path, spectrum_path, flux_path) if not p.exists()]
    if missing:
        raise RuntimeError("Missing required AMPS output files: " + ", ".join(missing))

    _, density_rows = read_table_file(density_path)
    _, flux_rows = read_table_file(flux_path)
    spectra = read_spectrum_file(spectrum_path)

    if len(density_rows) != len(flux_rows):
        raise RuntimeError("Density/flux row-count mismatch: %d vs %d" % (len(density_rows), len(flux_rows)))
    if len(density_rows) != len(spectra):
        raise RuntimeError("Density/spectrum zone-count mismatch: %d vs %d" % (len(density_rows), len(spectra)))
    if len(density_rows) == 0:
        raise RuntimeError("No output points found")

    n_col = find_col(density_rows[0], ["N_m^-3", "N_m3", "density"])
    f_col = find_col(flux_rows[0], ["F_tot_m2s1", "F_total", "F_tot"])
    if n_col is None:
        raise RuntimeError("Could not identify density column in %s" % density_path)
    if f_col is None:
        raise RuntimeError("Could not identify total-flux column in %s" % flux_path)

    rows = []
    passed = True

    densities = [r[n_col] for r in density_rows]
    fluxes = [r[f_col] for r in flux_rows]
    max_density = max(abs(x) for x in densities)
    max_flux_total = max(abs(x) for x in fluxes)

    passed = add_check(rows, "density_zero", "max_abs_density_m3", max_density, 0.0,
                       abs_tol=args.zero_density_tol, units="m^-3",
                       check_type="physical_reference",
                       note="all diagnostic points are inside R_INNER") and passed
    passed = add_check(rows, "flux_zero", "max_abs_flux_total_m2s1", max_flux_total, 0.0,
                       abs_tol=args.zero_flux_tol, units="m^-2 s^-1",
                       check_type="physical_reference",
                       note="all diagnostic points are inside R_INNER") and passed

    max_channel_flux = 0.0
    for ch in CHANNELS:
        col = find_col(flux_rows[0], ["F_%s_m2s1" % ch, "F_%s" % ch])
        present = 1.0 if col is not None else 0.0
        passed = add_check(rows, "channel_present", "F_%s_column_present" % ch, present, 1.0,
                           abs_tol=0.0, units="1", check_type="setup_identity",
                           note="requested flux channel must appear in gridless_points_flux.dat") and passed
        if col is not None:
            max_ch = max(abs(r[col]) for r in flux_rows)
            max_channel_flux = max(max_channel_flux, max_ch)
            passed = add_check(rows, "channel_flux_zero", "max_abs_F_%s_m2s1" % ch, max_ch, 0.0,
                               abs_tol=args.zero_flux_tol, units="m^-2 s^-1",
                               check_type="physical_reference",
                               note="blocked-access channel integral must vanish") and passed

    max_abs_T = 0.0
    max_abs_Jloc = 0.0
    max_closure_abs = 0.0
    max_closure_rel = 0.0
    min_jb = float("inf")
    n_spectrum_values = 0

    for iz, z in enumerate(spectra):
        E = z.get("E_MeV", [])
        T = z.get("T", [])
        Jb = z.get("J_boundary_perMeV", [])
        Jl = z.get("J_local_perMeV", [])
        if not (len(E) == len(T) == len(Jb) == len(Jl)) or len(E) < 2:
            raise RuntimeError("Malformed spectrum zone %d in %s" % (iz, spectrum_path))
        for t, jb, jl in zip(T, Jb, Jl):
            n_spectrum_values += 1
            max_abs_T = max(max_abs_T, abs(t))
            max_abs_Jloc = max(max_abs_Jloc, abs(jl))
            min_jb = min(min_jb, jb)
            closure = jl - t * jb
            max_closure_abs = max(max_closure_abs, abs(closure))
            max_closure_rel = max(max_closure_rel, abs(closure) / max(abs(jb), 1.0e-300))

    if n_spectrum_values == 0:
        raise RuntimeError("No spectrum values found")

    passed = add_check(rows, "transmission_zero", "max_abs_T", max_abs_T, 0.0,
                       abs_tol=args.t_zero_tol, units="1",
                       check_type="error_metric",
                       note="T(E) must be zero at every saved energy") and passed
    passed = add_check(rows, "local_spectrum_zero", "max_abs_J_local_perMeV", max_abs_Jloc, 0.0,
                       abs_tol=args.zero_spectrum_tol, units="m^-2 s^-1 sr^-1 MeV^-1",
                       check_type="error_metric",
                       note="J_local(E)=T(E)J_boundary(E) must be zero") and passed
    passed = add_check(rows, "spectrum_closure_abs", "max_abs_Jlocal_minus_TJb", max_closure_abs, 0.0,
                       abs_tol=args.zero_spectrum_tol, units="m^-2 s^-1 sr^-1 MeV^-1",
                       check_type="exact_internal_identity",
                       note="saved local spectrum must equal T(E)*J_boundary(E)") and passed
    passed = add_check(rows, "spectrum_closure_rel", "max_rel_Jlocal_minus_TJb", max_closure_rel, 0.0,
                       abs_tol=args.closure_tol, units="1",
                       check_type="exact_internal_identity",
                       note="relative closure residual normalized by the nonzero boundary spectrum") and passed

    boundary_present = 1.0 if min_jb > args.min_boundary_j else 0.0
    passed = add_check(rows, "boundary_spectrum_nonzero", "min_J_boundary_positive", boundary_present, 1.0,
                       abs_tol=0.0, units="1", check_type="setup_identity",
                       note="incident spectrum must be nonzero so F16 tests blocked access, not zero input") and passed

    summary_csv = workdir / "F16_summary.csv"
    fieldnames = ["check", "quantity", "check_type", "passed", "value",
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
        "parameters": {
            "nintervals": args.nintervals,
            "r_inner_km": args.r_inner_km,
            "zero_density_tol": args.zero_density_tol,
            "zero_flux_tol": args.zero_flux_tol,
            "zero_spectrum_tol": args.zero_spectrum_tol,
            "t_zero_tol": args.t_zero_tol,
            "closure_tol": args.closure_tol,
        },
        "failed_checks": [r for r in rows if not r["passed"]],
    }
    result_json = workdir / "F16_result.json"
    with result_json.open("w") as f:
        json.dump(result, f, indent=2, sort_keys=True)
        f.write("\n")

    print("F16 %s" % ("PASS" if passed else "FAIL"))
    print("Summary:", summary_csv)
    print("Result :", result_json)
    if not passed:
        print("Failed checks:")
        for r in rows:
            if not r["passed"]:
                print("  - {check} {quantity}: value={value:.6e}, expected={expected_value:.6e}, rel={rel_error:.3e}, abs={abs_error:.3e}".format(**r))
    return passed


def parse_args(argv=None):
    p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        F16: blocked-access zero-flux regression.

        The test runs gridless density/spectrum in a zero magnetic field but puts
        every diagnostic point inside R_INNER.  Because all trajectories are
        immediately classified as forbidden, T(E), J_local(E), density, total
        flux, and all channel fluxes must be zero to machine precision while the
        boundary spectrum remains nonzero.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/F16/run_F16.py
          python srcEarth/test/F16/run_F16.py -np 4 -nt 16
          python srcEarth/test/F16/run_F16.py --nintervals 160 --zero-flux-tol 1e-250
          python srcEarth/test/F16/run_F16.py --dry-run --workdir test_output/F16_dryrun
          python srcEarth/test/F16/run_F16.py --skip-run --workdir test_output/F16_gridless
        """))
    p.add_argument("-np", type=int, default=4, help="number of MPI ranks; default: 4")
    p.add_argument("-nt", type=int, default=16, help="threads per rank; default: 16")
    p.add_argument("--amps", default="./amps", help="AMPS executable path; default: ./amps")
    p.add_argument("--mpirun", default="mpirun", help="MPI launcher; default: mpirun")
    p.add_argument("--workdir", default="test_output/F16_gridless", help="run/output directory")
    p.add_argument("--scheduler", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], default="DYNAMIC")
    p.add_argument("--dynamic-chunk", type=int, default=0)
    p.add_argument("--nintervals", type=int, default=80, help="DS_NINTERVALS value; default: 80")
    p.add_argument("--r-inner-km", type=float, default=5000.0, help="inner absorbing sphere radius in km; default: 5000")
    p.add_argument("--dt-trace", type=float, default=0.05, help="DT_TRACE value")
    p.add_argument("--max-trace-time", type=float, default=5.0, help="MAX_TRACE_TIME / DS_MAX_TRAJ_TIME value")
    p.add_argument("--zero-density-tol", type=float, default=1.0e-250, help="absolute density tolerance for zero result")
    p.add_argument("--zero-flux-tol", type=float, default=1.0e-250, help="absolute flux tolerance for zero result")
    p.add_argument("--zero-spectrum-tol", type=float, default=1.0e-250, help="absolute J_local/closure tolerance for zero result")
    p.add_argument("--t-zero-tol", type=float, default=0.0, help="absolute T(E) tolerance for zero transmission")
    p.add_argument("--closure-tol", type=float, default=1.0e-14, help="relative tolerance for J_local(E)=T(E)J_boundary(E)")
    p.add_argument("--min-boundary-j", type=float, default=0.0, help="minimum accepted boundary spectrum value")
    p.add_argument("--skip-run", action="store_true", help="analyze existing files in --workdir")
    p.add_argument("--keep", action="store_true", help="do not delete an existing workdir")
    p.add_argument("--dry-run", action="store_true", help="render input and print the AMPS command without executing it")
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
    if args.r_inner_km <= 2500.0:
        raise SystemExit("--r-inner-km must remain > 2500 km so all template points are inside the absorber")

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template = script_dir / "AMPS_PARAM_F16_gridless.in"
    reference_repo_csv = script_dir / "reference_F16_blocked_access.csv"
    workdir = (launch_dir / args.workdir).resolve()

    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(str(workdir))
        workdir.mkdir(parents=True, exist_ok=True)
        render_input(template, workdir / "AMPS_PARAM_F16.in", args)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    if reference_repo_csv.exists() and not args.skip_run:
        shutil.copy2(str(reference_repo_csv), str(workdir / "reference_F16_blocked_access_used.csv"))

    if not args.skip_run:
        amps_path = Path(args.amps)
        if not amps_path.is_absolute():
            amps_path = (launch_dir / amps_path).resolve()
        cmd = [
            args.mpirun, "-np", str(args.np), str(amps_path),
            "-mode", "gridless",
            "-i", "AMPS_PARAM_F16.in",
            "-mode3d-parallel", "THREADS",
            "-mode3d-threads", str(args.nt),
            "-mode3d-mpi-scheduler", args.scheduler,
            "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
            "-density-transmission", "DIRECT",
        ]
        print("Running:", " ".join(cmd))
        if args.dry_run:
            print("F16 dry run complete. Generated input under %s" % workdir)
            return 0
        with (workdir / "F16_amps.log").open("w") as log:
            log.write("# Command: %s\n" % " ".join(cmd))
            log.flush()
            rc = subprocess.call(cmd, cwd=str(workdir), stdout=log, stderr=subprocess.STDOUT)
        if rc != 0:
            print("AMPS failed with exit code %d; see %s" % (rc, workdir / "F16_amps.log"))
            return rc

    ok = analyze(workdir, args)
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
