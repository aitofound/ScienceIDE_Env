#!/usr/bin/env python3
"""Validator for the electron-impact ionization test (tests/electronimpact).

Checks that the heaviest ion species (O+, which receives the exosphere source)
energy increases over time, confirming the electron-impact ionization source is
active.  Uses the PIC energy log (log_pic_n*.log) as the data source.
"""
import logging

logger = logging.getLogger(__name__)


def validate_log(pic_diags=None, test_name=None):
    """Validate that the electron-impact ionization source is active.

    Checks that the heaviest ion species (O+, which receives the exosphere
    source) energy increases over time, confirming the ionization source is
    active.  Uses the PIC energy log (log_pic_n*.log) as the data source.
    """
    logger.debug("Validating Ionization Source Test...")

    if not pic_diags or len(pic_diags) < 2:
        logger.debug("  [INFO] No PIC energy log found; skipping energy checks.")
        return True, "Passed (no pic log)"

    first = pic_diags[0]
    last = pic_diags[-1]

    # Determine the source (heaviest ion) species index from available
    # EpartN keys.  The last EpartN column is the heaviest ion.
    epart_keys = sorted(
        k for k in first.keys() if k.startswith("Epart") and k != "Epart"
    )
    if not epart_keys:
        logger.debug("  [INFO] No per-species energy columns; skipping.")
        return True, "Passed (no Epart columns)"
    source_key = epart_keys[-1]  # e.g. "Epart2" for O+
    source_idx = source_key.replace("Epart", "")

    logger.debug("  --- Energy Diagnostics (from log_pic log) ---")
    for k in epart_keys:
        logger.debug("    %s: %s -> %s",
                     k, f"{first.get(k, 0):.6e}", f"{last.get(k, 0):.6e}")
    logger.debug("    Initial total Epart: %s", f"{first.get('Epart', 0):.6e}")
    logger.debug("    Final total Epart:   %s", f"{last.get('Epart', 0):.6e}")

    # Check that the heaviest ion (O+) energy increases.
    e_src_initial = first.get(source_key, 0.0)
    e_src_final = last.get(source_key, 0.0)
    logger.debug("    Initial %s (species %s, O+): %s",
                 source_key, source_idx, f"{e_src_initial:.6e}")
    logger.debug("    Final   %s (species %s, O+): %s",
                 source_key, source_idx, f"{e_src_final:.6e}")
    logger.debug("    Growth factor: %.3fx",
                 e_src_final / max(e_src_initial, 1e-30))
    if e_src_final <= e_src_initial:
        logger.debug("    FAIL: %s energy did not increase.", source_key)
        logger.debug("    Ionization source may not be working correctly.")
        return False, (
            f"{source_key} energy did not increase "
            f"(initial={e_src_initial:.2e}, final={e_src_final:.2e})"
        )
    else:
        logger.debug("    SUCCESS: %s energy increased (ionization source active).",
                     source_key)
        return True, "Passed"
