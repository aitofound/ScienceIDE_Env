#!/usr/bin/env python3
"""Validator for the zero-current hybrid wave test (tests/zerocurrent).

No macroparticles are loaded (rho = 0 everywhere), so every cell is left inert:
the generalized Ohm's law is fully short-circuited by the ``if (rho > 0)`` guard
in assemble_ohm_E, so the electric field E = 0.  Faraday's law then gives
dB/dt = -curl E = 0: the seeded sinusoidal B perturbation is FROZEN and does NOT
propagate.
"""
import logging
import math

logger = logging.getLogger(__name__)


def validate_log(pic_diags=None, test_name=None):
    """Validate the zero-current hybrid wave test.

    The test passes iff (1) the magnetic energy Eb is conserved (the wave
    neither grows, decays, nor travels) AND (2) the electric field energy Ee
    stays ~0 -- the genuine signature that the field is frozen rather than
    merely energy-conserving (a non-dispersive propagating wave would also
    conserve Eb but would have Ee > 0).
    """
    logger.debug("=== Validating Zero-Current Hybrid Test ===")
    if not pic_diags:
        return False, "No diagnostics found"
    first = pic_diags[0]
    last = pic_diags[-1]
    passed = True
    reasons = []

    eb0 = first.get("Eb", 0.0)
    eb1 = last.get("Eb", 0.0)
    logger.debug("    Eb (magnetic): %s -> %s", f"{eb0:.6e}", f"{eb1:.6e}")
    if not math.isfinite(eb1):
        passed = False
        reasons.append("Eb not finite (NaN/Inf)")
    if eb0 > 0:
        ratio = eb1 / eb0
        logger.debug("    Eb ratio: %.6f", ratio)
        # The perturbation is frozen, so Eb is conserved to round-off.
        if ratio < 0.999 or ratio > 1.001:
            passed = False
            reasons.append(
                f"Eb ratio {ratio:.6f} not ~1 (zero-current wave propagated / "
                f"decayed; field should be frozen)")

    # Ee is the no-propagation discriminator: a frozen field has E = 0, so the
    # electric energy is ~0; a propagating wave keeps Ee > 0 even while conserving Eb.
    eemax = max((d.get("Ee", 0.0) for d in pic_diags), default=0.0)
    logger.debug("    Ee (electric, max): %s", f"{eemax:.6e}")
    if not math.isfinite(eemax):
        passed = False
        reasons.append("Ee not finite (NaN/Inf)")
    if eb0 > 0 and eemax > 1.0e-6 * eb0:
        passed = False
        reasons.append(
            f"Ee {eemax:.3e} not ~0 vs Eb {eb0:.3e} (wave is propagating / "
            f"field is evolving, not frozen)")

    if passed:
        logger.debug("Zero-Current Hybrid Test: PASSED")
        return True, "Passed"
    return False, "; ".join(reasons)
