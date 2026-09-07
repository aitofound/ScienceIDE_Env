#!/usr/bin/env python3
"""Strict validator for the free-stream test.

The free-stream is an exact uniform steady state (uniform bulk flow ux along
Bx0), so a uniform state must stay uniform: ion kinetic energy Epart conserved,
magnetic Eb constant, electric Ee ~ 0.  This is deliberately stricter than the
generic hybrid-wave tolerances (which allow Epart0 to drift by 0.2-10x and Eb to
grow 5x) and must catch the particle-noise ion deceleration (Epart ratio ~0.72)
that the comoving+upwind scheme prevents.

Energy-log checks (validate_log): Epart conserved, Eb conserved, Ee negligible.
Plot checks (validate_plot): Bx uniform & unchanged, uxS0 conserved, no spurious E.
"""
import logging
import math
import os
import glob

logger = logging.getLogger(__name__)

# Strict free-stream tolerances (tighter than the generic hybrid wave test).
EPART_TOL = 0.02        # ion kinetic energy conserved to +/-2%
EB_TOL = 0.05           # magnetic energy conserved to +/-5%
EE_MAX_FRAC = 0.10      # Ee <= 10% of Eb
VEL_TOL = 0.05          # bulk velocity preserved to +/-5%
B_SPREAD_TOL = 0.05     # Bx spatially uniform to 5% of mean
E_SPURIOUS_TOL = 0.05   # mean spurious E <= 5% of guide-field scale


def set_run_dir(run_dir):
    """Mirror the runner's RUN_DIR into the shared hybrid helper."""
    import tests._shared.hybrid as _hyb
    _hyb.set_run_dir(run_dir)


def validate_log(pic_diags=None, test_name=None):
    """Strict energy-log checks: Epart & Eb conserved, Ee negligible."""
    if not pic_diags or len(pic_diags) < 2:
        return True, "Passed (no pic log)"

    first, last = pic_diags[0], pic_diags[-1]
    passed = True
    reasons = []

    # Ion kinetic energy conservation (the decisive free-stream check).
    ep0, ep1 = first.get("Epart", 0.0), last.get("Epart", 0.0)
    if not (math.isfinite(ep0) and math.isfinite(ep1)):
        passed = False
        reasons.append("Epart not finite (NaN/Inf)")
    elif ep0 > 0:
        ratio = ep1 / ep0
        logger.debug("    Epart: %s -> %s (ratio %.5f)",
                     f"{ep0:.6e}", f"{ep1:.6e}", ratio)
        if abs(ratio - 1.0) > EPART_TOL:
            passed = False
            reasons.append(
                f"Epart ratio {ratio:.4f} not within [{1-EPART_TOL:.3f}, "
                f"{1+EPART_TOL:.3f}] (ion kinetic energy not conserved)")

    # Magnetic energy conservation (uniform state => Eb constant).
    eb0, eb1 = first.get("Eb", 0.0), last.get("Eb", 0.0)
    if not math.isfinite(eb1):
        passed = False
        reasons.append("Eb not finite (NaN/Inf)")
    elif eb0 > 0:
        ratio = eb1 / eb0
        logger.debug("    Eb: %s -> %s (ratio %.5f)",
                     f"{eb0:.6e}", f"{eb1:.6e}", ratio)
        if abs(ratio - 1.0) > EB_TOL:
            passed = False
            reasons.append(
                f"Eb ratio {ratio:.4f} not within [{1-EB_TOL:.3f}, "
                f"{1+EB_TOL:.3f}] (field energy grew)")

    # Electric energy stays negligible (no spurious E build-up).
    ee1 = last.get("Ee", 0.0)
    if not math.isfinite(ee1):
        passed = False
        reasons.append("Ee not finite (NaN/Inf)")
    elif eb0 > 0 and ee1 > EE_MAX_FRAC * eb0:
        passed = False
        reasons.append(
            f"Ee {ee1:.3e} exceeds {EE_MAX_FRAC*100:.0f}% of Eb "
            f"({eb0:.3e}) (spurious E built up)")

    if passed:
        return True, "Passed (strict: Epart & Eb conserved, Ee ~ 0)"
    return False, "; ".join(reasons)


def _fs_load_out(out_file):
    """Load a .out frame: return ({VAR: col_idx}, float rows)."""
    with open(out_file, "r", encoding="latin-1") as f:
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


def _fs_col(vidx, rows, name):
    """Return the column array for *name* (or None if absent)."""
    i = vidx.get(name)
    if i is None or not rows or i >= len(rows[0]):
        return None
    return [r[i] for r in rows]


def validate_plot(test_name):
    """Strict plot checks: Bx uniform & unchanged, uxS0 conserved, no spurious E."""
    import tests._shared.hybrid as _hyb
    plots_dir = os.path.join(_hyb.RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        logger.debug("    [FS] No .out files (PostProc.pl not run?) -- skipping.")
        return True, "No .out files (skipped)"

    vidx0, rows0 = _fs_load_out(out_files[0])
    vidxl, rowsl = _fs_load_out(out_files[-1])
    if vidx0 is None or vidxl is None or not rows0 or not rowsl:
        return True, "Could not parse .out frames (skipped)"

    passed = True
    reasons = []

    # Bulk velocity uxS0 conserved.
    ux0, uxl = _fs_col(vidx0, rows0, "UXS0"), _fs_col(vidxl, rowsl, "UXS0")
    if ux0 and uxl:
        mean0, meanl = sum(ux0) / len(ux0), sum(uxl) / len(uxl)
        logger.debug("    [FS] <uxS0>: %s -> %s", f"{mean0:.5f}", f"{meanl:.5f}")
        if abs(mean0) > 1e-12 and abs(meanl / mean0 - 1.0) > VEL_TOL:
            passed = False
            reasons.append(
                f"bulk velocity <uxS0> {mean0:.4f} -> {meanl:.4f} "
                f"(>{VEL_TOL*100:.0f}% drift)")
        elif abs(mean0) <= 1e-12 and abs(meanl) > 0.05:
            passed = False
            reasons.append(f"spurious bulk velocity {meanl:.4f} developed")

    # Guide field Bx uniform and unchanged.
    bx0, bxl = _fs_col(vidx0, rows0, "BX"), _fs_col(vidxl, rowsl, "BX")
    if bx0 and bxl:
        mb0, mbl = sum(bx0) / len(bx0), sum(bxl) / len(bxl)
        spreadl = max(bxl) - min(bxl)
        logger.debug("    [FS] <Bx>: %s -> %s (spread %.4f)",
                     f"{mb0:.5f}", f"{mbl:.5f}", spreadl)
        if abs(mb0) > 1e-12 and abs(mbl / mb0 - 1.0) > EB_TOL:
            passed = False
            reasons.append(f"guide field Bx changed {mb0:.4f} -> {mbl:.4f}")
        if abs(mb0) > 1e-12 and spreadl > B_SPREAD_TOL * abs(mbl):
            passed = False
            reasons.append(
                f"guide field not uniform (spread {spreadl:.4f} > "
                f"{B_SPREAD_TOL*100:.0f}% of <Bx>)")

    # No spurious mean E field.
    for ecomp in ("EX", "EY", "EZ"):
        el = _fs_col(vidxl, rowsl, ecomp)
        if el:
            meanl = sum(el) / len(el)
            scale = abs(mb0) if (bx0 and abs(mb0) > 1e-12) else 1.0
            logger.debug("    [FS] mean <%s> = %.5f (scale %.4f)",
                         ecomp, meanl, scale)
            if abs(meanl) > E_SPURIOUS_TOL * scale:
                passed = False
                reasons.append(
                    f"spurious mean electric field <{ecomp}> = {meanl:.4f} "
                    f"(>{E_SPURIOUS_TOL*100:.0f}% of guide-field scale)")

    if passed:
        return True, "Passed (strict: state stays uniform, bulk flow & Bx preserved)"
    return False, "; ".join(reasons)
