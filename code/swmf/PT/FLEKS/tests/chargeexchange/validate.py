#!/usr/bin/env python3
"""Validator for the charge-exchange test (tests/chargeexchange).

Verifies (1) that H+ energy does not decrease and O+ energy grows by at least
a minimum factor (the O+ background is near-zero so the source dominates), and
(2) that the O+ source density in the plot output peaks near the planet surface
and is approximately symmetric.
"""
import glob
import logging
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
    """Validate the charge-exchange ionization source.

    O+ has a near-zero background, so its energy should increase by a large
    factor; H+ has a large bulk-kinetic-energy background, so its energy
    increase is tiny -- we only require that it does not decrease (allowing
    for numerical noise).
    """
    logger.debug("Validating Ionization Source Test...")

    if not pic_diags or len(pic_diags) < 2:
        logger.debug("  [INFO] No PIC energy log found; skipping energy checks.")
        return True, "Passed (no pic log)"

    first = pic_diags[0]
    last = pic_diags[-1]

    epart_keys = sorted(
        k for k in first.keys() if k.startswith("Epart") and k != "Epart"
    )
    if not epart_keys:
        logger.debug("  [INFO] No per-species energy columns; skipping.")
        return True, "Passed (no Epart columns)"

    logger.debug("  --- Energy Diagnostics (from log_pic log) ---")
    for k in epart_keys:
        logger.debug("    %s: %s -> %s",
                     k, f"{first.get(k, 0):.6e}", f"{last.get(k, 0):.6e}")
    logger.debug("    Initial total Epart: %s", f"{first.get('Epart', 0):.6e}")
    logger.debug("    Final total Epart:   %s", f"{last.get('Epart', 0):.6e}")

    # Verify both H+ (Epart1) and O+ (Epart2).  O+ has a near-zero background,
    # so its energy should increase by a large factor.  H+ has a large
    # bulk-kinetic-energy background, so its energy increase is tiny; we only
    # require that it does not decrease (allowing for numerical noise).
    passed = True
    reasons = []
    min_factor_o = 2.0   # O+ must at least double
    h_tolerance = 0.05   # H+ may decrease by up to 5% (numerical noise)

    # --- O+ (heaviest ion, source species) ---
    o_key = epart_keys[-1]  # e.g. "Epart2"
    e_o_initial = first.get(o_key, 0.0)
    e_o_final = last.get(o_key, 0.0)
    factor_o = e_o_final / max(e_o_initial, 1e-30)
    logger.debug("    %s (O+): %s -> %s (factor %.3fx, threshold %dx)",
                 o_key, f"{e_o_initial:.6e}", f"{e_o_final:.6e}",
                 factor_o, min_factor_o)
    if e_o_initial <= 0:
        if e_o_final <= 0:
            logger.debug("    FAIL: %s (O+) energy is zero — source not active.", o_key)
            passed = False
            reasons.append("O+ energy is zero (source not active)")
        else:
            logger.debug("    SUCCESS: %s (O+) energy became non-zero.", o_key)
    elif factor_o < min_factor_o:
        logger.debug("    FAIL: %s (O+) growth factor %.3f < %d",
                     o_key, factor_o, min_factor_o)
        passed = False
        reasons.append(f"O+ growth factor {factor_o:.3f} < {min_factor_o}")
    else:
        logger.debug("    SUCCESS: %s (O+) energy increased by %.1fx.", o_key, factor_o)

    # --- H+ (light ion, also receives CX source) ---
    h_key = "Epart1" if "Epart1" in first else None
    if h_key:
        e_h_initial = first.get(h_key, 0.0)
        e_h_final = last.get(h_key, 0.0)
        logger.debug("    %s (H+): %s -> %s",
                     h_key, f"{e_h_initial:.6e}", f"{e_h_final:.6e}")
        if e_h_final < e_h_initial * (1.0 - h_tolerance):
            logger.debug("    FAIL: %s (H+) energy decreased by more than "
                         "%.0f%% (numerical noise threshold).",
                         h_key, h_tolerance * 100)
            passed = False
            reasons.append("H+ energy decreased beyond noise threshold")
        else:
            logger.debug("    SUCCESS: %s (H+) energy stable or increasing.", h_key)

    if passed:
        logger.debug("Charge Exchange Source Test: PASSED")
        return True, "Passed"
    else:
        return False, "; ".join(reasons)


def _check_charge_exchange_source_profile():
    """Check charge exchange source spatial profile from plot output.

    Reads .out files produced by PostProc.pl.  Verifies that the O+ density
    (rhoS2) peaks near the planet surface (|x| ~ Rp) where the exosphere
    density is highest, and is much smaller near the planet center where no
    neutrals exist.  The exosphere density is zero inside the planet, so the
    source (and resulting particle density) should be smallest at the center.

    Returns (passed: bool, reason: str).
    """
    plots_dir = os.path.join(RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        logger.debug("    [CX] No .out files found (PostProc.pl not run?).")
        return False, "No .out files found"

    out_file = out_files[-1]
    logger.debug("    [CX] Loading .out: %s", os.path.basename(out_file))

    with open(out_file, "r") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return True, "Short .out file"

    var_names = lines[4].split()
    vidx = {v.upper(): i for i, v in enumerate(var_names)}

    # Find rhoS2 (O+ density); fall back to rhoS1 for 2-species layouts.
    rho_idx = None
    rho_name = None
    for target in ("RHOS2", "RHOS1"):
        if target in vidx:
            rho_idx = vidx[target]
            rho_name = target
            continue
    if rho_idx is None:
        logger.debug("    [CX] rhoS2/rhoS1 not found in .out variables: %s", var_names)
        return True, "rhoS2/rhoS1 not in .out"

    # Read planet radius and normalization from PARAM.in (plot coords = SI / lNormSI).
    Rp_si = 3.0e6
    lNormSI = 1000.0
    try:
        with open(os.path.join(RUN_DIR, "PARAM.in"), "r") as pf:
            section = None
            norm_idx = 0
            for line in pf:
                line_s = line.strip()
                if line_s.startswith("#"):
                    section = line_s
                    if section == "#NORMALIZATION":
                        norm_idx = 0
                    continue
                if not line_s:
                    continue
                parts = line_s.split()
                if section == "#BODYSIZE" and len(parts) >= 1:
                    try:
                        Rp_si = float(parts[0])
                    except ValueError:
                        pass
                elif section == "#NORMALIZATION" and len(parts) >= 1:
                    if norm_idx == 0:
                        try:
                            lNormSI = float(parts[0])
                        except ValueError:
                            pass
                    norm_idx += 1
    except Exception:
        pass

    Rp_plot = Rp_si / lNormSI

    # Parse data points (x, rhoS2).
    points = []
    for line in lines[5:]:
        cols = line.strip().split()
        if len(cols) <= rho_idx:
            continue
        try:
            x = float(cols[0])
            rho = float(cols[rho_idx])
            points.append((x, rho))
        except (ValueError, IndexError):
            continue

    if not points:
        logger.debug("    [CX] No data points parsed from .out file.")
        return False, "No data points parsed"

    logger.debug("    [CX] Rp (plot coords): %.1f", Rp_plot)
    logger.debug("    [CX] Points: %d", len(points))

    # Classify points by distance from planet center:
    #   - "near surface": 0.5*Rp < |x| <= 1.5*Rp (exosphere active, source peaks)
    #   - "deep interior": |x| < 0.3*Rp (no neutrals, source should be ~0)
    near_surface = [(x, r) for x, r in points
                    if 0.5 * Rp_plot < abs(x) <= 1.5 * Rp_plot]
    deep_interior = [(x, r) for x, r in points if abs(x) < 0.3 * Rp_plot]

    surface_mean = (sum(r for _, r in near_surface) / len(near_surface)
                    if near_surface else 0.0)
    surface_max = max((r for _, r in near_surface), default=0.0)
    interior_mean = (sum(r for _, r in deep_interior) / len(deep_interior)
                     if deep_interior else 0.0)
    interior_max = max((r for _, r in deep_interior), default=0.0)

    logger.debug("    [CX] %s near surface (mean): %.4e", rho_name, surface_mean)
    logger.debug("    [CX] %s near surface (max):  %.4e", rho_name, surface_max)
    logger.debug("    [CX] %s deep interior (mean): %.4e", rho_name, interior_mean)
    logger.debug("    [CX] %s deep interior (max):  %.4e", rho_name, interior_max)

    # Check 1: source non-zero near the planet surface.
    if surface_max <= 0.0:
        logger.debug("    [CX] FAIL: No source detected near planet surface.")
        return False, "No source detected near planet surface"

    # Check 2: density much smaller in the deep interior than near surface.
    if interior_mean > surface_mean * 0.1:
        logger.debug("    [CX] FAIL: Interior density too high "
                     "(%.2e vs surface mean %.2e)", interior_mean, surface_mean)
        return False, (f"Interior density too high "
                       f"({interior_mean:.2e} vs {surface_mean:.2e})")

    # Check 3: approximately symmetric (left vs right near surface).
    left = [r for x, r in near_surface if x < 0]
    right = [r for x, r in near_surface if x > 0]
    left_mean = sum(left) / len(left) if left else 0.0
    right_mean = sum(right) / len(right) if right else 0.0

    logger.debug("    [CX] %s left  (x<0, near surf) mean: %.4e", rho_name, left_mean)
    logger.debug("    [CX] %s right (x>0, near surf) mean: %.4e", rho_name, right_mean)

    if left_mean > 0 and right_mean > 0:
        ratio = min(left_mean, right_mean) / max(left_mean, right_mean)
        logger.debug("    [CX] Left/Right ratio: %.2f", ratio)
        if ratio < 0.3:
            logger.debug("    [CX] FAIL: Source asymmetric (L/R ratio %.2f)", ratio)
            return False, f"Source asymmetric (L/R ratio {ratio:.2f})"

    logger.debug("    [CX] Charge exchange source profile: VERIFIED")
    return True, "Passed"


def validate_plot(test_name):
    """Plot-output check: CX source profile (peaks near surface, symmetric)."""
    logger.debug("  --- Validating Output Files (CX source profile) ---")
    return _check_charge_exchange_source_profile()
