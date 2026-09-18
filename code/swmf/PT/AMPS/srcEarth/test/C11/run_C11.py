#!/usr/bin/env python3
"""
C11 — Penumbra-pocket regression: legacy BINARY Rmin-collapse defect.

Run from the directory that contains the AMPS executable:

    python srcEarth/test/C11/run_C11.py -np 4 -nt 16

C11 targets the exact high-latitude dipole-shell failure mode that motivated the
penumbra-safe UPPER_SCAN cutoff search.  The legacy endpoint BINARY search can
return the lower search boundary when an isolated allowed pocket exists near
Rmin.  The production UPPER_SCAN search must instead return the final upper
forbidden-to-allowed transition, close to the analytical vertical Størmer cutoff.

The default run launches two Mode3D cases:

    1. CUTOFF_SEARCH_ALGORITHM BINARY      required to reproduce the Rmin collapse
    2. CUTOFF_SEARCH_ALGORITHM UPPER_SCAN  required to recover the Størmer cutoff

The input also enables the Mode3D single-point rigidity debug scan for the
primary C11 point.  The scan file reports Rc_selected, Rc_endpoint_binary, and
Rc_upper_scan in one compact diagnostic table.
"""

from __future__ import annotations

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
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

TEST_ID = "C11"
TEST_NAME = "Penumbra-pocket regression: BINARY Rmin-collapse defect"

RE_KM = 6371.2
PROTON_REST_GEV = 0.9382720813
STORMER_R0_GV = 0.299792458 * 0.25 * 3.12e-5 * (RE_KM * 1000.0)

DEFAULT_CUTOFF_EMIN_MEV = 0.05
DEFAULT_CUTOFF_EMAX_MEV = 20000.0
DEFAULT_TARGET_LONS_DEG = (0.0,)
DEFAULT_TARGET_LATS_DEG = (-60.0, 60.0)
DEFAULT_TARGET_ALTS_KM = (500.0, 9000.0)
PRIMARY_LON_DEG = 0.0
PRIMARY_LAT_DEG = -60.0
PRIMARY_ALT_KM = 9000.0


class ShellRow:
    def __init__(self, alt_km: float, lon_deg: float, lat_deg: float,
                 rc_num_gv: float, rc_code_stormer_gv: Optional[float],
                 rel_err_code: Optional[float]):
        self.alt_km = alt_km
        self.lon_deg = lon_deg
        self.lat_deg = lat_deg
        self.rc_num_gv = rc_num_gv
        self.rc_code_stormer_gv = rc_code_stormer_gv
        self.rel_err_code = rel_err_code


class DebugScan:
    def __init__(self, rows: List[Dict[str, float]], metadata: Dict[str, float]):
        self.rows = rows
        self.metadata = metadata

    def first_value(self, key: str) -> Optional[float]:
        for row in self.rows:
            if key in row:
                return row[key]
        return None


def preprocess_negative_option_values(argv: Sequence[str]) -> List[str]:
    """Allow '--lats -60,60' as well as '--lats=-60,60'."""
    options = {"--lats", "--alts", "--lons", "--target-lats", "--target-alts", "--target-lons"}
    out: List[str] = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in options and i + 1 < len(argv):
            b = argv[i + 1]
            if b.startswith("-") and not b.startswith("--"):
                out.append(a + "=" + b)
                i += 2
                continue
        out.append(a)
        i += 1
    return out


def parse_float_list(text: str, name: str) -> List[float]:
    vals: List[float] = []
    for item in str(text).split(','):
        item = item.strip()
        if not item:
            continue
        try:
            vals.append(float(item))
        except ValueError:
            raise SystemExit("Could not parse %s value '%s' in '%s'" % (name, item, text))
    if not vals:
        raise SystemExit("%s list is empty" % name)
    return vals


def close_to(value: float, target: float, tol: float = 1.0e-6) -> bool:
    return abs(value - target) <= tol


def nearest_key(value: float, targets: Iterable[float], tol: float = 1.0e-6) -> Optional[float]:
    for t in targets:
        if abs(value - t) <= tol:
            return t
    return None


def stormer_vertical_gv(lat_deg: float, alt_km: float) -> float:
    """Independent centered aligned-dipole vertical Størmer cutoff in GV."""
    r_re = (RE_KM + alt_km) / RE_KM
    c4 = math.cos(math.radians(lat_deg)) ** 4
    return STORMER_R0_GV * c4 / (r_re * r_re)


def proton_rigidity_from_ekin_mev(ekin_mev: float) -> float:
    """Return proton rigidity in GV for kinetic energy per nucleon in MeV/n."""
    e_gev = ekin_mev * 1.0e-3
    return math.sqrt(e_gev * e_gev + 2.0 * e_gev * PROTON_REST_GEV)


def write_reference_csv(path: Path, lons: Sequence[float], lats: Sequence[float],
                        alts: Sequence[float], cutoff_emin_mev: float) -> None:
    rmin = proton_rigidity_from_ekin_mev(cutoff_emin_mev)
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow([
            "case_id",
            "lon_deg",
            "lat_deg",
            "alt_km",
            "Rc_stormer_GV",
            "CUTOFF_EMIN_MeV",
            "Rmin_from_CUTOFF_EMIN_GV",
            "expected_binary_signature",
            "expected_upper_scan_signature",
            "required_by_default",
            "purpose",
        ])
        for alt in alts:
            for lat in lats:
                for lon in lons:
                    primary = close_to(lon, PRIMARY_LON_DEG) and close_to(lat, PRIMARY_LAT_DEG) and close_to(alt, PRIMARY_ALT_KM)
                    case_id = ("primary_south_outer" if primary
                               else "lon%g_lat%+g_alt%g" % (lon, lat, alt))
                    w.writerow([
                        case_id,
                        "%.6g" % lon,
                        "%.6g" % lat,
                        "%.6g" % alt,
                        "%.12e" % stormer_vertical_gv(lat, alt),
                        "%.12e" % cutoff_emin_mev,
                        "%.12e" % rmin,
                        "endpoint_binary_returns_Rmin" if primary else "diagnostic_binary_Rmin_collapse_check",
                        "upper_scan_returns_Stormer_upper_cutoff",
                        "yes" if primary else "no",
                        "C11 penumbra-pocket regression target" if primary else "C11 repeat diagnostic target",
                    ])


def parse_variables(line: str) -> List[str]:
    return [n.strip().lower() for n in re.findall(r'"([^"]+)"', line)]


def normalize_name(name: str) -> str:
    return name.lower().replace(" ", "_").replace("-", "_").replace("/", "_")


def pick_column(variables: Optional[Sequence[str]], candidates: Sequence[str], fallback: int) -> int:
    if variables:
        norm = [normalize_name(v) for v in variables]
        for cand in candidates:
            c = normalize_name(cand)
            for i, v in enumerate(norm):
                if v == c or c in v:
                    return i
    return fallback


def parse_tecplot_shell_output(path: Path) -> List[ShellRow]:
    """Parse AMPS Mode3D shell cutoff Tecplot output."""
    rows: List[ShellRow] = []
    current_alt: Optional[float] = None
    variables: Optional[List[str]] = None
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
                variables = parse_variables(line)
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
                lon_col = pick_column(variables, ["lon_deg", "longitude", "lon"], 0)
                lat_col = pick_column(variables, ["lat_deg", "latitude", "lat"], 1)
                rc_col = pick_column(
                    variables,
                    ["rc_num_gv", "rc_gv", "cutoff_gv", "cutoff_rigidity_gv", "rigidity_gv"],
                    5 if len(parts) > 5 else 2,
                )
                rc_ref_col = pick_column(
                    variables,
                    ["rc_vert_gv", "rc_stormer_gv", "rc_analytic_gv", "stormer_gv"],
                    6 if len(parts) > 6 else -1,
                )
                rel_col = pick_column(variables, ["rel_err", "relative_error"], 7 if len(parts) > 7 else -1)
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


def parse_key_value_metadata(line: str) -> Dict[str, float]:
    out: Dict[str, float] = {}
    for key, value in re.findall(r'([A-Za-z0-9_]+)\s*=\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)', line):
        try:
            out[normalize_name(key)] = float(value)
        except ValueError:
            pass
    return out


def parse_debug_scan(path: Path) -> DebugScan:
    variables: Optional[List[str]] = None
    rows: List[Dict[str, float]] = []
    metadata: Dict[str, float] = {}
    with path.open("r", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            upper = line.upper()
            if upper.startswith("TITLE"):
                continue
            if upper.startswith("VARIABLES"):
                variables = [normalize_name(v) for v in parse_variables(line)]
                continue
            if line.startswith("#"):
                metadata.update(parse_key_value_metadata(line))
                continue
            if upper.startswith("ZONE"):
                metadata.update(parse_key_value_metadata(line))
                continue
            parts = line.split()
            if not parts:
                continue
            try:
                vals = [float(x) for x in parts]
            except ValueError:
                continue
            if variables:
                row = {variables[i]: vals[i] for i in range(min(len(variables), len(vals)))}
            else:
                names = [
                    "r_gv", "allowed", "expected_allowed_stormer", "r_over_rc_stormer",
                    "rc_selected_gv", "rc_endpoint_binary_gv", "rc_upper_scan_gv", "rc_stormer_gv",
                ]
                row = {names[i]: vals[i] for i in range(min(len(names), len(vals)))}
            rows.append(row)
    if not rows:
        raise RuntimeError("No data rows were parsed from debug scan %s" % path)
    return DebugScan(rows, metadata)


def write_csv_dicts(rows: Sequence[Dict[str, object]], path: Path) -> None:
    if not rows:
        return
    keys: List[str] = []
    for row in rows:
        for key in row.keys():
            if key not in keys:
                keys.append(key)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for row in rows:
            w.writerow(row)


def collapse_to_rmin(rc_gv: float, rmin_gv: float, collapse_factor: float, rmin_rel_tol: float) -> bool:
    if not (math.isfinite(rc_gv) and math.isfinite(rmin_gv) and rmin_gv > 0.0):
        return False
    near_rmin = abs(rc_gv - rmin_gv) <= rmin_rel_tol * max(rmin_gv, 1.0e-30)
    below_factor = rc_gv < collapse_factor * rmin_gv
    return bool(near_rmin or below_factor)


def transition_count(debug: DebugScan) -> int:
    pairs: List[Tuple[float, int]] = []
    for row in debug.rows:
        if "r_gv" in row and "allowed" in row:
            pairs.append((row["r_gv"], int(round(row["allowed"]))))
    pairs.sort()
    if not pairs:
        return 0
    n = 0
    last = pairs[0][1]
    for _, allowed in pairs[1:]:
        if allowed != last:
            n += 1
            last = allowed
    return n


def summarize_shell_algorithm(algorithm: str, rows: Sequence[ShellRow], lons: Sequence[float],
                              lats: Sequence[float], alts: Sequence[float], rmin_gv: float,
                              upper_rel_tol: float, collapse_factor: float,
                              rmin_rel_tol: float, require_binary_collapse_all: bool) -> Tuple[List[Dict[str, object]], bool, List[str]]:
    selected: List[Dict[str, object]] = []
    messages: List[str] = []
    passed = True
    found_primary = False
    found_targets = set()

    for row in rows:
        alt_key = nearest_key(row.alt_km, alts)
        lat_key = nearest_key(row.lat_deg, lats)
        lon_key = nearest_key(row.lon_deg % 360.0, [x % 360.0 for x in lons])
        if alt_key is None or lat_key is None or lon_key is None:
            continue
        ref = stormer_vertical_gv(lat_key, alt_key)
        rel_err = (row.rc_num_gv - ref) / ref if ref > 0.0 else 0.0
        abs_rel_err = abs(rel_err)
        collapsed = collapse_to_rmin(row.rc_num_gv, rmin_gv, collapse_factor, rmin_rel_tol)
        is_primary = close_to(lon_key, PRIMARY_LON_DEG) and close_to(lat_key, PRIMARY_LAT_DEG) and close_to(alt_key, PRIMARY_ALT_KM)
        if is_primary:
            found_primary = True
        found_targets.add((lon_key, lat_key, alt_key))
        required_binary = is_primary or require_binary_collapse_all
        status = "diagnostic"
        row_pass = True
        if algorithm == "UPPER_SCAN":
            status = "pass" if (not collapsed and abs_rel_err <= upper_rel_tol) else "fail"
            row_pass = status == "pass"
            if collapsed:
                messages.append("UPPER_SCAN collapsed near Rmin at lon=%g lat=%g alt=%g: Rc=%g GV, Rmin=%g GV" % (lon_key, lat_key, alt_key, row.rc_num_gv, rmin_gv))
            if abs_rel_err > upper_rel_tol:
                messages.append("UPPER_SCAN Størmer residual too large at lon=%g lat=%g alt=%g: abs_rel_error=%.3e > %.3e" % (lon_key, lat_key, alt_key, abs_rel_err, upper_rel_tol))
        elif algorithm == "BINARY" and required_binary:
            status = "pass" if collapsed else "fail"
            row_pass = collapsed
            if not collapsed:
                messages.append("BINARY did not reproduce Rmin collapse at required point lon=%g lat=%g alt=%g: Rc=%g GV, Rmin=%g GV" % (lon_key, lat_key, alt_key, row.rc_num_gv, rmin_gv))
        passed = passed and row_pass
        selected.append({
            "source": "shell",
            "algorithm": algorithm,
            "lon_deg": lon_key,
            "lat_deg": lat_key,
            "alt_km": alt_key,
            "Rc_num_GV": row.rc_num_gv,
            "Rc_stormer_GV": ref,
            "Rmin_GV": rmin_gv,
            "rel_error_vs_stormer": rel_err,
            "abs_rel_error_vs_stormer": abs_rel_err,
            "collapsed_to_low_boundary": bool(collapsed),
            "is_primary_point": bool(is_primary),
            "required_binary_collapse": bool(required_binary if algorithm == "BINARY" else False),
            "check_status": status,
        })

    for alt in alts:
        for lat in lats:
            for lon in [x % 360.0 for x in lons]:
                if (lon, lat, alt) not in found_targets:
                    passed = False
                    messages.append("Missing shell output target for %s lon=%g lat=%g alt=%g" % (algorithm, lon, lat, alt))
    if not found_primary:
        passed = False
        messages.append("Primary C11 point lon=%g lat=%g alt=%g was not found for %s" % (PRIMARY_LON_DEG, PRIMARY_LAT_DEG, PRIMARY_ALT_KM, algorithm))
    return selected, passed, messages


def summarize_debug_algorithm(algorithm: str, debug: DebugScan, rmin_gv: float,
                              upper_rel_tol: float, collapse_factor: float,
                              rmin_rel_tol: float) -> Tuple[List[Dict[str, object]], bool, List[str]]:
    messages: List[str] = []
    passed = True
    rc_selected = debug.first_value("rc_selected_gv")
    rc_endpoint = debug.first_value("rc_endpoint_binary_gv")
    rc_upper = debug.first_value("rc_upper_scan_gv")
    rc_stormer = debug.first_value("rc_stormer_gv")
    n_transition = transition_count(debug)

    if rc_stormer is None or not (rc_stormer > 0.0):
        rc_stormer = stormer_vertical_gv(PRIMARY_LAT_DEG, PRIMARY_ALT_KM)

    endpoint_collapsed = collapse_to_rmin(rc_endpoint if rc_endpoint is not None else float("nan"), rmin_gv, collapse_factor, rmin_rel_tol)
    upper_collapsed = collapse_to_rmin(rc_upper if rc_upper is not None else float("nan"), rmin_gv, collapse_factor, rmin_rel_tol)
    upper_rel = ((rc_upper - rc_stormer) / rc_stormer) if rc_upper is not None and rc_stormer > 0.0 else float("nan")
    selected_expected = rc_endpoint if algorithm == "BINARY" else rc_upper
    selected_rel = abs((rc_selected - selected_expected) / max(abs(selected_expected), 1.0e-30)) if rc_selected is not None and selected_expected is not None else float("nan")

    if not endpoint_collapsed:
        passed = False
        messages.append("Debug scan did not show endpoint-BINARY Rmin collapse: Rc_endpoint_binary=%s, Rmin=%g" % (str(rc_endpoint), rmin_gv))
    if upper_collapsed:
        passed = False
        messages.append("Debug scan UPPER_SCAN collapsed near Rmin: Rc_upper_scan=%s, Rmin=%g" % (str(rc_upper), rmin_gv))
    if not math.isfinite(upper_rel) or abs(upper_rel) > upper_rel_tol:
        passed = False
        messages.append("Debug scan UPPER_SCAN Størmer residual too large: rel_error=%s > %.3e" % (str(upper_rel), upper_rel_tol))
    if not math.isfinite(selected_rel) or selected_rel > max(1.0e-6, rmin_rel_tol):
        passed = False
        messages.append("Debug scan selected cutoff does not match requested algorithm %s: Rc_selected=%s, expected=%s" % (algorithm, str(rc_selected), str(selected_expected)))

    row = {
        "source": "debug_scan",
        "algorithm": algorithm,
        "lon_deg": debug.metadata.get("lon_deg", PRIMARY_LON_DEG),
        "lat_deg": debug.metadata.get("lat_deg", PRIMARY_LAT_DEG),
        "alt_km": debug.metadata.get("alt_km", PRIMARY_ALT_KM),
        "Rmin_GV": rmin_gv,
        "Rc_selected_GV": rc_selected,
        "Rc_endpoint_binary_GV": rc_endpoint,
        "Rc_upper_scan_GV": rc_upper,
        "Rc_stormer_GV": rc_stormer,
        "endpoint_binary_collapsed_to_Rmin": bool(endpoint_collapsed),
        "upper_scan_collapsed_to_Rmin": bool(upper_collapsed),
        "upper_scan_rel_error_vs_stormer": upper_rel,
        "allowed_state_transition_count": n_transition,
        "check_status": "pass" if passed else "fail",
    }
    return [row], passed, messages


def make_plot(summary_rows: Sequence[Dict[str, object]], path: Path) -> None:
    try:
        import matplotlib.pyplot as plt
    except Exception:
        return
    shell = [r for r in summary_rows if r.get("source") == "shell" and r.get("is_primary_point")]
    if not shell:
        return
    labels = []
    values = []
    refs = []
    for row in shell:
        labels.append(str(row["algorithm"]))
        values.append(float(row["Rc_num_GV"]))
        refs.append(float(row["Rc_stormer_GV"]))
    x = list(range(len(labels)))
    plt.figure(figsize=(7.0, 4.5))
    plt.bar(x, values, label="AMPS Rc")
    plt.plot(x, refs, marker="o", linestyle="--", label="Størmer reference")
    plt.xticks(x, labels)
    plt.ylabel("Vertical cutoff rigidity (GV)")
    plt.title("C11 primary point: BINARY collapse vs UPPER_SCAN")
    plt.grid(True, axis="y", alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(path, dpi=160)
    plt.close()


def run_command(cmd: Sequence[str], cwd: Path, log_path: Path) -> int:
    with log_path.open("w") as log:
        log.write("Command:\n  " + " ".join(cmd) + "\n\n")
        log.flush()
        proc = subprocess.Popen(cmd, cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            log.write(line)
        return proc.wait()


def replace_or_append_keyword(text: str, keyword: str, value: object, section_marker: Optional[str] = None) -> str:
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


def render_input_template(template_path: Path, output_path: Path, *, nt: int,
                          scheduler: str, dynamic_chunk: int,
                          cutoff_emin_mev: float, cutoff_emax_mev: float,
                          debug_scan_n: int, debug_scan_file: str,
                          debug_lon: float, debug_lat: float, debug_alt: float,
                          target_alts: Sequence[float], dt_trace: float,
                          max_trace_time: float, max_trace_distance: float) -> None:
    text = template_path.read_text()
    text = replace_or_append_keyword(text, "CUTOFF_EMIN", str(cutoff_emin_mev), "#CUTOFF_RIGIDITY")
    text = replace_or_append_keyword(text, "CUTOFF_EMAX", str(cutoff_emax_mev), "#CUTOFF_RIGIDITY")
    text = replace_or_append_keyword(text, "CUTOFF_DEBUG_SCAN_LON", str(debug_lon), "#CUTOFF_RIGIDITY")
    text = replace_or_append_keyword(text, "CUTOFF_DEBUG_SCAN_LAT", str(debug_lat), "#CUTOFF_RIGIDITY")
    text = replace_or_append_keyword(text, "CUTOFF_DEBUG_SCAN_ALT", str(debug_alt), "#CUTOFF_RIGIDITY")
    text = replace_or_append_keyword(text, "CUTOFF_DEBUG_SCAN_N", str(debug_scan_n), "#CUTOFF_RIGIDITY")
    text = replace_or_append_keyword(text, "CUTOFF_DEBUG_SCAN_FILE", debug_scan_file, "#CUTOFF_RIGIDITY")
    text = replace_or_append_keyword(text, "SHELL_COUNT", str(len(target_alts)), "#OUTPUT_DOMAIN")
    text = replace_or_append_keyword(text, "SHELL_ALTS_KM", " ".join("%.12g" % x for x in target_alts), "#OUTPUT_DOMAIN")
    text = replace_or_append_keyword(text, "DT_TRACE", str(dt_trace), "#NUMERICAL")
    text = replace_or_append_keyword(text, "MAX_TRACE_TIME", str(max_trace_time), "#NUMERICAL")
    text = replace_or_append_keyword(text, "MAX_TRACE_DISTANCE", str(max_trace_distance), "#NUMERICAL")
    text = replace_or_append_keyword(text, "CUTOFF_MAX_TRAJ_TIME", str(max_trace_time), "#CUTOFF_RIGIDITY")
    text += (
        "\n! ── C11 harness run-time settings, supplied through CLI ─────────\n"
        "! C11_NT                  %d\n"
        "! C11_MPI_SCHEDULER       %s\n"
        "! C11_MPI_DYNAMIC_CHUNK   %d\n"
        "! C11_DEBUG_SCAN_POINT    lon=%g lat=%g alt=%g\n"
    ) % (nt, scheduler, dynamic_chunk, debug_lon, debug_lat, debug_alt)
    output_path.write_text(text)


def find_output_file(workdir: Path, user_output: Optional[str]) -> Path:
    if user_output:
        p = Path(user_output)
        if not p.is_absolute():
            p = workdir / p
        return p
    candidates = ["cutoff_3d_shells_dipole_compare.dat", "cutoff_3d_shells.dat"]
    for name in candidates:
        p = workdir / name
        if p.exists():
            return p
    matches = sorted(workdir.glob("cutoff_*shell*.dat"))
    if matches:
        return matches[0]
    return workdir / candidates[0]


def parse_algorithms(value: str) -> List[str]:
    if value.upper() == "BOTH":
        return ["BINARY", "UPPER_SCAN"]
    out: List[str] = []
    for item in value.split(','):
        a = item.strip().upper()
        if not a:
            continue
        if a not in ("BINARY", "UPPER_SCAN"):
            raise SystemExit("Unknown algorithm %s" % a)
        out.append(a)
    if not out:
        raise SystemExit("No algorithms requested")
    return out


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        C11: Penumbra-pocket regression for the legacy BINARY Rmin-collapse defect.

        The primary point is lon=0 deg, lat=-60 deg, alt=9000 km in a centered
        aligned dipole.  With CUTOFF_EMIN=0.05 MeV/n, Rmin is about 9.6866e-3 GV.
        The BINARY diagnostic run is expected to return that lower bound, while
        UPPER_SCAN must return the final upper cutoff near the analytical Størmer
        value, about 0.160 GV at the primary point.

        Defaults: -np 4 and -nt 16.  AMPS is launched with mpirun.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/C11/run_C11.py
          python srcEarth/test/C11/run_C11.py -np 4 -nt 16
          python srcEarth/test/C11/run_C11.py --mode3d-field-eval ANALYTIC --algorithms BOTH
          python srcEarth/test/C11/run_C11.py --algorithms UPPER_SCAN --cutoff-scan-n 200
          python srcEarth/test/C11/run_C11.py --target-alts 500,9000 --target-lats -60,60
          python srcEarth/test/C11/run_C11.py --skip-run --workdir test_output/C11_mode3d
          python srcEarth/test/C11/run_C11.py --dry-run
        """)
    )
    parser.add_argument("-np", type=int, default=4, help="number of MPI processes passed to mpirun; default: 4")
    parser.add_argument("-nt", type=int, default=16, help="number of threads per MPI rank; default: 16")
    parser.add_argument("--mode3d-field-eval", "--field-eval", dest="mode3d_field_eval", default="ANALYTIC", choices=["ANALYTIC", "MESH", "GRID_3D"], help="Mode3D field backend passed as -mode3d-field-eval; default: ANALYTIC")
    parser.add_argument("--scheduler", default="DYNAMIC", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], help="MPI scheduler to use; default: DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0, help="dynamic MPI chunk size; 0 means AMPS auto heuristic; default: 0")
    parser.add_argument("--algorithms", default="BOTH", help="BOTH, UPPER_SCAN, BINARY, or comma-separated list; default: BOTH")
    parser.add_argument("--cutoff-scan-n", type=int, default=200, help="number of UPPER_SCAN rigidity samples passed to AMPS; default: 200")
    parser.add_argument("--debug-scan-n", type=int, default=200, help="number of log-spaced rigidity samples in the C11 debug scan; default: 200")
    parser.add_argument("--cutoff-emin", type=float, default=DEFAULT_CUTOFF_EMIN_MEV, help="CUTOFF_EMIN in MeV/n; default: 0.05")
    parser.add_argument("--cutoff-emax", type=float, default=DEFAULT_CUTOFF_EMAX_MEV, help="CUTOFF_EMAX in MeV/n; default: 20000")
    parser.add_argument("--target-lons", "--lons", dest="target_lons", default=",".join(str(x) for x in DEFAULT_TARGET_LONS_DEG), help="comma-separated longitudes to check; default: 0")
    parser.add_argument("--target-lats", "--lats", dest="target_lats", default=",".join(str(x) for x in DEFAULT_TARGET_LATS_DEG), help="comma-separated latitudes to check; default: -60,60")
    parser.add_argument("--target-alts", "--alts", dest="target_alts", default=",".join(str(x) for x in DEFAULT_TARGET_ALTS_KM), help="comma-separated shell altitudes in km; default: 500,9000")
    parser.add_argument("--debug-lon", type=float, default=PRIMARY_LON_DEG, help="debug-scan longitude in degrees; default: 0")
    parser.add_argument("--debug-lat", type=float, default=PRIMARY_LAT_DEG, help="debug-scan latitude in degrees; default: -60")
    parser.add_argument("--debug-alt", type=float, default=PRIMARY_ALT_KM, help="debug-scan altitude in km; default: 9000")
    parser.add_argument("--dt-trace", type=float, default=1.0, help="DT_TRACE written into the generated input; default: 1.0 s")
    parser.add_argument("--max-trace-time", type=float, default=600.0, help="MAX_TRACE_TIME and CUTOFF_MAX_TRAJ_TIME written into the input; default: 600 s")
    parser.add_argument("--max-trace-distance", type=float, default=300.0, help="MAX_TRACE_DISTANCE written into the input in Re; default: 300")
    parser.add_argument("--upper-rel-tol", type=float, default=2.5e-1, help="UPPER_SCAN absolute relative-error tolerance vs Størmer; default: 0.25")
    parser.add_argument("--collapse-factor", type=float, default=2.0, help="Rc < factor*Rmin is flagged as lower-boundary collapse; default: 2")
    parser.add_argument("--rmin-rel-tol", type=float, default=1.0e-3, help="relative tolerance for exact Rmin comparison; default: 1e-3")
    parser.add_argument("--require-binary-collapse-all", action="store_true", help="require BINARY Rmin collapse at every target point, not only the primary point")
    parser.add_argument("--no-debug-scan-check", action="store_true", help="do not require or analyze the C11 debug rigidity scan file")
    parser.add_argument("--no-cutoff-search-cli", action="store_true", help="do not pass -cutoff-search/-cutoff-upper-scan-n; useful for older CLI checkouts")
    parser.add_argument("--amps", default="./amps", help="AMPS executable path, relative to the launch directory; default: ./amps")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher executable; default: mpirun")
    parser.add_argument("--workdir", default=None, help="directory where the test is run and outputs are written; default: test_output/C11_mode3d")
    parser.add_argument("--output-file", default=None, help="explicit AMPS cutoff shell output file to parse; only useful with one algorithm or --skip-run")
    parser.add_argument("--skip-run", action="store_true", help="do not run AMPS; analyze existing algorithm subdirectories under --workdir")
    parser.add_argument("--keep", action="store_true", help="keep an existing work directory instead of replacing it")
    parser.add_argument("--dry-run", action="store_true", help="create rendered inputs and print AMPS commands without executing them")
    return parser.parse_args(preprocess_negative_option_values(list(argv) if argv is not None else sys.argv[1:]))


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.cutoff_scan_n < 2:
        raise SystemExit("--cutoff-scan-n must be >= 2")
    if args.debug_scan_n < 2:
        raise SystemExit("--debug-scan-n must be >= 2")
    if args.cutoff_emin <= 0.0 or args.cutoff_emax <= args.cutoff_emin:
        raise SystemExit("cutoff energy bounds must satisfy 0 < CUTOFF_EMIN < CUTOFF_EMAX")
    if args.max_trace_distance < 0.0:
        raise SystemExit("--max-trace-distance must be >= 0")

    algorithms = parse_algorithms(args.algorithms)
    target_lons = parse_float_list(args.target_lons, "target_lons")
    target_lats = parse_float_list(args.target_lats, "target_lats")
    target_alts = parse_float_list(args.target_alts, "target_alts")
    rmin_gv = proton_rigidity_from_ekin_mev(args.cutoff_emin)

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template_input = script_dir / "AMPS_PARAM_C11_mode3d.in"
    if not template_input.exists():
        raise SystemExit("Missing template input: %s" % template_input)

    root_workdir = Path(args.workdir) if args.workdir else Path("test_output") / "C11_mode3d"
    if not root_workdir.is_absolute():
        root_workdir = (launch_dir / root_workdir).resolve()

    if not args.skip_run and not args.keep and root_workdir.exists():
        shutil.rmtree(root_workdir)
    root_workdir.mkdir(parents=True, exist_ok=True)

    reference_csv = root_workdir / "reference_C11_penumbra_pocket_generated.csv"
    write_reference_csv(reference_csv, target_lons, target_lats, target_alts, args.cutoff_emin)

    all_summary: List[Dict[str, object]] = []
    messages: List[str] = []
    passed = True
    algo_outputs: Dict[str, str] = {}
    algo_debug_outputs: Dict[str, str] = {}
    planned_commands: List[Dict[str, object]] = []

    for algorithm in algorithms:
        alg_dir = root_workdir / algorithm.lower()
        debug_scan_name = "C11_%s_debug_rigidity_scan.dat" % algorithm.lower()
        if not args.skip_run:
            alg_dir.mkdir(parents=True, exist_ok=True)
            render_input_template(
                template_input,
                alg_dir / "AMPS_PARAM_C11.in",
                nt=args.nt,
                scheduler=args.scheduler,
                dynamic_chunk=args.dynamic_chunk,
                cutoff_emin_mev=args.cutoff_emin,
                cutoff_emax_mev=args.cutoff_emax,
                debug_scan_n=args.debug_scan_n,
                debug_scan_file=debug_scan_name,
                debug_lon=args.debug_lon,
                debug_lat=args.debug_lat,
                debug_alt=args.debug_alt,
                target_alts=target_alts,
                dt_trace=args.dt_trace,
                max_trace_time=args.max_trace_time,
                max_trace_distance=args.max_trace_distance,
            )
            shutil.copy2(reference_csv, alg_dir / "reference_C11_penumbra_pocket.csv")

            amps_path = Path(args.amps)
            if not amps_path.is_absolute():
                amps_path = (launch_dir / amps_path).resolve()
            cmd = [args.mpirun, "-np", str(args.np), str(amps_path), "-mode", "3d", "-i", "AMPS_PARAM_C11.in"]
            if not args.no_cutoff_search_cli:
                cmd += ["-cutoff-search", algorithm]
                if algorithm == "UPPER_SCAN":
                    cmd += ["-cutoff-upper-scan-n", str(args.cutoff_scan_n)]
            cmd += [
                "-mode3d-field-eval", args.mode3d_field_eval,
                "-mode3d-parallel", "THREADS",
                "-mode3d-threads", str(args.nt),
                "-mode3d-mpi-scheduler", args.scheduler,
                "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
            ]
            planned_commands.append({"algorithm": algorithm, "cwd": str(alg_dir), "command": cmd})
            if args.dry_run:
                print("Dry run %s/%s in %s" % (TEST_ID, algorithm, alg_dir))
                print(" ".join(cmd))
                continue
            print("Running %s/%s in %s" % (TEST_ID, algorithm, alg_dir))
            print(" ".join(cmd))
            rc = run_command(cmd, cwd=alg_dir, log_path=alg_dir / ("C11_%s_amps.log" % algorithm.lower()))
            if rc != 0:
                print("AMPS command failed with exit code %d for %s" % (rc, algorithm), file=sys.stderr)
                return rc
        else:
            if not alg_dir.exists() and len(algorithms) == 1:
                alg_dir = root_workdir
            if not alg_dir.exists():
                raise SystemExit("Missing algorithm directory for --skip-run: %s" % alg_dir)

        if args.dry_run:
            continue

        output_file = find_output_file(alg_dir, args.output_file)
        if not output_file.exists():
            print("Expected AMPS shell output was not found for %s: %s" % (algorithm, output_file), file=sys.stderr)
            return 2
        rows = parse_tecplot_shell_output(output_file)
        shell_summary, shell_passed, shell_messages = summarize_shell_algorithm(
            algorithm,
            rows,
            target_lons,
            target_lats,
            target_alts,
            rmin_gv,
            args.upper_rel_tol,
            args.collapse_factor,
            args.rmin_rel_tol,
            args.require_binary_collapse_all,
        )
        all_summary.extend(shell_summary)
        messages.extend(shell_messages)
        passed = passed and shell_passed
        algo_outputs[algorithm] = str(output_file)

        debug_path = alg_dir / debug_scan_name
        if not debug_path.exists():
            # Also accept the template's default name for hand-run diagnostics.
            fallback = alg_dir / "C11_debug_rigidity_scan.dat"
            if fallback.exists():
                debug_path = fallback
        if not args.no_debug_scan_check:
            if not debug_path.exists():
                passed = False
                messages.append("Expected C11 debug rigidity scan was not found for %s: %s" % (algorithm, debug_path))
            else:
                debug = parse_debug_scan(debug_path)
                debug_summary, debug_passed, debug_messages = summarize_debug_algorithm(
                    algorithm,
                    debug,
                    rmin_gv,
                    args.upper_rel_tol,
                    args.collapse_factor,
                    args.rmin_rel_tol,
                )
                all_summary.extend(debug_summary)
                messages.extend(debug_messages)
                passed = passed and debug_passed
                algo_debug_outputs[algorithm] = str(debug_path)

    if args.dry_run:
        planned_path = root_workdir / "C11_dry_run_commands.json"
        with planned_path.open("w") as f:
            json.dump(planned_commands, f, indent=2)
            f.write("\n")
        print("Dry run complete. Rendered inputs are under %s" % root_workdir)
        print("Planned-command JSON: %s" % planned_path)
        return 0

    summary_csv = root_workdir / "C11_summary.csv"
    result_json = root_workdir / "C11_result.json"
    write_csv_dicts(all_summary, summary_csv)
    make_plot(all_summary, root_workdir / "C11_binary_vs_upper_scan.png")

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": bool(passed),
        "messages": messages,
        "workdir": str(root_workdir),
        "algorithms": algorithms,
        "mode": "3d",
        "mode3d_field_eval": args.mode3d_field_eval,
        "cutoff_emin_MeV": args.cutoff_emin,
        "Rmin_GV": rmin_gv,
        "target_lons_deg": target_lons,
        "target_lats_deg": target_lats,
        "target_alts_km": target_alts,
        "require_binary_collapse_all": bool(args.require_binary_collapse_all),
        "output_files": algo_outputs,
        "debug_scan_files": algo_debug_outputs,
        "summary_csv": str(summary_csv),
        "reference_csv": str(reference_csv),
    }
    with result_json.open("w") as f:
        json.dump(result, f, indent=2)
        f.write("\n")

    print("\nC11 summary written to: %s" % summary_csv)
    print("C11 result written to:  %s" % result_json)
    if passed:
        print("C11 PASS: BINARY reproduced the Rmin-collapse signature and UPPER_SCAN recovered the upper cutoff.")
        return 0
    print("C11 FAIL:")
    for msg in messages:
        print("  - " + msg)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
