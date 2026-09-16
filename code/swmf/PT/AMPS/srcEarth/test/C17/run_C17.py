#!/usr/bin/env python3
"""
C17 — Dipole charge-sign and meridional-reflection symmetry.

Run from the directory containing the AMPS executable:

    python srcEarth/test/C17/run_C17.py -np 4 -nt 16

C17 is an internal symmetry test for the aligned centered dipole.  Reflection
through the x-z meridional plane, M(x,y,z)=(x,-y,z), leaves an observation point
at longitude zero fixed and maps the centered-dipole field as B(Mx)=M B(x).
Because M is an improper orthogonal transformation, the Lorentz equation obeys

    A_q(x0, v, R) = A_-q(x0, M v, R).

For the global spherical coordinates used by the AMPS directional output, this
is the sky mapping

    (lon,lat) -> ((-lon) mod 360, lat).

The default DIRECT_ACCESS profile compares three-state access on the common
rigidity grid.  Its acceptance gate uses resolved stable-interior nodes, while
transition-adjacent and unresolved nodes remain explicit diagnostics.  A legacy
UPPER_SCAN profile is retained for existing scalar directional-map output; its
cutoff comparison is intentionally distributional because one scalar upper
transition is sensitive to penumbra topology and numerical separatrices.
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
import tempfile
import textwrap
from pathlib import Path

TEST_ID = "C17"
TEST_NAME = "Dipole charge-sign and meridional-reflection symmetry"
RUNNER_SCHEMA_VERSION = 5
RUNNER_RELEASE = "2026-08-28"

RE_KM = 6371.2
DEFAULT_RIGIDITIES_GV = (0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0)

TRAJECTORY_TERMINATION_NAMES = {
    0: "OUTER_BOUNDARY_ALLOWED",
    1: "INNER_BOUNDARY_FORBIDDEN",
    2: "MAGNETICALLY_TRAPPED_FORBIDDEN",
    3: "TIME_LIMIT",
    4: "STEP_LIMIT",
    5: "DISTANCE_LIMIT",
    6: "INVALID_TIME_STEP",
    7: "INVALID_FIELD",
    8: "NUMERICAL_FAILURE",
    9: "DRIFT_TRAPPED_FORBIDDEN",
}


class DirCell(object):
    def __init__(self, lon_deg, lat_deg, rc_gv, emin_mev):
        self.lon_deg = lon_deg
        self.lat_deg = lat_deg
        self.rc_gv = rc_gv
        self.emin_mev = emin_mev


class AccessSample(object):
    def __init__(self, lon_deg, lat_deg, rigidity_gv, energy_mev, state,
                 termination_code=None, trace_time_s=None,
                 trace_distance_re=None, trace_steps=None, retry_count=None,
                 trace_extension_count=None, final_trace_limit_s=None):
        self.lon_deg = lon_deg
        self.lat_deg = lat_deg
        self.rigidity_gv = rigidity_gv
        self.energy_mev = energy_mev
        self.state = state
        self.termination_code = termination_code
        self.termination = TRAJECTORY_TERMINATION_NAMES.get(
            termination_code, "UNKNOWN")
        self.trace_time_s = trace_time_s
        self.trace_distance_re = trace_distance_re
        self.trace_steps = trace_steps
        self.retry_count = retry_count
        self.trace_extension_count = trace_extension_count
        self.final_trace_limit_s = final_trace_limit_s


def parse_float_list(text, what):
    vals = []
    for item in str(text).replace(";", ",").split(","):
        item = item.strip()
        if not item:
            continue
        try:
            vals.append(float(item))
        except ValueError:
            raise SystemExit("Invalid %s value: %r" % (what, item))
    if not vals:
        raise SystemExit("No values supplied for %s" % what)
    return vals


def parse_mover_list(text):
    allowed = {"BORIS", "RK2", "RK4", "RK6", "GC2", "GC4", "GC6"}
    movers = []
    for item in str(text).replace(";", ",").split(","):
        item = item.strip().upper()
        if not item:
            continue
        if item not in allowed:
            raise SystemExit("Unsupported mover %r; allowed values: %s" % (item, ",".join(sorted(allowed))))
        movers.append(item)
    if not movers:
        raise SystemExit("No movers requested")
    return movers


def bool_token(text):
    t = str(text).strip().upper()
    if t in ("T", "TRUE", "1", "YES", "Y"):
        return "T"
    if t in ("F", "FALSE", "0", "NO", "N"):
        return "F"
    raise SystemExit("Expected boolean token T/F, got %r" % text)


def norm_lon(lon):
    x = lon % 360.0
    if abs(x - 360.0) < 1.0e-8 or abs(x) < 1.0e-8:
        return 0.0
    return x


def norm_lat(lat):
    if abs(lat) < 1.0e-8:
        return 0.0
    return lat


def key_lon_lat(lon, lat):
    return (round(norm_lon(lon), 6), round(norm_lat(lat), 6))


def reflected_direction_key(lon, lat):
    """Reflect a global sky vector through the x-z meridional plane.

    C17 observation points are constrained to longitude zero, so the spatial
    reflection leaves every start point fixed.  In spherical sky coordinates
    y -> -y changes longitude sign and leaves latitude unchanged.
    """
    return key_lon_lat(-lon, lat)


def point_xyz_km(lon_deg, lat_deg, alt_km):
    r = RE_KM + alt_km
    lon = math.radians(lon_deg)
    lat = math.radians(lat_deg)
    c = math.cos(lat)
    return (r * c * math.cos(lon), r * c * math.sin(lon), r * math.sin(lat))


def build_points(obs_lons, obs_lats, alt_km):
    points = []
    point_id = 0
    for lon in obs_lons:
        for lat in obs_lats:
            x, y, z = point_xyz_km(lon, lat, alt_km)
            points.append({
                "point_id": point_id,
                "obs_lon_deg": lon,
                "obs_lat_deg": lat,
                "obs_alt_km": alt_km,
                "x_km": x,
                "y_km": y,
                "z_km": z,
            })
            point_id += 1
    return points


def points_block(points):
    lines = []
    for p in points:
        lines.append("POINT %.12e %.12e %.12e ! km  point_id=%d lon=%g lat=%g alt=%g" %
                     (p["x_km"], p["y_km"], p["z_km"], p["point_id"],
                      p["obs_lon_deg"], p["obs_lat_deg"], p["obs_alt_km"]))
    return "\n".join(lines)


def render_input_template(template_path, output_path, run_id, species_name, charge_e,
                          mass_amu, points, args):
    text = template_path.read_text()
    repl = {
        "__RUN_ID__": run_id,
        "__CUTOFF_EMIN_MEV__": "%.10g" % args.cutoff_emin,
        "__CUTOFF_EMAX_MEV__": "%.10g" % args.cutoff_emax,
        "__CUTOFF_MAX_PARTICLES__": str(args.cutoff_max_particles),
        "__CUTOFF_NENERGY__": str(args.cutoff_nenergy),
        "__CUTOFF_UPPER_SCAN_N__": str(args.cutoff_upper_scan_n),
        "__CUTOFF_SEARCH__": args.comparison_mode,
        "__CUTOFF_RIGIDITY_LIST_GV__": ",".join(
            "%.12g" % value
            for value in parse_float_list(args.rigidities, "--rigidities")),
        "__DIRMAP_LON_RES__": "%.10g" % args.dir_lon_res,
        "__DIRMAP_LAT_RES__": "%.10g" % args.dir_lat_res,
        "__SPECIES_NAME__": species_name,
        "__CHARGE__": str(charge_e),
        "__MASS_AMU__": "%.10g" % mass_amu,
        "__POINTS_BLOCK__": points_block(points),
        "__DT_TRACE__": "%.10g" % args.dt_trace,
        "__ADAPTIVE_DT__": bool_token(args.adaptive_dt),
        "__MAX_TRACE_TIME__": "%.10g" % args.max_trace_time,
        "__MAX_TRACE_DISTANCE_RE__": "%.10g" % args.max_trace_distance,
        "__GRIDLESS_MPI_SCHEDULER__": args.scheduler,
        "__GRIDLESS_MPI_DYNAMIC_CHUNK__": str(args.dynamic_chunk),
        "__GRIDLESS_THREADS__": str(args.nt),
    }
    for key, val in repl.items():
        text = text.replace(key, val)
    output_path.write_text(text)


def run_command(cmd, cwd, log_path):
    with log_path.open("w") as log:
        log.write("Command:\n  " + " ".join(cmd) + "\n\n")
        log.flush()
        proc = subprocess.Popen(cmd, cwd=str(cwd), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, universal_newlines=True)
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            log.write(line)
        return proc.wait()


def parse_variables(line):
    return [n.strip().lower() for n in re.findall(r'"([^"]+)"', line)]


def pick_col(variables, candidates, fallback):
    if variables:
        norm = [v.lower().replace(" ", "_").replace("-", "_") for v in variables]
        for cand in candidates:
            c = cand.lower().replace(" ", "_").replace("-", "_")
            for i, v in enumerate(norm):
                if v == c or c in v:
                    return i
    return fallback


def parse_directional_map(path):
    variables = None
    rows = []
    in_zone = False
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
                in_zone = True
                continue
            if not in_zone:
                continue
            parts = line.split()
            if len(parts) < 3:
                continue
            try:
                lon_col = pick_col(variables, ["lon_deg", "longitude", "lon"], 0)
                lat_col = pick_col(variables, ["lat_deg", "latitude", "lat"], 1)
                rc_col = pick_col(variables, ["Rc_GV", "cutoff_gv", "rigidity_gv"], 2)
                emin_col = pick_col(variables, ["Emin_MeV", "energy_mev"], 3 if len(parts) > 3 else -1)
                lon = float(parts[lon_col])
                lat = float(parts[lat_col])
                rc = float(parts[rc_col])
                emin = float(parts[emin_col]) if 0 <= emin_col < len(parts) else float("nan")
                rows.append(DirCell(lon, lat, rc, emin))
            except (ValueError, IndexError):
                continue
    if not rows:
        raise RuntimeError("No directional-map rows parsed from %s" % path)
    return rows


def normalize_variable_name(name):
    return name.strip().lower().replace(" ", "_").replace("-", "_")


def parse_directional_access(path):
    """Parse a dense AMPS three-state directional-access cube.

    C17 deliberately requests the non-adaptive common rigidity grid.  Requiring
    every direction to contain the same strictly increasing grid makes missing,
    duplicated, or truncated access samples a producer error rather than a
    potentially invisible symmetry pass.
    """
    variables = []
    samples = {}
    with path.open("r", errors="replace") as stream:
        for line_number, raw in enumerate(stream, start=1):
            line = raw.strip()
            if not line:
                continue
            upper = line.upper()
            if upper.startswith("VARIABLES"):
                variables = [normalize_variable_name(name)
                             for name in re.findall(r'"([^"]+)"', line)]
                continue
            if upper.startswith(("TITLE", "ZONE", "#", "!")):
                continue
            parts = line.replace(",", " ").split()
            if not variables or len(parts) < len(variables):
                continue
            record = dict(zip(variables, parts))
            try:
                lon = float(record["lon_deg"])
                lat = float(record["lat_deg"])
                rigidity = float(record["rigidity_gv"])
                energy = float(record["energy_mev"])
                state = int(float(record["access_state"]))
            except (KeyError, ValueError) as exc:
                raise RuntimeError(
                    "Malformed direct-access row %d in %s: %s" %
                    (line_number, path, exc))
            if state not in (0, 1, 2):
                raise RuntimeError(
                    "Invalid access_state=%d at line %d in %s" %
                    (state, line_number, path))
            if not (math.isfinite(rigidity) and rigidity > 0.0 and
                    math.isfinite(energy) and energy >= 0.0):
                raise RuntimeError(
                    "Invalid energy/rigidity at line %d in %s" %
                    (line_number, path))

            def optional_float(*names):
                for name in names:
                    value = record.get(name)
                    if value is None:
                        continue
                    try:
                        parsed = float(value)
                    except ValueError:
                        continue
                    return parsed if math.isfinite(parsed) else None
                return None

            def optional_int(*names):
                value = optional_float(*names)
                return None if value is None else int(round(value))

            key = key_lon_lat(lon, lat)
            samples.setdefault(key, []).append(
                AccessSample(
                    lon, lat, rigidity, energy, state,
                    termination_code=optional_int("termination_code"),
                    trace_time_s=optional_float("trace_time_s"),
                    trace_distance_re=optional_float("trace_distance_re"),
                    trace_steps=optional_int("trace_steps"),
                    retry_count=optional_int("retry_count"),
                    trace_extension_count=optional_int("trace_extension_count"),
                    final_trace_limit_s=optional_float("final_trace_limit_s"),
                ))

    if not samples:
        raise RuntimeError("No direct-access rows parsed from %s" % path)

    result = {}
    reference_grid = None
    for key, grid in samples.items():
        grid.sort(key=lambda sample: sample.rigidity_gv)
        for left, right in zip(grid[:-1], grid[1:]):
            if not right.rigidity_gv > left.rigidity_gv:
                raise RuntimeError(
                    "Duplicate/non-increasing rigidity at direction %s in %s" %
                    (key, path))
        current = [sample.rigidity_gv for sample in grid]
        if reference_grid is None:
            reference_grid = current
        elif len(current) != len(reference_grid) or any(
                not math.isclose(a, b, rel_tol=5.0e-11, abs_tol=1.0e-12)
                for a, b in zip(current, reference_grid)):
            raise RuntimeError(
                "DIRECT_ACCESS grid differs by direction at %s in %s; "
                "C17 requires the dense common grid" % (key, path))
        result[key] = grid
    return result


def map_to_dict(rows):
    d = {}
    for c in rows:
        key = key_lon_lat(c.lon_deg, c.lat_deg)
        if key in d:
            raise RuntimeError(
                "Duplicate directional-map cell at lon=%g lat=%g" %
                (c.lon_deg, c.lat_deg)
            )
        d[key] = c
    return d


def expected_direction_keys(dir_lon_res, dir_lat_res, ignore_poles=True):
    """Return the complete directional grid required from each AMPS output.

    C17 is an output-to-output symmetry test, so merely comparing whatever rows
    happen to be present could allow two identically truncated maps to pass.  The
    required grid is derived independently from the requested map resolution.
    """
    nlon = int(round(360.0 / dir_lon_res))
    nlat = int(round(180.0 / dir_lat_res)) + 1
    keys = set()
    for j in range(nlat):
        lat = -90.0 + dir_lat_res * j
        if ignore_poles and abs(abs(lat) - 90.0) < 1.0e-8:
            continue
        for i in range(nlon):
            keys.add(key_lon_lat(i * dir_lon_res, lat))
    return keys


def find_dir_map_file(workdir, point_id):
    candidates = [
        workdir / ("cutoff_gridless_dir_map_point_%04d.dat" % point_id),
        workdir / ("cutoff_3d_dir_map_point_%04d.dat" % point_id),
    ]
    for p in candidates:
        if p.exists():
            return p
    matches = sorted(workdir.glob("*dir*map*point*%04d*.dat" % point_id))
    if matches:
        return matches[0]
    return candidates[0]


def find_dir_access_file(workdir, point_id):
    candidates = [
        workdir / ("cutoff_gridless_dir_access_point_%04d.dat" % point_id),
        workdir / ("cutoff_3d_dir_access_loc_%06d.dat" % point_id),
    ]
    for path in candidates:
        if path.exists():
            return path
    matches = sorted(workdir.glob("*dir*access*point*%04d*.dat" % point_id))
    if not matches:
        matches = sorted(workdir.glob("*dir*access*loc*%06d*.dat" % point_id))
    return matches[0] if matches else candidates[0]


def finite_positive(x):
    return math.isfinite(x) and x > 0.0


def cutoff_state(x):
    """Classify the scalar UPPER_SCAN result without discarding its sentinel.

    A negative value is the documented legacy sentinel meaning that the scan
    maximum was still forbidden and no allowed upper branch was found.  It is a
    valid physical comparison state when both reflected calculations report it;
    zero and non-finite values have no documented scalar-cutoff meaning here.
    """
    if finite_positive(x):
        return "FINITE"
    if math.isfinite(x) and x < 0.0:
        return "NO_ALLOWED_UPPER_BRANCH"
    return "INVALID"


def step_transmission_from_cutoff(rigidity_gv, rc_gv):
    state = cutoff_state(rc_gv)
    if state == "FINITE":
        return 1 if rigidity_gv >= rc_gv else 0
    if state == "NO_ALLOWED_UPPER_BRANCH":
        return 0
    return None


def nonpolar_map(all_cells, ignore_poles):
    if not ignore_poles:
        return all_cells
    return {key: value for key, value in all_cells.items()
            if abs(abs(key[1]) - 90.0) >= 1.0e-8}


def compare_upper_scan_reflection(
        points, pos_dir, neg_dir, rigidities_gv, rc_rel_tol, abs_tol_gv,
        min_rc_pass_fraction, max_transmission_mismatch_fraction,
        dir_lon_res, dir_lat_res, ignore_poles=True):
    """Compare legacy scalar cutoff maps under charge/reflection symmetry.

    Scalar upper cutoffs are a lossy reduction of a possibly alternating
    penumbra.  C17 therefore makes complete coverage, matched sentinel states,
    and a bounded transmission-proxy mismatch hard requirements, while applying
    the Rc tolerance to a declared fraction of finite pairs.  DIRECT_ACCESS is
    the default and stronger comparison.
    """
    summary = []
    pair_rows = []
    messages = []
    overall_passed = True

    expected_keys = expected_direction_keys(
        dir_lon_res, dir_lat_res, ignore_poles=ignore_poles)
    if not expected_keys:
        raise RuntimeError("The requested directional grid has no comparison cells")

    for p in points:
        point_id = p["point_id"]
        pos_file = find_dir_map_file(pos_dir, point_id)
        neg_file = find_dir_map_file(neg_dir, point_id)
        if not pos_file.exists() or not neg_file.exists():
            overall_passed = False
            messages.append("Missing directional-map file for point %d: %s or %s" % (point_id, pos_file, neg_file))
            continue

        pos_rows = parse_directional_map(pos_file)
        neg_rows = parse_directional_map(neg_file)
        pos_map_all = map_to_dict(pos_rows)
        neg_map_all = map_to_dict(neg_rows)

        pos_map = nonpolar_map(pos_map_all, ignore_poles)
        neg_map = nonpolar_map(neg_map_all, ignore_poles)
        pos_keys = set(pos_map)
        neg_keys = set(neg_map)
        missing_pos = expected_keys - pos_keys
        missing_neg = expected_keys - neg_keys
        unexpected_pos = pos_keys - expected_keys
        unexpected_neg = neg_keys - expected_keys

        n_pairs = 0
        n_missing_reflection = 0
        n_invalid_pairs = 0
        n_cutoff_state_mismatches = 0
        n_matching_negative_sentinels = 0
        n_finite_pairs = 0
        n_rc_tolerance_exceedances = 0
        n_t_mismatch = 0
        n_t_comparisons = 0
        max_abs_diff = 0.0
        max_rel_diff = 0.0
        worst = None

        for key in sorted(expected_keys):
            c = pos_map.get(key)
            if c is None:
                pair_rows.append({
                    "point_id": point_id,
                    "obs_lon_deg": p["obs_lon_deg"],
                    "obs_lat_deg": p["obs_lat_deg"],
                    "dir_lon_deg": key[0],
                    "dir_lat_deg": key[1],
                    "reflected_dir_lon_deg": reflected_direction_key(*key)[0],
                    "reflected_dir_lat_deg": reflected_direction_key(*key)[1],
                    "Rc_plus_GV": "",
                    "Rc_minus_reflected_GV": "",
                    "plus_cutoff_state": "MISSING",
                    "minus_cutoff_state": "",
                    "abs_diff_GV": "",
                    "rel_diff": "",
                    "within_rc_tolerance": False,
                    "transmission_mismatches": "",
                    "note": "missing direction in positive-charge map",
                })
                continue

            reflected_key = reflected_direction_key(c.lon_deg, c.lat_deg)
            reflected = neg_map.get(reflected_key)
            if reflected is None:
                n_missing_reflection += 1
                pair_rows.append({
                    "point_id": point_id,
                    "obs_lon_deg": p["obs_lon_deg"],
                    "obs_lat_deg": p["obs_lat_deg"],
                    "dir_lon_deg": c.lon_deg,
                    "dir_lat_deg": c.lat_deg,
                    "reflected_dir_lon_deg": reflected_key[0],
                    "reflected_dir_lat_deg": reflected_key[1],
                    "Rc_plus_GV": c.rc_gv,
                    "Rc_minus_reflected_GV": "",
                    "plus_cutoff_state": cutoff_state(c.rc_gv),
                    "minus_cutoff_state": "MISSING",
                    "abs_diff_GV": "",
                    "rel_diff": "",
                    "within_rc_tolerance": False,
                    "transmission_mismatches": "",
                    "note": "missing reflected direction in negative-charge map",
                })
                continue

            n_pairs += 1
            plus_state = cutoff_state(c.rc_gv)
            minus_state = cutoff_state(reflected.rc_gv)
            within_rc_tolerance = False
            if "INVALID" in (plus_state, minus_state):
                n_invalid_pairs += 1
                abs_diff = float("nan")
                rel_diff = float("nan")
            elif plus_state != minus_state:
                n_cutoff_state_mismatches += 1
                abs_diff = float("nan")
                rel_diff = float("nan")
            elif plus_state == "NO_ALLOWED_UPPER_BRANCH":
                n_matching_negative_sentinels += 1
                within_rc_tolerance = True
                abs_diff = 0.0
                rel_diff = 0.0
            else:
                n_finite_pairs += 1
                abs_diff = abs(c.rc_gv - reflected.rc_gv)
                denom = max(abs(c.rc_gv), abs(reflected.rc_gv), 1.0e-30)
                rel_diff = abs_diff / denom
                within_rc_tolerance = (
                    abs_diff <= abs_tol_gv or rel_diff <= rc_rel_tol)
                if not within_rc_tolerance:
                    n_rc_tolerance_exceedances += 1
                if abs_diff > max_abs_diff:
                    max_abs_diff = abs_diff
                if rel_diff > max_rel_diff:
                    max_rel_diff = rel_diff
                if worst is None or rel_diff > worst["rel_diff"]:
                    worst = {
                        "dir_lon_deg": c.lon_deg,
                        "dir_lat_deg": c.lat_deg,
                        "reflected_dir_lon_deg": reflected.lon_deg,
                        "reflected_dir_lat_deg": reflected.lat_deg,
                        "Rc_plus_GV": c.rc_gv,
                        "Rc_minus_reflected_GV": reflected.rc_gv,
                        "abs_diff_GV": abs_diff,
                        "rel_diff": rel_diff,
                    }

            tm = 0
            for R in rigidities_gv:
                tp = step_transmission_from_cutoff(R, c.rc_gv)
                tn = step_transmission_from_cutoff(R, reflected.rc_gv)
                if tp is not None and tn is not None:
                    n_t_comparisons += 1
                if tp is None or tn is None or tp != tn:
                    tm += 1
            n_t_mismatch += tm

            pair_rows.append({
                "point_id": point_id,
                "obs_lon_deg": p["obs_lon_deg"],
                "obs_lat_deg": p["obs_lat_deg"],
                "dir_lon_deg": c.lon_deg,
                "dir_lat_deg": c.lat_deg,
                "reflected_dir_lon_deg": reflected.lon_deg,
                "reflected_dir_lat_deg": reflected.lat_deg,
                "Rc_plus_GV": c.rc_gv,
                "Rc_minus_reflected_GV": reflected.rc_gv,
                "plus_cutoff_state": plus_state,
                "minus_cutoff_state": minus_state,
                "abs_diff_GV": abs_diff,
                "rel_diff": rel_diff,
                "within_rc_tolerance": within_rc_tolerance,
                "transmission_mismatches": tm,
                "note": "",
            })

        rc_pass_fraction = (
            float(n_finite_pairs - n_rc_tolerance_exceedances) / n_finite_pairs
            if n_finite_pairs else 0.0)
        transmission_mismatch_fraction = (
            float(n_t_mismatch) / n_t_comparisons
            if n_t_comparisons else 1.0)
        point_passed = (
            len(pos_keys) == len(expected_keys)
            and len(neg_keys) == len(expected_keys)
            and not missing_pos and not missing_neg
            and not unexpected_pos and not unexpected_neg
            and n_pairs == len(expected_keys)
            and n_missing_reflection == 0 and n_invalid_pairs == 0
            and n_cutoff_state_mismatches == 0
            and n_finite_pairs > 0
            and rc_pass_fraction >= min_rc_pass_fraction
            and transmission_mismatch_fraction <= max_transmission_mismatch_fraction
        )
        if not point_passed:
            overall_passed = False
            messages.append(
                "point %d lon=%g lat=%g: expected=%d pos=%d neg=%d "
                "missing_pos=%d missing_neg=%d extra_pos=%d extra_neg=%d "
                "missing_reflection=%d invalid=%d state_mismatch=%d "
                "sentinel_matches=%d finite=%d Rc_exceed=%d Rc_pass_fraction=%.3f "
                "T_mismatch=%d/%d (%.3f) max_rel=%.3e" %
                (point_id, p["obs_lon_deg"], p["obs_lat_deg"], len(expected_keys),
                 len(pos_keys), len(neg_keys), len(missing_pos), len(missing_neg),
                 len(unexpected_pos), len(unexpected_neg), n_missing_reflection,
                 n_invalid_pairs, n_cutoff_state_mismatches,
                 n_matching_negative_sentinels, n_finite_pairs,
                 n_rc_tolerance_exceedances, rc_pass_fraction,
                 n_t_mismatch, n_t_comparisons,
                 transmission_mismatch_fraction, max_rel_diff)
            )

        item = {
            "point_id": point_id,
            "obs_lon_deg": p["obs_lon_deg"],
            "obs_lat_deg": p["obs_lat_deg"],
            "obs_alt_km": p["obs_alt_km"],
            "n_expected_cells": len(expected_keys),
            "n_positive_cells": len(pos_keys),
            "n_negative_cells": len(neg_keys),
            "n_pairs_checked": n_pairs,
            "n_missing_positive": len(missing_pos),
            "n_missing_negative": len(missing_neg),
            "n_unexpected_positive": len(unexpected_pos),
            "n_unexpected_negative": len(unexpected_neg),
            "n_missing_reflection": n_missing_reflection,
            "n_invalid_pairs": n_invalid_pairs,
            "n_cutoff_state_mismatches": n_cutoff_state_mismatches,
            "n_matching_negative_sentinels": n_matching_negative_sentinels,
            "n_finite_pairs": n_finite_pairs,
            "n_rc_tolerance_exceedances": n_rc_tolerance_exceedances,
            "rc_pass_fraction": rc_pass_fraction,
            "n_transmission_mismatches": n_t_mismatch,
            "n_transmission_comparisons": n_t_comparisons,
            "transmission_mismatch_fraction": transmission_mismatch_fraction,
            "max_abs_diff_GV": max_abs_diff,
            "max_rel_diff": max_rel_diff,
            "rc_rel_tol": rc_rel_tol,
            "abs_tol_GV": abs_tol_gv,
            "min_rc_pass_fraction": min_rc_pass_fraction,
            "max_transmission_mismatch_fraction": max_transmission_mismatch_fraction,
            "passed": point_passed,
        }
        if worst:
            item.update({"worst_" + k: v for k, v in worst.items()})
        summary.append(item)

    return summary, pair_rows, overall_passed, messages


def compare_direct_access_reflection(
        points, pos_dir, neg_dir, max_access_mismatch_fraction,
        max_one_sided_unresolved_fraction, min_stable_sample_fraction,
        min_stable_state_count, dir_lon_res, dir_lat_res, ignore_poles=True):
    """Compare A(R,Omega) away from unresolved and penumbral boundaries.

    A binary access state is discontinuous at a cutoff separatrix.  Tiny,
    symmetry-preserving integration errors can move two reflected trajectories
    to opposite sides of that boundary without indicating a charge-sign defect.
    C17 therefore gates state equality on stable-interior nodes: the current
    node and every available immediate rigidity neighbor must be resolved, and
    each charge branch must retain its current state at those neighbors.

    One-sided UNRESOLVED outcomes remain a hard symmetry failure.  Matching
    two-sided UNRESOLVED outcomes are reported as completion diagnostics, while
    minimum stable coverage and matching ALLOWED/FORBIDDEN anchor counts prevent
    an unresolved calculation from passing without meaningful physics coverage.
    """
    summary = []
    pair_rows = []
    messages = []
    overall_passed = True
    expected_keys = expected_direction_keys(
        dir_lon_res, dir_lat_res, ignore_poles=ignore_poles)

    for point in points:
        point_id = point["point_id"]
        pos_file = find_dir_access_file(pos_dir, point_id)
        neg_file = find_dir_access_file(neg_dir, point_id)
        if not pos_file.exists() or not neg_file.exists():
            overall_passed = False
            messages.append(
                "Missing direct-access file for point %d: %s or %s" %
                (point_id, pos_file, neg_file))
            continue

        pos_map = nonpolar_map(parse_directional_access(pos_file), ignore_poles)
        neg_map = nonpolar_map(parse_directional_access(neg_file), ignore_poles)
        pos_keys = set(pos_map)
        neg_keys = set(neg_map)
        missing_pos = expected_keys - pos_keys
        missing_neg = expected_keys - neg_keys
        unexpected_pos = pos_keys - expected_keys
        unexpected_neg = neg_keys - expected_keys
        n_missing_reflection = 0
        n_grid_mismatches = 0
        n_samples = 0
        n_state_mismatches = 0
        n_unresolved = 0
        n_one_sided_unresolved = 0
        n_both_unresolved = 0
        n_resolved_pairs = 0
        n_resolved_state_mismatches = 0
        n_stable_pairs = 0
        n_stable_state_mismatches = 0
        n_stable_matching_allowed = 0
        n_stable_matching_forbidden = 0

        for key in sorted(expected_keys):
            pos_grid = pos_map.get(key)
            reflected_key = reflected_direction_key(*key)
            neg_grid = neg_map.get(reflected_key)
            if pos_grid is None:
                continue
            if neg_grid is None:
                n_missing_reflection += 1
                continue
            if len(pos_grid) != len(neg_grid):
                n_grid_mismatches += 1
                continue
            for sample_index, (plus, minus) in enumerate(zip(pos_grid, neg_grid)):
                same_rigidity = math.isclose(
                    plus.rigidity_gv, minus.rigidity_gv,
                    rel_tol=5.0e-11, abs_tol=1.0e-12)
                if not same_rigidity:
                    n_grid_mismatches += 1
                    continue
                n_samples += 1
                mismatch = plus.state != minus.state
                unresolved = plus.state == 2 or minus.state == 2
                both_unresolved = plus.state == 2 and minus.state == 2
                one_sided_unresolved = ((plus.state == 2) != (minus.state == 2))
                both_resolved = not unresolved
                stable_interior = both_resolved
                if stable_interior:
                    for neighbor_index in (sample_index - 1, sample_index + 1):
                        if not 0 <= neighbor_index < len(pos_grid):
                            continue
                        plus_neighbor = pos_grid[neighbor_index]
                        minus_neighbor = neg_grid[neighbor_index]
                        if (plus_neighbor.state == 2 or minus_neighbor.state == 2
                                or plus_neighbor.state != plus.state
                                or minus_neighbor.state != minus.state):
                            stable_interior = False
                            break
                n_state_mismatches += int(mismatch)
                n_unresolved += int(unresolved)
                n_one_sided_unresolved += int(one_sided_unresolved)
                n_both_unresolved += int(both_unresolved)
                n_resolved_pairs += int(both_resolved)
                n_resolved_state_mismatches += int(both_resolved and mismatch)
                n_stable_pairs += int(stable_interior)
                n_stable_state_mismatches += int(stable_interior and mismatch)
                n_stable_matching_allowed += int(
                    stable_interior and not mismatch and plus.state == 1)
                n_stable_matching_forbidden += int(
                    stable_interior and not mismatch and plus.state == 0)
                pair_rows.append({
                    "point_id": point_id,
                    "obs_lon_deg": point["obs_lon_deg"],
                    "obs_lat_deg": point["obs_lat_deg"],
                    "dir_lon_deg": key[0],
                    "dir_lat_deg": key[1],
                    "reflected_dir_lon_deg": reflected_key[0],
                    "reflected_dir_lat_deg": reflected_key[1],
                    "rigidity_GV": plus.rigidity_gv,
                    "energy_plus_MeV": plus.energy_mev,
                    "energy_minus_MeV": minus.energy_mev,
                    "access_state_plus": plus.state,
                    "access_state_minus_reflected": minus.state,
                    "state_match": not mismatch,
                    "both_resolved": both_resolved,
                    "resolved_state_match": ((not mismatch) if both_resolved else ""),
                    "stable_interior": stable_interior,
                    "stable_state_match": ((not mismatch) if stable_interior else ""),
                    "either_unresolved": unresolved,
                    "one_sided_unresolved": one_sided_unresolved,
                    "both_unresolved": both_unresolved,
                    "plus_termination_code": plus.termination_code,
                    "plus_termination": plus.termination,
                    "minus_termination_code": minus.termination_code,
                    "minus_termination": minus.termination,
                    "plus_trace_time_s": plus.trace_time_s,
                    "minus_trace_time_s": minus.trace_time_s,
                    "plus_trace_distance_Re": plus.trace_distance_re,
                    "minus_trace_distance_Re": minus.trace_distance_re,
                    "plus_trace_steps": plus.trace_steps,
                    "minus_trace_steps": minus.trace_steps,
                    "plus_retry_count": plus.retry_count,
                    "minus_retry_count": minus.retry_count,
                    "plus_trace_extension_count": plus.trace_extension_count,
                    "minus_trace_extension_count": minus.trace_extension_count,
                    "plus_final_trace_limit_s": plus.final_trace_limit_s,
                    "minus_final_trace_limit_s": minus.final_trace_limit_s,
                })

        mismatch_fraction = (
            float(n_state_mismatches) / n_samples if n_samples else 1.0)
        resolved_mismatch_fraction = (
            float(n_resolved_state_mismatches) / n_resolved_pairs
            if n_resolved_pairs else 1.0)
        unresolved_fraction = (
            float(n_unresolved) / n_samples if n_samples else 1.0)
        one_sided_unresolved_fraction = (
            float(n_one_sided_unresolved) / n_samples if n_samples else 1.0)
        stable_mismatch_fraction = (
            float(n_stable_state_mismatches) / n_stable_pairs
            if n_stable_pairs else 1.0)
        stable_sample_fraction = (
            float(n_stable_pairs) / n_samples if n_samples else 0.0)
        point_passed = (
            pos_keys == expected_keys and neg_keys == expected_keys
            and not missing_pos and not missing_neg
            and not unexpected_pos and not unexpected_neg
            and n_missing_reflection == 0 and n_grid_mismatches == 0
            and n_samples > 0 and n_stable_pairs > 0
            and stable_mismatch_fraction <= max_access_mismatch_fraction
            and stable_sample_fraction >= min_stable_sample_fraction
            and n_stable_matching_allowed >= min_stable_state_count
            and n_stable_matching_forbidden >= min_stable_state_count
            and one_sided_unresolved_fraction
                <= max_one_sided_unresolved_fraction
        )
        if not point_passed:
            overall_passed = False
            messages.append(
                "point %d lon=%g lat=%g: expected=%d pos=%d neg=%d "
                "missing_pos=%d missing_neg=%d extra_pos=%d extra_neg=%d "
                "missing_reflection=%d grid_mismatch=%d samples=%d "
                "stable_mismatch=%d/%d (%.3e) stable_coverage=%.3e "
                "stable_allowed=%d stable_forbidden=%d "
                "resolved_mismatch=%d/%d (%.3e; diagnostic) "
                "one_sided_unresolved=%d (%.3e) both_unresolved=%d "
                "any_unresolved=%d (%.3e); all_state_mismatch=%d (%.3e)" %
                (point_id, point["obs_lon_deg"], point["obs_lat_deg"],
                 len(expected_keys), len(pos_keys), len(neg_keys),
                 len(missing_pos), len(missing_neg), len(unexpected_pos),
                 len(unexpected_neg), n_missing_reflection, n_grid_mismatches,
                 n_samples, n_stable_state_mismatches, n_stable_pairs,
                 stable_mismatch_fraction, stable_sample_fraction,
                 n_stable_matching_allowed, n_stable_matching_forbidden,
                 n_resolved_state_mismatches, n_resolved_pairs,
                 resolved_mismatch_fraction, n_one_sided_unresolved,
                 one_sided_unresolved_fraction, n_both_unresolved,
                 n_unresolved, unresolved_fraction,
                 n_state_mismatches, mismatch_fraction))
        summary.append({
            "point_id": point_id,
            "obs_lon_deg": point["obs_lon_deg"],
            "obs_lat_deg": point["obs_lat_deg"],
            "obs_alt_km": point["obs_alt_km"],
            "n_expected_cells": len(expected_keys),
            "n_positive_cells": len(pos_keys),
            "n_negative_cells": len(neg_keys),
            "n_missing_positive": len(missing_pos),
            "n_missing_negative": len(missing_neg),
            "n_unexpected_positive": len(unexpected_pos),
            "n_unexpected_negative": len(unexpected_neg),
            "n_missing_reflection": n_missing_reflection,
            "n_grid_mismatches": n_grid_mismatches,
            "n_access_samples": n_samples,
            "n_access_state_mismatches": n_state_mismatches,
            "access_mismatch_fraction": mismatch_fraction,
            "n_resolved_pairs": n_resolved_pairs,
            "n_resolved_state_mismatches": n_resolved_state_mismatches,
            "resolved_mismatch_fraction": resolved_mismatch_fraction,
            "n_stable_pairs": n_stable_pairs,
            "n_stable_state_mismatches": n_stable_state_mismatches,
            "stable_mismatch_fraction": stable_mismatch_fraction,
            "stable_sample_fraction": stable_sample_fraction,
            "n_stable_matching_allowed": n_stable_matching_allowed,
            "n_stable_matching_forbidden": n_stable_matching_forbidden,
            "n_one_sided_unresolved": n_one_sided_unresolved,
            "one_sided_unresolved_fraction": one_sided_unresolved_fraction,
            "n_both_unresolved": n_both_unresolved,
            "n_unresolved_samples": n_unresolved,
            "unresolved_fraction": unresolved_fraction,
            "max_access_mismatch_fraction": max_access_mismatch_fraction,
            "max_one_sided_unresolved_fraction":
                max_one_sided_unresolved_fraction,
            "min_stable_sample_fraction": min_stable_sample_fraction,
            "min_stable_state_count": min_stable_state_count,
            "passed": point_passed,
        })

    return summary, pair_rows, overall_passed, messages


def write_dict_csv(rows, path):
    if not rows:
        path.write_text("")
        return
    keys = []
    for r in rows:
        for k in r.keys():
            if k not in keys:
                keys.append(k)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for r in rows:
            w.writerow(r)


def write_reference_csv(path, points, args, rigidities):
    dir_lons = []
    nlon = int(round(360.0 / args.dir_lon_res))
    for i in range(nlon):
        dir_lons.append(i * args.dir_lon_res)
    nlat = int(math.floor(180.0 / args.dir_lat_res)) + 1
    dir_lats = []
    for j in range(nlat):
        lat = -90.0 + args.dir_lat_res * j
        if lat > 90.0:
            lat = 90.0
        dir_lats.append(lat)

    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow([
            "runner_schema_version", "point_id", "obs_lon_deg", "obs_lat_deg", "obs_alt_km",
            "dir_lon_deg", "dir_lat_deg", "reflected_dir_lon_deg",
            "reflected_dir_lat_deg", "expected", "default_comparison_mode",
            "max_stable_access_mismatch_fraction",
            "max_one_sided_unresolved_fraction",
            "min_stable_sample_fraction", "min_stable_state_count",
            "trace_profile", "dt_trace_s", "trace_time_s",
            "max_trace_distance_Re",
            "legacy_rc_rel_tol", "legacy_abs_tol_GV",
            "legacy_min_rc_pass_fraction",
            "legacy_max_transmission_mismatch_fraction",
            "legacy_rigidity_list_GV", "note",
        ])
        for p in points:
            for lat in dir_lats:
                for lon in dir_lons:
                    if args.ignore_poles and abs(abs(lat) - 90.0) < 1.0e-8:
                        continue
                    reflected = reflected_direction_key(lon, lat)
                    w.writerow([
                        RUNNER_SCHEMA_VERSION, p["point_id"], p["obs_lon_deg"],
                        p["obs_lat_deg"], p["obs_alt_km"],
                        lon, lat, reflected[0], reflected[1],
                        "A_plus(dir,R) == A_minus(reflected_dir,R)",
                        "DIRECT_ACCESS",
                        "%.6g" % args.max_access_mismatch_fraction,
                        "%.6g" % args.max_one_sided_unresolved_fraction,
                        "%.6g" % args.min_stable_sample_fraction,
                        str(args.min_stable_state_count),
                        "PARSER_COMPATIBLE",
                        "%.6g" % args.dt_trace,
                        "%.6g" % args.max_trace_time,
                        "%.6g" % args.max_trace_distance,
                        "%.6g" % args.rc_rel_tol,
                        "%.6g" % args.abs_tol_gv,
                        "%.6g" % args.min_rc_pass_fraction,
                        "%.6g" % args.max_transmission_mismatch_fraction,
                        ";".join("%.10g" % r for r in rigidities),
                        "q reversal plus y->-y reflection in aligned dipole, E=0",
                    ])


def make_plot(pair_rows, out_path):
    try:
        import matplotlib.pyplot as plt
    except Exception:
        return
    vals = []
    for r in pair_rows:
        try:
            value = float(r["rel_diff"])
            if math.isfinite(value):
                vals.append(value)
        except Exception:
            pass
    if not vals:
        return
    plt.figure(figsize=(7.0, 4.5))
    plt.hist(vals, bins=50)
    plt.yscale("log")
    plt.xlabel("relative difference |Rc+ - Rc-(M v)| / max(Rc)")
    plt.ylabel("number of directional cells")
    plt.title("C17 charge/meridional-reflection cutoff residuals")
    plt.tight_layout()
    plt.savefig(out_path, dpi=160)
    plt.close()


def self_test():
    """Exercise reflection, sentinels, direct access, and coverage guards."""
    template_path = Path(__file__).resolve().parent / "AMPS_PARAM_C17_gridless.in"
    template_text = template_path.read_text(errors="replace")
    # Parser contract regression: DIRECT_ACCESS, PENUMBRA_SCAN, and an explicit
    # CUTOFF_RIGIDITY_LIST_GV all require CUTOFF_SAMPLING VERTICAL.  The sky
    # remains direction resolved through DIRECTIONAL_MAP/DIRMAP_*; VERTICAL here
    # is the common parser-compatible scalar sampling token.
    if not re.search(r"^CUTOFF_SAMPLING\s+VERTICAL\s*$",
                     template_text, re.MULTILINE):
        print("C17 self-test failed: input template must set "
              "CUTOFF_SAMPLING VERTICAL for DIRECT_ACCESS/RIGIDITY_LIST",
              file=sys.stderr)
        return 1
    if not re.search(r"^CUTOFF_RIGIDITY_LIST_GV\s+__CUTOFF_RIGIDITY_LIST_GV__\s*$",
                     template_text, re.MULTILINE):
        print("C17 self-test failed: explicit rigidity-list template is missing",
              file=sys.stderr)
        return 1
    required_trace_contract = {
        "trace-time control":
            r"^MAX_TRACE_TIME\s+__MAX_TRACE_TIME__\s*$",
        "cutoff trace-time control":
            r"^CUTOFF_MAX_TRAJ_TIME\s+__MAX_TRACE_TIME__\s*$",
        "path-cap control":
            r"^MAX_TRACE_DISTANCE\s+__MAX_TRACE_DISTANCE_RE__\s*$",
    }
    for label, pattern in required_trace_contract.items():
        if not re.search(pattern, template_text, re.MULTILINE):
            print("C17 self-test failed: input template lacks %s" % label,
                  file=sys.stderr)
            return 1
    unsupported_keywords = (
        "CUTOFF_UNRESOLVED_EXTENSION_PASSES",
        "CUTOFF_UNRESOLVED_EXTENSION_FACTOR",
        "TRAP_DETECTION",
        "TRAP_DRIFT_DETECTION",
        "TRAP_MIN_DRIFT_REVOLUTIONS",
    )
    for keyword in unsupported_keywords:
        if re.search(r"^%s\b" % re.escape(keyword), template_text,
                     re.MULTILINE):
            print("C17 self-test failed: compatibility template contains "
                  "unsupported keyword %s" % keyword, file=sys.stderr)
            return 1
    if len(expected_direction_keys(30.0, 30.0, ignore_poles=True)) != 60:
        print("C17 self-test failed: default directional-grid cardinality",
              file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="c17_selftest_") as raw:
        root = Path(raw)
        pos_dir = root / "positive"
        neg_dir = root / "negative"
        pos_dir.mkdir()
        neg_dir.mkdir()
        point = {
            "point_id": 0, "obs_lon_deg": 0.0, "obs_lat_deg": 0.0,
            "obs_alt_km": 9000.0,
        }

        def write_map(path, rows):
            lines = [
                'VARIABLES="lon_deg" "lat_deg" "Rc_GV" "Emin_MeV"',
                'ZONE T="synthetic"',
            ]
            lines.extend("%g %g %.17g 1" % row for row in rows)
            path.write_text("\n".join(lines) + "\n")

        def write_access(path, rows):
            lines = [
                'VARIABLES="lon_deg" "lat_deg" "rigidity_GV" '
                '"energy_MeV" "access_state"',
                'ZONE T="synthetic"',
            ]
            lines.extend("%g %g %.17g %.17g %d" % row for row in rows)
            path.write_text("\n".join(lines) + "\n")

        pos_path = pos_dir / "cutoff_gridless_dir_map_point_0000.dat"
        neg_path = neg_dir / "cutoff_gridless_dir_map_point_0000.dat"
        # With a 90-degree longitude grid, reflection exchanges 90 and 270
        # while leaving 0 and 180 fixed.  Matching negative sentinels are valid.
        write_map(pos_path, [(0.0, 0.0, -1.0), (90.0, 0.0, 2.0),
                             (180.0, 0.0, 3.0), (270.0, 0.0, 4.0)])
        write_map(neg_path, [(0.0, 0.0, -1.0), (90.0, 0.0, 4.0),
                             (180.0, 0.0, 3.0), (270.0, 0.0, 2.0)])
        summary, _, passed, _ = compare_upper_scan_reflection(
            [point], pos_dir, neg_dir, DEFAULT_RIGIDITIES_GV,
            1.0e-6, 1.0e-8, 1.0, 0.0,
            90.0, 90.0, ignore_poles=True)
        if (not passed or summary[0]["n_pairs_checked"] != 4 or
                summary[0]["n_matching_negative_sentinels"] != 1):
            print("C17 self-test failed: valid symmetric maps did not pass",
                  file=sys.stderr)
            return 1

        # A mutually plausible but truncated map must fail coverage before it
        # can be mistaken for a successful output-to-output comparison.
        write_map(neg_path, [(0.0, 0.0, -1.0), (90.0, 0.0, 4.0),
                             (180.0, 0.0, 3.0)])
        summary, _, passed, _ = compare_upper_scan_reflection(
            [point], pos_dir, neg_dir, DEFAULT_RIGIDITIES_GV,
            1.0e-6, 1.0e-8, 1.0, 0.0,
            90.0, 90.0, ignore_poles=True)
        if passed or summary[0]["n_missing_negative"] != 1:
            print("C17 self-test failed: truncated map was not rejected",
                  file=sys.stderr)
            return 1

        try:
            map_to_dict([DirCell(0.0, 0.0, 1.0, 1.0),
                         DirCell(360.0, 0.0, 1.0, 1.0)])
        except RuntimeError:
            pass
        else:
            print("C17 self-test failed: duplicate direction was not rejected",
                  file=sys.stderr)
            return 1

        # Rebuild complete synthetic dense direct-access cubes and require the
        # same three-state value at every reflected rigidity node.
        pos_access = pos_dir / "cutoff_gridless_dir_access_point_0000.dat"
        neg_access = neg_dir / "cutoff_gridless_dir_access_point_0000.dat"
        pos_rows = []
        neg_rows = []
        state_by_lon = {0.0: (0, 1), 90.0: (1, 1),
                        180.0: (2, 2), 270.0: (1, 0)}
        for lon in (0.0, 90.0, 180.0, 270.0):
            for rigidity, state in zip((1.0, 2.0), state_by_lon[lon]):
                pos_rows.append((lon, 0.0, rigidity, rigidity * 100.0, state))
            reflected_lon = reflected_direction_key(lon, 0.0)[0]
            for rigidity, state in zip((1.0, 2.0), state_by_lon[lon]):
                neg_rows.append((reflected_lon, 0.0, rigidity,
                                 rigidity * 100.0, state))
        write_access(pos_access, pos_rows)
        write_access(neg_access, neg_rows)
        summary, _, passed, _ = compare_direct_access_reflection(
            [point], pos_dir, neg_dir, 0.0, 0.30, 0.20, 0,
            90.0, 90.0, ignore_poles=True)
        if (not passed or summary[0]["n_access_samples"] != 8 or
                summary[0]["n_access_state_mismatches"] != 0):
            print("C17 self-test failed: direct-access reflection comparison",
                  file=sys.stderr)
            return 1

        # A persistent mismatch across neighboring rigidity nodes is not a
        # penumbral-boundary ambiguity.  Corrupt the negative branch at the two
        # stable lon=90 nodes and require the stable-interior gate to reject it.
        corrupted_neg_rows = []
        reflected_90 = reflected_direction_key(90.0, 0.0)[0]
        for row in neg_rows:
            if math.isclose(row[0], reflected_90, abs_tol=1.0e-12):
                row = (row[0], row[1], row[2], row[3], 0)
            corrupted_neg_rows.append(row)
        write_access(neg_access, corrupted_neg_rows)
        summary, _, passed, _ = compare_direct_access_reflection(
            [point], pos_dir, neg_dir, 0.0, 0.30, 0.20, 0,
            90.0, 90.0, ignore_poles=True)
        if passed or summary[0]["n_stable_state_mismatches"] != 2:
            print("C17 self-test failed: persistent stable mismatch was not rejected",
                  file=sys.stderr)
            return 1

    print("C17 self-test: PASS")
    return 0


def parse_args():
    parser = argparse.ArgumentParser(
        description="C17 charge-sign / meridional-reflection symmetry test",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/C17/run_C17.py -np 4 -nt 16
          python srcEarth/test/C17/run_C17.py --comparison-mode UPPER_SCAN --skip-run
          python srcEarth/test/C17/run_C17.py --movers BORIS,RK4 --dir-lon-res 60 --dir-lat-res 30
          python srcEarth/test/C17/run_C17.py --lats=-60,-30,0,30,60 --dry-run
          python srcEarth/test/C17/run_C17.py --skip-run --workdir test_output/C17_gridless
        """),
    )
    parser.add_argument("-np", type=int, default=4, help="MPI ranks passed to mpirun; default: 4")
    parser.add_argument("-nt", type=int, default=16, help="thread count passed to AMPS; default: 16")
    parser.add_argument("--movers", default="BORIS", help="comma-separated mover list; default: BORIS")
    parser.add_argument("--lats", default="-60,-30,0,30,60", help="observation latitudes, deg; default: -60,-30,0,30,60. Use --lats=-60,... if needed")
    parser.add_argument("--lons", default="0", help="observation longitudes, deg; default: 0")
    parser.add_argument("--alt", type=float, default=9000.0, help="observation altitude, km; default: 9000")
    parser.add_argument("--dir-lon-res", type=float, default=30.0, help="directional map longitude resolution, deg; default: 30")
    parser.add_argument("--dir-lat-res", type=float, default=30.0, help="directional map latitude resolution, deg; default: 30")
    parser.add_argument("--comparison-mode", choices=("DIRECT_ACCESS", "UPPER_SCAN"),
                        default="DIRECT_ACCESS",
                        help=("primary comparison product; DIRECT_ACCESS compares the "
                              "three-state access cube and is the default; UPPER_SCAN "
                              "reanalyzes legacy scalar cutoff maps"))
    parser.add_argument("--rigidities", default=",".join("%.10g" % r for r in DEFAULT_RIGIDITIES_GV), help="rigidity list for derived step-transmission check, GV")
    parser.add_argument("--cutoff-emin", type=float, default=0.1, help="CUTOFF_EMIN, MeV/n; default: 0.1")
    parser.add_argument("--cutoff-emax", type=float, default=20000.0, help="CUTOFF_EMAX, MeV/n; default: 20000")
    parser.add_argument("--cutoff-nenergy", type=int, default=50, help="CUTOFF_NENERGY; default: 50")
    parser.add_argument("--cutoff-upper-scan-n", type=int, default=80, help="CUTOFF_UPPER_SCAN_N; default: 80")
    parser.add_argument("--cutoff-max-particles", type=int, default=64, help="CUTOFF_MAX_PARTICLES for ordinary isotropic point cutoff; directional map is separate; default: 64")
    parser.add_argument("--dt-trace", type=float, default=0.25,
                        help="maximum base trace step, seconds; default: 0.25")
    parser.add_argument("--adaptive-dt", default="T", help="ADAPTIVE_DT setting T/F; default: T")
    parser.add_argument("--max-trace-time", type=float, default=2400.0,
                        help=("trace time applied to every trajectory, seconds; "
                              "default: 2400"))
    parser.add_argument("--max-trace-distance", type=float, default=0.0,
                        help=("maximum cumulative trace distance, Re; 0 disables "
                              "the energy-dependent path cap (default)"))
    parser.add_argument("--rc-rel-tol", "--rel-tol", dest="rc_rel_tol",
                        type=float, default=0.20,
                        help=("legacy UPPER_SCAN per-pair relative Rc tolerance; "
                              "default: 0.20"))
    parser.add_argument("--abs-tol-gv", type=float, default=1.0e-8,
                        help="legacy UPPER_SCAN absolute Rc tolerance, GV; default: 1e-8")
    parser.add_argument("--min-rc-pass-fraction", type=float, default=0.90,
                        help=("minimum finite Rc-pair fraction within tolerance at "
                              "each point in UPPER_SCAN mode; default: 0.90"))
    parser.add_argument("--max-transmission-mismatch-fraction", type=float,
                        default=0.03,
                        help=("maximum derived step-transmission mismatch fraction at "
                              "each point in UPPER_SCAN mode; default: 0.03"))
    parser.add_argument("--max-access-mismatch-fraction", type=float, default=0.01,
                        help=("maximum mismatch fraction among stable-interior "
                              "pairs at each point; default: 0.01"))
    parser.add_argument("--max-one-sided-unresolved-fraction",
                        "--max-unresolved-fraction",
                        dest="max_one_sided_unresolved_fraction",
                        type=float, default=0.05,
                        help=("maximum fraction with exactly one direct-access "
                              "state UNRESOLVED; default: 0.05. The old "
                              "--max-unresolved-fraction spelling is retained "
                              "as a compatibility alias."))
    parser.add_argument("--min-stable-sample-fraction", type=float, default=0.20,
                        help=("minimum fraction of samples in the resolved "
                              "stable-interior set at each point; default: 0.20"))
    parser.add_argument("--min-stable-state-count", type=int, default=5,
                        help=("minimum matching stable ALLOWED and FORBIDDEN "
                              "anchors at each point; default: 5 of each"))
    parser.add_argument("--include-poles", dest="ignore_poles", action="store_false", help="include +/-90 deg sky-map cells; default skips poles because longitude is degenerate there")
    parser.set_defaults(ignore_poles=True)
    parser.add_argument("--scheduler", default="DYNAMIC", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], help="gridless MPI scheduler; default: DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0, help="gridless dynamic chunk; 0 means AMPS auto heuristic; default: 0")
    parser.add_argument("--amps", default="./amps", help="AMPS executable path relative to launch directory; default: ./amps")
    parser.add_argument("--mpirun", default="mpirun", help="MPI launcher executable; default: mpirun")
    parser.add_argument("--workdir", default="test_output/C17_gridless", help="base work directory; default: test_output/C17_gridless")
    parser.add_argument("--skip-run", action="store_true", help="analyze existing output without launching AMPS")
    parser.add_argument("--keep", action="store_true", help="keep existing work directory instead of deleting it")
    parser.add_argument("--dry-run", action="store_true", help="render inputs and print commands, but do not run or analyze AMPS output")
    parser.add_argument("--self-test", action="store_true",
                        help="exercise mapping and coverage logic without launching AMPS")
    parser.add_argument("--version", action="version",
                        version="C17 runner schema %d (%s)" %
                                (RUNNER_SCHEMA_VERSION, RUNNER_RELEASE))
    return parser.parse_args()


def main():
    args = parse_args()
    if args.self_test:
        return self_test()
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.dir_lon_res <= 0.0 or args.dir_lat_res <= 0.0:
        raise SystemExit("directional-map resolutions must be positive")
    nlon_exact = 360.0 / args.dir_lon_res
    if abs(nlon_exact - round(nlon_exact)) > 1.0e-8:
        raise SystemExit("--dir-lon-res must divide 360 exactly for reflection mapping")
    nlat_exact = 180.0 / args.dir_lat_res
    if abs(nlat_exact - round(nlat_exact)) > 1.0e-8:
        raise SystemExit("--dir-lat-res must divide 180 exactly for reflection mapping")
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.alt <= 0.0:
        raise SystemExit("--alt must be positive")
    if args.dt_trace <= 0.0 or args.max_trace_time <= 0.0:
        raise SystemExit("DT_TRACE and MAX_TRACE_TIME must be positive")
    if args.max_trace_distance < 0.0:
        raise SystemExit("--max-trace-distance must be >= 0 (0 disables the cap)")
    for name in ("min_rc_pass_fraction", "max_transmission_mismatch_fraction",
                 "max_access_mismatch_fraction",
                 "max_one_sided_unresolved_fraction",
                 "min_stable_sample_fraction"):
        value = getattr(args, name)
        if not 0.0 <= value <= 1.0:
            raise SystemExit("--%s must be in [0,1]" % name.replace("_", "-"))
    if args.rc_rel_tol < 0.0 or args.abs_tol_gv < 0.0:
        raise SystemExit("cutoff tolerances must be non-negative")
    if args.min_stable_state_count < 0:
        raise SystemExit("--min-stable-state-count must be non-negative")

    movers = parse_mover_list(args.movers)
    obs_lats = parse_float_list(args.lats, "--lats")
    obs_lons = parse_float_list(args.lons, "--lons")
    if any(abs(math.sin(math.radians(lon))) > 1.0e-12 for lon in obs_lons):
        raise SystemExit(
            "C17 meridional reflection leaves only lon=0/180 observation points "
            "fixed; use --lons 0 (default) or 180")
    rigidities = parse_float_list(args.rigidities, "--rigidities")
    points = build_points(obs_lons, obs_lats, args.alt)

    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template_input = script_dir / "AMPS_PARAM_C17_gridless.in"
    reference_csv_src = script_dir / "reference_C17_symmetry.csv"
    base_workdir = (launch_dir / args.workdir).resolve()

    if not args.skip_run:
        if base_workdir.exists() and not args.keep:
            shutil.rmtree(base_workdir)
        base_workdir.mkdir(parents=True, exist_ok=True)
    else:
        if not base_workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % base_workdir)

    if not args.skip_run:
        generated_reference = base_workdir / "reference_C17_symmetry.csv"
        write_reference_csv(generated_reference, points, args, rigidities)

        # The checked-in table documents the default validation contract.  It is
        # immutable at runtime; custom grids receive a generated table only in
        # their output directory.  For the default configuration, detect source
        # drift instead of silently rewriting a tracked file.
        is_default_reference = (
            obs_lons == [0.0]
            and obs_lats == [-60.0, -30.0, 0.0, 30.0, 60.0]
            and abs(args.alt - 9000.0) < 1.0e-12
            and abs(args.dir_lon_res - 30.0) < 1.0e-12
            and abs(args.dir_lat_res - 30.0) < 1.0e-12
            and rigidities == list(DEFAULT_RIGIDITIES_GV)
            and abs(args.rc_rel_tol - 0.20) < 1.0e-15
            and abs(args.abs_tol_gv - 1.0e-8) < 1.0e-20
            and abs(args.min_rc_pass_fraction - 0.90) < 1.0e-15
            and abs(args.max_transmission_mismatch_fraction - 0.03) < 1.0e-15
            and abs(args.max_access_mismatch_fraction - 0.01) < 1.0e-15
            and abs(args.max_one_sided_unresolved_fraction - 0.05) < 1.0e-15
            and abs(args.min_stable_sample_fraction - 0.20) < 1.0e-15
            and args.min_stable_state_count == 5
            and abs(args.dt_trace - 0.25) < 1.0e-15
            and abs(args.max_trace_time - 2400.0) < 1.0e-12
            and abs(args.max_trace_distance) < 1.0e-15
            and args.ignore_poles
        )
        if is_default_reference:
            if not reference_csv_src.exists():
                raise SystemExit(
                    "Default C17 reference contract is missing: %s" %
                    reference_csv_src)
            # Compare normalized text lines so Git/platform newline conversion
            # cannot turn an otherwise identical CSV contract into a failure.
            if (generated_reference.read_text().splitlines() !=
                    reference_csv_src.read_text().splitlines()):
                raise SystemExit(
                    "Default C17 reference contract is stale; regenerate %s" %
                    reference_csv_src)

    amps_path = Path(args.amps)
    if not amps_path.is_absolute():
        amps_path = (launch_dir / amps_path).resolve()

    run_records = []
    all_summary = []
    all_pair_rows = []
    all_messages = []
    overall_passed = True

    for mover in movers:
        mover_dir = base_workdir / mover.lower()
        pos_dir = mover_dir / "charge_plus"
        neg_dir = mover_dir / "charge_minus_reflected"
        if args.skip_run and not neg_dir.exists():
            # Read pre-correction output without requiring the user to rename
            # its directory.  Only the directory label was wrong; the negative-
            # charge AMPS calculation itself is reusable.
            legacy_neg_dir = mover_dir / "charge_minus_reversed"
            if legacy_neg_dir.exists():
                neg_dir = legacy_neg_dir

        if not args.skip_run:
            pos_dir.mkdir(parents=True, exist_ok=True)
            neg_dir.mkdir(parents=True, exist_ok=True)
            render_input_template(template_input, pos_dir / "AMPS_PARAM_C17.in",
                                  "C17_%s_charge_plus" % mover.lower(),
                                  "PROTON", +1, 1.0073, points, args)
            # Use the proton mass with charge -1.  This isolates charge-sign symmetry
            # from species mass / kinetic-energy-to-rigidity conversion differences.
            render_input_template(template_input, neg_dir / "AMPS_PARAM_C17.in",
                                  "C17_%s_charge_minus" % mover.lower(),
                                  "NEGATIVE_PROTON", -1, 1.0073, points, args)

        for label, wdir in (("charge_plus", pos_dir), ("charge_minus_reflected", neg_dir)):
            cmd = [
                args.mpirun, "-np", str(args.np), str(amps_path),
                "-mode", "gridless",
                "-i", "AMPS_PARAM_C17.in",
                "-mover", mover,
                "-gridless-mpi-scheduler", args.scheduler,
                "-gridless-mpi-dynamic-chunk", str(args.dynamic_chunk),
                # Use the solver-specific aliases in the recorded command.  The
                # input deck carries the same GRIDLESS settings, so both the
                # rendered configuration and command provenance are explicit.
                "-gridless-parallel", "THREADS",
                "-gridless-threads", str(args.nt),
                "-cutoff-search", args.comparison_mode,
            ]
            if args.comparison_mode == "UPPER_SCAN":
                cmd.extend(["-cutoff-upper-scan-n",
                            str(args.cutoff_upper_scan_n)])
            log_file = wdir / ("C17_%s_%s_amps.log" % (mover.lower(), label))
            run_records.append({"mover": mover, "case": label, "workdir": str(wdir),
                                "command": cmd, "log_file": str(log_file)})
            if args.dry_run:
                print("[%s %s] %s" % (mover, label, " ".join(cmd)))
                continue
            if not args.skip_run:
                print("\nRunning C17 %s %s in %s" % (mover, label, wdir))
                print(" ".join(cmd))
                rc = run_command(cmd, cwd=wdir, log_path=log_file)
                if rc != 0:
                    overall_passed = False
                    all_messages.append("AMPS run failed for %s %s with exit code %d; see %s" %
                                        (mover, label, rc, log_file))

        if args.dry_run:
            continue

        if any("AMPS run failed for %s" % mover in msg for msg in all_messages):
            continue

        try:
            if args.comparison_mode == "DIRECT_ACCESS":
                summary, pair_rows, passed, messages = compare_direct_access_reflection(
                    points, pos_dir, neg_dir,
                    args.max_access_mismatch_fraction,
                    args.max_one_sided_unresolved_fraction,
                    args.min_stable_sample_fraction,
                    args.min_stable_state_count,
                    args.dir_lon_res, args.dir_lat_res,
                    ignore_poles=args.ignore_poles)
            else:
                summary, pair_rows, passed, messages = compare_upper_scan_reflection(
                    points, pos_dir, neg_dir, rigidities,
                    args.rc_rel_tol, args.abs_tol_gv,
                    args.min_rc_pass_fraction,
                    args.max_transmission_mismatch_fraction,
                    args.dir_lon_res, args.dir_lat_res,
                    ignore_poles=args.ignore_poles)
        except Exception as exc:
            overall_passed = False
            all_messages.append("C17 comparison failed for mover %s: %s" % (mover, exc))
            continue

        for r in summary:
            r["mover"] = mover
        for r in pair_rows:
            r["mover"] = mover
        all_summary.extend(summary)
        all_pair_rows.extend(pair_rows)
        if not passed:
            overall_passed = False
            all_messages.extend(["%s: %s" % (mover, m) for m in messages])

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "runner_schema_version": RUNNER_SCHEMA_VERSION,
        "runner_release": RUNNER_RELEASE,
        "passed": overall_passed if not args.dry_run else None,
        "dry_run": args.dry_run,
        "comparison_mode": args.comparison_mode,
        "points": points,
        "rigidities_GV": rigidities,
        "rc_rel_tol": args.rc_rel_tol,
        "abs_tol_GV": args.abs_tol_gv,
        "min_rc_pass_fraction": args.min_rc_pass_fraction,
        "max_transmission_mismatch_fraction": args.max_transmission_mismatch_fraction,
        "max_access_mismatch_fraction": args.max_access_mismatch_fraction,
        "max_one_sided_unresolved_fraction":
            args.max_one_sided_unresolved_fraction,
        "min_stable_sample_fraction": args.min_stable_sample_fraction,
        "min_stable_state_count": args.min_stable_state_count,
        "trace_configuration": {
            "profile": "PARSER_COMPATIBLE",
            "dt_trace_s": args.dt_trace,
            "adaptive_dt": bool_token(args.adaptive_dt),
            "max_trace_time_s": args.max_trace_time,
            "max_trace_distance_Re": args.max_trace_distance,
            "unresolved_extension_keywords_used": False,
            "trap_classifier_keywords_used": False,
        },
        "ignore_poles": args.ignore_poles,
        "run_records": run_records,
        "summary": all_summary,
        "messages": all_messages,
    }

    if args.dry_run:
        base_workdir.mkdir(parents=True, exist_ok=True)
        (base_workdir / "C17_result.json").write_text(json.dumps(result, indent=2))
        print("\nDry run complete.  Inputs and commands were prepared under %s" % base_workdir)
        return 0

    write_dict_csv(all_summary, base_workdir / "C17_summary.csv")
    common_pair_path = base_workdir / "C17_pairwise_comparison.csv"
    write_dict_csv(all_pair_rows, common_pair_path)
    if args.comparison_mode == "DIRECT_ACCESS":
        mode_pair_path = base_workdir / "C17_pairwise_access_states.csv"
    else:
        mode_pair_path = base_workdir / "C17_pairwise_directional_residuals.csv"
    write_dict_csv(all_pair_rows, mode_pair_path)
    if args.comparison_mode == "UPPER_SCAN":
        make_plot(all_pair_rows, base_workdir / "C17_residual_histogram.png")
    (base_workdir / "C17_result.json").write_text(json.dumps(result, indent=2))

    print("\nC17 summary written to:")
    print("  %s" % (base_workdir / "C17_summary.csv"))
    print("  %s" % common_pair_path)
    print("  %s" % mode_pair_path)
    print("  %s" % (base_workdir / "C17_result.json"))

    if overall_passed:
        print("\nC17 PASS: charge-sign / meridional-reflection symmetry satisfied.")
        return 0
    else:
        print("\nC17 FAIL:")
        for msg in all_messages:
            print("  - " + msg)
        return 1


if __name__ == "__main__":
    sys.exit(main())
