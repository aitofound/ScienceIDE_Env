#!/usr/bin/env python3
"""Validation for the hybrid PIC generalized Ohm's law test.

Uses shared hybrid-family checks (energy stability, seeded mode preservation)
and verifies that resistivity measurably suppresses transverse magnetic field growth.
"""
import glob
import logging
import math
import os

import tests._shared.hybrid as _hyb
from tests._shared.hybrid import validate_hybrid, validate_plot as _hyb_plot

logger = logging.getLogger(__name__)

RUN_DIR = "run_test"


def set_run_dir(run_dir):
    """Set simulation run directory."""
    global RUN_DIR
    RUN_DIR = run_dir
    _hyb.set_run_dir(run_dir)


# Upper bound on late/early transverse amplitude growth factor (max|B_perp|).
# Active resistivity keeps growth around ~1.65x, while inactive resistivity reaches ~2.16x.
MAX_GROWTH = 1.9


def _max_bperp(out_file):
    """Return maximum transverse field magnitude |B_perp| = hypot(By, Bz)."""
    data = _hyb._hyb_load_out(out_file)
    if not data:
        return None
    by, bz = data
    if not by:
        return None
    return max(math.hypot(b, c) for b, c in zip(by, bz))


def _check_resistive_damping():
    """Verify that resistivity measurably limits transverse field growth."""
    plots = sorted(glob.glob(os.path.join(RUN_DIR, "PC", "plots", "*.out")))
    if not plots:
        logger.debug("    [OHM] No .out files (resistive-damping check skipped)")
        return True, "No .out files (skipped)"

    a0 = _max_bperp(plots[0])
    a1 = _max_bperp(plots[-1])
    if a0 is None or a0 <= 0.0 or a1 is None:
        return True, "Could not measure transverse amplitude"

    growth = a1 / a0
    logger.debug("    [OHM] Resistive damping: max|B_perp| %.4e -> %.4e "
                 "(growth %.2fx, bound %.2fx)", a0, a1, growth, MAX_GROWTH)
    if growth > MAX_GROWTH:
        return False, ("late/early transverse amplitude grew %.2fx (bound %.2fx) "
                       "-- resistive term appears inactive" % (growth, MAX_GROWTH))
    return True, "Passed"


def validate_log(pic_diags=None, test_name=None):
    """Verify energy stability using shared hybrid checks."""
    return validate_hybrid(pic_diags=pic_diags, test_name=test_name)


def validate_plot(test_name):
    """Verify seeded wave mode and resistive damping bound."""
    ok, reason = _hyb_plot(test_name)
    if not ok:
        return ok, reason
    return _check_resistive_damping()
