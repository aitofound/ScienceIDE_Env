#!/usr/bin/env python3
"""
F11 — anisotropic PAD model sum-check.

Run from the directory containing the AMPS executable:

    python srcEarth/test/F11/run_F11.py -np 4 -nt 16

Examples:

    python srcEarth/test/F11/run_F11.py -np 1 -nt 1
    python srcEarth/test/F11/run_F11.py -np 4 -nt 16 --identity-tol 1e-8
    python srcEarth/test/F11/run_F11.py --models ISOTROPIC,SINALPHA_N,COSALPHA_N,BIDIRECTIONAL --exponents 0,2,4
    python srcEarth/test/F11/run_F11.py --dry-run
    python srcEarth/test/F11/run_F11.py --skip-run --workdir test_output/F11_gridless

The reference solution is a set of exact PAD identities:

  * BA_PAD_EXPONENT=0 makes every implemented PAD model isotropic.
  * COSALPHA_N and BIDIRECTIONAL use the same |cos(alpha)|^n formula.

The runner compares density, integral flux channels, T(E), J_boundary(E), and
J_local(E) pairwise between those analytically identical cases.
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

TEST_ID = "F11"
TEST_NAME = "Anisotropic PAD model sum-check"
PAD_MODELS_DEFAULT = ["ISOTROPIC", "SINALPHA_N", "COSALPHA_N", "BIDIRECTIONAL"]
EXPONENTS_DEFAULT = [0.0, 1.0, 2.0, 4.0, 8.0]


def exponent_label(n):
    if abs(n - round(n)) < 1.0e-12:
        return "n%d" % int(round(n))
    s = ("%.12g" % n).replace("-", "m").replace(".", "p")
    return "n" + s


def case_label(model, exponent):
    return "%s_%s" % (model.upper(), exponent_label(exponent))


def parse_csv_list(text, cast=str):
    vals = []
    for tok in text.split(','):
        tok = tok.strip()
        if tok:
            vals.append(cast(tok))
    if not vals:
        raise argparse.ArgumentTypeError("empty comma-separated list")
    return vals


def parse_models(text):
    allowed = set(PAD_MODELS_DEFAULT)
    models = [m.upper() for m in parse_csv_list(text, str)]
    bad = [m for m in models if m not in allowed]
    if bad:
        raise argparse.ArgumentTypeError("unsupported PAD model(s): %s" % ",".join(bad))
    return models


def parse_exponents(text):
    vals = parse_csv_list(text, float)
    for v in vals:
        if v < 0.0:
            raise argparse.ArgumentTypeError("BA_PAD_EXPONENT must be >= 0")
    return vals


def build_cases(models, exponents):
    """Build a compact model/exponent matrix for F11.

    ISOTROPIC does not depend on exponent, so one ISOTROPIC_n0 case is enough
    unless the caller explicitly omits exponent 0.  All other models are run for
    the requested exponent list.
    """
    cases = []
    for model in models:
        if model == "ISOTROPIC":
            cases.append({"model": model, "exponent": 0.0, "label": case_label(model, 0.0)})
        else:
            for exponent in exponents:
                cases.append({"model": model, "exponent": exponent, "label": case_label(model, exponent)})
    # Preserve order while removing possible duplicates.
    out = []
    seen = set()
    for case in cases:
        if case["label"] not in seen:
            seen.add(case["label"])
            out.append(case)
    return out


def identity_pairs(cases):
    labels = set(c["label"] for c in cases)
    pairs = []
    iso = case_label("ISOTROPIC", 0.0)
    for model in ("SINALPHA_N", "COSALPHA_N", "BIDIRECTIONAL"):
        lab = case_label(model, 0.0)
        if iso in labels and lab in labels:
            pairs.append({
                "check": "n0_identity",
                "case_a": iso,
                "case_b": lab,
                "expected_value": 0.0,
                "check_type": "exact_identity",
                "notes": "BA_PAD_EXPONENT=0 makes every PAD model isotropic",
            })
    for exponent in EXPONENTS_DEFAULT:
        a = case_label("COSALPHA_N", exponent)
        b = case_label("BIDIRECTIONAL", exponent)
        if a in labels and b in labels:
            pairs.append({
                "check": "cos_bidirectional_identity",
                "case_a": a,
                "case_b": b,
                "expected_value": 0.0,
                "check_type": "exact_identity",
                "notes": "COSALPHA_N and BIDIRECTIONAL share |cos(alpha)|^n",
            })
    # Also honor non-default exponents supplied by the user.
    exps = sorted(set(c["exponent"] for c in cases))
    for exponent in exps:
        a = case_label("COSALPHA_N", exponent)
        b = case_label("BIDIRECTIONAL", exponent)
        if a in labels and b in labels and not any(p["case_a"] == a and p["case_b"] == b for p in pairs):
            pairs.append({
                "check": "cos_bidirectional_identity",
                "case_a": a,
                "case_b": b,
                "expected_value": 0.0,
                "check_type": "exact_identity",
                "notes": "COSALPHA_N and BIDIRECTIONAL share |cos(alpha)|^n",
            })
    return pairs


def write_reference_csv(path, pairs):
    fields = ["check", "case_a", "case_b", "quantity", "expected_value", "units", "check_type", "notes"]
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for p in pairs:
            w.writerow({
                "check": p["check"],
                "case_a": p["case_a"],
                "case_b": p["case_b"],
                "quantity": "relative_difference",
                "expected_value": "%.17e" % p["expected_value"],
                "units": "1",
                "check_type": p["check_type"],
                "notes": p["notes"],
            })


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


def vector_rel_diff(a, b):
    max_rel = 0.0
    max_abs = 0.0
    if len(a) != len(b):
        return float("inf"), float("inf")
    for x, y in zip(a, b):
        abs_err = abs(x - y)
        rel_err = abs_err / max(abs(x), abs(y), 1.0e-300)
        if rel_err > max_rel:
            max_rel = rel_err
        if abs_err > max_abs:
            max_abs = abs_err
    return max_rel, max_abs


def scalar_rel_diff(a, b):
    return abs(a - b) / max(abs(a), abs(b), 1.0e-300)


def render_input(template_path, output_path, case, nt, scheduler, dynamic_chunk, scan_n):
    text = template_path.read_text()
    replacements = {
        "MODE3D_THREADS": str(nt),
        "GRIDLESS_MPI_SCHEDULER": scheduler,
        "GRIDLESS_MPI_DYNAMIC_CHUNK": str(dynamic_chunk),
        "DS_TRANSMISSION_SCAN_N": str(scan_n),
        "BA_PAD_MODEL": case["model"],
        "BA_PAD_EXPONENT": "%.17g" % case["exponent"],
    }
    for key, val in replacements.items():
        text = re.sub(r'(?m)^(\s*' + re.escape(key) + r'\s+)\S+', r'\g<1>' + val, text)
    text += ("\n! F11 harness settings\n"
             "! F11_CASE %s\n"
             "! F11_PAD_MODEL %s\n"
             "! F11_PAD_EXPONENT %.17g\n"
             "! F11_NT %d\n"
             "! F11_GRIDLESS_MPI_SCHEDULER %s\n"
             "! F11_GRIDLESS_MPI_DYNAMIC_CHUNK %d\n"
             "! F11_DS_TRANSMISSION_SCAN_N %d\n") % (
                 case["label"], case["model"], case["exponent"], nt,
                 scheduler, dynamic_chunk, scan_n)
    output_path.write_text(text)


def run_case(case, case_dir, template, args, launch_dir):
    if case_dir.exists() and not args.keep:
        shutil.rmtree(str(case_dir))
    case_dir.mkdir(parents=True, exist_ok=True)
    render_input(template, case_dir / "AMPS_PARAM_F11.in", case, args.nt,
                 args.scheduler, args.dynamic_chunk, args.scan_n)

    amps_path = Path(args.amps)
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()
    cmd = [
        args.mpirun, "-np", str(args.np), str(amps_path),
        "-mode", "gridless",
        "-i", "AMPS_PARAM_F11.in",
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
    with (case_dir / "F11_amps.log").open("w") as log:
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
    if not density_rows:
        raise RuntimeError("No point rows found in %s" % case_dir)

    points = []
    for ip, (drow, frow, spec) in enumerate(zip(density_rows, flux_rows, spectra)):
        n_col = find_col(drow, ["N_m^-3", "N_m3", "density", "n"])
        if n_col is None:
            raise RuntimeError("Could not identify density column in %s" % density_path)
        # Keep all flux-like numeric columns except coordinates, so named channels are compared too.
        flux_cols = []
        for k in frow.keys():
            nk = k.lower().replace(" ", "_").replace("-", "_")
            if nk in ("x_km", "y_km", "z_km", "x", "y", "z"):
                continue
            if nk.startswith("f") or "flux" in nk or "ch_" in nk or nk.startswith("ch"):
                flux_cols.append(k)
        if not flux_cols:
            f_col = find_col(frow, ["F_tot_m2s1", "F_total", "F_tot"])
            if f_col is not None:
                flux_cols.append(f_col)
        E = spec.get("E_MeV", [])
        T = spec.get("T", [])
        Jb = spec.get("J_boundary_perMeV", [])
        Jl = spec.get("J_local_perMeV", [])
        if not (len(E) == len(T) == len(Jb) == len(Jl)) or len(E) < 2:
            raise RuntimeError("Malformed spectrum zone in %s" % spectrum_path)
        points.append({
            "point": "point%d" % ip,
            "density": drow[n_col],
            "flux": dict((k, frow[k]) for k in flux_cols),
            "E_MeV": E,
            "T": T,
            "J_boundary_perMeV": Jb,
            "J_local_perMeV": Jl,
        })
    return points


def compare_case_pair(case_a_label, a_points, case_b_label, b_points, pair, args):
    rows = []
    passed = True
    if len(a_points) != len(b_points):
        raise RuntimeError("Point-count mismatch for %s vs %s" % (case_a_label, case_b_label))

    def add(point, quantity, value, expected_value, tol, units="1", note=""):
        nonlocal_pass = True
        abs_err = abs(value - expected_value)
        rel_err = abs_err / max(abs(expected_value), 1.0e-300) if expected_value != 0.0 else abs_err
        ok = abs_err <= tol
        rows.append({
            "identity_check": pair["check"],
            "case_a": case_a_label,
            "case_b": case_b_label,
            "point": point,
            "quantity": quantity,
            "check_type": pair["check_type"],
            "passed": ok,
            "value": value,
            "expected_value": expected_value,
            "abs_error": abs_err,
            "rel_error": rel_err,
            "tol": tol,
            "units": units,
            "note": note or pair["notes"],
        })
        return ok

    for a, b in zip(a_points, b_points):
        point = a["point"]
        passed = add(point, "density_relative_difference",
                     scalar_rel_diff(a["density"], b["density"]), 0.0, args.identity_tol) and passed
        common_flux_cols = sorted(set(a["flux"].keys()).intersection(set(b["flux"].keys())))
        if not common_flux_cols:
            raise RuntimeError("No common flux columns for %s vs %s" % (case_a_label, case_b_label))
        for col in common_flux_cols:
            passed = add(point, "flux_%s_relative_difference" % col,
                         scalar_rel_diff(a["flux"][col], b["flux"][col]), 0.0,
                         args.identity_tol) and passed

        e_rel, e_abs = vector_rel_diff(a["E_MeV"], b["E_MeV"])
        passed = add(point, "energy_grid_relative_difference", e_rel, 0.0,
                     args.energy_grid_tol, note="energy grid must match before spectrum identity checks") and passed
        if e_abs > args.energy_abs_tol:
            passed = add(point, "energy_grid_absolute_difference_MeV", e_abs, 0.0,
                         args.energy_abs_tol, units="MeV",
                         note="energy grid must match before spectrum identity checks") and passed
        for key in ("T", "J_boundary_perMeV", "J_local_perMeV"):
            rel, _ = vector_rel_diff(a[key], b[key])
            passed = add(point, "%s_relative_difference" % key,
                         rel, 0.0, args.identity_tol) and passed
    return passed, rows


def write_json(path, obj):
    with path.open("w") as f:
        json.dump(obj, f, indent=2, sort_keys=True)
        f.write("\n")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="F11 anisotropic PAD model sum-check",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  python srcEarth/test/F11/run_F11.py -np 4 -nt 16
  python srcEarth/test/F11/run_F11.py -np 1 -nt 1 --identity-tol 1e-8
  python srcEarth/test/F11/run_F11.py --models ISOTROPIC,COSALPHA_N,BIDIRECTIONAL --exponents 0,2,4
  python srcEarth/test/F11/run_F11.py --dry-run
  python srcEarth/test/F11/run_F11.py --skip-run --workdir test_output/F11_gridless
""",
    )
    parser.add_argument("-np", type=int, default=4, help="MPI ranks passed to mpirun")
    parser.add_argument("-nt", type=int, default=16, help="threads per MPI rank")
    parser.add_argument("--amps", default="./amps", help="path to AMPS executable")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher")
    parser.add_argument("--workdir", default="test_output/F11_gridless", help="output directory")
    parser.add_argument("--models", type=parse_models,
                        default=PAD_MODELS_DEFAULT,
                        help="comma-separated PAD models to run")
    parser.add_argument("--exponents", type=parse_exponents,
                        default=EXPONENTS_DEFAULT,
                        help="comma-separated BA_PAD_EXPONENT values for non-isotropic models")
    parser.add_argument("--scheduler", default="DYNAMIC", choices=["STATIC", "BLOCK_CYCLIC", "DYNAMIC"],
                        help="GRIDLESS_MPI_SCHEDULER value")
    parser.add_argument("--dynamic-chunk", type=int, default=0,
                        help="GRIDLESS_MPI_DYNAMIC_CHUNK value; 0 means AMPS auto chunking")
    parser.add_argument("--scan-n", type=int, default=64,
                        help="DS_TRANSMISSION_SCAN_N value for SCAN transmission mode")
    parser.add_argument("--identity-tol", type=float, default=1.0e-8,
                        help="relative tolerance for exact PAD identity comparisons")
    parser.add_argument("--energy-grid-tol", type=float, default=1.0e-12,
                        help="relative tolerance for comparing spectrum energy grids")
    parser.add_argument("--energy-abs-tol", type=float, default=1.0e-9,
                        help="absolute tolerance in MeV for comparing spectrum energy grids")
    parser.add_argument("--dry-run", action="store_true", help="render inputs and print commands without running AMPS")
    parser.add_argument("--skip-run", action="store_true", help="skip AMPS execution and analyze existing workdir")
    parser.add_argument("--keep", action="store_true", help="keep existing case directories when running")
    args = parser.parse_args(argv)

    launch_dir = Path.cwd()
    script_dir = Path(__file__).resolve().parent
    template = script_dir / "AMPS_PARAM_F11_gridless.in"
    workdir = (launch_dir / args.workdir).resolve()
    cases = build_cases(args.models, args.exponents)
    pairs = identity_pairs(cases)
    if not pairs:
        raise SystemExit("F11 needs at least one exact identity pair. Include ISOTROPIC plus another n=0 model, or COSALPHA_N and BIDIRECTIONAL.")

    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(str(workdir))
        workdir.mkdir(parents=True, exist_ok=True)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    reference_csv = workdir / "reference_F11_pad_identities_used.csv"
    write_reference_csv(reference_csv, pairs)

    if not args.skip_run:
        for case in cases:
            rc = run_case(case, workdir / case["label"], template, args, launch_dir)
            if rc != 0:
                print("AMPS failed for %s with exit code %d; see %s" %
                      (case["label"], rc, workdir / case["label"] / "F11_amps.log"))
                return rc
        if args.dry_run:
            print("F11 dry run complete. Generated inputs under %s" % workdir)
            return 0

    outputs = {}
    for case in cases:
        outputs[case["label"]] = read_case_outputs(workdir / case["label"])

    all_rows = []
    passed = True
    for pair in pairs:
        ok, rows = compare_case_pair(pair["case_a"], outputs[pair["case_a"]],
                                     pair["case_b"], outputs[pair["case_b"]],
                                     pair, args)
        passed = ok and passed
        all_rows.extend(rows)

    summary_csv = workdir / "F11_summary.csv"
    fields = ["identity_check", "case_a", "case_b", "point", "quantity",
              "check_type", "passed", "value", "expected_value", "abs_error",
              "rel_error", "tol", "units", "note"]
    with summary_csv.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in all_rows:
            w.writerow(row)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "n_cases": len(cases),
        "n_identity_pairs": len(pairs),
        "n_checks": len(all_rows),
        "n_failed": sum(1 for r in all_rows if not r["passed"]),
        "workdir": str(workdir),
        "summary_csv": str(summary_csv),
        "reference_csv": str(reference_csv),
        "cases": cases,
        "identity_pairs": pairs,
        "settings": {
            "np": args.np,
            "nt": args.nt,
            "scheduler": args.scheduler,
            "dynamic_chunk": args.dynamic_chunk,
            "scan_n": args.scan_n,
        },
        "tolerances": {
            "identity_tol": args.identity_tol,
            "energy_grid_tol": args.energy_grid_tol,
            "energy_abs_tol": args.energy_abs_tol,
        },
    }
    result_json = workdir / "F11_result.json"
    write_json(result_json, result)

    print("F11 %s" % ("PASS" if passed else "FAIL"))
    print("Summary:", summary_csv)
    print("Result :", result_json)
    return 0 if passed else 2


if __name__ == "__main__":
    sys.exit(main())
