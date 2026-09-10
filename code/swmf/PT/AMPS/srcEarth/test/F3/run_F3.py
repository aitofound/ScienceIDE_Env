#!/usr/bin/env python3
"""
F3 — dipole cutoff-filtered flux validation test.

Run from the directory containing the AMPS executable:

    python srcEarth/test/F3/run_F3.py -np 4
    python srcEarth/test/F3/run_F3.py --fast -np 4

F3 links the validated dipole cutoff calculation to the density/flux folding
path.  It runs gridless density/spectrum at explicit points on the 9000 km shell
and compares the output fluxes with a Størmer hard-cutoff power-law reference:

    F_ch = 4*pi*T_open*int_{max(E1,Ecut)}^{E2} J0*(E/E0)^(-gamma) dE

where Ecut is obtained by converting the vertical Størmer rigidity to proton
kinetic energy, and T_open is the analytic open-sky fraction outside the inner
absorbing sphere.  The comparison is approximate because AMPS evaluates a full
directional access map with penumbra, while the reference collapses access to a
vertical step function.
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
import threading
import time
from pathlib import Path

TEST_ID = "F3"
TEST_NAME = "Dipole cutoff-filtered flux"

RE_KM = 6371.2
R_INNER_RE = 1.01
R_INNER_KM = R_INNER_RE * RE_KM
ALT_KM_DEFAULT = 9000.0
STORMER_R0_GV = 14.9
MP_MEV = 938.2720813
C_LIGHT = 299792458.0
J0 = 1.0
E0 = 10.0
GAMMA = 3.5
EMIN = 1.0
EMAX = 1000.0
DEFAULT_LATS = [-70.0, -60.0, -45.0, -30.0, 0.0, 30.0, 45.0, 60.0, 70.0]
DEFAULT_LONS = [0.0, 90.0]
DEFAULT_SCAN_N = 100

# The fast profile is intended for routine regression testing rather than
# convergence studies.  It preserves every *kind* of F3 validation check while
# reducing all three multiplicative dimensions of the trajectory workload:
#
#   * geometry: keep both longitudes and five physically representative
#     latitudes (equator, an intermediate north/south pair, and a high-latitude
#     north/south pair);
#   * rigidity: use 12 log-rigidity samples instead of 100;
#   * direction: use a deterministic 288-direction subset instead of all 1152.
#
# The direction subset is particularly important.  The native grid is ordered
# as 24 zenith rings x 48 azimuths.  Asking the solver for exactly 288 directions
# selects every fourth entry, which retains all 24 zenith levels and 12 evenly
# spaced azimuths on every ring.  Thus the fast profile still samples the full
# sky rather than collapsing the test to a few special arrival directions.
#
# Workload comparison:
#
#   full: 2 x 9 x 100 x 1152 = 2,073,600 trajectories
#   fast: 2 x 5 x  12 x  288 =    34,560 trajectories
#
# The fast profile is therefore exactly 60 times cheaper than the documented
# full profile.  It remains suitable for detecting major regressions in cutoff
# filtering, spectral folding, symmetry, and latitude dependence.  The full
# profile remains the appropriate choice for accuracy or convergence studies.
FAST_LATS = [-70.0, -45.0, 0.0, 45.0, 70.0]
FAST_LONS = [0.0, 90.0]
FAST_SCAN_N = 12
FAST_DIRECTIONS_PER_ENERGY = 288
ENERGY_BINS = [
    ("BIN01_1_3", 1.0, 3.0),
    ("BIN02_3_10", 3.0, 10.0),
    ("BIN03_10_30", 10.0, 30.0),
    ("BIN04_30_100", 30.0, 100.0),
    ("BIN05_100_300", 100.0, 300.0),
    ("BIN06_300_1000", 300.0, 1000.0),
    ("FULL", 1.0, 1000.0),
]

# The gridless density driver currently uses a fixed 24 x 48 angular grid and
# groups 32 directions into one MPI work item.  Keeping the same values here
# lets the runner print an accurate workload estimate before AMPS starts.  The
# estimate is diagnostic only; the solver remains the source of truth and also
# prints the resolved values after parsing the generated input file.
N_ZENITH = 24
N_AZIMUTH = 48
FULL_DIRECTIONS_PER_ENERGY = N_ZENITH * N_AZIMUTH

# Canonical names accepted by the shared gridless mover selector.  The runner keeps
# the default as ``None`` so an invocation without --mover preserves the executable's
# existing default-selection behavior.  Supplying --mover makes the choice explicit
# and forwards the canonical token to AMPS as ``-mover <NAME>``.
MOVER_CHOICES = ("BORIS", "HC4", "RK2", "RK4", "RK6",
                 "GC2", "GC4", "GC6", "HYBRID")
DIRECTIONS_PER_MPI_TASK = 32


def format_elapsed(seconds):
    """Format elapsed wall time as HH:MM:SS for progress messages."""
    seconds = max(0, int(round(seconds)))
    hours, rem = divmod(seconds, 3600)
    minutes, seconds = divmod(rem, 60)
    return "%02d:%02d:%02d" % (hours, minutes, seconds)


def print_progress(message):
    """Print one runner progress line immediately, even through redirected I/O.

    Long MPI runs are commonly launched from batch systems where Python stdout
    is block-buffered.  Explicit flushing ensures that stage and heartbeat
    messages appear while F3 is running rather than only after the job exits.
    """
    print(message)
    sys.stdout.flush()


def estimate_gridless_work(points, args):
    """Return the task/trajectory counts expected for the F3 SCAN calculation.

    In SCAN mode ``DS_TRANSMISSION_SCAN_N`` determines the number of energy /
    rigidity samples.  Every point-energy pair evaluates either the complete
    24x48 direction grid or the deterministic subset requested through
    ``DS_MAX_PARTICLES``.  The collective MPI scheduler groups 32 directions
    into one task.  Reporting all three dimensions makes unexpectedly expensive
    configurations obvious before the executable is launched.
    """
    n_points = len(points)
    n_energy = args.scan_n
    # ``directions_per_energy`` is the resolved number actually requested from
    # the solver.  In the full profile it is the complete 24 x 48 sky grid.  In
    # fast mode it is the deterministic subset imposed through DS_MAX_PARTICLES.
    n_directions = args.directions_per_energy
    n_direction_blocks = (n_directions + DIRECTIONS_PER_MPI_TASK - 1) // DIRECTIONS_PER_MPI_TASK
    return {
        "n_points": n_points,
        "n_energy": n_energy,
        "n_directions": n_directions,
        "n_direction_blocks": n_direction_blocks,
        "n_tasks": n_points * n_energy * n_direction_blocks,
        "n_trajectories": n_points * n_energy * n_directions,
    }


def full_profile_trajectory_count():
    """Return the trajectory count of the documented full F3 configuration.

    Keeping this reference calculation in one helper prevents the progress
    message and README numbers from drifting away from the actual defaults.
    It is also used to report the effective speedup when a user combines
    ``--fast`` with explicit custom values such as ``--scan-n`` or ``--lats``.
    """
    return (len(DEFAULT_LONS) * len(DEFAULT_LATS) * DEFAULT_SCAN_N *
            FULL_DIRECTIONS_PER_ENERGY)


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

    argparse normally accepts ``--opt value`` for options with one argument, but
    a value such as ``-70,-60,0`` can be mistaken for another option because it
    begins with ``-``.  Users can always avoid that with ``--opt=-70,-60,0``.
    This helper also supports the more natural form used in the test README:

        --lats -70,-60,-45,-30,0,30,45,60,70

    It only rewrites values that look like negative numeric comma lists, so
    missing arguments and real options such as ``--skip-run`` are still reported
    by argparse in the usual way.
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


def stormer_vertical_cutoff_gv(lat_deg, alt_km):
    r_re = (RE_KM + alt_km) / RE_KM
    return STORMER_R0_GV * (math.cos(math.radians(lat_deg)) ** 4) / (r_re * r_re)


def energy_from_rigidity_mev(R_GV):
    pc_mev = 1000.0 * abs(R_GV)
    return math.sqrt(pc_mev * pc_mev + MP_MEV * MP_MEV) - MP_MEV


def rigidity_from_energy_gv(E_MeV):
    pc_mev = math.sqrt(max(0.0, E_MeV * (E_MeV + 2.0 * MP_MEV)))
    return pc_mev / 1000.0


def open_sky_fraction(alt_km, r_inner_km):
    r = RE_KM + alt_km
    if r <= r_inner_km:
        return 0.0
    s = r_inner_km / r
    return 0.5 * (1.0 + math.sqrt(max(0.0, 1.0 - s * s)))


def j_boundary(E_MeV):
    return J0 * (E_MeV / E0) ** (-GAMMA)


def speed_from_energy(E_MeV):
    gamma = 1.0 + E_MeV / MP_MEV
    beta2 = max(0.0, 1.0 - 1.0 / (gamma * gamma))
    return C_LIGHT * math.sqrt(beta2)


def flux_power_law(E1, E2):
    if E2 <= E1:
        return 0.0
    if abs(GAMMA - 1.0) < 1.0e-14:
        integ = J0 * (E0 ** GAMMA) * math.log(E2 / E1)
    else:
        integ = J0 * (E0 ** GAMMA) / (GAMMA - 1.0) * (E1 ** (1.0 - GAMMA) - E2 ** (1.0 - GAMMA))
    return 4.0 * math.pi * integ


def density_high_resolution(E1, E2, n=200000):
    if E2 <= E1:
        return 0.0
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
        prev_E = E
        prev_g = g
    return 4.0 * math.pi * total


def analytic_reference_for_point(point):
    rc = stormer_vertical_cutoff_gv(point["lat_deg"], point["alt_km"])
    ecut = energy_from_rigidity_mev(rc)
    topen = open_sky_fraction(point["alt_km"], R_INNER_KM)
    ref = {
        "Rc_vert_GV": rc,
        "Ecut_MeV": ecut,
        "T_open": topen,
        "T_at_Emax": topen if rigidity_from_energy_gv(EMAX) >= rc else 0.0,
        "density_total_m3": topen * density_high_resolution(max(EMIN, ecut), EMAX, n=40000),
        "flux_total_m2s1": topen * flux_power_law(max(EMIN, ecut), EMAX),
    }
    for name, e1, e2 in ENERGY_BINS:
        ref["flux_%s_m2s1" % name] = topen * flux_power_law(max(e1, ecut), e2)
    return ref


def compute_reference_rows(points, report_progress=False):
    """Build the analytical Størmer-reference table for all requested points.

    The density reference uses a high-resolution numerical integral and can take
    noticeable time when many points are requested.  Optional point-level output
    distinguishes time spent preparing the reference from time spent inside AMPS.
    """
    rows = []
    for i, p in enumerate(points):
        if report_progress:
            print_progress(
                "[F3][reference] point %d/%d: %s (lon=%g deg, lat=%g deg, alt=%g km)" %
                (i + 1, len(points), p["label"], p["lon_deg"], p["lat_deg"], p["alt_km"])
            )
        ref = analytic_reference_for_point(p)
        common = {
            "point": p["label"],
            "lon_deg": p["lon_deg"],
            "lat_deg": p["lat_deg"],
            "alt_km": p["alt_km"],
            "reference_type": "stormer_hard_cutoff_open_sky",
        }
        quantities = [
            ("Rc_vert_GV", ref["Rc_vert_GV"], "GV", "vertical Størmer cutoff rigidity"),
            ("Ecut_MeV", ref["Ecut_MeV"], "MeV", "proton kinetic energy equivalent to Rc_vert"),
            ("T_open", ref["T_open"], "1", "analytic straight-line open-sky fraction"),
            ("T_at_Emax", ref["T_at_Emax"], "1", "hard-cutoff transmission at SPEC_EMAX"),
            ("density_total_m3", ref["density_total_m3"], "m^-3", "T_open*4*pi*int J(E)/v(E)dE above Ecut"),
            ("flux_total_m2s1", ref["flux_total_m2s1"], "m^-2 s^-1", "T_open*4*pi*int J(E)dE above Ecut"),
        ]
        for name, _, _ in ENERGY_BINS:
            quantities.append(("flux_%s_m2s1" % name, ref["flux_%s_m2s1" % name], "m^-2 s^-1",
                               "T_open*4*pi*int over channel above Ecut"))
        for q, value, units, note in quantities:
            row = dict(common)
            row.update({
                "quantity": q,
                "expected_value": value,
                "units": units,
                "notes": note,
            })
            rows.append(row)
    return rows


def write_reference_csv(path, points, report_progress=False):
    """Write the generated F3 reference table, optionally showing point progress."""
    rows = compute_reference_rows(points, report_progress=report_progress)
    fields = ["point", "lon_deg", "lat_deg", "alt_km", "quantity", "expected_value",
              "units", "reference_type", "notes"]
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in rows:
            out = dict(row)
            for key in ("lon_deg", "lat_deg", "alt_km", "expected_value"):
                out[key] = "%.17e" % float(out[key])
            w.writerow(out)


def reference_map_from_rows(points):
    refs = {}
    for p in points:
        ref = analytic_reference_for_point(p)
        refs[p["label"]] = ref
    return refs


def _norm_key(k):
    return k.lower().replace(" ", "_").replace("-", "_").replace("^", "").replace("/", "_")


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
    lo = 0
    hi = len(xs) - 1
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


def flux_from_spectrum(E, T, E1, E2):
    xs = interval_grid(E, E1, E2)
    if len(xs) < 2:
        return 0.0
    ys = [interp(E, T, x) * j_boundary(x) for x in xs]
    return 4.0 * math.pi * trapz(xs, ys)


def density_from_spectrum(E, T, E1, E2):
    xs = interval_grid(E, E1, E2)
    if len(xs) < 2:
        return 0.0
    ys = [interp(E, T, x) * j_boundary(x) / speed_from_energy(x) for x in xs]
    return 4.0 * math.pi * trapz(xs, ys)


def max_rel_boundary_spectrum(E, Jb):
    m = 0.0
    for e, j in zip(E, Jb):
        ref = j_boundary(e)
        m = max(m, abs(j - ref) / max(abs(ref), 1.0e-300))
    return m


def max_abs_jlocal_residual(T, Jb, Jl):
    m = 0.0
    for t, jb, jl in zip(T, Jb, Jl):
        denom = max(abs(t * jb), 1.0e-300)
        m = max(m, abs(jl - t * jb) / denom)
    return m


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
    # DS_MAX_PARTICLES caps the total number of trajectories traced at one
    # observation point across all energy/rigidity samples.  Multiplying the
    # requested directions per energy by the resolved scan size makes the cap
    # exact and keeps custom ``--scan-n`` values from silently changing the
    # angular resolution of the selected profile.
    max_particles_per_point = args.scan_n * args.directions_per_energy
    for key, value in [
        # Gridless F3 is intentionally MPI-only.  Geopack and Tsyganenko models
        # maintain state in Fortran COMMON blocks, so a single computational
        # thread per rank is the conservative portable configuration even though
        # the present benchmark uses the analytic dipole field.
        ("MODE3D_PARALLEL", "SERIAL"),
        ("MODE3D_THREADS", 1),
        ("GRIDLESS_MPI_SCHEDULER", args.scheduler),
        ("GRIDLESS_MPI_DYNAMIC_CHUNK", args.dynamic_chunk),
        ("DS_TRANSMISSION_SCAN_N", args.scan_n),
        ("DS_MAX_PARTICLES", max_particles_per_point),
        ("DS_NINTERVALS", args.nintervals),
        ("DS_MAX_TRAJ_TIME", args.max_trace_time),
        ("DS_UNRESOLVED_TOL", args.unresolved_tol),
        ("DS_RETRY_UNRESOLVED", "T" if args.retry_unresolved else "F"),
        ("DS_SAVE_TERMINATION_SUMMARY", "T" if args.save_termination_summary else "F"),
        ("MAX_STEPS", args.max_steps),
        ("MAX_TRACE_TIME", args.max_trace_time),
        ("BOUNDARY_EVENT_TOL_M", args.boundary_refine_tol_m),
        ("BOUNDARY_EVENT_MAX_ITER", args.boundary_refine_max_iter),
        ("TRAP_DETECTION", "T" if args.trap_detection else "F"),
        ("TRAP_MIN_MIRROR_POINTS", args.trap_min_mirror_points),
        ("TRAP_MIN_BOUNCES", args.trap_min_bounces),
        ("TRAP_OUTER_MARGIN_RE", args.trap_outer_margin_re),
        ("TRAP_RADIAL_GROWTH_TOL_RE", args.trap_radial_growth_tol_re),
        ("TRAP_ENERGY_REL_TOL", args.trap_energy_rel_tol),
        ("TRAP_PARALLEL_DEADBAND", args.trap_parallel_deadband),
        ("DT_TRACE", args.dt_trace),
    ]:
        text = replace_key(text, key, value)
    block = points_block(points)
    text = re.sub(r'(?ms)^#OUTPUT_DOMAIN\s+.*?^\s*POINTS_END\s*$', "#OUTPUT_DOMAIN\n" + block, text)
    text += ("\n! F3 harness settings\n"
             "! F3_EXECUTION_PROFILE %s\n"
             "! F3_ALT_KM %.17g\n"
             "! F3_LONS %s\n"
             "! F3_LATS %s\n"
             "! F3_REQUESTED_NT %d\n"
             "! F3_EFFECTIVE_THREADS_PER_RANK 1\n"
             "! F3_MOVER %s\n"
             "! F3_GRIDLESS_MPI_SCHEDULER %s\n"
             "! F3_GRIDLESS_MPI_DYNAMIC_CHUNK %d\n"
             "! F3_DS_TRANSMISSION_SCAN_N %d\n"
             "! F3_DIRECTIONS_PER_ENERGY %d\n"
             "! F3_DS_MAX_PARTICLES_PER_POINT %d\n"
             "! F3_DS_NINTERVALS %d\n"
             "! F3_MAX_STEPS %d\n"
             "! F3_BOUNDARY_EVENT_TOL_M %.17g\n"
             "! F3_BOUNDARY_EVENT_MAX_ITER %d\n"
             "! F3_TRAP_DETECTION %s\n"
             "! F3_TRAP_MIN_MIRROR_POINTS %d\n"
             "! F3_TRAP_MIN_BOUNCES %d\n"
             "! F3_TRAP_OUTER_MARGIN_RE %.17g\n"
             "! F3_TRAP_RADIAL_GROWTH_TOL_RE %.17g\n"
             "! F3_TRAP_ENERGY_REL_TOL %.17g\n"
             "! F3_TRAP_PARALLEL_DEADBAND %.17g\n"
             "! F3_UNRESOLVED_TOL %.17g\n"
             "! F3_RETRY_UNRESOLVED %s\n") % (
                 args.execution_profile,
                 args.alt_km,
                 ",".join("%.12g" % x for x in args.lons_values),
                 ",".join("%.12g" % x for x in args.lats_values),
                 args.nt, args.mover if args.mover is not None else "AMPS_DEFAULT",
                 args.scheduler, args.dynamic_chunk, args.scan_n,
                 args.directions_per_energy, max_particles_per_point,
                 args.nintervals, args.max_steps, args.boundary_refine_tol_m,
                 args.boundary_refine_max_iter,
                 "T" if args.trap_detection else "F",
                 args.trap_min_mirror_points, args.trap_min_bounces,
                 args.trap_outer_margin_re, args.trap_radial_growth_tol_re,
                 args.trap_energy_rel_tol, args.trap_parallel_deadband,
                 args.unresolved_tol,
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
        rc_eff_col = find_col(drow, ["Rc_effective_GV", "Rc_eff_GV", "Rc_effective"])
        rc_low_col = find_col(drow, ["Rc_lower_GV", "Rc_lower"])
        rc_up_col = find_col(drow, ["Rc_upper_GV", "Rc_upper"])
        t_high_col = find_col(drow, ["T_high", "THigh"])
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
            "Rc_effective_GV": drow[rc_eff_col] if rc_eff_col is not None else float("nan"),
            "Rc_lower_GV": drow[rc_low_col] if rc_low_col is not None else float("nan"),
            "Rc_upper_GV": drow[rc_up_col] if rc_up_col is not None else float("nan"),
            "T_high": drow[t_high_col] if t_high_col is not None else float("nan"),
            "flux": flux,
            "E_MeV": spec["E_MeV"],
            "T": spec["T"],
            "J_boundary_perMeV": spec["J_boundary_perMeV"],
            "J_local_perMeV": spec["J_local_perMeV"],
        })
    return out



def read_termination_summary(workdir, points, required=True):
    path = workdir / "gridless_termination_summary.dat"
    if not path.exists():
        if required:
            raise RuntimeError("Missing required AMPS termination output: %s" % path)
        return []
    _, rows = read_table_file(path)
    if not rows:
        raise RuntimeError("Termination summary contains no data rows: %s" % path)
    for row in rows:
        ip = int(round(row.get("point_index", -1.0)))
        if ip < 0 or ip >= len(points):
            raise RuntimeError("Invalid point_index in termination summary: %r" % row)
    return rows

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


def mean(values):
    return sum(values) / float(len(values)) if values else float("nan")


def analyze(workdir, points, reference_csv, args):
    refs = reference_map_from_rows(points)
    outputs = read_outputs(workdir, points)
    termination_rows = read_termination_summary(
        workdir, points, required=args.save_termination_summary)
    rows = []
    passed = True

    for term in termination_rows:
        ip = int(round(term["point_index"]))
        label = points[ip]["label"]
        energy = term["E_MeV"]
        sampled = term["N_sampled"]
        retried = term.get("N_retried", 0.0)
        resolved = term["N_resolved"]
        allowed = term["N_allowed"]
        trapped = term.get("N_trapped", 0.0)
        inner_forbidden = term.get(
            "N_inner_forbidden", term["N_forbidden"] - trapped)
        forbidden = term["N_forbidden"]
        all_counts = (allowed + inner_forbidden + trapped + term["N_time_limit"] +
                      term["N_step_limit"] + term["N_distance_limit"] +
                      term["N_invalid_dt"] + term["N_invalid_field"] +
                      term["N_numerical_failure"])
        suffix = "E_%g_MeV" % energy
        passed = add_check(rows, "termination_count_closure", label, suffix + "_count_residual",
                           abs(sampled - all_counts), 0.0, abs_tol=0.0, units="count",
                           check_type="exact_internal_identity",
                           note="all termination categories must sum to N_sampled") and passed
        passed = add_check(rows, "resolved_count_closure", label, suffix + "_resolved_residual",
                           abs(resolved - allowed - inner_forbidden - trapped), 0.0,
                           abs_tol=0.0, units="count",
                           check_type="exact_internal_identity",
                           note="N_resolved must equal allowed plus inner-forbidden plus trapped") and passed
        passed = add_check(rows, "forbidden_count_closure", label,
                           suffix + "_forbidden_residual",
                           abs(forbidden - inner_forbidden - trapped), 0.0,
                           abs_tol=0.0, units="count",
                           check_type="exact_internal_identity",
                           note="N_forbidden must equal inner-forbidden plus trapped") and passed
        retry_excess = max(0.0, retried - sampled)
        passed = add_check(rows, "retry_count_bounds", label, suffix + "_retry_excess",
                           retry_excess, 0.0, abs_tol=0.0, units="count",
                           check_type="exact_internal_identity",
                           note="N_retried must be between zero and N_sampled") and passed
        passed = add_check(rows, "unresolved_fraction", label, suffix + "_unresolved_fraction",
                           term["unresolved_fraction"], 0.0, abs_tol=args.unresolved_tol,
                           units="1", check_type="numerical_resolution",
                           note="time/step/distance/invalid/numerical terminations are unresolved") and passed

    for data in outputs:
        p = data["point"]
        label = p["label"]
        ref = refs[label]
        E = data["E_MeV"]
        T = data["T"]
        Jb = data["J_boundary_perMeV"]
        Jl = data["J_local_perMeV"]
        if not (len(E) == len(T) == len(Jb) == len(Jl)) or len(E) < 2:
            raise RuntimeError("Malformed spectrum zone for %s" % label)

        passed = add_check(rows, "boundary_power_law", label, "max_rel_J_boundary", max_rel_boundary_spectrum(E, Jb),
                           0.0, abs_tol=args.differential_tol, units="1", check_type="setup_identity",
                           note="spectrum file must preserve the imposed power law") and passed
        passed = add_check(rows, "local_spectrum_closure", label, "max_rel_Jlocal_minus_TJb",
                           max_abs_jlocal_residual(T, Jb, Jl), 0.0, abs_tol=args.consistency_tol,
                           units="1", check_type="exact_internal_identity",
                           note="J_local(E) must equal T(E)*J_boundary(E)") and passed

        dens_spec = density_from_spectrum(E, T, EMIN, EMAX)
        passed = add_check(rows, "density_file_vs_spectrum", label, "density_total_relative_residual",
                           abs(data["density"] - dens_spec) / max(abs(dens_spec), 1.0e-300), 0.0,
                           abs_tol=args.consistency_tol, units="1", check_type="exact_internal_identity",
                           note="density file must match integration of saved T(E)J(E)/v(E)") and passed

        flux_spec_total = flux_from_spectrum(E, T, EMIN, EMAX)
        passed = add_check(rows, "flux_file_vs_spectrum", label, "flux_total_relative_residual",
                           abs(data["flux"]["F_tot_m2s1"] - flux_spec_total) / max(abs(flux_spec_total), 1.0e-300), 0.0,
                           abs_tol=args.consistency_tol, units="1", check_type="exact_internal_identity",
                           note="total flux file must match integration of saved T(E)J(E)") and passed

        # Analytical hard-cutoff comparison.  This is intentionally approximate;
        # the directional solver has penumbra and non-vertical access.
        analytic_tol = args.analytic_flux_tol
        for q_file, q_ref, units in [
            ("F_tot_m2s1", "flux_total_m2s1", "m^-2 s^-1"),
        ]:
            expected = ref[q_ref]
            value = data["flux"][q_file]
            if expected == 0.0:
                passed = add_check(rows, "stormer_hard_cutoff_flux", label, q_ref, value, expected,
                                   abs_tol=args.blocked_flux_abs_tol, units=units,
                                   check_type="approx_analytic_reference",
                                   note="vertical Størmer hard-cutoff flux; zero means Ecut exceeds channel upper edge") and passed
            else:
                passed = add_check(rows, "stormer_hard_cutoff_flux", label, q_ref, value, expected,
                                   rel_tol=analytic_tol, units=units,
                                   check_type="approx_analytic_reference",
                                   note="vertical Størmer hard-cutoff flux with analytic open-sky factor") and passed

        for name, e1, e2 in ENERGY_BINS:
            out_key = "F_%s_m2s1" % name
            ref_key = "flux_%s_m2s1" % name
            if out_key not in data["flux"]:
                passed = add_check(rows, "flux_channel_present", label, out_key, 0.0, 1.0,
                                   abs_tol=0.0, units="1", check_type="setup_identity",
                                   note="missing requested flux-channel column") and passed
                continue
            # Compare file-vs-spectrum for every channel tightly.
            f_spec = flux_from_spectrum(E, T, e1, e2)
            passed = add_check(rows, "flux_channel_file_vs_spectrum", label,
                               "%s_relative_residual" % out_key,
                               abs(data["flux"][out_key] - f_spec) / max(abs(f_spec), 1.0e-300),
                               0.0, abs_tol=args.consistency_tol, units="1",
                               check_type="exact_internal_identity",
                               note="channel flux file must match integration of saved T(E)J(E)") and passed
            # Compare analytical channel only when it is not nearly zero.  Tiny
            # high-cutoff channels are better covered by the blocked-total check.
            expected = ref[ref_key]
            if expected > args.channel_min_reference:
                passed = add_check(rows, "stormer_hard_cutoff_channel_flux", label, ref_key,
                                   data["flux"][out_key], expected, rel_tol=args.analytic_channel_tol,
                                   units="m^-2 s^-1", check_type="approx_analytic_reference",
                                   note="channel flux compared with vertical Størmer hard cutoff") and passed

        if ref["Rc_vert_GV"] > rigidity_from_energy_gv(EMAX) and data["T_high"] == 0.0:
            passed = add_check(rows, "stormer_cutoff_above_scan", label,
                               "Rc_above_scan_blocking", data["T_high"], 0.0,
                               abs_tol=args.t_high_tol, units="1",
                               check_type="approx_analytic_reference",
                               note="vertical cutoff exceeds the scan maximum; Rc_effective=0 is an above-scan sentinel") and passed
        else:
            passed = add_check(rows, "stormer_cutoff_diagnostic", label, "Rc_effective_minus_Rc_vert_GV",
                               data["Rc_effective_GV"], ref["Rc_vert_GV"], rel_tol=args.rc_tol,
                               units="GV", check_type="approx_analytic_reference",
                               note="effective cutoff from T(E) should be close to vertical Størmer cutoff away from penumbra") and passed
        passed = add_check(rows, "high_energy_transmission", label, "T_high", data["T_high"],
                           ref["T_at_Emax"], abs_tol=args.t_high_tol, units="1",
                           check_type="approx_analytic_reference",
                           note="T at SPEC_EMAX compared with hard-cutoff open-sky value") and passed

    # Symmetry and trend checks use actual AMPS output and are exact for a centered
    # aligned dipole up to directional quadrature/noise.
    by_lat = {}
    by_lon_lat = {}
    for data in outputs:
        lat = data["point"]["lat_deg"]
        lon = data["point"]["lon_deg"]
        by_lat.setdefault(lat, []).append(data)
        by_lon_lat[(lon, lat)] = data

    for lat, vals in sorted(by_lat.items()):
        flux_vals = [v["flux"]["F_tot_m2s1"] for v in vals]
        spread = (max(flux_vals) - min(flux_vals)) / max(abs(mean(flux_vals)), 1.0e-300)
        passed = add_check(rows, "dipole_longitude_symmetry", "lat_%g" % lat,
                           "flux_total_lon_spread", spread, 0.0, abs_tol=args.longitude_tol,
                           units="1", check_type="symmetry_identity",
                           note="centered aligned dipole should be longitude independent") and passed

    lons = sorted(set(p["lon_deg"] for p in points))
    lats = sorted(set(p["lat_deg"] for p in points))
    for lon in lons:
        for lat in lats:
            if lat <= 0.0:
                continue
            a = by_lon_lat.get((lon, lat))
            b = by_lon_lat.get((lon, -lat))
            if a is None or b is None:
                continue
            fa = a["flux"]["F_tot_m2s1"]
            fb = b["flux"]["F_tot_m2s1"]
            rel = abs(fa - fb) / max(abs(0.5 * (fa + fb)), 1.0e-300)
            passed = add_check(rows, "dipole_north_south_symmetry", "lon_%g_lat_%g" % (lon, lat),
                               "flux_total_NS_relative_difference", rel, 0.0,
                               abs_tol=args.ns_tol, units="1", check_type="symmetry_identity",
                               note="centered aligned dipole should be symmetric in north/south latitude") and passed

    abs_lat_means = []
    for alat in sorted(set(abs(x) for x in lats)):
        vals = []
        for lat in (alat, -alat):
            vals.extend([v["flux"]["F_tot_m2s1"] for v in by_lat.get(lat, [])])
        if vals:
            abs_lat_means.append((alat, mean(vals)))
    # Flux should generally increase toward high absolute latitude because the
    # Størmer cutoff decreases.  Allow tiny numerical non-monotonicity.
    prev = None
    for alat, fmean in abs_lat_means:
        if prev is not None:
            alat_prev, fprev = prev
            residual = max(0.0, fprev - fmean) / max(abs(fprev), abs(fmean), 1.0e-300)
            passed = add_check(rows, "latitude_access_trend", "abs_lat_%g" % alat,
                               "flux_total_monotonic_residual", residual, 0.0,
                               abs_tol=args.trend_tol, units="1", check_type="physical_trend",
                               note="mean flux should not decrease as |latitude| increases") and passed
        prev = (alat, fmean)

    summary_csv = workdir / "F3_summary.csv"
    fields = ["check", "point", "quantity", "check_type", "passed", "value",
              "expected_value", "abs_error", "rel_error", "rel_tol", "abs_tol",
              "units", "note"]
    with summary_csv.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in rows:
            w.writerow(row)

    result = {
        "test_id": TEST_ID,
        "test_name": TEST_NAME,
        "passed": passed,
        "n_points": len(points),
        "n_checks": len(rows),
        "n_failed": sum(1 for r in rows if not r["passed"]),
        "workdir": str(workdir),
        "summary_csv": str(summary_csv),
        "reference_csv": str(reference_csv),
        "settings": {
            "execution_profile": args.execution_profile,
            "fast": args.fast,
            "alt_km": args.alt_km,
            "lons": args.lons_values,
            "lats": args.lats_values,
            "np": args.np,
            "requested_nt": args.nt,
            "threads_per_rank": 1,
            "mover": args.mover if args.mover is not None else "AMPS_DEFAULT",
            "scheduler": args.scheduler,
            "dynamic_chunk": args.dynamic_chunk,
            "scan_n": args.scan_n,
            "directions_per_energy": args.directions_per_energy,
            "max_particles_per_point": args.scan_n * args.directions_per_energy,
            "nintervals": args.nintervals,
            "max_steps": args.max_steps,
            "max_trace_time_s": args.max_trace_time,
            "boundary_event_tolerance_m": args.boundary_refine_tol_m,
            "boundary_event_max_iterations": args.boundary_refine_max_iter,
            "trap_detection": args.trap_detection,
            "trap_min_mirror_points": args.trap_min_mirror_points,
            "trap_min_bounces": args.trap_min_bounces,
            "trap_outer_margin_Re": args.trap_outer_margin_re,
            "trap_radial_growth_tolerance_Re": args.trap_radial_growth_tol_re,
            "trap_energy_relative_tolerance": args.trap_energy_rel_tol,
            "trap_parallel_deadband": args.trap_parallel_deadband,
            "retry_unresolved": args.retry_unresolved,
            "save_termination_summary": args.save_termination_summary,
        },
        "tolerances": {
            "analytic_flux_tol": args.analytic_flux_tol,
            "analytic_channel_tol": args.analytic_channel_tol,
            "blocked_flux_abs_tol": args.blocked_flux_abs_tol,
            "rc_tol": args.rc_tol,
            "t_high_tol": args.t_high_tol,
            "longitude_tol": args.longitude_tol,
            "ns_tol": args.ns_tol,
            "trend_tol": args.trend_tol,
            "consistency_tol": args.consistency_tol,
            "differential_tol": args.differential_tol,
            "unresolved_tol": args.unresolved_tol,
        },
    }
    result_json = workdir / "F3_result.json"
    with result_json.open("w") as f:
        json.dump(result, f, indent=2, sort_keys=True)
        f.write("\n")

    print("F3 %s" % ("PASS" if passed else "FAIL"))
    print("Summary:", summary_csv)
    print("Result :", result_json)
    if not passed:
        print("Failed checks:")
        for r in rows:
            if not r["passed"]:
                print("  - {check} [{point}] {quantity}: value={value:.6e}, expected={expected_value:.6e}, rel={rel_error:.3e}, abs={abs_error:.3e}".format(**r))
    return passed


def describe_partial_outputs(workdir):
    """Return compact sizes for F3 output files that may appear during a run.

    Most AMPS Tecplot products are finalized near the end of the calculation, so
    an absent file is not itself an error.  Showing whether files exist and grow
    is nevertheless useful when diagnosing a run that becomes quiet after the
    trajectory phase.
    """
    names = [
        "gridless_points_density.dat",
        "gridless_points_spectrum.dat",
        "gridless_points_flux.dat",
        "gridless_termination_summary.dat",
    ]
    status = []
    for name in names:
        path = workdir / name
        if path.exists():
            status.append("%s=%dB" % (name, path.stat().st_size))
        else:
            status.append("%s=absent" % name)
    return ", ".join(status)


def run_command_with_progress(cmd, cwd, log_path, env, heartbeat_sec):
    """Run AMPS while teeing output and reporting prolonged silent periods.

    Previous versions sent all AMPS output only to ``F3_amps.log``.  The solver
    already emits a global task progress bar, but hiding the log made a healthy
    long calculation look hung.  This function streams every AMPS line to both
    the terminal and the log.  A separate reader thread is used so the main
    thread can emit a runner heartbeat even when AMPS has produced no new line.

    The heartbeat is deliberately based on *silence*, not just elapsed time.  If
    the native AMPS progress bar is active, no redundant runner messages are
    printed.  If AMPS stops producing output, the runner still reports that the
    process is alive, how long it has been quiet, the last line seen, and whether
    final output files have appeared.
    """
    start = time.monotonic()
    state_lock = threading.Lock()
    state = {
        "last_output_time": start,
        "last_line": "<no AMPS output received yet>",
        "bytes_written": 0,
        "reader_error": None,
    }

    with log_path.open("w", buffering=1) as log:
        command_line = "# Command: %s\n" % " ".join(cmd)
        log.write(command_line)
        log.flush()
        state["bytes_written"] += len(command_line.encode("utf-8", errors="replace"))

        proc = subprocess.Popen(
            cmd,
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            bufsize=1,
            env=env,
        )

        def copy_child_output():
            """Copy child output without blocking the runner heartbeat loop."""
            try:
                assert proc.stdout is not None
                for line in proc.stdout:
                    now = time.monotonic()
                    # Use one lock for state and both output streams so a runner
                    # heartbeat cannot split an AMPS line in either destination.
                    with state_lock:
                        state["last_output_time"] = now
                        stripped = line.strip()
                        if stripped:
                            state["last_line"] = stripped[-240:]
                        state["bytes_written"] += len(line.encode("utf-8", errors="replace"))
                        sys.stdout.write(line)
                        sys.stdout.flush()
                        log.write(line)
                        log.flush()
            except Exception as exc:
                with state_lock:
                    state["reader_error"] = repr(exc)

        reader = threading.Thread(target=copy_child_output, name="F3-AMPS-output-reader")
        reader.daemon = True
        reader.start()

        next_heartbeat = start + heartbeat_sec if heartbeat_sec > 0.0 else None
        while proc.poll() is None:
            time.sleep(0.25)
            now = time.monotonic()
            if next_heartbeat is None or now < next_heartbeat:
                continue

            with state_lock:
                quiet_for = now - state["last_output_time"]
                if quiet_for >= heartbeat_sec:
                    message = (
                        "[F3][runner-heartbeat] elapsed=%s pid=%d, no AMPS output for %s, "
                        "log=%dB; %s; last=\"%s\"\n" %
                        (format_elapsed(now - start), proc.pid, format_elapsed(quiet_for),
                         state["bytes_written"], describe_partial_outputs(cwd),
                         state["last_line"])
                    )
                    sys.stdout.write(message)
                    sys.stdout.flush()
                    log.write(message)
                    log.flush()
                    state["bytes_written"] += len(message.encode("utf-8", errors="replace"))

            # Advance by whole intervals so a delayed scheduler wake-up never
            # produces several back-to-back heartbeat lines.
            while next_heartbeat <= now:
                next_heartbeat += heartbeat_sec

        reader.join()
        rc = proc.wait()

        with state_lock:
            reader_error = state["reader_error"]
        if reader_error is not None:
            print_progress("[F3][runner] warning: output-reader error: %s" % reader_error)

    print_progress(
        "[F3][runner] AMPS process finished: exit=%d, elapsed=%s, log=%s" %
        (rc, format_elapsed(time.monotonic() - start), log_path)
    )
    return rc


def parse_args(argv=None):
    p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""
        F3: dipole cutoff-filtered flux.

        The test renders points on the 9000 km shell, runs gridless
        density/spectrum with FIELD_MODEL=DIPOLE and DS_TRANSMISSION_MODE=SCAN,
        and compares fluxes with a Størmer hard-cutoff power-law reference.
        """),
        epilog=textwrap.dedent("""
        Examples:
          python srcEarth/test/F3/run_F3.py
          python srcEarth/test/F3/run_F3.py --fast
          python srcEarth/test/F3/run_F3.py -np 4 --scan-n 100
          python srcEarth/test/F3/run_F3.py --mover BORIS --lons 0 --lats 45 --scan-n 40
          python srcEarth/test/F3/run_F3.py --mover RK4 --lons 0 --lats 45 --scan-n 40
          python srcEarth/test/F3/run_F3.py --lons 0,90,180,270 --lats -70,-60,-45,-30,0,30,45,60,70
          python srcEarth/test/F3/run_F3.py --analytic-flux-tol 0.75 --rc-tol 0.5
          python srcEarth/test/F3/run_F3.py --dry-run --workdir test_output/F3_dryrun
          python srcEarth/test/F3/run_F3.py --skip-run --workdir test_output/F3_gridless
        """))
    p.add_argument("-np", type=int, default=4, help="number of MPI ranks; default: 4")
    p.add_argument("-nt", type=int, default=1,
                   help=("accepted for compatibility but ignored by gridless F3; "
                         "the runner always uses one computational thread per MPI rank"))
    p.add_argument("--amps", default="./amps", help="AMPS executable path; default: ./amps")
    p.add_argument("--mpirun", default="mpirun", help="MPI launcher; default: mpirun")
    p.add_argument(
        "--mover",
        type=lambda value: value.upper(),
        choices=MOVER_CHOICES,
        default=None,
        help=("particle mover passed to AMPS as -mover; choices: %s. "
              "If omitted, preserve the AMPS executable default") %
             ",".join(MOVER_CHOICES),
    )
    p.add_argument("--workdir", default="test_output/F3_gridless", help="run/output directory")
    p.add_argument("--scheduler", choices=["DYNAMIC", "BLOCK_CYCLIC", "STATIC"], default="DYNAMIC")
    p.add_argument("--dynamic-chunk", type=int, default=0)
    p.add_argument("--fast", action="store_true",
                   help=("use the 60x-cheaper regression profile: five representative latitudes, "
                         "12 rigidity samples, and a deterministic 288-direction full-sky subset"))
    p.add_argument("--scan-n", type=int, default=None,
                   help=("DS_TRANSMISSION_SCAN_N value; default: 100, or 12 with --fast; "
                         "an explicit value overrides the selected profile"))
    p.add_argument("--directions-per-energy", type=int, default=None,
                   help=("number of deterministic sky directions traced at each rigidity sample; "
                         "default: 1152, or 288 with --fast. The solver implements this through "
                         "DS_MAX_PARTICLES = scan_n * directions_per_energy"))
    p.add_argument("--nintervals", type=int, default=120, help="DS_NINTERVALS value; default: 120")
    p.add_argument("--alt-km", type=float, default=ALT_KM_DEFAULT, help="shell altitude in km; default: 9000")
    p.add_argument("--lons", default=None,
                   help=("comma-separated longitudes in degrees; default: 0,90 for both full "
                         "and fast profiles; an explicit value overrides the selected profile"))
    p.add_argument("--lats", default=None,
                   help=("comma-separated latitudes in degrees; full default: "
                         "-70,-60,-45,-30,0,30,45,60,70; fast default: -70,-45,0,45,70; "
                         "an explicit value overrides the selected profile"))
    p.add_argument("--dt-trace", type=float, default=0.1, help="DT_TRACE upper bound")
    p.add_argument("--max-steps", type=int, default=300000, help="MAX_STEPS value")
    p.add_argument("--max-trace-time", type=float, default=900.0, help="MAX_TRACE_TIME / DS_MAX_TRAJ_TIME value")
    p.add_argument("--boundary-refine-tol-m", type=float, default=1.0,
                   help="BOUNDARY_EVENT_TOL_M geometric crossing tolerance")
    p.add_argument("--boundary-refine-max-iter", type=int, default=40,
                   help="BOUNDARY_EVENT_MAX_ITER reserved curved-step refinement limit")
    p.add_argument("--trap-detection", dest="trap_detection", action="store_true",
                   default=True,
                   help="enable conservative static-field trapped-orbit classification (default)")
    p.add_argument("--no-trap-detection", dest="trap_detection", action="store_false",
                   help="disable trapped-orbit classification")
    p.add_argument("--trap-min-mirror-points", type=int, default=8,
                   help="minimum parallel-velocity reversals before trapped classification")
    p.add_argument("--trap-min-bounces", type=int, default=4,
                   help="minimum complete bounce cycles before trapped classification")
    p.add_argument("--trap-outer-margin-re", type=float, default=1.0,
                   help="minimum clearance from every outer-box face during trapping test [Re]")
    p.add_argument("--trap-radial-growth-tol-re", type=float, default=0.05,
                   help="maximum cycle-to-cycle radial-envelope change [Re]")
    p.add_argument("--trap-energy-rel-tol", type=float, default=1.0e-4,
                   help="maximum relative momentum-magnitude spread for trapped classification")
    p.add_argument("--trap-parallel-deadband", type=float, default=1.0e-6,
                   help="deadband for detecting sign changes of p_parallel/|p|")
    p.add_argument("--unresolved-tol", type=float, default=0.01,
                   help="maximum unresolved trajectory fraction per point/energy")
    p.add_argument("--retry-unresolved", action="store_true",
                   help="retry unresolved trajectories with half DT_TRACE and doubled limits")
    p.add_argument("--save-termination-summary", dest="save_termination_summary",
                   action="store_true", default=True,
                   help="write gridless_termination_summary.dat (default)")
    p.add_argument("--no-save-termination-summary", dest="save_termination_summary",
                   action="store_false",
                   help="do not write gridless_termination_summary.dat")
    p.add_argument("--runner-heartbeat-sec", type=float, default=30.0,
                   help="print a runner heartbeat after this many seconds without AMPS output; 0 disables")
    p.add_argument("--task-heartbeat-sec", type=float, default=30.0,
                   help="ask each MPI rank to identify its current gridless task at this interval; 0 disables")
    p.add_argument("--slow-task-sec", type=float, default=60.0,
                   help="report any completed gridless MPI task taking at least this many seconds; 0 disables")
    p.add_argument("--analytic-flux-tol", type=float, default=0.75,
                   help="relative tolerance for approximate total flux vs Størmer hard-cutoff reference")
    p.add_argument("--analytic-channel-tol", type=float, default=0.75,
                   help="relative tolerance for nonzero channel fluxes vs Størmer hard-cutoff reference")
    p.add_argument("--channel-min-reference", type=float, default=1.0e-8,
                   help="skip approximate channel comparison when analytical channel flux is below this value")
    p.add_argument("--blocked-flux-abs-tol", type=float, default=1.0e-8,
                   help="absolute tolerance when analytical total flux is zero")
    p.add_argument("--rc-tol", type=float, default=0.75,
                   help="relative tolerance for Rc_effective vs vertical Størmer Rc")
    p.add_argument("--t-high-tol", type=float, default=0.35,
                   help="absolute tolerance for T_high vs hard-cutoff high-energy transmission")
    p.add_argument("--longitude-tol", type=float, default=5.0e-2,
                   help="allowed relative longitude spread in total flux at each latitude")
    p.add_argument("--ns-tol", type=float, default=5.0e-2,
                   help="allowed north/south relative difference in total flux")
    p.add_argument("--trend-tol", type=float, default=5.0e-2,
                   help="allowed monotonicity residual in the flux-vs-|latitude| trend")
    p.add_argument("--consistency-tol", type=float, default=2.0e-5,
                   help="tolerance for exact file-vs-spectrum consistency checks; allows Tecplot ASCII precision")
    p.add_argument("--differential-tol", type=float, default=2.0e-5,
                   help="tolerance for imposed power-law values in the spectrum file; allows Tecplot ASCII precision")
    p.add_argument("--skip-run", action="store_true", help="analyze existing files in --workdir")
    p.add_argument("--keep", action="store_true", help="do not delete an existing workdir")
    p.add_argument("--dry-run", action="store_true", help="render input and print the AMPS command without executing it")
    normalized_argv = normalize_negative_list_options(argv, ["--lats", "--lons"])
    args = p.parse_args(normalized_argv)

    # Resolve profile-dependent defaults only after argparse has recorded
    # whether the user supplied a value.  Using ``None`` as the parser default
    # lets explicit options override --fast naturally, which is useful for
    # controlled convergence experiments without creating another profile.
    if args.scan_n is None:
        args.scan_n = FAST_SCAN_N if args.fast else DEFAULT_SCAN_N
    if args.directions_per_energy is None:
        args.directions_per_energy = (FAST_DIRECTIONS_PER_ENERGY if args.fast
                                      else FULL_DIRECTIONS_PER_ENERGY)
    if args.lons is None:
        selected_lons = FAST_LONS if args.fast else DEFAULT_LONS
        args.lons = ",".join(str(x) for x in selected_lons)
    if args.lats is None:
        selected_lats = FAST_LATS if args.fast else DEFAULT_LATS
        args.lats = ",".join(str(x) for x in selected_lats)

    # The final profile label is resolved after the comma-separated longitude
    # and latitude strings have been converted to numbers in ``main``.  That
    # avoids classifying equivalent spellings such as ``0,90`` and ``0.0,90.0``
    # as different configurations.
    args.execution_profile = "fast" if args.fast else "full"
    return args


def main(argv=None):
    args = parse_args(argv)
    if args.np < 1:
        raise SystemExit("-np must be >= 1")
    if args.nt < 1:
        raise SystemExit("-nt must be >= 1")
    if args.nt != 1:
        print_progress(
            "[F3][parallel] -nt=%d requested, but gridless F3 is MPI-only; "
            "using one computational thread per rank" % args.nt
        )
    if args.dynamic_chunk < 0:
        raise SystemExit("--dynamic-chunk must be >= 0")
    if args.scan_n < 2:
        raise SystemExit("--scan-n must be >= 2")
    if args.directions_per_energy < 1 or args.directions_per_energy > FULL_DIRECTIONS_PER_ENERGY:
        raise SystemExit("--directions-per-energy must be in [1, %d]" %
                         FULL_DIRECTIONS_PER_ENERGY)
    if args.nintervals < 2:
        raise SystemExit("--nintervals must be >= 2")
    if args.max_steps < 1:
        raise SystemExit("--max-steps must be >= 1")
    if args.max_trace_time <= 0.0:
        raise SystemExit("--max-trace-time must be > 0")
    if args.boundary_refine_tol_m < 0.0:
        raise SystemExit("--boundary-refine-tol-m must be >= 0")
    if args.boundary_refine_max_iter < 1:
        raise SystemExit("--boundary-refine-max-iter must be >= 1")
    if args.trap_min_mirror_points < 2:
        raise SystemExit("--trap-min-mirror-points must be >= 2")
    if args.trap_min_bounces < 1:
        raise SystemExit("--trap-min-bounces must be >= 1")
    if args.trap_outer_margin_re < 0.0:
        raise SystemExit("--trap-outer-margin-re must be >= 0")
    if args.trap_radial_growth_tol_re < 0.0:
        raise SystemExit("--trap-radial-growth-tol-re must be >= 0")
    if args.trap_energy_rel_tol < 0.0:
        raise SystemExit("--trap-energy-rel-tol must be >= 0")
    if args.trap_parallel_deadband < 0.0 or args.trap_parallel_deadband >= 1.0:
        raise SystemExit("--trap-parallel-deadband must be in [0,1)")
    if args.unresolved_tol < 0.0 or args.unresolved_tol > 1.0:
        raise SystemExit("--unresolved-tol must be in [0,1]")
    if args.alt_km <= 0.0:
        raise SystemExit("--alt-km must be positive")
    if args.runner_heartbeat_sec < 0.0:
        raise SystemExit("--runner-heartbeat-sec must be >= 0")
    if args.task_heartbeat_sec < 0.0:
        raise SystemExit("--task-heartbeat-sec must be >= 0")
    if args.slow_task_sec < 0.0:
        raise SystemExit("--slow-task-sec must be >= 0")
    args.lons_values = parse_float_list(args.lons, "--lons")
    args.lats_values = parse_float_list(args.lats, "--lats")

    # ``fast-custom`` signals that --fast was requested but an explicit option
    # changed the nominal 60x profile.  The actual effective speedup is still
    # calculated and printed from the resolved workload below.
    if args.fast and not (args.scan_n == FAST_SCAN_N and
                          args.directions_per_energy == FAST_DIRECTIONS_PER_ENERGY and
                          args.lons_values == FAST_LONS and
                          args.lats_values == FAST_LATS):
        args.execution_profile = "fast-custom"
    for lat in args.lats_values:
        if lat < -90.0 or lat > 90.0:
            raise SystemExit("latitude out of range: %g" % lat)

    points = build_points(args.alt_km, args.lons_values, args.lats_values)
    work = estimate_gridless_work(points, args)
    full_trajectories = full_profile_trajectory_count()
    effective_speedup = (float(full_trajectories) /
                         float(max(1, work["n_trajectories"])))
    print_progress("[F3][stage 1/4] Preparing input geometry and analytical reference")
    print_progress(
        "[F3][profile] %s%s" %
        (args.execution_profile,
         " (nominal 60x regression profile)" if args.execution_profile == "fast" else "")
    )
    print_progress(
        "[F3][workload] points=%d, rigidity/energy samples=%d, directions/sample=%d, "
        "direction blocks=%d, MPI tasks=%d, trajectories=%d" %
        (work["n_points"], work["n_energy"], work["n_directions"],
         work["n_direction_blocks"], work["n_tasks"], work["n_trajectories"])
    )
    print_progress(
        "[F3][workload] effective reduction relative to the documented full profile: %.3gx" %
        effective_speedup
    )
    print_progress(
        "[F3][workload] DS_MAX_PARTICLES=%d per point (%d samples x %d directions)" %
        (args.scan_n * args.directions_per_energy,
         args.scan_n, args.directions_per_energy)
    )
    print_progress(
        "[F3][numerics] mover=%s, DT_TRACE=%g s, max time=%g s, max steps=%d, "
        "boundary tolerance=%g m, unresolved tolerance=%g" %
        (args.mover if args.mover is not None else "AMPS_DEFAULT",
         args.dt_trace, args.max_trace_time, args.max_steps,
         args.boundary_refine_tol_m, args.unresolved_tol)
    )
    print_progress(
        "[F3][trapping] enabled=%s, mirror points>=%d, bounce cycles>=%d, "
        "outer margin=%g Re, radial tol=%g Re, energy tol=%g" %
        ("T" if args.trap_detection else "F", args.trap_min_mirror_points,
         args.trap_min_bounces, args.trap_outer_margin_re,
         args.trap_radial_growth_tol_re, args.trap_energy_rel_tol)
    )
    print_progress(
        "[F3][workload] task heartbeat=%g s, slow-task threshold=%g s" %
        (args.task_heartbeat_sec, args.slow_task_sec)
    )
    launch_dir = Path.cwd().resolve()
    script_dir = Path(__file__).resolve().parent
    template = script_dir / "AMPS_PARAM_F3_gridless.in"
    reference_repo_csv = script_dir / "reference_F3_dipole_cutoff.csv"
    workdir = (launch_dir / args.workdir).resolve()

    if not args.skip_run:
        if workdir.exists() and not args.keep:
            shutil.rmtree(str(workdir))
        workdir.mkdir(parents=True, exist_ok=True)
        render_input(template, workdir / "AMPS_PARAM_F3.in", points, args)
    else:
        if not workdir.exists():
            raise SystemExit("--skip-run requested but workdir does not exist: %s" % workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    reference_used_csv = workdir / "reference_F3_dipole_cutoff_used.csv"
    write_reference_csv(reference_used_csv, points, report_progress=True)
    print_progress("[F3][stage 1/4] Reference preparation complete: %s" % reference_used_csv)
    if reference_repo_csv.exists() and not args.skip_run:
        shutil.copy2(str(reference_repo_csv), str(workdir / "reference_F3_dipole_cutoff_repository.csv"))

    if not args.skip_run:
        amps_path = Path(args.amps)
        if not amps_path.is_absolute():
            amps_path = (launch_dir / amps_path).resolve()
        cmd = [
            args.mpirun, "-np", str(args.np), str(amps_path),
            "-mode", "gridless",
            "-i", "AMPS_PARAM_F3.in",
            "-mode3d-parallel", "SERIAL",
            "-mode3d-mpi-scheduler", args.scheduler,
            "-mode3d-mpi-dynamic-chunk", str(args.dynamic_chunk),
            "-density-transmission", "SCAN",
        ]
        # The mover is a runtime executable option rather than an F3 input-file
        # keyword.  Add it only when the user requested an explicit method so the
        # no-option path remains backward compatible with earlier F3 runs.
        if args.mover is not None:
            cmd += ["-mover", args.mover]
        print_progress("[F3][stage 2/4] Launching AMPS")
        print_progress("Running: %s" % " ".join(cmd))
        if args.dry_run:
            print_progress("F3 dry run complete. Generated input under %s" % workdir)
            print_progress("Reference: %s" % reference_used_csv)
            return 0

        # The C++ gridless driver reads these optional diagnostics from the
        # environment.  Keeping them runner-controlled avoids adding permanent
        # output to unrelated production runs while making F3 verbose by default.
        child_env = os.environ.copy()
        child_env["AMPS_GRIDLESS_TASK_HEARTBEAT_SEC"] = "%.17g" % args.task_heartbeat_sec
        child_env["AMPS_GRIDLESS_SLOW_TASK_SEC"] = "%.17g" % args.slow_task_sec
        rc = run_command_with_progress(
            cmd,
            workdir,
            workdir / "F3_amps.log",
            child_env,
            args.runner_heartbeat_sec,
        )
        if rc != 0:
            print("AMPS failed with exit code %d; see %s" % (rc, workdir / "F3_amps.log"))
            return rc
        print_progress("[F3][stage 2/4] AMPS calculation complete")
    else:
        print_progress("[F3][stage 2/4] AMPS execution skipped; using existing output in %s" % workdir)

    print_progress("[F3][stage 3/4] Reading AMPS output and evaluating validation checks")
    ok = analyze(workdir, points, reference_used_csv, args)
    print_progress("[F3][stage 4/4] Validation analysis complete")
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
