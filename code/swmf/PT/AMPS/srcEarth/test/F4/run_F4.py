#!/usr/bin/env python3
"""
F4 — transmission reconstruction consistency validation test.

Run from the directory containing the AMPS executable:

    python srcEarth/test/F4/run_F4.py -np 4 -nt 16

F4 runs gridless density/spectrum with FIELD_MODEL=DIPOLE and
DS_TRANSMISSION_MODE=SCAN, then verifies exact internal reconstruction
identities from the saved spectrum:

    J_local(E) = T(E) * J_boundary(E)
    n          = 4*pi*int J_local(E)/v(E) dE
    F          = 4*pi*int J_local(E) dE
    F_channel  = 4*pi*int_channel J_local(E) dE
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

TEST_ID = "F4"
TEST_NAME = "Transmission reconstruction consistency"

RE_KM = 6371.2
ALT_KM_DEFAULT = 9000.0
MP_MEV = 938.2720813
C_LIGHT = 299792458.0
J0 = 1.0
E0 = 10.0
GAMMA = 3.5
EMIN = 1.0
EMAX = 1000.0
DEFAULT_LATS = [-60.0, -30.0, 0.0, 30.0, 60.0]
DEFAULT_LONS = [0.0]
ENERGY_BINS = [
    ("BIN01_1_3", 1.0, 3.0),
    ("BIN02_3_10", 3.0, 10.0),
    ("BIN03_10_30", 10.0, 30.0),
    ("BIN04_30_100", 30.0, 100.0),
    ("BIN05_100_300", 100.0, 300.0),
    ("BIN06_300_1000", 300.0, 1000.0),
    ("FULL", 1.0, 1000.0),
]

def run_with_live_log(cmd, cwd, log_path, live_output=True):
    """Run AMPS while preserving a complete log and optionally teeing output live.

    The previous F4 runner redirected all AMPS output to F4_amps.log.  For a long
    trajectory calculation that made a healthy run look frozen.  Reading raw bytes
    preserves carriage returns/newlines from MPI and the C++ progress bar.
    """
    with log_path.open("wb") as log:
        header = ("# Command: %s\n" % " ".join(cmd)).encode("utf-8")
        log.write(header)
        log.flush()
        proc = subprocess.Popen(
            cmd, cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        assert proc.stdout is not None
        while True:
            chunk = os.read(proc.stdout.fileno(), 4096)
            if not chunk:
                break
            log.write(chunk)
            log.flush()
            if live_output:
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
        return proc.wait()


def estimate_work(npoints, scan_n, max_particles):
    """Return (directions_per_energy, trajectories, direction_block_tasks)."""
    full_directions = 24 * 48
    if max_particles > 0:
        ndir = max(1, min(full_directions, max_particles // scan_n))
    else:
        ndir = full_directions
    trajectories = npoints * scan_n * ndir
    block_tasks = npoints * scan_n * ((ndir + 31) // 32)
    return ndir, trajectories, block_tasks



def parse_float_list(text, name):
    out = []
    for part in text.split(','):
        part = part.strip()
        if not part:
            continue
        out.append(float(part))
    if not out:
        raise SystemExit("%s must contain at least one numeric value" % name)
    return out


def normalize_negative_list_options(argv, option_names):
    """Return argv with negative comma-list values attached to their options.

    argparse normally accepts ``--opt value``, but a value like ``-60,-30,0``
    can be mistaken for another option.  This helper supports the natural form

        --lats -60,-30,0,30,60

    while preserving argparse's usual errors for missing values and real options.
    """
    if argv is None:
        argv = sys.argv[1:]
    argv = list(argv)
    out = []
    i = 0
    negative_numeric_list = re.compile(r"^-([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?(,.*)?$")
    option_names = set(option_names)
    while i < len(argv):
        token = argv[i]
        if token in option_names and i + 1 < len(argv):
            value = argv[i + 1]
            if negative_numeric_list.match(value):
                out.append(token + "=" + value)
                i += 2
                continue
        out.append(token)
        i += 1
    return out


def point_label(lon, lat):
    def fmt(x):
        if abs(x - round(x)) < 1.0e-10:
            return str(int(round(x))).replace('-', 'm')
        return ("%.6g" % x).replace('-', 'm').replace('.', 'p')
    return "lon%s_lat%s" % (fmt(lon), fmt(lat))


def build_points(alt_km, lons, lats):
    r = RE_KM + alt_km
    points = []
    for lon in lons:
        lon_rad = math.radians(lon)
        for lat in lats:
            lat_rad = math.radians(lat)
            c = math.cos(lat_rad)
            x = r * c * math.cos(lon_rad)
            y = r * c * math.sin(lon_rad)
            z = r * math.sin(lat_rad)
            points.append({
                "label": point_label(lon, lat),
                "lon_deg": lon,
                "lat_deg": lat,
                "alt_km": alt_km,
                "x_km": x,
                "y_km": y,
                "z_km": z,
            })
    return points


def j_boundary(E_MeV):
    return J0 * (E_MeV / E0) ** (-GAMMA)


def speed_from_energy(E_MeV):
    gamma = 1.0 + E_MeV / MP_MEV
    beta2 = max(0.0, 1.0 - 1.0 / (gamma * gamma))
    return C_LIGHT * math.sqrt(beta2)


def trapz(xs, ys):
    if len(xs) < 2:
        return 0.0
    s = 0.0
    for i in range(len(xs) - 1):
        s += 0.5 * (ys[i] + ys[i + 1]) * (xs[i + 1] - xs[i])
    return s


def interp(xs, ys, x):
    if x <= xs[0]:
        return ys[0]
    if x >= xs[-1]:
        return ys[-1]
    lo, hi = 0, len(xs) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if xs[mid] <= x:
            lo = mid
        else:
            hi = mid
    t = (x - xs[lo]) / (xs[hi] - xs[lo])
    return ys[lo] + t * (ys[hi] - ys[lo])


def interval_grid(E, E1, E2):
    lo = max(E1, E[0])
    hi = min(E2, E[-1])
    if lo >= hi:
        return []
    xs = [lo]
    for x in E:
        if x > lo and x < hi:
            xs.append(x)
    xs.append(hi)
    return xs


def flux_from_local_spectrum(E, Jloc, E1, E2):
    """Integrate a saved nodal local spectrum with piecewise-linear J_local.

    This is the quadrature used for the total-flux closure check because the
    total integral uses only the solver energy nodes.  It must not be used for
    user-defined channels whose endpoints can fall between energy nodes; AMPS
    handles those endpoints by interpolating T and evaluating J_boundary there.
    """
    xs = interval_grid(E, E1, E2)
    if len(xs) < 2:
        return 0.0
    ys = [interp(E, Jloc, x) for x in xs]
    return 4.0 * math.pi * trapz(xs, ys)


def flux_channel_as_amps(E, T, E1, E2):
    """Reproduce DensityGridless.cpp::FluxIntegrateChannel exactly.

    AMPS does *not* linearly interpolate the product J_local=T*J_boundary at a
    channel edge that lies between energy-grid nodes.  It linearly interpolates
    T(E), evaluates the configured boundary spectrum directly at the exact edge,
    forms J_local(edge)=T_interp(edge)*J_boundary(edge), and then applies the
    trapezoidal rule on the augmented grid containing both channel edges.

    Interpolating the saved J_local array instead is a different quadrature for a
    curved power-law spectrum.  At the compact 32-point logarithmic F4 grid that
    difference is expected to be O(10^-3--10^-2), which previously produced false
    channel-closure failures even when the AMPS flux file was correct.
    """
    xs = interval_grid(E, E1, E2)
    if len(xs) < 2:
        return 0.0
    ys = [interp(E, T, x) * j_boundary(x) for x in xs]
    return 4.0 * math.pi * trapz(xs, ys)


def density_from_local_spectrum(E, Jloc, E1, E2):
    xs = interval_grid(E, E1, E2)
    if len(xs) < 2:
        return 0.0
    ys = [interp(E, Jloc, x) / speed_from_energy(x) for x in xs]
    return 4.0 * math.pi * trapz(xs, ys)


def max_rel_boundary_spectrum(E, Jb):
    vals = []
    for e, jb in zip(E, Jb):
        ref = j_boundary(e)
        vals.append(abs(jb - ref) / max(abs(ref), 1.0e-300))
    return max(vals) if vals else float("inf")


def max_abs_jlocal_residual(T, Jb, Jl):
    vals = []
    for t, jb, jl in zip(T, Jb, Jl):
        ref = t * jb
        vals.append(abs(jl - ref) / max(abs(jb), abs(ref), 1.0e-300))
    return max(vals) if vals else float("inf")


def transmission_bounds_violation(T):
    if not T:
        return float("inf")
    low = max(0.0, -min(T))
    high = max(0.0, max(T) - 1.0)
    return max(low, high)


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


def points_block(points):
    lines = ["  OUTPUT_MODE POINTS", "  POINTS_BEGIN"]
    for p in points:
        lines.append("    ! %s lon=%g lat=%g alt=%g km" % (p["label"], p["lon_deg"], p["lat_deg"], p["alt_km"]))
        lines.append("    POINT  %.12e  %.12e  %.12e" % (p["x_km"], p["y_km"], p["z_km"]))
    lines.append("  POINTS_END")
    return "\n".join(lines)


def render_input(template_path, output_path, points, args):
    text = template_path.read_text()
    for key, value in [
        ("MODE3D_THREADS", args.nt),
        ("GRIDLESS_MPI_SCHEDULER", args.scheduler),
        ("GRIDLESS_MPI_DYNAMIC_CHUNK", args.dynamic_chunk),
        ("DS_TRANSMISSION_SCAN_N", args.scan_n),
        ("DS_NINTERVALS", args.nintervals),
        ("DS_MAX_TRAJ_TIME", args.max_trace_time),
        ("MAX_TRACE_TIME", args.max_trace_time),
        ("MAX_TRACE_DISTANCE", args.max_trace_distance),
        ("MAX_STEPS", args.max_steps),
        ("DS_MAX_PARTICLES", args.max_particles),
        ("DS_RETRY_UNRESOLVED", "T" if args.retry_unresolved else "F"),
        ("TRAP_DETECTION", "F" if args.no_trap_detection else "T"),
        ("DT_TRACE", args.dt_trace),
    ]:
        text = replace_key(text, key, value)
    block = points_block(points)
    text = re.sub(r'(?ms)^#OUTPUT_DOMAIN\s+.*?^\s*POINTS_END\s*$', "#OUTPUT_DOMAIN\n" + block, text)
    text += ("\n! F4 harness settings\n"
             "! F4_ALT_KM %.17g\n"
             "! F4_LONS %s\n"
             "! F4_LATS %s\n"
             "! F4_NT %d\n"
             "! F4_GRIDLESS_MPI_SCHEDULER %s\n"
             "! F4_GRIDLESS_MPI_DYNAMIC_CHUNK %d\n"
             "! F4_DS_TRANSMISSION_SCAN_N %d\n"
             "! F4_DS_NINTERVALS %d\n"
             "! F4_DS_MAX_PARTICLES %d\n"
             "! F4_MAX_STEPS %d\n"
             "! F4_MAX_TRACE_TIME_S %.17g\n"
             "! F4_MAX_TRACE_DISTANCE_RE %.17g\n"
             "! F4_TRAP_DETECTION %s\n"
             "! F4_RETRY_UNRESOLVED %s\n") % (
                 args.alt_km,
                 ",".join("%.12g" % x for x in args.lons_values),
                 ",".join("%.12g" % x for x in args.lats_values),
                 args.nt, args.scheduler, args.dynamic_chunk, args.scan_n, args.nintervals,
                 args.max_particles, args.max_steps, args.max_trace_time,
                 args.max_trace_distance,
                 "F" if args.no_trap_detection else "T",
                 "T" if args.retry_unresolved else "F")
    output_path.write_text(text)


def read_outputs(workdir, points):
    density_path = workdir / "gridless_points_density.dat"
    flux_path = workdir / "gridless_points_flux.dat"
    spectrum_path = workdir / "gridless_points_spectrum.dat"
    missing = [str(p) for p in (density_path, flux_path, spectrum_path) if not p.exists()]
    if missing:
        raise RuntimeError("Missing required AMPS output files: " + ", ".join(missing))
    _, density_rows = read_table_file(density_path)
    _, flux_rows = read_table_file(flux_path)
    spectra = read_spectrum_file(spectrum_path)
    if not (len(density_rows) == len(flux_rows) == len(spectra) == len(points)):
        raise RuntimeError("Output size mismatch: density=%d flux=%d spectra=%d expected=%d" %
                           (len(density_rows), len(flux_rows), len(spectra), len(points)))
    out = []
    for i, (p, drow, frow, spec) in enumerate(zip(points, density_rows, flux_rows, spectra)):
        n_col = find_col(drow, ["N_m^-3", "N_m3", "density", "n"])
        f_tot_col = find_col(frow, ["F_tot_m2s1", "F_total", "F_tot"])
        if n_col is None or f_tot_col is None:
            raise RuntimeError("Could not identify density or total-flux column in point %d" % i)
        for key in ("E_MeV", "T", "J_boundary_perMeV", "J_local_perMeV"):
            if key not in spec:
                raise RuntimeError("Spectrum output is missing column %s" % key)
        flux = {"F_tot_m2s1": frow[f_tot_col]}
        for name, e1, e2 in ENERGY_BINS:
            col = find_col(frow, ["F_%s_m2s1" % name, name])
            if col is not None:
                flux["F_%s_m2s1" % name] = frow[col]
        out.append({
            "point": p,
            "density": drow[n_col],
            "flux": flux,
            "E_MeV": spec["E_MeV"],
            "T": spec["T"],
            "J_boundary_perMeV": spec["J_boundary_perMeV"],
            "J_local_perMeV": spec["J_local_perMeV"],
        })
    return out


def add_check(rows, check, point, quantity, value, expected_value, rel_tol=None, abs_tol=None,
              units="", check_type="error_metric", note=""):
    abs_error = abs(value - expected_value)
    if expected_value == 0.0:
        rel_error = abs_error
    else:
        rel_error = abs_error / max(abs(expected_value), 1.0e-300)
    ok = True
    if rel_tol is not None:
        ok = ok and (rel_error <= rel_tol)
    if abs_tol is not None:
        ok = ok and (abs_error <= abs_tol)
    rows.append({
        "check": check,
        "point": point,
        "quantity": quantity,
        "check_type": check_type,
        "passed": ok,
        "value": value,
        "expected_value": expected_value,
        "abs_error": abs_error,
        "rel_error": rel_error,
        "rel_tol": rel_tol,
        "abs_tol": abs_tol,
        "units": units,
        "note": note,
    })
    return ok


def analyze(workdir, points, args):
    outputs = read_outputs(workdir, points)
    rows = []
    passed = True

    for data in outputs:
        label = data["point"]["label"]
        E = data["E_MeV"]
        T = data["T"]
        Jb = data["J_boundary_perMeV"]
        Jl = data["J_local_perMeV"]
        if not (len(E) == len(T) == len(Jb) == len(Jl)) or len(E) < 2:
            raise RuntimeError("Malformed spectrum zone for %s" % label)

        passed = add_check(rows, "boundary_power_law", label, "max_rel_J_boundary",
                           max_rel_boundary_spectrum(E, Jb), 0.0,
                           abs_tol=args.differential_tol, units="1",
                           check_type="setup_identity",
                           note="spectrum file must preserve the imposed power law") and passed

        passed = add_check(rows, "local_spectrum_closure", label, "max_rel_Jlocal_minus_TJb",
                           max_abs_jlocal_residual(T, Jb, Jl), 0.0,
                           abs_tol=args.closure_tol, units="1",
                           check_type="exact_internal_identity",
                           note="J_local(E) must equal T(E)*J_boundary(E)") and passed

        passed = add_check(rows, "transmission_bounds", label, "max_T_bounds_violation",
                           transmission_bounds_violation(T), 0.0,
                           abs_tol=args.t_bounds_tol, units="1",
                           check_type="physical_bound",
                           note="T(E) should remain inside [0,1]") and passed

        dens_spec = density_from_local_spectrum(E, Jl, EMIN, EMAX)
        dens_resid = abs(data["density"] - dens_spec) / max(abs(dens_spec), 1.0e-300)
        passed = add_check(rows, "density_file_vs_spectrum", label, "density_total_relative_residual",
                           dens_resid, 0.0, abs_tol=args.integral_tol, units="1",
                           check_type="exact_internal_identity",
                           note="density file must match 4*pi*int J_local(E)/v(E)dE") and passed

        flux_spec_total = flux_from_local_spectrum(E, Jl, EMIN, EMAX)
        flux_resid = abs(data["flux"]["F_tot_m2s1"] - flux_spec_total) / max(abs(flux_spec_total), 1.0e-300)
        passed = add_check(rows, "flux_file_vs_spectrum", label, "flux_total_relative_residual",
                           flux_resid, 0.0, abs_tol=args.integral_tol, units="1",
                           check_type="exact_internal_identity",
                           note="total flux file must match 4*pi*int J_local(E)dE") and passed

        for name, e1, e2 in ENERGY_BINS:
            out_key = "F_%s_m2s1" % name
            if out_key not in data["flux"]:
                passed = add_check(rows, "flux_channel_present", label, out_key,
                                   0.0, 1.0, abs_tol=0.0, units="1",
                                   check_type="setup_identity",
                                   note="missing requested flux-channel column") and passed
                continue
            # Match the actual AMPS channel quadrature.  Channel boundaries are
            # generally not nodes of the logarithmic SCAN grid.  The C++ solver
            # interpolates T at each boundary and evaluates J_boundary exactly;
            # linearly interpolating the saved product J_local would test a
            # different numerical rule and gives percent-level false residuals on
            # the intentionally compact F4 grid.
            f_spec = flux_channel_as_amps(E, T, e1, e2)
            resid = abs(data["flux"][out_key] - f_spec) / max(abs(f_spec), 1.0e-300)
            passed = add_check(rows, "flux_channel_file_vs_spectrum", label,
                               "%s_relative_residual" % out_key,
                               resid, 0.0, abs_tol=args.integral_tol, units="1",
                               check_type="exact_internal_identity",
                               note=("channel flux must match AMPS quadrature: interpolate T at "
                                     "the exact channel edges, evaluate J_boundary there, then "
                                     "integrate T*J_boundary on the augmented grid")) and passed

    summary_csv = workdir / "F4_summary.csv"
    fieldnames = ["check", "point", "quantity", "check_type", "passed", "value",
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
        "parameters": {
            "alt_km": args.alt_km,
            "lons": args.lons_values,
            "lats": args.lats_values,
            "scan_n": args.scan_n,
            "nintervals": args.nintervals,
            "max_particles": args.max_particles,
            "max_steps": args.max_steps,
            "max_trace_time_s": args.max_trace_time,
            "max_trace_distance_Re": args.max_trace_distance,
            "trap_detection": not args.no_trap_detection,
            "retry_unresolved": args.retry_unresolved,
            "closure_tol": args.closure_tol,
            "integral_tol": args.integral_tol,
            "differential_tol": args.differential_tol,
            "t_bounds_tol": args.t_bounds_tol,
        },
        "failed_checks": [r for r in rows if not r["passed"]],
    }
    result_json = workdir / "F4_result.json"
    with result_json.open("w") as f:
        json.dump(result, f, indent=2, sort_keys=True)
        f.write("\n")

    print("F4 %s" % ("PASS" if passed else "FAIL"))
    print("Summary:", summary_csv)
    print("Result :", result_json)
    if not passed:
        print("Failed checks:")
        for r in rows:
            if not r["passed"]:
                print("  - {check} [{point}] {quantity}: value={value:.6e}, expected={expected_value:.6e}, rel={rel_error:.3e}, abs={abs_error:.3e}".format(**r))
    return passed


def parse_args(argv=None):
    p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        F4: transmission reconstruction consistency.

        The test renders diagnostic points on the 9000 km shell, runs gridless
        density/spectrum with FIELD_MODEL=DIPOLE and DS_TRANSMISSION_MODE=SCAN,
        and checks that the saved T(E), boundary spectrum, local spectrum,
        density, and flux files are mutually consistent.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/F4/run_F4.py
          python srcEarth/test/F4/run_F4.py -np 4 -nt 16
          python srcEarth/test/F4/run_F4.py --scan-n 64 --max-particles 2048
          python srcEarth/test/F4/run_F4.py --lons 0,90 --lats -60,-30,0,30,60
          python srcEarth/test/F4/run_F4.py --integral-tol 5e-5 --closure-tol 2e-5
          python srcEarth/test/F4/run_F4.py --dry-run --workdir test_output/F4_dryrun
          python srcEarth/test/F4/run_F4.py --skip-run --workdir test_output/F4_gridless
        """))
    p.add_argument("-np", type=int, default=4, help="number of MPI ranks; default: 4")
    p.add_argument("-nt", type=int, default=16, help="threads per rank; default: 16")
    p.add_argument("--amps", default="./amps", help="AMPS executable path; default: ./amps")
    p.add_argument("--mpirun", default="mpirun", help="MPI launcher; default: mpirun")
    p.add_argument("--workdir", default="test_output/F4_gridless", help="run/output directory")
    p.add_argument("--scheduler", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], default="DYNAMIC")
    p.add_argument("--dynamic-chunk", type=int, default=0)
    p.add_argument("--scan-n", type=int, default=32, help="DS_TRANSMISSION_SCAN_N value; default: 32")
    p.add_argument("--nintervals", type=int, default=31, help="DS_NINTERVALS value; default: 31 (SCAN uses --scan-n for the actual grid)")
    p.add_argument("--alt-km", type=float, default=ALT_KM_DEFAULT, help="shell altitude in km; default: 9000")
    p.add_argument("--lons", default=",".join(str(x) for x in DEFAULT_LONS), help="comma-separated longitudes in degrees")
    p.add_argument("--lats", default=",".join(str(x) for x in DEFAULT_LATS), help="comma-separated latitudes in degrees")
    p.add_argument("--dt-trace", type=float, default=0.1, help="DT_TRACE value")
    p.add_argument("--max-trace-time", type=float, default=120.0, help="MAX_TRACE_TIME / DS_MAX_TRAJ_TIME value; default: 120 s")
    p.add_argument("--max-trace-distance", type=float, default=300.0, help="MAX_TRACE_DISTANCE in Re; 0 disables it; default: 300")
    p.add_argument("--max-steps", type=int, default=100000, help="MAX_STEPS per trajectory; default: 100000")
    p.add_argument("--max-particles", type=int, default=512, help="DS_MAX_PARTICLES per point across all energies; 0 means no cap; default: 512")
    p.add_argument("--retry-unresolved", action="store_true", help="retry unresolved trajectories once; disabled by default for this closure test")
    p.add_argument("--no-trap-detection", action="store_true", help="disable magnetic trap detection")
    p.add_argument("--quiet", action="store_true", help="write AMPS output only to F4_amps.log instead of teeing it live")
    p.add_argument("--closure-tol", type=float, default=2.0e-5,
                   help="tolerance for J_local(E)=T(E)*J_boundary(E); allows Tecplot ASCII precision")
    p.add_argument("--integral-tol", type=float, default=2.0e-5,
                   help="tolerance for density/flux file-vs-spectrum reconstruction")
    p.add_argument("--differential-tol", type=float, default=2.0e-5,
                   help="tolerance for imposed power-law values in the spectrum file")
    p.add_argument("--t-bounds-tol", type=float, default=1.0e-12,
                   help="allowed violation of the physical T(E) interval [0,1]")
    p.add_argument("--skip-run", action="store_true", help="analyze existing files in --workdir")
    p.add_argument("--keep", action="store_true", help="do not delete an existing workdir")
    p.add_argument("--dry-run", action="store_true", help="render input and print the AMPS command without executing it")
    normalized_argv = normalize_negative_list_options(argv, ["--lats", "--lons"])
    return p.parse_args(normalized_argv)


def main(argv=None):
    args = parse_args(argv)
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.scan_n < 2:
        raise SystemExit("--scan-n must be >= 2")
    if args.nintervals < 2:
        raise SystemExit("--nintervals must be >= 2")
    if args.max_particles < 0:
        raise SystemExit("--max-particles must be >= 0")
    if args.max_steps < 1:
        raise SystemExit("--max-steps must be >= 1")
    if args.max_trace_time <= 0.0:
        raise SystemExit("--max-trace-time must be positive")
    if args.max_trace_distance < 0.0:
        raise SystemExit("--max-trace-distance must be >= 0")
    if args.alt_km <= 0.0:
        raise SystemExit("--alt-km must be positive")
    args.lons_values = parse_float_list(args.lons, "--lons")
    args.lats_values = parse_float_list(args.lats, "--lats")
    for lat in args.lats_values:
        if lat < -90.0 or lat > 90.0:
            raise SystemExit("latitude out of range: %g" % lat)

    points = build_points(args.alt_km, args.lons_values, args.lats_values)
    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template = script_dir / "AMPS_PARAM_F4_gridless.in"
    reference_repo_csv = script_dir / "reference_F4_reconstruction.csv"
    workdir = (launch_dir / args.workdir).resolve()

    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(str(workdir))
        workdir.mkdir(parents=True, exist_ok=True)
        render_input(template, workdir / "AMPS_PARAM_F4.in", points, args)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    if reference_repo_csv.exists() and not args.skip_run:
        shutil.copy2(str(reference_repo_csv), str(workdir / "reference_F4_reconstruction_used.csv"))

    if not args.skip_run:
        amps_path = Path(args.amps)
        if not amps_path.is_absolute():
            amps_path = (launch_dir / amps_path).resolve()
        cmd = [
            args.mpirun, "-np", str(args.np), str(amps_path),
            "-mode", "gridless",
            "-i", "AMPS_PARAM_F4.in",
            "-mode3d-parallel", "THREADS",
            "-mode3d-threads", str(args.nt),
            "-mode3d-mpi-scheduler", args.scheduler,
            "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
            "-density-transmission", "SCAN",
        ]
        ndir, ntraj, ntasks = estimate_work(len(points), args.scan_n, args.max_particles)
        print("Running:", " ".join(cmd))
        print("F4 workload: %d points x %d energies x %d directions = %d trajectories" %
              (len(points), args.scan_n, ndir, ntraj))
        print("MPI scheduler tasks: %d direction blocks (up to 32 trajectories each)" % ntasks)
        if args.np > 1 and args.nt > 1:
            print("Note: the multi-rank gridless scheduler parallelizes these tasks across MPI ranks; -nt does not multiply the MPI task concurrency.")
        print("AMPS output log:", workdir / "F4_amps.log")
        if args.quiet:
            print("Live AMPS output is disabled by --quiet.")
        else:
            print("AMPS output follows and is also copied to the log.")
        sys.stdout.flush()
        if args.dry_run:
            print("F4 dry run complete. Generated input under %s" % workdir)
            return 0
        rc = run_with_live_log(cmd, workdir, workdir / "F4_amps.log", live_output=not args.quiet)
        if rc != 0:
            print("AMPS failed with exit code %d; see %s" % (rc, workdir / "F4_amps.log"))
            return rc

    ok = analyze(workdir, points, args)
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
