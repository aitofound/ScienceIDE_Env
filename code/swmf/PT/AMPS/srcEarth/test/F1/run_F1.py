#!/usr/bin/env python3
"""
F1 — zero-field density/flux normalization validation test.

Run from the directory containing the AMPS executable:

    python srcEarth/test/F1/run_F1.py -np 4 -nt 16

The test uses FIELD_MODEL=NONE and R_INNER=0 so every straight-line trajectory
from every requested point exits the box.  The expected transmission is exactly
T(E)=1.  The harness then checks J_local/J_boundary, density, total flux,
channel fluxes, and spatial uniformity.
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

TEST_ID = "F1"
TEST_NAME = "Zero-field density/flux normalization"

# Keep the validation-side particle constants identical to the constants used by
# DensityGridless.cpp.  Reconstructing density requires J(E)/v(E); even a small
# mismatch in the rest energy changes v(E) enough to be visible when the
# reconstruction tolerance is near machine precision.
C_LIGHT = 299792458.0
ELEMENTARY_CHARGE_C = 1.602176634e-19
AMU_KG = 1.66053906660e-27
MEV_TO_J = 1.0e6 * ELEMENTARY_CHARGE_C
PROTON_MASS_AMU = 1.007276466621
PROTON_REST_ENERGY_MEV = PROTON_MASS_AMU * AMU_KG * C_LIGHT ** 2 / MEV_TO_J

J0 = 1.0
E0 = 10.0
GAMMA = 3.5
EMIN = 1.0
EMAX = 1000.0
CHANNELS = [
    ("LOW", 1.0, 10.0),
    ("MID", 10.0, 100.0),
    ("HIGH", 100.0, 1000.0),
    ("FULL", 1.0, 1000.0),
]


def j_boundary(E_MeV):
    return J0 * (E_MeV / E0) ** (-GAMMA)


def speed_from_energy(E_MeV):
    """Return the relativistic proton speed using the solver's exact constants.

    F1 fixes MASS_AMU to PROTON_MASS_AMU in AMPS_PARAM_F1_gridless.in.  Deriving
    the rest energy from the same mass, atomic-mass-unit value, elementary charge,
    and speed of light used by DensityGridless.cpp prevents the test harness from
    introducing a small artificial density-reconstruction discrepancy.
    """
    gamma = 1.0 + E_MeV / PROTON_REST_ENERGY_MEV
    beta2 = max(0.0, 1.0 - 1.0 / (gamma * gamma))
    return C_LIGHT * math.sqrt(beta2)


def flux_power_law(E1, E2):
    if abs(GAMMA - 1.0) < 1.0e-14:
        integ = J0 * E0 * math.log(E2 / E1)
    else:
        integ = (J0 * E0 ** GAMMA / (GAMMA - 1.0) *
                 (E1 ** (1.0 - GAMMA) - E2 ** (1.0 - GAMMA)))
    return 4.0 * math.pi * integ


def density_high_resolution(E1, E2, n=200000):
    # Standard-library log trapezoid to avoid requiring numpy on HPC login nodes.
    log1 = math.log(E1)
    log2 = math.log(E2)
    total = 0.0
    prev_E = None
    prev_g = None
    for i in range(n):
        a = float(i) / float(n - 1)
        E = math.exp(log1 + a * (log2 - log1))
        g = j_boundary(E) / speed_from_energy(E)
        if prev_E is not None:
            total += 0.5 * (prev_g + g) * (E - prev_E)
        prev_E, prev_g = E, g
    return 4.0 * math.pi * total


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


def channel_flux_from_spectrum(E, T, E1, E2):
    lo = max(E1, E[0])
    hi = min(E2, E[-1])
    if lo >= hi:
        return 0.0
    xs = [lo]
    ts = [interp(E, T, lo)]
    for x, t in zip(E, T):
        if x > lo and x < hi:
            xs.append(x)
            ts.append(t)
    xs.append(hi)
    ts.append(interp(E, T, hi))
    ys = [t * j_boundary(x) for x, t in zip(xs, ts)]
    return 4.0 * math.pi * trapz(xs, ys)


def read_reference_csv(path):
    ref = {}
    with path.open("r", newline="") as f:
        for row in csv.DictReader(f):
            ref[row["quantity"]] = float(row["reference_value"])
    return ref


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


def render_input(template_path, output_path, nt, scheduler, dynamic_chunk):
    text = template_path.read_text()
    text = re.sub(r'(?m)^(\s*MODE3D_THREADS\s+)\S+', r'\g<1>%d' % nt, text)
    text = re.sub(r'(?m)^(\s*GRIDLESS_MPI_SCHEDULER\s+)\S+', r'\g<1>%s' % scheduler, text)
    text = re.sub(r'(?m)^(\s*GRIDLESS_MPI_DYNAMIC_CHUNK\s+)\S+', r'\g<1>%d' % dynamic_chunk, text)
    text += ("\n! F1 harness settings\n"
             "! F1_NT %d\n"
             "! F1_GRIDLESS_MPI_SCHEDULER %s\n"
             "! F1_GRIDLESS_MPI_DYNAMIC_CHUNK %d\n") % (nt, scheduler, dynamic_chunk)
    output_path.write_text(text)


def analyze(workdir, reference_csv, args):
    density_path = workdir / "gridless_points_density.dat"
    spectrum_path = workdir / "gridless_points_spectrum.dat"
    flux_path = workdir / "gridless_points_flux.dat"
    missing = [str(p) for p in (density_path, spectrum_path, flux_path) if not p.exists()]
    if missing:
        raise RuntimeError("Missing required AMPS output files: " + ", ".join(missing))

    ref = read_reference_csv(reference_csv)
    _, density_rows = read_table_file(density_path)
    _, flux_rows = read_table_file(flux_path)
    spectra = read_spectrum_file(spectrum_path)

    if len(density_rows) != 10:
        raise RuntimeError("Expected 10 density rows, found %d" % len(density_rows))
    if len(flux_rows) != 10:
        raise RuntimeError("Expected 10 flux rows, found %d" % len(flux_rows))
    if len(spectra) != 10:
        raise RuntimeError("Expected 10 spectrum zones, found %d" % len(spectra))

    checks = []
    # Python 3.6-compatible mutable closure flag.
    nonlocal_passed = [True]

    def add_check(name, value, expected_value, rel_tol=None, abs_tol=None, units="", note="",
                  check_type="error_metric"):
        abs_err = abs(value - expected_value)

        # A conventional relative error is undefined when the reference value is
        # exactly zero.  The previous fallback denominator of 1.0e-300 turned tiny
        # absolute deviations (for example, 2.5e-6) into meaningless values near
        # 1.0e294.  Zero-target checks in F1 are absolute error metrics, so record
        # rel_error as None/JSON null and evaluate them only with abs_tol.
        if expected_value == 0.0:
            rel_err = None
        else:
            rel_err = abs_err / abs(expected_value)

        if rel_tol is not None and rel_err is None:
            raise ValueError("Relative tolerance requested for zero expected value in check %s" % name)

        ok = True
        if rel_tol is not None:
            ok = ok and (rel_err <= rel_tol)
        if abs_tol is not None:
            ok = ok and (abs_err <= abs_tol)
        checks.append({
            "check": name,
            "check_type": check_type,
            "value": value,
            "expected_value": expected_value,
            "abs_error": abs_err,
            "rel_error": rel_err,
            "rel_tol": rel_tol,
            "abs_tol": abs_tol,
            "units": units,
            "passed": ok,
            "note": note,
        })
        if not ok:
            nonlocal_passed[0] = False

    n_col = find_col(density_rows[0], ["N_m^-3", "N_m3", "density"])
    f_col = find_col(flux_rows[0], ["F_tot_m2s1", "F_total", "F_tot"])
    if n_col is None:
        raise RuntimeError("Could not identify density column in %s" % density_path)
    if f_col is None:
        raise RuntimeError("Could not identify total-flux column in %s" % flux_path)

    densities = [r[n_col] for r in density_rows]
    fluxes = [r[f_col] for r in flux_rows]

    # Compare each point to the high-resolution analytical normalization.
    for i, n in enumerate(densities):
        add_check("density_point_%02d" % i, n, ref["density_total_m3"], rel_tol=args.norm_tol, units="m^-3",
                  check_type="physical_reference")
    for i, f in enumerate(fluxes):
        add_check("flux_total_point_%02d" % i, f, ref["flux_total_m2s1"], rel_tol=args.norm_tol, units="m^-2 s^-1",
                  check_type="physical_reference")

    # Spatial invariance: all points are physically identical in this zero-field setup.
    n_mean = sum(densities) / float(len(densities))
    f_mean = sum(fluxes) / float(len(fluxes))
    add_check("density_spatial_max_minus_min", max(densities) - min(densities), 0.0,
              abs_tol=max(args.spatial_tol * abs(n_mean), 1.0e-300), units="m^-3")
    add_check("flux_spatial_max_minus_min", max(fluxes) - min(fluxes), 0.0,
              abs_tol=max(args.spatial_tol * abs(f_mean), 1.0e-300), units="m^-2 s^-1")

    # Spectrum/transmission checks.
    max_t_err = 0.0
    max_jratio_err = 0.0
    max_density_reconstruct_rel = 0.0
    max_flux_reconstruct_rel = 0.0
    for iz, z in enumerate(spectra):
        E = z.get("E_MeV", [])
        T = z.get("T", [])
        Jb = z.get("J_boundary_perMeV", [])
        Jl = z.get("J_local_perMeV", [])
        if not (len(E) == len(T) == len(Jb) == len(Jl)) or len(E) < 2:
            raise RuntimeError("Malformed spectrum zone %d in %s" % (iz, spectrum_path))
        for e, t, jb, jl in zip(E, T, Jb, Jl):
            max_t_err = max(max_t_err, abs(t - 1.0))
            if jb != 0.0:
                max_jratio_err = max(max_jratio_err, abs(jl / jb - 1.0))
        dens_rec = 4.0 * math.pi * trapz(E, [jl / speed_from_energy(e) for e, jl in zip(E, Jl)])
        flux_rec = 4.0 * math.pi * trapz(E, Jl)
        max_density_reconstruct_rel = max(max_density_reconstruct_rel,
                                          abs(dens_rec - densities[iz]) / max(abs(densities[iz]), 1.0e-300))
        max_flux_reconstruct_rel = max(max_flux_reconstruct_rel,
                                       abs(flux_rec - fluxes[iz]) / max(abs(fluxes[iz]), 1.0e-300))

        # Channel fluxes from spectrum and, where present, from gridless_points_flux.dat.
        for name, E1, E2 in CHANNELS:
            f_rec = channel_flux_from_spectrum(E, T, E1, E2)
            add_check("flux_%s_from_spectrum_point_%02d" % (name, iz), f_rec,
                      ref["flux_%s_m2s1" % name], rel_tol=args.norm_tol, units="m^-2 s^-1",
                      check_type="physical_reference")

            ch_col = find_col(flux_rows[iz], ["F_%s_m2s1" % name, "F_%s" % name])
            if ch_col is not None:
                add_check("flux_%s_file_point_%02d" % (name, iz), flux_rows[iz][ch_col],
                          ref["flux_%s_m2s1" % name], rel_tol=args.norm_tol, units="m^-2 s^-1",
                      check_type="physical_reference")

    add_check("max_abs_T_minus_1", max_t_err, 0.0, abs_tol=args.transmission_tol)
    add_check("max_abs_Jlocal_over_Jboundary_minus_1", max_jratio_err, 0.0, abs_tol=args.transmission_tol)
    add_check("max_density_reconstruction_rel", max_density_reconstruct_rel, 0.0, abs_tol=args.consistency_tol)
    add_check("max_flux_reconstruction_rel", max_flux_reconstruct_rel, 0.0, abs_tol=args.consistency_tol)

    passed = nonlocal_passed[0]

    summary_csv = workdir / "F1_summary.csv"
    with summary_csv.open("w", newline="") as f:
        fields = ["check", "check_type", "passed", "value", "expected_value", "abs_error", "rel_error", "rel_tol", "abs_tol", "units", "note"]
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for c in checks:
            w.writerow(c)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "n_checks": len(checks),
        "n_failed": sum(1 for c in checks if not c["passed"]),
        "workdir": str(workdir),
        "summary_csv": str(summary_csv),
        "reference_csv": str(reference_csv),
        "tolerances": {
            "norm_tol": args.norm_tol,
            "transmission_tol": args.transmission_tol,
            "spatial_tol": args.spatial_tol,
            "consistency_tol": args.consistency_tol,
        },
        "checks": checks,
    }
    result_json = workdir / "F1_result.json"
    with result_json.open("w") as f:
        json.dump(result, f, indent=2, sort_keys=True)

    print("F1 summary: %d checks, %d failed" % (result["n_checks"], result["n_failed"]))
    print("  summary: %s" % summary_csv)
    print("  result : %s" % result_json)
    if not passed:
        print("Failed checks:")
        for c in checks:
            if not c["passed"]:
                # Zero-reference metrics intentionally have rel_error=None.  Format
                # that case explicitly instead of applying a floating-point format
                # code to None, which would hide the actual failed check behind a
                # reporting exception.
                rel_text = "n/a" if c["rel_error"] is None else "%.3e" % c["rel_error"]
                print("  - {check}: type={check_type}, value={value:.6e}, expected={expected_value:.6e}, rel={rel}, abs={abs_error:.3e}".format(
                    rel=rel_text, **c))
    return passed


def parse_args():
    p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        F1: zero-field density/flux normalization.

        The test runs gridless density/spectrum with FIELD_MODEL=NONE and
        R_INNER=0.  Expected values are the closed-form/high-resolution power-law
        normalization for T(E)=1 at all points.

        The script is expected to be launched from the directory containing the
        AMPS executable.  AMPS itself is launched with mpirun.  The default
        parallel configuration is -np 4 -nt 16.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/F1/run_F1.py
          python srcEarth/test/F1/run_F1.py -np 4 -nt 16
          python srcEarth/test/F1/run_F1.py -np 18 -nt 16 --scheduler DYNAMIC --dynamic-chunk 0
          python srcEarth/test/F1/run_F1.py --dry-run --workdir test_output/F1_dryrun
          python srcEarth/test/F1/run_F1.py --skip-run --workdir test_output/F1_gridless
        """))
    p.add_argument("-np", type=int, default=4, help="number of MPI ranks; default: 4")
    p.add_argument("-nt", type=int, default=16, help="threads per rank; default: 16")
    p.add_argument("--amps", default="./amps", help="AMPS executable path; default: ./amps")
    p.add_argument("--mpirun", default="mpirun", help="MPI launcher; default: mpirun")
    p.add_argument("--workdir", default="test_output/F1_gridless", help="run/output directory")
    p.add_argument("--scheduler", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], default="DYNAMIC")
    p.add_argument("--dynamic-chunk", type=int, default=0)
    p.add_argument("--norm-tol", type=float, default=2.0e-2, help="relative tolerance against continuous analytic normalization")
    p.add_argument("--transmission-tol", type=float, default=1.0e-12, help="absolute tolerance for T=1 and Jloc/Jb=1")
    p.add_argument("--spatial-tol", type=float, default=1.0e-12, help="relative spatial uniformity tolerance")
    p.add_argument("--consistency-tol", type=float, default=5.0e-10, help="absolute tolerance for reconstruction relative errors")
    p.add_argument("--skip-run", action="store_true", help="analyze existing files in --workdir")
    p.add_argument("--keep", action="store_true", help="do not delete an existing workdir")
    p.add_argument("--dry-run", action="store_true", help="print the AMPS command without executing it")
    return p.parse_args()


def finish_test(passed):
    """Print the C-test-style final result and return its shell exit code.

    Keeping the human-readable result and the process return value in one helper
    prevents a future edit from accidentally printing PASS while returning a
    failure code, or printing FAIL while returning success.  The repository test
    runner determines success from the exit status, while users commonly inspect
    the final RESULT line, so both interfaces must describe the same outcome.
    """
    print("\nRESULT: %s" % ("PASS" if passed else "FAIL"))
    return 0 if passed else 1


def main():
    args = parse_args()
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template = script_dir / "AMPS_PARAM_F1_gridless.in"
    reference_csv = script_dir / "reference_F1_zero_field.csv"
    workdir = (launch_dir / args.workdir).resolve()

    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(str(workdir))
        workdir.mkdir(parents=True, exist_ok=True)
        render_input(template, workdir / "AMPS_PARAM_F1.in", args.nt, args.scheduler, args.dynamic_chunk)
        shutil.copy2(str(reference_csv), str(workdir / "reference_F1_zero_field.csv"))

        amps_path = Path(args.amps)
        if not amps_path.is_absolute():
            amps_path = (launch_dir / amps_path).resolve()
        cmd = [
            args.mpirun, "-np", str(args.np), str(amps_path),
            "-mode", "gridless",
            "-i", "AMPS_PARAM_F1.in",
            "-mode3d-parallel", "THREADS",
            "-mode3d-threads", str(args.nt),
            "-mode3d-mpi-scheduler", args.scheduler,
            "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
            "-density-transmission", "DIRECT",
        ]
        print("Running:", " ".join(cmd))
        if args.dry_run:
            return 0
        with (workdir / "F1_amps.log").open("w") as log:
            log.write("# Command: %s\n" % " ".join(cmd))
            log.flush()
            rc = subprocess.call(cmd, cwd=str(workdir), stdout=log, stderr=subprocess.STDOUT)
        if rc != 0:
            print("AMPS failed with exit code %d; see %s" % (rc, workdir / "F1_amps.log"))

            # A nonzero AMPS status means that F1 did not complete successfully.
            # Normalize that condition to the validation-runner convention:
            # RESULT: FAIL and exit status 1.  The original AMPS status remains
            # visible in the diagnostic line above, so no debugging information
            # is lost by using a stable test-level failure code.
            return finish_test(False)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)

    ok = analyze(workdir, reference_csv, args)

    # Match the C-test runners: make RESULT the final status line and derive the
    # process exit code from the same Boolean used to select PASS or FAIL.
    return finish_test(ok)


if __name__ == "__main__":
    sys.exit(main())
