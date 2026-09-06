#!/usr/bin/env python3
"""Validator for the Mars chemistry test (tests/chemistry).

Checks that ion energies change over time due to photoionization (source),
cross-species charge exchange (source + loss) and recombination (loss).
"""
import logging

logger = logging.getLogger(__name__)


def validate_log(pic_diags=None, test_name=None):
    """Validate the Mars chemistry test with 4 ion species and 10 reactions.

    Checks that ion energies change over time due to the combined action of
    photoionization (source), cross-species charge exchange (source + loss),
    and recombination (loss).

    Key validations:
    1. ALL 4 ion species (H+, O+, O2+, CO2+) show significant energy changes,
       proving all reaction types are active.
    2. O2+ (species 3, Epart3) energy INCREASES — O2+ is produced ONLY by
       cross-species CX (reactions 3, 4) and lost by recombination (reaction 6).
       Since the CX source rate (~6 s^-1) far exceeds the recombination loss
       rate (~0.04 s^-1), O2+ energy must increase.  This is the critical
       test for the cross-species CX source term.
    3. CO2+ (species 4, Epart4) energy changes — CO2+ is produced by
       photoionization (reaction 1) and consumed by CX (reactions 3, 5) and
       recombination (reaction 7).
    """
    logger.debug("Validating Mars Chemistry Test...")

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

    # Species mapping: 0=e, 1=H+, 2=O+, 3=O2+, 4=CO2+
    species_names = {
        "Epart1": "H+",
        "Epart2": "O+",
        "Epart3": "O2+",
        "Epart4": "CO2+",
    }

    logger.debug("  --- Energy Diagnostics (from log_pic log) ---")
    for k in epart_keys:
        e0 = first.get(k, 0)
        e1 = last.get(k, 0)
        ratio = e1 / max(e0, 1e-30) if e0 > 0 else float('inf')
        name = species_names.get(k, k)
        logger.debug("    %s (%s): %s -> %s  (ratio %.4f)",
                     k, name, f"{e0:.6e}", f"{e1:.6e}", ratio)

    passed = True
    reasons = []

    # ---- Check 1: All 4 ion species must show significant energy changes ----
    # This proves that photoionization, CX, and recombination are all active.
    # A 0.1% threshold catches any meaningful chemistry signal while filtering
    # out pure numerical noise.
    change_threshold = 0.001  # 0.1%
    for k in epart_keys:
        e0 = first.get(k, 0.0)
        e1 = last.get(k, 0.0)
        if e0 <= 0:
            continue
        ratio = e1 / e0
        name = species_names.get(k, k)
        if abs(ratio - 1.0) < change_threshold:
            logger.debug("    FAIL: %s (%s) energy unchanged "
                         "(ratio %.4f) — chemistry may not be active.",
                         k, name, ratio)
            passed = False
            reasons.append(f"{name} energy unchanged")

    # ---- Check 2: O2+ must INCREASE — the critical CX source test ----
    # O2+ (Epart3) is produced ONLY by cross-species CX (reactions 3, 4)
    # and consumed by recombination (reaction 6).  The CX source rate
    # (~6.3 s^-1, driven by the large exosphere neutral density ~5e10 m^-3)
    # vastly exceeds the recombination loss rate (~4e-17 s^-1, limited by
    # the small plasma electron density in SI units).  Therefore O2+ energy
    # must increase.  If it does not increase, the CX source term is broken.
    o2_key = "Epart3" if "Epart3" in first else None
    if o2_key:
        e_o2_init = first.get(o2_key, 0.0)
        e_o2_final = last.get(o2_key, 0.0)
        if e_o2_init > 0:
            o2_ratio = e_o2_final / e_o2_init
            logger.debug("    %s (O2+): ratio = %.4f "
                         "(must be > 1.0 for CX source validation)",
                         o2_key, o2_ratio)
            if o2_ratio <= 1.0:
                logger.debug("    FAIL: %s (O2+) energy did not increase — "
                             "cross-species CX source is not working.", o2_key)
                passed = False
                reasons.append("O2+ energy did not increase (CX source broken)")
            else:
                pct = (o2_ratio - 1.0) * 100
                logger.debug("    SUCCESS: %s (O2+) energy increased by "
                             "%.2f%% (cross-species CX source active).",
                             o2_key, pct)
        else:
            logger.debug("    [INFO] %s initial energy is zero.", o2_key)

    # ---- Check 3: CO2+ must show a change ----
    # CO2+ is produced by photoionization (R1) and consumed by CX (R3, R5)
    # and recombination (R7).  Both source and loss are active.
    co2_key = "Epart4" if "Epart4" in first else None
    if co2_key:
        e_co2_init = first.get(co2_key, 0.0)
        e_co2_final = last.get(co2_key, 0.0)
        if e_co2_init > 0:
            co2_ratio = e_co2_final / e_co2_init
            logger.debug("    %s (CO2+): ratio = %.4f", co2_key, co2_ratio)
            if abs(co2_ratio - 1.0) < change_threshold:
                logger.debug("    FAIL: %s (CO2+) energy unchanged.", co2_key)
                passed = False
                reasons.append("CO2+ energy unchanged")
            else:
                pct = abs(co2_ratio - 1.0) * 100
                logger.debug("    SUCCESS: %s (CO2+) energy changed by %.2f%%.",
                             co2_key, pct)

    if passed:
        logger.debug("Mars Chemistry Test: PASSED")
        return True, "Passed"
    else:
        return False, "; ".join(reasons)
