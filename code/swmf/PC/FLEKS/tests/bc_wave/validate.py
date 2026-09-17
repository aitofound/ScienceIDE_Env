#!/usr/bin/env python3
"""Validator for the grouped wave-injection tests (tests/bc_wave/).

Two variants are discovered from this directory:
  - PARAM.in.mono  -> base_name "bc_inject_mono"   (Bz wave, pure-EM domain)
  - PARAM.in.alfven-> base_name "bc_inject_alfven" (shear Alfven wave, hybrid)

The single validate_log/validate_plot here branch on the variant's base_name
and apply variant-appropriate checks.
"""
import glob
import logging
import math
import os

logger = logging.getLogger(__name__)

import tests._shared.hybrid as _hyb

RUN_DIR = "run_test"


def set_run_dir(run_dir):
    """Point the plot helpers at the current run directory."""
    global RUN_DIR
    RUN_DIR = run_dir
    _hyb.set_run_dir(run_dir)

DB_OVER_B_MAX = 0.10   # linear shear-Alfven: dB/B must stay small


# ---------------------------------------------------------------------------
# Energy-log check (shared by both variants)
# ---------------------------------------------------------------------------
def validate_log(pic_diags=None, test_name=None):
    logger.debug("Validating %s (log)...", test_name)

    if not pic_diags or len(pic_diags) < 2:
        logger.debug("  No PIC energy log found; skipping.")
        return True, "Passed (no pic log)"

    e0 = pic_diags[0].get("Etot", 0.0)
    e1 = pic_diags[-1].get("Etot", 0.0)
    logger.debug("    Etot: %.4e -> %.4e", e0, e1)

    if not (math.isfinite(e0) and math.isfinite(e1)):
        return False, "Non-finite total energy"
    if e0 <= 0:
        return False, "Initial Etot is zero"
    if e1 <= 0:
        return False, "Final Etot is zero (wave collapsed)"

    ratio = e1 / e0
    logger.debug("    Etot_final/Etot_initial = %.4f", ratio)
    if ratio < 0.3 or ratio > 3.0:
        return False, f"Etot ratio {ratio:.3f} outside [0.3, 3.0]"

    return True, "Passed (energy finite and conserved)"


# ---------------------------------------------------------------------------
# Plot checks
# ---------------------------------------------------------------------------
def _read_plot():
    plots_dir = os.path.join(RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        return None, None
    out_file = out_files[-1]
    with open(out_file, "r") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return None, None
    vidx = {v.upper(): i for i, v in enumerate(lines[4].split())}
    rows = []
    for line in lines[5:]:
        cols = line.split()
        try:
            rows.append([float(c) for c in cols])
        except ValueError:
            continue
    return vidx, rows


def _interior_max(vidx, rows, name, x_name="X"):
    xi = vidx.get(x_name)
    vi = vidx.get(name)
    if xi is None or vi is None or not rows:
        return None, None, None
    xmin = min(r[xi] for r in rows)
    xmax = max(r[xi] for r in rows)
    lo = xmin + 0.33 * (xmax - xmin)
    hi = xmin + 0.67 * (xmax - xmin)
    v_int = 0.0
    v_all = 0.0
    for r in rows:
        v_all = max(v_all, abs(r[vi]))
        if lo <= r[xi] <= hi:
            v_int = max(v_int, abs(r[vi]))
    return v_int, v_all, (xmin, xmax)


def _check_wave_in_interior(vidx, rows, field, interior_thresh=0.1):
    """The injected field must be non-zero in the interior (away from faces)."""
    if vidx is None or rows is None:
        return True, "No .out data (skipped)"
    v_int, v_all, _ = _interior_max(vidx, rows, field)
    if v_int is None:
        return True, f"{field}/X not in .out (skipped)"
    logger.debug("    max |%s| interior = %.4e, overall = %.4e",
                 field, v_int, v_all)
    if v_int <= 0.0:
        return False, f"{field} zero in interior (wave did not propagate)"
    if v_all > 0.0 and v_int < interior_thresh * v_all:
        return False, f"{field} barely penetrates the interior"
    return True, f"Passed ({field} present in interior)"


def validate_plot(test_name):
    logger.debug("Validating %s (plot)...", test_name)
    vidx, rows = _read_plot()
    if test_name == "bc_inject_alfven":
        return _check_alfven(vidx, rows)
    # default: mono wave
    return _check_wave_in_interior(vidx, rows, "BZ")


def _check_alfven(vidx, rows):
    if vidx is None or rows is None:
        return True, "No .out data (skipped)"

    by_int, by_all, _ = _interior_max(vidx, rows, "BY")
    bx_guide = max(abs(r[vidx["BX"]]) for r in rows) if "BX" in vidx else 0.0
    logger.debug("    max |By| interior = %.4e, overall = %.4e, |Bx| = %.4e",
                 by_int, by_all, bx_guide)

    if by_int is None or by_int <= 0.0:
        return False, "By zero in interior (Alfven wave did not propagate)"

    if bx_guide > 0.0 and by_int > DB_OVER_B_MAX * bx_guide:
        return False, (f"By/Bx = {by_int/bx_guide:.3f} exceeds "
                       f"{DB_OVER_B_MAX*100:.0f}% -- not a linear Alfven wave")

    return True, "Passed (By wave present in interior, dB/B small)"
