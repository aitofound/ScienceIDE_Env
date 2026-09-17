#!/usr/bin/env python3
"""
F5 — directional anisotropy / PAD mapping.

Run from the directory containing the AMPS executable:

    python srcEarth/test/F5/run_F5.py -np 4 -nt 16

Examples:

    python srcEarth/test/F5/run_F5.py -np 1 -nt 1
    python srcEarth/test/F5/run_F5.py -np 4 -nt 16 --complement-tol 1e-7
    python srcEarth/test/F5/run_F5.py --scan-n 128 --nintervals 96
    python srcEarth/test/F5/run_F5.py --semi-n-theta 240 --semi-n-phi 480
    python srcEarth/test/F5/run_F5.py --dry-run
    python srcEarth/test/F5/run_F5.py --skip-run --workdir test_output/F5_gridless

F5 validates a nontrivial pitch-angle-distribution mapping in the gridless
ANISOTROPIC density/spectrum path.  The exact reference identity is

    sin^2(alpha) + cos^2(alpha) = 1.

Therefore, with identical access trajectories and boundary spectrum,

    T_sin(E) + T_cos(E) = T_iso(E),
    J_local_sin(E) + J_local_cos(E) = J_local_iso(E),
    n_sin + n_cos = n_iso, and
    F_sin + F_cos = F_iso.

The runner also writes and checks a high-energy straight-line semi-analytic
reference for the expected PAD ratios at three diagnostic points.
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
from pathlib import Path

TEST_ID = "F5"
TEST_NAME = "Directional anisotropy / PAD mapping"

RE_KM = 6371.2
RE_M = RE_KM * 1000.0
BOX_RE = 9.0
R_INNER_RE = 1.01

PAD_EXPONENT_DEFAULT = 2.0
SEMI_N_THETA_DEFAULT = 200
SEMI_N_PHI_DEFAULT = 400

POINTS = [
    {"label": "TC_EQ8", "x_km": 8.0 * RE_KM, "y_km": 0.0, "z_km": 0.0,
     "description": "8 Re equator, GSM x-axis", "semi_tol": 0.12},
    {"label": "TC_PL8", "x_km": 0.0, "y_km": 0.0, "z_km": 8.0 * RE_KM,
     "description": "8 Re magnetic pole, +Z axis", "semi_tol": 0.12},
    {"label": "TC_EQ6", "x_km": 6.0 * RE_KM, "y_km": 0.0, "z_km": 0.0,
     "description": "6 Re equator, marginal straight-line case", "semi_tol": 0.18},
]

CASES = [
    {"label": "ISOTROPIC", "model": "ISOTROPIC", "exponent": PAD_EXPONENT_DEFAULT},
    {"label": "COSALPHA_N_n2", "model": "COSALPHA_N", "exponent": PAD_EXPONENT_DEFAULT},
    {"label": "SINALPHA_N_n2", "model": "SINALPHA_N", "exponent": PAD_EXPONENT_DEFAULT},
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


def scalar_rel_err(value, expected):
    return abs(value - expected) / max(abs(expected), 1.0e-300)


def max_vector_residual(a, b, c):
    """Return max abs and relative residual for a + b - c."""
    if not (len(a) == len(b) == len(c)):
        return float("inf"), float("inf")
    max_abs = 0.0
    max_rel = 0.0
    for x, y, z in zip(a, b, c):
        r = x + y - z
        ar = abs(r)
        rr = ar / max(abs(z), 1.0e-300)
        max_abs = max(max_abs, ar)
        max_rel = max(max_rel, rr)
    return max_abs, max_rel


def max_vector_pair_rel(a, b):
    if len(a) != len(b):
        return float("inf")
    max_rel = 0.0
    for x, y in zip(a, b):
        max_rel = max(max_rel, abs(x - y) / max(abs(x), abs(y), 1.0e-300))
    return max_rel


# ---------------------------------------------------------------------------
# Semi-analytic PAD reference for the high-energy straight-line limit.
# ---------------------------------------------------------------------------


def dot(a, b):
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]


def norm(v):
    return math.sqrt(max(0.0, dot(v, v)))


def unit(v):
    n = norm(v)
    if n == 0.0:
        return [0.0, 0.0, 0.0]
    return [v[0]/n, v[1]/n, v[2]/n]


def dipole_bhat(x_m):
    """Centered dipole direction for moment parallel to +Z.

    The normalization and sign do not affect COSALPHA_N/SINALPHA_N n=2 because
    only cos^2(alpha) is used, but the vector direction should match the model.
    """
    x, y, z = x_m
    r2 = x*x + y*y + z*z
    if r2 <= 0.0:
        return [0.0, 0.0, 1.0]
    r = math.sqrt(r2)
    rhat = [x/r, y/r, z/r]
    mhat = [0.0, 0.0, 1.0]
    mdotr = z/r
    # Dipole direction proportional to 3(m·rhat)rhat - m.
    b = [3.0*mdotr*rhat[0] - mhat[0],
         3.0*mdotr*rhat[1] - mhat[1],
         3.0*mdotr*rhat[2] - mhat[2]]
    return unit(b)


def ray_box_exit(x0_m, vhat, box_m):
    t_exit = float("inf")
    for i in range(3):
        if abs(vhat[i]) <= 1.0e-15:
            continue
        for wall in (-box_m, box_m):
            t = (wall - x0_m[i]) / vhat[i]
            if t > 0.0:
                t_exit = min(t_exit, t)
    if not math.isfinite(t_exit):
        return list(x0_m)
    return [x0_m[i] + t_exit*vhat[i] for i in range(3)]


def hits_inner_sphere(x0_m, vhat, r_inner_m):
    # Solve |x0 + t vhat|^2 = r_inner^2 for t>0.
    b = dot(x0_m, vhat)
    c = dot(x0_m, x0_m) - r_inner_m*r_inner_m
    disc = b*b - c
    if disc < 0.0:
        return False
    sqrt_disc = math.sqrt(disc)
    t1 = -b - sqrt_disc
    t2 = -b + sqrt_disc
    return (t1 > 0.0) or (t2 > 0.0)


def pad_weight(cos_alpha, model, exponent):
    ca = max(-1.0, min(1.0, cos_alpha))
    if model == "ISOTROPIC":
        return 1.0
    if model == "COSALPHA_N" or model == "BIDIRECTIONAL":
        if exponent == 0.0:
            return 1.0
        return abs(ca) ** exponent
    if model == "SINALPHA_N":
        if exponent == 0.0:
            return 1.0
        return max(0.0, 1.0 - ca*ca) ** (0.5 * exponent)
    raise ValueError("unknown PAD model: %s" % model)


def semi_analytic_ratio(point, model, exponent, n_theta, n_phi):
    x0_m = [point["x_km"] * 1000.0, point["y_km"] * 1000.0, point["z_km"] * 1000.0]
    box_m = BOX_RE * RE_M
    r_inner_m = R_INNER_RE * RE_M
    n_total = 0
    n_allowed = 0
    weighted_sum = 0.0

    for it in range(n_theta):
        # Uniform midpoint rule in mu=cos(theta).
        mu = -1.0 + (it + 0.5) * 2.0 / float(n_theta)
        st = math.sqrt(max(0.0, 1.0 - mu*mu))
        for ip in range(n_phi):
            phi = (ip + 0.5) * 2.0 * math.pi / float(n_phi)
            vhat = [st*math.cos(phi), st*math.sin(phi), mu]
            n_total += 1
            if hits_inner_sphere(x0_m, vhat, r_inner_m):
                continue
            n_allowed += 1
            x_exit = ray_box_exit(x0_m, vhat, box_m)
            bhat = dipole_bhat(x_exit)
            ca = dot(vhat, bhat)
            weighted_sum += pad_weight(ca, model, exponent)

    T_geo = n_allowed / float(n_total) if n_total else 0.0
    T_aniso = weighted_sum / float(n_total) if n_total else 0.0
    ratio = T_aniso / T_geo if T_geo > 0.0 else float("nan")
    return {"T_geo": T_geo, "T_aniso": T_aniso, "ratio": ratio}


def compute_references(n_theta, n_phi, exponent):
    refs = []
    for point in POINTS:
        for model in ("COSALPHA_N", "SINALPHA_N"):
            ref = semi_analytic_ratio(point, model, exponent, n_theta, n_phi)
            refs.append({
                "point": point["label"],
                "model": model,
                "exponent": exponent,
                "quantity": "density_ratio_to_isotropic",
                "expected_value": ref["ratio"],
                "T_geo": ref["T_geo"],
                "T_aniso": ref["T_aniso"],
                "units": "1",
                "check_type": "semi_analytic_straight_line",
                "notes": "High-energy straight-line reference using exact PAD function at dipole-boundary exit",
            })
    return refs


def write_reference_csv(path, refs):
    fields = ["point", "model", "exponent", "quantity", "expected_value",
              "T_geo", "T_aniso", "units", "check_type", "notes"]
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in refs:
            out = dict(row)
            for key in ("exponent", "expected_value", "T_geo", "T_aniso"):
                out[key] = "%.17e" % float(out[key])
            w.writerow(out)


def render_input(template_path, output_path, case, args):
    text = template_path.read_text()
    replacements = {
        "MODE3D_THREADS": str(args.nt),
        "GRIDLESS_MPI_SCHEDULER": args.scheduler,
        "GRIDLESS_MPI_DYNAMIC_CHUNK": str(args.dynamic_chunk),
        "DS_TRANSMISSION_SCAN_N": str(args.scan_n),
        "DS_NINTERVALS": str(args.nintervals),
        "BA_PAD_MODEL": case["model"],
        "BA_PAD_EXPONENT": "%.17g" % case["exponent"],
    }
    for key, val in replacements.items():
        text = re.sub(r'(?m)^(\s*' + re.escape(key) + r'\s+)\S+', r'\g<1>' + val, text)
    text += ("\n! F5 harness settings\n"
             "! F5_CASE %s\n"
             "! F5_PAD_MODEL %s\n"
             "! F5_PAD_EXPONENT %.17g\n"
             "! F5_NT %d\n"
             "! F5_GRIDLESS_MPI_SCHEDULER %s\n"
             "! F5_GRIDLESS_MPI_DYNAMIC_CHUNK %d\n"
             "! F5_DS_TRANSMISSION_SCAN_N %d\n"
             "! F5_DS_NINTERVALS %d\n") % (
                 case["label"], case["model"], case["exponent"], args.nt,
                 args.scheduler, args.dynamic_chunk, args.scan_n, args.nintervals)
    output_path.write_text(text)


def run_case(case, case_dir, template, args, launch_dir):
    if case_dir.exists() and not args.keep:
        shutil.rmtree(str(case_dir))
    case_dir.mkdir(parents=True, exist_ok=True)
    render_input(template, case_dir / "AMPS_PARAM_F5.in", case, args)

    amps_path = Path(args.amps)
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()
    cmd = [
        args.mpirun, "-np", str(args.np), str(amps_path),
        "-mode", "gridless",
        "-i", "AMPS_PARAM_F5.in",
        "-mode3d-parallel", "THREADS",
        "-mode3d-threads", str(args.nt),
        "-mode3d-mpi-scheduler", args.scheduler,
        "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
        "-density-mode", "ANISOTROPIC",
        "-density-transmission", "SCAN",
    ]
    print("Running %s:" % case["label"], " ".join(cmd))
    if args.dry_run:
        return 0
    with (case_dir / "F5_amps.log").open("w") as log:
        log.write("# Command: %s\n" % " ".join(cmd))
        log.flush()
        return subprocess.call(cmd, cwd=str(case_dir), stdout=log, stderr=subprocess.STDOUT)


def read_case_outputs(case_dir):
    density_path = case_dir / "gridless_points_density.dat"
    flux_path = case_dir / "gridless_points_flux.dat"
    spectrum_path = case_dir / "gridless_points_spectrum.dat"
    missing = [str(p) for p in (density_path, flux_path, spectrum_path) if not p.exists()]
    if missing:
        raise RuntimeError("Missing required AMPS output files: %s" % ", ".join(missing))

    _, density_rows = read_table_file(density_path)
    _, flux_rows = read_table_file(flux_path)
    spectra = read_spectrum_file(spectrum_path)
    if not (len(density_rows) == len(flux_rows) == len(spectra)):
        raise RuntimeError("Output size mismatch in %s: density=%d flux=%d spectra=%d" %
                           (case_dir, len(density_rows), len(flux_rows), len(spectra)))
    if len(density_rows) != len(POINTS):
        raise RuntimeError("Expected %d point rows in %s, found %d" %
                           (len(POINTS), case_dir, len(density_rows)))

    points = []
    for ip, (drow, frow, spec) in enumerate(zip(density_rows, flux_rows, spectra)):
        n_col = find_col(drow, ["N_m^-3", "N_m3", "density", "n"])
        if n_col is None:
            raise RuntimeError("Could not identify density column in %s" % density_path)
        flux_cols = []
        for k in frow.keys():
            nk = _norm_key(k)
            if nk in ("x_km", "y_km", "z_km", "x", "y", "z"):
                continue
            if nk.startswith("f") or "flux" in nk or "ch_" in nk or nk.startswith("ch"):
                flux_cols.append(k)
        if not flux_cols:
            f_col = find_col(frow, ["F_tot_m2s1", "F_total", "F_tot"])
            if f_col is not None:
                flux_cols.append(f_col)
        for key in ("E_MeV", "T", "J_boundary_perMeV", "J_local_perMeV"):
            if key not in spec:
                raise RuntimeError("Spectrum output %s is missing column %s" % (spectrum_path, key))
        n = len(spec["E_MeV"])
        if n < 2 or any(len(spec[k]) != n for k in ("T", "J_boundary_perMeV", "J_local_perMeV")):
            raise RuntimeError("Malformed spectrum zone in %s" % spectrum_path)
        points.append({
            "point": POINTS[ip]["label"],
            "density": drow[n_col],
            "flux": dict((k, frow[k]) for k in flux_cols),
            "E_MeV": spec["E_MeV"],
            "T": spec["T"],
            "J_boundary_perMeV": spec["J_boundary_perMeV"],
            "J_local_perMeV": spec["J_local_perMeV"],
        })
    return points


def add_row(rows, check, point, quantity, value, expected_value, tol, units, check_type, note):
    abs_error = abs(value - expected_value)
    rel_error = abs_error / max(abs(expected_value), 1.0e-300) if expected_value != 0.0 else abs_error
    ok = abs_error <= tol
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
        "tol": tol,
        "units": units,
        "note": note,
    })
    return ok


def analyze(outputs, refs, args):
    iso = outputs["ISOTROPIC"]
    cos = outputs["COSALPHA_N_n2"]
    sin = outputs["SINALPHA_N_n2"]
    rows = []
    passed = True

    # Exact sin^2 + cos^2 = 1 complement checks.
    for ip, point in enumerate(POINTS):
        p = point["label"]
        di, dc, ds = iso[ip], cos[ip], sin[ip]

        # Make sure all cases used the same energy grid and boundary spectrum.
        for case_label, data in (("COSALPHA_N_n2", dc), ("SINALPHA_N_n2", ds)):
            e_rel = max_vector_pair_rel(data["E_MeV"], di["E_MeV"])
            passed = add_row(rows, "same_energy_grid", p,
                             "%s_energy_grid_relative_difference" % case_label,
                             e_rel, 0.0, args.energy_grid_tol, "1", "setup_identity",
                             "all PAD cases must use the same energy grid") and passed
            jb_rel = max_vector_pair_rel(data["J_boundary_perMeV"], di["J_boundary_perMeV"])
            passed = add_row(rows, "same_boundary_spectrum", p,
                             "%s_J_boundary_relative_difference" % case_label,
                             jb_rel, 0.0, args.complement_tol, "1", "setup_identity",
                             "all PAD cases must use the same boundary spectrum") and passed

        value = (ds["density"] + dc["density"] - di["density"]) / max(abs(di["density"]), 1.0e-300)
        passed = add_row(rows, "sin2_plus_cos2_complement", p,
                         "density_relative_residual", value, 0.0,
                         args.complement_tol, "1", "exact_pad_identity",
                         "n_sin + n_cos must equal n_iso because sin^2 + cos^2 = 1") and passed

        common_flux_cols = sorted(set(di["flux"].keys()).intersection(set(dc["flux"].keys())).intersection(set(ds["flux"].keys())))
        if not common_flux_cols:
            raise RuntimeError("No common flux columns for complement check at %s" % p)
        for col in common_flux_cols:
            denom = max(abs(di["flux"][col]), 1.0e-300)
            value = (ds["flux"][col] + dc["flux"][col] - di["flux"][col]) / denom
            passed = add_row(rows, "sin2_plus_cos2_complement", p,
                             "flux_%s_relative_residual" % col,
                             value, 0.0, args.complement_tol, "1", "exact_pad_identity",
                             "F_sin + F_cos must equal F_iso for every integral channel") and passed

        for key in ("T", "J_local_perMeV"):
            _, rel = max_vector_residual(ds[key], dc[key], di[key])
            passed = add_row(rows, "sin2_plus_cos2_complement", p,
                             "%s_max_relative_residual" % key,
                             rel, 0.0, args.complement_tol, "1", "exact_pad_identity",
                             "%s_sin + %s_cos must equal %s_iso at each energy" % (key, key, key)) and passed

    # Semi-analytic straight-line mapping checks.  This is intentionally looser
    # than the exact complement test because it neglects finite magnetic bending.
    ref_map = dict(((r["point"], r["model"]), r) for r in refs)
    if not args.skip_semianalytic:
        for ip, point in enumerate(POINTS):
            p = point["label"]
            for case_label, model in (("COSALPHA_N_n2", "COSALPHA_N"),
                                      ("SINALPHA_N_n2", "SINALPHA_N")):
                ref = ref_map[(p, model)]
                expected = ref["expected_value"]
                value = outputs[case_label][ip]["density"] / max(outputs["ISOTROPIC"][ip]["density"], 1.0e-300)
                # args.semianalytic_tol is a global override.  When it is
                # negative, use the documented per-point tolerance from POINTS.
                tol = point["semi_tol"] if args.semianalytic_tol < 0.0 else args.semianalytic_tol
                passed = add_row(rows, "semi_analytic_pad_mapping", p,
                                 "%s_density_ratio_to_isotropic" % model,
                                 value, expected, tol, "1", "semi_analytic_straight_line",
                                 "density ratio compared with high-energy straight-line PAD mapping") and passed

    return passed, rows


def write_json(path, obj):
    with path.open("w") as f:
        json.dump(obj, f, indent=2, sort_keys=True)
        f.write("\n")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="F5 directional anisotropy / PAD mapping test",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  python srcEarth/test/F5/run_F5.py -np 4 -nt 16
  python srcEarth/test/F5/run_F5.py -np 1 -nt 1 --complement-tol 1e-7
  python srcEarth/test/F5/run_F5.py --scan-n 128 --nintervals 96
  python srcEarth/test/F5/run_F5.py --semi-n-theta 240 --semi-n-phi 480
  python srcEarth/test/F5/run_F5.py --dry-run
  python srcEarth/test/F5/run_F5.py --skip-run --workdir test_output/F5_gridless
""",
    )
    parser.add_argument("-np", type=int, default=4, help="MPI ranks passed to mpirun")
    parser.add_argument("-nt", type=int, default=16, help="threads per MPI rank")
    parser.add_argument("--amps", default="./amps", help="path to AMPS executable")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher")
    parser.add_argument("--workdir", default="test_output/F5_gridless", help="output directory")
    parser.add_argument("--scheduler", default="DYNAMIC", choices=["STATIC", "BLOCK_CYCLIC", "DYNAMIC"],
                        help="GRIDLESS_MPI_SCHEDULER value")
    parser.add_argument("--dynamic-chunk", type=int, default=0,
                        help="GRIDLESS_MPI_DYNAMIC_CHUNK value; 0 means AMPS auto chunking")
    parser.add_argument("--scan-n", type=int, default=96,
                        help="DS_TRANSMISSION_SCAN_N value for SCAN transmission mode")
    parser.add_argument("--nintervals", type=int, default=64,
                        help="DS_NINTERVALS value")
    parser.add_argument("--pad-exponent", type=float, default=PAD_EXPONENT_DEFAULT,
                        help="BA_PAD_EXPONENT for COSALPHA_N and SINALPHA_N; default is 2")
    parser.add_argument("--semi-n-theta", type=int, default=SEMI_N_THETA_DEFAULT,
                        help="theta samples for semi-analytic straight-line reference")
    parser.add_argument("--semi-n-phi", type=int, default=SEMI_N_PHI_DEFAULT,
                        help="phi samples for semi-analytic straight-line reference")
    parser.add_argument("--complement-tol", type=float, default=1.0e-7,
                        help="absolute tolerance for exact sin^2+cos^2 complement residuals")
    parser.add_argument("--energy-grid-tol", type=float, default=1.0e-12,
                        help="relative tolerance for comparing spectrum energy grids")
    parser.add_argument("--semianalytic-tol", type=float, default=-1.0,
                        help="override tolerance for semi-analytic density-ratio checks; negative uses per-point defaults")
    parser.add_argument("--skip-semianalytic", action="store_true",
                        help="skip the approximate straight-line PAD-ratio checks")
    parser.add_argument("--dry-run", action="store_true", help="render inputs and print commands without running AMPS")
    parser.add_argument("--skip-run", action="store_true", help="skip AMPS execution and analyze existing workdir")
    parser.add_argument("--keep", action="store_true", help="keep existing case directories when running")
    args = parser.parse_args(argv)

    if args.pad_exponent < 0.0:
        raise SystemExit("--pad-exponent must be >= 0")
    if args.nintervals < 2:
        raise SystemExit("--nintervals must be >= 2")
    if args.scan_n < 2:
        raise SystemExit("--scan-n must be >= 2")
    if args.semi_n_theta < 2 or args.semi_n_phi < 4:
        raise SystemExit("semi-analytic grid must have --semi-n-theta >= 2 and --semi-n-phi >= 4")

    cases = []
    for c in CASES:
        cc = dict(c)
        if cc["model"] in ("COSALPHA_N", "SINALPHA_N"):
            cc["exponent"] = args.pad_exponent
            suffix = "n%d" % int(round(args.pad_exponent)) if abs(args.pad_exponent - round(args.pad_exponent)) < 1.0e-12 else ("n%.12g" % args.pad_exponent).replace(".", "p")
            cc["label"] = "%s_%s" % (cc["model"], suffix)
        cases.append(cc)

    # The analyzer expects the default labels.  Keep the validation identity
    # focused on n=2, which is the F5 test definition from validation.docx.
    if abs(args.pad_exponent - 2.0) > 1.0e-12:
        raise SystemExit("F5 is defined for BA_PAD_EXPONENT=2. Use F11 for exponent matrix checks.")

    launch_dir = Path.cwd()
    script_dir = Path(__file__).resolve().parent
    template = script_dir / "AMPS_PARAM_F5_gridless.in"
    workdir = (launch_dir / args.workdir).resolve()

    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(str(workdir))
        workdir.mkdir(parents=True, exist_ok=True)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    refs = compute_references(args.semi_n_theta, args.semi_n_phi, args.pad_exponent)
    reference_csv = workdir / "reference_F5_pad_mapping_used.csv"
    write_reference_csv(reference_csv, refs)

    if not args.skip_run:
        for case in cases:
            rc = run_case(case, workdir / case["label"], template, args, launch_dir)
            if rc != 0:
                print("AMPS failed for %s with exit code %d; see %s" %
                      (case["label"], rc, workdir / case["label"] / "F5_amps.log"))
                return rc
        if args.dry_run:
            print("F5 dry run complete. Generated inputs under %s" % workdir)
            print("Reference:", reference_csv)
            return 0

    outputs = {}
    for case in cases:
        outputs[case["label"]] = read_case_outputs(workdir / case["label"])

    passed, rows = analyze(outputs, refs, args)

    summary_csv = workdir / "F5_summary.csv"
    fields = ["check", "point", "quantity", "check_type", "passed", "value",
              "expected_value", "abs_error", "rel_error", "tol", "units", "note"]
    with summary_csv.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in rows:
            w.writerow(row)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "n_cases": len(cases),
        "n_checks": len(rows),
        "n_failed": sum(1 for r in rows if not r["passed"]),
        "workdir": str(workdir),
        "summary_csv": str(summary_csv),
        "reference_csv": str(reference_csv),
        "cases": cases,
        "points": POINTS,
        "settings": {
            "np": args.np,
            "nt": args.nt,
            "scheduler": args.scheduler,
            "dynamic_chunk": args.dynamic_chunk,
            "scan_n": args.scan_n,
            "nintervals": args.nintervals,
            "pad_exponent": args.pad_exponent,
            "semi_n_theta": args.semi_n_theta,
            "semi_n_phi": args.semi_n_phi,
        },
        "tolerances": {
            "complement_tol": args.complement_tol,
            "energy_grid_tol": args.energy_grid_tol,
            "semianalytic_tol": args.semianalytic_tol,
        },
    }
    result_json = workdir / "F5_result.json"
    write_json(result_json, result)

    print("F5 %s" % ("PASS" if passed else "FAIL"))
    print("Summary:", summary_csv)
    print("Result :", result_json)
    return 0 if passed else 2


if __name__ == "__main__":
    sys.exit(main())
