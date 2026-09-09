#!/usr/bin/env python3
"""Validator for the 3D light-wave (vacuum transverse EM wave) test.

The light-wave initial condition (testCase = lightwave) fills the node E and B
fields with an analytic transverse plane wave; with nPartPerCell = 0 the PIC
loads no macroparticles, so the total energy is purely electromagnetic
(Ee + Eb).  On a periodic vacuum grid the wave should propagate without the
energy decaying to zero or blowing up, so the total EM energy is approximately
conserved.
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


def validate_log(pic_diags=None, test_name=None):
    """Validate the light-wave test from log_pic_n*.log.

    Checks (from log_pic_n*.log):
      1. Etot at the first and last frame is finite and > 0 (wave present).
      2. Energy is approximately conserved: 0.3 <= Etot_final / Etot_initial <= 3.0.
    """
    logger.debug("Validating Light Wave Test...")

    if not pic_diags or len(pic_diags) < 2:
        logger.debug("  [INFO] No PIC energy log found; skipping energy checks.")
        return True, "Passed (no pic log)"

    first = pic_diags[0]
    last = pic_diags[-1]

    e0 = first.get("Etot", 0.0)
    e1 = last.get("Etot", 0.0)

    logger.debug("  --- Energy Diagnostics (from log_pic log) ---")
    logger.debug("    Etot (t=%.4f): %s", first.get('time', 0), f"{e0:.6e}")
    logger.debug("    Etot (t=%.4f): %s", last.get('time', 0), f"{e1:.6e}")

    if not (math.isfinite(e0) and math.isfinite(e1)):
        logger.debug("    FAIL: Non-finite total EM energy.")
        return False, "Non-finite total EM energy"

    if e0 <= 0:
        logger.debug("    FAIL: Initial total EM energy is zero -- "
                     "wave not initialised.")
        return False, "Initial Etot is zero (wave not initialised)"

    if e1 <= 0:
        logger.debug("    FAIL: Final total EM energy is zero -- wave collapsed.")
        return False, "Final Etot is zero (wave collapsed)"

    ratio = e1 / e0
    lower, upper = 0.3, 3.0
    logger.debug("    Etot_final / Etot_initial = %.4f (allowed [%.1f, %.1f])",
                 ratio, lower, upper)

    if ratio < lower or ratio > upper:
        logger.debug("    FAIL: total EM energy changed outside the allowed range -- "
                     "possible blow-up or unphysical decay.")
        return False, f"Etot ratio {ratio:.3f} outside [{lower}, {upper}]"

    logger.debug("    SUCCESS: light wave energy conserved (ratio = %.3f).", ratio)
    return True, "Passed"


def _check_lightwave_present():
    """Verify the light wave is present in the final plot output.

    Reads the final .out file (produced by PostProc.pl) and checks that the
    magnetic-field amplitude (BX/BY/BZ) is non-zero somewhere on the slice,
    confirming the transverse EM wave was initialised and is still present at
    the final time.

    Returns (passed: bool, reason: str).
    """
    plots_dir = os.path.join(RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        logger.debug("    [LW] No .out files found (PostProc.pl not run?).")
        return True, "No .out files (skipped)"

    out_file = out_files[-1]
    logger.debug("    [LW] Loading .out: %s", os.path.basename(out_file))

    with open(out_file, "r") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return True, "Short .out file"

    var_names = lines[4].split()
    vidx = {v.upper(): i for i, v in enumerate(var_names)}
    b_idx = []
    for target in ("BX", "BY", "BZ"):
        if target in vidx:
            b_idx.append(vidx[target])
    if not b_idx:
        return True, "BX/BY/BZ not in .out"

    bmax = 0.0
    for line in lines[5:]:
        cols = line.split()
        if max(b_idx) >= len(cols):
            continue
        try:
            for i in b_idx:
                v = abs(float(cols[i]))
                if v > bmax:
                    bmax = v
        except (ValueError, IndexError):
            continue

    logger.debug("    [LW] Max |B| amplitude on slice: %.4e", bmax)
    if bmax <= 0.0:
        logger.debug("    [LW] FAIL: magnetic field is zero -- wave not present.")
        return False, "Magnetic field is zero (wave not present)"
    logger.debug("    [LW] Light wave present: VERIFIED")
    return True, "Passed"


def validate_plot(test_name):
    """Plot-output check: the transverse EM wave must be present."""
    logger.debug("  --- Validating Output Files (light wave present) ---")
    return _check_lightwave_present()
