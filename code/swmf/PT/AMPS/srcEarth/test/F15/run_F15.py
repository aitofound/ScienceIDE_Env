#!/usr/bin/env python3
"""
F15 — density normalization from differential flux and relativistic speed.

Run from the directory containing the AMPS executable:

    python srcEarth/test/F15/run_F15.py -np 4 -nt 16

The test uses FIELD_MODEL=NONE, R_INNER=0, and one TABLE top-hat spectrum per
case.  Each case is a narrow constant differential-flux band centered at
1, 10, 100, or 1000 MeV.  Because T(E)=1 exactly, the analytical references are

    F = 4*pi*J0*(E2-E1)
    n = 4*pi*J0/c * [sqrt(E2*(E2+2m)) - sqrt(E1*(E1+2m))]

where energies and the proton rest mass m are in MeV, J0 is the directional
differential flux per MeV, and c is in m/s.  The density formula follows from
v(E)/c = sqrt(E*(E+2m))/(E+m), so d/dE sqrt(E*(E+2m)) = c/v(E).
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

TEST_ID = "F15"
TEST_NAME = "Density normalization from differential flux"

# Use the same physical constants and particle mass as DensityGridless.cpp and
# AMPS_PARAM_F15_gridless.in.  Deriving the rest energy instead of hard-coding
# a rounded proton value prevents the test harness from introducing a small
# artificial density mismatch in the relativistic 1/v(E) reference.
C_LIGHT = 299792458.0
ELEMENTARY_CHARGE_C = 1.602176634e-19
MEV_TO_J = 1.0e6 * ELEMENTARY_CHARGE_C
AMU_KG = 1.66053906660e-27
PROTON_MASS_AMU = 1.007276466621
MP_MEV = PROTON_MASS_AMU * AMU_KG * C_LIGHT ** 2 / MEV_TO_J
J0_DEFAULT = 1.0
WIDTH_FRAC_DEFAULT = 0.02
CASE_CENTERS_DEFAULT = [1.0, 10.0, 100.0, 1000.0]


def speed_from_energy(E_MeV):
    gamma = 1.0 + E_MeV / MP_MEV
    beta2 = max(0.0, 1.0 - 1.0 / (gamma * gamma))
    return C_LIGHT * math.sqrt(beta2)


def top_hat_bounds(center_MeV, width_frac):
    half = 0.5 * width_frac
    e1 = center_MeV * (1.0 - half)
    e2 = center_MeV * (1.0 + half)
    if not (e1 > 0.0 and e2 > e1):
        raise ValueError("invalid top-hat bounds for center=%g width_frac=%g" % (center_MeV, width_frac))
    return e1, e2


def flux_top_hat(E1, E2, j0):
    return 4.0 * math.pi * j0 * (E2 - E1)


def density_top_hat(E1, E2, j0):
    # Integral of dE/v(E) for relativistic kinetic energy E:
    #   v/c = sqrt(E*(E+2m))/(E+m)
    #   int dE/v(E) = sqrt(E*(E+2m))/c.
    a = math.sqrt(E2 * (E2 + 2.0 * MP_MEV))
    b = math.sqrt(E1 * (E1 + 2.0 * MP_MEV))
    return 4.0 * math.pi * j0 * (a - b) / C_LIGHT


def trapz(xs, ys):
    if len(xs) < 2:
        return 0.0
    s = 0.0
    for i in range(len(xs) - 1):
        s += 0.5 * (ys[i] + ys[i + 1]) * (xs[i + 1] - xs[i])
    return s


def write_reference_csv(path, cases):
    fields = [
        "case", "center_MeV", "E1_MeV", "E2_MeV", "quantity",
        "expected_value", "units", "reference_type", "notes",
    ]
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for case in cases:
            c = case["label"]
            E0, E1, E2, j0 = case["center"], case["E1"], case["E2"], case["j0"]
            F = flux_top_hat(E1, E2, j0)
            n = density_top_hat(E1, E2, j0)
            v0 = speed_from_energy(E0)
            veff = F / n if n != 0.0 else float("nan")
            mono_density = F / v0
            rows = [
                ("flux_total_m2s1", F, "m^-2 s^-1", "closed_form", "4*pi*J0*(E2-E1)"),
                ("density_total_m3", n, "m^-3", "closed_form", "4*pi*J0*int_E1^E2 dE/v(E)"),
                ("speed_center_mps", v0, "m s^-1", "relativistic", "proton speed at top-hat center"),
                ("speed_effective_mps", veff, "m s^-1", "closed_form", "F/n for the finite top-hat"),
                ("mono_density_m3", mono_density, "m^-3", "monoenergetic_approx", "F/v(Ecenter); should be close for a narrow top-hat"),
                ("mono_density_ratio", n / mono_density, "1", "closed_form", "finite top-hat density divided by F/v(Ecenter)"),
            ]
            for q, val, units, rtype, notes in rows:
                w.writerow({
                    "case": c,
                    "center_MeV": "%.17g" % E0,
                    "E1_MeV": "%.17g" % E1,
                    "E2_MeV": "%.17g" % E2,
                    "quantity": q,
                    "expected_value": "%.17e" % val,
                    "units": units,
                    "reference_type": rtype,
                    "notes": notes,
                })


def read_reference_csv(path):
    ref = {}
    with path.open("r", newline="") as f:
        for row in csv.DictReader(f):
            ref[(row["case"], row["quantity"])] = float(row["expected_value"])
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


def case_label(center_MeV):
    if center_MeV >= 1000.0:
        return "E%04gMeV" % center_MeV
    if center_MeV >= 100.0:
        return "E%03gMeV" % center_MeV
    if center_MeV >= 10.0:
        return "E%02gMeV" % center_MeV
    return "E%gMeV" % center_MeV


def build_cases(centers, width_frac, j0):
    cases = []
    for center in centers:
        E1, E2 = top_hat_bounds(center, width_frac)
        cases.append({
            "label": case_label(center),
            "center": center,
            "E1": E1,
            "E2": E2,
            "j0": j0,
        })
    return cases


def write_top_hat_table(path, E1, E2, j0):
    with path.open("w") as f:
        f.write("# F15 top-hat TABLE spectrum\n")
        f.write("# E_MeV  J_perMeV\n")
        f.write("%.17e %.17e\n" % (E1, j0))
        f.write("%.17e %.17e\n" % (E2, j0))


def render_input(template_path, output_path, case, nt, scheduler, dynamic_chunk, nintervals):
    text = template_path.read_text()
    replacements = {
        "MODE3D_THREADS": str(nt),
        "GRIDLESS_MPI_SCHEDULER": scheduler,
        "GRIDLESS_MPI_DYNAMIC_CHUNK": str(dynamic_chunk),
        "DS_NINTERVALS": str(nintervals),
        "DS_EMIN": "%.17g" % case["E1"],
        "DS_EMAX": "%.17g" % case["E2"],
        "SPEC_EMIN": "%.17g" % case["E1"],
        "SPEC_EMAX": "%.17g" % case["E2"],
        "SPEC_TABLE_FILE": "F15_top_hat_spectrum.dat",
    }
    for key, val in replacements.items():
        text = re.sub(r'(?m)^(\s*' + re.escape(key) + r'\s+)\S+', r'\g<1>' + val, text)
    text = re.sub(
        r'(?m)^(\s*TOPHAT\s+)\S+\s+\S+',
        r'\g<1>' + ("%.17g %.17g" % (case["E1"], case["E2"])),
        text,
    )
    text += ("\n! F15 harness settings\n"
             "! F15_CENTER_MEV %.17g\n"
             "! F15_E1_MEV %.17g\n"
             "! F15_E2_MEV %.17g\n"
             "! F15_J0 %.17g\n"
             "! F15_NT %d\n"
             "! F15_GRIDLESS_MPI_SCHEDULER %s\n"
             "! F15_GRIDLESS_MPI_DYNAMIC_CHUNK %d\n"
             "! F15_DS_NINTERVALS %d\n") % (
                 case["center"], case["E1"], case["E2"], case["j0"], nt,
                 scheduler, dynamic_chunk, nintervals)
    output_path.write_text(text)


def analyze_case(case_dir, case, ref, args):
    density_path = case_dir / "gridless_points_density.dat"
    spectrum_path = case_dir / "gridless_points_spectrum.dat"
    flux_path = case_dir / "gridless_points_flux.dat"
    missing = [str(p) for p in (density_path, spectrum_path, flux_path) if not p.exists()]
    if missing:
        raise RuntimeError("Missing required AMPS output files for %s: %s" % (case["label"], ", ".join(missing)))

    _, density_rows = read_table_file(density_path)
    _, flux_rows = read_table_file(flux_path)
    spectra = read_spectrum_file(spectrum_path)

    if len(density_rows) != 2:
        raise RuntimeError("%s: expected 2 density rows, found %d" % (case["label"], len(density_rows)))
    if len(flux_rows) != 2:
        raise RuntimeError("%s: expected 2 flux rows, found %d" % (case["label"], len(flux_rows)))
    if len(spectra) != 2:
        raise RuntimeError("%s: expected 2 spectrum zones, found %d" % (case["label"], len(spectra)))

    ref_flux = ref[(case["label"], "flux_total_m2s1")]
    ref_density = ref[(case["label"], "density_total_m3")]
    ref_vcenter = ref[(case["label"], "speed_center_mps")]
    ref_veff = ref[(case["label"], "speed_effective_mps")]
    ref_mono_ratio = ref[(case["label"], "mono_density_ratio")]

    rows = []
    passed = True

    def add_check(point, name, value, expected_value, rel_tol=None, abs_tol=None,
                  units="", note="", check_type="error_metric"):
        nonlocal_pass = True
        abs_err = abs(value - expected_value)
        rel_err = abs_err / max(abs(expected_value), 1.0e-300)
        ok = True
        if rel_tol is not None:
            ok = ok and (rel_err <= rel_tol)
        if abs_tol is not None:
            ok = ok and (abs_err <= abs_tol)
        rows.append({
            "case": case["label"],
            "center_MeV": case["center"],
            "E1_MeV": case["E1"],
            "E2_MeV": case["E2"],
            "point": point,
            "check": name,
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

    density_values = []
    flux_values = []
    for ip, (drow, frow, spec) in enumerate(zip(density_rows, flux_rows, spectra)):
        point_name = "point%d" % ip
        n_col = find_col(drow, ["N_m^-3", "N_m3", "density"])
        f_col = find_col(frow, ["F_tot_m2s1", "F_total", "F_tot"])
        if n_col is None:
            raise RuntimeError("Could not identify density column in %s" % density_path)
        if f_col is None:
            raise RuntimeError("Could not identify total-flux column in %s" % flux_path)

        density_file = drow[n_col]
        flux_file = frow[f_col]
        density_values.append(density_file)
        flux_values.append(flux_file)

        E = spec.get("E_MeV", [])
        T = spec.get("T", [])
        Jb = spec.get("J_boundary_perMeV", [])
        Jl = spec.get("J_local_perMeV", [])
        if not (len(E) == len(T) == len(Jb) == len(Jl)) or len(E) < 2:
            raise RuntimeError("Malformed spectrum zone in %s" % spectrum_path)
        if abs(E[0] - case["E1"]) > 1.0e-10 * max(case["E1"], 1.0):
            raise RuntimeError("%s: unexpected lower energy node %g" % (case["label"], E[0]))
        if abs(E[-1] - case["E2"]) > 1.0e-10 * max(case["E2"], 1.0):
            raise RuntimeError("%s: unexpected upper energy node %g" % (case["label"], E[-1]))

        max_t_err = max(abs(t - 1.0) for t in T)
        max_jb_rel = max(abs(j - case["j0"]) / max(abs(case["j0"]), 1.0e-300) for j in Jb)
        max_jloc_rel = max(abs(j - case["j0"]) / max(abs(case["j0"]), 1.0e-300) for j in Jl)
        max_jloc_ratio_err = 0.0
        for jb, jl in zip(Jb, Jl):
            if jb != 0.0:
                max_jloc_ratio_err = max(max_jloc_ratio_err, abs(jl / jb - 1.0))

        flux_spec = 4.0 * math.pi * trapz(E, Jl)
        density_spec = 4.0 * math.pi * trapz(E, [jl / speed_from_energy(e) for e, jl in zip(E, Jl)])
        veff_file = flux_file / density_file if density_file != 0.0 else float("nan")
        mono_ratio_file = density_file / (flux_file / ref_vcenter) if flux_file != 0.0 else float("nan")

        passed = add_check(point_name, "density_total_file", density_file, ref_density,
                           rel_tol=args.density_tol, units="m^-3",
                           check_type="physical_reference") and passed
        passed = add_check(point_name, "flux_total_file", flux_file, ref_flux,
                           rel_tol=args.flux_tol, units="m^-2 s^-1",
                           check_type="physical_reference") and passed
        passed = add_check(point_name, "effective_speed_flux_over_density", veff_file, ref_veff,
                           rel_tol=args.speed_tol, units="m s^-1",
                           note="checks relativistic 1/v density normalization",
                           check_type="physical_reference") and passed
        passed = add_check(point_name, "monoenergetic_density_ratio", mono_ratio_file, ref_mono_ratio,
                           rel_tol=args.mono_ratio_tol, units="1",
                           note="finite top-hat should match the analytical finite-width correction",
                           check_type="physical_reference") and passed
        passed = add_check(point_name, "max_abs_T_minus_1", max_t_err, 0.0,
                           abs_tol=args.transmission_tol) and passed
        passed = add_check(point_name, "max_abs_Jlocal_over_Jboundary_minus_1", max_jloc_ratio_err, 0.0,
                           abs_tol=args.transmission_tol) and passed
        passed = add_check(point_name, "max_rel_Jboundary_minus_tophat", max_jb_rel, 0.0,
                           abs_tol=args.differential_tol) and passed
        passed = add_check(point_name, "max_rel_Jlocal_minus_tophat", max_jloc_rel, 0.0,
                           abs_tol=args.differential_tol) and passed
        passed = add_check(point_name, "flux_total_spectrum_vs_file_rel",
                           abs(flux_spec - flux_file) / max(abs(flux_file), 1.0e-300), 0.0,
                           abs_tol=args.consistency_tol) and passed
        passed = add_check(point_name, "density_total_spectrum_vs_file_rel",
                           abs(density_spec - density_file) / max(abs(density_file), 1.0e-300), 0.0,
                           abs_tol=args.consistency_tol) and passed
        passed = add_check(point_name, "flux_total_from_spectrum", flux_spec, ref_flux,
                           rel_tol=args.flux_tol, units="m^-2 s^-1",
                           check_type="physical_reference") and passed
        passed = add_check(point_name, "density_total_from_spectrum", density_spec, ref_density,
                           rel_tol=args.density_tol, units="m^-3",
                           check_type="physical_reference") and passed

    d_spread = (max(density_values) - min(density_values)) / max(abs(ref_density), 1.0e-300)
    f_spread = (max(flux_values) - min(flux_values)) / max(abs(ref_flux), 1.0e-300)
    passed = add_check("all", "density_two_point_rel_spread", d_spread, 0.0,
                       abs_tol=args.spatial_tol,
                       note="center and near-boundary zero-field points must agree") and passed
    passed = add_check("all", "flux_two_point_rel_spread", f_spread, 0.0,
                       abs_tol=args.spatial_tol,
                       note="center and near-boundary zero-field points must agree") and passed

    return passed, rows


def run_case(case, case_dir, template, args, launch_dir):
    if case_dir.exists() and not args.keep:
        shutil.rmtree(str(case_dir))
    case_dir.mkdir(parents=True, exist_ok=True)
    render_input(template, case_dir / "AMPS_PARAM_F15.in", case, args.nt, args.scheduler,
                 args.dynamic_chunk, args.nintervals)
    write_top_hat_table(case_dir / "F15_top_hat_spectrum.dat", case["E1"], case["E2"], case["j0"])

    amps_path = Path(args.amps)
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()
    cmd = [
        args.mpirun, "-np", str(args.np), str(amps_path),
        "-mode", "gridless",
        "-i", "AMPS_PARAM_F15.in",
        "-mode3d-parallel", "THREADS",
        "-mode3d-threads", str(args.nt),
        "-mode3d-mpi-scheduler", args.scheduler,
        "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
        "-density-transmission", "DIRECT",
    ]
    print("Running %s:" % case["label"], " ".join(cmd))
    if args.dry_run:
        return 0
    with (case_dir / "F15_amps.log").open("w") as log:
        log.write("# Command: %s\n" % " ".join(cmd))
        log.flush()
        return subprocess.call(cmd, cwd=str(case_dir), stdout=log, stderr=subprocess.STDOUT)


def parse_centers(s):
    vals = []
    for tok in s.split(','):
        tok = tok.strip()
        if not tok:
            continue
        vals.append(float(tok))
    if not vals:
        raise argparse.ArgumentTypeError("at least one center energy is required")
    return vals


def parse_args():
    p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        F15: density normalization from differential flux.

        The test runs four zero-field gridless top-hat spectra centered at
        1, 10, 100, and 1000 MeV by default.  It checks the density relation
        n = 4*pi*int J(E)/v(E)dE and the corresponding integral flux
        F = 4*pi*int J(E)dE.  The finite-width top-hat density has a closed-form
        relativistic reference, so this directly tests the 1/v(E) factor.

        The script is expected to be launched from the directory containing the
        AMPS executable.  AMPS itself is launched with mpirun.  The default
        parallel configuration is -np 4 -nt 16.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/F15/run_F15.py
          python srcEarth/test/F15/run_F15.py -np 4 -nt 16 --nintervals 128
          python srcEarth/test/F15/run_F15.py -np 18 -nt 16 --scheduler DYNAMIC --dynamic-chunk 0 --centers 1,10,100,1000
          python srcEarth/test/F15/run_F15.py --width-frac 0.01 --density-tol 1e-6
          python srcEarth/test/F15/run_F15.py --dry-run --workdir test_output/F15_dryrun
          python srcEarth/test/F15/run_F15.py --skip-run --workdir test_output/F15_gridless
        """))
    p.add_argument("-np", type=int, default=4, help="number of MPI ranks; default: 4")
    p.add_argument("-nt", type=int, default=16, help="threads per rank; default: 16")
    p.add_argument("--amps", default="./amps", help="AMPS executable path; default: ./amps")
    p.add_argument("--mpirun", default="mpirun", help="MPI launcher; default: mpirun")
    p.add_argument("--workdir", default="test_output/F15_gridless", help="run/output directory")
    p.add_argument("--scheduler", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], default="DYNAMIC")
    p.add_argument("--dynamic-chunk", type=int, default=0)
    p.add_argument("--nintervals", type=int, default=128,
                   help="DS_NINTERVALS for each narrow top-hat run; default: 128")
    p.add_argument("--centers", type=parse_centers, default=CASE_CENTERS_DEFAULT,
                   help="comma-separated top-hat center energies in MeV; default: 1,10,100,1000")
    p.add_argument("--width-frac", type=float, default=WIDTH_FRAC_DEFAULT,
                   help="full top-hat width divided by center energy; default: 0.02")
    p.add_argument("--j0", type=float, default=J0_DEFAULT,
                   help="constant directional differential flux inside each top hat; default: 1")
    p.add_argument("--density-tol", type=float, default=2.0e-6,
                   help="relative tolerance for density against closed-form top-hat reference")
    p.add_argument("--flux-tol", type=float, default=2.0e-10,
                   help="relative tolerance for flux against closed-form top-hat reference")
    p.add_argument("--speed-tol", type=float, default=2.0e-6,
                   help="relative tolerance for F/n effective speed")
    p.add_argument("--mono-ratio-tol", type=float, default=2.0e-6,
                   help="relative tolerance for finite top-hat monoenergetic ratio")
    p.add_argument("--differential-tol", type=float, default=2.0e-10,
                   help="tolerance for differential top-hat values in the spectrum file")
    p.add_argument("--transmission-tol", type=float, default=1.0e-12,
                   help="absolute tolerance for T=1 and Jloc/Jb=1")
    p.add_argument("--consistency-tol", type=float, default=5.0e-10,
                   help="absolute tolerance for file-vs-spectrum relative differences")
    p.add_argument("--spatial-tol", type=float, default=5.0e-12,
                   help="absolute tolerance for center/off-center relative spread")
    p.add_argument("--skip-run", action="store_true", help="analyze existing case directories in --workdir")
    p.add_argument("--keep", action="store_true", help="do not delete existing case directories")
    p.add_argument("--dry-run", action="store_true", help="print AMPS commands and generate inputs without executing")
    return p.parse_args()


def main():
    args = parse_args()
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.nintervals < 4:
        raise SystemExit("--nintervals must be >= 4 for F15")
    if args.width_frac <= 0.0 or args.width_frac >= 1.0:
        raise SystemExit("--width-frac must be > 0 and < 1")
    if args.j0 <= 0.0:
        raise SystemExit("--j0 must be > 0")

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template = script_dir / "AMPS_PARAM_F15_gridless.in"
    workdir = (launch_dir / args.workdir).resolve()
    cases = build_cases(args.centers, args.width_frac, args.j0)

    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(str(workdir))
        workdir.mkdir(parents=True, exist_ok=True)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)

    generated_ref = workdir / "reference_F15_top_hat_generated.csv"
    workdir.mkdir(parents=True, exist_ok=True)
    write_reference_csv(generated_ref, cases)
    ref = read_reference_csv(generated_ref)

    if not args.skip_run:
        for case in cases:
            rc = run_case(case, workdir / case["label"], template, args, launch_dir)
            if rc != 0:
                print("AMPS failed for %s with exit code %d; see %s" %
                      (case["label"], rc, workdir / case["label"] / "F15_amps.log"))
                return rc
        if args.dry_run:
            print("F15 dry run complete. Generated inputs under %s" % workdir)
            return 0

    all_rows = []
    passed = True
    for case in cases:
        ok, rows = analyze_case(workdir / case["label"], case, ref, args)
        passed = ok and passed
        all_rows.extend(rows)

    summary_csv = workdir / "F15_summary.csv"
    with summary_csv.open("w", newline="") as f:
        fields = ["case", "center_MeV", "E1_MeV", "E2_MeV", "point", "check",
                  "check_type", "passed", "value", "expected_value", "abs_error",
                  "rel_error", "rel_tol", "abs_tol", "units", "note"]
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for r in all_rows:
            w.writerow(r)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "n_cases": len(cases),
        "n_checks": len(all_rows),
        "n_failed": sum(1 for r in all_rows if not r["passed"]),
        "workdir": str(workdir),
        "summary_csv": str(summary_csv),
        "reference_csv": str(generated_ref),
        "settings": {
            "centers_MeV": args.centers,
            "width_frac": args.width_frac,
            "j0": args.j0,
            "nintervals": args.nintervals,
        },
        "tolerances": {
            "density_tol": args.density_tol,
            "flux_tol": args.flux_tol,
            "speed_tol": args.speed_tol,
            "mono_ratio_tol": args.mono_ratio_tol,
            "differential_tol": args.differential_tol,
            "transmission_tol": args.transmission_tol,
            "consistency_tol": args.consistency_tol,
            "spatial_tol": args.spatial_tol,
        },
        "checks": all_rows,
    }
    result_json = workdir / "F15_result.json"
    with result_json.open("w") as f:
        json.dump(result, f, indent=2, sort_keys=True)

    print("F15 summary: %d cases, %d checks, %d failed" %
          (result["n_cases"], result["n_checks"], result["n_failed"]))
    print("  summary  : %s" % summary_csv)
    print("  result   : %s" % result_json)
    print("  reference: %s" % generated_ref)
    if not passed:
        print("Failed checks:")
        for r in all_rows:
            if not r["passed"]:
                print("  - {case}/{point}/{check}: type={check_type}, value={value:.6e}, expected={expected_value:.6e}, rel={rel_error:.3e}, abs={abs_error:.3e}".format(**r))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
