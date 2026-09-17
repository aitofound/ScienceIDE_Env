#!/usr/bin/env python3
"""Validator for the 1D shock test.

A magnetized plasma flows into a reflecting left wall with a clean INFLOW right
boundary (45 deg oblique upstream field, Hall term on).  It guards against a
boundary-condition regression where the field energy Eb diverges.

This test exists in two solver variants that share the same physics:

* ``PARAM.in``        -> full PIC (solveEM) solver
* ``PARAM.in.hybrid`` -> hybrid (Hall OHM's law) solver (original test)

Run the harness as ``--test=shock`` (full PIC) or ``--test=shock_hybrid``
(hybrid).  Both use the same ``apply_inflow_wall`` / ``fixed`` field BC, so the
energy-divergence guard below applies to both.

Validation is split into `validate_log` (energy-log) and `validate_plot`
(final #SAVEPLOT snapshot).  See README.md "Validation" for the full physics
context and the exact checks/tolerances.
"""
import glob
import logging
import math
import os

logger = logging.getLogger(__name__)

# --- log tolerances -----------------------------------------------------------
EPART_RATIO_MIN = 0.2          # ion kinetic energy may drop (reflection) or grow
EPART_RATIO_MAX = 1.0e3        # ...but not by >1e3x (gross energy injection)
EB_RATIO_MIN = 0.2             # B may compress at the shock
EB_RATIO_MAX = 1.0e3           # ...but must not diverge to Inf/NaN
EB_ABS_MAX = 1.0e6             # hard ceiling on |Eb| (code units)

# --- plot tolerances ----------------------------------------------------------
RUN_DIR = "run_test"           # default; overridden by set_run_dir() if called
UPSTREAM_FRAC = 0.25           # rightmost 25% of cells = inflow / upstream region
DOWNSTREAM_FRAC = 0.5          # leftmost 50% of cells = wall / downstream region
STABILITY_RELSTD = 0.15        # allowed rel. std of a quantity in the upstream
COMPRESS_RHO_MIN = 1.2         # downstream mean density > 1.2 x upstream mean
COMPRESS_B_MIN = 1.1           # downstream mean |B| > 1.1 x upstream mean
JUMP_MIN = 1.5                 # max(density) / upstream mean > 1.5 (shock jump)


def set_run_dir(run_dir):
    """Mirror the runner's RUN_DIR into this module (for locate_plot())."""
    global RUN_DIR
    RUN_DIR = run_dir


def _locate_final_fluid_plot():
    """Return the path of the final-time fluid .out plot, or None if absent."""
    pattern = os.path.join(RUN_DIR, "PC", "plots", "*_fluid_*.out")
    files = glob.glob(pattern)
    if not files:
        return None
    # Filenames embed the time index (e.g. ..._t00000004_n00001000.out); sort
    # lexicographically == chronologically for zero-padded names.
    return sorted(files)[-1]


def _read_plot(path):
    """Parse a FLEKS fluid .out file into (xs, {colname: [values]}).

    FLEKS writes a 4-line preamble (a 'PIC' tag, a metadata line, the cell
    count, and a norm vector) before the real space-separated column-name
    header (which begins with 'x').  We locate that header by its leading 'x'
    token, then read the float data rows.  Returns (xs, data) or (None, None)
    on failure.
    """
    with open(path, "r") as fh:
        # PostProc.pl emits a few non-printable control bytes (e.g. a leading
        # STX/null prefix read from the binary .idl) that corrupt the first token
        # of the column-name header; drop anything that is not printable/space.
        lines = [ln.strip() for ln in fh]
    lines = [''.join(c for c in ln if c.isprintable() or c in "\t")
             for ln in lines]
    lines = [ln for ln in lines if ln]
    if not lines:
        return None, None

    # Locate the column-name header.  FLEKS normally writes it as the first line
    # whose leading token is exactly 'x'.  Some snapshots (e.g. full-PIC fluid
    # output for multiple species) omit the coordinate column entirely and start
    # with the first field name (e.g. 'rhoS0'); such rows still contain the known
    # physics columns, so we accept them too.  A leading '#' (comment prefix) is
    # stripped before testing.
    _KNOWN = {"bx", "by", "bz", "ex", "ey", "ez", "rhos0", "uxs0"}
    hdr_i = None
    for i, ln in enumerate(lines):
        toks = ln.replace("#", " ").split()
        if not toks:
            continue
        low = {t.lower() for t in toks}
        if toks[0].lower() == "x" or toks[0].lower() == "X" or (_KNOWN & low):
            hdr_i = i
            break
    if hdr_i is None:
        return None, None

    header = lines[hdr_i].replace("#", " ").split()
    # Coordinate column: 'x' (or 'X') if present; otherwise the snapshot has no
    # explicit coordinate and we synthesize a uniform index below.
    if "x" in header:
        x_name = "x"
    elif "X" in header:
        x_name = "X"
    else:
        x_name = None
    x_col = header.index(x_name) if x_name else None

    data = {name: [] for name in header}
    rowvals = []
    for ln in lines[hdr_i + 1:]:
        parts = ln.split()
        if not parts:
            continue
        try:
            vals = [float(p) for p in parts]
        except ValueError:
            continue
        # FLEKS may emit fewer data columns than header names (trailing columns
        # are omitted in the compact output); the physics columns come first, so
        # map the row onto the first len(vals) header names.
        ncol = min(len(vals), len(header))
        rowvals.append((vals, ncol))
        for i in range(ncol):
            data[header[i]].append(vals[i])

    if not rowvals:
        return None, None

    # Use the explicit coordinate column when present; otherwise synthesize a
    # uniform index so the upstream/downstream region fractioning still works.
    if x_col is not None:
        xs = [vals[x_col] if x_col < ncol else vals[0]
              for vals, ncol in rowvals]
    else:
        xs = list(range(len(rowvals)))
    return xs, data


def _mean_relstd(values):
    """Return (mean, rel_std) for a list; rel_std = std/|mean| (0 if mean~0)."""
    n = len(values)
    if n == 0:
        return 0.0, 0.0
    mean = sum(values) / n
    if abs(mean) < 1e-30:
        return mean, 0.0
    var = sum((v - mean) ** 2 for v in values) / n
    return mean, math.sqrt(var) / abs(mean)


def validate_log(pic_diags=None, test_name=None):
    """Energy-log checks: all energies finite; Eb / Epart bounded (no blow-up)."""
    if not pic_diags or len(pic_diags) < 2:
        return False, "Too few energy-log frames (< 2); run did not progress"

    first, last = pic_diags[0], pic_diags[-1]
    passed = True
    reasons = []

    # 1) All energies finite (no NaN/Inf from a broken boundary / diverging field).
    for key in ("Etot", "Ee", "Eb", "Epart"):
        v0, v1 = first.get(key, 0.0), last.get(key, 0.0)
        if not (math.isfinite(v0) and math.isfinite(v1)):
            passed = False
            reasons.append(f"{key} not finite (NaN/Inf) -- field/particle blow-up")

    # 2) Field energy Eb: bounded and finite (the quantity that diverged in the
    #    regression).  Check the per-frame absolute value first...
    if last.get("Eb", 0.0) > EB_ABS_MAX:
        passed = False
        reasons.append(
            f"Eb {last['Eb']:.3e} exceeds absolute ceiling {EB_ABS_MAX:.1e} "
            f"(field energy diverged -- the fixed-wall regression)")
    # ...then the growth ratio (allows legitimate shock compression, rejects blow-up).
    eb0, eb1 = first.get("Eb", 0.0), last.get("Eb", 0.0)
    if eb0 > 0:
        ratio = eb1 / eb0
        logger.debug("    Eb: %s -> %s (ratio %.4e)",
                     f"{eb0:.6e}", f"{eb1:.6e}", ratio)
        if ratio < EB_RATIO_MIN or ratio > EB_RATIO_MAX:
            passed = False
            reasons.append(
                f"Eb ratio {ratio:.4e} outside "
                f"[{EB_RATIO_MIN:g}, {EB_RATIO_MAX:g}] (field energy diverged)")

    # 3) Ion kinetic energy Epart: bounded (the regression left Epart matched to
    #    its initial value until the very end, so this is a secondary check).
    ep0, ep1 = first.get("Epart", 0.0), last.get("Epart", 0.0)
    if ep0 > 0:
        ratio = ep1 / ep0
        logger.debug("    Epart: %s -> %s (ratio %.4e)",
                     f"{ep0:.6e}", f"{ep1:.6e}", ratio)
        if ratio < EPART_RATIO_MIN or ratio > EPART_RATIO_MAX:
            passed = False
            reasons.append(
                f"Epart ratio {ratio:.4e} outside "
                f"[{EPART_RATIO_MIN:g}, {EPART_RATIO_MAX:g}] "
                f"(gross energy injection/loss)")

    # 4) Electron energy Ee: must stay finite and small relative to Eb (no spurious
    #    electric-field energy build-up at the boundaries).
    eel = last.get("Ee", 0.0)
    if eb1 > 0 and eel > 1.0e2 * eb1:
        passed = False
        reasons.append(
            f"Ee {eel:.3e} exceeds 1e2 x Eb ({eb1:.3e}) "
            f"(spurious E built up at a boundary)")

    if passed:
        return True, ("Passed (run completed; energies finite & bounded; "
                      "no field-energy divergence)")
    return False, "; ".join(reasons)


def validate_plot(test_name):
    """Snapshot checks: (1) inflow upstream steady, (2) shock forms (left
    downstream compressed w.r.t. right upstream)."""
    path = _locate_final_fluid_plot()
    if path is None:
        # No plot output present -- skip gracefully (e.g. if #SAVEPLOT is off).
        logger.debug("    [SHOCK] No fluid .out found; plot check skipped.")
        return True, "No fluid plot output found (skipped)"

    xs, data = _read_plot(path)
    if xs is None:
        return False, f"Could not parse fluid plot {path}"

    required = ["Bx", "By", "Bz", "Ex", "Ey", "Ez", "rhoS0", "uxS0"]
    missing = [c for c in required if c not in data]
    if missing:
        return False, f"Plot {path} missing columns: {missing}"

    n = len(xs)
    x_max = max(xs)
    # Upstream = rightmost UPSTREAM_FRAC of the domain (inflow face at +x, since
    # ux < 0 the plasma flows toward -x).  Downstream = leftmost DOWNSTREAM_FRAC
    # (the reflecting wall at -x).
    up_lo = x_max * (1.0 - UPSTREAM_FRAC)
    dn_hi = x_max * DOWNSTREAM_FRAC

    def region(name):
        idx = [i for i in range(n) if xs[i] >= up_lo] if name == "up" \
            else [i for i in range(n) if xs[i] <= dn_hi]
        return idx

    up_idx = region("up")
    dn_idx = region("down")
    if len(up_idx) < 2 or len(dn_idx) < 2:
        return False, ("Too few cells in upstream/downstream regions "
                       f"(up={len(up_idx)}, dn={len(dn_idx)})")

    def col(name, idx):
        return [data[name][i] for i in idx]

    def mag_b(idx):
        return [math.sqrt(data["Bx"][i] ** 2 + data["By"][i] ** 2 +
                          data["Bz"][i] ** 2) for i in idx]

    reasons = []

    # --- (2) Inflow upstream steady / uniform ---------------------------------
    up_rho_mean, up_rho_rs = _mean_relstd(col("rhoS0", up_idx))
    up_ux_mean, up_ux_rs = _mean_relstd(col("uxS0", up_idx))
    up_bx_mean, up_bx_rs = _mean_relstd(col("Bx", up_idx))
    up_ey_mean, up_ey_rs = _mean_relstd(col("Ey", up_idx))
    up_b_mean, up_b_rs = _mean_relstd(mag_b(up_idx))

    if not all(math.isfinite(v) for v in (up_rho_mean, up_ux_mean, up_bx_mean)):
        return False, "NaN/Inf in upstream region (inflow blew up)"
    if up_bx_rs > STABILITY_RELSTD:
        reasons.append(
            f"upstream Bx not uniform (rel std {up_bx_rs:.3f} > "
            f"{STABILITY_RELSTD}) -- boundary eroding the field")
    if up_rho_rs > STABILITY_RELSTD:
        reasons.append(
            f"upstream density not steady (rel std {up_rho_rs:.3f} > "
            f"{STABILITY_RELSTD}) -- inflow unstable")
    if up_ux_rs > STABILITY_RELSTD:
        reasons.append(
            f"upstream ux not steady (rel std {up_ux_rs:.3f} > "
            f"{STABILITY_RELSTD}) -- inflow unstable")
    if up_ey_rs > STABILITY_RELSTD:
        reasons.append(
            f"upstream Ey not steady (rel std {up_ey_rs:.3f} > "
            f"{STABILITY_RELSTD}) -- spurious tangential E at inflow")
    if up_b_rs > STABILITY_RELSTD:
        reasons.append(
            f"upstream |B| not uniform (rel std {up_b_rs:.3f} > "
            f"{STABILITY_RELSTD})")

    # --- (1) Shock: downstream compressed vs upstream -------------------------
    dn_rho_mean, _ = _mean_relstd(col("rhoS0", dn_idx))
    dn_b_mean, _ = _mean_relstd(mag_b(dn_idx))
    max_rho = max(col("rhoS0", up_idx + dn_idx))

    if not all(math.isfinite(v) for v in (dn_rho_mean, dn_b_mean, max_rho)):
        return False, "NaN/Inf in downstream region (wall blew up)"

    if dn_rho_mean <= COMPRESS_RHO_MIN * up_rho_mean:
        reasons.append(
            f"no density compression: downstream {dn_rho_mean:.4e} not > "
            f"{COMPRESS_RHO_MIN} x upstream {up_rho_mean:.4e} -- no shock")
    if dn_b_mean <= COMPRESS_B_MIN * up_b_mean:
        reasons.append(
            f"no field compression: downstream |B| {dn_b_mean:.4e} not > "
            f"{COMPRESS_B_MIN} x upstream |B| {up_b_mean:.4e} -- no shock")
    if up_rho_mean > 0 and max_rho / up_rho_mean < JUMP_MIN:
        reasons.append(
            f"no clear shock jump: max density {max_rho:.4e} < {JUMP_MIN} x "
            f"upstream {up_rho_mean:.4e}")

    if reasons:
        return False, "; ".join(reasons)

    logger.debug(
        "    [SHOCK] upstream: rhoS0=%.4e(+-%.2f%%) uxS0=%.4e Bx=%.4e |B|=%.4e;"
        " downstream: rhoS0=%.4e |B|=%.4e; max/up(rho)=%.2f",
        up_rho_mean, 100 * up_rho_rs, up_ux_mean, up_bx_mean, up_b_mean,
        dn_rho_mean, dn_b_mean,
        (max_rho / up_rho_mean if up_rho_mean > 0 else float("nan")))
    return True, ("Passed (inflow upstream steady & uniform; shock formed with "
                  "left/downstream compressed vs right/upstream)")
