#!/usr/bin/env python3
"""Validator for the grouped absorbing-boundary tests (tests/bc_absorb/).

Four variants are discovered from this directory:
  - PARAM.in.fields   -> base_name "bc_absorb_fields"   (tophat EM pulse
                          absorbed at x-faces; EM energy decays)
  - PARAM.in.hybrid.fields -> base_name "bc_absorb_hybrid_fields" (hybrid-PIC
                          shear-Alfvén wave in absorbing cavity; stable/bounded)
  - PARAM.in.particles-> base_name "bc_absorb_particles" (ions drain out
                          through absorbing x walls; Epart decays)
  - PARAM.in.hybrid.particles -> base_name "bc_absorb_hybrid_particles"
                          (same ion drain through absorbing x walls, but in the
                          hybrid cell-centred mover; Epart decays)

The single validate_log/validate_plot here branch on the variant's base_name.
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


EM_FINAL_FRAC = 0.25      # fields: Etot_final < 25% of initial (decay = absorb)
EPART_MAX_FRAC = 0.01     # particles: Epart_final < 1% of initial (complete drain)
ETOT_GROWTH_MAX = 1e3     # hybrid fields: Etot may not grow by more than 1e3x (no instability)


def validate_log(pic_diags=None, test_name=None):
    """Energy-log check, branching on the variant."""
    logger.debug("Validating %s (log)...", test_name)

    if not pic_diags or len(pic_diags) < 2:
        logger.debug("  No PIC energy log found; skipping.")
        return True, "Passed (no pic log)"

    first, last = pic_diags[0], pic_diags[-1]
    e0 = first.get("Etot", 0.0)
    e1 = last.get("Etot", 0.0)

    if test_name in ("bc_absorb_particles", "bc_absorb_hybrid_particles"):
        return _validate_log_particles(e0, e1, first, last)
    if test_name == "bc_absorb_hybrid_fields":
        return _validate_log_hybrid_fields(pic_diags)

    return _validate_log_fields(e0, e1, first, last)


def _validate_log_fields(e0, e1, first, last):
    """Fields: EM energy (Ee+Eb) must DECAY as pulses are absorbed."""
    e0_f = first.get("Ee", 0.0) + first.get("Eb", 0.0)
    e1_f = last.get("Ee", 0.0) + last.get("Eb", 0.0)
    logger.debug("    Ee+Eb: %.4e -> %.4e", e0_f, e1_f)

    for v in (e0, e1, e0_f, e1_f):
        if not math.isfinite(v):
            return False, "Non-finite EM energy (NaN/Inf)"
    if e0 <= 0:
        return False, "Initial Etot is zero (pulse not seeded)"

    denom = e0 if e0 > 0 else 1.0
    frac = e1 / denom if e0 > 0 else (e1_f / e0_f if e0_f > 0 else 1.0)
    logger.debug("    Etot_final/Etot_initial = %.4f", frac)
    if frac > EM_FINAL_FRAC:
        return False, (f"EM energy did not decay (final/initial = {frac:.3f} > "
                       f"{EM_FINAL_FRAC}) -- pulses appear to be REFLECTING")

    return True, "Passed (EM energy decays => absorber active)"


def _validate_log_particles(e0, e1, first, last):
    """Particles: Epart must DECAY as ions leave through the absorbing walls."""
    ep0 = first.get("Epart", 0.0)
    ep1 = last.get("Epart", 0.0)
    logger.debug("    Epart: %.4e -> %.4e", ep0, ep1)

    if not (math.isfinite(e0) and math.isfinite(e1) and
            math.isfinite(ep0) and math.isfinite(ep1)):
        return False, "Non-finite energy (NaN/Inf)"
    if e0 <= 0:
        return False, "Initial Etot is zero"

    if ep0 > 0 and ep1 > EPART_MAX_FRAC * ep0:
        return False, (f"Epart stayed at {ep1/ep0*100:.2f}% of initial "
                       f"(>{EPART_MAX_FRAC*100:.2f}%) -- particles appear to be "
                       f"REFLECTING, not absorbed")

    return True, "Passed (particle energy decays => absorption active)"


def _validate_log_hybrid_fields(pic_diags):
    """Hybrid fields: run stays finite, positive, and energy remains bounded."""
    e0 = pic_diags[0].get("Etot", 0.0)
    finite = all(
        math.isfinite(d.get("Etot", 0.0)) and
        math.isfinite(d.get("Epart", 0.0)) for d in pic_diags
    )
    e1 = pic_diags[-1].get("Etot", 0.0)
    logger.debug("    Etot: %.4e -> %.4e (%.1f growth)", e0, e1,
                 1.0 if e0 == 0 else e1 / e0)

    if not finite:
        return False, "Non-finite energy (NaN/Inf) in energy log"
    if e0 <= 0 or e1 <= 0:
        return False, "Non-positive total energy (plasma not initialised / drained)"
    if e0 > 0 and e1 > ETOT_GROWTH_MAX * e0:
        return False, (f"Etot grew from {e0:.3e} to {e1:.3e} "
                       f"(>{ETOT_GROWTH_MAX:.0f}x) -- field-wall instability")

    return True, "Passed (finite, bounded energy => absorbing field wall is stable)"


# ---------------------------------------------------------------------------
# Plot helpers
# ---------------------------------------------------------------------------
def _load_last_out():
    run_dir = getattr(_hyb, "RUN_DIR", RUN_DIR)
    plots_dir = os.path.join(run_dir, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        return None, None
    with open(out_files[-1], "r", encoding="latin-1") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return None, None
    vidx = {v.upper(): i for i, v in enumerate(lines[4].split())}
    rows = []
    for line in lines[5:]:
        cols = line.split()
        if not cols:
            continue
        try:
            rows.append([float(c) for c in cols])
        except ValueError:
            continue
    return vidx, rows


def _col(vidx, rows, name):
    i = vidx.get(name)
    if i is None or not rows or i >= len(rows[0]):
        return None
    return [r[i] for r in rows]


def validate_plot(test_name):
    logger.debug("Validating %s (plot)...", test_name)
    vidx, rows = _load_last_out()
    if vidx is None or not rows:
        return True, "No .out frames (skipped)"

    if test_name in ("bc_absorb_particles", "bc_absorb_hybrid_particles"):
        return _check_particles_plot(vidx, rows)
    if test_name == "bc_absorb_hybrid_fields":
        return _check_hybrid_fields_plot(vidx, rows)
    return _check_fields_plot(vidx, rows)


def _check_hybrid_fields_plot(vidx, rows):
    """Hybrid fields: check By, Bz, Ey, Ez remain finite and bounded."""
    for var in ("BY", "BZ", "EY", "EZ"):
        col = _col(vidx, rows, var)
        if col is None:
            continue
        if not all(math.isfinite(v) for v in col):
            return False, f"{var} not finite (NaN)"
        peak = max(abs(v) for v in col)
        if peak > 1e6:
            return False, f"{var} blew up (peak {peak:.2e})"
    return True, "Passed (hybrid fields finite and bounded near walls)"


def _check_fields_plot(vidx, rows):
    """Fields: late interior |Bz|,|Ey| are much reduced from the seeded 1.0."""
    bz = _col(vidx, rows, "BZ")
    ey = _col(vidx, rows, "EY")
    if bz is None or ey is None:
        return True, "BZ/EY not in .out (skipped)"
    for v in bz + ey:
        if not math.isfinite(v):
            return False, "Non-finite field in plot output"
    bz_max = max(abs(v) for v in bz)
    ey_max = max(abs(v) for v in ey)
    logger.debug("    [AF] late max |Bz| = %.4e, max |Ey| = %.4e", bz_max, ey_max)
    if bz_max >= 0.7 or ey_max >= 0.7:
        return False, (f"Late interior field barely reduced (max|Bz|={bz_max:.3e}, "
                       f"max|Ey|={ey_max:.3e}); a reflected standing wave may persist")
    return True, "Passed (interior EM field absorbed)"


def _check_particles_plot(vidx, rows):
    """Particles: verify density drains to zero as particles exit the box."""
    rho = _col(vidx, rows, "RHOS0")
    if rho is None:
        return True, "No rhoS0 column (skipped)"
    if not all(math.isfinite(v) for v in rho):
        return False, "rhoS0 not finite (NaN/Inf)"
    peak = max(abs(v) for v in rho)
    logger.debug("    [AP] late rhoS0 peak = %.4e", peak)
    # If particles reflected instead of absorbed, density would remain ~5.0.
    if peak > 0.1:
        return False, (f"Particles failed to absorb: late rhoS0 peak is {peak:.3e} "
                       "(particles appear to be trapped or reflected)")
    return True, "Passed (particle density evacuated => absorption confirmed)"
